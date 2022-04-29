/**********************************************************************
Util.cpp - Utility functions.

begun 6/7/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"

/* globals */
static CBTree  gtreeMajorSection;   // used by FindMajor section. points to DWORD
CMem     gMemTemp;         // temporary memory for whatever

static WCHAR gszDay[] = L"day";
static WCHAR gszMonth[] = L"month";
static WCHAR gszYear[] = L"year";
static WCHAR gszHour[] = L"hour";
static WCHAR gszMinute[] = L"minute";
static WCHAR gszExtra[] = L"extra";
static WCHAR gszPlanTaskNumElem[] = L"PlanTaskNumElem";
static WCHAR gszPlanTaskTimeCompleted[] = L"PlanTaskTimeCompleted";
static WCHAR gszPlanTaskElem[] = L"PlanTaskElem";

PWSTR gaszMonth[12] = {
   L"January",
   L"February",
   L"March",
   L"April",
   L"May",
   L"June",
   L"July",
   L"August",
   L"September",
   L"October",
   L"November",
   L"December"
};

static PWSTR gaszMonthAbbrev[12] = {
   L"Jan",
   L"Feb",
   L"Mar",
   L"Apr",
   L"May",
   L"Jun",
   L"Jul",
   L"Aug",
   L"Sep",
   L"Oct",
   L"Nov",
   L"Dec"
};

static PWSTR gaszDayOfWeek[7] = {
   L"Sunday",
   L"Monday",
   L"Tuesday",
   L"Wednesday",
   L"Thursday",
   L"Friday",
   L"Saturday"
};

/**********************************************************************
NodeValueSet - Creates a sub node of name pszName and fills its
contents with pszValue. If the node already exists the old one is
deleted.

inputs
   PCMMLNode pNode - To create the subnode in
   PWSTR    pszName - Name for the subnode
   PWSTR    pszValue - Value for it
   int      iNumber - If non-zero, this sets the "number=" attribute to this
returns
   BOOL - TRUE if OK
*/
BOOL NodeValueSet (PCMMLNode pNode, PWSTR pszName, PWSTR pszValue, int iNumber)
{
   //HANGFUNCIN;
   // figure out number
   WCHAR szTemp[16];
   _itow (iNumber, szTemp, 10);

   // see if it exists
   DWORD dwFind;
   dwFind = pNode->ContentFind (pszName);
   if (dwFind < pNode->ContentNum()) {
      // found it
      PCMMLNode pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         return FALSE;  // error
      
      // delete all the sub's contents
      while (pSub->ContentNum())
         pSub->ContentRemove(0);

      // set the attribute
      if (iNumber)
         pSub->AttribSet (gszNumber, szTemp);

      // add it in
      return pSub->ContentAdd (pszValue);
   }

   // else not hree so just create
   PCMMLNode   pSub;
   pSub = pNode->ContentAddNewNode ();
   if (!pSub)
      return FALSE;
   pSub->NameSet (pszName);

   // set the attribute
   if (iNumber)
      pSub->AttribSet (gszNumber, szTemp);

   return pSub->ContentAdd (pszValue);
}

/**********************************************************************
NodeValueSet - Creates a sub node of name pszName and fills its
contents with pszValue. If the node already exists the old one is
deleted.

inputs
   PCMMLNode pNode - To create the subnode in
   PWSTR    pszName - Name for the subnode
   DWORD    dwValue - Value for it
returns
   BOOL - TRUE if OK
*/
BOOL NodeValueSet (PCMMLNode pNode, PWSTR pszName, int dwValue)
{
   //HANGFUNCIN;
   WCHAR szTemp[32];
   swprintf (szTemp, gszPercentD, dwValue);
   return NodeValueSet (pNode, pszName, szTemp);
}

/**********************************************************************
NodeValueGet - Finds the first subnode in pNode with the given pszName.
   It then finds the contents, and searches the 0th element. If that's
   a string it returns a pointer to the string.

inputs
   PCMMLNode pNode - To look in
   PWSTR    pszName - Name for the subnode
   int      *piNumber - filled with the number
returns
   PWSTR - value, or NULL if can't find. This is valid untilthe node is deleted
*/
PWSTR NodeValueGet (PCMMLNode pNode, PWSTR pszName, int*piNumber)
{
   // HANGFUNCIN;
   // see if it exists
   DWORD dwFind;
   dwFind = pNode->ContentFind (pszName);
   if (dwFind >= pNode->ContentNum())
      return NULL;

   // found it
   PCMMLNode pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (dwFind, &psz, &pSub);
   if (!pSub)
      return NULL;  // error
     
   // get the string
   PCMMLNode   pSub2;
   psz = NULL;
   pSub->ContentEnum(0, &psz, &pSub2);

   // if looking for a number get that
   if (piNumber) {
      *piNumber = -1;
      AttribToDecimal (pSub->AttribGet(gszNumber), piNumber);
   }

   return psz;
}

/**********************************************************************
NodeValueGet - Finds the first subnode in pNode with the given pszName.
   It then finds the contents, and searches the 0th element. If that's
   a string it returns a pointer to the string.

inputs
   PCMMLNode pNode - To look in
   PWSTR    pszName - Name for the subnode
   int      iDefault - default value
returns
   int - value, or iDefault if cant find
*/
int NodeValueGetInt (PCMMLNode pNode, PWSTR pszName, int iDefault)
{
   // HANGFUNCIN;
   PWSTR psz;
   psz = NodeValueGet (pNode, pszName);
   if (!psz)
      return iDefault;

   return _wtoi (psz);
}


/**********************************************************************
FindMajorSection - Looks in the root node (node 0) for a section
of the given name. If it finds an entry, it then caches that node
in and returns it. Otherwise, it creates a new one and returns it.
(This first checks in a tree to get the right number, just to save
file access.)

inputs
   PWSTR    pszSection - Section name like, "PeopleSection"
   DWORD    *pdwID - filled with the node ID number
returns
   PCMMLNode - Node for the people section. This must be released
      from the database object
*/
PCMMLNode FindMajorSection (PWSTR pszSection, DWORD *pdwID)
{
   HANGFUNCIN;
   // first, get the node number
   DWORD dwNum;

   if (pdwID)
      *pdwID = NULL;

   // see if it's in the tree
   DWORD *pdw;
   pdw = (DWORD*) gtreeMajorSection.Find(pszSection);
   if (pdw) {
      dwNum = *pdw;
   }
   else {
      // get thr root node
      PCMMLNode pRoot;
      pRoot = gpData->NodeGet(0);
      if (!pRoot)
         return NULL;   // shouldn't happen

      // find the section name
      PCMMLNode   pFound;
      DWORD dwIndex;
      dwIndex = pRoot->ContentFind (pszSection);
      pFound = NULL;
      PWSTR psz;
      pRoot->ContentEnum (dwIndex, &psz, &pFound);
      dwNum = (DWORD) -1;
      if (pFound) // get the number
         AttribToDecimal (pFound->AttribGet (gszNumber), (int*) &dwNum);

      // if no number then create
      if (dwNum == (DWORD) -1) {
         PCMMLNode   pNew;
         pNew = gpData->NodeAdd (pszSection, &dwNum);
         if (!pNew) {
            gpData->NodeRelease (pRoot);
            return NULL;
         }

         // just created the new one, so add pointers to it
         PCMMLNode   pSub;
         pSub = pRoot->ContentAddNewNode ();
         if (pSub) {
            WCHAR szTemp[16];
            _itow (dwNum, szTemp, 10);
            pSub->NameSet (pszSection);
            pSub->AttribSet (gszNumber, szTemp);
         }

         // and remember it in the tree
         gtreeMajorSection.Add (pszSection, &dwNum, sizeof(dwNum));

         // flush everything
         gpData->Flush();

         // and release
         gpData->NodeRelease (pNew);
      }

      // release the main node
      gpData->NodeRelease (pRoot);
   }

   // at this point we know the number
   if (pdwID)
      *pdwID = dwNum;
   return gpData->NodeGet(dwNum);
}


/**********************************************************************
FillFilteredList - This fills the in-memory list used for a filtered
list from a major node

inputs
   PCMMLNode      pNode- Node from FindMajorSection
   PWSTR          pszKeyword - Keyword string. The entries are written
                  in the node as <pszKeyword number=XXX>YYY</pszKeyword>
   PCListVariable pNames - Where the string section of the list goes
   PCListFixed    pLoc - DWORD locations for each string within the CDatabase
returns
   BOOL - TRUE if succede
*/
BOOL FillFilteredList (PCMMLNode pNode, PWSTR pszKeyword,
                       PCListVariable pNames, PCListFixed pLoc)
{
   HANGFUNCIN;
   // make sure names & loc are cleared and initialized
   pNames->Clear();
   pLoc->Init (sizeof(DWORD));
   pLoc->Clear();

   // look through the contents for the appropriate keyword
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;   // dont care about this

      // else, name check
      if (_wcsicmp(pszKeyword, pSub->NameGet()))
         continue;   // wrong name

      // else found

      // get the main string and the number
      PWSTR pszNum;
      pszNum = pSub->AttribGet(gszNumber);
      if (!pszNum)
         continue;   // no good
      DWORD dwNum;
      dwNum = _wtoi (pszNum);

      // string
      PCMMLNode   pSub2;
      psz = NULL;
      pSub->ContentEnum (0, &psz, &pSub2);
      if (!psz)
         continue;   // no good

      // add
      pNames->Add (psz, (wcslen(psz)+1)*2);
      pLoc->Add (&dwNum);
   }

   // all done
   return TRUE;
}


/**********************************************************************
AddFilteredList - This adds an element to the in-memory filtered list
and also updates the major node
inputs
   PWSTR          pszAddName - Name to add
   DWORD          dwAddLoc - Location to add
   PCMMLNode      pNode- Node from FindMajorSection
   PWSTR          pszKeyword - Keyword string. The entries are written
                  in the node as <pszKeyword number=XXX>YYY</pszKeyword>
   PCListVariable pNames - Where the string section of the list goes
   PCListFixed    pLoc - DWORD locations for each string within the CDatabase
returns
   BOOL - TRUE if succede
*/
BOOL AddFilteredList (PWSTR pszAddName, DWORD dwAddLoc,
                      PCMMLNode pNode, PWSTR pszKeyword,
                      PCListVariable pNames, PCListFixed pLoc)
{
   HANGFUNCIN;
   // first, add it to the node
   PCMMLNode   pSub;
   pSub = pNode->ContentAddNewNode ();
   if (!pSub)
      return FALSE;
   pSub->NameSet (pszKeyword);
   WCHAR szTemp[16];
   _itow (dwAddLoc, szTemp, 10);
   pSub->AttribSet (gszNumber, szTemp);
   pSub->ContentAdd (pszAddName);

   // then add it to the list
   pNames->Add (pszAddName, (wcslen(pszAddName)+1)*2);
   pLoc->Add (&dwAddLoc);

   return TRUE;
}


/**********************************************************************
RemoveFilteredList - This deletes an element to the in-memory filtered list
and also updates the major node
inputs
   DWORD          dwDelLoc - Location ID to delete
   PCMMLNode      pNode- Node from FindMajorSection
   PWSTR          pszKeyword - Keyword string. The entries are written
                  in the node as <pszKeyword number=XXX>YYY</pszKeyword>
   PCListVariable pNames - Where the string section of the list goes
   PCListFixed    pLoc - DWORD locations for each string within the CDatabase
returns
   BOOL - TRUE if succede
*/
BOOL RemoveFilteredList (DWORD dwDelLoc, PCMMLNode pNode, PWSTR pszKeyword,
                         PCListVariable pNames, PCListFixed pLoc)
{
   HANGFUNCIN;
   // find it in the nodes
   WCHAR szTemp[16];
   _itow (dwDelLoc, szTemp, 10);
   DWORD dwFound;
   dwFound = pNode->ContentFind (pszKeyword, gszNumber, szTemp);
   if (dwFound != (DWORD)-1)
      pNode->ContentRemove (dwFound);

   // then remove it from the list
   DWORD i;
   DWORD *pdw;
   for (i = 0; i < pLoc->Num(); i++) {
      pdw = (DWORD*) pLoc->Get(i);
      if (!pdw || (*pdw != dwDelLoc))
         continue;   // not a match

      // else found, so delte
      pLoc->Remove(i);
      pNames->Remove(i);
      return TRUE;
   }

   return FALSE;
}



/**********************************************************************
RenameFilteredList - This renames an element to the in-memory filtered list
and also updates the major node
inputs
   DWORD          dwDelLoc - Location ID to rename
   PWSTR          pszNewName - New name to use
   PCMMLNode      pNode- Node from FindMajorSection
   PWSTR          pszKeyword - Keyword string. The entries are written
                  in the node as <pszKeyword number=XXX>YYY</pszKeyword>
   PCListVariable pNames - Where the string section of the list goes
   PCListFixed    pLoc - DWORD locations for each string within the CDatabase
returns
   BOOL - TRUE if succede
*/
BOOL RenameFilteredList (DWORD dwDelLoc, PWSTR pszNewName,
                         PCMMLNode pNode, PWSTR pszKeyword,
                         PCListVariable pNames, PCListFixed pLoc)
{
   HANGFUNCIN;
   // find it in the nodes
   WCHAR szTemp[16];
   _itow (dwDelLoc, szTemp, 10);
   DWORD dwFound;
   dwFound = pNode->ContentFind (pszKeyword, gszNumber, szTemp);
   if (dwFound != (DWORD)-1) {
      PCMMLNode   pSub;
      PWSTR psz;
      pNode->ContentEnum (dwFound, &psz, &pSub);
      if (!pSub)
         return FALSE;

      // clear out
      while (pSub->ContentNum())
         pSub->ContentRemove(0);

      // add string back in
      pSub->ContentAdd (pszNewName);
   }

   // then change it in the list
   DWORD i;
   DWORD *pdw;
   for (i = 0; i < pLoc->Num(); i++) {
      pdw = (DWORD*) pLoc->Get(i);
      if (!pdw || (*pdw != dwDelLoc))
         continue;   // not a match

      // else found, so change
      pNames->Set (i, pszNewName, (wcslen(pszNewName)+1)*2);
      return TRUE;
   }

   return FALSE;
}

/***********************************************************************
FilteredIndexToDatabase - Given an index ID (into glistPeople), this
returns a DWORD CDatabase location for the person

inputs
   DWORD - dwIndex
   PCListVariable pNames - Where the string section of the list goes
   PCListFixed    pLoc - DWORD locations for each string within the CDatabase
returns
   DWORD - CDatabase location. -1 if cant find
*/
DWORD FilteredIndexToDatabase (DWORD dwIndex,
                               PCListVariable pNames, PCListFixed pLoc)
{
   HANGFUNCIN;
   DWORD *pdw;
   pdw = (DWORD*) pLoc->Get(dwIndex);
   if (pdw)
      return *pdw;
   else
      return (DWORD)-1;
}


/***********************************************************************
FilteredDatabaseToIndex - Given a CDatabase location for the person data
this returns an index into the glistPeople.

inputs
   DWORD - CDatabase location
   PCListVariable pNames - Where the string section of the list goes
   PCListFixed    pLoc - DWORD locations for each string within the CDatabase
returns
   DWORD - Index into clistPeople. -1 if cant find
*/
DWORD FilteredDatabaseToIndex (DWORD dwLoc,
                               PCListVariable pNames, PCListFixed pLoc)
{
   HANGFUNCIN;
   // then change it in the list
   DWORD i;
   DWORD *pdw;
   for (i = 0; i < pLoc->Num(); i++) {
      pdw = (DWORD*) pLoc->Get(i);
      if (!pdw || (*pdw != dwLoc))
         continue;   // not a match

      // else found, so change
      return i;
   }

   // not found
   return (DWORD)-1;
}


/***********************************************************************
FilteredStringToIndex - Given a name string in a variable list for
purposes of a filtered list, this searches through the list and finds
a case-insensative match.

inputs
   PWSTR          pszName - to look for
   PCListVariable pNames - Where the string section of the list goes
   PCListFixed    pLoc - DWORD locations for each string within the CDatabase
returns
   DWORD - Index into clistPeople. -1 if cant find
*/
DWORD FilteredStringToIndex (PWSTR pszName,
                               PCListVariable pNames, PCListFixed pLoc)
{
   HANGFUNCIN;
   // then change it in the list
   DWORD i;
   PWSTR psz;
   for (i = 0; i < pNames->Num(); i++) {
      psz = (PWSTR) pNames->Get(i);
      if (!psz || _wcsicmp(psz, pszName))
         continue;

      // else found, so change
      return i;
   }

   // not found
   return (DWORD)-1;
}



/***********************************************************************
MemCat - Concatenated a unicode string onto a CMem object. It also
adds null-termination, although the m_dwCurPosn is decreased by 2 so
then next MemCat() will overwrite the NULL.

inputs
   PCMem    pMem - memory object
   PWSTR    psz - string
*/
void MemCat (PCMem pMem, PWSTR psz)
{
   // HANGFUNCIN;
   pMem->StrCat (psz);
   DWORD dwTemp;
   dwTemp = pMem->m_dwCurPosn;
   pMem->CharCat(0);
   pMem->m_dwCurPosn = dwTemp;
}

/***********************************************************************
MemCat - Concatenated a number onto a CMem object. It also
adds null-termination, although the m_dwCurPosn is decreased by 2 so
then next MemCat() will overwrite the NULL.

inputs
   PCMem    pMem - memory object
   int      iNum - number
*/
void MemCat (PCMem pMem, int iNum)
{
   WCHAR szTemp[32];
   swprintf (szTemp, gszPercentD, iNum);
   MemCat (pMem, szTemp);
}

/***********************************************************************
MemZero - Zeros out memory
inputs
   PCMem    pMem - memory object
*/
void MemZero (PCMem pMem)
{
   pMem->m_dwCurPosn = 0;
   pMem->CharCat(0);
   pMem->m_dwCurPosn = 0;
}

/***********************************************************************
MemCatSanitize - Concatenates a memory string, but sanitizes it for MML
first.

inputs
   PCMem    pMem - memory object
   PWSTR    psz - Needs to be sanitized
*/
void MemCatSanitize (PCMem pMem, PWSTR psz)
{
   // assume santizied will be about the same size
   pMem->Required(pMem->m_dwCurPosn + (wcslen(psz)+1)*2);
   size_t dwNeeded;
   if (StringToMMLString(psz, (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn),
      pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded)) {

      pMem->m_dwCurPosn += wcslen((PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn))*2;
      return;  // worked
   }

   // else need larger
   pMem->Required(pMem->m_dwCurPosn + dwNeeded + 2);
   StringToMMLString(psz, (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn),
      pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded);
   pMem->m_dwCurPosn += wcslen((PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn))*2;
}


/***********************************************************************
DateControlSet - Given a DFDATE, this sets the date control.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
   DFDATE         date - date
returns
   BOOL - TRUE if succeded
*/
BOOL DateControlSet (PCEscPage pPage, PWSTR pszControl, DFDATE date)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return FALSE;

   pControl->AttribSetInt (gszDay, DAYFROMDFDATE(date));
   pControl->AttribSetInt (gszMonth, MONTHFROMDFDATE(date));
   pControl->AttribSetInt (gszYear, YEARFROMDFDATE(date));

   return TRUE;
}


/***********************************************************************
DateControlGet - Gets the date (DFDATE format) from the date control

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
returns
   DFDATE - date. 0 if none specified
*/
DFDATE DateControlGet (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return FALSE;

   int   iDay, iMonth, iYear;
   iDay = pControl->AttribGetInt (gszDay);
   iMonth = pControl->AttribGetInt (gszMonth);
   iYear = pControl->AttribGetInt (gszYear);

   // if any are 0 then return 0
   if (/*(iDay <= 0) ||*/ (iMonth <= 0) || (iYear <= 0))
      return 0;

   // else combin
   return TODFDATE(iDay, iMonth, iYear);
}


/***********************************************************************
TimeControlSet - Given a DFTIME, this sets the time control.

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
   DFTIME         time - time
returns
   BOOL - TRUE if succeded
*/
BOOL TimeControlSet (PCEscPage pPage, PWSTR pszControl, DFTIME time)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return FALSE;

   pControl->AttribSetInt (gszHour, HOURFROMDFTIME(time));
   pControl->AttribSetInt (gszMinute, MINUTEFROMDFTIME(time));

   return TRUE;
}


/***********************************************************************
TimeControlGet - Gets the time (DFTIME format) from the time control

inputs
   PCEscPage      pPage - page
   PWSTR          pszControl - control name
returns
   DFTIME - time. -1 if none specified
*/
DFTIME TimeControlGet (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return (DWORD)-1;

   int   iHour, iMinute;
   iHour = pControl->AttribGetInt (gszHour);
   iMinute = pControl->AttribGetInt (gszMinute);

   // if any are 0 then return 0
   if ((iHour < 0) || (iMinute < 0))
      return (DWORD)-1;

   // else combin
   return TODFTIME(iHour, iMinute);
}


/*******************************************************************
DFTIMEToString - Converts a DFTIME to a string.

inputs
   DFTIME      time - date. -1 if not specified
   PWSTR       psz - filled in. Should be about 32 chars long
returns
   none
*/
void DFTIMEToString (DFTIME time, PWSTR psz)
{
   DWORD dwHour, dwMinute;
   dwHour = HOURFROMDFTIME (time);
   dwMinute = MINUTEFROMDFTIME (time);

   if (dwHour == (DWORD) -1) {
      wcscpy (psz, L"No time");
      return;
   }

   BOOL  fAM;
   fAM = (dwHour < 12);
   dwHour = dwHour % 12;
   if (!dwHour)
      dwHour = 12;

   swprintf (psz, L"%d:%d%d %s", (int) dwHour, (int)(dwMinute / 10),
      (int)(dwMinute%10), fAM ? L"am" : L"pm");
}


/*******************************************************************
DFDATEToString - Converts a DFDATE to a string.
   Note: This only displays the year if it's different than the
   current year.

inputs
   DFDATE      date - date. 0 if not specified
   PWSTR       psz - filled in. Should be about 32 chars long
returns
   none
*/
void DFDATEToString (DFDATE date, PWSTR psz)
{
   if (!date) {
      wcscpy (psz, L"No date");
      return;
   }

   psz[0] = 0;

   // if no day then show month year
   if (!DAYFROMDFDATE(date)) {
      swprintf (psz + wcslen(psz), L"%s %d",
         gaszMonth[(MONTHFROMDFDATE(date) - 1)%12], YEARFROMDFDATE(date));
      return;
   }

   // show day of week
   if (TRUE) {
      int   iDays, iStart;
      iDays = EscDaysInMonth (MONTHFROMDFDATE(date), YEARFROMDFDATE(date), &iStart);

      int   idow;
      idow = (iStart + DAYFROMDFDATE(date) - 1) % 7;
      wcscpy(psz, gaszDayOfWeek[idow]);
      wcscat (psz, L", ");
   }

   // get the current year
   static WORD wYear = 0;
   if (!wYear) {
      SYSTEMTIME st;
      GetLocalTime (&st);
      wYear = st.wYear;
   }

   if (YEARFROMDFDATE(date) != (DWORD)wYear)
      swprintf (psz + wcslen(psz), L"%d %s %d",
         DAYFROMDFDATE(date), gaszMonth[(MONTHFROMDFDATE(date) - 1)%12], YEARFROMDFDATE(date));
   else
      swprintf (psz + wcslen(psz), L"%d %s",
         DAYFROMDFDATE(date), gaszMonth[(MONTHFROMDFDATE(date) - 1)%12]);
}

/*****************************************************************************
DFDATEToMinutes - Converts a DFDATE to the number of minutes since Jan1, 1601.

inputs
   DFDATE      date - date
returns
   __int64 - number of minutes
*/
__int64 DFDATEToMinutes (DFDATE date)
{
   SYSTEMTIME  st;
   FILETIME ft;
   __int64 *pi = (__int64*) &ft;

   memset (&st, 0, sizeof(st));
   st.wDay = (WORD) DAYFROMDFDATE(date);
   st.wMonth = (WORD) MONTHFROMDFDATE(date);
   st.wYear = (WORD) YEARFROMDFDATE(date);

   SystemTimeToFileTime (&st, &ft);

   *pi = *pi / ((__int64)10 * 1000000 * 60);

   return *pi;
}


/****************************************************************************
MinutesToDFDATE - Converts a minutes value (see DFDATEToMinutes) to a DFDATE value.

inputs
   __int64     iMinutes - minutes
returns
   DFDATE - date
*/
DFDATE MinutesToDFDATE (__int64 iMinutes)
{
   SYSTEMTIME  st;
   FILETIME ft;
   __int64 *pi = (__int64*) &ft;

   *pi = iMinutes * 10 * 1000000 * 60;

   FileTimeToSystemTime (&ft, &st);

   return TODFDATE ((DWORD)st.wDay, (DWORD)st.wMonth, (DWORD) st.wYear);
}


/****************************************************************************
DFDATEToDayOfWeek - Returns the day of the week (0..6).

inputs
   DFDATE      date - date to check up on
returns
   DWORRD - 0..6 for the day of the week, starting with sun
*/
DWORD DFDATEToDayOfWeek (DFDATE date)
{
   SYSTEMTIME  st;
   FILETIME ft;

   memset (&st, 0, sizeof(st));
   st.wDay = (WORD) DAYFROMDFDATE(date);
   st.wMonth = (WORD) MONTHFROMDFDATE(date);
   st.wYear = (WORD) YEARFROMDFDATE(date);

   SystemTimeToFileTime (&st, &ft);
   memset (&st, 0, sizeof(st));
   FileTimeToSystemTime (&ft, &st);

   return st.wDayOfWeek;
}


/*************************************************************************
Today - Returns today's date as DFDATE

*/
DFDATE Today (void)
{
   SYSTEMTIME  st;
   GetLocalTime (&st);
   return TODFDATE (st.wDay, st.wMonth, st.wYear);
}


/*************************************************************************
Now - Returns the curent time as DFTIME

*/
DFTIME Now (void)
{
   SYSTEMTIME  st;
   GetLocalTime (&st);
   return TODFTIME (st.wHour, st.wMinute);
}


/*****************************************************************************
DFTIMEToMinutes - Converts a DFTIME to the number of minutes since midnight

inputs
   DFTIME      time - date
returns
   int - number of minutes
*/
int DFTIMEToMinutes (DFTIME time)
{
   if (time == (DWORD)-1)
      return 0;

   return (int) ((DWORD)(time) >> 8) * 60 + (int) ((DWORD)(time) & 0xff);
}


/***********************************************************************
DialogRect - Fills in a rectangle used for dialog boxes.

inputs
   RECT*    pRect
*/
void DialogRect (RECT *pRect)
{
   int   ix, iy;
   ix = GetSystemMetrics (SM_CXSCREEN);
   iy = GetSystemMetrics (SM_CYSCREEN);

   pRect->top = iy / 8;
   pRect->bottom = iy - pRect->top;
   pRect->left = ix / 10;
   pRect->right = ix - pRect->left;
}


/***********************************************************************
ReoccurFromControls - Get reoccurance information from the controls
in reoccur.mml.

inputs
   PCEscPage      pPage - page
   PREOCCURANCE   pr - pointer to reoccur to fill in
returns
   none
*/
void ReoccurFromControls (PCEscPage pPage, PREOCCURANCE pr)
{
   HANGFUNCIN;
   PCEscControl pControl;
   // BUGFIX - Set all to 0
   pr->m_dwParam1 = pr->m_dwParam2 = pr->m_dwParam3 = 0;
   // keep pr->m_dwLastDate as it was. pr->m_dwLastDate = MinutesToDFDATE(DFDATEToMinutes(Today()) - 60*24);   // set it to yesterday

   pControl = pPage->ControlFind (gszEveryNDays);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_EVERYNDAYS;
      pControl = pPage->ControlFind (gszEveryNDaysEdit);
      if (pControl)
         pr->m_dwParam1 = (DWORD) pControl->AttribGetInt(gszText);
   };

   pControl = pPage->ControlFind (gszEveryWeekday);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_EVERYWEEKDAY;
   };

   // BUGFIX - get the date entered. As long as it's not immediately then set the last
   // date to the day before, just to ensure it gets set.
   pr->m_dwLastDate = DateControlGet (pPage, gszStartOn);
   if (pr->m_dwLastDate)
      pr->m_dwLastDate = MinutesToDFDATE(DFDATEToMinutes(pr->m_dwLastDate) - 24*60);

   pr->m_dwEndDate = DateControlGet (pPage, gszEndOn);
   //if (pr->m_dwEndDate)
   //   pr->m_dwEndDate = MinutesToDFDATE(DFDATEToMinutes(pr->m_dwEndDate) - 24*60);

   pControl = pPage->ControlFind (gszWeekly);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_WEEKLY;

      if ((pControl=pPage->ControlFind (gszWeeklySun)) && pControl->AttribGetBOOL(gszChecked))
         pr->m_dwParam1 |= 0x0001;
      if ((pControl=pPage->ControlFind (gszWeeklyMon)) && pControl->AttribGetBOOL(gszChecked))
         pr->m_dwParam1 |= 0x0002;
      if ((pControl=pPage->ControlFind (gszWeeklyTues)) && pControl->AttribGetBOOL(gszChecked))
         pr->m_dwParam1 |= 0x0004;
      if ((pControl=pPage->ControlFind (gszWeeklyWed)) && pControl->AttribGetBOOL(gszChecked))
         pr->m_dwParam1 |= 0x0008;
      if ((pControl=pPage->ControlFind (gszWeeklyThurs)) && pControl->AttribGetBOOL(gszChecked))
         pr->m_dwParam1 |= 0x0010;
      if ((pControl=pPage->ControlFind (gszWeeklyFri)) && pControl->AttribGetBOOL(gszChecked))
         pr->m_dwParam1 |= 0x0020;
      if ((pControl=pPage->ControlFind (gszWeeklySat)) && pControl->AttribGetBOOL(gszChecked))
         pr->m_dwParam1 |= 0x0040;
   };

   // BUGFIX - Allow every other thursday, etc.
   pControl = pPage->ControlFind (gszNWeekly);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_NWEEKLY;

      pControl = pPage->ControlFind (gszNWeeklyOther);
      if (pControl)
         pr->m_dwParam1 = (DWORD) pControl->AttribGetInt(gszCurSel)+2;

      pControl = pPage->ControlFind (gszNWeeklyDOW);
      if (pControl)
         pr->m_dwParam2 = (DWORD) pControl->AttribGetInt(gszCurSel);

   };

   pControl = pPage->ControlFind (gszMonthDay);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_MONTHDAY;
      pControl = pPage->ControlFind (gszMonthDayEdit);
      if (pControl)
         pr->m_dwParam1 = (DWORD) pControl->AttribGetInt(gszText);
   };

   pControl = pPage->ControlFind (gszMonthRelative);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_MONTHRELATIVE;
      pControl = pPage->ControlFind (gszMonthRelativeWeek);
      if (pControl)
         pr->m_dwParam1 = (DWORD) pControl->AttribGetInt(gszCurSel);
      pControl = pPage->ControlFind (gszMonthRelativeOf);
      if (pControl)
         pr->m_dwParam2 = (DWORD) pControl->AttribGetInt(gszCurSel);
   };

   pControl = pPage->ControlFind (gszYearDay);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_YEARDAY;
      pControl = pPage->ControlFind (gszYearMonth);
      if (pControl)
         pr->m_dwParam1 = (DWORD) pControl->AttribGetInt(gszCurSel) + 1;
      pControl = pPage->ControlFind (gszYearDayEdit);
      if (pControl)
         pr->m_dwParam2 = (DWORD) pControl->AttribGetInt(gszText);
   };

   pControl = pPage->ControlFind (gszYearRelative);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pr->m_dwPeriod = REOCCUR_YEARRELATIVE;
      pControl = pPage->ControlFind (gszYearRelativeWeek);
      if (pControl)
         pr->m_dwParam1 = (DWORD) pControl->AttribGetInt(gszCurSel);
      pControl = pPage->ControlFind (gszYearRelativeOf);
      if (pControl)
         pr->m_dwParam2 = (DWORD) pControl->AttribGetInt(gszCurSel);
      pControl = pPage->ControlFind (gszYearRelativeMonth);
      if (pControl)
         pr->m_dwParam3 = (DWORD) pControl->AttribGetInt(gszCurSel) + 1;
   };

}

/***********************************************************************
ReoccurToControls - Set reoccurance information in the controls
in reoccur.mml.

inputs
   PCEscPage      pPage - page
   PREOCCURANCE   pr - pointer to reoccur to fill in
returns
   none
*/
void ReoccurToControls (PCEscPage pPage, PREOCCURANCE pr)
{
   HANGFUNCIN;
   PCEscControl pControl;

   pControl = pPage->ControlFind (gszEveryNDays);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_EVERYNDAYS);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pControl = pPage->ControlFind (gszEveryNDaysEdit);
      if (pControl)
         pControl->AttribSetInt(gszText, (int) pr->m_dwParam1);
   };

   pControl = pPage->ControlFind (gszEveryWeekday);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_EVERYWEEKDAY);

   // BUGFIX - set the date entered. As long as it's not immediately then set the last
   // date to the day before, just to ensure it gets set.
   DFDATE t;
   t = pr->m_dwLastDate;
   if (t)
      t = MinutesToDFDATE(DFDATEToMinutes(t) + 24*60);
   DateControlSet (pPage, gszStartOn, t);

   // end on
   t = pr->m_dwEndDate;
   //if (t)
   //   t = MinutesToDFDATE(DFDATEToMinutes(t) + 24*60);
   DateControlSet (pPage, gszEndOn, t);

   pControl = pPage->ControlFind (gszWeekly);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_WEEKLY);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      if (pr->m_dwParam1 & 0x0001)
         if (pControl=pPage->ControlFind (gszWeeklySun))
            pControl->AttribSetBOOL(gszChecked, TRUE);
      if (pr->m_dwParam1 & 0x0002)
         if (pControl=pPage->ControlFind (gszWeeklyMon))
            pControl->AttribSetBOOL(gszChecked, TRUE);
      if (pr->m_dwParam1 & 0x0004)
         if (pControl=pPage->ControlFind (gszWeeklyTues))
            pControl->AttribSetBOOL(gszChecked, TRUE);
      if (pr->m_dwParam1 & 0x0008)
         if (pControl=pPage->ControlFind (gszWeeklyWed))
            pControl->AttribSetBOOL(gszChecked, TRUE);
      if (pr->m_dwParam1 & 0x0010)
         if (pControl=pPage->ControlFind (gszWeeklyThurs))
            pControl->AttribSetBOOL(gszChecked, TRUE);
      if (pr->m_dwParam1 & 0x0020)
         if (pControl=pPage->ControlFind (gszWeeklyFri))
            pControl->AttribSetBOOL(gszChecked, TRUE);
      if (pr->m_dwParam1 & 0x0040)
         if (pControl=pPage->ControlFind (gszWeeklySat))
            pControl->AttribSetBOOL(gszChecked, TRUE);
   };

   pControl = pPage->ControlFind (gszNWeekly);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_NWEEKLY);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pControl = pPage->ControlFind (gszNWeeklyOther);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam1-2);

      pControl = pPage->ControlFind (gszNWeeklyDOW);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam2);
   };

   pControl = pPage->ControlFind (gszMonthDay);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_MONTHDAY);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pControl = pPage->ControlFind (gszMonthDayEdit);
      if (pControl)
         pControl->AttribSetInt(gszText, (int) pr->m_dwParam1);
   };

   pControl = pPage->ControlFind (gszMonthRelative);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_MONTHRELATIVE);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pControl = pPage->ControlFind (gszMonthRelativeWeek);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam1);
      pControl = pPage->ControlFind (gszMonthRelativeOf);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam2);
   };

   pControl = pPage->ControlFind (gszYearDay);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_YEARDAY);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pControl = pPage->ControlFind (gszYearMonth);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam1-1);
      pControl = pPage->ControlFind (gszYearDayEdit);
      if (pControl)
         pControl->AttribSetInt(gszText, (int)pr->m_dwParam2);
   };

   pControl = pPage->ControlFind (gszYearRelative);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, pr->m_dwPeriod == REOCCUR_YEARRELATIVE);
   if (pControl && pControl->AttribGetBOOL (gszChecked)) {
      pControl = pPage->ControlFind (gszYearRelativeWeek);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam1);
      pControl = pPage->ControlFind (gszYearRelativeOf);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam2);
      pControl = pPage->ControlFind (gszYearRelativeMonth);
      if (pControl)
         pControl->AttribSetInt(gszCurSel, (int)pr->m_dwParam3-1);
   };

}

/**************************************************************************
ReoccurToString - Takes a reoccur structure (assuming dwPeriod != 0)
and write a human-readable string.

inputs
   PREOCCURANCE pr - reoccur
   PWSTR psz - string
returns
   none
*/
void ReoccurToString (PREOCCURANCE pr, PWSTR psz)
{
   HANGFUNCIN;
   PWSTR gaszRelativeWeek[] = {
      L"first",
      L"second",
      L"third",
      L"fourth",
      L"last"
   };

   PWSTR gaszRelativeOf[] = {
      L"day",
      L"weekday",
      L"weekend day",
      L"Sunday",
      L"Monday",
      L"Tuesday",
      L"Wednesday",
      L"Thursday",
      L"Friday",
      L"Saturday"
   };

   psz[0] = 0;

   switch (pr->m_dwPeriod) {
   case REOCCUR_EVERYNDAYS:
      swprintf (psz, L"Every %d days", (int) pr->m_dwParam1);
      break;

   case REOCCUR_EVERYWEEKDAY:
      swprintf (psz, L"Every weekday");
      break;

   case REOCCUR_WEEKLY:
      {
         swprintf (psz, L"Every ");
         DWORD i;
         BOOL fAlready = FALSE;
         for (i = 0; i < 7; i++) {
            // must have bit set
            if (!(pr->m_dwParam1 & (1 << i)))
               continue;

            // if already have something, then add ocmma
            if (fAlready)
               wcscat (psz, L", ");
            fAlready = TRUE;

            // day
            wcscat (psz, gaszDayOfWeek[i]);
         }
      }
      break;

   case REOCCUR_NWEEKLY:
      {
         // BUGFIX - Allow every other week
         swprintf (psz, L"Every ");
         switch (pr->m_dwParam1) {
         case 2:
            wcscat (psz, L"other");
            break;
         case 3:
            wcscat (psz, L"third");
            break;
         case 4:
            wcscat (psz, L"fourth");
            break;
         case 5:
            wcscat (psz, L"fifth");
            break;
         case 6:
            wcscat (psz, L"sixth");
            break;
         case 7:
            wcscat (psz, L"seventh");
            break;
         case 8:
            wcscat (psz, L"eigth");
            break;
         default:
            wcscat (psz, L"unknown");
            break;
         }

         wcscat (psz, L" ");

         // day
         wcscat (psz, gaszDayOfWeek[pr->m_dwParam2]);
      }
      break;

   case REOCCUR_MONTHDAY:
      swprintf (psz, L"Day %d of every month", (int) pr->m_dwParam1);
      break;

   case REOCCUR_MONTHRELATIVE:
      swprintf (psz, L"The %s %s of every month", gaszRelativeWeek[pr->m_dwParam1],
         gaszRelativeOf[pr->m_dwParam2]);
      break;

   case REOCCUR_YEARDAY:
      swprintf (psz, L"Every %s %d", gaszMonth[pr->m_dwParam1 - 1], (int) pr->m_dwParam2);
      break;

   case REOCCUR_YEARRELATIVE:
      swprintf (psz, L"The %s %s of every %s", gaszRelativeWeek[pr->m_dwParam1],
         gaszRelativeOf[pr->m_dwParam2], gaszMonth[pr->m_dwParam3 - 1]);
      break;


   }

}



/***********************************************************************
ReoccurFindNth - Given a month, find the Nth element of it.

inputs
   DWORD    dwMonth - month
   DWORD    dwYear - year
   DWORD    dwNth - 0=> first, 1=>second, 2=>third, 3=>fourth, 4=>last
   DWORD    dwElem - 0=>day of week, 1=>weekday, 2=>weekend day, 3=>Sunday, etc.
returns
   DWORD - day of the month
*/
DWORD ReoccurFindNth (DWORD dwMonth, DWORD dwYear, DWORD dwNth, DWORD dwElem)
{
   HANGFUNCIN;
   // get the number of days and day of week for 1st
   DWORD dwNum, dow;
   dwNum = (DWORD) EscDaysInMonth ((int)dwMonth, (int) dwYear, (int*) &dow);

   // loop, keeping a counter
   DWORD dwFound = 0;
   DWORD dwLast = 1; // day of last occrance found
   DWORD i;
   for (i = 1; i <= dwNum; i++, dow = (dow + 1)%7) {
      BOOL dwMatch = FALSE;
      switch (dwElem) {
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

      // else found
      switch (dwNth) {
      case 0:  // first
      case 1:  // second
      case 2:  // third
      case 3:  // fourth
         if (dwFound == dwNth)
            return i;   // found the match
         break;
      case 4:
         // do nothing because counting the last occurance
         break;
      }
      dwLast = i;
      dwFound++;
   }

   // if we get down to here, either there was an error or we
   // were looking for the last occurance. Either way, return
   // the last occurance
   return dwLast;
}

/***********************************************************************
ReoccurNext - Given a REOCCURANCE structure indicating that an event
was scheduled for a specific data already, this returns a DFDATE with
the next time the event will occur. NOTE: If the app makes it occur
it should set reoccuance.m_dwLastDate.

inputs
   PREOCCURANCE      pr- structure.
         if pr->m_dwLastDate == 0, assumes that this was just created.
retursn
   DFDATE - next date
*/
DFDATE ReoccurNext (PREOCCURANCE pr)
{
   HANGFUNCIN;
   // if there was no last date, then depending upon the perdio interval
   // come up with an answer
   if (!pr->m_dwLastDate) switch (pr->m_dwPeriod) {
   case REOCCUR_EVERYNDAYS:
      return Today();
      break;

   default:
   case REOCCUR_EVERYWEEKDAY:
   case REOCCUR_WEEKLY:
   case REOCCUR_NWEEKLY:
   case REOCCUR_MONTHDAY:
   case REOCCUR_MONTHRELATIVE:
   case REOCCUR_YEARDAY:
   case REOCCUR_YEARRELATIVE:
      // set it to yesterday
      pr->m_dwLastDate = MinutesToDFDATE(DFDATEToMinutes(Today()) - 60*24);   // set it to yesterday
      break;
   }

   // now do it
   switch (pr->m_dwPeriod) {
   case REOCCUR_EVERYNDAYS:
      {
         // make sure dont try to make an infinite #
         if (!pr->m_dwParam1)
            pr->m_dwParam1 = 1;

         return MinutesToDFDATE (DFDATEToMinutes(pr->m_dwLastDate) + 60*24*(__int64)pr->m_dwParam1);
      }
      break;

   case REOCCUR_EVERYWEEKDAY:
      // set dwParam1 to Mon-Fri and fall through
      pr->m_dwParam1 = 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020;
   case REOCCUR_WEEKLY:
      {
         // find out what the last day of the week was
         DWORD dow;
         dow = DFDATEToDayOfWeek (pr->m_dwLastDate);

         // loop for next 7 days finding a match
         DWORD i;
         for (i = 1, dow = (dow+1)%7; i <= 7; i++, dow = (dow + 1)%7) {
            if (pr->m_dwParam1 & (1 << dow)) // found match
               return MinutesToDFDATE (DFDATEToMinutes(pr->m_dwLastDate) + 60*24*(__int64)i);
         }
         // else, no match
         return 0;
      }
      break;

   case REOCCUR_NWEEKLY:
      {
         // BUGFIX - allow every second thursday, etc.

         // find out what the last day of the week was
         DWORD dow;
         dow = DFDATEToDayOfWeek (pr->m_dwLastDate);

         // if it's not a match for the day of the week then increase until
         // get that date and then accept. This should only happen if it's the
         // first time created
         DWORD dwInc;
         if (dow != pr->m_dwParam2) {
            dwInc = ((pr->m_dwParam2 < dow) ? (pr->m_dwParam2+7) : pr->m_dwParam2) - dow;
         }
         else {
            // else, go forward appropriate # of weeks
            dwInc = 7 * pr->m_dwParam1;
         }

         return MinutesToDFDATE (DFDATEToMinutes(pr->m_dwLastDate) + 60*24*(__int64)dwInc);
      }
      break;

   case REOCCUR_MONTHDAY:
      {
         DWORD dwDay, dwMonth, dwYear;
         dwDay = DAYFROMDFDATE(pr->m_dwLastDate);
         dwMonth = MONTHFROMDFDATE(pr->m_dwLastDate);
         dwYear = YEARFROMDFDATE(pr->m_dwLastDate);
         if (pr->m_dwParam1 < 1)
            pr->m_dwParam1 = 1;
         if (pr->m_dwParam1 > 31)
            pr->m_dwParam1 = 31;

         // if the last occurance was before the day of the month that it's
         // supposed to be then up the date
         if (dwDay < pr->m_dwParam1)
            return TODFDATE(pr->m_dwParam1, dwMonth, dwYear);

         // else, up the month
         dwDay = pr->m_dwParam1;
         dwMonth++;
         if (dwMonth > 12) {
            dwMonth = 1;
            dwYear++;
         }
         return TODFDATE (dwDay, dwMonth, dwYear);
      }
      break;

   case REOCCUR_MONTHRELATIVE:
      {
         DWORD dwDay, dwMonth, dwYear;
         dwDay = DAYFROMDFDATE(pr->m_dwLastDate);
         dwMonth = MONTHFROMDFDATE(pr->m_dwLastDate);
         dwYear = YEARFROMDFDATE(pr->m_dwLastDate);

         // see what date it's supposed to occur this month
         DWORD dwFound;
         dwFound = ReoccurFindNth (dwMonth, dwYear, pr->m_dwParam1, pr->m_dwParam2);

         // if it hasn't yet occured then use that
         if (dwFound > dwDay)
            return TODFDATE (dwFound, dwMonth, dwYear);

         // else, next month
         dwMonth++;
         if (dwMonth > 12) {
            dwMonth = 1;
            dwYear++;
         }
         dwFound = ReoccurFindNth (dwMonth, dwYear, pr->m_dwParam1, pr->m_dwParam2);
         return TODFDATE (dwFound, dwMonth, dwYear);
      }
      break;

   case REOCCUR_YEARDAY:
      {
         DWORD dwDay, dwMonth, dwYear;
         dwDay = DAYFROMDFDATE(pr->m_dwLastDate);
         dwMonth = MONTHFROMDFDATE(pr->m_dwLastDate);
         dwYear = YEARFROMDFDATE(pr->m_dwLastDate);
         if (pr->m_dwParam2 < 1)
            pr->m_dwParam2 = 1;
         if (pr->m_dwParam2 > 31)
            pr->m_dwParam2 = 31;
         if (pr->m_dwParam1 < 1)
            pr->m_dwParam1 = 1;
         if (pr->m_dwParam1 > 12)
            pr->m_dwParam1 = 12;


         // if the last occurance was before the day it should be this year
         // then use next year
         DWORD dwThisYear, dwNextYear;
         dwThisYear = TODFDATE (pr->m_dwParam2, pr->m_dwParam1, dwYear);
         dwNextYear = TODFDATE (pr->m_dwParam2, pr->m_dwParam1, dwYear+1);

         if (pr->m_dwLastDate < dwThisYear)
            return dwThisYear;
         else
            return dwNextYear;
      }
      break;

   case REOCCUR_YEARRELATIVE:
      {
         DWORD dwDay, dwMonth, dwYear;
         dwDay = DAYFROMDFDATE(pr->m_dwLastDate);
         dwMonth = MONTHFROMDFDATE(pr->m_dwLastDate);
         dwYear = YEARFROMDFDATE(pr->m_dwLastDate);
         if (pr->m_dwParam3 < 1)
            pr->m_dwParam3 = 1;
         if (pr->m_dwParam3 > 12)
            pr->m_dwParam3 = 12;

         // see what date it's supposed to occur this year
         DWORD dwFound, dwThisYear;
         dwFound = ReoccurFindNth (pr->m_dwParam3, dwYear, pr->m_dwParam1, pr->m_dwParam2);
         dwThisYear = TODFDATE (dwFound, pr->m_dwParam3, dwYear);
         if (dwThisYear > pr->m_dwLastDate)
            return dwThisYear;

         // else, it already occured so make it next year
         dwFound = ReoccurFindNth (pr->m_dwParam3, dwYear+1, pr->m_dwParam1, pr->m_dwParam2);
         return TODFDATE (dwFound, pr->m_dwParam3, dwYear+1);
      }
   }

   // shouldn get here
   return 0;
}


/**********************************************************************
NodeElemSet - Creates a sub node of name pszName and fills its
contents with pszValue. It also sets some attributes.

  THIS IS DIFFERENT THAN NodeDataSet because it allows multiple
  entries with the same name.

inputs
   PCMMLNode pNode - To create the subnode in
   PWSTR    pszName - Name for the subnode
   PWSTR    pszValue - Value for it
   int      iNumber - If non-zero, this sets the "number=" attribute to this
   BOOL     fRemove - If TRUE and another node with pszName and number=iNumber
                     is found then remove that first.
   DFDATE   date - Date info
   DFTIME   startTime - Start time
   DFTIME   endTime - End time
   int      iExtra - extra value
returns
   BOOL - TRUE if OK
*/
BOOL NodeElemSet (PCMMLNode pNode, PWSTR pszName, PWSTR pszValue, int iNumber,
                  BOOL fRemove, DFDATE date, DFTIME startTime, DFTIME endTime,
                  int iExtra)
{
   // figure out number
   WCHAR szTemp[16];
   _itow (iNumber, szTemp, 10);

   // see if it exists
   DWORD dwFind;
   PCMMLNode pSub;
   dwFind = pNode->ContentFind (pszName, gszNumber, szTemp);
   if (fRemove && (dwFind < pNode->ContentNum())) {
      // found it
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         return FALSE;  // error
      
      // delete all the sub's contents
      while (pSub->ContentNum())
         pSub->ContentRemove(0); // bugbug
   }
   else {
      // else not hree so just create
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         return FALSE;
      pSub->NameSet (pszName);
   }

   // set the attributes
   pSub->AttribSet (gszNumber, szTemp);   // sztemp already has the number
   _itow ((int)date, szTemp, 10);
   pSub->AttribSet (gszDate, szTemp);
   _itow ((int)startTime, szTemp, 10);
   pSub->AttribSet (gszStart, szTemp);
   _itow ((int)endTime, szTemp, 10);
   pSub->AttribSet (gszEnd, szTemp);
   _itow (iExtra, szTemp, 10);
   pSub->AttribSet (gszExtra, szTemp);

   return pSub->ContentAdd (pszValue);
}

/**********************************************************************
NodeElemGet - Finds the subnodes in pNode with the given pszName.
   It then finds the contents, and searches the 0th element. If that's
   a string it returns a pointer to the string.

   It's used to enumerate through all the choices. Just set *pdwIndex
   to 0 at first, and NodeElemGet will increment the value by iteself
   every time it's called.

inputs
   PCMMLNode pNode - To look in
   PWSTR    pszName - Name for the subnode
   DWORD    *pdwIndex - Should be set to 0 at first. Will be incremented
            every time this is called.
   int      *piNumber - filled with the number
   DFDATE   *pdate - filled with the date
   DFTIME   *pstartTime - filled with the start time
   DFTIME   *pendTime - filled with the end time
   int      *piExtra - filled in with extra value
returns
   PWSTR - value, or NULL if can't find. This is valid untilthe node is deleted
*/
PWSTR NodeElemGet (PCMMLNode pNode, PWSTR pszName, DWORD *pdwIndex,
                   int*piNumber, DFDATE *pdate, DFTIME *pstartTime, DFTIME *pendTime,
                   int*piExtra)
{
   // loop looking
   for (; *pdwIndex < pNode->ContentNum(); (*pdwIndex)++) {
      PCMMLNode pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum (*pdwIndex, &psz, &pSub);
      if (!pSub)
         continue;

      // compare the name
      if (_wcsicmp(pSub->NameGet(), pszName))
         continue;

      // found one

      // if looking for a number get that
      if (piNumber) {
         *piNumber = -1;
         AttribToDecimal (pSub->AttribGet(gszNumber), piNumber);
      }
      if (pdate) {
         *pdate = 0;
         AttribToDecimal (pSub->AttribGet(gszDate), (int*)pdate);
      }
      if (pstartTime) {
         *pstartTime = -1;
         AttribToDecimal (pSub->AttribGet(gszStart), (int*)pstartTime);
      }
      if (pendTime) {
         *pendTime = -1;
         AttribToDecimal (pSub->AttribGet(gszEnd), (int*) pendTime);
      }
      if (piExtra) {
         *piExtra = 0;
         AttribToDecimal (pSub->AttribGet(gszExtra), piExtra);
      }

      // increase the index for next time and done
      (*pdwIndex)++;

      // get the string
      PCMMLNode   pSub2;
      psz = NULL;
      pSub->ContentEnum(0, &psz, &pSub2);
      return psz;
   }


   return NULL;
}


/**********************************************************************
NodeElemRemove - Finds the subnodes in pNode with the given pszName.
   It then finds the contents, and searches the one with a match in iNumber.
   This is removed.

inputs
   PCMMLNode pNode - To look in
   PWSTR    pszName - Name for the subnode
   int      iNumber - the number to remove
returns
   BOOL - TRUE if found
*/
BOOL NodeElemRemove (PCMMLNode pNode, PWSTR pszName, int iNumber)
{
   // loop looking
   DWORD i;
   for (i=0; i < pNode->ContentNum(); i++) {
      PCMMLNode pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // compare the name
      if (_wcsicmp(pSub->NameGet(), pszName))
         continue;

      // make sure the number matches
      int iFound;
      iFound = iNumber+1;  // just to make sre they don't match
      AttribToDecimal (pSub->AttribGet(gszNumber), &iFound);
      if (iFound != iNumber)
         continue;   

      // found
      pNode->ContentRemove (i);
      return TRUE;
   }


   return FALSE;
}

/**********************************************************************
NodeListGet - Repeatedly calls NodeElemGet, filling in a list
of NLG structures. These are then sorted according to date and time.

inputs
   PCMMLNode pNode - To look in
   PWSTR    pszName - Name for the subnode
   BOOL     fAscending - If true, the more recent entries are at the end of list.
               if FALSE more recent at beginning. BUGFIX - So journal entries sorted by
               most recent at top.
returns
   PCListFixed - New fixed list with NLG structures. The caller must free this.
*/
int __cdecl NLGSort(const void *elem1, const void *elem2 )
{
   PNLG p1, p2;
   p1 = (PNLG)elem1;
   p2 = (PNLG)elem2;

   if (p1->date != p2->date)
      return (int)p1->date - (int)p2->date;

   // else by start
   return (int)p1->start - (int)p2->start;
}
int __cdecl NLGSortReverse(const void *elem1, const void *elem2 ) // most recents entries highest in list
{
   PNLG p1, p2;
   p1 = (PNLG)elem1;
   p2 = (PNLG)elem2;

   if (p1->date != p2->date)
      return (int)p2->date - (int)p1->date;

   // else by start
   return (int)p2->start - (int)p1->start;
}
PCListFixed NodeListGet (PCMMLNode pNode, PWSTR pszName, BOOL fAscending)
{
   NLG nlg;
   
   // create the list object
   PCListFixed pl = new CListFixed;
   DWORD dwIndex = 0;
   if (!pl)
      return NULL;
   pl->Init (sizeof(NLG));

   while (nlg.psz = NodeElemGet (pNode, pszName, &dwIndex, &nlg.iNumber, &nlg.date, &nlg.start, &nlg.end, &nlg.iExtra)) {
      pl->Add (&nlg);
   }

   // sort
   qsort (pl->Get(0), pl->Num(), sizeof(NLG), fAscending ? NLGSort : NLGSortReverse);

   // done
   return pl;
}

/**************************************************************************
MonthYearTree - This returns a pointer (and dwNode) to a database node
   specific to the date's month/year.

   It does this by creating a string from the pszPrefix, month, and year.
   This string is referenced in pRoot use pRoot->ContentFind (pszPrefix).
   The "number=" attribute stores the database node number that contains
   the month/year specific information.

inputs
   PCMMLNode      pRoot - Root node to create the index for month/year in.
   DFDATE         date - Date to look for
   PWSTR          pszPrefix - Short prefix to use
   BOOL           fCreateIfNotExit - if TRUE create the database node
                  if it doesn't exist. If FALSE, return NULL if it doesn't exist.
   DWORD          *pdwNode - Filled in with a node specific to the month/year.
returns
   PCMMLNode - Node (must be released) specific to the month/year. NULL if cant find/create.
*/
PCMMLNode MonthYearTree (PCMMLNode pRoot, DFDATE date, PWSTR pszPrefix,
                         BOOL fCreateIfNotExist, DWORD *pdwNode)
{
   HANGFUNCIN;
   if (date == 0)
      return NULL;

   // make up the string
   WCHAR szTemp[64];
   swprintf (szTemp, L"%s-%d-%d", pszPrefix, (int)YEARFROMDFDATE(date), (int)MONTHFROMDFDATE(date));

   // find the one that exists
   DWORD dwIndex;
   dwIndex = pRoot->ContentFind (szTemp);

   // if found
   if (dwIndex != (DWORD)-1) {
      PWSTR psz;
      PCMMLNode pSub;
      pSub = NULL;
      pRoot->ContentEnum (dwIndex, &psz, &pSub);
      if (!pSub)
         return NULL;

      int iNode;
      iNode = -1;
      AttribToDecimal (pSub->AttribGet(gszNumber), &iNode);
      if (iNode == -1)
         return NULL;

      // have the node
      *pdwNode = (DWORD)iNode;
      return gpData->NodeGet (*pdwNode);
   }


   // else, can't find
   if (!fCreateIfNotExist)
      return NULL;   // supposed to exit if cant find

   // else create
   PCMMLNode pNew;
   DWORD dwNewNode;
   pNew = gpData->NodeAdd (pszPrefix, &dwNewNode);
   if (!pNew)
      return NULL;   // error

   PCMMLNode pSub;
   pSub = pRoot->ContentAddNewNode ();
   if (!pSub)
      return NULL;
   pSub->NameSet (szTemp);
   WCHAR szNumber[16];
   _itow ((int)dwNewNode, szNumber, 10);
   pSub->AttribSet (gszNumber, szNumber);

   // done
   *pdwNode = dwNewNode;
   return pNew;
}

/**************************************************************************
EnumMonthYearTree - Enumerates a month-year tree and returns a list of
DFDATE elements (the list must be freed by the caller), sorted, containing
all the dates that are valid.

inputs
   PCMMLNode      pRoot - Root node to create the index for month/year in.
   PWSTR          pszPrefix - Short prefix to use
returns
   PCListFixed - of DFDATE. Must be freed by caller
*/
int __cdecl EMLSort(const void *elem1, const void *elem2 )
{
   DFDATE *p1, *p2;
   p1 = (DFDATE*)elem1;
   p2 = (DFDATE*)elem2;

   return (int)(*p1)-(int)(*p2);
}
PCListFixed EnumMonthYearTree (PCMMLNode pRoot, PWSTR pszPrefix)
{
   HANGFUNCIN;
   // new list
   CListFixed *pNew;
   pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(DFDATE));

   // make up the string
   WCHAR szTemp[64];
   DWORD dwLen;
   swprintf (szTemp, L"%s-", pszPrefix);
   dwLen = wcslen(szTemp);

   // find the one that exists
   DWORD dwIndex;
   PWSTR psz;
   PCMMLNode pSub;
   for (dwIndex = 0; dwIndex < pRoot->ContentNum(); dwIndex++) {
      pSub = NULL;
      pRoot->ContentEnum (dwIndex, &psz, &pSub);
      if (!pSub)
         continue;

      // get the name
      if (_wcsnicmp (pSub->NameGet(), szTemp, dwLen))
         continue;

      // else name matches

      // pull out the month & year into date
      DWORD dwMonth, dwYear;
      PWSTR pCur;
      pCur = pSub->NameGet() + dwLen;
      dwYear = (DWORD) _wtoi (pCur);
      pCur = wcschr (pCur, L'-');
      if (!pCur)
         continue;   // error
      pCur++;
      dwMonth = (DWORD)_wtoi(pCur);

      // add this
      DFDATE date;
      date = TODFDATE(0, dwMonth, dwYear);
      pNew->Add (&date);
   }

   // sort
   qsort (pNew->Get(0), pNew->Num(), sizeof(DFDATE), EMLSort);

   return pNew;

}


/**************************************************************************
DayMonthYearTree - This returns a pointer (and dwNode) to a database node
   specific to the date's day/month/year.

   It does this by first calling MonthYearTree(). Then, in the node
   returned by that, it creates a string from the pszPrefix, day, month, and year.
   This string is referenced in pRoot use pRoot->ContentFind (pszPrefix).
   The "number=" attribute stores the database node number that contains
   the day/month/year specific information.

inputs
   PCMMLNode      pRoot - Root node to create the index for month/year in.
   DFDATE         date - Date to look for
   PWSTR          pszPrefix - Short prefix to use
   BOOL           fCreateIfNotExit - if TRUE create the database node
                  if it doesn't exist. If FALSE, return NULL if it doesn't exist.
   DWORD          *pdwNode - Filled in with a node specific to the month/year.
returns
   PCMMLNode - Node (must be released) specific to the month/year. NULL if cant find/create.
*/
PCMMLNode DayMonthYearTree (PCMMLNode pRoot, DFDATE date, PWSTR pszPrefix,
                         BOOL fCreateIfNotExist, DWORD *pdwNode)
{
   HANGFUNCIN;
   if (date == 0)
      return NULL;

   // make up the string
   WCHAR szTemp[64], szTemp2[64];
   swprintf (szTemp, L"%s-%d-%d-%d", pszPrefix, (int)YEARFROMDFDATE(date), (int)MONTHFROMDFDATE(date),
      (int)DAYFROMDFDATE(date));
   swprintf (szTemp2, L"%sDay", pszPrefix);

   // find the monthyear tree
   PCMMLNode   pmy;
   DWORD       dwmy;
   pmy = MonthYearTree (pRoot, date, pszPrefix, fCreateIfNotExist, &dwmy);
   if (!pmy)
      return NULL;

   // find the one that exists
   DWORD dwIndex;
   dwIndex = pmy->ContentFind (szTemp);

   // if found
   if (dwIndex != (DWORD)-1) {
      PWSTR psz;
      PCMMLNode pSub;
      pSub = NULL;
      pmy->ContentEnum (dwIndex, &psz, &pSub);
      if (!pSub) {
         gpData->NodeRelease (pmy);
         return NULL;
      }

      int iNode;
      iNode = -1;
      AttribToDecimal (pSub->AttribGet(gszNumber), &iNode);
      gpData->NodeRelease (pmy);
      if (iNode == -1) {
         return NULL;
      }

      // have the node
      *pdwNode = (DWORD)iNode;
      return gpData->NodeGet (*pdwNode);
      // BUGBUG - Daily wrapup stopped working for awhile. Appears that gpData->NodeGet()
      // returns NULL, even though node should exist... which may mean some corruption
      // the system seems to have healed itself though
   }


   // else, can't find
   if (!fCreateIfNotExist) {
      gpData->NodeRelease (pmy);
      return NULL;   // supposed to exit if cant find
   }

   // else create
   PCMMLNode pNew;
   DWORD dwNewNode;
   pNew = gpData->NodeAdd (szTemp2, &dwNewNode);
   if (!pNew) {
      gpData->NodeRelease (pmy);
      return NULL;   // error
   }

   PCMMLNode pSub;
   pSub = pmy->ContentAddNewNode ();
   if (!pSub) {
      gpData->NodeRelease (pmy);
      return NULL;
   }
   pSub->NameSet (szTemp);
   WCHAR szNumber[16];
   _itow ((int)dwNewNode, szNumber, 10);
   pSub->AttribSet (gszNumber, szNumber);
   gpData->NodeRelease (pmy);

   // done
   *pdwNode = dwNewNode;
   return pNew;
}



/***************************************************************************8
RepeatableRandom - Returns a randomly generated number that's repeatable
(based on the current date). Used to ensure that on Jan-10 (or other specific day)
the same question is asked, or the same deep thought is given. User cant
keep going back and forth.

inputs
   int iSeed - Seed value
   DWORD dwIndex - 0 to use first random number that get, 1 second, etc.
returns
   int - random value
*/
int RepeatableRandom (int iSeed, DWORD dwIndex)
{
   HANGFUNCIN;
   // store the old random
   int iOld = rand();
   int   iRand;

   // new see
   srand(iSeed);
   dwIndex++;
   do {
      dwIndex--;
      iRand = rand();
   } while(dwIndex);


   // reseed
   srand(iOld);
   return iRand;
}

/***********************************************************************
GetCBValue - Returns value from the combobox. Uses atoi(name) for the return.

inputs
   PCEscPage   pPage - page
   PWSTR       pszName - name
   int         iDefault - Default value
returns
   int - value
*/
int GetCBValue (PCEscPage pPage, PWSTR pszName, int iDefault)
{
   PCEscControl pControl = pPage->ControlFind(pszName);
   if (!pControl)
      return iDefault;
   int   iSel;
   iSel = pControl->AttribGetInt (gszCurSel);
   ESCMCOMBOBOXGETITEM i;
   memset (&i, 0, sizeof(i));
   i.dwIndex = (DWORD) iSel;
   pControl->Message (ESCM_COMBOBOXGETITEM, &i);

   if (i.pszName)
      return _wtoi (i.pszName);
   else
      return iDefault;
}
   

/**************************************************************************8
DiskFreeSpace - Returns the amount of free space (in bytes) on the database's
disk.
*/
__int64 DiskFreeSpace (PWSTR psz)
{
   // BUGFIX - Take out GetDisktFreeSpaceEx since it's causing problems on early version of WIn95
   return 1000000000;   // large number because dont really know
#if 0
   __int64 free1, free2, free3;

   char szaTemp[256];
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szaTemp, sizeof(szaTemp), 0, 0);

   // go until get color
   char *c;
   for (c = szaTemp; *c; c++)
      if (*c == ':') {
         c[1] = 0;
         break;
      };

   if (!GetDiskFreeSpaceEx (szaTemp, (PULARGE_INTEGER ) &free1, (PULARGE_INTEGER )&free2, (PULARGE_INTEGER )&free3))
      return 1000000000;   // large number because dont really know

   return free1;
#endif // 0
}


/****************************************************************8
KeyGet - Get a DWORD value from registry.

inputs
   char  *pszKey - Key to use
   DWORD    dwDefault - default value
returns
   DWORD - value
*/
DWORD KeyGet (char *pszKey, DWORD dwDefault)
{
   HANGFUNCIN;
   DWORD dwKey;
   dwKey = 0;

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegBase, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return 0;

   DWORD dwSize, dwType;
   dwSize = sizeof(DWORD);
   dwKey = dwDefault;
   RegQueryValueEx (hKey, pszKey, NULL, &dwType, (LPBYTE) &dwKey, &dwSize);

   RegCloseKey (hKey);

   return dwKey;
}

/****************************************************************8
KeySet - Write a key to the registry

inputs
   char  *pszKey - Key to use
   DWORD dwValue - value
returns
   none
*/
void KeySet (char *pszKey, DWORD dwValue)
{
   HANGFUNCIN;
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegBase, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, pszKey, 0, REG_DWORD, (BYTE*) &dwValue, sizeof(dwValue));

   RegCloseKey (hKey);

   return;
}



/****************************************************************8
KeyGetString - Get a DWORD value from registry.

inputs
   char  *pszKey - Key to use
   char  *pszValue - filled with value
   DWORD dwSize - size of value
returns
   TRUE if success
*/
BOOL KeyGetString (char *pszKey, char *pszValue, DWORD dwSize)
{
   HANGFUNCIN;
   DWORD dwKey;
   dwKey = 0;

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegBase, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return 0;

   DWORD dwType;
   LONG lRet;
   lRet = RegQueryValueEx (hKey, pszKey, NULL, &dwType, (LPBYTE) pszValue, &dwSize);

   RegCloseKey (hKey);

   return (lRet == ERROR_SUCCESS);
}

/****************************************************************8
KeySetString - Write a key to the registry

inputs
   char  *pszKey - Key to use
   char  *pszValue - value to use
returns
   none
*/
void KeySetString (char *pszKey, char *pszValue)
{
   HANGFUNCIN;
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegBase, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   RegSetValueEx (hKey, pszKey, 0, REG_SZ, (BYTE*) pszValue, strlen(pszValue)+1);

   RegCloseKey (hKey);

   return;
}


/**********************************************************************
AlarmPage - Page callback for alarms.
*/
#define  NOTEBASE       62-8
#define  VOLUME         127

static DWORD   gdwAlarmMin;
static BOOL    gfAlarmPageActive = FALSE;
static PWSTR   gpszAlarmMain;
static PWSTR   gpszAlarmSub;
BOOL AlarmPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      pPage->m_pWindow->TimerSet(30*1000, pPage);
      pPage->Message (ESCM_TIMER);
      break;

   case ESCM_TIMER:
      {
         ESCMIDIEVENT aChime[] = {
            {0, MIDIINSTRUMENT (0, 8*15+4)}, // flute
            {0, MIDINOTEON (0, NOTEBASE+0,VOLUME)},
            {1000, MIDINOTEOFF (0, NOTEBASE+0)},
         };
         EscChime (aChime, sizeof(aChime) / sizeof(ESCMIDIEVENT));

         // speak
         EscSpeak (gpszAlarmMain);
      }
      return TRUE;

   case ESCM_LINK:
      {
         // just get the number of minutes and fall through
         PCEscControl pControl = pPage->ControlFind (L"time");
         gdwAlarmMin = pControl ? (DWORD)pControl->AttribGetInt (gszText) : 0;
      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"MAINALERT")) {
            p->pszSubString = gpszAlarmMain;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SUBALERT")) {
            p->pszSubString = gpszAlarmSub;
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
AlarmIsVisible - Returns TRUE if the alarm page is visible, which means that
no other alarms should be brought up.
*/
BOOL AlarmIsVisible (void)
{
   return gfAlarmPageActive;
}

/*****************************************************************************
AlarmUI - Asks how long a task took.

inputs
   PWSTR          pszMain - Main text to display.
   PWSTR          pszSub - Sub-text to display. This is MML.
returns
   DWORD - 0 if user pressed OK, or a number of the number of minutes to snooze
*/
DWORD AlarmUI (PWSTR pszMain, PWSTR pszSub)
{
   HANGFUNCIN;
   // dont pull up while visible
   if (AlarmIsVisible())
      return 0;
   gfAlarmPageActive = TRUE;

   // make sure dragonfly is visible
   ShowWindow (gpWindow->m_hWnd, SW_SHOW);
   if (IsIconic(gpWindow->m_hWnd))
      ShowWindow (gpWindow->m_hWnd, SW_RESTORE);

   gpszAlarmMain = pszMain;
   gpszAlarmSub = pszSub;

   CEscWindow  cWindow;
   //RECT  r;
   //DialogRect (&r);
   cWindow.Init (ghInstance, gpWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLALARM, AlarmPage);

   if (!pszRet || _wcsicmp(pszRet, L"snooze"))
      gdwAlarmMin = 0;

   gfAlarmPageActive = FALSE;

   return gdwAlarmMin;
}


/***********************************************************************
HowLongPage - Page callback for quick-adding a new user
*/
static double gfHowLong;   // for default value
BOOL HowLongPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR  szTemp[32];
         swprintf (szTemp, L"%g", gfHowLong);
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"howlong");
         if (pControl)
            pControl->AttribSet (gszText, szTemp);
      }
      break;   // fall through

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
         WCHAR szName[32];
         DWORD dwNeeded;

         // get the name
         szName[1] = 0;
         szName[0] = L'!';
         pControl = pPage->ControlFind (L"howlong");
         if (pControl)
            pControl->AttribGet (gszText, szName+1, sizeof(szName)-2, &dwNeeded);
         

         // return saying what the new node is
         pPage->Link (szName);
         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
HowLong - Asks how long a task took.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
   double         fDefault - Default time
returns
   DWORD - double - new value. -1 if no time
*/
double HowLong (PCEscPage pPage, double fDefault)
{
   HANGFUNCIN;
   gfHowLong = fDefault;

   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLHOWLONG, HowLongPage);

   if (!pszRet || (pszRet[0] != L'!'))
      return -1;   // canceled

   if (!AttribToDouble(pszRet + 1, &gfHowLong))
      return -1;
   return gfHowLong;
}




/*******************************************************************************8
CPlanTask:: constructor and destructor */
CPlanTask::CPlanTask (void)
{
   HANGFUNCIN;
   m_list.Init (sizeof(PLANTASKITEM));
   m_dwTimeCompleted = 0;
}

CPlanTask::~CPlanTask (void)
{
   HANGFUNCIN;
   // left blank
}


/*******************************************************************************8
CPlanTask::Num - returns the number of task items
*/
DWORD CPlanTask::Num (void)
{
   HANGFUNCIN;
   return m_list.Num();
}

/*******************************************************************************
CPlanTask::Get - Returns a pointer to a task item. This is valid until the
individual task is deleted, or the object is deleted. Changing this structure will
change the data in the task.
*/
PPLANTASKITEM CPlanTask::Get (DWORD dwElem)
{
   HANGFUNCIN;
   return (PPLANTASKITEM) m_list.Get(dwElem);
}

/*******************************************************************************
CPlanTask::Remove - Deletes item
*/
BOOL CPlanTask::Remove (DWORD dwElem)
{
   HANGFUNCIN;
   return m_list.Remove(dwElem);
}

/*******************************************************************************
CPlanTask::Add - Adds a new item, copying the data from the given structure
*/
DWORD CPlanTask::Add (PPLANTASKITEM pItem)
{
   HANGFUNCIN;
   return m_list.Add (pItem);
}

/*******************************************************************************
CPlanTask::Verify - This should be called after reading in the data and before
really using it. It goes through the task data and

   0) Sort the list by date.
   1) makes sure tasks are scheduled to occur on/after today. If one doesn't, it's
      deleted and it's time is added onto the next day. If the next day doesn't
      exist then it's not added anyhwere.
   2) Sums up all the time remaining in the list, and in m_dwTimeCompleted. If this
      is less than dwTotalTime Verify returns the time that remains unfinished, else 0.
   3) If there is too much time then time is deleted from the last elements.
   4) If there is time remaining then a new element is added with date addto, and the
         list is resorted. If it's longer than dwMinPerDay then it's added subsequent days

returns
   TRUE - if changed anything, FALSE if didn't change
*/
int __cdecl PlanTaskSort(const void *elem1, const void *elem2 )
{
   HANGFUNCIN;
   PPLANTASKITEM p1, p2;
   p1 = (PPLANTASKITEM)elem1;
   p2 = (PPLANTASKITEM)elem2;

   if (p1->date != p2->date)
      return (int)p1->date - (int)p2->date;

   // else, by priority
   return (int)p1->dwPriority - (int)p2->dwPriority;
}
BOOL CPlanTask::Verify (DWORD dwTotalTime, DFDATE today, DFDATE addto, DWORD dwPriority, DWORD dwMinPerDay)
{
   BOOL fChanged = FALSE;

   // sort
   qsort (Get(0), Num(), sizeof(PLANTASKITEM), PlanTaskSort);

   // delete tasks before today
   DWORD i;
   PPLANTASKITEM p, p2;
   for (i = 0; i < Num(); ) {
      p = Get(i);
      if (p->date >= today)
         break;   // if this is after today then they're all after today

      // need to delete this one
      p2 = Get(i+1);
      if (p2)
         p2->dwTimeAllotted += p->dwTimeAllotted;
      Remove(0);
      fChanged = TRUE;
      continue;   // using the same i
   }

   // how much time is accounted for
   DWORD dwTime, dwRet;
   dwTime = m_dwTimeCompleted;
   for (i = 0; i < Num(); i++) {
      p = Get(i);
      dwTime += p->dwTimeAllotted;
   }
   dwRet = dwTime;


   if (dwTime > dwTotalTime) {
      // work backwards taking time away from the end tasks.
      // note: don't delete the first task because the user hasn't
      // officially completed it so there must still be some work left
      fChanged = TRUE;
      DWORD dwDel = dwTime - dwTotalTime;
      for (i = Num()-1; dwDel && (i < Num()); i--) {
         p = Get(i);
         if (dwDel < p->dwTimeAllotted) {
            p->dwTimeAllotted -= dwDel;
            break;
         }

         if (i) {
            // else timealloted <= dwDel
            dwDel -= p->dwTimeAllotted;
            Remove(i);
         }
      }
   }
   else if (dwTime < dwTotalTime) {
      // see if there's a last split and try to add to that
      DWORD dwAdd = dwTotalTime - dwTime;
      DFDATE dLast = addto;
      if (Num()) {
         PPLANTASKITEM pp = Get(Num()-1);
         DWORD dwCanAdd;
         dwCanAdd = dwMinPerDay - min(pp->dwTimeAllotted, dwMinPerDay);
         dwCanAdd = min(dwAdd, dwCanAdd);
         pp->dwTimeAllotted += dwCanAdd;
         dwAdd -= dwCanAdd;

         // if we have to add more time then add the day after this
         dLast = MinutesToDFDATE(DFDATEToMinutes(pp->date)+24*60);
      }

      // add whatever is left to more splits at the end
      PLANTASKITEM pi;
      memset (&pi, 0, sizeof(pi));
      pi.dwPriority = dwPriority;
      pi.date = dLast;
      fChanged = TRUE;
      while (dwAdd) {
         pi.dwTimeAllotted = min(dwAdd, dwMinPerDay);
         dwAdd -= pi.dwTimeAllotted;

         Add (&pi);

         // next day
         pi.date = MinutesToDFDATE(DFDATEToMinutes(pi.date) + 24*60);
      }
   }

   return fChanged;
}

/*******************************************************************************
CPlanTask::Write - Writes the data into a PCMMLNode
*/
BOOL CPlanTask::Write (PCMMLNode pNode)
{
   HANGFUNCIN;
   NodeValueSet (pNode, gszPlanTaskNumElem, (int) Num());      // bugbug - never returned from nodevalueset. never got to num. Not possible
   NodeValueSet (pNode, gszPlanTaskTimeCompleted, (int) m_dwTimeCompleted);

   DWORD i;
   PPLANTASKITEM p; // bugbug
   for (i = 0; i < Num(); i++) {
      p = Get(i);

      NodeElemSet (pNode, gszPlanTaskElem, gszPlanTaskElem, (int)i, TRUE, p->date, p->start,
         p->dwPriority, (int)p->dwTimeAllotted);
   }
   return TRUE;
}


/*******************************************************************************
CPlanTask::Read - Reads the data into a PCMMLNode
*/
BOOL CPlanTask::Read (PCMMLNode pNode)
{
   HANGFUNCIN;
   DWORD dwNum;

   // clear out
   m_list.Clear();
   m_dwTimeCompleted = 0;

   dwNum = (DWORD) NodeValueGetInt (pNode, gszPlanTaskNumElem, 0);
   m_dwTimeCompleted = (DWORD) NodeValueGetInt (pNode, gszPlanTaskTimeCompleted, 0);

   DWORD i;
   PLANTASKITEM p;
   DWORD dwIndex;
   int iNumber;
   dwIndex = 0;
   memset (&p, 0, sizeof(p));
   for (i = 0; i < dwNum; i++) {
      Add(&p);
   }
   for (i = 0; ; ) {
      PWSTR psz;
      psz = NodeElemGet (pNode, gszPlanTaskElem, &dwIndex, &iNumber, &p.date, &p.start,
         (DFTIME*) &p.dwPriority, (int*) &p.dwTimeAllotted);
      if (!psz)
         break;   // shouldn't do this
      if (iNumber >= (int) dwNum)
         continue;   // shouldn't have this

      // get the existing blank one and copy over so it doesn't mtter
      // in wich order the data is saved in pNode
      PPLANTASKITEM pp;
      pp = Get((DWORD)iNumber);
      memcpy (pp, &p, sizeof(p));
      i++;
   }

   // sort since assume elsewhere that it's all sorted
   qsort (Get(0), Num(), sizeof(PLANTASKITEM), PlanTaskSort);

   return TRUE;
}


/************************************************************************************
DateToDayOfYear - Given a DFDATE, this returns the day number (as in nth day of the year),
the number of days in the year, and the week number.

inputs
   DFDATE      date - date to ask about
   DWORD       *pdwDayIndex - 1..365th day of the year
   DWORD       *pdwDaysInYear - 364 or 365
   DWORD       *pdwWeekIndex - 1..53 week of the year
returns
   none
*/
void DateToDayOfYear (DFDATE date, DWORD *pdwDayIndex, DWORD *pdwDaysInYear, DWORD *pdwWeekIndex)
{
   HANGFUNCIN;
   // find out when january 1st is
   DFDATE   Jan1 = TODFDATE(1, 1, YEARFROMDFDATE(date));
   static DFDATE sJan1 = 0;
   static __int64 iMinJan1;
   static DWORD dwDaysInYear;
   static int iDOW;
   if (Jan1 != sJan1) { // optimization so dont need to ask every time
      sJan1 = Jan1;
      iMinJan1 = DFDATEToMinutes (Jan1);
      dwDaysInYear = (DWORD) ((DFDATEToMinutes(TODFDATE(1,1,YEARFROMDFDATE(Jan1)+1)) - iMinJan1) / 60 / 24);

      // starting day
      EscDaysInMonth (1, YEARFROMDFDATE(Jan1), &iDOW);
   }

   // day index
   __int64 iNow;
   iNow = DFDATEToMinutes (date);
   *pdwDayIndex = (DWORD) ((iNow - iMinJan1) / 60 / 24)+1;
   *pdwDaysInYear = dwDaysInYear;
   *pdwWeekIndex = ((*pdwDayIndex-1) + (DWORD)iDOW) / 7 + 1;


}
