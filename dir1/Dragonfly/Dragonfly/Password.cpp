/*************************************************************************
Password.cpp - For Password featre

begun Mar-23-2001 by Mike Rozak
Copyright 2001 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

/* C++ objects */
class CPasswordEntry;
typedef CPasswordEntry * PCPasswordEntry;

// Password
class CPassword {
public:
   CPassword (void);
   ~CPassword (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCPasswordEntry Entry (PWSTR pszName, PWSTR pszUser, PWSTR pszPassword, DWORD dwBefore);
   PCPasswordEntry EntryGetByIndex (DWORD dwIndex);
   PCPasswordEntry EntryGetByID (DWORD dwID);
   DWORD EntryIndexByID (DWORD dwID);
   void Sort (void);

   CListFixed     m_listEntries;   // list of PCPasswordEntries, ordered according order of doing

   BOOL           m_fDirty;      // set to TRUE if the Password is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   DWORD          m_dwNextEntryID;   // next Entry ID created

private:

};

typedef CPassword * PCPassword;


// Entry
class CPasswordEntry {
public:
   CPasswordEntry (void);
   ~CPasswordEntry (void);

   PCPassword      m_pPassword; // Password that this belongs to. If change sets dirty
   CMem           m_memName;  // name of the Entry
   CMem           m_memUser;  // user name
   CMem           m_memPassword; // password
   DWORD          m_dwID;     // unique ID for the Entry.


   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);

private:
};

/* globals */
BOOL           gfPasswordPopulated = FALSE;// set to true if glistPassword is populated
CPassword       gCurPassword;      // current Password that working with

static WCHAR gszEntry[] = L"Entry";
static WCHAR gszPassword[] =L"Password";
static WCHAR gszUser[] = L"User";
static WCHAR gszNextEntryID[] = L"NextEntryID";
static WCHAR gszPasswordListNode[] = L"PasswordListNode";

/*************************************************************************
CPasswordEntry::constructor and destructor
*/
CPasswordEntry::CPasswordEntry (void)
{
   HANGFUNCIN;
   m_pPassword = NULL;
   m_dwID = 0;
   m_memName.CharCat (0);
   m_memUser.CharCat (0);
   m_memPassword.CharCat (0);
}

CPasswordEntry::~CPasswordEntry (void)
{
   HANGFUNCIN;
   // intentionally left blank
}


/*************************************************************************
CPasswordEntry::SetDirty - Sets the dirty flag in the main Password
*/
void CPasswordEntry::SetDirty (void)
{
   if (m_pPassword)
      m_pPassword->m_fDirty = TRUE;
}

/*************************************************************************
CPasswordEntry::Write - Writes the Entry information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CPasswordEntry::Write (PCMMLNode pParent)
{
   HANGFUNCIN;
   PCMMLNode   pSub;

   pSub = pParent->ContentAddNewNode ();
   if (!pSub)
      return FALSE;

   // set the name
   pSub->NameSet (gszEntry);

   // write out parameters
   NodeValueSet (pSub, gszName, (PWSTR) m_memName.p);
   NodeValueSet (pSub, gszUser, (PWSTR) m_memUser.p);
   NodeValueSet (pSub, gszPassword, (PWSTR) m_memPassword.p);
   NodeValueSet (pSub, gszID, (int) m_dwID);

   return TRUE;
}


/*************************************************************************
CPasswordEntry::Read - Writes the EntryEntry information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the Entry data
reutrns
   BOOL - TRUE if successful
*/
BOOL CPasswordEntry::Read (PCMMLNode pSub)
{
   HANGFUNCIN;
   PWSTR psz;

   psz = NodeValueGet (pSub, gszName);
   MemZero (&m_memName);
   if (psz)
      MemCat (&m_memName, psz);

   psz = NodeValueGet (pSub, gszUser);
   MemZero (&m_memUser);
   if (psz)
      MemCat (&m_memUser, psz);

   psz = NodeValueGet (pSub, gszPassword);
   MemZero (&m_memPassword);
   if (psz)
      MemCat (&m_memPassword, psz);

   m_dwID = (DWORD) NodeValueGetInt (pSub, gszID, 1);

   return TRUE;
}

/*************************************************************************
CPassword::Constructor and destructor - Initialize
*/
CPassword::CPassword (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwNextEntryID = 1;

   m_listEntries.Init (sizeof(PCPasswordEntry));
}

CPassword::~CPassword (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new Password is cleared
   Init();
}


/************************************************************************
CPassword::Entry - Adds a mostly default Entry.

inputs
   PWSTR    pszName - name
   PWSTR    pszSubPassword - sub Password
   DWORD    dwDuration - duration (1/100th of a day)
   DWORD    dwBefore - Entry index to insert before. -1 for after
returns
   PCPasswordEntry - To the Entry object, to be added further
*/
PCPasswordEntry CPassword::Entry (PWSTR pszName, PWSTR pszUser, PWSTR pszPassword,
                                 DWORD dwBefore)
{
   HANGFUNCIN;
   PCPasswordEntry pEntry;
   pEntry = new CPasswordEntry;
   if (!pEntry)
      return NULL;

   pEntry->m_dwID = m_dwNextEntryID++;
   m_fDirty = TRUE;
   pEntry->m_pPassword = this;

   MemZero (&pEntry->m_memName);
   MemCat (&pEntry->m_memName, pszName);

   MemZero (&pEntry->m_memUser);
   MemCat (&pEntry->m_memUser, pszUser);

   MemZero (&pEntry->m_memPassword);
   MemCat (&pEntry->m_memPassword, pszPassword);

   // add this to the list
   if (dwBefore < m_listEntries.Num())
      m_listEntries.Insert(dwBefore, &pEntry);
   else
      m_listEntries.Add (&pEntry);

   return pEntry;
}

/*************************************************************************
EntryGetByIndex - Given an index, this returns a Entry, or NULL if can't find.
*/
PCPasswordEntry CPassword::EntryGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCPasswordEntry pEntry, *ppEntry;

   ppEntry = (PCPasswordEntry*) m_listEntries.Get(dwIndex);
   if (!ppEntry)
      return NULL;
   pEntry = *ppEntry;
   return pEntry;
}

/*************************************************************************
EntryGetByID - Given a Entry ID, this returns a Entry, or NULL if can't find.
*/
PCPasswordEntry CPassword::EntryGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCPasswordEntry pEntry;
   DWORD i;
   for (i = 0; i < m_listEntries.Num(); i++) {
      pEntry = EntryGetByIndex (i);
      if (!pEntry)
         continue;

      if (pEntry->m_dwID == dwID)
         return pEntry;
   }

   // else, cant find
   return NULL;
}

/*************************************************************************
EntryIndexByID - Given a Entry ID, this returns a an index, or -1 if can't find.
*/
DWORD CPassword::EntryIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCPasswordEntry pEntry;
   DWORD i;
   if (!dwID)
      return (DWORD) -1;

   for (i = 0; i < m_listEntries.Num(); i++) {
      pEntry = EntryGetByIndex (i);
      if (!pEntry)
         continue;

      if (pEntry->m_dwID == dwID)
         return i;
   }

   // else, cant find
   return (DWORD)-1;
}

/*************************************************************************
CPassword::Write - Writes all the Password data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
Password object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CPassword::Write (void)
{
   HANGFUNCIN;
   // make sure have database name
   if (!m_dwDatabaseNode)
      return FALSE;

   // get the node
   PCMMLNode   pNode;
   pNode = gpData->NodeGet (m_dwDatabaseNode);
   if (!pNode)
      return FALSE;  // cant find node

   if (!NodeValueSet (pNode, gszNextEntryID, m_dwNextEntryID))
      return FALSE;


   // BUGBUG - 2.0 - at some point write out last modified, for search purposes

   // remove existing Entries
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   for (i = dwNum - 1; i < dwNum; i--) {
      PCMMLNode   pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszEntry))
         continue;

      // else, old Entry. delete
      pNode->ContentRemove (i);
   }

   // write out other info, such as Entries, completed and not
   PCPasswordEntry pEntry;
   for (i = 0; i < m_listEntries.Num(); i++) {
      pEntry = *((PCPasswordEntry*) m_listEntries.Get(i));
      pEntry->Write (pNode);
   }

   // remember to release the node before done
   gpData->NodeRelease (pNode);

   // flush the database
   if (!gpData->Flush())
      return FALSE;

   m_fDirty = FALSE;

   return TRUE;
}

/*************************************************************************
CPassword::Flush - If m_fDirty is set it writes out the Password and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CPassword::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CPassword::Init(void) - Initializes a blank Password. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CPassword::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCPasswordEntry pEntry;
   for (i = 0; i < m_listEntries.Num(); i++) {
      pEntry = *((PCPasswordEntry*) m_listEntries.Get(i));
      delete pEntry;
   }
   m_listEntries.Clear();

   // new values for name, description, and days per week
   m_dwNextEntryID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   // sort
   Sort ();

   return TRUE;
}

/*************************************************************************
CPassword::Init(dwDatabaseNode) - Initializes by reading the Password in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a Password, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CPassword::Init (DWORD dwDatabaseNode)
{
   HANGFUNCIN;
   // wipe out whatever have now
   if (!Init())
      return FALSE;

   // load in the database node
   PCMMLNode   pNode;
   pNode = gpData->NodeGet(dwDatabaseNode);
   if (!pNode)
      return FALSE;
   if (_wcsicmp(pNode->NameGet(), gszPasswordListNode)) {
      // invalid name
      gpData->NodeRelease(pNode);
      return FALSE;
   }
   m_dwDatabaseNode = dwDatabaseNode;


   // read in the name, description, and daysperweek
   m_dwNextEntryID = NodeValueGetInt (pNode, gszNextEntryID, 1);

   // read in other Entries
   DWORD i, dwNum;
   PCMMLNode pSub;
   PWSTR psz;
   dwNum = pNode->ContentNum();
   for (i = 0; i < dwNum; i++) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      BOOL  fCompleted;
      if (!_wcsicmp(pSub->NameGet(), gszEntry))
         fCompleted = FALSE;
      else
         continue;   // not looking for this

      PCPasswordEntry pEntry;
      pEntry = new CPasswordEntry;
      pEntry->Read (pSub);
      pEntry->m_pPassword = this;


      // add to list
      m_listEntries.Add (&pEntry);
   }


   // sort
   Sort ();

   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/*******************************************************************************
Sort - Sorts the Password list by date.
*/
int __cdecl PasswordSort(const void *elem1, const void *elem2 )
{
   PCPasswordEntry t1, t2;
   t1 = *((PCPasswordEntry*)elem1);
   t2 = *((PCPasswordEntry*)elem2);

   // return
   return _wcsicmp ((PWSTR) t1->m_memName.p, (PWSTR) t2->m_memName.p);
}

void CPassword::Sort (void)
{
   qsort (m_listEntries.Get(0), m_listEntries.Num(), sizeof(PCPasswordEntry), PasswordSort);
}


/***********************************************************************
PasswordInit - Makes sure the Password object is filled. If not,
it's loaded in. This CAN be called multiple times.

This fills in:
   gfPasswordPopulated - Set to TRUE when it's been loaded
   gCurPassword - fills in

inputs
   none
returns
   BOOL - error
*/
BOOL PasswordInit (void)
{
   HANGFUNCIN;
   if (gfPasswordPopulated)
      return TRUE;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszPasswordListNode, &dwNum);
   if (!pNode)
      return NULL;   // error. This shouldn't happen
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfPasswordPopulated = TRUE;

   // pass this on
   return gCurPassword.Init (dwNum);
}




/*****************************************************************************
PasswordEntryEditPage - Override page callback.
*/
BOOL PasswordEntryEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCPasswordEntry pEntry = (PCPasswordEntry) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set up values
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind(gszName);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pEntry->m_memName.p);
         pControl = pPage->ControlFind(gszUser);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pEntry->m_memUser.p);
         pControl = pPage->ControlFind(gszPassword);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pEntry->m_memPassword.p);


      }
      break;   // do default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[128], szUser[128], szPassword[128];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = szUser[0] = szPassword[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a web site or organization name.");
               return TRUE;
            }
            MemZero (&pEntry->m_memName);
            MemCat (&pEntry->m_memName, szName);

            // set values
            pControl = pPage->ControlFind(gszUser);
            if (pControl)
               pControl->AttribGet (gszText, szUser, sizeof(szUser),&dwNeeded);
            MemZero (&pEntry->m_memUser);
            MemCat (&pEntry->m_memUser, szUser);

            pControl = pPage->ControlFind(gszPassword);
            if (pControl)
               pControl->AttribGet (gszText, szPassword, sizeof(szPassword),&dwNeeded);
            MemZero (&pEntry->m_memPassword);
            MemCat (&pEntry->m_memPassword, szPassword);

            // set dirty
            pEntry->SetDirty();
            gCurPassword.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Entry removed.");

            // find it
            DWORD dwIndex;
            dwIndex = gCurPassword.EntryIndexByID (pEntry->m_dwID);
            gCurPassword.m_listEntries.Remove(dwIndex);

            // delete it
            delete pEntry;
            gCurPassword.m_fDirty = TRUE;
            gCurPassword.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
PasswordAdd - Add a password.

inputs
   PCEscPage pPage - page
returns
   BOOL - TRUE if added
*/
BOOL PasswordAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   // make sure have Password loaded
   PasswordInit ();

   // get the Entry
   CPasswordEntry Entry;

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPASSWORDADD, PasswordEntryEditPage, &Entry);

   if (!pszRet || _wcsicmp(pszRet, gszOK))
      return FALSE;

   // add it
   gCurPassword.Entry ((PWSTR)Entry.m_memName.p, (PWSTR)Entry.m_memUser.p, (PWSTR) Entry.m_memPassword.p, 0);
   gCurPassword.Flush();

   return TRUE;
}


/*****************************************************************************
PasswordEdit - Edit a password.

inputs
   PCEscPage pPage - page
   DWORD       dwNum - number
returns
   BOOL - TRUE if added
*/
BOOL PasswordEdit (PCEscPage pPage, DWORD dwNum)
{
   HANGFUNCIN;
   // make sure have Password loaded
   PasswordInit ();

   // get the Entry
   PCPasswordEntry pEntry;
   pEntry = gCurPassword.EntryGetByID (dwNum);
   if (!pEntry)
      return FALSE;

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPASSWORDEDIT, PasswordEntryEditPage, pEntry);

   if (!pszRet || _wcsicmp(pszRet, gszOK))
      return FALSE;

   return TRUE;
}

/***********************************************************************
PasswordVerifyPage - Page callback for quick-adding a new user
*/
BOOL PasswordVerifyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, gszAdd))
            break;

         // else, we're adding
         PCEscControl pControl;
         WCHAR szName[128];
         DWORD dwNeeded;

         // get the name
         szName[0] = 0;
         pControl = pPage->ControlFind (L"password");
         if (pControl)
            pControl->AttribGet (gszText, szName, sizeof(szName), &dwNeeded);

         if (!wcscmp(szName, gpData->m_szPassword))
            pPage->Exit (gszOK);
         else {
            pPage->MBError (L"The password is wrong.",
               L"The password you typed in doesn't match your Dragonfly password.");
         }

         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
PasswordVerify - Verify the password

inputs
   PCEscPage pPage - page
returns
   BOOL - TRUE if verified
*/
BOOL PasswordVerify (PCEscPage pPage)
{
   HANGFUNCIN;
   // remember the last time checked. If less than two minutes ago then acces
   static __int64 iLast = 0;
   __int64 iNow = 0;
   iNow = DFDATEToMinutes(Today()) + DFTIMEToMinutes(Now());
   if (iLast && (iNow >= iLast) && ((iNow - iLast) <= 3))
      return TRUE;

   // make sure have Password loaded
   PasswordInit ();

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPASSWORDVERIFY, PasswordVerifyPage);

   if (!pszRet || _wcsicmp(pszRet, gszOK))
      return FALSE;

   // store away to verify that checked
   iLast = iNow;

   return TRUE;
}


/*****************************************************************************
PasswordPage - Override page callback.
*/
BOOL PasswordPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PasswordInit();
      }
      break;   // make sure to continue on


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            if (PasswordAdd(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PWWARNING") && !gpData->m_szPassword[0]) {
            p->pszSubString = L"<p><font color=#c00000>"
               L"Warning: You don't currently have a password for your Dragonfly data. "
               L"<bold>Unless you specify a password for Dragonfly anyone with access to your "
               L"computer will be able to see passwords you store on this page.</bold> To "
               L"change your Dragonfly password click <a href=r:241>here</a>."
               L"</font></p>";
            return TRUE;
         }

         // only care about projectlist
         if (_wcsicmp(p->pszSubName, L"PASSWORDLIST"))
            break;

         PasswordInit();

         // if we don't have any Password then simple
         if (!gCurPassword.m_listEntries.Num()) {
            p->pszSubString = L"";
            return TRUE;
         }
         gCurPassword.Sort();

         MemZero (&gMemTemp);
         MemCat (&gMemTemp, L"<xlisttable innerlines=0><xtrheader>Passwords</xtrheader>");

         DWORD i;
         PCPasswordEntry pEntry;
         for (i = 0; i < gCurPassword.m_listEntries.Num(); i++) {
            pEntry = gCurPassword.EntryGetByIndex (i);
            if (!pEntry)
               continue;


            // show it
            MemCat (&gMemTemp, L"<tr>");
            
            MemCat (&gMemTemp, L"<td width=50%%><bold><a href=pe:");
            MemCat (&gMemTemp, (int) pEntry->m_dwID);
            MemCat (&gMemTemp, L">");
            MemCatSanitize (&gMemTemp, (PWSTR) pEntry->m_memName.p);
            MemCat (&gMemTemp, L"<xHoverHelpShort>Click this to edit the password.</xHoverHelpShort>");
            MemCat (&gMemTemp, L"</a></bold></td>");

            MemCat (&gMemTemp, L"<td width=25%%>");
            if (pEntry->m_memUser.p && ((PWSTR)pEntry->m_memUser.p)[0]) {
               MemCat (&gMemTemp, L"<italic><a href=pu:");
               MemCat (&gMemTemp, (int) pEntry->m_dwID);
               MemCat (&gMemTemp, L">User name");
               MemCat (&gMemTemp, L"<xHoverHelp>The user name is not shown for security reasons. Click this copy the user name to the clipboard.</xHoverHelp>");
               MemCat (&gMemTemp, L"</a></italic>");
            }
            MemCat (&gMemTemp, L"</td>");

            MemCat (&gMemTemp, L"<td width=25%%><italic><a href=pp:");
            MemCat (&gMemTemp, (int) pEntry->m_dwID);
            MemCat (&gMemTemp, L">Password");
            MemCat (&gMemTemp, L"<xHoverHelp>The password is not shown for security reasons. Click this to copy the password to the clipboard.</xHoverHelp>");
            MemCat (&gMemTemp, L"</a></italic></td>");

            MemCat (&gMemTemp, L"</tr>");
         }

         MemCat (&gMemTemp, L"</xlisttable>");
         MemCat (&gMemTemp, L"<xbr/>");
         p->pszSubString = (PWSTR) gMemTemp.p;
         return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         // if its a Password link delete and refresh
         if ((p->psz[0] == L'p') && (p->psz[1] == L'e') && (p->psz[2] = L':')) {
            if (!PasswordVerify(pPage))
               return TRUE;

            if (PasswordEdit (pPage, _wtoi(p->psz + 3)))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         // if its a Password link delete and refresh
         else if ((p->psz[0] == L'p') && ((p->psz[1] == L'p') || (p->psz[1] == L'u')) && (p->psz[2] = L':')) {
            DWORD dwNum = _wtoi(p->psz + 3);
            BOOL fPassword = (p->psz[1] == L'p');
            PCPasswordEntry pEntry = gCurPassword.EntryGetByID (dwNum);
            if (!pEntry)
               return TRUE;
            if (!PasswordVerify (pPage))
               return TRUE;

            PWSTR psz;
            psz = fPassword ? (PWSTR) pEntry->m_memPassword.p : (PWSTR) pEntry->m_memUser.p;

            if (!psz || !psz[0]) {
               pPage->MBSpeakWarning (fPassword ?
                  L"The password field is empty." : L"The user name field is empty.");
               return TRUE;
            }

            // convert from unicode to ANSI
            char szTemp[256];
            szTemp[0] = 0;
            WideCharToMultiByte (CP_ACP, 0,
               psz, -1, szTemp, sizeof(szTemp), 0, 0);
            int iLen;
            iLen = strlen(szTemp);

            // fill hmem
            HANDLE   hMem;
            hMem = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, iLen+1);
            if (!hMem)
               return TRUE;
            strcpy ((char*) GlobalLock(hMem), szTemp);
            GlobalUnlock (hMem);

            OpenClipboard (pPage->m_pWindow->m_hWnd);
            EmptyClipboard ();
            SetClipboardData (CF_TEXT, hMem);
            CloseClipboard ();

            // done
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Copied to the clipboard.");
            return TRUE;
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
PasswordSuspendPage - Override page callback.
*/
BOOL PasswordSuspendPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"password");
         if (pControl)
            pControl->AttribSet (gszText, gszPowerPassword);
      }
      break;   // make sure to continue on


   case ESCM_LINK:
      {
         // save password away
         PCEscControl pControl = pPage->ControlFind (L"password");
         if (pControl) {
            DWORD dwNeeded;
            pControl->AttribGet (gszText, gszPowerPassword, sizeof(gszPowerPassword), &dwNeeded);

            // Save the power-down password
            PCMMLNode pNode;
            pNode = FindMajorSection (gszMiscNode);
            if (pNode) {
               NodeValueSet (pNode, gszPP, gszPowerPassword);
               gpData->NodeRelease (pNode);
            }
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
PasswordSuspendVerifyPage - Page callback for quick-adding a new user
*/
BOOL PasswordSuspendVerifyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, gszAdd))
            break;

         // else, we're adding
         PCEscControl pControl;
         WCHAR szName[128];
         DWORD dwNeeded;

         // get the name
         szName[0] = 0;
         pControl = pPage->ControlFind (L"password");
         if (pControl)
            pControl->AttribGet (gszText, szName, sizeof(szName), &dwNeeded);

         if (!wcscmp(szName, gszPowerPassword))
            pPage->Exit (gszOK);
         else {
            pPage->Exit (gszCancel);
         }

         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
PasswordSuspendVerify - Verify the password

inputs
   PCEscWindow pPage - windiow
returns
   BOOL - TRUE if verified
*/
BOOL PasswordSuspendVerify (PCEscWindow pWindow)
{
   HANGFUNCIN;
   // if no password then ignore
   if (!gszPowerPassword[0])
      return TRUE;

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPASSWORDSUSPENDVERIFY, PasswordSuspendVerifyPage);

   if (!pszRet || _wcsicmp(pszRet, gszOK))
      return FALSE;

   return TRUE;
}


