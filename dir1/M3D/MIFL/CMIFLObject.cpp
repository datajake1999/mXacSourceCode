/*************************************************************************************
CMIFLObject.cpp - Definition for a object

begun 7/1/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"

// RECOMMENDPROP - Information for internal sort
typedef struct {
   PWSTR          pszName;          // name of the proper. Important this is first
   PWSTR          pszDescription;   // description of the property
   DWORD          dwRank;           // rank
} RECOMMENDPROP, *PRECOMMENDPROP;

// globals
static BOOL gfSortByCount = FALSE;        // if checked show the most common objects...
static DWORD gdwShowOnly = 0;             // which show-only is selected... 0=all, 1+ = based on specific class


/************************************************************************************************
RoomGraphXXX functions - Theoretically these should be part of the MIFServer.exe editor
code, but integrating with generic MIFL for now to make life easier.
*/

// ROOMGRAPHINFO - Information about a rom
typedef struct {
   PWSTR          pszRoom;          // this room
   PWSTR          apszExit[12];     // exit strings
   PWSTR          apszDoor[12];     // exit door strings
   CPoint         pLocation;        // XYZ location
   CPoint         pDimensions;      // XYZ size
   DWORD          dwShape;          // 0 for rectangular, 1 for elliptical
   fp             fRotation;        // rotation in degrees
} ROOMGRAPHINFO, *PROOMGRAPHINFO;


/************************************************************************************************
RoomGraphPropertyGetString - Returns a string with the property value. This looks in the one
lib (since help only has one) and gets it from the main object. Failing that, it looks in superclasses
until it gets the property.

inputs
   PCMIFLLib         pLib - Library to look in
   PWSTR             pszClass - CLass/object to use
   PWSTR             pszProp - Property
   DWORD             dwRecurse - Recurse count. Start at 0. If get to 5 then stops.
returns
   PWSTR - Property string (don't modify, and dont assume it lasts), or NULL if can't find
*/
PWSTR RoomGraphPropertyGetString (PCMIFLLib pLib, PWSTR pszClass, PWSTR pszProp, DWORD dwRecurse = 0)
{
   // make sure don't recurse
   if (dwRecurse >= 5)
      return NULL;


   DWORD dwIndex = pLib->ObjectFind (pszClass, (DWORD)-1);
   if (dwIndex == (DWORD)-1)
      return NULL;
   PCMIFLObject pObj = pLib->ObjectGet (dwIndex);
   if (!pObj)
      return NULL;

   PCMIFLProp *ppp = (PCMIFLProp*) pObj->m_lPCMIFLPropPub.Get(0);
   DWORD i;
   PWSTR psz;
   for (i = 0; i < pObj->m_lPCMIFLPropPub.Num(); i++) {
      PCMIFLProp pp = ppp[i];
      if (_wcsicmp (pszProp, (PWSTR)pp->m_memName.p))
         continue;   // different name

      // else, same name. Make sure there's something
      psz = (PWSTR)pp->m_memInit.p;
      if (psz && psz[0])
         return psz; // found match

      // else, repeat
   } // i
   
   // if get here, didn't find a match, so check in all the superclasses
   for (i = 0; i < pObj->m_lClassSuper.Num(); i++) {
      psz = RoomGraphPropertyGetString(pLib, (PWSTR)pObj->m_lClassSuper.Get(i), pszProp, dwRecurse+1);
      if (psz)
         return psz; // found in superclass
   } // i

   // else, didnt find
   return NULL;
}


/************************************************************************************************
RoomGraphPropertyGetDouble - Returns a string with the property value. This looks in the one
lib (since help only has one) and gets it from the main object. Failing that, it looks in superclasses
until it gets the property.

inputs
   PCMIFLLib         pLib - Library to look in
   PWSTR             pszClass - CLass/object to use
   PWSTR             pszProp - Property
   fp                fDefault - Default value
returns
   fp - Value.
*/
fp RoomGraphPropertyGetDouble (PCMIFLLib pLib, PWSTR pszClass, PWSTR pszProp, fp fDefault)
{
   PWSTR psz = RoomGraphPropertyGetString (pLib, pszClass, pszProp);
   if (!psz)
      return fDefault;

   // make sure
   double fVal;
   if (!AttribToDouble (psz, &fVal))
      return fDefault;

   // else, found
   return (fp)fVal;
}


/************************************************************************************************
RoomGraphPropertyGetPoint - Fills in a point with the property value. This looks in the one
lib (since help only has one) and gets it from the main object. Failing that, it looks in superclasses
until it gets the property.

inputs
   PCMIFLLib         pLib - Library to look in
   PWSTR             pszClass - CLass/object to use
   PWSTR             pszProp - Property. The property is assumed to be of the form [Number1, Number2, ... NumberN].
   PCPoint           pp - This should initially be filled with defaults, and will be modified in place
returns
   BOOL - TRUE if success. FALSE if couldnt find, or wasn't a list
*/
BOOL RoomGraphPropertyGetPoint (PCMIFLLib pLib, PWSTR pszClass, PWSTR pszProp, PCPoint pp)
{
   PWSTR psz = RoomGraphPropertyGetString (pLib, pszClass, pszProp);
   if (!psz)
      return FALSE;

   // skip space
   for (; iswspace(psz[0]); psz++);

   // should be [
   if (psz[0] != L'[')
      return FALSE;
   psz++;

   // up to 4 params
   WCHAR szTemp[64];
   DWORD i, j;
   for (i = 0; i < 4; i++) {
      // skip space
      for (; iswspace(psz[0]); psz++);

      // if end bracket then done
      if (psz[0] == L']')
         break;

      // if NULL then done, although not happy about it
      if (!psz[0])
         return FALSE;

      // find until comma or end bracked
      for (j = 0; j < (sizeof(szTemp)/sizeof(WCHAR)-1); j++) {
         if (!psz[j] || (psz[j] == L']') || (psz[j] == L','))
            break;
         szTemp[j] = psz[j];  // copy over
      } // j
      szTemp[j] = 0; // NULL terminate

      // get the value
      double fVal;
      if (!AttribToDouble (szTemp, &fVal))
         return FALSE;
      pp->p[i] = (fp)fVal;

      // increment
      if (psz[j] == L',')
         j++;  // to skip
      psz += j;
   } // i

   // done, and successful
   return TRUE;
}



/************************************************************************************************
RoomGraphFindObjectsRoom - Given an object, this finds what room its in.

inputs
   PCMIFLLib      pLib - Library that help compiled into
   PWSTR          pszObject - Object to start with
returns
   PWSTR - Room. Or NULL if not in room
*/
PWSTR RoomGraphFindObjectsRoom (PCMIFLLib pLib, PWSTR pszObject)
{
   DWORD i;
   PWSTR psz;
   for (i = 0; i < 10; i++) {
      // make sure it's an object
      PCMIFLObject pObj = pLib->ObjectGet (pLib->ObjectFind(pszObject, (DWORD)-1));
      if (!pObj || !pObj->m_fAutoCreate)
         return NULL;   // this doens't seem to be an object

      // see if this is a room
      psz = RoomGraphPropertyGetString (pLib, pszObject, L"pIsRoom");
      if (psz)
         return pszObject; // found something that claims to be a room

      // else, go to parent object
      pszObject = (PWSTR)pObj->m_memContained.p;
      if (!pszObject || !pszObject[0])
         return NULL;   // no parent object

      // else, fall through the loop and use new pszObject
   } // i

   // if gets here, recursed to far, so dont allow
   return NULL;
}


/************************************************************************************************
RoomGraphFindRoomsMap - Given a room, find the map it belongs to

inputs
   PCMIFLLib      pLib - Library that help compiled into
   PWSTR          pszRoom - Room.
   PCPoint        ppLoc - Filled with the map's location. Can be NULL
returns
   PWSTR - Map string. Or NULL if couldnt find
*/
PWSTR RoomGraphFindRoomsMap (PCMIFLLib pLib, PWSTR pszRoom, PCPoint ppLoc)
{
   PWSTR psz = RoomGraphPropertyGetString (pLib, pszRoom, L"pAutoMapMap");

   if (ppLoc) {
      if (psz)
         RoomGraphPropertyGetPoint (pLib, psz, L"pLocation", ppLoc);
      else
         ppLoc->Zero();
   }

   return psz;
}

/************************************************************************************************
RoomGraphFindMapsRegion - Given a room, find the map it belongs to

inputs
   PCMIFLLib      pLib - Library that help compiled into
   PWSTR          pszMap - Room.
   PCPoint        ppLoc - Filled with the map's location. Can be NULL
returns
   PWSTR - Map string. Or NULL if couldnt find
*/
PWSTR RoomGraphFindMapsRegion (PCMIFLLib pLib, PWSTR pszMap, PCPoint ppLoc)
{
   PWSTR psz = RoomGraphPropertyGetString (pLib, pszMap, L"pAutoMapRegion");

   if (ppLoc) {
      if (psz)
         RoomGraphPropertyGetPoint (pLib, psz, L"pLocation", ppLoc);
      else
         ppLoc->Zero();
   }

   return psz;
}

/************************************************************************************************
RoomGraphFindRegionsZone - Given a region, find the zone it belongs to

inputs
   PCMIFLLib      pLib - Library that help compiled into
   PWSTR          pszRegion - Room.
   PCPoint        ppLoc - Filled with the map's location. Can be NULL
returns
   PWSTR - Map string. Or NULL if couldnt find
*/
PWSTR RoomGraphFindRegionsZone (PCMIFLLib pLib, PWSTR pszRegion, PCPoint ppLoc)
{
   PWSTR psz = RoomGraphPropertyGetString (pLib, pszRegion, L"pAutoMapZone");

   if (ppLoc) {
      if (psz)
         RoomGraphPropertyGetPoint (pLib, psz, L"pLocation", ppLoc);
      else
         ppLoc->Zero();
   }

   return psz;
}


/************************************************************************************************
RoomGraphMatrixRoomParse - Assuming this is a matrix room, parses the X and Y values.

inputs
   PWSTR             pszRoom - ROom name
   DWORD             *pdwX - Filled with the X valur
   DWORD             *pdwY - Filled with the Y value
returns
   BOOL - TRUE if success. FALSE if this isn't a matrix room
*/
BOOL RoomGraphMatrixRoomParse (PWSTR pszRoom, DWORD *pdwX, DWORD *pdwY)
{
   // see if this is an matrix of rooms
   DWORD dwLen = (DWORD)wcslen(pszRoom);
   if (dwLen < 6)
      return FALSE;  // too short

   if (!iswdigit(pszRoom[dwLen-5]) || !iswdigit(pszRoom[dwLen-4]) ||
      !((pszRoom[dwLen-3] == L'x') || (pszRoom[dwLen-3] == L'X')) ||
      !iswdigit(pszRoom[dwLen-2]) || !iswdigit(pszRoom[dwLen-1]))
      return FALSE;  // not obviously a matrix
   
   // else, matrix
   *pdwX = (DWORD)(pszRoom[dwLen-5] - L'0')*10 + (DWORD)(pszRoom[dwLen-4] - L'0');
   *pdwY = (DWORD)(pszRoom[dwLen-2] - L'0')*10 + (DWORD)(pszRoom[dwLen-1] - L'0');

   return TRUE;
}

/************************************************************************************************
RoomGraphMatrixRoomNameGenerate - Generates a matrix name

inputs
   PWSTR             pszRoom - Room name to base this off of, must end with XXxYY, where
                        XX and YY are two digit numbers
   int               iXDelta - Change in XX delta over existing
   int               iYDelta - CHange in YY delta over existing
   PWSTR             pszNew - Filled with new room.
   DWORD             dwNewSize - sizeof(*pszNew)
returns
   BOOL - TRUE if success. FALSE if this isn't a matrix room
*/
BOOL RoomGraphMatrixRoomNameGenerate (PWSTR pszRoom, int iXDelta, int iYDelta, PWSTR pszNew, DWORD dwNewSize)
{
   DWORD dwLen = (DWORD)wcslen(pszRoom);
   if ((dwLen+1)*sizeof(WCHAR) > dwNewSize)
      return FALSE;  // too long

   // get the values
   DWORD dwX, dwY;
   if (!RoomGraphMatrixRoomParse(pszRoom, &dwX, &dwY))
      return FALSE;  // not a matrix room

   iXDelta += (int)dwX;
   iYDelta += (int)dwY;
   if ((iXDelta < 0) || (iXDelta > 99) || (iYDelta < 0) || (iYDelta > 99))
      return FALSE;  // beyond edge of map

   // generate name
   wcscpy (pszNew, pszRoom);
   pszNew[dwLen-5] = (WCHAR)(iXDelta / 10) + L'0';
   pszNew[dwLen-4] = (WCHAR)(iXDelta % 10) + L'0';
   pszNew[dwLen-2] = (WCHAR)(iYDelta / 10) + L'0';
   pszNew[dwLen-1] = (WCHAR)(iYDelta % 10) + L'0';

   return TRUE;
}

/************************************************************************************************
RoomGraphLocation - Fills in the point with the room's location.

inputs
   PCMIFLLib         pLib - library
   PWSTR             pszRoom - Room object
   PCPoint           pp - Filled in with the room's location.
returns
   BOOL - TRUE if success. FALSE if couldn't find location, so using 0,0,0.
*/
BOOL RoomGraphLocation (PCMIFLLib pLib, PWSTR pszRoom, PCPoint pp)
{
   pp->Zero();

   // get the map, region, and zone
   CPoint pMapOffset, pRegionOffset, pZoneOffset;
   pMapOffset.Zero();
   pRegionOffset.Zero();
   pZoneOffset.Zero();
   PWSTR pszMap = RoomGraphFindRoomsMap (pLib, pszRoom, &pMapOffset);
   PWSTR pszRegion = pszMap ? RoomGraphFindMapsRegion (pLib, pszMap, &pRegionOffset) : NULL;
   PWSTR pszZone = pszRegion ? RoomGraphFindRegionsZone (pLib, pszRegion, &pZoneOffset) : NULL;

   // get this point
   CPoint pLoc;
   pLoc.Zero();
   if (!RoomGraphPropertyGetPoint (pLib, pszRoom, L"pLocation", &pLoc)) {
      // if there's no map then can't tell
      if (!pszMap)
         return FALSE;

      // see if this is an matrix of rooms
      DWORD dwX, dwY;
      if (!RoomGraphMatrixRoomParse(pszRoom, &dwX, &dwY))
         return FALSE;

      // get the map's size
      fp fSize = RoomGraphPropertyGetDouble (pLib, pszMap, L"pMapRoomSeparation", 10);

      pLoc.p[0] = fSize * ((fp)dwX - 50);
      pLoc.p[1] = -fSize * ((fp)dwY - 50);
   }

   // add all the offsets and rern
   pLoc.Add (&pMapOffset);
   pLoc.Add (&pRegionOffset);
   pLoc.Add (&pZoneOffset);

   pp->Copy (&pLoc);

   return TRUE;
}



/************************************************************************************************
RoomGraphExit - Returns the exit from a room.

inputs
   PCMIFLLib         pLib - Library
   PWSTR             pszRoom - Room
   DWORD             dwDirection = 0 = N, 1 = NE, etc.
   PWSTR             *ppszDoor - Filled in with the door. This can be NULL.
returns
   PWSTR - Exit to room, or NULL
*/
PWSTR RoomGraphExit (PCMIFLLib pLib, PWSTR pszRoom, DWORD dwDirection, PWSTR *ppszDoor)
{
   if (dwDirection >= 12)
      return NULL;   // shouldnt happen
   if (ppszDoor)
      *ppszDoor = NULL; // to clear

   // swtich exit
   PWSTR apszExit[12] = {
      L"North", L"NorthEast", L"East", L"SouthEast",
      L"South", L"SouthWest", L"West", L"NorthWest",
      L"Up", L"Down", L"In", L"Out"};
   WCHAR szExit[64], szDoor[64];
   swprintf (szExit, L"pExit%s", apszExit[dwDirection]);

   // see if we're a grid room
   DWORD dwX, dwY;
   BOOL fGrid = RoomGraphMatrixRoomParse (pszRoom, &dwX, &dwY);

   // get the exit string
   PWSTR pszTo = RoomGraphPropertyGetString (pLib, pszRoom, szExit);
   
   if (fGrid && (!pszTo || (pLib->ObjectFind(pszTo, (DWORD)-1) == (DWORD)-1))) {
      // means that no exit defined, or it's defined and is not another room
      if (pszTo)
         return NULL;   // if it's defined but not another room then must be NULL, so cant go there
      if (dwDirection >= 8)
         return NULL;   // no autoexits

      // make up a new name
      WCHAR szTemp[128];
      int iXDelta = 0, iYDelta = 0;
      switch (dwDirection) {
         case 0:  // north
         case 1:  // NE
         case 7:  // NW
            iYDelta = -1;
            break;
         case 3:  // SE
         case 4:  // S
         case 5:  // SW
            iYDelta = 1;
            break;
      } // dwDirection
      switch (dwDirection) {
         case 1:  // NE
         case 2:  // E
         case 3:  // SE
            iXDelta = 1;
            break;
         case 5:  // SW
         case 6:  // W
         case 7:  // NW
            iXDelta = -1;
            break;
      } // dwDirection
      if (!RoomGraphMatrixRoomNameGenerate (pszRoom, iXDelta, iYDelta, szTemp, sizeof(szTemp)))
         return NULL;   // cant go there

      // try to find this object
      PCMIFLObject pObj = pLib->ObjectGet(pLib->ObjectFind (szTemp, (DWORD)-1));
      if (!pObj)
         return NULL;   // no object

      // else, use this
      pszTo = (PWSTR)pObj->m_memName.p;
   }

   if (ppszDoor) {
      swprintf (szDoor, L"pExit%sDoor", apszExit[dwDirection]);

      PWSTR psz = RoomGraphPropertyGetString (pLib, pszRoom, szDoor);

      if (psz && psz[0] && (pLib->ObjectFind(psz, (DWORD)-1) != (DWORD)-1))
         *ppszDoor = psz;  // found a door
   }

   // done
   return pszTo;
}


/************************************************************************************************
RoomGraphInfo - Fills in information about the room.

inputs
   PCMIFLLib         pLib - Library
   PWSTR             pszRoom - Room
   PROOMGRAPHINFO    pInfo - Fileld in
returns
   BOOL - TRUE if succes. FALSE if error
*/
BOOL RoomGraphInfo (PCMIFLLib pLib, PWSTR pszRoom, PROOMGRAPHINFO pInfo)
{
   memset (pInfo, 0 ,sizeof(*pInfo));
   pInfo->pszRoom = pszRoom;  // so have record of this

   // get the location
   if (!RoomGraphLocation(pLib, pszRoom, &pInfo->pLocation))
      return FALSE;

   // get all the exits
   DWORD i;
   for (i = 0; i < 12; i++)
      pInfo->apszExit[i] = RoomGraphExit (pLib, pszRoom, i, &pInfo->apszDoor[i]);

   // get the size
   if (!RoomGraphPropertyGetPoint (pLib, pszRoom, L"pDimensions", &pInfo->pDimensions)) {
      // cant get the dimensions so make it up
      pInfo->pDimensions.p[0] = pInfo->pDimensions.p[1] = 1000000;
      pInfo->pDimensions.p[2] = 3;

      CPoint pLocExit;
      for (i = 0; i < 8; i++) {
         if (!pInfo->apszExit[i])
            continue;   // not a room
         if (!RoomGraphLocation (pLib, pInfo->apszExit[i], &pLocExit))
            continue;   // cant get the location

         // else, get the distance
         pLocExit.Subtract (&pInfo->pLocation);
         fp fDist = pLocExit.Length() * .8; // make slightly smaller
         if (i % 2)
            fDist /= sqrt((fp)2); // since is on an angle
         if ((i != 0) && (i != 4))   // if not north and south
            pInfo->pDimensions.p[0] = min(pInfo->pDimensions.p[0], fDist);
         if ((i != 2) && (i != 6))   // if not east and west
            pInfo->pDimensions.p[1] = min(pInfo->pDimensions.p[1], fDist);
      } // i

      if (pInfo->pDimensions.p[0] >= 1000000)
         pInfo->pDimensions.p[0] = 5;   // undefined
      if (pInfo->pDimensions.p[1] >= 1000000)
         pInfo->pDimensions.p[1] = 5;   // undefined
   }

   pInfo->dwShape = (DWORD)RoomGraphPropertyGetDouble (pLib, pszRoom, L"pAutoMapShape", 0);
   pInfo->fRotation = (fp)RoomGraphPropertyGetDouble (pLib, pszRoom, L"pAutoMapRotation", 0);

   return TRUE;
}



/************************************************************************************************
RoomGraphHelpString - This potentially inserts room graph information onto the object's
help string.

inputs
   PCMIFLLib         pLib - Library
   PWSTR             pszObject - Object
   PCMem             pMem - To MemCat() to
returns
   none
*/
void RoomGraphHelpString (PCMIFLLib pLib, PWSTR pszObject, PCMem pMem)
{
   // get the room
   PWSTR pszRoom = RoomGraphFindObjectsRoom (pLib, pszObject);
   if (!pszRoom)
      return;

   // get the room's info
   ROOMGRAPHINFO infoRoom;
   if (!RoomGraphInfo (pLib, pszRoom, &infoRoom))
      return;

   // do three rooms around
   CListFixed lRooms;
   lRooms.Init (sizeof(ROOMGRAPHINFO), &infoRoom, 1); // so start with first one
   DWORD dwPass, i, j, dwExit;
   PROOMGRAPHINFO prgi, prgiCur;
   PWSTR pszExit;
   DWORD dwStart = 0;
   for (dwPass = 1 /* intentiaonl */; dwPass < 3; dwPass++) {
      DWORD dwNum = lRooms.Num();
      for (i = dwStart; i < dwNum; i++) {
         prgi = (PROOMGRAPHINFO) lRooms.Get(0);
         prgiCur = prgi + i;

         // loop through all the exits
         for (dwExit = 0; dwExit < 12; dwExit++) {
            pszExit = prgiCur->apszExit[dwExit];
            if (!pszExit)
               continue;   // empty exit

            // make sure this isnt already on the list
            for (j = 0; j < lRooms.Num(); j++)
               if (!_wcsicmp(pszExit, prgi[j].pszRoom))
                  break;
            if (j < lRooms.Num())
               continue;   // this room has alreay been gotten

            // else, get info on this
            if (!RoomGraphInfo(pLib, pszExit, &infoRoom))
               continue;   // error. will only happen if going to storyline

            // add
            lRooms.Add (&infoRoom);
            prgi = (PROOMGRAPHINFO) lRooms.Get(0);
            prgiCur = prgi + i;
         } // dwExit
      } // i, over rooms

      // start at the old end
      dwStart = dwNum;
   } // dwPass

   // figure out the min/max size
   CPoint pMin, pMax;
   CPoint pTemp, pSize;
   BOOL fSet = FALSE;
   prgi = (PROOMGRAPHINFO) lRooms.Get(0);
   for (i = 0; i < lRooms.Num(); i++, prgi++) {
      pSize.Copy (&prgi->pDimensions);
      pSize.Scale (0.5);
      pTemp.Add (&prgi->pLocation, &pSize);
      
      if (fSet) {
         pMin.Min (&pTemp);
         pMax.Max (&pTemp);
      }
      else {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         fSet = TRUE;
      }

      // and subtract
      pTemp.Subtract (&prgi->pLocation, &pSize);
      pMin.Min (&pTemp);
      pMax.Max (&pTemp);
   } // i

   // figure out and offset and total scale
   CPoint pOffset;
   fp fScale;
   fScale = 1;
   pOffset.Zero();
   prgi = (PROOMGRAPHINFO) lRooms.Get(0);
   if (fSet) {
      pOffset.Copy (&prgi->pLocation); // so room is in the center
      pMax.Subtract (&pMin);
      fScale = max(pMax.p[0], pMax.p[1]);
      if (!fScale)
         fScale = 1;
      fScale = 1.0 / fScale;
   }

   // create the table
   MemCat (pMem,
      L"<xtablecenter width=100%>"
      L"<xtrheader>Location of the object</xtrheader>"
      L"<tr><td>"
      L"<threed width=100% height=100% border=0 scrolldistance=threedscroll>");

   // apply scale
   WCHAR szTemp[256];
   swprintf (szTemp, L"<scale x=%g/>", (double)fScale * 10);
   MemCat (pMem, szTemp);

   // translate
   swprintf (szTemp, L"<translate point=%g,%g,%g/>",
      (double)-pOffset.p[0], (double)-pOffset.p[1], (double)-pOffset.p[2]);
   MemCat (pMem, szTemp);

   // create objects for all the rooms
   prgi = (PROOMGRAPHINFO) lRooms.Get(0);
   DWORD k;
   DWORD dwID = 10;
   for (i = 0; i < lRooms.Num(); i++) {
      prgiCur = prgi + i;

      MemCat (pMem,
         L"<matrixpush>"
         L"<colordefault color=#8080ff/>");

      // tranlsate to the center of the room
      swprintf (szTemp, L"<translate point=%g,%g,%g/>",
         (double)prgiCur->pLocation.p[0], (double)prgiCur->pLocation.p[1], (double)prgiCur->pLocation.p[2]);
      MemCat (pMem, szTemp);

      // deal with rotation
      if (prgiCur->fRotation) {
         swprintf (szTemp, L"<matrixpush><RotateZ val=%g/>", (double)prgiCur->fRotation);
         MemCat (pMem, szTemp);
      }

      // link if click on room
      swprintf (szTemp, L"<id val=%d href=\"index:", (int) dwID);
      MemCat (pMem, szTemp);
      MemCatSanitize (pMem, prgiCur->pszRoom);
      MemCat (pMem,
         L"\"/>");
      dwID++;

      // draw the room
      if (prgiCur->dwShape) // round
         swprintf (szTemp, L"<meshellipsoid x=%g y=%g z=%g/><shapemeshsurface/>",
            (double)prgiCur->pDimensions.p[0]/2, (double)prgiCur->pDimensions.p[1]/2, (double)prgiCur->pDimensions.p[2]/2);
      else  // square
         swprintf (szTemp, L"<shapebox x=%g y=%g z=%g/>",
            (double)prgiCur->pDimensions.p[0], (double)prgiCur->pDimensions.p[1], (double)prgiCur->pDimensions.p[2]);
      MemCat (pMem, szTemp);

      if (prgiCur->fRotation)
         MemCat (pMem, L"</matrixpush>");

      // text for room
      MemCat (pMem,
         L"<matrixpush>"
         L"<textsizepixels val=16/>"
         L"<colordefault color=#000000/>");
      swprintf (szTemp, L"<text left=%g,%g,%g right=%g,%g,%g align=center valign=center name=\"",
         (double)0, (double)0, (double)prgiCur->pDimensions.p[2],
         (double)1, (double)0, (double)prgiCur->pDimensions.p[2]
         );
      MemCat (pMem, szTemp);
      MemCatSanitize (pMem, prgiCur->pszRoom);
      MemCat (pMem,
         L"\"/>"
         L"</matrixpush>");

      // draw doors
      for (j = 0; j < 12; j++) {
         pszExit = prgiCur->apszDoor[j];
         if (!pszExit)
            continue;

         int iDeltaX = 0, iDeltaY = 0;
         switch (j) {
            case 1:  // NE
            case 2:  // E
            case 3:  // SE
               iDeltaX = 1;
               break;
            case 5:  // SW
            case 6:  // W
            case 7:  // NW
               iDeltaX = -1;
               break;
         } // j
         switch (j) {
            case 0:  // N
            case 1:  // NE
            case 7:  // NW
               iDeltaY = 1;
               break;
            case 3:  // SE
            case 4:  // S
            case 5:  // SW
               iDeltaY = -1;
               break;
         } // j

         swprintf (szTemp, L"<matrixpush><translate point=%g,%g,%g/>",
            (double)prgiCur->pDimensions.p[0]/2.0*(double)iDeltaX,
            (double)prgiCur->pDimensions.p[1]/2.0*(double)iDeltaY,
            (double)0);
         MemCat (pMem, szTemp);

         MemCat (pMem,
            L"<colordefault color=#ff0000/>");

         // link if click on door
         swprintf (szTemp, L"<id val=%d href=\"index:", (int) dwID);
         MemCat (pMem, szTemp);
         MemCatSanitize (pMem, pszExit);
         MemCat (pMem,
            L"\"/>");
         dwID++;

         fp fDoorSize = min(prgiCur->pDimensions.p[0], prgiCur->pDimensions.p[1]) / 4;
         swprintf (szTemp, L"<shapebox x=%g y=%g z=%g/>",
            (double)fDoorSize, (double)fDoorSize, (double)fDoorSize);
         MemCat (pMem, szTemp);

         // Note: not bothering with text for doors
         MemCat (pMem,
            L"</matrixpush>");
      } // j

      MemCat (pMem,
         L"</matrixpush>"
         L"<id val=1/>"
         );

      // lines to other rooms
      for (j = 0; j < 12; j++) {
         pszExit = prgiCur->apszExit[j];
         if (!pszExit)
            continue;

         // find match
         for (k = 0; k < lRooms.Num(); k++)
            if (!_wcsicmp(pszExit, prgi[k].pszRoom))
               break;
         CPoint pDest;
         if (k >= lRooms.Num()) {
            // no match, so make it up
            pDest.Copy (&prgiCur->pLocation);

            fp fSize = max(prgiCur->pDimensions.p[0], prgiCur->pDimensions.p[1]);

            if (j < 8) {
               pDest.p[0] += sin((fp)j / 8.0 * 2.0 * PI) * fSize;
               pDest.p[1] += cos((fp)j / 8.0 * 2.0 * PI) * fSize;
            }
         }
         else
            pDest.Copy (&prgi[k].pLocation);

         // line
         swprintf (szTemp, L"<shapeline c1=#ff0000 c2=#ff0000 p1=%g,%g,%g p2=%g,%g,%g/>",
            (double)prgiCur->pLocation.p[0], (double)prgiCur->pLocation.p[1], (double)prgiCur->pLocation.p[2],
            (double)pDest.p[0], (double)pDest.p[1], (double)pDest.p[2]);
         MemCat (pMem, szTemp);
      } // j

   } // i

   MemCat (pMem,
      L"</threed>"
      L"<br/>"
      L"<scrollbar width=100% orient=horz name=threedscroll/>"
      L"</td></tr>"
      L"</xtablecenter>");
   return;
}



/*************************************************************************************
CMIFLObject::Constructor and destructor
*/
CMIFLObject::CMIFLObject (void)
{
   m_lPCMIFLPropPriv.Init (sizeof(PCMIFLProp));
   m_lPCMIFLPropPub.Init (sizeof(PCMIFLProp));
   m_lPCMIFLMethPriv.Init (sizeof(PCMIFLFunc));
   m_lPCMIFLMethPub.Init (sizeof(PCMIFLFunc));
   m_hPropDefaultJustThis.Init (sizeof(CMIFLVarProp));
   m_hPropDefaultAllClass.Init (sizeof(CMIFLVarProp));

   Clear();
}

CMIFLObject::~CMIFLObject (void)
{
   Clear();
}


/*************************************************************************************
CMIFLObject::Clear - Clears out all values to their intiial value
*/
void CMIFLObject::Clear (void)
{
   m_dwTempID = MIFLTempID ();
   m_dwLibOrigFrom = 0;
   GUIDGen (&m_gID);

   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPriv.Get(0);
   for (i = 0; i < m_lPCMIFLPropPriv.Num(); i++)
      delete pp[i];
   m_lPCMIFLPropPriv.Clear();

   pp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
   for (i = 0; i < m_lPCMIFLPropPub.Num(); i++)
      delete pp[i];
   m_lPCMIFLPropPub.Clear();

   PCMIFLFunc *ppf = (PCMIFLFunc*)m_lPCMIFLMethPriv.Get(0);
   for (i = 0; i < m_lPCMIFLMethPriv.Num(); i++)
      delete ppf[i];
   m_lPCMIFLMethPriv.Clear();

   ppf = (PCMIFLFunc*)m_lPCMIFLMethPub.Get(0);
   for (i = 0; i < m_lPCMIFLMethPub.Num(); i++)
      delete ppf[i];
   m_lPCMIFLMethPub.Clear();

   m_lClassSuper.Clear();
   m_lRecMeth.Clear();
   m_lRecProp.Clear();

   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   MemZero (&m_aMemHelp[0]);
   MemZero (&m_aMemHelp[1]);
   MemZero (&m_memContained);

   ClearPropDefault();

   m_fAutoCreate = FALSE;
   m_dwInNewObjectMenu = 0;
}


/*************************************************************************************
CMIFLObject::ClearPropDefault - Frees up memory occupied by the default properties
*/
void CMIFLObject::ClearPropDefault (void)
{
   DWORD i;
   for (i = 0; i < m_hPropDefaultJustThis.Num(); i++) {
      PCMIFLVarProp pv = (PCMIFLVarProp)m_hPropDefaultJustThis.Get(i);
      pv->m_Var.SetUndefined();
   }
   m_hPropDefaultJustThis.Clear();

   for (i = 0; i < m_hPropDefaultAllClass.Num(); i++) {
      PCMIFLVarProp pv = (PCMIFLVarProp)m_hPropDefaultAllClass.Get(i);
      pv->m_Var.SetUndefined();
   }
   m_hPropDefaultAllClass.Clear();
}

/*************************************************************************************
CMIFLObject::CloneTo - Standard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
BOOL CMIFLObject::CloneTo (CMIFLObject *pTo, BOOL fKeepDocs)
{
   pTo->Clear();

   pTo->m_dwTempID = m_dwTempID;
   pTo->m_dwLibOrigFrom = m_dwLibOrigFrom;

   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   if (fKeepDocs) {
      MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
      MemCat (&pTo->m_memDescLong, (PWSTR)m_memDescLong.p);
      MemCat (&pTo->m_aMemHelp[0], (PWSTR)m_aMemHelp[0].p);
      MemCat (&pTo->m_aMemHelp[1], (PWSTR)m_aMemHelp[1].p);
   }
   MemCat (&pTo->m_memContained, (PWSTR)m_memContained.p);

   pTo->m_fAutoCreate = m_fAutoCreate;
   pTo->m_dwInNewObjectMenu = m_dwInNewObjectMenu;
   pTo->m_gID = m_gID;

   pTo->m_lPCMIFLPropPriv.Init (sizeof(PCMIFLProp), m_lPCMIFLPropPriv.Get(0), m_lPCMIFLPropPriv.Num());
   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*) pTo->m_lPCMIFLPropPriv.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLPropPriv.Num(); i++)
      pp[i] = pp[i]->Clone(fKeepDocs);

   pTo->m_lPCMIFLPropPub.Init (sizeof(PCMIFLProp), m_lPCMIFLPropPub.Get(0), m_lPCMIFLPropPub.Num());
   pp = (PCMIFLProp*) pTo->m_lPCMIFLPropPub.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLPropPub.Num(); i++)
      pp[i] = pp[i]->Clone(fKeepDocs);


   pTo->m_lPCMIFLMethPriv.Init (sizeof(PCMIFLFunc), m_lPCMIFLMethPriv.Get(0), m_lPCMIFLMethPriv.Num());
   PCMIFLFunc *ppf = (PCMIFLFunc*) pTo->m_lPCMIFLMethPriv.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLMethPriv.Num(); i++)
      ppf[i] = ppf[i]->Clone(fKeepDocs);


   pTo->m_lPCMIFLMethPub.Init (sizeof(PCMIFLFunc), m_lPCMIFLMethPub.Get(0), m_lPCMIFLMethPub.Num());
   ppf = (PCMIFLFunc*) pTo->m_lPCMIFLMethPub.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLMethPub.Num(); i++)
      ppf[i] = ppf[i]->Clone(fKeepDocs);

   pTo->m_lClassSuper.Clear();
   pTo->m_lClassSuper.Required (m_lClassSuper.Num());
   for (i = 0; i < m_lClassSuper.Num(); i++)
      pTo->m_lClassSuper.Add (m_lClassSuper.Get(i), m_lClassSuper.Size(i));

   pTo->m_lRecMeth.Clear();
   pTo->m_lRecMeth.Required (m_lRecMeth.Num());
   for (i = 0; i < m_lRecMeth.Num(); i++)
      pTo->m_lRecMeth.Add (m_lRecMeth.Get(i), m_lRecMeth.Size(i));

   pTo->m_lRecProp.Clear();
   pTo->m_lRecProp.Required (m_lRecProp.Num());
   for (i = 0; i < m_lRecProp.Num(); i++)
      pTo->m_lRecProp.Add (m_lRecProp.Get(i), m_lRecProp.Size(i));

   return TRUE;
}




/*************************************************************************************
CMIFLObject::Clone - Standard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
CMIFLObject *CMIFLObject::Clone (BOOL fKeepDocs)
{
   PCMIFLObject pNew = new CMIFLObject;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDocs)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


static PWSTR gpszMIFLObject = L"MIFLObject";
static PWSTR gpszName = L"Name";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszDescLong = L"DescLong";
static PWSTR gpszHelp0 = L"Help0";
static PWSTR gpszHelp1 = L"Help1";
static PWSTR gpszMIFLPropPriv = L"MIFLPropPriv";
static PWSTR gpszMIFLPropPub = L"MIFLPropPub";
static PWSTR gpszMIFLMethPriv = L"MIFLMethPriv";
static PWSTR gpszMIFLMethPub = L"MIFLMethPub";
static PWSTR gpszContained = L"Contained";
static PWSTR gpszAutoCreate = L"AutoCreate";
static PWSTR gpszInNewObjectMenu = L"InNewObjectMenu";
static PWSTR gpszID = L"ID";

/*************************************************************************************
CMIFLObject::MMLTo - Standard API
*/
PCMMLNode2 CMIFLObject::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLObject);

   if (m_memName.p && ((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR)m_memName.p);
   if (m_memDescShort.p && ((PWSTR)m_memDescShort.p)[0])
      MMLValueSet (pNode, gpszDescShort, (PWSTR)m_memDescShort.p);
   if (m_memDescLong.p && ((PWSTR)m_memDescLong.p)[0])
      MMLValueSet (pNode, gpszDescLong, (PWSTR)m_memDescLong.p);
   if (m_aMemHelp[0].p && ((PWSTR)m_aMemHelp[0].p)[0])
      MMLValueSet (pNode, gpszHelp0, (PWSTR)m_aMemHelp[0].p);
   if (m_aMemHelp[1].p && ((PWSTR)m_aMemHelp[1].p)[0])
      MMLValueSet (pNode, gpszHelp1, (PWSTR)m_aMemHelp[1].p);
   if (m_memContained.p && ((PWSTR)m_memContained.p)[0])
      MMLValueSet (pNode, gpszContained, (PWSTR)m_memContained.p);

   MMLValueSet (pNode, gpszAutoCreate, (int)m_fAutoCreate);
   MMLValueSet (pNode, gpszInNewObjectMenu, (int)m_dwInNewObjectMenu);
   MMLValueSet (pNode, gpszID, (PBYTE) &m_gID, sizeof(m_gID));

   WCHAR szTemp[32];
   DWORD i;
   for (i = 0; i < m_lClassSuper.Num(); i++) {
      PWSTR psz = (PWSTR)m_lClassSuper.Get(i);
      if (!psz && !psz[0])
         break;   // shouldnt happen
      swprintf (szTemp, L"Class%d", (int)i);
      MMLValueSet (pNode, szTemp, psz);
   } // i
   for (i = 0; i < m_lRecMeth.Num(); i++) {
      PWSTR psz = (PWSTR)m_lRecMeth.Get(i);
      if (!psz && !psz[0])
         break;   // shouldnt happen
      swprintf (szTemp, L"RecMeth%d", (int)i);
      MMLValueSet (pNode, szTemp, psz);
   } // i
   for (i = 0; i < m_lRecProp.Num(); i++) {
      PWSTR psz = (PWSTR)m_lRecProp.Get(i);
      if (!psz && !psz[0])
         break;   // shouldnt happen
      swprintf (szTemp, L"RecProp%d", (int)i);
      MMLValueSet (pNode, szTemp, psz);
   } // i

   PCMMLNode2 pSub;

   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPriv.Get(0);
   for (i = 0; i < m_lPCMIFLPropPriv.Num(); i++) {
      // BUGFIX - Don't bother saving if it's blank
      if (!((PWSTR)pp[i]->m_memName.p)[0] && !((PWSTR)pp[i]->m_memDescLong.p)[0] &&
          !((PWSTR)pp[i]->m_memDescShort.p)[0] && !((PWSTR)pp[i]->m_memInit.p)[0] &&
          !pp[i]->m_pCodeGet && !pp[i]->m_pCodeSet)
          continue;

      pSub = pp[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLPropPriv);
      pNode->ContentAdd (pSub);
   } // i

   pp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
   for (i = 0; i < m_lPCMIFLPropPub.Num(); i++) {
      // BUGFIX - Don't bother saving if it's blank
      if (!((PWSTR)pp[i]->m_memName.p)[0] && !((PWSTR)pp[i]->m_memInit.p)[0] &&
          !pp[i]->m_pCodeGet && !pp[i]->m_pCodeSet)
          continue;

      pSub = pp[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLPropPub);
      pNode->ContentAdd (pSub);
   } // i

   PCMIFLFunc *ppf = (PCMIFLFunc*)m_lPCMIFLMethPriv.Get(0);
   for (i = 0; i < m_lPCMIFLMethPriv.Num(); i++) {
      pSub = ppf[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLMethPriv);
      pNode->ContentAdd (pSub);
   } // i

   ppf = (PCMIFLFunc*)m_lPCMIFLMethPub.Get(0);
   for (i = 0; i < m_lPCMIFLMethPub.Num(); i++) {
      pSub = ppf[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLMethPub);
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CMIFLObject::MMLTo - Standard API
*/
BOOL CMIFLObject::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);
   psz = MMLValueGet (pNode, gpszDescLong);
   if (psz)
      MemCat (&m_memDescLong, psz);
   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);
   psz = MMLValueGet (pNode, gpszHelp0);
   if (psz)
      MemCat (&m_aMemHelp[0], psz);
   HACKRENAMEALL(&m_aMemHelp[0]);
   psz = MMLValueGet (pNode, gpszHelp1);
   if (psz)
      MemCat (&m_aMemHelp[1], psz);
   HACKRENAMEALL(&m_aMemHelp[1]);
   psz = MMLValueGet (pNode, gpszContained);
   if (psz)
      MemCat (&m_memContained, psz);

   m_fAutoCreate = (BOOL)MMLValueGetInt (pNode, gpszAutoCreate, 0);
   m_dwInNewObjectMenu = (DWORD)MMLValueGetInt (pNode, gpszInNewObjectMenu, 0);
   MMLValueGetBinary (pNode, gpszID, (PBYTE) &m_gID, sizeof(m_gID));


   WCHAR szTemp[32];
   DWORD i;
   for (i = 0; ; i++) {
      swprintf (szTemp, L"Class%d", (int)i);
      psz = MMLValueGet (pNode, szTemp);
      if (!psz)
         break;

      m_lClassSuper.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   } // i
   for (i = 0; ; i++) {
      swprintf (szTemp, L"RecMeth%d", (int)i);
      psz = MMLValueGet (pNode, szTemp);
      if (!psz)
         break;

      m_lRecMeth.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   } // i
   for (i = 0; ; i++) {
      swprintf (szTemp, L"RecProp%d", (int)i);
      psz = MMLValueGet (pNode, szTemp);
      if (!psz)
         break;

      m_lRecProp.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   } // i

   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszMIFLPropPriv)) {
         PCMIFLProp pp = new CMIFLProp;
         if (!pp)
            continue;
         pp->MMLFrom (pSub);
         m_lPCMIFLPropPriv.Add (&pp);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLPropPub)) {
         PCMIFLProp pp = new CMIFLProp;
         if (!pp)
            continue;
         pp->MMLFrom (pSub);
         m_lPCMIFLPropPub.Add (&pp);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLMethPriv)) {
         PCMIFLFunc pp = new CMIFLFunc;
         if (!pp)
            continue;
         pp->MMLFrom (pSub);
         m_lPCMIFLMethPriv.Add (&pp);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLMethPub)) {
         PCMIFLFunc pp = new CMIFLFunc;
         if (!pp)
            continue;
         pp->MMLFrom (pSub);
         m_lPCMIFLMethPub.Add (&pp);
         continue;
      }
   } // i

   return TRUE;
}


/*************************************************************************************
RecommendProp - Internal function to determine the recommended properties that
might be added to a given object.

inputs
   PCMIFLProj        pProj - Project
   PWSTR             pszClass - Class/object string
   PCHashString      phProp - Hash of properties that might be added. Data is a DWORD
                     with the rank. Just use dwRank. If an enty is found with a lower-number
                     property rank than currently found, then it's added.
   DWORD             dwRank - Property rank
returns
   none
*/
void RecommendProp (PCMIFLProj pProj, PWSTR pszClass, PCHashString phProp, DWORD dwRank = 1)
{
   // dont allow to get into deep recursion
   if (dwRank >= 5)
      return;

   DWORD i, j;
   for (i = 0; i < pProj->LibraryNum(); i++) {
      PCMIFLLib pLib = pProj->LibraryGet(i);
      PCMIFLObject pObj = pLib->ObjectGet (pLib->ObjectFind(pszClass, (DWORD)-1));
      if (!pObj)
         continue;

      // add all the properties from the object
      PCMIFLProp *ppp = (PCMIFLProp*)pObj->m_lPCMIFLPropPub.Get(0);
      for (j = 0; j < pObj->m_lPCMIFLPropPub.Num(); j++) {
         PCMIFLProp pp = ppp[j];
         PWSTR psz = (PWSTR) pp->m_memName.p;
         if (!psz[0])
            continue;

         DWORD *pdw = (DWORD*)phProp->Find (psz);
         if (pdw) {
            // already there, so maybe adjust rank and be done with it
            if (*pdw > dwRank)
               *pdw = dwRank;
            continue;   // done
         }

         // else, add it
         phProp->Add (psz, &dwRank);
      } // j

      // add all the sub-classes
      for (j = 0; j < pObj->m_lClassSuper.Num(); j++)
         RecommendProp (pProj, (PWSTR) pObj->m_lClassSuper.Get(j), phProp, dwRank+1);
   } // i
}


static int _cdecl PWSTRIndexCompare (const void *elem1, const void *elem2)
{
   PWSTR pdw1, pdw2;
   pdw1 = *((PWSTR*) elem1);
   pdw2 = *((PWSTR*) elem2);

   return _wcsicmp(pdw1, pdw2);
}

/*************************************************************************************
FillRecommendPropList - Internal function to fill the class' list box

inputs
   PCEscPage         pPage - Page
   PCMIFlProj        pProj - Project
   PCMIFLObject      pObj - object
*/
static void FillRecommendPropList (PCEscPage pPage, PCMIFLProj pProj, PCMIFLObject pObj)
{
   PCEscControl pControl = pPage->ControlFind (L"recommendproplist");
   if (!pControl)
      return;

   // start the hash with level-0 entries for all existing properties
   CHashString hProp;
   hProp.Init (sizeof(DWORD));
   DWORD i, j, dwRank;
   PCMIFLProp *ppp = (PCMIFLProp*)pObj->m_lPCMIFLPropPub.Get(0);
   dwRank = 0;
   for (i = 0; i < pObj->m_lPCMIFLPropPub.Num(); i++) {
      PCMIFLProp pp = ppp[i];
      PWSTR psz = (PWSTR) pp->m_memName.p;
      if (!psz[0])
         continue;
      hProp.Add (psz, &dwRank, sizeof(dwRank));
   } // i
   RecommendProp (pProj, (PWSTR)pObj->m_memName.p, &hProp);

   // create a list of all the properties to show, and info about showing
   CListFixed lProp;
   lProp.Init (sizeof(RECOMMENDPROP));
   RECOMMENDPROP rp;
   memset (&rp, 0, sizeof(rp));
   for (i = 0; i < hProp.Num(); i++) {
      DWORD *pdw = (DWORD*)hProp.Get(i);
      if (!*pdw)
         continue;   // dont add any with 0 rank since they're already on the list
      PWSTR psz = hProp.GetString (i);
      if (!psz)
         continue;   // shouldnt happen

      // find the definition
      rp.dwRank = *pdw;
      rp.pszName = psz;
      rp.pszDescription = NULL;
      for (j = 0; j < pProj->LibraryNum(); j++) {
         PCMIFLLib pLib = pProj->LibraryGet(j);
         PCMIFLProp pProp = pLib->PropDefGet (pLib->PropDefFind(psz, (DWORD)-1));
         if (!pProp)
            continue;   // not found

         // else found
         rp.pszName = (PWSTR)pProp->m_memName.p; // so have right caps
         if (!rp.pszDescription && ((PWSTR)pProp->m_memDescShort.p)[0])
            rp.pszDescription = (PWSTR)pProp->m_memDescShort.p;
      } // i

      // add this
      lProp.Add (&rp);
   } // i

   // sort
   qsort (lProp.Get(0), lProp.Num(), sizeof(RECOMMENDPROP), PWSTRIndexCompare);

   // clear the existing list
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   MemZero (&gMemTemp);

   PRECOMMENDPROP prp = (PRECOMMENDPROP) lProp.Get(0);
   PWSTR psz;
   for (i = 0; i < lProp.Num(); i++, prp++) {
      MemCat (&gMemTemp, L"<elem name=\"");
      MemCatSanitize (&gMemTemp, prp->pszName);
      MemCat (&gMemTemp, L"\">");

      switch (prp->dwRank) {
      case 0:
      case 1:
         psz = L"000000";
         break;
      case 2:
         psz = L"303030";
         break;
      case 3:
         psz = L"606060";
         break;
      case 4:
         psz = L"909090";
         break;
      case 5:
      default:
         psz = L"b0b0b0";
         break;
      }
      MemCat (&gMemTemp, L"<font color=#");
      MemCat (&gMemTemp, psz);
      MemCat (&gMemTemp, L"><bold>");
      MemCatSanitize (&gMemTemp, prp->pszName);
      MemCat (&gMemTemp, L"</bold>");
      if (prp->pszDescription) {
         MemCat (&gMemTemp, L" - ");
         MemCatSanitize (&gMemTemp, prp->pszDescription);
      }
      MemCat (&gMemTemp, L"</font>");
      MemCat (&gMemTemp, L"</elem>");
   }

   ESCMLISTBOXADD lba;
   memset (&lba, 0,sizeof(lba));
   lba.pszMML = (PWSTR)gMemTemp.p;

   pControl->Message (ESCM_LISTBOXADD, &lba);

   pControl->AttribSetInt (CurSel(), -1);
}


/*************************************************************************************
RecommendMethod - Internal function to determine the recommended methods that
might be added to a given object.

inputs
   PCMIFLProj        pProj - Project
   PWSTR             pszClass - Class/object string
   PCHashString      phProp - Hash of methods that might be added. Data is a DWORD
                     with the rank. Just use dwRank. If an enty is found with a lower-number
                     property rank than currently found, then it's added.
   DWORD             dwRank - Method rank
returns
   none
*/
void RecommendMethod (PCMIFLProj pProj, PWSTR pszClass, PCHashString phProp, DWORD dwRank = 1)
{
   // dont allow to get into deep recursion
   if (dwRank >= 5)
      return;

   DWORD i, j;
   for (i = 0; i < pProj->LibraryNum(); i++) {
      PCMIFLLib pLib = pProj->LibraryGet(i);
      PCMIFLObject pObj = pLib->ObjectGet (pLib->ObjectFind(pszClass, (DWORD)-1));
      if (!pObj)
         continue;

      // add all the properties from the object
      for (j = 0; j < pObj->MethPubNum(); j++) {
         PCMIFLFunc pMeth = pObj->MethPubGet(j);
         PWSTR psz = (PWSTR) pMeth->m_Meth.m_memName.p;
         if (!psz[0])
            continue;

         DWORD *pdw = (DWORD*)phProp->Find (psz);
         if (pdw) {
            // already there, so maybe adjust rank and be done with it
            if (*pdw > dwRank)
               *pdw = dwRank;
            continue;   // done
         }

         // else, add it
         phProp->Add (psz, &dwRank);
      } // j

      // add all the sub-classes
      for (j = 0; j < pObj->m_lClassSuper.Num(); j++)
         RecommendMethod (pProj, (PWSTR) pObj->m_lClassSuper.Get(j), phProp, dwRank+1);
   } // i
}



/*************************************************************************************
FillRecommendMethodList - Internal function to fill the class' list box

inputs
   PCEscPage         pPage - Page
   PCMIFlProj        pProj - Project
   PCMIFLObject      pObj - object
*/
static void FillRecommendMethodList (PCEscPage pPage, PCMIFLProj pProj, PCMIFLObject pObj)
{
   PCEscControl pControl = pPage->ControlFind (L"recommendmethodlist");
   if (!pControl)
      return;

   // start the hash with level-0 entries for all existing properties
   CHashString hProp;
   hProp.Init (sizeof(DWORD));
   DWORD i, j, dwRank;
   dwRank = 0;
   for (i = 0; i < pObj->MethPubNum(); i++) {
      PCMIFLFunc pFunc = pObj->MethPubGet(i);
      PWSTR psz = (PWSTR) pFunc->m_Meth.m_memName.p;
      if (!psz[0])
         continue;
      hProp.Add (psz, &dwRank, sizeof(dwRank));
   } // i
   RecommendMethod (pProj, (PWSTR)pObj->m_memName.p, &hProp);

   // create a list of all the properties to show, and info about showing
   CListFixed lProp;
   lProp.Init (sizeof(RECOMMENDPROP));
   RECOMMENDPROP rp;
   memset (&rp, 0, sizeof(rp));
   for (i = 0; i < hProp.Num(); i++) {
      DWORD *pdw = (DWORD*)hProp.Get(i);
      if (!*pdw)
         continue;   // dont add any with 0 rank since they're already on the list
      PWSTR psz = hProp.GetString (i);
      if (!psz)
         continue;   // shouldnt happen

      // find the definition
      rp.dwRank = *pdw;
      rp.pszName = psz;
      rp.pszDescription = NULL;
      for (j = 0; j < pProj->LibraryNum(); j++) {
         PCMIFLLib pLib = pProj->LibraryGet(j);
         PCMIFLMeth pMeth = pLib->MethDefGet (pLib->MethDefFind (psz, (DWORD)-1));
         if (!pMeth)
            continue;   // not found

         // else found
         rp.pszName = (PWSTR)pMeth->m_memName.p; // so have right caps
         if (!rp.pszDescription && ((PWSTR)pMeth->m_memDescShort.p)[0])
            rp.pszDescription = (PWSTR)pMeth->m_memDescShort.p;
      } // i

      // add this
      lProp.Add (&rp);
   } // i

   // sort
   qsort (lProp.Get(0), lProp.Num(), sizeof(RECOMMENDPROP), PWSTRIndexCompare);

   // clear the existing list
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   MemZero (&gMemTemp);

   PRECOMMENDPROP prp = (PRECOMMENDPROP) lProp.Get(0);
   PWSTR psz;
   for (i = 0; i < lProp.Num(); i++, prp++) {
      MemCat (&gMemTemp, L"<elem name=\"");
      MemCatSanitize (&gMemTemp, prp->pszName);
      MemCat (&gMemTemp, L"\">");

      switch (prp->dwRank) {
      case 0:
      case 1:
         psz = L"000000";
         break;
      case 2:
         psz = L"303030";
         break;
      case 3:
         psz = L"606060";
         break;
      case 4:
         psz = L"909090";
         break;
      case 5:
      default:
         psz = L"b0b0b0";
         break;
      }
      MemCat (&gMemTemp, L"<font color=#");
      MemCat (&gMemTemp, psz);
      MemCat (&gMemTemp, L"><bold>");
      MemCatSanitize (&gMemTemp, prp->pszName);
      MemCat (&gMemTemp, L"</bold>");
      if (prp->pszDescription) {
         MemCat (&gMemTemp, L" - ");
         MemCatSanitize (&gMemTemp, prp->pszDescription);
      }
      MemCat (&gMemTemp, L"</font>");
      MemCat (&gMemTemp, L"</elem>");
   }

   ESCMLISTBOXADD lba;
   memset (&lba, 0,sizeof(lba));
   lba.pszMML = (PWSTR)gMemTemp.p;

   pControl->Message (ESCM_LISTBOXADD, &lba);

   pControl->AttribSetInt (CurSel(), -1);
}


/*************************************************************************************
FillClassList - Internal function to fill the class' list box

inputs
   PCEscPage         pPage - Page
   PCMIFLObject      pObj - object
   DWORD             dwSel - One to select
*/
static void FillClassList (PCEscPage pPage, PCMIFLObject pObj, DWORD dwSel)
{
   PCEscControl pControl = pPage->ControlFind (L"classlist");
   if (!pControl)
      return;

   // clear the existing list
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   MemZero (&gMemTemp);

   DWORD i;
   for (i = 0; i < pObj->m_lClassSuper.Num(); i++) {
      PWSTR psz = (PWSTR)pObj->m_lClassSuper.Get(i);
      
      MemCat (&gMemTemp, L"<elem name=");
      MemCat (&gMemTemp, (int)i);
      MemCat (&gMemTemp, L"><bold>");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"</bold>");
      MemCat (&gMemTemp, L"</elem>");
   }

   ESCMLISTBOXADD lba;
   memset (&lba, 0,sizeof(lba));
   lba.pszMML = (PWSTR)gMemTemp.p;

   pControl->Message (ESCM_LISTBOXADD, &lba);

   pControl->AttribSetInt (CurSel(), dwSel);
}


/*************************************************************************************
CMIFLObject::PageLink - Call this when an link notification comes in.
It will see if it affects any of the object parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCMLINK         p - Buttonpress information
   PCMIFLLib         pLib - Library to use
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLObject::PageLink (PCEscPage pPage, PMIFLPAGE pmp, PESCMLINK p, PCMIFLLib pLib)
{
   if (!p->psz)
      return FALSE;
   PWSTR psz = p->psz;

   PWSTR pszMethodPriv = L"methodpriv", pszMethodPub = L"methodpub";
   DWORD dwMethodPrivLen = (DWORD)wcslen(pszMethodPriv), dwMethodPubLen = (DWORD)wcslen(pszMethodPub);
   
   if (!wcsncmp(psz, pszMethodPriv, dwMethodPrivLen)) {
      DWORD dwNum = _wtoi(psz + dwMethodPrivLen);
      if (dwNum >= MethPrivNum())
         return TRUE;   // shouldnt happen
      PCMIFLFunc pFunc = MethPrivGet (dwNum);
      if (!pFunc)
         return TRUE;

      // delte?
      if ((GetKeyState (VK_CONTROL) < 0) && !pLib->m_fReadOnly) {
         // requested a delete
         if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the method?"))
            return TRUE;

         // delete
         MethPrivRemove (pFunc->m_Meth.m_dwTempID, pLib);
         pPage->Exit (MIFLRedoSamePage());
         return TRUE;
      }

      // else, edit
      WCHAR szTemp[64];
      swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit", (int) pLib->m_dwTempID, (int)m_dwTempID, (int) pFunc->m_Meth.m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }

   else if (!wcsncmp(psz, pszMethodPub, dwMethodPubLen)) {
      DWORD dwNum = _wtoi(psz + dwMethodPubLen);
      if (dwNum >= MethPubNum())
         return TRUE;   // shouldnt happen
      PCMIFLFunc pFunc = MethPubGet (dwNum);
      if (!pFunc)
         return TRUE;

      // delte?
      if ((GetKeyState (VK_CONTROL) < 0) && !pLib->m_fReadOnly) {
         // requested a delete
         if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the method?"))
            return TRUE;

         // delete
         MethPubRemove (pFunc->m_Meth.m_dwTempID, pLib);
         pPage->Exit (MIFLRedoSamePage());
         return TRUE;
      }

      // else, edit
      WCHAR szTemp[64];
      swprintf (szTemp, L"lib:%dobject:%dmethpub:%dedit", (int) pLib->m_dwTempID, (int)m_dwTempID, (int) pFunc->m_Meth.m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CMIFLObject::PageButtonPress - Call this when an button press notification comes in.
It will see if it affects any of the object parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNBUTTONPRESS  p - Buttonpress information
   PCMIFLLib         pLib - Library to use
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLObject::PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;
   PCMIFLProj pProj = pmp->pProj;

   PWSTR pszPropPrivGetSet = L"propprivgetset", pszPropPrivGet = L"propprivget", pszPropPrivSet = L"propprivset";
   PWSTR pszPropPubGetSet = L"proppubgetset", pszPropPubGet = L"proppubget", pszPropPubSet = L"proppubset";
   DWORD dwPropPrivGetSetLen = (DWORD)wcslen(pszPropPrivGetSet), dwPropPrivGetLen = (DWORD)wcslen(pszPropPrivGet), dwPropPrivSetLen = (DWORD)wcslen(pszPropPrivSet);
   DWORD dwPropPubGetSetLen = (DWORD)wcslen(pszPropPubGetSet), dwPropPubGetLen = (DWORD)wcslen(pszPropPubGet), dwPropPubSetLen = (DWORD)wcslen(pszPropPubSet);

   if (!_wcsicmp(psz, L"methodprivadd")) {
      DWORD dwPropID = MethPrivNew (pLib);
      if (dwPropID == -1)
         return TRUE;

      WCHAR szTemp[64];
      swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit", (int) pLib->m_dwTempID, (int)m_dwTempID, (int) dwPropID);
      pPage->Exit (szTemp);
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"classadd")) {
      WCHAR szTemp[64];
      swprintf (szTemp, L"lib:%dobject:%dclassadd", (int) pLib->m_dwTempID, (int)m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"recommend")) {
      WCHAR szTemp[64];
      swprintf (szTemp, L"lib:%dobject:%drecedit", (int) pLib->m_dwTempID, (int)m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"autocreate")) {
      pLib->AboutToChange();
      m_fAutoCreate = p->pControl->AttribGetBOOL (Checked());
      pLib->Changed();
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"classremove")) {
      PCEscControl pControl = pPage->ControlFind (L"classlist");
      if (!pControl)
         return TRUE;

      // get the current selection
      DWORD dwSel = (DWORD)pControl->AttribGetInt (CurSel());
      if (dwSel >= m_lClassSuper.Num()) {
         pPage->MBWarning (L"You must select the super-class you wish to remove.");
         return TRUE;
      }

      // verify
      if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove this superclass?"))
         return TRUE;

      // remove it...
      pLib->AboutToChange();
      m_lClassSuper.Remove (dwSel);
      pLib->Changed();

      // remove from list
      FillClassList (pPage, this, dwSel);
      EscChime (ESCCHIME_INFORMATION);

      return TRUE;
   }

   else if (!_wcsicmp(psz, L"classmoveup") || !_wcsicmp(psz, L"classmovedown")) {
      BOOL fMoveUp = !_wcsicmp(psz, L"classmoveup");
      PCEscControl pControl = pPage->ControlFind (L"classlist");
      if (!pControl)
         return TRUE;

      // get the current selection
      DWORD dwSel = (DWORD)pControl->AttribGetInt (CurSel());
      if (dwSel >= m_lClassSuper.Num()) {
         pPage->MBWarning (L"You must select the super-class you wish to move.");
         return TRUE;
      }

      if (fMoveUp && !dwSel) {
         pPage->MBWarning (L"You can't move the selected super-class any higher.");
         return TRUE;
      }
      if (!fMoveUp && (dwSel+1 >= m_lClassSuper.Num())) {
         pPage->MBWarning (L"You can't move the selected super-class any lower.");
         return TRUE;
      }

      // make a copy of the one we're about to move
      CMem mem;
      if (!mem.Required (m_lClassSuper.Size(dwSel)))
         return TRUE;
      PWSTR pszMove = (PWSTR)mem.p;
      wcscpy (pszMove, (PWSTR)m_lClassSuper.Get(dwSel));

      // remove it...
      pLib->AboutToChange();
      m_lClassSuper.Remove (dwSel);
      if (fMoveUp)
         dwSel--;
      else
         dwSel++;

      m_lClassSuper.Insert (dwSel, pszMove, (wcslen(pszMove)+1)*sizeof(WCHAR));
      pLib->Changed();

      // remove from list
      FillClassList (pPage, this, dwSel);
      EscChime (ESCCHIME_INFORMATION);

      return TRUE;
   }

   else if (!_wcsicmp(psz, L"classhelp")) {
      PCEscControl pControl = pPage->ControlFind (L"classlist");
      if (!pControl)
         return TRUE;

      // get the current selection
      DWORD dwSel = (DWORD)pControl->AttribGetInt (CurSel());
      if (dwSel >= m_lClassSuper.Num()) {
         pPage->MBWarning (L"You must select the super-class you wish to move.");
         return TRUE;
      }

      pProj->HelpIndex (pPage, (PWSTR)m_lClassSuper.Get(dwSel));

      return TRUE;
   }

   else if (!_wcsicmp(psz, L"methodpubadd")) {
      // get the filtered list...
      PCEscControl pControl = pPage->ControlFind (L"methodpubaddlist");
      if (!pControl)
         return TRUE;
      int iVal = pControl->AttribGetInt(CurSel());
      MIFLFilteredListMethPub (pmp, pmp->pObj);   // just to make sure exists
      if ((iVal < 0) || (iVal >= (int)pmp->plMethPub->Num())) {
         pPage->MBWarning (L"You must first select a method to add.");
         return TRUE;
      }

      // get the method name
      PWSTR pszName = (PWSTR)pmp->plMethPub->Get((DWORD)iVal);
      if (!pszName)
         return TRUE;

      DWORD dwPropID = MethPubNew (pszName, pLib);
      if (dwPropID == -1)
         return TRUE;

      WCHAR szTemp[64];
      swprintf (szTemp, L"lib:%dobject:%dmethpub:%dedit", (int) pLib->m_dwTempID, (int)m_dwTempID, (int) dwPropID);
      pPage->Exit (szTemp);
      return TRUE;
   }

   else if (!_wcsicmp(psz, L"classaddquick")) {
      // get the filtered list...
      PCEscControl pControl = pPage->ControlFind (L"classaddlist");
      if (!pControl)
         return TRUE;
      int iVal = pControl->AttribGetInt(CurSel());
      MIFLFilteredListClass (pmp, FALSE);   // just to make sure exists
      if ((iVal < 0) || (iVal >= (int)pmp->plClass->Num())) {
         pPage->MBWarning (L"You must first select a class to add.");
         return TRUE;
      }

      // get the method name
      PWSTR pszName = (PWSTR)pmp->plClass->Get((DWORD)iVal);
      if (!pszName)
         return TRUE;


      // see if it's in the list
      DWORD i;
      for (i = 0; i < pmp->pObj->m_lClassSuper.Num(); i++)
         if (!_wcsicmp(pszName, (PWSTR)pmp->pObj->m_lClassSuper.Get(i)))
            break;
      if ( !_wcsicmp((PWSTR)pmp->pObj->m_memName.p, pszName) || (i < pmp->pObj->m_lClassSuper.Num()) ) {
         pPage->MBWarning (L"The class is already in the object.");
         return TRUE;
      }

      // else, insert on the top
      pLib->AboutToChange();
      pmp->pObj->ClassAddWithRecommend (pszName, TRUE, pProj);
      pLib->Changed();

      // remove from list
      FillClassList (pPage, this, 0);
      EscChime (ESCCHIME_INFORMATION);

      return TRUE;
   }



   else if (!wcsncmp(psz, pszPropPrivGetSet, dwPropPrivGetSetLen)) {
      DWORD dwNum = _wtoi(psz + dwPropPrivGetSetLen);
      PCMIFLProp *ppp = (PCMIFLProp*) m_lPCMIFLPropPriv.Get(dwNum);
      if (!ppp)
         return TRUE;
      PCMIFLProp pp = *ppp;

      BOOL fCode = (pp->m_pCodeGet || pp->m_pCodeSet);
      fCode = !fCode;

      pLib->AboutToChange ();

      // change the object
      // change the object
      if (!fCode)
         pp->CodeClear();
      else
         pp->CodeDefault ();

      pLib->Changed ();

      // enable the windows
      WCHAR szTemp[32];
      PCEscControl pControl;
      swprintf (szTemp, L"propprivget%d", (int)dwNum);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);
      swprintf (szTemp, L"propprivset%d", (int)dwNum);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);

      return TRUE;
   }
   else if (!wcsncmp(psz, pszPropPrivGet, dwPropPrivGetLen)) {
      DWORD dwNum = _wtoi(psz + dwPropPrivGetLen);
      PCMIFLProp *ppp = (PCMIFLProp*) m_lPCMIFLPropPriv.Get(dwNum);
      if (!ppp)
         return TRUE;
      PCMIFLProp pp = *ppp;

      WCHAR szTemp[64];
      // NOTE: ASsuming that modifying a global. since it should be the only way to get here
      swprintf (szTemp, L"lib:%dobject:%dproppriv:%dget", pLib->m_dwTempID, m_dwTempID, pp->m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }
   else if (!wcsncmp(psz, pszPropPrivSet, dwPropPrivSetLen)) {
      DWORD dwNum = _wtoi(psz + dwPropPrivSetLen);
      PCMIFLProp *ppp = (PCMIFLProp*) m_lPCMIFLPropPriv.Get(dwNum);
      if (!ppp)
         return TRUE;
      PCMIFLProp pp = *ppp;

      WCHAR szTemp[64];
      // NOTE: ASsuming that modifying a global. since it should be the only way to get here
      swprintf (szTemp, L"lib:%dobject:%dproppriv:%dset", pLib->m_dwTempID, m_dwTempID, pp->m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }
   else if (!wcsncmp(psz, pszPropPubGetSet, dwPropPubGetSetLen)) {
      DWORD dwNum = _wtoi(psz + dwPropPubGetSetLen);
      PCMIFLProp *ppp = (PCMIFLProp*) m_lPCMIFLPropPub.Get(dwNum);
      if (!ppp)
         return TRUE;
      PCMIFLProp pp = *ppp;

      BOOL fCode = (pp->m_pCodeGet || pp->m_pCodeSet);
      fCode = !fCode;

      pLib->AboutToChange ();

      // change the object
      // change the object
      if (!fCode)
         pp->CodeClear();
      else
         pp->CodeDefault ();

      pLib->Changed ();

      // enable the windows
      WCHAR szTemp[32];
      PCEscControl pControl;
      swprintf (szTemp, L"proppubget%d", (int)dwNum);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);
      swprintf (szTemp, L"proppubset%d", (int)dwNum);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);

      return TRUE;
   }
   else if (!wcsncmp(psz, pszPropPubGet, dwPropPubGetLen)) {
      DWORD dwNum = _wtoi(psz + dwPropPubGetLen);
      PCMIFLProp *ppp = (PCMIFLProp*) m_lPCMIFLPropPub.Get(dwNum);
      if (!ppp)
         return TRUE;
      PCMIFLProp pp = *ppp;

      WCHAR szTemp[64];
      // NOTE: ASsuming that modifying a global. since it should be the only way to get here
      swprintf (szTemp, L"lib:%dobject:%dproppub:%dget", pLib->m_dwTempID, m_dwTempID, pp->m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }
   else if (!wcsncmp(psz, pszPropPubSet, dwPropPubSetLen)) {
      DWORD dwNum = _wtoi(psz + dwPropPubSetLen);
      PCMIFLProp *ppp = (PCMIFLProp*) m_lPCMIFLPropPub.Get(dwNum);
      if (!ppp)
         return TRUE;
      PCMIFLProp pp = *ppp;

      WCHAR szTemp[64];
      // NOTE: ASsuming that modifying a global. since it should be the only way to get here
      swprintf (szTemp, L"lib:%dobject:%dproppub:%dset", pLib->m_dwTempID, m_dwTempID, pp->m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }



   return FALSE;
}


/*************************************************************************************
CMIFLObject::PageEditChange - Call this when an edit change notification comes in.
It will see if it affects any of the object parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNEDITCHANGE   p - Edit change information
   PCMIFLLib         pLib - Library to use
   BOOL              *pfChangedName - Will fill with TRUE if changed then object's
                     name, which would require a resort of the parent
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLObject::PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib, BOOL *pfChangedName)
{
   // init
   *pfChangedName = FALSE;

   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   // look for GUID
   DWORD dwNeed = 0;
   if (!_wcsicmp(psz, L"ID")) {
      WCHAR szTemp[64];
      p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);

      pLib->AboutToChange ();
      if (CLSIDFromString (szTemp, &m_gID))
         m_gID = GUID_NULL;
      pLib->Changed ();
      return TRUE;
   }

   // how big is this?
   p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);

   // library
   //PCMIFLLib pLib = ParentGetOnlyLib();

   PCMem pMem = NULL;

   if (!_wcsicmp(psz, L"name")) {
      *pfChangedName = TRUE;
      pMem = &m_memName;
   }
   else if (!_wcsicmp(psz, L"helpcat0"))
      pMem = &m_aMemHelp[0];
   else if (!_wcsicmp(psz, L"helpcat1"))
      pMem = &m_aMemHelp[1];
   else if (!_wcsicmp(psz, L"descshort"))
      pMem = &m_memDescShort;
   else if (!_wcsicmp(psz, L"desclong"))
      pMem = &m_memDescLong;

   BOOL fModPropPriv = FALSE;
   BOOL fModPropPub = FALSE;

   if (!pMem) {
      // if gets to here then might be one of the parameters...
      PWSTR pszParamName = L"propprivname", pszParamDesc = L"propprivdesc", pszParamInit = L"propprivinit";
      DWORD dwParamNameLen = (DWORD)wcslen(pszParamName), dwParamDescLen = (DWORD)wcslen(pszParamDesc), dwParamInitLen = (DWORD)wcslen(pszParamInit);

      DWORD dwNum = 0;
      DWORD dwParam = 0;
      if (!_wcsnicmp(psz, pszParamName, dwParamNameLen)) {
         dwNum = _wtoi (psz + dwParamNameLen);
         dwParam = 1;
         fModPropPriv = TRUE;
      }
      else if (!_wcsnicmp(psz, pszParamDesc, dwParamDescLen)) {
         dwNum = _wtoi (psz + dwParamDescLen);
         dwParam = 2;
         fModPropPriv = TRUE;
      }
      else if (!_wcsnicmp(psz, pszParamInit, dwParamInitLen)) {
         dwNum = _wtoi (psz + dwParamInitLen);
         dwParam = 3;
         fModPropPriv = TRUE;
      }

      // if need to add extra blank parameters then do so...
      while (dwParam && (dwNum >= m_lPCMIFLPropPriv.Num())) {
         PCMIFLProp pNew = new CMIFLProp;
         if (pNew) {
            pLib->AboutToChange ();
            m_lPCMIFLPropPriv.Add (&pNew);
            pLib->Changed ();
         }
      }

      // memory to be filled in...
      PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPriv.Get(0);
      switch (dwParam) {
      case 1: // pszParamName
         pMem = &pp[dwNum]->m_memName;
         break;
      case 2: // pszParamDesc
         pMem = &pp[dwNum]->m_memDescShort;
         break;
      case 3: // pszParamInit
         pMem = &pp[dwNum]->m_memInit;
         break;
      }
   }

   // see if it's a public setting
   if (!pMem) {
      // if gets to here then might be one of the parameters...
      // note: can only change the init value
      PWSTR pszParamInit = L"proppubinit";
      DWORD dwParamInitLen = (DWORD)wcslen(pszParamInit);

      DWORD dwNum = 0;
      DWORD dwParam = 0;
      if (!_wcsnicmp(psz, pszParamInit, dwParamInitLen)) {
         dwNum = _wtoi (psz + dwParamInitLen);
         dwParam = 3;
         fModPropPub = TRUE;
      }

      // if need to add extra blank parameters then do so...
      while (dwParam && (dwNum >= m_lPCMIFLPropPub.Num())) {
         PCMIFLProp pNew = new CMIFLProp;
         if (pNew) {
            pLib->AboutToChange ();
            m_lPCMIFLPropPub.Add (&pNew);
            pLib->Changed ();
         }
      }

      // memory to be filled in...
      PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
      switch (dwParam) {
      case 3: // pszParamInit
         pMem = &pp[dwNum]->m_memInit;
         break;
      }
   }

   if (pMem) {
      if (!pMem->Required (dwNeed))
         return FALSE;

      pLib->AboutToChange ();
      p->pControl->AttribGet (Text(), (PWSTR)pMem->p, (DWORD)pMem->m_dwAllocated, &dwNeed);

      // if modified a parameter will need to delete dead parameters at the end of the list
      if (fModPropPriv) while (m_lPCMIFLPropPriv.Num()) {
         PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPriv.Get(0);
         PCMIFLProp pm = pp[m_lPCMIFLPropPriv.Num()-1];
         if (!((PWSTR)(pm->m_memName.p))[0] && !((PWSTR)(pm->m_memDescShort.p))[0] && !((PWSTR)(pm->m_memInit.p))[0]) {
            // name was set to nothing, so delete
            delete pm;
            m_lPCMIFLPropPriv.Remove (m_lPCMIFLPropPriv.Num()-1);
         }
         else
            break;
      }

      // likewise, if modified public
      if (fModPropPub) while (m_lPCMIFLPropPub.Num()) {
         PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
         PCMIFLProp pm = pp[m_lPCMIFLPropPub.Num()-1];
         if (!((PWSTR)(pm->m_memName.p))[0] && !((PWSTR)(pm->m_memInit.p))[0]) {
            // name was set to nothing, so delete
            delete pm;
            m_lPCMIFLPropPub.Remove (m_lPCMIFLPropPub.Num()-1);
         }
         else
            break;
      }

      pLib->Changed ();
      return TRUE;
   }


   return FALSE;  // not it
}



/*************************************************************************************
CMIFLObject::PageFilteredListChange - Call this when an edit change notification comes in.
It will see if it affects any of the object parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNFILTEREDLISTCHANGE   p - Edit change information
   PCMIFLLib         pLib - Library to use
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLObject::PageFilteredListChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNFILTEREDLISTCHANGE p, PCMIFLLib pLib)
{
   PWSTR pszPropPubName = L"proppubname";
   DWORD dwPropPubNameLen = (DWORD)wcslen(pszPropPubName);
   PCMIFLProj pProj = pmp->pProj;
   PWSTR psz = p->pControl->m_pszName;
   DWORD dwNum;
   if (!psz)
      return FALSE;

   if (!_wcsicmp(psz, L"contained")) {
      // get the string from the selection
      MIFLFilteredListObject (pmp, TRUE);
      PWSTR pszGet = (PWSTR)pmp->plObject->Get((DWORD)p->iCurSel);
      if (!pszGet)
         pszGet = L"";

      // if no change ignore
      if (!_wcsicmp(pszGet, (PWSTR)m_memContained.p))
         return TRUE;

      // else, change...
      pLib->AboutToChange();
      MemZero (&m_memContained);
      MemCat (&m_memContained, pszGet);
      pLib->Changed();
      return TRUE;
   }
   else if (!_wcsnicmp(psz, pszPropPubName, dwPropPubNameLen)) {
      // get it
      dwNum = _wtoi (psz + dwPropPubNameLen);

      // add blank properties until get right number
      while (m_lPCMIFLPropPub.Num() <= dwNum) {
         PCMIFLProp pNew = new CMIFLProp;
         if (!pNew)
            return FALSE;
         m_lPCMIFLPropPub.Add (&pNew);
      }

      PCMIFLProp *ppp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(dwNum);
      if (!ppp)
         return FALSE;
      PCMIFLProp pp = *ppp;

      // get the string from the selection
      MIFLFilteredListPropPub (pmp);
      PWSTR pszGet = (PWSTR)pmp->plPropPub->Get((DWORD)p->iCurSel);
      if (!pszGet)
         pszGet = L"";

      // if no change ignore
      if (!_wcsicmp(pszGet, (PWSTR)pp->m_memName.p))
         return TRUE;

      // else, change...
      pLib->AboutToChange();
      MemZero (&pp->m_memName);
      MemCat (&pp->m_memName, pszGet);
      while (m_lPCMIFLPropPub.Num()) {
         PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
         PCMIFLProp pm = pp[m_lPCMIFLPropPub.Num()-1];
         if (!((PWSTR)(pm->m_memName.p))[0] && !((PWSTR)(pm->m_memDescShort.p))[0]) {
            // name was set to nothing, so delete
            delete pm;
            m_lPCMIFLPropPub.Remove (m_lPCMIFLPropPub.Num()-1);
         }
         else
            break;
      }
      pLib->Changed();

      // set the description
      WCHAR szTemp[32];
      PCEscControl pControl;
      // need to get a public description of the library
      swprintf (szTemp, L"proppubdesc%d", (int)dwNum);
      pControl = pPage->ControlFind (szTemp);
      DWORD j;
      PCMIFLLib pFindLib = NULL;
      PCMIFLProp pFindProp = NULL;
      if (pszGet[0]) for (j = 0; j < pProj->LibraryNum(); j++) {
         pFindLib = pProj->LibraryGet(j);
         DWORD dw = pFindLib->PropDefFind (pszGet, -1);
         if (dw == -1)
            continue;
         pFindProp = pFindLib->PropDefGet(dw);
         if (pFindProp)
            break;
      }
      if (pControl && pFindProp)
         pControl->AttribSet (Text(), (PWSTR)pFindProp->m_memDescShort.p);
      else if (pControl) {
         pControl->AttribSet (Text(), L"");  // blank out
      }

      return TRUE;
   }
   // BUGBUG - may want to put lib change here

   return FALSE;
}


/*************************************************************************************
CMIFLObject::PageInitPage - This functions handles the init-page call for a object.
This allows the code to be used for not only the object defiitions, but also functions,
and private object.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
returns
   none
*/
void CMIFLObject::PageInitPage (PCEscPage pPage, PMIFLPAGE pmp)
{
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLObject pObj = pmp->pObj;

   PCEscControl pControl;

   pControl = pPage->ControlFind (L"name");
   if (pControl && m_memName.p)
      pControl->AttribSet (Text(), (PWSTR)m_memName.p);

   pControl = pPage->ControlFind (L"helpcat0");
   if (pControl && m_aMemHelp[0].p)
      pControl->AttribSet (Text(), (PWSTR)m_aMemHelp[0].p);

   pControl = pPage->ControlFind (L"helpcat1");
   if (pControl && m_aMemHelp[1].p)
      pControl->AttribSet (Text(), (PWSTR)m_aMemHelp[1].p);

   pControl = pPage->ControlFind (L"descshort");
   if (pControl && m_memDescShort.p)
      pControl->AttribSet (Text(), (PWSTR)m_memDescShort.p);

   pControl = pPage->ControlFind (L"desclong");
   if (pControl && m_memDescLong.p)
      pControl->AttribSet (Text(), (PWSTR)m_memDescLong.p);

   // loop through all the proppriv
   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPriv.Get(0);
   WCHAR szTemp[64];
   for (i = 0; i < m_lPCMIFLPropPriv.Num(); i++) {
      swprintf (szTemp, L"propprivname%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memName.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memName.p);

      swprintf (szTemp, L"propprivdesc%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memDescShort.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memDescShort.p);

      swprintf (szTemp, L"propprivinit%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memInit.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memInit.p);

      // whether or not get/set button is checked
      BOOL fCode = (pp[i]->m_pCodeGet || pp[i]->m_pCodeSet);
      swprintf (szTemp, L"propprivgetset%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->AttribSetBOOL (Checked(), fCode);

      swprintf (szTemp, L"propprivget%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);
      swprintf (szTemp, L"propprivset%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);
   } // i

   // loop through all the public properties
   pp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
   for (i = 0; i < m_lPCMIFLPropPub.Num(); i++) {
      // public property name
      swprintf (szTemp, L"proppubname%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      MIFLFilteredListPropPub (pmp);   // to make sure have list
      DWORD dwFind = MIFLFilteredListSearch ((PWSTR)pp[i]->m_memName.p, pmp->plPropPub);
      if (pControl && (dwFind != -1))
         pControl->AttribSetInt (CurSel(), dwFind);

      // need to get a public description of the library
      swprintf (szTemp, L"proppubdesc%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      DWORD j;
      PCMIFLLib pFindLib = NULL;
      PCMIFLProp pFindProp = NULL;
      if (((PWSTR)pp[i]->m_memName.p)[0]) for (j = 0; j < pProj->LibraryNum(); j++) {
         pFindLib = pProj->LibraryGet(j);
         DWORD dw = pFindLib->PropDefFind ((PWSTR)pp[i]->m_memName.p, -1);
         if (dw == -1)
            continue;
         pFindProp = pFindLib->PropDefGet(dw);
         if (pFindProp)
            break;
      }
      if (pControl && pFindProp)
         pControl->AttribSet (Text(), (PWSTR)pFindProp->m_memDescShort.p);
      else if (pControl && !pFindProp && ((PWSTR)pp[i]->m_memName.p)[0]) {
         WCHAR szTemp[512];
         swprintf (szTemp, L"Error! Method definition for \"%s\" can't be found.",
            (PWSTR)pp[i]->m_memName.p);
         pControl->AttribSet (Text(), szTemp);
      }

      swprintf (szTemp, L"proppubinit%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memInit.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memInit.p);

      // whether or not get/set button is checked
      BOOL fCode = (pp[i]->m_pCodeGet || pp[i]->m_pCodeSet);
      swprintf (szTemp, L"proppubgetset%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->AttribSetBOOL (Checked(), fCode);

      swprintf (szTemp, L"proppubget%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);
      swprintf (szTemp, L"proppubset%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->Enable (fCode);
   } // i

   // auto create option
   pControl = pPage->ControlFind (L"autocreate");
   if (pControl)
      pControl->AttribSetBOOL (Checked(), m_fAutoCreate);

   // in new object menu
   ComboBoxSet (pPage, L"innewobjectmenu", m_dwInNewObjectMenu);

   // fill in the create in
   pControl = pPage->ControlFind (L"contained");
   if (pControl) {
      MIFLFilteredListObject (pmp, TRUE);   // to make sure have list
      DWORD dwFind = MIFLFilteredListSearch ((PWSTR)m_memContained.p, pmp->plObject);
      if (pControl && (dwFind != -1))
         pControl->AttribSetInt (CurSel(), dwFind);
   }

   // fill in the GUID
   if (IsEqualGUID (m_gID, GUID_NULL))
      szTemp[0] = 0;
   else
      StringFromGUID2 (m_gID, szTemp, sizeof(szTemp)/sizeof(WCHAR));
   pControl = pPage->ControlFind (L"ID");
   if (pControl)
      pControl->AttribSet (Text(), szTemp);

   // fill in the list box
   FillClassList (pPage, pObj, 0);
   FillRecommendPropList (pPage, pProj, pObj);
   FillRecommendMethodList (pPage, pProj, pObj);
}



/*************************************************************************************
CMIFLObject::PageSubstitution - This functions handles the ESCM_SUBSTITUTION call for a Object
This allows the code to be used for not only the object defiitions, but also functions,
and private Object.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCMSUBSTITUTION p - Substitution info
   PCMIFLLib         pLib - Library to use
returns
   BOOL - TRUE if captured and handled this, FALSE if processing should continue
*/
BOOL CMIFLObject::PageSubstitution (PCEscPage pPage, PMIFLPAGE pmp, PESCMSUBSTITUTION p, PCMIFLLib pLib)
{
   PCMIFLProj pProj = pmp->pProj;
   // PCMIFLLib pLib = pmp->pLib;
   PCMIFLObject pObj = pmp->pObj;

   if (!_wcsicmp(p->pszSubName, L"METHPRIVLIST")) {
      MemZero (&gMemTemp);

      // BUGFIX - Only do this for certain tabs
      if (*pmp->pdwTab != 3) {
         p->pszSubString = (PWSTR)gMemTemp.p;  // BUGFIX - Was &gMemTemp.p
         return TRUE;
      }

      DWORD i;
      for (i = 0; i < MethPrivNum(); i++) {
         PCMIFLFunc pFunc = MethPrivGet(i);

         MemCat (&gMemTemp, L"<tr><td width=33%><a href=methodpriv");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"><bold>");
         MemCatSanitize (&gMemTemp, ((PWSTR)(pFunc->m_Meth.m_memName.p))[0] ? (PWSTR)(pFunc->m_Meth.m_memName.p) : L"Not named");
         MemCat (&gMemTemp, L"</bold></a>");

         MemCat (&gMemTemp, L"</td><td width=66%>");
         pFunc->m_Meth.MemCatParam (&gMemTemp, FALSE);

         MemCat (&gMemTemp, L" - ");
         if (((PWSTR)(pFunc->m_Meth.m_memDescShort.p))[0]) {
            MemCatSanitize (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memDescShort.p);
         }
         MemCat (&gMemTemp, L"</td></tr>");
      } // i

      if (!i)
         MemCat (&gMemTemp, L"<p><bold>The object doesn't contain any private methods.</bold></p>");

      p->pszSubString = (PWSTR)gMemTemp.p;
      return TRUE;
   }

   else if (!_wcsicmp(p->pszSubName, L"METHPUBLIST")) {
      MemZero (&gMemTemp);

      // BUGFIX - Only do this for certain tabs
      if (*pmp->pdwTab != 3) {
         p->pszSubString = (PWSTR)gMemTemp.p;   // BUGFIX - Was &gMemTemp.p
         return TRUE;
      }

      DWORD i;
      for (i = 0; i < MethPubNum(); i++) {
         PCMIFLFunc pFunc = MethPubGet(i);

         MemCat (&gMemTemp, L"<tr><td width=33%><a href=methodpub");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"><bold>");
         MemCatSanitize (&gMemTemp, ((PWSTR)(pFunc->m_Meth.m_memName.p))[0] ? (PWSTR)(pFunc->m_Meth.m_memName.p) : L"Not named");
         MemCat (&gMemTemp, L"</bold></a>");

         MemCat (&gMemTemp, L"</td><td width=66%>");
         // find the public definition
         PCMIFLMeth pPub = pProj->MethDefUsed ((PWSTR)pFunc->m_Meth.m_memName.p);

         if (pPub)
            pPub->MemCatParam (&gMemTemp, FALSE);

         MemCat (&gMemTemp, L" - ");
         if (pPub && ((PWSTR)(pPub->m_memDescShort.p))[0]) {
            MemCatSanitize (&gMemTemp, (PWSTR)pPub->m_memDescShort.p);
         }

         // help link
         MemCat (&gMemTemp, L" ");
         pProj->HelpMML (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);


         MemCat (&gMemTemp, L"</td></tr>");
      } // i

      if (!i)
         MemCat (&gMemTemp, L"<p><bold>The object doesn't contain any public methods.</bold></p>");

      p->pszSubString = (PWSTR)gMemTemp.p;
      return TRUE;
   }

   else if (!_wcsicmp(p->pszSubName, L"PROPPRIVSLOT")) {
      MemZero (&gMemTemp);

      // BUGFIX - Only do this for certain tabs
      if (*pmp->pdwTab != 2) {
         p->pszSubString = (PWSTR)gMemTemp.p;   // BUGFIX - Was &gMemTemp.p
         return TRUE;
      }

      // how many slots?
      DWORD dwNum = pLib->m_fReadOnly ? m_lPCMIFLPropPriv.Num() : max(1, m_lPCMIFLPropPriv.Num()+1);
      DWORD i;
      WCHAR szTemp[32];

      // PCMIFLLib pLib = ParentGetOnlyLib();

      for (i = 0; i < dwNum; i++) {
         // BUGFIX - If long description then make multiline
         PCMIFLProp *ppp = (PCMIFLProp*)m_lPCMIFLPropPriv.Get(i);
         PCMIFLProp pProp = ppp ? ppp[0] : NULL;

         BOOL fLongVersion = pProp && pProp->m_memInit.p &&
            ( (wcslen((PWSTR)pProp->m_memInit.p) > 40) || wcschr((PWSTR)pProp->m_memInit.p, L'\r') );


         MemCat (&gMemTemp, L"<tr>"
            L"<td width=33%><bold><edit width=100% maxchars=64 ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"readonly=true ");
         MemCat (&gMemTemp, L"name=propprivname");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold></td>");

         MemCat (&gMemTemp, L"<td width=33%>");

         MemCat (&gMemTemp, L"<small><font face=courier><edit width=100% maxchars=20000 capturetab=true multiline=true ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"readonly=true ");
         if (fLongVersion)
            MemCat (&gMemTemp, L"height=150 wordwrap=true ");
         else
            MemCat (&gMemTemp, L"height=28 ");

         MemCat (&gMemTemp, L"name=propprivinit");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></font></small><br/><bold>");
         
         MemCat (&gMemTemp, L"<button checkbox=true style=x ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"enabled=false ");
         MemCat (&gMemTemp, L"margintopbottom=2 buttonheight=16 buttonwidth=16 name=propprivgetset");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L">Get/set<xHoverHelp>If checked then special functions will be used for getting and setting values.</xHoverHelp></button>");

         MemCat (&gMemTemp, L"<button margintopbottom=2 buttonheight=16 buttonwidth=16 enabled=false name=propprivget");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L">Get<xHoverHelp>Modify the code for getting the value.</xHoverHelp></button>");

			MemCat (&gMemTemp, L"<button margintopbottom=2 buttonheight=16 buttonwidth=16 enabled=false name=propprivset");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L">Set<xHoverHelp>Modify the code for setting the value.</xHoverHelp></button>");
         MemCat (&gMemTemp, L"</bold> ");
         swprintf (szTemp, L"propprivvalueindex:%d", (int)i);
         pProj->HelpMML (&gMemTemp, szTemp, FALSE);
         MemCat (&gMemTemp, L"</td>");

         MemCat (&gMemTemp,
            L"<td width=33%><bold><edit width=100% maxchars=20000 multiline=true wordwrap=true ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"readonly=true ");
         MemCat (&gMemTemp, L"name=propprivdesc");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold></td></tr>");
      } //i

      p->pszSubString = (PWSTR)gMemTemp.p;
      return TRUE;
   }
   else if (!_wcsicmp(p->pszSubName, L"PROPPUBSLOT")) {
      MemZero (&gMemTemp);

      // BUGFIX - Only do this for certain tabs
      if (*pmp->pdwTab != 2) {
         p->pszSubString = (PWSTR)gMemTemp.p;   // BUGFIX - Was &gMemTemp.p
         return TRUE;
      }

      // how many slots?
      DWORD dwNum = pLib->m_fReadOnly ? m_lPCMIFLPropPub.Num() : max(1, m_lPCMIFLPropPub.Num()+1);
      DWORD i;

      // PCMIFLLib pLib = ParentGetOnlyLib();

      WCHAR szTemp[32];
      for (i = 0; i < dwNum; i++) {
         // BUGFIX - If long description then make multiline
         PCMIFLProp *ppp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(i);
         PCMIFLProp pProp = ppp ? ppp[0] : NULL;

         MemCat (&gMemTemp, L"<tr>"
            L"<td width=33% align=center><bold><filteredlist width=90% sort=false cbheight=300 additem=\"\" blank=\"Blank\" ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"enabled=false ");
         MemCat (&gMemTemp, L"listname=proppub name=proppubname");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold> ");

         swprintf (szTemp, L"proppubindex:%d", (int)i);
         pProj->HelpMML (&gMemTemp, szTemp, FALSE);

         MemCat (&gMemTemp, L"</td>");

         BOOL fLongVersion = pProp && pProp->m_memInit.p &&
            ( (wcslen((PWSTR)pProp->m_memInit.p) > 40) || wcschr((PWSTR)pProp->m_memInit.p, L'\r') );

         MemCat (&gMemTemp,
            fLongVersion ? L"<td width=66%>" : L"<td width=33%>");

         MemCat (&gMemTemp, L"<small><font face=courier><edit width=100% maxchars=20000 capturetab=true multiline=true ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"readonly=true ");
         if (fLongVersion)
            MemCat (&gMemTemp, L"height=150 wordwrap=true ");
         else
            MemCat (&gMemTemp, L"height=28 ");

         MemCat (&gMemTemp, L"name=proppubinit");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></font></small>");

         if (fLongVersion)
            MemCat (&gMemTemp, L"<br/><bold>");
         else
            MemCat (&gMemTemp, L"</td><td><bold>");
         MemCat (&gMemTemp, L"<button checkbox=true style=x ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"enabled=false ");
         MemCat (&gMemTemp, L"margintopbottom=2 buttonheight=16 buttonwidth=16 name=proppubgetset");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L">Get/set<xHoverHelp>If checked then special functions will be used for getting and setting values.</xHoverHelp></button>");

         MemCat (&gMemTemp, L"<button margintopbottom=2 buttonheight=16 buttonwidth=16 enabled=false name=proppubget");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L">Get<xHoverHelp>Modify the code for getting the value.</xHoverHelp></button>");

			MemCat (&gMemTemp, L"<button margintopbottom=2 buttonheight=16 buttonwidth=16 enabled=false name=proppubset");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L">Set<xHoverHelp>Modify the code for setting the value.</xHoverHelp></button> ");
         // MemCat (&gMemTemp, L"</bold> ");
         swprintf (szTemp, L"proppubvalueindex:%d", (int)i);
         pProj->HelpMML (&gMemTemp, szTemp, FALSE);
         //MemCat (&gMemTemp, L"</td>");

         //MemCat (&gMemTemp,
         //   L"<td width=33%><bold><edit width=100% maxchars=20000 multiline=true wordwrap=true readonly=true name=proppubdesc");
         //MemCat (&gMemTemp, (int)i);
         //MemCat (&gMemTemp, L"/></bold>");

         MemCat (&gMemTemp, L"</bold></td></tr>");
      } //i

      p->pszSubString = (PWSTR)gMemTemp.p;
      return TRUE;
   }

   // else, didn't handle
   return FALSE;
}


/*********************************************************************************
ObjEditPage - UI
*/

BOOL ObjEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLObject pObj = pmp->pObj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // initialize params
         pObj->PageInitPage (pPage, pmp);

         // will need to set movelib combo
         MIFLComboLibs (pPage, L"movelib", L"move", pProj, pLib->m_dwTempID);
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;

         if (pObj->PageLink (pPage, pmp, p, pmp->pLib))
            return TRUE;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;

         PWSTR pszIndex = L"index:";
         DWORD dwIndexLen = (DWORD)wcslen(pszIndex);
         if (!wcsncmp (psz, pszIndex, dwIndexLen)) {
            pProj->HelpIndex (pPage, p->psz + dwIndexLen);
            return TRUE;
         }

         PWSTR pszPropPubIndex = L"proppubindex:", pszPropPubValueIndex = L"proppubvalueindex:",
            pszPropPrivValueIndex = L"propprivvalueindex:";
         DWORD dwPropPubIndexLen = (DWORD)wcslen (pszPropPubIndex), dwPropPubValueIndexLen =(DWORD) wcslen (pszPropPubValueIndex),
            dwPropPrivValueIndexLen = (DWORD)wcslen (pszPropPrivValueIndex);
         if (!wcsncmp (psz, pszPropPubIndex, dwPropPubIndexLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwPropPubIndexLen);

            PCMIFLProp *ppp = (PCMIFLProp*) pObj->m_lPCMIFLPropPub.Get(dwNum);
            PCMIFLProp pp = ppp ? *ppp : NULL;

            pProj->HelpIndex (pPage, pp ? (PWSTR)pp->m_memName.p : L"");
            return TRUE;
         }
         if (!wcsncmp (psz, pszPropPubValueIndex, dwPropPubValueIndexLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwPropPubValueIndexLen);

            PCMIFLProp *ppp = (PCMIFLProp*) pObj->m_lPCMIFLPropPub.Get(dwNum);
            PCMIFLProp pp = ppp ? *ppp : NULL;

            pProj->HelpIndex (pPage, pp ? (PWSTR)pp->m_memInit.p : L"", TRUE);
            return TRUE;
         }
         if (!wcsncmp (psz, pszPropPrivValueIndex, dwPropPrivValueIndexLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwPropPrivValueIndexLen);

            PCMIFLProp *ppp = (PCMIFLProp*) pObj->m_lPCMIFLPropPriv.Get(dwNum);
            PCMIFLProp pp = ppp ? *ppp : NULL;

            pProj->HelpIndex (pPage, pp ? (PWSTR)pp->m_memInit.p : L"", TRUE);
            return TRUE;
         }

         if (!_wcsicmp(psz, L"helpobject")) {
            pProj->HelpIndex (pPage, (PWSTR)pObj->m_memContained.p);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"NewID")) {
            pLib->AboutToChange();
            GUIDGen (&pObj->m_gID);
            pLib->Changed();

            // set the text
            WCHAR szTemp[64];
            StringFromGUID2 (pObj->m_gID, szTemp, sizeof(szTemp)/sizeof(WCHAR));
            PCEscControl pControl = pPage->ControlFind (L"ID");
            if (pControl)
               pControl->AttribSet (Text(), szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"recommendproplist")) {
            if (!p->pszName || pLib->m_fReadOnly)
               return TRUE;   // ignore since no name

            pLib->AboutToChange();

            // else, add property (or use last empty one)
            PCMIFLProp *ppp = (PCMIFLProp*)pObj->m_lPCMIFLPropPub.Get(0);
            PCMIFLProp pProp;
            if (pObj->m_lPCMIFLPropPub.Num() && !((PWSTR)ppp[pObj->m_lPCMIFLPropPub.Num()-1]->m_memName.p)[0] )
               pProp = ppp[pObj->m_lPCMIFLPropPub.Num()-1];
            else {
               pProp = new CMIFLProp;
               if (!pProp)
                  return TRUE;
               pObj->m_lPCMIFLPropPub.Add (&pProp);
            }
            MemZero (&pProp->m_memName);
            MemCat (&pProp->m_memName, p->pszName);

            pLib->Changed();
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"recommendmethodlist")) {
            if (!p->pszName || pLib->m_fReadOnly)
               return TRUE;   // ignore since no name

            // make sure doens't already have
            if ((DWORD)-1 != pObj->MethPubFind (p->pszName, (DWORD)-1)) {
               pPage->MBWarning (L"You already have that method.");
               return TRUE;
            }

            // add
            DWORD dwPropID = pObj->MethPubNew (p->pszName, pLib);
            if (dwPropID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dmethpub:%dedit", (int) pLib->m_dwTempID, (int)pObj->m_dwTempID, (int) dwPropID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"innewobjectmenu")) {
            DWORD dwNum = p->pszName ? _wtoi(p->pszName) : 0;
            if (pObj->m_dwInNewObjectMenu == dwNum)
               return TRUE;   // no change

            pLib->AboutToChange();
            pObj->m_dwInNewObjectMenu = dwNum;
            pLib->Changed();
            return TRUE;
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (pObj->PageButtonPress (pPage, pmp, p, pmp->pLib))
            return TRUE;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this object?"))
               return TRUE;

            // remove the object
            pLib->ObjectRemove (pObj->m_dwTempID);

            // set a link to go to the Object list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobjectist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated object.");

            // duplicate
            DWORD dwID = pLib->ObjectAddCopy (pObj, TRUE, TRUE);

            // set a link to go to the objectd list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit", pLib->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"move")) {
            // find where moved to
            PCEscControl pControl = pPage->ControlFind (L"movelib");
            if (!pControl)
               return TRUE;
            ESCMCOMBOBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            if (!gi.pszName)
               return FALSE;

            // find the library
            DWORD dwLibIndex = _wtoi(gi.pszName);
            PCMIFLLib pLibTo = pProj->LibraryGet (dwLibIndex);
            if (!pLibTo)
               return TRUE;

            if (pLib->m_fReadOnly) {
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the object?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The object will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->ObjectAddCopy (pObj, FALSE, FALSE);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->ObjectRemove (pObj->m_dwTempID);

            // set a link to go to the object list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"propprivadd")|| !_wcsicmp(psz, L"proppubadd")) {
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         BOOL fRename;

         if (pObj->PageEditChange (pPage, pmp, p, pmp->pLib, &fRename)) {
            // if renamed then need to resort the list
            if (fRename) {
               pLib->AboutToChange();
               pLib->ObjectSort();
               pLib->Changed();
            }

            return TRUE;
         }
      }
      break;

   case ESCN_FILTEREDLISTCHANGE:
      {
         PESCNFILTEREDLISTCHANGE p = (PESCNFILTEREDLISTCHANGE) pParam;

         if (pObj->PageFilteredListChange (pPage, pmp, p, pmp->pLib))
            return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (pObj->PageSubstitution (pPage, pmp, p, pmp->pLib))
            return TRUE;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Modify object - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pObj->m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pObj->m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MIFLTABS")) {
            PWSTR apsz[] = {L"Description", L"Super-classes", L"Properties", L"Methods", L"Misc."};
            PWSTR apszHelp[] = {
               L"Lets you modify the object's name and description.",
               L"Lets you modify what super-classes the object belongs to.",
               L"Lets you modify the object's properties.",
               L"Lets you modify the object's methods.",
               L"Lets you do miscellaneous changes to the object."
            };
            p->pszSubString = MIFLTabs (*pmp->pdwTab,
               sizeof(apsz)/sizeof(PWSTR), apsz, apszHelp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->ObjectOverridden (pLib->m_dwTempID, (PWSTR)pObj->m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by object in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides object in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/*********************************************************************************
ObjClassAddPage - UI
*/

BOOL ObjClassAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLObject pObj = pmp->pObj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // loop through all the classes that this is a member of and check them
         WCHAR szTemp[128];
         PCEscControl pControl;
         DWORD i;
         for (i = 0; i < pObj->m_lClassSuper.Num(); i++) {
            wcscpy (szTemp, L"class:");
            wcscat (szTemp, (PWSTR)pObj->m_lClassSuper.Get(i));

            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         } // i

         // check the sort flag
         pControl = pPage->ControlFind (L"sortbycount");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), gfSortByCount);

         // set the combo
         ComboBoxSet (pPage, L"showonly", gdwShowOnly);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"sortbycount")) {
            gfSortByCount = p->pControl->AttribGetBOOL (Checked());
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }

         PWSTR pszClass = L"class:";
         DWORD dwClassLen = (DWORD)wcslen(pszClass);
         if (!wcsncmp(psz, pszClass, dwClassLen)) {
            // has checked on a class
            psz += dwClassLen;

            // see if it's in the list
            DWORD i;
            for (i = 0; i < pObj->m_lClassSuper.Num(); i++)
               if (!_wcsicmp(psz, (PWSTR)pObj->m_lClassSuper.Get(i)))
                  break;
            if (i < pObj->m_lClassSuper.Num()) {
               // unchecked an existing library, so delete it
               pLib->AboutToChange();
               pObj->m_lClassSuper.Remove (i);
               pLib->Changed();
               return TRUE;
            }

            // else, insert on the top
            pLib->AboutToChange();
            pObj->ClassAddWithRecommend (psz, !(GetKeyState (VK_CONTROL) < 0), pProj);
            pLib->Changed();
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"showonly")) {
            DWORD dwNum = p->pszName ? _wtoi(p->pszName) : 0;
            if (gdwShowOnly == dwNum)
               return TRUE;   // no change

            // else, change
            gdwShowOnly = dwNum;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;

         PWSTR pszIndex = L"index:";
         DWORD dwIndexLen = (DWORD)wcslen(pszIndex);
         if (!wcsncmp (psz, pszIndex, dwIndexLen)) {
            pProj->HelpIndex (pPage, p->psz + dwIndexLen);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (pObj->PageSubstitution (pPage, pmp, p, pmp->pLib))
            return TRUE;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Add class(es) - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pObj->m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pObj->m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SHOWONLY")) {
            MemZero (&gMemTemp);
            CListVariable lObj;

            pProj->ObjectEnum (&lObj);

            DWORD i;
            for (i = 0; i < lObj.Num(); i++) {
               PWSTR psz = (PWSTR)lObj.Get(i);
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i+1);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</elem>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
         }
         else if (!_wcsicmp(p->pszSubName, L"CLASSLIST")) {
            MemZero (&gMemTemp);

            // enumerate the relationshop
            CListFixed lMIFLOER, lLib;
            DWORD i, j;
            pProj->ObjectEnumRelationship (&lMIFLOER, gfSortByCount);
            PMIFLOER poe = (PMIFLOER) lMIFLOER.Get(0);

            // keep the list of objects around if gdwShowOnly
            CListVariable lObj;
            PWSTR pszShowOnly = NULL;
            if (gdwShowOnly) {
               pProj->ObjectEnum (&lObj);
               pszShowOnly = (PWSTR)lObj.Get(gdwShowOnly - 1);
            }

            // write out entries
            BOOL fFound = FALSE;
            for (i = 0; i < lMIFLOER.Num(); i++) {
               // get the name, and continue if it's the name of the current object
               PWSTR pszName = (PWSTR) poe[i].plSuper->Get(0);
               if (!_wcsicmp(pszName, (PWSTR)pObj->m_memName.p))
                  continue;

               // determine which libraies its in
               lLib.Clear();
               pProj->ObjectInLib (pszName, &lLib);
               PCMIFLLib *ppl = (PCMIFLLib*)lLib.Get(0);
               if (!lLib.Num())
                  continue;   // shouldnt happen

               // get the object as it appears from the first library, so can get
               // a description
               PCMIFLObject pUse = ppl[0]->ObjectGet(ppl[0]->ObjectFind(pszName, -1));
               if (!pUse)
                  continue;   // shouldnt happen

               // may want to cancel out if show-only is set
               if (pszShowOnly) {
                  for (j = 0; j < poe[i].plSuper->Num(); j++)
                     if (!_wcsicmp((PWSTR)poe[i].plSuper->Get(j), pszShowOnly))
                        break;
                  if (j >= poe[i].plSuper->Num())
                     continue;   // not in list
               }

               // make up the table
               fFound = TRUE;
               MemCat (&gMemTemp, L"<tr><td width=33%>");
               MemCat (&gMemTemp, L"<button checkbox=true style=x ");
               if (pLib->m_fReadOnly)
                  MemCat (&gMemTemp, L"enabled=false ");
               MemCat (&gMemTemp, L"name=\"class:");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"\"><bold>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"</bold>");
               MemCat (&gMemTemp, L"</button> ");
               pProj->HelpMML (&gMemTemp, (PWSTR)pUse->m_memName.p);
               MemCat (&gMemTemp, L"</td><td width=66%>");
               
               // if is self-recursing, indicate this and error
               if (poe[i].fSelfRef)
                  MemCat (&gMemTemp, L"<bold><font color=#800000>ERROR: The class references itself as a subclass.</font></bold><br/>");

               PWSTR pszDesc = (PWSTR)pUse->m_memDescShort.p;
               if (pszDesc && pszDesc[0]) {
                  MemCatSanitize (&gMemTemp, pszDesc);
                  MemCat (&gMemTemp, L"<p/>");
               }
               
               // show the libraries
               MemCat (&gMemTemp, L"<bold>Library:</bold> ");
               for (j = 0; j < lLib.Num(); j++) {
                  if (j)
                     MemCat (&gMemTemp, L", ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(ppl[j]->m_szFile));
               } // j
               MemCat (&gMemTemp, L"<br/>");

               // show what sub-classes it's a member of
               MemCat (&gMemTemp, L"<bold>Sub-class of:</bold> ");
               for (j = 1; j < poe[i].plSuper->Num(); j++) {
                  if (j >= 2)
                     MemCat (&gMemTemp, L", ");
                  MemCatSanitize (&gMemTemp, (PWSTR)poe[i].plSuper->Get(j));
               } // j
               if (poe[i].plSuper->Num() <= 1)
                  MemCat (&gMemTemp, L"<italic>None</italic>");
               MemCat (&gMemTemp, L"<br/>");

               // show recommended properties and methods
               if (pUse->m_lRecProp.Num()) {
                  MemCat (&gMemTemp, L"<bold>Recommended properties:</bold> ");
                  for (j = 0; j < pUse->m_lRecProp.Num(); j++) {
                     if (j)
                        MemCat (&gMemTemp, L", ");
                     MemCatSanitize (&gMemTemp, (PWSTR)pUse->m_lRecProp.Get(j));
                  } // j
                  MemCat (&gMemTemp, L"<br/>");
               }
               if (pUse->m_lRecMeth.Num()) {
                  MemCat (&gMemTemp, L"<bold>Recommended methods:</bold> ");
                  for (j = 0; j < pUse->m_lRecMeth.Num(); j++) {
                     if (j)
                        MemCat (&gMemTemp, L", ");
                     MemCatSanitize (&gMemTemp, (PWSTR)pUse->m_lRecMeth.Get(j));
                  } // j
                  MemCat (&gMemTemp, L"<br/>");
               }
               MemCat (&gMemTemp, L"</td></tr>");
            } // i
            
            if (!fFound)
               MemCat (&gMemTemp, L"<tr><td><bold>No classes to list.</bold></td></tr>");

            // free up
            for (i = 0; i < lMIFLOER.Num(); i++)
               if (poe[i].plSuper)
                  delete poe[i].plSuper;

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}






/*********************************************************************************
ObjRecEditPage - UI
*/

BOOL ObjRecEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLObject pObj = pmp->pObj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // loop through all the properteis and methods that this is a member of and check them
         WCHAR szTemp[128];
         PCEscControl pControl;
         DWORD i;
         for (i = 0; i < pObj->m_lRecProp.Num(); i++) {
            wcscpy (szTemp, L"prop:");
            wcscat (szTemp, (PWSTR)pObj->m_lRecProp.Get(i));

            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         } // i
         for (i = 0; i < pObj->m_lRecMeth.Num(); i++) {
            wcscpy (szTemp, L"meth:");
            wcscat (szTemp, (PWSTR)pObj->m_lRecMeth.Get(i));

            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);
         } // i
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszProp = L"prop:", pszMeth = L"meth:";
         DWORD dwPropLen = (DWORD)wcslen(pszProp), dwMethLen = (DWORD)wcslen(pszMeth);
         if (!wcsncmp(psz, pszProp, dwPropLen) || !wcsncmp(psz, pszMeth, dwMethLen)) {
            PCListVariable pl;
            if (!wcsncmp(psz, pszProp, dwPropLen)) {
               psz += dwPropLen;
               pl = &pObj->m_lRecProp;
            }
            else {
               psz += dwMethLen;
               pl = &pObj->m_lRecMeth;
            }

            // see if it's in the list
            DWORD i;
            for (i = 0; i < pl->Num(); i++)
               if (!_wcsicmp(psz, (PWSTR)pl->Get(i)))
                  break;
            if (i < pl->Num()) {
               // unchecked an existing prop/meth, so delete it
               pLib->AboutToChange();
               pl->Remove (i);
               pLib->Changed();
               return TRUE;
            }

            // else, insert on the top
            pLib->AboutToChange();
            pl->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
            pLib->Changed();
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;

         PWSTR pszIndex = L"index:";
         DWORD dwIndexLen = (DWORD)wcslen(pszIndex);
         if (!wcsncmp (psz, pszIndex, dwIndexLen)) {
            pProj->HelpIndex (pPage, p->psz + dwIndexLen);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (pObj->PageSubstitution (pPage, pmp, p, pmp->pLib))
            return TRUE;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Recommended - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pObj->m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pObj->m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PROPLIST") || !_wcsicmp(p->pszSubName, L"METHLIST")) {
            PCListVariable pl;
            BOOL fProp;
            if (!_wcsicmp(p->pszSubName, L"PROPLIST")) {
               pl = &pObj->m_lRecProp;
               fProp = TRUE;
            }
            else {
               pl = &pObj->m_lRecMeth;
               fProp = FALSE;
            }

            // loop through all the superclasses and see what they recommend...
            CBTree tRec;
            CListFixed lLib;
            DWORD i, j, k;
            for (i = 0; i < pObj->m_lClassSuper.Num(); i++) {
               PWSTR pszSuper = (PWSTR)pObj->m_lClassSuper.Get(i);

               // look through super and find all the places it's defined
               lLib.Clear();
               pProj->ObjectInLib (pszSuper, &lLib);
               PCMIFLLib *ppl = (PCMIFLLib*) lLib.Get(0);

               // look through all the libraries
               for (j = 0; j < lLib.Num(); j++) {
                  PCMIFLObject pUse = ppl[j]->ObjectGet(ppl[j]->ObjectFind(pszSuper,-1));
                  if (!pUse)
                     continue;

                  // loop through those methods/properties that are recommended and
                  // add them to the list if they're not already
                  PCListVariable plUse = fProp ? &pUse->m_lRecProp : &pUse->m_lRecMeth;
                  for (k = 0; k < plUse->Num(); k++) {
                     tRec.Add ((PWSTR)plUse->Get(k), &k, sizeof(k));
                  } // k, plUse
               } // j, over lLib
            } // i, over pObj->m_lClassSuper

            // keep track of what's checked
            CBTree tChecked;
            for (i = 0; i < pl->Num(); i++)
               tChecked.Add ((PWSTR)pl->Get(i), &i, sizeof(i));

            // make a list of all the properties or methods...
            CListVariable lAll;
            MIFLPAGE mp;
            memset (&mp, 0, sizeof(mp));
            mp.pProj = pProj;
            if (fProp) {
               mp.plPropPub = &lAll;
               MIFLFilteredListPropPub (&mp);
            }
            else {
               mp.plMethPub = &lAll;
               MIFLFilteredListMethPub (&mp, NULL);
            }

            // fill in the list
            MemZero (&gMemTemp);

            BOOL fFound = FALSE;
            BOOL fSkipThis;
            for (i = 0; i < lAll.Num(); i++) {
               PWSTR pszName = (PWSTR)lAll.Get(i);
               fSkipThis = FALSE;

               // try to find the description
               PWSTR pszDesc = NULL;
               for (j = 0; j < pProj->LibraryNum(); j++) {
                  PCMIFLLib pLibUse = pProj->LibraryGet(j);
                  DWORD dwFind;

                  if (fProp) {
                     dwFind = pLibUse->PropDefFind (pszName, -1);
                     if (dwFind == -1)
                        continue;
                     PCMIFLProp pProp = pLibUse->PropDefGet (dwFind);
                     if (pProp)
                        pszDesc = (PWSTR)pProp->m_memDescShort.p;
                  }
                  else {
                     dwFind = pLibUse->MethDefFind (pszName, -1);
                     if (dwFind == -1)
                        continue;
                     PCMIFLMeth pMeth = pLibUse->MethDefGet (dwFind);
                     if (pMeth) {
                        pszDesc = (PWSTR)pMeth->m_memDescShort.p;

                        // BUGFIX - Dont include methods automatically supported by all objects
                        if (pMeth->m_fCommonAll) {
                           fSkipThis = TRUE;
                           break;
                        }
                     }
                  }
                  if (pszDesc)
                     break;
               } // j, over libraryies
               if (fSkipThis)
                  continue;   // if common to all dont show

               fFound = TRUE;

               MemCat (&gMemTemp, L"<tr><td width=33%>");
               MemCat (&gMemTemp, L"<button checkbox=true style=x ");
               if (pLib->m_fReadOnly)
                  MemCat (&gMemTemp, L"enabled=false ");
               MemCat (&gMemTemp, L"name=\"");
               MemCat (&gMemTemp, fProp ? L"prop:" : L"meth:");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"\"><bold>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"</bold>");
               MemCat (&gMemTemp, L"</button> ");
               pProj->HelpMML (&gMemTemp, pszName);
               
               MemCat (&gMemTemp, L"</td><td width=66%>");
               // if this is recommended by a super-class but not checked then not
               if (tRec.Find (pszName) && !tChecked.Find(pszName)) {
                  MemCat (&gMemTemp, L"<bold><font color=#800000>Recommended by a superclass of ");
                  MemCatSanitize (&gMemTemp, (PWSTR)pObj->m_memName.p);
                  MemCat (&gMemTemp, L" but not checked.</font></bold><br/>");
               }

               if (pszDesc && pszDesc[0]) {
                  MemCatSanitize (&gMemTemp, pszDesc);
                  // MemCat (&gMemTemp, L"<p/>");
               }
               
               MemCat (&gMemTemp, L"</td></tr>");
            } // i
            if (!fFound)
               MemCat (&gMemTemp, 
                  fProp ? L"<tr><td><bold>There aren't any properties.</bold></td></tr>" :
                     L"<tr><td><bold>There aren't any methods.</bold></td></tr>"
               );

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/********************************************************************************
CMIFLObject::ClassAddWithRecommend - Adds the class with all the properties
and methods that are recommended.

inputs
   PWSTR             pszClass - Class to use
   BOOL              fWithRecommend - If TRUE then add with recommend
   PCMIFLProj        pProj - Project that this is in
returns
   BOOL - TRUE if success
*/
BOOL CMIFLObject::ClassAddWithRecommend (PWSTR pszClass, BOOL fWithRecommend, PCMIFLProj pProj)
{
   m_lClassSuper.Insert (0, pszClass, (wcslen(pszClass)+1)*sizeof(WCHAR));

   // will need to add recommended methods and properites
   // unless control is checked
   if (!fWithRecommend)
      return TRUE;

   // get all the libraries that the object appears in
   CListFixed lLib;
   pProj->ObjectInLib (pszClass, &lLib);
   PCMIFLLib *ppl = (PCMIFLLib*) lLib.Get(0);

   // go through all the libs
   DWORD i;
   for (i = 0; i < lLib.Num(); i++) {
      PCMIFLObject pUse = ppl[i]->ObjectGet(ppl[i]->ObjectFind(pszClass,-1));
      if (!pUse)
         continue;

      // loop through all the super-classes recommended methods and props
      DWORD dwType;
      DWORD j;
      for (dwType = 0; dwType < 2; dwType++) {
         PCListVariable pSrc = dwType ? &pUse->m_lRecMeth : &pUse->m_lRecProp;
         PCListVariable pDest = dwType ? &m_lRecMeth : &m_lRecProp;

         // create a tree of all the methods/properties currently supported
         CBTree tHave;
         tHave.Clear();
         if (!dwType) {
            PCMIFLProp *ppp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
            for (j = 0; j < m_lPCMIFLPropPub.Num(); j++) {
               tHave.Add ((PWSTR)ppp[j]->m_memName.p, &j, sizeof(j));
            } // j
         }

         // loop over the source's and see if they appear in the dest
         for (j = 0; j < pSrc->Num(); j++) {
            PWSTR pszSrc = (PWSTR)pSrc->Get(j);

            // if appear then skip, dont add again
            if (dwType) {
               // see if method already exists
               if (MethPubFind (pszSrc, -1) != -1)
                  continue;
            }
            else {
               // use sorted list
               if (tHave.Find (pszSrc))
                  continue;
            }

            // else, add...
            if (dwType)
               MethPubNew (pszSrc, NULL); // pass NULL do doesnt send change
            else {
               PCMIFLProp pProp = new CMIFLProp;
               if (!pProp)
                  continue;
               MemZero (&pProp->m_memName);
               MemCat (&pProp->m_memName, pszSrc);
               m_lPCMIFLPropPub.Add (&pProp);
               tHave.Add (pszSrc, &i, sizeof(i));
            }
         } // j, src
      } // dwType
   } // i, all libs

   return TRUE;
}

/********************************************************************************
CMIFLObject::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library to use. Contains the object
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dobject%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLObject::DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   PWSTR pszPropPriv = L"proppriv:", pszPropPub = L"proppub:";
   DWORD dwPropPrivLen = (DWORD)wcslen(pszPropPriv), dwPropPubLen = (DWORD)wcslen(pszPropPub);
   PWSTR pszMethPriv = L"methpriv:", pszMethPub = L"methpub:";
   DWORD dwMethPrivLen = (DWORD)wcslen(pszMethPriv), dwMethPubLen = (DWORD)wcslen(pszMethPub);

   if (!_wcsnicmp(pszUse, pszPropPriv, dwPropPrivLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwPropPrivLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the property
      DWORD dwIndex;
      PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPriv.Get(0);
      for (dwIndex = 0; dwIndex < m_lPCMIFLPropPriv.Num(); dwIndex++) {
         if (pp[dwIndex]->m_dwTempID == dwNum)
            break;
      }
      if (dwIndex >= m_lPCMIFLPropPriv.Num())
         goto defpage;
      PCMIFLProp pProp = pp[dwIndex];

      if (!pProp->DialogEditSingleGlobal (pLib, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszPropPub, dwPropPubLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwPropPubLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the property
      DWORD dwIndex;
      PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropPub.Get(0);
      for (dwIndex = 0; dwIndex < m_lPCMIFLPropPub.Num(); dwIndex++) {
         if (pp[dwIndex]->m_dwTempID == dwNum)
            break;
      }
      if (dwIndex >= m_lPCMIFLPropPub.Num())
         goto defpage;
      PCMIFLProp pProp = pp[dwIndex];

      if (!pProp->DialogEditSingleGlobal (pLib, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszMethPriv, dwMethPrivLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwMethPrivLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the Metherty
      DWORD dwIndex = MethPrivFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLFunc pFunc = MethPrivGet (dwIndex);
      if (!pFunc)
         goto defpage;

      if (!pFunc->DialogEditSingleMethod (TRUE /*priv*/, pLib, this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszMethPub, dwMethPubLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwMethPubLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the Metherty
      DWORD dwIndex = MethPubFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLFunc pFunc = MethPubGet (dwIndex);
      if (!pFunc)
         goto defpage;

      if (!pFunc->DialogEditSingleMethod (FALSE, pLib, this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }

defpage:
   // what resource to use
   DWORD dwPage = IDR_MMLOBJEDIT;
   PESCPAGECALLBACK pPage = ObjEditPage;

   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLOBJEDIT;
      pPage = ObjEditPage;
   }
   else if (!_wcsicmp(pszUse, L"classadd")) {
      dwPage = IDR_MMLOBJCLASSADD;
      pPage = ObjClassAddPage;
   }
   else if (!_wcsicmp(pszUse, L"recedit")) {
      dwPage = IDR_MMLOBJRECEDIT;
      pPage = ObjRecEditPage;
   }

   PWSTR pszRet;
   MIFLPAGE mp;
   CListVariable lPropPub, lMethPub, lObject, lClass;
   memset (&mp, 0, sizeof(mp));
   mp.pObj = this;
   mp.pLib = pLib;
   mp.pProj = mp.pLib->ProjectGet();
   mp.plPropPub = &lPropPub;
   mp.plMethPub = &lMethPub;
   mp.plObject = &lObject;
   mp.plClass = &lClass;
   mp.pdwTab = (dwPage == IDR_MMLOBJEDIT) ? &mp.pProj->m_dwTabObjEdit : NULL;   // set the tab
   mp.iVScroll = *piVScroll;
redo:
   pszRet = pWindow->PageDialog (ghInstance, dwPage, pPage, &mp);
   *piVScroll = mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, MIFLRedoSamePage()))
      goto redo;

   if (pszRet)
      wcscpy (pszNext, pszRet);
   else
      pszNext[0] = 0;
   return TRUE;
}







/********************************************************************************
CMIFLObject::MethPrivNum - Returns the number of MethPrivod definitions in the library
*/
DWORD CMIFLObject::MethPrivNum (void)
{
   return m_lPCMIFLMethPriv.Num();
}


/********************************************************************************
CMIFLObject::MethPrivGet - Returns a pointer to a MethPrivod definition based on
the index. This object can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call MethPrivSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..MethPrivNum()-1
returns
   PCMIFLMethPriv - MethPrivod, or NULL. do NOT delte this
*/
PCMIFLFunc CMIFLObject::MethPrivGet (DWORD dwIndex)
{
   PCMIFLFunc *ppm = (PCMIFLFunc*)m_lPCMIFLMethPriv.Get(0);
   if (dwIndex >= m_lPCMIFLMethPriv.Num())
      return NULL;
   return ppm[dwIndex];
}




static int _cdecl MIFLMethPrivCompare (const void *elem1, const void *elem2)
{
   PCMIFLFunc pdw1, pdw2;
   pdw1 = *((PCMIFLFunc*) elem1);
   pdw2 = *((PCMIFLFunc*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_Meth.m_memName.p, (PWSTR)pdw2->m_Meth.m_memName.p);
}

/********************************************************************************
CMIFLObject::MethPrivSort - Sorts all the MethPrivod names. If a MethPrivod name is changed
the list must be resorted.
*/
void CMIFLObject::MethPrivSort (void)
{
   qsort (m_lPCMIFLMethPriv.Get(0), m_lPCMIFLMethPriv.Num(),
      sizeof(PCMIFLFunc), MIFLMethPrivCompare);
}



/********************************************************************************
CMIFLObject::MethPrivFind - Given a unique ID, this finds the MethPrivod's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLObject::MethPrivFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLMethPriv.Num();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLMethPriv.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_Meth.m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLObject::MethPrivFind - Finds the MethPrivod with the given name.

NOTE: This relies on the MethPrivods to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this MethPrivod number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLObject::MethPrivFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLMethPriv.Num();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLMethPriv.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_Meth.m_memName.p);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_Meth.m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_Meth.m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_Meth.m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLObject::MethPrivNew - Create a new MethPrivod definition. This returns
the unique ID of the new MethPrivod. Or -1 if error

inputs
   PCMIFLLib            pLib - Library to used to notify of changes
*/
DWORD CMIFLObject::MethPrivNew (PCMIFLLib pLib)
{
   if (pLib->m_fReadOnly)
      return -1;

   PCMIFLFunc pNew = new CMIFLFunc;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_Meth.m_dwTempID;
   // pNew->ParentSet (this);

   MemZero (&pNew->m_Meth.m_memName);
   MemCat (&pNew->m_Meth.m_memName, L"NewPrivateMethod");

   pLib->AboutToChange();
   m_lPCMIFLMethPriv.Add (&pNew);
   MethPrivSort();
   pLib->Changed();

   return dwID;
}


/********************************************************************************
CMIFLObject::MethPrivRemove - Deletes the current MethPrivod from the list.

inputs
   DWORD          dwID - ID for the MethPrivod
   PCMIFLLib            pLib - Library to used to notify of changes
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLObject::MethPrivRemove (DWORD dwID, PCMIFLLib pLib)
{
   if (pLib->m_fReadOnly)
      return FALSE;

   DWORD dwIndex = MethPrivFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   pLib->AboutToChange();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLMethPriv.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLMethPriv.Remove (dwIndex);
   pLib->Changed();

   return TRUE;
}



/********************************************************************************
CMIFLObject::MethPrivAddCopy - This adds a copy of the given MethPrivod to this
library.

inputs
   PCMIFLFunc        pMethPriv - Funcod
   PCMIFLLib            pLib - Library to used to notify of changes
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLObject::MethPrivAddCopy (PCMIFLFunc pMethPriv, PCMIFLLib pLib)
{
   if (pLib->m_fReadOnly)
      return FALSE;

   PCMIFLFunc pNew = pMethPriv->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_Meth.m_dwTempID = dwID = MIFLTempID(); // so have new ID
   // pNew->ParentSet (this);

   pLib->AboutToChange();
   m_lPCMIFLMethPriv.Add (&pNew);
   MethPrivSort();
   pLib->Changed();

   return dwID;
}












/********************************************************************************
CMIFLObject::MethPubNum - Returns the number of MethPubod definitions in the library
*/
DWORD CMIFLObject::MethPubNum (void)
{
   return m_lPCMIFLMethPub.Num();
}


/********************************************************************************
CMIFLObject::MethPubGet - Returns a pointer to a MethPubod definition based on
the index. This object can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call MethPubSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..MethPubNum()-1
returns
   PCMIFLMethPub - MethPubod, or NULL. do NOT delte this
*/
PCMIFLFunc CMIFLObject::MethPubGet (DWORD dwIndex)
{
   PCMIFLFunc *ppm = (PCMIFLFunc*)m_lPCMIFLMethPub.Get(0);
   if (dwIndex >= m_lPCMIFLMethPub.Num())
      return NULL;
   return ppm[dwIndex];
}




/********************************************************************************
CMIFLObject::MethPubSort - Sorts all the MethPubod names. If a MethPubod name is changed
the list must be resorted.
*/
void CMIFLObject::MethPubSort (void)
{
   qsort (m_lPCMIFLMethPub.Get(0), m_lPCMIFLMethPub.Num(),
      sizeof(PCMIFLFunc), MIFLMethPrivCompare);
}



/********************************************************************************
CMIFLObject::MethPubFind - Given a unique ID, this finds the MethPubod's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLObject::MethPubFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLMethPub.Num();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLMethPub.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_Meth.m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLObject::MethPubFind - Finds the MethPubod with the given name.

NOTE: This relies on the MethPubods to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this MethPubod number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLObject::MethPubFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLMethPub.Num();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLMethPub.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_Meth.m_memName.p);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_Meth.m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_Meth.m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_Meth.m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLObject::MethPubNew - Create a new MethPubod definition. This returns
the unique ID of the new MethPubod. Or -1 if error

inputs
   PWSTR                pszName - Name to use
   PCMIFLLib            pLib - Library to used to notify of changes. Can be null
*/
DWORD CMIFLObject::MethPubNew (PWSTR pszName, PCMIFLLib pLib)
{
   if (pLib && pLib->m_fReadOnly)
      return -1;

   PCMIFLFunc pNew = new CMIFLFunc;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_Meth.m_dwTempID;
   // pNew->ParentSet (this);

   MemZero (&pNew->m_Meth.m_memName);
   MemCat (&pNew->m_Meth.m_memName, pszName);

   if (pLib)
      pLib->AboutToChange();
   m_lPCMIFLMethPub.Add (&pNew);
   MethPubSort();
   if (pLib)
      pLib->Changed();

   return dwID;
}


/********************************************************************************
CMIFLObject::MethPubRemove - Deletes the current MethPubod from the list.

inputs
   DWORD          dwID - ID for the MethPubod
   PCMIFLLib            pLib - Library to used to notify of changes
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLObject::MethPubRemove (DWORD dwID, PCMIFLLib pLib)
{
   if (pLib->m_fReadOnly)
      return FALSE;

   DWORD dwIndex = MethPubFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   pLib->AboutToChange();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLMethPub.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLMethPub.Remove (dwIndex);
   pLib->Changed();

   return TRUE;
}



/********************************************************************************
CMIFLObject::MethPubAddCopy - This adds a copy of the given MethPubod to this
library.

inputs
   PCMIFLFunc        pMethPub - Funcod
   PCMIFLLib            pLib - Library to used to notify of changes
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLObject::MethPubAddCopy (PCMIFLFunc pMethPub, PCMIFLLib pLib)
{
   if (pLib->m_fReadOnly)
      return FALSE;

   PCMIFLFunc pNew = pMethPub->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_Meth.m_dwTempID = dwID = MIFLTempID(); // so have new ID
   // pNew->ParentSet (this);

   pLib->AboutToChange();
   m_lPCMIFLMethPub.Add (&pNew);
   MethPubSort();
   pLib->Changed();

   return dwID;
}






/********************************************************************************
CMIFLObject::Merge - This merges two or more libraries together into this library.

NOTE: This does NOT send an AboutToChange() and CHanged() message to the lib

inputs
   PCMIFLObject   *ppo - Pointer to an array of PCMIFLObject which are the objects to merge into this
   DWORD          *padwLibOrig - Pointer to an array of lib m_dwTempID where the objects came from
   DWORD          dwNum - Number of objects in ppo
   BOOL           fKeepDocs - Set to TRUE if should keep the docs in the object
   PCMIFLErrors   pErr - Where to add the errors to. THis can be NULLf
returns
   BOOL - TRUE if success
*/
BOOL CMIFLObject::Merge (PCMIFLObject *ppo, DWORD *padwLibOrig, DWORD dwNum, BOOL fKeepDocs, PCMIFLErrors pErr)
{
   // make a list of PWSTR that can be used for the compares
   CMem memsz;
   if (!memsz.Required (dwNum * sizeof(PWSTR)))
      return FALSE;
   PWSTR *ppsz = (PWSTR*) memsz.p;

   // clear this one out, save the name first
   Clear ();
   MemZero (&m_memName);
   MemCat (&m_memName, (PWSTR) ppo[0]->m_memName.p);  // take any name
   m_dwTempID = ppo[0]->m_dwTempID; // BUGFIX - Make sure to keep original tempID
   m_dwLibOrigFrom = ppo[0]->m_dwLibOrigFrom;   // BUGFIX - keeo lib orig

   // transfer over description and help stuff...
   DWORD i, j;
   for (i = 0; i < dwNum; i++)
      if (((PWSTR)ppo[i]->m_memDescShort.p)[0]) {
         MemCat (&m_memDescShort, (PWSTR)ppo[i]->m_memDescShort.p);
         break;
      }
   for (i = 0; i < dwNum; i++)
      if (((PWSTR)ppo[i]->m_memDescLong.p)[0]) {
         MemCat (&m_memDescLong, (PWSTR)ppo[i]->m_memDescLong.p);
         break;
      }
   for (j = 0; j < 2; j++) for (i = 0; i < dwNum; i++)
      if (((PWSTR)ppo[i]->m_aMemHelp[j].p)[0]) {
         MemCat (&m_aMemHelp[j], (PWSTR)ppo[i]->m_aMemHelp[j].p);
         break;
      }
   for (i = 0; i < dwNum; i++)
      if (((PWSTR)ppo[i]->m_memContained.p)[0]) {
         MemCat (&m_memContained, (PWSTR)ppo[i]->m_memContained.p);
         break;
      }
   m_fAutoCreate = FALSE;
   for (i = 0; i < dwNum; i++)
      if (ppo[i]->m_fAutoCreate) {
         m_fAutoCreate = TRUE;
         break;
      }
   m_dwInNewObjectMenu = 0;
   for (i = 0; i < dwNum; i++)
      if (ppo[i]->m_dwInNewObjectMenu) {
         m_dwInNewObjectMenu = ppo[i]->m_dwInNewObjectMenu;
         break;
      }

   // merge GUIDs
   m_gID = GUID_NULL;
   for (i = 0; i < dwNum; i++) {
      // BUGFIX - If this GUID is not marked as auto-create then don't bother
      // merging it in
      if (!ppo[i]->m_fAutoCreate)
         continue;

      if (IsEqualGUID(ppo[i]->m_gID, GUID_NULL))
         continue; // nothing

      // else, see if current is null
      if (IsEqualGUID(m_gID, GUID_NULL)) {
         m_gID = ppo[i]->m_gID;
         continue;
      }

      // else, see if different
      if (!IsEqualGUID(m_gID, ppo[i]->m_gID) && pErr) {
         pErr->Add (L"The object replaces an object in a lower priority library but it has a different ID "
            L"than the original object.", padwLibOrig[i], ppo[i], FALSE);
      }
   }

   // look through private properties and take first ones
   CBTree pt, ptThis;
   pt.m_fIgnoreCase = TRUE;   // just to make sure
   pt.Clear();
   for (i = 0; i < dwNum; i++) {
      DWORD dw = ppo[i]->m_lPCMIFLPropPriv.Num();
      PCMIFLProp *ppp = (PCMIFLProp*)ppo[i]->m_lPCMIFLPropPriv.Get(0);
      PCMIFLProp pNew;

      // BUGFIX - Keep track of private properties that duplicate in self
      ptThis.Clear();

      for (j = 0; j < dw; j++) {
         // BUGFIX - If it's blank then don't bother
         if (!((PWSTR)ppp[j]->m_memName.p)[0])
            continue;

         // if find in self then note error
         if (ptThis.Find ((PWSTR)ppp[j]->m_memName.p) && pErr) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The private property, ");
            MemCat (&gMemTemp, (PWSTR)ppp[j]->m_memName.p);
            MemCat (&gMemTemp, L", is defined twice in the object.");
            pErr->Add ((PWSTR)gMemTemp.p, padwLibOrig[i], ppo[i], TRUE);
         }

         if (pt.Find ((PWSTR)ppp[j]->m_memName.p))
            continue;   // already exists

         // add to tree
         pt.Add ((PWSTR)ppp[j]->m_memName.p, &i, sizeof(i));
         ptThis.Add ((PWSTR)ppp[j]->m_memName.p, &i, sizeof(i));

         // add to current object
         pNew = ppp[j]->Clone(fKeepDocs);
         if (!pNew)
            return FALSE;
         pNew->m_dwLibOrigFrom = padwLibOrig[i];
         m_lPCMIFLPropPriv.Add (&pNew);
      } // j, all elements in object
   } // i, all objects

   // look through public properties and take first ones
   pt.Clear();
   for (i = 0; i < dwNum; i++) {
      DWORD dw = ppo[i]->m_lPCMIFLPropPub.Num();
      PCMIFLProp *ppp = (PCMIFLProp*)ppo[i]->m_lPCMIFLPropPub.Get(0);
      PCMIFLProp pNew;

      // BUGFIX - Keep track of private properties that duplicate in self
      ptThis.Clear();

      for (j = 0; j < dw; j++) {
         // BUGFIX - If it's blank then don't bother
         if (!((PWSTR)ppp[j]->m_memName.p)[0])
            continue;

         // if find in self then note error
         if (ptThis.Find ((PWSTR)ppp[j]->m_memName.p) && pErr) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The public property, ");
            MemCat (&gMemTemp, (PWSTR)ppp[j]->m_memName.p);
            MemCat (&gMemTemp, L", is defined twice in the object.");
            pErr->Add ((PWSTR)gMemTemp.p, padwLibOrig[i], ppo[i], TRUE);
         }

         if (pt.Find ((PWSTR)ppp[j]->m_memName.p))
            continue;   // already exists

         // add to tree
         pt.Add ((PWSTR)ppp[j]->m_memName.p, &i, sizeof(i));
         ptThis.Add ((PWSTR)ppp[j]->m_memName.p, &i, sizeof(i));

         // add to current object
         pNew = ppp[j]->Clone(fKeepDocs);
         if (!pNew)
            return FALSE;
         pNew->m_dwLibOrigFrom = padwLibOrig[i];
         m_lPCMIFLPropPub.Add (&pNew);
      } // j, all elements in object
   } // i, all objects

   // look through superclasses and take first ones
   pt.Clear();
   for (i = 0; i < dwNum; i++) {
      DWORD dw = ppo[i]->m_lClassSuper.Num();

      for (j = 0; j < dw; j++) {
         PWSTR psz = (PWSTR) ppo[i]->m_lClassSuper.Get(j);
         if (pt.Find (psz))
            continue;   // already exists

         // add to tree
         pt.Add (psz, &i, sizeof(i));

         // add to current object
         m_lClassSuper.Add (psz, ppo[i]->m_lClassSuper.Size(j));
      } // j, all elements in object
   } // i, all objects


   if (fKeepDocs) {
      pt.Clear();
      for (i = 0; i < dwNum; i++) {
         DWORD dw = ppo[i]->m_lRecMeth.Num();

         for (j = 0; j < dw; j++) {
            PWSTR psz = (PWSTR) ppo[i]->m_lRecMeth.Get(j);
            if (pt.Find (psz))
               continue;   // already exists

            // add to tree
            pt.Add (psz, &i, sizeof(i));

            // add to current object
            m_lRecMeth.Add (psz, ppo[i]->m_lRecMeth.Size(j));
         } // j, all elements in object
      } // i, all objects

      pt.Clear();
      for (i = 0; i < dwNum; i++) {
         DWORD dw = ppo[i]->m_lRecProp.Num();

         for (j = 0; j < dw; j++) {
            PWSTR psz = (PWSTR) ppo[i]->m_lRecProp.Get(j);
            if (pt.Find (psz))
               continue;   // already exists

            // add to tree
            pt.Add (psz, &i, sizeof(i));

            // add to current object
            m_lRecProp.Add (psz, ppo[i]->m_lRecProp.Size(j));
         } // j, all elements in object
      } // i, all objects

   }

   // allocate memory of a list of data types
   CMem memList;
   if (!memList.Required (dwNum * sizeof(PCListFixed) + 2 * dwNum * sizeof(PVOID) + 3 * dwNum * sizeof(DWORD))) {
      return FALSE;
   }
   PCListFixed *paList = (PCListFixed*) memList.p;
   PVOID *papElem = (PVOID*) (paList + dwNum);
   PVOID *papUse = papElem + dwNum;
   DWORD *padwCount = (DWORD*) (papUse + dwNum);
   DWORD *padwCur = padwCount + dwNum;
   DWORD *padwLibFrom = padwCur + dwNum;

   // loop through all the types of data
   DWORD dwType, dwLib;
   for (dwType = 0; dwType < 2; dwType++) {
      // get pointers to all lists
      for (dwLib = 0; dwLib < dwNum; dwLib++) {
         switch (dwType) {
         case 0:  // methpriv
            paList[dwLib] = &(ppo[dwLib]->m_lPCMIFLMethPriv);
            break;
         case 1:  // methpub
            paList[dwLib] = &(ppo[dwLib]->m_lPCMIFLMethPub);
            break;
         } // switch

         // fill in the count and other points
         papElem[dwLib] = paList[dwLib]->Get(0);
         padwCount[dwLib] = paList[dwLib]->Num();
      } // dwLib

      // repeat as long as can, adding elements...
      memset (padwCur, 0, sizeof(DWORD)*dwNum);
      while (TRUE) {
         // fill in strings...
         for (dwLib = 0; dwLib < dwNum; dwLib++) {
            if (padwCur[dwLib] >= padwCount[dwLib]) {
               ppsz[dwLib] = NULL;  // past end of list
               continue;
            }

            // else, get string
            PWSTR psz;
            switch (dwType) {
            case 0:  // methpriv
               psz = (PWSTR) ((PCMIFLFunc*) papElem[dwLib])[padwCur[dwLib]]->m_Meth.m_memName.p;
               break;
            case 1:  // methpub
               psz = (PWSTR) ((PCMIFLFunc*) papElem[dwLib])[padwCur[dwLib]]->m_Meth.m_memName.p;
               break;
            } // switch
            ppsz[dwLib] = psz;
         } // dwLib

         // now that have all the strings, find out which is best
         DWORD dwIndex = MIFLStringArrayComp (ppsz, dwNum);
         if (dwIndex == -1)
            break;   // out of elements

         // compact the ones together that need for merge
         DWORD dwFrom = 0, dwTo = 0;
         for (dwFrom = 0; dwFrom < dwNum; dwFrom++) {
            if (!ppsz[dwFrom])
               continue;   // not used

            // else keep
            papUse[dwTo] = ((PCMIFLFunc*)papElem[dwFrom])[padwCur[dwFrom]];
            padwLibFrom[dwTo] = padwLibOrig[dwFrom];
            dwTo++;

            // also, increate the counter so will look elsewhere next time
            padwCur[dwFrom]++;
         } // dwFom
         if (!dwTo)
            break;   // shouldnt happen

         // merge the current item into the library
         PCMIFLFunc pFunc;
         switch (dwType) {
         case 0:  // methpriv
            pFunc = (PCMIFLFunc) papUse[0];
            if (!(pFunc = pFunc->Clone(fKeepDocs)))
               return FALSE;
            pFunc->m_Meth.m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLMethPriv.Add (&pFunc);
            break;
         case 1:  // methpub
            pFunc = (PCMIFLFunc) papUse[0];
            if (!(pFunc = pFunc->Clone(fKeepDocs)))
               return FALSE;
            pFunc->m_Meth.m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLMethPub.Add (&pFunc);
            break;
         } // switch
      } // while TRUE

      // if got here done with the merging
   } // dwType


   // changed notification
   return TRUE;
}



/********************************************************************************
CMIFLObject::KillDead - Loops through all the objects in the library and a) kills dead
parameters, and b) kills resources/strings without any info.

inputs
   PCMIFLLib         pLib - Library to use. Can be NULL.
   BOOL              fLowerCase - If TRUE this lowercases all the names
*/
void CMIFLObject::KillDead (PCMIFLLib pLib, BOOL fLowerCase)
{
   if (pLib)
      pLib->AboutToChange();

   // lower case own name
   if (fLowerCase) {
      MIFLToLower ((PWSTR)m_memName.p);
      MIFLToLower ((PWSTR)m_memContained.p);
   }

   // supert classes
   DWORD i;
   if (fLowerCase) for (i = 0; i < m_lClassSuper.Num(); i++)
      MIFLToLower ((PWSTR)m_lClassSuper.Get(i));

   // methds
   for (i = 0; i < MethPubNum(); i++) {
      PCMIFLFunc pMeth = MethPubGet(i);
      pMeth->KillDead (NULL, fLowerCase);
   } // i
   for (i = 0; i < MethPrivNum(); i++) {
      PCMIFLFunc pMeth = MethPrivGet(i);
      pMeth->KillDead (NULL, fLowerCase);
   } // i


   // properties...
   DWORD dwPriv;
   for (dwPriv = 0; dwPriv < 2; dwPriv++) {
      PCListFixed pl = dwPriv ? &m_lPCMIFLPropPriv : &m_lPCMIFLPropPub;
      PCMIFLProp *ppp = (PCMIFLProp*)pl->Get(0);
      for (i = pl->Num()-1; i < pl->Num(); i--) {
         PWSTR psz = (PWSTR)ppp[i]->m_memName.p;
         if (!psz[0]) {
            // delete this
            delete ppp[i];
            pl->Remove (i);
            ppp = (PCMIFLProp*)pl->Get(0);
            continue;
         }

      // pass on
      if (fLowerCase)
         MIFLToLower ((PWSTR)ppp[i]->m_memName.p);
      } // i
   } // dwPriv


   if (pLib)
      pLib->Changed();
}

