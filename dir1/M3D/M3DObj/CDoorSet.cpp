/**********************************************************************************
CDoorSet.cpp - File to draw sets of doors, windows, etc.

begun 27/4/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/**********************************************************************************
CDoorSet::Constructor and destructor
*/
CDoorSet::CDoorSet (void)
{
   m_lShape.Init (sizeof(CPoint));
   m_fThickness = CM_DOORTHICKNESS;
   m_fFixed = TRUE;
   m_fDivisions = 1;
   m_dwOrient = DSORIENT_LEFT;
   m_dwMove = DSMOVE_HINGED;
   m_fOpenMin = 0;
   m_fOpenMax = 1;
   m_fCustomAlternate = TRUE;
   m_dwCustomShape = 0;
   m_tCustomMinMax.h = .25;
   m_tCustomMinMax.v = .75;
   memset (m_adwDKStyle, 0, sizeof(m_adwDKStyle));
   memset (m_afKick, 0, sizeof(m_afKick));
   m_tDKLoc.h = CM_DOORKNOBINSIDE;
   m_tDKLoc.v = CM_DOOKNOBHEIGHT;
   m_tKickLoc.h = .05;
   m_tKickLoc.v = .2;
   m_fKnobRotate = 0;

   m_fDirty = TRUE;
   m_lDoors.Init (sizeof(PCDoor));
   m_dwDoors = 1;
   m_pShapeMin.Zero();
   m_pShapeMax.Zero();
   StyleSet (DSSTYLE_HINGEL | DS_DOORSOLID);
}

CDoorSet::~CDoorSet (void)
{
   // free up doors
   DWORD i;
   for (i = 0; i < m_lDoors.Num(); i++) {
      PCDoor pd = *((PCDoor*) m_lDoors.Get(i));
      delete pd;
   }
   m_lDoors.Clear();
}

/**********************************************************************************
CDoorSet::ShapeSet - Sets the shape of the door set. Note: When the door set is rendererd
   it will use the X and Z from the shape. Y will be from 0 to -fThickness.
   Y=-1 is front, Z=1 is top.

inputs
   PCSpline       pShape - Spline for the shape. THe information is copied from this.
returns
   BOOL - TRUE if success
*/
BOOL CDoorSet::ShapeSet (PCSpline pShape)
{
   if (!pShape->LoopedGet())
      return FALSE;

   // dirty
   m_fDirty = TRUE;
   
   // store away
   m_lShape.Clear();
   DWORD i;
   m_lShape.Required (pShape->QueryNodes());
   for (i = 0; i < pShape->QueryNodes(); i++)
      m_lShape.Add (pShape->LocationGet(i));

   DWORD dwNum;
   PCPoint pap;
   pap = (PCPoint) m_lShape.Get(0);
   dwNum = m_lShape.Num();
   for (i = 0; i < dwNum; i++)
      pap[i].p[1] = 0;  // just enforece y=0 to save work later


   return TRUE;
}

/**********************************************************************************
CDoorSet::DivisionsSet - Sets the number of divisions and orientation.

inputs
   BOOL     fFixed - If FALSE, fDivisions is a distance. The width/height of the
            opening is divided enough times so no length is greater than fDivisions.
            if TRUE, fDivisions is a fixed integer count (1+) of the number to divide into.
   fp   fDivisions - See fFixed
   DWORD    dwOrient - How the door divisiona are oriented. See DSORIENT_XXX
   DWORD    dwMovement - How the doors moves. See DSMOVE_XXX
reutrns
   BOOL - TRUE if success
*/
BOOL CDoorSet::DivisionsSet (BOOL fFixed, fp fDivisions, DWORD dwOrient, DWORD dwMovement)
{
   m_fDirty = TRUE;
   m_fFixed = fFixed;
   m_fDivisions = fDivisions;
   m_dwOrient = dwOrient;
   m_dwMove = dwMovement;
   return FALSE;
}

/**********************************************************************************
CDoorSet::DivisionsGet - Fills in info aobut the divisions

inptus
   BOOL     *pfFixed - Filled in with m_fFixed
   fp   *pfDivisions - Filled in with divisions
   DOWRD    *pdwOrient - Filled in with orientation info
   DWORD    *pdwMovement - Filled in with movement info
*/
void CDoorSet::DivisionsGet (BOOL *pfFixed, fp *pfDivisions, DWORD *pdwOrient, DWORD *pdwMovement)
{
   if (pfFixed)
      *pfFixed = m_fFixed;
   if (pfDivisions)
      *pfDivisions = m_fDivisions;
   if (pdwOrient)
      *pdwOrient = m_dwOrient;
   if (pdwMovement)
      *pdwMovement = m_dwMove;
}

/**********************************************************************************
ClipPolygon - Given a CListFixed with CPoints going clockwise, this clips
it along an axis and fills the remaining bit (first polygon in the list if there's
more than one) back in. If no poygons retusn clears out th elist.

inputs
   PCListFixed       pPoints - List of CPoints
   DWORD             dwPlane - Clip on X plane if 0, Y plane if 1, Z plane if 2
   fp            fOffset - Offset of the plane from 0
   BOOL              fKeepLess - If TRUE, keep the values to the left/front/bottom, else reverse
returns
   BOOL - TRUE if success
*/
BOOL ClipPolygons (PCListFixed pPoints, DWORD dwPlane, fp fOffset, BOOL fKeepLess)
{
   CRenderClip rc;

   // add the clipping plane
   CPoint pNormal, pPoint;
   pNormal.Zero4();
   pPoint.Zero4();
   pNormal.p[dwPlane] = fKeepLess ? 1 : -1;
   pPoint.p[dwPlane] = fOffset;
   rc.AddPlane (&pNormal, &pPoint);

   // for all the points set w=1
   DWORD dwNum, i;
   PCPoint pap;
   dwNum = pPoints->Num();
   pap = (PCPoint) pPoints->Get(0);
   if (dwNum < 3)
      return TRUE;   // not much point
   for (i = 0; i < dwNum; i++)
      pap[i].p[3] = 1;


   
   // fill in the polygon info
   CMem  mem;
   DWORD dwWant;
   dwWant = dwNum * sizeof(CPoint) + dwNum * sizeof(VERTEX) + sizeof(POLYDESCRIPT) + dwNum * sizeof(DWORD);
   if (!mem.Required (dwWant))
      return FALSE;  // error
   memset (mem.p, 0, dwWant);
   PCPoint paPoints;
   PVERTEX pVertex;
   PPOLYDESCRIPT pPoly;
   DWORD *padw;
   paPoints = (PCPoint) mem.p;
   pVertex = (PVERTEX) (paPoints + dwNum);
   pPoly = (PPOLYDESCRIPT) (pVertex + dwNum);
   padw = (DWORD*) (pPoly + 1);
   memcpy (paPoints, pap, dwNum * sizeof(CPoint));
   for (i = 0; i < dwNum; i++) {
      pVertex[i].dwPoint = i;
      padw[i] = i;
   }
   pPoly->wNumVertices = (WORD) dwNum;


   // fill in the polygon rendering info
   POLYRENDERINFO pri;
   memset (&pri, 0, sizeof(pri));
   pri.dwNumPoints = dwNum;
   pri.dwNumPolygons = 1;
   pri.dwNumVertices = dwNum;
   pri.fAlreadyNormalized = TRUE;
   pri.paPoints = paPoints;
   pri.paVertices = pVertex;
   pri.paPolygons = pPoly;

   // clip
   if (rc.ClipPolygons (-1, &pri)) {
      // something was clipped
      if (pri.dwNumPolygons < 1) {
         pPoints->Clear();
         return TRUE;
      }

      // move these back
      pPoints->Clear();
      padw = (DWORD*) (pri.paPolygons + 1);
      for (i = 0; i < pri.paPolygons[0].wNumVertices; i++) {
         PCPoint p;
         p = &pri.paPoints[pri.paVertices[padw[i]].dwPoint];
         pPoints->Add (p);
      }
   }

   return TRUE;
}


/**********************************************************************************
CDoorSet::OpenSet - Sets the minimum and maximum amounts that this can open/close.
Use this for fixed shutters to make them always open, or doors that can only open
up 90 degrees.

inputs
   fp      fMin - Minimum. 0..1, 0 is completely closed
   fp      fMax - Maximum. 0..1, 1 is comletely open for the type of hinge
*/
void CDoorSet::OpenSet (fp fMin, fp fMax)
{
   m_fDirty = TRUE;
   fMin = max(0,fMin);
   fMax = min(1,fMax);
   m_fOpenMax = fMax;
   m_fOpenMin = fMin;
   m_fDirty = TRUE;
}

/**********************************************************************************
CDoorSet::OpenGet - Gets the amount that the door set can open/close. See OpenSet()

inputs
   fp      *pfMin - Filled with minimum
   fp      *pfMax - Filled with maximum
*/
void CDoorSet::OpenGet (fp *pfMin, fp *pfMax)
{
   if (pfMin)
      *pfMin = m_fOpenMin;
   if (pfMax)
      *pfMax = m_fOpenMax;
}



/**********************************************************************************
CDoorSet::CustomShapeSet - Sets the custom shape as one of the predefined shapes.

Tells the opening to override the automagic shapes for
doors (gotten by dividing its opening up) and using a custom shape for the doors.
(Such as the wild-west saloon doors.)

inputs
   DWORD          dwShape - DSCS_XXX. 0 for none
   fp         fBottom - 0..1, multiplier for full height
   fp         fTop - 0..1, multiplier for full height
   BOOL           fAlternate - If TRUE, alternate mirror images of the shape
                  when have multiple doors.
returns
   BOOL - TRUE if success
*/
BOOL CDoorSet::CustomShapeSet (DWORD dwShape, fp fBottom, fp fTop, BOOL fAlternate)
{
   m_fDirty = TRUE;
   m_dwCustomShape = dwShape;
   m_tCustomMinMax.h = fBottom;
   m_tCustomMinMax.v = fTop;
   m_fCustomAlternate = fAlternate;
   
   // set the style to custom since have changed from the standard
   // Takt this out since dont need m_dwStyle = DSSTYLE_CUSTOM;
   return FALSE;
}

/**********************************************************************************
CDoorSet::CustomShapeGet - Returns the custom shape.

inputs
   DWORD       *pdwShape - See CusotmShapeSet
   fp      *pfBottom
   fp      *pfTop
   BOOL        *pfAlternate
*/
void CDoorSet::CustomShapeGet (DWORD *pdwShape, fp *pfBottom, fp *pfTop, BOOL *pfAlternate)
{
   if (pdwShape)
      *pdwShape = m_dwCustomShape;
   if (pfBottom)
      *pfBottom = m_tCustomMinMax.h;
   if (pfTop)
      *pfTop = m_tCustomMinMax.v;
   if (pfAlternate)
      *pfAlternate = m_fCustomAlternate;
}


/**********************************************************************************
CDoorSet::Clone - Clones the door.

returns
   CDoor *- CLone
*/
CDoorSet * CDoorSet::Clone (void)
{
   PCDoorSet pNew = new CDoorSet;
   if (!pNew)
      return NULL;

   pNew->m_lShape.Init (sizeof(CPoint), m_lShape.Get(0), m_lShape.Num());
   pNew->m_dwStyle = m_dwStyle;
   pNew->m_fThickness = m_fThickness;
   pNew->m_fFixed = m_fFixed;
   pNew->m_fDivisions = m_fDivisions;
   pNew->m_dwOrient = m_dwOrient;
   pNew->m_dwMove = m_dwMove;
   pNew->m_fOpenMin = m_fOpenMin;
   pNew->m_fOpenMax = m_fOpenMax;
   pNew->m_fCustomAlternate = m_fCustomAlternate;
   pNew->m_dwCustomShape = m_dwCustomShape;
   pNew->m_tCustomMinMax = m_tCustomMinMax;
   pNew->m_fDirty = m_fDirty;
   pNew->m_dwDoors = m_dwDoors;
   pNew->m_pShapeMin.Copy (&m_pShapeMin);
   pNew->m_pShapeMax.Copy (&m_pShapeMax);
   memcpy (pNew->m_adwDKStyle, m_adwDKStyle, sizeof(m_adwDKStyle));
   memcpy (pNew->m_afKick, m_afKick, sizeof(m_afKick));
   pNew->m_tDKLoc = m_tDKLoc;
   pNew->m_tKickLoc = m_tKickLoc;
   pNew->m_fKnobRotate = m_fKnobRotate;

   pNew->m_lDoors.Init (sizeof(PCDoor), m_lDoors.Get(0), m_lDoors.Num());
   DWORD i;
   for (i = 0; i < pNew->m_lDoors.Num(); i++) {
      PCDoor *ppd = (PCDoor*) pNew->m_lDoors.Get(i);
      if (ppd)
         *ppd = (*ppd)->Clone();
   }

   return pNew;
}


static PWSTR gpszDoorSet = L"DoorSet";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszStyle = L"Style";
static PWSTR gpszThickness = L"Thickness";
static PWSTR gpszFixed = L"Fixed";
static PWSTR gpszDivisions = L"Divisions";
static PWSTR gpszOrient = L"Orient";
static PWSTR gpszMove = L"Move";
static PWSTR gpszOpenMin = L"OpenMin";
static PWSTR gpszOpenMax = L"OpenMax";
static PWSTR gpszCustomAlternate = L"CustomAlternate";
static PWSTR gpszCustomShape = L"CustomShape";
static PWSTR gpszCustomMinMax = L"CustomMinMax";
static PWSTR gpszDoors = L"Doors";
static PWSTR gpszDKStyle0 = L"DKStyle0";
static PWSTR gpszDKStyle1 = L"DKStyle1";
static PWSTR gpszKick0 = L"Kick0";
static PWSTR gpszKick1 = L"Kick1";
static PWSTR gpszDKLoc = L"DKLoc";
static PWSTR gpszKickLoc = L"KickLoc";
static PWSTR gpszKnobRotate = L"KnobRotate";

/**********************************************************************************
CDoorSet::MMLTo - Writes the door to MML.

returns
   PCMMLNode2 - Node that contains the door information
*/
PCMMLNode2 CDoorSet::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszDoorSet);

   // shape... make spline out of it
   CSpline s;
   s.Init (TRUE, m_lShape.Num(), (PCPoint)m_lShape.Get(0), NULL, (DWORD*)SEGCURVE_LINEAR, 0,0,0);
   PCMMLNode2 pSub;
   pSub = s.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszShape);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gpszStyle, (int) m_dwStyle);
   MMLValueSet (pNode, gpszThickness, m_fThickness);
   MMLValueSet (pNode, gpszFixed, (int)m_fFixed);
   MMLValueSet (pNode, gpszDivisions, m_fDivisions);
   MMLValueSet (pNode, gpszOrient, (int)m_dwOrient);
   MMLValueSet (pNode, gpszMove, (int)m_dwMove);
   MMLValueSet (pNode, gpszOpenMin, m_fOpenMin);
   MMLValueSet (pNode, gpszOpenMax, m_fOpenMax);
   MMLValueSet (pNode, gpszCustomAlternate, (int)m_fCustomAlternate);
   MMLValueSet (pNode, gpszCustomShape, (int)m_dwCustomShape);
   MMLValueSet (pNode, gpszCustomMinMax, &m_tCustomMinMax);

   MMLValueSet (pNode, gpszDKStyle0, (int)m_adwDKStyle[0]);
   MMLValueSet (pNode, gpszDKStyle1, (int)m_adwDKStyle[1]);
   MMLValueSet (pNode, gpszKick0, (int)m_afKick[0]);
   MMLValueSet (pNode, gpszKick1, (int)m_afKick[1]);
   MMLValueSet (pNode, gpszDKLoc, &m_tDKLoc);
   MMLValueSet (pNode, gpszKickLoc, &m_tKickLoc);
   MMLValueSet (pNode, gpszKnobRotate, m_fKnobRotate);

   // dont write m_fDirty
   // dont wite m_dwDoors

   // write all the door shapes out though
   DWORD i;
   for (i = 0; i < m_lDoors.Num(); i++) {
      PCDoor pd = *((PCDoor*)m_lDoors.Get(i));
      pSub = pd->MMLTo();
      if (pSub) {
         pSub->NameSet (gpszDoors);
         pNode->ContentAdd (pSub);
      }
   }

   return pNode;
}

/**********************************************************************************
CDoorSet::MMLFrom - Reads in information about the door.

inputs
   PCMMLNode2      pNode - TO read from
returns
   BOOL - TRUE if success
*/
BOOL CDoorSet::MMLFrom (PCMMLNode2 pNode)
{
   // free up doors
   DWORD i;
   for (i = 0; i < m_lDoors.Num(); i++) {
      PCDoor pd = *((PCDoor*) m_lDoors.Get(i));
      delete pd;
   }
   m_lDoors.Clear();

   m_lShape.Clear();

   // loop over all
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         continue;
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszShape)) {
         CSpline s;
         s.MMLFrom (pSub);
         m_lShape.Clear();

         DWORD j;
         CPoint p;
         m_lShape.Required (s.OrigNumPointsGet());
         for (j = 0; j < s.OrigNumPointsGet(); j++) {
            s.OrigPointGet (j, &p);
            m_lShape.Add (&p);
         }
      }
      else if (!_wcsicmp(psz, gpszDoors)) {
         PCDoor pDoor = new CDoor;
         if (!pDoor)
            continue;
         pDoor->MMLFrom (pSub);
         m_lDoors.Add (&pDoor);
      }
   }

   m_dwStyle = (DWORD) MMLValueGetInt (pNode, gpszStyle, 0);
   m_fThickness = MMLValueGetDouble (pNode, gpszThickness, .1);
   m_fFixed = (BOOL) MMLValueGetInt (pNode, gpszFixed, FALSE);
   m_fDivisions = MMLValueGetDouble (pNode, gpszDivisions, 1);
   m_dwOrient = (DWORD) MMLValueGetInt (pNode, gpszOrient, 0);
   m_dwMove = (DWORD) MMLValueGetInt (pNode, gpszMove, 0);
   m_fOpenMin = MMLValueGetDouble (pNode, gpszOpenMin, 0);
   m_fOpenMax = MMLValueGetDouble (pNode, gpszOpenMax, 1);
   m_fCustomAlternate = (BOOL) MMLValueGetInt (pNode, gpszCustomAlternate, 0);
   m_dwCustomShape = (DWORD) MMLValueGetInt (pNode, gpszCustomShape, 0);
   MMLValueGetTEXTUREPOINT (pNode, gpszCustomMinMax, &m_tCustomMinMax);

   m_adwDKStyle[0] = (DWORD) MMLValueGetInt (pNode, gpszDKStyle0, (int)0);
   m_adwDKStyle[1] = (DWORD) MMLValueGetInt (pNode, gpszDKStyle1, (int)0);
   m_afKick[0] = (BOOL) MMLValueGetInt (pNode, gpszKick0, (int)FALSE);
   m_afKick[1] = (BOOL) MMLValueGetInt (pNode, gpszKick1, (int)FALSE);
   MMLValueGetTEXTUREPOINT (pNode, gpszDKLoc, &m_tDKLoc);
   MMLValueGetTEXTUREPOINT (pNode, gpszKickLoc, &m_tKickLoc);
   m_fKnobRotate = MMLValueGetDouble (pNode, gpszKnobRotate, 0);

   // dont get m_fDirty
   m_fDirty = TRUE;

   // dont get m_dwDoors

   return TRUE;
}

/**********************************************************************************
CDoorSet::SurfaceQuery - Returns a DWORD with a bitfield indicating what types
of surfaces are on the door (glass, framing, etc.). This is used to determine what
colors are needed for the object calling.

returns
   DWORD - bitfield of DSURF_XXX types
*/
DWORD CDoorSet::SurfaceQuery (void)
{
   DWORD dwSurface = 0;
   DWORD i;

   CalcIfNecessary();

   for (i = 0; i < m_lDoors.Num(); i++) {
      PCDoor pd = *((PCDoor*)m_lDoors.Get(i));
      dwSurface |= pd->SurfaceQuery();
   }

   // include the roller
   if (m_dwMove == DSMOVE_ROLLER)
      dwSurface |= DSURF_BRACING;

   return dwSurface;
}

/**********************************************************************************
CDoorSet::StyleSet - Sets the door set style. Once the style has been set, it sets
all other parameters accordinly. Use this for an easy way of specifying window and
door styles.

inputs
   DWORD       dwStyle - One of DSSTYLE_XXX
returns
   BOOL - TRUE if succes.
*/
BOOL CDoorSet::StyleSet (DWORD dwStyle)
{
   // set dirty
   m_fDirty = TRUE;
   m_dwStyle = dwStyle;
   if (!(m_dwStyle & DSSTYLE_BITS))
      return TRUE;  // customs style

   BOOL fWindow, fCabinet;
   fWindow = (m_dwStyle & DSSTYLE_WINDOW) ? TRUE : FALSE;
   fCabinet = (m_dwStyle & DSSTYLE_CABINET) ? TRUE : FALSE;
   m_fThickness = fCabinet ? CM_CABINETDOORTHICKNESS : (fWindow ? CM_WINDOWTHICKNESS : CM_DOORTHICKNESS);
   m_fFixed = TRUE;
   m_fDivisions = 1;
   m_dwOrient = DSORIENT_LEFT;
   m_dwMove = DSMOVE_HINGED;
   m_fOpenMin = 0;
   m_fOpenMax = 1;
   m_fCustomAlternate = 1;
   m_dwCustomShape = 0;
   m_tCustomMinMax.h = 1.0 / 3.0;
   m_tCustomMinMax.v = 2.0 / 3.0;
   m_adwDKStyle[0] = m_adwDKStyle[1] = 0;
   m_afKick[0] = m_afKick[1] = 0;

   if (fCabinet) {
      m_tDKLoc.h = CM_CABINETKNOBINSIDE;
      m_tDKLoc.v = CM_CABINETKNOBHEIGHT;
   }

   switch (m_dwStyle & DSSTYLE_BITS) {
   case DSSTYLE_FIXED:   // fixed
      m_dwMove = DSMOVE_FIXED;
      break;
   case DSSTYLE_HINGEL:   // hinged, left, 1 division
   case DSSTYLE_HINGELO:   // hinged, left, 1 division
      if (!fWindow) {
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_LEVER;
      }
      break;
   case DSSTYLE_HINGEL2:   // hinged, left, 1 division, extrior
      if (!fWindow) {
         m_adwDKStyle[0] = DKS_DOORKNOBLOCK;
         m_adwDKStyle[1] = DKS_LEVER;
      }
      break;
   case DSSTYLE_HINGER:   // hinged, right, 1 division
   case DSSTYLE_HINGERO:   // hinged, right, 1 division
      m_dwOrient = DSORIENT_RIGHT;
      if (!fWindow) {
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_LEVER;
      }
      break;
   case DSSTYLE_HINGELR:   // hinged, left and right, 2 divisions
   case DSSTYLE_HINGELRO:   // hinged, left and right, 2 divisions
      m_dwOrient = DSORIENT_LEFTRIGHT;
      m_fDivisions = 2;
      if (!fWindow) {
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_LEVER;
      }
      break;
   case DSSTYLE_HINGEU:   // hinged, up, 1 dicision
      m_dwOrient = DSORIENT_UP;
      m_fOpenMax = .5;  // only open half way
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_DOORPULL1;
      break;
   case DSSTYLE_BIL:   // bifold, left 4 divisions
      m_dwMove = DSMOVE_BIFOLD;
      m_fDivisions = 4;
      if (!fWindow)
         m_adwDKStyle[0] = DKS_DOORPULL1;
      break;
   case DSSTYLE_BILR:   // bifold, left/right 4 divisions
      m_dwOrient = DSORIENT_LEFTRIGHT;
      m_dwMove = DSMOVE_BIFOLD;
      m_fDivisions = 4;
      if (!fWindow)
         m_adwDKStyle[0] = DKS_DOORPULL1;
      break;
   case DSSTYLE_SLIDEL:   // slide, left, 2 divisions
      m_dwMove = DSMOVE_SLIDE1;
      m_fDivisions = 2;
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_SLIDER;
      break;
   case DSSTYLE_SLIDEU:   // slide, up, 2 divisions
      m_dwOrient = DSORIENT_UP;
      m_dwMove = DSMOVE_SLIDE1;
      m_fDivisions = 2;
      break;
   case DSSTYLE_POCKL:   // pocket, left, 1 division
      m_dwMove = DSMOVE_SLIDE2;
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_POCKET;
      break;
   case DSSTYLE_POCKLR:   // pocket, left/right, 2 divisions
      m_dwOrient = DSORIENT_LEFTRIGHT;
      m_dwMove = DSMOVE_SLIDE2;
      m_fDivisions = 2;
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_POCKET;
      break;
   case DSSTYLE_POCKLR2:  // pocket, left/right, 2 divisions, on outside
      m_dwOrient = DSORIENT_LEFTRIGHT;
      m_dwMove = DSMOVE_SLIDE2;
      m_fDivisions = 2;
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_DOORPULL1;
      break;
   case DSSTYLE_POCKU:   // pocket up, 1 division
      m_dwOrient = DSORIENT_UP;
      m_dwMove = DSMOVE_SLIDE2;
      break;
   case DSSTYLE_GARAGEU:   // garage, up, 4 divisions
      m_dwOrient = DSORIENT_UP;
      m_dwMove = DSMOVE_GARAGE;
      m_fDivisions = 4;
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_DOORPULL1;
      break;
   case DSSTYLE_GARAGEU2:   // garage up, 1 division
      m_dwOrient = DSORIENT_UP;
      m_dwMove = DSMOVE_GARAGE;
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_DOORPULL1;
      break;
   case DSSTYLE_ROLLERU:   // roller up, 8 divisions
      m_dwOrient = DSORIENT_UP;
      m_dwMove = DSMOVE_ROLLER;
      m_fDivisions = 8;
      if (!fWindow)
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_DOORPULL1;
      break;
   case DSSTYLE_LOUVER:   // pivot, up(?), division for louver height
      m_dwOrient = DSORIENT_DOWN;
      m_dwMove = DSMOVE_PIVOT;
      m_fFixed = FALSE;
      m_fDivisions = CM_LOUVERHEIGHT;
      m_fOpenMax = .5;  // only open half way
      m_fThickness = 0.01; // thin
      break;
   case DSSTYLE_HINGELR2:   // hinged, left/right, 2 divisions, custom shape - saloon door
      m_dwOrient = DSORIENT_LEFTRIGHT;
      m_fDivisions = 2;
      m_dwCustomShape = DSCS_SALOON;
      break;
   }

   // override handle style
   if (fCabinet) {
      m_adwDKStyle[0] = DKS_CABINET1;
      m_adwDKStyle[1] = 0;
   }

   // special cases for some doors
   if (!fWindow) switch (m_dwStyle & DSSTYLE_BITS) {
   case DSSTYLE_HINGEL:   // hinged, left, 1 division
   case DSSTYLE_HINGEL2:   // hinged, left, 1 division, extrior
   case DSSTYLE_HINGER:   // hinged, right, 1 division
   case DSSTYLE_HINGELR:   // hinged, left and right, 2 divisions
   case DSSTYLE_HINGELO:   // hinged, left, 1 division
   case DSSTYLE_HINGERO:   // hinged, right, 1 division
   case DSSTYLE_HINGELRO:   // hinged, left and right, 2 divisions
      switch (LOBYTE(dwStyle)) {
      case DS_DOORGLASS7:
         m_adwDKStyle[0] = m_adwDKStyle[1] = DKS_DOORPULL5;
         break;
      case DS_DOORGLASS9:  // oval on top
      case DS_DOORGLASS12: // square on top
         m_adwDKStyle[1] = DKS_DOORPUSH1;
         m_adwDKStyle[0] = DKS_DOORPULL2;
         break;
      }
      break;
   }

   return TRUE;
}

/**********************************************************************************
CDoorSet::StyleGet - Returns the door style. NOTE: May be DSSTYLE_CUSTOM if
some custom settings were made.

returns
   DWORD - Style.
*/
DWORD CDoorSet::StyleGet (void)
{
   return m_dwStyle;
}

/**********************************************************************************
CDoorSet::DoorNum -Returns the number of doors (at the moment).

returns
   DWORD - number of doors
*/
DWORD CDoorSet::DoorNum (void)
{
   CalcIfNecessary();

   return m_lDoors.Num();
}

/**********************************************************************************
CDoorSet::DoorGet - Retuns a CLONE of the specified door object. The caller must free
this.

inptus
   DWORD    dwID - Door number, 0 is left-most
returns
   CDoor * - Door object. Must be deleted by the caller.
*/
CDoor *CDoorSet::DoorGet (DWORD dwID)
{
   CalcIfNecessary();

   PCDoor *ppd;
   ppd = (PCDoor*) m_lDoors.Get(dwID);
   if (!ppd)
      return NULL;

   return (*ppd)->Clone();
}

/**********************************************************************************
CDoorSet::DoorSet - Sets the door. The passed in object is cloned, so the caller
must still free the object.

inputs
   DWORD    dwID - Door number, 0 if left-most
   CDoor*   pDoor - Door.
returns
   BOOL - TRUE if success
*/
BOOL CDoorSet::DoorSet (DWORD dwID, CDoor *pDoor)
{
   PCDoor *ppd;
   ppd = (PCDoor*) m_lDoors.Get(dwID);
   if (!ppd)
      return FALSE;

   PCDoor pNew;
   pNew = pDoor->Clone();
   if (!pNew)
      return FALSE;

   m_fDirty = TRUE;
   delete (*ppd);
   (*ppd) = pNew;

   return TRUE;
}

/**********************************************************************************
CDoorSet::CalcIfNecessary - If the dirty flag is set then this recalculates all the
necessary doors.
*/
void CDoorSet::CalcIfNecessary (void)
{
   // dont bother if not dirty
   if (!m_fDirty)
      return;

   // first, figure out min and max of shape
   DWORD i;
   DWORD dwShapeNum = m_lShape.Num();
   PCPoint paShape = (PCPoint) m_lShape.Get(0);
   for (i = 0; i < dwShapeNum; i++) {
      if (i) {
         m_pShapeMin.Min(&paShape[i]);
         m_pShapeMax.Max(&paShape[i]);
      }
      else {
         m_pShapeMin.Copy (&paShape[i]);
         m_pShapeMax.Copy (&paShape[i]);
      }
   }

   // which direction are we looking at for the divisions
   DWORD dwDir, dwPerp;
   dwDir = ((m_dwOrient == DSORIENT_LEFT) || (m_dwOrient == DSORIENT_RIGHT) ||
      (m_dwOrient == DSORIENT_LEFTRIGHT)) ? 0 : 2;
   dwPerp = (dwDir ? 0 : 2);  // other direciton

   // find the distance
   fp fDist;
   fDist = m_pShapeMax.p[dwDir] - m_pShapeMin.p[dwDir];
   if (fDist < CLOSE) {
      m_dwDoors = 0;

      for (i = 0; i < m_lDoors.Num(); i++) {
         PCDoor pd = *((PCDoor*) m_lDoors.Get(i));
         delete pd;
      }
      m_lDoors.Clear();
      m_fDirty = FALSE;
      return;
   }

   // how many divisons
   if (m_fFixed) {
      m_dwDoors = (DWORD) m_fDivisions;
      m_dwDoors = max(1,m_dwDoors);
      m_fDivisions = m_dwDoors;
   }
   else {
      m_fDivisions = max(CLOSE, m_fDivisions);
      m_dwDoors = (DWORD) ceil(fDist / m_fDivisions);
      m_dwDoors = max(1,m_dwDoors);
   }

   // how wide is each door
   fp fWidth;
   fWidth = fDist / (fp) m_dwDoors;

   // if there are too many doors remove them
   while (m_lDoors.Num() > m_dwDoors) {
      PCDoor pd = *((PCDoor*) m_lDoors.Get(m_dwDoors));
      delete pd;
      m_lDoors.Remove (m_dwDoors);
   }

   // while there aren't enough doors add them
   while (m_lDoors.Num() < m_dwDoors) {
      PCDoor pd;
      PCDoor *ppd;
      pd = NULL;
      ppd = (PCDoor*) m_lDoors.Get(0);
      if (ppd && *ppd)
         pd = (*ppd)->Clone();
      if (!pd) {
         pd = new CDoor;
         if (!pd)
            continue;

         // May want to initialize some of the door style here
         DWORD dwSet;
         dwSet = LOBYTE(m_dwStyle);
         if (!dwSet)
            dwSet = DS_DOORSOLID;
         pd->StyleSet (dwSet);
      }
      m_lDoors.Add (&pd);
   }

   // how many doors on each side
   DWORD dwNumDoors, dwDoorsOnLeft, dwDoorsOnRight;
   dwNumDoors = m_lDoors.Num();
   dwDoorsOnLeft = dwDoorsOnRight = 0;
   switch (m_dwOrient) {
   default:
   case DSORIENT_LEFT:
   case DSORIENT_DOWN:
      dwDoorsOnLeft = dwNumDoors;
      break;
   case DSORIENT_RIGHT:
   case DSORIENT_UP:
      dwDoorsOnRight = dwNumDoors;
      break;
   case DSORIENT_LEFTRIGHT:
   case DSORIENT_UPDOWN:
      dwDoorsOnLeft = dwNumDoors / 2;
      dwDoorsOnRight = dwNumDoors - dwDoorsOnLeft;
      break;
   }

   // go through all the doors and tell them where to be drawn
   for (i = 0; i < m_lDoors.Num(); i++) {
      PCDoor pd = *((PCDoor*) m_lDoors.Get(i));

      // start with current shape
      CListFixed lShape;
      lShape.Init (sizeof(CPoint), m_lShape.Get(0), m_lShape.Num());

      // clip on left and right
      fp fLeft, fRight;
      fLeft = m_pShapeMin.p[dwDir] + i*fWidth;
      fRight = fLeft + fWidth;
      if (i)
         ::ClipPolygons (&lShape, dwDir, fLeft, FALSE);
      if (i+1 < m_lDoors.Num())
         ::ClipPolygons (&lShape, dwDir, fRight, TRUE);

      // offset all the points so that they're from -fWidth/2 to fWidth/2
      // dont bother centering in perpendicular direction though
      DWORD j;
      PCPoint pap;
      DWORD dwNum;
      fp fOffset;
      fOffset = -(fLeft + fRight)/2;
      pap = (PCPoint)lShape.Get(0);
      dwNum = lShape.Num();
      for (j = 0; j < dwNum; j++)
         pap[j].p[dwDir] += fOffset;

      // find min and max Z
      fp fMin, fMax;
      for (j = 0; j < dwNum; j++) {
         if (!j)
            fMin = fMax = pap[j].p[dwPerp];
         else {
            fMin = min(pap[j].p[dwPerp], fMin);
            fMax = max(pap[j].p[dwPerp], fMax);
         }
      }

      // if it's a custom shape then redo
      if (m_dwCustomShape) {

         // know that goes from -fWidth/2 to fWidth/2
         // and new height
         fp fMinHeight, fMaxHeight;
         fMinHeight = m_tCustomMinMax.h * (fMax - fMin) + fMin;
         fMaxHeight = m_tCustomMinMax.v * (fMax - fMin) + fMin;
         fMaxHeight = max(fMaxHeight, fMinHeight + .01); // at least some height
         
         // clear and rebuild
         lShape.Clear();
         CPoint p;
         p.Zero();
         switch (m_dwCustomShape) {
         default:
         case DSCS_RECTANGLE:
            p.p[dwDir] = -fWidth/2;
            p.p[dwPerp] = fMaxHeight;
            lShape.Required (4);
            lShape.Add (&p);
            p.p[dwDir] = fWidth/2;
            lShape.Add (&p);
            p.p[dwPerp] = fMinHeight;
            lShape.Add (&p);
            p.p[dwDir] = -fWidth/2;
            lShape.Add (&p);
            break;
         case DSCS_SALOON:
            for (j = 0; j <= 8; j++) {
               p.p[dwDir] = fWidth/8.0*(fp)j - fWidth/2;
               p.p[dwPerp] = fMaxHeight - (cos((fp)j / 8.0 * PI)+1) * (fMaxHeight - fMinHeight) / 8.0;
               lShape.Add (&p);
            }
            lShape.Required (2);
            p.p[dwPerp] = fMinHeight;
            lShape.Add (&p);
            p.p[dwDir] = -fWidth/2;
            lShape.Add (&p);
            break;
         }

         if (m_fCustomAlternate && (i % 2)) {
            pap = (PCPoint)lShape.Get(0);
            dwNum = lShape.Num();
            for (j = 0; j < dwNum/2; j++) {
               p.Copy (&pap[j]);
               pap[j].Copy (&pap[dwNum-j-1]);
               pap[dwNum-j-1].Copy (&p);
            }
            for (j = 0; j < dwNum; j++)
               pap[j].p[dwDir] *= -1;
         }
      }

      // set the door
      pd->ShapeSet (lShape.Num(), (PCPoint) lShape.Get(0), m_fThickness);

      // door knob info
      DOORKNOB dk;
      memset (&dk, 0, sizeof(dk));
      dk.fKickHeight = m_tKickLoc.v;
      dk.fKickIndent = m_tKickLoc.h;
      if (dwDir == 2) {
         dk.pKnob.p[0] = 0;   // always center when up/down
         dk.pKnob.p[2] = fWidth/2 - m_tDKLoc.h;
      }
      else {
         // open left/right
         dk.pKnob.p[0] = fWidth/2 - m_tDKLoc.h;
         dk.pKnob.p[2] = fMin + m_tDKLoc.v;
      }
      dk.pOpposite.Copy (&dk.pKnob);
      dk.pOpposite.p[dwDir] -= (fWidth - 2 * m_tDKLoc.h); // other side
      dk.afKick[0] = m_afKick[0];   // since from this view, pull side is the front
      dk.afKick[1] = m_afKick[1];
      dk.fKnobRotate = m_fKnobRotate * ((i >= dwDoorsOnLeft) ? 1 : -1);

      // if this is a set of >1 doors, then only one on indisde will have the knob
      BOOL fHasKnob, fFlipH;
      fHasKnob = fFlipH = FALSE;
      if (i >= dwDoorsOnLeft)
         fFlipH = TRUE;
      switch (m_dwMove) {
         case DSMOVE_HINGED:
            fHasKnob = TRUE;  // all hinged doors have knob
            break;

         case DSMOVE_PIVOT:
            fHasKnob = TRUE;  // all hinged doors have knob
            break;

         case DSMOVE_BIFOLD:
         case DSMOVE_GARAGE:
         case DSMOVE_ROLLER:
            // only on end bits
            if ( (dwDoorsOnLeft && (i == (dwDoorsOnLeft - 1))) || (i == dwDoorsOnLeft))
               fHasKnob = TRUE;
            break;

         case DSMOVE_SLIDE1:  // sliding glass, with ends fixed
            // only false if on ends
            fHasKnob = TRUE;
            if (dwDoorsOnLeft && (i == 0))
               fHasKnob = FALSE;
            if (dwDoorsOnRight && (i == dwNumDoors-1))
               fHasKnob = FALSE;
            break;

         case DSMOVE_SLIDE2:
            // on every slider
            fHasKnob = TRUE;
            break;

         default:
         case DSMOVE_FIXED:
            // dont have knobs
            break;
      }

      if (fFlipH) {
         CPoint p;
         p.Copy (&dk.pOpposite);
         dk.pOpposite.Copy (&dk.pKnob);
         dk.pKnob.Copy (&p);
      }
      
      dk.adwStyle[0] = fHasKnob ? m_adwDKStyle[0] : 0;
      dk.adwStyle[1] = fHasKnob ? m_adwDKStyle[1] : 0;

      pd->StyleHandleSet (&dk);
   }

   m_fDirty = FALSE;
}

/**********************************************************************************
CDoorSet::Render - Tells the door to render itself. It will be within the original X and Z
coordinates given, but will go from 0 to m_fTHickness (recessed)

inputs
   POBJECTRENDER        pr - Rendering information
   PCRenderSurface      *prs - Render to.
   DWORD                dwSurface - Which surfaces to render. From DSURF_XXX
   fp               fOpen - Amount that it's open. 0 for closed. 1 for opened as much as possible
returns
   none
*/
void CDoorSet::Render (POBJECTRENDER pr, CRenderSurface *prs, DWORD dwSurface, fp fOpen)
{
   CalcIfNecessary();


   // if this bit isn't one to be displayed dont bother
   if (!(SurfaceQuery() & dwSurface))
      return;

   // translate open into a number based on settings
   fOpen = min(1,fOpen);
   fOpen = max(0,fOpen);
   fOpen = fOpen * (m_fOpenMax - m_fOpenMin) + m_fOpenMin;

   // which direction are we looking at for the divisions
   DWORD dwDir, dwPerp;
   dwDir = ((m_dwOrient == DSORIENT_LEFT) || (m_dwOrient == DSORIENT_RIGHT) ||
      (m_dwOrient == DSORIENT_LEFTRIGHT)) ? 0 : 2;
   dwPerp = (dwDir ? 0 : 2);  // other direciton

   // how wide is each door
   fp fWidth;
   fp fDist;
   fDist = m_pShapeMax.p[dwDir] - m_pShapeMin.p[dwDir];
   fWidth = fDist / (fp) m_dwDoors;

   DWORD i;
   DWORD dwNumDoors, dwDoorsOnLeft, dwDoorsOnRight;
   dwNumDoors = m_lDoors.Num();
   dwDoorsOnLeft = dwDoorsOnRight = 0;
   for (i = 0; i < dwNumDoors; i++) {
      PCDoor pd = *((PCDoor*) m_lDoors.Get(i));
      fp fCur;
      fCur = m_pShapeMin.p[dwDir] + fWidth * i + fWidth/2;

      fp fScaleX, fScaleZ;
      fScaleX = (dwDir == 0) ? 1 : 0;
      fScaleZ = (dwDir == 0) ? 0 : 1;

      // is it on the left/bottom or right/top?
      fp fSide;
      fp fRotScale;
      switch (m_dwOrient) {
      default:
      case DSORIENT_LEFT:
      case DSORIENT_DOWN:
         dwDoorsOnLeft = dwNumDoors;
         fSide = -1;
         fRotScale = (m_dwOrient == DSORIENT_LEFT) ? 1 : -1;
         break;
      case DSORIENT_RIGHT:
      case DSORIENT_UP:
         dwDoorsOnRight = dwNumDoors;
         fSide = 1;
         fRotScale = (m_dwOrient == DSORIENT_RIGHT) ? 1 : -1;
         break;
      case DSORIENT_LEFTRIGHT:
      case DSORIENT_UPDOWN:
         dwDoorsOnLeft = dwNumDoors / 2;
         dwDoorsOnRight = dwNumDoors - dwDoorsOnLeft;
         fSide = (i < dwDoorsOnLeft) ? -1 : 1;
         fRotScale = (m_dwOrient == DSORIENT_LEFTRIGHT) ? 1 : -1;
         break;
      }

      // how far many doors from the edge is this
      DWORD dwFromEdge, dwFromMiddle;
      if (i < dwDoorsOnLeft) {
         dwFromEdge = i;
         dwFromMiddle = dwDoorsOnLeft - dwFromEdge - 1;
      }
      else {
         dwFromEdge = dwNumDoors - 1 - i;
         dwFromMiddle = dwDoorsOnRight - dwFromEdge - 1;
      }

      // where rotate around?
      fp   fXRot, fYRot;  // -1 to 1, for range in X (dwDir) and Y
      fp   fRot;    // number of radians to rotate around X (dwDir)
      fp   fXTrans, fYTrans;
      fXRot = fYRot = fXTrans = fYTrans = fRot = 0;
      switch (m_dwMove) {
         case DSMOVE_HINGED:
            fXRot = fSide;
            fYRot = -1; // rotate around front corner
            fRot = fOpen * PI * fSide * fRotScale;
            break;

         case DSMOVE_PIVOT:
            fXRot = 0;
            fYRot = 0;  // rotate around center
            fRot = fOpen * PI * fSide * fRotScale;
            break;

         case DSMOVE_SLIDE1:  // sliding glass, with ends fixed
            fXTrans = fSide * fWidth * (fp) dwFromEdge * fOpen;
            fYTrans = m_fThickness * (fp) dwFromMiddle; // doors indented
            break;

         case DSMOVE_SLIDE2:
            fXTrans = ((i < dwDoorsOnLeft) ? dwDoorsOnLeft : dwDoorsOnRight) * fOpen;
            fXTrans = min(fXTrans, dwFromEdge+1);
            fXTrans = fSide * fWidth * fXTrans;
            fYTrans = m_fThickness * (fp) dwFromMiddle; // doors indented
            break;

         case DSMOVE_BIFOLD:
            {
               // start with standard hinged door
               fXRot = fSide * (1.0 - m_fThickness / fWidth / 2);
               fYRot = 0;  // rotate around 0
               fRot = fOpen * PI/2 * fSide * fRotScale;

               // alternate
               if (dwFromEdge % 2) {
                  fXRot *= -1;
                  fRot *= -1;
               }

#if 0 //dead code
               // what's the length of the door at an angle
               fp fLenAtAngle;
               fLenAtAngle = fabs(cos(fRot)) * fWidth + fabs(sin(fRot)) * m_fThickness;

               // where should this be
               //fp fShouldBe;
               //if (i < dwDoorsOnLeft)
               //   fShouldBe = m_pShapeMin.p[dwDir] + fLenAtAngle * ((fp)dwFromEdge + .5);
               //else
               //   fShouldBe = m_pShapeMax.p[dwDir] - fLenAtAngle * ((fp)dwFromEdge + .5);

               //fXTrans = fShouldBe - fCur;
               fXTrans = (fWidth - fLenAtAngle) * (fp) dwFromEdge;
               //fXTrans -= fabs(sin(fRot)) * m_fThickness/4;
               if (dwFromEdge%2) {
                  fXTrans -= fabs(sin(fRot)) * m_fThickness;
               }
               else {
                  fXTrans += fabs(sin(fRot)) * m_fThickness;
               }
               //fXTrans += floor((dwFromEdge+1) / 2) * fabs(sin(fRot)) * m_fThickness/8;
               fXTrans *= fSide;
#endif // 0
            }
            break;

         default:
         case DSMOVE_FIXED:
            // do nothing
            break;
      }


      // push the stack
      prs->Push();

      if (fXTrans || fYTrans)
         prs->Translate (fXTrans * fScaleX, fYTrans, fXTrans * fScaleZ);

      if ((m_dwMove == DSMOVE_GARAGE) || (m_dwMove == DSMOVE_ROLLER)) {
         // find out how far along the length this is
         fp   fLenVert, fDist, fRadius, fCurveDist, fMiddleX;
         fLenVert = (fp) ((i < dwDoorsOnLeft) ? dwDoorsOnLeft : dwDoorsOnRight) * fWidth;
         fDist = fLenVert * fOpen + (fp) dwFromMiddle * fWidth;
         // BUGFIX - Was fWidth/2 but caused problem when wanted to have 1 panel garage door
         fRadius = (fWidth * dwNumDoors)/16;   // diameter of rotation if 1 width
         fRadius = max(CLOSE,fRadius);
         fRadius *= 2;
         fCurveDist = fRadius * PI / 2;
         fMiddleX = m_pShapeMin.p[dwDir] + fWidth * (fp)dwDoorsOnLeft;

         // FUTURERELEASE - Because only doing a hack job at calculating angles a
         // a garage door that's entirely flat ends up being at a bit of an angle
         // if it's lifted all the way, and it shouldn't

         // if it's a roller door and on the drawing the first one then also draw the roller
         if ((m_dwMove == DSMOVE_ROLLER) && !dwFromEdge && (dwSurface & DSURF_BRACING)) {
            CNoodle n;
            CPoint pStart, pEnd, pSize;
            pStart.Zero();
            pEnd.Zero();
            pSize.Zero();
            pStart.p[dwDir] = pEnd.p[dwDir] = (fLenVert + fRadius/2) * fSide + fMiddleX;
            pStart.p[dwPerp] = m_pShapeMin.p[dwPerp] - m_fThickness;
            pStart.p[1] = pEnd.p[1] = -fRadius;
            pEnd.p[dwPerp] = m_pShapeMax.p[dwPerp] + m_fThickness;
            pSize.p[0] = pSize.p[1] = fRadius * 2;
            n.PathLinear (&pStart, &pEnd);
            n.ScaleVector (&pSize);
            n.ShapeDefault (NS_CIRCLE);
            n.DrawEndsSet (TRUE);
            n.Render (pr, prs);
         }
         if (fDist + fWidth <= fLenVert) {
            // going straight up and down
            fCur = fMiddleX + fDist * fSide;
            prs->Translate (fCur * fScaleX, 0, fCur * fScaleZ);
         }
         else if (fDist <= fLenVert + fCurveDist) {
            // translate if on the curve
            if (fDist > fLenVert) {
               fp f1, f2, fAngle;
               fAngle = (fDist - fLenVert) / fCurveDist * PI / 2;
               f1 = fLenVert + sin(fAngle) * fRadius;
               f1 = f1 * fSide + fMiddleX;
               f2 = (cos(fAngle) - 1) * fRadius;
               prs->Translate (f1 * fScaleX, f2, f1 * fScaleZ);
            }
            else {
               // going straight up and down
               fCur = fMiddleX + fDist * fSide;
               prs->Translate (fCur * fScaleX, 0, fCur * fScaleZ);
            }

            // rotating
            prs->Rotate ((fDist + fWidth - fLenVert) / (fCurveDist + fWidth) * PI/2
               * fSide * fRotScale * -1,
               dwPerp+1);

         }
         else {
            fp f1, f2;
            f1 = fLenVert + fRadius;
            f1 = f1 * fSide + fMiddleX;
            f2 = fRadius + fDist - fLenVert - fCurveDist;

            // if it's a roller door then dont draw at this point
            if (m_dwMove == DSMOVE_ROLLER)
               goto skipdraw;

            prs->Translate (f1 * fScaleX, -f2, f1 * fScaleZ);

            // on top
            prs->Rotate (PI/2 * fSide * fRotScale * -1, dwPerp+1);
         }

         // translate to the pivot point
         fp fPivotPoint;
         fPivotPoint = -(fWidth/2) * fSide;
         prs->Translate (-fScaleX * fPivotPoint, m_fThickness/2, -fScaleZ * fPivotPoint);
      }
      else if (m_dwMove == DSMOVE_BIFOLD) {
         // what's the length of the door at an angle
         fp fLenAtAngle;
         fLenAtAngle = fabs(cos(fRot)) * fWidth + fabs(sin(fRot)) * m_fThickness;

         // translate the pivot point to where it should be
         fp fShouldBe;
         fShouldBe = m_fThickness/2;   // since rotate around this point
         fShouldBe += 2.0 * fLenAtAngle * (fp)(DWORD)((dwFromEdge+1)/2);
         if (dwFromEdge % 2)
            fShouldBe -= (fabs(sin(fRot)) + fabs(cos(fRot))) * m_fThickness;
         // if (dwFromEdge%2) == 0 then dont adjust

         if (i < dwDoorsOnLeft)
            fShouldBe = m_pShapeMin.p[dwDir] + fShouldBe;
         else
            fShouldBe = m_pShapeMax.p[dwDir] - fShouldBe;

         // translate
         prs->Translate (fShouldBe * fScaleX, m_fThickness/2, fShouldBe * fScaleZ);

         // rotate
         prs->Rotate (fRot, dwPerp+1);

         // translate to the pivot point
         fp fPivotPoint;
         fPivotPoint = (fWidth/2 - m_fThickness/2) * fSide * ((dwFromEdge % 2) ? -1 : 1);
         prs->Translate (-fScaleX * fPivotPoint, 0, -fScaleZ * fPivotPoint);
      }
      else {
         // do the rotation and translation for hinged and pivot
         if (fRot) {
            fp fTrans;
            fTrans = (fWidth/2 * fXRot + fCur);
            prs->Translate (
               fTrans * fScaleX,
               (1+fYRot) * m_fThickness/2,
               fTrans * fScaleZ);
            prs->Rotate (fRot, dwPerp+1);
            prs->Translate (
               -fTrans * fScaleX,
               -(1.0+fYRot) * m_fThickness/2,
               -fTrans * fScaleZ);
         }

         // translate
         prs->Translate (fScaleX * fCur, m_fThickness/2, fScaleZ * fCur);
      }

      pd->Render (pr, prs, dwSurface);
skipdraw:
      // pop the stack
      prs->Pop();
   }
}


/**********************************************************************************
CDoorSet::ThicknessGet - Returns the thickness of the doors. Used by functions
calling.

retunrs
   fp - Thickness
*/
fp CDoorSet::ThicknessGet (void)
{
   return m_fThickness;
}

/**********************************************************************************
CDoorSet::ThicknessSet - Sets the thickness

retunrs
   fp - Thickness
*/
BOOL CDoorSet::ThicknessSet (fp fThick)
{
   if (fThick < 0)
      return FALSE;

   m_fThickness = fThick;
   m_fDirty = TRUE;

   return TRUE;
}

typedef struct {
   PCDoorSet      pDoorSet;   // door set
   PCWorldSocket        pWorld;     // wolrd
   PCObjectSocket pThis;      // notification to pWorld
   PCObjectDoor   pObjectDoor;
} KDS, *PKDS;


/* DoorKnobPage
*/
BOOL DoorKnobPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PKDS pkds = (PKDS) pPage->m_pUserData;
   PCDoorSet pv = pkds->pDoorSet;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[64];
         PCEscControl pControl;
         DWORD i;
         for (i = 0; i < 2; i++) {
            // style
            swprintf (szTemp, L"dkstyle%d", (int) i);
            ComboBoxSet (pPage, szTemp, pv->m_adwDKStyle[i]);

            // kickboard
            swprintf (szTemp, L"kick%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), pv->m_afKick[i]);
         }

         // valiues
         MeasureToString (pPage, L"dkloch", pv->m_tDKLoc.h);
         MeasureToString (pPage, L"dklocv", pv->m_tDKLoc.v);
         MeasureToString (pPage, L"kickh", pv->m_tKickLoc.h);
         MeasureToString (pPage, L"kickv", pv->m_tKickLoc.v);
         AngleToControl (pPage, L"knobrotate", pv->m_fKnobRotate);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"kick0") || !_wcsicmp (psz, L"kick1")) {
            if (pkds->pWorld)
               pkds->pWorld->ObjectAboutToChange (pkds->pThis);
            pv->m_afKick[!_wcsicmp (psz, L"kick1")] = p->pControl->AttribGetBOOL(Checked());
            pv->m_fDirty = TRUE;
            pv->CalcIfNecessary();
            if (pkds->pWorld)
               pkds->pWorld->ObjectChanged (pkds->pThis);
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         
         if (!_wcsicmp (psz, L"dkstyle0") || !_wcsicmp (psz, L"dkstyle1")) {
            DWORD dwIndex = !_wcsicmp (psz, L"dkstyle1");
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (pv->m_adwDKStyle[dwIndex] == dwVal)
               break;   // no change

            if (pkds->pWorld)
               pkds->pWorld->ObjectAboutToChange (pkds->pThis);
            pv->m_adwDKStyle[dwIndex] = dwVal;
            pv->m_fDirty = TRUE;
            pkds->pObjectDoor->m_fDirty = TRUE; // BUGFIX - So if add handle it gets drawn
            pv->CalcIfNecessary();
            if (pkds->pWorld)
               pkds->pWorld->ObjectChanged (pkds->pThis);
            return TRUE;
         }
      }


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;

         // anything that happens here will be one of my edit boxes
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         if (pkds->pWorld)
            pkds->pWorld->ObjectAboutToChange (pkds->pThis);

         // since only our edit boxes will be visible, get them all
         fp fTemp;
         MeasureParseString (pPage, L"dkloch", &fTemp);
         pv->m_tDKLoc.h = fTemp;
         MeasureParseString (pPage, L"dklocv", &fTemp);
         pv->m_tDKLoc.v = fTemp;
         MeasureParseString (pPage, L"kickh", &fTemp);
         pv->m_tKickLoc.h = fTemp;
         MeasureParseString (pPage, L"kickv", &fTemp);
         pv->m_tKickLoc.v = fTemp;
         pv->m_fKnobRotate = AngleFromControl (pPage, L"knobrotate");

         pv->m_fDirty = TRUE;
         pv->CalcIfNecessary();
         if (pkds->pWorld)
            pkds->pWorld->ObjectChanged (pkds->pThis);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Door/window handle";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/************************************************************************************
CDoorSet::KnobDialog - Bring up the UI to change the door knob.

inputs
   PCEscWindow       pWindow - To bring up dialog in
   PCWorldSocket           pWorld - notify when object changtes
   PCObjectSocket    pThis - Notify when object changes
   PCObjectDoor      pObjectDoor - So can refresh when have new scheme
returns
   BOOL - TRUE if user pressed OK, FALSE if cancel
*/
BOOL CDoorSet::KnobDialog (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pThis,
                           PCObjectDoor pObjectDoor)
{
   PWSTR psz;
   KDS kds;
   memset (&kds, 0, sizeof(kds));
   kds.pDoorSet = this;
   kds.pWorld = pWorld;
   kds.pThis = pThis;
   kds.pObjectDoor = pObjectDoor;
   psz = pWindow->PageDialog (ghInstance, IDR_MMLDOORKNOB, DoorKnobPage, &kds);
   if (psz && !_wcsicmp(psz, Back()))
      return TRUE;
   else
      return FALSE;
}


// FUTURERELEASE - Fan door - like bifold but it never completely closes
