/*************************************************************************
Today.cpp - Shows the daily tasks window

begun 8/6/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

/*****************************************************************************
TodayPage - Override page callback.
*/
BOOL TodayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         static BOOL fCheckedFreeSpace = FALSE;
         // check to see if not enought free space
         if (!fCheckedFreeSpace) {
            fCheckedFreeSpace = TRUE;
            if (DiskFreeSpace (gpData->Dir()) < 10000000) {
               pPage->MBWarning (L"The disk where the Dragonfly files are saved is running low on memory.",
                  L"You should either free up some more space or move your Dragonfly files.");
            }
         }
      }
      break;   // defaukt handler

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"TODAYSDATE")) {
            DFDATE   date;
            WCHAR    szTemp[64];
            date = Today();
            DFDATEToString (date, szTemp);
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
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
         else if (!_wcsicmp(p->pszSubName, L"PROJECTSUMMARY")) {
            // show projects
            p->pszSubString = ProjectSummary();
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"REMINDERSUMMARY")) {
            // show projects
            p->pszSubString = RemindersSummary(Today());
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORKSUMMARY")) {
            // show projects
            p->pszSubString = WorkSummary(FALSE,Today());
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MEETINGSUMMARY")) {
            // show projects
            p->pszSubString = PlannerSummaryScale(Today());
            return TRUE;
         }
      }
      break;   // default behavior

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
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
TomorrowPage - Override page callback.
*/
BOOL TomorrowPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"TODAYSDATE")) {
            DFDATE   date;
            WCHAR    szTemp[64];
            date = MinutesToDFDATE(DFDATEToMinutes(Today())+60*24);
            DFDATEToString (date, szTemp);
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PROJECTSUMMARY")) {
            // show projects
            p->pszSubString = ProjectSummary();
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"REMINDERSUMMARY")) {
            // show projects
            p->pszSubString = RemindersSummary(MinutesToDFDATE(DFDATEToMinutes(Today())+60*24));
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORKSUMMARY")) {
            // show projects
            p->pszSubString = WorkSummary(FALSE,MinutesToDFDATE(DFDATEToMinutes(Today())+60*24));
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MEETINGSUMMARY")) {
            // show projects
            p->pszSubString = PlannerSummaryScale(MinutesToDFDATE(DFDATEToMinutes(Today())+60*24));
            return TRUE;
         }

      }
      break;   // default behavior

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
