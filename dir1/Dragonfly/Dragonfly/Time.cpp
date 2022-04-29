/*************************************************************************
Time.cpp - For time zones, sunlight, phases of the moon.

begun Mar-23-2001 by Mike Rozak
Copyright 2001 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


/* time zone structure */
typedef struct {
   DWORD       dwRegion;      //0=pacific islands,1=NA,2=CA,3=SA,4=Mid-atlantic,5=Europe,6=Africa,7=Midest,8=Russia,9=India,10=Asia,11=Oceania
   PWSTR       pszCities;    // name of cities
   BOOL        fNorth;        // TRUE if northen hemisphere, FALSE if southern
   BOOL        fDaylight;     // TRUE if daylight savings time
   int         iMinutes;      // minutes off
   BOOL        fShown;        // TRUE if shown
} TIMEZ, *PTIMEZ;

typedef struct {
   DWORD       dwRegion;      //0=pacific islands,1=NA,2=CA,3=SA,4=Mid-atlantic,5=Europe,6=Africa,7=Midest,8=Russia,9=India,10=Asia,11=Oceania
   WCHAR       szCities[64];    // name of cities
   BOOL        fNorth;        // TRUE if northen hemisphere, FALSE if southern
   BOOL        fDaylight;     // TRUE if daylight savings time
   int         iMinutes;      // minutes off
} TIMEZ2, *PTIMEZ2;

/* timezones */
TIMEZ    gaTimeZones[] = {
   0, L"Eniwetok, Kwajalein", TRUE, FALSE, -720, TRUE,
   0, L"Midway Island, Samoa", TRUE, FALSE, -660, TRUE,
   0, L"Hawaii", TRUE, FALSE, -600, TRUE,
   1, L"Alaska", TRUE, TRUE, -540, TRUE,
   1, L"Pacific time (US and Canada)", TRUE, TRUE, -480, TRUE,
   1, L"Arizona", TRUE, FALSE, -420, TRUE,
   1, L"Mountain time (US and Canada)", TRUE, TRUE, -420, TRUE,
   1, L"Central time (US and Canada)", TRUE, TRUE, -360, TRUE,
   1, L"Saskatchewan", TRUE, TRUE, -360, TRUE,
   1, L"Eastern time (US and Canada)", TRUE, TRUE, -300, TRUE,
   1, L"Indiana", TRUE, FALSE, -300, TRUE,
   1, L"Atlantic time (Canada)", TRUE, TRUE, -240, TRUE,
   1, L"Newfoundland", TRUE, TRUE, -210, TRUE,
   2, L"Tijuana", TRUE, TRUE, -480, TRUE,
   2, L"Mexico City", TRUE, FALSE, -360, TRUE,
   3, L"Bogota, Lima", FALSE, FALSE, -300, TRUE,
   3, L"Caracas, La Paz", FALSE, FALSE, -240, TRUE,
   3, L"Brasilia", FALSE, FALSE, -180, TRUE,
   3, L"Buenos Aires, Georgetown", FALSE, TRUE, -180, TRUE,
   4, L"Mid-Atlantic", FALSE, FALSE, -120, TRUE,
   5, L"Azores, Cape Verde Island", FALSE, FALSE, -60, TRUE,
   5, L"Dublin, Edinburgh, London, Lisbon", TRUE, TRUE, 0, TRUE,
   5, L"Monrovia, Casablanca", TRUE, FALSE, 0, TRUE,
   5, L"Berlin, Stockholm, Rome, Bern, Brussels, Vienna, Amsterdam", TRUE, TRUE, 60, TRUE,
   5, L"Paris, Madrid", TRUE, FALSE, 60, TRUE,
   5, L"Prague, Bratislava", TRUE, TRUE, 60, TRUE,
   5, L"Warsaw", TRUE, TRUE, 60, TRUE,
   5, L"Athens, Helsinki, Istanbul", TRUE, TRUE, 120, TRUE,
   5, L"Eastern Europe", TRUE, TRUE, 120, TRUE,
   6, L"Cairo", TRUE, FALSE, 120, TRUE,
   6, L"Harare, Pretoria", FALSE, TRUE, 120, TRUE,
   7, L"Israel", TRUE, FALSE, 120, TRUE,
   7, L"Baghdad, Kuwait, Niarobi, Riyadh", TRUE, FALSE, 180, TRUE,
   7, L"Tehran", TRUE, FALSE, 210, TRUE,
   7, L"Abu Dhabi, Muscat, Tbilisi, Kazan", TRUE, FALSE, 240, TRUE,
   7, L"Kabul", TRUE, FALSE, 270, TRUE,
   8, L"Volgograd", TRUE, FALSE, 240, TRUE,
   8, L"Moscow, St. Petersburg", TRUE, TRUE, 180, TRUE,
   8, L"Yakutsk", TRUE, TRUE, 540, TRUE,
   9, L"Islamabad, Karachi, Ekaterinburg, Tashkent", TRUE, FALSE, 300, TRUE,
   9, L"Bombay, Calcutta, Madras, New Delhi, Colombo", TRUE, FALSE, 330, TRUE,
   9, L"Almaty, Dhaka", TRUE, FALSE, 360, TRUE,
   10, L"Bankok, Jakarta, Hanoi", TRUE, FALSE, 420, TRUE,
   10, L"Beijing, Chongqing, Urumqi", TRUE, TRUE, 480, TRUE,
   10, L"Hong Kong, Singapore, Taipei", FALSE, FALSE, 480, TRUE,
   10, L"Tokyo, Osaka, Sapporo, Seoul", TRUE, TRUE, 540, TRUE,
   11, L"Perth", FALSE, FALSE, 480, TRUE,
   11, L"Adelaide", FALSE, TRUE, 570, TRUE,
   11, L"Darwin", FALSE, FALSE, 570, TRUE,
   11, L"Brisbane", FALSE, FALSE, 600, TRUE,
   11, L"Canberra, Melbourne, Sydney", FALSE, TRUE, 600, TRUE,
   11, L"Guam, Port Moresby, Vladivostok", FALSE, FALSE, 600, TRUE,
   11, L"Hobart", FALSE, TRUE, 600, TRUE,
   11, L"Magadan, Soloman Islands, New Calendonia", FALSE, FALSE, 660, TRUE,
   11, L"Fiji, Kamchatka, Marshall Islands", FALSE, FALSE, 720, TRUE,
   11, L"Wellington, Auckland", FALSE, TRUE, 720, TRUE
};

static WCHAR gszTimeZone[] = L"TimeZone";

static CListFixed glistTIMEZ2;

/*************************************************************************
TimeZoneSort - Sorts the time zones.
*/
int __cdecl TZSortCallback(const void *elem1, const void *elem2 )
{
   PTIMEZ2 p1, p2;
   p1 = (PTIMEZ2)elem1;
   p2 = (PTIMEZ2)elem2;

   if (p1->dwRegion != p2->dwRegion)
      return (int)p2->dwRegion - (int)p1->dwRegion;
   if (p1->iMinutes != p2->iMinutes)
      return (int)p2->iMinutes - (int)p1->iMinutes;
   return _wcsicmp(p1->szCities, p2->szCities);
}
void TimeZoneSort (void)
{
   qsort (glistTIMEZ2.Get(0), glistTIMEZ2.Num(), sizeof(TIMEZ2), TZSortCallback);
}

/*************************************************************************
TimeZoneDefault - Fills in the global time zone information with defaults.
*/
void TimeZoneDefault (void)
{
   HANGFUNCIN;
   glistTIMEZ2.Init(sizeof(TIMEZ2));
   glistTIMEZ2.Clear();

   TIMEZ2   tz;
   DWORD i;
   for (i = 0; i < sizeof(gaTimeZones)/sizeof(TIMEZ); i++) {
      tz.dwRegion = gaTimeZones[i].dwRegion;
      tz.fDaylight = gaTimeZones[i].fDaylight;
      tz.fNorth = gaTimeZones[i].fNorth;
      tz.iMinutes = gaTimeZones[i].iMinutes;
      wcscpy (tz.szCities, gaTimeZones[i].pszCities);
      glistTIMEZ2.Add (&tz);
   }

   TimeZoneSort();
}


/*************************************************************************
TimeZoneInit - Loads in the time zones. If it cant find any it uses defaults.
*/
void TimeZoneInit (void)
{
   HANGFUNCIN;
   // if already have stuff dont bother
   if (glistTIMEZ2.Num())
      return;

   // else init
   glistTIMEZ2.Init (sizeof(TIMEZ2));


   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszTimeZoneNode);
   if (!pNode) {
      TimeZoneDefault();
      return;   // unexpected error
   }

   // get the number
   DWORD dwNum;
   dwNum = (DWORD) NodeValueGetInt (pNode, gszNumber, 0);

   // loop
   DWORD dwIndex;
   int   iNumber;
   TIMEZ2   tz;
   dwIndex = 0;
   while (TRUE) {
      PWSTR psz;
      psz = NodeElemGet (pNode, gszTimeZone, &dwIndex, &iNumber, &tz.dwRegion, (DWORD*) &tz.fDaylight, (DWORD*) &tz.fNorth, &tz.iMinutes);
      if (!psz)
         break;
      if ((DWORD)iNumber >= dwNum)
         continue;   // skip this

      wcscpy (tz.szCities, psz);
      glistTIMEZ2.Add(&tz);
   }

   gpData->NodeRelease(pNode);

   // sort
   TimeZoneSort();

   // if nothing then defaults
   if (!glistTIMEZ2.Num())
      TimeZoneDefault();
}



/*************************************************************************
TimeZoneWrite - Writes out the time zones.
*/
void TimeZoneWrite (void)
{
   HANGFUNCIN;
   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszTimeZoneNode);
   if (!pNode) {
      return;   // unexpected error
   }

   // write the number
   NodeValueSet (pNode, gszNumber, (int) glistTIMEZ2.Num());

   // loop
   DWORD i;
   for (i = 0; i < glistTIMEZ2.Num(); i++) {
      PTIMEZ2  p = (PTIMEZ2) glistTIMEZ2.Get(i);
      NodeElemSet (pNode, gszTimeZone, p->szCities, (int)i, TRUE, p->dwRegion, p->fDaylight, p->fNorth, p->iMinutes);
   }

   gpData->NodeRelease(pNode);

}


/*************************************************************************
GetTimeAndDate - Gets the time and date given the time zone.

inputs
   int      iBias - Bias in minutes from GMT.
   BOOL     fDaylight - If true assume daylight savings is used.
   BOOL     fSouthern - TRUE if it's in the southern hemisphere, false if northern.
               Used for daylight savings.
   DFDATE   *pDate - Filled in with the date
   DFTIME   *pTime - Filled in with the time
*/
void GetTimeAndDate (int iBias, BOOL fDaylight, BOOL fSouthern, DFDATE *pDate, DFTIME *pTime)
{
   HANGFUNCIN;
   // find the time at the location
   SYSTEMTIME st;
   GetSystemTime (&st);
   DFDATE date;
   DFTIME time;
   __int64 iMin;
   date = TODFDATE (st.wDay, st.wMonth, st.wYear);
   time = TODFTIME (st.wHour, st.wMinute);
   iMin = DFDATEToMinutes (date) + DFTIMEToMinutes (time) + iBias;

   // work back to date
   date = MinutesToDFDATE (iMin);
   iMin -= DFDATEToMinutes(date);
   time = TODFTIME((int)iMin / 60, (int) iMin % 60);

   // changeover
   DFDATE April, October;
   DFTIME TwoAM;
   April = TODFDATE (ReoccurFindNth(4, YEARFROMDFDATE(date), 0 /*first*/, 3/*sunday*/), 4, YEARFROMDFDATE(date));
   October = TODFDATE (ReoccurFindNth(10, YEARFROMDFDATE(date), 4 /*last*/, 3/*sunday*/), 10, YEARFROMDFDATE(date));
   TwoAM = TODFTIME(2,0);

   // do daylight savings
   if (fDaylight) {
      BOOL fMoveBackHour = FALSE;

      if (!fSouthern) {
         fMoveBackHour |= (date > April) && (date < October);
         fMoveBackHour |= ((date == April) && (time >= TwoAM));
         fMoveBackHour |= ((date == October) && (time < TwoAM));
      }
      else {   // southern
         fMoveBackHour |= (date > October);
         fMoveBackHour |= (date < April);
         fMoveBackHour |= ((date == October) && (time >= TwoAM));
         fMoveBackHour |= ((date == April) && (time < TwoAM));
      }

      if (fMoveBackHour) {
         // BGUFIX - Changed from -60 to +60 since was saying East Coast US was 4:30 PM
         // when it was 8AM in Darwin. Should say 6:30AM. In June.
         iMin = DFDATEToMinutes (date) + DFTIMEToMinutes (time) + 60;

         // work back to date
         date = MinutesToDFDATE (iMin);
         iMin -= DFDATEToMinutes(date);
         time = TODFTIME((int)iMin / 60, (int) iMin % 60);
      }
   }


   *pDate = date;
   *pTime = time;
}


/*****************************************************************************
TimeZoneAddPage - Override page callback.
*/
BOOL TimeZoneAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
            TIMEZ2   tz;
            memset (&tz, 0, sizeof(tz));

            PCEscControl pControl;
            DWORD dwNeeded;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, tz.szCities, sizeof(tz.szCities),&dwNeeded);
            if (!tz.szCities[0]) {
               pPage->MBWarning (L"You must type in a name for the time zone.");
               return TRUE;
            }

            tz.iMinutes = GetCBValue(pPage, L"timezone", 0);
            tz.dwRegion = (DWORD)GetCBValue(pPage, L"region", 0);
            tz.fNorth = (BOOL)GetCBValue(pPage, L"hemisphere", 1);
            tz.fDaylight = (BOOL)GetCBValue(pPage, L"daylight", 1);

            // save it
            glistTIMEZ2.Add (&tz);
            TimeZoneSort ();
            TimeZoneWrite ();

            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
TimeZoneAddUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project task was added
*/
BOOL TimeZoneAddUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, gpWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTIMEZONEADD, TimeZoneAddPage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}


/*****************************************************************************
TimeZonesPage - Override page callback.
*/
BOOL TimeZonesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         if ((p->psz[0] == L'm') && (p->psz[1] == 'x') && (p->psz[2] == ':')) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to delete this time zone?"))
               return TRUE;
            glistTIMEZ2.Remove (_wtoi(p->psz+3));
            TimeZoneWrite();
            pPage->Exit(gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"restore")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to restore the original time zones?",
               L"Any changes you have made will permanently be lost."))
               return TRUE;
            TimeZoneDefault();
            TimeZoneWrite();
            pPage->Exit(gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"add")) {
            if (TimeZoneAddUI (pPage))
               pPage->Exit(gszRedoSamePage);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (p->pszSubName && !_wcsicmp(p->pszSubName, L"TIMEZONES")) {
            MemZero (&gMemTemp);

            // load this in
            TimeZoneInit();

            DFDATE today;
            today = Today();
            // loop
            DWORD dwLastRegion, i;
            dwLastRegion = (DWORD)-1;
            for (i = 0; i < glistTIMEZ2.Num(); i++) {
               PTIMEZ2  p = (PTIMEZ2) glistTIMEZ2.Get(i);

               if (p->dwRegion != dwLastRegion) {
                  dwLastRegion = p->dwRegion;
                  PWSTR pszRegions[12] = {
                     L"Pacific Islands",
                     L"North America",
                     L"Central America",
                     L"South America",
                     L"Mid-Atlantic",
                     L"Europe",
                     L"Africa",
                     L"Middle East",
                     L"Russia",
                     L"Indian subcontinent",
                     L"Asia",
                     L"Oceania"
                  };

                  MemCat (&gMemTemp, L"<xtrheader>");
                  MemCat (&gMemTemp, pszRegions[dwLastRegion]);
                  MemCat (&gMemTemp, L"</xtrheader>");
               }

               // display
               MemCat (&gMemTemp, L"<tr><xTimeZone>");

               // option to delete
               MemCat (&gMemTemp, L"<image bmpresource=237 border=0 transparent=true href=mx:");
               MemCat (&gMemTemp, i);
               MemCat (&gMemTemp, L"><xhoverhelpshort>Removes the time zone from the list.</xhoverhelpshort></image>    ");

               MemCatSanitize (&gMemTemp, p->szCities);
               MemCat (&gMemTemp, L"</xTimeZone><xTimeInZone>");

               // get the time
               DFDATE date;
               DFTIME time;
               GetTimeAndDate (p->iMinutes, p->fDaylight, !p->fNorth,
                  &date, &time);

               // time
               WCHAR szTemp[16];
               DFTIMEToString (time, szTemp);
               MemCat (&gMemTemp, szTemp);

               // indicate if it's yesterday or tomorrow
               if (date < today)
                  MemCat (&gMemTemp, L" (Yesterday)");
               else if (date > today)
                  MemCat (&gMemTemp, L" (Tomorrow)");

               MemCat (&gMemTemp, L"</xTimeInZone></tr>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************
PhaseOfMoon - Given a date and time, this returns the phase of the mooon.

inputs
   DFDATE      date - date
   DFTIME      time - time
returns
   double - Phase. 0 = no light. 0.5 = Full light, 1.0 = no light.
*/
double PhaseOfMoon (DFDATE date, DFTIME time)
{
   HANGFUNCIN;
   __int64 iRef, iCur;

   // adjust the date and time for the time zone, GMT
   iCur = DFDATEToMinutes (date) + DFTIMEToMinutes(time);
   TIME_ZONE_INFORMATION tzi;
   memset (&tzi, 0, sizeof(tzi));
   GetTimeZoneInformation (&tzi);
   iCur -= tzi.Bias;

   // reference is new moon sunday, Mar 25, 20001, 00:46 GMT.
   iRef = DFDATEToMinutes (TODFDATE(25,3,2001)) + DFTIMEToMinutes(TODFTIME(0,46));

   // how many minutes in a moon cycle
   int   iMoon;
   iMoon = (29 * 24 + 12) * 60 + 44;

   // so that means
   iRef = iCur - iRef;
   // fix broken mod
   if (iRef < 0)
      iRef += ((int)(-iRef) / iMoon + 1) * iMoon;
   iRef = iRef % iMoon;

   return (double) (int) iRef / (double) iMoon;
}

/***********************************************************************
TimeMoonPage - Page callback for viewing a phone message
*/
BOOL TimeMoonPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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

         gdateCalendar = TODFDATE (1, p->iMonth, p->iYear);
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
            // how many days in the month
            if (gdateCalendar == -1)
               gdateCalendar = today;
            int iDays, iStartDOW, iMonth, iYear;
            iDays = EscDaysInMonth (iMonth = MONTHFROMDFDATE(gdateCalendar),
               iYear = YEARFROMDFDATE(gdateCalendar), &iStartDOW);
            if (!iDays) {
               iDays = 31;
               iStartDOW = 0;
            }

            // determine what links should have for reminders, task, etc.

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

                  if ( (MONTHFROMDFDATE(gdateCalendar) == MONTHFROMDFDATE(today)) &&
                     (YEARFROMDFDATE(gdateCalendar) == YEARFROMDFDATE(today)) &&
                     ((int) DAYFROMDFDATE(today) == iDay))
                     MemCat (&gMemTemp, L"<xday bgcolor=#ffff40>");
                  else
                     MemCat (&gMemTemp, L"<xday>");


                  // Draw moon
                  double fPhase;
                  fPhase = PhaseOfMoon (TODFDATE(iDay, iMonth, iYear), TODFTIME(20,0));
                  WCHAR szTemp[64];
                  swprintf (szTemp, L"%f,0,%f", (double) sin(fPhase*2*3.14159), (double)-cos(fPhase*2*3.14159));
                  MemCat (&gMemTemp, L"<xMoon point=");
                  MemCat (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L"/>");

                  // show the number
                  MemCat (&gMemTemp, L"<big><big><big><bold>");

                  // clicking goes to combo view
                  MemCat (&gMemTemp, L"<a color=#8080ff href=cd:");
                  MemCat (&gMemTemp, (int) TODFDATE(iDay, iMonth, iYear));
                  MemCat (&gMemTemp, L">");
                  MemCat (&gMemTemp, iDay);
                  MemCat (&gMemTemp, L"</a>");

                  MemCat (&gMemTemp, L"</bold></big></big></big>");


                  MemCat (&gMemTemp, L"</xday>");
               }

               MemCat (&gMemTemp, L"</tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
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

            gdateCalendarCombo = d;
            pPage->Exit (L"r:258");
            return TRUE;
         }
      }
      break;


   };

   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
TimeSunlightPage - Page callback for viewing a phone message
*/
BOOL TimeSunlightPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"LIGHTROT")) {
            MemZero (&gMemTemp);

            SYSTEMTIME st;
            GetSystemTime (&st);
            WCHAR szTemp[128];
            double fRot;

            // rotate the light counterclockwise as the day progresses
            fRot = -((double) st.wHour + (double) st.wMinute / 60.0) / 24.0 * 360;
            swprintf (szTemp, L"<rotatey val=%f/>", (double) fRot);
            MemCat (&gMemTemp, szTemp);

            // BUGFIX - Moved this after to make it right
            // rotate for the seasons
            // st.wMonth = 12;
            st.wMonth -= 1;
            fRot = st.wMonth + st.wDay / 31.0 - (2 + 22.0 / 31.0);   // normalize so March 22 (equinox) is 0
            fRot = fRot / 12 * 2 * 3.1415;
            fRot = sin(fRot) * 22.5;
            swprintf (szTemp, L"<rotatez val=%f/>", (double) fRot);
            MemCat (&gMemTemp, szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TIMEZONEROT")) {
            MemZero (&gMemTemp);

            TIME_ZONE_INFORMATION tzi;
            memset (&tzi, 0, sizeof(tzi));
            GetTimeZoneInformation (&tzi);
            WCHAR szTemp[128];
            double fRot;

            // rotate so users time zone is centered
            fRot = 90 /* to center on GMT */ + tzi.Bias / 60.0 / 24.0 * 360.0;
            swprintf (szTemp, L"<rotatey val=%f/>", (double) fRot);
            MemCat (&gMemTemp, szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;


   };

   return DefPage (pPage, dwMessage, pParam);
}
