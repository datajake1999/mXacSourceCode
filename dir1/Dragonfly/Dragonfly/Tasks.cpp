/*************************************************************************
Tasks.cpp - For takss featre

begun 8/10/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


/* C++ objects */
class CWorkTask;
typedef CWorkTask * PCWorkTask;

// Work
class CWork {
public:
   CWork (void);
   ~CWork (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCWorkTask TaskAdd (PWSTR pszName, PWSTR pszDescription, DWORD dwBefore);
   PCWorkTask TaskGetByIndex (DWORD dwIndex);
   PCWorkTask TaskGetByID (DWORD dwID);
   DWORD TaskIndexByID (DWORD dwID);
   void Sort (void);
   void BringUpToDate (DFDATE today);
   void DeleteAllChildren (DWORD dwID);


   CListFixed     m_listTasks;   // list of PCWorkTasks, ordered according order of doing

   BOOL           m_fDirty;      // set to TRUE if the Work is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   DWORD          m_dwNextTaskID;   // next task ID created

private:

};

typedef CWork * PCWork;


// task
class CWorkTask {
public:
   CWorkTask (void);
   ~CWorkTask (void);

   PCWork         m_pWork; // Work that this belongs to. If change sets dirty
   CMem           m_memName;  // name of the task
   CMem           m_memDescription; // description
   DWORD          m_dwID;     // unique ID for the task.
   DFDATE         m_dwDateShow;   // date the Work is shown
   DFDATE         m_dwCompleteBy;   // work must be completed by
   DWORD          m_dwPriority;  // priority, 1..6
   DWORD          m_dwEstimated;  // estimated amount of work, in minutes
   DWORD          m_dwParent;    // non-zero if created by a reoccurring task
   int            m_iJournalID;  // journal entry that it's linked to, or -1
   REOCCURANCE    m_reoccur;     // how often the task reoccurs.
   CPlanTask      m_PlanTask;    // when plan to work on


   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);
   DWORD Priority (void);
   void BringUpToDate (DFDATE today);
private:
};

/* globals */
BOOL           gfWorkPopulated = FALSE;// set to true if glistWork is populated
CWork       gCurWork;      // current Work that working with

static WCHAR gszWorkDate[] = L"Workdate";
static WCHAR gszDaysOfWork[] = L"DaysOfWork";
static WCHAR gszCompleteBy[] = L"completeby";

/*************************************************************************
CWorkTask::constructor and destructor
*/
CWorkTask::CWorkTask (void)
{
   HANGFUNCIN;
   m_pWork = NULL;
   m_dwID = 0;
   m_memName.CharCat (0);
   m_memDescription.CharCat (0);
   m_dwDateShow = 0;
   m_dwCompleteBy = 0;
   m_dwPriority = 3;
   m_dwEstimated = 60;
   m_dwParent = 0;
   m_iJournalID = -1;
   memset (&m_reoccur, 0, sizeof(m_reoccur));
}

CWorkTask::~CWorkTask (void)
{
   HANGFUNCIN;
   // intentionally left blank
}


/*************************************************************************
CWorkTask::SetDirty - Sets the dirty flag in the main Work
*/
void CWorkTask::SetDirty (void)
{
   HANGFUNCIN;
   if (m_pWork)
      m_pWork->m_fDirty = TRUE;
}

/*************************************************************************
CWorkTask::Priority - Returns a priority based on the complete-by date.
   The priority gradually ramps up.
*/
DWORD CWorkTask::Priority (void)
{
   HANGFUNCIN;
   // if not complete-by date return current priority
   if (!m_dwCompleteBy)
      return m_dwPriority;

   // if past the completeby then pri=1
   DFDATE   today;
   today = Today();
   if (today >= m_dwCompleteBy)
      return 1;

   // if before start date then use that pri
   if (!m_dwDateShow || (today <= m_dwDateShow))
      return m_dwPriority;

   // how many minutes between the start/end, and between today and start
   __int64 iStart, iDelta, iAlpha;
   iStart = DFDATEToMinutes(m_dwDateShow);
   iDelta = DFDATEToMinutes(m_dwCompleteBy) - iStart;
   iAlpha = DFDATEToMinutes(today) - iStart;

   // priority deltas?
   if (m_dwPriority <= 2)
      return m_dwPriority; // cant get any higher than it's set
   DWORD dwPriDelta;
   dwPriDelta = m_dwPriority - 2;

   iDelta /= (dwPriDelta + 1);
   if (!iDelta)
      iDelta = 1;
   
   // calculate the new pri
   iAlpha = (int) m_dwPriority - (iAlpha / iDelta);
   if (iAlpha < 1)
      iAlpha = 1;

   // done
   return (DWORD) iAlpha;
}

/*************************************************************************
CWorkTask::BringUpToDate - This is a reoccurring task, it makes sure
that all occurances of the reoccurring task have been generated up to
(and one beyond) today.

inputs
   DFDATE      today - today
returns
   none
*/
void CWorkTask::BringUpToDate (DFDATE today)
{
   HANGFUNCIN;
   if (!m_reoccur.m_dwPeriod)
      return;  // nothing to do

   // else, see what the lastes date was
   if (m_reoccur.m_dwLastDate > today)
      return;  // already up to date

   // else, must add new tasks
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

      // else, add task
      PCWorkTask pNew;
      pNew = m_pWork->TaskAdd ((PWSTR)m_memName.p, (PWSTR)m_memDescription.p, (DWORD)-1);
      if (!pNew)
         break;   // error
      pNew->m_dwDateShow = next;
      pNew->m_dwEstimated = m_dwEstimated;
      pNew->m_dwPriority = m_dwPriority;
      pNew->m_dwParent = m_dwID;
      pNew->m_iJournalID = m_iJournalID;
      // all copied over
   }

   // done
}

/*************************************************************************
CWorkTask::Write - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CWorkTask::Write (PCMMLNode pParent)
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
   NodeValueSet (pSub, gszDescription, (PWSTR) m_memDescription.p);
   NodeValueSet (pSub, gszID, (int) m_dwID);
   NodeValueSet (pSub, gszDateShow, (int) m_dwDateShow);
   NodeValueSet (pSub, gszCompleteBy, (int) m_dwCompleteBy);
   NodeValueSet (pSub, gszPriority, (int) m_dwPriority);
   NodeValueSet (pSub, gszEstimated, (int) m_dwEstimated);
   NodeValueSet (pSub, gszParent, (int) m_dwParent);
   NodeValueSet (pSub, gszJournal, m_iJournalID);

   // write out reoccurance
   NodeValueSet (pSub, gszReoccurPeriod, m_reoccur.m_dwPeriod);
   NodeValueSet (pSub, gszReoccurLastDate, m_reoccur.m_dwLastDate);
   NodeValueSet (pSub, gszReoccurEndDate, m_reoccur.m_dwEndDate);
   NodeValueSet (pSub, gszReoccurParam1, m_reoccur.m_dwParam1);
   NodeValueSet (pSub, gszReoccurParam2, m_reoccur.m_dwParam2);
   NodeValueSet (pSub, gszReoccurParam3, m_reoccur.m_dwParam3);

   m_PlanTask.Write(pSub);

   return TRUE;
}


/*************************************************************************
CWorkTask::Read - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the task data
reutrns
   BOOL - TRUE if successful
*/
BOOL CWorkTask::Read (PCMMLNode pSub)
{
   HANGFUNCIN;
   PWSTR psz;

   psz = NodeValueGet (pSub, gszName);
   MemZero (&m_memName);
   if (psz)
      MemCat (&m_memName, psz);

   psz = NodeValueGet (pSub, gszDescription);
   MemZero (&m_memDescription);
   if (psz)
      MemCat (&m_memDescription, psz);

   m_dwID = (DWORD) NodeValueGetInt (pSub, gszID, 1);
   m_dwDateShow = (DWORD) NodeValueGetInt (pSub, gszDateShow, 0);
   m_dwCompleteBy = (DWORD) NodeValueGetInt (pSub, gszCompleteBy, 0);
   m_dwPriority = (DWORD) NodeValueGetInt (pSub, gszPriority, 0);
   m_dwParent = (DWORD) NodeValueGetInt (pSub, gszParent, 0);
   m_dwEstimated = (DWORD) NodeValueGetInt (pSub, gszEstimated, 0);
   m_iJournalID = NodeValueGetInt (pSub, gszJournal, -1);
   m_reoccur.m_dwPeriod = (DWORD) NodeValueGetInt (pSub, gszReoccurPeriod, 0);
   m_reoccur.m_dwLastDate = (DWORD) NodeValueGetInt (pSub, gszReoccurLastDate, 0);
   m_reoccur.m_dwEndDate = (DWORD) NodeValueGetInt (pSub, gszReoccurEndDate, 0);
   m_reoccur.m_dwParam1 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam1, 0);
   m_reoccur.m_dwParam2 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam2, 0);
   m_reoccur.m_dwParam3 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam3, 0);

   m_PlanTask.Read(pSub);

   return TRUE;
}

/*************************************************************************
CWork::Constructor and destructor - Initialize
*/
CWork::CWork (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwNextTaskID = 1;

   m_listTasks.Init (sizeof(PCWorkTask));
}

CWork::~CWork (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new Work is cleared
   Init();
}


/************************************************************************
CWork::TaskAdd - Adds a mostly default task.

inputs
   PWSTR    pszName - name
   PWSTR    pszDescription - description
   DWORD    dwBefore - Task index to insert before. -1 for after
returns
   PCWorkTask - To the task object, to be added further
*/
PCWorkTask CWork::TaskAdd (PWSTR pszName, PWSTR pszDescription,
                                 DWORD dwBefore)
{
   HANGFUNCIN;
   PCWorkTask pTask;
   pTask = new CWorkTask;
   if (!pTask)
      return NULL;

   pTask->m_dwID = m_dwNextTaskID++;
   m_fDirty = TRUE;
   pTask->m_pWork = this;

   MemZero (&pTask->m_memName);
   MemCat (&pTask->m_memName, pszName);
   MemZero (&pTask->m_memDescription);
   MemCat (&pTask->m_memDescription, pszDescription);
  
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
PCWorkTask CWork::TaskGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCWorkTask pTask, *ppTask;

   ppTask = (PCWorkTask*) m_listTasks.Get(dwIndex);
   if (!ppTask)
      return NULL;
   pTask = *ppTask;
   return pTask;
}

/*************************************************************************
TaskGetByID - Given a task ID, this returns a task, or NULL if can't find.
*/
PCWorkTask CWork::TaskGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCWorkTask pTask;
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
DWORD CWork::TaskIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCWorkTask pTask;
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
CWork::Write - Writes all the Work data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
Work object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CWork::Write (void)
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
   PCWorkTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCWorkTask*) m_listTasks.Get(i));
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
CWork::Flush - If m_fDirty is set it writes out the Work and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CWork::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CWork::Init(void) - Initializes a blank Work. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CWork::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCWorkTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCWorkTask*) m_listTasks.Get(i));
      delete pTask;
   }
   m_listTasks.Clear();

   // new values for name, description, and days per week
   m_dwNextTaskID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   // bring reoccurring tasks up to date
   BringUpToDate (Today());

   // sort
   Sort ();

   return TRUE;
}

/*************************************************************************
CWork::Init(dwDatabaseNode) - Initializes by reading the Work in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a Work, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CWork::Init (DWORD dwDatabaseNode)
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
   if (_wcsicmp(pNode->NameGet(), gszWorkListNode)) {
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

      PCWorkTask pTask;
      pTask = new CWorkTask;
      pTask->Read (pSub);
      pTask->m_pWork = this;


      // add to list
      m_listTasks.Add (&pTask);
   }


   // bring reoccurring tasks up to date
   BringUpToDate (Today());
   // sort
   Sort ();

   
   // if first start up then put in a reminder 1 month from now for people to register
   if (!NodeValueGetInt (pNode, gszExamples, FALSE)) {
      // set flag true
      NodeValueSet (pNode, gszExamples, (int) TRUE);

      PCWorkTask pTask;

      // experiment with dragonfly
      pTask = TaskAdd (L"Familiarize yourself with Dragonfly",
         L"Spend some time playing with Dragonfly and experimenting with the features.", (DWORD)-1);
      if (pTask) {
         pTask->m_dwPriority = 3;
      }

      // backup
      pTask = TaskAdd (L"Backup Dragonfly files",
         L"Backup Dragonfly files regularly so you don't lose any information if your PC dies.", (DWORD)-1);
      if (pTask) {
         pTask->m_dwPriority = 4;
         pTask->m_reoccur.m_dwLastDate = Today();
         pTask->m_reoccur.m_dwPeriod = REOCCUR_EVERYNDAYS;
         pTask->m_reoccur.m_dwParam1 = 31;
      }

      // download new dragonfly
      pTask = TaskAdd (L"Download the latest version of Dragonfly",
         L"Download the latest version of Dragonfly. Make sure to backup your Dragonfly files first.", (DWORD)-1);
      if (pTask) {
         pTask->m_dwPriority = 5;
         pTask->m_reoccur.m_dwLastDate = Today();
         pTask->m_reoccur.m_dwPeriod = REOCCUR_EVERYNDAYS;
         pTask->m_reoccur.m_dwParam1 = 365/2;
      }


      // write to disk
      Flush();
      gpData->Flush();
   }

   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/*******************************************************************************
Sort - Sorts the Work list by date.
*/
int __cdecl WorkSort(const void *elem1, const void *elem2 )
{
   PCWorkTask t1, t2;
   t1 = *((PCWorkTask*)elem1);
   t2 = *((PCWorkTask*)elem2);

   // return
   if (t1->Priority() != t2->Priority())
      return (int) t1->Priority() - (int) t2->Priority();

   // non-automatic have priority
   if (t1->m_reoccur.m_dwPeriod && !t2->m_reoccur.m_dwPeriod)
      return 1;
   else if (!t1->m_reoccur.m_dwPeriod && t2->m_reoccur.m_dwPeriod)
      return -1;

   // by date appear
   // BUGFIX - Was non-deterministic in date that showed
   if (t1->m_dwDateShow != t2->m_dwDateShow)
      return (int) t1->m_dwDateShow - (int)t2->m_dwDateShow;

   // alphabetically
   if (t1->m_memName.p && t2->m_memName.p) {
      int iRet = _wcsicmp ((PWSTR) t1->m_memName.p, (PWSTR) t2->m_memName.p);
      if (iRet)
         return iRet;
   }

   return (int) t1->m_dwID - (int) t2->m_dwID;

}

void CWork::Sort (void)
{
   qsort (m_listTasks.Get(0), m_listTasks.Num(), sizeof(PCWorkTask), WorkSort);
}


/************************************************************************
BringUpToDate - Brings all the reoccurring tasks up to date, today.
*/
void CWork::BringUpToDate (DFDATE today)
{
   HANGFUNCIN;
   DWORD dwNum = m_listTasks.Num();
   DWORD i;
   PCWorkTask pTask;
   for (i = 0; i < dwNum; i++) {
      pTask = TaskGetByIndex (i);
      pTask->BringUpToDate (today);
   }

   Flush();
}


/*************************************************************************
DeleteAllChildren - Given a reoccurring task ID, this deletes all task
instances created by the reoccurring task.

inputs
   DWORD    dwID - ID
*/
void CWork::DeleteAllChildren (DWORD dwID)
{
   HANGFUNCIN;
   DWORD dwNum = m_listTasks.Num();
   DWORD i;
   PCWorkTask pTask;
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
WorkInit - Makes sure the Work object is filled. If not,
it's loaded in. This CAN be called multiple times.

This fills in:
   gfWorkPopulated - Set to TRUE when it's been loaded
   gCurWork - fills in

inputs
   none
returns
   BOOL - error
*/
BOOL WorkInit (void)
{
   HANGFUNCIN;
   if (gfWorkPopulated)
      return TRUE;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszWorkListNode, &dwNum);
   if (!pNode)
      return NULL;   // error. This shouldn't happen
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfWorkPopulated = TRUE;

   // pass this on
   return gCurWork.Init (dwNum);
}

/****************************************************************************
JournalControlGet - Gets the value from the journal control

inputs
   PCEscPage      pPage - page
   PCWorkTask     pTask - task
returns
   none
*/
static void JournalControlGet (PCEscPage pPage, PCWorkTask pTask)
{
   HANGFUNCIN;
   PCEscControl pControl;

   pControl = pPage->ControlFind (gszJournal);
   if (pControl) {
      // get the values
      int   iCurSel;
      iCurSel = pControl->AttribGetInt (gszCurSel);

      // find the node number for the person's data and the name
      pTask->m_iJournalID = (int) JournalIndexToDatabase ((DWORD)iCurSel);
   }
}

/****************************************************************************
JournalControlSet - Sets the control value based on the journal tag

inputs
   PCEscPage      pPage - page
   PCWorkTask     pTask - task
returns
   none
*/
static void JournalControlSet (PCEscPage pPage, PCWorkTask pTask)
{
   HANGFUNCIN;
   PCEscControl pControl;
   if (pTask->m_iJournalID != -1) {
      DWORD dw;
      dw = JournalDatabaseToIndex((DWORD)pTask->m_iJournalID);

      pControl = pPage->ControlFind (gszJournal);
      if (pControl)
         pControl->AttribSetInt (gszCurSel, (int) dw);
   }
}

/*****************************************************************************
WorkTaskAddPage - Override page callback.
*/
BOOL WorkTaskAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set the start date to today
         // BUGFIX - Set date to the day that the button in combo was pressed
         DateControlSet (pPage, gszDate, pPage->m_pUserData ? (DFDATE)pPage->m_pUserData : Today());
      }
      break;   // default

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[128];
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a name for the task.");
               return TRUE;
            }
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);


            // add it
            PCWorkTask pTask;
            pTask = gCurWork.TaskAdd (szName, szDescription, (DWORD)-1);
            if (!pTask) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // other data
            double   fWork;
            WCHAR szTemp[128];
            pControl = pPage->ControlFind (gszPriority);
            if (pControl)
               pTask->m_dwPriority = (DWORD)pControl->AttribGetInt (gszCurSel)+1;
            pTask->m_dwDateShow = DateControlGet (pPage, gszDate);
            pTask->m_dwCompleteBy = DateControlGet (pPage, gszCompleteBy);

            pControl = pPage->ControlFind (gszDaysOfWork);
            fWork = 1.0;
            szTemp[0]= 0 ;
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            AttribToDouble (szTemp, &fWork);
            pTask->m_dwEstimated = (DWORD) (fWork * 60);

            JournalControlGet (pPage, pTask);


            // exit this
            gCurWork.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
WorkTaskShowAddUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   DFDATE         dAddDate - Date to use. 0 => don't set
returns
   BOOL - TRUE if a project task was added
*/
BOOL WorkTaskShowAddUI (PCEscPage pPage, DFDATE dAddDate)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   WorkInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTASKADD, WorkTaskAddPage, (PVOID) dAddDate);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}


/*****************************************************************************
WorkActionItemAdd - Show the UI for adding an action item (really a task).

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   PWSTR - String description of what was added. This is really a pointer
   to gMemTemp.
*/
PWSTR WorkActionItemAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   WorkInit ();
   MemZero (&gMemTemp);

   // remember the next task
   DWORD dwNext;
   dwNext = gCurWork.m_dwNextTaskID;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMEETINGACTIONSELF, WorkTaskAddPage);

   // if the task ID has changed then we've added
   PCWorkTask pTask;
   if ((dwNext != gCurWork.m_dwNextTaskID) && (pTask = gCurWork.TaskGetByID(dwNext))) {
      // figure out the string for the action item
      MemCat (&gMemTemp, L"\r\n\r\nACTION ITEM FOR SELF:\r\n");
      MemCat (&gMemTemp, L"Name: ");
      MemCat (&gMemTemp, (PWSTR) pTask->m_memName.p);
      if (((PWSTR)pTask->m_memDescription.p)[0]) {
         MemCat (&gMemTemp, L"\r\nDescription: ");
         MemCat (&gMemTemp, (PWSTR) pTask->m_memDescription.p);
      }
      MemCat (&gMemTemp, L"\r\n\r\n");
   }

   return (PWSTR) gMemTemp.p;
}

/*****************************************************************************
WorkTaskAddReoccurPage - Override page callback.
*/
BOOL WorkTaskAddReoccurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
            // pressed add. Do some verification
            WCHAR szName[128];
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a name for the task.");
               return TRUE;
            }
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);


            // add it
            PCWorkTask pTask;
            pTask = gCurWork.TaskAdd (szName, szDescription, (DWORD)-1);
            if (!pTask) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // other data
            double   fWork;
            WCHAR szTemp[128];
            pControl = pPage->ControlFind (gszPriority);
            if (pControl)
               pTask->m_dwPriority = (DWORD)pControl->AttribGetInt (gszCurSel)+1;
            pTask->m_dwDateShow = 0;
            pControl = pPage->ControlFind (gszDaysOfWork);
            fWork = 1.0;
            szTemp[0]= 0 ;
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            AttribToDouble (szTemp, &fWork);
            pTask->m_dwEstimated = (DWORD) (fWork * 60);

            JournalControlGet (pPage, pTask);

            // fill in reoccur
            ReoccurFromControls (pPage, &pTask->m_reoccur);

            // bring it up to date
            pTask->BringUpToDate(Today());

            // exit this
            gCurWork.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
WorkTaskShowAddReoccurUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project task was added
*/
BOOL WorkTaskShowAddReoccurUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   WorkInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLTASKADDREOCCUR, WorkTaskAddReoccurPage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}


/*****************************************************************************
WorkSummary - Fill in the substitution for the task list.

inputs
   BOOL     fAll - If TRUE show all, FALSE show only active tasks
   DFDATE   date - date to view up until
   DFDATE   start - date to start from. 0 for all days to and including
         date. if date==start then only show reminders for that day.
   BOOL     fShowIfEmpty - If TRUE then show even if it's empty
retursn
   PWSTR - string from gMemTemp

BUGFIX - Put in start and fShowEmpty
*/
PWSTR WorkSummary (BOOL fAll, DFDATE date, DFDATE start, BOOL fShowIfEmpty)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are tasks to show
   WorkInit();
   // bring reoccurring tasks up to date - 1 day in advance so can use WorkSummary for Tomrrow page
   gCurWork.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24));
   gCurWork.Sort();  // make sure they're sorted
   DWORD i;
   PCWorkTask pTask;
   for (i = 0; i < gCurWork.m_listTasks.Num(); i++) {
      pTask = gCurWork.TaskGetByIndex(i);

      // if !fAll different
      if (!fAll) {
         if (pTask->m_reoccur.m_dwPeriod || (pTask->m_dwDateShow > date) || (pTask->m_dwDateShow < start))
            continue;
      }

      break;
   }
   if (!fShowIfEmpty && (i >= gCurWork.m_listTasks.Num()))
      return L""; // nothing

   // else, we have tasks. Show them off
   MemZero (&gMemTemp);
   if (fAll) {
      MemCat (&gMemTemp, L"<xbr/>");
      if (gfMicroHelp) {
         MemCat (&gMemTemp, fAll ?
            L"<xSectionTitle>Tasks</xSectionTitle>" :
            L"<xSectionTitle>Task Summary</xSectionTitle>");
      }
      // MemCat (&gMemTemp, L"<p>This shows the list of project tasks you're scheduled to work on this week.</p>");
   }
   MemCat (&gMemTemp, L"<xlisttable innerlines=0>");
   if (!fAll) {
      // BUGFIX - User request for add XXX button in today, tomorrow, etc. page
      if (!gfPrinting) {
         MemCat (&gMemTemp, L"<xtrheader align=left>");
         MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddtask");
         if (date)
            MemCat (&gMemTemp, (int) date);
         MemCat (&gMemTemp, L"><small><font color=#ffffff>Add task</font></small></button>");
         MemCat (&gMemTemp, L"<align align=right><a href=r:115 color=#c0c0ff>Tasks</a></align></xtrheader>");
      }
      else {
         MemCat (&gMemTemp, L"<xtrheader align=right>");
         MemCat (&gMemTemp, L"<a href=r:115 color=#c0c0ff>Tasks</a></xtrheader>");
      }
      //MemCat (&gMemTemp, L"<xtrheader><a href=r:115 color=#c0c0ff>Tasks</a></xtrheader>");
   }


   DWORD dwLastPriority;
   DWORD dwLastPeriod;
   BOOL fFound;
   fFound = FALSE;
   dwLastPriority = 0;
   dwLastPeriod = 0; // so know if just changed to show reoccurring tasks
   for (i = 0; i < gCurWork.m_listTasks.Num(); i++) {
      pTask = gCurWork.TaskGetByIndex(i);

      // filter out task if !fAll
      if (!fAll) {
         if (pTask->m_reoccur.m_dwPeriod || (pTask->m_dwDateShow > date) || (pTask->m_dwDateShow < start))
            continue;
      }

      // if different priority then show
      DWORD dwPri;
      dwPri = pTask->Priority();
      if (dwPri != dwLastPriority) {
         if (dwLastPriority)
            MemCat (&gMemTemp, L"</xtrmonth>");
         dwLastPriority = dwPri;
         dwLastPeriod = pTask->m_reoccur.m_dwPeriod;

         MemCat (&gMemTemp, L"<xtrmonth><tr><td align=right><big>");
         if (gfFullColor)
            MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#20a020 bcolor=#60d060/>");
         MemCat (&gMemTemp, L"<bold>Priority ");
         MemCat (&gMemTemp, (int) dwPri);
         MemCat (&gMemTemp, L"</bold><italic> (");

         PWSTR psz;
         switch (dwPri) {
         case 1:
            psz = L"Very important";
            break;
         case 2:
            psz = L"Important";
            break;
         case 3:
            psz = L"Normal";
            break;
         case 4:
            psz = L"Low";
            break;
         default:
            psz = L"Very low";
            break;
         }
         MemCat (&gMemTemp, psz);

         MemCat (&gMemTemp, L")</italic></big></td></tr>");
      }

      // consider putting in a spacer between normal tasks and
      // reoccurring tasks
      if ((!dwLastPeriod) != (!pTask->m_reoccur.m_dwPeriod)) {
         // separator
         MemCat (&gMemTemp, L"<tr><td><br/></td></tr>");
      }
      dwLastPeriod = pTask->m_reoccur.m_dwPeriod;

      // remeber that found at least one task
      fFound = TRUE;

      // display task info
      MemCat (&gMemTemp, L"<tr>");
      if (gfPrinting) {
         MemCat (&gMemTemp, L"<xtdtasknolink>");
      }
      else {
         MemCat (&gMemTemp, L"<xtdtask href=tt:");
         MemCat (&gMemTemp, (int) pTask->m_dwID);
         MemCat (&gMemTemp, L">");
      }
      // BUGFIX - Crash unless as sanitized
      MemCatSanitize (&gMemTemp, (PWSTR) pTask->m_memName.p);
      
      WCHAR szTemp[128];
      if (gfPrinting) {
         MemCat (&gMemTemp, L"</xtdtasknolink>");
      }
      else {
         // hover help
         MemCat (&gMemTemp, L"<xHoverHelp><align parlinespacing=0>");
         if (((PWSTR) pTask->m_memDescription.p)[0]) {
            MemCatSanitize (&gMemTemp, (PWSTR) pTask->m_memDescription.p);
            MemCat (&gMemTemp, L"<p/>");
         }
         swprintf (szTemp, L"%g hours of work.", (double)pTask->m_dwEstimated / 60.0);
         MemCat (&gMemTemp, szTemp);

         // show due date if it has one
         if (pTask->m_dwCompleteBy) {
            MemCat (&gMemTemp, L"<br/>Task must be completed by ");
            DFDATEToString (pTask->m_dwCompleteBy, szTemp);
            MemCat (&gMemTemp, szTemp);
         }

         MemCat (&gMemTemp, L"</align></xHoverHelp>");

         // completed
         MemCat (&gMemTemp, L"</xtdtask>");
      }

      // show the mount of work
      MemCat (&gMemTemp, L"<xtdcompleted>");
      swprintf (szTemp, L"%g hours", (double)pTask->m_dwEstimated / 60.0);
      MemCat (&gMemTemp, szTemp);
      MemCat (&gMemTemp, L"</xtdcompleted>");

      // last category
      MemCat (&gMemTemp, L"<xtdcompleted>");
      if (fAll) {
         // show start date
         if (pTask->m_reoccur.m_dwPeriod) {
            MemCat (&gMemTemp, L"<italic>");
            ReoccurToString (&pTask->m_reoccur, szTemp);
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</italic>");
         }
         else {
            DFDATEToString (pTask->m_dwDateShow, szTemp);
            MemCat (&gMemTemp, szTemp);
         }
      }
      else {
         // show completion date
         if (pTask->m_dwCompleteBy) {
            DFDATEToString (pTask->m_dwCompleteBy, szTemp);
            MemCat (&gMemTemp, szTemp);
         }
         else
            // no completion date stored so just show standard
            MemCat (&gMemTemp, L"<italic>No completion date specified</italic>");
      }
      MemCat (&gMemTemp, L"</xtdcompleted></tr>");
   }

   // finish off list
   if (dwLastPriority)
      MemCat (&gMemTemp, L"</xtrmonth>");

   // if found nothing then at least say it's empty
   if (!fFound) {
      MemCat (&gMemTemp, L"<tr><td><italic>No tasks.</italic></td></tr>");
   }


   MemCat (&gMemTemp, L"</xlisttable>");
   return (PWSTR) gMemTemp.p;
}

   
/*****************************************************************************
WorkTasksPage - Override page callback.
*/
BOOL WorkTasksPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // make sure initalized
      WorkInit ();
      break;   // fall through

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only care about projectlist
         if (!_wcsicmp(p->pszSubName, L"TASKS")) {
            p->pszSubString = WorkSummary (TRUE, 0);
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

         if (!_wcsicmp(p->pControl->m_pszName, L"addtask")) {
            if (WorkTaskShowAddUI(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addreoccur")) {
            if (WorkTaskShowAddReoccurUI(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         BOOL fRefresh;
         if (WorkParseLink (p->psz, pPage, &fRefresh, TRUE)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}




/*****************************************************************************
MarkTaskCompleted - Mark task as completed.

inputs
   PCEscPage      pPage - for messages
   PCWorkTask     pTask - Task
   DWORD          dwSplit - Split to mark. -1 if whole task
returns
   BOOL - TRUE if shoudl refresh display
*/
BOOL MarkTaskCompleted (PCEscPage pPage, PCWorkTask pTask, DWORD dwSplit)
{
   HANGFUNCIN;
   // BUGFIX - If there's only one split left then do entire
   if (pTask->m_PlanTask.Num() <= 1)
      dwSplit = (DWORD)-1;

   // BUGFIX - If only a part of a split it fixed then use the time specified
   // in the split
   double   fInitial;
   fInitial = pTask->m_dwEstimated / 60.0;
   if (pTask->m_PlanTask.Num()) {
      PPLANTASKITEM pi = pTask->m_PlanTask.Get(dwSplit);
      if (pi)
         fInitial = pi->dwTimeAllotted / 60.0;
      else
         fInitial = ((double)pTask->m_dwEstimated - (double)pTask->m_PlanTask.m_dwTimeCompleted) / 60.0;
   }
   fInitial = max(fInitial, 0);

   // BUGFIX - ask how long it took
   double   fHowLong = -1;
   if (pTask->m_iJournalID != -1) {
      fHowLong = HowLong (pPage, fInitial);
      if (fHowLong < 0)
         return TRUE;   // pressed cancel
   }

   // tell user
   EscChime (ESCCHIME_INFORMATION);
   EscSpeak (L"Task removed.");

   // get the category so changes registered
   JournalControlGet (pPage, pTask);

   // if the split is valid then remove that, else entire task
   BOOL  fRemoveSplit;
   fRemoveSplit = (dwSplit < pTask->m_PlanTask.Num());
   // find it
   if (fRemoveSplit) {
      PPLANTASKITEM pp = pTask->m_PlanTask.Get(dwSplit);
      pTask->m_PlanTask.m_dwTimeCompleted += pp->dwTimeAllotted;
      pTask->m_PlanTask.Remove(dwSplit);
   }
   else {
      DWORD dwIndex;
      dwIndex = gCurWork.TaskIndexByID (pTask->m_dwID);
      gCurWork.m_listTasks.Remove(dwIndex);
   }

   // note in the daily log that resolved
   WCHAR szHuge[10000];
   wcscpy (szHuge, fRemoveSplit ? L"Task worked on: " : L"Task completed: ");
   wcscat (szHuge, (PWSTR)pTask->m_memName.p);
   if (pTask->m_memDescription.p && ((PWSTR)pTask->m_memDescription.p)[0]) {
      wcscat (szHuge, L"\r\n");
      wcscat (szHuge, (PWSTR)pTask->m_memDescription.p);
   }
   DFDATE date;
   DFTIME start, end;
   CalendarLogAdd (date = Today(), start = Now(), end = -1, szHuge);

   // log the task completed
   if (pTask->m_iJournalID != -1)
      JournalLink ((DWORD)pTask->m_iJournalID, szHuge, (DWORD)-1, date, start, end, fHowLong);


   // delete it
   if (!fRemoveSplit)
      delete pTask;
   gCurWork.m_fDirty = TRUE;
   gCurWork.Flush();
   pPage->Exit (gszOK);

   return TRUE;
}

/*****************************************************************************
WorkTaskEditPage - Override page callback.
*/
BOOL WorkTaskEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCWorkTask pTask = (PCWorkTask) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"IFREOCCUR")) {
            p->pszSubString = pTask->m_dwParent ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFREOCCUR")) {
            p->pszSubString = pTask->m_dwParent ? L"" : L"</comment>";
            return TRUE;
         }
      }
      break;   // default

   case ESCM_INITPAGE:
      {
         // set up values
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind(gszName);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pTask->m_memName.p);
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pTask->m_memDescription.p);


         // other data
         WCHAR szTemp[128];
         pControl = pPage->ControlFind (gszPriority);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) pTask->m_dwPriority - 1);
         DateControlSet (pPage, gszDate, pTask->m_dwDateShow);
         DateControlSet (pPage, gszCompleteBy, pTask->m_dwCompleteBy);
         pControl = pPage->ControlFind (gszDaysOfWork);
         swprintf (szTemp, L"%g", (double)pTask->m_dwEstimated / 60.0);
         if (pControl)
            pControl->AttribSet (gszText, szTemp);

         JournalControlSet (pPage, pTask);
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
            WCHAR szName[128];
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a name for the task.");
               return TRUE;
            }
            MemZero (&pTask->m_memName);
            MemCat (&pTask->m_memName, szName);

            // set values
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pTask->m_memDescription);
            MemCat (&pTask->m_memDescription, szDescription);

            // other data
            double   fWork;
            WCHAR szTemp[128];
            pControl = pPage->ControlFind (gszPriority);
            if (pControl)
               pTask->m_dwPriority = (DWORD)pControl->AttribGetInt (gszCurSel)+1;
            pTask->m_dwDateShow = DateControlGet (pPage, gszDate);
            pTask->m_dwCompleteBy = DateControlGet (pPage, gszCompleteBy);
            // loop through all the planned work. If planned work occurs before m_dwDateShow
            // then delete it
            DWORD i;
            for (i = 0; i < pTask->m_PlanTask.Num(); ) {
               PPLANTASKITEM pp = pTask->m_PlanTask.Get(i);
               if (pp->date < pTask->m_dwDateShow) {
                  pTask->m_PlanTask.Remove(i);
               }
               else
                  i++;
            }
            pControl = pPage->ControlFind (gszDaysOfWork);
            fWork = 1.0;
            szTemp[0]= 0 ;
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            AttribToDouble (szTemp, &fWork);
            pTask->m_dwEstimated = (DWORD) (fWork * 60);

            JournalControlGet (pPage, pTask);

            // set dirty
            pTask->SetDirty();
            gCurWork.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszCompleted)) {
            return MarkTaskCompleted(pPage, pTask, (DWORD) -1);
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Task removed.");

            // get the category so changes registered
            JournalControlGet (pPage, pTask);

            // find it
            DWORD dwIndex;
            dwIndex = gCurWork.TaskIndexByID (pTask->m_dwID);
            gCurWork.m_listTasks.Remove(dwIndex);

            // delete it
            delete pTask;
            gCurWork.m_fDirty = TRUE;
            gCurWork.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
WorkTaskEditReoccurPage - Override page callback.
*/
BOOL WorkTaskEditReoccurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCWorkTask pTask = (PCWorkTask) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set up values
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind(gszName);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pTask->m_memName.p);
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pTask->m_memDescription.p);


         // other data
         WCHAR szTemp[128];
         pControl = pPage->ControlFind (gszPriority);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) pTask->m_dwPriority - 1);
         pControl = pPage->ControlFind (gszDaysOfWork);
         swprintf (szTemp, L"%g", (double)pTask->m_dwEstimated / 60.0);
         if (pControl)
            pControl->AttribSet (gszText, szTemp);

         // fill in reoccur
         ReoccurToControls (pPage, &pTask->m_reoccur);

         JournalControlSet (pPage, pTask);

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
            WCHAR szName[128];
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a name for the task.");
               return TRUE;
            }
            MemZero (&pTask->m_memName);
            MemCat (&pTask->m_memName, szName);

            // set values
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pTask->m_memDescription);
            MemCat (&pTask->m_memDescription, szDescription);

            // other data
            double   fWork;
            WCHAR szTemp[128];
            pControl = pPage->ControlFind (gszPriority);
            if (pControl)
               pTask->m_dwPriority = (DWORD)pControl->AttribGetInt (gszCurSel)+1;
            pControl = pPage->ControlFind (gszDaysOfWork);
            fWork = 1.0;
            szTemp[0]= 0 ;
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            AttribToDouble (szTemp, &fWork);
            pTask->m_dwEstimated = (DWORD) (fWork * 60);

            JournalControlGet (pPage, pTask);

            // BUGFIX - loop throught all existing instances that are children and modify
            DWORD i;
            for (i = 0; i < gCurWork.m_listTasks.Num(); i++) {
               PCWorkTask pChild = gCurWork.TaskGetByIndex(i);
               if (!pChild)
                  continue;
               if (pChild->m_dwParent != pTask->m_dwID)
                  continue;

               // found, so change
               MemZero (&pChild->m_memName);
               MemCat (&pChild->m_memName, (PWSTR) pTask->m_memName.p);
               MemZero (&pChild->m_memDescription);
               MemCat (&pChild->m_memDescription, (PWSTR) pTask->m_memDescription.p);
               pChild->m_dwEstimated = pTask->m_dwEstimated;
               pChild->m_dwPriority = pTask->m_dwPriority;
               pChild->m_iJournalID = pTask->m_iJournalID;
               pChild->SetDirty();

            }

            // fill in reoccur
            REOCCURANCE rOld;
            rOld = pTask->m_reoccur;

            ReoccurFromControls (pPage, &pTask->m_reoccur);

            // BUGFIX - if this has changed ask the user if they want to reschedule
            // all existing occurances
            if (memcmp(&rOld, &pTask->m_reoccur, sizeof(rOld))) {
               pPage->MBSpeakInformation (L"Any existing task instances have been rescheduled.",
                  L"Because you have changed how often the task occurs any tasks already "
                  L"generated by this reoccurring task have been rescheduled.");
               gCurWork.DeleteAllChildren (pTask->m_dwID);
               pTask->m_reoccur.m_dwLastDate = 0;
            }

            // bring it up to date
            pTask->BringUpToDate(Today());

            // set dirty
            pTask->SetDirty();
            gCurWork.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            int iRet;
            iRet = pPage->MBYesNo (L"Do you want to delete all instances of the task?",
               L"Because this is a reoccurring task it may have generated instances of the "
               L"task occurring on specific days. If you press 'Yes' you will delete them too. "
               L"'No' will leave them on your task list.", TRUE);
            if (iRet == IDCANCEL)
               return TRUE;   // exit

            // if delete children do so
            if (iRet == IDYES)
               gCurWork.DeleteAllChildren (pTask->m_dwID);

            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Reoccurring task deleted.");

            // delete it
            DWORD dwIndex;
            dwIndex = gCurWork.TaskIndexByID (pTask->m_dwID);
            gCurWork.m_listTasks.Remove(dwIndex);
            delete pTask;
            gCurWork.m_fDirty = TRUE;
            gCurWork.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
WorkParseLink - If a page uses a work substitution, which generates
"tt:XXXX" to indicate that a work link was called, call this. If it's
not a work link, returns FALSE. If it is, it deletes the work and
returns TRUE. The page should then refresh itself.

It also looks for a second colon to indicate a task split.

inputs
   PWSTR    pszLink - link
   PCEscPage pPage - page
   BOOL     *pfRefresh - set to TRUE if should refresh
   BOOL     fJumpToEdit - If set to TRUE jumps to edit. Else, asks if completed first.
returns
   BOOL
*/
BOOL WorkParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, BOOL fJumpToEdit)
{
   HANGFUNCIN;
   // make sure its the right type
   if ((pszLink[0] != L't') || (pszLink[1] != L't') || (pszLink[2] != L':'))
      return FALSE;

   // make sure have work loaded
   WorkInit ();

   // get the task
   PCWorkTask pTask;
   DWORD dwID;
   dwID = 0;
   AttribToDecimal (pszLink + 3, (int*) &dwID);

   // see if has colon for another split
   DWORD dwSplit;
   PWSTR pszColon;
   pszColon = wcschr(pszLink+3, L':');
   dwSplit = pszColon ? _wtoi(pszColon+1) : (DWORD)-1;

   pTask = gCurWork.TaskGetByID (dwID);


   // if there's a colon it means we're in the planner page. Once there,
   // users that click on a task get the quesiton if they've completed it.
   if (!fJumpToEdit) {
      int iRet;
      iRet = pPage->MBYesNo(
         (!pszColon || (pTask->m_PlanTask.Num() <= 1)) ?
            L"Have you completed the entire task?" :
            L"Have you completed THIS PORTION of the task?",
         (!pszColon || (pTask->m_PlanTask.Num() <= 1)) ?
            L"Pressing 'Yes' will mark the task as completed. 'No' will edit the task." :
            L"Pressing 'Yes' will mark the portion as completed. 'No' will edit the task.",
         TRUE);
      if (iRet == IDCANCEL)
         return TRUE;
      if (iRet == IDYES) {
         *pfRefresh = MarkTaskCompleted(pPage, pTask, dwSplit);
         return TRUE;
      }
      // else, go on to editing
   }

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, 
      pTask->m_reoccur.m_dwPeriod ? IDR_MMLTASKEDITREOCCUR : IDR_MMLTASKEDIT,
      pTask->m_reoccur.m_dwPeriod ? WorkTaskEditReoccurPage : WorkTaskEditPage,
      pTask);

   *pfRefresh = (pszRet && !_wcsicmp(pszRet, gszOK));
   return TRUE;
}


/***************************************************************************
TaskBirthday - Called by People.cpp to handle reoccurring birthdays.

inputs
   DWORD       dwNode - Node for the person. Or with 0x80000000 to get an ID for the bday.
   PWSTR       pszFirst - Person's first name
   PWSTR       pszLast - Person's last name
   DFDATE      bday - Person's birthday.
   BOOL        fEnable - If TRUE then make sure birthday is there, FALSE then make sure it's gone
returns
   none
*/
void TaskBirthday (DWORD dwNode, PWSTR pszFirst, PWSTR pszLast,
                   DFDATE bday, BOOL fEnable)
{
   HANGFUNCIN;
   // if the birthday == 0 then automatically set fenable to false
   if (bday == 0)
      fEnable = FALSE;

   // init
   WorkInit();

   // figure out what the ID should be and see if can find it
   dwNode |= 0x80000000;
   DWORD dwIndex;
   PCWorkTask pTask;
   dwIndex = gCurWork.TaskIndexByID (dwNode);

   // if it's not supposed to exist do that now
   if (!fEnable) {
      if (dwIndex == (DWORD)-1)
         return;  // doesn't exist. No problem

      // else, must delete
      pTask = gCurWork.TaskGetByIndex (dwIndex);
      if (pTask)
         delete pTask;
      gCurWork.m_listTasks.Remove(dwIndex);
      gCurWork.m_fDirty = TRUE;
      gCurWork.Flush();

      // done
      return;
   }

   // else it should exist

   // if it already exists then consider modifying
   if (dwIndex != (DWORD)-1)
      pTask = gCurWork.TaskGetByIndex (dwIndex);
   else {
      // create it
      pTask = gCurWork.TaskAdd (L"", L"", (DWORD)-1);
      if (!pTask)
         return;
   }
   WCHAR    szName[512];
   if (pszFirst) {
      if (pszLast)
         swprintf (szName, L"%s %s", pszFirst, pszLast);
      else
         wcscpy (szName, pszFirst);
   }
   else {
      wcscpy (szName, pszLast);
   }
   swprintf (szName + wcslen(szName), L"'s birthday is on %d %s.",
      DAYFROMDFDATE(bday), gaszMonth[MONTHFROMDFDATE(bday)-1]);

   DFDATE week;
   week = MinutesToDFDATE (DFDATEToMinutes(bday)-24*60*7);
   
   // only if it changes
   if ((pTask->m_reoccur.m_dwParam1 != MONTHFROMDFDATE (week)) ||
      (pTask->m_reoccur.m_dwParam2 != DAYFROMDFDATE (week)) ||
      (pTask->m_reoccur.m_dwPeriod != REOCCUR_YEARDAY) ) {

      MemZero (&pTask->m_memName);
      MemCat (&pTask->m_memName, szName);
      pTask->m_dwEstimated = 0;
      pTask->m_dwID = dwNode;
      pTask->m_dwPriority = 3;
      pTask->m_reoccur.m_dwPeriod = REOCCUR_YEARDAY;
      pTask->m_reoccur.m_dwParam1 = MONTHFROMDFDATE (week);
      pTask->m_reoccur.m_dwParam2 = DAYFROMDFDATE (week);
      pTask->m_reoccur.m_dwLastDate = Today();
   }

   pTask->SetDirty();
   gCurWork.Flush();

   // done
}

/*************************************************************************
WorkMonthEnumerate - Enumerates a months worth of data.

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
void WorkMonthEnumerate (DFDATE date, PCMem *papMem, DFDATE bunchup, BOOL fWithLinks)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are tasks to show
   WorkInit();
   // bring reoccurring tasks up to date - 1 month up to date so see
   // for the whole month
   gCurWork.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24*30));
   gCurWork.Sort();  // make sure they're sorted
   DWORD i;
   PCWorkTask pTask;

   for (i = 0; i < gCurWork.m_listTasks.Num(); i++) {
      pTask = gCurWork.TaskGetByIndex(i);

      // filter out reoccuring
      // BUGFIX - If task was assigned to the future and change the date to "immediately"
      // it wouldn't show up on today list becayse pTask->m_dwDateShow = 0
      if (pTask->m_reoccur.m_dwPeriod)
         continue;

      DFDATE bunch;
      bunch = pTask->m_dwDateShow;
      if (bunch < bunchup)
         bunch = bunchup;

      // filter out non-month
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
         MemCat (papMem[iDay], L"<a href=tt:");
         MemCat (papMem[iDay], (int) pTask->m_dwID);
         MemCat (papMem[iDay], L">");
      }
      MemCatSanitize (papMem[iDay], (PWSTR) pTask->m_memName.p);
      if (fWithLinks)
         MemCat (papMem[iDay], L"</a>");
      MemCat (papMem[iDay], L" - Priority ");
      MemCat (papMem[iDay], (int)pTask->m_dwPriority);
      WCHAR szTemp[128];
      swprintf (szTemp, L", %g hours of work", (double)pTask->m_dwEstimated / 60.0);
      MemCat (papMem[iDay], szTemp);
      MemCat (papMem[iDay], L"</li>");

      if (((PWSTR) pTask->m_memDescription.p)[0]) {
         MemCat (papMem[iDay], L"<p>Description: ");
         MemCatSanitize (papMem[iDay], (PWSTR) pTask->m_memDescription.p);
         MemCat (papMem[iDay], L"</p>");
      }

      // show due date if it has one
      if (pTask->m_dwCompleteBy) {
         MemCat (papMem[iDay], L"<p>Task must be completed by <bold>");
         DFDATEToString (pTask->m_dwCompleteBy, szTemp);
         MemCat (papMem[iDay], szTemp);
         MemCat (papMem[iDay], L"</bold></p>");
      }
   }

}

/*******************************************************************************
WorkEnumForPlanner - Enumerates the meetings in a format the planner can use.

inputs
   DFDATE      date - start date
   DWORD       dwDays - # of days to generate
   PCListFixed palDays - Pointer to an array of CListFixed. Number of elements
               is dwDays. These will be concatenated with relevent information.
               Elements in the list are PLANNERITEM strctures.
returns
   none
*/
void WorkEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   WorkInit();
   // bring reoccurring Meetings up to date. 1 day in advance so can use WorkSummary for tomrrow page
   gCurWork.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(date)+(60*24)*(dwDays+1)));
   gCurWork.Sort();  // make sure they're sorted

   DWORD i;
   PCWorkTask pTask;

   // figure out when the upcoming days are
#define  MAXDAYS  28
   DFDATE   adDay[MAXDAYS], dMax;
   dwDays = min(dwDays,MAXDAYS);
   for (i = 0; i < dwDays; i++)
      dMax = adDay[i] = MinutesToDFDATE(DFDATEToMinutes(date)+(60*24)*i);


   // find all the Tasks and Work Tasksfor the date
   PLANNERITEM pi;
   for (i = 0; i < gCurWork.m_listTasks.Num(); i++) {
      pTask = gCurWork.TaskGetByIndex(i);

      // filter out reoccuring
      if (pTask->m_reoccur.m_dwPeriod)
         continue;

      // if the task occurs after dMax and there's no PlanTask yet then ignore
      if ((pTask->m_dwDateShow > dMax) && !pTask->m_PlanTask.Num())
         continue;

      // else, update the plan task
      gCurWork.m_fDirty |= pTask->m_PlanTask.Verify (pTask->m_dwEstimated, today,
         (pTask->m_dwDateShow < today) ? today : pTask->m_dwDateShow,
         pTask->m_dwPriority * 100);

      // loop through all the plans
      DWORD j;
      DFDATE      dfPrevious; // when previous task item happened
      dfPrevious = pTask->m_PlanTask.m_dwTimeCompleted ? (DWORD)-1 : 0;
      for (j = 0; j < pTask->m_PlanTask.Num(); j++) {
         PPLANTASKITEM pt = pTask->m_PlanTask.Get(j);
         if ((pt->date < date) || (pt->date > dMax))
            continue;   // not appearing here

         // figure out which date this is on
         DWORD d;
         for (d = 0; d < dwDays; d++)
            if (adDay[d] == pt->date)
               break;
         if (d >= dwDays)
            continue;

         // and the time
         // remember this
         memset (&pi, 0, sizeof(pi));
         pi.fFixedTime = FALSE;
         pi.dwTime = pt->start;
         pi.dwDuration = pt->dwTimeAllotted;
         pi.dwPriority = pt->dwPriority;
         pi.dwMajor = pTask->m_dwID;
         pi.dwSplit = j;
         pi.dwType = 2;

         // BUGFIX - Provide a pointer to the next item
         if ((j+1) < pTask->m_PlanTask.Num()) {
            PPLANTASKITEM pt2 = pTask->m_PlanTask.Get(j+1);
            pi.dwNextDate = pt2->date;
         }
         else {
            pi.dwNextDate = 0;
         }
         // note when the previous instance occurred
         pi.dwPreviousDate = dfPrevious;
         dfPrevious = pt->date;

         pi.pMemMML = new CMem;
         PCMem pMem = pi.pMemMML;

         // MML
         MemZero (pMem);
         if (!gfPrinting) {
            MemCat (pMem, L"<a href=tt:");
            MemCat (pMem, (int) pTask->m_dwID);
            MemCat (pMem, L":");
            MemCat (pMem, (int) j);
            MemCat (pMem, L">");
         }
         MemCatSanitize (pMem, (PWSTR) pTask->m_memName.p);
         if (!gfPrinting)
            MemCat (pMem, L"</a>");
         MemCat (pMem, L" <italic>(Task)</italic>");

         MemCat (pMem, L" - Priority ");
         MemCat (pMem, (int)pTask->m_dwPriority);

         if (((PWSTR) pTask->m_memDescription.p)[0]) {
            MemCat (pMem, L"<br/>");
            MemCat (pMem, L"Description: ");
            MemCatSanitize (pMem, (PWSTR) pTask->m_memDescription.p);
         }

         // show due date if it has one
         if (pTask->m_dwCompleteBy) {
            MemCat (pMem, L"<br/>");
            MemCat (pMem, L"Task must be completed by <bold>");
            WCHAR szTemp[64];
            DFDATEToString (pTask->m_dwCompleteBy, szTemp);
            MemCat (pMem, szTemp);
            MemCat (pMem, L"</bold>");
         }

         
         // add it
         palDays[d].Add(&pi);
      }

      // done with all plans

   }
   gCurWork.Flush();

}


/*******************************************************************************
WorkAdjustPriority - Adjusts the priority of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DWORD       dwPriority - New priority
returns
   none
*/
void WorkAdjustPriority (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwPriority)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   WorkInit();

   PCWorkTask pTask;
   pTask = gCurWork.TaskGetByID (dwMajor);
   if (!pTask)
      return;
   PPLANTASKITEM pi;
   pi = pTask->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->dwPriority = dwPriority;

   gCurWork.m_fDirty = TRUE;
   gCurWork.Flush();
}

/*******************************************************************************
WorkChangeDate - Adjusts the date of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DFDATE      date - new date
returns
   none
*/
void WorkChangeDate (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFDATE date)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   WorkInit();

   PCWorkTask pTask;
   pTask = gCurWork.TaskGetByID (dwMajor);
   if (!pTask)
      return;
   PPLANTASKITEM pi;
   pi = pTask->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->date = date;

   // adjust the date too. Make sure that acceptable start date begins with this
   if (pTask->m_dwDateShow)
      pTask->m_dwDateShow = min(pTask->m_dwDateShow, date);

   gCurWork.m_fDirty = TRUE;
   gCurWork.Flush();
}

/*******************************************************************************
WorkChangeTime - Adjusts the date of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DFTIME      time -new time
returns
   none
*/
void WorkChangeTime (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFTIME time)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   WorkInit();

   PCWorkTask pTask;
   pTask = gCurWork.TaskGetByID (dwMajor);
   if (!pTask)
      return;
   PPLANTASKITEM pi;
   pi = pTask->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->start = time;

   gCurWork.m_fDirty = TRUE;
   gCurWork.Flush();
}


/*******************************************************************************
WorkSplitPlan - Split a work item into two sections

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DWORD       dwMin - Split this many minutes down
returns
   none
*/
void WorkSplitPlan (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwMin)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   WorkInit();

   PCWorkTask pTask;
   pTask = gCurWork.TaskGetByID (dwMajor);
   if (!pTask)
      return;
   PPLANTASKITEM pi;
   pi = pTask->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   if (!dwMin || (dwMin >= pi->dwTimeAllotted))
      return;


   // clone this
   PLANTASKITEM it;
   memcpy (&it, pi, sizeof(it));
   it.dwPriority++;  // so it occurs one after
   it.dwTimeAllotted = pi->dwTimeAllotted - dwMin;

   pi->dwTimeAllotted = dwMin;

   // add it
   pTask->m_PlanTask.Add (&it);

   gCurWork.m_fDirty = TRUE;
   gCurWork.Flush();
}