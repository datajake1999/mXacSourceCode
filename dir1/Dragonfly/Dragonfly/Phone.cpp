/**********************************************************************
Phone.cpp - Code that handles telephone UI and interaction.

begun 8/19/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <tapi.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


/* C++ objects */

static WCHAR gszPhoneCall[] = L"Phonecall";
static WCHAR gszOutgoing[] = L"Outgoing";
static WCHAR gszIncoming[] = L"Incoming";
static WCHAR gszPhoneNum[] = L"PhoneNum";
static WCHAR gszNoAnswer[] = L"Noanswer";
static WCHAR gszBusy[] = L"Busy";
static WCHAR gszLeftMessage[] = L"LeftMessage";
static WCHAR gszSpeedDial[] = L"SpeedDial";
static WCHAR gszCall[] = L"Call";
static WCHAR gszAttend[] = L"Attend";
static WCHAR gszNextCallID[] = L"NextCallID";
static WCHAR gszCallDate[] = L"MeetingDate";
static WCHAR gszCallStart[] = L"MeetingStart";
static WCHAR gszCallEnd[] = L"MeetingEnd";

static DWORD gdwNode = 0;  // current node viewing
static DFDATE gdateLog = Today();   // set the default date to view in the call log to this month
static DFDATE gToday = 0;

BOOL           gfPhonePopulated = FALSE;// set to true if glistPhone is populated
CPhone       gCurPhone;      // current Phone that Phoneing with
static DWORD gdwActionPersonID = (DWORD)-1;  // for action item dialog

PCMMLNode GetCallLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode);
PWSTR PhoneSummary (BOOL fAll, DFDATE date);

/***********************************************************************
PhoneInit - Makes sure the Phone object is filled. If not,
it's loaded in. This CAN be called multiple times.

This fills in:
   gfPhonePopulated - Set to TRUE when it's been loaded
   gCurPhone - fills in

inputs
   none
returns
   BOOL - error
*/
BOOL PhoneInit (void)
{
   HANGFUNCIN;
   if (gfPhonePopulated)
      return TRUE;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszPhoneNode, &dwNum);
   if (!pNode)
      return NULL;   // error. This shouldn't happen
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfPhonePopulated = TRUE;

   // pass this on
   return gCurPhone.Init (dwNum);
}




/***********************************************************************
PhoneLogCall - Given a dwNode for a phonecall notes node, this
reads in the info and logs the call in 1) the call log,
2) the person's data, and 3) the daily journa, and 4) if link the journal category.

inputs
   DWORD    dwNode - Node for the phone call notes
*/
void PhoneLogCall (DWORD dwNode)
{
   HANGFUNCIN;
   // get all the info
   PCMMLNode   pNotes;
   pNotes = gpData->NodeGet(dwNode);
   if (!pNotes)
      return;

   PWSTR pszName, pszPhoneNum, pszSummary;
   DFDATE date;
   DFTIME start, end;
   int   iOutgoing;
   DWORD dwPerson, dwJournal;

   pszPhoneNum = NodeValueGet (pNotes, gszPhoneNum);
   pszSummary = NodeValueGet (pNotes, gszSummary);
   dwPerson = (DWORD)NodeValueGetInt (pNotes, gszPerson, -1);
   date = (DFDATE)NodeValueGetInt (pNotes, gszDate, 0);
   start = (DFTIME)NodeValueGetInt (pNotes, gszStart, -1);
   end = (DFTIME)NodeValueGetInt (pNotes, gszEnd, -1);
   iOutgoing = NodeValueGetInt (pNotes, gszOutgoing, 1);
   pszName = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(dwPerson));
   dwJournal = (DWORD)NodeValueGetInt (pNotes, gszJournal, -1);

   // friendly name
   PWSTR pszFriendly;
   pszFriendly = pszName ? pszName : ((pszPhoneNum && pszPhoneNum[0]) ? pszPhoneNum : L"Unknown");

   // call log
   PCMMLNode pCallLog;
   DWORD dwCallLog;
   pCallLog = GetCallLogNode (date, TRUE, &dwCallLog);
   if (pCallLog) {
      // overwrite if already exists
      NodeElemSet (pCallLog, gszPhoneCall, pszFriendly, (int) dwNode, TRUE,
         date, start, end);

      gpData->NodeRelease (pCallLog);
   }

   // person
   PCMMLNode pPerson;
   pPerson = NULL;
   if (dwPerson != (DWORD)-1)
      pPerson = gpData->NodeGet (dwPerson);
   if (pPerson) {
      PWSTR pszString = L"Telephone conversation.";
      // overwrite if already exists
      NodeElemSet (pPerson, gszInteraction, pszString, (int) dwNode, TRUE,
         date, start, end);

      gpData->NodeRelease (pPerson);

   }

   // log in the daily journal
   WCHAR szHuge[10000];
   wcscpy (szHuge, L"Phone conversation: ");
   wcscat (szHuge, pszFriendly);
   CalendarLogAdd (date, start, end, szHuge, dwNode);

   if (dwJournal != (DWORD)-1) {
      JournalLink (dwJournal, szHuge, dwNode, date, start, end);
   }
   if (dwPerson != (DWORD)-1) {
      // also note this in the journal log
      PersonLinkToJournal (dwPerson, szHuge, dwNode, date, start, end);
   }


   gpData->NodeRelease (pNotes);
   gpData->Flush();
}

/***********************************************************************
PhoneCall - Actually call someone (optional), create a new entry
   for a phone call log, and returns the database node for that log.
   It's then the caller's responsibiliy to do "e:XXXX" where XXXX
   is the returned number.

inputs
   PCEscPage pPage - Page to display error message off of
   BOOL     fReallyCall - If set to TRUE TAPI is really used to call.
               Else, nothing is actually dialed.
   BOOL     fOutgoing - If TRUE defaults to outgoing, else incoming
   DWORD    dwPerson - Persson node. -1 if no person
   PWSTR    pszNumber - Phone number. This can be NULL. This must be
               set if fReallyCall is set.
returns
   DWORD - Call notes ID. -1 if error.
*/
DWORD PhoneCall (PCEscPage pPage, BOOL fReallyCall, BOOL fOutgoing,
                 DWORD dwPerson, PWSTR pszNumber)
{
   HANGFUNCIN;
   // get the person's name
   PWSTR pszName;
   pszName = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(dwPerson));

   // TAPI?
   if (fReallyCall) {
      // BUGFIX - User reported problem that cant link to tapi 32
      HMODULE hMod = LoadLibrary ("tapi32.dll");
      if (!hMod) {
         pPage->MBWarning (L"Windows reported an error when making the phone call.",
            L"The modem might be busy, improperly installed, or your TAPI assisted-call application unavailable.");
         goto tapierr;
      }
      typedef LONG (WINAPI *TtapiRequest) (
         LPCSTR              lpszDestAddress,
         LPCSTR              lpszAppName,
         LPCSTR              lpszCalledParty,
         LPCSTR              lpszComment
         );
      TtapiRequest tapiRequest;
      
      tapiRequest = (TtapiRequest) GetProcAddress (hMod, "tapiRequestMakeCallA");
      if (!tapiRequest) {
         FreeLibrary (hMod);
         pPage->MBWarning (L"Windows reported an error when making the phone call.",
            L"The modem might be busy, improperly installed, or your TAPI assisted-call application unavailable.");
         goto tapierr;
      }

      // NOTE - this should work but doesn't seem to work on win95
      // Gets the error 0x80000049 - not supported
      // maybe something on this machine is busted

      // get the person's name in ASCII
      char  szName[256];
      szName[0] = 0;
      if (pszName)
         WideCharToMultiByte (CP_ACP, 0, pszName, -1, szName, sizeof(szName), 0, 0);

      // dial thia
      char  szTemp[128];
      WideCharToMultiByte (CP_ACP, 0, pszNumber, -1, szTemp, sizeof(szTemp),0,0);
      int iRet;
      iRet = tapiRequest (szTemp, "Dragonfly", pszName ? szName : NULL, NULL);
      if (iRet < 0) {
         // error
         pPage->MBWarning (L"Windows reported an error when making the phone call.",
            L"The modem might be busy, improperly installed, or your TAPI assisted-call application unavailable.");
         // continue on anyway
      }

      FreeLibrary (hMod);
   }

tapierr:
   // create the node
   DWORD dwNode;
   PCMMLNode pNode;
   pNode = gpData->NodeAdd (gszPhoneNotesNode, &dwNode);
   if (!pNode)
      return (DWORD) -1;

   // write some information
   NodeValueSet (pNode, gszName, pszName ? pszName : (pszNumber ? pszNumber : gszPhoneCall) );
      // writing out the name so that search will have something to latch on to
   NodeValueSet (pNode, gszOutgoing, (int) fOutgoing);
   NodeValueSet (pNode, gszPerson, (int)dwPerson);
   NodeValueSet (pNode, gszPhoneNum, pszNumber ? pszNumber : L"");
   NodeValueSet (pNode, gszDate, (int)Today());
   NodeValueSet (pNode, gszStart, (int)Now());
   NodeValueSet (pNode, gszEnd, (int)-1);
   NodeValueSet (pNode, gszSummary, L"");

   // generate a call log
   PhoneLogCall (dwNode);

   // done
   gpData->NodeRelease (pNode);
   return dwNode;
}


/***************************************************************************
PhoneSetView - Tells the phone unit what phone call is being viewed/edited.

inputs
   DWORD    dwNode - index
returns
   BOOL - TRUE if success
*/
BOOL PhoneSetView (DWORD dwNode)
{
   gdwNode = dwNode;
   return TRUE;
}

/***********************************************************************
PhoneNotesViewPage - Page callback for viewing a phone message
*/
BOOL PhoneNotesViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         // make sure intiialized
         PhoneInit ();
         MemZero (&gMemTemp);
         PCMMLNode   pNode;
         PWSTR psz;
         
         if (!_wcsicmp(p->pszSubName, L"CALLORIGIN")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               int   iOut;
               iOut = NodeValueGetInt (pNode, gszOutgoing, 1);
               MemCat (&gMemTemp, iOut ? L"You made the phone call." : L"You received the phone call.");
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"DESCRIPTION")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               psz = NodeValueGet (pNode, gszPhoneNum);
               MemCat (&gMemTemp, (psz && psz[0]) ? psz : L"Unknown");
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"TIME")) {
            pNode = gpData->NodeGet (gdwNode);
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

         else if (!_wcsicmp(p->pszSubName, L"PERSON")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               DWORD dwData;
               dwData = (DWORD) NodeValueGetInt (pNode, gszPerson, -1);
               PWSTR psz;
               psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(dwData));

               if (psz) {
                  // make a link
                  MemCat (&gMemTemp, L"<a href=v:");
                  MemCat (&gMemTemp, (int) dwData);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"</a><br/>");
               }
               else
                  MemCat (&gMemTemp, L"Unknown");

               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"MEETINGNOTES")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               psz = NodeValueGet (pNode, gszSummary);
               MemCat (&gMemTemp, (psz && psz[0]) ? psz : L"None");
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
            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }

         break;
      }
   };

   return DefPage (pPage, dwMessage, pParam);
}




/***********************************************************************
AppendString - Appends a string to the page's info

inputs
   PCEscPage   pPage - page
   PWSTR       psz - string
returns
   none
*/
static void AppendString (PCEscPage pPage, PWSTR psz)
{
   HANGFUNCIN;
   // set the selection and append to the end
   PCEscControl pControl;
   pControl = pPage->ControlFind (gszSummary);
   pControl->AttribSet (L"selend", L"1000000");
   pControl->AttribSet (L"selstart", L"1000000");
   ESCMEDITREPLACESEL rep;
   memset (&rep, 0, sizeof(rep));
   rep.dwLen = wcslen(psz);
   rep.psz = psz;
   pControl->Message (ESCM_EDITREPLACESEL, &rep);
   pControl->AttribSet (L"selend", L"1000000");
   pControl->AttribSet (L"selstart", L"1000000");
   pControl->Message (ESCM_EDITSCROLLCARET);
   pPage->FocusSet (pControl);
}


/***********************************************************************
PhoneClearToDo - If scheduled to call someone today and end up calling
them then remove the call from the list.

inputs
   DWORD    dwPerson - Person node.
returns
   none
*/
void PhoneClearToDo (DWORD dwPerson)
{
   HANGFUNCIN;
   PhoneInit();
   DFDATE today = Today();

   DWORD i;
   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      PCPhoneCall pChild = gCurPhone.CallGetByIndex(i);
      if (!pChild || pChild->m_reoccur.m_dwPeriod)
         continue;
      if ((pChild->m_dwDate != today) && pChild->m_dwDate)
         continue;
      if (pChild->m_dwAttend != dwPerson)
         continue;

      // found, so remove
      gCurPhone.m_listCalls.Remove(i);
      delete pChild;
      gCurPhone.m_fDirty = TRUE;
      gCurPhone.Flush();
      return;  // all done
   }

}


/***********************************************************************
PhoneNotesEditPage - Page callback for viewing an existing user.
*/
BOOL PhoneNotesEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // just make sure have loaded in people info
         PhoneInit();

         // fill in details
         PCEscControl pControl;
         PCMMLNode pNode;
         PWSTR psz;
         pNode = gpData->NodeGet (gdwNode);
         if (!pNode)
            break;

         psz = NodeValueGet (pNode, gszPhoneNum);
         pControl = pPage->ControlFind(gszPhoneNum);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);
         psz = NodeValueGet (pNode, gszSummary);
         pControl = pPage->ControlFind(gszSummary);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);

         // outgoing
         int   iOut;
         iOut = NodeValueGetInt (pNode, gszOutgoing, 2);
         pControl = pPage->ControlFind (gszOutgoing);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, !(!iOut));
         pControl = pPage->ControlFind (gszIncoming);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, !iOut);

         // other data
         DateControlSet (pPage, gszDate, (DFDATE) NodeValueGetInt(pNode, gszDate, 0));
         TimeControlSet (pPage, gszStart, (DFTIME) NodeValueGetInt(pNode, gszStart, -1));
         TimeControlSet (pPage, gszEnd, (DFTIME) NodeValueGetInt(pNode, gszEnd, -1));

         DWORD dw;
         dw = PeopleBusinessDatabaseToIndex((DWORD)NodeValueGetInt(pNode, gszPerson, -1));
         pControl = pPage->ControlFind (gszPerson);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) dw);

         dw = JournalDatabaseToIndex((DWORD)NodeValueGetInt(pNode, gszJournal, -1));
         pControl = pPage->ControlFind (gszJournal);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) dw);

         
         gpData->NodeRelease(pNode);
      }
      break;

   case ESCN_FILTEREDLISTCHANGE:
      {
         PESCNFILTEREDLISTCHANGE p = (PESCNFILTEREDLISTCHANGE) pParam;

         // if the user changes the person then show a phone number from the person

         // only care if selected add
         if (p->iCurSel < 0)
            break;

         WCHAR szListName[128];
         DWORD dwNeeded;

         // only care if the phone number is blank
         PCEscControl pEdit;
         pEdit = pPage->ControlFind (gszPhoneNum);
         if (!pEdit)
            break;
         szListName[0] = 0;
         pEdit->AttribGet(gszText, szListName, sizeof(szListName), &dwNeeded);
         if (szListName[0])
            break;   // already entered


         // get the type of list
         szListName[0] = 0;
         p->pControl->AttribGet(L"ListName", szListName, sizeof(szListName), &dwNeeded);

         // only care about gszPerson
         if (_wcsicmp(szListName, gszPerson))
            break;

         // get the person
         DWORD dwNode;
         dwNode = PeopleBusinessIndexToDatabase ((DWORD)p->iCurSel);
         if (dwNode == (DWORD)-1)
            break;
         PCMMLNode pNode;
         pNode = gpData->NodeGet (dwNode);
         if (!pNode)
            break;

         // phone numbers
         PWSTR psz;
         psz = NodeValueGet (pNode, gszWorkPhone);
         if (!psz || !psz[0])
            psz = NodeValueGet (pNode, gszHomePhone);
         if (!psz || !psz[0])
            psz = NodeValueGet (pNode, gszMobilePhone);
         if (psz)
            pEdit->AttribSet (gszText, psz);

         gpData->NodeRelease(pNode);

      }
      return TRUE;

   case ESCM_LINK:
      {
         // this doesn't actually trap and links; it just saves the ifnormation
         // to disk just in case it exits

         // IMPORTANT - if user closes window the changes dont get saved

         // fill in details
         PCEscControl pControl;
         PCMMLNode pNode;
         DWORD dwNeeded;
         WCHAR szTemp[10000];
         pNode = gpData->NodeGet (gdwNode);
         if (!pNode)
            break;

         pControl = pPage->ControlFind(gszPhoneNum);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszPhoneNum, szTemp);
         NodeValueSet (pNode, gszName, szTemp[0] ? szTemp : gszPhoneCall); // so search has something to go by

         pControl = pPage->ControlFind (gszPerson);
         int   iCurSel;
         iCurSel = pControl ? pControl->AttribGetInt (gszCurSel) : -1;
         DWORD dwNode;
         dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
         NodeValueSet (pNode, gszPerson, (int) dwNode);
         PWSTR pszName;
         pszName = PeopleBusinessIndexToName ((DWORD)iCurSel);
         if (pszName)
            NodeValueSet (pNode, gszName, pszName);   // override the previous since a name is better

         // journal
         pControl = pPage->ControlFind (gszJournal);
         iCurSel = pControl ? pControl->AttribGetInt (gszCurSel) : -1;
         dwNode = JournalIndexToDatabase ((DWORD)iCurSel);
         NodeValueSet (pNode, gszJournal, (int) dwNode);

         pControl = pPage->ControlFind(gszSummary);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszSummary, szTemp);


         // other data
         NodeValueSet (pNode, gszDate, (int) DateControlGet (pPage, gszDate));
         NodeValueSet (pNode, gszStart, TimeControlGet (pPage, gszStart));
         NodeValueSet (pNode, gszEnd, TimeControlGet (pPage, gszEnd));


         // outgoing
         pControl = pPage->ControlFind (gszOutgoing);
         int   iOut;
         iOut = pControl ? pControl->AttribGetBOOL(gszChecked) : 1;
         NodeValueSet (pNode, gszOutgoing, iOut);

         // note in the cal log
         PhoneLogCall (gdwNode);


         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszCompleted) || !_wcsicmp(p->pControl->m_pszName, gszNoAnswer) ||
            !_wcsicmp(p->pControl->m_pszName, gszBusy) || !_wcsicmp(p->pControl->m_pszName, gszLeftMessage)) {
            // set the time
            TimeControlSet (pPage, gszEnd, Now());

            // append a different string to the notes depending upon the setting
            if (!_wcsicmp(p->pControl->m_pszName, gszNoAnswer)) {
               AppendString (pPage, L"\r\nNo one answered the phone.");
            }
            else if (!_wcsicmp(p->pControl->m_pszName, gszBusy)) {
               AppendString (pPage, L"\r\nThe phone line was busy (engaged).");
            }
            else if (!_wcsicmp(p->pControl->m_pszName, gszLeftMessage)) {
               AppendString (pPage, L"\r\nLeft a message asking the person to call me back.");

               // get the person's name
               PCEscControl pControl;
               PWSTR pszName;
               pControl = pPage->ControlFind (gszPerson);
               pszName = pControl ? PeopleBusinessIndexToName ((DWORD) pControl->AttribGetInt (gszCurSel)) : NULL;

               // add a reminder
               WCHAR szHuge[1024], szTemp[64];
               szHuge[0] = 0;
               wcscat (szHuge, L"Called ");
               if (pszName)
                  wcscat (szHuge, pszName);
               else {
                  // get the phone number
                  szTemp[0];
                  pControl = pPage->ControlFind(gszPhoneNum);
                  DWORD dwNeeded;
                  if (pControl)
                     pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);

                  wcscat (szHuge, szTemp[0] ? szTemp : L"[unknown]");
               }
               wcscat (szHuge, L" on ");
               DFDATEToString (Today(), szTemp);
               wcscat (szHuge, szTemp);
               wcscat (szHuge, L" at ");
               DFTIMEToString (Now(), szTemp);
               wcscat (szHuge, szTemp);
               wcscat (szHuge, L" and left a message. The person should have called back by now.");
               ReminderAdd (szHuge, MinutesToDFDATE(DFDATEToMinutes(Today()) + 60*24*2));
            }

            // get the peron's node and remove from to-call list
            if (!_wcsicmp(p->pControl->m_pszName, gszCompleted) || !_wcsicmp(p->pControl->m_pszName, gszLeftMessage)) {
               PCEscControl pControl;
               pControl = pPage->ControlFind (gszPerson);
               if (pControl)
                  PhoneClearToDo (PeopleBusinessIndexToDatabase ((DWORD) pControl->AttribGetInt (gszCurSel)));
            }

            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"v:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"actionself") || !_wcsicmp(p->pControl->m_pszName, L"actionother")) {
            // show UI
            PWSTR psz;
            PCEscControl pControl = pPage->ControlFind(gszPerson);

            psz = (!_wcsicmp(p->pControl->m_pszName, L"actionself")) ?
               WorkActionItemAdd (pPage) :
               ActionItemOther(pPage, PeopleBusinessIndexToDatabase ((DWORD)(pControl ? pControl->AttribGetInt(gszCurSel) : -1)) );

            AppendString (pPage, psz);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addmeeting")) {
            SchedMeetingShowAddUI (pPage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"schedulecall")) {
            PhoneCallShowAddUI (pPage);
            return TRUE;
         }


         break;
      }
   };

   return DefPage (pPage, dwMessage, pParam);
}


// BUGBUG - 2.0 - Phone quick keys that brings up the speed dial list with
// one touch?


/***********************************************************************
PhoneMakeCallPage - Page callback
*/
BOOL PhoneMakeCallPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_FILTEREDLISTCHANGE:
      {
         PESCNFILTEREDLISTCHANGE  p = (PESCNFILTEREDLISTCHANGE) pParam;

         if (p->iCurSel < 0)
            break;   // default

         // else handle
         DWORD dwNode;
         dwNode = PeopleBusinessIndexToDatabase ((DWORD)p->iCurSel);
         if (dwNode == (DWORD)-1)
            return TRUE;

         // get the phone number
         WCHAR szNum[64];
         if (!PhoneNumGet (pPage, dwNode, szNum, sizeof(szNum)))
            return TRUE;

         // call
         DWORD dwNew;
         dwNew = PhoneCall (pPage, TRUE, TRUE, dwNode, szNum);
         if (dwNew == (DWORD)-1)
            return TRUE;

         // new link
         WCHAR szTemp[16];
         swprintf (szTemp, L"e:%d", (int) dwNew);
         pPage->Exit (szTemp);
         return TRUE;
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
PhoneSpeedNewPage - Page callback
*/
BOOL PhoneSpeedNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // get the user
            DWORD dwNode, dwIndex;
            PWSTR pszName;
            PCEscControl pControl;
            pControl = pPage->ControlFind(gszPerson);
            dwIndex = pControl ? (DWORD)pControl->AttribGetInt(gszCurSel) : (DWORD)-1;
            dwNode = PeopleBusinessIndexToDatabase(dwIndex);
            pszName = PeopleBusinessIndexToName (dwIndex);

            if (!pszName || (dwNode == (DWORD)-1)) {
               pPage->MBWarning (L"You must select a person to add first.");
               return TRUE;
            }

            // do they have a number
            WCHAR szNum[64];
            if (!PhoneNumGet (pPage, dwNode, szNum, sizeof(szNum)))
               return TRUE;

            // add them
            PCMMLNode pNode;
            pNode = FindMajorSection (gszPhoneNode);
            if (!pNode)
               return TRUE;   // unexpected error
      
            // add subnode
            PCMMLNode pSub;
            pSub = pNode->ContentAddNewNode ();
            if (pSub) {
               pSub->NameSet (gszSpeedDial);
               NodeValueSet (pSub, gszPerson, (int)dwNode);
               NodeValueSet (pSub, gszName, pszName);
               NodeValueSet (pSub, gszPhoneNum, szNum);
            }

            gpData->NodeRelease(pNode);
            gpData->Flush();

            // added
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Speed dial added.");
            pPage->Exit (L"r:117"); // return to the phone
            return TRUE;
         }

         break;
      }

   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PhonePage - Page callback
*/
BOOL PhonePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"SPEEDDIAL")) {
            PCMMLNode pNode;
            pNode = FindMajorSection (gszPhoneNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            // see if can find a speed dial
            if ((DWORD)-1 == pNode->ContentFind (gszSpeedDial)) {
               // can't find, so leave blank
               p->pszSubString = (PWSTR)gMemTemp.p;
               gpData->NodeRelease(pNode);
               return TRUE;
            }

            // else, there are speed dials so write the info
            MemCat (&gMemTemp, L"<xbr/>");
            if (gfMicroHelp)
               MemCat (&gMemTemp, L"<xSectionTitle>Speed dial</xSectionTitle>");
            MemCat (&gMemTemp, L"<xtablecenter>");
            DWORD i;
            PCMMLNode   pSub;
            PWSTR psz;
            DWORD dwCellsInRow;
            dwCellsInRow = 0;
            for (i = 0; i < pNode->ContentNum(); i++) {
               pSub = NULL;
               pNode->ContentEnum(i, &psz, &pSub);
               if (!pSub)
                  continue;
               if (_wcsicmp(pSub->NameGet(), gszSpeedDial))
                  continue;

               // have speed dial

               // depending upon the number of cells in the row display text
               if (!dwCellsInRow)
                  MemCat (&gMemTemp, L"<tr>");

               // the text
               MemCat (&gMemTemp, L"<xSpeedDial href=\"call:");
               MemCat (&gMemTemp, NodeValueGetInt (pSub, gszPerson));
               MemCat (&gMemTemp, L":");
               PWSTR pszNum, pszName;
               pszNum = NodeValueGet (pSub, gszPhoneNum);
               MemCatSanitize (&gMemTemp, pszNum ? pszNum : L"");
               MemCat (&gMemTemp, L"\"><bold>");
               pszName = NodeValueGet (pSub, gszName);
               MemCatSanitize (&gMemTemp, pszName ? pszName : L"Unknown");
               MemCat (&gMemTemp, L"</bold><br/><small>");
               if (pszNum)
                  MemCatSanitize (&gMemTemp, pszNum);
               MemCat (&gMemTemp, L"</small></xSpeedDial>");


               if (dwCellsInRow)
                  MemCat (&gMemTemp, L"</tr>");
               dwCellsInRow = (dwCellsInRow+1)%2;
            }

            // if we're ending with a partially filled row then make that empty
            if (dwCellsInRow)
               MemCat (&gMemTemp, L"<td width=50%%/></tr>");

            MemCat (&gMemTemp, L"</xtablecenter>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         // only care about projectlist
         else if (!_wcsicmp(p->pszSubName, L"CALLS")) {
            p->pszSubString = PhoneSummary (TRUE, 0);
            return TRUE;
         }

      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         WCHAR szCall[] = L"call:";
         DWORD dwLenCall = wcslen(szCall);
         if (!_wcsnicmp(p->psz, szCall, dwLenCall)) {
            PWSTR psz = p->psz + dwLenCall;

            DWORD dwPerson = _wtoi (psz);

            // find next color
            psz = wcschr (psz, L':');
            if (!psz)
               return FALSE;
            psz++;

            DWORD dwNode;
            dwNode = PhoneCall (pPage, TRUE, TRUE, dwPerson, psz);
            if (dwNode == (DWORD)-1)
               return TRUE;   // error

            // exit this, editing the new node
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }

         BOOL fRefresh;
         DWORD dwNext;
         if (PhoneParseLink (p->psz, pPage, &fRefresh, &dwNext)) {
            // if user has specified to enter a Call then do so now
            if (dwNext) {
               WCHAR szTemp[16];
               swprintf (szTemp, L"e:%d", dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

         break;
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"takenotes")) {
            DWORD dwNode;
            dwNode = PhoneCall (pPage, FALSE, FALSE, (DWORD)-1, L"");
            if (dwNode == (DWORD)-1)
               return TRUE;   // error

            // exit this, editing the new node
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addCall")) {
            if (PhoneCallShowAddUI(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addreoccur")) {
            if (PhoneCallShowAddReoccurUI(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

         break;
      }
   };


   return DefPage (pPage, dwMessage, pParam);
}



/***********************************************************************
PhoneSpeedRemovePage - Page callback
*/
BOOL PhoneSpeedRemovePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"speedlist");
         if (!pControl)
            break;

         // set up the combo box
         PCMMLNode pNode;
         pNode = FindMajorSection (gszPhoneNode);
         if (!pNode)
            return FALSE;   // unexpected error

         DWORD i;
         PCMMLNode   pSub;
         PWSTR psz;
         for (i = 0; i < pNode->ContentNum(); i++) {
            pSub = NULL;
            pNode->ContentEnum(i, &psz, &pSub);
            if (!pSub)
               continue;
            if (_wcsicmp(pSub->NameGet(), gszSpeedDial))
               continue;

            // have speed dial
            WCHAR szTemp[512];
            PWSTR pszNum, pszName;
            pszNum = NodeValueGet (pSub, gszPhoneNum);
            pszName = NodeValueGet (pSub, gszName);

            wcscpy (szTemp, pszName ? pszName : L"Unknown");
            wcscat (szTemp, L" (");
            wcscat (szTemp, pszNum ? pszNum : L"Unknown");
            wcscat (szTemp, L")");

            // add it
            ESCMCOMBOBOXADD ca;
            memset (&ca, 0, sizeof(ca));
            ca.dwInsertBefore = (DWORD)-1;
            ca.pszText = szTemp;
            pControl->Message (ESCM_COMBOBOXADD, &ca);
         }

         gpData->NodeRelease(pNode);
         return TRUE;
      }
      break;   // go to default handler

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            // get the selection
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"speedlist");
            if (!pControl)
               break;
            int   iCurSel;
            iCurSel = pControl->AttribGetInt (gszCurSel);

            // find it
            PCMMLNode pNode;
            pNode = FindMajorSection (gszPhoneNode);
            if (!pNode)
               return FALSE;   // unexpected error

            DWORD i;
            PCMMLNode   pSub;
            PWSTR psz;
            BOOL fRemoved;
            fRemoved = FALSE;
            for (i = 0; i < pNode->ContentNum(); i++) {
               pSub = NULL;
               pNode->ContentEnum(i, &psz, &pSub);
               if (!pSub)
                  continue;
               if (_wcsicmp(pSub->NameGet(), gszSpeedDial))
                  continue;

               // if cursel == 0 then delete this and done
               if (!iCurSel) {
                  pNode->ContentRemove (i);
                  fRemoved = TRUE;
                  break;
               }

               // else try next
               iCurSel--;
            }

            gpData->NodeRelease(pNode);
            gpData->Flush();


            // removed
            if (fRemoved) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Speed dial removed.");
               pPage->Exit (L"r:117"); // return to the phone
            }
            else {
               pPage->MBWarning (L"Sorry. There aren't any speed dials to remove.");
            }
            return TRUE;
         }

         break;
      }

   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
GetCallLogNode - Given a date, this returns a month-specific node for the
call log.


inputs
   DFDATE         date - Date to look for
   BOOL           fCreateIfNotExit - if TRUE create the database node
                  if it doesn't exist. If FALSE, return NULL if it doesn't exist.
   DWORD          *pdwNode - Filled in with a node specific to the month/year.
returns
   PCMMLNode - Node (must be released) specific to the month/year. NULL if cant find/create.
*/
PCMMLNode GetCallLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode)
{
   HANGFUNCIN;
   PCMMLNode   pNew;

   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszPhoneNode);
   if (!pNode)
      return FALSE;   // unexpected error

   pNew = MonthYearTree (pNode, date, L"CallLog", fCreateIfNotExist, pdwNode);

   gpData->NodeRelease(pNode);

   return pNew;
}

/***********************************************************************
PhoneCallLogPage - Page callback
*/
BOOL PhoneCallLogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set the dropdown to the right date
         DateControlSet (pPage, gszDate, gdateLog);
      }
      break;   // go to default handler

   case ESCN_DATECHANGE:
      {
         // if the date changes then refresh the page
         PESCNDATECHANGE p = (PESCNDATECHANGE) pParam;

         if (_wcsicmp(p->pControl->m_pszName, gszDate))
            break;   // wrong one

         gdateLog = TODFDATE (1, p->iMonth, p->iYear);
         pPage->Exit (gszRedoSamePage);
      }
      return TRUE;

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
            pNode = GetCallLogNode (gdateLog, FALSE, &dwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszPhoneCall, FALSE);

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
               
               if ((pnlg->start != (DWORD)-1) && (pnlg->end != (DWORD)-1)) {
                  int   iDif;
                  iDif = DFTIMEToMinutes (pnlg->end) - DFTIMEToMinutes(pnlg->start);
                  if (iDif < 0)
                     iDif = 0;
                  MemCat (&gMemTemp, iDif);
                  MemCat (&gMemTemp, L" minutes");

               }

               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // if no entries then say so
            if (!pl || !pl->Num())
               MemCat (&gMemTemp, L"<tr><td>No call log entries for this month.</td></tr>");

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



// BUGBUG - 2.0 - abilit to record phone calls

/*************************************************************************
CPhoneCall::constructor and destructor
*/
CPhoneCall::CPhoneCall (void)
{
   HANGFUNCIN;
   m_pPhone = NULL;
   m_dwID = 0;
   m_dwParent = 0;
   m_memDescription.CharCat (0);
   memset (&m_reoccur, 0, sizeof(m_reoccur));

   m_dwDate = 0;
   m_dwStart = m_dwEnd = (DWORD)-1;
   m_dwAlarm = 0;
   m_dwAlarmDate = 0;
   m_dwAlarmTime = 0;
   m_dwAttend = (DWORD)-1;

}

CPhoneCall::~CPhoneCall (void)
{
   HANGFUNCIN;
   // intentionally left blank
}


/*************************************************************************
CPhoneCall::SetDirty - Sets the dirty flag in the main Phone
*/
void CPhoneCall::SetDirty (void)
{
   HANGFUNCIN;
   if (m_pPhone)
      m_pPhone->m_fDirty = TRUE;
}

/*************************************************************************
CPhoneCall::BringUpToDate - This is a reoccurring Call, it makes sure
that all occurances of the reoccurring Call have been generated up to
(and one beyond) today.

inputs
   DFDATE      today - today
returns
   none
*/
void CPhoneCall::BringUpToDate (DFDATE today)
{
   HANGFUNCIN;
   if (!m_reoccur.m_dwPeriod)
      return;  // nothing to do

   // else, see what the lastes date was
   if (m_reoccur.m_dwLastDate > today)
      return;  // already up to date

   // else, must add new Calls
   while (m_reoccur.m_dwLastDate <= today) {
      // find the next one
      DFDATE   next;
      next = ReoccurNext (&m_reoccur);
      if (!next)
         break;   // error


      // store this
      m_reoccur.m_dwLastDate = next;
      SetDirty();

      // if reoccurring event is beyond end date then don't bother
      if (m_reoccur.m_dwEndDate && (next > m_reoccur.m_dwEndDate))
         break;

      // else, add Call
      PCPhoneCall pNew;
      pNew = m_pPhone->CallAdd (NULL, (PWSTR)m_memDescription.p, (DWORD)-1);
      if (!pNew)
         break;   // error
      pNew->m_dwAttend = m_dwAttend;
      pNew->m_dwDate = next;
      pNew->m_dwEnd = m_dwEnd;
      pNew->m_dwStart = m_dwStart;
      pNew->m_dwParent = m_dwID;
      pNew->m_dwAlarm = m_dwAlarm;
      CalcAlarmTime (pNew->m_dwDate, pNew->m_dwStart, pNew->m_dwAlarm,
         &pNew->m_dwAlarmDate, &pNew->m_dwAlarmTime);
      // all copied over
   }

   // done
}

/*************************************************************************
CPhoneCall::Write - Writes the Call information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CPhoneCall::Write (PCMMLNode pParent)
{
   HANGFUNCIN;
   PCMMLNode   pSub;

   pSub = pParent->ContentAddNewNode ();
   if (!pSub)
      return FALSE;

   // set the name
   pSub->NameSet (gszCall);

   // write out parameters
   NodeValueSet (pSub, gszDescription, (PWSTR) m_memDescription.p);
   NodeValueSet (pSub, gszID, (int) m_dwID);
   NodeValueSet (pSub, gszParent, (int) m_dwParent);

   NodeValueSet (pSub, gszDate, (int) m_dwDate);
   NodeValueSet (pSub, gszEnd, (int) m_dwEnd);
   NodeValueSet (pSub, gszStart, (int) m_dwStart);
   NodeValueSet (pSub, gszAttend, (int) m_dwAttend);
   NodeValueSet (pSub, gszAlarm, (int) m_dwAlarm);
   NodeValueSet (pSub, gszAlarmDate, (int) m_dwAlarmDate);
   NodeValueSet (pSub, gszAlarmTime, (int) m_dwAlarmTime);

   // write out reoccurance
   NodeValueSet (pSub, gszReoccurPeriod, m_reoccur.m_dwPeriod);
   NodeValueSet (pSub, gszReoccurLastDate, m_reoccur.m_dwLastDate);
   NodeValueSet (pSub, gszReoccurEndDate, m_reoccur.m_dwEndDate);
   NodeValueSet (pSub, gszReoccurParam1, m_reoccur.m_dwParam1);
   NodeValueSet (pSub, gszReoccurParam2, m_reoccur.m_dwParam2);
   NodeValueSet (pSub, gszReoccurParam3, m_reoccur.m_dwParam3);

   m_PlanTask.Write(pSub);
   return TRUE;
}


/*************************************************************************
CPhoneCall::Read - Writes the Call information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the Call data
reutrns
   BOOL - TRUE if successful
*/
BOOL CPhoneCall::Read (PCMMLNode pSub)
{
   HANGFUNCIN;
   PWSTR psz;

   psz = NodeValueGet (pSub, gszDescription);
   MemZero (&m_memDescription);
   if (psz)
      MemCat (&m_memDescription, psz);

   m_dwID = (DWORD) NodeValueGetInt (pSub, gszID, 1);

   m_dwDate = (DWORD) NodeValueGetInt (pSub, gszDate, 0);
   m_dwEnd = (DWORD) NodeValueGetInt (pSub, gszEnd, -1);
   m_dwStart = (DWORD) NodeValueGetInt (pSub, gszStart, -1);
   m_dwParent = (DWORD) NodeValueGetInt (pSub, gszParent, 0);
   m_dwAttend = (DWORD) NodeValueGetInt (pSub, gszAttend, -1);
   m_dwAlarm = (DWORD) NodeValueGetInt (pSub, gszAlarm, 0);
   m_dwAlarmDate = (DWORD) NodeValueGetInt (pSub, gszAlarmDate, 0);
   m_dwAlarmTime = (DWORD) NodeValueGetInt (pSub, gszAlarmTime, 0);

   m_reoccur.m_dwPeriod = (DWORD) NodeValueGetInt (pSub, gszReoccurPeriod, 0);
   m_reoccur.m_dwLastDate = (DWORD) NodeValueGetInt (pSub, gszReoccurLastDate, 0);
   m_reoccur.m_dwEndDate = (DWORD) NodeValueGetInt (pSub, gszReoccurEndDate, 0);
   m_reoccur.m_dwParam1 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam1, 0);
   m_reoccur.m_dwParam2 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam2, 0);
   m_reoccur.m_dwParam3 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam3, 0);

   m_PlanTask.Read(pSub);

   return TRUE;
}

/*************************************************************************
CPhone::Constructor and destructor - Initialize
*/
CPhone::CPhone (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwNextCallID = 1;

   m_listCalls.Init (sizeof(PCPhoneCall));
}

CPhone::~CPhone (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new Phone is cleared
   Init();
}


/************************************************************************
CPhone::CallAdd - Adds a mostly default Call.

inputs
   PWSTR    pszName - name
   PWSTR    pszDescription - description
   DWORD    dwBefore - Call index to insert before. -1 for after
returns
   PCPhoneCall - To the Call object, to be added further
*/
PCPhoneCall CPhone::CallAdd (PWSTR pszName, PWSTR pszDescription,
                                 DWORD dwBefore)
{
   HANGFUNCIN;
   PCPhoneCall pCall;
   pCall = new CPhoneCall;
   if (!pCall)
      return NULL;

   pCall->m_dwID = m_dwNextCallID++;
   m_fDirty = TRUE;
   pCall->m_pPhone = this;

   MemZero (&pCall->m_memDescription);
   MemCat (&pCall->m_memDescription, pszDescription);
  
   // add this to the list
   if (dwBefore < m_listCalls.Num())
      m_listCalls.Insert(dwBefore, &pCall);
   else
      m_listCalls.Add (&pCall);

   return pCall;
}

/*************************************************************************
CallGetByIndex - Given an index, this returns a Call, or NULL if can't find.
*/
PCPhoneCall CPhone::CallGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCPhoneCall pCall, *ppCall;

   ppCall = (PCPhoneCall*) m_listCalls.Get(dwIndex);
   if (!ppCall)
      return NULL;
   pCall = *ppCall;
   return pCall;
}

/*************************************************************************
CallGetByID - Given a Call ID, this returns a Call, or NULL if can't find.
*/
PCPhoneCall CPhone::CallGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCPhoneCall pCall;
   DWORD i;
   for (i = 0; i < m_listCalls.Num(); i++) {
      pCall = CallGetByIndex (i);
      if (!pCall)
         continue;

      if (pCall->m_dwID == dwID)
         return pCall;
   }

   // else, cant find
   return NULL;
}

/*************************************************************************
CallIndexByID - Given a Call ID, this returns a an index, or -1 if can't find.
*/
DWORD CPhone::CallIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCPhoneCall pCall;
   DWORD i;
   if (!dwID)
      return (DWORD) -1;

   for (i = 0; i < m_listCalls.Num(); i++) {
      pCall = CallGetByIndex (i);
      if (!pCall)
         continue;

      if (pCall->m_dwID == dwID)
         return i;
   }

   // else, cant find
   return (DWORD)-1;
}

/*************************************************************************
CPhone::Write - Writes all the Phone data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
Phone object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CPhone::Write (void)
{
   HANGFUNCIN;
   // make sure have database name
   if (!m_dwDatabaseNode)
      return FALSE;

   // get the node
   PCMMLNode   pNode;
   pNode = gpData->NodeGet (m_dwDatabaseNode);
   if (!pNode)
      return FALSE;  // cant find node

   if (!NodeValueSet (pNode, gszNextCallID, m_dwNextCallID))
      return FALSE;


   // BUGBUG - 2.0 - at some point write out last modified, for search purposes

   // remove existing Calls
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   for (i = dwNum - 1; i < dwNum; i--) {
      PCMMLNode   pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszCall))
         continue;

      // else, old Call. delete
      pNode->ContentRemove (i);
   }

   // write out other info, such as Calls, completed and not
   PCPhoneCall pCall;
   for (i = 0; i < m_listCalls.Num(); i++) {
      pCall = *((PCPhoneCall*) m_listCalls.Get(i));
      pCall->Write (pNode);
   }

   // remember to release the node before done
   gpData->NodeRelease (pNode);

   // flush the database
   if (!gpData->Flush())
      return FALSE;

   m_fDirty = FALSE;

   return TRUE;
}

/*************************************************************************
CPhone::Flush - If m_fDirty is set it writes out the Phone and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CPhone::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CPhone::Init(void) - Initializes a blank Phone. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CPhone::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCPhoneCall pCall;
   for (i = 0; i < m_listCalls.Num(); i++) {
      pCall = *((PCPhoneCall*) m_listCalls.Get(i));
      delete pCall;
   }
   m_listCalls.Clear();

   // new values for name, description, and days per week
   m_dwNextCallID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   // bring reoccurring Calls up to date
   BringUpToDate (Today());

   // sort
   Sort ();

   return TRUE;
}

/*************************************************************************
CPhone::Init(dwDatabaseNode) - Initializes by reading the Phone in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a Phone, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CPhone::Init (DWORD dwDatabaseNode)
{
   HANGFUNCIN;
   // wipe out whatever have now
   if (!Init())
      return FALSE;

   // load in the database node
   PCMMLNode   pNode;
   pNode = gpData->NodeGet(dwDatabaseNode);
   if (!pNode)
      return FALSE;
   if (_wcsicmp(pNode->NameGet(), gszPhoneNode)) {
      // invalid name
      gpData->NodeRelease(pNode);
      return FALSE;
   }
   m_dwDatabaseNode = dwDatabaseNode;


   // read in the name, description, and daysperweek
   m_dwNextCallID = NodeValueGetInt (pNode, gszNextCallID, 1);

   // read in other Calls
   DWORD i, dwNum;
   PCMMLNode pSub;
   PWSTR psz;
   dwNum = pNode->ContentNum();
   for (i = 0; i < dwNum; i++) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      BOOL  fCompleted;
      if (!_wcsicmp(pSub->NameGet(), gszCall))
         fCompleted = FALSE;
      else
         continue;   // not looking for this

      PCPhoneCall pCall;
      pCall = new CPhoneCall;
      pCall->Read (pSub);
      pCall->m_pPhone = this;


      // add to list
      m_listCalls.Add (&pCall);
   }


   // bring reoccurring Calls up to date
   BringUpToDate (Today());
   // sort
   Sort ();

   
   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/*******************************************************************************
Sort - Sorts the Phone list by date.
*/
int __cdecl PhoneSort(const void *elem1, const void *elem2 )
{
   PCPhoneCall t1, t2;
   t1 = *((PCPhoneCall*)elem1);
   t2 = *((PCPhoneCall*)elem2);

   // non-automatic have priority
   if (t1->m_reoccur.m_dwPeriod && !t2->m_reoccur.m_dwPeriod)
      return 1;
   else if (!t1->m_reoccur.m_dwPeriod && t2->m_reoccur.m_dwPeriod)
      return -1;

   // by date appear
   DFDATE d1, d2;
   d1 = t1->m_dwDate;
   d2 = t2->m_dwDate;
   if (!d1)
      d1 = gToday;
   if (!d2)
      d2 = gToday;
   if (d1 != d2)
      return (int) d1 - (int)d2;

   // else by time
   // BUGFIX - Was randomly sorting if started at same time
   if (t1->m_dwStart != t2->m_dwStart)
      return (int) t1->m_dwStart - (int)t2->m_dwStart;

   return (int) t1->m_dwID - t2->m_dwID;
}

void CPhone::Sort (void)
{
   gToday = Today();

   qsort (m_listCalls.Get(0), m_listCalls.Num(), sizeof(PCPhoneCall), PhoneSort);
}


/************************************************************************
BringUpToDate - Brings all the reoccurring Calls up to date, today.
*/
void CPhone::BringUpToDate (DFDATE today)
{
   HANGFUNCIN;
   DWORD dwNum = m_listCalls.Num();
   DWORD i;
   PCPhoneCall pCall;
   for (i = 0; i < dwNum; i++) {
      pCall = CallGetByIndex (i);
      pCall->BringUpToDate (today);
   }

   // BUGFIX - Was crashing
   Flush();
}

/*************************************************************************
DeleteAllChildren - Given a reoccurring Call ID, this deletes all Call
instances created by the reoccurring Call.

inputs
   DWORD    dwID - ID
*/
void CPhone::DeleteAllChildren (DWORD dwID)
{
   HANGFUNCIN;
   DWORD dwNum = m_listCalls.Num();
   DWORD i;
   PCPhoneCall pCall;
   for (i = dwNum - 1; i < dwNum; i--) {
      pCall = CallGetByIndex (i);
      if (!pCall || (pCall->m_dwParent != dwID))
         continue;

      // delete
      m_listCalls.Remove (i);
      delete pCall;
   }

   m_fDirty = TRUE;
   Flush();
}

/*****************************************************************************
PhoneCallAddPage - Override page callback.
*/
BOOL PhoneCallAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // BUGFIX - If have date that want to use for add then set
         if (pPage->m_pUserData)
            DateControlSet (pPage, gszCallDate, (DFDATE)pPage->m_pUserData);
      }
      return TRUE;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[256];
            WCHAR szDescription[10024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // get the person
            pControl = pPage->ControlFind (gszAttend);
            // get the values
            int   iCurSel;
            iCurSel = pControl ? pControl->AttribGetInt (gszCurSel) : -1;
            // find the node number for the person's data and the name
            DWORD dwNode;
            dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
            if (dwNode == (DWORD)-1) {
               pPage->MBWarning (L"You must specify who you're going to call.");
               return TRUE;
            }

            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);

            // make sure a date is set for the Call
            DFDATE d;
            d = DateControlGet (pPage, gszCallDate);
            if (d && (d < Today())) {
               pPage->MBWarning (L"You must enter a date for the call sometime on or after today.");
               return TRUE;
            }

            // see if there's a conflict
            if (LookForConflict(DateControlGet(pPage,gszCallDate),
               TimeControlGet(pPage,gszCallStart), TimeControlGet(pPage,gszCallEnd),
               (DWORD)-1), FALSE) {
                  if (IDYES != pPage->MBYesNo (L"The call conflicts with another meeting or call. Do you want to schedule it anyway?"))
                     return TRUE;
            }


            // add it
            PCPhoneCall pCall;
            pCall = gCurPhone.CallAdd (szName, szDescription, (DWORD)-1);
            if (!pCall) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // other data
            pCall->m_dwDate = DateControlGet (pPage, gszCallDate);
            pCall->m_dwStart = TimeControlGet (pPage, gszCallStart);
            pCall->m_dwEnd = TimeControlGet (pPage, gszCallEnd);

            pCall->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);
            if (pCall->m_dwAlarm && (!pCall->m_dwDate || (pCall->m_dwStart == (DWORD)-1))) {
               pPage->MBWarning (L"You must specify a date and time for an alarm to work.",
                  L"Dragonfly will ignore the alarm setting. Edit this scheduled call "
                  L"to set the alarm.");
               pCall->m_dwAlarm = 0;
            }

            CalcAlarmTime (pCall->m_dwDate, pCall->m_dwStart, pCall->m_dwAlarm,
               &pCall->m_dwAlarmDate, &pCall->m_dwAlarmTime);

            // fix some confusion. Make sure cant have end time without start
            if ((pCall->m_dwStart == (DWORD)-1) && (pCall->m_dwEnd != (DWORD)-1)) {
               pCall->m_dwStart = pCall->m_dwEnd;
               pCall->m_dwEnd = (DWORD)-1;
            }

            pCall->m_dwAttend = dwNode;


            // exit this
            gCurPhone.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
PhoneSummary - Fill in the substitution for the Call list.

inputs
   BOOL     fAll - If TRUE show all, FALSE show only active Calls
   DFDATE   date - date to show up until
retursn
   PWSTR - string from gMemTemp
*/
PWSTR PhoneSummary (BOOL fAll, DFDATE date)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Calls to show
   PhoneInit();
   // bring reoccurring Calls up to date. 1 day in advance so can use PhoneSummary for tomrrow page
   gCurPhone.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24));
   gCurPhone.Sort();  // make sure they're sorted
   DWORD i;
   PCPhoneCall pCall;
   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      pCall = gCurPhone.CallGetByIndex(i);

      // if !fAll different
      if (!fAll) {
         if (pCall->m_reoccur.m_dwPeriod || (pCall->m_dwDate > date))
            continue;
      }

      break;
   }
   if (i >= gCurPhone.m_listCalls.Num())
      return L""; // nothing

   // else, we have Calls. Show them off
   MemZero (&gMemTemp);
   if (fAll) {
      MemCat (&gMemTemp, L"<xbr/>");
      MemCat (&gMemTemp, fAll ?
         L"<xSectionTitle>Scheduled calls</xSectionTitle>" :
         L"<xSectionTitle>Calls</xSectionTitle>");
      // MemCat (&gMemTemp, L"<p>This shows the list of project Calls you're Phoneuled to Phone on this week.</p>");
   }
   MemCat (&gMemTemp, L"<xlisttable innerlines=0>");
   if (!fAll)
      MemCat (&gMemTemp, L"<xtrheader><a href=r:113 color=#c0c0ff>Calls</a></xtrheader>");

   DWORD dwLastDate;
   dwLastDate = (DWORD)-1;
   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      pCall = gCurPhone.CallGetByIndex(i);

      // filter out Call if !fAll
      if (!fAll) {
         if (pCall->m_reoccur.m_dwPeriod || (pCall->m_dwDate > date))
            continue;
      }

      // if different priority then show
      DWORD dwPhantomDate;
      dwPhantomDate = pCall->m_dwDate;
      if (!dwPhantomDate && !pCall->m_reoccur.m_dwPeriod)
         dwPhantomDate = Today();
      if (dwPhantomDate != dwLastDate) {
         if (dwLastDate != (DWORD)-1)
            MemCat (&gMemTemp, L"</xtrmonth>");
         dwLastDate = dwPhantomDate;

         MemCat (&gMemTemp, L"<xtrmonth><xMeetingdate>");
         if (dwLastDate) {
            WCHAR szTemp[64];
            DFDATEToString (dwLastDate, szTemp);
            MemCat (&gMemTemp, szTemp);
         }
         else
            MemCat (&gMemTemp, L"Reoccurring");
         MemCat (&gMemTemp, L"</xMeetingdate>");
      }

      // display Call info
      MemCat (&gMemTemp, L"<tr><xtdtask href=sp:");
      MemCat (&gMemTemp, (int) pCall->m_dwID);
      MemCat (&gMemTemp, L">");
      PWSTR psz;
      if (pCall->m_dwAttend == (DWORD)-1)
         psz = L"Unknown";
      else
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pCall->m_dwAttend));
      if (!psz)
         continue;

      MemCatSanitize (&gMemTemp, psz);
      
      WCHAR szTemp[128];
      // hover help
      MemCat (&gMemTemp, L"<xHoverHelp>");
      if (((PWSTR) pCall->m_memDescription.p)[0]) {
         MemCatSanitize (&gMemTemp, (PWSTR) pCall->m_memDescription.p);
      }

      MemCat (&gMemTemp, L"</xHoverHelp>");

      // completed
      MemCat (&gMemTemp, L"</xtdtask>");

      MemCat (&gMemTemp, L"<xtdcompleted></xtdcompleted>"); // filler to space with the rest
      // last category
      MemCat (&gMemTemp, L"<xtdcompleted>");
      if ((pCall->m_dwStart != (DWORD) -1) && (pCall->m_dwEnd != (DWORD)-1)) {
         DFTIMEToString (pCall->m_dwStart, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L" to ");
         DFTIMEToString (pCall->m_dwEnd, szTemp);
         MemCat (&gMemTemp, szTemp);
      }
      else if (pCall->m_dwStart != (DWORD)-1) {
         DFTIMEToString (pCall->m_dwStart, szTemp);
         MemCat (&gMemTemp, szTemp);
      }
      else if (pCall->m_dwEnd != (DWORD)-1) {
         DFTIMEToString (pCall->m_dwEnd, szTemp);
         MemCat (&gMemTemp, szTemp);
      }
      else {   // both -1
         MemCat (&gMemTemp, L"Any time");
      }


      if (pCall->m_reoccur.m_dwPeriod) {
         MemCat (&gMemTemp, L"<br/><italic>");
         ReoccurToString (&pCall->m_reoccur, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"</italic>");
      }
      MemCat (&gMemTemp, L"</xtdcompleted></tr>");
   }

   // finish off list
   if (dwLastDate != (DWORD)-1)
      MemCat (&gMemTemp, L"</xtrmonth>");

   MemCat (&gMemTemp, L"</xlisttable>");
   return (PWSTR) gMemTemp.p;
}

/*****************************************************************************
PhoneCallShowAddUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   DFDATE         dAddDate - Date to use. 0 => don't set
returns
   BOOL - TRUE if a project Call was added
*/
BOOL PhoneCallShowAddUI (PCEscPage pPage, DFDATE dAddDate)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   PhoneInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPHONESCHEDADD, PhoneCallAddPage, (PVOID) dAddDate);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}

/*****************************************************************************
PhoneCallAddReoccurPage - Override page callback.
*/
BOOL PhoneCallAddReoccurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[256];
            WCHAR szDescription[10024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // get the person
            pControl = pPage->ControlFind (gszAttend);
            // get the values
            int   iCurSel;
            iCurSel = pControl ? pControl->AttribGetInt (gszCurSel) : -1;
            // find the node number for the person's data and the name
            DWORD dwNode;
            dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
            if (dwNode == (DWORD)-1) {
               pPage->MBWarning (L"You must specify who you're going to call.");
               return TRUE;
            }

            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);


            // add it
            PCPhoneCall pCall;
            pCall = gCurPhone.CallAdd (szName, szDescription, (DWORD)-1);
            if (!pCall) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // other data
            pCall->m_dwDate = 0;
            pCall->m_dwStart = TimeControlGet (pPage, gszCallStart);
            pCall->m_dwEnd = TimeControlGet (pPage, gszCallEnd);
            pCall->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);

            if (pCall->m_dwAlarm && (pCall->m_dwStart == (DWORD)-1)) {
               pPage->MBWarning (L"You must specify a time for an alarm to work.",
                  L"Dragonfly will ignore the alarm setting. Edit this scheduled call "
                  L"to set the alarm.");
               pCall->m_dwAlarm = 0;
            }

            // fix some confusion. Make sure cant have end time without start
            if ((pCall->m_dwStart == (DWORD)-1) && (pCall->m_dwEnd != (DWORD)-1)) {
               pCall->m_dwStart = pCall->m_dwEnd;
               pCall->m_dwEnd = (DWORD)-1;
            }

            // store attend node
            pCall->m_dwAttend = dwNode;


            // fill in reoccur
            ReoccurFromControls (pPage, &pCall->m_reoccur);

            // bring it up to date
            pCall->BringUpToDate(Today());

            // exit this
            gCurPhone.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
PhoneCallShowAddReoccurUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project Call was added
*/
BOOL PhoneCallShowAddReoccurUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   PhoneInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPHONESCHEDADDREOCCUR, PhoneCallAddReoccurPage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}


/*****************************************************************************
PhoneCallEditPage - Override page callback.
*/
BOOL PhoneCallEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCPhoneCall pCall = (PCPhoneCall) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"IFREOCCUR")) {
            p->pszSubString = pCall->m_dwParent ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFREOCCUR")) {
            p->pszSubString = pCall->m_dwParent ? L"" : L"</comment>";
            return TRUE;
         }
      }
      break;   // default

   case ESCM_INITPAGE:
      {
         // set up values
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pCall->m_memDescription.p);


         // other data
         DateControlSet (pPage, gszCallDate, pCall->m_dwDate);
         TimeControlSet (pPage, gszCallStart, pCall->m_dwStart);
         TimeControlSet (pPage, gszCallEnd, pCall->m_dwEnd);
         AlarmControlSet (pPage, gszAlarm, pCall->m_dwAlarm);

         if (pCall->m_dwAttend != (DWORD)-1) {
            DWORD dw;
            dw = PeopleBusinessDatabaseToIndex(pCall->m_dwAttend);

            pControl = pPage->ControlFind (gszAttend);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) dw);
         }


      }
      break;   // do default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {

            // pressed add. Do some verification
            WCHAR szName[128];
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // get the person
            pControl = pPage->ControlFind (gszAttend);
            // get the values
            int   iCurSel;
            iCurSel = pControl ? pControl->AttribGetInt (gszCurSel) : -1;
            // find the node number for the person's data and the name
            DWORD dwNode;
            dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
            if (dwNode == (DWORD)-1) {
               pPage->MBWarning (L"You must specify who you're going to call.");
               return TRUE;
            }

            // see if there's a conflict (only if not editing now)
            if (LookForConflict(DateControlGet(pPage,gszCallDate),
               TimeControlGet(pPage,gszCallStart), TimeControlGet(pPage,gszCallEnd),
               pCall->m_dwID, FALSE)) {
                  if (IDYES != pPage->MBYesNo (L"The call conflicts with another meeting or call. Do you want to schedule it anyway?"))
                     return TRUE;
            }

            // name
            // set values
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pCall->m_memDescription);
            MemCat (&pCall->m_memDescription, szDescription);

            // don't reset alarm unless chage the date, time, etc
            DFDATE   dwOldDate;
            DFTIME   dwOldTime;
            DWORD    dwOldAlarm;
            dwOldDate = pCall->m_dwDate;
            dwOldTime = pCall->m_dwStart;
            dwOldAlarm = pCall->m_dwAlarm;

            // other data
            DFDATE date;
            date = DateControlGet (pPage, gszCallDate);
            if (date != pCall->m_dwDate) {
               if (pCall->m_PlanTask.Num()) {
                  PPLANTASKITEM pp = (PPLANTASKITEM) pCall->m_PlanTask.Get(0);
                  if (date)
                     pp->date = date;
               }
               pCall->m_dwDate = date;
            }
            pCall->m_dwStart = TimeControlGet (pPage, gszCallStart);
            pCall->m_dwEnd = TimeControlGet (pPage, gszCallEnd);

            pCall->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);

            if (pCall->m_dwAlarm && (!pCall->m_dwDate || (pCall->m_dwStart == (DWORD)-1))) {
               pPage->MBWarning (L"You must specify a date and time for an alarm to work.",
                  L"Dragonfly will ignore the alarm setting. Edit this scheduled call "
                  L"to set the alarm.");
               pCall->m_dwAlarm = 0;
            }

            if ((dwOldDate != pCall->m_dwDate) || (dwOldTime != pCall->m_dwStart) || (dwOldAlarm != pCall->m_dwAlarm))
               CalcAlarmTime (pCall->m_dwDate, pCall->m_dwStart, pCall->m_dwAlarm,
                  &pCall->m_dwAlarmDate, &pCall->m_dwAlarmTime);

            // fix some confusion. Make sure cant have end time without start
            if ((pCall->m_dwStart == (DWORD)-1) && (pCall->m_dwEnd != (DWORD)-1)) {
               pCall->m_dwStart = pCall->m_dwEnd;
               pCall->m_dwEnd = (DWORD)-1;
            }

            // store away attend
            pCall->m_dwAttend = dwNode;



            // set dirty
            pCall->SetDirty();
            gCurPhone.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {

            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Scheduled call removed.");

            // delete it
            DWORD dwIndex;
            dwIndex = gCurPhone.CallIndexByID (pCall->m_dwID);
            gCurPhone.m_listCalls.Remove(dwIndex);
            delete pCall;
            gCurPhone.m_fDirty = TRUE;
            gCurPhone.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
PhoneCallEditReoccurPage - Override page callback.
*/
BOOL PhoneCallEditReoccurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCPhoneCall pCall = (PCPhoneCall) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set up values
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pCall->m_memDescription.p);


         // other data
         DateControlSet (pPage, gszCallDate, pCall->m_dwDate);
         TimeControlSet (pPage, gszCallStart, pCall->m_dwStart);
         TimeControlSet (pPage, gszCallEnd, pCall->m_dwEnd);
         AlarmControlSet (pPage, gszAlarm, pCall->m_dwAlarm);

         if (pCall->m_dwAttend != (DWORD)-1) {
            DWORD dw;
            dw = PeopleBusinessDatabaseToIndex(pCall->m_dwAttend);

            pControl = pPage->ControlFind (gszAttend);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) dw);
         }


         // fill in reoccur
         ReoccurToControls (pPage, &pCall->m_reoccur);

      }
      break;   // do default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[128];
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // get the person
            pControl = pPage->ControlFind (gszAttend);
            // get the values
            int   iCurSel;
            iCurSel = pControl ? pControl->AttribGetInt (gszCurSel) : -1;
            // find the node number for the person's data and the name
            DWORD dwNode;
            dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
            if (dwNode == (DWORD)-1) {
               pPage->MBWarning (L"You must specify who you're going to call.");
               return TRUE;
            }

            // set values
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pCall->m_memDescription);
            MemCat (&pCall->m_memDescription, szDescription);

            // other data
            pCall->m_dwDate = 0;
            pCall->m_dwStart = TimeControlGet (pPage, gszCallStart);
            pCall->m_dwEnd = TimeControlGet (pPage, gszCallEnd);
            pCall->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);
            if (pCall->m_dwAlarm && (pCall->m_dwStart == (DWORD)-1)) {
               pPage->MBWarning (L"You must specify a time for an alarm to work.",
                  L"Dragonfly will ignore the alarm setting. Edit this scheduled call "
                  L"to set the alarm.");
               pCall->m_dwAlarm = 0;
            }

            // fix some confusion. Make sure cant have end time without start
            if ((pCall->m_dwStart == (DWORD)-1) && (pCall->m_dwEnd != (DWORD)-1)) {
               pCall->m_dwStart = pCall->m_dwEnd;
               pCall->m_dwEnd = (DWORD)-1;
            }
            // store away call
            pCall->m_dwAttend = dwNode;


            // BUGFIX - loop throught all existing instances that are children and modify
            DWORD i;
            for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
               PCPhoneCall pChild = gCurPhone.CallGetByIndex(i);
               if (!pChild)
                  continue;
               if (pChild->m_dwParent != pCall->m_dwID)
                  continue;

               // found, so change
               MemZero (&pChild->m_memDescription);
               MemCat (&pChild->m_memDescription, (PWSTR) pCall->m_memDescription.p);
               pChild->m_dwStart = pCall->m_dwStart;
               pChild->m_dwEnd = pCall->m_dwEnd;
               pChild->m_dwAttend = pCall->m_dwAttend;

               pChild->m_dwAlarm = pCall->m_dwAlarm;
               CalcAlarmTime (pChild->m_dwDate, pChild->m_dwStart, pChild->m_dwAlarm,
                  &pChild->m_dwAlarmDate, &pChild->m_dwAlarmTime);

               pChild->SetDirty();

            }

            // fill in reoccur
            REOCCURANCE rOld;
            rOld = pCall->m_reoccur;

            ReoccurFromControls (pPage, &pCall->m_reoccur);

            // BUGFIX - if this has changed ask the user if they want to reschedule
            // all existing occurances
            if (memcmp(&rOld, &pCall->m_reoccur, sizeof(rOld))) {
               pPage->MBSpeakInformation (L"Any existing scheduled call instances have been rescheduled.",
                  L"Because you have changed how often the call occurs any calls already "
                  L"generated by this reoccurring call have been rescheduled.");
               gCurPhone.DeleteAllChildren (pCall->m_dwID);
               pCall->m_reoccur.m_dwLastDate = 0;
            }

            // bring it up to date
            pCall->BringUpToDate(Today());

            // set dirty
            pCall->SetDirty();
            gCurPhone.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            int iRet;
            iRet = pPage->MBYesNo (L"Do you want to delete all instances of the call?",
               L"Because this is a reoccurring call it may have generated instances of the "
               L"call occurring on specific days. If you press 'Yes' you will delete them too. "
               L"'No' will leave them on your call list.", TRUE);
            if (iRet == IDCANCEL)
               return TRUE;   // exit

            // if delete children do so
            if (iRet == IDYES)
               gCurPhone.DeleteAllChildren (pCall->m_dwID);

            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Reoccurring call deleted.");

            // delete it
            DWORD dwIndex;
            dwIndex = gCurPhone.CallIndexByID (pCall->m_dwID);
            gCurPhone.m_listCalls.Remove(dwIndex);
            delete pCall;
            gCurPhone.m_fDirty = TRUE;
            gCurPhone.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
PhoneParseLink - If a page uses a Phone substitution, which generates
"sc:XXXX" to indicate that a Phone link was called, call this. If it's
not a Phone link, returns FALSE. If it is, it deletes the Phone and
returns TRUE. The page should then refresh itself.

inputs
   PWSTR    pszLink - link
   PCEscPage pPage - page
   BOOL     *pfRefresh - set to TRUE if should refresh
   DWORD    *pdwNext - Norammly filled with 0. If this is NOT then
               the next page should be "e:XXXX" where XXX is *pdwNext
returns
   BOOL
*/
BOOL PhoneParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, DWORD *pdwNext)
{
   HANGFUNCIN;
   *pdwNext = 0;
   *pfRefresh = FALSE;

   // make sure its the right type
   if ((pszLink[0] != L's') || (pszLink[1] != L'p') || (pszLink[2] != L':'))
      return FALSE;

   // make sure have Phone loaded
   PhoneInit ();

   // get the Call
   PCPhoneCall pCall;
   DWORD dwID;
   dwID = 0;
   AttribToDecimal (pszLink + 3, (int*) &dwID);
   pCall = gCurPhone.CallGetByID (dwID);
   if (!pCall)
      return FALSE;

   // if the call is for today and it's not reoccurring then ask if want to call
   if (!pCall->m_reoccur.m_dwPeriod && (pCall->m_dwAttend != (DWORD)-1) && (!pCall->m_dwDate || (pCall->m_dwDate == Today())) ) {
      // ask if want to call

      if (IDYES == pPage->MBYesNo (L"Do you want to call this person now?", L"If you press \"No\" you'll be able to edit the entry.")) {
         // get the phone number
         WCHAR szNum[64];
         if (!PhoneNumGet (pPage, pCall->m_dwAttend, szNum, sizeof(szNum)))
            return TRUE;

         // call
         DWORD dwNew;
         dwNew = PhoneCall (pPage, TRUE, TRUE, pCall->m_dwAttend, szNum);
         if (dwNew == (DWORD)-1)
            return TRUE;

         *pdwNext = dwNew;
         return TRUE;
         // else, called
      }

   }

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, 
      pCall->m_reoccur.m_dwPeriod ? IDR_MMLPHONESCHEDEDITREOCCUR : IDR_MMLPHONESCHEDEDIT,
      pCall->m_reoccur.m_dwPeriod ? PhoneCallEditReoccurPage : PhoneCallEditPage,
      pCall);


   // remember if should refresh
   *pfRefresh = (pszRet && !_wcsicmp(pszRet, gszOK));

   return TRUE;
}


/*************************************************************************
PhoneMonthEnumerate - Enumerates a months worth of data.

inputs
   DFDATE   date - Use only the month and year field
   PCMem    papMem - Pointer to an arry of 31 PCMem, initially set to NULL.
               If any events occur on that day then the function will create
               a new CMem (which must be freed by the caller), and
               fill it with a MML (null-terminated) text string. The
               MML has <li>TEXT</li> so that these can be put into a list.
   BOOL     fWithLinks - If TRUE, then not only include the text, but include links
               in the text so can jump to the project. BUGFIX - Added to make view
               calendar as list better
returns
   none
*/
void PhoneMonthEnumerate (DFDATE date, PCMem *papMem, BOOL fWithLinks)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Calls to show
   PhoneInit();
   // bring reoccurring Calls up to date -
   // bring up a whole month ahead of time so guaranteed to at least
   // see on month's worth
   gCurPhone.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24*30));
   gCurPhone.Sort();  // make sure they're sorted
   DWORD i;
   PCPhoneCall pCall;

   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      pCall = gCurPhone.CallGetByIndex(i);

      // filter out Call if reoccurring
      if (pCall->m_reoccur.m_dwPeriod)
         continue;

      // filter out non-month
      DFDATE d;
      d = pCall->m_dwDate;
      if (!d)
         d = today;
      if ( (MONTHFROMDFDATE(d) != MONTHFROMDFDATE(date)) ||
         (YEARFROMDFDATE(d) != YEARFROMDFDATE(date)) )
         continue;

      // find the day
      int   iDay;
      iDay = (int) DAYFROMDFDATE(d);
      if (!papMem[iDay]) {
         papMem[iDay] = new CMem;
         MemZero (papMem[iDay]);
      }

      // append
      MemCat (papMem[iDay], L"<li>");
      if (fWithLinks) {
         MemCat (papMem[iDay], L"<a href=sp:");
         MemCat (papMem[iDay], (int) pCall->m_dwID);
         MemCat (papMem[iDay], L">");
      }
      PWSTR psz;
      if (pCall->m_dwAttend == (DWORD)-1)
         psz = NULL;
      else
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pCall->m_dwAttend));
      if (!psz)
         psz = L"Unknown";

      MemCatSanitize (papMem[iDay], psz);

      if (fWithLinks)
         MemCat (papMem[iDay], L"</a>");
      MemCat (papMem[iDay], L" - ");

      // time
      WCHAR szTemp[64];
      MemCat (papMem[iDay], L"<bold>");
      if ((pCall->m_dwStart != (DWORD) -1) && (pCall->m_dwEnd != (DWORD)-1)) {
         DFTIMEToString (pCall->m_dwStart, szTemp);
         MemCat (papMem[iDay], szTemp);
         MemCat (papMem[iDay], L" to ");
         DFTIMEToString (pCall->m_dwEnd, szTemp);
         MemCat (papMem[iDay], szTemp);
      }
      else if (pCall->m_dwStart != (DWORD)-1) {
         DFTIMEToString (pCall->m_dwStart, szTemp);
         MemCat (papMem[iDay], szTemp);
      }
      else if (pCall->m_dwEnd != (DWORD)-1) {
         DFTIMEToString (pCall->m_dwEnd, szTemp);
         MemCat (papMem[iDay], szTemp);
      }
      else {   // both -1
         MemCat (papMem[iDay], L"Any time");
      }
      MemCat (papMem[iDay], L"</bold>");
     
      
      // finish up table
      MemCat (papMem[iDay], L"</li>");

      // location
      if (((PWSTR)pCall->m_memDescription.p)[0]) {
         MemCat (papMem[iDay], L"<p>");
         MemCatSanitize (papMem[iDay], (PWSTR) pCall->m_memDescription.p);
         MemCat (papMem[iDay], L"</p>");
      }

   }
}


/*******************************************************************************
PhoneEnumForPlanner - Enumerates the meetings in a format the planner can use.

inputs
   DFDATE      date - start date
   DWORD       dwDays - # of days to generate
   PCListFixed palDays - Pointer to an array of CListFixed. Number of elements
               is dwDays. These will be concatenated with relevent information.
               Elements in the list are PLANNERITEM strctures.
   BOOL        fForCombo - If TRUE then it assumes phone calls without a scheduled
               time happen Today(), if they're not already scheduled. And, it doesn't update scheduling.
returns
   none
*/
void PhoneEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays, BOOL fForCombo)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   PhoneInit();
   // bring reoccurring Meetings up to date. 1 day in advance so can use PhoneSummary for tomrrow page
   gCurPhone.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(date)+(60*24)*(dwDays+1)));
   gCurPhone.Sort();  // make sure they're sorted

   DWORD i;
   PCPhoneCall pCall;

   // figure out when the upcoming days are
#define  MAXDAYS  28
   DFDATE   adDay[MAXDAYS], dMax;
   dwDays = min(dwDays,MAXDAYS);
   for (i = 0; i < dwDays; i++)
      dMax = adDay[i] = MinutesToDFDATE(DFDATEToMinutes(date)+(60*24)*i);


   // find all the Calls and phone callsfor the date
   PLANNERITEM pi;
   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      pCall = gCurPhone.CallGetByIndex(i);

      BOOL  fFixed;
      DFDATE pdate;
      pdate = pCall->m_dwDate;
      fFixed = (pCall->m_dwStart != (DWORD)-1);

      PPLANTASKITEM pt;
      PLANTASKITEM ptCombo;
      if (!fFixed && pCall->m_PlanTask.Num()) {
         // before looking at the date specified, verify it to adjust the
         // date forward if necessry
         gCurPhone.m_fDirty |= pCall->m_PlanTask.Verify (15, today, pdate ? pdate : today, 600);

         pt = pCall->m_PlanTask.Get(0);
         pdate = pt->date;
      }
      if (!pdate)
         pdate = today;

      if (pCall->m_reoccur.m_dwPeriod || (pdate < date) || (pdate > dMax) )
         continue;

      // figure out which date this is on
      DWORD d;
      for (d = 0; d < dwDays; d++)
         if (adDay[d] == pdate)
            break;
      if (d >= dwDays)
         continue;

      // reverify if don't have anything
      // BUGFIX - If this is for a combo view then don't assign phone call dates
      if (!fForCombo && !fFixed && !pCall->m_PlanTask.Num()) {
         gCurPhone.m_fDirty |= pCall->m_PlanTask.Verify (15, today, pdate, 600);
      }
      if (!fFixed) {
         // if this is for a combo and don't have anything planned then
         // make something up (assume it's today)
         if (fForCombo && !pCall->m_PlanTask.Num()) {
            memset (&ptCombo, 0, sizeof(ptCombo));
            pt = &ptCombo;
            pt->date = pdate;
            pt->dwPriority = 100 + i;
            pt->dwTimeAllotted = 15;
         }
         else
            pt = pCall->m_PlanTask.Get(0);
      }

      // and the time
      // remember this
      memset (&pi, 0, sizeof(pi));
      pi.fFixedTime = fFixed;
      if (pi.fFixedTime) {
         // have specified time so use that
         pi.dwTime = pCall->m_dwStart;
         if ((pCall->m_dwEnd != (DWORD)-1) && (pCall->m_dwEnd > pCall->m_dwStart))
            pi.dwDuration = DFTIMEToMinutes(pCall->m_dwEnd) - DFTIMEToMinutes(pCall->m_dwStart);
         pi.fFixedTime = TRUE;
      }
      else {
         pi.dwTime = pt->start;
         pi.dwDuration = pt->dwTimeAllotted;
         pi.dwPriority = pt->dwPriority;
      }
      if (!pi.dwDuration)
         pi.dwDuration = 15;  // assume 15 minute phone calls
      pi.dwMajor = pCall->m_dwID;
      pi.dwType = 1;
      pi.pMemMML = new CMem;
      PCMem pMem = pi.pMemMML;

      // MML
      MemZero (pMem);
      if (!gfPrinting) {
         MemCat (pMem, L"<a href=sp:");
         MemCat (pMem, (int) pCall->m_dwID);
         MemCat (pMem, L">");
      }
      PWSTR psz;
      if (pCall->m_dwAttend == (DWORD)-1)
         psz = L"Unknown";
      else
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pCall->m_dwAttend));
      if (!psz)
         continue;

      MemCatSanitize (pMem, psz);

      if (!gfPrinting) {
         MemCat (pMem, L"</a>");
      }

      MemCat (pMem, L" <italic>(Scheduled call)</italic>");

      if (((PWSTR) pCall->m_memDescription.p)[0]) {
         MemCat (pMem, L"<br/>");
         MemCatSanitize (pMem, (PWSTR) pCall->m_memDescription.p);
      }


      // add it
      palDays[d].Add(&pi);
   }
   gCurPhone.Flush();

}

/*******************************************************************************
PhoneAdjustPriority - Adjusts the priority of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DWORD       dwPriority - New priority
returns
   none
*/
void PhoneAdjustPriority (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwPriority)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   PhoneInit();

   PCPhoneCall pCall;
   pCall = gCurPhone.CallGetByID (dwMajor);
   if (!pCall)
      return;
   PPLANTASKITEM pi;
   pi = pCall->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->dwPriority = dwPriority;

   gCurPhone.m_fDirty = TRUE;
   gCurPhone.Flush();
}

/*******************************************************************************
PhoneChangeDate - Adjusts the date of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DFDATE      date - new date
returns
   none
*/
void PhoneChangeDate (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFDATE date)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   PhoneInit();

   PCPhoneCall pCall;
   pCall = gCurPhone.CallGetByID (dwMajor);
   if (!pCall)
      return;
   PPLANTASKITEM pi;
   pi = pCall->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->date = date;

   // adjust the date too
   if (pCall->m_dwDate) {
      pCall->m_dwDate = date;

      // and fix the alarm time
      CalcAlarmTime (pCall->m_dwDate, pCall->m_dwStart, pCall->m_dwAlarm,
         &pCall->m_dwAlarmDate, &pCall->m_dwAlarmTime);
   }

   gCurPhone.m_fDirty = TRUE;
   gCurPhone.Flush();
}

/*******************************************************************************
PhoneChangeTime - Adjusts the date of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DFTIME      time -new time
returns
   none
*/
void PhoneChangeTime (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFTIME time)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   PhoneInit();

   PCPhoneCall pCall;
   pCall = gCurPhone.CallGetByID (dwMajor);
   if (!pCall)
      return;
   PPLANTASKITEM pi;
   pi = pCall->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->start = time;

   gCurPhone.m_fDirty = TRUE;
   gCurPhone.Flush();
}


/*******************************************************************************
PhoneCheckAlarm - Check to see if any alarms go off.

inputs
   DFDATE      today - today's date
   DFTIME      now - current time
*/
void PhoneCheckAlarm (DFDATE today, DFTIME now)
{
   HANGFUNCIN;
   // first of all, amke sure there are Calls to show
   PhoneInit();
   // don't do either of these to save CPU
   //gCurPhone.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24));
   //gCurPhone.Sort();  // make sure they're sorted

   DWORD i;
   PCPhoneCall pCall;

   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      pCall = gCurPhone.CallGetByIndex(i);

      if (pCall->m_reoccur.m_dwPeriod || (!pCall->m_dwAlarmDate) )
         continue;
      if (pCall->m_dwAlarmDate > today)
         continue;
      if ((pCall->m_dwAlarmDate == today) && (pCall->m_dwAlarmTime > now))
         continue;

      // else, have Call
      CMem  mSub, mMain, *pMem;
      MemZero (&mSub);
      MemZero (&mMain);
      pMem = &mSub;

      // main
      WCHAR szTemp[64];
      MemCat (&mMain, L"You have a scheduled call with ");

      PWSTR psz;
      if (pCall->m_dwAttend == (DWORD)-1)
         psz = L"Unknown";
      else
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pCall->m_dwAttend));
      if (!psz)
         psz = L"Unknown";

      MemCatSanitize (&mMain, psz);

      MemCat (&mMain, L" at ");
      DFTIMEToString (pCall->m_dwStart, szTemp);
      MemCat (&mMain, szTemp);
      if (pCall->m_dwDate != today) {
         MemCat (&mMain, L" on ");
         DFDATEToString (pCall->m_dwDate, szTemp);
         MemCat (&mMain, szTemp);
      }
      MemCat (&mMain, L".");


      if (((PWSTR) pCall->m_memDescription.p)[0]) {
         MemCatSanitize (pMem, (PWSTR) pCall->m_memDescription.p);
      }

      // UI
      DWORD dwSnooze;
      dwSnooze = AlarmUI ((PWSTR)mMain.p, (PWSTR)mSub.p);

      if (dwSnooze) {
         __int64 iMin;
         iMin = DFDATEToMinutes(Today()) +
            DFTIMEToMinutes(Now()) +
            (__int64) dwSnooze;
         pCall->m_dwAlarmDate = MinutesToDFDATE(iMin);
         iMin -= DFDATEToMinutes (pCall->m_dwAlarmDate);
         pCall->m_dwAlarmTime = TODFTIME(iMin / 60, iMin % 60);
      }
      else {
         pCall->m_dwAlarmDate = 0;
         pCall->m_dwAlarmTime = 0;
         pCall->m_dwAlarm = 0;
      }

      // flush
      gCurPhone.m_fDirty = TRUE;
      gCurPhone.Flush();

   }
}
