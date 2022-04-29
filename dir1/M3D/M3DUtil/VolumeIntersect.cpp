/*******************************************************************************8
VolumeIntersect.cpp - Inersect space with a bounding box and returns the polygons
in that space.

begun 21/8/2002 by Mike Rozak
Copyright Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/******************************************************************************
CPolyInVolume - static class that's used as a callback for rendering
so that we can clip the object against the volume.
*/
class CPolyInVolume : public CRenderSocket  {
public:
   ESCNEWDELETE;

   CPolyInVolume (void);
   ~CPolyInVolume (void);
   void Reset (DWORD dwPlanes, PCRenderClip prc);               // call this to reset member variables

   // from CRenderSocket
   BOOL QueryWantNormals (void);    // returns TRUE if renderer wants normal vectors
   BOOL QueryWantTextures (void);    // returns TRUE if renderer wants texture vectors
   fp QueryDetail (void);       // returns detail resoltion (in meters) that renderer wants now
                                                // this may change from one object to the next.
   void MatrixSet (PCMatrix pm);    // sets the current operating matrix. Set NULL implies identity.
                                                // Note: THis matrix will be right-multiplied by any used by renderer for camera
   void PolyRender (PPOLYRENDERINFO pInfo);
                                                // draws/commits all the polygons. The data in pInfo, and what it points
                                                // to may be modified by this call.
   BOOL QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail);
   BOOL QueryCloneRender (void);
   BOOL CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix);

   DWORD    m_dwPolyAllClipped;     // number of times all polygons clipped
   DWORD    m_dwPolySomeClipped;    // number of times some of the polygons clipped
   DWORD    m_dwPolyNoneClipped;    // number of times none of the polygons clipped
   DWORD    m_dwAnyClipped;         // 0 => none clipped, 1=>some but not all clipped, 2=>all clipped
   fp   m_fDetail;              // desired detail
   CMatrix  m_Matrix;               // current operating matrix
   DWORD    m_dwPlanes;             // clipping planes to clip against
   PCRenderClip m_prc;              // render clip
   CListFixed m_lPoints;            // initialize to CPoint - keeps a record of all points
};
typedef CPolyInVolume *PCPolyInVolume;

CPolyInVolume::CPolyInVolume (void)
{
   m_fDetail = .05;   // 5 cm seems reasonable since will be for embedded objects
   m_lPoints.Init (sizeof(CPoint));
   Reset (0, 0);
}

CPolyInVolume::~CPolyInVolume (void)
{
   // do nothing
}

void CPolyInVolume::Reset (DWORD dwPlanes, PCRenderClip prc)
{
   m_dwAnyClipped = 2;
   m_dwPolyAllClipped = m_dwPolySomeClipped = m_dwPolyNoneClipped = 0;
   m_Matrix.Identity();
   m_dwPlanes = dwPlanes;
   m_prc = prc;
   //m_lPoints.Clear(); - dont do this
}


BOOL CPolyInVolume::QueryWantNormals (void)
{
   return FALSE;
}

BOOL CPolyInVolume::QueryWantTextures (void)
{
   return FALSE;
}


/******************************************************************************
CPolyInVolume::QueryCloneRender - From CRenderSocket
*/
BOOL CPolyInVolume::QueryCloneRender (void)
{
   return FALSE;
}


/******************************************************************************
CPolyInVolume::CloneRender - From CRenderSocket
*/
BOOL CPolyInVolume::CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix)
{
   return FALSE;
}

/******************************************************************************
CPolyInVolume::QuerySubDetail - From CRenderSocket. Basically end up ignoring
*/
BOOL CPolyInVolume::QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail)
{
   *pfDetail = QueryDetail();
   return TRUE;
}


fp CPolyInVolume::QueryDetail (void)
{
   return m_fDetail;
}

void CPolyInVolume::MatrixSet (PCMatrix pm)
{
   m_Matrix.Copy (pm);
}

void CPolyInVolume::PolyRender (PPOLYRENDERINFO pInfo)
{
   // if don't test against clipping planes then assume they all went through
   // don't do this because need to transform all the points
   //if (!m_dwPlanes) {
   //   m_dwPolyNoneClipped++;
   //   goto calc;
   //}

   // transform all the points
   DWORD i;
   CPoint   temp;
   PCPoint  p;
   for (i = 0, p = pInfo->paPoints; i < pInfo->dwNumPoints; i++, p++) {
      p->p[3] = 1.0;
      m_Matrix.Multiply (p, &temp);
      p->Copy (&temp);
   }

   // if want to clip then do so. Note that PPOLYRENDERINFO may be completely changed
   BOOL fClipped;
   DWORD dwNum;
   dwNum = pInfo->dwNumPolygons;
   fClipped = m_prc->ClipPolygons (m_dwPlanes, pInfo);

   // remember what was clipped
   if (!fClipped) {
      if (dwNum)
         m_dwPolyNoneClipped++;
   }
   else {
      if (pInfo->dwNumPolygons)
         m_dwPolySomeClipped++;
      else
         m_dwPolyAllClipped++;
   }

//calc:
   DWORD dwTotal;
   dwTotal = m_dwPolyAllClipped + m_dwPolySomeClipped + m_dwPolyNoneClipped;
   if (!dwTotal)
      m_dwAnyClipped  = 2; // default to all clipped;
   else if (m_dwPolyAllClipped == dwTotal)
      m_dwAnyClipped = 2;  // all clipped
   else if (m_dwPolyNoneClipped == dwTotal)
      m_dwAnyClipped = 0;  // none clipped
   else
      m_dwAnyClipped = 1;  // some clipped

   // if keeping track of the points that come out
   PPOLYDESCRIPT pd;
   PVERTEX pv;
   PCPoint pp;
   DWORD j;
   for (i = 0, pd = pInfo->paPolygons; i < pInfo->dwNumPolygons;
      i++, pd = (PPOLYDESCRIPT)((PBYTE)pd + (sizeof(POLYDESCRIPT) + sizeof(DWORD)*pd->wNumVertices))) {

      // go through all the verticies
      m_lPoints.Required (m_lPoints.Num() + pd->wNumVertices);
      for (j = 0; j < pd->wNumVertices; j++) {
         DWORD dwVertex;
         dwVertex = ((DWORD*)(pd + 1))[j];
         pv = &pInfo->paVertices[dwVertex];
         pp = &pInfo->paPoints[pv->dwPoint];

         m_lPoints.Add (pp);
      }
   } // over polygons
}

/*******************************************************************************
PointsInVolume - Returns what objects appear with a volume (defined by 8 vertices).

inputs
   PCWorldSocket     pWorld - World to look through
   CPoint      apVolume[2][2][2] - Volume [left=0,right=1][front=0,back=1][bottom=0,top=1].
                  Its assumed that each side of the volume is a plane (and not somehow bent)
                  In world space
   DWORD       dwDimZero - Dimension in points to zero out because don't care about... 0 for x, 1 for y, 2 for z, 3 for none

   DWORD       *padwIgnore - Pointer to a list of object indecies to ignore.
                  Usually used with fZValues to dont recheck own object.
   DWORD       dwIgnoreNum - Numer to ignore
                  Usually used with fZValues to dont recheck own object.
returns
   PCListFixed - Initialized to sizeof(CPoint) and filled with list of unique points
*/
PCListFixed PointsInVolume (PCWorldSocket pWorld, CPoint apVolume[2][2][2], DWORD dwDimZero,
                             DWORD *padwIgnore, DWORD dwIgnoreNum)
{
   CPolyInVolume sink;

   // create the clip from the 6 sides
   CRenderClip rc;
   DWORD i, dwIgnore;
   CPoint pNormal, p1, p2;
   PCPoint pA, pB, pC;
   for (i = 0; i < 6; i++) {
      // depending on the side
      switch (i) {
      case 0:  // left
         pB = &apVolume[0][0][1];
         pA = &apVolume[0][1][1];
         pC = &apVolume[0][0][0];
         break;
      case 1:  // right
         pB = &apVolume[1][0][1];
         pC = &apVolume[1][1][1];
         pA = &apVolume[1][0][0];
         break;
      case 2:  // front
         pB = &apVolume[1][0][1];
         pA = &apVolume[0][0][1];
         pC = &apVolume[1][0][0];
         break;
      case 3:  // back
         pB = &apVolume[1][1][1];
         pC = &apVolume[0][1][1];
         pA = &apVolume[1][1][0];
         break;
      case 4:  // bottom
         pB = &apVolume[1][0][0];
         pA = &apVolume[0][0][0];
         pC = &apVolume[1][1][0];
         break;
      case 5:  // top
         pB = &apVolume[1][0][1];
         pC = &apVolume[0][0][1];
         pA = &apVolume[1][1][1];
      }

      p1.Subtract (pA, pB);
      p2.Subtract (pC, pB);
      pNormal.CrossProd (&p1, &p2);
      pNormal.Normalize();
      pNormal.p[3] = 0; // set W=0
      p1.Copy (pB);
      p1.p[3] = 0;   // set W=0

      rc.AddPlane (&pNormal, &p1);
   }

   // loop through all the objects in the world
   CMatrix mBound;
   CPoint acCorner[2];
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      // is this to be ignored?
      for (dwIgnore = 0; dwIgnore < dwIgnoreNum; dwIgnore++)
         if (i == padwIgnore[dwIgnore])
            break;
      if (dwIgnore < dwIgnoreNum)
         continue;

      // get the bounding box
      if (!pWorld->BoundingBoxGet (i, &mBound, &acCorner[0], &acCorner[1]))
         continue;

      // turn this into a series of 8 points
      CPoint apBox[2][2][2];
      DWORD x,y,z;
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z= 0; z < 2; z++) {
         // set the point
         apBox[x][y][z].p[0] = acCorner[x].p[0];
         apBox[x][y][z].p[1] = acCorner[y].p[1];
         apBox[x][y][z].p[2] = acCorner[z].p[2];
         apBox[x][y][z].p[3] = 1;

         // rotate it by the bounding box matrix
         CPoint pRight;
         pRight.Copy (&apBox[x][y][z]);
         mBound.Multiply (&pRight, &apBox[x][y][z]);
      }

      // is it trivially clipped
      DWORD dwPlanes;
      if (rc.TrivialClip (8, &apBox[0][0][0], &dwPlanes))
         continue;   // trivially clipped based on the bounding box

      // only test if the bound box crosses any planes. If it doesn't then assume
      // it's in
      if (dwPlanes) {
         // Call into the object
         sink.Reset (dwPlanes, &rc);
         OBJECTRENDER or;
         memset (&or, 0, sizeof(or));
         or.pRS = &sink;
         or.dwReason = ORREASON_UNKNOWN;
         or.dwShow = -1;
         (pWorld->ObjectGet (i))->Render (&or, (DWORD)-1);

         // if entirely clipped then skip
         if (sink.m_dwAnyClipped == 2)
            continue;

         // if only partially clipped AND we it must be wholly in then skip
         //if ((sink.m_dwAnyClipped == 1) && fMustBeWhollyIn)
         //   continue;
      }

   }  // over all objects

   // loop through all the points and remove duplicates
   CPoint pMin, pMax, pErr;
   PCPoint pList, pLook;
   DWORD dwNum, j;
   pErr.Zero();
   pErr.p[0] = pErr.p[1] = pErr.p[2] = .001;
   for (i = sink.m_lPoints.Num()-1; i < sink.m_lPoints.Num(); i--) {
      // get info
      pList = (PCPoint) sink.m_lPoints.Get(0);
      dwNum = sink.m_lPoints.Num();

      // zero out
      if (dwDimZero < 3)
         pList[i].p[dwDimZero] = 0;

      // acceptable min and max
      pMin.Subtract (&pList[i], &pErr);
      pMax.Add (&pList[i], &pErr);

      // loop over all points previous
      for (j = 0, pLook = pList; j < i; j++, pLook++) {
         // compare dimensions
         if ((dwDimZero != 0) && ((pLook->p[0] < pMin.p[0]) || pLook->p[0] > pMax.p[0]))
            continue;
         if ((dwDimZero != 1) && ((pLook->p[1] < pMin.p[1]) || pLook->p[1] > pMax.p[1]))
            continue;
         if ((dwDimZero != 2) && ((pLook->p[2] < pMin.p[2]) || pLook->p[2] > pMax.p[2]))
            continue;

         // found it
         sink.m_lPoints.Remove (i);
         break;
      }

      // if get to here either have just removed the point we were looking at because
      // it's a duplicate, or searched through everything and didn't find anything
   }  // over all points

   // copy the list
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(CPoint), sink.m_lPoints.Get(0), sink.m_lPoints.Num());

   return pl;
}


/*******************************************************************************
IsEmptyToLeft - Given two points (A and B) in a set of points, this draws a line
from A to B. (Ifinite line). It returns TRUE if there aren't any points on the
left side of the line (standing at A and looking towards B). FALSE if there
are any points. Use this to outline a series of points.

inputs
   PCPoint        paPoints - Points
   DWORD          dwNum - Number of points
   DWORD          dwA - Index of A into paPoints
   DWORD          dwB - Index of B into paPoints
   DWORD          dwDim1, dwDim2 - Which two dimensions to look at
returns
   BOOL - TRUE if it's empty
*/
BOOL IsEmptyToLeft (PCPoint paPoints, DWORD dwNum, DWORD dwA, DWORD dwB,
                    DWORD dwDim1, DWORD dwDim2)
{
   CPoint pLeft, pRight;
   fp f;
   pLeft.Zero();
   pLeft.p[0] = paPoints[dwB].p[dwDim1] - paPoints[dwA].p[dwDim1];
   pLeft.p[1] = paPoints[dwB].p[dwDim2] - paPoints[dwA].p[dwDim2];
   f = pLeft.Length();
   if (f < EPSILON)
      return FALSE;
   pLeft.Scale (1.0 / f);

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      if ((i == dwA) || (i == dwB))
         continue;

      pRight.Zero();
      pRight.p[0] = paPoints[i].p[dwDim1] - paPoints[dwA].p[dwDim1];
      pRight.p[1] = paPoints[i].p[dwDim2] - paPoints[dwA].p[dwDim2];
      f = pRight.Length();
      if (f < EPSILON)
         continue;
      pRight.Scale (1.0 / f);


      f = pLeft.p[0] * pRight.p[1] - pLeft.p[1] * pRight.p[0];

      if (f > EPSILON)
         return FALSE;
   }
   return TRUE;


#if 0 //- doesn't work
   // find the slope
   fp fDeltaX, fDeltaY;
   fp m, mInv, b, bInv;
   m = mInv = b = bInv = 0;
   BOOL fUseInv;
   fDeltaX = paPoints[dwB].p[dwDim1] - paPoints[dwA].p[dwDim1];
   fDeltaY = paPoints[dwB].p[dwDim2] - paPoints[dwA].p[dwDim2];
   if ((fabs(fDeltaX) < EPSILON) && (fabs(fDeltaY) < EPSILON))
      return FALSE;  // cant really tell
   if (fabs(fDeltaX) > fabs(fDeltaY)) {
      m = fDeltaY / fDeltaX;
      b = paPoints[dwA].p[dwDim2] - m * paPoints[dwA].p[dwDim1];
   }
   else {
      fUseInv = TRUE;
      mInv = fDeltaX / fDeltaY;
      bInv = paPoints[dwA].p[dwDim1] - mInv * paPoints[dwA].p[dwDim2];
   }

   // loop through all the points and see
   DWORD i;
   fp fLeft, fRight;
   for (i = 0; i < dwNum; i++) {
      if ((i == dwA) || (i == dwB))
         continue;

      // see if any points to the left
      if (!fUseInv) {
         fLeft = paPoints[i].p[dwDim1] * m;
         fRight = paPoints[i].p[dwDim2] - b;
      }
      else {
         fLeft = paPoints[i].p[dwDim2] * mInv;
         fRight = paPoints[i].p[dwDim1] - bInv;
      }

      if (fLeft < fRight - EPSILON)
         return FALSE;

   }


   // if gets there nothing to the left, so return true
   return TRUE;
#endif // 0
}



/*******************************************************************************
OutlineFromPoints - Given a plane (either the X, Y, or Z plane), this intersect
it with the objects in the world. The resulting polygons are isolated into points
and then an outline for the intersection is created. This outline can be used
to create a cutout for the shape.

inputs
   PCWorldSocket     pWorld - World to look through
   DWORD             dwPlane - 0 for x=0, 1 for y=0, 2 for z=0
   BOOL              fFlip - If TRUE flip returned list from clockwise to counterclockwise
returns
   PCListFixed - Initialized to sizeof(CPoint) and filled with list of points that go
   clockwise around the outline. The caller must free this.
*/
PCListFixed OutlineFromPoints (PCWorldSocket pWorld, DWORD dwPlane, BOOL fFlip)
{
   // figure out the bounding box
   CPoint apVolume[2][2][2];
   DWORD x,y,z;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) for (z = 0; z < 2; z++) {
      apVolume[x][y][z].p[0] = x ? 1000 : -1000;
      apVolume[x][y][z].p[1] = y ? 1000 : -1000;
      apVolume[x][y][z].p[2] = z ? 1000 : -1000;
      apVolume[x][y][z].p[3] = 1;

      // make a thin wafer
      apVolume[x][y][z].p[dwPlane] = (apVolume[x][y][z].p[dwPlane] < 0) ? -.001 : .001;
   }

   // intersect and generate the points
   PCListFixed plPoints;
   plPoints = PointsInVolume (pWorld, apVolume, dwPlane, NULL, 0);
   if (!plPoints)
      return NULL;

   // find the min and max
   PCPoint p;
   DWORD dwNum;
   p = (PCPoint) plPoints->Get(0);
   dwNum = plPoints->Num();
   if (dwNum < 3) {
      // not enough points
      delete plPoints;
      return NULL;
   }
   CPoint pMin, pMax, pLeftmost;
   DWORD dwLeftMost;
   pMin.Copy (&p[0]);
   pMax.Copy (&p[0]);
   pLeftmost.Copy (&p[0]);
   dwLeftMost = 0;
   DWORD i;
   for (i = 1; i < dwNum; i++) {
      pMin.Min (&p[i]);
      pMax.Max (&p[i]);
      if (p[i].p[0] < pLeftmost.p[0]) {
         pLeftmost.Copy (&p[i]);
         dwLeftMost = i;
      }
   }

   PCListFixed pNew;
   CPoint pTemp;
   pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(CPoint));
   DWORD dwDim, dwDim2;
   dwDim = (dwPlane+1)%3;
   dwDim2 = (dwDim+1)%3;


   // keep track of which points already used in the outline since no
   // need to test them again
   CListFixed lUsed;
   lUsed.Init (sizeof(BOOL));
   BOOL f;
   lUsed.Required (dwNum);
   f = FALSE;
   for (i = 0; i < dwNum; i++)
      lUsed.Add (&f);
   BOOL *pf;
   pf = (BOOL*) lUsed.Get(0);

   // add the left-most to the list
   pf[dwLeftMost] = TRUE;
   pNew->Add (&pLeftmost);

   // loop through all the points and find out a match
   DWORD j;
   while (TRUE) {
      // try all points for a match
      for (j = 0; j < dwNum; j++) {
         if (pf[j])
            continue;   // already used so skip
         if (IsEmptyToLeft (p, dwNum, dwLeftMost, j, dwDim, dwDim2))
            break;
      }

      if (j >= dwNum)
         break;   // no points left

      // keep this
      pNew->Add (&p[j]);
      pf[j] = TRUE;
      dwLeftMost = j;
   }

#if 0 // test code
   // For now just create a box, so can test
   pTemp.Zero();
   pTemp.p[dwDim] = pMin.p[dwDim];
   pTemp.p[dwDim2] = pMax.p[dwDim2];
   pNew->Add (&pTemp);
   pTemp.p[dwDim] = pMax.p[dwDim];
   pNew->Add (&pTemp);
   pTemp.p[dwDim2] = pMin.p[dwDim2];
   pNew->Add (&pTemp);
   pTemp.p[dwDim] = pMin.p[dwDim];
   pNew->Add (&pTemp);
#endif // 0

   // if flip clockwise to counterclockwise then do so
   if (fFlip) {
      dwNum = pNew->Num();
      p = (PCPoint) pNew->Get(0);

      for (i = 0; i < dwNum/2; i++) {
         pTemp.Copy (&p[i]);
         p[i].Copy (&p[dwNum-i-1]);
         p[dwNum-i-1].Copy (&pTemp);
      }
   }

   // done
   delete plPoints;
   return pNew;
}
