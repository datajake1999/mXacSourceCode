/*******************************************************************************
CSplineSurface.cpp - Code for calculating a spline.

begun 15-11-2001 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"




#undef MESH
#define  MESH(x,y) ((x) + (y) * m_dwMeshH)
#define  MESHT(x,y) ((x) + (y) * dwTextH)
#define  MESHN(x,y,z) (((x) + (y) * dwNormH) * 4 + (z))
#define  MESHC(x,y) ((x) + (y) * m_dwControlH)
#define  MESHG(x,y) ((x) + (y) * dwNormH)
// MGTICK - Used to keep track of cutout count on left edge of grid
typedef struct {
   fp            fV;         // V value of the tick
   int               iCount;     // count used at the tick
} MGTICK, *PMGTICK;

// MGQUAD - Keeps track of a quadrilateral formed from clipping
typedef struct {
   DWORD             dwPoints;   // either 3 or 4
   CPoint            apPosn[4];  // position of each point in world space
   CPoint            apNorm[4];  // normal of each point, normalized
   TEXTUREPOINT      atText[4];  // texture at each point
   TEXTUREPOINT      atHV[4];    // Location in entire surface, from 0..1, 0..1
} MGQUAD, *PMGQUAD;


/**********************************************************************************
CMeshGrid::Constructor and destructor
*/
CMeshGrid::CMeshGrid (void)
{
   m_listMGLINE.Init (sizeof(MGLINE));
   m_listMGTICK.Init (sizeof(MGTICK));
   m_listMGQUAD.Init (sizeof(MGQUAD));
   m_fTrivialClip = TRUE;
   m_fTrivialAccept = FALSE;
}

CMeshGrid::~CMeshGrid (void)
{
   // do nothing for now
}

/************************************************************************************
CMeshGrid::FindMinMaxH - Looks through the obejct with cutouts and
finds the minimum and maximum HV values that occurs. If this is not 0..1,0..1 then
the cutouts remove enough that some of the edge completely removed, so potentially
the spline could be shrunk without any problem.

inputs
   PTEXTUREPOINT     pMin, pMax - Filled with the minimum and maximum values
returns
   BOOL - TRUE if found min,max. If FALSE then can't find anything.
*/
BOOL CMeshGrid::FindMinMaxHV (PTEXTUREPOINT pMin, PTEXTUREPOINT pMax)
{
   if (m_fTrivialClip)
      return FALSE;

   if (m_fTrivialAccept) {
      pMin->h = m_afH[0];
      pMax->h = m_afH[1];
      pMin->v = m_afV[0];
      pMax->v = m_afV[1];
      return TRUE;
   }

   // find all the points with this slope
   PMGQUAD pq;
   BOOL fFound;
   fFound = FALSE;
   DWORD i, j;
   for (i = 0; i < m_listMGQUAD.Num(); i++) {
      pq = (PMGQUAD) m_listMGQUAD.Get(i);

      // loop through all the poins
      for (j = 0; j < pq->dwPoints; j++) {
         if (!fFound) {
            fFound = TRUE;
            *pMin = pq->atHV[j];
            *pMax = pq->atHV[j];
            continue;
         }

         // else, min, max
         pMin->h = min(pMin->h, pq->atHV[j].h);
         pMin->v = min(pMin->v, pq->atHV[j].v);
         pMax->h = max(pMax->h, pq->atHV[j].h);
         pMax->v = max(pMax->v, pq->atHV[j].v);
      }
   }

   return fFound;
}

/************************************************************************************
CMeshGrid::FindLongestSegment - Called when determining of should draw an border
ribbon between two splines. This function takes a line (in HV coordinates from 0..1
for the spline) from pStart to pEnd. It then searches through the CMeshGrid's
polygons to see if the line is on the edge of any of the polygons. The segment closest
to pStart is then used as the beginning. This is connected with other segments
until pEnd is reached, or no more can be found. The longest segment is then
put intp pFoundStart and pFound and and TRUE is return. If no matching segments can
be found the FALSE is returned.

inputs
   PTEXUREPOINT      pStart - Start searching for
   PTEXTUREPOINT     pEnd - Finished searching for
   PTEXTUREPOINT     pFoundStart - Filled with start that found
   PTEXGTURPOINT     pFoundEnd - Filled witht he end that found
returns
   BOOL - TRUE if found (and pFoundXXX are valid), or FALSE if not
*/
typedef struct {
   TEXTUREPOINT   pStart;
   TEXTUREPOINT   pEnd;
} FLS, *PFLS;

// #define CLOSE     .0001

BOOL CMeshGrid::FindLongestSegment (PTEXTUREPOINT pStart, PTEXTUREPOINT pEnd,
   PTEXTUREPOINT pFoundStart, PTEXTUREPOINT pFoundEnd)
{
	MALLOCOPT_INIT;
   // if marked as tivial reject then return false
   if (m_fTrivialClip)
      return FALSE;


   // keep a list of the line segments that found
   // CListFixed lFound;
   m_lFindLongestSegmentFound.Init (sizeof(FLS));

   // if this is marked as trivial accept then special exception
   MGQUAD qEntire;
   DWORD i, j, k;
   if (m_fTrivialAccept) {
      memset (&qEntire, 0, sizeof(qEntire));
      qEntire.dwPoints = 4;
      for (k = 0; k < 4; k++) {
         qEntire.atHV[k].v = m_afV[(k >= 2) ? 1 : 0];
         qEntire.atHV[k].h = m_afH[(k==1 || k==2) ? 1 : 0];
      }
   }

   // find the slope and whatnot of the input line
   fp fDeltaH, fDeltaV, m, mInv, b, bInv;
   BOOL fHorz, fVert;
   DWORD dwDir;   // 0 for mostly horizontal, 1 for mostly vertical
   fDeltaH = pEnd->h - pStart->h;
   fDeltaV = pEnd->v - pStart->v;
   fHorz = (fabs(fDeltaV) < EPSILON);
   fVert = (fabs(fDeltaH) < EPSILON);
   if (fHorz && fVert)
      return FALSE;
   dwDir = (fabs(fDeltaH) < fabs(fDeltaV));
   if (dwDir == 0) { // mosly horizontal
      m = fDeltaV / fDeltaH;
      b = pStart->v - m * pStart->h;
   }
   else {   // mostly vertical
      mInv = fDeltaH / fDeltaV;
      bInv = pStart->h - mInv * pStart->v;
   }

   BOOL fLessOrig, fLessAdd;
   fLessOrig = (dwDir == 0) ? (pStart->h < pEnd->h) : (pStart->v < pEnd->v);

   // find all the points with this slope
   PMGQUAD pq;
   for (i = 0; i < (m_fTrivialAccept ? 1 : m_listMGQUAD.Num()); i++) {
      pq = m_fTrivialAccept ? &qEntire : (PMGQUAD) m_listMGQUAD.Get(i);

      // BUGFIX - If this is just a sliver then ignore. Put in because sometimes there's
      // a small sliver just under the door which causes drawing problems
      TEXTUREPOINT tpMax, tpMin;
      for (j = 0; j < pq->dwPoints; j++) {
         if (j == 0) {
            tpMax = pq->atHV[j];
            tpMin = tpMax;
         }
         else {
            tpMax.h = max(tpMax.h,pq->atHV[j].h);
            tpMax.v = max(tpMax.v,pq->atHV[j].v);
            tpMin.h = min(tpMin.h,pq->atHV[j].h);
            tpMin.v = min(tpMin.v,pq->atHV[j].v);
         }
      }
      if ((tpMax.h - tpMin.h < CLOSE) || (tpMax.v - tpMin.v < CLOSE))
         continue;

      // loop through all the poins
      FLS fls;
      fp fVal;
      for (j = 0; j < pq->dwPoints; j++) {
         // see if the point is on the line
         // and see if the next line is close enough
         k = (j+1)% pq->dwPoints;
         if (dwDir == 0) { // mostly horizontal
            fVal = m * pq->atHV[j].h + b;
            if (fabs(fVal - pq->atHV[j].v) > CLOSE)
               continue;   // not close enough

            fVal = m * pq->atHV[k].h + b;
            if (fabs(fVal - pq->atHV[k].v) > CLOSE)
               continue;   // not close enough
         }
         else {   // mostly vertical
            fVal = mInv * pq->atHV[j].v + bInv;
            if (fabs(fVal - pq->atHV[j].h) > CLOSE)
               continue;   // not close enough

            fVal = mInv * pq->atHV[k].v + bInv;
            if (fabs(fVal - pq->atHV[k].h) > CLOSE)
               continue;   // not close enough
         }


         // else, line is ok, arrange line so that in same order as HV
         fLessAdd = (dwDir == 0) ? (pq->atHV[j].h < pq->atHV[k].h) : (pq->atHV[j].v < pq->atHV[k].v);
         if (fLessOrig != fLessAdd) {
            fls.pStart = pq->atHV[k];
            fls.pEnd = pq->atHV[j];
         }
         else {
            fls.pStart = pq->atHV[j];
            fls.pEnd = pq->atHV[k];
         }

         // add it
	      MALLOCOPT_OKTOMALLOC;
         m_lFindLongestSegmentFound.Add (&fls);
	      MALLOCOPT_RESTORE;
      }
   }

   // if there's nothing in the list then easy finish
   if (!m_lFindLongestSegmentFound.Num())
      return FALSE;

   // if start does not occur within a line segment then advance until there is one
   PFLS pf;
   BOOL fFound;
   fFound = FALSE;
   fp fLineMin, fLineMax;
   DWORD dwClosest;
   fp fClosest, fDist;
   BOOL fInside;
   fInside = FALSE;
   dwClosest = (DWORD)-1;
   if (dwDir == 0) { // mostly horzontal
      fLineMin = min(pStart->h, pEnd->h);
      fLineMax = max(pStart->h, pEnd->h);
   }
   else {   // mostly veritcal
      fLineMin = min(pStart->v, pEnd->v);
      fLineMax = max(pStart->v, pEnd->v);
   }
   for (i = 0; i < m_lFindLongestSegmentFound.Num(); i++) {
      fp fMin, fMax;
      pf = (PFLS) m_lFindLongestSegmentFound.Get(i);
      if (dwDir == 0) { // mostly horizontal
         fMin = min(pf->pEnd.h, pf->pStart.h);
         fMax = max(pf->pEnd.h, pf->pStart.h);
      }
      else {
         fMin = min(pf->pEnd.v, pf->pStart.v);
         fMax = max(pf->pEnd.v, pf->pStart.v);
      }

      // combination
      fp fCMin, fCMax;
      fCMin = max(fLineMin, fMin);
      fCMax = min(fLineMax, fMax);
      if (fCMin >= fCMax)
         continue;   // lines don't intersect

      // else they intersect
      fDist = 1000000;
      if (dwDir == 0) { // horizontal
         if ((pStart->h >= fMin) && (pStart->h <= fMax))
            fInside = TRUE;   // the start is on the sinside, so don't worry about closest
         else
            fDist = fabs(pf->pStart.h - pStart->h);
      }
      else {   // vertical
         if ((pStart->v >= fMin) && (pStart->v <= fMax))
            fInside = TRUE;   // the start is on the sinside, so don't worry about closest
         else
            fDist = fabs(pf->pStart.v - pStart->v);
      }
      if ((fDist < 1) && ((dwClosest == (DWORD)-1) || (fDist < fClosest))) {
         dwClosest = i;
         fClosest = fDist;
      }
   }

   // if we're not starting inside one of the segments, and we've found the
   // start occurs near the beginning of another, then move the start to that
   if (!fInside && (dwClosest != (DWORD) -1)) {
      pf = (PFLS) m_lFindLongestSegmentFound.Get(dwClosest);
      pStart = &pf->pStart;
   }

   // find the first (and only) linesegment in m_lFindLongestSegmentFound where pStart is within it
   fFound = FALSE;
   while (TRUE) {
      for (i = 0; i < m_lFindLongestSegmentFound.Num(); i++) {
         pf = (PFLS) m_lFindLongestSegmentFound.Get(i);
         if (dwDir == 0) { // mostly horizontal
            if (fLessOrig) {  // start < end
               if ((pStart->h > pf->pStart.h - EPSILON) && (pStart->h < pf->pEnd.h - EPSILON))
                  break;   // match
            }
            else { // end < start
               if ((pStart->h < pf->pStart.h + EPSILON) && (pStart->h > pf->pEnd.h + EPSILON))
                  break;   // match
            }
         }
         else {   // mostly vertical
            if (fLessOrig) {  // start < end
               if ((pStart->v > pf->pStart.v - EPSILON) && (pStart->v < pf->pEnd.v - EPSILON))
                  break;   // match
            }
            else { // end < start
               if ((pStart->v < pf->pStart.v + EPSILON) && (pStart->v > pf->pEnd.v + EPSILON))
                  break;   // match
            }
         }
      }
      if (i >= m_lFindLongestSegmentFound.Num())
         break;

      // if got here then i is the line segment that will use

      BOOL fRecalc;
      if (!fFound) {
         fFound = TRUE;

         // if the line segment start is not in bounds then calculate a new one
         fRecalc = FALSE;
         if (dwDir == 0) { // mostly horizontal
            fRecalc = (fLessOrig ? (pf->pStart.h < pStart->h) : (pf->pStart.h > pStart->h));
         }
         else {   // mostly vertical
            fRecalc = (fLessOrig ? (pf->pStart.v < pStart->v) : (pf->pStart.v > pStart->v));
         }
         if (fRecalc)
            *pFoundStart = *pStart;
         else
            *pFoundStart = pf->pStart;
      }

      // may need to trim down the end
      fRecalc = FALSE;
      if (dwDir == 0) { // mostly horizontal
         fRecalc = (fLessOrig ? (pf->pEnd.h > pEnd->h) : (pf->pEnd.h < pEnd->h));
      }
      else {   // mostly vertical
         fRecalc = (fLessOrig ? (pf->pEnd.v > pEnd->v) : (pf->pEnd.v < pEnd->v));
      }
      if (fRecalc) {
         *pFoundEnd = *pEnd;
         break;   // all done
      }
      else
         *pFoundEnd = pf->pEnd;

      // finally, move the start up for the next time
      pStart = &pf->pEnd;
   }

   return fFound;
}


/************************************************************************************
CMeshGrid::FindIntersectLongestSegment - Given a line from pStart to pEnd, this
intersects it against the polygons in the segment. These intersections are combined
together into the longest segment (starting at pStart if possible) that can be made
up until pEnd.

inputs
   PTEXUREPOINT      pStart - Start searching for
   PTEXTUREPOINT     pEnd - Finished searching for
   PTEXTUREPOINT     pFoundStart - Filled with start that found
   PTEXGTURPOINT     pFoundEnd - Filled witht he end that found
returns
   BOOL - TRUE if found (and pFoundXXX are valid), or FALSE if not
*/
BOOL CMeshGrid::FindLongestIntersectSegment (PTEXTUREPOINT pStart, PTEXTUREPOINT pEnd,
   PTEXTUREPOINT pFoundStart, PTEXTUREPOINT pFoundEnd)
{
   // if marked as tivial reject then return false
   if (m_fTrivialClip)
      return FALSE;

   // keep a list of the line segments that found
   CListFixed lFound;
   lFound.Init (sizeof(FLS));

   // if this is marked as trivial accept then special exception
   MGQUAD qEntire;
   DWORD i, k;
   if (m_fTrivialAccept) {
      memset (&qEntire, 0, sizeof(qEntire));
      qEntire.dwPoints = 4;
      for (k = 0; k < 4; k++) {
         qEntire.atHV[k].v = m_afV[(k >= 2) ? 1 : 0];
         qEntire.atHV[k].h = m_afH[(k==1 || k==2) ? 1 : 0];
      }
   }

   // find all the points with this slope
   PMGQUAD pq;
   for (i = 0; i < (m_fTrivialAccept ? 1 : m_listMGQUAD.Num()); i++) {
      pq = m_fTrivialAccept ? &qEntire : (PMGQUAD) m_listMGQUAD.Get(i);

      // loop through all the poins
      FLS fls, fls2;

      if (!Intersect2DLineWith2DPoly (pStart, pEnd, &pq->atHV[0], &pq->atHV[1],
         &pq->atHV[2], (pq->dwPoints == 4) ? &pq->atHV[3] : NULL,
         &fls.pStart, &fls.pEnd, &fls2.pStart, &fls2.pEnd))
         continue;   // doesn't inersect

      // add it
      lFound.Add (&fls);
   }

   // if there's nothing in the list then easy finish
   if (!lFound.Num())
      return FALSE;

   // find the slope and whatnot of the input line
   fp fDeltaH, fDeltaV;
   BOOL fHorz, fVert;
   DWORD dwDir;   // 0 for mostly horizontal, 1 for mostly vertical
   fDeltaH = pEnd->h - pStart->h;
   fDeltaV = pEnd->v - pStart->v;
   fHorz = (fabs(fDeltaV) < EPSILON);
   fVert = (fabs(fDeltaH) < EPSILON);
   if (fHorz && fVert)
      return FALSE;
   dwDir = (fabs(fDeltaH) < fabs(fDeltaV));
   BOOL fLessOrig;
   fLessOrig = (dwDir == 0) ? (pStart->h < pEnd->h) : (pStart->v < pEnd->v);

   // if start does not occur within a line segment then advance until there is one
   PFLS pf;
   BOOL fFound;
   fFound = FALSE;
   fp fLineMin, fLineMax;
   DWORD dwClosest;
   fp fClosest, fDist;
   BOOL fInside;
   fInside = FALSE;
   dwClosest = (DWORD)-1;
   if (dwDir == 0) { // mostly horzontal
      fLineMin = min(pStart->h, pEnd->h);
      fLineMax = max(pStart->h, pEnd->h);
   }
   else {   // mostly veritcal
      fLineMin = min(pStart->v, pEnd->v);
      fLineMax = max(pStart->v, pEnd->v);
   }
   for (i = 0; i < lFound.Num(); i++) {
      fp fMin, fMax;
      pf = (PFLS) lFound.Get(i);
      if (dwDir == 0) { // mostly horizontal
         fMin = min(pf->pEnd.h, pf->pStart.h);
         fMax = max(pf->pEnd.h, pf->pStart.h);
      }
      else {
         fMin = min(pf->pEnd.v, pf->pStart.v);
         fMax = max(pf->pEnd.v, pf->pStart.v);
      }

      // combination
      fp fCMin, fCMax;
      fCMin = max(fLineMin, fMin);
      fCMax = min(fLineMax, fMax);
      if (fCMin >= fCMax)
         continue;   // lines don't intersect

      // else they intersect
      fDist = 1000000;
      if (dwDir == 0) { // horizontal
         if ((pStart->h >= fMin) && (pStart->h <= fMax))
            fInside = TRUE;   // the start is on the sinside, so don't worry about closest
         else
            fDist = fabs(pf->pStart.h - pStart->h);
      }
      else {   // vertical
         if ((pStart->v >= fMin) && (pStart->v <= fMax))
            fInside = TRUE;   // the start is on the sinside, so don't worry about closest
         else
            fDist = fabs(pf->pStart.v - pStart->v);
      }
      if ((fDist < 1) && ((dwClosest == (DWORD)-1) || (fDist < fClosest))) {
         dwClosest = i;
         fClosest = fDist;
      }
   }

   // if we're not starting inside one of the segments, and we've found the
   // start occurs near the beginning of another, then move the start to that
   if (!fInside && (dwClosest != (DWORD) -1)) {
      pf = (PFLS) lFound.Get(dwClosest);
      pStart = &pf->pStart;
   }

   // find the first (and only) linesegment in lFound where pStart is within it
   fFound = FALSE;
   while (TRUE) {
      for (i = 0; i < lFound.Num(); i++) {
         pf = (PFLS) lFound.Get(i);
         if (dwDir == 0) { // mostly horizontal
            if (fLessOrig) {  // start < end
               if ((pStart->h > pf->pStart.h - EPSILON) && (pStart->h < pf->pEnd.h - EPSILON))
                  break;   // match
            }
            else { // end < start
               if ((pStart->h < pf->pStart.h + EPSILON) && (pStart->h > pf->pEnd.h + EPSILON))
                  break;   // match
            }
         }
         else {   // mostly vertical
            if (fLessOrig) {  // start < end
               if ((pStart->v > pf->pStart.v - EPSILON) && (pStart->v < pf->pEnd.v - EPSILON))
                  break;   // match
            }
            else { // end < start
               if ((pStart->v < pf->pStart.v + EPSILON) && (pStart->v > pf->pEnd.v + EPSILON))
                  break;   // match
            }
         }
      }
      if (i >= lFound.Num())
         break;

      // if got here then i is the line segment that will use

      BOOL fRecalc;
      if (!fFound) {
         fFound = TRUE;

         // if the line segment start is not in bounds then calculate a new one
         fRecalc = FALSE;
         if (dwDir == 0) { // mostly horizontal
            fRecalc = (fLessOrig ? (pf->pStart.h < pStart->h) : (pf->pStart.h > pStart->h));
         }
         else {   // mostly vertical
            fRecalc = (fLessOrig ? (pf->pStart.v < pStart->v) : (pf->pStart.v > pStart->v));
         }
         if (fRecalc)
            *pFoundStart = *pStart;
         else
            *pFoundStart = pf->pStart;
      }

      // may need to trim down the end
      fRecalc = FALSE;
      if (dwDir == 0) { // mostly horizontal
         fRecalc = (fLessOrig ? (pf->pEnd.h > pEnd->h) : (pf->pEnd.h < pEnd->h));
      }
      else {   // mostly vertical
         fRecalc = (fLessOrig ? (pf->pEnd.v > pEnd->v) : (pf->pEnd.v < pEnd->v));
      }
      if (fRecalc) {
         *pFoundEnd = *pEnd;
         break;   // all done
      }
      else
         *pFoundEnd = pf->pEnd;

      // finally, move the start up for the next time
      pStart = &pf->pEnd;
   }

   return fFound;
}

/************************************************************************************
CMeshGrid::IntersectLineWithGrid - Intersects a line in space with the grid.
If the grid is intersected in two points then it choses the point closest to pStart.

inputs
   PCPoint           pStart - Start of the line
   PCPoint           pDirection - direction of the line
   PTEXTUREPOINT     ptp - Filled with the H and V (0..1 each) where the line interesects
                     with the grid.
   fp            *pfAlpha - Filled with the alpha value. 0.0 => pStart, 1.0 => pStart + pDirection.
                     Can be null.
   BOOL              fIncludeCutouts - If TRUE, then intersect with the surface, but only
                     if the area is not already cutout
returns
   BOOL - TRUE if it interesects
*/
BOOL CMeshGrid::IntersectLineWithGrid (PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp,
                                       fp *pfAlpha, BOOL fIncludeCutouts)
{
   // BUGFIX - Call global function for intersect quad
   if (!IntersectLineQuad (pStart, pDirection, &m_apPosn[0][0], &m_apPosn[0][1],
      &m_apPosn[1][1], &m_apPosn[1][0], NULL, pfAlpha, ptp))
      return FALSE;

   // adjust H and V
   if (ptp) {
      ptp->h = ptp->h * (m_afH[1] - m_afH[0]) + m_afH[0];
      ptp->v = ptp->v * (m_afV[1] - m_afV[0]) + m_afV[0];
   }

   if (!fIncludeCutouts || m_fTrivialAccept)
      return TRUE;
   if (m_fTrivialClip)
      return FALSE;  // nothing here

   // else, if get here then need to see if intersect against any cutouts
   // take advantage of a few facts:
   // 1) Polygons in cutout have flat top and bottom
   // 2) Most of the time (unless very unlucky) surface will be reasonably flat
   //    so can take intersection that found
   DWORD i, j;
   for (i = 0; i < m_listMGQUAD.Num(); i++) {
      PMGQUAD pq = (PMGQUAD) m_listMGQUAD.Get(i);

      // find min/max in HV
      TEXTUREPOINT tpMin, tpMax;
      for (j = 0; j < pq->dwPoints; j++) {
         if (j) {
            tpMin.h = min(tpMin.h, pq->atHV[j].h);
            tpMax.h = max(tpMax.h, pq->atHV[j].h);
            tpMin.v = min(tpMin.v, pq->atHV[j].v);
            tpMax.v = max(tpMax.v, pq->atHV[j].v);
         }
         else {
            tpMin = pq->atHV[i];
            tpMax = pq->atHV[i];
         }
      } // loop find min/max

      // if outside this boundary continue
      if (!((ptp->h >= tpMin.h) && (ptp->h <= tpMax.h) && (ptp->v >= tpMin.v) && (ptp->v <= tpMax.v)))
         continue;

      // go through all the lines and figure out how many lines the point
      // is to the right of
      fp fL[2];
      DWORD dwFound;
      dwFound = 0;
      for (j = 0; j < pq->dwPoints; j++) {
         PTEXTUREPOINT p1, p2;
         p1 = &pq->atHV[j];
         p2 = &pq->atHV[(j+1)%pq->dwPoints];

         fp fDeltaH, fDeltaV;
         fDeltaH = p2->h - p1->h;
         fDeltaV = p2->v - p1->v;
         if (fabs(fDeltaV) < EPSILON)
            continue;   // horizontal line

         // becayse top and bottom are level, already know that vertically inside line

         // see which side horizontally on
         fp mInv, bInv;
         mInv = fDeltaH / fDeltaV;
         bInv = p1->h - mInv * p1->v;

         // and the point
         fp fH;
         fH = mInv * ptp->v + bInv;
         if (dwFound >= 2) {
#ifdef _DEBUG
            OutputDebugString ("IntersectLineWithGrid: Found too many sides\r\n");
#endif // _DEBUG
            break;   // should only find two max
         }
         fL[dwFound] = fH;
         dwFound++;
      }

      // if between the two points then intersect
      if ((dwFound ==2) && (ptp->h >= min(fL[0],fL[1])-EPSILON) && (ptp->h <= max(fL[0],fL[1])+EPSILON))
         return TRUE;
   }

   // if get here dind't intersect
   return FALSE;
}


/************************************************************************************
CMeshGrid::Init - Call this to initialize the grid object

inputs
   fp      fHLeft, fHRight - H value on left and right edge of grid. left < right
   fp      fHTop, fHBottom - V values on top and bottom edge of grid. top < bottom
   PCPoint     pUL...pLR - Point (in world space) of corner
   PCPoint     nUL...nUR - Normal of corner. Must be normalized.
   PTEXTUREPOINT ptUL...puLR - Texture h and v at corners
returns
   BOOL - TRUE if successful
*/
BOOL CMeshGrid::Init (fp fHLeft, fp fHRight, fp fVTop, fp fVBottom,
   PCPoint pUL, PCPoint pUR, PCPoint pLL, PCPoint pLR,
   PCPoint nUL, PCPoint nUR, PCPoint nLL, PCPoint nLR,
   PTEXTUREPOINT ptUL, PTEXTUREPOINT ptUR, PTEXTUREPOINT ptLL, PTEXTUREPOINT ptLR)
{
   // points
   m_apPosn[0][0].Copy (pUL);
   m_apPosn[0][1].Copy (pUR);
   m_apPosn[1][0].Copy (pLL);
   m_apPosn[1][1].Copy (pLR);

   // normals
   m_apNorm[0][0].Copy (nUL);
   m_apNorm[0][1].Copy (nUR);
   m_apNorm[1][0].Copy (nLL);
   m_apNorm[1][1].Copy (nLR);

   // textures
   m_aText[0][0] = *ptUL;
   m_aText[0][1] = *ptUR;
   m_aText[1][0] = *ptLL;
   m_aText[1][1] = *ptLR;

   // start and end h
   m_afH[0] = fHLeft;
   m_afH[1] = fHRight;
   m_afV[0] = fVTop;
   m_afV[1] = fVBottom;

   // cleaer out the lists
   m_listMGLINE.Clear();
   m_listMGTICK.Clear();
   m_fTrivialClip = FALSE;
   m_fTrivialAccept = TRUE;
   m_listMGQUAD.Clear();

   // call tickincrement to set some values
   TickIncrement (&m_listMGTICK, fVTop, fVBottom, 0);

   return TRUE;
}

/****************************************************************************************
CMeshGrid::Point - Given an h and v known to be within the grid, fills in a point
with the world space location of teh surface.

inputs
   fp      h,v - Within grid
   PCPoint     pP - Filled with point for location of point in world space. Can be NULL
   PCPoint     pN - Filled with normal for the point in world space. Can be NULL.
   PTEXTUREPOINT pT - Filled with the texture map for the point in world space. Can be NULL.
returns
   none
*/
void CMeshGrid::Point (fp h, fp v, PCPoint pP, PCPoint pN, PTEXTUREPOINT pT)
{
   // 0 to 1 within the grid
   h = (h - m_afH[0]) / (m_afH[1] - m_afH[0]);
   v = (v - m_afV[0]) / (m_afV[1] - m_afV[0]);

   CPoint pTop, pBottom;

   // convert to points
   if (pP) {
      pTop.Subtract (&m_apPosn[0][1], &m_apPosn[0][0]);
      pTop.Scale (h);
      pTop.Add (&m_apPosn[0][0]);

      pBottom.Subtract (&m_apPosn[1][1], &m_apPosn[1][0]);
      pBottom.Scale (h);
      pBottom.Add (&m_apPosn[1][0]);

      pP->Subtract (&pBottom, &pTop);
      pP->Scale (v);
      pP->Add (&pTop);
   }

   if (pN) {
      pTop.Subtract (&m_apNorm[0][1], &m_apNorm[0][0]);
      pTop.Scale (h);
      pTop.Add (&m_apNorm[0][0]);

      pBottom.Subtract (&m_apNorm[1][1], &m_apNorm[1][0]);
      pBottom.Scale (h);
      pBottom.Add (&m_apNorm[1][0]);

      pN->Subtract (&pBottom, &pTop);
      pN->Scale (v);
      pN->Add (&pTop);
      pN->Normalize ();
   }

   if (pT) {
      TEXTUREPOINT tTop, tBottom;
      tTop.h = h * (m_aText[0][1].h - m_aText[0][0].h) + m_aText[0][0].h;
      tTop.v = h * (m_aText[0][1].v - m_aText[0][0].v) + m_aText[0][0].v;
      tBottom.h = h * (m_aText[1][1].h - m_aText[1][0].h) + m_aText[1][0].h;
      tBottom.v = h * (m_aText[1][1].v - m_aText[1][0].v) + m_aText[1][0].v;

      pT->h = v * (tBottom.h - tTop.h) + tTop.h;
      pT->v = v * (tBottom.v - tTop.v) + tTop.v;
   }
}

/****************************************************************************************
CMeshGrid::IntersectLineWithGrid - Intersects the line with the grid and remembers
the intersection. Note that the line goes from p1 to p2, and that to the right
of this is clipped, this if p1 is lower than p2 the the right part of the line is
clipped, and it p1 is higher than p2 then the left side is clipped. This is so
a clockwise set of points will clip within the region, instead of outside it.

inputs
   PTEXTUREPOINT     p1 - Point 1. In mesh space, from 0..1 x 0..1. This grid is only part of it
   PTEXTUREPOINT     p2 - Second point.
returns
   BOOL - TRUE if success
*/
BOOL CMeshGrid::IntersectLineWithGrid (PTEXTUREPOINT p1, PTEXTUREPOINT p2)
{
   BOOL fRight = FALSE;
   if (p1->v > p2->v) {
      // flip the points
      PTEXTUREPOINT pt;
      pt = p1;
      p1 = p2;
      p2 = pt;
      fRight = !fRight;
   }

   // clip against top and bottom since if beyond dont care
   TEXTUREPOINT D, A, B;
   fp fAlpha;
   A = *p1;
   B = *p2;
   D.h = B.h - A.h;  // delta
   D.v = B.v - A.v;

   // if it's a strictly horizontal line then completely ignore since it wont affect us
   if (D.v == 0.0)
      return TRUE;

   if (D.v) {
      // top
      fAlpha = (m_afV[0] - A.v) / D.v;
      if (fAlpha >= 1.0)
         return TRUE;   // trivially clipped below
      else if (fAlpha > 0) {
         A.h = D.h * fAlpha + A.h;
         A.v = m_afV[0];

         // recalculate delta
         D.h = B.h - A.h;  // delta
         D.v = B.v - A.v;
      };
      // else, fAlpha < 0... A and B are both below the top so dont clip

      // bottom
      fAlpha = (m_afV[1] - A.v) / D.v;
      if (fAlpha >= 1.0) {
         // do nothing because A and B are both above the bottom
      }
      else if (fAlpha > 0.0) {
         B.h = D.h * fAlpha + A.h;
         B.v = m_afV[1];

         // recalculate delta
         D.h = B.h - A.h;  // delta
         D.v = B.v - A.v;
      }
      else
         return TRUE;   // trivially clipped above.
   }
   // else its a horizontal line

   // clip against right and left
   if (D.h) {
      // right
      fAlpha = (m_afH[1] - A.h) / D.h;
      if ((fAlpha > 0) && (fAlpha < 1))  {
         // clipped
         if (B.h > A.h) {
            B.h = m_afH[1];
            B.v = D.v * fAlpha + A.v;
         }
         else {
            A.h = m_afH[1];
            A.v = D.v * fAlpha + A.v;
         }

         // recalculate delta
         D.h = B.h - A.h;  // delta
         D.v = B.v - A.v;
      }
      else {
         // note that if A.h >= H[1] then B.h >= H[1] also, otherwise would have clipped
         if ((A.h >= m_afH[1])  && (B.h >= m_afH[1]))
            return TRUE;   // trivially clipped to the right
      }

      // left
      fAlpha = (m_afH[0] - A.h) / D.h;
      if ((fAlpha > 0) && (fAlpha < 1)) {
         // remember top and bottom of region to add to ticks
         fp fTop = A.v;
         fp fBottom = B.v;

         // it's partially within. clip
         if (A.h < B.h) {
            A.h = m_afH[0];
            A.v = D.v * fAlpha + A.v;
            fBottom = A.v;
         }
         else {
            B.h = m_afH[0];
            B.v = D.v * fAlpha + A.v;
            fTop = B.v;
         }

         // increment this tick
         TickIncrement (&m_listMGTICK, fTop, fBottom, fRight ? -1 : 1);
      }
      else {
         // NOTE: If A.h < H[0] then B.h < H[0] since otherwise would have cliped
         if ((A.h <= m_afH[0]) && (B.h <= m_afH[0])) {
            // it's entirely to the left. However, since it's going to affect
            // the left edge, adjust that based on top and bottom
            TickIncrement (&m_listMGTICK, A.v, B.v, fRight ? -1 : 1);
            return TRUE;
         }
         // else, its wholly within, so do nothing
      }
   }
   else {
      // else its a vertical line

      // if it's beyond the right edge then skip it
      if (A.h >= m_afH[1])
         return TRUE;

      // it it's beyond the left edge then just make a note for ticks
      if (A.h <= m_afH[0]) {
         TickIncrement (&m_listMGTICK, A.v, B.v, fRight ? -1 : 1);
         return TRUE;
      }

      // else, it's wholly within so do nothing
   }

   // we have taken at least part of the line
   MGLINE line;
   DWORD i;
   line.fRight = fRight;
   for (i = 0; i < 2; i++) {
      line.atHV[i].h = i ? B.h : A.h;
      line.atHV[i].v = i ? B.v : A.v;

#ifdef _DEBUG
      if ((line.atHV[i].h < -EPSILON) || (line.atHV[i].h > 1+EPSILON) ||
         (line.atHV[i].v < -EPSILON) || (line.atHV[i].v > 1+EPSILON))
         OutputDebugString ("WARNING: Line beyond spline\r\n");
#endif

      Point (line.atHV[i].h, line.atHV[i].v,
         &line.apPosn[i], &line.apNorm[i], &line.atText[i]);

   }
   m_listMGLINE.Add (&line);

   return TRUE;
}


/******************************************************************************************
CMeshGrid::TickBisect - Given a list of MGTICKs sorted in ascenting order by fV, this
inserts another tick of fValue in place (if it's ont there already.) The new tick's iCount
is the same as the one just above it (which is split.)

inputs
   PCListFixed       pl - List of MGTICK structures
   fp            fValue - Where to split. (If the value does not exist then it's not split)
retursn
   DWORD - Index pointing to the tick.
*/
DWORD CMeshGrid::TickBisect (PCListFixed pl, fp fValue)
{
   DWORD dwInsertBefore = 0;
   DWORD dwNumElem = pl->Num();

   // get the start of the list
   PMGTICK pList = (PMGTICK) pl->Get(0);

   // loop
   if (dwNumElem) {
      DWORD dwStart, dwCur, dwTest;

      // count the bits to represent
      for (dwCur = 1, dwStart = (dwNumElem >> 1); dwStart; dwStart >>= 1, dwCur <<= 1);

      // start with that bit
      for (dwStart = 0; dwCur; dwCur >>= 1) {
         dwTest = dwStart | dwCur;
         if (dwTest > dwNumElem)
            continue;   // this number would be too large
         if (pList[dwTest-1].fV == fValue)
            return dwTest - 1;
         else if (pList[dwTest-1].fV <= fValue) // leave <= even though really <
            dwStart = dwTest; // it's less than so use this as a starting point
      }

      dwInsertBefore = dwStart;
   };
   // else dwNumElem == 0, so dwInsertBefore = 0

   // if it's equal don't do anything
   MGTICK mg;
   if (dwInsertBefore)
      mg.iCount = pList[dwInsertBefore-1].iCount;
   else
      mg.iCount = 0;
   mg.fV = fValue;

   // add/insert
   if (dwInsertBefore >= dwNumElem) {
      pl->Add (&mg);
      return dwNumElem - 1;
   }
   else {
      pl->Insert (dwInsertBefore, &mg);
      return dwInsertBefore;
   }

}

/************************************************************************************
CMeshGrid::TickIncrement - Increments all the tick values in range by the given amount.

inputs
   int            iInc - increment
returns
   BOOL - TRUE if succcessful
*/
BOOL CMeshGrid::TickIncrement (int iInc)
{
   return TickIncrement (&m_listMGTICK, m_afV[0], m_afV[1], iInc);
}

/************************************************************************************
CMeshGrid::TickIncrement - Find two ticks at fmIn and fMax, and increment everything
in-between by iInc.

inputs
   PCListFixed    pl - List of MGTICK
   fp         fMin - Smallest value
   fp         fMax - Largest value
   int            iInc - Increment all ticks betwwen fMin(inclusive) and fMax (exclusive)
                     by iInc
returns
   BOOL - TRUE if successful
*/
BOOL CMeshGrid::TickIncrement (PCListFixed pl, fp fMin, fp fMax, int iInc)
{
   DWORD dwMin = TickBisect (pl, fMin);   // can do this because max is later in list
   DWORD dwMax = TickBisect (pl, fMax);

   if (iInc) {
      PMGTICK pTick = (PMGTICK) pl->Get(0);
      for (; dwMin < dwMax; dwMin++) {
         pTick[dwMin].iCount += iInc;
      }
   }

   return TRUE;
}


/************************************************************************************
CMeshGrid::TickMinimize - Loops through the ticks in pl looking for adjacent ticks with the
same number in iCount, and merging them into one.

inputs
   PCListFixed       pl - List of MGTICK
returns
   BOOL - TRUE if successful
*/
BOOL CMeshGrid::TickMinimize (PCListFixed pl)
{
   DWORD dwNum = pl->Num();
   DWORD i;
   for (i = dwNum-1; i; i--) {
      PMGTICK p1 = (PMGTICK) pl->Get(i-1);
      PMGTICK p2 = p1 + 1;
      if (p1->iCount == p2->iCount)
         pl->Remove (i);
   }

   return TRUE;
}

/************************************************************************************
CMeshGrid::IntersectTwoLines - Given two lines (with bounds on each end), figure
out where they intersect (in V) and return that point. Or, return -1 if they dont
interesect.

inputs
   PMGLINE        pl1   - first line
   PMGLINE        pl2 - second line
returns
   fp - Point in V where they intersect, or -1 if dont
*/
fp CMeshGrid::IntersectTwoLines (PMGLINE pl1, PMGLINE pl2)
{
   fp fRet;

   // trivial test, bottom of one is less than top of other
   if ((pl1->atHV[1].v <= pl2->atHV[0].v) || (pl2->atHV[1].v <= pl1->atHV[0].v))
      return -1;

   // right of one is left than left of other
   fp fLeft1, fLeft2, fRight1, fRight2;
   fLeft1 = min(pl1->atHV[0].h, pl1->atHV[1].h);
   fRight1 = max(pl1->atHV[0].h, pl1->atHV[1].h);
   fLeft2 = min(pl2->atHV[0].h, pl2->atHV[1].h);
   fRight2 = max(pl2->atHV[0].h, pl2->atHV[1].h);
   if ((fRight2 <= fLeft1) || (fRight1 <= fLeft2))
      return -1;

   // calculate slope for each one
   BOOL fVert1, fVert2;
   fp fm1, fm2, fb2, fb1;
   fp fx, fy;
   fx = pl1->atHV[1].h - pl1->atHV[0].h;
   fy = pl1->atHV[1].v - pl1->atHV[0].v;
   if (fx) {
      fVert1 = FALSE;
      fm1 = fy / fx;
      fb1 = pl1->atHV[0].v - fm1 * pl1->atHV[0].h;
   }
   else
      fVert1 = TRUE;
   fx = pl2->atHV[1].h - pl2->atHV[0].h;
   fy = pl2->atHV[1].v - pl2->atHV[0].v;
   if (fx) {
      fVert2 = FALSE;
      fm2 = fy / fx;
      fb2 = pl2->atHV[0].v - fm2 * pl2->atHV[0].h;
   }
   else
      fVert2 = TRUE;

   if (!fVert1 && !fVert2) {
      // two diagonal lines that will eventually intersct
      fRet = (fm2 * fb1 - fm1 * fb2) / (fm2 - fm1);
   }
   else if (fVert1 && fVert2)
      return -1;  // never intersect
   else if (fVert1) {   // && !fVert2
      fRet = fm2 * pl1->atHV[0].h + fb2;
   }
   else {   // !fVert1 && fVert2
      fRet = fm1 * pl2->atHV[0].h + fb1;
   }

   if ((fRet <= pl1->atHV[0].v) || (fRet >= pl1->atHV[1].v) ||
      (fRet <= pl2->atHV[0].v) || (fRet >= pl2->atHV[1].v))
      return -1;  // intersect, but not in the lengths
   return fRet;
}

/************************************************************************************
CMeshGrid::IntersectLinesAndTick - Loops through all the lines and intersects every
line with every other line. If they intersect then a tick is added to m_listMGTICK.
This way, know vertical locations in grid wherever there's an intersection.
This also ticks the line endpoints.

inputs
   none
returns
   BOOL - TRUE if successful
*/
BOOL CMeshGrid::IntersectLinesAndTick (void)
{
   DWORD i, j;
   DWORD dwNum = m_listMGLINE.Num();
   PMGLINE pLine = (PMGLINE) m_listMGLINE.Get(0);
   PMGLINE p1, p2;

   for (i = 0; i < dwNum; i++) {
      p1 = pLine + i;
      // tick endpoints if they're in range
      if ((p1->atHV[0].v > m_afV[0]) && (p1->atHV[0].v < m_afV[1]))
         TickBisect (&m_listMGTICK, p1->atHV[0].v);
      if ((p1->atHV[1].v > m_afV[0]) && (p1->atHV[1].v < m_afV[1]))
         TickBisect (&m_listMGTICK, p1->atHV[1].v);

      // tick intersections
      fp fInter;
      for (j = i+1; j < dwNum; j++) {
         p2 = pLine + j;

         fInter = IntersectTwoLines (p1, p2);
         if ((fInter > m_afV[0]) && (fInter < m_afV[1]))
            TickBisect (&m_listMGTICK, fInter);
      }
   }

   return TRUE;
}

typedef struct {
   PMGLINE     pLine;   // line
   fp      fHIntersect[2];   // where it intersects top (0) and bottom (1) lines
   fp      fHAvg;      /// average - used for sorting
} MGLINESEG, *PMGLINESEG;

/***********************************************************************************
IntersectHorzLine - Intersect a line (ignoring end-points) against a given horiztonal
line.

inputs
   fp      fV - V value of horizontal line
   PMGLINE     pLine - Line
returns
   fp - H where intersects, or -1 if error
*/
fp IntersectHorzLine (fp fV, PMGLINE pLine)
{
   fp fx, fy;
   fx = pLine->atHV[1].h - pLine->atHV[0].h;
   fy = pLine->atHV[1].v - pLine->atHV[0].v;

   if (fabs(fy) < EPSILON) // BUGFIX Was checking for just 0
      return -1;  // never intersects
   if (fabs(fx) < EPSILON) {  // BUGFIX - Was checking for 0
      // it's vertical.
      return pLine->atHV[0].h;
   }
   
   // find slope and intersection
   fp fb;
   fp fm;
   fm = fy / fx;
   fb = pLine->atHV[0].v - fm * pLine->atHV[0].h;

   // and where it actually interesects
   return (fV - fb) * fx / fy;
}


/***********************************************************************************
HVToAlpha - Given a MGLINE and known h,v, figure out an alpha (from 0..1) indicating
distance between the top point and bottom point where the intersection occurs

inputs
   PMGLINE     pLine - Line
   fp      h - h value of intersection
   fp      v - v value of intersection
returns
   fp - alpha
*/
fp HVToAlpha (PMGLINE pLine, fp h, fp v)
{
   fp fx, fy;
   fx = pLine->atHV[1].h - pLine->atHV[0].h;
   fy = pLine->atHV[1].v - pLine->atHV[0].v;

   // use either of h or v depending upon which one is not 0
   if ((fx == 0) && (fy == 0))
      return 0;   // seems to be a point
   if (fabs(fx) > fabs(fy))
      return (h - pLine->atHV[0].h) / fx;
   else
      return (v - pLine->atHV[0].v) / fy;
}

static int _cdecl GenerateQuadsSort (const void *elem1, const void *elem2)
{
   PMGLINESEG pdw1, pdw2;
   pdw1 = (PMGLINESEG) elem1;
   pdw2 = (PMGLINESEG) elem2;

   if (pdw1->fHAvg > pdw2->fHAvg)
      return 1;
   else if (pdw1->fHAvg < pdw2->fHAvg)
      return -1;
   else
      return 0;
}


/************************************************************************************
CMeshGrid::GenerateQuads - Given a starting iCoutout value (0+ means to leave in,
negative numbers mean cutout), this generates quadrilaterals for the 
"scan line" from fVTop to fVBottom. It's assumed that none of the lines intersect
between fVTop and fVBottom. Adds to m_listMGQUAD.

inputs
   int         iCutOut - Starting cutout value.
   fp      fVTop - top V value (smallest)
   fp      fVBottom - bottom V value (largest)
returns
   BOOL - TRUE if OK
*/
BOOL CMeshGrid::GenerateQuads (int iCutOut, fp fVTop, fp fVBottom)
{
   // will have a list of MGLINESEG containing a pointer to the line, and
   // where it interesects the top and bottom (which it must otherwise it
   // wouldn't be included
   CListFixed lInter;
   lInter.Init (sizeof(MGLINESEG));
   MGLINESEG ls;

   // make lines for two edges
   MGLINE   mEdge[2];
   DWORD x, y;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
      mEdge[x].apNorm[y].Copy (&m_apNorm[y][x]);
      mEdge[x].apPosn[y].Copy (&m_apPosn[y][x]);
      mEdge[x].atHV[y].h = m_afH[x];
      mEdge[x].atHV[y].v = m_afV[y];
      mEdge[x].atText[y] = m_aText[y][x];
      mEdge[x].fRight = (x ? TRUE : FALSE);
   }
   for (x = 0; x < 2; x++) {
      // because it's a vertical line, intersection is easy
      ls.pLine = &mEdge[x];
      ls.fHIntersect[0] = ls.fHIntersect[1] = ls.fHAvg = ls.pLine->atHV[0].h;
      lInter.Add (&ls);
   }

   // because left line will be included, offset iCutOut by -1 so that doesn't
   // start drawing polygon until left line
   iCutOut--;
   if (iCutOut >= 0)
      return FALSE;  // error. Shouldn't happen

   // loop through lines and figure out which of them occur between fVTop and
   // fVBottom, adding them to our temporary list, along with two edges.
   DWORD i;
   for (i = 0; i < m_listMGLINE.Num(); i++) {
      PMGLINE pLine = (PMGLINE) m_listMGLINE.Get(i);

      // if the bottom of the line is less than the top line looking at,
      // or the top of the line is more than the bottom line looking at then skip
      if ((pLine->atHV[1].v <= fVTop) || (pLine->atHV[0].v >= fVBottom))
         continue;

      // else, find add it
      ls.pLine = pLine;
      ls.fHIntersect[0] = IntersectHorzLine (fVTop, pLine);
      ls.fHIntersect[1] = IntersectHorzLine (fVBottom, pLine);
      // BUGFIX - There was roundoff error at test for ls.fHIntersect[1] < 0, so changed
      if ((ls.fHIntersect[0] < -EPSILON) || (ls.fHIntersect[1] < -EPSILON))
         continue;   // didnt intersect with the lines, which means horizontal, and don't care
#ifdef _DEBUG
      if ((ls.fHIntersect[0] < -EPSILON) || (ls.fHIntersect[0] > 1+EPSILON) ||
         (ls.fHIntersect[1] < -EPSILON) || (ls.fHIntersect[1] > 1+EPSILON))
         OutputDebugString ("WARNING: GenerateQuads - Intersect beyond range\r\n");
#endif
      ls.fHAvg = (ls.fHIntersect[0] + ls.fHIntersect[1]) / 2;
      lInter.Add (&ls);
   }

   // sort edges from left to right - as they appear in in the list. The easiest
   // way is to average left/right interesctions with top/bottom line
   qsort (lInter.Get(0), lInter.Num(), sizeof(MGLINESEG), GenerateQuadsSort);

   DWORD dwLeft, dwRight, dwCur;
   PMGLINESEG pls;
   dwCur = 0;
   while (TRUE) {
      // repeat until there's and edge that turns iCutout >= 0
      while (TRUE) {
         pls = (PMGLINESEG) lInter.Get(dwCur);
         if (!pls)
            return TRUE;   // end of list, all done

         if (pls->pLine->fRight)
            iCutOut--;
         else
            iCutOut++;

         // if transitioned then remember this
         if (iCutOut >= 0) {
            dwLeft = dwCur;
            dwCur++;
            break;
         }

         // else, look at the next one
         dwCur++;
      }

      // repeat until there's an edge that turns iCutout <= 0
      while (TRUE) {
         pls = (PMGLINESEG) lInter.Get(dwCur);
         if (!pls)
            return TRUE;   // end of list, all done

         if (pls->pLine->fRight)
            iCutOut--;
         else
            iCutOut++;

         // if transitioned then remember this
         if (iCutOut < 0) {
            dwRight = dwCur;
            dwCur++;
            break;
         }

         // else, look at the next one
         dwCur++;
      }

      // make a polygon from those two edges and top/bottom (note that may not need
      // top or bottom). And watch out for polygons without a top or bottom
      MGQUAD q;
      PMGLINESEG pLeft, pRight;
      DWORD j;
      pLeft = (PMGLINESEG) lInter.Get(dwLeft);
      pRight = (PMGLINESEG) lInter.Get(dwRight);
      for (i = 0, j = 0; i < 4; i++) {
         // which line?
         PMGLINE pLine;
         switch (i) {
         case 0:
         case 3:
            pls = pLeft;
            break;
         case 1:
         case 2:
            pls = pRight;
            break;
         }
         pLine = pls->pLine;

         // if this is line from 0 to 1, or 2 to 3 then it's an intersection
         // with the top or bottom. If the top/bottom intersections are the same
         // then dont add one of the points since it's a triangle (or worse, nothing)
         if (i == 0) {
            if (pLeft->fHIntersect[0] + EPSILON >= pRight->fHIntersect[0])
               continue;
         }
         else if (i == 2) {
            if (pLeft->fHIntersect[1] + EPSILON >= pRight->fHIntersect[1])
               continue;
         }

         // where does it intersect?
         fp h, v;
         switch (i) {
         case 0:
         case 1:
            h = pls->fHIntersect[0];
            v = fVTop;
            break;
         case 2:
         case 3:
            h = pls->fHIntersect[1];
            v = fVBottom;
            break;
         }

         // convert this to an alpha
         fp fAlpha;
         fAlpha = HVToAlpha (pLine, h, v);

         // generate the points, normals, etc from this
         q.apPosn[j].Subtract (&pLine->apPosn[1], &pLine->apPosn[0]);
         q.apPosn[j].Scale (fAlpha);
         q.apPosn[j].Add (&pLine->apPosn[0]);
         q.apNorm[j].Subtract (&pLine->apNorm[1], &pLine->apNorm[0]);
         q.apNorm[j].Scale (fAlpha);
         q.apNorm[j].Add (&pLine->apNorm[0]);
         q.apNorm[j].Normalize();
         q.atText[j].h = (1.0 - fAlpha) * pLine->atText[0].h + fAlpha * pLine->atText[1].h;
         q.atText[j].v = (1.0 - fAlpha) * pLine->atText[0].v + fAlpha * pLine->atText[1].v;
         q.atHV[j].h = h;
         q.atHV[j].v = v;
#ifdef _DEBUG
         if ((q.atHV[j].h < -EPSILON) || (q.atHV[j].h > 1+EPSILON) ||
            (q.atHV[j].v < -EPSILON) || (q.atHV[j].v > 1+EPSILON))
            OutputDebugString ("WARNING: GenerateQuads - Line beyond spline\r\n");
#endif
         j++;
      }
      q.dwPoints = j;


      // if there are 3 or more points then add it
      if (q.dwPoints >= 3)
         m_listMGQUAD.Add (&q);
#ifdef _DEBUG
      else
         q.dwPoints = 0;
#endif

      // go back to looking until iCoutout >= 0
   }
}

/****************************************************************************************
CMeshGrid::GenerateQuads - Call this after all the lines have been interested with
the grid through IntersectLineWithGrid(). It then generates the quadrilaterals used
to draw the object

Whtat this does:
- Set trivial reject and accept to false
- Clear the list of quadrilaterals
- Calls TickMinimize() to reduce the number of ticks down
- If there aren't any lines...:
   If all the tick values are 0+ then set trivial accept.
   If all the tick values are < 0 then set trivial reject
- Calls IntersectLinesAndTick() to find out where all the lines intersect.
- For each tick, call GenerateQuads() with the extent of the tick to genate all quadrilaterals
- If no quadrilaterals are generated set trivial reject

inputs
   none
returns
   BOOL - TRUE if success
*/
BOOL CMeshGrid::GenerateQuads (void)
{
   DWORD i;
   PMGTICK pTick;

   // reset values to empty
   m_fTrivialAccept = m_fTrivialClip = FALSE;
   m_listMGQUAD.Clear();

   // minimize the list of ticks
   TickMinimize (&m_listMGTICK);

   // if arent any lines may be able to quit here
   if (!m_listMGLINE.Num()) {
      // loop through all the ticks. And see how many positive and how many negative
      DWORD dwPositive = 0, dwNegative = 0;
      for (i = 0; i < m_listMGTICK.Num(); i++) {
         pTick = (PMGTICK) m_listMGTICK.Get(i);
         if (pTick->iCount >= 0)
            dwPositive++;
         else
            dwNegative++;
      }

      if (!dwPositive) {
         m_fTrivialClip = TRUE;
         return TRUE;
      }
      if (!dwNegative) {
         m_fTrivialAccept = TRUE;
         return TRUE;
      }
   }

   // Calls IntersectLinesAndTick() to find out where all the lines intersect.
   IntersectLinesAndTick ();

   // For each tick, call GenerateQuads() with the extent of the tick to genate all quadrilaterals
   for (i = 0; i < m_listMGTICK.Num(); i++) {
      PMGTICK pNext;
      pTick = (PMGTICK) m_listMGTICK.Get(i);
      pNext = (PMGTICK) m_listMGTICK.Get(i+1);

      // range
      fp fMin, fMax;
      fMin = max(pTick->fV, m_afV[0]);
      fMax = pNext ? min(pNext->fV, m_afV[1]) : m_afV[1];
      if (fMin + EPSILON >= fMax)   // BUGFIX - Add EPSILON so don't end up with tiny wedges drawn
         continue;   // nothing to scan out

      // add it
      GenerateQuads (pTick->iCount, fMin, fMax);
   }

   // If no quadrilaterals are generated set trivial reject
   if (!m_listMGQUAD.Num())
      m_fTrivialClip = TRUE;

   return TRUE;
}


/****************************************************************************************
CMeshGrid::DrawQuads - Draws all the quadrilaterals cached in m_listMGQUAD (as
generated by GenerateQuads()). It also checks for trivial accept and reject.

inputs
   PCRenderSurface      pSurf - Draw to this
   BOOL                 fBackfaceCull - If TRUE then backface cull is on
   BOOL                 fIgnoreCutouts - If TRUE then just draw the entire quad, ignoring any cutouts
returns
   BOOL - TRUE if success
*/
BOOL CMeshGrid::DrawQuads (PCRenderSurface pSurf, BOOL fBackfaceCull, BOOL fIgnoreCutouts)
{
   DWORD i, j;
   PCPoint  pPoints, pNormals;
   PTEXTPOINT5 pTextures;
   PVERTEX pVertex;
   DWORD dwPointsIndex, dwNormalsIndex, dwTexturesIndex, dwVertexIndex;

   // should we draw the whole thing?
   if (fIgnoreCutouts || m_fTrivialAccept) {
      pPoints = pSurf->NewPoints (&dwPointsIndex, 4);
      pTextures = pSurf->NewTextures (&dwTexturesIndex, 4);
      pNormals = pSurf->NewNormals (TRUE, &dwNormalsIndex, 4);
      pVertex = pSurf->NewVertices (&dwVertexIndex, 4);
      if (!pPoints || !pVertex) // can deal with no textures or no normals
         return FALSE;

      // fill them in
      for (i = 0; i < 4; i++) {
         DWORD x,y;
         x = ((i == 1) || (i == 2)) ? 1 : 0;
         y = (i >= 2) ? 1 : 0;

         pPoints[i].Copy (&m_apPosn[y][x]);
         if (pNormals)
            pNormals[i].Copy (&m_apNorm[y][x]);
         if (pTextures) {
            pTextures[i].hv[0] = m_aText[y][x].h;
            pTextures[i].hv[1] = m_aText[y][x].v;
            pTextures[i].xyz[0] = m_apPosn[y][x].p[0];
            pTextures[i].xyz[1] = m_apPosn[y][x].p[1];
            pTextures[i].xyz[2] = m_apPosn[y][x].p[2];
         }
         pVertex[i].dwNormal = pNormals ? (dwNormalsIndex + i) : 0;
         pVertex[i].dwPoint = dwPointsIndex + i;
         pVertex[i].dwTexture = pTextures ? (dwTexturesIndex + i) : 0;
      }

      // BUGFIX - Apply texture mods
      if (pTextures)
         pSurf->ApplyTextureRotation (pTextures, 4);

      // draw it
      pSurf->NewQuad (dwVertexIndex+0, dwVertexIndex+1, dwVertexIndex+2, dwVertexIndex+3,
         fBackfaceCull);

      return TRUE;
   };

   if (m_fTrivialClip)
      return TRUE;

   // loop through all the wuads
   for (j = 0; j < m_listMGQUAD.Num(); j++) {
      PMGQUAD pq = (PMGQUAD) m_listMGQUAD.Get(j);

      pPoints = pSurf->NewPoints (&dwPointsIndex, pq->dwPoints);
      pTextures = pSurf->NewTextures (&dwTexturesIndex, pq->dwPoints);
      pNormals = pSurf->NewNormals (TRUE, &dwNormalsIndex, pq->dwPoints);
      pVertex = pSurf->NewVertices (&dwVertexIndex, pq->dwPoints);
      if (!pPoints || !pVertex) // can deal with no textures or no normals
         return FALSE;

      // fill them in
      for (i = 0; i < pq->dwPoints; i++) {
         pPoints[i].Copy (&pq->apPosn[i]);
         if (pNormals)
            pNormals[i].Copy (&pq->apNorm[i]);
         if (pTextures) {
            pTextures[i].hv[0] = pq->atText[i].h;
            pTextures[i].hv[1] = pq->atText[i].v;
            pTextures[i].xyz[0] = pq->apPosn[i].p[0];
            pTextures[i].xyz[1] = pq->apPosn[i].p[1];
            pTextures[i].xyz[2] = pq->apPosn[i].p[2];
         }
         pVertex[i].dwNormal = pNormals ? (dwNormalsIndex + i) : 0;
         pVertex[i].dwPoint = dwPointsIndex + i;
         pVertex[i].dwTexture = pTextures ? (dwTexturesIndex + i) : 0;
      }

      // BUGFIX - Apply texture mods
      if (pTextures)
         pSurf->ApplyTextureRotation (pTextures, pq->dwPoints);

      // draw it
      if (pq->dwPoints == 4)
         pSurf->NewQuad (dwVertexIndex+0, dwVertexIndex+1, dwVertexIndex+2, dwVertexIndex+3,
            fBackfaceCull);
      else if (pq->dwPoints == 3)
         pSurf->NewTriangle (dwVertexIndex+0, dwVertexIndex+1, dwVertexIndex+2, fBackfaceCull);

   }

   return TRUE;
}







/*************************************************************************************
CSplineSurface::Constructor and destructor
*/
CSplineSurface::CSplineSurface (void)
{
   m_fSplineDirty = TRUE;
   m_fCutoutsDirty = TRUE;
   m_fTextureDirty = TRUE;
   m_fLoopH = m_fLoopV = FALSE;
   m_dwControlH = m_dwControlV = 0;
   m_paControlPoints = NULL;
   m_padwSegCurveH = NULL;
   m_padwSegCurveV = NULL;
   m_dwMinDivideH = m_dwMinDivideV = 0;
   m_dwMaxDivideH = m_dwMaxDivideV = 3;
   m_fDetail = .1;
   m_dwDivideH = m_dwDivideV = 0;
   m_listCutoutClockwise.Init (sizeof(BOOL));
   m_listOverlayClockwise.Init (sizeof(BOOL));

   m_dwMeshH = m_dwMeshV= 0;
   m_paMesh = NULL;
   m_paMeshNormals = 0;

   m_aTextMatrix[0][0] = m_aTextMatrix[1][1] = 1;
   m_aTextMatrix[1][0] = m_aTextMatrix[0][1] = 0;
   m_paTexture = NULL;

   m_listListMG.Init (sizeof(PCListFixed));
}

CSplineSurface::~CSplineSurface (void)
{
   // free up the mesh grid objects
   DWORD i, j;
   for (j = 0; j < m_listListMG.Num(); j++) {
      PCListFixed pl = *((PCListFixed*)m_listListMG.Get(j));
      for (i = 0; i < pl->Num(); i++) {
         PCMeshGrid pmg = *((PCMeshGrid*) pl->Get(i));
         delete pmg;
      }

      delete pl;
   }
}

/*************************************************************************************
CSplineSurface::CutoutSet - Adds a cutout to the list. If a cutout already exists
with the given name then that one is replaced by the new setting.

inputs
   PWSTR          pszName - Name of the cutout.
   PTEXTUREPOINT  paPoints - Pointer to an array of TEXTUREPOINT defining the cutout.
                  h and v range from 0..1 to cover the area of the mesh. 0,0 is the
                  upper-left corner. This array should go clockwise to cutout within
                  the points, counterclockwise to do so outside.
   DWORD          dwNum - Number of points in paPoints. The points are considered part
                  of a loop.
   BOOL           fClockwise - If TRUE the points go in a clockwise direction and cutout
                  the area, else FALSE, couterclockwise and keep.
returns
   BOOL - TRUE if success
*/
BOOL CSplineSurface::CutoutSet (PWSTR pszName, PTEXTUREPOINT paPoints, DWORD dwNum, BOOL fClockwise)
{
   // try to find one with the same name
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_listCutoutNames.Num(); i++) {
      psz = (PWSTR) m_listCutoutNames.Get(i);
      if (_wcsicmp(psz, pszName))
         continue;

      // else, found a match
      m_listCutoutPoints.Set (i, paPoints, dwNum * sizeof(TEXTUREPOINT));
      m_listCutoutClockwise.Set (i, &fClockwise);

      m_fCutoutsDirty = TRUE;
      return TRUE;
   }

   // else, cant find, so add
   m_listCutoutNames.Add (pszName, (wcslen(pszName)+1) * sizeof(WCHAR));
   m_listCutoutPoints.Add (paPoints, dwNum * sizeof(TEXTUREPOINT));
   m_listCutoutClockwise.Add (&fClockwise);

   m_fCutoutsDirty = TRUE;
   return TRUE;
}

/*************************************************************************************
CSplineSurface::CutoutRemove - Removed the cutout with the given name.

inputs
   PWSTR       pszName - Cutout name
returns
   BOOL - TRUE if found and removed
*/
BOOL CSplineSurface::CutoutRemove (PWSTR pszName)
{
   // try to find one with the same name
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_listCutoutNames.Num(); i++) {
      psz = (PWSTR) m_listCutoutNames.Get(i);
      if (_wcsicmp(psz, pszName))
         continue;

      // else, found a match
      m_listCutoutPoints.Remove(i);
      m_listCutoutNames.Remove(i);
      m_listCutoutClockwise.Remove(i);

      m_fCutoutsDirty = TRUE;
      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CSplineSurface::CutoutEnum - Based on an index, from 0 .. CutoutNum()-1, this enumerates
the contents. DONT change the contents of the returned variables.

inputs
   DWORD          dwIndex - Index
   PWSTR          *ppszName - Name of the cutout.
   PTEXTUREPOINT  *ppaPoints - Pointer to an array of TEXTUREPOINT defining the cutout.
                  h and v range from 0..1 to cover the area of the mesh. 0,0 is the
                  upper-left corner. This array should go clockwise to cutout within
                  the points, counterclockwise to do so outside.
   DWORD          *pdwNum - Number of points in paPoints. The points are considered part
                  of a loop.
   BOOL           *pfClockwise - Filled with TRUE if cutout is clockwise direction, false if counter
returns
   BOOL - TRUE if success
*/
BOOL CSplineSurface::CutoutEnum (DWORD dwIndex, PWSTR *ppszName, PTEXTUREPOINT *ppaPoints, DWORD *pdwNum, BOOL *pfClockwise)
{
   *ppszName = (PWSTR) m_listCutoutNames.Get(dwIndex);
   if (!(*ppszName))
      return FALSE;
   *ppaPoints = (PTEXTUREPOINT) m_listCutoutPoints.Get(dwIndex);
   if (!(*ppaPoints))
      return FALSE;
   *pdwNum = (DWORD)m_listCutoutPoints.Size (dwIndex) / sizeof(TEXTUREPOINT);
   
   if (pfClockwise)
      *pfClockwise = *((BOOL*) m_listCutoutClockwise.Get(dwIndex));

   return TRUE;
}


/*************************************************************************************
CSplineSurface::CutoutNum - Returns the number of cutouts.

returns
   DWORD - Number
*/
DWORD CSplineSurface::CutoutNum (void)
{
   return m_listCutoutNames.Num();
}



/*************************************************************************************
CSplineSurface::OverlaySet - Adds a Overlay to the list. If a Overlay already exists
with the given name then that one is replaced by the new setting.

An overlay differes from a cutout in that a cutout actually removed the surface, while
an overlay creates its own surface so that several different colors (or textures) can
be applied to different regions of the same surface. Overlays are used for doing stuff
like waitnscotting,. walkways, etc.

inputs
   PWSTR          pszName - Name of the Overlay.
   PTEXTUREPOINT  paPoints - Pointer to an array of TEXTUREPOINT defining the Overlay.
                  h and v range from 0..1 to cover the area of the mesh. 0,0 is the
                  upper-left corner. This array should go clockwise ONLY.
   DWORD          dwNum - Number of points in paPoints. The points are considered part
                  of a loop.
   BOOL           fClockwise - If TRUE the points go in a clockwise direction and cutout
                  the area, else FALSE, couterclockwise and keep.
returns
   BOOL - TRUE if success
*/
BOOL CSplineSurface::OverlaySet (PWSTR pszName, PTEXTUREPOINT paPoints, DWORD dwNum, BOOL fClockwise)
{
   // try to find one with the same name
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_listOverlayNames.Num(); i++) {
      psz = (PWSTR) m_listOverlayNames.Get(i);
      if (_wcsicmp(psz, pszName))
         continue;

      // else, found a match
      m_listOverlayPoints.Set (i, paPoints, dwNum * sizeof(TEXTUREPOINT));
      m_listOverlayClockwise.Set (i, &fClockwise);

      m_fCutoutsDirty = TRUE;
      return TRUE;
   }

   // else, cant find, so add
   m_listOverlayNames.Add (pszName, (wcslen(pszName)+1) * sizeof(WCHAR));
   m_listOverlayPoints.Add (paPoints, dwNum * sizeof(TEXTUREPOINT));
   m_listOverlayClockwise.Add (&fClockwise);

   m_fCutoutsDirty = TRUE;
   return TRUE;
}

/*************************************************************************************
CSplineSurface::OverlayRemove - Removed the Overlay with the given name.

inputs
   PWSTR       pszName - Overlay name
returns
   BOOL - TRUE if found and removed
*/
BOOL CSplineSurface::OverlayRemove (PWSTR pszName)
{
   // try to find one with the same name
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_listOverlayNames.Num(); i++) {
      psz = (PWSTR) m_listOverlayNames.Get(i);
      if (_wcsicmp(psz, pszName))
         continue;

      // else, found a match
      m_listOverlayPoints.Remove(i);
      m_listOverlayNames.Remove(i);
      m_listOverlayClockwise.Remove (i);

      m_fCutoutsDirty = TRUE;
      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CSplineSurface::OverlayEnum - Based on an index, from 0 .. OverlayNum()-1, this enumerates
the contents. DONT change the contents of the returned variables.

inputs
   DWORD          dwIndex - Index
   PWSTR          *ppszName - Name of the Overlay.
   PTEXTUREPOINT  *ppaPoints - Pointer to an array of TEXTUREPOINT defining the Overlay.
                  h and v range from 0..1 to cover the area of the mesh. 0,0 is the
                  upper-left corner. This array should go clockwise to Overlay within
                  the points, counterclockwise to do so outside.
   DWORD          *pdwNum - Number of points in paPoints. The points are considered part
                  of a loop.
   BOOL           *pfClockwise - Filled with TRUE if cutout is clockwise direction, false if counter
returns
   BOOL - TRUE if success
*/
BOOL CSplineSurface::OverlayEnum (DWORD dwIndex, PWSTR *ppszName, PTEXTUREPOINT *ppaPoints, DWORD *pdwNum, BOOL *pfClockwise)
{
   *ppszName = (PWSTR) m_listOverlayNames.Get(dwIndex);
   if (!(*ppszName))
      return FALSE;
   *ppaPoints = (PTEXTUREPOINT) m_listOverlayPoints.Get(dwIndex);
   if (!(*ppaPoints))
      return FALSE;
   *pdwNum = (DWORD)m_listOverlayPoints.Size (dwIndex) / sizeof(TEXTUREPOINT);

   if (pfClockwise)
      *pfClockwise = *((BOOL*) m_listOverlayClockwise.Get(dwIndex));
   return TRUE;
}


/*************************************************************************************
CSplineSurface::OverlayNum - Returns the number of Overlays.

returns
   DWORD - Number
*/
DWORD CSplineSurface::OverlayNum (void)
{
   return m_listOverlayNames.Num();
}

/*************************************************************************************
CSplineSurface::ControlPointSet - Sets the control points, basically copying the information
passed in into internal data structures.

inputs
   BOOL        fLoopH - Set to true if loops in horizontal direction
   BOOL        fLoopV - Set to true if loops in vertical direction
   DWORD       dwControlH - Number of control points in H
   DWORD       dwControlV - Number of control points in V
   PCPoint     paPoints - Pointer to control pointed. Arrayed as [dwControlV][dwControlH]
   DWORD       *padwSegCurveH - Pointer to an array of dwControlH DWORDs continging
                  SEGCURVE_XXX descrbing the curve. This can also be (DWORD*) SEGCURVE_XXX
                  directly. If not looped in h then dwControlH-1 points.
   DWORD       *padwSegCurveV - Like padwSegCurveH. If not looped in v then dwControlV-1 points.
returns
   BOOL - TRUE if succeded
*/
BOOL CSplineSurface::ControlPointsSet (BOOL fLoopH, BOOL fLoopV, DWORD dwControlH, DWORD dwControlV,
   PCPoint paPoints, DWORD *padwSegCurveH, DWORD *padwSegCurveV)
{
   // set dirty
   m_fSplineDirty = TRUE;
   m_fCutoutsDirty = TRUE;
   m_fTextureDirty = TRUE;

   // copy over
   m_fLoopH = fLoopH;
   m_fLoopV = fLoopV;
   m_dwControlH = dwControlH;
   m_dwControlV = dwControlV;

   // points
   DWORD dwNeeded;
   dwNeeded = m_dwControlH * m_dwControlV * sizeof(CPoint);
   if (!m_memControlPoints.Required (dwNeeded))
      return FALSE;
   m_paControlPoints = (PCPoint) m_memControlPoints.p;
   memcpy (m_paControlPoints, paPoints, dwNeeded);

   // curve infor
   DWORD i;
   DWORD dwSegments;
   dwSegments = m_dwControlH - (fLoopH ? 0 : 1);
   dwNeeded = (dwSegments+1) * sizeof(DWORD);   // BUGFIX - One extra so that have enough room for looped
   if (!m_memSegCurveH.Required(dwNeeded))
      return FALSE;
   m_padwSegCurveH = (DWORD*) m_memSegCurveH.p;
   for (i = 0; i < dwSegments; i++)
      m_padwSegCurveH[i] = SegCurve (i, padwSegCurveH);

   dwSegments = m_dwControlV - (fLoopV ? 0 : 1);
   dwNeeded = (dwSegments+1) * sizeof(DWORD);   // BUGFIX - One extra so that have enough froom for looped
   if (!m_memSegCurveV.Required(dwNeeded))
      return FALSE;
   m_padwSegCurveV = (DWORD*) m_memSegCurveV.p;
   for (i = 0; i < dwSegments; i++)
      m_padwSegCurveV[i] = SegCurve (i, padwSegCurveV);

   return TRUE;
}


/*************************************************************************************
CSplineSurface::TextureInfoSet - Sets the texture rotation and scaling matrix.

inputs
   fp         m[2][2] - Scaling and rotation matrix
returns
   BOOL - TRUE if succeded
*/
BOOL CSplineSurface::TextureInfoSet (fp m[2][2])
{
   DWORD i, j;
   for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
      if (m[i][j] != m_aTextMatrix[i][j]) {
         m_aTextMatrix[i][j]= m[i][j];

         // set dirty
         m_fTextureDirty = TRUE;
         m_fCutoutsDirty = TRUE; // since store texture information in cutouts
      }
   }

   return TRUE;
}

/*************************************************************************************
CSplineSurface::DetailSet - Sets the detail information for drawing the surface.

inputs
   DWORD          dwMinDivideH - Minimum number of divides that will do horizontal
   DWORD          dwMaxDivideH - Maximum number of divides that will do horizontal
   DWORD          dwMinDivideV - Minimum number of divides that will do vertical
   DWORD          dwMaxDivideV - Maximum number of divides that will do vertical
   fp         fDetail - Divide until every spline distance is less than this
returns
   BOOL - TRUE if succeded
*/
BOOL CSplineSurface::DetailSet (DWORD dwMinDivideH, DWORD dwMaxDivideH, DWORD dwMinDivideV, DWORD dwMaxDivideV,
   fp fDetail)
{
   // set dirty
   m_fSplineDirty = TRUE;
   m_fCutoutsDirty = TRUE;
   m_fTextureDirty = TRUE;

   // set values
   m_dwMinDivideH = dwMinDivideH;
   m_dwMaxDivideH = dwMaxDivideH;
   m_dwMinDivideV = dwMinDivideV;
   m_dwMaxDivideV = dwMaxDivideV;
   m_fDetail = fDetail;

   return TRUE;
}

/*************************************************************************************
CSplineSurface::Render - Draws the surface to the CRenderSurface object.

inputs
   CRenderSurface*      pSurf - Surface object to render to
   BOOL                 fBackfaceCull - If TRUE then use backface culling, FALSE dont
   BOOL                 fWithCutout - If FALSE then dont remove cutouts from drawing,
                           if TRUE then do
   DWORD                dwOverlay - If... (Must have fWithCutout set to TRUE)
                           - 0 then draw the surface with cutouts removed, but
                             no overlays removed
                           - 1 then draw the surface with cutouts removed and
                             ALL overlays removed
                           - 2+, draw with cutouts removed, and surface limited to
                             the overlay numberd dwOverlay-1.
returns
   BOOL - TRUE if succeded
*/

BOOL CSplineSurface::Render (CRenderSurface *pSurf, BOOL fBackfaceCull, BOOL fWithCutout,
                             DWORD dwOverlay)
{
   // make sure everything is in order
   MakeSureSpline();

   // get the texture rotation matrix because may require a change in object
   if (pSurf->m_fNeedTextures) {
      fp m[2][2];
      pSurf->GetTextureRotation (m);
      TextureInfoSet (m);
   }

   BOOL fCutouts;
   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);
   fCutouts = fWithCutout && (CutoutNum() || OverlayNum());
   // If there aren't any cutouts in the object then dont do makesurecutouts
   if (fCutouts) {
      MakeSureCutouts();

      PCListFixed pl;
      pl = *((PCListFixed*) m_listListMG.Get(dwOverlay));
      DWORD x, y;
      PCMeshGrid pmg;
      for (y = 0; y < dwNormV; y++) for (x = 0; x < dwNormH; x++) {
         pmg = *((PCMeshGrid*) pl->Get(MESHG(x,y)));
         pmg->DrawQuads (pSurf, fBackfaceCull, !fWithCutout);
      }
      return TRUE;
   }

   // reset the texture map if surface has moved AND texture maps are requested
   PTEXTPOINT5 paTexture;
   DWORD    dwTextureIndex;
   DWORD    dwTextV, dwTextH;
   dwTextV = m_dwMeshV + (m_fLoopV ? 1 : 0);
   dwTextH = m_dwMeshH + (m_fLoopH ? 1 : 0);
   paTexture = pSurf->NewTextures (&dwTextureIndex, dwTextV * dwTextH);


   // add the points en-masse
   PCPoint  paPoints;
   DWORD    dwPointIndex;
   paPoints = pSurf->NewPoints (&dwPointIndex, m_dwMeshV * m_dwMeshH);
   if (!paPoints)
      return FALSE;
   memcpy (paPoints, m_paMesh, m_dwMeshV * m_dwMeshH * sizeof(CPoint));

   // add all the normals en-masse
   PCPoint  paNormals;
   DWORD    dwNormalIndex;
   paNormals = pSurf->NewNormals (TRUE, &dwNormalIndex, dwNormV * dwNormH * 4);
   if (paNormals) {
      memcpy (paNormals, m_paMeshNormals, dwNormV * dwNormH * 4 * sizeof(CPoint));
   }

   // add all the textures en-masse
   if (paTexture) {
      MakeSureTexture();
      memcpy (paTexture, m_paTexture, dwTextV * dwTextH * sizeof(TEXTPOINT5));
   }


   // loop
   DWORD x, y, i;
   for (y = 0; y < m_dwMeshV; y++) {
      if (!m_fLoopV && ((y+1) >= m_dwMeshV))
         break;

      for (x = 0; x < m_dwMeshH; x++) {
         // dont do next one unless loop
         if (!m_fLoopH && ((x+1) >= m_dwMeshH))
            break;

         // what are the points, normals, and textures for the 4 corners
         DWORD adwPoint[4], adwNorm[4], adwText[4];
         adwPoint[0] = dwPointIndex + MESH(x, y);
         adwPoint[1] = dwPointIndex + MESH((x+1) % m_dwMeshH, y);
         adwPoint[2] = dwPointIndex + MESH((x+1) % m_dwMeshH, (y+1) % m_dwMeshV);
         adwPoint[3] = dwPointIndex + MESH(x, (y+1) % m_dwMeshV);

         if (paNormals) {
            adwNorm[0] = dwNormalIndex + MESHN(x,y,0);
            adwNorm[1] = dwNormalIndex + MESHN(x,y,1);
            adwNorm[2] = dwNormalIndex + MESHN(x,y,2);
            adwNorm[3] = dwNormalIndex + MESHN(x,y,3);
         }
         else {
            memset (adwNorm, 0, sizeof(adwNorm));
         }

         if (paTexture) {
            adwText[0] = dwTextureIndex + MESHT(x,y);
            adwText[1] = dwTextureIndex + MESHT(x+1,y);
            adwText[2] = dwTextureIndex + MESHT(x+1,y+1);
            adwText[3] = dwTextureIndex + MESHT(x,y+1);
         }
         else {
            memset (adwText, 0, sizeof(adwText));
         }

         // new verticies
         PVERTEX pv;
         DWORD dwVertIndex;
         pv = pSurf->NewVertices (&dwVertIndex, 4);
         if (!pv)
            return FALSE;
         for (i = 0; i < 4; i++) {
            pv[i].dwNormal = adwNorm[i];
            pv[i].dwPoint = adwPoint[i];
            pv[i].dwTexture = adwText[i];
         }

         // quadrilaterals
         pSurf->NewQuad (dwVertIndex+0,dwVertIndex+1,dwVertIndex+2,dwVertIndex+3,
            fBackfaceCull);
      }
   }

   // done
   return TRUE;
}


/**********************************************************************************
CSplineSurface::QueryBoundingBox - Standard API
*/
void CSplineSurface::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   // make sure everything is in order
   MakeSureSpline();

   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);

   DWORD i;
   for (i = 0; i < m_dwMeshV * m_dwMeshH; i++) {
      if (i) {
         pCorner1->Min (&m_paMesh[i]);
         pCorner2->Max (&m_paMesh[i]);
      }
      else {
         pCorner1->Copy (&m_paMesh[i]);
         pCorner2->Copy (&m_paMesh[i]);
      }
   } // i

}

/*************************************************************************************
CSplineSurface::MakeSureTexture - Call this to make sure the derived texture information is
correct.
*/
void CSplineSurface::MakeSureTexture(void)
{
   // need to have the splines correct first
   MakeSureSpline();

   // if the rotation matrix is not the same as the last time called
   // then need to reapply

   if (!m_fTextureDirty)
      return;  // nothing to do
   m_fTextureDirty = FALSE;

   // how big?
   DWORD    dwTextV, dwTextH;
   dwTextV = m_dwMeshV + (m_fLoopV ? 1 : 0);
   dwTextH = m_dwMeshH + (m_fLoopH ? 1 : 0);

   // make sure have enough
   DWORD dwNeeded;
   dwNeeded = dwTextV * dwTextH * sizeof(TEXTPOINT5);
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   if (!m_memTexture.Required(dwNeeded)) {
   	MALLOCOPT_RESTORE;
      m_paTexture = NULL;
      return;
   }
	MALLOCOPT_RESTORE;
   m_paTexture  = (PTEXTPOINT5) m_memTexture.p;

   // fill in the h component first
   fp fTotal, fInc;
   fInc = 0;
   DWORD x, y;
   for (x = 0; x < dwTextH; x++) {
      // find the average distance between the left and rigth points,
      // exept the first row, which is 0
      fTotal = 0;
      if (x) for (y = 0; y < dwTextV; y++)
         fTotal += DistancePointToPoint (&m_paMesh[MESH(x-1,y%m_dwMeshV)],
            &m_paMesh[MESH(x%m_dwMeshH,y%m_dwMeshV)]);
      fTotal /= dwTextV;
      fInc += fTotal;

      // write the total in
      for (y = 0; y < dwTextV; y++)
         m_paTexture[MESHT(x,y)].hv[0] = fInc;

   }

   // fill in the v component
   fInc = 0;
   // to the v starting from the bottom and working up so that bricks start
   // at the floor
   for (y = dwTextV-1; y < dwTextV; y--) {
      // find the average distance between the top and bottom points,
      // exept the first row, which is 0
      fTotal = 0;
      if (y < (dwTextV-1)) for (x = 0; x < dwTextH; x++)
         fTotal += DistancePointToPoint (&m_paMesh[MESH(x%m_dwMeshH,(y+1)%m_dwMeshV)],
            &m_paMesh[MESH(x%m_dwMeshH,y%m_dwMeshV)]);
      fTotal /= dwTextH;
      fInc -= fTotal;

      // write the total in
      for (x = 0; x < dwTextH; x++) {
         m_paTexture[MESHT(x,y)].hv[1] = fInc;

         // while at it fill in other points
         m_paTexture[MESHT(x,y)].xyz[0] = m_paMesh[MESH(x%m_dwMeshH,y%m_dwMeshV)].p[0];
         m_paTexture[MESHT(x,y)].xyz[1] = m_paMesh[MESH(x%m_dwMeshH,y%m_dwMeshV)].p[1];
         m_paTexture[MESHT(x,y)].xyz[2] = m_paMesh[MESH(x%m_dwMeshH,y%m_dwMeshV)].p[2];
      }

   }

   // BUGFIX - No longer call ApplyTextureRotation() since is handled
   // elsewhere, in final render, and calling this causes a double rotation/scaling
   // ApplyTextureRotation (m_paTexture, dwTextH * dwTextV, m_aTextMatrix, NULL);
}


/***************************************************************************************
CSplineSurface::CutoutOfGrid - Internalfunction that takes a pointer to an array of
PTEXTUREPOINT and cuts them out of the grid.

inputs
   PTEXTUREPOINT   pt - Cutout. Go clockwise to cutout, counterclockwise to keep the
                  area within.
   DWORD       dwNum - Number of points in papCutout
   BOOL        fReverse - If TRUE, then reverse (internally within the function) the
               order of drawing.
   PCListFixed    plMG - List initialied with PCMeshGrid objects. dwNormH x dwNormV
   BOOL        fClockwise - TRUE if this is clockwise, FALSE if counterclockwise
returns
   BOOl - TRUE if success
*/
BOOL CSplineSurface::CutoutOfGrid (PTEXTUREPOINT pt, DWORD dwNum, BOOL fClockwise, BOOL fReverse, PCListFixed plMG)
{
   DWORD x, y, dwNormV, dwNormH, j;
   PCMeshGrid pmg;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);

   if (dwNum <= 2)
      return TRUE;   // cant cut this out since nothing to cutout

   // BUGFIX - If any points are close to the edge but not quite then make them the edge
   for (x = 0; x < dwNum; x++) {
      if (fabs(pt[x].h - 0.0) < EPSILON)
         pt[x].h = 0;
      if (fabs(pt[x].h - 1.0) < EPSILON)
         pt[x].h = 1;
      if (fabs(pt[x].v - 0.0) < EPSILON)
         pt[x].v = 0;
      if (fabs(pt[x].v - 1.0) < EPSILON)
         pt[x].v = 1;
   }

   BOOL fInverted;
   fInverted = !fClockwise;
   if (fReverse)
      fInverted = !fInverted;
   if (fInverted) {
      // reverse direction. Therefore, go through ALL the meshgrid
      // and decrement their count
      for (y = 0; y < dwNormV; y++) for (x = 0; x < dwNormH; x++) {
         pmg = *((PCMeshGrid*) plMG->Get(MESHG(x,y)));
         pmg->TickIncrement (-1);
      }
   }  // normal test

   // loop through all the lines
   for (j = 0; j < dwNum; j++) {
      PTEXTUREPOINT pt1, pt2;
      pt1 = pt + (fReverse ? (dwNum - j - 1) : j);
      pt2 = pt + (fReverse ? ((dwNum*2 - j - 2) % dwNum) : ((j+1) % dwNum));

      // minimum and maximum y, and maximum x
      fp fTPMinY, fTPMaxY, fTPMinX;
      fTPMinY = min(pt1->v, pt2->v);
      fTPMaxY = max(pt1->v, pt2->v);
      fTPMinX = min(pt1->h, pt2->h);

      // loop through all y grid elements. If the line crosses over the
      // grid then call all the meshgrid objects in the row and pass in the line
      for (y = 0; y < dwNormV; y++) {
         fp fYMin = (fp) y / (fp)dwNormV;
         fp fYMax = (fp)(y+1) / (fp)dwNormV;

         if ((fTPMinY >= fYMax) || (fTPMaxY <= fYMin))
            continue;   // out of range

         // through all x's now
         for (x = 0; x < dwNormH; x++) {
            // if to the right of the grid then don't waste time
            if (fTPMinX >= (fp)(x+1) / (fp)dwNormH)
               continue;

            // else, pass it in
            pmg = *((PCMeshGrid*) plMG->Get(MESHG(x,y)));
            pmg->IntersectLineWithGrid (pt1, pt2);
         }
      }
   }

   return TRUE;
}

/***************************************************************************************
CSplineSurface::MakeSureCutouts - Builds the cutouts information if its
not already built.
*/
void CSplineSurface::MakeSureCutouts(void)
{
   MALLOCOPT_INIT;
   MakeSureSpline();
   MakeSureTexture();

   if (!m_fCutoutsDirty)
      return;
   m_fCutoutsDirty = FALSE;

   MALLOCOPT_OKTOMALLOC;

   // allocate enough cutout objects
   DWORD dwNormH, dwNormV, dwTotal;
   PCMeshGrid pmg;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);
   dwTotal = dwNormH * dwNormV;

   // loop through all the overlays.
   // Overlay 0 is the surface with cutouts, but no overlays removed
   // overlay 1 is the surface with cutouts, and all overlays removed
   // overlays 2+ is the surface with cutouts, limited to overlay# - 2.
   DWORD dwOver;
   for (dwOver = 0; dwOver < (m_listOverlayPoints.Num()+2); dwOver++) {
      PCListFixed plMG;

      // if this list isn't allocated then do so
      if (m_listListMG.Num() <= dwOver) {
         plMG = new CListFixed;
         if (!plMG) {
            m_fCutoutsDirty = TRUE;
            MALLOCOPT_RESTORE;
            return;
         }
         plMG->Init (sizeof (PCMeshGrid));
         m_listListMG.Add (&plMG);
      }
      else
         plMG = *((PCListFixed*) m_listListMG.Get(dwOver));

      plMG->Required (dwTotal);
      while (plMG->Num() < dwTotal) {
         pmg = new CMeshGrid;
         plMG->Add (&pmg);
      }

      DWORD    dwTextV, dwTextH;
      dwTextV = m_dwMeshV + (m_fLoopV ? 1 : 0);
      dwTextH = m_dwMeshH + (m_fLoopH ? 1 : 0);

      // loop through each mesh grid and initialize it
      DWORD x,y;
      for (y = 0; y < dwNormV; y++) for (x = 0; x < dwNormH; x++) {
         pmg = *((PCMeshGrid*) plMG->Get(MESHG(x,y)));

         pmg->Init ((fp) x / (fp)dwNormH, (fp)(x+1) / (fp)dwNormH,
            (fp) y / (fp)dwNormV, (fp)(y+1) / (fp)dwNormV,
            m_paMesh + MESH(x,y), m_paMesh + MESH((x+1)%m_dwMeshH,y),
            m_paMesh + MESH(x,(y+1)%m_dwMeshV), m_paMesh + MESH((x+1)%m_dwMeshH,(y+1)%m_dwMeshV),
            m_paMeshNormals + MESHN(x,y,0), m_paMeshNormals + MESHN(x,y,1),
            m_paMeshNormals + MESHN(x,y,3), m_paMeshNormals + MESHN(x,y,2),
            (PTEXTUREPOINT)(m_paTexture + MESHT(x,y))->hv,
            (PTEXTUREPOINT)(m_paTexture + MESHT(x+1,y))->hv,
            (PTEXTUREPOINT)(m_paTexture + MESHT(x,y+1))->hv,
            (PTEXTUREPOINT)(m_paTexture + MESHT(x+1,y+1))->hv);
      }


      // loop through all the cutouts
      DWORD i;
      PTEXTUREPOINT pt;
      DWORD dwNum;
      for (i = 0; i < m_listCutoutPoints.Num(); i++) {
         pt = (PTEXTUREPOINT) m_listCutoutPoints.Get(i);
         dwNum = (DWORD)m_listCutoutPoints.Size (i) / sizeof(TEXTUREPOINT);

         CutoutOfGrid (pt, dwNum, *((BOOL*)m_listCutoutClockwise.Get(i)), FALSE, plMG);

      } // loop through all cutouts

      // Do overlays. If dwOver == 0, this is a special one without any overlays
      if (dwOver) {
         DWORD dwCur = dwOver - 2;

         // keep this one
         if (dwOver >= 2) {
            pt = (PTEXTUREPOINT) m_listOverlayPoints.Get(dwCur);
            dwNum = (DWORD)m_listOverlayPoints.Size (dwCur) / sizeof(TEXTUREPOINT);
            CutoutOfGrid (pt, dwNum, *((BOOL*)m_listOverlayClockwise.Get(dwCur)), TRUE, plMG);
         }

         // remove any overlays from this occuring after it. That way,
         // the last overlay on the list will overlap and cover any overlays
         // below it
         DWORD k;
         for (k = dwCur+1; k < m_listOverlayPoints.Num(); k++) {
            pt = (PTEXTUREPOINT) m_listOverlayPoints.Get(k);
            dwNum =(DWORD) m_listOverlayPoints.Size (k) / sizeof(TEXTUREPOINT);
            CutoutOfGrid (pt, dwNum, *((BOOL*)m_listOverlayClockwise.Get(k)), FALSE, plMG);
         }

      }

      // now that all the grids have lines, call them all again and make
      // them create polygons from them
      for (y = 0; y < dwNormV; y++) for (x = 0; x < dwNormH; x++) {
         pmg = *((PCMeshGrid*) plMG->Get(MESHG(x,y)));
         pmg->GenerateQuads ();
      }

   }  // dwOver
   // done

   MALLOCOPT_RESTORE;
}

/***************************************************************************************
CSplineSurface::MakeSureSpline - Rebuilds the spline from the information set in
   ControlPointsSet().
*/
void CSplineSurface::MakeSureSpline(void)
{
   CListFixed  listH, listV;
   DWORD i, j;
   PCSpline pSpline;
   listH.Init (sizeof(PCSpline));
   listV.Init (sizeof(PCSpline));
   CMem  memTemp;

   // deal with dirty flags
   if (!m_fSplineDirty)
      return;
   m_fSplineDirty = FALSE;
   m_fCutoutsDirty = TRUE;
   m_fTextureDirty = TRUE;

   // how big?
   DWORD    dwTextV, dwTextH;
   dwTextV = m_dwControlV + (m_fLoopV ? 1 : 0);
   dwTextH = m_dwControlH + (m_fLoopH ? 1 : 0);

   // find out what the longest distance is horizontal and vertical
   // fill in the h component first
   fp fFound, fLongestH, fLongestV;
   fLongestH = fLongestV = 0;
   DWORD x, y;
   for (x = 0; x < dwTextH; x++) {
      if (x) for (y = 0; y < dwTextV; y++) {
         fFound = DistancePointToPoint (&m_paControlPoints[MESHC(x-1,y%m_dwControlV)],
            &m_paControlPoints[MESHC(x%m_dwControlH,y%m_dwControlV)]);
         fLongestH = max(fLongestH, fFound);
      }
   }

   // fill in the v component
   for (y = 0; y < dwTextV; y++) {
      if (y) for (x = 0; x < dwTextH; x++) {
         fFound = DistancePointToPoint (&m_paControlPoints[MESHC(x%m_dwControlH,y-1)],
            &m_paControlPoints[MESHC(x%m_dwControlH,y%m_dwControlV)]);
         fLongestV = max(fLongestV, fFound);
      }
   }

   // how much to subdivide
   DWORD dwDivideH, dwDivideV;
   for (dwDivideH = m_dwMinDivideH;
      (dwDivideH < m_dwMaxDivideH) && (fLongestH > m_fDetail);
      dwDivideH++, fLongestH /= 2);
   for (dwDivideV = m_dwMinDivideV;
      (dwDivideV < m_dwMaxDivideV) && (fLongestV > m_fDetail);
      dwDivideV++, fLongestV /= 2);

   // if they're all linear then use minimum number of subdivides
   DWORD dwSegments;
   dwSegments = m_dwControlH - (m_fLoopH ? 0 : 1);
   for (i = 0; i < dwSegments; i++)
      if (SegCurve(i, m_padwSegCurveH) != SEGCURVE_LINEAR)
         break;
   if (i >= dwSegments)
      dwDivideH = m_dwMinDivideH;
   dwSegments = m_dwControlV - (m_fLoopV ? 0 : 1);
   for (i = 0; i < dwSegments; i++)
      if (SegCurve(i, m_padwSegCurveV) != SEGCURVE_LINEAR)
         break;
   if (i >= dwSegments)
      dwDivideV = m_dwMinDivideV;
   m_dwDivideH = dwDivideH;
   m_dwDivideV = dwDivideV;



   // do horizontal splines all the way down
   for (i = 0; i < m_dwControlV; i++) {
      pSpline = new CSpline;
      if (!pSpline)
         goto done;

      if (!pSpline->Init (m_fLoopH, m_dwControlH, m_paControlPoints + MESHC(0,i),
         NULL, m_padwSegCurveH, dwDivideH, dwDivideH))
         goto done;

      // remember number of points
      m_dwMeshH = pSpline->QueryNodes();

      // add this
      listH.Add (&pSpline);
   }

   // go over the existing splines and bring together points to do the vertical splines
   DWORD dwNeeded;
   PCPoint pTemp;
   dwNeeded = m_dwControlV * sizeof(CPoint);
   if (!memTemp.Required(dwNeeded))
      goto done;
   pTemp = (PCPoint) memTemp.p;

   DWORD dwHNum;
   dwHNum = listH.Num();
   for (i = 0; i < m_dwMeshH; i++) {
      // put together points
      for (j = 0; j < dwHNum; j++) {
         pSpline = *((PCSpline*) listH.Get(j));
         pTemp[j].Copy (pSpline->LocationGet (i));
      }

      // new spline out of this
      pSpline = new CSpline;
      if (!pSpline)
         goto done;

      if (!pSpline->Init (m_fLoopV, dwHNum, pTemp, NULL, m_padwSegCurveV, dwDivideV, dwDivideV))
         goto done;

      // remember number of points
      m_dwMeshV = pSpline->QueryNodes();

      // add this
      listV.Add (&pSpline);
   }

   // allocate enough memory for all the points
   if (!m_memMesh.Required (m_dwMeshH * m_dwMeshV * sizeof(CPoint)))
      goto done;
   m_paMesh = (PCPoint) m_memMesh.p;

   // allocate memory for normals
   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);
   if (!m_memMeshNormals.Required (dwNormH * dwNormV * 4 * sizeof(CPoint)))
      goto done;
   m_paMeshNormals = (PCPoint) m_memMeshNormals.p;

   // fill in the points
   for (x = 0; x < m_dwMeshH; x++) {
      pSpline = *((PCSpline*) listV.Get(x));

      for (y = 0; y < m_dwMeshV; y++) {
         m_paMesh[MESH(x,y)].Copy (pSpline->LocationGet (y));
      }
   }
   
   // calculate the normals around the loop
   // first, go through the vertical splines and get their tangents,
   // storing them in the normal memory
   PCSpline pLeft,pRight;
   for (x = 0; x < dwNormH; x++) {
      pLeft = *((PCSpline*) listV.Get(x));
      pRight = *((PCSpline*) listV.Get((x+1) % m_dwMeshH));

      for (y = 0; y < dwNormV; y++) {
         PCPoint pTanUpDown;

         // two normals on the left of the grid
         pTanUpDown = pLeft->TangentGet (y, TRUE);
         m_paMeshNormals[MESHN(x,y,0)].Copy (pTanUpDown);
         pTanUpDown = pRight->TangentGet (y, TRUE);
         m_paMeshNormals[MESHN(x,y,1)].Copy (pTanUpDown);

         // two normals on the right of the grid
         pTanUpDown = pRight->TangentGet ((y+1) % m_dwMeshV, FALSE);
         m_paMeshNormals[MESHN(x,y,2)].Copy (pTanUpDown);
         pTanUpDown = pLeft->TangentGet ((y+1) % m_dwMeshV, FALSE);
         m_paMeshNormals[MESHN(x,y,3)].Copy (pTanUpDown);
      }
   }

   // how many times divide?
   DWORD dwDivCount;
   dwDivCount = 1 << dwDivideV;

   // loop over the horizontal splines and get their tangents
   PCSpline pAbove, pBelow;
   for (y = 0; y < m_dwControlV; y++) {
      pAbove = *((PCSpline*) listH.Get(y));
      pBelow = *((PCSpline*) listH.Get((y+1) % m_dwControlV));

      // loop across these points
      for (x = 0; x < dwNormH; x++) {
         PCPoint pTanUL, pTanUR, pTanLL, pTanLR;
         pTanUL = pAbove->TangentGet (x, TRUE);
         pTanLL = pBelow->TangentGet (x, TRUE);
         pTanUR = pAbove->TangentGet ((x+1)%m_dwMeshH, FALSE);
         pTanLR = pBelow->TangentGet ((x+1)%m_dwMeshH, FALSE);


         // loop over divide doing averaging...
         CPoint pL, pR;
         CPoint N;
         for (i = 0; i < dwDivCount; i++) {
            // where to put it
            j = y * dwDivCount + i;
            if (j >= dwNormV)
               break;   // beyond the end of the mesh

            pL.Subtract (pTanLL, pTanUL);
            pL.Scale ((fp)i / (fp) dwDivCount);
            pL.Add (pTanUL);

            pR.Subtract (pTanLR, pTanUR);
            pR.Scale ((fp)i / (fp) dwDivCount);
            pR.Add (pTanUR);

            // cross product and normalize
            PCPoint pn;
            pn = m_paMeshNormals + MESHN(x, j, 0);
            N.CrossProd (pn, &pL);
            N.Normalize ();
            pn->Copy (&N);

            pn = m_paMeshNormals + MESHN(x, j, 1);
            N.CrossProd (pn, &pR);
            N.Normalize ();
            pn->Copy (&N);

            // two bottom ones
            pL.Subtract (pTanLL, pTanUL);
            pL.Scale ((fp)(i+1) / (fp) dwDivCount);
            pL.Add (pTanUL);

            pR.Subtract (pTanLR, pTanUR);
            pR.Scale ((fp)(i+1) / (fp) dwDivCount);
            pR.Add (pTanUR);

            pn = m_paMeshNormals + MESHN(x, j, 2);
            N.CrossProd (pn, &pR);
            N.Normalize ();
            pn->Copy (&N);

            pn = m_paMeshNormals + MESHN(x, j, 3);
            N.CrossProd (pn, &pL);
            N.Normalize ();
            pn->Copy (&N);

         }  // over divcount

      } // over meshh
   } // cover controlv

   // OPTIMIZATION - At some point do intelligent optimization where looks for
   // duplicate tangents (and hence normals) and notes this (for drawing)
   // and doesnt bother recalculating


done:
   // free all the memory
   for (i = 0; i < listH.Num(); i++) {
      pSpline = *((PCSpline*) listH.Get(i));
      delete pSpline;
   }
   for (i = 0; i < listV.Num(); i++) {
      pSpline = *((PCSpline*) listV.Get(i));
      delete pSpline;
   }
}



// OPTIMIZATION - Have a function that finds duplicate normals on corners, and combines
// verticies, so as to speed up drawing. Basically, generates list of normals,
// and vertex information so can pass directly into CRenderSurface


static PWSTR gszLoopH = L"m_fLoopH";
static PWSTR gszLoopV = L"m_fLoopV";
static PWSTR gszControlH = L"m_dwControlH";
static PWSTR gszControlV = L"m_dwControlV";
static PWSTR gszControlPoints = L"m_dwControlPoints%d";
static PWSTR gszSegCurveH = L"m_padwSegCurveH%d";
static PWSTR gszSegCurveV = L"m_padwSegCurveV%d";
static PWSTR gszMinDivideH = L"m_dwMinDivideH";
static PWSTR gszMaxDivideH = L"m_dwMaxDivideH";
static PWSTR gszMinDivideV = L"m_dwMinDivideV";
static PWSTR gszMaxDivideV = L"m_dwMaxDivideV";
static PWSTR gszDetail = L"m_fDetail";
static PWSTR gszTextMatrix = L"m_aTextMatrix%d%d";
static PWSTR gszCutout = L"Cutout";
static PWSTR gszOverlay = L"Overlay";
static PWSTR gszName = L"Name";
static PWSTR gszNumber = L"Number";
static PWSTR gszCutoutPoint = L"CutoutPoint%d";
static PWSTR gszClockwise = L"Clockwise";

/*******************************************************************************
CSplineSurface::MMLTo - Returns a PCMMLNode2 with information about the spline
so that it can be copied to the clipboard or saved to disk.

inputs
   none
returns
   PCMMLNode2 - node
*/
PCMMLNode2 CSplineSurface::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   WCHAR szTemp[64];
   DWORD i;
   if (!pNode)
      return NULL;
   pNode->NameSet (L"CSplineSurface");

   MMLValueSet (pNode, gszLoopH, (int) m_fLoopH);
   MMLValueSet (pNode, gszLoopV, (int) m_fLoopV);
   MMLValueSet (pNode, gszControlH, (int) m_dwControlH);
   MMLValueSet (pNode, gszControlV, (int) m_dwControlV);
   for (i = 0; i < m_dwControlH * m_dwControlV; i++) {
      swprintf (szTemp, gszControlPoints, (int) i);
      MMLValueSet (pNode, szTemp, &m_paControlPoints[i]);
   }
   for (i = 0; i < m_dwControlH; i++) {
      swprintf (szTemp, gszSegCurveH, (int) i);
      MMLValueSet (pNode, szTemp, (int) SegCurve(i, m_padwSegCurveH));
   }
   for (i = 0; i < m_dwControlV; i++) {
      swprintf (szTemp, gszSegCurveV, (int) i);
      MMLValueSet (pNode, szTemp, (int) SegCurve(i, m_padwSegCurveV));
   }
   MMLValueSet (pNode, gszMinDivideH, (int) m_dwMinDivideH);
   MMLValueSet (pNode, gszMaxDivideH, (int) m_dwMaxDivideH);
   MMLValueSet (pNode, gszMinDivideV, (int) m_dwMinDivideV);
   MMLValueSet (pNode, gszMaxDivideV, (int) m_dwMaxDivideV);
   MMLValueSet (pNode, gszDetail, m_fDetail);

   DWORD x,y;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
      swprintf (szTemp, gszTextMatrix, (int) x, (int) y);
      MMLValueSet (pNode, szTemp, m_aTextMatrix[x][y]);
   }


   for (i = 0; i < m_listCutoutNames.Num(); i++) {
      PCMMLNode2 pCut = new CMMLNode2;
      if (!pCut)
         continue;
      pCut->NameSet (gszCutout);

      PWSTR psz;
      psz = (PWSTR) m_listCutoutNames.Get(i);
      MMLValueSet (pCut, gszName, psz);

      DWORD dwNum;
      PTEXTUREPOINT ptp;
      dwNum = (DWORD)m_listCutoutPoints.Size(i) / sizeof(TEXTUREPOINT);
      ptp = (PTEXTUREPOINT) m_listCutoutPoints.Get(i);
      MMLValueSet (pCut, gszNumber, (int) dwNum);

      for (x = 0; x < dwNum; x++) {
         swprintf (szTemp, gszCutoutPoint, (int) x);
         MMLValueSet (pCut, szTemp, ptp + x);
      }

      MMLValueSet (pCut, gszClockwise, (int) *((BOOL*) m_listCutoutClockwise.Get(i)));

      pNode->ContentAdd (pCut);
   }
   
   for (i = 0; i < m_listOverlayNames.Num(); i++) {
      PCMMLNode2 pCut = new CMMLNode2;
      if (!pCut)
         continue;
      pCut->NameSet (gszOverlay);

      PWSTR psz;
      psz = (PWSTR) m_listOverlayNames.Get(i);
      MMLValueSet (pCut, gszName, psz);

      DWORD dwNum;
      PTEXTUREPOINT ptp;
      dwNum = (DWORD)m_listOverlayPoints.Size(i) / sizeof(TEXTUREPOINT);
      ptp = (PTEXTUREPOINT) m_listOverlayPoints.Get(i);
      MMLValueSet (pCut, gszNumber, (int) dwNum);

      for (x = 0; x < dwNum; x++) {
         swprintf (szTemp, gszCutoutPoint, (int) x);
         MMLValueSet (pCut, szTemp, ptp + x);
      }

      MMLValueSet (pCut, gszClockwise, (int) *((BOOL*) m_listOverlayClockwise.Get(i)));

      pNode->ContentAdd (pCut);
   }
   
   return pNode;
}


/*******************************************************************************
CSplineSurface::MMLFrom - Reads all the parameters for the spline and initializes
member variables.

inputs
   PCMLNode    pNode - Node to read from
returns
   BOOL - TRUE if successful
*/
BOOL CSplineSurface::MMLFrom (PCMMLNode2 pNode)
{
   BOOL fLoopH, fLoopV;
   DWORD dwControlH, dwControlV;

   fLoopH = (BOOL) MMLValueGetInt (pNode, gszLoopH, FALSE);
   fLoopV = (BOOL) MMLValueGetInt (pNode, gszLoopV, FALSE);
   dwControlH = (int) MMLValueGetInt (pNode, gszControlH, 0);
   dwControlV = (int) MMLValueGetInt (pNode, gszControlV, 0);
   
   DWORD dwTotal;
   dwTotal = dwControlH * dwControlV;
   if (!dwTotal)
      return FALSE;
   CMem memPoints, memSegCurveH, memSegCurveV;
   if (!memPoints.Required (dwTotal * sizeof(CPoint)))
      return FALSE;
   if (!memSegCurveH.Required (dwControlH * sizeof(DWORD)))
      return FALSE;
   if (!memSegCurveV.Required (dwControlV * sizeof(DWORD)))
      return FALSE;
   PCPoint pPoints;
   DWORD *padwSegCurveH, *padwSegCurveV;
   pPoints = (PCPoint) memPoints.p;
   padwSegCurveH = (DWORD*) memSegCurveH.p;
   padwSegCurveV = (DWORD*) memSegCurveV.p;

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < dwTotal; i++) {
      swprintf (szTemp, gszControlPoints, (int) i);
      pPoints[i].Zero();
      MMLValueGetPoint (pNode, szTemp, &pPoints[i]);
   }
   for (i = 0; i < dwControlH; i++) {
      swprintf (szTemp, gszSegCurveH, (int) i);
      padwSegCurveH[i] = (DWORD) MMLValueGetInt (pNode, szTemp, SEGCURVE_LINEAR);
   }
   for (i = 0; i < dwControlV; i++) {
      swprintf (szTemp, gszSegCurveV, (int) i);
      padwSegCurveV[i] = (DWORD) MMLValueGetInt (pNode, szTemp, SEGCURVE_LINEAR);
   }

   // set the control points
   if (!ControlPointsSet (fLoopH, fLoopV, dwControlH, dwControlV, pPoints,
      padwSegCurveH, padwSegCurveV))
      return FALSE;

   DWORD dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV;
   fp fDetail;
   dwMinDivideH = (DWORD) MMLValueGetInt (pNode, gszMinDivideH, 0);
   dwMaxDivideH = (DWORD) MMLValueGetInt (pNode, gszMaxDivideH, 3);
   dwMinDivideV = (DWORD) MMLValueGetInt (pNode, gszMinDivideV, 0);
   dwMaxDivideV = (DWORD) MMLValueGetInt (pNode, gszMaxDivideV, 3);
   fDetail = MMLValueGetDouble (pNode, gszDetail, .1);

   // set the divide detail
   DetailSet (dwMinDivideH, dwMaxDivideH, dwMinDivideV, dwMaxDivideV, fDetail);

   DWORD x,y;
   fp aTextMatrix[2][2];
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
      swprintf (szTemp, gszTextMatrix, (int) x, (int) y);
      aTextMatrix[x][y] = MMLValueGetDouble (pNode, szTemp, (x == y) ? 1 : 0);
   }

   // set the texture info
   TextureInfoSet (aTextMatrix);

   // wipe out existing cutouts
   m_listCutoutNames.Clear();
   m_listCutoutPoints.Clear();
   m_listCutoutClockwise.Clear();
   m_listOverlayNames.Clear();
   m_listOverlayPoints.Clear();
   m_listOverlayClockwise.Clear();

   // loop for cutouts
   PCMMLNode2 pSub;
   PWSTR psz;
   CMem memCutout;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      BOOL fCutout;
      fCutout = !_wcsicmp(psz, gszCutout);
      if (!fCutout && _wcsicmp(psz,gszOverlay) )
         continue;

      // found a cutout

      // get the same
      psz = MMLValueGet (pSub, gszName);
      if (!psz)
         continue;

      // number of points
      DWORD dwNum;
      dwNum = (DWORD) MMLValueGetInt (pSub, gszNumber, 0);
      if (!dwNum)
         continue;

      // allocate enough memory
      if (!memCutout.Required (dwNum * sizeof(TEXTUREPOINT)))
         continue;
      PTEXTUREPOINT ptp;
      ptp = (PTEXTUREPOINT) memCutout.p;

      for (x = 0; x < dwNum; x++) {
         swprintf (szTemp, gszCutoutPoint, (int) x);
         ptp[x].h = ptp[x].v = 0;
         MMLValueGetTEXTUREPOINT (pSub, szTemp, ptp + x);
      }

      BOOL fClockwise;
      fClockwise = (BOOL) MMLValueGetInt (pSub, gszClockwise, TRUE);

      // add this
      if (fCutout)
         CutoutSet (psz, ptp, dwNum, fClockwise);
      else
         OverlaySet (psz, ptp, dwNum, fClockwise);

   }

   
   return TRUE;
}



/***********************************************************************************
CSplineSurface::CloneTo - Copies all the information in this spline to a new
one, effectively cloning all the information.

inputs
   PCSplineSurface      pCloneTo - Copy to this
retursn
   BOOL - TRUE if success, FALSE if fail
*/
BOOL CSplineSurface::CloneTo (CSplineSurface *pCloneTo)
{
   // clear the cutouts
   pCloneTo->m_listCutoutNames.Clear();
   pCloneTo->m_listCutoutPoints.Clear();
   pCloneTo->m_listCutoutClockwise.Clear();
   pCloneTo->m_listOverlayNames.Clear();
   pCloneTo->m_listOverlayPoints.Clear();
   pCloneTo->m_listOverlayClockwise.Clear();

   if (!pCloneTo->ControlPointsSet (m_fLoopH, m_fLoopV, m_dwControlH, m_dwControlV,
      m_paControlPoints, m_padwSegCurveH, m_padwSegCurveV))
      return FALSE;

   pCloneTo->DetailSet (m_dwMinDivideH, m_dwMaxDivideH, m_dwMinDivideV, m_dwMaxDivideV,
      m_fDetail);

   pCloneTo->TextureInfoSet (m_aTextMatrix);

   // at the cutouts
   DWORD i;
   for (i = 0; i < this->CutoutNum(); i++) {
      PWSTR psz;
      DWORD dwNum;
      PTEXTUREPOINT ptp;
      BOOL fClockwise;
      if (!CutoutEnum (i, &psz, &ptp, &dwNum, &fClockwise))
         continue;
      pCloneTo->CutoutSet (psz, ptp, dwNum, fClockwise);
   }

   // at the overlays
   for (i = 0; i < this->OverlayNum(); i++) {
      PWSTR psz;
      DWORD dwNum;
      PTEXTUREPOINT ptp;
      BOOL fClockwise;
      if (!OverlayEnum (i, &psz, &ptp, &dwNum, &fClockwise))
         continue;
      pCloneTo->OverlaySet (psz, ptp, dwNum, fClockwise);
   }

   return TRUE;
}



/*******************************************************************************
CSplineSurface::ControlNumGet - Returns the number of control points across or down.

inputs
   BOOL        fH - If TRUE then in H(across) direction, else V(down) direction
returns
   DWORD number
*/
DWORD CSplineSurface::ControlNumGet (BOOL fH)
{
   return fH ? m_dwControlH : m_dwControlV;
}


/*******************************************************************************
CSplineSurface::QueryNodes - Returns the number of nodes in the interpolated spline.

returns
   BOOL        fH - If TRUE then in H(across) direction, else V(down) direction
returns
   DWORD - number of nodes. If looped, does NOT include the final node to loop around
*/
DWORD CSplineSurface::QueryNodes (BOOL fH)
{
   MakeSureSpline();

   return fH ? m_dwMeshH : m_dwMeshV;
}

/*******************************************************************************
CSplineSurface::LoopGet - Returns TRUE if the spline is looping across/down.

inputs
   BOOL        fH - If TRUE then in H(across) direction, else V(down) direction
returns
   BOOL - TRUE if looping
*/
BOOL CSplineSurface::LoopGet (BOOL fH)
{
   return fH ? m_fLoopH : m_fLoopV;
}


/*******************************************************************************
CSplineSurface::LoopSet - Sets how it's looped.

inputs
   BOOL        fH - If TRUE use the horizontal segment, FALSE use vertical
   BOOL        fLoop - if true, set to looped, false not looped
returns
   BOOL - TRUE if success
*/
BOOL CSplineSurface::LoopSet (BOOL fH, BOOL fLoop)
{
   BOOL *pf = (fH ? &m_fLoopH : &m_fLoopV);
   if (*pf == fLoop)
      return TRUE;

   // else set it
   *pf = fLoop;

   // if just turned on the loop have an extra curve to worry about, so
   // write this in
   if (fLoop && (ControlNumGet(fH) >= 2)) {
      SegCurveSet (fH, ControlNumGet(fH)-1, SegCurveGet (fH, ControlNumGet(fH) - 2));
   }

   m_fSplineDirty = TRUE;
   m_fCutoutsDirty = TRUE;
   m_fTextureDirty = TRUE;

   return TRUE;
}

/*******************************************************************************
CSplineSurface::ControlPointGet - Given an H and V index into the control
pointers (see ContorlNumGet()), fills in a point with the value of the control
point.

inputs
   DWORD       dwH, dwV - Index from 0..# pooints across - 1
   PCPoint     pVal - Filled in with the point
returns
   BOOL - TRUE if succeded
*/
BOOL CSplineSurface::ControlPointGet (DWORD dwH, DWORD dwV, PCPoint pVal)
{
   if ((dwH >= m_dwControlH) || (dwV >= m_dwControlV))
      return FALSE;

   pVal->Copy (m_paControlPoints + (dwH + dwV * m_dwControlH));
   return TRUE;
}

/*******************************************************************************
CSplineSurface::ControlPointSet - Given an H and V index into the control
pointers (see ContorlNumGet()), changes the control point to something new.

inputs
   DWORD       dwH, dwV - Index from 0..# pooints across - 1
   PCPoint     pVal - Use this point now
returns
   BOOL - TRUE if succeded
*/
BOOL CSplineSurface::ControlPointSet (DWORD dwH, DWORD dwV, PCPoint pVal)
{
   if ((dwH >= m_dwControlH) || (dwV >= m_dwControlV))
      return FALSE;

   m_paControlPoints[dwH + dwV * m_dwControlH].Copy (pVal);

   m_fSplineDirty = TRUE;
   m_fCutoutsDirty = TRUE;
   m_fTextureDirty = TRUE;

   return TRUE;
}

/*******************************************************************************
CSplineSurface::SegCurveGet - Gets the curvature of the segment at the given index.

inputs
   BOOL        fH - If TRUE use the horizontal segment, FALSE use vertical
   DWORD       dwIndex - Index into segment. From 0 .. ControlNumGet()-1.
returns
   DWORD - Curve. 0 if failed.
*/
DWORD CSplineSurface::SegCurveGet (BOOL fH, DWORD dwIndex)
{
   DWORD dwNum;
   DWORD *pdw;
   dwNum = fH ? m_dwControlH : m_dwControlV;
   if (fH) {
      if (!m_fLoopH)
         dwNum--;
   }
   else {
      if (!m_fLoopV)
         dwNum--;
   }
   pdw = fH ? m_padwSegCurveH : m_padwSegCurveV;
   if (dwIndex >= dwNum)
      return 0;

   return pdw[dwIndex];
}

/*******************************************************************************
CSplineSurface::SegCurveSet - Sets the curvature of the segment at the given index.

inputs
   BOOL        fH - If TRUE use the horizontal segment, FALSE use vertical
   DWORD       dwIndex - Index into segment. From 0 .. ControlNumGet()-1.
   DWORD       dwValue - new value
returns
   BOOL - TRUE if success
*/
BOOL CSplineSurface::SegCurveSet (BOOL fH, DWORD dwIndex, DWORD dwValue)
{
   DWORD dwNum;
   DWORD *pdw;
   dwNum = fH ? m_dwControlH : m_dwControlV;
   if (fH) {
      if (!m_fLoopH)
         dwNum--;
   }
   else {
      if (!m_fLoopV)
         dwNum--;
   }
   pdw = fH ? m_padwSegCurveH : m_padwSegCurveV;
   if (dwIndex >= dwNum)
      return 0;

   if (pdw[dwIndex] == dwValue)
      return TRUE;
   
   pdw[dwIndex] = dwValue;

   m_fSplineDirty = TRUE;
   m_fCutoutsDirty = TRUE;
   //m_fTextureDirty = TRUE; Texture is not dirty since doesn't change fundamental lenght

   return TRUE;
}

/*******************************************************************************
CSplineSurface::TextureInfoGet - Gets the current texture information from the spline.

inputs
   fp         m[2][2] - Filled with the information
returns
   BOOL - TRUE if scuced
*/
BOOL CSplineSurface::TextureInfoGet (fp m[2][2])
{
   DWORD x,y;
   for (x = 0; x < 2; x++) for (y = 0; y < 2; y++)
      m[x][y] = m_aTextMatrix[x][y];

   return TRUE;
}


/*******************************************************************************
CSplineSurface::DetailGet - Gets the current detail vlaues.

inputs
   DWORD*      pdwMin/MaxDivideH/V - Filled in the value. Can be null
   fp*     pfDetail - Filled in with detail. Can be null
returns
   non
*/
void CSplineSurface::DetailGet (DWORD *pdwMinDivideH, DWORD *pdwMaxDivideH, DWORD *pdwMinDivideV, DWORD *pdwMaxDivideV,
   fp *pfDetail)
{
   if (pdwMinDivideH)
      *pdwMinDivideH = m_dwMinDivideH;

   if (pdwMaxDivideH)
      *pdwMaxDivideH = m_dwMaxDivideH;

   if (pdwMinDivideV)
      *pdwMinDivideV = m_dwMinDivideV;

   if (pdwMaxDivideV)
      *pdwMaxDivideV = m_dwMaxDivideV;

   if (pfDetail)
      *pfDetail = m_fDetail;
}



/*********************************************************************************
CSplineSurface::IntersectLine - Intersects a line with the spline. If there is
more than one intersection point then it chooses the point closest to the start
of the line. NOTE: The line is not bounded by pStart or pDirection, but go beyond.

inputs
   PCPoint        pStart - Start of the line.
   PCPoint        pDirection - Direction vector.
   PTEXTUREPOINT  ptp - Filled with texture
   BOOL              fIncludeCutouts - If TRUE, then intersect with the surface, but only
                     if the area is not already cutout
   BOOL           fPositiveAlpha - If true, it only accepts intersections of pDirection x fAlpha + pStart
                  where fAlpha >= 0.
*/
BOOL CSplineSurface::IntersectLine (PCPoint pStart, PCPoint pDirection, PTEXTUREPOINT ptp,
                                    BOOL fIncludeCutouts, BOOL fPositiveAlpha)
{
   BOOL  fFound = FALSE;
   fp   fAlphaBest;

   fp fAlpha;
   TEXTUREPOINT temp;

   // make sure generate
   MakeSureCutouts();

   // get the first one for this
   PCListFixed plMG;
   plMG = *((PCListFixed*) m_listListMG.Get(0));

   // loop through all the points looking
   DWORD i, dwNum;
   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);
   dwNum = dwNormH * dwNormV;
   for (i = 0; i < dwNum; i++) {
      PCMeshGrid pmg = *((PCMeshGrid*) plMG->Get(i));
      if (!pmg->IntersectLineWithGrid (pStart, pDirection, &temp, &fAlpha, fIncludeCutouts))
         continue;

      // BUGFIX - Way that intersect will only accept positive alpha... needed when have
      // wrap around cabinets and cant add internal doors because the intersect line
      // comes with a negative alpha
      if (fPositiveAlpha && (fAlpha < 0))
         continue;

      fAlpha = fabs(fAlpha);

      if (fFound && (fAlpha > fAlphaBest))
         continue;   // not close enough

      fAlphaBest = fAlpha;
      fFound = TRUE;
      *ptp = temp;
   }

   return fFound;

}


/***********************************************************************************
CSplineSurface::ArchToPoints - Given an archway, this fills in a list with the
points (in HV) resulting from the intersection of the arch and the surface.

Arch: An arch is a set of points that loop around to form an opening in the surface.
Well, actually it's two loops so that the opening can be angled. The arch is the
volume formed by the loops.

input
   DWORD          dwNum - Number of points in the arch
   PCPoint        paFront - List of points that form the front of the arch. dwNum points
   PCPoint        paBack - List of points that form the back of the arch. dwNumPoints.
                           For every i, paFront[i] and paBack[i] are assumed to be
                           correlated, same points but on opposite ends of the wall.
returns
   PCListFixed - List of TEXTUREPOINT structures that provide a cutout for the arch.
   The points are arranged clockwise. This list must be freed by teh caller.
*/
typedef struct {
   DWORD          dwIndex;    // index into paFont and paBack that generates it
   BOOL           fValid;     // if TRUE the tp is valid, FALSE then tp invalid
   TEXTUREPOINT   tp;         // point
   BOOL           fGap;       // if TRUE there's a gap between this point and the next
} ARCHNODE, *PARCHNODE;
PCListFixed CSplineSurface::ArchToPoints (DWORD dwNum, PCPoint paFront, PCPoint paBack)
{
   // IMPORTANT: This has problems if the arch line crosses over more than one patch
   // because then the resulting line becomes bumpy. Technically the best way to solve
   // is to intersect the plane against all grid elements, but this is way to slow.
   // The next best way is to see if the distance covered by the arch length
   // is greater than th length of a grid element then divide the length in half
   // or quarters and use that.

   // start with an internal list and fill in what arch points we can
   CListFixed lArch;
   lArch.Init (sizeof(ARCHNODE));
   ARCHNODE an;
   DWORD i;
   CPoint pDirection;
   PARCHNODE pan;
   lArch.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      memset (&an, 0, sizeof(an));
      an.dwIndex = i;
      pDirection.Subtract (paBack + i, paFront + i);
      pDirection.Normalize(); // BUGFIX - To minimize roundoff error
      an.fValid = IntersectLine (paFront + i, &pDirection, &an.tp, FALSE, FALSE);

      // BUGFIX - Make sure witin 0..1 range if valid
      if (an.fValid) {
         an.tp.h = max(0,an.tp.h);
         an.tp.h = min(1,an.tp.h);
         an.tp.v = max(0,an.tp.v);
         an.tp.v = min(1,an.tp.v);
      }

      lArch.Add (&an);
   }

   // loop until the first valid point, so we know we have something
   for (i = 0; i < dwNum; i++) {
      pan = (PARCHNODE) lArch.Get(i);
      if (pan->fValid)
         break;
   }
   if (i >= dwNum)
      return NULL;  // nothing intersects. This is a problem.
   // IMPORTANT - Technically it's possible to have none of the points on the plane
   // but still have some visible, but I think this is an unlikely case so I wont do.

   // move all the points in front to the end until the point is in
   while (i) {
      pan = (PARCHNODE) lArch.Get(0);
      an = *pan;
      lArch.Remove(0);
      lArch.Add (&an);
      i--;
   }

   // loop through again... if there find a valid point followed by an invalid point,
   // or, and invalid point followed by valid then intersect with a plane.
   PARCHNODE pan2;
   TEXTUREPOINT pInter;
   for (i = 0; i < lArch.Num(); i++) { // use lArch.Num() because number may change
      pan = (PARCHNODE) lArch.Get(i);
      pan2 = (PARCHNODE) lArch.Get((i+1)%lArch.Num());

      // if it's not valid then we had a gap previous, so special case
      if (!pan->fValid) {
         // if the next one isn't valid then remove it.
         if (!pan2->fValid) {
            // IMPORTANT - This is sligntly broken. Really, should do intersectplaneborder
            // and find all the points that intersect, creating lines for them and dealing
            // with all that. Otherwise if more than one point outside the surface
            // then it assumed the points never cross back on. Usually this is safe so
            // I'll leave it
            lArch.Remove (i);
            i--;
            continue;
         }

         // know that the next one is valid, so intersect
         if (!IntersectPlaneBorderClosest (&paFront[pan->dwIndex], &paFront[pan2->dwIndex],
            &paBack[pan2->dwIndex], &paBack[pan->dwIndex], &pInter)) {
            // if cant intersect this point with what follows then assume the next point
            // is on the edge or something, so continue delet this one
            lArch.Remove (i);
            i--;
            continue;
         }

         // change this point
         pan->tp = pInter;
         pan->fValid = TRUE;
         continue;
      }

      // if get here, know than pan->fValid is true.

      // if the next one is valid (also) then do nothing
      if (pan2->fValid)
         continue;
      if (pan->fGap)
         continue;   // ignore the gap ones

      // else, need to intersect plane
      if (!IntersectPlaneBorderClosest (&paFront[pan->dwIndex], &paFront[pan2->dwIndex],
         &paBack[pan2->dwIndex], &paBack[pan->dwIndex], &pInter)) {
         // if cant intersect this point with what follows then assume this point is
         // on the edge, so mark it as a gap and delete the next one
         pan->fGap = TRUE;
         lArch.Remove (i+1);
         continue;
      }

      // if intersected then insert a new point with the intersection. The
      // inserted point will be marked as a gap (potentially wrap around border)
      // to fill later.
      an = *pan;
      an.fGap = TRUE;
      an.fValid = TRUE;
      an.tp = pInter;
      if (i+1 < lArch.Num())
         lArch.Insert (i+1, &an);
      else
         lArch.Add (&an);

   }

   // Figure out if clockwise or counter clockwise
   BOOL fDirection;
   CMem memClock;
   if (!memClock.Required(sizeof(TEXTUREPOINT) * lArch.Num()))
      return NULL;
   PTEXTUREPOINT ptClock;
   DWORD dwCount;
   dwCount = 0;
   ptClock = (PTEXTUREPOINT) memClock.p;
   for (i = 0; i < lArch.Num(); i++) {
      pan = (PARCHNODE) lArch.Get(i);
      if (!pan->fValid)
         continue;
      ptClock[i] = pan->tp;
      dwCount++;
   }
   fDirection = IsCurveClockwise (ptClock, dwCount);

#if 0 // cant do it this way
   DWORD dwIndex;
   CPoint p1, p2, p3;
   dwIndex = 0;
   for (i = 0; i < lArch.Num(); i++) {
      pan = (PARCHNODE) lArch.Get(i);
      if (!pan->fValid)
         continue;
      PCPoint pp;
      switch (dwIndex) {
      case 0:
         pp = &p1;
         break;
      case 1:
         pp = &p2;
         break;
      case 2:
         pp = &p3;
         break;
      }

      pp->Zero();
      pp->p[0] = pan->tp.h;
      pp->p[1] = pan->tp.v;

      dwIndex++;
      if (dwIndex >= 3)
         break;
   }
   if (dwIndex >= 3) {
      CPoint pN;
      p3.Subtract (&p2);
      p1.Subtract (&p2);
      pN.CrossProd (&p1, &p3);
      fDirection = (pN.p[2] < 0);   // if TRUE then going clockwise
   }
   else
      fDirection = TRUE;   // assume clockwise
#endif // 0

   // fill in the gaps
   for (i = 0; i < lArch.Num(); i++) { // use lArch.Num() because number may change
      pan = (PARCHNODE) lArch.Get(i);
      pan2 = (PARCHNODE) lArch.Get((i+1)%lArch.Num());

      if (!pan->fValid) {
         lArch.Remove(i);
         i--;  // can do this because know lArch.Get(0) is valid.
         continue;
      }

      if (!pan->fGap)
         continue;

      // we have a gap. find out which side point ends on
      int iV1, iV2, iH1, iH2;
      if (pan->tp.h < 0.001)
         iH1 = -1;
      else if (pan->tp.h > 0.999)
         iH1 = 1;
      else
         iH1 = 0;
      if (pan2->tp.h < 0.001)
         iH2 = -1;
      else if (pan2->tp.h > 0.999)
         iH2 = 1;
      else
         iH2 = 0;
      if (pan->tp.v < 0.001)
         iV1 = -1;
      else if (pan->tp.v > 0.999)
         iV1 = 1;
      else
         iV1 = 0;
      if (pan2->tp.v < 0.001)
         iV2 = -1;
      else if (pan2->tp.v > 0.999)
         iV2 = 1;
      else
         iV2 = 0;

      // first of all, if either pan or pan2 are not on the edge then
      // some bad calculation has happened so, kill the gap and ignore it
      if ((!iH1 && !iV1) || (!iH2 && !iV2)) {
         pan->fGap = FALSE;
         continue;
      }

      // if they're on the same edge then don't worry about the gap. It's
      // realdy solved
      if ((iH1 && (iH1 == iH2)) || (iV1 && (iV1 == iV2)) ) {
         pan->fGap = FALSE;
         continue;
      }

      // if get here, we'll need to insert a new point which rotates around until
      // the gap is fileld
      an = *pan;
      pan->fGap = FALSE;
      if (fDirection) { // clockwise
         if ((iV1 == -1) && (iH1 <= 0))
            iH1 = 1; // on the top and going right
         else if ((iH1 == 1) && (iV1 <= 0))
            iV1 = 1; // on the right and going down
         else if ((iV1 == 1) && (iH1 >= 0))
            iH1 = -1;   // on the bottom and going left
         else if ((iH1 == -1) && (iV1 >= 0))
            iV1 = -1;   // on left and going up
      }
      else { //counterclocksize
         if ((iV1 == -1) && (iH1 >= 0))
            iH1 = -1; // on the top and going left
         else if ((iH1 == 1) && (iV1 >= 0))
            iV1 = -1; // on the right and going up
         else if ((iV1 == 1) && (iH1 <= 0))
            iH1 = 1;   // on the bottom and going right
         else if ((iH1 == -1) && (iV1 <= 0))
            iV1 = 1;   // on left and going down
      }

      an.tp.h = (iH1 == -1) ? 0 : 1; // will either be -1 or 1
      an.tp.v = (iV1 == -1) ? 0 : 1;   // will either be -1 or 1
      an.fGap = TRUE;
      if (i+1 < lArch.Num())
         lArch.Insert (i+1, &an);
      else
         lArch.Add (&an);

      // go on
   }

   // Get rid of duplicate points
   for (i = 0; i < lArch.Num(); i++) { // use lArch.Num() because number may change
      pan = (PARCHNODE) lArch.Get(i);
      pan2 = (PARCHNODE) lArch.Get((i+1)%lArch.Num());

      if ( (fabs(pan->tp.h - pan2->tp.h) < .001) && (fabs(pan->tp.v - pan2->tp.v) < .001)) {
         // delete since they're the same
         lArch.Remove ((i+1)%lArch.Num());
      }
   }


   // create the list
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(TEXTUREPOINT));

   dwNum = lArch.Num();

   if (fDirection) {
      pl->Required (dwNum);
      for (i = 0; i < dwNum; i++) {
         pan = (PARCHNODE) lArch.Get(i);
         pl->Add (&pan->tp);
      }
   }
   else {
      pl->Required (dwNum);
      for (i = dwNum-1; i < dwNum; i--) {
         pan = (PARCHNODE) lArch.Get(i);
         pl->Add (&pan->tp);
      }
   }

   return pl;
}

/***********************************************************************************
CSplineSurface::ArchToCutout - Given an archway, this fills in a list with the
points (in HV) resulting from the intersection of the arch and the surface.
The list is then passed to the cutouts.

Arch: An arch is a set of points that loop around to form an opening in the surface.
Well, actually it's two loops so that the opening can be angled. The arch is the
volume formed by the loops.

input
   DWORD          dwNum - Number of points in the arch
   PCPoint        paFront - List of points that form the front of the arch. dwNum points
   PCPoint        paBack - List of points that form the back of the arch. dwNumPoints.
                           For every i, paFront[i] and paBack[i] are assumed to be
                           correlated, same points but on opposite ends of the wall.
   PWSTR          pszCutout - Cutout name to overwrite and use for the arch
   BOOL           fOverlay - If TRUE then really want to write to an overlay, not cutout
returns
   BOOL - TRUE if successful.
*/
BOOL CSplineSurface::ArchToCutout (DWORD dwNum, PCPoint paFront, PCPoint paBack, PWSTR pszCutout,
                                   BOOL fOverlay)
{
   PCListFixed pl = ArchToPoints (dwNum, paFront, paBack);
   if (!pl)
      return FALSE;

   BOOL fRet;

   if (fOverlay)
      fRet = OverlaySet (pszCutout, (PTEXTUREPOINT) pl->Get(0), pl->Num(), TRUE);
   else
      fRet = CutoutSet (pszCutout, (PTEXTUREPOINT) pl->Get(0), pl->Num(), TRUE);
   delete pl;
   return fRet;
}



/***********************************************************************************
CSplineSurface::IntersectSemiPlaneBorder - Intersects a semi plane against the
border of the mesh, from (0,0) to (1,0) to (1,1) to (0,1). Any intersection
points are added to the list.

inputs
   PCPoint        p1,p2,p3 - Three points that define a plane. The line will not intersect
                  if:
                        If's on the oppoise side of line p1,p3 to p2.
                        It's on the opposite side of line p1,p2 to p3.
                        If't on beyond the line running through p3 and parallel to p1,p2
   PCListFixed    pl - Should be a list initialized to TEXTUREPOINT structures. Any points that
                  intersect will be added.
   DWORD          dwIntersectFlags - Flags passed to IntersectLineTriangle, and affect
                  the rules for p1,p2,p3 that are cut out.
returns
   none
*/
void CSplineSurface::IntersectSemiPlaneBorder (PCPoint p1, PCPoint p2, PCPoint p3, PCListFixed pl,
                                               DWORD dwIntersectFlags)
{
   // make sure generate cutouts
   MakeSureCutouts();

   // get the first one for this
   PCListFixed plMG;
   plMG = *((PCListFixed*) m_listListMG.Get(0));



   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);

   // loop through the top and bottom
   DWORD x,y;
   PCMeshGrid pmg;
   CPoint pDelta, pIntersect;
   fp fAlpha;
   TEXTUREPOINT tHV, tMesh;
   for (x = 0; x < dwNormH; x++) {
      // top
      pmg = *((PCMeshGrid*) plMG->Get(MESHG(x, 0)));
      pDelta.Subtract (&pmg->m_apPosn[0][1], &pmg->m_apPosn[0][0]);
      if (IntersectLineTriangle (&pmg->m_apPosn[0][0], &pDelta, p1, p2, p3,
         &pIntersect, &fAlpha, &tHV, dwIntersectFlags)) {
            // else, intersected, so save
            if ((fAlpha >= 0) && (fAlpha <= 1)) {
               tMesh.h = fAlpha * (pmg->m_afH[1] - pmg->m_afH[0]) + pmg->m_afH[0];
               tMesh.v = pmg->m_afV[0];   // top
               pl->Add (&tMesh);
            }
      }

      // bottom
      pmg = *((PCMeshGrid*) plMG->Get(MESHG(x, dwNormV-1)));
      pDelta.Subtract (&pmg->m_apPosn[1][1], &pmg->m_apPosn[1][0]);
      if (IntersectLineTriangle (&pmg->m_apPosn[1][0], &pDelta, p1, p2, p3,
         &pIntersect, &fAlpha, &tHV, dwIntersectFlags)) {
            // else, intersected, so save
            if ((fAlpha >= 0) && (fAlpha <= 1)) {
               tMesh.h = fAlpha * (pmg->m_afH[1] - pmg->m_afH[0]) + pmg->m_afH[0];
               tMesh.v = pmg->m_afV[1];   // bottom
               pl->Add (&tMesh);
            }
      }
   }

   // left and right
   for (y = 0; y < dwNormV; y++) {
      // left
      pmg = *((PCMeshGrid*) plMG->Get(MESHG(0, y)));
      pDelta.Subtract (&pmg->m_apPosn[1][0], &pmg->m_apPosn[0][0]);
      if (IntersectLineTriangle (&pmg->m_apPosn[0][0], &pDelta, p1, p2, p3,
         &pIntersect, &fAlpha, &tHV, dwIntersectFlags)) {
            // else, intersected, so save
            if ((fAlpha >= 0) && (fAlpha <= 1)) {
               tMesh.v = fAlpha * (pmg->m_afV[1] - pmg->m_afV[0]) + pmg->m_afV[0];
               tMesh.h = pmg->m_afH[0];   // left
               pl->Add (&tMesh);
            }
      }

      // right
      pmg = *((PCMeshGrid*) plMG->Get(MESHG(dwNormH-1, y)));
      pDelta.Subtract (&pmg->m_apPosn[1][1], &pmg->m_apPosn[0][1]);
      if (IntersectLineTriangle (&pmg->m_apPosn[0][1], &pDelta, p1, p2, p3,
         &pIntersect, &fAlpha, &tHV, dwIntersectFlags)) {
            // else, intersected, so save
            if ((fAlpha >= 0) && (fAlpha <= 1)) {
               tMesh.v = fAlpha * (pmg->m_afV[1] - pmg->m_afV[0]) + pmg->m_afV[0];
               tMesh.h = pmg->m_afH[1];   // right
               pl->Add (&tMesh);
            }
      }
   }
}


/***********************************************************************************
CSplineSurface::IntersectPlaneBorderClosest - Intersects a plane defined by 4 points
with the border. This may produce many points. If it does, the point closes to
the line p1 to p2 is kept.

inputs
   PCPoint     p1,p2,p3,p4 - 4 points on a plane, running clockwise (or counter clockwise)
               in a box. The borders to the test are defined by the lines p2->p3 and
               p4->p1. If more than one point exists then the point closest to p1->p2
               is kept.
   PTEXTUREPOINT  ptp - Filled with the location of the closest intersection
returns
   BOOL - TRUE if found one
*/
BOOL CSplineSurface::IntersectPlaneBorderClosest (PCPoint p1, PCPoint p2, PCPoint p3, PCPoint p4,
                                                  PTEXTUREPOINT ptp)
{
   CListFixed list;
   list.Init (sizeof(TEXTUREPOINT));

   // split plane in two and intersect just in case there are angled cant handle
   IntersectSemiPlaneBorder (p1, p2, p3, &list, 0x27);   // BUGFIX - Make intersect 0x27 in these two to make more forgiving
   IntersectSemiPlaneBorder (p3, p4, p1, &list, 0x27);

   // if there's only one then done
   DWORD dwNum;
   dwNum = list.Num();
   if (!dwNum)
      return FALSE;
   if (dwNum == 1) {
      *ptp = *((PTEXTUREPOINT) list.Get(0));
      return TRUE;
   }

   // else, find location of points and distance from line
   DWORD dwClosest;
   fp fClosest, fDist;
   PTEXTUREPOINT p;
   CPoint pPosn, pVector, pNearest;
   pVector.Subtract (p2, p1);
   DWORD i;
   for (i = 0; i < list.Num(); i++) {
      p = (PTEXTUREPOINT) list.Get(i);

      // get the psotion based on p.h and p.v
      if (!HVToInfo (p->h, p->v, &pPosn))
         continue;
      fDist = DistancePointToLine (&pPosn, p1, &pVector, &pNearest);

      if (!i || (fDist < fClosest)) {
         // nothing so far, so keep this
         dwClosest = i;
         fClosest = fDist;
      }

   }

   // use that one
   p = (PTEXTUREPOINT) list.Get(dwClosest);
   *ptp = *p;

   return TRUE;
}

/***********************************************************************************
CSplineSurface::HVToInfo - Given an H and V, this will fill in the position,
normal, and texture.

inputs
   fp      h, v - Location
   PCPoint     pPoint - Filled in with the point. Can be NULL.
   PCPoint     pNorm - Filled in with the normal. Can be NULL.
   PTEXTUREPOINT pText - Filled in with the texture. Can be NULL.
rturns
   BOOL - TRUE if point h,v inside. FALSE if not
*/
BOOL CSplineSurface::HVToInfo (fp h, fp v, PCPoint pPoint, PCPoint pNorm, PTEXTUREPOINT pText)
{
   // BUGFIX - Was a roundoff error problem where v was -2e9
   if ((h < -EPSILON) || (h > 1.0 + EPSILON) || (v < -EPSILON) || (v > 1.0+EPSILON))
      return FALSE;
   h = max(h,0);
   h = min(h,1);
   v = max(v,0);
   v = min(v,1);

   // make sure generate cutouts
   MakeSureCutouts();

   // get the first one for this
   PCListFixed plMG;
   plMG = *((PCListFixed*) m_listListMG.Get(0));

   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);

   // which grid
   DWORD dwX, dwY;
   dwX = (DWORD) (h * dwNormH);
   dwX = min(dwX, dwNormH-1);
   dwY = (DWORD) (v * dwNormV);
   dwY = min(dwY, dwNormV-1);

   PCMeshGrid pmg;
   pmg = *((PCMeshGrid*) plMG->Get(MESHG(dwX, dwY)));
   pmg->Point (h, v, pPoint, pNorm, pText);

   return TRUE;
}



/***************************************************************************************
CSplineSurface::ControlPointsToNormals - Given an X and Y for a CONTROL point, this
looks at the subdivided spline in the same area and determines a normal to the control point.

NOTE: These are not drawing normals, but really normals perpendicular to the surface.

inputs
   DWORD       dwX, dwY - 0..m_dwControlH-1, 0..m_dwControlV-1
   PCPoint     pNormal - Filled with the normal (pV x pH). Normalized. Can be NULL.
   PCPoint     pH - Filled with the horizontal generated basically by p(dX+1) - p(dX-1).
                  Of course, the interpolated grid is used to generate something more
                  accurate, and boundary conditions are taken into account. Normalized.
                  Can be NULL.
   PCPoint     pV - Vertical. Same stuff as pH.
return
   BOOL - TRUE if success
*/
BOOL CSplineSurface::ControlPointsToNormals (DWORD dwX, DWORD dwY, PCPoint pNormal, PCPoint pH, PCPoint pV)
{
   // will need the spline calculated
   MakeSureSpline();

   // convert dwX and dwY to spline coordinates
   dwX <<= m_dwDivideH;
   dwY <<= m_dwDivideV;

   if ((dwX >= m_dwMeshH) || (dwY >= m_dwMeshV))
      return FALSE;

   // find out which points are to the left, right, up, down
   DWORD dwLeft, dwRight, dwUp, dwDown;
   if (m_fLoopH) {
      dwLeft = (dwX + m_dwMeshH - 1) % m_dwMeshH;
      dwRight = (dwX + 1) % m_dwMeshH;
   }
   else {
      dwLeft = dwX ? (dwX - 1) : 0;
      dwRight = (dwX < (m_dwMeshH-1)) ? (dwX + 1) : (m_dwMeshH - 1);
   }
   if (m_fLoopV) {
      dwUp = (dwY + m_dwMeshV - 1) % m_dwMeshV;
      dwDown = (dwY + 1) % m_dwMeshV;
   }
   else {
      dwUp = dwY ? (dwY - 1) : 0;
      dwDown = (dwY < (m_dwMeshV-1)) ? (dwY + 1) : (m_dwMeshV - 1);
   }

   // use these to generate h and V
   CPoint h, v;
   h.Subtract (m_paMesh + MESH(dwRight, dwY), m_paMesh + MESH(dwLeft, dwY));
   v.Subtract (m_paMesh + MESH(dwX, dwDown), m_paMesh + MESH(dwX, dwUp));
   if (pH) {
      pH->Copy (&h);
      pH->Normalize();
   }
   if (pV) {
      pV->Copy (&v);
      pV->Normalize();
   }
   if (pNormal) {
      pNormal->CrossProd (&v, &h);
      pNormal->Normalize();
   }

   return TRUE;
}



/***************************************************************************************
CSplineSurface::HVDrag - Utility function used to help dragging of control points
in a surface. It helps with control points stored as HV coordinates (0..1,0..1) relative
to the surface.

1) It runs a ray from the viewer to the new point (in object space) and
   sees where it intersects the surface, and that HV is returned.
2) If if doesn't intersect the surface then a triangle (unbounded on one end) is
   created with three points, the old HV converted to object space, the new control
   point location, and the viewer. This is intersected with the edge of the surface
   to find a hit point. (The closest one to the viewer is taken).
3) If nothing can be found then returns FALSE, meaning the drag should be ignored.

BUGFIX - Modified so it enforces the drag to be in the pDirection. It will not
accept a NEGATIVE alpha for the pDirection.

inputs
   PTEXTUREPOINT        pTextOld - Old H and 
   PCPoint              pNew - New Point, in object space. (Spline space actually)
   PCPoint              pViewer - Viewer, in object space. (Spline space actually)
   PTEXTUREPOINT        pTextNew - Filled with the new texture point if successful
returns
   BOOL - TRUE if success, FALSE if cant find intersect
*/
BOOL CSplineSurface::HVDrag (PTEXTUREPOINT pTextOld, PCPoint pNew, PCPoint pViewer, PTEXTUREPOINT pTextNew)
{
   if (!pNew || !pViewer)
      return FALSE;

   CPoint pDirection;
   pDirection.Subtract (pNew, pViewer);

   if (IntersectLine (pViewer, &pDirection, pTextNew, FALSE, TRUE))
      return TRUE;

   // else, doesn't so intersect with edge
   CPoint pOld;
   if (!HVToInfo (pTextOld->h, pTextOld->v, &pOld))
      return FALSE;
   CListFixed list;
   list.Init (sizeof(TEXTUREPOINT));
   IntersectSemiPlaneBorder (&pOld, pViewer, pNew, &list, 0x01 | 0x02);
   DWORD dwNum;
   dwNum = list.Num();
   if (!dwNum)
      return FALSE;

   // find the best fit
   DWORD dwBest, i;
   fp   fDistBest, fDist;
   dwBest = -1;
   fDistBest = 0;
   PTEXTUREPOINT pt;
   for (i = 0; i < dwNum; i++) {
      pt = (PTEXTUREPOINT) list.Get(i);

      // find the distance
      fDist = fabs(pt->h - pTextOld->h) + fabs(pt->v - pTextOld->v);

      if ((dwBest == -1) || (fDist < fDistBest)) {
         dwBest = i;
         fDistBest = fDist;
      }
   }

   // use the best
   pt = (PTEXTUREPOINT) list.Get(dwBest);
   *pTextNew = *pt;
   return TRUE;
}


/*************************************************************************************
IntersectLineWithGrid - Intersects a line in HV with a mesh, it then fills in a list
with all the intersection points.

inputs
   PTEXTUREPOINT     p1, p2 - Starting and ending point of the line.
   BOOL              fIncludeEnd - p1 is always included in the final list. If fIncludeEnd
                     then also include p2.
   PCListFixed       pl - Initialized to TEXTUREPOINT arrays. This is filled with
                     p1, then the intersection of points between p1 and p2. ANd matbe p2.
   fp            fGridH - Spacing of the grid along the X direction, distance between elem
   fp            fGridV - Spacing of the grid along the Y direction, distance between elem
returns
   none
*/
void IntersectLineWithGrid (PTEXTUREPOINT p1, PTEXTUREPOINT p2, BOOL fIncludeEnd,
                            PCListFixed pl, fp fGridH, fp fGridV)
{
   // figure out slope
   fp fRise, fRun;
   fRise = p2->v - p1->v;
   fRun = p2->h - p1->h;
   fp fm, fmInv, fb, fbInv;
   BOOL fVertLine, fHorzLine;
   fVertLine = fHorzLine = FALSE;
   if (fabs(fRun) > EPSILON) {   // y = m * x + b
      fm = fRise / fRun;
      fb = p1->v - fm * p1->h;
   }
   else {
      fVertLine = TRUE;
      fb = p1->v;
   }
   if (fabs(fRise) > EPSILON) {  // x = y * mInv + bInv
      fmInv = fRun / fRise;
      fbInv = p1->h - fmInv * p1->v;
   }
   else {
      fHorzLine = TRUE;
      fbInv = p1->h;
   }
   if (fVertLine && fHorzLine)
      return;  // not much of a line at all

   // do along v first
   fp fVCur, fVNext, fHCur, fHNext;
   int   iv, ih;
   BOOL fForwardV, fForwardH;
   fForwardV = p2->v > p1->v;
   fForwardH = p2->h > p1->h;
   fVCur = p1->v;
   if (fForwardV)
      iv = (int) floor(fVCur / fGridV);
   else
      iv = (int) ceil(fVCur / fGridV);
   while (TRUE) {
      fp fH1, fH2;
      if (!fHorzLine) {
         if (fForwardV)
            iv++;
         else
            iv--;

         fVNext = iv * fGridV;
         if (fForwardV)
            fVNext = min(fVNext, p2->v);
         else
            fVNext = max(fVNext, p2->v);

         // have a line from fVCur to fVNext, what are the h1 and h2 positions of that line
         fH1 = fmInv * fVCur + fbInv;
         fH2 = fmInv * fVNext + fbInv;
      }
      else {
         // since fmInv is infinity, just use fBInv
         fH1 = p1->h;
         fH2 = p2->h;
         fVCur = p1->v;
         fVNext = p2->v;
      }

      // the line segment from fH1,fVCur to fH2,fVNext is entiurely within a row

      // do all the horizontal lines within this
      fHCur = fH1;
      if (fForwardH)
         ih = (int) floor(fHCur / fGridH);
      else
         ih = (int) ceil(fHCur / fGridH);
      while (TRUE) {
         fp fV1, fV2;
         if (!fVertLine) {
            if (fForwardH)
               ih++;
            else
               ih--;

            fHNext = ih * fGridH;
            if (fForwardH)
               fHNext = min(fHNext, fH2);
            else
               fHNext = max(fHNext, fH2);

            // have a line from fHCur to fHNext, what are the v1 and v2 positions of that line
            fV1 = fm * fHCur + fb;
            fV2 = fm * fHNext + fb;
         }
         else {
            // since fmInv is infinity, just use fBInv
            fV1 = fVCur;
            fV2 = fVNext;
            fHCur = fH1;
            fHNext = fH2;
         }

         // the line segment from fHCur,fV1 to fHNext,fV2 is entirely within
         // a column. And since we're in this loop, it's also entirely within
         // a row. Therefore add it
         TEXTUREPOINT tp;
         tp.h = fHCur;
         tp.v = fV1;
         pl->Add (&tp);
         // NOTE: Not adding the second point since that will be added later
         // anyway

         // if >= end then break
         if ( (fForwardH && (fHNext >= fH2)) || (!fForwardH && (fHNext <= fH2)))
            break;
         fHCur = fHNext;
      }



      // if >= end then break
      if ( (fForwardV && (fVNext >= p2->v)) || (!fForwardV && (fVNext <= p2->v)))
         break;
      fVCur = fVNext;
   }

   // add last point
   if (fIncludeEnd)
      pl->Add (p2);
}



/**********************************************************************************
IntersectLoopWithGrid - Given a set of TEXTUREPOINTs, this intersects them with
the grid and comes up with a list of HVXYZ structures. Use this to ensure that
the loop tracks the bends in the surface perfectly.

inputs
   PTEXTUREPOINT     pLoop - List of HV points
   DWORd             dwNum - Number of points in pLoop
   BOOL              fLooped - If TRUE, pLoop is connected at the ends. If FALSE, end points are separate
   PCListFixed       pl - List of HVXYZ. Must already be initialized.
   BOOL              fReverse - If TRUE, go through the points in pLoop in reverse order
returns
   none
*/
void CSplineSurface::IntersectLoopWithGrid (PTEXTUREPOINT pLoop, DWORD dwNum, BOOL fLooped, PCListFixed pl,
                                            BOOL fReverse)
{
   MakeSureSpline();

   fp fGridH, fGridV;
   if (m_fLoopH)
      fGridH = 1.0 / (fp)m_dwMeshH;
   else
      fGridH = 1.0 / ((fp)m_dwMeshH - 1);
   if (m_fLoopV)
      fGridV = 1.0 / (fp)m_dwMeshV;
   else
      fGridV = 1.0 / ((fp)m_dwMeshV - 1);


   // loop
   // CListFixed lTP;
   m_lIntersectLoopWithGridTP.Init (sizeof(TEXTUREPOINT));
   DWORD i;
   PTEXTUREPOINT p1, p2;
   for (i = 0; i < dwNum - (fLooped ? 0 : 1); i++) {
      if (fReverse) {
         p1 = pLoop + (dwNum - i - 1);
         p2 = pLoop + ((dwNum*2 - i - 2) % dwNum);
      }
      else {
         p1 = pLoop + i;
         p2 = pLoop + ((i + 1) % dwNum);
      }

      // generate the points. Note: Only put the end on if we're not looped
      // AND we're on the last point
      IntersectLineWithGrid (p1, p2, !fLooped && (i == dwNum-2), &m_lIntersectLoopWithGridTP, fGridH, fGridV);
   }

   // loop through the points generated, get the XYZ, and usee those
   HVXYZ hv;
   pl->Required (pl->Num() + m_lIntersectLoopWithGridTP.Num());
   for (i = 0; i < m_lIntersectLoopWithGridTP.Num(); i++) {
      p1 = (PTEXTUREPOINT) m_lIntersectLoopWithGridTP.Get(i);
      hv.h = p1->h;
      hv.v = p1->v;
      HVToInfo (hv.h, hv.v, &hv.p);
      pl->Add (&hv);
   }

   // all done
}



#define  NUMVOXEL    16
/*************************************************************************************
BoundToVoxel - Given a MGQUAD, this finds what voxels it spans and fills in aiMinMax
array with the min/max values (inclusive). If the MGQUAD is entirely outside
of the voxel range then it returns FALSE - indicating trivial clip.

inputs
   PMGQUAD        pq - Quadrilateral (or triangle)
   PCPoint        pMin - Minimum extent of surface's bounding box
   PCPoint        pMax - Maximum extent of surface's bounding box
   PCPoint        pVoxelSize - Size of each voxel. Assume NUMVOXEL max.
   int            aiMinMax[3][2] - [X=0,Y=1,Z=2][min=0,max=1] - Filled in with
                  the voxel numbers.
returns
   BOOL - TRUE if in volxel space, FALSE if trivial clip
*/
BOOL BoundToVoxel (PMGQUAD pq, PCPoint pMin, PCPoint pMax, PCPoint pVoxelSize, int aiMinMax[3][2])
{
   // find min and max
   CPoint pQMax, pQMin;
   pQMax.Copy (&pq->apPosn[0]);
   pQMin.Copy (&pq->apPosn[0]);
   DWORD i, j;
   for (i = 1; i < pq->dwPoints; i++) for (j = 0; j < 3; j++) {
      pQMin.p[j] = min(pQMin.p[j], pq->apPosn[i].p[j]);
      pQMax.p[j] = max(pQMax.p[j], pq->apPosn[i].p[j]);
   }

   // trivial reject
   for (j = 0; j < 3; j++) {
      pQMin.p[j] = max(pQMin.p[j], pMin->p[j]);
      pQMax.p[j] = min(pQMax.p[j], pMax->p[j]);
      if (pQMin.p[j] > pQMax.p[j])
         return FALSE;  // clip
   }

   // scale
   for (j = 0; j < 3; j++) {
      pQMin.p[j] = (pQMin.p[j] - pMin->p[j]) / pVoxelSize->p[j];
      pQMax.p[j] = (pQMax.p[j] - pMin->p[j]) / pVoxelSize->p[j];

      aiMinMax[j][0] = min((int)pQMin.p[j], NUMVOXEL-1);
      aiMinMax[j][1] = min((int)pQMax.p[j], NUMVOXEL-1);
   }
   
   return TRUE;
}


/*************************************************************************************
CSplineSurface::IntersectWithSurfaces - Intersect this surface with one or more
surfaces and produce a list of line sequences that can be used for clipping, or whatever.

inputs
   fp                fExtend - Once find the intersections, extend them by this much
                     in HV space just to make sure they overlap
   DWORD             dwNum - Number of surfaces to intersect with
   PCSplineSurface   *papss - Pointer to an array of dwNum PCSplineSurface
   PCMatrix          pam - Pointer to an array of matricies that convert points
                        from the other spline surface's coordinates to this one's
   BOOL              fIgnoreCutouts - If TRUE, ignore cutouts when intersecting,
                        which means intersect the whole sheet
returns
   PCListFixed - List of line seqments. Each line segment is TEXTUREPOINT[2] from
      start to end. The direction of the segment is such that looking at the front of the
      surface, the front of the surface that produced the intersection
      segment is on the left side of the line. These line segments may cross over one
      another and there may be duplicates;
      The caller must free up this list
*/

static int _cdecl MGSort (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   return (int) (*pdw1) - (int)(*pdw2);
}


PCListFixed CSplineSurface::IntersectWithSurfaces (fp fExtend, DWORD dwNum, PCSplineSurface *papss, PCMatrix pam,
                                                   BOOL fIgnoreCutouts)
{
   // no intersection if no points
   if (!dwNum)
      return NULL;

   // find the bounding box
   CPoint pMin, pMax;
   BOOL fFirstTime = TRUE;
   pMin.Zero();
   pMax.Zero();
   MakeSureCutouts();
   PCListFixed plMG;
   plMG = *((PCListFixed*) m_listListMG.Get(0));
   DWORD dwNormH, dwNormV, dwNumMG;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);
   dwNumMG = dwNormH * dwNormV;
   PCMeshGrid pmg;
   DWORD i, j, k;
   PCPoint p;
   for (i = 0; i < dwNumMG; i++) {
      pmg = *((PCMeshGrid*) plMG->Get(i));

      for (j = 0; j < 4; j++) {
         p = &pmg->m_apPosn[j/2][j%2];

         if (fFirstTime) {
            pMin.Copy (p);
            pMax.Copy (p);
            fFirstTime = FALSE;
            continue;
         }

         // else min, max
         for (k = 0; k < 3; k++) {
            pMin.p[k] = min(pMin.p[k], p->p[k]);
            pMax.p[k] = max(pMax.p[k], p->p[k]);
         }
      } // loop over corners
   }  // loop over mesh grids

   // BUGFIX - Make just a bit larger to minimize round-off error chances
   CPoint pExtra;
   pExtra.Zero();
   pExtra.p[0] = pExtra.p[1] = pExtra.p[2] = .01;
   pMin.Subtract (&pExtra);
   pMax.Add (&pExtra);

   CPoint pVoxelSize;   // determine size of voxel
   pVoxelSize.Subtract (&pMax, &pMin);
   pVoxelSize.Scale (1.0 / NUMVOXEL);
   for (k = 0; k < 3; k++)
      pVoxelSize.p[k] = max(pVoxelSize.p[k], .01); // just so they're not infinitely small

   // initialize the voxel memory
   PCListFixed    apVoxel[NUMVOXEL][NUMVOXEL][NUMVOXEL];
   memset (&apVoxel, 0, sizeof(apVoxel));

   // loop through all the polygons used to create the surface. For each
   // polygon find out what voxels it occupies, and add a pointer to the polygon
   // to the voxel
   PMGQUAD pq;
   MGQUAD q;
   DWORD x, y, z;
   CListFixed lTempQuad;
   DWORD dwQuadNum, dwAddNum;
   lTempQuad.Init (sizeof(MGQUAD));
   int aiBound[3][2];
   for (i = 0; i < dwNumMG; i++) {
      pmg = *((PCMeshGrid*) plMG->Get(i));

      // NOTE: Because want to find intersection regardless of cutouts or overlays
      // that may be present, just use the entire CMeshGrid area.
      memset (&q, 0, sizeof(q));
      q.dwPoints = 4;
      for (k = 0; k < 4; k++) {
         q.apPosn[k].Copy (&pmg->m_apPosn[(k >= 2) ? 1 : 0][(k==1 || k==2) ? 1 : 0]);
         q.atText[k].v = pmg->m_afV[(k >= 2) ? 1 : 0];
         q.atText[k].h = pmg->m_afH[(k==1 || k==2) ? 1 : 0];
      }
      dwAddNum = lTempQuad.Add (&q);
      pq = (PMGQUAD) lTempQuad.Get(lTempQuad.Num()-1);

      // find out limits of this
      if (!BoundToVoxel (pq, &pMin, &pMax, &pVoxelSize, aiBound))
         continue;

      // loop through all of these voxels
      for (x = (DWORD)aiBound[0][0]; x <= (DWORD) aiBound[0][1]; x++)
      for (y = (DWORD)aiBound[1][0]; y <= (DWORD) aiBound[1][1]; y++)
      for (z = (DWORD)aiBound[2][0]; z <= (DWORD) aiBound[2][1]; z++) {
         if (!apVoxel[x][y][z]) {
            apVoxel[x][y][z] = new CListFixed;
            if (!apVoxel[x][y][z])
               continue;
            apVoxel[x][y][z]->Init (sizeof(DWORD));
         }

         apVoxel[x][y][z]->Add (&dwAddNum);
      }

   } // loop over mesh grids

   // loop through all the surfaces, and all the quads in the surfaces, finding
   // the intersection
   DWORD dwSurf;
   CListFixed  lInter, lLinesTemp, *plLines;
   lInter.Init (sizeof (DWORD));
   lLinesTemp.Init (sizeof(TEXTUREPOINT)*2);
   plLines = new CListFixed;
   if (!plLines)
      return NULL;
   plLines->Init (sizeof(TEXTUREPOINT) * 2);
   for (dwSurf = 0; dwSurf < dwNum; dwSurf++) {
      PCSplineSurface pss = papss[dwSurf];

      // make sure its initialized
      pss->MakeSureCutouts();

      // clear out the temporary lines for this surface
      lLinesTemp.Clear();

      // how many mesh grids
      dwNormH = pss->m_dwMeshH - (pss->m_fLoopH ? 0 : 1);
      dwNormV = pss->m_dwMeshV - (pss->m_fLoopV ? 0 : 1);
      dwNumMG = dwNormH * dwNormV;

      // NOTE: Getting MGList # 0, which is includes all the cutouts
      plMG = *((PCListFixed*) pss->m_listListMG.Get(0));

      // loop through these
      for (i = 0; i < dwNumMG; i++) {
         pmg = *((PCMeshGrid*) plMG->Get(i));

         // if trivially gotten rid of then continue
         if (!fIgnoreCutouts && pmg->m_fTrivialClip)
            continue;

         // if trivial accept then just use CMeshGrid infomration
         dwQuadNum = pmg->m_listMGQUAD.Num();
         if (fIgnoreCutouts || pmg->m_fTrivialAccept)
            dwQuadNum = 1;

         for (j = 0; j < dwQuadNum; j++) {
            if (fIgnoreCutouts || pmg->m_fTrivialAccept) {
               memset (&q, 0, sizeof(q));
               q.dwPoints = 4;
               for (k = 0; k < 4; k++) {
                  q.apPosn[k].Copy (&pmg->m_apPosn[(k >= 2) ? 1 : 0][(k==1 || k==2) ? 1 : 0]);
                  //q.atText[k].v = pmg->m_afV[(k >= 2) ? 1 : 0];
                  //q.atText[k].h = pmg->m_afH[(k==1 || k==2) ? 1 : 0];
                  //q.apPosn[k].Copy (&pmg->m_apPosn[(k > 2) ? 1 : 0][(k==1 || k==2) ? 1 : 0]);
                  //q.atText[k] = pmg->m_aText[(k > 2) ? 1 : 0][(k==1 || k==2) ? 1 : 0];
               }
            }
            else {
               pq = (PMGQUAD) pmg->m_listMGQUAD.Get(j);
               q = *pq;
            }

            // conver these points from the other spline's coordinate system
            // to this one's
            for (k = 0; k < q.dwPoints; k++) {
               q.apPosn[k].p[3] = 1;
               q.apPosn[k].MultiplyLeft (&pam[dwSurf]);
            }

            // find out limits of this
            if (!BoundToVoxel (&q, &pMin, &pMax, &pVoxelSize, aiBound))
               continue;

            // loop through all of these voxels
            lInter.Clear();
            for (x = (DWORD)aiBound[0][0]; x <= (DWORD) aiBound[0][1]; x++)
            for (y = (DWORD)aiBound[1][0]; y <= (DWORD) aiBound[1][1]; y++)
            for (z = (DWORD)aiBound[2][0]; z <= (DWORD) aiBound[2][1]; z++) {
               if (!apVoxel[x][y][z])
                  continue;

               // add all the quads to our list of ones to check
               for (k = 0; k < apVoxel[x][y][z]->Num(); k++) {
                  dwAddNum = *((DWORD*) apVoxel[x][y][z]->Get(k));
                  lInter.Add (&dwAddNum);
               }
            }

            // not, eliminiate duplicates
            qsort (lInter.Get(0), lInter.Num(), sizeof(DWORD), MGSort);
            
            // loop through all the quads
            DWORD dwLast;
            dwLast = (DWORD)-1;
            for (k = 0; k < lInter.Num(); k++) {
               dwAddNum = *((DWORD*) lInter.Get(k));
               if (dwAddNum == dwLast)
                  continue;
               dwLast = dwAddNum;
               pq = (PMGQUAD) lTempQuad.Get(dwAddNum);

               IntersectTwoPolygons (&pq->apPosn[0], &pq->atText[0], pq->dwPoints,
                  &q.apPosn[0], q.dwPoints, &lLinesTemp);
            }
         } // loop over quads
      } // loop over mesh grids

      // BUGFIX - Merge some lines here to minimize
      LinesMinimize (&lLinesTemp, fExtend);
      plLines->Required (plLines->Num() + lLinesTemp.Num());
      for (x = 0; x < lLinesTemp.Num(); x++) {
         PTEXTUREPOINT pt = (PTEXTUREPOINT) lLinesTemp.Get(x);
         plLines->Add (pt);
      }
   } // over surfaces

   // free all the voxels
   for (x = 0; x < NUMVOXEL; x++)
   for (y = 0; y < NUMVOXEL; y++)
   for (z = 0; z < NUMVOXEL; z++) {
      if (!apVoxel[x][y][z])
         continue;
      delete apVoxel[x][y][z];
   };

#if 0//def _DEBUG
   char szTemp[64];
   OutputDebugString ("Inter:");
   for (j = 0; j < plLines->Num(); j++) {
      PTEXTUREPOINT p = (PTEXTUREPOINT) plLines->Get(j);

      sprintf (szTemp, "(%g,%g) to (%g,%g), ", (double)p[0].h, (double)p[0].v, (double)p[1].h, (double)p[1].v);
      OutputDebugString (szTemp);
   }

   OutputDebugString ("\r\n");
#endif

   // convert the list of lines into list of sequences and return that
   return plLines;
}

/************************************************************************************
CSplineSurface::FindLongestSegment - Called when determining of should draw an border
ribbon between two splines. This function takes a line (in HV coordinates from 0..1
for the spline) from pStart to pEnd. It then searches through the CMeshGrid's
polygons to see if the line is on the edge of any of the polygons. The segment closest
to pStart is then used as the beginning. This is connected with other segments
until pEnd is reached, or no more can be found. The longest segment is then
put intp pFoundStart and pFound and and TRUE is return. If no matching segments can
be found the FALSE is returned.

inputs
   PTEXUREPOINT      pStart - Start searching for
   PTEXTUREPOINT     pEnd - Finished searching for
   PTEXTUREPOINT     pFoundStart - Filled with start that found
   PTEXGTURPOINT     pFoundEnd - Filled witht he end that found
   BOOL              fEdgeOnly - If TRUE, expects line to follow along the edge of
                     polygons. FALSE it can be inside
returns
   BOOL - TRUE if found (and pFoundXXX are valid), or FALSE if not
*/
BOOL CSplineSurface::FindLongestSegment (PTEXTUREPOINT pStart, PTEXTUREPOINT pEnd,
   PTEXTUREPOINT pFoundStart, PTEXTUREPOINT pFoundEnd, BOOL fEdgeOnly)
{
   // make sure the mesh objects are generated
   MakeSureCutouts();

   TEXTUREPOINT tTempStart;


   // find out which direction line heading
   fp fDeltaH, fDeltaV;
   DWORD dwDirH, dwDirV;   // if 1 heading in positive direction, 0 heading negative
   fDeltaH = pEnd->h - pStart->h;
   fDeltaV = pEnd->v - pStart->v;
   dwDirH = (fDeltaH >= 0);
   dwDirV = (fDeltaV >= 0);

   // find the grid size
   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);
   PCListFixed pl;
   pl = *((PCListFixed*) m_listListMG.Get(0));  // get the mesh-grid without overlays removed

   BOOL fFound;
   fFound = FALSE;

   while (TRUE) {
      // if nothing left to check
      if (AreClose (pStart, pEnd))
         return fFound;

      // convert the start into a grid
      fp fH, fV;
      DWORD x, y;
      PCMeshGrid pmg;
      fH = (pStart->h + (dwDirH ? EPSILON : -EPSILON)) * (fp) dwNormH;
      fV = (pStart->v + (dwDirV ? EPSILON : -EPSILON)) * (fp) dwNormV;
      fH = max(0,fH);
      fV = max(0,fV);
      fH = min(dwNormH-1,fH);
      fV = min(dwNormV-1,fV);
      x = (DWORD) fH;
      y = (DWORD) fV;
      pmg = *((PCMeshGrid*) pl->Get(MESHG(x,y)));

      // see where this intersects
      TEXTUREPOINT ts, te;
      BOOL fRet;
      if (fEdgeOnly)
         fRet = pmg->FindLongestSegment (pStart, pEnd, &ts, &te);
      else
         fRet = pmg->FindLongestIntersectSegment (pStart, pEnd, &ts, &te);
      if (!fRet) {
         if (fFound)
            return fFound;

         // else, haven't found it, so try the next mesh grid
         int iNextH, iNextV;
         iNextH = (int)x + (dwDirH ? 1 : 0);
         iNextV = (int)y + (dwDirV ? 1 : 0);
         fp fAlpha, fInter;
         fAlpha = 1;
         if ((iNextH > 0) && (iNextH < (int) dwNormH) && (fabs(fDeltaH) > EPSILON)) {
            fInter = (fp) iNextH / dwNormH;
            fInter = (fInter - pStart->h) / fDeltaH;
            // BUGFIX - Can't use 0 and 1 because of round-off error
            // BUGFIX - Changed from EPSILON to CLOSE
            if ((fInter > CLOSE) && (fInter < 1.0 - CLOSE))
               fAlpha = min(fAlpha, fInter);
         }
         if ((iNextV > 0) && (iNextV < (int) dwNormV) && (fabs(fDeltaV) > EPSILON)) {
            fInter = (fp) iNextV / dwNormV;
            fInter = (fInter - pStart->v) / fDeltaV;
            // BUGFIX - Can't use 0 and 1 because of round-off error
            // BUGFIX - Changed from EPSILON to CLOSE
            if ((fInter > CLOSE) && (fInter < 1.0 - CLOSE))
               fAlpha = min(fAlpha, fInter);
         }
         if (fAlpha >= 1-EPSILON)
            return FALSE;  // doesnt intersect with anything

         // else, have a new start
         TEXTUREPOINT t;
         t.h = (1.0 - fAlpha) * pStart->h + fAlpha * pEnd->h;
         t.v = (1.0 - fAlpha) * pStart->v + fAlpha * pEnd->v;
         tTempStart = t;
         pStart = &tTempStart;



         continue;
      }

      // if this is the first time then use these
      if (!fFound) {
         fFound = TRUE;
         *pFoundStart = ts;
         *pFoundEnd = te;
         pStart = pFoundEnd;
         continue;
      }
      else {
         // BUGFIX - If the old end is not close to the new start then just return
         // because have a gap
         if (!AreClose (pFoundEnd, &ts))
            return TRUE;
      }

      // else, append to the end
      *pFoundEnd = te;
      pStart = pFoundEnd;
   }  // end while true
}

/************************************************************************************
CSplineSurface::FindMinMaxH - Looks through the obejct with cutouts and
finds the minimum and maximum HV values that occurs. If this is not 0..1,0..1 then
the cutouts remove enough that some of the edge completely removed, so potentially
the spline could be shrunk without any problem.

inputs
   PTEXTUREPOINT     pMin, pMax - Filled with the minimum and maximum values
returns
   BOOL - TRUE if found min,max. If FALSE then can't find anything.
*/
BOOL CSplineSurface::FindMinMaxHV (PTEXTUREPOINT pMin, PTEXTUREPOINT pMax)
{
   // make sure the mesh objects are generated
   MakeSureCutouts();

   // find the grid size
   DWORD dwNormH, dwNormV;
   dwNormH = m_dwMeshH - (m_fLoopH ? 0 : 1);
   dwNormV = m_dwMeshV - (m_fLoopV ? 0 : 1);
   PCListFixed pl;
   PCMeshGrid pmg;
   pl = *((PCListFixed*) m_listListMG.Get(0));  // get the mesh-grid without overlays removed

   BOOL fFound;
   fFound = FALSE;

   DWORD x;
   for (x = 0; x < dwNormH * dwNormV; x++) {
      pmg = *((PCMeshGrid*) pl->Get(x));
      TEXTUREPOINT tMin, tMax;
      if (!pmg->FindMinMaxHV (&tMin, &tMax))
         continue;

      if (!fFound) {
         fFound = TRUE;
         *pMin = tMin;
         *pMax = tMax;
         continue;
      }

      // else
      pMin->h = min(pMin->h, tMin.h);
      pMin->v = min(pMin->v, tMin.v);
      pMax->h = max(pMax->h, tMax.h);
      pMax->v = max(pMax->v, tMax.v);

   }

   return fFound;
}


/*****************************************************************************************
ClipAgainstBox - Clips the line from p1 to p2 against the border of the surface (0,0) to (1,1)

inputs
   PTEXTUREPOINT  p1, p2 - Points that define line. These are fill with the clipped version
retruns
   BOOL - TRUE if line exists, FALSE if totally clipped out
*/
BOOL ClipAgainstBox (PTEXTUREPOINT p1, PTEXTUREPOINT p2)
{
   BOOL  oc[2][4];
   fp   bc[2][4];
   fp alpha1, alpha2, alpha;
   TEXTUREPOINT newStart, newEnd;
   DWORD i,j;

   bc[0][3] = p1->h;
   bc[1][3] = p2->h;
   bc[0][2] = 1 - p1->h;
   bc[1][2] = 1 - p2->h;
   bc[0][1] = p1->v;
   bc[1][1] = p2->v;
   bc[0][0] = 1 - p1->v;
   bc[1][0] = 1 - p2->v;

   for (i = 0; i < 4; i++)
      for (j = 0; j < 2; j++)
         oc[j][i] = (bc[j][i] < 0.0);

   // if all values are false then trivial accept
   j = 0;
   for (i = 0; i < 4; i++)
      if (oc[0][i] || oc[1][i])
         j++;
   if (j == 0) {
      return TRUE;
   }

   // if any values and to true then trivial reject
   j = 0;
   for (i = 0; i < 4; i++)
      if (oc[0][i] && oc[1][i])
         j++;
   if (j > 0) {
      return FALSE;  // trivial reject
      }

   // neither tribial reject nor accept
   alpha1 = 0.0;
   alpha2 = 1.0;

   for (i = 0; i < 4; i++)
      if ((oc[0][i] && !oc[1][i]) || (!oc[0][i] && oc[1][i])) { // XOR
         // the line stadles this boundary
         // alpha = BCli / (BCli - BC2i

         alpha = bc[0][i] / (bc[0][i] - bc[1][i]);

         if (oc[0][i])
            alpha1 = max(alpha1, alpha);
         else
            alpha2 =min(alpha2, alpha);
      }

   if (alpha1 > alpha2) {
      return FALSE;  // don't draw a line
   }

   newStart.h = p1->h * (1.0 - alpha1) + p2->h * alpha1;
   newStart.v = p1->v * (1.0 - alpha1) + p2->v * alpha1;
   newEnd.h = p1->h * (1.0 - alpha2) + p2->h * alpha2;
   newEnd.v = p1->v * (1.0 - alpha2) + p2->v * alpha2;

   *p1 = newStart;
   *p2 = newEnd;

   return TRUE;
}

#if 0 // doesnt work
{
   fp fAlpha1, fAlpha2, fAlpha;

   // start out not clipping
   fAlpha1 = 0;
   fAlpha2 = 1;

   // delta
   TEXTUREPOINT tDelta;
   tDelta.h = p2->h - p1->h;
   tDelta.v = p2->v - p1->v;

   if (fabs(tDelta.h) > EPSILON) {
      // could interset against left and right
      fAlpha = (0 - p1->h) / tDelta.h;
      if (tDelta.h > 0)
         fAlpha1 = max(fAlpha1, fAlpha);
      else
         fAlpha1 = min(fAlpha1, fAlpha);
      fAlpha = (1 - p1->h) / tDelta.h;
      if (tDelta.h > 0)
         fAlpha2 = min(fAlpha2, fAlpha);
      else
         fAlpha2 = max(fAlpha2, fAlpha);
   }
   if (fabs(tDelta.v) > EPSILON) {
      // could interset against left and right
      fAlpha = (0 - p1->v) / tDelta.v;
      if (tDelta.v > 0)
         fAlpha1 = max(fAlpha1, fAlpha);
      else
         fAlpha1 = min(fAlpha1, fAlpha);
      fAlpha = (1 - p1->v) / tDelta.v;
      if (tDelta.v > 0)
         fAlpha2 = min(fAlpha2, fAlpha);
      else
         fAlpha2 = max(fAlpha2, fAlpha);
   }
   if (fAlpha1 >= fAlpha2)
      return FALSE;

   // intersect
   TEXTUREPOINT tS;
   tS = *p1;
   p1->h = fAlpha1 * tDelta.h + tS.h;
   p1->v = fAlpha1 * tDelta.v + tS.v;
   p2->h = fAlpha2 * tDelta.h + tS.h;
   p2->v = fAlpha2 * tDelta.v + tS.v;

   return TRUE;
}
#endif // 0


/***************************************************************************
ExtendSegment - Extends the line segment on the side specified.

inputs
   PTEXTUREPOINT     lLine - Pointer to array of two TEXTUREPOINT.
      Start and end of line. Extended in place
   DWORD             dwSide - to extend. 0 for pLine[0], 1 for pLine[1]
   fp            fAmt - Length to extend by
   BOOL              fKeepOnlyIfEdge - Set to true if want to keep this extension
                     only if it extends to an edge. Else, if FALSE, always keep

   NOTE: This only extends the segment if it makes it clip against one of the edges
returns
   none
*/
void ExtendSegment (PTEXTUREPOINT pLine, DWORD dwSide, fp fAmt, BOOL fKeepOnlyIfEdge)
{

   TEXTUREPOINT aLine[2];
   aLine[0] = pLine[0];
   aLine[1] = pLine[1];

   // else, extend the line
   TEXTUREPOINT pDelta;
   fp fLen;
   pDelta.h = aLine[1].h - aLine[0].h;
   pDelta.v = aLine[1].v - aLine[0].v;
   fLen = sqrt(pDelta.h * pDelta.h + pDelta.v * pDelta.v);
   if (fLen > EPSILON) {
      fLen = 1.0 / fLen * (fAmt * (dwSide ? 1 : -1));
      pDelta.h *= fLen;
      pDelta.v *= fLen;
   }
   aLine[dwSide].h += pDelta.h;
   aLine[dwSide].v += pDelta.v;
   ClipAgainstBox (aLine + 0, aLine + 1);

   BOOL fKeep;
   fKeep = !fKeepOnlyIfEdge;
   if ((aLine[dwSide].h <= EPSILON) || (aLine[dwSide].v <= EPSILON) ||
      (aLine[dwSide].h >= 1-EPSILON) || (aLine[dwSide].v >= 1-EPSILON))
      fKeep = TRUE;
   if (fKeep) {
      pLine[0] = aLine[0];
      pLine[1] = aLine[1];
   }
}

// BUGFIX - Disable optimizations in thsi one because something in the optimizations
// is breaking the roof intersection code. If maximize speed optimization (but not
// default or disable) then when two roof sections (such as half-hip roof) intersect
// they're not properly clipped against one another
// #pragma optimize ("", off)


/************************************************************************************
CSplineSurface::IntersectWithSurfacesAndCutout - Intersects this surface
with one or more surfaces.

inputs
   PCSplineSurface      *papss - Pointer to an array of surfaces to test against
   PCMatrix             paMatrix - Pointer to an array of matricies that convert
                        from papss space into this spline's space
   DWORD                dwNum - Number of papss and paMatrix
   PWSTR                pszName - Name to use for the cutout
   BOOL                 fIgnoreCutouts - If TRUE then looks at the entire sheet of papss.
                        If FALSE, use only the post-cutout version.
   BOOL                 fClockwise - If TRUE use clockwise border, and clockwise orientation
   DWORD                dwMode -
                           LOWORD values
                           0 - Don't change the orientation of intersections
                           1 - reorient the intersection so the smallest
                              cutout area (estimated) for that one intersection, is on the left.
                              The reorient function
                              only really works if there's just one sequence.
                           2 - Use pKeep

                           0x10000 - If this bit set then toss out any sequences that don't
                           start and end at an edge
   BOOL                 fUseMin - If TRUE, use the minimum size result, FALSE use maximum.
                           Note: If !fUseMin then won't draw a loop around the edge of
                           the surface
   BOOL                 fFinalClockwise - If TRUE, want the final loop to be clockwise
                           (cut out the inside), or counter clockwise (cut outside out).
   PTEXTUREPOINT        pKeep - if dwMode==2, then when intersect, aways keep the direction
                           such that pKeep is going to be kept
returns
   BOOL - TRUE if success
*/
BOOL CSplineSurface::IntersectWithSurfacesAndCutout (CSplineSurface **papss,
      PCMatrix paMatrix, DWORD dwNum, PWSTR pszName, BOOL fIgnoreCutouts,
      BOOL fClockwise, DWORD dwMode, BOOL fUseMin, BOOL fFinalClockwise,
      PTEXTUREPOINT pKeep)
{
   // create a default list of lines
   CListFixed  lLines;
   lLines.Init (sizeof(TEXTUREPOINT) * 2);

   // clear the old cutout
   CutoutRemove (pszName);

   // loop through all the surfaces
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      PCListFixed pNew;
      pNew = IntersectWithSurfaces (0.0, 1, papss+i, paMatrix+i, fIgnoreCutouts);
      if (!pNew)
         continue;   // no intersections

      // consider flipping these
      // convert this to sequences
      PCListFixed plSeq;
      plSeq = LineSegmentsToSequences (pNew);
      if (!plSeq)
         goto donereorient;
      if (!plSeq->Num())
         goto deletesequences;

      PTEXTUREPOINT pt1, pt2;
      PCListFixed pl;

      // remove any flips from these sequence
      if (plSeq->Num() > 1)
         SequencesLookForFlips (plSeq);

      // if there's more than one sequence choose the longest
      DWORD dwBest;
      fp fLenBest, fLen;
      for (j = 0; j < plSeq->Num(); j++) {
         pl = *((PCListFixed*) plSeq->Get(j));

         // BUGFIX - extend at least by a little bit. Otherwise the cone roof (which
         // has some gaps) doesn't clip out the walls
         pt1 = (PTEXTUREPOINT) pl->Get(0);
         ExtendSegment (pt1, 0, .02, FALSE); // slight extension
         pt1 = (PTEXTUREPOINT) pl->Get(pl->Num()-1);
         ExtendSegment (pt1-1, 1, .02, FALSE); // slight extension

         // because clipping one surface against another sometimes leaves a line sequences
         // that comes almost up to the edge, but not quite, remember the end poitns
         // of the sequence so that we can see if want to extend them later
         // Dont boher adding if it's on the edge
         pt1 = (PTEXTUREPOINT) pl->Get(0);
         if (!( (pt1->h <= 0) || (pt1->h >= 1) || (pt1->v <= 0) || (pt1->v >= 1)))
            ExtendSegment (pt1, 0, .05, TRUE);  // MAGIC NUMBER: .05 is amount to extend
         pt1 = (PTEXTUREPOINT) pl->Get(pl->Num()-1);
         if (!( (pt1->h <= 0) || (pt1->h >= 1) || (pt1->v <= 0) || (pt1->v >= 1)))
            ExtendSegment (pt1-1, 1, .05, TRUE);  // MAGIC NUMBER: .05 is amount to extend
         // BUGFIX - Magic number was set to .2, but that seems too much, so reduced.
         // Did this at same time stopped passing 0x10000 to dwMode for edge check.
         // Originally the 0x10000 was to fix 3-gable roof, but seems that with more
         // recent changes that may not be necessary
        
         pt1 = (PTEXTUREPOINT) pl->Get(0);
         pt2 = (PTEXTUREPOINT) pl->Get(pl->Num()-1);
         fLen = (pt1->h - pt2->h) * (pt1->h - pt2->h) + (pt1->v - pt2->v) * (pt1->v - pt2->v);
         if (!j || (fLen > fLenBest)) {
            dwBest = j;
            fLenBest = fLen;
         }
      }
#ifdef _DEBUG
      if (plSeq->Num() > 1)
         OutputDebugString ("WARNING: More than one path in IntersectWithSurfacesAndCutout");
#endif

      // find the starting and ending position
      pl = *((PCListFixed*) plSeq->Get(dwBest));

      // find the area tot he left (as the line direction goes)
      BOOL fLeftSmallest;
      fLeftSmallest = TRUE;
      if (LOWORD(dwMode) == 1) {   // by area
         fp fArea, fTotal;
         fp fDeltaH, fDeltaV;
         fTotal = 0;
         for (j = 0; j+1 < pl->Num(); j++) {
            pt1 = (PTEXTUREPOINT) pl->Get(j);
            pt2 = pt1+1;
            fDeltaH = pt2->h - pt1->h;
            fDeltaV = pt2->v - pt1->v;


            fArea = fabs(fDeltaH * fDeltaV) / 2 + fabs(fDeltaV) * min(pt1->h, pt2->h);
            if (fDeltaV < 0)
               fTotal += fArea;
            else
               fTotal -= fArea;
         }

         // adjust the area based on starting points
         pt1 = (PTEXTUREPOINT) pl->Get(0);
         pt2 = (PTEXTUREPOINT) pl->Get(pl->Num()-1);
         fDeltaH = pt2->h - pt1->h;
         fDeltaV = pt2->v - pt1->v;
         if (fDeltaH > 0) {
            fArea = pt2->v;   // include top
            fTotal += fArea;
         }
         else {
            fArea = 1.0 - pt1->v; // lower part
            fTotal += fArea;
         }
#ifdef _DEBUG
         if ((fArea < -EPSILON) || (fArea > 1+EPSILON))
            OutputDebugString ("ASSERT: IntersectWithSurfacesAndCutout");
#endif

         // which direction do we want to go
         fLeftSmallest = (fTotal < .5);
      }
      else if (LOWORD(dwMode) == 2) {
         // basically - using the line there, and the point in question, create
         // a polygon. If it's clockwsie (and if we want it to be clockwise) then
         // no problem. Otherwise flip it

         // temporarily add the point in question to the list
         pl->Add (pKeep);

         fLeftSmallest = IsCurveClockwise((PTEXTUREPOINT)pl->Get(0), pl->Num());

         // remove the temporary point
         pl->Remove(pl->Num()-1);
/* This doesn't work properly - misses a few bits, such as in balinese roof
         // find all the points to the left/right of the contorl point
         int iScore;
         fp fMin, fMax;
         iScore = 0;
         for (j = 0; j+1 < pl->Num(); j++) {
            pt1 = (PTEXTUREPOINT) pl->Get(j);
            pt2 = pt1+1;

            fMin = min(pt1->v, pt2->v);
            fMax = max(pt1->v, pt2->v);
            if ((pKeep->v <= fMin) || (pKeep->v > fMax))
               continue;   // doesn't intersect

            // where would this occur
            fp fAlpha, fX;
            fAlpha = (pKeep->v - pt1->v) / (pt2->v - pt1->v);
            fX = fAlpha * (pt2->h - pt1->h) + pt1->h;
            if (pKeep->h < fX)
               continue;   // ignore since the line is to the right

            if (pt2->v < pt1->v)
               iScore--;   // going up
            else
               iScore++; // going down
         }

         // adjust the area based on starting points
         pt1 = (PTEXTUREPOINT) pl->Get(0);
         pt2 = (PTEXTUREPOINT) pl->Get(pl->Num()-1);
         fMin = min(pt1->v, pt2->v);
         fMax = max(pt1->v, pt2->v);
         int iLastPoint;
         iLastPoint = 1;
         if (pt2->h > pt1->h) // going right
            iLastPoint *= -1;
         //if (p2->v > pt1->v)  // going down
         //   iLastPoint *= -1;
         if (pKeep->v <= fMin)   // above
            iLastPoint *= -1;
         else if (pKeep->v > fMax)  // below
            iLastPoint *= 1;
         else 
            iLastPoint = 0;
         iScore += iLastPoint;
         // if no score then we're within fMin and fMax, decide based on direction
         if (!iScore) {
            iScore = (pt2->v < pt1->v) ? 1 : -1;
         }

         // assuming that taking inside the clockwise, reverse if iScore > 0
         fLeftSmallest = !(iScore > 0);

         // if not going clockwise then reverse
         if (!fClockwise)
            fLeftSmallest = !fLeftSmallest;
*/
      }

      // if the left isn't the smallest then reverse when add
      // As a bit of an optimiation, since already converted into
      // sequences and removed some redundant points, use this to
      // our advantage
      TEXTUREPOINT atp[2];
      DWORD k;
      for (j = 0; j < plSeq->Num(); j++) {
         pl = *((PCListFixed*) plSeq->Get(j));

         // BUGFIX - Only take one of these. I think this is ok. Doing the fix so
         // balinese curved roofs work
         if (j != dwBest)
            continue;

         // if dwMode & 0x10000 then check to make sure start/end at corner
         if (dwMode & 0x10000) {
            BOOL fEdge1, fEdge2;
            pt1 = (PTEXTUREPOINT) pl->Get(0);
            pt2 = (PTEXTUREPOINT) pl->Get(pl->Num()-1);
            fEdge1 = (pt1->h < EPSILON) || (pt1->h > 1-EPSILON) || (pt1->v < EPSILON) || (pt1->v > 1-EPSILON);
            fEdge2 = (pt2->h < EPSILON) || (pt2->h > 1-EPSILON) || (pt2->v < EPSILON) || (pt2->v > 1-EPSILON);
            if (!fEdge1 || !fEdge2)
               continue;
         }

         for (k = 0; k+1 < pl->Num(); k++) {
            pt1 = (PTEXTUREPOINT) pl->Get(k);

            memcpy (atp, pt1, sizeof(atp));
            if (!fLeftSmallest) {
               TEXTUREPOINT t;
               t = atp[0];
               atp[0] = atp[1];
               atp[1] = t;
            }
            lLines.Add (&atp);
         }
      }


deletesequences:
      // delete the sequences
      for (j = 0; j < plSeq->Num(); j++) {
         pl = *((PCListFixed*) plSeq->Get(j));
         delete pl;
      }
      delete plSeq;

donereorient:
      delete pNew;
   }

   // add an outline
   if (fUseMin) {
      // if add a perimiter, need to eliminate any lines walking along the edge
      // because they end up creating duplicates - sometimes doing the wrong direction
      // which causes other problems.
      for (i = lLines.Num()-1; i < lLines.Num(); i--) {
         PTEXTUREPOINT ptp = (PTEXTUREPOINT) lLines.Get(i);
         BOOL fRemove = FALSE;

         if ((ptp[0].h < EPSILON) && (ptp[1].h < EPSILON))
            fRemove = TRUE;
         if ((ptp[0].v < EPSILON) && (ptp[1].v < EPSILON))
            fRemove = TRUE;
         if ((ptp[0].h > 1- EPSILON) && (ptp[1].h >1 - EPSILON))
            fRemove = TRUE;
         if ((ptp[0].v > 1- EPSILON) && (ptp[1].v >1 - EPSILON))
            fRemove = TRUE;

         if (fRemove)
            lLines.Remove (i);
      }

      // add the edge of this loop
      TEXTUREPOINT A[2];
      A[0].h = 0;
      A[0].v = 0;
      if (!fClockwise) {
         lLines.Required (lLines.Num() + 4);

         A[1].h = 0;
         A[1].v = 1;
         lLines.Add (&A);
         A[0] = A[1];
         A[1].h = 1;
         lLines.Add (&A);
         A[0] = A[1];
         A[1].v = 0;
         lLines.Add (&A);
         A[0] = A[1];
         A[1].h = 0;
         lLines.Add (&A);
      }
      else {
         lLines.Required (lLines.Num() + 4);

         A[1].h = 1;
         A[1].v = 0;
         lLines.Add (&A);
         A[0] = A[1];
         A[1].v = 1;
         lLines.Add (&A);
         A[0] = A[1];
         A[1].h = 0;
         lLines.Add (&A);
         A[0] = A[1];
         A[1].v = 0;
         lLines.Add (&A);
      }
   }

   // convert to sequences
   PCListFixed pSeq;
   pSeq = LineSegmentsToSequences (&lLines);
   if (!pSeq)
      return NULL;

   // get rid of dangling sequences
   LineSequencesRemoveHanging (pSeq, FALSE);


   CListVariable lv;
   DWORD       dwTemp[100];

   // trim down the number of sequneces so have fewer paths
   //SequencesTrimByScores (pSeq, !fClockwise);

   PCListFixed plScores;
   plScores = SequencesSortByScores (pSeq, !fClockwise);
   if (!plScores)
      return TRUE;
   PathFromSequences (pSeq, &lv, dwTemp, 0, sizeof(dwTemp)/sizeof(DWORD));

   DWORD dwBest;
   if (!fUseMin)
      dwBest = PathWithHighestScore (pSeq, &lv, plScores);
   else
      dwBest = PathWithLowestScore (pSeq, &lv, plScores);
   delete plScores;

   if (dwBest != (DWORD) -1) {
      PCListFixed pEdge;
      pEdge = PathToSplineList (pSeq, &lv, dwBest, fClockwise != fFinalClockwise);
         // Reversing this so goes around counter clockwise

      if (pEdge) {
         CListFixed ltp;
         ltp.Init (sizeof(TEXTUREPOINT));
         TEXTUREPOINT tp;
         PCPoint pp;
         pp = (PCPoint) pEdge->Get(0);
         ltp.Required (pEdge->Num());
         for (j = 0; j < pEdge->Num(); j++) {
            tp.h = pp[j].p[0];
            tp.v = pp[j].p[1];
            ltp.Add (&tp);
         }

         // if it's not the direction we want then force it
         PTEXTUREPOINT ptc;
         DWORD dwc;
         ptc = (PTEXTUREPOINT) ltp.Get(0);
         dwc = ltp.Num();
         if (IsCurveClockwise(ptc, dwc) != fFinalClockwise)
            for (j = 0; j < dwc/2; j++) {
               tp = ptc[j];
               ptc[j] = ptc[dwc-j-1];
               ptc[dwc-j-1] = tp;
            }


         CutoutSet (pszName, (PTEXTUREPOINT) ltp.Get(0),
            ltp.Num(), fFinalClockwise);

         delete pEdge;
      }
   }

   // Delete sequences
   for (j = 0; j < pSeq->Num(); j++) {
      PCListFixed pl = *((PCListFixed*) pSeq->Get(j));
      delete pl;
   }
   delete pSeq;

   return TRUE;
}
// BUGFIX - Disable optimizations in thsi one because something in the optimizations
// is breaking the roof intersection code. If maximize speed optimization (but not
// default or disable) then when two roof sections (such as half-hip roof) intersect
// they're not properly clipped against one another
// #pragma optimize ("", on)

/**********************************************************************************
CSplineSurface::CloneAndExtend - Clone the surface and then extend it just a bit
to the left and right, top and bottom. This extension improves intersections, since
won't get an intersection that's close to the edge but not quite.

NOTE: This cloning gets rid of the cutouts and overlays.

inputs
   fp   fExtendL, fExtendR - Amount to extend on left/right side (H == 0 or H ==1). Positive #'s larger
   fp   fExtendU, fExtendD - Amount to extend on top/bottom side (V == 0 or V == 1)
   BOOL     fFlip - If TRUE, flip along left/right axis.
   BOOL     fRemoveCutouts - If TRUE, remove the cutouts when clone. NOTE: If !fRemoveCutouts
            then don't use fExtendLR or fExtendUD
returns
   PCSplineSurface - new surface
*/
CSplineSurface *CSplineSurface::CloneAndExtend (fp fExtendL, fp fExtendR, fp fExtendU, fp fExtendD, BOOL fFlip,
                                                BOOL fRemoveCutouts)
{
   PCSplineSurface pNew = new CSplineSurface;
   if (!pNew)
      return NULL;
   CloneTo (pNew);

   // delete all the cutouts and stuff
   PWSTR psz;
   PTEXTUREPOINT ptp;
   DWORD dwNum;
   BOOL fClockwise;
   if (fRemoveCutouts) {
      while (pNew->CutoutEnum (0, &psz, &ptp, &dwNum, &fClockwise))
         pNew->CutoutRemove (psz);
   }
   while (pNew->OverlayEnum (0, &psz, &ptp, &dwNum, &fClockwise))
      pNew->OverlayRemove (psz);

   // flip?
   DWORD x, y, dwWidth, dwHeight;
   CPoint pt1, pt2;
   DWORD dw1, dw2;
   dwWidth = pNew->ControlNumGet (TRUE);
   dwHeight = pNew->ControlNumGet (FALSE);
   if (fFlip) {
      for (x = 0; x < dwWidth/2; x++) for (y = 0; y < dwHeight; y++) {
         pNew->ControlPointGet (x, y, &pt1);
         pNew->ControlPointGet (dwWidth - x - 1, y, &pt2);
         pNew->ControlPointSet (x, y, &pt2);
         pNew->ControlPointSet (dwWidth - x - 1, y, &pt1);
      }

      DWORD dwSegWidth;
      dwSegWidth = dwWidth;
      if (!pNew->LoopGet(TRUE))
         dwSegWidth--;
      for (x = 0; x < dwSegWidth/2; x++) {
         dw1 = pNew->SegCurveGet (TRUE, x);
         dw2 = pNew->SegCurveGet (TRUE, dwSegWidth - x - 1);

         // for side B flip left to right, and also take care to flip circle and ellipse
         switch (dw1) {
         case SEGCURVE_CIRCLEPREV:
            dw1 = SEGCURVE_CIRCLENEXT;
            break;
         case SEGCURVE_CIRCLENEXT:
            dw1 = SEGCURVE_CIRCLEPREV;
            break;
         case SEGCURVE_ELLIPSEPREV:
            dw1 = SEGCURVE_ELLIPSENEXT;
            break;
         case SEGCURVE_ELLIPSENEXT:
            dw1 = SEGCURVE_ELLIPSEPREV;
            break;
         }
         switch (dw2) {
         case SEGCURVE_CIRCLEPREV:
            dw2 = SEGCURVE_CIRCLENEXT;
            break;
         case SEGCURVE_CIRCLENEXT:
            dw2 = SEGCURVE_CIRCLEPREV;
            break;
         case SEGCURVE_ELLIPSEPREV:
            dw2 = SEGCURVE_ELLIPSENEXT;
            break;
         case SEGCURVE_ELLIPSENEXT:
            dw2 = SEGCURVE_ELLIPSEPREV;
            break;
         }

         pNew->SegCurveSet (TRUE, x, dw2);
         pNew->SegCurveSet (TRUE, dwSegWidth - x - 1, dw1);
      }
   }  // if f flip

   // extend the top and bottom
   if (fExtendU || fExtendD) for (x = 0; x < dwWidth; x++) {
      // get the point and extend it up.
      // do in this round about way because wall may be warped
      pNew->ControlPointGet (x, 0, &pt1);
      pNew->ControlPointGet (x, 1, &pt2);
      pt2.Subtract (&pt1);
      pt2.Normalize();
      pt2.Scale (-fExtendU);
      pt1.Add (&pt2);
      pNew->ControlPointSet (x, 0, &pt1);

      // get the point and extend it up.
      // do in this round about way because wall may be warped
      pNew->ControlPointGet (x, dwHeight-1, &pt1);
      pNew->ControlPointGet (x, dwHeight-2, &pt2);
      pt2.Subtract (&pt1);
      pt2.Normalize();
      pt2.Scale (-fExtendD);
      pt1.Add (&pt2);
      pNew->ControlPointSet (x, dwHeight-1, &pt1);
   }

   // extend left and right
   if (fExtendL || fExtendR) for (y = 0; y < dwHeight; y++) {
      // get the point and extend it up.
      // do in this round about way because wall may be warped
      pNew->ControlPointGet (0, y, &pt1);
      pNew->ControlPointGet (1, y, &pt2);
      pt2.Subtract (&pt1);
      pt2.Normalize();
      pt2.Scale (-fExtendL);
      pt1.Add (&pt2);
      pNew->ControlPointSet (0, y, &pt1);

      // get the point and extend it up.
      // do in this round about way because wall may be warped
      pNew->ControlPointGet (dwWidth-1, y, &pt1);
      pNew->ControlPointGet (dwWidth-2, y, &pt2);
      pt2.Subtract (&pt1);
      pt2.Normalize();
      pt2.Scale (-fExtendR);
      pt1.Add (&pt2);
      pNew->ControlPointSet (dwWidth-1, y, &pt1);
   }

   return pNew;
}


