/*************************************************************************
Reminders.cpp - For reminders featre

begun 8/9/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

/* C++ objects */
class CReminderTask;
typedef CReminderTask * PCReminderTask;

// Reminder
class CReminder {
public:
   CReminder (void);
   ~CReminder (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCReminderTask TaskAdd (PWSTR pszName, DFDATE date, DWORD dwBefore);
   PCReminderTask TaskGetByIndex (DWORD dwIndex);
   PCReminderTask TaskGetByID (DWORD dwID);
   DWORD TaskIndexByID (DWORD dwID);
   void Sort (void);
   void BringUpToDate (DFDATE today);
   void DeleteAllChildren (DWORD dwID);

   CListFixed     m_listTasks;   // list of PCReminderTasks, ordered according order of doing

   BOOL           m_fDirty;      // set to TRUE if the Reminder is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   DWORD          m_dwNextTaskID;   // next task ID created

private:

};

typedef CReminder * PCReminder;


// task
class CReminderTask {
public:
   CReminderTask (void);
   ~CReminderTask (void);

   PCReminder      m_pReminder; // Reminder that this belongs to. If change sets dirty
   CMem           m_memName;  // name of the task
   DWORD          m_dwID;     // unique ID for the task.
   DFDATE         m_dwDate;   // date the reminder is due
   DFTIME         m_dwAlarm;  // when the alarm goes off
   DWORD          m_dwParent;    // non-zero if created by a reoccurring task
   REOCCURANCE    m_reoccur;     // how often the Meeting reoccurs.


   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);
   void BringUpToDate (DFDATE today);

private:
};

/* globals */
BOOL           gfReminderPopulated = FALSE;// set to true if glistReminder is populated
CReminder       gCurReminder;      // current Reminder that working with

static WCHAR gszDate[] = L"date";
static WCHAR gszReminderDate[] = L"reminderdate";

/*************************************************************************
CReminderTask::constructor and destructor
*/
CReminderTask::CReminderTask (void)
{
   HANGFUNCIN;
   m_pReminder = NULL;
   m_dwID = 0;
   m_dwDate = 0;
   m_dwAlarm = (DWORD)-1;
   m_memName.CharCat (0);
   m_dwParent = 0;
   memset (&m_reoccur, 0, sizeof(m_reoccur));
}

CReminderTask::~CReminderTask (void)
{
   HANGFUNCIN;
   // intentionally left blank
}

/*************************************************************************
CReminderTask::BringUpToDate - This is a reoccurring Meeting, it makes sure
that all occurances of the reoccurring Meeting have been generated up to
(and one beyond) today.

inputs
   DFDATE      today - today
returns
   none
*/
void CReminderTask::BringUpToDate (DFDATE today)
{
   HANGFUNCIN;
   if (!m_reoccur.m_dwPeriod)
      return;  // nothing to do

   // else, see what the lastes date was
   if (m_reoccur.m_dwLastDate > today)
      return;  // already up to date

   // else, must add new Meetings
   while (m_reoccur.m_dwLastDate <= today) {
      // find the next one
      DFDATE   next;
      next = ReoccurNext (&m_reoccur);
      if (!next)
         break;   // error
      // store this
      m_reoccur.m_dwLastDate = next;
      SetDirty();

      // if reoccurring event is beyond end date then don't bother
      if (m_reoccur.m_dwEndDate && (next > m_reoccur.m_dwEndDate))
         break;

      // else, add Meeting
      PCReminderTask pNew;
      pNew = m_pReminder->TaskAdd ((PWSTR)m_memName.p, today, (DWORD)-1);
      if (!pNew)
         break;   // error
      pNew->m_dwDate = next;
      pNew->m_dwParent = m_dwID;
      pNew->m_dwAlarm = m_dwAlarm;
      // all copied over
   }

   // done
}



/*************************************************************************
CReminderTask::SetDirty - Sets the dirty flag in the main Reminder
*/
void CReminderTask::SetDirty (void)
{
   HANGFUNCIN;
   if (m_pReminder)
      m_pReminder->m_fDirty = TRUE;
}

/*************************************************************************
CReminderTask::Write - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CReminderTask::Write (PCMMLNode pParent)
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
   NodeValueSet (pSub, gszID, (int) m_dwID);
   NodeValueSet (pSub, gszDate, (int) m_dwDate);
   NodeValueSet (pSub, gszAlarm, (int) m_dwAlarm);

   // write out reoccurance
   NodeValueSet (pSub, gszParent, (int) m_dwParent);
   NodeValueSet (pSub, gszReoccurPeriod, m_reoccur.m_dwPeriod);
   NodeValueSet (pSub, gszReoccurLastDate, m_reoccur.m_dwLastDate);
   NodeValueSet (pSub, gszReoccurEndDate, m_reoccur.m_dwEndDate);
   NodeValueSet (pSub, gszReoccurParam1, m_reoccur.m_dwParam1);
   NodeValueSet (pSub, gszReoccurParam2, m_reoccur.m_dwParam2);
   NodeValueSet (pSub, gszReoccurParam3, m_reoccur.m_dwParam3);
   return TRUE;
}


/*************************************************************************
CReminderTask::Read - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the task data
reutrns
   BOOL - TRUE if successful
*/
BOOL CReminderTask::Read (PCMMLNode pSub)
{
   HANGFUNCIN;
   PWSTR psz;

   psz = NodeValueGet (pSub, gszName);
   MemZero (&m_memName);
   if (psz)
      MemCat (&m_memName, psz);

   m_dwID = (DWORD) NodeValueGetInt (pSub, gszID, 1);
   m_dwDate = (DWORD) NodeValueGetInt (pSub, gszDate, 0);
   m_dwAlarm = (DWORD) NodeValueGetInt (pSub, gszAlarm, -1);

   m_dwParent = (DWORD) NodeValueGetInt (pSub, gszParent, 0);
   m_reoccur.m_dwPeriod = (DWORD) NodeValueGetInt (pSub, gszReoccurPeriod, 0);
   m_reoccur.m_dwLastDate = (DWORD) NodeValueGetInt (pSub, gszReoccurLastDate, 0);
   m_reoccur.m_dwEndDate = (DWORD) NodeValueGetInt (pSub, gszReoccurEndDate, 0);
   m_reoccur.m_dwParam1 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam1, 0);
   m_reoccur.m_dwParam2 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam2, 0);
   m_reoccur.m_dwParam3 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam3, 0);
   return TRUE;
}

/*************************************************************************
CReminder::Constructor and destructor - Initialize
*/
CReminder::CReminder (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwNextTaskID = 1;

   m_listTasks.Init (sizeof(PCReminderTask));
}

CReminder::~CReminder (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new Reminder is cleared
   Init();
}

/************************************************************************
CReminder::TaskAdd - Adds a mostly default task.

inputs
   PWSTR    pszName - name
   PWSTR    pszSubReminder - sub Reminder
   DWORD    dwDuration - duration (1/100th of a day)
   DWORD    dwBefore - Task index to insert before. -1 for after
returns
   PCReminderTask - To the task object, to be added further
*/
PCReminderTask CReminder::TaskAdd (PWSTR pszName, DFDATE date,
                                 DWORD dwBefore)
{
   HANGFUNCIN;
   PCReminderTask pTask;
   pTask = new CReminderTask;
   if (!pTask)
      return NULL;

   pTask->m_dwID = m_dwNextTaskID++;
   m_fDirty = TRUE;
   pTask->m_pReminder = this;

   MemZero (&pTask->m_memName);
   MemCat (&pTask->m_memName, pszName);
   pTask->m_dwDate = date;

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
PCReminderTask CReminder::TaskGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCReminderTask pTask, *ppTask;

   ppTask = (PCReminderTask*) m_listTasks.Get(dwIndex);
   if (!ppTask)
      return NULL;
   pTask = *ppTask;
   return pTask;
}

/*************************************************************************
TaskGetByID - Given a task ID, this returns a task, or NULL if can't find.
*/
PCReminderTask CReminder::TaskGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCReminderTask pTask;
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
DWORD CReminder::TaskIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCReminderTask pTask;
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

/*************************************************************************
CReminder::Write - Writes all the Reminder data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
Reminder object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CReminder::Write (void)
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


   // BUGBUG - 2.0 - at some point write out last modified, for search purposes

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
      if (_wcsicmp(pSub->NameGet(), gszTask))
         continue;

      // else, old task. delete
      pNode->ContentRemove (i);
   }

   // write out other info, such as tasks, completed and not
   PCReminderTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCReminderTask*) m_listTasks.Get(i));
      pTask->Write (pNode);
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
CReminder::Flush - If m_fDirty is set it writes out the Reminder and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CReminder::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CReminder::Init(void) - Initializes a blank Reminder. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CReminder::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCReminderTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCReminderTask*) m_listTasks.Get(i));
      delete pTask;
   }
   m_listTasks.Clear();

   // new values for name, description, and days per week
   m_dwNextTaskID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   // bring reoccurring Meetings up to date
   BringUpToDate (Today());
   // sort
   Sort ();

   return TRUE;
}

/*************************************************************************
CReminder::Init(dwDatabaseNode) - Initializes by reading the Reminder in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a Reminder, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CReminder::Init (DWORD dwDatabaseNode)
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
   if (_wcsicmp(pNode->NameGet(), gszReminderListNode)) {
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

      PCReminderTask pTask;
      pTask = new CReminderTask;
      pTask->Read (pSub);
      pTask->m_pReminder = this;


      // add to list
      m_listTasks.Add (&pTask);
   }


   // bring reoccurring Meetings up to date
   BringUpToDate (Today());
   // sort
   Sort ();

   // if first start up then put in a reminder 1 month from now for people to register
   if (!NodeValueGetInt (pNode, gszExamples, FALSE)) {
      // set flag true
      NodeValueSet (pNode, gszExamples, (int) TRUE);

      TaskAdd (L"Dragonfly is shareware so you can try it out. "
         L"If you're still using Dragonfly a month after installing it, please register your copy.",
         MinutesToDFDATE (DFDATEToMinutes(Today()) + 60*24*31), (DWORD)-1);

      // write to disk
      Flush();
      gpData->Flush();
   }
   
   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/*******************************************************************************
Sort - Sorts the reminder list by date.
*/
int __cdecl ReminderSort(const void *elem1, const void *elem2 )
{
   PCReminderTask t1, t2;
   t1 = *((PCReminderTask*)elem1);
   t2 = *((PCReminderTask*)elem2);

   // non-automatic have priority
   if (t1->m_reoccur.m_dwPeriod && !t2->m_reoccur.m_dwPeriod)
      return 1;
   else if (!t1->m_reoccur.m_dwPeriod && t2->m_reoccur.m_dwPeriod)
      return -1;

   // return
   // BUGFIX - Was non-deterministic in sort if had same date
   if (t1->m_dwDate != t2->m_dwDate)
      return (int) t1->m_dwDate - (int) t2->m_dwDate;

   // BUGFIX - sort by alarm time
   if (t1->m_dwAlarm != t2->m_dwAlarm)
      return (int) t1->m_dwAlarm - (int) t2->m_dwAlarm;

   return (int) t1->m_dwID - (int) t2->m_dwID;
}

void CReminder::Sort (void)
{
   qsort (m_listTasks.Get(0), m_listTasks.Num(), sizeof(PCReminderTask), ReminderSort);
}

/************************************************************************
BringUpToDate - Brings all the reoccurring Meetings up to date, today.
*/
void CReminder::BringUpToDate (DFDATE today)
{
   HANGFUNCIN;
   DWORD dwNum = m_listTasks.Num();
   DWORD i;
   PCReminderTask pTask;
   for (i = 0; i < dwNum; i++) {
      pTask = TaskGetByIndex (i);
      pTask->BringUpToDate (today);
   }

   // BUGFIX - Was crashing
   Flush();
}

/*************************************************************************
DeleteAllChildren - Given a reoccurring Task ID, this deletes all Task
instances created by the reoccurring Task.

inputs
   DWORD    dwID - ID
*/
void CReminder::DeleteAllChildren (DWORD dwID)
{
   HANGFUNCIN;
   DWORD dwNum = m_listTasks.Num();
   DWORD i;
   PCReminderTask pTask;
   for (i = dwNum - 1; i < dwNum; i--) {
      pTask = TaskGetByIndex (i);
      if (!pTask || (pTask->m_dwParent != dwID))
         continue;

      // delete
      m_listTasks.Remove (i);
      delete pTask;
   }

   m_fDirty = TRUE;
   Flush();
}

/***********************************************************************
ReminderInit - Makes sure the reminder object is filled. If not,
it's loaded in. This CAN be called multiple times.

This fills in:
   gfReminderPopulated - Set to TRUE when it's been loaded
   gCurReminder - fills in

inputs
   none
returns
   BOOL - error
*/
BOOL ReminderInit (void)
{
   HANGFUNCIN;
   if (gfReminderPopulated)
      return TRUE;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszReminderListNode, &dwNum);
   if (!pNode)
      return NULL;   // error. This shouldn't happen
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfReminderPopulated = TRUE;

   // pass this on
   return gCurReminder.Init (dwNum);
}


/*****************************************************************************
ReminderComplete - Remove a reminder
*/
void ReminderComplete (PCEscPage pPage, PCReminderTask pTask)
{
   HANGFUNCIN;
   DWORD dwIndex;
   DWORD dwDelChildren = 0;
   dwIndex = gCurReminder.TaskIndexByID (pTask->m_dwID);

   // if it's a reoccuring reminder then ask if want to delete all instances
   if (pTask->m_reoccur.m_dwPeriod) {
      int iRet;
      iRet = pPage->MBYesNo (L"Do you want to delete all instances of the reminder?",
         L"Because this is a reoccurring reminder it may have generated instances of the "
         L"reminder occurring on specific days. If you press 'Yes' you will delete them too. "
         L"'No' will leave them on your reminders list.");

      // if delete children do so
      if (iRet == IDYES)
         dwDelChildren = pTask->m_dwID;
   }

   // note in the daily log that resolved
   WCHAR szHuge[10000];
   wcscpy (szHuge, L"Reminder deleted: ");
   wcscat (szHuge, (PWSTR)pTask->m_memName.p);
   CalendarLogAdd (Today(), Now(), -1, szHuge);

   // delete this
   delete pTask;
   gCurReminder.m_listTasks.Remove (dwIndex);
   gCurReminder.m_fDirty = TRUE;
   gCurReminder.Flush();

   if (dwDelChildren)
      gCurReminder.DeleteAllChildren (dwDelChildren);

   // tell user
   EscChime (ESCCHIME_INFORMATION);
   EscSpeak (L"Reminder deleted.");
}

/*****************************************************************************
ReminderParseLink - If a page uses a reminders substitution, which generates
"rn:XXXX" to indicate that a reminder link was called, call this. If it's
not a reminder link, returns FALSE. If it is, it deletes the reminder and
returns TRUE. The page should then refresh itself.

inputs
   PWSTR    pszLink - link
   PCEscPage pPage - page
returns
   BOOL
*/
BOOL ReminderParseLink (PWSTR pszLink, PCEscPage pPage)
{
   HANGFUNCIN;
   // make sure its the right type
   if ((pszLink[0] != L'r') || (pszLink[1] != L'n') || (pszLink[2] != L':'))
      return FALSE;

   // make sure have reminders loaded
   ReminderInit ();

   // get the number
   DWORD dwIndex, dwID;
   dwID = 0;
   AttribToDecimal (pszLink + 3, (int*) &dwID);
   dwIndex = gCurReminder.TaskIndexByID (dwID);
   if (dwIndex == (DWORD)-1)
      return TRUE;   // error

   PCReminderTask pTask;
   pTask = *((PCReminderTask*) gCurReminder.m_listTasks.Get(dwIndex));

   // BUGFIX - ask to make sure wantt o delete
   if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the reminder?"))
      return TRUE;  // nothing change

   ReminderComplete (pPage, pTask);

   // done
   return TRUE;
}
/*****************************************************************************
RemindersTaskAddReoccurPage - Override page callback.
*/
BOOL RemindersTaskAddReoccurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
            // get the text
            WCHAR szHuge[1024];
            PCEscControl   pControl;
            DWORD dwNeeded;
            szHuge[0] = 0;
            pControl = pPage->ControlFind (L"remindertext");
            if (pControl)
               pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBWarning (L"You must type in some text for the reminder.");
               return TRUE;
            }

            // else add
            PCReminderTask pTask;
            pTask = gCurReminder.TaskAdd (szHuge, Today(), (DWORD)-1);

            // fill in reoccur
            ReoccurFromControls (pPage, &pTask->m_reoccur);

            // BUGFIX - Alarm
            pTask->m_dwAlarm = TimeControlGet (pPage, gszAlarm);

            // bring it up to date
            pTask->BringUpToDate(Today());

            gCurReminder.Sort();

            // tell user this
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Reminder added.");

            gCurReminder.m_fDirty = TRUE;
            gCurReminder.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
RemindersTaskShowAddReoccurUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project Task was added
*/
BOOL RemindersTaskShowAddReoccurUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   ReminderInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLREMINDERADDREOCCUR, RemindersTaskAddReoccurPage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}


/*****************************************************************************
RemindersPage - Override page callback.
*/
BOOL RemindersPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the date for the reminder to today
         DFDATE date;
         date = Today();
         DateControlSet (pPage, gszReminderDate, date);

         ReminderInit();
      }
      break;   // make sure to continue on


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"addreminder")) {
            // get the text
            WCHAR szHuge[1024];
            PCEscControl   pControl;
            DWORD dwNeeded;
            szHuge[0] = 0;
            pControl = pPage->ControlFind (L"remindertext");
            if (pControl)
               pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBWarning (L"You must type in some text for the reminder.");
               return TRUE;
            }

            // get the data
            DFDATE date;
            date = DateControlGet (pPage, gszReminderDate);

            // else add
            PCReminderTask pTask;
            pTask = gCurReminder.TaskAdd (szHuge, date, (DWORD)-1);

            // BUGFIX - Alarm
            pTask->m_dwAlarm = TimeControlGet (pPage, gszAlarm);

            gCurReminder.Sort();

            // tell user this
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Reminder added.");

            gCurReminder.m_fDirty = TRUE;
            gCurReminder.Flush();
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addreoccur")) {
            if (RemindersTaskShowAddReoccurUI(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only care about projectlist
         if (_wcsicmp(p->pszSubName, L"REMINDERLIST"))
            break;

         ReminderInit();
         gCurReminder.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(Today())+60*24*7));
         gCurReminder.Sort();

         // if we don't have any reminders then simple
         if (!gCurReminder.m_listTasks.Num()) {
            p->pszSubString = L"";
            return TRUE;
         }

         MemZero (&gMemTemp);
         MemCat (&gMemTemp, L"<xbr/>");
         MemCat (&gMemTemp, L"<xsectiontitle>Reminders</xsectiontitle>");
         MemCat (&gMemTemp, L"<p>Clicking on a reminder will delete it.</p>");
         MemCat (&gMemTemp, L"<xListTable innerlines=0>");

         // put in reminders
         DFDATE   lastdate, today;
         lastdate = (DWORD) -1;
         today = Today();

         DWORD i;
         PCReminderTask pTask;
         for (i = 0; i < gCurReminder.m_listTasks.Num(); i++) {
            pTask = gCurReminder.TaskGetByIndex (i);
            if (!pTask)
               continue;

            // if switched date regime (after today) then separator
            if (((lastdate <= today) || (lastdate==-1)) && (pTask->m_dwDate > today))
               MemCat (&gMemTemp, L"<xfuturereminder/>");
            else if ((lastdate==-1) && (pTask->m_dwDate <= today))
               MemCat (&gMemTemp, L"<xactivereminder/>");

            lastdate = pTask->m_dwDate;

            // show it
            MemCat (&gMemTemp, L"<tr><xtdtask href=rn:");
            MemCat (&gMemTemp, (int) pTask->m_dwID);
            MemCat (&gMemTemp, L">");
            MemCatSanitize (&gMemTemp, (PWSTR) pTask->m_memName.p);
            MemCat (&gMemTemp, L"<xHoverHelpShort>Click on this reminder to delete it.</xHoverHelpShort>");
            MemCat (&gMemTemp, L"</xtdtask>");
            MemCat (&gMemTemp, L"<xtdcompleted></xtdcompleted>"); // filler to space with the rest
            MemCat (&gMemTemp, L"<xtdcompleted>");

            // if it's reoccurring then show
            WCHAR szTemp[128];
            if (pTask->m_reoccur.m_dwPeriod) {
               MemCat (&gMemTemp, L"<italic>");
               ReoccurToString (&pTask->m_reoccur, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</italic>");
            }
            else {
               DFDATEToString (pTask->m_dwDate, szTemp);
               MemCat (&gMemTemp, szTemp);
            }

            // BUGFIX - If there's an alarm show tht
            if (pTask->m_dwAlarm != (DWORD)-1) {
               DFTIMEToString (pTask->m_dwAlarm, szTemp);
               MemCat (&gMemTemp, L"<br/>Alarm at ");
               MemCat (&gMemTemp, szTemp);
            }

            MemCat (&gMemTemp, L"</xtdcompleted>");
            MemCat (&gMemTemp, L"</tr>");
#if 0
            WCHAR szTemp[64];
            MemCat (&gMemTemp, L"<tr><xMeetingTime>");
            DFDATEToString (pTask->m_dwDate, szTemp);
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</xMeetingTime><xMeetingInfo href=rn:");
            MemCat (&gMemTemp, (int) pTask->m_dwID);
            MemCat (&gMemTemp, L">");
            MemCatSanitize (&gMemTemp, (PWSTR) pTask->m_memName.p);
            MemCat (&gMemTemp, L"<xClickReminder/></xMeetingInfo></tr>");
#endif // 0
         }

         MemCat (&gMemTemp, L"</xlisttable>");
         p->pszSubString = (PWSTR) gMemTemp.p;
         return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         // if its a reminder link delete and refresh
         if (ReminderParseLink (p->psz, pPage)) {
            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************
RemindersSummary - Make a short summary of all the reminders work for
the "Today" page.

inputs
   DFDATE   date - date to look up until
   DFDATE   start - date to start from. 0 for all days to and including
         date. if date==start then only show reminders for that day.
   BOOL     fShowIfEmpty - If TRUE then show even if it's empty
returns
   PWSTR - Pointer to the MML to insert. Usually gMemTemp.p
*/
PWSTR RemindersSummary (DFDATE date, DFDATE start, BOOL fShowIfEmpty)
{
   HANGFUNCIN;
   // make sure there are reminders that happen today or before
   ReminderInit();
   gCurReminder.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(date)+60*24*7));
   gCurReminder.Sort();

   DWORD i;
   PCReminderTask pTask;
   for (i = 0; i < gCurReminder.m_listTasks.Num(); i++) {
      pTask = gCurReminder.TaskGetByIndex (i);
      if (!pTask)
         continue;

      if ((pTask->m_dwDate <= date) && (pTask->m_dwDate >= start))
         break;
   }
   if (!fShowIfEmpty && (i >= gCurReminder.m_listTasks.Num()))
      return L""; // no reminders


   // else have some
   MemZero (&gMemTemp);
   //MemCat (&gMemTemp, L"<xbr/>");
   //MemCat (&gMemTemp, L"<xsectiontitle>Reminders</xsectiontitle>");
   // MemCat (&gMemTemp, L"<p>Clicking on a reminder will delete it.</p>");
   MemCat (&gMemTemp, L"<xListTable innerlines=0>");
   // BUGFIX - User request for add XXX button in today, tomorrow, etc. page
   if (!gfPrinting) {
      MemCat (&gMemTemp, L"<xtrheader align=left>");
      MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddreminder");
      if (date)
         MemCat (&gMemTemp, (int) date);
      MemCat (&gMemTemp, L"><small><font color=#ffffff>Add reminder</font></small></button>");
      MemCat (&gMemTemp, L"<align align=right><a href=r:114 color=#c0c0ff>Reminders</a></align></xtrheader>");
   }
   else {
      MemCat (&gMemTemp, L"<xtrheader align=right>");
      MemCat (&gMemTemp, L"<a href=r:114 color=#c0c0ff>Reminders</a></xtrheader>");
   }
   // put in reminders

   BOOL fFound;
   fFound = FALSE;
   for (i = 0; i < gCurReminder.m_listTasks.Num(); i++) {
      pTask = gCurReminder.TaskGetByIndex (i);
      if (!pTask)
         continue;

      // if after today ignore
      if (pTask->m_dwDate > date)
         continue;

      // if before start then ignore
      if (pTask->m_dwDate < start)
         continue;

      // if it's reoccurring don't show
      if (pTask->m_reoccur.m_dwPeriod)
         continue;

      // show it
      MemCat (&gMemTemp, L"<tr>");
      if (gfPrinting) {
         MemCat (&gMemTemp, L"<xtdtasknolink>");
      }
      else {
         MemCat (&gMemTemp, L"<xtdtask href=rn:");
         MemCat (&gMemTemp, (int) pTask->m_dwID);
         MemCat (&gMemTemp, L">");
      }
      MemCatSanitize (&gMemTemp, (PWSTR) pTask->m_memName.p);
      if (gfPrinting) {
         MemCat (&gMemTemp, L"</xtdtasknolink>");
      }
      else {
         MemCat (&gMemTemp, L"<xHoverHelpShort>Click on this reminder to delete it.</xHoverHelpShort>");
         MemCat (&gMemTemp, L"</xtdtask>");
      }
      MemCat (&gMemTemp, L"<xtdcompleted></xtdcompleted>"); // filler to space with the rest
      MemCat (&gMemTemp, L"<xtdcompleted>");
      WCHAR szTemp[64];
      DFDATEToString (pTask->m_dwDate, szTemp);
      MemCat (&gMemTemp, szTemp);

      // BUGFIX - If there's an alarm show tht
      if (pTask->m_dwAlarm != (DWORD)-1) {
         DFTIMEToString (pTask->m_dwAlarm, szTemp);
         MemCat (&gMemTemp, L"<br/>Alarm at ");
         MemCat (&gMemTemp, szTemp);
      }
      MemCat (&gMemTemp, L"</xtdcompleted>");
      MemCat (&gMemTemp, L"</tr>");
      fFound = TRUE;
#if 0
      MemCat (&gMemTemp, L"<tr><td><small><button style=righttriangle valign=top buttonheight=12 buttonwidth=12 href=rn:");
      MemCat (&gMemTemp, (int) pTask->m_dwID);
      MemCat (&gMemTemp, L">");
      MemCatSanitize (&gMemTemp, (PWSTR) pTask->m_memName.p);
      MemCat (&gMemTemp, L"<xHoverHelpShort>Click on this reminder to delete it.</xHoverHelpShort>");
      MemCat (&gMemTemp, L"</button></small></td></tr>");
#endif // 0
   }

   // if found nothing then at least say it's empty
   if (!fFound) {
      MemCat (&gMemTemp, L"<tr><td><italic>No reminders.</italic></td></tr>");
   }

   MemCat (&gMemTemp, L"</xlisttable>");
   return (PWSTR) gMemTemp.p;
}


/*************************************************************************
ReminderAdd - Adds a reminder to the list.

inputs
   PWSTR    psz - reminder string
   DFDATE   due - when it goes off
returns
   BOOL - TRUE if OK
*/
BOOL ReminderAdd (PWSTR psz, DFDATE due)
{
   HANGFUNCIN;
   ReminderInit();

   PCReminderTask pTask;
   pTask = gCurReminder.TaskAdd (psz, due, (DWORD)-1);
   gCurReminder.m_fDirty = TRUE;
   gCurReminder.Sort();
   gCurReminder.Flush();
   return pTask ? TRUE : FALSE;
}



/*************************************************************************
ReminderMonthEnumerate - Enumerates a months worth of data.

inputs
   DFDATE   date - Use only the month and year field
   PCMem    papMem - Pointer to an arry of 31 PCMem, initially set to NULL.
               If any events occur on that day then the function will create
               a new CMem (which must be freed by the caller), and
               fill it with a MML (null-terminated) text string. The
               MML has <li>TEXT</li> so that these can be put into a list.
   DFDATE   bunchup - If this is non-zero then bunch up any reminders started
            before bunchup to bunchp. Used by the weekly calendar list to
            make sure person has a list of all reminders/tasks
   BOOL     fWithLinks - If TRUE, then not only include the text, but include links
               in the text so can jump to the project. BUGFIX - Added to make view
               calendar as list better
returns
   none
*/
void ReminderMonthEnumerate (DFDATE date, PCMem *papMem, DFDATE bunchup, BOOL fWithLinks)
{
   HANGFUNCIN;
   // make sure there are reminders that happen today or before
   ReminderInit();
   gCurReminder.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(date)+60*24*7));
   gCurReminder.Sort();

   DWORD i;
   PCReminderTask pTask;
   for (i = 0; i < gCurReminder.m_listTasks.Num(); i++) {
      pTask = gCurReminder.TaskGetByIndex (i);
      if (!pTask)
         continue;

      // if it's reoccurring dont show
      if (pTask->m_reoccur.m_dwPeriod)
         continue;

      DFDATE bunch;
      bunch = pTask->m_dwDate;
      if (bunch < bunchup)
         bunch = bunchup;

      // if after today ignore
      if ( (MONTHFROMDFDATE(bunch) != MONTHFROMDFDATE(date)) ||
         (YEARFROMDFDATE(bunch) != YEARFROMDFDATE(date)) )
         continue;

      // find the day
      int   iDay;
      iDay = (int) DAYFROMDFDATE(bunch);
      if (!papMem[iDay]) {
         papMem[iDay] = new CMem;
         MemZero (papMem[iDay]);
      }

      // append
      MemCat (papMem[iDay], L"<li>");
      if (fWithLinks) {
         MemCat (papMem[iDay], L"<a href=rn:");
         MemCat (papMem[iDay], (int) pTask->m_dwID);
         MemCat (papMem[iDay], L">");
      }
      MemCatSanitize (papMem[iDay], (PWSTR) pTask->m_memName.p);
      if (fWithLinks)
         MemCat (papMem[iDay], L"</a>");

      // BUGFIX - If there's an alarm show tht
      if (pTask->m_dwAlarm != (DWORD)-1) {
         WCHAR szTemp[32];
         DFTIMEToString (pTask->m_dwAlarm, szTemp);
         MemCat (papMem[iDay], L" <italic>(Alarm at ");
         MemCat (papMem[iDay], szTemp);
         MemCat (papMem[iDay], L")</italic>");
      }

      MemCat (papMem[iDay], L"</li>");
   }
}


/*****************************************************************************
ReminderQuickAddPage - Override page callback.
*/
BOOL ReminderQuickAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the date for the reminder to today
         // BUGFIX - Set date to the day that the button in combo was pressed
         DateControlSet (pPage, gszReminderDate, pPage->m_pUserData ? (DFDATE)pPage->m_pUserData : Today());

         ReminderInit();
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
            pControl = pPage->ControlFind (L"remindertext");
            if (pControl)
               pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBWarning (L"You must type in some text for the reminder.");
               return TRUE;
            }

            // get the data
            DFDATE date;
            date = DateControlGet (pPage, gszReminderDate);

            // else add
            PCReminderTask pTask;
            pTask = gCurReminder.TaskAdd (szHuge, date, (DWORD)-1);

            // BUGFIX - Alarm
            pTask->m_dwAlarm = TimeControlGet (pPage, gszAlarm);
            gCurReminder.Sort();

            // tell user this
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Reminder added.");

            gCurReminder.m_fDirty = TRUE;
            gCurReminder.Flush();

            break;   // go to default handler, which exits page
         }

      }
      break;


   };


   return DefPage (pPage, dwMessage, pParam);
}
/**************************************************************************************
ReminderQuickAdd - Quickly add some reminderss.

inputs
   PCEscPage      pPage - page
   DFDATE         dAddDate - Date to use. 0 => don't set
returns
   BOOL - TRUE if it was added, FALSE if not
*/
BOOL ReminderQuickAdd (PCEscPage pPage, DFDATE dAddDate)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   ReminderInit();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLREMINDERQUICKADD, ReminderQuickAddPage, (PVOID) dAddDate);

   // add this
   if (pszRet && !_wcsicmp(pszRet, gszOK)) {
      return TRUE;
   }
   else
      return FALSE;
}


/*****************************************************************************
RemindersCheckAlarm - Check to see if any alarms go off.

inputs
   DFDATE      today - today's date
   DFTIME      now - current time
*/
void RemindersCheckAlarm (DFDATE today, DFTIME now)
{
   HANGFUNCIN;
   ReminderInit();

   // loop
   DWORD i;
   PCReminderTask pTask;
   for (i = 0; i < gCurReminder.m_listTasks.Num(); i++) {
      pTask = gCurReminder.TaskGetByIndex (i);
      if (!pTask)
         continue;

      // skip reoccurring reminders
      if (pTask->m_reoccur.m_dwPeriod)
         continue;

      DFDATE date;
      date = pTask->m_dwDate;
      if (!date)
         date = today;
      if (date > today)
         continue;
      if (pTask->m_dwAlarm == (DWORD)-1)
         continue;
      if ((date == today) && (pTask->m_dwAlarm > now))
         continue;   // just a bit shy

      // else, have an alarm
      CMem mSub;
      MemZero (&mSub);
      
      // sub
      MemCat (&mSub, (PWSTR) pTask->m_memName.p);

      DWORD dwSnooze;
      dwSnooze = AlarmUI (L"You have a reminder.", (PWSTR)mSub.p);

      if (dwSnooze) {
         int iTime;
         iTime = DFTIMEToMinutes(Now()) + dwSnooze;
         pTask->m_dwAlarm = TODFTIME(iTime / 60, iTime % 60);
      }
      else {
         // remove the task
         ReminderComplete (NULL, pTask);
      }
   }


}


/* BUGBUG
From: "Marguerite" <marguerite@runbox.com>
Sent: Tuesday, April 27, 2004 11:28 PM
> Re the alarm. I guess what would be most useful to me is a 'random' 
> alarm... one for instance that could be set go off randomly 5 times (or 
> however many times) from one point in the day to another in a day that the 
> user can define. Studies indicate this is a great tool to help increase focus.
*/

