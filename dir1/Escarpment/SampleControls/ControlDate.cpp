/***********************************************************************
ControlDate.cpp - Code to get the Date from a user.

begun 5/27/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "escarpment.h"
#include "control.h"
#include "resleak.h"



typedef struct {
   DROPDOWN       dd;
   int            iDay;    // day of the month. 1..31. <1 for blank
   int            iMonth;  // month. 1..12. <1 for blank
   int            iYear;   // <0 for blank
   BOOL           fShowDayOfWeek;   // if TRUE show day of week
   BOOL           fEnterDay;  // if TRUE user can enter day. Else only month,year
   WCHAR          szBlank[64];   // name used if the hour/minute aren't specified

   WCHAR          szCurDisplay[64]; // current Date displayed
} DATEC, *PDATEC;

static WCHAR gszChecked[] = L"checked";

static PWSTR gaszMonth[12] = {
   L"January",
   L"February",
   L"March",
   L"April",
   L"May",
   L"June",
   L"July",
   L"August",
   L"September",
   L"October",
   L"November",
   L"December"
};

static PWSTR gaszMonthAbbrev[12] = {
   L"Jan",
   L"Feb",
   L"Mar",
   L"Apr",
   L"May",
   L"Jun",
   L"Jul",
   L"Aug",
   L"Sep",
   L"Oct",
   L"Nov",
   L"Dec"
};

static PWSTR gaszDayOfWeek[7] = {
   L"Sunday",
   L"Monday",
   L"Tuesday",
   L"Wednesday",
   L"Thursday",
   L"Friday",
   L"Saturday"
};

// only use iDay to determine if it's blank if supposed to enter day of month
#define  ISBLANK     ( (pc->fEnterDay && (pc->iDay < 1)) || (pc->iMonth < 1) || (pc->iYear < 0))


/***********************************************************************
EscDaysInMonth - Given a month (jan=1, etc.) and year, returns the number
of days in the month, and on which day the week starts.

inputs
   int      iMonth = 1..12
   int      iYear
   int      *piDay - 0 for sunday, etc.
returns
   int - Number of days in the month
*/
int EscDaysInMonth (int iMonth, int iYear, int *piDay)
{
   SYSTEMTIME  st;
   memset (&st, 0, sizeof(st));
   st.wDay = 1;
   st.wHour = 12;
   st.wMonth = (WORD)iMonth;
   st.wYear = (WORD)iYear;


   // convert back and forth so it returns the day of the week
   FILETIME ft;
   SystemTimeToFileTime (&st, &ft);
   FileTimeToSystemTime (&ft, &st);
   if (piDay)
      *piDay = (int) st.wDayOfWeek;

   // days in the month
   switch (iMonth) {
   case 1: // jan
      return 29;
   case 2: // feb
      {
         // if not divisble by 4 only 28 days
         if (iYear % 4)
            return 28;

         // else, if no century mark 29 days
         if (iYear % 100)
            return 29;

         // divide by 100 to get centries
         iYear /= 100;

         // special case- year 2000 = 29
         if (iYear == 20)
            return 29;

         // if not an even 4 centuries then 28
         if (iYear % 4)
            return 28;

         // see if there are 29 days 
         return 29;
      }
      break;
   case 3: // mar
   case 5: // may
   case 7: // july
   case 8: // august
   case 10: // oct
   case 12: // dec
      return 31;
   case 4: // april
   case 6: // june
   case 9: // sept
   case 11: // nov
      return 30;
   default:
      return 0;
   }
}

/***********************************************************************
Page callback
*/
BOOL DateCallback (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDATEC pc = (PDATEC) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_ESCAPE;
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);
         a.c = L' ';
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);
         a.c = VK_RETURN;
         pPage->m_listESCACCELERATOR.Add (&a);

         // if Date is blank then set that, else set minutes/hours
         PCEscControl   pControl;
         if (ISBLANK) {
            pControl = pPage->ControlFind(L"blank");
            if (pControl) {
               pControl->AttribSetBOOL (gszChecked, TRUE);
               pPage->FocusSet (pControl);
            }

            pControl = pPage->ControlFind (L"year");
            if (pControl)
               pControl->Enable (FALSE);
         }
         else {
            // hour/minute
            WCHAR szTemp[16];

            swprintf (szTemp, L"d%d", pc->iDay);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               pControl->AttribSetBOOL (gszChecked, TRUE);
               pPage->FocusSet (pControl);
            }

            swprintf (szTemp, L"m%d", pc->iMonth);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (gszChecked, TRUE);

            pControl = pPage->ControlFind (L"year");
            if (pControl)
               pControl->AttribSetInt (L"text", pc->iYear);
         }

         DropDownFitToScreen(pPage);

      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         PCEscControl pControl;

         // if pressed "blank" then special
         if (!wcsicmp(p->pControl->m_pszName, L"blank")) {
            WCHAR szTemp[16];

            // if turned blank on, no Date for minutes/seconds
            if (p->pControl->AttribGetBOOL (gszChecked)) {
               if (pc->iDay >= 1) {
                  swprintf (szTemp, L"d%d", pc->iDay);
                  pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->AttribSetBOOL (gszChecked, FALSE);
               }
               if (pc->iMonth >= 1) {
                  swprintf (szTemp, L"m%d", pc->iMonth);
                  pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->AttribSetBOOL (gszChecked, FALSE);
               }
               pControl = pPage->ControlFind(L"year");
               if (pControl) {
                  pControl->AttribSet (L"text", L"");
                  pControl->Enable(FALSE);
               }

               pc->iDay = pc->iMonth = pc->iYear = -1;
            }
            else {
               // if turned blank off, set to today
               SYSTEMTIME st;
               GetLocalTime (&st);

               if (pc->iDay < 1)
                  pc->iDay = (int) st.wDay;
               if (pc->iMonth < 1)
                  pc->iMonth = (int) st.wMonth;
               if (pc->iYear < 1)
                  pc->iYear = (int) st.wYear;

               // because changing this might change the display for
               // the month, must exit the dialog and restart
               pPage->Exit(L"notblank");
               return TRUE;
            }
         }
         else {
            // only care if it's checked
            if (!p->pControl->AttribGetBOOL (gszChecked))
               return TRUE;

            // which one is it
            PWSTR psz, pEnd;
            psz = p->pControl->m_pszName;
            if (!psz)
               return TRUE;
            if (psz[0] == L'd') {
               pc->iDay = wcstol (psz+1, &pEnd, 10);

               // if nothing else is blank then done
               if (!ISBLANK)
                  return TRUE;
            }
            else if (psz[0] == L'm') {
               pc->iMonth = wcstol (psz+1, &pEnd, 10);
            }

            // if anything else is -1 then fix
            SYSTEMTIME  st;
            GetLocalTime (&st);
            if (pc->iDay < 1)
               pc->iDay = 1;
            if (pc->iMonth < 1)
               pc->iMonth = (int)st.wMonth;
            if (pc->iYear < 0) {
               pc->iYear = (int)st.wYear;
            }

            // in this case need to exit because a changed month means
            // that the days may change
            pPage->Exit(L"newmonth");
            return TRUE;

         }
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         // if the year is changed and it's 4 digits then need to refresh
         WCHAR szTemp[16];
         DWORD dwNeeded;
         szTemp[0] = 0;
         p->pControl->AttribGet (L"text", szTemp, sizeof(szTemp), &dwNeeded);

         int   iYear;
         PWSTR pEnd;
         iYear = wcstol (szTemp, &pEnd, 10);
         if ((iYear >= 1000) && (iYear <= 9999)) {
            pc->iYear = iYear;

            // if anything else is -1 then fix
            SYSTEMTIME  st;
            GetLocalTime (&st);
            if (pc->iDay < 1)
               pc->iDay = 1;
            if (pc->iMonth < 1)
               pc->iMonth = (int)st.wMonth;
            if (pc->iYear < 0) {
               pc->iYear = (int)st.wYear;
            }

            // in this case need to exit because a changed month means
            // that the days may change
            pPage->Exit(L"newmonth");
            return TRUE;
         }
      }
      return TRUE;
   }

   return FALSE;
}


/***********************************************************************
Control callback
*/
BOOL ControlDate (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   DATEC *pc = (DATEC*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(DATEC));
         pc = (DATEC*) pControl->m_mem.p;
         memset (pc, 0, sizeof(DATEC));

         pc->iMonth = pc->iYear = pc->iDay = -1;
         pc->fShowDayOfWeek = TRUE;
         pc->fEnterDay = TRUE;
         wcscpy (pc->szBlank, L"Not entered");

         // attributes
         pControl->AttribListAddDecimal (L"day", &pc->iDay, NULL, TRUE);
         pControl->AttribListAddDecimal (L"month", &pc->iMonth, NULL, TRUE);
         pControl->AttribListAddDecimal (L"year", &pc->iYear, NULL, TRUE);
         pControl->AttribListAddBOOL (L"showdayofweek", &pc->fShowDayOfWeek, NULL, TRUE);
         pControl->AttribListAddBOOL (L"enterday", &pc->fEnterDay, NULL, TRUE);
         pControl->AttribListAddString (L"blank", pc->szBlank, sizeof(pc->szBlank), FALSE, TRUE);

         // constructor for dropdown
         DropDownMessageHandler (pControl, dwMessage, pParam, &pc->dd);
      }
      return TRUE;

   case ESCM_DROPDOWNOPENED:
      {
         PESCMDROPDOWNOPENED p = (PESCMDROPDOWNOPENED)pParam;

         // put together the MML text to be used
         CMem  mem;

regenerate:
         mem.m_dwCurPosn = 0;

         // macro definitions and headers
         mem.StrCat (
            L"<!xDayButton>"
               L"<td width=15% bgcolor=#d0d0ff lrmargin=0 tbmargin=0 MACROATTRIBUTE=1>"
                  L"<Button MACROATTRIBUTE=1 showbutton=false width=100% radiobutton=true highlightcolor=#ff8000 margintopbottom=2 marginleftright=2 "
                  L"group=\"d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,d16,d17,d18,d19,d20,d21,d22,d23,d24,d25,d26,d27,d28,d29,d30,d31\">"
                     L"<align align=center><font MACROATTRIBUTE=1><?MacroContent?></font></align>"
                  L"</button>"
               L"</td>"
            L"</xDayButton>"
            L"<!xDayBlank>"
               L"<td width=15%>"
               L"</td>"
            L"</xDayBlank>"
            L"<!xMonthButton>"
               L"<td width=33% lrmargin=0 tbmargin=0 MACROATTRIBUTE=1>"
                  L"<Button MACROATTRIBUTE=1 showbutton=false width=100% radiobutton=true highlightcolor=#ff8000 margintopbottom=2 marginleftright=2 "
                  L"group=\"m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12\">"
                     L"<align align=center><font MACROATTRIBUTE=1><?MacroContent?></font></align>"
                  L"</button>"
               L"</td>"
            L"</xMonthButton>"
            );

         // background color
         WCHAR szTemp[512];
         WCHAR szColor[16], szColor2[16];
         ColorToAttrib (szColor, pc->dd.cTBackground);
         ColorToAttrib (szColor2, pc->dd.cBBackground);
         swprintf (szTemp, L"<colorblend posn=background tcolor=%s bcolor=%s/>",
            szColor, szColor2);
         mem.StrCat (szTemp);

         mem.StrCat (
            L"<small>"
            );

         // more default string
         if (pc->fEnterDay) mem.StrCat (
            L"<table width=200 lrmargin=2 tbmargin=2 align=center valign=center>"
               L"<tr>"
                  L"<td width=15% bgcolor=#004000>"
                     L"<font color=#ffffff>S</font>"
                  L"</td>"
                  L"<td width=15% bgcolor=#004000>"
                     L"<font color=#ffffff>M</font>"
                  L"</td>"
                  L"<td width=15% bgcolor=#004000>"
                     L"<font color=#ffffff>T</font>"
                  L"</td>"
                  L"<td width=15% bgcolor=#004000>"
                     L"<font color=#ffffff>W</font>"
                  L"</td>"
                  L"<td width=15% bgcolor=#004000>"
                     L"<font color=#ffffff>Th</font>"
                  L"</td>"
                  L"<td width=15% bgcolor=#004000>"
                     L"<font color=#ffffff>F</font>"
                  L"</td>"
                  L"<td width=15% bgcolor=#004000>"
                     L"<font color=#ffffff>S</font>"
                  L"</td>"
               L"</tr>"
            );

         // days in this month
         int   iMonth, iDay, iYear, iFirstDayOfMonth, iNumDays;
         iMonth = pc->iMonth;
         iDay = pc->iDay;
         iYear = pc->iYear;
         SYSTEMTIME  st;
         GetLocalTime (&st);
         if ((iMonth < 1) || (iMonth > 12))
            iMonth = (int)st.wMonth;
         if (iYear < 0)
            iYear = (int)st.wYear;
         if ((iDay < 1) || (iDay > 31))
            iDay = (int)st.wDay;
         iNumDays = EscDaysInMonth (iMonth, iYear, &iFirstDayOfMonth);
         if (iDay > iNumDays)
            iDay = iNumDays;

         // if it's not blank then set values
         if (!ISBLANK) {
            pc->iMonth = iMonth;
            pc->iDay = iDay;
            pc->iYear = iYear;
         }
         else {
            pc->iMonth = pc->iDay = pc->iYear = -1;
         }

         // start before day 1 often
         int   iCurDay;
         iCurDay = 1 - iFirstDayOfMonth;
         if (pc->fEnterDay) while (iCurDay < iNumDays) {
            int   i;

            // add tr
            mem.StrCat (L"<tr>");

            for (i = 0; i < 7; i++) {
               if ((i+iCurDay < 1) || (i+iCurDay > iNumDays)) {
                  // add a blank
                  mem.StrCat (L"<xDayBlank/>");
                  continue;
               }

               // else, generate a button
               // if it's today's date then bold-face
               swprintf (szTemp,
                  ((pc->iMonth == (int)st.wMonth) && (pc->iYear == (int)st.wYear) && (i+iCurDay == (int)st.wDay)) ?
                     L"<xDayButton name=d%d %s><bold>%d</bold></xDayButton>" :
                     L"<xDayButton name=d%d %s>%d</xDayButton>",
                  i+iCurDay, ((i == 0) || (i == 6)) ? L"bgcolor=#8080c0" : L"",
                  i+iCurDay);

               // add it
               mem.StrCat (szTemp);
            }

            // add end row
            mem.StrCat (L"</tr>");
            iCurDay += 7;
         }

         if (pc->fEnterDay) mem.StrCat (
            L"</table>"
            L"<br/>"
            );

         // end of days and start of month
         mem.StrCat (
            L"<table width=200 lrmargin=2 tbmargin=2 align=center valign=center>"
               L"<colorblend posn=background color=#d0d0ff rcolor=#8080c0/>"
            );

         // months
         DWORD i, j;
         for (i = 0; i < 4; i++) {
            // add tr
            mem.StrCat (L"<tr>");

            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"<xMonthButton name=m%d>%s</xMonthButton>",
                  i*3+j+1, gaszMonthAbbrev[i*3+j]);
               // add it
               mem.StrCat (szTemp);
            }

            // add end row
            mem.StrCat (L"</tr>");
         }

         // year and blank
         mem.StrCat (
            L"</table>"
            L"<br/>"
            L"<table width=200 lrmargin=0 tbmargin=0 align=center valign=center border=0 innerlines=0>"
               L"<tr>"
                  L"<td width=33%>"
                     L"<Edit name=year width=100% maxchars=5/>"
                  L"</td>"
                  L"<td width=17%>"
                  L"</td>"
                  L"<td width=50% bgcolor=#d0d0ff >"
                     L"<Button name=blank showbutton=false width=100% checkbox=true highlightcolor=#ff8000>"
                     L"<align align=center>"
            );

         // blank name
         // when append szBlank here and in time control need to clean
         // up so it doesn't contain any tags
         WCHAR szMML[512];
         DWORD dwNeeded;
         szMML[0] = 0;
         StringToMMLString (pc->szBlank, szMML, sizeof(szMML), &dwNeeded);
         mem.StrCat (szMML);

         // ending
         mem.StrCat (
                     L"</align>"
                  L"</button>"
               L"</td>"
            L"</tr>"
            L"</table>"
            L"</small>"
            );

         // done. Append the NULL
         mem.CharCat (0);

         // bring up the window
         WCHAR    *psz;

         psz = p->pWindow->PageDialog ((PWSTR)mem.p, DateCallback, (PVOID) pc);

         // keep bringing up until get a close
         if (psz && (psz[0] != L'['))
            goto regenerate;
         
         // hide the window so it's not visible is a call is made from this
         p->pWindow->ShowWindow (SW_HIDE);

         // invalidate the control since its contents have changed
         pControl->Invalidate();

         // alert the app
         ESCNDATECHANGE tc;
         memset (&tc, 0, sizeof(tc));
         tc.iDay = pc->iDay;
         tc.iMonth = pc->iMonth;
         tc.iYear = pc->iYear;
         tc.pControl = pControl;
         pControl->MessageToParent (ESCN_DATECHANGE, &tc);
      }
      return TRUE;

   case ESCM_DROPDOWNTEXTBLOCK:
      {
         PESCMDROPDOWNTEXTBLOCK p = (PESCMDROPDOWNTEXTBLOCK) pParam;

         // determine what we should say
         WCHAR szTemp[64];
         if (ISBLANK) {
            wcscpy (szTemp, pc->szBlank);
         }
         else {
            szTemp[0] = 0;

            // show day of week
            if (pc->fShowDayOfWeek && pc->fEnterDay) {
               int   iDays, iStart;
               iDays = EscDaysInMonth (pc->iMonth, pc->iYear, &iStart);

               int   idow;
               idow = (iStart + pc->iDay - 1) % 7;
               wcscpy(szTemp, gaszDayOfWeek[idow]);
               wcscat (szTemp, L", ");
            }

            if (pc->fEnterDay)
               swprintf (szTemp + wcslen(szTemp), L"%d %s %d",
                  pc->iDay, gaszMonth[(pc->iMonth - 1)%12], pc->iYear);
            else
               swprintf (szTemp + wcslen(szTemp), L"%s %d",
                  gaszMonth[(pc->iMonth - 1)%12], pc->iYear);
         }

         // if this isn't forced and this string is identical to the old
         // one then just accept it
         if (!p->fMustSet && !wcsicmp(szTemp, pc->szCurDisplay))
            return TRUE;

         // else set it
         wcscpy (pc->szCurDisplay, szTemp);
         p->pszText = pc->szCurDisplay;
      }
      return TRUE;

   }

   return DropDownMessageHandler (pControl, dwMessage, pParam, pc ? &pc->dd : NULL);
}






