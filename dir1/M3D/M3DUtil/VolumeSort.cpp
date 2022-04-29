/*******************************************************************************8
VolumeSort.cpp - Code to find out what objects are in what volumes

begun 25/10/2001 by Mike Rozak
Copyright Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/******************************************************************************
CObjectsInVolume - static class that's used as a callback for rendering
so that we can clip the object against the volume.
*/
class CObjectsInVolume : public CRenderSocket  {
public:
   ESCNEWDELETE;

   CObjectsInVolume (void);
   ~CObjectsInVolume (void);
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
   CListFixed m_lZ;                 // initialize to fp - keeps a record of Z values of polyongs passing through
   BOOL     m_fZKeepRecord;         // only keep a record of m_fZKeepRecord is true
};
typedef CObjectsInVolume *PCObjectsInVolume;

CObjectsInVolume::CObjectsInVolume (void)
{
   m_fDetail = .1;   // 10 cm seems reasonable
   m_lZ.Init (sizeof(fp));
   Reset (0, 0);
}

CObjectsInVolume::~CObjectsInVolume (void)
{
   // do nothing
}

void CObjectsInVolume::Reset (DWORD dwPlanes, PCRenderClip prc)
{
   m_dwAnyClipped = 2;
   m_dwPolyAllClipped = m_dwPolySomeClipped = m_dwPolyNoneClipped = 0;
   m_Matrix.Identity();
   m_dwPlanes = dwPlanes;
   m_prc = prc;
   m_lZ.Clear();
   m_fZKeepRecord = FALSE;
}


BOOL CObjectsInVolume::QueryWantNormals (void)
{
   return FALSE;
}

BOOL CObjectsInVolume::QueryWantTextures (void)
{
   return FALSE;
}


/******************************************************************************
CObjectsInVolume::QueryCloneRender - From CRenderSocket
*/
BOOL CObjectsInVolume::QueryCloneRender (void)
{
   return FALSE;
}


/******************************************************************************
CObjectsInVolume::CloneRender - From CRenderSocket
*/
BOOL CObjectsInVolume::CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix)
{
   return FALSE;
}

/******************************************************************************
CObjectsInVolume::QuerySubDetail - From CRenderSocket. Basically end up ignoring
*/
BOOL CObjectsInVolume::QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail)
{
   *pfDetail = QueryDetail();
   return TRUE;
}


fp CObjectsInVolume::QueryDetail (void)
{
   return m_fDetail;
}

void CObjectsInVolume::MatrixSet (PCMatrix pm)
{
   m_Matrix.Copy (pm);
}

void CObjectsInVolume::PolyRender (PPOLYRENDERINFO pInfo)
{
   // if don't test against clipping planes then assume they all went through
   if (!m_dwPlanes && !m_fZKeepRecord) {
      m_dwPolyNoneClipped++;
      goto calc;
   }

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

calc:
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

   // if keeping track of polygon Z vlues then do so
   if (m_fZKeepRecord) {
      PPOLYDESCRIPT pd;
      PVERTEX pv;
      PCPoint pp;
      DWORD j;
      m_lZ.Required (m_lZ.Num() + pInfo->dwNumPolygons);
      for (i = 0, pd = pInfo->paPolygons; i < pInfo->dwNumPolygons;
         i++, pd = (PPOLYDESCRIPT)((PBYTE)pd + (sizeof(POLYDESCRIPT) + sizeof(DWORD)*pd->wNumVertices))) {

         // go through all the verticies
         fp fAverage;
         fAverage = 0;
         for (j = 0; j < pd->wNumVertices; j++) {
            DWORD dwVertex;
            dwVertex = ((DWORD*)(pd + 1))[j];
            pv = &pInfo->paVertices[dwVertex];
            pp = &pInfo->paPoints[pv->dwPoint];

            fAverage += pp->p[2];
         }
         fAverage /= (fp)pd->wNumVertices;

         // add this
         m_lZ.Add (&fAverage);
      } // over polygons

   }
}

static int _cdecl OVCompare (const void *elem1, const void *elem2)
{
   fp *pdw1, *pdw2;
   pdw1 = (fp*) elem1;
   pdw2 = (fp*) elem2;

   if (*pdw1 > *pdw2)
      return 1;
   else if (*pdw1 < *pdw2)
      return -1;
   else
      return 0;
}


/*******************************************************************************
ObjectsInVolume - Returns what objects appear with a volume (defined by 8 vertices).

inputs
   PCWorldSocket     pWorld - World to look through
   CPoint      apVolume[2][2][2] - Volume [left=0,right=1][front=0,back=1][bottom=0,top=1].
                  Its assumed that each side of the volume is a plane (and not somehow bent)
                  In world space
   BOOL        fMustBeWhollyIn - If TRUE, objects are only accepted if they're wholly in
                  the volume. If FALSE then it's OK if they're partially in the voume
   BOOL        fZValues - If FALSE, this returns what objects apear within the volume.
                  If TRUE, returns a list of doubles for the average Z value of the polygons
                  passing throight apVolume (which should be a very narrow vertical ray). Can
                  use this to calculate elevations.
   DWORD       *padwIgnore - Pointer to a list of object indecies to ignore.
                  Usually used with fZValues to dont recheck own object.
   DWORD       dwIgnoreNum - Numer to ignore
                  Usually used with fZValues to dont recheck own object.
returns
   PCListFixed -
      If (fZValues == FALSE)
         List of objects indexes (sorted numerically) that are in the volume.
         The caller must delete this. List is array of DWORDs.
      If (fZValues == TRUE)
         List of Z-values for polygons passing through the volume. These are sorted
         by height.
*/
PCListFixed ObjectsInVolume (PCWorldSocket pWorld, CPoint apVolume[2][2][2], BOOL fMustBeWhollyIn,
                             BOOL fZValues, DWORD *padwIgnore, DWORD dwIgnoreNum)
{
   CObjectsInVolume sink;

   // create the list
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (fZValues ? sizeof(fp) : sizeof(DWORD));

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
         if (fZValues)
            sink.m_fZKeepRecord = TRUE;
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
         if ((sink.m_dwAnyClipped == 1) && fMustBeWhollyIn)
            continue;
      }

      // if it got here then add it
      if (fZValues) {
         DWORD k;
         for (k = 0; k < sink.m_lZ.Num(); k++) {
            fp *pf = (fp*) sink.m_lZ.Get(k);
            pl->Add (pf);
         }
      }
      else
         pl->Add (&i);
   }

   // if generated Z values then sort
   if (fZValues)
      qsort (pl->Get(0), pl->Num(), sizeof(fp), OVCompare);

   // done
   return pl;
}
