/*************************************************************************
Archive.cpp - Archiving feature

begun 8/31/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

#define  WM_SENDCOPY       (WM_USER+108)
#define  WM_SENDCOPY2      (WM_USER+109)
#define  WM_FROMCLIPBOARD  (WM_USER+110)
#define  WM_NEWKEYWORDS    (WM_USER+111)
#define  WM_TASKBAR        (WM_USER+112)

/* globals */
HWND        ghWndLastActive = NULL;
HWND        ghWndNextClip = NULL;
char        gszLastActive[256];
DWORD       gdwTimeSent;      // time that sent the copy message. 0 if didn't send
HWND        ghWndCapture = NULL;
static DWORD   gdwJournalNode = (DWORD)-1;   // if right click then also log to this journal node
static DWORD       gdwEntryNode;
DFDATE      gdateLog = Today();

char gszCaptureClass[] = "DragonflyCapture";
WCHAR gszArchiveOn[] = L"ArchiveOn";
WCHAR gszIconOn[]= L"IconOn";
WCHAR gszNoCtrlA[] = L"NoCtrlA";
WCHAR gszUseFirstLine[] = L"UseFirstLine";
WCHAR gszArchive[] = L"Archive";


/*****************************************************************************
GetArchiveLogNode - Given a date, this returns a month-specific node for the
call log.


inputs
   DFDATE         date - Date to look for
   BOOL           fCreateIfNotExit - if TRUE create the database node
                  if it doesn't exist. If FALSE, return NULL if it doesn't exist.
   DWORD          *pdwNode - Filled in with a node specific to the month/year.
returns
   PCMMLNode - Node (must be released) specific to the month/year. NULL if cant find/create.
*/
PCMMLNode GetArchiveLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode)
{
   HANGFUNCIN;
   PCMMLNode   pNew;

   // find it
   PCMMLNode pNode;
   pNode = FindMajorSection (gszArchiveNode);
   if (!pNode) {
      HANGFUNCOUT;
      return FALSE;   // unexpected error
   }

   pNew = MonthYearTree (pNode, date, L"Archive", fCreateIfNotExist, pdwNode);

   gpData->NodeRelease(pNode);

   HANGFUNCOUT;
   return pNew;
}

/****************************************************************************
ArchiveAddText - Adds text to the clipard.

inputs
   char     *pszText - ANSI text
   char     *pszTitle - ANSI title
   DFDATE   date - date to time stamp
   DFTIME   time - time to time stamp
   DWORD    dwJournalNode - Archive to this journal node also. -1 not to log
returns
   DWORD - Database index. -1 if error
*/
DWORD ArchiveAddText (char *pszText, char *pszTitle, DFDATE date, DFTIME time,
                      DWORD dwJournalNode = (DWORD) -1)
{
   HANGFUNCIN;
   // convert to unicode
   CMem  mem;
   if (!mem.Required((strlen(pszText)+2)*2)) {
      HANGFUNCOUT;
      return -1;
   }
   MultiByteToWideChar (CP_ACP, 0, pszText, -1, (PWSTR)mem.p, mem.m_dwAllocated/2);
   WCHAR szTitle[256];
   szTitle[0] = 0;
   MultiByteToWideChar (CP_ACP, 0, pszTitle, -1, szTitle, sizeof(szTitle)/2);
   if (!szTitle[0])
      wcscpy (szTitle, L"Unknown");


   // create the database entry
   DWORD dwNode;
   PCMMLNode   pNew;
   pNew = gpData->NodeAdd (gszArchiveEntryNode, &dwNode);
   if (!pNew) {
      HANGFUNCOUT;
      return (DWORD)-1;
   }
   NodeValueSet (pNew, gszName, szTitle);
   NodeValueSet (pNew, gszSummary, (PWSTR) mem.p);
   NodeValueSet (pNew, gszDate, (int) date);
   NodeValueSet (pNew, gszStart, (int) time);
   gpData->NodeRelease (pNew);

   // link it into the daily journal
   WCHAR szJournal[256+32];
   swprintf (szJournal, L"Archived document: %s", szTitle);
   CalendarLogAdd (date, time, -1, szJournal, dwNode);

   // log in journal
   if (dwJournalNode != (DWORD)-1) {
      JournalLink (dwJournalNode, szJournal, dwNode, date, time, -1);
   }

   // link it into the archive log
   PCMMLNode pLog;
   DWORD dwLog;
   pLog = GetArchiveLogNode(date, TRUE, &dwLog);
   if (pLog) {
      NodeElemSet (pLog, gszArchive, szTitle, (int) dwNode, TRUE,
         date, time, -1);
      gpData->NodeRelease(pLog);
   }

   // return
   HANGFUNCOUT;
   return dwNode;
}


/****************************************************************************
ArchiveAddTextFromClipboard - Gets the text from the clipboard and adds
it.

inputs
   HWND     hWnd - clipboard functions want this
   DWORD    dwJournalNode - Archive to this journal node also. -1 not to log
returns
   DWORD - database index, -1 if error
*/
DWORD ArchiveAddTextFromClipbard (HWND hWnd, DWORD dwJournalNode)
{
   HANGFUNCIN;
   // called when the contents of the clipboard should be analyzed
   if (!OpenClipboard (hWnd)) {
      EscChime (ESCCHIME_ERROR);
      EscSpeak (L"There's nothing on the clipboard. The application may not provide text to analyze.");
      HANGFUNCOUT;
      return (DWORD)-1;
   }

   HANDLE   h;



   h = GetClipboardData (CF_TEXT);
   DWORD dwRet;
   dwRet = (DWORD)-1;
   if (h) {
      char  *pszClip = (char*) GlobalLock(h);

      // title?
      // what's is supposed to be
      PCMMLNode pNode;
      pNode = FindMajorSection (gszJournalNode);
      if (!pNode) {
         HANGFUNCOUT;
         return 0;   // unexpected error
      }
      int   iOn;
      iOn = NodeValueGetInt (pNode, gszUseFirstLine, 0);
      gpData->NodeRelease (pNode);
      char* pszTitle;
      char szTemp[128];
      // BUGFIX - Allow to remember first line of text
      pszTitle = gszLastActive;
      if (iOn && pszClip) {
         int   iOffset;
         char *pc;
         // skip first white space
         for (pc = pszClip; *pc && iswspace (*pc); pc++);

         // fill in temp
         for (iOffset = 0; *pc && (*pc != '\r') && (*pc != '\n') && (iOffset < (sizeof(szTemp)-1)); iOffset++, pc++)
            szTemp[iOffset] = *pc;

         // done
         szTemp[iOffset] = 0;
         if (!iOffset)
            strcpy (szTemp, "No title");
         pszTitle = szTemp;
      }


      dwRet = ArchiveAddText (pszClip,pszTitle, Today(), Now(), dwJournalNode);
      GlobalUnlock (h);
      
      // it worked
      EscChime (ESCCHIME_INFORMATION);
      EscSpeak (L"Document archived.");
   }
   else {
      EscChime (ESCCHIME_ERROR);
      EscSpeak (L"There's nothing on the clipboard. The application may not provide text to analyze.");
   }

   CloseClipboard ();

   HANGFUNCOUT;
   return dwRet;
}


/****************************************************************************
CaptureWndProc - Handles capturing of text from clipboard
*/
long CALLBACK CaptureWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   HANGFUNCIN;
   switch (uMsg) {
   case WM_CREATE:
      {
         SetTimer (hWnd, 42, 1000, NULL);
         // add to the clipboard chain
         ghWndNextClip = SetClipboardViewer (hWnd);

         // turn on icon?
         NOTIFYICONDATA nid;
         memset (&nid, 0, sizeof(nid));
         nid.cbSize = sizeof(nid);
         nid.hWnd = hWnd;
         nid.uID = 42;
         nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
         nid.uCallbackMessage = WM_TASKBAR;
         nid.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_ARCHIVE));
         strcpy (nid.szTip, "Press this to archive the document.");
         Shell_NotifyIcon (NIM_ADD, &nid);

      }
      HANGFUNCOUT;
      return 0;

   case WM_DESTROY:
      {
         KillTimer (hWnd, 42);
         ChangeClipboardChain (hWnd, ghWndNextClip);

         // kill icon
         NOTIFYICONDATA nid;
         memset (&nid, 0, sizeof(nid));
         nid.cbSize = sizeof(nid);
         nid.hWnd = hWnd;
         nid.uID = 42;
         Shell_NotifyIcon (NIM_DELETE, &nid);
      }
      HANGFUNCOUT;
      return 0;

   case WM_TASKBAR:
      switch (lParam) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
         {
            // what's is supposed to be
            PCMMLNode pNode;
            pNode = FindMajorSection (gszJournalNode);
            gdwJournalNode = (DWORD)-1;   // default to not in journal
            if (!pNode) {
               HANGFUNCOUT;
               return 0;   // unexpected error
            }

            // BUGFIX - if it's a right button then create a menu and ask
            if (lParam == WM_RBUTTONDOWN) {
               PCListVariable pl = JournalListVariable();
               if (!pl->Num())
                  goto skiprbutton;
               HMENU hMenu;
               hMenu = CreatePopupMenu ();

               DWORD i;
               for (i = 0; i < pl->Num(); i++) {
                  PWSTR psz = (PWSTR) pl->Get(i);
                  char szTemp[128];
                  szTemp[0] = 0;
                  WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0,0);
                  if (!szTemp[0])
                     continue;

                  AppendMenu (hMenu, MF_ENABLED | MF_STRING, i+1, szTemp);
               }

               // handle menu
               int iRet;
               POINT p;
               GetCursorPos (&p);
               iRet = TrackPopupMenu (hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y,
                  0, hWnd, NULL);

               DestroyMenu (hMenu);

               if (iRet)
                  gdwJournalNode = JournalIndexToDatabase((DWORD)iRet-1);
               else
                  break;   // exited
            }

skiprbutton:
            int   iOn;
            iOn = NodeValueGetInt (pNode, gszNoCtrlA, 0);
            gpData->NodeRelease (pNode);

            PostMessage (hWnd, WM_SENDCOPY, (WPARAM) iOn, 0);
         }
         break;

      default:
         break;
      }
      HANGFUNCOUT;
      return 0;

   case WM_SENDCOPY:
      // switch to the last active window and send a copy
      SetForegroundWindow (ghWndLastActive);

      // hacks to work around bugs in NT
      Sleep (100);
      PostMessage (hWnd, WM_SENDCOPY2, wParam, lParam);
      HANGFUNCOUT;
      return 0;

   case WM_SENDCOPY2:
      // BUGFIX - Move timeset up above so not missing a copy-to-clipboard somehow
      gdwTimeSent = GetTickCount ();

      // sent select all and a copy
      keybd_event (VK_CONTROL, MapVirtualKey (VK_CONTROL, 0), 0,
         ((DWORD) MapVirtualKey (VK_CONTROL, 0) << 16) | 0);

      // BUGFIX - right click just copies the selection
      if (!wParam) {
         keybd_event ('A', MapVirtualKey ('A', 0), 0,
            ((DWORD) MapVirtualKey ('A', 0) << 16) | 0);
         keybd_event ('A', MapVirtualKey ('A', 0), KEYEVENTF_KEYUP,
            ((DWORD) MapVirtualKey ('A', 0) << 16) | (1<<30));
      }

      keybd_event ('C', MapVirtualKey ('C', 0), 0,
         ((DWORD) MapVirtualKey ('C', 0) << 16) | 0);
      keybd_event ('C', MapVirtualKey ('C', 0), KEYEVENTF_KEYUP,
         ((DWORD) MapVirtualKey ('C', 0) << 16) | (1<<30));

      keybd_event (VK_CONTROL, MapVirtualKey (VK_CONTROL, 0), KEYEVENTF_KEYUP,
         ((DWORD) MapVirtualKey (VK_CONTROL, 0) << 16) | (1<<30));

      HANGFUNCOUT;
      return 0;

   case WM_CHANGECBCHAIN:
      {
         HWND  hWndRemove = (HWND)wParam, hWndNext = (HWND) lParam;

         if (hWndRemove == ghWndNextClip) {
            // just removed the next one, so no next
            ghWndNextClip = hWndNext;
         }
         else {
            SendMessage (ghWndNextClip, WM_CHANGECBCHAIN, wParam, lParam);
         }
      }
      HANGFUNCOUT;
      return 0;

   case WM_DRAWCLIPBOARD:
      if (ghWndNextClip)
         SendMessage (ghWndNextClip, WM_DRAWCLIPBOARD, wParam, lParam);

      // if not expecting anything then return
      if (!gdwTimeSent) {
         HANGFUNCOUT;
         return 0;
      }

      // do something with clipboard
      PostMessage (hWnd, WM_FROMCLIPBOARD, 0, 0);

      HANGFUNCOUT;
      return 0;

   case WM_FROMCLIPBOARD:
      {
         // get text from clipboard
         if (ArchiveAddTextFromClipbard(hWnd,gdwJournalNode) != -1)
            gdwTimeSent = 0;  // BUGFIX - Don't clear unless get valid text from clipboard,
               //just in case app first adds one type of text and then changes its mind
      
      }
      HANGFUNCOUT;
      return 0;

   case WM_TIMER:
      // remember the last window with focus
      {
         // if it's been more than 5 seconds since sent then error
         // BUGFIX - wait 10 seconds
         if (gdwTimeSent && (GetTickCount() > (gdwTimeSent+10000))) {
            gdwTimeSent = 0;

            EscChime (ESCCHIME_ERROR);
            EscSpeak (L"Nothing was copied to the clipboard. You should copy the text "
                     L"to the clipboard yourself and use the Remember Document button.");
         }

         HWND  hWndActive;
         hWndActive = GetForegroundWindow ();
         if (hWndActive) {
            // make sure it's ont the tray
            char  szTemp[256];
            szTemp[0] = 0;
            GetClassName (hWndActive, szTemp, sizeof(szTemp));
            if (!_stricmp(szTemp, "Shell_traywnd")) {
               HANGFUNCOUT;
               return 0;
            }
            else {
               ghWndLastActive = hWndActive;
            }
         }
         else {
            HANGFUNCOUT;
            return 0;
         }

         // remember the window title
         GetWindowText (ghWndLastActive, gszLastActive, sizeof(gszLastActive));
      }
      HANGFUNCOUT;
      return 0;


   }

   // else
   HANGFUNCOUT;
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


static void RegisterCaptureWindow (void)
{
   HANGFUNCIN;
   WNDCLASS wc;

   memset (&wc, 0, sizeof(wc));
   wc.lpfnWndProc = CaptureWndProc;
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hInstance = ghInstance;
   wc.lpszClassName = gszCaptureClass;
   RegisterClass (&wc);
   HANGFUNCOUT;
}


/***********************************************************************
ArchiveIsOn - Returns TRUE if the archive taskbar is on.
*/
BOOL ArchiveIsOn (void)
{
   return ghWndCapture ? TRUE : FALSE;
}

/***********************************************************************
ArchiveInit - Turn archive on/off based upon the archive data note
setting. You can also call again if the node state has changed
*/
void ArchiveInit (void)
{
   HANGFUNCIN;
   // what's is supposed to be
   PCMMLNode pNode;
   pNode = FindMajorSection (gszJournalNode);
   if (!pNode) {
      HANGFUNCOUT;
      return;   // unexpected error
   }
   int   iOn;
   iOn = NodeValueGetInt (pNode, gszArchiveOn, 0);
   gpData->NodeRelease (pNode);

   // if off
   if (!iOn) {
      if (ghWndCapture) {
         DestroyWindow (ghWndCapture);
         ghWndCapture = NULL;
      }
      HANGFUNCOUT;
      return;
   }

   // else should be on
   if (ghWndCapture) {
      HANGFUNCOUT;
      return;  // already on
   }

      // register class
   RegisterCaptureWindow ();

   // create the window
   ghWndCapture = CreateWindow (
      gszCaptureClass, "Dragonfly Capture Window",
      0,
      0,0,0,0,
      NULL, NULL,
      ghInstance, NULL);


   HANGFUNCOUT;
   // done
}


/***********************************************************************
ArchiveOnOff - Turn archive on/off.

inputs
   BOOL     fOn - If true turn it on
*/
void ArchiveOnOff (BOOL fOn)
{
   HANGFUNCIN;
   // what's is supposed to be
   PCMMLNode pNode;
   pNode = FindMajorSection (gszJournalNode);
   if (!pNode) {
      HANGFUNCOUT;
      return;   // unexpected error
   }
   NodeValueSet (pNode, gszArchiveOn, (int) fOn);
   gpData->NodeRelease (pNode);
   gpData->Flush();

   // init if necessary
   ArchiveInit();
   HANGFUNCOUT;
}

/***********************************************************************
ArchiveShutDown - Shutton down, so make sure it's off.
*/
void ArchiveShutDown (void)
{
   HANGFUNCIN;
   if (ghWndCapture) {
      DestroyWindow (ghWndCapture);
      ghWndCapture = NULL;
   }
   HANGFUNCOUT;
}

/***********************************************************************
ArchiveOptionsPage - Page callback
*/
BOOL ArchiveOptionsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // check/uncheck based upon is archive on
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszIconOn);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, ArchiveIsOn());

         // check/uncheck based upon sent ctrl-A
         PCMMLNode pNode;
         pNode = FindMajorSection (gszJournalNode);
         if (!pNode)
            break;   // unexpected error
         pControl = pPage->ControlFind (gszNoCtrlA);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, (BOOL)NodeValueGetInt (pNode, gszNoCtrlA, 0));
         // BUGFIX - Allow to remember first line of text
         pControl = pPage->ControlFind (gszUseFirstLine);
         if (pControl)
            pControl->AttribSetBOOL (gszChecked, (BOOL)NodeValueGetInt (pNode, gszUseFirstLine, 0));
         gpData->NodeRelease (pNode);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszIconOn)) {
            ArchiveOnOff (p->pControl->AttribGetBOOL (gszChecked));
            HANGFUNCOUT;
            return TRUE;
         }

         // handle ctrl-A option
         if (!_wcsicmp(p->pControl->m_pszName, gszNoCtrlA)) {
            PCMMLNode pNode;
            pNode = FindMajorSection (gszJournalNode);
            if (!pNode) {
               HANGFUNCOUT;
               return TRUE;   // unexpected error
            }
            NodeValueSet (pNode, gszNoCtrlA, p->pControl->AttribGetBOOL(gszChecked));
            gpData->NodeRelease (pNode);
            gpData->Flush();
            HANGFUNCOUT;
            return TRUE;
         }

         // handle firstline option
         // BUGFIX - Allow to remember first line of text
         if (!_wcsicmp(p->pControl->m_pszName, gszUseFirstLine)) {
            PCMMLNode pNode;
            pNode = FindMajorSection (gszJournalNode);
            if (!pNode) {
               HANGFUNCOUT;
               return TRUE;   // unexpected error
            }
            NodeValueSet (pNode, gszUseFirstLine, p->pControl->AttribGetBOOL(gszChecked));
            gpData->NodeRelease (pNode);
            gpData->Flush();
            HANGFUNCOUT;
            return TRUE;
         }
      }
      break;


   };


   HANGFUNCOUT;
   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
ArchiveReadInPossum - Read in one possum file and return the number of
articles red.

inputs
   char     *pszFile - file to read
returns
   DWORD - Number of articles read.
*/
DWORD ArchiveReadInPossum (char *pszFile)
{
   HANGFUNCIN;
   DWORD dwNumArt = 0;

   char  szPossum[] = "***POSSUM.";
   char  *pszRet = NULL;

   FILE  *f;
   f = fopen(pszFile, "rt");
   if (!f)
      return FALSE;

   // allocate enough memory for everything
   fseek (f, 0, SEEK_END);
   long lSize;
   lSize = ftell (f);
   fseek (f, 0, 0);

   char  *pMem;
   pMem = (char*) malloc (lSize+1);
   lSize = fread (pMem, 1, lSize, f);
   pMem[lSize] = 0;

   fclose (f);

   char  *pCurArticle;
   pCurArticle = pMem;

   while (pCurArticle && pCurArticle[0]) {
      FILETIME ft;
      char szName[512];
      memset (&ft, 0, sizeof(ft));
      szName[0] = 0;

      // search through memory for beginning
      char szArticle[] = "***POSSUM.Article=";
      char  *pf, *pEnd;
      pf = strstr (pCurArticle, szArticle);
      if (!pf)
         goto done;
      pf += (strlen(szArticle)-1);
      // ignore the article number

      while (TRUE) {
         // find the next line
         pf = strchr (pf + 1, '\n');
         if (!pf)
            goto done;
         pf++;
         if (pf[0] == '\r')
            pf++;

         // if if starts with possum then analyze
         if (strncmp (pf, szPossum, strlen(szPossum)))
            break;   // found text

         // else, parse
         pf += strlen(szPossum);

         char  szDate[] = "Date=";
         char  szTitle[] = "Title=";
         if (!strncmp(pf, szDate, strlen(szDate))) {
            pf += strlen(szDate);
            ft.dwHighDateTime = atoi(pf);
            pf = strchr (pf, ' ');
            if (!pf)
               goto done;
            pf++;
            ft.dwLowDateTime = atoi(pf);
         }
         else if (!strncmp(pf, szTitle, strlen(szTitle))) {
            pf += strlen(szTitle);
            pEnd = strchr (pf, '\n');
            if (!pEnd)
               goto done;

            if ((DWORD) (pEnd - pf) > (sizeof(szName)-1))
               pEnd = pf + (sizeof(szName)-1);
            memcpy (szName, pf, pEnd - pf);
            szName[pEnd-pf] = 0;
         }
      }

      // find the end
      char  *pfEnd;
      pfEnd = strstr (pf, szPossum);
      if (!pfEnd)
         pfEnd = pf + strlen(pf);

      // temporarily store pfend
      char c;
      c = *pfEnd;
      *pfEnd = 0;

      FILETIME lft;
      SYSTEMTIME  st;
      if (ft.dwHighDateTime || ft.dwLowDateTime) {
         FileTimeToLocalFileTime (&ft, &lft);
         FileTimeToSystemTime (&lft, &st);
      }
      else {
         GetLocalTime (&st);
      }
      ArchiveAddText (pf, szName, TODFDATE(st.wDay, st.wMonth, st.wYear), TODFTIME(st.wHour, st.wMinute));

      *pfEnd = c;

      // must advance pCurArticle
      pCurArticle = pEnd;
      dwNumArt++;
   }

done:
   // finall, free memory
   free (pMem);
   return dwNumArt;

}


/***********************************************************************
ArchivePage - Page callback
*/
BOOL ArchivePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"remember")) {
            // get the journal node
            PCEscControl pControl;
            DWORD dwRetJournal;
            pControl = pPage->ControlFind (gszJournal);
            dwRetJournal = pControl ? (DWORD) pControl->AttribGetInt (gszCurSel) : (DWORD)-1;

            DWORD dwRet;
            dwRet = ArchiveAddTextFromClipbard(pPage->m_pWindow->m_hWnd,
               JournalIndexToDatabase(dwRetJournal));

            if (dwRet == (DWORD)-1) {
               pPage->MBSpeakWarning (L"No text was on the clipboard.");
            }
            else {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Document archived.");

               // view it
               WCHAR szTemp[16];
               swprintf (szTemp, L"v:%d", (int)dwRet);
               pPage->Exit(szTemp);
            }
            HANGFUNCOUT;
            return TRUE;
         }

         if (!_wcsicmp(p->pControl->m_pszName, L"import")) {
            // alert the user what they need to do in the open dialog
            if (IDOK != pPage->MBInformation(
               L"Use the upcoming file open dialog box to select one of possum's data files.",
               L"Possum stores its data in \"dXXXX\" subdirectories (where XXXX is a number) in the same directory "
               L"where Possum.exe is located. The datafiles are fXXXX.txt, where XXXX is a number. "
               L"Use the file open dialog to open one of these and process them. Repeat this "
               L"procedure until all the files have been read in.", TRUE))
               return TRUE;   // user cancelled out of it

            // get the file name
            OPENFILENAME   ofn;
            HWND  hWnd = pPage->m_pWindow->m_hWnd;
            char  szTemp[256];
            szTemp[0] = 0;

            memset (&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter =
               "Possum data file (*.txt)\0*.txt\0"
               "\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open Possum Data File";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "txt";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;

            // read it in
            DWORD dwNum;
            dwNum = ArchiveReadInPossum (szTemp);
            WCHAR szw[256];
            if (dwNum) {
               swprintf (szw, L"%d articles converted.", (int)dwNum);
            }
            else {
               wcscpy (szw, L"Sorry, the file does not appear to be a possum file.");
            }
            // keep this a speak information
            pPage->MBSpeakInformation (szw);

            return TRUE;
         }
      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
ArchiveViewPage - Page callback
*/
BOOL ArchiveViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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

         if (!_wcsicmp(p->pszSubName, L"TIME")) {
            PCMMLNode pNode;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwEntryNode);
            if (pNode) {
               WCHAR szTemp[64];
               DFDATEToString ((DFDATE) NodeValueGetInt (pNode, gszDate, 0), szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString ((DFTIME) NodeValueGetInt (pNode, gszStart, -1), szTemp);
               MemCat (&gMemTemp, szTemp);
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
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwEntryNode);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
ArchiveEditPage - Page callback
*/
BOOL ArchiveEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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

      }
      break;

   case ESCM_INITPAGE:
      {
         // save the information away
         PCMMLNode pNode;
         pNode = gpData->NodeGet (gdwEntryNode);
         if (!pNode)
            return FALSE;   // unexpected error

         // date
         DFDATE date;
         DFTIME time;
         date = (DFDATE) NodeValueGetInt (pNode, gszDate, 0);
         time = (DFTIME) NodeValueGetInt (pNode, gszStart, -1);

         DateControlSet (pPage, gszDate, date);
         TimeControlSet (pPage, gszStart, time);

         gpData->NodeRelease(pNode);
      }
      break;

   case ESCM_LINK:
      {
         // save the information away
         PCMMLNode pNode;
         pNode = gpData->NodeGet (gdwEntryNode);
         if (!pNode)
            return FALSE;   // unexpected error

         // get the text
         CMem mem;
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszSummary);
         if (pControl) {
            DWORD dwNeeded;
            dwNeeded = 0;
            pControl->AttribGet(gszText, NULL, 0, &dwNeeded);
            dwNeeded += 2;
            mem.Required (dwNeeded);
            pControl->AttribGet (gszText, (PWSTR) mem.p, dwNeeded, &dwNeeded);
            NodeValueSet (pNode, gszSummary, (PWSTR) mem.p);
         }


         // get the old date and remove it from the archive list
         DFDATE olddate;
         DFTIME oldtime;
         olddate = (DFDATE) NodeValueGetInt (pNode, gszDate, 0);
         oldtime = (DFTIME) NodeValueGetInt (pNode, gszStart, -1);
         PCMMLNode pLog;
         DWORD dwLog;
         pLog = GetArchiveLogNode(olddate, TRUE, &dwLog);
         if (pLog) {
            NodeElemRemove (pLog, gszArchive, gdwEntryNode);
            gpData->NodeRelease(pLog);
         }

         // date
         DFDATE date;
         DFTIME time;
         date = DateControlGet (pPage, gszDate);
         time = TimeControlGet (pPage, gszStart);
         NodeValueSet (pNode, gszDate, (int) date);
         NodeValueSet (pNode, gszStart, (int) time);


         // title
         WCHAR szTemp[256];
         pControl = pPage->ControlFind (gszName);
         if (pControl) {
            szTemp[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            NodeValueSet (pNode, gszName, szTemp);

            // change the title
            PCMMLNode pLog;
            DWORD dwLog;
            pLog = GetArchiveLogNode(date, TRUE, &dwLog);
            if (pLog) {
               NodeElemSet (pLog, gszArchive, szTemp, (int) gdwEntryNode, TRUE,
                  date, time, -1);
               gpData->NodeRelease(pLog);
            }

         }

         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default
   }

   return DefPage (pPage, dwMessage, pParam);
}

/**********************************************************************
ArchiveEntrySet - Sets the category node to view
*/
void ArchiveEntrySet (DWORD dwNode)
{
   HANGFUNCIN;
   gdwEntryNode = dwNode;
}

/***********************************************************************
ArchiveListPage - Page callback
*/
BOOL ArchiveListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   static BOOL gfRemoveMode = FALSE;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set the dropdown to the right date
         DateControlSet (pPage, gszDate, gdateLog);

         // make sure we're not in remove mode
         gfRemoveMode = FALSE;
      }
      break;   // go to default handler

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // BUGFIX - Allow person to delete entrye
         if (gfRemoveMode && p->psz && (p->psz[0] == L'v') && (p->psz[1] == L':')) {
            gfRemoveMode = FALSE;

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to delete this archive?"))
               return TRUE;   // eat up the link

            // delete this
            PCMMLNode pNode;
            DWORD dwNode;
            pNode = GetArchiveLogNode (gdateLog, FALSE, &dwNode);

            if (pNode) {
               NodeElemRemove (pNode, gszArchive, _wtoi(p->psz+2));
               gpData->NodeRelease (pNode);
               gpData->NodeDelete (_wtoi(p->psz+2));
               gpData->Flush();
            }

            pPage->Link(gszRedoSamePage);
            return TRUE;
         }
      }
      break;   // fall through

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
            pNode = GetArchiveLogNode (gdateLog, FALSE, &dwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszArchive, FALSE);

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
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // if no entries then say so
            if (!pl || !pl->Num())
               MemCat (&gMemTemp, L"<tr><td>No documents archived for this month.</td></tr>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

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

         if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            // BUGFIX - Allow to remove from journal

            pPage->MBSpeakInformation (L"Click on the archived document you wish to delete.");
            gfRemoveMode = TRUE;
         }

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



// BUGBUG - 2.0 - Save away rich text in addition to plain text

// BUGBUG - 2.0 - Have a way for people to delete bad archives of text

