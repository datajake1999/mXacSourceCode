/******************************************************************************
EscHelp.cpp - Code for the escarpment help applicaton

begun 4/22/2000 by Mike ROzak
Copyright 2000 Mike Rozak. All rights reserved
*/
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "escarpment.h"
#include "resource.h"

HINSTANCE      ghInstance;
CListVariable  glistHistory;  // list of string (usually r:XXX) for history
CListFixed     glistHistoryVScroll; // here the VScroll was
DWORD          gdwCurPage;    // resource ID of the current page
int            giVScroll;     // vertical pixel to scroll to. If -1 then use gszCurPageString
WCHAR          gszCurPageString[512]; // string used to access the current page, such as "r:132#hello"
CEscSearch     gSearch;       // search info


/********************************************************************
DataToUnicode - Takes a pointer to unknown data, either from a file
or from a resource, and determines if it's a unciode text. If it's
unicode, the data is copied and null-terminated. If it's ANSI it's
converted and NULL terminated. Use the magic unicode tag to determine

inputs
   PVOID       pData - data
   DWORD       dwSize - data size
   BOOL        *pfWasUnicode - if not NULL, this is filled in with TRUE
               if the original data was unciode, FLASE if its ANSI
returns
   PWSTR - Unicode string. Must bee freeed()
*/
PWSTR DataToUnicode (PVOID pData, DWORD dwSize, BOOL *pfWasUnicode)
{
   WCHAR *pc;

   // might be unicode
   if (dwSize >= 2) {
      pc = (WCHAR*) pData;
      if (pc[0] == 0xfeff) {
         // it's unicode
         PWSTR psz;
         psz = (PWSTR) malloc(dwSize);
         if (!psz)
            return NULL;
         memcpy (psz, pc + 1, dwSize - 2);
         psz[dwSize/2 - 1] = 0;   // null terminate

         if (pfWasUnicode)
            *pfWasUnicode = TRUE;
         return psz;
      }
   }

   // else it's ANSI
   int   iLen;
   pc = (PWSTR) malloc (dwSize*2+2);
   if (!pc)
      return FALSE;
   iLen = MultiByteToWideChar (CP_ACP, 0, (char*) pData, dwSize, pc, dwSize+1);
   pc[iLen] = 0;  // NULL terminate

   if (pfWasUnicode)
      *pfWasUnicode = FALSE;
   return pc;
}

/********************************************************************
ResourceToUnicode - Reads in a 'MML' resource element. Determines if
it's unicde, and converts to ANSI if necessary

inputs
   HINSTANCE   hInstance - module
   DWORD       dwID - resource ID
   char        *pszResType - Resource type. if NULL use "MML"
returns
   PWSTR - Unicode string. Must be freed(). NULL if error
*/
PWSTR ResourceToUnicode (HINSTANCE hInstance, DWORD dwID, char *pszResType)
{
   if (!pszResType)
      pszResType = "MML";

   HRSRC    hr;

   hr = FindResource (hInstance, MAKEINTRESOURCE (dwID), pszResType);
   if (!hr)
      return NULL;

   HGLOBAL  hg;
   hg = LoadResource (hInstance, hr);
   if (!hg)
      return NULL;


   PVOID pMem;
   pMem = LockResource (hg);
   if (!pMem)
      return NULL;

   DWORD dwSize;
   dwSize = SizeofResource (hInstance, hr);

   // convert
   return DataToUnicode (pMem, dwSize, NULL);
}



/***********************************************************************
GetList - Returns the pointer to the list for the sample FilteredList
controls displayed in help.

inputs
returns
   PCListVariable - list
*/
static PCListVariable GetList (void)
{
   static BOOL fListFilled = FALSE;
   static CListVariable list;

   if (!fListFilled) {
      fListFilled = TRUE;

      WCHAR szFillList[] =
         L"Jenny Abernacky\0\0"
         L"Bill Bradburry\0Billy\0"
         L"Jim Briggs\0\0"
         L"Mike Rozak\0Michael\0"
         L"Fred Smith\0Freddie\0"
         L"John Smith\0\0"
         L"Samantha Walters\0\0"
         L"Harry Weatherspoon\0\0"
         L"Carry Wellsford\0\0"
         L"Jone Zeck\0\0"
         L"\0\0";

      PWSTR pCur;
      for (pCur = szFillList; pCur[0]; ) {
         DWORD dwLen;
         dwLen = wcslen(pCur)+1;
         dwLen += wcslen(pCur+dwLen)+1;

         list.Add (pCur, dwLen*2);

         pCur += dwLen;
      }
   }

   return &list;
}


/***********************************************************************
PageViewSource - For handlng view source
*/
BOOL PageViewSource (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // if it's TEMPLATE then replace with <?Template resource=102?> since
         // this then allows us to print without printing all the control gunk
         if (!wcsicmp(p->pszSubName, L"SOURCE")) {
            p->fMustFree = FALSE;
            p->pszSubString = (PWSTR) pPage->m_pUserData;
         }
      }
      return TRUE;
   }
   return FALSE;
}


/***********************************************************************
UpdateSearchList - If this is the search window then it fills the
search listbox with the search items

inputs
   PCEscPage      pPage
*/
void UpdateSearchList (PCEscPage pPage)
{
   PCEscControl pc;
   pc = pPage->ControlFind(L"searchlist");
   if (!pc)
      return;  // not the search window

   // clear out the current elemnts
   pc->Message (ESCM_LISTBOXRESETCONTENT);

   // add them in
   WCHAR szHuge[1024];
   WCHAR szTitle[512], szSection[512], szLink[512];
   DWORD dwNeeded;

   DWORD i;
   PWSTR psz;
   for (i = 0; i < min(20,gSearch.m_listFound.Num()); i++) {
      psz = (PWSTR) gSearch.m_listFound.Get(i);
      if (!psz)
         continue;

      // the search info is packed, so unpack and convert so
      // none of the characters interfere with MML

      // scorew
      DWORD dwScore;
      dwScore = *((DWORD*) psz);
      psz += (sizeof(DWORD)/sizeof(WCHAR));

      // document title
      szTitle[0] = 0;
      StringToMMLString (psz, szTitle, sizeof(szTitle), &dwNeeded);
      psz += (wcslen(psz)+1);

      // section title
      szSection[0] = 0;
      StringToMMLString (psz, szSection, sizeof(szSection), &dwNeeded);
      psz += (wcslen(psz)+1);

      // link
      szLink[0] = 0;
      StringToMMLString (psz, szLink, sizeof(szLink), &dwNeeded);

      // combine this into 1 large mml string
      swprintf (szHuge,
         L"<elem name=\"%s\">"
            L"<br><bold>Document: %s</bold></br>"
            L"<br>&tab;Section: %s</br>"
            L"<br>&tab;Score: %g</br>"
         L"</elem>",
         szLink,
         szTitle[0] ? szTitle : L"Unknown",
         szSection[0] ? szSection : L"None",
         (double) (dwScore/100) / 10
         );

      // add to the list
      ESCMLISTBOXADD a;
      memset (&a, 0, sizeof(a));
      a.dwInsertBefore = (DWORD)-1;
      a.pszMML = szHuge;
      pc->Message (ESCM_LISTBOXADD, &a);
   }

   // done
}

/***********************************************************************
PageMain - For handlng main page

NOTE: This PageMain is somewhat unusual since I'm using the same code for
all the pages. Usually you wouldn't do this, but because this application
is so trivial I decided it would make it more complex to have more
than one page callback.
*/
BOOL PageMain (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static BOOL fFirstTime = TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      // speak the first time
         // play a chime
#define  NOTEBASE       62
#define  VOLUME         64
      if (fFirstTime) {

         ESCMIDIEVENT aChime[] = {
            {0, MIDIINSTRUMENT (0, 72+3)}, // flute
            {0, MIDINOTEON (0, NOTEBASE+0,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE+0)},
            {0, MIDINOTEON (0, NOTEBASE-1,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE-1)},
            {0, MIDINOTEON (0, NOTEBASE+0,VOLUME)},
            {300, MIDINOTEOFF (0, NOTEBASE+0)},
            {100, MIDINOTEON (0, NOTEBASE+6,VOLUME)},
            {200, MIDINOTEOFF (0, NOTEBASE+6)},
            {100, MIDINOTEON (0, NOTEBASE-6,VOLUME)},
            {750, MIDINOTEOFF (0, NOTEBASE-6)}
         };
         EscChime (aChime, sizeof(aChime) / sizeof(ESCMIDIEVENT));

         // speak
         EscSpeak (L"Welcome to Escarpment.");

      }
      fFirstTime = FALSE;

      // if this is the seach window then set the search text and fill
      // in the list box
      PCEscControl   pc;
      pc = pPage->ControlFind (L"SearchString");
      if (pc && gSearch.m_pszLastSearch)
         pc->AttribSet (L"text", gSearch.m_pszLastSearch);
      UpdateSearchList(pPage);

      // if we have somplace to scroll to then do so
      if (giVScroll >= 0)
         pPage->VScroll (giVScroll);
      else {
         // if the current page string has a "#" in it then skip to that
         // section
         PWSTR psz;
         psz = (PWSTR) pPage->m_pUserData;
         if (psz)
            psz = wcschr(psz, L'#');
         if (psz) {
            psz++;
            pPage->VScrollToSection (psz);
         };
      }

      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // if it's TEMPLATE then replace with <?Template resource=102?> since
         // this then allows us to print without printing all the control gunk
         if (!wcsicmp(p->pszSubName, L"TEMPLATE")) {
            p->fMustFree = FALSE;
            p->pszSubString = L"<?Template resource=102?>";
         }
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // if it's viewsource then trap
         if (!wcsicmp(p->psz, L"viewsource") || !wcsicmp(p->psz, L"viewmacros") || !wcsicmp(p->psz, L"viewtemplate")) {
            DWORD dwRes;
            if (!wcsicmp(p->psz, L"viewmacros"))
               dwRes = IDR_MMLMACROS;
            else if (!wcsicmp(p->psz, L"viewtemplate"))
               dwRes = IDR_MMLTEMPLATE;
            else
               dwRes = gdwCurPage;

            PWSTR psz;
            psz = ResourceToUnicode (ghInstance, dwRes, NULL);
            RECT  r;
            r.left = GetSystemMetrics (SM_CXSCREEN) / 6;
            r.right = GetSystemMetrics (SM_CXSCREEN) - r.left;
            r.top = 100;
            r.bottom = GetSystemMetrics (SM_CYSCREEN) - r.top;
            CEscWindow  cWindow;
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
            cWindow.PageDialog (ghInstance, IDR_MMLVIEWSOURCE, PageViewSource, psz);
            if (psz)
               free (psz);
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"print")) {
            // user clicked the hello world sample
            CEscWindow cWindow;
            cWindow.InitForPrint (NULL, ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.m_PrintInfo.wOtherScale = 0x100 * 2 / 3;
            cWindow.m_PrintInfo.wFontScale = 0x100 * 2 / 3;
            cWindow.m_PrintInfo.wColumns = 2;
            cWindow.PrintPageLoad (ghInstance, gdwCurPage);
            cWindow.Print ();
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"searchbutton")) {
            // get the text to search for
            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            PCEscControl   pc;
            pc = pPage->ControlFind (L"SearchString");
            if (pc)
               pc->AttribGet (L"text", szTemp, sizeof(szTemp), &dwNeeded);

            // index the search if it need reindexing because of a new app version
            if (gSearch.NeedIndexing()) {
               DWORD adwDontIndex[] = {IDR_MMLMACROS, IDR_MMLTEMPLATE};
               gSearch.Index (pPage->m_pWindow->m_hWnd, NULL, TRUE,
                  adwDontIndex, sizeof(adwDontIndex) / sizeof(DWORD));

            }

            // search
            gSearch.Search (szTemp);

            // update the list
            UpdateSearchList(pPage);

            // inform user that search complete
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Search finished.");

            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"helloworld")) {
            // user clicked the hello world sample
            CEscWindow cWindow;
            WCHAR sz[] = L"<p>Hello World!</p>";
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, sz, NULL);
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"helloworld2")) {
            // user clicked the hello world sample
            CEscWindow cWindow;
            WCHAR sz[] = L"<p align=center><big>Hello World!</big></p>";
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, sz, NULL);
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"helloworld3")) {
            // user clicked the hello world sample
            CEscWindow cWindow;
            WCHAR sz[] = L"<p align=center><big>Hello World!</big></p>"
               L"<button>My button</button>";
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, sz, NULL);
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"helloworld4")) {
            // user clicked the hello world sample
            CEscWindow cWindow;
            WCHAR sz[] = L"<p align=center><big>Hello World!</big></p>"
               L"<button href=\"http://www.mxac.com.au\">My button</button>";
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, sz, NULL);
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"MBInformation")) {
            pPage->MBInformation (L"This is information.", L"Fine print.");
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"MBWarning")) {
            pPage->MBWarning (L"This is a warning.", L"Fine print.");
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"MBYesNo")) {
            pPage->MBYesNo (L"This is a question.", L"Fine print.");
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"MBError")) {
            pPage->MBError (L"This is an error.", L"Fine print.");
            return TRUE;
         }
         else if (!wcsicmp(p->psz, L"MBSpeakInformation")) {
            pPage->MBSpeakInformation (L"This is spoken information.", L"Fine print.");
            return TRUE;
         }
      }
      return FALSE;  // pass onto default hander

      case ESCN_LISTBOXSELCHANGE:
         {
            PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;

            // only care about the search list control
            if (!p->pControl->m_pszName || wcsicmp(p->pControl->m_pszName, L"searchlist"))
               return TRUE;

            // if no name then ignore
            if (!p->pszName)
               return TRUE;

            // use the name as a link
            pPage->Link(p->pszName);
         }
         return TRUE;

      case ESCN_FILTEREDLISTQUERY:
         {
            // this is a message sent by the filtered list controls
            // used as samples in help.

            PESCNFILTEREDLISTQUERY p = (PESCNFILTEREDLISTQUERY) pParam;

            // only repsond to the people list
            if (!p->pszListName || wcsicmp(p->pszListName, L"people"))
               return FALSE;

            p->pList = GetList();
         }
         return TRUE;
   }
   return FALSE;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;
   gdwCurPage = IDR_MMLMAIN;
   giVScroll = -1;
   wcscpy (gszCurPageString, L"r:101");

   // Note: you would replace "sample" and "42" with your E-mail and the
   // registration key sent to you
   EscInitialize(L"sample", 42, 0);

   glistHistoryVScroll.Init (sizeof(int));

   // initialize the search, use a hash of the date for the version
   char szDate[] = __DATE__;
   DWORD dwSum, i;
   dwSum = 1;
   for (i = 0; i < strlen(szDate); i++)
      dwSum += (i * 0x100 * szDate[i] + szDate[i]);
   gSearch.Init (ghInstance, dwSum);

   RECT  r;
   r.left = GetSystemMetrics (SM_CXSCREEN) / 6;
   r.right = GetSystemMetrics (SM_CXSCREEN) - r.left;
   r.top = 32;
   r.bottom = GetSystemMetrics (SM_CYSCREEN) - r.top;
   CEscWindow  cWindow;
   
   WCHAR *psz;
   cWindow.Init (hInstance, NULL, 0, &r);
   cWindow.IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));
   while (TRUE) {
      psz = cWindow.PageDialog (ghInstance, gdwCurPage, PageMain, gszCurPageString);
      if (!psz)
         break;

      // see if it starts with r:
      if (!wcsnicmp (psz, L"r:", 2)) {
         // it's another page
         DWORD dwPage;
         dwPage = _wtoi (psz + 2);

         // remember the page
         glistHistory.Insert (0, gszCurPageString, (wcslen(gszCurPageString)+1)*2);
         glistHistoryVScroll.Insert (0, &cWindow.m_iExitVScroll);
         if (glistHistory.Num() > 50) {
            glistHistory.Remove(glistHistory.Num()-1);
            glistHistoryVScroll.Remove(glistHistoryVScroll.Num()-1);
         }
         
         // move on
         gdwCurPage = dwPage;
         giVScroll = -1;
         wcscpy (gszCurPageString, psz);
         continue;
      }
      else if (!wcsicmp (psz, L"back")) {
         PWSTR psz;
         psz = (PWSTR) glistHistory.Get (0);
         if (!psz)
            continue;   // cant go back

         wcscpy (gszCurPageString, psz);
         gdwCurPage = _wtoi (psz + 2);
         giVScroll = *((int*)glistHistoryVScroll.Get(0));
         glistHistory.Remove (0);
         glistHistoryVScroll.Remove(0);
         continue;
      }

      // else, quit
      break;
   }


   EscUninitialize();

   // done
   return 0;
}


