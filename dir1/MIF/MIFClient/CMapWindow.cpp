/*************************************************************************************
CMapWindow.cpp - Code for managing the map window.

begun 4/6/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <zmouse.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"



#define DEFAULTROOMCOLOR_LIGHT            RGB(0xe0, 0xe0, 0xe0)
#define DEFAULTROOMCOLOR_DARK             RGB(0x60, 0x60, 0x60)


#define TIMER_SCROLLMAP          2068     // scroll
#define TIMERSCROLLMAP_INTERVAL  100      // 100 milliseconds
#define TIMERSCROLLMAP_DELAY     (500 / TIMERSCROLLMAP_INTERVAL)     // time before actually scroll
#define TIMERSCROLLMAP_RATE      2000      // scroll all the way across in this time

#define ROOMUPDATETIME           500      // room updates must come within 1/2 second

/*************************************************************************************
CMapRoom::Constructor and destructor
*/
CMapRoom::CMapRoom (void)
{
   m_gID = GUID_NULL;
   MemZero (&m_memName);
   MemZero (&m_memDescription);
   m_pLoc.Zero4();
   m_dwShape = 0;
   m_acColor[0] = DEFAULTROOMCOLOR_DARK;
   m_acColor[1] = DEFAULTROOMCOLOR_LIGHT;
   m_fRotation = 0;
   memset (m_agExits, 0, sizeof(m_agExits));
   m_pZone = NULL;
   m_pRegion = NULL;
   m_pMap = NULL;
}

CMapRoom::~CMapRoom (void)
{
   // nothing for now
}


static PWSTR gpszRoom = L"Room";
static PWSTR gpszID = L"ID";
static PWSTR gpszName = L"Name";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszColorDark = L"ColorDark";
static PWSTR gpszColorLight = L"ColorLight";
static PWSTR gpszRotation = L"Rotation";
static PWSTR gpszExit = L"Exit";
static PWSTR gpszDirection = L"Direction";
static PWSTR gpszTo = L"To";
static PWSTR gpszDescription = L"Description";

/*************************************************************************************
CMapRoom::MMLTo - Standard API
*/
PCMMLNode2 CMapRoom::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszRoom);

   MMLValueSet (pNode, gpszID, (PBYTE)&m_gID, sizeof(m_gID));
   PWSTR psz = (PWSTR)m_memName.p;
   if (psz[0])
      MMLValueSet (pNode, gpszName, psz);
   psz = (PWSTR)m_memDescription.p;
   if (psz[0])
      MMLValueSet (pNode, gpszDescription, psz);
   MMLValueSet (pNode, gpszLoc, &m_pLoc);
   if (m_dwShape)
      MMLValueSet (pNode, gpszShape, (int)m_dwShape);
   if (m_acColor[0] != DEFAULTROOMCOLOR_DARK) {
      WCHAR szTemp[32];
      ColorToAttrib (szTemp, m_acColor[0]);
      MMLValueSet (pNode, gpszColorDark, szTemp);
   }
   if (m_acColor[1] != DEFAULTROOMCOLOR_LIGHT) {
      WCHAR szTemp[32];
      ColorToAttrib (szTemp, m_acColor[1]);
      MMLValueSet (pNode, gpszColorLight, szTemp);
   }
   if (m_fRotation)
      MMLValueSet (pNode, gpszRotation, m_fRotation);

   DWORD i;
   for (i = 0; i < ROOMEXITS; i++) {
      if (IsEqualGUID (m_agExits[i], GUID_NULL))
         continue;

      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszExit);
      MMLValueSet (pSub, gpszDirection, (int)i);
      MMLValueSet (pSub, gpszTo, (PBYTE)&m_agExits[i], sizeof(m_agExits[i]));
   }

   return pNode;
}


/*************************************************************************************
CMapRoom::DeleteAll - Deletes all the objects this holds.

inputs
   PCHashGUID     phRooms - If not NULL, then when a room is deleted it will
                  remove itself from the list of roooms.
returns
   none
*/
void CMapRoom::DeleteAll (PCHashGUID phRooms)
{
   if (phRooms) {
      DWORD dwIndex = phRooms->FindIndex (&m_gID);
      phRooms->Remove (dwIndex);
   }
}

/*************************************************************************************
CMapRoom::MMLFrom - Standard API, except

inputs:
   PCMapZone         pZone - Zone where to write this
   PCMapRegion       pRegion - Region where to write this
   PCMapMap          pMap - Map where to write this
   PCHashGUID        phRooms - Hash of GUIDs where to write the objects.
                     Every new object will be added to the hash. (This can be NULL)
*/
BOOL CMapRoom::MMLFrom (PCMMLNode2 pNode, PCMapZone pZone, PCMapRegion pRegion, PCMapMap pMap,
                        PCHashGUID phRooms)
{
   m_gID = GUID_NULL;
   MMLValueGetBinary (pNode, gpszID, (PBYTE)&m_gID, sizeof(m_gID));
   if (IsEqualGUID (m_gID, GUID_NULL))
      return FALSE;

   PWSTR psz;
   MemZero (&m_memName);
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   MemZero (&m_memDescription);
   psz = MMLValueGet (pNode, gpszDescription);
   if (psz)
      MemCat (&m_memDescription, psz);

   m_pLoc.Zero4();
   MMLValueGetPoint (pNode, gpszLoc, &m_pLoc);

   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int)0);

   psz = MMLValueGet (pNode, gpszColorDark);
   if (!psz || !AttribToColor (psz, &m_acColor[0]))
      m_acColor[0] = DEFAULTROOMCOLOR_DARK;
   psz = MMLValueGet (pNode, gpszColorLight);
   if (!psz || !AttribToColor (psz, &m_acColor[1]))
      m_acColor[1] = DEFAULTROOMCOLOR_LIGHT;

   m_fRotation = MMLValueGetDouble (pNode, gpszRotation, 0);

   memset (m_agExits, 0, sizeof(m_agExits));
   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, gpszExit)) {
         DWORD dwDir = MMLValueGetInt (pSub, gpszDirection, 0);
         if (dwDir >= ROOMEXITS)
            continue;

         MMLValueGetBinary (pSub, gpszTo, (PBYTE)&m_agExits[dwDir], sizeof(m_agExits[dwDir]));
      }

   } // i

   // add to list
   m_pZone = pZone;
   m_pRegion = pRegion;
   m_pMap = pMap;
   if (phRooms) {
      PCMapRoom pThis = this;
      // NOTE: Assuming that one doesn't already exist in the hash
      phRooms->Add (&m_gID, &pThis);
   }

   return TRUE;
}





/*************************************************************************************
CMapMap::Constructor and destructor
*/
CMapMap::CMapMap (void)
{
   MemZero (&m_memName);
   m_lPCMapRoom.Init (sizeof(PCMapRoom));
   m_pZone = NULL;
   m_pRegion = NULL;
}

CMapMap::~CMapMap (void)
{
   DeleteAll (NULL);
}


/*************************************************************************************
CMapMap::DeleteAll - Deletes all the objects this holds.

inputs
   PCHashGUID     phRooms - If not NULL, then when a room is deleted it will
                  remove itself from the list of roooms.
returns
   none
*/
void CMapMap::DeleteAll (PCHashGUID phRooms)
{
   DWORD i;
   PCMapRoom *ppm = (PCMapRoom*)m_lPCMapRoom.Get(0);
   for (i = 0; i < m_lPCMapRoom.Num(); i++) {
      if (phRooms)
         ppm[i]->DeleteAll (phRooms);
      delete ppm[i];
   } // i
   m_lPCMapRoom.Clear();
}

static PWSTR gpszMap = L"Map";

/*************************************************************************************
CMapMap::MMLTo - Standard API
*/
PCMMLNode2 CMapMap::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMap);

   PWSTR psz = (PWSTR)m_memName.p;
   if (psz[0])
      MMLValueSet (pNode, gpszName, psz);

   DWORD i;
   PCMapRoom *ppm = (PCMapRoom*)m_lPCMapRoom.Get(0);
   for (i = 0; i < m_lPCMapRoom.Num(); i++) {
      PCMMLNode2 pSub = ppm[i]->MMLTo ();
      if (!pSub)
         continue;
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CMapMap::MMLFrom - Standard API, except

inputs:
   PCMapZone         pZone - Zone where to write this
   PCMapRegion       pRegion - Region where to write this
   PCHashGUID        phRooms - Hash of GUIDs where to write the objects.
                     Every new object will be added to the hash. (This can be NULL)
*/
BOOL CMapMap::MMLFrom (PCMMLNode2 pNode, PCMapZone pZone, PCMapRegion pRegion, PCHashGUID phRooms)
{
   DeleteAll (phRooms);

   MemZero (&m_memName);
   PWSTR psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, gpszRoom)) {
         PCMapRoom pRoom = new CMapRoom;
         if (!pRoom)
            return FALSE;
         if (!pRoom->MMLFrom (pSub, pZone, pRegion, this, phRooms)) {
            delete pRoom;
            return FALSE;
         }
         m_lPCMapRoom.Add (&pRoom);
      }
   } // i

   m_pZone = pZone;
   m_pRegion = pRegion;

   return TRUE;
}




/*************************************************************************************
CMapMap::RoomAdd - Adds a room to the map. NOTE: The caller should also add
it to the CHashGUID of rooms.

inputs
   PCMapROom         pRoom - Room to add
returns
   BOOL - TRUE if success
*/
BOOL CMapMap::RoomAdd (PCMapRoom pRoom)
{
   m_lPCMapRoom.Add (&pRoom);
   return TRUE;
}



/*************************************************************************************
CMapMap::RoomRemove - Removes a room from the list.

inputs
   DWORD          dwIndex - Index of the room
   BOOL           fDelete - if TRUE then delete the room
   PCHashGUID     phRooms - If not NULL, and deleting, then remove all rooms
                     from the phRooms list
returns
   BOOL - TRUE if success
*/
BOOL CMapMap::RoomRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms)
{
   if (dwIndex >= m_lPCMapRoom.Num())
      return FALSE;

   if (fDelete) {
      // NOTE: no tested
      PCMapRoom pm = *((PCMapRoom*)m_lPCMapRoom.Get(dwIndex));
      pm->DeleteAll (phRooms);
      delete pm;
   }

   m_lPCMapRoom.Remove (dwIndex);

   return TRUE;
}

/*************************************************************************************
CMapMap::RoomNum - Returns the number of rooms
*/
DWORD CMapMap::RoomNum (void)
{
   return m_lPCMapRoom.Num();
}


/*************************************************************************************
CMapMap::RoomGet - Gets a room by index

inputs
   DWORD          dwIndex - Index
returns
   PCMapRoom - Room
*/
PCMapRoom CMapMap::RoomGet (DWORD dwIndex)
{
   PCMapRoom *ppm = (PCMapRoom*)m_lPCMapRoom.Get(dwIndex);
   if (!ppm)
      return NULL;
   return ppm[0];
}



/*************************************************************************************
CMapMap::RoomFindIndex - Finds a given room.

inputs
   GUID           *pgID - ID
returns
   DWORD - Index, or -1 if can't find
*/
DWORD CMapMap::RoomFindIndex (GUID *pgID)
{
   DWORD i;
   PCMapRoom *ppm = (PCMapRoom*)m_lPCMapRoom.Get(0);
   for (i = 0; i < m_lPCMapRoom.Num(); i++) {
      if (IsEqualGUID (ppm[i]->m_gID,*pgID))
         return i;
   } // i

   return -1;
}



/*************************************************************************************
CMapMap::RoomFind - Finds a room.

inputs
   GUID           *pgID - ID
returns
   PCMapRoom - Room. NULL if can't find
*/
PCMapRoom CMapMap::RoomFind (GUID *pgID)
{
   return RoomGet(RoomFindIndex (pgID));
}



/*************************************************************************************
CMapMap::Size - Looks over all the rooms to figure out how large this map
needs to be.

inputs
   PCPoint     pSize - p[0] filled with left, p[1] right, [2] top (min y), [3] bottom (max y)
returns
   BOOL - TRUE if success, FALSE if there are no rooms (but will fill size with 0,1,0,1 so have something)
*/
BOOL CMapMap::Size (PCPoint pSize)
{
   pSize->Zero4();

   DWORD i;
   PCMapRoom *ppm = (PCMapRoom*)m_lPCMapRoom.Get(0);
   CPoint pRoom;
   for (i = 0; i < m_lPCMapRoom.Num(); i++) {
      // size of the room
      pRoom.p[0] = ppm[i]->m_pLoc.p[0] - ppm[i]->m_pLoc.p[2];  // should be /2, but add some extra
      pRoom.p[1] = ppm[i]->m_pLoc.p[0] + ppm[i]->m_pLoc.p[2];
      pRoom.p[2] = ppm[i]->m_pLoc.p[1] - ppm[i]->m_pLoc.p[3];
      pRoom.p[3] = ppm[i]->m_pLoc.p[1] + ppm[i]->m_pLoc.p[3];

      if (i) {
         pSize->p[0] = min(pSize->p[0], pRoom.p[0]);
         pSize->p[1] = max(pSize->p[1], pRoom.p[1]);
         pSize->p[2] = min(pSize->p[2], pRoom.p[2]);
         pSize->p[3] = max(pSize->p[3], pRoom.p[3]);
      }
      else
         pSize->Copy (&pRoom);
   } // i

   if ((pSize->p[0] == pSize->p[1]) || (pSize->p[2] == pSize->p[3])) {
      pSize->p[1] += 1;
      pSize->p[3] += 1;
      return FALSE;
   }

   // allow a bit center
   fp fWidth = (pSize->p[1] - pSize->p[0]) / 4.0;
   fp fHeight = (pSize->p[3] - pSize->p[2]) / 4.0;
   pSize->p[0] -= fWidth;
   pSize->p[1] += fWidth;
   pSize->p[2] -= fHeight;
   pSize->p[3] += fHeight;

   return TRUE;
}








/*************************************************************************************
CMapRegion::Constructor and destructor
*/
CMapRegion::CMapRegion (void)
{
   MemZero (&m_memName);
   m_lPCMapMap.Init (sizeof(PCMapMap));
   m_pZone = NULL;
}

CMapRegion::~CMapRegion (void)
{
   DeleteAll (NULL);
}


/*************************************************************************************
CMapRegion::DeleteAll - Deletes all the objects this holds.

inputs
   PCHashGUID     phRooms - If not NULL, then when a Map is deleted it will
                  remove itself from the list of roooms.
returns
   none
*/
void CMapRegion::DeleteAll (PCHashGUID phRooms)
{
   DWORD i;
   PCMapMap *ppm = (PCMapMap*)m_lPCMapMap.Get(0);
   for (i = 0; i < m_lPCMapMap.Num(); i++) {
      ppm[i]->DeleteAll (phRooms);
      delete ppm[i];
   } // i
   m_lPCMapMap.Clear();
}

static PWSTR gpszRegion = L"Region";

/*************************************************************************************
CMapRegion::MMLTo - Standard API
*/
PCMMLNode2 CMapRegion::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszRegion);

   PWSTR psz = (PWSTR)m_memName.p;
   if (psz[0])
      MMLValueSet (pNode, gpszName, psz);

   DWORD i;
   PCMapMap *ppm = (PCMapMap*)m_lPCMapMap.Get(0);
   for (i = 0; i < m_lPCMapMap.Num(); i++) {
      PCMMLNode2 pSub = ppm[i]->MMLTo ();
      if (!pSub)
         continue;
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CMapRegion::MMLFrom - Standard API, except

inputs:
   PCMapZone         pZone - Zone where to write this
   PCHashGUID        phRooms - Hash of GUIDs where to write the objects.
                     Every new object will be added to the hash. (This can be NULL)
*/
BOOL CMapRegion::MMLFrom (PCMMLNode2 pNode, PCMapZone pZone, PCHashGUID phRooms)
{
   DeleteAll (phRooms);

   MemZero (&m_memName);
   PWSTR psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, gpszMap)) {
         PCMapMap pMap = new CMapMap;
         if (!pMap)
            return FALSE;
         if (!pMap->MMLFrom (pSub, pZone, this, phRooms)) {
            delete pMap;
            return FALSE;
         }
         pMap->m_pRegion = this;
         pMap->m_pZone = pZone;
         m_lPCMapMap.Add (&pMap);
      }
   } // i

   m_pZone = pZone;

   return TRUE;
}




/*************************************************************************************
CMapRegion::MapAdd - Adds a Map to the Region.

inputs
   PCMapMap         pMap - Map to add
returns
   BOOL - TRUE if success
*/
BOOL CMapRegion::MapAdd (PCMapMap pMap)
{
   m_lPCMapMap.Add (&pMap);
   return TRUE;
}



/*************************************************************************************
CMapRegion::MapRemove - Removes a Map from the list.

inputs
   DWORD          dwIndex - Index of the Map
   BOOL           fDelete - if TRUE then delete the Map
   PCHashGUID     phRooms - If not NULL, and deleting, then remove all Maps
                     from the phRooms list
returns
   BOOL - TRUE if success
*/
BOOL CMapRegion::MapRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms)
{
   if (dwIndex >= m_lPCMapMap.Num())
      return FALSE;

   if (fDelete) {
      PCMapMap pm = *((PCMapMap*)m_lPCMapMap.Get(dwIndex));
      pm->DeleteAll (phRooms);
      delete pm;
   }

   m_lPCMapMap.Remove (dwIndex);

   return TRUE;
}

/*************************************************************************************
CMapRegion::MapNum - Returns the number of Maps
*/
DWORD CMapRegion::MapNum (void)
{
   return m_lPCMapMap.Num();
}


/*************************************************************************************
CMapRegion::MapGet - Gets a Map by index

inputs
   DWORD          dwIndex - Index
returns
   PCMapMap - Map
*/
PCMapMap CMapRegion::MapGet (DWORD dwIndex)
{
   PCMapMap *ppm = (PCMapMap*)m_lPCMapMap.Get(dwIndex);
   if (!ppm)
      return NULL;
   return ppm[0];
}



/*************************************************************************************
CMapRegion::MapFindIndex - Finds a given Map.

inputs
   PWSTR          pszName - Name
   BOOL           fCreateIfNotExist - If can't find then create one
returns
   DWORD - Index, or -1 if can't find
*/
DWORD CMapRegion::MapFindIndex (PWSTR pszName, BOOL fCreateIfNotExist)
{
   DWORD i;
   PCMapMap *ppm = (PCMapMap*)m_lPCMapMap.Get(0);
   for (i = 0; i < m_lPCMapMap.Num(); i++) {
      if (!_wcsicmp((PWSTR)ppm[i]->m_memName.p, pszName))
         return i;
   } // i

   if (!fCreateIfNotExist)
      return -1;

   // else add
   PCMapMap pNew = new CMapMap;
   if (!pNew)
      return -1;
   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, pszName);
   pNew->m_pRegion = this;
   pNew->m_pZone = m_pZone;
   m_lPCMapMap.Add (&pNew);

   return m_lPCMapMap.Num()-1;
}



/*************************************************************************************
CMapRegion::MapFind - Finds a Map.

inputs
   GUID           *pgID - ID
returns
   PCMapMap - Map. NULL if can't find
*/
PCMapMap CMapRegion::MapFind (PWSTR pszName, BOOL fCreateIfNotExist)
{
   return MapGet(MapFindIndex (pszName, fCreateIfNotExist));
}


/*************************************************************************************
CMapRegion::PopupMenu - This creates a popup menu of all the rooms in the map.
If there is only one room, it doesn't create a popup menu, but returns a pointer
to the room.

inputs
   PCListFixed       plPCMapMap - List that's initialized to sizeof(PCMapRoom).
                        When a room is added to a popup, it will be added to this list.
                        The menu ID is 1000 + index into this list.
   PCMapMap          pCurMap - Current map (so can check it)
   PCMapMap          *ppMap - If the function returns NULL (non popup), then this
                        will be NULL or a single room from the list.
returns
   HMENU - Menu created, or NULL. This must be freed
*/
HMENU CMapRegion::PopupMenu (PCListFixed plPCMapMap, PCMapMap pCurMap, PCMapMap *ppMap)
{
   *ppMap = NULL;

   if (!MapNum())
      return NULL;   // no rooms
   if (MapNum() == 1) {
      *ppMap = MapGet(0);
      return NULL;
   }

   // else, multiple rooms
   DWORD i;
   CMem mem;
   BeepWindowBeep (ESCBEEP_MENUOPEN);
   HMENU hMenu = CreatePopupMenu ();
   if (!hMenu)
      return NULL;
   for (i = 0; i < MapNum(); i++) {
      PCMapMap pm = MapGet(i);

      // convert the name
      DWORD dwLen = ((DWORD)wcslen((PWSTR)pm->m_memName.p)+1)*sizeof(WCHAR);
      if (!mem.Required (dwLen))
         continue;
      WideCharToMultiByte (CP_ACP, 0, (PWSTR)pm->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

      // add to the menu
      plPCMapMap->Add (&pm);
      AppendMenu (hMenu,
         ((pCurMap == pm) ? MF_CHECKED : 0) | MF_STRING | MF_ENABLED,
         plPCMapMap->Num() - 1 + 1000, (char*)mem.p);
   } // i

   return hMenu;
}














/*************************************************************************************
CMapZone::Constructor and destructor
*/
CMapZone::CMapZone (void)
{
   MemZero (&m_memName);
   m_lPCMapRegion.Init (sizeof(PCMapRegion));
}

CMapZone::~CMapZone (void)
{
   DeleteAll (NULL);
}


/*************************************************************************************
CMapZone::DeleteAll - Deletes all the objects this holds.

inputs
   PCHashGUID     phRooms - If not NULL, then when a Region is deleted it will
                  remove itself from the list of roooms.
returns
   none
*/
void CMapZone::DeleteAll (PCHashGUID phRooms)
{
   DWORD i;
   PCMapRegion *ppm = (PCMapRegion*)m_lPCMapRegion.Get(0);
   for (i = 0; i < m_lPCMapRegion.Num(); i++) {
      ppm[i]->DeleteAll (phRooms);
      delete ppm[i];
   } // i
   m_lPCMapRegion.Clear();
}

static PWSTR gpszZone = L"Zone";

/*************************************************************************************
CMapZone::MMLTo - Standard API
*/
PCMMLNode2 CMapZone::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszZone);

   PWSTR psz = (PWSTR)m_memName.p;
   if (psz[0])
      MMLValueSet (pNode, gpszName, psz);

   DWORD i;
   PCMapRegion *ppm = (PCMapRegion*)m_lPCMapRegion.Get(0);
   for (i = 0; i < m_lPCMapRegion.Num(); i++) {
      PCMMLNode2 pSub = ppm[i]->MMLTo ();
      if (!pSub)
         continue;
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CMapZone::MMLFrom - Standard API, except

inputs:
   PCHashGUID        phRooms - Hash of GUIDs where to write the objects.
                     Every new object will be added to the hash. (This can be NULL)
*/
BOOL CMapZone::MMLFrom (PCMMLNode2 pNode, PCHashGUID phRooms)
{
   DeleteAll (phRooms);

   MemZero (&m_memName);
   PWSTR psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, gpszRegion)) {
         PCMapRegion pRegion = new CMapRegion;
         if (!pRegion)
            return FALSE;
         if (!pRegion->MMLFrom (pSub, this, phRooms)) {
            delete pRegion;
            return FALSE;
         }
         pRegion->m_pZone = this;
         m_lPCMapRegion.Add (&pRegion);
      }
   } // i

   return TRUE;
}




/*************************************************************************************
CMapZone::RegionAdd - Adds a Region to the Zone.

inputs
   PCMapRegion         pRegion - Region to add
returns
   BOOL - TRUE if success
*/
BOOL CMapZone::RegionAdd (PCMapRegion pRegion)
{
   m_lPCMapRegion.Add (&pRegion);
   return TRUE;
}



/*************************************************************************************
CMapZone::RegionRemove - Removes a Region from the list.

inputs
   DWORD          dwIndex - Index of the Region
   BOOL           fDelete - if TRUE then delete the Region
   PCHashGUID     phRooms - If not NULL, and deleting, then remove all Regions
                     from the phRooms list
returns
   BOOL - TRUE if success
*/
BOOL CMapZone::RegionRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms)
{
   if (dwIndex >= m_lPCMapRegion.Num())
      return FALSE;

   if (fDelete) {
      PCMapRegion pm = *((PCMapRegion*)m_lPCMapRegion.Get(dwIndex));
      pm->DeleteAll (phRooms);
      delete pm;
   }

   m_lPCMapRegion.Remove (dwIndex);

   return TRUE;
}

/*************************************************************************************
CMapZone::RegionNum - Returns the number of Regions
*/
DWORD CMapZone::RegionNum (void)
{
   return m_lPCMapRegion.Num();
}


/*************************************************************************************
CMapZone::RegionGet - Gets a Region by index

inputs
   DWORD          dwIndex - Index
returns
   PCMapRegion - Region
*/
PCMapRegion CMapZone::RegionGet (DWORD dwIndex)
{
   PCMapRegion *ppm = (PCMapRegion*)m_lPCMapRegion.Get(dwIndex);
   if (!ppm)
      return NULL;
   return ppm[0];
}



/*************************************************************************************
CMapZone::RegionFindIndex - Finds a given Region.

inputs
   PWSTR          pszName - Name
   BOOL           fCreateIfNotExist - If can't find then create one
returns
   DWORD - Index, or -1 if can't find
*/
DWORD CMapZone::RegionFindIndex (PWSTR pszName, BOOL fCreateIfNotExist)
{
   DWORD i;
   PCMapRegion *ppm = (PCMapRegion*)m_lPCMapRegion.Get(0);
   for (i = 0; i < m_lPCMapRegion.Num(); i++) {
      if (!_wcsicmp((PWSTR)ppm[i]->m_memName.p, pszName))
         return i;
   } // i

   if (!fCreateIfNotExist)
      return -1;

   // else add
   PCMapRegion pNew = new CMapRegion;
   if (!pNew)
      return -1;
   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, pszName);
   pNew->m_pZone = this;
   m_lPCMapRegion.Add (&pNew);

   return m_lPCMapRegion.Num()-1;
}



/*************************************************************************************
CMapZone::RegionFind - Finds a Region.

inputs
   GUID           *pgID - ID
returns
   PCMapRegion - Region. NULL if can't find
*/
PCMapRegion CMapZone::RegionFind (PWSTR pszName, BOOL fCreateIfNotExist)
{
   return RegionGet(RegionFindIndex (pszName, fCreateIfNotExist));
}


/*************************************************************************************
CMapZone::PopupMenu - This creates a popup menu of all the rooms in the Zone.
If there is only one room, it doesn't create a popup menu, but returns a pointer
to the room.

inputs
   PCListFixed       plPCMapMap - List that's initialized to sizeof(PCMapRoom).
                        When a room is added to a popup, it will be added to this list.
                        The menu ID is 1000 + index into this list.
   PCMapMap          pCurMap - Current map (so can check it)
   PCMapMap          *ppMap - If the function returns NULL (non popup), then this
                        will be NULL or a single room from the list.
returns
   HMENU - Menu created, or NULL. This must be freed
*/
HMENU CMapZone::PopupMenu (PCListFixed plPCMapMap, PCMapMap pCurMap, PCMapMap *ppMap)
{
   *ppMap = NULL;
   HMENU hMenuSingle = NULL;
   HMENU hMenu = NULL;
   PCMapRegion pRegionSingle = NULL;

   BeepWindowBeep (ESCBEEP_MENUOPEN);

   // loop through
   CMem mem;
   DWORD dwLen, i;
   for (i = 0; i < RegionNum(); i++) {
      PCMapRegion pRegion = RegionGet(i);
      PCMapMap pMap2 = NULL;
      HMENU hMenu2 = pRegion->PopupMenu (plPCMapMap, pCurMap, &pMap2);

      // if menu then...
      if (hMenu2) {
         if (!hMenu && !hMenuSingle && !(*ppMap)) {
            // only found one thing so far, and it's a menu
            hMenuSingle = hMenu2;
            pRegionSingle = pRegion;
            continue;
         }
      }
      else if (pMap2) {
         if (!hMenu && !hMenuSingle && !(*ppMap)) {
            // only found one thing so far, and it's a room
            *ppMap = pMap2;
            continue;
         }
      }
      else
         continue;   // no match at all

      // create menu if it's not already
      if (!hMenu)
         hMenu = CreatePopupMenu ();
      if (!hMenu)
         continue;   // error

      // add pre-existing items
      if (*ppMap) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)(*ppMap)->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)(*ppMap)->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         // add to the menu
         plPCMapMap->Add (ppMap);
         AppendMenu (hMenu,
            ((pCurMap == *ppMap) ? MF_CHECKED : 0) | MF_STRING | MF_ENABLED,
            plPCMapMap->Num() - 1 + 1000, (char*)mem.p);

         *ppMap = NULL;
      }
      if (hMenuSingle) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)pRegionSingle->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)pRegionSingle->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         AppendMenu (hMenu, MF_POPUP | MF_ENABLED, (UINT_PTR) hMenuSingle, (char*)mem.p);
         hMenuSingle = NULL; // since don't need to delete
      }

      // add new items
      if (pMap2) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)pMap2->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)pMap2->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         // add to the menu
         plPCMapMap->Add (&pMap2);
         AppendMenu (hMenu,
            ((pCurMap == pMap2) ? MF_CHECKED : 0) | MF_STRING | MF_ENABLED,
            plPCMapMap->Num() - 1 + 1000, (char*)mem.p);
      }
      if (hMenu2) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)pRegion->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)pRegion->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         AppendMenu (hMenu, MF_POPUP | MF_ENABLED, (UINT_PTR) hMenu2, (char*)mem.p);
      }

   } // i


   return hMenu ? hMenu : hMenuSingle;

}

















/*************************************************************************************
CMapWindow::Constructor and destructor
*/
CMapWindow::CMapWindow (void)
{
   m_lPCMapZone.Init (sizeof(PCMapZone));
   m_hRooms.Init (sizeof(PCMapRoom));
   m_pMain = NULL;
   m_gRoomCur = GUID_NULL;
   m_fHiddenByUser = FALSE;
   m_fHiddenByServer = TRUE;
   m_hWnd = NULL;
   memset (&m_rClient, 0, sizeof(m_rClient));

   m_fScrollTimer = FALSE;
   m_dwScrollDelay = TIMERSCROLLMAP_DELAY;

   MemZero (&m_memCurZone);
   MemZero (&m_memCurRegion);
   MemZero (&m_memCurMap);
   m_pMapSize.Zero4();
   m_pView.Zero4();

   m_dwLastUpdateTime = 0;
   m_lLastUpdate.Init (sizeof(GUID));

   m_fMapPointTo = FALSE;
   m_pMapPointTo.Zero();
   m_acMapPointTo[0] = RGB(0xff,0xff,0xff);
   m_acMapPointTo[1] = RGB(0x10, 0x10, 0x0);
   MemZero (&m_memMapPointTo);
}

static PWSTR gpszMapWindow = L"MapWindow";

CMapWindow::~CMapWindow (void)
{
   if (m_fScrollTimer && m_hWnd) {
      KillTimer (m_hWnd, TIMER_SCROLLMAP);
      m_fScrollTimer = FALSE;
      m_dwScrollDelay = TIMERSCROLLMAP_DELAY;
   }

   // save it
   if (m_pMain) {
      // get rid of dead maps just to keep things clean
      ClearDeadMaps ();

      PCMMLNode2 pNode = MMLTo ();
      if (pNode) {
         m_pMain->UserSave (TRUE, gpszMapWindow, pNode);
         delete pNode;
      }
   }

   DeleteAll (NULL);

   if (m_hWnd)
      DestroyWindow (m_hWnd);
   m_hWnd = NULL;
}


/*************************************************************************************
CMapWindow::DeleteAll - Deletes all the objects this holds.

inputs
   PCHashGUID     phRooms - If not NULL, then when a Zone is deleted it will
                  remove itself from the list of roooms.
returns
   none
*/
void CMapWindow::DeleteAll (PCHashGUID phRooms)
{
   DWORD i;
   PCMapZone *ppm = (PCMapZone*)m_lPCMapZone.Get(0);
   for (i = 0; i < m_lPCMapZone.Num(); i++) {
      ppm[i]->DeleteAll (phRooms);
      delete ppm[i];
   } // i
   m_lPCMapZone.Clear();
}


static PWSTR gpszRoomCur = L"RoomCur";
static PWSTR gpszCurZone = L"CurZone";
static PWSTR gpszCurRegion = L"CurRegion";
static PWSTR gpszCurMap = L"CurMap";
static PWSTR gpszCurView = L"CurView";

/*************************************************************************************
CMapWindow::MMLTo - Standard API
*/
PCMMLNode2 CMapWindow::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMapWindow);

   // write out location
   //if (m_pMain && m_hWnd)
   //   m_pMain->ChildLocSave (m_hWnd, pNode, m_fHiddenByUser);
   m_pMain->ChildLocSave (m_hWnd, TW_MAP, NULL, &m_fHiddenByUser);

   // write out the current zone, region, and map
   PWSTR psz;
   psz = (PWSTR)m_memCurZone.p;
   if (psz[0])
      MMLValueSet (pNode, gpszCurZone, psz);
   psz = (PWSTR)m_memCurRegion.p;
   if (psz[0])
      MMLValueSet (pNode, gpszCurRegion, psz);
   psz = (PWSTR)m_memCurMap.p;
   if (psz[0])
      MMLValueSet (pNode, gpszCurMap, psz);
   MMLValueSet (pNode, gpszCurView, &m_pView);

   // write the current room
   MMLValueSet (pNode, gpszRoomCur, (PBYTE)&m_gRoomCur, sizeof(m_gRoomCur));

   // write out the zones
   DWORD i;
   PCMapZone *ppm = (PCMapZone*)m_lPCMapZone.Get(0);
   for (i = 0; i < m_lPCMapZone.Num(); i++) {
      PCMMLNode2 pSub = ppm[i]->MMLTo ();
      if (!pSub)
         continue;
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CMapWindow::MMLFrom - Standard API, except
*/
BOOL CMapWindow::MMLFrom (PCMMLNode2 pNode)
{
   DeleteAll (&m_hRooms);
   MemZero (&m_memCurZone);
   MemZero (&m_memCurRegion);
   MemZero (&m_memCurMap);

   // read in the screen location
   // if (m_pMain)
   //   m_pMain->ChildLocGet (pNode, &m_rWindowLoc, &m_fHiddenByUser);
   BOOL fTitle = TRUE;
   DWORD dwMonitor;
   m_pMain->ChildLocGet (TW_MAP, NULL, &dwMonitor, &m_rWindowLoc, &m_fHiddenByUser, &fTitle);

   // get the zone, region, and map
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszCurZone);
   if (psz)
      MemCat (&m_memCurZone, psz);
   psz = MMLValueGet (pNode, gpszCurRegion);
   if (psz)
      MemCat (&m_memCurRegion, psz);
   psz = MMLValueGet (pNode, gpszCurMap);
   if (psz)
      MemCat (&m_memCurMap, psz);
   MMLValueGetPoint (pNode, gpszCurView, &m_pView);

   // get the current room
   m_gRoomCur = GUID_NULL;
   MMLValueGetBinary (pNode, gpszRoomCur, (PBYTE)&m_gRoomCur, sizeof(m_gRoomCur));

   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, gpszZone)) {
         PCMapZone pZone = new CMapZone;
         if (!pZone)
            return FALSE;
         if (!pZone->MMLFrom (pSub, &m_hRooms)) {
            delete pZone;
            return FALSE;
         }
         m_lPCMapZone.Add (&pZone);
      }
   } // i

   return TRUE;
}




/*************************************************************************************
CMapWindow::ZoneAdd - Adds a Zone to the MapWindow.

inputs
   PCMapZone         pZone - Zone to add
returns
   BOOL - TRUE if success
*/
BOOL CMapWindow::ZoneAdd (PCMapZone pZone)
{
   m_lPCMapZone.Add (&pZone);
   return TRUE;
}



/*************************************************************************************
CMapWindow::ZoneRemove - Removes a Zone from the list.

inputs
   DWORD          dwIndex - Index of the Zone
   BOOL           fDelete - if TRUE then delete the Zone
   PCHashGUID     phRooms - If not NULL, and deleting, then remove all Zones
                     from the phRooms list
returns
   BOOL - TRUE if success
*/
BOOL CMapWindow::ZoneRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms)
{
   if (dwIndex >= m_lPCMapZone.Num())
      return FALSE;

   if (fDelete) {
      PCMapZone pm = *((PCMapZone*)m_lPCMapZone.Get(dwIndex));
      pm->DeleteAll (phRooms);
      delete pm;
   }

   m_lPCMapZone.Remove (dwIndex);

   return TRUE;
}

/*************************************************************************************
CMapWindow::ZoneNum - Returns the number of Zones
*/
DWORD CMapWindow::ZoneNum (void)
{
   return m_lPCMapZone.Num();
}


/*************************************************************************************
CMapWindow::ZoneGet - Gets a Zone by index

inputs
   DWORD          dwIndex - Index
returns
   PCMapZone - Zone
*/
PCMapZone CMapWindow::ZoneGet (DWORD dwIndex)
{
   PCMapZone *ppm = (PCMapZone*)m_lPCMapZone.Get(dwIndex);
   if (!ppm)
      return NULL;
   return ppm[0];
}



/*************************************************************************************
CMapWindow::ZoneFindIndex - Finds a given Zone.

inputs
   PWSTR          pszName - Name
   BOOL           fCreateIfNotExist - If can't find then create one
returns
   DWORD - Index, or -1 if can't find
*/
DWORD CMapWindow::ZoneFindIndex (PWSTR pszName, BOOL fCreateIfNotExist)
{
   DWORD i;
   PCMapZone *ppm = (PCMapZone*)m_lPCMapZone.Get(0);
   for (i = 0; i < m_lPCMapZone.Num(); i++) {
      if (!_wcsicmp((PWSTR)ppm[i]->m_memName.p, pszName))
         return i;
   } // i

   if (!fCreateIfNotExist)
      return -1;

   // else add
   PCMapZone pNew = new CMapZone;
   if (!pNew)
      return -1;
   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, pszName);
   m_lPCMapZone.Add (&pNew);

   return m_lPCMapZone.Num()-1;
}



/*************************************************************************************
CMapWindow::ZoneFind - Finds a Zone.

inputs
   PWSTR          pszName - Name
   BOOL           fCreateIfNotExist - If can't find then create one
returns
   PCMapZone - Zone. NULL if can't find
*/
PCMapZone CMapWindow::ZoneFind (PWSTR pszName, BOOL fCreateIfNotExist)
{
   return ZoneGet(ZoneFindIndex (pszName, fCreateIfNotExist));
}


/*************************************************************************************
CMapWindow::RoomFind - Finds a room.

inputs
   GUID           *pgID - ID
returns
   PCMapRoom - Room, or NULL if cant find
*/
PCMapRoom CMapWindow::RoomFind (GUID *pgID)
{
   PCMapRoom *ppm = (PCMapRoom*)m_hRooms.Find (pgID);
   if (!ppm)
      return NULL;
   return ppm[0];
}



/************************************************************************************
MapWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK MapWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCMapWindow p = (PCMapWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCMapWindow p = (PCMapWindow)(LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCMapWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************************
CMapWindow::Init - Initializes the map window

inputs
   PCMainWindow         pMain - Main window
returns
   BOOL - TRUE if success
*/
BOOL CMapWindow::Init (PCMainWindow pMain)
{
   if (m_pMain)
      return FALSE;
   m_pMain = pMain;

   // will need default screen loc
   GetClientRect (pMain->m_hWndPrimary, &m_rWindowLoc);  // assume primary for now
   m_rWindowLoc.left = (m_rWindowLoc.right + m_rWindowLoc.left)/2;
   m_rWindowLoc.bottom = (m_rWindowLoc.top + m_rWindowLoc.bottom)/2;
   m_fHiddenByUser = FALSE; // default to showing it


   // try to get all the information from the MML
   PCMMLNode2 pNode = m_pMain->UserLoad (TRUE, gpszMapWindow);
   if (pNode) {
      MMLFrom (pNode);
      delete pNode;
   }

   // register the class
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = MapWndProc;
   wc.lpszClassName = "CircumrealityClientMap";
   wc.style = CS_HREDRAW | CS_VREDRAW;
   wc.hIcon = NULL;
   wc.hCursor = NULL;
   wc.hbrBackground = NULL;
   RegisterClass (&wc);

   // get the window info
   BOOL fHidden = FALSE, fTitle = FALSE;
   // BUGFIX - Was using RECT rLoc; in ChildLocGet(), but then was ignoring it
   // This meant the map window started in the wrong location
   DWORD dwMonitor;
   m_pMain->ChildLocGet (TW_MAP, NULL, &dwMonitor, &m_rWindowLoc, &fHidden, &fTitle);

   m_pMain->ChildShowTitleIfOverlap (NULL, &m_rWindowLoc, dwMonitor, fHidden, &fTitle);
   m_hWnd = CreateWindowEx (
      (fTitle ? WS_EX_IFTITLE : WS_EX_IFNOTITLE) | WS_EX_ALWAYS,
      wc.lpszClassName, "Map",
      WS_ALWAYS | (fTitle ? WS_IFTITLECLOSE : 0) | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, // BUGFIX - no scroll | WS_HSCROLL | WS_VSCROLL,
      m_rWindowLoc.left , m_rWindowLoc.top , m_rWindowLoc.right - m_rWindowLoc.left , m_rWindowLoc.bottom - m_rWindowLoc.top ,
      dwMonitor ? m_pMain->m_hWndSecond  : m_pMain->m_hWndPrimary,
      NULL, ghInstance, (PVOID) this);

   // need to calc some stuff, like what looking at
   Show(TRUE);

   return TRUE;
}



static PWSTR gpszYouAreHere = L"YouAreHere";
static PWSTR gpszCenterOnRoom = L"CenterOnRoom";

/*************************************************************************************
CMapWindow::AutoMapMessage - This is called when an <AutoMap> message comes
in. It parses the message.

inputs
   PCMMLNode2         pNode - Node that's of type <AutoMap>. This does NOT delete the node
   BOOL              fShow - Set to TRUE if message was <AutoMapShow>
returns
   BOOL - TRUE if success
*/
BOOL CMapWindow::AutoMapMessage (PCMMLNode2 pNode, BOOL fShow)
{
   // if showing
   if (fShow) {
      BOOL fDelete = (BOOL)MMLValueGetInt (pNode, L"Delete", 0);
      BOOL fAutoShow = (BOOL)MMLValueGetInt (pNode, L"AutoShow", 0);
      // BOOL fHidden = (BOOL)MMLValueGetInt (pNode, L"Hidden", 0);

      if (fDelete) {
         // want is hidden
         m_fHiddenByServer = TRUE;
         Show (TRUE);
         return TRUE;
      }

      // else, want to show
      m_fHiddenByServer = FALSE;

      if (fAutoShow)
         m_fHiddenByUser = FALSE;

      Show (TRUE);

      return TRUE;
   }

   // see if youarehere is set
   BOOL fYouAreHere = MMLValueGetInt (pNode, gpszYouAreHere, FALSE);
         // BUGFIX - Was TRUE, but with multiple rooms, default to FALSE
   BOOL fCenterOnRoom = MMLValueGetInt (pNode, gpszCenterOnRoom, FALSE);

   // get the ID
   GUID gID = GUID_NULL;
   MMLValueGetBinary (pNode, gpszID, (PBYTE)&gID, sizeof(gID));
   if (fYouAreHere && !IsEqualGUID(m_gRoomCur, gID))
      m_gRoomCur = gID;
   else
      fYouAreHere = FALSE; // didn't really change rooms so dont care

   if (IsEqualGUID (gID, GUID_NULL)) {
      // just set room to nowhere (above) and return
      // will need to refresh map display
      RecalcView();
      RecalcView360();
      return TRUE;
   }

   // get the zone and the region
   PWSTR pszZone = MMLValueGet (pNode, gpszZone);
   PWSTR pszRegion = MMLValueGet (pNode, gpszRegion);
   PWSTR pszMap = MMLValueGet (pNode, gpszMap);
   if (!pszZone || !pszZone[0] || !pszRegion || !pszRegion[0] || !pszMap || !pszMap[0])
      return FALSE;  // must have these

   // try to find the zone, region, map
   PCMapZone pZone = ZoneFind (pszZone, TRUE);
   if (!pZone)
      return FALSE;
   PCMapRegion pRegion = pZone->RegionFind (pszRegion, TRUE);
   if (!pRegion)
      return FALSE;
   PCMapMap pMap = pRegion->MapFind (pszMap, TRUE);
   if (!pMap)
      return FALSE;

   // see if the room already exists
   PCMapRoom pRoom = RoomFind (&gID);
   BOOL fFound = pRoom ? TRUE : FALSE;
   if (!fFound) {
      pRoom = new CMapRoom;
      if (!pRoom)
         return FALSE;
      pMap->RoomAdd (pRoom);
   }
   else {
      // if the map has changed then remove from the old map and put in the new one
      if (pMap != pRoom->m_pMap) {
         pRoom->m_pMap->RoomRemove (pRoom->m_pMap->RoomFindIndex(&pRoom->m_gID), FALSE, NULL);
         pMap->RoomAdd (pRoom);
      }
   }

   // read in the MML
   pRoom->MMLFrom (pNode, pZone, pRegion, pMap, fFound ? NULL : &m_hRooms);

   // BUGFIX - Only if fCenterOnRoom
   if (fCenterOnRoom) {
      // set the current zone, etc.
      MemZero (&m_memCurZone);
      MemZero (&m_memCurRegion);
      MemZero (&m_memCurMap);
      MemCat (&m_memCurZone, pszZone);
      MemCat (&m_memCurRegion, pszRegion);
      MemCat (&m_memCurMap, pszMap);
   }
   
   // if just moved to the current room, then jump there
   if (fCenterOnRoom) {
      PCMapRoom pCurRoom = RoomFind (&m_gRoomCur);
      if (pCurRoom) {
         m_pView.p[0] = pCurRoom->m_pLoc.p[0];
         m_pView.p[1] = pCurRoom->m_pLoc.p[1];
      }
   } // fCenterOnRoom

   // remember last updated rooms
   DWORD i;
   DWORD dwTime = GetTickCount();
   DWORD dwTimeMin = (dwTime > ROOMUPDATETIME) ? (dwTime - ROOMUPDATETIME) : 0;
   DWORD dwTimeMax = (dwTime < (DWORD)-1 - ROOMUPDATETIME) ? (dwTime + ROOMUPDATETIME) : (DWORD)-1;
   if ((m_dwLastUpdateTime >= dwTimeMin) && (m_dwLastUpdateTime <= dwTimeMax)) {
      // only add if not duplicate
      GUID *pg = (GUID*)m_lLastUpdate.Get(0);
      for (i = 0; i < m_lLastUpdate.Num(); i++, pg++)
         if (IsEqualGUID(*pg, gID))
            break;
      if (i >= m_lLastUpdate.Num())
         m_lLastUpdate.Add (&gID);
   }
   else {
      // new set of rooms, so update
      m_dwLastUpdateTime = dwTime;
      m_lLastUpdate.Init (sizeof(GUID), &gID, 1);
   }

   // will need to refresh display with new room info (if displaying
   // the current map)
   RecalcView ();
   RecalcView360 ();

   // BUGFIX - Don't show since have special options for this
   // show
   // m_fHiddenByServer = FALSE;
   // Show (TRUE);

   // done
   return TRUE;
}

/*************************************************************************************
CMapWindow::ClearDeadMaps - Look through all the maps and delete dead ones.
*/
void CMapWindow::ClearDeadMaps (void)
{
   DWORD dwZone, dwRegion, dwMap;
   for (dwZone = (DWORD)ZoneNum()-1; dwZone < ZoneNum(); dwZone--) {
      PCMapZone pZone = ZoneGet(dwZone);
      if (!pZone)
         continue;

      for (dwRegion = (DWORD)pZone->RegionNum()-1; dwRegion < pZone->RegionNum(); dwRegion--) {
         PCMapRegion pRegion = pZone->RegionGet(dwRegion);
         if (!pRegion)
            continue;

         // maps
         for (dwMap = (DWORD)pRegion->MapNum()-1; dwMap < pRegion->MapNum(); dwMap--) {
            PCMapMap pMap = pRegion->MapGet(dwMap);
            if (!pMap)
               continue;

            // see if it's empty
            if (!pMap->RoomNum())
               pRegion->MapRemove (dwMap, TRUE, &m_hRooms);

         } // dwMap

         // if the region is empty delete
         if (!pRegion->MapNum())
            pZone->RegionRemove (dwRegion, TRUE, &m_hRooms);

      } // dwRegion

      // if the zone is empty delete
      if (!pZone->RegionNum())
         ZoneRemove (dwZone, TRUE, &m_hRooms);
   } // dwZone
}


/*************************************************************************************
CMapWindow::Show - Shows or hides the map window based on m_fHiddenXXX

inputs
   BOOL        fNoChangeTitle - If TRUE then don't change the title bar
*/
void CMapWindow::Show (BOOL fNoChangeTitle)
{
   BOOL fVisible = IsWindowVisible (m_hWnd);

   m_pMain->ChildShowWindow (m_hWnd, TW_MAP, NULL,
      (m_fHiddenByServer || m_fHiddenByUser) ? SW_HIDE : SW_SHOWNA,
      &m_fHiddenByUser, fNoChangeTitle);

   // relcalc the view just in case
   BOOL fVisibleNow = IsWindowVisible (m_hWnd);
   if (fVisibleNow && (fVisible != fVisibleNow))
      RecalcView ();
}


/*************************************************************************************
CMapWindow::PaintEnlargedNoBitmap - Paint an enlarged version of the map to
a new bitmap.

inputs
   HDC            hDC - Paint on this
   int            iScale - Scale amount. 1x = normal size
   BOOL           fClearBackground - If TRUE clear the background
returns
   none
*/
void CMapWindow::PaintEnlargedNoBitmap (HDC hDC, int iScale, BOOL fClearBackground)
{
   HFONT hFontBig = NULL, hFontSmall = NULL;
   HFONT hFontOld = NULL;
   RECT rClient;
   rClient = m_rClient;
   rClient.left *= iScale;
   rClient.right *= iScale;
   rClient.top *= iScale;
   rClient.bottom *= iScale;

   // paint to bitmap and then blit so don't see flickers
   COLORREF cMap = gpMainWindow->m_fLightBackground ? RGB(0xc0, 0xc0, 0x80) : RGB(0x00, 0x00, 0x00);
   HBRUSH hBrush = CreateSolidBrush (cMap /*m_pMain->m_cTextDim*/);
   if (fClearBackground)
      FillRect (hDC, &rClient, hBrush);
   DeleteObject (hBrush);

   // draw all the rooms
   PCMapZone pZone = ZoneFind ((PWSTR)m_memCurZone.p);
   PCMapRegion pRegion = pZone ? pZone->RegionFind ((PWSTR)m_memCurRegion.p) : NULL;
   PCMapMap pMap = pRegion ? pRegion->MapFind ((PWSTR)m_memCurMap.p) : NULL;

   // draw the text for the map
   CMem memW, mem;
   MemZero (&memW);
   if (pMap)
      MemCat (&memW, (PWSTR) pMap->m_memName.p);
   if (pZone && ((pZone->RegionNum() > 1) || (ZoneNum() > 1))) {
      MemCat (&memW, L" in ");

      if (pZone->RegionNum() > 1)
         MemCat (&memW, (PWSTR)pRegion->m_memName.p);
      if ((pZone->RegionNum() > 1) && (ZoneNum() > 1))
         MemCat (&memW, L", ");
      if (ZoneNum() > 1)
         MemCat (&memW, (PWSTR)pZone->m_memName.p);
   }

   DWORD dwLen = ((DWORD)wcslen((PWSTR)memW.p)+1)*sizeof(WCHAR);
   if (!mem.Required (dwLen))
      goto skiptext;  // error
   WideCharToMultiByte (CP_ACP, 0, (PWSTR)memW.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = FontScaleByScreenSize(-10 * iScale); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   lf.lfItalic = TRUE;
   strcpy (lf.lfFaceName, "Arial");
   hFontSmall = CreateFontIndirect (&lf);
   lf.lfHeight = FontScaleByScreenSize(-14 * iScale);
   lf.lfWeight = FW_BOLD;
   lf.lfItalic = FALSE;
   hFontBig = CreateFontIndirect (&lf);


   hFontOld = (HFONT) SelectObject (hDC, hFontSmall);
   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   COLORREF cText;
   cText = gpMainWindow->m_fLightBackground ? RGB(0x10,0x10,0x00) : RGB(0xff,0xff,0xff);
   SetTextColor (hDC, cText);

   // figure out size
   RECT r;
   r = rClient;
   r.left += 10 * iScale;
   r.top += 10 * iScale;
   DrawText (hDC, (char*)mem.p, -1, &r, DT_SINGLELINE | DT_LEFT | DT_TOP);

   SelectObject (hDC, hFontOld);
   SetBkMode (hDC, iOldMode);
skiptext:   // in case error


   // draw all the rooms
   DWORD i, dwPass;
   if (pMap)
      for (dwPass = 0; dwPass < 2; dwPass++)
         for (i = 0; i < pMap->RoomNum(); i++) {
            PCMapRoom pRoom = pMap->RoomGet (i);
            if (pRoom)
               PaintRoom (hDC, pRoom, dwPass, iScale, hFontBig, hFontSmall);
         } // i, dwPass



   // draw the line
   if (m_fMapPointTo)
      PaintMapPointTo (hDC, iScale, hFontBig);
   if (hFontOld)
      SelectObject (hDC, hFontOld);
   if (hFontBig)
      DeleteObject (hFontBig);
   if (hFontSmall)
      DeleteObject (hFontSmall);
}


/*************************************************************************************
CMapWindow::PaintEnlarged - Paint an enlarged version of the map to
a new bitmap.

inputs
   HDC            hDCPaint - Create a compatible DC for this
   int            iScale - Scale amount. 1x = normal size
   BOOL           fTransprent - TRUE if transparent background
returns
   HBITMAP - Bitmap
*/
HBITMAP CMapWindow::PaintEnlarged (HDC hDCPaint, int iScale, BOOL fTransparent)
{
   HFONT hFont = NULL;
   RECT rClient;
   rClient = m_rClient;
   rClient.left *= iScale;
   rClient.right *= iScale;
   rClient.top *= iScale;
   rClient.bottom *= iScale;

   HDC hDC = CreateCompatibleDC (hDCPaint);
   HBITMAP hBit = CreateCompatibleBitmap (hDCPaint, rClient.right, rClient.bottom);
   SelectObject (hDC, hBit);

   // fill the backround
   m_pMain->BackgroundStretch (BACKGROUND_TEXT, fTransparent, WANTDARK_DARK, &m_rClient, m_hWnd, GetParent(m_hWnd),
      (DWORD)iScale, hDC, NULL);

   PaintEnlargedNoBitmap (hDC, iScale, !fTransparent);

   DeleteDC (hDC);

   return hBit;
}

/*************************************************************************************
CMapWindow::PaintAnti - Paint antialiased

inputs
   HDC            hDCPaint - Create compatible bitmap from this
   BOOL           fTransprent - TRUE if transparent background
returns
   HBITMAP - Bitmap
*/
HBITMAP CMapWindow::PaintAnti (HDC hDCPaint, BOOL fTransparent)
{
   int iScale = 2;      // 3x antialiasing. BUGFIX - Was 3x
   HBITMAP hBitLarge = PaintEnlarged (hDCPaint, iScale, fTransparent);
   if (!hBitLarge)
      return NULL;

   // to image
   CImage Image, ImageDown;
   Image.Init (hBitLarge);
   DeleteObject (hBitLarge);

   Image.Downsample (&ImageDown, (DWORD)iScale);
   return ImageDown.ToBitmap (hDCPaint);
}


/*************************************************************************************
CMapWindow::WalkToAction - Determine what command to use to walk to.

inputs
   PCMapRoom         pRoom - Room walking to
   PCMem             pMemDo - Filled with string and sent to the command
   PCMem             pMemShow - What to show. If empty string then use do
   LANGID            *pLangID - Filled with the language of the command
returns
   BOOL - TRUE if there's a command, FALSE if cant go there
*/
BOOL CMapWindow::WalkToAction (PCMapRoom pRoom, PCMem pMemDo, PCMem pMemShow, LANGID *pLangID)
{
   MemZero (pMemDo);
   MemZero (pMemShow);
   *pLangID = 1033;     // english

   if (!pRoom)
      return FALSE;
   DWORD dwDir = WalkToDirection (pRoom);
   if (dwDir == (DWORD)-1)
      return FALSE;

   PWSTR psz;
   PWSTR pszShow = NULL;
   WCHAR szCmdVisible[256], szCmdSent[256];
   switch (dwDir) {
   default:
   case 0:
      psz = L"go north";
      break;
   case 1:
      psz = L"go northeast";
      break;
   case 2:
      psz = L"go east";
      break;
   case 3:
      psz = L"go southeast";
      break;
   case 4:
      psz = L"go south";
      break;
   case 5:
      psz = L"go southwest";
      break;
   case 6:
      psz = L"go west";
      break;
   case 7:
      psz = L"go northwest";
      break;
   case 8:
      psz = L"go up";
      break;
   case 9:
      psz = L"go down";
      break;
   case 10:
      psz = L"go in";
      break;
   case 11:
      psz = L"go out";
      break;

   case ROOMEXITS:
      {
         if (!pRoom || !pRoom->m_memName.p)
            return FALSE;   // shouldnt happen

         swprintf (szCmdVisible, L"walk to %s", (PWSTR)pRoom->m_memName.p);
         wcscpy (szCmdSent, L"walk to |");
         MMLBinaryToString ((PBYTE) &pRoom->m_gID, sizeof(pRoom->m_gID), szCmdSent + wcslen(szCmdSent) );

         psz = szCmdSent;
         pszShow = szCmdVisible;
      }
      break;
   } // switch


   MemCat (pMemDo, psz);
   if (pszShow)
      MemCat (pMemShow, pszShow);
   return TRUE;
}



/*************************************************************************************
CMapWindow::RandomAction - Performs a random action from the map

inputs
   PCMem          pMemAction - Filled in with the action string
   GUID           *pgObject - Filled in with the object this is associated with, or NULL if none
   LANGID         *pLangID - Filled in with the language ID, or 0 if unknown
returns
   BOOL - TRUE if action filled in
*/
BOOL CMapWindow::RandomAction (PCMem pMemAction, GUID *pgObject, LANGID *pLangID)
{
   *pgObject = GUID_NULL;
   *pLangID = 0;

   // find the current map
   PCMapZone pCurZone = ZoneFind ((PWSTR)m_memCurZone.p);
   PCMapRegion pCurRegion = pCurZone ? pCurZone->RegionFind ((PWSTR)m_memCurRegion.p) : NULL;
   PCMapMap pCurMap = pCurRegion ? pCurRegion->MapFind ((PWSTR)m_memCurMap.p) : NULL;
   if (!pCurMap)
      return NULL;
   
   // random room
   if (!pCurMap->RoomNum())
      return FALSE;
   PCMapRoom pr = pCurMap->RoomGet((DWORD)rand() % pCurMap->RoomNum());
   if (!pr)
      return FALSE;

   CMem memShow;
   return WalkToAction (pr, pMemAction, &memShow, pLangID);

}

/*************************************************************************************
CMapWindow::WndProc - Manages the window calls
*/
LRESULT CMapWindow::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      {
         m_hWnd = hWnd;
      }
      break;

   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDCPaint = BeginPaint (hWnd, &ps);

#ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
         BOOL fTransparent = !m_pMain->ChildHasTitle (m_hWnd);
#else
         BOOL fTransparent = FALSE;
#endif

         HBITMAP hBit = PaintAnti (hDCPaint, fTransparent);
         if (hBit) {
            HDC hDC = CreateCompatibleDC (hDCPaint);
            SelectObject (hDC, hBit);

            // blit back
            BitBlt (hDCPaint, 0, 0, m_rClient.right, m_rClient.bottom,
               hDC, 0, 0, SRCCOPY);

            DeleteDC (hDC);
            DeleteObject (hBit);
         }

         EndPaint (hWnd, &ps);
      }
      return 0;

   case WM_MOUSEWHEEL:
      {
         short sAmt = (short)HIWORD(wParam);
         if (sAmt > 0)
            m_pView.p[2] /= pow(2, 0.25);
         else if (sAmt < 0)
            m_pView.p[2] *= pow(2, 0.25);

         RecalcView ();
      }
      return 0;

#if 0 // no scrollbars
   case WM_HSCROLL:
   case WM_VSCROLL:
      {
         BOOL fHorz = (uMsg == WM_HSCROLL);
         // only deal with horizontal scroll
         HWND hWndScroll = m_hWnd;

         // get the scrollbar info
         SCROLLINFO si;
         memset (&si, 0, sizeof(si));
         si.cbSize = sizeof(si);
         si.fMask = SIF_ALL;
         GetScrollInfo (hWndScroll, fHorz ? SB_HORZ : SB_VERT, &si);
         
         // what's the new position?
         switch (LOWORD(wParam)) {
         default:
            return 0;
         case SB_ENDSCROLL:
            return 0;
         case SB_LINEUP:
         //case SB_LINELEFT:
            si.nPos  -= max(si.nPage / 8, 1);
            break;

         case SB_LINEDOWN:
         //case SB_LINERIGHT:
            si.nPos  += max(si.nPage / 8, 1);
            break;

         case SB_PAGELEFT:
         //case SB_PAGEUP:
            si.nPos  -= si.nPage;
            break;

         case SB_PAGERIGHT:
         //case SB_PAGEDOWN:
            si.nPos  += si.nPage;
            break;

         case SB_THUMBPOSITION:
            si.nPos = si.nTrackPos;
            break;
         case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
         }

         // don't go beyond min and max
         si.nPos = min((int)(si.nMax - si.nPage), (int)si.nPos);
         si.nPos = max(si.nPos,si.nMin);

         si.fMask = SIF_ALL;
         SetScrollInfo (hWndScroll, fHorz ? SB_HORZ : SB_VERT, &si, TRUE);

         // adjust this in floating point settings
         fp fAlpha = ((fp)si.nPos + (fp)si.nPage/2) / (fp)si.nMax;
         if (fHorz)
            m_pView.p[0] = (1.0 - fAlpha) * m_pMapSize.p[0] + fAlpha * m_pMapSize.p[1];
         else
            m_pView.p[1] = (1.0 - fAlpha) * m_pMapSize.p[3] + fAlpha * m_pMapSize.p[2];

         // update it all
         RecalcView (TRUE);
         return 0;
      }
      break;
#endif // 0

   case WM_MOVING:
      if (m_pMain->TrapWM_MOVING (hWnd, lParam, FALSE))
         return TRUE;
      break;
   case WM_SIZING:
      if (m_pMain->TrapWM_MOVING (hWnd, lParam, TRUE))
         return TRUE;
      break;

   case WM_MOVE:
      m_pMain->ChildLocSave (hWnd, TW_MAP, NULL, &m_fHiddenByUser);
      break;

   case WM_SIZE:
      m_pMain->ChildLocSave (hWnd, TW_MAP, NULL, &m_fHiddenByUser);
      RecalcView ();
      break;

   case WM_MOUSEACTIVATE:
      m_pMain->CommandSetFocus(FALSE);
      break;

   case WM_SETFOCUS:
      m_pMain->CommandSetFocus(FALSE);
      break;


   case WM_DESTROY:
      m_hWnd = NULL;
      break;

   case WM_CLOSE:
      // just hide this
      m_fHiddenByUser = TRUE;
      Show (TRUE);

      // show bottom pane so player knows is closed
      if (m_pMain->m_pSlideBottom)
         m_pMain->m_pSlideBottom->SlideDownTimed (1000);

      return 0;

   case WM_TIMER:
      if (wParam == TIMER_SCROLLMAP) {
         POINT p;
         fp fRight, fDown;
         GetCursorPos (&p);
         ScreenToClient (m_hWnd, &p);
         CursorToScroll (p, &fRight, &fDown);

         // if 0 then done
         if (!fRight && !fDown) {
            if (m_fScrollTimer && m_hWnd) {
               KillTimer (m_hWnd, TIMER_SCROLLMAP);
               m_fScrollTimer = FALSE;
               m_dwScrollDelay = TIMERSCROLLMAP_DELAY;
            }
            return 0;
         }

         // if haven't waited long enough then done
         if (m_dwScrollDelay) {
            m_dwScrollDelay--;
            return 0;
         }

         // else, scroll
         m_pView.p[0] += m_pView.p[2] * fRight * TIMERSCROLLMAP_INTERVAL / TIMERSCROLLMAP_RATE;
         m_pView.p[1] -= m_pView.p[3] * fDown * TIMERSCROLLMAP_INTERVAL / TIMERSCROLLMAP_RATE;
         RecalcView (FALSE);

         return 0;
      }
      break;

   case WM_MOUSEMOVE:
      {
         // pan cursor
         fp fRight, fDown;
         POINTS ps;
         POINT p;
         ps = MAKEPOINTS (lParam);
         p.x = ps.x;
         p.y = ps.y;
         CursorToScroll (p, &fRight, &fDown);
         if (fRight || fDown) {
            DWORD dwCursor;
            if (fabs(fRight) > fabs(fDown))
               dwCursor = (fRight > 0) ? IDC_CURSORPOINTRIGHT : IDC_CURSORPOINTLEFT;
            else
               dwCursor = (fDown > 0) ? IDC_CURSORROTDOWN : IDC_CURSORROTUP;
            SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(dwCursor)));

            // set the timer
            if (!m_fScrollTimer) {
               m_fScrollTimer = TRUE;
               m_dwScrollDelay = TIMERSCROLLMAP_DELAY;
               SetTimer (m_hWnd, TIMER_SCROLLMAP, TIMERSCROLLMAP_INTERVAL, 0);
            }

            return 0;
         }

         if (!m_pMain->m_fMenuExclusive && !m_pMain->m_fMessageDiabled) {
            // see if can walk to the room
            POINT p;
            p.x = (short)LOWORD(lParam);
            p.y = (short)HIWORD(lParam);
            if (WalkToDirection (&p) != -1) {
               SetCursor (LoadCursor (ghInstance, MAKEINTRESOURCE(IDC_CURSORWALK)));
               return 0;
            }
         }

         // default is context menu
         SetCursor (m_pMain->m_hCursorMenu);
      }
      return 0;

   case WM_LBUTTONDOWN:
      // set this as the foreground window
      SetWindowPos (hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

      if (!m_pMain->m_fMenuExclusive && !m_pMain->m_fMessageDiabled) {
         // see if can walk to the room
         POINT p;
         p.x = (short)LOWORD(lParam);
         p.y = (short)HIWORD(lParam);
         // DWORD dwDir;
         PCMapRoom pRoom = IsOverRoom (&p);
         
         CMem memDo, memShow;
         LANGID lid;
         if (WalkToAction (pRoom, &memDo, &memShow, &lid)) {
            PWSTR psz = (PWSTR)memDo.p;
            PWSTR pszShow = (PWSTR)memShow.p;
            if (!pszShow[0])
               pszShow = NULL;

            // sent it
            m_pMain->SendTextCommand (lid, psz, pszShow ? pszShow : psz, NULL, NULL, TRUE, TRUE, TRUE);
            m_pMain->HotSpotDisable();
            BeepWindowBeep (ESCBEEP_LINKCLICK);
            return 0;
         }
      }

// nowalk:
      // else, do the context menu
      ContextMenu ();
      return 0;

   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CMapWindow::AdjustSizeForAspect - This takes a CPoint with size (from CMapMap::Size),
and expands it so that it matches the aspect ration of the map window

inputs
   PCPoint     pSize - From CMapMap::Size. Modified in place
reutrns
   none
*/
void CMapWindow::AdjustSizeForAspect (PCPoint pSize)
{
   RECT r;
   GetClientRect (m_hWnd, &r);
   OffsetRect (&r, -r.left, -r.top);   // just in case
   r.right = max(r.right,1);  // just in case
   r.bottom = max(r.bottom, 1);  // just in case
   fp fAspectWindow = (fp)r.right / (fp)r.bottom;
   fp fX = pSize->p[1] - pSize->p[0];
   fp fY = pSize->p[3] - pSize->p[2];
   fp fCenterX = (pSize->p[1] + pSize->p[0])/2;
   fp fCenterY = (pSize->p[3] + pSize->p[2])/2;
   fp fAspectMap = fX / fY;

   if (fAspectWindow >= fAspectMap) { // window is wider
      fAspectWindow /= fAspectMap;
      pSize->p[0] = fCenterX - fX * fAspectWindow / 2;
      pSize->p[1] = fCenterX + fX * fAspectWindow / 2;
   }
   else { // window is narrower, so make map taller
      fAspectMap /= fAspectWindow;
      pSize->p[2] = fCenterY - fY * fAspectMap / 2;
      pSize->p[3] = fCenterY + fY * fAspectMap / 2;
   }
}



/*************************************************************************************
CMapWindow::RecalcView360 - Recalculates all the settings and potentially updates the
scrollbars.

inputs
   BOOL        fIgnoreScroll - If TRUE doens't draw scrollbar
*/
void CMapWindow::RecalcView360 (void)
{
   // causes the main image to be redraw
   PCVisImage pvi = m_pMain->FindMainVisImage ();
   if (!pvi)
      return;

   if (!pvi->IsAnyLayer360())
      return;

   // tell it that changed
   pvi->Vis360Changed();
}

/*************************************************************************************
CMapWindow::RecalcView - Recalculates all the settings and potentially updates the
scrollbars.

inputs
   BOOL        fIgnoreScroll - If TRUE doens't draw scrollbar
*/
void CMapWindow::RecalcView (BOOL fIgnoreScroll)
{
   if (!m_hWnd)
      return;

   GetClientRect (m_hWnd, &m_rClient);
   m_rClient.right = max(m_rClient.right, 1);   // just to make sure non-zero
   m_rClient.bottom = max(m_rClient.bottom, 1);   // just to make sure non-zero
   
   // find the map...
   PCMapZone pZone = ZoneFind ((PWSTR)m_memCurZone.p);
   if (!pZone) {
      pZone = ZoneGet (0); // pick first one
      if (!pZone)
         return;
      MemZero (&m_memCurZone);
      MemCat (&m_memCurZone, (PWSTR) pZone->m_memName.p);

      m_pView.p[2] = 0; // so scales up
   }
   PCMapRegion pRegion = pZone->RegionFind ((PWSTR)m_memCurRegion.p);
   if (!pRegion) {
      pRegion = pZone->RegionGet (0); // pick first one
      if (!pRegion)
         return;
      MemZero (&m_memCurRegion);
      MemCat (&m_memCurRegion, (PWSTR) pRegion->m_memName.p);

      m_pView.p[2] = 0; // so scales up
   }
   PCMapMap pMap = pRegion->MapFind ((PWSTR)m_memCurMap.p);
   if (!pMap) {
      pMap = pRegion->MapGet (0); // pick first one
      if (!pMap)
         return;
      MemZero (&m_memCurMap);
      MemCat (&m_memCurMap, (PWSTR) pMap->m_memName.p);

      m_pView.p[2] = 0; // so scales up
   }

   // find the extent of this map
   pMap->Size (&m_pMapSize);
   AdjustSizeForAspect (&m_pMapSize);
   fp fMapWidth = m_pMapSize.p[1] - m_pMapSize.p[0];
   fp fMapHeight = m_pMapSize.p[3] - m_pMapSize.p[2];

   // if view too large or too small then fix
   // also, if would be looking off edge then fix
   m_pView.p[2] = min(m_pView.p[2], fMapWidth);
   if (m_pView.p[2] <= CLOSE)
      m_pView.p[2] = fMapWidth;
   else if (m_pView.p[2] < 1)
      m_pView.p[2] = 1;
   m_pView.p[3] = m_pView.p[2] / (fp)m_rClient.right * (fp) m_rClient.bottom; // so have around
   m_pView.p[0] = max(m_pView.p[0], m_pView.p[2]/2 + m_pMapSize.p[0]);
   m_pView.p[0] = min(m_pView.p[0], m_pMapSize.p[1] - m_pView.p[2]/2);
   m_pView.p[1] = max(m_pView.p[1], m_pView.p[3]/2 + m_pMapSize.p[2]);
   m_pView.p[1] = min(m_pView.p[1], m_pMapSize.p[3] - m_pView.p[3]/2);


#if 0 // no scrollbars
   // update the scrollbars
   if (!fIgnoreScroll) {
      SCROLLINFO siCur, siNew;
      DWORD i;
      for (i = 0; i < 2; i++) {
         memset (&siCur, 0, sizeof(siCur));
         siCur.cbSize = sizeof(siCur);
         siCur.fMask = SIF_ALL;
         GetScrollInfo (m_hWnd, i ? SB_HORZ : SB_VERT, &siCur);

         siNew.nMin = 0;
         siNew.nMax = 1000;
         siNew.nPage = (int)(m_pView.p[3-i] / (i ? fMapWidth : fMapHeight) * (fp)siNew.nMax);
         siNew.nPage = max(siNew.nPage, 1);
         siNew.nPage = min(siNew.nPage, (DWORD)siNew.nMax);
         if (i)
            siNew.nPos = (int)((m_pView.p[0] - m_pView.p[3-i]/2 - m_pMapSize.p[0]) / fMapWidth * (fp)siNew.nMax);
         else
            siNew.nPos = (int)((1.0 - (m_pView.p[1] + m_pView.p[3-i]/2 - m_pMapSize.p[2]) / fMapHeight) * (fp)siNew.nMax);

         siNew.cbSize = sizeof(siCur);
         siNew.fMask =
            ((siNew.nPage != siCur.nPage) ? SIF_PAGE : 0) |
            ((siNew.nPos != siCur.nPos) ? SIF_POS : 0) |
            (((siNew.nMin != siCur.nMin) || (siNew.nMax != siCur.nMax)) ? SIF_RANGE : 0);
         if (siNew.fMask) {
            siNew.fMask |= SIF_DISABLENOSCROLL;
            SetScrollInfo (m_hWnd, i ? SB_HORZ : SB_VERT, &siNew, TRUE);
         }
      } // i
   } // !fIgnoreScroll
#endif // 0
   
   // finally, cause redraw
   // m_pMain->InvalidateRectSpecial (m_hWnd);
   InvalidateRect (m_hWnd, NULL, FALSE);
}


/*************************************************************************************
CMapWindow::CursorToScroll - Cursor location in the client rect to the
amount to scroll left/right.

inputs
   POINT          pCursor - Cursor location in client rect
   fp             *pfRight - Filled with amount to scroll right (or neg=left), from 0..1. 0 = none
   fp             *pfDown - Filled with amount to scroll down (or neg=up), from 0..1. 0 = none
returns
   none
*/
#define MAPBORDERSIZE      6       // 1/10th
#define MAPBORDERSIZEINV   (1.0 / (fp)MAPBORDERSIZE)
void CMapWindow::CursorToScroll (POINT pCursor, fp *pfRight, fp *pfDown)
{
   RECT r;
   GetClientRect (m_hWnd, &r);

   // if not in rect then done
   if (!PtInRect (&r, pCursor)) {
      *pfRight = 0;
      *pfDown = 0;
      return;
   }

   // else, how much
   int iX = max(r.right - r.left, 1);
   int iY = max(r.bottom - r.top, 1);

   fp fRight = (fp)(pCursor.x - r.left) / (fp)iX;
   fp fDown = (fp)(pCursor.y - r.top) / (fp)iY;
   if (fRight < MAPBORDERSIZEINV)
      fRight = fRight * MAPBORDERSIZE - 1.0;
   else if (fRight > 1.0 - MAPBORDERSIZEINV)
      fRight = (fRight - (1.0 - MAPBORDERSIZEINV)) * MAPBORDERSIZE;
   else
      fRight = 0;

   if (fDown < MAPBORDERSIZEINV)
      fDown = fDown * MAPBORDERSIZE - 1.0;
   else if (fDown > 1.0 - MAPBORDERSIZEINV)
      fDown = (fDown - (1.0 - MAPBORDERSIZEINV)) * MAPBORDERSIZE;
   else
      fDown = 0;

   // limits
   if ((fRight < 0) && (m_pView.p[0] - m_pView.p[2]/2 <= m_pMapSize.p[0]))
      fRight = 0;
   if ((fRight > 0) && (m_pView.p[0] + m_pView.p[2]/2 >= m_pMapSize.p[1]))
      fRight = 0;
   if ((fDown > 0) && (m_pView.p[1] - m_pView.p[3]/2 <= m_pMapSize.p[2]))
      fDown = 0;
   if ((fDown < 0) && (m_pView.p[1] + m_pView.p[3]/2 >= m_pMapSize.p[3]))
      fDown = 0;

   *pfRight = fRight;
   *pfDown = fDown;
}


/*************************************************************************************
CMapWindow::MetersToClient - Converts a location in meters into the client (pixel) space.

inputs
   fp       fX - X
   fp       fY - Y
   int      *piX - Filled with X
   int      *piY - Filled with Y
*/
void CMapWindow::MetersToClient (fp fX, fp fY, LONG *piX, LONG *piY)
{
   fX = ((fX - m_pView.p[0]) / m_pView.p[2] + 0.5) * (fp)m_rClient.right;
   fY = (0.5 - (fY - m_pView.p[1]) / m_pView.p[3]) * (fp)m_rClient.bottom;
   *piX = (int)fX;
   *piY = (int)fY;
}


/*************************************************************************************
CMapWindow::ClientToMeters - Converts a location on the client (pixel) into meters.

inputs
   int      iX - pixels
   int      iY - pixels
   fp       *pfX - Filled with X
   fp       *pfY - Filled with Y
   int      *piY - Filled with Y
*/
void CMapWindow::ClientToMeters (int iX, int iY, fp *pfX, fp *pfY)
{
   *pfX = ((fp)iX / (fp)m_rClient.right - 0.5) * m_pView.p[2] + m_pView.p[0];
   *pfY = (0.5 - (fp)iY / (fp)m_rClient.bottom) * m_pView.p[3] + m_pView.p[1];
}


/*************************************************************************************
CMapWindow::PaintRoom - This paints a room.

inputs
   HDC         hDC - To paint on
   PCMapRoom   pRoom - Room to paint
   DWORD       dwPass - Pass 0 draws lines between rooms, pass 1 draws rooms
   int         iScale - Amount to scale everything by
   HFONT       hFontBig - Font to use
   HFONT       hFontSmall - Small font to use
*/
void CMapWindow::PaintRoom (HDC hDC, PCMapRoom pRoom, DWORD dwPass, int iScale, HFONT hFontBig, HFONT hFontSmall)
{
   RECT rClient;
   rClient = m_rClient;
   rClient.left *= iScale;
   rClient.right *= iScale;
   rClient.top *= iScale;
   rClient.bottom *= iScale;

   // figure out the location of this and everything it connects to
   RECT rBound;   // bounding box
   POINT pRoomLoc, pRoomSize;
   MetersToClient (pRoom->m_pLoc.p[0], pRoom->m_pLoc.p[1], &pRoomLoc.x, &pRoomLoc.y);

   // scale
   pRoomLoc.x *= iScale;
   pRoomLoc.y *= iScale;

   pRoomSize.x = (int)(pRoom->m_pLoc.p[2] / m_pView.p[2] * (fp)rClient.right);
   pRoomSize.y = (int)(pRoom->m_pLoc.p[3] / m_pView.p[2] * (fp)rClient.right);

   rBound.left = pRoomLoc.x - pRoomSize.x;
   rBound.right = pRoomLoc.x + pRoomSize.x;
   rBound.top = pRoomLoc.y - pRoomSize.y;
   rBound.bottom = pRoomLoc.y + pRoomSize.y;

   // account for other links
   DWORD i;
   POINT apConnectTo[ROOMEXITS];
   PCMapRoom paRoom[ROOMEXITS];
   memset (paRoom, 0, sizeof(paRoom));
   for (i = 0; i < ROOMEXITS; i++) {
      if (IsEqualGUID(pRoom->m_agExits[i], GUID_NULL))
         continue;

      PCMapRoom pCon = RoomFind (&pRoom->m_agExits[i]);
      if (!pCon || (pCon->m_pZone != pRoom->m_pZone) || (pCon->m_pRegion != pRoom->m_pRegion) || (pCon->m_pMap != pRoom->m_pMap))
         pCon = NULL;   // cant draw a link to because either doesn't exist or not on the same map
      if (pCon) {
         paRoom[i] = pCon;
         MetersToClient (pCon->m_pLoc.p[0], pCon->m_pLoc.p[1], &apConnectTo[i].x, &apConnectTo[i].y);

         // scale
         apConnectTo[i].x *= iScale;
         apConnectTo[i].y *= iScale;
      }
      else {
         // draw a line in the general direction...
         if (i >= 8)
            continue;   // up, down, in, out... can't really draw this

         // length
         int iLen = (int)((fp)max(pRoomSize.x, pRoomSize.y) * 1.2 / 2.0);
            // BUGFIX - Was *2 if unknown room. Instead make it less because was too long
         apConnectTo[i] = pRoomLoc;
         switch (i) {
         case 2: // E
         case 1: // NE
         case 3: // SE
            apConnectTo[i].x += iLen;
            break;
         case 6: // W
         case 7: // NW
         case 5: // SW
            apConnectTo[i].x -= iLen;
            break;
         } // switch
         switch (i) {
         case 0: // N
         case 1: // NE
         case 7: // NW
            apConnectTo[i].y -= iLen;
            break;
         case 4: // S
         case 3: // SE
         case 5: // SW
            apConnectTo[i].y += iLen;
            break;
         } // switch
         paRoom[i] = pRoom;   // just to use as flag so know draw line
      } // if no room

      // min/max this
      rBound.left = min(rBound.left, apConnectTo[i].x);
      rBound.right = max(rBound.right, apConnectTo[i].x);
      rBound.top = min(rBound.top, apConnectTo[i].y);
      rBound.bottom = max(rBound.bottom, apConnectTo[i].y);
   } // i

   // if the bounding box isn't in the client then dont bother
   if ((rBound.right < rClient.left) || (rBound.left > rClient.right) ||
      (rBound.bottom < rClient.top) || (rBound.top > rClient.bottom))
      return;

   // draw the lines connecting the room to other rooms
   HPEN hPenOld, hPen;
   if (dwPass == 0) {
      COLORREF cLine;
      cLine = m_pMain->m_fLightBackground ? RGB(0x40, 0x40, 0x40) : RGB(0x80, 0x80, 0x80);
      hPen = CreatePen (PS_SOLID, 1 * iScale, cLine);
      hPenOld = (HPEN)SelectObject (hDC, hPen);
      for (i = 0; i < ROOMEXITS; i++) {
         if (!paRoom[i])
            continue;   // no connection

         MoveToEx (hDC, pRoomLoc.x, pRoomLoc.y, NULL);
         LineTo (hDC, apConnectTo[i].x, apConnectTo[i].y);
      } // i
      SelectObject (hDC, hPenOld);
      DeleteObject (hPen);
   }
   else if (dwPass == 1) {

      // draw the box
   #define MAXPOLY      32    // max polygon ponints
      POINT apPoly[MAXPOLY];
      DWORD dwPoints;
      DWORD j;

      if (pRoom->m_dwShape == 1) {  // circle
         dwPoints = (max(pRoomSize.x, pRoomSize.y) < 20) ? (MAXPOLY/2) : MAXPOLY;
         for (j = 0; j < dwPoints; j++) {
            fp fAngle = (fp)j / (fp)dwPoints * PI * 2.0;
            apPoly[j].x = (int)(sin(fAngle) * (fp)pRoomSize.x/2);
            apPoly[j].y = (int)(cos(fAngle) * (fp)pRoomSize.y/2);
         } // j
      }
      else { // box
         apPoly[0].x = - pRoomSize.x / 2;
         apPoly[0].y = - pRoomSize.y / 2;
         apPoly[1] = apPoly[0];
         apPoly[1].x = pRoomSize.x / 2;
         apPoly[2] = apPoly[1];
         apPoly[2].y = pRoomSize.y / 2;
         apPoly[3] = apPoly[2];
         apPoly[3].x = apPoly[0].x;
         dwPoints = 4;
      }
      
      // rotate it
      if (pRoom->m_fRotation) {
         fp am[2][2];
         TEXTUREPOINT tp;
         TextureMatrixRotate (am, pRoom->m_fRotation);

         for (j = 0; j < dwPoints; j++) {
            tp.h = apPoly[j].x;
            tp.v = apPoly[j].y;
            TextureMatrixMultiply2D (am, &tp);
            apPoly[j].x = (int)tp.h;
            apPoly[j].y = (int)tp.v;
         } // j
      }

      // offset
      for (j = 0; j < dwPoints; j++) {
         apPoly[j].x += pRoomLoc.x;
         apPoly[j].y += pRoomLoc.y;
      } // j

      // is this the current one?
      BOOL fCur = IsEqualGUID (pRoom->m_gID, m_gRoomCur);

      HBRUSH hBrush, hBrushOld;
      COLORREF cPen;
      if (!m_pMain->m_fLightBackground)
         cPen = fCur ? RGB(0xff, 0x80, 0x80) : RGB(0x80, 0x80, 0x80);
      else
         cPen = fCur ? RGB(0x80, 0, 0) : RGB(0x40, 0x40, 0x40);
      hPen = CreatePen (PS_SOLID, (fCur ? 4 : 2) * iScale, cPen);

      hPenOld = (HPEN)SelectObject (hDC, hPen);
      hBrush = CreateSolidBrush (pRoom->m_acColor[m_pMain->m_fLightBackground ? 1 : 0]);
      hBrushOld = (HBRUSH) SelectObject (hDC, hBrush);

      Polygon (hDC, &apPoly[0], dwPoints);

      SelectObject (hDC, hPenOld);
      DeleteObject (hPen);
      SelectObject (hDC, hBrushOld);
      DeleteObject (hBrush);

      // draw text
      if (max(pRoomSize.x, pRoomSize.y) > 40 * iScale) {
         PWSTR pszEllipsis = L"(...)";
         PWSTR pszName = (PWSTR)pRoom->m_memName.p;
         PWSTR pszDescription = (PWSTR)pRoom->m_memDescription.p;
         if (!pszDescription || !pszDescription[0])
            pszDescription = NULL;

         // rectangle to draw in.. alow to go beyond box
         RECT r, r2;
         r.left = pRoomLoc.x - pRoomSize.x/2;
         r.right = pRoomLoc.x + pRoomSize.x/2;
         r.top = pRoomLoc.y - pRoomSize.y;
         r.bottom = pRoomLoc.y + 2*pRoomSize.y; // so can get larger
         r2 = r;

         HFONT hFontOld = (HFONT) SelectObject (hDC, hFontBig);
         int iOldMode = SetBkMode (hDC, TRANSPARENT);

         // figure out size of the title
         DrawTextW (hDC, pszName, -1, &r, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
         int iTitleWidth = r.right - r.left;
         int iTitleHeight = r.bottom - r.top;

         // different font for text
         SelectObject (hDC, hFontSmall);

         // figure out how large the description text is
         RECT rDesc = r2;
         int iDescWidth = 0, iDescHeight = 0;
         if (pszDescription) {
            DrawTextW (hDC, pszDescription, -1, &rDesc, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX | DT_CALCRECT);
            iDescWidth = rDesc.right - rDesc.left;
            iDescHeight = rDesc.bottom - rDesc.top;

            if (iDescHeight + iTitleHeight > pRoomSize.y*2) {
               // too large
               pszDescription = pszEllipsis;  // to indicate more
               rDesc = r2;
               DrawTextW (hDC, pszDescription, -1, &rDesc, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX | DT_CALCRECT);
               iDescWidth = rDesc.right - rDesc.left;
               iDescHeight = rDesc.bottom - rDesc.top;
            }
         }

         r.left = pRoomLoc.x - max(iTitleWidth, iDescWidth)/2;
         r.right = r.left + max(iTitleWidth, iDescWidth);
         r.top = pRoomLoc.y - (iTitleHeight + iDescHeight)/2;
         r.bottom = r.top + (iTitleHeight + iDescHeight);

         // draw the description

         // may want different text color for out-of-date description
         // see if the room is on a recent-room list
         GUID *pg = (GUID*) m_lLastUpdate.Get(0);
         for (i = 0; i < m_lLastUpdate.Num(); i++, pg++)
            if (IsEqualGUID (*pg, pRoom->m_gID))
               break;
         BOOL fRecent = (i < m_lLastUpdate.Num());

         COLORREF cText;
         if (!m_pMain->m_fLightBackground)
            cText = fRecent ? RGB(0xff,0xff,0x80) : RGB(0xc0, 0xc0, 0xc0);
         else
            cText = fRecent ? RGB(0x40,0x40,0) : RGB(0x40, 0x40, 0x40);

         SetTextColor (hDC, cText);

         rDesc = r;
         rDesc.top += iTitleHeight;
         if (pszDescription == pszEllipsis) {
            // so ellipsis centered under title
            rDesc.left = pRoomLoc.x - iDescWidth/2;
            rDesc.right = rDesc.left + iDescWidth;
         }
         if (pszDescription)
            DrawTextW (hDC, pszDescription, -1, &rDesc, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);

         // different font for title
         SelectObject (hDC, hFontBig);
         cText = m_pMain->m_fLightBackground ? RGB(0x10,0x10,0x00) : RGB(0xff,0xff,0xff);
         SetTextColor (hDC, cText);

         // draw the title
         r2 = r;
         r2.left = pRoomLoc.x - iTitleWidth/2;
         r2.right = r2.left + iTitleWidth;
         DrawTextW (hDC, pszName, -1, &r2, DT_WORDBREAK | DT_NOPREFIX | DT_CENTER);
               // BUGFIX - Was DT_VCENTER and NOT DT_CENTER, but need long text centered

         SelectObject (hDC, hFontOld);
         SetBkMode (hDC, iOldMode);
      } // if draw text
   } // dwPass==1
}



/*************************************************************************************
CMapWindow::PopupMenu - This creates a popup menu of all the rooms in the Window.
If there is only one room, it doesn't create a popup menu, but returns a pointer
to the room.

inputs
   PCListFixed       plPCMapMap - List that's initialized to sizeof(PCMapRoom).
                        When a room is added to a popup, it will be added to this list.
                        The menu ID is 1000 + index into this list.
   PCMapMap          pCurMap - Current map (so can check it)
   PCMapMap          *ppMap - If the function returns NULL (non popup), then this
                        will be NULL or a single room from the list.
returns
   HMENU - Menu created, or NULL. This must be freed
*/
HMENU CMapWindow::PopupMenu (PCListFixed plPCMapMap, PCMapMap pCurMap, PCMapMap *ppMap)
{
   *ppMap = NULL;
   HMENU hMenuSingle = NULL;
   HMENU hMenu = NULL;
   PCMapZone pZoneSingle = NULL;

   // loop through
   CMem mem;
   DWORD dwLen, i;
   for (i = 0; i < ZoneNum(); i++) {
      PCMapZone pZone = ZoneGet(i);
      PCMapMap pMap2 = NULL;
      HMENU hMenu2 = pZone->PopupMenu (plPCMapMap, pCurMap, &pMap2);

      // if menu then...
      if (hMenu2) {
         if (!hMenu && !hMenuSingle && !(*ppMap)) {
            // only found one thing so far, and it's a menu
            hMenuSingle = hMenu2;
            pZoneSingle = pZone;
            continue;
         }
      }
      else if (pMap2) {
         if (!hMenu && !hMenuSingle && !(*ppMap)) {
            // only found one thing so far, and it's a room
            *ppMap = pMap2;
            continue;
         }
      }
      else
         continue;   // no match at all

      // create menu if it's not already
      if (!hMenu)
         hMenu = CreatePopupMenu ();
      if (!hMenu)
         continue;   // error

      // add pre-existing items
      if (*ppMap) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)(*ppMap)->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)(*ppMap)->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         // add to the menu
         plPCMapMap->Add (ppMap);
         AppendMenu (hMenu,
            ((pCurMap == *ppMap) ? MF_CHECKED : 0) | MF_ENABLED,
            plPCMapMap->Num() - 1 + 1000, (char*)mem.p);

         *ppMap = NULL;
      }
      if (hMenuSingle) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)pZoneSingle->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)pZoneSingle->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         AppendMenu (hMenu, MF_POPUP | MF_ENABLED, (UINT_PTR) hMenuSingle, (char*)mem.p);
         hMenuSingle = NULL; // since don't need to delete
      }

      // add new items
      if (pMap2) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)pMap2->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)pMap2->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         // add to the menu
         plPCMapMap->Add (&pMap2);
         AppendMenu (hMenu,
            ((pCurMap == pMap2) ? MF_CHECKED : 0) | MF_ENABLED,
            plPCMapMap->Num() - 1 + 1000, (char*)mem.p);
      }
      if (hMenu2) {
         // convert the name
         dwLen = ((DWORD)wcslen((PWSTR)pZone->m_memName.p)+1)*sizeof(WCHAR);
         if (!mem.Required (dwLen))
            continue; // error
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)pZone->m_memName.p, -1, (char*)mem.p, (DWORD)mem.m_dwAllocated, 0, 0);

         AppendMenu (hMenu, MF_POPUP | MF_ENABLED, (UINT_PTR) hMenu2, (char*)mem.p);
      }

   } // i


   return hMenu ? hMenu : hMenuSingle;

}



/*************************************************************************************
CMapWindow::RoomBoundingBox - Finds the bounding box for this rooom
and neighboring rooms.

inputs
   PCMapRoom      pCurRoom - Current room to look at
   DWORD          dwDist - Number of rooms distant from this to look. If 0, only does this room
                     Only neighboring rooms on the same map will be used
   PCPoint        pMinMax - p[0] filled with left, p[1] with top, p[2] with right, p[3] with bottom
                     Initially fill with pCurRoom's center.
returns
   none
*/
void CMapWindow::RoomBoundingBox (PCMapRoom pCurRoom, DWORD dwDist, PCPoint pMinMax)
{
   pMinMax->p[0] = min(pMinMax->p[0], pCurRoom->m_pLoc.p[0] - pCurRoom->m_pLoc.p[2] / 2.0);
   pMinMax->p[1] = min(pMinMax->p[1], pCurRoom->m_pLoc.p[1] - pCurRoom->m_pLoc.p[3] / 2.0);
   pMinMax->p[2] = max(pMinMax->p[2], pCurRoom->m_pLoc.p[0] + pCurRoom->m_pLoc.p[2] / 2.0);
   pMinMax->p[3] = max(pMinMax->p[3], pCurRoom->m_pLoc.p[1] + pCurRoom->m_pLoc.p[3] / 2.0);

   // surrounding rooms
   DWORD i;
   if (dwDist && pCurRoom->m_pMap) for (i = 0; i < ROOMEXITS; i++) {
      if (IsEqualGUID(pCurRoom->m_agExits[i], GUID_NULL))
         continue;   // empty

      // get the room
      PCMapRoom pNeighbor = pCurRoom->m_pMap->RoomFind (&pCurRoom->m_agExits[i]);
      if (!pNeighbor)
         continue;

      // include this
      RoomBoundingBox (pNeighbor, dwDist-1, pMinMax);
   } // i
}


/*************************************************************************************
CMapWindow::RoomCenter - Center on the given room (and surrounding few rooms)

inputs
   PCMapRoom      pCurRoom - Current room to look at
*/
void CMapWindow::RoomCenter (PCMapRoom pCurRoom)
{
   // make this the current one
   MemZero (&m_memCurMap);
   MemCat (&m_memCurMap, (PWSTR) pCurRoom->m_pMap->m_memName.p);
   MemZero (&m_memCurRegion);
   MemCat (&m_memCurRegion, (PWSTR) pCurRoom->m_pRegion->m_memName.p);
   MemZero (&m_memCurZone);
   MemCat (&m_memCurZone, (PWSTR) pCurRoom->m_pZone->m_memName.p);

   m_pView.p[0] = pCurRoom->m_pLoc.p[0];
   m_pView.p[1] = pCurRoom->m_pLoc.p[1];

   // find the bounding box
   CPoint pMinMax;
   pMinMax.p[0] = pMinMax.p[2] = pCurRoom->m_pLoc.p[0];
   pMinMax.p[1] = pMinMax.p[3] = pCurRoom->m_pLoc.p[1];
   RoomBoundingBox (pCurRoom, 1, &pMinMax);

   // so view two rooms around, and a bit
   m_pView.p[2] = 1.5 * max(pMinMax.p[3] - pMinMax.p[1], pMinMax.p[2] - pMinMax.p[0]);

   RecalcView();
}


/*************************************************************************************
CMapWindow::ContextMenu - This displays the context menu when clicking on the map.
*/
void CMapWindow::ContextMenu (void)
{
   BeepWindowBeep (ESCBEEP_MENUOPEN);

   HMENU hMenu = CreatePopupMenu ();
   if (!hMenu)
      return;

   // add menu options
   AppendMenu (hMenu, MF_STRING | MF_ENABLED, 10, "Zoom in");
   AppendMenu (hMenu, MF_STRING | MF_ENABLED, 11, "Zoom out");
   AppendMenu (hMenu, MF_STRING | MF_ENABLED, 12, "Show all");

   // jump to current room menu
   PCMapRoom pCurRoom = RoomFind (&m_gRoomCur);
   if (pCurRoom)
      AppendMenu (hMenu, MF_STRING | MF_ENABLED, 13, "Go to current room");

   AppendMenu (hMenu, MF_SEPARATOR, 0, NULL);


   // find the current map
   PCMapZone pCurZone = ZoneFind ((PWSTR)m_memCurZone.p);
   PCMapRegion pCurRegion = pCurZone ? pCurZone->RegionFind ((PWSTR)m_memCurRegion.p) : NULL;
   PCMapMap pCurMap = pCurRegion ? pCurRegion->MapFind ((PWSTR)m_memCurMap.p) : NULL;

   // add the options for switching maps
   CListFixed lPCMapMap;
   lPCMapMap.Init (sizeof(PCMapMap));
   PCMapMap pMap = NULL;
   HMENU hMenu2 = PopupMenu (&lPCMapMap, pCurMap, &pMap);
   if (hMenu2) {
      AppendMenu (hMenu, MF_POPUP | MF_ENABLED, (UINT_PTR) hMenu2, "Maps");
      AppendMenu (hMenu, MF_SEPARATOR, 0, NULL);
   }
   // else, there's only one map, so don't bother

   AppendMenu (hMenu, MF_STRING | MF_ENABLED, 20, "Erase this map");
   AppendMenu (hMenu, MF_STRING | MF_ENABLED, 21, "Erase all maps");


   // show the menu
   POINT p;
   GetCursorPos (&p);
   int iRet;
   iRet = TrackPopupMenu (hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
      p.x, p.y, 0, m_hWnd, NULL);
   DestroyMenu (hMenu); // BUGFIX - added


   // handle map clicking
   if ((iRet >= 1000) && (iRet < 1000 + (int)lPCMapMap.Num())) {
      pMap = *((PCMapMap*) lPCMapMap.Get(iRet - 1000));

      // view entire thing by default
      m_pView.p[2] = 0; // so will view all

      // make this the current one
      MemZero (&m_memCurMap);
      MemCat (&m_memCurMap, (PWSTR) pMap->m_memName.p);
      MemZero (&m_memCurRegion);
      if (pMap->m_pRegion)
         MemCat (&m_memCurRegion, (PWSTR) pMap->m_pRegion->m_memName.p);
      MemZero (&m_memCurZone);
      if (pMap->m_pZone)
         MemCat (&m_memCurZone, (PWSTR) pMap->m_pZone->m_memName.p);


      RecalcView ();
      return;
   }

   switch (iRet) {
   case 10: // zoom in
      m_pView.p[2] /= sqrt((fp)2);
      RecalcView();
      return;

   case 11: // zoom out
      m_pView.p[2] *= sqrt((fp)2);
      RecalcView();
      return;

   case 12: // show all
      m_pView.p[2] = 0; // force to show all
      RecalcView();
      return;

   case 13: // go to current room
      if (pCurRoom)
         RoomCenter (pCurRoom);
      return;

   case 20: // erase this map
      {
         if (IDYES != EscMessageBox (m_pMain->m_hWndPrimary, gpszCircumrealityClient,
            L"Are you sure you wish to erase this map?",
            L"You will NOT be able to undo the erasure.", MB_YESNO))
            return;

         DWORD dwIndex = pCurRegion->MapFindIndex ((PWSTR) pCurMap->m_memName.p);
         pCurRegion->MapRemove (dwIndex, TRUE, &m_hRooms);
         ClearDeadMaps();
         RecalcView();
      }
      return;

   case 21: // erase all maps
      {
         if (IDYES != EscMessageBox (m_pMain->m_hWndPrimary, gpszCircumrealityClient,
            L"Are you sure you wish to erase ALL the maps?",
            L"You will NOT be able to undo the erasure.", MB_YESNO))
            return;

         DeleteAll (&m_hRooms);
         ClearDeadMaps();
         RecalcView();
      }
      return;
   } // switch
}



/*************************************************************************************
CMapWindow::IsOverRoom - Given a pixel (in client coords), this determines if it's over
a room, and returns the room.

inputs
   POINT       *p - Client coords
returns
   PCMapRoom - Room, or NULL if not over one
*/
PCMapRoom CMapWindow::IsOverRoom (POINT *p)
{
   // find the current map
   PCMapZone pCurZone = ZoneFind ((PWSTR)m_memCurZone.p);
   PCMapRegion pCurRegion = pCurZone ? pCurZone->RegionFind ((PWSTR)m_memCurRegion.p) : NULL;
   PCMapMap pCurMap = pCurRegion ? pCurRegion->MapFind ((PWSTR)m_memCurMap.p) : NULL;
   if (!pCurMap)
      return NULL;
   
   // convert to coords
   fp fX, fY;
   ClientToMeters (p->x, p->y, &fX, &fY);

   // see if it's over anything
   DWORD i;
   for (i = 0; i < pCurMap->RoomNum(); i++) {
      PCMapRoom pr = pCurMap->RoomGet(i);
      if ((fX >= pr->m_pLoc.p[0] - pr->m_pLoc.p[2]/2) &&
         (fX <= pr->m_pLoc.p[0] + pr->m_pLoc.p[2]/2) &&
         (fY >= pr->m_pLoc.p[1] - pr->m_pLoc.p[3]/2) &&
         (fY <= pr->m_pLoc.p[1] + pr->m_pLoc.p[3]/2))
         return pr;
   } // i

   // else, no match
   return NULL;
}


/*************************************************************************************
CMapWindow::WalkToDirection - Returns the direction the user must walk, 0..ROOMEXITS-1
if they wish to go to the room. Or, -1 if cant get there

inputs
   PCMapRoom         pRoom - Room wanting to walk to
returns
   DWORD - Direction, 0..ROOMEXITS-1, ROOMEXITS for a distant room, or -1 if cant go
*/
DWORD CMapWindow::WalkToDirection (PCMapRoom pRoom)
{
   // get the current room
   PCMapRoom pCurRoom = RoomFind (&m_gRoomCur);
   if (!pCurRoom)
      return -1;

   // loop
   DWORD i;
   for (i = 0; i < ROOMEXITS; i++)
      if (IsEqualGUID(pCurRoom->m_agExits[i], pRoom->m_gID))
         return i;

   // else none
   return ROOMEXITS; // distant room
}


/*************************************************************************************
CMapWindow::WalkToDirection - Returns the direction the user must walk, 0..ROOMEXITS-1
if they wish to go to the room. Or, -1 if cant get there

inputs
   POINT       *p - Client coords
returns
   DWORD - Direction, 0..ROOMEXITS-1, or -1 if cant go
*/
DWORD CMapWindow::WalkToDirection (POINT *p)
{
   PCMapRoom pRoom = IsOverRoom (p);
   if (pRoom)
      return WalkToDirection (pRoom);
   else
      return -1;
}


/*************************************************************************************
CMapWindow::MapPointTo - Tells the map to point to a location

inputs
   PWSTR          pszText - Text to display
   PCPoint        pLoc - Location in XYZ
   COLORREF       cColor - Color. if -1 then default
returns
   none
*/
void CMapWindow::MapPointTo (PWSTR pszText, PCPoint pLoc, COLORREF cColor)
{
   if (cColor == (COLORREF)-1) {
      m_acMapPointTo[0] = RGB(0x00,0xff,0x00);
      m_acMapPointTo[1] = RGB(0x00, 0x40, 0x00);
   }
   else
      m_acMapPointTo[0] = m_acMapPointTo[1] = cColor;

   m_fMapPointTo = TRUE;
   m_pMapPointTo.Copy(pLoc);
   MemZero (&m_memMapPointTo);
   MemCat (&m_memMapPointTo, pszText);

   // refresh the map
   if (m_hWnd)
      InvalidateRect (m_hWnd, NULL, FALSE);
}



/*************************************************************************************
CMapWindow::PaintMapPointTo - Draws the line for the pointer

inputs
   HDC            hDC - HDC
   int            iScale - Amount to scale everything by
   HFONT          hFont - Font to use
*/
void CMapWindow::PaintMapPointTo (HDC hDC, int iScale, HFONT hFont)
{
   // if not current room then dont paint
   if (IsEqualGUID (m_gRoomCur, GUID_NULL))
      return;

   // find it
   PCMapRoom pCurRoom = RoomFind (&m_gRoomCur);
   if (!pCurRoom)
      return;
   CPoint apLoc[2];
   apLoc[0].Zero();
   apLoc[0].p[0] = pCurRoom->m_pLoc.p[0];
   apLoc[0].p[1] = pCurRoom->m_pLoc.p[1];
   apLoc[1].Copy(&m_pMapPointTo);
   apLoc[1].p[3] = 1;   // just in case

   // if they're close then ignore
   if (apLoc[0].AreClose (&apLoc[1]))
      return;

   // find the boundaries of the client rect
   CPoint pUL, pLR;
   pUL.Zero();
   pLR.Zero();
   ClientToMeters (0, 0, &pUL.p[0], &pUL.p[1]);
   ClientToMeters (m_rClient.right, m_rClient.bottom, &pLR.p[0], &pLR.p[1]);

   // clip
   CRenderClip rc;
   CPoint pN, pP;

   // left clip plane
   pN.Zero4();
   pP.Zero4();
   pN.p[0] = -1;
   pP.p[0] = pUL.p[0];
   rc.AddPlane (&pN, &pP);

   // right clip plane
   pN.Zero4();
   pP.Zero4();
   pN.p[0] = 1;
   pP.p[0] = pLR.p[0];
   rc.AddPlane (&pN, &pP);

   // top clip plane
   pN.Zero4();
   pP.Zero4();
   pN.p[1] = 1;
   pP.p[1] = pUL.p[1];
   rc.AddPlane (&pN, &pP);

   // bottom clip plane
   pN.Zero4();
   pP.Zero4();
   pN.p[1] = -1;
   pP.p[1] = pLR.p[1];
   rc.AddPlane (&pN, &pP);

   // figure out the clip bits
   DWORD adwBits[2];
   rc.CalcClipBits ((DWORD)-1, 2, apLoc, adwBits);

   // figure out which bits change and clip the line according to those
   DWORD i, j;
   for (i = 0; i < 4; i++) {
      BOOL afClip[2];
      for (j = 0; j < 2; j++)
         afClip[j] = (adwBits[j] & (1 << i)) ? TRUE : FALSE;
      if (afClip[0] && afClip[1])
         return;  // totally clipped
      if (!afClip[0] && !afClip[1])
         continue;   // not clipped

      // else,  clip
      DWORD dwNC = afClip[0] ? 1 : 0;  // not-clipped
      rc.ClipLine (i, &apLoc[dwNC], NULL, NULL, NULL, &apLoc[1 - dwNC], NULL, NULL, NULL);

      // since clipped this, need to recompare
      rc.CalcClipBits ((DWORD)-1, 2, apLoc, adwBits);
   } // i

   // if get here, have a line
   LONG iX[3], iY[3];
   MetersToClient (apLoc[0].p[0], apLoc[0].p[1], &iX[0], &iY[0]);
   MetersToClient (apLoc[1].p[0], apLoc[1].p[1], &iX[1], &iY[1]);

   // scale
   iX[0] *= iScale;
   iX[1] *= iScale;
   iY[0] *= iScale;
   iY[1] *= iScale;

   // if same then skip
   if ((iX[0] == iX[1]) && (iY[0] == iY[1]))
      return;

   // draw it
#define ARROWSIZE       (10*iScale)
#define ARROWSPACE      (ARROWSIZE * 8)
#define ARROWTHICKNESS  (5*iScale)
   HPEN hPen = CreatePen (PS_SOLID, ARROWTHICKNESS, m_acMapPointTo[m_pMain->m_fLightBackground ? 1 : 0]);
   HPEN hPenOld = (HPEN) SelectObject (hDC, hPen);
   MoveToEx (hDC, iX[0], iY[0], NULL);
   LineTo (hDC, iX[1], iY[1]);

   // how many arrows
   DWORD dwArrows = (DWORD) sqrt((fp)((iX[0] - iX[1]) * (iX[0] - iX[1]) + (iY[0] - iY[1]) * (iY[0] - iY[1]))) / ARROWSPACE + 2;
   CPoint pCenter, pLeft, pDir, pZ, pArrow;
   pZ.Zero();
   pZ.p[2] = 1;
   pDir.Zero();
   pDir.p[0] = iX[1] - iX[0];
   pDir.p[1] = iY[1] - iY[0];
   pDir.Normalize();
   for (i = 1; i < dwArrows; i++) { // NOTE: Always skipping first point
      fp fAlpha = (fp)i / (fp)(dwArrows-1);

      pCenter.Zero();
      pCenter.p[0] = fAlpha * (fp)(iX[1] - iX[0]) + (fp)iX[0];
      pCenter.p[1] = fAlpha * (fp)(iY[1] - iY[0]) + (fp)iY[0];

      pLeft.CrossProd (&pDir, &pZ);
         // know that will be normalized

      // make the arrow
      for (j = 0; j < 2; j++) {
         pArrow.Copy (&pDir);
         pArrow.Scale (-1);
         if (j)
            pArrow.Add (&pLeft);
         else
            pArrow.Subtract (&pLeft);
         pArrow.Scale (ARROWSIZE);
         pArrow.Add (&pCenter);

         MoveToEx (hDC, (int)pArrow.p[0], (int)pArrow.p[1], NULL);
         LineTo (hDC, (int)pCenter.p[0], (int)pCenter.p[1]);
      } // j
   } // i
   
   SelectObject (hDC, hPenOld);
   DeleteObject (hPen);

   // find the center of the line
   iX[2] = (iX[0] + iX[1]) / 2;
   iY[2] = (iY[0] + iY[1]) / 2;

   HFONT hFontOld = (HFONT) SelectObject (hDC, hFont);
   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   SetTextColor (hDC, m_acMapPointTo[m_pMain->m_fLightBackground ? 1 : 0]);

   // if we're basically horizontal then draw text over the line
   PWSTR psz = (PWSTR) m_memMapPointTo.p;
   RECT rSize;
   rSize = m_rClient;
   rSize.left *= iScale;
   rSize.right *= iScale;
   rSize.top *= iScale;
   rSize.bottom *= iScale;
   if (fabs(pDir.p[0]) > 0.9) { // straight
      OffsetRect (&rSize, iX[2] - (rSize.left + rSize.right)/2, 0);   // so centered
      if (iY[2] < (rSize.top + rSize.bottom)/2) {
         // draw below
         OffsetRect (&rSize, 0, max(iY[0], iY[1]) - rSize.top + ARROWTHICKNESS);
         DrawTextW (hDC, psz, -1, &rSize, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_WORD_ELLIPSIS);
      }
      else {
         // draw above
         OffsetRect (&rSize, 0, min(iY[0], iY[1]) - rSize.bottom - ARROWTHICKNESS);
         DrawTextW (hDC, psz, -1, &rSize, DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_WORD_ELLIPSIS);
      }
   }
   else {   // angle
      fp fSlope = pDir.p[1] * ((pDir.p[0] >= 0) ? 1 : -1);
      if (fSlope > 0) {
         // downward slope, text to upper right
         OffsetRect (&rSize, iX[2] + ARROWTHICKNESS, iY[2] - rSize.bottom - ARROWTHICKNESS);
         DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_WORD_ELLIPSIS);
      }
      else {
         // upward slope, text to lower right
         OffsetRect (&rSize, iX[2] + ARROWTHICKNESS, iY[2] - rSize.top + ARROWTHICKNESS);
         DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_WORD_ELLIPSIS);
      }
   }
   SelectObject (hDC, hFontOld);
   SetBkMode (hDC, iOldMode);
}


/*************************************************************************************
CMapWindow:Generate360InfoRecurse - Recursive function to add to map info.

inputs
   GUID              *pgRoom - Room to add.
   DWORD             dwDistance - If 0, then only add this room. 1 then add this room and immediately
                     surounding, etc.
   DWORD             dwThisDistance - Distance to use for this.
   PCMapRoom         pRoomCenter - Room at the center
   PCListFixed       plMAP360INFO - To Add to
   PCListVariable    plMapStrings - To add to
returns
   none
*/
void CMapWindow::Generate360InfoRecurse (GUID *pgRoom, DWORD dwDistance, DWORD dwThisDistance, PCMapRoom pRoomCenter,
                                         PCListFixed plMAP360INFO, PCListVariable plMapStrings)
{
   // if it already exists on the list then don't add
   DWORD i;
   PMAP360INFO pmi = (PMAP360INFO)plMAP360INFO->Get(0);
   for (i = 0; i < plMAP360INFO->Num(); i++, pmi++)
      if (IsEqualGUID(pmi->gRoom, *pgRoom))
         return;

   // get this room
   PCMapRoom pRoom = RoomFind (pgRoom);
   if (!pRoom)
      return;  // nothing

   // basic info
   MAP360INFO mi;
   memset (&mi, 0, sizeof(mi));
   mi.gRoom = *pgRoom;
   mi.dwRoomDist = dwThisDistance;
   PWSTR pszDescription = (PWSTR)pRoom->m_memDescription.p;
   if (pszDescription && pszDescription[0]) {
      plMapStrings->Add (pszDescription, (wcslen(pszDescription) + 1) * sizeof(WCHAR));
      mi.pszDescription = (PWSTR) plMapStrings->Get(plMapStrings->Num()-1);
   }  // else, leave as NULL

   // distance
   CPoint pVector;
   pVector.Subtract (&pRoom->m_pLoc, &pRoomCenter->m_pLoc);
   mi.fDistance = pVector.Length();
   mi.fAngle = atan2(pVector.p[0], pVector.p[1]);

   plMAP360INFO->Add (&mi);

   // surrounding rooms
   if (dwDistance) for (i = 0; i < ROOMEXITS; i++) {
      if (IsEqualGUID (pRoom->m_agExits[i], GUID_NULL))
         continue;

      Generate360InfoRecurse (&pRoom->m_agExits[i], dwDistance-1, dwThisDistance+1, pRoomCenter, plMAP360INFO, plMapStrings);
   } // i

}


/*************************************************************************************
CMapWindow:Generate360Info - Generates the information to be displayed on the 360-degree
view.

inputs
   GUID              *pgRoom - Room.
   PCListFixed       plMAP360INFO - Initialized and filled in with a sorted list of MAP360INFO
                     structures. Sorted so the first is the most distant.
   PCListVariable    plMapStrings - Initialized and filled with strings that are pointed
                     to by plMAP360INFO.
returns
   none
*/


static int _cdecl MAP360INFOSort (const void *elem1, const void *elem2)
{
   MAP360INFO *pdw1, *pdw2;
   pdw1 = (MAP360INFO*) elem1;
   pdw2 = (MAP360INFO*) elem2;

   // sort by distance
   if (pdw1->fDistance > pdw2->fDistance)
      return -1;
   else if (pdw1->fDistance < pdw2->fDistance)
      return 1;
   else
      return memcmp (pdw1, pdw2, sizeof(*pdw1)); // something arbitrary
}

void CMapWindow::Generate360Info (GUID *pgRoom, PCListFixed plMAP360INFO, PCListVariable plMapStrings)
{
   // initialize
   plMAP360INFO->Init (sizeof(MAP360INFO));
   plMapStrings->Clear();

   // if not any room then nothing
   if (IsEqualGUID (m_gRoomCur, GUID_NULL))
      return;

   // need to figure out which room and map we're in
   PCMapRoom pCenter = RoomFind (&m_gRoomCur);
   if (!pCenter)
      return;  // doesnt exist

   Generate360InfoRecurse (&m_gRoomCur, 2 /* rooms distant */, 0, pCenter, plMAP360INFO, plMapStrings);

   // get rid of the first element since must be the center room
   plMAP360INFO->Remove (0);

#if 0
   // this is a hack just to fill in some info
   PWSTR apszDir[] = {L"North", L"North-East", L"East", L"South-East", L"South", L"South-West", L"West", L"North-West"};
   DWORD i;
   MAP360INFO mi;
   memset (&mi, 0, sizeof(mi));
   for (i = 0; i < sizeof(apszDir) / sizeof(apszDir[0]); i++) {
      plMapStrings->Add (apszDir[i], (wcslen(apszDir[i]) + 1) * sizeof(WCHAR));
      mi.dwRoomDist = (i % NUMMAINFONT) + 1;
      mi.fDistance = i;
      mi.fAngle = (fp)i / 8.0 * 2.0 * PI;
      mi.pszDescription = (PWSTR) plMapStrings->Get(i);

      plMAP360INFO->Add (&mi);
   } // i
#endif // 0

   // sort the list
   qsort (plMAP360INFO->Get(0), plMAP360INFO->Num(), sizeof(MAP360INFO), MAP360INFOSort);
}


// BUGBUG - Eventually, may want to allow users to annotate their map with text
// or pictures

// BUGBUG - when select "erase this map" for a second time on the same map, crashes the client
