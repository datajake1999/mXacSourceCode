/*************************************************************************
Memory.cpp - Handles memory lane functionality.

begun 8/25/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


#define  MAXPEOPLE      4

static DWORD gdwEntryNode;    // for which memoy is being edited/vioewd
static DWORD gdwListNode;     // for which year is being viewed
static WCHAR gszPlace[] = L"Place";
static WCHAR gszMemoryType[] = L"MemoryType";
static WCHAR gszGoodMemory[] = L"GoodMemory";
static WCHAR gszEditedDate[] = L"EditedDate";
static WCHAR gszEditedStart[] = L"EditedStart";
static WCHAR gszMemoryLog[] = L"MemoryLog";
static WCHAR gszQuickAdd[] = L"quickadd";

/***********************************************************************
MemoryQuickAddPage - Page callback
*/
BOOL MemoryQuickAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // handle the add button
         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // get the name
            WCHAR szName[256];
            PCEscControl pControl;
            szName[0] = 0;
            pControl = pPage->ControlFind(gszName);
            DWORD dwNeeded;
            pControl->AttribGet (gszText, szName, sizeof(szName), &dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a one-line description.");
               return TRUE;
            }

            // else, add it
            PCMMLNode pNode;
            pNode = FindMajorSection (gszMemoryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            NodeElemSet (pNode, gszQuickAdd, szName, -1, FALSE, Today(), Now(), -1);
            gpData->NodeRelease(pNode);

            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Memory added to your quick-add list.");

            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}



/*****************************************************************************
MemoryQuickAdd - Adds a new memory to quick-add list.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   none
*/
void MemoryQuickAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMEMORYQUICKADD, MemoryQuickAddPage);
}

/*****************************************************************************
GetMemoryLogNode - Given a date, this returns a year-specific node for the
memory log.


inputs
   DFDATE         date - Date to look for. Only the year is used
   BOOL           fCreateIfNotExit - if TRUE create the database node
                  if it doesn't exist. If FALSE, return NULL if it doesn't exist.
   DWORD          *pdwNode - Filled in with a node specific to the month/year.
returns
   PCMMLNode - Node (must be released) specific to the month/year. NULL if cant find/create.
*/
PCMMLNode GetMemoryLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode)
{
   HANGFUNCIN;
   PCMMLNode   pNew;

   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszMemoryNode);
   if (!pNode)
      return FALSE;   // unexpected error

   pNew = DayMonthYearTree (pNode, TODFDATE(0, 0, YEARFROMDFDATE(date)), gszMemoryLog, fCreateIfNotExist, pdwNode);

   gpData->NodeRelease(pNode);

   return pNew;
}

/***********************************************************************
MemoryAdd - Add a new memory entry. The new entry is NOT registered anywhere.

inputs
   PWSTR    psz - Name string. This can be NULL
reutrns
   DWORD - database ID. -1 if error
*/
DWORD MemoryAdd (PWSTR psz)
{
   HANGFUNCIN;
   PCMMLNode pNode;
   DWORD dwID;
   pNode = gpData->NodeAdd (gszMemoryEntryNode, &dwID);
   if (!pNode)
      return (DWORD)-1;

   // set the name
   if (psz)
      NodeValueSet (pNode, gszName, psz);

   gpData->NodeRelease (pNode);
   return dwID;
}

/***********************************************************************
MemoryLanePage - Page callback
*/
BOOL MemoryLanePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // handle the add button
         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // get the new node
            DWORD dwNode;
            dwNode = MemoryAdd (NULL);
            if (dwNode == (DWORD)-1)
               break;

            // go to edit it
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszQuickAdd)) {
            MemoryQuickAdd (pPage);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

         // if it's a qf button
         WCHAR szqf[] = L"qf:";
         DWORD dwLen;
         dwLen = wcslen(szqf);
         if (!_wcsnicmp(p->pControl->m_pszName, szqf, dwLen)) {
            // it's a quick add
            DWORD dwIndex;
            DWORD dwFound;
            PWSTR pszName = p->pControl->m_pszName + dwLen;
            dwFound = (DWORD)-1;

            // find it
            PCMMLNode pNode;
            pNode = FindMajorSection (gszMemoryNode);
            if (!pNode)
               return FALSE;   // unexpected error

            for (dwIndex = 0; dwIndex < pNode->ContentNum(); dwIndex++) {
               PCMMLNode pSub;
               PWSTR psz;
               pSub = NULL;
               pNode->ContentEnum (dwIndex, &psz, &pSub);
               if (!pSub)
                  continue;

               // compare the name
               if (_wcsicmp(pSub->NameGet(), gszQuickAdd))
                  continue;

               // get the string
               PCMMLNode   pSub2;
               psz = NULL;
               pSub->ContentEnum(0, &psz, &pSub2);
               if (!psz)
                  continue;
               if (_wcsicmp(psz, pszName))
                  continue;

               // found it
               dwFound = dwIndex;

               break;
            }

            // if found it then delete the name
            if (dwFound != (DWORD)-1)
               pNode->ContentRemove (dwFound);

            gpData->NodeRelease(pNode);
            gpData->Flush();

            // now add this
            // get the new node
            DWORD dwNode;
            dwNode = MemoryAdd (pszName);
            if (dwNode == (DWORD)-1)
               break;

            // go to edit it
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;


         if (!_wcsicmp(p->pszSubName, L"QUICKLYADDED")) {
            MemZero (&gMemTemp);

            // find out what years have been entered
            PCMMLNode pNode;
            pNode = FindMajorSection (gszMemoryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            PCListFixed pl;
            pl = NodeListGet (pNode, gszQuickAdd, TRUE);

            // if blank then exit
            if (!pl || !pl->Num()) {
               gpData->NodeRelease(pNode);
               if (pl)
                  delete pl;
               p->pszSubString = (PWSTR)gMemTemp.p;
               return TRUE;
            }

            // else text
            MemCat (&gMemTemp, L"<xbr/><xSectionTitle>Quickly-Added Memories</xSectionTitle>");
            MemCat (&gMemTemp, L"<p>While you were using other parts of Dragonfly you \"quickly added\" some memories. ");
            MemCat (&gMemTemp, L"Their titles are stored in Dragonfly but some information about them is ");
            MemCat (&gMemTemp, L"incomplete. When you have the chance, please fill in the rest of the memory.</p>");
            MemCat (&gMemTemp, L"<blockquote><p>");

            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               if (!pnlg->psz)
                  continue;
               
               // write it out
               MemCat (&gMemTemp, L"<xQuickFillButton name=\"qf:");
               MemCatSanitize (&gMemTemp, pnlg->psz);
               MemCat (&gMemTemp, L"\">");
               MemCatSanitize (&gMemTemp, pnlg->psz);
               MemCat (&gMemTemp, L"</xQuickFillButton>");
            }

            // finish
            MemCat (&gMemTemp, L"</p></blockquote>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            delete pl;
            return TRUE;
         }


         if (!_wcsicmp(p->pszSubName, L"MEMORIESENTERED")) {
            MemZero (&gMemTemp);

            // find out what years have been entered
            PCMMLNode pNode;
            pNode = FindMajorSection (gszMemoryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            PCListFixed pl;
            pl = EnumMonthYearTree (pNode, gszMemoryLog);
            gpData->NodeRelease(pNode);

            // if blank then exit
            if (!pl || !pl->Num()) {
               if (pl)
                  delete pl;
               p->pszSubString = (PWSTR)gMemTemp.p;
               return TRUE;
            }

            // else text
            MemCat (&gMemTemp, L"<xbr/><xsectiontitle>Memories you've entered</xsectiontitle>");
            MemCat (&gMemTemp, L"<p>Click the year to see what memories you've entered for that year.</p>");
            MemCat (&gMemTemp, L"<xtablecenter align=center><tr>");

            // how long
#define COLUMNS      4
            DWORD dwRows, dwColumns;
            dwRows = (pl->Num()+COLUMNS-1)/COLUMNS;
            if (dwRows < 4)
               dwRows = 4;
            dwColumns = (pl->Num()+dwRows-1)/dwRows;

            DWORD x,y;
            for (x = 0; x < dwColumns; x++) {
               MemCat (&gMemTemp, L"<td><big>");

               for (y = 0; y < dwRows; y++) {
                  DFDATE *pd;
                  pd = (DFDATE*) pl->Get(x * dwRows + y);
                  if (!pd)
                     break;   // out of bounds

                  // else link
                  PCMMLNode pYear;
                  DWORD dwYear;
                  pYear = GetMemoryLogNode(*pd, FALSE, &dwYear);
                  if (pYear)
                     gpData->NodeRelease(pYear);

                  MemCat (&gMemTemp, L"<a href=v:");
                  MemCat (&gMemTemp, (int) dwYear);
                  MemCat (&gMemTemp, L">");
                  MemCat (&gMemTemp, (int) YEARFROMDFDATE(*pd));
                  MemCat (&gMemTemp, L"</a><br/>");

               }

               MemCat (&gMemTemp, L"</big></td>");
            }

            // finish
            MemCat (&gMemTemp, L"</tr></xtablecenter>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            delete pl;
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
MemoryEditPage - Page callback
*/
BOOL MemoryEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set up parameters
         PWSTR psz;
         PCEscControl pControl;
         PCMMLNode pNode;
         pNode = gpData->NodeGet (gdwEntryNode);
         if (!pNode)
            break;

         psz = NodeValueGet (pNode, gszName);
         pControl = pPage->ControlFind (gszName);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);

         psz = NodeValueGet (pNode, gszSummary);
         pControl = pPage->ControlFind (gszSummary);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);

         DateControlSet (pPage, gszMeetingDate, NodeValueGetInt (pNode, gszDate, 0));

         psz = NodeValueGet (pNode, gszPlace);
         pControl = pPage->ControlFind (gszPlace);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);

         // people
         DWORD i;
         DWORD dwNum, dwIndex;
         for (i = 0; i < MAXPEOPLE; i++) {
            pControl = pPage->ControlFind (gaszPerson[i]);
            dwNum = (DWORD)NodeValueGetInt (pNode, gaszPerson[i]);
            dwIndex = PeopleBusinessDatabaseToIndex (dwNum);
            if (pControl && (dwIndex != (DWORD)-1))
               pControl->AttribSetInt (gszCurSel, (int) dwIndex);
         }
         
         // journal
         pControl = pPage->ControlFind (gszJournal);
         dwNum = (DWORD)NodeValueGetInt (pNode, gszJournal, -1);
         dwIndex = JournalDatabaseToIndex (dwNum);
         if (pControl && (dwIndex != (DWORD)-1))
            pControl->AttribSetInt (gszCurSel, (int) dwIndex);

         psz = NodeValueGet (pNode, gszMemoryType);
         pControl = pPage->ControlFind (gszMemoryType);
         ESCMCOMBOBOXSELECTSTRING item;
         memset (&item, 0, sizeof(item));
         item.fExact = TRUE;
         item.dwIndex = (DWORD)-1;
         item.iStart = 0;
         item.psz = psz;
         if (pControl && psz)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &item);

         psz = NodeValueGet (pNode, gszGoodMemory);
         pControl = pPage->ControlFind (gszGoodMemory);
         memset (&item, 0, sizeof(item));
         item.fExact = TRUE;
         item.dwIndex = (DWORD)-1;
         item.iStart = 0;
         item.psz = psz;
         if (pControl && psz)
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &item);

         gpData->NodeRelease(pNode);
      }
      break;   // fall through

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
         if (!szName[0]) {
            pPage->MBWarning (L"You must type in a one-line description for the memory.");
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         NodeValueSet (pNode, gszName, szName);

         // date that happens
         DFDATE when;
         when = DateControlGet (pPage, gszMeetingDate);
         if (when == 0) {
            pPage->MBWarning (L"You must enter a month and year when the memory occurred.");
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         when = TODFDATE(0, MONTHFROMDFDATE(when), YEARFROMDFDATE(when));
         NodeValueSet (pNode, gszDate, (int) when);

         pControl = pPage->ControlFind(gszSummary);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszSummary, szTemp);

         pControl = pPage->ControlFind(gszPlace);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszPlace, szTemp);

         // two comboboxes
         WriteCombo (pNode, gszMemoryType, pPage, gszMemoryType);
         WriteCombo (pNode, gszGoodMemory, pPage, gszGoodMemory);


         // also store away now so know when last edited
         DFDATE date;
         DFTIME start;
         date = Today();
         start = Now();
         NodeValueSet (pNode, gszEditedDate, (int) date);
         NodeValueSet (pNode, gszEditedStart, start);

         // people
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
               // overwrite if already exists
               NodeElemSet (pPerson, gszMemory, szName, (int) gdwEntryNode, TRUE,
                  when, -1, -1);

               gpData->NodeRelease (pPerson);
            }
         }

         // journal
         DWORD dwJournal;
         dwJournal = (DWORD)-1;
         pControl = pPage->ControlFind (gszJournal);
         if (pControl) {
            int   iCurSel;
            iCurSel = pControl->AttribGetInt (gszCurSel);

            // find the node number for the person's data and the name
            DWORD dwNode;
            dwJournal = dwNode = JournalIndexToDatabase ((DWORD)iCurSel);

            NodeValueSet (pNode, gszJournal, (int) dwNode);
         }

         // write into memory log
         DWORD dwID;
         PCMMLNode pLog;
         pLog = GetMemoryLogNode (when, TRUE, &dwID);
         if (pLog) {
            NodeElemSet (pLog, gszMemory, szName, (int) gdwEntryNode, TRUE,
               when, -1, -1);

            // sdet the date
            NodeValueSet (pLog, gszDate, when);

            gpData->NodeRelease (pLog);
         }

         // write out the daily log
         swprintf (szTemp, L"Add memory: %s", szName);
         CalendarLogAdd (date, start, -1, szTemp, gdwEntryNode);

         if (dwJournal != (DWORD)-1)
            JournalLink (dwJournal, szTemp, gdwEntryNode, date, start, -1);

         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszQuickAdd)) {
            MemoryQuickAdd (pPage);
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
MemoryViewPage - Page callback
*/
BOOL MemoryViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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

         if (!_wcsicmp(p->pszSubName, L"WHERE")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszPlace);
            if (psz)
               MemCat (&gMemTemp, psz);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"TYPE")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszMemoryType);
            if (psz)
               MemCat (&gMemTemp, psz);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"GOODMEMORY")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwEntryNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszGoodMemory);
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
               DFDATEToString ((DFDATE) NodeValueGetInt (pNode, gszEditedDate, 0), szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString ((DFTIME) NodeValueGetInt (pNode, gszEditedStart, -1), szTemp);
               MemCat (&gMemTemp, szTemp);
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"WHEN")) {
            PCMMLNode pNode;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwEntryNode);
            if (pNode) {
               WCHAR szTemp[64];
               PCMMLNode pLog;
               DWORD dwLog;
               DFDATE   date;
               date = (DFDATE) NodeValueGetInt (pNode, gszDate, 0);
               date = TODFDATE(0,MONTHFROMDFDATE(date),YEARFROMDFDATE(date));
               pLog = GetMemoryLogNode(date, FALSE, &dwLog);
               if (pLog)
                  gpData->NodeRelease (pLog);
               DFDATEToString (date, szTemp);
               MemCat (&gMemTemp, L"<a href=v:");
               MemCat (&gMemTemp, (int) dwLog);
               MemCat (&gMemTemp, L">");
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</a>");
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

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // handle the add button
         if (!_wcsicmp(p->pControl->m_pszName, L"edit")) {
            // go to edit it
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwEntryNode);
            pPage->Exit (szTemp);
         }

         if (!_wcsicmp(p->pControl->m_pszName, gszQuickAdd)) {
            MemoryQuickAdd (pPage);
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
MemoryListPage - Page callback
*/
BOOL MemoryListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         // only care about projectlist
         if (!_wcsicmp(p->pszSubName, L"LOGDATE")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwListNode);
            if (!pNode)
               return FALSE;

            // get the date
            DFDATE date;
            date = NodeValueGetInt (pNode, gszDate, 0);
            MemCat (&gMemTemp, (int) YEARFROMDFDATE(date));

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"LOG")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwListNode);
            if (pNode)
               pl = NodeListGet (pNode, gszMemory, TRUE);

            NLG *pnlg;
            DWORD i;

            // BUGFIX - randomly check the integrity of a memory, and remove it if not there
            if (pl && pl->Num()) {
               i = rand() % pl->Num();
               pnlg = (NLG*) pl->Get(i);

               PCMMLNode pSub = gpData->NodeGet (pnlg->iNumber);
               if (!pSub)
                  goto remove;

               DFDATE date = (DFDATE) NodeValueGetInt (pSub, gszDate, 0);
               if (YEARFROMDFDATE(date) != YEARFROMDFDATE(pnlg->date)) {
remove:
                  NodeElemRemove (pNode, gszMemory, pnlg->iNumber);

                  // rescan
                  delete pl;
                  pl = NULL;
                  pl = NodeListGet (pNode, gszMemory, TRUE);
               }
            } // random check

            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr>");
               MemCat (&gMemTemp, L"<xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask>");
               MemCat (&gMemTemp, L"<xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // if no entries then say so
            if (!pl || !pl->Num())
               MemCat (&gMemTemp, L"<tr><td>None</td></tr>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

      }
      break;


   };

   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
RandomSuggestion - Randomly come up with a suggestion. Fills gMemTemp
with the string (MML).

BUGBUG - 2.0 - At some point pull out random vocabulary word from search.
Although isn't that important right now
*/
void RandomSuggestion (void)
{
   HANGFUNCIN;
   int   iRand;
retry:
   iRand = rand() % 5;

   switch (iRand) {
   case 0:  // random picture
      {
         // find the photo first
         DWORD dwNode;
         dwNode = PhotoRandom ();
         if (dwNode == (DWORD)-1)
            goto retry;

         WCHAR szTemp[256];
         if (!PhotosFileName (dwNode, szTemp))
            goto retry;

         MemCat (&gMemTemp, L"Does this photo stir any memories?<p/>");
         MemCat (&gMemTemp, L"<p align=center><image width=80%% file=\"");
         MemCatSanitize (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"\" href=v:");
         MemCat (&gMemTemp, dwNode);
         MemCat (&gMemTemp, L" border=2/></p>");
      }
      break;
   case 1:  // dealing with a person's name
      {
         // come up with someone's name
         PCListVariable pl = PeopleFilteredList();
         if (!pl)
            goto retry;
         if (!pl->Num())
            goto retry;
         PWSTR pszName;
         DWORD dwIndex;
         dwIndex = (DWORD) rand() % pl->Num();
         pszName = (PWSTR) pl->Get(dwIndex);
         if (!pszName)
            goto retry;

         // another random
         PWSTR pszBefore, pszAfter;
         pszBefore = L"";
         pszAfter = L"?";
         switch (rand() % 6) {
         case 0:  // memories about this person
            pszBefore = L"Can you remember anything about ";
            break;

         case 1:  // parties with
            pszBefore = L"Did you go to any parties with ";
            break;

         case 2:  // relative of
            pszBefore = L"Did you ever meet any relatives of ";
            break;

         case 3:  // friends of
            pszBefore = L"Did you and ";
            pszAfter = L" have any friends in common?";
            break;

         case 4:  // good times with
            pszBefore = L"Do you remember any good times with ";
            break;

         case 5:  // arguements with
            pszBefore = L"Did you ever have any arguements with ";
            break;

         default:
            goto retry;
         }

         MemCat (&gMemTemp, pszBefore);
         MemCat (&gMemTemp, L"<a href=v:");
         MemCat (&gMemTemp, (int) PeopleIndexToDatabase(dwIndex));
         MemCat (&gMemTemp, L">");
         MemCatSanitize (&gMemTemp, pszName);
         MemCat (&gMemTemp, L"</a>");
         MemCat (&gMemTemp, pszAfter);
      }
      break;

   case 4:  // dealing with another memory
      {
         // come up with another memory

         // pick a random year
         PCMMLNode pNode;
         pNode = FindMajorSection (gszMemoryNode);
         if (!pNode)
            goto retry;   // unexpected error
         PCListFixed pl;
         pl = EnumMonthYearTree (pNode, gszMemoryLog);
         gpData->NodeRelease(pNode);
         if (!pl)
            goto retry;
         if (!pl->Num()) {
            delete pl;
            goto retry;
         }
         DFDATE   year;
         year = *((DFDATE*) pl->Get((DWORD)rand() % pl->Num()));
         delete pl;

         // from the year pick a random list
         PCMMLNode pYear;
         DWORD dwNode;
         pYear = GetMemoryLogNode (year, FALSE, &dwNode);
         if (!pYear)
            goto retry;

         PCListFixed pl2;
         pl2 = NULL;
         pl2 = NodeListGet (pYear, gszMemory, TRUE);

         if (!pl2 || !pl2->Num()) {
            if (pl2)
               delete pl2;
            gpData->NodeRelease (pYear);
            goto retry;
         }

         // pick random
         NLG *pnlg;
         pnlg = (NLG*) pl2->Get((DWORD)rand() % pl2->Num());
         if (!pnlg->psz) {
            if (pl2)
               delete pl2;
            gpData->NodeRelease (pYear);
            goto retry;
         }

         // string
         PWSTR pszBefore, pszAfter;
         pszBefore = L"";
         pszAfter = L"?";
         switch (rand() % 5) {
         default:
         case 0:
            pszBefore = L"Does the memory, ";
            pszAfter = L" stir any more memories?";
            break;
         case 1:
            pszBefore = L"What happened before the memory, ";
            break;
         case 2:
            pszBefore = L"What happened after the memory, ";
            break;
         case 3:
            pszBefore = L"Do you remember anything else that happened in the same place as the memory, ";
            break;
         case 4:
            pszBefore = L"Can you remember anything else about the group of people in the memory, ";
            break;
         }

         MemCat (&gMemTemp, pszBefore);
         MemCat (&gMemTemp, L"<a href=v:");
         MemCat (&gMemTemp, (int) pnlg->iNumber);
         MemCat (&gMemTemp, L">");
         MemCatSanitize (&gMemTemp, pnlg->psz);
         MemCat (&gMemTemp, L"</a>");
         MemCat (&gMemTemp, pszAfter);


         gpData->NodeRelease (pYear);
         delete pl2;


      }
      break;

   case 2:  // dealing with a random date
      {
         // pick a random year
         PCMMLNode pNode;
         pNode = FindMajorSection (gszMemoryNode);
         if (!pNode)
            goto retry;   // unexpected error
         PCListFixed pl;
         pl = EnumMonthYearTree (pNode, gszMemoryLog);
         gpData->NodeRelease(pNode);
         if (!pl)
            goto retry;
         if (!pl->Num()) {
            delete pl;
            goto retry;
         }
         int   iFirstYear, iToday, iRand;
         DFDATE   year;
         year = *((DFDATE*) pl->Get(0));
         delete pl;
         iFirstYear = YEARFROMDFDATE(year);
         iToday = YEARFROMDFDATE(Today());
         if (iToday <= iFirstYear)
            iRand = iToday;
         else
            iRand = (rand() % (iToday - iFirstYear+1)) + iFirstYear;
         year = TODFDATE (0,0,iRand);


         // and a random month
         DFDATE date;
         date = TODFDATE (0, (rand() % 12)+1, YEARFROMDFDATE(year));

         PWSTR pszBefore, pszAfter;
         pszBefore = L"";
         pszAfter = L"?";

         switch (rand() % 5) {
         case 0:
            pszBefore = L"Do you have any memories from ";
            break;
         case 1:
            pszBefore = L"Did you attend any parties in ";
            break;
         case 2:
            pszBefore = L"Did you meet any new friends in ";
            break;
         case 3:
            pszBefore = L"Did you take any vacations in ";
            break;
         case 4:
            pszBefore = L"Do you remember any religious events in ";
            break;
         }

         MemCat (&gMemTemp, pszBefore);
         switch (rand() % 3) {
         case 0:
            // full month year
            WCHAR szTemp[64];
            DFDATEToString (date, szTemp);
            MemCat (&gMemTemp, szTemp);
            break;
         case 1: // year
            MemCat (&gMemTemp, (int) YEARFROMDFDATE(date));
            break;
         case 2: // month
            MemCat (&gMemTemp, gaszMonth[MONTHFROMDFDATE(date)-1]);
            break;
         }
         MemCat (&gMemTemp, pszAfter);
      }
      break;

   case 3:  // just a suggestion
      {
         PWSTR apsz[] = {
            L"Do you have any memories with relatives?",
            L"Did you take any vacations?",
            L"Did you every have anything bad happen on a vacation?",
            L"Do you have any memories about people on vacation with you?",
            L"Do you have any memories about school?",
            L"Do you have memories about your teachers?",
            L"Do you have memories about your classmates?",
            L"What happened on the first day of school?",
            L"Did anyone bully you in school?",
            L"Did you have any favorite toys?",
            L"What did you do after school?",
            L"What did you do on the way to school?",
            L"Did you play any games with friends?",
            L"Do you have any sports storied?",
            L"What did you do besides watch television?",
            L"When did you first see the milky way?",
            L"Where were you when JFK was assassinated?",
            L"Where were you when the space shuttle Challenger blew up?",
            L"Where were you when the man landed on the moon?",
            L"Did your friends do anything funny?",
            L"Do you have memories of your school friends?",
            L"Do you fondly remember any religious holidays?",
            L"Think of a smell (such as pine) and remember the last time you smelled it.",
            L"Can you recall any sad times?",
            L"Have you ever been embarassed?",
            L"Can you recall any happy times?",
            L"Do you remember the day you graduated?",
            L"Do you remember your first day of work?",
            L"What did you do at work?",
            L"What did your coworkers do?",
            L"Do you remember any particularly good meals?",
            L"Do you remember any really awful meals?",
            L"Did you ever have any fights with friends/relatives?",
            L"What are some memories of your parents?",
            L"What is your earliest childhood memory?",
            L"What are some memories of your siblings?",
            L"What are some memories of your children?",
            L"Where have you vacationed?",
            L"Do you remember when you slept over at your friend's house?",
            L"Where did you go sightseeing?",
            L"Think of a sound (like a peacock's call) and remember the last time you heard it.",
            L"Did you ever attend any music concerts?",
            L"Were you ever pulled over by the police for speeding?",
            L"Did you every act in your school play?",
            L"When was the first time you met your spouse?",
            L"Any fishing or boating memories?",
            L"Can you recall any trips to the beach?",
            L"Have you ever been out of the country?",
            L"Do you recall the first time you were on an airplane?"
         };

         MemCat (&gMemTemp, apsz[rand() % (sizeof(apsz) / sizeof(PWSTR))]);
      }
      break;

   default:
      // shouldn't get this
      goto retry;
   }
}

/***********************************************************************
MemorySuggestPage - Page callback
*/
BOOL MemorySuggestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"SUGGESTION")) {
            MemZero (&gMemTemp);

            RandomSuggestion ();

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

         // handle the add button
         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // get the new node
            DWORD dwNode;
            dwNode = MemoryAdd (NULL);
            if (dwNode == (DWORD)-1)
               break;

            // go to edit it
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, gszQuickAdd)) {
            MemoryQuickAdd (pPage);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************
MemoryEntrySet - Sets the category node to view
*/
void MemoryEntrySet (DWORD dwNode)
{
   HANGFUNCIN;
   gdwEntryNode = dwNode;
}

/**********************************************************************
MemoryListSet - Sets the category node to view
*/
void MemoryListSet (DWORD dwNode)
{
   HANGFUNCIN;
   gdwListNode = dwNode;
}

