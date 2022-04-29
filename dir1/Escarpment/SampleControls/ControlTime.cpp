/***********************************************************************
ControlTime.cpp - Code to get the time from a user.

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
   int            iHour;   // hour attribute, 0..23. -1 for blank
   int            iMinute; // minute attribute, 0..59, -1 for blank
   BOOL           fIgnorePress;  // ignore the press when first setting
   WCHAR          szBlank[64];   // name used if the hour/minute aren't specified

   WCHAR          szCurDisplay[64]; // current time displayed
} TIME, *PTIME;

static WCHAR gszChecked[] = L"checked";

/***********************************************************************
Page callback
*/
BOOL TimeCallback (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTIME pc = (PTIME) pPage->m_pUserData;

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

         // if time is blank then set that, else set minutes/hours
         PCEscControl   pControl;
         if ((pc->iHour < 0) || (pc->iMinute < 0)) {
            pControl = pPage->ControlFind(L"blank");
            if (pControl) {
               pControl->AttribSetBOOL (gszChecked, TRUE);
               pPage->FocusSet (pControl);
            }
         }
         else {
            // hour/minute
            WCHAR szTemp[16];

            pc->iHour = pc->iHour % 24;
            pc->iMinute = pc->iMinute % 60;

            swprintf (szTemp, L"h%d", pc->iHour);
            pControl = pPage->ControlFind (szTemp);
            pc->fIgnorePress = TRUE;
            if (pControl) {
               pControl->AttribSetBOOL (gszChecked, TRUE);
               pPage->FocusSet (pControl);
            }
            pc->fIgnorePress = FALSE;

            int   iMinute;
            iMinute = (pc->iMinute / 15) * 15;
            swprintf (szTemp, L"m%d%d", iMinute / 10, iMinute%10); // 15-minute increments
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (gszChecked, TRUE);
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

            // if turned blank on, no time for minutes/seconds
            if (p->pControl->AttribGetBOOL (gszChecked)) {
               if (pc->iHour >= 0) {
                  swprintf (szTemp, L"h%d", pc->iHour);
                  pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->AttribSetBOOL (gszChecked, FALSE);
               }
               if (pc->iMinute >= 0) {
                  int   iMinute;
                  iMinute = (pc->iMinute / 15) * 15;
                  swprintf (szTemp, L"m%d%d", iMinute / 10, iMinute%10); // 15-minute increments
                  pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->AttribSetBOOL (gszChecked, FALSE);
               }

               pc->iHour = pc->iMinute = -1;
            }
            else {
               // if turned blank off, set minutes/seconds
               SYSTEMTIME st;
               GetLocalTime (&st);

               if (pc->iHour < 0) {
                  pc->iHour = (int) st.wHour % 24;
                  swprintf (szTemp, L"h%d", pc->iHour);
                  pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->AttribSetBOOL (gszChecked, TRUE);
               }
               if (pc->iMinute < 0) {
                  pc->iMinute = (int) (st.wMinute / 15) * 15;
                  int   iMinute;
                  iMinute = (pc->iMinute / 15) * 15;
                  swprintf (szTemp, L"m%d%d", iMinute / 10, iMinute%10); // 15-minute increments
                  pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->AttribSetBOOL (gszChecked, TRUE);
               }
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
            if (psz[0] == L'h') {
               pc->iHour = wcstol (psz+1, &pEnd, 10);

               // BUGFIX - When create a meeting the drop-down time-setting only shows time to
               // withn 15 minutes, and it starts at a value, like 8:07. The user drops down and
               // sees it to be 8:00, changes the time to 10:00, and is surprised to get 10:07
               if (!pc->fIgnorePress && (pc->iMinute % 15))
                  pc->iMinute = pc->iMinute - (pc->iMinute % 15);
            }
            else if (psz[0] == L'm') {
               pc->iMinute = wcstol (psz+1, &pEnd, 10);
            }

            // make sure blank is turned off and both minutes and second have time
            pControl = pPage->ControlFind (L"blank");
            if (pControl && pControl->AttribGetBOOL(gszChecked)) {
               // make up a time if none there
               PCEscControl pc2;
               if (pc->iHour < 0) {
                  pc2 = pPage->ControlFind (L"h12");
                  if (pc2)
                     pc2->AttribSetBOOL (gszChecked, TRUE);
                  pc->iHour = 12;
               }
               if (pc->iMinute < 0) {
                  pc2 = pPage->ControlFind (L"m00");
                  if (pc2)
                     pc2->AttribSetBOOL (gszChecked, TRUE);
                  pc->iMinute = 0;
               }

               // uncheck
               pControl->AttribSetBOOL (gszChecked, FALSE);
            }
         }
      }
      return TRUE;

   }

   return FALSE;
}


/***********************************************************************
Control callback
*/
BOOL ControlTime (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   TIME *pc = (TIME*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(TIME));
         pc = (TIME*) pControl->m_mem.p;
         memset (pc, 0, sizeof(TIME));

         pc->iHour = pc->iMinute = -1;
         pc->fIgnorePress = FALSE;
         wcscpy (pc->szBlank, L"Not entered");

         // attributes
         pControl->AttribListAddDecimal (L"hour", &pc->iHour, NULL, TRUE);
         pControl->AttribListAddDecimal (L"minute", &pc->iMinute, NULL, TRUE);
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

         // macro definitions and headers
         mem.StrCat (
            L"<!xTimeButton>"
               L"<td MACROATTRIBUTE=1>"
                  L"<Button MACROATTRIBUTE=1 showbutton=false width=100% radiobutton=true highlightcolor=#ff8000 margintopbottom=2 "
                  L"group=\"h6,h7,h8,h9,h10,h11,h12,h13,h14,h15,h16,h17,h18,h19,h20,h21,h22,h23,h0,h1,h2,h3,h4,h5\">"
                     L"<align align=center><font MACROATTRIBUTE=1><?MacroContent?></font></align>"
                  L"</button>"
               L"</td>"
            L"</xTimeButton>"
            L"<!xMinuteButton>"
               L"<td MACROATTRIBUTE=1>"
                  L"<Button MACROATTRIBUTE=1 showbutton=false width=100% radiobutton=true highlightcolor=#ff8000 "
                  L"group=\"m00,m15,m30,m45\">"
                     L"<align align=center><font MACROATTRIBUTE=1><?MacroContent?></font></align>"
               L"</button>"
               L"</td>"
            L"</xMinuteButton>"
            );

         // background color
         WCHAR szTemp[512];
         WCHAR szColor[16], szColor2[16];
         ColorToAttrib (szColor, pc->dd.cTBackground);
         ColorToAttrib (szColor2, pc->dd.cBBackground);
         swprintf (szTemp, L"<colorblend posn=background tcolor=%s bcolor=%s/>",
            szColor, szColor2);
         mem.StrCat (szTemp);

         // more default string
         mem.StrCat (
            L"<small>"
            L"<table width=200 border=0 innerlines=0>"
            L"<tr>"
            L"<td width=60%>"
            L"<table width=100% lrmargin=0 tbmargin=0 innerlines=0>"
            );

         // add all the times
         COLORREF acrTime[24] = {   // colors at the given times
            RGB(0,0,0), // midnight
            RGB(0,0,0),
            RGB(0,0,0),
            RGB(0,0,0x40), // 3:00 AM
            RGB(0x40,0,0x80),
            RGB(0x40,0x40,0xc0),
            RGB(0x60,0x60,0xd0), // 6:00 AM
            RGB(0x90,0x90,0xff),
            RGB(0xa0,0xa0,0xff),
            RGB(0xb0,0xb0,0xff), // 9:00 AM
            RGB(0xc0,0xc0,0xff),
            RGB(0xd0,0xd0,0xff),
            RGB(0xe0,0xe0,0xff), // 12:00 PM
            RGB(0xe0,0xe0,0xff),
            RGB(0xd0,0xd0,0xff),
            RGB(0xc0,0xc0,0xff), // 3:00 PM
            RGB(0xb0,0xb0,0xff),
            RGB(0xa0,0xa0,0xff),
            RGB(0x90,0x90,0xff), // 6:00 PM
            RGB(0x40,0x40,0xc0),
            RGB(0x40,0,0x80),
            RGB(0,0,0x40), // 9:00 AM
            RGB(0,0,0),
            RGB(0,0,0)
         };
         DWORD i;
         for (i = 0; i < 12; i++) {
            int   iLeft, iRight;
            iLeft = (i+7)%24;
            iRight = (iLeft+12)%24;

            ColorToAttrib (szColor, acrTime[iLeft]);
            ColorToAttrib (szColor2, acrTime[iRight]);

            swprintf (szTemp,
               L"<tr>"
                  L"<xTimeButton name=h%d bgcolor=%s>%d %s</xTimeButton>"
                  L"<xTimeButton name=h%d bgcolor=%s color=#ffffff>%d %s</xTimeButton>"
               L"</tr>",
               iLeft, szColor,
               (iLeft%12) ? (iLeft%12) : 12, (iLeft < 12) ? L"am" : L"pm",
               iRight, szColor2,
               (iRight%12) ? (iRight%12) : 12, (iRight < 12) ? L"am" : L"pm"
               );

            mem.StrCat (szTemp);
         }

         // minutes & the start of the blank button
         mem.StrCat (
            L"</table>"
            L"</td>"
            L"<td width=10%></td>"
            L"<td width=30% valign=center>"
            L"<table bgcolor=#d0d0ff width=100% lrmargin=0 tbmargin=0 innerlines=0>"
               L"<tr>"
                  L"<xMinuteButton name=m00>:00</xMinuteButton>"
               L"</tr>"
               L"<tr>"
                  L"<xMinuteButton name=m15>:15</xMinuteButton>"
               L"</tr>"
               L"<tr>"
                  L"<xMinuteButton name=m30>:30</xMinuteButton>"
               L"</tr>"
               L"<tr>"
                  L"<xMinuteButton name=m45>:45</xMinuteButton>"
               L"</tr>"
            L"</table>"
            L"</td>"
            L"</tr>"
            L"<tr>"
            L"<td bgcolor=#d0d0ff lrmargin=0 tbmargin=0>"
               L"<Button name=blank showbutton=false width=100% checkbox=true highlightcolor=#ff8000>"
                  L"<align align=center>"
            );

         // blank-button name
         // when append szBlank here and in time control need to clean
         // up so it doesn't contain any tags
         WCHAR szMML[512];
         DWORD dwNeeded;
         szMML[0] = 0;
         StringToMMLString (pc->szBlank, szMML, sizeof(szMML), &dwNeeded);
         mem.StrCat (szMML);

         // rest of text
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
         psz = p->pWindow->PageDialog ((PWSTR)mem.p, TimeCallback, (PVOID) pc);
         
         // hide the window so it's not visible is a call is made from this
         p->pWindow->ShowWindow (SW_HIDE);

         // invalidate the control since its contents have changed
         pControl->Invalidate();

         // alert the app
         ESCNTIMECHANGE tc;
         memset (&tc, 0, sizeof(tc));
         tc.iHour = pc->iHour;
         tc.iMinute = pc->iMinute;
         tc.pControl = pControl;
         pControl->MessageToParent (ESCN_TIMECHANGE, &tc);
      }
      return TRUE;

   case ESCM_DROPDOWNTEXTBLOCK:
      {
         PESCMDROPDOWNTEXTBLOCK p = (PESCMDROPDOWNTEXTBLOCK) pParam;

         // determine what we should say
         WCHAR szTemp[64];
         if ((pc->iHour < 0) || (pc->iMinute < 0)) {
            wcscpy (szTemp, pc->szBlank);
         }
         else {
            // have a time
            int   iHour;
            iHour = pc->iHour % 12;
            if (!iHour)
               iHour = 12;
            swprintf (szTemp, L"%d:%d%d %s",
               iHour, pc->iMinute / 10, pc->iMinute % 10,
               (pc->iHour >= 12) ? L"pm" : L"am");
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




