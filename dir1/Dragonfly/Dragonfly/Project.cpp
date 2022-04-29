/*************************************************************************
Project.cpp - For project maintinence.

begun 8/3/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

typedef struct {
   DWORD       dwNode;        // person node
   DWORD       dwDays;        // # of days a week they work
} DAYSPERPERSON, *PDAYSPERPERSON;

/* C++ objects */
class CProjectTask;
typedef CProjectTask * PCProjectTask;

// project
class CProject {
public:
   CProject (void);
   ~CProject (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCProjectTask TaskAdd (PWSTR pszName, PWSTR pszSubProject, DWORD dwDuration, DWORD dwBefore);
   PCProjectTask TaskGetByIndex (DWORD dwIndex);
   PCProjectTask TaskGetByID (DWORD dwID);
   DWORD TaskIndexByID (DWORD dwID);
   void GeneratedFilteredList (void);
   void CalculateSchedule (void);

   CListFixed     m_listTasks;   // list of PCProjectTasks, ordered according order of doing
   CListFixed     m_listTasksCompleted;   // list of completed tasks

   CListVariable  m_listFilteredTasks; // task list for filtered list
   CListVariable  m_listFilteredSubProjects; // subprojects list for filtered list

   CListFixed     m_listDaysPerPerson; // how many days per week other people work

   BOOL           m_fDirty;      // set to TRUE if the project is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   CMem           m_memName;     // contains Unicode string with project name
   CMem           m_memDescription; // contains unicode string with description
   DWORD          m_dwDaysPerWeek;  // number of days per week to work on, from 1..7
   int            m_iJournalID;     // journal category ID to link to
   DWORD          m_dwNextTaskID;   // next task ID created
   int            m_fDisabled;      // if this is true then don't show project on to-do lists

private:

};

typedef CProject * PCProject;


// task
class CProjectTask {
public:
   CProjectTask (void);
   ~CProjectTask (void);

   PCProject      m_pProject; // project that this belongs to. If change sets dirty
   CMem           m_memName;  // name of the task
   CMem           m_memDescription; // description of the task
   CMem           m_memSubProject;  // subproject string
   DWORD          m_dwAssignedTo;   // 0 = self, +=person
   DWORD          m_dwDuration;  // amount of work (in 100th of a day)
   DWORD          m_dwID;     // unique ID for the task.
   DFDATE         m_dwDateCompleted;   // date that completed work. 0 if not yet done
   DFDATE         m_dwDateBefore;   // must occur before this date. 0 if NA
   DFDATE         m_dwDateAfter;    // must occur after this date. 0 if NA
   DWORD          m_dwIDBefore;     // must occur m_dwDaysBefore this task. 0 if NA
   DWORD          m_dwIDAfter;      // must occur m_dwDaysAfter this task. 0 if NA
   DWORD          m_dwDaysBefore;   // occur after this
   DWORD          m_dwDaysAfter;    // occur after this

   DFDATE         m_dwDatePredicted;   // date that's predicted to be finished on
   DFDATE         m_dwDateStarted;     // when predict that will actual start
   DWORD          m_dwConflict;        // # indicating conflict. 0 if no conflict

   CPlanTask      m_PlanTask;

#define CONFLICT_DATEBEFORE         0x0001   // breaking the m_dwDateBefore rule
#define CONFLICT_DATEAFTER          0x0002   // breaking the m_dwDateAfter rule
#define CONFLICT_IDBEFORE           0x0004   // breaking m_dwIDBefore rule
#define CONFLICT_IDAFTER            0x0008   // breaking m_dwIDAfter rule

   BOOL           m_dwDeadTimeBefore;   // number of days of dead time before this before of m_dwDateBefore
   __int64        m_iMinuteMinStart;   // if there's a previous task that must occur N days before this
                                       // one, this value will be non-zero, indicating the soonest that
                                       // this can start
   DWORD          m_dwOldOrder;        // order it was in before. Used to resort tasks after calculate start times

   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);

private:
};

/* globals */
BOOL           gfProjectPopulated = FALSE;// set to true if glistProject is populated
CListVariable  glistProject;   // list of project's names
CListFixed     glistProjectID; // database ID/location for each project so named in glistProject
CProject       gCurProject;      // current project that working with
DWORD          gdwProjectView = -1; // -1 for viewing everyone, 0 tasks only assigned to self, + for person number
static DWORD   gdwMoveMode = 0;  // 0 for no move mode, 1 if expecting to click on task, 2 if expect to click on where to move
static DWORD   gdwMoveTaskIndex; // task to move

WCHAR gszProjectRemove[] = L"projecttoremove";
WCHAR gszTaskCompleted[] = L"taskcompleted";
static WCHAR gszAssignedTo[] = L"assignedto";
static WCHAR gszDaysPerPerson[] = L"daysperperson";
static WCHAR gszDisabled[] = L"disabled";

/*************************************************************************
CProjectTask::constructor and destructor
*/
CProjectTask::CProjectTask (void)
{
   HANGFUNCIN;
   m_pProject = NULL;
   m_dwDuration = 0;
   m_dwID = 0;
   m_dwDateCompleted = 0;
   m_dwDateBefore = 0;
   m_dwDateAfter  =0;
   m_dwIDBefore = 0;
   m_dwIDAfter = 0;
   m_dwDaysBefore = 0;
   m_dwDaysAfter = 0;
   m_dwDatePredicted = 0;
   m_dwDateStarted = 0;
   m_dwConflict = 0;
   m_dwDeadTimeBefore = 0;
   m_iMinuteMinStart = 0;
   m_memName.CharCat (0);
   m_memDescription.CharCat (0);
   m_memSubProject.CharCat (0);
   m_dwAssignedTo = 0;
}

CProjectTask::~CProjectTask (void)
{
   HANGFUNCIN;
   // intentionally left blank
}


/*************************************************************************
CProjectTask::SetDirty - Sets the dirty flag in the main project
*/
void CProjectTask::SetDirty (void)
{
   HANGFUNCIN;
   if (m_pProject)
      m_pProject->m_fDirty = TRUE;
}

/*************************************************************************
CProjectTask::Write - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CProjectTask::Write (PCMMLNode pParent)
{
   HANGFUNCIN;
   PCMMLNode   pSub;

   pSub = pParent->ContentAddNewNode ();
   if (!pSub)
      return FALSE;

   // set the name
   pSub->NameSet (m_dwDateCompleted ? gszTaskCompleted : gszTask);

   // write out parameters
   NodeValueSet (pSub, gszName, (PWSTR) m_memName.p);
   NodeValueSet (pSub, gszDescription, (PWSTR) m_memDescription.p);
   NodeValueSet (pSub, gszSubProject, (PWSTR) m_memSubProject.p);
   NodeValueSet (pSub, gszDuration, (int) m_dwDuration);
   NodeValueSet (pSub, gszID, (int) m_dwID);
   NodeValueSet (pSub, gszDateCompleted, (int) m_dwDateCompleted);
   NodeValueSet (pSub, gszDateBefore, (int) m_dwDateBefore);
   NodeValueSet (pSub, gszDateAfter, (int) m_dwDateAfter);
   NodeValueSet (pSub, gszIDBefore, (int) m_dwIDBefore);
   NodeValueSet (pSub, gszIDAfter, (int) m_dwIDAfter);
   NodeValueSet (pSub, gszDaysBefore, (int) m_dwDaysBefore);
   NodeValueSet (pSub, gszDaysAfter, (int) m_dwDaysAfter);
   NodeValueSet (pSub, gszAssignedTo, (int) m_dwAssignedTo);

   m_PlanTask.Write (pSub);

   return TRUE;
}


/*************************************************************************
CProjectTask::Read - Writes the task information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the task data
reutrns
   BOOL - TRUE if successful
*/
BOOL CProjectTask::Read (PCMMLNode pSub)
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

   psz = NodeValueGet (pSub, gszSubProject);
   MemZero (&m_memSubProject);
   if (psz)
      MemCat (&m_memSubProject, psz);

   m_dwDuration = (DWORD) NodeValueGetInt (pSub, gszDuration, 100);
   m_dwID = (DWORD) NodeValueGetInt (pSub, gszID, 1);
   m_dwDateCompleted = (DWORD) NodeValueGetInt (pSub, gszDateCompleted, 0);
   m_dwDateBefore = (DWORD) NodeValueGetInt (pSub, gszDateBefore, 0);
   m_dwDateAfter = (DWORD) NodeValueGetInt (pSub, gszDateAfter, 0);
   m_dwIDBefore = (DWORD) NodeValueGetInt (pSub, gszIDBefore, 0);
   m_dwIDAfter = (DWORD) NodeValueGetInt (pSub, gszIDAfter, 0);
   m_dwDaysBefore = (DWORD) NodeValueGetInt (pSub, gszDaysBefore, 0);
   m_dwDaysAfter = (DWORD) NodeValueGetInt (pSub, gszDaysAfter, 0);
   m_dwAssignedTo = (DWORD) NodeValueGetInt (pSub, gszAssignedTo, 0);

   m_PlanTask.Read(pSub);

   return TRUE;
}

/*************************************************************************
CProject::Constructor and destructor - Initialize
*/
CProject::CProject (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwDaysPerWeek = 0;
   m_dwNextTaskID = 1;
   m_iJournalID = -1;
   m_fDisabled = FALSE;

   m_listTasks.Init (sizeof(PCProjectTask));
   m_listTasksCompleted.Init (sizeof(PCProjectTask));
   m_listDaysPerPerson.Init (sizeof(DAYSPERPERSON));
}

CProject::~CProject (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new project is cleared
   Init();
}


/************************************************************************
CProject::TaskAdd - Adds a mostly default task.

inputs
   PWSTR    pszName - name
   PWSTR    pszSubProject - sub project
   DWORD    dwDuration - duration (1/100th of a day)
   DWORD    dwBefore - Task index to insert before. -1 for after
returns
   PCProjectTask - To the task object, to be added further
*/
PCProjectTask CProject::TaskAdd (PWSTR pszName, PWSTR pszSubProject, DWORD dwDuration,
                                 DWORD dwBefore)
{
   HANGFUNCIN;
   PCProjectTask pTask;
   pTask = new CProjectTask;
   if (!pTask)
      return NULL;

   pTask->m_dwID = m_dwNextTaskID++;
   m_fDirty = TRUE;
   pTask->m_pProject = this;

   MemZero (&pTask->m_memName);
   MemCat (&pTask->m_memName, pszName);

   MemZero (&pTask->m_memSubProject);
   MemCat (&pTask->m_memSubProject, pszSubProject);

   pTask->m_dwDuration = dwDuration;

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
PCProjectTask CProject::TaskGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCProjectTask pTask, *ppTask;

   ppTask = (PCProjectTask*) m_listTasks.Get(dwIndex);
   if (!ppTask)
      return NULL;
   pTask = *ppTask;
   return pTask;
}

/*************************************************************************
TaskGetByID - Given a task ID, this returns a task, or NULL if can't find.
*/
PCProjectTask CProject::TaskGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCProjectTask pTask;
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
DWORD CProject::TaskIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCProjectTask pTask;
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
CProject::Write - Writes all the project data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
project object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CProject::Write (void)
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

   // write out the name, description, and daysperweek
   if (m_memName.p) {
      if (!NodeValueSet (pNode, gszName, (PWSTR) m_memName.p))
         return FALSE;
   }
   if (m_memDescription.p) {
      if (!NodeValueSet (pNode, gszDescription, (PWSTR) m_memDescription.p))
         return FALSE;
   }
   if (!NodeValueSet (pNode, gszDaysPerWeek, m_dwDaysPerWeek))
      return FALSE;
   if (!NodeValueSet (pNode, gszJournal, m_iJournalID))
      return FALSE;
   if (!NodeValueSet (pNode, gszDisabled, (int) m_fDisabled))
      return FALSE;
   if (!NodeValueSet (pNode, gszNextTaskID, m_dwNextTaskID))
      return FALSE;

   // BUGFIX - Write out days per person
   DWORD i, dwNum;
   for (i = 0; i < m_listDaysPerPerson.Num(); i++) {
      PDAYSPERPERSON p = (PDAYSPERPERSON) m_listDaysPerPerson.Get(i);
      NodeElemSet (pNode, gszDaysPerPerson, L"p", (int) p->dwNode, TRUE,
         0, 0, 0, (int) p->dwDays);
   }



   // BUGBUG - 2.0 - at some point write out last modified, for search purposes

   // remove existing tasks
   dwNum = pNode->ContentNum();
   for (i = dwNum - 1; i < dwNum; i--) {
      PCMMLNode   pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszTask) && _wcsicmp(pSub->NameGet(), gszTaskCompleted))
         continue;

      // else, old task. delete
      pNode->ContentRemove (i);
   }

   // write out other info, such as tasks, completed and not
   PCProjectTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCProjectTask*) m_listTasks.Get(i));
      pTask->Write (pNode);
   }
   for (i = 0; i < m_listTasksCompleted.Num(); i++) {
      pTask = *((PCProjectTask*) m_listTasksCompleted.Get(i));
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
CProject::Flush - If m_fDirty is set it writes out the project and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CProject::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CProject::Init(void) - Initializes a blank project. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CProject::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCProjectTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCProjectTask*) m_listTasks.Get(i));
      delete pTask;
   }
   m_listTasks.Clear();
   for (i = 0; i < m_listTasksCompleted.Num(); i++) {
      pTask = *((PCProjectTask*) m_listTasksCompleted.Get(i));
      delete pTask;
   }
   m_listTasksCompleted.Clear();

   // new values for name, description, and days per week
   m_memName.m_dwCurPosn = 0;
   m_memName.CharCat (0);
   m_memDescription.m_dwCurPosn = 0;
   m_memDescription.CharCat (0);
   m_dwDaysPerWeek = 5;
   m_iJournalID = -1;
   m_fDisabled = FALSE;
   m_dwNextTaskID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   // filtered list elements
   GeneratedFilteredList ();
   CalculateSchedule ();

   return TRUE;
}

/*************************************************************************
CProject::Init(dwDatabaseNode) - Initializes by reading the project in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a project, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CProject::Init (DWORD dwDatabaseNode)
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
   if (_wcsicmp(pNode->NameGet(), gszProjectNode)) {
      // invalid name
      gpData->NodeRelease(pNode);
      return FALSE;
   }
   m_dwDatabaseNode = dwDatabaseNode;


   // read in the name, description, and daysperweek
   PWSTR pszTemp;
   pszTemp = NodeValueGet (pNode, gszName);
   if (pszTemp) {
      m_memName.m_dwCurPosn = 0;
      m_memName.StrCat (pszTemp);
      m_memName.CharCat (0);
   }
   pszTemp = NodeValueGet (pNode, gszDescription);
   if (pszTemp) {
      m_memDescription.m_dwCurPosn = 0;
      m_memDescription.StrCat (pszTemp);
      m_memDescription.CharCat (0);
   }
   m_dwDaysPerWeek = NodeValueGetInt (pNode, gszDaysPerWeek, 5);
   m_iJournalID = NodeValueGetInt (pNode, gszJournal, -1);
   m_fDisabled = NodeValueGetInt (pNode, gszDisabled, 0);
   m_dwNextTaskID = NodeValueGetInt (pNode, gszNextTaskID, 1);

   // BUGFIX - Read in days per person
   m_listDaysPerPerson.Clear();
   DWORD i, dwNum;
   PWSTR psz;
   dwNum = 0;
   for (i = 0; ; i++) {
      DAYSPERPERSON dp;
      psz = NodeElemGet(pNode, gszDaysPerPerson, &dwNum, (int*) &dp.dwNode, NULL,
         NULL, NULL, (int*) &dp.dwDays);
      if (!psz)
         break;
      m_listDaysPerPerson.Add (&dp);
   }

   // read in other tasks
   PCMMLNode pSub;
   dwNum = pNode->ContentNum();
   for (i = 0; i < dwNum; i++) {
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      BOOL  fCompleted;
      if (!_wcsicmp(pSub->NameGet(), gszTask))
         fCompleted = FALSE;
      else if (!_wcsicmp(pSub->NameGet(), gszTaskCompleted))
         fCompleted = TRUE;
      else
         continue;   // not looking for this

      PCProjectTask pTask;
      pTask = new CProjectTask;
      pTask->Read (pSub);
      pTask->m_pProject = this;


      // add to list
      if (fCompleted)
         m_listTasksCompleted.Add (&pTask);
      else
         m_listTasks.Add (&pTask);
   }


   // generate list for name-control of tasks & subprojects
   GeneratedFilteredList ();
   CalculateSchedule ();

   
   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/****************************************************************
TaskSort - Resort funtino for qsort so that tasks can be resorted
after they start date has been figured out. They have to be resorted
because with several people now in the work queue if they're not
resorted it may task #5 may begin in december, task # 6 in january,
and task #7 in the previous december. This causes display problems.
*/
int __cdecl TaskSort(const void *elem1, const void *elem2 )
{
   PCProjectTask t1, t2;
   t1 = *((PCProjectTask*)elem1);
   t2 = *((PCProjectTask*)elem2);

   if (t1->m_dwDateStarted != t2->m_dwDateStarted)
      return (int) t1->m_dwDateStarted - (int) t2->m_dwDateStarted;

   // else, return previous order if don't care
   return (int) t1->m_dwOldOrder - (int) t2->m_dwOldOrder;
}


/***********************************************************************
CalculateSchedule - Looks through all the active tasks and calculates
approximately when they'll get done, assuming today is a starting day.
*/
typedef struct {
   DWORD    dwPerson;   // person. 0 = yourself. Otherwise someone else
   DWORD    dwDaysPerWeek; // days per week the person works on the project
   __int64  iMinuteCur; // current time he/she is on
} PERSONTIME, *PPERSONTIME;
void CProject::CalculateSchedule (void)
{
   HANGFUNCIN;
   CListFixed listTime;
   listTime.Init (sizeof(PERSONTIME));

   // get today's date as the starting point
   DFDATE   dateCur;
   __int64  iMinuteMovedTo, iTaskDuration, iMinuteFinished, iStart;
   dateCur = Today();
   iStart = DFDATEToMinutes (dateCur);

   // zero out all the minute befores
   DWORD i;
   PCProjectTask pTask;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = TaskGetByIndex (i);
      if (!pTask)
         break;

      pTask->m_iMinuteMinStart = 0;
   }

   // loop
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = TaskGetByIndex (i);
      if (!pTask)
         break;

      // remember the old sort order
      pTask->m_dwOldOrder = i;

      // find the person
      DWORD j;
      PERSONTIME *pt;
      for (j = 0; j < listTime.Num(); j++) {
         pt = (PPERSONTIME) listTime.Get(j);
         if (!pt)
            continue;
         if (pt->dwPerson == pTask->m_dwAssignedTo)
            break;
      }
      if (j >= listTime.Num()) {
         // couldn't find so add
         PERSONTIME per;
         memset (&per, 0, sizeof(per));
         if (pTask->m_dwAssignedTo == 0)
            per.dwDaysPerWeek = m_dwDaysPerWeek;
         else {
            // find person
            DWORD k;
            for (k = 0; k < gCurProject.m_listDaysPerPerson.Num(); k++) {
               PDAYSPERPERSON p = (PDAYSPERPERSON) gCurProject.m_listDaysPerPerson.Get(k);
               if (p->dwNode == pTask->m_dwAssignedTo) {
                  per.dwDaysPerWeek = p->dwDays;
                  break;
               }
            }
            // else, if have no record set to 5 days
            if (k >= gCurProject.m_listDaysPerPerson.Num()) {
               DAYSPERPERSON dp;
               per.dwDaysPerWeek = dp.dwDays = 5;
               dp.dwNode = pTask->m_dwAssignedTo;
               gCurProject.m_listDaysPerPerson.Add(&dp);
               gCurProject.m_fDirty=TRUE;
               gCurProject.Flush();
            }
         }
         per.dwPerson = pTask->m_dwAssignedTo;
         per.iMinuteCur = iStart;
         listTime.Add(&per);
         pt = (PPERSONTIME) listTime.Get(listTime.Num()-1);
      }

      // zero out
      pTask->m_dwConflict = 0;
      pTask->m_dwDatePredicted = 0;
      pTask->m_dwDateStarted = 0;
      pTask->m_dwDeadTimeBefore = 0;
      iMinuteMovedTo = max(pt->iMinuteCur, pTask->m_iMinuteMinStart);

      // the task duration in minutes. This is the duraction in 1/100th of a day, taking
      // into account that only work on project a few days of the week
      iTaskDuration = 60 * 24 * 7 * (__int64)pTask->m_dwDuration / 100 / max(pt->dwDaysPerWeek,1);

      // if it must occur after a specific date then consider moving it back
      if (pTask->m_dwDateAfter) {
         __int64 iMinuteAfter;
         iMinuteAfter = DFDATEToMinutes(pTask->m_dwDateAfter);
         if (iMinuteAfter > pt->iMinuteCur) {
            // it needs to occur after this
            iMinuteMovedTo = max(iMinuteMovedTo, iMinuteAfter);
            // pTask->m_dwConflict |= CONFLICT_DATEAFTER;
         }
      }

      // if it must occur after another task then consider moving it back
      if (pTask->m_dwIDAfter) {
         // find the task index to make sure it's already occured
         DWORD dwIndex;
         dwIndex = TaskIndexByID (pTask->m_dwIDAfter);

         // if it cant be found at all then assume it's alredy fixed so ignore
         if (dwIndex == (DWORD)-1)
            goto finishedafter;

         // if it occurs after this then error
         if (dwIndex >= i) {
            pTask->m_dwConflict |= CONFLICT_IDAFTER;
            goto finishedafter;
         }

         // get the time that it occurs
         PCProjectTask pLink;
         __int64 iWhen;
         pLink = TaskGetByIndex (dwIndex);
         iWhen = DFDATEToMinutes (pLink->m_dwDatePredicted) + 60 * 24 * (__int64) pTask->m_dwDaysAfter;

         if (iWhen > pt->iMinuteCur) {
            // it needs to occur after this
            iMinuteMovedTo = max(iMinuteMovedTo, iWhen);
         }
      }
finishedafter:

      // calculate when it should be finished
      iMinuteFinished = iMinuteMovedTo + iTaskDuration;

      // if it must finish before a date and it doesn't then conflict
      if (pTask->m_dwDateBefore) {
         __int64 iMinuteBefore;
         iMinuteBefore = DFDATEToMinutes(pTask->m_dwDateBefore);
         if (iMinuteBefore < iMinuteFinished)
            pTask->m_dwConflict |= CONFLICT_DATEBEFORE;
      }

      // if it must finish before a task and it doesn't then conflict
      if (pTask->m_dwIDBefore) {
         // find the task index to make sure it's already occured
         DWORD dwIndex;
         dwIndex = TaskIndexByID (pTask->m_dwIDBefore);

         // if it cant be found at all then assume it's alredy fixed so ignore
         if (dwIndex == (DWORD)-1)
            goto finishedbefore;

         // if it occurs after this then error
         if (dwIndex <= i) {
            pTask->m_dwConflict |= CONFLICT_IDBEFORE;
            goto finishedbefore;
         }

         // store the minimum requirement away
         // however, if there's another value there already, take the maximum of the two
         PCProjectTask pLink;
         pLink = TaskGetByIndex (dwIndex);
         __int64 iNew;
         iNew = iMinuteFinished + 60 * 24 * (__int64) pTask->m_dwDaysBefore;
         pLink->m_iMinuteMinStart = max(pLink->m_iMinuteMinStart, iNew);
      }
finishedbefore:

      // write out the info
      pTask->m_dwDateStarted = MinutesToDFDATE(iMinuteFinished - iTaskDuration);
      pTask->m_dwDatePredicted = MinutesToDFDATE (iMinuteFinished);
      pTask->m_dwDeadTimeBefore = (DWORD) ((iMinuteMovedTo - pt->iMinuteCur) / 60 / 24);
      pt->iMinuteCur = iMinuteFinished;
   }

   // resort the tasks
   qsort (m_listTasks.Get(0), m_listTasks.Num(), sizeof(PCProjectTask), TaskSort);
}


/***********************************************************************
GeneratedFilteredList - Generates the CListVariable to be used for
filterd list controls. This is a list of null-terminated strings.
*/
void CProject::GeneratedFilteredList (void)
{
   HANGFUNCIN;
   // clear the current lists
   m_listFilteredTasks.Clear();
   m_listFilteredSubProjects.Clear();

   // create one for the tasks
   DWORD i;
   PCProjectTask pTask;
   CBTree tree;
   for (i = 0; i < m_listTasks.Num(); i++) {
      pTask = *((PCProjectTask*) m_listTasks.Get(i));
      m_listFilteredTasks.Add (pTask->m_memName.p, (wcslen((WCHAR*)pTask->m_memName.p)+1)*2);

      // see if the subproject is in a tree
      PWSTR pszSub;
      pszSub = (WCHAR*) pTask->m_memSubProject.p;
      if (!pszSub || !pszSub[0])
         continue;
      if (tree.Find(pszSub))
         continue;   // already there

      // else, the subproject is new, so add it
      tree.Add (pszSub, &i, sizeof(i));
      m_listFilteredSubProjects.Add (pszSub, (wcslen(pszSub)+1)*2);
   }

}


/***********************************************************************
ProjectFilteredList - This makes sure the filtered list (CListVariable)
for Project is populated. If not, all the information is loaded from
the Project major-section (see FindMajorSection()).

This fills in:
   gfProjectPopulated - Set to TRUE when it's been loaded
   glistProject - CListVariable of Project's names followed by nicknames
   glistProjectID - CListFixed with the project's CDatabase ID.

inputs
   none
returns
   PCListVariable - for the FilteredList control
*/
PCListVariable ProjectFilteredList (void)
{
   HANGFUNCIN;
   if (gfProjectPopulated)
      return &glistProject;

   // else get it
   PCMMLNode   pNode;
   pNode = FindMajorSection (gszProjectListNode);
   if (!pNode)
      return NULL;   // error. This shouldn't happen

   // fill the list in
   FillFilteredList (pNode, gszProjectNode, &glistProject, &glistProjectID);

   // finall, flush release
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfProjectPopulated = TRUE;

   return &glistProject;
}

/***********************************************************************
ProjectIndexToDatabase - Given an index ID (into glistProject), this
returns a DWORD CDatabase location for the project

inputs
   DWORD - dwIndex
returns
   DWORD - CDatabase location. -1 if cant find
*/
DWORD ProjectIndexToDatabase (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   ProjectFilteredList();

   // find the index
   return FilteredIndexToDatabase (dwIndex, &glistProject, &glistProjectID);
}


/***********************************************************************
ProjectDatabaseToIndex - Given a CDatabase location for the project data
this returns an index into the glistProject.

inputs
   DWORD - CDatabase location
returns
   DWORD - Index into clistProject. -1 if cant find
*/
DWORD ProjectDatabaseToIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   ProjectFilteredList();

   // find the index
   return FilteredDatabaseToIndex (dwIndex, &glistProject, &glistProjectID);
}


/***********************************************************************
ProjectShutDown - Makes sure everything is shut down on the project side.
*/
void ProjectShutDown (void)
{
   HANGFUNCIN;
   // save current project
   // this shouldn't really matter but doit shut in case
   gCurProject.Init();

   // don't need to save the list of nodes because it should already be saved
}


/*****************************************************************************
ProjectTaskAddPage - Override page callback.
*/
BOOL ProjectTaskAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
            WCHAR szDescription[512];
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

            // days of work
            double   fWork;
            WCHAR szTemp[128];
            pControl = pPage->ControlFind (L"daysofwork");
            fWork = 1.0;
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            AttribToDouble (szTemp, &fWork);

            // get the subproject
            WCHAR szSubProject[128];
            szSubProject[0] = 0;
            pControl = pPage->ControlFind (gszSubProject);
            if (pControl) {
               int   iRet;
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0)
                  wcscpy (szSubProject, (PWSTR) gCurProject.m_listFilteredSubProjects.Get(iRet));
            }

            // insert before
            DWORD dwBefore;
            dwBefore = (DWORD)-1;
            pControl = pPage->ControlFind (L"insertbefore");
            if (pControl)
               dwBefore = (DWORD) pControl->AttribGetInt (gszCurSel);

            // need to create a dummy task to store some variables away
            CProjectTask dummyTask;
            // complete N days before task
            // complete N days after task
            dummyTask.m_dwDaysBefore = dummyTask.m_dwDaysAfter = 0;
            pControl = pPage->ControlFind (gszDaysBefore);
            if (pControl)
               dummyTask.m_dwDaysBefore = (DWORD)pControl->AttribGetInt(gszText);
            pControl = pPage->ControlFind (gszDaysAfter);
            if (pControl)
               dummyTask.m_dwDaysAfter = (DWORD)pControl->AttribGetInt(gszText);
            dummyTask.m_dwIDAfter = dummyTask.m_dwIDBefore = (DWORD)-1;
            pControl = pPage->ControlFind (L"taskbefore");
            int iRet;
            PCProjectTask pLink;
            if (pControl) {
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0) {
                  pLink = gCurProject.TaskGetByIndex((DWORD) iRet);
                  if (pLink)
                     dummyTask.m_dwIDBefore = pLink->m_dwID;
               }
            }
            pControl = pPage->ControlFind (L"taskafter");
            if (pControl) {
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0) {
                  pLink = gCurProject.TaskGetByIndex((DWORD) iRet);
                  if (pLink)
                     dummyTask.m_dwIDAfter = pLink->m_dwID;
               }
            }

            // add it
            PCProjectTask pTask;
            pTask = gCurProject.TaskAdd (szName, szSubProject, (DWORD)(fWork * 100), dwBefore);
            if (!pTask) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // description
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pTask->m_memDescription);
            MemCat (&pTask->m_memDescription, szDescription);

            // complete before/after date
            pTask->m_dwDateAfter = DateControlGet (pPage, gszDateAfter);
            pTask->m_dwDateBefore = DateControlGet (pPage, gszDateBefore);

            // copy stuff from dummy task
            pTask->m_dwDaysBefore = dummyTask.m_dwDaysBefore;
            pTask->m_dwDaysAfter = dummyTask.m_dwDaysAfter;
            pTask->m_dwIDAfter = dummyTask.m_dwIDAfter;
            pTask->m_dwIDBefore = dummyTask.m_dwIDBefore;

            //get the assigned to
            pControl = pPage->ControlFind (gszAssignedTo);
            if (pControl) {
               iRet = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iRet);
               pTask->m_dwAssignedTo = (dwNode != (DWORD)-1) ? dwNode : 0;
            }


            // exit this
            gCurProject.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
ProjectTaskAddMultiplePage - Override page callback.
*/
BOOL ProjectTaskAddMultiplePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
            WCHAR szControl[64];
            PCEscControl pControl;
            DWORD dwNeeded;

            // get the subproject
            WCHAR szSubProject[128];
            szSubProject[0] = 0;
            pControl = pPage->ControlFind (gszSubProject);
            if (pControl) {
               int   iRet;
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0)
                  wcscpy (szSubProject, (PWSTR) gCurProject.m_listFilteredSubProjects.Get(iRet));
            }

            // insert before
            DWORD dwBefore;
            dwBefore = (DWORD)-1;
            pControl = pPage->ControlFind (L"insertbefore");
            if (pControl)
               dwBefore = (DWORD) pControl->AttribGetInt (gszCurSel);

            //get the assigned to
            DWORD dwAssignedTo;
            dwAssignedTo = 0;
            pControl = pPage->ControlFind (gszAssignedTo);
            if (pControl) {
               int iRet;
               iRet = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iRet);
               dwAssignedTo = (dwNode != (DWORD)-1) ? dwNode : 0;
            }

            DWORD i;
            for (i = 0; i < 10; i++) {
               swprintf (szControl, L"task%d", i+1);
               szName[0] = 0;

               // name
               pControl = pPage->ControlFind(szControl);
               if (pControl)
                  pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
               if (!szName[0])
                  continue;

               // days of work
               swprintf (szControl, L"time%d", i+1);
               double   fWork;
               WCHAR szTemp[128];
               pControl = pPage->ControlFind (szControl);
               fWork = 1.0;
               if (pControl)
                  pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
               AttribToDouble (szTemp, &fWork);

               // add it
               PCProjectTask pTask;
               pTask = gCurProject.TaskAdd (szName, szSubProject, (DWORD)(fWork * 100), dwBefore);
               if (!pTask) {
                  pPage->MBError (gszWriteError);
                  return FALSE;
               }
               pTask->m_dwAssignedTo = dwAssignedTo;

               // if insering before keep on increasing insertion point
               if (dwBefore < gCurProject.m_listTasks.Num())
                  dwBefore++;

            }


            // exit this
            gCurProject.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
ProjectTaskShowAddUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project task was added
*/
BOOL ProjectTaskShowAddUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPROJECTTASKADD, ProjectTaskAddPage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}

/*****************************************************************************
ProjectTaskShowAddMultipleUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project task was added
*/
BOOL ProjectTaskShowAddMultipleUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPROJECTTASKADDMULTILE, ProjectTaskAddMultiplePage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}


/**************************************************************************
MarkTaskCompleted - Mark a project task as completed.

inputs
   PCEscPage      pPage - Page for questions
   PCProjectTask  pTask - Task
   DWORD          dwSplit - If -1 then no split was marked, otherwise a specific one was
returns
   BOOL - TRUE if changed, FALSE if didn't
*/
static BOOL MarkTaskCompleted(PCEscPage pPage, PCProjectTask pTask, DWORD dwSplit = (DWORD)-1)
{
   HANGFUNCIN;
   // otherwise delete it
   DWORD dwIndex;
   dwIndex = gCurProject.TaskIndexByID (pTask->m_dwID);
   if (dwIndex == (DWORD)-1)
      return TRUE;   // shoudlnt happen

   // BUGFIX - If there's only one split left then do entire
   if (pTask->m_PlanTask.Num() <= 1)
      dwSplit = (DWORD)-1;

   // BUGFIX - If only a part of a split it fixed then use the time specified
   // in the split
   double   fInitial;
   fInitial = pTask->m_dwDuration / 100.0 * 8.0;
   if (pTask->m_PlanTask.Num()) {
      PPLANTASKITEM pi = pTask->m_PlanTask.Get(dwSplit);
      if (pi)
         fInitial = pi->dwTimeAllotted / 60.0;
      else
         fInitial = fInitial - ((double)pTask->m_PlanTask.m_dwTimeCompleted) / 60.0;
   }
   fInitial = max(fInitial, 0);

   // BUGFIX - ask how long it took
   // BUGFIX - only ask how long if it was for yourself, not if it was
   // assigned to anyone else
   double   fHowLong;
   fHowLong = -1;
   if ((pTask->m_dwAssignedTo==0) && (gCurProject.m_iJournalID != -1)) {
      fHowLong = HowLong (pPage, fInitial);
      if (fHowLong < 0)
         return FALSE;   // pressed cancel
   }
   else {
#if 0
      if (IDYES != pPage->MBYesNo (L"Are you sure you want to mark this task as completed?",
         L"Once a task is completed you'll be able to see information about it, but you won't be able to modify it again."))
         return TRUE;   // do nothing
#endif // 0
   }

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
      // remove from current list
      EscChime (ESCCHIME_INFORMATION);
      EscSpeak (L"Task completed.");
      gCurProject.m_listTasks.Remove(dwIndex);

      // add to completed.
      pTask->m_dwDateCompleted = Today();
      gCurProject.m_listTasksCompleted.Add (&pTask);
   }
   gCurProject.m_fDirty = TRUE;

   // add to log
   WCHAR szHuge[10000];
   wcscpy (szHuge, fRemoveSplit ? L"Task worked on in " : L"Task completed in ");
   wcscat (szHuge, (PWSTR) gCurProject.m_memName.p);
   wcscat (szHuge, L" project: ");
   wcscat (szHuge, (PWSTR)pTask->m_memName.p);
   DFDATE date;
   DFTIME start, end;
   CalendarLogAdd (date = Today(), start = Now(), end = -1, szHuge, gCurProject.m_dwDatabaseNode, FALSE);

   // BUGFIX - if it was assigned to someone else then log that in their interaction history
   if (pTask->m_dwAssignedTo) {
      // person
      PCMMLNode pPerson;
      pPerson = NULL;
      pPerson = gpData->NodeGet (pTask->m_dwAssignedTo);
      if (pPerson) {
         // overwrite if already exists
         NodeElemSet (pPerson, gszInteraction, szHuge, (int) gCurProject.m_dwDatabaseNode, TRUE,
            Today(), Now(), -1);

         gpData->NodeRelease (pPerson);

      }
   }

   // if log to project that indicate that
   if (gCurProject.m_iJournalID != -1)
      JournalLink ((DWORD)gCurProject.m_iJournalID, szHuge, gCurProject.m_dwDatabaseNode, date,
      start, end, fHowLong, TRUE);

   // exit this
   gCurProject.Flush();

   return TRUE;
}

/*****************************************************************************
ProjectTaskEditPage - Override page callback.
*/
BOOL ProjectTaskEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCProjectTask pTask = (PCProjectTask) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind (gszName);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR)pTask->m_memName.p);

         // days of work
         WCHAR szTemp[128];
         pControl = pPage->ControlFind (L"daysofwork");
         swprintf (szTemp, L"%g", (double) pTask->m_dwDuration / 100.0);
         if (pControl)
            pControl->AttribSet (gszText, szTemp);

         // get the subproject
         PWSTR pszSubProject, psz;
         DWORD i;
         pszSubProject = (PWSTR)pTask->m_memSubProject.p;
         if (pszSubProject[0]) {
            for (i = 0; i < gCurProject.m_listFilteredSubProjects.Num(); i++) {
               psz = (PWSTR) gCurProject.m_listFilteredSubProjects.Get(i);

               if (!_wcsicmp(psz, pszSubProject))
                  break;
            }
            if (i >= gCurProject.m_listFilteredSubProjects.Num())
               i = gCurProject.m_listFilteredSubProjects.Add (pszSubProject, (wcslen(pszSubProject)+1)*2);
         }
         else
            i = -1;
         pControl = pPage->ControlFind (gszSubProject);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) i);

         // description
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR)pTask->m_memDescription.p);

         // complete before/after date
         DateControlSet (pPage, gszDateAfter, pTask->m_dwDateAfter);
         DateControlSet (pPage, gszDateBefore, pTask->m_dwDateBefore);

         // complete N days before task
         // complete N days after task
         pControl = pPage->ControlFind (gszDaysBefore);
         if (pControl)
            pControl->AttribSetInt(gszText, (int) pTask->m_dwDaysBefore);
         pControl = pPage->ControlFind (gszDaysAfter);
         if (pControl)
            pControl->AttribSetInt(gszText, (int) pTask->m_dwDaysAfter);

         //get the assigned to
         pControl = pPage->ControlFind (gszAssignedTo);
         if (pControl) {
            DWORD dw;
            dw = PeopleBusinessDatabaseToIndex(pTask->m_dwAssignedTo);

            pControl->AttribSetInt (gszCurSel, (int) dw);
         }

         int iRet;
         pControl = pPage->ControlFind (L"taskbefore");
         if (pControl) {
            iRet = (int) gCurProject.TaskIndexByID (pTask->m_dwIDBefore);
            pControl->AttribSetInt (gszCurSel, iRet);
         }
         pControl = pPage->ControlFind (L"taskafter");
         if (pControl) {
            iRet = (int) gCurProject.TaskIndexByID (pTask->m_dwIDAfter);
            pControl->AttribSetInt (gszCurSel, iRet);
         }

         // if it's not assigned to user then can't start timer
         if (pTask->m_dwAssignedTo) {
            pControl = pPage->ControlFind (gszTimer);
            if (pControl)
               pControl->Enable(FALSE);
         }

         // if a timer for the task is already running then disable
         PCMMLNode pNode;
         pNode = FindMajorSection (gszProjectNode);
         if (pNode) {
            DWORD dwIndex2;
            dwIndex2 = 0;
            int   iNumber, extra;
            DFDATE   date;
            DFTIME   start, end;
            while (TRUE) {
               PWSTR psz;
               psz = NodeElemGet (pNode, gszTimer, &dwIndex2, &iNumber, &date, &start, &end, &extra);
               if (!psz)
                  break; // cant find
               if ( ((DWORD)end == gCurProject.m_dwDatabaseNode) && ((DWORD)extra == pTask->m_dwID)) {
                  pControl = pPage->ControlFind (gszTimer);
                  if (pControl)
                     pControl->Enable(FALSE);
                  break;
               }
            }
            gpData->NodeRelease (pNode);
         }

      }
      break;   // make sure to fall through

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"deletetask")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to delete this task?",
               L"It will be permenantly removed from the list."))
               return TRUE;   // do nothing

            // otherwise delete it
            DWORD dwIndex;
            dwIndex = gCurProject.TaskIndexByID (pTask->m_dwID);
            if (dwIndex == (DWORD)-1)
               return TRUE;   // shoudlnt happen

            // remove
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Task deleted.");
            delete pTask;
            gCurProject.m_listTasks.Remove(dwIndex);
            gCurProject.m_fDirty = TRUE;
            // exit this
            gCurProject.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"markcompleted")) {
            if (!MarkTaskCompleted(pPage, pTask, (DWORD) -1))
               return TRUE;
            pPage->Exit (gszOK);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszTimer)) {
            // BUGFIX - Timer from project so can log work done

            // create the new timer
            PCMMLNode pNode;
            pNode = FindMajorSection (gszProjectNode);
            if (pNode) {
               // get the category name
               // write it out
               NodeElemSet (pNode, gszTimer, pTask->m_memName.p ? (PWSTR) pTask->m_memName.p : L"Unknown name",
                  gCurProject.m_dwDatabaseNode ^ GetTickCount() ^ pTask->m_dwID, FALSE, Today(), Now(),
                  (DFTIME) gCurProject.m_dwDatabaseNode, (int) pTask->m_dwID);

               // release
               gpData->NodeRelease (pNode);
               gpData->Flush();
            }
         


            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Timer started.");
            pPage->Exit (gszOK);
            return TRUE;
         }
         // user pressed OK
         else if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[128];
            WCHAR szDescription[512];
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

            // days of work
            double   fWork;
            WCHAR szTemp[128];
            pControl = pPage->ControlFind (L"daysofwork");
            fWork = 1.0;
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            AttribToDouble (szTemp, &fWork);
            pTask->m_dwDuration = (DWORD) (fWork * 100.0);

            // get the subproject
            WCHAR szSubProject[128];
            szSubProject[0] = 0;
            pControl = pPage->ControlFind (gszSubProject);
            if (pControl) {
               int   iRet;
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0)
                  wcscpy (szSubProject, (PWSTR) gCurProject.m_listFilteredSubProjects.Get(iRet));
            }
            MemZero (&pTask->m_memSubProject);
            MemCat (&pTask->m_memSubProject, szSubProject);


            // modify it
            pTask->SetDirty();

            // description
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pTask->m_memDescription);
            MemCat (&pTask->m_memDescription, szDescription);

            // complete before/after date
            pTask->m_dwDateAfter = DateControlGet (pPage, gszDateAfter);
            pTask->m_dwDateBefore = DateControlGet (pPage, gszDateBefore);

            // complete N days before task
            // complete N days after task
            pTask->m_dwDaysBefore = pTask->m_dwDaysAfter = 0;
            pControl = pPage->ControlFind (gszDaysBefore);
            if (pControl)
               pTask->m_dwDaysBefore = (DWORD)pControl->AttribGetInt(gszText);
            pControl = pPage->ControlFind (gszDaysAfter);
            if (pControl)
               pTask->m_dwDaysAfter = (DWORD)pControl->AttribGetInt(gszText);
            pTask->m_dwIDAfter = pTask->m_dwIDBefore = (DWORD)-1;
            pControl = pPage->ControlFind (L"taskbefore");
            int iRet;
            PCProjectTask pLink;
            if (pControl) {
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0) {
                  pLink = gCurProject.TaskGetByIndex((DWORD) iRet);
                  if (pLink)
                     pTask->m_dwIDBefore = pLink->m_dwID;
               }
            }
            pControl = pPage->ControlFind (L"taskafter");
            if (pControl) {
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0) {
                  pLink = gCurProject.TaskGetByIndex((DWORD) iRet);
                  if (pLink)
                     pTask->m_dwIDAfter = pLink->m_dwID;
               }
            }

             //get the assigned to
            pControl = pPage->ControlFind (gszAssignedTo);
            if (pControl) {
               iRet = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iRet);
               pTask->m_dwAssignedTo = (dwNode != (DWORD)-1) ? dwNode : 0;
            }
           // exit this
            gCurProject.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
ProjectTaskSplitPage - Override page callback.
*/
BOOL ProjectTaskSplitPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   // get the task that splitting
   PCProjectTask pTask = (PCProjectTask) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set the person it's assigned to
         // get the subproject
         PWSTR pszSubProject, psz;
         DWORD i;
         pszSubProject = (PWSTR)pTask->m_memSubProject.p;
         if (pszSubProject[0]) {
            for (i = 0; i < gCurProject.m_listFilteredSubProjects.Num(); i++) {
               psz = (PWSTR) gCurProject.m_listFilteredSubProjects.Get(i);

               if (!_wcsicmp(psz, pszSubProject))
                  break;
            }
            if (i >= gCurProject.m_listFilteredSubProjects.Num())
               i = gCurProject.m_listFilteredSubProjects.Add (pszSubProject, (wcslen(pszSubProject)+1)*2);
         }
         else
            i = -1;
         pControl = pPage->ControlFind (gszSubProject);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) i);

         //get the assigned to
         pControl = pPage->ControlFind (gszAssignedTo);
         if (pControl) {
            DWORD dw;
            dw = PeopleBusinessDatabaseToIndex(pTask->m_dwAssignedTo);

            pControl->AttribSetInt (gszCurSel, (int) dw);
         }


         // set the project
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[128];
            WCHAR szControl[64];
            PCEscControl pControl;
            DWORD dwNeeded;

            // get the subproject
            WCHAR szSubProject[128];
            szSubProject[0] = 0;
            pControl = pPage->ControlFind (gszSubProject);
            if (pControl) {
               int   iRet;
               iRet = pControl->AttribGetInt (gszCurSel);
               if (iRet >= 0)
                  wcscpy (szSubProject, (PWSTR) gCurProject.m_listFilteredSubProjects.Get(iRet));
            }

            // insert before
            DWORD dwBefore;
            dwBefore = gCurProject.TaskIndexByID (pTask->m_dwID);

            //get the assigned to
            DWORD dwAssignedTo;
            dwAssignedTo = 0;
            pControl = pPage->ControlFind (gszAssignedTo);
            if (pControl) {
               int iRet;
               iRet = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iRet);
               dwAssignedTo = (dwNode != (DWORD)-1) ? dwNode : 0;
            }

            DWORD i;
            DWORD dwFound;
            dwFound = 0;
            for (i = 0; i < 10; i++) {
               swprintf (szControl, L"task%d", i+1);
               szName[0] = 0;

               // name
               pControl = pPage->ControlFind(szControl);
               if (pControl)
                  pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
               if (!szName[0])
                  continue;

               dwFound++;

               // days of work
               swprintf (szControl, L"time%d", i+1);
               double   fWork;
               WCHAR szTemp[128];
               pControl = pPage->ControlFind (szControl);
               fWork = 1.0;
               if (pControl)
                  pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
               AttribToDouble (szTemp, &fWork);

               // add it
               PCProjectTask pTask;
               pTask = gCurProject.TaskAdd (szName, szSubProject, (DWORD)(fWork * 100), dwBefore);
               if (!pTask) {
                  pPage->MBError (gszWriteError);
                  return FALSE;
               }
               pTask->m_dwAssignedTo = dwAssignedTo;

               // if insering before keep on increasing insertion point
               if (dwBefore < gCurProject.m_listTasks.Num())
                  dwBefore++;

            }

            // delete this task
            // otherwise delete it
            DWORD dwIndex;
            dwIndex = gCurProject.TaskIndexByID (pTask->m_dwID);
            if (dwFound && (dwIndex != (DWORD)-1)) {
               delete pTask;
               gCurProject.m_listTasks.Remove(dwIndex);
               gCurProject.m_fDirty = TRUE;
            }

            // exit this
            gCurProject.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
ProjectTaskShowEditUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   PCProjectTask  pTask - Task
returns
   BOOL - TRUE if a project task was added
*/
BOOL ProjectTaskShowEditUI (PCEscPage pPage, PCProjectTask pTask)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPROJECTTASKEDIT, ProjectTaskEditPage, pTask);

   if (pszRet && !_wcsicmp(pszRet, L"split")) {
      // BUGFIX - User has selected the option of splitting the task, so do that
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPROJECTTASKSPLIT, ProjectTaskSplitPage, pTask);
   }

   return pszRet && !_wcsicmp(pszRet, gszOK);
}


/*****************************************************************************
ProjectListPage - Override page callback.
*/
BOOL ProjectListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only care about projectlist
         if (_wcsicmp(p->pszSubName, L"PROJECTLIST"))
            break;


         // make sure we have the list filled in
         ProjectFilteredList();

         // if there aren't any elements then set to NULL and return
         if (!glistProject.Num()) {
            p->pszSubString = NULL;
            return TRUE;
         }

         // else, need to produce a table
         MemZero (&gMemTemp);
         if (gfMicroHelp) {
            MemCat (&gMemTemp, L"<xbr/>");
            MemCat (&gMemTemp, L"<p>Click the project to view.</p>");
         }
         MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");

         // loop
         DWORD i;
         for (i = 0; i < glistProject.Num(); i += 2) {
            MemCat (&gMemTemp, L"<tr>");

            // loop through the two sub elements
            DWORD j;
            for (j = i; j < i+2; j++) {
               DWORD dwNode;
               dwNode = FilteredIndexToDatabase(j, &glistProject, &glistProjectID);

               // try loading in the project
               if (dwNode != (DWORD)-1) {
                  if (!gCurProject.Init (dwNode))
                     dwNode = (DWORD)-1;
               }

               // if it's blank stick in empty node
               if (dwNode == (DWORD)-1) {
                  MemCat (&gMemTemp, L"<xProjectBlank/>");
                  continue;
               }

               // have a loaded project for the current once, use this for the name and stuff
               MemCat (&gMemTemp, L"<xProjectButton href=v:");
               MemCat (&gMemTemp, (int) dwNode);   // so view the node number
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, (PWSTR) gCurProject.m_memName.p);
               MemCat (&gMemTemp, L"<xHoverHelp>");
               MemCatSanitize (&gMemTemp, (PWSTR) gCurProject.m_memDescription.p);
               MemCat (&gMemTemp, L"</xHoverHelp>");
               MemCat (&gMemTemp, L"</xProjectButton>");
               
            }

            MemCat (&gMemTemp, L"</tr>");
         }

         // and finally
         MemCat (&gMemTemp, L"</table><p/>");

         p->pszSubString = (PWSTR) gMemTemp.p;
         return TRUE;
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
JournalControlGet - Gets the value from the journal control

inputs
   PCEscPage      pPage - page
   PCWorkTask     pTask - task
returns
   none
*/
static void JournalControlGet (PCEscPage pPage, PCProject pTask)
{
   HANGFUNCIN;
   PCEscControl pControl;

   pControl = pPage->ControlFind (gszJournal);
   if (pControl) {
      // get the values
      int   iCurSel;
      iCurSel = pControl->AttribGetInt (gszCurSel);

      // find the node number for the person's data and the name
      int iRet;
      iRet = (int) JournalIndexToDatabase ((DWORD)iCurSel);
      if (iRet != pTask->m_iJournalID) {
         pTask->m_iJournalID = iRet;
         pTask->m_fDirty = TRUE;
      }

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
static void JournalControlSet (PCEscPage pPage, PCProject pTask)
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
ProjectAddPage - Override page callback.
*/
BOOL ProjectAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"addproject"))
            break;

         // make sure the list of projects is loaded
         ProjectFilteredList ();

         // make sure a name is typed in
         PCEscControl   pControl;
         WCHAR szName[128], szDescription[1024];
         DWORD dwNeeded;
         szName[0] = 0;
         szDescription[0] = 0;
         pControl = pPage->ControlFind(gszName);
         if (pControl)
            pControl->AttribGet(gszText, szName, sizeof(szName), &dwNeeded);
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribGet(gszText, szDescription, sizeof(szDescription), &dwNeeded);

         // make sure the name is unique
         if (!szName[0]) {
            pPage->MBWarning (L"You need to type in a name for the project.");
            return TRUE;
         }
         if ((DWORD)-1 != FilteredStringToIndex (szName,&glistProject, &glistProjectID)) {
            pPage->MBWarning (L"You already have a project with that name.",
               L"Please type in a different name.");
            return TRUE;
         }

         // make sure a description is typed in
         if (!szDescription[0]) {
            pPage->MBWarning (L"You need to type in a description for the project.",
               L"The description is for your own future reference, so that you'll know "
               L"what the project is about several years from now.");
            return TRUE;
         }

         // add the node to the database
         DWORD dwNode;
         PCMMLNode   pNew;
         pNew = gpData->NodeAdd (gszProjectNode, &dwNode);
         if (!pNew) {
            pPage->MBError (gszWriteError);
            return FALSE;  // unexpected error
         }
         gpData->NodeRelease(pNew);

         // set gCurPorject to use the database
         if (!gCurProject.Init (dwNode)) {
            pPage->MBError (gszWriteError);
            return FALSE;  // unexpected error
         }

         // set the description, name, and days usage for that
         gCurProject.m_memName.m_dwCurPosn = 0;
         gCurProject.m_memName.StrCat (szName);
         gCurProject.m_memName.CharCat (0);
         gCurProject.m_memDescription.m_dwCurPosn = 0;
         gCurProject.m_memDescription.StrCat (szDescription);
         gCurProject.m_memDescription.CharCat (0);
         gCurProject.m_fDirty = TRUE;
         pControl = pPage->ControlFind (gszDaysPerWeek);
         if (pControl) {
            gCurProject.m_dwDaysPerWeek = (DWORD) pControl->AttribGetInt (gszCurSel) + 1;
         }
         JournalControlGet(pPage, &gCurProject);

         // flush
         gCurProject.Flush();

         // add the node to the list of projects
         PCMMLNode   pMajor;
         pMajor = FindMajorSection (gszProjectListNode);
         if (!pMajor) {
            pPage->MBError (gszWriteError);
            return FALSE;  // unexpected error
         }
         if (!AddFilteredList (szName, dwNode, pMajor, gszProjectNode,
            &glistProject, &glistProjectID)) {
               pPage->MBError (gszWriteError);
               gpData->NodeRelease(pMajor);
               return FALSE;  // unexpected error
         }
         gpData->NodeRelease(pMajor);
         gpData->Flush();

         // use TTS to speak
         EscChime (ESCCHIME_INFORMATION);
         EscSpeak (L"Project created.");

         // go to the edit project page
         WCHAR szTemp[32];
         swprintf (szTemp, L"v:%d", (int)dwNode);
         pPage->Exit(szTemp);
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}



/*****************************************************************************
ProjectRemovePage - Override page callback.
*/
BOOL ProjectRemovePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl   pControl;
         pControl = pPage->ControlFind (gszProjectRemove);
         if (!pControl)
            break;

         // get the list of choices
         ProjectFilteredList();

         // add this
         ESCMCOMBOBOXADD add;
         memset (&add, 0, sizeof(add));
         add.dwInsertBefore = (DWORD)-1;
         
         DWORD i;
         for (i = 0; i < glistProject.Num(); i++) {
            PWSTR psz;
            psz = (PWSTR) glistProject.Get(i);
            add.pszText = psz;

            pControl->Message (ESCM_COMBOBOXADD, &add);
         }

      }
      break;   // make sure to break and continue on with other init page

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName || _wcsicmp(p->pControl->m_pszName, L"remove"))
            break;

         // make sure the list of projects is loaded
         ProjectFilteredList ();

         // get the selection
         PCEscControl   pControl;
         DWORD dwIndex;
         dwIndex = (DWORD)-1;
         pControl = pPage->ControlFind (gszProjectRemove);
         if (pControl) {
            dwIndex = (DWORD) pControl->AttribGetInt (gszCurSel);
         }

         // if nothing to remove then say so
         if (dwIndex >= glistProject.Num()) {
            pPage->MBInformation (L"You must have projects before you can remove them.");
            return FALSE;
         }

         // verify
         if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove the selected project?",
            L"The project information will still be available through search, "
            L"but you won't be able to access it directly from the Project page."))
            return FALSE;

         // remove
         PCMMLNode   pMajor;
         pMajor = FindMajorSection (gszProjectListNode);
         if (!pMajor) {
            pPage->MBError (gszWriteError);
            return FALSE;  // unexpected error
         }

         DWORD dwNode;
         dwNode =FilteredIndexToDatabase (dwIndex, &glistProject, &glistProjectID);

         if (!RemoveFilteredList (dwNode, pMajor, gszProjectNode,
            &glistProject, &glistProjectID)) {
               pPage->MBError (gszWriteError);
               gpData->NodeRelease(pMajor);
               return FALSE;  // unexpected error
         }
         gpData->NodeRelease(pMajor);
         gpData->Flush();

         // use TTS to speak
         EscChime (ESCCHIME_INFORMATION);
         EscSpeak (L"Project removed.");

         // go to the main project page
         pPage->Exit(L"r:145");
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}






/*****************************************************************************
VerifyCurrentlyViewing - This function looks at gdwProjectView (0 for yourself,
+ for person, -1 for everyone) and decides if it's a person actually working
on this project. If it isn't, it sets gdwProjectView to -1. Also, this returns
TRUE if more than one person is working on the project, FALSE if only one is.
*/
BOOL VerifyCurrentlyViewing (void)
{
   HANGFUNCIN;
   BOOL fFoundMoreThanOne = FALSE;
   BOOL fFoundPerson = FALSE;
   DWORD i;
   PCProjectTask  pTask;
   DWORD dwNum;
   dwNum = gCurProject.m_listTasks.Num();
   for (i = 0; i < dwNum; i++) {
      pTask = *((PCProjectTask*) gCurProject.m_listTasks.Get(i));

      if (pTask->m_dwAssignedTo)
         fFoundMoreThanOne = TRUE;
      if (pTask->m_dwAssignedTo == gdwProjectView)
         fFoundPerson = TRUE;
      if (fFoundPerson && fFoundMoreThanOne)
         return fFoundMoreThanOne;  // found then mentioned person, so done
   }

   // if get here then didn't find, so set to -1
   if (!fFoundPerson)
      gdwProjectView = -1;
   return fFoundMoreThanOne;

}

/*****************************************************************************
ProjectViewPage - Override page callback.
*/
BOOL ProjectViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // BUGFIX: set the title
         pPage->m_pWindow->TitleSet ((PWSTR)gCurProject.m_memName.p);

         // reset the move mode
         gdwMoveMode = 0;  // no move mode

         // set the days per week
         PCEscControl   pControl;
         pControl = pPage->ControlFind (gszDaysPerWeek);
         if (pControl) {
            DWORD dwDays;
            if (gdwProjectView && (gdwProjectView != (DWORD)-1)) {
               dwDays = 5;

               // find the person
               DWORD k;
               for (k = 0; k < gCurProject.m_listDaysPerPerson.Num(); k++) {
                  PDAYSPERPERSON p = (PDAYSPERPERSON) gCurProject.m_listDaysPerPerson.Get(k);
                  if (p->dwNode == gdwProjectView) {
                     dwDays = p->dwDays;
                     break;
                  }
               }
            }
            else
               dwDays = gCurProject.m_dwDaysPerWeek;
            pControl->AttribSetInt (gszCurSel, dwDays - 1);
         }
         JournalControlSet (pPage, &gCurProject);

         pControl = pPage->ControlFind (gszAssignedTo);
         if (pControl) {
            // else, add
            PWSTR psz;
            psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(gdwProjectView));
            if (gdwProjectView == 0)
               psz = L"Yourself";
            else if (!psz || (gdwProjectView == (DWORD)-1))
               psz = L"Everyone";

            ESCMCOMBOBOXSELECTSTRING ss;
            memset (&ss, 0, sizeof(ss));
            ss.fExact = TRUE;
            ss.iStart = -1;
            ss.psz = psz;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &ss);
         }

         // BUGFIX - SHow if disabled/enabled
         pControl = pPage->ControlFind (gszDisabled);
         if (pControl) {
            pControl->AttribSetBOOL (gszChecked, gCurProject.m_fDisabled);
         }


         // need to set up this list of task names & sub-project names
         // so can use in filterd list
         gCurProject.GeneratedFilteredList ();
         gCurProject.CalculateSchedule ();
      }
      break;   // make sure to break and continue on with other init page


   case ESCM_DESTRUCTOR:
      {
         // make sure the schedule is calculated
         gCurProject.CalculateSchedule ();
      }
      break; // fall through


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PROJECTNAME")) {
            p->pszSubString = (PWSTR) gCurProject.m_memName.p;
            return TRUE;
         }

         // only go beyond this point if have task list
         if (!_wcsicmp(p->pszSubName, L"CURRENTPERSON")) {
            MemZero (&gMemTemp);
            PWSTR psz;
            psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(gdwProjectView));
            if (psz) {
               MemCat (&gMemTemp, L"does ");
               MemCatSanitize (&gMemTemp, psz);
            }
            else
               MemCat (&gMemTemp, L"do you");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TASKLIST")) {
            // if there aren't any tasks then done
            if (!gCurProject.m_listTasks.Num()) {
               p->pszSubString = L"";
               return TRUE;
            }

            // else there are tasks, so display the info
            MemZero (&gMemTemp);

            // if there's more than one person then have drop-down asking who to show
            BOOL fMoreThanOne;
            if (fMoreThanOne = VerifyCurrentlyViewing()) {
               MemCat (&gMemTemp, L"<br>Show tasks assigned to:</br>");
               MemCat (&gMemTemp, L"<p align=right><bold><combobox sort=true cbheight=100 width=50% name=assignedto>");
               MemCat (&gMemTemp, L"<elem name=Everyone data=-1><bold>Everyone</bold></elem>");
               MemCat (&gMemTemp, L"<elem name=Yourself data=0><italic>Yourself</italic></elem>");

               CListFixed cl;
               cl.Init (sizeof(DWORD));
               DWORD i, j;
               PCProjectTask  pTask;
               DWORD dwNum;
               dwNum = gCurProject.m_listTasks.Num();
               for (i = 0; i < dwNum; i++) {
                  pTask = *((PCProjectTask*) gCurProject.m_listTasks.Get(i));
                  if (!pTask->m_dwAssignedTo)
                     continue;

                  // see if on list already
                  for (j = 0; j < cl.Num(); j++) {
                     if ( (*(DWORD*)cl.Get(j)) == pTask->m_dwAssignedTo)
                        break;   // already have
                  }
                  if (j < cl.Num())
                     continue;   // already on list

                  // else, add
                  PWSTR psz;
                  psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pTask->m_dwAssignedTo));
                  if (!psz)
                     continue;

                  MemCat (&gMemTemp, L"<elem name=\"");
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"\" data=");
                  MemCat (&gMemTemp, (int) pTask->m_dwAssignedTo);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"</elem>");

                  // dont forget this
                  cl.Add (&pTask->m_dwAssignedTo);
               }

               MemCat (&gMemTemp, L"</combobox></bold></p>");
            }

            if (gfMicroHelp) {
               MemCat (&gMemTemp, L"<xbr/>");
               MemCat (&gMemTemp, L"<p>Click on a task to view, edit, delete it, or mark it as being completed.</p>");
            }
            MemCat (&gMemTemp, L"<xlisttable><tr>");
            MemCat (&gMemTemp, L"<td width=50%% bgcolor=#004000><bold><font color=#ffffff>Task</font></bold></td>");
            MemCat (&gMemTemp, L"<td width=25%% bgcolor=#004000><bold><font color=#ffffff>Sub-project</font></bold></td>");
            MemCat (&gMemTemp, L"<td width=25%% bgcolor=#004000><bold><font color=#ffffff>Expected Timeframe</font></bold></td>");
            MemCat (&gMemTemp, L"</tr>");

            // all the tasks
            DWORD i;
            PCProjectTask  pTask;
            DWORD dwLastMonth;
            DWORD dwNum;
            dwNum = gCurProject.m_listTasks.Num();
            dwLastMonth = (DWORD)-1;
            for (i = 0; i < dwNum; i++) {
               pTask = *((PCProjectTask*) gCurProject.m_listTasks.Get(i));

               // if we're looking for tasks specifically assigned to people and
               // this task isn't the right person then skip
               if ((gdwProjectView != (DWORD)-1) && (pTask->m_dwAssignedTo != gdwProjectView))
                  continue;

               // if this is a different month than the last one then finish that up
               DWORD dwCurMonth;
               dwCurMonth = MONTHFROMDFDATE (pTask->m_dwDateStarted);
               if (dwLastMonth != dwCurMonth) {
                  if (dwLastMonth != (DWORD) -1)
                     MemCat (&gMemTemp, L"</xtrmonth>");
                  MemCat (&gMemTemp, L"<xtrmonth>");

                  // month string
                  MemCat (&gMemTemp, L"<tr><td align=right><big><bold>");
                  if (gfFullColor)
                     MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#20a020 bcolor=#60d060/>");
                  MemCat (&gMemTemp, gaszMonth[dwCurMonth-1]);
                  MemCat (&gMemTemp, L" ");
                  MemCat (&gMemTemp, (int) YEARFROMDFDATE(pTask->m_dwDateStarted));
                  MemCat (&gMemTemp, L"</bold></big></td></tr>");
                  dwLastMonth = dwCurMonth;
               }

               // if there's a spacer then enter this
               // BUGFIX - Only show idle time if not viewing all
               if (((fMoreThanOne && (gdwProjectView != -1)) || !fMoreThanOne) && pTask->m_dwDeadTimeBefore) {
                  MemCat (&gMemTemp, L"<tr><xtdtasknolink>");
                  MemCat (&gMemTemp, L"<xSpaceArrow/><xSpaceArrow/>  <font color=#800000>Idle time</font>");
                  MemCat (&gMemTemp, L"</xtdtasknolink>");
                  MemCat (&gMemTemp, L"<xtdsubproject/>");
                  MemCat (&gMemTemp, L"<xtdcompleted><font color=#800000>");
                  MemCat (&gMemTemp, (int) pTask->m_dwDeadTimeBefore);
                  MemCat (&gMemTemp, L" days</font></xtdcompleted></tr>");
               }

               MemCat (&gMemTemp, L"<tr><xtdtaskNoLink>");

               // arrows
               if (i) {
                  // up arrow
                  MemCat (&gMemTemp, L"<xUpArrow href=u:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");
               }
               else {
                  MemCat (&gMemTemp, L"<xSpaceArrow/>");
               }
               if ((i+1) < dwNum) {
                  // down arrow
                  MemCat (&gMemTemp, L"<xDownArrow href=d:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");
               }
               else {
                  MemCat (&gMemTemp, L"<xSpaceArrow/>");
               }

               // link
               MemCat (&gMemTemp, L"  <a href=!");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">");

               // if there's a conflict then red and big
               if (pTask->m_dwConflict)
                  MemCat (&gMemTemp, L"<font color=#ff0000>");

               // task name
               PWSTR psz;
               psz = (PWSTR) pTask->m_memName.p;
               if (psz && psz[0])
                  MemCatSanitize (&gMemTemp, psz);

               // if there's a conflict then red and big
               if (pTask->m_dwConflict)
                  MemCat (&gMemTemp, L"</font>");


               // hover help
               MemCat (&gMemTemp, L"<xHoverHelp>");

               // description if there is one
               psz = (PWSTR) pTask->m_memDescription.p;
               if (psz && psz[0]) {
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"<p/>");
               }

               // duration
               WCHAR szTemp[64];
               swprintf (szTemp, L"Duration: <bold>%g days of work</bold>", (double) pTask->m_dwDuration / 100.0);
               MemCat (&gMemTemp, szTemp);

               // start/end after/before date
               if (pTask->m_dwDateAfter) {
                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_DATEAFTER)
                     MemCat (&gMemTemp, L"<font color=#ff0000>");

                  WCHAR szTemp[64];
                  MemCat (&gMemTemp, L"<br/>This cannot be started until <bold>after ");
                  DFDATEToString (pTask->m_dwDateAfter, szTemp);
                  MemCat (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L"</bold>.");

                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_DATEAFTER)
                     MemCat (&gMemTemp, L"</font>");
               }
               if (pTask->m_dwDateBefore) {
                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_DATEBEFORE)
                     MemCat (&gMemTemp, L"<font color=#ff0000>");

                  WCHAR szTemp[64];
                  MemCat (&gMemTemp, L"<br/>This task must be finished <bold>before ");
                  DFDATEToString (pTask->m_dwDateBefore, szTemp);
                  MemCat (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L"</bold>.");

                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_DATEBEFORE)
                     MemCat (&gMemTemp, L"</font>");
               }

               // start/end after/before task
               if (pTask->m_dwIDAfter) {
                  PCProjectTask pLink;
                  pLink = gCurProject.TaskGetByID (pTask->m_dwIDAfter);
                  if (pLink) {
                     // if this is teh cause of conflic then red
                     if (pTask->m_dwConflict & CONFLICT_IDAFTER)
                        MemCat (&gMemTemp, L"<font color=#ff0000>");

                     MemCat (&gMemTemp, L"<br/>This cannot be started for <bold>");
                     MemCat (&gMemTemp, (int) pTask->m_dwDaysAfter);
                     MemCat (&gMemTemp, L" days after the \"");
                     MemCatSanitize (&gMemTemp, (PWSTR) pLink->m_memName.p);
                     MemCat (&gMemTemp, L"\"</bold> task.");

                     // if this is teh cause of conflic then red
                     if (pTask->m_dwConflict & CONFLICT_IDAFTER)
                        MemCat (&gMemTemp, L"</font>");

                  }
               }
               if (pTask->m_dwIDBefore) {
                  PCProjectTask pLink;
                  pLink = gCurProject.TaskGetByID (pTask->m_dwIDBefore);
                  if (pLink) {
                     // if this is teh cause of conflic then red
                     if (pTask->m_dwConflict & CONFLICT_IDBEFORE)
                        MemCat (&gMemTemp, L"<font color=#ff0000>");

                     MemCat (&gMemTemp, L"<br/>This must be started <bold>");
                     MemCat (&gMemTemp, (int) pTask->m_dwDaysBefore);
                     MemCat (&gMemTemp, L" days before the \"");
                     MemCatSanitize (&gMemTemp, (PWSTR) pLink->m_memName.p);
                     MemCat (&gMemTemp, L"\"</bold> task.");

                     // if this is teh cause of conflic then red
                     if (pTask->m_dwConflict & CONFLICT_IDBEFORE)
                        MemCat (&gMemTemp, L"</font>");

                  }
               }

               // end hover help, task, and start subproject
               MemCat (&gMemTemp, L"</xHoverHelp></a>");
               
               // SHow # days
               swprintf (szTemp, (pTask->m_dwDuration == 100) ? L" (%g day)" : L" (%g days)", (double) pTask->m_dwDuration / 100.0);
               MemCat (&gMemTemp, szTemp);

               // finish off task
               MemCat (&gMemTemp, L"</xtdtasknolink><xtdsubproject>");

               // subproject name
               psz = (PWSTR) pTask->m_memSubProject.p;
               if (psz && psz[0])
                  MemCatSanitize (&gMemTemp, psz);
               else
                  MemCat (&gMemTemp, L"<italic>None specified</italic>");

               // Show who it's assigned to
               if (pTask->m_dwAssignedTo) {
                  MemCat (&gMemTemp, L"<br/>Assigned to <a href=v:");
                  MemCat (&gMemTemp, pTask->m_dwAssignedTo);
                  MemCat (&gMemTemp, L">");
                  PWSTR psz;
                  psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pTask->m_dwAssignedTo));
                  MemCatSanitize (&gMemTemp, psz ? psz : L"Unknown");
                  MemCat (&gMemTemp, L"</a>");
               }

               // end sub project and show when completed
               MemCat (&gMemTemp, L"</xtdsubproject><xtdcompleted>");

               // date when finished
               // if there's a conflict then red and big
               if (pTask->m_dwConflict)
                  MemCat (&gMemTemp, L"<font color=#ff0000>");

               // BUGFIX - Show task duration
               if (pTask->m_dwDateStarted != pTask->m_dwDatePredicted) {
                  DFDATEToString (pTask->m_dwDateStarted, szTemp);
                  MemCat (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L" to<br/>");
               }

               DFDATEToString (pTask->m_dwDatePredicted, szTemp);
               MemCat (&gMemTemp, szTemp);
               if (pTask->m_dwConflict)
                  MemCat (&gMemTemp, L"</font>");


               // finish with task
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // finish up the month
            if (dwLastMonth != (DWORD) -1)
               MemCat (&gMemTemp, L"</xtrmonth>");

            // finnish up the table
            MemCat (&gMemTemp, L"</xlisttable>");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TASKLISTCOMPLETED")) {
            // if there aren't any tasks then done
            if (!gCurProject.m_listTasksCompleted.Num()) {
               p->pszSubString = L"";
               return TRUE;
            }

            // else there are tasks, so display the info
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<xbr/>");
            MemCat (&gMemTemp, L"<xSectionTitle>Completed tasks</xSectionTitle>");
            if (gfMicroHelp) {
               MemCat (&gMemTemp, L"<p>The tasks you've completed are listed below.</p>");
            }
            MemCat (&gMemTemp, L"<xlisttable><tr>");
            MemCat (&gMemTemp, L"<td width=50%% bgcolor=#004000><bold><font color=#ffffff>Completed Task</font></bold></td>");
            MemCat (&gMemTemp, L"<td width=25%% bgcolor=#004000><bold><font color=#ffffff>Sub-project</font></bold></td>");
            MemCat (&gMemTemp, L"<td width=25%% bgcolor=#004000><bold><font color=#ffffff>Completion Date</font></bold></td>");
            MemCat (&gMemTemp, L"</tr>");

            // all the tasks
            DWORD i;
            PCProjectTask  pTask;
            DWORD dwLastMonth;
            DWORD dwNum;
            dwNum = gCurProject.m_listTasksCompleted.Num();
            dwLastMonth = (DWORD)-1;
            for (i = 0; i < dwNum; i++) {
               pTask = *((PCProjectTask*) gCurProject.m_listTasksCompleted.Get(i));

               // if this is a different month than the last one then finish that up
               DWORD dwCurMonth;
               dwCurMonth = MONTHFROMDFDATE (pTask->m_dwDateCompleted);
               if (dwLastMonth != dwCurMonth) {
                  if (dwLastMonth != (DWORD) -1)
                     MemCat (&gMemTemp, L"</xtrmonth>");
                  MemCat (&gMemTemp, L"<xtrmonth>");

                  // month string
                  MemCat (&gMemTemp, L"<tr><td align=right><big><bold>");
                  if (gfFullColor)
                     MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#20a020 bcolor=#60d060/>");
                  MemCat (&gMemTemp, gaszMonth[dwCurMonth-1]);
                  MemCat (&gMemTemp, L" ");
                  MemCat (&gMemTemp, (int) YEARFROMDFDATE(pTask->m_dwDateCompleted));
                  MemCat (&gMemTemp, L"</bold></big></td></tr>");
                  dwLastMonth = dwCurMonth;
               }

               MemCat (&gMemTemp, L"<tr><xtdtaskNoLink>");

               // link
               MemCat (&gMemTemp, L"  <a>");

               // task name
               PWSTR psz;
               psz = (PWSTR) pTask->m_memName.p;
               if (psz && psz[0])
                  MemCatSanitize (&gMemTemp, psz);

               // hover help
               MemCat (&gMemTemp, L"<xHoverHelp>");

               // description if there is one
               psz = (PWSTR) pTask->m_memDescription.p;
               if (psz && psz[0]) {
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"<p/>");
               }

               // duration
               WCHAR szTemp[64];
               swprintf (szTemp, L"Duration: <bold>%g days of work</bold>", (double) pTask->m_dwDuration / 100.0);
               MemCat (&gMemTemp, szTemp);

               // end hover help, task, and start subproject
               MemCat (&gMemTemp, L"</xHoverHelp></a>");
               
               // SHow # days
               swprintf (szTemp, (pTask->m_dwDuration == 100) ? L" (%g day)" : L" (%g days)", (double) pTask->m_dwDuration / 100.0);
               MemCat (&gMemTemp, szTemp);

               // finish off task
               MemCat (&gMemTemp, L"</xtdtasknolink><xtdsubproject>");

               // subproject name
               psz = (PWSTR) pTask->m_memSubProject.p;
               if (psz && psz[0])
                  MemCatSanitize (&gMemTemp, psz);
               else
                  MemCat (&gMemTemp, L"<italic>None specified</italic>");

               // Show who it's assigned to
               if (pTask->m_dwAssignedTo) {
                  MemCat (&gMemTemp, L"<br/>Assigned to <a href=v:");
                  MemCat (&gMemTemp, pTask->m_dwAssignedTo);
                  MemCat (&gMemTemp, L">");
                  PWSTR psz;
                  psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pTask->m_dwAssignedTo));
                  MemCatSanitize (&gMemTemp, psz ? psz : L"Unknown");
                  MemCat (&gMemTemp, L"</a>");
               }

               // end sub project and show when completed
               MemCat (&gMemTemp, L"</xtdsubproject><xtdcompleted>");

               // date when finished
               DFDATEToString (pTask->m_dwDateCompleted, szTemp);
               MemCat (&gMemTemp, szTemp);

               // finish with task
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // finish up the month
            if (dwLastMonth != (DWORD) -1)
               MemCat (&gMemTemp, L"</xtrmonth>");

            // finnish up the table
            MemCat (&gMemTemp, L"</xlisttable>");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;   // default behavior

   case ESCN_FILTEREDLISTCHANGE:
      {
         // always store away the current setting
         JournalControlGet (pPage, &gCurProject);

      }
      break;   // call through

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         
         // only care about assigned to
         if (!_wcsicmp(p->pControl->m_pszName, gszAssignedTo)) {
            DWORD dwVal;
            dwVal = p->pszData ? (DWORD)_wtoi (p->pszData) : (DWORD)-1;
            if (dwVal != gdwProjectView) {
               gdwProjectView = dwVal;
               pPage->Exit (gszRedoSamePage);
               return TRUE;
            }
            break;
         }

         if (_wcsicmp(p->pControl->m_pszName, gszDaysPerWeek))
            break;

         // see how many days supposed to have
         PDAYSPERPERSON pp;
         DWORD dwDays;
         pp = NULL;
         if (gdwProjectView && (gdwProjectView != (DWORD)-1)) {
            dwDays = 5;

            // find the person
            DWORD k;
            for (k = 0; k < gCurProject.m_listDaysPerPerson.Num(); k++) {
               pp = (PDAYSPERPERSON) gCurProject.m_listDaysPerPerson.Get(k);
               if (pp->dwNode == gdwProjectView) {
                  dwDays = pp->dwDays;
                  break;
               }
            }
            if (k >= gCurProject.m_listDaysPerPerson.Num())
               pp = NULL;
         }
         else
            dwDays = gCurProject.m_dwDaysPerWeek;

         // if no change to the selection then who cares
         if (p->dwCurSel+1 == dwDays)
            break;

         // else, change
         if (gdwProjectView && (gdwProjectView != (DWORD)-1)) {
            if (pp)
               pp->dwDays = p->dwCurSel+1;
            else {
               DAYSPERPERSON dp;
               dp.dwDays = p->dwCurSel+1;
               dp.dwNode = gdwProjectView;
               gCurProject.m_listDaysPerPerson.Add (&dp);
            }
         }
         else {
            gCurProject.m_dwDaysPerWeek = p->dwCurSel+1;
         }
         gCurProject.m_fDirty = TRUE;

         // always store away the current setting
         JournalControlGet (pPage, &gCurProject);

         // and refresh everything since just recalculated dates
         pPage->Exit (gszRedoSamePage);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // always store away the current setting
         JournalControlGet (pPage, &gCurProject);

         if (!p->psz)
            break;

         // if it's a specialy link for editing
         if (p->psz[0] == L'!') {
            DWORD dwIndex;
            dwIndex = 0;
            AttribToDecimal (p->psz + 1, (int*) &dwIndex);

            // BUGFIX - If move mode 1 then store this info away
            if (gdwMoveMode == 1) {
               gdwMoveTaskIndex = dwIndex;
               gdwMoveMode = 2;

               pPage->MBSpeakInformation (L"Click on the task to insert before.");
               return TRUE;
            }
            else if (gdwMoveMode == 2) {
               gdwMoveMode = 0;

               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Task moved.");

               PCProjectTask pTask;

               // remove the one that moving from the list
               pTask = gCurProject.TaskGetByIndex(gdwMoveTaskIndex);
               gCurProject.m_listTasks.Remove((DWORD) gdwMoveTaskIndex);

               // take into account that just deleted
               if (dwIndex > gdwMoveTaskIndex)
                  dwIndex--;  // take into account that just deleted before what moving to

               // insert or append as see fit
               if (dwIndex < (int) gCurProject.m_listTasks.Num())
                  gCurProject.m_listTasks.Insert ((DWORD)dwIndex, &pTask);
               else
                  gCurProject.m_listTasks.Add (&pTask);


               // dirty
               gCurProject.m_fDirty = TRUE;
               gCurProject.Flush();
               pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh
               return TRUE;

            }

            // get the task
            PCProjectTask pTask;
            pTask = gCurProject.TaskGetByIndex (dwIndex);
            if (!pTask)
               break;   // dont know what to do

            if (ProjectTaskShowEditUI(pPage, pTask))
               pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh

            return TRUE;
         }
         else if ( ((p->psz[0] == L'u') || (p->psz[0] == L'd')) && (p->psz[1] == L':')) {
            // up/down arrow
            int iIndex, iMoveTo;
            int iDir = (p->psz[0] == L'u') ? -1 : 1;
            iIndex = 0;
            AttribToDecimal (p->psz + 2, &iIndex);

            // find out where to move to
            PCProjectTask pTask;
            DWORD dwPerson;
            pTask = gCurProject.TaskGetByIndex(iIndex);
            dwPerson = pTask->m_dwAssignedTo;
#if 0 // BUGFIX - when move tasks up/down, even when viewing all, move with respect to tasks assigned to user
            if (gdwProjectView == (DWORD)-1) {
               iMoveTo = iIndex + iDir;
            }
            else {
#endif // 0
               // movement is a bit more complex. Move to before/after the next
               // task with the same user
               iMoveTo = iIndex;
               while (TRUE) {
                  iMoveTo += iDir;
                  if (iMoveTo < 0) {
                     iMoveTo = 0;
                     break;
                  }
                  if ((DWORD) iMoveTo >= gCurProject.m_listTasks.Num()) {
                     break;
                  }
                  pTask = gCurProject.TaskGetByIndex(iMoveTo);

                  if (pTask->m_dwAssignedTo != dwPerson)
                     continue;

                  // else match
                  break;
               }
#if 0
            }
#endif //0

            // remove the one that moving from the list
            pTask = gCurProject.TaskGetByIndex(iIndex);
            gCurProject.m_listTasks.Remove((DWORD) iIndex);
            //if (iMoveTo > iIndex)
            //   iMoveTo--;  // take into account that just deleted before what moving to

            // insert or append as see fit
            if (iMoveTo < (int) gCurProject.m_listTasks.Num())
               gCurProject.m_listTasks.Insert ((DWORD)iMoveTo, &pTask);
            else
               gCurProject.m_listTasks.Add (&pTask);


            // dirty
            gCurProject.m_fDirty = TRUE;
            gCurProject.Flush();
            pPage->Exit (gszRedoSamePage); // was successfully edited, so refresh

            return TRUE;
         }
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         
         if (!_wcsicmp(p->pControl->m_pszName, L"addnewtask")) {
            if (ProjectTaskShowAddUI(pPage)) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Task added.");
               pPage->Exit (gszRedoSamePage); // was successfully added, so refresh
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addseveraltasks")) {
            if (ProjectTaskShowAddMultipleUI(pPage)) {
               EscChime (ESCCHIME_INFORMATION);
               EscSpeak (L"Tasks added.");
               pPage->Exit (gszRedoSamePage); // was successfully added, so refresh
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"movetaskfar")) {
            gdwMoveMode = 1;

            pPage->MBSpeakInformation (L"Click on the task you wish to move.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDisabled)) {
            gCurProject.m_fDisabled = p->pControl->AttribGetBOOL(gszChecked);
            gCurProject.m_fDirty = TRUE;
            gCurProject.Flush();
            return TRUE;
         }

      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
ProjectSetView - Set the data file to view. Called by Main.cpp when it
sees "v:XXXX".

inputs
   DWORD    dwNode - Node ID from database
returns
   BOOL - TRUE if succede
*/
BOOL ProjectSetView (DWORD dwNode)
{
   HANGFUNCIN;
   return gCurProject.Init(dwNode);
}

/******************************************************************************
ProjectGetTaskList - Returns the project task list

returns
   PCListVariable - list
*/
PCListVariable ProjectGetTaskList (void)
{
   HANGFUNCIN;
   return &gCurProject.m_listFilteredTasks;
}

/******************************************************************************
ProjectGetSubProjectList - Returns the project task list

returns
   PCListVariable - list
*/
PCListVariable ProjectGetSubProjectList (void)
{
   HANGFUNCIN;
   return &gCurProject.m_listFilteredSubProjects;
}

/*****************************************************************************
ProjectNewSubProjectPage - Override page callback.
*/
BOOL ProjectNewSubProjectPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
         pControl = pPage->ControlFind(gszSubProject);
         if (pControl)
            pControl->AttribGet (gszText, szTemp+1, sizeof(szTemp)-2, &dwNeeded);
         if (!szTemp[1]) {
            pPage->MBWarning (L"You must type in a sub-project name or press Cancel.");
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
ProjectAddSubProject - Adds a subproject to the list.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   DWORD - New subproject index. -1 if didn't add.
*/
DWORD ProjectAddSubProject (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPROJECTNEWSUBPROJECT, ProjectNewSubProjectPage);

   if (!pszRet || (pszRet[0] != L'!'))
      return (DWORD) -1;   // canceled

   return gCurProject.m_listFilteredSubProjects.Add(pszRet+1, (wcslen(pszRet+1)+1)*2);
}


/*************************************************************************
IsProjectDisabled - Returns TRUE if the current project is disabled and
should not be shown on the to-do lists.
*/
BOOL IsProjectDisabled (void)
{
   HANGFUNCIN;
   return (BOOL) gCurProject.m_fDisabled;
}

/*************************************************************************
ProjectSummary - Make a short summary of all the project work for
the "Today" page.

BUGFIX - Added endDate, startDate

inputs
   DFDATE      endDate - Don't show tasks occurring after this date. If 0 then show
               about a week after today
   DFDATE      startDate - Don't show tasks ocurring before this date. If 0 then show
               all tasks
returns
   PWSTR - Pointer to the MML to insert. Usually gMemTemp.p
*/
PWSTR ProjectSummary (DFDATE endDate, DFDATE startDate)
{
   HANGFUNCIN;
   // if there aren't any projects then return empty string
   if ((DWORD)-1 == ProjectIndexToDatabase(0))
      return L"";

   // else, we have projects. Show them off
   MemZero (&gMemTemp);
   //MemCat (&gMemTemp, L"<xbr/>");
   //MemCat (&gMemTemp, L"<xSectionTitle>Project Summary</xSectionTitle>");
   //MemCat (&gMemTemp, L"<p>This shows the list of project tasks you're scheduled to work on this week.</p>");
   MemCat (&gMemTemp, L"<xlisttable>");
   MemCat (&gMemTemp, L"<xtrheader><a href=r:145 color=#c0c0ff>Projects</a></xtrheader>");
   //MemCat (&gMemTemp, L"<tr>");
   //MemCat (&gMemTemp, L"<td width=50%% bgcolor=#004000><bold><font color=#ffffff>Task</font></bold></td>");
   //MemCat (&gMemTemp, L"<td width=25%% bgcolor=#004000><bold><font color=#ffffff>Sub-project</font></bold></td>");
   //MemCat (&gMemTemp, L"<td width=25%% bgcolor=#004000><bold><font color=#ffffff>Expected Completion</font></bold></td>");
   //MemCat (&gMemTemp, L"</tr>");

   // loop
   DWORD dwProject, dwDB;
   DFDATE   nextweek;

   // BUGFIX - If it's all empty then totally skip
   BOOL fAllEmpty;
   fAllEmpty = TRUE;

   nextweek = MinutesToDFDATE (DFDATEToMinutes (Today()) + 60 * 24 * 7);
   for (dwProject = 0; ; dwProject++) {
      // BUGFIX - If nothing displayed then don't display project
      DWORD dwMemLocation;
      BOOL fEmpty;
      fEmpty = TRUE;
      dwMemLocation = gMemTemp.m_dwCurPosn;

      dwDB = ProjectIndexToDatabase (dwProject);
      if (dwDB == (DWORD)-1)
         break;

      // load the project
      if (!gCurProject.Init (dwDB))
         break;   // couldnt load for whatever reason

      // BUGFIX: If project disabled then skip
      if (gCurProject.m_fDisabled)
         continue;

      // show project title
      MemCat (&gMemTemp, L"<xtrmonth>");

      // month string
      MemCat (&gMemTemp, L"<tr><td align=right><big><bold>");
      if (gfFullColor)
         MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#20a020 bcolor=#60d060/>");
      MemCat (&gMemTemp, L"<a color=#000080 href=v:");
      MemCat (&gMemTemp, (int) dwDB);
      MemCat (&gMemTemp, L">");
      MemCatSanitize (&gMemTemp, (PWSTR) gCurProject.m_memName.p);
      MemCat (&gMemTemp, L"<xHoverHelp>");
      MemCatSanitize (&gMemTemp, (PWSTR) gCurProject.m_memDescription.p);
      MemCat (&gMemTemp, L"</xHoverHelp>");
      MemCat (&gMemTemp, L"</a>");
      MemCat (&gMemTemp, L"</bold></big></td></tr>");

      // all the tasks
      DWORD i;
      PCProjectTask  pTask;
      DWORD dwLastMonth;
      DWORD dwNum;
      dwNum = gCurProject.m_listTasks.Num();
      dwLastMonth = (DWORD)-1;
      for (i = 0; i < dwNum; i++) {
         pTask = *((PCProjectTask*) gCurProject.m_listTasks.Get(i));

         // if the task if before time then skip
         if (pTask->m_dwDatePredicted < startDate)
            continue;

         // if the task ends after then end date then skip.
         // however, only do this if we haven't already skipped one because
         // it's ended already. This makes sure that if we show a date mid-way
         // through the task it lists the task
         if (endDate && (pTask->m_dwDateStarted > endDate))
            continue;

         // BUGFIX - Changed a break to a continue
         // if the date > nextweek then make sure that the next one will be last
         if (!endDate && (pTask->m_dwDateStarted > nextweek))
            continue;

         // we displayed somethnig here
         fEmpty = FALSE;

#if 0 // BUGFIX - Don't show idle time if showing for everyone
         // if there's a spacer then enter this
         if (pTask->m_dwDeadTimeBefore) {
            MemCat (&gMemTemp, L"<tr><xtdtasknolink>");
            MemCat (&gMemTemp, L"<font color=#800000>Idle time</font>");
            MemCat (&gMemTemp, L"</xtdtasknolink>");
            MemCat (&gMemTemp, L"<xtdsubproject/>");
            MemCat (&gMemTemp, L"<xtdcompleted><font color=#800000>");
            MemCat (&gMemTemp, (int) pTask->m_dwDeadTimeBefore);
            MemCat (&gMemTemp, L" days</font></xtdcompleted></tr>");
         }
#endif // 0

         MemCat (&gMemTemp, L"<tr><xtdtaskNoLink>");

         // hover help
         // BUGFIX : Actually link to task
         if (!gfPrinting) {
            MemCat (&gMemTemp, L"<a href=pt:");
            MemCat (&gMemTemp, dwDB);
            MemCat (&gMemTemp, L":");
            MemCat (&gMemTemp, pTask->m_dwID);
            MemCat (&gMemTemp, L">");
         }

         // if there's a conflict then red and big
         if (pTask->m_dwConflict)
            MemCat (&gMemTemp, L"<font color=#ff0000>");

         // task name
         PWSTR psz;
         psz = (PWSTR) pTask->m_memName.p;
         if (psz && psz[0])
            MemCatSanitize (&gMemTemp, psz);

         // if there's a conflict then red and big
         if (pTask->m_dwConflict)
            MemCat (&gMemTemp, L"</font>");


         WCHAR szTemp[64];
         if (!gfPrinting) {
            // hover help
            MemCat (&gMemTemp, L"<xHoverHelp>");

            // description if there is one
            psz = (PWSTR) pTask->m_memDescription.p;
            if (psz && psz[0]) {
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"<p/>");
            }

            // duration
            swprintf (szTemp, L"Duration: <bold>%g days of work</bold>", (double) pTask->m_dwDuration / 100.0);
            MemCat (&gMemTemp, szTemp);

            // start/end after/before date
            if (pTask->m_dwDateAfter) {
               // if this is teh cause of conflic then red
               if (pTask->m_dwConflict & CONFLICT_DATEAFTER)
                  MemCat (&gMemTemp, L"<font color=#ff0000>");

               WCHAR szTemp[64];
               MemCat (&gMemTemp, L"<br/>This cannot be started until <bold>after ");
               DFDATEToString (pTask->m_dwDateAfter, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</bold>.");

               // if this is teh cause of conflic then red
               if (pTask->m_dwConflict & CONFLICT_DATEAFTER)
                  MemCat (&gMemTemp, L"</font>");
            }
            if (pTask->m_dwDateBefore) {
               // if this is teh cause of conflic then red
               if (pTask->m_dwConflict & CONFLICT_DATEBEFORE)
                  MemCat (&gMemTemp, L"<font color=#ff0000>");

               WCHAR szTemp[64];
               MemCat (&gMemTemp, L"<br/>This task must be finished <bold>before ");
               DFDATEToString (pTask->m_dwDateBefore, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</bold>.");

               // if this is teh cause of conflic then red
               if (pTask->m_dwConflict & CONFLICT_DATEBEFORE)
                  MemCat (&gMemTemp, L"</font>");
            }

            // start/end after/before task
            if (pTask->m_dwIDAfter) {
               PCProjectTask pLink;
               pLink = gCurProject.TaskGetByID (pTask->m_dwIDAfter);
               if (pLink) {
                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_IDAFTER)
                     MemCat (&gMemTemp, L"<font color=#ff0000>");

                  MemCat (&gMemTemp, L"<br/>This cannot be started for <bold>");
                  MemCat (&gMemTemp, (int) pTask->m_dwDaysAfter);
                  MemCat (&gMemTemp, L" days after the \"");
                  MemCatSanitize (&gMemTemp, (PWSTR) pLink->m_memName.p);
                  MemCat (&gMemTemp, L"\"</bold> task.");

                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_IDAFTER)
                     MemCat (&gMemTemp, L"</font>");

               }
            }
            if (pTask->m_dwIDBefore) {
               PCProjectTask pLink;
               pLink = gCurProject.TaskGetByID (pTask->m_dwIDBefore);
               if (pLink) {
                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_IDBEFORE)
                     MemCat (&gMemTemp, L"<font color=#ff0000>");

                  MemCat (&gMemTemp, L"<br/>This must be started <bold>");
                  MemCat (&gMemTemp, (int) pTask->m_dwDaysBefore);
                  MemCat (&gMemTemp, L" days before the \"");
                  MemCatSanitize (&gMemTemp, (PWSTR) pLink->m_memName.p);
                  MemCat (&gMemTemp, L"\"</bold> task.");

                  // if this is teh cause of conflic then red
                  if (pTask->m_dwConflict & CONFLICT_IDBEFORE)
                     MemCat (&gMemTemp, L"</font>");

               }
            }

            // end hover help, task, and start subproject
            MemCat (&gMemTemp, L"</xHoverHelp></a>");
         } // !gfPrinting
      
         // SHow # days
         swprintf (szTemp, (pTask->m_dwDuration == 100) ? L" (%g day)" : L" (%g days)", (double) pTask->m_dwDuration / 100.0);
         MemCat (&gMemTemp, szTemp);

         // finish off task
         MemCat (&gMemTemp, L"</xtdtasknolink><xtdsubproject>");

         // subproject name
         psz = (PWSTR) pTask->m_memSubProject.p;
         if (psz && psz[0])
            MemCatSanitize (&gMemTemp, psz);
         else
            MemCat (&gMemTemp, L"<italic>None specified</italic>");

         // Show who it's assigned to
         if (pTask->m_dwAssignedTo) {
            MemCat (&gMemTemp, L"<br/>Assigned to ");
            if (!gfPrinting) {
               MemCat (&gMemTemp, L"<a href=v:");
               MemCat (&gMemTemp, pTask->m_dwAssignedTo);
               MemCat (&gMemTemp, L">");
            }
            PWSTR psz;
            psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pTask->m_dwAssignedTo));
            MemCatSanitize (&gMemTemp, psz ? psz : L"Unknown");
            if (!gfPrinting) {
               MemCat (&gMemTemp, L"</a>");
            }
         }

         // end sub project and show when completed
         MemCat (&gMemTemp, L"</xtdsubproject><xtdcompleted>");

         // date when finished
         // if there's a conflict then red and big
         if (pTask->m_dwConflict)
            MemCat (&gMemTemp, L"<font color=#ff0000>");

         // BUGFIX - Show task duration
         if (pTask->m_dwDateStarted != pTask->m_dwDatePredicted) {
            DFDATEToString (pTask->m_dwDateStarted, szTemp);
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L" to<br/>");
         }

         DFDATEToString (pTask->m_dwDatePredicted, szTemp);
         MemCat (&gMemTemp, szTemp);
         if (pTask->m_dwConflict)
            MemCat (&gMemTemp, L"</font>");


         // finish with task
         MemCat (&gMemTemp, L"</xtdcompleted></tr>");
      }

      // finish up the month
      MemCat (&gMemTemp, L"</xtrmonth>");

      // if it was empty then jump back
      if (fEmpty)
         gMemTemp.m_dwCurPosn = dwMemLocation;
      else
         fAllEmpty = FALSE;
   }

   // finnish up the table
   MemCat (&gMemTemp, L"</xlisttable>");

   return fAllEmpty ? L"" : (PWSTR) gMemTemp.p;
}



// BUGBUG - 2.0 - When complete task ask how many hours (days) it took. Store away
// and show info when user moved mouse over completed task.

// BUGBUG - 2.0 - Analysis - Use hours estimated vs. hours to complete to tell
// person how good at estimating they are

// BUGBUG - 2.0 - option from projectview.mml for "View in printable form" that displays
// black& white, plenty of large text, so that can print to-do list on papers


/*************************************************************************
ProjectMonthEnumerate - Enumerates a months worth of data.

inputs
   DFDATE   date - Use only the month and year field
   PCMem    papMem - Pointer to an arry of 31 PCMem, initially set to NULL.
               If any events occur on that day then the function will create
               a new CMem (which must be freed by the caller), and
               fill it with a MML (null-terminated) text string. The
               MML has <li>TEXT</li> so that these can be put into a list.
   BOOL     fWithLinks - If TRUE, then not only include the text, but include links
               in the text so can jump to the project. BUGFIX - Added to make view
               calendar as list better
returns
   none
*/
void ProjectMonthEnumerate (DFDATE date, PCMem *papMem, BOOL fWithLinks)
{
   HANGFUNCIN;
   // if there aren't any projects then return empty string
   if ((DWORD)-1 == ProjectIndexToDatabase(0))
      return;

   // loop
   DWORD dwProject, dwDB;
   for (dwProject = 0; ; dwProject++) {
      dwDB = ProjectIndexToDatabase (dwProject);
      if (dwDB == (DWORD)-1)
         break;

      // load the project
      if (!gCurProject.Init (dwDB))
         break;   // couldnt load for whatever reason

      // BUGFIX - Don't show disabled projects
      if (gCurProject.m_fDisabled)
         continue;

      // all the tasks
      DWORD i;
      PCProjectTask  pTask;
      DWORD dwLastMonth;
      DWORD dwNum;
      dwNum = gCurProject.m_listTasks.Num();
      dwLastMonth = (DWORD)-1;
      for (i = 0; i < dwNum; i++) {
         pTask = *((PCProjectTask*) gCurProject.m_listTasks.Get(i));

         DFDATE dRange;
         // BUGFIX - show from stared to finished
         for (dRange = pTask->m_dwDateStarted; dRange <= pTask->m_dwDatePredicted; dRange = MinutesToDFDATE (DFDATEToMinutes (dRange) + 60 * 24)) {
            // if before/after month exit
            if ( (MONTHFROMDFDATE(dRange) != MONTHFROMDFDATE(date)) ||
               (YEARFROMDFDATE(dRange) != YEARFROMDFDATE(date)) )
               continue;

            // find the day
            int   iDay;
            iDay = (int) DAYFROMDFDATE(dRange);
            if (!papMem[iDay]) {
               papMem[iDay] = new CMem;
               MemZero (papMem[iDay]);
            }

            // append
            MemCat (papMem[iDay], L"<li>");

            if (pTask->m_dwConflict)
               MemCat (papMem[iDay], L"<font color=#ff0000>");

            MemCat (papMem[iDay], L"<italic>");
            if (fWithLinks) {
               // BUGFIX - Actually link to task
               MemCat (papMem[iDay], L"<a href=v:");
               MemCat (papMem[iDay], (int) dwDB);
               MemCat (papMem[iDay], L">");
            }
            MemCatSanitize (papMem[iDay], (PWSTR) gCurProject.m_memName.p);
            if (fWithLinks)
               MemCat (papMem[iDay], L"</a>");
            MemCat (papMem[iDay], L"</italic>");

            MemCat (papMem[iDay], L": ");
            // task name
            PWSTR psz;
            psz = (PWSTR) pTask->m_memName.p;
            if (fWithLinks) {
               // BUGFIX - Actually link to task
               MemCat (papMem[iDay], L"<a href=pt:");
               MemCat (papMem[iDay], (int) dwDB);
               MemCat (papMem[iDay], L":");
               MemCat (papMem[iDay], (int) pTask->m_dwID);
               MemCat (papMem[iDay], L">");
            }
            if (psz && psz[0])
               MemCatSanitize (papMem[iDay], psz);
            if (fWithLinks)
               MemCat (papMem[iDay], L"</a>");

            // SHow # days
            WCHAR szTemp[64];
            swprintf (szTemp, (pTask->m_dwDuration == 100) ? L" (%g day)" : L" (%g days)", (double) pTask->m_dwDuration / 100.0);
            MemCat (papMem[iDay], szTemp);

            if (pTask->m_dwConflict)
               MemCat (papMem[iDay], L"</font>");
            MemCat (papMem[iDay], L"</li>");

            // Show who it's assigned to
            if (pTask->m_dwAssignedTo) {
               MemCat (papMem[iDay], L"<p>Assigned to ");
               if (fWithLinks) {
                  MemCat (papMem[iDay], L"<a href=v:");
                  MemCat (papMem[iDay], pTask->m_dwAssignedTo);
                  MemCat (papMem[iDay], L">");
               }
               PWSTR psz;
               psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pTask->m_dwAssignedTo));
               MemCatSanitize (papMem[iDay], psz ? psz : L"Unknown");
               if (fWithLinks) {
                  MemCat (papMem[iDay], L"</a>");
               }
               MemCat (papMem[iDay], L"</p>");
            }

            // description if there is one
            psz = (PWSTR) pTask->m_memDescription.p;
            if (psz && psz[0]) {
               MemCat (papMem[iDay], L"<p>Description: ");
               MemCatSanitize (papMem[iDay], psz);
               MemCat (papMem[iDay], L"</p>");
            }

         }

      }

   }
}




/*******************************************************************************
ProjectTimerSubst - Returns a pointer to a string for the substitution (below
the main menu) for timers. This is usually a pointer to gMemTemp.p

inputs
   none
returns
   PWSTR - psz
*/
PWSTR ProjectTimerSubst (void)
{
   HANGFUNCIN;
   PCMMLNode pNode;
   pNode = FindMajorSection (gszProjectNode);
   if (!pNode)
      return NULL;

   DWORD dwIndex;
   dwIndex = 0;
   MemZero (&gMemTemp);
   DFDATE today = 0;
   while (TRUE) {
      PWSTR psz;
      int   iID;
      DFDATE   date;
      DFTIME   start, end;
      DWORD dwHash;
      psz = NodeElemGet (pNode, gszTimer, &dwIndex, (int*) &dwHash, &date, &start, &end, &iID);
      if (!psz)
         break;

      // write it out
      MemCat (&gMemTemp, L"<tr><td width=5%/><td width=95%><font color=#ffffff><small>");
      MemCat (&gMemTemp, L"Work timer: ");
      MemCat (&gMemTemp, L"<a href=px:");
      MemCat (&gMemTemp, (int)dwIndex-1);
      MemCat (&gMemTemp, L" color=#8080ff>");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"<xhoverhelpshort>Press this to stop the timer and log it in your project.</xhoverhelpshort>");
      MemCat (&gMemTemp, L"</a>");
      MemCat (&gMemTemp, L", Started ");
      if (!today)
         today = Today();
      WCHAR szTemp[64];
      if (today != date) {
         DFDATEToString (date, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L" ");
      }
      DFTIMEToString (start, szTemp);
      MemCat (&gMemTemp, szTemp);

      MemCat (&gMemTemp, L"</small></font></td></tr>");
   }

   gpData->NodeRelease(pNode);
   return (PWSTR) gMemTemp.p;
}



/*****************************************************************************
ProjectTimerEndUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   DWORD          dwIndex - For the timer
returns
   BOOL - TRUE if the Project was changed, requiring a screen refresh
*/
BOOL ProjectTimerEndUI (PCEscPage pPage, DWORD dwIndex)
{
   HANGFUNCIN;
   // ask the user what they want to do?
   int iRet;
   iRet = pPage->MBYesNo (
      L"Have you finished the project task?",
      L"Pressing 'Yes' will create a journal entry indicating how much time you "
      L"spent on the project, and mark the task as completed.\r\n"
      L"Pressing 'No' will just store the time you worked on the project away, but "
      L"won't mark it as completed.\r\n"
      L"Pressing 'Cancel' will leave the timer untouched.",
      TRUE);
   if (iRet == IDCANCEL)
      return FALSE;

   // kill the timer
   PCMMLNode pNode;
   pNode = FindMajorSection (gszProjectNode);
   if (!pNode)
      return TRUE;

   DWORD dwIndex2;
   dwIndex2 = 0;
   int   iNumber, extra;
   DFDATE   date;
   DFTIME   start, end;
   while (TRUE) {
      PWSTR psz;
      psz = NodeElemGet (pNode, gszTimer, &dwIndex2, &iNumber, &date, &start, &end, &extra);
      if (!psz)
         return TRUE;   // error, can't find
      if ((dwIndex2-1) == dwIndex)
         break;   // found the right one
   }

   // delete the info
   // NodeElemRemove (pNode, gszTimer, iNumber);
   pNode->ContentRemove (dwIndex);
   gpData->NodeRelease (pNode);
   gpData->Flush();

   // know some info
   DWORD dwProjectNode, dwID;
   dwProjectNode= (DWORD) end;
   dwID = (DWORD) extra;

   // swap in the new project
   DWORD dwOldProject = gCurProject.m_dwDatabaseNode;

   if (dwProjectNode != dwOldProject)
      ProjectSetView (dwProjectNode);

   PCProjectTask pTask;
   pTask = gCurProject.TaskGetByID (dwID);
   if (!pTask)
      return TRUE;   // can't find though

   // find out today
   DFDATE today;
   DFTIME   now;
   today = Today();
   now = Now();
   while (date <= today) {
      // figure out end time
      DFTIME end;
      end = (today == date) ? now : TODFTIME(23,59);

      // add to log
      WCHAR szHuge[10000];
      wcscpy (szHuge, (iRet == IDYES) ? L"Task completed in " : L"Worked on task in ");
      wcscat (szHuge, (PWSTR) gCurProject.m_memName.p);
      wcscat (szHuge, L" project: ");
      wcscat (szHuge, (PWSTR)pTask->m_memName.p);
      CalendarLogAdd (date, start, end, szHuge, gCurProject.m_dwDatabaseNode, FALSE);

      // if log to project that indicate that
      if (gCurProject.m_iJournalID != -1)
         JournalLink ((DWORD)gCurProject.m_iJournalID, szHuge, gCurProject.m_dwDatabaseNode, date, start, end,
         ((int) end - (int) start) / 60.0, TRUE);


      // increase the date, and rese thte start time to the early hours
      date = MinutesToDFDATE (DFDATEToMinutes(date) + 24 * 60);
      start = 0;
   }

   // mark the task as completed if the user wants that
   if (iRet == IDYES) {
      dwIndex = gCurProject.TaskIndexByID (dwID);

      EscChime (ESCCHIME_INFORMATION);
      EscSpeak (L"Task completed.");
      gCurProject.m_listTasks.Remove(dwIndex);

      // add to completed.
      pTask->m_dwDateCompleted = Today();
      gCurProject.m_listTasksCompleted.Add (&pTask);
      gCurProject.m_fDirty = TRUE;
      gCurProject.Flush();
   }

   // swap the old one back just in case we're looking at a project page
   if (dwOldProject && (dwProjectNode != dwOldProject))
      ProjectSetView (dwOldProject);

   return TRUE;
}



/*****************************************************************************
ProjectParseLink - If a page uses a Project substitution, which generates
"pt:XXXX" to indicate that a Project link was called, call this. If it's
not a Project link, returns FALSE. If it is, it deletes the Project and
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
BOOL ProjectParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, BOOL fJumpToEdit)
{
   HANGFUNCIN;
   CEscWindow  cWindow;

   // make sure its the right type
   if ((pszLink[0] != L'p') || (pszLink[1] != L't') || (pszLink[2] != L':'))
      return FALSE;

   // get the project ID, task ID, and maybe entry ID
   DWORD dwProjectNode, dwTaskID, dwSplit;
   PWSTR pszColon;
   dwProjectNode = (DWORD) _wtoi(pszLink + 3);

   // colon for project ID
   pszColon = wcschr(pszLink+3, L':');
   if (!pszColon)
      return TRUE;
   dwTaskID = _wtoi(pszColon+1);

   // see if has colon for another split
   pszColon = wcschr(pszColon+1, L':');
   dwSplit = pszColon ? _wtoi(pszColon+1) : -1;

   // swap in the new project
   DWORD dwOldProject = gCurProject.m_dwDatabaseNode;
   if (dwProjectNode != dwOldProject)
      ProjectSetView (dwProjectNode);
   PCProjectTask pTask;
   pTask = gCurProject.TaskGetByID (dwTaskID);


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
         goto done;
      if (iRet == IDYES) {
         *pfRefresh = MarkTaskCompleted(pPage, pTask, dwSplit);
         goto done;
      }
      // else, go on to editing
   }

   // pull up UI
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPROJECTTASKEDIT, ProjectTaskEditPage, pTask);
   if (pszRet && !_wcsicmp(pszRet, L"split")) {
      // BUGFIX - User has selected the option of splitting the task, so do that
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPROJECTTASKSPLIT, ProjectTaskSplitPage, pTask);
   }

   *pfRefresh = (pszRet && !_wcsicmp(pszRet, gszOK));

done:
   // swap the old one back just in case we're looking at a project page
   if (dwOldProject && (dwProjectNode != dwOldProject))
      ProjectSetView (dwOldProject);

   return TRUE;
}



/*******************************************************************************
ProjectEnumForPlanner - Enumerates the meetings in a format the planner can use.

inputs
   DFDATE      date - start date
   DWORD       dwDays - # of days to generate
   PCListFixed palDays - Pointer to an array of CListFixed. Number of elements
               is dwDays. These will be concatenated with relevent information.
               Elements in the list are PLANNERITEM strctures.
returns
   none
*/
void ProjectEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays)
{
   HANGFUNCIN;
   DFDATE today = Today();
   DFDATE twoWeeks = MinutesToDFDATE(DFDATEToMinutes(today)+(60*24)*14);

   // figure out when the upcoming days are
#define  MAXDAYS  28
   DFDATE   adDay[MAXDAYS], dMax;
   DWORD i;
   dwDays = min(dwDays,MAXDAYS);
   for (i = 0; i < dwDays; i++)
      dMax = adDay[i] = MinutesToDFDATE(DFDATEToMinutes(date)+(60*24)*i);

   // if there aren't any projects then return empty string
   if ((DWORD)-1 == ProjectIndexToDatabase(0))
      return;

   // loop
   DWORD dwProject, dwDB;
   for (dwProject = 0; ; dwProject++) {
      dwDB = ProjectIndexToDatabase (dwProject);
      if (dwDB == (DWORD)-1)
         break;

      // load the project
      if (!gCurProject.Init (dwDB))
         break;   // couldnt load for whatever reason

      // BUGFIX - Don't show disabled projects
      if (gCurProject.m_fDisabled)
         continue;


      PCProjectTask pTask;
      // find all the Tasks and Project Tasksfor the date
      PLANNERITEM pi;
      for (i = 0; i < gCurProject.m_listTasks.Num(); i++) {
         pTask = gCurProject.TaskGetByIndex(i);

         // if it's not assigned to self then exit (don't show on own to-do)
         if (pTask->m_dwAssignedTo)
            continue;

         // if the task occurs after dMax and there's no PlanTask yet then ignore
         if ((pTask->m_dwDateStarted > dMax) && !pTask->m_PlanTask.Num())
            continue;

         // BUGFIX: if the task occurs more than two two weeks from now then clear the plan
         // task since it must have been delayed anyway
         if (pTask->m_dwDateStarted > twoWeeks) {
            while (pTask->m_PlanTask.Num())
               pTask->m_PlanTask.Remove(0);
            continue;
         }

         // BUGFIX: if any planned dates are more than two weeks away then remove them
         // since not logical for task to be that far out
         DWORD j;
         for (j = pTask->m_PlanTask.Num()-1; j < pTask->m_PlanTask.Num(); j--) {
            PPLANTASKITEM ppi = pTask->m_PlanTask.Get(j);
            if (ppi->date > twoWeeks) {
               pTask->m_PlanTask.Remove(j);
            }
         }

         // else, update the plan task
         gCurProject.m_fDirty |= pTask->m_PlanTask.Verify (pTask->m_dwDuration * 8 * 60 / 100,
            today,
            (pTask->m_dwDateStarted < today) ? today : pTask->m_dwDateStarted,
            100+1);

         // loop through all the plans
         DFDATE dfPrevious;
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
            pi.dwMajor = dwDB;
            pi.dwMinor = pTask->m_dwID;
            pi.dwSplit = j;
            pi.dwType = 3;

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
            MemCat (pMem, L"<italic>");
            if (!gfPrinting) {
               MemCat (pMem, L"<a href=v:");
               MemCat (pMem, (int) dwDB);
               MemCat (pMem, L">");
            }

            MemCatSanitize (pMem, (PWSTR) gCurProject.m_memName.p);

            if (!gfPrinting) {
               MemCat (pMem, L"</a>");
            }
            MemCat (pMem, L"</italic>");
            MemCat (pMem, L": ");
            // task name
            PWSTR psz;
            psz = (PWSTR) pTask->m_memName.p;
            // BUGFIX - Actually link to task
            if (!gfPrinting) {
               MemCat (pMem, L"<a href=pt:");
               MemCat (pMem, (int) dwDB);
               MemCat (pMem, L":");
               MemCat (pMem, (int) pTask->m_dwID);
               MemCat (pMem, L":");
               MemCat (pMem, (int) j);
               MemCat (pMem, L">");
            }
            if (psz && psz[0])
               MemCatSanitize (pMem, psz);
            if (!gfPrinting)
               MemCat (pMem, L"</a>");


            MemCat (pMem, L" <italic>(Project task)</italic>");

            // SHow # days
            WCHAR szTemp[64];
            swprintf (szTemp, (pTask->m_dwDuration == 100) ? L" (%g day)" : L" (%g days)", (double) pTask->m_dwDuration / 100.0);
            MemCat (pMem, szTemp);

            // subproject name
            psz = (PWSTR) pTask->m_memSubProject.p;
            if (psz && psz[0]) {
               MemCat (pMem, L"<br/>Subproject: ");
               MemCatSanitize (pMem, psz);
            }

            // description if there is one
            psz = (PWSTR) pTask->m_memDescription.p;
            if (psz && psz[0]) {
               MemCat (pMem, L"<br/>Description: ");
               MemCatSanitize (pMem, psz);
            }

        
            // add it
            palDays[d].Add(&pi);
         }

         // done with all plans

      }
      gCurProject.Flush();

   }
}



/*******************************************************************************
ProjectAdjustPriority - Adjusts the priority of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DWORD       dwPriority - New priority
returns
   none
*/
void ProjectAdjustPriority (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwPriority)
{
   HANGFUNCIN;
   DFDATE today = Today();

   if (dwMajor != gCurProject.m_dwDatabaseNode)
      ProjectSetView (dwMajor);

   PCProjectTask pTask;
   pTask = gCurProject.TaskGetByID (dwMinor);
   if (!pTask)
      return;
   PPLANTASKITEM pi;
   pi = pTask->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->dwPriority = dwPriority;

   gCurProject.m_fDirty = TRUE;
   gCurProject.Flush();
}

/*******************************************************************************
ProjectChangeDate - Adjusts the date of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DFDATE      date - new date
returns
   none
*/
void ProjectChangeDate (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFDATE date)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   if (dwMajor != gCurProject.m_dwDatabaseNode)
      ProjectSetView (dwMajor);

   PCProjectTask pTask;
   pTask = gCurProject.TaskGetByID (dwMinor);
   if (!pTask)
      return;
   PPLANTASKITEM pi;
   pi = pTask->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->date = date;

   gCurProject.m_fDirty = TRUE;
   gCurProject.Flush();
}

/*******************************************************************************
ProjectChangeTime - Adjusts the date of a specific item for the planner

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DFTIME      time -new time
returns
   none
*/
void ProjectChangeTime (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFTIME time)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   if (dwMajor != gCurProject.m_dwDatabaseNode)
      ProjectSetView (dwMajor);

   PCProjectTask pTask;
   pTask = gCurProject.TaskGetByID (dwMinor);
   if (!pTask)
      return;
   PPLANTASKITEM pi;
   pi = pTask->m_PlanTask.Get(dwSplit);
   if (!pi)
      return;
   pi->start = time;

   gCurProject.m_fDirty = TRUE;
   gCurProject.Flush();
}


/*******************************************************************************
ProjectSplitPlan - Split a Project item into two sections

inputs
   DWORD       dwMajor, dwMinor, dwSplit - To adjust
   DWORD       dwMin - Split this many minutes down
returns
   none
*/
void ProjectSplitPlan (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwMin)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   if (dwMajor != gCurProject.m_dwDatabaseNode)
      ProjectSetView (dwMajor);

   PCProjectTask pTask;
   pTask = gCurProject.TaskGetByID (dwMinor);
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

   gCurProject.m_fDirty = TRUE;
   gCurProject.Flush();
}