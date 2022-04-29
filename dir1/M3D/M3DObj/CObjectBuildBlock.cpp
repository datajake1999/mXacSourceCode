/************************************************************************
CObjectBuildBlock.cpp - Draws a box.

begun 23/12/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszPoints = L"points%d-%d";
PWSTR gpszBuildBlockClip = L"BuildBlockClip";
static PWSTR gpszTrimRoof = L"TrimRoof";

#define DISPLAYBASE_BALUSTRADE   10000    // balustrade numbers start here

#define BBSURFMAJOR_WALL         0
#define BBSURFMAJOR_ROOF         1
#define BBSURFMAJOR_FLOOR        2
#define BBSURFMAJOR_TOPCEILING   3
#define BBSURFMAJOR_VERANDAH     4


#define BBSURFFLAG_CLIPAGAINSTROOF 0x0001 // this wall/roof need to be clipped against the roof, if roof changed
#define BBSURFFLAG_CLIPAGAINSTWALLS 0x0002   // this floor need to be clipped against the walls
#define BBSURFFLAG_CLIPAGAINSRROOFWALLS 0x0003  // floor needs clipping against the roof and walls

#define WALLSHAPE_RECTANGLE            0
#define WALLSHAPE_NONE                 1
#define WALLSHAPE_TRIANGLE             2
#define WALLSHAPE_PENTAGON             3
#define WALLSHAPE_HEXAGON              4
#define WALLSHAPE_CIRCLE               5
#define WALLSHAPE_SEMICIRCLE           6
#define WALLSHAPE_ANY                  7

#define ROOFSHAPE_HIPHALF        0
#define ROOFSHAPE_HIPPEAK        1
#define ROOFSHAPE_HIPRIDGE       2
#define ROOFSHAPE_OUTBACK1       3
#define ROOFSHAPE_SALTBOX        4
#define ROOFSHAPE_GABLETHREE     5
#define ROOFSHAPE_SHED           6
#define ROOFSHAPE_GABLEFOUR      7
#define ROOFSHAPE_SHEDFOLDED     8
#define ROOFSHAPE_HIPSHEDPEAK    9
#define ROOFSHAPE_HIPSHEDRIDGE   10
#define ROOFSHAPE_MONSARD        11
#define ROOFSHAPE_GAMBREL        12
#define ROOFSHAPE_GULLWING       13
#define ROOFSHAPE_OUTBACK2       14
#define ROOFSHAPE_BALINESE       15
#define ROOFSHAPE_FLAT           16
#define ROOFSHAPE_NONE           17 // special case
#define ROOFSHAPE_TRIANGLEPEAK   18
#define ROOFSHAPE_PENTAGONPEAK   19
#define ROOFSHAPE_HEXAGONPEAK    20
#define ROOFSHAPE_HALFHIPSKEW    21
#define ROOFSHAPE_SHEDSKEW       22

#define ROOFSHAPE_SHEDCURVED     100
#define ROOFSHAPE_BALINESECURVED 101
#define ROOFSHAPE_CONE           102
#define ROOFSHAPE_CONECURVED     103
#define ROOFSHAPE_HALFHIPLOOP    104
#define ROOFSHAPE_HEMISPHERE     105
#define ROOFSHAPE_HEMISPHEREHALF 106
#define ROOFSHAPE_GULLWINGCURVED 107
#define ROOFSHAPE_HALFHIPCURVED  109




// DialogShow information
typedef struct {
   PCObjectBuildBlock pThis;  // this object
   DWORD       dwSurface;  // surface ID that clicked on
   PBBSURF     pSurf;      // surface that clicked on (if did)
   int         iSide;      // side that clicked on: 1 for side A, -1 for B, 0 for unknown
   PBBBALUSTRADE pBal;     // balustrade that clicked on (if did)
   PCSplineSurface pssAny; // spline surface used for any shape
   BOOL        fWall;      // set to TRUE if mucking with walls, FALSE if vernadah
} DSI, *PDSI;

typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaMoveBB[] = {
   L"Center", 0, 0,
   L"Corner 1", -1, 1,
   L"Corner 2", 1, 1,
   L"Corner 3", -1, -1,
   L"Corner 4", 1, -1
};
#define     SURFACEEDGE       10
#define     SIDEA             100
#define     SIDEB             200
#define     MAXCOLORPERSIDE   99          // maximum number of color settings per side




static BOOL GenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline, PCSplineSurface pss,
                                      DWORD dwUse);
BOOL CutoutBasedOnSpline (PCDoubleSurface pSurf, PCSpline pSpline, BOOL fCutout);
/**********************************************************************************
CObjectBuildBlock::Constructor and destructor

(DWORD)pParams is passed into CDoubleSurface::Init(). See definitions of parameters

 */
CObjectBuildBlock::CObjectBuildBlock (PVOID pParams, POSINFO pInfo)
{
   m_dwType = (DWORD)(size_t) pParams;
   m_OSINFO = *pInfo;

   m_listBBSURF.Init (sizeof(BBSURF));
   m_listWish.Init (sizeof(BBSURF));
   m_lBBBALUSTRADE.Init (sizeof(BBBALUSTRADE));
   m_dwDisplayControl = 0;

   m_fXMin = -4;
   m_fXMax = 4;
   m_fYMin = -4;
   m_fYMax = 4;
   m_fHeight = FLOORHEIGHT_TROPICAL;
   m_fRoofOverhang1 = m_fRoofOverhang2 = .5;
   m_dwRoofHeight = 0;  // average of width and depth

   m_lRoofControl.Init (sizeof(CPoint));
   m_dwRoofControlFreedom = 0;

   m_lWallAngle.Init(sizeof(fp));
   m_lVerandahAngle.Init (sizeof(fp));

   m_fRoofThickStruct = CM_THICKROOFBEAM;
   m_fRoofThickSideA = CM_THICKROOFING;
   m_fRoofThickSideB = CM_THICKSHEETROCK;
   m_fWallThickStruct = CM_THICKSTUDWALL;
   m_fWallThickSideA = CM_THICKEXTNERALCLADDING;
   m_fWallThickSideB = CM_THICKSHEETROCK;
   m_fFloorThickStruct = CM_THICKFLOORJOIST;
   m_fFloorThickSideA = CM_THICKWOODFLOOR;
   m_fFloorThickSideB = CM_THICKSHEETROCK;
   m_fBasementThickStruct = CM_THICKCEMENTBLOCK;
   m_fBasementThickSideA = CM_THICKRENDER;
   m_fBasementThickSideB = CM_THICKRENDER;
   m_fPadThickStruct = CM_THICKPAD;
   m_fPadThickSideA = CM_THICKTILEONLY;
   m_fPadThickSideB = .000;
   m_fCeilingThickStruct = CM_THICKDROPCEILING;
   m_fCeilingThickSideA = .000;
   m_fCeilingThickSideB = CM_THICKSHEETROCK;

   m_fCathedral = TRUE;
   m_fFloorsIntoRoof = FALSE;
   m_dwFoundation = 1;  // piers
   m_fPerimDepth = -1;
   m_fFoundWall = FALSE;

   m_fLevelGlobal = TRUE;
   m_fRestrictElevation = TRUE;
   m_fRestrictHeight = FALSE;
   m_fKeepFloor = FALSE;
   m_fKeepWalls = FALSE;
   m_fLevelElevation[0] = 1 - FLOORHEIGHT_TROPICAL;
   m_dwGroundFloor = 1;
   DWORD i;
   for (i = 1; i < NUMLEVELS; i++)
      m_fLevelElevation[i] = m_fLevelElevation[i-1] + FLOORHEIGHT_TROPICAL;
   m_fLevelHigher = FLOORHEIGHT_TROPICAL;
   m_fLevelMinDist = 2.0;



   DWORD dwClimate;
   BOOL fTwoFloors;
   PWSTR psz;
   PCWorldSocket pWorld;
   pWorld = WorldGet(m_OSINFO.dwRenderShard, NULL);
   psz = pWorld ? pWorld->VariableGet (WSClimate()) : NULL;
   dwClimate = 0;
   fTwoFloors = FALSE;
   if (psz)
      dwClimate = (DWORD) _wtoi(psz);
   else
      DefaultBuildingSettings (NULL, NULL, NULL, &dwClimate);  // BUGFIX - Default building settings
   switch (dwClimate) {
   case 0:  // tropical
   case 5:  // arid
      m_fRoofOverhang1 *= 3.0 / 2.0;
      m_fRoofOverhang2 *= 3.0 / 2.0;
      break;
   case 1:  // sub-tropical
   case 6:  // desert
      // no change
      break;
   case 2:  // temperate
   case 7:  // tundra
   case 3:  // alpine
      m_fRoofOverhang1 /= 2;
      m_fRoofOverhang2 /= 2;
      m_fCathedral = FALSE;
      fTwoFloors = TRUE;
      break;
   }

   psz = pWorld ? pWorld->VariableGet (WSFoundation()) : NULL;
   m_dwFoundation = 1;
   if (psz)
      m_dwFoundation = (DWORD) _wtoi(psz);
   else
      DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, NULL, &m_dwFoundation);  // BUGFIX - extenral

   DWORD dwExternalWall;
   psz = pWorld ? pWorld->VariableGet (WSExternalWalls()) : NULL;
   dwExternalWall = 0;
   if (psz)
      dwExternalWall = (DWORD) _wtoi(psz);
   else
      DefaultBuildingSettings (NULL, NULL, NULL, NULL, NULL, &dwExternalWall);  // BUGFIX - extenral
   switch (dwExternalWall) {
   case 0:  // corrogated
   default:
      break;
   case 1:  // brick veneer
      m_fWallThickSideA = CM_THICKBRICKVENEER;
      break;
   case 2:  // clapboards
      break;
   case 3:  // stucco
      break;
   case 4:  // brick
      m_fFoundWall = TRUE;
      m_fWallThickStruct = CM_THICKBRICKWALL;
      break;
   case 5:  // stone
      m_fFoundWall = TRUE;
      m_fWallThickStruct = CM_THICKSTONEWALL;
      break;
   case 6:  // cement block
      m_fFoundWall = TRUE;
      m_fWallThickStruct = CM_THICKCEMENTBLOCK;
      break;
   case 7:  // hay bale
      m_fWallThickStruct = CM_THICKHAYBALE;
      break;
   case 8:  // logs
      m_fWallThickStruct = CM_THICKLOGWALL;
      break;
   }

   m_dwWallType = WALLSHAPE_RECTANGLE;
   m_dwVerandahType = WALLSHAPE_NONE;  // start out with no verandah
   m_fVerandahEveryFloor = FALSE;
   m_dwBalEveryFloor = 0;
   m_dwRoofType = LOWORD(m_dwType);
   m_fRoofHeight = 0;

   // HIWORD says what type
   switch (HIWORD(m_dwType)) {
   case 1:  // main buildinb
      m_fRestrictHeight = TRUE;
      if (fTwoFloors)
         m_fHeight *= 2;
      break;
   case 2:  // dormer
      m_fXMin /= 6;
      m_fXMax /= 6;
      m_fYMin /= 3;
      m_fYMax /= 3;
      m_fRoofOverhang1 /= 2;
      m_fRoofOverhang2 /= 2;
      m_fHeight *= 2.0 / 3.0;
      m_fCathedral = TRUE;
      m_dwFoundation = 0;  // no foundation
      break;
   case 3:  // bay
      m_fXMin /= 4;
      m_fXMax /= 4;
      if (m_dwRoofType == ROOFSHAPE_HEXAGONPEAK) {
         m_fYMin /= 4;
         m_fYMax /= 4;
      }
      else {
         m_fYMin /= 8;
         m_fYMax /= 8;
      }
      m_fRoofOverhang1 /= 2;
      m_fRoofOverhang2 /= 2;
      m_fHeight *= 2.0 / 3.0;
      m_fCathedral = TRUE;
      break;
   case 4:  // bay window
      m_fRestrictElevation = FALSE;

      m_fXMin /= 6;
      m_fXMax /= 6;
      if (m_dwRoofType == ROOFSHAPE_HEXAGONPEAK) {
         m_fYMin /= 6;
         m_fYMax /= 6;
      }
      else {
         m_fYMin /= 12;
         m_fYMax /= 12;
      }
      m_fRoofOverhang1 /= 2;
      m_fRoofOverhang2 /= 2;
      m_fHeight /= 2;
      m_fCathedral = TRUE;
      m_dwFoundation = 0;  // no foundation
      break;
   case 5:  // verandah
      m_fXMin /= 2;
      m_fXMax /= 2;
      m_fYMin /= 2;
      m_fYMax /= 2;
      m_fRoofOverhang1 /= 2;
      m_fRoofOverhang2 /= 2;
      m_fHeight *= 3.0 / 4.0;
      m_fCathedral = TRUE;
      m_dwVerandahType = WALLSHAPE_RECTANGLE;
      m_dwWallType = WALLSHAPE_NONE;
      break;
   case 6:  // deck
      m_fXMin /= 2;
      m_fXMax /= 2;
      m_fYMin /= 2;
      m_fYMax /= 2;
      m_fRoofOverhang1 /= 2;
      m_fRoofOverhang2 /= 2;
      m_fHeight *= 3.0 / 4.0;
      m_fCathedral = TRUE;
      m_dwVerandahType = WALLSHAPE_RECTANGLE;
      m_dwWallType = WALLSHAPE_NONE;
      break;

   case 7:  // elevated walkway, enclosed
   case 8:  // elevated walkway, open
      m_fRoofOverhang1 /= 2;
      m_fRoofOverhang2 /= 2;
      m_fHeight *= 3.0 / 4.0;
      m_fCathedral = TRUE;

      if (HIWORD(m_dwType) != 7) {
         m_dwWallType = WALLSHAPE_NONE;
         m_dwVerandahType = WALLSHAPE_RECTANGLE;
      }

      switch (m_dwRoofType) {
      case ROOFSHAPE_SHED:
      case ROOFSHAPE_SHEDFOLDED:
      case ROOFSHAPE_HIPSHEDPEAK:
      case ROOFSHAPE_HIPSHEDRIDGE:
      case ROOFSHAPE_SHEDCURVED:
         m_fYMin /= 6;
         m_fYMax /= 6;
         m_fXMin /= 2;
         m_fXMax /= 2;
         break;
      default:
         m_fXMin /= 6;
         m_fXMax /= 6;
         m_fYMin /= 2;
         m_fYMax /= 2;
         break;
      }
      break;
   }


   DWORD dwWantShape;
   dwWantShape = WALLSHAPE_RECTANGLE;

   // special modifications based on roof type
   switch (m_dwRoofType) {
   case ROOFSHAPE_HIPSHEDPEAK:
   case ROOFSHAPE_HIPSHEDRIDGE:
      if (HIWORD(m_dwType) == 1) {
         // make it half the size on one side
         m_fYMin /= 2;
         m_fYMax /= 2;
      }
      break;

   case ROOFSHAPE_NONE:
      // just raise walls a bit so can see top roof
      m_fHeight += .5;
      break;

   case ROOFSHAPE_TRIANGLEPEAK:
      dwWantShape = WALLSHAPE_TRIANGLE;
      break;

   case ROOFSHAPE_PENTAGONPEAK:
      dwWantShape = WALLSHAPE_PENTAGON;
      break;

   case ROOFSHAPE_HEXAGONPEAK:
      dwWantShape = WALLSHAPE_HEXAGON;
      break;

   case ROOFSHAPE_CONE:
   case ROOFSHAPE_CONECURVED:
   case ROOFSHAPE_HEMISPHERE:
      dwWantShape = WALLSHAPE_CIRCLE;
      break;

   case ROOFSHAPE_HEMISPHEREHALF:
      if (HIWORD(m_dwType) == 1) {
         // make it half the size on one side
         m_fYMin /= 2;
         m_fYMax /= 2;
      }

      // need different wall shapes
      dwWantShape = WALLSHAPE_SEMICIRCLE;
      break;
   }
   if (m_dwWallType != WALLSHAPE_NONE)
      m_dwWallType = dwWantShape;
   if (m_dwVerandahType != WALLSHAPE_NONE)
      m_dwVerandahType = dwWantShape;

   // if it's a deck then no roof
   if (HIWORD(m_dwType) == 6)
      m_dwRoofType = ROOFSHAPE_NONE;

   // keep the old values the same as the new ones so dont
   // try to stretch contained objects
   m_fOldXMin = m_fXMin;
   m_fOldXMax = m_fXMax;
   m_fOldYMin = m_fYMin;
   m_fOldYMax = m_fYMax;
   m_fOldHeight = m_fHeight;

   // Call intialization function to set up m_lRoofControl based on m_dwRoofType
   RoofDefaultControl (m_dwRoofType);
   WallDefaultSpline (m_dwWallType, TRUE);
   WallDefaultSpline (m_dwVerandahType, FALSE);

   m_pOldOMSPoint.Zero();

   m_dwRenderShow = RENDERSHOW_WALLS | RENDERSHOW_FLOORS | RENDERSHOW_ROOFS |
      RENDERSHOW_BALUSTRADES | RENDERSHOW_PIERS | RENDERSHOW_FRAMING;

   AdjustAllSurfaces();

   IntegrityCheck();
}


CObjectBuildBlock::~CObjectBuildBlock (void)
{
   // Need to delete doublesurface objects
   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(i);
      if (ps->pSurface)
         delete ps->pSurface;
   }
   m_listBBSURF.Clear();

   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      PBBBALUSTRADE ps = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(i);
      if (ps->pBal)
         delete ps->pBal;
      if (ps->pPiers)
         delete ps->pPiers;
   }
   m_lBBBALUSTRADE.Clear();
}




/**********************************************************************************
CObjectBuildBlock::AdjustAllSurfaces - This function:
   1) Figures out what surfaces are needed given the building block shape.
   2) If any exist that shouldn't, they're removed.
   3) If any don't exist, they're created with dummy sizes.
   4) Goes through all of them and makes sure they're in the right location
      and the right size.
   5) Clip the walls agasint the roof, and roof against roof, if necessary. (If
      any have changed.)
   6) Remembers to call the world and tell it things have changed (if they have)

It uses the globals for m_fXMin..m_fYMax, m_fHeight, etc. to judge this
inputs
   none
returns
   none
*/
static PWSTR gszWall = L"Wall";
static PWSTR gszFloor = L"Floor";
static PWSTR gszRoof = L"Roof";
static PWSTR gszCeiling = L"Ceiling";
static PWSTR gszBasement = L"Basement";
static PWSTR gszPad = L"Pad";
static PWSTR gszVerandah = L"Verandah";

void CObjectBuildBlock::AdjustAllSurfaces (void)
{
#ifdef _DEBUG
   OutputDebugString ("AdjustAllSurfaces\r\n");
#endif

   m_dwGroundFloor = 1; // to clear
   m_listWish.Clear();
   BalustradeReset ();

   // remeber if sent world changed notifcation
   BOOL  fChanged = FALSE;

   // what floor is this on?
   CMatrix m;
   CPoint zero;
   zero.Zero();
   ObjectMatrixGet (&m);
   zero.MultiplyLeft (&m);

   // loop through current elevations and find
   DWORD i, dwClosest;
   fp fClosest, fDist, fHeight;
   dwClosest = -1;
   fClosest = 0;
   // ignore the basement
   for (i = 1; i < 100; i++) {   // arbitrary max of 100 floors
      if (i < NUMLEVELS)
         fHeight = m_fLevelElevation[i];
      else {
         if (zero.p[2] <= m_fLevelElevation[NUMLEVELS-1])
            break;
         fHeight = m_fLevelElevation[NUMLEVELS-1] + (i-NUMLEVELS+1) * m_fLevelHigher;
      }
      fDist = zero.p[2] - fHeight;

      if ((dwClosest == -1) || (fabs(fDist) < fabs(fClosest))) {
         dwClosest = i;
         fClosest = fDist;
         m_fClosestHeight = fHeight;
      }
      else if (i >= NUMLEVELS)
         break;   // if this one isn't closert than the last, then neither will the next one be
   }

   // if there's no restrictions on elevations then different
   if (!m_fRestrictElevation) {
      dwClosest = 0;
      m_fClosestHeight = 0;
   }

   // figure out if there's a basement, or basement walls, etc.
   fp   fWallsStartAt; // height walls start at, vertical
   fp   fPadHeight;    // height of the pad
   fp   fPerimeterHeight; // How low the perimeter gets to
   fp   fPadX, fPadY;  // width and height of pad
   fp   fFloorX, fFloorY; // width and height of floor
   fp   fBasementX, fBasementY; // width and height of basement
   BOOL     fPad; // if true, then will have a cement pad someplace (as pad or basement)
   BOOL     fPerimeter; // if true then will have a perimiter wall
   fWallsStartAt = -(m_fFloorThickSideA + m_fFloorThickSideB + m_fFloorThickStruct);
      // walls start at the base of the floor level
   fPad = fPerimeter = FALSE;
   fPadHeight = 0;
   fFloorX = (m_fXMax - m_fXMin);
   fFloorY = (m_fYMax - m_fYMin);
   fBasementX = (m_fXMax - m_fXMin) + m_fWallThickStruct - m_fBasementThickStruct;
   fBasementY = (m_fYMax - m_fYMin) + m_fWallThickStruct - m_fBasementThickStruct;
   fPadX = fBasementX;
   fPadY = fBasementY;
   DWORD dwFoundation;
   dwFoundation = m_dwFoundation;
   if (!m_fRestrictElevation)
      dwFoundation = 0;
   if (dwFoundation == 2) { // pad
      fWallsStartAt = 0;   // start at the top of the pad
      fPad = TRUE;
      fPadHeight = -(m_fPadThickSideA + m_fPadThickStruct/2);

      // pad is wider because walls sit on it
      fPadX = (m_fXMax - m_fXMin) + m_fWallThickStruct;
      fPadY = (m_fYMax - m_fYMin) + m_fWallThickStruct;

   }
   else if (dwFoundation == 3) {  // perimeter
      fPerimeter = TRUE;
      fPerimeterHeight = m_fPerimDepth - m_fClosestHeight - (m_fPadThickSideA + m_fPadThickSideB + m_fPadThickStruct);
      fPerimeterHeight = min(fWallsStartAt - .1, fPerimeterHeight);
   }
   else if (dwFoundation == 4) {  // basement
      fPad = TRUE;
      fPadHeight = m_fLevelElevation[0] - m_fClosestHeight  - (m_fPadThickSideA + m_fPadThickStruct/2);
      fPerimeter = TRUE;
      fPerimeterHeight = m_fLevelElevation[0] - m_fClosestHeight  - (m_fPadThickSideA + m_fPadThickSideB + m_fPadThickStruct);
      fPerimeterHeight = min(fWallsStartAt - .1, fPerimeterHeight);
   }

   // if the walls can be used as a foundation then send them all the way down
   if (m_fFoundWall && ((dwFoundation == 3) || (dwFoundation == 4))) {
      // using normal walls for foundation
      fPadX = fFloorX;
      fPadY = fFloorY;
      fPerimeter = FALSE;
      fWallsStartAt = fPerimeterHeight;
   }

   // start and end floor
   DWORD dwStartFloor;
   fp fCeilingHeight;  // center of the ceiling
   dwStartFloor = m_dwGroundFloor = dwClosest;
   if (m_dwFoundation == 2)
      dwStartFloor++;   // since will be pad on ground floor
   fCeilingHeight = 0;

   BBSURF         bbs;
   memset (&bbs, 0, sizeof(bbs));

   // make the roof
   fp fRoofHeight;
   MakeTheRoof (&fRoofHeight, &fCeilingHeight);

   // if allow floors into the roof then the ceiling height = roofheight
   if (m_fFloorsIntoRoof)
      fCeilingHeight = fRoofHeight;

   MakeTheWalls (fWallsStartAt, fRoofHeight, fPerimeter ? fPerimeterHeight : fWallsStartAt);

   PBBSURF pbbs;

   // keep track of what floors are drawn for use with the verandah
   CListFixed lFloors;
   BOOL fFirstFloorPad;
   lFloors.Init (sizeof(fp));
   fFirstFloorPad = FALSE;

   // reember this for patio
   if (fPad && (dwFoundation == 2)) {
      lFloors.Add (&fPadHeight);
      fFirstFloorPad = TRUE;
   }

   // do the pad
   if (fPad && (m_dwWallType != WALLSHAPE_NONE)) {
      memset (&bbs, 0, sizeof(bbs));
      wcscpy (bbs.szName, gszPad);
      bbs.dwMajor = BBSURFMAJOR_FLOOR;
      bbs.dwType = 0x30001;
      bbs.fRotX = -PI/2;
      bbs.fThickStud = m_fPadThickStruct;
      bbs.fThickSideA = m_fPadThickSideA;
      bbs.fThickSideB = m_fPadThickSideB;
      bbs.fHideEdges = FALSE;
      bbs.pTrans.Zero();
      bbs.pTrans.p[0] = (m_fXMin + m_fXMax) / 2;
      bbs.pTrans.p[1] = (m_fYMin + m_fYMax) / 2 - fPadY/2;
      bbs.pTrans.p[2] = fPadHeight;
      bbs.fWidth = fPadX;
      bbs.fHeight = fPadY;
      bbs.dwFlags = BBSURFFLAG_CLIPAGAINSRROOFWALLS;
      bbs.dwMinor = 0;
      if ((m_fOldXMin == m_fXMin) && (m_fOldXMax != m_fXMax))
         bbs.dwStretchHInfo = 2;   // moving the right hand side
      else if ((m_fOldXMin != m_fXMin) && (m_fOldXMax == m_fXMax))
         bbs.dwStretchHInfo = 1;   // moving the left hand side
      if ((m_fOldYMin == m_fYMin) && (m_fOldYMax != m_fYMax))
         bbs.dwStretchVInfo = 1;   // moving the top
      else if ((m_fOldYMin != m_fYMin) && (m_fOldYMax == m_fYMax))
         bbs.dwStretchVInfo = 2;   // moving the bottom
      pbbs = WishListAdd (&bbs);

      // set edge of floor to walls
      if (pbbs) {
         PCSpline pSpline = WallSplineAtHeight (fPadHeight, TRUE);
         if (pSpline) {
            CutoutBasedOnSpline (pbbs->pSurface, pSpline, FALSE);
            delete pSpline;
         }
      }

   }

   // Do floors
   PBBSURF pTopFloor;
   pTopFloor = NULL;
   for (i = dwStartFloor; ; i++) {
      if (m_fRestrictElevation) {
         if (i < NUMLEVELS)
            fHeight = m_fLevelElevation[i];
         else 
            fHeight = m_fLevelElevation[NUMLEVELS-1] + (i-NUMLEVELS+1) * m_fLevelHigher;
         }
      else
         fHeight = 0;
      fHeight -= (m_fFloorThickSideA + m_fFloorThickStruct/2);
         // so that floor ends up at this height
      fHeight -= m_fClosestHeight;
      if (m_fRestrictElevation && (i > dwClosest) && (fHeight + m_fLevelMinDist > fCeilingHeight))
         break;   // no more floors - although make sure at least one

      // reember this for patio
      lFloors.Add (&fHeight);

      // if there aren't any walls the dont do the rest
      if (m_dwWallType == WALLSHAPE_NONE) {
         // BUGFIX - Was hanging when adjusted height of deck
         if (!m_fRestrictElevation)
            break;

         continue;
      }

      memset (&bbs, 0, sizeof(bbs));
      wcscpy (bbs.szName, gszFloor);
      bbs.dwMajor = BBSURFMAJOR_FLOOR;
      bbs.dwType = 0x30002;
      bbs.fRotX = -PI/2;
      bbs.fThickStud = m_fFloorThickStruct;
      bbs.fThickSideA = m_fFloorThickSideA;
      bbs.fThickSideB = m_fFloorThickSideB;
      bbs.fHideEdges = FALSE;
      bbs.pTrans.Zero();
      bbs.pTrans.p[0] = (m_fXMin + m_fXMax) / 2;
      bbs.pTrans.p[1] = m_fYMin;
      bbs.pTrans.p[2] = fHeight;
      bbs.fWidth = (m_fXMax - m_fXMin);
      bbs.fHeight = (m_fYMax - m_fYMin);
      bbs.fDontIncludeInVolume = !(i == dwClosest);   // only include the first floor

      // if there's a pad, dont include this
      if (fPad)
         bbs.fDontIncludeInVolume = TRUE;

      bbs.dwFlags = BBSURFFLAG_CLIPAGAINSRROOFWALLS;
      bbs.dwMinor = i;
      if ((m_fOldXMin == m_fXMin) && (m_fOldXMax != m_fXMax))
         bbs.dwStretchHInfo = 2;   // moving the right hand side
      else if ((m_fOldXMin != m_fXMin) && (m_fOldXMax == m_fXMax))
         bbs.dwStretchHInfo = 1;   // moving the left hand side
      if ((m_fOldYMin == m_fYMin) && (m_fOldYMax != m_fYMax))
         bbs.dwStretchVInfo = 1;   // moving the top
      else if ((m_fOldYMin != m_fYMin) && (m_fOldYMax == m_fYMax))
         bbs.dwStretchVInfo = 2;   // moving the bottom
      pbbs = WishListAdd (&bbs);

      pTopFloor = pbbs;


      // set edge of floor to walls
      if (pbbs) {
         PCSpline pSpline = WallSplineAtHeight (fHeight, TRUE);
         if (pSpline) {
            CutoutBasedOnSpline (pbbs->pSurface, pSpline, FALSE);
            delete pSpline;
         }

         // if this is the ground floor and the type is piers, then add those
         if ((i == dwStartFloor) && (m_dwFoundation == 1)) {
            fp fBalHeight;
            fBalHeight = bbs.pTrans.p[2] - bbs.fThickStud/2 - bbs.fThickSideB;
            CMatrix m;
            pbbs->pSurface->MatrixGet (&m);
            BalustradeAdd (BBBALMAJOR_PIERS, bbs.dwMinor, fBalHeight, fBalHeight-1,TRUE,
               &pbbs->pSurface->m_SplineB, &m);
         }
      }


      // only allow one floor with restricted
      if (!m_fRestrictElevation)
         break;
   }

   // if there's a top floor and we're marked as no roof, then include this
   if (pTopFloor && (m_dwRoofType == ROOFSHAPE_NONE))
      pTopFloor->fDontIncludeInVolume = FALSE;

   // Do top floor ceiling (if necessary)
   if (!m_fCathedral && !m_fFloorsIntoRoof && (m_dwRoofType != ROOFSHAPE_NONE)) {
      memset (&bbs, 0, sizeof(bbs));
      wcscpy (bbs.szName, gszCeiling);
      bbs.dwMajor = BBSURFMAJOR_TOPCEILING;
      bbs.dwType = 0x30003;
      bbs.fRotX = -PI/2;
      bbs.fThickStud = m_fCeilingThickStruct;
      bbs.fThickSideA = m_fCeilingThickSideA;
      bbs.fThickSideB = m_fCeilingThickSideB;
      bbs.fHideEdges = TRUE;
      bbs.dwFlags = BBSURFFLAG_CLIPAGAINSRROOFWALLS;
      bbs.pTrans.Zero();
      bbs.pTrans.p[0] = (m_fXMin + m_fXMax) / 2;
      bbs.pTrans.p[1] = m_fYMin;
      bbs.pTrans.p[2] = fCeilingHeight;
      bbs.fWidth = (m_fXMax - m_fXMin);
      bbs.fHeight = (m_fYMax - m_fYMin);
      bbs.dwMinor = 1000;
      bbs.fDontIncludeInVolume = TRUE; // never include cathedral ceiling
      if ((m_fOldXMin == m_fXMin) && (m_fOldXMax != m_fXMax))
         bbs.dwStretchHInfo = 2;   // moving the right hand side
      else if ((m_fOldXMin != m_fXMin) && (m_fOldXMax == m_fXMax))
         bbs.dwStretchHInfo = 1;   // moving the left hand side
      if ((m_fOldYMin == m_fYMin) && (m_fOldYMax != m_fYMax))
         bbs.dwStretchVInfo = 1;   // moving the top
      else if ((m_fOldYMin != m_fYMin) && (m_fOldYMax == m_fYMax))
         bbs.dwStretchVInfo = 2;   // moving the bottom
      pbbs = WishListAdd (&bbs);

      // set edge of floor to walls
      if (pbbs && (m_dwWallType != WALLSHAPE_NONE)) {
         PCSpline pSpline = WallSplineAtHeight (fCeilingHeight, TRUE);
         if (pSpline) {
            CutoutBasedOnSpline (pbbs->pSurface, pSpline, FALSE);
            delete pSpline;
         }
      }

   }

   m_fRoofHeight = 0;

   // draw the verandah floors
   if (m_dwVerandahType != WALLSHAPE_NONE) for (i = 0; i < lFloors.Num(); i++) {
      fp fHeight = *((fp*) lFloors.Get(i));
      BOOL fIsPad = (!i && fFirstFloorPad);

      // if only supposed to be on ground floor then do that.
      if (!m_fVerandahEveryFloor && i)
         break;

      // remember the last roof height - for puroses of verandah clipping
      m_fRoofHeight = fHeight+1;

      memset (&bbs, 0, sizeof(bbs));
      wcscpy (bbs.szName, gszVerandah);
      bbs.dwMajor = BBSURFMAJOR_VERANDAH;
      bbs.dwType = fIsPad ? 0x30004 : 0x30005;
      bbs.fRotX = -PI/2;
      bbs.fThickStud = fIsPad ? m_fPadThickStruct : m_fFloorThickStruct;
      bbs.fThickSideA = fIsPad ? m_fPadThickSideA : m_fFloorThickSideA;
      bbs.fThickSideB = fIsPad ? m_fPadThickSideB : m_fFloorThickSideB;
      bbs.fHideEdges = fIsPad ? FALSE : TRUE;
      bbs.pTrans.Zero();
      bbs.pTrans.p[0] = (m_fXMin + m_fXMax) / 2;
      bbs.pTrans.p[1] = m_fYMin;
      bbs.pTrans.p[2] = fHeight;
      bbs.fWidth = (m_fXMax - m_fXMin);
      bbs.fHeight = (m_fYMax - m_fYMin);
      bbs.fDontIncludeInVolume = TRUE; // never include this in volume
      bbs.dwFlags = BBSURFFLAG_CLIPAGAINSRROOFWALLS;
      bbs.dwMinor = (fIsPad ? 1000 : 1001) + i;

      // leaving this in to stretch - but may cause problems at some point
      if ((m_fOldXMin == m_fXMin) && (m_fOldXMax != m_fXMax))
         bbs.dwStretchHInfo = 2;   // moving the right hand side
      else if ((m_fOldXMin != m_fXMin) && (m_fOldXMax == m_fXMax))
         bbs.dwStretchHInfo = 1;   // moving the left hand side
      if ((m_fOldYMin == m_fYMin) && (m_fOldYMax != m_fYMax))
         bbs.dwStretchVInfo = 1;   // moving the top
      else if ((m_fOldYMin != m_fYMin) && (m_fOldYMax == m_fYMax))
         bbs.dwStretchVInfo = 2;   // moving the bottom

      pbbs = WishListAdd (&bbs);

      // NOTE: A pad could be entirely cut out due to the floor, but still left around.

      // set edge of floor to walls
      if (pbbs) {
         // remove the walls
         PCSpline pSpline;
         if (m_dwWallType != WALLSHAPE_NONE) {
            pSpline = WallSplineAtHeight (fHeight, TRUE);
            if (pSpline) {
               CutoutBasedOnSpline (pbbs->pSurface, pSpline, TRUE);
               delete pSpline;
            }
         }
         else
            CutoutBasedOnSpline (pbbs->pSurface, NULL, TRUE);

         // edge for the pad
         pSpline = WallSplineAtHeight (fHeight, FALSE);
         if (pSpline) {
            CutoutBasedOnSpline (pbbs->pSurface, pSpline, FALSE);
            delete pSpline;
         }

         // BUGFIX - Don't always have balustrade
         BOOL fHaveBal;
         fHaveBal = TRUE;
         if (m_dwBalEveryFloor == 1)
            fHaveBal = (i >= 1);
         else if (m_dwBalEveryFloor == 2)
            fHaveBal = FALSE;
         fp fBalHeight;
         if (fHaveBal) {
            // add the balustrades
            CMatrix m;
            pbbs->pSurface->MatrixGet (&m);
            fBalHeight = bbs.pTrans.p[2] + bbs.fThickStud/2 + bbs.fThickSideA;
            BalustradeAdd (BBBALMAJOR_BALUSTRADE, bbs.dwMinor, fBalHeight+1, fBalHeight, FALSE,
               &pbbs->pSurface->m_SplineA, &m);
         }

         // if this is the ground floor and the type is piers, then add those
         // Verandah can have peirs if it's not a pad or none. piers, basement, or perimeter ok
         if (!i && (m_dwFoundation != 2) && (m_dwFoundation != 0)) {
            fBalHeight = bbs.pTrans.p[2] - bbs.fThickStud/2 - bbs.fThickSideB;
            BalustradeAdd (BBBALMAJOR_PIERS, bbs.dwMinor, fBalHeight, fBalHeight-1,FALSE,
               &pbbs->pSurface->m_SplineB, &m);
         }
      }

   }


   // look for ones that exist that shouldn't
   DWORD j;
   PBBSURF pLookFor, pLookAt;
   for (i = m_listBBSURF.Num()-1; i < m_listBBSURF.Num(); i--) {
      pLookFor = (PBBSURF) m_listBBSURF.Get(i);

      // see if can find it
      for (j = 0; j < m_listWish.Num(); j++) {
         pLookAt = (PBBSURF) m_listWish.Get(j);
         if ((pLookFor->dwMajor == pLookAt->dwMajor) && (pLookFor->dwMinor == pLookAt->dwMinor))
            break;
      }
      if (j >= m_listWish.Num()) {
         // world about to change
         if (!fChanged && m_pWorld) {
            m_pWorld->ObjectAboutToChange (this);
            fChanged = TRUE;
         }

         // FUTURERELEASE - If any objects embedded in this the delete them

         // didn't find it on the wish list, so remove it
         pLookFor->pSurface->ClaimClear();
         delete pLookFor->pSurface;
         m_listBBSURF.Remove(i);
      }
   }

   // get rid of deal balustrade
   BalustradeRemoveDead ();


   // if stuff changed then may need to readjust clipping
   for (i = m_listBBSURF.Num()-1; i < m_listBBSURF.Num(); i--) {
      pLookFor = (PBBSURF) m_listBBSURF.Get(i);

      switch (pLookFor->dwMajor) {
      case BBSURFMAJOR_WALL:
         // If any walls/roofs changed, and they need to be clipped against
         // other walls/roofs in the object then do so
         if (pLookFor->dwFlags & BBSURFFLAG_CLIPAGAINSTROOF) {
            if (!fChanged && m_pWorld) {
               m_pWorld->ObjectAboutToChange (this);
               fChanged = TRUE;
            }

            // find all the roofs and set edge based on that
            // BUGFIX - Changed this from 0x0002, 0 to 0x0002, 4 because was
            // mucking with the intersection code while getting the paint-overlay
            // working and broke it
            // BUGFIX - Make "ignore cutouts" = FALSE otherwise invisible roof segments intersect
            pLookFor->pSurface->IntersectWithOtherSurfaces (0x0002, 4, NULL, FALSE);
         }
         break;
      case BBSURFMAJOR_FLOOR:
      case BBSURFMAJOR_VERANDAH:
         // If any floors changed and they need to be clipped against any
         // walls then do so
         if (pLookFor->dwFlags & BBSURFFLAG_CLIPAGAINSRROOFWALLS) {
            if (!fChanged && m_pWorld) {
               m_pWorld->ObjectAboutToChange (this);
               fChanged = TRUE;
            }

            // find all the roofs and set edge based on that
            pLookFor->pSurface->IntersectWithOtherSurfaces (0x0002, 3, L"IntersectRoof", FALSE);
         }
         break;
      case BBSURFMAJOR_TOPCEILING:
         // If any floors ceilings and they need to be clipped against any
         // walls then do so
         if (pLookFor->dwFlags & BBSURFFLAG_CLIPAGAINSRROOFWALLS) {
            if (!fChanged && m_pWorld) {
               m_pWorld->ObjectAboutToChange (this);
               fChanged = TRUE;
            }

            // find all the roofs and set edge based on that
            pLookFor->pSurface->IntersectWithOtherSurfaces (0x0002, 3, L"IntersectRoof", FALSE);
         }
         break;
      }
   }

   // extend the balustrades to roof lines
   BalustradeExtendToRoof();

   // finally, if changed then send finish changed notification
   if (fChanged && m_pWorld)
      m_pWorld->ObjectChanged (this);
}

/**********************************************************************************
CObjectBuildBlock::Delete - Called to delete this object
*/
void CObjectBuildBlock::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectBuildBlock::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectBuildBlock::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   // create the surface render object and draw
   // CRenderSurface rs;
   m_Renderrs.ClearAll();

   CMatrix mObject;
   m_Renderrs.Init (pr->pRS);

//#define TESTCLIP
#ifdef TESTCLIP
   if (pr->dwReason != ORREASON_BOUNDINGBOX) {
      CListFixed l;
      ToClippingVolume (&l);
      DWORD i;
      BBCLIPVOLUME *pbb;
      for (i = 0; i < l.Num(); i++) {
         pbb = (PBBCLIPVOLUME) l.Get(i);
         //if (pbb->iSide != -1)
         //if (pbb->iSide != 1)
         if (pbb->iSide != 0)
            continue;

         m_Renderrs.Push();
         m_Renderrs.Multiply (&pbb->mToWorld);
         // pbb->pss->Render (&m_Renderrs, TRUE /* backface cull*/);
         pbb->pss->Render (&m_Renderrs, FALSE);
         m_Renderrs.Pop();
      }
      for (i = 0; i < l.Num(); i++) {
         pbb = (PBBCLIPVOLUME) l.Get(i);

         // free up
         delete pbb->pss;
      }
      return;
   }

#endif

   // real code
   ObjectMatrixGet (&mObject);
   m_Renderrs.Multiply (&mObject);

   DWORD i;
   PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(0);
   PBBBALUSTRADE pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (0);
   DWORD dwNumSurf = m_listBBSURF.Num();
   DWORD dwNumBal = m_lBBBALUSTRADE.Num();

   DWORD dwStart, dwEnd;
   if (dwSubObject == (DWORD)-1) {
      dwStart = 0;
      dwEnd = dwNumSurf + dwNumBal;
   }
   else {
      dwStart = dwSubObject;
      dwEnd = min(dwSubObject+1, dwNumSurf + dwNumBal);
   }

   for (i = dwStart; i < dwEnd; i++) {
      if (i < dwNumSurf) {
         // surface
         if (pbbs[i].fHidden)
            continue;
         pbbs[i].pSurface->Render (m_OSINFO.dwRenderShard, pr, &m_Renderrs);
      }
      else {
         if (pbb[i-dwNumSurf].pBal)
            pbb[i-dwNumSurf].pBal->Render(m_OSINFO.dwRenderShard, pr, &m_Renderrs);
         if (pbb[i-dwNumSurf].pPiers)
            pbb[i-dwNumSurf].pPiers->Render(m_OSINFO.dwRenderShard, pr, &m_Renderrs);
      }
   } // i

   m_Renderrs.Commit();
}




/**********************************************************************************
CObjectBuildBlock::QueryBoundingBox - Standard API
*/
void CObjectBuildBlock::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   BOOL fSet = FALSE;
   pCorner1->Zero();
   pCorner2->Zero();

   DWORD i;
   CPoint p1, p2;
   PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(0);
   PBBBALUSTRADE pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (0);
   DWORD dwNumSurf = m_listBBSURF.Num();
   DWORD dwNumBal = m_lBBBALUSTRADE.Num();

   DWORD dwStart, dwEnd;
   if (dwSubObject == (DWORD)-1) {
      dwStart = 0;
      dwEnd = dwNumSurf + dwNumBal;
   }
   else {
      dwStart = dwSubObject;
      dwEnd = min(dwSubObject+1, dwNumSurf + dwNumBal);
   }

   for (i = dwStart; i < dwEnd; i++) {
      if (i < dwNumSurf) {
         // surface
         if (pbbs[i].fHidden)
            continue;
         pbbs[i].pSurface->QueryBoundingBox (&p1, &p2);

         if (fSet) {
            pCorner1->Min(&p1);
            pCorner2->Max(&p2);
         }
         else {
            pCorner1->Copy(&p1);
            pCorner2->Copy(&p2);
            fSet = TRUE;
         }
      }
      else {
         if (pbb[i-dwNumSurf].pBal) {
            pbb[i-dwNumSurf].pBal->QueryBoundingBox (&p1, &p2);

            if (fSet) {
               pCorner1->Min(&p1);
               pCorner2->Max(&p2);
            }
            else {
               pCorner1->Copy(&p1);
               pCorner2->Copy(&p2);
               fSet = TRUE;
            }
         }
         if (pbb[i-dwNumSurf].pPiers) {
            pbb[i-dwNumSurf].pPiers->QueryBoundingBox (&p1, &p2);

            if (fSet) {
               pCorner1->Min(&p1);
               pCorner2->Max(&p2);
            }
            else {
               pCorner1->Copy(&p1);
               pCorner2->Copy(&p2);
               fSet = TRUE;
            }
         }
      }
   } // i


#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectBuildBlock::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectBuildBlock::QuerySubObjects - Standard API
*/
DWORD CObjectBuildBlock::QuerySubObjects (void)
{
   return m_listBBSURF.Num() + m_lBBBALUSTRADE.Num();
}


/**********************************************************************************
CObjectBuildBlock::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectBuildBlock::Clone (void)
{
   PCObjectBuildBlock pNew;

   pNew = new CObjectBuildBlock((PVOID)(size_t) m_dwType, &m_OSINFO);

   // clone template info
   CloneTemplate(pNew);

   // variables
   pNew->m_dwDisplayControl = m_dwDisplayControl;
   pNew->m_fXMin = m_fXMin;
   pNew->m_fXMax = m_fXMax;
   pNew->m_fYMin = m_fYMin;
   pNew->m_fYMax = m_fYMax;
   pNew->m_fHeight = m_fHeight;
   pNew->m_dwRoofHeight = m_dwRoofHeight;
   pNew->m_fRoofOverhang1 = m_fRoofOverhang1;
   pNew->m_fRoofOverhang2 = m_fRoofOverhang2;
   pNew->m_fRoofThickStruct = m_fRoofThickStruct;
   pNew->m_fRoofThickSideA = m_fRoofThickSideA;
   pNew->m_fRoofThickSideB = m_fRoofThickSideB;
   pNew->m_fWallThickStruct = m_fWallThickStruct;
   pNew->m_fWallThickSideA = m_fWallThickSideA;
   pNew->m_fWallThickSideB = m_fWallThickSideB;
   pNew->m_fFloorThickStruct = m_fFloorThickStruct;
   pNew->m_fFloorThickSideA = m_fFloorThickSideA;
   pNew->m_fFloorThickSideB = m_fFloorThickSideB;
   pNew->m_fBasementThickStruct = m_fBasementThickStruct;
   pNew->m_fBasementThickSideA = m_fBasementThickSideA;
   pNew->m_fBasementThickSideB = m_fBasementThickSideB;
   pNew->m_fPadThickStruct = m_fPadThickStruct;
   pNew->m_fPadThickSideA = m_fPadThickSideA;
   pNew->m_fPadThickSideB = m_fPadThickSideB;
   pNew->m_fCeilingThickStruct = m_fCeilingThickStruct;
   pNew->m_fCeilingThickSideA = m_fCeilingThickSideA;
   pNew->m_fCeilingThickSideB = m_fCeilingThickSideB;

   pNew->m_fCathedral = m_fCathedral;
   pNew->m_fFloorsIntoRoof = m_fFloorsIntoRoof;

   pNew->m_dwFoundation = m_dwFoundation;
   pNew->m_fPerimDepth = m_fPerimDepth;
   pNew->m_fFoundWall = m_fFoundWall;
   memcpy (pNew->m_fLevelElevation, m_fLevelElevation, sizeof(m_fLevelElevation));
   pNew->m_dwGroundFloor = m_dwGroundFloor;
   pNew->m_fLevelGlobal = m_fLevelGlobal;
   pNew->m_fRestrictElevation = m_fRestrictElevation;
   pNew->m_fRestrictHeight = m_fRestrictHeight;
   pNew->m_fKeepFloor = m_fKeepFloor;
   pNew->m_fKeepWalls = m_fKeepWalls;
   pNew->m_fLevelHigher = m_fLevelHigher;
   pNew->m_fLevelMinDist = m_fLevelMinDist;

   pNew->m_lRoofControl.Init (sizeof(CPoint), m_lRoofControl.Get(0), m_lRoofControl.Num());
   pNew->m_dwRoofType = m_dwRoofType;
   pNew->m_dwRoofControlFreedom = m_dwRoofControlFreedom;

   // wall information
   m_sWall.CloneTo (&pNew->m_sWall);
   pNew->m_dwWallType = m_dwWallType;
   pNew->m_lWallAngle.Init (sizeof(fp), m_lWallAngle.Get(0), m_lWallAngle.Num());

   // wall information
   m_sVerandah.CloneTo (&pNew->m_sVerandah);
   pNew->m_dwVerandahType = m_dwVerandahType;
   pNew->m_fVerandahEveryFloor = m_fVerandahEveryFloor;
   pNew->m_dwBalEveryFloor = m_dwBalEveryFloor;
   pNew->m_fRoofHeight = m_fRoofHeight;
   pNew->m_lVerandahAngle.Init (sizeof(fp), m_lVerandahAngle.Get(0), m_lVerandahAngle.Num());

   // clone all the surfaces
   DWORD i;
   PBBSURF pbbs;
   BBSURF bbs;
   for (i = 0; i < pNew->m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) pNew->m_listBBSURF.Get(i);
      if (ps->pSurface)
         delete ps->pSurface;
   }
   pNew->m_listBBSURF.Clear();
   pNew->m_listBBSURF.Required (m_listBBSURF.Num());
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      bbs = *pbbs;
      bbs.pSurface = new CDoubleSurface;
      if (!bbs.pSurface)
         continue;

      // initialize but dont create
      bbs.pSurface->InitButDontCreate (bbs.dwType, pNew);
      pbbs->pSurface->CloneTo (bbs.pSurface, pNew);
      pNew->m_listBBSURF.Add (&bbs);
   }

   {
      PBBBALUSTRADE pbbs;
      BBBALUSTRADE bbs;
      for (i = 0; i < pNew->m_lBBBALUSTRADE.Num(); i++) {
         PBBBALUSTRADE ps = (PBBBALUSTRADE) pNew->m_lBBBALUSTRADE.Get(i);
         if (ps->pBal)
            delete ps->pBal;
         if (ps->pPiers)
            delete ps->pPiers;
      }
      pNew->m_lBBBALUSTRADE.Clear();
      pNew->m_lBBBALUSTRADE.Required (m_lBBBALUSTRADE.Num());
      for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
         pbbs = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(i);
         bbs = *pbbs;

         // initialize but dont create
         if (pbbs->pBal) {
            bbs.pBal = new CBalustrade;
            if (!bbs.pBal)
               continue;

            // initialize but dont create
            bbs.pBal->InitButDontCreate (m_OSINFO.dwRenderShard, 0/*dwType*/, pNew);
            pbbs->pBal->CloneTo (bbs.pBal, pNew);
         }
         if (pbbs->pPiers) {
            bbs.pPiers = new CPiers;
            if (!bbs.pPiers)
               continue;

            // initialize but dont create
            bbs.pPiers->InitButDontCreate (m_OSINFO.dwRenderShard, 0/*dwType*/, pNew);
            pbbs->pPiers->CloneTo (bbs.pPiers, pNew);
         }
         pNew->m_lBBBALUSTRADE.Add (&bbs);

      }
   }

   pNew->IntegrityCheck();

   return pNew;
}

PWSTR gpszDisplayControl = L"DisplayControl";
static PWSTR gpszXMin = L"XMin";
static PWSTR gpszXMax = L"XMax";
static PWSTR gpszYMin = L"YMin";
static PWSTR gpszYMax = L"YMax";
static PWSTR gpszHeight = L"Height";
static PWSTR gpszRoofAngle1 = L"RoofAngle1";
static PWSTR gpszRoofAngle2 = L"RoofAngle2";
static PWSTR gpszRoofOverhang1 = L"RoofOverhang1";
static PWSTR gpszRoofOverhang2 = L"RoofOverhang2";
static PWSTR gpszRoofOverhang3 = L"RoofOverhang3";
static PWSTR gpszRoofOverhang4 = L"RoofOverhang4";
static PWSTR gpszRoofThickStruct = L"RoofThickStruct";
static PWSTR gpszRoofThickSideA = L"RoofThickSideA";
static PWSTR gpszRoofThickSideB = L"RoofThickSideB";
static PWSTR gpszWallThickStruct = L"WallThickStruct";
static PWSTR gpszWallThickSideA = L"WallThickSideA";
static PWSTR gpszWallThickSideB = L"WallThickSideB";
static PWSTR gpszFloorThickStruct = L"FloorThickStruct";
static PWSTR gpszFloorThickSideA = L"FloorThickSideA";
static PWSTR gpszFloorThickSideB = L"FloorThickSideB";
static PWSTR gpszBasementThickStruct = L"BasementThickStruct";
static PWSTR gpszBasementThickSideA = L"BasementThickSideA";
static PWSTR gpszBasementThickSideB = L"BasementThickSideB";
static PWSTR gpszPadThickStruct = L"PadThickStruct";
static PWSTR gpszPadThickSideA = L"PadThickSideA";
static PWSTR gpszPadThickSideB = L"PadThickSideB";
static PWSTR gpszCeilingThickStruct = L"CeilingThickStruct";
static PWSTR gpszCeilingThickSideA = L"CeilingThickSideA";
static PWSTR gpszCeilingThickSideB = L"CeilingThickSideB";

static PWSTR gpszBBSName = L"BBSName";
static PWSTR gpszBBSHidden = L"BBSHidden";
static PWSTR gpszBBSMajor = L"BBSMajor";
static PWSTR gpszBBSMinor = L"BBSMinor";
static PWSTR gpszBBSFlags = L"BBSFlags";
static PWSTR gpszBBSHeight = L"BBSHeight";
static PWSTR gpszBBSWidth = L"BBSWidth";
static PWSTR gpszBBSType = L"BBSType";
static PWSTR gpszBBSTrans = L"BBSTrans";
static PWSTR gpszBBSRotX = L"BBSRotX";
static PWSTR gpszBBSRotY = L"BBSRotY";
static PWSTR gpszBBSRotZ = L"BBSRotZ";
static PWSTR gpszFoundation = L"Foundation";
static PWSTR gpszFoundWall = L"FoundWall";
static PWSTR gpszPerimDepth = L"PerimDepth";
static PWSTR gpszCathedral = L"Cathedral";
static PWSTR gpszFloorsIntoRoof = L"FloorsIntoRoof";
static PWSTR gpszLevelGlobal = L"LevelGlobal";
static PWSTR gpszLevelElevation = L"LevelElevation%d";
static PWSTR gpszLevelHigher = L"LevelHigher";
static PWSTR gpszLevelMinDist = L"LevelMinDist";
static PWSTR gpszRoofType = L"RoofType";
static PWSTR gpszRoofControlFreedom = L"RoofControlFreedom";
static PWSTR gpszRoofControl = L"RoofControl%d";
static PWSTR gpszWallType = L"WallType";
static PWSTR gpszWallSpline = L"WallSpline";
static PWSTR gpszWallAngle = L"WallAngle%d";
static PWSTR gpszRestrictElevation = L"RestrictElevation";
static PWSTR gpszRestrictHeight = L"RestrictHeight";
static PWSTR gpszVerandahType = L"VerandahType";
static PWSTR gpszVerandahSpline = L"VerandahSpline";
static PWSTR gpszVerandahAngle = L"VerandahAngle%d";
static PWSTR gpszVerandahEveryFloor = L"VerandahEveryFloor";
static PWSTR gpszRoofHeight = L"RoofHeight";
static PWSTR gpszKeepWalls = L"KeepWalls";
static PWSTR gpszKeepFloor = L"KeepFloor";
static PWSTR gpszBalustrade = L"Balustrade";
static PWSTR gpszDontIncludeInVolume = L"DontIncludeInVolume";
static PWSTR gpszBalEveryFloor = L"BalEveryFloor";
static PWSTR gpszRoofHeightChange = L"RoofHeightChange";
static PWSTR gpszGroundFloor = L"GroundFloor";

PCMMLNode2 CObjectBuildBlock::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // member variables go here
   PCMMLNode2 pSub;
   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);

      pSub = pbbs->pSurface->MMLTo();
      if (!pSub)
         continue;

      pSub->NameSet (L"DoubleSurface");
      pNode->ContentAdd (pSub);

      MMLValueSet (pSub, gpszBBSName, pbbs->szName);
      MMLValueSet (pSub, gpszBBSHidden, (int) pbbs->fHidden);
      MMLValueSet (pSub, gpszBBSMajor, (int) pbbs->dwMajor);
      MMLValueSet (pSub, gpszBBSMinor, (int) pbbs->dwMinor);
      MMLValueSet (pSub, gpszBBSFlags, (int) pbbs->dwFlags);
      MMLValueSet (pSub, gpszBBSHeight, pbbs->fHeight);
      MMLValueSet (pSub, gpszBBSWidth, pbbs->fWidth);
      MMLValueSet (pSub, gpszBBSType, (int) pbbs->dwType);
      MMLValueSet (pSub, gpszBBSTrans, &pbbs->pTrans);
      MMLValueSet (pSub, gpszBBSRotX, pbbs->fRotX);
      MMLValueSet (pSub, gpszBBSRotY, pbbs->fRotY);
      MMLValueSet (pSub, gpszBBSRotZ, pbbs->fRotZ);
      MMLValueSet (pSub, gpszDontIncludeInVolume, (int)pbbs->fDontIncludeInVolume);
         //BUGFIX - So that intersect multistory building blocks works
      // dont need to save bevel
   }


   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      PBBBALUSTRADE pbbs = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(i);

      if (pbbs->pBal)
         pSub = pbbs->pBal->MMLTo();
      else if (pbbs->pPiers)
         pSub = pbbs->pPiers->MMLTo();
      if (!pSub)
         continue;

      pSub->NameSet (gpszBalustrade);
      pNode->ContentAdd (pSub);

      MMLValueSet (pSub, gpszBBSMajor, (int) pbbs->dwMajor);
      MMLValueSet (pSub, gpszBBSMinor, (int) pbbs->dwMinor);
   }


   MMLValueSet (pNode, gpszDisplayControl, (int) m_dwDisplayControl);
   MMLValueSet (pNode, gpszXMin, m_fXMin);
   MMLValueSet (pNode, gpszXMax, m_fXMax);
   MMLValueSet (pNode, gpszYMin, m_fYMin);
   MMLValueSet (pNode, gpszYMax, m_fYMax);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszRoofHeightChange, (int)m_dwRoofHeight);
   MMLValueSet (pNode, gpszRoofOverhang1, m_fRoofOverhang1);
   MMLValueSet (pNode, gpszRoofOverhang2, m_fRoofOverhang2);
   MMLValueSet (pNode, gpszRoofThickStruct, m_fRoofThickStruct);
   MMLValueSet (pNode, gpszRoofThickSideA, m_fRoofThickSideA);
   MMLValueSet (pNode, gpszRoofThickSideB, m_fRoofThickSideB);
   MMLValueSet (pNode, gpszWallThickStruct, m_fWallThickStruct);
   MMLValueSet (pNode, gpszWallThickSideA, m_fWallThickSideA);
   MMLValueSet (pNode, gpszWallThickSideB, m_fWallThickSideB);
   MMLValueSet (pNode, gpszFloorThickStruct, m_fFloorThickStruct);
   MMLValueSet (pNode, gpszFloorThickSideA, m_fFloorThickSideA);
   MMLValueSet (pNode, gpszFloorThickSideB, m_fFloorThickSideB);
   MMLValueSet (pNode, gpszBasementThickStruct, m_fBasementThickStruct);
   MMLValueSet (pNode, gpszBasementThickSideA, m_fBasementThickSideA);
   MMLValueSet (pNode, gpszBasementThickSideB, m_fBasementThickSideB);
   MMLValueSet (pNode, gpszPadThickStruct, m_fPadThickStruct);
   MMLValueSet (pNode, gpszPadThickSideA, m_fPadThickSideA);
   MMLValueSet (pNode, gpszPadThickSideB, m_fPadThickSideB);
   MMLValueSet (pNode, gpszCeilingThickStruct, m_fCeilingThickStruct);
   MMLValueSet (pNode, gpszCeilingThickSideA, m_fCeilingThickSideA);
   MMLValueSet (pNode, gpszCeilingThickSideB, m_fCeilingThickSideB);

   MMLValueSet (pNode, gpszFoundation, (int) m_dwFoundation);
   MMLValueSet (pNode, gpszFoundWall, (int) m_fFoundWall);
   MMLValueSet (pNode, gpszPerimDepth, m_fPerimDepth);

   MMLValueSet (pNode, gpszCathedral, (int) m_fCathedral);
   MMLValueSet (pNode, gpszFloorsIntoRoof, (int) m_fFloorsIntoRoof);

   MMLValueSet (pNode, gpszRestrictElevation, (int) m_fRestrictElevation);
   MMLValueSet (pNode, gpszRestrictHeight, (int) m_fRestrictHeight);
   MMLValueSet (pNode, gpszKeepFloor, (int) m_fKeepFloor);
   MMLValueSet (pNode, gpszKeepWalls, (int) m_fKeepWalls);
   MMLValueSet (pNode, gpszLevelGlobal, (int) m_fLevelGlobal);
   MMLValueSet (pNode, gpszLevelHigher, m_fLevelHigher);
   MMLValueSet (pNode, gpszLevelMinDist, m_fLevelMinDist);
   MMLValueSet (pNode, gpszGroundFloor, (int)m_dwGroundFloor);
   WCHAR szTemp[64];
   for (i = 0; i < NUMLEVELS; i++) {
      swprintf (szTemp, gpszLevelElevation, (int) i);
      MMLValueSet (pNode, szTemp, m_fLevelElevation[i]);
   }


   MMLValueSet (pNode, gpszRoofType, (int)m_dwRoofType);
   MMLValueSet (pNode, gpszRoofControlFreedom, (int) m_dwRoofControlFreedom);
   for (i = 0; i < m_lRoofControl.Num(); i++) {
      swprintf (szTemp, gpszRoofControl, (int) i);
      MMLValueSet (pNode, szTemp, (PCPoint) m_lRoofControl.Get(i));
   }

   // walls
   MMLValueSet (pNode, gpszWallType, (int)m_dwWallType);
   pSub = m_sWall.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszWallSpline);
      pNode->ContentAdd (pSub);
   }
   for (i = 0; i < m_lWallAngle.Num(); i++) {
      swprintf (szTemp, gpszWallAngle, (int) i);
      MMLValueSet (pNode, szTemp, *((fp*) m_lWallAngle.Get(i)));
   }

   // Verandahs
   MMLValueSet (pNode, gpszVerandahType, (int)m_dwVerandahType);
   MMLValueSet (pNode, gpszVerandahEveryFloor, (int)m_fVerandahEveryFloor);
   MMLValueSet (pNode, gpszBalEveryFloor, (int)m_dwBalEveryFloor);
   MMLValueSet (pNode, gpszRoofHeight, m_fRoofHeight);
   pSub = m_sVerandah.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszVerandahSpline);
      pNode->ContentAdd (pSub);
   }
   for (i = 0; i < m_lVerandahAngle.Num(); i++) {
      swprintf (szTemp, gpszVerandahAngle, (int) i);
      MMLValueSet (pNode, szTemp, *((fp*) m_lVerandahAngle.Get(i)));
   }
   return pNode;
}

BOOL CObjectBuildBlock::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;


   // delete all old walls and stuff surfaces
   // dont need to remove their Claims (ObjectSurfaceSet, etc.) because by calling
   // MMLFromTemplate() that's already been done
   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(i);
      if (ps->pSurface)
         delete ps->pSurface;
   }
   m_listBBSURF.Clear();

   // remove all the balustreads
   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      PBBBALUSTRADE ps = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(i);
      if (ps->pBal)
         delete ps->pBal;
      if (ps->pPiers)
         delete ps->pPiers;
   }
   m_lBBBALUSTRADE.Clear();

   // member variables go here
   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, L"DoubleSurface")) {
         PCDoubleSurface pds;
         pds = new CDoubleSurface;
         if (!pds)
            continue;

         // fill in the attached information
         BBSURF bbs;
         memset (&bbs, 0, sizeof(bbs));
         psz = MMLValueGet (pSub, gpszBBSName);
         if (psz)
            wcscpy (bbs.szName, psz);
         bbs.fHidden = (BOOL) MMLValueGetInt (pSub, gpszBBSHidden, 0);
         bbs.dwMajor = (DWORD) MMLValueGetInt (pSub, gpszBBSMajor, 0);
         bbs.dwMinor = (DWORD) MMLValueGetInt (pSub, gpszBBSMinor, 0);
         bbs.dwFlags = (DWORD) MMLValueGetInt (pSub, gpszBBSFlags, 0);
         bbs.fHeight = MMLValueGetDouble (pSub, gpszBBSHeight, 1);
         bbs.fWidth = MMLValueGetDouble (pSub, gpszBBSWidth, 1);
         bbs.dwType = (DWORD) MMLValueGetInt (pSub, gpszBBSType, 0);
         MMLValueGetPoint (pSub, gpszBBSTrans, &bbs.pTrans, &bbs.pTrans);
         bbs.fRotX = MMLValueGetDouble (pSub, gpszBBSRotX, 0);
         bbs.fRotZ = MMLValueGetDouble (pSub, gpszBBSRotZ, 0);
         bbs.fRotY = MMLValueGetDouble (pSub, gpszBBSRotY, 0);
         bbs.fDontIncludeInVolume = (int) MMLValueGetInt (pSub, gpszDontIncludeInVolume, FALSE);
         //BUGFIX - So that intersect multistory building blocks works
         // dont need to save bevel

         // Initializing the object, but then not creating any surfaces to register
         bbs.pSurface = pds;
         pds->InitButDontCreate (bbs.dwType, this);
         pds->MMLFrom (pSub);

         // add it
         m_listBBSURF.Add (&bbs);
      }
      else if (!_wcsicmp(psz, gpszBalustrade)) {
         // fill in the attached information
         BBBALUSTRADE bbs;
         memset (&bbs, 0, sizeof(bbs));
         bbs.dwMajor = (DWORD) MMLValueGetInt (pSub, gpszBBSMajor, 0);
         bbs.dwMinor = (DWORD) MMLValueGetInt (pSub, gpszBBSMinor, 0);
         bbs.fTouched = FALSE;

         if (bbs.dwMajor == BBBALMAJOR_BALUSTRADE) {
            PCBalustrade pds;
            pds = new CBalustrade;
            if (!pds)
               continue;
            bbs.pBal = pds;

            // Initializing the object, but then not creating any surfaces to register
            pds->InitButDontCreate (m_OSINFO.dwRenderShard, 0 /*dwType*/, this);
            pds->MMLFrom (pSub);
         }
         else if (bbs.dwMajor == BBBALMAJOR_PIERS) {
            PCPiers pds;
            pds = new CPiers;
            if (!pds)
               continue;
            bbs.pPiers = pds;

            // Initializing the object, but then not creating any surfaces to register
            pds->InitButDontCreate (m_OSINFO.dwRenderShard, 0 /*dwType*/, this);
            pds->MMLFrom (pSub);
         }

         // add it
         m_lBBBALUSTRADE.Add (&bbs);
      }
      else if (!_wcsicmp(psz, gpszWallSpline)) {
         m_sWall.MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszVerandahSpline)) {
         m_sVerandah.MMLFrom (pSub);
      }
   }

   m_dwDisplayControl = (DWORD) MMLValueGetInt (pNode, gpszDisplayControl, 0);
   m_fXMin = MMLValueGetDouble (pNode, gpszXMin, -1);
   m_fXMax = MMLValueGetDouble (pNode, gpszXMax, 1);
   m_fYMin = MMLValueGetDouble (pNode, gpszYMin, -1);
   m_fYMax = MMLValueGetDouble (pNode, gpszYMax, 1);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, 2);
   m_dwRoofHeight = (DWORD) MMLValueGetInt (pNode, gpszRoofHeightChange, 0);
   m_fRoofOverhang1 = MMLValueGetDouble (pNode, gpszRoofOverhang1, 0);
   m_fRoofOverhang2 = MMLValueGetDouble (pNode, gpszRoofOverhang2, 0);
   m_fRoofThickStruct = MMLValueGetDouble (pNode, gpszRoofThickStruct, 0.1);
   m_fRoofThickSideA = MMLValueGetDouble (pNode, gpszRoofThickSideA, 0.1);
   m_fRoofThickSideB = MMLValueGetDouble (pNode, gpszRoofThickSideB, 0.1);
   m_fWallThickStruct = MMLValueGetDouble (pNode, gpszWallThickStruct, 0.1);
   m_fWallThickSideA = MMLValueGetDouble (pNode, gpszWallThickSideA, 0.1);
   m_fWallThickSideB = MMLValueGetDouble (pNode, gpszWallThickSideB, 0.1);
   m_fFloorThickStruct = MMLValueGetDouble (pNode, gpszFloorThickStruct, 0.1);
   m_fFloorThickSideA = MMLValueGetDouble (pNode, gpszFloorThickSideA, 0.1);
   m_fFloorThickSideB = MMLValueGetDouble (pNode, gpszFloorThickSideB, 0.1);
   m_fBasementThickStruct = MMLValueGetDouble (pNode, gpszBasementThickStruct, 0.1);
   m_fBasementThickSideA = MMLValueGetDouble (pNode, gpszBasementThickSideA, 0.1);
   m_fBasementThickSideB = MMLValueGetDouble (pNode, gpszBasementThickSideB, 0.1);
   m_fPadThickStruct = MMLValueGetDouble (pNode, gpszPadThickStruct, 0.1);
   m_fPadThickSideA = MMLValueGetDouble (pNode, gpszPadThickSideA, 0.1);
   m_fPadThickSideB = MMLValueGetDouble (pNode, gpszPadThickSideB, 0.1);
   m_fCeilingThickStruct = MMLValueGetDouble (pNode, gpszCeilingThickStruct, 0.1);
   m_fCeilingThickSideA = MMLValueGetDouble (pNode, gpszCeilingThickSideA, 0.1);
   m_fCeilingThickSideB = MMLValueGetDouble (pNode, gpszCeilingThickSideB, 0.1);

   m_dwFoundation = (DWORD) MMLValueGetInt (pNode, gpszFoundation, 0);
   m_fFoundWall = (BOOL) MMLValueGetInt (pNode, gpszFoundWall, 0);
   m_fPerimDepth = MMLValueGetDouble (pNode, gpszPerimDepth, -1);
   m_fCathedral = (BOOL) MMLValueGetInt (pNode, gpszCathedral, 0);
   m_fFloorsIntoRoof = (BOOL) MMLValueGetInt (pNode, gpszFloorsIntoRoof, 0);

   m_fRestrictElevation = (BOOL) MMLValueGetInt (pNode, gpszRestrictElevation, TRUE);
   m_fRestrictHeight = (BOOL) MMLValueGetInt (pNode, gpszRestrictHeight, TRUE);
   m_fKeepFloor = (BOOL) MMLValueGetInt (pNode, gpszKeepFloor, TRUE);
   m_fKeepWalls = (BOOL) MMLValueGetInt (pNode, gpszKeepWalls, TRUE);
   m_fLevelGlobal = (DWORD) MMLValueGetInt (pNode, gpszLevelGlobal, TRUE);
   m_fLevelHigher = MMLValueGetDouble (pNode, gpszLevelHigher, FLOORHEIGHT_TROPICAL);
   m_fLevelMinDist = MMLValueGetDouble (pNode, gpszLevelMinDist, 2);
   m_dwGroundFloor = (DWORD) MMLValueGetInt (pNode, gpszGroundFloor, 1);
   WCHAR szTemp[64];
   for (i = 0; i < NUMLEVELS; i++) {
      swprintf (szTemp, gpszLevelElevation, (int) i);
      m_fLevelElevation[i] = MMLValueGetDouble (pNode, szTemp,
         (i ? m_fLevelElevation[i-1] : 0) + m_fLevelMinDist);
   }

   m_dwRoofType = (DWORD) MMLValueGetInt (pNode, gpszRoofType, 0);
   m_dwRoofControlFreedom = (DWORD) MMLValueGetInt (pNode, gpszRoofControlFreedom, 0);
   m_lRoofControl.Clear();
   for (i = 0; ; i++) {
      CPoint p, zero;
      zero.Zero();
      swprintf (szTemp, gpszRoofControl, (int) i);
      if (!MMLValueGetPoint (pNode, szTemp, &p, &zero))
         break;
      m_lRoofControl.Add (&p);
   }

   // wall
   m_dwWallType = (DWORD) MMLValueGetInt (pNode, gpszWallType, 0);
   m_lWallAngle.Clear();
   for (i = 0; ; i++) {
      fp f;
      swprintf (szTemp, gpszWallAngle, (int) i);
      f = MMLValueGetDouble (pNode, szTemp, -1000);
      if (f == -1000)
         break;
      m_lWallAngle.Add (&f);
   }

   // Verandah
   m_dwVerandahType = (DWORD) MMLValueGetInt (pNode, gpszVerandahType, 0);
   m_fVerandahEveryFloor = (BOOL) MMLValueGetInt (pNode, gpszVerandahEveryFloor, 0);
   m_dwBalEveryFloor = (DWORD) MMLValueGetInt (pNode, gpszBalEveryFloor, 0);
   m_fRoofHeight = MMLValueGetDouble (pNode, gpszRoofHeight, m_fRoofHeight);
   m_lVerandahAngle.Clear();
   for (i = 0; ; i++) {
      fp f;
      swprintf (szTemp, gpszVerandahAngle, (int) i);
      f = MMLValueGetDouble (pNode, szTemp, -1000);
      if (f == -1000)
         break;
      m_lVerandahAngle.Add (&f);
   }

   // keep the old values the same as the new ones so dont
   // try to stretch contained objects
   m_fOldXMin = 0;
   m_fOldXMax = 0;
   m_fOldYMin = 0;
   m_fOldYMax = 0;
   m_fOldHeight = 0;

   IntegrityCheck();

   return TRUE;
}


/*************************************************************************************
CObjectBuildBlock::IntegrityCheck - Just a check to make sure there aren't extraneous
surfaces around.
*/
void CObjectBuildBlock::IntegrityCheck (void)
{
#ifdef _DEBUG
   // jsut to be paranoid, go through all the claims and make sure they have
   // an accompanying surface
   DWORD dw, j, i;
   PBBSURF pbbs;
   for (i = 0; ;i++) {
      dw = ObjectSurfaceGetIndex(i);
      if (dw == (DWORD)-1)
         break;

      // find in surfaces
      for (j = 0; j < m_listBBSURF.Num(); j++) {
         pbbs = (PBBSURF) m_listBBSURF.Get(j);
         if (pbbs->pSurface->ClaimFindByID (dw))
            break;
      }
      if (j < m_listBBSURF.Num())
         continue;

      // find in balustradesw
      for (j = 0; j < m_lBBBALUSTRADE.Num(); j++) {
         PBBBALUSTRADE ps = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(j);
         if (ps->pBal && ps->pBal->ClaimFindByID (dw))
            break;
         if (ps->pPiers && ps->pPiers->ClaimFindByID (dw))
            break;
      }
      if (j < m_lBBBALUSTRADE.Num())
         continue;

      OutputDebugString ("ERROR: Unclaimed surface found.\r\n");
   };
   for (i = 0; ;i++) {
      dw = ContainerSurfaceGetIndex(i);
      if (dw == (DWORD)-1)
         break;

      for (j = 0; j < m_listBBSURF.Num(); j++) {
         pbbs = (PBBSURF) m_listBBSURF.Get(j);
         if (pbbs->pSurface->ClaimFindByID (dw))
            break;
      }
      if (j <= m_listBBSURF.Num())
         continue;

      // find in balustradesw
      for (j = 0; j < m_lBBBALUSTRADE.Num(); j++) {
         PBBBALUSTRADE ps = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(j);
         // Only need to test piers because that only has the fp surface
         //if (ps->pBal && ps->pBal->ClaimFindByID (dw))
         //   break;
         if (ps->pPiers && ps->pPiers->ClaimFindByID (dw))
            break;
      }
      if (j < m_lBBBALUSTRADE.Num())
         continue;

      OutputDebugString ("ERROR: Unclaimed constainer surface found.\r\n");
   };
#endif

}
/*************************************************************************************
CObjectBuildBlock::ControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBuildBlock::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = .3;
   DWORD i;

   if (m_dwDisplayControl == 0) {
      if (dwID >= 12)
         return FALSE;

      DWORD x,y,z;
      x = dwID & 0x01;
      y = (dwID >> 1) & 0x01;
      z = (dwID >> 2);// & 0x01;

      memset (pInfo,0, sizeof(*pInfo));

      pInfo->dwID = dwID;
//      pInfo->dwFreedom = 0;   // any direction
      if (!z) {
         // only freedom on plane in z direction
         // BUGFIX - Since freedom != 0 causes problems, use freedom=0: pInfo->dwFreedom = 1;
//         pInfo->pV1.p[0] = 1;
//         pInfo->pV2.p[1] = 1;
      }
      pInfo->dwStyle = CPSTYLE_CUBE;
      pInfo->fSize = fKnobSize;
      pInfo->cColor = RGB(0xff,0,0xff);
      wcscpy (pInfo->szName, L"Size");

      // show width and height
      WCHAR szWidth[32], szDepth[32], szHeight[32];
      MeasureToString (m_fXMax - m_fXMin, szWidth);
      MeasureToString (m_fYMax - m_fYMin, szDepth);
      MeasureToString (m_fHeight, szHeight);
      swprintf (pInfo->szMeasurement, L"%s x %s x %s", szWidth, szDepth, szHeight);

      pInfo->pLocation.p[0] = x ? m_fXMax : m_fXMin;
      pInfo->pLocation.p[1] = y ? m_fYMax : m_fYMin;
      pInfo->pLocation.p[2] = z ? m_fHeight : 0;
      if (z == 2)
         pInfo->pLocation.p[2] += m_fLevelHigher;

      return TRUE;
   }
   else if (m_dwDisplayControl == 1) {//roof
      return RoofControlPointQuery (dwID, pInfo);
   }
   else if (m_dwDisplayControl == 2) { //wall
      return WallControlPointQuery (dwID, pInfo, TRUE);
   }
   else if (m_dwDisplayControl == 3) { //verandah
      return WallControlPointQuery (dwID, pInfo, FALSE);
   }
   else if (m_dwDisplayControl >= DISPLAYBASE_BALUSTRADE) {
      PBBBALUSTRADE pBal;
      pBal = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (m_dwDisplayControl - DISPLAYBASE_BALUSTRADE);
      if (!pBal || !(pBal->pBal || pBal->pPiers))
         return FALSE;

      if (pBal->pBal)
         return pBal->pBal->ControlPointQuery (dwID, pInfo);
      else
         return pBal->pPiers->ControlPointQuery (dwID, pInfo);
   }
   else {   // see if it's one of the overlays
      for (i = 0; i < m_listBBSURF.Num(); i++) {
         PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);
         if (pbbs->pSurface->ControlPointQuery (m_dwDisplayControl, dwID, pInfo))
            return TRUE;
      }
   }

   return FALSE;
}

/*************************************************************************************
CObjectBuildBlock::ControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBuildBlock::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   DWORD i;

   if (m_dwDisplayControl == 0) {
      if (dwID >= 12)
         return FALSE;

      DWORD x,y,z;
      x = dwID & 0x01;
      y = (dwID >> 1) & 0x01;
      z = (dwID >> 2);// & 0x01;

      // tell the world we're about to change
      if (m_pWorld)
         m_pWorld->ObjectAboutToChange (this);

      CPoint p;
      p.Copy (pVal);

      // can't change level of p0
      if (z) {
         m_fHeight = p.p[2];
         if (z == 2)
            m_fHeight -= m_fLevelHigher;
         m_fHeight = max(m_fHeight, .1);   // can't  make too short
      }
   
      // adjust the height
      AdjustHeightToFloor ();

      if (x)
         m_fXMax = max(p.p[0], m_fXMin+.1);
      else
         m_fXMin = min(p.p[0], m_fXMax-.1);

      if (y)
         m_fYMax = max(p.p[1], m_fYMin+.1);
      else
         m_fYMin = min(p.p[1], m_fYMax-.1);

      // need to store old point so that when move windows/embedded
      // objects will stay in appropriate place

      // recalc
      AdjustAllSurfaces ();

      // tell the world we've changed
      if (m_pWorld)
         m_pWorld->ObjectChanged (this);

      return TRUE;
   }
   else if (m_dwDisplayControl == 1) {
      return RoofControlPointSet (dwID, pVal, pViewer);
   }
   else if (m_dwDisplayControl == 2) {
      return WallControlPointSet (dwID, pVal, pViewer, TRUE);
   }
   else if (m_dwDisplayControl == 3) {
      return WallControlPointSet (dwID, pVal, pViewer, FALSE);
   }
   else if (m_dwDisplayControl >= DISPLAYBASE_BALUSTRADE) {
      PBBBALUSTRADE pBal;
      pBal = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (m_dwDisplayControl - DISPLAYBASE_BALUSTRADE);
      if (!pBal || !(pBal->pBal || pBal->pPiers))
         return FALSE;

      if (pBal->pBal)
         return pBal->pBal->ControlPointSet (dwID, pVal, pViewer, this);
      else
         return pBal->pPiers->ControlPointSet (m_OSINFO.dwRenderShard, dwID, pVal, pViewer, this);
   }
   else {   // see if it's one of the overlays
      for (i = 0; i < m_listBBSURF.Num(); i++) {
         PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);
         if (pbbs->pSurface->ControlPointSet (m_dwDisplayControl, dwID, pVal, pViewer))
            return TRUE;
      }
   }

   return FALSE;
}

/*************************************************************************************
CObjectBuildBlock::ControlPointEnum - Called to enumerate a list of control point IDs
into the list.

inputs
   PCListFixed       plDWORD - Should be filled in with control points
returns
   none
*/
void CObjectBuildBlock::ControlPointEnum (PCListFixed plDWORD)
{
   // 6 control points starting at 10
   DWORD i;

   if (m_dwDisplayControl == 0) {
      DWORD dwNum = 12;  // 8 control points
      plDWORD->Required (plDWORD->Num() + dwNum);
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 1) {
      DWORD dwNum = m_lRoofControl.Num();
      plDWORD->Required (plDWORD->Num() + dwNum);
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 2) {
      DWORD dwNum = (m_dwWallType == WALLSHAPE_NONE) ? 0 : m_sWall.OrigNumPointsGet();
      plDWORD->Required (plDWORD->Num() + dwNum);
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl == 3) {
      DWORD dwNum = (m_dwVerandahType == WALLSHAPE_NONE) ? 0 : m_sVerandah.OrigNumPointsGet();
      plDWORD->Required (plDWORD->Num() + dwNum);
      for (i = 0; i < dwNum; i++)
         plDWORD->Add (&i);
   }
   else if (m_dwDisplayControl >= DISPLAYBASE_BALUSTRADE) {
      PBBBALUSTRADE pBal;
      pBal = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (m_dwDisplayControl - DISPLAYBASE_BALUSTRADE);
      if (!pBal || !(pBal->pBal || pBal->pPiers))
         return;

      if (pBal->pBal)
         pBal->pBal->ControlPointEnum (plDWORD);
      else
         pBal->pPiers->ControlPointEnum (plDWORD);
   }
   else {   // see if it's one of the overlays
      for (i = 0; i < m_listBBSURF.Num(); i++) {
         PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);
         pbbs->pSurface->ControlPointEnum (m_dwDisplayControl, plDWORD);
      }
   }
}

/**********************************************************************************
CObjectBuildBlock::DialogQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectBuildBlock::DialogQuery (void)
{
   return TRUE;
}


/**********************************************************************************
CObjectBuildBlock::DialogCPQuery - Returns TRUE if the object supports a custom
dialog box, FALSE if it doesn't. The default object doesn't
*/
BOOL CObjectBuildBlock::DialogCPQuery (void)
{
   return TRUE;
}

/* BuildBlockDialogPage
*/
BOOL BuildBlockDialogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"overside");
         if (pControl && (!pv->pSurf || !pv->iSide))
            pControl->Enable (FALSE);

         pControl = pPage->ControlFind (L"framing");
         if (pControl && !pv->pSurf)
            pControl->Enable (FALSE);

         pControl = pPage->ControlFind (L"balappear");
         if (pControl && !(pv->pBal && pv->pBal->pBal))
            pControl->Enable (FALSE);
         pControl = pPage->ControlFind (L"balopenings");
         if (pControl && !(pv->pBal && pv->pBal->pBal))
            pControl->Enable (FALSE);

         pControl = pPage->ControlFind (L"piersappear");
         if (pControl && !(pv->pBal && pv->pBal->pPiers))
            pControl->Enable (FALSE);
         pControl = pPage->ControlFind (L"piersopenings");
         if (pControl && !(pv->pBal && pv->pBal->pPiers))
            pControl->Enable (FALSE);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Building block settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* BuildBlockRoofPage
*/
BOOL BuildBlockRoofPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"overhang1", pv->pThis->m_fRoofOverhang1);
         MeasureToString (pPage, L"overhang2", pv->pThis->m_fRoofOverhang2);

         MeasureToString (pPage, L"stud", pv->pThis->m_fRoofThickStruct);
         MeasureToString (pPage, L"sidea", pv->pThis->m_fRoofThickSideA);
         MeasureToString (pPage, L"sideb", pv->pThis->m_fRoofThickSideB);

         WCHAR szTemp[32];
         swprintf (szTemp, L"%d", (int) pv->pThis->m_dwRoofType);
         ESCMLISTBOXSELECTSTRING lss;
         memset (&lss, 0, sizeof(lss));
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"roofshape");
         if (pControl)
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &lss);

         switch (pv->pThis->m_dwRoofHeight) {
         default:
         case 0:  // combination
            pControl = pPage->ControlFind (L"rhcombo");
            break;
         case 1:  // width
            pControl = pPage->ControlFind (L"rh1");
            break;
         case 2:  // length
            pControl = pPage->ControlFind (L"rh2");
            break;
         case 3:  // constant
            pControl = pPage->ControlFind (L"rhconst");
            break;
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"rhcombo") ||
            !_wcsicmp(p->pControl->m_pszName, L"rh1") ||
            !_wcsicmp(p->pControl->m_pszName, L"rh2") ||
            !_wcsicmp(p->pControl->m_pszName, L"rhconst")) {

            // what's the new setting
            DWORD dwNew;
            if (!_wcsicmp(p->pControl->m_pszName, L"rhcombo"))
               dwNew = 0;
            else if (!_wcsicmp(p->pControl->m_pszName, L"rh1"))
               dwNew = 1;
            else if (!_wcsicmp(p->pControl->m_pszName, L"rh2"))
               dwNew = 2;
            else
               dwNew = 3;
            if (dwNew == pv->pThis->m_dwRoofHeight)
               return TRUE;   // no change

            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->ConvertRoofPoints (pv->pThis->m_dwRoofHeight, dwNew);
            pv->pThis->m_dwRoofHeight = dwNew;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // nothing to adjust since effective height is still the same
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"roofshape")) {
            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == pv->pThis->m_dwRoofType)
               return TRUE;   // no change

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);

            pv->pThis->m_dwRoofType = dwVal;
            pv->pThis->RoofDefaultControl (dwVal);
            pv->pSurf = NULL; // since may no longer exist

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);
            return TRUE;
         }
      }
      break;
   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszAngle = L"angle";
         PWSTR pszOverhang = L"overhang";
         int iLenAngle = (DWORD)wcslen(pszAngle);
         int iLenOverhang = (DWORD)wcslen(pszOverhang);
         fp   *pf;
         if (!_wcsicmp (p->pControl->m_pszName, L"stud") ||
            !_wcsicmp (p->pControl->m_pszName, L"sidea") ||
            !_wcsicmp (p->pControl->m_pszName, L"sideb")) {

            if (!_wcsicmp (p->pControl->m_pszName, L"stud"))
               pf = &pv->pThis->m_fRoofThickStruct;
            else if (!_wcsicmp (p->pControl->m_pszName, L"sidea"))
               pf = &pv->pThis->m_fRoofThickSideA;
            else
               pf = &pv->pThis->m_fRoofThickSideB;

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
           
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = .01;
            *pf = max(0, *pf);
            if (pf == &pv->pThis->m_fRoofThickStruct)
               *pf = max(.01, *pf);

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsnicmp(p->pControl->m_pszName, pszOverhang, iLenOverhang)) {
            switch (p->pControl->m_pszName[iLenOverhang] - '0') {
            case 1:
               pf = &pv->pThis->m_fRoofOverhang1;
               break;
            case 2:
               pf = &pv->pThis->m_fRoofOverhang2;
               break;
            default:
               break;
            }

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = .5;
            *pf = max(0,*pf);
            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         };

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Roof";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* BuildBlockWallAnglePage
*/
BOOL BuildBlockWallAnglePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   fp *pf = (fp*)pPage->m_pUserData;
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         AngleToControl (pPage, L"angle", *pf, TRUE);
      }
      break;

   case ESCM_LINK:
      {
         // get the angle
         *pf = AngleFromControl (pPage, L"angle");
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* BuildBlockWallsPage
*/
BOOL BuildBlockWallsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;
   DWORD *pdwType = pv->fWall ? &pv->pThis->m_dwWallType : &pv->pThis->m_dwVerandahType;
   PCSpline plWallVerandah = pv->fWall ? &pv->pThis->m_sWall : &pv->pThis->m_sVerandah;
   PCListFixed plAngle = pv->fWall ? &pv->pThis->m_lWallAngle : &pv->pThis->m_lVerandahAngle;


   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"stud", pv->pThis->m_fWallThickStruct);
         MeasureToString (pPage, L"sidea", pv->pThis->m_fWallThickSideA);
         MeasureToString (pPage, L"sideb", pv->pThis->m_fWallThickSideB);

         WCHAR szTemp[32];
         swprintf (szTemp, L"%d", (int) (*pdwType));
         ESCMLISTBOXSELECTSTRING lss;
         memset (&lss, 0, sizeof(lss));
         lss.fExact = TRUE;
         lss.iStart = -1;
         lss.psz = szTemp;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"wallshape");
         if (pControl)
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &lss);

         if (*pdwType != WALLSHAPE_ANY) {
            pControl = pPage->ControlFind (L"anyshape");
            if (pControl)
               pControl->Enable(FALSE);
         }

         pControl = pPage->ControlFind (L"everyfloor");
         if (pControl && pv->pThis->m_fVerandahEveryFloor)
            pControl->AttribSetBOOL (Checked(), TRUE);

         switch (pv->pThis->m_dwBalEveryFloor) {
         default:
         case 0:
            pControl = pPage->ControlFind (L"balall");
            break;
         case 1:
            pControl = pPage->ControlFind (L"balabove");
            break;
         case 2:
            pControl = pPage->ControlFind (L"balnone");
            break;
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         GenerateThreeDFromSpline (L"wallangle", pPage, plWallVerandah, pv->pssAny, 0);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"everyfloor")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fVerandahEveryFloor = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"balall") ||
            !_wcsicmp(p->pControl->m_pszName, L"balabove") ||
            !_wcsicmp(p->pControl->m_pszName, L"balnone")) {

            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            if (!_wcsicmp(p->pControl->m_pszName, L"balall"))
               pv->pThis->m_dwBalEveryFloor = 0;
            else if (!_wcsicmp(p->pControl->m_pszName, L"balabove"))
               pv->pThis->m_dwBalEveryFloor = 1;
            else
               pv->pThis->m_dwBalEveryFloor = 2;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (!_wcsicmp(p->pControl->m_pszName, L"wallshape")) {
            DWORD dwVal;
            dwVal = _wtoi(p->pszName);
            if (dwVal == *pdwType)
               return TRUE;   // no change

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);

            if ((*pdwType != WALLSHAPE_NONE) && (dwVal == WALLSHAPE_ANY)) {
               // just remember the new wall type and keep the old points
               *pdwType = dwVal;
            }
            else {
               *pdwType = dwVal;
               pv->pThis->WallDefaultSpline (dwVal, pv->fWall);
            }
            pv->pSurf = NULL; // since may no longer exist

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // enable/disable any shape
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"anyshape");
            if (pControl)
               pControl->Enable(*pdwType == WALLSHAPE_ANY);

            GenerateThreeDFromSpline (L"wallangle", pPage, plWallVerandah, pv->pssAny, 0);
            return TRUE;
         }
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (_wcsicmp(p->pControl->m_pszName, L"wallangle"))
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if (dwMode != 1)
            return TRUE;

         fp f;
         f = *((fp*) plAngle->Get(x));

         // get the angle
         CEscWindow cWindow;
         RECT r;
         DialogBoxLocation2 (pPage->m_pWindow->m_hWnd, &r);
         cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
         PWSTR pszRet;
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLBUILDBLOCKWALLANGLE, BuildBlockWallAnglePage, &f);

         if (_wcsicmp(pszRet, L"ok"))
            return TRUE;

         // pressed OK, so set it
         pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
         plAngle->Set(x, &f);
         pv->pThis->AdjustAllSurfaces();
         pv->pThis->m_pWorld->ObjectChanged (pv->pThis);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         fp   *pf;
         if (!_wcsicmp (p->pControl->m_pszName, L"stud") ||
            !_wcsicmp (p->pControl->m_pszName, L"sidea") ||
            !_wcsicmp (p->pControl->m_pszName, L"sideb")) {

            if (!_wcsicmp (p->pControl->m_pszName, L"stud"))
               pf = &pv->pThis->m_fWallThickStruct;
            else if (!_wcsicmp (p->pControl->m_pszName, L"sidea"))
               pf = &pv->pThis->m_fWallThickSideA;
            else
               pf = &pv->pThis->m_fWallThickSideB;

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
           
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = .01;
            *pf = max(0, *pf);
            if (pf == &pv->pThis->m_fWallThickStruct)
               *pf = max(.01, *pf);

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = pv->fWall ? L"Walls" : L"Verandahs";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* BuildBlockIntelPage
*/
BOOL BuildBlockIntelPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;


   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"restrict");
         if (pControl && pv->pThis->m_fRestrictElevation)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"restrictheight");
         if (pControl && pv->pThis->m_fRestrictHeight)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"keepfloor");
         if (pControl && pv->pThis->m_fKeepFloor)
            pControl->AttribSetBOOL (Checked(), TRUE);
         pControl = pPage->ControlFind (L"keepwalls");
         if (pControl && pv->pThis->m_fKeepWalls)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"restrict")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fRestrictElevation = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            if (pv->pThis->m_fRestrictElevation) {
               // May realign vertically so ground floor is ok
               pv->pThis->VerifyObjectMatrix();

               // make sure floor ok
               pv->pThis->AdjustHeightToFloor ();

               pv->pThis->AdjustAllSurfaces();
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"restrictheight")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fRestrictHeight = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            if (pv->pThis->m_fRestrictElevation) {
               // May realign vertically so ground floor is ok
               pv->pThis->VerifyObjectMatrix();

               // make sure floor ok
               pv->pThis->AdjustHeightToFloor ();

               pv->pThis->AdjustAllSurfaces();
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"keepfloor")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fKeepFloor = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"keepwalls")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fKeepWalls = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Intelligent adjust";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* BuildBlockFoundationPage
*/
BOOL BuildBlockFoundationPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"stud", pv->pThis->m_fBasementThickStruct);
         MeasureToString (pPage, L"sidea", pv->pThis->m_fBasementThickSideA);
         MeasureToString (pPage, L"sideb", pv->pThis->m_fBasementThickSideB);

         MeasureToString (pPage, L"perimdepth", pv->pThis->m_fPerimDepth);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"foundwall");
         if (pControl && pv->pThis->m_fFoundWall)
            pControl->AttribSetBOOL (Checked(), TRUE);

         PWSTR psz;
         switch (pv->pThis->m_dwFoundation) {
         case 1:
            psz = L"piers";
            break;
         case 2:
            psz = L"pad";
            break;
         case 3:
            psz = L"perimiter";
            break;
         case 4:
            psz = L"basement";
            break;
         case 0:
         default:
            psz = L"none";
            break;
         }
         pControl = pPage->ControlFind (psz);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"foundwall")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fFoundWall = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"none")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_dwFoundation = 0;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"piers")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_dwFoundation = 1;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"pad")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_dwFoundation = 2;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"perimiter")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_dwFoundation = 3;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"basement")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_dwFoundation = 4;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         fp   *pf;
         if (!_wcsicmp (p->pControl->m_pszName, L"stud") ||
            !_wcsicmp (p->pControl->m_pszName, L"sidea") ||
            !_wcsicmp (p->pControl->m_pszName, L"sideb")) {

            if (!_wcsicmp (p->pControl->m_pszName, L"stud"))
               pf = &pv->pThis->m_fBasementThickStruct;
            else if (!_wcsicmp (p->pControl->m_pszName, L"sidea"))
               pf = &pv->pThis->m_fBasementThickSideA;
            else
               pf = &pv->pThis->m_fBasementThickSideB;

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
           
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = .01;
            *pf = max(0, *pf);
            if (pf == &pv->pThis->m_fBasementThickStruct)
               *pf = max(.01, *pf);

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp (p->pControl->m_pszName, L"perimdepth")) {

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
           
            pf = &pv->pThis->m_fPerimDepth;
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = -1;

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Foundation";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/* BuildBlockFloorsPage
*/
BOOL BuildBlockFloorsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"stud", pv->pThis->m_fFloorThickStruct);
         MeasureToString (pPage, L"sidea", pv->pThis->m_fFloorThickSideA);
         MeasureToString (pPage, L"sideb", pv->pThis->m_fFloorThickSideB);

         MeasureToString (pPage, L"padstud", pv->pThis->m_fPadThickStruct);
         MeasureToString (pPage, L"padsidea", pv->pThis->m_fPadThickSideA);
         MeasureToString (pPage, L"padsideb", pv->pThis->m_fPadThickSideB);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"intoroof");
         if (pControl && pv->pThis->m_fFloorsIntoRoof)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"intoroof")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fFloorsIntoRoof = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;
   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         fp   *pf;
         if (!_wcsicmp (p->pControl->m_pszName, L"stud") ||
            !_wcsicmp (p->pControl->m_pszName, L"sidea") ||
            !_wcsicmp (p->pControl->m_pszName, L"sideb")) {

            if (!_wcsicmp (p->pControl->m_pszName, L"stud"))
               pf = &pv->pThis->m_fFloorThickStruct;
            else if (!_wcsicmp (p->pControl->m_pszName, L"sidea"))
               pf = &pv->pThis->m_fFloorThickSideA;
            else
               pf = &pv->pThis->m_fFloorThickSideB;

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
           
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = .01;
            *pf = max(0, *pf);
            if (pf == &pv->pThis->m_fFloorThickStruct)
               *pf = max(.01, *pf);

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
         else if (!_wcsicmp (p->pControl->m_pszName, L"padstud") ||
            !_wcsicmp (p->pControl->m_pszName, L"padsidea") ||
            !_wcsicmp (p->pControl->m_pszName, L"padsideb")) {

            if (!_wcsicmp (p->pControl->m_pszName, L"padstud"))
               pf = &pv->pThis->m_fPadThickStruct;
            else if (!_wcsicmp (p->pControl->m_pszName, L"padsidea"))
               pf = &pv->pThis->m_fPadThickSideA;
            else
               pf = &pv->pThis->m_fPadThickSideB;

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
           
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = .01;
            *pf = max(0, *pf);
            if (pf == &pv->pThis->m_fPadThickStruct)
               *pf = max(.01, *pf);

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Floors";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/* BuildBlockLevelsPage
*/
BOOL BuildBlockLevelsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < NUMLEVELS; i++) {
            swprintf (szTemp, L"level%d", (int) i);
            MeasureToString (pPage, szTemp, pv->pThis->m_fLevelElevation[i]);
         }

         MeasureToString (pPage, L"upperlevels", pv->pThis->m_fLevelHigher);
         MeasureToString (pPage, L"ceilmindist", pv->pThis->m_fLevelMinDist);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"useglobal");
         if (pControl && pv->pThis->m_fLevelGlobal)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"commit")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);

            // get the values
            DWORD i;
            WCHAR szTemp[64];
            for (i = 0; i < NUMLEVELS; i++) {
               swprintf (szTemp, L"level%d", (int) i);
               MeasureParseString (pPage, szTemp, &pv->pThis->m_fLevelElevation[i]);
            }

            BOOL fOldUseGlobal;
            fOldUseGlobal = pv->pThis->m_fLevelGlobal;

            MeasureParseString (pPage, L"upperlevels", &pv->pThis->m_fLevelHigher);
            pv->pThis->m_fLevelHigher = max(pv->pThis->m_fLevelHigher, .1);
            MeasureParseString (pPage, L"ceilmindist", &pv->pThis->m_fLevelMinDist);

            PCEscControl pControl;
            pControl = pPage->ControlFind (L"useglobal");
            if (pControl)
               pv->pThis->m_fLevelGlobal = pControl->AttribGetBOOL (Checked());

            // If changed from local floors to global floors, ask if want
            // to use global floors as they currently are, or set all global floors
            // to the heights in the dialog
            if (pv->pThis->m_fLevelGlobal && (fOldUseGlobal != pv->pThis->m_fLevelGlobal)) {
               int iRet;
               iRet = pPage->MBYesNo (L"Do you want all building blocks using the global "
                  L"floor setting to change to the elevations you entered?",
                  L"If you press No any elevations already used in this building block will "
                  L"be ignored and instead replaced by the current global elevations. If you "
                  L"press Yes the object will keep the current elevations and all building blocks "
                  L"will change to use these elevations.");
               if (iRet == IDYES) {
                  GlobalFloorLevelsSet (pv->pThis->m_pWorld, pv->pThis,
                     pv->pThis->m_fLevelElevation, pv->pThis->m_fLevelHigher);
               }
               else {
                  GlobalFloorLevelsGet (pv->pThis->m_pWorld, pv->pThis,
                     pv->pThis->m_fLevelElevation, &pv->pThis->m_fLevelHigher);
               }
            }
            else if (pv->pThis->m_fLevelGlobal)
               GlobalFloorLevelsSet (pv->pThis->m_pWorld, pv->pThis,
                  pv->pThis->m_fLevelElevation, pv->pThis->m_fLevelHigher);

            // FUTURERELEASE -  may need to move pieces (windows, doors, etc.) to deal
            // with floors moving up and down

            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);


            // May realign vertically so ground floor is ok
            pv->pThis->VerifyObjectMatrix();

            // make sure floor ok
            pv->pThis->AdjustHeightToFloor ();

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Levels";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/* BuildBlockCeilingPage
*/
BOOL BuildBlockCeilingPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"stud", pv->pThis->m_fCeilingThickStruct);
         MeasureToString (pPage, L"sidea", pv->pThis->m_fCeilingThickSideA);
         MeasureToString (pPage, L"sideb", pv->pThis->m_fCeilingThickSideB);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"cathedral");
         if (pControl && pv->pThis->m_fCathedral)
            pControl->AttribSetBOOL (Checked(), TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"cathedral")) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_fCathedral = p->pControl->AttribGetBOOL (Checked());
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         fp   *pf;
         if (!_wcsicmp (p->pControl->m_pszName, L"stud") ||
            !_wcsicmp (p->pControl->m_pszName, L"sidea") ||
            !_wcsicmp (p->pControl->m_pszName, L"sideb")) {

            if (!_wcsicmp (p->pControl->m_pszName, L"stud"))
               pf = &pv->pThis->m_fCeilingThickStruct;
            else if (!_wcsicmp (p->pControl->m_pszName, L"sidea"))
               pf = &pv->pThis->m_fCeilingThickSideA;
            else
               pf = &pv->pThis->m_fCeilingThickSideB;

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
           
            if (!MeasureParseString (pPage, p->pControl->m_pszName, pf))
               *pf = .01;
            *pf = max(0, *pf);
            if (pf == &pv->pThis->m_fCeilingThickStruct)
               *pf = max(.01, *pf);

            if (pv->pThis->m_pWorld)
               pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // adjust the surfaces to the new setting
            pv->pThis->AdjustAllSurfaces();
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Ceiling";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* BuildBlockDisplayPage
*/
BOOL BuildBlockDisplayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set thecheckboxes
         pControl = NULL;
         switch (pv->pThis->m_dwDisplayControl) {
         case 0:  // curvature
            pControl = pPage->ControlFind (L"curve");
            break;
         case 1:  // roof
            pControl = pPage->ControlFind (L"roof");
            break;
         case 2:  // walls
            pControl = pPage->ControlFind (L"walls");
            break;
         case 3:  // verandah
            pControl = pPage->ControlFind (L"verandah");
            break;
         default:
            {
               WCHAR szTemp[32];
               swprintf (szTemp, L"xc%d", (int) pv->pThis->m_dwDisplayControl);
               pControl = pPage->ControlFind (szTemp);
            }

         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         if (pv->pThis->m_dwRoofControlFreedom == 1)
            pControl = pPage->ControlFind (L"constreduced");
         else if (pv->pThis->m_dwRoofControlFreedom == 2)
            pControl = pPage->ControlFind (L"constnone");
         else
            pControl = pPage->ControlFind (L"constnorm");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // disable controls if no roof/wall
         if (pv->pThis->m_dwRoofType == ROOFSHAPE_NONE) {
            pControl = pPage->ControlFind (L"roof");
            if (pControl)
               pControl->Enable (FALSE);
         }
         if (pv->pThis->m_dwWallType == WALLSHAPE_NONE) {
            pControl = pPage->ControlFind (L"walls");
            if (pControl)
               pControl->Enable (FALSE);
         }
         if (pv->pThis->m_dwVerandahType == WALLSHAPE_NONE) {
            pControl = pPage->ControlFind (L"verandah");
            if (pControl)
               pControl->Enable (FALSE);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // find otu which is checked
         PCEscControl pControl;
         DWORD dwNew;

         if (!_wcsicmp(p->pControl->m_pszName, L"constnone") ||
            !_wcsicmp(p->pControl->m_pszName, L"constreduced") ||
            !_wcsicmp(p->pControl->m_pszName, L"constnorm")) {

            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            if (!_wcsicmp(p->pControl->m_pszName, L"constnone"))
               pv->pThis->m_dwRoofControlFreedom = 2;
            else if (!_wcsicmp(p->pControl->m_pszName, L"constreduced"))
               pv->pThis->m_dwRoofControlFreedom = 1;
            else
               pv->pThis->m_dwRoofControlFreedom = 0;
            pv->pThis->m_dwDisplayControl = 1;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

            // make sure they're visible
            pv->pThis->m_pWorld->SelectionClear();
            GUID gObject;
            pv->pThis->GUIDGet (&gObject);
            pv->pThis->m_pWorld->SelectionAdd (pv->pThis->m_pWorld->ObjectFind(&gObject));
            return TRUE;
         }


         if ((pControl = pPage->ControlFind (L"curve")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 0;  // curve;
         else if ((pControl = pPage->ControlFind (L"roof")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 1; // roof
         else if ((pControl = pPage->ControlFind (L"walls")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 2; // walls
         else if ((pControl = pPage->ControlFind (L"verandah")) && pControl->AttribGetBOOL(Checked()))
            dwNew = 3; // verandah
         else if (p->pControl->AttribGetBOOL(Checked()) && (p->pControl->m_pszName[0] == L'x') && (p->pControl->m_pszName[1] == L'c')) {
            dwNew = _wtoi(p->pControl->m_pszName + 2);
         }
         else
            break;   // none of the above
         if (dwNew == pv->pThis->m_dwDisplayControl)
            return TRUE;   // no change

         pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
         pv->pThis->m_dwDisplayControl = dwNew;
         pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

         // make sure they're visible
         pv->pThis->m_pWorld->SelectionClear();
         GUID gObject;
         pv->pThis->GUIDGet (&gObject);
         pv->pThis->m_pWorld->SelectionAdd (pv->pThis->m_pWorld->ObjectFind(&gObject));
         return TRUE;

      }
      break;   // default


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Which control points are displayed";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DISPLAY")) {
            MemZero (&gMemTemp);


            // first, calculate all the choices for overlays
            CMem  memGroup;
            MemZero (&memGroup);
            MemCat (&memGroup, L"curve,roof,walls,verandah");
            DWORD i;
            PWSTR psz;
            PTEXTUREPOINT ptp;
            DWORD dwNum;
            DWORD dwColor;
            BOOL fClockwise;
            PCSplineSurface pss;
            if (pv->iSide && pv->pSurf && pv->pSurf->pSurface) {
               pss = (pv->iSide == 1) ? &pv->pSurf->pSurface->m_SplineA :
                     &pv->pSurf->pSurface->m_SplineB;
               for (i = 0; i < pss->OverlayNum(); i++) {
                  if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
                     continue;
                  dwColor = OverlayShapeNameToColor(psz);
                  if (dwColor == (DWORD)-1)
                     continue;
                  MemCat (&memGroup, L",xc");
                  MemCat (&memGroup, (int) dwColor);
               }
            }

            // balustrade
            if (pv->pThis)
               for (i = 0; i < pv->pThis->m_lBBBALUSTRADE.Num(); i++) {
                  MemCat (&memGroup, L",xc");
                  MemCat (&memGroup, (int) i + DISPLAYBASE_BALUSTRADE);
               }

            // then put in the buttons
            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=curve><bold>Size</bold><br/>");
            MemCat (&gMemTemp, L"These control points let resize the building block.</xChoiceButton>");

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=roof><bold>Roof</bold><br/>");
            MemCat (&gMemTemp, L"Control the shape of the roof. (For the style of roof see the \"Roof\"page.)</xChoiceButton>");

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=walls><bold>Walls</bold><br/>");
            MemCat (&gMemTemp, L"Control the shape of the walls. (For the style of walls see the \"Walls\"page.)</xChoiceButton>");

            MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
            MemCat (&gMemTemp, (PWSTR) memGroup.p);
            MemCat (&gMemTemp, L" name=verandah><bold>Verandah</bold><br/>");
            MemCat (&gMemTemp, L"Control the shape of the verandah. (For the style of verandah see the \"Verandahs\"page.)</xChoiceButton>");

            if (pv->iSide && pv->pSurf && pv->pSurf->pSurface) {
               pss = (pv->iSide == 1) ? &pv->pSurf->pSurface->m_SplineA :
                     &pv->pSurf->pSurface->m_SplineB;
               for (i = 0; i < pss->OverlayNum(); i++) {
                  if (!pss->OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
                     continue;
                  dwColor = OverlayShapeNameToColor(psz);
                  if (dwColor == (DWORD)-1)
                     continue;
                  WCHAR szTemp[128];
                  wcscpy (szTemp, psz);
                  if (wcschr(szTemp, L':'))
                     (wcschr(szTemp, L':'))[0] = 0;

                  MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
                  MemCat (&gMemTemp, (PWSTR) memGroup.p);
                  MemCat (&gMemTemp, L" name=xc");
                  MemCat (&gMemTemp, (int)dwColor);
                  MemCat (&gMemTemp, L"><bold>");
                  MemCatSanitize (&gMemTemp, szTemp);
                  MemCat (&gMemTemp, L"</bold><br/>");
                  MemCat (&gMemTemp, L"Control points for a color/texture overlay.</xChoiceButton>");
               }
            }

            // balustrade
            if (pv->pThis)
               for (i = 0; i < pv->pThis->m_lBBBALUSTRADE.Num(); i++) {
                  PBBBALUSTRADE pBal = (PBBBALUSTRADE) pv->pThis->m_lBBBALUSTRADE.Get(i);
                  if (!pBal)
                     continue;

                  MemCat (&gMemTemp, L"<xChoiceButton style=check radiobutton=true group=");
                  MemCat (&gMemTemp, (PWSTR) memGroup.p);
                  MemCat (&gMemTemp, L" name=xc");
                  MemCat (&gMemTemp, (int)i + DISPLAYBASE_BALUSTRADE);
                  MemCat (&gMemTemp, L"><bold>");
                  if (pBal->dwMajor == BBBALMAJOR_BALUSTRADE) {
                     MemCat (&gMemTemp, L"Balustrade (Level ");
                     MemCat (&gMemTemp, (DWORD)i+1);
                     MemCat (&gMemTemp, L")");
                  }
                  else
                     MemCat (&gMemTemp, L"Piers");
                  MemCat (&gMemTemp, L"</bold><br/>");
                  MemCat (&gMemTemp, L"Control points for the balustrade openings.</xChoiceButton>");
               }

            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/********************************************************************************
GenerateThreeDFromSpline - Given a spline on a surface, this sets a threeD
control with the spline.

inputs
   PWSTR       pszControl - Control name.
   PCEscPage   pPage - Page
   PCSpline    pSpline - Spline to draw
   PCSplineSurface pss - Surface
   DWORD       dwUse - If 0 it's for adding/remove splines, else if 1 it's for cycling curves
returns
   BOOl - TRUE if success

NOTE: The ID's are such:
   LOBYTE = x
   3rd lowest byte = 1 for edge, 2 for point
*/
static BOOL GenerateThreeDFromSpline (PWSTR pszControl, PCEscPage pPage, PCSpline pSpline, PCSplineSurface pss,
                                      DWORD dwUse)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // figure out the center
   CPoint pCenter, pTemp, pMin, pMax;
   DWORD i;
   pCenter.Zero();
   pMin.Zero();
   pMax.Zero();
   DWORD x, y, dwWidth, dwHeight;
   dwWidth = pss->ControlNumGet (TRUE);
   dwHeight = pss->ControlNumGet (FALSE);
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      if ((y == 0) && (x == 0)) {
         pMin.Copy (&pTemp);
         pMax.Copy (&pTemp);
         continue;
      }

      // else, do min/max
      for (i = 0; i < 3; i++) {
         pMin.p[i] = min(pMin.p[i], pTemp.p[i]);
         pMax.p[i] = max(pMax.p[i], pTemp.p[i]);
      }
   }
   pCenter.Copy (&pMin);
   pCenter.Add (&pMax);
   pCenter.Scale (.5);

   // figure out the maximum distance
   fp fTemp, fMax;
   fMax = 0.001;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      if (!pss->ControlPointGet (x, y, &pTemp))
         return FALSE;
      pTemp.Subtract (&pCenter);
      fTemp = pTemp.Length();
      if (fTemp > fMax)
         fMax = fTemp;
   }
   fMax /= 5;  // so is larger

   // when draw points, get the point, subtract the center, and divide by fMax

   // use gmemtemp
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<rotatex val=-90/><backculloff/>");   //  so that Z is up (on the screen)

   // draw the surface as wireframe
   MemCat (&gMemTemp, L"<id val=1/><wireframeon/>");
   WCHAR szTemp[128];
   for (y = 0; y < dwHeight-1; y++) for (x = 0; x < dwWidth-1; x++) {
      MemCat (&gMemTemp, L"<colordefault color=#404080/>");

      MemCat (&gMemTemp, L"<shapepolygon");

      for (i = 0; i < 4; i++) {
         switch (i) {
         case 0: // UL
            pss->ControlPointGet (x,y, &pTemp);
            break;
         case 1: // UR
            pss->ControlPointGet (x+1,y, &pTemp);
            break;
         case 2: // LR
            pss->ControlPointGet (x+1,y+1, &pTemp);
            break;
         case 3: // LL
            pss->ControlPointGet (x,y+1, &pTemp);
            break;
         }
         pTemp.Subtract (&pCenter);
         pTemp.Scale (1.0 / fMax);

         swprintf (szTemp, L" p%d=%g,%g,%g",
            (int) i+1, (double)pTemp.p[0], (double)pTemp.p[1], (double)pTemp.p[2]);
         MemCat (&gMemTemp, szTemp);
      }

      MemCat (&gMemTemp, L"/>");
   }

   MemCat (&gMemTemp, L"<wireframeoff/>");

   // draw the outline
   DWORD dwNum;
   dwNum = pSpline->OrigNumPointsGet();
   for (x = 0; x < dwNum; x++) {
      CPoint p1, p2;
      pSpline->OrigPointGet (x, &p1);
      pSpline->OrigPointGet ((x+1) % dwNum, &p2);

      // convert from HV to object space
      pss->HVToInfo (p1.p[0], p1.p[1], &p1);
      pss->HVToInfo (p2.p[0], p2.p[1], &p2);
      p1.Subtract (&pCenter);
      p2.Subtract (&pCenter);
      p1.Scale (1.0 / fMax);
      p2.Scale (1.0 / fMax);


      // draw a line
      if (dwUse == 0)
         MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
      else {
         DWORD dwSeg;
         pSpline->OrigSegCurveGet (x, &dwSeg);
         switch (dwSeg) {
         case SEGCURVE_CUBIC:
            MemCat (&gMemTemp, L"<colordefault color=#8080ff/>");
            break;
         case SEGCURVE_CIRCLENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#ffc0c0/>");
            break;
         case SEGCURVE_CIRCLEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#c04040/>");
            break;
         case SEGCURVE_ELLIPSENEXT:
            MemCat (&gMemTemp, L"<colordefault color=#40c040/>");
            break;
         case SEGCURVE_ELLIPSEPREV:
            MemCat (&gMemTemp, L"<colordefault color=#004000/>");
            break;
         default:
         case SEGCURVE_LINEAR:
            MemCat (&gMemTemp, L"<colordefault color=#c0c0c0/>");
            break;
         }
      }

      // set the ID
      MemCat (&gMemTemp, L"<id val=");
      MemCat (&gMemTemp, (int)((1 << 16) | x));
      MemCat (&gMemTemp, L"/>");

      MemCat (&gMemTemp, L"<shapearrow tip=false width=.1");

      swprintf (szTemp, L" p1=%g,%g,%g p2=%g,%g,%g/>",
         (double)p1.p[0], (double)p1.p[1], (double)p1.p[2], (double)p2.p[0], (double)p2.p[1], (double)p2.p[2]);
      MemCat (&gMemTemp, szTemp);

      // do push point if more than 3 points
      if ((dwUse == 0) && (dwNum > 3)) {
         MemCat (&gMemTemp, L"<matrixpush>");
         swprintf (szTemp, L"<translate point=%g,%g,%g/>",
            (double)p1.p[0], (double)p1.p[1], (double)p1.p[2]);
         MemCat (&gMemTemp, szTemp);
         MemCat (&gMemTemp, L"<colordefault color=#ff0000/>");
         // set the ID
         MemCat (&gMemTemp, L"<id val=");
         MemCat (&gMemTemp, (int)((2 << 16) | x));
         MemCat (&gMemTemp, L"/>");

         MemCat (&gMemTemp, L"<MeshSphere radius=.3/><shapemeshsurface/>");

         MemCat (&gMemTemp, L"</matrixpush>");
      }
   }

   // set the threeD control
   ESCMTHREEDCHANGE tc;
   memset (&tc, 0, sizeof(tc));
   tc.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_THREEDCHANGE, &tc);

   return TRUE;
}



/* BuildBlockWallsAnyPage
*/
BOOL BuildBlockWallsAnyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDSI pv = (PDSI)pPage->m_pUserData;
   DWORD *pdwType = pv->fWall ? &pv->pThis->m_dwWallType : &pv->pThis->m_dwVerandahType;
   PCSpline plWallVerandah = pv->fWall ? &pv->pThis->m_sWall : &pv->pThis->m_sVerandah;
   PCListFixed plAngle = pv->fWall ? &pv->pThis->m_lWallAngle : &pv->pThis->m_lVerandahAngle;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // draw images
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, plWallVerandah, pv->pssAny, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, plWallVerandah, pv->pssAny, 1);

         // make sure display control
         DWORD dwDisplay = pv->fWall ? 2 : 3;
         if (pv->pThis->m_dwDisplayControl != dwDisplay) {
            pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
            pv->pThis->m_dwDisplayControl = dwDisplay;
            pv->pThis->m_pWorld->ObjectChanged (pv->pThis);
         }

         // make sure they're visible
         pv->pThis->m_pWorld->SelectionClear();
         GUID gObject;
         pv->pThis->GUIDGet (&gObject);
         pv->pThis->m_pWorld->SelectionAdd (pv->pThis->m_pWorld->ObjectFind(&gObject));
      }
      break;

   case ESCN_THREEDCLICK:
      {
         PESCNTHREEDCLICK p = (PESCNTHREEDCLICK) pParam;
         if (!p->pControl->m_pszName)
            break;

         BOOL  fCol;
         if (!_wcsicmp(p->pControl->m_pszName, L"edgeaddremove"))
            fCol = TRUE;
         else if (!_wcsicmp(p->pControl->m_pszName, L"edgecurve"))
            fCol = FALSE;
         else
            break;

         // figure out x, y, and what clicked on
         DWORD x, dwMode;
         dwMode = (BYTE)(p->dwMajor >> 16);
         x = (BYTE) p->dwMajor;
         if ((dwMode < 1) || (dwMode > 2))
            break;

         // allocate enough memory so can do the calculations
         CMem  memPoints;
         CMem  memSegCurve;
         DWORD dwOrig;
         dwOrig = plWallVerandah->OrigNumPointsGet();
         if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
            return TRUE;
         if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
            return TRUE;

         // load it in
         PCPoint paPoints;
         DWORD *padw;
         paPoints = (PCPoint) memPoints.p;
         padw = (DWORD*) memSegCurve.p;
         DWORD i;
         for (i = 0; i < dwOrig; i++) {
            plWallVerandah->OrigPointGet (i, paPoints+i);
         }
         for (i = 0; i < dwOrig; i++)
            plWallVerandah->OrigSegCurveGet (i, padw + i);
         DWORD dwMinDivide, dwMaxDivide;
         fp fDetail;
         plWallVerandah->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);

         if (fCol) {
            if (dwMode == 1) {
               // inserting
               memmove (paPoints + (x+1), paPoints + x, sizeof(CPoint) * (dwOrig-x));
               paPoints[x+1].Add (paPoints + ((x+2) % (dwOrig+1)));
               paPoints[x+1].Scale (.5);
               memmove (padw + (x+1), padw + x, sizeof(DWORD) * (dwOrig - x));
               dwOrig++;

               fp fAngle;
               fAngle = *((fp*) plAngle->Get(x));
               if ((x+1) < plAngle->Num())
                  plAngle->Insert (x+1, &fAngle);
               else
                  plAngle->Add (&fAngle);
            }
            else if (dwMode == 2) {
               // deleting
               memmove (paPoints + x, paPoints + (x+1), sizeof(CPoint) * (dwOrig-x-1));
               memmove (padw + x, padw + (x+1), sizeof(DWORD) * (dwOrig - x - 1));
               dwOrig--;

               plAngle->Remove (x);
            }
         }
         else {
            // setting curvature
            if (dwMode == 1) {
               padw[x] = (padw[x] + 1) % (SEGCURVE_MAX+1);
            }
         }

         // how much divide
         DWORD dwDivide;
         dwDivide = 0;
         for (i = 0; i < dwOrig; i++)
            if (padw[i] != SEGCURVE_LINEAR)
               dwDivide = 3;
         dwMinDivide = dwMaxDivide = dwDivide;

         pv->pThis->m_pWorld->ObjectAboutToChange (pv->pThis);
         plWallVerandah->Init (TRUE, dwOrig, paPoints, NULL, padw, dwMinDivide, dwMaxDivide, fDetail);
         pv->pThis->AdjustAllSurfaces();
         pv->pThis->m_pWorld->ObjectChanged (pv->pThis);

         // redraw the shapes
         GenerateThreeDFromSpline (L"edgeaddremove", pPage, plWallVerandah, pv->pssAny, 0);
         GenerateThreeDFromSpline (L"edgecurve", pPage, plWallVerandah, pv->pssAny, 1);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = pv->fWall ?
               L"Modify wall sections for Any-Shape walls" : 
               L"Modify wall sections for Any-Shape verandah";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CObjectBuildBlock::DialogShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/

BOOL CObjectBuildBlock::DialogShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   DSI dsi;
   memset (&dsi, 0, sizeof(dsi));
   dsi.pThis = this;

   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);
      PCLAIMSURFACE pcs;
      if (pcs = pbbs->pSurface->ClaimFindByID (dwSurface)) {
         dsi.pSurf = pbbs;
         switch (pcs->dwReason) {
         case 0:
         case 3:
            dsi.iSide = 1;
            break;
         case 1:
         case 4:
            dsi.iSide = -1;
         }
         break;
      }
   }

   // see if matches a balustrrade]
   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      PBBBALUSTRADE pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(i);
      if (pbb->pBal && pbb->pBal->ClaimFindByID (dwSurface)) {
         dsi.pBal = pbb;
         break;
      }
      else if (pbb->pPiers && pbb->pPiers->ClaimFindByID (dwSurface)) {
         dsi.pBal = pbb;
         break;
      }
   }

   PWSTR pszRet;

firstpage:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKDIALOG, BuildBlockDialogPage, &dsi);
firstpage2:
   if (!pszRet)
      return FALSE;
  if (!_wcsicmp(pszRet, L"overside") && dsi.iSide && dsi.pSurf) {
      BOOL fSideA = (dsi.iSide == 1);
      pszRet = dsi.pSurf->pSurface->SurfaceOverMainPage (pWindow, fSideA, &m_dwDisplayControl);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
  if (!_wcsicmp(pszRet, L"framing") && dsi.pSurf) {
      pszRet = dsi.pSurf->pSurface->SurfaceFramingPage (pWindow);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
  else if (!_wcsicmp(pszRet, L"balappear") && dsi.pBal->pBal) {
      pszRet = dsi.pBal->pBal->AppearancePage (pWindow, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"balopenings") && dsi.pBal->pBal) {
      pszRet = dsi.pBal->pBal->OpeningsPage (pWindow, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"piersappear") && dsi.pBal->pPiers) {
      pszRet = dsi.pBal->pPiers->AppearancePage (m_OSINFO.dwRenderShard, pWindow, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"piersopenings") && dsi.pBal->pPiers) {
      pszRet = dsi.pBal->pPiers->OpeningsPage (m_OSINFO.dwRenderShard, pWindow, this);

      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"roof")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKROOF, BuildBlockRoofPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"walls")) {
      CSplineSurface ss;
      CPoint paPoints[2][2];
      DWORD x,y;
      for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
         paPoints[y][x].Zero();
         paPoints[y][x].p[0] = x ? m_fXMax : m_fXMin;
         paPoints[y][x].p[2] = y ? m_fYMin : m_fYMax;
      }
      ss.ControlPointsSet (FALSE, FALSE, 2, 2, &paPoints[0][0], (DWORD*)SEGCURVE_LINEAR, (DWORD*)SEGCURVE_LINEAR);
      dsi.pssAny = &ss;
      dsi.fWall = TRUE;
backtowalls:
      
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKWALLS, BuildBlockWallsPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, L"anyshape")) {
backtoanyshape:
         pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKWALLSANY, BuildBlockWallsAnyPage, &dsi);
         if (pszRet && !_wcsicmp(pszRet, L"displaycontrol")) {
            pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKDISPLAY, BuildBlockDisplayPage, &dsi);
            if (pszRet && !_wcsicmp(pszRet, Back()))
               goto backtoanyshape;
         }
         if (pszRet && !_wcsicmp(pszRet, Back()))
            goto backtowalls;
      }
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"verandah")) {
      CSplineSurface ss;
      CPoint paPoints[2][2];
      DWORD x,y;
      for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) {
         paPoints[y][x].Zero();
         paPoints[y][x].p[0] = x ? m_fXMax : m_fXMin;
         paPoints[y][x].p[2] = y ? m_fYMin : m_fYMax;
      }
      ss.ControlPointsSet (FALSE, FALSE, 2, 2, &paPoints[0][0], (DWORD*)SEGCURVE_LINEAR, (DWORD*)SEGCURVE_LINEAR);
      dsi.pssAny = &ss;
      dsi.fWall = FALSE;
backtoverandahs:
      
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKVERANDAH, BuildBlockWallsPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, L"anyshape")) {
backtoanyshapeverandah:
         pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKVERANDAHANY, BuildBlockWallsAnyPage, &dsi);
         if (pszRet && !_wcsicmp(pszRet, L"displaycontrol")) {
            pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKDISPLAY, BuildBlockDisplayPage, &dsi);
            if (pszRet && !_wcsicmp(pszRet, Back()))
               goto backtoanyshapeverandah;
         }
         if (pszRet && !_wcsicmp(pszRet, Back()))
            goto backtoverandahs;
      }
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"foundation")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKFOUNDATION, BuildBlockFoundationPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"intel")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKINTEL, BuildBlockIntelPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"floors")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKFLOORS, BuildBlockFloorsPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"ceiling")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKCEILING, BuildBlockCeilingPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
   else if (!_wcsicmp(pszRet, L"levels")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKLEVELS, BuildBlockLevelsPage, &dsi);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto firstpage;
      else
         goto firstpage2;
   }
//   else if (!_wcsicmp(pszRet, L"displaycontrol")) {
//      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKDISPLAY, BuildBlockDisplayPage, &dsi);
//      if (pszRet && !_wcsicmp(pszRet, Back()))
//         goto firstpage;
//      else
//         goto firstpage2;
//   }

   return !_wcsicmp(pszRet, Back());
}



/**********************************************************************************
CObjectBuildBlock::DialogCPShow - Causes the object to show a dialog box that allows
different attributes to be changed.

inputs
   DWORD             dwSuface - Surface that was clicked on. 0 if unknown.
   PCEscWindow       pWindow - Escarpment window to draw it in
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/

BOOL CObjectBuildBlock::DialogCPShow (DWORD dwSurface, PCEscWindow pWindow, PCPoint pClick)
{
   DSI dsi;
   memset (&dsi, 0, sizeof(dsi));
   dsi.pThis = this;

   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);
      PCLAIMSURFACE pcs;
      if (pcs = pbbs->pSurface->ClaimFindByID (dwSurface)) {
         dsi.pSurf = pbbs;
         switch (pcs->dwReason) {
         case 0:
         case 3:
            dsi.iSide = 1;
            break;
         case 1:
         case 4:
            dsi.iSide = -1;
         }
         break;
      }
   }

   // see if matches a balustrrade]
   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      PBBBALUSTRADE pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(i);
      if (pbb->pBal && pbb->pBal->ClaimFindByID (dwSurface)) {
         dsi.pBal = pbb;
         break;
      }
      else if (pbb->pPiers && pbb->pPiers->ClaimFindByID (dwSurface)) {
         dsi.pBal = pbb;
         break;
      }
   }

   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLBUILDBLOCKDISPLAY, BuildBlockDisplayPage, &dsi);
   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}

/**************************************************************************************
CObjectBuildBlock::MoveReferencePointQuery - 
given a move reference index, this fill in pp with the position of
the move reference RELATIVE to ObjectMatrixGet. References are numbers
from 0+. If the index is more than the number of points then the
function returns FALSE

inputs
   DWORD       dwIndex - index.0 .. # ref
   PCPoint     pp - Filled with point relative to ObjectMatrixGet() IF its valid
returns
   BOOL - TRUE if valid index.
*/
BOOL CObjectBuildBlock::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaMoveBB;
   dwDataSize = sizeof(gaMoveBB);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   pp->Zero();
   switch (ps[dwIndex].iX) {
   case -1:
      pp->p[0] = m_fXMin;
      break;
   case 1:
      pp->p[0] = m_fXMax;
      break;
   case 0:
   default:
      pp->p[0] = (m_fXMin + m_fXMax) / 2;
      break;
   }
   switch (ps[dwIndex].iY) {
   case -1:
      pp->p[1] = m_fYMin;
      break;
   case 1:
      pp->p[1] = m_fYMax;
      break;
   case 0:
   default:
      pp->p[1] = (m_fYMin + m_fYMax) / 2;
      break;
   }

   return TRUE;
}

/**************************************************************************************
CObjectBuildBlock::MoveReferenceStringQuery -
given a move reference index (numbered from 0 to the number of references)
this fills in a string at psz and dwSize BYTES that names the move reference
to the end user. *pdwNeeded is filled with the number of bytes needed for
the string. Returns FALSE if dwIndex is too high, or dwSize is too small (although
pdwNeeded will be filled in)

inputs
   DWORD       dwIndex - index. 0.. # ref
   PWSTR       psz - To be filled in witht he string
   DWORD       dwSize - # of bytes available in psz
   DWORD       *pdwNeeded - If not NULL, filled with the size needed
returns
   BOOL - TRUE if psz copied.
*/
BOOL CObjectBuildBlock::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   ps = gaMoveBB;
   dwDataSize = sizeof(gaMoveBB);
   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   DWORD dwNeeded;
   dwNeeded = ((DWORD)wcslen (ps[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, ps[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CObjectBuildBlock::ContHVQuery
Called to see the HV location of what clicked on, or in other words, given a line
from pEye to pClick (object sapce), where does it intersect surface dwSurface (LOWORD(PIMAGEPIXEL.dwID))?
Fills pHV with the point. Returns FALSE if doesn't intersect, or invalid surface, or
can't tell with that surface. NOTE: if pOld is not NULL, then assume a drag from
pOld (converted to world object space) to pClick. Finds best intersection of the
surface then. Useful for dragging embedded objects around surface
*/
BOOL CObjectBuildBlock::ContHVQuery (PCPoint pEye, PCPoint pClick, DWORD dwSurface, PTEXTUREPOINT pOld, PTEXTUREPOINT pHV)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContHVQuery (pEye, pClick, dwSurface, pOld, pHV))
         return TRUE;
   }
   return FALSE;
}


/**********************************************************************************
CObjectTemplate::ContCutout
Called by an embeded object to specify an arch cutout within the surface so the
object can go through the surface (like a window). The container should check
that pgEmbed is a valid object. dwNum is the number of points in the arch,
paFront and paBack are the container-object coordinates. (See CSplineSurface::ArchToCutout)
If dwNum is 0 then arch is simply removed. If fBothSides is true then both sides
of the surface are cleared away, else only the side where the object is embedded
is affected.

*/
BOOL CObjectBuildBlock::ContCutout (const GUID *pgEmbed, DWORD dwNum, PCPoint paFront, PCPoint paBack, BOOL fBothSides)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContCutout (pgEmbed, dwNum, paFront, paBack, fBothSides))
         return TRUE;
   }
   return FALSE;
}

/**********************************************************************************
CObjectBuildBlock::ContCutoutToZipper -
Assumeing that ContCutout() has already been called for the pgEmbed, this askes the surfaces
for the zippering information (CRenderSufrace::ShapeZipper()) that would perfectly seal up
the cutout. plistXXXHVXYZ must be initialized to sizeof(HVXYZ). plisstFrontHVXYZ is filled with
the points on the side where the object was embedded, and plistBackHVXYZ are the opposite side.
NOTE: These are in the container's object space, NOT the embedded object's object space.
*/
BOOL CObjectBuildBlock::ContCutoutToZipper (const GUID *pgEmbed, PCListFixed plistFrontHVXYZ, PCListFixed plistBackHVXYZ)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContCutoutToZipper (pgEmbed, plistFrontHVXYZ, plistBackHVXYZ))
         return TRUE;
   }
   return FALSE;
}


/**********************************************************************************
CObjectBuildBlock::ContThickness - 
returns the thickness of the surface (dwSurface) at pHV. Used by embedded
objects like windows to know how deep they should be.

NOTE: usually overridden
*/
fp CObjectBuildBlock::ContThickness (DWORD dwSurface, PTEXTUREPOINT pHV)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      fp fThickness;
      fThickness = pbbs->pSurface->ContThickness (dwSurface, pHV);
      if (fThickness)
         return fThickness;
   }
   return 0;
}


/**********************************************************************************
CObjectBuildBlock::ContSideInfo -
returns a DWORD indicating what class of surface it is... right now
0 = internal, 1 = external. dwSurface is the surface. If fOtherSide
then want to know what's on the opposite side. Returns 0 if bad dwSurface.

NOTE: usually overridden
*/
DWORD CObjectBuildBlock::ContSideInfo (DWORD dwSurface, BOOL fOtherSide)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      DWORD dw;
      dw = pbbs->pSurface->ContSideInfo (dwSurface, fOtherSide);
      if (dw)
         return dw;
   }
   return 0;
}


/**********************************************************************************
CObjectBuildBlock::ContMatrixFromHV - THE SUPERCLASS SHOULD OVERRIDE THIS. Given
a point on a surface (that supports embedding), this returns a matrix that translates
0,0,0 to the same in world space as where pHV is, and also applies fRotation around Y (clockwise)
so that X and Z are (for the most part) still on the surface.

inputs
   DWORD             dwSurface - Surface ID that can be embedded. Should check to make sure is valid
   PTEXTUREPOINT     pHV - HV, 0..1 x 0..1 locaiton within the surface
   fp            fRotation - Rotation in radians, clockwise.
   PCMatrix          pm - Filled with the new rotation matrix
returns
   BOOL - TRUE if success
*/
BOOL CObjectBuildBlock::ContMatrixFromHV (DWORD dwSurface, PTEXTUREPOINT pHV, fp fRotation, PCMatrix pm)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->pSurface->ContMatrixFromHV (dwSurface, pHV, fRotation, pm)) {
         // then rotate by the object's rotation matrix to get to world space
         pm->MultiplyRight (&m_MatrixObject);
         return TRUE;
      }
   }
   return FALSE;
}


/**********************************************************************************
CObjectBuildBlock::Message -
sends a message to the object. The interpretation of the message depends upon
dwMessage, which is OSM_XXX. If the function understands and handles the
message it returns TRUE, otherwise FALE.
*/
BOOL CObjectBuildBlock::Message (DWORD dwMessage, PVOID pParam)
{
   BOOL fRet = FALSE;

   // trap some messages
   switch (dwMessage) {
   case OSM_BUILDBLOCKQUERY:
      {
         POSMBUILDBLOCKQUERY p = (POSMBUILDBLOCKQUERY) pParam;
         p->fArea = (m_fXMax - m_fXMin) * (m_fYMax - m_fYMin);
         if (m_fKeepFloor)
            p->fArea = 1000000;
         p->gID = m_gGUID;
         p->plBBCLIPVOLUME = new CListFixed;
         if (!p->plBBCLIPVOLUME)
            return FALSE;

         if (!ToClippingVolume (p->plBBCLIPVOLUME)) {
            delete p->plBBCLIPVOLUME;
            return FALSE;
         }
      }
      return TRUE;

   case OSM_GLOBALLEVELCHANGED:
      if (!m_fLevelGlobal || !(m_fRestrictElevation || m_fRestrictHeight))
         return TRUE;

      if (GlobalFloorLevelsGet(m_pWorld, this, m_fLevelElevation, &m_fLevelHigher)) {
         // FUTURERELEASE  - Deal with moving floor levels up and down, and stuff embedded within

         // rearrange some stuff
         if (m_pWorld)
            m_pWorld->ObjectAboutToChange (this);

         // realign vertically so ground floor ok
         VerifyObjectMatrix ();

         // adjust the height of the building
         AdjustHeightToFloor ();

         AdjustAllSurfaces();
         if (m_pWorld)
            m_pWorld->ObjectChanged (this);
      }
      return TRUE;

   case OSM_FLOORLEVEL:
      {
         POSMFLOORLEVEL p = (POSMFLOORLEVEL) pParam;

         // translate to object space
         CPoint pObject;
         CMatrix mWorldToObject;
         m_MatrixObject.Invert4 (&mWorldToObject);
         pObject.Copy (&p->pTest);
         pObject.p[3] = 1;
         pObject.MultiplyLeft (&mWorldToObject);

         // figure out it's location...
         if ((pObject.p[0] < m_fXMin) || (pObject.p[0] > m_fXMax) || (pObject.p[1] < m_fYMin) || (pObject.p[1] > m_fYMax)) // BUGFIX - Was using p[2] in one case
            return FALSE;

         // get the height
            // BUGFIX - Use m_dwGroundFloor so can get right floor level
         int iLevel = p->iFloor + (int)m_dwGroundFloor;   // since basement -1
         iLevel = max(0, iLevel);
         if (iLevel < NUMLEVELS)
            pObject.p[2] = m_fLevelElevation[iLevel];
         else
            pObject.p[2] = m_fLevelElevation[NUMLEVELS-1] + (iLevel - (NUMLEVELS - 1)) * m_fLevelHigher;

         // BUGFIX - Only mutliply by matrix object is NOT restricting building elevation
         // because if restricted, then floor heights automatically converted to world height
         if (!m_fRestrictElevation)
            pObject.MultiplyLeft (&m_MatrixObject);
         p->fLevel = pObject.p[2];
         p->fIsWater = FALSE;
         p->fArea = (m_fXMax - m_fXMin) * (m_fYMax - m_fYMin);
      }
      return TRUE;

   default:
      break;   // pass it one
   }

   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      fRet |= pbbs->pSurface->Message (dwMessage, pParam);
   }
   return fRet;
}


/**************************************************************************************
IsSurfaceEntirelyInside - Test to see if a surface is entirely inside a volume
created by other surfaces.

NOTE: Only call this if there are NO intersections of the surface against the
other surfaces.

This works by taking a point in the center of pTest. Send a ray east and anotherone
west. If both intersect volumes its inside. Else, it's outside.

inputs
   PCSplineSurface      pTest - Surface to test
   DWORD                dwNum - Number of surfaces that form the volume
   PCSplineSurface      *paVolume - Pointer to an array of dwNum PCSplineSurfaces that define
                           the volume.
   PCMatrix             paVolumeMatrix - Pointer to an array of dwNum matricies that convert
                        from paVolume[i] space into pTest space
returns
   BOOL - TRUE if it's entirely inside
*/
BOOL IsSurfaceEntirelyInside (PCSplineSurface pTest,
                              DWORD dwNum, PCSplineSurface *paVolume, PCMatrix paVolumeMatrix)
{
   // get the cetner point
   CPoint pCenter;
   pTest->HVToInfo (.5, .5, &pCenter);
   pCenter.p[3] = 1;
   //pCenter.MultiplyLeft (pmTestToWorld);

   // two rays
   CPoint pRight, pLeft;
   pRight.Zero();
   pRight.p[0] = 1;
   pRight.Add (&pCenter);
   pLeft.Zero();
   pLeft.p[0] = -1;
   pLeft.Add (&pCenter);

   // see if there's anything tor ight or left
   BOOL fRight, fLeft;
   DWORD i;
   fRight = fLeft = FALSE;
   for (i = 0; i < dwNum; i++) {
      CMatrix mInv;
      paVolumeMatrix[i].Invert4 (&mInv);

      CPoint pC, pR, pL;
      pC.Copy (&pCenter);
      pC.MultiplyLeft (&mInv);
      pR.Copy (&pRight);
      pR.MultiplyLeft (&mInv);
      pL.Copy (&pLeft);
      pL.MultiplyLeft (&mInv);
      pR.Subtract (&pC);
      pL.Subtract (&pC);

      // intersect
      TEXTUREPOINT tp;
      CPoint pInter;
      PCPoint p;
      DWORD j, k;
      for (j = 0; j < 2; j++) {
         p = j ? &pR : &pL;
         if (!paVolume[i]->IntersectLine (&pC, p, &tp, TRUE, FALSE))
            continue;

         // it intersected
         paVolume[i]->HVToInfo (tp.h, tp.v, &pInter);

         // since may have intersected anywhere along ray, test to find
         // out which it intersected along. use longest side
         DWORD dwBest;
         fp fBest, fCur;
         for (k = 0; k < 3; k++) {
            fCur = p->p[k];
            if (!k || (fabs(fCur) > fabs(fBest))) {
               dwBest = k;
               fBest = fCur;
            }
         }

         // know which side it's longest on, so test against that one
         fp fInter;
         fInter = pInter.p[dwBest] - pC.p[dwBest];
         if (fInter * fBest <= 0)
            continue;   // intersect on oppoiste sides

         // else, remember this
         if (j)
            fRight = TRUE;
         else
            fLeft = TRUE;

         // if both true then it's entirely within
         if (fRight && fLeft)
            return TRUE;
      }
   }

   // if get here then they weren't both true, so not entirely within
   return FALSE;
}

/*************************************************************************************
ClipClear - Clear out clipping information for the given PCDoubleSurface. Gets rid of
   gpszBuildBlockClear();

inputs
   PCDoubleSurface   pSurface - surface
*/
static void ClipClear (PCDoubleSurface pSurface)
{
   int iSide;
   PCSplineSurface pss;
   DWORD j;
   for (iSide = -1; iSide <= 1; iSide += 2) {  // -1 for side B, 1 for for side A, 0 for center
      pss = (iSide == 1) ? &pSurface->m_SplineA : &pSurface->m_SplineB;

      // clear out the side's cutouts
      for (j = pss->CutoutNum()-1; j < pss->CutoutNum(); j--) {
         PWSTR pszName;
         PTEXTUREPOINT ptp;
         DWORD dwNum;
         BOOL fClockwise;
         if (!pss->CutoutEnum(j, &pszName, &ptp, &dwNum, &fClockwise))
            continue;
         if (!_wcsnicmp (pszName, gpszBuildBlockClip, wcslen(gpszBuildBlockClip)))
            pss->CutoutRemove (pszName);
      }
   }
}

/*************************************************************************************
CObjectBuildBlock::IntelligentAdjust
Tells the object to intelligently adjust itself based on nearby objects.
For walls, this means triming to the roof line, for floors, different
textures, etc. If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden
*/
BOOL CObjectBuildBlock::IntelligentAdjust (BOOL fAct)
{
   if (!fAct)
      return TRUE;

   if (!m_pWorld)
      return FALSE;

   // BUGFIX - Do an integrity check on embedded objects to make sure they're stil there
   DWORD i;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(i);
      if (ps->pSurface)
         ps->pSurface->EmbedIntegrityCheck();
   }

   // find what parts of the world this intersects with
   // get the bounding box for this. I know that because of tests in render
   // it will return even already clipped stuff
   DWORD dwThis, j;
   dwThis = m_pWorld->ObjectFind (&m_gGUID);
   CPoint pCorner1, pCorner2;
   CMatrix m, mInv;
   m_pWorld->BoundingBoxGet (dwThis, &m, &pCorner1, &pCorner2);
   m.Invert4 (&mInv);

   // find out what objects this intersects with
   CListFixed     lObjects;
   lObjects.Init(sizeof(DWORD));
   m_pWorld->IntersectBoundingBox (&m, &pCorner1, &pCorner2, &lObjects);

   // calculate area
   fp fArea;
   fArea = (m_fXMax - m_fXMin) * (m_fYMax - m_fYMin);
   if (m_fKeepFloor)
      fArea = 1000000;

   // remember where intersect
   CListFixed lBBQ;
   lBBQ.Init (sizeof(OSMBUILDBLOCKQUERY));

   // go through those objects and find out which are building blocks
   PCObjectSocket pos;
   POSMBUILDBLOCKQUERY pq;
   PBBCLIPVOLUME pcv;
   for (i = 0; i < lObjects.Num(); i++) {
      DWORD dwOn = *((DWORD*)lObjects.Get(i));
      if (dwOn == dwThis)
         continue;
      pos = m_pWorld->ObjectGet (dwOn);
      if (!pos)
         continue;

      OSMBUILDBLOCKQUERY q;
      memset (&q, 0, sizeof(q));
      if (!pos->Message (OSM_BUILDBLOCKQUERY, &q) || !q.plBBCLIPVOLUME)
         continue;

      // else, remember this
      lBBQ.Add (&q);
   }

   // note that about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // loop through each of the surfaces in this object
   DWORD dwSurf;
   CListFixed lPSS, lMatrix;
   lPSS.Init (sizeof(PCSplineSurface));
   lMatrix.Init (sizeof(CMatrix));
   CListVariable lCutoutsToRemove;
   for (dwSurf = 0; dwSurf < m_listBBSURF.Num(); dwSurf++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(dwSurf);

      // set dirty flag for framing
      if (ps->pSurface)
         ps->pSurface->m_fFramingDirty = TRUE;

      // clear the cutouts to remove
      lCutoutsToRemove.Clear();

      // create a matrix that converts from world space to spline space
      CMatrix mSplineToWorld, mWorldToSpline;
      ps->pSurface->MatrixGet (&mSplineToWorld);   // spline to object
      mSplineToWorld.MultiplyRight (&m_MatrixObject);
      mSplineToWorld.Invert4 (&mWorldToSpline);

      // assume that want to show
      ps->fHidden = FALSE;

      BOOL fFloor;
      fFloor = ((ps->dwMajor == BBSURFMAJOR_FLOOR) || (ps->dwMajor == BBSURFMAJOR_VERANDAH) || (ps->dwMajor == BBSURFMAJOR_TOPCEILING));

      ClipClear (ps->pSurface);

      DWORD dwClipNum;
      dwClipNum = 0;

      // clip the inside and the outside
      int iSide;
      CMatrix m;
      PCSplineSurface pss;
      for (iSide = -1; iSide <= 1; iSide += 2) {  // -1 for side B, 1 for for side A, 0 for center
         pss = (iSide == 1) ? &ps->pSurface->m_SplineA : &ps->pSurface->m_SplineB;

         // two passes for verandahs
         DWORD dwVerandah, dwVPasses;
         dwVPasses = 1;
         if (ps->dwMajor == BBSURFMAJOR_VERANDAH)
            dwVPasses = 3;
         for (dwVerandah = 0; dwVerandah < dwVPasses; dwVerandah++) {
            // keep a list of what surfaces need to clip against
            for (i = 0; i < lBBQ.Num(); i++) {
               pq = (POSMBUILDBLOCKQUERY) lBBQ.Get(i);

               // if this is the verandah, and intersecting against other verandahs,
               // then cutout now, but may want to remove later. Don't remove
               // until calculate intersection of verandahs for use with the 
               // balistrades, so that if two verandahs intersect their ballistrades
               // intersect, and you don't have one verandah blocking out another.

               BOOL fWantToRemoveLater;
               fWantToRemoveLater = FALSE;

               if (dwVerandah) {
                  // if it's "superior" to the
                  // surface we're up against, then don't clip
                  // this is larger so dont clip floor
                  if (fArea > pq->fArea)
                     fWantToRemoveLater = TRUE;
                  // this is higher so don't clip floor
                  else if ((fArea == pq->fArea) && (memcmp(&pq->gID, &m_gGUID, sizeof(m_gGUID)) <= 0))
                     fWantToRemoveLater = TRUE;

                  // 1st pass of verandah, only remove verandahs that dont want to remove
                  if (fWantToRemoveLater && (dwVerandah == 1))
                     continue;
                  // 2nd pass, opposite
                  if (!fWantToRemoveLater && (dwVerandah == 2))
                     continue;
               }
               else {
                  // if this is a floor/ceiling AND it's "superior" to the
                  // surface we're up against, then don't clip
                  if (fFloor && (ps->dwMajor != BBSURFMAJOR_VERANDAH)) {
                     // this is larger so dont clip floor
                     if (fArea > pq->fArea)
                        continue;

                     // this is higher so don't clip floor
                     if ((fArea == pq->fArea) && (memcmp(&pq->gID, &m_gGUID, sizeof(m_gGUID)) <= 0))
                        continue;

                     // else, clip
                  }
               }

               // clear list
               lPSS.Clear();
               lMatrix.Clear();

               for (j = 0; j < pq->plBBCLIPVOLUME->Num(); j++) {
                  pcv = (PBBCLIPVOLUME) pq->plBBCLIPVOLUME->Get(j);

                  if (dwVerandah) {
                     // only do verandahs here
                     if (pcv->dwType != 4)
                        continue;

                     // always clip floors against the half-wa
                     if (pcv->iSide != 0)
                        continue;
                  }
                  else {
                     // dont do verandahs here
                     if (pcv->dwType == 4)
                        continue;

                     // if have keep walls checked then don't
                     // intersect walls against anything
                     if (m_fKeepWalls && (ps->dwMajor == BBSURFMAJOR_WALL))
                        continue;

                     // floors dont clip against other floors
                     if (fFloor && ((pcv->dwType == 2) || pcv->dwType == 3))
                        continue;

                     if (fFloor) {
                        // always clip floors against the half-wa
                        if (pcv->iSide != 0)
                           continue;
                     }
                     else {
                        // only clip against same side - inside vs. inside, outside vs. outside,
                        if (iSide != pcv->iSide)
                           continue;
                     }
                  }
            
                  lPSS.Add (&pcv->pss);

                  // make a matrix that translates from the clipping surface's coordinates
                  // into the clipped surface coordinates
                  m.Multiply (&mWorldToSpline, &pcv->mToWorld);
                  lMatrix.Add (&m);
               }  // loop over pq->plBBCLIPVOLUME


               // generate the name
               WCHAR szTemp[64];
               swprintf (szTemp, L"%s%d", gpszBuildBlockClip, (int) (dwClipNum++));

               // remember if we want to remove later
               if (fWantToRemoveLater)
                  lCutoutsToRemove.Add (szTemp, (wcslen(szTemp)+1)*2);

               // will it be clockwise
               BOOL fClockwise;
               fClockwise = TRUE;
               if (fFloor) {
                  //if (iSide == -1)
                  //   fClockwise = !fClockwise;
               }
               if (!fFloor && (iSide == -1))
                  fClockwise = !fClockwise;

               pss->IntersectWithSurfacesAndCutout ((PCSplineSurface*) lPSS.Get(0),
                  (PCMatrix) lMatrix.Get(0), lPSS.Num(), szTemp,
                  FALSE /*fIgnoreCutouts*/, fClockwise, 0 /* dont change interseciton orient*/,
                  TRUE /*fUseMin*/,
                  TRUE /*fFinalClockwise*/, NULL);

               // find the bit that just added
               DWORD dwCutout, dwCutoutNum;
               PTEXTUREPOINT ptp;
               PWSTR pszName;
               for (dwCutout = 0; dwCutout < pss->CutoutNum(); dwCutout++) {
                  if (!pss->CutoutEnum (dwCutout, &pszName, &ptp, &dwCutoutNum, &fClockwise))
                     continue;
                  if (!_wcsicmp(pszName, szTemp))
                     break;
               }

               ps->fHidden = FALSE; // make sure it's visible

               if (dwCutout >= pss->CutoutNum()) {
                  // if didn't find then see if it's entirely inside
                  // see if the surface is compeletey hidden
                  if (IsSurfaceEntirelyInside(pss, lPSS.Num(),
                     (PCSplineSurface*) lPSS.Get(0), (PCMatrix) lMatrix.Get(0)))
                      ps->fHidden = TRUE;
               }
               else {
                  // see if this cuts out the entire object, because if it does
                  // then want to see if it's entirely within


                  // BUGFIX: Do fIsWholeThing so that perimeters don't
                  // get cut out entirely because just a bit of them is nicked
                  // if it's the whole thing then don't clip
                  if (IsCutoutEntireSurface (dwCutoutNum, ptp) && fClockwise) {
                     // see if the surface is compeletey hidden
                     if (IsSurfaceEntirelyInside(pss, lPSS.Num(),
                        (PCSplineSurface*) lPSS.Get(0), (PCMatrix) lMatrix.Get(0)))
                         ps->fHidden = TRUE;

                     // either way, remove the cutout
                     pss->CutoutRemove (szTemp);
                  }
               }
            }  // loop over pBBQ for main
         }  // loop over dwVerandah



      }  // loop over iSide

      // if it's a floor, then look for ballistrade elements that should be clipped based
      // on this
      if ((ps->dwMajor == BBSURFMAJOR_FLOOR) || (ps->dwMajor == BBSURFMAJOR_VERANDAH)) {
         DWORD k;
         for (k = 0; k < m_lBBBALUSTRADE.Num(); k++) {
            PBBBALUSTRADE pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(k);
            if (pbb->dwMinor != ps->dwMinor)
               continue;

            // else, adjust
            CMatrix m;
            ps->pSurface->MatrixGet (&m);
            if (pbb->pBal)
               pbb->pBal->CutoutBasedOnSurface (&ps->pSurface->m_SplineA, &m);
            else if (pbb->pPiers)
               pbb->pPiers->CutoutBasedOnSurface (m_OSINFO.dwRenderShard, &ps->pSurface->m_SplineB, &m);
         }
      }

      // remove cutouts
      for (j = 0; j < lCutoutsToRemove.Num(); j++) {
         PWSTR psz = (PWSTR) lCutoutsToRemove.Get(j);

         // just tell both sides to remove even though only one side will have it
         ps->pSurface->m_SplineA.CutoutRemove (psz);
         ps->pSurface->m_SplineB.CutoutRemove (psz);
      }
   }  // loop over dwSurf

   // free up the lists
   for (i = 0; i < lBBQ.Num(); i++) {
      pq = (POSMBUILDBLOCKQUERY) lBBQ.Get(i);

      for (j = 0; j < pq->plBBCLIPVOLUME->Num(); j++) {
         pcv = (PBBCLIPVOLUME) pq->plBBCLIPVOLUME->Get(j);
         if (pcv->pss)
            delete pcv->pss;
      }
      delete pq->plBBCLIPVOLUME;
   }

   // note that changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   // tell all the ground objects about the basement border
   CListFixed lTop, lBottom;
   lTop.Init (sizeof(CPoint));
   lBottom.Init (sizeof(CPoint));
   PCPoint ppTop, ppBottom;
   DWORD dwNumPoint;
   ppTop = ppBottom = NULL;
   dwNumPoint = 0;
   if (m_dwFoundation == 4) {
      PCSpline pTop, pBottom;
      pTop = WallSplineAtHeight (0, TRUE);
      pBottom = WallSplineAtHeight (-1, TRUE);

      if (pTop && pBottom) for (i = 0; i < pTop->QueryNodes(); i++) {
         lTop.Add (pTop->LocationGet(i));
         lBottom.Add (pBottom->LocationGet(i));
      }

      if (pTop)
         delete pTop;
      if (pBottom)
         delete pBottom;

      // convert to world space
      dwNumPoint = lTop.Num();
      ppTop = (PCPoint) lTop.Get(0);
      ppBottom = (PCPoint) lBottom.Get(0);
      for (i = 0; i < dwNumPoint; i++) {
         ppTop[i].MultiplyLeft (&m_MatrixObject);
         ppBottom[i].MultiplyLeft (&m_MatrixObject);
      }
      
   }

   // extend the balustrades to roof lines
   BalustradeExtendToRoof();


   // Could be picky and find only the ground objects close, by but might
   // as well tell them all
   OSMGROUNDCUTOUT gc;
   memset (&gc, 0, sizeof(gc));
   gc.dwNum = dwNumPoint;
   gc.gBuildBlock = m_gGUID;
   gc.paBottom = ppBottom;
   gc.paTop = ppTop;
   for (i = 0; i < m_pWorld->ObjectNum(); i++) {
      PCObjectSocket pos = m_pWorld->ObjectGet(i);
      pos->Message (OSM_GROUNDCUTOUT, &gc);
   }

   return TRUE;
}

/**********************************************************************************
CObjectBuildBlock::
Called to tell the container that one of its contained objects has been renamed.
*/
BOOL CObjectBuildBlock::ContEmbeddedRenamed (const GUID *pgOld, const GUID *pgNew)
{
   // cycle through all the surfaces asking them this question
   DWORD i;
   PBBSURF pbbs;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);

      // call this several times because need to make sure all bits are hit
      pbbs->pSurface->ContEmbeddedRenamed (pgOld, pgNew);
   }
   return FALSE;
}

/**********************************************************************************
CObjectBuildBlock::VerifyObjectMatrix - Verifies that the object matrix's floor
is one of the floor levels. If not, it calls object matrix set and changes
it.
*/
void CObjectBuildBlock::VerifyObjectMatrix (void)
{
   CMatrix m;
   CPoint zero;
   zero.Zero();
   ObjectMatrixGet (&m);
   zero.MultiplyLeft (&m);

   // loop through current elevations and find
   DWORD i, dwClosest;
   fp fClosest, fDist;
   dwClosest = -1;
   fClosest = 0;
   // ignore the basement
   for (i = 1; i < 100; i++) {   // arbitrary max of 100 floors
      if (i < NUMLEVELS)
         fDist = zero.p[2] - m_fLevelElevation[i];
      else {
         if (zero.p[2] <= m_fLevelElevation[NUMLEVELS-1])
            break;
         fDist = zero.p[2] - (m_fLevelElevation[NUMLEVELS-1] + (i-NUMLEVELS+1) * m_fLevelHigher);
      }
      if ((dwClosest == -1) || (fabs(fDist) < fabs(fClosest))) {
         dwClosest = i;
         fClosest = fDist;
      }
   }

   // if the closest distance is small then accept as ok
   if (fabs(fClosest) < .001)
      return;

   // else, translate and call again
   CMatrix mTrans;
   mTrans.Translation (0, 0, -fClosest);
   mTrans.MultiplyLeft (&m);
   CObjectTemplate::ObjectMatrixSet (&mTrans);
      //NOTE: Specifically calling the template's function since if called our own
      // would get into a bit of a recursive loop
}


/**********************************************************************************
CObjectBuildBlock::AdjustHeightToFloor - Adjust the height, m_fHeight, to the nearest
floor. (NOTE: This doesnt call ObjectAboutToChanged() and ObjectChnged());

returns
   BOOL - TRUE if changed the value
*/
BOOL CObjectBuildBlock::AdjustHeightToFloor (void)
{
   if (!m_fRestrictHeight)
      return FALSE;


   // convert height into world space
   CPoint zero, pHeight;
   zero.Zero();
   pHeight.Zero();
   pHeight.p[2] = m_fHeight;
   zero.MultiplyLeft (&m_MatrixObject);
   pHeight.MultiplyLeft (&m_MatrixObject);

   // loop through current elevations and find
   DWORD i, dwClosest;
   fp fClosest, fDist;
   dwClosest = -1;
   fClosest = 0;
   // ignore the basement
   for (i = 1; i < 100; i++) {   // arbitrary max of 100 floors
      fp fElev;

      if (i < NUMLEVELS)
         fElev = m_fLevelElevation[i];
      else {
         fElev = (m_fLevelElevation[NUMLEVELS-1] + (i-NUMLEVELS+1) * m_fLevelHigher);
      }

      // if this is less that zero then continue
      if (fElev <= zero.p[2])
         continue;

      // find the distance
      fDist = pHeight.p[2] - fElev;

      if ((dwClosest == -1) || (fabs(fDist) < fabs(fClosest))) {
         dwClosest = i;
         fClosest = fDist;
      }
      else if ((dwClosest != -1) && (fDist < 0) && (fabs(fDist) > fabs(fClosest)))
         break;   // gone too far
   }
   if (dwClosest == -1)
      return FALSE;

   if (fabs(fClosest) < EPSILON)
      return FALSE;  // no change

   // else change the height
   m_fHeight -= fClosest;

   if (m_dwRoofType == ROOFSHAPE_NONE)
      m_fHeight += .5;  // to compensate

   return TRUE;
}

/**********************************************************************************
CObjectBuildBlock::ObjectMatrixSet - Capture this to make sure the lowest level
starts at a floor.
*/
BOOL CObjectBuildBlock::ObjectMatrixSet (CMatrix *pObject)
{
   // get the old height of floor
   CPoint   pOldFloor;
   pOldFloor.Zero();
   pOldFloor.MultiplyLeft (&m_MatrixObject);

   if (!CObjectTemplate::ObjectMatrixSet (pObject))
      return FALSE;

   // if no restrictions on elevation then dont care
   if (m_fRestrictElevation) {
      VerifyObjectMatrix ();


      // if didn't change height, don't care. Can translate in X or Y, or rotate
      // so remember old
      CPoint pNew;
      pNew.Zero();
      pNew.MultiplyLeft (&m_MatrixObject);
      if (fabs(pOldFloor.p[2] - pNew.p[2]) > .01)
         AdjustAllSurfaces(); // Do I want to do this here? I think I need to.
      m_pOldOMSPoint.Copy (&pNew);
   }

   // see about the height now
   if (AdjustHeightToFloor ()) {
      AdjustAllSurfaces ();
   }

   return TRUE;
}

/**************************************************************************************
CObjectBuildBlock::WorldSetFinished - So object knows that has been moved and is finished
being moved.

  Called after every WorldSet() call. This is called to tell the objects
  they're in the new world and all their embedded objects are in the new
  world so they may want to do some processing
*/
void CObjectBuildBlock::WorldSetFinished (void)
{
   if (!m_fLevelGlobal || !m_fRestrictElevation)
      return;

   if (!GlobalFloorLevelsGet(m_pWorld, this, m_fLevelElevation, &m_fLevelHigher))
      return;  // nothing changed

   // FUTURERELEASE - Deal with moving floor levels up and down, moving embedded objects up and down

   // rearrange some stuff
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // realign vertically so ground floor ok
   VerifyObjectMatrix ();
   AdjustHeightToFloor ();

   AdjustAllSurfaces();
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);
}

/**************************************************************************************
CObjectBuildBlock::ToClippingVolume - Takes the building block and generates a
clipping volume out of it.

inputs
   PCListFixed       plBBCV - Initialized to sizeof(BBCLIPVOLUME) by this
                     call. Filled in with a list of spline surfaces. These spline
                     surfaces must be freed by the caller.
                     NOTE: Initialized by this function
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBuildBlock::ToClippingVolume (PCListFixed plBBCV)
{
   plBBCV->Init (sizeof(BBCLIPVOLUME));

   // amount to extend
   fp fExtend;
   fExtend = max(m_fBasementThickSideA + m_fBasementThickSideB + m_fBasementThickStruct,
      m_fFloorThickSideA + m_fFloorThickSideB + m_fFloorThickStruct);
   fExtend = max(fExtend, m_fRoofThickSideA + m_fRoofThickSideB + m_fRoofThickStruct);
   fExtend = max(fExtend, m_fWallThickSideA + m_fWallThickSideB + m_fWallThickStruct);

   DWORD i;
   PBBSURF pbbs;
   BBCLIPVOLUME cv;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->fDontIncludeInVolume)
         continue;    // dont want this

      // if there aren't any walls then dont bother adding, roof, floor, or anything
      // although may add verandah later
      if (m_dwWallType == WALLSHAPE_NONE)
         continue;


      memset (&cv, 0, sizeof(cv));
      switch (pbbs->dwMajor) {
      case BBSURFMAJOR_ROOF:
         cv.dwType = 1;
         break;
      case BBSURFMAJOR_FLOOR:
         cv.dwType = 2;
         break;
      case BBSURFMAJOR_TOPCEILING:
         //cv.dwType = 3;
         continue;   // dont incorporate this
         break;
      case BBSURFMAJOR_VERANDAH:
         // dont add verandahs
         continue;
      default:
      case BBSURFMAJOR_WALL:
         cv.dwType = 0;
         break;
      }
      pbbs->pSurface->MatrixGet (&cv.mToWorld);
      cv.mToWorld.MultiplyRight (&m_MatrixObject);

      PCSplineSurface pOrig, pNew;
      BOOL  fFlip;

      // make side a and clone it, and then side b
      int iSide;
      for (iSide = -1; iSide <= 1; iSide++) {
         fFlip = FALSE;
         cv.iSide = iSide;
         if (iSide == -1) {
            pOrig = (pbbs->dwMajor == BBSURFMAJOR_FLOOR) ? &pbbs->pSurface->m_SplineA : &pbbs->pSurface->m_SplineB;
         }
         else if (iSide == 1) {
            pOrig = (pbbs->dwMajor == BBSURFMAJOR_FLOOR) ? &pbbs->pSurface->m_SplineB : &pbbs->pSurface->m_SplineA;
         }
         else
            pOrig = &pbbs->pSurface->m_SplineCenter;

         fp fExtendL, fExtendR, fExtendU, fExtendD;
         fExtendL = fExtendR = fExtendU = fExtendD = 0;
         if (pbbs->dwMajor == BBSURFMAJOR_WALL)
            fExtendD = fExtend;  // extend walls down, but never up - or causes problems
               // with floors hiting both the basement center and main wall center
         if (pbbs->dwMajor == BBSURFMAJOR_FLOOR) {
            fExtendU = fExtendD = fExtend;
            fExtendL = fExtendR = fExtend;
         }
         // NOTE: Never extend the roof


         // BUGFIX - Took this out since walls weren't extend down enough to clip against floor
         //if ((pbbs->dwMajor != BBSURFMAJOR_FLOOR) && (pbbs->dwMajor != BBSURFMAJOR_TOPCEILING)) {
            // BUGFIX - Dont extend because the extension theory of clipping only
            // works if the planes are perfectly flat. Warped planes or non-linear
            // dont work, so use the other solution of extending lines when clipped
         //   fExtendUD = fExtendLR = 0;
         //}

         // if it's a roof then don't remove the cutouts since did rather a large
         // amount of work to get it working
         pNew = pOrig->CloneAndExtend (fExtendL, fExtendR, fExtendU, fExtendD, fFlip,
            !(pbbs->dwMajor == BBSURFMAJOR_ROOF));
         if (!pNew)
            continue;

         // if it's the roof, remove all cutouts except for trim roof
         if (pbbs->dwMajor == BBSURFMAJOR_ROOF) {
            DWORD i;
            PWSTR pszName;
            PTEXTUREPOINT pt;
            DWORD dwNum;
            BOOL fClock;
            for (i = pNew->CutoutNum()-1; i < pNew->CutoutNum(); i--) {
               if (!pNew->CutoutEnum (i, &pszName, &pt, &dwNum, &fClock))
                  continue;
               if (!_wcsicmp(pszName, gpszTrimRoof))
                  continue;

               // else, remove
               pNew->CutoutRemove (pszName);
            }
         }


         cv.pss = pNew;
         plBBCV->Add (&cv);
      }

   } // over all surf

   // loop through all the sides again, this time intersecting them with all
   // other relevant sides, creating a clipping area, and setting that.
   CListFixed lSSOther, lSSRoof;   // list of PCSplineSurface
   CListFixed lMatrixOther, lMatrixRoof;  // list of matrices
   PBBCLIPVOLUME pcv1, pcv2;
   lSSRoof.Init (sizeof(PCSplineSurface));
   lMatrixRoof.Init (sizeof(CMatrix));
   lSSOther.Init (sizeof(PCSplineSurface));
   lMatrixOther.Init (sizeof(CMatrix));
   CMatrix m1, m2;
   DWORD j;
   DWORD dwPass;
   for (dwPass = 0; dwPass < 2; dwPass++) {
      for (i = 0; i < plBBCV->Num(); i++) {
         pcv1 = (PBBCLIPVOLUME) plBBCV->Get(i);

         // don't do the roofs in pass 1, do them in pass 2
         switch (dwPass) {
         case 0:
            if (pcv1->dwType == 1)
               continue;
            break;
         case 1:
            if (pcv1->dwType != 1)
               continue;
            break;
         }
      
         // invert the matrix so can convert from world space to this spline's coords
         pcv1->mToWorld.Invert4 (&m1);

         // add them?
         lSSRoof.Clear();
         lMatrixRoof.Clear();
         lSSOther.Clear();
         lMatrixOther.Clear();
         for (j = 0; j < plBBCV->Num(); j++) {
            if (i == j)
               continue;
            pcv2 = (PBBCLIPVOLUME) plBBCV->Get(j);

            // if not the same side then ignore
            if (pcv1->iSide != pcv2->iSide)
               continue;

            switch (pcv1->dwType) {
            default:
            case 0:  // wall
               // only clip walls against roof - doesn't really matter with floor
               if (pcv2->dwType != 1)
                  continue;
               // only clip walls against roof and floor
               //if ((pcv2->dwType != 1) && (pcv2->dwType != 2))
               //   continue;
               break;
            case 1:  // roof
               // only clip roof against walls and roof
               if (pcv2->dwType != 0)
                  continue;
               break;
            case 2:  // floor
               // only clip floors against walls and roof
               // only clip roof against walls and other roof
               if ((pcv2->dwType != 1) && (pcv2->dwType != 0))
                  continue;
               break;
            }

            // if get here add it
            m2.Multiply (&m1, &pcv2->mToWorld);

            if (pcv2->dwType == 1) {
               // roofs are special because sometimes they bend in one one
               // another so can't use the un-clipped version
               lSSRoof.Add (&pcv2->pss);
               lMatrixRoof.Add (&m2);
            }
            else {
               lSSOther.Add (&pcv2->pss);
               lMatrixOther.Add (&m2);
            }
         } // see which to add

         BOOL fClockwise;
         fClockwise = TRUE;
         if (pcv1->iSide == -1)  //(!pcv1->fExternal)
            fClockwise = !fClockwise;
         //if ((pcv1->dwType == 1) || (pcv1->dwType == 2))
         //   fClockwise = !fClockwise;

         // if it's a floor, take the largest size possible. Otherwise take smallest.
         BOOL fTakeLargest;
         fTakeLargest = FALSE;
         // BUGFIX - Even with floor take smallest
         //fTakeLargest = (pcv1->dwType == 2);

         // use the clipping routines
         pcv1->pss->IntersectWithSurfacesAndCutout ((PCSplineSurface*) lSSOther.Get(0),
            (PCMatrix) lMatrixOther.Get(0), lSSOther.Num(), L"OtherClipVolume", TRUE, fClockwise,
            0, !fTakeLargest, FALSE, NULL);
         pcv1->pss->IntersectWithSurfacesAndCutout ((PCSplineSurface*) lSSRoof.Get(0),
            (PCMatrix) lMatrixRoof.Get(0), lSSRoof.Num(), L"RoofClipVolume", FALSE, fClockwise,
            0, !fTakeLargest, FALSE, NULL);
      } // over all splines
   }

   // make the verandah cutout
   if (m_dwVerandahType != WALLSHAPE_NONE) {
      CMem memPoints, memCurve;
      PCPoint paPoints;
      DWORD *padwCurve;
      DWORD dwNum = m_sVerandah.OrigNumPointsGet();
      if (!memPoints.Required (dwNum * 2 * sizeof(CPoint)))
         goto doneverandah;
      if (!memCurve.Required (dwNum * sizeof(DWORD)))
         goto doneverandah;
      paPoints = (PCPoint) memPoints.p;
      padwCurve = (DWORD*) memCurve.p;

      for (i = 0; i < dwNum; i++) {
         m_sVerandah.OrigSegCurveGet (i, padwCurve + i);
         
         // note - flipped vertical/horizontal to what normally do so that
         // outside of surface is pointing away from house
         WallCornerAtHeight (i, m_fRoofHeight + 2*fExtend, paPoints + (i+dwNum), FALSE);
         WallCornerAtHeight (i, 0 - 2*fExtend, paPoints + (i + 0), FALSE);
      }

      // min and max
      DWORD dwMin, dwMax;
      fp fDetail;
      m_sVerandah.DivideGet (&dwMin, &dwMax, &fDetail);

      // make it
      PCSplineSurface pss;
      pss = new CSplineSurface;
      if (!pss)
         goto doneverandah;
      pss->DetailSet (dwMin, dwMax, 0, 0, fDetail);
      pss->ControlPointsSet (TRUE, FALSE, dwNum, 2, paPoints, padwCurve, (DWORD*)SEGCURVE_LINEAR);
      
      // write it
      memset (&cv, 0, sizeof(cv));
      cv.dwType = 4;
      cv.iSide = 0;
      cv.mToWorld.Copy (&m_MatrixObject);
      cv.pss = pss;
      plBBCV->Add (&cv);
   }
doneverandah:

   // done
   return TRUE;
}

/*************************************************************************************
CObjectBuildBlock::Deconstruct -
Tells the object to deconstruct itself into sub-objects.
Basically, new objects will be added that exactly mimic this object,
and any embedeeding objects will be moved to the new ones.
NOTE: This old one will not be deleted - the called of Deconstruct()
will need to call Delete()
If fAct is FALSE the function is just a query, that returns
TRUE if the object cares about adjustment and can try, FALSE if it can't.

NOTE: Often overridden.
*/
BOOL CObjectBuildBlock::Deconstruct (BOOL fAct)
{
   if (!m_pWorld)
      return FALSE;
   if (!fAct)
      return TRUE;

   // notify world of object changing
   m_pWorld->ObjectAboutToChange (this);

   // loop through all the surfaces... create them and clone them
   DWORD i, j;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF ps = (PBBSURF) m_listBBSURF.Get(i);

      // dont do hidden surfaces
      if (ps->fHidden)
         continue;

      // create a new one
      PCObjectStructSurface pss;
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_StructSurface;
      OI.gSub = CLSID_StructSurfaceExternalStudWall;
      OI.dwRenderShard = m_OSINFO.dwRenderShard;
      pss = new CObjectStructSurface((PVOID) 0x1000, &OI);
      if (!pss)
         continue;

      // BUGFIX - Set a name
      pss->StringSet (OSSTRING_NAME, L"Surface from building block");

      m_pWorld->ObjectAdd (pss);

      m_pWorld->ObjectAboutToChange (pss);

      // remove any colors or links in the new object
      while (TRUE) {
         j = pss->ContainerSurfaceGetIndex (0);
         if (j == (DWORD)-1)
            break;
         pss->ContainerSurfaceRemove (j);
      }
      while (TRUE) {
         j = pss->ObjectSurfaceGetIndex (0);
         if (j == (DWORD)-1)
            break;
         pss->ObjectSurfaceRemove (j);
      }

      // clone the surfaces
      ps->pSurface->ClaimCloneTo (&pss->m_ds, pss);

      // imbue it with new information
      ps->pSurface->CloneTo (&pss->m_ds, pss);

      // set the matrix
      CMatrix m;
      m.Identity();
      pss->m_ds.MatrixSet (&m);
      ps->pSurface->MatrixGet(&m);
      m.MultiplyRight (&m_MatrixObject);
      pss->ObjectMatrixSet (&m);

      // move embedded objects over that are embedded in this one
      while (TRUE) {
         for (j = 0; j < ContEmbeddedNum(); j++) {
            GUID gEmbed;
            TEXTUREPOINT pHV;
            fp fRotation;
            DWORD dwSurface;
            if (!ContEmbeddedEnum (j, &gEmbed))
               continue;
            if (!ContEmbeddedLocationGet (&gEmbed, &pHV, &fRotation, &dwSurface))
               continue;

            if (!ps->pSurface->ClaimFindByID (dwSurface))
               continue;

            // else, it's on this surface, so remove it from this object and
            // move it to the new one
            ContEmbeddedRemove (&gEmbed);
            pss->ContEmbeddedAdd (&gEmbed, &pHV, fRotation, dwSurface);

            // set j=0 just to make sure repeat
            j = 0;
            break;
         }

         // keep repeating until no more objects embedded in this surface
         if (j >= ContEmbeddedNum())
            break;
      }

      // eventually need to shorten walls if clones walls are clipped
      // and dont use extra
      pss->m_ds.ShrinkToCutouts (FALSE);

      // note changed
      m_pWorld->ObjectChanged (pss);
   }

   // Do the balustrades
   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      PBBBALUSTRADE ps = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get(i);

      // make sure exists
      if (ps->pBal) {
         // create a new one
         PCObjectBalustrade pss;
         OSINFO OI;
         memset (&OI, 0, sizeof(OI));
         OI.gCode = CLSID_Balustrade;
         OI.gSub = CLSID_BalustradeBalVertWood;
         OI.dwRenderShard = m_OSINFO.dwRenderShard;
         pss = new CObjectBalustrade((PVOID)BS_BALVERTWOOD, &OI);  // need one style just to make sure can recreate (PVOID) ps->pBal->m_dwBalStyle);
         if (!pss)
            continue;

         // BUGFIX - Set a name
         pss->StringSet (OSSTRING_NAME, L"Balustrade from building block");

         m_pWorld->ObjectAdd (pss);

         m_pWorld->ObjectAboutToChange (pss);

         // remove any colors or links in the new object
         pss->m_ds.ClaimClear();

         // clone the surfaces
         ps->pBal->ClaimCloneTo (&pss->m_ds, pss);

         // imbue it with new information
         ps->pBal->CloneTo (&pss->m_ds, pss);

         // make sure looking at drag points
         pss->m_ds.m_dwDisplayControl = 0;

         // set the matrix
         pss->ObjectMatrixSet (&m_MatrixObject);

         // note changed
         m_pWorld->ObjectChanged (pss);
      }
      else if (ps->pPiers) {
         // create a new one
         PCObjectPiers pss;
         OSINFO OI;
         memset (&OI, 0, sizeof(OI));
         OI.gCode = CLSID_Piers;
         OI.gSub = CLSID_PiersSteel;
         OI.dwRenderShard = m_OSINFO.dwRenderShard;
         pss = new CObjectPiers((PVOID)PS_STEEL, &OI);  // need one style just to make sure can recreate (PVOID) ps->pBal->m_dwBalStyle);
         if (!pss)
            continue;

         // BUGFIX - Set a name
         pss->StringSet (OSSTRING_NAME, L"Piers from building block");

         m_pWorld->ObjectAdd (pss);

         m_pWorld->ObjectAboutToChange (pss);

         // remove any colors or links in the new object
         pss->m_ds.ClaimClear();

         // clone the surfaces
         ps->pPiers->ClaimCloneTo (&pss->m_ds, pss);

         // imbue it with new information
         ps->pPiers->CloneTo (&pss->m_ds, pss);

         // make sure looking at drag points
         pss->m_ds.m_dwDisplayControl = 0;

         // set the matrix
         pss->ObjectMatrixSet (&m_MatrixObject);

         // note changed
         m_pWorld->ObjectChanged (pss);
      }
   }


   m_pWorld->ObjectChanged (this);
   return TRUE;
}

/**********************************************************************************
CObjectBuildBlock::WishListAdd - Adds an item (wall, roof, floor) to the wish
list of objects that are to be used in the building block. What this does is:
   1) If the object doesn't exist already it's created.
   2) All settings in the object are changed based on the wish list.

NOTE: Need to do the following before using:
   a) clear m_listWish
   b) Set m_fClosestHeight.

inputs
   PBBSURF        pSurf - What surface is desired
returns
   PBBSURF - Surface as is appears in the final list. This includes a pointer to
      the CDoubleSurface object
*/
PBBSURF CObjectBuildBlock::WishListAdd (PBBSURF pSurf)
{
   BOOL fChanged;
   fChanged = FALSE;

   // create any that don't exist but which should
   PBBSURF pLookFor, pLookAt;
   DWORD j;
   BBSURF bbs;
   pLookFor = pSurf;
   for (j = 0; j < m_listBBSURF.Num(); j++) {
      pLookAt = (PBBSURF) m_listBBSURF.Get(j);
      if ((pLookFor->dwMajor == pLookAt->dwMajor) && (pLookFor->dwMinor == pLookAt->dwMinor))
         break;
   }
   if (j >= m_listBBSURF.Num()) {
      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }

      // couldn't find, so add

      // copy over, but set some values so that the object will be properly
      // moved
      bbs = *pLookFor;
      bbs.fHeight = -1;
      bbs.fRotX = 0;
      bbs.fRotZ = 0;
      bbs.fRotY = 0;
      bbs.fWidth = -1;
      bbs.pTrans.Zero();
      bbs.fBevelBottom = bbs.fBevelTop = bbs.fBevelLeft = bbs.fBevelRight = 0;
      
      // create
      bbs.pSurface = new CDoubleSurface;
      if (!bbs.pSurface)
         return NULL;   // error
      bbs.pSurface->Init (m_OSINFO.dwRenderShard, bbs.dwType, this);
      
      // add it
      pLookAt = (PBBSURF) m_listBBSURF.Get(m_listBBSURF.Add (&bbs));
      if (!pLookAt)
         return NULL;   // error
      pLookAt->fChanged = TRUE;
   }
   else
      pLookAt->fChanged = FALSE;

   // remember the surface in the wish list since use this later
   pSurf->pSurface = pLookAt->pSurface;
   // add to wish list
   m_listWish.Add (pSurf);


   // if get here, pLookAt is filled with what's really there, and pLookFor
   // is what we want. If these differ then fix the problem

   // remember the setting for not including in volume
   // Do this, because if first create as perimiter (which will have as TRUE) and 
   // convert to basement (which will have as false) then doesn't use basement walls.
   pLookAt->fDontIncludeInVolume = pLookFor->fDontIncludeInVolume;
   pLookAt->fKeepClip = pLookFor->fKeepClip;
   pLookAt->pKeepA = pLookFor->pKeepA;
   pLookAt->pKeepB = pLookFor->pKeepB;
   pLookAt->fKeepLargest = pLookFor->fKeepLargest;
   pLookAt->dwFlags = pLookFor->dwFlags;

   // if the curvature is non-linear then need to resize anyway
   BOOL fNonLinear;
   fNonLinear = FALSE;
   DWORD i;
   for (i = 0; i < pLookAt->pSurface->ControlNumGet(TRUE)-2; i++)
      if (pLookAt->pSurface->SegCurveGet(TRUE, i) != SEGCURVE_LINEAR)
         fNonLinear = TRUE;
   for (i = 0; i < pLookAt->pSurface->ControlNumGet(FALSE)-2; i++)
      if (pLookAt->pSurface->SegCurveGet(FALSE, i) != SEGCURVE_LINEAR)
         fNonLinear = TRUE;

   // If different sizes deal with that.
   if (fNonLinear || (pLookAt->fWidth != pLookFor->fWidth) || (pLookAt->fHeight != pLookFor->fHeight)) {
      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      // remember old height and width
      fp fOldHeight, fOldWidth;
      fOldHeight = pLookAt->fHeight;
      fOldWidth = pLookAt->fWidth;
      pLookAt->dwStretchHInfo = pLookFor->dwStretchHInfo; // copy over from the widh
      pLookAt->dwStretchVInfo = pLookFor->dwStretchVInfo;
      if ((fOldHeight < 0) || (fOldHeight == pLookFor->fHeight))
         pLookAt->dwStretchVInfo = 0;  // no change
      if ((fOldWidth < 0) || (fOldWidth == pLookFor->fWidth))
         pLookAt->dwStretchHInfo = 0;  // no change

      // adjust the width and height
      pLookAt->fWidth = pLookFor->fWidth;
      pLookAt->fHeight = pLookFor->fHeight;


      // If resize, what do with overlays and embedded objects since HV
      // will change for them?
      fp fHOrig, fVOrig, fHNew, fVNew;
      switch (pLookAt->dwStretchHInfo) {
      case 1:  // stretch/shrink on the left
         if (pLookAt->fWidth > fOldWidth) {
            // expanded it
            fHOrig = EPSILON;
            fHNew = 1 - fOldWidth / pLookAt->fWidth;
         }
         else {
            // shrunk it
            fHOrig = 1 - pLookAt->fWidth / fOldWidth;
            fHNew = EPSILON;
         }
         break;

      case 2:  // stretch/shrink on the right
         if (pLookAt->fWidth > fOldWidth) {
            // expanded it
            fHOrig = 1 - EPSILON;
            fHNew = fOldWidth / pLookAt->fWidth;
         }
         else {
            // shrunk it
            fHOrig = pLookAt->fWidth / fOldWidth;
            fHNew = 1 - EPSILON;
         }
         break;
      }

      switch (pLookAt->dwStretchVInfo) {
      case 1:  // stretch/shrink on the top
         if (pLookAt->fHeight > fOldHeight) {
            // expanded it
            fVOrig = EPSILON;
            fVNew = 1 - fOldHeight / pLookAt->fHeight;
         }
         else {
            // shrunk it
            fVOrig = 1 - pLookAt->fHeight / fOldHeight;
            fVNew = EPSILON;
         }
         break;

      case 2:  // stretch/shrink on the bottom
         if (pLookAt->fHeight > fOldHeight) {
            // expanded it
            fVOrig = 1 - EPSILON;
            fVNew = fOldHeight / pLookAt->fHeight;
         }
         else {
            // shrunk it
            fVOrig = pLookAt->fHeight / fOldHeight;
            fVNew = 1 - EPSILON;
         }
         break;
      }


      pLookAt->pSurface->NewSize (pLookAt->fWidth, pLookAt->fHeight,
            pLookAt->dwStretchHInfo ? 1 : 0,
            pLookAt->dwStretchVInfo ? 1 : 0,
            &fHOrig, &fVOrig, &fHNew, &fVNew);
      // Because called NewSize() all the embedded objects have already been
      // told that they've moved

   }

   // If different rotation than deal with that
   if ((pLookAt->pTrans.p[0] != pLookFor->pTrans.p[0]) || (pLookAt->pTrans.p[1] != pLookFor->pTrans.p[1]) ||
      (pLookAt->pTrans.p[2] != pLookFor->pTrans.p[2]) || (pLookAt->fRotX != pLookFor->fRotX) ||
      (pLookAt->fRotZ != pLookFor->fRotZ) || (pLookAt->fRotY != pLookFor->fRotY)) {

      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      pLookAt->pTrans.Copy (&pLookFor->pTrans);
      pLookAt->fRotX = pLookFor->fRotX;
      pLookAt->fRotY = pLookFor->fRotY;
      pLookAt->fRotZ = pLookFor->fRotZ;

      CMatrix m;
      m.FromXYZLLT (&pLookAt->pTrans, pLookAt->fRotZ, pLookAt->fRotX, pLookAt->fRotY);
      pLookAt->pSurface->MatrixSet (&m);
      //CMatrix mRotX, mRotZ, mTrans;
      //mRotX.RotationX (pLookAt->fRotX);
      //mRotZ.RotationZ (pLookAt->fRotZ);
      //mTrans.Translation (pLookAt->pTrans.p[0], pLookAt->pTrans.p[1], pLookAt->pTrans.p[2]);
      //mTrans.MultiplyLeft (&mRotZ);
      //mTrans.MultiplyLeft (&mRotX);

      //pLookAt->pSurface->MatrixSet (&mTrans);

      // Because called MatrixSet, all the embedded objects are already
      // told that they've moved
   }

   // If different bevelling or thickness then deal with that
   if ((pLookFor->fBevelTop != pLookAt->pSurface->m_fBevelTop) ||
      (pLookFor->fBevelBottom != pLookAt->pSurface->m_fBevelBottom) ||
      (pLookFor->fBevelLeft != pLookAt->pSurface->m_fBevelLeft) ||
      (pLookFor->fBevelRight != pLookAt->pSurface->m_fBevelRight) ||
      (pLookFor->fThickStud != pLookAt->pSurface->m_fThickStud) ||
      (pLookFor->fThickSideA != pLookAt->pSurface->m_fThickA) ||
      (pLookFor->fThickSideB != pLookAt->pSurface->m_fThickB) ||
      (pLookFor->fHideEdges != pLookAt->pSurface->m_fHideEdges)
      ) {


      // world about to change
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      pLookAt->pSurface->m_fBevelTop = pLookFor->fBevelTop;
      pLookAt->pSurface->m_fBevelBottom = pLookFor->fBevelBottom;
      pLookAt->pSurface->m_fBevelLeft = pLookFor->fBevelLeft;
      pLookAt->pSurface->m_fBevelRight = pLookFor->fBevelRight;

      pLookAt->pSurface->m_fThickStud = pLookFor->fThickStud;
      pLookAt->pSurface->m_fThickA = pLookFor->fThickSideA;
      pLookAt->pSurface->m_fThickB = pLookFor->fThickSideB;

      pLookAt->pSurface->m_fHideEdges = pLookFor->fHideEdges;

      pLookAt->pSurface->RecalcSides();
      pLookAt->pSurface->RecalcEdges();

      // Don't need to tell objects they've moved because automagically
      // done by RecalcSides()
   }

   // Hide or not hide
   if (pLookAt->fHidden != pLookFor->fHidden) {
      if (!fChanged && m_pWorld) {
         m_pWorld->ObjectAboutToChange (this);
         fChanged = TRUE;
      }
      pLookAt->fChanged = TRUE;

      pLookAt->fHidden = pLookFor->fHidden;
   }

   // find all the walls and figure out where the floors are
   CListFixed lFloors;
   lFloors.Init (sizeof(fp));
   if (pLookAt->dwMajor == BBSURFMAJOR_WALL) {
      DWORD dwSide;
      for (dwSide = 0; dwSide < 2; dwSide++) {  // BUGFIX - Was dwSide < 1, which I think is a typo
         // dwSide == 0 => sideA, dwSide==1 => sideb

         // what's the angle? Cheat by using the top indent
         fp fAngle;
         fAngle = pLookAt->pSurface->m_fBevelTop;

         // and how thick
         fp fThick;
         fThick = pLookAt->pSurface->m_fThickStud / 2 +
            ((dwSide == 0) ? pLookAt->pSurface->m_fThickA : pLookAt->pSurface->m_fThickB);

         // scale
         fp fScale, fOffset;
         fScale = 1.0 / cos(fAngle);
         fOffset = sin(fAngle) * fThick;
         if (dwSide == 1)
            fOffset *= -1;

         lFloors.Clear();
         for (j = 0; j < 100; j++) {
            fp fHeight;
            if (j < NUMLEVELS)
               fHeight = m_fLevelElevation[j];
            else
               fHeight = m_fLevelElevation[NUMLEVELS-1] + (j-NUMLEVELS+1) * m_fLevelHigher;
            fHeight -= m_fClosestHeight; // so get into dimensions of floors

            // if below the wall the continue
            fHeight -= pLookAt->pTrans.p[2];
            if (fHeight < 0)
               continue;

            // take wall angle into account
            fHeight += fOffset;
            fHeight *= fScale;

            // if above then stop right there
            fHeight /= pLookAt->fHeight;
            if (fHeight > 1)
               break;
            fHeight = 1.0 - fHeight;

            // add to the list
            lFloors.Add (&fHeight);
         }

         OSMSETFLOORS sf;
         PCLAIMSURFACE pcs;
         memset (&sf, 0, sizeof(sf));
         sf.dwNum = lFloors.Num();
         sf.pafV = (fp*) lFloors.Get(0);

         // set for side
         pcs = pLookAt->pSurface->ClaimFindByReason (dwSide);
         if (pcs) {
            sf.dwSurface = pcs->dwID;
            pLookAt->pSurface->Message (OSM_SETFLOORS, &sf);
         }
      }
   }

   // clear out the surface of bounding box clipping that had done
   ClipClear (pLookAt->pSurface);

   if (fChanged && m_pWorld) {
      m_pWorld->ObjectChanged (this);
   }

   return pLookAt;
}


/**********************************************************************************
ExtendLine - Extends a linear section from the line formed by p1 and p2.

inputs
   PCPoint     p1,p2 - Sequence that defines line
   fp      fExtend - Amount to extend by
   PCPoint     pExtend - Filled with the extension of p2 by fExtend meters.
retursn
   BOOL - TRUE if succcess
*/
static BOOL ExtendLine (PCPoint p1, PCPoint p2, fp fExtend, PCPoint pExtend)
{
   CPoint pDelta;
   pDelta.Subtract (p2, p1);
   pDelta.Normalize();
   pDelta.Scale (fExtend);
   pExtend->Add (p2, &pDelta);
   return TRUE;
}

/**********************************************************************************
ExtendCircle - Given three points that form a circle, p1..p3, this extends it
by fExtend (approximtely) meters. The p3 point is extended and written into
pExtend.

inputs
   PCPoint     p1,p2,p3 - Three sequenctial points that define a circle
   fp      fExtend - Number of meters (approximately) to extend by
   PCPoint     pExtend - Filled with the extended value for p3 (if returns true)
returns
   BOOL - TRUE if success, FALSE if failure
*/
static BOOL ExtendCircle (PCPoint p1, PCPoint p2, PCPoint p3, fp fExtend, PCPoint pExtend)
{
   // calcualte up
   CPoint pUp, pH, pV, pC;
   pC.Copy (p2);
   pH.Subtract (p3, &pC);
   pV.Subtract (p1, &pC);
   pUp.CrossProd (&pH, &pV);
   pUp.Normalize();
   pH.Normalize();
   pV.CrossProd (&pUp, &pH);

   // convert the three points into HV space
   CPoint t1, t2, t3;
   CPoint pTemp;
   t1.Zero();
   pTemp.Subtract (p1, &pC);
   t1.p[0] = pTemp.DotProd (&pH);
   t1.p[1] = pTemp.DotProd (&pV);
   t2.Zero();   // since p2 is used as the center point
   t3.Zero();
   pTemp.Subtract (p3, &pC);
   t3.p[0] = pTemp.DotProd (&pH);
   t3.p[1] = pTemp.DotProd (&pV);
   CPoint pHVUp;
   pHVUp.Zero();
   pHVUp.p[2] = 1;

   // create two lines, line 1 bisects p1 and p2, and line 2 biescts
   // p2 and p3. The slope of the lines is perpendicular to the
   // line p1->p2, and p2->p3. pLineP is the line anchor, pLineQ is the multiplied by alpha
   CPoint pLineP1, pLineP2, pLineQ1, pLineQ2;
   pLineP1.Subtract (&t1, &t2);
   pLineQ1.CrossProd (&pLineP1, &pHVUp);
   pLineP1.Add (&t1, &t2);
   pLineP1.Scale(.5);
   pLineQ1.Add (&pLineP1);

   // change of plans - treat the second one like a plane and intersect
   // the line with the plane, using algorithm already have
   pLineQ2.Subtract (&t3, &t2);  // normal to plane
   pLineP2.Add (&t3, &t2);
   pLineP2.Scale(.5);   // point in plane
   CPoint pIntersect;
   if (!IntersectLinePlane (&pLineP1, &pLineQ1, &pLineP2, &pLineQ2, &pIntersect)) {
      // if they don't intersect then dont have a circle, so pretend its a line
      return FALSE;
   }

   // calculate radius
   fp fRadius;
   pTemp.Subtract (&t2, &pIntersect);
   fRadius = pTemp.Length();
   if (fabs(fRadius) < EPSILON)
      return FALSE;

   // calculate the angle at the 2 points averaging
   // add two-pi so that know its a positive number
   fp fAngle1, fAngle2;
   fAngle1 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
   fAngle2 = atan2((t3.p[0] - pIntersect.p[0]), (t3.p[1] - pIntersect.p[1]));
   // below was used in case of segcurvenext
   // fAngle1 = atan2((t1.p[0] - pIntersect.p[0]), (t1.p[1] - pIntersect.p[1]));
   // fAngle2 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
   if (fAngle1 < fAngle2)
      fAngle1 += 2 * PI;

   // if the radius were 1, then the circmfrance would be 2 pi r - which means
   // that divide the extend amount by radius to get the amount of angle to change
   fExtend /= fRadius;

   fp fAngle;
   fAngle = fAngle2 - fExtend;

   // determine location in space
   fp h, v;
   h = sin (fAngle) * fRadius + pIntersect.p[0];
   v = cos (fAngle) * fRadius + pIntersect.p[1];
   CPoint pNew;
   pNew.Copy (&pH);
   pNew.Scale (h);
   pTemp.Copy (&pV);
   pTemp.Scale (v);
   pNew.Add (&pTemp);
   pNew.Add (&pC);

   pExtend->Copy (&pNew);
   return TRUE;
}

/**********************************************************************************
CObjectBuildBlock::WishListAddCurved - Adds a curved surface (sphere only) to the
wish list. It does this by pretending its a flat surface (using the edges) and calling
WishListAddFlat. Then, with what that returns it translates the other points and
creates a new surface with that. It's not too efficient, but will probably reduce the
number of bugs.

inputs
   PBBSURF        pSurf - What surface is desired
   DWORD          dwH, dwV - Number of control points horizontal and vertical
   PCPoint        papControl - An array of control points: Aranged as [0..dwV-1][0..dwH-1].
                     (Points in object space)
   DWORD          *padwSegCurveH, *padwSegCurveV - dwH-1 and dwV-1 elements. of SEGCURVE_XXX
   DWORD          dwFlags - Extra settings. See WishListAdd
returns
   PBBSURF - Surface as it appears in the final list. Like WishListAdd() return.
*/
#undef MESH
#define  MESH(x,y)   (papControl[(x)+(y)*(dwH)])
PBBSURF CObjectBuildBlock::WishListAddCurved (PBBSURF pSurf, DWORD dwH, DWORD dwV,
   PCPoint papControl, DWORD *padwSegCurveH, DWORD *padwSegCurveV, DWORD dwFlags)
{
   PBBSURF pAdd;
   DWORD i;
   pAdd = WishListAddFlat (pSurf, &MESH(0,dwV-1), &MESH(dwH-1,dwV-1), &MESH(0,0), &MESH(dwH-1,0),0);
   if (!pAdd)
      return NULL;

   PCDoubleSurface ps;
   ps = pAdd->pSurface;

   // keep flags indicating that flat
   ps->m_fConstAbove = FALSE;
   ps->m_fConstBottom = FALSE;
   ps->m_fConstRectangle = FALSE;

   // transform all the points into array
   CMem mem;
   if (!mem.Required(dwH*dwV*sizeof(CPoint)))
      return pAdd;
   memcpy (mem.p, papControl, dwH * dwV * sizeof(CPoint));
   papControl = (PCPoint) mem.p;
   CMatrix m, mInv;
   ps->MatrixGet(&m);
   m.Invert4 (&mInv);
   for (i = 0; i < dwH*dwV; i++) {
      papControl[i].p[3] = 1;
      papControl[i].MultiplyLeft (&mInv);
   }

   // Will need to extend
   fp fOverlap;
   fOverlap = (pSurf->fThickSideA + pSurf->fThickSideB + pSurf->fThickStud) * 2;
   if (dwFlags & 0x10)
      fOverlap *= 4;

   // left/right
   BOOL fRet;
   CPoint pTemp;
   for (i = 0; i < dwV; i++) {
      if (dwFlags & 0x01) {
         //extending left
         switch (padwSegCurveH[0]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(1,i), &MESH(0,i), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLENEXT:
            if (dwH >= 3)
               fRet = ExtendCircle (&MESH(2,i), &MESH(1,i), &MESH(0,i), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(1,i), &MESH(0,i), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(0,i).Copy (&pTemp);
      }
      if (dwFlags & 0x02) {
         //extending right
         switch (padwSegCurveH[dwH-2]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(dwH-2,i), &MESH(dwH-1,i), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLEPREV:
            if (dwH >= 3)
               fRet = ExtendCircle (&MESH(dwH-3,i), &MESH(dwH-2,i), &MESH(dwH-1,i), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(dwH-2,i), &MESH(dwH-1,i), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(dwH-1,i).Copy (&pTemp);
      }
   }

   for (i = 0; i < dwH; i++) {
      if (dwFlags & 0x04) {
         //extending top
         switch (padwSegCurveV[0]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(i,1), &MESH(i,0), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLENEXT:
            if (dwV >= 3)
               fRet = ExtendCircle (&MESH(i,2), &MESH(i,1), &MESH(i,0), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(i,1), &MESH(i,0), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(i,0).Copy (&pTemp);
      }
      if (dwFlags & 0x08) {
         //extending bottom
         switch (padwSegCurveV[dwV-2]) {
         case SEGCURVE_LINEAR:
            fRet = ExtendLine (&MESH(i,dwV-2), &MESH(i,dwV-1), fOverlap, &pTemp);
            break;
         case SEGCURVE_CIRCLEPREV:
            if (dwV >= 3)
               fRet = ExtendCircle (&MESH(i,dwV-3), &MESH(i,dwV-2), &MESH(i,dwV-1), fOverlap, &pTemp);
            else
               fRet = ExtendLine (&MESH(i,dwV-2), &MESH(i,dwV-1), fOverlap, &pTemp);
            break;
         default:
            fRet = FALSE;
         }
         if (fRet)
            MESH(i, dwV-1).Copy (&pTemp);
      }
   }

   // write them out
   ps->ControlPointsSet (dwH, dwV, papControl, padwSegCurveH, padwSegCurveV, 3);

   return pAdd;
}

/**********************************************************************************
CObjectBuildBlock::WishListAddFlat - Adds an item (wall, roof, floor) to wish
list like in WishListAdd(). The only difference is that this takes 2 base points (bottom
of roof) and 1 or 2 upper points (peak of roof). It then calculates the rotation and size4
based on that and stores them in pSurf. It calls WishListAdd. If there's any bend
in the surface then the object is modified to account for the bend.

inputs
   PBBSURF        pSurf - What surface is desired
   PCPoint        pBase1, pBase2 - Two base points
   PCPoint        pTop1, pTop2 - pTop2 can be null
   DWORD          dwFlags - Extra settings - one or more...
                        0x01 - Extend left sides by a surface thickness to ensure overlap
                        0x02 - Extenr right side
                        0x04 - Extend top side by a surface thickness to ensure overlap
                        0x08 - Extend bottom down down
                        0x10 - Make the extension 4x as large on either side
                        0x20 - Don't warp. No matter what, once the flat surface is created, return
returns
   PBBSURF - Surface as it appears in the final list. Like WishListAdd() return.
*/
PBBSURF CObjectBuildBlock::WishListAddFlat (PBBSURF pSurf, PCPoint pBase1, PCPoint pBase2,
                                            PCPoint pTop1, PCPoint pTop2, DWORD dwFlags)
{
   // calculate the size and angle based upon the base

   // three points for a plane. Two are the bottom, the third is either the only
   // top point or the average
   CPoint pPlane;
   if (pTop2) {
      pPlane.Add (pTop1, pTop2);
      pPlane.Scale (.5);
   }
   else
      pPlane.Copy (pTop1);

   // figure out the three vectors. pA gets translated to X, pB to Y, and pC to Z
   CPoint pA, pB, pC;
   pA.Subtract (pBase2, pBase1);
   pA.Normalize();
   pC.Subtract (&pPlane, pBase1);
   pB.CrossProd (&pC, &pA);
   pB.Normalize();
   pC.CrossProd (&pA, &pB);
   pC.Normalize();

   // make a matrix that converts from spline to object space
   CMatrix mSplineToObject;
   mSplineToObject.RotationFromVectors (&pA, &pB, &pC);

   // now, make a matrix that converts from object space to spline
   CMatrix mObjectToSpline;
   mSplineToObject.Invert (&mObjectToSpline);

   // convert the 4 points
   CPoint pB1, pB2, pT1, pT2;
   pB1.Copy (pBase1);
   pB1.p[3] = 1;
   pB1.MultiplyLeft (&mObjectToSpline);
   pB2.Copy (pBase2);
   pB2.p[3] = 1;
   pB2.MultiplyLeft (&mObjectToSpline);
   pT1.Copy (pTop1);
   pT1.p[3] = 1;
   pT1.MultiplyLeft (&mObjectToSpline);
   if (pTop2) {
      pT2.Copy (pTop2);
      pT2.p[3] = 1;
      pT2.MultiplyLeft (&mObjectToSpline);
   }

   // figure out the min/max in x and Z
   CPoint pMin, pMax;
   pMin.Copy (&pB1);
   pMax.Copy (&pB1);
   pMin.Min (&pB2);
   pMax.Max (&pB2);
   pMin.Min (&pT1);
   pMax.Max (&pT1);
   if (pTop2) {
      pMin.Min (&pT2);
      pMax.Max (&pT2);
   }

   // overlap
   fp fOverlap;
   fOverlap = (pSurf->fThickSideA + pSurf->fThickSideB + pSurf->fThickStud) * 2;
   if (dwFlags & 0x10)
      fOverlap *= 4;
   if (dwFlags & 0x01)
      pMin.p[0] -= fOverlap;
   if (dwFlags & 0x02)
      pMax.p[0] += fOverlap;
   if (dwFlags & 0x04)
      pMax.p[2] += fOverlap;
   if (dwFlags & 0x08)
      pMin.p[2] -= fOverlap;

   // fill in width and height
   pSurf->fWidth = pMax.p[0] - pMin.p[0];
   pSurf->fHeight = pMax.p[2] - pMin.p[2];

   // rotation
   CPoint pTrans;
   mSplineToObject.ToXYZLLT (&pTrans, &pSurf->fRotZ, &pSurf->fRotX, &pSurf->fRotY);

   // translate so that half way between (pMin.x, pB1.y,pB1.z) and (pMax,pB2.y,pB2.z) ends up at 0,0,0
   CPoint pHalf;
   pHalf.Add (&pB1, &pB2);
   pHalf.Scale (.5);
   pHalf.p[0] = (pMin.p[0] + pMax.p[0]) / 2;
   if (dwFlags & 0x08)
      pHalf.p[2] -= fOverlap;
   pHalf.MultiplyLeft (&mSplineToObject);
   pSurf->pTrans.Copy (&pHalf);

 
   // Call wishlistadd
   PBBSURF pAdd;
   pAdd = WishListAdd (pSurf);
   if (!pAdd)
      return NULL;

   // keep flags indicating that flat
   pAdd->pSurface->m_fConstAbove = TRUE;
   pAdd->pSurface->m_fConstBottom = TRUE;
   pAdd->pSurface->m_fConstRectangle = TRUE;

   if (!pTop2 || (dwFlags & 0x20))
      goto done;  // cant be warped

   BOOL fDontWarp;
   fDontWarp = (fabs(pT1.p[1] - pT2.p[1]) < CLOSE);

   // if not supposed to extend left BUT the top is not over the bottom then need to warp
   if (!(dwFlags & 0x01) && (fabs(pT1.p[0] - pB1.p[0]) > CLOSE) )
      fDontWarp = FALSE;
   // same for right
   if (!(dwFlags & 0x02) && (fabs(pT2.p[0] - pB2.p[0]) > CLOSE) )
      fDontWarp = FALSE;
   // same for above
   if (!(dwFlags & 0x04) && (fabs(pT1.p[2] - pT2.p[2]) > CLOSE) )
      fDontWarp = FALSE;
   // dont worry about below since they're always flat

   if (fDontWarp)
      goto done;  // not warped


   // need to convert T1 and T2, along with B1, B2, into coords
   // that reflect the new coord space
   // X
   CMatrix m, mInv;
   pAdd->pSurface->MatrixGet (&m);
   m.Invert4 (&mInv);
   pT1.Copy (pTop1);
   pT1.p[3] = 1;
   pT1.MultiplyLeft (&mInv);
   pT2.Copy (pTop2);
   pT2.p[3] = 1;
   pT2.MultiplyLeft (&mInv);
   pB1.Copy (pBase1);
   pB1.p[3] = 1;
   pB1.MultiplyLeft (&mInv);
   pB2.Copy (pBase2);
   pB2.p[3] = 1;
   pB2.MultiplyLeft (&mInv);

   // note if warped
   fp fDeltaXT, fDeltaXB;
   // else, it's warped, so see where the line from pT1 to pT2 intersects pMin and pMax
   fDeltaXT = pT2.p[0] - pT1.p[0];
   fDeltaXB = pB2.p[0] - pB1.p[0];

   if ((fabs(fDeltaXT) < CLOSE) || (fabs(fDeltaXB) < CLOSE))
      goto done;  // to close

   // if we're allowed to extend left then move topleft (T1) or bottom left(B1)
   // to keep the sheet as square as possible
   CPoint pDelta;
   CPoint pTemp;
   fp fAlpha;
   if (dwFlags & 0x01) {   // ented to the left
      if (pT1.p[0] > pB1.p[0]) {
         pDelta.Subtract (&pT2, &pT1);
         fDeltaXT = pT2.p[0] - pT1.p[0];
         if (fabs((fp)fDeltaXT) < CLOSE) // BUGFIX - Was fabs(fDeltaXT < CLOSE)
            goto done;
         fAlpha = (pB1.p[0] - pT1.p[0]) / fDeltaXT;
         pTemp.Copy (&pDelta);
         pTemp.Scale (fAlpha);
         pTemp.Add (&pT1);
         pT1.Copy (&pTemp);
      }
      else {
         // extend base. Since it's on a flat plane already just move it
         pB1.p[0] = pT1.p[0];
      }
   }
   if (dwFlags & 0x02) {   // extend to the right
      if (pT2.p[0] < pB2.p[0]) {
         pDelta.Subtract (&pT2, &pT1);
         fDeltaXT = pT2.p[0] - pT1.p[0];
         if (fabs((fp)fDeltaXT) < CLOSE) // BUGFIX - was fabs(fDeltaXT < CLOSE)
            goto done;
         fAlpha = (pB2.p[0] - pT1.p[0]) / fDeltaXT;
         pTemp.Copy (&pDelta);
         pTemp.Scale (fAlpha);
         pTemp.Add (&pT1);
         pT2.Copy (&pTemp);
      }
      else {
         // extend base. Since it's on a flat plane already just move it
         pB2.p[0] = pT2.p[0];
      }
   }

   // extend the points by overlap
   DWORD dwDir, i;   // 0 for horiz, 1 for vertical
   for (dwDir = 0; dwDir < 2; dwDir++) for (i = 0; i < 2; i++) {
      PCPoint p1, p2;
      DWORD dwBitLeft, dwBitRight;
      switch (dwDir) {
      case 0:  // horizontal
         p1 = i ? &pB1 : &pT1;
         p2 = i ? &pB2 : &pT2;
         dwBitLeft = 0x01;
         dwBitRight = 0x02;
         break;
      case 1:  // vertical
         p1 = i ? &pB2 : &pB1;
         p2 = i ? &pT2 : &pT1;
         dwBitLeft = 0x08;
         dwBitRight = 0x04;
         break;
      }

      // find the vector
      pDelta.Subtract (p2, p1);
      pDelta.Normalize();
      pDelta.Scale (fOverlap);
      if (dwFlags & dwBitRight)
         p2->Add (&pDelta);
      pDelta.Scale (-1);
      if (dwFlags & dwBitLeft)
         p1->Add (&pDelta);
   }

   // convert these points back into object space
   //pT1.MultiplyLeft (&m);
   //pT2.MultiplyLeft (&m);
   //pB1.MultiplyLeft (&m);
   //pB2.MultiplyLeft (&m);

   // write them out
   pAdd->pSurface->m_fConstAbove = FALSE;
   pAdd->pSurface->m_fConstBottom = FALSE;
   pAdd->pSurface->m_fConstRectangle = FALSE;
   CPoint   apPoints[2][2];
   apPoints[0][0].Copy (&pT1);
   apPoints[0][1].Copy (&pT2);
   apPoints[1][0].Copy (&pB1);
   apPoints[1][1].Copy (&pB2);

   pAdd->pSurface->ControlPointsSet (2, 2, &apPoints[0][0],
      (DWORD*) SEGCURVE_LINEAR, (DWORD*) SEGCURVE_LINEAR, 3);
   // Use a higher level of detail in curved sections because otherwise wierd
   // stuff happens

   // extend the 
done:
   return pAdd;
}

typedef struct {
   CMatrix           m;    // translate from the roof-spline space to the object space
   DWORD             dwID; // original minor surface iD
   PCSplineSurface   pssA; // A side
   PCSplineSurface   pssB; // B side
   fp            fRotX, fRotY, fRotZ; // rotation on xyz
} MTR, *PMTR;

/**********************************************************************************
CObjectBuildBlock::MakeTheRoof - This function looks at the roof type
and calls WishListAdd() to add the new roof elements (or resize them). It
may do some reszing of its own at the end, and clips the roof against itself.

inputs
   fp      *pfRoofHeight - Filled with the max height of the roof (in object)
                  space, so this can be used to determine how high the walls need to be
   fp      *pfCeilingHeight - Filled with the height of the ceiling (in object space)
                  so the ceiling can be calculated later on
returns
   none
*/
void CObjectBuildBlock::MakeTheRoof (fp *pfRoofHeight, fp *pfCeilingHeight)
{
   // translate all the control points from relative coords to
   // object space
   CListFixed lControl;
   PCPoint  paOrig;
   paOrig = (PCPoint) m_lRoofControl.Get(0);
   lControl.Init (sizeof(CPoint), paOrig, m_lRoofControl.Num());
   PCPoint  paControl;
   DWORD    dwNum;
   paControl = (PCPoint) lControl.Get(0);
   dwNum = lControl.Num();
   RoofControlToObject (paControl, dwNum);

   // roof height based upon max control point
   // ceiling height based upon minimum
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         *pfRoofHeight = max(*pfRoofHeight, paControl[i].p[2]);
      }
      else {
         *pfRoofHeight = paControl[i].p[2];
      }
   }
   // increase roof height a bit just to make sure the walls will intersect against the roof
   *pfRoofHeight += (m_fCeilingThickSideA + m_fCeilingThickSideB + m_fCeilingThickStruct) * 4;

   // if no roof then roof height
   if (!dwNum)
      *pfRoofHeight = m_fHeight;

   *pfCeilingHeight = m_fHeight;

   // if there isnt a roof then the ceiking height (which limits the floors)
   // is set such that a floor can exist all the way to the top of the walls.
   if (m_dwRoofType == ROOFSHAPE_NONE)
      *pfCeilingHeight = m_fHeight + m_fLevelMinDist -
         (m_fFloorThickStruct + m_fFloorThickSideA + m_fFloorThickSideB);

   // NOTE: May want to get more sophisticated about ceiling height


   BOOL fIntersect;
   fIntersect = TRUE;   // will want to intersect roof against one another

   // set roof defaults
   BBSURF         bbs;
   memset (&bbs, 0, sizeof(bbs));
   wcscpy (bbs.szName, gszRoof);
   bbs.dwMajor = BBSURFMAJOR_ROOF;
   bbs.fThickStud = m_fRoofThickStruct;
   bbs.fThickSideA = m_fRoofThickSideA;
   bbs.fThickSideB = m_fRoofThickSideB;
   bbs.fHideEdges = FALSE;
   bbs.dwType = 0x20001;

   bbs.fKeepClip = TRUE;
   bbs.pKeepA.h = .5;
   bbs.pKeepA.v = 1;
   bbs.pKeepB = bbs.pKeepA;
   bbs.pKeepB.h = 1 - bbs.pKeepB.h;
   // FUTURERELEASE - some sort of option for roof bevel

   DWORD adwFlat[1] = {SEGCURVE_LINEAR};
   DWORD adwCurved[2] = {SEGCURVE_CIRCLENEXT, SEGCURVE_CIRCLEPREV};
   DWORD adwEllipse[2] = {SEGCURVE_ELLIPSENEXT, SEGCURVE_ELLIPSEPREV};
   CPoint apControl[9];
   CPoint pSum;
   PCPoint pTemp;
   DWORD dwH, dwV;
#define CONTROLPT(x,y) apControl[(x)+(y)*dwH]
#define FILLCONTROL(x,y,pt) (CONTROLPT(x,y).Copy(pt))

   switch (m_dwRoofType) {
   default:

   case ROOFSHAPE_HALFHIPLOOP:
      // bottom part
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[5], &paControl[7], 4);

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[6], &paControl[4], 4);

      // top part
      dwH = 2;
      dwV = 3;
      FILLCONTROL (0, 2, &paControl[5]);
      FILLCONTROL (1, 2, &paControl[7]);
      FILLCONTROL (0, 1, &paControl[8]);
      FILLCONTROL (1, 1, &paControl[9]);
      FILLCONTROL (0, 0, &paControl[4]);
      FILLCONTROL (1, 0, &paControl[6]);
      bbs.dwMinor = 2;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;

      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 4 | 8);  // extend top and bottom
      break;

   case ROOFSHAPE_GULLWINGCURVED:
      dwH = 2;
      dwV = 3;

      // right
      FILLCONTROL (0, 2, &paControl[1]);
      FILLCONTROL (1, 2, &paControl[3]);
      FILLCONTROL (0, 1, &paControl[5]);
      FILLCONTROL (1, 1, &paControl[7]);
      FILLCONTROL (0, 0, &paControl[8]);
      FILLCONTROL (1, 0, &paControl[9]);
      bbs.dwMinor = 0;
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 4);  // extend top and bottom

      // left
      FILLCONTROL (0, 2, &paControl[2]);
      FILLCONTROL (1, 2, &paControl[0]);
      FILLCONTROL (0, 1, &paControl[6]);
      FILLCONTROL (1, 1, &paControl[4]);
      FILLCONTROL (0, 0, &paControl[9]);
      FILLCONTROL (1, 0, &paControl[8]);
      bbs.dwMinor = 1;
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 4);  // extend top and bottom
      break;


   case ROOFSHAPE_CONE:
      fIntersect = FALSE;
      dwH = 3;
      dwV = 2;

      // IMPORTANT - This isn't sealed very well at the top, which may cause problems
      // in the future. Right now it doesn't seem to be causing any, and since fixing
      // the problem could be a pain, don't bother.

      // front, left
      for (i = 0; i < 4; i++) {
         DWORD dw1, dw2, dw3;
         switch (i) {
         case 0:
            dw1 = 2;
            dw2 = 0;
            dw3 = 1;
            break;
         case 1:
            dw1 = 0;
            dw2 = 1;
            dw3 = 3;
            break;
         case 2:
            dw1 = 1;
            dw2 = 3;
            dw3 = 2;
            break;
         case 3:
            dw1 = 3;
            dw2 = 2;
            dw3 = 0;
            break;
         }
         bbs.dwMinor = i;
         pSum.Average (&paControl[dw2], &paControl[dw1]);
         FILLCONTROL(0,1,&pSum);
         pSum.Average (&paControl[dw2], &paControl[dw3]);
         FILLCONTROL(2,1,&pSum);
         FILLCONTROL(0,0,&paControl[4]);
         FILLCONTROL(2,0,&paControl[4]);
         CONTROLPT(0,0).Average (&CONTROLPT(0,1), CLOSE*100);   // so not exactly at 0
         CONTROLPT(2,0).Average (&CONTROLPT(2,1), CLOSE*100);   // so not exactly at 0
         // BUGFIX - Multiply CLOSEx10 so normals at peak pointing in right direction
         pTemp = &CONTROLPT(1,1);
         if (i%2) {
            pTemp->p[1] = CONTROLPT(0,1).p[1];
            pTemp->p[0] = CONTROLPT(2,1).p[0];
         }
         else {
            pTemp->p[0] = CONTROLPT(0,1).p[0];
            pTemp->p[1] = CONTROLPT(2,1).p[1];
         }
         pTemp->p[2] = (CONTROLPT(0,1).p[2] + CONTROLPT(2,1).p[2])/2;
         pTemp = &CONTROLPT(1,0);
         if (i % 2) {
            pTemp->p[1] = CONTROLPT(0,0).p[1];
            pTemp->p[0] = CONTROLPT(2,0).p[0];
         }
         else {
            pTemp->p[0] = CONTROLPT(0,0).p[0];
            pTemp->p[1] = CONTROLPT(2,0).p[1];
         }
         pTemp->p[2] = (CONTROLPT(0,0).p[2] + CONTROLPT(2,0).p[2])/2;
         WishListAddCurved (&bbs, dwH, dwV, apControl, adwEllipse, adwFlat, 0);  // extend none
      }
      break;

   case ROOFSHAPE_CONECURVED:
      fIntersect = FALSE;
      dwH = 3;
      dwV = 3;

      // IMPORTANT - This isn't sealed very well at the top, which may cause problems
      // in the future. Right now it doesn't seem to be causing any, and since fixing
      // the problem could be a pain, don't bother.

      // front, left
      for (i = 0; i < 4; i++) {
         DWORD dw1, dw2, dw3;
         switch (i) {
         case 0:
            dw1 = 2;
            dw2 = 0;
            dw3 = 1;
            break;
         case 1:
            dw1 = 0;
            dw2 = 1;
            dw3 = 3;
            break;
         case 2:
            dw1 = 1;
            dw2 = 3;
            dw3 = 2;
            break;
         case 3:
            dw1 = 3;
            dw2 = 2;
            dw3 = 0;
            break;
         }
         bbs.dwMinor = i;

         pSum.Average (&paControl[dw2], &paControl[dw1]);
         FILLCONTROL(0,2,&pSum);
         pSum.Average (&paControl[dw2], &paControl[dw3]);
         FILLCONTROL(2,2,&pSum);
         pTemp = &CONTROLPT(1,2);
         if (i%2) {
            pTemp->p[1] = CONTROLPT(0,2).p[1];
            pTemp->p[0] = CONTROLPT(2,2).p[0];
         }
         else {
            pTemp->p[0] = CONTROLPT(0,2).p[0];
            pTemp->p[1] = CONTROLPT(2,2).p[1];
         }
         pTemp->p[2] = (CONTROLPT(0,2).p[2] + CONTROLPT(2,2).p[2])/2;

         pSum.Average (&paControl[dw2+4], &paControl[dw1+4]);
         FILLCONTROL(0,1,&pSum);
         pSum.Average (&paControl[dw2+4], &paControl[dw3+4]);
         FILLCONTROL(2,1,&pSum);
         pTemp = &CONTROLPT(1,1);
         if (i%2) {
            pTemp->p[1] = CONTROLPT(0,1).p[1];
            pTemp->p[0] = CONTROLPT(2,1).p[0];
         }
         else {
            pTemp->p[0] = CONTROLPT(0,1).p[0];
            pTemp->p[1] = CONTROLPT(2,1).p[1];
         }
         pTemp->p[2] = (CONTROLPT(0,1).p[2] + CONTROLPT(2,1).p[2])/2;

         FILLCONTROL(0,0,&paControl[8]);
         FILLCONTROL(2,0,&paControl[8]);
         CONTROLPT(0,0).Average (&CONTROLPT(0,1), CLOSE*100);   // so not exactly at 0
         CONTROLPT(2,0).Average (&CONTROLPT(2,1), CLOSE*100);   // so not exactly at 0
         // BUGFIX - Multiply CLOSEx10 so normals at peak pointing in right direction
         pTemp = &CONTROLPT(1,0);
         if (i % 2) {
            pTemp->p[1] = CONTROLPT(0,0).p[1];
            pTemp->p[0] = CONTROLPT(2,0).p[0];
         }
         else {
            pTemp->p[0] = CONTROLPT(0,0).p[0];
            pTemp->p[1] = CONTROLPT(2,0).p[1];
         }
         pTemp->p[2] = (CONTROLPT(0,0).p[2] + CONTROLPT(2,0).p[2])/2;

         WishListAddCurved (&bbs, dwH, dwV, apControl, adwEllipse, adwCurved, 0);  // extend none
      }
      break;

   case ROOFSHAPE_HEMISPHERE:
      fIntersect = FALSE;
      dwH = 3;
      dwV = 3;

      // front, left
      for (i = 0; i < 4; i++) {
         DWORD dw1, dw2, dw3;
         switch (i) {
         case 0:
            dw1 = 2;
            dw2 = 0;
            dw3 = 1;
            break;
         case 1:
            dw1 = 0;
            dw2 = 1;
            dw3 = 3;
            break;
         case 2:
            dw1 = 1;
            dw2 = 3;
            dw3 = 2;
            break;
         case 3:
            dw1 = 3;
            dw2 = 2;
            dw3 = 0;
            break;
         }
         bbs.dwMinor = i;

         pSum.Average (&paControl[dw2], &paControl[dw1]);
         FILLCONTROL(0,2,&pSum);
         pSum.Average (&paControl[dw2], &paControl[dw3]);
         FILLCONTROL(2,2,&pSum);
         pTemp = &CONTROLPT(1,2);
         if (i%2) {
            pTemp->p[1] = CONTROLPT(0,2).p[1];
            pTemp->p[0] = CONTROLPT(2,2).p[0];
         }
         else {
            pTemp->p[0] = CONTROLPT(0,2).p[0];
            pTemp->p[1] = CONTROLPT(2,2).p[1];
         }
         pTemp->p[2] = (CONTROLPT(0,2).p[2] + CONTROLPT(2,2).p[2])/2;

         DWORD j;
         for (j = 0; j < 3; j++) {
            pTemp = &CONTROLPT(j,1);
            pTemp->Copy (&CONTROLPT(j,2));
            pTemp->p[2] = paControl[4].p[2];
         }

         for (j = 0; j < 3; j++) {
            FILLCONTROL(j,0,&paControl[4]);
            CONTROLPT(j,0).Average (&CONTROLPT(j,1), CLOSE*10);   // so not exactly at 0
            // BUGFIX - Multiplied close*10 so normals would be correct at top
         }

         WishListAddCurved (&bbs, dwH, dwV, apControl, adwEllipse, adwEllipse, 0);  // extend none
      }
      break;


   case ROOFSHAPE_HEMISPHEREHALF:
      fIntersect = FALSE;
      dwH = 3;
      dwV = 3;

      // front, left
      for (i = 0; i < 2; i++) {
         DWORD dw1, dw2, dw3;
         switch (i) {
         case 0:
            dw1 = 2;
            dw2 = 0;
            dw3 = 1;
            break;
         case 1:
            dw1 = 0;
            dw2 = 1;
            dw3 = 3;
            break;
         }
         bbs.dwMinor = i;

         if (i == 0) {
            FILLCONTROL(0,2,&paControl[dw1]);
            FILLCONTROL(1,2,&paControl[dw2]);
            pSum.Average (&paControl[dw2], &paControl[dw3]);
            FILLCONTROL(2,2,&pSum);
         }
         else {
            pSum.Average (&paControl[dw1], &paControl[dw2]);
            FILLCONTROL(0,2,&pSum);
            FILLCONTROL(1,2,&paControl[dw2]);
            FILLCONTROL(2,2,&paControl[dw3]);
         }

         DWORD j;
         for (j = 0; j < 3; j++) {
            pTemp = &CONTROLPT(j,1);
            pTemp->Copy (&CONTROLPT(j,2));
            pTemp->p[2] = paControl[4].p[2];
         }

         for (j = 0; j < 3; j++) {
            FILLCONTROL(j,0,&paControl[4]);
            CONTROLPT(j,0).Average (&CONTROLPT(j,1), CLOSE*10);   // so not exactly at 0
            // BUGFIX - Multiplied close*10 so normals at peak are right
         }

         WishListAddCurved (&bbs, dwH, dwV, apControl, adwEllipse, adwEllipse, 0);  // extend none
      }
      break;

   case ROOFSHAPE_SHED:
   case ROOFSHAPE_FLAT:
   case ROOFSHAPE_SHEDSKEW:
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[1], &paControl[0], 0); // extend none
      break;

   case ROOFSHAPE_SHEDCURVED:
      bbs.dwMinor = 0;
      dwH = 2;
      dwV = 3;
      FILLCONTROL(0,2,&paControl[3]);
      FILLCONTROL(1,2,&paControl[2]);
      FILLCONTROL(0,0,&paControl[1]);
      FILLCONTROL(1,0,&paControl[0]);
      FILLCONTROL(0,1,&paControl[5]);
      FILLCONTROL(1,1,&paControl[4]);
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 0);  // extend none
      break;

   case ROOFSHAPE_HALFHIPCURVED:
      bbs.dwMinor = 0;
      dwH = 2;
      dwV = 3;
      FILLCONTROL(0,2,&paControl[1]);
      FILLCONTROL(1,2,&paControl[3]);
      FILLCONTROL(0,0,&paControl[0]);
      FILLCONTROL(1,0,&paControl[2]);
      FILLCONTROL(0,1,&paControl[4]);
      FILLCONTROL(1,1,&paControl[5]);
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 0);  // extend none
      break;
   case ROOFSHAPE_HALFHIPSKEW:
      CPoint pDif, pOther;
      pDif.Subtract (&paControl[1], &paControl[0]);
      pOther.Subtract (&paControl[3], &pDif);
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &pOther, &paControl[3], 4); // extend on left and top

      pDif.Subtract (&paControl[2], &paControl[3]);
      pOther.Subtract (&paControl[0], &pDif);
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &pOther, &paControl[0], 4); // extend on left and top
      break;

   case ROOFSHAPE_SHEDFOLDED:
      bbs.dwMinor = 0;  // back
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[5], &paControl[4], 4); // extend up

      bbs.dwMinor = 1; //font
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[5], &paControl[4], &paControl[1], &paControl[0], 8); // extend down
      break;

   case ROOFSHAPE_GABLEFOUR:
      CPoint pt, pb;
      fp fYRidge, fXRidge;

      fYRidge = (paOrig[4].p[1] + paOrig[5].p[1])/2;
      fXRidge = (paOrig[6].p[0] + paOrig[7].p[0])/2;

      pt.Average (&paControl[4], &paControl[5], 1-fYRidge);
      pb.Average (&paControl[1+8], &paControl[3+8], 1-fYRidge);

      // right roof, back and front side
      bbs.dwMinor = 0;
      bbs.pKeepA.h = 1;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &pb, &paControl[3+8], &pt, &paControl[5], 4 | 8); // extend up and down
      bbs.dwMinor = 1;
      bbs.pKeepA.h = 0;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[1+8], &pb, &paControl[4], &pt, 4 | 8); // extend up and down


      pt.Average (&paControl[4], &paControl[5], 1-fYRidge);
      pb.Average (&paControl[0+8], &paControl[2+8], 1-fYRidge);

      // left roof, front and back
      bbs.dwMinor = 2;
      bbs.pKeepA.h = 1;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &pb, &paControl[0+8], &pt, &paControl[4], 4 | 8); // extend up and down
      bbs.dwMinor = 3;
      bbs.pKeepA.h = 0;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[2+8], &pb, &paControl[5], &pt, 4 | 8); // extend up and down


      pt.Average (&paControl[6], &paControl[7], 1-fXRidge);
      pb.Average (&paControl[3], &paControl[2], 1-fXRidge);

      // back roof, right and left
      bbs.dwMinor = 4;
      bbs.pKeepA.h = 1;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &pb, &paControl[2], &pt, &paControl[7], 4); // extend up
      bbs.dwMinor = 5;
      bbs.pKeepA.h = 0;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[3], &pb, &paControl[6], &pt, 4); // extend up

      pt.Average (&paControl[6], &paControl[7], 1-fXRidge);
      pb.Average (&paControl[0], &paControl[1], 1-fXRidge);

      // front roof, right and left
      bbs.dwMinor = 6;
      bbs.pKeepA.h = 1;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &pb, &paControl[1], &pt, &paControl[6], 4); // extend up
      bbs.dwMinor = 7;
      bbs.pKeepA.h = 0;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[0], &pb, &paControl[7], &pt, 4); // extend up

      break;

   case ROOFSHAPE_HIPSHEDPEAK:
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[4], NULL, 4 | 1);   // up and left

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[4], NULL, 4 | 2);   // up and right

      // front roof
      bbs.dwMinor = 2;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &paControl[4], NULL, 4 | 2 | 1);  // up, left,right
      break;

   case ROOFSHAPE_HIPSHEDRIDGE:
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[5], NULL, 4 | 1);   // up and left

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[4], NULL, 4 | 2);   // up and right

      // front roof
      bbs.dwMinor = 2;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &paControl[4], &paControl[5], 2 | 1);  // left,right
      break;

   case ROOFSHAPE_NONE:
      // dont add antyhing to the wish list
      break;

   case ROOFSHAPE_TRIANGLEPEAK:
   case ROOFSHAPE_PENTAGONPEAK:
   case ROOFSHAPE_HEXAGONPEAK:
      DWORD dwCount;
      if (m_dwRoofType == ROOFSHAPE_TRIANGLEPEAK)
         dwCount = 3;
      else if (m_dwRoofType == ROOFSHAPE_PENTAGONPEAK)
         dwCount = 5;
      else
         dwCount = 6;
      for (i = 0; i < dwCount; i++) {
         bbs.dwMinor = i;
         WishListAddFlat (&bbs, &paControl[i+1], &paControl[((i+1)%dwCount)+1], &paControl[0], NULL, 4 | 2 | 1);
      }
      break;

   case ROOFSHAPE_HIPHALF:  // gable roof
   case ROOFSHAPE_SALTBOX:  // offset gable - salt-box
      // Gable roof
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[4], &paControl[5], 4);
      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[5], &paControl[4], 4);
      break;

   case ROOFSHAPE_HIPPEAK:  // single peak
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[4], NULL, 7);

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[4], NULL, 7);

      // front roof
      bbs.dwMinor = 2;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &paControl[4], NULL, 7);

      // back roof
      bbs.dwMinor = 3;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[4], NULL, 7);
      break;
   case ROOFSHAPE_HIPRIDGE:  // peak with ridge
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[4], &paControl[5], 7);

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[5], &paControl[4], 7);

      // front roof
      bbs.dwMinor = 2;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &paControl[4], NULL, 7);

      // back roof
      bbs.dwMinor = 3;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[5], NULL, 7);
      break;

   case ROOFSHAPE_OUTBACK1:  // peak with ridge, two sets of angles
   case ROOFSHAPE_OUTBACK2:
      // bottom part
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[5], &paControl[7], 7);

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[6], &paControl[4], 7);

      // front roof
      bbs.dwMinor = 2;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &paControl[4], &paControl[5], 7);

      // back roof
      bbs.dwMinor = 3;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[7], &paControl[6], 7);

      // top part
      // right roof
      bbs.dwMinor = 4;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[5], &paControl[7], &paControl[8], &paControl[9], 15);

      // left roof
      bbs.dwMinor = 5;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[6], &paControl[4], &paControl[9], &paControl[8], 15);

      // front roof
      bbs.dwMinor = 6;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[4], &paControl[5], &paControl[8], NULL, 15);

      // back roof
      bbs.dwMinor = 7;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[7], &paControl[6], &paControl[9], NULL, 15);
      break;

   case ROOFSHAPE_BALINESE:
      // bottom part
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[5], &paControl[7], 7);

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[6], &paControl[4], 7);

      // front roof
      bbs.dwMinor = 2;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &paControl[4], &paControl[5], 7);

      // back roof
      bbs.dwMinor = 3;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[7], &paControl[6], 7);

      // top part
      // right roof
      bbs.dwMinor = 4;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[5], &paControl[7], &paControl[9], &paControl[11], 1 | 2 | 4 | 8);

      // left roof
      bbs.dwMinor = 5;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[6], &paControl[4], &paControl[10], &paControl[8], 15);

      // front roof
      bbs.dwMinor = 6;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[4], &paControl[5], &paControl[8], &paControl[9], 15);

      // back roof
      bbs.dwMinor = 7;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[7], &paControl[6], &paControl[11], &paControl[10], 15);

      // very top
      bbs.dwMinor = 8;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[11], &paControl[10], &paControl[9], &paControl[8], 15 | 16);
         // the 16 makes the extension 4x as large
      break;

   case ROOFSHAPE_BALINESECURVED:
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;

      // right roof
      bbs.dwMinor = 0;
      dwH = 2;
      dwV = 3;
      FILLCONTROL(0,2,&paControl[1]);
      FILLCONTROL(1,2,&paControl[3]);
      FILLCONTROL(0,1,&paControl[5]);
      FILLCONTROL(1,1,&paControl[7]);
      FILLCONTROL(0,0,&paControl[9]);
      FILLCONTROL(1,0,&paControl[11]);
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 1|2|4);  // left,right,above

      // left roof
      bbs.dwMinor = 1;
      dwH = 2;
      dwV = 3;
      FILLCONTROL(0,2,&paControl[2]);
      FILLCONTROL(1,2,&paControl[0]);
      FILLCONTROL(0,1,&paControl[6]);
      FILLCONTROL(1,1,&paControl[4]);
      FILLCONTROL(0,0,&paControl[10]);
      FILLCONTROL(1,0,&paControl[8]);
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 1|2|4);  // left,right,above

      // front roof
      bbs.dwMinor = 2;
      dwH = 2;
      dwV = 3;
      FILLCONTROL(0,2,&paControl[0]);
      FILLCONTROL(1,2,&paControl[1]);
      FILLCONTROL(0,1,&paControl[4]);
      FILLCONTROL(1,1,&paControl[5]);
      FILLCONTROL(0,0,&paControl[8]);
      FILLCONTROL(1,0,&paControl[9]);
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 1|2|4);  // left,right,above

      // back roof
      bbs.dwMinor = 3;
      dwH = 2;
      dwV = 3;
      FILLCONTROL(0,2,&paControl[3]);
      FILLCONTROL(1,2,&paControl[2]);
      FILLCONTROL(0,1,&paControl[7]);
      FILLCONTROL(1,1,&paControl[6]);
      FILLCONTROL(0,0,&paControl[11]);
      FILLCONTROL(1,0,&paControl[10]);
      WishListAddCurved (&bbs, dwH, dwV, apControl, adwFlat, adwCurved, 1|2|4);  // left,right,above

      // very top
      bbs.dwMinor = 8;
      // NOTE: In order to get this to work, had to chose system of taking least error
      // in cutout
      bbs.fKeepClip = FALSE;
      bbs.fKeepLargest = TRUE;
      //bbs.pKeepA.h = .5;
      //bbs.pKeepA.v = .5;
      //bbs.pKeepB = bbs.pKeepA;
      //bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[11], &paControl[10], &paControl[9], &paControl[8], 15 | 16);
         // the 16 makes the extension 4x as large
      break;

   case ROOFSHAPE_GAMBREL:
   case ROOFSHAPE_GULLWING:
      // bottom part
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[5], &paControl[7], 4);

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[6], &paControl[4], 4);

      // top part
      // right roof
      bbs.dwMinor = 4;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[5], &paControl[7], &paControl[8], &paControl[9], 4|8);

      // left roof
      bbs.dwMinor = 5;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[6], &paControl[4], &paControl[9], &paControl[8], 4|8);

      break;

   case ROOFSHAPE_MONSARD:
      // bottom part
      // right roof
      bbs.dwMinor = 0;
      WishListAddFlat (&bbs, &paControl[1], &paControl[3], &paControl[5], &paControl[7], 7);

      // left roof
      bbs.dwMinor = 1;
      WishListAddFlat (&bbs, &paControl[2], &paControl[0], &paControl[6], &paControl[4], 7);

      // front roof
      bbs.dwMinor = 2;
      WishListAddFlat (&bbs, &paControl[0], &paControl[1], &paControl[4], &paControl[5], 7);

      // back roof
      bbs.dwMinor = 3;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[7], &paControl[6], 7);

      // top part
      // right roof
      bbs.dwMinor = 4;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[5], &paControl[7], &paControl[8], NULL, 15);

      // left roof
      bbs.dwMinor = 5;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[6], &paControl[4], &paControl[8], NULL, 15);

      // front roof
      bbs.dwMinor = 6;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[4], &paControl[5], &paControl[8], NULL, 15);

      // back roof
      bbs.dwMinor = 7;
      bbs.pKeepA.h = .5;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[7], &paControl[6], &paControl[8], NULL, 15);
      dwNum = 10;
      break;

   case ROOFSHAPE_GABLETHREE:  // one and a half ridge
      CPoint pCenterPeak, pLeft, pRight;
      pCenterPeak.Add (&paControl[7], &paControl[8]);
      pCenterPeak.Scale (.5);
      pCenterPeak.p[0] = paControl[6].p[0];
      pLeft.Copy (&paControl[4]);
      pLeft.p[1] += .1; // just to keep on a plane
      pRight.Copy (&paControl[5]);
      pRight.p[1] += .1;   // just to keep on a plane

      // back roof
      bbs.dwMinor = 0;
      bbs.fKeepClip = FALSE;
      WishListAddFlat (&bbs, &paControl[3], &paControl[2], &paControl[8], &paControl[7], 4); // extend top

      // front, left
      bbs.dwMinor = 1;
      bbs.fKeepClip = TRUE;
      bbs.pKeepA.h = 0;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[0], &paControl[4], &paControl[7], &pCenterPeak, 4 | 2);   // extend top+right

      // front, right
      bbs.dwMinor = 2;
      bbs.pKeepA.h = 1;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[5], &paControl[1], &pCenterPeak, &paControl[8], 4 | 1); // extend top+left

      // front, facing left
      bbs.dwMinor = 3;
      bbs.pKeepA.h = 1;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &pLeft, &paControl[4], &pCenterPeak, &paControl[6], 8 | 4 | 1);  // extend top,bottom and left

      // front, facing right
      bbs.dwMinor = 4;
      bbs.pKeepA.h = 0;
      bbs.pKeepA.v = .5;
      bbs.pKeepB = bbs.pKeepA;
      bbs.pKeepB.h = 1 - bbs.pKeepB.h;
      WishListAddFlat (&bbs, &paControl[5], &pRight, &paControl[6], &pCenterPeak, 8 | 4 | 2);  // extend top+bottom+right
      break;
   }


   // intersection routines below

   // clip the roof against itself
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // go through all the roof surfaces, cloning them (but extending them), so
   // will intersect better
   CListFixed lMTR;
   PBBSURF pClip;
   fp fExtend;
   lMTR.Init (sizeof(MTR));
   PMTR pmtr;
   lMTR.Required (m_listWish.Num());
   for (i = 0; i < m_listWish.Num(); i++) {
      MTR mtr;

      // if we're not intersecting then skip this step and the rest will
      // fall into place
      if (!fIntersect)
         continue;

      pClip = (PBBSURF) m_listWish.Get(i);
      if ((pClip->dwMajor != BBSURFMAJOR_ROOF) || !pClip->pSurface)
         continue;
   
      fExtend = (pClip->fThickStud + pClip->fThickSideA + pClip->fThickSideB);

      // BUGFIX - Dont extend because the extension theory of clipping only
      // works if the planes are perfectly flat. Warped planes or non-linear
      // dont work, so use the other solution of extending lines when clipped
      fExtend = 0;

      mtr.dwID = pClip->dwMinor;
      pClip->pSurface->MatrixGet(&mtr.m);
      mtr.pssA = pClip->pSurface->m_SplineA.CloneAndExtend (fExtend, fExtend, fExtend, fExtend, FALSE);
      mtr.pssB = pClip->pSurface->m_SplineB.CloneAndExtend (fExtend, fExtend, fExtend, fExtend, FALSE);
      mtr.fRotX = pClip->fRotX;
      mtr.fRotY = pClip->fRotY;
      mtr.fRotZ = pClip->fRotZ;

      lMTR.Add (&mtr);
   }

   CListFixed lSurfA, lSurfB, lMatrix;
   lSurfA.Init (sizeof(PCSplineSurface));
   lSurfB.Init (sizeof(PCSplineSurface));
   lMatrix.Init (sizeof(CMatrix));
   for (i = 0; i < m_listWish.Num(); i++) {
      pClip = (PBBSURF) m_listWish.Get(i);
      if ((pClip->dwMajor != BBSURFMAJOR_ROOF) || !pClip->pSurface)
         continue;

      // invert this
      CMatrix m, mInv;
      pClip->pSurface->MatrixGet (&m);
      m.Invert4 (&mInv);

      lSurfA.Clear();
      lSurfB.Clear();
      lMatrix.Clear();

      // go through ones that might clip
      lSurfA.Required (lMTR.Num());
      lSurfB.Required (lMTR.Num());
      lMatrix.Required (lMTR.Num());
      for (j = 0; j < lMTR.Num(); j++) {
         pmtr = (PMTR) lMTR.Get(j);

         // dont repeat same one
         if (pmtr->dwID == pClip->dwMinor)
            continue;

         // if they're similar angles then don't test against
         if ((fabs(pmtr->fRotX - pClip->fRotX) < CLOSE) &&
            (fabs(pmtr->fRotY - pClip->fRotY) < CLOSE) &&
            (fabs(pmtr->fRotZ - pClip->fRotZ) < CLOSE))
            continue;

         // keep track of these
         lSurfA.Add (&pmtr->pssA);
         lSurfB.Add (&pmtr->pssB);

         // and matrix
         m.Copy (&pmtr->m);
         m.MultiplyRight (&mInv);   // will now convert from other spline space into this one
         lMatrix.Add (&m);
      }

      // clip
      DWORD dwMode;
      if (pClip->fKeepClip)
         dwMode = 2;
         // BUGFIX - Was passing in 0x10002 but changed to 0x02 to fix baliniese. At
         // same time reduced overhang from .2 to .05. Seems to work.
      else
         dwMode = 1;
      if (pClip->fKeepLargest)
         dwMode = 0; // dont adjust
      pClip->pSurface->IntersectWithSurfacesAndCutout (
         (PCSplineSurface*) lSurfA.Get(0), (PCMatrix) lMatrix.Get(0), lSurfA.Num(), &pClip->pKeepA,
         (PCSplineSurface*) lSurfB.Get(0), (PCMatrix) lMatrix.Get(0), lSurfB.Num(), &pClip->pKeepB,
         gpszTrimRoof, TRUE, !pClip->fKeepLargest, dwMode, !pClip->fKeepLargest, FALSE);
      // NOTE: Specifically not using 0x10000 flag in IntersectXXX because if I do it
      // cases problem with 3-gabled roof like EA house.
   }
   
   // free the clones
   for (i = 0; i < lMTR.Num(); i++) {
      pmtr = (PMTR) lMTR.Get(i);
      if (pmtr->pssA)
         delete pmtr->pssA;
      if (pmtr->pssB)
         delete pmtr->pssB;

   }
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

}

/**********************************************************************************
CObjectBuildBlock::RoofDefaultControl - Sets the default control points based
on the type of roof. Used for initialization or when the roof's type is changed.

inputs
   DWORD       dwType - Roof type
returns
   none
*/
void CObjectBuildBlock::RoofDefaultControl (DWORD dwType)
{
   CPoint   apTemp[32];
   memset (apTemp, 0, sizeof(apTemp));

   // keep 4 corners around since might be useful
   // 0 = left, front
   // 1 = right, front
   // 2 = left, back
   // 3 = right, back
   DWORD x, i;
   for (x = 0; x < 4; x++) {
      apTemp[x].p[0] = (fp)(x%2);
      apTemp[x].p[1] = (fp)(x/2);
   }

   // clear the current list
   m_lRoofControl.Clear();
   DWORD dwNum;
   dwNum = 4;

   switch (m_dwRoofType) {
   default:
   case ROOFSHAPE_SHEDCURVED:
      apTemp[0].p[2] = .25;
      apTemp[1].p[2] = .25;
      apTemp[4].p[0] = 0;
      apTemp[4].p[1] = .5;
      apTemp[4].p[2] = .25;
      apTemp[5].p[0] = 1;
      apTemp[5].p[1] = .5;
      apTemp[5].p[2] = .25;
      dwNum = 6;
      break;

   case ROOFSHAPE_SHEDFOLDED:
      apTemp[0].p[2] = .25;
      apTemp[1].p[2] = .25;
      apTemp[4].p[0] = 0; // left
      apTemp[4].p[1] = .5;
      apTemp[4].p[2] = .05;
      apTemp[5].p[0] = 1; // right
      apTemp[5].p[1] = .5;
      apTemp[5].p[2] = .05;
      dwNum = 6;
      break;

   case ROOFSHAPE_SHED:
      apTemp[0].p[2] = .25;
      apTemp[1].p[2] = .25;
      dwNum = 4;
      break;

   case ROOFSHAPE_SHEDSKEW:
      apTemp[0].p[2] = .25;
      apTemp[1].p[2] = .25/2;
      apTemp[2].p[2] = .25/2;
      apTemp[3].p[2] = 0;
      dwNum = 4;
      break;

   case ROOFSHAPE_HALFHIPSKEW:
      apTemp[0].p[2] = .25;
      apTemp[3].p[2] = .25;
      dwNum = 4;
      break;

   case ROOFSHAPE_FLAT:
      // take what's there
      dwNum = 4;
      break;

   case ROOFSHAPE_GABLEFOUR:
      apTemp[4].p[0] = .5; // front peak
      apTemp[4].p[1] = 0;
      apTemp[4].p[2] = .25;
      apTemp[5].p[0] = .5; // back peak
      apTemp[5].p[1] = 1;
      apTemp[5].p[2] = .25;
      apTemp[6].p[0] = 1;  // right peak
      apTemp[6].p[1] = .5;
      apTemp[6].p[2] = .25;
      apTemp[7].p[0] = 0;  // left peak
      apTemp[7].p[1] = .5;
      apTemp[7].p[2] = .25;

      // edges of the front peak
      for (i = 0; i < 4; i++) {
         apTemp[8+i] = apTemp[i];
         if (apTemp[8+i].p[0] > .5)
            apTemp[8+i].p[0] -= .1;
         else
            apTemp[8+i].p[0] += .1;
      }
      dwNum = 12;
      break;

   case ROOFSHAPE_HIPSHEDPEAK:
   case ROOFSHAPE_HEMISPHEREHALF:
      apTemp[4].p[0] = .5;
      apTemp[4].p[1] = 1;
      apTemp[4].p[2] = .25;
      dwNum = 5;
      break;

   case ROOFSHAPE_HIPSHEDRIDGE:
      apTemp[4].p[0] = .33; // left peak
      apTemp[4].p[1] = 1;
      apTemp[4].p[2] = .25;
      apTemp[5].p[0] = .66; // right
      apTemp[5].p[1] = 1;
      apTemp[5].p[2] = .25;
      dwNum = 6;
      break;

   case ROOFSHAPE_NONE:
      // no control points
      dwNum = 0;
      break;

   case ROOFSHAPE_TRIANGLEPEAK:
   case ROOFSHAPE_PENTAGONPEAK:
   case ROOFSHAPE_HEXAGONPEAK:
      DWORD dwCount;
      if (m_dwRoofType == ROOFSHAPE_TRIANGLEPEAK)
         dwCount = 3;
      else if (m_dwRoofType == ROOFSHAPE_PENTAGONPEAK)
         dwCount = 5;
      else
         dwCount = 6;
      apTemp[0].Zero();
      apTemp[0].p[0] = apTemp[0].p[1] = .5;
      apTemp[0].p[2] = .25;
      for (i = 0; i < dwCount; i++) {
         apTemp[i+1].Zero();
         apTemp[i+1].p[0] = -sin((fp)i / (fp)dwCount * 2 * PI) * .5 + .5;
         apTemp[i+1].p[1] = cos((fp)i / (fp)dwCount * 2 * PI) * .5 + .5;
      }
      dwNum = dwCount+1;
      break;

   case ROOFSHAPE_HIPHALF:  // gable
   case ROOFSHAPE_HALFHIPCURVED:
      // gable roof
      apTemp[4].p[0] = .5; // front peak
      apTemp[4].p[1] = 0;
      apTemp[4].p[2] = .25;
      apTemp[5].p[0] = .5; // back
      apTemp[5].p[1] = 1;
      apTemp[5].p[2] = .25;
      dwNum = 6;
      break;

   case ROOFSHAPE_HIPPEAK:  // single peak
   case ROOFSHAPE_CONE:
   case ROOFSHAPE_HEMISPHERE:
      apTemp[4].p[0] = .5;
      apTemp[4].p[1] = .5;
      apTemp[4].p[2] = .25;
      dwNum = 5;
      break;


   case ROOFSHAPE_HIPRIDGE:  // peak with ridge
      apTemp[4].p[0] = .5; // front peak
      apTemp[4].p[1] = .33;
      apTemp[4].p[2] = .25;
      apTemp[5].p[0] = .5; // back
      apTemp[5].p[1] = .67;
      apTemp[5].p[2] = .25;
      dwNum = 6;
      break;

   case ROOFSHAPE_OUTBACK1:  // peak with ridge, two sets of angles
   case ROOFSHAPE_OUTBACK2:
      for (i = 0; i < 4; i++) {
         apTemp[i+4].Copy (&apTemp[i]);

         // average corners in with center point
         apTemp[i+4].p[0] += .5;
         apTemp[i+4].p[1] += .5;
         apTemp[i+4].p[2] += .25;
         apTemp[i+4].Scale (.5);
      }
      apTemp[8].p[0] = .5; // front peak
      apTemp[8].p[1] = (m_dwRoofType == ROOFSHAPE_OUTBACK1) ? .33 : .25;
      apTemp[8].p[2] = .35;
      apTemp[9].p[0] = .5; // back
      apTemp[9].p[1] = 1 - apTemp[8].p[1];
      apTemp[9].p[2] = .35;
      dwNum = 10;
      break;

   case ROOFSHAPE_BALINESE:
   case ROOFSHAPE_BALINESECURVED:
      for (i = 0; i < 4; i++) {
         apTemp[i+4].Copy (&apTemp[i]);

         // average corners in with center point
         apTemp[i+4].p[0] += .5;
         apTemp[i+4].p[1] += .5;
         apTemp[i+4].p[2] += .25;
         apTemp[i+4].Scale (.5);
      }
      for (i = 0; i < 4; i++) {
         apTemp[i+8].Copy (&apTemp[i+4]);

         // average corners in with center point
         apTemp[i+8].p[0] += .5;
         apTemp[i+8].p[1] += .5;
         apTemp[i+8].p[2] += .5;
         apTemp[i+8].Scale (.5);
      }
      dwNum = 12;
      break;

   case ROOFSHAPE_GAMBREL:
   case ROOFSHAPE_GULLWING:
   case ROOFSHAPE_GULLWINGCURVED:
      for (i = 0; i < 4; i++) {
         apTemp[i+4].Copy (&apTemp[i]);

         if (apTemp[i+4].p[0] < .5)
            apTemp[i+4].p[0] += .2;
         else
            apTemp[i+4].p[0] -= .2;
         apTemp[i+4].p[2] += ((m_dwRoofType == ROOFSHAPE_GAMBREL) ? .2 : .05);
      }
      apTemp[8].p[0] = .5; // front peak
      apTemp[8].p[1] = 0;
      apTemp[8].p[2] = .25;
      apTemp[9].p[0] = .5; // back
      apTemp[9].p[1] = 1;
      apTemp[9].p[2] = .25;
      dwNum = 10;
      break;


   case ROOFSHAPE_HALFHIPLOOP:
      for (i = 0; i < 4; i++) {
         apTemp[i+4].Copy (&apTemp[i]);

         if (apTemp[i+4].p[0] < .5)
            apTemp[i+4].p[0] += .4;
         else
            apTemp[i+4].p[0] -= .4;
         apTemp[i+4].p[2] += .2;
      }
      apTemp[8].p[0] = .5; // front peak
      apTemp[8].p[1] = 0;
      apTemp[8].p[2] = .25;
      apTemp[9].p[0] = .5; // back
      apTemp[9].p[1] = 1;
      apTemp[9].p[2] = .25;
      dwNum = 10;
      break;

   case ROOFSHAPE_MONSARD:  // peak with ridge, two sets of angles
   case ROOFSHAPE_CONECURVED:
      for (i = 0; i < 4; i++) {
         apTemp[i+4].Copy (&apTemp[i]);

         if (apTemp[i+4].p[0] < .5)
            apTemp[i+4].p[0] += .2;
         else
            apTemp[i+4].p[0] -= .2;

         if (apTemp[i+4].p[1] < .5)
            apTemp[i+4].p[1] += .2;
         else
            apTemp[i+4].p[1] -= .2;

         if (m_dwRoofType == ROOFSHAPE_CONECURVED)
            apTemp[i+4].p[2] += .05;
         else
            apTemp[i+4].p[2] += .20;
      }
      apTemp[8].p[0] = .5; // peak
      apTemp[8].p[1] = .5;
      apTemp[8].p[2] = .3;
      dwNum = 9;
      break;

   case ROOFSHAPE_SALTBOX:  // offset gable - salt-box roof
      // gable roof
      apTemp[0].p[2] -= .25/2;
      apTemp[2].p[2] -= .25/2;
      apTemp[4].p[0] = .75; // front peak
      apTemp[4].p[1] = 0;
      apTemp[4].p[2] = .25;
      apTemp[5].p[0] = .75; // back
      apTemp[5].p[1] = 1;
      apTemp[5].p[2] = .25;
      dwNum = 6;
      break;

   case ROOFSHAPE_GABLETHREE:  // one and a half ridge
      apTemp[4].Copy (&apTemp[0]);  // near front,left
      apTemp[4].p[0] += .1;
      apTemp[5].Copy (&apTemp[1]);  // near front right
      apTemp[5].p[0] -= .1;
      apTemp[6].p[0] = .5; // front peak
      apTemp[6].p[1] = 0;
      apTemp[6].p[2] = .25;
      apTemp[7].p[0] = 0;  // left peak
      apTemp[7].p[1] = .5;
      apTemp[7].p[2] = .25;
      apTemp[8].p[0] = 1;  // right peak
      apTemp[8].p[1] = .5;
      apTemp[8].p[2] = .25;
      dwNum = 9;
      break;

   }

   // adjust the slope based upon climate
   fp fScale;
   fScale = 1;
   DWORD dwClimate;
   PWSTR psz;
   PCWorldSocket pWorld;
   pWorld = WorldGet(m_OSINFO.dwRenderShard, NULL);
   psz = pWorld ? pWorld->VariableGet (WSClimate()) : NULL;
   dwClimate = 0;
   if (psz)
      dwClimate = (DWORD) _wtoi(psz);
   else
      DefaultBuildingSettings (NULL, NULL, NULL, &dwClimate);  // BUGFIX - Def building climate
   switch (dwClimate) {
   case 0:  // tropical
   case 5:  // arid
      fScale *= 2.0 / 3.0;
      break;
   case 1:  // sub-tropical
   case 6:
      // no change
      break;
   case 2:  // temperate
   case 7:  // tundra
      fScale *= 4.0 / 3.0;
      break;
   case 3:  // alpine
      fScale *= 2.0;
      break;
   }
   for (i = 0; i < dwNum; i++)
      apTemp[i].p[2] *= fScale;

   m_lRoofControl.Init (sizeof(CPoint), apTemp, dwNum);

   // convert all the points into our current scheme because the
   // programming is all assuming (width + length)/2
   ConvertRoofPoints (0, m_dwRoofHeight);
}

/**********************************************************************************
CObjectBuildBlock::RoofControlToObject - Converts one or more roof control points
from roof-space (x = 0..1, y=0..1, z=0 for normal bottom of roof, scaled as average of
x and y.

inputs
   PCPoint     paConvert - Points with initial roof-space, converted to object space
   DWORD       dwNum - Number of points
*/
void CObjectBuildBlock::RoofControlToObject (PCPoint paConvert, DWORD dwNum)
{
   // calculate the boundaries in world space
   fp   fXMin, fXMax, fYMin, fYMax, fZMin, fZMax, fXDelta, fYDelta, fZDelta;
   fp   fWall;
   fWall = m_fWallThickStruct / 2 + m_fWallThickSideA;
   fXMin = m_fXMin - fWall - m_fRoofOverhang1;
   fXMax = m_fXMax + fWall + m_fRoofOverhang1;
   fYMin = m_fYMin - fWall - m_fRoofOverhang2;
   fYMax = m_fYMax + fWall + m_fRoofOverhang2;
   fXDelta = fXMax - fXMin;
   fYDelta = fYMax - fYMin;

   switch (m_dwRoofHeight) {
      default:
      case 0:  // sum of width and length
         fZDelta = (fXDelta + fYDelta)/2;
         break;
      case 1:  // width
         fZDelta = fXDelta;
         break;
      case 2:  // length
         fZDelta = fYDelta;
         break;
      case 3:  // constant
         fZDelta = 1;
         break;
   }
   fZDelta = max(.01, fZDelta);

   fZMin = m_fHeight;
   fZMax = fZMin + fZDelta;
   fXDelta = max(.01, fXDelta);
   fYDelta = max(.01, fYDelta);

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      paConvert[i].p[0] = paConvert[i].p[0] * fXDelta + fXMin;
      paConvert[i].p[1] = paConvert[i].p[1] * fYDelta + fYMin;
      paConvert[i].p[2] = paConvert[i].p[2] * fZDelta + fZMin;
   }
}

/**********************************************************************************
CObjectBuildBlock::RoofControlFromObject - Converts one or more roof control points
to roof-space (x = 0..1, y=0..1, z=0 for normal bottom of roof, scaled as average of
x and y.) from object space.

inputs
   PCPoint     paConvert - Points with initial object space converted to roof-space
   DWORD       dwNum - Number of points
*/
void CObjectBuildBlock::RoofControlFromObject (PCPoint paConvert, DWORD dwNum)
{
   // calculate the boundaries in world space
   fp   fXMin, fXMax, fYMin, fYMax, fZMin, fZMax, fXDelta, fYDelta, fZDelta;
   fp   fWall;
   fWall = m_fWallThickStruct / 2 + m_fWallThickSideA;
   fXMin = m_fXMin - fWall - m_fRoofOverhang1;
   fXMax = m_fXMax + fWall + m_fRoofOverhang1;
   fYMin = m_fYMin - fWall - m_fRoofOverhang2;
   fYMax = m_fYMax + fWall + m_fRoofOverhang2;
   fXDelta = fXMax - fXMin;
   fYDelta = fYMax - fYMin;
   fZDelta = (fXDelta + fYDelta)/2;
   fZMin = m_fHeight;
   fZMax = fZMin + fZDelta;
   fXDelta = max(.01, fXDelta);
   fYDelta = max(.01, fYDelta);
   fZDelta = max(.01, fZDelta);

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      paConvert[i].p[0] = (paConvert[i].p[0] - fXMin) / fXDelta;
      paConvert[i].p[1] = (paConvert[i].p[1] - fYMin) / fYDelta;
      paConvert[i].p[2] = (paConvert[i].p[2] - fZMin) / fZDelta;
   }
}

/*************************************************************************************
CObjectBuildBlock::RoofControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBuildBlock::RoofControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   fp fKnobSize = .3;

   if (dwID >= m_lRoofControl.Num())
      return FALSE;

   // translate all the control points from relative coords to
   // object space. It's a bit wasteful but doing this because
   // may want to display the angle, so need world coords for all
   CListFixed lControl;
   lControl.Init (sizeof(CPoint), m_lRoofControl.Get(0), m_lRoofControl.Num());
   PCPoint  paControl;
   DWORD    dwNum;
   paControl = (PCPoint) lControl.Get(0);
   dwNum = lControl.Num();
   RoofControlToObject (paControl, dwNum);

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = RGB(0xff, 0xff, 0);
   wcscpy (pInfo->szName, L"Roof");

   pInfo->pLocation.Copy (&paControl[dwID]);

   // display angle information
   fp    afAngle[4];
   DWORD i, j, dwCurAngle;
   dwCurAngle = 0;
   fp fAngle;
   for (i = 0; i < m_listBBSURF.Num(); i++) {
      PBBSURF pbbs = (PBBSURF) m_listBBSURF.Get(i);
      if (pbbs->dwMajor != BBSURFMAJOR_ROOF)
         continue;   // only care about roofs
      if (fabs(pbbs->fRotY) > CLOSE)
         continue;   // with a strange shape dont guess at angle
      fAngle = PI/2 + pbbs->fRotX;
      for (j = 0; j < dwCurAngle; j++)
         if (fabs((fp)(fAngle - afAngle[j])) < CLOSE) // BUGFIX - was fabs((fAngle - afAngle[j]) < CLOSE)
            break;
      if ((j < dwCurAngle) || (dwCurAngle > 3))
         continue;   // either it's the same as one already have, or too many angles already

      // else, add
      afAngle[dwCurAngle] = fAngle;
      dwCurAngle++;
   }
   for (i = 0; i < dwCurAngle; i++) {
      if (i)
         wcscat (pInfo->szMeasurement, L", ");
      fAngle = ApplyGrid(fmod((fp)afAngle[i], (fp)(2 * PI)) / 2 / PI * 360, .1);
      swprintf (pInfo->szMeasurement + wcslen(pInfo->szMeasurement), L"%g", (double)fAngle);
   }
   if (dwCurAngle)
      wcscat (pInfo->szMeasurement, L" deg");

   return TRUE;
}


/*************************************************************************************
CObjectBuildBlock::RoofControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBuildBlock::RoofControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   DWORD i;
   fp fZ;
   if (dwID >= m_lRoofControl.Num())
      return FALSE;

   // convert this into roof space
   CPoint pNew;
   pNew.Copy (pVal);
   RoofControlFromObject (&pNew, 1);

   // translate all the control points from relative coords to
   // object space. It's a bit wasteful but doing this because
   // may want to display the angle, so need world coords for all
   PCPoint  paControl;
   DWORD    dwNum;
   paControl = (PCPoint) m_lRoofControl.Get(0);
   dwNum = m_lRoofControl.Num();

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   PCPoint pCur;
   pCur = &paControl[dwID];
   // Restrictions
   if (m_dwRoofControlFreedom < 2) {   // if it's 1 or 0

      switch (m_dwRoofType) {
      case ROOFSHAPE_SHEDSKEW:
         // cant move either of those
         if ((dwID >= 1) && (dwID <= 2))
            pNew.Copy (pCur);
         break;
      case ROOFSHAPE_TRIANGLEPEAK:
      case ROOFSHAPE_PENTAGONPEAK:
      case ROOFSHAPE_HEXAGONPEAK:
         DWORD dwCount;
         if (m_dwRoofType == ROOFSHAPE_TRIANGLEPEAK)
            dwCount = 3;
         else if (m_dwRoofType == ROOFSHAPE_PENTAGONPEAK)
            dwCount = 5;
         else
            dwCount = 6;
         if (dwID == 0) {
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         if ((dwID >= 1) && (dwID < 1 + dwCount))
            pNew.p[2] = pCur->p[2];
         break;
      case ROOFSHAPE_GABLEFOUR:
         // cant move these at all
         if ((dwID >= 0) && (dwID <= 3))
            pNew = *pCur;

         // cant move the near front up.down, forwards, back
         if ((dwID >= 8) && (dwID <= 11)) {
            pNew.p[1] = pCur->p[1];
            pNew.p[2] = pCur->p[2];
         }

         // cant move front and gable forwards
         if ((dwID == 4) || (dwID == 5))
            pNew.p[1] = pCur->p[1];

         // cant move left/right gables left/right
         if ((dwID == 7) || (dwID == 6))
            pNew.p[0] = pCur->p[0];

         break;
      case ROOFSHAPE_HIPHALF:  // half hip
      case ROOFSHAPE_SALTBOX:  // salt-box roof
      case ROOFSHAPE_HALFHIPCURVED:
         // cant move front/back
         if ((dwID == 4) || (dwID == 5))
            pNew.p[1] = pCur->p[1];
         break;

      case ROOFSHAPE_HIPSHEDRIDGE:  // hip, ridge
         // cant move front/back
         if ((dwID == 4) || (dwID == 5)) {
            pNew.p[1] = pCur->p[1];
         }
         break;

      case ROOFSHAPE_HIPPEAK:  // hip, peak
         break;

      case ROOFSHAPE_HIPRIDGE:  // hip, ridge
         break;

      case ROOFSHAPE_OUTBACK1:  // inverse monsard
      case ROOFSHAPE_OUTBACK2:
         // cant move left/right
         if ((dwID == 8) || (dwID == 9)) {
            pNew.p[0] = pCur->p[0];

            if (m_dwRoofType == ROOFSHAPE_OUTBACK2)
               pNew.p[1] = pCur->p[1];
         }
         break;

      case ROOFSHAPE_GAMBREL:
      case ROOFSHAPE_GULLWING:
      case ROOFSHAPE_HALFHIPLOOP:
      case ROOFSHAPE_GULLWINGCURVED:
         // cant move left/right, front/back
         if ((dwID == 8) || (dwID == 9)) {
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         if ((dwID >= 4) && (dwID <= 7))
            pNew.p[1] = pCur->p[1]; // cant move front,back
         break;

      case ROOFSHAPE_MONSARD:
      case ROOFSHAPE_CONECURVED:
         // cant move left/right
         if (dwID == 8) {
            pNew.p[0] = pCur->p[0];
         }
         break;

      case ROOFSHAPE_GABLETHREE:  // three gable
         // cant move these at all
         if ((dwID >= 0) && (dwID <= 3))
            pNew = *pCur;

         // cant move the near front up.down, forwards, back
         if ((dwID == 4) || (dwID == 5)) {
            pNew.p[1] = pCur->p[1];
            pNew.p[2] = pCur->p[2];
         }

         // cant move front gable forwards
         if (dwID == 6)
            pNew.p[1] = pCur->p[1];

         // cant move left/right gables left/right
         if ((dwID == 7) || (dwID == 8))
            pNew.p[0] = pCur->p[0];

         break;
      default:
         break;
      }

   }
   BOOL fRestrict;
   fRestrict = !((m_dwRoofType == ROOFSHAPE_SHED) || (m_dwRoofType == ROOFSHAPE_SHEDFOLDED) ||
      (m_dwRoofType == ROOFSHAPE_TRIANGLEPEAK) || (m_dwRoofType == ROOFSHAPE_PENTAGONPEAK) ||
      (m_dwRoofType == ROOFSHAPE_HEXAGONPEAK) ||
      (m_dwRoofType == ROOFSHAPE_HALFHIPSKEW) || (m_dwRoofType == ROOFSHAPE_SHEDSKEW) ||
      (m_dwRoofType == ROOFSHAPE_SHEDCURVED));
   if (m_dwRoofControlFreedom == 1) {   // if it's 1
      // default behaviour for edges is (pooint 0..3) is to let them move up and down only
      if (fRestrict && (dwID >= 0) && (dwID <= 3)) {
         pNew.p[0] = pCur->p[0];
         pNew.p[1] = pCur->p[1];
      }

      switch (m_dwRoofType) {
      case ROOFSHAPE_HIPHALF:  // half hip
         break;

      case ROOFSHAPE_HIPPEAK:  // hip, peak
         break;

      case ROOFSHAPE_HIPRIDGE:  // hip, ridge
         break;

      case ROOFSHAPE_OUTBACK1:  // inverse monsard
         break;

      case ROOFSHAPE_SALTBOX:  // salt-box roof
         break;

      case ROOFSHAPE_GABLETHREE:  // three gable
         break;
      default:
         break;
      }

   }
   if (m_dwRoofControlFreedom == 0) {  // only if it's 0
      // default behaviour for edges is (pooint 0..3) is to not let them move
      if (fRestrict && (dwID >= 0) && (dwID <= 3))
         pNew.Copy (pCur);

      switch (m_dwRoofType) {
      case ROOFSHAPE_TRIANGLEPEAK:
      case ROOFSHAPE_PENTAGONPEAK:
      case ROOFSHAPE_HEXAGONPEAK:
         DWORD dwCount;
         if (m_dwRoofType == ROOFSHAPE_TRIANGLEPEAK)
            dwCount = 3;
         else if (m_dwRoofType == ROOFSHAPE_PENTAGONPEAK)
            dwCount = 5;
         else
            dwCount = 6;
         if ((dwID >= 1) && (dwID < 1 + dwCount)) {
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         break;

      case ROOFSHAPE_GABLEFOUR:
         // cant move the near front left/right
         if ((dwID >= 8) && (dwID <= 11)) {
            pNew.p[0] = pCur->p[0];
         }

         // cant move front gable left/right
         if ((dwID == 4) || (dwID == 5))
            pNew.p[0] = pCur->p[0];

         // cant move left/right gables forwards/backwards
         if ((dwID == 6) || (dwID == 7))
            pNew.p[1] = pCur->p[1];

         break;
      case ROOFSHAPE_SHED:
      case ROOFSHAPE_FLAT:
      case ROOFSHAPE_HALFHIPSKEW:
      case ROOFSHAPE_SHEDSKEW:
      case ROOFSHAPE_SHEDCURVED:
         if ((dwID >= 0) && (dwID <= 5)) {
            // can only move up and down
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         break;

      case ROOFSHAPE_SHEDFOLDED:
         if ((dwID >= 0) && (dwID <= 3)) {
            // can only move up and down
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         if ((dwID >= 4) && (dwID <= 5)) {
            // can only move up and down
            pNew.p[0] = pCur->p[0];
         }
         break;

      case ROOFSHAPE_HIPHALF:  // half hip
      case ROOFSHAPE_HALFHIPCURVED:
         // cant move right/left
         if ((dwID == 4) || (dwID == 5))
            pNew.p[0] = pCur->p[0];
         break;

      case ROOFSHAPE_HIPPEAK:  // hip, peak
      case ROOFSHAPE_HIPSHEDPEAK:
      case ROOFSHAPE_CONE:
      case ROOFSHAPE_HEMISPHERE:
      case ROOFSHAPE_HEMISPHEREHALF:
         if (dwID == 4) {
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         break;


      case ROOFSHAPE_HIPRIDGE:  // hip, ridge
         // cant move front/back
         if ((dwID == 4) || (dwID == 5)) {
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         break;

      case ROOFSHAPE_HIPSHEDRIDGE:  // hip, ridge
         // cant move left/right
         if ((dwID == 4) || (dwID == 5)) {
            pNew.p[0] = pCur->p[0];
         }
         break;

      case ROOFSHAPE_OUTBACK1:  // inverse monsard
      case ROOFSHAPE_OUTBACK2:  // inverse monsard
      case ROOFSHAPE_MONSARD:
      case ROOFSHAPE_CONECURVED:
         // only move up and down
         if ((dwID >= 4) && (dwID <= 8)) {
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         break;

      case ROOFSHAPE_BALINESE:
      case ROOFSHAPE_BALINESECURVED:
         // only move up and down
         pNew.p[0] = pCur->p[0];
         pNew.p[1] = pCur->p[1];
         break;

      case ROOFSHAPE_GAMBREL:
      case ROOFSHAPE_GULLWING:
      case ROOFSHAPE_HALFHIPLOOP:
      case ROOFSHAPE_GULLWINGCURVED:
         // only move up and down
         if ((dwID >= 4) && (dwID <= 7)) {
            pNew.p[0] = pCur->p[0];
            pNew.p[1] = pCur->p[1];
         }
         break;

      case ROOFSHAPE_SALTBOX:  // salt-box roof
         break;

      case ROOFSHAPE_GABLETHREE:  // three gable
         // cant move the near front left/right
         if ((dwID == 4) || (dwID == 5)) {
            pNew.p[0] = pCur->p[0];
         }

         // cant move front gable left/right
         if (dwID == 6)
            pNew.p[0] = pCur->p[0];

         // cant move left/right gables forwards/backwards
         if ((dwID == 7) || (dwID == 8))
            pNew.p[1] = pCur->p[1];

         break;
      default:
         break;
      }
   }

   // store this away if there are no arguements
   pCur->Copy (&pNew);

   // move other points along with this
   if (m_dwRoofControlFreedom < 2) {   // if it's 1 or 0

      switch (m_dwRoofType) {
      case ROOFSHAPE_SHED:
      case ROOFSHAPE_FLAT:
      case ROOFSHAPE_SHEDCURVED:
         if ((dwID == 0) || (dwID == 1)) {
            paControl[1-dwID].p[0] = 1-pNew.p[0];
            paControl[1-dwID].p[1] = pNew.p[1];
            paControl[1-dwID].p[2] = pNew.p[2];
         }
         if ((dwID == 2) || (dwID == 3)) {
            paControl[5-dwID].p[0] = 1-pNew.p[0];
            paControl[5-dwID].p[1] = pNew.p[1];
            paControl[5-dwID].p[2] = pNew.p[2];
         }
         if ((dwID == 4) || (dwID == 5)) {   // only for shed curved
            paControl[9-dwID].p[0] = 1-pNew.p[0];
            paControl[9-dwID].p[1] = pNew.p[1];
            paControl[9-dwID].p[2] = pNew.p[2];
         }
         break;

      case ROOFSHAPE_HALFHIPSKEW:
         if ((dwID == 0) || (dwID == 3)) {
            paControl[3-dwID].p[0] = 1-pNew.p[0];
            paControl[3-dwID].p[1] = 1-pNew.p[1];
            paControl[3-dwID].p[2] = pNew.p[2];
         }
         if ((dwID == 1) || (dwID == 2)) {
            paControl[3-dwID].p[0] = 1-pNew.p[0];
            paControl[3-dwID].p[1] = 1-pNew.p[1];
            paControl[3-dwID].p[2] = pNew.p[2];
         }
         break;

      case ROOFSHAPE_SHEDSKEW:
         if ((dwID == 0) || (dwID == 3)) {
            fp fDist, fAvg;
            fAvg = (paControl[1].p[2] + paControl[2].p[2]) / 2;
            fDist = pNew.p[2] - fAvg;

            paControl[3-dwID].p[0] = 1-pNew.p[0];
            paControl[3-dwID].p[1] = 1-pNew.p[1];
            paControl[3-dwID].p[2] = fAvg - fDist;
         }
         if ((dwID == 1) || (dwID == 2)) {
            paControl[3-dwID].p[0] = 1-pNew.p[0];
            paControl[3-dwID].p[1] = 1-pNew.p[1];
            paControl[3-dwID].p[2] = pNew.p[2];
         }
         break;

      case ROOFSHAPE_SHEDFOLDED:
         if ((dwID == 0) || (dwID == 1)) {
            paControl[1-dwID].p[0] = 1-pNew.p[0];
            paControl[1-dwID].p[1] = pNew.p[1];
            paControl[1-dwID].p[2] = pNew.p[2];
         }
         if ((dwID == 2) || (dwID == 3)) {
            paControl[5-dwID].p[0] = 1-pNew.p[0];
            paControl[5-dwID].p[1] = pNew.p[1];
            paControl[5-dwID].p[2] = pNew.p[2];
         }
         if ((dwID == 4) || (dwID == 5)) {
            paControl[9-dwID].p[0] = 1-pNew.p[0];
            paControl[9-dwID].p[1] = pNew.p[1];
            paControl[9-dwID].p[2] = pNew.p[2];
         }
         break;

      case ROOFSHAPE_HIPHALF:  // half hip
      case ROOFSHAPE_HIPRIDGE:  // hip, ridge
      case ROOFSHAPE_HALFHIPCURVED:
         // keep same and opposite
         if (dwID == 4) {
            paControl[5].Copy (&pNew);
            paControl[5].p[1] = 1 - pNew.p[1];
         }
         else if (dwID == 5) {
            paControl[4].Copy (&pNew);
            paControl[4].p[1] = 1 - pNew.p[1];
         }
         break;

      case ROOFSHAPE_HIPSHEDRIDGE:  // hip, ridge
         // keep same and opposite
         if ((dwID == 4) || (dwID == 5)) {
            paControl[9-dwID].Copy (&pNew);
            paControl[9-dwID].p[0] = 1 - pNew.p[0];
         }
         break;
      case ROOFSHAPE_HIPPEAK:  // hip, peak
         break;

      case ROOFSHAPE_OUTBACK1:  // inverse monsard
      case ROOFSHAPE_OUTBACK2:
      case ROOFSHAPE_MONSARD:
      case ROOFSHAPE_GAMBREL:
      case ROOFSHAPE_GULLWING:
      case ROOFSHAPE_CONECURVED:
      case ROOFSHAPE_HALFHIPLOOP:
      case ROOFSHAPE_GULLWINGCURVED:
         // keep it square
         if ((dwID >= 4) && (dwID <= 7)) {
            fp fx, fy, fz;
            fx = min(pNew.p[0], 1-pNew.p[0]);
            fy = min(pNew.p[1], 1-pNew.p[1]);
            fz = pNew.p[2];

            DWORD x;
            for (x = 0; x < 4; x++) {
               paControl[x+4].p[0] = (x%2) ? (1-fx) : fx;
               paControl[x+4].p[1] = (x/2) ? (1-fy) : fy;
               paControl[x+4].p[2] = fz;
            }

            if (m_dwRoofType == ROOFSHAPE_OUTBACK2) {
               paControl[8].p[1] = fy;
               paControl[9].p[1] = 1-fy;
            }
         }

         if ((m_dwRoofType == ROOFSHAPE_OUTBACK2) && (dwID == 8 || dwID == 9)) {
            fp fy;
            fy = min(pNew.p[1], 1-pNew.p[1]);

            DWORD x;
            for (x = 0; x < 4; x++) {
               paControl[x+4].p[1] = (x/2) ? (1-fy) : fy;
            }

            if (m_dwRoofType == ROOFSHAPE_OUTBACK2) {
               paControl[8].p[1] = fy;
               paControl[9].p[1] = 1-fy;
            }
         }

         if (!((m_dwRoofType == ROOFSHAPE_MONSARD) || (m_dwRoofType == ROOFSHAPE_CONECURVED))){
            // keep same and opposite
            if (dwID == 8) {
               paControl[9].Copy (&pNew);
               paControl[9].p[1] = 1 - pNew.p[1];
            }
            else if (dwID == 9) {
               paControl[8].Copy (&pNew);
               paControl[8].p[1] = 1 - pNew.p[1];
            }
         }
         break;

      case ROOFSHAPE_BALINESE:
      case ROOFSHAPE_BALINESECURVED:
         // keep it square
         if ((dwID >= 4) && (dwID <= 7)) {
            fp fx, fy, fz;
            fx = min(pNew.p[0], 1-pNew.p[0]);
            fy = min(pNew.p[1], 1-pNew.p[1]);
            fz = pNew.p[2];

            DWORD x;
            for (x = 0; x < 4; x++) {
               paControl[x+4].p[0] = (x%2) ? (1-fx) : fx;
               paControl[x+4].p[1] = (x/2) ? (1-fy) : fy;
               paControl[x+4].p[2] = fz;
            }
         }

         if ((dwID >= 8) && (dwID <= 11)) {
            fp fx, fy, fz;
            fx = min(pNew.p[0], 1-pNew.p[0]);
            fy = min(pNew.p[1], 1-pNew.p[1]);
            fz = pNew.p[2];

            DWORD x;
            for (x = 0; x < 4; x++) {
               paControl[x+8].p[0] = (x%2) ? (1-fx) : fx;
               paControl[x+8].p[1] = (x/2) ? (1-fy) : fy;
               paControl[x+8].p[2] = fz;
            }
         }
         break;

      case ROOFSHAPE_SALTBOX:  // salt-box roof
         // default behaviour for edges is (pooint 0..3) is to let them move up and down only
         if ((dwID == 0) || (dwID == 2)) {
            paControl[2-dwID].p[0] = pNew.p[0];
            paControl[2-dwID].p[1] = 1 - pNew.p[1];
            paControl[2-dwID].p[2] = pNew.p[2];
         }
         if ((dwID == 1) || (dwID == 3)) {
            paControl[4-dwID].p[0] = pNew.p[0];
            paControl[4-dwID].p[1] = 1 - pNew.p[1];
            paControl[4-dwID].p[2] = pNew.p[2];
         }
         if ((dwID == 4) || (dwID == 5)) {
            paControl[9-dwID].p[0] = pNew.p[0];
            paControl[9-dwID].p[1] = 1 - pNew.p[1];
            paControl[9-dwID].p[2] = pNew.p[2];
         }
         break;

      case ROOFSHAPE_GABLEFOUR:
         {
            DWORD adwXGroup[12] = {1,2,1,2, 3,4,2,1, 5,6,5,6};
            DWORD adwYGroup[12] = {1,1,2,2, 1,2,3,4, 1,1,2,2};
            DWORD adwZGroup[12] = {1,1,1,1, 2,2,2,2, 1,1,1,1};

            // keep all the x's the same
            fZ = pNew.p[0];
            for (i = 0; i < 12; i++)
               if (adwXGroup[i] == adwXGroup[dwID])
                  paControl[i].p[0] = fZ;

            // keep all the y's the same
            fZ = pNew.p[1];
            for (i = 0; i < 12; i++)
               if (adwYGroup[i] == adwYGroup[dwID])
                  paControl[i].p[1] = fZ;

            // keep all z's the sampe
            fZ = pNew.p[2];
            for (i = 0; i < 12; i++)
               if (adwZGroup[i] == adwZGroup[dwID])
                  paControl[i].p[2] = fZ;
         }
         break;
      case ROOFSHAPE_GABLETHREE:  // three gable
         // keep z consant on back
         if ((dwID == 2) || (dwID == 3)) {
            paControl[5-dwID].p[0] = 1- pNew.p[0];
            paControl[5-dwID].p[1] = pNew.p[1];
            paControl[5-dwID].p[2] = pNew.p[2];

            // front
            paControl[0].p[0] = paControl[2].p[0];
            paControl[1].p[0] = paControl[3].p[0];

            // keep left and right peaks flush
            paControl[7].p[0] = paControl[2].p[0];
            paControl[8].p[0] = paControl[3].p[0];
         }

         // move the front
         if ((dwID == 0) || (dwID == 1)) {
            paControl[1-dwID].p[0] = 1-pNew.p[0];
            paControl[1-dwID].p[1] = pNew.p[1];
            paControl[1-dwID].p[2] = pNew.p[2];

            paControl[4].p[1] = pNew.p[1];   // near, front left
            paControl[4].p[2] = pNew.p[2];   // near, front left
            paControl[5].p[1] = pNew.p[1];   // near, front right
            paControl[5].p[2] = pNew.p[2];   // near, front right

            paControl[6].p[1] = pNew.p[1];   // front peak

            // back flus
            paControl[2].p[0] = paControl[0].p[0];
            paControl[3].p[0] = paControl[1].p[0];

            // keep left and right peaks flush
            paControl[7].p[0] = paControl[2].p[0];
            paControl[8].p[0] = paControl[3].p[0];         }

         if (dwID == 6) {
            // moved front peak
            paControl[0].p[1] = pNew.p[1];
            paControl[1].p[1] = pNew.p[1];
            paControl[4].p[1] = pNew.p[1];
            paControl[5].p[1] = pNew.p[1];

            paControl[7].p[2] = pNew.p[2];
            paControl[8].p[2] = pNew.p[2];
         }

         if ((dwID == 7) || (dwID == 8)) {
            // moved left/right peak
            paControl[15-dwID].p[0] = 1 - pNew.p[0];
            paControl[15-dwID].p[1] = pNew.p[1];
            paControl[15-dwID].p[2] = pNew.p[2];

            // move front peak up
            paControl[6].p[2] = pNew.p[2];
            // back flus
            paControl[2].p[0] = paControl[7].p[0];
            paControl[3].p[0] = paControl[8].p[0];
            paControl[0].p[0] = paControl[7].p[0];
            paControl[1].p[0] = paControl[8].p[0];

         }
         break;
      default:
         break;
      }
   }
   if (m_dwRoofControlFreedom == 1) {   // if it's 1
   }
   if (m_dwRoofControlFreedom == 0) {  // only if it's 0
   }

   // recalc
   AdjustAllSurfaces ();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}



/**********************************************************************************
IntersectionAngle - Given a CSpline that represents a horizontal slice through
a wall, and a point on the spline, this figure out the angle or intersection (basically
the bevel angle for the wall) for each point.

NOTE: Assuming both walls the same thickness.

inputs
   PCSpline       pSpline - spline
   DWORD          dwCorner - Corner point to look at on the spline
   fp         fAngleOutRight - The wall is angled out this much (for the right side)
   fp         fAngleOutLeft - The wall (on the left) is angled out this much.
   fp         *pfAngleLeft - Filled with Left bevel angle (can be passed to CSplineSurface)
   fp         *pfAngleRight - Filled with right bevel angle
   fp         fThickExternal, fThickInteral - External and internal thickness
returns
   BOOL - TRUE if sccueded
*/
BOOL IntersectionAngle (PCSpline pSpline, DWORD dwCorner, fp fAngleOutRight, fp fAngleOutLeft,
                        fp *pfAngleLeft, fp *pfAngleRight,
                        fp fThickExternal, fp fThickInternal)
{
   // since the CSplineSurface function determines its bevelling by the divided edge,
   // do the same... get the angle just to the left and just to the right
   PCPoint pCenter, pLeft, pRight;
   DWORD dwNodes = pSpline->QueryNodes ();
   DWORD dwCenter = dwNodes * dwCorner / pSpline->OrigNumPointsGet();
      // assuming it's looped when calculating dwCenter
   pCenter = pSpline->LocationGet (dwCenter);
   pLeft = pSpline->LocationGet ((dwCenter + dwNodes - 1) % dwNodes);
   pRight = pSpline->LocationGet ((dwCenter + 1) % dwNodes);
   if (!pCenter || !pLeft || !pRight)
      return FALSE;

   // calculate the normals on left and right
   CPoint pUp, pTemp, pNLeft, pNRight;
   pUp.Zero();
   pUp.p[2] = 1;
   pTemp.Subtract (pCenter, pLeft);
   pNLeft.CrossProd (&pUp, &pTemp);
   pNLeft.Normalize();
   pNLeft.Scale (1 / cos(fAngleOutLeft));
   pTemp.Subtract (pRight, pCenter);
   pNRight.CrossProd (&pUp, &pTemp);
   pNRight.Normalize();
   pNRight.Scale (1 / cos(fAngleOutRight));

   // calculate the two points where the outside intersects the outside, and insisde the inside
   CPoint apInter[2];
   DWORD i;
   for (i = 0; i < 2; i++) {
      // left and right line
      CPoint pSideLeft1, pSideLeft2, pSideRight1, pSideRight2;
      CPoint pNLeftScale, pNRightScale;
      
      // take different thicknesses into account
      pNLeftScale.Copy(&pNLeft);
      pNLeftScale.Scale (i ? fThickExternal : fThickInternal);
      pNRightScale.Copy(&pNRight);
      pNRightScale.Scale (i ? fThickExternal : fThickInternal);

      if (i) { // outside
         pSideLeft1.Add (pLeft, &pNLeftScale);
         pSideLeft2.Add (pCenter, &pNLeftScale);
         pSideRight1.Add (pRight, &pNRightScale);
         pSideRight2.Add (pCenter, &pNRightScale);
      }
      else {   // inside
         pSideLeft1.Subtract (pLeft, &pNLeftScale);
         pSideLeft2.Subtract (pCenter, &pNLeftScale);
         pSideRight1.Subtract (pRight, &pNRightScale);
         pSideRight2.Subtract (pCenter, &pNRightScale);
      }

      if (!IntersectLinePlane (&pSideLeft1, &pSideLeft2, &pSideRight1, &pNRight, &apInter[i]))
         return FALSE;  // dont intersect

      // NOTE: If the two lines are parallel then won't intersect, and returns false.
      // this is OK since the calling function then assumes a 0-degree bevel, which
      // is a reasonable assumption.
   }
   
   // convert this to an angle line
   CPoint pInter;
   fp fInter, f;
   pInter.Subtract (&apInter[0], &apInter[1]);
   fInter = -atan2(pInter.p[1], pInter.p[0]);

   // left side
   pTemp.Subtract (pLeft, pCenter);
   f = -atan2(pTemp.p[1], pTemp.p[0]) - PI/2;
   f = fInter - f;
   f = fmod(f + 3 * PI, 2 * PI) - PI; 

   *pfAngleLeft = f;

   // right side
   pTemp.Subtract (pRight, pCenter);
   f = -atan2(pTemp.p[1], pTemp.p[0]) + PI/2;
   f = f - fInter;
   f = fmod(f + 3 * PI, 2 * PI) - PI; 
   *pfAngleRight = f;

   return TRUE;
}


/**********************************************************************************
CObjectBuildBlock::MakeTheWalls - Build the walls of the building block object.
This should be called after MakeTheRoof.

inputs
   fp      fStartAt - Height at which the walls start
   fp      fRoofHeight - Height that they go up to
   fp      fPerimBase - Lowest perimiter point. If this is >= fStartAt (wall base)
                  then no perimiter is drawn.
returns
   none
*/
void CObjectBuildBlock::MakeTheWalls (fp fStartAt, fp fRoofHeight, fp fPerimBase)
{
   // do nothing if no walls
   if (m_dwWallType == WALLSHAPE_NONE)
      return;

   // is there a perimietr?
   BOOL fPerim;
   fPerim = (fPerimBase < fStartAt);

   // calculate where to split the spline up
   CMem  memSplit;
   DWORD dwNum = m_sWall.OrigNumPointsGet();
   if (!memSplit.Required (dwNum * sizeof(DWORD)))
      return;
   DWORD *padwSplit;
   padwSplit = (DWORD*) memSplit.p;
   memset (padwSplit, 0, dwNum * sizeof(DWORD));

   DWORD i, dwCurve;
   for (i = 0; i < dwNum; i++) {
      m_sWall.OrigSegCurveGet (i, &dwCurve);
      switch (dwCurve) {
      case SEGCURVE_CIRCLENEXT:
      case SEGCURVE_ELLIPSENEXT:
         // cant accept the one to the riht
         padwSplit[(i+1)%dwNum] += 2;
         break;
      case SEGCURVE_CIRCLEPREV:
      case SEGCURVE_ELLIPSEPREV:
         // cant accept the one to the left
         padwSplit[i] += 2;
         break;
      case SEGCURVE_CUBIC:
         // cant accept either right or left
         padwSplit[(i+1)%dwNum]++;
         padwSplit[i]++;
         break;
      case SEGCURVE_LINEAR:
      default:
         break;   // can take either side
      }
   }

   // find the number of corners with count 0, and with count 1
   DWORD dwCount0, dwCount1, dwCount2, dwCount3;
   dwCount0 = dwCount1 = dwCount2 = dwCount3 = 0;
   for (i = 0; i < dwNum; i++) {
      if (padwSplit[i] == 0)
         dwCount0++;
      if (padwSplit[i] <= 1)
         dwCount1++;
      if (padwSplit[i] <= 2)
         dwCount2++;
      if (padwSplit[i] <= 3)
         dwCount3++;
   }

   // if 3 or more points for dwCount0 then splitat 0's, else if
   // 3 or more of dwCount1 then split at 1's, else split at 2's
   DWORD dwSplitAt;
   if (dwCount0 >= 2)
      dwSplitAt = 0;
   else if (dwCount1 >= 2)
      dwSplitAt = 1;
   else if (dwCount2 >= 2)
      dwSplitAt = 2;
   else if (dwCount3 >= 2)
      dwSplitAt = 3;
   else
      dwSplitAt = 4;

   // create the spline for the top and bottom
   PCSpline pTop, pBottom;
   pTop = WallSplineAtHeight (fRoofHeight, TRUE);
   pBottom = WallSplineAtHeight (fStartAt, TRUE);
   if (!pTop || !pBottom) {
      if (pTop)
         delete pTop;
      if (pBottom)
         delete pBottom;
      return;
   }

   // perimiter?
   PCSpline pPerimTop, pPerimBottom;
   pPerimTop = pPerimBottom = NULL;
   if (fPerim) {
      CMem memExpand;
      fp *pafExpand;
      memExpand.Required (sizeof(fp) * pBottom->OrigNumPointsGet());
      pafExpand = (fp*) memExpand.p;

      if (pafExpand) {
         for (i = 0; i < pBottom->OrigNumPointsGet(); i++) {
            pafExpand[i] = *((fp*) m_lWallAngle.Get(i));
            pafExpand[i] = 1.0 / cos(pafExpand[i]);
            pafExpand[i] *= (m_fWallThickStruct - m_fBasementThickStruct) / 2;
         }

         // bottom
         pPerimTop = WallSplineAtHeight (fPerimBase, TRUE);
         pPerimBottom = pPerimTop->Expand (pafExpand);
         delete pPerimTop;


         // top
         pPerimTop = pBottom->Expand (pafExpand);
      }
   }
   if (!pPerimTop || !pPerimBottom)
      fPerim = FALSE;


   // note if we can stretch this
   BOOL fCanStretch;
   fCanStretch = (m_dwWallType == WALLSHAPE_RECTANGLE);
   fp fXMax, fXMin, fYMax, fYMin;
   if (fCanStretch) {
      for (i = 0; (i < m_sWall.OrigNumPointsGet()) && fCanStretch; i++) {
         // if angle isn't 0 then can't stretch
         fp fAngle;
         fAngle = *((fp*)m_lWallAngle.Get(i));
         if (fAngle)
            fCanStretch = FALSE;

         CPoint pPoint;
         WallCornerAtHeight (i, 0, &pPoint, TRUE);
         if (i) {
            fXMin = min(fXMin, pPoint.p[0]);
            fXMax = max(fXMax, pPoint.p[0]);
            fYMin = min(fYMin, pPoint.p[1]);
            fYMax = max(fYMax, pPoint.p[1]);
         }
         else {
            fXMin = fXMax = pPoint.p[0];
            fYMin = fYMax = pPoint.p[1];
         }
      }
   }
   if (!fCanStretch)
      m_fOldXMin = m_fOldXMax = m_fOldYMin = m_fOldYMax = 0;

   DWORD dwPerim;
   for (dwPerim = 0; dwPerim < 2; dwPerim++) {
      // if dwPerim==0 then top, else dwPerim==0 then basement perimiter
      // don't do the perimiter unless we're supposed to
      if (dwPerim && !fPerim)
         continue;

      PCSpline pCurTop, pCurBottom;
      if (dwPerim) {
         pCurTop = pPerimTop;
         pCurBottom = pPerimBottom;
      }
      else {
         pCurTop = pTop;
         pCurBottom = pBottom;
      }

      // loop until there's a corner can split at
      DWORD dwStart;
      for (dwStart = 0; dwStart < dwNum; dwStart++)
         if (padwSplit[dwStart] <= dwSplitAt)
            break;

      // wall angles
      fp *pafAngle;
      pafAngle = (fp*) m_lWallAngle.Get(0);

      // temporary memory for points
      CMem memPoints, memCurve;
      PCPoint  papControl;
      DWORD *padwCurve;
      DWORD    dwH, dwV, dwLinear;
      dwLinear = SEGCURVE_LINEAR;
   #undef MESH
   #define  MESH(x,y)   (papControl[(x)+(y)*(dwH)])

      // put all the segments in
      while (TRUE) {
         // find the end
         DWORD dwEnd;
         for (dwEnd = dwStart+1; ; dwEnd++)
            if (padwSplit[dwEnd % dwNum] <= dwSplitAt)
               break;

         // set up info
         BBSURF bbs;
         memset (&bbs, 0, sizeof(bbs));
         wcscpy (bbs.szName, dwPerim ? gszBasement : gszWall);
         bbs.dwMajor = BBSURFMAJOR_WALL;
         bbs.dwType = dwPerim ? 0x10006 : 0x1000A;

         if (dwPerim) {
            bbs.fThickStud = m_fBasementThickStruct;
            bbs.fThickSideA = m_fBasementThickSideA;
            bbs.fThickSideB = m_fBasementThickSideB;
         }
         else {
            bbs.fThickStud = m_fWallThickStruct;
            bbs.fThickSideA = m_fWallThickSideA;
            bbs.fThickSideB = m_fWallThickSideB;
         }
         bbs.fHideEdges = (m_dwRoofType == ROOFSHAPE_NONE) ? FALSE : TRUE;
         if (dwPerim)
            bbs.fHideEdges = TRUE;
         bbs.dwFlags = dwPerim ? 0 : BBSURFFLAG_CLIPAGAINSTROOF;
         bbs.dwMinor = dwStart + (dwPerim ? 1000 : 0);
         bbs.dwStretchVInfo = dwPerim ? 1 : 0;   // moving the top side
         if (dwPerim)
            bbs.fDontIncludeInVolume = !(m_dwFoundation == 4); // only include if basement

         // streching
         bbs.dwStretchHInfo = 0;
         if (fCanStretch) {
            // if get here know it's rectangle and not angled
            switch (dwStart) {
            case 0:  // top (back)
               if ((fabs(m_fOldXMin - fXMin) < EPSILON) && (m_fOldXMax != fXMax))
                  bbs.dwStretchHInfo = 1;   // moving the left hand side
               else if ((m_fOldXMin != fXMin) && (fabs(m_fOldXMax - fXMax) < EPSILON))
                  bbs.dwStretchHInfo = 2;   // moving the right hand side
               break;
            case 1:  // right
               if ((fabs(m_fOldYMin - fYMin) < EPSILON) && (m_fOldYMax != fYMax))
                  bbs.dwStretchHInfo = 2;   // moving the right hand side
               else if ((m_fOldYMin != fYMin) && (fabs(m_fOldYMax - fYMax) < EPSILON))
                  bbs.dwStretchHInfo = 1;   // moving the left hand side
               break;
            case 2:  // bottom (front)
               if ((fabs(m_fOldXMin - fXMin) < EPSILON) && (m_fOldXMax != fXMax))
                  bbs.dwStretchHInfo = 2;   // moving the right hand side
               else if ((m_fOldXMin != fXMin) && (fabs(m_fOldXMax - fXMax) < EPSILON))
                  bbs.dwStretchHInfo = 1;   // moving the left hand side
               break;
            case 3:  // left
               if ((fabs(m_fOldYMin - fYMin) < EPSILON) && (m_fOldYMax != fYMax))
                  bbs.dwStretchHInfo = 1;   // moving the left hand side
               else if ((m_fOldYMin != fYMin) && (fabs(m_fOldYMax - fYMax) < EPSILON))
                  bbs.dwStretchHInfo = 2;   // moving the right hand side
               break;
            }
         }

         // find left and right segments
         DWORD dwLeft, dwRight;
         dwLeft = dwStart;
         dwRight = dwEnd;


         // Determine if need to extend beyond the edge for curved that depend
         // on previous points

         // Dont bother to do this because it's easier to tell the user to have at
         // least a few linear sections
         //if (padwSplit[dwEnd%dwNum] >= 1)
         //   dwRight = dwEnd+1;
         //if (padwSplit[dwStart] >= 1) {
         //   // take one to the left
         //   if (dwStart)
         //      dwLeft = dwStart - 1;
         //   else {
         //      dwLeft = dwStart + dwNum -1;
         //      dwRight += dwNum;
         //   }
         //}

         // calculate the bevel on the bottom
         bbs.fBevelTop = 0;
         for (i = dwStart; i < dwEnd; i++)
            bbs.fBevelTop += pafAngle[i % dwNum];
         bbs.fBevelTop /= (fp) (dwEnd - dwStart);
         bbs.fBevelBottom = -bbs.fBevelTop;

         // how thick is external and internal
         fp fThickExt, fThickInt;
         fThickExt = fThickInt = bbs.fThickStud / 2;
         fThickExt += bbs.fThickSideA;
         fThickInt += bbs.fThickSideB;

         // bevel on left and right sides
         // left side
         fp fJunk;
         IntersectionAngle (pCurBottom, dwStart % dwNum, pafAngle[dwStart%dwNum],
            pafAngle[(dwStart+dwNum-1)%dwNum], &fJunk, &bbs.fBevelRight,
            fThickExt, fThickInt);
         IntersectionAngle (pCurBottom, dwEnd % dwNum, pafAngle[dwEnd%dwNum],
            pafAngle[(dwEnd+dwNum-1)%dwNum], &bbs.fBevelLeft, &fJunk,
            fThickExt, fThickInt);


         if (dwRight - dwLeft > 1) {
            dwH = dwRight - dwLeft + 1;
            dwV = 2;
            if (!memPoints.Required(dwH * dwV * sizeof(CPoint)))
               return;
            papControl = (PCPoint) memPoints.p;
            if (!memCurve.Required(dwH * sizeof(DWORD)))
               return;
            padwCurve = (DWORD*) memCurve.p;
            CPoint t;

            for (i = dwLeft; i <= dwRight; i++) {
               pCurTop->OrigPointGet (i % dwNum, &t);
               MESH(dwRight-i,0).Copy (&t);
               pCurBottom->OrigPointGet (i%dwNum, &t);
               MESH(dwRight-i,1).Copy (&t);

               if (i < dwRight)  {
                  DWORD dwCopyTo;
                  dwCopyTo = dwRight - 1 - i;
                  pCurTop->OrigSegCurveGet (i%dwNum, padwCurve + dwCopyTo);

                  // because flipping direction need to swap circle/ellipse prev/next
                  switch (padwCurve[dwCopyTo]) {
                  case SEGCURVE_CIRCLENEXT:
                     padwCurve[dwCopyTo] = SEGCURVE_CIRCLEPREV;
                     break;
                  case SEGCURVE_CIRCLEPREV:
                     padwCurve[dwCopyTo] = SEGCURVE_CIRCLENEXT;
                     break;
                  case SEGCURVE_ELLIPSENEXT:
                     padwCurve[dwCopyTo] = SEGCURVE_ELLIPSEPREV;
                     break;
                  case SEGCURVE_ELLIPSEPREV:
                     padwCurve[dwCopyTo] = SEGCURVE_ELLIPSENEXT;
                     break;
                  }
               }
            }

            // add it
            WishListAddCurved (&bbs, dwH, dwV, papControl, padwCurve, &dwLinear, 0);
         }
         else {
            // create a single piece of wall
            CPoint b1, b2, t1, t2;
            pCurBottom->OrigPointGet (dwLeft%dwNum, &b1);
            pCurBottom->OrigPointGet (dwRight%dwNum, &b2);
            pCurTop->OrigPointGet (dwLeft%dwNum, &t1);
            pCurTop->OrigPointGet (dwRight%dwNum, &t2);

            // if get here know it's just a flat piece
            WishListAddFlat (&bbs, &b2, &b1, &t2, &t1, 0);
         }

         // NOTE - If had ectensions then would clip out part of extension used to get right shape,
         // but since not doing that functionality dont bother

         // finally, set start to end, unless ended past the beginning
         if (dwEnd >= dwNum)
            break;
         dwStart = dwEnd;
      }
   }

   delete pTop;
   delete pBottom;
   if (pPerimTop)
      delete pPerimTop;
   if (pPerimBottom)
      delete pPerimBottom;

   if (fCanStretch) {
      // keep the old values the same as the new ones so dont
      // try to stretch contained objects
      m_fOldXMin = fXMin;
      m_fOldXMax = fXMax;
      m_fOldYMin = fYMin;
      m_fOldYMax = fYMax;
   }
   else {
      // no values
      m_fOldXMin = 0;
      m_fOldXMax = 0;
      m_fOldYMin = 0;
      m_fOldYMax = 0;
   }
}

/**********************************************************************************
CObjectBuildBlock::WallDefaultSpline - Given a type of wall, creates the
default spline for it, filling the the global, m_sWall.

inputs
   DWORD       dwType - One of WALLSHAPE_XXX
   BOOL        fWall - if TRUE thne it's a wall, FALSE it's a verandah
returns
   none
*/

void CObjectBuildBlock::WallDefaultSpline (DWORD dwType, BOOL fWall)
{
   PCSpline plWallVerandah = fWall ? &m_sWall : &m_sVerandah;
   PCListFixed plAngle = fWall ? &m_lWallAngle : &m_lVerandahAngle;

   CPoint   apWall[8];
   DWORD    adwWallCurve[8];
   DWORD i, dwPoints;
   memset (apWall, 0, sizeof(apWall));
   for (i = 0; i < sizeof(adwWallCurve) / sizeof(DWORD); i++)
      adwWallCurve[i] = SEGCURVE_LINEAR;

   if (fWall)
      m_dwWallType = dwType;
   else
      m_dwVerandahType = dwType;
   switch (dwType) {
   case WALLSHAPE_NONE:
      dwPoints = 0;
      break;

   default:
   case WALLSHAPE_RECTANGLE:
   case WALLSHAPE_ANY:
      apWall[0].p[0] = 0;
      apWall[0].p[1] = 0;
      apWall[1].p[0] = 1;
      apWall[1].p[1] = 0;
      apWall[2].p[0] = 1;
      apWall[2].p[1] = 1;
      apWall[3].p[0] = 0;
      apWall[3].p[1] = 1;
      dwPoints = 4;
      break;

   case WALLSHAPE_CIRCLE:
      adwWallCurve[0] = SEGCURVE_ELLIPSEPREV;
      apWall[0].p[0] = 0;
      apWall[0].p[1] = 0;
      adwWallCurve[1] = SEGCURVE_ELLIPSENEXT;
      apWall[1].p[0] = .5;
      apWall[1].p[1] = 0;
      adwWallCurve[2] = SEGCURVE_ELLIPSEPREV;
      apWall[2].p[0] = 1;
      apWall[2].p[1] = 0;
      adwWallCurve[3] = SEGCURVE_ELLIPSENEXT;
      apWall[3].p[0] = 1;
      apWall[3].p[1] = .5;
      adwWallCurve[4] = SEGCURVE_ELLIPSEPREV;
      apWall[4].p[0] = 1;
      apWall[4].p[1] = 1;
      adwWallCurve[5] = SEGCURVE_ELLIPSENEXT;
      apWall[5].p[0] = .5;
      apWall[5].p[1] = 1;
      adwWallCurve[6] = SEGCURVE_ELLIPSEPREV;
      apWall[6].p[0] = 0;
      apWall[6].p[1] = 1;
      adwWallCurve[7] = SEGCURVE_ELLIPSENEXT;
      apWall[7].p[0] = 0;
      apWall[7].p[1] = .5;
      dwPoints = 8;
      break;

   case WALLSHAPE_SEMICIRCLE:
      adwWallCurve[0] = SEGCURVE_ELLIPSENEXT;
      apWall[0].p[0] = 1;
      apWall[0].p[1] = 0;
      adwWallCurve[1] = SEGCURVE_ELLIPSEPREV;
      apWall[1].p[0] = 1;
      apWall[1].p[1] = 1;
      adwWallCurve[2] = SEGCURVE_ELLIPSENEXT;
      apWall[2].p[0] = .5;
      apWall[2].p[1] = 1;
      adwWallCurve[3] = SEGCURVE_ELLIPSEPREV;
      apWall[3].p[0] = 0;
      apWall[3].p[1] = 1;
      adwWallCurve[4] = SEGCURVE_LINEAR;
      apWall[4].p[0] = 0;
      apWall[4].p[1] = 0;
      dwPoints = 5;
      break;

   case WALLSHAPE_TRIANGLE:
   case WALLSHAPE_PENTAGON:
   case WALLSHAPE_HEXAGON:
      DWORD dwCount;
      if (dwType == WALLSHAPE_TRIANGLE)
         dwCount = 3;
      else if (dwType == WALLSHAPE_PENTAGON)
         dwCount = 5;
      else
         dwCount = 6;
      for (i = 0; i < dwCount; i++) {
         apWall[dwCount-1-i].p[0] = -sin((fp)i / (fp)dwCount * 2 * PI) * .5 + .5;
         apWall[dwCount-1-i].p[1] = -cos((fp)i / (fp)dwCount * 2 * PI) * .5 + .5;
      }
      dwPoints = dwCount;
      break;
   }



   // how much divide
   DWORD dwDivide;
   dwDivide = 0;
   for (i = 0; i < dwPoints; i++)
      if (adwWallCurve[i] != SEGCURVE_LINEAR)
         dwDivide = 3;


   // create the main spline
   if (dwPoints)
      plWallVerandah->Init (TRUE, dwPoints, apWall, NULL, adwWallCurve, dwDivide, dwDivide);

   // create the angles
   plAngle->Clear();
   fp fAngle;
   fAngle = 0;
   //fAngle = 20.0 / 180.0 * PI;
   plAngle->Required (dwPoints);
   for (i = 0; i < dwPoints; i++) {
      plAngle->Add (&fAngle);
   }

   // note if we can stretch this
   BOOL fCanStretch;
   fCanStretch = (dwType == WALLSHAPE_RECTANGLE);
   fp fXMax, fXMin, fYMax, fYMin;
   if (fCanStretch) {
      for (i = 0; (i < plWallVerandah->OrigNumPointsGet()) && fCanStretch; i++) {
         // if angle isn't 0 then can't stretch
         fp fAngle;
         fAngle = *((fp*)plAngle->Get(i));
         if (fAngle)
            fCanStretch = FALSE;

         CPoint pPoint;
         WallCornerAtHeight (i, 0, &pPoint, fWall);
         if (i) {
            fXMin = min(fXMin, pPoint.p[0]);
            fXMax = max(fXMax, pPoint.p[0]);
            fYMin = min(fYMin, pPoint.p[1]);
            fYMax = max(fYMax, pPoint.p[1]);
         }
         else {
            fXMin = fXMax = pPoint.p[0];
            fYMin = fYMax = pPoint.p[1];
         }
      }
   }
   if (!fCanStretch)
      m_fOldXMin = m_fOldXMax = m_fOldYMin = m_fOldYMax = 0;
   else {
      // keep the old values the same as the new ones so dont
      // try to stretch contained objects
      m_fOldXMin = fXMin;
      m_fOldXMax = fXMax;
      m_fOldYMin = fYMin;
      m_fOldYMax = fYMax;
   }
}

/**********************************************************************************
CObjectBuildBlock::WallCornerAtHeight - Given a height (1/2xm_fHeight is the basic spline height)
fills in a point with the corner location at that height. This takes into account
walls angling out/in. (Verandah at 0 height).

inputs
   DWORD       dwCorner - From 0..dwNum-1, where dwNum is # of points in splien
   fp      fHeight - Height
   CPoint      pPoint - Filled in the location in object space.
   BOOL        fWall - TRUE if it's a wall, FALSE if it'sa  verandah
returns
   BOOL - TRUE if success
*/
BOOL CObjectBuildBlock::WallCornerAtHeight (DWORD dwCorner, fp fHeight, CPoint *pPoint, BOOL fWall)
{
   PCSpline plWallVerandah = fWall ? &m_sWall : &m_sVerandah;
   PCListFixed plAngle = fWall ? &m_lWallAngle : &m_lVerandahAngle;

   // get the tangents on ether side
   PCPoint pTanLeft, pTanRight;
   CPoint pOrig;
   DWORD dwCurPoints, dwOrigPoints;
   dwCurPoints = plWallVerandah->QueryNodes();
   dwOrigPoints = plWallVerandah->OrigNumPointsGet();
   pTanLeft = plWallVerandah->TangentGet (dwCorner * dwCurPoints / dwOrigPoints, FALSE);
   pTanRight = plWallVerandah->TangentGet (dwCorner * dwCurPoints / dwOrigPoints, TRUE);
      // assuming that looped when doing /dwOrigPoints
   if (!pTanLeft || !pTanRight)
      return FALSE;
   if (!plWallVerandah->OrigPointGet (dwCorner, &pOrig))
      return FALSE;

   // convert these to object space
   CPoint pTLObject, pTRObject, pObject;
   pObject.Zero();
   pObject.p[0] = pOrig.p[0] * (m_fXMax - m_fXMin) + m_fXMin;
   pObject.p[1] = pOrig.p[1] * (m_fYMin - m_fYMax) + m_fYMax;  // notice the flip here
   pTLObject.Zero();
   pTLObject.p[0] = (pTanLeft->p[0] + pOrig.p[0]) * (m_fXMax - m_fXMin) + m_fXMin;
   pTLObject.p[1] = (pTanLeft->p[1] + pOrig.p[1]) * (m_fYMin - m_fYMax) + m_fYMax;  // notice the flip here
   pTLObject.Subtract (&pObject);
   pTRObject.Zero();
   pTRObject.p[0] = (pTanRight->p[0] + pOrig.p[0]) * (m_fXMax - m_fXMin) + m_fXMin;
   pTRObject.p[1] = (pTanRight->p[1] + pOrig.p[1]) * (m_fYMin - m_fYMax) + m_fYMax;  // notice the flip here
   pTRObject.Subtract (&pObject);

   // cross product tangent with up to get normal
   CPoint pUp, pNormLeft, pNormRight;
   pUp.Zero();
   pUp.p[2] = 1;
   pNormLeft.CrossProd (&pUp, &pTLObject);
   pNormLeft.Normalize();
   pNormRight.CrossProd (&pUp, &pTRObject);
   pNormRight.Normalize();

   // distance from m_fHeight/2
   fp fDist;
   fDist = fHeight;
   if (fWall)
      fDist -= (m_fHeight/2);

   // angle
   fp fAngleLeft, fAngleRight;
   if (plAngle->Num()) {
      fAngleLeft = *((fp*) plAngle->Get((dwCorner + plAngle->Num() - 1) % plAngle->Num()));
      fAngleRight = *((fp*) plAngle->Get(dwCorner % plAngle->Num()));
   }
   else {
      // BUGFIX - Fail gracefully
      fAngleLeft = fAngleRight = 0;
   }
   fAngleLeft = max(fAngleLeft, -PI/2 * .95);
   fAngleLeft = min(fAngleLeft, PI/2 * .95);
   fAngleRight = max(fAngleRight, -PI/2 * .95);
   fAngleRight = min(fAngleRight, PI/2 * .95);

   // scale the distance by f(x) of the angle between pNormLeft and pNormRight - which
   // means that if they're parallel then don't do any scaling, and maximum
   // if parallele - fDist *= .5 (half from each)
   // if 90degrees - fDist *= 1
   // if 180 degrees - fDist *= infinity
   // NOTE: I think I finally have this, and could use the same function elsewhere
   // it's 1.0 / ( (1 + cos(theta)) * cos(theta/2) )
   // 1/cos(theta/2) is length of line between two edges, 1/(1+cos(theta)) ensures 1/2
   // contribution for each when normals parallel, 1x when perpendicular
   CPoint pHalf;
   pHalf.Add (&pNormLeft, &pNormRight);
   fp fLen;
   fLen = pHalf.Length();
   if (fabs((fp)fLen) > EPSILON) { // BUGFIX - was fabs(fLen > EPSILON)
      pHalf.Scale (1.0 / fLen);
      fp fDot1, fDot2;
      fDot1 = pHalf.DotProd (&pNormRight);
      fDot2 = sqrt(2 * (pNormLeft.DotProd (&pNormRight) + 1));
      if ((fabs(fDot1) > EPSILON) && (fDot2 > EPSILON))
         fDist = fDist / (fDot1 * fDot2);
   }
   else
      fDist *= 0;

   // scale
   if (fAngleLeft)
      pNormLeft.Scale (tan(fAngleLeft) * fDist);
   else
      pNormLeft.Zero();
   if (fAngleRight)
      pNormRight.Scale (tan(fAngleRight) * fDist);
   else
      pNormRight.Zero();

   // add the two to the point
   pObject.Add (&pNormLeft);
   pObject.Add (&pNormRight);
   pObject.p[2] = fHeight;

   // done
   pPoint->Copy (&pObject);
   return TRUE;
}

/**********************************************************************************
CObjectBuildBlock::WallSplineAtHeight - Given a height (0 is the basic spline height)
this returns a new CSpline (that must be freed) describing what the wall looks
like at the height. It takes into account the angle of the wall.

NOTE: The returned spline's coordinates are in object space. The global m_sWall's
coordinates are in 0..1 to 0..1.

inputs
   fp      fHeight - Height
   BOOL        fWall - TRUE if its a wall, FALSE if verandah
returns
   PCSpline - New spline. Must be freed by the caller.
*/
PCSpline CObjectBuildBlock::WallSplineAtHeight (fp fHeight, BOOL fWall)
{
   PCSpline plWallVerandah = fWall ? &m_sWall : &m_sVerandah;

   CListFixed lCorner, lCurve;
   lCorner.Init (sizeof(CPoint));
   lCurve.Init (sizeof(DWORD));

   DWORD i;
   CPoint pCorner;
   DWORD dwCurve;
   lCorner.Required (plWallVerandah->OrigNumPointsGet());
   lCurve.Required (plWallVerandah->OrigNumPointsGet());
   for (i = 0; i < plWallVerandah->OrigNumPointsGet(); i++) {
      WallCornerAtHeight (i, fHeight, &pCorner, fWall);
      lCorner.Add (&pCorner);

      plWallVerandah->OrigSegCurveGet (i, &dwCurve);
      lCurve.Add (&dwCurve);
   }

   DWORD dwMin, dwMax;
   fp fDetail;
   plWallVerandah->DivideGet (&dwMin, &dwMax, &fDetail);

   // create new spline
   PCSpline pNew;
   pNew = new CSpline;
   if (!pNew)
      return NULL;
   if (!pNew->Init (TRUE, lCorner.Num(), (PCPoint) lCorner.Get(0), NULL, (DWORD*) lCurve.Get(0),
      dwMin, dwMax, fDetail)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}

/*************************************************************************************
CObjectBuildBlock::WallControlPointQuery - Called to query the information about a control
point identified by dwID.

inputs
   DWORD       dwID - ID
   POSCONTROL  pInfo - Filled in with the infomration
   BOOL        fWall - TRUE if it's a wall, FALSE if its a verandah
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBuildBlock::WallControlPointQuery (DWORD dwID, POSCONTROL pInfo, BOOL fWall)
{
   DWORD dwType = fWall ? m_dwWallType : m_dwVerandahType;
   PCSpline plWallVerandah = fWall ? &m_sWall : &m_sVerandah;

   if (dwType == WALLSHAPE_NONE)
      return FALSE;
   if (dwID >= plWallVerandah->OrigNumPointsGet())
      return FALSE;

   CPoint pPosn;
   if (!WallCornerAtHeight (dwID, fWall ? (m_fHeight/2) : 0, &pPosn, fWall))
      return FALSE;

   fp fKnobSize = .3;

   memset (pInfo,0, sizeof(*pInfo));

   pInfo->dwID = dwID;
//   pInfo->dwFreedom = 0;   // any direction
   pInfo->dwStyle = CPSTYLE_CUBE;
   pInfo->fSize = fKnobSize;
   pInfo->cColor = RGB(0xff, 0xff, 0);
   wcscpy (pInfo->szName, L"Wall");

   pInfo->pLocation.Copy (&pPosn);

   return TRUE;
}


/*************************************************************************************
CObjectBuildBlock::WallControlPointSet - Called to change the valud of a control point.

inputs
   DWORD       dwID - ID
   PCPoint     pVal - Contains the new location, in object coordinates
   BOOL        fWall - TRUE its a wall, FALSE ists a verandah
returns
   BOOL - TRUE if successful
*/
BOOL CObjectBuildBlock::WallControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer, BOOL fWall)
{
   DWORD dwType = fWall ? m_dwWallType : m_dwVerandahType;
   PCSpline plWallVerandah = fWall ? &m_sWall : &m_sVerandah;

   if (dwType == WALLSHAPE_NONE)
      return FALSE;
   if (dwID >= plWallVerandah->OrigNumPointsGet())
      return FALSE;

   // get current values
   CMem  memPoints, memCurve;
   PCPoint papPoints;
   DWORD *padwCurve;
   DWORD dwNum, i, dwMin, dwMax;
   fp fDetail;
   dwNum = plWallVerandah->OrigNumPointsGet();
   if (!memPoints.Required(dwNum * sizeof(CPoint)))
      return FALSE;
   if (!memCurve.Required(dwNum*sizeof(DWORD)))
      return FALSE;
   papPoints = (PCPoint) memPoints.p;
   padwCurve = (DWORD*) memCurve.p;
   for (i = 0; i < dwNum; i++) {
      plWallVerandah->OrigPointGet (i, papPoints + i);
      plWallVerandah->OrigSegCurveGet (i, padwCurve + i);
   }
   plWallVerandah->DivideGet (&dwMin, &dwMax, &fDetail);

   // convert the current setting into HV
   CPoint pCur;
   pCur.Zero();
   pCur.p[0] = (pVal->p[0] - m_fXMin) / (m_fXMax - m_fXMin);
   pCur.p[1] = (pVal->p[1] - m_fYMax) / (m_fYMin - m_fYMax);   // note the swap
   pCur.p[0] = min(1, pCur.p[0]);
   pCur.p[0] = max(0, pCur.p[0]);
   pCur.p[1] = min(1, pCur.p[1]);
   pCur.p[1] = max(0, pCur.p[1]);

   // remember this
   papPoints[dwID].Copy (&pCur);

   // move other points in lock step
   switch (dwType) {
   case WALLSHAPE_RECTANGLE:
      // keep x's level
      if ((dwID == 0) || (dwID == 3))
         papPoints[3-dwID].p[0] = pCur.p[0];
      if ((dwID == 1) || (dwID == 2))
         papPoints[3-dwID].p[0] = pCur.p[0];

      // keep y's level
      if ((dwID == 0) || (dwID == 1))
         papPoints[1-dwID].p[1] = pCur.p[1];
      if ((dwID == 2) || (dwID == 3))
         papPoints[5-dwID].p[1] = pCur.p[1];
      break;

   case WALLSHAPE_CIRCLE:
      {
         fp fXMin, fXMax, fYMin, fYMax;
         for (i = 0; i < dwNum; i++) {
            if (i) {
               fXMin = min(fXMin, papPoints[i].p[0]);
               fXMax = max(fXMax, papPoints[i].p[0]);
               fYMin = min(fYMin, papPoints[i].p[1]);
               fYMax = max(fYMax, papPoints[i].p[1]);
            }
            else {
               fXMin = fXMax = papPoints[i].p[0];
               fYMin = fYMax = papPoints[i].p[1];
            }
         }
         
         // change x's
         switch (dwID) {
         case 0:  // UL
         case 6:  // LL
         case 7:  // L
            fXMin = pCur.p[0];
            break;
         case 2:  // UR
         case 3:  // R
         case 4:  // LR
            fXMax = pCur.p[0];
            break;
         }

         // change y's
         switch (dwID) {
         case 0:  // UL
         case 1:  // U
         case 2:  // UR
            fYMin = pCur.p[1];
            break;
         case 4:  // LR
         case 5:  // L
         case 6:  // LL
            fYMax = pCur.p[1];
            break;
         }

         papPoints[0].p[0] = fXMin;
         papPoints[0].p[1] = fYMin;

         papPoints[1].p[0] = (fXMin + fXMax) / 2;
         papPoints[1].p[1] = fYMin;

         papPoints[2].p[0] = fXMax;
         papPoints[2].p[1] = fYMin;

         papPoints[3].p[0] = fXMax;
         papPoints[3].p[1] = (fYMin + fYMax) / 2;

         papPoints[4].p[0] = fXMax;
         papPoints[4].p[1] = fYMax;

         papPoints[5].p[0] = (fXMin + fXMax) / 2;
         papPoints[5].p[1] = fYMax;

         papPoints[6].p[0] = fXMin;
         papPoints[6].p[1] = fYMax;

         papPoints[7].p[0] = fXMin;
         papPoints[7].p[1] = (fYMin + fYMax) / 2;
      }
      break;

   case WALLSHAPE_SEMICIRCLE:
      {
         fp fXMin, fXMax, fYMin, fYMax;
         for (i = 0; i < dwNum; i++) {
            if (i) {
               fXMin = min(fXMin, papPoints[i].p[0]);
               fXMax = max(fXMax, papPoints[i].p[0]);
               fYMin = min(fYMin, papPoints[i].p[1]);
               fYMax = max(fYMax, papPoints[i].p[1]);
            }
            else {
               fXMin = fXMax = papPoints[i].p[0];
               fYMin = fYMax = papPoints[i].p[1];
            }
         }
         
         // change x's
         switch (dwID) {
         case 3:  // LL
         case 4:  // UL
            fXMin = pCur.p[0];
            break;
         case 0:  // UR
         case 1:  // LR
            fXMax = pCur.p[0];
            break;
         }

         // change y's
         switch (dwID) {
         case 0:  // UR
         case 4:  // UL
            fYMin = pCur.p[1];
            break;
         case 1:  // LR
         case 2:  // LC
         case 3:  // LL
            fYMax = pCur.p[1];
            break;
         }

         papPoints[0].p[0] = fXMax;
         papPoints[0].p[1] = fYMin;

         papPoints[1].p[0] = fXMax;
         papPoints[1].p[1] = fYMax;
         
         papPoints[2].p[0] = (fXMin + fXMax) / 2;
         papPoints[2].p[1] = fYMax;
         
         papPoints[3].p[0] = fXMin;
         papPoints[3].p[1] = fYMax;
         
         papPoints[4].p[0] = fXMin;
         papPoints[4].p[1] = fYMin;
      }
      break;

   case WALLSHAPE_TRIANGLE:
   case WALLSHAPE_PENTAGON:
   case WALLSHAPE_HEXAGON:
      {
         fp fDist;
         fDist = sqrt((pCur.p[0] - .5) * (pCur.p[0] - .5) + (pCur.p[1] - .5) * (pCur.p[1] - .5));

         DWORD dwCount;
         if (dwType == WALLSHAPE_TRIANGLE)
            dwCount = 3;
         else if (dwType == WALLSHAPE_PENTAGON)
            dwCount = 5;
         else
            dwCount = 6;
         for (i = 0; i < dwCount; i++) {
            papPoints[dwCount-1-i].p[0] = -sin((fp)i / (fp)dwCount * 2 * PI) * fDist + .5;
            papPoints[dwCount-1-i].p[1] = -cos((fp)i / (fp)dwCount * 2 * PI) * fDist + .5;
         }
      }
      break;
   }

   // tell the world we're about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // store it away
   plWallVerandah->Init (TRUE, dwNum, papPoints, NULL, padwCurve, dwMin, dwMax, fDetail);

   // recalc
   AdjustAllSurfaces ();

   // tell the world we've changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   return TRUE;
}


/********************************************************************************
CutoutBasedOnSpline - This takes a spline (in object coordinates) and cuts the
shape out of a CDoubleSurface. The CDoubleSurface needs to be flat (no warping
either) for this to work.

inputs
   PCDoubleSurface      pSurf - Surface
   PCSpline             pSpline - Spline to use. Coordinates are in object coords.
                           If this is NULL then the edge/cutout is restored. Goes
                           clockwise when looking at side A of the surface.
   BOOL                 fCutout - If TRUE its a cutout is used. If FALSE then
                           it sets the edge of the surface.
returns
   BOOL - TRUE if success
*/
BOOL CutoutBasedOnSpline (PCDoubleSurface pSurf, PCSpline pSpline, BOOL fCutout)
{
   PWSTR pszSplineCutout = L"SplineCutout";

   // NOTE: Assuming pSurf is 2x2 and linear
   if ((pSurf->ControlNumGet(TRUE) != 2) || (pSurf->ControlNumGet(FALSE) != 2))
      return FALSE;
   if ((pSurf->SegCurveGet (TRUE, 0) != SEGCURVE_LINEAR) || (pSurf->SegCurveGet (FALSE, 0) != SEGCURVE_LINEAR))
      return FALSE;
   CPoint pMin, pMax, pCur;
   DWORD i;
   pMax.Zero();   // BUGFIX - Put these in so dont get runtime errors
   pMin.Zero();
   for (i = 0; i < 4; i++) {
      pSurf->m_SplineCenter.ControlPointGet (i % 2, i / 2, &pCur);
      if (i) {
         pMin.Min (&pCur);
         pMax.Max (&pCur);
      }
      else {
         pMin.Copy (&pCur);
         pMax.Copy (&pCur);
      }
   }
   if (fabs(pMax.p[1] - pMin.p[1]) > EPSILON)
      return FALSE;
   if ((fabs(pMax.p[0] - pMin.p[0]) < EPSILON) ||(fabs(pMax.p[2] - pMin.p[2]) < EPSILON))
      return FALSE;

   // if there's no spline then restore what's there
   if (!pSpline) {
      if (fCutout) {
         pSurf->m_SplineA.CutoutRemove (pszSplineCutout);
         pSurf->m_SplineB.CutoutRemove (pszSplineCutout);
      }
      else {
         // edge
         CPoint paEdge[4];
         paEdge[0].Zero();
         paEdge[1].Copy (&paEdge[0]);
         paEdge[1].p[0] = 1;
         paEdge[2].Copy (&paEdge[1]);
         paEdge[2].p[1] = 1;
         paEdge[3].Copy (&paEdge[1]);
         paEdge[3].p[0] = 0;

         pSurf->EdgeInit (TRUE, 4, paEdge, NULL, (DWORD*)SEGCURVE_LINEAR, 0, 0, .1);
      }

      return TRUE;
   }

   // get the points of the spline
   CMem  memPoints;
   CMem  memSegCurve;
   DWORD dwOrig;
   dwOrig = pSpline->OrigNumPointsGet();
   if (!memPoints.Required ((dwOrig+1) * sizeof(CPoint)))
      return TRUE;
   if (!memSegCurve.Required ((dwOrig+1) * sizeof(DWORD)))
      return TRUE;

   // load it in
   PCPoint paPoints;
   DWORD *padw;
   paPoints = (PCPoint) memPoints.p;
   padw = (DWORD*) memSegCurve.p;
   for (i = 0; i < dwOrig; i++) {
      pSpline->OrigPointGet (i, &paPoints[i]);
      pSpline->OrigSegCurveGet (i, &padw[i]);
      paPoints[i].p[3] = 1;
   }
   DWORD dwMinDivide, dwMaxDivide;
   fp fDetail;
   BOOL fLooped;
   pSpline->DivideGet (&dwMinDivide, &dwMaxDivide, &fDetail);
   fLooped = pSpline->QueryLooped ();


   // translate to object space
   CMatrix m, mInv;
   pSurf->MatrixGet (&m);
   m.Invert4 (&mInv);

   for (i = 0; i < dwOrig; i++)
      paPoints[i].MultiplyLeft (&mInv);

   // convert to HV
   fp fDeltaH, fDeltaV;
   fDeltaH = pMax.p[0] - pMin.p[0];
   fDeltaV = pMax.p[2] - pMin.p[2];
   for (i = 0; i < dwOrig; i++) {
      paPoints[i].p[0] = (paPoints[i].p[0] - pMin.p[0]) / fDeltaH;
      paPoints[i].p[1] = 1 - ((paPoints[i].p[2] - pMin.p[2]) / fDeltaV);
      paPoints[i].p[2] = 0;

      paPoints[i].p[0] = min(1, paPoints[i].p[0]);
      paPoints[i].p[0] = max(0, paPoints[i].p[0]);
      paPoints[i].p[1] = min(1, paPoints[i].p[1]);
      paPoints[i].p[1] = max(0, paPoints[i].p[1]);

   }

   // write it out
   if (fCutout) {
      pSurf->CutoutFromSpline (pszSplineCutout, dwOrig, paPoints, padw,
         dwMinDivide, dwMaxDivide, fDetail, TRUE);
   }
   else {
      // set the edge
      pSurf->EdgeInit (fLooped, dwOrig, paPoints, NULL, padw, dwMinDivide,
         dwMaxDivide, fDetail);
   }


   return TRUE;
}


/*****************************************************************************
CObjectBuildBlock::BalustradeReset - Resets all the fTouched flags to FALSE
for the balustrade.
*/
void CObjectBuildBlock::BalustradeReset (void)
{
   DWORD i;
   PBBBALUSTRADE pbb;
   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (i);
      pbb->fTouched = FALSE;
   }
}

/*****************************************************************************
CObjectBuildBlock::BalustradeAdd - Adds (or sets if it already exists) the
balustrade.

inputs
   DWORD       dwMajor - BBBALMAJOR_XXX
   DWORD       dwMinor - Minor ID. Same as the ID of the floor that created
   fp      fHeightTop - Height of the top
   fp      fHeightBottom - Height of the bottom
   BOOL        fWall - If TRUE, use the wall shape, FALSE use the verandah shape
   PCSplineSurface pss - If not NULL, use this to cutout of the surface
   PCMatrix    pm - If not NULL, this is the matrix that translates from pss coords
               to the balustrade coords
returns
   BOOL - TRUE if success
*/
BOOL CObjectBuildBlock::BalustradeAdd (DWORD dwMajor, DWORD dwMinor, fp fHeightTop,
                                       fp fHeightBottom, BOOL fWall, PCSplineSurface pss, PCMatrix pm)
{
   DWORD i;
   PBBBALUSTRADE pbb;
   BOOL fExisted = FALSE;
   for (i = 0; i < m_lBBBALUSTRADE.Num(); i++) {
      pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (i);
      if ((pbb->dwMajor == dwMajor) && (pbb->dwMinor == dwMinor)) {
         fExisted = TRUE;
         break;
      }
   }
   if (i >= m_lBBBALUSTRADE.Num()) {
      BBBALUSTRADE bb;
      memset (&bb, 0, sizeof(bb));
      bb.dwMajor = dwMajor;
      bb.dwMinor = dwMinor;
      if (dwMajor == BBBALMAJOR_BALUSTRADE) {
         bb.pBal = new CBalustrade;
         if (!bb.pBal)
            return FALSE;
         bb.pBal->m_dwDisplayControl = 1; // make sure to show openings
      }
      else if (dwMajor == BBBALMAJOR_PIERS) {
         bb.pPiers = new CPiers;
         if (!bb.pPiers)
            return FALSE;
         bb.pPiers->m_dwDisplayControl = 1; // make sure to show openings
      }
      m_lBBBALUSTRADE.Add (&bb);
      pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (m_lBBBALUSTRADE.Num()-1);
      if (!pbb)
         return FALSE;
   }

   pbb->fTouched = TRUE;

   PCSpline pBottom, pTop;
   pBottom = WallSplineAtHeight (fHeightBottom, fWall);
   pTop = WallSplineAtHeight (fHeightTop, fWall);

   if (pbb->pBal) {
      if (fExisted)
         pbb->pBal->NewSplines (pBottom, pTop); // BUGFIX - If already exists dont call init. otherwise lose settings
      else
         pbb->pBal->Init (m_OSINFO.dwRenderShard, 0, this, pBottom, pTop, TRUE);

      // if there's a spline surface the create cutouts based on it
      pbb->pBal->CutoutBasedOnSurface (pss, pm);
   }
   else if (pbb->pPiers) {
      if (fExisted)
         pbb->pPiers->NewSplines (m_OSINFO.dwRenderShard, pBottom, pTop); // BUGFIX - If already exists dont call init. otherwise lose settings
      else
         pbb->pPiers->Init (m_OSINFO.dwRenderShard, 0, this, pBottom, pTop, TRUE);

      // if there's a spline surface the create cutouts based on it
      pbb->pPiers->CutoutBasedOnSurface (m_OSINFO.dwRenderShard, pss, pm);
   }

   delete pBottom;
   delete pTop;


   return TRUE;
}


/*****************************************************************************
CObjectBuildBlock::BalustradeExtendToRoof - Extend the balustrades to the roof
line.
*/
void CObjectBuildBlock::BalustradeExtendToRoof (void)
{
   CMatrix mInv;
   DWORD i;
   CListFixed lMatrix, lPSS;
   lMatrix.Init (sizeof(CMatrix));
   lPSS.Init (sizeof(PCSplineSurface));
   if (m_pWorld) {
      // find what parts of the world this intersects with
      // get the bounding box for this. I know that because of tests in render
      // it will return even already clipped stuff
      DWORD dwThis;
      dwThis = m_pWorld->ObjectFind (&m_gGUID);
      CPoint pCorner1, pCorner2;
      CMatrix m;
      m_pWorld->BoundingBoxGet (dwThis, &m, &pCorner1, &pCorner2);
      m.Invert4 (&mInv);

      // find out what objects this intersects with
      CListFixed     lObjects;
      lObjects.Init(sizeof(DWORD));
      m_pWorld->IntersectBoundingBox (&m, &pCorner1, &pCorner2, &lObjects);


      // go through those objects and get the surfaces
      PCObjectSocket pos;
      for (i = 0; i < lObjects.Num(); i++) {
         DWORD dwOn = *((DWORD*)lObjects.Get(i));
         if (dwOn == dwThis)
            continue;
         pos = m_pWorld->ObjectGet (dwOn);
         if (!pos)
            continue;

         OSMSPLINESURFACEGET q;
         memset (&q, 0, sizeof(q));
         q.dwType = 0;
         q.pListMatrix = &lMatrix;
         q.pListPSS = &lPSS;
         pos->Message (OSM_SPLINESURFACEGET, &q);
         // if it liked the message will have added on surface
      }
   }

   // ask self
   OSMSPLINESURFACEGET q;
   memset (&q, 0, sizeof(q));
   q.dwType = 0;
   q.pListMatrix = &lMatrix;
   q.pListPSS = &lPSS;
   Message (OSM_SPLINESURFACEGET, &q);


   // loop through all the matricies and multiply them by the inverse of this object's
   // matrix, so all matrices convert to object space
   m_MatrixObject.Invert4 (&mInv);
   PCMatrix pm;
   pm = (PCMatrix) lMatrix.Get(0);
   for (i = 0; i < lMatrix.Num(); i++)
      pm[i].MultiplyRight (&mInv);

   // loop through all the balustrades and extend the posts
   PBBBALUSTRADE pbb;
   for (i = 0;  i < m_lBBBALUSTRADE.Num(); i++) {
      pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (i);
      if (pbb->pBal)
         pbb->pBal->ExtendPostsToRoof (lMatrix.Num(), (PCSplineSurface*) lPSS.Get(0), pm);
      else if (pbb->pPiers)
         pbb->pPiers->ExtendPostsToGround(m_OSINFO.dwRenderShard);
   }
}

/*****************************************************************************
CObjectBuildBlock::BalustradeRemoveDead - Removes balustrades that weren't touched
this go-around.
*/
void CObjectBuildBlock::BalustradeRemoveDead (void)
{
   DWORD i;
   PBBBALUSTRADE pbb;
   for (i = m_lBBBALUSTRADE.Num() - 1; i < m_lBBBALUSTRADE.Num(); i--) {
      pbb = (PBBBALUSTRADE) m_lBBBALUSTRADE.Get (i);
      if (!pbb->fTouched) {
         if (pbb->pBal) {
            pbb->pBal->ClaimClear();
            delete pbb->pBal;
         }
         if (pbb->pPiers) {
            pbb->pPiers->ClaimClear();
            delete pbb->pPiers;
         }
         m_lBBBALUSTRADE.Remove (i);
      }
   }
}


/*****************************************************************************
CObjectBuildBlock::ConvertRoofPoints - Go through all the control points stored
away for the roof and adjust their Z value as change from one scaling system (defined
by m_dwRoofHeight) to another one.

inputs
   DWORD       dwOrig - Original scaling system. This can be different than m_dwRoofHeight
   DWORD       dwNew - New scaling system. NOTE: This is NOT written to m_dwRoofHeight.
returns
   none
*/
void CObjectBuildBlock::ConvertRoofPoints (DWORD dwOrig, DWORD dwNew)
{
   // store away current settings
   DWORD dwCur = m_dwRoofHeight;

   if (dwOrig == dwNew)
      return;  // no change

   // create two heights in the old scale and convet them
   CPoint apOrig[2], apNew[2];
   apOrig[0].Zero();
   apOrig[1].Zero();
   apOrig[1].p[2] = 1;
   memcpy (apNew, apOrig, sizeof(apNew));

   m_dwRoofHeight = dwOrig;
   RoofControlToObject (apOrig, 2);

   m_dwRoofHeight = dwNew;
   RoofControlToObject (apNew, 2);

   // since know that roofcontroltoobject is a linear operation, adjust all
   // the existing Z values
   PCPoint  pr;
   DWORD i;
   pr = (PCPoint) m_lRoofControl.Get(0);
   for (i = 0; i < m_lRoofControl.Num(); i++, pr++) {
      // convert to meters
      pr->p[2] = pr->p[2] * (apOrig[1].p[2] - apOrig[0].p[2]) + apOrig[0].p[2];

      // convert to new space
      pr->p[2] = (pr->p[2] - apNew[0].p[2]) / (apNew[1].p[2] - apNew[0].p[2]);
   }


   // restore setting
   m_dwRoofHeight = dwCur;
}

// FUTURERELEASE - In the far future may want a way to merge floors
// from different building blocks together. But not now.

// FUTURERELEASE - Way to specify the edge or a roof in building blocks.
// So can have flat, circular roof.

// FUTURERELEASE - Option for crenelations if on top wall

// FUTURERELEASE - If intersect two Build Blocks with piers, right now piers are cut out
// from where they interesct. Really, should keep the piers from the major building block
// and remove the ones from the minor.

// NOT REPRO - Seems to say "Balustrade level 2" and "Balustrade level 4" for a 2-story building

// NOT GOING TO FIX - In EA house building block wall, added an overlay left of the location clicked
// (so can do voerlay inside kitchen). Created overlay but didn't seem to show drag point
// when selected. - Happened because control point was displayed above the floor level.
// No easy way to universally fix this problem

// NOT GOING TO FIX - After made fix so that floor/ceiling would be properly clipped against roof
// if floor ceiling goes into roof area, get a strange artifact on one section of EA house
// (back attic) where a long strip of infinitely thin ceiling appears outside the roof,
// about 1' below peak. Got rid of by checking "cathedral ceilings". Not too worried
// about at the moment but watch out for later.


// FUTURERELEASE - Way to have roof of BB trim edges to outline of walls, in case to custom walls
// underneath? So can do back of EA house in one go.

// FUTURERELEASE - Have push/pull knobs on any-shape wall that pull out perpendicular to the wall.

// FUTURERELEASE - Have an any-shape roof - which is either based on the walls and has gable
// vs. hip roof, or allows lines to be dragged across to make the ridges.

// FUTURERELEASE - Roof-shape stuff like do in other building programs. Specify which ends are
// gables and hips, angle of roof, etc. and it calculates the roof. Make this an
// "any shaped" roof.


// BUGBUG - Building eagle eye bedroom wing. Put 10 degree angle in. The walls on either
// side (from flat view above) look to be bent in somehow. - Only happens after switch
// walls to any-side and the move corners.



// BUGBUG - When make sure floors don't go above roof level, calculate based on the lowest
// point of the roof (eaves), not the height of the roof intersecting the walls.
// This causes problems with steep roofs.


// BUGBUG - When go into attributes and rotate builing around Y axis seems to hang -
// not sure why

// BUGBUG - May have broken the automatic detection of what type of building
// block to use (tropical, board and batten, etc.) based on language settings
// NOTE: It doesn't seem to be broken; I think I deleted my registry keys

// BUGBUG - Created elevated building with angled walls. Applied cinder-block
// textures, but texture seemed to be squashed improperly and too small


