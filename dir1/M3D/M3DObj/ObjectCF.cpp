/*******************************************************************************8
ObjectCF.cpp - Functions for creating and enumerating objects.

begun 6/11/2001 by Mike Rozak
Copyright Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#ifdef _DEBUG
// BUGBUG - Defined
#define WORKONOBJECTS      // enable if want to modify built-in Objects
#endif



typedef PCObjectSocket (*PObjectCreator)(PVOID pParams);

typedef struct {
   WCHAR       szMajor[128];   // major category name
   WCHAR       szMinor[128];   // minor category name
   WCHAR       szName[128];    // name
   HBITMAP     hBit;       // bitmap of the image - 100 x 100
   WCHAR       szCurMajorCategory[128]; // current major category shown
   WCHAR       szCurMinorCategory[128]; // current minor cateogyr shown
   BOOL        fPressedOK; // set to TRUE if press OK
   GUID        gCode;      // object code
   GUID        gSub;       // object sub-code
   PCMMLNode2   pNode;      // node being edited, used for some dialogs
   DWORD       dwRenderShard; // render shard
} TOSPAGE, *PTOSPAGE;

static PWSTR gpszObjectByID = L"ObjectByID";    // use this in MML to indicate that pass ID into creator only
static PWSTR gpszObject = L"Object";

//static CBTree gtreeObjects;      // each node contains a major-category name and pointer to a tree
                                 // each of those contains a minor-category name and pointer to another tree
                                 // each of those contains a pointer to a fixed list of POBJECTRECORD





/****************************************************************************
UniqueObjectName - Given a name, comes up with a unique one that wont
conflict with other Objects.

inputs
   PWSTR       pszMajor, pszMinor - Categories
   PWSTR       pszName - Initially filled with name, and then modified
   PCLibrary   pLibrary - Alternate user library. Can be NULL to use default user library
returns
   none
*/
void UniqueObjectName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, PCLibrary pLibrary = NULL)
{
   // which category object
   // BUGFIX - Allow to come from different library
   PCLibraryCategory plc = pLibrary ?
      pLibrary->CategoryGet (L"Objects" /*gpszObjects*/) :
      LibraryObjects(dwRenderShard, TRUE);

   DWORD i;
   for (i = 0; ; i++) {
      if (i) {
         PWSTR pszCur;
         // remove numbers from end
         for (pszCur = pszName  + (wcslen(pszName)-1); (pszCur >= pszName) && (*pszCur >= L'0') && (*pszCur <= L'9'); pszCur--)
            pszCur[0] = 0;

         // remove spaces
         for (pszCur = pszName  + (wcslen(pszName)-1); (pszCur >= pszName) && (*pszCur == L' '); pszCur--)
            pszCur[0] = 0;

         // append space and number
         swprintf (pszName + wcslen(pszName), L" %d", (int) i+1);
      }

      // if it matches try again
      // BUGFIX - Call IntItemGet() since only care if exists; dont need to load
      if (LibraryObjects(dwRenderShard, FALSE)->ItemExists(pszMajor, pszMinor, pszName))
         continue;
      if (plc->ItemExists(pszMajor, pszMinor, pszName))
         continue;

      // no match
      return;
   }
}


#if 0 // No longer used since getting info from file
/*************************************************************************************
Globla with objects */

// major category strings
static PWSTR gszMCFurniture = L"Furniture";
static PWSTR gszMCStructure = L"(Basic structure)";
static PWSTR gszMCOpenings = L"Openings";
static PWSTR gszMCTest = L"Test";
static PWSTR gszMCLandscaping = L"Outdoors";
static PWSTR gszMCBuildBlock = L"Building blocks";
static PWSTR gszMCBasicShapes = L"Basic shapes";
static PWSTR gszMCDoors = L"Doors";
static PWSTR gszMCWindows = L"Windows";
static PWSTR gszMCPlants = L"Plants";
static PWSTR gszMCLighting = L"Lighting";

// sub-category strings
static PWSTR gszSCFurnitureTableware = L"Tableware";
static PWSTR gszSCStructureWallInternal = L"Walls (internal)";
static PWSTR gszSCStructureWallExternal = L"Walls (external)";
static PWSTR gszSCStructureRoof = L"Roofing";
static PWSTR gszSCStructureMisc = L"Misc";
static PWSTR gszSCFlooring = L"Flooring";
static PWSTR gszSCPads = L"Cement pads";
static PWSTR gszSCDropCeilings = L"Drop ceilings";
static PWSTR gszSCStructureUniHole = L"Universal holes";
static PWSTR gszSCWindows = L"Windows";
static PWSTR gszSCDoors = L"Doors";
static PWSTR gszSCDoorways = L"Doorways";
static PWSTR gszSCGround = L"Topography";
static PWSTR gszSCHouseBlocks = L"House blocks (common)";
static PWSTR gszSCHouseBlocksUn = L"House blocks (unusual)";
static PWSTR gszSCDormers = L"Dormers";
static PWSTR gszSCBays = L"Bays";
static PWSTR gszSCBayWindows = L"Bay windows";
static PWSTR gszSCVerandahs = L"Verandas (common)";
static PWSTR gszSCVerandahsUn = L"Verandas (unusual)";
static PWSTR gszSCDecks = L"Decks";
static PWSTR gszSCWalkways = L"Elevated walkways";
static PWSTR gszSCWalls = L"Walls";
static PWSTR gszSCExtrusions = L"Extrusions";
static PWSTR gszSCBalustrades = L"Balustrades";
static PWSTR gszSCFencing = L"Fencing";
static PWSTR gszSCPiers = L"Piers";
static PWSTR gszSCStairs = L"Staircases";
static PWSTR gszSCEntry = L"Entry";
static PWSTR gszSCInternal = L"Internal hinged";
static PWSTR gszSCGarage = L"Garage";
static PWSTR gszSCUnusual = L"Unusual";
static PWSTR gszSCPocket = L"Pocket";
static PWSTR gszSCSliding = L"Sliding";
static PWSTR gszSCBifold = L"Bifold";
static PWSTR gszSCCommercial = L"Commercial";
static PWSTR gszSCHung = L"Hung";
static PWSTR gszSCCasement = L"Casement";
static PWSTR gszSCAwning = L"Awning";
static PWSTR gszSCLouver = L"Louver";
static PWSTR gszSCFixed = L"Fixed";
static PWSTR gszSCTrees = L"Trees";
static PWSTR gszSCConifers = L"Evergreens, conifer";
static PWSTR gszSCPalms = L"Palms";
static PWSTR gszSCEverLeaf = L"Evergreens, leafy";
static PWSTR gszSCDeciduous = L"Deciduous";
static PWSTR gszSCMisc = L"Miscellaneous";
static PWSTR gszSCShrubs = L"Shrubs";
static PWSTR gszSCGroundCover = L"Ground cover";
static PWSTR gszSCPot = L"Potted";
static PWSTR gszSCWall = L"Wall lights";
static PWSTR gszSCCeiling = L"Ceiling lights";
static PWSTR gszSCHanging = L"Hanging lights";
static PWSTR gszSCDesk = L"Desk lamps";
static PWSTR gszSCTable = L"Table lamps";
static PWSTR gszSCFloor = L"Floor lamps";
static PWSTR gszSCOutdoor = L"Outdoor lighting";
static PWSTR gszSCFire = L"Fire";
static PWSTR gszSCLightbulbs = L"Lightbulbs";

// OBJECTRECORD - Global array of these to indicate what objects
// are supported by ASP. In the future, might read from a file.
typedef struct {
   PWSTR       pszMajor;   // name of the main category
   PWSTR       pszMinor;   // name of the minor category
   PWSTR       pszName;    // name of the object
   const GUID  *pCode;     // identifying guid
   const GUID  *pSub;      // identifying guid
   PObjectCreator pFunc;   // function to create
   PVOID       pParams;    // passed into pFunc
} OBJECTRECORD, *POBJECTRECORD;

/**************************************************************************************
ObjectCreators */
PCObjectSocket OCTeapot (PVOID pParams)
{
   return new CObjectTeapot (pParams);
}
PCObjectSocket OCJaw (PVOID pParams)
{
   return new CObjectJaw (pParams);
}
PCObjectSocket OCTooth (PVOID pParams)
{
   return new CObjectTooth (pParams);
}
PCObjectSocket OCHairLock (PVOID pParams)
{
   return new CObjectHairLock (pParams);
}
PCObjectSocket OCHairHead (PVOID pParams)
{
   return new CObjectHairHead (pParams);
}
PCObjectSocket OCEye (PVOID pParams)
{
   return new CObjectEye (pParams);
}
PCObjectSocket OCBranch (PVOID pParams)
{
   return new CObjectBranch (pParams);
}
PCObjectSocket OCLeaf (PVOID pParams)
{
   return new CObjectLeaf (pParams);
}
PCObjectSocket OCSkydome (PVOID pParams)
{
   return new CObjectSkydome (pParams);
}
PCObjectSocket OCAnimCamera (PVOID pParams)
{
   return new CObjectAnimCamera (pParams);
}
PCObjectSocket OCBone (PVOID pParams)
{
   return new CObjectBone (pParams);
}
PCObjectSocket OCLight (PVOID pParams)
{
   return new CObjectLight (pParams);
}
PCObjectSocket OCLightGeneric (PVOID pParams)
{
   return new CObjectLightGeneric (pParams);
}
PCObjectSocket OCNoodle (PVOID pParams)
{
   return new CObjectNoodle (pParams);
}
PCObjectSocket OCColumn (PVOID pParams)
{
   return new CObjectColumn (pParams);
}
PCObjectSocket OCTestBox (PVOID pParams)
{
   return new CObjectTestBox (pParams);
}

#if 0 // DEAD code
PCObjectSocket OCTree (PVOID pParams)
{
   return new CObjectTree (pParams);
}
#endif // 0

PCObjectSocket OCStructSurface (PVOID pParams)
{
   return new CObjectStructSurface (pParams);
}
PCObjectSocket OCBalustrade (PVOID pParams)
{
   return new CObjectBalustrade (pParams);
}
PCObjectSocket OCStairs (PVOID pParams)
{
   return new CObjectStairs (pParams);
}
PCObjectSocket OCPiers (PVOID pParams)
{
   return new CObjectPiers (pParams);
}
PCObjectSocket OCBuildBlock (PVOID pParams)
{
   return new CObjectBuildBlock (pParams);
}
PCObjectSocket OCDoor (PVOID pParams)
{
   return new CObjectDoor (pParams);
}
PCObjectSocket OCCamera (PVOID pParams)
{
   return new CObjectCamera (pParams);
}
PCObjectSocket OCUniHole (PVOID pParams)
{
   return new CObjectUniHole (pParams);
}
PCObjectSocket OCGround (PVOID pParams)
{
   return new CObjectGround (pParams);
}


static OBJECTRECORD gaObjectRecord[] = {
   gszMCBuildBlock, gszSCHouseBlocks, L"Hip roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHipHalf,
      OCBuildBlock, (PVOID) (0x10000 | 0),
   gszMCBuildBlock, gszSCHouseBlocks, L"Hip roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHipPeak,
      OCBuildBlock, (PVOID) (0x10000 | 1),
   gszMCBuildBlock, gszSCHouseBlocks, L"Hip roof, ridged",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHipRidge,
      OCBuildBlock, (PVOID) (0x10000 | 2),
   gszMCBuildBlock, gszSCHouseBlocks, L"Outback roof, style 1",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildOutback1,
      OCBuildBlock, (PVOID) (0x10000 | 3),
   gszMCBuildBlock, gszSCHouseBlocks, L"Saltbox roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildSaltBox,
      OCBuildBlock, (PVOID) (0x10000 | 4),
   gszMCBuildBlock, gszSCHouseBlocks, L"Gable roof, three",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildGableThree,
      OCBuildBlock, (PVOID) (0x10000 | 5),
   gszMCBuildBlock, gszSCHouseBlocks, L"Shed roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildShed,
      OCBuildBlock, (PVOID) (0x10000 | 6),
   gszMCBuildBlock, gszSCHouseBlocks, L"Gable roof, four",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildGableFour,
      OCBuildBlock, (PVOID) (0x10000 | 7),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Hip roof, half, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHalfHipCurved,
      OCBuildBlock, (PVOID) (0x10000 | 109),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Shed roof, folded",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildShedFolded,
      OCBuildBlock, (PVOID) (0x10000 | 8),
   gszMCBuildBlock, gszSCHouseBlocks, L"Shed roof, peaked hip",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHipShedPeak,
      OCBuildBlock, (PVOID) (0x10000 | 9),
   gszMCBuildBlock, gszSCHouseBlocks, L"Shed roof, ridged hip",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHipShedRidge,
      OCBuildBlock, (PVOID) (0x10000 | 10),
   gszMCBuildBlock, gszSCHouseBlocks, L"Monsard roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildMonsard,
      OCBuildBlock, (PVOID) (0x10000 | 11),
   gszMCBuildBlock, gszSCHouseBlocks, L"Gambrel roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildGambrel,
      OCBuildBlock, (PVOID) (0x10000 | 12),
   gszMCBuildBlock, gszSCHouseBlocks, L"Gull-wing roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildGullwing,
      OCBuildBlock, (PVOID) (0x10000 | 13),
   gszMCBuildBlock, gszSCHouseBlocks, L"Outback roof, style 2",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildOutback2,
      OCBuildBlock, (PVOID) (0x10000 | 14),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Balinese roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildBalinese,
      OCBuildBlock, (PVOID) (0x10000 | 15),
   gszMCBuildBlock, gszSCHouseBlocks, L"Flat roof, overhanging",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildFlat,
      OCBuildBlock, (PVOID) (0x10000 | 16),
   gszMCBuildBlock, gszSCHouseBlocks, L"Flat roof, floor",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildNone,
      OCBuildBlock, (PVOID) (0x10000 | 17),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Triangle roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildTrianglePeak,
      OCBuildBlock, (PVOID) (0x10000 | 18),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Pentagon roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildPentagonPeak,
      OCBuildBlock, (PVOID) (0x10000 | 19),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Hexagon roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHexagonPeak,
      OCBuildBlock, (PVOID) (0x10000 | 20),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Hip, half skewed",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHalfHipSkew,
      OCBuildBlock, (PVOID) (0x10000 | 21),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Shed, skewed",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildShedSkew,
      OCBuildBlock, (PVOID) (0x10000 | 22),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Shed, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildShedCurved,
      OCBuildBlock, (PVOID) (0x10000 | 100),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Balinese, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildBalineseCurved,
      OCBuildBlock, (PVOID) (0x10000 | 101),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Cone roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildCone,
      OCBuildBlock, (PVOID) (0x10000 | 102),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Cone roof, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildConeCurved,
      OCBuildBlock, (PVOID) (0x10000 | 103),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Hip roof, half with curved at ridge",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHalfHipLoop,
      OCBuildBlock, (PVOID) (0x10000 | 104),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Hemisphere roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHemisphere,
      OCBuildBlock, (PVOID) (0x10000 | 105),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Hemisphere roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildHemisphereHalf,
      OCBuildBlock, (PVOID) (0x10000 | 106),
   gszMCBuildBlock, gszSCHouseBlocksUn, L"Gull-wing roof, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockBuildGullWingCurved,
      OCBuildBlock, (PVOID) (0x10000 | 107),

   gszMCBuildBlock, gszSCWalkways, L"Hip roof, half (enclosed)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayEnclosedHipHalf,
      OCBuildBlock, (PVOID) (0x70000 | 0),
   gszMCBuildBlock, gszSCWalkways, L"Saltbox roof (enclosed)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayEnclosedSaltBox,
      OCBuildBlock, (PVOID) (0x70000 | 4),
   gszMCBuildBlock, gszSCWalkways, L"Shed roof (enclosed)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayEnclosedShed,
      OCBuildBlock, (PVOID) (0x70000 | 6),
   gszMCBuildBlock, gszSCWalkways, L"Flat roof, overhanging (enclosed)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayEnclosedFlat,
      OCBuildBlock, (PVOID) (0x70000 | 16),
   gszMCBuildBlock, gszSCWalkways, L"Shed, curved (enclosed)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayEnclosedShedCurved,
      OCBuildBlock, (PVOID) (0x70000 | 100),
   gszMCBuildBlock, gszSCWalkways, L"Hip roof, half, curved (enclosed)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayEnclosedHalfHipCurved,
      OCBuildBlock, (PVOID) (0x70000 | 109),

   gszMCBuildBlock, gszSCWalkways, L"Hip roof, half (open)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayOpenHipHalf,
      OCBuildBlock, (PVOID) (0x80000 | 0),
   gszMCBuildBlock, gszSCWalkways, L"Saltbox roof (open)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayOpenSaltBox,
      OCBuildBlock, (PVOID) (0x80000 | 4),
   gszMCBuildBlock, gszSCWalkways, L"Shed roof (open)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayOpenShed,
      OCBuildBlock, (PVOID) (0x80000 | 6),
   gszMCBuildBlock, gszSCWalkways, L"Flat roof, overhanging (open)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayOpenFlat,
      OCBuildBlock, (PVOID) (0x80000 | 16),
   gszMCBuildBlock, gszSCWalkways, L"Shed, curved (open)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayOpenShedCurved,
      OCBuildBlock, (PVOID) (0x80000 | 100),
   gszMCBuildBlock, gszSCWalkways, L"Hip roof, half, curved (open)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayOpenHalfHipCurved,
      OCBuildBlock, (PVOID) (0x80000 | 109),
   gszMCBuildBlock, gszSCWalkways, L"No roof (open)",
      &CLSID_BuildBlock, &CLSID_BuildBlockWalkwayOpenNone,
      OCBuildBlock, (PVOID) (0x80000 | 17),


   gszMCBuildBlock, gszSCVerandahs, L"Hip roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHipHalf,
      OCBuildBlock, (PVOID) (0x50000 | 0),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hip roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHipPeak,
      OCBuildBlock, (PVOID) (0x50000 | 1),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hip roof, ridged",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHipRidge,
      OCBuildBlock, (PVOID) (0x50000 | 2),
   gszMCBuildBlock, gszSCVerandahsUn, L"Outback roof, style 1",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahOutback1,
      OCBuildBlock, (PVOID) (0x50000 | 3),
   gszMCBuildBlock, gszSCVerandahsUn, L"Saltbox roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahSaltBox,
      OCBuildBlock, (PVOID) (0x50000 | 4),
   gszMCBuildBlock, gszSCVerandahsUn, L"Gable roof, three",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahGableThree,
      OCBuildBlock, (PVOID) (0x50000 | 5),
   gszMCBuildBlock, gszSCVerandahs, L"Shed roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahShed,
      OCBuildBlock, (PVOID) (0x50000 | 6),
   gszMCBuildBlock, gszSCVerandahsUn, L"Gable roof, four",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahGableFour,
      OCBuildBlock, (PVOID) (0x50000 | 7),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hip roof, half, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHalfHipCurved,
      OCBuildBlock, (PVOID) (0x50000 | 109),
   gszMCBuildBlock, gszSCVerandahsUn, L"Shed roof, folded",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahShedFolded,
      OCBuildBlock, (PVOID) (0x50000 | 8),
   gszMCBuildBlock, gszSCVerandahs, L"Shed roof, peaked hip",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHipShedPeak,
      OCBuildBlock, (PVOID) (0x50000 | 9),
   gszMCBuildBlock, gszSCVerandahs, L"Shed roof, ridged hip",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHipShedRidge,
      OCBuildBlock, (PVOID) (0x50000 | 10),
   gszMCBuildBlock, gszSCVerandahsUn, L"Monsard roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahMonsard,
      OCBuildBlock, (PVOID) (0x50000 | 11),
   gszMCBuildBlock, gszSCVerandahsUn, L"Gambrel roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahGambrel,
      OCBuildBlock, (PVOID) (0x50000 | 12),
   gszMCBuildBlock, gszSCVerandahsUn, L"Gull-wing roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahGullwing,
      OCBuildBlock, (PVOID) (0x50000 | 13),
   gszMCBuildBlock, gszSCVerandahsUn, L"Outback roof, style 2",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahOutback2,
      OCBuildBlock, (PVOID) (0x50000 | 14),
   gszMCBuildBlock, gszSCVerandahsUn, L"Balinese roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahBalinese,
      OCBuildBlock, (PVOID) (0x50000 | 15),
   gszMCBuildBlock, gszSCVerandahs, L"Flat roof, overhanging",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahFlat,
      OCBuildBlock, (PVOID) (0x50000 | 16),
   gszMCBuildBlock, gszSCVerandahsUn, L"Triangle roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahTrianglePeak,
      OCBuildBlock, (PVOID) (0x50000 | 18),
   gszMCBuildBlock, gszSCVerandahsUn, L"Pentagon roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahPentagonPeak,
      OCBuildBlock, (PVOID) (0x50000 | 19),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hexagon roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHexagonPeak,
      OCBuildBlock, (PVOID) (0x50000 | 20),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hip, half skewed",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHalfHipSkew,
      OCBuildBlock, (PVOID) (0x50000 | 21),
   gszMCBuildBlock, gszSCVerandahsUn, L"Shed, skewed",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahShedSkew,
      OCBuildBlock, (PVOID) (0x50000 | 22),
   gszMCBuildBlock, gszSCVerandahsUn, L"Shed, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahShedCurved,
      OCBuildBlock, (PVOID) (0x50000 | 100),
   gszMCBuildBlock, gszSCVerandahsUn, L"Balinese, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahBalineseCurved,
      OCBuildBlock, (PVOID) (0x50000 | 101),
   gszMCBuildBlock, gszSCVerandahsUn, L"Cone roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahCone,
      OCBuildBlock, (PVOID) (0x50000 | 102),
   gszMCBuildBlock, gszSCVerandahsUn, L"Cone roof, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahConeCurved,
      OCBuildBlock, (PVOID) (0x50000 | 103),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hip roof, half with curved at ridge",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHalfHipLoop,
      OCBuildBlock, (PVOID) (0x50000 | 104),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hemisphere roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHemisphere,
      OCBuildBlock, (PVOID) (0x50000 | 105),
   gszMCBuildBlock, gszSCVerandahsUn, L"Hemisphere roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahHemisphereHalf,
      OCBuildBlock, (PVOID) (0x50000 | 106),
   gszMCBuildBlock, gszSCVerandahsUn, L"Gull-wing roof, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockVerandahGullWingCurved,
      OCBuildBlock, (PVOID) (0x50000 | 107),

   gszMCBuildBlock, gszSCDecks, L"Rectangle",
      &CLSID_BuildBlock, &CLSID_BuildBlockDeckRectangle,
      OCBuildBlock, (PVOID) (0x60000 | 0),
   gszMCBuildBlock, gszSCDecks, L"Triangle",
      &CLSID_BuildBlock, &CLSID_BuildBlockDeckTriangle,
      OCBuildBlock, (PVOID) (0x60000 | 18),
   gszMCBuildBlock, gszSCDecks, L"Pentagon",
      &CLSID_BuildBlock, &CLSID_BuildBlockDeckPentagon,
      OCBuildBlock, (PVOID) (0x60000 | 19),
   gszMCBuildBlock, gszSCDecks, L"Hexagon",
      &CLSID_BuildBlock, &CLSID_BuildBlockDeckHexagon,
      OCBuildBlock, (PVOID) (0x60000 | 20),
   gszMCBuildBlock, gszSCDecks, L"Circle",
      &CLSID_BuildBlock, &CLSID_BuildBlockDeckCircle,
      OCBuildBlock, (PVOID) (0x60000 | 102),
   gszMCBuildBlock, gszSCDecks, L"Circle, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockDeckCircleHalf,
      OCBuildBlock, (PVOID) (0x60000 | 106),

      gszMCBuildBlock, gszSCDormers, L"Hip roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockDormerHipHalf,
      OCBuildBlock, (PVOID) (0x20000 | 0),
   gszMCBuildBlock, gszSCDormers, L"Shed roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockDormerShed,
      OCBuildBlock, (PVOID) (0x20000 | 6),
   gszMCBuildBlock, gszSCDormers, L"Hip roof, half, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockDormerHalfHipCurved,
      OCBuildBlock, (PVOID) (0x20000 | 109),
   gszMCBuildBlock, gszSCDormers, L"Shed roof, ridged hip",
      &CLSID_BuildBlock, &CLSID_BuildBlockDormerHipShedRidge,
      OCBuildBlock, (PVOID) (0x20000 | 10),
   gszMCBuildBlock, gszSCDormers, L"Flat roof, overhanging",
      &CLSID_BuildBlock, &CLSID_BuildBlockDormerFlat,
      OCBuildBlock, (PVOID) (0x20000 | 16),

   gszMCBuildBlock, gszSCBays, L"Hip roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayHipHalf,
      OCBuildBlock, (PVOID) (0x30000 | 0),
   gszMCBuildBlock, gszSCBays, L"Shed roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayShed,
      OCBuildBlock, (PVOID) (0x30000 | 6),
   gszMCBuildBlock, gszSCBays, L"Hip roof, half, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayHalfHipCurved,
      OCBuildBlock, (PVOID) (0x30000 | 109),
   gszMCBuildBlock, gszSCBays, L"Shed roof, ridged hip",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayHipShedRidge,
      OCBuildBlock, (PVOID) (0x30000 | 10),
   gszMCBuildBlock, gszSCBays, L"Flat roof, overhanging",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayFlat,
      OCBuildBlock, (PVOID) (0x30000 | 16),
   gszMCBuildBlock, gszSCBays, L"Flat roof, floor",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayNone,
      OCBuildBlock, (PVOID) (0x30000 | 17),
   gszMCBuildBlock, gszSCBays, L"Hexagon roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayHexagonPeak,
      OCBuildBlock, (PVOID) (0x30000 | 20),
   gszMCBuildBlock, gszSCBays, L"Hemisphere roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayHemisphereHalf,
      OCBuildBlock, (PVOID) (0x30000 | 106),

   gszMCBuildBlock, gszSCBayWindows, L"Hip roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayWindowHipHalf,
      OCBuildBlock, (PVOID) (0x40000 | 0),
   gszMCBuildBlock, gszSCBayWindows, L"Shed roof",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayWindowShed,
      OCBuildBlock, (PVOID) (0x40000 | 6),
   gszMCBuildBlock, gszSCBayWindows, L"Hip roof, half, curved",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayWindowHalfHipCurved,
      OCBuildBlock, (PVOID) (0x40000 | 109),
   gszMCBuildBlock, gszSCBayWindows, L"Shed roof, ridged hip",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayWindowHipShedRidge,
      OCBuildBlock, (PVOID) (0x40000 | 10),
   gszMCBuildBlock, gszSCBayWindows, L"Flat roof, overhanging",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayWindowFlat,
      OCBuildBlock, (PVOID) (0x40000 | 16),
   gszMCBuildBlock, gszSCBayWindows, L"Hexagon roof, peaked",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayWindowHexagonPeak,
      OCBuildBlock, (PVOID) (0x40000 | 20),
   gszMCBuildBlock, gszSCBayWindows, L"Hemisphere roof, half",
      &CLSID_BuildBlock, &CLSID_BuildBlockBayWindowHemisphereHalf,
      OCBuildBlock, (PVOID) (0x40000 | 106),

   gszMCBuildBlock, gszSCWalls, L"Stud wall",
      &CLSID_StructSurface, &CLSID_StructSurfaceInternalStudWall,
      OCStructSurface, (PVOID) 0x10001,
   gszMCBuildBlock, gszSCWalls, L"Block wall",
      &CLSID_StructSurface, &CLSID_StructSurfaceInternalBlockWall,
      OCStructSurface, (PVOID) 0x10005,

   gszMCStructure, gszSCStructureWallInternal, L"Stud wall",
      &CLSID_StructSurface, &CLSID_StructSurfaceInternalStudWall,
      OCStructSurface, (PVOID) 0x10001,
   gszMCStructure, gszSCStructureWallExternal, L"Stud wall",
      &CLSID_StructSurface, &CLSID_StructSurfaceExternalStudWall,
      OCStructSurface, (PVOID) 0x10002,
   gszMCStructure, gszSCStructureWallInternal, L"Stud wall (curved)",
      &CLSID_StructSurface, &CLSID_StructSurfaceInternalStudWallCurved,
      OCStructSurface, (PVOID) 0x10003,
   gszMCStructure, gszSCStructureWallExternal, L"Stud wall (curved)",
      &CLSID_StructSurface, &CLSID_StructSurfaceExternalStudWallCurved,
      OCStructSurface, (PVOID) 0x10004,
   gszMCStructure, gszSCStructureWallInternal, L"Block wall",
      &CLSID_StructSurface, &CLSID_StructSurfaceInternalBlockWall,
      OCStructSurface, (PVOID) 0x10005,
   gszMCStructure, gszSCStructureWallExternal, L"Block wall",
      &CLSID_StructSurface, &CLSID_StructSurfaceExternalBlockWall,
      OCStructSurface, (PVOID) 0x10006,
   gszMCStructure, gszSCStructureWallInternal, L"Block wall (curved)",
      &CLSID_StructSurface, &CLSID_StructSurfaceInternalBlockWallCurved,
      OCStructSurface, (PVOID) 0x10007,
   gszMCStructure, gszSCStructureWallExternal, L"Block wall (curved)",
      &CLSID_StructSurface, &CLSID_StructSurfaceExternalBlockWallCurved,
      OCStructSurface, (PVOID) 0x10008,
   gszMCStructure, gszSCStructureWallExternal, L"External wall from location info",
      &CLSID_StructSurface, &CLSID_StructSurfaceExternalWallOption,
      OCStructSurface, (PVOID) 0x1000A,
   gszMCStructure, gszSCStructureWallExternal, L"Skirting around piers",
      &CLSID_StructSurface, &CLSID_StructSurfaceExternalWallSkirting,
      OCStructSurface, (PVOID) 0x1000C,


   gszMCStructure, gszSCStructureRoof, L"Flat roof section",
      &CLSID_StructSurface, &CLSID_StructSurfaceRoofFlat,
      OCStructSurface, (PVOID) 0x20001,
   gszMCStructure, gszSCStructureRoof, L"Curved roof section",
      &CLSID_StructSurface, &CLSID_StructSurfaceRoofCurved,
      OCStructSurface, (PVOID) 0x20002,
   gszMCStructure, gszSCStructureRoof, L"Half curved-roof section",
      &CLSID_StructSurface, &CLSID_StructSurfaceRoofCurvedPartial,
      OCStructSurface, (PVOID) 0x20003,

   gszMCStructure, gszSCPads, L"Cement pad, tiled",
      &CLSID_StructSurface, &CLSID_StructSurfaceFloorPadTiles,
      OCStructSurface, (PVOID) 0x30001,
   gszMCStructure, gszSCPads, L"Cement pad, plain",
      &CLSID_StructSurface, &CLSID_StructSurfaceFloorPadPlain,
      OCStructSurface, (PVOID) 0x30004,
   gszMCStructure, gszSCFlooring, L"Floor",
      &CLSID_StructSurface, &CLSID_StructSurfaceFloorFloor,
      OCStructSurface, (PVOID) 0x30002,
   gszMCStructure, gszSCDropCeilings, L"Drop ceiling",
      &CLSID_StructSurface, &CLSID_StructSurfaceFloorDropCeiling,
      OCStructSurface, (PVOID) 0x30003,
   gszMCStructure, gszSCFlooring, L"Deck",
      &CLSID_StructSurface, &CLSID_StructSurfaceFloorDeck,
      OCStructSurface, (PVOID) 0x30005,

   // entry doors
   gszMCDoors, gszSCEntry, L"Glass",
      &CLSID_Door, &CLSID_DoorEntry3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS1),
   gszMCDoors, gszSCEntry, L"Screen",
      &CLSID_Door, &CLSID_DoorEntry2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORSCREEN),
   gszMCDoors, gszSCEntry, L"Glass with lites",
      &CLSID_Door, &CLSID_DoorEntry4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS2),
   gszMCDoors, gszSCEntry, L"Glass, half arch",
      &CLSID_Door, &CLSID_DoorEntry5,
      OCDoor, (PVOID) ((DFS_ARCHHALF << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS3),
   gszMCDoors, gszSCEntry, L"Glass, large lites 1",
      &CLSID_Door, &CLSID_DoorEntry6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS4),
   gszMCDoors, gszSCEntry, L"Glass, large lites 2",
      &CLSID_Door, &CLSID_DoorEntry7,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS5),
   gszMCDoors, gszSCEntry, L"Japanese",
      &CLSID_Door, &CLSID_DoorEntry8,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKLR | DS_DOORGLASS6),
   gszMCDoors, gszSCEntry, L"Glass, large lites 2, double",
      &CLSID_Door, &CLSID_DoorEntry9,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_DOORGLASS5),
   gszMCDoors, gszSCEntry, L"Oval",
      &CLSID_Door, &CLSID_DoorEntry10,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS8),
   gszMCDoors, gszSCEntry, L"Eight panels, side windows",
      &CLSID_Door, &CLSID_DoorEntry11,
      OCDoor, (PVOID) ((DFS_ARCHLRLITES << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL6),
   gszMCDoors, gszSCEntry, L"Semi-circle, side windows",
      &CLSID_Door, &CLSID_DoorEntry12,
      OCDoor, (PVOID) ((DFS_RECTANGLELRLITES << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS10),
   gszMCDoors, gszSCEntry, L"Glass, thin",
      &CLSID_Door, &CLSID_DoorEntry13,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS11),
   gszMCDoors, gszSCEntry, L"Small oval, side windows",
      &CLSID_Door, &CLSID_DoorEntry14,
      OCDoor, (PVOID) ((DFS_ARCHLRLITES3 << 16) | DSSTYLE_HINGEL2 | DS_DOORPG4),
   gszMCDoors, gszSCEntry, L"Glass, fancy",
      &CLSID_Door, &CLSID_DoorEntry15,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGLASS13),
   gszMCDoors, gszSCEntry, L"Single panel, vent window",
      &CLSID_Door, &CLSID_DoorEntry16,
      OCDoor, (PVOID) ((DFS_RECTANGLEVENT << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL1),
   gszMCDoors, gszSCEntry, L"Double panel, in arch",
      &CLSID_Door, &CLSID_DoorEntry17,
      OCDoor, (PVOID) ((DFS_ARCHSPLIT << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL2),
   gszMCDoors, gszSCEntry, L"Tripple panel 1",
      &CLSID_Door, &CLSID_DoorEntry18,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL3),
   gszMCDoors, gszSCEntry, L"Tripple panel 2",
      &CLSID_Door, &CLSID_DoorEntry19,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL4),
   gszMCDoors, gszSCEntry, L"Quadruple panel",
      &CLSID_Door, &CLSID_DoorEntry20,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL5),
   gszMCDoors, gszSCEntry, L"Eight panels",
      &CLSID_Door, &CLSID_DoorEntry21,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL6),
   gszMCDoors, gszSCEntry, L"Arched door",
      &CLSID_Door, &CLSID_DoorEntry22,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_HINGEL2 | DS_DOORPANEL7),
   gszMCDoors, gszSCEntry, L"Panels with large window",
      &CLSID_Door, &CLSID_DoorEntry23,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPG1),
   gszMCDoors, gszSCEntry, L"Panels with two windows",
      &CLSID_Door, &CLSID_DoorEntry24,
      OCDoor, (PVOID) ((DFS_RECTANGLELRLITEVENT << 16) | DSSTYLE_HINGEL2 | DS_DOORPG2),
   gszMCDoors, gszSCEntry, L"Oval window",
      &CLSID_Door, &CLSID_DoorEntry25,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPG3),
   gszMCDoors, gszSCEntry, L"Small oval",
      &CLSID_Door, &CLSID_DoorEntry26,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPG4),
   gszMCDoors, gszSCEntry, L"Semicircle window",
      &CLSID_Door, &CLSID_DoorEntry27,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORPG5),
   gszMCDoors, gszSCEntry, L"Semicircle window, double",
      &CLSID_Door, &CLSID_DoorEntry28,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_DOORPG5),

   // internal hinged
   gszMCDoors, gszSCInternal, L"Slab",
      &CLSID_Door, &CLSID_DoorInternal1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORSOLID),
   gszMCDoors, gszSCInternal, L"Louver 1",
      &CLSID_Door, &CLSID_DoorInternal2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORLOUVER1),
   gszMCDoors, gszSCInternal, L"Louver 2",
      &CLSID_Door, &CLSID_DoorInternal3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORLOUVER2),
   gszMCDoors, gszSCInternal, L"Louver 3",
      &CLSID_Door, &CLSID_DoorInternal4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORLOUVER3),
   gszMCDoors, gszSCInternal, L"Panel 1",
      &CLSID_Door, &CLSID_DoorInternal5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORPANEL1),
   gszMCDoors, gszSCInternal, L"Panel 2",
      &CLSID_Door, &CLSID_DoorInternal6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORPANEL2),
   gszMCDoors, gszSCInternal, L"Panel 3",
      &CLSID_Door, &CLSID_DoorInternal7,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORPANEL3),
   gszMCDoors, gszSCInternal, L"Panel 4",
      &CLSID_Door, &CLSID_DoorInternal8,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORPANEL4),
   gszMCDoors, gszSCInternal, L"Panel 5",
      &CLSID_Door, &CLSID_DoorInternal9,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORPANEL5),
   gszMCDoors, gszSCInternal, L"Panel 6",
      &CLSID_Door, &CLSID_DoorInternal10,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORPANEL6),
   gszMCDoors, gszSCInternal, L"Panel 7",
      &CLSID_Door, &CLSID_DoorInternal11,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORPANEL7),
   gszMCDoors, gszSCInternal, L"Braced",
      &CLSID_Door, &CLSID_DoorInternal12,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORBRACE1),

   gszMCDoors, gszSCGarage, L"Slab",
      &CLSID_Door, &CLSID_DoorGarage1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_GARAGEU | DS_DOORGARAGE1),
   gszMCDoors, gszSCGarage, L"Panel",
      &CLSID_Door, &CLSID_DoorGarage2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_GARAGEU | DS_DOORGARAGE2),
   gszMCDoors, gszSCGarage, L"Panel with windows",
      &CLSID_Door, &CLSID_DoorGarage3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_GARAGEU | DS_DOORGARAGE3),
   gszMCDoors, gszSCGarage, L"Solid",
      &CLSID_Door, &CLSID_DoorGarage4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_GARAGEU2 | DS_DOORGARAGE1),
   gszMCDoors, gszSCGarage, L"Roller",
      &CLSID_Door, &CLSID_DoorUnusual8,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_ROLLERU | DS_DOORSOLID),

   gszMCDoors, gszSCUnusual, L"Gate, portcullis",
      &CLSID_Door, &CLSID_DoorEntry1,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_POCKU | DS_DOORGATE1),
   gszMCDoors, gszSCUnusual, L"Gate, double",
      &CLSID_Door, &CLSID_DoorEntry29,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_HINGELR | DS_DOORGATE1),
   gszMCDoors, gszSCUnusual, L"Gate",
      &CLSID_Door, &CLSID_DoorEntry30,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL2 | DS_DOORGATE1),
   gszMCDoors, gszSCUnusual, L"Barn",
      &CLSID_Door, &CLSID_DoorUnusual1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKLR2 | DS_DOORBRACE1),
   gszMCDoors, gszSCUnusual, L"Saloon 1",
      &CLSID_Door, &CLSID_DoorUnusual2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR2 | DS_DOORPANEL1),
   gszMCDoors, gszSCUnusual, L"Saloon 2",
      &CLSID_Door, &CLSID_DoorUnusual3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR2 | DS_DOORLOUVER1),
   gszMCDoors, gszSCUnusual, L"Arch, circlular",
      &CLSID_Door, &CLSID_DoorUnusual4,
      OCDoor, (PVOID) ((DFS_ARCHCIRCLE << 16) | DSSTYLE_HINGELR | DS_DOORPANEL2),
   gszMCDoors, gszSCUnusual, L"Arch, peaked",
      &CLSID_Door, &CLSID_DoorUnusual5,
      OCDoor, (PVOID) ((DFS_ARCHPEAK << 16) | DSSTYLE_HINGELR | DS_DOORLOUVER2),
   gszMCDoors, gszSCUnusual, L"Rectangular, peaked",
      &CLSID_Door, &CLSID_DoorUnusual6,
      OCDoor, (PVOID) ((DFS_RECTANGLEPEAK << 16) | DSSTYLE_HINGEL | DS_DOORPANEL2),
   gszMCDoors, gszSCUnusual, L"Trapezoid",
      &CLSID_Door, &CLSID_DoorUnusual7,
      OCDoor, (PVOID) ((DFS_TRAPEZOID1 << 16) | DSSTYLE_HINGEL | DS_DOORGLASS3),

   // sliding doors
   gszMCDoors, gszSCSliding, L"Glass",
      &CLSID_Door, &CLSID_DoorSliding1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_DOORGLASS1),
   gszMCDoors, gszSCSliding, L"Japanese",
      &CLSID_Door, &CLSID_DoorSliding2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_DOORGLASS6),
   gszMCDoors, gszSCSliding, L"Louver 1",
      &CLSID_Door, &CLSID_DoorSliding3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_DOORLOUVER1),
   gszMCDoors, gszSCSliding, L"Louver 2",
      &CLSID_Door, &CLSID_DoorSliding4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_DOORLOUVER2),
   gszMCDoors, gszSCSliding, L"Louver 3",
      &CLSID_Door, &CLSID_DoorSliding5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_DOORLOUVER3),
   gszMCDoors, gszSCSliding, L"Screen",
      &CLSID_Door, &CLSID_DoorSliding6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_DOORSCREEN),
   gszMCDoors, gszSCSliding, L"Slab",
      &CLSID_Door, &CLSID_DoorSliding7,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_DOORSOLID),

   // pocket doors
   gszMCDoors, gszSCPocket, L"Glass",
      &CLSID_Door, &CLSID_DoorPocket1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKL | DS_DOORGLASS1),
   gszMCDoors, gszSCPocket, L"Japanese",
      &CLSID_Door, &CLSID_DoorPocket2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKL | DS_DOORGLASS6),
   gszMCDoors, gszSCPocket, L"Louver 1",
      &CLSID_Door, &CLSID_DoorPocket3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKL | DS_DOORLOUVER1),
   gszMCDoors, gszSCPocket, L"Louver 2",
      &CLSID_Door, &CLSID_DoorPocket4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKL | DS_DOORLOUVER2),
   gszMCDoors, gszSCPocket, L"Louver 3",
      &CLSID_Door, &CLSID_DoorPocket5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKL | DS_DOORLOUVER3),
   gszMCDoors, gszSCPocket, L"Screen",
      &CLSID_Door, &CLSID_DoorPocket6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKL | DS_DOORSCREEN),
   gszMCDoors, gszSCPocket, L"Slab",
      &CLSID_Door, &CLSID_DoorPocket7,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKL | DS_DOORSOLID),
   gszMCDoors, gszSCPocket, L"Glass, double",
      &CLSID_Door, &CLSID_DoorPocket8,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKLR | DS_DOORGLASS1),
   gszMCDoors, gszSCPocket, L"Japanese, double",
      &CLSID_Door, &CLSID_DoorPocket9,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKLR | DS_DOORGLASS6),
   gszMCDoors, gszSCPocket, L"Louver 1, double",
      &CLSID_Door, &CLSID_DoorPocket10,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_POCKLR | DS_DOORLOUVER1),
   gszMCDoors, gszSCPocket, L"Louver 2, double",
      &CLSID_Door, &CLSID_DoorPocket11,
      OCDoor, (PVOID) ((DFS_ARCHPEAK << 16) | DSSTYLE_POCKLR | DS_DOORLOUVER2),
   gszMCDoors, gszSCPocket, L"Louver 3, double",
      &CLSID_Door, &CLSID_DoorPocket12,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKLR | DS_DOORLOUVER3),
   gszMCDoors, gszSCPocket, L"Screen, double",
      &CLSID_Door, &CLSID_DoorPocket13,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKLR | DS_DOORSCREEN),
   gszMCDoors, gszSCPocket, L"Slab, double",
      &CLSID_Door, &CLSID_DoorPocket14,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_POCKLR | DS_DOORSOLID),

   // bifold
   gszMCDoors, gszSCBifold, L"Slab",
      &CLSID_Door, &CLSID_DoorBifold1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORSOLID),
   gszMCDoors, gszSCBifold, L"Japanese",
      &CLSID_Door, &CLSID_DoorBifold2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORGLASS6),
   gszMCDoors, gszSCBifold, L"Louver 1",
      &CLSID_Door, &CLSID_DoorBifold3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORLOUVER1),
   gszMCDoors, gszSCBifold, L"Louver 2",
      &CLSID_Door, &CLSID_DoorBifold4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORLOUVER2),
   gszMCDoors, gszSCBifold, L"Louver 3",
      &CLSID_Door, &CLSID_DoorBifold5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORLOUVER3),
   gszMCDoors, gszSCBifold, L"Panel 1",
      &CLSID_Door, &CLSID_DoorBifold6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORPANEL1),
   gszMCDoors, gszSCBifold, L"Panel 2",
      &CLSID_Door, &CLSID_DoorBifold7,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORPANEL2),
   gszMCDoors, gszSCBifold, L"Glass, solid",
      &CLSID_Door, &CLSID_DoorBifold8,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORGLASS1),
   gszMCDoors, gszSCBifold, L"Glass, lites",
      &CLSID_Door, &CLSID_DoorBifold9,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORGLASS2),
   gszMCDoors, gszSCBifold, L"Glass, lites 2",
      &CLSID_Door, &CLSID_DoorBifold10,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BIL | DS_DOORGLASS3),
   gszMCDoors, gszSCBifold, L"Slab, double",
      &CLSID_Door, &CLSID_DoorBifold11,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORSOLID),
   gszMCDoors, gszSCBifold, L"Japanese, double",
      &CLSID_Door, &CLSID_DoorBifold12,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORGLASS6),
   gszMCDoors, gszSCBifold, L"Louver 1, double",
      &CLSID_Door, &CLSID_DoorBifold13,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORLOUVER1),
   gszMCDoors, gszSCBifold, L"Louver 2, double",
      &CLSID_Door, &CLSID_DoorBifold14,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORLOUVER2),
   gszMCDoors, gszSCBifold, L"Louver 3, double",
      &CLSID_Door, &CLSID_DoorBifold15,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORLOUVER3),
   gszMCDoors, gszSCBifold, L"Panel 1, double",
      &CLSID_Door, &CLSID_DoorBifold16,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORPANEL1),
   gszMCDoors, gszSCBifold, L"Panel 2, double",
      &CLSID_Door, &CLSID_DoorBifold17,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORPANEL2),
   gszMCDoors, gszSCBifold, L"Glass, solid, double",
      &CLSID_Door, &CLSID_DoorBifold18,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORGLASS1),
   gszMCDoors, gszSCBifold, L"Glass, lites, double",
      &CLSID_Door, &CLSID_DoorBifold19,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORGLASS2),
   gszMCDoors, gszSCBifold, L"Glass, lites 2, double",
      &CLSID_Door, &CLSID_DoorBifold20,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_DOORGLASS3),
   gszMCDoors, gszSCBifold, L"Fancy",
      &CLSID_Door, &CLSID_DoorBifold21,
      OCDoor, (PVOID) ((DFS_ARCHLRLITES2 << 16) | DSSTYLE_BILR | DS_DOORGLASS13),

   // commercial
   gszMCDoors, gszSCCommercial, L"Slab",
      &CLSID_Door, &CLSID_DoorCommercial1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORSOLID),
   gszMCDoors, gszSCCommercial, L"Glass",
      &CLSID_Door, &CLSID_DoorCommercial2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORGLASS1),
   gszMCDoors, gszSCCommercial, L"Glass, commercial",
      &CLSID_Door, &CLSID_DoorCommercial3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORGLASS7),
   gszMCDoors, gszSCCommercial, L"Glass, circle",
      &CLSID_Door, &CLSID_DoorCommercial4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORGLASS9),
   gszMCDoors, gszSCCommercial, L"Glass, narrow",
      &CLSID_Door, &CLSID_DoorCommercial5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORGLASS11),
   gszMCDoors, gszSCCommercial, L"Glass, square",
      &CLSID_Door, &CLSID_DoorCommercial6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_DOORGLASS12),
   gszMCDoors, gszSCCommercial, L"Frame with lites 1",
      &CLSID_Door, &CLSID_DoorCommercial7,
      OCDoor, (PVOID) ((DFS_RECTANGLEVENT << 16) | DSSTYLE_HINGEL | DS_DOORSOLID),
   gszMCDoors, gszSCCommercial, L"Frame with lites 2",
      &CLSID_Door, &CLSID_DoorCommercial8,
      OCDoor, (PVOID) ((DFS_RECTANGLELITEVENT << 16) | DSSTYLE_HINGEL | DS_DOORSOLID),
   gszMCDoors, gszSCCommercial, L"Entry",
      &CLSID_Door, &CLSID_DoorCommercial9,
      OCDoor, (PVOID) ((DFS_REPEATNUMTOP << 16) | DSSTYLE_HINGELR | DS_DOORGLASS7),

   // hung windows - slide up
   gszMCWindows, gszSCHung, L"Aluminum frame",
      &CLSID_Door, &CLSID_WindowHung1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEU | DS_WINDOWPLAIN2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCHung, L"Wood frame",
      &CLSID_Door, &CLSID_WindowHung2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEU | DS_WINDOWPLAIN3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCHung, L"Lites",
      &CLSID_Door, &CLSID_WindowHung3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEU | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCHung, L"Japanese",
      &CLSID_Door, &CLSID_WindowHung4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEU | DS_WINDOWLITE8 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCHung, L"Arch",
      &CLSID_Door, &CLSID_WindowHung5,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_SLIDEU | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCHung, L"Arch, fancy",
      &CLSID_Door, &CLSID_WindowHung6,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_SLIDEU | DS_WINDOWLITE7 | DSSTYLE_WINDOW),

   // sliding - LR
   gszMCWindows, gszSCSliding, L"Aluminum frame",
      &CLSID_Door, &CLSID_WindowSliding1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_WINDOWPLAIN2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCSliding, L"Wood frame",
      &CLSID_Door, &CLSID_WindowSliding2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_WINDOWPLAIN3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCSliding, L"Lites",
      &CLSID_Door, &CLSID_WindowSliding3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCSliding, L"Japanese",
      &CLSID_Door, &CLSID_WindowSliding4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_SLIDEL | DS_WINDOWLITE8 | DSSTYLE_WINDOW),

   // casement - swing out
   gszMCWindows, gszSCCasement, L"Aluminum frame",
      &CLSID_Door, &CLSID_WindowCasement1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_WINDOWPLAIN2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Wood frame",
      &CLSID_Door, &CLSID_WindowCasement2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_WINDOWPLAIN3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Lites",
      &CLSID_Door, &CLSID_WindowCasement3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Japanese",
      &CLSID_Door, &CLSID_WindowCasement4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_WINDOWLITE8 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Shutter",
      &CLSID_Door, &CLSID_WindowCasement5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_WINDOWSHUTTER1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Aluminum frame, single",
      &CLSID_Door, &CLSID_WindowCasement6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_WINDOWPLAIN2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Wood frame, single",
      &CLSID_Door, &CLSID_WindowCasement7,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_WINDOWPLAIN3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Lites, single",
      &CLSID_Door, &CLSID_WindowCasement8,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Japanese, single",
      &CLSID_Door, &CLSID_WindowCasement9,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_WINDOWLITE8 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Shutter, single",
      &CLSID_Door, &CLSID_WindowCasement10,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEL | DS_WINDOWSHUTTER1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Arch",
      &CLSID_Door, &CLSID_WindowCasement11,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_HINGELR | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCCasement, L"Arch, peaked",
      &CLSID_Door, &CLSID_WindowCasement12,
      OCDoor, (PVOID) ((DFS_ARCHPEAK << 16) | DSSTYLE_HINGELR | DS_WINDOWLITE1 | DSSTYLE_WINDOW),

   // unusual
   gszMCWindows, gszSCUnusual, L"Bars, horizontal",
      &CLSID_Door, &CLSID_WindowUnusual1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWBARS1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCUnusual, L"Bars, vertical",
      &CLSID_Door, &CLSID_WindowUnusual2,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_FIXED | DS_WINDOWBARS2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCUnusual, L"Bars, diagonal",
      &CLSID_Door, &CLSID_WindowUnusual3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGELR | DS_WINDOWBARS3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCUnusual, L"Slats, horizontal",
      &CLSID_Door, &CLSID_WindowUnusual4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWBARS4 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCUnusual, L"Slats, vertical",
      &CLSID_Door, &CLSID_WindowUnusual5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWBARS5 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCUnusual, L"Octagon",
      &CLSID_Door, &CLSID_WindowUnusual6,
      OCDoor, (PVOID) ((DFS_OCTAGON << 16) | DSSTYLE_HINGELR | DS_WINDOWPLAIN3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCUnusual, L"Roller",
      &CLSID_Door, &CLSID_WindowUnusual7,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_ROLLERU | DS_WINDOWPLAIN1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCUnusual, L"Bifold",
      &CLSID_Door, &CLSID_WindowUnusual8,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_BILR | DS_WINDOWPLAIN3 | DSSTYLE_WINDOW),


   // awning - swing up
   gszMCWindows, gszSCAwning, L"Aluminum frame",
      &CLSID_Door, &CLSID_WindowAwning1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEU | DS_WINDOWPLAIN2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCAwning, L"Wood frame",
      &CLSID_Door, &CLSID_WindowAwning2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEU | DS_WINDOWPLAIN3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCAwning, L"Lites",
      &CLSID_Door, &CLSID_WindowAwning3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEU | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCAwning, L"Japanese",
      &CLSID_Door, &CLSID_WindowAwning4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEU | DS_WINDOWLITE8 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCAwning, L"Shutter 1",
      &CLSID_Door, &CLSID_WindowAwning5,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEU | DS_WINDOWSHUTTER1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCAwning, L"Shutter 2",
      &CLSID_Door, &CLSID_WindowAwning6,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_HINGEU | DS_WINDOWSHUTTER2 | DSSTYLE_WINDOW),

   // louvers
   gszMCWindows, gszSCLouver, L"Clear",
      &CLSID_Door, &CLSID_WindowLouver1,
      OCDoor, (PVOID) ((DFS_REPEATMIN << 16) | DSSTYLE_WINDOW | DSSTYLE_LOUVER | DS_WINDOWLOUVER1),
   gszMCWindows, gszSCLouver, L"Frosted",
      &CLSID_Door, &CLSID_WindowLouver2,
      OCDoor, (PVOID) ((DFS_REPEATMIN << 16) | DSSTYLE_WINDOW | DSSTYLE_LOUVER | DS_WINDOWPLAIN4),
   gszMCWindows, gszSCLouver, L"Wood",
      &CLSID_Door, &CLSID_WindowLouver3,
      OCDoor, (PVOID) ((DFS_REPEATMIN << 16) | DSSTYLE_WINDOW | DSSTYLE_LOUVER | DS_DOORSOLID),

   // fixed
   gszMCWindows, gszSCFixed, L"Rectangle",
      &CLSID_Door, &CLSID_WindowFixed1,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWPLAIN1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Rectangle, with lites",
      &CLSID_Door, &CLSID_WindowFixed2,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Screen",
      &CLSID_Door, &CLSID_WindowFixed3,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWSCREEN | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Rectangle, with lites 2",
      &CLSID_Door, &CLSID_WindowFixed4,
      OCDoor, (PVOID) ((DFS_RECTANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWLITE3 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Arch",
      &CLSID_Door, &CLSID_WindowFixed5,
      OCDoor, (PVOID) ((DFS_ARCH << 16) | DSSTYLE_FIXED | DS_WINDOWLITE7 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Arch, peaked",
      &CLSID_Door, &CLSID_WindowFixed6,
      OCDoor, (PVOID) ((DFS_ARCHPEAK << 16) | DSSTYLE_FIXED | DS_WINDOWLITE7 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Circle",
      &CLSID_Door, &CLSID_WindowFixed7,
      OCDoor, (PVOID) ((DFS_CIRCLE << 16) | DSSTYLE_FIXED | DS_WINDOWPLAIN1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Circle, lites",
      &CLSID_Door, &CLSID_WindowFixed8,
      OCDoor, (PVOID) ((DFS_CIRCLE << 16) | DSSTYLE_FIXED | DS_WINDOWLITE2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Rectangle, peaked",
      &CLSID_Door, &CLSID_WindowFixed9,
      OCDoor, (PVOID) ((DFS_ARCHTRIANGLE << 16) | DSSTYLE_FIXED | DS_WINDOWLITE1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Triangle",
      &CLSID_Door, &CLSID_WindowFixed10,
      OCDoor, (PVOID) ((DFS_TRIANGLERIGHT << 16) | DSSTYLE_FIXED | DS_WINDOWPLAIN1 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Pentagon",
      &CLSID_Door, &CLSID_WindowFixed11,
      OCDoor, (PVOID) ((DFS_PENTAGON << 16) | DSSTYLE_FIXED | DS_WINDOWLITE2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Hexagon",
      &CLSID_Door, &CLSID_WindowFixed12,
      OCDoor, (PVOID) ((DFS_HEXAGON << 16) | DSSTYLE_FIXED | DS_WINDOWLITE2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Octagon",
      &CLSID_Door, &CLSID_WindowFixed13,
      OCDoor, (PVOID) ((DFS_OCTAGON << 16) | DSSTYLE_FIXED | DS_WINDOWLITE2 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Circle, quarter",
      &CLSID_Door, &CLSID_WindowFixed14,
      OCDoor, (PVOID) ((DFS_ARCHHALF2 << 16) | DSSTYLE_FIXED | DS_WINDOWLITE6 | DSSTYLE_WINDOW),
   gszMCWindows, gszSCFixed, L"Circle, half",
      &CLSID_Door, &CLSID_WindowFixed15,
      OCDoor, (PVOID) ((DFS_ARCH2 << 16) | DSSTYLE_FIXED | DS_WINDOWLITE4 | DSSTYLE_WINDOW),

   // doorways
   gszMCOpenings, gszSCDoorways, L"Rectangular",
      &CLSID_Door, &CLSID_Doorway1,
      OCDoor, (PVOID) (DFS_RECTANGLE << 16),
   gszMCOpenings, gszSCDoorways, L"Rectangular, side lites",
      &CLSID_Door, &CLSID_Doorway2,
      OCDoor, (PVOID) (DFS_RECTANGLELRLITES << 16),
   gszMCOpenings, gszSCDoorways, L"Rectangular, side and top lites",
      &CLSID_Door, &CLSID_Doorway3,
      OCDoor, (PVOID) (DFS_RECTANGLELRLITEVENT << 16),
   gszMCOpenings, gszSCDoorways, L"Arch",
      &CLSID_Door, &CLSID_Doorway4,
      OCDoor, (PVOID) (DFS_ARCH << 16),
   gszMCOpenings, gszSCDoorways, L"Arch, top lite",
      &CLSID_Door, &CLSID_Doorway5,
      OCDoor, (PVOID) (DFS_ARCHSPLIT << 16),
   gszMCOpenings, gszSCDoorways, L"Arch, side and top lites",
      &CLSID_Door, &CLSID_Doorway6,
      OCDoor, (PVOID) (DFS_ARCHLRLITES2 << 16),
   gszMCOpenings, gszSCDoorways, L"Circle, half",
      &CLSID_Door, &CLSID_Doorway7,
      OCDoor, (PVOID) (DFS_ARCH2 << 16),
   gszMCOpenings, gszSCDoorways, L"Arch, circular 1",
      &CLSID_Door, &CLSID_Doorway8,
      OCDoor, (PVOID) (DFS_ARCHPARTCIRCLE << 16),
   gszMCOpenings, gszSCDoorways, L"Arch, circular 2",
      &CLSID_Door, &CLSID_Doorway9,
      OCDoor, (PVOID) (DFS_ARCHCIRCLE << 16),
   gszMCOpenings, gszSCDoorways, L"Arch, peaked",
      &CLSID_Door, &CLSID_Doorway10,
      OCDoor, (PVOID) (DFS_ARCHPEAK << 16),
   gszMCOpenings, gszSCDoorways, L"Trapezoid",
      &CLSID_Door, &CLSID_Doorway11,
      OCDoor, (PVOID) (DFS_TRAPEZOID1 << 16),
   gszMCOpenings, gszSCDoorways, L"Octagon",
      &CLSID_Door, &CLSID_Doorway12,
      OCDoor, (PVOID) (DFS_OCTAGON << 16),
   gszMCOpenings, gszSCDoorways, L"Pentagon",
      &CLSID_Door, &CLSID_Doorway13,
      OCDoor, (PVOID) (DFS_PENTAGON << 16),
   gszMCOpenings, gszSCDoorways, L"Hexagon",
      &CLSID_Door, &CLSID_Doorway14,
      OCDoor, (PVOID) (DFS_HEXAGON << 16),


   gszMCOpenings, gszSCStructureUniHole, L"Rectangular hole",
      &CLSID_UniHole, &CLSID_UniHoleRect,
      OCUniHole, (PVOID) 0x0000,
   gszMCOpenings, gszSCStructureUniHole, L"Circular hole",
      &CLSID_UniHole, &CLSID_UniHoleCirc,
      OCUniHole, (PVOID) 0x0001,
   gszMCOpenings, gszSCStructureUniHole, L"Any-shaped hole",
      &CLSID_UniHole, &CLSID_UniHoleAny,
      OCUniHole, (PVOID) 0x0002,

   gszMCBuildBlock, gszSCPiers, L"Log-stump piers",
      &CLSID_Piers, &CLSID_PiersLogStump,
      OCPiers, (PVOID) PS_LOGSTUMP,
   gszMCBuildBlock, gszSCPiers, L"Steel piers",
      &CLSID_Piers, &CLSID_PiersSteel,
      OCPiers, (PVOID) PS_STEEL,
   gszMCBuildBlock, gszSCPiers, L"Wood piers",
      &CLSID_Piers, &CLSID_PiersWood,
      OCPiers, (PVOID) PS_WOOD,
   gszMCBuildBlock, gszSCPiers, L"Concrete (square) piers",
      &CLSID_Piers, &CLSID_PiersCementSquare,
      OCPiers, (PVOID) PS_CEMENTSQUARE,
   gszMCBuildBlock, gszSCPiers, L"Concrete (round) piers",
      &CLSID_Piers, &CLSID_PiersCementRound,
      OCPiers, (PVOID) PS_CEMENTROUND,
   gszMCBuildBlock, gszSCPiers, L"Greek piers",
      &CLSID_Piers, &CLSID_PiersGreek,
      OCPiers, (PVOID) PS_GREEK,

   gszMCBuildBlock, gszSCBalustrades, L"Wood balustrade, style 1",
      &CLSID_Balustrade, &CLSID_BalustradeBalVertWood,
      OCBalustrade, (PVOID) BS_BALVERTWOOD,
   gszMCBuildBlock, gszSCBalustrades, L"Wood balustrade, style 2",
      &CLSID_Balustrade, &CLSID_BalustradeBalVertWood2,
      OCBalustrade, (PVOID) BS_BALVERTWOOD2,
   gszMCBuildBlock, gszSCBalustrades, L"Wood balustrade, style 3",
      &CLSID_Balustrade, &CLSID_BalustradeBalVertWood3,
      OCBalustrade, (PVOID) BS_BALVERTWOOD3,
   gszMCBuildBlock, gszSCBalustrades, L"Log balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalVertLog,
      OCBalustrade, (PVOID) BS_BALVERTLOG,
   gszMCBuildBlock, gszSCBalustrades, L"Wrought-iron balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalVertWroughtIron,
      OCBalustrade, (PVOID) BS_BALVERTWROUGHTIRON,
   gszMCBuildBlock, gszSCBalustrades, L"Steel-verticals balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalVertSteel,
      OCBalustrade, (PVOID) BS_BALVERTSTEEL,
   gszMCBuildBlock, gszSCBalustrades, L"Horizontal-board balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalHorzPanels,
      OCBalustrade, (PVOID) BS_BALHORZPANELS,
   gszMCBuildBlock, gszSCBalustrades, L"Horizontal-wood balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalHorzWood,
      OCBalustrade, (PVOID) BS_BALHORZWOOD,
   gszMCBuildBlock, gszSCBalustrades, L"Stainless-steel cable balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalHorzWire,
      OCBalustrade, (PVOID) BS_BALHORZWIRE,
   gszMCBuildBlock, gszSCBalustrades, L"Stainless-steel cable balustrade w/rail",
      &CLSID_Balustrade, &CLSID_BalustradeBalHorzWireRail,
      OCBalustrade, (PVOID) BS_BALHORZWIRERAIL,
   gszMCBuildBlock, gszSCBalustrades, L"Horizontal steel-pole balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalHorzPole,
      OCBalustrade, (PVOID) BS_BALHORZPOLE,
   gszMCBuildBlock, gszSCBalustrades, L"Suspended panel balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalPanel,
      OCBalustrade, (PVOID) BS_BALPANEL,
   gszMCBuildBlock, gszSCBalustrades, L"Solid balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalPanelSolid,
      OCBalustrade, (PVOID) BS_BALPANELSOLID,
   gszMCBuildBlock, gszSCBalustrades, L"Open balustrade, style 1",
      &CLSID_Balustrade, &CLSID_BalustradeBalOpen,
      OCBalustrade, (PVOID) BS_BALOPEN,
   gszMCBuildBlock, gszSCBalustrades, L"Open balustrade, style 2",
      &CLSID_Balustrade, &CLSID_BalustradeBalOpenMiddle,
      OCBalustrade, (PVOID) BS_BALOPENMIDDLE,
   gszMCBuildBlock, gszSCBalustrades, L"Open pole balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalOpenPole,
      OCBalustrade, (PVOID) BS_BALOPENPOLE,
   gszMCBuildBlock, gszSCBalustrades, L"X-brace balustrade",
      &CLSID_Balustrade, &CLSID_BalustradeBalBraceX,
      OCBalustrade, (PVOID) BS_BALBRACEX,
   gszMCBuildBlock, gszSCBalustrades, L"Fancy, style 1",
      &CLSID_Balustrade, &CLSID_BalustradeBalFancyGreek,
      OCBalustrade, (PVOID) BS_BALFANCYGREEK,
   gszMCBuildBlock, gszSCBalustrades, L"Fancy, style 2",
      &CLSID_Balustrade, &CLSID_BalustradeBalFancyGreek2,
      OCBalustrade, (PVOID) BS_BALFANCYGREEK2,
   gszMCBuildBlock, gszSCBalustrades, L"Fancy, style 3",
      &CLSID_Balustrade, &CLSID_BalustradeBalFancyWood,
      OCBalustrade, (PVOID) BS_BALFANCYWOOD,

   gszMCLighting, gszSCWall, L"Walls sconce 1",
      &CLSID_Light, &CLSID_LightWall1,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANSCONCECONE, 0),
   gszMCLighting, gszSCWall, L"Walls sconce 2",
      &CLSID_Light, &CLSID_LightWall2,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANSCONCEHEMISPHERE, 0),
   gszMCLighting, gszSCWall, L"Walls sconce 3",
      &CLSID_Light, &CLSID_LightWall3,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANSCONELOOP, 0),
   gszMCLighting, gszSCWall, L"Flourescent, 8w",
      &CLSID_Light, &CLSID_LightWall4,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_FLOURO8W1CURVED, 0),
   gszMCLighting, gszSCWall, L"Flourescent, 18w",
      &CLSID_Light, &CLSID_LightWall5,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_FLOURO18W1CURVED, 0),
   gszMCLighting, gszSCWall, L"Flourescent, 36w",
      &CLSID_Light, &CLSID_LightWall6,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_FLOURO36W1CURVED, 0),
   gszMCLighting, gszSCWall, L"Spotlight 1",
      &CLSID_Light, &CLSID_LightWall7,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_INCANSPOT2, 0),
   gszMCLighting, gszSCWall, L"Spotlight 2",
      &CLSID_Light, &CLSID_LightWall8,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_INCANSPOT3, 0),
   gszMCLighting, gszSCWall, L"Spotlight 3",
      &CLSID_Light, &CLSID_LightWall9,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_INCANSPOT4, 0),
   gszMCLighting, gszSCWall, L"Spotlight 4",
      &CLSID_Light, &CLSID_LightWall10,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_INCANSPOT5, 0),
   gszMCLighting, gszSCWall, L"Spotlight 5",
      &CLSID_Light, &CLSID_LightWall11,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_INCANSPOT6, 0),
   gszMCLighting, gszSCWall, L"Flourescent spotlight",
      &CLSID_Light, &CLSID_LightWall12,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_FLOUROSPOT2LID, 0),
   gszMCLighting, gszSCWall, L"Glass shade 1, down",
      &CLSID_Light, &CLSID_LightWall13,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEDDOWN, LST_GLASSINCANROUNDCONE, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Glass shade 1, up",
      &CLSID_Light, &CLSID_LightWall14,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_GLASSINCANROUNDCONE, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Glass shade 2, down",
      &CLSID_Light, &CLSID_LightWall15,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEDDOWN, LST_GLASSINCANROUNDED, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Glass shade 2, up",
      &CLSID_Light, &CLSID_LightWall16,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_GLASSINCANROUNDED, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Glass shade 3, down",
      &CLSID_Light, &CLSID_LightWall17,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEDDOWN, LST_GLASSINCANROUNDHEMI, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Glass shade 3, up",
      &CLSID_Light, &CLSID_LightWall18,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_GLASSINCANROUNDHEMI, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Lamp shade, down",
      &CLSID_Light, &CLSID_LightWall19,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEDDOWN, LST_ROUNDDIAGCLOTH23, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Lamp shade, up",
      &CLSID_Light, &CLSID_LightWall20,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_ROUNDDIAGCLOTH23DN, LSCALE_THIRD),
   gszMCLighting, gszSCWall, L"Bulb, down",
      &CLSID_Light, &CLSID_LightWall21,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEDDOWN, LST_GLASSINCANBULBBBASE, LSCALE_23),
   gszMCLighting, gszSCWall, L"Bulb, up",
      &CLSID_Light, &CLSID_LightWall22,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_GLASSINCANBULBBBASE, LSCALE_23),
   gszMCLighting, gszSCWall, L"Fake candle",
      &CLSID_Light, &CLSID_LightWall23,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_HALOGENFAKECANDLE, 0),
   gszMCLighting, gszSCWall, L"Old fashioned",
      &CLSID_Light, &CLSID_LightWall24,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_GLASSINCANOLDFASHIONED, LSCALE_23),
   gszMCLighting, gszSCWall, L"Light globes 2",
      &CLSID_Light, &CLSID_LightWall25,
      OCLight, LIGHTTYPE(LSTANDT_TRACKGLOBES2, LST_INCANBULBSOCKET, 0),
   gszMCLighting, gszSCWall, L"Light globes 3",
      &CLSID_Light, &CLSID_LightWall26,
      OCLight, LIGHTTYPE(LSTANDT_TRACKGLOBES3, LST_INCANBULBSOCKET, 0),
   gszMCLighting, gszSCWall, L"Light globes 4",
      &CLSID_Light, &CLSID_LightWall27,
      OCLight, LIGHTTYPE(LSTANDT_TRACKGLOBES4, LST_INCANBULBSOCKET, 0),
   gszMCLighting, gszSCWall, L"Light globes 5",
      &CLSID_Light, &CLSID_LightWall28,
      OCLight, LIGHTTYPE(LSTANDT_TRACKGLOBES5, LST_INCANBULBSOCKET, 0),
   gszMCLighting, gszSCOutdoor, L"Spotlight",
      &CLSID_Light, &CLSID_LightWall29,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_INCANSPOTSOCKET, 0),
   gszMCLighting, gszSCOutdoor, L"Spotlight, halogen",
      &CLSID_Light, &CLSID_LightWall30,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_HALOGENTUBESPOT, 0),
   gszMCLighting, gszSCOutdoor, L"Spotlights",
      &CLSID_Light, &CLSID_LightWall31,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALK2, LST_INCANSPOTSOCKET, 0),
   gszMCLighting, gszSCOutdoor, L"Entry light",
      &CLSID_Light, &CLSID_LightWall32,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANCEILHEMISPHERE, 0),
   gszMCLighting, gszSCOutdoor, L"Entry light, old fashioned",
      &CLSID_Light, &CLSID_LightWall33,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKCURVEUP, LST_GLASSINCANBOXROOF, 0),

   gszMCLighting, gszSCFire, L"Candle 1",
      &CLSID_Light, &CLSID_LightFire1,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_FIRECANDLE1, 0),
   gszMCLighting, gszSCFire, L"Candle 2",
      &CLSID_Light, &CLSID_LightFire2,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_FIRECANDLE2, 0),
   gszMCLighting, gszSCFire, L"Torch",
      &CLSID_Light, &CLSID_LightFire3,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_FIRETORCH, 0),
   gszMCLighting, gszSCFire, L"Torch, wall mounted",
      &CLSID_Light, &CLSID_LightFire4,
      OCLight, LIGHTTYPE(LSTANDT_FIRETORCHHOLDER, LST_FIRETORCH, 0),
   gszMCLighting, gszSCFire, L"Torch, on a stand",
      &CLSID_Light, &CLSID_LightFire5,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPSIMPLE1, LST_FIRETORCH, 0),
   gszMCLighting, gszSCFire, L"Fire, on a stand",
      &CLSID_Light, &CLSID_LightFire6,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLETALL, LST_FIREBOWL, 0),
   gszMCLighting, gszSCFire, L"Fireplace log",
      &CLSID_Light, &CLSID_LightFire7,
      OCLight, LIGHTTYPE(LSTANDT_FIRELOG, LST_FLAMEFIRE, 0),
   gszMCLighting, gszSCFire, L"Chandelier with candles",
      &CLSID_Light, &CLSID_LightFire8,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER6UP, LST_FIRECANDLE1, 0),
   gszMCLighting, gszSCFire, L"Candle, on a stand",
      &CLSID_Light, &CLSID_LightFire9,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_FIRECANDLE1, 0),
   gszMCLighting, gszSCFire, L"Candelabra",
      &CLSID_Light, &CLSID_LightFire10,
      OCLight, LIGHTTYPE(LSTANDT_FIRECANDELABRA, LST_FIRECANDLE1, 0),
   gszMCLighting, gszSCFire, L"Flame, candle",
      &CLSID_Light, &CLSID_LightFire11,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_FLAMECANDLE, 0),
   gszMCLighting, gszSCFire, L"Flame, torch",
      &CLSID_Light, &CLSID_LightFire12,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_FLAMETORCH, 0),
   gszMCLighting, gszSCFire, L"Flame, fire",
      &CLSID_Light, &CLSID_LightFire13,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_FLAMEFIRE, 0),

   gszMCLighting, gszSCLightbulbs, L"Incandescent, globe",
      &CLSID_Light, &CLSID_LightBulb1,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBINCANGLOBE, 0),
   gszMCLighting, gszSCLightbulbs, L"Incandescent, spotlight",
      &CLSID_Light, &CLSID_LightBulb2,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBINCANSPOT, 0),
   gszMCLighting, gszSCLightbulbs, L"Halogen, spotlight",
      &CLSID_Light, &CLSID_LightBulb3,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBHALOGENSPOT, 0),
   gszMCLighting, gszSCLightbulbs, L"Halogen, bulb",
      &CLSID_Light, &CLSID_LightBulb4,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBHALOGENBULB, 0),
   gszMCLighting, gszSCLightbulbs, L"Halogen, tube",
      &CLSID_Light, &CLSID_LightBulb5,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBHALOGENTUBE, 0),
   gszMCLighting, gszSCLightbulbs, L"Flourescent, 8w",
      &CLSID_Light, &CLSID_LightBulb6,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBFLOURO8, 0),
   gszMCLighting, gszSCLightbulbs, L"Flourescent, 18w",
      &CLSID_Light, &CLSID_LightBulb7,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBFLOURO18, 0),
   gszMCLighting, gszSCLightbulbs, L"Flourescent, 36w",
      &CLSID_Light, &CLSID_LightBulb8,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBFLOURO36, 0),
   gszMCLighting, gszSCLightbulbs, L"Flourescent, round",
      &CLSID_Light, &CLSID_LightBulb9,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBFLOUROROUND, 0),
   gszMCLighting, gszSCLightbulbs, L"Flourescent, globe",
      &CLSID_Light, &CLSID_LightBulb10,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBFLOUROGLOBE, 0),
   gszMCLighting, gszSCLightbulbs, L"Sodium, globe",
      &CLSID_Light, &CLSID_LightBulb11,
      OCLight, LIGHTTYPE(LSTANDT_NONESTANDING, LST_BULBSODIUMGLOBE, 0),

   gszMCLighting, gszSCHanging, L"Hanging 1",
      &CLSID_Light, &CLSID_LightHanging1,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANROUNDCONE, 0),
   gszMCLighting, gszSCHanging, L"Hanging 2",
      &CLSID_Light, &CLSID_LightHanging2,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANROUNDHEMI, 0),
   gszMCLighting, gszSCHanging, L"Hanging 3",
      &CLSID_Light, &CLSID_LightHanging3,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANHEXHEMI, 0),
   gszMCLighting, gszSCHanging, L"Hanging 4",
      &CLSID_Light, &CLSID_LightHanging5,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANSQUAREOPEN, 0),
   gszMCLighting, gszSCHanging, L"Hanging 5",
      &CLSID_Light, &CLSID_LightHanging6,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANBULB, LSCALE_LARGER),
   gszMCLighting, gszSCHanging, L"Hanging 6",
      &CLSID_Light, &CLSID_LightHanging7,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANCYLINDER, LSCALE_LARGER),
   gszMCLighting, gszSCHanging, L"Hanging 7",
      &CLSID_Light, &CLSID_LightHanging8,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_INCANBULBSOCKET, LSCALE_LARGER),
   gszMCLighting, gszSCHanging, L"Hanging 8",
      &CLSID_Light, &CLSID_LightHanging9,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_HALOGENBOWL, 0),
   gszMCLighting, gszSCHanging, L"Pool table light",
      &CLSID_Light, &CLSID_LightHanging10,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_INCANPOOLTABLE, 0),
   gszMCLighting, gszSCHanging, L"Hanging group 1",
      &CLSID_Light, &CLSID_LightHanging11,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK3, LST_GLASSINCANROUNDED, LSCALE_23),
   gszMCLighting, gszSCHanging, L"Hanging group 2",
      &CLSID_Light, &CLSID_LightHanging12,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK3, LST_GLASSINCANBULBBBASE, LSCALE_23),
   gszMCLighting, gszSCHanging, L"Chandelier down 3",
      &CLSID_Light, &CLSID_LightHanging13,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER3, LST_GLASSINCANBULBBBASE, 0),
   gszMCLighting, gszSCHanging, L"Chandelier down 4",
      &CLSID_Light, &CLSID_LightHanging14,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER4, LST_ROUNDDIAGCLOTH2, LSCALE_HALF),
   gszMCLighting, gszSCHanging, L"Chandelier down 5",
      &CLSID_Light, &CLSID_LightHanging15,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER5, LST_HALOGEN8BULB, 0),
   gszMCLighting, gszSCHanging, L"Chandelier down 6",
      &CLSID_Light, &CLSID_LightHanging16,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER6, LST_HALOGEN8CYLINDER, 0),
   gszMCLighting, gszSCHanging, L"Chandelier up 3",
      &CLSID_Light, &CLSID_LightHanging17,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER3UP, LST_ROUNDDIAGCLOTH23DN, 0),
   gszMCLighting, gszSCHanging, L"Chandelier up 4",
      &CLSID_Light, &CLSID_LightHanging18,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER4UP, LST_GLASSINCANROUNDEDDN, LSCALE_HALF),
   gszMCLighting, gszSCHanging, L"Chandelier up 5",
      &CLSID_Light, &CLSID_LightHanging19,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER5UP, LST_HALOGEN8BULB, 0),
   gszMCLighting, gszSCHanging, L"Chandelier up 6",
      &CLSID_Light, &CLSID_LightHanging20,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGCHANDELIER6UP, LST_HALOGENFAKECANDLE, 0),
   gszMCLighting, gszSCHanging, L"Hanging 9",
      &CLSID_Light, &CLSID_LightHanging4,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANHEXCONE, 0),
   gszMCLighting, gszSCHanging, L"Hanging 10",
      &CLSID_Light, &CLSID_LightHanging21,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGSTALK1, LST_GLASSINCANROUNDCONEDN, 0),

   gszMCLighting, gszSCCeiling, L"Flourescent, round",
      &CLSID_Light, &CLSID_LightCeiling1,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_FLOUROROUND, 0),
   gszMCLighting, gszSCCeiling, L"Recessed, halogen",
      &CLSID_Light, &CLSID_LightCeiling2,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_HALOGENCEILSPOT, 0),
   gszMCLighting, gszSCCeiling, L"Recessed, incandescent",
      &CLSID_Light, &CLSID_LightCeiling3,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANCEILSPOT, 0),
   gszMCLighting, gszSCCeiling, L"Incandescent, square",
      &CLSID_Light, &CLSID_LightCeiling4,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANCEILSQUARE, 0),
   gszMCLighting, gszSCCeiling, L"Incandescent, hemisphere",
      &CLSID_Light, &CLSID_LightCeiling5,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANCEILHEMISPHERE, 0),
   gszMCLighting, gszSCCeiling, L"Incandescent, cone",
      &CLSID_Light, &CLSID_LightCeiling6,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANCEILCONE, 0),
   gszMCLighting, gszSCCeiling, L"Incandescent, round",
      &CLSID_Light, &CLSID_LightCeiling7,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_INCANCEILROUND, 0),
   gszMCLighting, gszSCCeiling, L"Skylight diffuser, round",
      &CLSID_Light, &CLSID_LightCeiling8,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_SKYLIGHTDIFFUSERROUND, 0),
   gszMCLighting, gszSCCeiling, L"Skylight diffuser, square",
      &CLSID_Light, &CLSID_LightCeiling9,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_SKYLIGHTDIFFUSERSQUARE, 0),
   gszMCLighting, gszSCCeiling, L"Spotlight 1",
      &CLSID_Light, &CLSID_LightCeiling10,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALKOUT, LST_INCANSPOT2, 0),
   gszMCLighting, gszSCCeiling, L"Spotlight 2",
      &CLSID_Light, &CLSID_LightCeiling11,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALK2, LST_INCANSPOT3, 0),
   gszMCLighting, gszSCCeiling, L"Spotlight 3",
      &CLSID_Light, &CLSID_LightCeiling12,
      OCLight, LIGHTTYPE(LSTANDT_WALLSTALK3, LST_INCANSPOT4, 0),
   gszMCLighting, gszSCCeiling, L"Globe",
      &CLSID_Light, &CLSID_LightCeiling13,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGFIXED1, LST_GLASSINCANBULBBBASE, 0),
   gszMCLighting, gszSCCeiling, L"Cylinder",
      &CLSID_Light, &CLSID_LightCeiling14,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGFIXED1, LST_GLASSINCANCYLINDER, 0),
   gszMCLighting, gszSCCeiling, L"Globe 2",
      &CLSID_Light, &CLSID_LightCeiling15,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGFIXED2, LST_GLASSINCANBULBBBASE, 0),
   gszMCLighting, gszSCCeiling, L"Globe 3",
      &CLSID_Light, &CLSID_LightCeiling16,
      OCLight, LIGHTTYPE(LSTANDT_CEILINGFIXED3, LST_GLASSINCANBULBBBASE, 0),
   gszMCLighting, gszSCCeiling, L"Track lighting 2",
      &CLSID_Light, &CLSID_LightCeiling17,
      OCLight, LIGHTTYPE(LSTANDT_TRACK2, LST_HALOGENSPOT1, 0),
   gszMCLighting, gszSCCeiling, L"Track lighting 3",
      &CLSID_Light, &CLSID_LightCeiling18,
      OCLight, LIGHTTYPE(LSTANDT_TRACK3, LST_HALOGENSPOT2, 0),
   gszMCLighting, gszSCCeiling, L"Track lighting 4",
      &CLSID_Light, &CLSID_LightCeiling19,
      OCLight, LIGHTTYPE(LSTANDT_TRACK4, LST_HALOGENSPOT3, 0),
   gszMCLighting, gszSCCeiling, L"Track lighting 5",
      &CLSID_Light, &CLSID_LightCeiling20,
      OCLight, LIGHTTYPE(LSTANDT_TRACK5, LST_HALOGENSPOT4, 0),
   gszMCLighting, gszSCCeiling, L"Flourescent, 8w",
      &CLSID_Light, &CLSID_LightCeiling21,
      OCLight, LIGHTTYPE(LSTANDT_FLOURO8W1, LST_FLOURO8W1, 0),
   gszMCLighting, gszSCCeiling, L"Flourescent, 18w",
      &CLSID_Light, &CLSID_LightCeiling22,
      OCLight, LIGHTTYPE(LSTANDT_FLOURO18W1, LST_FLOURO18W1, 0),
   gszMCLighting, gszSCCeiling, L"Flourescent, 18w x 2",
      &CLSID_Light, &CLSID_LightCeiling23,
      OCLight, LIGHTTYPE(LSTANDT_FLOURO18W2, LST_FLOURO18W2, 0),
   gszMCLighting, gszSCCeiling, L"Flourescent, 36w",
      &CLSID_Light, &CLSID_LightCeiling24,
      OCLight, LIGHTTYPE(LSTANDT_FLOURO36W1, LST_FLOURO36W1, 0),
   gszMCLighting, gszSCCeiling, L"Flourescent, 36w x 2",
      &CLSID_Light, &CLSID_LightCeiling25,
      OCLight, LIGHTTYPE(LSTANDT_FLOURO36W2, LST_FLOURO36W2, 0),
   gszMCLighting, gszSCCeiling, L"Flourescent, 36w x 4",
      &CLSID_Light, &CLSID_LightCeiling26,
      OCLight, LIGHTTYPE(LSTANDT_NONEMOUNTED, LST_FLOURO36W4, 0),

   gszMCLighting, gszSCOutdoor, L"Pathway, halogen style 1",
      &CLSID_Light, &CLSID_LightOutdoor1,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_HALOGEN8BULBLID, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, halogen style 2",
      &CLSID_Light, &CLSID_LightOutdoor2,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_HALOGEN8BOXLID, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, halogen style 3",
      &CLSID_Light, &CLSID_LightOutdoor3,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_HALOGEN8CYLINDERLID, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, halogen style 4",
      &CLSID_Light, &CLSID_LightOutdoor4,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_HALOGEN8BOXROOF, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, halogen style 5",
      &CLSID_Light, &CLSID_LightOutdoor5,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_HALOGEN8CYLINDERROOF, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, incandescent style 1",
      &CLSID_Light, &CLSID_LightOutdoor6,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_GLASSINCANBULBLID, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, incandescent style 2",
      &CLSID_Light, &CLSID_LightOutdoor7,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_GLASSINCANBOXLID, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, incandescent style 3",
      &CLSID_Light, &CLSID_LightOutdoor8,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_HALOGEN8BULBLID, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, incandescent style 4",
      &CLSID_Light, &CLSID_LightOutdoor9,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_GLASSINCANCYLINDERLID, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, incandescent style 5",
      &CLSID_Light, &CLSID_LightOutdoor10,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_GLASSINCANBOXROOF, 0),
   gszMCLighting, gszSCOutdoor, L"Pathway, incandescent style 6",
      &CLSID_Light, &CLSID_LightOutdoor11,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORPATHWAY, LST_GLASSINCANCYLINDERROOF, 0),
   gszMCLighting, gszSCOutdoor, L"Lamp post, style 1",
      &CLSID_Light, &CLSID_LightOutdoor12,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORLAMP2M, LST_GLASSINCANBULB, LSCALE_LARGER),
   gszMCLighting, gszSCOutdoor, L"Lamp post, style 2",
      &CLSID_Light, &CLSID_LightOutdoor13,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORLAMP2M, LST_GLASSINCANBOX, LSCALE_LARGER),
   gszMCLighting, gszSCOutdoor, L"Lamp post, style 3",
      &CLSID_Light, &CLSID_LightOutdoor14,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORLAMP2M, LST_GLASSINCANCYLINDER, LSCALE_LARGER),
   gszMCLighting, gszSCOutdoor, L"Lamp post, style 4",
      &CLSID_Light, &CLSID_LightOutdoor15,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORLAMP2M, LST_GLASSINCANBOXROOF, LSCALE_LARGER),
   gszMCLighting, gszSCOutdoor, L"Lamp post, style 5",
      &CLSID_Light, &CLSID_LightOutdoor16,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORLAMP2M, LST_GLASSINCANCYLINDERROOF, LSCALE_LARGER),
   gszMCLighting, gszSCOutdoor, L"Lamp post, style 6",
      &CLSID_Light, &CLSID_LightOutdoor17,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORLAMPTRIPPLE, LST_GLASSINCANBULB, LSCALE_LARGER),
   gszMCLighting, gszSCOutdoor, L"Lamp post, tulip 1",
      &CLSID_Light, &CLSID_LightOutdoor18,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORTULIP, LST_GLASSINCANROUNDCONE, 0),
   gszMCLighting, gszSCOutdoor, L"Lamp post, tulip 2",
      &CLSID_Light, &CLSID_LightOutdoor19,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOORTULIP, LST_HALOGENBOWL, 0),
   gszMCLighting, gszSCOutdoor, L"Lamp post, sodium 1",
      &CLSID_Light, &CLSID_LightOutdoor20,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOOROVERHANG, LST_SODIUMSPOT1, 0),
   gszMCLighting, gszSCOutdoor, L"Lamp post, sodium 2",
      &CLSID_Light, &CLSID_LightOutdoor21,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOOROVERHANG2, LST_SODIUMSPOT1, 0),
   gszMCLighting, gszSCOutdoor, L"Lamp post, sodium 3",
      &CLSID_Light, &CLSID_LightOutdoor22,
      OCLight, LIGHTTYPE(LSTANDT_OUTDOOROVERHANG3, LST_SODIUMSPOT2, 0),

   gszMCLighting, gszSCFloor, L"Simple, style 1",
      &CLSID_Light, &CLSID_LightFloor1,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_ROUNDDIAGCLOTH2DN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 2",
      &CLSID_Light, &CLSID_LightFloor2,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_ROUNDDIAGSTRAIGHTDN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 3",
      &CLSID_Light, &CLSID_LightFloor3,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_SQUAREDIAGCLOTH23DN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 4",
      &CLSID_Light, &CLSID_LightFloor4,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_FANDIAGCLOTH2DN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 5",
      &CLSID_Light, &CLSID_LightFloor5,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_ROUNDOLDFASHCLOTHDN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 6",
      &CLSID_Light, &CLSID_LightFloor6,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_GLASSINCANROUNDCONEDN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 7",
      &CLSID_Light, &CLSID_LightFloor7,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_GLASSINCANSQUARECONEDN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 8",
      &CLSID_Light, &CLSID_LightFloor8,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_GLASSINCANOCTHEMIDN, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 9",
      &CLSID_Light, &CLSID_LightFloor9,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_GLASSINCANOLDFASHIONED, 0),
   gszMCLighting, gszSCFloor, L"Simple, style 10",
      &CLSID_Light, &CLSID_LightFloor10,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLE, LST_GLASSINCANBULB, LSCALE_LARGER),
   gszMCLighting, gszSCFloor, L"Simple, style 11",
      &CLSID_Light, &CLSID_LightFloor11,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPSIMPLETALL, LST_HALOGENBOWL, 0),
   gszMCLighting, gszSCFloor, L"Several lamps, style 1",
      &CLSID_Light, &CLSID_LightFloor12,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPMULTISTALK, LST_INCANSPOT2, 0),
   gszMCLighting, gszSCFloor, L"Several lamps, style 2",
      &CLSID_Light, &CLSID_LightFloor13,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPMULTISTALK, LST_INCANSPOT5, 0),
   gszMCLighting, gszSCFloor, L"Curved, style 1",
      &CLSID_Light, &CLSID_LightFloor14,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPTULIP, LST_HALOGENBOWL, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Curved, style 2",
      &CLSID_Light, &CLSID_LightFloor15,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPTULIP, LST_SQUAREDIAGSTRAIGHT, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Curved, style 3",
      &CLSID_Light, &CLSID_LightFloor16,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPTULIP, LST_ROUNDOLDFASHCLOTH, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Curved, style 4",
      &CLSID_Light, &CLSID_LightFloor17,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPTULIP, LST_GLASSINCANSQUARECONE, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Curved, style 5",
      &CLSID_Light, &CLSID_LightFloor18,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPTULIP, LST_GLASSINCANHEXHEMI, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Rotating arm, style 1",
      &CLSID_Light, &CLSID_LightFloor19,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPARMUP, LST_ROUNDDIAGCLOTH23DN, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Rotating arm, style 2",
      &CLSID_Light, &CLSID_LightFloor20,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPARMUP, LST_GLASSINCANROUNDHEMIDN, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Rotating arm, style 3",
      &CLSID_Light, &CLSID_LightFloor21,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPARMUP, LST_GLASSINCANROUNDCONEDN, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Rotating arm, style 4",
      &CLSID_Light, &CLSID_LightFloor22,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPARMANY, LST_GLASSINCANROUNDCONEDN, LSCALE_23),
   gszMCLighting, gszSCFloor, L"Rotating arm, style 5",
      &CLSID_Light, &CLSID_LightFloor23,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPARMANY, LST_INCANSPOT5, 0),
   gszMCLighting, gszSCFloor, L"Rotating arm, style 6",
      &CLSID_Light, &CLSID_LightFloor24,
      OCLight, LIGHTTYPE(LSTANDT_FLOORLAMPARMANY, LST_INCANSPOT6, 0),

   gszMCLighting, gszSCTable, L"Simple round, style 1",
      &CLSID_Light, &CLSID_LightTable1,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPSIMPLE1, LST_ROUNDDIAGCLOTH2DN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Simple round, style 2",
      &CLSID_Light, &CLSID_LightTable2,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPSIMPLE1, LST_ROUNDOLDFASHCLOTHDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Simple square, style 1",
      &CLSID_Light, &CLSID_LightTable3,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPSIMPLE2, LST_SQUAREDIAGCLOTH2DN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Simple square, style 2",
      &CLSID_Light, &CLSID_LightTable4,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPSIMPLE2, LST_GLASSINCANSQUARECONEDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 1",
      &CLSID_Light, &CLSID_LightTable5,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC1, LST_FANDIAGCLOTH23DN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 2",
      &CLSID_Light, &CLSID_LightTable6,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC1, LST_GLASSINCANROUNDHEMIDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 3",
      &CLSID_Light, &CLSID_LightTable7,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC2, LST_ROUNDDIAGSTRAIGHTDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 4",
      &CLSID_Light, &CLSID_LightTable8,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC2, LST_GLASSINCANOCTHEMIDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 5",
      &CLSID_Light, &CLSID_LightTable9,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC3, LST_FANDIAGCLOTH2DN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 6",
      &CLSID_Light, &CLSID_LightTable10,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC3, LST_FANDIAGSTRAIGHTDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 7",
      &CLSID_Light, &CLSID_LightTable11,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC4, LST_ROUNDDIAGCLOTH2DN, 0),
   gszMCLighting, gszSCTable, L"Ceramic, style 8",
      &CLSID_Light, &CLSID_LightTable12,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC4, LST_GLASSINCANROUNDCONEDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 9",
      &CLSID_Light, &CLSID_LightTable13,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC5, LST_GLASSINCANHEXCONEDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 10",
      &CLSID_Light, &CLSID_LightTable14,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC5, LST_GLASSINCANHEXHEMIDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 11",
      &CLSID_Light, &CLSID_LightTable15,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC6, LST_SQUAREDIAGSTRAIGHTDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Ceramic, style 12",
      &CLSID_Light, &CLSID_LightTable16,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCERAMIC6, LST_GLASSINCANSQUARECONEDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Base, square",
      &CLSID_Light, &CLSID_LightTable17,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPSQUAREBASE, LST_GLASSINCANBOX, LSCALE_LARGER),
   gszMCLighting, gszSCTable, L"Base, cylinder",
      &CLSID_Light, &CLSID_LightTable18,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCYLINDERBASE, LST_GLASSINCANCYLINDER, LSCALE_LARGER),
   gszMCLighting, gszSCTable, L"Base, curved",
      &CLSID_Light, &CLSID_LightTable19,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPCURVEDBASE, LST_GLASSINCANROUNDHEMIDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Base, hexagon",
      &CLSID_Light, &CLSID_LightTable20,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPHEXBASE, LST_GLASSINCANHEXCONEDN, LSCALE_23),
   gszMCLighting, gszSCTable, L"Base, fluted",
      &CLSID_Light, &CLSID_LightTable21,
      OCLight, LIGHTTYPE(LSTANDT_TABLELAMPHORNBASE, LST_GLASSINCANBULB, LSCALE_LARGER),

      
   gszMCLighting, gszSCDesk, L"Arm, style 1",
      &CLSID_Light, &CLSID_LightDesk1,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARM, LST_INCANSPOT1, 0),
   gszMCLighting, gszSCDesk, L"Arm, style 2",
      &CLSID_Light, &CLSID_LightDesk2,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARM, LST_INCANSPOT2, 0),
   gszMCLighting, gszSCDesk, L"Arm, style 3",
      &CLSID_Light, &CLSID_LightDesk3,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARM, LST_INCANSPOT3, 0),
   gszMCLighting, gszSCDesk, L"Arm, style 4",
      &CLSID_Light, &CLSID_LightDesk4,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARM, LST_INCANSPOT5, 0),
   gszMCLighting, gszSCDesk, L"Arm, style 5",
      &CLSID_Light, &CLSID_LightDesk5,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARM, LST_INCANSPOT6, 0),
   gszMCLighting, gszSCDesk, L"Arm, small, style 1",
      &CLSID_Light, &CLSID_LightDesk6,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARMSMALL, LST_INCANSPOT1, LSCALE_23),
   gszMCLighting, gszSCDesk, L"Arm, small, style 2",
      &CLSID_Light, &CLSID_LightDesk7,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARMSMALL, LST_INCANSPOT4, 0),
   gszMCLighting, gszSCDesk, L"Arm, small, style 3",
      &CLSID_Light, &CLSID_LightDesk8,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARMSMALL, LST_INCANSPOT5, 0),
   gszMCLighting, gszSCDesk, L"Arm, small, style 4",
      &CLSID_Light, &CLSID_LightDesk9,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPARMSMALL, LST_INCANSPOT6, 0),
   gszMCLighting, gszSCDesk, L"Arm, flexible style 1",
      &CLSID_Light, &CLSID_LightDesk10,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPFLEXI, LST_INCANSPOT4, 0),
   gszMCLighting, gszSCDesk, L"Arm, flexible style 2",
      &CLSID_Light, &CLSID_LightDesk11,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPFLEXI, LST_ROUNDDIAGCLOTH2, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Stand, style 1",
      &CLSID_Light, &CLSID_LightDesk12,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPSIMPLE, LST_SQUAREDIAGCLOTH23DN, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Stand, style 2",
      &CLSID_Light, &CLSID_LightDesk13,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPSIMPLE, LST_GLASSINCANROUNDCONEDN, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Stand, style 3",
      &CLSID_Light, &CLSID_LightDesk14,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPSIMPLE, LST_GLASSINCANSQUARECONEDN, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Stand, style 4",
      &CLSID_Light, &CLSID_LightDesk15,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPSIMPLE, LST_GLASSINCANHEXHEMIDN, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Stand, curved, style 1",
      &CLSID_Light, &CLSID_LightDesk16,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCURVEDOWN, LST_HALOGENBOWL, LSCALE_23),
   gszMCLighting, gszSCDesk, L"Stand, curved, style 2",
      &CLSID_Light, &CLSID_LightDesk17,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCURVEDOWN, LST_ROUNDDIAGCLOTH23, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Stand, curved, style 3",
      &CLSID_Light, &CLSID_LightDesk18,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCURVEDOWN, LST_GLASSINCANROUNDCONE, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Stand, curved, style 4",
      &CLSID_Light, &CLSID_LightDesk19,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCURVEFRONT, LST_FLOUROSPOT1, 0),
   gszMCLighting, gszSCDesk, L"Stand, curved, style 5",
      &CLSID_Light, &CLSID_LightDesk20,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCURVEFRONT, LST_FLOUROSPOT2, 0),
   gszMCLighting, gszSCDesk, L"Stand, curved, style 6",
      &CLSID_Light, &CLSID_LightDesk21,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCURVEFRONT, LST_INCANSPOT5, 0),
   gszMCLighting, gszSCDesk, L"Stand, curved, style 7",
      &CLSID_Light, &CLSID_LightDesk22,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCURVEFRONT, LST_INCANSPOT6, 0),
   gszMCLighting, gszSCDesk, L"Lava lamp",
      &CLSID_Light, &CLSID_LightDesk23,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPLAVA, LST_LAVALAMP, 0),
   gszMCLighting, gszSCDesk, L"Ceramic, style 1",
      &CLSID_Light, &CLSID_LightDesk24,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCERAMIC1, LST_FANDIAGCLOTH23DN, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Ceramic, style 2",
      &CLSID_Light, &CLSID_LightDesk25,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCERAMIC2, LST_GLASSINCANHEXCONEDN, LSCALE_HALF),
   gszMCLighting, gszSCDesk, L"Ceramic, style 3",
      &CLSID_Light, &CLSID_LightDesk26,
      OCLight, LIGHTTYPE(LSTANDT_DESKLAMPCERAMIC3, LST_SQUAREDIAGCLOTH23DN, LSCALE_HALF),

   gszMCPlants, gszSCConifers, L"Fir",
      &CLSID_Tree, &GUID_TreeConifer1,
      OCTree, (PVOID) TS_CONIFERFIR,
   gszMCPlants, gszSCConifers, L"Cedar",
      &CLSID_Tree, &GUID_TreeConifer2,
      OCTree, (PVOID) TS_CONIFERCEDAR,
   gszMCPlants, gszSCConifers, L"Cypress, leyland",
      &CLSID_Tree, &GUID_TreeConifer3,
      OCTree, (PVOID) TS_CONIFERLEYLANDCYPRESS,
   gszMCPlants, gszSCConifers, L"Cypress, Italian",
      &CLSID_Tree, &GUID_TreeConifer4,
      OCTree, (PVOID) TS_CONIFERITALIANCYPRESS,
   gszMCPlants, gszSCConifers, L"Juniper",
      &CLSID_Tree, &GUID_TreeConifer5,
      OCTree, (PVOID) TS_CONIFERJUNIPER,
   gszMCPlants, gszSCConifers, L"Spruce",
      &CLSID_Tree, &GUID_TreeConifer6,
      OCTree, (PVOID) TS_CONIFERSPRUCE,
   gszMCPlants, gszSCConifers, L"Pine",
      &CLSID_Tree, &GUID_TreeConifer7,
      OCTree, (PVOID) TS_CONIFERPINE,
   gszMCPlants, gszSCConifers, L"Fir, douglas",
      &CLSID_Tree, &GUID_TreeConifer8,
      OCTree, (PVOID) TS_CONIFERDOUGLASFIR,
   gszMCPlants, gszSCConifers, L"Sequoia",
      &CLSID_Tree, &GUID_TreeConifer9,
      OCTree, (PVOID) TS_CONIFERSEQUOIA,

   gszMCPlants, gszSCEverLeaf, L"Acacia",
      &CLSID_Tree, &GUID_TreeEverLeaf1,
      OCTree, (PVOID) TS_EVERLEAFACACIA,
   gszMCPlants, gszSCEverLeaf, L"Eucalypt, Red river gum",
      &CLSID_Tree, &GUID_TreeEverLeaf2,
      OCTree, (PVOID) TS_EVERLEAFEUCALYPTREDRIVER,
   gszMCPlants, gszSCEverLeaf, L"Eucalypt, Salmon gum",
      &CLSID_Tree, &GUID_TreeEverLeaf3,
      OCTree, (PVOID) TS_EVERLEAFEUCALYPTSALMONGUM,
   gszMCPlants, gszSCEverLeaf, L"Melaleuca",
      &CLSID_Tree, &GUID_TreeEverLeaf4,
      OCTree, (PVOID) TS_EVERLEAFMELALEUCA,
   gszMCPlants, gszSCEverLeaf, L"Boab",
      &CLSID_Tree, &GUID_TreeEverLeaf5,
      OCTree, (PVOID) TS_EVERLEAFBOAB,
   gszMCPlants, gszSCEverLeaf, L"Banyan (fig)",
      &CLSID_Tree, &GUID_TreeEverLeaf6,
      OCTree, (PVOID) TS_EVERLEAFBANYAN,
   gszMCPlants, gszSCEverLeaf, L"Citrus",
      &CLSID_Tree, &GUID_TreeEverLeaf7,
      OCTree, (PVOID) TS_EVERLEAFCITRUS,
   gszMCPlants, gszSCEverLeaf, L"Eucalypt, Small",
      &CLSID_Tree, &GUID_TreeEverLeaf8,
      OCTree, (PVOID) TS_EVERLEAFEUCALYPTSMALL,

   gszMCPlants, gszSCPalms, L"Carpentaria",
      &CLSID_Tree, &GUID_TreePalm1,
      OCTree, (PVOID) TS_PALMCARPENTARIA,
   gszMCPlants, gszSCPalms, L"Livistona",
      &CLSID_Tree, &GUID_TreePalm2,
      OCTree, (PVOID) TS_PALMLIVISTONA,
   gszMCPlants, gszSCPalms, L"Date",
      &CLSID_Tree, &GUID_TreePalm3,
      OCTree, (PVOID) TS_PALMDATE,
   gszMCPlants, gszSCMisc, L"Cycad",
      &CLSID_Tree, &GUID_TreePalm4,
      OCTree, (PVOID) TS_PALMCYCAD,
   gszMCPlants, gszSCPalms, L"Sand palm",
      &CLSID_Tree, &GUID_TreePalm5,
      OCTree, (PVOID) TS_PALMSAND,

   gszMCPlants, gszSCDeciduous, L"Maple, sugar",
      &CLSID_Tree, &GUID_TreeDeciduous1,
      OCTree, (PVOID) TS_DECIDUOUSMAPLESUGAR,
   gszMCPlants, gszSCDeciduous, L"Maple, Japanese",
      &CLSID_Tree, &GUID_TreeDeciduous2,
      OCTree, (PVOID) TS_DECIDUOUSMAPLEJAPANESE,
   gszMCPlants, gszSCDeciduous, L"Mimosa",
      &CLSID_Tree, &GUID_TreeDeciduous3,
      OCTree, (PVOID) TS_DECIDUOUSMIMOSA,
   gszMCPlants, gszSCDeciduous, L"Birch",
      &CLSID_Tree, &GUID_TreeDeciduous4,
      OCTree, (PVOID) TS_DECIDUOUSBIRCH,
   gszMCPlants, gszSCDeciduous, L"Dogwood",
      &CLSID_Tree, &GUID_TreeDeciduous5,
      OCTree, (PVOID) TS_DECIDUOUSDOGWOOD,
   gszMCPlants, gszSCDeciduous, L"Cherry",
      &CLSID_Tree, &GUID_TreeDeciduous6,
      OCTree, (PVOID) TS_DECIDUOUSCHERRY,
   gszMCPlants, gszSCDeciduous, L"Oak",
      &CLSID_Tree, &GUID_TreeDeciduous7,
      OCTree, (PVOID) TS_DECIDUOUSOAK,
   gszMCPlants, gszSCDeciduous, L"Beech",
      &CLSID_Tree, &GUID_TreeDeciduous8,
      OCTree, (PVOID) TS_DECIDUOUSBEECH,
   gszMCPlants, gszSCDeciduous, L"Magnolia",
      &CLSID_Tree, &GUID_TreeDeciduous9,
      OCTree, (PVOID) TS_DECIDUOUSMAGNOLIA,
   gszMCPlants, gszSCDeciduous, L"Willow",
      &CLSID_Tree, &GUID_TreeDeciduous10,
      OCTree, (PVOID) TS_DECIDUOUSWILLOW,

   gszMCPlants, gszSCMisc, L"Fern",
      &CLSID_Tree, &GUID_TreeMisc1,
      OCTree, (PVOID) TS_MISCFERN,
   gszMCPlants, gszSCMisc, L"Bamboo",
      &CLSID_Tree, &GUID_TreeMisc2,
      OCTree, (PVOID) TS_MISCBAMBOO,
   gszMCPlants, gszSCMisc, L"Grass, ornamental 1",
      &CLSID_Tree, &GUID_TreeMisc3,
      OCTree, (PVOID) TS_MISCGRASS1,
   gszMCPlants, gszSCMisc, L"Grass, ornamental 2",
      &CLSID_Tree, &GUID_TreeMisc4,
      OCTree, (PVOID) TS_MISCGRASS2,

   gszMCPlants, gszSCShrubs, L"Blueberry",
      &CLSID_Tree, &GUID_TreeShrub1,
      OCTree, (PVOID) TS_SHRUBBLUEBERRY,
   gszMCPlants, gszSCShrubs, L"Boxwood",
      &CLSID_Tree, &GUID_TreeShrub2,
      OCTree, (PVOID) TS_SHRUBBOXWOOD,
   gszMCPlants, gszSCShrubs, L"Quince, flowering",
      &CLSID_Tree, &GUID_TreeShrub3,
      OCTree, (PVOID) TS_SHRUBFLOWERINGQUINCE,
   gszMCPlants, gszSCShrubs, L"Junper, false",
      &CLSID_Tree, &GUID_TreeShrub4,
      OCTree, (PVOID) TS_SHRUBJUNIPERFALSE,
   gszMCPlants, gszSCShrubs, L"Hibiscus",
      &CLSID_Tree, &GUID_TreeShrub5,
      OCTree, (PVOID) TS_SHRUBHIBISCUS,
   gszMCPlants, gszSCShrubs, L"Hydrangea",
      &CLSID_Tree, &GUID_TreeShrub6,
      OCTree, (PVOID) TS_SHRUBHYDRANGEA,
   gszMCPlants, gszSCShrubs, L"Lilac",
      &CLSID_Tree, &GUID_TreeShrub7,
      OCTree, (PVOID) TS_SHRUBLILAC,
   gszMCPlants, gszSCShrubs, L"Rose",
      &CLSID_Tree, &GUID_TreeShrub8,
      OCTree, (PVOID) TS_SHRUBROSE,
   gszMCPlants, gszSCShrubs, L"Rhododendron",
      &CLSID_Tree, &GUID_TreeShrub9,
      OCTree, (PVOID) TS_SHRUBRHODODENDRON,
   gszMCPlants, gszSCShrubs, L"Mugho pine",
      &CLSID_Tree, &GUID_TreeShrub10,
      OCTree, (PVOID) TS_SHRUBMUGHOPINE,
   gszMCPlants, gszSCShrubs, L"Oleander",
      &CLSID_Tree, &GUID_TreeShrub11,
      OCTree, (PVOID) TS_SHRUBOLEANDER,
   gszMCPlants, gszSCShrubs, L"Juniper 2",
      &CLSID_Tree, &GUID_TreeShrub12,
      OCTree, (PVOID) TS_SHRUBJUNIPER2,
   gszMCPlants, gszSCShrubs, L"Juniper 1",
      &CLSID_Tree, &GUID_TreeShrub13,
      OCTree, (PVOID) TS_SHRUBJUNIPER1,

   gszMCPlants, gszSCGroundCover, L"Blue flowers in summer",
      &CLSID_Tree, &GUID_TreeGround1,
      OCTree, (PVOID) TS_ANNUALSUMBLUE,
   gszMCPlants, gszSCGroundCover, L"Red flowers in spring",
      &CLSID_Tree, &GUID_TreeGround2,
      OCTree, (PVOID) TS_ANNUALSPRINGRED,

   gszMCPlants, gszSCPot, L"Conifer",
      &CLSID_Tree, &GUID_TreePot1,
      OCTree, (PVOID) (0x10000 | TS_POTCONIFER),
   gszMCPlants, gszSCPot, L"Shaped 1",
      &CLSID_Tree, &GUID_TreePot2,
      OCTree, (PVOID) (0x10000 | TS_POTSHAPED1),
   gszMCPlants, gszSCPot, L"Shaped 2",
      &CLSID_Tree, &GUID_TreePot3,
      OCTree, (PVOID) (0x10000 | TS_POTSHAPED2),
   gszMCPlants, gszSCPot, L"Leafy",
      &CLSID_Tree, &GUID_TreePot4,
      OCTree, (PVOID) (0x10000 | TS_POTLEAFY),
   gszMCPlants, gszSCPot, L"Corn plant",
      &CLSID_Tree, &GUID_TreePot5,
      OCTree, (PVOID) (0x10000 | TS_POTCORNPLANT),
   gszMCPlants, gszSCPot, L"Succulant",
      &CLSID_Tree, &GUID_TreePot6,
      OCTree, (PVOID) (0x10000 | TS_POTSUCCULANT),
   gszMCPlants, gszSCPot, L"Pointsetia",
      &CLSID_Tree, &GUID_TreePot7,
      OCTree, (PVOID) (0x10000 | TS_POTPOINTSETIA),
   gszMCPlants, gszSCPot, L"Fern",
      &CLSID_Tree, &GUID_TreePot8,
      OCTree, (PVOID) (0x10000 | (DWORD) TS_MISCFERN),
   gszMCPlants, gszSCPot, L"Grass, ornamental 1",
      &CLSID_Tree, &GUID_TreePot9,
      OCTree, (PVOID) (0x10000 | (DWORD) TS_MISCGRASS1),
   gszMCPlants, gszSCPot, L"Grass, ornamental 2",
      &CLSID_Tree, &GUID_TreePot10,
      OCTree, (PVOID) (0x10000 | (DWORD) TS_MISCGRASS2),

   gszMCLandscaping, gszSCFencing, L"Picket fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceVertPicket,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEVERTPICKET),
   gszMCLandscaping, gszSCFencing, L"Steel-verticals fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceVertSteel,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEVERTSTEEL),
   gszMCLandscaping, gszSCFencing, L"Wrought iron fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceVertWroughtIron,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEVERTWROUGHTIRON),
   gszMCLandscaping, gszSCFencing, L"Picket fence, small",
      &CLSID_Balustrade, &CLSID_BalustradeFenceVertPicketSmall,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEVERTPICKETSMALL),
   gszMCLandscaping, gszSCFencing, L"Vertical-board fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceVertPanel,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEVERTPANEL),
   gszMCLandscaping, gszSCFencing, L"Log fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceHorzLog,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEHORZLOG),
   gszMCLandscaping, gszSCFencing, L"Stick fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceHorzStick,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEHORZSTICK),
   gszMCLandscaping, gszSCFencing, L"Horizontal-board fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceHorzPanels,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEHORZPANELS),
   gszMCLandscaping, gszSCFencing, L"Wire fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceHorzWire,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEHORZWIRE),
   gszMCLandscaping, gszSCFencing, L"Masonry wall",
      &CLSID_Balustrade, &CLSID_BalustradeFencePanelSolid,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEPANELSOLID),
   gszMCLandscaping, gszSCFencing, L"Chain-mesh fence",
      &CLSID_Balustrade, &CLSID_BalustradeFencePanelPole,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEPANELPOLE),
   gszMCLandscaping, gszSCFencing, L"X-brace fence",
      &CLSID_Balustrade, &CLSID_BalustradeFenceBraceX,
      OCBalustrade, (PVOID) (0x10000 | BS_FENCEBRACEX),

   gszMCBuildBlock, gszSCStairs, L"Entry stairs",
      &CLSID_Stairs, &CLSID_StairsEntry,
      OCStairs, (PVOID) (SPATH_STRAIGHT | 0x10000),
   gszMCBuildBlock, gszSCStairs, L"Straight stairs",
      &CLSID_Stairs, &CLSID_StairsStraight,
      OCStairs, (PVOID) SPATH_STRAIGHT,
   gszMCBuildBlock, gszSCStairs, L"Stairs turning on a landing",
      &CLSID_Stairs, &CLSID_StairsLandingTurn,
      OCStairs, (PVOID) SPATH_LANDINGTURN,
   gszMCBuildBlock, gszSCStairs, L"Stairwell",
      &CLSID_Stairs, &CLSID_StairsStairwell,
      OCStairs, (PVOID) SPATH_STAIRWELL,
   gszMCBuildBlock, gszSCStairs, L"Stairwell 2",
      &CLSID_Stairs, &CLSID_StairsStairwell2,
      OCStairs, (PVOID) SPATH_STAIRWELL2,
   gszMCBuildBlock, gszSCStairs, L"Spiral staircase",
      &CLSID_Stairs, &CLSID_StairsSpiral,
      OCStairs, (PVOID) SPATH_SPIRAL,
   gszMCBuildBlock, gszSCStairs, L"Winding staircase",
      &CLSID_Stairs, &CLSID_StairsWinding,
      OCStairs, (PVOID) SPATH_WINDING,

   gszMCLandscaping, gszSCGround, L"Ground topography square",
      &CLSID_Ground, &GUID_NULL,
      OCGround, (PVOID) 0,

//   gszMCTest, L"Camera", L"Camera",
//      &CLSID_Camera, &GUID_NULL,
//      OCCamera, NULL,

   gszMCBasicShapes, gszSCExtrusions, L"Extrusion",
      &CLSID_Noodle, &GUID_NULL,
      OCNoodle, NULL,
   gszMCBasicShapes, gszSCExtrusions, L"Column",
      &CLSID_Column, &GUID_NULL,
      OCColumn, NULL,
   //L"Test", L"Teapot", L"Teapot",
   //   &CLSID_TEAPOT, &GUID_NULL,
   //   OCTeapot, NULL,
};

/**********************************************************************************
HackObjectsCreate - Temproary function to populate the objects list.
*/
void HackObjectsCreate (void)
{
   DWORD i, dwNum;
   dwNum = sizeof(gaObjectRecord) / sizeof(OBJECTRECORD);
   for (i = 0; i < dwNum; i++) {
      POBJECTRECORD pt = &gaObjectRecord[i];

      PCMMLNode2 pNode;
      pNode = new CMMLNode2;
      if (!pNode)
         continue;
      pNode->NameSet (gpszObject);
      MMLValueSet (pNode, gpszObjectByID, (int) pt->pParams);

      AttachDateTimeToMML(pNode);

      // make sure name is unique
      WCHAR szMajor[128], szMinor[128], szName[128];
      wcscpy (szMajor, pt->pszMajor);
      wcscpy (szMinor, pt->pszMinor);
      wcscpy (szName, pt->pszName);
      UniqueObjectName (szMajor, szMinor, szName);

      LibraryObjects(FALSE)->ItemAdd (szMajor, szMinor, szName,
         pt->pCode, pt->pSub, pNode);
   }
}
#endif // 0 - no longer used since getting objects from  file

/***********************************************************************************
ObjectCFEnumMajor - Enumerates the major category names for objects.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
returns
   none
*/
void ObjectCFEnumMajor (DWORD dwRenderShard, PCListFixed pl)
{
   // NOTE: Always sorting the list
   CListFixed l2;
   LibraryObjects(dwRenderShard, FALSE)->EnumMajor (pl);
   LibraryObjects(dwRenderShard, TRUE)->EnumMajor(&l2);
   LibraryCombineLists (pl, &l2);
}


/***********************************************************************************
ObjectCFEnumMinor - Enumerates the minor category names for Objects.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
   PWSTR          pszMajor - Major category name
returns
   none
*/
void ObjectCFEnumMinor (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor)
{
   CListFixed l2;
   LibraryObjects(dwRenderShard, FALSE)->EnumMinor (pl, pszMajor);
   LibraryObjects(dwRenderShard, TRUE)->EnumMinor (&l2, pszMajor);
   LibraryCombineLists (pl, &l2);
}

/***********************************************************************************
ObjectCFEnumItems - Enumerates the items of a minor category in Objects.

inputs
   PCListFixed    pl- List to be filled with, List of PWSTR that point to the category
      names. DO NOT change the strings. This IS sorted.
   PWSTR          pszMajor - Major category name
   PWSTR          pszMinor - Minor category name
returns
   none
*/
void ObjectCFEnumItems (DWORD dwRenderShard, PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor)
{
   CListFixed l2;
   LibraryObjects(dwRenderShard, FALSE)->EnumItems (pl, pszMajor, pszMinor);
   LibraryObjects(dwRenderShard, TRUE)->EnumItems (&l2, pszMajor, pszMinor);
   LibraryCombineLists (pl, &l2);
}

/***********************************************************************************
ObjectCFGUIDsFromName - Given a Object name, retuns the GUIDs

inputs
   PWSTR          pszMajor - Major category name
   PWSTR          pszMinor - Minor category name
   PWSTR          pszName - Name of the Object
   GUID           *pgCode - Filled with the code GUID if successful
   GUID           *pgSub - Filled with the sub GUID if successful
returns
   BOOL - TRUE if find
*/
BOOL ObjectCFGUIDsFromName (DWORD dwRenderShard, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub)
{
   if (LibraryObjects(dwRenderShard, FALSE)->ItemGUIDFromName (pszMajor, pszMinor, pszName, pgCode, pgSub))
      return TRUE;
   if (LibraryObjects(dwRenderShard, TRUE)->ItemGUIDFromName (pszMajor, pszMinor, pszName, pgCode, pgSub))
      return TRUE;
   return FALSE;
}

/***********************************************************************************
ObjectCFNameFromGUIDs - Given a Objects GUIDs, this fills in a buffer with
its name strings.

inputs
   GUID           *pgCode - Code guid
   GUID           *pgSub - Sub guid
   PWSTR          pszMajor - Filled with Major category name
   PWSTR          pszMinor - Filled with Minor category name
   PWSTR          pszName - Filled with Name of the Object
returns
   BOOL - TRUE if find
*/
BOOL ObjectCFNameFromGUIDs (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub,PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   if (LibraryObjects(dwRenderShard, FALSE)->ItemNameFromGUID (pgCode, pgSub, pszMajor, pszMinor, pszName))
      return TRUE;
   if (LibraryObjects(dwRenderShard, TRUE)->ItemNameFromGUID (pgCode, pgSub, pszMajor, pszMinor, pszName))
      return TRUE;
   return FALSE;
}

/***********************************************************************************
ObjectCFInit - Call in winmain to initialize
*/
void ObjectCFInit (void)
{
   // do nothing for now
}

/***********************************************************************************
ObjectCFEnd - Call before leaving to free up any cached Objects
*/
void ObjectCFEnd (void)
{
   // do nothing for now
   // NOTE: Getting called after LibrarySaveFiles()
}

/***********************************************************************************
ObjectCFCreate - Creates a new Object object based on the GUIDs.

inputs
   GUID     *pCode - Code guid
   GUID     *pSub - sub guid. If this is NLL then it takes the first instance of
               pCode that appears
returns
   PCObjectSocket - New object. NULL if error
*/
PCObjectSocket ObjectCFCreate (DWORD dwRenderShard, const GUID *pCode, const GUID *pSub)
{
   PCMMLNode2 pNode;
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!ObjectCFNameFromGUIDs (dwRenderShard, (GUID*) pCode, (GUID*) pSub, szMajor, szMinor, szName))
      return NULL;
   pNode = LibraryObjects(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
   if (!pNode)
      pNode = LibraryObjects(dwRenderShard, TRUE)->ItemGet  (szMajor, szMinor, szName);
   if (!pNode)
      return NULL;

   // get the ID
   DWORD dwID;
   dwID = (int) MMLValueGetInt (pNode, gpszObjectByID, 0);

   // what is the creator function
   PCObjectSocket pNew;
   OSINFO OI;
   pNew = NULL;
   memset (&OI, 0, sizeof(OI));
   OI.gCode = *pCode;
   OI.gSub = *pSub;
   OI.dwRenderShard = dwRenderShard;
   if (IsEqualGUID (*pCode, CLSID_TEAPOT))
      pNew = new CObjectTeapot ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Grass))
      pNew = new CObjectGrass ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Rock))
      pNew = new CObjectRock ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Cave))
      pNew = new CObjectCave ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Waterfall))
      pNew = new CObjectWaterfall ((PVOID) (size_t)  dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Tooth))
      pNew = new CObjectTooth ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Jaw))
      pNew = new CObjectJaw ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Branch))
      pNew = new CObjectBranch ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_HairLock))
      pNew = new CObjectHairLock ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_HairHead))
      pNew = new CObjectHairHead ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Eye))
      pNew = new CObjectEye ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Leaf))
      pNew = new CObjectLeaf ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Truss))
      pNew = new CObjectTruss ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Skydome))
      pNew = new CObjectSkydome ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_AnimCamera))
      pNew = new CObjectAnimCamera ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Bone))
      pNew = new CObjectBone ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Pool))
      pNew = new CObjectPool ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Fireplace))
      pNew = new CObjectFireplace ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_CeilingFan))
      pNew = new CObjectCeilingFan ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Tarp))
      pNew = new CObjectTarp ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Pathway))
      pNew = new CObjectPathway ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Drawer))
      pNew = new CObjectDrawer ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Books))
      pNew = new CObjectBooks ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_TableCloth))
      pNew = new CObjectTableCloth ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Cushion))
      pNew = new CObjectCushion ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Curtain))
      pNew = new CObjectCurtain ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Painting))
      pNew = new CObjectPainting ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Polyhedron))
      pNew = new CObjectPolyhedron ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_PolyMesh))
      pNew = new CObjectPolyMesh ((PVOID) (size_t) dwID, &OI);
//   else if (IsEqualGUID (*pCode, CLSID_PolyMeshOld))
//      pNew = new CObjectPolyMeshOld ((PVOID) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Light))
      pNew = new CObjectLight ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_LightGeneric))
      pNew = new CObjectLightGeneric ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Revolution))
      pNew = new CObjectRevolution ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Noodle))
      pNew = new CObjectNoodle ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Column))
      pNew = new CObjectColumn ((PVOID) (size_t) dwID, &OI);
#if 0 // DEAD code
   else if (IsEqualGUID (*pCode, CLSID_Tree))
      pNew = new CObjectTree ((PVOID) dwID, &OI);
#endif // 0
   else if (IsEqualGUID (*pCode, CLSID_StructSurface))
      pNew = new CObjectStructSurface ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_SingleSurface))
      pNew = new CObjectSingleSurface ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Balustrade))
      pNew = new CObjectBalustrade ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Stairs))
      pNew = new CObjectStairs ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Piers))
      pNew = new CObjectPiers ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_BuildBlock))
      pNew = new CObjectBuildBlock ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Cabinet))
      pNew = new CObjectCabinet ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Door))
      pNew = new CObjectDoor ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_UniHole))
      pNew = new CObjectUniHole ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_Ground))
      pNew = new CObjectGround ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_ObjEditor))
      pNew = new CObjectEditor ((PVOID) (size_t) dwID, &OI);
   else if (IsEqualGUID (*pCode, CLSID_ObjTemplate)) {
      pNew = MMLToObject (dwRenderShard, pNode);
      if (!pNew)
         goto done;

      GUID g;
      if (pNew->EmbedContainerGet (&g))
         pNew->EmbedContainerSet(NULL);

      // reset location
      CMatrix m;
      m.Identity();
      pNew->ObjectMatrixSet (&m);
   }
   // NOTE: Not allowing to do new CObjectCamera()

done:
   if (!pNew)
      return NULL;

   // BUGFIX - Set the name
   pNew->StringSet (OSSTRING_NAME, szName);
   pNew->StringSet (OSSTRING_GROUP, szMajor);

   return pNew;
}


/****************************************************************************
ObjectCreateThumbnail - Create a thumbnail from an object. This is added to
the thumbnail list.

inputs
   GUID        *pgMajor - Major GUID
   GUID        *pgMinor - Minor GUID
   PCRenderTraditional pRender - To draw to
   PCWorldSocket     pWorld - World to use for scratch
   PCImage     pOrig - To draw to
   GUID        *pgEffectCode - If not NULL then use NPR effect
   GUID        *pgEffectSub - If not NULL then use NPR effect
returns
   BOOL - TRUE if success
*/
BOOL ObjectCreateThumbnail (GUID *pgMajor, GUID *pgMinor, PCRenderTraditional pRender,
                            PCWorldSocket pWorld, PCImage pOrig,
                            GUID *pgEffectCode, GUID *pgEffectSub)
{
   pWorld->Clear();

   PCObjectSocket pNew;
   DWORD dwRenderShard = pWorld->RenderShardGet();
   pNew = ObjectCFCreate (dwRenderShard, pgMajor, pgMinor);
   if (!pNew)
      return FALSE;  // error that shouldnt happen
   pWorld->ObjectAdd (pNew);
   pNew->WorldSetFinished();  // do this now since wont contain anything

   // find the object's bounding box and move it to the center of the world
   CMatrix m2;
   CPoint apCorner[2], pSum;
   pNew->QueryBoundingBox (&apCorner[0], &apCorner[1], (DWORD)-1);
   pSum.Add (&apCorner[0], &apCorner[1]);
   pSum.Scale (.5);
   pNew->ObjectMatrixGet (&m2);
   pSum.MultiplyLeft (&m2);
   CPoint pCenter, pSize;
   pCenter.Copy (&pSum);

   // find the size of the object
   pSum.Subtract (&apCorner[0], &apCorner[1]);
   pSize.p[0] = fabs(pSum.p[0]);
   pSize.p[1] = fabs(pSum.p[1]);
   pSize.p[2] = fabs(pSum.p[2]);
   fp fSize, fLat, fLong;
   fSize = pSum.Length();

   // BUGFIX - Round up
   //fSize *= 1.2;  // BUGFIX - Since some off edge
   fSize = MeasureFindScale (fSize);

   // reset rotation
   fLat = PI/8;
   fLong = PI/8;

   // how far away does it need to be put to be visible?
   fp fDist;
   fp fFOV;
   fFOV = PI / 4;
   fDist = fSize / tan(fFOV / 2.0) / 2;

   // set the camera
   CPoint pTrans;
   pTrans.Zero();
   pTrans.p[1] = fDist;
   pRender->CameraPerspObject (&pTrans, &pCenter, fLong, fLat, 0, fFOV);

   // draw it
   pRender->BackgroundSet(RGB (0x80, 0x80, 0xc0));   // BUGFIX - NOt black RGB(0x0, 0x0, 0x0);
   // BUGFIX - Took out removal of RENDERSHOW_WEATHER
   pRender->RenderShowSet (pRender->RenderShowGet() & (~(RENDERSHOW_VIEWCAMERA)));  // BUGFIX - so dont draw the sun

   // if doing post processing then make sure to turn off outlineing and fog
   if (pgEffectCode) {
      if (pRender->m_pEffectFog)
         delete pRender->m_pEffectFog;
      if (pRender->m_pEffectOutline)
         delete pRender->m_pEffectOutline;
      pRender->m_pEffectFog = NULL;
      pRender->m_pEffectOutline = NULL;
   }

   //pRender->m_dwOutline = 0;
   pRender->Render (-1, NULL, NULL);

   // if doing post processing then apply effect
   if (pgEffectCode) {
      PCNPREffectsList pe = EffectCreate (dwRenderShard, pgEffectCode, pgEffectSub);
      if (pe) {
         pe->Render (pOrig, pRender, pWorld, TRUE, NULL);
         delete pe;
      }
   }

   // create a smaller image
   CImage  Image;
#define DOWNSAMPLE      4
   Image.Init (pOrig->Width() / DOWNSAMPLE, pOrig->Height() / DOWNSAMPLE);

   // loop
   DWORD x,y,xx,yy;
   for (x = 0; x < Image.Width(); x++) for (y = 0; y < Image.Height(); y++) {
      PIMAGEPIXEL pipNew = Image.Pixel(x,y);
      pipNew->wRed = pipNew->wGreen = pipNew->wBlue = 0;

      BOOL  fFoundObject;
      fFoundObject = FALSE;

      for (xx = 0; xx < DOWNSAMPLE; xx++) for (yy = 0; yy < DOWNSAMPLE; yy++) {
         PIMAGEPIXEL pipOrig = pOrig->Pixel(x*DOWNSAMPLE + xx, y * DOWNSAMPLE + yy);
         if (pipOrig->dwID)
            fFoundObject = TRUE;
         pipNew->wRed += pipOrig->wRed / DOWNSAMPLE / DOWNSAMPLE;
         pipNew->wGreen += pipOrig->wGreen / DOWNSAMPLE / DOWNSAMPLE;
         pipNew->wBlue += pipOrig->wBlue / DOWNSAMPLE / DOWNSAMPLE;
      }

      if (!pgEffectCode) {
         // convert to 8-bit and back to make sure only one color is background
         COLORREF cr;
         cr = Image.UnGamma (&pipNew->wRed);

         // in an effect use entire image
         if (fFoundObject) {
            if (cr == 0)
               cr = RGB(0x01,0x01,0x01);  // dont allow black
         }
         else {
            cr = 0;
         }
         Image.Gamma (cr, &pipNew->wRed);
      }
   }

   if (!pgEffectCode) {
      // apply the text
      WCHAR szTemp[64];
      MeasureToString (fSize, szTemp);
      DrawTextOnImage (&Image, 1, -1, NULL, szTemp);
   }

   // cache this
   ThumbnailGet()->ThumbnailAdd (
      pgEffectCode ? pgEffectCode : pgMajor,
      pgEffectSub ? pgEffectSub : pgMinor,
      &Image, pgEffectCode ? -1 : 0);
   

   return TRUE;
}

/****************************************************************************
Thumbnail - Given the GUIDS, and window to use for HDC, returns a HBITMAP
to the thumbnail. (If the thumbnail doesn't exist it's created.) The caller
MUST destroy the bitmap

inputs
   GUID        *pgMajor - Major GUID
   GUID        *pgMinor - Minor GUID
   HWND        hWnd - WIndow for HDC
   COLORREF    *pcrBackground - Background color, -1 if no background
returns
   HBITMAP - Bitmap to use. Must be freed by caller.
*/
HBITMAP Thumbnail (DWORD dwRenderShard, GUID *pgMajor, GUID *pgMinor, HWND hWnd, COLORREF *pcrBackground)
{
   HBITMAP hBit;

   hBit = ThumbnailGet()->ThumbnailToBitmap (pgMajor, pgMinor,hWnd, pcrBackground);
   if (hBit)
      return hBit;

   // else no bitmap created
   {
      CImage   Image;
      PCRenderTraditional pRender = new CRenderTraditional (dwRenderShard);
      if (!pRender)
         return NULL;

      CWorld   World;
      World.RenderShardSet (dwRenderShard);
      Image.Init (OBJECTTHUMBNAIL * 4, OBJECTTHUMBNAIL * 4, RGB(0xff,0xff,0xff));
      pRender->CImageSet (&Image);
      pRender->CWorldSet (&World);

      // Need to set to summer so that leaves will be out, also cntrols sun
      WCHAR szTemp[64];
      swprintf (szTemp, L"%d", TODFDATE(1,7,YEARFROMDFDATE(TodayFast())+1));
      World.VariableSet (WSDate(), szTemp);
      swprintf (szTemp, L"%g", (double)(PI / 4));  // BUGFIX - Northern hemisphere to sun in south
      World.VariableSet (WSLatitude(), szTemp);
      swprintf (szTemp, L"%d", (int) TODFTIME(15,00));   // BUGFIX - 3:00 so west
      World.VariableSet (WSTime(), szTemp);

      BOOL fRet = ObjectCreateThumbnail (pgMajor, pgMinor, pRender, &World, &Image);

      delete pRender;
      if (!fRet)
         return NULL;
   }

   // return what we have
   return ThumbnailGet()->ThumbnailToBitmap (pgMajor, pgMinor,hWnd, pcrBackground);
}



typedef struct {
   PCWorldSocket     pWorld;   // world to use it in
   PCRenderTraditional pRender;   // renderer
   GUID        gCode;      // id GUID
   GUID        gSub;       // id GUID
   WCHAR       szMajor[128];   // major category name
   WCHAR       szMinor[128];   // minor category name
   WCHAR       szName[128];    // name
   PCImage     pImage;     // image usaing
   //HBITMAP     hBit;       // bitmap of the image - 200 x 200
   WCHAR       szCurMajorCategory[128]; // current major category shown
   WCHAR       szCurMinorCategory[128]; // current minor cateogyr shown
   fp      fLong, fLat;   // latitude and longitude
   fp      fSize;         // size of the object
   CPoint      pCenter;       // center of the object
   CPoint      pSize;         // size to display
   int         iVScroll;      // where to scroll to
   PCListFixed pThumbInfo;    // for whichimages there
   DWORD       dwTimerID;     // for page
} ONPAGE, *PONPAGE;

typedef struct {
   HBITMAP     hBitmap;    // bitmap used
   BOOL        fEmpty;     // if fEmpty is set then the bitmap used is a placeholder
   PWSTR       pszName;    // name of the object - do not modify
   COLORREF    cTransparent;  // if not -1 then this is the transparent color
} THUMBINFO, *PTHUMBINFO;

BOOL ObjectNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PONPAGE pt = (PONPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->pWorld->RenderShardGet();

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // add enter - close
         ESCACCELERATOR a;
         memset (&a, 0, sizeof(a));
         a.c = VK_ESCAPE;
         a.fAlt = FALSE;
         a.dwMessage = ESCM_CLOSE;
         pPage->m_listESCACCELERATOR.Add (&a);

         // Handle scroll on redosamepage
         if (pt->iVScroll >= 0) {
            pPage->VScroll (pt->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pt->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // set off time if any images are unfinished
         DWORD i;
         PTHUMBINFO pti;
         pt->dwTimerID = 0;
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i < pt->pThumbInfo->Num())
            pt->dwTimerID = pPage->m_pWindow->TimerSet (100, pPage); // first one right away
      }
      break;

   case ESCM_DESTRUCTOR:
      // kill the timer
      if (pt->dwTimerID)
         pPage->m_pWindow->TimerKill (pt->dwTimerID);
      pt->dwTimerID = 0;
      break;

   case ESCM_TIMER:
      {
         pPage->m_pWindow->TimerKill (pt->dwTimerID);
         pt->dwTimerID = 0;

         DWORD i;
         PTHUMBINFO pti;
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i >= pt->pThumbInfo->Num())
            return TRUE;

         // BUGFIX - Clear the schemes otherwise have same scheme in every image
         PCSurfaceSchemeSocket pss;
         pss = pt->pWorld->SurfaceSchemeGet ();
         if (pss)
            pss->Clear();
   
         // create
         pPage->m_pWindow->SetCursor (IDC_NOCURSOR);
         GUID gCode, gSub;
         ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pti->pszName, &gCode, &gSub);
         ObjectCreateThumbnail (&gCode, &gSub, pt->pRender, pt->pWorld, pt->pImage);
         COLORREF cTransparent;
         pti->hBitmap = ThumbnailGet()->ThumbnailToBitmap (&gCode, &gSub, pPage->m_pWindow->m_hWnd,
            &cTransparent);
         if (pti->hBitmap) {
            pti->fEmpty = FALSE;

            WCHAR szTemp[32];
            swprintf (szTemp, L"bitmap%d", (int) i);
            PCEscControl pControl;
            pControl = pPage->ControlFind (szTemp);
            swprintf (szTemp, L"%lx", (__int64) pti->hBitmap);
            if (pControl)
               pControl->AttribSet (L"hbitmap", szTemp);
         }
         pPage->m_pWindow->SetCursor (IDC_HANDCURSOR);

         // set another timer
         for (i = 0; i < pt->pThumbInfo->Num(); i++) {
            pti = (PTHUMBINFO) pt->pThumbInfo->Get(i);
            if (pti->fEmpty && !pti->hBitmap)
               break;
         }
         if (i < pt->pThumbInfo->Num())
            pt->dwTimerID = pPage->m_pWindow->TimerSet (250, pPage);
      }
      return TRUE;

   }

   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
ObjectCFNewDialog - This brings up the UI for changing CObjectSurface. It's
passed in an object surface that's manupulated (and changed). Retrusn TRUE
if the user presses OK, FALSE if they press cancel. If the user presses OK
the object surface should probably be set.

inputs
   HWND           hWnd - Window to create this over
   GUID           *pgCode - Filled in with selected object guid if user pressed OK
   GUID           *pgSub - Filled in with selected obhect guid if user presses OK
returns
   BOOL - TRUE if the user presses OK, FALSE if not
*/
BOOL ObjectCFNewDialog (DWORD dwRenderShard, HWND hWnd, GUID *pgCode, GUID *pgSub)
{
   // BUGFIX - Remember last settings
   static WCHAR szMajor[128] = L"Building blocks";
   static WCHAR szMinor[128] = L"House blocks (common)";
   static WCHAR szName[128] = L"";

   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation2 (hWnd, &r);

   CImage   Image;
   PCRenderTraditional pRender = new CRenderTraditional(dwRenderShard);
   if (!pRender)
      return FALSE;
   CWorld   World;
   World.RenderShardSet (dwRenderShard);
   Image.Init (OBJECTTHUMBNAIL * 4, OBJECTTHUMBNAIL * 4, RGB(0xff,0xff,0xff));
   pRender->CImageSet (&Image);
   pRender->CWorldSet (&World);
   // BUGFIX - Ignore this
   //if (pScheme)
   //   World.SurfaceSchemeSet (pScheme);

   // set the floor levels in purgatory
   fp afElev[NUMLEVELS], fHigher;
   GlobalFloorLevelsGet (WorldGet(dwRenderShard, NULL), NULL, afElev, &fHigher);
   GlobalFloorLevelsSet (&World, NULL, afElev, fHigher);

   // Need to set to summer so that leaves will be out
   WCHAR szTemp[64];
   swprintf (szTemp, L"%d", TODFDATE(1,1,YEARFROMDFDATE(TodayFast())+1));
   World.VariableSet (WSDate(), szTemp);
   swprintf (szTemp, L"%g", (double)(-PI / 4));
   World.VariableSet (WSLatitude(), szTemp);

   // how many columns
   DWORD dwColumns;
   dwColumns = (DWORD)max(r.right - r.left,0) / (OBJECTTHUMBNAIL * 5 / 4);
   dwColumns = max(dwColumns, 3);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up the info
   ONPAGE t;
   memset (&t, 0, sizeof(t));
   t.pRender = pRender;
   t.pWorld = &World;
   t.pImage = &Image;
   wcscpy (t.szMajor, szMajor);
   wcscpy (t.szMinor, szMinor);
   wcscpy (t.szName, szName);

   //HDC hDC;
   //hDC = GetDC (hWnd);
   //if (t.hBit)
   //   DeleteObject (t.hBit);
   //t.hBit = t.pImage->ToBitmap (hDC);
   //ReleaseDC (hWnd, hDC);

   CListFixed lThumbInfo;
   lThumbInfo.Init (sizeof(THUMBINFO));
   t.pThumbInfo = &lThumbInfo;

redopage:
   // create a bitmap for error
   HBITMAP hBlank;
   HDC hDCBlank, hDCWnd;
   hDCWnd = GetDC (hWnd);
   hDCBlank = CreateCompatibleDC (hDCWnd);
   hBlank = CreateCompatibleBitmap (hDCWnd, OBJECTTHUMBNAIL, OBJECTTHUMBNAIL);
   SelectObject (hDCBlank, hBlank);
   ReleaseDC (hWnd, hDCWnd);
   RECT rBlank;
   rBlank.left = rBlank.top = 0;
   rBlank.right = rBlank.bottom = OBJECTTHUMBNAIL;
   FillRect (hDCBlank, &rBlank, (HBRUSH) GetStockObject (BLACK_BRUSH));
   // draw the text
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv(12, GetDeviceCaps(hDCBlank, LOGPIXELSY), 72); 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   SelectObject (hDCBlank, hFont);
   SetTextColor (hDCBlank, RGB(0xff,0xff,0xff));
   SetBkMode (hDCBlank, TRANSPARENT);
   DrawText(hDCBlank, "Drawing...", -1, &rBlank, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_SINGLELINE);
   DeleteObject (hFont);
   DeleteDC (hDCBlank);

   // create the page
   CListFixed  lNames;
   DWORD i;
   PWSTR psz;
   BOOL fBold;
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<?Include resource=500?>"
            L"<PageInfo index=false title=\"Add a new object\"/>"
            L"<colorblend tcolor=#000040 bcolor=#000080 posn=background/>"
            L"<font color=#ffffff><table width=100%% innerlines=0 bordercolor=#c0c0c0 valign=top><tr>");

   // major category
   MemCat (&gMemTemp, L"<td bgcolor=#000000 lrmargin=0 tbmargin=0 width=");
   MemCat (&gMemTemp, (int) 100 / (int) dwColumns);
   MemCat (&gMemTemp, L"%%>");
   MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
   ObjectCFEnumMajor (dwRenderShard, &lNames);
   // make sure major is valid
   for (i = 0; i < lNames.Num(); i++) {
      psz = *((PWSTR*) lNames.Get(i));
      if (!_wcsicmp(psz, t.szMajor))
         break;
   }
   if (i >= lNames.Num())
      wcscpy (t.szMajor, *((PWSTR*) lNames.Get(0)));
   for (i = 0; i < lNames.Num(); i++) {
      psz = *((PWSTR*) lNames.Get(i));

      MemCat (&gMemTemp, L"<tr><td");
      if (!_wcsicmp(psz, t.szMajor)) {
         MemCat (&gMemTemp, L" bgcolor=#202040");
         fBold = TRUE;
      }
      else
         fBold = FALSE;

      MemCat (&gMemTemp, L">");
      MemCat (&gMemTemp, L"<a href=\"mcat:");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"\">");
      if (fBold)
         MemCat (&gMemTemp, L"<font color=#ffffff>");
      MemCatSanitize (&gMemTemp, psz);
      if (fBold)
         MemCat (&gMemTemp, L"</font>");
      MemCat (&gMemTemp, L"</a>");
      MemCat (&gMemTemp, L"</td></tr>");
   }
   MemCat (&gMemTemp, L"</table>");
   MemCat (&gMemTemp, L"</td>");

   // minor categories
   MemCat (&gMemTemp, L"<td lrmargin=0 bgcolor=#202040 tbmargin=0 width=");
   MemCat (&gMemTemp, (int) 100 / (int) dwColumns);
   MemCat (&gMemTemp, L"%%>");
   MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
   ObjectCFEnumMinor (dwRenderShard, &lNames, t.szMajor);
   // make sure minor is valid
   for (i = 0; i < lNames.Num(); i++) {
      psz = *((PWSTR*) lNames.Get(i));
      if (!_wcsicmp(psz, t.szMinor))
         break;
   }
   if ((i >= lNames.Num()) && lNames.Num())
      wcscpy (t.szMinor, *((PWSTR*) lNames.Get(0)));
   for (i = 0; i < lNames.Num(); i++) {
      psz = *((PWSTR*) lNames.Get(i));

      MemCat (&gMemTemp, L"<tr><td");
      if (!_wcsicmp(psz, t.szMinor)) {
         MemCat (&gMemTemp, L" bgcolor=#8080c0");
         fBold = TRUE;
      }
      else
         fBold = FALSE;

      MemCat (&gMemTemp, L">");
      MemCat (&gMemTemp, L"<a href=\"icat:");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"\">");
      if (fBold)
         MemCat (&gMemTemp, L"<font color=#ffffff>");
      MemCatSanitize (&gMemTemp, psz);
      if (fBold)
         MemCat (&gMemTemp, L"</font>");
      MemCat (&gMemTemp, L"</a>");
      MemCat (&gMemTemp, L"</td></tr>");
   }
   MemCat (&gMemTemp, L"</table>");
   MemCat (&gMemTemp, L"</td>");

   // objects within major and minor
   MemCat (&gMemTemp, L"<td lrmargin=0 tbmargin=0 bgcolor=#8080c0 width=");
   MemCat (&gMemTemp, (int) 100 * (int)(dwColumns - 2) / (int) dwColumns);
   MemCat (&gMemTemp, L"%%>");
   MemCat (&gMemTemp, L"<table width=100%% border=0 innerlines=0>");
   ObjectCFEnumItems (dwRenderShard, &lNames, t.szMajor, t.szMinor);
   // make sure minor is valid
   for (i = 0; i < lNames.Num(); i++) {
      psz = *((PWSTR*) lNames.Get(i));
      if (!_wcsicmp(psz, t.szName))
         break;
   }
   if ((i >= lNames.Num()) && lNames.Num())
      wcscpy (t.szName, *((PWSTR*) lNames.Get(0)));
   // fill in the list of bitmaps
   lThumbInfo.Clear();
   DWORD j;
   for (i = 0; i < lNames.Num(); ) {
      MemCat (&gMemTemp, L"<tr>");
      for (j = 0; j < dwColumns-2; j++) {
         MemCat (&gMemTemp, L"<td width=");
         MemCat (&gMemTemp, (int) 100 / (int)(dwColumns - 2));
         MemCat (&gMemTemp, L"%%>");

         if (i + j < lNames.Num()) {
            psz = *((PWSTR*) lNames.Get(i+j));
            MemCat (&gMemTemp, L"<p align=center>");

            THUMBINFO ti;
            ti.fEmpty = TRUE;
            ObjectCFGUIDsFromName (dwRenderShard, t.szMajor, t.szMinor, psz, &t.gCode, &t.gSub);
            ti.hBitmap = ThumbnailGet()->ThumbnailToBitmap (&t.gCode, &t.gSub, cWindow.m_hWnd, &ti.cTransparent);
            ti.pszName = *((PWSTR*) lNames.Get(i+j));
            if (!ti.hBitmap)
               ti.cTransparent = 0;
            lThumbInfo.Add (&ti);

            // bitmap
            MemCat (&gMemTemp, L"<image hbitmap=");
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", ti.hBitmap ? (__int64) ti.hBitmap : (__int64)hBlank);
            MemCat (&gMemTemp, szTemp);
            if (ti.cTransparent != -1) {
               MemCat (&gMemTemp, L" transparent=true transparentdistance=0 transparentcolor=");
               ColorToAttrib (szTemp, ti.cTransparent);
               MemCat (&gMemTemp, szTemp);
            }

            MemCat (&gMemTemp, L" name=bitmap");
            MemCat (&gMemTemp, (int) i+j);
            MemCat (&gMemTemp, L" border=0 href=\"obj:");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"\"/>");

            MemCat (&gMemTemp, L"<br/>");
            MemCat (&gMemTemp, L"<small><a color=#ffffff href=\"obj:");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"\">");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"</a></small>");
            MemCat (&gMemTemp, L"</p>");
         }
         
         MemCat (&gMemTemp, L"</td>");
      }
      MemCat (&gMemTemp, L"</tr>");
      i += dwColumns-2;
   }
   MemCat (&gMemTemp, L"</table>");
   MemCat (&gMemTemp, L"</td>");

   // finish off the table
   MemCat (&gMemTemp, L"</tr></table>");

   // add button for clearing
   MemCat (&gMemTemp, L"<p/><xbr/>"
      L"<xchoicebutton href=clearcache>"
      L"<bold>Refresh thumbnails</bold><br/>"
      L"The thumbnails you see are generated the first time they're needed and then "
      L"saved to disk. From then on the saved images are used. If you install a new "
      L"version of " APPLONGNAMEW L", some objects may have changed; you can "
      L"press this button to refresh all your thumbnails. Otherise, ignore it."
      L"</xchoicebutton>");
   MemCat (&gMemTemp, L"</font>");

   // rmember the names in case changes
   wcscpy (szMajor, t.szMajor);
   wcscpy (szMinor, t.szMinor);
   wcscpy (szName, t.szName);

   // start with the first page
   pszRet = cWindow.PageDialog (ghInstance, (PWSTR) gMemTemp.p, ObjectNewPage, &t);
   t.iVScroll = cWindow.m_iExitVScroll;

   // free bitmaps
   if (hBlank)
      DeleteObject (hBlank);
   for (i = 0; i < lThumbInfo.Num(); i++) {
      PTHUMBINFO pti = (PTHUMBINFO) lThumbInfo.Get(i);
      if (pti->hBitmap)
         DeleteObject (pti->hBitmap);
   }


   if (!pszRet) {
      delete pRender;
      return FALSE;
   }
   PWSTR pszMajorCat = L"mcat:", pszMinorCat = L"icat:", pszNameCat = L"obj:";
   DWORD dwMajorCat = (DWORD)wcslen(pszMajorCat), dwMinorCat = (DWORD)wcslen(pszMinorCat), dwNameCat = (DWORD)wcslen(pszNameCat);
   if (!_wcsicmp(pszRet, L"clearcache")) {
      ThumbnailGet()->ThumbnailClearAll (FALSE);
      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszMajorCat, dwMajorCat)) {
      wcscpy (t.szMajor, pszRet + dwMajorCat);
      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszMinorCat, dwMinorCat)) {
      wcscpy (t.szMinor, pszRet + dwMinorCat);
      goto redopage;
   }
   else if (!wcsncmp(pszRet, pszNameCat, dwNameCat)) {
      wcscpy (t.szName, pszRet + dwNameCat);
      wcscpy (szName, t.szName);

      // get the guids
      delete pRender;
      return ObjectCFGUIDsFromName (dwRenderShard, t.szMajor, t.szMinor, t.szName, pgCode, pgSub);
   }
   
   delete pRender;

   return FALSE;
}





/**************************************************************************
ObjectRenamePage
*/
BOOL ObjectRenamePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTOSPAGE pt = (PTOSPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pt->szName);
         pControl = pPage->ControlFind (L"category");
         if (pControl)
            pControl->AttribSet (Text(), pt->szMajor);
         pControl = pPage->ControlFind (L"subcategory");
         if (pControl)
            pControl->AttribSet (Text(), pt->szMinor);
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"ok")) {
            WCHAR szMajor[128], szMinor[128], szName[128];

            PCEscControl pControl;

            szMajor[0] = szMinor[0] = szName[0] = 0;
            DWORD dwNeeded;
            pControl = pPage->ControlFind (L"name");
            if (pControl)
               pControl->AttribGet (Text(), szName, sizeof(szName), &dwNeeded);
            pControl = pPage->ControlFind (L"category");
            if (pControl)
               pControl->AttribGet (Text(), szMajor, sizeof(szName), &dwNeeded);
            pControl = pPage->ControlFind (L"subcategory");
            if (pControl)
               pControl->AttribGet (Text(), szMinor, sizeof(szName), &dwNeeded);

            if (!szMajor[0] || !szMinor[0] || !szName[0]) {
               pPage->MBWarning (L"You cannot leave the category, sub-category, or name blank.");
               return TRUE;
            }

            GUID gCode, gSub;
            if (ObjectCFGUIDsFromName(dwRenderShard, szMajor, szMinor, szName, &gCode, &gSub)) {
               pPage->MBWarning (L"That name already exists.",
                  L"Your object must have a unique name.");
               return TRUE;
            }

            // ok, rename it
            LibraryObjects(dwRenderShard, FALSE)->ItemRename (pt->szMajor, pt->szMinor, pt->szName,
               szMajor, szMinor, szName);
            LibraryObjects(dwRenderShard, TRUE)->ItemRename (pt->szMajor, pt->szMinor, pt->szName,
               szMajor, szMinor, szName);

            wcscpy (pt->szMajor, szMajor);
            wcscpy (pt->szMinor, szMinor);
            wcscpy (pt->szName, szName);

            break;
         }
      }
      break;
   }

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
ObjectLibraryPage
*/
BOOL ObjectLibraryPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   static WCHAR sObjectTemp[16];

   PTOSPAGE pt = (PTOSPAGE) pPage->m_pUserData;
   DWORD dwRenderShard = pt->dwRenderShard;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list of categories
         pPage->Message (ESCM_USER+91);

         // new Object so set all the parameters
         pPage->Message (ESCM_USER+84);

         // disable create template button if more than one object selected
         DWORD dwNum;
         DWORD *pdws;
         PCEscControl pControl;
         PCWorldSocket pWorld;
         pWorld = WorldGet(dwRenderShard, NULL);
         pdws = pWorld ? pWorld->SelectionEnum(&dwNum) : NULL;
         if (dwNum != 1) {
            pControl = pPage->ControlFind (L"newtemp");
            if (pControl)
               pControl->Enable (FALSE);
         }
      }
      break;

   case ESCM_USER+91: // update the major categoriy list
      {
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         ObjectCFEnumMajor (dwRenderShard, &list);
         MemZero (&mem);
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\">");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"</elem>");
         }
         pControl = pPage->ControlFind (L"major");
         if (pControl) {
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            ESCMCOMBOBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_COMBOBOXADD, &add);

            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMajor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
      }
      return TRUE;

   case ESCM_USER+82: // set the minor category box
      {
         // if we're showing the right one then don't care
         if (!_wcsicmp(pt->szCurMajorCategory, pt->szMajor))
            return TRUE;

         // if changed, then names will be changed to
         pt->szCurMinorCategory[0] = 0;
         wcscpy (pt->szCurMajorCategory, pt->szMajor);

         // fill in the list of categories
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         ObjectCFEnumMinor (dwRenderShard, &list, pt->szMajor);
         MemZero (&mem);
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\">");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"</elem>");
         }
         pControl = pPage->ControlFind (L"minor");
         if (pControl) {
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            ESCMCOMBOBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_COMBOBOXADD, &add);

            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
      }
      break;


   case ESCM_USER+83: // set the names list box
      {
         // if we're showing the right one then don't care
         if (!_wcsicmp(pt->szCurMinorCategory, pt->szMinor))
            return TRUE;

         // if changed, then names will be changed to
         wcscpy (pt->szCurMinorCategory, pt->szMinor);

         // fill in the list of names
         CListFixed list;
         DWORD i;
         CMem mem;
         PWSTR psz;
         PCEscControl pControl;
         ObjectCFEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
         MemZero (&mem);
         BOOL fBold;
         for (i = 0; i < list.Num(); i++) {
            psz = *((PWSTR*) list.Get(i));
            MemCat (&mem, L"<elem name=\"");
            MemCatSanitize (&mem, psz);
            MemCat (&mem, L"\"><small>");

            // if it's a custom Object then bold it
            if (LibraryObjects(dwRenderShard, TRUE)->ItemGet (pt->szMajor, pt->szMinor, psz))
               fBold = TRUE;
            else
               fBold = FALSE;
            if (fBold)
               MemCat (&mem, L"<font color=#008000>");
            MemCatSanitize (&mem, psz);
            if (fBold) {
               // if it's a template object then show (t)
               GUID gCode, gSub;
               ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, psz, &gCode, &gSub);
               if (IsEqualGUID (gCode, CLSID_ObjTemplate))  // BUGFIX - Was pt->gCode, which is wrong
                  MemCat (&mem, L" (template)");
            }
            if (fBold)
               MemCat (&mem, L"</font>");
            MemCat (&mem, L"</small></elem>");
         }
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            pControl->Message (ESCM_LISTBOXRESETCONTENT);

            ESCMLISTBOXADD add;
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            add.pszMML = (PWSTR) mem.p;
            pControl->Message (ESCM_LISTBOXADD, &add);

            ESCMLISTBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &sel);
         }
      }
      break;

   case ESCM_USER+84:  // we have a new Object so set the parameters
      {
         // set the minor category box if necessary
         pPage->Message (ESCM_USER+82);

         // set the name box if necessary
         pPage->Message (ESCM_USER+83);

         // set it in the comboboxes and list boxes
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"major");
         if (pControl) {
            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMajor;  // BUGFIX - Was pt->szMinor, whisch is wrong
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
         pControl = pPage->ControlFind (L"minor");
         if (pControl) {
            ESCMCOMBOBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szMinor;
            pControl->Message (ESCM_COMBOBOXSELECTSTRING, &sel);
         }
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            ESCMLISTBOXSELECTSTRING sel;
            memset (&sel, 0, sizeof(sel));
            sel.fExact = TRUE;
            sel.psz = pt->szName;
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &sel);
         }

         // redo the bitmap
         pPage->Message (ESCM_USER+85);
      }
      return TRUE;

   case ESCM_USER+85:   // redo the bitmap
      {
         // update the buttons for editing if custom
         BOOL  fBuiltIn;
         PCEscControl pControl;
         PCMMLNode2 pNode;

         pNode = LibraryObjects(dwRenderShard, FALSE)->ItemGet (pt->szMajor, pt->szMinor, pt->szName);
         if (pNode)
            fBuiltIn = TRUE;
         else {
            fBuiltIn = FALSE;
            pNode = LibraryObjects(dwRenderShard, TRUE)->ItemGet (pt->szMajor, pt->szMinor, pt->szName);
         }
#ifdef WORKONOBJECTS
         fBuiltIn = FALSE; // allow to modify all
#endif

         if (!ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &pt->gCode, &pt->gSub))
            return FALSE;  // error

         // only allow to edit if workonobjects and not a number-based
         BOOL  fCanEdit;
         fCanEdit = !fBuiltIn;

#ifndef WORKONOBJECTS
         // BUGFIX - Allow to edit based on GUID
         if (!IsEqualGUID(pt->gCode, CLSID_ObjEditor))
            fCanEdit = FALSE;
#endif

         // BUGFIX - Can't edit template
         if (IsEqualGUID (pt->gCode, CLSID_ObjTemplate))
            fCanEdit = FALSE;

//         if (pNode) {
//            DWORD dwID;
//            dwID = MMLValueGetInt (pNode, gpszObjectByID, 0x1234567);
//            if (dwID != 0x1234567) {
//#ifndef WORKONOBJECTS
//               fCanEdit = FALSE;
//#endif
//            }
//         }

         pControl = pPage->ControlFind (L"edit");
         if (pControl)
            pControl->Enable (fCanEdit);
         pControl = pPage->ControlFind (L"copyedit");
         if (pControl)
            pControl->Enable (fCanEdit);
         pControl = pPage->ControlFind (L"rename");
         if (pControl)
            pControl->Enable (!fBuiltIn);
         pControl = pPage->ControlFind (L"remove");
         if (pControl)
            pControl->Enable (!fBuiltIn);


         // get the teture
         COLORREF crBackground;
         if (pt->hBit)
            DeleteObject (pt->hBit);
         pt->hBit = Thumbnail (dwRenderShard, &pt->gCode, &pt->gSub, pPage->m_pWindow->m_hWnd, &crBackground);
         // NOTE: Just keeping the black background color
         if (!pt->hBit)
            return FALSE;

         pControl = pPage->ControlFind (L"image");

         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64) pt->hBit);
         pControl->AttribSet (L"hbitmap", szTemp);

#ifdef _DEBUG
         // write out guids
         FILE *f;
         f = fopen("c:\\texture.txt", "wt");
         if (f) {
            fprintf (f, "DEFINE_GUID(GTEXTURECODE,\n"
               "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x);\n",
               (DWORD) pt->gCode.Data1, 
               (DWORD) pt->gCode.Data2, 
               (DWORD) pt->gCode.Data3, 
               (DWORD) pt->gCode.Data4[0],
               (DWORD) pt->gCode.Data4[1],
               (DWORD) pt->gCode.Data4[2],
               (DWORD) pt->gCode.Data4[3],
               (DWORD) pt->gCode.Data4[4],
               (DWORD) pt->gCode.Data4[5],
               (DWORD) pt->gCode.Data4[6],
               (DWORD) pt->gCode.Data4[7]
               );
            fprintf (f, "DEFINE_GUID(GTEXTURESUB,\n"
               "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x);\n",
               (DWORD) pt->gSub.Data1, 
               (DWORD) pt->gSub.Data2, 
               (DWORD) pt->gSub.Data3, 
               (DWORD) pt->gSub.Data4[0],
               (DWORD) pt->gSub.Data4[1],
               (DWORD) pt->gSub.Data4[2],
               (DWORD) pt->gSub.Data4[3],
               (DWORD) pt->gSub.Data4[4],
               (DWORD) pt->gSub.Data4[5],
               (DWORD) pt->gSub.Data4[6],
               (DWORD) pt->gSub.Data4[7]
               );

            fclose (f);
         }
#endif

      }
      return TRUE;

   case ESCM_USER + 86: // create a new Object using the new major, minor, and name
      {
         // get the guids
         if (!ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &pt->gCode, &pt->gSub))
            return FALSE;  // error

         // and make sure to refresh
         pPage->Message (ESCM_USER+84);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"copyedit") || !_wcsicmp(p->pControl->m_pszName, L"justcopy")) {
            // get it
            GUID gCode, gSub;
            PCMMLNode2 pNode;
            if (!ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub))
               return TRUE;
            pNode = LibraryObjects(dwRenderShard, FALSE)->ItemGet (pt->szMajor, pt->szMinor, pt->szName);
            if (!pNode)
               pNode = LibraryObjects(dwRenderShard, TRUE)->ItemGet  (pt->szMajor, pt->szMinor, pt->szName);
            if (!pNode)
               return TRUE;

            // clone
            pNode = pNode->Clone();
            if (!pNode)
               return TRUE;

            // find a new name
            UniqueObjectName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName);

            GUIDGen(&gSub);

            //add it
#ifdef WORKONOBJECTS
            LibraryObjects(dwRenderShard, FALSE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub, pNode);
#else
            LibraryObjects(dwRenderShard, TRUE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub, pNode);
#endif

            // clean out the thumbnail just in case
            ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);

            ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName,
               &pt->gCode, &pt->gSub);

            // update lists and selection
            pt->szCurMinorCategory[0] = 0;   // so will refresh
            pt->szCurMajorCategory[0] = 0;   // so refreshes

            pPage->MBSpeakInformation (L"Object copied.");

            if (!_wcsicmp(p->pControl->m_pszName, L"copyedit"))
               pPage->Exit (L"edit");
            else {
               pPage->Message (ESCM_USER+91);   // new major categories
               pPage->Message (ESCM_USER+86);   // other stuff may have changed
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"newtemp")) {
            // find out what's selected
            DWORD *pdws;
            DWORD dwNum;
            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
            pdws = pWorld ? pWorld->SelectionEnum (&dwNum) : NULL;
            if (!dwNum) {
               pPage->MBWarning (L"You must select an object.");
               return TRUE;
            }
            if (dwNum > 1) {
               pPage->MBWarning (L"You can only create a template from a single object.",
                  L"You have more than one object currently selected.");
               return TRUE;
            }
            
            // get the object
            PCObjectSocket pos;
            pos = pWorld ? pWorld->ObjectGet (pdws[0]) : NULL;
            if (!pos)
               return TRUE;   // shouldnt happen
            if (pos->ContEmbeddedNum()) {
               pPage->MBWarning (L"You cannot create a template from an object that has "
                  L"other objects embedded in it.");
               return TRUE;
            }

            PCMMLNode2 pNode;
            pNode = MMLFromObject (pos);
            if (!pNode)
               return FALSE;
            pNode->NameSet (gpszObject);

            AttachDateTimeToMML (pNode);

            GUID gSub, gCode;
            gCode = CLSID_ObjTemplate;
            GUIDGen (&gSub);

            // get a unique name
            wcscpy (pt->szName, L"New object");
            UniqueObjectName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName);

      #ifdef WORKONOBJECTS
            LibraryObjects(dwRenderShard, FALSE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName,
               &gCode, &gSub, pNode);
      #else
            LibraryObjects(dwRenderShard, TRUE)->ItemAdd (pt->szMajor, pt->szMinor, pt->szName,
               &gCode, &gSub, pNode);
      #endif

            pt->szCurMajorCategory[0] = 0;
            pt->szCurMinorCategory[0] = 0;
            pPage->Message (ESCM_USER+91);   // new major categories
            pPage->Message (ESCM_USER+86);   // other stuff may have changed

            pPage->MBSpeakInformation (L"Template created.");

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"rename")) {
            // get the angle
            CEscWindow cWindow;
            RECT r;
            DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
            PWSTR pszRet;
            pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJECTRENAME, ObjectRenamePage, pt);

            //if (!_wcsicmp(pszRet, L"ok")) { // BUGFIX - Because if hit enter returns [close]
               pt->szCurMajorCategory[0] = 0;
               pt->szCurMinorCategory[0] = 0;
               pPage->Message (ESCM_USER+91);   // new major categories
               pPage->Message (ESCM_USER+86);   // other stuff may have changed
               // BUGFIX - Was +84, but try 86 to see if fixes occasional bug
            //}
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"remove")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to permenantly delete this object?"))
               return TRUE;

            // clear the cache
            GUID gCode, gSub;
            ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName, &gCode, &gSub);

            // remove it from either one
            LibraryObjects(dwRenderShard, FALSE)->ItemRemove (pt->szMajor, pt->szMinor, pt->szName);
            LibraryObjects(dwRenderShard, TRUE)->ItemRemove (pt->szMajor, pt->szMinor, pt->szName);

            // clean out the thumbnail just in case
            ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);


            // new Object
            CListFixed list;
            PWSTR psz;
            ObjectCFEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            if (!list.Num()) {
               // minor no longer exists
               ObjectCFEnumMinor (dwRenderShard, &list, pt->szMajor);
               if (!list.Num()) {
                  // minor no longer exists
                  ObjectCFEnumMajor (dwRenderShard, &list);
                  psz = *((PWSTR*) list.Get(0));
                  wcscpy (pt->szMajor, psz);

                  ObjectCFEnumMinor (dwRenderShard, &list, pt->szMajor);
                  // will have something there
               }

               psz = *((PWSTR*) list.Get(0));
               wcscpy (pt->szMinor, psz);
               ObjectCFEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
               // will have something there
            }
            psz = *((PWSTR*) list.Get(0));
            wcscpy(pt->szName, psz);
            pt->szCurMajorCategory[0] = 0;
            pt->szCurMinorCategory[0] = 0;
            ObjectCFGUIDsFromName (dwRenderShard, pt->szMajor, pt->szMinor, pt->szName,
               &pt->gCode, &pt->gSub);
            pPage->Message (ESCM_USER+91);   // new major categories
            pPage->Message (ESCM_USER+86);   // other stuff may have changed
               // BUGFIX - Was +84 but changed to +86

            pPage->MBSpeakInformation (L"Object removed.");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and Object that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"major")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szMajor, p->pszName))
               return TRUE;

            wcscpy (pt->szMajor, p->pszName);

            CListFixed list;
            PWSTR psz;

            // first minor we find
            ObjectCFEnumMinor (dwRenderShard, &list, pt->szMajor);
            psz = *((PWSTR*) list.Get(0));
            wcscpy (pt->szMinor, psz);

            // first ObjectCF we find
            ObjectCFEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            psz = *((PWSTR*) list.Get(0));
            wcscpy(pt->szName, psz);

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"minor")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szMinor, p->pszName))
               return TRUE;

            wcscpy (pt->szMinor, p->pszName);

            CListFixed list;
            PWSTR psz;

            // first Object we find
            ObjectCFEnumItems (dwRenderShard, &list, pt->szMajor, pt->szMinor);
            if (list.Num()) {
               psz = *((PWSTR*) list.Get(0));
               wcscpy(pt->szName, psz);
            }
            else
               pt->szName[0] = 0;

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // if the major ID changed then pick the first minor and Object that
         // comes to mind
         if (!_wcsicmp(p->pControl->m_pszName, L"name")) {
            // if it hasn't reall change then ignore
            if (!_wcsicmp(pt->szName, p->pszName))
               return TRUE;

            wcscpy (pt->szName, p->pszName);

            pPage->Message (ESCM_USER + 86);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            swprintf (sObjectTemp, L"%lx", (__int64) pt->hBit);
            p->pszSubString = sObjectTemp;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Object library";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
ObjectClassNewPage
*/
BOOL ObjectClassNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Create a new object from scratch";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
ObjectClassEditPage
*/
BOOL ObjectClassEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTOSPAGE pt = (PTOSPAGE) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DoubleToControl (pPage, L"id", MMLValueGetInt (pt->pNode, gpszObjectByID, 123));
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // get the value
         MMLValueSet (pt->pNode, gpszObjectByID, (int) DoubleFromControl (pPage, L"id"));
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Class edit";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
ObjectEditorShowWindow - This functions shows the window for the object
being edited.

inputs
   GUID        *pgCode - Major ID code
   GUID        *pgSub - Minor ID code
returns
   BOOL - TRUE if success (or if already exists), FALSE if failed.
*/
BOOL ObjectEditorShowWindow (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub)
{
   if (!IsEqualGUID (CLSID_ObjEditor, *pgCode))
      return FALSE; // cant edit this

   // take the guid and convert to name
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!ObjectCFNameFromGUIDs (dwRenderShard, pgCode, pgSub, szMajor, szMinor, szName))
      return FALSE;

   // get the MML
   PCMMLNode2 pNode;
   BOOL fUser;
   fUser = TRUE;
   pNode = LibraryObjects(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
#ifdef WORKONOBJECTS
   if (!pNode) {
      pNode = LibraryObjects(dwRenderShard, FALSE)->ItemGet (szMajor, szMinor, szName);
      fUser = FALSE;
   }
#endif
   if (!pNode)
      return FALSE;  // either doesnt exist, or it's a built-in object and cant edit

   // make sure one isn't visible already
   DWORD i;
   PVIEWOBJ pvo;
   for (i = 0; i < ListVIEWOBJ()->Num(); i++) {
      pvo = (PVIEWOBJ) ListVIEWOBJ()->Get(i);
      if (IsEqualGUID (pvo->gCode, *pgCode) && IsEqualGUID (pvo->gSub, *pgSub)) {
         ShowWindow (pvo->pView->m_hWnd, SW_SHOW);
         SetForegroundWindow (pvo->pView->m_hWnd);
         return TRUE;
      }
   }

   // edit it
   PCWorld pWorld;
   pWorld = new CWorld;
   if (!pWorld)
      return FALSE;
   pWorld->RenderShardSet (dwRenderShard);
   BOOL fFailedToLoad;
   if (!pWorld->MMLFrom (pNode, &fFailedToLoad))
      fFailedToLoad = TRUE;
   if (fFailedToLoad) {
      delete pWorld;
      return FALSE;
      //EscMessageBox (cWindow.m_hWnd, ASPString(),
      //   L"That object couldn't be edited.",
      //   L"It may contain other objects that aren't stored in the current object list.",
      //   MB_ICONEXCLAMATION | MB_OK);
   }

   // BUGFIX - Always set the lighting for the world object
   WCHAR szTemp[64];
   swprintf (szTemp, L"%d", TODFDATE(1,7,YEARFROMDFDATE(TodayFast())+1));
   if (!pWorld->VariableGet (WSDate()))
      pWorld->VariableSet (WSDate(), szTemp);
   swprintf (szTemp, L"%g", (double)(PI / 2 * .5));  // so sun coming from south
   if (!pWorld->VariableGet (WSLatitude()))
      pWorld->VariableSet (WSLatitude(), szTemp);
   swprintf (szTemp, L"%d", (int) TODFTIME(15,00));
   if (!pWorld->VariableGet (WSTime()))
      pWorld->VariableSet (WSTime(), szTemp);

   pWorld->UndoClear(TRUE, TRUE);
   pWorld->m_fDirty = FALSE;

   // bring up the iwndow
   PCHouseView pView;
   pView = new CHouseView (pWorld, NULL, VIEWWHAT_OBJECT, pgCode, pgSub, pWorld);
   if (!pView) {
      delete pWorld;
      return FALSE;
   }
   pView->Init();
   // done

   return TRUE;
}



/****************************************************************************
ObjectLibraryDialog - This brings up the UI for changing the library

inputs
   HWND           hWnd - Window to create this over
returns
   BOOL - TRUE if the user presses OK, FALSE if not
*/
BOOL ObjectLibraryDialog (DWORD dwRenderShard, HWND hWnd)
{
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);
   CImage   Image;
   Image.Init (200, 200, RGB(0xff,0xff,0xff));

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up the info
   TOSPAGE t;
   memset (&t, 0, sizeof(t));
   t.hBit = NULL;
   t.dwRenderShard = dwRenderShard;
   
   // couldn't find, so start out with something
   CListFixed list;
   PWSTR psz;

   // first major we find
   ObjectCFEnumMajor (dwRenderShard, &list);
   psz = *((PWSTR*) list.Get(0));
   wcscpy (t.szMajor, psz);

   // first minor we find
   ObjectCFEnumMinor (dwRenderShard, &list, t.szMajor);
   psz = *((PWSTR*) list.Get(0));
   wcscpy (t.szMinor, psz);

   // first Object we find
   ObjectCFEnumItems (dwRenderShard, &list, t.szMajor, t.szMinor);
   psz = *((PWSTR*) list.Get(0));
   wcscpy(t.szName, psz);

newtext:
   // get the guids
   if (!ObjectCFGUIDsFromName (dwRenderShard, t.szMajor, t.szMinor, t.szName, &t.gCode, &t.gSub))
      return FALSE;  // error

   // get the teture
   COLORREF crBackground;
   if (t.hBit)
      DeleteObject (t.hBit);
   t.hBit = Thumbnail (dwRenderShard, &t.gCode, &t.gSub, hWnd, &crBackground);
   // NOTE: Just keeping the black background color
   if (!t.hBit)
      return FALSE;


   // start with the first page
mainpage:
   // BUGFIX - Set curmajor and minor to 0 so refreshes
   t.szCurMajorCategory[0] = 0;
   t.szCurMinorCategory[0] = 0;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJECTLIBRARY, ObjectLibraryPage, &t);

#ifdef _DEBUG
mainpage2:
#endif // _DEBUG
   if (pszRet && !_wcsicmp(pszRet, L"newtext")) {
      pszRet = L"XXXObjEditor";
      DWORD dwID;
      dwID = 0;
      BOOL fUseID;
      fUseID = FALSE;

      // only ask what sort of object when special _DEBUG is on
#ifdef _DEBUG
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJECTCLASSNEW, ObjectClassNewPage, &t);
      if (!pszRet)
         goto mainpage2;

      if (!_wcsicmp(pszRet, Back()))
         goto mainpage;

      if ((pszRet[0] != L'X') || (pszRet[1] != L'X') || (pszRet[2] != L'X'))
         goto mainpage2;
#endif   // _DEBUG

      // fill in the info
      // If its object editor (default) then set gChoose to object editor
      GUID  gChoose;
      gChoose = CLSID_ObjEditor;
      if (!_wcsicmp(pszRet, L"XXXBalustrade")) {
         fUseID = TRUE;
         gChoose = CLSID_Balustrade;
      }
      else if (!_wcsicmp(pszRet, L"XXXBuildBlock")) {
         fUseID = TRUE;
         gChoose = CLSID_BuildBlock;
      }
      else if (!_wcsicmp(pszRet, L"XXXCabinet")) {
         fUseID = TRUE;
         gChoose = CLSID_Cabinet;
      }
      else if (!_wcsicmp(pszRet, L"XXXColumn")) {
         fUseID = TRUE;
         gChoose = CLSID_Column;
      }
      else if (!_wcsicmp(pszRet, L"XXXDoor")) {
         fUseID = TRUE;
         gChoose = CLSID_Door;
      }
      else if (!_wcsicmp(pszRet, L"XXXGround")) {
         fUseID = TRUE;
         gChoose = CLSID_Ground;
      }
      else if (!_wcsicmp(pszRet, L"XXXLight")) {
         fUseID = TRUE;
         gChoose = CLSID_Light;
      }
      else if (!_wcsicmp(pszRet, L"XXXLightGeneric")) {
         fUseID = TRUE;
         gChoose = CLSID_LightGeneric;
      }
      else if (!_wcsicmp(pszRet, L"XXXNoodle")) {
         fUseID = TRUE;
         gChoose = CLSID_Noodle;
      }
      else if (!_wcsicmp(pszRet, L"XXXPiers")) {
         fUseID = TRUE;
         gChoose = CLSID_Piers;
      }
      else if (!_wcsicmp(pszRet, L"XXXStairs")) {
         fUseID = TRUE;
         gChoose = CLSID_Stairs;
      }
      else if (!_wcsicmp(pszRet, L"XXXStructSurface")) {
         fUseID = TRUE;
         gChoose = CLSID_StructSurface;
      }
      else if (!_wcsicmp(pszRet, L"XXXSingleSurface")) {
         fUseID = TRUE;
         gChoose = CLSID_SingleSurface;
      }
      else if (!_wcsicmp(pszRet, L"XXXTeapot")) {
         fUseID = TRUE;
         gChoose = CLSID_TEAPOT;
      }
      else if (!_wcsicmp(pszRet, L"XXXGrass")) {
         fUseID = TRUE;
         gChoose = CLSID_Grass;
      }
      else if (!_wcsicmp(pszRet, L"XXXRock")) {
         fUseID = TRUE;
         gChoose = CLSID_Rock;
      }
      else if (!_wcsicmp(pszRet, L"XXXCave")) {
         fUseID = TRUE;
         gChoose = CLSID_Cave;
      }
      else if (!_wcsicmp(pszRet, L"XXXWaterfall")) {
         fUseID = TRUE;
         gChoose = CLSID_Waterfall;
      }
      else if (!_wcsicmp(pszRet, L"XXXjaw")) {
         fUseID = TRUE;
         gChoose = CLSID_Jaw;
      }
      else if (!_wcsicmp(pszRet, L"XXXTooth")) {
         fUseID = TRUE;
         gChoose = CLSID_Tooth;
      }
      else if (!_wcsicmp(pszRet, L"XXXhairlock")) {
         fUseID = TRUE;
         gChoose = CLSID_HairLock;
      }
      else if (!_wcsicmp(pszRet, L"XXXhairhead")) {
         fUseID = TRUE;
         gChoose = CLSID_HairHead;
      }
      else if (!_wcsicmp(pszRet, L"XXXEye")) {
         fUseID = TRUE;
         gChoose = CLSID_Eye;
      }
      else if (!_wcsicmp(pszRet, L"XXXBranch")) {
         fUseID = TRUE;
         gChoose = CLSID_Branch;
      }
      else if (!_wcsicmp(pszRet, L"XXXLeaf")) {
         fUseID = TRUE;
         gChoose = CLSID_Leaf;
      }
      else if (!_wcsicmp(pszRet, L"XXXSkydome")) {
         fUseID = TRUE;
         gChoose = CLSID_Skydome;
      }
      else if (!_wcsicmp(pszRet, L"XXXAnimCamera")) {
         fUseID = TRUE;
         gChoose = CLSID_AnimCamera;
      }
      else if (!_wcsicmp(pszRet, L"XXXBone")) {
         fUseID = TRUE;
         gChoose = CLSID_Bone;
      }
      else if (!_wcsicmp(pszRet, L"XXXTruss")) {
         fUseID = TRUE;
         gChoose = CLSID_Truss;
      }
      else if (!_wcsicmp(pszRet, L"XXXFireplace")) {
         fUseID = TRUE;
         gChoose = CLSID_Fireplace;
      }
      else if (!_wcsicmp(pszRet, L"XXXPool")) {
         fUseID = TRUE;
         gChoose = CLSID_Pool;
      }
      else if (!_wcsicmp(pszRet, L"XXXTarp")) {
         fUseID = TRUE;
         gChoose = CLSID_Tarp;
      }
      else if (!_wcsicmp(pszRet, L"XXXPathway")) {
         fUseID = TRUE;
         gChoose = CLSID_Pathway;
      }
      else if (!_wcsicmp(pszRet, L"XXXDrawer")) {
         fUseID = TRUE;
         gChoose = CLSID_Drawer;
      }
      else if (!_wcsicmp(pszRet, L"XXXCeilingFan")) {
         fUseID = TRUE;
         gChoose = CLSID_CeilingFan;
      }
      else if (!_wcsicmp(pszRet, L"XXXBooks")) {
         fUseID = TRUE;
         gChoose = CLSID_Books;
      }
      else if (!_wcsicmp(pszRet, L"XXXTableCloth")) {
         fUseID = TRUE;
         gChoose = CLSID_TableCloth;
      }
      else if (!_wcsicmp(pszRet, L"XXXCushion")) {
         fUseID = TRUE;
         gChoose = CLSID_Cushion;
      }
      else if (!_wcsicmp(pszRet, L"XXXCurtain")) {
         fUseID = TRUE;
         gChoose = CLSID_Curtain;
      }
      else if (!_wcsicmp(pszRet, L"XXXPainting")) {
         fUseID = TRUE;
         gChoose = CLSID_Painting;
      }
      else if (!_wcsicmp(pszRet, L"XXXPolyhedron")) {
         fUseID = TRUE;
         gChoose = CLSID_Polyhedron;
      }
      else if (!_wcsicmp(pszRet, L"XXXPolyMesh")) {
         fUseID = TRUE;
         gChoose = CLSID_PolyMesh;
      }
//      else if (!_wcsicmp(pszRet, L"XXXPolyMeshOld")) {
//         fUseID = TRUE;
//         gChoose = CLSID_PolyMeshOld;
//      }
      else if (!_wcsicmp(pszRet, L"XXXRevolution")) {
         fUseID = TRUE;
         gChoose = CLSID_Revolution;
      }
#if 0 // DEAD code
      else if (!_wcsicmp(pszRet, L"XXXTree")) {
         fUseID = TRUE;
         gChoose = CLSID_Tree;
      }
#endif // 0
      else if (!_wcsicmp(pszRet, L"XXXUniHole")) {
         fUseID = TRUE;
         gChoose = CLSID_UniHole;
      }

      PCMMLNode2 pNode;
      // create a blank world for the object editor
      if (IsEqualGUID (gChoose, CLSID_ObjEditor)) {
         CWorld World;
         World.RenderShardSet (dwRenderShard);
         pNode = World.MMLTo();
      }
      else {
         pNode = new CMMLNode2;
      }
      if (!pNode)
         goto mainpage;
      pNode->NameSet (gpszObject);
      if (fUseID)
         MMLValueSet (pNode, gpszObjectByID, (int) dwID);

      AttachDateTimeToMML (pNode);

      GUID gSub;
      GUIDGen (&gSub);

      // get a unique name
      wcscpy (t.szName, L"New object");
      UniqueObjectName (dwRenderShard, t.szMajor, t.szMinor, t.szName);
      t.szCurMajorCategory[0] = t.szCurMinorCategory[0] = 0;

#ifdef WORKONOBJECTS
      LibraryObjects(dwRenderShard, FALSE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
         &gChoose, &gSub, pNode);
#else
      LibraryObjects(dwRenderShard, TRUE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
         &gChoose, &gSub, pNode);
#endif

      // clean out the thumbnail just in case
      ThumbnailGet()->ThumbnailRemove (&gChoose, &gSub);

      goto edit;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"edit")) {
edit:
      GUID gCode, gSub;
      if (!ObjectCFGUIDsFromName(dwRenderShard, t.szMajor, t.szMinor, t.szName, &gCode, &gSub))
         goto mainpage; // error

      if (IsEqualGUID (CLSID_ObjEditor, gCode)) {
         if (ObjectEditorShowWindow(dwRenderShard, &gCode, &gSub))
            goto alldone;
         else {
            EscMessageBox (cWindow.m_hWnd, ASPString(),
               L"That object couldn't be edited.",
               NULL,
               MB_ICONEXCLAMATION | MB_OK);
            goto mainpage;
         }
      }

      // get the MML
      PCMMLNode2 pNode;
      BOOL fUser;
      fUser = TRUE;
      pNode = LibraryObjects(dwRenderShard, TRUE)->ItemGet (t.szMajor, t.szMinor, t.szName);
      if (!pNode) {
         pNode = LibraryObjects(dwRenderShard, FALSE)->ItemGet (t.szMajor, t.szMinor, t.szName);
         fUser = FALSE;
      }
      if (!pNode)
         goto mainpage; // error

      // if get here know that it's not of class CLSID_ObjEditor, so do special
      // code for editing

      // clone this node and modify clone
      pNode = pNode->Clone();
      if (!pNode)
         goto mainpage;

      pszRet = NULL; // BUGFIX so doesnt crash

      // UI for this
      BOOL fRet;
      t.pNode = pNode;
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLOBJECTCLASSEDIT, ObjectClassEditPage, &t);
      fRet = (pszRet && !_wcsicmp(pszRet, Back()));
      int iRet;
      iRet = EscMessageBox (cWindow.m_hWnd, ASPString(),
         L"Do you want to save the changes to your object?",
         NULL,
         MB_ICONQUESTION | MB_YESNO);
      if (iRet == IDYES) {
         if (pNode)
            AttachDateTimeToMML(pNode);

         if (fUser) {
            LibraryObjects(dwRenderShard, TRUE)->ItemRemove (t.szMajor, t.szMinor, t.szName);
            LibraryObjects(dwRenderShard, TRUE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
               &gCode, &gSub, pNode);
         }
         else {
            LibraryObjects(dwRenderShard, FALSE)->ItemRemove (t.szMajor, t.szMinor, t.szName);
            LibraryObjects(dwRenderShard, FALSE)->ItemAdd (t.szMajor, t.szMinor, t.szName,
               &gCode, &gSub, pNode);
         }
         pNode = NULL;  // do dont delete

         // clean out the thumbnail just in case
         ThumbnailGet()->ThumbnailRemove (&gCode, &gSub);

         // tell the world that enough might have changed that should redraw everuthing
         ObjEditReloadCache (dwRenderShard, &gCode, &gSub);
         PCWorldSocket pWorld;
         pWorld = WorldGet(dwRenderShard, NULL);
         if (pWorld)
            pWorld->ObjectCacheReset ();
      }
      if (pNode)
         delete pNode;

      t.szCurMinorCategory[0] = 0;   // so will refresh
      if (fRet)
         goto newtext; // done
      // else fall through
   }

alldone:
   // free the Object map and bitmap
   if (t.hBit)
      DeleteObject (t.hBit);

   if (pszRet && !_wcsicmp(pszRet, L"ok"))
      return TRUE;
   else
      return FALSE;
}


/*********************************************************************************
ObjectEditorSave - Given a VIEWOBJ that's to be saved, this converts to to MML
and writes in all the necessary gunk, and saves it to the world. It also
alerts all other views using this object to redraw.
*/
BOOL ObjectEditorSave (PVIEWOBJ pvo)
{
   DWORD dwRenderShard = pvo->pWorld->RenderShardGet();

   // convert to MML
   PCMMLNode2 pNode;
   pNode = pvo->pWorld->MMLTo (FALSE);
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszObject);

   // set date and time
   AttachDateTimeToMML(pNode);

   // get the name
   if (!ObjectCFNameFromGUIDs (dwRenderShard, &pvo->gCode, &pvo->gSub, pvo->szMajor, pvo->szMinor, pvo->szName)) {
      // cant find it anymore so make up new name
      wcscpy (pvo->szMajor, L"Unknown");
      wcscpy (pvo->szMinor, L"Unknown");
      wcscpy (pvo->szName, L"Unknown");
      UniqueObjectName (dwRenderShard, pvo->szMajor, pvo->szMinor, pvo->szName);
   }

   // find where the object is
   BOOL fUser;
   fUser = TRUE;
   if (LibraryObjects(dwRenderShard, FALSE)->ItemGet (&pvo->gCode, &pvo->gSub))
      fUser = FALSE;

   if (fUser) {
      LibraryObjects(dwRenderShard, TRUE)->ItemRemove (pvo->szMajor, pvo->szMinor, pvo->szName);
      LibraryObjects(dwRenderShard, TRUE)->ItemAdd (pvo->szMajor, pvo->szMinor, pvo->szName,
         &pvo->gCode, &pvo->gSub, pNode);
   }
   else {
      LibraryObjects(dwRenderShard, FALSE)->ItemRemove (pvo->szMajor, pvo->szMinor, pvo->szName);
      LibraryObjects(dwRenderShard, FALSE)->ItemAdd (pvo->szMajor, pvo->szMinor, pvo->szName,
         &pvo->gCode, &pvo->gSub, pNode);
   }

   // clean out the thumbnail just in case
   ThumbnailGet()->ThumbnailRemove (&pvo->gCode, &pvo->gSub);

   // tell the world that enough might have changed that should redraw everuthing
   ObjEditReloadCache (dwRenderShard, &pvo->gCode, &pvo->gSub);
   PCWorldSocket pWorld;
   pWorld = WorldGet(dwRenderShard, NULL);
   if (pWorld)
      pWorld->ObjectCacheReset ();

   // set dirty to false
   pvo->pWorld->DirtySet (FALSE);

   // BUGFIX - if it's a clone then clean up that
   PCObjectClone pc;
   pc = ObjectCloneGet (dwRenderShard, &pvo->gCode, &pvo->gSub, FALSE);
   if (pc) {
      pc->UpdateToNewObject(FALSE);
      pc->Release();
   }

   return TRUE;
}




static PWSTR gpszUserObject = L"UserObject";

/******************************************************************************
ObjectCacheUserObjects - This is called when an ASP file is being saved.
This function calls all of the objects in the world and asks them what
Objects they use. It then determines which ones are "installed" and ignores them.
The custom Objects are then indexed into the custom list and saved into the
MML for retrieval by ObjectUncacheUserObjects. This way a user Object
will be tranferred to a new machine.

inputs
   PCWorldSocket        pWorld - World to look through
returns
   PCMMLNode2 - Cache node
*/
PCMMLNode2 ObjectCacheUserObjects (PCWorldSocket pWorld)
{
   DWORD dwRenderShard = pWorld->RenderShardGet();

   // make the list
   CListFixed lText;
   lText.Init (sizeof(GUID)*2);

   CBTree tree;
   WCHAR szTemp[sizeof(GUID)*4+2];

   // all the objects
   DWORD i, j;
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      PCObjectSocket pos = pWorld->ObjectGet(i);
      if (!pos)
         continue;

      lText.Clear();
      pos->ObjectClassQuery (&lText);

      // loop through all the elements returned
      for (j = 0; j < lText.Num(); j++) {
         GUID *pg = (GUID*) lText.Get(j);

         // add to the tree
         MMLBinaryToString ((PBYTE)pg, sizeof(GUID)*2, szTemp);

         if (!tree.Find(szTemp))
            tree.Add (szTemp, &i, sizeof(i));
      }
   }

   PCMMLNode2 pList;
   pList = new CMMLNode2;
   if (!pList)
      return NULL;
   pList->NameSet (gpszUserObject);

   // loop through the tree
   GUID ag[2];
   size_t dwSize;
   for (i = 0; i < tree.Num(); i++) {
      PWSTR psz = tree.Enum(i);
      if (!psz)
         continue;
      dwSize = MMLBinaryFromString (psz, (PBYTE) ag, sizeof(ag));
      if (dwSize != sizeof(ag))
         continue;

      // see if can load it from the user ID
      WCHAR szMajor[128], szMinor[128], szName[128];
      if (!LibraryObjects(dwRenderShard, TRUE)->ItemNameFromGUID (&ag[0], &ag[1], szMajor, szMinor, szName))
         continue;   // not a user Object

      // else, found it
      PCMMLNode2 pFound;
      pFound = LibraryObjects(dwRenderShard, TRUE)->ItemGet (szMajor, szMinor, szName);
      if (!pFound)
         continue;

      // clone and write in into
      PCMMLNode2 pNew;
      pNew = pFound->Clone();
      if (!pNew)
         continue;
      MMLValueSet (pNew, CatMajor(), szMajor);
      MMLValueSet (pNew, CatMinor(), szMinor);
      MMLValueSet (pNew, CatName(), szName);
      MMLValueSet (pNew, CatGUIDCode(), (PBYTE) &ag[0], sizeof(ag[0]));
      MMLValueSet (pNew, CatGUIDSub(), (PBYTE) &ag[1], sizeof(ag[1]));
      pNew->NameSet (gpszUserObject);

      // add it
      pList->ContentAdd (pNew);
   }

   return pList;
}


/******************************************************************************
ObjectUnCacheUserObjects - Called when an ASP file is being loaded. It
looks through all Objects embedded in the file. If they don't exist in
the current library then they're added (make sure name is unique). If they
do exist and they're older, they're ignored. If they're newer, ask the user
if they want to import the new changes.

inputs
   PCMMLNode2      pNode - From ObjectCacheUserObjects()
   HWND           hWnd - To bring question message boxes up from. If pLibrary != NULL
                  then hWnd won't be used. Instead, a default response will be assumed
   PCLibrary      pLibrary - Alternative user library to save into.
returns
   BOOL - TRUE if success
*/
BOOL ObjectUnCacheUserObjects (DWORD dwRenderShard, PCMMLNode2 pNode, HWND hWnd, PCLibrary pLibrary)
{
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      if (!pNode->ContentEnum(i, &psz, &pSub))
         continue;
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, gpszUserObject))
         continue;

      // have a textre.... get its guids and name
      PWSTR pszMajor, pszMinor, pszName;
      GUID gCode, gSub;
      DFDATE dwDate;
      DFTIME dwTime;
      pszMajor = MMLValueGet (pSub, CatMajor());
      pszMinor = MMLValueGet (pSub, CatMinor());
      pszName = MMLValueGet (pSub, CatName());
      if (!pszMajor || !pszMinor || !pszName)
         continue;
      MMLValueGetBinary (pSub, CatGUIDCode(), (PBYTE) &gCode, sizeof(gCode));
      MMLValueGetBinary (pSub, CatGUIDSub(), (PBYTE) &gSub, sizeof(gSub));
      GetDateAndTimeFromMML (pSub, &dwDate, &dwTime);

      // if it exists as an installed then ignore
      if (LibraryObjects(dwRenderShard, FALSE)->ItemGet(&gCode, &gSub))
         continue;

      // which category object
      // BUGFIX - Allow to come from different library
      PCLibraryCategory plc = pLibrary ?
         pLibrary->CategoryGet (L"Objects" /*gpszObjects*/) :
         LibraryObjects(dwRenderShard, TRUE);

      // see if it exsits as a user
      PCMMLNode2 pUser;
      pUser = plc->ItemGet(&gCode, &gSub);
      WCHAR szOldMajor[128], szOldMinor[128], szOldName[128];
      DFDATE dwExistDate;
      DFTIME dwExistTime;
      if (pUser) {
         GetDateAndTimeFromMML (pUser, &dwExistDate, &dwExistTime);
         plc->ItemNameFromGUID (&gCode, &gSub, szOldMajor, szOldMinor, szOldName);
      }

      BOOL  fAdd;
      fAdd = FALSE;
      if (!pUser)
         fAdd = TRUE;
      else {
         // date compare
         if ((dwDate > dwExistDate) || ((dwDate == dwExistDate) && (dwTime > dwExistTime))) {
            if (!pLibrary) {
               // the one in the file is more recent. Ask user if want to replace
               WCHAR szTemp[512];
               swprintf (szTemp, L"The file contains a more recent version of the object, \"%s\", "
                  L"located in \"%s, %s\". Do you want to use the new version?",
                  szOldName, szOldMajor, szOldMinor);

               int iRet;
               iRet = EscMessageBox (hWnd, ASPString(),
                  szTemp,
                  L"If you press \"Yes\" your old object will be overwritten. If you "
                  L"press \"No\" the building you are viewing may not look correct because "
                  L"it won't be displayed with the updated object.",
                  MB_ICONQUESTION | MB_YESNOCANCEL);
               if (iRet == IDYES)
                  fAdd = TRUE;
            }
            else
               fAdd = TRUE;   // if using a different library
         }
      }
      if (!fAdd)
         continue;

      // else, adding...

      // if there is an existing one then remove it
      if (pUser) {
         plc->ItemRemove (szOldMajor, szOldMinor, szOldName);

         // NOTE: Don't bother to refresh all the displays because none
         // should be open at this point
      }

      // make sure the new name is safe
      wcscpy (szOldMajor, pszMajor);
      wcscpy (szOldMinor, pszMinor);
      wcscpy (szOldName, pszName);
      UniqueObjectName (dwRenderShard, szOldMajor, szOldMinor, szOldName, pLibrary);

      // add it
      PCMMLNode2 pNew;
      pNew = pSub->Clone();
      if (!pNew)
         continue;

      // add it
      plc->ItemAdd (szOldMajor, szOldMinor, szOldName, &gCode, &gSub, pNew);
   } // over all nodes

   return TRUE;
}



// FUTURERELEASE - When preview objects show outlines around those already used. Will
// also be useful feature for quoting.

// BUGBUG - When draw a cone (length = .2, radius = .5) as only object in object selection,
// runs into clip plane, (or no clipping there?) and lines drawn everywhere

