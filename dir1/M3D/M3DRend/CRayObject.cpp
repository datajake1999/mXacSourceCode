/************************************************************************
CRayObject.cpp - Stores ray tracing information for an object.

begun 18/4/03 by Mike Rozak
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"

#define  DISTFROMSTART        (CLOSE)        // distance that reflected ray starts from origin

// CRTINBOUND - Used to store what objects are within the bounding volume
typedef struct {
   DWORD             dwID;          // object ID
   CPoint            pBoundSphere;  // bounding sphere of only the portion of the object
                                    // within the bounding volume
   float             fDistance;     // distance between the center of the sphere and the start of the ray
} CRTINBOUND, *PCRTINBOUND;

/********************************************************************************
GridSetBit - Sets a bit in a grid.

inputs
   DWORD          x, y, z - xyz location (from 0..RTGRIDDIM-1)
   PCRTGRID       pGrid - To write to
returns
   none
*/
static inline void GridSetBit (DWORD x, DWORD y, DWORD z, PCRTGRID pGrid)
{
   DWORD dwBit, dwDWORD;
   dwBit = x + y * RTGRIDDIM + z * RTGRIDDIM * RTGRIDDIM;
   dwDWORD = dwBit / 32;
   dwBit -= dwDWORD * 32;
   pGrid->adw[dwDWORD] |= (1 << dwBit);
}


/********************************************************************************
GridTestIntersectBit - Returns TRUE if the bit number intersects in the grid

inputs
   DWORD          dwNum - Bit number 0.. RTGRIDBINS
   PCRTGRID       pGrid - To test against
returns
   BOOL - TRUE if intersects
*/
static inline BOOL GridTestIntersectBit (DWORD dwNum, PCRTGRID pGrid)
{
   DWORD dwDWORD;
   dwDWORD = dwNum / 32;
   dwNum -= dwDWORD * 32;
   if (pGrid->adw[dwDWORD] & (1 << dwNum))
      return TRUE;
   else
      return FALSE;
}


/********************************************************************************
GridTestIntersect - Returns TRUE if any of the bits of the grid intersect.

inputs
   PCRTGRID       pGridA - To test against
   PCRTGRID       pGridB - To test against
returns
   BOOL - TRUE if intersects
*/
static inline BOOL GridTestIntersect (PCRTGRID pGridA, PCRTGRID pGridB)
{
   DWORD i;
   for (i = 0; i < RTGRIDDWORD; i++)
      if (pGridA->adw[i] & pGridB->adw[i])
         return TRUE;
   return FALSE;
}

/********************************************************************************
GridCalcBox - Given a box with corners pCorner1 and pCorner2, this calculates
the DWORDs the define its bounds in RTGRIDDIM.

NOTE: This assumes that CRTGRID either zeroed or filled with
valid information.

inputs
   PCPoint     pCorner1, pCorner2 - Two opposide corners. These points must have already
                  been converted so that the area of bounding box (min to max)
                  goes from 0..RTGRIDDIM. (Makes calculates faster here)
   DWORD       *padw - Array of 2 (0=min,1=max) by 3 (xyz) that's filled in
returns
   DWORD - Number of bits that filled in
*/
static inline DWORD GridCalcBox (PCPoint pCorner1, PCPoint pCorner2, DWORD *padw)
{
   // min/max points
   DWORD i;
   fp fMin, fMax;
   for (i = 0; i < 3; i++) {
      fMin = min(pCorner1->p[i], pCorner2->p[i]) - CLOSE;   // CLOSE so dont get polygon exactly on edge
      fMax = max(pCorner1->p[i], pCorner2->p[i]) + CLOSE;

      fMin = max(0, fMin);
      fMax = min(RTGRIDDIM, fMax);

      padw[i] = (DWORD)floor(fMin);
      padw[3+i] = (DWORD)ceil(fMax);
   } // i

   // area
   return (padw[3+0] - padw[3]) * (padw[3+1]-padw[1]) * (padw[3+2]-padw[2]);
}

/********************************************************************************
GridDrawBox - Draws a box through the grid, setting all bits where the box
goes through to 1. NOTE: This assumes that CRTGRID either zeroed or filled with
valid information.

inputs
   PCPoint     pCorner1, pCorner2 - Two opposide corners. These points must have already
                  been converted so that the area of bounding box (min to max)
                  goes from 0..RTGRIDDIM. (Makes calculates faster here)
   PCRTGRID    pGrid - Grid
returns
   none
*/
static void GridDrawBox (PCPoint pCorner1, PCPoint pCorner2, PCRTGRID pGrid)
{
   DWORD adw[2][3];  // [0=min,1=max][0..2=xyz]

   GridCalcBox (pCorner1, pCorner2, &adw[0][0]);

   DWORD x,y,z;
   for (z = adw[0][2]; z < adw[1][2]; z++)
   for (y = adw[0][1]; y < adw[1][1]; y++)
   for (x = adw[0][0]; x < adw[1][0]; x++)
      GridSetBit (x,y,z, pGrid);
}

/********************************************************************************
GridDrawLine - Draws a line through the grid, setting all bits where the line
goes through to 1. NOTE: This assumesthat CRTGRID either zeroed or filled with
valid information.

inputs
   PCPoint     pStart, pEnd - Start and end point. These points must have already
                  been converted so that the area of bounding box (min to max)
                  goes from 0..RTGRIDDIM. (Makes calculates faster here)
   PCRTGRID    pGrid - Grid
returns
   none
*/
static void GridDrawLine (PCPoint pStart, PCPoint pEnd, PCRTGRID pGrid)
{
   // if this is out of bounds then not going to affect
   DWORD i;
   for (i = 0; i < 3; i++) {
      if ((pStart->p[i] < 0) && (pEnd->p[i] < 0))
         return;  // out of bounds
      if ((pStart->p[i] > RTGRIDDIM) && (pEnd->p[i] > RTGRIDDIM))
         return;  // out of bounds
   }

   // figure out the length
   fp fLen;
   CPoint pLen;
   pLen.Subtract (pEnd, pStart);
   pLen.p[0] = fabs(pLen.p[0]);
   pLen.p[1] = fabs(pLen.p[1]);
   pLen.p[2] = fabs(pLen.p[2]);
   fLen = max(max(pLen.p[0], pLen.p[1]), pLen.p[2]);

   // if the length is less than 1/2 then fill in the bounding boxes
   if (fLen <= 1) {  // BUGFIX - Was < .5
      GridDrawBox (pStart, pEnd, pGrid);
      return;
   }

   // else, subdivide
   pLen.Average (pStart, pEnd);
   GridDrawLine (pStart, &pLen, pGrid);
   GridDrawLine (&pLen, pEnd, pGrid);
}



/********************************************************************************
GridDrawTriangle - Draws a tirangle through the grid, setting all bits where the triangle
goes through to 1. NOTE: This assumes that CRTGRID either zeroed or filled with
valid information.

inputs
   PCPoint     p1, p2, p3 - Three points.
                  These points must have already
                  been converted so that the area of bounding box (min to max)
                  goes from 0..RTGRIDDIM. (Makes calculates faster here)
   PCRTGRID    pGrid - Grid
returns
   none
*/
static void GridDrawTriangle (PCPoint p1, PCPoint p2, PCPoint p3, PCRTGRID pGrid)
{
   // if this is out of bounds then not going to affect
   DWORD i;
   for (i = 0; i < 3; i++) {
      if ((p1->p[i] < 0) && (p2->p[i] < 0) && (p3->p[i] < 0))
         return;  // out of bounds
      if ((p1->p[i] > RTGRIDDIM) && (p2->p[i] > RTGRIDDIM) && (p3->p[i] > RTGRIDDIM))
         return;  // out of bounds
   }

   // figure out the bounding box
   CPoint pMin, pMax, pLen;
   pMin.Copy (p1);
   pMax.Copy (p1);
   pMin.Min (p2);
   pMax.Max (p2);
   pMin.Min (p3);
   pMax.Max (p3);
   pLen.Subtract (&pMax, &pMin);

   // if all less then .5 then fill in bits
   if ((pLen.p[0] < .5) && (pLen.p[1] < .5) && (pLen.p[2] < .5)) {
      GridDrawBox (&pMin, &pMax, pGrid);
      return;
   }

   // else, subdivide
   CPoint p12, p23, p31;
   p12.Average (p1, p2);
   p23.Average (p2, p3);
   p31.Average (p3, p1);
   GridDrawTriangle (p1, &p12, &p31, pGrid);
   GridDrawTriangle (p2, &p12, &p23, pGrid);
   GridDrawTriangle (p3, &p23, &p31, pGrid);
   GridDrawTriangle (&p12, &p23, &p31, pGrid);
}


/********************************************************************************
QuickIntersectNormLineSphere - Intersect a line with a spehere.

inputs
   PCPoint           pStart - line start
   PCPonint          pDir - Line direction. This is NORMALIZED.
   PCPoint           pCenter - p[0]..p[2] = Center of sphere. p[3] = radius of sphere
   fp                fMaxAlpha - If the closest this sphere will get is still futher away
                     than fMax alpha then no intersections
   fp                *pafAlpha - Pointer to an array of 2 floats.
                        Filled in with alpha where intersect.
                        pafAlpha[x] == 0 then intersect at pStart. pafAlpha[x] == 1.0 then intersect
                        at pStart + pDir
returns
   DWORD - 0 if no intersections, 1 if one intersection (only pfAlpha1 valid), 2 if two intersection

NOTE - This is optimized to be as fast as possible
*/

#if 0 // slow code, although should be fast
static _inline DWORD QuickIntersectNormLineSphere (PCPoint pStart, PCPoint pDir, PCPoint pCenter,
                           fp fMaxAlpha, fp *pafAlpha)
{
   // easiest thing to do is make new start that's offset by center of the sphere
   fp aS[3];
   DWORD i;
   for (i = 0; i < 3; i++)
      aS[i] = pStart->p[i] - pCenter->p[i];

   // calculate aq, bq, cq
   double bq, cq;
   
   double fDistSqr;
   fDistSqr = aS[0] * aS[0] + aS[1] * aS[1] + aS[2] * aS[2];
   // if wouldnt meet alpha requirements then fail
   double fDist;
   fDist = sqrt(fDistSqr);
   if (fDist - pCenter->p[3] >= fMaxAlpha)
      return 0;

   //aq = pD.p[0] * pD.p[0] + pD.p[1] * pD.p[1] + pD.p[2] * pD.p[2];
   //aq = 1;  // since pD is normalized
   bq = 2 * (aS[0] * pDir->p[0] + aS[1] * pDir->p[1] + aS[2] * pDir->p[2]);
   cq = fDistSqr - pCenter->p[3] * pCenter->p[3];

   //if (fabs(aq) > EPSILON) {
      double r;
      r = bq * bq - 4.0 /* * aq*/ * cq;
      if (r < 0)
         return 0;   // no intersection
      r = sqrt(r);

      //double t1, t2;
      //bq = -bq / (2.0 * aq);
      //r /= (2.0 * aq);
      //t1 = bq + r;
      //t2 = bq - r;
      //pafAlpha[0] = t1;// * fLen;
      //pafAlpha[1] = t2;// * fLen;
      pafAlpha[1] = (-bq + r) / 2.0;
      pafAlpha[0] = (-bq - r) / 2.0;
      return 2;
   //}
   //else {
   //   // aw == 0, so one solution
   //   double t;
   //   if (fabs(bq) < EPSILON)
   //      return 0;
   //   t = -cq / bq;
   //
   //   pafAlpha[0] = t;// * fLen;
   //   return 1;
   //}
}
#endif // slow code

static _inline DWORD QuickIntersectNormLineSphere (PCPoint pStart, PCPoint pDir, PCPoint pCenter,
                           /*fp fMaxAlpha,*/ fp *pafAlpha)
{
   // easiest thing to do is make new start that's offset by center of the sphere
   fp aS[3];
   DWORD i;
   for (i = 0; i < 3; i++)
      aS[i] = pStart->p[i] - pCenter->p[i];
   //CPoint pS;//, pD;
   //pS.Subtract (pStart, pCenter);
   //pD.Copy (pDir);
   //fp fLen;
   //fLen = pD.Length();
   //if (fabs(fLen) < EPSILON)
   //   return 0;   // error
   //fLen = 1.0 / fLen;
   //pD.Scale(fLen);

   // calculate aq, bq, cq
   double /*aq,*/ bq, cq;
   
   // not doing because slows stuff down
   //double fDistSqr;
   //fDistSqr = aS[0] * aS[0] + aS[1] * aS[1] + aS[2] * aS[2];
   // if wouldnt meet alpha requirements then fail
   //double fDist;
   //fDist = sqrt(fDistSqr);
   //if (fDist - pCenter->p[3] >= fMaxAlpha)
   //   return 0;

   //aq = pD.p[0] * pD.p[0] + pD.p[1] * pD.p[1] + pD.p[2] * pD.p[2];
   //aq = 1;  // since pD is normalized
   bq = 2 * (aS[0] * pDir->p[0] + aS[1] * pDir->p[1] + aS[2] * pDir->p[2]);
   cq = aS[0] * aS[0] + aS[1] * aS[1] + aS[2] * aS[2] - pCenter->p[3] * pCenter->p[3];

   //if (fabs(aq) > EPSILON) {
      double r;
      r = bq * bq - 4.0 /* * aq*/ * cq;
      if (r < 0)
         return 0;   // no intersection
      r = sqrt(r);

      //double t1, t2;
      //bq = -bq / (2.0 * aq);
      //r /= (2.0 * aq);
      //t1 = bq + r;
      //t2 = bq - r;
      //pafAlpha[0] = t1;// * fLen;
      //pafAlpha[1] = t2;// * fLen;
      pafAlpha[1] = (-bq + r) / 2.0;
      pafAlpha[0] = (-bq - r) / 2.0;
      return 2;
   //}
   //else {
   //   // aw == 0, so one solution
   //   double t;
   //   if (fabs(bq) < EPSILON)
   //      return 0;
   //   t = -cq / bq;
   //
   //   pafAlpha[0] = t;// * fLen;
   //   return 1;
   //}
}



/***************************************************************************************
CRayObject::Constructor and destructor

inputs
   PCRenderRay          pRenderRay - Ray tracing parnt. Can be NULL if pRenderTrad is NOT null
   PCRenderTraditional  pRenderTrad - Traditional renderer. Can be NULL if pRenderRay is NOT null.
   BOOL                 fIsClone - Set to TRUE if is a clone, FALSE if normal object
*/
CRayObject::CRayObject (DWORD dwRenderShard, PCRenderRay pRenderRay, PCRenderTraditional pRenderTrad, BOOL fIsClone)
{
   m_dwRenderShard = dwRenderShard;
   m_dwID = 0;
   m_fDontDelete = FALSE;
   m_fIsClone = fIsClone;
   m_pRenderRay = pRenderRay;
   m_pRenderTrad = pRenderTrad;
#ifdef USEGRID
   m_lCRTGRID.Init (sizeof (CRTGRID));
#endif // USEGRID


   // just call clear
   Clear (FALSE);

}

CRayObject::~CRayObject (void)
{
   Clear();
}

/***************************************************************************************
CRayObject::Clear - Frees up all the memory used by the ray-tracing object

inputs
   BOOL        fFreeExist - If TRUE, if any memory was allocated that's freed.
                  If FALSE, everything is just set to 0, and hopefully there's no memory pointed to.
*/
void CRayObject::Clear (BOOL fFreeExist)
{
   DWORD i;
   if (fFreeExist) {
      // release textures
      if (m_pSurfaces) for (i = 0; i < m_dwNumSurfaces; i++)
         if (m_pSurfaces[i].pTexture)
            TextureCacheRelease (m_dwRenderShard, m_pSurfaces[i].pTexture);

      // delete sub-objects
      if (!m_fDontDelete && m_lCRTSUBOBJECT.Num()) {
         PCRTSUBOBJECT pSubObjects = (PCRTSUBOBJECT) m_lCRTSUBOBJECT.Get(0);
         for (i = 0; i < m_lCRTSUBOBJECT.Num(); i++)
            if (pSubObjects[i].pObject)
               delete pSubObjects[i].pObject;
      }

      // release clones
      if (m_pCloneObject) {
         // Will need to release the clones
         if (m_pRenderRay)
            m_pRenderRay->CloneRelease (&m_gCloneCode, &m_gCloneSub);
         if (m_pRenderTrad)
            m_pRenderTrad->CloneRelease (&m_gCloneCode, &m_gCloneSub);
      }

      // release memory
      if (m_pmemPoly)
         delete m_pmemPoly;
   }



   // wipe out memory
   memset (m_apBound, 0, sizeof(m_apBound));
   m_pBoundSphere.Zero4();

   // polygons
   m_pmemPoly = NULL;
   m_dwPolyDWORD = FALSE;
   m_dwNumSurfaces = m_dwNumPoints = m_dwNumNormals = m_dwNumTextures = m_dwNumColors = 0;
   m_dwNumVertices = m_dwNumTri = m_dwNumQuad = 0;
   m_pSurfaces = NULL;
   m_pRENDERSURFACE = NULL;
   m_pPoints = NULL;
   m_pNormals = NULL;
   m_pTextures = NULL;
   m_pColors = NULL;
   m_pVertices = NULL;
   m_pTri = NULL;
   m_pQuad = NULL;

   // sub-objects
   m_lCRTSUBOBJECT.Init (sizeof(CRTSUBOBJECT));
   m_lCRTSUBOBJECT.Clear();
   m_lCRTCLONE.Init (sizeof(CRTCLONE));
   m_lCRTCLONE.Clear();
   m_lLIGHTINFO.Clear();
   m_pCloneObject = NULL;

   // bound
   DWORD j;
   for (i = 0; i < MAXRAYTHREAD; i++) for (j = 0; j < CRTBOUND; j++)
      m_adwLastObject[i][j] = -1;

}

/***************************************************************************************
CRayObject::SubObjectAdd - Adds a new sub-object to the list.

inputs
   PCRayObject       pAdd - Object to add. From hereon out th eobject will be owned
                     by this rayobject
   PCMatrix          pMatrix - Matrix to do transform of points for object. If NULL 
                     then uses identity matrix
*/
BOOL CRayObject::SubObjectAdd (PCRayObject pAdd, PCMatrix pMatrix)
{
   // can only have one type of object, so if there are any polygons or clones then exit
   if (m_lCRTCLONE.Num() || m_dwNumTri || m_dwNumQuad)
      return FALSE;

   CRTSUBOBJECT so;
   CMatrix mIdent;
   memset (&so, 0, sizeof(so));
   mIdent.Identity();
   so.mMatrix.Copy (pMatrix ? pMatrix : &mIdent);
   so.fMatrixIdent = so.mMatrix.AreClose (&mIdent);
   if (so.fMatrixIdent)
      so.mMatrixInv.Copy (&mIdent);
   else
      so.mMatrix.Invert4 (&so.mMatrixInv);
   so.pObject = pAdd;
   
   // calculate the bounding box
   DWORD x, y, z;
   CPoint p;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
      p.p[0] = pAdd->m_apBound[x].p[0];
      p.p[1] = pAdd->m_apBound[y].p[1];
      p.p[2] = pAdd->m_apBound[z].p[2];
      p.p[3] = 1;
      p.MultiplyLeft (&so.mMatrix);

      if (x || y || z) {
         so.apBound[0].Min (&p);
         so.apBound[1].Max (&p);
      }
      else {
         so.apBound[0].Copy (&p);
         so.apBound[1].Copy (&p);
      }
   }

   // bounding spehere
   so.pBoundSphere.Average (&so.apBound[0], &so.apBound[1]);
   p.Subtract (&so.apBound[0], &so.pBoundSphere);
   so.pBoundSphere.p[3] = p.Length();

   // add to list
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_lCRTSUBOBJECT.Add (&so);
	MALLOCOPT_RESTORE;

   // recalc bounding box
   // BUGFIX - Dont do here CalcBoundBox ();

   return TRUE;
}

/***************************************************************************************
CRayObject::ClonesAdd - Add a set of clones to the object for rendering (such as
leaves in a tree, or trees on the ground)

inputs
   GUID           *pgCode - code identifier
   GUID           *pgSub - sub-code identifier
   PCMatrix       paMatrix - Pointer to an array of dwNumMatricies for trans and rot
   DWORD          dwNum - Number of copies of the clone
returns
   BOOL - TRUE if success
*/
BOOL CRayObject::ClonesAdd (GUID *pgCode, GUID *pgSub, PCMatrix paMatrix, DWORD dwNum)
{
   // can only have one type of object, so if there are any polygons or clones then exit
   if (m_lCRTCLONE.Num() || m_lCRTSUBOBJECT.Num() || m_dwNumTri || m_dwNumQuad)
      return FALSE;

   if (!dwNum)
      return FALSE;  // nothing to add

   CPoint apCloneBound[2];
   memset (&apCloneBound, 0, sizeof(apCloneBound));
   m_pCloneObject = m_pRenderRay ?
      m_pRenderRay->CloneGet (pgCode, pgSub, apCloneBound) :
      m_pRenderTrad->CloneGet (pgCode, pgSub, TRUE, apCloneBound, NULL);
   if (!m_pCloneObject)
      return FALSE;
   m_gCloneCode = *pgCode;
   m_gCloneSub = *pgSub;

   CRTCLONE cl;
   memset (&cl, 0, sizeof(cl));

   // add all the objects
   DWORD x, y, z, i;
   CPoint p;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_lCRTCLONE.Required (m_lCRTCLONE.Num() + dwNum);
	MALLOCOPT_RESTORE;
   for (i = 0; i < dwNum; i++) {
      cl.mMatrix.Copy (&paMatrix[i]);
      cl.mMatrix.Invert4 (&cl.mMatrixInv);
      

      // convert bounding volume
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
         p.p[0] = apCloneBound[x].p[0];
         p.p[1] = apCloneBound[y].p[1];
         p.p[2] = apCloneBound[z].p[2];
         p.p[3] = 1;
         p.MultiplyLeft (&cl.mMatrix);

         if (x || y || z) {
            cl.apBound[0].Min (&p);
            cl.apBound[1].Max (&p);
         }
         else {
            cl.apBound[0].Copy (&p);
            cl.apBound[1].Copy (&p);
         }
      } // xyz

      // bounding spehere
      cl.pBoundSphere.Average (&cl.apBound[0], &cl.apBound[1]);
      p.Subtract (&cl.apBound[0], &cl.pBoundSphere);
      cl.pBoundSphere.p[3] = p.Length();

      // add to list
      m_lCRTCLONE.Add (&cl);
   }


   // recalc bounding box
   // BUGFIX - Dont do here CalcBoundBox ();

   return TRUE;
}

#ifdef USEGRID
/***************************************************************************************
CRayObject::GridAddPoly - Calculates where the polygon is in the grid and returns
a DWORD to fill into dwGridInfo.

inputs
   PCRTPOINT        p1,p2,p3,p4 - Points in the polygon. If it's a triangle p4=NULL
   PCMatrix       pmWorldToGrid - Converts from the world to the grid points
   PCListFixed    plCRTGRID - If need to have a grid, this is the list where to add to
returns
   DWORD - Number to store in dwGridInfo
*/
DWORD CRayObject::GridAddPoly (PCRTPOINT p1, PCRTPOINT p2, PCRTPOINT p3, PCRTPOINT p4,
                               PCMatrix pmWorldToGrid, PCListFixed plCRTGRID)
{
   DWORD dwNum = p4 ? 4 : 3;
   CPoint ap[4];
   DWORD i;

   for (i = 0; i < 3; i++) {
      ap[0].p[i] = p1->p[i];
      ap[1].p[i] = p2->p[i];
      ap[2].p[i] = p3->p[i];
      if (p4)
         ap[3].p[i] = p4->p[i];
   }

   // convert all these to grid space, and get min/max
   CPoint pMin, pMax;
   for (i = 0; i < dwNum; i++) {
      ap[i].p[3] = 1;
      ap[i].MultiplyLeft (pmWorldToGrid);

      if (i) {
         pMin.Min (&ap[i]);
         pMax.Max (&ap[i]);
      }
      else {
         pMin.Copy (&ap[i]);
         pMax.Copy (&ap[i]);
      }
   }
   
   // figure out the area that they cover
   DWORD adw[2][3];
   GridCalcBox (&pMin, &pMax, &adw[0][0]);

   // make this inclusive
   adw[1][0] -= 1;
   adw[1][1] -= 1;
   adw[1][2] -= 1;

   // if this is <= 2x2x2 then just store numbers
   if ((adw[0][0]+1 >= adw[1][0]) && (adw[0][1]+1 >= adw[1][1]) && (adw[0][2]+1 >= adw[1][2])) {
      DWORD dwMin, dwMax;
      dwMin = adw[0][0] + adw[0][1] * RTGRIDDIM + adw[0][2] * RTGRIDDIM * RTGRIDDIM;
      dwMax = adw[1][0] + adw[1][1] * RTGRIDDIM + adw[1][2] * RTGRIDDIM * RTGRIDDIM;
      return 0x80000000 | (dwMax << 16) | dwMin;
   }

   // else, need to create bits
   CRTGRID g;
   memset (&g, 0, sizeof(g));
   GridDrawTriangle (&ap[0], &ap[1], &ap[2], &g);
   if (p4)
      GridDrawTriangle (&ap[0], &ap[2], &ap[3], &g);

   // add this
   plCRTGRID->Add (&g);
   
   return plCRTGRID->Num()-1;
}


/***************************************************************************************
CRayObject::GridAddBox - Calculates where the bounding box is in the grid and returns
a DWORD to fill into dwGridInfo.

inputs
   PCPoint        papBound - Pointer to an array of two points that define the bounding box
   PCMatrix       pmWorldToGrid - Converts from the world to the grid points
   PCListFixed    plCRTGRID - If need to have a grid, this is the list where to add to
returns
   DWORD - Number to store in dwGridInfo
*/
DWORD CRayObject::GridAddBox (PCPoint papBound,
                               PCMatrix pmWorldToGrid, PCListFixed plCRTGRID)
{
   CPoint ap[2];
   DWORD i;

   for (i = 0; i < 2; i++) {
      ap[i].Copy (&papBound[i]);
      ap[i].p[3] = 1;
      ap[i].MultiplyLeft (pmWorldToGrid);
   }
   
   // figure out the area that they cover
   DWORD adw[2][3];
   GridCalcBox (&ap[0], &ap[1], &adw[0][0]);

   // make this inclusive
   adw[1][0] -= 1;
   adw[1][1] -= 1;
   adw[1][2] -= 1;

   // if this is <= 2x2x2 then just store numbers
   if ((adw[0][0]+1 >= adw[1][0]) && (adw[0][1]+1 >= adw[1][1]) && (adw[0][2]+1 >= adw[1][2])) {
      DWORD dwMin, dwMax;
      dwMin = adw[0][0] + adw[0][1] * RTGRIDDIM + adw[0][2] * RTGRIDDIM * RTGRIDDIM;
      dwMax = adw[1][0] + adw[1][1] * RTGRIDDIM + adw[1][2] * RTGRIDDIM * RTGRIDDIM;
      return 0x80000000 | (dwMax << 16) | dwMin;
   }

   // else, need to create bits
   CRTGRID g;
   memset (&g, 0, sizeof(g));
   GridDrawBox (&ap[0], &ap[1], &g);

   // add this
   plCRTGRID->Add (&g);
   
   return plCRTGRID->Num()-1;
}
#endif // USEGRID

/*******************************************************************************
CRTINBOUNDSort */
static int __cdecl CRTINBOUNDSort (const void *elem1, const void *elem2 )
{
   PCRTINBOUND p1, p2;
   p1 = (PCRTINBOUND) elem1;
   p2 = (PCRTINBOUND) elem2;

   if (p1->fDistance > p2->fDistance)
      return 1;
   else if (p1->fDistance < p2->fDistance)
      return -1;
   else
      return 0;
}



/***************************************************************************************
CRayObject::CalcBoundBox - Calculates the bounding box for the entire object. This
must be called after any data in the object has been changed
*/
void CRayObject::CalcBoundBox (void)
{
   MALLOCOPT_INIT;
   BOOL fFound = FALSE;

   if (m_dwNumPoints) {
      // do nothing because will have already calculated

      fFound = TRUE;
   }
   else if (m_lCRTSUBOBJECT.Num()) {
      DWORD i,x,y,z;
      PCRTSUBOBJECT pso = (PCRTSUBOBJECT) m_lCRTSUBOBJECT.Get(0);
      CPoint p;
      for (i = 0; i < m_lCRTSUBOBJECT.Num(); i++) for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
         p.p[0] = pso[i].pObject->m_apBound[x].p[0];
         p.p[1] = pso[i].pObject->m_apBound[y].p[1];
         p.p[2] = pso[i].pObject->m_apBound[z].p[2];
         p.p[3] = 1;
         p.MultiplyLeft (&pso[i].mMatrix);

         if (fFound) {
            m_apBound[0].Min (&p);
            m_apBound[1].Max (&p);
         }
         else {
            m_apBound[0].Copy (&p);
            m_apBound[1].Copy (&p);
            fFound = TRUE;
         }
      }
   } // if sub-objects
   else if (m_lCRTCLONE.Num()) {
      DWORD i;
      PCRTCLONE pc = (PCRTCLONE) m_lCRTCLONE.Get(0);
      for (i = 0; i < m_lCRTCLONE.Num(); i++) {
         if (fFound) {
            m_apBound[0].Min (&pc[i].apBound[0]);
            m_apBound[1].Max (&pc[i].apBound[1]);
         }
         else {
            m_apBound[0].Copy (&pc[i].apBound[0]);
            m_apBound[1].Copy (&pc[i].apBound[1]);
            fFound = TRUE;
         }
      }
   } // if clones

   // if nothing found zero
   if (!fFound) {
      m_apBound[0].Zero();
      m_apBound[1].Zero();
   }

   // bounding sphere
   CPoint p;
   m_pBoundSphere.Average (&m_apBound[0], &m_apBound[1]);
   p.Subtract (&m_apBound[0], &m_pBoundSphere);
   m_pBoundSphere.p[3] = p.Length();

   // increase the bounding box just a bit so dont get divide by zeros
   DWORD i;
   if (fFound)
      for (i = 0; i < 3; i++) {
         m_apBound[0].p[i] -= CLOSE;
         m_apBound[1].p[i] += CLOSE;
      }

   // if there are enough objects then calculate the grid
   if (m_dwNumTri + m_dwNumQuad + m_lCRTSUBOBJECT.Num() + m_lCRTCLONE.Num() >= (DWORD) (m_fIsClone ? MINOBJECTFORGRID : MINOBJECTFORGRIDNOCLONE)) {
      // matrix that converts from bounding box to grid
      CMatrix m, mScale;
      CPoint pDelta;
      m.Translation (-m_apBound[0].p[0], -m_apBound[0].p[1], -m_apBound[0].p[2]);
      pDelta.Subtract (&m_apBound[1], &m_apBound[0]);
      mScale.Scale ((fp)RTGRIDDIM / pDelta.p[0], (fp)RTGRIDDIM / pDelta.p[1], (fp)RTGRIDDIM / pDelta.p[2]);
      m.MultiplyRight (&mScale);

#ifdef USEGRID
      m_lCRTGRID.Clear();
#endif // USEGRID

      // create a list so can keep the DWORD and sphere
      // CListFixed lSort;
      // Changed lSort to m_lCalcBoundBoxSort. Somewhat worried about recursion
      // but can't seem to find it happening
      CRTINBOUND b;
      m_lCalcBoundBoxSort.Init (sizeof(CRTINBOUND));

      // loop through all the triangles
      if (m_dwPolyDWORD) {
         PCRTTRIDWORD pt = (PCRTTRIDWORD) m_pTri;
         PCRTQUADDWORD pq = (PCRTQUADDWORD) m_pQuad;
         PCRTVERTEXDWORD pv = (PCRTVERTEXDWORD) m_pVertices;

         m_lCalcBoundBoxSort.Required (m_lCalcBoundBoxSort.Num() + m_dwNumTri);
         for (i = 0; i < m_dwNumTri; i++, pt++) {
#ifdef USEGRID
            pt->dwGridInfo = GridAddPoly (
               &m_pPoints[pv[pt->adwVertex[0]].dwPoint],
               &m_pPoints[pv[pt->adwVertex[1]].dwPoint],
               &m_pPoints[pv[pt->adwVertex[2]].dwPoint],
               NULL,
               &m, &m_lCRTGRID);
#endif// USEGRID
            b.dwID = i | RTPF_TRIANGLE;
            b.pBoundSphere.Copy (&pt->pBoundSphere);
            m_lCalcBoundBoxSort.Add (&b);
         } // i

         m_lCalcBoundBoxSort.Required (m_lCalcBoundBoxSort.Num() + m_dwNumQuad);
         for (i = 0; i < m_dwNumQuad; i++, pq++) {
#ifdef USEGRID
            pq->dwGridInfo = GridAddPoly (
               &m_pPoints[pv[pq->adwVertex[0]].dwPoint],
               &m_pPoints[pv[pq->adwVertex[1]].dwPoint],
               &m_pPoints[pv[pq->adwVertex[2]].dwPoint],
               &m_pPoints[pv[pq->adwVertex[3]].dwPoint],
               &m, &m_lCRTGRID);
#endif// USEGRID
            b.dwID = i | RTPF_QUAD1;
            b.pBoundSphere.Copy (&pq->pBoundSphere);
            m_lCalcBoundBoxSort.Add (&b);
         } // i
      }
      else {
         PCRTTRIWORD pt = m_pTri;
         PCRTQUADWORD pq =  m_pQuad;
         PCRTVERTEXWORD pv = m_pVertices;

	      MALLOCOPT_OKTOMALLOC;
         m_lCalcBoundBoxSort.Required (m_lCalcBoundBoxSort.Num() + m_dwNumTri);
	      MALLOCOPT_RESTORE;
         for (i = 0; i < m_dwNumTri; i++, pt++) {
#ifdef USEGRID
            pt->dwGridInfo = GridAddPoly (
               &m_pPoints[pv[pt->adwVertex[0]].dwPoint],
               &m_pPoints[pv[pt->adwVertex[1]].dwPoint],
               &m_pPoints[pv[pt->adwVertex[2]].dwPoint],
               NULL,
               &m, &m_lCRTGRID);
#endif// USEGRID
            b.dwID = i | RTPF_TRIANGLE;
            b.pBoundSphere.Copy (&pt->pBoundSphere);
            m_lCalcBoundBoxSort.Add (&b);
         } // i

	      MALLOCOPT_OKTOMALLOC;
         m_lCalcBoundBoxSort.Required (m_lCalcBoundBoxSort.Num() + m_dwNumQuad);
	      MALLOCOPT_RESTORE;
         for (i = 0; i < m_dwNumQuad; i++, pq++) {
#ifdef USEGRID
            pq->dwGridInfo = GridAddPoly (
               &m_pPoints[pv[pq->adwVertex[0]].dwPoint],
               &m_pPoints[pv[pq->adwVertex[1]].dwPoint],
               &m_pPoints[pv[pq->adwVertex[2]].dwPoint],
               &m_pPoints[pv[pq->adwVertex[3]].dwPoint],
               &m, &m_lCRTGRID);
#endif// USEGRID
            b.dwID = i | RTPF_QUAD1;
            b.pBoundSphere.Copy (&pq->pBoundSphere);
            m_lCalcBoundBoxSort.Add (&b);
         } // i
      } // word


      // sub-objects
      PCRTSUBOBJECT pso = (PCRTSUBOBJECT) m_lCRTSUBOBJECT.Get(0);
      MALLOCOPT_OKTOMALLOC;
      m_lCalcBoundBoxSort.Required (m_lCalcBoundBoxSort.Num() + m_lCRTSUBOBJECT.Num());
      MALLOCOPT_RESTORE;
      for (i = 0; i < m_lCRTSUBOBJECT.Num(); i++, pso++) {
#ifdef USEGRID
         pso->dwGridInfo = GridAddBox (pso->apBound, &m, &m_lCRTGRID);
#endif// USEGRID

         b.dwID = i;
         b.pBoundSphere.Copy (&pso->pBoundSphere);
         m_lCalcBoundBoxSort.Add (&b);
      }

      // clones
      PCRTCLONE pc = (PCRTCLONE) m_lCRTCLONE.Get(0);
      MALLOCOPT_OKTOMALLOC;
      m_lCalcBoundBoxSort.Required (m_lCalcBoundBoxSort.Num() + m_lCRTCLONE.Num());
      MALLOCOPT_RESTORE;
      for (i = 0; i < m_lCRTCLONE.Num(); i++, pc++) {
#ifdef USEGRID
         pc->dwGridInfo = GridAddBox (pc->apBound, &m, &m_lCRTGRID);
#endif// USEGRID

         b.dwID = i;
         b.pBoundSphere.Copy (&pc->pBoundSphere);
         m_lCalcBoundBoxSort.Add (&b);
      }

      // create the three sorted lists
	   MALLOCOPT_OKTOMALLOC;
      if (!m_memGridSorted.Required (m_lCalcBoundBoxSort.Num() * sizeof(DWORD) * 3)) {
   	   MALLOCOPT_RESTORE;
         return;  // error
      }
	   MALLOCOPT_RESTORE;
      for (i = 0; i < 3; i++) {
         DWORD *padw = ((DWORD*) m_memGridSorted.p) + (i * m_lCalcBoundBoxSort.Num());
         PCRTINBOUND pb = (PCRTINBOUND) m_lCalcBoundBoxSort.Get(0);
         DWORD j;

         // distance
         for (j = 0; j < m_lCalcBoundBoxSort.Num(); j++)
            pb[j].fDistance = pb[j].pBoundSphere.p[i];

         // sort
         qsort(pb, m_lCalcBoundBoxSort.Num(), sizeof (CRTINBOUND), CRTINBOUNDSort);

         // write these IDs away
         for (j = 0; j < m_lCalcBoundBoxSort.Num(); j++, pb++, padw++)
            padw[0] = pb->dwID;
      }

   } // if calc grid
}


/***************************************************************************************
CRayObject::CalcPolyBound - Given a polygon number, this fills in the bounding information
for it so that quicker intersections can be done.

inputs
   BOOL           fTriangle - if true then intersecting with triangle, else quad
   DWORD          dwNum - Number
returns
   none
*/
void CRayObject::CalcPolyBound (BOOL fTriangle, DWORD dwNum)
{
   CPoint ap[4];  // points
   PCPoint pBoundSphere;
   PCMatrix pmBound;
   DWORD dwNumVert = fTriangle ? 3 : 4;
   DWORD i, j;

   // get all the points
   if (m_dwPolyDWORD) {
      PCRTVERTEXDWORD pv = (PCRTVERTEXDWORD) m_pVertices;
      
      if (fTriangle) {
         PCRTTRIDWORD pt = (PCRTTRIDWORD) m_pTri + dwNum;
         pBoundSphere = &pt->pBoundSphere;
         pmBound = &pt->mBound;

         for (i = 0; i < dwNumVert; i++) for (j = 0; j < 3; j++)
            ap[i].p[j] = m_pPoints[pv[pt->adwVertex[i]].dwPoint].p[j];
      }
      else {   // quad
         PCRTQUADDWORD pq = (PCRTQUADDWORD) m_pQuad + dwNum;
         pBoundSphere = &pq->pBoundSphere;
         pmBound = &pq->amBound[0];

         for (i = 0; i < dwNumVert; i++) for (j = 0; j < 3; j++)
            ap[i].p[j] = m_pPoints[pv[pq->adwVertex[i]].dwPoint].p[j];
      }
   }
   else {
      PCRTVERTEXWORD pv = m_pVertices;
      
      if (fTriangle) {
         PCRTTRIWORD pt = m_pTri + dwNum;
         pBoundSphere = &pt->pBoundSphere;
         pmBound = &pt->mBound;

         for (i = 0; i < dwNumVert; i++) for (j = 0; j < 3; j++)
            ap[i].p[j] = m_pPoints[pv[pt->adwVertex[i]].dwPoint].p[j];
      }
      else {   // quad
         PCRTQUADWORD pq = m_pQuad + dwNum;
         pBoundSphere = &pq->pBoundSphere;
         pmBound = &pq->amBound[0];

         for (i = 0; i < dwNumVert; i++) for (j = 0; j < 3; j++)
            ap[i].p[j] = m_pPoints[pv[pq->adwVertex[i]].dwPoint].p[j];
      }
   }

   // find the midpoint
   CPoint pMid;
   pMid.Copy (&ap[0]);
   for (i = 1; i < dwNumVert; i++)
      pMid.Add (&ap[i]);
   pMid.Scale (1.0 / (fp)dwNumVert);

   // find the longest length
   fp fLen, f;
   CPoint pDist;
   fLen = 0;
   for (i = 0; i < dwNumVert; i++) {
      pDist.Subtract (&ap[i], &pMid);
      f = pDist.Length();
      fLen = max(f, fLen);
   }

   // store this away
   pBoundSphere->Copy (&pMid);
   pBoundSphere->p[3] = fLen;


   // calculate a matrix that converts from world space into a space where the traingel
   // or quad plane(s) is on xy and runs from 0..1. This can be used for quick bounding
   // test
   DWORD dwPlane;
   CPoint apTest[4];
   BOOL fIncludeLast;
   fIncludeLast = FALSE;
   for (dwPlane = 0; dwPlane < dwNumVert-2; dwPlane++, pmBound++) {
      CPoint pA, pB, pC;
      if (dwPlane) {
         pA.Subtract (&ap[3], &ap[2]);
         pB.Subtract (&ap[0], &ap[3]);
      }
      else {
         pA.Subtract (&ap[1], &ap[0]);
         pB.Subtract (&ap[2], &ap[1]);
      }

      pA.Normalize();
      pB.Normalize();
      pC.CrossProd (&pA, &pB);
      fLen = pC.Length();
      if ((fLen < CLOSE) || fIncludeLast) {
         // the tirangle is more a line, so cant really test. just set some magic numbers
         pmBound->p[0][0] = 1;
         pmBound->p[0][1] = 2;
         pmBound->p[0][2] = 3;
         pmBound->p[0][3] = 4;
         continue;
      }
      pC.Scale (1.0 / fLen);

      // recalc b
      pB.CrossProd (&pC, &pA);
      pB.Normalize();

      // create a matrix that converts from plane space to world space
      CMatrix mP2W, mTrans;
      mP2W.RotationFromVectors (&pA, &pB, &pC);
      mTrans.Translation (ap[0].p[0], ap[0].p[1], ap[0].p[2]);
      mP2W.MultiplyRight (&mTrans);

      // invert this so convert from world to plane space
      mP2W.Invert4 (pmBound);
      
      // run all the points through...
      memcpy (apTest, ap, sizeof(ap));
      for (i = 0; i < dwNumVert; i++) {
         apTest[i].p[3] = 1;
         apTest[i].MultiplyLeft (pmBound);
      }
      // all apTest[i] should be 0 (or close to it), except for points not on
      // the plane we're testing

      // if it's a quad, and completely flat, then just use one matrix for all 4 points,
      // saving some calculations
      fIncludeLast = FALSE;
      if ((dwNumVert == 4) && !dwPlane && (fabs(apTest[3].p[2]) < EPSILON*10.0))
         fIncludeLast = TRUE;

      // find the min and max as they appear on the plane
      CPoint pMin, pMax, pt;
      pMin.Copy (&apTest[0]);
      pMax.Copy (&pMin);
      for (i = 1; i < dwNumVert; i++) {
         if (dwPlane) { // second part of quad
            if (i == 1)
               continue;   // dont include this
         }
         else { // first part of quad
            if ((i == 3) && !fIncludeLast)
               continue;   // dont include this
         }

         pMin.Min (&apTest[i]);
         pMax.Max (&apTest[i]);
      }

      // add a bit of breathing space
      pt.Subtract (&pMax, &pMin);
      pt.Scale (.01);
      pt.p[0] += CLOSE;
      pt.p[1] += CLOSE;
      pMin.Subtract (&pt);
      pMax.Add (&pt);

      // translate the matrix to that the points go from 0..1 on x and y
      mTrans.Translation (-pMin.p[0], -pMin.p[1], 0);
      pmBound->MultiplyRight (&mTrans);

      // scale the points
      pMax.Subtract (&pMin);
      pMax.p[0] = max(pMax.p[0], EPSILON);
      pMax.p[1] = max(pMax.p[1], EPSILON);
      mTrans.Scale (1.0 / pMax.p[0], 1.0 / pMax.p[1], 1);
      pmBound->MultiplyRight (&mTrans);

#ifdef _DEBUG // for test
      memcpy (apTest, ap, sizeof(ap));
      for (i = 0; i < dwNumVert; i++) {
         apTest[i].p[3] = 1;
         apTest[i].MultiplyLeft (pmBound);
      }
#endif
      // done
   } // dwPlane
}


/***************************************************************************************
CRayObject::PolygonSet - Takes a POLYRENDERINFO structure and copies (and comresses) the
information to store away the rendered polygons.

inputs
   PPOLYRENDERINFO         pInfo - Information passed down about polygons to render
   PCMatrix                pMatrix - Matrix that's multiplied to all points
   PCWorldSocket           pWorld - World that using for getting textures
returns
   BOOL - TRUE if success
*/
BOOL CRayObject::PolygonsSet (PPOLYRENDERINFO pInfo, PCMatrix pMatrix, PCWorldSocket pWorld)
{
   // can only have one type of object, so if there are any polygons or clones then exit
   if (m_lCRTCLONE.Num() || m_lCRTSUBOBJECT.Num() || m_dwNumTri || m_dwNumQuad)
      return FALSE;

   // if already have polyinfo then error
   if (m_pmemPoly)
      return FALSE;

   if (!pInfo->dwNumPolygons)
      return FALSE;

   // if more than 64K points then will use DWORD version of vertices, triangles, and quads
   DWORD dwMax;
   dwMax = max(pInfo->dwNumColors, pInfo->dwNumNormals);
   dwMax = max(dwMax, pInfo->dwNumPoints);
   dwMax = max(dwMax, pInfo->dwNumSurfaces);
   dwMax = max(dwMax, pInfo->dwNumTextures);
   dwMax = max(dwMax, pInfo->dwNumVertices);
   m_dwPolyDWORD = (dwMax >= 0xffff);


   // how many triangles and quads
   m_dwNumTri = m_dwNumQuad = 0;
   PPOLYDESCRIPT pPoly;
   DWORD i;
   for (pPoly = pInfo->paPolygons, i = 0;
      i < pInfo->dwNumPolygons;
      i++, pPoly = (PPOLYDESCRIPT) ((DWORD*)(pPoly+1) + pPoly->wNumVertices)) {

      if (pPoly->wNumVertices <= 2)
         continue;
      else if (pPoly->wNumVertices == 3) {
         m_dwNumTri++;
         continue;
      }
      else if (pPoly->wNumVertices == 4) {
         m_dwNumQuad++;
         continue;
      }

      // else, too may sides, so split up
      m_dwNumTri += (pPoly->wNumVertices - 2);
   }

   // fill in other numbers
   m_dwNumSurfaces = pInfo->dwNumSurfaces;
   m_dwNumPoints = pInfo->dwNumPoints;
   m_dwNumNormals = pInfo->dwNumNormals;
   m_dwNumTextures = pInfo->dwNumTextures;
   m_dwNumColors = pInfo->dwNumColors;
   m_dwNumVertices = pInfo->dwNumVertices;

   // allocate the memory
   DWORD dwNeed;
   dwNeed = m_dwNumSurfaces * sizeof(CRTSURF);
   if (m_pRenderTrad)
      dwNeed += m_dwNumSurfaces * sizeof(RENDERSURFACE);
   dwNeed += m_dwNumPoints * sizeof(CRTPOINT);
   dwNeed += m_dwNumNormals * sizeof(CRTNORMAL);
   dwNeed += m_dwNumTextures * sizeof(TEXTPOINT5);
   dwNeed += m_dwNumColors * sizeof(COLORREF);
   dwNeed += m_dwNumVertices * (m_dwPolyDWORD ? sizeof(CRTVERTEXDWORD) : sizeof(CRTVERTEXWORD));
   dwNeed += m_dwNumTri * (m_dwPolyDWORD ? sizeof(CRTTRIDWORD) : sizeof(CRTTRIWORD));
   dwNeed += m_dwNumQuad * (m_dwPolyDWORD ? sizeof(CRTQUADDWORD) : sizeof(CRTQUADWORD));
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_pmemPoly = new CMem;
	MALLOCOPT_RESTORE;
   if (!m_pmemPoly)
      return FALSE;
	MALLOCOPT_OKTOMALLOC;
   if (!m_pmemPoly->Required (dwNeed)) {
   	MALLOCOPT_RESTORE;
      delete m_pmemPoly;
      m_pmemPoly = NULL;
      return FALSE;
   }
	MALLOCOPT_RESTORE;
   m_pSurfaces = (PCRTSURF) m_pmemPoly->p;
   if (m_pRenderTrad) {
      m_pRENDERSURFACE = (PRENDERSURFACE) (m_pSurfaces + m_dwNumSurfaces);
      m_pPoints = (PCRTPOINT) (m_pRENDERSURFACE + m_dwNumSurfaces);
   }
   else {
      m_pRENDERSURFACE = NULL;
      m_pPoints = (PCRTPOINT) (m_pSurfaces + m_dwNumSurfaces);
   }
   m_pNormals = (PCRTNORMAL) (m_pPoints + m_dwNumPoints);
   m_pTextures = (PTEXTPOINT5) (m_pNormals + m_dwNumNormals);
   m_pColors = (COLORREF*) (m_pTextures + m_dwNumTextures);
   m_pVertices = (PCRTVERTEXWORD) (m_pColors + m_dwNumColors);
   PCRTVERTEXDWORD pDWVertices;
   pDWVertices = (PCRTVERTEXDWORD) m_pVertices;

   m_pTri = m_dwPolyDWORD ? (PCRTTRIWORD)(pDWVertices + m_dwNumVertices) : (PCRTTRIWORD)(m_pVertices + m_dwNumVertices);
   PCRTTRIDWORD pDWTri;
   pDWTri = (PCRTTRIDWORD) m_pTri;

   m_pQuad = m_dwPolyDWORD ? (PCRTQUADWORD)(pDWTri + m_dwNumTri) : (PCRTQUADWORD)(m_pTri + m_dwNumTri);
   PCRTQUADDWORD pDWQuad;
   pDWQuad = (PCRTQUADDWORD) m_pTri;

   // potentially copy over the surfaces
   if (m_pRENDERSURFACE)
      memcpy (m_pRENDERSURFACE, pInfo->paSurfaces, sizeof(RENDERSURFACE)*pInfo->dwNumSurfaces);

   // convert all the surfaces
   for (i = 0; i < m_dwNumSurfaces; i++) {
      m_pSurfaces[i].wMinorID = pInfo->paSurfaces[i].wMinorID;
      memcpy (&m_pSurfaces[i].Material, &pInfo->paSurfaces[i].Material, sizeof(m_pSurfaces[i].Material));

      if (pInfo->paSurfaces[i].fUseTextureMap) {
         m_pSurfaces[i].pTexture = TextureCacheGet (m_dwRenderShard, &pInfo->paSurfaces[i], NULL, NULL);

         // BUGFIX - tellthe texture to cache everythingf
         m_pSurfaces[i].pTexture->ForceCache(
            m_pRenderRay ? (FORCECACHE_ALL & ~FORCECACHE_COMBINED) : m_pRenderTrad->m_dwForceCache);
               // on render traditional, make sure it's all cached, just in case???
      }
      else
         m_pSurfaces[i].pTexture = NULL;
   }

   // convert all points
   // NOTE: Calc bounding box for polygons while at it
   CPoint p;
   m_apBound[0].Zero();
   m_apBound[1].Zero();
   for (i = 0; i < m_dwNumPoints; i++) {
      pInfo->paPoints[i].p[3] = 1;
      pMatrix->Multiply (&pInfo->paPoints[i], &p);
      m_pPoints[i].p[0] = p.p[0];
      m_pPoints[i].p[1] = p.p[1];
      m_pPoints[i].p[2] = p.p[2];

      // bounding box
      if (i) {
         m_apBound[0].Min (&p);
         m_apBound[1].Max (&p);
      }
      else {
         m_apBound[0].Copy (&p);
         m_apBound[1].Copy (&p);
      }
   }
   
   // convert all normals
   CMatrix mNorm;
   pMatrix->Invert (&mNorm);
   mNorm.Transpose();
   mNorm.MakeSquare();
   for (i = 0; i < m_dwNumNormals; i++) {
      p.Copy (&pInfo->paNormals[i]);
      p.p[3] = 1;
      p.MultiplyLeft (&mNorm);
      p.Normalize ();   // just in case

      short as[3];
      as[0] = (short) (p.p[0] * 32767.0);
      as[1] = (short) (p.p[1] * 32767.0);
      as[2] = (short) (p.p[2] * 32767.0);

      m_pNormals[i].dw = ((DWORD)(WORD)as[0] >> 6) |
         (((DWORD)(WORD)as[1] >> 6) << 10) |
         (((DWORD)(WORD)as[2] >> 6) << 20);

#if 0 // to test
      DWORD dw;
      CPoint pNorm;
      dw = m_pNormals[i].dw;
      as[0] = (short)(WORD)((dw & 0x3ff) << 6);
      as[1] = (short)(WORD)(((dw >> 10) & 0x3ff) << 6);
      as[2] = (short)(WORD)(((dw >> 20) & 0x3ff) << 6);
      pNorm.p[0] = (fp)as[0] / 32767.0;
      pNorm.p[1] = (fp)as[1] / 32767.0;
      pNorm.p[2] = (fp)as[2] / 32767.0;
#endif
   }

   // textures
   memcpy (m_pTextures, pInfo->paTextures, m_dwNumTextures * sizeof(TEXTPOINT5));

   // colors
   memcpy (m_pColors, pInfo->paColors, m_dwNumColors * sizeof(COLORREF));

   // vertices
   if (m_dwPolyDWORD) {
      for (i = 0; i < m_dwNumVertices; i++) {
         pDWVertices[i].dwColor = pInfo->paVertices[i].dwColor;
         pDWVertices[i].dwPoint = pInfo->paVertices[i].dwPoint;
         pDWVertices[i].dwTexture = pInfo->paVertices[i].dwTexture;
         pDWVertices[i].dwNormal = pInfo->paVertices[i].dwNormal;
      }
   }
   else {
      for (i = 0; i < m_dwNumVertices; i++) {
         m_pVertices[i].dwColor = (WORD) pInfo->paVertices[i].dwColor;
         m_pVertices[i].dwPoint = (WORD) pInfo->paVertices[i].dwPoint;
         m_pVertices[i].dwTexture = (WORD) pInfo->paVertices[i].dwTexture;
         m_pVertices[i].dwNormal = (WORD) pInfo->paVertices[i].dwNormal;
      }
   }

   // polygons
   DWORD dwCurTri, dwCurQuad;
   DWORD j;
   dwCurTri = dwCurQuad = 0;
   for (pPoly = pInfo->paPolygons, i = 0;
      i < pInfo->dwNumPolygons;
      i++, pPoly = (PPOLYDESCRIPT) ((DWORD*)(pPoly+1) + pPoly->wNumVertices)) {

      if (pPoly->wNumVertices <= 2)
         continue;
      else if (pPoly->wNumVertices == 3) {
         if (m_dwPolyDWORD) {
            pDWTri[dwCurTri].adwVertex[0] = ((DWORD*) (pPoly+1))[0];
            pDWTri[dwCurTri].adwVertex[1] = ((DWORD*) (pPoly+1))[1];
            pDWTri[dwCurTri].adwVertex[2] = ((DWORD*) (pPoly+1))[2];
            pDWTri[dwCurTri].dwSurface = pPoly->dwSurface;
            pDWTri[dwCurTri].dwIDPart = pPoly->dwIDPart;
            pDWTri[dwCurTri].fCanBackfaceCull = pPoly->fCanBackfaceCull;

            // calculate some bounding parameters so can quickly intersect
            CalcPolyBound (TRUE, dwCurTri);
         }
         else {
            m_pTri[dwCurTri].adwVertex[0] = ((DWORD*) (pPoly+1))[0];
            m_pTri[dwCurTri].adwVertex[1] = ((DWORD*) (pPoly+1))[1];
            m_pTri[dwCurTri].adwVertex[2] = ((DWORD*) (pPoly+1))[2];
            m_pTri[dwCurTri].dwSurface = pPoly->dwSurface;
            m_pTri[dwCurTri].dwIDPart = pPoly->dwIDPart;
            m_pTri[dwCurTri].fCanBackfaceCull = pPoly->fCanBackfaceCull;

            // calculate some bounding parameters so can quickly intersect
            CalcPolyBound (TRUE, dwCurTri);
         }

         dwCurTri++;
         continue;
      }
      else if (pPoly->wNumVertices == 4) {
         if (m_dwPolyDWORD) {
            pDWQuad[dwCurQuad].adwVertex[0] = ((DWORD*) (pPoly+1))[0];
            pDWQuad[dwCurQuad].adwVertex[1] = ((DWORD*) (pPoly+1))[1];
            pDWQuad[dwCurQuad].adwVertex[2] = ((DWORD*) (pPoly+1))[2];
            pDWQuad[dwCurQuad].adwVertex[3] = ((DWORD*) (pPoly+1))[3];
            pDWQuad[dwCurQuad].dwSurface = pPoly->dwSurface;
            pDWQuad[dwCurQuad].dwIDPart = pPoly->dwIDPart;
            pDWQuad[dwCurQuad].fCanBackfaceCull = pPoly->fCanBackfaceCull;

            // calculate some bounding parameters so can quickly intersect
            CalcPolyBound (FALSE, dwCurQuad);
         }
         else {
            m_pQuad[dwCurQuad].adwVertex[0] = ((DWORD*) (pPoly+1))[0];
            m_pQuad[dwCurQuad].adwVertex[1] = ((DWORD*) (pPoly+1))[1];
            m_pQuad[dwCurQuad].adwVertex[2] = ((DWORD*) (pPoly+1))[2];
            m_pQuad[dwCurQuad].adwVertex[3] = ((DWORD*) (pPoly+1))[3];
            m_pQuad[dwCurQuad].dwSurface = pPoly->dwSurface;
            m_pQuad[dwCurQuad].dwIDPart = pPoly->dwIDPart;
            m_pQuad[dwCurQuad].fCanBackfaceCull = pPoly->fCanBackfaceCull;

            // calculate some bounding parameters so can quickly intersect
            CalcPolyBound (FALSE, dwCurQuad);
         }
         dwCurQuad++;
         continue;
      }

      // else, too may sides, so split up
      for (j = 1; j+1 < (DWORD)pPoly->wNumVertices; j++) {
         if (m_dwPolyDWORD) {
            pDWTri[dwCurTri].adwVertex[0] = ((DWORD*) (pPoly+1))[0];
            pDWTri[dwCurTri].adwVertex[1] = ((DWORD*) (pPoly+1))[j];
            pDWTri[dwCurTri].adwVertex[2] = ((DWORD*) (pPoly+1))[j+1];
            pDWTri[dwCurTri].dwSurface = pPoly->dwSurface;
            pDWTri[dwCurTri].dwIDPart = pPoly->dwIDPart;
            pDWTri[dwCurTri].fCanBackfaceCull = pPoly->fCanBackfaceCull;

            // calculate some bounding parameters so can quickly intersect
            CalcPolyBound (TRUE, dwCurTri);
         }
         else {
            m_pTri[dwCurTri].adwVertex[0] = ((DWORD*) (pPoly+1))[0];
            m_pTri[dwCurTri].adwVertex[1] = ((DWORD*) (pPoly+1))[j];
            m_pTri[dwCurTri].adwVertex[2] = ((DWORD*) (pPoly+1))[j+1];
            m_pTri[dwCurTri].dwSurface = pPoly->dwSurface;
            m_pTri[dwCurTri].dwIDPart = pPoly->dwIDPart;
            m_pTri[dwCurTri].fCanBackfaceCull = pPoly->fCanBackfaceCull;

            // calculate some bounding parameters so can quickly intersect
            CalcPolyBound (TRUE, dwCurTri);
         }
         dwCurTri++;
      } // j, split up
   } // i, all polygons

   // calc bounding box for all
   // BUGFIX - Dont do here CalcBoundBox();

   return TRUE;
}

/***************************************************************************************
GuaranteedOpaque - Returns TRUE if the surface is guaranteed opaque.

inputs
   DWORD          dwThread - Thread, 0..MAXRAYTHREAD-1
   PCRTSURF       pSurf - Surface
returns
   BOOL - TRUE if always opaque
*/
static BOOL GuaranteedOpaque (DWORD dwThread, PCRTSURF pSurf)
{
   if (!pSurf->pTexture)
      return (pSurf->Material.m_wTransparency == 0);

   // else texture
   if (pSurf->Material.m_wTransparency || pSurf->pTexture->MightBeTransparent(dwThread))
      return FALSE;
   else
      return TRUE;
}

#ifdef USEGRID
/***************************************************************************************
CRayObject::TestAgainstGrid - See if this line should intersect the object.

inputs
   PCRTGRID       pLine - Grid containing the line
   DWORD          dwGridInfo - Grid info from the object
returns
   BOOL - TRUE if intersects, FALSE if miss
*/
BOOL CRayObject::TestAgainstGrid (PCRTGRID pLine, DWORD dwGridInfo)
{
   if (dwGridInfo & 0x80000000) {
      // has a range stored, so pull out
      DWORD adwMinMax[2];
      DWORD i,x,y,z;
      adwMinMax[0] = LOWORD(dwGridInfo);
      adwMinMax[1] = HIWORD(dwGridInfo) & 0x7fff;
      DWORD adw[2][3];
      for (i = 0; i < 2; i++) {
         adw[i][2] = adwMinMax[i] / (RTGRIDDIM * RTGRIDDIM);
         adwMinMax[i] -= adw[i][2] * RTGRIDDIM * RTGRIDDIM;
         adw[i][1] = adwMinMax[i] / RTGRIDDIM;
         adwMinMax[i] -= adw[i][1] * RTGRIDDIM;
         adw[i][0] = adwMinMax[i];
      }

      // loop
      for (z = adw[0][2]; z <= adw[1][2]; z++)
      for (y = adw[0][1]; y <= adw[1][1]; y++)
      for (x = adw[0][0]; x <= adw[1][0]; x++)
         if (GridTestIntersectBit (x + y * RTGRIDDIM + z * RTGRIDDIM * RTGRIDDIM, pLine))
            return TRUE;

      return FALSE;  // didnt hit
   }

   // else, just compare whole grid
   PCRTGRID pg;
   pg = (PCRTGRID) m_lCRTGRID.Get(dwGridInfo);
   if (!pg)
      return TRUE;   // shouldnt happen

   return GridTestIntersect (pLine, pg);
}
#endif// USEGRID


/***************************************************************************************
CRayObject::RayIntersectPoly - Intersect against a specific polygon in an object

inputs
   DWORD          dwThread - Thread to intersect with
   PCRTGRID       pGrid - if not NULL then this grid has the line's path; use it to
                  determine what it intersects with
   PCRTRAYPATH    pRay - Ray to intersect against
   DWORD          dwRay - Ray ID, a combination of RTPF_XXX and a tri/quad number
   BOOL           fNoBoundSphere - If TRUE then skip the bounding sphere check
returns
   BOOL - TRUE if intersected something
*/
BOOL CRayObject::RayIntersectPoly (DWORD dwThread, PCRTGRID pGrid, PCRTRAYPATH pRay, DWORD dwRay, BOOL fNoBoundSphere)
{

   CPoint ap[4];
   PCRTPOINT pp;
   DWORD i, j, k;
   fp fAlpha;
   DWORD dwQuadFlags;
   BOOL fInterA;
   BOOL fTriangle;
   i = dwRay & RTPF_NUMMASK;
   fTriangle = ((dwRay & RTPF_FLAGMASK) == RTPF_TRIANGLE);

   if (fNoBoundSphere)
      goto skipbound;

   // get a pointer to the bounding sphere, and do some quick tests for trivial reject
   PCPoint pBoundSphere;
   PCMatrix pmBound;
#ifdef USEGRID
   DWORD dwGridInfo;
#endif// USEGRID
   if (m_dwPolyDWORD) {
      if (fTriangle) {
         PCRTTRIDWORD pt = (PCRTTRIDWORD) m_pTri + i;
         if (i >= m_dwNumTri)
            return FALSE;

         // if are detecting for a light and run into a material that doesn't cast shadows
         // then ignore it
         if (pRay->fStopAtAnyOpaque && (m_pSurfaces[pt->dwSurface].Material.m_fNoShadows))
            return FALSE;

         // if this is on the list of do-not-intersect then skip
         if ((pRay->pDontIntersect == this) && (pRay->dwDontIntersect == (i | RTPF_TRIANGLE)))
            return FALSE;

         pBoundSphere = &pt->pBoundSphere;
         pmBound = &pt->mBound;
#ifdef USEGRID
         dwGridInfo = pt->dwGridInfo;
#endif// USEGRID
      }
      else {   // quad
         PCRTQUADDWORD pq = (PCRTQUADDWORD) m_pQuad + i;
         if (i >= m_dwNumQuad)
            return FALSE;

         // if are detecting for a light and run into a material that doesn't cast shadows
         // then ignore it
         if (pRay->fStopAtAnyOpaque && (m_pSurfaces[pq->dwSurface].Material.m_fNoShadows))
            return FALSE;

         pBoundSphere = &pq->pBoundSphere;
         pmBound = &pq->amBound[0]; // BUGFIX - Was incorrectly getting amBound[2]
#ifdef USEGRID
         dwGridInfo = pq->dwGridInfo;
#endif// USEGRID
      }
   }
   else {
      if (fTriangle) {
         PCRTTRIWORD pt = m_pTri + i;
         if (i >= m_dwNumTri)
            return FALSE;


         // if are detecting for a light and run into a material that doesn't cast shadows
         // then ignore it
         if (pRay->fStopAtAnyOpaque && (m_pSurfaces[pt->dwSurface].Material.m_fNoShadows))
            return FALSE;

         // if this is on the list of do-not-intersect then skip
         if ((pRay->pDontIntersect == this) && (pRay->dwDontIntersect == (i | RTPF_TRIANGLE)))
            return FALSE;

         pBoundSphere = &pt->pBoundSphere;
         pmBound = &pt->mBound;
#ifdef USEGRID
         dwGridInfo = pt->dwGridInfo;
#endif// USEGRID
      }
      else {   // quad
         PCRTQUADWORD pq = m_pQuad + i;
         if (i >= m_dwNumQuad)
            return FALSE;


         // if are detecting for a light and run into a material that doesn't cast shadows
         // then ignore it
         if (pRay->fStopAtAnyOpaque && (m_pSurfaces[pq->dwSurface].Material.m_fNoShadows))
            return FALSE;

         pBoundSphere = &pq->pBoundSphere;
         pmBound = &pq->amBound[0]; // BUGFIX - Was incorrectly getting amBound[2]
#ifdef USEGRID
         dwGridInfo = pq->dwGridInfo;
#endif// USEGRID
      }
   } // get bounding sphere

#ifdef USEGRID
   // intersect grid test
   if (pGrid)
      if (!TestAgainstGrid (pGrid, dwGridInfo))
         return FALSE;
#endif// USEGRID

   // test against bounding sphere and see if miss
   DWORD dwInter;
   fp af[2];
   dwInter = QuickIntersectNormLineSphere (&pRay->pStart, &pRay->pDir, pBoundSphere, /*pRay->fAlpha,*/ af);
   if (!dwInter)
      return FALSE;  // totally miss

   // if the lowest alpha (0) is >= the current alpha then this object occurs after
   // our current list. Or, if the highest intersection is less than the start
   fp fMin, fMax, f;
   if (dwInter == 2) {
      fMin = min(af[0], af[1]);
      fMax = max(af[0], af[1]);
   }
   else
      fMin = fMax = af[0];
   if ((fMin >= pRay->fAlpha) || (fMax <= 0))
      return FALSE;  // miss



   // try a longer test... test against flat plane
   CPoint ps, pe;
   BOOL fSecondChance;
   if (fTriangle) {
      fSecondChance = FALSE;
   }
   else {
      if ((pmBound[1].p[0][0] != 1) || (pmBound[1].p[0][1] != 2) || (pmBound[1].p[0][2] != 3) || (pmBound[1].p[0][3] != 4))
         fSecondChance = TRUE;
      else
         fSecondChance = FALSE;
   }

   //BOOL fInfinite;
   DWORD dwPass;
   for (dwPass = 0; dwPass < (DWORD) (fSecondChance ? 1 : 2); dwPass++, pmBound++) {
      // if it's the error matrix then do something
      if ((pmBound[1].p[0][0] == 1) && (pmBound[1].p[0][1] == 2) && (pmBound[1].p[0][2] == 3) && (pmBound[1].p[0][3] == 4))
         break;   // cant really tell, so need better intersect

      // convert the start and end points of the ray
      ps.Copy (&pRay->pStart);
      pe.Copy (&pRay->pDir);
      //if (pRay->fAlpha > 1000)
      //   fInfinite = TRUE;
      //else {
         pe.Scale (pRay->fAlpha);
      //   fInfinite = FALSE;
      //}
      pe.Add (&pRay->pStart);
      ps.p[3] = pe.p[3] = 1;
      ps.MultiplyLeft (pmBound);
      pe.MultiplyLeft (pmBound);

      // trivial reject, if on same side then dont his
      if (/*!fInfinite &&*/ (ps.p[2] * pe.p[2] >= -EPSILON)) {
         // doesn't intersect
         if (!fSecondChance)
            return FALSE;

         // else, its a quad on the first go and cant tell about second
         break;
      }

      // find out where intersect
      pe.Subtract (&ps);
      f = (0.0 - ps.p[2]) / pe.p[2];
      pe.p[0] = ps.p[0] + pe.p[0] * f;
      pe.p[1] = ps.p[1] + pe.p[1] * f;

      if ((pe.p[0] >= 0) && (pe.p[0] <= 1) && (pe.p[1] >= 0) && (pe.p[1] <= 1))
         break; // intersects

      // else, missed
      if (!fSecondChance)
         return FALSE;

      // else, missed, but there's another triangle to test
   }


skipbound:
   // test aginst intersection with using full intersection test
   if (m_dwPolyDWORD) {
      PCRTVERTEXDWORD pv = (PCRTVERTEXDWORD) m_pVertices;
      
      if (fTriangle) {
         PCRTTRIDWORD pt = (PCRTTRIDWORD) m_pTri + i;
         for (j = 0; j < 3; j++) {
            pp = m_pPoints + pv[pt->adwVertex[j]].dwPoint;
            for (k = 0; k < 3; k++)
               ap[j].p[k] = pp->p[k];
         }

         if (!IntersectLineTriangle (&pRay->pStart, &pRay->pDir, &ap[0], &ap[1], &ap[2], NULL, &fAlpha))
            return FALSE;

         // BUGFIX - Changed to close so edge between two triangles wont cause shadow
         if ((fAlpha > DISTFROMSTART) && (fAlpha < pRay->fAlpha)) {
            pRay->fAlpha = fAlpha;
            pRay->dwInterPolygon = i | RTPF_TRIANGLE;
            if (pRay->fStopAtAnyOpaque && GuaranteedOpaque (dwThread, &m_pSurfaces[pt->dwSurface]))
               pRay->fInterOpaque = TRUE;
            pRay->pInterStart.Copy (&pRay->pStart);
            pRay->pInterDir.Copy (&pRay->pDir);
            pRay->mInterToWorld.Copy (&pRay->mToWorld);
            pRay->pInterRayObject = this;
            
            return TRUE;
         }
      } // if triangle
      else { // quad
         PCRTQUADDWORD pq = (PCRTQUADDWORD) m_pQuad + i;

         // if this is on the list of do-not-intersect then skip
         dwQuadFlags = 0;
         if ((pRay->pDontIntersect == this) && ((pRay->dwDontIntersect & RTPF_NUMMASK) == i) ) {
            // already intersected with the quad before, so make sure that dont test
            // with intersect against the same aprt of the quad
            if ((pRay->dwDontIntersect & RTPF_FLAGMASK) == RTPF_QUAD1)
               dwQuadFlags = 0x02;
            else if ((pRay->dwDontIntersect & RTPF_FLAGMASK) == RTPF_QUAD2)
               dwQuadFlags =  0x04;
         }

         for (j = 0; j < 4; j++) {
            pp = m_pPoints + pv[pq->adwVertex[j]].dwPoint;
            for (k = 0; k < 3; k++)
               ap[j].p[k] = pp->p[k];
         }

         if (!IntersectLineQuad (&pRay->pStart, &pRay->pDir, &ap[0], &ap[1], &ap[2], &ap[3],
            NULL, &fAlpha, NULL, &fInterA, dwQuadFlags | 0x01))
            return FALSE;

         // BUGFIX - Changed to close so edge between two triangles wont cause shadow
         if ((fAlpha > DISTFROMSTART) && (fAlpha < pRay->fAlpha)) {
            pRay->fAlpha = fAlpha;
            pRay->dwInterPolygon = i | (fInterA ? RTPF_QUAD1 : RTPF_QUAD2);
            if (pRay->fStopAtAnyOpaque && GuaranteedOpaque (dwThread, &m_pSurfaces[pq->dwSurface]))
               pRay->fInterOpaque = TRUE;
            pRay->pInterStart.Copy (&pRay->pStart);
            pRay->pInterDir.Copy (&pRay->pDir);
            pRay->mInterToWorld.Copy (&pRay->mToWorld);
            pRay->pInterRayObject = this;
            
            return TRUE;
         }
      } // if quad
   } // if dword
   else { // word
      // NOTE: This is practically an exact copy of DWORD above
      PCRTVERTEXWORD pv = m_pVertices;
      
      if (fTriangle) {
         PCRTTRIWORD pt = m_pTri + i;
         for (j = 0; j < 3; j++) {
            pp = m_pPoints + pv[pt->adwVertex[j]].dwPoint;
            for (k = 0; k < 3; k++)
               ap[j].p[k] = pp->p[k];
         }

         if (!IntersectLineTriangle (&pRay->pStart, &pRay->pDir, &ap[0], &ap[1], &ap[2], NULL, &fAlpha))
            return FALSE;

         // BUGFIX - Changed to close so edge between two triangles wont cause shadow
         if ((fAlpha > DISTFROMSTART) && (fAlpha < pRay->fAlpha)) {
            pRay->fAlpha = fAlpha;
            pRay->dwInterPolygon = i | RTPF_TRIANGLE;
            if (pRay->fStopAtAnyOpaque && GuaranteedOpaque (dwThread, &m_pSurfaces[pt->dwSurface]))
               pRay->fInterOpaque = TRUE;
            pRay->pInterStart.Copy (&pRay->pStart);
            pRay->pInterDir.Copy (&pRay->pDir);
            pRay->mInterToWorld.Copy (&pRay->mToWorld);
            pRay->pInterRayObject = this;
            
            return TRUE;
         }
      } // if tri
      else { // quad
         PCRTQUADWORD pq = m_pQuad + i;

         // if this is on the list of do-not-intersect then skip
         dwQuadFlags = 0;
         if ((pRay->pDontIntersect == this) && ((pRay->dwDontIntersect & RTPF_NUMMASK) == i) ) {
            // already intersected with the quad before, so make sure that dont test
            // with intersect against the same aprt of the quad
            if ((pRay->dwDontIntersect & RTPF_FLAGMASK) == RTPF_QUAD1)
               dwQuadFlags = 0x02;
            else if ((pRay->dwDontIntersect & RTPF_FLAGMASK) == RTPF_QUAD2)
               dwQuadFlags =  0x04;
         }


         for (j = 0; j < 4; j++) {
            pp = m_pPoints + pv[pq->adwVertex[j]].dwPoint;
            for (k = 0; k < 3; k++)
               ap[j].p[k] = pp->p[k];
         }

         if (!IntersectLineQuad (&pRay->pStart, &pRay->pDir, &ap[0], &ap[1], &ap[2], &ap[3],
            NULL, &fAlpha, NULL, &fInterA, dwQuadFlags | 0x01))
            return FALSE;

         // BUGFIX - Changed to close so edge between two triangles wont cause shadow
         if ((fAlpha > DISTFROMSTART) && (fAlpha < pRay->fAlpha)) {
            pRay->fAlpha = fAlpha;
            pRay->dwInterPolygon = i | (fInterA ? RTPF_QUAD1 : RTPF_QUAD2);
            if (pRay->fStopAtAnyOpaque && GuaranteedOpaque (dwThread, &m_pSurfaces[pq->dwSurface]))
               pRay->fInterOpaque = TRUE;
            pRay->pInterStart.Copy (&pRay->pStart);
            pRay->pInterDir.Copy (&pRay->pDir);
            pRay->mInterToWorld.Copy (&pRay->mToWorld);
            pRay->pInterRayObject = this;
            
            return TRUE;
         }
      } // if quad
   } // if word

   return FALSE;
}


/***************************************************************************************
CRayObject::RayIntersectSub - Intersect against a specific sub-object in an object

inputs
   DWORD          dwThread - Thread to intersect with
   DWORD          dwBound - ID of the bounding volume
   PCRTGRID       pGrid - if not NULL then this grid has the line's path; use it to
                  determine what it intersects with
   PCRTRAYPATH    pRay - Ray to intersect against
   DWORD          dwSub - Sub object number.
   BOOL           fNoBoundSphere - If TRUE then skip the bounding sphere check
returns
   BOOL - TRUE if intersected something
*/
BOOL CRayObject::RayIntersectSub (DWORD dwThread, DWORD dwBound, PCRTGRID pGrid, PCRTRAYPATH pRay,
                                  DWORD dwSub,BOOL fNoBoundSphere)
{
   PCRTSUBOBJECT po = (PCRTSUBOBJECT) m_lCRTSUBOBJECT.Get(dwSub);
   BOOL fSub;
   if (!po)
      return FALSE;

#ifdef USEGRID
   // intersect grid test
   if (!fNoBoundSphere && pGrid)
      if (!TestAgainstGrid (pGrid, po->dwGridInfo))
         return FALSE;
#endif// USEGRID


   if (po->fMatrixIdent)
      // identity, so no coord transforms
      return po->pObject->RayIntersect (dwThread, dwBound, pRay, fNoBoundSphere);

   // else, need to transform coordinates
   // NOTE: Haven't really tested because hasn't occured so far, but duplicate of clone code
   // so should be ok

   // remember original values
   CPoint pOrigStart, pOrigDir;
   CMatrix mOrigToWorld;
   fp fOrigAlpha;
   pOrigStart.Copy (&pRay->pStart);
   pOrigDir.Copy (&pRay->pDir);
   fOrigAlpha = pRay->fAlpha;
   mOrigToWorld.Copy (&pRay->mToWorld);

   // not identity, so transform coords
   CPoint pSTrans, pDTrans;
   fp fLen, fLenInv;
   pSTrans.Copy (&pRay->pStart);
   pSTrans.p[3] = 1;
   pSTrans.MultiplyLeft (&po->mMatrixInv);
   pDTrans.Add (&pRay->pStart, &pRay->pDir);
   pDTrans.p[3] = 1;
   pDTrans.MultiplyLeft (&po->mMatrixInv);
   pDTrans.Subtract (&pSTrans);
   fLen = pDTrans.Length();
   if (fabs(fLen) < EPSILON)
      return FALSE;
   fLenInv = 1.0 / fLen;
   pDTrans.Scale (fLenInv);

   // substitude
   pRay->pStart.Copy (&pSTrans);
   pRay->pDir.Copy (&pDTrans);
   pRay->fAlpha *= fLen;
   pRay->mToWorld.MultiplyLeft (&po->mMatrix);


   // reverse
   fSub = po->pObject->RayIntersect (dwThread, dwBound, pRay, fNoBoundSphere);

   // restore
   pRay->pStart.Copy (&pOrigStart);
   pRay->pDir.Copy (&pOrigDir);
   pRay->mToWorld.Copy (&mOrigToWorld);

   if (fSub) {
      pRay->fAlpha *= fLenInv;  // undo the transform
      return TRUE;
   }
   else
      pRay->fAlpha = fOrigAlpha;

   return FALSE;
}


/***************************************************************************************
TestAgainstBoundSphere - Tests to see if a ray intersects a bounting sphere.

inputs
   PCRTRAYPATH          pRay - Ray
   PCPoint              pBoundSphere - Bounding sphere
returns
   BOOL - TRUE if does intersect, FALSE if doesn't
*/
static BOOL TestAgainstBoundSphere (PCRTRAYPATH pRay, PCPoint pBoundSphere)
{
   if (!pBoundSphere->p[3])
      return FALSE;  // nothing there

   // see if just miss the entire object
   DWORD dwInter;
   fp af[2];
   dwInter = QuickIntersectNormLineSphere (&pRay->pStart, &pRay->pDir, pBoundSphere, /*pRay->fAlpha,*/ af);
   if (dwInter < 2)
      return FALSE;  // totally miss

   // if the lowest alpha (0) is >= the current alpha then this object occurs after
   // our current list. Or, if the highest intersection is less than the start
   fp fMin, fMax;
   fMin = min(af[0], af[1]);
   fMax = max(af[0], af[1]);
   if ((fMin >= pRay->fAlpha) || (fMax <= 0))
      return FALSE;  // miss

   return TRUE;   // must hit
}


/***************************************************************************************
CRayObject::RayIntersectClone - Intersect against a specific clone in an object

inputs
   DWORD          dwThread - Thread to intersect with
   PCRTRAYPATH    pRay - Ray to intersect against
   PCRTGRID       pGrid - if not NULL then this grid has the line's path; use it to
                  determine what it intersects with
   DWORD          dwSub - Sub object number.
   BOOL           fNoBoundSphere - If TRUE then skip the bounding sphere check
returns
   BOOL - TRUE if intersected something
*/
BOOL CRayObject::RayIntersectClone (DWORD dwThread, PCRTGRID pGrid, PCRTRAYPATH pRay,
                                  DWORD dwSub,BOOL fNoBoundSphere)
{
   PCRTCLONE po = (PCRTCLONE) m_lCRTCLONE.Get(dwSub);
   BOOL fSub;
   if (!po)
      return FALSE;


   if (!fNoBoundSphere) {
#ifdef USEGRID
      // intersect grid test
      if (pGrid)
         if (!TestAgainstGrid (pGrid, po->dwGridInfo))
            return FALSE;
#endif// USEGRID

      // BUGFIX - intersect against bounding sphere before test, and when go to
      // ray intersect skip intersection test
      if (!TestAgainstBoundSphere(pRay, &po->pBoundSphere))
         return FALSE;
   }

   // else, need to transform coordinates

   // remember original values
   CPoint pOrigStart, pOrigDir;
   CMatrix mOrigToWorld;
   fp fOrigAlpha;
   pOrigStart.Copy (&pRay->pStart);
   pOrigDir.Copy (&pRay->pDir);
   fOrigAlpha = pRay->fAlpha;
   mOrigToWorld.Copy (&pRay->mToWorld);

   // not identity, so transform coords
   CPoint pSTrans, pDTrans;
   fp fLen, fLenInv;
   pSTrans.Copy (&pRay->pStart);
   pSTrans.p[3] = 1;
   pSTrans.MultiplyLeft (&po->mMatrixInv);
   pDTrans.Add (&pRay->pStart, &pRay->pDir);
   pDTrans.p[3] = 1;
   pDTrans.MultiplyLeft (&po->mMatrixInv);
   pDTrans.Subtract (&pSTrans);
   fLen = pDTrans.Length();
   if (fabs(fLen) < EPSILON)
      return FALSE;
   fLenInv = 1.0 / fLen;
   pDTrans.Scale (fLenInv);

   // substitude
   pRay->pStart.Copy (&pSTrans);
   pRay->pDir.Copy (&pDTrans);
   pRay->fAlpha *= fLen;
   pRay->mToWorld.MultiplyLeft (&po->mMatrix);


   // reverse
   fSub = m_pCloneObject->RayIntersect (dwThread, -1, pRay, TRUE);
      // note: no bounding volume for clones

   // restore
   pRay->pStart.Copy (&pOrigStart);
   pRay->pDir.Copy (&pOrigDir);
   pRay->mToWorld.Copy (&mOrigToWorld);

   if (fSub) {
      pRay->fAlpha *= fLenInv;  // undo the transform
      return TRUE;
   }
   else
      pRay->fAlpha = fOrigAlpha;

   return FALSE;
}

/***************************************************************************************
ClipLineAgainstGrid - Clips the line against the grid borders to make sure don't
waste time subdividing the line

inputs
   PCPoint     pStart - Start point. Modified in place
   PCPoint     pEnd - End point. modified in place
returns
   BOOL - TRUE if line still exists, FALSE if completely outside
*/
static BOOL ClipLineAgainstGrid (PCPoint pStart, PCPoint pEnd)
{
   DWORD i;
   CPoint ap[2];
   fp afAlpha[2], fAlpha;
   DWORD dwLow, dwHigh;
   ap[0].Copy (pStart);
   ap[1].Copy (pEnd);
   afAlpha[0] = 0;
   afAlpha[1] = 1;
   for (i = 0; i < 3; i++) {
      if (ap[0].p[i] + CLOSE < ap[1].p[i]) {
         dwLow = 0;
         dwHigh = 1;
      }
      else if (ap[1].p[i] + CLOSE < ap[0].p[i]) {
         dwLow = 1;
         dwHigh = 0;
      }
      else
         continue;   // not enough change to matter

      // see if cross 0
      if ((ap[dwLow].p[i] < 0) && (ap[dwHigh].p[i] > 0)) {
         fAlpha = (0.0 - ap[0].p[i]) / (ap[1].p[i] - ap[0].p[i]);
         if (dwLow == 0)
            afAlpha[0] = max(afAlpha[0], fAlpha);
         else
            afAlpha[1] = min(afAlpha[1], fAlpha);
      }

      // see if cross RTGRIDDIM
      if ((ap[dwLow].p[i] < RTGRIDDIM) && (ap[dwHigh].p[i] > RTGRIDDIM)) {
         fAlpha = (RTGRIDDIM - ap[0].p[i]) / (ap[1].p[i] - ap[0].p[i]);
         if (dwHigh == 0)
            afAlpha[0] = max(afAlpha[0], fAlpha);
         else
            afAlpha[1] = min(afAlpha[1], fAlpha);
      }
   }

   if (afAlpha[1] < afAlpha[0])
      return FALSE;

   for (i = 0; i < 3; i++) {
      pStart->p[i] = (1.0 - afAlpha[0]) * ap[0].p[i] + afAlpha[0] * ap[1].p[i];
      pEnd->p[i] = (1.0 - afAlpha[1]) * ap[0].p[i] + afAlpha[1] * ap[1].p[i];
   }

   return TRUE;
}

/***************************************************************************************
CRayObject::RayIntersect - Intersects the given ray with the object.
If there is an intersection, updates the alpha and intersection information in
pRay.

inputs
   DWORD          dwThread - That that this is in, 0..MAXRAYTHREAD-1
   DWORD          dwBound - Bounding volume to use
   PCRTRAYPATH    pRay - Ray to intersect against
   BOOL           fNoBoundSphere - If TRUE then the ray wont do its initial
                  bounding sphere test against the object
returns
   BOOL - TRUE if success and interescted something
*/
BOOL CRayObject::RayIntersect (DWORD dwThread, DWORD dwBound, PCRTRAYPATH pRay, BOOL fNoBoundSphere)
{
   // if m_dDontDelete flag is set then it's the rayobject for the main world, so
   // dont bother checking the global bounding volume since it will always return true
   if (!m_fDontDelete && !fNoBoundSphere) {
      if (!TestAgainstBoundSphere (pRay, &m_pBoundSphere))
         return FALSE;
   }

   // if there are enough objects for a grid then initialize it with the line
   // BUGFIX - if we have a bounding area and have less than 10 objects in the area
   // then dont bother with the grid
   DWORD i, dwNumObjects;
   dwNumObjects = m_dwNumTri + m_dwNumQuad + m_lCRTSUBOBJECT.Num() + m_lCRTCLONE.Num();
#ifdef USEGIRD // disable the grid because any use of it seems to slow things down
   CRTGRID Grid;
   BOOL fUseGrid = FALSE;
   if ((dwBound < CRTBOUND) && (m_alInBound[dwThread][dwBound].Num() < MINOBJECTFORGRIDNOCLONE))
      fUseGrid = FALSE;
   else if (dwNumObjects >= (DWORD) (m_fIsClone ? MINOBJECTFORGRID : MINOBJECTFORGRIDNOCLONE)) {
      memset (&Grid, 0, sizeof(Grid));
      fUseGrid = TRUE;

      // line across
      CPoint pStart, pEnd;
      pStart.Copy (&pRay->pStart);
      pEnd.Copy (&pRay->pDir);
      pEnd.Scale (pRay->fAlpha);
      pEnd.Add (&pStart);

      // convert points
      CPoint pDelta;
      fp fScale;
      pDelta.Subtract (&m_apBound[1], &m_apBound[0]);
      pStart.Subtract (&m_apBound[0]);
      pEnd.Subtract (&m_apBound[0]);
      for (i = 0; i < 3; i++) {
         fScale = (fp)RTGRIDDIM / pDelta.p[i];
         pStart.p[i] *= fScale;
         pEnd.p[i] *= fScale;
      }

      // Clip against bounding box
      if (!ClipLineAgainstGrid(&pStart, &pEnd))
         return FALSE;

      // intersect line
      GridDrawLine (&pStart, &pEnd, &Grid);
   }
#endif // USEGRID

   // see if should go through the list in a sorted order. Do this if dont have bounding
   // volume
   DWORD dwGridSortStart, dwGridSortInc;
   DWORD *padwGridSort;
   BOOL fUseGridSort;
   fUseGridSort = FALSE;
   if ((dwBound >= CRTBOUND) && m_memGridSorted.m_dwAllocated) {
      fUseGridSort = TRUE;

      // find direction that generally travelling
      DWORD dwDim = 0;
      if (fabs(pRay->pDir.p[1]) > fabs(pRay->pDir.p[dwDim]))
         dwDim = 1;
      if (fabs(pRay->pDir.p[2]) > fabs(pRay->pDir.p[dwDim]))
         dwDim = 2;
      padwGridSort = ((DWORD*) m_memGridSorted.p) + (dwNumObjects * dwDim);
      if (pRay->pDir.p[dwDim] < 0) {
         dwGridSortStart = dwNumObjects-1;
         dwGridSortInc = -1;
      }
      else {
         dwGridSortStart = 0;
         dwGridSortInc = 1;
      }
   }

   BOOL fFind = FALSE;
   DWORD dwSkipInter = -1;

   // BUGFIX - Was just one = and was using dwNum, not dwNumObjects
   if (dwNumObjects == m_lCRTSUBOBJECT.Num()) {
      DWORD dwNum = m_lCRTSUBOBJECT.Num();   // BUGFIX - Put dwNumIn here since removed above

      // if there's a previous intersection then try that first
      if ((dwBound < CRTBOUND) && (m_adwLastObject[dwThread][dwBound] != -1)) {
         dwSkipInter = m_adwLastObject[dwThread][dwBound];

         // since this was hit in the last ray, dont bother with intersection test
         if (RayIntersectSub (dwThread, dwBound, /*fUseGrid ? &Grid :*/ NULL, pRay, dwSkipInter, TRUE))
            fFind = TRUE;
      }

      if (fUseGridSort) {
         for (i = dwGridSortStart; i < dwNumObjects; i += dwGridSortInc) {
            DWORD *padw = padwGridSort + i;

            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == padw[0])
               continue;

            fFind |= RayIntersectSub (dwThread, dwBound, /*fUseGrid ? &Grid :*/ NULL, pRay, padw[0], FALSE);
         } // i
      }
      else if (dwBound < CRTBOUND) {
         PCRTINBOUND pib = (PCRTINBOUND) m_alInBound[dwThread][dwBound].Get(0);
         DWORD dwNum = m_alInBound[dwThread][dwBound].Num();
         for (i = 0; i < dwNum; i++, pib++) {
            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == pib->dwID)
               continue;

            // use the bounding spehere stored in pib since it should be smaller
            // and more accurate.
            if (!TestAgainstBoundSphere (pRay, &pib->pBoundSphere))
               continue;   // miss

            if (RayIntersectSub (dwThread, dwBound, /*fUseGrid ? &Grid :*/ NULL, pRay, pib->dwID, TRUE)) {
               fFind = TRUE;

               // remember the last object that intersected
               if (dwBound < CRTBOUND)
                  m_adwLastObject[dwThread][dwBound] = pib->dwID;
            }
         }
      }
      else for (i = 0; i < dwNum; i++) {  // loop through them all
         // if found an object and we're in "stop at any opaque" and found an
         // opaque obejct then exit
         if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
            break;

         // if already checked this then ignore
         if (dwSkipInter == i)
            continue;

         if (RayIntersectSub (dwThread, dwBound, /*fUseGrid ? &Grid :*/ NULL, pRay, i, FALSE)) {
            fFind = TRUE;

            // remember the last object that intersected
            if (dwBound < CRTBOUND)
               m_adwLastObject[dwThread][dwBound] = i;
         }
      } // i - all subobjects
   }

   // loop through clones
   // BUGFIX - Was just one = and using dwNum, not dwNumObjects
   if (dwNumObjects == m_lCRTCLONE.Num()) {
      DWORD dwNum = m_lCRTCLONE.Num(); // BUGFIX - Put dwNumObjects in here since removed above

      // if there's a previous intersection then try that first
      if ((dwBound < CRTBOUND) && (m_adwLastObject[dwThread][dwBound] != -1)) {
         dwSkipInter = m_adwLastObject[dwThread][dwBound];

         // since this was hit in the last ray, dont bother with intersection test
         if (RayIntersectClone (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, dwSkipInter, TRUE))
            fFind = TRUE;
      }

      if (fUseGridSort) {
         for (i = dwGridSortStart; i < dwNumObjects; i += dwGridSortInc) {
            DWORD *padw = padwGridSort + i;

            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == padw[0])
               continue;

            fFind |= RayIntersectClone (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, padw[0], FALSE);
         } // i
      }
      else if (dwBound < CRTBOUND) {
         PCRTINBOUND pib = (PCRTINBOUND) m_alInBound[dwThread][dwBound].Get(0);
         DWORD dwNum = m_alInBound[dwThread][dwBound].Num();
         for (i = 0; i < dwNum; i++, pib++) {
            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == pib->dwID)
               continue;

            // use the bounding spehere stored in pib since it should be smaller
            // and more accurate.
            if (!TestAgainstBoundSphere (pRay, &pib->pBoundSphere))
               continue;   // miss

            if (RayIntersectClone (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, pib->dwID, TRUE)) {
               fFind = TRUE;

               // remember the last object that intersected
               if (dwBound < CRTBOUND)
                  m_adwLastObject[dwThread][dwBound] = pib->dwID;
            }
         }
      }
      else for (i = 0; i < dwNum; i++) {  // loop through them all
         // if found an object and we're in "stop at any opaque" and found an
         // opaque obejct then exit
         if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
            break;

         // if already checked this then ignore
         if (dwSkipInter == i)
            continue;

         if (RayIntersectClone (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, i, FALSE)) {
            fFind = TRUE;

            // remember the last object that intersected
            if (dwBound < CRTBOUND)
               m_adwLastObject[dwThread][dwBound] = i;
         }
      } // i - all subobjects
   }

   // loop through all the polygons seeing if intersect
   if (m_dwNumTri || m_dwNumQuad) {
      // if there's a previous intersection then try that first
      if ((dwBound < CRTBOUND) && (m_adwLastObject[dwThread][dwBound] != -1)) {
         dwSkipInter = m_adwLastObject[dwThread][dwBound];

         if (RayIntersectPoly (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, dwSkipInter, TRUE))
            fFind = TRUE;
      };

      if (fUseGridSort) {
         for (i = dwGridSortStart; i < dwNumObjects; i += dwGridSortInc) {
            DWORD *padw = padwGridSort + i;

            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == padw[0])
               continue;

            fFind |= RayIntersectPoly (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, padw[0], FALSE);
         } // i
      }
      else if ((dwBound < CRTBOUND) && (m_dwNumTri || m_dwNumQuad)) {
         DWORD *padw = (DWORD*) m_alInBound[dwThread][dwBound].Get(0);
         DWORD dwNum = m_alInBound[dwThread][dwBound].Num();
         for (i = 0; i < dwNum; i++, padw++) {
            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == padw[0])
               continue;

            if (RayIntersectPoly (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, padw[0], FALSE)) {
               fFind = TRUE;

               // remember the last object that intersected
               if (dwBound < CRTBOUND)
                  m_adwLastObject[dwThread][dwBound] = padw[0];
            }
         }
      } // if have object cache
      else { // loop through them all
         for (i = 0; i < m_dwNumTri; i++) {
            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == (i | RTPF_TRIANGLE))
               continue;

            if (RayIntersectPoly (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, i | RTPF_TRIANGLE, FALSE)) {
               fFind = TRUE;

               // remember the last object that intersected
               if (dwBound < CRTBOUND)
                  m_adwLastObject[dwThread][dwBound] = i | RTPF_TRIANGLE;
            }
         }

         for (i = 0; i < m_dwNumQuad; i++) {
            // if found an object and we're in "stop at any opaque" and found an
            // opaque obejct then exit
            if (fFind && pRay->fStopAtAnyOpaque && pRay->fInterOpaque)
               break;

            // if already checked this then ignore
            if (dwSkipInter == (i | RTPF_QUAD1))
               continue;

            if (RayIntersectPoly (dwThread, /*fUseGrid ? &Grid :*/ NULL, pRay, i | RTPF_QUAD1, FALSE)) {
               fFind = TRUE;

               // remember the last object that intersected
               if (dwBound < CRTBOUND)
                  m_adwLastObject[dwThread][dwBound] = i | RTPF_QUAD1;
            }
         }
      }  // if any quad
   } // if quad or tri

   return fFind;
}


/***************************************************************************************
CRayObject::LightsAdd - Adds the lights in pLight to the object.

inputs
   PLIGHTINFO           pLight - Light information
   DWORD                dwNum - Number of lights
returns
   BOOL - TRUE if success
*/
BOOL CRayObject::LightsAdd (PLIGHTINFO pLight, DWORD dwNum)
{
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_lLIGHTINFO.Init (sizeof(LIGHTINFO), pLight, dwNum);
	MALLOCOPT_RESTORE;


   // make sure the direction is normalized
   DWORD i;
   PLIGHTINFO pli;
   pli = (PLIGHTINFO) m_lLIGHTINFO.Get(0);
   for (i = 0; i < m_lLIGHTINFO.Num(); i++, pli++) {
      pli->pDir.Normalize();
   }
   return TRUE;
}

/***************************************************************************************
CRayObject::LightsGet - Returns the lights emitted by this object.

inputs
   DWORD                *pdwNum - Filled with the number of lights
returns
   PLIGHTINFO - Pointer to lights, or NULL if no lights. DONT change the values in here.
*/
PLIGHTINFO CRayObject::LightsGet (DWORD *pdwNum)
{
   *pdwNum = m_lLIGHTINFO.Num();
   if (!m_lLIGHTINFO.Num())
      return NULL;

   return (PLIGHTINFO) m_lLIGHTINFO.Get(0);
}


/***************************************************************************************
CRayObject::SurfaceInfoGet - Re-intersects a ray with the polygon in the object. This
is then used to get the color, glow, etc. for the object.

inputs
   DWORD                dwThread - Thread, 0..MAXRAYTHREAD-1
   PCRTRAYPATH          pRay - Use the "inter" information to determine what polygon intersected and how
   PCRTSURFACE          pSurf - Filled in with the surface information
   BOOL                 fOnlyWantRefract - If TRUE used to send ray to light, and only care
                        if end up with opaque surface or not, and how much light gets through.
returns
   BOOL - TRUE if success
*/
BOOL CRayObject::SurfaceInfoGet (DWORD dwThread, PCRTRAYPATH pRay, PCRTSURFACE pSurf, BOOL fOnlyWantRefract)
{
   // clear the surface info
   memset (pSurf, 0, sizeof(*pSurf));
   pSurf->dwIDMajor = m_dwID;
   pSurf->pInterRayObject = pRay->pInterRayObject;
   pSurf->dwInterPolygon = pRay->dwInterPolygon;
   pSurf->fBeamWidth = pRay->fBeamOrig + pRay->fAlpha * pRay->fBeamSpread;
   fp fBeamWidth = max(pSurf->fBeamWidth, EPSILON);


   // find out where the ray intersects...
   DWORD dwVert, adwVert[4];
   DWORD dwNum, i;
   DWORD dwSurface;
   dwNum = pRay->dwInterPolygon & RTPF_NUMMASK;
   switch (pRay->dwInterPolygon & RTPF_FLAGMASK) {
   case RTPF_TRIANGLE:
      dwVert = 3;
      if (m_dwPolyDWORD) {
         PCRTTRIDWORD pt = (PCRTTRIDWORD) m_pTri + dwNum;
         for (i = 0; i < dwVert; i++)
            adwVert[i] = pt->adwVertex[i];
         pSurf->dwIDPart = pt->dwIDPart;
         dwSurface = pt->dwSurface;
      }
      else {
         PCRTTRIWORD pt = (PCRTTRIWORD) m_pTri + dwNum;
         for (i = 0; i < dwVert; i++)
            adwVert[i] = pt->adwVertex[i];
         pSurf->dwIDPart = pt->dwIDPart;
         dwSurface = pt->dwSurface;
      }
      break;

   case RTPF_QUAD1:
   case RTPF_QUAD2:
      dwVert = 4;
      if (m_dwPolyDWORD) {
         PCRTQUADDWORD pt = (PCRTQUADDWORD) m_pQuad + dwNum;
         for (i = 0; i < dwVert; i++)
            adwVert[i] = pt->adwVertex[i];
         pSurf->dwIDPart = pt->dwIDPart;
         dwSurface = pt->dwSurface;
      }
      else {
         PCRTQUADWORD pt = (PCRTQUADWORD) m_pQuad + dwNum;
         for (i = 0; i < dwVert; i++)
            adwVert[i] = pt->adwVertex[i];
         pSurf->dwIDPart = pt->dwIDPart;
         dwSurface = pt->dwSurface;
      }
      break;

   default:
      return FALSE;
   }

   // get points
   PCRTVERTEXDWORD pDWVert;
   pDWVert = (PCRTVERTEXDWORD) m_pVertices;
   DWORD adwPoint[4], adwNorm[4], adwColor[4], adwText[4];
   if (m_dwPolyDWORD) {
      for (i = 0; i < dwVert; i++) {
         adwPoint[i] = pDWVert[adwVert[i]].dwPoint;
         adwNorm[i] = pDWVert[adwVert[i]].dwNormal;
         adwColor[i] = pDWVert[adwVert[i]].dwColor;
         adwText[i] = pDWVert[adwVert[i]].dwTexture;
      }
   }
   else {
      for (i = 0; i < dwVert; i++) {
         adwPoint[i] = m_pVertices[adwVert[i]].dwPoint;
         adwNorm[i] = m_pVertices[adwVert[i]].dwNormal;
         adwColor[i] = m_pVertices[adwVert[i]].dwColor;
         adwText[i] = m_pVertices[adwVert[i]].dwTexture;
      }
   }

   // get the points
   CPoint apPoint[4], apNorm[4];
   COLORREF acColor[4];
   TEXTPOINT5 aText[4];
   DWORD dw, j;
   short as[3];
   memset (apPoint, 0, sizeof(apPoint));
   memset (apNorm, 0, sizeof(apNorm));
   memset (acColor, 0, sizeof(acColor));
   memset (aText, 0, sizeof(aText));
   for (i = 0; i < dwVert; i++) {
      for (j = 0; j < 3; j++)
         apPoint[i].p[j] = m_pPoints[adwPoint[i]].p[j];

      if (adwNorm[i] < m_dwNumNormals) {
         dw = m_pNormals[adwNorm[i]].dw;
         as[0] = (short)(WORD)((dw & 0x3ff) << 6);
         as[1] = (short)(WORD)(((dw >> 10) & 0x3ff) << 6);
         as[2] = (short)(WORD)(((dw >> 20) & 0x3ff) << 6);
         apNorm[i].p[0] = (fp)as[0] / 32767.0;
         apNorm[i].p[1] = (fp)as[1] / 32767.0;
         apNorm[i].p[2] = (fp)as[2] / 32767.0;
      }

      if (adwColor[i] < m_dwNumColors)
         acColor[i] = m_pColors[adwColor[i]];

      if (adwText[i] < m_dwNumTextures)
         aText[i] = m_pTextures[adwText[i]];
   }

   // intersect
   BOOL fRet;
   CPoint pIntersect;
   TEXTUREPOINT tpIntersect;
   DWORD dwQuadFlags;
   BOOL fInterA;
   dwQuadFlags = 0;
   // already intersected with the quad before, so make sure that dont test
   // with intersect against the same aprt of the quad
   if ((pSurf->dwInterPolygon & RTPF_FLAGMASK) == RTPF_QUAD1)
      dwQuadFlags = 0x04;  // since intersected with quad 1, dont bother checking against quad2
   else if ((pSurf->dwInterPolygon & RTPF_FLAGMASK) == RTPF_QUAD2)
      dwQuadFlags =  0x02; // since intersected with quad 2, dont bother checking against quad1

   if (dwVert == 3)
      fRet = IntersectLineTriangle (&pRay->pInterStart, &pRay->pInterDir,
                  &apPoint[0], &apPoint[1], &apPoint[2], &pIntersect, NULL, &tpIntersect);
   else
      fRet = IntersectLineQuad (&pRay->pInterStart, &pRay->pInterDir,
                  &apPoint[0], &apPoint[1], &apPoint[2], &apPoint[3],
                  &pIntersect, NULL, &tpIntersect, &fInterA, dwQuadFlags | 0x01);
   if (!fRet)
      return FALSE;  // shouldnt happen

   // figure out location in space
   pIntersect.p[3] = 1;
   pIntersect.MultiplyLeft (&pRay->mInterToWorld);
   pSurf->pLoc.Copy (&pIntersect);

   // figure out SOLID color
   // NOTE: Dont bother interpolating. Just take one
   Gamma (acColor[0], pSurf->awColor);

   // figure out SOLID surface info
   memcpy (&pSurf->Material, &m_pSurfaces[dwSurface].Material, sizeof(pSurf->Material));

   // interpolate the normals
   if (dwVert == 3) {
      apNorm[1].Scale (1.0 - tpIntersect.h - tpIntersect.v);
      apNorm[0].Scale (tpIntersect.h);
      apNorm[2].Scale (tpIntersect.v);
      pSurf->pNorm.Add (&apNorm[0], &apNorm[1]);
      pSurf->pNorm.Add (&apNorm[2]);
   }
   else { // quad
      CPoint pTop, pBottom;
      pTop.Average (&apNorm[1], &apNorm[0], tpIntersect.h);
      pBottom.Average (&apNorm[2], &apNorm[3], tpIntersect.h);
      pSurf->pNorm.Average (&pBottom, &pTop, tpIntersect.v);
   }
   CMatrix mInv;
   pRay->mInterToWorld.Invert (&mInv);
   mInv.Transpose();
   pSurf->pNorm.p[3] = 1;
   pSurf->pNorm.MultiplyLeft (&mInv);
   pSurf->pNorm.Normalize();

   // copy the normal as the texture norm
   pSurf->pNormText.Copy (&pSurf->pNorm);

   // misc
   pSurf->dwIDMinor = m_pSurfaces[dwSurface].wMinorID;

   if (m_pSurfaces[dwSurface].pTexture) {
      PCTextureMapSocket pt = m_pSurfaces[dwSurface].pTexture;
      TEXTPOINT5 tp;

      // interpolate texture point
      if (dwVert == 3) {
         for (j = 0; j < 2; j++) {
            aText[1].hv[j] *= 1.0 - tpIntersect.h - tpIntersect.v;
            aText[0].hv[j] *= tpIntersect.h;
            aText[2].hv[j] *= tpIntersect.v;
         }
         for (j = 0; j < 3; j++) {
            aText[1].xyz[j] *= 1.0 - tpIntersect.h - tpIntersect.v;
            aText[0].xyz[j] *= tpIntersect.h;
            aText[2].xyz[j] *= tpIntersect.v;
         }

         for (j = 0; j < 2; j++)
            tp.hv[j] = aText[0].hv[j] + aText[1].hv[j] + aText[2].hv[j];
         for (j = 0; j < 3; j++)
            tp.xyz[j] = aText[0].xyz[j] + aText[1].xyz[j] + aText[2].xyz[j];
      }
      else { // quad
         TEXTPOINT5 tpTop, tpBottom;

         for (j = 0; j < 2; j++) {
            tpTop.hv[j] = aText[1].hv[j] * tpIntersect.h + aText[0].hv[j] * (1.0 - tpIntersect.h);
            tpBottom.hv[j] = aText[2].hv[j] * tpIntersect.h + aText[3].hv[j] * (1.0 - tpIntersect.h);
         }
         for (j = 0; j < 3; j++) {
            tpTop.xyz[j] = aText[1].xyz[j] * tpIntersect.h + aText[0].xyz[j] * (1.0 - tpIntersect.h);
            tpBottom.xyz[j] = aText[2].xyz[j] * tpIntersect.h + aText[3].xyz[j] * (1.0 - tpIntersect.h);
         }

         for (j = 0; j < 2; j++)
            tp.hv[j] = tpBottom.hv[j] * tpIntersect.v + tpTop.hv[j] * (1.0 - tpIntersect.v);
         for (j = 0; j < 3; j++)
            tp.xyz[j] = tpBottom.xyz[j] * tpIntersect.v + tpTop.xyz[j] * (1.0 - tpIntersect.v);
      }

      // calculate the size of the beam
      // NOTE: Assuming the beam hits straight on
      CPoint pH, pV, pT1, pT2, ptpH, ptpV, ptpHxyz, ptpVxyz;
      fp fH, fV;
      // figure out vectors in H and V (in object coords)
      ptpH.Zero();
      ptpV.Zero();
      ptpHxyz.Zero();
      ptpVxyz.Zero();
      if (dwVert == 3) {
         pH.Subtract (&apPoint[0], &apPoint[1]);
         pV.Subtract (&apPoint[2], &apPoint[1]);

         // also see texture delta
         for (j = 0; j < 2; j++) {
            ptpH.p[j] = aText[0].hv[j] - aText[1].hv[j];
            ptpV.p[j] = aText[2].hv[j] - aText[1].hv[j];
         }
         for (j = 0; j < 3; j++) {
            ptpHxyz.p[j] = aText[0].xyz[j] - aText[1].xyz[j];
            ptpVxyz.p[j] = aText[2].xyz[j] - aText[1].xyz[j];
         }
      }
      else {
         pT1.Subtract (&apPoint[1], &apPoint[0]);
         pT2.Subtract (&apPoint[2], &apPoint[3]);
         pH.Average (&pT2, &pT1, tpIntersect.v);

         pT1.Subtract (&apPoint[3], &apPoint[0]);
         pT2.Subtract (&apPoint[2], &apPoint[1]);
         pV.Average (&pT2, &pT1, tpIntersect.h);

         // also see texture delta
         for (j = 0; j < 2; j++) {
            pT1.p[j] = aText[1].hv[j] - aText[0].hv[j]; 
            pT2.p[j] = aText[2].hv[j] - aText[3].hv[j];
         }
         pT1.p[2] = pT2.p[2] = 0;
         ptpH.Average (&pT2, &pT1, tpIntersect.v);
         for (j = 0; j < 3; j++) {
            pT1.p[j] = aText[1].xyz[j] - aText[0].xyz[j]; 
            pT2.p[j] = aText[2].xyz[j] - aText[3].xyz[j];
         }
         ptpHxyz.Average (&pT2, &pT1, tpIntersect.v);

         for (j = 0; j < 2; j++) {
            pT1.p[j] = aText[3].hv[j] - aText[0].hv[j];
            pT2.p[j] = aText[2].hv[j] - aText[1].hv[j];
         }
         pT1.p[2] = pT2.p[2] = 0;
         ptpV.Average (&pT2, &pT1, tpIntersect.h);
         for (j = 0; j < 3; j++) {
            pT1.p[j] = aText[3].xyz[j] - aText[0].xyz[j];
            pT2.p[j] = aText[2].xyz[j] - aText[1].xyz[j];
         }
         ptpVxyz.Average (&pT2, &pT1, tpIntersect.h);
      }
      fH = pH.Length();
      fV = pV.Length();
      fH = max(fH, EPSILON);
      fV = max(fV, EPSILON);
      pH.Scale (1.0 / fH);
      pV.Scale (1.0 / fV);

      // create A vector (=h), B vector (perp to H, close to V), and C (perp to A and B)
      CPoint pA, pB, pC;
      pA.Copy (&pH);
      pC.CrossProd (&pH, &pV);
      pB.CrossProd (&pC, &pA);
      pB.Normalize();

      // figure out params for pV = k1*pA + k2*pB
      fp k1, k2;
      k1 = pV.DotProd (&pA);  // can do this since pA and pB are normalized
      k2 = pV.DotProd (&pB);
      if (fabs(k2) < EPSILON) {
         if (k2 >= 0)
            k2 = EPSILON;
         else k2 = -EPSILON;
      }

      // scale beam with according to 1.0 / N.Eye (eye = pRay->pDir)
      fp fScale;
      fScale = fabs(pSurf->pNorm.DotProd (&pRay->pDir));
      fScale = max(fScale, .001);   // so not infinitely large
      fBeamWidth /= fScale;
      // BUGFIX - Remove this so blurring is more appropirate.
      // when had this in got aliasing... fBeamWidth /= 2.0;   // BUGFIX - So ray tracing doesn't blur textures so much

      // texture H and V covered for pA x fRadius, pB x fRadius
      CPoint ptpA, ptpB, ptpAxyz, ptpBxyz;
      ptpA.Copy (&ptpH);
      ptpA.Scale (fBeamWidth / fH);
      ptpAxyz.Copy (&ptpHxyz);
      ptpAxyz.Scale (fBeamWidth / fH);
      // pB = pV/k2 - pA*k1/k2
      ptpB.Copy (&ptpA);
      ptpB.Scale (-k1 / k2);
      ptpV.Scale (fBeamWidth / fV / k2);
      ptpB.Add (&ptpV);
      ptpBxyz.Copy (&ptpAxyz);
      ptpBxyz.Scale (-k1 / k2);
      ptpVxyz.Scale (fBeamWidth / fV / k2);
      ptpBxyz.Add (&ptpVxyz);

      // need to calculate out the size of the beam
      TEXTPOINT5 tpBeam;
      for (j = 0; j < 2; j++)
         tpBeam.hv[j] = max(fabs(ptpB.p[j]), fabs(ptpA.p[j])) * 2.0;
      for (j = 0; j < 3; j++)
         tpBeam.xyz[j] = max(fabs(ptpBxyz.p[j]), fabs(ptpAxyz.p[j])) * 2.0;

      pt->FillPixel (dwThread, TMFP_ALL, pSurf->awColor, &tp, &tpBeam, &pSurf->Material, pSurf->afGlow, FALSE);
         // NOTE: Passing fHighQuality = FALSE in. BUGBUG - may eventually want to pass in TRUE, but slower

      // bump map
      TEXTPOINT5 tpRight, tpBelow;
      TEXTUREPOINT tpSlope;
      for (j = 0; j < 2; j++) {
         tpRight.hv[j] = ptpA.p[j];
         tpBelow.hv[j] = ptpB.p[j];
      }
      for (j = 0; j < 3; j++) {
         tpRight.xyz[j] = ptpAxyz.p[j];
         tpBelow.xyz[j] = ptpBxyz.p[j];
      }
      if (!fOnlyWantRefract && pt->PixelBump (dwThread, &tp, &tpRight, &tpBelow, &tpSlope)) {
            // NOTE: Passing fHighQuality = FALSE in. BUGBUG - may eventually want to pass in TRUE, but slower
         // create vectors in average normal, not absolute normal
         CPoint pNA, pNB;
         pNB.CrossProd (&pSurf->pNormText, &pA);
         pNB.Normalize();
         pNA.CrossProd (&pNB, &pSurf->pNormText);
         pNA.Normalize();

         // scale
         pNA.Scale (fBeamWidth);
         pNB.Scale (fBeamWidth);

         // add the bump
         pT1.Copy (&pSurf->pNormText);
         pT1.Scale (tpSlope.h / 2.0);
         pNA.Add (&pT1);
         pT1.Copy (&pSurf->pNormText);
         pT1.Scale (-tpSlope.v / 2.0); // BUGFIX - Use minus since is bottom-top
         pNB.Add (&pT1);

         // new normal
         pSurf->pNormText.CrossProd (&pNA, &pNB);
         pSurf->pNormText.Normalize();
      }
   } // if texture

   // determine if reflection
   if (!fOnlyWantRefract && pSurf->Material.m_wReflectAmount) {
      pSurf->fReflect = TRUE;

      CPoint pN;
      pN.Copy (&pSurf->pNormText);
      if (pN.DotProd (&pRay->pDir) > 0)
         pN.Scale (-1); // mirror on the opposite side;

      // R = I - 2(N.I)N
      pSurf->pReflectDir.Copy (&pN);
      pSurf->pReflectDir.Scale (-pN.DotProd (&pRay->pDir) * 2.0);
      pSurf->pReflectDir.Add (&pRay->pDir);
      pSurf->pReflectDir.Normalize (); // just in case

      // amount based on angle of incidence
      fp fAngle, f;
      fAngle = 1.0 - fabs(pN.DotProd (&pRay->pDir));
      f = ((fp)pSurf->Material.m_wReflectAngle / (fp)0xffff);
      fAngle = f * fAngle + (1.0-f) * 1.0;   // if glass reflects most when dotprod is 0

      // Need to normalize spec-plastic surface color
      WORD wMaxPlastic;
      fp fPlastic;
      wMaxPlastic = max(max(pSurf->awColor[0], pSurf->awColor[1]), pSurf->awColor[2]);
      if (wMaxPlastic)
         fPlastic = (fp)0xffff / (fp)wMaxPlastic;   // so specularity always max brightness
      else
         fPlastic = 1;

      // color based on surface color and plasticness and angle
      f = (fp)pSurf->Material.m_wSpecPlastic / (fp)0xffff;  // = 1 if plastic
      for (i = 0; i < 3; i++) {
         pSurf->afReflectColor[i] = (1.0 - f) * (fp) pSurf->awColor[i] * fPlastic / (fp)0xffff + f * 1.0;
         pSurf->afReflectColor[i] *= fAngle;

         // BUGFIX - Hadn't included reflect amount in calculations
         pSurf->afReflectColor[i] *= (fp)pSurf->Material.m_wReflectAmount / (fp)0xffff;
      }
   }
   else
      pSurf->fReflect = FALSE;

   if (pSurf->Material.m_wTransparency) {
      // determine if refraction, dir, and color
      pSurf->fRefract = TRUE;

      // calculate info
      BOOL fFlip;
      CPoint pN;
      pN.Copy (&pSurf->pNormText);
      fp fCi = -pRay->pDir.DotProd (&pN);
      if (fCi < 0) {
         // coming from the other side so flip
         fFlip = TRUE;
         fCi *= -1;
         pN.Scale (-1);
      }
      else
         fFlip = FALSE;
      
      // angle of incidence
      fp fNit;
      fNit = (fp) pSurf->Material.m_wIndexOfRefract / 100.0;
      if (!fNit)
         fNit = .01;  // need something
      if (!fFlip)
         fNit = 1.0 / fNit; // so if glass (1.44), can never get total internal reflection

      // sqrt value
      fp fRoot;
      fRoot = 1 + fNit * fNit * (fCi * fCi - 1);
      if (fOnlyWantRefract)
         fRoot = 0;  // if only want refraction then just case about color
      if (fRoot < 0) {
         // total internal reflection
         pSurf->afRefractColor[0] = pSurf->afRefractColor[1] = pSurf->afRefractColor[2] = 1;

         // R = I - 2(N.I)N
         pSurf->pRefractDir.Copy (&pN);
         pSurf->pRefractDir.Scale (-pN.DotProd (&pRay->pDir) * 2.0);
         pSurf->pRefractDir.Add (&pRay->pDir);
         pSurf->pRefractDir.Normalize (); // just in case
      }
      else {
         // refraction
         fRoot = fNit * fCi - sqrt(fRoot);
         pSurf->pRefractDir.Copy (&pN);
         pSurf->pRefractDir.Scale (fRoot);
         pN.Copy (&pRay->pDir);
         pN.Scale (fNit);
         pSurf->pRefractDir.Add (&pN);
         pSurf->pRefractDir.Normalize();  // just in case

         // color is affected by transparency
         fp fTrans;
         fTrans = (fp)pSurf->Material.m_wTransparency / (fp)0xffff;

         if (fTrans) {
            // BUGFIX - Transparency depends on angle of viewer
            fp fNDotEye, fTransAngle;
            fNDotEye = fabs(pRay->pDir.DotProd (&pSurf->pNormText));
            fTransAngle = (fp) pSurf->Material.m_wTransAngle / (fp)0xffff;
            fTrans *= (fNDotEye + (1.0 - fNDotEye) * (1.0 - fTransAngle));
         }

         for (i = 0; i < 3; i++)
            pSurf->afRefractColor[i] = fTrans * (fTrans  + (1.0 - fTrans) * (1.0 - fRoot) * (fp)pSurf->awColor[i] / (fp)0xffff);
            // pSurf->afRefractColor[i] = fTrans * (1.0  + (1.0 - fTrans) * (1.0 - fRoot) * (fp)pSurf->awColor[i] / (fp)0xffff);
            //pSurf->afRefractColor[i] = fTrans + (1.0 - fTrans) * (1.0 - fRoot) * (fp)pSurf->awColor[i] / (fp)0xffff;
            // BUGFIX - Changed fTrans to 1.0 - fTrans since completely transparent was
            // actually making brighter
            // BUGFIX - Refraction still wasn't working properly so changed equation again
         // BUGFIX - Do this equation so if it's 100% transparent all of the color goes through,
         // while if 0 transparency then none. Need to do this for leaves
      }
   }
   else
      pSurf->fRefract = FALSE;

   return TRUE;
}


/***************************************************************************************
IntersectBoundingVolume - Returns TRUE if the object intersects the bounding volume

inputs
   PCPoint        pObject - Bounding volume for the object. [0..2]=xyz, [3]=radius
   PCPoint        papSphere - Pointer to an array of 2 points than describe where the points are
   PCPoint        papLine - Pointer to an arraay of 2 points that describe a line (and its mirror)
                     that needs to be in. If NULL then there isn't any line
returns
   BOOL - TRUE if it intersects the volume, FALSE if not
*/
static BOOL IntersectBoundingVolume (PCPoint pObject, PCPoint papSphere, PCPoint papLine)
{
   // see if it intersects any of the spheres
   CPoint pDist;
   fp fDist;
   DWORD i;
   for (i = 0; i < 2; i++) {
      pDist.Subtract (pObject, &papSphere[i]);
      fDist = pDist.Length();
      if (fDist < pObject->p[3] + papSphere[i].p[3])
         return TRUE;   // intersects spheres
   }

   if (!papLine)
      return FALSE;  // no line so done

   // convert from the world coords into coords that reflect sphere coords
   CPoint pCenter;
   pCenter.Zero();
   CPoint pNearest;
   pDist.Subtract (&papSphere[1], &papSphere[0]);
   pCenter.p[1] = DistancePointToLine (pObject, &papSphere[0], &pDist, &pNearest);

   // find out how far down the line nearest is
   pNearest.Subtract (&papSphere[0]);
   //pDist.Normalize();
   //pCenter.p[0] = pDist.DotProd (&pNearest);
   pCenter.p[0] = pNearest.Length();

   // BUGFIX - Was just doing the length. Need to take the sign of the length into
   // account also
   fp fSign;
   fSign = pNearest.DotProd (&pDist);
   if (fSign < 0)
      pCenter.p[0] *= -1;

   // BUGFIX - Was doing just the check: if ((pCenter.p[0] >= papLine[0].p[0]) && (pCenter.p[0] <= papLine[1].p[0])) {
   // this is erronious since get casesd where apaLine[0].p[0] > papLine[1].p[0]
   BOOL fInside;
   fInside = FALSE;
   if (papLine[0].p[0] <= papLine[1].p[0])
      fInside = (pCenter.p[0] >= papLine[0].p[0]) && (pCenter.p[0] <= papLine[1].p[0]);
   else
      fInside = (pCenter.p[0] >= papLine[1].p[0]) && (pCenter.p[0] <= papLine[0].p[0]);

   // if it's completely within the lines then ok
   if (fInside) {
      fp fx = (pCenter.p[0] - papLine[0].p[0]) / (papLine[1].p[0] - papLine[0].p[0]);
      fp fy = (1.0 - fx) * papLine[0].p[1] + fx * papLine[1].p[1];

      // BUGFIX - Was just testing if (fabs(pCenter.p[1]) <= fy), but this didn't
      // take into account the radius of the bounding sphere.
      // BUGFIX - Take this out (-pObject->p[3]) because just realized that
      // test works with intersectlinesphere below
      if (fabs(pCenter.p[1]) <= fy)
         return TRUE;
   }

   // see if it touches any of the lines
   CPoint pStart, pDir, apInter[2];
   DWORD dwInter;
   pStart.Copy (&papLine[0]);
   pDir.Subtract (&papLine[1], &papLine[0]);
   DWORD j;
   for (j = 0; j < 2; j++) {
      // if second time around flip so compare against other line
      if (j) {
         pStart.p[1] *= -1;
         pDir.p[1] *= -1;  // line below
      }

      dwInter = IntersectLineSphere (&pStart, &pDir, &pCenter, pObject->p[3], &apInter[0], &apInter[1]);
      if (!dwInter)
         continue;   // no intersection
      
      // BUGFIX - was just checking that intersected, but really need to see if any of the
      // intersections are within the given range
      if (papLine[0].p[0] <= papLine[1].p[0]) {
         for (i = 0; i < dwInter; i++)
            if ((apInter[i].p[0] >= papLine[0].p[0]) && (apInter[i].p[0] <= papLine[1].p[0]))
               return TRUE;
      }
      else {
         // compare reverse direction
         for (i = 0; i < dwInter; i++)
            if ((apInter[i].p[0] >= papLine[1].p[0]) && (apInter[i].p[0] <= papLine[0].p[0]))
               return TRUE;
      }
   }

   // else, doesnt intersect
   return FALSE;
}


/***************************************************************************************
CRayObject::BoundingVolumeInit - Initializes the bounding volume based for the object
so that object searches are faster.

inputs
   DWORD          dwThread - Thread number
   DWORD          dwBound - Bounding number, 0..3
   PCPoint        papStart - Pointer to an array of two points that define the bounding
                           box min and max (where rays start)
   PCPoint        papEnd - Pointer to an array of two points that define the bounding box
                           min and max where the rays end
   PCPoint        papNewBound - Pointer to an array of two points that receive the new
                           bounding box (min and max). This new bounding box only includes
                           the area of the objects that were kept, excluding the ones thrown out
returns
   BOOL - TRUE if success
*/
BOOL CRayObject::BoundingVolumeInit (DWORD dwThread, DWORD dwBound, PCPoint papStart, PCPoint papEnd,
                                     PCPoint papNewBound)
{
	MALLOCOPT_INIT;

   DWORD i,j,x,y,z;
   CPoint p;

   // clear out last object intersected
   m_adwLastObject[dwThread][dwBound] = -1;

   // convert these volumes to spheres
   CPoint apSphere[2];
   apSphere[0].Average (&papStart[0], &papStart[1]);
   p.Subtract (&papStart[0], &papStart[1]);
   apSphere[0].p[3] = p.Length() / 2.0;
   apSphere[1].Average (&papEnd[0], &papEnd[1]);
   p.Subtract (&papEnd[0], &papEnd[1]);
   apSphere[1].p[3] = p.Length() / 2.0;

   // find the distance between the two points and their radius
   fp k, ar[2], frDelta;
   p.Subtract (&apSphere[0], &apSphere[1]);
   k = p.Length();
   ar[0] = apSphere[0].p[3];
   ar[1] = apSphere[1].p[3];
   frDelta = ar[0] - ar[1];

   // initialize the intersection info
   BOOL fBetweenLines, fLinesParallel;
   CPoint apLine[2];    // 0 = line's left, 1 = line's right, in sphere space
   fBetweenLines = TRUE;
   fLinesParallel = FALSE;
   apLine[0].Zero();
   apLine[1].Zero();

   // if the sphere object's are within one another then no lines
   if (fabs(frDelta) >= k)
      fBetweenLines = FALSE;  // circles within one another

   fp fSinTheta, fTheta;
   // BUGFIX - Old code wrong fSinTheta = fabs(frDelta) / k;, causing some bounding volumes to be wrong
   fSinTheta = -frDelta / k;
   fTheta = asin(fSinTheta);
   if ((fabs(fTheta) < CLOSE) || (fabs(fSinTheta) < CLOSE))
      fLinesParallel = TRUE;

   if (fBetweenLines && !fLinesParallel) {
      // BUGFIX - Old code worked fine if fSinTheta positive, but not if negative
      if (fSinTheta >= 0) {
         fp d1 = ar[0] / fSinTheta;
         fp d2 = ar[1] / fSinTheta;
         fp a = sqrt (d1 * d1 - ar[0] * ar[0]);
         apLine[0].p[0] = cos(fTheta) * a;
         apLine[0].p[1] = fSinTheta * a;
         a = sqrt (d2 * d2 - ar[1] * ar[1]);
         apLine[1].p[0] = cos(fTheta) * a;
         apLine[1].p[1] = fSinTheta * a;

         // move back x so that center of first circle is at 0
         apLine[0].p[0] -= d1;
         apLine[1].p[0] -= d1;
      }
      else {
         fp d1 = -ar[0] / fSinTheta;
         fp d2 = -ar[1] / fSinTheta;
         fp a = sqrt (d1 * d1 - ar[0] * ar[0]);
         apLine[0].p[0] = -cos(fTheta) * a;
         apLine[0].p[1] = -fSinTheta * a;
         a = sqrt (d2 * d2 - ar[1] * ar[1]);
         apLine[1].p[0] = -cos(fTheta) * a;
         apLine[1].p[1] = -fSinTheta * a;

         // move back x so that center of first circle is at 0
         apLine[0].p[0] += d1;
         apLine[1].p[0] += d1;
      }
   }
   else if (fBetweenLines && fLinesParallel) {
      apLine[0].p[0] = 0;
      apLine[0].p[1] = ar[0];
      apLine[1].p[0] = k;
      apLine[1].p[1] = ar[1];
   }
   PCPoint papLine;
   papLine = (fBetweenLines ? apLine : NULL);


   // clear the list of objects...
   PCListFixed pList;
   // CListFixed lScratch;
   BOOL fPoly;
   fPoly = (m_dwNumTri || m_dwNumQuad);
   pList  = &m_alInBound[dwThread][dwBound];
   pList->Init (fPoly ? sizeof(DWORD) : sizeof(CRTINBOUND));
   pList->Clear();

   // keep track of new bounding volume
   BOOL fFound;
   fFound = FALSE;
   papNewBound[0].Zero();
   papNewBound[1].Zero();

   // loop thorugh all the objects added if they're in bounding volume

   if (fPoly) {
      // if doing polygons, ultimately store as DWORDs, but want to use as CRTINBOUND
      // for now
      CRTINBOUND ib;
      memset (&ib, 0, sizeof(ib));
      m_alBoundingVolumeInitScratch[dwThread].Init (sizeof(CRTINBOUND));
      pList = &m_alBoundingVolumeInitScratch[dwThread];

      if (m_dwPolyDWORD) {
         PCRTTRIDWORD pt = (PCRTTRIDWORD) m_pTri;
         PCRTQUADDWORD pq = (PCRTQUADDWORD) m_pQuad;
         PCRTVERTEXDWORD pv = (PCRTVERTEXDWORD) m_pVertices;

         for (i = 0; i < m_dwNumTri; i++, pt++) {
            if (IntersectBoundingVolume(&pt->pBoundSphere, apSphere, papLine)) {
               ib.dwID = i | RTPF_TRIANGLE;
               ib.pBoundSphere.Copy (&pt->pBoundSphere);
	            MALLOCOPT_OKTOMALLOC;
               pList->Add (&ib);
	            MALLOCOPT_RESTORE;

               // min/max points
               for (j = 0; j < 3; j++) {
                  if (fFound) {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = min(papNewBound[0].p[x],
                           m_pPoints[pv[pt->adwVertex[j]].dwPoint].p[x]);
                        papNewBound[1].p[x] = max(papNewBound[1].p[x],
                           m_pPoints[pv[pt->adwVertex[j]].dwPoint].p[x]);
                     } // x
                  }
                  else {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = papNewBound[1].p[x] =
                           m_pPoints[pv[pt->adwVertex[j]].dwPoint].p[x];
                     } // x
                     fFound = TRUE;
                  }
               } // j
            }
         }
         for (i = 0; i < m_dwNumQuad; i++, pq++) {
            if (IntersectBoundingVolume(&pq->pBoundSphere, apSphere, papLine)) {
               ib.dwID = i | RTPF_QUAD1;
               ib.pBoundSphere.Copy (&pq->pBoundSphere);
	            MALLOCOPT_OKTOMALLOC;
               pList->Add (&ib);
	            MALLOCOPT_RESTORE;

               // min/max points
               for (j = 0; j < 4; j++) {
                  if (fFound) {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = min(papNewBound[0].p[x],
                           m_pPoints[pv[pq->adwVertex[j]].dwPoint].p[x]);
                        papNewBound[1].p[x] = max(papNewBound[1].p[x],
                           m_pPoints[pv[pq->adwVertex[j]].dwPoint].p[x]);
                     } // x
                  }
                  else {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = papNewBound[1].p[x] =
                           m_pPoints[pv[pq->adwVertex[j]].dwPoint].p[x];
                     } // x
                     fFound = TRUE;
                  }
               } // j
            }
         }
      }
      else {
         PCRTTRIWORD pt = m_pTri;
         PCRTQUADWORD pq =  m_pQuad;
         PCRTVERTEXWORD pv = m_pVertices;

         for (i = 0; i < m_dwNumTri; i++, pt++) {
            if (IntersectBoundingVolume(&pt->pBoundSphere, apSphere, papLine)) {
               ib.dwID = i | RTPF_TRIANGLE;
               ib.pBoundSphere.Copy (&pt->pBoundSphere);
	            MALLOCOPT_OKTOMALLOC;
               pList->Add (&ib);
	            MALLOCOPT_RESTORE;

               // min/max points
               for (j = 0; j < 3; j++) {
                  if (fFound) {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = min(papNewBound[0].p[x],
                           m_pPoints[pv[pt->adwVertex[j]].dwPoint].p[x]);
                        papNewBound[1].p[x] = max(papNewBound[1].p[x],
                           m_pPoints[pv[pt->adwVertex[j]].dwPoint].p[x]);
                     } // x
                  }
                  else {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = papNewBound[1].p[x] =
                           m_pPoints[pv[pt->adwVertex[j]].dwPoint].p[x];
                     } // x
                     fFound = TRUE;
                  }
               } // j
            }
         }
         for (i = 0; i < m_dwNumQuad; i++, pq++) {
            if (IntersectBoundingVolume(&pq->pBoundSphere, apSphere, papLine)) {
               ib.dwID = i | RTPF_QUAD1;
               ib.pBoundSphere.Copy (&pq->pBoundSphere);
	            MALLOCOPT_OKTOMALLOC;
               pList->Add (&ib);
	            MALLOCOPT_RESTORE;

               // min/max points
               for (j = 0; j < 4; j++) {
                  if (fFound) {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = min(papNewBound[0].p[x],
                           m_pPoints[pv[pq->adwVertex[j]].dwPoint].p[x]);
                        papNewBound[1].p[x] = max(papNewBound[1].p[x],
                           m_pPoints[pv[pq->adwVertex[j]].dwPoint].p[x]);
                     } // x
                  }
                  else {
                     for (x = 0; x < 3; x++) {
                        papNewBound[0].p[x] = papNewBound[1].p[x] =
                           m_pPoints[pv[pq->adwVertex[j]].dwPoint].p[x];
                     } // x
                     fFound = TRUE;
                  }
               } // j
            }
         }
      } // word
   } // if polygons

   else if (m_lCRTSUBOBJECT.Num()) {
      PCRTSUBOBJECT po = (PCRTSUBOBJECT) m_lCRTSUBOBJECT.Get(0);
      CRTINBOUND ib;
      memset (&ib, 0, sizeof(ib));
      for (i = 0; i < m_lCRTSUBOBJECT.Num(); i++, po++) {
         if (IntersectBoundingVolume(&po->pBoundSphere, apSphere, papLine)) {
            ib.dwID = i;
            // dont need this: ib.pBoundSphere.Copy (&po->pBoundSphere);
	         MALLOCOPT_OKTOMALLOC;
            pList->Add (&ib);
	         MALLOCOPT_RESTORE;
         }
      } // i
   } // subobject

   else if (m_lCRTCLONE.Num()) {
      PCRTCLONE po = (PCRTCLONE) m_lCRTCLONE.Get(0);
      CRTINBOUND ib;
      memset (&ib, 0, sizeof(ib));
      for (i = 0; i < m_lCRTCLONE.Num(); i++, po++) {
         if (IntersectBoundingVolume(&po->pBoundSphere, apSphere, papLine)) {
            ib.dwID = i;
            ib.pBoundSphere.Copy (&po->pBoundSphere);
	         MALLOCOPT_OKTOMALLOC;
            pList->Add (&ib);
	         MALLOCOPT_RESTORE;

            // also update the bounding volume
            if (fFound) {
               papNewBound[0].Min (&po->apBound[0]);
               papNewBound[1].Max (&po->apBound[1]);
            }
            else {
               papNewBound[0].Copy (&po->apBound[0]);
               papNewBound[1].Copy (&po->apBound[1]);
               fFound = TRUE;
            }
         }
      } // i
   } // clones

   // pass it onto the children sub-objects
   if (m_lCRTSUBOBJECT.Num()) {
      // then pass only to those sub-objects which are in the beam
      PCRTINBOUND pib = (PCRTINBOUND) pList->Get(0);

      for (i = 0; i < pList->Num(); i++, pib++) {
         PCRTSUBOBJECT pso = (PCRTSUBOBJECT) m_lCRTSUBOBJECT.Get(pib->dwID);
         CPoint apBound[2][2];
         CPoint apNewBound[2];
         PCPoint pp;

         if (pso->fMatrixIdent) {
            // no change, so pass on
            pso->pObject->BoundingVolumeInit (dwThread, dwBound, papStart, papEnd, apNewBound);

            goto includebound;
         }

         // else, need to convert points
         for (j = 0; j < 2; j++) {
            pp = j ? papEnd : papStart;
            BOOL fFound = FALSE;

            for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
               p.p[0] = pp[x].p[0];
               p.p[1] = pp[y].p[1];
               p.p[2] = pp[z].p[2];
               p.p[3] = 1;
               p.MultiplyLeft (&pso[i].mMatrixInv);

               if (fFound) {
                  apBound[j][0].Min (&p);
                  apBound[j][1].Max (&p);
               }
               else {
                  apBound[j][0].Copy (&p);
                  apBound[j][1].Copy (&p);
                  fFound = TRUE;
               }
            } // xyz

         } // j

         // send it
         pso->pObject->BoundingVolumeInit (dwThread, dwBound, &apBound[0][0], &apBound[1][0], apNewBound);

         // convert the bound points to our space
         apBound[0][0].Copy (&apNewBound[0]);
         apBound[0][1].Copy (&apNewBound[1]);
         for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
            p.p[0] = apBound[0][x].p[0];
            p.p[1] = apBound[0][y].p[1];
            p.p[2] = apBound[0][z].p[2];
            p.p[3] = 1;
            p.MultiplyLeft (&pso[i].mMatrix);

            if (x || y || z) {
               apNewBound[0].Min (&p);
               apNewBound[1].Max (&p);
            }
            else {
               apNewBound[0].Copy (&p);
               apNewBound[1].Copy (&p);
            }
         } // xyz

includebound:
         // include the new bound in this object's bound...
         // note: only do this if get non-zero value. If get zero then nothing to intersect
         // so dont even bother including it in new bounding volume
         if (!apNewBound[0].AreClose (&apNewBound[1]) || (apNewBound[0].p[3] != apNewBound[1].p[3])) {
            if (fFound) {
               papNewBound[0].Min (&apNewBound[0]);
               papNewBound[1].Max (&apNewBound[1]);
            }
            else {
               papNewBound[0].Copy (&apNewBound[0]);
               papNewBound[1].Copy (&apNewBound[1]);
               fFound = TRUE;
            }
         }

         // srite this into the bounding sphere
         pib->pBoundSphere.Average (&apNewBound[0], &apNewBound[1]);
         apNewBound[1].Subtract (&apNewBound[0]);
         pib->pBoundSphere.p[3] = apNewBound[1].Length()/2.0;
      } // i
   } // if m_lCRTSUBOBJECT.Num()

   // NOTE: Dont need to pass through clones again because don't use bounding volumes
   // on clones

   // will need to sort objects by distance from the origin.
   PCRTINBOUND pib;
   pib = (PCRTINBOUND) pList->Get(0);
   for (i = 0; i < pList->Num(); i++, pib++) {
      p.Subtract (&pib->pBoundSphere, &apSphere[0]);
      pib->fDistance = p.Length() - pib->pBoundSphere.p[3];
      pib->fDistance = max(pib->fDistance, 0);
      // BUGFIX - Decrease the distance by the bounding sphere's radius, so if
      // object is generally closer checked first. Before a large object slightly
      // further away would be checked later

      // I know keeping the distance around is a bit wasteful but ultimately i think
      // it will be faster than shufflying memory around multiple times. And all this
      // info is discarded for polygons - which is most of it
   }
   pib = (PCRTINBOUND) pList->Get(0);
   qsort(pib, pList->Num(), sizeof (CRTINBOUND), CRTINBOUNDSort);


   // Will want to transfer polygon locations into just dwords here, after sort
   if (fPoly) {
      PCRTINBOUND pib = (PCRTINBOUND) pList->Get(0);
      DWORD dwNum = pList->Num();
      pList = &m_alInBound[dwThread][dwBound];
      MALLOCOPT_OKTOMALLOC;
      pList->Required (pList->Num() + dwNum);
      MALLOCOPT_RESTORE;
      for (i = 0; i < dwNum; i++, pib++) {
	      MALLOCOPT_OKTOMALLOC;
         pList->Add (&pib->dwID);
	      MALLOCOPT_RESTORE;
      }
   }

   return TRUE;
}

// FUTURERELESE - Subdivide ray tracing objects if they contain too many sub-objects (or
// clones, or polygons)

// BUGBUG - Clone render will get object ID from the clone setting... which means that
// clones within an object will have a different object ID than the object.
// This needs to be fixed

