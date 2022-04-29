/*************************************************************************
Journal.cpp - For the journal feature

begun 8/24/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


#define  MAXVALUES      4
#define  CV_DURATION    9      // special clipboard value for duration

CListVariable glistJournalNames;
static CListFixed    glistJournalID;   // array of DWORD
static BOOL          gJournalListInit = FALSE;  // set to true if the list is already initialized
static DFDATE        gdateJournal = 0;    // filter for journal entries

static WCHAR gszJournalCategory[] = L"JournalCategory";
static WCHAR gszCategoryRemove[] = L"CategoryToRemove";
static PWSTR gaszValue[MAXVALUES] = {L"value1",L"value2",L"value3",L"value4"};   // name of the value
static PWSTR gaszValueNumber[MAXVALUES] = {L"valuen1",L"valuen2",L"valuen3",L"valuen4"};   // number in the value
static DWORD gdwCategoryNode, gdwEntryNode;
static WCHAR gszMeetingStart[] = L"MeetingStart";
static WCHAR gszMeetingEnd[] = L"MeetingEnd";



/*************************************************************************
JournalLink - Given a journal node, this creates a link entry in the node to
another node (such as a phone call).

inputs
   DWORD    dwCategory - category node
   PWSTR    pszName - Name to put in as link
   DWORD    dwEntry - Entry number to link to
   DFDATE   dfDate - Date
   DFTIME   dfStart, dfEnd - Start and end time.
   double   fOverride - Override the time to this many hours. if <=0 ignore
   BOOL     fKeepOld - If TRUE it keeps the old entries also linked to this
returns
   BOOL - TRUE if succeded
*/
BOOL JournalLink (DWORD dwCategory, PWSTR pszName, DWORD dwEntry, DFDATE dfDate,
                  DFTIME dfStart, DFTIME dfEnd, double fOverride, BOOL fKeepOld)
{
   HANGFUNCIN;
   PCMMLNode pCategory;
   pCategory = gpData->NodeGet (dwCategory);
   if (!pCategory) {
      return FALSE;  // error
   }

   // index entry into cateogyr as unknown for now
   NodeElemSet (pCategory, gszJournalEntryNode, pszName, (int) dwEntry,
      !fKeepOld && (dwEntry != (DWORD)-1), dfDate, dfStart, dfEnd,
      (fOverride >= 0) ? (int) (fOverride * 60.0) : 0);

   gpData->NodeRelease (pCategory);
   return TRUE;
}

/************************************************************************8
JournalListVariable - Returns the list variable for the journal category.
   This also fills in parallel CListFixed of Node IDs.

inputs
   none
returns
   PCListVariable - List of names. Used for filter-list.
*/
PCListVariable JournalListVariable (void)
{
   HANGFUNCIN;
   if (gJournalListInit)
      return &glistJournalNames;
   gJournalListInit = TRUE;
   glistJournalID.Init (sizeof(DWORD));

   PCMMLNode pNode;
   pNode = FindMajorSection (gszJournalNode);
   if (!pNode)
      return NULL;   // unexpected error

   DWORD i;
   PCMMLNode   pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszJournalCategory))
         continue;

      // have entry

      PWSTR pszName;
      DWORD dwNum;
      dwNum = (DWORD) NodeValueGetInt (pSub, gszNumber, -1);
      pszName = NodeValueGet (pSub, gszName);

      glistJournalNames.Add (pszName, (wcslen(pszName)+1)*2);
      glistJournalID.Add (&dwNum);
   }

   gpData->NodeRelease (pNode);

   // done
   return &glistJournalNames;
}

/***********************************************************************
JournalQuickAddPage - Page callback for quick-adding a new user
*/
BOOL JournalQuickAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, gszAdd))
            break;

         // else, we're adding
         PCEscControl pControl;
         WCHAR szName[128];
         DWORD dwNeeded;

         // get the name
         szName[0] = 0;
         pControl = pPage->ControlFind (gszName);
         if (pControl)
            pControl->AttribGet (gszText, szName, sizeof(szName), &dwNeeded);
         if (!szName[0]) {
            pPage->MBWarning (L"You must enter a name for the category.");
            return TRUE;
         }

         // look for duplicate names
         JournalListVariable();
         DWORD i;
         for (i = 0; i < glistJournalNames.Num(); i++) {
            PWSTR psz = (PWSTR) glistJournalNames.Get(i);
            if (!_wcsicmp(psz, szName)) {
               pPage->MBWarning (L"You already have a journal category with that name.");
               return TRUE;
            }
         }

         // create the new node
         PCMMLNode pNode, pSub, pCategory;
         DWORD dwCategory;
         pCategory = gpData->NodeAdd (gszJournalCategoryNode, &dwCategory);
         if (!pCategory)
            return TRUE;   // error
         pNode = FindMajorSection (gszJournalNode);
         if (!pNode) {
            gpData->NodeRelease (pCategory);
            return TRUE;
         }
         
         // create the subnode
         pSub = pNode->ContentAddNewNode ();
         if (!pSub) {
            gpData->NodeRelease (pCategory);
            gpData->NodeRelease (pNode);
            return TRUE;
         }
         pSub->NameSet (gszJournalCategory);
         
         // write the name
         NodeValueSet (pSub, gszName, szName);
         NodeValueSet (pCategory, gszName, szName);

         // write the category number
         NodeValueSet (pSub, gszNumber, (int) dwCategory);

         glistJournalNames.Add (szName, (wcslen(szName)+1)*2);
         glistJournalID.Add (&dwCategory);

         // release
         gpData->NodeRelease (pNode);
         gpData->NodeRelease (pCategory);
         gpData->Flush();

         EscChime (ESCCHIME_INFORMATION);
         EscSpeak (L"Journal category added.");

         // return saying what the new node is
         WCHAR szTemp[16];
         swprintf (szTemp, L"!%d", (int) dwCategory);
         pPage->Link (szTemp);
         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
JouranlDatabaseToIndex - Given a CDatabase location for the journal category
this returns an index into the glistPeople.

inputs
   DWORD - CDatabase location
returns
   DWORD - Index into glistJouralNames. -1 if cant find
*/
DWORD JournalDatabaseToIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   JournalListVariable();

   // find the index
   return FilteredDatabaseToIndex (dwIndex, &glistJournalNames, &glistJournalID);
}

/***********************************************************************
JournalIndexToDatabase - Given an index ID (into glistJournal), this
returns a DWORD CDatabase location for the person

inputs
   DWORD - dwIndex
returns
   DWORD - CDatabase location. -1 if cant find
*/
DWORD JournalIndexToDatabase (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   JournalListVariable();

   // find the index
   return FilteredIndexToDatabase (dwIndex, &glistJournalNames, &glistJournalID);
}


/*****************************************************************************
JournalQuickAdd - Quick-adds a person to the list.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   DWORD - New subproject index. -1 if didn't add.
*/
DWORD JournalQuickAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLJOURNALQUICKADD, JournalQuickAddPage);

   if (!pszRet || (pszRet[0] != L'!'))
      return (DWORD) -1;   // canceled

   return JournalDatabaseToIndex(_wtoi(pszRet+1));
}


/************************************************************************8
CreateSampleJournal - Create a sample journal category

inputs
   PCMMLNoed      pNode - From FindMajorSection (gszJournalNode)
   PWSTR          psz - Journal name
returns
   none
*/
void CreateSampleJournal (PCMMLNode pNode, PWSTR psz)
{
   HANGFUNCIN;
   JournalListVariable();

   // create the new node
   PCMMLNode pSub, pCategory;
   DWORD dwCategory;
   pCategory = gpData->NodeAdd (gszJournalCategoryNode, &dwCategory);
   if (!pCategory)
      return ;   // error

   // create the subnode
   pSub = pNode->ContentAddNewNode ();
   if (!pSub) {
      gpData->NodeRelease (pCategory);
      return;
   }
   pSub->NameSet (gszJournalCategory);

   // write the name
   NodeValueSet (pSub, gszName, psz);
   NodeValueSet (pCategory, gszName, psz);

   // write the category number
   NodeValueSet (pSub, gszNumber, (int) dwCategory);

   // release
   gpData->NodeRelease (pCategory);

   // add this to the list
   glistJournalNames.Add (psz, (wcslen(psz)+1)*2);
   glistJournalID.Add (&dwCategory);
}

// JOURNALSORT - Sorted journal list
typedef struct {
   PWSTR          psz;        // string
   PCMMLNode      pNode;      // journal node
} JOURNALSORT, *PJOURNALSORT;

int __cdecl JOURNALSORTSort(const void *elem1, const void *elem2 )
{
   PJOURNALSORT dw1, dw2;
   dw1 = (PJOURNALSORT) elem1;
   dw2 = (PJOURNALSORT) elem2;

   return _wcsicmp(dw1->psz, dw2->psz);
}

/***********************************************************************
JournalPage - Page callback
*/
BOOL JournalPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"CATEGORIES")) {
            PCMMLNode pNode;
            pNode = FindMajorSection (gszJournalNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            // when first start up set up default categories
            if (!NodeValueGetInt (pNode, gszExamples, FALSE)) {
               // set flag true
               NodeValueSet (pNode, gszExamples, (int) TRUE);

               // add journal categories
               CreateSampleJournal (pNode, L"My thoughts and ideas");
               CreateSampleJournal (pNode, L"Work done");
               CreateSampleJournal (pNode, L"Dreams");

               // write to disk
               gpData->Flush();
            }

            // see if can find a speed dial
            if ((DWORD)-1 == pNode->ContentFind (gszJournalCategory)) {
               // can't find, so leave blank
               p->pszSubString = (PWSTR)gMemTemp.p;
               gpData->NodeRelease(pNode);
               return TRUE;
            }

            // BUGFIX - sort the journal
            CListFixed lJOURNALSORT;
            JOURNALSORT js;
            lJOURNALSORT.Init (sizeof(JOURNALSORT));
            DWORD i;
            PCMMLNode   pSub;
            PWSTR psz;
            for (i = 0; i < pNode->ContentNum(); i++) {
               pSub = NULL;
               pNode->ContentEnum(i, &psz, &pSub);
               if (!pSub)
                  continue;
               if (_wcsicmp(pSub->NameGet(), gszJournalCategory))
                  continue;

               PWSTR pszName;
               pszName = NodeValueGet (pSub, gszName);
               if (!pszName)
                  pszName = L"Unknown";

               js.psz = pszName;
               js.pNode = pSub;

               lJOURNALSORT.Add (&js);
            } // i


            // else, there are speed dials so write the info
            if (gfMicroHelp)
               MemCat (&gMemTemp, L"<p>Click the category of journal entry to wish to create:</p>");
            MemCat (&gMemTemp, L"<table width=100% border=0 innerlines=0>");
            DWORD dwCellsInRow;
            PJOURNALSORT pjs = (PJOURNALSORT) lJOURNALSORT.Get(0);
            qsort (pjs, lJOURNALSORT.Num(), sizeof(JOURNALSORT), JOURNALSORTSort);
            dwCellsInRow = 0;
            DWORD dwNum = lJOURNALSORT.Num();
            DWORD dwColumns = 2;
            DWORD dwRows = (dwNum+dwColumns-1) / dwColumns;
            for (i = 0; i < dwNum; i++) {
               pjs = (PJOURNALSORT) lJOURNALSORT.Get((i % dwColumns) * dwRows + (i / dwColumns));
               if (!pjs)
                  continue;   // shouldnt happen

               pSub = pjs->pNode;

               // have entry

               // depending upon the number of cells in the row display text
               if (!dwCellsInRow)
                  MemCat (&gMemTemp, L"<tr>");

               // the text
               MemCat (&gMemTemp, L"<xJournalButton href=v:");
               PWSTR pszNum, pszName;
               pszNum = NodeValueGet (pSub, gszNumber);
               MemCatSanitize (&gMemTemp, pszNum ? pszNum : L"");
               MemCat (&gMemTemp, L">");
               pszName = NodeValueGet (pSub, gszName);
               MemCatSanitize (&gMemTemp, pszName ? pszName : L"Unknown");
               MemCat (&gMemTemp, L"</xJournalButton>");


               if (dwCellsInRow)
                  MemCat (&gMemTemp, L"</tr>");
               dwCellsInRow = (dwCellsInRow+1)%2;
            } // i

            // if we're ending with a partially filled row then make that empty
            if (dwCellsInRow)
               MemCat (&gMemTemp, L"<xJournalBlank/></tr>");

            MemCat (&gMemTemp, L"</table><p/><xbr/>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
JournalCatAddPage - Page callback
*/
BOOL JournalCatAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, gszAdd))
            break;

         // else, we're adding
         PCEscControl pControl;
         WCHAR szName[128];
         DWORD dwNeeded;

         // get the name
         szName[0] = 0;
         pControl = pPage->ControlFind (gszName);
         if (pControl)
            pControl->AttribGet (gszText, szName, sizeof(szName), &dwNeeded);
         if (!szName[0]) {
            pPage->MBWarning (L"You must enter a name for the category.");
            return TRUE;
         }

         // look for duplicate names
         JournalListVariable();
         DWORD i;
         for (i = 0; i < glistJournalNames.Num(); i++) {
            PWSTR psz = (PWSTR) glistJournalNames.Get(i);
            if (!_wcsicmp(psz, szName)) {
               pPage->MBWarning (L"You already have a journal category with that name.");
               return TRUE;
            }
         }

         // create the new node
         PCMMLNode pNode, pSub, pCategory;
         DWORD dwCategory;
         pCategory = gpData->NodeAdd (gszJournalCategoryNode, &dwCategory);
         if (!pCategory)
            return TRUE;   // error
         pNode = FindMajorSection (gszJournalNode);
         if (!pNode) {
            gpData->NodeRelease (pCategory);
            return TRUE;
         }
         
         // create the subnode
         pSub = pNode->ContentAddNewNode ();
         if (!pSub) {
            gpData->NodeRelease (pCategory);
            gpData->NodeRelease (pNode);
            return TRUE;
         }
         pSub->NameSet (gszJournalCategory);
         
         // write the name
         NodeValueSet (pSub, gszName, szName);
         NodeValueSet (pCategory, gszName, szName);

         // write the category number
         NodeValueSet (pSub, gszNumber, (int) dwCategory);

         glistJournalNames.Add (szName, (wcslen(szName)+1)*2);
         glistJournalID.Add (&dwCategory);

         // people
         // DWORD i;
         for (i = 0; i < MAXPEOPLE; i++) {
            pControl = pPage->ControlFind (gaszPerson[i]);
            if (!pControl) continue;
            int iIndex, iData;
            iIndex = pControl->AttribGetInt (gszCurSel);
            iData = (int) PeopleBusinessIndexToDatabase((DWORD)iIndex);
            NodeValueSet (pCategory, gaszPerson[i], iData);
         }

         // values
         for (i = 0; i < MAXVALUES; i++) {
            pControl = pPage->ControlFind (gaszValue[i]);
            if (!pControl) continue;
            szName[0] = 0;
            pControl->AttribGet(gszText, szName, sizeof(szName), &dwNeeded);
            NodeValueSet (pCategory, gaszValue[i], szName);
         }

         // release
         gpData->NodeRelease (pNode);
         gpData->NodeRelease (pCategory);
         gpData->Flush();

         EscChime (ESCCHIME_INFORMATION);
         EscSpeak (L"Journal category added.");

         // go to the category page
         swprintf (szName, L"v:%d", (int) dwCategory);
         pPage->Exit (szName);
         return TRUE;
      }

   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
JournalCatRemovePage - Override page callback.
*/
BOOL JournalCatRemovePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl   pControl;
         pControl = pPage->ControlFind (gszCategoryRemove);
         if (!pControl)
            break;

         PCMMLNode pNode;
         pNode = FindMajorSection (gszJournalNode);
         if (!pNode)
            break;   // unexpected error

         // add this
         ESCMCOMBOBOXADD add;
         memset (&add, 0, sizeof(add));
         add.dwInsertBefore = (DWORD)-1;
         
         // else, there are speed dials so write the info
         DWORD i;
         PCMMLNode   pSub;
         PWSTR psz;
         for (i = 0; i < pNode->ContentNum(); i++) {
            pSub = NULL;
            pNode->ContentEnum(i, &psz, &pSub);
            if (!pSub)
               continue;
            if (_wcsicmp(pSub->NameGet(), gszJournalCategory))
               continue;

            // have entry

            // the text
            PWSTR pszName;
            pszName = NodeValueGet (pSub, gszName);

            // add it
            add.pszText = pszName;
            pControl->Message (ESCM_COMBOBOXADD, &add);
         }

         gpData->NodeRelease(pNode);

      }
      break;   // make sure to break and continue on with other init page

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"remove"))
            break;

         // get the selection
         PCEscControl   pControl;
         ESCMCOMBOBOXGETITEM gi;
         memset (&gi, 0, sizeof(gi));
         DWORD dwIndex;
         dwIndex = (DWORD)-1;
         pControl = pPage->ControlFind (gszCategoryRemove);
         if (pControl) {
            dwIndex = (DWORD) pControl->AttribGetInt (gszCurSel);
            gi.dwIndex = dwIndex;
         }

         PCMMLNode pNode;
         pNode = FindMajorSection (gszJournalNode);
         if (!pNode)
            break;   // unexpected error

         // find it
         DWORD i;
         PCMMLNode   pSub;
         PWSTR psz;
         for (i = 0; i < pNode->ContentNum(); i++) {
            pSub = NULL;
            pNode->ContentEnum(i, &psz, &pSub);
            if (!pSub)
               continue;
            if (_wcsicmp(pSub->NameGet(), gszJournalCategory))
               continue;

            // have entry

            // if index != 0 then continue
            if (dwIndex) {
               dwIndex--;
               continue;
            }

            // verify
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove the selected category?",
               L"The category list will still be available through search, "
               L"but you won't be able to access it directly from the Journal page."))
               return FALSE;

            // remove from internal list
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            JournalListVariable();
            DWORD j;
            for (j = 0; j < glistJournalNames.Num(); j++) {
               PWSTR psz = (PWSTR) glistJournalNames.Get(j);
               if (gi.pszName && !_wcsicmp(psz, gi.pszName)) {
                  glistJournalNames.Remove(j);
                  glistJournalID.Remove(j);
                  break;
               }
            }

            // remove
            pNode->ContentRemove (i);
            gpData->NodeRelease (pNode);
            gpData->Flush();

            // use TTS to speak
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Category removed.");

            // go to the main project page
            pPage->Exit(L"r:121");
            return TRUE;
         }

         // else not found
         gpData->NodeRelease(pNode);


         pPage->MBWarning (L"You must have projects before you can remove them.");
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************
JournalCatSet - Sets the category node to view
*/
void JournalCatSet (DWORD dwNode)
{
   HANGFUNCIN;
   gdwCategoryNode = dwNode;
}

/***********************************************************************
JournalCatPage - Page callback
*/
BOOL JournalCatPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   static BOOL gfRemoveMode = FALSE;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set the dropdown to the right date
         DateControlSet (pPage, gszDate, gdateJournal);

         // make sure we're not in remove mode
         gfRemoveMode = FALSE;
      }
      break;   // go to default handler

   case ESCN_DATECHANGE:
      {
         // if the date changes then refresh the page
         PESCNDATECHANGE p = (PESCNDATECHANGE) pParam;

         if (_wcsicmp(p->pControl->m_pszName, gszDate))
            break;   // wrong one

         if ((p->iMonth > 0) && (p->iYear > 0))
            gdateJournal = TODFDATE (1, p->iMonth, p->iYear);
         else
            gdateJournal = 0;
         pPage->Exit (gszRedoSamePage);
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // BUGFIX - Allow person to delete entrye
         if (gfRemoveMode && p->psz && (p->psz[0] == L'v') && (p->psz[1] == L':')) {
            gfRemoveMode = FALSE;

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove this entry from the list?"))
               return TRUE;   // eat up the link

            // delete this
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwCategoryNode);
            if (pNode) {
               NodeElemRemove (pNode, gszJournalEntryNode, _wtoi(p->psz+2));
               gpData->NodeRelease (pNode);
               gpData->Flush();
            }

            pPage->Link(gszRedoSamePage);
            return TRUE;
         }
      }
      break;   // fall through
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"CATEGORYNAME")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwCategoryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR pszName;
            pszName = NodeValueGet (pNode, gszName);

            MemCat (&gMemTemp, pszName ? pszName : L"Unknown");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"GRAPHBUTTONS")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwCategoryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR pszName;
            pszName = NodeValueGet (pNode, gszName);

            // if there are any node values then make a button for it
            DWORD i;
            PWSTR psz;
            for (i = 0; i < MAXVALUES; i++) {
               psz = NodeValueGet (pNode, gaszValue[i]);
               if (!psz || !psz[0])
                  continue;

               // button
               MemCat (&gMemTemp, L"<xChoiceButton name=copy");
               MemCat (&gMemTemp, (int)i+1);
               MemCat (&gMemTemp, L"><bold>Copy ");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L" to the clipboard</bold><br/>");
               MemCat (&gMemTemp, L"This lets you view a history of the value on your spreadsheet. Just paste the text into your spreadsheet and graph it.");
               MemCat (&gMemTemp, L"</xChoiceButton>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"LOG")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwCategoryNode);
            if (pNode)
               pl = NodeListGet (pNode, gszJournalEntryNode, FALSE);

            DWORD i;
            NLG *pnlg;
            DWORD dwShown;
            dwShown = 0;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);

               // if it's not in the date range we're searching for then skip
               if (gdateJournal) {
                  if ((MONTHFROMDFDATE(gdateJournal) != MONTHFROMDFDATE(pnlg->date)) ||
                     (YEARFROMDFDATE(gdateJournal) != YEARFROMDFDATE(pnlg->date)) )
                      continue;
               }

               dwShown++;

               // write it out
               MemCat (&gMemTemp, L"<tr>");
               if (pnlg->iNumber != -1) {
                  MemCat (&gMemTemp, L"<xtdtask href=v:");
                  MemCat (&gMemTemp, pnlg->iNumber);
                  MemCat (&gMemTemp, L">");
               }
               else
                  MemCat (&gMemTemp, L"<xtdtasknolink>");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               if (pnlg->iNumber != -1)
                  MemCat (&gMemTemp, L"</xtdtask>");
               else
                  MemCat (&gMemTemp, L"</xtdtasknolink>");
               MemCat (&gMemTemp, L"<xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString (pnlg->start, szTemp);
               MemCat (&gMemTemp, szTemp);
               if ((pnlg->end != (DWORD)-1) && (pnlg->start != pnlg->end)) {
                  MemCat (&gMemTemp, L" to ");
                  DFTIMEToString (pnlg->end, szTemp);
                  MemCat (&gMemTemp, szTemp);
               }
               
               // if indicate # of hours then include that
               if (pnlg->iExtra > 0) {
                  MemCat (&gMemTemp, L"<br/>");
                  swprintf (szTemp, L"%g", pnlg->iExtra / 60.0);
                  MemCat (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L" hours work");
               }

               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // if no entries then say so
            if (!pl || !pl->Num())
               MemCat (&gMemTemp, L"<tr><td>You haven't added any journal entries yet.</td></tr>");
            else if (!dwShown)
               MemCat (&gMemTemp, L"<tr><td>There aren't any journal entries for the specified month.</td></tr>");

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

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         WCHAR szCopy[] = L"copy";
         int iLen = wcslen(szCopy);
         if (!wcsncmp (p->pControl->m_pszName, szCopy, iLen)) {
            // pressed the copy button
            // which one?
            DWORD dwNum;
            dwNum = (DWORD) (p->pControl->m_pszName[iLen] - L'1');
            if (dwNum == CV_DURATION-1)
               dwNum = CV_DURATION;

            // loop through all the entries
            // find the log
            char  *psz;
            DWORD dwAllocated, dwCur;
            dwAllocated = 10;
            dwCur = 0;
            psz = (char*) malloc(dwAllocated);
            PCMMLNode pNode;
            PCListFixed pl = NULL;
            pNode = gpData->NodeGet (gdwCategoryNode);
            if (pNode)
               pl = NodeListGet (pNode, gszJournalEntryNode, TRUE);

            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);

               // if it's not in the date range we're searching for then skip
               if (gdateJournal) {
                  if ((MONTHFROMDFDATE(gdateJournal) != MONTHFROMDFDATE(pnlg->date)) ||
                     (YEARFROMDFDATE(gdateJournal) != YEARFROMDFDATE(pnlg->date)) )
                      continue;
               }

               // get the entry
               PCMMLNode pEntry;
               pEntry = gpData->NodeGet (pnlg->iNumber);

               // see if it has anything to copy
               PWSTR pwsz;
               WCHAR szNumber[32];
               pwsz = NULL;
               if (dwNum == CV_DURATION) {
                  // can get the duration info from here

                  // if have override value (for # minutes that took) then use
                  // that
                  if (pnlg->iExtra > 0) {
                     swprintf (szNumber, L"%g", (double) pnlg->iExtra / 60.0);
                     pwsz = szNumber;
                  }
                  else if ((pnlg->start != (DWORD)-1) && (pnlg->end != (DWORD)-1) && (pnlg->start != pnlg->end)) {
                     swprintf (szNumber, L"%g", (double) (DFTIMEToMinutes(pnlg->end) - DFTIMEToMinutes(pnlg->start)) / 60.0);
                     pwsz = szNumber;
                  }
               }
               else
                  pwsz = pEntry ? NodeValueGet (pEntry, gaszValueNumber[dwNum]) : NULL;
               
               if (pwsz && pwsz[0]) {
                  // it actually has data!

                  // make sure there's enough space
                  // len
                  int   iLen;
                  iLen = 2000; // padding
                  if (dwCur + (DWORD)iLen >= dwAllocated) {
                     dwAllocated = dwAllocated * 2 + (DWORD)iLen;
                     psz = (char*) realloc (psz, dwAllocated);
                     if (!psz)
                        return 0;   // error
                  }

                  // put in log string (max 64 chars)
                  WCHAR szLogString[64];
                  if (pnlg->psz) {
                     wcsncpy (szLogString, pnlg->psz, 63);
                     szLogString[63] = 0;
                  }
                  else {
                     wcscpy (szLogString, L"Unknown");
                  }
                  WCHAR *pc;
                  for (pc = szLogString; *pc; pc++)
                     if ((*pc == L'\t') || (*pc == L'\r') || (*pc == L'\n'))
                        *pc = L' ';
                  WideCharToMultiByte (CP_ACP, 0, szLogString, -1, psz + dwCur, 128, 0, 0);
                  dwCur += (DWORD) strlen(psz+dwCur);

                  // date and string
                  char szTemp[256], szTemp2[128];
                  DFDATE date;
                  char  szSep[16];
                  date = pEntry ? (DFDATE) NodeValueGetInt (pEntry, gszDate, 0) : pnlg->date;
                  if (!date)
                     date = pnlg->date;
                  WideCharToMultiByte (CP_ACP, 0, pwsz, -1, szTemp2, sizeof(szTemp2), 0, 0);
                  WORD wFmt;
                  wFmt = 0xffff;
                  GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IDATE, (LPSTR)&wFmt, sizeof(wFmt));
                  GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SDATE, szSep, sizeof(szSep));

                  switch (wFmt) {
                  case '0': // mdy
                     sprintf (szTemp, "\t%d%s%d%s%d\t%s\r\n",
                        MONTHFROMDFDATE(date), szSep, DAYFROMDFDATE(date), szSep, YEARFROMDFDATE(date), szTemp2);
                     break;
                  case '1': // dmy
                     sprintf (szTemp, "\t%d%s%d%s%d\t%s\r\n",
                        DAYFROMDFDATE(date), szSep, MONTHFROMDFDATE(date), szSep, YEARFROMDFDATE(date), szTemp2);
                     break;
                  default: // ymd
                     sprintf (szTemp, "\t%d%s%d%s%d\t%s\r\n",
                        YEARFROMDFDATE(date), szSep, MONTHFROMDFDATE(date), szSep, DAYFROMDFDATE(date), szTemp2);
                  }
                  strcpy (psz + dwCur, szTemp);
                  dwCur += (DWORD) strlen(szTemp);
               }

               if (pEntry)
                  gpData->NodeRelease (pEntry);
            }

            // copy to the clipboard
            psz[dwCur] = 0;
            HANDLE   hMem;
            hMem = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, dwCur+1);
            if (!hMem)
               return TRUE;
            memcpy ((char*) GlobalLock(hMem), psz, dwCur+1);
            GlobalUnlock (hMem);

            OpenClipboard (pPage->m_pWindow->m_hWnd);
            EmptyClipboard ();
            SetClipboardData (CF_TEXT, hMem);
            CloseClipboard ();

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);
            if (psz)
               free (psz);


            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Copied to the clipboard.");

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            // BUGFIX - Allow to remove from journal

            pPage->MBSpeakInformation (L"Click on the journal entry you wish to remove.");
            gfRemoveMode = TRUE;
         }

         else if (!_wcsicmp(p->pControl->m_pszName, gszTimer)) {
            // BUGFIX - Timer from journal so can log work done

            // create the new timer
            PCMMLNode pNode;
            pNode = FindMajorSection (gszJournalNode);
            if (pNode) {
               // get the category name
               PCMMLNode pCategory;
               pCategory = gpData->NodeGet (gdwCategoryNode);
               if (!pCategory) {
                  gpData->NodeRelease(pNode);
                  return FALSE;  // error
               }

               // write some stuff into the entry
               PWSTR pszName;
               pszName = NodeValueGet (pCategory, gszName);

               // write it out
               NodeElemSet (pNode, gszTimer, pszName ? pszName : L"Unknown category",
                  gdwCategoryNode, FALSE, Today(), Now());

               // release
               gpData->NodeRelease (pCategory);
               gpData->NodeRelease (pNode);
               gpData->Flush();
            }
         


            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Timer started.");
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"import")) {
            ImportPhotos (pPage, gdwCategoryNode);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // add button is pressed. do so

            DWORD dwEntry;
            PCMMLNode pCategory, pEntry;
            pEntry = gpData->NodeAdd (gszJournalEntryNode, &dwEntry);
            if (!pEntry)
               return FALSE;  // error
            pCategory = gpData->NodeGet (gdwCategoryNode);
            if (!pCategory) {
               gpData->NodeRelease(pEntry);
               return FALSE;  // error
            }

            // index entry into cateogyr as unknown for now
            NodeElemSet (pCategory, gszJournalEntryNode, L"Unknown", (int) dwEntry,
               TRUE, Today(), Now(), -1);

            // write some stuff into the entry
            PWSTR pszName;
            pszName = NodeValueGet (pCategory, gszName);
            if (pszName)
               NodeValueSet (pEntry, gszCategory, pszName, (int) gdwCategoryNode);
            DWORD i;
            for (i = 0; i < MAXPEOPLE; i++)
               NodeValueSet (pEntry, gaszPerson[i], NodeValueGetInt(pCategory, gaszPerson[i], -1));
            for (i = 0; i < MAXVALUES; i++) {
               pszName = NodeValueGet (pCategory, gaszValue[i]);
               if (pszName)
                  NodeValueSet (pEntry, gaszValue[i], pszName);
            }
            NodeValueSet (pEntry, gszDate, (int) Today());
            NodeValueSet (pEntry, gszStart, (int) Now());
            NodeValueSet (pEntry, gszEnd, (int)-1);
            NodeValueSet (pEntry, gszSummary, L"");
            NodeValueSet (pEntry, gszName, L"");



            gpData->NodeRelease(pEntry);
            gpData->NodeRelease(pCategory);
            gpData->Flush();

            // go to the enty
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwEntry);
            pPage->Exit (szTemp);
         }
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************
JournalEntrySet - Sets the category node to view
*/
void JournalEntrySet (DWORD dwNode)
{
   HANGFUNCIN;
   gdwEntryNode = dwNode;

   // BUGFIX - find the category node
   PCMMLNode pNode;
   pNode = gpData->NodeGet (gdwEntryNode);
   if (!pNode)
      return;   // unexpected error
   PWSTR psz;
   DWORD dwNum;
   psz = NodeValueGet (pNode, gszCategory, (int*)&dwNum);
   gdwCategoryNode = dwNum;
   gpData->NodeRelease(pNode);
}

/***********************************************************************
JournalEntryEditPage - Page callback
*/
BOOL JournalEntryEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"VALUES")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR pszName;
            DWORD i;
            for (i = 0; i < MAXVALUES; i++) {
               pszName = NodeValueGet (pNode, gaszValue[i]);
               if (pszName && pszName[0]) {
                  MemCat (&gMemTemp, L"<tr><xtdleft>");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp, L":</xtdLeft><xtdright><xEditInTable name=valuen");
                  MemCat (&gMemTemp, (int) i+1);
                  MemCat (&gMemTemp, L"/></xtdright></tr>");
               }
            }

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
         TimeControlSet (pPage, gszMeetingStart, (DFTIME) NodeValueGetInt (pEntry, gszStart, 0));
         TimeControlSet (pPage, gszMeetingEnd, (DFTIME) NodeValueGetInt (pEntry, gszEnd, 0));

         // all the values
         DWORD i;
         for (i = 0; i < MAXVALUES; i++) {
            psz = NodeValueGet (pEntry, gaszValueNumber[i]);
            pControl = pPage->ControlFind (gaszValueNumber[i]);
            if (psz && pControl)
               pControl->AttribSet(gszText, psz);
         }
         
         // all the people
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
         DWORD dwIndex;
         dwIndex = JournalDatabaseToIndex (gdwCategoryNode);
         if (pControl && (dwIndex != (DWORD)-1))
            pControl->AttribSetInt (gszCurSel, (int) dwIndex);

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
         PCEscControl pControl;
         PCMMLNode pNode;
         DWORD dwNeeded;
         WCHAR szTemp[10000], szName[128];
         pNode = gpData->NodeGet (gdwEntryNode);
         if (!pNode)
            break;

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
         DFTIME start, end;
         date = DateControlGet (pPage, gszMeetingDate);
         start = TimeControlGet (pPage, gszMeetingStart);
         end = TimeControlGet (pPage, gszMeetingEnd);
         if (end == -1)
            end = Now();
         NodeValueSet (pNode, gszDate, (int) date);
         NodeValueSet (pNode, gszStart, start);
         NodeValueSet (pNode, gszEnd, end);

         DWORD i;
         for (i = 0; i < MAXPEOPLE; i++) {
            pControl = pPage->ControlFind (gaszPerson[i]);
            if (!pControl)
               continue;
               // get the values
            int   iCurSel;
            iCurSel = pControl->AttribGetInt (gszCurSel);

            // find the node number for the person's data and the name
            DWORD dwNode;
            dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);

            NodeValueSet (pNode, gaszPerson[i], (int) dwNode);

            // write out the people links
            PCMMLNode pPerson;
            pPerson = NULL;
            if (dwNode != (DWORD)-1)
               pPerson = gpData->NodeGet (dwNode);
            if (pPerson) {
               swprintf (szTemp, L"Journal entry: %s", szName);

               // overwrite if already exists
               NodeElemSet (pPerson, gszInteraction, szTemp, (int) gdwEntryNode, TRUE,
                  date, start, end);

               gpData->NodeRelease (pPerson);
            }
         }

         
         // the values
         for (i = 0; i < MAXVALUES; i++) {
            pControl = pPage->ControlFind (gaszValueNumber[i]);
            if (!pControl)
               continue;
            szTemp[0] = 0;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            NodeValueSet (pNode, gaszValueNumber[i], szTemp);
         }

         // BUGFIX - If category changed then delete old one
         PCMMLNode pCategory;
         pControl = pPage->ControlFind (gszCategory);
         DWORD dwNewCat;
         DWORD dwIndex;
         dwIndex = pControl ? pControl->AttribGetInt(gszCurSel) : -1;
         dwNewCat = JournalIndexToDatabase(dwIndex);
         if (dwNewCat != gdwCategoryNode) {
            pCategory = gpData->NodeGet (gdwCategoryNode);
            if (pCategory) {
               NodeElemRemove (pCategory, gszJournalEntryNode, (int) gdwEntryNode);
               gpData->NodeRelease(pCategory);
            }
            gdwCategoryNode = dwNewCat;

            // write this
            PWSTR psz;
            psz = (PWSTR) glistJournalNames.Get(dwIndex);
            NodeValueSet (pNode, gszCategory, psz ? psz : L"No category", (int) gdwCategoryNode);
         }


         // write this to the log to the category
         pCategory = gpData->NodeGet (gdwCategoryNode);
         if (pCategory) {
            NodeElemSet (pCategory, gszJournalEntryNode, szName, (int) gdwEntryNode,
               TRUE, date, start, end);
            gpData->NodeRelease(pCategory);
         }




         // write out the daily log
         swprintf (szTemp, L"Journal entry: %s", szName);
         CalendarLogAdd (date, start, end, szTemp, gdwEntryNode);

         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default behavior

   };

   return DefPage (pPage, dwMessage, pParam);
}



/***********************************************************************
JournalEntryViewPage - Page callback
*/
BOOL JournalEntryViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
         if (!_wcsicmp(p->pszSubName, L"CATEGORY")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            DWORD dwNum;
            psz = NodeValueGet (pNode, gszCategory, (int*)&dwNum);
            if (psz) {
               MemCat (&gMemTemp, L"<a href=v:");
               MemCat (&gMemTemp, (int) dwNum);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</a>");
            }

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

         if (!_wcsicmp(p->pszSubName, L"VALUES")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR pszName;
            DWORD i;
            for (i = 0; i < MAXVALUES; i++) {
               pszName = NodeValueGet (pNode, gaszValue[i]);
               if (pszName && pszName[0]) {
                  MemCat (&gMemTemp, L"<tr><xlt>");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp, L":</xlt><xrt>");
                  pszName = NodeValueGet (pNode, gaszValueNumber[i]);
                  if (pszName)
                     MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp, L"</xrt></tr>");
               }
            }

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
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString ((DFTIME) NodeValueGetInt (pNode, gszStart, -1), szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L" to ");
               DFTIMEToString ((DFTIME) NodeValueGetInt (pNode, gszEnd, -1), szTemp);
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

/*******************************************************************************
JournalTimerSubst - Returns a pointer to a string for the substitution (below
the main menu) for timers. This is usually a pointer to gMemTemp.p

inputs
   none
returns
   PWSTR - psz
*/
PWSTR JournalTimerSubst (void)
{
   HANGFUNCIN;
   PCMMLNode pNode;
   pNode = FindMajorSection (gszJournalNode);
   if (!pNode)
      return NULL;

   DWORD dwIndex;
   dwIndex = 0;
   MemZero (&gMemTemp);
   DFDATE today = 0;
   while (TRUE) {
      PWSTR psz;
      int   iNumber;
      DFDATE   date;
      DFTIME   start;
      psz = NodeElemGet (pNode, gszTimer, &dwIndex, &iNumber, &date, &start);
      if (!psz)
         break;

      // write it out
      MemCat (&gMemTemp, L"<tr><td width=5%/><td width=95%><font color=#ffffff><small>");
      MemCat (&gMemTemp, L"Work timer: ");
      MemCat (&gMemTemp, L"<a href=wt:");
      MemCat (&gMemTemp, (int)dwIndex-1);
      MemCat (&gMemTemp, L" color=#8080ff>");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"<xhoverhelpshort>Press this to stop the timer and log it in your journal.</xhoverhelpshort>");
      MemCat (&gMemTemp, L"</a>");
      MemCat (&gMemTemp, L", Started ");
      if (!today)
         today = Today();
      WCHAR szTemp[64];
      if (today != date) {
         DFDATEToString (date, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L" ");
      }
      DFTIMEToString (start, szTemp);
      MemCat (&gMemTemp, szTemp);

      MemCat (&gMemTemp, L"</small></font></td></tr>");
   }

   gpData->NodeRelease(pNode);
   return (PWSTR) gMemTemp.p;
}


/*****************************************************************************
JournalTimerEndPage - Override page callback.
*/
BOOL JournalTimerEndPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   DWORD dwIndex = (DWORD) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            PCMMLNode pNode;
            pNode = FindMajorSection (gszJournalNode);
            if (!pNode)
               return TRUE;

            pNode->ContentRemove (dwIndex);

            // release
            gpData->NodeRelease (pNode);
            gpData->Flush();
            pPage->Exit (gszDelete);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // find the entry and get the journal category and start time
            PCMMLNode pNode;
            pNode = FindMajorSection (gszJournalNode);
            if (!pNode)
               return TRUE;

            DWORD dwIndex2;
            dwIndex2 = 0;
            int   iNumber;
            DFDATE   date;
            DFTIME   start;
            while (TRUE) {
               PWSTR psz;
               psz = NodeElemGet (pNode, gszTimer, &dwIndex2, &iNumber, &date, &start);
               if (!psz)
                  return TRUE;   // error, can't find
               if ((dwIndex2-1) == dwIndex)
                  break;   // found the right one
            }

            // delete the info
            pNode->ContentRemove (dwIndex);
            gpData->NodeRelease (pNode);
            gpData->Flush();

            // get the journal cateogry
            PCMMLNode pCategory;
            pCategory = gpData->NodeGet ((DWORD)iNumber);
            if (!pCategory) {
               return FALSE;  // error
            }

            //get the text
            WCHAR szTemp[256];
            WCHAR szTitle[128];
            PCEscControl pControl;
            pControl = pPage->ControlFind(gszSummary);
            szTitle[0] = 0;
            DWORD dwNeeded;
            if (pControl)
               pControl->AttribGet (gszText, szTitle, sizeof(szTitle), &dwNeeded);
            if (!szTitle[0])
               wcscpy (szTitle, L"Work timer");

            // find out today
            DFDATE today;
            DFTIME   now;
            today = Today();
            now = Now();
            while (date <= today) {
               // figure out end time
               DFTIME end;
               end = (today == date) ? now : TODFTIME(23,59);

               // create an entry
               DWORD dwEntry;
               PCMMLNode pEntry;
               pEntry = gpData->NodeAdd (gszJournalEntryNode, &dwEntry);
               if (!pEntry)
                  break;  // error

               // index entry into cateogyr as unknown for now
               NodeElemSet (pCategory, gszJournalEntryNode, szTitle, (int) dwEntry,
                  TRUE, date, start, end);


               // write some stuff into the entry
               PWSTR pszName;
               pszName = NodeValueGet (pCategory, gszName);
               if (pszName)
                  NodeValueSet (pEntry, gszCategory, pszName, (int) gdwCategoryNode);
               DWORD i;
               for (i = 0; i < MAXVALUES; i++) {
                  pszName = NodeValueGet (pCategory, gaszValue[i]);
                  if (pszName)
                     NodeValueSet (pEntry, gaszValue[i], pszName);
               }
               NodeValueSet (pEntry, gszDate, (int) date);
               NodeValueSet (pEntry, gszStart, (int) start);
               NodeValueSet (pEntry, gszEnd, (int)end);
               NodeValueSet (pEntry, gszSummary, L"");
               NodeValueSet (pEntry, gszName, szTitle);

               for (i = 0; i < MAXPEOPLE; i++) {
                  // find the node number for the person's data and the name
                  DWORD dwNode;
                  dwNode = (DWORD) NodeValueGetInt(pCategory, gaszPerson[i], -1);

                  NodeValueSet (pEntry, gaszPerson[i], (int) dwNode);

                  // write out the people links
                  PCMMLNode pPerson;
                  pPerson = NULL;
                  if (dwNode != (DWORD)-1)
                     pPerson = gpData->NodeGet (dwNode);
                  if (pPerson) {
                     swprintf (szTemp, L"Journal entry: %s", szTitle);

                     // overwrite if already exists
                     NodeElemSet (pPerson, gszInteraction, szTemp, (int) dwEntry, TRUE,
                        date, start, end);

                     gpData->NodeRelease (pPerson);
                  }
               }

         
               // write out the daily log
               swprintf (szTemp, L"Journal entry: %s", szTitle);
               CalendarLogAdd (date, start, end, szTemp, dwEntry);


               // release
               gpData->NodeRelease (pEntry);

               // increase the date, and rese thte start time to the early hours
               date = MinutesToDFDATE (DFDATEToMinutes(date) + 24 * 60);
               start = 0;
            }


            // release
            gpData->NodeRelease (pCategory);
            gpData->Flush();
            pPage->Exit (gszOK);
            return TRUE;



         }  // add
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
JournalTimerEndUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   DWORD          dwIndex - Index returned by the link (generated by JournalTimerSubst)
returns
   BOOL - TRUE if the journal was changed, requiring a screen refresh
*/
BOOL JournalTimerEndUI (PCEscPage pPage, DWORD dwIndex)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLJOURNALTIMEREND, JournalTimerEndPage, (PVOID) dwIndex);

   return pszRet && (!_wcsicmp(pszRet, gszOK) || !_wcsicmp(pszRet, gszDelete));
}


