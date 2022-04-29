/*************************************************************************
Planner.cpp - For the planner feature

begun July 10 2001 by Mike Rozak
Copyright 2001 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

/* globals */
#define  MAXDAYS        7  // # of days to look ahead for
static CListFixed    galListPLANNERITEM[MAXDAYS];  // list of planner info
static DFDATE        gdwMenuDate;      // date used for menu
static DWORD         gdwListCB = 1;   // default index into the list combo box for PlannerPage
static DFTIME        gaBreaks[5][7];   // [start, lunch, lunch end, dinner, dinner end][sun..sat]
static BOOL          gfBreaksLoaded = FALSE; // TRUE if they're loaded

static WCHAR gszDateTo[] = L"DateTo";
static WCHAR gszStartTime[] = L"StartTime";
static WCHAR gszSplitTime[] = L"SplitTime";
static WCHAR gszSplit[] = L"Split";
static WCHAR gszTime[] = L"Time";
static WCHAR gszViewNext[] = L"ViewNext";
static WCHAR gszBreakString[] = L"t%d%d";


/***********************************************************************
DayOfWeekFromDFDATE - Given DFDATE returns the day of the week, 0..6
*/
DWORD DayOfWeekFromDFDATE (DFDATE day)
{
   HANGFUNCIN;
   int iDOW;
   int iDays = EscDaysInMonth (MONTHFROMDFDATE(day), YEARFROMDFDATE(day), &iDOW);

   iDOW = (iDOW + DAYFROMDFDATE(day)-1)%7;
   return iDOW;
}

/***********************************************************************
PlannerBreaksInit - Read in the breaks.
*/
void PlannerBreaksInit (void)
{
   HANGFUNCIN;
   if (gfBreaksLoaded)
      return;
   gfBreaksLoaded = TRUE;

   // set them up just in case don't read
   DWORD dow, dwBreak;
   for (dow = 0; dow < 7; dow++) {
      BOOL  fWeekend = ((dow == 0) || (dow == 6));

      gaBreaks[0][dow] = fWeekend ? TODFTIME(10,00) : TODFTIME(9,0);
      gaBreaks[1][dow] = TODFTIME(12,00);
      gaBreaks[2][dow] = TODFTIME(13,00);
      gaBreaks[3][dow] = TODFTIME(18,00);
      gaBreaks[4][dow] = TODFTIME(19, 00);
   }

   // read them in
   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszPlannerNode);
   if (!pNode)
      return;   // unexpected error

   WCHAR szTemp[64];
   for (dwBreak = 0; dwBreak < 5; dwBreak++) for (dow = 0; dow < 7; dow++) {
      swprintf (szTemp, gszBreakString, (int) dwBreak, (int) dow);
      gaBreaks[dwBreak][dow] = NodeValueGetInt (pNode, szTemp, (int)gaBreaks[dwBreak][dow]);
   }

   gpData->NodeRelease(pNode);
   gpData->Flush();
}

/***********************************************************************
PlannerBreaksWrite - write out the breaks
*/
void PlannerBreaksWrite (void)
{
   HANGFUNCIN;
   if (!gfBreaksLoaded)
      return;

   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszPlannerNode);
   if (!pNode)
      return;   // unexpected error

   WCHAR szTemp[64];
   DWORD dow, dwBreak;
   for (dwBreak = 0; dwBreak < 5; dwBreak++) for (dow = 0; dow < 7; dow++) {
      swprintf (szTemp, gszBreakString, (int) dwBreak, (int) dow);
      NodeValueSet (pNode, szTemp, (int)gaBreaks[dwBreak][dow]);
   }

   gpData->NodeRelease(pNode);
}

/***********************************************************************
PlannerFreeMem - Frees the PCMem in the galListPLANNERITEM. This MUST
be called after using the strings.
*/
void PlannerFreeMem (void)
{
   HANGFUNCIN;
   // clear existing lists
   DWORD i, j;
   for (i = 0; i < MAXDAYS; i++) {
      for (j = 0; j < galListPLANNERITEM[i].Num(); j++) {
         PPLANNERITEM p = (PPLANNERITEM) galListPLANNERITEM[i].Get(j);
         if (p->pMemMML) {
            delete p->pMemMML;
            p->pMemMML = 0;
         }
      }
   }
}

/***********************************************************************
BreaksEnumForPlanner - Do breaks

inputs
   DFDATE      date - start date
   DWORD       dwDays - # of days to generate
   PCListFixed palDays - Pointer to an array of CListFixed. Number of elements
               is dwDays. These will be concatenated with relevent information.
               Elements in the list are PLANNERITEM strctures.
returns
   none
*/
void BreaksEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays)
{
   HANGFUNCIN;
   DFTIME   now = Now();
   DFDATE   today = Today();

   // breaks like lunch
   PlannerBreaksInit();
   int iDOW;
   iDOW = DayOfWeekFromDFDATE(date);
   DWORD i;
   for (i = 0; i < dwDays; i++, iDOW = (iDOW+1)%7) {
      PLANNERITEM pi;
      memset (&pi, 0, sizeof(pi));
      pi.fFixedTime = TRUE;
      pi.dwType = 4;

      // lunch
      if ((gaBreaks[1][iDOW] != (DWORD)-1) && (gaBreaks[2][iDOW] > gaBreaks[1][iDOW])) {
         pi.dwDuration = (DWORD)(DFTIMEToMinutes(gaBreaks[2][iDOW]) - DFTIMEToMinutes(gaBreaks[1][iDOW]));
         pi.dwTime = gaBreaks[1][iDOW];

         // if it's today and this has already occurred then don't add
         if (i || (date != today) || (pi.dwTime >= now)) {
            pi.pMemMML = new CMem;
            if (!pi.pMemMML)
               break;
            MemZero (pi.pMemMML);
            MemCat (pi.pMemMML, L"<italic>Lunch break</italic>");

            palDays[i].Add (&pi);
         }
      }

      // dinner
      if ((gaBreaks[3][iDOW] != (DWORD)-1) && (gaBreaks[4][iDOW] > gaBreaks[3][iDOW])) {
         pi.dwDuration = (DWORD)(DFTIMEToMinutes(gaBreaks[4][iDOW]) - DFTIMEToMinutes(gaBreaks[3][iDOW]));
         pi.dwTime = gaBreaks[3][iDOW];

         // if it's today and this has already occurred then don't add
         if (i || (date != today) || (pi.dwTime >= now)) {
            pi.pMemMML = new CMem;
            if (!pi.pMemMML)
               break;
            MemZero (pi.pMemMML);
            MemCat (pi.pMemMML, L"<italic>Dinner</italic>");

            palDays[i].Add (&pi);
         }
      }
   }
}


/***********************************************************************
PlannerSortItems - Sort the work items for the day. A helper function
   to PlannerCollectInfo.

inputs
   PCListFixed    plistPLANNERITEM - If not NULL then use only this. Else use globals.
*/
int __cdecl PlannerSort(const void *elem1, const void *elem2 )
{
   HANGFUNCIN;
   PLANNERITEM *p1, *p2;
   p1 = (PLANNERITEM*)elem1;
   p2 = (PLANNERITEM*)elem2;

   // fixed has priority
   if (p1->fFixedTime != p2->fFixedTime)
      return -((int) p1->fFixedTime - (int)p2->fFixedTime);

   // if it's fixed time then sort by start time
   if (p1->fFixedTime) {   //p2->fFixedTime must also be true
      if (p1->dwTime != p2->dwTime)
         return (int)p1->dwTime - (int)p2->dwTime;
   }
   else {   // sort by priority
      if (p1->dwPriority != p2->dwPriority)
         return (int)p1->dwPriority - (int)p2->dwPriority;
   }

   // else, same time/priority, so sort by ID
   if (p1->dwMajor != p2->dwMajor)
      return (int) p1->dwMajor - (int)p2->dwMajor;
   if (p1->dwMinor != p2->dwMinor)
      return (int) p1->dwMinor - (int)p2->dwMinor;
   if (p1->dwSplit != p2->dwSplit)
      return (int) p1->dwSplit - (int)p2->dwSplit;

   // finally sort by name
   return _wcsicmp((PWSTR)p1->pMemMML->p, (PWSTR)p2->pMemMML->p);
}

void PlannerSortItems (PCListFixed plistPLANNERITEM = NULL)
{

   HANGFUNCIN;
   DWORD i;
   // sort this
   for (i = 0; i < (DWORD) (gdwListCB ? MAXDAYS : 1); i++) {
      PCListFixed pl;
      pl = plistPLANNERITEM ? plistPLANNERITEM : &galListPLANNERITEM[i];

      // BUGFIX - Loop through and set dwDuration2
      PPLANNERITEM p;
      DWORD j;
      for (j = 0; j < pl->Num(); j++) {
         p = (PPLANNERITEM) pl->Get(j);
         p->dwDuration2 = p->dwDuration;
      }

      qsort (pl->Get(0), pl->Num(), sizeof(PLANNERITEM), PlannerSort);

      // if any priorities are equal then use callbacks to change priorities
      // so that none are the same
      DWORD dwLastPriority;
      dwLastPriority = 0;
      for (j = 0; j < pl->Num(); j++) {
         p = (PPLANNERITEM) pl->Get(j);

         // ignore fixed time
         if (p->fFixedTime)
            continue;

         // if priorities OK then continue
         if (p->dwPriority > dwLastPriority) {
            dwLastPriority = p->dwPriority;
            continue;
         }

         // else, tell this to change priority
         dwLastPriority++;
         p->dwPriority = dwLastPriority;
         switch (p->dwType) {
         case 1:  // phone call
            PhoneAdjustPriority (p->dwMajor, p->dwMinor, p->dwSplit, p->dwPriority);
            break;
         case 2:  // task
            WorkAdjustPriority (p->dwMajor, p->dwMinor, p->dwSplit, p->dwPriority);
            break;
         case 3:  // project task
            ProjectAdjustPriority (p->dwMajor, p->dwMinor, p->dwSplit, p->dwPriority);
            break;
         }
      }

      // if only hat list then break here
      if (plistPLANNERITEM)
         break;
   }
}

/***********************************************************************
PlannerCollectInfo - Ask the meeting, phone call, tasks, projec tasks
what work items they need to add. Then, sort the items.

inputs
   none
returns
   none
*/
void PlannerCollectInfo (void)
{
   HANGFUNCIN;
   DFTIME now = Now();

   // clear existing lists
   DWORD i;
   PlannerFreeMem();
   for (i = 0; i < MAXDAYS; i++) {
      galListPLANNERITEM[i].Clear();
      galListPLANNERITEM[i].Init (sizeof(PLANNERITEM));
   }

   // get it from different places
   DFDATE today = Today();
   SchedEnumForPlanner (today, gdwListCB ? MAXDAYS : 1, galListPLANNERITEM);
   PhoneEnumForPlanner (today, gdwListCB ? MAXDAYS : 1, galListPLANNERITEM);
   WorkEnumForPlanner (today, gdwListCB ? MAXDAYS : 1, galListPLANNERITEM);
   ProjectEnumForPlanner (today, gdwListCB ? MAXDAYS : 1, galListPLANNERITEM);
   BreaksEnumForPlanner (today, gdwListCB ? MAXDAYS : 1, galListPLANNERITEM);

   PlannerSortItems();
}

/*********************************************************************
HeightFromTime - Given the duration of a meeting, this figures out
how hight (as a percentage of the screen width) it should be.

inputs
   DWORD    dwStart, dwEnd - DFTIME
   BOOL     fMeeting - TRUE if it's a meeting, FALSE if it's a phone call
returns
   int - Percent value, 0+
*/
#define  STARTOFDAY     TODFTIME(8,0)
#define  ENDOFDAY       TODFTIME(18,0)

static int HeightFromTime (DFTIME dwStart, DFTIME dwEnd)
{
   HANGFUNCIN;
   if (dwStart == (DWORD)-1) {
      dwStart = STARTOFDAY;
   }
   if (dwEnd == (DWORD)-1) {
      dwEnd = ENDOFDAY;
   }
   if (dwStart >= dwEnd)
      return 0;

   int   iStart, iEnd;
   iStart = DFTIMEToMinutes(dwStart);
   iEnd = DFTIMEToMinutes(dwEnd);

   // BUGFIX - Reduce in size somewhat, from 10 / 60 to 5 / 60
   return (iEnd - iStart) * 6 / 60;
}

/*********************************************************************
FillInTimes - Writes onto gMemTemp a table of times.

inputs
   DFTIME   dwStart, wdEnd - DFTIME
*/
static void FillInTimes (DFTIME dwStart,DFTIME dwEnd)
{
   HANGFUNCIN;
   if (dwStart == (DWORD)-1)
      dwStart = STARTOFDAY;
   if (dwEnd == (DWORD)-1)
      dwEnd = ENDOFDAY;

   // BUGFIX - to prevent really long days, max 24 hour day
   DWORD dwMax = TODFTIME(24,0);
   if (dwEnd > dwMax) {
      HANGFUNCIN;
      dwEnd = dwMax;
   }

   if (dwStart >= dwEnd)
      return;

   // BUGFIX - lrmargin=1 so that can fit more in
   MemCat (&gMemTemp, L"<small><table width=100% lrmargin=1 innerlines=1 border=1 valign=center align=center>");

   DFTIME   dwNext;
   DWORD dwLastStart = 0;
   for (; dwStart < dwEnd; dwStart = dwNext) {
      // find the next half hour increment
      int iStart;
      iStart = DFTIMEToMinutes(dwStart);
#ifdef _OLD
      iStart = iStart + 30;
      iStart -= iStart % 30;
#else
      // BUGFIX - Changed from 30 to 60 minute blocks
      iStart = iStart + 60;
      iStart -= iStart % 60;
#endif
      dwNext = TODFTIME(iStart / 60, iStart % 60);
      // BUGFIX - Prevent an infinite loop
      if (dwNext < dwLastStart) {
         dwNext = dwLastStart;
         HANGFUNCIN;
      }
      if (dwNext > dwEnd)
         dwNext = dwEnd;

      // section this off and write in the time
      MemCat (&gMemTemp, L"<tr><td height=");
      MemCat (&gMemTemp, HeightFromTime(dwStart, dwNext) * 5); // use *5 because this table is only 1/5th of other table width
      MemCat (&gMemTemp, L"%>");
      WCHAR szTemp[64];
      DFTIMEToString (dwStart, szTemp);
      MemCat (&gMemTemp, szTemp);
      MemCat (&gMemTemp, L"</td></tr>");
   }

   MemCat (&gMemTemp, L"</table></small>");
}


/***********************************************************************
PlannerMMLForDay - Generate the MML entry for the day. Adds it onto
   gMemTemp.

inputs
   DFDATE   date - date
   DFTIME   now - Set this to Now() if date == Today(). Else set to 0.
   PCListFixed pl - pointer to galListPLANNERITEM for the day. Must be sorted already
         with PlannerSort()
   DWORD dwListIndex - Index into the galListPLANNERITEM
   BOOL  fForCombo - If TRUE, then this is for the combo box, today, or tomorrow list,
            which means... Only calls and meetings. No reminders. No tasks. No up/down arrows.
            And a different header.
returns
   none
*/
void PlannerMMLForDay (DFDATE date, DFTIME now, PCListFixed pl, DWORD dwListIndex,
                       BOOL fForCombo = FALSE)
{
   HANGFUNCIN;
   PLANNERITEM *pi;

   // what are the holidays for today?
   CMem     aMemHoliday[32];
   HolidaysFromMonth (MONTHFROMDFDATE(date), YEARFROMDFDATE(date), aMemHoliday);

   // find the start of the non-fixed entries
   DWORD dwNonFixed, dwNum;
   dwNum = pl->Num();
   for (dwNonFixed = 0; dwNonFixed < dwNum; dwNonFixed++) {
      pi = (PPLANNERITEM)pl->Get(dwNonFixed);
      if (!pi->fFixedTime)
         break;
   }

   // remember a minimium start time for non-fixed tasks so that if have two meetings
   // overlapping, one long and one short, don't invent new time after display short one
   int   iMeetingEndMax;
   iMeetingEndMax = 0;

   // figure out the start time, which is a minimum of 8 AM or the first fixed entry
   // unless the user has specified
   int   iCurMin, iDOW;
   DFTIME   tUser;
   PlannerBreaksInit();
   tUser = gaBreaks[0][iDOW = DayOfWeekFromDFDATE(date)];
   iCurMin = (tUser != (DWORD)-1) ? DFTIMEToMinutes(tUser) : 8 * 60;

   // if it's today, then start iCurMin at now
   if (date == Today())
      iCurMin = max(iCurMin, DFTIMEToMinutes(now));
      // BUGFIX - If started early ignored start time

   pi = (PLANNERITEM*) pl->Get(0);
   if (pi && pi->fFixedTime)
      iCurMin = min(iCurMin, (int) DFTIMEToMinutes(pi->dwTime));

#if 0
   // Show off the meetings and stuff
   MemCat (&gMemTemp, L"<xlisttable>");
   // BUGFIX - User request for add XXX button in today, tomorrow, etc. page
   MemCat (&gMemTemp, L"<xtrheader align=left>");
   MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddmeeting");
   MemCat (&gMemTemp, (int) date);
   MemCat (&gMemTemp, L"><small><font color=#8080ff>Add meeting</font></small></button>");
   MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddcall");
   MemCat (&gMemTemp, (int) date);
   MemCat (&gMemTemp, L"><small><font color=#8080ff>Schedule call</font></small></button>");
   MemCat (&gMemTemp, szTemp);
   MemCat (&gMemTemp, L"<br/>");
   MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddreminder");
   MemCat (&gMemTemp, (int) date);
   MemCat (&gMemTemp, L"><small><font color=#8080ff>Add reminder</font></small></button>");
   MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddtask");
   MemCat (&gMemTemp, (int) date);
   MemCat (&gMemTemp, L"><small><font color=#8080ff>Add task</font></small></button>");
   MemCat (&gMemTemp, L"</xtrheader>");
   //MemCat (&gMemTemp, L"<xtrheader><a href=r:113 color=#c0c0ff>Meetings</a></xtrheader>");
#endif // 0
   // Show off the meetings and stuff
   MemCat (&gMemTemp, L"<table width=100%%>");
   if (gfPrinting) {
      if (fForCombo) {
         // BUGFIX - User request for add XXX button in today, tomorrow, etc. page
         MemCat (&gMemTemp, L"<xtrheader align=right>");
         MemCat (&gMemTemp, L"<a href=r:113 color=#c0c0ff>Meetings</a> and");
         MemCat (&gMemTemp, L"<br/>");
         MemCat (&gMemTemp, L"<a href=r:117 color=#c0c0ff>Scheduled calls</a>");
         MemCat (&gMemTemp, L"</xtrheader>");
      }
      else {
         //if (gfFullColor)
         //   MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#80c080 bcolor=#60b060/>");
         MemCat (&gMemTemp, L"<xtrheader align=right>");
         WCHAR szTemp[64];
         DFDATEToString (date, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<br/>");

         // if it's a holiday indicate that
         if (aMemHoliday[DAYFROMDFDATE(date)].p) {
            MemCat (&gMemTemp, L"<italic><small> (");
            MemCatSanitize (&gMemTemp, (PWSTR) aMemHoliday[DAYFROMDFDATE(date)].p);
            MemCat (&gMemTemp, L" )</small></italic>");
         }

         MemCat (&gMemTemp, L"</xtrheader>");
      }
   }
   else {
      if (fForCombo) {
         // BUGFIX - User request for add XXX button in today, tomorrow, etc. page
         MemCat (&gMemTemp, L"<xtrheader align=left>");
         MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddmeeting");
         MemCat (&gMemTemp, (int) date);
         MemCat (&gMemTemp, L"><small><font color=#ffffff>Add meeting</font></small></button>");
         MemCat (&gMemTemp, L"<align align=right><a href=r:113 color=#c0c0ff>Meetings</a> and</align>");
         MemCat (&gMemTemp, L"<br/>");
         MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddcall");
         MemCat (&gMemTemp, (int) date);
         MemCat (&gMemTemp, L"><small><font color=#ffffff>Schedule call</font></small></button>");
         MemCat (&gMemTemp, L"<align align=right><a href=r:117 color=#c0c0ff>Scheduled calls</a></align>");
         MemCat (&gMemTemp, L"</xtrheader>");
      }
      else {
         //if (gfFullColor)
         //   MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#80c080 bcolor=#60b060/>");
         MemCat (&gMemTemp, L"<xtrheader align=left><align align=right>");
         WCHAR szTemp[64];
         DFDATEToString (date, szTemp);
         MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddmeeting");
         MemCat (&gMemTemp, (int) date);
         MemCat (&gMemTemp, L"><small><font color=#8080ff>Add meeting</font></small></button>");
         MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddcall");
         MemCat (&gMemTemp, (int) date);
         MemCat (&gMemTemp, L"><small><font color=#8080ff>Schedule call</font></small></button>");
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<br/>");
         MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddreminder");
         MemCat (&gMemTemp, (int) date);
         MemCat (&gMemTemp, L"><small><font color=#8080ff>Add reminder</font></small></button>");
         MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddtask");
         MemCat (&gMemTemp, (int) date);
         MemCat (&gMemTemp, L"><small><font color=#8080ff>Add task</font></small></button>");

         // if it's a holiday indicate that
         if (aMemHoliday[DAYFROMDFDATE(date)].p) {
            MemCat (&gMemTemp, L"<italic><small> (");
            MemCatSanitize (&gMemTemp, (PWSTR) aMemHoliday[DAYFROMDFDATE(date)].p);
            MemCat (&gMemTemp, L" )</small></italic>");
         }

         MemCat (&gMemTemp, L"</align></xtrheader>");
      }
   }

   if (!fForCombo) {
      // show reminders
      PCMem    apmemReminders[32];
      memset (apmemReminders, 0, sizeof(apmemReminders));
      ReminderMonthEnumerate (date, apmemReminders, Today(), !gfPrinting);
      PCMem pr;
      pr = apmemReminders[DAYFROMDFDATE(date)];
      if (pr && pr->p && ((PWSTR)pr->p)[0]) {
         MemCat (&gMemTemp, L"<tr>");

         // times
         MemCat (&gMemTemp, L"<td width=25% ");
         if (gfFullColor)
            MemCat (&gMemTemp, L"bgcolor=#808080 ");
         MemCat (&gMemTemp, L"valign=center align=center>");
         MemCat (&gMemTemp, L"<bold><italic>Reminders</italic></bold>");
         MemCat (&gMemTemp, L"</td>");

         // and show that it's empty
         MemCat (&gMemTemp, L"<td width=75% ");
         if (gfFullColor)
            MemCat (&gMemTemp, L"bgcolor=#c0ffc0");
         MemCat (&gMemTemp, L"><xul>");
         MemCat (&gMemTemp, (PWSTR) pr->p);
         MemCat (&gMemTemp, L"</xul></td>");

         MemCat (&gMemTemp, L"</tr>");
      }
      DWORD i;
      for (i = 0; i < 32; i++) {
         if (apmemReminders[i])
            delete apmemReminders[i];
      }
   }

   // start the loop
   DWORD dwFix, dwNon;

   // BUGFIX - Make it clear when a task is bisected by a meeting or planned call
   BOOL  fThisTaskInterrupted;   // set to TRUE if this task has been cut in half because meeting coming up
   BOOL  fNextTaskContinue;      // set to TRUE if the next task is a continuation
   fThisTaskInterrupted = fNextTaskContinue = FALSE;

   // BUGFIX - Maximum length so wont take forever
   DWORD dwMaxLength = 100;
   for (dwFix = 0, dwNon=dwNonFixed; ((dwFix < dwNonFixed) || (dwNon < dwNum)) && dwMaxLength; dwMaxLength--) {

      // at what time can the next meeting occur, vs. the next task
      int   iNextMeeting, iNextTask;
      BOOL  fNextMeeting, fNextTask;
      iNextMeeting = iNextTask = 1000000;
      if (dwFix < dwNonFixed) {
         fNextMeeting = TRUE;
         pi = (PPLANNERITEM)pl->Get(dwFix);
         iNextMeeting = DFTIMEToMinutes(pi->dwTime);
      }
      else
         fNextMeeting = FALSE;   // cant fit any more meetings
      if (dwNon < dwNum) {
         fNextTask = TRUE;
         pi = (PPLANNERITEM)pl->Get(dwNon);
         iNextTask = DFTIMEToMinutes(pi->dwTime);
         iNextTask = max(iNextTask, iCurMin);
         iNextTask = max(iNextTask, (int) DFTIMEToMinutes(now)); // if it's today and later on then push all the tasks out
      }
      else
         fNextTask = FALSE;   // cant fit any more meetings


      // if we have time left, put in a non-fixed task, assuming it can be split
      if (fNextTask) {
         // if it's a phone call then need the whole time
         pi = (PLANNERITEM*) pl->Get(dwNon);
         if ((pi->dwType == 1) && ((iNextTask + (int)pi->dwDuration2) > iNextMeeting)) {
            // it doesn't fit before the next meeting so skip this task
            iNextTask = 1000000;
            fNextTask = FALSE;
         }
      }

      // if there's neither meeting nor task then exit
      if (!fNextMeeting && !fNextTask)
         break;

      // what time does the meeting/task happen
      int   iNext, iMinutesAvail;
      iNext = 1000000;
      if (fNextMeeting)
         iNext = min(iNext, iNextMeeting);
      if (fNextTask)
         iNext = min(iNext, iNextTask);
      iMinutesAvail = iNext - iCurMin;
      if (iMinutesAvail < 0)
         iMinutesAvail = 0;

      if (iMinutesAvail) {
         MemCat (&gMemTemp, L"<tr>");

         // times
         MemCat (&gMemTemp, L"<td width=25% lrmargin=0 tbmargin=0 bgcolor=#c0c0c0>");
         FillInTimes (TODFTIME(iCurMin / 60, iCurMin % 60), TODFTIME(iNext / 60, iNext % 60));
         MemCat (&gMemTemp, L"</td>");

         // and show that it's empty
         MemCat (&gMemTemp, L"<td width=75% align=center valign=center>");
         MemCat (&gMemTemp, L"<big><italic><font color=#808080>Nothing scheduled</font></italic></big>");
         MemCat (&gMemTemp, L"</td>");

         MemCat (&gMemTemp, L"</tr>");
      }
      iCurMin = iNext;  // jump to the next item

      // which one are we choosing
      BOOL  fUseMeeting;
      DWORD dwCurIndex; // remember the index
      BOOL  fConflict;  // set to TRUE if have meeting conflicts
      fConflict = FALSE;
      int iEnd;
      if (fNextMeeting && (iCurMin == iNextMeeting)) {
         pi = (PPLANNERITEM)pl->Get(dwFix);
         fUseMeeting = TRUE;
         iEnd = iCurMin + (int)pi->dwDuration2;

         if ((iEnd < iMeetingEndMax) || (iNextMeeting < iMeetingEndMax))
            fConflict = TRUE;

         iMeetingEndMax = max(iMeetingEndMax, iEnd);

         // increment to the next meeting after this
         dwCurIndex = dwFix;
         dwFix++;
      }
      else {
         pi = (PPLANNERITEM)pl->Get(dwNon);
         fUseMeeting = FALSE;
         iEnd = iCurMin + (int)pi->dwDuration2;

         // if there's a meeting, and iEnd > that meeting, then split this task
         // into two
         dwCurIndex = dwNon;
         if (fNextMeeting && (iEnd > iNextMeeting)) {
            int   iOver = iEnd - iNextMeeting;
            iEnd = iNextMeeting;
            pi->dwDuration2 = (DWORD) iOver;

            // note that it's split
            fThisTaskInterrupted = TRUE;

            // and don't increment so we continue with this task later
         }
         else {
            // increment to the next task after this
            dwNon++;
         }
      }

      MemCat (&gMemTemp, L"<tr>");
      MemCat (&gMemTemp, L"<td width=25% valign=center lrmargin=0 tbmargin=0 bgcolor=");
      MemCat (&gMemTemp, fConflict ? L"#ff4040" : L"#808080");
      MemCat (&gMemTemp, L"><bold>");
      FillInTimes (TODFTIME(iCurMin / 60, iCurMin % 60), TODFTIME(iEnd / 60, iEnd % 60));

      // update the current time to a new end. However, if we had one really long meeting,
      // and a shorter one sceduled at the same time (but starting a bit later and finishing earelier) then make
      // sure we don't invent time for tasks
      iCurMin = max(iMeetingEndMax,iEnd);

      MemCat (&gMemTemp, L"</bold></td>");

      // and show that it's empty
      MemCat (&gMemTemp, L"<td width=75%><small>");

      if (gfFullColor) {
         switch (pi->dwType) {
         case 0:  // meeting
            MemCat (&gMemTemp, L"<colorblend posn=background lcolor=#ff8080 rcolor=#ff8080/>");
            break;
         case 1:  // phone call
            if (pi->fFixedTime)
               MemCat (&gMemTemp, L"<colorblend posn=background lcolor=#ff80c0 rcolor=#ff80c0/>");
            else
               MemCat (&gMemTemp, L"<colorblend posn=background lcolor=#c0c0c0 rcolor=#ff80c0/>");
            break;
         case 2:  // task
            MemCat (&gMemTemp, L"<colorblend posn=background lcolor=#c0c0c0 rcolor=#80ffff/>");
            break;
         case 3:  // projec task
            MemCat (&gMemTemp, L"<colorblend posn=background lcolor=#c0c0c0 rcolor=#80ffc0/>");
            break;
         case 4:  // break - lunch
            MemCat (&gMemTemp, L"<colorblend posn=background lcolor=#e0e0e0 rcolor=#e0e0e0/>");
            break;
         }
      }

      // Add buttons to manipulate ordering
      if (!gfPrinting && !fForCombo && !pi->fFixedTime) {
         // down arrow
         if (dwCurIndex+1 >= dwNum) {
            MemCat (&gMemTemp, L"<image bmpresource=156 border=0 transparent=true posn=edgeright/>");
         }
         else {
            MemCat (&gMemTemp, L"<image bmpresource=155 border=0 transparent=true posn=edgeright href=md:");
            MemCat (&gMemTemp, dwListIndex);
            MemCat (&gMemTemp, L":");
            MemCat (&gMemTemp, dwCurIndex);
            MemCat (&gMemTemp, L"><xhoverhelpshort>Moves the task down.</xhoverhelpshort></image>");
         }

         // misc
         MemCat (&gMemTemp, L"<image bmpresource=237 border=0 transparent=true posn=edgeright href=mx:");
         MemCat (&gMemTemp, dwListIndex);
         MemCat (&gMemTemp, L":");
         MemCat (&gMemTemp, dwCurIndex);
         MemCat (&gMemTemp, L"><xhoverhelp>Brings up a menu that lets you split the task, change the date, or set its start time.</xhoverhelp></image>");

         // up arrow
         if (dwCurIndex <= dwNonFixed) {
            MemCat (&gMemTemp, L"<image bmpresource=156 border=0 transparent=true posn=edgeright/>");
         }
         else {
            MemCat (&gMemTemp, L"<image bmpresource=154 border=0 transparent=true posn=edgeright href=mu:");
            MemCat (&gMemTemp, dwListIndex);
            MemCat (&gMemTemp, L":");
            MemCat (&gMemTemp, dwCurIndex);
            MemCat (&gMemTemp, L"><xhoverhelpshort>Moves the task up.</xhoverhelpshort></image>");
         }
      }

      // if its a continuation then note that
      if (!pi->fFixedTime && fNextTaskContinue) {
         fNextTaskContinue = FALSE;

         MemCat (&gMemTemp, L"<font color=#c00000><italic>... Continued from above.</italic></font><br/>");
      }
      else if (pi->dwPreviousDate) {
         // BUFIX - If there's another part of the task after this then say so
         WCHAR szTemp[64];
         MemCat (&gMemTemp, L"<font color=#c00000><italic>... Continued from ");
         if (pi->dwPreviousDate != (DWORD)-1) {
            DFDATEToString (pi->dwPreviousDate, szTemp);
            MemCat (&gMemTemp, szTemp);
         }
         else
            MemCat (&gMemTemp, L"before");   // not really sure how many days before
         MemCat (&gMemTemp, L".</italic></font><br/>");
      }

      // text
      MemCat (&gMemTemp, (PWSTR)pi->pMemMML->p);

      // if it's a continuation note that
      if (!pi->fFixedTime && fThisTaskInterrupted) {
         fNextTaskContinue = TRUE;
         fThisTaskInterrupted = FALSE;
         MemCat (&gMemTemp, L"<br/><font color=#c00000><italic>Continued below...</italic></font>");
      }
      else if (pi->dwNextDate) {
         // BUFIX - If there's another part of the task after this then say so
         WCHAR szTemp[64];
         MemCat (&gMemTemp, L"<br/><font color=#c00000><italic>Continued on ");
         DFDATEToString (pi->dwNextDate, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"... </italic></font>");
      }


      // completed
      MemCat (&gMemTemp, L"</small></td>");
      MemCat (&gMemTemp, L"</tr>");
      
   }

   // draw empty to the end of the day
   int   iEndOfDay;
   tUser = gaBreaks[3][iDOW];
   iEndOfDay = (tUser != (DWORD)-1) ? DFTIMEToMinutes(tUser) : 18 * 60;
   if (iCurMin < iEndOfDay) {
      int   iNext = iEndOfDay;

      MemCat (&gMemTemp, L"<tr>");

      // times
      MemCat (&gMemTemp, L"<td width=25% lrmargin=0 tbmargin=0 bgcolor=#c0c0c0>");
      FillInTimes (TODFTIME(iCurMin / 60, iCurMin % 60), TODFTIME(iNext / 60, iNext % 60));
      MemCat (&gMemTemp, L"</td>");

      // and show that it's empty
      MemCat (&gMemTemp, L"<td width=75% align=center valign=center>");
      MemCat (&gMemTemp, L"<big><italic><font color=#808080>Nothing scheduled</font></italic></big>");
      MemCat (&gMemTemp, L"</td>");

      MemCat (&gMemTemp, L"</tr>");
   }

   // finish off
   MemCat (&gMemTemp, L"</table><p/>");
}


/*****************************************************************************
PlannerMenuPage - Override page callback.
*/
BOOL PlannerMenuPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PPLANNERITEM pItem = (PPLANNERITEM) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // pressed add. Do some verification
         PCEscControl pControl;

         DFDATE date, today;
         today = Today();
         date = (gdwMenuDate > today) ? gdwMenuDate : today;
         DateControlSet (pPage, gszDateTo, MinutesToDFDATE(DFDATEToMinutes(date)+60*24));
         TimeControlSet (pPage, gszStartTime, pItem->dwTime ? pItem->dwTime : -1);


         // duration
         pControl = pPage->ControlFind(gszSplitTime);
         WCHAR szTemp[64];
         swprintf (szTemp, L"%g", (double) pItem->dwDuration / 60 / 2);
         if (pControl)
            pControl->AttribSet (gszText, szTemp);

         // disable split if it's a phone call
         if (pItem->dwType == 1) {
            pControl = pPage->ControlFind (gszSplit);
            if (pControl)
               pControl->Enable(FALSE);

            pControl = pPage->ControlFind(gszSplitTime);
            if (pControl) {
               pControl->AttribSet (gszText, L"");
               pControl->Enable(FALSE);
            }
         }
      }
      break;   // default

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"IFNOTCALL")) {
            p->pszSubString = (pItem->dwType == 1) ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFNOTCALL")) {
            p->pszSubString = (pItem->dwType == 1) ? L"</comment>" : L"";
            return TRUE;
         }

      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         // if OK then do action
         if (!_wcsicmp(p->psz, gszOK)) {
            PCEscControl pControl;
            
            if ((pControl = pPage->ControlFind (gszDate)) && pControl->AttribGetBOOL(gszChecked)) {
               DFDATE dNew = DateControlGet (pPage, gszDateTo);

               // change the date
               switch (pItem->dwType) {
               case 1:  // phone call
                  PhoneChangeDate (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, dNew);
                  break;
               case 2:  // task
                  WorkChangeDate (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, dNew);
                  break;
               case 3:  // project task
                  ProjectChangeDate (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, dNew);
                  break;
               }
               
            }
            else if ((pControl = pPage->ControlFind (gszTime)) && pControl->AttribGetBOOL(gszChecked)) {
               DFTIME dNew = TimeControlGet (pPage, gszStartTime);
               if (dNew == (DWORD)-1)
                  dNew = 0;

               // change the date
               switch (pItem->dwType) {
               case 1:  // phone call
                  PhoneChangeTime (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, dNew);
                  break;
               case 2:  // task
                  WorkChangeTime (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, dNew);
                  break;
               case 3:  // project task
                  ProjectChangeTime (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, dNew);
                  break;
               }
            }
            else if ((pControl = pPage->ControlFind (gszSplit)) && pControl->AttribGetBOOL(gszChecked)) {
               pControl = pPage->ControlFind (gszSplitTime);
               double fVal;
               int iVal;
               WCHAR szTemp[256];
               DWORD dwNeeded;
               fVal = 0;
               szTemp[0]= 0;
               if (pControl)
                  pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
               AttribToDouble (szTemp, &fVal);
               iVal = (int) (fVal * 60);
               if ((iVal <= 0) || (iVal >= (int) pItem->dwDuration)) {
                  pPage->MBWarning (L"The split you've requested is past the expected duration of the task.");
                  return TRUE;
               }

               // change the date
               switch (pItem->dwType) {
               case 2:  // task
                  WorkSplitPlan (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, (DWORD) iVal);
                  break;
               case 3:  // project task
                  ProjectSplitPlan (pItem->dwMajor, pItem->dwMinor, pItem->dwSplit, (DWORD) iVal);
                  break;
               }
            }
         }
      }
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
PlannerMenuUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   PPLANNERITEM   pItem - Item that's being modified
   DFDATE         date - date of the item
returns
   BOOL - TRUE if a project Day was added
*/
BOOL PlannerMenuUI (PCEscPage pPage, PPLANNERITEM pItem, DFDATE date)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   int iCenter, iWidth;
   iCenter = (r.right + r.left) / 2;
   iWidth = (r.right - iCenter) / 2;
   r.left = iCenter - iWidth;
   r.right = iCenter + iWidth;
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_NOTITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   //cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   gdwMenuDate = date;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPLANNERMENU, PlannerMenuPage, pItem);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}

/***********************************************************************
PlannerPage - Page callback
*/
BOOL PlannerPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"PLANNER")) {
            MemZero (&gMemTemp);

            // get the info
            PlannerCollectInfo();

            // display it
            DFDATE   date = Today();
            DFTIME   now = Now();
            DWORD    i;
            for (i = 0; i < (DWORD) (gdwListCB ? MAXDAYS : 1); i++) {
               PlannerMMLForDay (date, i ? 0 : now, galListPLANNERITEM + i, i);
               MemCat (&gMemTemp, L"<p/>");

               date = MinutesToDFDATE(DFDATEToMinutes(date)+60*24);

               // BUGFIX - Show next 7 days option at end
               if (!i)
                  MemCat (&gMemTemp, L"<xbr/><p align=right><bold>"
                     L"<combobox name=viewnext width=50%% cbheight=75 cursel=1>"
                     L"<elem name=1>Show only today</elem>"
                     L"<elem name=7>Show another 6 days</elem>"
                     L"</combobox>"
                     L"</bold></p>");
            }

            // free the string
            PlannerFreeMem();

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

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         // if its a reminder link delete and refresh
         BOOL fRefresh;
         DWORD dwNext;
         if ((p->psz[0] == L'm') && ((p->psz[1] == L'u') || (p->psz[1] == L'd')) && (p->psz[2] == L':')) {
            // get the index and item
            DWORD dwDay, dwItem;
            WCHAR *psz;
            dwDay = _wtoi (p->psz+3);
            psz = wcschr(p->psz+3, L':');
            dwItem = psz ? _wtoi(psz+1) : 0;

            // swap this with?
            DWORD dwSwap;
            dwSwap = (p->psz[1] == L'u') ? (dwItem - 1) : (dwItem+1);

            // get the two
            PPLANNERITEM p1, p2;
            p1 = (PPLANNERITEM) galListPLANNERITEM[dwDay].Get(dwItem);
            if (!p1)
               return TRUE;
            p2 = (PPLANNERITEM) galListPLANNERITEM[dwDay].Get(dwSwap);
            if (!p2)
               return TRUE;

            // swap the priorities
            switch (p1->dwType) {
            case 1:  // phone call
               PhoneAdjustPriority (p1->dwMajor, p1->dwMinor, p1->dwSplit, p2->dwPriority);
               break;
            case 2:  // task
               WorkAdjustPriority (p1->dwMajor, p1->dwMinor, p1->dwSplit, p2->dwPriority);
               break;
            case 3:  // project task
               ProjectAdjustPriority (p1->dwMajor, p1->dwMinor, p1->dwSplit, p2->dwPriority);
               break;
            }
            switch (p2->dwType) {
            case 1:  // phone call
               PhoneAdjustPriority (p2->dwMajor, p2->dwMinor, p2->dwSplit, p1->dwPriority);
               break;
            case 2:  // task
               WorkAdjustPriority (p2->dwMajor, p2->dwMinor, p2->dwSplit, p1->dwPriority);
               break;
            case 3:  // project task
               ProjectAdjustPriority (p2->dwMajor, p2->dwMinor, p2->dwSplit, p1->dwPriority);
               break;
            }

            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if ((p->psz[0] == L'm') && (p->psz[1] == L'x') && (p->psz[2] == L':')) {
            // get the index and item
            DWORD dwDay, dwItem;
            WCHAR *psz;
            dwDay = _wtoi (p->psz+3);
            psz = wcschr(p->psz+3, L':');
            dwItem = psz ? _wtoi(psz+1) : 0;

            // get the two
            PPLANNERITEM p1;
            p1 = (PPLANNERITEM) galListPLANNERITEM[dwDay].Get(dwItem);
            if (!p1)
               return TRUE;

            if (PlannerMenuUI(pPage, p1, MinutesToDFDATE(DFDATEToMinutes(Today())+60*24*dwDay)))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (ReminderParseLink (p->psz, pPage)) {
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
               swprintf (szTemp, L"e:%d", dwNext);
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
               swprintf (szTemp, L"e:%d", dwNext);
               pPage->Exit (szTemp);
               return TRUE;
            }

            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}



/***********************************************************************
PlannerBreaksPage - Page callback
*/
BOOL PlannerBreaksPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PlannerBreaksInit ();

         WCHAR szTemp[64];
         DWORD dow, dwBreak;
         for (dwBreak = 0; dwBreak < 5; dwBreak++) for (dow = 0; dow < 7; dow++) {
            swprintf (szTemp, gszBreakString, (int) dwBreak, (int) dow);
            TimeControlSet (pPage, szTemp, gaBreaks[dwBreak][dow]);
         }
      }
      break;   // go to default handler

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         WCHAR szTemp[64];
         DWORD dow, dwBreak;
         for (dwBreak = 0; dwBreak < 5; dwBreak++) for (dow = 0; dow < 7; dow++) {
            swprintf (szTemp, gszBreakString, (int) dwBreak, (int) dow);
            gaBreaks[dwBreak][dow] = (DFTIME) TimeControlGet (pPage, szTemp);
         }

         // save stuff away before leave
         PlannerBreaksWrite();
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
PlannerSummaryScale - Show meetings for one day, scaled so that there's
   a Microsoft Outlook type view with vertical amounts being equal time.

inputs
   DFDATE   date - date to show
   BOOL     fShowIfEmpty - If TRUE show even if it's empty
retursn
   PWSTR - string from gMemTemp
*/

PWSTR PlannerSummaryScale (DFDATE date, BOOL fShowIfEmpty)
{
   HANGFUNCIN;
   DFTIME now = Now();
   PWSTR pszRet;

   CListFixed  listPLANNERITEM;
   listPLANNERITEM.Init (sizeof(PLANNERITEM));

   // get it from different places
   SchedEnumForPlanner (date, 1, &listPLANNERITEM);
   PhoneEnumForPlanner (date, 1, &listPLANNERITEM, TRUE);

   // if there aren't any items added yet AND we don't show if empty then reutnr
   // NULL
   if (!fShowIfEmpty && !listPLANNERITEM.Num()) {
      pszRet = L"";
      goto done;
   }

   // put in lunch breaks, etc.
   BreaksEnumForPlanner (date, 1, &listPLANNERITEM);

   // sort this
   PlannerSortItems(&listPLANNERITEM);

   // display this
   MemZero (&gMemTemp);
   PlannerMMLForDay (date, now, &listPLANNERITEM, 1, TRUE);

   pszRet = (PWSTR) gMemTemp.p;

done:
   DWORD j;
   for (j = 0; j < listPLANNERITEM.Num(); j++) {
      PPLANNERITEM p = (PPLANNERITEM) listPLANNERITEM.Get(j);
      if (p->pMemMML) {
         delete p->pMemMML;
         p->pMemMML = 0;
      }
   }
   return pszRet;
}
