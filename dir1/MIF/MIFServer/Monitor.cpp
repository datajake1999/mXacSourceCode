/*************************************************************************************
Monitor.cpp - Code for monitoring the server.

begun 24/9/07 by Mike Rozak.
Copyright 2007 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <direct.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"


#ifdef _DEBUG
#define TIMETOWAIT               10000    // wait 10 seconds... just to be fast
#else
#define TIMETOWAIT               60000    // wait 60 seconds
#endif

#define TIMDERID_SHUTDOWN        1023     // timer ID to indicate a shutdown
#define TIMDERID_FINDWINDOW      1024     // find the window
#define TIMDERID_HEARTBEAT       1025     // wait foe a heartbeat

static UINT_PTR gHeartbeatTimerID = 0;    // timer iD
static WCHAR gszHeartbeatFile[256];       // file for the heartbeat
static PCEmailThread gpEmailThread = NULL;
static HWND ghWndEdit = NULL;
static WORLDMONITOR gWM;      // load the world monitor
static DWORD gdwWorldNum;        // world number to monitor
static DWORD gdwMonitorTryToFindCount; // number of times tried to find window
static DWORD gdwMonitorHeartbeatCount; // number of times waited for heartbeat
static BOOL gfMonitorSuccess = TRUE;

/*************************************************************************************
MonitorFindServerWindow - Finds a server window.

inputs
   PWSTR          pszFile - File name, like "c:\world\myworld.crf"
returns
   BOOL - TRUE if found
*/
BOOL MonitorFindServerWindow (PCWSTR pszFile)
{
   // figure out what the title should be
   char szTitle[512];
   MainWindowTitle (pszFile, szTitle, sizeof(szTitle));
   DWORD dwLen = (DWORD)strlen(szTitle);

   // loop through all the windows
   // HWND hWndDesktop = GetDesktopWindow ();

   HWND hWnd;
   char szTemp[512];
   for (hWnd = GetTopWindow (NULL); hWnd; hWnd = GetNextWindow(hWnd, GW_HWNDNEXT)) {
      if (!GetClassName (hWnd, szTemp, sizeof(szTemp)))
         continue;

      // string compare
      if (_stricmp(szTemp, gpszCircumRealityServerMainWindow))
         continue;   // not the same

      // get the text
      if (!GetWindowText (hWnd, szTemp, sizeof(szTemp)))
         continue;

      // compare
      if (strlen(szTemp) < dwLen)
         continue;   // to short
      szTemp[dwLen] = 0;   // so can compare root
      if (!_stricmp(szTemp, szTitle))
         return TRUE;
   } // i

   // else fail
   return FALSE;
}

/*************************************************************************************
MonitorFilenameToRegValue - Converts the filename of the .crf to a registry
name value where the date/time stamp will be written/read.

inputs
   PCWSTR         pszFile - Filename, like c:\server\myworld.crf
   char           *pszValueName - Filled with the value name
   DWORD          dwValueNameSize - size of the value name
returns
   none
*/
void MonitorFilenameToRegValue (PCWSTR pszFile, char *pszValueName, DWORD dwValueNameSize)
{
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, pszValueName, dwValueNameSize, 0, 0);

   // get rid of backslash
   char *pc = pszValueName;
   while (pc = strchr(pc, '\\')) {
      *pc = '_';
      pc++;
   }
}


/*************************************************************************************
MonitorHeartbeatUpdate - Updates the registry entry with a new heartbeat time
so that know alive.

inputs
   PCWSTR         pszFile - Filename of the .crf file
returns
   none
*/
void MonitorHeartbeatUpdate (PCWSTR pszFile)
{
   FILETIME ft;
   GetSystemTimeAsFileTime (&ft);

   char szTemp[256];
   MonitorFilenameToRegValue (pszFile, szTemp, sizeof(szTemp));

   // key to the registry
   HKEY hKey = NULL;
   RegCreateKey (HKEY_CURRENT_USER, gpszServerKey, &hKey);

   RegSetValueEx (hKey, szTemp, 0, REG_BINARY, (LPBYTE)&ft, sizeof(ft));

   // done
   RegCloseKey (hKey);
}




/*************************************************************************************
MonitorHeartbeatCheck - Checks the heartbeat, to make sure it's within a reasonable
amount of time.

inputs
   PCWSTR         pszFile - Filename of the .crf file
returns
   BOOL - If is recent enough, FALSE if not
*/
BOOL MonitorHeartbeatCheck (PCWSTR pszFile)
{
   FILETIME ft, ftBeat;
   GetSystemTimeAsFileTime (&ft);

   char szTemp[256];
   MonitorFilenameToRegValue (pszFile, szTemp, sizeof(szTemp));

   // key to the registry
   HKEY hKey = NULL;
   RegCreateKey (HKEY_CURRENT_USER, gpszServerKey, &hKey);

   DWORD dwSize = sizeof(ftBeat);
   DWORD dwType;
   memset (&ftBeat, 0, sizeof(ftBeat));
   RegQueryValueEx (hKey, szTemp, 0, &dwType, (LPBYTE)&ftBeat, &dwSize);

   // done
   RegCloseKey (hKey);

   __int64 iNow = *((__int64*)&ft);
   __int64 iThen = *((__int64*)&ftBeat);

   iNow -= iThen;
   iNow /= 10000000;   // 100-nanoseconds per

   // must be within 2 minutes
   return ((iNow > -120) && (iNow < 120));
}

/*************************************************************************************
MonitorInfoFromReg - Read in the monitoring information from the registry.

inputs
   PWORLDMONITOR     pWM - Filled in
returns
   none
*/
void MonitorInfoFromReg (PWORLDMONITOR pWM)
{
   memset (pWM, 0, sizeof(pWM));
   pWM->fEmailOnSuccess = TRUE;
   DWORD i;
   for (i = 0; i < NUMWORLDSTOMONITOR; i++)
      pWM->adwShardNum[i] = 1;   // default

   // key to the registry
   HKEY hKey = NULL;
   RegCreateKey (HKEY_CURRENT_USER, gpszServerKey, &hKey);

   DWORD dwType;
   DWORD dwSize;
   char szTemp[256];
   char szName[256];

   for (i = 0; i < NUMWORLDSTOMONITOR; i++) {
      szTemp[0] = 0;
      dwSize = sizeof(szTemp);
      sprintf (szName, "WorldFile%d", (int)i);
      RegQueryValueEx (hKey, szName, 0, &dwType, (LPBYTE)szTemp, &dwSize);
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, pWM->aszWorldFile[i], sizeof(pWM->aszWorldFile[i]));

      sprintf (szName, "ShardNum%d", (int)i);
      dwSize = sizeof(pWM->adwShardNum[i]);
      RegQueryValueEx (hKey, szName, 0, &dwType, (LPBYTE)&pWM->adwShardNum[i], &dwSize);
   } // i

   PSTR  apszFields[] = {"Domain", "SMTPServer", "EmailTo", "EmailFrom", "AuthUser", "AuthPassword"};
   PWSTR apszFieldsTo[] = {pWM->szDomain, pWM->szSMTPServer, pWM->szEMailTo, pWM->szEMailFrom,
      pWM->szAuthUser, pWM->szAuthPassword};
   for (i = 0; i < sizeof(apszFields) / sizeof(apszFields[0]); i++) {
      szTemp[0] = 0;
      dwSize = sizeof(szTemp);
      RegQueryValueEx (hKey, apszFields[i], 0, &dwType, (LPBYTE)szTemp, &dwSize);
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, apszFieldsTo[i], sizeof(pWM->szAuthPassword));
   } // i

   dwSize = sizeof(pWM->fEmailOnSuccess);
   RegQueryValueEx (hKey, "EmailOnSuccess", 0, &dwType, (LPBYTE)&pWM->fEmailOnSuccess, &dwSize);

   // done
   RegCloseKey (hKey);
}



/*************************************************************************************
MonitorInfoToReg - Write the monitoring information from the registry.

inputs
   PWORLDMONITOR     pWM - Should have all the info in
returns
   none
*/
void MonitorInfoToReg (PWORLDMONITOR pWM)
{
   // key to the registry
   HKEY hKey = NULL;
   RegCreateKey (HKEY_CURRENT_USER, gpszServerKey, &hKey);

   char szTemp[256];
   DWORD i;
   char szName[256];

   for (i = 0; i < NUMWORLDSTOMONITOR; i++) {
      szTemp[0] = 0;
      sprintf (szName, "WorldFile%d", (int)i);
      WideCharToMultiByte (CP_ACP, 0, pWM->aszWorldFile[i], -1, szTemp, sizeof(szTemp), 0, 0);
      RegSetValueEx (hKey, szName, 0, REG_SZ, (LPBYTE)szTemp, (DWORD)strlen(szTemp)+1);

      sprintf (szName, "ShardNum%d", (int)i);
      RegSetValueEx (hKey, szName, 0, REG_DWORD, (LPBYTE)&pWM->adwShardNum[i], sizeof(pWM->adwShardNum[i]));
   } // i

   PSTR  apszFields[] = {"Domain", "SMTPServer", "EmailTo", "EmailFrom", "AuthUser", "AuthPassword"};
   PWSTR apszFieldsTo[] = {pWM->szDomain, pWM->szSMTPServer, pWM->szEMailTo, pWM->szEMailFrom,
      pWM->szAuthUser, pWM->szAuthPassword};
   for (i = 0; i < sizeof(apszFields) / sizeof(apszFields[0]); i++) {
      szTemp[0] = 0;
      WideCharToMultiByte (CP_ACP, 0, apszFieldsTo[i], -1, szTemp, sizeof(szTemp), 0, 0);
      RegSetValueEx (hKey, apszFields[i], 0, REG_SZ, (LPBYTE)szTemp, (DWORD)strlen(szTemp)+1);
   } // i

   RegSetValueEx (hKey, "EmailOnSuccess", 0, REG_DWORD, (LPBYTE)&pWM->fEmailOnSuccess, sizeof(pWM->fEmailOnSuccess));

   // done
   RegCloseKey (hKey);
}





/*****************************************************************************
MonitorTimer - Receives the timer messages
*/
VOID CALLBACK MonitorTimer(
  HWND hwnd,         // handle to window
  UINT uMsg,         // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime       // current system time
)
{
   if (gszHeartbeatFile[0])
      MonitorHeartbeatUpdate (gszHeartbeatFile);
}



/*****************************************************************************
MonitorHeartbeatTimerStop - Stop the heardbeat timer
*/
void MonitorHeartbeatTimerStop (void)
{
   if (gHeartbeatTimerID)
      KillTimer (NULL, gHeartbeatTimerID);
   gHeartbeatTimerID = 0;
}

/*****************************************************************************
MonitorHeartbeatTimerStart - Starts the timer that writes the heartbeat.

inputs
   PWSTR       pszFile - File for the heartbeat
*/
void MonitorHeartbeatTimerStart (PCWSTR pszFile)
{
   if (gHeartbeatTimerID)
      MonitorHeartbeatTimerStop ();

   if (!pszFile[0])
      return;  // shouldnt happen

   wcscpy (gszHeartbeatFile, pszFile);

   // update the heartbeat right now
   MonitorHeartbeatUpdate (pszFile);

   gHeartbeatTimerID = SetTimer (NULL, 0, 30000 /* 30 sec */, MonitorTimer);

}



/************************************************************************************
MonitorWindowText - Displays text in the montior window

inputs
   PWSTR          pszText - Text
*/
void MonitorWindowText (PWSTR pszText)
{
   if (!ghWndEdit)
      return;

   CMem mem;
   DWORD dwLen = (DWORD) wcslen(pszText)+1;
   if (!mem.Required(dwLen * 2))
      return;  // shouldnt happen
   WideCharToMultiByte (CP_ACP, 0, pszText, -1, (char*)mem.p, (int) mem.m_dwAllocated, 0, 0);

   // append text
   int iLength = GetWindowTextLength (ghWndEdit);
   SendMessage (ghWndEdit, EM_SETSEL, (WPARAM) iLength, (WPARAM) iLength);
   SendMessage (ghWndEdit, EM_REPLACESEL, FALSE, (LPARAM)mem.p);
   iLength = GetWindowTextLength (ghWndEdit);
   SendMessage (ghWndEdit, EM_SETSEL, (WPARAM) iLength, (WPARAM) iLength);
   SendMessage (ghWndEdit, EM_SCROLLCARET, 0, 0);
}

/************************************************************************************
MonitorMail - Send E-mail from the monitoring app. Uses the text of the
display for the body.

inputs
   PWSTR          pszTitle - Title
returns
   none
*/
void MonitorMail (PWSTR pszTitle)
{
   MonitorWindowText (L"\r\n\r\nSending mail: ");
   MonitorWindowText (pszTitle);
   MonitorWindowText (L"\r\nTo: ");
   MonitorWindowText (gWM.szEMailTo);
   MonitorWindowText (L"\r\nFrom: ");
   MonitorWindowText (gWM.szEMailFrom);
   MonitorWindowText (L"\r\nSMTPServer: ");
   MonitorWindowText (gWM.szSMTPServer);
   MonitorWindowText (L"\r\nDomain: ");
   MonitorWindowText (gWM.szDomain);
   MonitorWindowText (L"\r\nAuthUser: ");
   MonitorWindowText (gWM.szAuthUser);
   MonitorWindowText (L"\r\nAuthPassword: Not shown for security reasons");

   DWORD dwLength = GetWindowTextLengthW (ghWndEdit);
   CMem mem;
   if (!mem.Required ((dwLength+1)*sizeof(WCHAR)))
      return;  // shouldnt happen
   PWSTR pszMessage = (PWSTR)mem.p;
   GetWindowTextW (ghWndEdit, pszMessage, (DWORD)mem.m_dwAllocated / sizeof(WCHAR));

   BOOL fRet = gpEmailThread->Mail (gWM.szDomain, gWM.szSMTPServer, gWM.szEMailTo, gWM.szEMailFrom, L"CircumReality Monitoring Application",
      pszTitle, pszMessage, gWM.szAuthUser, gWM.szAuthPassword);

   if (!fRet)
      MonitorWindowText (L"\r\nMail failed!");
}


/************************************************************************************
MonitorWindowWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK MonitorWindowWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE: 
      {
         ghWndEdit = CreateWindow ("EDIT", "Starting monitoring application\r\n",
            WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_HSCROLL | WS_VSCROLL,
            0, 0, 10, 10,
            hWnd, NULL, ghInstance, NULL);

         // start processing
         PostMessage (hWnd, WM_USER+100, 0, 0);
      }
      break;

   case WM_USER+100: // see about  monitoring the next .crf file
      {
         gdwWorldNum++;

         if (gdwWorldNum >= NUMWORLDSTOMONITOR) {
            // all done
            MonitorWindowText (L"\r\n\r\nMonitoring completed. Shutting down in 60 seconds.");

            // may want to send Email
            if (gWM.fEmailOnSuccess && gfMonitorSuccess)
               MonitorMail (L"CircumRealtiy server running properly.");

            SetTimer (hWnd, TIMDERID_SHUTDOWN, TIMETOWAIT, NULL);
            return 0;
         }

         // if this is empty then next
         if (!gWM.aszWorldFile[gdwWorldNum][0]) {
            PostMessage (hWnd, WM_USER + 100, 0, 0);
            return 0;
         }

         // say that checking
         MonitorWindowText (L"\r\n\r\nChecking world: ");
         MonitorWindowText (gWM.aszWorldFile[gdwWorldNum]);

         // set some globals
         gdwMonitorTryToFindCount = 0;
         gdwMonitorHeartbeatCount = 0;

         // look for the window
         PostMessage (hWnd, WM_USER + 102, 0, 0);
      }
      return 0;

   case WM_USER+101: // try to find the window
      {
         if (MonitorFindServerWindow(gWM.aszWorldFile[gdwWorldNum])) {
            MonitorWindowText (L"\r\nFound the window.");

            // go onto next world
            PostMessage (hWnd, WM_USER + 100, 0, 0);
            return 0;
         }

         MonitorWindowText (L"\r\nWindow not found.");

         // if can't find it
         if (!gdwMonitorTryToFindCount) {
            // set a timer to retry
            MonitorWindowText (L"\r\nWaiting 60 seconds and looking again.");
            SetTimer (hWnd, TIMDERID_FINDWINDOW, TIMETOWAIT, NULL);
            return 0;
         }

         MonitorWindowText (L"\r\nRunning the server");

         // try to winexec
         // so that will restart with existing file
         WCHAR szCmdLine[512];
         WCHAR szAppPath[512];
         swprintf (szCmdLine, L"-%d %s", (int)gWM.adwShardNum[gdwWorldNum], gWM.aszWorldFile[gdwWorldNum]);
         MultiByteToWideChar (CP_ACP, 0, gszAppPath, -1, szAppPath, sizeof(szAppPath)/sizeof(WCHAR));
         WCHAR szDir[512];
         _wgetcwd (szDir, sizeof(szDir) / sizeof(WCHAR));
         HINSTANCE hInst = ShellExecuteW (hWnd, NULL, szAppPath,
            szCmdLine, szDir, SW_SHOW);

         PWSTR pszMailTitle;
         if ((__int64)hInst <= 32) {
            MonitorWindowText (L"\r\nShell execute failed with:");
            MonitorWindowText (L"\r\nPath=");
            MonitorWindowText (szAppPath);
            MonitorWindowText (L"\r\nCommand line=");
            MonitorWindowText (szCmdLine);
            MonitorWindowText (L"\r\nDirectory=");
            MonitorWindowText (szDir);
            swprintf (szCmdLine, L"\r\nReturn=%d", (int)(__int64)hInst);
            MonitorWindowText (szCmdLine);

            pszMailTitle = L"CircumReality SERVER FAILED TO START";
         }
         else {
            MonitorWindowText (L"\r\nServer ran successfully.");
            pszMailTitle = L"CircumReality server started";
         }

         // will need to send Email
         MonitorMail (pszMailTitle);
         gfMonitorSuccess = FALSE;  // since send one Email


         // go onto next world
         PostMessage (hWnd, WM_USER + 100, 0, 0);
         return 0;
      }
      return 0;

   case WM_USER+102: // try to find a heartbeat
      {
         if (MonitorHeartbeatCheck(gWM.aszWorldFile[gdwWorldNum])) {
            MonitorWindowText (L"\r\nHeartbeat found.");

            // go onto next world
            PostMessage (hWnd, WM_USER + 100, 0, 0);
            return 0;
         }

         MonitorWindowText (L"\r\nHeartbeat NOT found.");

         // else
         if (gdwMonitorHeartbeatCount > 1) {
            MonitorWindowText (L"\r\nNo heartbeat. Checking to see if window exists.");
            // try to find the window
            PostMessage (hWnd, WM_USER + 101, 0, 0);
            return TRUE;
         }

         // else, wait
         MonitorWindowText (L"\r\nWaiting 60 seconds.");
         SetTimer (hWnd, TIMDERID_HEARTBEAT, TIMETOWAIT, NULL);
         return 0;
         
      }
      return 0;

   case WM_TIMER:
      switch (wParam) {
      case TIMDERID_SHUTDOWN:
         // shut down now
         KillTimer (hWnd, TIMDERID_SHUTDOWN);
         DestroyWindow (hWnd);
         return 0;

      case TIMDERID_FINDWINDOW:
         KillTimer (hWnd, TIMDERID_FINDWINDOW);

         // waited, so try to find again
         gdwMonitorTryToFindCount++;
         PostMessage (hWnd, WM_USER + 101, 0, 0);
         return 0;

      case TIMDERID_HEARTBEAT:
         KillTimer (hWnd, TIMDERID_HEARTBEAT);

         // waited, so try to find again
         gdwMonitorHeartbeatCount++;
         PostMessage (hWnd, WM_USER + 102, 0, 0);
         return 0;

      } // switch
      break;

   case WM_SIZE:
      {
         // resize the edit
         MoveWindow (ghWndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
      }
      break;

   case WM_DESTROY:
      ghWndEdit = NULL;
      PostQuitMessage (0);
      break;

   };

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}





/*************************************************************************************
MonitorAll - This creates a window to show the monitoring status,
checks monitoring of everything, and then shuts down the window when done.
*/
void MonitorAll (void)
{
   // make sure E-mail thread is going
   CEmailThread EmailThread;
   gpEmailThread = &EmailThread;

   // load the world monitor
   MonitorInfoFromReg (&gWM);

   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = MonitorWindowWndProc;
   wc.lpszClassName = "CircumrealityMonitorWindow";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_APPICON));
   wc.hCursor = NULL; // LoadCursor (NULL, IDC_ARROW);
   wc.hbrBackground = NULL; // (HBRUSH)(COLOR_WINDOW+1);
   RegisterClass (&wc);

   gdwWorldNum = (DWORD)-1;
   gfMonitorSuccess = TRUE;  // assume success

   HWND hWnd = CreateWindow (
      wc.lpszClassName, "Circumreality World Monitor",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT ,CW_USEDEFAULT ,CW_USEDEFAULT ,CW_USEDEFAULT , NULL,
      NULL, ghInstance, NULL);
   if (!hWnd)
      goto done;  // error. shouldt happen

   // window loop
   MSG msg;
   while( GetMessage( &msg, NULL, 0, 0 ) ) {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
   }

done:
   gpEmailThread = NULL;
}
