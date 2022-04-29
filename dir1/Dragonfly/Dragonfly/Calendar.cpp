/**********************************************************************
Calendar.cpp - Code that handles the calendar UI.

begun 8/19/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <tapi.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


#define  HOLIDAYGROUPS     7
#define  GROUP_USA         0
#define  GROUP_CANADA      1
#define  GROUP_AUSTRALIA   2
#define  GROUP_CHRISTIAN   3
#define  GROUP_SOLSTICE    4
#define  GROUP_NEWZEALAND  5
#define  GROUP_FRANCE      6

DFDATE gdateCalendar = Today();    // month that the calendar displays
DFDATE gdateCalendarCombo = Today();  // to show on the combo
static DFDATE gdateCalendarYearly = Today();  // to show on the combo
static WCHAR gszLogEntry[] = L"LogEntry";
static DWORD gdwNode;   // node viewing in daily log
static DWORD gdwListCB = 0;   // default index into the list combo box for CalendarListPage
static DWORD gdwListSummaryCB = 0;   // default index into the list combo box for CalendarSummaryPage
static BOOL gfOnlyShowRestOfYear = TRUE;  // used by annual calendar views

static WCHAR gszViewNext[] = L"ViewNext";
static WCHAR gszLi[] = L"<li>";
static WCHAR gszED[] = L"e:%d";
static WCHAR gszCalendarLog[] = L"CalendarLog";

PCMMLNode GetCalendarLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode);
void HolidaysFromMonth (DWORD dwMonth, DWORD dwYear, PCMem  paMem);

/***********************************************************************
LoadInHolidayGroups - Filles in an array of bool (size holidaygroups),
with flags indicating if that holiday group is turned on.

inputs
   BOOL     *pafGroup - HOLIDAYGROUPS elemes
returns
   none
*/
void LoadInHolidayGroups (BOOL *pafGroup)
{
   HANGFUNCIN;
   // first of all set
   memset (pafGroup, 0, sizeof(BOOL) * HOLIDAYGROUPS);
   pafGroup[GROUP_AUSTRALIA] = TRUE;
   pafGroup[GROUP_SOLSTICE] = TRUE;

   // load it in
   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszCalendarNode);
   if (!pNode)
      return;   // unexpected error

   WCHAR szTemp[16];
   DWORD i;
   for (i = 0; i < HOLIDAYGROUPS; i++) {
      swprintf (szTemp, L"holidaygroup%d", i);
      pafGroup[i] = (BOOL) NodeValueGetInt (pNode, szTemp, (int) pafGroup[i]);
   }

   gpData->NodeRelease(pNode);
}

/***********************************************************************
SaveHolidayGroups - Saves holiday group on flags to the user's preferences.

inputs
   BOOL     *pafGroup - HOLIDAYGROUPS elemes
returns
   none
*/
void SaveHolidayGroups (BOOL *pafGroup)
{
   HANGFUNCIN;
   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszCalendarNode);
   if (!pNode)
      return;   // unexpected error

   WCHAR szTemp[16];
   DWORD i;
   for (i = 0; i < HOLIDAYGROUPS; i++) {
      swprintf (szTemp, L"holidaygroup%d", i);
      NodeValueSet (pNode, szTemp, (int) pafGroup[i]);
   }

   gpData->NodeRelease(pNode);
}

/***********************************************************************
CountOccurances - Countt he nuumber of occurances of a string.

inputs
   PWSTR    pszLookIn - Main string to look in.
   PWSTR    pszFind - To count. Case sensative
returns
   DWORD - count
*/
DWORD CountOccurances (PWSTR pszLookIn, PWSTR pszFind)
{
   HANGFUNCIN;
   if (!pszLookIn)
      return 0;

   DWORD dwCount;
   dwCount = 0;
   PWSTR pszCur;
   for (pszCur = wcsstr(pszLookIn, pszFind); pszCur; pszCur = wcsstr(pszCur+1, pszFind))
      dwCount++;

   return dwCount;
}

/***********************************************************************
CalendarPage - Page callback for viewing a phone message
*/
BOOL CalendarPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the dropdown to the right date
         DateControlSet (pPage, gszDate, gdateCalendar);
      }
      break;   // go to default handler

   case ESCN_DATECHANGE:
      {
         // if the date changes then refresh the page
         PESCNDATECHANGE p = (PESCNDATECHANGE) pParam;

         if (_wcsicmp(p->pControl->m_pszName, gszDate))
            break;   // wrong one

         // BUGFIX - if change to no date, use today
         if ((p->iMonth >= 1) && (p->iYear >= 1))
            gdateCalendar = TODFDATE (1, p->iMonth, p->iYear);
         else
            gdateCalendar = Today();
         pPage->Exit (gszRedoSamePage);
      }
      return TRUE;
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         // if its a reminder link delete and refresh
         if ((p->psz[0] == L'c') && (p->psz[1] == L'd') && (p->psz[2] == L':')) {
            DFDATE d = (DFDATE)_wtoi(p->psz+3);

            // pressed the day button
            gdateCalendarCombo = d;
            pPage->Exit (L"r:258");
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"MONTH")) {
            MemZero (&gMemTemp);

            DFDATE today;
            today = Today();
            // how many days in the month
            if (gdateCalendar == -1)
               gdateCalendar = today;
            int iDays, iStartDOW;
            iDays = EscDaysInMonth (MONTHFROMDFDATE(gdateCalendar),
               YEARFROMDFDATE(gdateCalendar), &iStartDOW);
            if (!iDays) {
               iDays = 31;
               iStartDOW = 0;
            }

            // determine what links should have for reminders, task, etc.
            PCMem    apmemReminders[32], apmemProjects[32], apmemTasks[32], apmemMeetings[32], apmemCalls[32];
            CMem     aMemHoliday[32];
            memset (apmemReminders, 0, sizeof(apmemReminders));
            memset (apmemProjects, 0, sizeof(apmemProjects));
            memset (apmemTasks, 0, sizeof(apmemTasks));
            memset (apmemMeetings, 0, sizeof(apmemMeetings));
            memset (apmemCalls, 0, sizeof(apmemCalls));
            ReminderMonthEnumerate (gdateCalendar, apmemReminders);
            SchedMonthEnumerate (gdateCalendar, apmemMeetings);
            PhoneMonthEnumerate (gdateCalendar, apmemCalls);
            WorkMonthEnumerate (gdateCalendar, apmemTasks);
            ProjectMonthEnumerate (gdateCalendar, apmemProjects);
            HolidaysFromMonth (MONTHFROMDFDATE(gdateCalendar), YEARFROMDFDATE(gdateCalendar), aMemHoliday);

            // starting days 3 back
            int   iDOW, iDay;
            iDay = 1 - iStartDOW;

            for (; iDay <= iDays; ) {
               // loop by the week
               MemCat (&gMemTemp, L"<tr>");

               // do the week
               for (iDOW = 0; iDOW < 7; iDOW++, iDay++) {
                  if ((iDay < 1) || (iDay > iDays)) {
                     // blank day
                     MemCat (&gMemTemp, L"<xdaynull/>");
                     continue;
                  }

                  PWSTR pszHoliday;
                  pszHoliday = (PWSTR) aMemHoliday[iDay].p;
                  // else it has a number

                  // if it's today
                  if ( (MONTHFROMDFDATE(gdateCalendar) == MONTHFROMDFDATE(today)) &&
                     (YEARFROMDFDATE(gdateCalendar) == YEARFROMDFDATE(today)) &&
                     ((int) DAYFROMDFDATE(today) == iDay))
                        MemCat (&gMemTemp, L"<xday bgcolor=#ffff40>");
                  else {
                     if (pszHoliday)
                        MemCat (&gMemTemp, L"<xday bgcolor=#80ffc0>");
                     else if ((iDOW == 0) || (iDOW == 6))
                        MemCat (&gMemTemp, L"<xday bgcolor=#b0b0ef>");
                     else
                        MemCat (&gMemTemp, L"<xday>");
                  }


                  // show the number
                  MemCat (&gMemTemp, L"<big><big><big><bold>");

                  // BUGFIX - Link to the combo page instead of the daily log
                  // show the number
                  if (!gfPrinting) {
                     MemCat (&gMemTemp, L"<a href=cd:");
                     MemCat (&gMemTemp, (int) TODFDATE(iDay, MONTHFROMDFDATE(gdateCalendar), YEARFROMDFDATE(gdateCalendar)));
                     MemCat (&gMemTemp, L">");
                     MemCat (&gMemTemp, L"<xhoverhelpshort>");
                     // BUGFIX - day of the week
                     DWORD dwDayOfYear, dwDaysInYear, dwWeekOfYear;
                     DateToDayOfYear (TODFDATE(iDay, MONTHFROMDFDATE(gdateCalendar), YEARFROMDFDATE(gdateCalendar)),
                        &dwDayOfYear, &dwDaysInYear, &dwWeekOfYear);
                     MemCat (&gMemTemp, L"<italic>Week ");
                     MemCat (&gMemTemp, dwWeekOfYear);
                     MemCat (&gMemTemp, L", Day ");
                     MemCat (&gMemTemp, (int) dwDayOfYear);
                     MemCat (&gMemTemp, L", ");
                     MemCat (&gMemTemp, (int) dwDaysInYear - dwDayOfYear);
                     MemCat (&gMemTemp, L" days left</italic>");

                     MemCat (&gMemTemp, L"</xhoverhelpshort>");
                  }
                  MemCat (&gMemTemp, iDay);
                  if (!gfPrinting)
                     MemCat (&gMemTemp, L"</a>");
                  MemCat (&gMemTemp, L"</bold></big></big></big>");

                  // if there's a holiday show that
                  if (pszHoliday) {
                     MemCat (&gMemTemp, L"<br/><big><bold>");
                     MemCatSanitize (&gMemTemp, pszHoliday);
                     MemCat (&gMemTemp, L"</bold></big>");
                  }

                  // see if there's any meetings, etc. today. If so show
                  // PCMem    apmemReminders[31], apmemProjects[31], apmemTasks[31], apmemMeetings[31];
                  DWORD dwCount;
                  if (apmemMeetings[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                        MemCat (&gMemTemp, L"<a href=r:113>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemMeetings[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Meetings (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Meeting");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemMeetings[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemCalls[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                     MemCat (&gMemTemp, L"<a href=r:117>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemCalls[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Scheduled calls (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Scheduled call");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemCalls[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemReminders[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                     MemCat (&gMemTemp, L"<a href=r:114>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemReminders[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Reminders (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Reminder");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemReminders[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemTasks[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                        MemCat (&gMemTemp, L"<a href=r:115>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemTasks[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Tasks (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Task");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemTasks[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemProjects[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                        MemCat (&gMemTemp, L"<a href=r:145>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemProjects[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Projects (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Project");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemProjects[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }

                  MemCat (&gMemTemp, L"</xday>");
               }

               MemCat (&gMemTemp, L"</tr>");
            }

            // free up memory for reminders, meetings, etc.
            DWORD i;
            for (i = 0; i < 32; i++) {
               if (apmemReminders[i])
                  delete apmemReminders[i];
               if (apmemProjects[i])
                  delete apmemProjects[i];
               if (apmemTasks[i])
                  delete apmemTasks[i];
               if (apmemMeetings[i])
                  delete apmemMeetings[i];
               if (apmemCalls[i])
                  delete apmemCalls[i];
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}





/*****************************************************************************
GetCalendarLogNode - Given a date, this returns a day-month-specific node for the
calendar log.


inputs
   DFDATE         date - Date to look for
   BOOL           fCreateIfNotExit - if TRUE create the database node
                  if it doesn't exist. If FALSE, return NULL if it doesn't exist.
   DWORD          *pdwNode - Filled in with a node specific to the month/year.
returns
   PCMMLNode - Node (must be released) specific to the month/year. NULL if cant find/create.
*/
PCMMLNode GetCalendarLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode)
{
   HANGFUNCIN;
   PCMMLNode   pNew;

   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszCalendarNode);
   if (!pNode)
      return FALSE;   // unexpected error

   pNew = DayMonthYearTree (pNode, date, gszCalendarLog, fCreateIfNotExist, pdwNode);

   gpData->NodeRelease(pNode);

   return pNew;
}



/*******************************************************************************
CalendarLogRandom - Finds a random day in the calendar log. This is used so
reflections can pick a day at random.

inputs
   nonr
returns
   DFDATE - date that returns. 0 if can't find
*/
DFDATE CalendarLogRandom (void)
{
   HANGFUNCIN;
   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszCalendarNode);
   if (!pNode)
      return FALSE;   // unexpected error

   PCListFixed   pl;
   pl = EnumMonthYearTree(pNode, gszCalendarLog);
   if (!pl || !pl->Num()) {
      if (pl)
         delete pl;
      gpData->NodeRelease(pNode);
      return 0;
   }

   gpData->NodeRelease(pNode);

   // pick a month and year at random
   DFDATE date;
   date = *((DFDATE*) pl->Get(rand() % pl->Num()));
   delete pl;

   // pick a day at random
   DWORD dwDay;
   dwDay = (DWORD)(rand()%31)+1;
   date = TODFDATE(dwDay, MONTHFROMDFDATE(date), YEARFROMDFDATE(date));

   // trey getting it
   pNode = GetCalendarLogNode(date, FALSE, &dwDay);
   if (pNode)
      gpData->NodeRelease(pNode);

   return pNode ? date : 0;
}

/*******************************************************************************8
CalendarLogAdd - Adds an entry to the daily log.

inputs
   DFDATE      date - date to add for. If this is 0 then the log isn't added.
   DFTIME      start - start time.
   DFTIME      end - end time
   PWSTR       psz - string to display
   DWORD       dwLink - Link to this database node, or -1 for no link.
               If the daily log already has a link with dwLink the old
               one is replaced UNLESS dwLink==-1.
   BOOL        fReplaceOld - Defaults to TRUE. If FALSE then the old entry is
               not replaced, so can have multiple entries for the same day
               pointing the the same link
returns
   BOOL - TRUE if OK
*/
BOOL CalendarLogAdd (DFDATE date, DFTIME start, DFTIME end, PWSTR psz, DWORD dwLink, BOOL fReplaceOld)
{
   HANGFUNCIN;
   PCMMLNode   pNode;
   DWORD dwNode;
   pNode = GetCalendarLogNode (date, TRUE, &dwNode);
   if (!pNode)
      return FALSE;

   // add it
   BOOL fRet;
   fRet = NodeElemSet (pNode, gszLogEntry, psz, (int)dwLink,
      (dwLink != (DWORD)-1) && fReplaceOld, date, start, end);

   // also write the date here so search engine can find
   NodeValueSet (pNode, gszDate, (int)date);

   gpData->NodeRelease (pNode);
   gpData->Flush();
   return fRet;
}

/*******************************************************************************8
CalendarLogRemove - Removes an entry from the calendar log

inputs
   DFDATE      date - date to add for. If this is 0 then the log isn't added.
   DWORD       dwLink - Link to this database node
returns
   BOOL - TRUE if OK
*/
BOOL CalendarLogRemove (DFDATE date, DWORD dwLink)
{
   HANGFUNCIN;
   PCMMLNode   pNode;
   DWORD dwNode;
   pNode = GetCalendarLogNode (date, TRUE, &dwNode);
   if (!pNode)
      return FALSE;

   // figure out number
   WCHAR szTemp[16];
   _itow ((int)dwLink, szTemp, 10);

   // see if it exists
   DWORD dwFind;
   dwFind = pNode->ContentFind (gszLogEntry, gszNumber, szTemp);
   if (dwFind < pNode->ContentNum())
      pNode->ContentRemove (dwFind);

   gpData->NodeRelease (pNode);
   gpData->Flush();
   return TRUE;
}


/***************************************************************************
CalendarSetView - Tells the meeting unit what meeting is being viewed/edited.

inputs
   DWORD    dwNode - index
returns
   BOOL - TRUE if success
*/
BOOL CalendarSetView (DWORD dwNode)
{
   HANGFUNCIN;
   gdwNode = dwNode;
   return TRUE;
}



/***********************************************************************
CalendarLogPage - Page callback for viewing an existing user.
*/
BOOL CalendarLogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            // get the date
            WCHAR szTemp[64];
            DFDATEToString (NodeValueGetInt (pNode, gszDate, 0), szTemp);
            MemCat (&gMemTemp, szTemp);

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
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszLogEntry, TRUE);

            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
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
               // dont bother with date since already known
               // DFDATEToString (pnlg->date, szTemp);
               // MemCat (&gMemTemp, szTemp);
               // MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString (pnlg->start, szTemp);
               MemCat (&gMemTemp, szTemp);
               if (pnlg->end != -1) {
                  DFTIMEToString (pnlg->end, szTemp);
                  MemCat (&gMemTemp, L" to ");
                  MemCat (&gMemTemp, szTemp);
               }
               
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

// BUGBUG - 2.0 - Watch what user does on computer (when it's on, off, what apps used) and
// log all of that

/***********************************************************************
CalendarHolidaysPage - Page callback for viewing a phone message
*/
BOOL CalendarHolidaysPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         BOOL afHoliday[HOLIDAYGROUPS];
         LoadInHolidayGroups (afHoliday);

         // write out
         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[16];
         for (i = 0; i < HOLIDAYGROUPS; i++) {
            swprintf (szTemp, L"h%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (!pControl)
               continue;

            pControl->AttribSetBOOL (gszChecked, afHoliday[i]);
         }
      }
      break;   // go to default handler


   case ESCM_LINK:
      {
         // save the holiday groups away
         BOOL afHoliday[HOLIDAYGROUPS];
         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[16];
         for (i = 0; i < HOLIDAYGROUPS; i++) {
            swprintf (szTemp, L"h%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (!pControl)
               continue;

            afHoliday[i] = pControl->AttribGetBOOL (gszChecked);
         }

         SaveHolidayGroups (afHoliday);

      }
      break;   // go to default handler
   };

   return DefPage (pPage, dwMessage, pParam);
}


static WCHAR gszMartinLuther[] = L"Martin Luther King, Jr's Birthday";
static WCHAR gszIndependence[] = L"Independence Day";
static WCHAR gszHaloween[] = L"Haloween";
static WCHAR gszThanksgiving[] = L"Thanksgiving";
static WCHAR gszChristmas[] = L"Christmas";
static WCHAR gszChristmasEve[] = L"Christmas Eve";
static WCHAR gszPresidentsDay[] = L"Presidents Day";
static WCHAR gszMemorialDay[] = L"Memorial Day";
static WCHAR gszColumbusDay[] = L"Columbus Day";
static WCHAR gszVeteransDay[] = L"Veteran's Day";
static WCHAR gszNewYearsDay[] = L"New Year's Day";
static WCHAR gszNewYearsEve[] = L"New Year's Eve";
static WCHAR gszFathersDay[] = L"Fathers' Day";
static WCHAR gszAmericanIndianDay[] = L"American Indian Day";
static WCHAR gszArmedForcesDay[] = L"Armed Forces Day";
static WCHAR gszFlagDay[] = L"Flag Day";
static WCHAR gszMothersDay[] = L"Mothers' Day";
static WCHAR gszStPatricksDay[] = L"St. Patrick's Day";
static WCHAR gszValentinesDay[] = L"Valentine's Day";
static WCHAR gszLaborDay[] = L"Labor Day";
static WCHAR gszElectionDay[] = L"Election Day";
static WCHAR gszGoodFriday[] = L"Good Friday";
static WCHAR gszEasterSunday[] = L"Easter Sunday";
static WCHAR gszEasterMonday[] = L"Easter Monday";
static WCHAR gszBoxingDay[] = L"Boxing Day";
static WCHAR gszAprilFoolsDay[] = L"April Fool's Day";
static WCHAR gszNewYearsDayCelebrated[] = L"New Year's Day Celebrated";
static WCHAR gszDayAfterNewYearsDay[] = L"Day after New Year's Day";
static WCHAR gszChristmasCelebrated[] = L"Christmas Celebrated";
static WCHAR gszAucklandAnniversaryDay[] = L"Auckland Anniversary Day";
static WCHAR gszChathamIslands[] = L"Chatham Islands Anniversary Day";
static WCHAR gszNelson[] = L"Nelson Anniversary Day";
static WCHAR gszOtago[] = L"Otago Anniversary Day";
static WCHAR gszSouthland[] = L"Southland Anniversary Day";
static WCHAR gszTaranaki[] = L"Taranaki Anniversary Day";
static WCHAR gszWellington[] = L"Wellington Anniversary Day";
static WCHAR gszWestland[] = L"Westland Anniversary Day";
static WCHAR gszWaitangiDay[] = L"Waitangi Anniversary Day";
static WCHAR gszQueensBirthday[] = L"Queen's Birthday Holiday";
static WCHAR gszCanteburySouth[] = L"Cantebury (South) Anniversary Day";
static WCHAR gszHawkesBay[] = L"Hawkes' Bay Anniversary Day";
static WCHAR gszMarlborough[] = L"Marlborough Anniversary Day";
static WCHAR gszCanteburyNorthCentral[] = L"Cantebury (North Central) Anniversary Day";
static WCHAR gszEasterAscension[] = L"Feast of the Ascension";
static WCHAR gszEasterPentecost[] = L"Pentecost";

/***********************************************************************
stristr - Finds an occrance of the string within the other

inputs
   PWSTR    psz - string to search in
   PWSTR    pszFind - string to findr
returns
   PWSTR - NULL if error, else where found
*/
PWSTR stristr (PWSTR psz, PWSTR pszFind)
{
   HANGFUNCIN;
   // lowercase first char
   WCHAR cLower;
   DWORD dwLen;
   cLower = towlower (pszFind[0]);
   dwLen = wcslen(pszFind);

   PWSTR pCur;
   for (pCur = psz; *pCur; pCur++) {
      if (towlower(*pCur) != cLower)
         continue;

      // string compare
      if (_wcsnicmp (pCur, pszFind, dwLen))
         continue;

      // else found
      return pCur;
   }

   return NULL;
}



/**********************************************************************
Holiday - Put a holiday in the the given day
   PCMem    pMem - Pointer to an array of 32 CMem objects, 1..31 are used for a date
   DWORD    dwDate - Date in pMem to use
   PWSTR    psz - String to append. If the string already exists then it's not appended
*/
void Holiday (PCMem paMem, DWORD dwDate, PWSTR psz)
{
   HANGFUNCIN;
   if ((dwDate < 1) || (dwDate > 31))
      return;

   PCMem pMem;
   pMem = paMem + dwDate;
   if (!pMem->p || !((PWSTR)(pMem->p))[0]) {
      // empty, so add the string
      MemCat (pMem, psz);
      return;
   }

   // else, see if it's in
   if (stristr((PWSTR)pMem->p, psz))
      return;  // already there

   // add comma,etc.
   MemCat (pMem, L", ");
   MemCat (pMem, psz);
   // done
}

/***********************************************************************
Easter - Calculate easter and put in.

inputs
   DWORD    dwMonth
   DWORD    dwYear
   PWSTR    paMem
   BOOL     fAscension - Do the acension and stuff
*/
void Easter (DWORD dwMonth, DWORD dwYear, PCMem paMem, BOOL fAscension = FALSE)
{
   HANGFUNCIN;
   if ((dwMonth < 3) || (dwMonth > 6))
      return;  // easter only in march or april, included ascension, etc.

   // easter sunday. This is cheap. Look in a table for the next 100 years
   if ((dwYear < 2000) || (dwYear >= 2100))
      return;
   DFDATE   adate[100] = {
      TODFDATE (23, 4, 2000),
      TODFDATE (15, 4, 2001),
      TODFDATE (31, 3, 2002),
      TODFDATE (20, 4, 2003),
      TODFDATE (11, 4, 2004),
      TODFDATE (27, 3, 2005),
      TODFDATE (16, 4, 2006),
      TODFDATE (8, 4, 2007),
      TODFDATE (23, 3, 2008),
      TODFDATE (12, 4, 2009),
      TODFDATE (4, 4, 2010),
      TODFDATE (24, 4, 2011),
      TODFDATE (8, 4, 2012),
      TODFDATE (31, 5, 2013),
      TODFDATE (20, 4, 2014),
      TODFDATE (5, 4, 2015),
      TODFDATE (27, 3, 2016),
      TODFDATE (16, 4, 2017),
      TODFDATE (1, 4, 2018),
      TODFDATE (21, 4, 2019),
      TODFDATE (12, 4, 2020),
      TODFDATE (4, 4, 2021),
      TODFDATE (17, 4, 2022),
      TODFDATE (9, 4, 2023),
      TODFDATE (31, 3, 2024),
      TODFDATE (20, 4, 2025),
      TODFDATE (5, 4, 2026),
      TODFDATE (28, 3, 2027),
      TODFDATE (16, 4, 2028),
      TODFDATE (1, 4, 2029),
      TODFDATE (21, 4, 2030),
      TODFDATE (13, 4, 2031),
      TODFDATE (28, 3, 2032),
      TODFDATE (17, 4, 2033),
      TODFDATE (9, 4, 2034),
      TODFDATE (25, 3, 2035),
      TODFDATE (13, 4, 2036),
      TODFDATE (5, 4, 2037),
      TODFDATE (25, 4, 2038),
      TODFDATE (10, 4, 2039),
      TODFDATE (1, 4, 2040),
      TODFDATE (21, 4, 2041),
      TODFDATE (6, 4, 2042),
      TODFDATE (29, 3, 2043),
      TODFDATE (17, 4, 2044),
      TODFDATE (9, 4, 2045),
      TODFDATE (25, 3, 2046),
      TODFDATE (14, 4, 2047),
      TODFDATE (5, 4, 2048),
      TODFDATE (18, 4, 2049),
      TODFDATE (10, 4, 2050),
      TODFDATE (2, 4, 2051),
      TODFDATE (21, 4, 2052),
      TODFDATE (6, 4, 2053),
      TODFDATE (20, 3, 2054),
      TODFDATE (18, 4, 2055),
      TODFDATE (2, 4, 2056),
      TODFDATE (22, 4, 2057),
      TODFDATE (14, 4, 2058),
      TODFDATE (30, 3, 2059),
      TODFDATE (18, 4, 2060),
      TODFDATE (10, 4, 2061),
      TODFDATE (26, 3, 2062),
      TODFDATE (15, 4, 2063),
      TODFDATE (6, 4, 2064),
      TODFDATE (29, 3, 2065),
      TODFDATE (11, 4, 2066),
      TODFDATE (3, 4, 2067),
      TODFDATE (22, 4, 2068),
      TODFDATE (14, 4, 2069),
      TODFDATE (30, 3, 2070),
      TODFDATE (19, 4, 2071),
      TODFDATE (10, 4, 2072),
      TODFDATE (26, 3, 2073),
      TODFDATE (15, 4, 2074),
      TODFDATE (7, 4, 2075),
      TODFDATE (19, 4, 2076),
      TODFDATE (11, 4, 2077),
      TODFDATE (3, 4, 2078),
      TODFDATE (23, 4, 2079),
      TODFDATE (7, 4, 2080),
      TODFDATE (30, 3, 2081),
      TODFDATE (19, 4, 2082),
      TODFDATE (4, 4, 2083),
      TODFDATE (26, 3, 2084),
      TODFDATE (15, 4, 2085),
      TODFDATE (31, 3, 2086),
      TODFDATE (20, 4, 2087),
      TODFDATE (11, 4, 2088),
      TODFDATE (3, 4, 2089),
      TODFDATE (16, 4, 2090),
      TODFDATE (8, 4, 2091),
      TODFDATE (30, 3, 2092),
      TODFDATE (12, 4, 2093),
      TODFDATE (4, 4, 2094),
      TODFDATE (24, 4, 2095),
      TODFDATE (15, 4, 2096),
      TODFDATE (31, 3, 2097),
      TODFDATE (20, 4, 2098),
      TODFDATE (12, 4, 2099),
   };

   DFDATE   dSun, dMon, dFri;
   dSun = adate[dwYear-2000];
   dMon = MinutesToDFDATE (DFDATEToMinutes(dSun) + 24*60*1);
   dFri = MinutesToDFDATE (DFDATEToMinutes(dSun) - 24*60*2);

   // figure out the ascension, pentecost
   DFDATE   dAscension, dPentecost;
   dAscension = MinutesToDFDATE (DFDATEToMinutes(dSun) + 24*60*40);
   dPentecost = MinutesToDFDATE (DFDATEToMinutes(dAscension) + 24*60*10);

   // match
   if (MONTHFROMDFDATE(dSun) == dwMonth)
      Holiday (paMem, DAYFROMDFDATE(dSun), gszEasterSunday);
   if (MONTHFROMDFDATE(dMon) == dwMonth)
      Holiday (paMem, DAYFROMDFDATE(dMon), gszEasterMonday);
   if (MONTHFROMDFDATE(dFri) == dwMonth)
      Holiday (paMem, DAYFROMDFDATE(dFri), gszGoodFriday);
   if (fAscension) {
      if (MONTHFROMDFDATE(dAscension) == dwMonth)
         Holiday (paMem, DAYFROMDFDATE(dAscension), gszEasterAscension);
      if (MONTHFROMDFDATE(dPentecost) == dwMonth)
         Holiday (paMem, DAYFROMDFDATE(dPentecost), gszEasterPentecost);
   }
}

/***********************************************************************
USHolidays
*/
void USHolidays (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   DWORD dwDay;

   switch (dwMonth) {
   case 1:  // january
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_THIRD, R_MONDAY), gszMartinLuther);
      Holiday (paMem, 1,  gszNewYearsDay);
      break;
   case 2:  // february
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_THIRD, R_MONDAY), gszPresidentsDay);
      Holiday (paMem, 14, gszValentinesDay);
      break;
   case 3:  // march
      Holiday (paMem, 17, gszStPatricksDay);
      break;
   case 4:  // april
      Holiday (paMem, 1,  gszAprilFoolsDay);
      break;
   case 5:  // may
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszMemorialDay);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_THIRD, R_SATURDAY), gszArmedForcesDay);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_SECOND, R_SUNDAY), gszMothersDay);
      break;
   case 6:  // june
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_THIRD, R_SUNDAY), gszFathersDay);
      Holiday (paMem, 14, gszFlagDay);
      break;
   case 7:  // july
      Holiday (paMem, 4, gszIndependence);
      break;
   case 8:  // august
      break;
   case 9:  // september
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FOURTH, R_FRIDAY), gszAmericanIndianDay);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszLaborDay);
      break;
   case 10: // october
      Holiday (paMem, 31, gszHaloween);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_SECOND, R_MONDAY), gszColumbusDay);
      break;
   case 11: // november
      dwDay = ReoccurFindNth (dwMonth, dwYear, R_FOURTH, R_FRIDAY);
      Holiday (paMem, dwDay, gszThanksgiving);
      Holiday (paMem, dwDay-1, gszThanksgiving);
      Holiday (paMem, 14, gszVeteransDay);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY)+1, gszElectionDay);
      break;
   case 12: // december
      Holiday (paMem, 25, gszChristmas);
      Holiday (paMem, 24, gszChristmasEve);
      Holiday (paMem, 31, gszNewYearsEve);
      break;
   }

   // do easter
   Easter (dwMonth, dwYear, paMem);
}

static WCHAR gszVictoriaDay[] = L"Victoria Day";
static WCHAR gszCanadaDay[] = L"Canada Day";
static WCHAR gszRememberenceDay[] = L"Rememberence Day";

/***********************************************************************
CanadianHolidays
*/
void CanadianHolidays (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   switch (dwMonth) {
   case 1:  // january
      Holiday (paMem, 1, gszNewYearsDay);
      break;
   case 2:  // february
      Holiday (paMem, 14, gszValentinesDay);
      break;
   case 3:  // march
      break;
   case 4:  // april
      break;
   case 5:  // may
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_SECOND, R_SUNDAY), gszMothersDay);
      Holiday (paMem, 24, gszVictoriaDay);
      break;
   case 6:  // june
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_THIRD, R_SUNDAY), gszFathersDay);
      break;
   case 7:  // july
      Holiday (paMem, 1, gszCanadaDay);
      break;
   case 8:  // august
      break;
   case 9:  // september
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszLaborDay);
      break;
   case 10: // october
      Holiday (paMem, 31, gszHaloween);
      Holiday (paMem, 12, gszThanksgiving);
      break;
   case 11: // november
      Holiday (paMem, 11, gszRememberenceDay);
      break;
   case 12: // december
      Holiday (paMem, 25, gszChristmas);
      Holiday (paMem, 24, gszChristmasEve);
      Holiday (paMem, 31, gszNewYearsEve);
      Holiday (paMem, 26, gszBoxingDay);
      break;
   }

   // do easter
   Easter (dwMonth, dwYear, paMem);
}

static WCHAR gszAustraliaDay[] = L"Australia Day";
static WCHAR gszEightHoursDay[] = L"Eight Hours Day(Tas), Labour Day(WA)";
static WCHAR gszLaborDayVic[] = L"Labour Day(Vic)";
static WCHAR gszCanberraDay[] = L"Canberra Day";
static WCHAR gszAnzacDay[] = L"Anzac Day";
static WCHAR gszMayDayNT[] = L"May Day(NT)";
static WCHAR gszLaborDayQld[] = L"Labour Day(Qld)";
static WCHAR gszAdelaideCupDay[] = L"Adelaide Cup Day(Adelaide)";
static WCHAR gszFoundationDay[] = L"Foundation Day(WA)";
static WCHAR gszQueensBirthdayDayNotWA[] = L"Queen's Birthday Holiday(except WA)";
static WCHAR gszPicnicDay[] = L"Picnic Day(NT)";
static WCHAR gszRoyalQueensland[] = L"Royal Queensland Show Holiday(Brisbane)";
static WCHAR gszLaborDayActNSWSA[] = L"Labour Day(Act,NSW,SA), Queen's Birthday Holiday(WA)";
static WCHAR gszMelbourneCupDay[] = L"Melbourne Cup Day(Melbourne)";


/***********************************************************************
MovableHoliday - Call this if the holiday moves depending upon conditions (like
   it falls on a weekend).

inputs
   DWORD    dwAction - One of MOVE_XXX
   DWORD    dwMonthFillingIng - Filling in this month. If it ends up not
               being on this month then don't fill in.
   DWORD    dwYear - Year filling in.
   PWSTR    *paMem - The proper date field is filled in.
   DWORD    dwDaySupposed - Day it's supposed to be, assuming it's not on a weekend.
   DWORD    dwMonthSupposed - Month it's suppposed to be.
   PWSTR    pszString - String to use. This must be a global
returns
   none
*/
#define  MOVE_WEEKENDTOMONDAY          0     // if falls on weekend, move to monday
#define  MOVE_SATTOMON_SUNTOTUES       1     // if falls on a saturday move to monday, sunday to tuesday
#define  MOVE_NEARESTMON               2     // Fri, Sat, Sun to next money, Tues Wed to previous monday

void MovableHoliday (DWORD dwAction, DWORD dwMonthFillingIn, DWORD dwYear, PCMem paMem,
                         DWORD dwDaySupposed, DWORD dwMonthSupposed, PWSTR pszString)
{
   HANGFUNCIN;
   // if it's not the month, or the one before or after then no chance
   if ((dwMonthSupposed != dwMonthFillingIn) && ((dwMonthSupposed+1) != dwMonthFillingIn) &&
      ((dwMonthSupposed-1) != dwMonthFillingIn))
      return;

   // find out about this month and surrounding ones
   int iNum[3], iDOW[3];
   int i, dwm;
   for (i = 0; i < 3; i++) {
      dwm = i + dwMonthSupposed - 1;
      if ((dwm > 12) || (!dwm)) {
         // error condition
         iNum[i] = 31;
         iDOW[i] = 0;
         continue;
      }

      iNum[i] = EscDaysInMonth ((int) dwm, (int) dwYear, (int*)&iDOW[i]);
   }

   // find out what day of the week the proposed day is on
   DWORD iDOWOn;
   iDOWOn = ((int)dwDaySupposed-1+iDOW[1]) % 7;

   // figure out when the next moday, etc. is, and the previous monday, etc is.
   int aiNextDay[7], aiNextMonth[7], aiPreviousDay[7], aiPreviousMonth[7];
   int iCurDay, iCurMonth;
   for (i = 1, iCurDay = (int)dwDaySupposed+1, iCurMonth = (int)dwMonthSupposed; i <= 7; i++) {
      //forward
      // if it's the next month then update some info
      if (iCurDay > iNum[1]) {
         iCurDay = 1;
         iCurMonth = (int)dwMonthSupposed+1;
      }

      // store this away
      aiNextDay[(i+iDOWOn)%7] = iCurDay;
      aiNextMonth[(i+iDOWOn)%7] = iCurMonth;
      iCurDay++;
   }
   for (i = 1, iCurDay = (int)dwDaySupposed-1, iCurMonth = (int)dwMonthSupposed; i <= 7; i++) {
      //back
      // if it's the next month then update some info
      if (iCurDay < 1) {
         iCurDay = iNum[0];
         iCurMonth = (int)dwMonthSupposed-1;
      }

      // store this away
      aiPreviousDay[(iDOWOn-i+7)%7] = iCurDay;
      aiPreviousMonth[(iDOWOn-i+7)%7] = iCurMonth;
      iCurDay--;
   }

   // figure out what day and month it really occurs
   int iDayReally, iMonthReally;
   iDayReally = (int) dwDaySupposed;
   iMonthReally = (int) dwMonthSupposed;
   switch (dwAction) {
   case MOVE_WEEKENDTOMONDAY:
      // if sun or sat then move to next monday
      if ((iDOWOn == 0) || (iDOWOn == 6)) {
         iDayReally = aiNextDay[1]; // 1 == monday
         iMonthReally = aiNextMonth[1];
      }
      break;
   case MOVE_SATTOMON_SUNTOTUES:
      // if on a sat the move to next monday
      if (iDOWOn == 6) {
         iDayReally = aiNextDay[1]; // 1 == monday
         iMonthReally = aiNextMonth[1];
      }

      // if on a sun then move to next tuesday
      if (iDOWOn == 0) {
         iDayReally = aiNextDay[2]; // 2 == tueday
         iMonthReally = aiNextMonth[2];
      }
      break;
   case MOVE_NEARESTMON:
      // if Fri, Sat, or Sun, then move to next monday
      if ((iDOWOn == 0) || (iDOWOn == 6) || (iDOWOn == 5)) {
         iDayReally = aiNextDay[1]; // 1 == monday
         iMonthReally = aiNextMonth[1];
      }

      // if on a Tues, Wed then move to previous monday
      if ((iDOWOn == 3) || (iDOWOn == 4)) {
         iDayReally = aiPreviousDay[1]; // 1 == monday
         iMonthReally = aiPreviousMonth[1];
      }
      break;
   }

   // if month not the same as what looking for no change
   if ((DWORD)iMonthReally != dwMonthFillingIn)
      return;

   // else fill in
   Holiday (paMem, iDayReally, pszString);
}


/***********************************************************************
AustralianHolidays
*/
void AustralianHolidays (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   switch (dwMonth) {
   case 1:  // january
      Holiday (paMem, 1, gszNewYearsDay);
      Holiday (paMem, 26,  gszAustraliaDay);
      break;
   case 2:  // february
      Holiday (paMem, 14, gszValentinesDay);
      break;
   case 3:  // march
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszEightHoursDay);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_SECOND, R_MONDAY), gszLaborDayVic);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_THIRD, R_MONDAY), gszCanberraDay);
      break;
   case 4:  // april
      Holiday (paMem, 25, gszAnzacDay);
      break;
   case 5:  // may
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszLaborDayQld);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_SECOND, R_SUNDAY), gszMothersDay);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_THIRD, R_MONDAY), gszAdelaideCupDay);
      // BUGFIX - May day is on the first monday of the month in NT
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszMayDayNT);
      break;
   case 6:  // june
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszFoundationDay);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_SECOND, R_MONDAY), gszQueensBirthdayDayNotWA);
      break;
   case 7:  // july
      break;
   case 8:  // august
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY),gszPicnicDay);
      Holiday (paMem, 16, gszRoyalQueensland);
      break;
   case 9:  // september
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_SUNDAY), gszFathersDay);
      break;
   case 10: // october
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY),  gszLaborDayActNSWSA);
      break;
   case 11: // november
      // BUGFIX - Melbourne cup day is first tuesday in november
      //papsz[7] = gszMelbourneCupDay;
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_TUESDAY),  gszMelbourneCupDay);
      break;
   case 12: // december
      Holiday (paMem, 25, gszChristmas);
      Holiday (paMem, 24, gszChristmasEve);
      Holiday (paMem, 31, gszNewYearsEve);
      Holiday (paMem, 26, gszBoxingDay);
      break;
   }

   // do easter
   Easter (dwMonth, dwYear, paMem);
}

/***********************************************************************
NewZealandHolidays

BUGFIX - User request
*/
void NewZealandHolidays (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   // holidays that move a bit
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      29, 1, gszAucklandAnniversaryDay);
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      30, 12, gszChathamIslands);
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      1, 2, gszNelson);
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      23, 3, gszOtago);
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      17, 1, gszSouthland);
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      31, 3, gszTaranaki); // except moved if conflicts with easter
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      22, 1, gszWellington);
   MovableHoliday (MOVE_NEARESTMON, dwMonth, dwYear, paMem,
      1, 12, gszWestland);
   MovableHoliday (MOVE_WEEKENDTOMONDAY, dwMonth, dwYear, paMem,
      1, 1, gszNewYearsDayCelebrated);
   MovableHoliday (MOVE_SATTOMON_SUNTOTUES, dwMonth, dwYear, paMem,
      2, 1, gszDayAfterNewYearsDay);
   MovableHoliday (MOVE_WEEKENDTOMONDAY, dwMonth, dwYear, paMem,
      25, 12, gszChristmasCelebrated);
   MovableHoliday (MOVE_SATTOMON_SUNTOTUES, dwMonth, dwYear, paMem,
      26, 12, gszBoxingDay);

   switch (dwMonth) {
   case 1:  // january
      Holiday (paMem, 1, gszNewYearsDay);
      break;
   case 2:  // february
      Holiday (paMem, 6, gszWaitangiDay);
      break;
   case 3:  // march
      switch (dwYear) {
      case 2001:
         Holiday (paMem, 12, gszTaranaki);  // Monday nearest 31 march, except if conflic w/easter
         break;
      case 2002:
         Holiday (paMem, 11, gszTaranaki);
         break;
      case 2003:
         Holiday (paMem, 10, gszTaranaki);
         break;
      case 2004:
         Holiday (paMem, 8, gszTaranaki);
         break;
      }
      break;
   case 4:  // april
      Holiday (paMem, 25, gszAnzacDay);
      break;
   case 5:  // may
      break;
   case 6:  // june
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FIRST, R_MONDAY), gszQueensBirthday);
      break;
   case 7:  // july
      break;
   case 8:  // august
      break;
   case 9:  // september
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FOURTH, R_MONDAY), gszCanteburySouth);
      break;
   case 10: // october
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_FOURTH, R_MONDAY), gszLaborDay);
      switch (dwYear) {
      case 2001:
         Holiday (paMem, 19, gszHawkesBay);  // friday before labor day
         Holiday (paMem, 29, gszMarlborough);  // 1st monday after labor day
         break;
      case 2002:
         Holiday (paMem, 25, gszHawkesBay);
         break;
      case 2003:
         Holiday (paMem, 24, gszHawkesBay);
         break;
      case 2004:
         Holiday (paMem, 22, gszHawkesBay);
         break;
      }
      break;
   case 11: // november
      switch (dwYear) {
      case 2001:
         Holiday (paMem, 9, gszCanteburyNorthCentral);  // 3rd firday after labor day
         break;
      case 2002:
         Holiday (paMem, 15, gszCanteburyNorthCentral);
         Holiday (paMem, 4, gszMarlborough);  // 1st monday after labor day
         break;
      case 2003:
         Holiday (paMem, 14, gszCanteburyNorthCentral);
         Holiday (paMem, 3, gszMarlborough);  // 1st monday after labor day
         break;
      case 2004:
         Holiday (paMem, 12, gszCanteburyNorthCentral);
         Holiday (paMem, 1, gszMarlborough);  // 1st monday after labor day
         break;
      }
      break;
   case 12: // december
      Holiday (paMem, 25, gszChristmas);
      break;
   }

   // do easter
   Easter (dwMonth, dwYear, paMem);

}

/***********************************************************************
ChristianHolidays
*/
void ChristianHolidays (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   switch (dwMonth) {
   case 1:  // january
      Holiday (paMem, 1, gszNewYearsDay);
      break;
   case 2:  // february
      break;
   case 3:  // march
      break;
   case 4:  // april
      break;
   case 5:  // may
      break;
   case 6:  // june
      break;
   case 7:  // july
      break;
   case 8:  // august
      break;
   case 9:  // september
      break;
   case 10: // october
      break;
   case 11: // november
      break;
   case 12: // december
      Holiday (paMem, 25,  gszChristmas);
      Holiday (paMem, 24, gszChristmasEve);
      Holiday (paMem, 31, gszNewYearsEve);
      break;
   }

   // do easter
   Easter (dwMonth, dwYear, paMem, TRUE);
}

/***********************************************************************
FrenchCountry holidays
BUGFIX - For user
*/
static WCHAR gszFrenchNewYear[] = L"le Jour de l'An";
static WCHAR gszFranceRois[] = L"la fête des Rois";
static WCHAR gszFranceChandeleur[] = L"la Chandeleur";
static WCHAR gszFranceValentine[] = L"la Saint - Valentin";
static WCHAR gszFrancePoisson[] = L"le poisson d'avril";
static WCHAR gszFranceMayDay[] = L"la fête du Travail";
static WCHAR gszFrance8May[] = L"le 8 mai";
static WCHAR gszFranceMeres[] = L"la fête des Mères";
static WCHAR gszFranceBastille[] = L"La Prise de la Bastille";
static WCHAR gszFranceVirg[] = L"l'Assomption de la Vierge";
static WCHAR gszFranceArmistice[] = L"l'Armistice";
static WCHAR gszFranceCatherine[] = L"la Sainte - Catherine";
static WCHAR gszFranceNoel[] = L"Noël";

void FranceHolidays (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   switch (dwMonth) {
   case 1:  // january
      Holiday (paMem, 1, gszFrenchNewYear);
      Holiday (paMem, 2, gszFranceRois);
      break;
   case 2:  // february
      Holiday (paMem, 2, gszFranceChandeleur);
      Holiday (paMem, 14, gszFranceValentine);
      break;
   case 3:  // march
      break;
   case 4:  // april
      Holiday (paMem, 1, gszFrancePoisson);
      break;
   case 5:  // may
      Holiday (paMem, 1, gszFranceMayDay);
      Holiday (paMem, 8, gszFrance8May);
      Holiday (paMem, ReoccurFindNth (dwMonth, dwYear, R_LAST, R_SUNDAY), gszFranceMeres);
      break;
   case 6:  // june
      break;
   case 7:  // july
      Holiday (paMem, 14, gszFranceBastille);
      break;
   case 8:  // august
      Holiday (paMem, 15, gszFranceVirg);
      break;
   case 9:  // september
      break;
   case 10: // october
      break;
   case 11: // november
      Holiday (paMem, 11, gszFranceArmistice);
      Holiday (paMem, 25, gszFranceCatherine);
      break;
   case 12: // december
      Holiday (paMem, 25, gszFranceNoel);
      break;
   }
   // do easter
   Easter (dwMonth, dwYear, paMem, TRUE);
}

static WCHAR gszVernalEquinox[] = L"Vernal Equinox (Sth. Hemisphere)";
static WCHAR gszAutumnalEquinox[] = L"Autumnal Equinox (Sth. Hemisphere)";
static WCHAR gszSummerSolstice[] = L"Summer Solstice (Sth. Hemisphere)";
static WCHAR gszWinterSolstice[] = L"Winter Solstice (Sth. Hemisphere)";

/***********************************************************************
SolsticeHolidays
*/
void SolsticeHolidays (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   switch (dwMonth) {
   case 1:  // january
      break;
   case 2:  // february
      break;
   case 3:  // march
      Holiday (paMem, 21, gszAutumnalEquinox);
      break;
   case 4:  // april
      break;
   case 5:  // may
      break;
   case 6:  // june
      Holiday (paMem, 21,  gszWinterSolstice);
      break;
   case 7:  // july
      break;
   case 8:  // august
      break;
   case 9:  // september
      Holiday (paMem, 23, gszVernalEquinox);
      break;
   case 10: // october
      break;
   case 11: // november
      break;
   case 12: // december
      Holiday (paMem, 22, gszSummerSolstice);
      break;
   }
}

/***********************************************************************
HolidaysFromMonth - Given a month, this fills in an any of an array of 32 CMem
with wide strings indicating the holiday name for the day, or
NULL if there is no holday.

inputs
   DWORD    dwMonth - 1..12
   PCMem    paMem - Pointer to 32 entries. 0 is ignored. 1..31 are used for the day
reutrns
   none
*/
void HolidaysFromMonth (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   // BUGFIX - Don't cleasr
   // memset (papsz, 0, sizeof(PWSTR) * 32);

   // BUGIFX - Also pull in events
   EventEnumMonth (dwMonth, dwYear, paMem);

   // figure out the holidays
   BOOL afHoliday[HOLIDAYGROUPS];
   LoadInHolidayGroups (afHoliday);

   if (afHoliday[GROUP_USA])
      USHolidays (dwMonth, dwYear, paMem);
   if (afHoliday[GROUP_CANADA])
      CanadianHolidays (dwMonth, dwYear, paMem);
   if (afHoliday[GROUP_AUSTRALIA])
      AustralianHolidays (dwMonth, dwYear, paMem);
   if (afHoliday[GROUP_CHRISTIAN])
      ChristianHolidays (dwMonth, dwYear, paMem);
   if (afHoliday[GROUP_SOLSTICE])
      SolsticeHolidays (dwMonth, dwYear, paMem);
   if (afHoliday[GROUP_NEWZEALAND])
      NewZealandHolidays (dwMonth, dwYear, paMem);
   if (afHoliday[GROUP_FRANCE])
      FranceHolidays (dwMonth, dwYear, paMem);
}



/***********************************************************************
CalendarListPage - Page callback for viewing a phone message
*/
BOOL CalendarListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszViewNext);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) gdwListCB);
      }
      break;   // go to default handler

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         // if its a reminder link delete and refresh
         BOOL fRefresh;
         DWORD dwNext;
         if (ReminderParseLink (p->psz, pPage)) {
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (WorkParseLink (p->psz, pPage, &fRefresh, FALSE)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (ProjectParseLink (p->psz, pPage, &fRefresh, FALSE)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (SchedParseLink (p->psz, pPage, &fRefresh, &dwNext)) {
            // if user has specified to enter a meeting then do so now
            if (dwNext) {
               WCHAR szTemp[16];
               swprintf (szTemp, gszED, dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (PhoneParseLink (p->psz, pPage, &fRefresh, &dwNext)) {
            // if user has specified to enter a Call then do so now
            if (dwNext) {
               WCHAR szTemp[16];
               swprintf (szTemp, gszED, dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh)
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

         if (!_wcsicmp(p->pControl->m_pszName, L"addyesterday")) {
            DWORD dwNode;
            dwNode = WrapUpCreateOrGet (MinutesToDFDATE(DFDATEToMinutes(Today())-24*60), TRUE);
            if (dwNode == (DWORD)-1)
               break;

            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         // if the date changes then refresh the page
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (_wcsicmp(p->pControl->m_pszName, gszViewNext))
            break;   // wrong one

         gdwListCB = (DWORD) p->dwCurSel;
         pPage->Exit (gszRedoSamePage);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"MONTH")) {
            MemZero (&gMemTemp);

            DFDATE today;
            today = Today();
            DWORD dwMonth;
            DWORD dwDaysLeft, dwDay, dwDaysUsed;
            dwDaysLeft = gdwListCB ? 28 : 7;
            dwDaysUsed = 0;
            dwDay = DAYFROMDFDATE(today);

            // BUGFIX - Did some rearranging to avoid internal compiler error
            PCMem    apmemReminders[32], apmemProjects[32], apmemTasks[32], apmemMeetings[32], apmemCalls[32];
            CMem     aMemHoliday[32];
            // loop over two months becuase may be looking over that time
            for (dwMonth = 0; dwMonth < 2; dwMonth++) {
               DWORD dwThisMonth, dwThisYear;
               dwThisYear = YEARFROMDFDATE(today);
               dwThisMonth = MONTHFROMDFDATE(today) + dwMonth;
               if (dwThisMonth > 12) {
                  dwThisMonth -= 12;
                  dwThisYear++;
               }
               DFDATE thismonth;
               thismonth = TODFDATE(1, dwThisMonth, dwThisYear);
               int iDays, iStartDOW;
               iDays = EscDaysInMonth (dwThisMonth, dwThisYear, &iStartDOW);
               if (!iDays) {
                  iDays = 31;
                  iStartDOW = 0;
               }

               // determine what links should have for reminders, task, etc.
               memset (apmemReminders, 0, sizeof(apmemReminders));
               memset (apmemProjects, 0, sizeof(apmemProjects));
               memset (apmemTasks, 0, sizeof(apmemTasks));
               memset (apmemMeetings, 0, sizeof(apmemMeetings));
               memset (apmemCalls, 0, sizeof(apmemCalls));
               ReminderMonthEnumerate (thismonth, apmemReminders, today, !gfPrinting);
               SchedMonthEnumerate (thismonth, apmemMeetings, !gfPrinting);
               PhoneMonthEnumerate (thismonth, apmemCalls, !gfPrinting);
               WorkMonthEnumerate (thismonth, apmemTasks, today, !gfPrinting);
               ProjectMonthEnumerate (thismonth, apmemProjects, !gfPrinting);
               HolidaysFromMonth (dwThisMonth, dwThisYear, aMemHoliday);

               // loop
               while (dwDaysLeft) {
                  // if gone beyond the end of the month go to the next one
                  if (dwDay > (DWORD) iDays) {
                     dwDay = 1;
                     break;
                  }

                  // header
                  MemCat (&gMemTemp, L"<xtablecenter innerlines=0>");
                  WCHAR szTemp[64];
                  DFDATEToString (TODFDATE(dwDay, dwThisMonth, dwThisYear), szTemp);
                  if (gfPrinting) {
                     MemCat (&gMemTemp, L"<xtrheader align=right>");
                     MemCat (&gMemTemp, szTemp);
                     MemCat (&gMemTemp, L"<br/>");
                  }
                  else {
                     MemCat (&gMemTemp, L"<xtrheader align=left><align align=right>");
                     MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddmeeting");
                     MemCat (&gMemTemp, (int) TODFDATE(dwDay, dwThisMonth, dwThisYear));
                     MemCat (&gMemTemp, L"><small><font color=#8080ff>Add meeting</font></small></button>");
                     MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddcall");
                     MemCat (&gMemTemp, (int) TODFDATE(dwDay, dwThisMonth, dwThisYear));
                     MemCat (&gMemTemp, L"><small><font color=#8080ff>Schedule call</font></small></button>");
                     MemCat (&gMemTemp, szTemp);
                     MemCat (&gMemTemp, L"<br/>");
                     MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddreminder");
                     MemCat (&gMemTemp, (int) TODFDATE(dwDay, dwThisMonth, dwThisYear));
                     MemCat (&gMemTemp, L"><small><font color=#8080ff>Add reminder</font></small></button>");
                     MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddtask");
                     MemCat (&gMemTemp, (int) TODFDATE(dwDay, dwThisMonth, dwThisYear));
                     MemCat (&gMemTemp, L"><small><font color=#8080ff>Add task</font></small></button>");
                  }

                  // if it's a holiday indicate that
                  if (aMemHoliday[dwDay].p) {
                     MemCat (&gMemTemp, L"<italic><small> (");
                     MemCatSanitize (&gMemTemp, (PWSTR) aMemHoliday[dwDay].p);
                     MemCat (&gMemTemp, L" )</small></italic>");
                  }

                  if (gfPrinting)
                     MemCat (&gMemTemp, L"</xtrheader><tr><td>");
                  else
                     MemCat (&gMemTemp, L"</align></xtrheader><tr><td>");

                  DWORD dwShown;
                  dwShown = 0;

                  if (apmemMeetings[dwDay]) {
                     MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Meetings</bold></p><xul>");
                     MemCat (&gMemTemp, (PWSTR) apmemMeetings[dwDay]->p);
                     MemCat (&gMemTemp, L"</xul></align>");
                     dwShown++;
                  }
                  if (apmemCalls[dwDay]) {
                     MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Scheduled calls</bold></p><xul>");
                     MemCat (&gMemTemp, (PWSTR) apmemCalls[dwDay]->p);
                     MemCat (&gMemTemp, L"</xul></align>");
                     dwShown++;
                  }
                  if (apmemReminders[dwDay]) {
                     if (dwShown)
                        MemCat (&gMemTemp, L"<br/>");

                     MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Reminders</bold></p><xul>");
                     MemCat (&gMemTemp, (PWSTR) apmemReminders[dwDay]->p);
                     MemCat (&gMemTemp, L"</xul></align>");
                     dwShown++;
                  }
                  if (apmemTasks[dwDay]) {
                     if (dwShown)
                        MemCat (&gMemTemp, L"<br/>");

                     MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Tasks</bold></p><xul>");
                     MemCat (&gMemTemp, (PWSTR) apmemTasks[dwDay]->p);
                     MemCat (&gMemTemp, L"</xul></align>");
                     dwShown++;
                  }

                  // BUGFIX - Did some rearranging to avoid internal compiler error
                  if (apmemProjects[dwDay] && dwShown)
                        MemCat (&gMemTemp, L"<br/>");

                  if (apmemProjects[dwDay]) {
                     MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Projects</bold></p><xul>");
                     MemCat (&gMemTemp, (PWSTR) apmemProjects[dwDay]->p);
                     MemCat (&gMemTemp, L"</xul></align>");
                     dwShown++;
                  }

                  if (!dwShown)
                     MemCat (&gMemTemp, L"Nothing scheduled for the day.");

                  // finished up the day
                  MemCat (&gMemTemp, L"</td></tr></xtablecenter>");
                  dwDaysLeft--;
                  dwDaysUsed++;
                  dwDay++;

                  // BUGFIX - Add the "show next 28 days" at the end.
                  if (dwDaysUsed == 7)
                     MemCat (&gMemTemp,
                        L"<xbr/><p align=right><bold>"
                        L"<combobox name=viewnext width=50%% cbheight=75 cursel=0>"
                        L"<elem name=7>Don't show anymore</elem>"
                        L"<elem name=28>Show another 21 days</elem>"
                        L"</combobox>"
                        L"</bold></p>"
                     );
               }

#if 0
               // starting days 3 back
               int   iDOW, iDay;
               iDay = 1 - iStartDOW;

               for (; iDay <= iDays; ) {
                  // loop by the week
                  MemCat (&gMemTemp, L"<tr>");

                  // do the week
                  for (iDOW = 0; iDOW < 7; iDOW++, iDay++) {
                     if ((iDay < 1) || (iDay > iDays)) {
                        // blank day
                        MemCat (&gMemTemp, L"<xdaynull/>");
                        continue;
                     }

                     PWSTR pszHoliday;
                     pszHoliday = (PWSTR) aMemHoliday[iDay].p;
                     // else it has a number

                     // if it's today
                     if ( (MONTHFROMDFDATE(thismonth) == MONTHFROMDFDATE(today)) &&
                        (YEARFROMDFDATE(thismonth) == YEARFROMDFDATE(today)) &&
                        ((int) DAYFROMDFDATE(today) == iDay))
                           MemCat (&gMemTemp, L"<xday bgcolor=#ffff40>");
                     else {
                        if (pszHoliday)
                           MemCat (&gMemTemp, L"<xday bgcolor=#80ffc0>");
                        else if ((iDOW == 0) || (iDOW == 6))
                           MemCat (&gMemTemp, L"<xday bgcolor=#b0b0ef>");
                        else
                           MemCat (&gMemTemp, L"<xday>");
                     }

                     // see if there's a daily log, if so create a link
                     PCMMLNode pDaily;
                     DWORD dwDaily;
                     pDaily = GetCalendarLogNode (
                        TODFDATE(iDay,MONTHFROMDFDATE(thismonth),YEARFROMDFDATE(thismonth)),
                        FALSE, &dwDaily);
                     if (pDaily)
                        gpData->NodeRelease(pDaily);


                     // show the number
                     MemCat (&gMemTemp, L"<big><big><big><bold>");
                     if (pDaily) {
                        MemCat (&gMemTemp, L"<a href=v:");
                        MemCat (&gMemTemp, (int) dwDaily);
                        MemCat (&gMemTemp, L">");
                        MemCat (&gMemTemp, L"<xhoverhelp>Click this to see a log showing what you did that day.</xhoverhelp>");
                     }
                     MemCat (&gMemTemp, iDay);
                     if (pDaily) {
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</bold></big></big></big>");

                     // if there's a holiday show that
                     if (pszHoliday) {
                        MemCat (&gMemTemp, L"<br/><big><bold>");
                        MemCatSanitize (&gMemTemp, pszHoliday);
                        MemCat (&gMemTemp, L"</bold></big>");
                     }

                     // see if there's any meetings, etc. today. If so show
                     // PCMem    apmemReminders[31], apmemProjects[31], apmemTasks[31], apmemMeetings[31];

                     MemCat (&gMemTemp, L"</xday>");
                  }

                  MemCat (&gMemTemp, L"</tr>");


               }
#endif // 0

               // free up memory for reminders, meetings, etc.
               DWORD i;
               for (i = 0; i < 32; i++) {
                  if (apmemReminders[i])
                     delete apmemReminders[i];
                  if (apmemProjects[i])
                     delete apmemProjects[i];
                  if (apmemTasks[i])
                     delete apmemTasks[i];
                  if (apmemMeetings[i])
                     delete apmemMeetings[i];
                  if (apmemCalls[i])
                     delete apmemCalls[i];
               }

               if (!dwDaysLeft)
                  break;

            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WRAPUP")) {
            // if wrapped up yesterday, or didn't have any journal entries (which
            // implies didn't use the machine and was on vacation) then daily
            // wrap up
            DWORD dwWrap;
            DFDATE yesterday;
            // BUGFIX - Allow user to turn off bugging about wrapup
            if (!gfBugAboutWrap)
               return TRUE;
            yesterday = MinutesToDFDATE(DFDATEToMinutes(Today())-24*60);
            dwWrap = WrapUpCreateOrGet(yesterday, FALSE);
            if (dwWrap != (DWORD) -1)
               return TRUE;   // leave blank
            PCMMLNode pYesterday;
            pYesterday = GetCalendarLogNode (yesterday, FALSE, &dwWrap);
            if (!pYesterday)
               return TRUE;   // leave blank
            
            gpData->NodeRelease (pYesterday);

            // else, didn't do daily wrapup even though should have
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<p><bold>");
            MemCat (&gMemTemp, L"You forgot to do a \"Daily wrap-up\" yesterday. ");
            MemCat (&gMemTemp, L"Before you get too busy today, please fill in yesterday's daily wrap-up.");
            MemCat (&gMemTemp, L"</bold></p>");
            MemCat (&gMemTemp, L"<xChoiceButton name=addyesterday><bold>Do yesterday's daily wrapup</bold>");
            MemCat (&gMemTemp, L"<br/>This will allow you to write up what you did yesterday.</xChoiceButton>");
            MemCat (&gMemTemp, L"<xbr/>");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DEEPTHOUGHT")) {
            // show projects
            p->pszSubString = DeepThoughtGenerate ();

            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
CalendarLogSub - Given a date, this loads in the calendar log information
for that day and returns a PWSTR. This is a substitution string for displaying
in a window (such as the daily wrap-up). It uses gMemTemp.

inputs
   DFDATE      date - date
   BOOL        fCat - if TRUE then don't zero gMemTemp, but concatenate on - BUGFIX
   BOOL        *pfFound - Filled with TRUE if found data, FALSE if none
returns
   PWSTR - Substitution string
*/
PWSTR CalendarLogSub (DFDATE date, BOOL fCat, BOOL *pfFound)
{
   HANGFUNCIN;
   if (pfFound)
      *pfFound = FALSE;

   // zero go
   if (!fCat)
      MemZero (&gMemTemp);

   // get the day
   PCMMLNode pNode;
   DWORD dwDaily;
   pNode = GetCalendarLogNode (date, FALSE, &dwDaily);
   if (!pNode) {
      MemCat (&gMemTemp, L"Nothing logged for the day.");
      return (PWSTR) gMemTemp.p;
   }

   BOOL fDisplayed = FALSE;
   PCListFixed pl = NULL;
   pl = NodeListGet (pNode, gszLogEntry, TRUE);

   DWORD i;
   NLG *pnlg;
   if (pl) for (i = 0; i < pl->Num(); i++) {
      pnlg = (NLG*) pl->Get(i);
   
      if (pfFound)
         *pfFound = TRUE;

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
      // dont bother with date since already known
      // DFDATEToString (pnlg->date, szTemp);
      // MemCat (&gMemTemp, szTemp);
      // MemCat (&gMemTemp, L"<br/>");
      DFTIMEToString (pnlg->start, szTemp);
      MemCat (&gMemTemp, szTemp);
      if (pnlg->end != -1) {
         DFTIMEToString (pnlg->end, szTemp);
         MemCat (&gMemTemp, L" to ");
         MemCat (&gMemTemp, szTemp);
      }
      
      MemCat (&gMemTemp, L"</xtdcompleted></tr>");
   }

   // if no entries then say so
   if (!pl || !pl->Num())
      MemCat (&gMemTemp, L"<tr><td>None</td></tr>");

   if (pl)
      delete pl;
   if (pNode)
      gpData->NodeRelease (pNode);

   return (PWSTR) gMemTemp.p;
}


/***********************************************************************
CalendarComboPage - Page callback for viewing a phone message

BUGFIX - Added this at user requests for more info crammed in.
*/
BOOL CalendarComboPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the dropdown to the right date
         DateControlSet (pPage, gszDate, gdateCalendarCombo);
      }
      break;   // go to default handler


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         // if its a reminder link delete and refresh
         DWORD dwNext;
         BOOL fRefresh2, fRefresh = FALSE;
         if (ReminderParseLink (p->psz, pPage)) {
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (WorkParseLink (p->psz, pPage, &fRefresh, FALSE)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (ProjectParseLink (p->psz, pPage, &fRefresh, FALSE)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (PhoneParseLink (p->psz, pPage, &fRefresh, &dwNext)) {
            // if user has specified to enter a Call then do so now
            if (dwNext) {
               WCHAR szTemp[16];
               swprintf (szTemp, gszED, dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh) {
               pPage->Exit (gszRedoSamePage);
               return TRUE;
            }
            return TRUE;
         }

         else if ((p->psz[0] == L'c') && (p->psz[1] == L'd') && (p->psz[2] == L':')) {
            // pressed the day button
            gdateCalendarCombo = TODFDATE(
               _wtoi(p->psz+3),
               MONTHFROMDFDATE(gdateCalendarCombo),
               YEARFROMDFDATE(gdateCalendarCombo)
               );
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (SchedParseLink (p->psz, pPage, &fRefresh2, &dwNext)) {
            fRefresh = fRefresh2;
            // if user has specified to enter a meeting then do so now
            if (dwNext) {
               WCHAR szTemp[16];
               swprintf (szTemp, gszED, dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;

   case ESCN_DATECHANGE:
      {
         // if the date changes then refresh the page
         PESCNDATECHANGE p = (PESCNDATECHANGE) pParam;

         if (_wcsicmp(p->pControl->m_pszName, gszDate))
            break;   // wrong one

         // make sure to use a day within the month's 1..31 days, etc.
         int   iDay;
         iDay = (int) DAYFROMDFDATE(gdateCalendarCombo);
         iDay = max(1,iDay);
         int iDays, iStartDOW;
         iDays = EscDaysInMonth (p->iMonth, p->iYear, &iStartDOW);
         iDay = min(iDays, iDay);
         // BUGFIX - if change to no date, use today
         if ((iDay >= 1) && (p->iMonth >= 1) && (p->iYear >= 1))
            gdateCalendarCombo = TODFDATE (1, p->iMonth, p->iYear);
         else
            gdateCalendarCombo = Today();
         pPage->Exit (gszRedoSamePage);
      }
      return TRUE;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"TODAYSDATE")) {
            DFDATE   today, tomorrow;
            WCHAR szTemp[64];
            today = Today();
            tomorrow = MinutesToDFDATE(DFDATEToMinutes(today)+60*24);
            DFDATEToString (gdateCalendarCombo, szTemp);
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);

            // if it's today then note that
            if (today == gdateCalendarCombo)
               MemCat (&gMemTemp, L"<italic> (Today)</italic>");
            if (tomorrow == gdateCalendarCombo)
               MemCat (&gMemTemp, L"<italic> (Tomorrow)</italic>");

            // holiday
            CMem    aMemHoliday[32];
            HolidaysFromMonth (MONTHFROMDFDATE(gdateCalendarCombo), YEARFROMDFDATE(gdateCalendarCombo), aMemHoliday);
            PWSTR pszHoliday;
            pszHoliday = (PWSTR) aMemHoliday[DAYFROMDFDATE(gdateCalendarCombo)].p;
            if (pszHoliday) {
               MemCat (&gMemTemp, L"<br/>");
               MemCatSanitize (&gMemTemp, pszHoliday);
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MONTH")) {
            MemZero (&gMemTemp);

            DFDATE today;
            today = Today();
            // how many days in the month
            if (gdateCalendarCombo == -1)
               gdateCalendarCombo = today;
            int iDays, iStartDOW;
            iDays = EscDaysInMonth (MONTHFROMDFDATE(gdateCalendarCombo),
               YEARFROMDFDATE(gdateCalendarCombo), &iStartDOW);
            if (!iDays) {
               iDays = 31;
               iStartDOW = 0;
            }

            // determine what links should have for reminders, task, etc.
            PCMem    apmemReminders[32], apmemProjects[32], apmemTasks[32], apmemMeetings[32], apmemCalls[32];
            CMem     aMemHoliday[32];
            memset (apmemReminders, 0, sizeof(apmemReminders));
            memset (apmemProjects, 0, sizeof(apmemProjects));
            memset (apmemTasks, 0, sizeof(apmemTasks));
            memset (apmemMeetings, 0, sizeof(apmemMeetings));
            memset (apmemCalls, 0, sizeof(apmemCalls));
            // BUGFIX - Was gdateCalendar instead of gdateCalendarCombo
            ReminderMonthEnumerate (gdateCalendarCombo, apmemReminders);
            SchedMonthEnumerate (gdateCalendarCombo, apmemMeetings);
            PhoneMonthEnumerate (gdateCalendarCombo, apmemCalls);
            WorkMonthEnumerate (gdateCalendarCombo, apmemTasks);
            ProjectMonthEnumerate (gdateCalendarCombo, apmemProjects);
            HolidaysFromMonth (MONTHFROMDFDATE(gdateCalendarCombo), YEARFROMDFDATE(gdateCalendarCombo), aMemHoliday);

            // starting days 3 back
            int   iDOW, iDay;
            iDay = 1 - iStartDOW;

            for (; iDay <= iDays; ) {
               // loop by the week
               MemCat (&gMemTemp, L"<tr>");

               // do the week
               for (iDOW = 0; iDOW < 7; iDOW++, iDay++) {
                  if ((iDay < 1) || (iDay > iDays)) {
                     // blank day
                     MemCat (&gMemTemp, L"<xdaynull/>");
                     continue;
                  }

                  PWSTR pszHoliday;
                  pszHoliday = (PWSTR) aMemHoliday[iDay].p;
                  // else it has a number

                  // if it's today
                  if ( (MONTHFROMDFDATE(gdateCalendarCombo) == MONTHFROMDFDATE(today)) &&
                     (YEARFROMDFDATE(gdateCalendarCombo) == YEARFROMDFDATE(today)) &&
                     ((int) DAYFROMDFDATE(today) == iDay))
                        MemCat (&gMemTemp, L"<xday bgcolor=#ffff40>");
                  else {
                     if (pszHoliday)
                        MemCat (&gMemTemp, L"<xday bgcolor=#80ffc0>");
                     else if ((iDOW == 0) || (iDOW == 6))
                        MemCat (&gMemTemp, L"<xday bgcolor=#b0b0ef>");
                     else
                        MemCat (&gMemTemp, L"<xday>");
                  }


                  // show the number
                  if (!gfPrinting) {
                     MemCat (&gMemTemp, L"<a href=cd:");
                     MemCat (&gMemTemp, iDay);
                     MemCat (&gMemTemp, L">");
                  }
                  if (DAYFROMDFDATE(gdateCalendarCombo) == (DWORD) iDay)
                     MemCat (&gMemTemp, L"<bold><big>");
                  MemCat (&gMemTemp, iDay);
                  if (DAYFROMDFDATE(gdateCalendarCombo) == (DWORD) iDay)
                     MemCat (&gMemTemp, L"</big></bold>");

                  if (!gfPrinting) {
                     // show list of stuff for the day
                     MemCat (&gMemTemp, L"<xhoverhelp>");
                     DWORD dwShown;
                     dwShown = 0;

                     // BUGFIX - Show holidays in tooltip
                     if (pszHoliday) {
                        MemCat (&gMemTemp, L"<br><bold>");
                        MemCatSanitize (&gMemTemp, pszHoliday);
                        MemCat (&gMemTemp, L"</bold></br>");
                        dwShown++;
                     }

                  
                     if (apmemMeetings[iDay]) {
                        MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Meetings</bold></p><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemMeetings[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align>");
                        dwShown++;
                     }
                     if (apmemCalls[iDay]) {
                        MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Scheduled calls</bold></p><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemCalls[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align>");
                        dwShown++;
                     }
                     if (apmemReminders[iDay]) {
                        if (dwShown)
                           MemCat (&gMemTemp, L"<br/>");

                        MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Reminders</bold></p><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemReminders[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align>");
                        dwShown++;
                     }
                     if (apmemTasks[iDay]) {
                        if (dwShown)
                           MemCat (&gMemTemp, L"<br/>");

                        MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Tasks</bold></p><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemTasks[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align>");
                        dwShown++;
                     }
                     if (apmemProjects[iDay]) {
                        if (dwShown)
                           MemCat (&gMemTemp, L"<br/>");

                        MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Projects</bold></p><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemProjects[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align>");
                        dwShown++;
                     }

                     if (!dwShown)
                        MemCat (&gMemTemp, L"Nothing scheduled for the day.");

                     // BUGFIX - day of the week
                     DWORD dwDayOfYear, dwDaysInYear, dwWeekOfYear;
                     DateToDayOfYear (TODFDATE(iDay, MONTHFROMDFDATE(gdateCalendarCombo), YEARFROMDFDATE(gdateCalendarCombo)),
                        &dwDayOfYear, &dwDaysInYear, &dwWeekOfYear);
                     MemCat (&gMemTemp, L"<br/><italic>Week ");
                     MemCat (&gMemTemp, dwWeekOfYear);
                     MemCat (&gMemTemp, L", Day ");
                     MemCat (&gMemTemp, (int) dwDayOfYear);
                     MemCat (&gMemTemp, L", ");
                     MemCat (&gMemTemp, (int) dwDaysInYear - dwDayOfYear);
                     MemCat (&gMemTemp, L" days left</italic>");


                     MemCat (&gMemTemp, L"</xhoverhelp>");

                     MemCat (&gMemTemp, L"</a>");
                  }  // gfPrinting


                  MemCat (&gMemTemp, L"</xday>");
               }

               MemCat (&gMemTemp, L"</tr>");
            }

               // free up memory for reminders, meetings, etc.
               DWORD i;
               for (i = 0; i < 32; i++) {
                  if (apmemReminders[i])
                     delete apmemReminders[i];
                  if (apmemProjects[i])
                     delete apmemProjects[i];
                  if (apmemTasks[i])
                     delete apmemTasks[i];
                  if (apmemMeetings[i])
                     delete apmemMeetings[i];
                  if (apmemCalls[i])
                     delete apmemCalls[i];
               }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MEETINGSUMMARY")) {
            // show projects
            p->pszSubString = PlannerSummaryScale(gdateCalendarCombo, TRUE);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DEEPTHOUGHT")) {
            // show projects
            p->pszSubString = DeepThoughtGenerate ();

            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WRAPUP")) {
            // if wrapped up yesterday, or didn't have any journal entries (which
            // implies didn't use the machine and was on vacation) then daily
            // wrap up
            DWORD dwWrap;
            DFDATE yesterday;
            // BUGFIX - Allow user to turn off bugging about wrapup
            if (!gfBugAboutWrap)
               return TRUE;
            yesterday = MinutesToDFDATE(DFDATEToMinutes(Today())-24*60);
            dwWrap = WrapUpCreateOrGet(yesterday, FALSE);
            if (dwWrap != (DWORD) -1)
               return TRUE;   // leave blank
            PCMMLNode pYesterday;
            pYesterday = GetCalendarLogNode (yesterday, FALSE, &dwWrap);
            if (!pYesterday)
               return TRUE;   // leave blank
            
            if (pYesterday)
               gpData->NodeRelease (pYesterday);

            // else, didn't do daily wrapup even though should have
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<p><bold>");
            MemCat (&gMemTemp, L"You forgot to do a \"Daily wrap-up\" yesterday. ");
            MemCat (&gMemTemp, L"Before you get too busy today, please fill in yesterday's daily wrap-up.");
            MemCat (&gMemTemp, L"</bold></p>");
            MemCat (&gMemTemp, L"<xChoiceButton name=addyesterday><bold>Do yesterday's daily wrapup</bold>");
            MemCat (&gMemTemp, L"<br/>This will allow you to write up what you did yesterday.</xChoiceButton>");
            MemCat (&gMemTemp, L"<xbr/>");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PROJECTSUMMARY")) {
            // show projects
            if (gdateCalendarCombo >= Today())
               p->pszSubString = ProjectSummary(gdateCalendarCombo, gdateCalendarCombo); // BUGBUG
            else
               return FALSE;  // leave empty
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"REMINDERSUMMARY")) {
            // show projects
            p->pszSubString = RemindersSummary(gdateCalendarCombo,
               (gdateCalendarCombo <= Today()) ? 0 : gdateCalendarCombo, TRUE);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORKSUMMARY")) {
            // show projects
            p->pszSubString = WorkSummary(FALSE, gdateCalendarCombo,
               (gdateCalendarCombo <= Today()) ? 0 : gdateCalendarCombo, TRUE);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"CALENDARLOG")) {
            // show log of activites for the day
            BOOL fFound;
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<xbr/><xlisttable><xtrheader>Completed activities</xtrheader>");
            CalendarLogSub (gdateCalendarCombo, TRUE, &fFound);
            MemCat (&gMemTemp, L"</xlisttable>");
            p->pszSubString = fFound ? (PWSTR) gMemTemp.p : L"";
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

         if (!_wcsicmp(p->pControl->m_pszName, L"addyesterday")) {
            // BUGFIX - If didn't enter yesterday's draily wrap up, would bug and
            // show button, but button wouldn't work
            DWORD dwNode;
            dwNode = WrapUpCreateOrGet (MinutesToDFDATE(DFDATEToMinutes(Today())-24*60), TRUE);
            if (dwNode == (DWORD)-1)
               break;

            WCHAR szTemp[16];
            swprintf (szTemp, gszED, (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
CalendarSummaryPage - Page callback for viewing a phone message
*/
BOOL CalendarSummaryPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszViewNext);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) gdwListSummaryCB);
      }
      break;   // go to default handler

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         // if its a reminder link delete and refresh
         BOOL fRefresh;
         DWORD dwNext;
         if (ReminderParseLink (p->psz, pPage)) {
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (WorkParseLink (p->psz, pPage, &fRefresh, FALSE)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (ProjectParseLink (p->psz, pPage, &fRefresh, FALSE)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (SchedParseLink (p->psz, pPage, &fRefresh, &dwNext)) {
            // if user has specified to enter a meeting then do so now
            if (dwNext) {
               WCHAR szTemp[16];
               swprintf (szTemp, gszED, dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (PhoneParseLink (p->psz, pPage, &fRefresh, &dwNext)) {
            // if user has specified to enter a Call then do so now
            if (dwNext) {
               WCHAR szTemp[16];
               swprintf (szTemp, gszED, dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         // if the date changes then refresh the page
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (_wcsicmp(p->pControl->m_pszName, gszViewNext))
            break;   // wrong one

         gdwListSummaryCB = (DWORD) p->dwCurSel;
         pPage->Exit (gszRedoSamePage);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"MONTH")) {
            MemZero (&gMemTemp);

            __int64 iNow = DFDATEToMinutes(Today());
            DWORD dwDaysLeft;
            dwDaysLeft = gdwListSummaryCB ? 28 : 7;
            iNow -= (((int)dwDaysLeft-1) * 60 * 24);

            for (; dwDaysLeft; dwDaysLeft--, iNow += 60*24) {
               DFDATE day = MinutesToDFDATE (iNow);

               BOOL fFound;
               MemCat (&gMemTemp, L"<xlisttable><xtrheader>");
               WCHAR szTemp[64];
               DFDATEToString (day, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</xtrheader>");
               CalendarLogSub (day, TRUE, &fFound);
               if (!fFound)
                  MemCat (&gMemTemp, L"<tr><td>No activities recorded.</td></tr>");
               MemCat (&gMemTemp, L"</xlisttable>");
            }


            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
CalendarYearlyPage - Page callback for viewing a phone message

BUGFIX - User request for a yearly calendar.
*/
BOOL CalendarYearlyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   static DWORD sdwClickMeaning = 0;   // 0=>go to, 1=>days from today, 2=>first date, 3=>second date
   static DFDATE sDateFirst;  // first date

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the dropdown to the right date
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszDate);
         if (pControl)
            pControl->AttribSetInt (gszText, YEARFROMDFDATE(gdateCalendarYearly));

         // BUGFIX - Set the botton for rest of year
         pControl = pPage->ControlFind (L"restofyear");
         if (pControl) {
            pControl->AttribSetBOOL (gszChecked, gfOnlyShowRestOfYear);

            // disable if its not this year
            if (YEARFROMDFDATE(gdateCalendarYearly) != YEARFROMDFDATE(Today()))
               pControl->Enable(FALSE);
         }
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
            gdateCalendarYearly = TODFDATE(1,1,iYear);
            pPage->Exit(gszRedoSamePage);
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         // if its a reminder link delete and refresh
         if ((p->psz[0] == L'c') && (p->psz[1] == L'd') && (p->psz[2] == L':')) {
            DFDATE d = (DFDATE)_wtoi(p->psz+3);

            switch (sdwClickMeaning) {
            default:
               // pressed the day button
               gdateCalendarCombo = d;
               pPage->Exit (L"r:258");
               break;

            case 1:
               {
                  sdwClickMeaning = 0;
                  WCHAR szTemp[256];
                  if (d >= sDateFirst)
                     swprintf (szTemp, L"%d days from now.",
                     (int) ( (DFDATEToMinutes(d) - DFDATEToMinutes(sDateFirst)) / 60 / 24) );
                  else
                     swprintf (szTemp, L"%d days before today.",
                     (int) ( (DFDATEToMinutes(sDateFirst) - DFDATEToMinutes(d)) / 60 / 24) );
                  pPage->MBSpeakInformation (szTemp);
               }
               break;

            case 2:
               sDateFirst = d;
               sdwClickMeaning = 3;
               pPage->MBSpeakInformation (L"Click on the second date.");
               break;

            case 3:
               sdwClickMeaning = 0;
               WCHAR szTemp[256];
               swprintf (szTemp, L"%d days.",
                  (int) ( (DFDATEToMinutes(max(d,sDateFirst)) - DFDATEToMinutes(min(d,sDateFirst))) / 60 / 24) );
               pPage->MBSpeakInformation (szTemp);
               break;
            }
            return TRUE;
         }
         else if ((p->psz[0] == L'c') && (p->psz[1] == L'm') && (p->psz[2] == L':')) {
            // pressed the day button
            gdateCalendar = (DFDATE)_wtoi(p->psz+3);
            pPage->Exit (L"r:112");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"MONTH")) {
            MemZero (&gMemTemp);

            DFDATE today;
            today = Today();
            // how many days in the month
            if (gdateCalendarYearly == -1)
               gdateCalendarYearly = today;

            int iYear, iMonth;
            iYear = YEARFROMDFDATE (gdateCalendarYearly);

            // show in two columns
            MemCat (&gMemTemp, L"<table width=100% border=0 innerlines=0>");

            int iStart;
            iStart = 1;
            BOOL fOnlyShow;
            fOnlyShow = gfOnlyShowRestOfYear && (iYear == (int) YEARFROMDFDATE(today));

            // BUGFIX - If only show this month to the rest of the year then skip
            if (fOnlyShow) {
               iStart = MONTHFROMDFDATE(today);

               // since show two months in column, round down
               if (!(iStart % 2))
                  iStart--;
            }

            for (iMonth = iStart; iMonth <= 12; iMonth++) {
               // show in two columns
               if (iMonth % 2)
                  MemCat (&gMemTemp, L"<tr>");
               MemCat (&gMemTemp, L"<td>");

               if (!fOnlyShow || (iMonth >= (int) MONTHFROMDFDATE(today))) {
                  MemCat (&gMemTemp, L"<table width=100%% lrmargin=1 tbmargin=3 align=center valign=center>");
                  if (gfFullColor)
                     MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#ffffe0 bcolor=#d0d0a0/>");
                  MemCat (&gMemTemp, L"<xtrheader><a color=#c0c0ff href=cm:");
                  MemCat (&gMemTemp, (int) TODFDATE(1,iMonth,iYear));
                  MemCat (&gMemTemp, L">");
                  MemCat (&gMemTemp, gpszMonth[iMonth-1]);
                  MemCat (&gMemTemp, L"</a>");
                  MemCat (&gMemTemp, L"</xtrheader>");
                  MemCat (&gMemTemp, L"<tr><xDayOfWeek>S</xDayOfWeek><xDayOfWeek>M</xDayOfWeek><xDayOfWeek>T</xDayOfWeek><xDayOfWeek>W</xDayOfWeek><xDayOfWeek>Th</xDayOfWeek><xDayOfWeek>F</xDayOfWeek><xDayOfWeek>S</xDayOfWeek></tr>");

                  int iDays, iStartDOW;
                  iDays = EscDaysInMonth (iMonth,
                     iYear, &iStartDOW);
                  if (!iDays) {
                     iDays = 31;
                     iStartDOW = 0;
                  }

                  // determine what links should have for reminders, task, etc.
                  PCMem    apmemReminders[32], apmemProjects[32], apmemTasks[32], apmemMeetings[32], apmemCalls[32];
                  CMem     aMemHoliday[32];
                  memset (apmemReminders, 0, sizeof(apmemReminders));
                  memset (apmemProjects, 0, sizeof(apmemProjects));
                  memset (apmemTasks, 0, sizeof(apmemTasks));
                  memset (apmemMeetings, 0, sizeof(apmemMeetings));
                  memset (apmemCalls, 0, sizeof(apmemCalls));
                  DFDATE d;
                  d = TODFDATE(1, iMonth, iYear);
                  ReminderMonthEnumerate (d, apmemReminders);
                  SchedMonthEnumerate (d, apmemMeetings);
                  PhoneMonthEnumerate (d, apmemCalls);
                  WorkMonthEnumerate (d, apmemTasks);
                  ProjectMonthEnumerate (d, apmemProjects);
                  HolidaysFromMonth (iMonth, iYear, aMemHoliday);

                  // starting days 3 back
                  int   iDOW, iDay;
                  iDay = 1 - iStartDOW;

                  for (; iDay <= iDays; ) {
                     // loop by the week
                     MemCat (&gMemTemp, L"<tr>");

                     // do the week
                     for (iDOW = 0; iDOW < 7; iDOW++, iDay++) {
                        if ((iDay < 1) || (iDay > iDays)) {
                           // blank day
                           MemCat (&gMemTemp, L"<xdaynull/>");
                           continue;
                        }

                        PWSTR pszHoliday;
                        pszHoliday = (PWSTR) aMemHoliday[iDay].p;
                        // else it has a number

                        // if it's today
                        if ( ((DWORD)iMonth == MONTHFROMDFDATE(today)) &&
                           ((DWORD)iYear == YEARFROMDFDATE(today)) &&
                           ((int) DAYFROMDFDATE(today) == iDay))
                              MemCat (&gMemTemp, L"<xday bgcolor=#ffff40>");
                        else {
                           if (pszHoliday)
                              MemCat (&gMemTemp, L"<xday bgcolor=#80ffc0>");
                           else if ((iDOW == 0) || (iDOW == 6))
                              MemCat (&gMemTemp, L"<xday bgcolor=#b0b0ef>");
                           else
                              MemCat (&gMemTemp, L"<xday>");
                        }

                        // bold if there's anything in the tip
                        BOOL fBold;
                        fBold = apmemMeetings[iDay] || apmemCalls[iDay] || apmemReminders[iDay] || apmemTasks[iDay] || apmemProjects[iDay];

                        // show the number
                        if (!gfPrinting) {
                           MemCat (&gMemTemp, L"<a href=cd:");
                           MemCat (&gMemTemp, (int) TODFDATE(iDay, iMonth, iYear));
                           MemCat (&gMemTemp, L">");
                        }
                        if (fBold)
                           MemCat (&gMemTemp, L"<bold><big>");
                        MemCat (&gMemTemp, iDay);
                        if (fBold)
                           MemCat (&gMemTemp, L"</big></bold>");

                        // show list of stuff for the day
                        if (!gfPrinting) {
                           MemCat (&gMemTemp, L"<xhoverhelp>");
                           DWORD dwShown;
                           dwShown = 0;

                           if (pszHoliday) {
                              MemCat (&gMemTemp, L"<br><bold>");
                              MemCatSanitize (&gMemTemp, pszHoliday);
                              MemCat (&gMemTemp, L"</bold></br>");
                              dwShown++;
                           }

                           if (apmemMeetings[iDay]) {
                              MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Meetings</bold></p><xul>");
                              MemCat (&gMemTemp, (PWSTR) apmemMeetings[iDay]->p);
                              MemCat (&gMemTemp, L"</xul></align>");
                              dwShown++;
                           }
                           if (apmemCalls[iDay]) {
                              MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Scheduled calls</bold></p><xul>");
                              MemCat (&gMemTemp, (PWSTR) apmemCalls[iDay]->p);
                              MemCat (&gMemTemp, L"</xul></align>");
                              dwShown++;
                           }
                           if (apmemReminders[iDay]) {
                              if (dwShown)
                                 MemCat (&gMemTemp, L"<br/>");

                              MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Reminders</bold></p><xul>");
                              MemCat (&gMemTemp, (PWSTR) apmemReminders[iDay]->p);
                              MemCat (&gMemTemp, L"</xul></align>");
                              dwShown++;
                           }
                           if (apmemTasks[iDay]) {
                              if (dwShown)
                                 MemCat (&gMemTemp, L"<br/>");

                              MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Tasks</bold></p><xul>");
                              MemCat (&gMemTemp, (PWSTR) apmemTasks[iDay]->p);
                              MemCat (&gMemTemp, L"</xul></align>");
                              dwShown++;
                           }
                           if (apmemProjects[iDay]) {
                              if (dwShown)
                                 MemCat (&gMemTemp, L"<br/>");

                              MemCat (&gMemTemp, L"<align parlinespacing=0><p><bold>Projects</bold></p><xul>");
                              MemCat (&gMemTemp, (PWSTR) apmemProjects[iDay]->p);
                              MemCat (&gMemTemp, L"</xul></align>");
                              dwShown++;
                           }

                           if (!dwShown)
                              MemCat (&gMemTemp, L"Nothing scheduled for the day.");

                           // BUGFIX - day of the week
                           DWORD dwDayOfYear, dwDaysInYear, dwWeekOfYear;
                           DateToDayOfYear (TODFDATE(iDay, iMonth, iYear),
                              &dwDayOfYear, &dwDaysInYear, &dwWeekOfYear);
                           MemCat (&gMemTemp, L"<br/><italic>Week ");
                           MemCat (&gMemTemp, dwWeekOfYear);
                           MemCat (&gMemTemp, L", Day ");
                           MemCat (&gMemTemp, (int) dwDayOfYear);
                           MemCat (&gMemTemp, L", ");
                           MemCat (&gMemTemp, (int) dwDaysInYear - dwDayOfYear);
                           MemCat (&gMemTemp, L" days left</italic>");


                           MemCat (&gMemTemp, L"</xhoverhelp>");

                           MemCat (&gMemTemp, L"</a>");
                        }  // !gfPrinting


                        MemCat (&gMemTemp, L"</xday>");
                     }

                     MemCat (&gMemTemp, L"</tr>");
                  }

                  // free up memory for reminders, meetings, etc.
                  DWORD i;
                  for (i = 0; i < 32; i++) {
                     if (apmemReminders[i])
                        delete apmemReminders[i];
                     if (apmemProjects[i])
                        delete apmemProjects[i];
                     if (apmemTasks[i])
                        delete apmemTasks[i];
                     if (apmemMeetings[i])
                        delete apmemMeetings[i];
                     if (apmemCalls[i])
                        delete apmemCalls[i];
                  }

                  MemCat (&gMemTemp, L"</table>");
               }  // gfOnlyShowRestOfYear check

               // show in two columns
               MemCat (&gMemTemp, L"</td>");
               if (!(iMonth % 2))
                  MemCat (&gMemTemp, L"</tr>");
            }
            MemCat (&gMemTemp, L"</table>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         // Annual calendar by weeks
         else if (!_wcsicmp(p->pszSubName, L"MONTH2")) {
            MemZero (&gMemTemp);

            DFDATE today;
            today = Today();
            // how many days in the month
            if (gdateCalendarYearly == -1)
               gdateCalendarYearly = today;

            int iYear, iMonth;
            iYear = YEARFROMDFDATE (gdateCalendarYearly);

            // show in two columns
            MemCat (&gMemTemp, L"<table width=100%%>");
            MemCat (&gMemTemp, L"<tr><xWeekNum/><xDayOfWeek2>Sun</xDayOfWeek2><xDayOfWeek2>Mon</xDayOfWeek2><xDayOfWeek2>Tues</xDayOfWeek2><xDayOfWeek2>Wed</xDayOfWeek2><xDayOfWeek2>Thurs</xDayOfWeek2><xDayOfWeek2>Fri</xDayOfWeek2><xDayOfWeek2>Sat</xDayOfWeek2>");

            BOOL fOnlyShow;
            fOnlyShow = gfOnlyShowRestOfYear && (iYear == (int) YEARFROMDFDATE(today));

            // find out when Jans starts
            int iDOWJan;
            EscDaysInMonth (1, iYear, &iDOWJan);

            // loop
            int iDayOfYear, iDOW, iDaysInMonth, iDay, iWeek;
            DWORD i;
            iDayOfYear = -iDOWJan;
            iMonth = 0;
            iDaysInMonth  = iDOWJan;
            iDOW = 0;
            iDay = 1;
            iWeek = 0;
            PCMem    apmemReminders[32], apmemProjects[32], apmemTasks[32], apmemMeetings[32], apmemCalls[32];
            CMem     aMemHoliday[32];
            memset (apmemReminders, 0, sizeof(apmemReminders));
            memset (apmemProjects, 0, sizeof(apmemProjects));
            memset (apmemTasks, 0, sizeof(apmemTasks));
            memset (apmemMeetings, 0, sizeof(apmemMeetings));
            memset (apmemCalls, 0, sizeof(apmemCalls));
            while (TRUE) {
               // if there are no days left in this month then go to the next one
               if (iDaysInMonth <= 0) {
                  for (i = 0; i < 32; i++) {
                     if (apmemReminders[i])
                        delete apmemReminders[i];
                     if (apmemProjects[i])
                        delete apmemProjects[i];
                     if (apmemTasks[i])
                        delete apmemTasks[i];
                     if (apmemMeetings[i])
                        delete apmemMeetings[i];
                     if (apmemCalls[i])
                        delete apmemCalls[i];
                     MemZero (&aMemHoliday[i]);
                  }

                  iMonth++;
                  if (iMonth > 12)
                     break;

                  // load new month info in
                  // iDay = 1; BUGFIX - Changed to 1-iDaysInMonth
                  iDay = 1 - iDaysInMonth;
                  iDaysInMonth = EscDaysInMonth (iMonth, iYear, (int*) &i);
                  iDaysInMonth -= (iDay - 1);

                  // determine what links should have for reminders, task, etc.
                  memset (apmemReminders, 0, sizeof(apmemReminders));
                  memset (apmemProjects, 0, sizeof(apmemProjects));
                  memset (apmemTasks, 0, sizeof(apmemTasks));
                  memset (apmemMeetings, 0, sizeof(apmemMeetings));
                  memset (apmemCalls, 0, sizeof(apmemCalls));
                  DFDATE d;
                  d = TODFDATE(1, iMonth, iYear);
                  ReminderMonthEnumerate (d, apmemReminders);
                  SchedMonthEnumerate (d, apmemMeetings);
                  PhoneMonthEnumerate (d, apmemCalls);
                  WorkMonthEnumerate (d, apmemTasks);
                  ProjectMonthEnumerate (d, apmemProjects);
                  HolidaysFromMonth (iMonth, iYear, aMemHoliday);
               }

               // BUGFIX - If user only wants to see current month and beyond
               // then skip ahead until find it
               if (fOnlyShow && (iMonth < (int) MONTHFROMDFDATE(today))) {
                  // will it be the right day in 7 days
                  BOOL fRightInSeven = FALSE;
                  if ((iMonth+1 == (int) MONTHFROMDFDATE(today)) && (iDaysInMonth < 7))
                     fRightInSeven = TRUE;

                  // if not going to be shown in 7 days then skip
                  if (!fRightInSeven) {
                     iWeek++;
                     // dont cahnge iDOW
                     iDay += 7;
                     iDaysInMonth -= 7;
                     iDayOfYear += 7;
                     continue;
                  }
               }

               // consider starting a new row
               if (!iDOW) {
                  if (!iWeek) {
                     MemCat (&gMemTemp, L"<xWeekNum/>");
                  }
                  else {
                     MemCat (&gMemTemp, L"<xWeekNum>Week ");
                     MemCat (&gMemTemp, (iWeek > 52) ? 1 : iWeek);
                     if (iWeek > 52)
                        MemCat (&gMemTemp, L" (next year)");
                     MemCat (&gMemTemp, L"</xWeekNum>");
                  }

                  iWeek++;
                  MemCat (&gMemTemp, L"</tr><tr><xWeekNum>");

                  // if it's a change of month then indicate so
                  if ((iDaysInMonth < 7) && (iMonth < 12)) {
                     MemCat (&gMemTemp, L"<a color=#");
                     MemCat (&gMemTemp, (iMonth % 2) ? L"e0e0ff" : L"e0ffe0");
                     MemCat (&gMemTemp, L" href=cm:");
                     MemCat (&gMemTemp, (int) TODFDATE(1,iMonth+1,iYear));
                     MemCat (&gMemTemp, L"><bold>");
                     MemCat (&gMemTemp, gpszMonth[iMonth-1+1]);
                     MemCat (&gMemTemp, L"</bold></a>");

                  }

                  MemCat (&gMemTemp, L"</xWeekNum>");
               }

               // if the day isn't 
               if (!iMonth) {
                  MemCat (&gMemTemp, L"<xdaynull2/>");
               }
               else {   // it's a day in the month
                  PWSTR pszHoliday;
                  pszHoliday = (PWSTR) aMemHoliday[iDay].p;
                  if (pszHoliday && !pszHoliday[0])
                     pszHoliday = 0;
                  // else it has a number

                  // if it's today
                  if ( ((DWORD)iMonth == MONTHFROMDFDATE(today)) &&
                     ((DWORD)iYear == YEARFROMDFDATE(today)) &&
                     ((int) DAYFROMDFDATE(today) == iDay))
                        MemCat (&gMemTemp, L"<xday2 bgcolor=#ffff40>");
                  else {
                     if (pszHoliday)
                        MemCat (&gMemTemp, L"<xday2 bgcolor=#80ffc0>");
                     else if ((iDOW == 0) || (iDOW == 6))
                        MemCat (&gMemTemp,
                        (iMonth % 2) ? L"<xday2 bgcolor=#c0ffc0>" : L"<xday2 bgcolor=#c0c0ff>");
                     else
                        MemCat (&gMemTemp,
                        gfFullColor ?
                           ((iMonth % 2) ? L"<xday2 bgcolor=#e0ffe0>" : L"<xday2 bgcolor=#e0e0ff>") :
                            L"<xday2>"
                     );
                  }

                  // show the number
                  MemCat (&gMemTemp, L"<big><big><bold>");

                  // BUGFIX - Link to the combo page instead of the daily log
                  // show the number
                  if (!gfPrinting) {
                     MemCat (&gMemTemp, L"<a href=cd:");
                     MemCat (&gMemTemp, (int) TODFDATE(iDay, iMonth, iYear));
                     MemCat (&gMemTemp, L">");
                     MemCat (&gMemTemp, L"<xhoverhelpshort>");
                     // BUGFIX - day of the week
                     DWORD dwDayOfYear, dwDaysInYear, dwWeekOfYear;
                     DateToDayOfYear (TODFDATE(iDay, iMonth, iYear),
                        &dwDayOfYear, &dwDaysInYear, &dwWeekOfYear);
                     MemCat (&gMemTemp, L"<italic>Week ");
                     MemCat (&gMemTemp, dwWeekOfYear);
                     MemCat (&gMemTemp, L", Day ");
                     MemCat (&gMemTemp, (int) dwDayOfYear);
                     MemCat (&gMemTemp, L", ");
                     MemCat (&gMemTemp, (int) dwDaysInYear - dwDayOfYear);
                     MemCat (&gMemTemp, L" days left</italic>");

                     MemCat (&gMemTemp, L"</xhoverhelpshort>");
                  }
                  MemCat (&gMemTemp, iDay);
                  if (!gfPrinting)
                     MemCat (&gMemTemp, L"</a>");
                  MemCat (&gMemTemp, L"</bold></big></big>");

                  // if there's a holiday show that
                  if (pszHoliday) {
                     MemCat (&gMemTemp, L"<br/><bold>");
                     MemCatSanitize (&gMemTemp, pszHoliday);
                     MemCat (&gMemTemp, L"</bold>");
                  }

                  // see if there's any meetings, etc. today. If so show
                  // PCMem    apmemReminders[31], apmemProjects[31], apmemTasks[31], apmemMeetings[31];
                  DWORD dwCount;
                  if (apmemMeetings[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                        MemCat (&gMemTemp, L"<a href=r:113>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemMeetings[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Meetings (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Meeting");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemMeetings[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemCalls[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                     MemCat (&gMemTemp, L"<a href=r:117>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemCalls[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Scheduled calls (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Scheduled call");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemCalls[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemReminders[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                     MemCat (&gMemTemp, L"<a href=r:114>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemReminders[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Reminders (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Reminder");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemReminders[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemTasks[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                        MemCat (&gMemTemp, L"<a href=r:115>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemTasks[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Tasks (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Task");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemTasks[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }
                  if (apmemProjects[iDay]) {
                     MemCat (&gMemTemp, L"<small><br/>");
                     if (!gfPrinting)
                        MemCat (&gMemTemp, L"<a href=r:145>");
                     // BUGFIX - If occurs multple times then indicate so
                     dwCount = CountOccurances ((PWSTR)apmemProjects[iDay]->p, gszLi);
                     if (dwCount > 1) {
                        MemCat (&gMemTemp, L"Projects (");
                        MemCat (&gMemTemp, (int) dwCount);
                        MemCat (&gMemTemp, L")");
                     }
                     else
                        MemCat (&gMemTemp, L"Project");
                     if (!gfPrinting) {
                        MemCat (&gMemTemp, L"<xhoverhelp><align parlinespacing=0><xul>");
                        MemCat (&gMemTemp, (PWSTR) apmemProjects[iDay]->p);
                        MemCat (&gMemTemp, L"</xul></align></xhoverhelp>");
                        MemCat (&gMemTemp, L"</a>");
                     }
                     MemCat (&gMemTemp, L"</small>");
                  }

                  MemCat (&gMemTemp, L"</xday2>");
               }

               // next day
               iDOW = (iDOW+1)%7;
               iDay++;
               iDaysInMonth--;
               iDayOfYear++;

            }

            // finish up
            if (iDOW) {
               for (i = iDOW; i < 7; i++) {
                  MemCat (&gMemTemp, L"<xdaynull2/>");
               }
               MemCat (&gMemTemp, L"<xWeekNum>Week ");
               MemCat (&gMemTemp, (iWeek > 52) ? 1 : iWeek);
               if (iWeek > 52)
                  MemCat (&gMemTemp, L" (next year)");
               MemCat (&gMemTemp, L"</xWeekNum>");
            }


            MemCat (&gMemTemp, L"</tr></table>");

            
            

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "daysfromtoday"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"daysfromtoday")) {
            pPage->MBSpeakInformation (L"Click on a day in the calendar.");
            sdwClickMeaning = 1;
            sDateFirst = Today();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"restofyear")) {
            gfOnlyShowRestOfYear = p->pControl->AttribGetBOOL (gszChecked);
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"daysfromdate")) {
            pPage->MBSpeakInformation (L"Click on a day in the calendar.");
            sdwClickMeaning = 2;
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


