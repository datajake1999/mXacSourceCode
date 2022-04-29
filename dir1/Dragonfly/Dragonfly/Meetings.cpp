/*************************************************************************
Meetingss.cpp - For takss featre

begun 8/18/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

#define  STARTOFDAY     TODFTIME(8,0)
#define  ENDOFDAY       TODFTIME(18,0)

/* C++ objects */
class CSchedMeeting;
typedef CSchedMeeting * PCSchedMeeting;

// Sched
class CSched {
public:
   CSched (void);
   ~CSched (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCSchedMeeting MeetingAdd (PWSTR pszName, PWSTR pszDescription, DWORD dwBefore);
   PCSchedMeeting MeetingGetByIndex (DWORD dwIndex);
   PCSchedMeeting MeetingGetByID (DWORD dwID);
   DWORD MeetingIndexByID (DWORD dwID);
   void Sort (void);
   void BringUpToDate (DFDATE today);
   void DeleteAllChildren (DWORD dwID);


   CListFixed     m_listMeetings;   // list of PCSchedMeetings, ordered according order of doing

   BOOL           m_fDirty;      // set to TRUE if the Sched is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   DWORD          m_dwNextMeetingID;   // next Meeting ID created

private:

};

typedef CSched * PCSched;

#define  MAXATTEND      8

// Meeting
class CSchedMeeting {
public:
   CSchedMeeting (void);
   ~CSchedMeeting (void);

   PCSched         m_pSched; // Sched that this belongs to. If change sets dirty
   CMem           m_memName;  // name of the Meeting
   CMem           m_memDescription; // description
   DWORD          m_dwID;     // unique ID for the Meeting.
   DWORD          m_dwParent;    // non-zero if created by a reoccurring task
   REOCCURANCE    m_reoccur;     // how often the Meeting reoccurs.

   CMem           m_memWhere; // where the meeting is
   DFDATE         m_dwDate;   // date of the meeting
   DFTIME         m_dwStart;  // start time
   DFTIME         m_dwEnd;    // end time
   DWORD          m_dwAlarm;  // sound an alarm this many minutes before
   DFDATE         m_dwAlarmDate; // if follow m_dwAlarm, then this is the date of the alarm
   DFTIME         m_dwAlarmTime; // if follow m_dwAlarm, then this is the time of the alarm
   DWORD          m_adwAttend[MAXATTEND];    // attendee node IDs. -1 if empty
   int            m_iJournalID;     // index for journal entry

   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);
   void BringUpToDate (DFDATE today);
private:
};

DWORD SchedCreateMeetingNotes (PCSchedMeeting pMeeting, BOOL fNow);

/* globals */
BOOL           gfSchedPopulated = FALSE;// set to true if glistSched is populated
CSched       gCurSched;      // current Sched that Scheding with
static DWORD gdwNode = 0;  // node being edited
static DWORD gdwActionPersonID = (DWORD)-1;  // for action item dialog

static WCHAR gszDate[] = L"date";
//static WCHAR gszSchedDate[] = L"Scheddate";
//static WCHAR gszDaysOfSched[] = L"DaysOfSched";
//static WCHAR gszCompleted[] = L"completed";
//static WCHAR gszDelete[] = L"delete";
static WCHAR gszHadMeeting[] = L"hadmeeting";
static WCHAR gszCompleteBy[] = L"completeby";
static WCHAR gszMeeting[] = L"meeting";
static WCHAR gszWhere[] = L"where";
static PWSTR gaszAttend[MAXATTEND] = {L"attend1",L"attend2",L"attend3",L"attend4",
   L"attend5",L"attend6",L"attend7",L"attend8"};
static WCHAR gszNextMeetingID[] = L"NextMeetingID";
static WCHAR gszMeetingLoc[] = L"MeetingLoc";
static WCHAR gszMeetingDate[] = L"MeetingDate";
static WCHAR gszMeetingStart[] = L"MeetingStart";
static WCHAR gszMeetingEnd[] = L"MeetingEnd";

/*************************************************************************
CSchedMeeting::constructor and destructor
*/
CSchedMeeting::CSchedMeeting (void)
{
   HANGFUNCIN;
   m_pSched = NULL;
   m_dwID = 0;
   m_dwParent = 0;
   m_memName.CharCat (0);
   m_memDescription.CharCat (0);
   memset (&m_reoccur, 0, sizeof(m_reoccur));

   m_memWhere.CharCat (0);
   m_dwDate = 0;
   m_dwAlarm = 15;
   m_dwAlarmDate = 0;
   m_dwAlarmTime = 0;
   m_dwStart = m_dwEnd = (DWORD)-1;
   DWORD i;
   for (i = 0; i < MAXATTEND; i++)
      m_adwAttend[i] = (DWORD)-1;
   m_iJournalID = -1;

}

CSchedMeeting::~CSchedMeeting (void)
{
   HANGFUNCIN;
   // intentionally left blank
}


/*************************************************************************
CSchedMeeting::SetDirty - Sets the dirty flag in the main Sched
*/
void CSchedMeeting::SetDirty (void)
{
   HANGFUNCIN;
   if (m_pSched)
      m_pSched->m_fDirty = TRUE;
}

/**********************************************************************
AlarmControlGet - Gets the alarm time from a xComboAlarm control.

inputs
   PCEscPage   pPage - page
   PWSTR       pszName - Control name
returns
   DWORD - # of minutes
*/
DWORD AlarmControlGet (PCEscPage pPage, PWSTR pszName)
{
   HANGFUNCIN;
   PCEscControl pc;
   pc = pPage->ControlFind (pszName);
   if (!pc)
      return 0;

   ESCMCOMBOBOXGETITEM gi;
   memset (&gi, 0, sizeof(gi));
   gi.dwIndex = pc->AttribGetInt (gszCurSel);
   pc->Message (ESCM_COMBOBOXGETITEM, &gi);

   if (gi.pszName)
      return (DWORD) _wtoi(gi.pszName);
   else
      return 0;
}

/**********************************************************************
AlarmControlSet - Sets the alarm time from a xComboAlarm control.

inputs
   PCEscPage   pPage - page
   PWSTR       pszName - Control name
   DWORD       dwAlarm - # of minutes to go off before
returns
   BOOL - TRUE if succede
*/
BOOL AlarmControlSet (PCEscPage pPage, PWSTR pszName, DWORD dwAlarm)
{
   HANGFUNCIN;
   PCEscControl pc;
   pc = pPage->ControlFind (pszName);
   if (!pc)
      return 0;

   ESCMCOMBOBOXSELECTSTRING item;
   memset (&item, 0, sizeof(item));
   item.fExact = TRUE;
   item.dwIndex = (DWORD)-1;
   item.iStart = 0;
   WCHAR szTemp[32];
   swprintf (szTemp, L"%d", (int) dwAlarm);
   item.psz = szTemp;
   return pc->Message (ESCM_COMBOBOXSELECTSTRING, &item);
}


/*************************************************************************
CalcAlarmTime - Calculates when the alarm will happen.

inputs
   DFDATE      m_dwDate - Meeting date
   DFTIME      m_dwStart - Meeting start
   DWORD       m_dwAlarm - Alarm value. if 0 then will set alarm date to 0
   DFDATE      *pdwAlarmDate - FIlled with alarm date
   DFTIME      *pdwAlarmTime - Filled with alarm time
returns
   none
*/
void CalcAlarmTime (DFDATE m_dwDate, DFTIME m_dwStart, DWORD m_dwAlarm,
                    DFDATE *pdwAlarmDate, DFTIME *pdwAlarmTime)
{
   HANGFUNCIN;
   if (!m_dwAlarm) {
      *pdwAlarmDate = 0;
      *pdwAlarmTime = 0;
      return;
   }

   __int64 iMin;
   iMin = DFDATEToMinutes(m_dwDate) +
      DFTIMEToMinutes((m_dwStart != (DWORD)-1) ? m_dwStart : STARTOFDAY) -
      (__int64) m_dwAlarm;
   *pdwAlarmDate = MinutesToDFDATE(iMin);
   iMin -= DFDATEToMinutes (*pdwAlarmDate);
   *pdwAlarmTime = TODFTIME(iMin / 60, iMin % 60);
}

/*************************************************************************
CSchedMeeting::BringUpToDate - This is a reoccurring Meeting, it makes sure
that all occurances of the reoccurring Meeting have been generated up to
(and one beyond) today.

inputs
   DFDATE      today - today
returns
   none
*/
void CSchedMeeting::BringUpToDate (DFDATE today)
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
      PCSchedMeeting pNew;
      pNew = m_pSched->MeetingAdd ((PWSTR)m_memName.p, (PWSTR)m_memDescription.p, (DWORD)-1);
      if (!pNew)
         break;   // error
      memcpy (pNew->m_adwAttend, m_adwAttend, sizeof(m_adwAttend));
      pNew->m_dwDate = next;
      pNew->m_dwEnd = m_dwEnd;
      pNew->m_dwStart = m_dwStart;
      pNew->m_dwParent = m_dwID;
      pNew->m_iJournalID = m_iJournalID;
      pNew->m_dwAlarm = m_dwAlarm;
      CalcAlarmTime (pNew->m_dwDate, pNew->m_dwStart, pNew->m_dwAlarm,
         &pNew->m_dwAlarmDate, &pNew->m_dwAlarmTime);
      MemZero (&pNew->m_memWhere);
      MemCat (&pNew->m_memWhere, (PWSTR)m_memWhere.p);
      // all copied over
   }

   // done
}

/*************************************************************************
CSchedMeeting::Write - Writes the Meeting information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CSchedMeeting::Write (PCMMLNode pParent)
{
   HANGFUNCIN;
   PCMMLNode   pSub;

   pSub = pParent->ContentAddNewNode ();
   if (!pSub)
      return FALSE;

   // set the name
   pSub->NameSet (gszMeeting);

   // write out parameters
   NodeValueSet (pSub, gszName, (PWSTR) m_memName.p);
   NodeValueSet (pSub, gszDescription, (PWSTR) m_memDescription.p);
   NodeValueSet (pSub, gszID, (int) m_dwID);
   NodeValueSet (pSub, gszParent, (int) m_dwParent);

   NodeValueSet (pSub, gszWhere, (PWSTR) m_memWhere.p);
   NodeValueSet (pSub, gszDate, (int) m_dwDate);
   NodeValueSet (pSub, gszEnd, (int) m_dwEnd);
   NodeValueSet (pSub, gszStart, (int) m_dwStart);
   NodeValueSet (pSub, gszAlarm, (int) m_dwAlarm);
   NodeValueSet (pSub, gszAlarmDate, (int) m_dwAlarmDate);
   NodeValueSet (pSub, gszAlarmTime, (int) m_dwAlarmTime);
   DWORD i;
   for (i = 0; i < MAXATTEND; i++)
      NodeValueSet (pSub, gaszAttend[i], (int) m_adwAttend[i]);

   // journal
   NodeValueSet (pSub, gszJournal, m_iJournalID);

   // write out reoccurance
   NodeValueSet (pSub, gszReoccurPeriod, m_reoccur.m_dwPeriod);
   NodeValueSet (pSub, gszReoccurLastDate, m_reoccur.m_dwLastDate);
   NodeValueSet (pSub, gszReoccurEndDate, m_reoccur.m_dwEndDate);
   NodeValueSet (pSub, gszReoccurParam1, m_reoccur.m_dwParam1);
   NodeValueSet (pSub, gszReoccurParam2, m_reoccur.m_dwParam2);
   NodeValueSet (pSub, gszReoccurParam3, m_reoccur.m_dwParam3);

   return TRUE;
}


/*************************************************************************
CSchedMeeting::Read - Writes the Meeting information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the Meeting data
reutrns
   BOOL - TRUE if successful
*/
BOOL CSchedMeeting::Read (PCMMLNode pSub)
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

   psz = NodeValueGet (pSub, gszWhere);
   MemZero (&m_memWhere);
   if (psz)
      MemCat (&m_memWhere, psz);
   m_dwDate = (DWORD) NodeValueGetInt (pSub, gszDate, 0);
   m_dwEnd = (DWORD) NodeValueGetInt (pSub, gszEnd, -1);
   m_dwStart = (DWORD) NodeValueGetInt (pSub, gszStart, -1);
   m_dwParent = (DWORD) NodeValueGetInt (pSub, gszParent, 0);
   m_dwAlarm = (DWORD) NodeValueGetInt (pSub, gszAlarm, 0);
   m_dwAlarmDate = (DWORD) NodeValueGetInt (pSub, gszAlarmDate, 0);
   m_dwAlarmTime = (DWORD) NodeValueGetInt (pSub, gszAlarmTime, 0);
   DWORD i;
   for (i = 0; i < MAXATTEND; i++)
      m_adwAttend[i] = (DWORD) NodeValueGetInt (pSub, gaszAttend[i], -1);

   // journal
   m_iJournalID = -1;
   m_iJournalID = NodeValueGetInt (pSub, gszJournal, -1);

   m_reoccur.m_dwPeriod = (DWORD) NodeValueGetInt (pSub, gszReoccurPeriod, 0);
   m_reoccur.m_dwLastDate = (DWORD) NodeValueGetInt (pSub, gszReoccurLastDate, 0);
   m_reoccur.m_dwEndDate = (DWORD) NodeValueGetInt (pSub, gszReoccurEndDate, 0);
   m_reoccur.m_dwParam1 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam1, 0);
   m_reoccur.m_dwParam2 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam2, 0);
   m_reoccur.m_dwParam3 = (DWORD) NodeValueGetInt (pSub, gszReoccurParam3, 0);

   return TRUE;
}

/*************************************************************************
CSched::Constructor and destructor - Initialize
*/
CSched::CSched (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwNextMeetingID = 1;

   m_listMeetings.Init (sizeof(PCSchedMeeting));
}

CSched::~CSched (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new Sched is cleared
   Init();
}


/************************************************************************
CSched::MeetingAdd - Adds a mostly default Meeting.

inputs
   PWSTR    pszName - name
   PWSTR    pszDescription - description
   DWORD    dwBefore - Meeting index to insert before. -1 for after
returns
   PCSchedMeeting - To the Meeting object, to be added further
*/
PCSchedMeeting CSched::MeetingAdd (PWSTR pszName, PWSTR pszDescription,
                                 DWORD dwBefore)
{
   HANGFUNCIN;
   PCSchedMeeting pMeeting;
   pMeeting = new CSchedMeeting;
   if (!pMeeting)
      return NULL;

   pMeeting->m_dwID = m_dwNextMeetingID++;
   m_fDirty = TRUE;
   pMeeting->m_pSched = this;

   MemZero (&pMeeting->m_memName);
   MemCat (&pMeeting->m_memName, pszName);
   MemZero (&pMeeting->m_memDescription);
   MemCat (&pMeeting->m_memDescription, pszDescription);
  
   // add this to the list
   if (dwBefore < m_listMeetings.Num())
      m_listMeetings.Insert(dwBefore, &pMeeting);
   else
      m_listMeetings.Add (&pMeeting);

   return pMeeting;
}

/*************************************************************************
MeetingGetByIndex - Given an index, this returns a Meeting, or NULL if can't find.
*/
PCSchedMeeting CSched::MeetingGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCSchedMeeting pMeeting, *ppMeeting;

   ppMeeting = (PCSchedMeeting*) m_listMeetings.Get(dwIndex);
   if (!ppMeeting)
      return NULL;
   pMeeting = *ppMeeting;
   return pMeeting;
}

/*************************************************************************
MeetingGetByID - Given a Meeting ID, this returns a Meeting, or NULL if can't find.
*/
PCSchedMeeting CSched::MeetingGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCSchedMeeting pMeeting;
   DWORD i;
   for (i = 0; i < m_listMeetings.Num(); i++) {
      pMeeting = MeetingGetByIndex (i);
      if (!pMeeting)
         continue;

      if (pMeeting->m_dwID == dwID)
         return pMeeting;
   }

   // else, cant find
   return NULL;
}

/*************************************************************************
MeetingIndexByID - Given a Meeting ID, this returns a an index, or -1 if can't find.
*/
DWORD CSched::MeetingIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCSchedMeeting pMeeting;
   DWORD i;
   if (!dwID)
      return (DWORD) -1;

   for (i = 0; i < m_listMeetings.Num(); i++) {
      pMeeting = MeetingGetByIndex (i);
      if (!pMeeting)
         continue;

      if (pMeeting->m_dwID == dwID)
         return i;
   }

   // else, cant find
   return (DWORD)-1;
}

/*************************************************************************
CSched::Write - Writes all the Sched data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
Sched object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CSched::Write (void)
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

   if (!NodeValueSet (pNode, gszNextMeetingID, m_dwNextMeetingID))
      return FALSE;


   // BUGBUG - 2.0 - at some point write out last modified, for search purposes

   // remove existing Meetings
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   for (i = dwNum - 1; i < dwNum; i--) {
      PCMMLNode   pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszMeeting))
         continue;

      // else, old Meeting. delete
      pNode->ContentRemove (i);
   }

   // write out other info, such as Meetings, completed and not
   PCSchedMeeting pMeeting;
   for (i = 0; i < m_listMeetings.Num(); i++) {
      pMeeting = *((PCSchedMeeting*) m_listMeetings.Get(i));
      pMeeting->Write (pNode);
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
CSched::Flush - If m_fDirty is set it writes out the Sched and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CSched::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CSched::Init(void) - Initializes a blank Sched. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CSched::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCSchedMeeting pMeeting;
   for (i = 0; i < m_listMeetings.Num(); i++) {
      pMeeting = *((PCSchedMeeting*) m_listMeetings.Get(i));
      delete pMeeting;
   }
   m_listMeetings.Clear();

   // new values for name, description, and days per week
   m_dwNextMeetingID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   // bring reoccurring Meetings up to date
   BringUpToDate (Today());

   // sort
   Sort ();

   return TRUE;
}

/*************************************************************************
CSched::Init(dwDatabaseNode) - Initializes by reading the Sched in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a Sched, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CSched::Init (DWORD dwDatabaseNode)
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
   if (_wcsicmp(pNode->NameGet(), gszSchedListNode)) {
      // invalid name
      gpData->NodeRelease(pNode);
      return FALSE;
   }
   m_dwDatabaseNode = dwDatabaseNode;


   // read in the name, description, and daysperweek
   m_dwNextMeetingID = NodeValueGetInt (pNode, gszNextMeetingID, 1);

   // read in other Meetings
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
      if (!_wcsicmp(pSub->NameGet(), gszMeeting))
         fCompleted = FALSE;
      else
         continue;   // not looking for this

      PCSchedMeeting pMeeting;
      pMeeting = new CSchedMeeting;
      pMeeting->Read (pSub);
      pMeeting->m_pSched = this;


      // add to list
      m_listMeetings.Add (&pMeeting);
   }


   // bring reoccurring Meetings up to date
   BringUpToDate (Today());
   // sort
   Sort ();

   
   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/*******************************************************************************
Sort - Sorts the Sched list by date.
*/
int __cdecl SchedSort(const void *elem1, const void *elem2 )
{
   PCSchedMeeting t1, t2;
   t1 = *((PCSchedMeeting*)elem1);
   t2 = *((PCSchedMeeting*)elem2);

   // non-automatic have priority
   if (t1->m_reoccur.m_dwPeriod && !t2->m_reoccur.m_dwPeriod)
      return 1;
   else if (!t1->m_reoccur.m_dwPeriod && t2->m_reoccur.m_dwPeriod)
      return -1;

   // by date appear
   if (t1->m_dwDate != t2->m_dwDate)
      return (int) t1->m_dwDate - (int)t2->m_dwDate;

   // else by time
   return (int) t1->m_dwStart - (int)t2->m_dwStart;
}

void CSched::Sort (void)
{
   qsort (m_listMeetings.Get(0), m_listMeetings.Num(), sizeof(PCSchedMeeting), SchedSort);
}


/************************************************************************
BringUpToDate - Brings all the reoccurring Meetings up to date, today.
*/
void CSched::BringUpToDate (DFDATE today)
{
   HANGFUNCIN;
   DWORD dwNum = m_listMeetings.Num();
   DWORD i;
   PCSchedMeeting pMeeting;
   for (i = 0; i < dwNum; i++) {
      pMeeting = MeetingGetByIndex (i);
      pMeeting->BringUpToDate (today);
   }

   // BUGFIX - Was crashing
   Flush();
}

/*************************************************************************
DeleteAllChildren - Given a reoccurring Meeting ID, this deletes all Meeting
instances created by the reoccurring Meeting.

inputs
   DWORD    dwID - ID
*/
void CSched::DeleteAllChildren (DWORD dwID)
{
   HANGFUNCIN;
   DWORD dwNum = m_listMeetings.Num();
   DWORD i;
   PCSchedMeeting pMeeting;
   for (i = dwNum - 1; i < dwNum; i--) {
      pMeeting = MeetingGetByIndex (i);
      if (!pMeeting || (pMeeting->m_dwParent != dwID))
         continue;

      // delete
      m_listMeetings.Remove (i);
      delete pMeeting;
   }

   m_fDirty = TRUE;
   Flush();
}

/***********************************************************************
SchedInit - Makes sure the Sched object is filled. If not,
it's loaded in. This CAN be called multiple times.

This fills in:
   gfSchedPopulated - Set to TRUE when it's been loaded
   gCurSched - fills in

inputs
   none
returns
   BOOL - error
*/
BOOL SchedInit (void)
{
   HANGFUNCIN;
   if (gfSchedPopulated)
      return TRUE;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszSchedListNode, &dwNum);
   if (!pNode)
      return NULL;   // error. This shouldn't happen
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfSchedPopulated = TRUE;

   // pass this on
   return gCurSched.Init (dwNum);
}



/*****************************************************************************
LookForConflict - Look for a conflict in times with the meeting.

inputs
   DFDATE      date - date of the meeting
   DFTIME      start - start time
   DFTIME      end - end time
   DWORD       dwExclude - Exclude this ID.
   BOOL        fMeeting - If TRUE this is a meeting, FALSE it's a phone call
returns
   BOOL - TRUE if conflic
*/
BOOL LookForConflict (DFDATE date, DFTIME start, DFTIME end, DWORD dwExclude, BOOL fMeeting)
{
   HANGFUNCIN;
   SchedInit();
   PhoneInit();

   if (!date)
      return FALSE;  // reoccuring. Can't really tell conflicts

   // if no start or end time and it's a phone call then no conflict
   if (!fMeeting) {
      if ((start == (DWORD)-1) && (end == (DWORD)-1))
         return FALSE;
      if (start == (DWORD)-1)
         start = end;
      if (end == (DWORD)-1)
         end = start;
   }

   // not no start/end time then assume at beginng/end of day
   if (start == (DWORD)-1)
      start = 0;
   if (end == (DWORD)-1)
      end = 0x2400;

   // find those that match
   DWORD i;
   PCSchedMeeting pMeeting;
   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);
      if (!pMeeting)
         continue;
      if (fMeeting && (pMeeting->m_dwID == dwExclude))
         continue;
      if (pMeeting->m_dwDate != date)
         continue;

      DFTIME s2, e2;
      s2 = pMeeting->m_dwStart;
      e2 = pMeeting->m_dwEnd;
      if (s2 == (DWORD)-1)
         s2 = 0;
      if (e2 == (DWORD)-1)
         e2 = 0x2400;

      // if start after end OK
      // BUGFIX - Was using pMeeting->m_dwStart and m_dwEnd, so wasnt
      // cantch all conflicts
      if (start >= e2)
         continue;
      if (end <= s2)
         continue;

      // else conflict
      return TRUE;
   }

   // look for conflicts with phone calls
   PCPhoneCall pCall;
   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      pCall = gCurPhone.CallGetByIndex(i);
      if (!pCall)
         continue;
      if (!fMeeting && (pCall->m_dwID == dwExclude))
         continue;
      if (pCall->m_dwDate != date)
         continue;

      // if no time then skip
      if ((pCall->m_dwStart == (DWORD)-1) && (pCall->m_dwEnd == (DWORD)-1))
         continue;

      DFTIME s2, e2;
      s2 = pCall->m_dwStart;
      e2 = pCall->m_dwEnd;
      if (s2 == (DWORD)-1)
         s2 = e2;
      if (e2 == (DWORD)-1)
         e2 = s2;

      // if start after end OK
      // BUGFIX - Was using pCall->m_dwStart and m_dwEnd, so wasnt
      // cantch all conflicts
      if (start >= e2)
         continue;
      if (end <= s2)
         continue;

      // else conflict
      return TRUE;
   }
   
   return FALSE;  // no conflict
}

/*****************************************************************************
SchedMeetingAddPage - Override page callback.
*/
BOOL SchedMeetingAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // BUGFIX - If have date that want to use for add then set
         if (pPage->m_pUserData)
            DateControlSet (pPage, gszMeetingDate, (DFDATE)pPage->m_pUserData);

         // always have an alarm 15 minutes before
         AlarmControlSet (pPage, gszAlarm, 15);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[256];
            WCHAR szDescription[10024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in one-line summary of the meeting.");
               return TRUE;
            }
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);

            // make sure a date is set for the meeting
            DFDATE d;
            d = DateControlGet (pPage, gszMeetingDate);
            if ((d == (DWORD)-1) || (d < Today())) {
               pPage->MBWarning (L"You must enter a date for the meeting sometime on or after today.");
               return TRUE;
            }

            // see if there's a conflict
            if (LookForConflict(DateControlGet(pPage,gszMeetingDate),
               TimeControlGet(pPage,gszMeetingStart), TimeControlGet(pPage,gszMeetingEnd),
               (DWORD)-1)) {
                  if (IDYES != pPage->MBYesNo (L"The meeting conflicts with another meeting or call. Do you want to schedule it anyway?"))
                     return TRUE;
            }


            // add it
            PCSchedMeeting pMeeting;
            pMeeting = gCurSched.MeetingAdd (szName, szDescription, (DWORD)-1);
            if (!pMeeting) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // other data
            pControl = pPage->ControlFind(gszMeetingLoc);
            szDescription[0] = 0;
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pMeeting->m_memWhere);
            MemCat (&pMeeting->m_memWhere, szDescription);


            pMeeting->m_dwDate = DateControlGet (pPage, gszMeetingDate);
            pMeeting->m_dwStart = TimeControlGet (pPage, gszMeetingStart);
            pMeeting->m_dwEnd = TimeControlGet (pPage, gszMeetingEnd);

            pMeeting->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);
            CalcAlarmTime (pMeeting->m_dwDate, pMeeting->m_dwStart, pMeeting->m_dwAlarm,
               &pMeeting->m_dwAlarmDate, &pMeeting->m_dwAlarmTime);

            DWORD i;
            for (i = 0; i < MAXATTEND; i++) {
               pControl = pPage->ControlFind (gaszAttend[i]);
               if (!pControl)
                  continue;

               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_adwAttend[i] = dwNode;
            }

            // journal
            pControl = pPage->ControlFind (gszJournal);
            if (pControl) {
               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = JournalIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_iJournalID = (int) dwNode;
            }


            // exit this
            gCurSched.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
SchedSummary - Fill in the substitution for the Meeting list.

inputs
   BOOL     fAll - If TRUE show all, FALSE show only active Meetings
   DFDATE   date - date to show up until
retursn
   PWSTR - string from gMemTemp
*/
PWSTR SchedSummary (BOOL fAll, DFDATE date)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   SchedInit();
   // bring reoccurring Meetings up to date. 1 day in advance so can use SchedSummary for tomrrow page
   gCurSched.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24));
   gCurSched.Sort();  // make sure they're sorted
   DWORD i;
   PCSchedMeeting pMeeting;
   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);

      // if !fAll different
      if (!fAll) {
         if (pMeeting->m_reoccur.m_dwPeriod || (pMeeting->m_dwDate > date))
            continue;
      }

      break;
   }
   if (i >= gCurSched.m_listMeetings.Num())
      return L""; // nothing

   // else, we have Meetings. Show them off
   MemZero (&gMemTemp);
   if (fAll) {
      MemCat (&gMemTemp, L"<xbr/>");
      MemCat (&gMemTemp, fAll ?
         L"<xSectionTitle>Meeting appointments</xSectionTitle>" :
         L"<xSectionTitle>Meeting</xSectionTitle>");
      // MemCat (&gMemTemp, L"<p>This shows the list of project Meetings you're scheduled to Sched on this week.</p>");
   }
   MemCat (&gMemTemp, L"<xlisttable innerlines=0>");
   if (!fAll)
      MemCat (&gMemTemp, L"<xtrheader><a href=r:113 color=#c0c0ff>Meetings</a></xtrheader>");

   DWORD dwLastDate;
   dwLastDate = (DWORD)-1;
   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);

      // filter out Meeting if !fAll
      if (!fAll) {
         if (pMeeting->m_reoccur.m_dwPeriod || (pMeeting->m_dwDate > date))
            continue;
      }

      // if different priority then show
      if (pMeeting->m_dwDate != dwLastDate) {
         if (dwLastDate != (DWORD)-1)
            MemCat (&gMemTemp, L"</xtrmonth>");
         dwLastDate = pMeeting->m_dwDate;

         MemCat (&gMemTemp, L"<xtrmonth><xmeetingdate>");
         if (dwLastDate) {
            WCHAR szTemp[64];
            DFDATEToString (dwLastDate, szTemp);
            MemCat (&gMemTemp, szTemp);
         }
         else
            MemCat (&gMemTemp, L"Reoccurring");
         MemCat (&gMemTemp, L"</xmeetingdate>");
      }

      // display Meeting info
      MemCat (&gMemTemp, L"<tr><xtdtask href=ma:");
      MemCat (&gMemTemp, (int) pMeeting->m_dwID);
      MemCat (&gMemTemp, L">");
      // BUGFIX - Wash crashing with ampersand
      MemCatSanitize (&gMemTemp, (PWSTR) pMeeting->m_memName.p);
      
      WCHAR szTemp[128];
      // hover help
      MemCat (&gMemTemp, L"<xHoverHelp>");
      if (((PWSTR) pMeeting->m_memDescription.p)[0]) {
         MemCatSanitize (&gMemTemp, (PWSTR) pMeeting->m_memDescription.p);
         MemCat (&gMemTemp, L"<p/>");
      }

      // location
      if (((PWSTR)pMeeting->m_memWhere.p)[0]) {
         MemCat (&gMemTemp, L"Location: <bold>");
         MemCatSanitize (&gMemTemp, (PWSTR) pMeeting->m_memWhere.p);
         MemCat (&gMemTemp, L"</bold><br/>");
      }

      // attendees
      MemCat (&gMemTemp, L"Attendees: <bold>");
      DWORD j;
      BOOL  fNameAlready;
      fNameAlready = FALSE;
      for (j = 0; j < MAXATTEND; j++) {
         if (pMeeting->m_adwAttend[j] == (DWORD)-1)
            continue;
         PWSTR psz;
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pMeeting->m_adwAttend[j]));
         if (!psz)
            continue;

         // add it
         if (fNameAlready)
            MemCat (&gMemTemp, L"; ");
         MemCatSanitize (&gMemTemp, psz);
         fNameAlready = TRUE;
      }
      if (!fNameAlready)
         MemCat (&gMemTemp, L"None");
      MemCat (&gMemTemp, L"</bold>");

      MemCat (&gMemTemp, L"</xHoverHelp>");

      // completed
      MemCat (&gMemTemp, L"</xtdtask>");

      MemCat (&gMemTemp, L"<xtdcompleted></xtdcompleted>"); // filler to space with the rest
      // last category
      MemCat (&gMemTemp, L"<xtdcompleted>");
      if ((pMeeting->m_dwStart != (DWORD) -1) && (pMeeting->m_dwEnd != (DWORD)-1)) {
         DFTIMEToString (pMeeting->m_dwStart, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L" to ");
         DFTIMEToString (pMeeting->m_dwEnd, szTemp);
         MemCat (&gMemTemp, szTemp);
      }
      else if (pMeeting->m_dwStart != (DWORD)-1) {
         DFTIMEToString (pMeeting->m_dwStart, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L" until the end of the day");
      }
      else if (pMeeting->m_dwEnd != (DWORD)-1) {
         MemCat (&gMemTemp, L"Early until ");
         DFTIMEToString (pMeeting->m_dwEnd, szTemp);
         MemCat (&gMemTemp, szTemp);
      }
      else {   // both -1
         MemCat (&gMemTemp, L"All day");
      }


      if (pMeeting->m_reoccur.m_dwPeriod) {
         MemCat (&gMemTemp, L"<br/><italic>");
         ReoccurToString (&pMeeting->m_reoccur, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"</italic>");
      }
      MemCat (&gMemTemp, L"</xtdcompleted></tr>");
   }

   // finish off list
   if (dwLastDate != (DWORD)-1)
      MemCat (&gMemTemp, L"</xtrmonth>");

   MemCat (&gMemTemp, L"</xlisttable>");
   return (PWSTR) gMemTemp.p;
}

/*****************************************************************************
SchedMeetingShowAddUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   DFDATE         dAddDate - Date to use. 0 => don't set
returns
   BOOL - TRUE if a project Meeting was added
*/
BOOL SchedMeetingShowAddUI (PCEscPage pPage, DFDATE dAddDate)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   SchedInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMEETINGADD, SchedMeetingAddPage, (PVOID) dAddDate);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}

/*****************************************************************************
SchedMeetingAddReoccurPage - Override page callback.
*/
BOOL SchedMeetingAddReoccurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // always have an alarm 15 minutes before
         AlarmControlSet (pPage, gszAlarm, 15);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // pressed add. Do some verification
            WCHAR szName[256];
            WCHAR szDescription[10024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in one-line summary of the meeting.");
               return TRUE;
            }
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);


            // add it
            PCSchedMeeting pMeeting;
            pMeeting = gCurSched.MeetingAdd (szName, szDescription, (DWORD)-1);
            if (!pMeeting) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // other data
            pControl = pPage->ControlFind(gszMeetingLoc);
            szDescription[0] = 0;
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pMeeting->m_memWhere);
            MemCat (&pMeeting->m_memWhere, szDescription);

            pMeeting->m_dwDate = 0;
            pMeeting->m_dwStart = TimeControlGet (pPage, gszMeetingStart);
            pMeeting->m_dwEnd = TimeControlGet (pPage, gszMeetingEnd);
            pMeeting->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);

            DWORD i;
            for (i = 0; i < MAXATTEND; i++) {
               pControl = pPage->ControlFind (gaszAttend[i]);
               if (!pControl)
                  continue;

               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_adwAttend[i] = dwNode;
            }


            // journal
            pControl = pPage->ControlFind (gszJournal);
            if (pControl) {
               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = JournalIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_iJournalID = (int) dwNode;
            }

            // fill in reoccur
            ReoccurFromControls (pPage, &pMeeting->m_reoccur);

            // bring it up to date
            pMeeting->BringUpToDate(Today());

            // exit this
            gCurSched.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
SchedMeetingShowAddReoccurUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project Meeting was added
*/
BOOL SchedMeetingShowAddReoccurUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   SchedInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMEETINGADDREOCCUR, SchedMeetingAddReoccurPage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}

/*****************************************************************************
SchedMeetingsPage - Override page callback.
*/
BOOL SchedMeetingsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // make sure initalized
      SchedInit ();
      break;   // fall through

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only care about projectlist
         if (!_wcsicmp(p->pszSubName, L"MEETINGS")) {
            p->pszSubString = SchedSummary (TRUE, 0);
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

         if (!_wcsicmp(p->pControl->m_pszName, L"addMeeting")) {
            if (SchedMeetingShowAddUI(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addreoccur")) {
            if (SchedMeetingShowAddReoccurUI(pPage))
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"takenotes")) {
            DWORD dwNext = SchedCreateMeetingNotes(NULL, TRUE);
            if (!dwNext)
               return TRUE;   // error
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", dwNext);
            pPage->Exit (szTemp);
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
         DWORD dwNext;
         if (SchedParseLink (p->psz, pPage, &fRefresh, &dwNext)) {
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


/*****************************************************************************
SchedMeetingEditPage - Override page callback.
*/
BOOL SchedMeetingEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCSchedMeeting pMeeting = (PCSchedMeeting) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"IFREOCCUR")) {
            p->pszSubString = pMeeting->m_dwParent ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFREOCCUR")) {
            p->pszSubString = pMeeting->m_dwParent ? L"" : L"</comment>";
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
            pControl->AttribSet (gszText, (PWSTR) pMeeting->m_memName.p);
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pMeeting->m_memDescription.p);
         pControl = pPage->ControlFind(gszMeetingLoc);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pMeeting->m_memWhere.p);


         // other data
         DateControlSet (pPage, gszMeetingDate, pMeeting->m_dwDate);
         TimeControlSet (pPage, gszMeetingStart, pMeeting->m_dwStart);
         TimeControlSet (pPage, gszMeetingEnd, pMeeting->m_dwEnd);
         AlarmControlSet (pPage, gszAlarm, pMeeting->m_dwAlarm);

         DWORD i;
         for (i = 0; i < MAXATTEND; i++) {
            if (pMeeting->m_adwAttend[i] == (DWORD)-1)
               continue;
            DWORD dw;
            dw = PeopleBusinessDatabaseToIndex(pMeeting->m_adwAttend[i]);

            pControl = pPage->ControlFind (gaszAttend[i]);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) dw);
         }

         // journal
         if (pMeeting->m_iJournalID != -1) {
            DWORD dw;
            dw = JournalDatabaseToIndex((DWORD)pMeeting->m_iJournalID);

            pControl = pPage->ControlFind (gszJournal);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) dw);
         }

      }
      break;   // do default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add") || !_wcsicmp(p->pControl->m_pszName, gszCompleted) ||
             !_wcsicmp(p->pControl->m_pszName, gszHadMeeting)) {
            BOOL fHadMeeting =!_wcsicmp(p->pControl->m_pszName, gszHadMeeting);
            BOOL fCompleted = fHadMeeting || !_wcsicmp(p->pControl->m_pszName, gszCompleted);

            // pressed add. Do some verification
            WCHAR szName[128];
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;
            szDescription[0] = 0;

            // see if there's a conflict (only if not editing now)
            if (!fCompleted && LookForConflict(DateControlGet(pPage,gszMeetingDate),
               TimeControlGet(pPage,gszMeetingStart), TimeControlGet(pPage,gszMeetingEnd),
               pMeeting->m_dwID)) {
                  if (IDYES != pPage->MBYesNo (L"The meeting conflicts with another meeting or call. Do you want to schedule it anyway?"))
                     return TRUE;
            }

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a one-line summary for the meeting.");
               return TRUE;
            }
            MemZero (&pMeeting->m_memName);
            MemCat (&pMeeting->m_memName, szName);

            // set values
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pMeeting->m_memDescription);
            MemCat (&pMeeting->m_memDescription, szDescription);

            // other data
            pControl = pPage->ControlFind(gszMeetingLoc);
            szDescription[0] = 0;
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pMeeting->m_memWhere);
            MemCat (&pMeeting->m_memWhere, szDescription);

            // don't reset alarm unless chage the date, time, etc
            DFDATE   dwOldDate;
            DFTIME   dwOldTime;
            DWORD    dwOldAlarm;
            dwOldDate = pMeeting->m_dwDate;
            dwOldTime = pMeeting->m_dwStart;
            dwOldAlarm = pMeeting->m_dwAlarm;

            pMeeting->m_dwDate = DateControlGet (pPage, gszMeetingDate);
            pMeeting->m_dwStart = TimeControlGet (pPage, gszMeetingStart);
            pMeeting->m_dwEnd = TimeControlGet (pPage, gszMeetingEnd);

            pMeeting->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);

            if ((dwOldDate != pMeeting->m_dwDate) || (dwOldTime != pMeeting->m_dwStart) || (dwOldAlarm != pMeeting->m_dwAlarm))
               CalcAlarmTime (pMeeting->m_dwDate, pMeeting->m_dwStart, pMeeting->m_dwAlarm,
                  &pMeeting->m_dwAlarmDate, &pMeeting->m_dwAlarmTime);

            DWORD i;
            for (i = 0; i < MAXATTEND; i++) {
               pControl = pPage->ControlFind (gaszAttend[i]);
               if (!pControl)
                  continue;

               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_adwAttend[i] = dwNode;
            }



            // journal
            pControl = pPage->ControlFind (gszJournal);
            if (pControl) {
               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = JournalIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_iJournalID = (int) dwNode;
            }

            // set dirty
            pMeeting->SetDirty();
            gCurSched.Flush();
            pPage->Exit (fCompleted ? (fHadMeeting ? gszHadMeeting : gszCompleted) : gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {

            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Meeting removed.");

            // delete it
            DWORD dwIndex;
            dwIndex = gCurSched.MeetingIndexByID (pMeeting->m_dwID);
            gCurSched.m_listMeetings.Remove(dwIndex);
            delete pMeeting;
            gCurSched.m_fDirty = TRUE;
            gCurSched.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
SchedMeetingEditReoccurPage - Override page callback.
*/
BOOL SchedMeetingEditReoccurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCSchedMeeting pMeeting = (PCSchedMeeting) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set up values
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind(gszName);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pMeeting->m_memName.p);
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pMeeting->m_memDescription.p);
         pControl = pPage->ControlFind(gszMeetingLoc);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pMeeting->m_memWhere.p);


         // other data
         DateControlSet (pPage, gszMeetingDate, pMeeting->m_dwDate);
         TimeControlSet (pPage, gszMeetingStart, pMeeting->m_dwStart);
         TimeControlSet (pPage, gszMeetingEnd, pMeeting->m_dwEnd);
         AlarmControlSet (pPage, gszAlarm, pMeeting->m_dwAlarm);

         DWORD i;
         for (i = 0; i < MAXATTEND; i++) {
            if (pMeeting->m_adwAttend[i] == (DWORD)-1)
               continue;
            DWORD dw;
            dw = PeopleBusinessDatabaseToIndex(pMeeting->m_adwAttend[i]);

            pControl = pPage->ControlFind (gaszAttend[i]);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) dw);
         }


         // journal
         if (pMeeting->m_iJournalID != -1) {
            DWORD dw;
            dw = JournalDatabaseToIndex((DWORD)pMeeting->m_iJournalID);

            pControl = pPage->ControlFind (gszJournal);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) dw);
         }

         // fill in reoccur
         ReoccurToControls (pPage, &pMeeting->m_reoccur);

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
               pPage->MBWarning (L"You must type in a one-line summary for the meeting.");
               return TRUE;
            }
            MemZero (&pMeeting->m_memName);
            MemCat (&pMeeting->m_memName, szName);

            // set values
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pMeeting->m_memDescription);
            MemCat (&pMeeting->m_memDescription, szDescription);

            // other data
            pControl = pPage->ControlFind(gszMeetingLoc);
            szDescription[0] = 0;
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pMeeting->m_memWhere);
            MemCat (&pMeeting->m_memWhere, szDescription);

            pMeeting->m_dwDate = 0;
            pMeeting->m_dwStart = TimeControlGet (pPage, gszMeetingStart);
            pMeeting->m_dwEnd = TimeControlGet (pPage, gszMeetingEnd);
            pMeeting->m_dwAlarm = AlarmControlGet (pPage, gszAlarm);

            DWORD i;
            for (i = 0; i < MAXATTEND; i++) {
               pControl = pPage->ControlFind (gaszAttend[i]);
               if (!pControl)
                  continue;

               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_adwAttend[i] = dwNode;
            }

            // journal
            pControl = pPage->ControlFind (gszJournal);
            if (pControl) {
               // get the values
               int   iCurSel;
               iCurSel = pControl->AttribGetInt (gszCurSel);

               // find the node number for the person's data and the name
               DWORD dwNode;
               dwNode = JournalIndexToDatabase ((DWORD)iCurSel);
               pMeeting->m_iJournalID = (int) dwNode;
            }

            // BUGFIX - loop throught all existing instances that are children and modify
            for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
               PCSchedMeeting pChild = gCurSched.MeetingGetByIndex(i);
               if (!pChild)
                  continue;
               if (pChild->m_dwParent != pMeeting->m_dwID)
                  continue;

               // found, so change
               MemZero (&pChild->m_memName);
               MemCat (&pChild->m_memName, (PWSTR) pMeeting->m_memName.p);
               MemZero (&pChild->m_memDescription);
               MemCat (&pChild->m_memDescription, (PWSTR) pMeeting->m_memDescription.p);
               MemZero (&pChild->m_memWhere);
               MemCat (&pChild->m_memWhere, (PWSTR) pMeeting->m_memWhere.p);
               pChild->m_dwStart = pMeeting->m_dwStart;
               pChild->m_dwEnd = pMeeting->m_dwEnd;
               memcpy (pChild->m_adwAttend, pMeeting->m_adwAttend, sizeof(pMeeting->m_adwAttend));
               pChild->m_iJournalID = pMeeting->m_iJournalID;

               pChild->m_dwAlarm = pMeeting->m_dwAlarm;
               CalcAlarmTime (pChild->m_dwDate, pChild->m_dwStart, pChild->m_dwAlarm,
                  &pChild->m_dwAlarmDate, &pChild->m_dwAlarmTime);

               pChild->SetDirty();

            }

            // fill in reoccur
            REOCCURANCE rOld;
            rOld = pMeeting->m_reoccur;

            ReoccurFromControls (pPage, &pMeeting->m_reoccur);

            // BUGFIX - if this has changed ask the user if they want to reschedule
            // all existing occurances
            if (memcmp(&rOld, &pMeeting->m_reoccur, sizeof(rOld))) {
               pPage->MBSpeakInformation (L"Any existing meeting instances have been rescheduled.",
                  L"Because you have changed how often the meeting occurs any meetings already "
                  L"generated by this reoccurring meeting have been rescheduled.");
               gCurSched.DeleteAllChildren (pMeeting->m_dwID);
               pMeeting->m_reoccur.m_dwLastDate = 0;
            }

            // bring it up to date
            pMeeting->BringUpToDate(Today());

            // set dirty
            pMeeting->SetDirty();
            gCurSched.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            int iRet;
            iRet = pPage->MBYesNo (L"Do you want to delete all instances of the meeting?",
               L"Because this is a reoccurring meeting it may have generated instances of the "
               L"meeting occurring on specific days. If you press 'Yes' you will delete them too. "
               L"'No' will leave them on your meeting list.", TRUE);
            if (iRet == IDCANCEL)
               return TRUE;   // exit

            // if delete children do so
            if (iRet == IDYES)
               gCurSched.DeleteAllChildren (pMeeting->m_dwID);

            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Reoccurring meeting deleted.");

            // delete it
            DWORD dwIndex;
            dwIndex = gCurSched.MeetingIndexByID (pMeeting->m_dwID);
            gCurSched.m_listMeetings.Remove(dwIndex);
            delete pMeeting;
            gCurSched.m_fDirty = TRUE;
            gCurSched.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
SchedParseLink - If a page uses a Sched substitution, which generates
"ma:XXXX" to indicate that a Sched link was called, call this. If it's
not a Sched link, returns FALSE. If it is, it deletes the Sched and
returns TRUE. The page should then refresh itself.

inputs
   PWSTR    pszLink - link
   PCEscPage pPage - page
   BOOL     *pfRefresh - set to TRUE if should refresh
   DWORD    *pdwNext - Norammly filled with 0. If this is NOT then
               the next page should be "e:XXXX" where XXX is *pdwNext
returns
   BOOL
*/
BOOL SchedParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, DWORD *pdwNext)
{
   HANGFUNCIN;
   *pdwNext = 0;

   // make sure its the right type
   if ((pszLink[0] != L'm') || (pszLink[1] != L'a') || (pszLink[2] != L':'))
      return FALSE;

   // make sure have Sched loaded
   SchedInit ();

   // get the Meeting
   PCSchedMeeting pMeeting;
   DWORD dwID;
   dwID = 0;
   AttribToDecimal (pszLink + 3, (int*) &dwID);
   pMeeting = gCurSched.MeetingGetByID (dwID);

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, 
      pMeeting->m_reoccur.m_dwPeriod ? IDR_MMLMEETINGEDITREOCCUR : IDR_MMLMEETINGEDIT,
      pMeeting->m_reoccur.m_dwPeriod ? SchedMeetingEditReoccurPage : SchedMeetingEditPage,
      pMeeting);


   // remember if should refresh
   *pfRefresh = (pszRet && !_wcsicmp(pszRet, gszOK));

   // if returned "completed" then need to add the meeting as a new node and reutrn
   // a special marker
   if (pszRet && (!_wcsicmp(pszRet, gszCompleted) || !_wcsicmp(pszRet, gszHadMeeting))) {
      // add as node
      DWORD dwNext;
      dwNext = SchedCreateMeetingNotes (pMeeting, !_wcsicmp(pszRet, gszCompleted));
      if (!dwNext)
         return TRUE;   // error

      // delete the meeting
      DWORD dwIndex;
      dwIndex = gCurSched.MeetingIndexByID (pMeeting->m_dwID);
      gCurSched.m_listMeetings.Remove(dwIndex);
      delete pMeeting;
      gCurSched.m_fDirty = TRUE;
      gCurSched.Flush();

      // need to move on
      *pdwNext = dwNext;
   }

   return TRUE;
}


/*****************************************************************************
MeetingLog - Log the meeting in 1) all the people's records, and 2) the daily log

inputs
   DWORD    dwMeetingNode - Meeting node to read the info from
*/
void MeetingLog (DWORD dwMeetingNode)
{
   HANGFUNCIN;
   // get all the info
   PCMMLNode   pNotes;
   pNotes = gpData->NodeGet(dwMeetingNode);
   if (!pNotes)
      return;

   PWSTR pszName;
   DFDATE date;
   DFTIME start, end;
   DWORD adwAttend[MAXATTEND];
   DWORD i;

   pszName = NodeValueGet (pNotes, gszName);
   date = (DFDATE)NodeValueGetInt (pNotes, gszDate, 0);
   start = (DFTIME)NodeValueGetInt (pNotes, gszStart, -1);
   end = (DFTIME)NodeValueGetInt (pNotes, gszEnd, -1);
   for (i = 0; i < MAXATTEND; i++)
      adwAttend[i] = (DWORD) NodeValueGetInt (pNotes, gaszAttend[i], -1);

   // person
   for (i = 0; i < MAXATTEND; i++) {
      PCMMLNode pPerson;
      pPerson = NULL;
      if (adwAttend[i] != (DWORD)-1)
         pPerson = gpData->NodeGet (adwAttend[i]);
      if (pPerson) {
         // prepend meeting
         WCHAR szHuge[10000];
         wcscpy (szHuge, L"Meeting: ");
         wcscat (szHuge, pszName ? pszName : L"");

         // overwrite if already exists
         NodeElemSet (pPerson, gszInteraction, szHuge, (int) dwMeetingNode, TRUE,
            date, start, end);

         gpData->NodeRelease (pPerson);

         // also note this in the journal log
         PersonLinkToJournal (adwAttend[i], szHuge, dwMeetingNode, date, start, end);

      }
   }

   // log in the daily journal
   WCHAR szHuge[10000];
   wcscpy (szHuge, L"Meeting: ");
   wcscat (szHuge, pszName ? pszName : L"Unknown");
   CalendarLogAdd (date, start, end, szHuge, dwMeetingNode);

   // log if the meeting is supposed to be
   DWORD dwJournal;
   dwJournal = (DWORD) NodeValueGetInt (pNotes, gszJournal, -1);
   if (dwJournal != (DWORD)-1)
      JournalLink (dwJournal, szHuge, dwMeetingNode, date, start, end);


   gpData->NodeRelease (pNotes);
   gpData->Flush();
}


/*****************************************************************************
SchedCreateMeetingNotes - Create a meeting notes database object. This
is either created from an existing meeting or if pmeeting is NULL, from
scratch.

inputs
   PCSchedMeeting    pMeeting - To create from. Can be NULL.
   BOOL              fNow - If TRUE the meeting is now, so set the start to now.
                     If FALSE, the meeting happened already so don't touch the meeting start.
returns
   DWORD - database node. 0 if error
*/
DWORD SchedCreateMeetingNotes (PCSchedMeeting pMeeting, BOOL fNow)
{
   HANGFUNCIN;
   PCMMLNode   pNode;
   DWORD dwNode;
   pNode = gpData->NodeAdd (gszMeetingNotesNode, &dwNode);
   if (!pNode)
      return 0;

   // set it up
   NodeValueSet (pNode, gszName, pMeeting ? (PWSTR) pMeeting->m_memName.p : L"Type in a short summary here.");
   NodeValueSet (pNode, gszDescription, pMeeting ? (PWSTR) pMeeting->m_memDescription.p : L"");
   NodeValueSet (pNode, gszSummary, L"");

   NodeValueSet (pNode, gszWhere, pMeeting ? (PWSTR) pMeeting->m_memWhere.p : L"");
   if (fNow) {
      NodeValueSet (pNode, gszDate, Today());
      NodeValueSet (pNode, gszEnd, (DWORD)-1);
      NodeValueSet (pNode, gszStart, Now() );   // always set the start to now
   }
   else {
      NodeValueSet (pNode, gszDate, (int) (pMeeting ? pMeeting->m_dwDate : Today()));
      NodeValueSet (pNode, gszEnd, (int) (pMeeting ? pMeeting->m_dwEnd :  (DWORD)-1));
      NodeValueSet (pNode, gszStart, (int) (pMeeting ? pMeeting->m_dwStart :  (DWORD)-1));
   }
   DWORD i;
   for (i = 0; i < MAXATTEND; i++)
      NodeValueSet (pNode, gaszAttend[i], pMeeting ? (int) pMeeting->m_adwAttend[i] : -1);

   // journal
   NodeValueSet (pNode, gszJournal, pMeeting ? pMeeting->m_iJournalID : -1);

   gpData->NodeRelease(pNode);

   // log this
   MeetingLog (dwNode);

   return dwNode;
}

/***************************************************************************
SchedSetView - Tells the meeting unit what meeting is being viewed/edited.

inputs
   DWORD    dwNode - index
returns
   BOOL - TRUE if success
*/
BOOL SchedSetView (DWORD dwNode)
{
   gdwNode = dwNode;
   return TRUE;
}

/***********************************************************************
SchedMeetingNotesViewPage - Page callback for viewing an existing user.
*/
BOOL SchedMeetingNotesViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         // make sure intiialized
         SchedInit ();
         MemZero (&gMemTemp);
         PCMMLNode   pNode;
         PWSTR psz;
         
         if (!_wcsicmp(p->pszSubName, L"MEETINGSUMMARY")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               psz = NodeValueGet (pNode, gszName);
               MemCat (&gMemTemp, (psz && psz[0]) ? psz : L"None");
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"DESCRIPTION")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               psz = NodeValueGet (pNode, gszDescription);
               MemCat (&gMemTemp, (psz && psz[0]) ? psz : L"None");
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"LOCATION")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               psz = NodeValueGet (pNode, gszWhere);
               MemCat (&gMemTemp, (psz && psz[0]) ? psz : L"Unknown");
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"TIME")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               WCHAR szTemp[64];
               DFDATEToString ((DFDATE) NodeValueGetInt (pNode, gszDate, 0), szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString ((DFTIME) NodeValueGetInt (pNode, gszStart, -1), szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L" to ");
               DFTIMEToString ((DFTIME) NodeValueGetInt (pNode, gszEnd, -1), szTemp);
               MemCat (&gMemTemp, szTemp);
               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"ATTENDEES")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               DWORD i;
               BOOL fAdded = FALSE;
               for (i = 0; i < MAXATTEND; i++) {
                  DWORD dwData;
                  dwData = (DWORD) NodeValueGetInt (pNode, gaszAttend[i], -1);
                  if (dwData == (DWORD)-1)
                     continue;

                  PWSTR psz;
                  psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(dwData));
                  if (!psz)
                     continue;

                  // make a link
                  MemCat (&gMemTemp, L"<a href=v:");
                  MemCat (&gMemTemp, (int) dwData);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"</a><br/>");
                  fAdded = TRUE;
               }

               if (!fAdded)
                  MemCat (&gMemTemp, L"None");

               gpData->NodeRelease(pNode);
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"MEETINGNOTES")) {
            pNode = gpData->NodeGet (gdwNode);
            if (pNode) {
               psz = NodeValueGet (pNode, gszSummary);
               MemCat (&gMemTemp, (psz && psz[0]) ? psz : L"None");
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
            // set the time
            TimeControlSet (pPage, gszMeetingEnd, Now());

            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }

         break;
      }
   };

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
MeetingActionOtherPage - Override page callback.
*/
BOOL MeetingActionOtherPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set the person
         PCEscControl pControl = pPage->ControlFind (L"person");
         if ((gdwActionPersonID != (DWORD)-1) && pControl)
            pControl->AttribSetInt (gszCurSel, (int)PeopleBusinessDatabaseToIndex (gdwActionPersonID));
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
            WCHAR szDescription[1024];
            PCEscControl pControl;
            DWORD dwNeeded;
            szDescription[0] = 0;

            // name
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            if (!szDescription[0]) {
               pPage->MBWarning (L"You must type in a description of the action item.");
               return TRUE;
            }

            // get the date it's due
            DFDATE   due;
            due = DateControlGet (pPage, gszCompleteBy);
            if (!due) {
               pPage->MBWarning (L"You must specify a completion date.");
               return TRUE;
            }

            // and the person
            pControl = pPage->ControlFind (L"person");
            int   iCurSel;
            iCurSel = -1;
            if (pControl)
               iCurSel = pControl->AttribGetInt (gszCurSel);
            PWSTR pszName;
            pszName = PeopleBusinessIndexToName ((DWORD)iCurSel);
            if (!pszName) {
               pPage->MBWarning (L"You must select the person or business responsible for the action item.");
               return TRUE;
            }

            // generate the string to add as a reminder
            WCHAR szTemp[64];
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"Action item assigned to ");
            MemCat (&gMemTemp, pszName);
            MemCat (&gMemTemp, L": ");
            MemCat (&gMemTemp, szDescription);
            ReminderAdd ((PWSTR)gMemTemp.p, due);

            // generate a string
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"\r\n\r\nACTION ITEM FOR ");
            MemCat (&gMemTemp, pszName);
            MemCat (&gMemTemp, L":\r\nDescription: ");
            MemCat (&gMemTemp, szDescription);
            MemCat (&gMemTemp, L"\r\nCompletion date: ");
            DFDATEToString (due, szTemp);
            MemCat (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\r\n\r\n");

            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
ActionItemOther - UI for adding an action item for someone else.

inputs
   PCEscPage   pPage - page
   DWORD       dwPersonID - ID of the person to assign to. -1 if no person.
*/
PWSTR ActionItemOther (PCEscPage pPage, DWORD dwPersonID)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // clear out the global temp-mem, because using that
   MemZero (&gMemTemp);
   gdwActionPersonID = dwPersonID;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      // BUGFIX - Adjust the size of window to size of data
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLMEETINGACTIONOTHER, MeetingActionOtherPage);

   return (PWSTR) gMemTemp.p;
}

/***********************************************************************
SchedMeetingNotesEditPage - Page callback for viewing an existing user.
*/
BOOL SchedMeetingNotesEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // just make sure have loaded in people info
         SchedInit();

         // fill in details
         PCEscControl pControl;
         PCMMLNode pNode;
         PWSTR psz;
         pNode = gpData->NodeGet (gdwNode);
         if (!pNode)
            break;

         psz = NodeValueGet (pNode, gszName);
         pControl = pPage->ControlFind(gszName);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);
         psz = NodeValueGet (pNode, gszDescription);
         pControl = pPage->ControlFind(gszDescription);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);
         psz = NodeValueGet (pNode, gszWhere);
         pControl = pPage->ControlFind(gszMeetingLoc);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);
         psz = NodeValueGet (pNode, gszSummary);
         pControl = pPage->ControlFind(gszSummary);
         if (pControl && psz)
            pControl->AttribSet (gszText, psz);


         // other data
         DateControlSet (pPage, gszMeetingDate, (DFDATE) NodeValueGetInt(pNode, gszDate, 0));
         TimeControlSet (pPage, gszMeetingStart, (DFTIME) NodeValueGetInt(pNode, gszStart, -1));
         TimeControlSet (pPage, gszMeetingEnd, (DFTIME) NodeValueGetInt(pNode, gszEnd, -1));

         // if there's no start time then set it to now
         if ((DWORD)-1 == TimeControlGet(pPage, gszMeetingStart))
            TimeControlSet (pPage, gszMeetingStart, Now());

         DWORD i;
         DWORD dw;
         for (i = 0; i < MAXATTEND; i++) {
            dw = PeopleBusinessDatabaseToIndex((DWORD)NodeValueGetInt(pNode, gaszAttend[i], -1));

            pControl = pPage->ControlFind (gaszAttend[i]);
            if (pControl)
               pControl->AttribSetInt (gszCurSel, (int) dw);
         }

         // journal
         dw = JournalDatabaseToIndex ((DWORD)NodeValueGetInt(pNode, gszJournal, -1));
         pControl = pPage->ControlFind (gszJournal);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) dw);


         gpData->NodeRelease(pNode);
      }
      break;

   case ESCM_LINK:
      {
         // this doesn't actually trap and links; it just saves the ifnormation
         // to disk just in case it exits

         // IMPORTANT - if user closes window the changes dont get saved

         // fill in details
         PCEscControl pControl;
         PCMMLNode pNode;
         DWORD dwNeeded;
         WCHAR szTemp[10000];
         pNode = gpData->NodeGet (gdwNode);
         if (!pNode)
            break;

         pControl = pPage->ControlFind(gszName);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszName, szTemp);

         pControl = pPage->ControlFind(gszDescription);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszDescription, szTemp);

         pControl = pPage->ControlFind(gszMeetingLoc);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszWhere, szTemp);

         pControl = pPage->ControlFind(gszSummary);
         szTemp[0] = 0;
         if (pControl)
            pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
         NodeValueSet (pNode, gszSummary, szTemp);

         // remove the old version of this because may change the date on
         // a journal entry and screw up the links
         DFDATE olddate;
         olddate = (DFDATE) NodeValueGetInt (pNode, gszDate, 0);
         CalendarLogRemove (olddate, gdwNode);


         // other data
         NodeValueSet (pNode, gszDate, (int) DateControlGet (pPage, gszMeetingDate));
         NodeValueSet (pNode, gszStart, TimeControlGet (pPage, gszMeetingStart));
         NodeValueSet (pNode, gszEnd, TimeControlGet (pPage, gszMeetingEnd));

         DWORD i;
         for (i = 0; i < MAXATTEND; i++) {
            pControl = pPage->ControlFind (gaszAttend[i]);
            if (!pControl)
               continue;
               // get the values
            int   iCurSel;
            iCurSel = pControl->AttribGetInt (gszCurSel);

            // find the node number for the person's data and the name
            DWORD dwNode;
            dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);

            NodeValueSet (pNode, gaszAttend[i], (int) dwNode);
         }

         // write out journal
         pControl = pPage->ControlFind (gszJournal);
         if (pControl) {
            // get the values
            int   iCurSel;
            iCurSel = pControl->AttribGetInt (gszCurSel);

            // find the node number for the person's data and the name
            DWORD dwNode;
            dwNode = JournalIndexToDatabase ((DWORD)iCurSel);
            NodeValueSet (pNode, gszJournal, (int) dwNode);
         }


         // write this to the logs
         MeetingLog (gdwNode);

         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszCompleted)) {
            // set the time
            if (TimeControlGet (pPage, gszMeetingEnd) == -1)
               TimeControlSet (pPage, gszMeetingEnd, Now());

            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"v:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"actionself") || !_wcsicmp(p->pControl->m_pszName, L"actionother")) {
            // show UI
            PWSTR psz;
            PCEscControl pControl = pPage->ControlFind(gaszAttend[0]);

            psz = (!_wcsicmp(p->pControl->m_pszName, L"actionself")) ?
               WorkActionItemAdd (pPage) :
               ActionItemOther(pPage, PeopleBusinessIndexToDatabase ((DWORD)(pControl ? pControl->AttribGetInt(gszCurSel) : -1)) );

            // set the selection and append to the end
            pControl = pPage->ControlFind (gszSummary);
            pControl->AttribSet (L"selend", L"1000000");
            pControl->AttribSet (L"selstart", L"1000000");
            ESCMEDITREPLACESEL rep;
            memset (&rep, 0, sizeof(rep));
            rep.dwLen = wcslen(psz);
            rep.psz = psz;
            pControl->Message (ESCM_EDITREPLACESEL, &rep);
            pControl->AttribSet (L"selend", L"1000000");
            pControl->AttribSet (L"selstart", L"1000000");
            pControl->Message (ESCM_EDITSCROLLCARET);
            pPage->FocusSet (pControl);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"addmeeting")) {
            SchedMeetingShowAddUI (pPage);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"schedulecall")) {
            PhoneCallShowAddUI (pPage);
            return TRUE;
         }


         break;
      }
   };

   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************
SchedMonthEnumerate - Enumerates a months worth of data.

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
void SchedMonthEnumerate (DFDATE date, PCMem *papMem, BOOL fWithLinks)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   SchedInit();
   // bring reoccurring Meetings up to date -
   // bring up a whole month ahead of time so guaranteed to at least
   // see on month's worth
   gCurSched.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24*30));
   gCurSched.Sort();  // make sure they're sorted
   DWORD i;
   PCSchedMeeting pMeeting;

   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);

      // filter out Meeting if reoccurring
      if (pMeeting->m_reoccur.m_dwPeriod)
         continue;

      // filter out non-month
      if ( (MONTHFROMDFDATE(pMeeting->m_dwDate) != MONTHFROMDFDATE(date)) ||
         (YEARFROMDFDATE(pMeeting->m_dwDate) != YEARFROMDFDATE(date)) )
         continue;

      // find the day
      int   iDay;
      iDay = (int) DAYFROMDFDATE(pMeeting->m_dwDate);
      if (!papMem[iDay]) {
         papMem[iDay] = new CMem;
         MemZero (papMem[iDay]);
      }

      // append
      MemCat (papMem[iDay], L"<li>");
      if (fWithLinks) {
         MemCat (papMem[iDay], L"<a href=ma:");
         MemCat (papMem[iDay], (int) pMeeting->m_dwID);
         MemCat (papMem[iDay], L">");
      }
      MemCatSanitize (papMem[iDay], (PWSTR) pMeeting->m_memName.p);
      if (fWithLinks)
         MemCat (papMem[iDay], L"</a>");
      MemCat (papMem[iDay], L" - ");

      // time
      WCHAR szTemp[64];
      MemCat (papMem[iDay], L"<bold>");
      if ((pMeeting->m_dwStart != (DWORD) -1) && (pMeeting->m_dwEnd != (DWORD)-1)) {
         DFTIMEToString (pMeeting->m_dwStart, szTemp);
         MemCat (papMem[iDay], szTemp);
         MemCat (papMem[iDay], L" to ");
         DFTIMEToString (pMeeting->m_dwEnd, szTemp);
         MemCat (papMem[iDay], szTemp);
      }
      else if (pMeeting->m_dwStart != (DWORD)-1) {
         DFTIMEToString (pMeeting->m_dwStart, szTemp);
         MemCat (papMem[iDay], szTemp);
         MemCat (papMem[iDay], L" until the end of the day");
      }
      else if (pMeeting->m_dwEnd != (DWORD)-1) {
         MemCat (papMem[iDay], L"Early until ");
         DFTIMEToString (pMeeting->m_dwEnd, szTemp);
         MemCat (papMem[iDay], szTemp);
      }
      else {   // both -1
         MemCat (papMem[iDay], L"All day");
      }
      MemCat (papMem[iDay], L"</bold>");
     
      
      // finish up table
      MemCat (papMem[iDay], L"</li>");

      // location
      if (((PWSTR)pMeeting->m_memWhere.p)[0]) {
         MemCat (papMem[iDay], L"<p>Location: <bold>");
         MemCatSanitize (papMem[iDay], (PWSTR) pMeeting->m_memWhere.p);
         MemCat (papMem[iDay], L"</bold></p>");
      }

      // attendees
      MemCat (papMem[iDay], L"<p>Attendees: <bold>");
      DWORD j;
      BOOL  fNameAlready;
      fNameAlready = FALSE;
      for (j = 0; j < MAXATTEND; j++) {
         if (pMeeting->m_adwAttend[j] == (DWORD)-1)
            continue;
         PWSTR psz;
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pMeeting->m_adwAttend[j]));
         if (!psz)
            continue;

         // add it
         if (fNameAlready)
            MemCat (papMem[iDay], L"; ");
         if (fWithLinks) {
            MemCat (papMem[iDay], L"<a href=v:");
            MemCat (papMem[iDay], (int) pMeeting->m_adwAttend[j]);
            MemCat (papMem[iDay], L">");
         }
         MemCatSanitize (papMem[iDay], psz);
         if (fWithLinks)
            MemCat (papMem[iDay], L"</a>");
         fNameAlready = TRUE;
      }
      if (!fNameAlready)
         MemCat (papMem[iDay], L"None");
      MemCat (papMem[iDay], L"</bold></p>");

   }
}

/*********************************************************************
HeightFromTime - Given the duration of a meeting, this figures out
how hight (as a percentage of the screen width) it should be.

inputs
   DWORD    dwStart, dwEnd - DFTIME
   BOOL     fMeeting - TRUE if it's a meeting, FALSE if it's a phone call
returns
   int - Percent value, 0+
*/

int HeightFromTime (DFTIME dwStart, DFTIME dwEnd, BOOL fMeeting = TRUE)
{
   HANGFUNCIN;
   if (dwStart == (DWORD)-1) {
      dwStart = fMeeting ? STARTOFDAY : dwEnd;
   }
   if (dwEnd == (DWORD)-1) {
      dwEnd = fMeeting ? ENDOFDAY : dwStart;
   }
   if ((dwStart == dwEnd) && !fMeeting)
      dwEnd += 30;   // 30 minutes. Because low-byte is minutes this will work
   if (dwStart >= dwEnd)
      return 0;

   int   iStart, iEnd;
   iStart = DFTIMEToMinutes(dwStart);
   iEnd = DFTIMEToMinutes(dwEnd);

   return (iEnd - iStart) * 10 / 60;
}

#if 0 // no longer used
/*********************************************************************
FillInTimes - Writes onto gMemTemp a table of times.

inputs
   DFTIME   dwStart, wdEnd - DFTIME
   BOOL     fMeeting - TRUE if it's a meeting, FALSE if it's a phone call
*/
void FillInTimes (DFTIME dwStart,DFTIME dwEnd, BOOL fMeeting = TRUE)
{
   HANGFUNCIN;
   if (fMeeting) {
      if (dwStart == (DWORD)-1)
         dwStart = STARTOFDAY;
      if (dwEnd == (DWORD)-1)
         dwEnd = ENDOFDAY;
      if (dwStart >= dwEnd)
         return;
   }
   else {
      if (dwStart == (DWORD)-1)
         dwStart = dwEnd;
      if (dwEnd == (DWORD)-1)
         dwEnd = dwStart;
      if (dwEnd == dwStart)
         dwEnd += 30;   // 30 mintes worth
   }

   MemCat (&gMemTemp, L"<small><table width=100% innerlines=1 border=1 valign=center align=center>");

   if (!fMeeting && ((dwStart == (DWORD)-1) || (dwEnd == (DWORD)-1))) {
      MemCat (&gMemTemp, L"<tr><td height=");
      MemCat (&gMemTemp, HeightFromTime(dwStart, dwEnd, fMeeting) * 5); // use *5 because this table is only 1/5th of other table width
      MemCat (&gMemTemp, L"%>");
      if (dwStart != (DWORD)-1) {
         WCHAR szTemp[64];
         DFTIMEToString (dwStart, szTemp);
         MemCat (&gMemTemp, szTemp);
      }
      else {
         // else, leave blank since no time really specified
         MemCat (&gMemTemp, L"Any time");
      }
      MemCat (&gMemTemp, L"</td></tr>");
   }
   else {
      DFTIME   dwNext;

      // BUGFIX - to prevent really long days, max 24 hour day
      DWORD dwMax = TODFTIME(24,0);
      if (dwEnd > dwMax) {
         HANGFUNCIN;
         dwEnd = dwMax;
      }

      DWORD dwLastStart = 0;
      for (; dwStart < dwEnd; dwStart = dwNext) {
         // find the next half hour increment
         int iStart;
         iStart = DFTIMEToMinutes (dwStart);
         iStart = iStart + 30;
         iStart -= iStart % 30;
         dwNext = TODFTIME(iStart / 60, iStart % 60);
         // BUGFIX - Prevent an infinite loop
         if (dwNext < dwLastStart) {
            dwNext = dwLastStart;
            HANGFUNCIN;
         }
         if (dwNext > dwEnd)
            dwNext = dwEnd;

         // section this off and write in the time
         MemCat (&gMemTemp, L"<tr><td height=");
         MemCat (&gMemTemp, HeightFromTime(dwStart, dwNext, fMeeting) * 5); // use *5 because this table is only 1/5th of other table width
         MemCat (&gMemTemp, L"%>");
         WCHAR szTemp[64];
         DFTIMEToString (dwStart, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"</td></tr>");
      }
   }

   MemCat (&gMemTemp, L"</table></small>");
}
#endif // 0

#if 0    // replaced by planner
/*****************************************************************************
SchedSummaryScale - Show meetings for one day, scaled so that there's
   a Microsoft Outlook type view with vertical amounts being equal time.

inputs
   DFDATE   date - date to show
   BOOL     fShowIfEmpty - If TRUE show even if it's empty
retursn
   PWSTR - string from gMemTemp
*/

typedef struct {
   BOOL     fMeeting;   // TRUE if meeting, FALSE if phone call
   DWORD    dwIndex;    // index into meetings or phone calls
} KEEP, PKEEP;

PWSTR SchedSummaryScale (DFDATE date, BOOL fShowIfEmpty)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   SchedInit();
   // bring reoccurring Meetings up to date. 1 day in advance so can use SchedSummary for tomrrow page
   gCurSched.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24));
   gCurSched.Sort();  // make sure they're sorted

   // do the same for phone calls
   PhoneInit();
   gCurPhone.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24));
   gCurPhone.Sort();  // make sure they're sorted

   DWORD i;
   PCSchedMeeting pMeeting;
   PCPhoneCall pCall;

   // find all the meetings and phone callsfor the date
   CListFixed  lMeetings, lCalls;
   DFTIME      dwEarliest;
   lMeetings.Init(sizeof(KEEP));
   lCalls.Init(sizeof(KEEP));
   KEEP keep;
   dwEarliest = (DWORD)-1;
   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);

      if (pMeeting->m_reoccur.m_dwPeriod || (pMeeting->m_dwDate != date))
         continue;

      // keep this
      if (!lMeetings.Num()) {
         dwEarliest = pMeeting->m_dwStart;
         if (dwEarliest == (DWORD)-1)
            dwEarliest = STARTOFDAY;
      }
      keep.dwIndex = i;
      keep.fMeeting = TRUE;
      lMeetings.Add (&keep);
   }
   for (i = 0; i < gCurPhone.m_listCalls.Num(); i++) {
      pCall = gCurPhone.CallGetByIndex(i);
      if (pCall->m_reoccur.m_dwPeriod)
         continue;

      DFDATE d;
      d = pCall->m_dwDate;
      if (!d)
         d = today;
      if (d != date)
         continue;

      // keep this
      if (dwEarliest == (DWORD)-1) {
         dwEarliest = pCall->m_dwStart;
         if (dwEarliest == (DWORD)-1)
            dwEarliest = STARTOFDAY;
      }
      else {
         // make sure earliest is correct
         DWORD dwTime;
         dwTime = pCall->m_dwStart;
         if (dwTime == (DWORD)-1)
            dwTime = STARTOFDAY;
         dwEarliest = min(dwEarliest, dwTime);
      }
      keep.dwIndex = i;
      keep.fMeeting = FALSE;
      lCalls.Add (&keep);
   }
   if (!fShowIfEmpty && !(lMeetings.Num()+lCalls.Num()))
      return L""; // nothing

   // else, we have Meetings. Show them off
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<xlisttable>");
   // BUGFIX - User request for add XXX button in today, tomorrow, etc. page
   MemCat (&gMemTemp, L"<xtrheader align=left>");
   MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddmeeting");
   MemCat (&gMemTemp, (int) date);
   MemCat (&gMemTemp, L"><small><font color=#ffffff>Add meeting</font></small></button>");
   MemCat (&gMemTemp, L"<align align=right><a href=r:113 color=#c0c0ff>Meetings</a> and</align>");
   MemCat (&gMemTemp, L"<br/>");
   MemCat (&gMemTemp, L"<button posn=edgeleft buttonheight=12 buttonwidth=12 buttondepth=6 valign=top margintopbottom=2 href=qzaddcall");
   MemCat (&gMemTemp, (int) date);
   MemCat (&gMemTemp, L"><small><font color=#ffffff>Schedule call</font></small></button>");
   MemCat (&gMemTemp, L"<align align=right><a href=r:117 color=#c0c0ff>Scheduled calls</a></align>");
   MemCat (&gMemTemp, L"</xtrheader>");
   //MemCat (&gMemTemp, L"<xtrheader><a href=r:113 color=#c0c0ff>Meetings</a></xtrheader>");

   DFTIME   dwCurTime;
   DWORD    dwCurMeeting, dwCurCall;
   BOOL     fAroundOnce;
   fAroundOnce = FALSE;
   DWORD dwMaxLength = 500;
   for (dwCurTime = min(dwEarliest,STARTOFDAY), dwCurMeeting = dwCurCall = 0; dwMaxLength; ) {
      // get the next meeting
      KEEP  *pk;
      pk = (KEEP*) lMeetings.Get(dwCurMeeting);
      if (pk)
         pMeeting = gCurSched.MeetingGetByIndex(pk->dwIndex);
      else
         pMeeting = NULL;
      pk = (KEEP*) lCalls.Get(dwCurCall);
      if (pk)
         pCall = gCurPhone.CallGetByIndex (pk->dwIndex);
      else
         pCall = NULL;
      BOOL fUseMeeting;
      if (pMeeting && !pCall)
         fUseMeeting = TRUE;
      else if (!pMeeting && pCall)
         fUseMeeting = FALSE;
      else if (pMeeting && pCall) {
         if (pCall->m_dwStart == (DWORD)-1)
            fUseMeeting = FALSE;
         else
            fUseMeeting = (pMeeting->m_dwStart <= pCall->m_dwStart);
      }
      else {
         // neither. If we've already done this once then exit
         if (fAroundOnce)
            break;
         fAroundOnce = TRUE;
         fUseMeeting = TRUE;
      }

      if (fUseMeeting)
         dwCurMeeting++;
      else
         dwCurCall++;

      // put in a blank time before now and the next meeting
      DWORD dwNextMtg;
      if (fUseMeeting)
         dwNextMtg = pMeeting ? pMeeting->m_dwStart : ENDOFDAY;
      else
         dwNextMtg = pCall ? pCall->m_dwStart : ENDOFDAY;
      if (dwNextMtg == (DWORD)-1)
         dwNextMtg = STARTOFDAY;

      // if there's a time delta between now and the next meeting then add a blank
      if (dwCurTime < dwNextMtg) {
         MemCat (&gMemTemp, L"<tr>");

         // times
         MemCat (&gMemTemp, L"<td width=20% lrmargin=0 tbmargin=0 bgcolor=#c0c0c0>");
         FillInTimes (dwCurTime, dwNextMtg);
         MemCat (&gMemTemp, L"</td>");

         // and show that it's empty
         MemCat (&gMemTemp, L"<td width=80% align=center valign=center>");
         MemCat (&gMemTemp, L"<big><italic><font color=#808080>Empty</font></italic></big>");
         MemCat (&gMemTemp, L"</td>");

         MemCat (&gMemTemp, L"</tr>");
      }

      // move on
      dwCurTime = dwNextMtg;

      // show meetings
      if (fUseMeeting) {
         if (!pMeeting)
            continue;
      }
      else {
         if (!pCall)
            continue;
      }

      // BUGFIX - make sure not too many meetings
      dwMaxLength--;

      MemCat (&gMemTemp, L"<tr>");
      MemCat (&gMemTemp, L"<td width=20% valign=center lrmargin=0 tbmargin=0 bgcolor=#808080><bold>");
      if (fUseMeeting)
         FillInTimes (pMeeting->m_dwStart, pMeeting->m_dwEnd, fUseMeeting);
      else
         FillInTimes (pCall->m_dwStart, pCall->m_dwEnd, fUseMeeting);
      MemCat (&gMemTemp, L"</bold></td>");

      if (fUseMeeting) {
         // and show that it's empty
         MemCat (&gMemTemp, L"<td width=80%><small>");
         // display Meeting info
         MemCat (&gMemTemp, L"<a href=ma:");
         MemCat (&gMemTemp, (int) pMeeting->m_dwID);
         MemCat (&gMemTemp, L">");
         // BUGFIX - Wash crashing with ampersand
         MemCatSanitize (&gMemTemp, (PWSTR) pMeeting->m_memName.p);

         if (((PWSTR) pMeeting->m_memDescription.p)[0]) {
            MemCat (&gMemTemp, L"<xhoverhelp>");
            MemCatSanitize (&gMemTemp, (PWSTR) pMeeting->m_memDescription.p);
            MemCat (&gMemTemp, L"</xHoverHelp>");
         }
         MemCat (&gMemTemp, L"</a>");

         MemCat (&gMemTemp, L" <italic>(Meeting)</italic>");

         MemCat (&gMemTemp, L"<p/>");
         // location
         if (((PWSTR)pMeeting->m_memWhere.p)[0]) {
            MemCat (&gMemTemp, L"Location: <bold>");
            MemCatSanitize (&gMemTemp, (PWSTR) pMeeting->m_memWhere.p);
            MemCat (&gMemTemp, L"</bold><br/>");
         }

         // attendees
         MemCat (&gMemTemp, L"Attendees: <bold>");
         DWORD j;
         BOOL  fNameAlready;
         fNameAlready = FALSE;
         for (j = 0; j < MAXATTEND; j++) {
            if (pMeeting->m_adwAttend[j] == (DWORD)-1)
               continue;
            PWSTR psz;
            psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pMeeting->m_adwAttend[j]));
            if (!psz)
               continue;

            // add it
            if (fNameAlready)
               MemCat (&gMemTemp, L"; ");
            MemCat (&gMemTemp, L"<a href=v:");
            MemCat (&gMemTemp, pMeeting->m_adwAttend[j]);
            MemCat (&gMemTemp, L">");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"</a>");
            fNameAlready = TRUE;
         }
         if (!fNameAlready)
            MemCat (&gMemTemp, L"None");
         MemCat (&gMemTemp, L"</bold>");

         // completed
         MemCat (&gMemTemp, L"</small></td>");
         MemCat (&gMemTemp, L"</tr>");

         // increase time
         dwCurTime = pMeeting->m_dwEnd;
      }
      else {   // phone call
         // and show that it's empty
         MemCat (&gMemTemp, L"<td width=80%><small>");
         // display call info
         MemCat (&gMemTemp, L"<a href=sp:");
         MemCat (&gMemTemp, (int) pCall->m_dwID);
         MemCat (&gMemTemp, L">");
         PWSTR psz;
         if (pCall->m_dwAttend == (DWORD)-1)
            psz = L"Unknown";
         else
            psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pCall->m_dwAttend));
         if (!psz)
            continue;

         MemCatSanitize (&gMemTemp, psz);

         if (((PWSTR) pCall->m_memDescription.p)[0]) {
            MemCat (&gMemTemp, L"<xhoverhelp>");
            MemCatSanitize (&gMemTemp, (PWSTR) pCall->m_memDescription.p);
            MemCat (&gMemTemp, L"</xHoverHelp>");
         }
         MemCat (&gMemTemp, L"</a>");

         MemCat (&gMemTemp, L" <italic>(Scheduled call)</italic>");

         // completed
         MemCat (&gMemTemp, L"</small></td>");
         MemCat (&gMemTemp, L"</tr>");

         // increase time
         if (pCall->m_dwEnd != (DWORD)-1)
            dwCurTime = pCall->m_dwEnd;
      }

      if (dwCurTime == (DWORD)-1)
         dwCurTime = ENDOFDAY;
   } // over times

#if 0
   DWORD dwLastDate;
   dwLastDate = (DWORD)-1;
   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);

      // filter out Meeting if !fAll
      if (!fAll) {
         if (pMeeting->m_reoccur.m_dwPeriod || (pMeeting->m_dwDate > date))
            continue;
      }

      // if different priority then show
      if (pMeeting->m_dwDate != dwLastDate) {
         if (dwLastDate != (DWORD)-1)
            MemCat (&gMemTemp, L"</xtrmonth>");
         dwLastDate = pMeeting->m_dwDate;

         MemCat (&gMemTemp, L"<xtrmonth><xmeetingdate>");
         if (dwLastDate) {
            WCHAR szTemp[64];
            DFDATEToString (dwLastDate, szTemp);
            MemCat (&gMemTemp, szTemp);
         }
         else
            MemCat (&gMemTemp, L"Reoccurring");
         MemCat (&gMemTemp, L"</xmeetingdate>");
      }


      MemCat (&gMemTemp, L"<xtdcompleted></xtdcompleted>"); // filler to space with the rest
      // last category
      MemCat (&gMemTemp, L"<xtdcompleted>");


      if (pMeeting->m_reoccur.m_dwPeriod) {
         MemCat (&gMemTemp, L"<br/><italic>");
         ReoccurToString (&pMeeting->m_reoccur, szTemp);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"</italic>");
      }
      MemCat (&gMemTemp, L"</xtdcompleted></tr>");
   }

   // finish off list
   if (dwLastDate != (DWORD)-1)
      MemCat (&gMemTemp, L"</xtrmonth>");
#endif // 0

   MemCat (&gMemTemp, L"</xlisttable>");
   return (PWSTR) gMemTemp.p;
}
#endif // 0

// BUGBUG - 2.0 - ability to record meetings


/*******************************************************************************
SchedEnumForPlanner - Enumerates the meetings in a format the planner can use.

inputs
   DFDATE      date - start date
   DWORD       dwDays - # of days to generate
   PCListFixed palDays - Pointer to an array of CListFixed. Number of elements
               is dwDays. These will be concatenated with relevent information.
               Elements in the list are PLANNERITEM strctures.
returns
   none
*/
void SchedEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays)
{
   HANGFUNCIN;
   DFDATE today = Today();

   // first of all, amke sure there are Meetings to show
   SchedInit();
   // bring reoccurring Meetings up to date. 1 day in advance so can use SchedSummary for tomrrow page
   gCurSched.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(date)+(60*24)*(dwDays+1)));
   gCurSched.Sort();  // make sure they're sorted

   DWORD i;
   PCSchedMeeting pMeeting;

   // figure out when the upcoming days are
#define  MAXDAYS  28
   DFDATE   adDay[MAXDAYS], dMax;
   dwDays = min(dwDays,MAXDAYS);
   for (i = 0; i < dwDays; i++)
      dMax = adDay[i] = MinutesToDFDATE(DFDATEToMinutes(date)+(60*24)*i);


   // find all the meetings and phone callsfor the date
   PLANNERITEM pi;
   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);

      if (pMeeting->m_reoccur.m_dwPeriod || (pMeeting->m_dwDate < date) || (pMeeting->m_dwDate > dMax) )
         continue;

      // figure out which date this is on
      DWORD d;
      for (d = 0; d < dwDays; d++)
         if (adDay[d] == pMeeting->m_dwDate)
            break;
      if (d >= dwDays)
         continue;


      // and the time
      DFTIME dwStart, dwEnd;
      dwStart = pMeeting->m_dwStart;
      dwEnd = pMeeting->m_dwEnd;
      if ((dwStart != (DWORD)-1) && (dwEnd != (DWORD)-1) && (dwStart > dwEnd)) {
         // swap
         DFTIME dw2;
         dw2 = dwEnd;
         dwEnd = dwStart;
         dwStart = dw2;
      }
      if (dwStart == (DWORD)-1) {
         dwStart = STARTOFDAY;
      }
      if (dwEnd == (DWORD)-1) {
         dwEnd = ENDOFDAY;
      }
      if (dwStart >= dwEnd) {
         int iEnd;
         iEnd = DFTIMEToMinutes(dwStart)+30;
         dwEnd = TODFTIME(iEnd / 60, iEnd % 60);
      }

      // remember this
      memset (&pi, 0, sizeof(pi));
      pi.dwDuration = DFTIMEToMinutes(dwEnd) - DFTIMEToMinutes(dwStart);
      pi.dwMajor = pMeeting->m_dwID;
      pi.dwTime = dwStart;
      pi.dwType = 0;
      pi.fFixedTime = TRUE;
      pi.pMemMML = new CMem;
      PCMem pMem = pi.pMemMML;

      // MML
      MemZero (pMem);
      if (!gfPrinting) {
         MemCat (pMem, L"<a href=ma:");
         MemCat (pMem, (int) pMeeting->m_dwID);
         MemCat (pMem, L">");
      }
      // BUGFIX - Wash crashing with ampersand
      MemCatSanitize (pMem, (PWSTR) pMeeting->m_memName.p);

      if (!gfPrinting && ((PWSTR) pMeeting->m_memDescription.p)[0]) {
         MemCat (pMem, L"<xhoverhelp>");
         MemCatSanitize (pMem, (PWSTR) pMeeting->m_memDescription.p);
         MemCat (pMem, L"</xHoverHelp>");
      }
      if (!gfPrinting) {
         MemCat (pMem, L"</a>");
      }

      MemCat (pMem, L" <italic>(Meeting)</italic>");

      MemCat (pMem, L"<p/>");
      // location
      if (((PWSTR)pMeeting->m_memWhere.p)[0]) {
         MemCat (pMem, L"Location: <bold>");
         MemCatSanitize (pMem, (PWSTR) pMeeting->m_memWhere.p);
         MemCat (pMem, L"</bold><br/>");
      }

      // attendees
      MemCat (pMem, L"Attendees: <bold>");
      DWORD j;
      BOOL  fNameAlready;
      fNameAlready = FALSE;
      for (j = 0; j < MAXATTEND; j++) {
         if (pMeeting->m_adwAttend[j] == (DWORD)-1)
            continue;
         PWSTR psz;
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pMeeting->m_adwAttend[j]));
         if (!psz)
            continue;

         // add it
         if (fNameAlready)
            MemCat (pMem, L"; ");
         if (!gfPrinting) {
            MemCat (pMem, L"<a href=v:");
            MemCat (pMem, pMeeting->m_adwAttend[j]);
            MemCat (pMem, L">");
         }
         MemCatSanitize (pMem, psz);
         if (!gfPrinting) {
            MemCat (pMem, L"</a>");
         }
         fNameAlready = TRUE;
      }
      if (!fNameAlready)
         MemCat (pMem, L"None");
      MemCat (pMem, L"</bold>");


      // add it
      palDays[d].Add(&pi);
   }

}

/*******************************************************************************
SchedCheckAlarm - Check to see if any alarms go off.

inputs
   DFDATE      today - today's date
   DFTIME      now - current time
*/
void SchedCheckAlarm (DFDATE today, DFTIME now)
{
   HANGFUNCIN;
   // first of all, amke sure there are Meetings to show
   SchedInit();
   // don't do either of these to save CPU
   //gCurSched.BringUpToDate (MinutesToDFDATE(DFDATEToMinutes(today)+60*24));
   //gCurSched.Sort();  // make sure they're sorted

   DWORD i;
   PCSchedMeeting pMeeting;

   for (i = 0; i < gCurSched.m_listMeetings.Num(); i++) {
      pMeeting = gCurSched.MeetingGetByIndex(i);

      if (pMeeting->m_reoccur.m_dwPeriod || (!pMeeting->m_dwAlarmDate) )
         continue;
      if (pMeeting->m_dwAlarmDate > today)
         continue;
      if ((pMeeting->m_dwAlarmDate == today) && (pMeeting->m_dwAlarmTime > now))
         continue;

      // else, have meeting
      CMem  mSub, mMain, *pMem;
      MemZero (&mSub);
      MemZero (&mMain);
      pMem = &mSub;

      // main
      WCHAR szTemp[64];
      MemCat (&mMain, L"You have a meeting at ");
      DFTIMEToString (pMeeting->m_dwStart, szTemp);
      MemCat (&mMain, szTemp);
      if (pMeeting->m_dwDate != today) {
         MemCat (&mMain, L" on ");
         DFDATEToString (pMeeting->m_dwDate, szTemp);
         MemCat (&mMain, szTemp);
      }
      MemCat (&mMain, L".");

      // BUGFIX - Wash crashing with ampersand
      MemCat (pMem, L"<bold>");
      MemCatSanitize (pMem, (PWSTR) pMeeting->m_memName.p);
      MemCat (pMem, L"</bold>");

      if (((PWSTR) pMeeting->m_memDescription.p)[0]) {
         MemCat (pMem, L"<br/>");
         MemCatSanitize (pMem, (PWSTR) pMeeting->m_memDescription.p);
      }

      MemCat (pMem, L"<p/>");
      // location
      if (((PWSTR)pMeeting->m_memWhere.p)[0]) {
         MemCat (pMem, L"Location: <bold>");
         MemCatSanitize (pMem, (PWSTR) pMeeting->m_memWhere.p);
         MemCat (pMem, L"</bold><br/>");
      }

      // attendees
      MemCat (pMem, L"Attendees: <bold>");
      DWORD j;
      BOOL  fNameAlready;
      fNameAlready = FALSE;
      for (j = 0; j < MAXATTEND; j++) {
         if (pMeeting->m_adwAttend[j] == (DWORD)-1)
            continue;
         PWSTR psz;
         psz = PeopleBusinessIndexToName (PeopleBusinessDatabaseToIndex(pMeeting->m_adwAttend[j]));
         if (!psz)
            continue;

         // add it
         if (fNameAlready)
            MemCat (pMem, L"; ");
         MemCatSanitize (pMem, psz);
         fNameAlready = TRUE;
      }
      if (!fNameAlready)
         MemCat (pMem, L"None");
      MemCat (pMem, L"</bold>");

      // UI
      DWORD dwSnooze;
      dwSnooze = AlarmUI ((PWSTR)mMain.p, (PWSTR)mSub.p);

      if (dwSnooze) {
         __int64 iMin;
         iMin = DFDATEToMinutes(Today()) +
            DFTIMEToMinutes(Now()) +
            (__int64) dwSnooze;
         pMeeting->m_dwAlarmDate = MinutesToDFDATE(iMin);
         iMin -= DFDATEToMinutes (pMeeting->m_dwAlarmDate);
         pMeeting->m_dwAlarmTime = TODFTIME(iMin / 60, iMin % 60);
      }
      else {
         pMeeting->m_dwAlarmDate = 0;
         pMeeting->m_dwAlarmTime = 0;
         pMeeting->m_dwAlarm = 0;
      }

      // flush
      gCurSched.m_fDirty = TRUE;
      gCurSched.Flush();

   }
}
