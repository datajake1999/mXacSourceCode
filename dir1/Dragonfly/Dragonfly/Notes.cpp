/*************************************************************************
Notes.cpp - For Notes featre

begun 8/9/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

/* C++ objects */
class CNoteTask;
typedef CNoteTask * PCNoteTask;

// Note
class CNote {
public:
   CNote (void);
   ~CNote (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCNoteTask TaskAdd (PWSTR pszName, PWSTR pszCategory, DWORD dwBefore);
   PCNoteTask TaskGetByIndex (DWORD dwIndex);
   PCNoteTask TaskGetByID (DWORD dwID);
   DWORD TaskIndexByID (DWORD dwID);
   void Sort (void);

   CListFixed     m_listTasks;   // list of PCNoteTasks, ordered according order of doing

   BOOL           m_fDirty;      // set to TRUE if the Note is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   DWORD          m_dwNextTaskID;   // next task ID created

   CListVariable  m_listCategories;  // list of categories

private:

};

typedef CNote * PCNote;


// task
class CNoteTask {
public:
   CNoteTask (void);
   ~CNoteTask (void);

   PCNote      m_pNote; // Note that this belongs to. If change sets dirty
   CMem           m_memName;  // name of the task
   CMem           m_memCategory; // category for the note. Might be empty string
   DWORD          m_dwID;     // unique ID for the task.


   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);

private:
};

/* globals */
BOOL           gfNotePopulated = FALSE;// set to true if glistNote is populated
CNote       gCurNote;      // current Note that working with

static WCHAR gszDate[] = L"date";
static WCHAR gszNoteDate[] = L"Notedate";
static WCHAR gszCategory[] = L"Category";

/*************************************************************************
CNoteTask::constructor and destructor
*/
CNoteTask::CNoteTask (void)
{
   HANGFUNCIN;
   m_pNote = NULL;
   m_dwID = 0;
   m_memName.CharCat (0);
   m_memCategory.CharCat(0);
}

CNoteTask::~CNoteTask (void)
{
   HANGFUNCIN;
   // intentionally left blank
}


/*************************************************************************
CNoteTask::SetDirty - Sets the dirty flag in the main Note
*/
void CNoteTask::SetDirty (void)
{
   HANGFUNCIN;
   if (m_pNote)
      m_pNote->m_fDirty = TRUE;
}

/*************************************************************************
CNoteTask::Write - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CNoteTask::Write (PCMMLNode pParent)
{
   HANGFUNCIN;
   PCMMLNode   pSub;

   pSub = pParent->ContentAddNewNode ();
   if (!pSub)
      return FALSE;

   // set the name
   pSub->NameSet (gszTask);

   // write out parameters
   NodeValueSet (pSub, gszName, (PWSTR) m_memName.p);
   NodeValueSet (pSub, gszCategory, (PWSTR) m_memCategory.p);
   NodeValueSet (pSub, gszID, (int) m_dwID);

   return TRUE;
}


/*************************************************************************
CNoteTask::Read - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the task data
reutrns
   BOOL - TRUE if successful
*/
BOOL CNoteTask::Read (PCMMLNode pSub)
{
   HANGFUNCIN;
   PWSTR psz;

   psz = NodeValueGet (pSub, gszName);
   MemZero (&m_memName);
   if (psz)
      MemCat (&m_memName, psz);

   psz = NodeValueGet (pSub, gszCategory);
   MemZero (&m_memCategory);
   if (psz)
      MemCat (&m_memCategory, psz);

   m_dwID = (DWORD) NodeValueGetInt (pSub, gszID, 1);

   return TRUE;
}

/*************************************************************************
CNote::Constructor and destructor - Initialize
*/
CNote::CNote (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwNextTaskID = 1;

   m_listTasks.Init (sizeof(PCNoteTask));
}

CNote::~CNote (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new Note is cleared
   Init();
}


/************************************************************************
CNote::TaskAdd - Adds a mostly default task.

inputs
   PWSTR    pszName - name
   PWSTR    pszSubNote - sub Note
   DWORD    dwDuration - duration (1/100th of a day)
   DWORD    dwBefore - Task index to insert before. -1 for after
returns
   PCNoteTask - To the task object, to be added further
*/
PCNoteTask CNote::TaskAdd (PWSTR pszName, PWSTR pszCategory, DWORD dwBefore)
{
   HANGFUNCIN;
   PCNoteTask pTask;
   pTask = new CNoteTask;
   if (!pTask)
      return NULL;

   pTask->m_dwID = m_dwNextTaskID++;
   m_fDirty = TRUE;
   pTask->m_pNote = this;

   MemZero (&pTask->m_memName);
   MemCat (&pTask->m_memName, pszName);

   MemZero (&pTask->m_memCategory);
   MemCat (&pTask->m_memCategory, pszCategory);

   // add this to the list
   if (dwBefore < m_listTasks.Num())
      m_listTasks.Insert(dwBefore, &pTask);
   else
      m_listTasks.Add (&pTask);

   return pTask;
}

/*************************************************************************
TaskGetByIndex - Given an index, this returns a task, or NULL if can't find.
*/
PCNoteTask CNote::TaskGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCNoteTask pTask, *ppTask;

   ppTask = (PCNoteTask*) m_listTasks.Get(dwIndex);
   if (!ppTask)
      return NULL;
   pTask = *ppTask;
   return pTask;
}

/*************************************************************************
TaskGetByID - Given a task ID, this returns a task, or NULL if can't find.
*/
PCNoteTask CNote::TaskGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCNoteTask pTask;
   DWORD i;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = TaskGetByIndex (i);
      if (!pTask)
         continue;

      if (pTask->m_dwID == dwID)
         return pTask;
   }

   // else, cant find
   return NULL;
}

/*************************************************************************
TaskIndexByID - Given a task ID, this returns a an index, or -1 if can't find.
*/
DWORD CNote::TaskIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCNoteTask pTask;
   DWORD i;
   if (!dwID)
      return (DWORD) -1;

   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = TaskGetByIndex (i);
      if (!pTask)
         continue;

      if (pTask->m_dwID == dwID)
         return i;
   }

   // else, cant find
   return (DWORD)-1;
}


/*******************************************************************************
Sort - Sorts the notes list by category.
*/
static int __cdecl CategorySort(const void *elem1, const void *elem2 )
{
   PCNoteTask t1, t2;
   t1 = *((PCNoteTask*)elem1);
   t2 = *((PCNoteTask*)elem2);

   int   iRet;
   iRet = _wcsicmp((PWSTR) t1->m_memCategory.p, (PWSTR) t2->m_memCategory.p);
   if (iRet)
      return iRet;

   // else sort by order
   return (int) t1->m_dwID - (int)t2->m_dwID;
}

void CNote::Sort (void)
{
   qsort (m_listTasks.Get(0), m_listTasks.Num(), sizeof(PCNoteTask), CategorySort);
}


/*************************************************************************
CNote::Write - Writes all the Note data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
Note object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CNote::Write (void)
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

   if (!NodeValueSet (pNode, gszNextTaskID, m_dwNextTaskID))
      return FALSE;


   // remove existing tasks
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   for (i = dwNum - 1; i < dwNum; i--) {
      PCMMLNode   pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszTask) && _wcsicmp(pSub->NameGet(), gszCategory))
         continue;

      // else, old task. delete
      pNode->ContentRemove (i);
   }

   // write out other info, such as tasks, completed and not
   PCNoteTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCNoteTask*) m_listTasks.Get(i));
      pTask->Write (pNode);
   }

   // write out the categories
   for (i = 0; i < m_listCategories.Num(); i++) {
      NodeElemSet (pNode, gszCategory, (PWSTR) m_listCategories.Get(i), 0, FALSE);
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
CNote::Flush - If m_fDirty is set it writes out the Note and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CNote::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CNote::Init(void) - Initializes a blank Note. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CNote::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCNoteTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCNoteTask*) m_listTasks.Get(i));
      delete pTask;
   }
   m_listTasks.Clear();

   // read in the cateogries
   m_listCategories.Clear();

   // new values for name, description, and days per week
   m_dwNextTaskID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   return TRUE;
}

/*************************************************************************
CNote::Init(dwDatabaseNode) - Initializes by reading the Note in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a Note, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CNote::Init (DWORD dwDatabaseNode)
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
   if (_wcsicmp(pNode->NameGet(), gszNoteListNode)) {
      // invalid name
      gpData->NodeRelease(pNode);
      return FALSE;
   }
   m_dwDatabaseNode = dwDatabaseNode;


   // read in the name, description, and daysperweek
   m_dwNextTaskID = NodeValueGetInt (pNode, gszNextTaskID, 1);

   // read in other tasks
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
      if (!_wcsicmp(pSub->NameGet(), gszTask))
         fCompleted = FALSE;
      else
         continue;   // not looking for this

      PCNoteTask pTask;
      pTask = new CNoteTask;
      pTask->Read (pSub);
      pTask->m_pNote = this;


      // add to list
      m_listTasks.Add (&pTask);
   }


   
   // read in the cateogries
   m_listCategories.Clear();
   DWORD dwIndex;
   dwIndex = 0;
   while (TRUE) {
      psz = NodeElemGet (pNode, gszCategory, &dwIndex);
      if (!psz)
         break;
      m_listCategories.Add (psz, (wcslen(psz)+1)*2);
   }

   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/***********************************************************************
NoteInit - Makes sure the Note object is filled. If not,
it's loaded in. This CAN be called multiple times.

This fills in:
   gfNotePopulated - Set to TRUE when it's been loaded
   gCurNote - fills in

inputs
   none
returns
   BOOL - error
*/
BOOL NoteInit (void)
{
   HANGFUNCIN;
   if (gfNotePopulated)
      return TRUE;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszNoteListNode, &dwNum);
   if (!pNode)
      return NULL;   // error. This shouldn't happen
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfNotePopulated = TRUE;

   // pass this on
   return gCurNote.Init (dwNum);
}

/***********************************************************************
NotesFilteredList - Returns a filtered list for the notes categorues.
*/
PCListVariable NotesFilteredList (void)
{
   HANGFUNCIN;
   NoteInit();
   return &gCurNote.m_listCategories;
}
/*****************************************************************************
NotesQuickAddPage - Override page callback.
*/
BOOL NotesQuickAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (_wcsicmp(p->psz, gszOK))
            break;   // default behaviour

         // get the text
         WCHAR szTemp[256];
         szTemp[0] = L'!'; // indicator that have a name
         szTemp[1] = 0;

         PCEscControl pControl;
         DWORD dwNeeded;
         pControl = pPage->ControlFind(gszName);
         if (pControl)
            pControl->AttribGet (gszText, szTemp+1, sizeof(szTemp)-2, &dwNeeded);
         if (!szTemp[1]) {
            pPage->MBWarning (L"You must type in a category name or press Cancel.");
            return TRUE;
         }

         // got it
         pPage->Exit (szTemp);
         return TRUE;   // suck up the other link
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
NotesQuickAdd - Adds a subproject to the list.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   DWORD - New subproject index. -1 if didn't add.
*/
DWORD NotesQuickAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   NoteInit();

   CEscWindow  cWindow;
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLNOTESADDCATEGORY, NotesQuickAddPage);

   if (!pszRet || (pszRet[0] != L'!'))
      return (DWORD) -1;   // canceled

   gCurNote.m_fDirty = TRUE;
   DWORD dwRet;
   dwRet =  gCurNote.m_listCategories.Add(pszRet+1, (wcslen(pszRet+1)+1)*2);
   gCurNote.Flush();

   return dwRet;
}


/*****************************************************************************
NoteParseLink - If a page uses a Notes substitution, which generates
"rn:XXXX" to indicate that a Note link was called, call this. If it's
not a Note link, returns FALSE. If it is, it deletes the Note and
returns TRUE. The page should then refresh itself.

inputs
   PWSTR    pszLink - link
   PCEscPage pPage - page
returns
   BOOL
*/
BOOL NoteParseLink (PWSTR pszLink, PCEscPage pPage)
{
   HANGFUNCIN;
   // make sure its the right type
   if ((pszLink[0] != L'r') || (pszLink[1] != L'n') || (pszLink[2] != L':'))
      return FALSE;

   // make sure have Notes loaded
   NoteInit ();

   // get the number
   DWORD dwIndex, dwID;
   dwID = 0;
   AttribToDecimal (pszLink + 3, (int*) &dwID);
   dwIndex = gCurNote.TaskIndexByID (dwID);
   if (dwIndex == (DWORD)-1)
      return TRUE;   // error

   PCNoteTask pTask;
   pTask = *((PCNoteTask*) gCurNote.m_listTasks.Get(dwIndex));

   // delete this
   delete pTask;
   gCurNote.m_listTasks.Remove (dwIndex);
   gCurNote.m_fDirty = TRUE;
   gCurNote.Flush();

   // tell user
   EscChime (ESCCHIME_INFORMATION);
   EscSpeak (L"Note deleted.");

   // done
   return TRUE;
}

/*****************************************************************************
NotesEditPage - Override page callback.
*/
BOOL NotesEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCNoteTask pTask = (PCNoteTask) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set up values
         PCEscControl pControl;
         NoteInit ();

         // name
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pTask->m_memName.p);

         // subject
         DWORD i;
         for (i = 0; i < gCurNote.m_listCategories.Num(); i++) {
            PWSTR psz  =(PWSTR) gCurNote.m_listCategories.Get(i);
            if (!psz || !pTask->m_memCategory.p)
               continue;
            if (!_wcsicmp(psz, (PWSTR)pTask->m_memCategory.p))
               break;
         }
         pControl = pPage->ControlFind(gszCategory);
         if (pControl && (i < gCurNote.m_listCategories.Num()))
            pControl->AttribSetInt (gszCurSel, (int) i);


      }
      break;   // do default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // get the text
            WCHAR szHuge[1024];
            PCEscControl   pControl;
            DWORD dwNeeded;
            szHuge[0] = 0;
            pControl = pPage->ControlFind (gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBWarning (L"You must type in some text for the Note.");
               return TRUE;
            }

            // get the cateogyr
            // BUGFIX - Category for notes
            pControl = pPage->ControlFind (gszCategory);
            PWSTR pszCat;
            int   iIndex;
            iIndex = -1;
            if (pControl)
               iIndex = pControl->AttribGetInt (gszCurSel);
            pszCat = (PWSTR) gCurNote.m_listCategories.Get((DWORD)iIndex);

            // write new values
            MemZero (&pTask->m_memCategory);
            MemZero (&pTask->m_memName);
            if (pszCat)
               MemCat (&pTask->m_memCategory, pszCat);
            MemCat (&pTask->m_memName, szHuge);

            // set dirty
            pTask->SetDirty();
            gCurNote.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
NotesParseButton - If a page uses a work substitution, which generates
"tt:XXXX" to indicate that a work link was called, call this. If it's
not a work link, returns FALSE. If it is, it deletes the work and
returns TRUE. The page should then refresh itself.

inputs
   PWSTR    pszLink - link
   PCEscPage pPage - page
   BOOL     *pfRefresh - set to TRUE if should refresh
returns
   BOOL
*/
BOOL NotesParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh)
{
   HANGFUNCIN;
   // make sure its the right type
   if ((pszLink[0] != L't') || (pszLink[1] != L't') || (pszLink[2] != L':'))
      return FALSE;

   // make sure have work loaded
   NoteInit ();

   // get the task
   PCNoteTask pTask;
   DWORD dwID;
   dwID = 0;
   AttribToDecimal (pszLink + 3, (int*) &dwID);
   pTask = gCurNote.TaskGetByID (dwID);

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLNOTESEDIT, NotesEditPage, pTask);

   *pfRefresh = (pszRet && !_wcsicmp(pszRet, gszOK));
   return TRUE;
}



/*****************************************************************************
NotesPage - Override page callback.
*/
BOOL NotesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         NoteInit();
      }
      break;   // make sure to continue on


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // get the text
            WCHAR szHuge[1024];
            PCEscControl   pControl;
            DWORD dwNeeded;
            szHuge[0] = 0;
            pControl = pPage->ControlFind (L"Notetext");
            if (pControl)
               pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBWarning (L"You must type in some text for the Note.");
               return TRUE;
            }

            // get the cateogyr
            // BUGFIX - Category for notes
            pControl = pPage->ControlFind (gszCategory);
            PWSTR pszCat;
            int   iIndex;
            iIndex = -1;
            if (pControl)
               iIndex = pControl->AttribGetInt (gszCurSel);
            pszCat = (PWSTR) gCurNote.m_listCategories.Get((DWORD)iIndex);

            // else add
            PCNoteTask pTask;
            pTask = gCurNote.TaskAdd (szHuge, pszCat ? pszCat : L"", (DWORD)-1);

            // note in the daily log that resolved
            WCHAR szHuge2[10000];
            wcscpy (szHuge2, L"Note: ");
            wcscat (szHuge2, szHuge);
            CalendarLogAdd (Today(), Now(), -1, szHuge2);


            // tell user this
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Note added.");

            gCurNote.m_fDirty = TRUE;
            gCurNote.Flush();
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            // remove the selected category

            PCEscControl   pControl;
            pControl = pPage->ControlFind (gszCategory);
            int   iIndex;
            iIndex = -1;
            if (pControl)
               iIndex = pControl->AttribGetInt (gszCurSel);
            if (iIndex < 0) {
               pPage->MBWarning (L"You must first select a category to remove.");
               return TRUE;
            }
            pControl->AttribSetInt (gszCurSel, -1);

            gCurNote.m_listCategories.Remove((DWORD)iIndex);
            gCurNote.m_fDirty = TRUE;
            gCurNote.Flush();

            return TRUE;
         }

      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only care about projectlist
         if (_wcsicmp(p->pszSubName, L"CURRENTNOTES"))
            break;

         NoteInit();
         gCurNote.Sort();

         // if we don't have any Notes then simple
         if (!gCurNote.m_listTasks.Num()) {
            p->pszSubString = L"";
            return TRUE;
         }

         MemZero (&gMemTemp);
         BOOL fTable;
         DWORD dwElemInTable;
         fTable = FALSE;
         dwElemInTable = 0;
         PWSTR pszLastCategory;
         pszLastCategory = L"!!!!";


         DWORD i;
         PCNoteTask pTask;
         for (i = 0; i < gCurNote.m_listTasks.Num(); i++) {
            pTask = gCurNote.TaskGetByIndex (i);
            if (!pTask)
               continue;

            // if category changed then use new one
            if (_wcsicmp(pszLastCategory, (PWSTR)pTask->m_memCategory.p)) {
               pszLastCategory = (PWSTR) pTask->m_memCategory.p;

               // end tr
               if (fTable) {
                  // null node?
                  if (dwElemInTable%2) {
                     MemCat (&gMemTemp, L"<td width=50%%/>");
                  }

                  MemCat (&gMemTemp, L"</tr>");
                  MemCat (&gMemTemp, L"</table>");
               }

               fTable = FALSE;
            }

            // if no table then add
            if (!fTable) {
               fTable = TRUE;
               dwElemInTable = 0;

               MemCat (&gMemTemp, L"<xbr/>");
               MemCat (&gMemTemp, L"<xSectionTitle>Notes - ");
               MemCatSanitize (&gMemTemp, pszLastCategory[0] ? pszLastCategory : L"No category");
               MemCat (&gMemTemp, L"</xSectionTitle>");
               MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0 lrmargin=16 tbmargin=16>");
            }

            // end tr?
            if (!(dwElemInTable%2) && dwElemInTable)
               MemCat (&gMemTemp, L"</tr>");

            // add tr?
            if (!(dwElemInTable % 2))
               MemCat (&gMemTemp, L"<tr>");
            dwElemInTable++;

            // info
            MemCat (&gMemTemp, L"<td width=50%%>");
            MemCat (&gMemTemp, L"<table width=100%% innerlines=0 border=0><tr><td bgcolor=#ffffc0 innerlines=4 border=2><align parlinespacing=0>");
            MemCatSanitize (&gMemTemp, (PWSTR) pTask->m_memName.p);
            MemCat (&gMemTemp, L"</align></td></tr><tr><td align=left><button style=uparrow href=tt:");
            MemCat (&gMemTemp, (int) pTask->m_dwID);
            MemCat (&gMemTemp, L">Edit</button></td><td align=right><button style=uparrow href=rn:");
            MemCat (&gMemTemp, (int) pTask->m_dwID);
            MemCat (&gMemTemp, L">Delete</button></td></tr></table>");

            MemCat (&gMemTemp, L"</td>");
         }

         // end tr
         if (fTable) {
            // null node?
            if (dwElemInTable%2) {
               MemCat (&gMemTemp, L"<td width=50%%/>");
            }

            MemCat (&gMemTemp, L"</tr>");
            MemCat (&gMemTemp, L"</table>");
         }

         p->pszSubString = (PWSTR) gMemTemp.p;
         return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         // if its a Note link delete and refresh
         BOOL fRefresh;
         if (NotesParseLink (p->psz, pPage, &fRefresh)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (NoteParseLink (p->psz, pPage)) {
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}



/*****************************************************************************
NotesQuickAddPage - Override page callback.
*/
BOOL NotesQuickAddNotePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         NoteInit();
      }
      break;   // make sure to continue on


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // only handle button press of "addproject"
         if (!p->psz)
            break;

         if (!_wcsicmp(p->psz, gszOK)) {
            // get the text
            WCHAR szHuge[1024];
            PCEscControl   pControl;
            DWORD dwNeeded;
            szHuge[0] = 0;
            pControl = pPage->ControlFind (L"Notetext");
            if (pControl)
               pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBWarning (L"You must type in some text for the Note.");
               return TRUE;
            }

            // get the cateogyr
            // BUGFIX - Category for notes
            pControl = pPage->ControlFind (gszCategory);
            PWSTR pszCat;
            int   iIndex;
            iIndex = -1;
            if (pControl)
               iIndex = pControl->AttribGetInt (gszCurSel);
            pszCat = (PWSTR) gCurNote.m_listCategories.Get((DWORD)iIndex);

            // else add
            PCNoteTask pTask;
            pTask = gCurNote.TaskAdd (szHuge, pszCat ? pszCat : L"", (DWORD)-1);

            // note in the daily log that resolved
            WCHAR szHuge2[10000];
            wcscpy (szHuge2, L"Note: ");
            wcscat (szHuge2, szHuge);
            CalendarLogAdd (Today(), Now(), -1, szHuge2);


            // tell user this
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Note added.");

            gCurNote.m_fDirty = TRUE;
            gCurNote.Flush();
            break;   // go to default handler, which exits page
         }

      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}
/**************************************************************************************
NotesQuickAddNote - Quickly add some notes.

inputs
   PCEscPage      pPage - page
returns
   BOOL - TRUE if it was added, FALSE if not
*/
BOOL NotesQuickAddNote (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   NoteInit();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLNOTESQUICKADD, NotesQuickAddNotePage);

   // add this
   if (pszRet && !_wcsicmp(pszRet, gszOK)) {
      return TRUE;
   }
   else
      return FALSE;
}

