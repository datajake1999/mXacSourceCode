/**********************************************************************
LogOn.cpp - Code for users to log on. This provides the user-interface
and code behind it.

begun 6/3/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

typedef struct {
   WCHAR    szName[64];
   WCHAR    szDir[256];
   WCHAR    szPassword[64];
} NEWUSER, *PNEWUSER;

static CListVariable gListUsers;

// #define HANGDEBUGOLD // BUGBUG - for testing

/********************************************************************************
EnumUsers - Look in the registry and enumerate the number of users that
have been created. Use this to fill in the list object.
*/
void EnumUsers (void)
{
   HANGFUNCIN;
   gListUsers.Clear();

   // loop
   HKEY  hKey;
   char  szaTemp[256];
   WideCharToMultiByte (CP_ACP, 0, gszRegUsers, -1, szaTemp, sizeof(szaTemp), 0, 0);
   hKey = NULL;
   RegOpenKeyEx (HKEY_CURRENT_USER, szaTemp, 0, KEY_READ, &hKey);
   if (!hKey)
      return;  // no users

   // enumerate
   DWORD i, dwSize;
   FILETIME ft;
   for (i = 0; ;i++) {
      dwSize = sizeof(szaTemp);
      if (ERROR_SUCCESS != RegEnumKeyEx (hKey, i, szaTemp, &dwSize, 0, 0, 0, &ft))
         break;

      // else found a key
      WCHAR szTemp[256];
      MultiByteToWideChar (CP_ACP, 0, szaTemp, -1, szTemp, sizeof(szTemp)/2);
      gListUsers.Add (szTemp, (wcslen(szTemp)+1)*2);
   }

   RegCloseKey (hKey);

}


/********************************************************************************
LogOff - Frees data from LogOnInit
*/
void LogOff (void)
{
   HANGFUNCIN;
   if (gpData)
      delete gpData;
   gpData = NULL;
}

/********************************************************************************
LogOnInit - Sets up the globals with the logged on info.

inputs
   PCDatabase     pNew - New database
   PWSTR          pszName - user name
returns 
   none
*/
void LogOnInit (PCDatabase pNew, PWSTR pszName)
{
   HANGFUNCIN;
   gpData = pNew;
   wcscpy (gszUserName, pszName);

   // init the search
   WCHAR szFile[256];
   wcscpy (szFile, pNew->Dir());
   wcscat (szFile, L"\\Search.xsr");
   // initialize the search, use a hash of the date for the version
   char szDate[] = __DATE__;
   DWORD dwSum, i;
   dwSum = 1;
   for (i = 0; i < strlen(szDate); i++)
      dwSum += (i * 0x100 * szDate[i] + szDate[i]);
   gSearch.Init (ghInstance, dwSum, szFile);

}


/********************************************************************************
LoadUser - Loads an existing user. It also verifies that the user password is ok.

inputs
   PWSTR    pszName - User name
   PWSTR    pszDir - Directory to create in.
   PWSTR    pszPassword - Password
   PCEscPage   pPage - Page to report errors from
retursn
   BOOL - TRUE if OK, FALSE if error
*/
BOOL LoadUser (PWSTR pszName, PWSTR pszDir, PWSTR pszPassword,
                    PCEscPage pPage)
{
   HANGFUNCIN;
   // try to create a new user data
   CDatabase   *pNew;
   pNew = new CDatabase;
   if (!pNew) {
      pPage->MBError (gszOutOfMemory);
      return FALSE;
   }
   if (!pNew->InitOpen (pszDir, pszPassword, gszDatabasePrefix, NULL)) {
      pPage->MBError (L"Dragonfly was unable to load the user.",
         L"The directory that Dragonfly thinks the data is in may no "
         L"longer exist.");
      delete pNew;
      return FALSE;
   }

   // verify that can decrypt
   if (!pNew->VerifyPassword ()) {
      pPage->MBError (L"The passwords don't match.",
         L"You may have typed in the wrong password. Please try again.");
      delete pNew;
      return FALSE;
   }

   // just for test
#ifdef _DEBUG
   PCMMLNode   pNode;
   PWSTR psz;
   pNode = pNew->NodeGet (0);
   psz = NodeValueGet (pNode, gszName);
   pNew->NodeRelease(pNode);
#endif


   // call API to fill in all the globals
   LogOnInit (pNew, pszName);

   return TRUE;
}

/********************************************************************************
CreateNewUser - Creates a new user and initializes all the data structures
so that Dragonfly is ready to go.

inputs
   PWSTR    pszName - User name
   PWSTR    pszDir - Directory to create in.
   PWSTR    pszPassword - Password
   PCEscPage   pPage - Page to report errors from
retursn
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CreateNewUser (PWSTR pszName, PWSTR pszDir, PWSTR pszPassword,
                    PCEscPage pPage)
{
   HANGFUNCIN;
   WCHAR szTemp[256];
   char  szaTemp[256];
   HKEY  hKey;
   DWORD dwDisp;

   // basic error checking
   if (!pszName[0]) {
      pPage->MBError (L"You must type in your name.");
      return FALSE;
   }
   if (!pszDir[0]) {
      pPage->MBError (L"You must type in a directory.");
      return FALSE;
   }

   // make sure user doesn't already exist
   wcscpy (szTemp, gszRegUsers);
   wcscat (szTemp, L"\\");
   wcscat (szTemp, pszName);
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
   hKey = NULL;
   RegOpenKeyEx (HKEY_CURRENT_USER, szaTemp, 0, KEY_READ, &hKey);
   if (hKey) {
      RegCloseKey (hKey);
      pPage->MBError (L"The user name already exists.", L"Please choose another one.");
      return FALSE;
   }


   // do a check for free space on disk?
   if (DiskFreeSpace (pszDir) < 10000000) {
      if (IDYES != pPage->MBYesNo (L"The disk you selected doesn't have very much free space. "
         L"Are you sure you want Dragonfly to use it?",
         L"Dragonfly's data files get fairly large over time, so it's good "
         L"to make sure it's writing to a disk with plenty of room."))
         return FALSE;
   }


   // try to create a new user data
   CDatabase   *pNew;
   pNew = new CDatabase;
   if (!pNew) {
      pPage->MBError (gszOutOfMemory);
      return FALSE;
   }
   if (!pNew->InitCreate (pszDir, pszPassword, gszDatabasePrefix, TRUE)) {
      pPage->MBError (L"Dragonfly was unable to create a user.",
         L"Dragonfly user data may already be saved in the directory or "
         L"you may have made a typing mistake in the directory name.");
      return FALSE;
   }

   // create the 0th element
   DWORD dwNode;
   PCMMLNode   pNode;
   pNode = pNew->NodeAdd (gszNodeZero, &dwNode);
   if (!pNode) {
      pPage->MBError (gszWriteError, gszWriteErrorSmall);
      return FALSE;
   }

   // write the name out
   NodeValueSet (pNode, gszName, pszName);

   // release it
   if (!pNew->NodeRelease(pNode)) {
      pPage->MBError (gszWriteError, gszWriteErrorSmall);
      return FALSE;
   }

   // write out registry entry for name
   wcscpy (szTemp, gszRegUsers);
   wcscat (szTemp, L"\\");
   wcscat (szTemp, pszName);
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
   hKey = NULL;
   RegCreateKeyEx (HKEY_CURRENT_USER, szaTemp, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szaTemp, sizeof(szaTemp), 0,0);
      RegSetValueEx (hKey, gszRegUserDir, 0, REG_SZ, (LPBYTE) szaTemp, strlen(szaTemp)+1);
      RegCloseKey (hKey);
   }

   // call API to fill in all the globals
   LogOnInit (pNew, pszName);

   return TRUE;
}

/********************************************************************************
LogOnNewUserPage - Page callback for new user
*/
BOOL LogOnNewUserPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, gszName))
            break;   // only care about the edit control

         WCHAR szName[64];
         DWORD dwNeeded;
         if (!p->pControl->AttribGet(gszText, szName, sizeof(szName), &dwNeeded))
            return FALSE;

         // append this to the current directory
         WCHAR szFull[256];
         MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szFull, sizeof(szFull)/2);
         wcscat (szFull, szName);

         // set the directory name
         PCEscControl   pControl;
         pControl = pPage->ControlFind (gszDirectory);
         if (pControl)
            pControl->AttribSet (gszText, szFull);
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // only care about next
         if (_wcsicmp(p->psz, gszNext))
            break;

         PNEWUSER pnu;
         pnu = (PNEWUSER) pPage->m_pUserData;
         memset (pnu, 0, sizeof(*pnu));

         // verify there's a name
         PCEscControl pControl;
         DWORD dwNeeded;
         pControl = pPage->ControlFind (gszName);
         if (pControl)
            pControl->AttribGet (gszText, pnu->szName, sizeof(pnu->szName), &dwNeeded);

         // verify there's a directory
         pControl = pPage->ControlFind (gszDirectory);
         if (pControl)
            pControl->AttribGet (gszText, pnu->szDir, sizeof(pnu->szDir), &dwNeeded);

         // verify that the passwords are the same
         pControl = pPage->ControlFind (L"password1");
         if (pControl)
            pControl->AttribGet (gszText, pnu->szPassword, sizeof(pnu->szPassword), &dwNeeded);
         WCHAR szp2[64];
         szp2[0] = 0;
         pControl = pPage->ControlFind (L"password2");
         if (pControl)
            pControl->AttribGet (gszText, szp2, sizeof(szp2), &dwNeeded);
         if (wcscmp (pnu->szPassword, szp2)) {
            pPage->MBError (L"Your passwords don't match.",
               L"You may have made a typo in one of them. Please retype both of them.");
            return TRUE;
         }


         // try to create
         if (!CreateNewUser (pnu->szName, pnu->szDir, pnu->szPassword, pPage))
            return TRUE;   // go bck

         // else all went through. default handler
         break;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}

/********************************************************************************
LogOnNewUser - Create a new user. Specify name, directory, and password.
   Expalin danger with password. After create new user, log on directly. DOn't
   ask for user.

inputs
   none
returns
   int - 0 for quit, -1 for back, 1 for all done
*/
int LogOnNewUser (void)
{
   HANGFUNCIN;
   PWSTR psz;
   NEWUSER  nu;

   psz = gpWindow->PageDialog (ghInstance, IDR_MMLLOGONNEWUSER, LogOnNewUserPage, &nu);
   if (!_wcsicmp(psz, gszBack))
      return -1;
   else if (_wcsicmp(psz, gszNext))
      return 0;

   // else, next. New user is already created and loaded
   return 1;
}

static PWSTR gpszAutoLogon = L"AutoLogon";


/********************************************************************************
AskUserPage - Page callback to ask who the user is
*/
BOOL AskUserPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   static DWORD gdwTimer = 0;
   static BOOL fFirstTime = TRUE;


   switch (dwMessage) {

   case ESCM_INITPAGE:
      {

         // fill in the combo box
         PCEscControl   pControl;
         pControl = pPage->ControlFind (gszUser);
         if (pControl) {
            ESCMCOMBOBOXADD cb;
            memset (&cb, 0, sizeof(cb));
            cb.dwInsertBefore = (DWORD)-1;
            
            DWORD i;
            for (i = 0; i < gListUsers.Num(); i++) {
               cb.pszText = (PWSTR) gListUsers.Get(i);
               pControl->Message (ESCM_COMBOBOXADD, &cb);
            }

            pControl->AttribSetInt (gszCurSel, (int) gdwLastLogin);
         }

         pControl = pPage->ControlFind (gpszAutoLogon);
         if (pControl && gfAutoLogon)
            pControl->AttribSetBOOL (gszChecked, TRUE);

#ifdef HANGDEBUGOLD
         MessageBox (pPage->m_pWindow->m_hWnd, "a", NULL, MB_OK);
#endif
         gdwTimer = 0;
         if (gfAutoLogon) {
#ifdef HANGDEBUGOLD
            MessageBox (pPage->m_pWindow->m_hWnd, "b", NULL, MB_OK);
#endif
            gdwTimer = pPage->m_pWindow->TimerSet(5000, pPage);
         }

#ifdef HANGDEBUGOLD
         MessageBox (pPage->m_pWindow->m_hWnd, "c", NULL, MB_OK);
#endif

         // speak the first time
         // play a chime
#define  NOTEBASE       62
#define  VOLUME         64
         if (fFirstTime) {

#ifdef HANGDEBUGOLD
         MessageBox (pPage->m_pWindow->m_hWnd, "d", NULL, MB_OK);
#endif
            // load tts in befor chime
            // BUGFIX - No TTS EscSpeakTTS ();

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
            // BUGBUG - Temporarily disable chime... EscChime (aChime, sizeof(aChime) / sizeof(ESCMIDIEVENT));

            // speak
            // BUGFIX - No TTS - EscSpeak (L"Welcome to Dragonfly.");
            fFirstTime = FALSE;

         }
#ifdef HANGDEBUGOLD
         MessageBox (pPage->m_pWindow->m_hWnd, "e", NULL, MB_OK);
#endif


      }
      break;

   case ESCM_TIMER:
      {
         PESCMTIMER p = (PESCMTIMER) pParam;
#ifdef HANGDEBUGOLD
         MessageBox (pPage->m_pWindow->m_hWnd, "T", NULL, MB_OK);
#endif

         if (gdwTimer && (p->dwID == gdwTimer)) {
#ifdef HANGDEBUGOLD
         MessageBox (pPage->m_pWindow->m_hWnd, "U", NULL, MB_OK);
#endif
            pPage->m_pWindow->TimerKill (gdwTimer);
            gdwTimer = 0;
            pPage->Link (gszNext);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (p->pControl && p->pControl->m_pszName && !_wcsicmp(p->pControl->m_pszName, L"AutoLogOn")) {
            gfAutoLogon = p->pControl->AttribGetBOOL (gszChecked);
            KeySet (gszAutoLogon, gfAutoLogon);
         }

         if (gdwTimer)
            pPage->m_pWindow->TimerKill (gdwTimer);
         gdwTimer = 0;
      }
      break;
#if 0
   case ESCN_FILTEREDLISTQUERY:
      {
         // this is a message sent by the filtered list controls
         // used as samples in help.

         PESCNFILTEREDLISTQUERY p = (PESCNFILTEREDLISTQUERY) pParam;

         // only repsond to the people list
         if (!p->pszListName || _wcsicmp(p->pszListName, L"users"))
            break;

         p->pList = &gListUsers;
      }
      return TRUE;
#endif // 0

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // kill the timer
         if (gdwTimer)
            pPage->m_pWindow->TimerKill (gdwTimer);
         gdwTimer = 0;

         // only care about next
         if (_wcsicmp(p->psz, gszNext))
            break;

         // get the name and password
         WCHAR szPassword[64];
         WCHAR *pszUser;
         DWORD dwNeeded;
         szPassword[0] = 0;
         pszUser = NULL;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"password");
         if (pControl)
            pControl->AttribGet (gszText, szPassword, sizeof(szPassword), &dwNeeded);
         pControl = pPage->ControlFind (gszUser);
         int   iRet;
         if (pControl) {
            iRet = pControl->AttribGetInt (gszCurSel);
            pszUser = (PWSTR) gListUsers.Get((DWORD)iRet);
         }

         if (!pszUser) {
            pPage->MBError (L"You must select an account to log in as.");
            return TRUE;
         }

         // determine the directory
         WCHAR szTemp[256];
         char  szaTemp[256];
         HKEY hKey;
         wcscpy (szTemp, gszRegUsers);
         wcscat (szTemp, L"\\");
         wcscat (szTemp, pszUser);
         WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
         hKey = NULL;
         RegOpenKeyEx (HKEY_CURRENT_USER, szaTemp, 0, KEY_READ, &hKey);
         if (!hKey)
            return TRUE;   // some sort of error somewhere
         szTemp[0] = 0;
         DWORD dwType;
         if (hKey) {
            char  szaTemp2[256];
            DWORD dwSize;
            dwSize = sizeof(szaTemp2);
            szaTemp2[0] = 0;
            RegQueryValueEx (hKey, gszRegUserDir, 0, &dwType, (LPBYTE) szaTemp2, &dwSize);
            RegCloseKey (hKey);

            MultiByteToWideChar (CP_ACP, 0, szaTemp2, -1, szTemp, sizeof(szTemp)/2);
         }

         // go to load
         if (!LoadUser (pszUser, szTemp, szPassword, pPage))
            return TRUE;   // some sort of error

         // save the selection
         gdwLastLogin = (DWORD) iRet;
         KeySet (gszLastLogin, gdwLastLogin);

         // else all went through. default handler
         break;
      }
      break;

      case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         //if (!_wcsicmp(p->pszSubName, L"Template1")) {
         //   p->pszSubString = gfPrinting ? L"" :
         //      ((!gfTemplateR) ? L"<?Template resource=202?>" : L"<?Template resource=301?>");
         //   return TRUE;
         //}
         //else
         if (!_wcsicmp(p->pszSubName, L"Template2")) {
            p->pszSubString = gfPrinting ? L"" : L"<?Template resource=203?>";
            return TRUE;
         }
         //else if (!_wcsicmp(p->pszSubName, L"Template3")) {
         //   p->pszSubString = gfPrinting ? L"" : L"<?Template resource=204?>";
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pszSubName, L"FAVORITEMENU")) {
         //   p->pszSubString = FavoritesMenuSub ();
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pszSubName, L"FAVORITEMENU2")) {
         //   p->pszSubString = FavoritesMenuSub (TRUE);
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pszSubName, L"IFCOLOR")) {
            p->pszSubString = gfFullColor ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFCOLOR")) {
            p->pszSubString = gfFullColor ? L"" : L"</comment>";
            return TRUE;
         }
         //else if (!_wcsicmp(p->pszSubName, L"IFPCACTIVE")) {
            // BUGFIX - Special PCACTIVE edition
#ifdef PCACTIVE
         //   p->pszSubString = L"";
#else
         //   p->pszSubString = L"<comment>";
#endif
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pszSubName, L"ENDIFPCACTIVE")) {
            // BUGFIX - Special PCACTIVE edition
#ifdef PCACTIVE
         //   p->pszSubString = L"";
#else
         //   p->pszSubString = L"</comment>";
#endif
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pszSubName, L"IFNOTPRINT")) {
         //   p->pszSubString = (!gfPrinting) ? L"" : L"<comment>";
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pszSubName, L"ENDIFNOTPRINT")) {
         //   p->pszSubString = (!gfPrinting) ? L"" : L"</comment>";
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pszSubName, L"IFMICROHELP")) {
            p->pszSubString = gfMicroHelp ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFMICROHELP")) {
            p->pszSubString = gfMicroHelp ? L"" : L"</comment>";
            return TRUE;
         }
         //else if (!_wcsicmp(p->pszSubName, L"WHITECOLOR")) {
         //   p->pszSubString = gfFullColor ? L"color=#ffffff" : L"";
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pszSubName, L"JOURNALTIMER")) {
         //   p->pszSubString = JournalTimerSubst();
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pszSubName, L"PROJECTTIMER")) {
         //   p->pszSubString = ProjectTimerSubst();
         //   return TRUE;
         //}
      }
      return FALSE;
};

   return FALSE; // BUGFIX - Dont go to DefPage (pPage, dwMessage, pParam);
}

/********************************************************************************
FromBackupPage - Page callback to ask who the user is
*/
BOOL FromBackupPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // only care about next
         if (_wcsicmp(p->psz, gszNext))
            break;

         // verify that it's a valid database file
         WCHAR szFile[256];
         DWORD dwNeeded;
         PCEscControl pControl;
         szFile[0] = 0;
         pControl = pPage->ControlFind (L"dir");
         if (pControl)
            pControl->AttribGet (gszText, szFile, sizeof(szFile), &dwNeeded);

#define TESTINITOPENFAIL   // BUGBUG
#ifdef TESTINITOPENFAIL
         int iError;
#endif

         CDatabase db;
         if (!db.InitOpen (szFile, L"", gszDatabasePrefix, &iError)) {
            pPage->MBError (L"The directory is not a Dragonfly directory.",
               L"Please retype in the directory name. The directory should contain "
               L"the file Dragonfly.dfd and a number of subdirectories.");

#ifdef TESTINITOPENFAIL
            WCHAR szTemp[256];
            swprintf (szTemp, L"Error number = %d (%s)", (int)iError,
               _wcserror (iError) );
            pPage->MBError (szFile, szTemp);
#endif

            return TRUE;
         }

         // make sure it's not already in the list of users
         WCHAR szName[256];
         szName[0] = 0;
         pControl = pPage->ControlFind (L"login");
         if (pControl)
            pControl->AttribGet (gszText, szName, sizeof(szName), &dwNeeded);
         if (!szName[0]) {
            pPage->MBError (L"Please type in a name.");
            return TRUE;
         }
         PWSTR psz;
         DWORD i;
         for (i = 0; i < gListUsers.Num(); i++) {
            psz = (PWSTR) gListUsers.Get(i);
            if (!_wcsicmp(psz, szName)) {
               pPage->MBError (L"Please type in a different name.",
                  L"You're already using that name as a log-on. Either type in a new "
                  L"name, or delete the other name from the login list.");
               return TRUE;
            }
         }

         // add it
         char  szaTemp[256];
         WCHAR szTemp[256];
         HKEY hKey;
         wcscpy (szTemp, gszRegUsers);
         wcscat (szTemp, L"\\");
         wcscat (szTemp, szName);
         WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
         hKey = NULL;
         DWORD dwDisp;
         RegCreateKeyEx (HKEY_CURRENT_USER, szaTemp, 0, 0, REG_OPTION_NON_VOLATILE,
            KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
         if (hKey) {
            WideCharToMultiByte (CP_ACP, 0, szFile, -1, szaTemp, sizeof(szaTemp), 0,0);
            RegSetValueEx (hKey, gszRegUserDir, 0, REG_SZ, (LPBYTE) szaTemp, strlen(szaTemp)+1);
            RegCloseKey (hKey);
         }

         // add to the list
         gListUsers.Add (szName, (wcslen(szName)+1)*2);

         // go back
         pPage->Exit (gszBack);
         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}

/********************************************************************************
DeleteUserPage - Page callback to ask who the user is
*/
BOOL DeleteUserPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {


   case ESCM_INITPAGE:
      {
         // fill in the combo box
         PCEscControl   pControl;
         pControl = pPage->ControlFind (gszUser);
         if (pControl) {
            ESCMCOMBOBOXADD cb;
            memset (&cb, 0, sizeof(cb));
            cb.dwInsertBefore = (DWORD)-1;
            
            DWORD i;
            for (i = 0; i < gListUsers.Num(); i++) {
               cb.pszText = (PWSTR) gListUsers.Get(i);
               pControl->Message (ESCM_COMBOBOXADD, &cb);
            }

            pControl->AttribSetInt (gszCurSel, 0);
         }

      }
      break;
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // only care about next
         if (_wcsicmp(p->psz, gszNext))
            break;

         // get the name
         PCEscControl pControl;
         pControl = pPage->ControlFind (gszUser);
         PWSTR pszUser;
         pszUser = NULL;
         int iUser;
         if (pControl) {
            iUser = pControl->AttribGetInt (gszCurSel);
            pszUser = (PWSTR) gListUsers.Get((DWORD)iUser);
         }
         if (!pszUser) {
            pPage->MBWarning (L"There aren't any accounts to delete.");
            return TRUE;
         }


         // Verify delete
         WCHAR szTemp[512], szTemp2[1024];
         swprintf (szTemp, L"Are you sure you want to delete the account \"%s\"?", pszUser);
         if (IDYES != pPage->MBYesNo (szTemp))
            return TRUE;

         // find the old directory
         WCHAR szDir[256];
         char szaDir[256];
         char  szaTemp[256];
         char  szName[256];
         WideCharToMultiByte (CP_ACP, 0, pszUser, -1, szName, sizeof(szName), 0, 0);
         HKEY hKey;
         wcscpy (szTemp, gszRegUsers);
         wcscat (szTemp, L"\\");
         wcscat (szTemp, pszUser);
         WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
         hKey = NULL;
         DWORD dwDisp;
         RegCreateKeyEx (HKEY_CURRENT_USER, szaTemp, 0, 0, REG_OPTION_NON_VOLATILE,
            KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
         szaDir[0] = 0;
         if (hKey) {
            DWORD dwSize,dwType;
            dwSize = sizeof(szaDir);
            RegQueryValueEx (hKey, gszRegUserDir, 0, &dwType, (LPBYTE) szaDir, &dwSize);
            RegDeleteValue (hKey, gszRegUserDir);
            RegCloseKey (hKey);
         }
         MultiByteToWideChar (CP_ACP, 0, szaDir, -1, szDir, sizeof(szDir)/2);

         // delete
         RegDeleteKey (HKEY_CURRENT_USER, szaTemp);

         // inform that deleted
         swprintf (szTemp, L"%s is removed from the login list, but...", pszUser);
         gListUsers.Remove (iUser);
         swprintf (szTemp2,
            L"Because large amounts of data are at stake, Dragonfly will not delete the user data. "
            L"If you want to delete the data, then delete the \"%s\" directory. Dragonfly "
            L"will pull up the directory for you.", szDir);
         pPage->MBInformation (szTemp,szTemp2);

         // pull up the direcotry
         ShellExecute (pPage->m_pWindow->m_hWnd, NULL, szaDir, NULL, NULL, SW_SHOWNORMAL);

         // go back
         pPage->Exit (gszBack);
         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}

/********************************************************************************
LogOnAskUser - The main log-on screen, which asks the user which user they
wish to log on as, or if they wish to create a new user.

returns
   BOOL - TRUE if ok and a user is loaded, FALSE if should quit the app
*/
BOOL LogOnAskUser (void)
{
   HANGFUNCIN;
   // no data, so go to LongOnFirstTime
   PWSTR psz;
again:
   psz = gpWindow->PageDialog (ghInstance, IDR_MMLLOGONASKUSER, AskUserPage, NULL);

#ifdef HANGDEBUGOLD
   MessageBoxW (NULL, L"x", psz ? psz : L"Empty", MB_OK);
#endif

   if (!_wcsicmp(psz, gszNext))
      return TRUE;   // it wouldn get past here without succeding

   // if add user?
   if (!_wcsicmp(psz, L"newuser")) {
      // new user
      int   iRet;
      iRet = LogOnNewUser ();
      if (iRet < 0)
         goto again;
      return (iRet ? TRUE : FALSE);
   }
   else if (!_wcsicmp (psz, L"frombackup")) {
      psz = gpWindow->PageDialog (ghInstance, IDR_MMLLOGONFROMBACKUP, FromBackupPage);
      if (!psz || psz[0] == L'[')
         return FALSE;
      goto again;
   }
   else if (!_wcsicmp (psz, L"delete")) {
      psz = gpWindow->PageDialog (ghInstance, IDR_MMLLOGONDELETE, DeleteUserPage);
      if (!psz || psz[0] == L'[')
         return FALSE;
      goto again;
   }

   // else
   return FALSE;

}


/********************************************************************************
LogOnMain -If there are users then go to LogOnUser, else LogOnFirstTime.

inputs
   none
returns
   BOOL - TRUE if succede. FALSE if fail and should exit
*/
BOOL LogOnMain (void)
{
   HANGFUNCIN;
   // should check to see if other user data is saved
   EnumUsers();

   // if there are any users do LogOnAskUser
   if (gListUsers.Num())
      return LogOnAskUser();

   // no data, so go to LongOnFirstTime
   PWSTR psz;
again:
   psz = gpWindow->PageDialog (ghInstance, IDR_MMLLOGONFIRSTTIME, DefPage, NULL);
   if (_wcsicmp(psz, gszNext))
      return FALSE;

   // new user
   int   iRet;
   iRet = LogOnNewUser ();
   if (iRet < 0)
      goto again;
   return (iRet ? TRUE : FALSE);
}




// BUGBUG - 2.0 - API/UI to get info about how large database is

// BUGBUG - 2.0 - API/UI to rename user

// BUGBUG - 2.0 - API/UI to change directory

// BUGBUG - 2.0 - API/UI to print all user data to printer (in very small type)?
// Maybe an option to print only that which has changed since the last
// time printed out.


