/**********************************************************************
People.cpp - Code that handles people database and UI.

begun 6/14/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <tapi.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


#define  NUMRELATIONS         8

BOOL           gfPeoplePopulated = FALSE;// set to true if glistPeople is populated
CListVariable  glistPeople;   // list of people's names
CListFixed     glistPeopleID; // database ID/location for each person so named in glistPeople
CListVariable  glistBusiness; // list of business names
CListFixed     glistBusinessID;  // list of ID/location for business
CListVariable  glistPeopleBusiness; // combination of people & busienss
CListFixed     glistPeopleBusinessID;  // combination of people and business
static DWORD   gdwNode = 0;   // current node being viewed
static CListVariable glistFields;   // list of field names for import
static PCListFixed gplistPeople; // list of PCListVariable containing field names for people.

static WCHAR   gszPersonQuickAdd[] = L"PersonQuickAdd";
static PWSTR   gaszRelations[NUMRELATIONS] = {
   L"Aquaintance", L"Business",  // NOTE: "Business" is hardcoded to "1", and referred to later
   L"Coworker", L"Family",
   L"Friend", L"Miscellaneous",
   L"Neighbor", L"Relative"
};
static WCHAR gszHomeStreet[] = L"HomeStreet";
static WCHAR gszHomeCity[] = L"HomeCity";
static WCHAR gszHomeState[] = L"HomeState";
static WCHAR gszHomePostal[] = L"HomePostal";
static WCHAR gszHomeCountry[] = L"HomeCountry";
static WCHAR gszBusinessStreet[] = L"BusinessStreet";
static WCHAR gszBusinessCity[] = L"BusinessCity";
static WCHAR gszBusinessState[] = L"BusinessState";
static WCHAR gszBusinessPostal[] = L"BusinessPostal";
static WCHAR gszBusinessCountry[] = L"BusinessCountry";

DWORD AddPerson (PCEscPage pPage, PWSTR pszFirst, PWSTR pszLast, PWSTR pszNick);
DWORD AddBusiness (PCEscPage pPage, PWSTR pszLast);

/***********************************************************************
XXXFilteredList - This makes sure the filtered list (CListVariable)
for people and businesses is populated. If not, all the information is loaded from
the people major-section (see FindMajorSection()).

This fills in:
   gfPeoplePopulated - Set to TRUE when it's been loaded
   glistPeople - CListVariable of people's names followed by nicknames
   glistPeopleID - CListFixed with the person's CDatabase ID.
   glistBusiness, glistBusinessID
   glistPeopleBusiness, glistPeopleBusinessID

inputs
   none
returns
   none
*/
void XXXFilteredList (void)
{
   HANGFUNCIN;
   if (gfPeoplePopulated)
      return;

   // else get it
   PCMMLNode   pNode;
   pNode = FindMajorSection (gszPeopleNode);
   if (!pNode)
      return;   // error. This shouldn't happen

   // fill the list in
   FillFilteredList (pNode, gszPersonNode,&glistPeople, &glistPeopleID);
   FillFilteredList (pNode, gszBusinessNode, &glistBusiness, &glistBusinessID);
   gfPeoplePopulated = TRUE;

   // when first start up set up default entry
   if (!NodeValueGetInt (pNode, gszExamples, FALSE)) {
      // set flag true
      NodeValueSet (pNode, gszExamples, (int) TRUE);

      // add journal categories
      DWORD dwNode;
      dwNode = AddPerson (NULL, L"mXac", L"", L"");
      PCMMLNode pPerson;
      pPerson = NULL;
      if (dwNode != (DWORD)-1)
         pPerson = gpData->NodeGet (dwNode);
      if (pPerson) {
         NodeValueSet (pPerson, gszBusinessEMail, L"Mike@mXac.com.au");
         NodeValueSet (pPerson, gszCompany, L"mXac");
         gpData->NodeRelease(pPerson);
      }

      // write to disk
      gpData->Flush();
   }

   // combine the person and busines list
   glistPeopleBusinessID.Init (sizeof(DWORD));
   DWORD i;
   for (i = 0; i < glistPeople.Num(); i++) {
      DWORD dwLen;
      PWSTR psz;
      DWORD *pdw;
      dwLen = glistPeople.Size(i);
      psz = (PWSTR) glistPeople.Get(i);
      pdw = (DWORD*) glistPeopleID.Get(i);
      if (psz && dwLen && pdw) {
         glistPeopleBusiness.Add (psz, dwLen);
         glistPeopleBusinessID.Add (pdw);
      }
   }
   for (i = 0; i < glistBusiness.Num(); i++) {
      DWORD dwLen;
      PWSTR psz;
      DWORD *pdw;
      dwLen = glistBusiness.Size(i);
      psz = (PWSTR) glistBusiness.Get(i);
      pdw = (DWORD*) glistBusinessID.Get(i);
      if (psz && dwLen && pdw) {
         glistPeopleBusiness.Add (psz, dwLen);
         glistPeopleBusinessID.Add (pdw);
      }
   }

   // finall, flush release
   gpData->Flush();
   gpData->NodeRelease(pNode);


   return;
}

/***********************************************************************
PeopleFilteredList - This makes sure the filtered list (CListVariable)
for people is populated. If not, all the information is loaded from
the people major-section (see FindMajorSection()).

This fills in:
   gfPeoplePopulated - Set to TRUE when it's been loaded
   glistPeople - CListVariable of people's names followed by nicknames
   glistPeopleID - CListFixed with the person's CDatabase ID.

inputs
   none
returns
   PCListVariable - for the FilteredList control
*/
PCListVariable PeopleFilteredList (void)
{
   HANGFUNCIN;
   XXXFilteredList ();
   return &glistPeople;
}
PCListVariable BusinessFilteredList (void)
{
   HANGFUNCIN;
   XXXFilteredList ();
   return &glistBusiness;
}
PCListVariable PeopleBusinessFilteredList (void)
{
   HANGFUNCIN;
   XXXFilteredList ();
   return &glistPeopleBusiness;
}

/***********************************************************************
PeopleIndexToDatabase - Given an index ID (into glistPeople), this
returns a DWORD CDatabase location for the person

inputs
   DWORD - dwIndex
returns
   DWORD - CDatabase location. -1 if cant find
*/
DWORD PeopleIndexToDatabase (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   PeopleFilteredList();

   // find the index
   return FilteredIndexToDatabase (dwIndex, &glistPeople, &glistPeopleID);
}
DWORD BusinessIndexToDatabase (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   BusinessFilteredList();

   // find the index
   return FilteredIndexToDatabase (dwIndex, &glistBusiness, &glistBusinessID);
}
DWORD PeopleBusinessIndexToDatabase (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   PeopleBusinessFilteredList();

   // find the index
   return FilteredIndexToDatabase (dwIndex, &glistPeopleBusiness, &glistPeopleBusinessID);
}


/***********************************************************************
PeopleIndexToName - Given an index return a string for the person's name

inputs
   DWORD - dwIndex
returns
   PWSTR - string valid for awhile. NULL if cant find
*/
PWSTR PeopleIndexToName (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   PeopleFilteredList();

   // find the index
   return (PWSTR) glistPeople.Get(dwIndex);
}
PWSTR BusinessIndexToName (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   BusinessFilteredList();

   // find the index
   return (PWSTR) glistBusiness.Get(dwIndex);
}
PWSTR PeopleBusinessIndexToName (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   PeopleBusinessFilteredList();

   // find the index
   return (PWSTR) glistPeopleBusiness.Get(dwIndex);
}


/***********************************************************************
PeopleDatabaseToIndex - Given a CDatabase location for the person data
this returns an index into the glistPeople.

inputs
   DWORD - CDatabase location
returns
   DWORD - Index into clistPeople. -1 if cant find
*/
DWORD PeopleDatabaseToIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   PeopleFilteredList();

   // find the index
   return FilteredDatabaseToIndex (dwIndex, &glistPeople, &glistPeopleID);
}
DWORD BusinessDatabaseToIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   BusinessFilteredList();

   // find the index
   return FilteredDatabaseToIndex (dwIndex, &glistBusiness, &glistBusinessID);
}
DWORD PeopleBusinessDatabaseToIndex (DWORD dwIndex)
{
   HANGFUNCIN;
   // make sure the list is loaded
   PeopleBusinessFilteredList();

   // find the index
   return FilteredDatabaseToIndex (dwIndex, &glistPeopleBusiness, &glistPeopleBusinessID);
}

/***********************************************************************
PeopleFullName - Given last, first, and nick-name, generate a full name.

inputs
   PWSTR       pszLast - last name. may be null or empty.
   PWSTR       pszFirst - first name. may be null or empty.
   PWSTR       pszNick - nickname. may be null or empty.
   PWSTR       pszFull - to be filled in with the full name.
   DWORD       dwSize - number of bytes in pszFull
returns
   BOOL - TRUE if succeded
*/
BOOL PeopleFullName (PWSTR pszLast, PWSTR pszFirst, PWSTR pszNick,
                      PWSTR pszFull, DWORD dwSize)
{
   HANGFUNCIN;
   // make sure there's enough space
   DWORD dwRequired;
   dwRequired = (wcslen(pszLast) + wcslen(pszFirst) + wcslen(pszNick) + 6) * 2;
   if (dwRequired > dwSize)
      return FALSE;

   pszFull[0] = 0;
   if (pszLast[0] && pszFirst[0])
      swprintf (pszFull, L"%s, %s", pszLast, pszFirst);
   else if (pszLast[0])
      wcscpy (pszFull, pszLast);
   else
      wcscpy (pszFull, pszFirst);

   if (pszNick[0])
      swprintf (pszFull + wcslen(pszFull), L" (%s)", pszNick);

   return TRUE;
}



/***********************************************************************
WriteCombo - Gets the combobox contents and write it to a node

inputs
   PCMMLNode      pNode - Node to write in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the combo resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL WriteCombo (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   iCurSel = pControl->AttribGetInt (gszCurSel);
   ESCMCOMBOBOXGETITEM item;
   memset (&item, 0, sizeof(item));
   item.dwIndex = (DWORD)iCurSel;
   pControl->Message (ESCM_COMBOBOXGETITEM, &item);
   if (!item.pszName)
      item.pszName = L"";  // blank

   // set it
   return NodeValueSet (pNode, pszName, item.pszName);
}

/***********************************************************************
WriteEdit - Gets the edit contents and write it to a node

inputs
   PCMMLNode      pNode - Node to write in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL WriteEdit (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   WCHAR szHuge[10000];
   DWORD dwNeeded;
   szHuge[0] = 0;
   pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);

   // set it
   return NodeValueSet (pNode, pszName, szHuge);
}

/***********************************************************************
WriteDate - Write a date to a node

inputs
   PCMMLNode      pNode - Node to write in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL WriteDate (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   DFDATE   date;
   date = DateControlGet (pPage, pszControlName);

   // write it
   return NodeValueSet (pNode, pszName, (int) date);
}


/***********************************************************************
ClearRelationships - This clears any relationship (parent, spouse ,children)
thant A has with B.

inputs
   DWORD    dwNodeA - If this ends up being any relationship in dwNodwB
               then dwNodwB has that relation cleared.
   DWORD    dwNodeB - As above
void
*/
void ClearRelationships (DWORD dwNodeA, DWORD dwNodeB)
{
   HANGFUNCIN;
   // get the node B
   PCMMLNode   pNode;
   pNode = gpData->NodeGet(dwNodeB);
   if (!pNode)
      return;

   // clear
   DWORD dwMatch;
   PWSTR psz;
   PWSTR szEmpty = L"";

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszSpouse, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszSpouse, szEmpty, -1);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszFather, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszFather, szEmpty, -1);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszMother, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszMother, szEmpty, -1);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild1, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild1, szEmpty, -1);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild2, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild2, szEmpty, -1);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild3, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild3, szEmpty, -1);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild4, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild4, szEmpty, -1);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild5, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild5, szEmpty, -1);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild6, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild6, szEmpty, -1);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild7, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild7, szEmpty, -1);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild8, (int*) &dwMatch);
   if (dwMatch == dwNodeA)
      NodeValueSet (pNode, gszChild8, szEmpty, -1);

   gpData->NodeRelease(pNode);
}


/************************************************************
ClearAllRelationships - For a pNode of a person, clears all relationships
that the person has with others (in family)

inputs
   PCMMLNode   pNode - node for person
   DWORD       dwNodw - database node for pNode
*/
void ClearAllRelationships (PCMMLNode pNode, DWORD dwNode)
{
   HANGFUNCIN;
  // clear
   DWORD dwMatch;
   PWSTR psz;

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszSpouse, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszFather, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszMother, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild1, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild2, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);
   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild3, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild4, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild5, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild6, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild7, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);

   dwMatch = (DWORD)-1;
   psz = NodeValueGet (pNode, gszChild8, (int*) &dwMatch);
   if (dwMatch != (DWORD)-1)
      ClearRelationships (dwNode, dwMatch);
}


/************************************************************
ConnectRelationship - Connect the currently modified person
with his relations - the other way around

inputs
   DWORD    dwNodeA - The field in dwNodeB will be back-pointed to A
   PWSTR    pszNodeA - Name of node-A to backpoint
   DWORD    dwNodeB - As above
   DWORD    dwRelation - 1 to set dwNodeA as father, 2 to set dwNodeA as monther,
               3 to set dwNodeA as spouse, 4 to set dwNodeA as a child
returns
   none
*/
void ConnectRelationship (DWORD dwNodeA, PWSTR pszNodeA, DWORD dwNodeB, DWORD dwRelation)
{
   HANGFUNCIN;
   if (dwNodeB == (DWORD)-1)
      return;

   // load in nodeB
   // get the node B
   PCMMLNode   pNode;
   int   iNumber;
   pNode = gpData->NodeGet(dwNodeB);
   if (!pNode)
      return;

   switch (dwRelation) {
   case 1:  // father
      NodeValueSet (pNode, gszFather, pszNodeA, (int) dwNodeA);
      break;
   case 2:  // mother
      NodeValueSet (pNode, gszMother, pszNodeA, (int) dwNodeA);
      break;
   case 3:  // spouse
      NodeValueSet (pNode, gszSpouse, pszNodeA, (int) dwNodeA);
      break;
   case 4:  // a child
      // find the child
      iNumber = -1;
      NodeValueGet (pNode, gszChild1, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild1, pszNodeA, (int) dwNodeA);
         break;
      }
      iNumber = -1;
      NodeValueGet (pNode, gszChild2, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild2, pszNodeA, (int) dwNodeA);
         break;
      }
      iNumber = -1;
      NodeValueGet (pNode, gszChild3, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild3, pszNodeA, (int) dwNodeA);
         break;
      }

      iNumber = -1;
      NodeValueGet (pNode, gszChild4, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild4, pszNodeA, (int) dwNodeA);
         break;
      }

      iNumber = -1;
      NodeValueGet (pNode, gszChild5, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild5, pszNodeA, (int) dwNodeA);
         break;
      }

      iNumber = -1;
      NodeValueGet (pNode, gszChild6, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild6, pszNodeA, (int) dwNodeA);
         break;
      }

      iNumber = -1;
      NodeValueGet (pNode, gszChild7, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild7, pszNodeA, (int) dwNodeA);
         break;
      }

      iNumber = -1;
      NodeValueGet (pNode, gszChild8, &iNumber);
      if (iNumber == -1) {
         NodeValueSet (pNode, gszChild8, pszNodeA, (int) dwNodeA);
         break;
      }
      break;
   }

   gpData->NodeRelease(pNode);
}


/***********************************************************************
WritePersonControl - Gets the person filter-list contntents and and write it to a node

inputs
   PCMMLNode      pNode - Node to write in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
   DWORD    dwNodeA - The field in dwNodeB will be back-pointed to A.
               Used for back-connecting. if pszNodeA is null ignored.
   PWSTR    pszNodeA - Name of node-A to backpoint
               Used for back-connecting. If pszNodeA is null ignored.
               Else, checked to see if pszName == gszFather, gszMother, gszSpouse, gszChild1

returns
   BOOL - TRUE if success
*/
BOOL WritePersonControl (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName,
                         DWORD dwNodeA = (DWORD)-1, PWSTR pszNodeA = NULL)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   iCurSel = pControl->AttribGetInt (gszCurSel);

   // find the node number for the person's data and the name
   DWORD dwNode;
   dwNode = PeopleIndexToDatabase ((DWORD)iCurSel);
   PWSTR pszPersonName;
   pszPersonName = (PWSTR) glistPeople.Get((DWORD)iCurSel);
   if (!pszPersonName)
      pszPersonName = L"";

   // back-connect
   if (pszNodeA) {
      // get gender
      PWSTR pszGender;
      pszGender = NodeValueGet (pNode, gszGender);

      if ((pszName == gszFather) || (pszName == gszMother))
         ConnectRelationship (dwNodeA, pszNodeA, dwNode, 4);
      else if (pszName == gszSpouse)
         ConnectRelationship (dwNodeA, pszNodeA, dwNode, 3);
      else if ((pszName == gszChild1) || (pszName == gszChild2) || (pszName == gszChild3) || (pszName == gszChild4) ||
         (pszName == gszChild5) || (pszName == gszChild6) || (pszName == gszChild7) || (pszName == gszChild8)) {
         ConnectRelationship (dwNodeA, pszNodeA, dwNode, (pszGender && !_wcsicmp(pszGender,L"male")) ? 1 : 2);
      }

   }

   // write it
   return NodeValueSet (pNode, pszName, pszPersonName, (int) dwNode);
}


/***********************************************************************
BusinessFromPerson - Given a DWORD node for a person, this returns
a PCMMLNode (which must be released), which is the current business
they belong to.

inputs
   DWORD          dwPerson - person node
return
   PCMMLNode - Business. Must be reelased
*/
PCMMLNode BusinessFromPerson (DWORD dwPerson)
{
   HANGFUNCIN;
   PCMMLNode pPerson, pBusiness;
   pPerson = gpData->NodeGet (dwPerson);
   if (!pPerson)
      return NULL;
   DWORD dwBusiness;
   dwBusiness = -1;
   NodeValueGet (pPerson, gszBusiness, (int*) &dwBusiness);
   pBusiness = gpData->NodeGet (dwBusiness);
   gpData->NodeRelease (pPerson);
   return pBusiness;
}

/***********************************************************************
ClearLinkFromBusiness - Given a person, this opens up the business node
they link to, and removes their name from this list.

inputs
   DWORD          dwPerson - Person node
returns
   none
*/
void ClearLinkFromBusiness (DWORD dwPerson)
{
   HANGFUNCIN;
   PCMMLNode pBusiness;
   pBusiness = BusinessFromPerson (dwPerson);
   if (!pBusiness)
      return;

   NodeElemRemove (pBusiness, gszPerson, (int) dwPerson);

   gpData->NodeRelease (pBusiness);
   gpData->Flush();
}

/***********************************************************************
WriteBusinessControl - Gets the business filter-list contntents and and write it to a node

inputs
PWSTR             pszWhoLink - Person's full name
   DWORD          dwPerson - person node
   PCMMLNode      pNode - Node to write in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName

returns
   BOOL - TRUE if success
*/
BOOL WriteBusinessControl (PWSTR pszWhoLink, DWORD dwPerson, PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   iCurSel = pControl->AttribGetInt (gszCurSel);

   // find the node number for the person's data and the name
   DWORD dwNode;
   dwNode = BusinessIndexToDatabase ((DWORD)iCurSel);
   PWSTR pszPersonName;
   pszPersonName = (PWSTR) glistBusiness.Get((DWORD)iCurSel);
   if (!pszPersonName)
      pszPersonName = L"";

   // delete old business link
   PCMMLNode pBusiness;
   ClearLinkFromBusiness (dwPerson);// clear old link to business

   BOOL fRet;
   fRet = NodeValueSet (pNode, pszName, pszPersonName, (int) dwNode);

   // write new link
   pBusiness = BusinessFromPerson (dwPerson);
   if (pBusiness) {
      NodeElemSet (pBusiness, gszPerson, pszWhoLink, (int) dwPerson, TRUE);
      gpData->NodeRelease(pBusiness);
   }

   // write it
   return fRet;
}

/***********************************************************************
WritePersonBusinessControl - Gets the person filter-list contntents and and write it to a node

inputs
   PCMMLNode      pNode - Node to write in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName

returns
   BOOL - TRUE if success
*/
BOOL WritePersonBusinessControl (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   iCurSel = pControl->AttribGetInt (gszCurSel);

   // find the node number for the person's data and the name
   DWORD dwNode;
   dwNode = PeopleBusinessIndexToDatabase ((DWORD)iCurSel);
   PWSTR pszPersonName;
   pszPersonName = (PWSTR) glistPeopleBusiness.Get((DWORD)iCurSel);
   if (!pszPersonName)
      pszPersonName = L"";

   // write it
   return NodeValueSet (pNode, pszName, pszPersonName, (int) dwNode);
}

/***********************************************************************
WriteJournalControl - Gets the journal filter-list contntents and and write it to a node

inputs
   PCMMLNode      pNode - Node to write in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName

returns
   BOOL - TRUE if success
*/
BOOL WriteJournalControl (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   iCurSel = pControl->AttribGetInt (gszCurSel);

   // find the node number for the person's data and the name
   PCListVariable pl;
   pl = JournalListVariable();
   DWORD dwNode;
   dwNode = JournalIndexToDatabase ((DWORD)iCurSel);
   PWSTR pszPersonName;
   pszPersonName = (PWSTR) pl->Get((DWORD)iCurSel);
   if (!pszPersonName)
      pszPersonName = L"";

   // write it
   return NodeValueSet (pNode, pszName, pszPersonName, (int) dwNode);
}

/***********************************************************************
PersonFromControls - Look through all the controls in the add/edit
page and use that to fill in the info. (This rewrites the name
entry in the glistPeople and glistPeopleiD)

inputs
   PCEscPage   pPage - Page to get the info from
   DWORD       dwNode - person's index in the database
returns
   none
*/
void PersonFromControls (PCEscPage pPage, DWORD dwNode)
{
   HANGFUNCIN;
   // step one, get the name
   WCHAR szLast[128], szFirst[128], szNick[128];
   PCEscControl pControl;
   DWORD dwNeeded;
   szLast[0] = szFirst[0] = szNick[0] = 0;
   pControl = pPage->ControlFind(gszLastName);
   if (pControl)
      pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
   pControl = pPage->ControlFind(gszFirstName);
   if (pControl)
      pControl->AttribGet (gszText, szFirst, sizeof(szFirst), &dwNeeded);
   pControl = pPage->ControlFind(gszNickName);
   if (pControl)
      pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);
   WCHAR szFull[256];
   if (!PeopleFullName (szLast, szFirst, szNick, szFull, sizeof(szFull)))
      return;   // ignore

   // modify the list so the new name is used
   PCMMLNode   pNode;
   pNode = FindMajorSection (gszPeopleNode);
   if (!pNode)
      return;   // error. This shouldn't happen
   RenameFilteredList (dwNode, szFull, pNode, gszPersonNode, &glistPeople, &glistPeopleID);

   // BUGFIX - Also rename combined list
   RenameFilteredList (dwNode, szFull, pNode, gszPersonBusinessNode, &glistPeopleBusiness, &glistPeopleBusinessID);
   gpData->Flush();
   gpData->NodeRelease(pNode);

   // get the node for the person and start filling in info
   pNode = gpData->NodeGet (dwNode);
   if (!pNode)
      return;  // error

   // write out the name
   WriteEdit (pNode, gszLastName, pPage, gszLastName);
   WriteEdit (pNode, gszFirstName, pPage, gszFirstName);
   WriteEdit (pNode, gszNickName, pPage, gszNickName);

   // relationship
   WriteCombo (pNode, gszRelationship, pPage, gszRelationship);

   // gender
   WriteCombo (pNode, gszGender, pPage, gszGender);

   // home phone
   WriteEdit (pNode, gszHomePhone, pPage, gszHomePhone);

   // work phone
   WriteEdit (pNode, gszWorkPhone, pPage, gszWorkPhone);

   // mobile phone
   WriteEdit (pNode, gszMobilePhone, pPage, gszMobilePhone);

   // fax
   WriteEdit (pNode, gszFAXPhone, pPage, gszFAXPhone);

   // personal email
   WriteEdit (pNode, gszPersonalEmail, pPage, gszPersonalEmail);

   // business email
   WriteEdit (pNode, gszBusinessEMail, pPage, gszBusinessEmail);

   // personal web
   WriteEdit (pNode, gszPersonalWeb, pPage, gszPersonalWeb);

   // home address
   WriteEdit (pNode, gszHomeAddress, pPage, gszHomeAddress);

   // work address
   WriteEdit (pNode, gszWordAddress, pPage, gszWordAddress);

   // company
   WriteEdit (pNode, gszCompany, pPage, gszCompany);

   WriteEdit (pNode, gszJobTitle, pPage, gszJobTitle);
   WriteEdit (pNode, gszDepartment, pPage, gszDepartment);
   WriteEdit (pNode, gszOffice, pPage, gszOffice);

   // manager
   WritePersonControl (pNode, gszManager, pPage, gszManager);

   // assistant
   WritePersonControl (pNode, gszAssistant, pPage, gszAssistant);

   // wipe out all the relationships
   ClearAllRelationships (pNode, dwNode);

   // spouse
   WritePersonControl (pNode, gszSpouse, pPage, gszSpouse, dwNode, szFull);

   // 4 children
   WritePersonControl (pNode, gszChild1, pPage, gszChild1, dwNode, szFull);
   WritePersonControl (pNode, gszChild2, pPage, gszChild2, dwNode, szFull);
   WritePersonControl (pNode, gszChild3, pPage, gszChild3, dwNode, szFull);
   WritePersonControl (pNode, gszChild4, pPage, gszChild4, dwNode, szFull);
   WritePersonControl (pNode, gszChild5, pPage, gszChild5, dwNode, szFull);
   WritePersonControl (pNode, gszChild6, pPage, gszChild6, dwNode, szFull);
   WritePersonControl (pNode, gszChild7, pPage, gszChild7, dwNode, szFull);
   WritePersonControl (pNode, gszChild8, pPage, gszChild8, dwNode, szFull);

   // mother
   WritePersonControl (pNode, gszMother, pPage, gszMother, dwNode, szFull);

   // father
   WritePersonControl (pNode, gszFather, pPage, gszFather, dwNode, szFull);

   // birthday
   WriteDate (pNode, gszBirthday, pPage, gszBirthday);

   // remind of birthday
   pControl = pPage->ControlFind (gszRemindBDay);
   BOOL   iRemind;
   iRemind = FALSE;
   if (pControl)
      iRemind = pControl->AttribGetBOOL (gszChecked);
   NodeValueSet (pNode, gszRemindBDay, (int) iRemind);
   // if remind-bday change then turn on/off yearly task
   TaskBirthday (dwNode, szFirst, szLast, DateControlGet(pPage, gszBirthday), iRemind);

   // show birthday
   pControl = pPage->ControlFind (gszShowBDay);
   iRemind = FALSE;
   if (pControl)
      iRemind = pControl->AttribGetBOOL (gszChecked);
   NodeValueSet (pNode, gszShowBDay, (int) iRemind);
   EventBirthday (dwNode, szFirst, szLast, DateControlGet(pPage, gszBirthday), iRemind);


   // business
   WriteBusinessControl (szFull, dwNode, pNode, gszBusiness, pPage, gszBusiness);

   // whendied
   WriteDate (pNode, gszDeathDay, pPage, gszDeathDay);

   // journal
   WriteJournalControl (pNode, gszJournal, pPage, gszJournal);

   // misc notes
   WriteEdit (pNode, gszMiscNotes, pPage, gszMiscNotes);


   // release the node
   gpData->NodeRelease(pNode);

   // find the quick-add list and make sure this person is removed
   pNode = FindMajorSection (gszPeopleNode);
   if (pNode) {

      DWORD dwIndex;
      WCHAR szTemp[16];
      _itow ((int)dwNode, szTemp, 10);
      dwIndex = pNode->ContentFind (gszPersonQuickAdd, gszNumber, szTemp);

      if (dwIndex != (DWORD)-1)
         pNode->ContentRemove (dwIndex);

      gpData->NodeRelease (pNode);
   }

   gpData->Flush();
}


/***********************************************************************
BusinessFromControls - Look through all the controls in the add/edit
page and use that to fill in the info. (This rewrites the name
entry in the glistPeople and glistPeopleiD)

inputs
   PCEscPage   pPage - Page to get the info from
   DWORD       dwNode - person's index in the database
returns
   none
*/
void BusinessFromControls (PCEscPage pPage, DWORD dwNode)
{
   HANGFUNCIN;
   // step one, get the name
   WCHAR szLast[128], szNick[128];
   PCEscControl pControl;
   DWORD dwNeeded;
   szLast[0] = szNick[0] = 0;
   pControl = pPage->ControlFind(gszLastName);
   if (pControl)
      pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
   pControl = pPage->ControlFind(gszNickName);
   if (pControl)
      pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);
   WCHAR szFull[256];
   if (!PeopleFullName (szLast, L"", szNick, szFull, sizeof(szFull)))
      return;   // ignore

   // modify the list so the new name is used
   PCMMLNode   pNode;
   pNode = FindMajorSection (gszPeopleNode);
   if (!pNode)
      return;   // error. This shouldn't happen
   RenameFilteredList (dwNode, szFull, pNode, gszBusinessNode, &glistBusiness, &glistBusinessID);

   // BUGFIX - Also rename combined list
   RenameFilteredList (dwNode, szFull, pNode, gszPersonBusinessNode, &glistPeopleBusiness, &glistPeopleBusinessID);
   gpData->Flush();
   gpData->NodeRelease(pNode);

   // get the node for the person and start filling in info
   pNode = gpData->NodeGet (dwNode);
   if (!pNode)
      return;  // error

   // write out the name
   WriteEdit (pNode, gszLastName, pPage, gszLastName);
   WriteEdit (pNode, gszFirstName, pPage, gszFirstName);
   WriteEdit (pNode, gszNickName, pPage, gszNickName);

   // work phone
   WriteEdit (pNode, gszWorkPhone, pPage, gszWorkPhone);

   // fax
   WriteEdit (pNode, gszFAXPhone, pPage, gszFAXPhone);

   // business email
   WriteEdit (pNode, gszBusinessEMail, pPage, gszBusinessEmail);

   // personal web
   WriteEdit (pNode, gszPersonalWeb, pPage, gszPersonalWeb);

   // work address
   WriteEdit (pNode, gszWordAddress, pPage, gszWordAddress);

   // journal
   WriteJournalControl (pNode, gszJournal, pPage, gszJournal);

   // misc notes
   WriteEdit (pNode, gszMiscNotes, pPage, gszMiscNotes);

   // release the node
   gpData->NodeRelease(pNode);

   // find the quick-add list and make sure this person is removed
   pNode = FindMajorSection (gszPeopleNode);
   if (pNode) {

      DWORD dwIndex;
      WCHAR szTemp[16];
      _itow ((int)dwNode, szTemp, 10);
      dwIndex = pNode->ContentFind (gszPersonQuickAdd, gszNumber, szTemp);

      if (dwIndex != (DWORD)-1)
         pNode->ContentRemove (dwIndex);

      gpData->NodeRelease (pNode);
   }

   gpData->Flush();
}


/***********************************************************************
ReadCombo - Gets the combobox contents and Read it to a node

inputs
   PCMMLNode      pNode - Node to Read in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the combo resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL ReadCombo (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   PWSTR psz;
   psz = NodeValueGet (pNode, pszName);
   if (!psz)
      psz = L"";
   ESCMCOMBOBOXSELECTSTRING item;
   memset (&item, 0, sizeof(item));
   item.fExact = TRUE;
   item.dwIndex = (DWORD)-1;
   item.iStart = 0;
   item.psz = psz;
   pControl->Message (ESCM_COMBOBOXSELECTSTRING, &item);

   return TRUE;
}

/***********************************************************************
ReadEdit - Gets the edit contents and Read it to a node

inputs
   PCMMLNode      pNode - Node to Read in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL ReadEdit (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   PWSTR psz;
   psz = NodeValueGet (pNode, pszName);
   if (psz)
      pControl->AttribSet (gszText, psz);
   return TRUE;
}

/***********************************************************************
ReadPersonControl - Gets the person filter-list contntents and and Read it to a node

inputs
   PCMMLNode      pNode - Node to Read in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL ReadPersonControl (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   PWSTR psz;
   iCurSel = -1;
   psz = NodeValueGet (pNode, pszName, &iCurSel);
   DWORD dwNode;
   dwNode = PeopleDatabaseToIndex ((DWORD)iCurSel);
   if (dwNode != (DWORD)-1)
      pControl->AttribSetInt (gszCurSel, (int) dwNode);

   return TRUE;
}

/***********************************************************************
ReadBusinessControl - Gets the Business filter-list contntents and and Read it to a node

inputs
   PCMMLNode      pNode - Node to Read in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL ReadBusinessControl (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   PWSTR psz;
   iCurSel = -1;
   psz = NodeValueGet (pNode, pszName, &iCurSel);
   DWORD dwNode;
   dwNode = BusinessDatabaseToIndex ((DWORD)iCurSel);
   if (dwNode != (DWORD)-1)
      pControl->AttribSetInt (gszCurSel, (int) dwNode);

   return TRUE;
}

/***********************************************************************
ReadJournalControl - Gets the journal filter-list contntents and and Read it to a node

inputs
   PCMMLNode      pNode - Node to Read in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL ReadJournalControl (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   int   iCurSel;
   PWSTR psz;
   iCurSel = -1;
   psz = NodeValueGet (pNode, pszName, &iCurSel);
   DWORD dwNode;
   dwNode = JournalDatabaseToIndex ((DWORD)iCurSel);
   if (dwNode != (DWORD)-1)
      pControl->AttribSetInt (gszCurSel, (int) dwNode);

   return TRUE;
}

/***********************************************************************
ReadDate - Read a date to a node

inputs
   PCMMLNode      pNode - Node to Read in.
   PWSTR          pszName - Sub-node to create in pNode
   PCEscPage      pPage - Page where the control resides
   PWSTR          pszControlName - name of the control to get the value
                     for. This is written under pszName
returns
   BOOL - TRUE if success
*/
BOOL ReadDate (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControlName);
   if (!pControl)
      return FALSE;

   // get the values
   DFDATE   date;
   date = (DFDATE) NodeValueGetInt (pNode, pszName, 0);
   DateControlSet (pPage, pszControlName, date);

   return TRUE;
}

/***********************************************************************
PersonToControls - Get all the information from a person node
and fill in the edit information.

inputs
   PCEscPage   pPage - Page to get the info from
   DWORD       dwNode - person's index in the database
returns
   none
*/
void PersonToControls (PCEscPage pPage, DWORD dwNode)
{
   HANGFUNCIN;
   // get the node for the person and start filling in info
   PCMMLNode pNode;
   pNode = gpData->NodeGet (dwNode);
   if (!pNode)
      return;  // error

   // read out the name
   ReadEdit (pNode, gszLastName, pPage, gszLastName);
   ReadEdit (pNode, gszFirstName, pPage, gszFirstName);
   ReadEdit (pNode, gszNickName, pPage, gszNickName);

   // relationship
   ReadCombo (pNode, gszRelationship, pPage, gszRelationship);

   // gender
   ReadCombo (pNode, gszGender, pPage, gszGender);

   // home phone
   ReadEdit (pNode, gszHomePhone, pPage, gszHomePhone);

   // work phone
   ReadEdit (pNode, gszWorkPhone, pPage, gszWorkPhone);

   // mobile phone
   ReadEdit (pNode, gszMobilePhone, pPage, gszMobilePhone);

   // fax
   ReadEdit (pNode, gszFAXPhone, pPage, gszFAXPhone);

   // personal email
   ReadEdit (pNode, gszPersonalEmail, pPage, gszPersonalEmail);

   // business email
   ReadEdit (pNode, gszBusinessEMail, pPage, gszBusinessEmail);

   // personal web
   ReadEdit (pNode, gszPersonalWeb, pPage, gszPersonalWeb);

   // home address
   ReadEdit (pNode, gszHomeAddress, pPage, gszHomeAddress);

   // work address
   ReadEdit (pNode, gszWordAddress, pPage, gszWordAddress);

   ReadEdit (pNode, gszJobTitle, pPage, gszJobTitle);
   ReadEdit (pNode, gszDepartment, pPage, gszDepartment);
   ReadEdit (pNode, gszOffice, pPage, gszOffice);

   // company
   ReadEdit (pNode, gszCompany, pPage, gszCompany);
   ReadBusinessControl (pNode, gszBusiness, pPage, gszBusiness);

   // manager
   ReadPersonControl (pNode, gszManager, pPage, gszManager);

   // assistant
   ReadPersonControl (pNode, gszAssistant, pPage, gszAssistant);

   // spouse
   ReadPersonControl (pNode, gszSpouse, pPage, gszSpouse);

   // 4 children
   ReadPersonControl (pNode, gszChild1, pPage, gszChild1);
   ReadPersonControl (pNode, gszChild2, pPage, gszChild2);
   ReadPersonControl (pNode, gszChild3, pPage, gszChild3);
   ReadPersonControl (pNode, gszChild4, pPage, gszChild4);
   ReadPersonControl (pNode, gszChild5, pPage, gszChild5);
   ReadPersonControl (pNode, gszChild6, pPage, gszChild6);
   ReadPersonControl (pNode, gszChild7, pPage, gszChild7);
   ReadPersonControl (pNode, gszChild8, pPage, gszChild8);

   // mother
   ReadPersonControl (pNode, gszMother, pPage, gszMother);

   // father
   ReadPersonControl (pNode, gszFather, pPage, gszFather);

   // birthday
   ReadDate (pNode, gszBirthday, pPage, gszBirthday);

   // remind of birthday
   PCEscControl pControl;
   pControl = pPage->ControlFind (gszRemindBDay);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, NodeValueGetInt (pNode, gszRemindBDay, 0));
   pControl = pPage->ControlFind (gszShowBDay);
   if (pControl)
      pControl->AttribSetBOOL (gszChecked, NodeValueGetInt (pNode, gszShowBDay, 0));

   // whendied
   ReadDate (pNode, gszDeathDay, pPage, gszDeathDay);

   // manager
   ReadJournalControl (pNode, gszJournal, pPage, gszJournal);

   // misc notes
   ReadEdit (pNode, gszMiscNotes, pPage, gszMiscNotes);


   // release the node
   gpData->NodeRelease(pNode);
}


/***********************************************************************
BusinessToControls - Get all the information from a person node
and fill in the edit information.

inputs
   PCEscPage   pPage - Page to get the info from
   DWORD       dwNode - person's index in the database
returns
   none
*/
void BusinessToControls (PCEscPage pPage, DWORD dwNode)
{
   HANGFUNCIN;
   // get the node for the person and start filling in info
   PCMMLNode pNode;
   pNode = gpData->NodeGet (dwNode);
   if (!pNode)
      return;  // error

   // read out the name
   ReadEdit (pNode, gszLastName, pPage, gszLastName);
   ReadEdit (pNode, gszNickName, pPage, gszNickName);

   // work phone
   ReadEdit (pNode, gszWorkPhone, pPage, gszWorkPhone);

   // fax
   ReadEdit (pNode, gszFAXPhone, pPage, gszFAXPhone);

   // business email
   ReadEdit (pNode, gszBusinessEMail, pPage, gszBusinessEmail);

   // personal web
   ReadEdit (pNode, gszPersonalWeb, pPage, gszPersonalWeb);

   // work address
   ReadEdit (pNode, gszWordAddress, pPage, gszWordAddress);

   // misc notes
   ReadEdit (pNode, gszMiscNotes, pPage, gszMiscNotes);

   // manager
   ReadJournalControl (pNode, gszJournal, pPage, gszJournal);


   // release the node
   gpData->NodeRelease(pNode);
}

/***********************************************************************
AddPerson - Adds a new user. Used by PeopleNewPage and QuickAdd.
Also checks that the name is unique. Fills in glistPeople and glistPeopleID.

inputs
   PCEscPage   pPage - To pull up error messages from
   PWSTR       pszFirst - first name
   PWSTR       pszLast - last name
   PWSTR       pszNick - nick name
returns
   DWORD - new element number. -1 if error
*/
DWORD AddPerson (PCEscPage pPage, PWSTR pszFirst, PWSTR pszLast, PWSTR pszNick)
{
   HANGFUNCIN;
   // make sure it's not a duplicate
   WCHAR szFull[256];
   if (!PeopleFullName (pszLast, pszFirst, pszNick, szFull, sizeof(szFull)))
      return TRUE;   // ignore
   if (FilteredStringToIndex(szFull, &glistPeopleBusiness, &glistPeopleBusinessID) != (DWORD)-1) {
      if (pPage)
         pPage->MBWarning (L"That name is already in your address book.",
            L"You can't add another person/business with the exact same name.");
      return TRUE;   // ignore
   }

   // create new database entry
   DWORD dwNode;
   PCMMLNode   pNode;
   pNode = gpData->NodeAdd (gszPersonNode, &dwNode);
   if (!pNode)
      return (DWORD)-1;

   // fill  in the name
   if (pszLast)
      NodeValueSet (pNode, gszLastName, pszLast);
   if (pszFirst)
      NodeValueSet (pNode, gszFirstName, pszFirst);
   if (pszNick)
      NodeValueSet (pNode, gszNickName, pszNick);

   // all done
   gpData->NodeRelease(pNode);

   // add them to the list
   // else get it
   pNode = FindMajorSection (gszPeopleNode);
   if (!pNode)
      return NULL;   // error. This shouldn't happen

   AddFilteredList (szFull, dwNode, pNode, gszPersonNode, &glistPeople, &glistPeopleID);
   // BUGFIX - Add to combined people/business node
   AddFilteredList (szFull, dwNode, pNode, gszPersonBusinessNode, &glistPeopleBusiness, &glistPeopleBusinessID);

   // add to log
   WCHAR szHuge[10000];
   wcscpy (szHuge, L"Add person to address book: ");
   wcscat (szHuge, szFull);
   CalendarLogAdd (Today(), Now(), -1, szHuge, dwNode);

   // finall, flush release
   gpData->Flush();
   gpData->NodeRelease(pNode);

   return dwNode;
}

/***********************************************************************
AddBusiness - Adds a new business.

inputs
   PCEscPage   pPage - To pull up error messages from
   PWSTR       pszLast - Business name
   PWSTR       pszNick - nick name
returns
   DWORD - new element number. -1 if error
*/
DWORD AddBusiness (PCEscPage pPage, PWSTR pszLast, PWSTR pszNick)
{
   HANGFUNCIN;
   // make sure it's not a duplicate
   if (FilteredStringToIndex(pszLast, &glistPeopleBusiness, &glistPeopleBusinessID) != (DWORD)-1) {
      if (pPage)
         pPage->MBWarning (L"That name is already in your address book.",
            L"You can't add another person/business with the exact same name.");
      return TRUE;   // ignore
   }

   WCHAR szFull[256];
   if (!PeopleFullName (pszLast, L"", pszNick, szFull, sizeof(szFull)))
      return TRUE;   // ignore

   // create new database entry
   DWORD dwNode;
   PCMMLNode   pNode;
   pNode = gpData->NodeAdd (gszBusinessNode, &dwNode);
   if (!pNode)
      return (DWORD)-1;

   // fill  in the name
   if (pszLast)
      NodeValueSet (pNode, gszLastName, pszLast);
   if (pszNick)
      NodeValueSet (pNode, gszNickName, pszNick);

   // all done
   gpData->NodeRelease(pNode);

   // add them to the list
   // else get it
   pNode = FindMajorSection (gszPeopleNode);
   if (!pNode)
      return NULL;   // error. This shouldn't happen

   AddFilteredList (szFull, dwNode, pNode, gszBusinessNode, &glistBusiness, &glistBusinessID);
   // BUGFIX - Add to combined people/business node
   AddFilteredList (szFull, dwNode, pNode, gszPersonBusinessNode, &glistPeopleBusiness, &glistPeopleBusinessID);

   // add to log
   WCHAR szHuge[10000];
   wcscpy (szHuge, L"Add business to address book: ");
   wcscat (szHuge, pszLast);
   CalendarLogAdd (Today(), Now(), -1, szHuge, dwNode);

   // finall, flush release
   gpData->Flush();
   gpData->NodeRelease(pNode);

   return dwNode;
}

/***********************************************************************
PeopleNewPage - Page callback for new user (not quick-add though)
*/
BOOL PeopleNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // just make sure have loaded in people info
      PeopleFilteredList();
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it's back or starts with r: then ask if they're sure wnat to go
         if (!_wcsicmp(p->psz, gszBack) || ((p->psz[0] == L'r') && (p->psz[1] == L':'))) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to leave this page?",
               L"You will lose all the information you've typed in if you leave."))
               return TRUE;

            // else continue
            break;
         }


         break;
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // first of all, verify name and verify it doesn't already exist
            WCHAR szLast[128], szFirst[128], szNick[128];
            PCEscControl pControl;
            DWORD dwNeeded;
            szLast[0] = szFirst[0] = szNick[0] = 0;
            pControl = pPage->ControlFind(gszLastName);
            if (pControl)
               pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
            pControl = pPage->ControlFind(gszFirstName);
            if (pControl)
               pControl->AttribGet (gszText, szFirst, sizeof(szFirst), &dwNeeded);
            pControl = pPage->ControlFind(gszNickName);
            if (pControl)
               pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);

            // if it's empty then error
            if (!szLast[0] && !szFirst[0]) {
               pPage->MBWarning (L"You must type in a name.",
                  L"Either the first or last name must be filled in.");
               return TRUE;   // ignore
            }

            // add
            DWORD dwNode;
            dwNode = AddPerson (pPage, szFirst, szLast, szNick);
            if (dwNode == (DWORD)-1)
               return TRUE;

            // add
            PersonFromControls (pPage, dwNode);

            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"v:%d", (int) dwNode);
            pPage->Link (szTemp);
            return TRUE;
         }


         break;
      }
   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
BusinessNewPage - Page callback for new user (not quick-add though)
*/
BOOL BusinessNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // just make sure have loaded in people info
      BusinessFilteredList();
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it's back or starts with r: then ask if they're sure wnat to go
         if (!_wcsicmp(p->psz, gszBack) || ((p->psz[0] == L'r') && (p->psz[1] == L':'))) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to leave this page?",
               L"You will lose all the information you've typed in if you leave."))
               return TRUE;

            // else continue
            break;
         }


         break;
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // first of all, verify name and verify it doesn't already exist
            WCHAR szLast[128], szNick[128];
            PCEscControl pControl;
            DWORD dwNeeded;
            szLast[0] = szNick[0] = 0;
            pControl = pPage->ControlFind(gszLastName);
            if (pControl)
               pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
            pControl = pPage->ControlFind(gszNickName);
            if (pControl)
               pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);

            // if it's empty then error
            if (!szLast[0]) {
               pPage->MBWarning (L"You must type in a name.");
               return TRUE;   // ignore
            }

            // add
            DWORD dwNode;
            dwNode = AddBusiness (pPage, szLast, szNick);
            if (dwNode == (DWORD)-1)
               return TRUE;

            // add
            BusinessFromControls (pPage, dwNode);

            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"v:%d", (int) dwNode);
            pPage->Link (szTemp);
            return TRUE;
         }


         break;
      }
   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PeopleLookUpPage - Page callback
*/
BOOL PeopleLookUpPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_FILTEREDLISTCHANGE:
      {
         PESCNFILTEREDLISTCHANGE  p = (PESCNFILTEREDLISTCHANGE) pParam;

         if (p->iCurSel < 0)
            break;   // default

         // else handle
         DWORD dwNode;
         dwNode = PeopleBusinessIndexToDatabase ((DWORD)p->iCurSel);
         if (dwNode == (DWORD)-1)
            return TRUE;

         // new link
         WCHAR szTemp[16];
         swprintf (szTemp, L"v:%d", (int) dwNode);
         pPage->Link (szTemp);
         return TRUE;
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PhoneDialLink - Adds a phone dial link to gMemTemp. THis ends up being
   <a href="call:###:pszstring">pszString<hoverhelp>blahblah</hoverhelp></a>

inputs
   PWSTR    pszString - Phone number string
returns
   none
*/
void PhoneDialLink (PWSTR pszString)
{
   HANGFUNCIN;
   MemCat (&gMemTemp, L"<a href=\"call:");
   MemCat (&gMemTemp, (int) gdwNode);
   MemCat (&gMemTemp, L":");
   MemCatSanitize (&gMemTemp, pszString);
   MemCat (&gMemTemp, L"\">");
   MemCatSanitize (&gMemTemp, pszString);
   MemCat (&gMemTemp, L"<xhoverhelp>Click on this to have your computer dial the phone number.</xhoverhelp>");
   MemCat (&gMemTemp, L"</a>");
}

/***********************************************************************
PersonLink - Adds a view-person link to gMemTemp. THis ends up being
   <a href="v:###">pszString<hoverhelp>blahblah</hoverhelp></a>

inputs
   PWSTR    pszString - Phone number string
   DWORD    dwNode - Node for the person
returns
   none
*/
void PersonLink (PWSTR pszString, DWORD dwNode)
{
   HANGFUNCIN;
   MemCat (&gMemTemp, L"<a href=v:");
   MemCat (&gMemTemp, (int) dwNode);
   MemCat (&gMemTemp, L">");
   MemCatSanitize (&gMemTemp, pszString);
   MemCat (&gMemTemp, L"</a>");
}

/***********************************************************************
CopyLink - Adds a link to gMemTemp that causes the text to be copied to the clipboard.
   THis ends up being
   <a href="copy:pszstring">pszString<hoverhelp>blahblah</hoverhelp></a>

inputs
   PWSTR    pszString - Phone number string
returns
   none
*/
void CopyLink (PWSTR pszString)
{
   HANGFUNCIN;
   MemCat (&gMemTemp, L"<a href=\"copy:");
   MemCatSanitize (&gMemTemp, pszString);
   MemCat (&gMemTemp, L"\">");
   MemCatSanitize (&gMemTemp, pszString);
   MemCat (&gMemTemp, L"<xhoverhelp>Click on this to copy the text to the clipboard so you can paste it into another application.</xhoverhelp>");
   MemCat (&gMemTemp, L"</a>");
}

/***********************************************************************
URLLink - Adds a link to gMemTemp that is a URL or E-mail name.
   THis ends up being
   <a href="http://pszstring">pszString<hoverhelp>blahblah</hoverhelp></a>

inputs
   PWSTR    pszString - Phone number string
   BOOL     fEmail - if TRUE it's an Email, else it's a URL
returns
   none
*/
void URLLink (PWSTR pszString, BOOL fEmail)
{
   HANGFUNCIN;
   WCHAR szHTTPSlash[] = L"http://";

   MemCat (&gMemTemp, L"<a href=\"");

   // make sure it's prepended with the right stuff
   if (fEmail) {
      MemCat (&gMemTemp, L"mailto:");
   }
   else {
      if (!_wcsnicmp (pszString, szHTTPSlash, wcslen(szHTTPSlash))) {
         // do nothing
      }
      else {
         MemCat (&gMemTemp, szHTTPSlash);
      }
   }

   MemCatSanitize (&gMemTemp, pszString);
   MemCat (&gMemTemp, L"\">");

   MemCatSanitize (&gMemTemp, pszString);
   MemCat (&gMemTemp, L"<xhoverhelpshort>Click on this to ");
   MemCat (&gMemTemp, fEmail ? L"send Email" : L"follow the link");
   MemCat (&gMemTemp, L".</xhoverhelpshort>");
   MemCat (&gMemTemp, L"</a>");
}

/***********************************************************************
TableIfPhone - Add a table entry if the phone number is specified.
Clicking the number will cause it to dial.

inputs
   PCMMLNode   pNode - Person node
   PWSTR       pszEntry - entry name under the node
   PWSTR       pszDescription - deccription used if node found
returns
   BOOL - TRUE if found
*/
BOOL TableIfPhone (PCMMLNode pNode, PWSTR pszEntry, PWSTR pszDescription)
{
   HANGFUNCIN;
   // find it
   PWSTR psz;
   psz = NodeValueGet (pNode, pszEntry);
   if (!psz || !psz[0])
      return FALSE;

   // else add table
   MemCat (&gMemTemp, L"<tr><xtdleft valign=top>");
   MemCatSanitize (&gMemTemp, pszDescription);
   MemCat (&gMemTemp, L"</xtdleft><xtdright valign=top>");
   PhoneDialLink (psz);
   MemCat (&gMemTemp, L"</xtdright></tr>");

   return TRUE;
}


/***********************************************************************
TableIfCopy - Add a table entry if the copy-text is specified.
Clicking the number will cause it to be copied to the clipboard.

inputs
   PCMMLNode   pNode - Person node
   PWSTR       pszEntry - entry name under the node
   PWSTR       pszDescription - deccription used if node found
returns
   BOOL - TRUE if found
*/
BOOL TableIfCopy (PCMMLNode pNode, PWSTR pszEntry, PWSTR pszDescription)
{
   HANGFUNCIN;
   // find it
   PWSTR psz;
   psz = NodeValueGet (pNode, pszEntry);
   if (!psz || !psz[0])
      return FALSE;

   // else add table
   MemCat (&gMemTemp, L"<tr><xtdleft valign=top>");
   MemCatSanitize (&gMemTemp, pszDescription);
   MemCat (&gMemTemp, L"</xtdleft><xtdright valign=top><align parlinespacing=0>");
   CopyLink (psz);
   MemCat (&gMemTemp, L"</align></xtdright></tr>");

   return TRUE;
}

/***********************************************************************
TableIfURL - Add a table entry if the URL is specified.
Clicking the URL will connect to the internet.

inputs
   PCMMLNode   pNode - Person node
   PWSTR       pszEntry - entry name under the node
   PWSTR       pszDescription - deccription used if node found
   BOOL        fEmail - if TRUE is Email
returns
   BOOL - TRUE if found
*/
BOOL TableIfURL (PCMMLNode pNode, PWSTR pszEntry, PWSTR pszDescription, BOOL fEmail)
{
   HANGFUNCIN;
   // find it
   PWSTR psz;
   psz = NodeValueGet (pNode, pszEntry);
   if (!psz || !psz[0])
      return FALSE;

   // else add table
   MemCat (&gMemTemp, L"<tr><xtdleft valign=top>");
   MemCatSanitize (&gMemTemp, pszDescription);
   MemCat (&gMemTemp, L"</xtdleft><xtdright valign=top><align parlinespacing=0>");
   URLLink (psz, fEmail);
   MemCat (&gMemTemp, L"</align></xtdright></tr>");

   return TRUE;
}

/***********************************************************************
TableIfPerson - Add a table entry if the person is specified.
Clicking the person will view info about them.

inputs
   PCMMLNode   pNode - Person node
   PWSTR       pszEntry - entry name under the node
   PWSTR       pszDescription - deccription used if node found
returns
   BOOL - TRUE if found
*/
BOOL TableIfPerson (PCMMLNode pNode, PWSTR pszEntry, PWSTR pszDescription)
{
   HANGFUNCIN;
   // find it
   PWSTR psz;
   int   iNumber;
   psz = NodeValueGet (pNode, pszEntry, &iNumber);
   if (!psz || !psz[0])
      return FALSE;

   // else add table
   MemCat (&gMemTemp, L"<tr><xtdleft valign=top>");
   MemCatSanitize (&gMemTemp, pszDescription);
   MemCat (&gMemTemp, L"</xtdleft><xtdright valign=top>");
   PersonLink (psz, iNumber);
   MemCat (&gMemTemp, L"</xtdright></tr>");

   return TRUE;
}

/***********************************************************************
TableIfDate - Add a table entry if the date is specified.

inputs
   PCMMLNode   pNode - Person node
   PWSTR       pszEntry - entry name under the node
   PWSTR       pszDescription - deccription used if node found
returns
   BOOL - TRUE if found
*/
BOOL TableIfDate (PCMMLNode pNode, PWSTR pszEntry, PWSTR pszDescription)
{
   HANGFUNCIN;
   // find it
   int   iNumber;
   iNumber = NodeValueGetInt (pNode, pszEntry, 0);
   if (!iNumber)
      return FALSE;

   // else add table
   MemCat (&gMemTemp, L"<tr><xtdleft valign=top>");
   MemCatSanitize (&gMemTemp, pszDescription);
   MemCat (&gMemTemp, L"</xtdleft><xtdright valign=top>");
   WCHAR szTemp[128];
   DFDATEToString ((DFDATE)iNumber, szTemp);
   MemCat (&gMemTemp, szTemp);
   MemCat (&gMemTemp, L"</xtdright></tr>");

   return TRUE;
}

/***********************************************************************
PeoplePersonViewPage - Page callback for viewing an existing user.
*/
BOOL PeoplePersonViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it starts with copy:
         WCHAR szCopy[] = L"copy:";
         DWORD dwLenCopy = wcslen(szCopy);
         if (!_wcsnicmp(p->psz, szCopy, dwLenCopy)) {
            // convert from unicode to ANSI
            CMem  mem;
            int   iLen;
            if (!mem.Required ((dwLenCopy + 1) * 2))
               return TRUE;
            iLen = WideCharToMultiByte (CP_ACP, 0,
               p->psz + dwLenCopy, -1,
               (char*) mem.p, mem.m_dwAllocated, 0, 0);
            ((char*) mem.p)[iLen] = 0; // null terminate

            // fill hmem
            HANDLE   hMem;
            hMem = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, iLen+1);
            if (!hMem)
               return TRUE;
            strcpy ((char*) GlobalLock(hMem), (char*) mem.p);
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

         WCHAR szCall[] = L"call:";
         DWORD dwLenCall = wcslen(szCall);
         if (!_wcsnicmp(p->psz, szCall, dwLenCall)) {
            PWSTR psz = p->psz + dwLenCall;

            DWORD dwPerson = _wtoi (psz);

            // find next color
            psz = wcschr (psz, L':');
            if (!psz)
               return FALSE;
            psz++;

            DWORD dwNode;
            dwNode = PhoneCall (pPage, TRUE, TRUE, dwPerson, psz);
            if (dwNode == (DWORD)-1)
               return TRUE;   // error

            // exit this, editing the new node
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }

         break;
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         
         if (!_wcsicmp(p->pControl->m_pszName, L"edit")) {
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove this person from your address book?"))
               return TRUE;

            // else remove
            PCMMLNode   pNode;
            pNode = FindMajorSection (gszPeopleNode);
            if (!pNode)
               return TRUE;   // error. This shouldn't happen

            // fill the list in
            RemoveFilteredList (gdwNode, pNode, gszPersonNode,&glistPeople, &glistPeopleID);
            // BUGFIX - Remove from combined list
            RemoveFilteredList (gdwNode, pNode, gszPersonBusinessNode,&glistPeopleBusiness, &glistPeopleBusinessID);
            // BUGBUG - De-link from business? Not now, but maybe later

            // finall, flush release
            gpData->Flush();
            gpData->NodeRelease(pNode);


            // remove
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Removed from address book.");

            pPage->Link (L"r:118");
            return TRUE;
         }

      }
      break;   // default behavior

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         // only care about projectlist
         if (!_wcsicmp(p->pszSubName, L"PERSONNAME")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            // get the name
            PWSTR pszFirst, pszLast, pszNick;
            WCHAR szTemp[256];
            pszFirst = NodeValueGet (pNode, gszFirstName);
            pszLast = NodeValueGet (pNode, gszLastName);
            pszNick = NodeValueGet (pNode, gszNickName);
            szTemp[0] = 0;

            PeopleFullName (pszLast ? pszLast : L"", pszFirst ? pszFirst : L"",
               pszNick ? pszNick : L"", szTemp, sizeof(szTemp));
            MemCat (&gMemTemp, szTemp);

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         // gender
         if (!_wcsicmp(p->pszSubName, gszGender)) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            // get the name
            PWSTR pszGender;
            pszGender = NodeValueGet (pNode, gszGender);
            if (pszGender)
               MemCat (&gMemTemp, pszGender);

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         // relationship
         if (!_wcsicmp(p->pszSubName, gszRelationship)) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            // get the name
            PWSTR pszGender;
            pszGender = NodeValueGet (pNode, gszRelationship);
            if (pszGender)
               MemCat (&gMemTemp, pszGender);

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"CONTACTINFO")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            DWORD dwSum = 0;
            PCMMLNode pBusiness;
            pBusiness = BusinessFromPerson (gdwNode);

            // phone numbers that can dial
            if (TableIfPhone(pNode, gszHomePhone, L"Home phone number:"))
               dwSum++;
            if (TableIfPhone(pNode, gszWorkPhone, L"Work phone number:"))
               dwSum++;
            if (TableIfPhone(pNode, gszMobilePhone, L"Mobile phone number:"))
               dwSum++;
            if (pBusiness && TableIfPhone(pBusiness, gszWorkPhone, L"Company's phone number:"))
               dwSum++;

            // FAX phone number
            if (TableIfCopy(pNode, gszFAXPhone, L"FAX phone number:"))
               dwSum++;
            if (pBusiness && TableIfCopy(pBusiness, gszFAXPhone, L"Company's FAX phone number:"))
               dwSum++;

            // Emails and web site
            if (TableIfURL (pNode, gszPersonalEmail, L"Personal E-mail:", TRUE))
               dwSum++;
            if (TableIfURL (pNode, gszBusinessEmail, L"Work E-mail:", TRUE))
               dwSum++;
            if (pBusiness && TableIfURL (pBusiness, gszBusinessEmail, L"Company's E-mail:", TRUE))
               dwSum++;
            if (TableIfURL (pNode, gszPersonalWeb, L"Personal web site:", FALSE))
               dwSum++;
            if (pBusiness && TableIfURL (pBusiness, gszPersonalWeb, L"Company's web site:", FALSE))
               dwSum++;

            // address
            if (TableIfCopy(pNode, gszHomeAddress, L"Home address:"))
               dwSum++;
            if (TableIfCopy(pNode, gszWordAddress, L"Work address:"))
               dwSum++;
            else if (pBusiness && TableIfCopy(pBusiness, gszWordAddress, L"Work address:"))
               dwSum++;

            if (TableIfPerson (pNode, gszJournal, L"Link to journal category:"))
               dwSum++;

            // if nothing added then tell user
            if (!dwSum)
               MemCat (&gMemTemp, L"<tr><td>No contact information specified.</td></tr>");

            gpData->NodeRelease (pNode);
            if (pBusiness)
               gpData->NodeRelease (pBusiness);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"BUSINESSINFO")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            DWORD dwSum = 0;

            // business name
            if (TableIfCopy(pNode, gszCompany, L"Company:"))
               dwSum++;

            // business
            if (TableIfPerson (pNode, gszBusiness, L"Company link:"))
               dwSum++;

            if (TableIfCopy(pNode, gszJobTitle, L"Job title:"))
               dwSum++;
            if (TableIfCopy(pNode, gszDepartment, L"Department:"))
               dwSum++;
            if (TableIfCopy(pNode, gszOffice, L"Office:"))
               dwSum++;

            // contacts in business
            if (TableIfPerson (pNode, gszManager, L"Manager:"))
               dwSum++;
            if (TableIfPerson (pNode, gszAssistant, L"Assistant:"))
               dwSum++;

            // if nothing added then tell user
            if (!dwSum)
               MemCat (&gMemTemp, L"<tr><td>No business information specified.</td></tr>");

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"PERSONALINFO")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            DWORD dwSum = 0;

            // relations
            if (TableIfPerson (pNode, gszSpouse, L"Spouse"))
               dwSum++;
            DWORD dwChild = 0;
            WCHAR szChildren[] = L"Children:";
            WCHAR szBlank[] = L"";
            if (TableIfPerson (pNode, gszChild1, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszChild2, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszChild3, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszChild4, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszChild5, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszChild6, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszChild7, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszChild8, dwChild ? szBlank : szChildren)) {
               dwSum++;
               dwChild++;
            }
            if (TableIfPerson (pNode, gszMother, L"Mother:"))
               dwSum++;
            if (TableIfPerson (pNode, gszFather, L"Father:"))
               dwSum++;

            // birtday and death day
            if (TableIfDate (pNode, gszBirthday, L"Birthday:"))
               dwSum++;
            if (TableIfDate (pNode, gszDeathDay, L"Died:"))
               dwSum++;

            // if nothing added then tell user
            if (!dwSum)
               MemCat (&gMemTemp, L"<tr><td>No personal information specified.</td></tr>");

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         if (!_wcsicmp(p->pszSubName, L"MISCINFO")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            // see if can get the misc text
            PWSTR psz;
            psz = NodeValueGet (pNode, gszMiscNotes);

            if (psz && psz[0]) {
               //MemCat (&gMemTemp, L"<xSectionTitle>Miscellaneous</xSectionTitle>");
               MemCat (&gMemTemp, L"<xTableCenter innerlines=0><xtheader>Miscellaneous</xtheader><tr><td>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</td></tr></xTableCenter>");
            }

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"LOG")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszInteraction, FALSE);

            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask><xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString (pnlg->start, szTemp);
               MemCat (&gMemTemp, szTemp);
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // if no entries then say so
            if (!pl || !pl->Num())
               MemCat (&gMemTemp, L"<tr><td>You haven't had any meetings or phone conversations with the person yet.</td></tr>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"PHOTOS")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszPhotos, FALSE);

            if (!pl || !pl->Num()) {
               if (pl)
                  delete pl;
               if (pNode)
                  gpData->NodeRelease(pNode);
               p->pszSubString = (PWSTR) gMemTemp.p;
               return TRUE;
            }

            // header
            MemCat (&gMemTemp, L"<xTableCenter><xtheader>Photos</xtheader>");
            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask><xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // footer
            MemCat (&gMemTemp, L"</xtablecenter>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"MEMORIES")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszMemory, FALSE);

            if (!pl || !pl->Num()) {
               if (pl)
                  delete pl;
               if (pNode)
                  gpData->NodeRelease(pNode);
               p->pszSubString = (PWSTR) gMemTemp.p;
               return TRUE;
            }

            // header
            MemCat (&gMemTemp, L"<xTableCenter><xtheader>Memories</xtheader>");
            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask><xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // footer
            MemCat (&gMemTemp, L"</xtablecenter>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PeopleBusinessViewPage - Page callback for viewing an existing user.
*/
BOOL PeopleBusinessViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it starts with copy:
         WCHAR szCopy[] = L"copy:";
         DWORD dwLenCopy = wcslen(szCopy);
         if (!_wcsnicmp(p->psz, szCopy, dwLenCopy)) {
            // convert from unicode to ANSI
            CMem  mem;
            int   iLen;
            if (!mem.Required ((dwLenCopy + 1) * 2))
               return TRUE;
            iLen = WideCharToMultiByte (CP_ACP, 0,
               p->psz + dwLenCopy, -1,
               (char*) mem.p, mem.m_dwAllocated, 0, 0);
            ((char*) mem.p)[iLen] = 0; // null terminate

            // fill hmem
            HANDLE   hMem;
            hMem = GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, iLen+1);
            if (!hMem)
               return TRUE;
            strcpy ((char*) GlobalLock(hMem), (char*) mem.p);
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

         WCHAR szCall[] = L"call:";
         DWORD dwLenCall = wcslen(szCall);
         if (!_wcsnicmp(p->psz, szCall, dwLenCall)) {
            PWSTR psz = p->psz + dwLenCall;

            DWORD dwPerson = _wtoi (psz);

            // find next color
            psz = wcschr (psz, L':');
            if (!psz)
               return FALSE;
            psz++;

            DWORD dwNode;
            dwNode = PhoneCall (pPage, TRUE, TRUE, dwPerson, psz);
            if (dwNode == (DWORD)-1)
               return TRUE;   // error

            // exit this, editing the new node
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) dwNode);
            pPage->Exit (szTemp);
            return TRUE;
         }

         break;
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "remove"
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         
         if (!_wcsicmp(p->pControl->m_pszName, L"edit")) {
            WCHAR szTemp[16];
            swprintf (szTemp, L"e:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove this business from your address book?"))
               return TRUE;

            // else remove
            PCMMLNode   pNode;
            pNode = FindMajorSection (gszPeopleNode);
            if (!pNode)
               return TRUE;   // error. This shouldn't happen

            // fill the list in
            RemoveFilteredList (gdwNode, pNode, gszBusinessNode,&glistBusiness, &glistBusinessID);
            // BUGFIX - Remove from combined list
            RemoveFilteredList (gdwNode, pNode, gszPersonBusinessNode,&glistPeopleBusiness, &glistPeopleBusinessID);

            // finall, flush release
            gpData->Flush();
            gpData->NodeRelease(pNode);


            // remove
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Removed from address book.");

            pPage->Link (L"r:118");
            return TRUE;
         }

      }
      break;   // default behavior

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         // only care about projectlist
         if (!_wcsicmp(p->pszSubName, L"PERSONNAME")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            // get the name
            PWSTR pszLast, pszNick;
            pszLast = NodeValueGet (pNode, gszLastName);
            pszNick = NodeValueGet (pNode, gszNickName);

            if (pszLast)
               MemCat (&gMemTemp, pszLast);

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }


         if (!_wcsicmp(p->pszSubName, L"CONTACTINFO")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            DWORD dwSum = 0;

            if (TableIfCopy(pNode, gszNickName, L"Primary contact:"))
               dwSum++;

            // phone numbers that can dial
            if (TableIfPhone(pNode, gszWorkPhone, L"Phone number:"))
               dwSum++;

            // FAX phone number
            if (TableIfCopy(pNode, gszFAXPhone, L"FAX phone number:"))
               dwSum++;

            // Emails and web site
            if (TableIfURL (pNode, gszBusinessEmail, L"E-mail:", TRUE))
               dwSum++;
            if (TableIfURL (pNode, gszPersonalWeb, L"Web site:", FALSE))
               dwSum++;

            // address
            if (TableIfCopy(pNode, gszWordAddress, L"Address:"))
               dwSum++;

            if (TableIfPerson (pNode, gszJournal, L"Link to journal category:"))
               dwSum++;

            // if nothing added then tell user
            if (!dwSum)
               MemCat (&gMemTemp, L"<tr><td>No contact information specified.</td></tr>");

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"MISCINFO")) {
            // zero memory and get the node
            MemZero (&gMemTemp);
            PCMMLNode   pNode;
            pNode = gpData->NodeGet (gdwNode);
            if (!pNode)
               return FALSE;

            // see if can get the misc text
            PWSTR psz;
            psz = NodeValueGet (pNode, gszMiscNotes);

            if (psz && psz[0]) {
               //MemCat (&gMemTemp, L"<xSectionTitle>Miscellaneous</xSectionTitle>");
               MemCat (&gMemTemp, L"<xTableCenter innerlines=0><xtheader>Miscellaneous</xtheader><tr><td>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</td></tr></xTableCenter>");
            }

            gpData->NodeRelease (pNode);
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"LOG")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszInteraction, FALSE);

            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask><xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"<br/>");
               DFTIMEToString (pnlg->start, szTemp);
               MemCat (&gMemTemp, szTemp);
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // if no entries then say so
            if (!pl || !pl->Num())
               MemCat (&gMemTemp, L"<tr><td>You haven't had any meetings or phone conversations with the business yet.</td></tr>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"MEMORIES")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszMemory, FALSE);

            if (!pl || !pl->Num()) {
               if (pl)
                  delete pl;
               if (pNode)
                  gpData->NodeRelease(pNode);
               p->pszSubString = (PWSTR) gMemTemp.p;
               return TRUE;
            }

            // header
            MemCat (&gMemTemp, L"<xTableCenter><xtheader>Memories</xtheader>");
            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask><xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // footer
            MemCat (&gMemTemp, L"</xtablecenter>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"PHOTOS")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszPhotos, FALSE);

            if (!pl || !pl->Num()) {
               if (pl)
                  delete pl;
               if (pNode)
                  gpData->NodeRelease(pNode);
               p->pszSubString = (PWSTR) gMemTemp.p;
               return TRUE;
            }

            // header
            MemCat (&gMemTemp, L"<xTableCenter><xtheader>Photos</xtheader>");
            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><xtdtask href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</xtdtask><xtdcompleted>");

               WCHAR szTemp[64];
               DFDATEToString (pnlg->date, szTemp);
               MemCat (&gMemTemp, szTemp);
               
               MemCat (&gMemTemp, L"</xtdcompleted></tr>");
            }

            // footer
            MemCat (&gMemTemp, L"</xtablecenter>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         if (!_wcsicmp(p->pszSubName, L"EMPLOYEES")) {
            // find the log
            PCMMLNode pNode;
            BOOL fDisplayed = FALSE;
            PCListFixed pl = NULL;
            MemZero (&gMemTemp);
            pNode = gpData->NodeGet (gdwNode);
            if (pNode)
               pl = NodeListGet (pNode, gszPerson, TRUE);

            if (!pl || !pl->Num()) {
               if (pl)
                  delete pl;
               if (pNode)
                  gpData->NodeRelease(pNode);
               p->pszSubString = (PWSTR) gMemTemp.p;
               return TRUE;
            }

            // header
            MemCat (&gMemTemp, L"<xTableCenter innerlines=0><xtheader>Employees</xtheader>");
            DWORD i;
            NLG *pnlg;
            if (pl) for (i = 0; i < pl->Num(); i++) {
               pnlg = (NLG*) pl->Get(i);
               
               // write it out
               MemCat (&gMemTemp, L"<tr><td><a href=v:");
               MemCat (&gMemTemp, pnlg->iNumber);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pnlg->psz ? pnlg->psz : L"Unknown");
               MemCat (&gMemTemp, L"</a></td></tr>");
            }

            // footer
            MemCat (&gMemTemp, L"</xtablecenter>");

            if (pl)
               delete pl;
            if (pNode)
               gpData->NodeRelease (pNode);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PeoplePersonEditPage - Page callback for new user (not quick-add though)
*/
BOOL PeoplePersonEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // just make sure have loaded in people info
      PeopleFilteredList();

      // fill in details
      PersonToControls (pPage, gdwNode);
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it's back or starts with r: then ask if they're sure wnat to go
         if (!_wcsicmp(p->psz, gszBack) || ((p->psz[0] == L'r') && (p->psz[1] == L':'))) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to leave this page?",
               L"You will lose all the changes you've typed in if you leave."))
               return TRUE;

            // else continue
            break;
         }


         break;
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // first of all, verify name and verify it doesn't already exist
            WCHAR szLast[128], szFirst[128], szNick[128];
            PCEscControl pControl;
            DWORD dwNeeded;
            szLast[0] = szFirst[0] = szNick[0] = 0;
            pControl = pPage->ControlFind(gszLastName);
            if (pControl)
               pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
            pControl = pPage->ControlFind(gszFirstName);
            if (pControl)
               pControl->AttribGet (gszText, szFirst, sizeof(szFirst), &dwNeeded);
            pControl = pPage->ControlFind(gszNickName);
            if (pControl)
               pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);

            // if it's empty then error
            if (!szLast[0] && !szFirst[0]) {
               pPage->MBWarning (L"You must type in a name.",
                  L"Either the first or last name must be filled in.");
               return TRUE;   // ignore
            }

            // make sure still find same old person
            WCHAR szFull[256];
            if (!PeopleFullName (szLast, szFirst, szNick, szFull, sizeof(szFull)))
               return TRUE;   // ignore
            DWORD dwMatch, dwFoundNode;
            // BUGFIX - Check in combined list for match
            dwMatch = FilteredStringToIndex(szFull, &glistPeopleBusiness, &glistPeopleBusinessID);
            dwFoundNode = PeopleBusinessIndexToDatabase (dwMatch);
            if ((dwMatch != (DWORD)-1) && (dwFoundNode != gdwNode)) {
               pPage->MBWarning (L"That name is already in your address book.",
                  L"You can't have another person/business with the exact same name.");
               return TRUE;   // ignore
            }

            // edit
            PersonFromControls (pPage, gdwNode);

            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"v:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }


         break;
      }
   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PeopleBusinessEditPage - Page callback for new user (not quick-add though)
*/
BOOL PeopleBusinessEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // just make sure have loaded in people info
      BusinessFilteredList();

      // fill in details
      BusinessToControls (pPage, gdwNode);
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it's back or starts with r: then ask if they're sure wnat to go
         if (!_wcsicmp(p->psz, gszBack) || ((p->psz[0] == L'r') && (p->psz[1] == L':'))) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to leave this page?",
               L"You will lose all the changes you've typed in if you leave."))
               return TRUE;

            // else continue
            break;
         }


         break;
      }
      break;   // default behavior

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            // first of all, verify name and verify it doesn't already exist
            WCHAR szLast[128], szNick[128];
            PCEscControl pControl;
            DWORD dwNeeded;
            szLast[0] = szNick[0] = 0;
            pControl = pPage->ControlFind(gszLastName);
            if (pControl)
               pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
            pControl = pPage->ControlFind(gszNickName);
            if (pControl)
               pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);

            // if it's empty then error
            if (!szLast[0]) {
               pPage->MBWarning (L"You must type in a name.");
               return TRUE;   // ignore
            }

            // make sure still find same old person
            DWORD dwMatch, dwFoundNode;
            // BUGFIX - Check in combined list for match
            dwMatch = FilteredStringToIndex(szLast, &glistPeopleBusiness, &glistPeopleBusinessID);
            dwFoundNode = PeopleBusinessIndexToDatabase (dwMatch);
            if ((dwMatch != (DWORD)-1) && (dwFoundNode != gdwNode)) {
               pPage->MBWarning (L"That name is already in your address book.",
                  L"You can't have another person/business with the exact same name.");
               return TRUE;   // ignore
            }

            // edit
            BusinessFromControls (pPage, gdwNode);

            // set the new page to viewing that person
            WCHAR szTemp[16];
            swprintf (szTemp, L"v:%d", (int) gdwNode);
            pPage->Link (szTemp);
            return TRUE;
         }


         break;
      }
   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
PeopleListPage - Lists people in the address book.
*/
int __cdecl PeopleSort(const void *elem1, const void *elem2 )
{
   DWORD dw1, dw2;
   dw1 = *((DWORD*)elem1);
   dw2 = *((DWORD*)elem2);

   PWSTR psz1, psz2;
   // BUGFIX - Show people and businesses
   psz1 = (PWSTR) glistPeopleBusiness.Get(dw1);
   psz2 = (PWSTR) glistPeopleBusiness.Get(dw2);

   return _wcsicmp(psz1, psz2);
}

BOOL PeopleListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"PEOPLE")) {

            // make sure the list is created
            PeopleFilteredList();

            // sort it
            CListFixed list;
            list.Init (sizeof(DWORD));
            DWORD i;
            // BUGFIX - Show people and businesses
            for (i = 0; i < glistPeopleBusiness.Num(); i++)
               list.Add (&i);
            qsort (list.Get(0), list.Num(), sizeof(DWORD), PeopleSort);

            MemZero (&gMemTemp);

            // display
            WCHAR cLast = 0;  // last character displayed
            for (i = 0; i < list.Num(); i++) {
               // get the name string and node
               DWORD dwIndex, dwNode;
               PWSTR psz;
               dwIndex = *((DWORD*) list.Get(i));
               dwNode = PeopleBusinessIndexToDatabase (dwIndex);
               psz = (PWSTR) glistPeopleBusiness.Get(dwIndex);
               if ((dwNode == (DWORD)-1) || !psz)
                  continue;

               // if last characters don't match then new table
               // BUGFIX - Was being case sensative here
               if (towupper(cLast) != towupper(psz[0])) {
                  if (cLast)
                     MemCat (&gMemTemp, L"</xtd2add></xLetterTable>");
                  MemCat (&gMemTemp, L"<xLetterTable><xtd1add>");
                  WCHAR szTemp[16];
                  szTemp[0] = cLast = psz[0];
                  szTemp[1] = 0;
                  MemCatSanitize (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L"</xtd1add><xtd2add>");
               }

               // display the link
               MemCat (&gMemTemp, L"<a href=v:");
               MemCat (&gMemTemp, (int) dwNode);
               MemCat (&gMemTemp, L">");
               BOOL fBusiness;
               fBusiness = (BusinessDatabaseToIndex (dwNode) != -1);
               if (fBusiness)
                  MemCat (&gMemTemp, L"<bold>");
               MemCatSanitize (&gMemTemp, psz);
               if (fBusiness)
                  MemCat (&gMemTemp, L"</bold>");
               MemCat (&gMemTemp, L"</a><br/>");
            }

            // if was a table then show
            if (cLast)
               MemCat (&gMemTemp, L"</xtd2add></xLetterTable>");

            if (!cLast)
               MemCat (&gMemTemp, L"<p>You address book is empty.</p>");

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
PeopleQuickAddPage - Page callback for quick-adding a new user
*/
BOOL PeopleQuickAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
         // first of all, verify name and verify it doesn't already exist
         WCHAR szLast[128], szFirst[128], szNick[128];
         PCEscControl pControl;
         DWORD dwNeeded;
         szLast[0] = szFirst[0] = szNick[0] = 0;
         pControl = pPage->ControlFind(gszLastName);
         if (pControl)
            pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
         pControl = pPage->ControlFind(gszFirstName);
         if (pControl)
            pControl->AttribGet (gszText, szFirst, sizeof(szFirst), &dwNeeded);
         pControl = pPage->ControlFind(gszNickName);
         if (pControl)
            pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);
         WCHAR szFull[256];
         if (!PeopleFullName (szLast, szFirst, szNick, szFull, sizeof(szFull)))
            return TRUE;   // ignore

         // if it's empty then error
         if (!szLast[0] && !szFirst[0]) {
            pPage->MBWarning (L"You must type in a name.",
               L"Either the first or last name must be filled in.");
            return TRUE;   // ignore
         }

         // add
         DWORD dwNode;
         dwNode = AddPerson (pPage, szFirst, szLast, szNick);
         if (dwNode == (DWORD)-1)
            return TRUE;

         // add
         PersonFromControls (pPage, dwNode);

         // add this person to the quick-add list on the main page
         PCMMLNode   pNode;
         pNode = FindMajorSection (gszPeopleNode);
         if (!pNode)
            return NULL;   // error. This shouldn't happen
         WCHAR szTemp[16];
         _itow ((int)dwNode, szTemp, 10);
         PCMMLNode   pSub;
         pSub = pNode->ContentAddNewNode ();
         if (!pSub)
            return FALSE;
         pSub->NameSet (gszPersonQuickAdd);
         pSub->AttribSet (gszNumber, szTemp);
         pSub->ContentAdd (szFull);
         gpData->Flush();
         gpData->NodeRelease(pNode);



         // return saying what the new node is
         //WCHAR szTemp[16];
         swprintf (szTemp, L"!%d", (int) dwNode);
         pPage->Link (szTemp);
         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}


/*****************************************************************************
PeopleQuickAdd - Quick-adds a person to the list.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   DWORD - New subproject index. -1 if didn't add.
*/
DWORD PeopleQuickAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   // BUGFIX - Autosize the window
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPERSONQUICKADD, PeopleQuickAddPage);

   if (!pszRet || (pszRet[0] != L'!'))
      return (DWORD) -1;   // canceled

   return PeopleDatabaseToIndex(_wtoi(pszRet+1));
}

/***********************************************************************
BusinessQuickAddPage - Page callback for quick-adding a new user
*/
BOOL BusinessQuickAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
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
         // first of all, verify name and verify it doesn't already exist
         WCHAR szLast[128], szNick[128];
         PCEscControl pControl;
         DWORD dwNeeded;
         szLast[0] = szNick[0] = 0;
         pControl = pPage->ControlFind(gszLastName);
         if (pControl)
            pControl->AttribGet (gszText, szLast, sizeof(szLast), &dwNeeded);
         pControl = pPage->ControlFind(gszNickName);
         if (pControl)
            pControl->AttribGet (gszText, szNick, sizeof(szNick), &dwNeeded);

         // if it's empty then error
         if (!szLast[0]) {
            pPage->MBWarning (L"You must type in a name.");
            return TRUE;   // ignore
         }

         // add
         DWORD dwNode;
         dwNode = AddBusiness (pPage, szLast, szNick);
         if (dwNode == (DWORD)-1)
            return TRUE;

         // add
         BusinessFromControls (pPage, dwNode);

         // add this person to the quick-add list on the main page
         PCMMLNode   pNode;
         pNode = FindMajorSection (gszPeopleNode);
         if (!pNode)
            return NULL;   // error. This shouldn't happen
         WCHAR szTemp[16];
         _itow ((int)dwNode, szTemp, 10);
         PCMMLNode   pSub;
         pSub = pNode->ContentAddNewNode ();
         if (!pSub)
            return FALSE;
         pSub->NameSet (gszPersonQuickAdd);
         pSub->AttribSet (gszNumber, szTemp);
         pSub->ContentAdd (szLast);
         gpData->Flush();
         gpData->NodeRelease(pNode);



         // return saying what the new node is
         //WCHAR szTemp[16];
         swprintf (szTemp, L"!%d", (int) dwNode);
         pPage->Link (szTemp);
         return TRUE;
      }
      break;

   };

   return DefPage (pPage, dwMessage, pParam);
}

/*****************************************************************************
BusinessQuickAdd - Quick-adds a person to the list.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   DWORD - New subproject index. -1 if didn't add.
*/
DWORD BusinessQuickAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLBUSINESSQUICKADD, BusinessQuickAddPage);

   if (!pszRet || (pszRet[0] != L'!'))
      return (DWORD) -1;   // canceled

   return BusinessDatabaseToIndex(_wtoi(pszRet+1));
}

/*****************************************************************************
PeopleBusinessQuickAdd - Quick-adds a person to the list.

inputs
   PCEscPage      pPage - page to show it off of. Gets the window and uses that.
returns
   DWORD - New subproject index. -1 if didn't add.
*/
DWORD PeopleBusinessQuickAdd (PCEscPage pPage)
{
   HANGFUNCIN;
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPERSONBUSINESSQUICKADD, DefPage);

   DWORD dwRet;
   if (pszRet && !_wcsicmp(pszRet, L"newperson")) {
      dwRet = PeopleQuickAdd (pPage);
      if (dwRet == (DWORD)-1)
         return dwRet;
      return PeopleBusinessDatabaseToIndex (PeopleIndexToDatabase (dwRet));
   }
   else if (pszRet && !_wcsicmp(pszRet, L"newbusiness")) {
      dwRet = BusinessQuickAdd (pPage);
      if (dwRet == (DWORD)-1)
         return dwRet;
      return PeopleBusinessDatabaseToIndex (BusinessIndexToDatabase (dwRet));
   }
   else
      return -1;
}
/***********************************************************************
PeopleSetView - Tells the people unit what person is being viewed/edited.

inputs
   DWORD    dwNode - index
returns
   BOOL - TRUE if success
*/
BOOL PeopleSetView (DWORD dwNode)
{
   HANGFUNCIN;
   gdwNode = dwNode;
   return TRUE;
}

/***********************************************************************
GetNextField - Assumes that have *pStart just after the comma or CR.
   Reads until the next field.

inputs
   PSTR     pszStart - Starting position
   PSTR     *ppszNext - Next starting position to use.
   PCMem    pMem - Filled with wide character string. Might be an empty string
returns
   DWORD - 2 if there are more fields for this line, 1 if this was the end of the line,
         0 if this was the end of the file.
*/
DWORD GetNextField (PSTR pszStart, PSTR *ppszNext, PCMem pMem)
{
   HANGFUNCIN;
   MemZero (pMem);
   *ppszNext = pszStart;   // just in case

   // if start with quote then look for a quote followed by comma, or quote followed by cr/f
   // if start with non-quote then look for comma
   char  *pS, *pE;
   DWORD dwExitCode = 2;
   if (*pszStart == '"') {
      pS = pszStart + 1;
      for (pE = pS; ; pE++) {
         if (!pE[0]) {
            dwExitCode = 0;
            *ppszNext = pE;
            break;
         }
         if (pE[0] != '"')
            continue;

         if (pE[1] == 0) {
            dwExitCode = 0;
            *ppszNext = pE+1;
            break;
         }
         if (pE[1] == ',') {
            dwExitCode = 2;
            *ppszNext = pE+2;
            break;
         }
         if ((pE[1] == '\n') || (pE[1] == '\r')) {
            dwExitCode = 1;
            *ppszNext = pE+2;
            break;
         }
         
         // else continue
      }
   }
   else { // no quote
      pS = pszStart;
      for (pE = pS; ; pE++) {
         if (!pE[0]) {
            dwExitCode = 0;
            *ppszNext = pE;
            break;
         }
         if (pE[0] == ',') {
            dwExitCode = 2;
            *ppszNext = pE+1;
            break;
         }
         if ((pE[0] == '\n') || (pE[0] == '\r')) {
            dwExitCode = 1;
            *ppszNext = pE+1;
            break;
         }
         
         // else continue
      }
   }

   // Temporarily fill pE with NULL and convert the string
   char c;
   c = *pE;
   *pE = 0;
   int iSize;
   pMem->Required (iSize = ((int)(pE - pS)+1)*2);
   MultiByteToWideChar (CP_ACP, 0, pS, -1, (PWSTR) pMem->p, iSize/2);
   *pE = c;

   // done
   return dwExitCode;
}


/***********************************************************************
FillImportField - Finds a field that matches the given string and
fills the import field.

inputs
   PCEscPage   pPage - page
   PWSTR       pszControl - Control name to fill
   PWSTR       pszField - Field name that looking for
*/
BOOL FillImportField (PCEscPage pPage, PWSTR pszControl, PWSTR pszField)
{
   HANGFUNCIN;
   // find the control
   PCEscControl pControl;
   pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // find the field
   DWORD i;
   for (i = 0; i < glistFields.Num(); i++) {
      PWSTR psz = (PWSTR) glistFields.Get(i);

      if (!_wcsicmp(psz, pszField)) {
         pControl->AttribSetInt (gszCurSel, i);
         return TRUE;
      }
   }

   // if get here couldn't find, so leave blank
   return FALSE;
}


/***********************************************************************
GetField - Given a control name for the conversion, this finds out
   what's selected as the conversion source. The index into the list
   of input paremeters is then found, which is used to index into
   the person's file.

inputs
   PCEscPage   pPage - page
   PCListVariable pPerson - List of field values
   PWSTR       pszControl - Control name
returns
   PWSTR - String for the field. NULL if doesn't exist or is blank
*/
PWSTR GetField (PCEscPage pPage, PCListVariable pPerson, PWSTR pszControl)
{
   HANGFUNCIN;
   PCEscControl pControl;
   pControl = pPage->ControlFind(pszControl);
   if (!pControl)
      return NULL;

   int   iSel;
   iSel = pControl->AttribGetInt (gszCurSel);
   if (iSel < 0)
      return NULL;   // no conversion

   // find this in index
   DWORD i;
   i = (DWORD) iSel;
   if (i >= glistFields.Num())
      return NULL;   // can't find

   // get this from person
   PWSTR psz;
   psz = (PWSTR) pPerson->Get(i);
   if (psz && psz[0])
      return psz;
   else
      return NULL;
}

/***********************************************************************
ConvertPeople - Called from the import page, this goes through all the
people and creates conversions for them.

inputs
   PCEscPage - page
returns
   BOOL - TRUE if all OK, FALSE if cancel
*/
BOOL ConvertPeople (PCEscPage pPage)
{
   HANGFUNCIN;
   DWORD dwAdded = 0, dwSkipped = 0, dwOverwritten=0, dwNoName = 0;

   // set wait cursor
   pPage->SetCursor (IDC_NOCURSOR);

   // figure out which fields are not used
   BOOL f, *pf;
   DWORD i;
   CListFixed  listUsed;
   listUsed.Init (sizeof(f));
   f = FALSE;
   for (i = 0; i < glistFields.Num(); i++) {
      listUsed.Add (&f);
   }
   PWSTR apszFields[] = {
      gszName, gszLastName, gszFirstName, gszNickName, gszHomePhone, gszMobilePhone,
      gszWorkPhone, gszFAXPhone, gszPersonalEmail, gszBusinessEmail,
      gszHomeStreet, gszHomeCity, gszHomeState, gszHomePostal, gszHomeCountry,
      gszBusinessStreet, gszBusinessCity, gszBusinessState, gszBusinessPostal, gszBusinessCountry,
      gszCompany, gszJobTitle, gszDepartment, gszOffice
   };
   for (i = 0; i < sizeof(apszFields)/sizeof(PWSTR); i++) {
      // find the control
      PCEscControl pControl;
      pControl = pPage->ControlFind (apszFields[i]);
      if (!pControl)
         continue;

      int   iSel;
      iSel = pControl->AttribGetInt (gszCurSel);
      if (iSel < 0)
         continue;

      // remember that it's used
      pf = (BOOL*) listUsed.Get((DWORD)iSel);
      if (pf)
         *pf = TRUE;
   }

   // loop
   for (i = 0; i < gplistPeople->Num(); i++) {
      PCListVariable pl = *((PCListVariable*)gplistPeople->Get(i));

      // get the name
      PWSTR pszFirst, pszLast, pszNick, pszName;
      pszName = GetField (pPage, pl, gszName);
      pszFirst = GetField (pPage, pl, gszFirstName);
      pszLast = GetField (pPage, pl, gszLastName);
      pszNick = GetField (pPage, pl, gszNickName);

      // see if the person already exists
      BOOL  fExists = FALSE;
      WCHAR szFull[256];
      WCHAR szEmpty[] = L"";

      // if there's no first & last names then try to use name
      WCHAR szFirst[512];
      WCHAR szLast[512];
      if (!pszFirst && !pszLast && pszName && pszName[0]) {
         szFirst[0] = szLast[0] = 0;
         WCHAR *pCur, *pSpace;

         // if there's a comma in the name then that's the first/last separator
         for (pCur = pszName, pSpace = NULL; *pCur; pCur++)
            if (*pCur == L',') {
               pSpace = pCur+1;
               break;
            }
         if (pSpace) {
            // space comes after commas
            for (; *pSpace == L' '; pSpace++);

            wcscpy (szFirst, pSpace);
            wcscpy (szLast, pszName);
            for (pCur = szLast; *pCur; pCur++)
               if (*pCur == L',') {
                  *pCur = 0;
                  break;
                  }

            pszFirst = szFirst;
            pszLast = szLast;
         }
         else {
            // find the last space
            for (pCur = pSpace = pszName; *pCur; pCur++)
               if (*pCur == L' ')
                  pSpace = pCur + 1;

            // last name is at pSpace
            wcscpy (szLast, pSpace);
            DWORD dwLen;
            dwLen = ((PBYTE)pSpace - (PBYTE)pszName) / 2;
            memcpy (szFirst, pszName, dwLen*2);
            szFirst[dwLen ? (dwLen-1) : 0] = 0;

            pszFirst = szFirst;
            pszLast = szLast;
         }
      }
      if (!pszFirst && !pszLast && !pszNick) {
         dwNoName++;
         continue;   // no name whatsoever, so can't really add
      }
      if (!PeopleFullName (pszLast ? pszLast : szEmpty, pszFirst ? pszFirst : szEmpty, pszNick ? pszNick : szEmpty, szFull, sizeof(szFull))) {
         dwNoName++;
         continue;   // ignore
      }

      DWORD dwIndex;
      dwIndex = FilteredStringToIndex(szFull, &glistPeople, &glistPeopleID);
      if (dwIndex != (DWORD)-1)
         fExists = TRUE;

      if (fExists) {
         // see what want to do
         PCEscControl pControl;
         PWSTR pszIf = NULL;
         pControl = pPage->ControlFind (L"ifexist");
         ESCMCOMBOBOXGETITEM gi;
         memset (&gi, 0, sizeof(gi));
         if (pControl) {
            gi.dwIndex = (DWORD) pControl->AttribGetInt (gszCurSel);
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            pszIf = gi.pszName;
         }
         if (pszIf && !_wcsicmp(pszIf, L"ignore")) {
            // chose to ignore. Since there's a match just continue
            dwSkipped++;
            continue;
         }
         else if (pszIf && !_wcsicmp(pszIf, L"ask")) {
            // chose to ask, so do so
            WCHAR szTemp[1024];
            swprintf (szTemp, L"%s is already in your Dragonfly address book. Do you want to replace the entry?",
               szFull);
            int iRet;
            iRet = pPage->MBYesNo (szTemp, NULL, TRUE);
            if (iRet == IDCANCEL)
               return FALSE;  // pressed cancel
            if (iRet == IDNO) {
               dwSkipped++;
               continue;   // skip
            }
            // else go on
         }
      }

      // find or create the element
      PCMMLNode   pNode;
      dwIndex = PeopleIndexToDatabase (dwIndex);
      if ((dwIndex != (DWORD)-1) && fExists) {
         pNode = gpData->NodeGet (dwIndex);
         dwOverwritten++;
      }
      else {
         dwIndex = AddPerson (pPage, pszFirst ? pszFirst : szEmpty, pszLast ? pszLast : szEmpty, pszNick ? pszNick : szEmpty);
         pNode = gpData->NodeGet (dwIndex);
         dwAdded++;
      }
      if (!pNode)
         continue;   // error

      // write in info
      PWSTR psz;
      psz = GetField (pPage, pl, gszHomePhone);
      if (psz)
         NodeValueSet (pNode, gszHomePhone, psz);
      psz = GetField (pPage, pl, gszMobilePhone);
      if (psz)
         NodeValueSet (pNode, gszMobilePhone, psz);
      psz = GetField (pPage, pl, gszWorkPhone);
      if (psz)
         NodeValueSet (pNode, gszWorkPhone, psz);
      psz = GetField (pPage, pl, gszFAXPhone);
      if (psz)
         NodeValueSet (pNode, gszFAXPhone, psz);
      psz = GetField (pPage, pl, gszPersonalEmail);
      if (psz)
         NodeValueSet (pNode, gszPersonalEmail, psz);
      psz = GetField (pPage, pl, gszBusinessEmail);
      if (psz)
         NodeValueSet (pNode, gszBusinessEmail, psz);
      psz = GetField (pPage, pl, gszPersonalWeb);
      if (psz)
         NodeValueSet (pNode, gszPersonalWeb, psz);
      psz = GetField (pPage, pl, gszCompany);
      if (psz)
         NodeValueSet (pNode, gszCompany, psz);
      psz = GetField (pPage, pl, gszJobTitle);
      if (psz)
         NodeValueSet (pNode, gszJobTitle, psz);
      psz = GetField (pPage, pl, gszDepartment);
      if (psz)
         NodeValueSet (pNode, gszDepartment, psz);
      psz = GetField (pPage, pl, gszOffice);
      if (psz)
         NodeValueSet (pNode, gszOffice, psz);
      
      // home address
      WCHAR szCR[] = L"\r\n";
      BOOL fLine;
      MemZero (&gMemTemp);
      fLine = FALSE;
      psz = GetField (pPage, pl, gszHomeStreet);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszHomeCity);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszHomeState);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszHomePostal);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszHomeCountry);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      if (fLine)
         NodeValueSet (pNode, gszHomeAddress, (PWSTR) gMemTemp.p);

      // work address
      MemZero (&gMemTemp);
      fLine = FALSE;
      psz = GetField (pPage, pl, gszBusinessStreet);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszBusinessCity);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszBusinessState);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszBusinessPostal);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      psz = GetField (pPage, pl, gszBusinessCountry);
      if (psz) {
         if (fLine)
            MemCat (&gMemTemp, szCR);
         fLine = TRUE;
         MemCat (&gMemTemp, psz);
      }
      if (fLine)
         NodeValueSet (pNode, gszWordAddress, (PWSTR) gMemTemp.p);

      // append to notes
      PCEscControl pControl;
      pControl = pPage->ControlFind (L"restinnotes");
      if (pControl->AttribGetBOOL (gszChecked)) {
         psz = NodeValueGet (pNode, gszMiscNotes);
         MemZero (&gMemTemp);
         if (psz)
            MemCat (&gMemTemp, psz);
         DWORD dwField;
         for (dwField = 0; dwField < listUsed.Num(); dwField++) {
            BOOL *pf;
            pf = (BOOL*) listUsed.Get(dwField);
            if (!pf || *pf)
               continue;   // already used so dont bother

            // if there's no text then don't add
            psz = (PWSTR) pl->Get(dwField);
            if (!psz || !psz[0])
               continue;

            // get the name of the field
            PWSTR pszFieldName;
            pszFieldName = (PWSTR) glistFields.Get(dwField);

            // if both these strings already exist in notes then don't add
            if (wcsstr((WCHAR*)gMemTemp.p, psz) && wcsstr((WCHAR*)gMemTemp.p, pszFieldName))
               continue;

            // if it's just "Name" then skip
            if (!_wcsicmp (pszFieldName, L"Name"))
               continue;

            //else add
            if (((PWSTR)gMemTemp.p)[0])
               MemCat (&gMemTemp, L"\r\n");
            MemCat (&gMemTemp, pszFieldName);
            MemCat (&gMemTemp, L": ");
            MemCat (&gMemTemp, psz);
         }
         NodeValueSet (pNode, gszMiscNotes, (PWSTR) gMemTemp.p);
      }

      // done
      gpData->NodeRelease (pNode);
   }

   // flush
   gpData->Flush();

   // tell the user statistics
   WCHAR szHuge[128];
   swprintf (szHuge, L"%d people added.\r\n%d people overwritten.\r\n%d people skipped.\r\n%d entries skipped because they didn't have a name.",
      dwAdded, dwOverwritten, dwSkipped, dwNoName);
   pPage->MBInformation (L"Address book merged into Dragonfly's.", szHuge);

   return TRUE;
}


/***********************************************************************
PeopleImportPage - Lists people in the address book.
*/

BOOL PeopleImportPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_FILTEREDLISTQUERY:
      {
         // this is a message sent by the filtered list controls
         // used as samples in help.

         PESCNFILTEREDLISTQUERY p = (PESCNFILTEREDLISTQUERY) pParam;

         if (!_wcsicmp(p->pszListName, L"addressfield")) {
            p->pList = &glistFields;
            return TRUE;
         }
      }
      break;   // default

   case ESCM_INITPAGE:
      {
         FillImportField (pPage, gszLastName, L"Last Name");
         FillImportField (pPage, gszName, L"Name");
         FillImportField (pPage, gszFirstName, L"First Name");
         FillImportField (pPage, gszNickName, L"Nickname");
         FillImportField (pPage, gszHomePhone, L"Home Phone");
         FillImportField (pPage, gszMobilePhone, L"Mobile Phone");
         FillImportField (pPage, gszWorkPhone, L"Business Phone");
         if (!FillImportField (pPage, gszFAXPhone, L"Home Fax"))
            FillImportField (pPage, gszFAXPhone, L"Business Fax");
         FillImportField (pPage, gszPersonalEmail, L"E-mail Address");
         FillImportField (pPage, gszBusinessEmail, L"E-mail Address");
         if (!FillImportField (pPage, gszPersonalWeb, L"Personal Web Page"))
            FillImportField (pPage, gszPersonalWeb, L"Business Web Page");
         FillImportField (pPage, gszHomeStreet, L"Home Street");
         FillImportField (pPage, gszHomeCity, L"Home City");
         FillImportField (pPage, gszHomeState, L"Home State");
         FillImportField (pPage, gszHomePostal, L"Home Postal Code");
         FillImportField (pPage, gszHomeCountry, L"Home Country");
         FillImportField (pPage, gszBusinessStreet, L"Business Street");
         FillImportField (pPage, gszBusinessCity, L"Business City");
         FillImportField (pPage, gszBusinessState, L"Business State");
         FillImportField (pPage, gszBusinessPostal, L"Business Postal Code");
         FillImportField (pPage, gszBusinessCountry, L"Business Country");
         FillImportField (pPage, gszCompany, L"Company");
         FillImportField (pPage, gszJobTitle, L"Job title");
         FillImportField (pPage, gszDepartment, L"Department");
         FillImportField (pPage, gszOffice, L"Office");
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, gszAdd)) {
            ConvertPeople (pPage);

            pPage->Exit (gszOK);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
/***********************************************************************
ImportAddress - UI and code to import and addres book from a comma
separated file.

inputs
   PCEscPage      pPage - Page
returns
   none
*/
void ImportAddress (PCEscPage pPage)
{
   HANGFUNCIN;
   // first, file open dialog
   OPENFILENAME   ofn;
   HWND  hWnd = pPage->m_pWindow->m_hWnd;
   char  szTemp[256];
   szTemp[0] = 0;

   memset (&ofn, 0, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter =
      "Comma separated values (*.csv)\0*.csv\0"
      "Text file (*.txt)\0*.txt\0"
      "All files (*.*)\0*.*\0"
      "\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = "Open comma-separated address-book file";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "csv";
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return;

   // read it in
   CMem  mem;
   FILE *f;
   f = fopen (ofn.lpstrFile, "rt");
   if (!f) {
      pPage->MBError (L"Dragonfly couldn't open the file.");
      return;
   }
   // allocate enough memory for everything
   fseek (f, 0, SEEK_END);
   long lSize;
   lSize = ftell (f);
   fseek (f, 0, 0);
   mem.Required (lSize+1);

   lSize = fread (mem.p, 1, lSize, f);
   ((char*)mem.p)[lSize] = 0;
   fclose (f);
   char *pszFile;
   pszFile = (char*) mem.p;

   // parse the first line for fields
   char *pszNext;
   DWORD dwRet;
   CMem  memField;
   PWSTR pszField;
   glistFields.Clear();
   while (TRUE) {
      dwRet = GetNextField (pszFile, &pszNext, &memField);
      pszFile = pszNext;
      pszField = (PWSTR) memField.p;
      if (!pszField[0]) {
         pPage->MBError (L"The file isn't a comma-separated list.",
            L"One of the field names is empty.");
         return;
      }
      if (dwRet == 0) {
         pPage->MBError (L"The file isn't a comma-separated list.",
            L"It only has a list of fields without any items in the address book.");
         return;
      }
      glistFields.Add (pszField, (wcslen(pszField)+1)*2);

      // exit if end if line
      if (dwRet == 1)
         break;
      // else get more
   }

   // parse the people
   CListFixed  listPeople;
   listPeople.Init (sizeof(PCListVariable));
   PCListVariable pPerson;
   while (TRUE) {
      pPerson = new CListVariable;
      BOOL  fNullPerson = FALSE;

      // repeat getting nodes
      while (TRUE) {
         dwRet = GetNextField (pszFile, &pszNext, &memField);
         pszFile = pszNext;
         pszField = (PWSTR) memField.p;

         // if it's a blank field, and there's an EOF and CR, and it's
         // the first one then it's a NULL person
         if (!pszField && ((dwRet == 0) || (dwRet == 1)) && !pPerson->Num())
            fNullPerson = TRUE;

         pPerson->Add (pszField, (wcslen(pszField)+1)*2);

         // exit unless more in this field
         if (dwRet != 2)
            break;
      }

      if (fNullPerson)
         delete pPerson;
      else
         // add the person
         listPeople.Add (&pPerson);

      // continue on
      if (dwRet == 0)
         break;   // end of file
   }
   gplistPeople = &listPeople;

   // Show UI
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   PWSTR pszRet;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPEOPLEIMPORT, PeopleImportPage);

   // delete people
   DWORD i;
   for (i = 0; i < listPeople.Num(); i++) {
      pPerson = *((PCListVariable*) listPeople.Get(i));
      delete pPerson;
   }
}


/***********************************************************************
PeoplePage - Lists people in the address book.
*/

BOOL PeoplePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"importaddress")) {
            // BUGFIX - User wants ability to import comman delimited
            ImportAddress (pPage);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"QUICKADD")) {

            // make sure the list is created
            PeopleFilteredList();

            // see if there are any people
            PCMMLNode   pNode;
            pNode = FindMajorSection (gszPeopleNode);
            if (!pNode)
               return NULL;   // error. This shouldn't happen

            DWORD dwIndex;
            MemZero (&gMemTemp);
            dwIndex = pNode->ContentFind (gszPersonQuickAdd);
            if (dwIndex != (DWORD)-1) {
               // we have stuff so add it
               MemCat (&gMemTemp, L"<xbr/><xSectionTitle>Quickly-added people</xSectionTitle>");
               MemCat (&gMemTemp, L"<p>While you were using other parts of Dragonfly you \"quickly added\" some people. ");
               MemCat (&gMemTemp, L"Their names are stored in Dragonfly but some information about them is ");
               MemCat (&gMemTemp, L"incomplete. When you have the chance, please fill in the rest of the personal ");
               MemCat (&gMemTemp, L"information.</p>");
               MemCat (&gMemTemp, L"<blockquote><p>");

               DWORD i;
               PWSTR psz;
               PCMMLNode   pSub;
               for (i = 0; i < pNode->ContentNum(); i++) {
                  pSub = NULL;
                  pNode->ContentEnum (i, &psz, &pSub);
                  if (!pSub)
                     continue;   // dont care about this

                  // else, name check
                  if (_wcsicmp(gszPersonQuickAdd, pSub->NameGet()))
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

                  // write this up
                  MemCat (&gMemTemp, L"<xQuickFillButton href=e:");
                  MemCat (&gMemTemp, (int) dwNum);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, psz);
                  MemCat (&gMemTemp, L"</xQuickFillButton>");
               }

               
               MemCat (&gMemTemp, L"</p></blockquote>");
            }

            gpData->NodeRelease(pNode);



            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


typedef struct {
   PWSTR       pszName;    // name to display
   PWSTR       pszHome;    // home phone. NULL if none
   PWSTR       pszWork;    // work number
   PWSTR       pszMobile;  // mobile number
   PWSTR       pszBusiness;   // business main #
} PHONEQUERY, *PPHONEQUERY;

/*****************************************************************************
PhoneWhichNumberPage - Override page callback.
*/
BOOL PhoneWhichNumberPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   PPHONEQUERY pq = (PPHONEQUERY) pPage->m_pUserData;

   switch (dwMessage) {

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!p->pszSubName)
            break;

         if (!_wcsicmp(p->pszSubName, L"PERSONNAME")) {
            p->pszSubString = pq->pszName;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"CHOICES")) {
            MemZero (&gMemTemp);

            if (pq->pszHome) {
               MemCat (&gMemTemp, L"<tr><xtdleft align=right>Home:</xtdleft><xtdright><a href=home>");
               MemCatSanitize (&gMemTemp, pq->pszHome);
               MemCat (&gMemTemp, L"</a></xtdright></tr>");
            }
            if (pq->pszWork) {
               MemCat (&gMemTemp, L"<tr><xtdleft align=right>Work:</xtdleft><xtdright><a href=Work>");
               MemCatSanitize (&gMemTemp, pq->pszWork);
               MemCat (&gMemTemp, L"</a></xtdright></tr>");
            }
            if (pq->pszMobile) {
               MemCat (&gMemTemp, L"<tr><xtdleft align=right>Mobile:</xtdleft><xtdright><a href=Mobile>");
               MemCatSanitize (&gMemTemp, pq->pszMobile);
               MemCat (&gMemTemp, L"</a></xtdright></tr>");
            }
            if (pq->pszBusiness) {
               MemCat (&gMemTemp, L"<tr><xtdleft align=right>Company:</xtdleft><xtdright><a href=Business>");
               MemCatSanitize (&gMemTemp, pq->pszBusiness);
               MemCat (&gMemTemp, L"</a></xtdright></tr>");
            }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


/**************************************************************************
PhoneNumGet - Given a person, gets their phone number. If they have more
than one number then this asks for clarification. If they don't have any
numbers then it returns an error.

inputs
   PCEscPage pPage - To show message boxes from if have questions/errors
   DWORD    dwNode - person's data node
   PWSTR    psz - Filled with the phone number
   DWORD    wdSize - # bytes in psz
returns
   BOOL - TRUE if succede and psz is valid.. FALSE if no number is gotten.
*/
BOOL PhoneNumGet (PCEscPage pPage, DWORD dwNode, PWSTR psz, DWORD dwSize)
{
   HANGFUNCIN;
   // get the numbers
   PWSTR  pszHome, pszMobile, pszWork, pszFirst, pszLast, pszBusiness;

   PCMMLNode   pNode, pBusiness;
   pNode = gpData->NodeGet (dwNode);
   if (!pNode)
      return FALSE;
   pBusiness = BusinessFromPerson (dwNode);


   // get the numbers
   pszFirst = NodeValueGet (pNode, gszFirstName);
   pszLast = NodeValueGet (pNode, gszLastName);
   pszHome = NodeValueGet (pNode, gszHomePhone);
   pszWork = NodeValueGet (pNode, gszWorkPhone);
   pszMobile = NodeValueGet (pNode, gszMobilePhone);
   pszBusiness = pBusiness ? NodeValueGet (pBusiness, gszWorkPhone) : NULL;

   // come up with a friendly name
   PWSTR pszFriendly;
   WCHAR szTemp[512];
   if (pszFirst && pszFirst[0])
      pszFriendly = pszFirst;
   else
      pszFriendly = pszLast;

   // see which numbes have values
   BOOL fHome, fWork, fMobile, fBusiness;
   DWORD dwNum;
   fHome = pszHome && pszHome[0];
   fWork = pszWork && pszWork[0];
   fMobile = pszMobile && pszMobile[0];
   // BUGFIX - Show company's main phone, but only if work phone not entered
   fBusiness = !fWork && pszBusiness && pszBusiness[0];
   dwNum = (fHome ? 1 : 0) + (fWork ? 1 : 0) + (fMobile ? 1 : 0) + (fBusiness ? 1 : 0);

   // make up a phone # string
   WCHAR szNumbers[512];
   szNumbers[0] = 0;
   if (fMobile) {
      if (szNumbers[0])
         wcscat (szNumbers, L"\r\n");
      wcscat (szNumbers, L"Mobile: ");
      wcscat (szNumbers, pszMobile);
   }
   if (fWork) {
      if (szNumbers[0])
         wcscat (szNumbers, L"\r\n");
      wcscat (szNumbers, L"Work: ");
      wcscat (szNumbers, pszWork);
   }
   if (fHome) {
      if (szNumbers[0])
         wcscat (szNumbers, L"\r\n");
      wcscat (szNumbers, L"Home: ");
      wcscat (szNumbers, pszHome);
   }
   if (fBusiness) {
      if (szNumbers[0])
         wcscat (szNumbers, L"\r\n");
      wcscat (szNumbers, L"Work: ");
      wcscat (szNumbers, pszBusiness);
   }

   // if none have numbers then return
   if (!dwNum) {
      swprintf (szTemp, L"%s's address book entry doesn't list any phone numbers.",
         pszFriendly);
      pPage->MBWarning (szTemp);
      gpData->NodeRelease(pNode);
      if (pBusiness)
         gpData->NodeRelease (pBusiness);
      return FALSE;
   }
   
   // if there's only one then it's obvious
   CEscWindow  cWindow;
   PWSTR pszWant;
   pszWant = NULL;
   if (dwNum == 1) {
      if (fHome)
         pszWant = pszHome;
      else if (fWork)
         pszWant = pszWork;
      else if (fMobile)
         pszWant = pszMobile;
      else
         pszWant = pszBusiness;
      goto done;
   }

   // BUGFIX - Rather than showing message boxes show custom list
   // else, choice
   // pull up UI
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd,
      EWS_TITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH, &r);
   PWSTR pszRet;
   PHONEQUERY pq;
   pq.pszHome = fHome ? pszHome : NULL;
   pq.pszWork = fWork ? pszWork : NULL;
   pq.pszMobile = fMobile ? pszMobile : NULL;
   pq.pszName = pszFriendly;
   pq.pszBusiness = fBusiness ? pszBusiness : NULL;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLPHONEWHICHNUMBER, PhoneWhichNumberPage, &pq);
   if (!pszRet) {
      gpData->NodeRelease(pNode);
      if (pBusiness)
         gpData->NodeRelease (pBusiness);
      return FALSE;
   }
   if (!_wcsicmp(pszRet, L"home"))
      pszWant = pszHome;
   else if (!_wcsicmp(pszRet, L"work"))
      pszWant = pszWork;
   else if (!_wcsicmp(pszRet, L"mobile"))
      pszWant = pszMobile;
   else if (!_wcsicmp(pszRet, L"business"))
      pszWant = pszBusiness;
   else {
      gpData->NodeRelease(pNode);
      if (pBusiness)
         gpData->NodeRelease (pBusiness);
      return FALSE;
   }


done:
   if ( ((wcslen(pszWant)+1)*2) > dwSize)
      return FALSE;
   wcscpy (psz, pszWant);
   gpData->NodeRelease(pNode);
      if (pBusiness)
         gpData->NodeRelease (pBusiness);
   return TRUE;
}

/***********************************************************************
PrintAddressBook - Prints the address book.

inputs
   PCEscPage      pPage - for messages
   BOOL           *pafRleations - 8 bools indicating what relations chosen
   DWORD          dwNumYears - Number of years back to go
   DWORD          dwFields - 0 for only phone #, 1 also includes address
   DWORD          dwWidth - Width
   DWORD          dwHeight - Height
   DWORD          dwFontSize - In points
returns
   none
*/
void PrintAddressBook (PCEscPage pPage, BOOL *pafRelations, DWORD dwNumYears,
                       DWORD dwFields, DWORD dwWidth, DWORD dwHeight, DWORD dwFontSize)
{
   HANGFUNCIN;
   // sort the list alpabetically
   PeopleFilteredList();
   CListFixed list;
   list.Init (sizeof(DWORD));
   DWORD i;
   // BUGFIX - Show people and business
   for (i = 0; i < glistPeopleBusiness.Num(); i++)
      list.Add (&i);
   qsort (list.Get(0), list.Num(), sizeof(DWORD), PeopleSort);

   // set up memory
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<font size=");
   MemCat (&gMemTemp, (int) dwFontSize);
   MemCat (&gMemTemp, L"><table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=100%%>");
   DWORD dwPrinted;
   dwPrinted = 0;

   // load in people and print
   for (i = 0; i < list.Num(); i++) {
      DWORD dwIndex, dwNode;
      dwIndex = *((DWORD*) list.Get(i));
      dwNode = PeopleBusinessIndexToDatabase(dwIndex);
      PCMMLNode   pNode, pBusiness;
      PWSTR pszName;
      pszName = PeopleBusinessIndexToName (dwIndex);
      if (!pszName)
         continue;
      pNode = gpData->NodeGet (dwNode);
      if (!pNode)
         continue;
      pBusiness = BusinessFromPerson (dwNode);

      if (pNode->NameGet() && !_wcsicmp(pNode->NameGet(), gszBusinessNode)) {
         // it's a business
         if (!pafRelations[1]) // "business"
            goto release;  // don't want to show businesses
      }
      else {
         // it's a person

         // make sure they're an acceptable type
         PWSTR pszRelation;
         pszRelation = NodeValueGet (pNode, gszRelationship);
         DWORD j;
         for (j = 0; j < NUMRELATIONS; j++) {
            if (pszRelation && !_wcsicmp(pszRelation, gaszRelations[j])) {
               // matches. If FALSE then done, else continue
               if (pafRelations[j])
                  break;
               else
                  goto release;
            }
         }
         // if we can't find it, then classify it as miscellaneous and see if
         // the user wants that included
         if (j >= NUMRELATIONS)
            if (!pafRelations[5])
               goto release;
      }

      // see how many phone #s and address books
      PWSTR pszHomePhone, pszWorkPhone, pszMobilePhone, pszHomeAddress, pszWorkAddress;
      DWORD dwNumPhone, dwNumAddress;
      pszHomePhone = NodeValueGet (pNode, gszHomePhone);
      if (pszHomePhone && !pszHomePhone[0])
         pszHomePhone = NULL;
      pszWorkPhone = NodeValueGet (pNode, gszWorkPhone);
      if (pszWorkPhone && !pszWorkPhone[0])
         pszWorkPhone = NULL;
      pszMobilePhone = NodeValueGet (pNode, gszMobilePhone);
      if (pszMobilePhone && !pszMobilePhone[0])
         pszMobilePhone = NULL;
      pszHomeAddress = NodeValueGet (pNode, gszHomeAddress);
      if (pszHomeAddress && !pszHomeAddress[0])
         pszHomeAddress = NULL;
      pszWorkAddress = NodeValueGet (pNode, gszWordAddress);
      if (pszWorkAddress && !pszWorkAddress[0])
         pszWorkAddress = NULL;

      // BUGFIX - Use business address if not listed
      if (pBusiness && !pszWorkPhone) {
         pszWorkPhone = NodeValueGet (pBusiness, gszWorkPhone);
         if (pszWorkPhone && !pszWorkPhone[0])
            pszWorkPhone = NULL;
      }
      if (pBusiness && !pszWorkAddress) {
         pszWorkAddress = NodeValueGet (pBusiness, gszWordAddress);
         if (pszWorkAddress && !pszWorkAddress[0])
            pszWorkAddress = NULL;
      }

      dwNumPhone = (pszHomePhone ? 1 : 0) + (pszWorkPhone ? 1 : 0) + (pszMobilePhone ? 1 : 0);
      dwNumAddress = (pszHomeAddress ? 1 : 0) + (pszWorkAddress ? 1 : 0);
      if (!dwFields)
         dwNumAddress = 0; // if not supposed to show addresses then dont

      // if there's nothing to show from this person then skip
      if (!dwNumPhone && !dwNumAddress)
         goto release;

      // see what his most recent contact with user was
      PCListFixed pl;
      pl = NodeListGet (pNode, gszInteraction, TRUE);
      NLG *pnlg;
      DFDATE   last;
      last = 0;
      if (pl) {
         if (pl->Num()) {
            pnlg = (NLG*) pl->Get(pl->Num()-1);
            last = pnlg->date;
         }
         delete pl;
      }
      // cutoff point
      DFDATE cutoff, today;
      today = Today();
      cutoff = TODFDATE (DAYFROMDFDATE(today), MONTHFROMDFDATE(today), (YEARFROMDFDATE(today)-dwNumYears));
      // get rid of if last interaction is older then cutoff. However, if never
      // interacted then keep just out of paranoia
      if (last && (last < cutoff))
         goto release;

      // else, print it out
      MemCat (&gMemTemp, L"<tr>");
      MemCat (&gMemTemp, L"<td>");
      MemCatSanitize (&gMemTemp, pszName);
      dwPrinted++;
      MemCat (&gMemTemp, L"</td>");
      if (dwNumPhone) {
         DWORD dwLeft = dwNumPhone;
         MemCat (&gMemTemp, L"<td><align align=right>");
         if (pszHomePhone) {
            MemCatSanitize (&gMemTemp, pszHomePhone);
            if (dwNumPhone > 1)
               MemCat (&gMemTemp, L"(H)");
            dwLeft--;
            if (dwLeft)
               MemCat (&gMemTemp, L"<br/>");
         }
         if (pszWorkPhone) {
            MemCatSanitize (&gMemTemp, pszWorkPhone);
            if (dwNumPhone > 1)
               MemCat (&gMemTemp, L"(W)");
            dwLeft--;
            if (dwLeft)
               MemCat (&gMemTemp, L"<br/>");
         }
         if (pszMobilePhone) {
            MemCatSanitize (&gMemTemp, pszMobilePhone);
            if (dwNumPhone > 1)
               MemCat (&gMemTemp, L"(M)");
            dwLeft--;
            if (dwLeft)
               MemCat (&gMemTemp, L"<br/>");
         }
         MemCat (&gMemTemp, L"</align></td>");
      }
      MemCat (&gMemTemp, L"</tr>");

      // show addresses
      if (dwNumAddress) {
         MemCat (&gMemTemp, L"<tr><td width=20%%/>");
         if (pszHomeAddress) {
            MemCat (&gMemTemp, L"<td width=");
            MemCat (&gMemTemp, (int) 80 / (int)dwNumAddress);
            MemCat (&gMemTemp, L"%%><align parlinespacing=0>");
            MemCatSanitize (&gMemTemp, pszHomeAddress);
            MemCat (&gMemTemp, L"</align></td>");
         }
         if (pszWorkAddress) {
            MemCat (&gMemTemp, L"<td width=");
            MemCat (&gMemTemp, (int) 80 / (int)dwNumAddress);
            MemCat (&gMemTemp, L"%%><align parlinespacing=0>");
            MemCatSanitize (&gMemTemp, pszWorkAddress);
            MemCat (&gMemTemp, L"</align></td>");
         }
         MemCat (&gMemTemp, L"</tr>");
      }


release:
      // release
      gpData->NodeRelease (pNode);
      if (pBusiness)
         gpData->NodeRelease (pBusiness);
   }


   // end the table
   if (!dwPrinted)
      MemCat (&gMemTemp, L"<tr><td>No acceptable address book entries.</td></tr>");
   MemCat (&gMemTemp, L"</table></font>");

#if 0
   // doing test show of UI
   CEscWindow  cWindow;
   RECT  r;
   DialogRect (&r);
   cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0, &r);
   cWindow.PageDialog ((PWSTR) gMemTemp.p, NULL);
#endif // 0

   // print dialog box
   PRINTDLG pd;
   memset (&pd, 0, sizeof(pd));
   pd.lStructSize = sizeof(pd);
   pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
   pd.hwndOwner = pPage->m_pWindow->m_hWnd;
   pd.nFromPage = 0;
   pd.nToPage = 0;
   pd.nCopies = 1;

   if (!PrintDlg (&pd))
      return;  // cancelled or error

   // print
   CEscWindow cWindow;
   cWindow.InitForPrint (pd.hDC, ghInstance, pPage->m_pWindow->m_hWnd);
   cWindow.m_PrintInfo.fColumnLine = TRUE;
   cWindow.m_PrintInfo.fFooterLine = FALSE;
   cWindow.m_PrintInfo.fHeaderLine = FALSE;
   cWindow.m_PrintInfo.fRowLine = TRUE;
   cWindow.m_PrintInfo.iColumnSepX = 20 * 72 / 8;  // 1/8"
   cWindow.m_PrintInfo.iFooterSepY = 0;
   cWindow.m_PrintInfo.iHeaderSepY = 0;
   cWindow.m_PrintInfo.iRowSepY = cWindow.m_PrintInfo.iColumnSepX;
   cWindow.m_PrintInfo.pszFooter = NULL;
   cWindow.m_PrintInfo.pszHeader = NULL;
   cWindow.m_PrintInfo.rPageMargin.left = cWindow.m_PrintInfo.rPageMargin.right =
      cWindow.m_PrintInfo.rPageMargin.top = cWindow.m_PrintInfo.rPageMargin.bottom =
      20 * 72 / 4;   // 1/4"
   cWindow.m_PrintInfo.wColumns = (WORD) dwWidth;
   cWindow.m_PrintInfo.wFontScale = 0x100;
   cWindow.m_PrintInfo.wOtherScale = 0x100;
   cWindow.m_PrintInfo.wRows = (WORD) dwHeight;
   cWindow.PrintPageLoad (ghInstance, (PWSTR) gMemTemp.p, NULL);
   if (!cWindow.Print ())
      pPage->MBError (L"Printing didn't work.");
   
   // free printing
   DeleteDC (pd.hDC);
}

/***********************************************************************
PeoplePrintPage - Page callback for printing
*/
BOOL PeoplePrintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it's back or starts with r: then ask if they're sure wnat to go
         if (!_wcsicmp(p->psz, gszPrint)) {
            // get the values and print
            PCEscControl pControl;
            BOOL afRelations[NUMRELATIONS];
            DWORD i;
            memset (afRelations, 0, sizeof(afRelations));
            for (i = 0; i < NUMRELATIONS; i++) {
               pControl = pPage->ControlFind(gaszRelations[i]);
               if (!pControl) continue;
               afRelations[i] = pControl->AttribGetBOOL(gszChecked);
            }

            DWORD dwNumYears, dwFields, dwWidth, dwHeight, dwFontSize;
            dwNumYears = (DWORD) GetCBValue (pPage, L"NumYears", 1);
            pControl = pPage->ControlFind(L"fields");
            dwFields = pControl ? (DWORD)pControl->AttribGetInt(gszCurSel) : 0;
            dwWidth = (DWORD) GetCBValue (pPage, L"width", 2);
            dwHeight = (DWORD) GetCBValue (pPage, L"height", 3);
            dwFontSize = (DWORD) GetCBValue (pPage, L"size", 6);

            // print
            PrintAddressBook (pPage, afRelations, dwNumYears, dwFields, dwWidth,
               dwHeight, dwFontSize);

            return TRUE;
         }


         break;
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************
PersonLinkToJournal - Given a person node, if the person has a link to a jouranl,
this writes an entry in the journal, linking the meeting/phone call.

inputs
   DWORD    dwPerson - person node
   PWSTR    pszName - Name to put in as link
   DWORD    dwEntry - Entry number to link to
   DFDATE   dfDate - Date
   DFTIME   dfStart, dfEnd - Start and end time.
returns
   BOOL - TRUE if succeded. FALSE if person doesn't have any link
*/
BOOL PersonLinkToJournal (DWORD dwPerson, PWSTR pszName, DWORD dwEntry, DFDATE dfDate,
                  DFTIME dfStart, DFTIME dfEnd)
{
   HANGFUNCIN;
   PCMMLNode pPerson;
   pPerson = gpData->NodeGet (dwPerson);
   if (!pPerson) {
      return FALSE;  // error
   }

   // get the journal node
   PWSTR pszJournal;
   int iJournalID;
   iJournalID = -1;
   pszJournal = NodeValueGet (pPerson, gszJournal, &iJournalID);
   gpData->NodeRelease (pPerson);
   if (!pszJournal || (iJournalID < 0))
      return FALSE;

   // write it
   JournalLink ((DWORD)iJournalID, pszName, dwEntry, dfDate, dfStart, dfEnd);
   return TRUE;
}


// BUGBUG - 2.0 - Ability to export comma delimited address

// BUGBUG - 2.0 - Option to change a person entry to a business so that keep all the old history
// if had been using Dragonfly before added businesses


/***********************************************************************
DeconstructAddress - Deconstruct an address into line 1..N

inputs
   PWSTR    pszOrig - Original multi-line address.
   DWORD    dwLine - line that want. 0 based.
   PWSTR    pszFill - fill in
   DWORD    dwSize - size of pszFill in bytes
returns
   BOOL - TRUE if found
*/
BOOL DeconstructAddress (PWSTR pszOrig, DWORD dwLine, PWSTR pszFill, DWORD dwSize)
{
   HANGFUNCIN;
   PWSTR pszStart = pszOrig;
   pszFill[0] = 0;

   if (!pszOrig)
      return FALSE;

   // skip lines
   while (dwLine) {
      pszStart = wcschr (pszStart, L'\n');
      if (!pszStart)
         return FALSE;
      pszStart++;
      // if next is \r, and didn't have \r before this then skip
      if ( ((pszStart != pszOrig) && (pszStart[-1] != L'\r')) && (pszStart[0] == L'\r'))
         pszOrig++;
      if (!pszStart[0])
         return FALSE;  // blank
      dwLine--;
   }

   // copy
   while (pszStart[0]) {
      if (!pszStart[0] || (pszStart[0] == L'\r') || (pszStart[0] == L'\n'))
         break;

      if (dwSize <= 2)
         break;

      pszFill[0] = pszStart[0];
      pszFill++;
      dwSize -= 2;
      pszStart++;
   }
   pszFill[0] = 0;   // null terminate

   return TRUE;
}


/***********************************************************************
SaveAddressBook - Prints the address book.

inputs
   PCEscPage      pPage - for messages
   char           *pszFile - file name
   BOOL           *pafRleations - 8 bools indicating what relations chosen
   DWORD          dwNumYears - Number of years back to go
returns
   none
*/
typedef struct {
   char     *pszCSV;
   WCHAR    *pszMML;
} TOCSV, *PTOCSV;
static PSTR paszName = "Name";
static PSTR paszEmailAddress = "E-mail address";
static PSTR paszHomeAddress1 = "Home address line 1";
static PSTR paszHomeAddress2 = "Home address line 2";
static PSTR paszHomeAddress3 = "Home address line 3";
static PSTR paszHomeAddress4 = "Home address line 4";
static PSTR paszHomeAddress5 = "Home address line 5";
static PSTR paszBusinessAddress1 = "Business address line 1";
static PSTR paszBusinessAddress2 = "Business address line 2";
static PSTR paszBusinessAddress3 = "Business address line 3";
static PSTR paszBusinessAddress4 = "Business address line 4";
static PSTR paszBusinessAddress5 = "Business address line 5";

void SaveAddressBook (PCEscPage pPage, char *pszFile, BOOL *pafRelations, DWORD dwNumYears)
{
   HANGFUNCIN;
   // conversion from variable stored in person's entry, to CSV. First is string in MML, second is title
   TOCSV apCSV[] = {
      "First Name", gszFirstName,
      "Last Name", gszLastName,
      //"Middle Name", L"",
      paszName, NULL,     // NULL indicates special processing
      paszEmailAddress, NULL, // NULL indicates special processing
      "Home Address", gszHomeAddress,
      paszHomeAddress1, NULL,
      paszHomeAddress2, NULL,
      paszHomeAddress3, NULL,
      paszHomeAddress4, NULL,
      paszHomeAddress5, NULL,
      "Home Phone", gszHomePhone,
      "Home FAX", gszFAXPhone,
      "Mobile Phone", gszMobilePhone,
      "Personal Web Page", gszPersonalWeb,
      "Business Address", gszWordAddress,
      paszBusinessAddress1, NULL,
      paszBusinessAddress2, NULL,
      paszBusinessAddress3, NULL,
      paszBusinessAddress4, NULL,
      paszBusinessAddress5, NULL,
      "Business Phone", gszWorkPhone,
      "Business FAX", gszFAXPhone,
      "Company", gszCompany,
      "Job Title", gszJobTitle,
      "Office Location", gszOffice,
      "Gender", gszGender,
      "Personal E-mail", gszPersonalEmail,
      "Business E-mail", gszBusinessEmail,
      "Department", gszDepartment,
      "Manager", gszManager,
      "Assistant", gszAssistant,
      "Relationship", gszRelationship,
      "Spouse", gszSpouse,
      "Child1", gszChild1,
      "Child2", gszChild2,
      "Child3", gszChild3,
      "Child4", gszChild4,
      "Child5", gszChild5,
      "Child6", gszChild6,
      "Child7", gszChild7,
      "Child8", gszChild8,
      "Mother", gszMother,
      "Father", gszFather
   };
   
   // sort the list alpabetically
   PeopleFilteredList();
   CListFixed list;
   list.Init (sizeof(DWORD));
   DWORD i;
   // BUGFIX - Show people and business
   for (i = 0; i < glistPeopleBusiness.Num(); i++)
      list.Add (&i);
   qsort (list.Get(0), list.Num(), sizeof(DWORD), PeopleSort);

   // open the file and write out the header
   FILE *f;
   f = fopen(pszFile, "wt");
   if (!f) {
      pPage->MBWarning (L"The file couldn't be written.",
         L"The file may already be opened by another application or write protected.");
      return;
   }
   for (i = 0; i < sizeof(apCSV) / sizeof(TOCSV); i++)
      fprintf (f, i ? ",%s" : "%s", apCSV[i].pszCSV);
   fprintf (f, "\n");

   DWORD dwPrinted;
   dwPrinted = 0;

   // load in people and print
   for (i = 0; i < list.Num(); i++) {
      DWORD dwIndex, dwNode;
      dwIndex = *((DWORD*) list.Get(i));
      dwNode = PeopleBusinessIndexToDatabase(dwIndex);
      PCMMLNode   pNode;
      PWSTR pszName;
      pszName = PeopleBusinessIndexToName (dwIndex);
      if (!pszName)
         continue;
      pNode = gpData->NodeGet (dwNode);
      if (!pNode)
         continue;

      if (pNode->NameGet() && !_wcsicmp(pNode->NameGet(), gszBusinessNode)) {
         // it's a business
         if (!pafRelations[1]) // "business"
            goto release;  // don't want to show businesses
      }
      else {
         // it's a person

         // make sure they're an acceptable type
         PWSTR pszRelation;
         pszRelation = NodeValueGet (pNode, gszRelationship);
         DWORD j;
         for (j = 0; j < NUMRELATIONS; j++) {
            if (pszRelation && !_wcsicmp(pszRelation, gaszRelations[j])) {
               // matches. If FALSE then done, else continue
               if (pafRelations[j])
                  break;
               else
                  goto release;
            }
         }
         // if we can't find it, then classify it as miscellaneous and see if
         // the user wants that included
         if (j >= NUMRELATIONS)
            if (!pafRelations[5])
               goto release;
      }
      // see what his most recent contact with user was
      PCListFixed pl;
      pl = NodeListGet (pNode, gszInteraction, TRUE);
      NLG *pnlg;
      DFDATE   last;
      last = 0;
      if (pl) {
         if (pl->Num()) {
            pnlg = (NLG*) pl->Get(pl->Num()-1);
            last = pnlg->date;
         }
         delete pl;
      }
      // cutoff point
      DFDATE cutoff, today;
      today = Today();
      cutoff = TODFDATE (DAYFROMDFDATE(today), MONTHFROMDFDATE(today), (YEARFROMDFDATE(today)-dwNumYears));
      // get rid of if last interaction is older then cutoff. However, if never
      // interacted then keep just out of paranoia
      if (last && (last < cutoff))
         goto release;

      dwPrinted++;

      // write out the person info
      DWORD dwItem;
      WCHAR szTemp[256];
      for (dwItem = 0; dwItem < sizeof(apCSV) / sizeof(TOCSV); dwItem++) {
         PWSTR pszMML;
         if (apCSV[dwItem].pszMML) {
            pszMML = NodeValueGet (pNode, apCSV[dwItem].pszMML);
         }
         else {
            // it's a special case
            pszMML = NULL;
            if (apCSV[dwItem].pszCSV == paszName) {
               PWSTR pszFirst, pszLast;
               pszFirst = NodeValueGet (pNode, gszFirstName);
               pszLast = NodeValueGet(pNode, gszLastName);
               // NOTE: Do not use nickname here

               // first then last, since that's normal for CSV
               szTemp[0] = 0;
               if (pszFirst)
                  wcscat (szTemp, pszFirst);
               if (pszLast) {
                  if (szTemp[0])
                     wcscat (szTemp, L" ");
                  wcscat (szTemp, pszLast);
               }
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszEmailAddress) {
               // use personal. If not, business
               pszMML = NodeValueGet (pNode, gszPersonalEmail);
               if (!pszMML)
                  pszMML = NodeValueGet (pNode, gszBusinessEmail);
            }
            else if (apCSV[dwItem].pszCSV == paszHomeAddress1) {
               pszMML = NodeValueGet (pNode, gszHomeAddress);
               DeconstructAddress (pszMML, 0, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszHomeAddress2) {
               pszMML = NodeValueGet (pNode, gszHomeAddress);
               DeconstructAddress (pszMML, 1, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszHomeAddress3) {
               pszMML = NodeValueGet (pNode, gszHomeAddress);
               DeconstructAddress (pszMML, 2, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszHomeAddress4) {
               pszMML = NodeValueGet (pNode, gszHomeAddress);
               DeconstructAddress (pszMML, 3, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszHomeAddress5) {
               pszMML = NodeValueGet (pNode, gszHomeAddress);
               DeconstructAddress (pszMML, 4, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszBusinessAddress1) {
               pszMML = NodeValueGet (pNode, gszWordAddress);
               DeconstructAddress (pszMML, 0, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszBusinessAddress2) {
               pszMML = NodeValueGet (pNode, gszWordAddress);
               DeconstructAddress (pszMML, 1, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszBusinessAddress3) {
               pszMML = NodeValueGet (pNode, gszWordAddress);
               DeconstructAddress (pszMML, 2, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszBusinessAddress4) {
               pszMML = NodeValueGet (pNode, gszWordAddress);
               DeconstructAddress (pszMML, 3, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
            else if (apCSV[dwItem].pszCSV == paszBusinessAddress5) {
               pszMML = NodeValueGet (pNode, gszWordAddress);
               DeconstructAddress (pszMML, 4, szTemp, sizeof(szTemp));
               pszMML = szTemp;
            }
         }
         if (!pszMML)
            pszMML = L"";

         // convert this to ASCII
         gMemTemp.Required ((wcslen(pszMML)+1)*2 + 6);   // +6 for just a bit extra
         WideCharToMultiByte (CP_ACP, 0, pszMML, -1, (char*)gMemTemp.p, gMemTemp.m_dwAllocated, 0, 0);

         // if it's comma separated then get rid of commas
         char *pc;
         BOOL fComma;
         fComma = FALSE;
         for (pc = (char*) gMemTemp.p; *pc; pc++) {
            if (pc[0] == ',')
               fComma = TRUE;
            if ((pc[0] == '\r') || (pc[0] == '\n'))
               pc[0] = ' ';
            if (pc[0] == '"')
               pc[0] = '\'';
         }
         // write it out
         if (fComma) {
            // put quotes around it
            fprintf (f, dwItem ? ",\"%s\"" : "\"%s\"", (char*) gMemTemp.p);
         }
         else {
            fprintf (f, dwItem ? ",%s" : "%s", (char*) gMemTemp.p);
         }
      }
      fprintf (f, "\n");

release:
      // release
      gpData->NodeRelease (pNode);
   }

   fclose (f);

   WCHAR szw[256];
   swprintf (szw, L"%d address book entries were saved into the file.", dwPrinted);
   pPage->MBInformation (L"Address book saved.", szw);
}
/***********************************************************************
PeopleExportPage - Lists people in the address book.
*/

BOOL PeopleExportPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;   // default behaviour

         // if it's back or starts with r: then ask if they're sure wnat to go
         if (!_wcsicmp(p->psz, L"save")) {
            // get the values and print
            PCEscControl pControl;
            BOOL afRelations[NUMRELATIONS];
            DWORD i;
            memset (afRelations, 0, sizeof(afRelations));
            for (i = 0; i < NUMRELATIONS; i++) {
               pControl = pPage->ControlFind(gaszRelations[i]);
               if (!pControl) continue;
               afRelations[i] = pControl->AttribGetBOOL(gszChecked);
            }

            DWORD dwNumYears;
            dwNumYears = (DWORD) GetCBValue (pPage, L"NumYears", 1);

            // get the file name
            char  szTemp[256];
            strcpy (szTemp, "Dragonfly addresses.csv");

            OPENFILENAME ofn;
            memset (&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;

            ofn.lpstrFilter =
               "Comma separated values (*.csv)\0*.csv\0"
               "Text file (*.txt)\0*.txt\0"
               "\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Save comman-separated address-book file";
            ofn.lpstrDefExt = "csv";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

            // nFileExtension 
            if (!GetSaveFileName(&ofn))
               return TRUE;

            SaveAddressBook (pPage, szTemp, afRelations, dwNumYears);

            return TRUE;
         }


         break;
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}
