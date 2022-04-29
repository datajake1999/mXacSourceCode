/*************************************************************************
Event.cpp - For self-programmed holidays and stuff

begun July 4 2001 by Mike Rozak
Copyright 2001 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


/* C++ objects */
class CEventDay;
typedef CEventDay * PCEventDay;

// Event
class CEvent {
public:
   CEvent (void);
   ~CEvent (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCEventDay DayAdd (PWSTR pszName, PWSTR pszDescription, DWORD dwBefore);
   PCEventDay DayGetByIndex (DWORD dwIndex);
   PCEventDay DayGetByID (DWORD dwID);
   DWORD DayIndexByID (DWORD dwID);
   void Sort (void);


   CListFixed     m_listDays;   // list of PCEventDays, ordered according order of doing

   BOOL           m_fDirty;      // set to TRUE if the Event is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   DWORD          m_dwNextDayID;   // next Day ID created

private:

};

typedef CEvent * PCEvent;


// Day
class CEventDay {
public:
   CEventDay (void);
   ~CEventDay (void);

   PCEvent         m_pEvent; // Event that this belongs to. If change sets dirty
   CMem           m_memName;  // name of the Day
   DWORD          m_dwID;     // unique ID for the Day.
   DWORD          m_dwMonth;   // month it occurs on
   DWORD          m_dwRule;   // rule used to govern when it happens
#define  ERULE_DOM         0  // occurs on the day of the month as specified
#define  ERULE_WEEKDAY     1  // occurs on the first(see m_dwOrinal) m_dwWeekday of the month
#define  ERULE_AFTER       2  // occurs on the first(see m_dwOrdinal) m_dwWeekday on/after the day specified
   DWORD          m_dwDOM;    // day of month. depends on rule
   DWORD          m_dwWeekday;   // weekday used on rule. See R_WEEKDAY..R_SUNDARY
   DWORD          m_dwOrdinal;   // ordinal used in rule. See R_FIRST...R_LAST


   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);
   DWORD Priority (void);
private:
};

/* globals */
BOOL           gfEventPopulated = FALSE;// set to true if glistEvent is populated
CEvent       gCurEvent;      // current Event that Eventing with

static WCHAR gszEventDate[] = L"Eventdate";
static WCHAR gszDaysOfEvent[] = L"DaysOfEvent";
static WCHAR gszCompleteBy[] = L"completeby";
static WCHAR gszDay[] = L"Day";
static WCHAR gszMonth[] = L"Month";
static WCHAR gszRule[] = L"Rule";
static WCHAR gszDOM[] = L"DOM";
static WCHAR gszWeekday[] = L"Weekday";
static WCHAR gszOrdinal[] = L"Ordinal";
static WCHAR gszNextDayID[] = L"NextDayID";
static WCHAR gszDay1[] = L"day1";
static WCHAR gszDay3[] = L"day3";
static WCHAR gszMonth1[] = L"month1";
static WCHAR gszMonth2[] = L"month2";
static WCHAR gszMonth3[] = L"month3";
static WCHAR gszOrdinal2[] = L"ordinal2";
static WCHAR gszOrdinal3[] = L"ordinal3";
static WCHAR gszWeekday2[] = L"weekday2";
static WCHAR gszWeekday3[] = L"weekday3";
static WCHAR gszAfter[] = L"after";

/*************************************************************************
CEventDay::constructor and destructor
*/
CEventDay::CEventDay (void)
{
   HANGFUNCIN;
   m_pEvent = NULL;
   m_dwID = 0;
   m_memName.CharCat (0);
   m_dwMonth = 1;
   m_dwRule = ERULE_DOM;
   m_dwDOM = 1;
   m_dwWeekday = R_WEEKDAY;
   m_dwOrdinal = R_FIRST;
}

CEventDay::~CEventDay (void)
{
   HANGFUNCIN;
   // intentionally left blank
}


/*************************************************************************
CEventDay::SetDirty - Sets the dirty flag in the main Event
*/
void CEventDay::SetDirty (void)
{
   HANGFUNCIN;
   if (m_pEvent)
      m_pEvent->m_fDirty = TRUE;
}

/*************************************************************************
CEventDay::Write - Writes the Day information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pParent - node to create sub-node in
reutrns
   BOOL - TRUE if successful
*/
BOOL CEventDay::Write (PCMMLNode pParent)
{
   HANGFUNCIN;
   PCMMLNode   pSub;

   pSub = pParent->ContentAddNewNode ();
   if (!pSub)
      return FALSE;

   // set the name
   pSub->NameSet (gszDay);

   // write out parameters
   NodeValueSet (pSub, gszName, (PWSTR) m_memName.p);
   NodeValueSet (pSub, gszID, (int) m_dwID);
   NodeValueSet (pSub, gszMonth, (int) m_dwMonth);
   NodeValueSet (pSub, gszRule, (int) m_dwRule);
   NodeValueSet (pSub, gszDOM, (int) m_dwDOM);
   NodeValueSet (pSub, gszWeekday, (int) m_dwWeekday);
   NodeValueSet (pSub, gszOrdinal, (int) m_dwOrdinal);

   return TRUE;
}


/*************************************************************************
CEventDay::Read - Writes the Day information to the node, creating
a new subnode.

inputs
   PCMMLNOde      pSub - node to containing the Day data
reutrns
   BOOL - TRUE if successful
*/
BOOL CEventDay::Read (PCMMLNode pSub)
{
   HANGFUNCIN;
   PWSTR psz;

   psz = NodeValueGet (pSub, gszName);
   MemZero (&m_memName);
   if (psz)
      MemCat (&m_memName, psz);

   m_dwID = (DWORD) NodeValueGetInt (pSub, gszID, 1);
   m_dwMonth = (DWORD) NodeValueGetInt (pSub, gszMonth, 1);
   m_dwRule = (DWORD) NodeValueGetInt (pSub, gszRule, ERULE_DOM);
   m_dwDOM = (DWORD) NodeValueGetInt (pSub, gszDOM, 1);
   m_dwWeekday = (DWORD) NodeValueGetInt (pSub, gszWeekday, R_WEEKDAY);
   m_dwOrdinal = (DWORD) NodeValueGetInt (pSub, gszOrdinal, R_FIRST);

   return TRUE;
}

/*************************************************************************
CEvent::Constructor and destructor - Initialize
*/
CEvent::CEvent (void)
{
   HANGFUNCIN;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;
   m_dwNextDayID = 1;

   m_listDays.Init (sizeof(PCEventDay));
}

CEvent::~CEvent (void)
{
   HANGFUNCIN;
   // just call Init() to make sure the prvious one is flushed if necessary
   // and that the new Event is cleared
   Init();
}


/************************************************************************
CEvent::DayAdd - Adds a mostly default Day.

inputs
   PWSTR    pszName - name
   PWSTR    pszDescription - description
   DWORD    dwBefore - Day index to insert before. -1 for after
returns
   PCEventDay - To the Day object, to be added further
*/
PCEventDay CEvent::DayAdd (PWSTR pszName, PWSTR pszDescription,
                                 DWORD dwBefore)
{
   HANGFUNCIN;
   PCEventDay pDay;
   pDay = new CEventDay;
   if (!pDay)
      return NULL;

   pDay->m_dwID = m_dwNextDayID++;
   m_fDirty = TRUE;
   pDay->m_pEvent = this;

   MemZero (&pDay->m_memName);
   MemCat (&pDay->m_memName, pszName);
  
   // add this to the list
   if (dwBefore < m_listDays.Num())
      m_listDays.Insert(dwBefore, &pDay);
   else
      m_listDays.Add (&pDay);

   return pDay;
}

/*************************************************************************
DayGetByIndex - Given an index, this returns a Day, or NULL if can't find.
*/
PCEventDay CEvent::DayGetByIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   PCEventDay pDay, *ppDay;

   ppDay = (PCEventDay*) m_listDays.Get(dwIndex);
   if (!ppDay)
      return NULL;
   pDay = *ppDay;
   return pDay;
}

/*************************************************************************
DayGetByID - Given a Day ID, this returns a Day, or NULL if can't find.
*/
PCEventDay CEvent::DayGetByID (DWORD dwID)
{
   HANGFUNCIN;
   PCEventDay pDay;
   DWORD i;
   for (i = 0; i < m_listDays.Num(); i++) {
      pDay = DayGetByIndex (i);
      if (!pDay)
         continue;

      if (pDay->m_dwID == dwID)
         return pDay;
   }

   // else, cant find
   return NULL;
}

/*************************************************************************
DayIndexByID - Given a Day ID, this returns a an index, or -1 if can't find.
*/
DWORD CEvent::DayIndexByID (DWORD dwID)
{
   HANGFUNCIN;
   PCEventDay pDay;
   DWORD i;
   if (!dwID)
      return (DWORD) -1;

   for (i = 0; i < m_listDays.Num(); i++) {
      pDay = DayGetByIndex (i);
      if (!pDay)
         continue;

      if (pDay->m_dwID == dwID)
         return i;
   }

   // else, cant find
   return (DWORD)-1;
}

/*************************************************************************
CEvent::Write - Writes all the Event data to m_dwDatabaseNode.
Any elements of the node already there are overwritten. If the
Event object doesn't understand the meaning of a sub-node then it's
ignored. At the end this causes the node file to be written to disk.

returns
   BOOL - TRUE if success.
*/
BOOL CEvent::Write (void)
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

   if (!NodeValueSet (pNode, gszNextDayID, m_dwNextDayID))
      return FALSE;


   // BUGBUG - 2.0 - at some point write out last modified, for search purposes

   // remove existing Days
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   for (i = dwNum - 1; i < dwNum; i--) {
      PCMMLNode   pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);
      if (!pSub)
         continue;
      if (_wcsicmp(pSub->NameGet(), gszDay))
         continue;

      // else, old Day. delete
      pNode->ContentRemove (i);
   }

   // write out other info, such as Days, completed and not
   PCEventDay pDay;
   for (i = 0; i < m_listDays.Num(); i++) {
      pDay = *((PCEventDay*) m_listDays.Get(i));
      pDay->Write (pNode);
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
CEvent::Flush - If m_fDirty is set it writes out the Event and
clears the flag. If not, it does nothing

returns
   BOOL - TRUE if success
*/
BOOL CEvent::Flush (void)
{
   HANGFUNCIN;
   if (!m_fDirty)
      return TRUE;

   return Write ();
}

/*************************************************************************
CEvent::Init(void) - Initializes a blank Event. Init() also calls Flush()
just to make sure anything unsaved from previous is written.

returns
   BOOL - TRUE if succes
*/
BOOL CEvent::Init (void)
{
   HANGFUNCIN;
   // flush
   if (m_dwDatabaseNode && !Flush())
      return FALSE;

   // wipe out existing databased allocated
   DWORD i;
   PCEventDay pDay;
   for (i = 0; i < m_listDays.Num(); i++) {
      pDay = *((PCEventDay*) m_listDays.Get(i));
      delete pDay;
   }
   m_listDays.Clear();

   // new values for name, description, and days per week
   m_dwNextDayID = 1;
   m_fDirty = FALSE;
   m_dwDatabaseNode = 0;

   // sort
   Sort ();

   return TRUE;
}

/*************************************************************************
CEvent::Init(dwDatabaseNode) - Initializes by reading the Event in
from file. This first calls Init(void), and then reads through the database.
It verified it's really a Event, and loads in all the info.

inputs
   DWORD    dwDatabaseNode - database
returns
   BOOL - TRUE if success
*/
BOOL CEvent::Init (DWORD dwDatabaseNode)
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
   if (_wcsicmp(pNode->NameGet(), gszEventListNode)) {
      // invalid name
      gpData->NodeRelease(pNode);
      return FALSE;
   }
   m_dwDatabaseNode = dwDatabaseNode;


   // read in the name, description, and daysperweek
   m_dwNextDayID = NodeValueGetInt (pNode, gszNextDayID, 1);

   // read in other Days
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
      if (!_wcsicmp(pSub->NameGet(), gszDay))
         fCompleted = FALSE;
      else
         continue;   // not looking for this

      PCEventDay pDay;
      pDay = new CEventDay;
      pDay->Read (pSub);
      pDay->m_pEvent = this;


      // add to list
      m_listDays.Add (&pDay);
   }


   // sort
   Sort ();

   
   // release
   gpData->NodeRelease(pNode);
   return TRUE;
}


/*******************************************************************************
Sort - Sorts the Event list by date.
*/
int __cdecl EventSort(const void *elem1, const void *elem2 )
{
   HANGFUNCIN;
   PCEventDay t1, t2;
   t1 = *((PCEventDay*)elem1);
   t2 = *((PCEventDay*)elem2);

   // return
   if (t1->m_dwMonth != t2->m_dwMonth)
      return (int) t1->m_dwMonth - (int) t2->m_dwMonth;

   if (t1->m_dwDOM != t2->m_dwDOM)
      return (int) t1->m_dwDOM - (int) t2->m_dwDOM;

   // else
   return (int) t1->m_dwID - (int)t2->m_dwID;

}

void CEvent::Sort (void)
{
   qsort (m_listDays.Get(0), m_listDays.Num(), sizeof(PCEventDay), EventSort);
}


/***********************************************************************
EventInit - Makes sure the Event object is filled. If not,
it's loaded in. This CAN be called multiple times.

This fills in:
   gfEventPopulated - Set to TRUE when it's been loaded
   gCurEvent - fills in

inputs
   none
returns
   BOOL - error
*/
BOOL EventInit (void)
{
   HANGFUNCIN;
   if (gfEventPopulated)
      return TRUE;

   // else get it
   PCMMLNode   pNode;
   DWORD dwNum;
   pNode = FindMajorSection (gszEventListNode, &dwNum);
   if (!pNode)
      return NULL;   // error. This shouldn't happen
   gpData->Flush();
   gpData->NodeRelease(pNode);

   gfEventPopulated = TRUE;

   // pass this on
   return gCurEvent.Init (dwNum);
}


#if 0 // BUGBUG



/*****************************************************************************
EventDayEditPage - Override page callback.
*/
BOOL EventDayEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PCEventDay pDay = (PCEventDay) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"IFREOCCUR")) {
            p->pszSubString = pDay->m_dwParent ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFREOCCUR")) {
            p->pszSubString = pDay->m_dwParent ? L"" : L"</comment>";
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
            pControl->AttribSet (gszText, (PWSTR) pDay->m_memName.p);
         pControl = pPage->ControlFind(gszDescription);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pDay->m_memDescription.p);


         // other data
         WCHAR szTemp[128];
         pControl = pPage->ControlFind (gszPriority);
         if (pControl)
            pControl->AttribSetInt (gszCurSel, (int) pDay->m_dwPriority - 1);
         DateControlSet (pPage, gszDate, pDay->m_dwDateShow);
         DateControlSet (pPage, gszCompleteBy, pDay->m_dwCompleteBy);
         pControl = pPage->ControlFind (gszDaysOfEvent);
         swprintf (szTemp, L"%g", (double)pDay->m_dwEstimated / 60.0);
         if (pControl)
            pControl->AttribSet (gszText, szTemp);

         JournalControlSet (pPage, pDay);
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
               pPage->MBWarning (L"You must type in a name for the Day.");
               return TRUE;
            }
            MemZero (&pDay->m_memName);
            MemCat (&pDay->m_memName, szName);

            // set values
            pControl = pPage->ControlFind(gszDescription);
            if (pControl)
               pControl->AttribGet (gszText, szDescription, sizeof(szDescription),&dwNeeded);
            MemZero (&pDay->m_memDescription);
            MemCat (&pDay->m_memDescription, szDescription);

            // other data
            double   fEvent;
            WCHAR szTemp[128];
            pControl = pPage->ControlFind (gszPriority);
            if (pControl)
               pDay->m_dwPriority = (DWORD)pControl->AttribGetInt (gszCurSel)+1;
            pDay->m_dwDateShow = DateControlGet (pPage, gszDate);
            pDay->m_dwCompleteBy = DateControlGet (pPage, gszCompleteBy);
            pControl = pPage->ControlFind (gszDaysOfEvent);
            fEvent = 1.0;
            szTemp[0]= 0 ;
            if (pControl)
               pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);
            AttribToDouble (szTemp, &fEvent);
            pDay->m_dwEstimated = (DWORD) (fEvent * 60);

            JournalControlGet (pPage, pDay);

            // set dirty
            pDay->SetDirty();
            gCurEvent.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszCompleted) || !_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            // BUGFIX - ask how long it took
            double   fHowLong = -1;
            if ((pDay->m_iJournalID != -1) && !_wcsicmp(p->pControl->m_pszName, gszCompleted)) {
               fHowLong = HowLong (pPage, pDay->m_dwEstimated / 60.0);
               if (fHowLong < 0)
                  return TRUE;   // pressed cancel
            }

            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Day removed.");

            // get the category so changes registered
            JournalControlGet (pPage, pDay);

            // find it
            DWORD dwIndex;
            dwIndex = gCurEvent.DayIndexByID (pDay->m_dwID);
            gCurEvent.m_listDays.Remove(dwIndex);

            // note in the daily log that resolved
            if (!_wcsicmp(p->pControl->m_pszName, gszCompleted)) {
               WCHAR szHuge[10000];
               wcscpy (szHuge, L"Day completed: ");
               wcscat (szHuge, (PWSTR)pDay->m_memName.p);
               if (pDay->m_memDescription.p && ((PWSTR)pDay->m_memDescription.p)[0]) {
                  wcscat (szHuge, L"\r\n");
                  wcscat (szHuge, (PWSTR)pDay->m_memDescription.p);
               }
               DFDATE date;
               DFTIME start, end;
               CalendarLogAdd (date = Today(), start = Now(), end = -1, szHuge);

               // log the Day completed
               if (pDay->m_iJournalID != -1)
                  JournalLink ((DWORD)pDay->m_iJournalID, szHuge, (DWORD)-1, date, start, end, fHowLong);
            }


            // delete it
            delete pDay;
            gCurEvent.m_fDirty = TRUE;
            gCurEvent.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}





#endif // 0




/*************************************************************************
EventEnumMonth - Enumerate all the event occurring in the month, filling them
into paMem.
inputs
   DWORD    dwMonth - 1..12
   DWORD    dwYear - year
   PCMem    paMem - Pointer to 32 entries. 0 is ignored. 1..31 are used for the day
reutrns
   none
*/
void EventEnumMonth (DWORD dwMonth, DWORD dwYear, PCMem paMem)
{
   HANGFUNCIN;
   if (!EventInit())
      return;

   // loop through them all
   DWORD i;
   for (i = 0; i < gCurEvent.m_listDays.Num(); i++) {
      PCEventDay pDay = gCurEvent.DayGetByIndex(i);
      if (!pDay)
         continue;

      // see when it occurs on the year
      DFDATE d;
      d = 0;
      switch (pDay->m_dwRule) {
      case ERULE_DOM:
         if (pDay->m_dwMonth != dwMonth)
            continue;
         d = TODFDATE (pDay->m_dwDOM, pDay->m_dwMonth, dwYear);
         break;
      case ERULE_AFTER:
         {
            d = TODFDATE (pDay->m_dwDOM, pDay->m_dwMonth, dwYear);

            // get the number of days and day of week for 1st
            DWORD dwNum, dow;
            dwNum = (DWORD) EscDaysInMonth ((int)pDay->m_dwMonth, (int) dwYear, (int*) &dow);

            // up to the current date
            dow = (dow + pDay->m_dwDOM - 1) % 7;

            // loop, keeping a counter
            DWORD dwFound = 0;
            for (dwFound=0; dwFound <= pDay->m_dwOrdinal; dow = (dow+1)%7, d = MinutesToDFDATE(DFDATEToMinutes(d) + 60*24)) {
               BOOL dwMatch = FALSE;
               switch (pDay->m_dwWeekday) {
               case 0:  // day of week
                  dwMatch = TRUE;   // always match
                  break;
               case 1:  // weekday
                  dwMatch = (dow != 0) && (dow != 6);
                  break;
               case 2:  // weekend day
                  dwMatch = (dow == 0) || (dow == 6);
                  break;
               case 3:  // Sunday
                  dwMatch = (dow == 0);
                  break;
               case 4:  // Monday
                  dwMatch = (dow == 1);
                  break;
               case 5:  // Tuesday
                  dwMatch = (dow == 2);
                  break;
               case 6:  // weds
                  dwMatch = (dow == 3);
                  break;
               case 7:  // thurs
                  dwMatch = (dow == 4);
                  break;
               case 8:  // fri
                  dwMatch = (dow == 5);
                  break;
               case 9:  // sat
                  dwMatch = (dow == 6);
                  break;
               }

               // if not match continue
               if (!dwMatch)
                  continue;
               
               // found a match
               dwFound++;

               if (dwFound > pDay->m_dwOrdinal)
                  break;
            }

         }
         break;
      case ERULE_WEEKDAY:
         if (pDay->m_dwMonth != dwMonth)
            continue;
         d = TODFDATE(ReoccurFindNth (pDay->m_dwMonth, dwYear, pDay->m_dwOrdinal, pDay->m_dwWeekday), pDay->m_dwMonth, dwYear);
         break;
      }

      if ((MONTHFROMDFDATE(d) == dwMonth) && (YEARFROMDFDATE(d) == dwYear))
         Holiday (paMem, DAYFROMDFDATE(d), (PWSTR) pDay->m_memName.p);

   }
}

/***************************************************************************
EventBirthday - Called by People.cpp to handle reoccurring birthdays.

inputs
   DWORD       dwNode - Node for the person. Or with 0x80000000 to get an ID for the bday.
   PWSTR       pszFirst - Person's first name
   PWSTR       pszLast - Person's last name
   DFDATE      bday - Person's birthday.
   BOOL        fEnable - If TRUE then make sure birthday is there, FALSE then make sure it's gone
returns
   none
*/
void EventBirthday (DWORD dwNode, PWSTR pszFirst, PWSTR pszLast,
                   DFDATE bday, BOOL fEnable)
{
   HANGFUNCIN;
   // if the birthday == 0 then automatically set fenable to false
   if (bday == 0)
      fEnable = FALSE;

   // init
   EventInit();

   // figure out what the ID should be and see if can find it
   dwNode |= 0x80000000;
   DWORD dwIndex;
   PCEventDay pDay;
   dwIndex = gCurEvent.DayIndexByID (dwNode);

   // if it's not supposed to exist do that now
   if (!fEnable) {
      if (dwIndex == (DWORD)-1)
         return;  // doesn't exist. No problem

      // else, must delete
      pDay = gCurEvent.DayGetByIndex (dwIndex);
      if (pDay)
         delete pDay;
      gCurEvent.m_listDays.Remove(dwIndex);
      gCurEvent.m_fDirty = TRUE;
      gCurEvent.Flush();

      // done
      return;
   }

   // else it should exist

   // if it already exists then consider modifying
   if (dwIndex != (DWORD)-1)
      pDay = gCurEvent.DayGetByIndex (dwIndex);
   else {
      // create it
      pDay = gCurEvent.DayAdd (L"", L"", (DWORD)-1);
      if (!pDay)
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
   wcscat (szName + wcslen(szName), L"'s birthday");

   // only if it changes
   if ((pDay->m_dwMonth != MONTHFROMDFDATE (bday)) ||
      (pDay->m_dwDOM != DAYFROMDFDATE (bday)) ||
      (pDay->m_dwRule != ERULE_DOM) ) {

      MemZero (&pDay->m_memName);
      MemCat (&pDay->m_memName, szName);
      pDay->m_dwID = dwNode;
      pDay->m_dwRule = ERULE_DOM;
      pDay->m_dwMonth = MONTHFROMDFDATE (bday);
      pDay->m_dwDOM = DAYFROMDFDATE (bday);
   }

   pDay->SetDirty();
   gCurEvent.Flush();

   // done
}


/*****************************************************************************
EventSummary - Fill in the substitution for the Day list.

inputs
retursn
   PWSTR - string from gMemTemp

BUGFIX - Put in start and fShowEmpty
*/
PWSTR EventSummary (void)
{
   HANGFUNCIN;
   // first of all, amke sure there are Days to show
   EventInit();
   gCurEvent.Sort();  // make sure they're sorted
   DWORD i;
   PCEventDay pDay;
   if (!gCurEvent.m_listDays.Num())
      return L""; // nothing

   // else, we have Days. Show them off
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<xlisttable innerlines=0>");

   DWORD dwLastMonth;
   BOOL fFound;
   fFound = FALSE;
   dwLastMonth = 0;
   for (i = 0; i < gCurEvent.m_listDays.Num(); i++) {
      pDay = gCurEvent.DayGetByIndex(i);

      // if different month then show
      DWORD dwMonth;
      dwMonth = pDay->m_dwMonth;
      if (dwMonth != dwLastMonth) {
         if (dwLastMonth)
            MemCat (&gMemTemp, L"</xtrmonth>");
         dwLastMonth = dwMonth;

         MemCat (&gMemTemp, L"<xtrmonth><tr><td align=right><big>");
         if (gfFullColor)
            MemCat (&gMemTemp, L"<colorblend posn=background tcolor=#20a020 bcolor=#60d060/>");
         MemCat (&gMemTemp, L"<bold>");
         MemCat (&gMemTemp, gpszMonth[dwMonth-1]);
         MemCat (&gMemTemp, L"</bold>");

         MemCat (&gMemTemp, L"</big></td></tr>");
      }


      // display Day info
      MemCat (&gMemTemp, L"<tr><xtdTask href=sd:");
      MemCat (&gMemTemp, (int) pDay->m_dwID);
      MemCat (&gMemTemp, L">");
      // BUGFIX - Crash unless as sanitized
      MemCatSanitize (&gMemTemp, (PWSTR) pDay->m_memName.p);
      
      // completed
      MemCat (&gMemTemp, L"</xtdTask>");

      // Blank area
      MemCat (&gMemTemp, L"<xtdcompleted>");
      MemCat (&gMemTemp, L"</xtdcompleted>");

      // When it occurs
      MemCat (&gMemTemp, L"<xtdcompleted>");
      if ((pDay->m_dwRule == ERULE_WEEKDAY) || (pDay->m_dwRule == ERULE_AFTER)) {
         MemCat (&gMemTemp, L"The ");
         PWSTR pszOrder[5] = {L"first", L"second", L"third", L"fourth", L"last"};
         MemCat (&gMemTemp, pszOrder[pDay->m_dwOrdinal]);

         MemCat (&gMemTemp, L" ");
         PWSTR pszWeekday[10] = {L"day of the week", L"weekday", L"week-end day",
            L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday",
            L"Friday", L"Saturday"};
         MemCat (&gMemTemp, pszWeekday[pDay->m_dwWeekday]);

         MemCat (&gMemTemp, (pDay->m_dwRule == ERULE_WEEKDAY) ?
            L" of " : L" on/after ");
      }
      MemCat (&gMemTemp, gpszMonth[pDay->m_dwMonth-1]);
      if ((pDay->m_dwRule == ERULE_DOM) || (pDay->m_dwRule == ERULE_AFTER)) {
         MemCat (&gMemTemp, L" ");
         MemCat (&gMemTemp, pDay->m_dwDOM);
      }
      MemCat (&gMemTemp, L"</xtdcompleted></tr>");
   }

   // finish off list
   if (dwLastMonth)
      MemCat (&gMemTemp, L"</xtrmonth>");


   MemCat (&gMemTemp, L"</xlisttable>");
   return (PWSTR) gMemTemp.p;
}

/*****************************************************************************
EventDayAddPage - Override page callback.
*/
BOOL EventDayAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         // set controls
         PCEventDay pDay;
         pDay = (PCEventDay) pPage->m_pUserData;
         if (!pDay)
            break;

         // pressed add. Do some verification
         PCEscControl pControl;

         // name
         pControl = pPage->ControlFind(gszName);
         if (pControl)
            pControl->AttribSet (gszText, (PWSTR) pDay->m_memName.p);


         // find out which is checked
         pControl = pPage->ControlFind (gszDOM);
         if (pControl)
            pControl->AttribSetBOOL(gszChecked, pDay->m_dwRule == ERULE_DOM);
         pControl = pPage->ControlFind (gszWeekday);
         if (pControl)
            pControl->AttribSetBOOL(gszChecked, pDay->m_dwRule == ERULE_WEEKDAY);
         pControl = pPage->ControlFind (gszAfter);
         if (pControl)
            pControl->AttribSetBOOL(gszChecked, pDay->m_dwRule == ERULE_AFTER);

         pControl = pPage->ControlFind (gszDay1);
         if (pControl)
            pControl->AttribSetInt (gszText,pDay->m_dwDOM);
         pControl = pPage->ControlFind (gszDay3);
         if (pControl)
            pControl->AttribSetInt (gszText,pDay->m_dwDOM);

         pControl = pPage->ControlFind (gszMonth1);
         if (pControl)
            pControl->AttribSetInt(gszCurSel, pDay->m_dwMonth-1);
         pControl = pPage->ControlFind (gszMonth2);
         if (pControl)
            pControl->AttribSetInt(gszCurSel, pDay->m_dwMonth-1);
         pControl = pPage->ControlFind (gszMonth3);
         if (pControl)
            pControl->AttribSetInt(gszCurSel, pDay->m_dwMonth-1);

         pControl = pPage->ControlFind (gszOrdinal2);
         if (pControl)
            pControl->AttribSetInt(gszCurSel, pDay->m_dwOrdinal);
         pControl = pPage->ControlFind (gszOrdinal3);
         if (pControl)
            pControl->AttribSetInt(gszCurSel, pDay->m_dwOrdinal);

         pControl = pPage->ControlFind (gszWeekday2);
         if (pControl)
            pControl->AttribSetInt(gszCurSel, pDay->m_dwWeekday-1);
         pControl = pPage->ControlFind (gszWeekday3);
         if (pControl)
            pControl->AttribSetInt(gszCurSel, pDay->m_dwWeekday-1);

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
            PCEscControl pControl;
            DWORD dwNeeded;
            szName[0] = 0;

            // name
            pControl = pPage->ControlFind(gszName);
            if (pControl)
               pControl->AttribGet (gszText, szName, sizeof(szName),&dwNeeded);
            if (!szName[0]) {
               pPage->MBWarning (L"You must type in a name for the day.");
               return TRUE;
            }


            // add it
            PCEventDay pDay;
            pDay = (PCEventDay) pPage->m_pUserData;
            if (!pDay)
               pDay = gCurEvent.DayAdd (szName, NULL, (DWORD)-1);
            if (!pDay) {
               pPage->MBError (gszWriteError);
               return FALSE;
            }

            // set the name
            MemZero (&pDay->m_memName);
            MemCat (&pDay->m_memName, szName);

            // find out which is checked
            pControl = pPage->ControlFind (gszDOM);
            if (pControl && pControl->AttribGetBOOL(gszChecked)) {
               pDay->m_dwRule = ERULE_DOM;

               pControl = pPage->ControlFind (gszDay1);
               if (pControl)
                  pDay->m_dwDOM = pControl->AttribGetInt (gszText);
               pDay->m_dwDOM = max(1,pDay->m_dwDOM);
               pDay->m_dwDOM = min(31,pDay->m_dwDOM);

               pControl = pPage->ControlFind (gszMonth1);
               if (pControl)
                  pDay->m_dwMonth = pControl->AttribGetInt(gszCurSel)+1;
            }

            pControl = pPage->ControlFind (gszWeekday);
            if (pControl && pControl->AttribGetBOOL(gszChecked)) {
               pDay->m_dwRule = ERULE_WEEKDAY;

               pControl = pPage->ControlFind (gszOrdinal2);
               if (pControl)
                  pDay->m_dwOrdinal = pControl->AttribGetInt (gszCurSel);

               pControl = pPage->ControlFind (gszWeekday2);
               if (pControl)
                  pDay->m_dwWeekday = pControl->AttribGetInt (gszCurSel)+1;

               pControl = pPage->ControlFind (gszMonth2);
               if (pControl)
                  pDay->m_dwMonth = pControl->AttribGetInt(gszCurSel)+1;
            }

            pControl = pPage->ControlFind (gszAfter);
            if (pControl && pControl->AttribGetBOOL(gszChecked)) {
               pDay->m_dwRule = ERULE_AFTER;

               pControl = pPage->ControlFind (gszOrdinal3);
               if (pControl)
                  pDay->m_dwOrdinal = pControl->AttribGetInt (gszCurSel);

               pControl = pPage->ControlFind (gszWeekday3);
               if (pControl)
                  pDay->m_dwWeekday = pControl->AttribGetInt (gszCurSel)+1;

               pControl = pPage->ControlFind (gszMonth3);
               if (pControl)
                  pDay->m_dwMonth = pControl->AttribGetInt(gszCurSel)+1;

               pControl = pPage->ControlFind (gszDay3);
               if (pControl)
                  pDay->m_dwDOM = pControl->AttribGetInt (gszText);
               pDay->m_dwDOM = max(1,pDay->m_dwDOM);
               pDay->m_dwDOM = min(31,pDay->m_dwDOM);
            }

            // exit this
            pDay->SetDirty();
            gCurEvent.Flush();
            pPage->Exit (gszOK);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, gszDelete)) {
            PCEventDay pDay;
            pDay = (PCEventDay) pPage->m_pUserData;
            if (!pDay)
               break;

            // tell user
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Day removed.");

            // find it
            DWORD dwIndex;
            dwIndex = gCurEvent.DayIndexByID (pDay->m_dwID);
            gCurEvent.m_listDays.Remove(dwIndex);

            // delete it
            delete pDay;
            gCurEvent.m_fDirty = TRUE;
            gCurEvent.Flush();
            pPage->Exit (gszOK);

            return TRUE;
         };
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
EventDayShowAddUI - Shows the add user interface.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   BOOL - TRUE if a project Day was added
*/
BOOL EventDayShowAddUI (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;

   // make sure it's all initialized
   EventInit ();

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   //cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLEVENTADD, EventDayAddPage);

   return pszRet && !_wcsicmp(pszRet, gszOK);
}

/*****************************************************************************
EventParseLink - If a page uses a Event substitution, which generates
"tt:XXXX" to indicate that a Event link was called, call this. If it's
not a Event link, returns FALSE. If it is, it deletes the Event and
returns TRUE. The page should then refresh itself.

inputs
   PWSTR    pszLink - link
   PCEscPage pPage - page
   BOOL     *pfRefresh - set to TRUE if should refresh
returns
   BOOL
*/
BOOL EventParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh)
{
   HANGFUNCIN;
   // make sure its the right type
   if ((pszLink[0] != L's') || (pszLink[1] != L'd') || (pszLink[2] != L':'))
      return FALSE;

   // make sure have Event loaded
   EventInit ();

   // get the Day
   PCEventDay pDay;
   DWORD dwID;
   dwID = 0;
   AttribToDecimal (pszLink + 3, (int*) &dwID);
   pDay = gCurEvent.DayGetByID (dwID);

   // pull up UI
   CEscWindow  cWindow;
   RECT  r;

   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   //cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLEVENTEDIT, EventDayAddPage, pDay);

   *pfRefresh = (pszRet && !_wcsicmp(pszRet, gszOK));
   return TRUE;
}
   
/*****************************************************************************
EventDaysPage - Override page callback.
*/
BOOL EventDaysPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // make sure initalized
      EventInit ();
      break;   // fall through

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only care about projectlist
         if (!_wcsicmp(p->pszSubName, L"EVENTS")) {
            p->pszSubString = EventSummary ();
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

         if (!_wcsicmp(p->pControl->m_pszName, L"addevent")) {
            if (EventDayShowAddUI(pPage))
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
         if (EventParseLink (p->psz, pPage, &fRefresh)) {
            if (fRefresh)
               pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


