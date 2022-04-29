/*************************************************************************
WrapUp.cpp - handle the wrap-up

begun 8/27/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

DFDATE WrapUpDetermineReflectionDay (DFDATE today);

static DWORD gdwWrapUpNode;
static WCHAR gszMorning[] = L"Morning";
static WCHAR gszAfternoon[] = L"Afternoon";
static WCHAR gszEvening[] = L"Evening";
static WCHAR gszGoodDay[] = L"GoodDay";
static WCHAR gszHealth[] = L"Health";
static WCHAR gszAnswer[] = L"Answer";
static WCHAR gszQuestion[] = L"Question";
static WCHAR gszReflectionOn[] = L"ReflectionOn";
static WCHAR gszReflectionFrom[] = L"ReflectionFrom";
static WCHAR gszReflection[] = L"Reflection";
static WCHAR gszBugAboutWrapButton[] = L"BugAboutWrap";

/***********************************************************************
WrapUpDailyQuestion - Returns the question of the day:

inputs
   DFDATE      date - date
   PWSTR       pszMML - MML to display in edit page. Can be NULL.
                  This may also be of the form, "Please fill in XXX".
                  Should be about 1024 chars
   PWSTR       pszText - Text form, no links. Should be about 1024 chars.
returns
   BOOL - TRUE if it came up with a question
*/
BOOL WrapUpDailyQuestion (DFDATE date, PWSTR pszMML, PWSTR pszText)
{
   HANGFUNCIN;
   if (pszMML)
      pszMML[0] = 0;
   if (pszText)
      pszText[0] = 0;

   // get random value for the type, and then the sub-elem
   int   ir1, ir2;
   // BUGFIX - Make more random
   ir1 = RepeatableRandom ((int)date + (int) (date<<8), 5);
   ir2 = RepeatableRandom ((int)date + (int) (date<<8), 10);

   // get the deep thoughts
   DEEPTHOUGHTS dt;
   PCMMLNode pDeep;
   pDeep = DeepFillStruct(&dt);
   if (!pDeep)
      return FALSE;  // error

   // make sure there are actually elements typed in. If there a
   ir1 = ir1 % 5; // for 5 different lists
   PWSTR *papsz;
   switch (ir1) {
   case 0:  // people
      papsz = dt.apszPerson;
      break;
   case 1:  // short
      papsz = dt.apszShort;
      break;
   case 2:  // long
      papsz = dt.apszLong;
      break;
   case 3:  // change
      papsz = dt.apszChange;
      break;
   case 4:  // world
      papsz = dt.apszWorld;
      break;
   }
   DWORD dwCount, i;
   for (i = dwCount = 0; i < TOPN; i++)
      if (papsz[i])
         dwCount++;

   // if empty then ask user to fill in
   if (!dwCount) {
      if (pszText)
         wcscpy (pszText, L"Unable to generate a question.");

      if (pszMML) {
         wcscpy (pszMML, L"Please fill in the ");
         switch (ir1) {
         case 0:  // people
            wcscat (pszMML, L"<a href=r:223>most important people</a>");
            break;
         case 1:  // short
            wcscat (pszMML, L"<a href=r:224>short term goals</a>");
            break;
         case 2:  // long
            wcscat (pszMML, L"<a href=r:225>long term goals</a>");
            break;
         case 3:  // change
            wcscat (pszMML, L"<a href=r:226>things to change</a>");
            break;
         case 4:  // world
            wcscat (pszMML, L"<a href=r:227>world problems</a>");
            break;
         }
         wcscat (pszMML, L" section so that Dragonfly can pose a deep question to you.");
      }

      gpData->NodeRelease (pDeep);
      return FALSE;
   }

   // else, theres something entered, so find the index into the element
   DWORD dwIndex;
   ir2 = ir2 % (int) dwCount;
   dwIndex = 0;
   for (i = 0; i < TOPN; i++)
      if (papsz[i]) {
         // BUGFIX - Was always asking about the last item in the top-N list
         if (!ir2) {
            dwIndex = i;
            break;
         }
         ir2--;
      }

   // sanitize the string
   WCHAR szSanitize[1024];
   size_t dwNeeded;
   StringToMMLString(papsz[dwIndex], szSanitize, sizeof(szSanitize), &dwNeeded);

   // display string
   switch (ir1) {
   case 0:  // people
      if (pszMML)
         swprintf (pszMML, L"Have you talked to <a href=r:223>%s</a> in the last week?", szSanitize);
      if (pszText)
         swprintf (pszText, L"Have you talked to %s in the last week?", papsz[dwIndex]);
      break;
   case 1:  // short
      if (pszMML)
         swprintf (pszMML, L"Did you make any progress on your short-term goal, <a href=r:224>%s</a>, in the last week?", szSanitize);
      if (pszText)
         swprintf (pszText, L"Did you make any progress on your short-term goal, %s, in the last week?", papsz[dwIndex]);
      break;
   case 2:  // long
      if (pszMML)
         swprintf (pszMML, L"Did you make any progress on your long-term goal, <a href=r:225>%s</a>, in the last week?", szSanitize);
      if (pszText)
         swprintf (pszText, L"Did you make any progress on your long-term goal, %s, in the last week?", papsz[dwIndex]);
      break;
   case 3:  // change
      if (pszMML)
         swprintf (pszMML, L"Did you work on improving, <a href=r:226>%s</a>, in the last week?", szSanitize);
      if (pszText)
         swprintf (pszText, L"Did you work on improving, %s, in the last week?", papsz[dwIndex]);
      break;
   case 4:  // world
      if (pszMML)
         swprintf (pszMML, L"Did you work towards solving the world problem, <a href=r:227>%s</a>, in the last week?", szSanitize);
      if (pszText)
         swprintf (pszText, L"Did you work towards solving the world problem, %s, in the last week?", papsz[dwIndex]);
      break;
   }

   gpData->NodeRelease (pDeep);
   return TRUE;
}

/***********************************************************************
WrapUpEditPage - Page callback
*/
BOOL WrapUpEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"DATE")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            DFDATE date;
            date = (DFDATE)NodeValueGetInt(pNode, gszDate, (int)0);

            WCHAR szTemp[64];
            DFDATEToString (date, szTemp);
            MemCat (&gMemTemp, szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"LOG")) {
            // BUGFIX - Show the logged list when do dialy wrapup
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            DFDATE date;
            date = (DFDATE)NodeValueGetInt(pNode, gszDate, (int)0);

            gpData->NodeRelease(pNode);

            // look in log
            p->pszSubString = CalendarLogSub(date);
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"RANDOMTHOUGHT")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            WCHAR szTemp[1024];
            WrapUpDailyQuestion ((DFDATE)NodeValueGetInt(pNode, gszDate,0),szTemp, NULL);
            MemCat (&gMemTemp, szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"REFLECTION")) {
            // get today's date
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            DFDATE today;
            today = (DFDATE)NodeValueGetInt(pNode, gszDate, (int)0);
            gpData->NodeRelease(pNode);


            DFDATE dReflect;
            dReflect = WrapUpDetermineReflectionDay (today);
            if (!dReflect)
               return FALSE;  // nothing to relfect on

            // else, display some stuff
            MemCat (&gMemTemp, L"<xbr/><xsectiontitle>Reflections</xsectiontitle>");
            WCHAR szTemp[64];
            DFDATEToString (dReflect, szTemp);
            MemCat (&gMemTemp, L"<p>Sometimes it's good to look back and re-evaluate what you accomplished. "
               L"Please take the time to write about <bold>");
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</bold>. To jog your memory, here's what happened that day:</p>");

            // what was logged
            MemCat (&gMemTemp, L"<font color=#000000><xTableCenter><xtrheader>Automatically logged</xtrheader>");
            CalendarLogSub (dReflect,TRUE);
            MemCat (&gMemTemp, L"</xtablecenter></font>");

            // what wrote in daily wrapup
            PCMMLNode pOld;
            DWORD dwOld;
            dwOld = WrapUpCreateOrGet(dReflect, FALSE);
            pOld = gpData->NodeGet(dwOld);
            if (pOld) {
               MemCat (&gMemTemp, L"<xtablecenter><xtrheader>In the morning</xtrheader><tr><td><align parlinespacing=0><font color=#000000>");
               PWSTR psz;
               psz = NodeValueGet (pOld, gszMorning);
               MemCatSanitize (&gMemTemp, psz ? psz : L"None");
               MemCat (&gMemTemp, L"</font></align></td></tr><xtrheader>In the afternoon</xtrheader><tr><td><align parlinespacing=0><font color=#000000>");
               psz = NodeValueGet (pOld, gszAfternoon);
               MemCatSanitize (&gMemTemp, psz ? psz : L"None");
               MemCat (&gMemTemp, L"</font></align></td></tr><xtrheader>In the evening</xtrheader><tr><td><align parlinespacing=0><font color=#000000>");
               psz = NodeValueGet (pOld, gszEvening);
               MemCatSanitize (&gMemTemp, psz ? psz : L"None");
               MemCat (&gMemTemp, L"</font></align></td></tr></xtablecenter>");

               gpData->NodeRelease(pOld);
            }

            // and ask to write out what did
            MemCat (&gMemTemp, L"<p>Now that time has passed since <bold>");
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</bold>, take advantage of your hindsight and write "
               L"about that day (or period) again.</p>");
            MemCat (&gMemTemp, L"<xtablecenter><xtrheader>");
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</xtrheader><tr><td><font color=#000000><edit name=reflection width=100%% height=25%% multiline=true wordwrap=true maxchars=10000 defcontrol=true/></font></td></tr></xtablecenter>");

            p->pszSubString = (PWSTR)gMemTemp.p;
         }

      }
      break;

   case ESCM_INITPAGE:
      {
         PCMMLNode   pEntry;
         pEntry = gpData->NodeGet (gdwWrapUpNode);
         if (!pEntry)
            break; // error

         // show info
         PWSTR psz;
         PCEscControl pControl;

         // write some stuff into the entry
         psz = NodeValueGet (pEntry, gszMorning);
         pControl = pPage->ControlFind (gszMorning);
         if (psz && pControl)
            pControl->AttribSet(gszText, psz);

         psz = NodeValueGet (pEntry, gszAfternoon);
         pControl = pPage->ControlFind (gszAfternoon);
         if (psz && pControl)
            pControl->AttribSet(gszText, psz);

         psz = NodeValueGet (pEntry, gszEvening);
         pControl = pPage->ControlFind (gszEvening);
         if (psz && pControl)
            pControl->AttribSet(gszText, psz);

         // BUGFIX - Do reflections
         pControl = pPage->ControlFind (gszReflection);
         if (pControl) {
            DFDATE today = (DFDATE)NodeValueGetInt(pEntry, gszDate, (int)0);
            DFDATE dReflect = WrapUpDetermineReflectionDay(today);
            DWORD  dwReflect = WrapUpCreateOrGet(dReflect, FALSE);
            PCMMLNode pReflect = gpData->NodeGet (dwReflect);
            PWSTR psz = pReflect ? NodeValueGet(pReflect, gszReflection) : NULL;

            if (psz)
               pControl->AttribSet(gszText, psz);

            if (pReflect)
               gpData->NodeRelease(pReflect);
         }

         ReadCombo (pEntry, gszGoodDay, pPage, gszGoodDay);
         ReadCombo (pEntry, gszHealth, pPage, gszHealth);

         // get answer to the question of the day
         int   iRet;
         BOOL  fQuestion;
         fQuestion = WrapUpDailyQuestion((DFDATE)NodeValueGetInt(pEntry, gszDate,0), NULL, NULL);
         iRet = NodeValueGetInt (pEntry, gszAnswer, 0);
         pControl = pPage->ControlFind (L"yes");
         if (pControl) {
            pControl->AttribSetBOOL (gszChecked, iRet);
            if (!fQuestion)
               pControl->Enable(FALSE);
         }
         pControl = pPage->ControlFind (L"no");
         if (pControl) {
            pControl->AttribSetBOOL (gszChecked, !iRet);
            if (!fQuestion)
               pControl->Enable(FALSE);
         }

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
         WCHAR szTemp[10000];
         pNode = gpData->NodeGet (gdwWrapUpNode);
         if (!pNode)
            break;

         pControl = pPage->ControlFind(gszMorning);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszMorning, szTemp);

         pControl = pPage->ControlFind(gszAfternoon);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszAfternoon, szTemp);

         pControl = pPage->ControlFind(gszEvening);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszEvening, szTemp);

         // BUGFIX - Do reflections
         pControl = pPage->ControlFind (gszReflection);
         if (pControl) {
            DFDATE today = (DFDATE)NodeValueGetInt(pNode, gszDate, (int)0);
            DFDATE dReflect = WrapUpDetermineReflectionDay(today);
            DWORD  dwReflect = WrapUpCreateOrGet(dReflect, FALSE);
            PCMMLNode pReflect = gpData->NodeGet (dwReflect);

            szTemp[0] = 0;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);

            if (pReflect) {
               NodeValueSet (pReflect, gszReflection, szTemp);
               gpData->NodeRelease(pReflect);
            }
         }

         // save away answer to question of the day
         pControl = pPage->ControlFind (L"yes");
         NodeValueSet (pNode, gszAnswer, pControl && pControl->AttribGetBOOL(gszChecked));

         WriteCombo (pNode, gszGoodDay, pPage, gszGoodDay);
         WriteCombo (pNode, gszHealth, pPage, gszHealth);

         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default behavior
   };

   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************
WrapUpSet - Sets the category node to view
*/
void WrapUpSet (DWORD dwNode)
{
   HANGFUNCIN;
   gdwWrapUpNode = dwNode;
}

/***********************************************************************
WrapUpViewPage - Page callback
*/
BOOL WrapUpViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"DATE")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            DFDATE date;
            date = (DFDATE)NodeValueGetInt(pNode, gszDate, (int)0);

            WCHAR szTemp[64];
            DFDATEToString (date, szTemp);
            MemCat (&gMemTemp, szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"LOG")) {
            // BUGFIX - Show the logged list when do dialy wrapup
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            DFDATE date;
            date = (DFDATE)NodeValueGetInt(pNode, gszDate, (int)0);

            gpData->NodeRelease(pNode);

            // look in log
            p->pszSubString = CalendarLogSub(date);
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, gszGoodDay)) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszGoodDay);
            MemCat (&gMemTemp, psz ? psz : L"Typical");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, gszHealth)) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszHealth);
            MemCat (&gMemTemp, psz ? psz : L"Typical");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, gszMorning)) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszMorning);
            MemCat (&gMemTemp, psz ? psz : L"None");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, gszAfternoon)) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszAfternoon);
            MemCat (&gMemTemp, psz ? psz : L"None");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, gszEvening)) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            PWSTR psz;
            psz = NodeValueGet (pNode, gszEvening);
            MemCat (&gMemTemp, psz ? psz : L"None");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"DEEPQUESTION")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            WCHAR szTemp[1024];
            WrapUpDailyQuestion ((DFDATE)NodeValueGetInt(pNode, gszDate,0), NULL, szTemp);
            MemCat (&gMemTemp, szTemp);

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"DEEPANSWER")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            MemCat (&gMemTemp, NodeValueGetInt(pNode,gszAnswer,0) ? L"Yes" : L"<font color=#ff0000>No</font>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"REFLECTION")) {
            PCMMLNode pNode;
            pNode = gpData->NodeGet (gdwWrapUpNode);
            if (!pNode)
               return FALSE;   // unexpected error
            MemZero (&gMemTemp);

            // see if have any reflection
            DFDATE dFrom;
            PWSTR psz;
            dFrom = (DFDATE)NodeValueGetInt(pNode, gszReflectionFrom, 0);
            psz = NodeValueGet (pNode, gszReflection);
            if (!dFrom || !psz || !psz[0]) {
               gpData->NodeRelease (pNode);
               return FALSE;  // nothing here
            }

            // else some reflections
            MemCat (&gMemTemp, L"<xtrheader>Reflections written on ");
            WCHAR szTemp[64];
            DFDATEToString (dFrom, szTemp);
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</xtrheader><tr><td><align parlinespacing=0><font color=#000000>");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"</font></align></td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            gpData->NodeRelease(pNode);
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
            swprintf (szTemp, L"e:%d", (int) gdwWrapUpNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
WrapUpDetermineReflectionDay - Given that a new daily wrapup is being
edited, this determines (and writes in the log node) the DFDATE that
we're reflecting on.

inputs
   DFDATE      today - today's date. AKA: What date we're wrapping up tday
returns
   DFDATE - what date is getting reflected upon today
*/
DFDATE WrapUpDetermineReflectionDay (DFDATE today)
{
   HANGFUNCIN;
   DWORD       dwNode = WrapUpCreateOrGet(today, TRUE);
   PCMMLNode   pToday = gpData->NodeGet (dwNode);
   if (!pToday)
      return 0;

   // see if it's there already
   DFDATE dWhen;
   dWhen = (DFDATE) NodeValueGetInt (pToday, gszReflectionOn, 0);
   if (dWhen) {
#ifdef _DEBUG
      __int64 iMin;
      iMin = ((DFDATEToMinutes(today) - DFDATEToMinutes(dWhen)) / 60 / 24 / 31);
      if (iMin < 6) goto fixdebug;
#endif

      gpData->NodeRelease(pToday);
      return dWhen;
   }

#ifdef _DEBUG
fixdebug:
#endif

   // BUGFIX - 75% change of skipping algorther
   if ((rand() % 100) > 25)
      goto skiplookback;


   // try up to 5 times
   // BUGFIX - Reduced to 3 so faster
   // BUGFUX - Reduced to 1
   DWORD i;
   for (i = 0; i < 1; i++) {
      // pick a random date
      DFDATE dRand;
      dRand = CalendarLogRandom();
      if (!dRand)
         continue;

      // if it occurred more less than 6 months ago then don't ask because it
      // was too recent
      // BUGFIX - Moved up so it's faster
      __int64 iMin;
      iMin = ((DFDATEToMinutes(today) - DFDATEToMinutes(dRand)) / 60 / 24 / 31);
      if ( iMin < 6) {
         continue;
      }

      // see if there's a daily wrapup for that date
      dwNode = WrapUpCreateOrGet(dRand, FALSE);
      PCMMLNode   pRand;
      pRand = gpData->NodeGet(dwNode);
      if (!pRand)
         continue;

      // if it already has a reflection-from then skip
      DFDATE dFrom;
      dFrom = (DFDATE) NodeValueGetInt (pRand, gszReflectionFrom, 0);
      if (dFrom) {
         gpData->NodeRelease(pRand);
         continue;
      }

      // else done
      // else, have a match. Node all this
      NodeValueSet (pRand, gszReflectionFrom, (int) today);
      NodeValueSet (pToday, gszReflectionOn, (int) dRand);

      // finall
      gpData->NodeRelease(pRand);
      gpData->NodeRelease(pToday);

      return dRand;
   }
skiplookback:

   // if got here didn't find anything
   gpData->NodeRelease(pToday);
   return 0;

}

/***********************************************************************
WrapUpCreateOrGet - Given a day (DFDATE), this looks in the calendar's
daily journal for a wrapup entry. If there isn't one it creates it
and adds it to the daily journal. If there is one it returns it

inputs
   DFDATE      date - date to look for
   BOOL        fCreate - If TRUE then create if don't exist. If FALSE,
               then fail if it doesn't exist.
returns
   DWORD - Node ID. -1 if failed
*/
DWORD WrapUpCreateOrGet (DFDATE date, BOOL fCreate)
{
   HANGFUNCIN;
   // get the daily journal entry
   PCMMLNode pNode;
   DWORD dwCalendar;
   pNode = GetCalendarLogNode (date, fCreate, &dwCalendar);
   if (!pNode)
      return (DWORD)-1;

   // look for node
   DWORD dwWrap;
   dwWrap = NodeValueGetInt (pNode, gszWrapUpNode, -1);
   if (dwWrap != (DWORD)-1) {
      // already exists
      gpData->NodeRelease(pNode);
      return dwWrap;
   }

   // if dont want to create return error
   if (!fCreate) {
      gpData->NodeRelease(pNode);
      return (DWORD)-1;
   }

   // must create
   PCMMLNode pWrap;
   pWrap = gpData->NodeAdd (gszWrapUpNode, &dwWrap);
   if (!pWrap) {
      // error
      gpData->NodeRelease(pNode);
      return (DWORD)-1;
   }
   NodeValueSet (pWrap, gszDate, (int) date);
   gpData->NodeRelease (pWrap);

   // store date
   NodeValueSet (pNode, gszWrapUpNode, (int) dwWrap);

   // also store the fact that created
   CalendarLogAdd (date, Now(), -1, L"Daily wrap-up", dwWrap);

   gpData->NodeRelease(pNode);
   gpData->Flush();
   return dwWrap;
}

/***********************************************************************
WrapUpPage - Page callback
*/
BOOL WrapUpPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszBugAboutWrapButton);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, gfBugAboutWrap);

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszBugAboutWrapButton)) {
            // BUGFIX - ALlow to turn off bugging about wrapup
            gfBugAboutWrap = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszBugAboutWrap, gfBugAboutWrap);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"addtoday")) {
            DWORD dwNode;
            dwNode = WrapUpCreateOrGet (Today(), TRUE);
            if (dwNode == (DWORD)-1)
               break;

            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
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
   };

   return DefPage (pPage, dwMessage, pParam);
}



