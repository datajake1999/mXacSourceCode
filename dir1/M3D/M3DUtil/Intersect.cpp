/********************************************************************************
Intersect.cpp - Intersection routines

begun 5/9/2001 by Mike Rozak.
Copyrighg 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"


/*****************************************************************************
DistancePointToPoint - Returns the distance between two points.

inputs
   CPoint      pP1, pP2 - Two points
returns
   fp - distance
*/
fp DistancePointToPoint (PCPoint pP1, PCPoint pP2)
{
   CPoint p;
   p.Subtract (pP1, pP2);
   return p.Length();
}

/*****************************************************************************
DistancePointToLine - Finds the shortest distance between point pPoint and
the line defined by pLineStart and pLineVector.

inputs
   PCPoint     pPoint - Point to use
   PCPoint     pLineStart - A point on the line
   PCPoint     pLineVector - A vector indicating which direction the line goes in.
                  This does not have to be normalized becase this function
                  will normalize a version for itself.
   PCPoint     pNearest - Filled in with the lication of the nearest point.
returns
   fp - distance
*/
fp DistancePointToLine (PCPoint pPoint, PCPoint pLineStart, PCPoint pLineVector, PCPoint pNearest)
{
   CPoint pLVN;
   pLVN.Copy (pLineVector);
   pLVN.Normalize();

   CPoint pToPoint;
   pToPoint.Subtract (pPoint, pLineStart);
   
   fp fAlpha;
   fAlpha = pToPoint.DotProd (&pLVN);

   pNearest->Copy (&pLVN);
   pNearest->Scale (fAlpha);
   pNearest->Add (pLineStart);

   return DistancePointToPoint (pNearest, pPoint);
}

/*****************************************************************************
DistancePointToPlane - Figures out the distance beteen the point pPoint
and the plane defined by pPlaneAnchor (a point in the plane), and two
bectors pHVector and pVVector.

inputs  
   PCPoint     pPoint - Point
   PCPoint     pPlaneAnchor - Point in the plane
   PCPoint     pHVector, pVVector - Two vectors that define the plane.
               They MUST be orthogonal, although they dont have to be normalized
   PCPoint     pNearest - Filled in with the nearest point as is appears on the plane.
returns
   fp - Distance
*/
fp DistancePointToPlane (PCPoint pPoint, PCPoint pPlaneAnchor, PCPoint pHVector, PCPoint pVVector, PCPoint pNearest)
{
   // normalize the H and V vectors
   CPoint pH, pV;
   pH.Copy (pHVector);
   pV.Copy (pVVector);
   pH.Normalize ();
   pV.Normalize();

   CPoint pToPoint;
   pToPoint.Subtract (pPoint, pPlaneAnchor);
   
   fp fAlphaH, fAlphaV;
   fAlphaH = pToPoint.DotProd (&pH);
   fAlphaV = pToPoint.DotProd (&pV);

   pH.Scale (fAlphaH);
   pV.Scale (fAlphaV);
   pNearest->Add (&pH, &pV);
   pNearest->Add (pPlaneAnchor);

   return DistancePointToPoint (pNearest, pPoint);
}


/********************************************************************************************
IntersectLinePlane - Intersect a line with a plane.

inputs
   PCPoint     pLineStart - start of the line
   PCPoint     pLineEnd - End of line
   PCPoint     pPlane - Point in plane
   PCPoint     pPlaneNormal - Normal of the plane
   PCPoint     pIntersect - Filled with the point where they intersct
returns
   BOOL - TRUE if intersect, FALSE if dont
*/
BOOL IntersectLinePlane (PCPoint pLineStart, PCPoint pLineEnd, PCPoint pPlane,
                         PCPoint pPlaneNormal, PCPoint pIntersect)
{
   // find the longest dimension between pNC and pC
   CPoint   pSub;
   pSub.Subtract (pLineEnd, pLineStart);

   // find out where it crosses the plane on the dwLongest dimension
   // solve alpha = ((P-Q).v) / (w.v), P=point on plalne, Q=pLineStart, w=pLineEnd-pLineStart, v=Plane normal
   CPoint pPQ;
   pPQ.Subtract (pPlane, pLineStart);
   fp fAlpha;
   fAlpha = pSub.DotProd (pPlaneNormal);
   if (fabs(fAlpha) < EPSILON*10)   // BUGFIX - Was EPSILON, but problems when using with float accuracy
      return FALSE;  // doens't intersect
   fAlpha = pPQ.DotProd(pPlaneNormal) / fAlpha;

   // find out where intersects
   pIntersect->Copy (&pSub);
   pIntersect->Scale(fAlpha);
   pIntersect->Add (pLineStart);
   return TRUE;

}


/********************************************************************************************
IntersectLineQuad - Intersects a line with a quadrilateral.

inputs
   PCPoint     pLineStart - start of the line
   PCPoint     pSub - Direction of line. End of line minus start of line.
   PCPoint     pUL, pUR, pLR, pLL - Points
   PCPoint     pIntersect - Location where intersect. Only filled in if intersect
   fp      *pfAlpha - Filled with the alpha where the line intersects the plane.
                  At 0 it intersects at linestart, at 1 at lineend. Can be NULL.
   PTEXTUREPOINT pHV - Filled in with HV location of intersection (if intersects). 0,0 = UL,
                  1,0 = UR, 1,1=LR, 0,1=LL. Can be NULL.
   BOOL        *pfIntersectA - Can be NULL. Otherwise will fill in with what triangle
                  it intersected. If TRUE it intersected A (pUL, pUR, pLR), if FALSE
                  it intersected B (pLR, pLL, pUL). This is only filled in if an intersection
                  occurred
   DWORD          dwFlags - Some extra control flags that control intersections:
                     0x01 - If set then an exact match is required. Otherwise, if the ray
                              comes close to the edge it's accepted
                     0x02 - If set then the intersection against A is skipped
                     0x04 - If set then the intersection against B is skipped
returns
   BOOL - TRUE if intersect
*/
BOOL IntersectLineQuad (PCPoint pLineStart, PCPoint pSub,
                        PCPoint pUL, PCPoint pUR, PCPoint pLR, PCPoint pLL,
                        PCPoint pIntersect, fp *pfAlpha, PTEXTUREPOINT pHV,
                        BOOL *pfIntersectA, DWORD dwFlags)
{
   BOOL fInterA, fInterB;
   fp fAlphaA, fAlphaB;
   CPoint pInterA, pInterB;
   TEXTUREPOINT HVA, HVB;

   // NOTE - If ever draw quads as 4 triangles (instead of 2) will need to change
   // this code

   // intersect with plane A
   if (!(dwFlags & 0x02))
      fInterA = IntersectLineTriangle (pLineStart, pSub, pUL, pUR, pLR,
         &pInterA, &fAlphaA, &HVA, ((dwFlags & 0x01) ? 0 : 0x20) | 0x07);
   else
      fInterA = FALSE;

   if (!(dwFlags & 0x04))
      fInterB = IntersectLineTriangle (pLineStart, pSub,  pLR, pLL, pUL,
         &pInterB, &fAlphaB, &HVB, ((dwFlags & 0x01) ? 0 : 0x20) | 0x07);
   else
      fInterB = FALSE;

   if (!fInterA && !fInterB)
      return FALSE;

   // BUGFIX - Calculate the length of either side and use this to create a ratio
   CPoint pD;
   fp fTop, fRight, fBottom, fLeft;
   pD.Subtract (pUL, pUR);
   fTop = pD.Length();
   pD.Subtract (pUR, pLR);
   fRight = pD.Length();
   pD.Subtract (pLL, pLR);
   fBottom = pD.Length();
   pD.Subtract (pUL, pLL);
   fLeft = pD.Length();

   TEXTUREPOINT tp;
   if (fInterA) {
      // because triangle HV space is not exactly equivalent to quad HV space,
      // do some adjustments
      tp = HVA;
      if (fabs(fBottom) > EPSILON)
         HVA.h *= ((1.0 - tp.v) * fBottom + tp.v * fTop) / fBottom;
      if (fabs(fLeft) > EPSILON)
         HVA.v *= ((1.0 - tp.h) * fLeft + tp.h * fRight) / fLeft;

      // scale to mesh HV space
      HVA.h = 1.0 - HVA.h;
      HVA.v = HVA.v;
      //HVA.h = HVA.h * (m_afH[0] - m_afH[1]) + m_afH[1];
      //HVA.v = HVA.v * (m_afV[1] - m_afV[0]) + m_afV[0];
   }
   if (fInterB) {
      // because triangle HV space is not exactly equivalent to quad HV space,
      // do some adjustments
      tp = HVB;
      if (fabs(fTop) > EPSILON)
         HVB.h *= ((1.0 - tp.v) * fTop + tp.v * fBottom) / fTop;
      if (fabs(fRight) > EPSILON)
         HVB.v *= ((1.0 - tp.h) * fRight + tp.h * fLeft) / fRight;

      // scale to mesh HV space
      HVB.h = HVB.h;
      HVB.v = 1.0 - HVB.v;
      //HVB.h = HVB.h * (m_afH[1] - m_afH[0]) + m_afH[0];
      //HVB.v = HVB.v * (m_afV[0] - m_afV[1]) + m_afV[1];
   }

   if (fInterA && fInterB) {
      // if both, chose the closest
      BOOL fUseA = (fabs(fAlphaA) < fabs(fAlphaB));

      if (pHV)
         *pHV = fUseA ? HVA : HVB;
      if (pfAlpha)
         *pfAlpha = fUseA ? fAlphaA : fAlphaB;
      if (pIntersect)
         pIntersect->Copy (fUseA ? &pInterA : &pInterB);
      if (pfIntersectA)
         *pfIntersectA = fUseA;
      return TRUE;
   }
   else if (fInterA) {
      if (pHV)
         *pHV = HVA;
      if (pfAlpha)
         *pfAlpha = fAlphaA;
      if (pIntersect)
         pIntersect->Copy (&pInterA);
      if (pfIntersectA)
         *pfIntersectA = TRUE;
      return TRUE;
   }
   else if (fInterB) {
      if (pHV)
         *pHV = HVB;
      if (pfAlpha)
         *pfAlpha = fAlphaB;
      if (pIntersect)
         pIntersect->Copy (&pInterB);
      if (pfIntersectA)
         *pfIntersectA = FALSE;
      return TRUE;
   }
   else
      return FALSE;
}


/********************************************************************************************
IntersectLineTriangle - Intersect a line with a triangle

inputs
   PCPoint     pLineStart - start of the line
   PCPoint     pSub - Direction of line. End of line minus start of line.
   PCPoint     p1, p2, p3 - Three vertecies of triangle.
   PCPoint     pIntersect - Filled with the point where they intersct. Can be NULL;
   fp      *pfAlpha - Filled with the alpha where the line intersects the plane.
                  At 0 it intersects at linestart, at 1 at lineend. Can be NULL.
   PTEXTUREPOINT pHV - Where it intersects in the triangle. H is 0..1 from p2 to p1.
               V is 0..1 from p2 to p3. Can be NULL
   DWORD       dwTestBits - 
                  If bit 0x01 then throw out intersection if h < 0.
                  If bit 0x02 then throw out intersection if v < 0.
                  if bit 0x04 then trhow out intersection if h+v > 1
                  If bit 0x08 then throw out intersection if h > 1
                  If bit 0x10 then throw out intersection if v > 1
                  Disabled - If bit 0x20 then return TRUE if with EPSILON of border
returns
   BOOL - TRUE if intersect, FALSE if dont
*/
BOOL IntersectLineTriangle (PCPoint pLineStart, PCPoint pSub,
                         PCPoint p1, PCPoint p2, PCPoint p3,
                         PCPoint pIntersect, fp *pfAlpha, PTEXTUREPOINT pHV,
                         DWORD dwTestBits)
{
   // BUGFIX - Put fCloseOK in because was having some problems with stair-walls
   // being too high in random points. Wasn't actually a problem with this test,
   // but thought I'd leave it in to be safe
   // BUGFIX again - Took out fCloseOK because causing problems with intellisense intersection
   // of two building blocks... actually not sure if this is causing the problem but leaving it out
   // BUGFIX - Put this back in (gopefully wont muck up intersect building blocks) because
   // was causing rectangular doorways not to always be cut out. In process also made more forgiving
   BOOL fCloseOK = (dwTestBits & 0x20) ? TRUE : FALSE;
   ///BOOL fCloseOK = FALSE;
#define OKERR        .01
   // find the normal
   CPoint pH, pV, pN;
   pH.Subtract (p1, p2);
   pV.Subtract (p3, p2);
   pN.CrossProd (&pH, &pV);


   // find out where it crosses the plane on the dwLongest dimension
   // solve alpha = ((P-Q).v) / (w.v), P=point on plalne, Q=pLineStart, w=pLineEnd-pLineStart, v=Plane normal
   CPoint pPQ;
   pPQ.Subtract (p2, pLineStart);
   fp fAlpha;
   fAlpha = pSub->DotProd (&pN);
   if (fabs(fAlpha) < EPSILON)
      return FALSE;  // doens't intersect
   fAlpha = pPQ.DotProd(&pN) / fAlpha;

   // find out where intersects
   CPoint   pInter;
   pInter.Copy (pSub);
   pInter.Scale(fAlpha);
   pInter.Add (pLineStart);

   // copy intesection and alpha if wanted
   if (pIntersect)
      pIntersect->Copy (&pInter);
   if (pfAlpha)
      *pfAlpha = fAlpha;

#if 0 // doesnt work
   // find out where relative to vertex p2, the point intersects so we
   // know if it's in the triangle
   pInter.Subtract (p2);
   h = pH.p[0] * pH.p[0] + pH.p[1] * pH.p[1] + pH.p[2] * pH.p[2]; // length square
   v = pV.p[0] * pV.p[0] + pV.p[1] * pV.p[1] + pV.p[2] * pV.p[2]; // length square
   h = pInter.DotProd(&pH) / h;
   v = pInter.DotProd(&pV) / v;
#endif

   // this code works
   fp h,v;
   CPoint Nc, Nb, Na, Pa, Pb, Pc, Pd;
   //pInter.Subtract (p2);
   Pd.Copy (p2);
   Pc.Copy (&pV);  // pc = p01 - p00
   Pb.Copy (&pH);  // pb = p10 - p00
   Pa.Zero();  // Pa = p00 - p10 + p11 - p01. p11=(p10-p00) + p01 = 0
   Nc.CrossProd (&Pc, &pN);
   Na.CrossProd (&Pa, &pN);   // should be 0
   fp Du0, Du1, Dv1, Dv0;
   Du0 = Nc.DotProd (&Pd);
   Du1 = Na.DotProd (&Pd) + Nc.DotProd(&Pb);
   h = Du1 - Na.DotProd (&pInter);
   if (!h)
      h = 0;
   else
      h = (Nc.DotProd(&pInter) - Du0) / h;

   if (fCloseOK) {
      if ((dwTestBits & 0x01) && (h < 0-OKERR))
         return FALSE;
      if ((dwTestBits & 0x08) && (h > 1+OKERR))
         return FALSE;
   }
   else {
      if ((dwTestBits & 0x01) && (h < 0))
         return FALSE;
      if ((dwTestBits & 0x08) && (h > 1))
         return FALSE;
   }

   Nb.CrossProd (&Pb, &pN);
   Dv0 = Nb.DotProd (&Pd);
   Dv1 = Na.DotProd (&Pd) + Nb.DotProd (&Pc);
   v = Dv1 - Na.DotProd (&pInter);
   if (!v)
      v = 1;
   else
      v = (Nb.DotProd(&pInter) - Dv0) / v;


   if (fCloseOK) {
      if ((dwTestBits & 0x02) && (v < 0-OKERR))
         return FALSE;
      if ((dwTestBits & 0x10) && (v > 1+OKERR))
         return FALSE;
      if ((dwTestBits & 0x04) && (h+v > 1+OKERR))
         return FALSE;
   }
   else {
      if ((dwTestBits & 0x02) && (v < 0))
         return FALSE;
      if ((dwTestBits & 0x10) && (v > 1))
         return FALSE;
      if ((dwTestBits & 0x04) && (h+v > 1))
         return FALSE;
   }

   // intersects with the triangle
   if (pHV) {
      pHV->h = h;
      pHV->v = v;
   }

#if 0
   char szTemp[64];
   sprintf (szTemp, "Inter triangle: %g,%g, H=%g,%g,%g V=%g,%g,%g\r\n", (double)pHV->h, (double)pHV->v,
      (double)pH.p[0], (double)pH.p[1], (double)pH.p[2], (double)pV.p[0], (double)pV.p[1], (double)pV.p[2]);
   OutputDebugString (szTemp);
#endif
   return TRUE;

}


/*********************************************************************************
IntersectTwoTriangles - Intersects triangle B against triangle A. Use for determining
intersection of two spline surfaces, so the important information is the PTEXTUREPOINT
intersections.

inputs
   PCPoint     pA1, pA2, pA3 - Triangle A. Points going clockwise.
   PTEXTUREPOINT ptA1, ptA2, ptA3 - HV of triangle A
   PCPoint     pB1, pB2, pB3 - Triangle B. Points going clickwise
   PTEXTUREPOINT pI1, pI2 - Filled with the intersection of triangle B with A.
               The direction of pI1 to pI2 is such that, looking at A from the front,
               B's front is on the left side of the line pI1 to pI2. (Sitting
               at pI1, and looking towards pI2, B's front-normal will be pointing left)
returns
   BOOL - TRUE if there's an intersection an pI1 and pI2 are valid. FALSE if none
*/
BOOL IntersectTwoTriangles (PCPoint pA1, PCPoint pA2, PCPoint pA3,
                            PTEXTUREPOINT ptA1, PTEXTUREPOINT ptA2, PTEXTUREPOINT ptA3,
                            PCPoint pB1, PCPoint pB2, PCPoint pB3,
                            PTEXTUREPOINT pI1, PTEXTUREPOINT pI2)
{
   // if they are parallel then no intersections
   CPoint pN1, pN2, pT1, pT2;
   pT1.Subtract (pA1, pA2);
   pT2.Subtract (pA3, pA2);
   pN1.CrossProd (&pT1, &pT2);
   pN1.Normalize();
   pT1.Subtract (pB1, pB2);
   pT2.Subtract (pB3, pB2);
   pN2.CrossProd (&pT1, &pT2);
   pN2.Normalize();
   fp fDot;
   fDot = pN1.DotProd (&pN2);
   if (fabs(fDot) > 1 - CLOSE)
      return FALSE;

   // BUGFIX - Put in all the "CLOSE" values so that if two building blocks
   // were perfectly aligned along their walls wouldn't actually clip one against
   // the other. what's happening without CLOSE is that one edge come up at a right
   // angle to the one testing, but barely hits it. As a result, get clipping there
   // even though shouldnt

   // FUTURERELEASE - I've done a cheap hack here regarding the intersection of
   // two buildinging blocks whose walls are co-planar. It works, but only moderately
   // well. If I fell into it, I should do a better job later.

   // make three lines from triangle B and find out where they intersect
   DWORD i, dwInter;
   CPoint apl[3];
   BOOL fInter[3];
   TEXTUREPOINT tpInter[3];
   dwInter = 0;
   apl[0].Subtract (pB2, pB1);
   apl[1].Subtract (pB3, pB2);
   apl[2].Subtract (pB1, pB3);
   fp fAlpha;
   for (i = 0; i < 3; i++) {
      fInter[i] = IntersectLineTriangle (i ? ((i == 2) ? pB3 : pB2) : pB1, &apl[i], pA1, pA2, pA3, NULL, &fAlpha,
         &tpInter[i], 0);
      // BUGFIX - Was if (fInter[i] && ((fAlpha < CLOSE) || (fAlpha > 1-CLOSE))).
      // Changed so that wouldnt get triangle intersections sneaking right through border
      if (fInter[i] && ((fAlpha < -CLOSE) || (fAlpha > 1+CLOSE)))
         fInter[i] = FALSE;
      if (fInter[i])
         dwInter++;
   }

   // if intersect at less than two points then doesn't intersect at all
   if (dwInter < 2)
      return FALSE;

   PTEXTUREPOINT patp[2];

   // if intersects at 3 points then have rounding off error, so choose the
   // two points furthest apart in tpInter
   if (dwInter == 3) {
      fp fDist, fA, fB, fMax;
      DWORD dwUse;
      for (i = 0; i < 3; i++) {
         fA = tpInter[(i+1)%3].h - tpInter[i].h;
         fB = tpInter[(i+1)%3].v - tpInter[i].v;
         fDist = fA * fA + fB * fB;
         if (!i) {
            fMax = fDist;
            dwUse = 0;
            continue;
         }
         if (fDist > fMax) {
            fMax = fDist;
            dwUse = i;
         }
      }
      if (dwUse < 2) {
         patp[0] = &tpInter[dwUse];
         patp[1] = &tpInter[(dwUse+1)%3];
      }
      else {
         patp[1] = &tpInter[dwUse];
         patp[0] = &tpInter[(dwUse+1)%3];
      }
   }
   else {   // dwInter == 2
      if (!fInter[0]) {
         patp[0] = &tpInter[1];
         patp[1] = &tpInter[2];
      }
      else if (!fInter[1]) {
         // BUGFIX - Need this otehrwise would have flipped direction
         patp[0] = &tpInter[2];
         patp[1] = &tpInter[0];
      }
      else {   // !fInter[2]
         patp[0] = &tpInter[0];
         patp[1] = &tpInter[1];
      }
   }

   // now, intersect this line against the the h & v boundaries
   fp fAlphaMin, fAlphaMax;
   fAlphaMin = 0;
   fAlphaMax = 1;
   CPoint pW, pQ, pP, pV, pPQ;
   pW.Zero();
   pW.p[0] = patp[1]->h - patp[0]->h;
   pW.p[1] = patp[1]->v - patp[0]->v;
   pQ.Zero();
   pQ.p[0] = patp[0]->h;
   pQ.p[1] = patp[0]->v;

   for (i = 0; i < 3; i++) {
      pP.Zero();
      pV.Zero();
      switch (i) {
      case 0:  // against v = 0
         // trivial reject
         // BUGFIX - Was if ((patp[0]->v < CLOSE) && (patp[1]->v < CLOSE)).
         // Changed so that if right on edge will be a bit forgiving
         if ((patp[0]->v < -CLOSE) && (patp[1]->v < -CLOSE))
            return FALSE;  // trivial reject
         // BUGFIX - Was if ((patp[0]->v >= 0) && (patp[1]->v >= 0))
         // Changed so will be forgiving
         if ((patp[0]->v >= -CLOSE) && (patp[1]->v >= -CLOSE))
            continue;   // trivial accept against v

         pV.p[1] = 1;
         break;
      case 1:  // against h = 0
         // trivial reject against h
         // BUGFIX - Was if ((patp[0]->h < CLOSE) && (patp[1]->h < CLOSE))
         // Changed so would be forgiving
         if ((patp[0]->h < -CLOSE) && (patp[1]->h < -CLOSE))
            return FALSE;  // trivial reject
         // BUGFIX - Was if ((patp[0]->h >= 0) && (patp[1]->h >= 0))
         // Changed so would be forgiving
         if ((patp[0]->h >= -CLOSE) && (patp[1]->h >= -CLOSE))
            continue;   // trivial accept against v

         pV.p[0] = 1;
         break;
      case 2:  // against v+h=1
         // trivial reject against v+h = 1
         // BUGFIX - Was if ((patp[0]->h + patp[0]->v > 1-CLOSE) && (patp[1]->h + patp[1]->v > 1-CLOSE))
         // Changed so would be forgiving
         if ((patp[0]->h + patp[0]->v > 1+CLOSE) && (patp[1]->h + patp[1]->v > 1+CLOSE))
            return FALSE;  // trivial reject
         // BUGFIX - Was if ((patp[0]->h + patp[0]->v <= 1) && (patp[1]->h + patp[1]->v <= 1))
         // Changed so would be forgiving
         if ((patp[0]->h + patp[0]->v <= 1+CLOSE) && (patp[1]->h + patp[1]->v <= 1+CLOSE))
            continue;   // trivial accept

         pV.p[0] = -1;
         pV.p[1] = -1;
         pP.p[0] = 1;
         break;
      }

      pPQ.Subtract (&pP, &pQ);
      fp fDot;
      fDot = pW.DotProd(&pV);
      // BUGFIX - Was if (fDot == 0)
      // Changed to take into accounr round-off error
      if (fabs(fDot) < CLOSE)
         continue;   // lines dont intersect.
      fAlpha = pPQ.DotProd (&pV) / fDot;

      switch (i) {
      case 0:  // against v = 0
         if (pW.p[1] >= 0) // is the line going up direction
            fAlphaMin = max(fAlphaMin, fAlpha);
         else
            fAlphaMax = min(fAlphaMax, fAlpha);
         break;
      case 1:  // against h = 0
         if (pW.p[0] >= 0) // is the line going to the right
            fAlphaMin = max(fAlphaMin, fAlpha);
         else
            fAlphaMax = min(fAlphaMax, fAlpha);
         break;
      case 2:  // aainst v+h=1
         if (pW.p[0] + pW.p[1] <= 0)   // is the line heading down and left
            fAlphaMin = max(fAlphaMin, fAlpha);
         else
            fAlphaMax = min(fAlphaMax, fAlpha);
         break;
      }
   }

   // if alphamin >= alphamax then the line doesn't actually intersect the triangle
   if (fAlphaMin + CLOSE >= fAlphaMax)
      return FALSE;

   // else, intersects, so translate this into points
   TEXTUREPOINT HV, *pI;
   DWORD k;
   CPoint   pInter[2];
   for (i = 0; i < 2; i++) {
      fAlpha = i ? fAlphaMax : fAlphaMin;
      pI = i ? pI2 : pI1;

      // calculate in the triangle's coordinates, from 0..1, 0..1, where this
      // point intersects
      HV.h = patp[0]->h + fAlpha * (patp[1]->h - patp[0]->h);
      HV.v = patp[0]->v + fAlpha * (patp[1]->v - patp[0]->v);

      // because h & v are at right angles and sum up to one, it's fairly easy math
      pI->h = HV.h * (ptA1->h - ptA2->h) + HV.v * (ptA3->h - ptA2->h) + ptA2->h;
      pI->v = HV.h * (ptA1->v - ptA2->v) + HV.v * (ptA3->v - ptA2->v) + ptA2->v;

      for (k = 0; k < 3; k++)
         pInter[i].p[k] = HV.h * (pA1->p[k] - pA2->p[k]) + HV.v * (pA3->p[k] - pA2->p[k]) + pA2->p[k];
   }

   // might want to flip the direction of the line
   // find the normals for borth A and B
   CPoint pNA, pNB, pH, pNCrossN;
   pH.Subtract (pB1, pB2);
   pV.Subtract (pB3, pB2);
   pNB.CrossProd (&pH, &pV);
   pH.Subtract (pA1, pA2);
   pV.Subtract (pA3, pA2);
   pNA.CrossProd (&pH, &pV);

   // cross the normals to give a direction
   pNCrossN.CrossProd (&pNA, &pNB);

   // if the direction doesn't match direction then reverse
   pInter[1].Subtract (&pInter[0]);
   if (pInter[1].DotProd (&pNCrossN) > 0) {
      TEXTUREPOINT pTemp;
      pTemp = *pI1;
      *pI1 = *pI2;
      *pI2 = pTemp;
   }

   return TRUE;
}



/*********************************************************************************
IntersectTwoPolygons - Intersects two polygons (of three or four verticies only)
and adds zero or more line segments where they intersect to a list. NOTE: The
line segments are not necessarily added in order. However, they in each line
from p1 to p2, looking at polygon A, the normal for polygon B will be pointing to
the left.

This function is used to intersect two spline surfaces togehter and generate
a cutout, so the most important information (the HV-coordinates line) is returned.

inputs
   PCPOINT     paA   - Polygon A;
   PTEXTUREPOINT patA - HV positions of vertices for A
   DWORD       dwA - Number of points in A
   PCPoint     paB - Polygon B
   DWORD       dwB - Number points in B
   PCListFixed pl - Should be intiialized to 2 x sizeof(TEXTUREPOINT). Each pair
               of TEXTUREPOINT is a line from p1 to p2
returns
   BOOL - TRUE if success. FALSE if failure
*/
BOOL IntersectTwoPolygons (PCPoint pA, PTEXTUREPOINT patA, DWORD dwA,
                           PCPoint pB, DWORD dwB, PCListFixed pl)
{
   // only 3 or 4 points
   if ((dwA < 3) || (dwA > 4) || (dwB < 3) || (dwB > 4))
      return FALSE;

   // loop
   TEXTUREPOINT atp[2];
   DWORD a,b;
   PCPoint pA1, pA2, pA3, pB1, pB2, pB3;
   PTEXTUREPOINT ptA1, ptA2, ptA3;
   CPoint pCenterA, pCenterB;
   TEXTUREPOINT tCenterA;

   // if more than three sides, use center point
   pCenterA.Zero();
   pCenterB.Zero();
   tCenterA.h = tCenterA.v = 0;
   if (dwA > 3) {
      for (a = 0; a < dwA; a++) {
         pCenterA.Add (&pA[a]);
         tCenterA.h += patA[a].h;
         tCenterA.v += patA[a].v;
      }
      pCenterA.Scale (1.0 / (fp)dwA);
      tCenterA.h /= (fp)dwA;
      tCenterA.v /= (fp)dwA;
   }
   if (dwB > 3) {
      for (b = 0; b < dwB; b++) {
         pCenterB.Add (&pB[b]);
      }
      pCenterB.Scale (1.0 / (fp)dwB);
   }

   DWORD dwLoopA, dwLoopB;
   dwLoopA = (dwA <= 3) ? 1 : dwA;
   dwLoopB = (dwB <= 3) ? 1 : dwB;
   for (a = 0; a < dwLoopA; a++) {
      pA1 = &pA[a];
      ptA1 = &patA[a];
      pA2 = &pA[(a+1)%dwA];
      ptA2 = &patA[(a+1)%dwA];
      if (dwA > 3) {
         pA3 = &pCenterA;
         ptA3 = &tCenterA;
      }
      else {
         // dont have to worry about modulo
         pA3 = pA2+1;
         ptA3 = ptA2+1;
      }

      for (b = 0; b < dwLoopB; b++) {
         pB1 = &pB[b];
         pB2 = &pB[(b+1)%dwB];
         if (dwB > 3)
            pB3 = &pCenterB;
         else  // dont have to worry about module
            pB3 = pB2 + 1;

         if (IntersectTwoTriangles (pA1, pA2, pA3, ptA1, ptA2, ptA3, pB1, pB2, pB3, &atp[0], &atp[1])) {
            pl->Add (&atp[0]);   // add the two points
         }
      }
   }

   return TRUE;
}

#if 0 // Causing problems if polygons are not entirely flat
BOOL IntersectTwoPolygons (PCPoint pA, PTEXTUREPOINT patA, DWORD dwA,
                           PCPoint pB, DWORD dwB, PCListFixed pl)
{
   // only 3 or 4 points
   if ((dwA < 3) || (dwA > 4) || (dwB < 3) || (dwB > 4))
      return FALSE;

   // loop
   TEXTUREPOINT atp[2];
   DWORD a,b;
   PCPoint pA1, pA2, pA3, pB1, pB2, pB3;
   PTEXTUREPOINT ptA1, ptA2, ptA3;
   pA1 = &pA[0];
   ptA1 = &patA[0];
   pB1 = &pB[0];
   for (a = 0; a < dwA-2; a++) {
      pA2 = &pA[a ? 2 : 1];
      ptA2 = &patA[a ? 2 : 1];
      pA3 = pA2 + 1;
      ptA3 = ptA2 + 1;

      for (b = 0; b < dwB-2; b++) {
         pB2 = &pB[b ? 2 : 1];
         pB3 = pB2 + 1;

         if (IntersectTwoTriangles (pA1, pA2, pA3, ptA1, ptA2, ptA3, pB1, pB2, pB3, &atp[0], &atp[1])) {
            pl->Add (&atp[0]);   // add the two points
         }
      }
   }

   return TRUE;
}
#endif // 0 - casues problems if polygons are not entirely flat


// BUGFIX - Disable optimizations here otherwise causes an infinite loop in release
// mode
// #pragma optimize ("", off)

/*********************************************************************************
IntersectLineSegmentList - Given a CListFixed of line segments (two TEXTUREPOINT structures),
this intersects all the line segments with each other so that none of them
cross over one another. It also removed duplicate line segments.

inputs
   PCListFixed    pl - List of line segments. Initialized to 2 * sizeof(TEXTUREPOINT).
                        This data is modified in place.
returns
   none
*/
void IntersectLineSegmentList (PCListFixed pl)
{
   DWORD a, b, k;
   PTEXTUREPOINT ptpA, ptpB;
   TEXTUREPOINT A[2], B[2];
   TEXTUREPOINT aMin, aMax, bMin, bMax;
   CPoint pQ, pP, pPQ, pV, pW, pTemp1, pTemp2;
   fp fDot, fAlpha;
   for (a = 0; a < pl->Num(); a++) {
      ptpA = (PTEXTUREPOINT) pl->Get(a);
      // if close to edge make sure it's on the edge
      for (k = 0; k < 2; k++) {
         if (ptpA[k].h < EPSILON)
            ptpA[k].h = 0;
         if (ptpA[k].v < EPSILON)
            ptpA[k].v = 0;
         if (ptpA[k].h > 1 - EPSILON)
            ptpA[k].h = 1;
         if (ptpA[k].v > 1 - EPSILON)
            ptpA[k].v = 1;
      }
      
      memcpy (&A, ptpA, sizeof(A)); // because texture point memory may move around

      // if these two points are close then delete and continue
      if (AreClose(&ptpA[0], &ptpA[1])) {
         pl->Remove(a);
         a--;
         continue;
      }

      // find min and max
      aMin.h = min(A[0].h, A[1].h);
      aMax.h = max(A[0].h, A[1].h);
      aMin.v = min(A[0].v, A[1].v);
      aMax.v = max(A[0].v, A[1].v);

      // convert this to a 3D plane so can find intersection with other lines
      pTemp1.Zero();
      pTemp1.p[0] = A[1].h - A[0].h;
      pTemp1.p[1] = A[1].v - A[0].v;
      pTemp2.Zero();
      pTemp2.p[2] = 1;
      pV.CrossProd (&pTemp1, &pTemp2); // perpendicular to line
      pP.Zero();
      pP.p[0] = A[0].h;
      pP.p[1] = A[0].v;


      // loop through all the other points
      for (b = a+1; b < pl->Num(); b++) {
         ptpB = (PTEXTUREPOINT) pl->Get(b);

         // if these two points are close then delete and continue
         if (AreClose(&ptpB[0], &ptpB[1])) {
            pl->Remove(b);
            b--;
            continue;
         }

         // if either of the endpoints are close then dont bother intersecting
         // since it would only interect at the very tips
         if (AreClose(&A[0], &ptpB[0]) || AreClose (&A[1], &ptpB[0]) ||
            AreClose(&A[0], &ptpB[1]) || AreClose (&A[1], &ptpB[1]))
            continue;

         // if close to edge make sure it's on the edge
         for (k = 0; k < 2; k++) {
            if (ptpB[k].h < EPSILON)
               ptpB[k].h = 0;
            if (ptpB[k].v < EPSILON)
               ptpB[k].v = 0;
            if (ptpB[k].h > 1 - EPSILON)
               ptpB[k].h = 1;
            if (ptpB[k].v > 1 - EPSILON)
               ptpB[k].v = 1;
         }
         
         // find min and max
         bMin.h = min(ptpB[0].h, ptpB[1].h);
         bMax.h = max(ptpB[0].h, ptpB[1].h);
         bMin.v = min(ptpB[0].v, ptpB[1].v);
         bMax.v = max(ptpB[0].v, ptpB[1].v);

         // trivial reject
         if ((aMin.h > bMax.h+CLOSE) || (aMax.h < bMin.h-CLOSE) || (aMin.v > bMax.v+CLOSE) || (aMax.v < bMin.v-CLOSE))
            continue;

         // look for duplicates
         if (AreClose(&A[0], &ptpB[0]) && AreClose(&A[1], &ptpB[1])) {
            // they're close enough, so delete
            pl->Remove (b);
            b--;
            continue;
         }

         // look for intersection
         pQ.Zero();
         pQ.p[0] = ptpB[0].h;
         pQ.p[1] = ptpB[0].v;
         pW.p[0] = ptpB[1].h - ptpB[0].h;
         pW.p[1] = ptpB[1].v - ptpB[0].v;
         pPQ.Subtract (&pP, &pQ);
         fDot = pW.DotProd (&pV);
         if (fDot == 0)
            continue;   // dont intersect
         fAlpha = pPQ.DotProd (&pV) / fDot;
         if ((fAlpha < -CLOSE) || (fAlpha > 1+CLOSE))
            continue;   // dont intersect

         // see where they intersect
         pTemp1.Copy (&pW);
         pTemp1.Scale (fAlpha);
         pTemp1.Add (&pQ);

         // where is this in the other line?
         fp fDeltaX, fDeltaY;
         fp fAlpha2;
         fDeltaX = A[1].h - A[0].h;
         fDeltaY = A[1].v - A[0].v;
         if (fabs(fDeltaX) > fabs(fDeltaY)) {
            fAlpha2 = (pTemp1.p[0] - A[0].h) / fDeltaX;
         }
         else {
            fAlpha2 = (pTemp1.p[1] - A[0].v) / fDeltaY;
         }
         if ((fAlpha2 < -CLOSE) || (fAlpha2 > 1+CLOSE))
            continue;   // doesn't intersect

         // intersect, so split this up.
         TEXTUREPOINT A2[2];
         memcpy (&A2, &A, sizeof(A));
         memcpy (&B, ptpB, sizeof(B));
         DWORD dwSplit = 0;
         DWORD k;
         TEXTUREPOINT aSplit[2], Split;
         Split.h = pTemp1.p[0];
         Split.v = pTemp1.p[1];
         for (k = 0; k < 4; k++) {
            switch (k) {
            case 0:
               aSplit[0] = A2[0];
               aSplit[1] = Split;
               break;
            case 1:
               aSplit[0] = Split;
               aSplit[1] = A2[1];
               break;
            case 2:
               aSplit[0] = B[0];
               aSplit[1] = Split;
               break;
            case 3:
               aSplit[0] = Split;
               aSplit[1] = B[1];
               break;
            }

            // if the line is essentially nothing then skip
            if (AreClose(&aSplit[0], &aSplit[1]))
               continue;

            // if this is the first line segment split really used then replace
            // the current one
            // NOTE: We will always have at least two splits
            if (!dwSplit) {
               // refill A and recalculate min and max
               memcpy (&A, &aSplit, sizeof(aSplit));
               aMin.h = min(A[0].h, A[1].h);
               aMax.h = max(A[0].h, A[1].h);
               aMin.v = min(A[0].v, A[1].v);
               aMax.v = max(A[0].v, A[1].v);

               // store away
               ptpA = (PTEXTUREPOINT) pl->Get(a);
               memcpy (ptpA, &aSplit, sizeof(aSplit));
            }
            else if (dwSplit == 1) {
               // replace B
               ptpB = (PTEXTUREPOINT) pl->Get(b);
               memcpy (ptpB, &aSplit, sizeof(aSplit));
            }
            else {
               // add it
               pl->Add (&aSplit);
            }

            // increment
            dwSplit++;
         }  // loop over k

      }  // loop over b
   } // loop over a

   // done
}
// #pragma optimize ("", on)


/*********************************************************************************
LineSegmentsToSequences - Takes a list of line segments (probably produced by
IntersectTwoPolygons() while interecting one or more spline surfaces with a single
spline surface. (Note that line segment direction is preserved.) It then links
these into sequences of lines. The sequences are choses so the do NOT intersect with
one another, which is useful for clipping purposes later on.

inputs
   PCListFixed    plLines - Pointer to a list containing line segments. Initialized
                     to 2 * sizeof(TEXTUREPOINT).
                     NOTE: This data structure will be modified by this call as
                     line segments are intersected against one another and/or removed.
returns
   PCListFixed - List of PCListFixed. The lists contained within are initialized
                  to sizeof(TEXTUREPOINT), and contain a sequence of points.
                  All the contained lists, and then the main list must be freed
                  by the caller
*/
PCListFixed LineSegmentsToSequences (PCListFixed plLines)
{
   // intersect and minimize the line segment list
   IntersectLineSegmentList (plLines);

#if 0//def _DEBUG
   char szTemp[64];
   DWORD j;
   OutputDebugString ("Inter:");
   for (j = 0; j < plLines->Num(); j++) {
      PTEXTUREPOINT p = (PTEXTUREPOINT) plLines->Get(j);

      sprintf (szTemp, "(%g,%g) to (%g,%g), ", (double)p[0].h, (double)p[0].v, (double)p[1].h, (double)p[1].v);
      OutputDebugString (szTemp);
   }

   OutputDebugString ("\r\n");
#endif

   // create the master list
   PCListFixed pl;
   pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(PCListFixed));

   // loop through all the points and note if they have a unique start and/or
   // a unique ends. Line segments with non-unique ends can no longer be added to.
   CListFixed lUnique;
   lUnique.Init (sizeof(BYTE));
   DWORD a, b;
   PTEXTUREPOINT pA, pB;
   PBYTE pAb, pBb;
   BYTE bByte;
   bByte = 0x03;  // 0x01 for unique start, 0x02 for unique end, 0x04 for used already
   lUnique.Required (plLines->Num());
   for (a = 0; a < plLines->Num(); a++)
      lUnique.Add (&bByte);

   for (a = 0; a < plLines->Num(); a++) {
      pA = (PTEXTUREPOINT) plLines->Get(a);
      pAb = (PBYTE) lUnique.Get(a);

      for (b = a+1; b < plLines->Num(); b++) {
         pB = (PTEXTUREPOINT) plLines->Get(b);
         pBb = (PBYTE) lUnique.Get(b);

         // if the start values are close note this
         if (AreClose(&pA[0],&pB[0])) {
            pAb[0] &= ~(0x01);   // no longer unique
            pBb[0] &= ~(0x01);   // no longer unique
         }
         if (AreClose(&pA[1],&pB[1])) {
            pAb[0] &= ~(0x02);   // no longer unique
            pBb[0] &= ~(0x02);   // no longer unique
         }
      }
   }
   PBYTE pbUnique;
   pbUnique = (PBYTE) lUnique.Get(0);  // can do this since will no longer be adding

   // loop, adding onto sequences
   PCListFixed plCur;
   DWORD dwCurSeg;
   plCur = NULL;
   while (TRUE) {
      // if there's no current list that's being looked at then start one
      if (!plCur) {
         // find a segment to use
         dwCurSeg = (DWORD)-1;
         for (a = 0; a < plLines->Num(); a++) {
            if (pbUnique[a] & 0x04)
               continue;

            // potentially can use this one
            dwCurSeg = a;

            // however, if it's not a starting sequence then look further
            // in hopes that can start with that and speed things up
            if (!(pbUnique[a] & 0x01))
               break;
         }

         if (dwCurSeg == (DWORD)-1)
            break;   // no more line, anywhere

         // loop through the pbUnique list and set the 0x08 bit to FALSE
         // so we can trace if we've been there already when walking backwards
         for (b = 0; b < plLines->Num(); b++)
            pbUnique[b] &= ~(0x08);

         // follow this back until
         // a) It's a non-unique start
         // b) The previous one is a non-unique end
         // c) Can't find anymore
         // d) Get back to the beginning (which means its a loop)
         while (TRUE) {
            // if this is a non-unique start then quit
            if (!(pbUnique[dwCurSeg] & 0x01))
               break;

            pA = (PTEXTUREPOINT) plLines->Get(dwCurSeg);
            BOOL fExitWhile;
            fExitWhile = FALSE;
            for (b = 0; b < plLines->Num(); b++) {
               if (b == dwCurSeg)
                  continue;
               if (pbUnique[b] & (0x04 | 0x08))
                  continue;   // already used or already been there

               pB = (PTEXTUREPOINT) plLines->Get(b);
               if (!AreClose (&pA[0], &pB[1]))
                  continue;   // start is not close to end

               // if the previous one is an non-unique end then we're finished going back
               if (!(pbUnique[b] & 0x02)) {
                  fExitWhile = TRUE;
                  break;
               }

               // else, found a previous one. use this
               dwCurSeg = b;
               pbUnique[b] |= 0x08;
               break;
            }
            if (b >= plLines->Num())
               break;   // cant go back anymore

            if (fExitWhile)
               break;
         }

         // when get here, dwCurSeg is a starting segment of a sequence.
         // create a new sequence
         plCur = new CListFixed;
         if (!plCur)
            return pl;  // error
         pl->Add (&plCur);
         pA = (PTEXTUREPOINT) plLines->Get(dwCurSeg);
         plCur->Init (sizeof(TEXTUREPOINT), pA, 2);
         pbUnique[dwCurSeg] |= 0x04;   // note that it's used

         // if this is an end-segment in itself then we're done with this list
         // already
         if (!(pbUnique[dwCurSeg] & 0x02)) {
            plCur = NULL;
            continue;
         }
      } // if no plcur

      // find the end point in the list
      PTEXTUREPOINT pEnd, pBefore;
      pEnd = (PTEXTUREPOINT) plCur->Get(plCur->Num()-1);
      pBefore = pEnd-1;

      // loop through all the points finding a line segment that starts
      // where we left off
      for (a = 0; a < plLines->Num(); a++) {
         if (pbUnique[a] & 0x04)
            continue;   // skip if already been used

         // if it's a start point then skip
         if (!(pbUnique[a] & 0x01))
            continue;

         pA = (PTEXTUREPOINT) plLines->Get(a);
         if (!AreClose (pEnd, &pA[0]))
            continue;

         // if get here, found something that lines

         // if get several straight lines in a row then save on points
         fp fDeltaX, fDeltaY;
         fp fRet;
         BOOL fLinear;
         fLinear = FALSE;
         fDeltaX = pA[1].h - pBefore->h;
         fDeltaY = pA[1].v - pBefore->v;
         if (fabs (fDeltaX) > fabs(fDeltaY)) {
            fp m, b;
            m = fDeltaY / fDeltaX;
            b = pBefore->v - m * pBefore->h;

            fRet = m * pEnd->h + b;
            TEXTUREPOINT pT;
            pT.h = pEnd->h;
            pT.v = fRet;
            if (AreClose (&pT, pEnd))
               fLinear = TRUE;
            //if (fabs(fRet - pEnd->v) < EPSILON)
            //   fLinear = TRUE;
         }
         else {
            fp mInv, bInv;
            mInv = fDeltaX / fDeltaY;
            bInv = pBefore->h - mInv * pBefore->v;
            fRet = mInv * pEnd->v + bInv;

            TEXTUREPOINT pT;
            pT.h = fRet;
            pT.v = pEnd->v;
            if (AreClose (&pT, pEnd))
               fLinear = TRUE;
            //if (fabs(fRet - pEnd->h) < EPSILON)
            //   fLinear = TRUE;
         }

         if (fLinear)
            *pEnd = pA[1]; // just eliminate the last point
         else
            plCur->Add (pA+1);   // since follow on, but not linera

         pbUnique[a] |= 0x04;

         // if this is not a unique end then must be ending plCur here
         if (!(pbUnique[a] & 0x02)) {
            plCur = NULL;
            break;
         }
         break;
      }  // for (a < plLines->Num())
      if (a >= plLines->Num())
         plCur = NULL;   // no more points left for this one

      // then loop around and get the next point in line
   }

#if 0
   // loop through all the lines
   DWORD a, b;
   PTEXTUREPOINT pA, pB;
   TEXTUREPOINT A[2];
   for (a = 0; a < plLines->Num(); a++) {
      pA = (PTEXTUREPOINT) plLines->Get(a);
      memcpy (&A, pA, sizeof(A));   // since in course of adding/deleting pA may change

      // first thing... if this line segment has the same starting point
      // as any other line segments then create a new line sequence for it AND
      // for any that match
      BOOL fAlreadyAdded;
      fAlreadyAdded = FALSE;
      for (b = plLines->Num()-1; b > a; b--) {
         pB = (PTEXTUREPOINT) plLines->Get(b);

         if (!AreClose(&A[0], pB))
            continue;

         // they are close
         if (!fAlreadyAdded) {
            // create a new sequence for A
            pNew = new CListFixed;
            if (!pNew)
               continue;
            pNew->Init (sizeof(TEXTUREPOINT), &A, 2);
            pl->Add (&pNew);

            fAlreadyAdded = TRUE;
         }

         // add this one
         pNew = new CListFixed;
         if (!pNew)
            continue;
         pNew->Init (sizeof(TEXTUREPOINT), pB, 2);
         pl->Add (&pNew);

         // and delete it
         plLines->Remove (b);
         continue;
      }

      // if get here know that the line is NOT the start of a sequence
      // therefore, add it to the end of an existing sequence or
      // insert before the beginning
      PCListFixed pt;
      BOOL fAdded;
      fAdded = FALSE;
      for (b = 0; b < pl->Num(); b++) {
         pt = *((PCListFixed*) pl->Get(b));

         // get the last element and see if should add onto the end
         pB = (PTEXTUREPOINT) pt->Get(pt->Num()-1);
         if (AreClose (pB, &A[0])) {
            // found a match... stick this on the end
            pt->Add (&A[1]);
            fAdded = TRUE;
            break;
         }

         // see if it should go before the one at the beginning
         pB = (PTEXTUREPOINT) pt->Get(0);
         if (AreClose (pB, &A[1])) {
            // found a match... stick this at the beginning
            pt->Insert (0, &A[0]);
            fAdded = TRUE;
            break;
         }
      }
      if (fAdded)
         continue;

      // if that didn't work then just create a new sequence because
      // it doesn't appear to fit with anything
      pNew = new CListFixed;
      if (!pNew)
         continue;
      pNew->Init (sizeof(TEXTUREPOINT), &A, 2);
      pl->Add (&pNew);
   }  // loop over all points

   // combine all the sequences together, since some tails may match up with some heads
   PCListFixed pt, p2;
   PTEXTUREPOINT pStart, pEnd, pStart2, pEnd2;
   BOOL fUniqueStart, fUniqueEnd;
   DWORD i,j;
   for (i = 0; i < pl->Num(); i++) {
      pt = *((PCListFixed*)pl->Get(i));
      pStart = (PTEXTUREPOINT) pt->Get(0);
      pEnd = (PTEXTUREPOINT) pt->Get(pt->Num()-1);

      // remember if the start and end are unique
      fUniqueStart = fUniqueEnd = TRUE;
      for (j = 0; j < pl->Num(); j++) {
         if (i == j)
            continue;
         pt2 = *((PCListFixed*)pl->Get(j));
         pStart2 = (PTEXTUREPOINT) pt2->Get(0);
         pEnd2 = (PTEXTUREPOINT) pt2->Get(pt2->Num()-1);

         if (fUniqueStart && AreClose(&pStart, &pStart2))
            fUniqueStart = FALSE;
         if (fUniqueEnd && AreClose(&pEnd, &pEnd2))
            fUniqueEnd = FALSE;
      }

      // if it has a unique end (which means other curves don't end there
      // then see this end matches the start of another one
   }
#endif // 0
#if 0//def _DEBUG
   {
   // display all the sequences
   char szTemp[64];
   DWORD i, j;
   PCListFixed pt;
   for (i = 0; i < pl->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)pl->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
   }
#endif

   // done
   return pl;
}


/*********************************************************************************
LineSequencesRemoveLoops - Look for loops in the line sequences and split them in
two.

inputs
   PCListFixed       pl - List from LineSegmentsToSequences. This is changed
returns
   none
*/
void LineSequencesRemoveLoops (PCListFixed pl)
{
   DWORD i, j;
   PCListFixed plTP;
   PTEXTUREPOINT ptp, p1, p2;
   DWORD dwNum;
   i = 0;
   while (i < pl->Num()) {
      plTP = *((PCListFixed*) pl->Get(i));
      ptp = (PTEXTUREPOINT) plTP->Get(0);
      dwNum = plTP->Num();

      // BUGFIX - Had a dwNum==1, which cause a problem with %(dwNum-1) later on
      if (dwNum < 2) {
         i++;
         continue;
      }

      // is this a loop
      p1 = ptp+0;
      p2 = ptp+(dwNum - 1);
      if (!AreClose(p1, p2)) {
         i++;
         continue;
      }
      
      // it's a loop, so find the furthest left point, and the furthest
      // right point, and split the loop at these two
      DWORD dwLeft, dwRight;
      fp fLeft, fRight;
      dwLeft = dwRight = 0;
      fLeft = fRight = p1->h;
      for (j = 1; j < dwNum; j++) {
         if (ptp[j].h < fLeft) {
            fLeft = ptp[j].h;
            dwLeft = j;
         }
         else if (ptp[j].h > fRight) {
            fRight = ptp[j].h;
            dwRight = j;
         }
      }

      // how many points in the left loop and the right loop
      DWORD dwLeft2;
      DWORD dwCountLeft, dwCountRight;
      if (dwRight < dwLeft)
         dwRight = dwRight + (dwNum-1);
      for (dwLeft2 = dwLeft; dwLeft2 < dwRight; dwLeft2 += (dwNum-1));
      dwCountLeft = dwRight - dwLeft + 1;
      dwCountRight = dwLeft2 - dwRight + 1;


      // create two new lists for these points and move the points over
      PCListFixed pNew1, pNew2;
      pNew1 = new CListFixed;
      pNew2 = new CListFixed;
      if (!pNew1 || !pNew2) {
         if (pNew1)
            delete pNew1;
         if (pNew2)
            delete pNew2;
         return;
      }
      pNew1->Init (sizeof(TEXTUREPOINT));
      pNew2->Init (sizeof(TEXTUREPOINT));
      pNew1->Required (dwCountLeft);
      pNew2->Required (dwCountRight);
      for (j = 0; j < dwCountLeft; j++)
         pNew1->Add (ptp + ((dwLeft + j) % (dwNum-1)));
      for (j = 0; j < dwCountRight; j++)
         pNew2->Add (ptp + ((dwRight + j) % (dwNum-1)));
      pl->Add (&pNew1);
      pl->Add (&pNew2);
      pl->Remove (i);
      delete plTP;
   }
}

/*********************************************************************************
LineSequencesRemoveHanging - Remove any line sequences that aren't conencted
to other line sequences. (This only considers line sequences attached if the
end of one meets with the tail of another, not head-to-head or tail-to-tail.)

inputs
   PCListFixed       pl - List from LineSegmentsToSequences. This is changed
   BOOL              fAllowBothSides - If TRUE then can potentially connect to
                     either side (which means sequences can be connected backwards).
                     If FALSE, only allow sequences to be connected forwards
returns
   none
*/
void LineSequencesRemoveHanging (PCListFixed pl, BOOL fAllowBothSides)
{
   // cant have any loops
   LineSequencesRemoveLoops (pl);

   // must make an entire pass through the line sequences without
   // removing anything before success is declared
   while (TRUE) {
      BOOL fRemoved = FALSE;

      // loop through all the elements, backwards, and see if should be removed
      DWORD dwS;
      for (dwS = pl->Num()-1; dwS < pl->Num(); dwS--) {
         PCListFixed plSeq = *((PCListFixed*) pl->Get(dwS));

         // get the head and the tail
         PTEXTUREPOINT p1, p2;
         p1 = (PTEXTUREPOINT) plSeq->Get(0);
         p2 = (PTEXTUREPOINT) plSeq->Get(plSeq->Num()-1);

         // remember if found match for head and tail
         BOOL fConnectP1, fConnectP2;
         fConnectP1 = fConnectP2 = FALSE;

         // loop through other sequences
         DWORD j;
         for (j = 0; j < pl->Num(); j++) {
            if (j == dwS)
               continue;   // dont test against the same one

            PCListFixed plJ = *((PCListFixed*) pl->Get(j));

            // get the head and the tail
            PTEXTUREPOINT p1j, p2j;
            p1j = (PTEXTUREPOINT) plJ->Get(0);
            p2j = (PTEXTUREPOINT) plJ->Get(plJ->Num()-1);

            if (!fConnectP1 && AreClose(p1, p2j))
               fConnectP1 = TRUE;
            if (fAllowBothSides && !fConnectP1 && AreClose(p1, p1j))
               fConnectP1 = TRUE;
            if (!fConnectP2 && AreClose (p2, p1j))
               fConnectP2 = TRUE;
            if (fAllowBothSides && !fConnectP2 && AreClose (p2, p2j))
               fConnectP2 = TRUE;

            if (fConnectP1 && fConnectP2)
               break;   // no point going further since found what we wanted
         }

         // if both are connected then continue
         if (fConnectP1 && fConnectP2)
            continue;

         // else, remove this
         delete plSeq;
         pl->Remove (dwS);
         fRemoved = TRUE;
      }

      if (!fRemoved)
         break;
   }

   // done
#if 0//def _DEBUG
   // display all the sequences
   char szTemp[64];
   DWORD i, j;
   PCListFixed pt;
   for (i = 0; i < pl->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)pl->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
#endif
}


/*********************************************************************************
SequencesAddFlips - Takes a pointer to a CListFixed, which contains other CListFixed
of sequences. (Output of LineSequencesForRoof() and others). Each one in the original
list is duplicated and flipped so that path goes backwards.

inputs
   PCListFixed       plSeq - List of sequences
returns
   none
*/
void SequencesAddFlips (PCListFixed plSeq)
{
   DWORD dwNum = plSeq->Num();
   DWORD i, j, dwNum2;
   PTEXTUREPOINT pt;
   TEXTUREPOINT t;
   plSeq->Required (plSeq->Num() + dwNum);
   for (i = 0; i < dwNum; i++) {
      PCListFixed pl = *((PCListFixed*) plSeq->Get(i));
      PCListFixed pNew = new CListFixed;
      if (!pNew)
         return;
      pNew->Init (sizeof(TEXTUREPOINT), pl->Get(0), pl->Num());

      pt = (PTEXTUREPOINT)pNew->Get(0);
      dwNum2 = pNew->Num();
      for (j = 0; j < dwNum2 / 2; j++) {
         t = pt[j];
         pt[j] = pt[dwNum2-j-1];
         pt[dwNum2-j-1] = t;
      }

      // add it
      plSeq->Add (&pNew);
   }
}


/*********************************************************************************
SequenceRightCount - Given a point and a line sequnce (array of texturepoint), this
returns a score. The score is the number of lines in the sequnce that the point is to
the right of MINUS the number of points its to the left of.

inputs
   PTEXTUREPOINT     pCheck - Point to check against
   PTEXTUREPOINT     paSeq - Pointer to an array of points that form the sequence
   DWORD             dwNum - Number of points in the sequnce
returns
   int - Number of lines that it's to the right vs. to the left
*/
int SequenceRightCount (PTEXTUREPOINT pCheck, PTEXTUREPOINT paSeq, DWORD dwNum)
{
   DWORD dwOnLeft = 0, dwOnRight = 0, dwOnLine = 0;

   // loop through all these points
   DWORD k;
   BOOL fLeft;
   for (k = 0; k+1 <dwNum; k++) {
      // if beyond top or bottom then ignore
      // NOTE: Using <= for a reason. and > for a reason
      if ((pCheck->v <= min(paSeq[k+1].v, paSeq[k].v)) || (pCheck->v > max(paSeq[k+1].v, paSeq[k].v)) )
         continue;

      // NOTE: This will not fall through a crack since at the top made
      // sure that all sequences that start and end closely are exact
      // start and end

      fp fDeltaX, fDeltaY;
      fDeltaX = paSeq[k+1].h - paSeq[k].h;
      fDeltaY = paSeq[k+1].v - paSeq[k].v;

      // if no deltaY then probably can ignore the line
      if (fabs(fDeltaY) < EPSILON)
         continue;

      // trivial reject on left/right
      if (pCheck->h < min(paSeq[k+1].h, paSeq[k].h)) {
         fLeft = (fDeltaY < 0);
         if (!fLeft)
            dwOnRight++;
         else
            dwOnLeft++;
         continue;
      }

      if (pCheck->h > max(paSeq[k+1].h, paSeq[k].h)) {
         fLeft = (fDeltaY > 0);
         if (!fLeft)
            dwOnRight++;
         else
            dwOnLeft++;
         continue;
      }

      // calculate y=mx+b
      // if this is to the right of the line (as opposed to right side of line)
      // then ignore
      fp fLineH;
      fp m, b;
      if (fabs(fDeltaX) > EPSILON) {
         m = fDeltaY / fDeltaX;
         b =  paSeq[k].v - m * paSeq[k].h;

         if (fabs(m) > EPSILON) {
            fLineH = (pCheck->v - b) / m;
            fLeft = (pCheck->h < fLineH);
         }
         else {   // left/right
            dwOnLine++;
            continue;   // horizontal, so no change
         }
      }
      else {   // fDeltaX = 0
         // it's up/down
         fLeft = (pCheck->h < paSeq[k].h);
      }

      // accound for line going down
      if (fDeltaY >= 0)
         fLeft = !fLeft;

      //fLeft = (m * pCheck->h + b - pCheck->v) < 0;  // BUGFIX - Was > 0
      if (!fLeft)
         dwOnRight++;
      else
         dwOnLeft++;
   }

   return (int) dwOnRight - (int) dwOnLeft;
}

/*********************************************************************************
SequenceRightCount - Given a point and path (array of DWORDs that are indecies into
a list of sequnces), this
returns a score. The score is the number of lines in the sequnce that the point is to
the right of MINUS the number of points its to the left of.

inputs
   PTEXTUREPOINT     pCheck - Point to check against
   DWORD             *padwPath - Pointer to an array of dwNum sequence numbers
   DWORD             dwNum - Number of sequences in the path
   PCListFixed       plSeq - Pointer ot a list of sequences, from  LineSequencesForRoof() etc.
returns
   int - Number of lines that it's to the right vs. to the left.
*/
int SequenceRightCount (PTEXTUREPOINT pCheck, DWORD *padwPath, DWORD dwNum, PCListFixed plSeq)
{
   int iScore = 0;
   DWORD i;

   for (i = 0; i < dwNum; i++) {
      PCListFixed pl = *((PCListFixed*) plSeq->Get(padwPath[i]));
      iScore += SequenceRightCount (pCheck, (PTEXTUREPOINT) pl->Get(0), pl->Num());
   }

   return iScore;
}


/*********************************************************************************
LineSequencesForRoof - Takes a bunch of line segments from several roof planes
intersecting with this roof and produces a set of line sequences that can then
be passed into some path finding functions to find what part of the roof hasn't
been trimmed out.

inputs
   PCListFixed       plLines - List of TEXTUREPOINT[2] from intersecting two splines.
                     This is modified somewhat by the functions because a surrounding
                     loop around the entire surface is added (0,0) to (0, 1) to (1,1) to
                     (1,0) to (0,0) to keep things bounded.
   BOOL              fClockwise - If TRUE, draw the outside line clockwise around edge,
                     if FALSE, counter clockwise.
   BOOL              fAllowBothSides - If TRUE then can potentially connect to
                     either side (which means sequences can be connected backwards).
                     If FALSE, only allow sequences to be connected forwards
   BOOL              fAddPerimeter - If TRUE adds a perimeter around the whole object
                     so that any bits crossing across the edge of the object wont
                     be thrown out hanging.
returns
   PCListFixed - List containings other PCListFixed. Those are lists of TEXTUREPOINT
                  indicating sequences. The sub-lists and main list must be freed
                  by the caller. All the sequences have been narrowed down to the
                  point where they're all connected to other sequences.
*/
PCListFixed LineSequencesForRoof (PCListFixed plLines, BOOL fClockwise, BOOL fAllowBothSides,
                                  BOOL fPerimeter)
{
   PCListFixed pNew;
   
   if (fPerimeter) {
      // if add a perimiter, need to eliminate any lines walking along the edge
      // because they end up creating duplicates - sometimes doing the wrong direction
      // which causes other problems.
      DWORD i;
      for (i = plLines->Num()-1; i < plLines->Num(); i--) {
         PTEXTUREPOINT ptp = (PTEXTUREPOINT) plLines->Get(i);
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
            plLines->Remove (i);
      }

      // add the edge of this loop
      TEXTUREPOINT A[2];
      A[0].h = 0;
      A[0].v = 0;
      if (!fClockwise) {
         plLines->Required (plLines->Num() + 4);

         A[1].h = 0;
         A[1].v = 1;
         plLines->Add (&A);
         A[0] = A[1];
         A[1].h = 1;
         plLines->Add (&A);
         A[0] = A[1];
         A[1].v = 0;
         plLines->Add (&A);
         A[0] = A[1];
         A[1].h = 0;
         plLines->Add (&A);
      }
      else {
         plLines->Required (plLines->Num() + 4);

         A[1].h = 1;
         A[1].v = 0;
         plLines->Add (&A);
         A[0] = A[1];
         A[1].v = 1;
         plLines->Add (&A);
         A[0] = A[1];
         A[1].h = 0;
         plLines->Add (&A);
         A[0] = A[1];
         A[1].v = 0;
         plLines->Add (&A);
      }
   }

   // convert to sequences
   pNew = LineSegmentsToSequences (plLines);
   if (!pNew)
      return NULL;

   // get rid of dangling sequences
   LineSequencesRemoveHanging (pNew, fAllowBothSides);

   return pNew;
}


/*********************************************************************************
LineSequencesSplitHoriz - Takes the line segments from LineSegmentsToSequences.
Loops through them and finds any that are not inrementally hozitonal (where one
line segment is always in the same direction as the next.) If any of these are found
they're split into fiurther sequences. This way all none of the sequences backtrack
in H.

// this takes care of loops, unrolling them

inputs
   PCListFixed       pl - List from LineSegmentsToSequences. This is changed
returns
   none
*/
void LineSequencesSplitHoriz (PCListFixed pl)
{
   DWORD i, j;
   PCListFixed plTP;
   PTEXTUREPOINT ptp;
   DWORD dwNum;

   // remove loops
   LineSequencesRemoveLoops (pl);

   i = 0;
   while (i < pl->Num()) {
      plTP = *((PCListFixed*) pl->Get(i));
      ptp = (PTEXTUREPOINT) plTP->Get(0);
      dwNum = plTP->Num();

      // find out which direction its going in the beginning
      BOOL fForward;
      for (j = 1; j < dwNum; j++)
         if (!((ptp[j].h + EPSILON > ptp[0].h) && (ptp[j].h - EPSILON < ptp[0].h))) {
            fForward = ptp[j].h > ptp[0].h;
            break;
         }
      if (j >= dwNum) {
         i++;
         continue;   // line just goes up and down
      }

      // and where the direction changes
      for (j = 2; j < dwNum; j++) {
         fp fDelta;
         fDelta = ptp[j].h - ptp[j-1].h;
         if (!fForward)
            fDelta *= -1;
         if (fDelta < 0)
            break;   // stopping on the first point where it goes backwards
      }
      if (j >= dwNum) {
         i++;
         continue;   // always going in the same direction
      }

      // create a new list to append onto
      PCListFixed pNew;
      pNew = new CListFixed;
      if (!pNew)
         return;
      pNew->Init (sizeof(TEXTUREPOINT));
      DWORD k;
      for (k = j-1; k < dwNum; k++)
         pNew->Add (ptp + k);
      pl->Add (&pNew);
      // remove points from the original
      for (k = dwNum-1; k >= j; k--)
         plTP->Remove(k);

      // we know that plTP is clear, so go onto the next one
      i++;
      continue;
   } // loop over all sequences

#if 0//def _DEBUG
   // display all the sequences
   char szTemp[64];
   //DWORD i, j;
   PCListFixed pt;
   for (i = 0; i < pl->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)pl->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
#endif
}


/*********************************************************************************
LineSequencesSortHorz - Go through each of the line sequences (returned from
LineSegmentsToSequence() and rearrange the segments within them so they go from
left (low h) to right (high h). This throws out the meaning that was in the direction.
Then, sort all the sequences in order from low h, to high h.

inputs
   PCListFixed    pl - Line sequences from LineSegmentsToSequences()
returns
   none
*/
static int _cdecl LSSHSort (const void *elem1, const void *elem2)
{
   PCListFixed p1, p2;
   p1 = *((PCListFixed*) elem1);
   p2 = *((PCListFixed*) elem2);

   PTEXTUREPOINT pt1, pt2;
   pt1 = (PTEXTUREPOINT) p1->Get(0);
   pt2 = (PTEXTUREPOINT) p2->Get(0);
   if (pt1->h > pt2->h)
      return 1;
   else if (pt1->h < pt2->h)
      return -1;
   else
      return 0;
}

void LineSequencesSortHorz (PCListFixed pl)
{
   DWORD i, j;
   PCListFixed plTP;
   PTEXTUREPOINT ptp;
   DWORD dwNum;
   for (i = 0; i < pl->Num(); i++) {
      plTP = *((PCListFixed*) pl->Get(i));
      ptp = (PTEXTUREPOINT) plTP->Get(0);
      dwNum = plTP->Num();

      // find out which direction its going in the beginning
      BOOL fForward;
      for (j = 1; j < dwNum; j++)
         if (!((ptp[j].h + EPSILON > ptp[0].h) && (ptp[j].h - EPSILON < ptp[0].h))) {
            fForward = ptp[j].h > ptp[0].h;
            break;
         }
      if (j >= dwNum) {
         continue;   // line just goes up and down
      }
      if (fForward)
         continue;   // no need to change

      // else, reverse the points
      TEXTUREPOINT tp;
      for (j = 0; j < dwNum / 2; j++) {
         tp = ptp[j];
         ptp[j] = ptp[dwNum-j-1];
         ptp[dwNum-j-1] = tp;
      }

   }

   // sort
   qsort (pl->Get(0), pl->Num(), sizeof(PCListFixed), LSSHSort);

#if 0//def _DEBUG
   // display all the sequences
   char szTemp[64];
   //DWORD i, j;
   PCListFixed pt;
   for (i = 0; i < pl->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)pl->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
#endif
}


/***********************************************************************************
LineSegmentsForWall - Takes a list of line segments from
CSplineSurface::IntersectWithSurfaces and:
   1) Intersects the lines with one another and get rid of duplicates
   2) Change the direction of each line segment so they're from left
      to right.
   3) Reorder the line segments from left to right.
   4) Create a new list of line segments which follows the lowest border of the
      line segments from left to right.

inputs
   PCListFixed    plLine - List of TEXTUREPOINT[2]
returns
   PCListFixed - list of TEXTUREPOINT[2]. Must be freed by the caller
*/
static int _cdecl LSFWSort (const void *elem1, const void *elem2)
{
   PTEXTUREPOINT p1, p2;
   p1 = (PTEXTUREPOINT) elem1;
   p2 = (PTEXTUREPOINT) elem2;

   if (p1->h > p2->h)
      return 1;
   else if (p1->h < p2->h)
      return -1;
   else
      return 0;
}

static fp VFromSegment (PTEXTUREPOINT pt, fp h)
{
   if (h <= pt[0].h)
      return pt[0].v;
   else if (h >= pt[1].h)
      return pt[1].v;

   // interp
   return (h - pt[0].h) / (pt[1].h - pt[0].h) * (pt[1].v - pt[0].v) + pt[0].v;
}

PCListFixed LineSegmentsForWall (PCListFixed plLine)
{
   // intersect with other segments
   IntersectLineSegmentList (plLine);

   // arrange members so left to right
   DWORD i;
   PTEXTUREPOINT pt;
   for (i = 0; i < plLine->Num(); i++) {
      pt = (PTEXTUREPOINT) plLine->Get(i);
      if (pt[0].h <= pt[1].h)
         continue;

      // else flip
      TEXTUREPOINT t;
      t = pt[0];
      pt[0] = pt[1];
      pt[1] = t;
   }

   // sort these
   qsort (plLine->Get(0), plLine->Num(), sizeof(TEXTUREPOINT)*2, LSFWSort);

   // create the new list
   PCListFixed pNew;
   pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(TEXTUREPOINT)*2);

   // remember the point
   PTEXTUREPOINT paLines;
   DWORD dwNum;
   paLines = (PTEXTUREPOINT) plLine->Get(0);
   dwNum = plLine->Num();

   // start out at far left
   DWORD dwCurSeg;
   fp fCurH, fCurEnd;
   fCurH = 0;
   dwCurSeg = (DWORD)-1;

   while (TRUE) {
      // figure out which line segment to use
      DWORD dwLowest;
      fp fLowest, fLowestSlope;
      dwLowest = (DWORD)-1;
      for (i = 0; i < dwNum; i++) {
         pt = paLines + (i*2);
         if ((fCurH < pt[0].h) || (fCurH >= pt[1].h))
            continue;

         // this one will work, see if it's the lowest
         fp fV, fSlope;
         fV = VFromSegment (pt, fCurH);
         fSlope = pt[1].h - pt[0].h;
         if (fabs(fSlope) < EPSILON)
            continue;   // if straight up down don't bother
         fSlope = (pt[1].v - pt[0].v) / fSlope;

         // take the lowest point. If more than one lowest point then take the
         // one sloping down the most
         if ((dwLowest == (DWORD)-1) || (fV > fLowest) || ((fV == fLowest) && (fSlope < fLowestSlope)) ) {
            dwLowest = i;
            fLowest = fV;
            fLowestSlope = fSlope;
         }
      }
      if (dwLowest == (DWORD) -1) {
         // if didn't find any that are within our range then loop
         // until find point
         dwLowest = (DWORD)-1;
         for (i = 0; i < dwNum; i++) {
            pt = paLines + (i*2);
            if (pt[0].h <= fCurH)
               continue;

            if ((dwLowest == (DWORD)-1) || (pt[0].h < fLowest)) {
               dwLowest = i;
               fLowest = pt[0].h;
            }
         }
         if (dwLowest == (DWORD)-1)
            goto finished;   // no more points, so all done

         // else, use this
         fCurH = fLowest;
         continue;   // so actually go an find best slope
      }

      // if get here the lowest is the current segment
      dwCurSeg = dwLowest;
      pt = paLines + (dwCurSeg*2);

      // find the end of this
      fCurEnd = paLines[dwCurSeg*2+1].h;

      // loop through all points looking for a start after fCurH but before fCurEnd
      // if find, set fCurEnd to that
      for (i = 0; i < dwNum; i++) {
         pt = paLines + (i*2);
         if ((i != dwCurSeg) && (pt[0].h > fCurH) && (pt[0].h < fCurEnd)) {
            // only use this if it's actually lower that our line
            if (pt[0].v > VFromSegment(paLines + (dwCurSeg*2), pt[0].h))
               fCurEnd = pt[0].h;
            else
               continue;
         }
      }
      pt = paLines + (dwCurSeg*2);


      // the line segment goes from fCurH to fCurEnd
      TEXTUREPOINT A[2];
      A[0].h = fCurH;
      A[0].v = VFromSegment (pt, fCurH);
      A[1].h = fCurEnd;
      A[1].v = VFromSegment (pt, fCurEnd);

      // if the points are almost the same then get rid of them
      if (AreClose (&A[0], &A[1])) {
         fCurH = fCurEnd;
         continue;
      }

      // add this line segment
      // if it's linear with the previous line segments added then just merge
      // the two
      BOOL fLinear;
      fLinear = FALSE;
      if (pNew->Num()) {
         pt = (PTEXTUREPOINT) pNew->Get(pNew->Num()-1);
         if (AreClose (&pt[1], &A[0])) {
            fp fDeltaX, fDeltaY;
            fp fRet;
            fDeltaX = A[1].h - pt[0].h;
            fDeltaY = A[1].v - pt[0].v;
            if (fabs (fDeltaX) > fabs(fDeltaY)) {
               fp m, b;
               m = fDeltaY / fDeltaX;
               b = pt[0].v - m * pt[0].h;

               fRet = m * A[0].h + b;
               if (fabs(fRet - A[0].v) < CLOSE)
                  fLinear = TRUE;
            }
            else {
               fp mInv, bInv;
               mInv = fDeltaX / fDeltaY;
               bInv = pt[0].h - mInv * pt[0].v;
               fRet = mInv * A[0].v + bInv;
               if (fabs(fRet - A[0].h) < CLOSE)
                  fLinear = TRUE;
            }

            if (fLinear)
               pt[1] = A[1];
            // dont need to add since just merged the two lines
         }
      }
      if (!fLinear)
         pNew->Add (&A[0]);

      fCurH = fCurEnd;
   }


finished:
#if 0//def _DEBUG
   // display all the sequences
   char szTemp[64];
   DWORD j;
   for (j = 0; j < pNew->Num(); j++) {
      PTEXTUREPOINT p = (PTEXTUREPOINT) pNew->Get(j);

      sprintf (szTemp, "(%g,%g) to (%g,%g), ", (double)p[0].h, (double)p[0].v, (double)p[1].h, (double)p[1].v);
      OutputDebugString (szTemp);
   }

   OutputDebugString ("\r\n");
#endif

   return pNew;
}

/***********************************************************************************
LineSegmentsForWallToPoints - Takes the output from LineSegmentsForWall() and
returns a new CListFixed which is a list of point that can be passed directly
to CSpline::Init(). The points are clockwise, and use the roof to cut off the top
part of the wall.

inputs
   PCListFixed    plLine - List of TEXTUREPOINT[2]
returns
   PCListFixed - list of TEXTUREPOINT. Must be freed by the caller Pass into
      CSpline::Init() as a looped point.
*/
PCListFixed LineSegmentsFromWallToPoints (PCListFixed plLine)
{
   // if no points return null
   if (!plLine->Num())
      return NULL;

   PCListFixed pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(TEXTUREPOINT));

   // if the first point isn't on the far left then add some stuff to
   // make it so
   TEXTUREPOINT A[2];
   PTEXTUREPOINT pt, pt2;

   // loop through all the segments
   DWORD i;
   TEXTUREPOINT zero, tTemp;
   zero.h = zero.v = 0;
   for (i = 0; i < plLine->Num(); i++) {
      pt = (PTEXTUREPOINT) plLine->Get(i);
      pt2 = pNew->Num() ? (PTEXTUREPOINT) pNew->Get(pNew->Num()-1) : &zero;

      // just to force left and right edges to 0
      if (pt[0].h < CLOSE)
         pt[0].h = 0;
      if (pt[1].h > 1-CLOSE)
         pt[1].h = 1;

      // if there's a difference between the last point (pt2) and the
      // current point then insert a line
      if (pt2 && !AreClose(pt2, &pt[0])) {
         tTemp = *pt2;

         // if they're the same H then draw a line connecting the two
         // BUGFIX - The .01 used to be EPSILON, then CLOSE, now larger
         if ((tTemp.h + .01 > pt[0].h) && (tTemp.h - .01 < pt[0].h)) {
            // they're basically the same in H, so draw a line connecting
            pNew->Add (&pt[0]);
         }
         else {
            // the're distance in H
            // draw a line from the last point (top) to this point (top)
            A[0] = tTemp;
            A[1].v = 0;
            A[1] = pt[0];
            A[1].v = 0;
            if (!AreClose (&A[0], &A[1]))
               pNew->Add (&A[0]);

            A[0] = A[1];
            A[1] = pt[0];
            if (!AreClose (&A[0], &A[1]))
               pNew->Add (&A[0]);

            // draw a line to  this point
            pNew->Add (&pt[0]);
         }
      }

      // add the new point
      pNew->Add (&pt[1]);
   }
   
   // close off the loop
   // make sure the end point is all the way to the right
   pt = (PTEXTUREPOINT) pNew->Get(pNew->Num()-1);
   if (pt->h != 1) {
      // draw a line from here to the right
      A[0] = *pt;
      A[1] = A[0];
      A[1].v = 0;
      if (!AreClose (&A[0], &A[1]))
         pNew->Add (&A[1]);

      A[0] = A[1];
      A[1].h = 1;
      pNew->Add (&A[1]);
   }
   
   // make sure the end point is all the way to the right and bottom
   pt = (PTEXTUREPOINT) pNew->Get(pNew->Num()-1);
   if ((pt->h >= 1) && (pt->v < 1)) {
      tTemp.h = 1;
      tTemp.v = 1;
      pNew->Add (&tTemp);
   }

   // get the first point. I know it's all the way on the left. If it's all the
   // way on the left and bottom then don't need to do anything
   pt = (PTEXTUREPOINT) pNew->Get(0);
   if (pt->v < 1) {
      tTemp.h = 0;
      tTemp.v = 1;
      pNew->Add (&tTemp);
   }

   // should be a full loop by now
   return pNew;
}


/***************************************************************************************
PathFromSequences - Given a list of sequences, this figureout out which path through
the sequences produces a closed loop. If a closed loop is produced then it adds
the loop (which is a list of DWORDs indicating sequence IDs) to the list.

inputs
   PCListFixed       pl - List of PCListFixed. Each of those is a list of TEXTUREPOINT
                     for the sequences. From LineSegmentsToLineSequences()
   PCListVariable    pListPath - List to add the path to (if it completes). It will
                     be added as a series of DWORDs indicating the iD
   DWORD             *padwHistory - Pointer to an array of DWORDs containing the
                     path looked at so far.
//   PCListFixed       plScores - List of scores. From SequencesSortByScores() or  SequencesScores()
//                                 If this is NULL, not used. otherwise, used as a speed optimization
//                                 to eliminate paths that would result in a higher score than lowest.
//
//                                 NOTE: Assuming all scores are 0 or greater, and things are optimized
//                                 if scores are sorted from 0 and up.

   DWORD             dwNum - Number of DWORDs in padwHistory that are valid. Should start out with 0.
   DWORD             dwMax - Maximum number of DWORDs in padwHistory
//   int               iStartScore - Starting score. Caller should pass in 0.
//   int               *piLowestScore - Lowest score yet encountered. Caller should pass in NULL if
//                     it doesn't want to know.
   PTEXTUREPOINT     pInPath - If not NULL, then only paths that have this point within the
                     path loop will be accepted
returns
   none
*/
void PathFromSequences (PCListFixed pl, PCListVariable pListPath, DWORD *padwHistory,
                        DWORD dwNum, DWORD dwMax, PTEXTUREPOINT pInPath)
{
//   int *paiScores = plScores ? (int*) plScores->Get(0) : NULL;

   // if too deep give up
   if (dwNum+1 >= dwMax)
      return;

   // if there is no history then special case loop through all the
   // paths
   DWORD i, j;
   PCListFixed plTP;
   if (!dwNum) {
      //int iLowest;
      //if (!piLowestScore)
      //   piLowestScore = &iLowest;
      //*piLowestScore = 1000000;

      DWORD i;
      //int iScore;
      for (i = 0; i < pl->Num(); i++) {
         padwHistory[0] = i;
         //iScore = iStartScore + (paiScores ? paiScores[i] : 0);

         // Shouldnt be doing this test here because if add future bits might be negative
         // Should use >, not >=
         // see if score too low
         //if (iScore >= *piLowestScore)
         //   continue;

         //PathFromSequences (pl, pListPath, padwHistory, plScores,
         //   dwNum+1, dwMax, iScore, piLowestScore);
         PathFromSequences (pl, pListPath, padwHistory,
            dwNum+1, dwMax, pInPath);
      }
      goto alldone;
   }

   // find the last point in the current node
   plTP = *((PCListFixed*) pl->Get(padwHistory[dwNum-1]));
   PTEXTUREPOINT pLast;
   pLast = (PTEXTUREPOINT) plTP->Get(plTP->Num()-1);

   // Shouldnt be doing this test here because if add future bits might be negative
   // Should use >, not >=
   //if (iStartScore >= *piLowestScore)
   //   return;

   // loop through all the sequences and see if they can be used
   PTEXTUREPOINT pFirst;
   for (i = 0; i < pl->Num(); i++) {
      plTP = *((PCListFixed*) pl->Get(i));
      pFirst = (PTEXTUREPOINT) plTP->Get(0);

      // see if score too low
      //int iScore;
      //iScore = iStartScore + (paiScores ? paiScores[i] : 0);
      // Shouldnt be doing this test here because if add future bits might be negative
      // Should use >, not >=
      //if (iScore >= *piLowestScore)
      //   continue;

      // if not close enough to connect then skip
      if (!AreClose (pLast, pFirst))
         continue;

      // if this is back to the beginning then add
      BOOL fSkip;
      fSkip = FALSE;
      if (i == padwHistory[0]) {
#if 0 // I dont think I need this
         // look for duplicates
         DWORD *padw, dw, k;
         for (j = 0; j < pListPath->Num(); j++) {
            dw = pListPath->Size(j) / sizeof(DWORD);
            padw = (DWORD*) pListPath->Get(j);
            if (dw != dwNum)
               continue;   // not the same number so cant match

            for (k = 0; k < dw; k++)
               if (padwHistory[0] == padw[k])
                  break;
            if (k >= dw)
               continue;   // no match of the first one

            // see if the rest match
            DWORD dwStart;
            dwStart = k;
            for (k = 1; k < dw; k++)
               if (padwHistory[k] != padw[(k+dwStart)%dw])
                  break;
            if (k < dw)
               continue;   // didn't match so not duplicate

            // else, if get here, it matched
            fSkip = TRUE;
            break;
         }

#endif // 0
         // BUGFIX - If the pInPath point is not within the loop then get rid of it
         if (!fSkip && pInPath) {
            int iRet = SequenceRightCount(pInPath, padwHistory, dwNum, pl);
            if (iRet != 2)
               fSkip = TRUE;
         }

         // add it
         if (!fSkip) {
            //if (iScore > *piLowestScore)
            //   continue;
            //if (iScore < *piLowestScore)
            //   *piLowestScore = iScore;

            pListPath->Add (padwHistory, dwNum * sizeof(DWORD));
#if 0//def _DEBUG
            // display all the sequences
            char szTemp[64];
            //DWORD i, j;
            DWORD *padw, dw;
            sprintf (szTemp, "Path %d:", pListPath->Num());
            OutputDebugString (szTemp);

            dw = dwNum;
            padw = padwHistory;

            for (j = 0; j < dw; j++) {
               sprintf (szTemp, " %d", padw[j]);
               OutputDebugString (szTemp);
            }

            OutputDebugString ("\r\n");
#endif
         }

         continue;
      }

      // if this matches any that we've already done then skip
      for (j = 1; j < dwNum; j++)   // intentionally start at 1 since know 0 doesnt match
         if (i == padwHistory[j]) {
            fSkip = TRUE;
            break;
         }
      if (fSkip)
         continue;

      // if this matches the start of any loop we've already added then we've
      // already been here, so skip
      // I'm pretty sure I can do this and I won't eliminate anything improperly
      DWORD *pdw;
      for (j = 0; j < pListPath->Num(); j++) {
         pdw = (DWORD*) pListPath->Get(j);
         if (*pdw == i) {
            fSkip = TRUE;
            break;
         }
      }
      if (fSkip)
         continue;

      // BUGFIX - If the end of this path matches the start of any of the paths in the
      // list then don't use because the path crosses overitself and forms a figure 8.
      // Put this in so that creating the path around the point that clicked wont have
      // figure 8
      if (dwNum >= 2) {
         PTEXTUREPOINT ptEnd, ptStart;
         PCListFixed plComp;
         plComp = *((PCListFixed*) pl->Get(padwHistory[dwNum-1]));
         ptEnd = (PTEXTUREPOINT) plComp->Get(plComp->Num()-1);
         for (j = 0; j < dwNum-1; j++) {
            plComp = *((PCListFixed*) pl->Get(padwHistory[j]));
            ptStart = (PTEXTUREPOINT) plComp->Get(0);
            if (AreClose(ptEnd, ptStart))
               break;
         }
         if (j < dwNum-1)
            continue;
      }

      // if got here than try going down this path
      padwHistory[dwNum] = i;
      //PathFromSequences (pl, pListPath, padwHistory, plScores,
      //   dwNum+1, dwMax, iScore, piLowestScore);
      PathFromSequences (pl, pListPath, padwHistory,
         dwNum+1, dwMax, pInPath);
   }

alldone:
   // NOTE: Shouldnt have any duplicates if get here because of test against
   // pListPath for repeating what have already done
#if 0//def _DEBUG
   if (dwNum == 0) {
      // display all the sequences
      char szTemp[64];
      //DWORD i, j;
      DWORD *padw, dw;
      for (i = 0; i < pListPath->Num(); i++) {
         sprintf (szTemp, "Path %d:", i);
         OutputDebugString (szTemp);

         dw = pListPath->Size(i) / sizeof(DWORD);
         padw = (DWORD*) pListPath->Get(i);

         for (j = 0; j < dw; j++) {
            sprintf (szTemp, " %d", padw[j]);
            OutputDebugString (szTemp);
         }

         OutputDebugString ("\r\n");
      }
   }
#endif
   return;
}


/***************************************************************************************
EliminateDuplicateLines - Self-modifies the line list and eliminates duplicates.

inputs
   PCListFixed       plLines - Pointer to a CListFixed with TEXTUREPOINT[2] elements.
         This is modified IN PLACE.
*/
void EliminateDuplicateLines (PCListFixed plLines)
{
   DWORD i,j;
   PTEXTUREPOINT p1, p2;
   for (i = 0; i < plLines->Num(); i++) {
      for (j = plLines->Num()-1; j > i; j--) {
         p1 = (PTEXTUREPOINT) plLines->Get(i);
         p2 = (PTEXTUREPOINT) plLines->Get(j);
         if (AreClose(p1+0,p2+0) && AreClose(p1+1,p2+1)) {
            plLines->Remove (j);
         }
      }  // j
   } // i
}

/***************************************************************************************
LinesMinimize - Minimize the number of lines by intersecting them into sequences and then
expanding them back into lines. This will merge lines together. It will also expand the
sequences.

inputs
   PCListFixed       plLines - Pointer to a CListFixed with TEXTUREPOINT[2] elements.
         This is modified IN PLACE.
   fp                fExtend - Amount to extend the line, in HV units, to encourage overlap
retursn
   none
*/
void LinesMinimize (PCListFixed plLines, fp fExtend)
{
   if (!plLines->Num())
      return;  // no point since no lines

   // eliminate any duplicates in this list because there may well be
   EliminateDuplicateLines (plLines);

   // BUGFIX - Originally didn't think could use cutouts because of the doors in walls
   // would not make a perfect intersection, but since intersecting against the center
   // I can. Want to use the cutouts so that walls on ground floor don't intrude to floor
   // above. NOTE: This doesn't really seem to fix the walls from ground floor intruding above
   // problem

   // BUGFIX - Mirror interected lines so they go in both directions. Before doing that,
   // convert into sequences so that will minimize the number of line segments

   DWORD i, j;
   PCListFixed plLines2;
   plLines2 = LineSegmentsToSequences (plLines);
   if (!plLines2)
      return;
   if (!plLines2->Num()) {
      delete plLines2;
      return;
   }

   PCListFixed pt;
#if 0//def _DEBUG
   // display all the sequences
   char szTemp[64];
   for (j = 0; j < plLines->Num(); j++) {
      PTEXTUREPOINT p = (PTEXTUREPOINT) plLines->Get(j);

      sprintf (szTemp, "(%g,%g) to (%g,%g)\r\n", (double)p[0].h, (double)p[0].v, (double)p[1].h, (double)p[1].v);
      OutputDebugString (szTemp);
   }

   OutputDebugString ("\r\n");
#endif

   // BUGFIX - Extend the sequences by a bit on either end to make sure that if they're
   // almost touching that they really do touch
   for (i = 0; i < plLines2->Num(); i++) {
      pt = *((PCListFixed*)plLines2->Get(i));

      DWORD dwNum;
      PTEXTUREPOINT ptp;
      dwNum = pt->Num();
      ptp = (PTEXTUREPOINT) pt->Get(0);
      if ((dwNum >= 2) && fExtend) for (j = 0; j < 2; j++) {
         TEXTUREPOINT td;
         fp fLen;
         if (j == 0) {
            td.h = ptp[0].h - ptp[1].h;
            td.v = ptp[0].v - ptp[1].v;
         }
         else {
            td.h = ptp[dwNum-1].h - ptp[dwNum-2].h;
            td.v = ptp[dwNum-1].v - ptp[dwNum-2].v;
         }
         fLen = sqrt(td.h * td.h + td.v * td.v);
         if (fLen > CLOSE) {
            td.h = td.h / fLen * fExtend;
            td.v = td.v / fLen * fExtend;
         }
         ptp[j ? (dwNum-1) : 0].h += td.h;
         ptp[j ? (dwNum-1) : 0].v += td.v;
      }
   }

#if 0//def _DEBUG
   // display all the sequences
   for (i = 0; i < plLines2->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)plLines2->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }

   OutputDebugString ("\r\n");
#endif

   plLines->Clear();
   for (i = 0; i < plLines2->Num(); i++) {
      PCListFixed pSeg = *((PCListFixed*)plLines2->Get(i));
      PTEXTUREPOINT pt = (PTEXTUREPOINT) pSeg->Get(0);
      for (j = 0; j+1 < pSeg->Num(); j++) {
         TEXTUREPOINT atp[2];
         atp[0] = pt[j];
         atp[1] = pt[j+1];
         plLines->Add (&atp);
      }
      delete pSeg;
   }
   delete plLines2;
}

/***************************************************************************************
SequencesScores - This is used to determine which paths (set of connecting sequences)
are inside (holding the smallest area) and which are outside. Basically, take a point
from path A (that's not on the edge) and see which side of all the other paths
its on. Because the paths don't intersect, A is definitely one one side or another.
Sum the scores up if it's a keeper, don't change. If it's not kept thne add one.

inputs
   PCListFixed       pl - List from LineSegmentsToLineSequence
   BOOL              fKeepOnLeft - If TRUE, then keep values on the left side of the
                     line, else keep on the right.
returns
   PCListFixed - List of int. One value per pl->Num().
*/
PCListFixed SequencesScores (PCListFixed pl, BOOL fKeepOnLeft)
{
   DWORD i, j, k;
   PCListFixed plTPI;
   DWORD dwI;
   PTEXTUREPOINT pI;
   // makre sure that if any start/finishes are close that they're changed
   // to exactly euqal
   for (i = 0; i < pl->Num(); i++) {
      plTPI = *((PCListFixed*) pl->Get(i));
      pI = (PTEXTUREPOINT) plTPI->Get(0);
      dwI = plTPI->Num();

      PCListFixed plTPJ;
      DWORD dwJ;
      PTEXTUREPOINT pJ;
      for (j = i+1; j < pl->Num(); j++) {
         plTPJ = *((PCListFixed*) pl->Get(j));
         pJ = (PTEXTUREPOINT) plTPJ->Get(0);
         dwJ = plTPJ->Num();

         if (AreClose (&pI[0], &pJ[dwJ-1]))
            pJ[dwJ-1] = pI[0];
         if (AreClose (&pI[0], &pJ[0]))
            pJ[0] = pI[0];
         if (AreClose (&pI[dwI-1], &pJ[0]))
            pJ[0] = pI[dwI-1];
         if (AreClose (&pI[dwI-1], &pJ[dwJ-1]))
            pJ[dwJ-1] = pI[dwI-1];

      }
   }

   // loop through all the sequences and pick one point from one line
   // on each sequence... this will be used as a non-uniform way of testing
   // how much area is covered by a path
   // figure out point
   CListFixed  lPoints; // list of texturepoints for points using
   CListFixed  lTested; // list of DWORDs for the line used to generate the point
   lPoints.Init (sizeof(TEXTUREPOINT));
   lTested.Init (sizeof(DWORD));
   for (i = 0; i < pl->Num(); i++) {
      DWORD dwLineTested;
      TEXTUREPOINT pTest;

      // pick a point on this sequence. Use a point half way between
      // the first two points
      plTPI = *((PCListFixed*) pl->Get(i));

      // I know that because sequences don't intersect, that if I take a point
      // on the sequence (that's not an end-point), then the intersection score I
      // get for that point is EXACTLY the same as the intersection score for
      // any point on the sequence

      // try to find a line segments that isn't horizontal
      pI = (PTEXTUREPOINT) plTPI->Get(0);
      dwI = plTPI->Num();

      for (j = 0; j+1 < plTPI->Num(); j++) {
         if (fabs(pI[j].v - pI[j+1].v) > EPSILON)
            break;
      }
      if (j+1 >= plTPI->Num()) {
         dwLineTested = 0; // remember this
         pTest.h = (pI[0].h + pI[1].h) / 2;
         pTest.v = max(pI[0].v, pI[1].v);
      }
      else {
         dwLineTested = j; // remember this
         pTest.h = (pI[j].h + pI[j+1].h) / 2;
         pTest.v = (pI[j].v + pI[j+1].v) / 2;
      }

      lPoints.Add (&pTest);
      lTested.Add (&dwLineTested);
   }


   PCListFixed pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(int));

   BOOL fLeft;

   // loop through all the sequences and find out the number of points left, right, cant tell
   for (i = 0; i < pl->Num(); i++) {
      plTPI = *((PCListFixed*) pl->Get(i));
      pI = (PTEXTUREPOINT) plTPI->Get(0);
      dwI = plTPI->Num();

      // count the number of points in different circumstances
      DWORD dwOnLeft, dwOnRight, dwOnLine;
      dwOnLeft = dwOnRight = dwOnLine = 0;

      // trivial elimination
      // need to find a point in "i" that is within the min/max V of "j"
      fp fMaxI, fMinI;
      fMaxI = fMinI = 0;
      for (k = 0; k < plTPI->Num(); k++) {
         if (!k) {
            fMaxI = fMinI = pI[k].v;
            continue;
         }
         fMaxI = max(fMaxI, pI[k].v);
         fMinI = min(fMinI, pI[k].v);
      }

      // loop through all the points
      for (j = 0; j < lPoints.Num(); j++) {
         PTEXTUREPOINT ptp = (PTEXTUREPOINT) lPoints.Get(j);
         DWORD dwLine = *((DWORD*) lTested.Get(j));



         // if test point completely below/above object then it's either entirely in
         // or out
         // NOTE: Using <= for a reason, and > for a reason
         if ((ptp->v <= fMinI) || (ptp->v > fMaxI)) {
            // not going to encounder so just ignore
            // NOTE - May want to test that it's that i!=j?
            continue;
         }

         // loop through all these points
         for (k = 0; k+1 < plTPI->Num(); k++) {
            // don't bother with itself
            if ((i == j) && (dwLine == k)) {
               dwOnLeft++; // if self, always assume it's on the left
               continue;
            }

            // if beyond top or bottom then ignore
            // NOTE: Using <= for a reason. and > for a reason
            if ((ptp->v <= min(pI[k+1].v, pI[k].v)) || (ptp->v > max(pI[k+1].v, pI[k].v)) )
               continue;

            // NOTE: This will not fall through a crack since at the top made
            // sure that all sequences that start and end closely are exact
            // start and end

            fp fDeltaX, fDeltaY;
            fDeltaX = pI[k+1].h - pI[k].h;
            fDeltaY = pI[k+1].v - pI[k].v;

            // if no deltaY then probably can ignore the line
            if (fabs(fDeltaY) < EPSILON)
               continue;

            // trivial reject on left/right
            if (ptp->h < min(pI[k+1].h, pI[k].h)) {
               fLeft = (fDeltaY < 0);
               if (!fKeepOnLeft)
                  fLeft = !fLeft;
               if (!fLeft)
                  dwOnRight++;
               else
                  dwOnLeft++;
               continue;
            }

            if (ptp->h > max(pI[k+1].h, pI[k].h)) {
               fLeft = (fDeltaY > 0);
               if (!fKeepOnLeft)
                  fLeft = !fLeft;
               if (!fLeft)
                  dwOnRight++;
               else
                  dwOnLeft++;
               continue;
            }

            // calculate y=mx+b
            // if this is to the right of the line (as opposed to right side of line)
            // then ignore
            fp fLineH;
            fp m, b;
            if (fabs(fDeltaX) > EPSILON) {
               m = fDeltaY / fDeltaX;
               b =  pI[k].v - m * pI[k].h;

               if (fabs(m) > EPSILON) {
                  fLineH = (ptp->v - b) / m;
                  fLeft = (ptp->h < fLineH);
               }
               else {   // left/right
                  dwOnLine++;
                  continue;   // horizontal, so no change
               }
            }
            else {   // fDeltaX = 0
               // it's up/down
               fLeft = (ptp->h < pI[k].h);
            }

            // accound for line going down
            if (fDeltaY >= 0)
               fLeft = !fLeft;

            //fLeft = (m * ptp->h + b - ptp->v) < 0;  // BUGFIX - Was > 0
            if (!fKeepOnLeft)
               fLeft = !fLeft;
            if (!fLeft)
               dwOnRight++;
            else
               dwOnLeft++;
         }

      } // loop over all sequences

      // lower scores are better, and so are more points on the left
      // however, ultimately want as few points as possible on the left since that
      // would mean the largest box - and really want the smallest.
      int iScore;
      iScore = -((int) dwOnRight - (int) dwOnLeft);

#if 0
      // keep track of the smallest the sum gets
      if (i)
         iMaxInside = max(iMaxInside, iInside);
      else
         iMaxInside = iInside;
#endif //0

      // save this
      pNew->Add (&iScore);
   }

#if 0
   // loop through the sums and offset so that anything with the minimum
   // sum is set to 0
   int *pi;
   for (i = 0; i < pNew->Num(); i++) {
      pi = (int*) pNew->Get(i);
      *pi = iMaxInside - *pi; // so that lower values are better
   }
#endif // 0
   return pNew;
}


#ifdef BROKEN
/***************************************************************************************
SequencesScores - This is used to determine which paths (set of connecting sequences)
are inside (holding the smallest area) and which are outside. Basically, take a point
from path A (that's not on the edge) and see which side of all the other paths
its on. Because the paths don't intersect, A is definitely one one side or another.
Sum the scores up if it's a keeper, don't change. If it's not kept thne add one.

inputs
   PCListFixed       pl - List from LineSegmentsToLineSequence
   BOOL              fKeepOnLeft - If TRUE, then keep values on the left side of the
                     line, else keep on the right.
returns
   PCListFixed - List of int. One value per pl->Num().
*/
PCListFixed SequencesScores (PCListFixed pl, BOOL fKeepOnLeft)
{
   PCListFixed pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(int));

   DWORD i, j, k;
   PCListFixed plTPJ, plTPI;
   DWORD dwJ, dwI;
   PTEXTUREPOINT pJ, pI;
   TEXTUREPOINT pTest;
   DWORD dwLineTested;

   // makre sure that if any start/finishes are close that they're changed
   // to exactly euqal
   for (i = 0; i < pl->Num(); i++) {
      plTPI = *((PCListFixed*) pl->Get(i));
      pI = (PTEXTUREPOINT) plTPI->Get(0);
      dwI = plTPI->Num();

      for (j = i+1; j < pl->Num(); j++) {
         plTPJ = *((PCListFixed*) pl->Get(j));
         pJ = (PTEXTUREPOINT) plTPJ->Get(0);
         dwJ = plTPJ->Num();

         if (AreClose (&pI[0], &pJ[dwJ-1]))
            pJ[dwJ-1] = pI[0];
         if (AreClose (&pI[0], &pJ[0]))
            pJ[0] = pI[0];
         if (AreClose (&pI[dwI-1], &pJ[0]))
            pJ[0] = pI[dwI-1];
         if (AreClose (&pI[dwI-1], &pJ[dwJ-1]))
            pJ[dwJ-1] = pI[dwI-1];

      }
   }

   BOOL fLeft;
   int iMaxInside;
   iMaxInside = 0;

   // figure out point
   for (i = 0; i < pl->Num(); i++) {
      // pick a point on this sequence. Use a point half way between
      // the first two points
      plTPI = *((PCListFixed*) pl->Get(i));

      // I know that because sequences don't intersect, that if I take a point
      // on the sequence (that's not an end-point), then the intersection score I
      // get for that point is EXACTLY the same as the intersection score for
      // any point on the sequence

      // try to find a line segments that isn't horizontal
      pI = (PTEXTUREPOINT) plTPI->Get(0);
      dwI = plTPI->Num();

      for (j = 0; j+1 < plTPI->Num(); j++) {
         if (fabs(pI[j].v - pI[j+1].v) > EPSILON)
            break;
      }
      if (j+1 >= plTPI->Num()) {
         dwLineTested = 0; // remember this
         pTest.h = (pI[0].h + pI[1].h) / 2;
         pTest.v = max(pI[0].v, pI[1].v);
      }
      else {
         dwLineTested = j; // remember this
         pTest.h = (pI[j].h + pI[j+1].h) / 2;
         pTest.v = (pI[j].v + pI[j+1].v) / 2;
      }


      // calculate the score
      int iInside;
      iInside = 0;   // positive values mean inside more lines
      fp fMaxJ, fMinJ;

      // loop through all the other sequences
      for (j = 0; j < pl->Num(); j++) {

         // even test against selt
         plTPJ = *((PCListFixed*) pl->Get(j));
         pJ = (PTEXTUREPOINT) plTPJ->Get(0);
         dwJ = plTPJ->Num();

         // trivial elimination
         // need to find a point in "i" that is within the min/max V of "j"
         fMaxJ = fMinJ = 0;
         for (k = 0; k < plTPJ->Num(); k++) {
            if (!k) {
               fMaxJ = fMinJ = pJ[k].v;
               continue;
            }
            fMaxJ = max(fMaxJ, pJ[k].v);
            fMinJ = min(fMinJ, pJ[k].v);
         }

         // if test point completely below/above object then it's either entirely in
         // or out
         // NOTE: Using <= for a reason, and > for a reason
         if ((i != j) && ((pTest.v <= fMinJ) || (pTest.v > fMaxJ))) {
            // not going to encounder so just ignore
            //iInside--;
            continue;
         }

         // loop through all these points
         for (k = 0; k+1 < plTPJ->Num(); k++) {
            // don't bother with itself
            if ((i == j) && (dwLineTested == k)) {
               iInside++;
               continue;
            }

            // if beyond top or bottom then ignore
            // NOTE: Using <= for a reason. and > for a reason
            if ((pTest.v <= min(pJ[k+1].v, pJ[k].v)) || (pTest.v > max(pJ[k+1].v, pJ[k].v)) )
               continue;

            // NOTE: This will not fall through a crack since at the top made
            // sure that all sequences that start and end closely are exact
            // start and end

            fp fDeltaX, fDeltaY;
            fDeltaX = pJ[k+1].h - pJ[k].h;
            fDeltaY = pJ[k+1].v - pJ[k].v;

            // if no deltaY then probably can ignore the line
            if (fabs(fDeltaY) < EPSILON)
               continue;

            // trivial reject on left/right
            if (pTest.h < min(pJ[k+1].h, pJ[k].h)) {
               fLeft = (fDeltaY < 0);
               if (!fKeepOnLeft)
                  fLeft = !fLeft;
               if (!fLeft)
                  iInside--;
               else
                  iInside++;
               continue;
            }

            if (pTest.h > max(pJ[k+1].h, pJ[k].h)) {
               fLeft = (fDeltaY > 0);
               if (!fKeepOnLeft)
                  fLeft = !fLeft;
               if (!fLeft)
                  iInside--;
               else
                  iInside++;
               continue;
            }

            // calculate y=mx+b
            // if this is to the right of the line (as opposed to right side of line)
            // then ignore
            fp fLineH;
            fp m, b;
            if (fabs(fDeltaX) > EPSILON) {
               m = fDeltaY / fDeltaX;
               b =  pJ[k].v - m * pJ[k].h;

               if (fabs(m) > EPSILON) {
                  fLineH = (pTest.v - b) / m;
                  fLeft = (pTest.h < fLineH);
               }
               else {   // left/right
                  continue;   // horizontal, so no change
               }
            }
            else {   // fDeltaX = 0
               // it's up/down
               fLeft = (pTest.h < pJ[k].h);
            }

            // accound for line going down
            if (fDeltaY >= 0)
               fLeft = !fLeft;

            //fLeft = (m * pTest.h + b - pTest.v) < 0;  // BUGFIX - Was > 0
            if (!fKeepOnLeft)
               fLeft = !fLeft;
            if (!fLeft)
               iInside--;
            else
               iInside++;
         }

      } // loop over all sequences

      // keep track of the smallest the sum gets
      if (i)
         iMaxInside = max(iMaxInside, iInside);
      else
         iMaxInside = iInside;

      // save this
      pNew->Add (&iInside);
   }

   // loop through the sums and offset so that anything with the minimum
   // sum is set to 0
   int *pi;
   for (i = 0; i < pNew->Num(); i++) {
      pi = (int*) pNew->Get(i);
      *pi = iMaxInside - *pi; // so that lower values are better
   }

   return pNew;
}
#endif // BROKEN

/***************************************************************************************
PathWithLowestScore - Used to calculate roof intersections, seaches through
all the paths and returns the one with the lowest score. The score is gotten
by calling SequenceScores().

inputs
   PCListFixed       pListSequences - List of sequences from LineSegmentsToLineSequences()
   PCListVariable    pListPaths - List of paths from PathFromSequences()
   PCListFixed       plScores - List of scores. From SequencesSortByScores() or  SequencesScores()
returns
   DWORD - Index into pListPaths for the one with the lowest score, and hence the
            one left after all the cutting
*/
DWORD PathWithLowestScore (PCListFixed pListSequences, PCListVariable pListPaths, PCListFixed plScores)
{
   int *paiScores;
   paiScores = (int*) plScores->Get(0);

   // find it
   DWORD dwBest, i, j;
   int   iBestScore;
   dwBest = (DWORD)-1;
   for (i = 0; i < pListPaths->Num(); i++) {
      DWORD dw, *pdw;
      pdw = (DWORD*) pListPaths->Get(i);
      dw = (DWORD)pListPaths->Size(i) / sizeof(DWORD);

      int iScore;
      iScore = 0;
      for (j = 0; j < dw; j++)
         iScore += paiScores[pdw[j]];

      if ((dwBest == (DWORD)-1) || (iScore < iBestScore)) {
         dwBest = i;
         iBestScore = iScore;
      }
   }

   return dwBest;
}


/***************************************************************************************
PathWithHighestScore - Used to calculate roof intersections, seaches through
all the paths and returns the one with the highest score. The score is gotten
by calling SequenceScores().

inputs
   PCListFixed       pListSequences - List of sequences from LineSegmentsToLineSequences()
   PCListVariable    pListPaths - List of paths from PathFromSequences()
   PCListFixed       plScores - List of scores. From SequencesSortByScores() or  SequencesScores()
returns
   DWORD - Index into pListPaths for the one with the highest score, and hence the
            one left after all the cutting
*/
DWORD PathWithHighestScore (PCListFixed pListSequences, PCListVariable pListPaths, PCListFixed plScores)
{
   int *paiScores;
   paiScores = (int*) plScores->Get(0);

   // find it
   DWORD dwBest, i, j;
   int   iBestScore;
   dwBest = (DWORD)-1;
   for (i = 0; i < pListPaths->Num(); i++) {
      DWORD dw, *pdw;
      pdw = (DWORD*) pListPaths->Get(i);
      dw = (DWORD)pListPaths->Size(i) / sizeof(DWORD);

      int iScore;
      iScore = 0;
      for (j = 0; j < dw; j++)
         iScore += paiScores[pdw[j]];

      if ((dwBest == (DWORD)-1) || (iScore > iBestScore)) {
         dwBest = i;
         iBestScore = iScore;
      }
   }

   return dwBest;
}
/*************************************************************************************
SequencesSortByScores  - Calculate the scores for all the sequences (which is returned
as a CListFixed  that must be freed by the caller). Then, the sequences and scores
are sorted so the low scores are first. This will make finding paths faster.

NOTE: This also eliminates duplicate line sequences.
inputs
   PCListFixed       pl - List from LineSegmentsToLineSequence
   BOOL              fKeepOnLeft - If TRUE, then keep values on the left side of the
                     line, else keep on the right.
   PTEXTUREPOINT     pCenter - If this is NULL then sequences are sorted by scores,
                     as normal. If not NULL, sequences are sorted by their proximity
                     to pCenter, with nearer sequences first.
returns
   PCListFixed - List of ints indicating the score of each sequence. This must be freed.
*/
typedef struct {
   int            iScore;
   fp             fCenterScore;  // center distance score
   PCListFixed    pl;
} SSS, *PSSS;

static int _cdecl SSSSort (const void *elem1, const void *elem2)
{
   PSSS p1, p2;
   p1 = (PSSS) elem1;
   p2 = (PSSS) elem2;

   if (p1->fCenterScore < p2->fCenterScore)
      return -1;
   else if (p1->fCenterScore > p2->fCenterScore)
      return 1;

   return p1->iScore - p2->iScore;
}

PCListFixed SequencesSortByScores (PCListFixed pl, BOOL fKeepOnLeft, PTEXTUREPOINT pCenter)
{
   // eliminate duplicate sequences
   DWORD i, j, k;
   PCListFixed plJ, plI;
   PTEXTUREPOINT ptJ, ptI;
   DWORD dwJ, dwI;
   for (i = 0; i < pl->Num(); i++) {
      plI = *((PCListFixed*) pl->Get(i));
      ptI = (PTEXTUREPOINT) plI->Get(0);
      dwI = plI->Num();

      for (j = pl->Num()-1; j > i; j--) {
         plJ = *((PCListFixed*) pl->Get(j));
         ptJ = (PTEXTUREPOINT) plJ->Get(0);
         dwJ = plJ->Num();

         if (dwI != dwJ)
            continue;
         for (k = 0; k < dwI; k++)
            if (!AreClose (ptI + k, ptJ + k))
               break;
         if (k < dwI)
            continue;

         // if got here then j is a duplicate
         delete plJ;
         pl->Remove (j);
      }
   }

   PCListFixed plScores = SequencesScores (pl, fKeepOnLeft);
   if (!plScores)
      return NULL;   // error
   int *paiScores;
   paiScores = (int*) plScores->Get(0);

   // create a combined list of scores and sequences
   SSS s;
   CListFixed lSort;
   lSort.Init (sizeof(SSS));
   lSort.Required (pl->Num());
   for (i = 0; i < pl->Num(); i++) {
      s.iScore = paiScores[i];
      s.pl = *((PCListFixed*) pl->Get(i));
      s.fCenterScore = 0;

      if (pCenter) {
         // get the sequence
         PTEXTUREPOINT pts;
         DWORD dwNum;
         dwNum = s.pl->Num();
         pts = (PTEXTUREPOINT) s.pl->Get(0);

         // find the distance from the first and last points
         s.fCenterScore = fabs(pCenter->h - pts[0].h) + fabs(pCenter->v - pts[0].v) +
            fabs(pCenter->h - pts[dwNum-1].h) + fabs(pCenter->v - pts[dwNum-1].v);
      }

      lSort.Add (&s);
   }
   qsort (lSort.Get(0), lSort.Num(), sizeof(SSS), SSSSort);

   // rebuild the lists
   plScores->Clear();
   pl->Clear();
   plScores->Required (lSort.Num());
   pl->Required (lSort.Num());
   for (i = 0; i < lSort.Num(); i++) {
      PSSS p= (PSSS) lSort.Get(i);
      plScores->Add (&p->iScore);
      pl->Add (&p->pl);
   }


#if 0//def _DEBUG
   {
   // display all the sequences
   char szTemp[64];
   PCListFixed pt;
   for (i = 0; i < pl->Num(); i++) {
      sprintf (szTemp, "Sequence %d (score=%d):", i, *((int*)plScores->Get(i)));
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)pl->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
   }
#endif

   return plScores;
}


/***************************************************************************************
PathToSplineList - Takes a list of paths and a path index. Uses the index to
find a specific path and then create a list of TEXTUREPOINTs which can be passed
into a CSpline. Can be used to cut out a section of roof, etc.

inputs
   PCListFixed       pListSequences - List of sequences from LineSegmentsToLineSequences()
   PCListVariable    pListPaths - List of paths from PathFromSequences()
   DWORD             dwPathNum - Path number, 0 .. pListPaths->Num()-1
   BOOL              fReverse - If TRUE, reverse the direction
returns
   PCListFixed - Intiialized to sizeof(CPoint), containing points (assuming a looped
      spline) to draw. Must be freed by the caller.
*/
PCListFixed PathToSplineList (PCListFixed pListSequences, PCListVariable pListPaths,
                              DWORD dwPathNum, BOOL fReverse)
{
   PCListFixed pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(CPoint));

   DWORD i, j;
   DWORD dw, *pdw;
   pdw = (DWORD*) pListPaths->Get(dwPathNum);
   dw = (DWORD)pListPaths->Size(dwPathNum) / sizeof(DWORD);
   if (!pdw) {
      delete pNew;
      return NULL;
   }

   // loop
   CPoint p;
   PTEXTUREPOINT pt;
   for (i = 0; i < dw; i++) {
      PCListFixed plTP = *((PCListFixed*) pListSequences->Get(pdw[i]));

      // loop through all points, ignoring last one since is duplicate with next one
      for (j = 0; j+1 < plTP->Num(); j++) {
         pt = (PTEXTUREPOINT) plTP->Get(j);
         p.Zero();
         p.p[0] = pt->h;
         p.p[1] = pt->v;
         pNew->Add (&p);
      }
   }

   // reverse?
   if (fReverse) {
      PCPoint pp = (PCPoint) pNew->Get(0);
      CPoint t;
      dw = pNew->Num();
      for (i = 0; i < dw/2; i++) {
         t.Copy (&pp[i]);
         pp[i].Copy (&pp[dw-i-1]);
         pp[dw-i-1].Copy (&t);
      }
   }

   return pNew;
}

/***************************************************************************************
IsCurveClockwise - Takes a list of TEXTUREPOINT that are assumed to looped. It sums
up all the angles between the points. This can be used to determine which way is clockwise
because if it goes around clockwise the angles will be lowest.

inputs
   PTEXTUREPOINT     pt - Array of texture points
   DWORD             dwNum - Number of texture points
returns
   BOOL - TRUE if the points go around clockwise, FALSE if counterclockwise
*/
BOOL IsCurveClockwise (PTEXTUREPOINT pt, DWORD dwNum)
{
   fp   fSumClock, fSumCounter;
   fSumClock = fSumCounter = 0;

   // loop
   DWORD i;
   PTEXTUREPOINT pA, pB, pC;
   TEXTUREPOINT t1, t2;
   fp f1, f2;
   for (i = 0; i < dwNum; i++) {
      pA = pt + ((i + dwNum - 1) % dwNum);
      pB = pt + i;   // cetner point
      pC = pt + ((i + 1) % dwNum);

      t1.h = pA->h - pB->h;
      t1.v = pA->v - pB->v;
      t2.h = pC->h - pB->h;
      t2.v = pC->v - pB->v;

      // angle
      f1 = atan2 (t1.v, t1.h);
      f2 = atan2 (t2.v, t2.h);
      if (f1 < 0)
         f1 += 2 * PI;
      if (f2 < 0)
         f2 += 2 * PI;
      if (f1 < f2)
         f1 += 2 * PI;
      f1 -= f2;
      fSumClock += f1;
      fSumCounter += (2 * PI - f1);

   }

   return (fSumClock < fSumCounter);
}



/********************************************************************************
PathToSplineList - Given a set of sequence numbers (not in any particular order),
this combines all the sequences together to form a list of TEXTUREPOINTs. These
can be used to translate an intersection of walls with floor into an outline
for an overlay.

inputs
   PCListFixed       pListSequences - List of sequences from LineSegmentsToLineSequences()
   DWORD             *padwPath - Pointer to an array of DWORDs, that are seuqnece numbers.
                           This order doens't matter
   DWORD             dwNum - Number in padwPath
   BOOL              fClockwise - If TRUE, want path to be clockwise in the end, else counter
returns
   PCListFixed - Intialized to sizeof(TEXTUREPOINT). Contains a clockwise list of
      texturepoints (from the sequences). Or, NULL if a path couldn't be made out of
      the points in padwPath.
*/
PCListFixed PathToSplineList (PCListFixed pListSequences, DWORD *padwPath, DWORD dwNum,
                              BOOL fClockwise)
{
   if (dwNum < 1)
      return NULL;

   // create own list of paths so can remove from the list as they connect
   CListFixed lRemain;
   lRemain.Init (sizeof(DWORD), padwPath+1, dwNum-1);

   // and a list of what have used so part
   CListFixed lUsed;
   lUsed.Init (sizeof(DWORD), padwPath, 1);

   // repeat while there are points remaining
   while (lRemain.Num()) {
      // get the last point used
      DWORD dwLast = *((DWORD*) lUsed.Get(lUsed.Num()-1));

      // find out where that ends
      PTEXTUREPOINT pLastEnd;
      PCListFixed plLast;
      plLast = *((PCListFixed*) pListSequences->Get(dwLast & (~0x80000000)));
      pLastEnd = (PTEXTUREPOINT) plLast->Get((dwLast & 0x80000000) ? 0 : (plLast->Num()-1));

      // see if it connects to any that are left
      DWORD i;
      BOOL fAdded;
      fAdded = FALSE;
      for (i = 0; i < lRemain.Num(); i++) {
         DWORD dwCur = *((DWORD*) lRemain.Get(i));

         PCListFixed pTry;
         pTry = *((PCListFixed*) pListSequences->Get(dwCur));
         PTEXTUREPOINT pt;
         pt = (PTEXTUREPOINT) pTry->Get(0);
         if (AreClose (pt, pLastEnd)) {
            lUsed.Add (&dwCur);
            lRemain.Remove (i);
            fAdded = TRUE;
            break;
         }

         // try the reverse
         pt = (PTEXTUREPOINT) pTry->Get(pTry->Num()-1);
         if (AreClose (pt, pLastEnd)) {
            dwCur |= 0x80000000;
            lUsed.Add (&dwCur);
            lRemain.Remove (i);
            fAdded = TRUE;
            break;
         }
      }
      if (fAdded)
         continue;

      // if didn't add any after check through all the points then cant
      // finish up the loop
      return NULL;
   }

   // make sure beginning connects to end
   PTEXTUREPOINT pStart, pEnd;
   DWORD dw;
   PCListFixed pl;
   dw = *((DWORD*) lUsed.Get(0));
   pl = *((PCListFixed*) pListSequences->Get(dw & (~0x80000000)));
   pStart = (PTEXTUREPOINT) pl->Get((dw & 0x80000000) ? (pl->Num()-1) : 0);
   dw = *((DWORD*) lUsed.Get(lUsed.Num()-1));
   pl = *((PCListFixed*) pListSequences->Get(dw & (~0x80000000)));
   pEnd = (PTEXTUREPOINT) pl->Get((dw & 0x80000000) ? 0 : (pl->Num()-1));

   if (!AreClose (pStart, pEnd))
      return NULL;   // dont connect

   // else, they connect, so add them all
   PCListFixed pNew;
   pNew = new CListFixed;
   if (!pNew)
      return NULL;
   pNew->Init (sizeof(TEXTUREPOINT));
   DWORD i;
   for (i = 0; i < lUsed.Num(); i++) {
      dw = *((DWORD*) lUsed.Get(i));
      pl = *((PCListFixed*) pListSequences->Get(dw & (~0x80000000)));
      pStart = (PTEXTUREPOINT) pl->Get(0);

      DWORD j;
      for (j = 0; j+1 < pl->Num(); j++) {
         // note that doing j+1 so don't repeat points
         pNew->Add (&pStart[(dw & 0x80000000) ? (pl->Num() - j - 1) : j]);
      }
   }

   // if it's not clockwise then reverse it
   pStart = (PTEXTUREPOINT) pNew->Get(0);
   dw = pNew->Num();
   BOOL fIsClockwise;
   fIsClockwise = IsCurveClockwise (pStart, dw);
   if ((!fClockwise) != (!fIsClockwise)) {
      TEXTUREPOINT t;
      for (i = 0; i < dw/2; i++) {
         t = pStart[i];
         pStart[i] = pStart[dw-i-1];
         pStart[dw-i-1] = t;
      }
   }

   return pNew;

}


/********************************************************************************
FindPathBetweenTwoPoints - Given a start and end texturepoint, find all paths
between the two points. The direction of the sequences doesn't matter

inputs
   PTEXTUREPOINT     pStart - Start
   PTEXTUREPOINT     pEnd - End
   PCListFixed       pl - List of sequences
   PCListVariagle    pListPath - New (and valid) paths are added to this
   DWORD             *padwExclude - Pointer to an array of sequences that are excluded from search
   DWORD             dwExcludeNum - Number of elements in padwExclude
   DWORD             *padwHistory - Pointer to an array of DWORDs containing the
                     path looked at so far.
   DWORD             dwNum - Number of DWORDs in padwHistory that are valid. Should start out with 0.
   DWORD             dwMax - Maximum number of DWORDs in padwHistory
*/
void FindPathBetweenTwoPoints (PTEXTUREPOINT pStart, PTEXTUREPOINT pEnd,
                               PCListFixed pl, PCListVariable pListPath,
                               DWORD *padwExclude, DWORD dwExcludeNum,
                               DWORD *padwHistory, DWORD dwNum, DWORD dwMax)
{
//   int *paiScores = plScores ? (int*) plScores->Get(0) : NULL;

   // if too deep give up
   if (dwNum+1 >= dwMax)
      return;

   // Shouldnt be doing this test here because if add future bits might be negative
   // Should use >, not >=
   //if (iStartScore >= *piLowestScore)
   //   return;

   // loop through all the sequences and see if they can be used
   PTEXTUREPOINT pS, pE, pOther;
   PCListFixed plTP;
   DWORD i,j;
   BOOL fCloseStart;
   for (i = 0; i < pl->Num(); i++) {
      plTP = *((PCListFixed*) pl->Get(i));
      pS = (PTEXTUREPOINT) plTP->Get(0);
      pE = (PTEXTUREPOINT) plTP->Get(plTP->Num()-1);

      // see if it's close to where want to be
      if (AreClose (pStart, pS)) {
         fCloseStart = TRUE;
         pOther = pE;
      }
      else if (AreClose (pStart, pE)) {
         fCloseStart = FALSE;
         pOther = pS;
      }
      else
         continue;

      // if it's in any exclusion list then get rid of
      for (j = 0; j < dwExcludeNum; j++)
         if (i == padwExclude[j])
            break;
      if (j < dwExcludeNum)
         continue;

      for (j = 0; j < dwNum; j++)
         if (i == padwHistory[j])
            break;
      if (j < dwNum)
         continue;

      // valid path
      padwHistory[dwNum] = i;

      // if the other other end matches the end we're looking for then have it. done
      if (AreClose (pEnd, pOther)) {
         pListPath->Add (padwHistory, (dwNum+1) * sizeof(DWORD));
         return;
      }

      // else, loop further
      FindPathBetweenTwoPoints (pOther, pEnd, pl, pListPath, padwExclude, dwExcludeNum,
         padwHistory, dwNum+1, dwMax);
   }

#if 0//def _DEBUG
   if (dwNum == 0) {
      // display all the sequences
      char szTemp[64];
      //DWORD i, j;
      DWORD *padw, dw;
      for (i = 0; i < pListPath->Num(); i++) {
         sprintf (szTemp, "Path %d:", i);
         OutputDebugString (szTemp);

         dw = pListPath->Size(i) / sizeof(DWORD);
         padw = (DWORD*) pListPath->Get(i);

         for (j = 0; j < dw; j++) {
            sprintf (szTemp, " %d", padw[j]);
            OutputDebugString (szTemp);
         }

         OutputDebugString ("\r\n");
      }
   }
#endif
   return;
}


/********************************************************************************
PathToSplineListFillInGaps - Like PathToSplineList, except that if this one can't
complete the path, then it fills in the gaps by analyzing all available sequences.

inputs
   PCListFixed       pListSequences - List of sequences from LineSegmentsToLineSequences()
   DWORD             *padwPath - Pointer to an array of DWORDs, that are seuqnece numbers.
                           This order doens't matter
   DWORD             dwNum - Number in padwPath
   BOOL              fClockwise - If TRUE, want path to be clockwise in the end, else counter
returns
   PCListFixed - Intialized to sizeof(TEXTUREPOINT). Contains a clockwise list of
      texturepoints (from the sequences). Or, NULL if a path couldn't be made out of
      the points in padwPath.
*/
PCListFixed PathToSplineListFillInGaps (PCListFixed pListSequences, DWORD *padwPath, DWORD dwNum,
                              BOOL fClockwise)
{
   PCListFixed pNew;
   pNew = PathToSplineList (pListSequences, padwPath, dwNum, fClockwise);
   if (pNew)
      return pNew;

   // else, find out which of these are dangling
   CListFixed lHangingStart, lHangingEnd;
   lHangingStart.Init (sizeof(BOOL));
   lHangingEnd.Init (sizeof(BOOL));
   DWORD i, j;

   PCListFixed plA, plB;
   PTEXTUREPOINT pAStart, pAEnd, pBStart, pBEnd;
   lHangingStart.Required (dwNum);
   lHangingEnd.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      plA = *((PCListFixed*) pListSequences->Get(padwPath[i]));
      pAStart = (PTEXTUREPOINT) plA->Get(0);
      pAEnd = (PTEXTUREPOINT) plA->Get(plA->Num()-1);

      BOOL fHangStart, fHangEnd;
      fHangStart = fHangEnd = TRUE;
      for (j = 0; j < dwNum; j++) {
         if (i == j)
            continue;   // no self-connecting
         plB = *((PCListFixed*) pListSequences->Get(padwPath[j]));
         pBStart = (PTEXTUREPOINT) plB->Get(0);
         pBEnd = (PTEXTUREPOINT) plB->Get(plB->Num()-1);

         if (AreClose (pAStart, pBStart) || AreClose(pAStart, pBEnd))
            fHangStart = FALSE;
         if (AreClose (pAEnd, pBStart) || AreClose(pAEnd, pBEnd))
            fHangEnd = FALSE; // BUGFIX: Was fHangStart=TRUE
      }

#ifdef _DEBUG
      char szTemp[128];
      if (fHangStart) {
         sprintf (szTemp, "Hang start:%d\r\n", i);
         OutputDebugString (szTemp);
      }
      if (fHangEnd) {
         sprintf (szTemp, "Hang end:%d\r\n", i);
         OutputDebugString (szTemp);
      }
#endif

      lHangingStart.Add (&fHangStart);
      lHangingEnd.Add (&fHangEnd);
   }

   // create a new list of items
   CListFixed lPath;
   CListVariable lEnum;
   PCListFixed pHangA, pHangB;
   lPath.Init (sizeof(DWORD), padwPath, dwNum);

   // loop through all the hanging paths, finding out where they connect
   for (i = 0; i < lPath.Num(); i++) {
      padwPath = (DWORD*) lPath.Get(0);
      plA = *((PCListFixed*) pListSequences->Get(padwPath[i]));
      pAStart = (PTEXTUREPOINT) plA->Get(0);
      pAEnd = (PTEXTUREPOINT) plA->Get(plA->Num()-1);

      DWORD dwSA, dwSB;
      for (dwSA = 0; dwSA < 2; dwSA++) {  // dwS == 0 then start, dwS == 1 then end
         pHangA = dwSA ? &lHangingEnd : &lHangingStart;
         BOOL *pfB;
         BOOL *pfA = (BOOL*) pHangA->Get(i);
         if (!*pfA)
            continue;   // not hanging

         lEnum.Clear ();

         DWORD dwShortest, dwShortestLen, dwShortestConnect, dwShortestEnd;
         fp fShortestLen;
         dwShortest = -1;
         fShortestLen = 0;

         // find another hanging one
         for (j = i+1; j < lPath.Num(); j++) {
            padwPath = (DWORD*) lPath.Get(0);
            plB = *((PCListFixed*) pListSequences->Get(padwPath[j]));
            pBStart = (PTEXTUREPOINT) plB->Get(0);
            pBEnd = (PTEXTUREPOINT) plB->Get(plB->Num()-1);
            for (dwSB = 0; dwSB < 2; dwSB++) {  // dwS == 0 then start, dwS == 1 then end
               pHangB = dwSB ? &lHangingEnd : &lHangingStart;
               pfB = (BOOL*) pHangB->Get(j); // BUGFIX - Was i
               if (!*pfB)
                  continue;   // not hanging

               // remember size of list before this
               DWORD dwSize;
               dwSize = lEnum.Num();

               // know that this hangs, so find any connections
               DWORD adwHistory[10];   // BUGFIX - Was adwHistory[100] - but reduced number reduces complexity and speeds up
               FindPathBetweenTwoPoints (dwSA ? pAEnd : pAStart, dwSB ? pBEnd : pBStart,
                  pListSequences, &lEnum, (DWORD*) lPath.Get(0) , lPath.Num(),
                  adwHistory, 0, sizeof(adwHistory)/sizeof(DWORD));

               // look through any added and find shorted
               DWORD k;
               for (k = dwSize; k < lEnum.Num(); k++) {
                  // BUGFIX - figure out the length, as a measure of distance
                  fp fLen;
                  DWORD *padwIndex;
                  DWORD dwPathLen;
                  padwIndex = (DWORD*) lEnum.Get(k);
                  dwPathLen = (DWORD)lEnum.Size(k) / sizeof(DWORD);
                  fLen  =0;
                  DWORD dwSeq;
                  for (dwSeq = 0; dwSeq < dwPathLen; dwSeq++) {
                     PCListFixed plSeq = *((PCListFixed*) pListSequences->Get(padwIndex[dwSeq]));
                     PTEXTUREPOINT pSeq = (PTEXTUREPOINT) plSeq->Get(0);
                     DWORD dwBend;
                     for (dwBend = 0; dwBend+1 < plSeq->Num(); dwBend++) {
                        TEXTUREPOINT tp;
                        tp.h = pSeq[dwBend].h - pSeq[dwBend+1].h;
                        tp.v = pSeq[dwBend].v - pSeq[dwBend+1].v;
                        fLen += sqrt(tp.h * tp.h + tp.v * tp.v);
                     }
                  }

                  if ((dwShortest == -1) || (fLen < fShortestLen)) {
                     dwShortest = k;
                     fShortestLen = fLen;
                     dwShortestLen = (DWORD)lEnum.Size(k);
                     dwShortestConnect = j;
                     dwShortestEnd = dwSB;
                  }
               }

            }  // dwSB
         }  // j

         // if there aren't any paths here then they just dont connect, so error
         if (dwShortest == -1)
            return NULL;

         // note that no longer hanging
         *pfA = FALSE;
         pHangB = dwShortestEnd ? &lHangingEnd : &lHangingStart;
         pfB = (BOOL*) pHangB->Get(dwShortestConnect);
         *pfB = FALSE;

         // add the shortest one
         DWORD *padwLA;
         DWORD dwLA;
         BOOL fFalse;
         fFalse = FALSE;
         dwLA = dwShortestLen / sizeof(DWORD);
         padwLA = (DWORD*) lEnum.Get(dwShortest);
         lPath.Required (lPath.Num() + dwLA);
         lHangingStart.Required (lHangingStart.Num() + dwLA);
         lHangingEnd.Required (lHangingEnd.Num() + dwLA);
         for (j = 0; j < dwLA; j++) {
            lPath.Add (&padwLA[j]);
            lHangingStart.Add (&fFalse);
            lHangingEnd.Add (&fFalse);
         }


      } // dwSA
   } // i

   // if get here then have connected everything
   return PathToSplineList (pListSequences, (DWORD*)lPath.Get(0), lPath.Num(), fClockwise);

}

/*************************************************************************************
SequencesLooksForFlips - This looks through the list of sequences for two sequences
whose sends meet. This means that the intersecting curves have undergone a flip
part way through (easy to do this with curved balanese roofs when the curve is set
so that there's a valley). If this condition is met then the second one is flipped
around and added to the first.

NOTE: If any squences were split up before this call (because they crossed one another),
they may be recombined again.

NOTE: This reorders the sequences in any which way, clockwise or counterclockwise.

NOTE: Because of more tests... I attach bits according to which are the straightest
line segments first. This encourages straight cuts...

inputs
   PCListFixed       plSeq - List of sequences. This will be modified in place.
returns
   none
*/
typedef struct {
   DWORD    dw1, dw2;   // two sequences that match up
   BOOL     fEnd;       // TRUE if match up at end, FALSE if at beginning
   fp   fError;     // if a straight line is drawn between the two nearest points (other
                        // than one intersects), what's the error - distance from point to line
} SLF, *PSLF;

void SequencesLookForFlips (PCListFixed plSeq)
{
#if 0//def _DEBUG
   {
   // display all the sequences
   char szTemp[64];
   DWORD i, j;
   PCListFixed pt;
   OutputDebugString ("Before connect\r\n");
   for (i = 0; i < plSeq->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)plSeq->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
   }
#endif

   CListFixed lIntersect;
   lIntersect.Init (sizeof(SLF));

start:
   DWORD i, j, k;
   PTEXTUREPOINT pTemp, pTemp2;
   CPoint pStart,pDelta, pPoint, pNearest;
   lIntersect.Clear();

   for (i = 0; i < plSeq->Num(); i++) {
      PCListFixed pl1 = *((PCListFixed*) plSeq->Get(i));
      PTEXTUREPOINT pEnd1 = (PTEXTUREPOINT)pl1->Get(pl1->Num()-1);
      PTEXTUREPOINT pStart1 = (PTEXTUREPOINT)pl1->Get(0);

      for (j = 0; j < plSeq->Num(); j++) {
         if (i == j)
            continue;

         PCListFixed pl2 = *((PCListFixed*) plSeq->Get(j));
         PTEXTUREPOINT pEnd2 = (PTEXTUREPOINT)pl2->Get(pl2->Num()-1);
         PTEXTUREPOINT pStart2 = (PTEXTUREPOINT)pl2->Get(0);

         SLF slf;
         slf.dw1 = i;
         slf.dw2 = j;
         if (AreClose (pEnd2, pEnd1)) {
            slf.fEnd = TRUE;

            // find the distance
            pTemp = (PTEXTUREPOINT)pl1->Get(pl1->Num()-2);
            pTemp2 = (PTEXTUREPOINT)pl2->Get(pl2->Num()-2);
            pStart.Zero();
            pStart.p[0] = pTemp->h;
            pStart.p[1] = pTemp->v;
            pDelta.Zero();
            pDelta.p[0] = pTemp2->h;
            pDelta.p[1] = pTemp2->v;
            pDelta.Subtract (&pStart);
            pPoint.Zero();
            pPoint.p[0] = pEnd1->h;
            pPoint.p[1] = pEnd1->v;
            slf.fError = DistancePointToLine (&pPoint, &pStart, &pDelta, &pNearest);

            lIntersect.Add (&slf);
         }

         if (AreClose (pStart2, pStart1)) {
            slf.fEnd = FALSE;

            // find the distance
            pTemp = (PTEXTUREPOINT)pl1->Get(1);
            pTemp2 = (PTEXTUREPOINT)pl2->Get(1);
            pStart.Zero();
            pStart.p[0] = pTemp->h;
            pStart.p[1] = pTemp->v;
            pDelta.Zero();
            pDelta.p[0] = pTemp2->h;
            pDelta.p[1] = pTemp2->v;
            pDelta.Subtract (&pStart);
            pPoint.Zero();
            pPoint.p[0] = pEnd1->h;
            pPoint.p[1] = pEnd1->v;
            slf.fError = DistancePointToLine (&pPoint, &pStart, &pDelta, &pNearest);

            lIntersect.Add (&slf);
         }
      }
   }

   // find the best match
   PSLF ps, psBest;
   psBest = NULL;
   for (i = 0; i < lIntersect.Num(); i++) {
      ps = (PSLF) lIntersect.Get(i);
      if (!i || (ps->fError < psBest->fError))
         psBest = ps;
   }
   if (psBest) {
      PCListFixed pl1 = *((PCListFixed*) plSeq->Get(psBest->dw1));
      PCListFixed pl2 = *((PCListFixed*) plSeq->Get(psBest->dw2));

      if (psBest->fEnd) {
         // add the ones from list two backwards
         for (k = pl2->Num()-2; k < pl2->Num(); k--) {
            pTemp = (PTEXTUREPOINT) pl2->Get(k);
            pl1->Add (pTemp);
         }
         delete pl2;
         plSeq->Remove (psBest->dw2);
         goto start; // go back and recheck everything
      }
      else {
         // insert everything pl2 in front of pl1
         for (k = 1; k < pl2->Num(); k++) {
            pTemp = (PTEXTUREPOINT) pl2->Get(k);
            pl1->Insert (0, pTemp);
         }
         delete pl2;
         plSeq->Remove (psBest->dw2);
         goto start; // go back and recheck everything
      }
   }

#if 0//def _DEBUG
   {
   // display all the sequences
   char szTemp[64];
   DWORD i, j;
   PCListFixed pt;
   OutputDebugString ("After connect\r\n");
   for (i = 0; i < plSeq->Num(); i++) {
      sprintf (szTemp, "Sequence %d:", i);
      OutputDebugString (szTemp);

      pt = *((PCListFixed*)plSeq->Get(i));

      for (j = 0; j < pt->Num(); j++) {
         PTEXTUREPOINT p = (PTEXTUREPOINT) pt->Get(j);

         sprintf (szTemp, " (%g,%g)", (double)p->h, (double)p->v);
         OutputDebugString (szTemp);
      }

      OutputDebugString ("\r\n");
   }
   }
#endif
   // if gets here then nothing left to combine
   return;
}



/**********************************************************************************
Is2DPointToLeft - Returns TRUE if the 2D point is to the left of a an unbounded
line, FALSE if it's to the right.

NOTE: Grid system assumes 0,0 is UL corner, 1,1 is LR corner

inputs
   PTEXTUREPOINT      pTest - Point to test against
   PTEXTUREPOINT      p1, p2 - Two points on unbounded line. "Left" is defined
                      as being at p1 and looking towards p2
returns
   BOOL - TRUE if to left. If it's right on returns FALSE.
*/
BOOL Is2DPointToLeft (PTEXTUREPOINT pTest, PTEXTUREPOINT p1, PTEXTUREPOINT p2)
{
   TEXTUREPOINT d;
   d.h = p2->h - p1->h;
   d.v = p2->v - p1->v;

   if ((fabs(d.h) < EPSILON) && (fabs(d.v) < EPSILON))
      return FALSE;  // too close to call

   fp fAlpha, fV;
   if (fabs(d.h) > fabs(d.v)) {
      // mostly horizontal
      fAlpha = (pTest->h - p1->h) / d.h;
      fV = fAlpha * d.v + p1->v; // point on the line
      if (d.h > 0)
         return (pTest->v < (fV - EPSILON));
      else
         return ((pTest->v - EPSILON) > fV);
   }
   else {
      // mostly vertical
      fAlpha = (pTest->v - p1->v) / d.v;
      fV = fAlpha * d.h + p1->h; // point on the line
      if (d.v < 0)
         return (pTest->h < (fV - EPSILON));
      else
         return ((pTest->h - EPSILON) > fV);
   }
}


/**********************************************************************************
Intersect2DLineWith2DPlane - Intersects a two-dimensional line with a 2D plane (unbounded
line).

inputs
   PTEXTUREPOINT     pl1, pl2 - Points defining line
   PTEXTUREPOINT     pp1, pp2 - Points defining plane
   fp            *pfAlpha - Alpha intersection, 0=>pl1, 1=>pl2, where intersects
returns
   BOOL - TRUE if intersects with pfAlpha between -EPSILON and 1+EPSILON. FALSE if doesn't
*/
BOOL Intersect2DLineWith2DPlane (PTEXTUREPOINT pl1, PTEXTUREPOINT pl2,
                                 PTEXTUREPOINT pp1, PTEXTUREPOINT pp2, fp *pfAlpha)
{
   // use the InersectLinePlane function

   CPoint pLineStart, pLineEnd;
   pLineStart.Zero();
   pLineStart.p[0] = pl1->h;
   pLineStart.p[1] = pl1->v;
   pLineEnd.Zero();
   pLineEnd.p[0] = pl2->h;
   pLineEnd.p[1] = pl2->v;

   CPoint pPlane, pPlaneNormal, pUp, pDelta;
   pPlane.Zero();
   pPlane.p[0] = pp1->h;
   pPlane.p[1] = pp1->v;
   pUp.Zero();
   pUp.p[2] = 1;
   pDelta.Zero();
   pDelta.p[0] = pp2->h - pp1->h;
   pDelta.p[1] = pp2->v - pp1->v;
   pPlaneNormal.CrossProd (&pUp, &pDelta);

   CPoint pIntersect;
   if (!IntersectLinePlane(&pLineStart, &pLineEnd, &pPlane, &pPlaneNormal, &pIntersect))
      return FALSE;

   // find the alpha
   TEXTUREPOINT d;
   d.h = pl2->h - pl1->h;
   d.v = pl2->v - pl1->v;
   fp fAlpha;
   if (fabs(d.h) > fabs(d.v)) {
      // horizontal
      fAlpha = (pIntersect.p[0] - pl1->h) / d.h;
   }
   else {
      // vertical
      fAlpha = (pIntersect.p[1] - pl1->v) / d.v;
   }

   *pfAlpha = fAlpha;
   return (fAlpha > -EPSILON) && (fAlpha < 1+EPSILON);
}

/**********************************************************************************
Intersect2DLineWith2DPoly - Intersects a 2D line with a 2D polygon. Polygon goes
clockwise around. Returns TRUE if they intersect. This then fills in some information
about the new line.

inputs
   PTEXTUREPOINT     pl1, pl2 - Line
   PTEXTUREPOINT     pp1, pp2, pp3, pp4 - Polygon points. pp4 can be null for a triangle
   PTEXTUREPOINT     pnl1, pnl2 - Filled with new start and end
   PTEXTUREPOINT     ps1, ps2 - Filled with the slope (deltas) at the start and end of the intersection
                     OR, with 0,0 if the line stared within the polygon.
returns
   BOOL - TRUE if it intersects, FALSE if it doesn't
*/
BOOL Intersect2DLineWith2DPoly (PTEXTUREPOINT pl1, PTEXTUREPOINT pl2,
                                PTEXTUREPOINT pp1, PTEXTUREPOINT pp2, PTEXTUREPOINT pp3, PTEXTUREPOINT pp4,
                                PTEXTUREPOINT pnl1, PTEXTUREPOINT pnl2,
                                PTEXTUREPOINT ps1, PTEXTUREPOINT ps2)
{
   // store the pointers away for easy reference
   PTEXTUREPOINT ap[4];
   ap[0] = pp1;
   ap[1] = pp2;
   ap[2] = pp3;
   ap[3] = pp4;
   PTEXTUREPOINT al[2];
   DWORD adwBits[2];
   al[0] = pl1;
   al[1] = pl2;

   // how many points
   DWORD dwNum;
   dwNum = ap[3] ? 4 : 3;

   // max bits
   DWORD dwMax;
   dwMax = (1 << (dwNum+1)) - 1;

   // for each point in the line, see which side of the 3 (or 4) planes formed by
   // the polygon it's on
   DWORD i, j;
   for (i = 0; i < 2; i++) {
      adwBits[i] = 0;

      for (j = 0; j < dwNum; j++)
         if (Is2DPointToLeft (al[i], ap[j], ap[(j+1)%dwNum]))
            adwBits[i] |= (1 << j);
   }

   // trivial reject
   if (adwBits[0] & adwBits[1])
      return FALSE;  // both points are one one of the rejection sides
   ps1->h = ps1->v = ps2->h = ps2->v = 0;
   if (!(adwBits[0] | adwBits[1])) {
      // entirely inside. trivial accept
      *pnl1 = *pl1;
      *pnl2 = *pl2;
      return TRUE;
   }

   // find out where clipped
   fp fAlpha, afAlpha[2];
   afAlpha[0] = 0;
   afAlpha[1] = 1;
   for (i = 0; i < dwNum; i++) {
      DWORD dwTest = (1 << i);
      // know that both bits aren't true
      if ((adwBits[0] & dwTest) == (adwBits[1] & dwTest))
         continue;   // nothing to clip against

      // clip the line against the plane
      if (!Intersect2DLineWith2DPlane(al[0], al[1], ap[i], ap[(i+1)%dwNum], &fAlpha))
         continue;
      if (adwBits[0] & dwTest) { // point 1 is outside
         if (fAlpha > afAlpha[0]) {
            afAlpha[0] = fAlpha;
            if (ps1) {
               ps1->h = ap[(i+1)%dwNum]->h - ap[i]->h;
               ps1->v = ap[(i+1)%dwNum]->v - ap[i]->v;
            }
         }
      }
      else { // adwBits[1] & dwTest => point 2 is outside
         if (fAlpha < afAlpha[1]) {
            afAlpha[1] = fAlpha;
            if (ps2) {
               ps2->h = ap[(i+1)%dwNum]->h - ap[i]->h;
               ps2->v = ap[(i+1)%dwNum]->v - ap[i]->v;
            }
         }
      }
   }

   // if entirely clipped no intersect
   if (afAlpha[1] <= afAlpha[0])
      return FALSE;

   // got an intersection
   TEXTUREPOINT d;
   d.h = pl2->h - pl1->h;
   d.v = pl2->v - pl1->v;
   pnl1->h = d.h * afAlpha[0] + pl1->h;
   pnl1->v = d.v * afAlpha[0] + pl1->v;
   pnl2->h = d.h * afAlpha[1] + pl1->h;
   pnl2->v = d.v * afAlpha[1] + pl1->v;

   return TRUE;
}


/********************************************************************************
PointOn2DLine - Given a 2D point that is known to be on a 2D line, returns the alpha
for the location on the line.

inputs
   PTEXTUREPOINT     p - Point
   PTEXTUREPOINT     p1, p2 - Line sttart and end
returns
   fp - Alpha... 0 => p1, 1.0 => p2
*/
fp PointOn2DLine (PTEXTUREPOINT p, PTEXTUREPOINT p1, PTEXTUREPOINT p2)
{
   TEXTUREPOINT d;
   d.h = p2->h - p1->h;
   d.v = p2->v - p1->v;
   if (fabs(d.h) > fabs(d.v)) {
      // horizontal
      return (p->h - p1->h) / d.h;
   }
   else {
      // vertical
      return (p->v - p1->v) / d.v;
   }
}

/********************************************************************************
IntersectLineSphere - Intersect a line with a spehere.

inputs
   PCPoint           pStart - line start
   PCPonint          pDir - Line direction
   PCPoint           pCenter - Center of sphere
   double            fRadius - Radius
   PCPoint           pt0 - First point
   PCPoint           pt1 - Second intersection point
returns
   DWORD - 0 if no intersections, 1 if one intersection (only pt0 valid), 2 if two intersection
*/
DWORD IntersectLineSphere (PCPoint pStart, PCPoint pDir, PCPoint pCenter, fp fRadius,
                           PCPoint pt0, PCPoint pt1)
{
   // easiest thing to do is make new start that's offset by center of the sphere
   CPoint pS, pD;
   pS.Subtract (pStart, pCenter);
   pD.Copy (pDir);
   pD.Normalize();

   // calculate aq, bq, cq
   double aq, bq, cq;
   //aq = pD.p[0] * pD.p[0] + pD.p[1] * pD.p[1] + pD.p[2] * pD.p[2];
   aq = 1;  // BUGFIX - Since pD is normalized
   bq = 2 * (pS.p[0] * pD.p[0] + pS.p[1] * pD.p[1] + pS.p[2] * pD.p[2]);
   cq = pS.p[0] * pS.p[0] + pS.p[1] * pS.p[1] + pS.p[2] * pS.p[2] - fRadius * fRadius;

   if (fabs(aq) > EPSILON) {
      double r;
      r = bq * bq - 4.0 * aq * cq;
      if (r < 0)
         return 0;   // no intersection
      r = sqrt(r);

      double t1, t2;
      bq = -bq / (2.0 * aq);
      r /= (2.0 * aq);
      t1 = bq + r;
      t2 = bq - r;

      pt0->Copy (&pD);
      pt0->Scale (t1);
      pt0->Add (pStart);   // returns us to original offset

      pt1->Copy (&pD);
      pt1->Scale (t2);
      pt1->Add (pStart);   // returns us to original offset

      return 2;
   }
   else {
      // aw == 0, so one solution
      double t;
      if (fabs(bq) < EPSILON)
         return 0;
      t = -cq / bq;
      pt0->Copy (&pD);
      pt0->Scale (t);
      pt0->Add (pStart);   // returns us to original offset
      return 1;
   }
}



/********************************************************************************
IntersectNormLineSphere - Intersect a line with a spehere.

inputs
   PCPoint           pStart - line start
   PCPonint          pDir - Line direction. This is NORMALIZED.
   PCPoint           pCenter - p[0]..p[2] = Center of sphere. p[3] = radius of sphere
   fp                *pafAlpha - Pointer to an array of 2 floats.
                        Filled in with alpha where intersect.
                        pafAlpha[x] == 0 then intersect at pStart. pafAlpha[x] == 1.0 then intersect
                        at pStart + pDir
returns
   DWORD - 0 if no intersections, 1 if one intersection (only pfAlpha1 valid), 2 if two intersection
*/
DWORD IntersectNormLineSphere (PCPoint pStart, PCPoint pDir, PCPoint pCenter,
                           fp *pafAlpha)
{
   // easiest thing to do is make new start that's offset by center of the sphere
   CPoint pS;//, pD;
   pS.Subtract (pStart, pCenter);
   //pD.Copy (pDir);
   //fp fLen;
   //fLen = pD.Length();
   //if (fabs(fLen) < EPSILON)
   //   return 0;   // error
   //fLen = 1.0 / fLen;
   //pD.Scale(fLen);

   // calculate aq, bq, cq
   double aq, bq, cq;
   //aq = pD.p[0] * pD.p[0] + pD.p[1] * pD.p[1] + pD.p[2] * pD.p[2];
   aq = 1;  // since pD is normalized
   bq = 2 * (pS.p[0] * pDir->p[0] + pS.p[1] * pDir->p[1] + pS.p[2] * pDir->p[2]);
   cq = pS.p[0] * pS.p[0] + pS.p[1] * pS.p[1] + pS.p[2] * pS.p[2] - pCenter->p[3] * pCenter->p[3];

   if (fabs(aq) > EPSILON) {
      double r;
      r = bq * bq - 4.0 * aq * cq;
      if (r < 0)
         return 0;   // no intersection
      r = sqrt(r);

      double t1, t2;
      bq = -bq / (2.0 * aq);
      r /= (2.0 * aq);
      t1 = bq + r;
      t2 = bq - r;

      pafAlpha[0] = t1;// * fLen;
      pafAlpha[1] = t2;// * fLen;

      return 2;
   }
   else {
      // aw == 0, so one solution
      double t;
      if (fabs(bq) < EPSILON)
         return 0;
      t = -cq / bq;

      pafAlpha[0] = t;// * fLen;
      return 1;
   }
}


/********************************************************************************************
MakePointCoplanar - Given a point and a plant (with a normal and point), moves the first
point down onto the plane (at the closest point of the plane to the point)

inputs
   PCPoint     pPoint - start of the line
   PCPoint     pPlane - Point in plane
   PCPoint     pPlaneNormal - Normal of the plane
   PCPoint     pIntersect - Filled with the new point
returns
   BOOL - TRUE if intersect, FALSE if dont
*/
BOOL MakePointCoplanar (PCPoint pPoint, PCPoint pPlane,
                         PCPoint pPlaneNormal, PCPoint pIntersect)
{
   // find out where it crosses the plane on the dwLongest dimension
   // solve alpha = ((P-Q).v) / (w.v), P=point on plalne, Q=pPoint, w=pPlaneNormal, v=Plane normal
   CPoint pPQ;
   pPQ.Subtract (pPlane, pPoint);
   fp fAlpha;
   fAlpha = pPlaneNormal->DotProd (pPlaneNormal);   // do this since length of pPlaneNormal might not be 1
   if (fabs(fAlpha) < EPSILON)
      return FALSE;  // doens't intersect
   fAlpha = pPQ.DotProd(pPlaneNormal) / fAlpha;

   // find out where intersects
   pIntersect->Copy (pPlaneNormal);
   pIntersect->Scale(fAlpha);
   pIntersect->Add (pPoint);
   return TRUE;

}


#if 0 // MISTAKEN CODE
{
   // store the pointers away for easy reference
   PTEXTUREPOINT ap[4];
   ap[0] = pp1;
   ap[1] = pp2;
   ap[2] = pp3;
   ap[3] = pp4;
   PTEXTUREPOINT al[2];
   DWORD adwBits[2];
   al[0] = pl1;
   al[1] = pl2;

   // how many points
   DWORD dwNum;
   dwNum = ap[3] ? 4 : 3;

   // max bits
   DWORD dwMax;
   dwMax = (1 << (dwNum+1)) - 1;

   // for each point in the line, see which side of the 3 (or 4) planes formed by
   // the polygon it's on
   DWORD i, j;
   for (i = 0; i < 2; i++) {
      adwBits[i] = 0;

      for (j = 0; j < dwNum; j++)
         if (Is2DPointToLeft (al[i], ap[j], ap[(j+1)%dwNum]))
            adwBits[i] |= (1 << j);
   }

   // trivial reject
   if (adwBits[0] & adwBits[1])
      return FALSE;  // both points are one one of the rejection sides
   if (!(adwBits[0] | adwBits[1])) {
      // entirely inside. trivial accept
      *pnl1 = *pl1;
      *pnl2 = *pl2;
      return TRUE;
   }

   // find out where clipped
   fp fAlpha, afAlpha[2];
   afAlpha[0] = 0;
   afAlpha[1] = 1;
   for (i = 0; i < dwNum; i++) {
      DWORD dwTest = (1 << i);
      // know that both bits aren't true
      if ((adwBits[0] & dwTest) == (adwBits[1] & dwTest))
         continue;   // nothing to clip against

      // find out where it intersects
      switch (i) {
      case 0:  // left line
         fAlpha = (0 - al[0]->h) / (al[1]->h - al[0]->h);
         break;
      case 1:  // right line
         fAlpha = (1 - al[0]->h) / (al[1]->h - al[0]->h);
         break;
      case 2:  // top line
         fAlpha = (0 - al[0]->v) / (al[1]->v - al[0]->v);
         break;
      case 3:  // bottom line
         fAlpha = (1 - al[0]->v) / (al[1]->v - al[0]->v);
         break;
      }

      if (adwBits[0] & dwTest) { // point 1 is outside
         if (fAlpha > afAlpha[0]) {
            afAlpha[0] = fAlpha;
         }
      }
      else { // adwBits[1] & dwTest => point 2 is outside
         if (fAlpha < afAlpha[1]) {
            afAlpha[1] = fAlpha;
         }
      }
   }

   // if entirely clipped no intersect
   if (afAlpha[1] <= afAlpha[0])
      return FALSE;

   // got an intersection
   TEXTUREPOINT d;
   d.h = pl2->h - pl1->h;
   d.v = pl2->v - pl1->v;
   pnl1->h = d.h * afAlpha[0] + pl1->h;
   pnl1->v = d.v * afAlpha[0] + pl1->v;
   pnl2->h = d.h * afAlpha[1] + pl1->h;
   pnl2->v = d.v * afAlpha[1] + pl1->v;

   return TRUE;
}
#endif // 0 - mistaken code

/**********************************************************************************
Intersect2DLineWithBox - Intersects a 2D line (pl1 to pl2) with a box from 0,0 to 1,1.
Fills in pnl1 and pnl2 with th enew line. Or, returns FALSE if out of bounds.

inputs
   PTEXTUREPOINT     pl1, pl2 - Line
   PTEXTUREPOINT     pnl1, pnl2 - Filled with new start and end
returns
   BOOL - TRUE if line in pnl1 pnl2 is valid, FALSE if totally clipped
*/
BOOL Intersect2DLineWithBox (PTEXTUREPOINT pl1, PTEXTUREPOINT pl2,
                             PTEXTUREPOINT pnl1, PTEXTUREPOINT pnl2)
{
   // store the pointers away for easy reference
   PTEXTUREPOINT al[2];
   DWORD adwBits[2]; // bit 0 = left line, 1 = right line, 0 = top line, 1 = bottomline
   al[0] = pl1;
   al[1] = pl2;

   // how many points
   DWORD dwNum;
   dwNum = 4;

   // max bits
   DWORD dwMax;
   dwMax = (1 << (dwNum+1)) - 1;

   // for each point in the line, see which side of the 3 (or 4) planes formed by
   // the polygon it's on
   DWORD i, j;
   for (i = 0; i < 2; i++) {
      adwBits[i] = 0;

      for (j = 0; j < dwNum; j++) {
         BOOL fLeft;
         fLeft = FALSE;

         switch (j) {
         case 0:  // left line
            fLeft = (al[i]->h < 0);
            break;
         case 1:  // right line
            fLeft = (al[i]->h > 1);
            break;
         case 2:  // top line
            fLeft = (al[i]->v < 0);
            break;
         case 3:  // bottom line
            fLeft = (al[i]->v > 1);
            break;
         }

         if (fLeft)
            adwBits[i] |= (1 << j);
      }
   }

   // trivial reject
   if (adwBits[0] & adwBits[1])
      return FALSE;  // both points are one one of the rejection sides
   if (!(adwBits[0] | adwBits[1])) {
      // entirely inside. trivial accept
      *pnl1 = *pl1;
      *pnl2 = *pl2;
      return TRUE;
   }

   // find out where clipped
   fp fAlpha, afAlpha[2];
   afAlpha[0] = 0;
   afAlpha[1] = 1;
   for (i = 0; i < dwNum; i++) {
      DWORD dwTest = (1 << i);
      // know that both bits aren't true
      if ((adwBits[0] & dwTest) == (adwBits[1] & dwTest))
         continue;   // nothing to clip against

      // find out where it intersects
      switch (i) {
      case 0:  // left line
         fAlpha = (0 - al[0]->h) / (al[1]->h - al[0]->h);
         break;
      case 1:  // right line
         fAlpha = (1 - al[0]->h) / (al[1]->h - al[0]->h);
         break;
      case 2:  // top line
         fAlpha = (0 - al[0]->v) / (al[1]->v - al[0]->v);
         break;
      case 3:  // bottom line
         fAlpha = (1 - al[0]->v) / (al[1]->v - al[0]->v);
         break;
      }

      if (adwBits[0] & dwTest) { // point 1 is outside
         if (fAlpha > afAlpha[0]) {
            afAlpha[0] = fAlpha;
         }
      }
      else { // adwBits[1] & dwTest => point 2 is outside
         if (fAlpha < afAlpha[1]) {
            afAlpha[1] = fAlpha;
         }
      }
   }

   // if entirely clipped no intersect
   if (afAlpha[1] <= afAlpha[0])
      return FALSE;

   // got an intersection
   TEXTUREPOINT d;
   d.h = pl2->h - pl1->h;
   d.v = pl2->v - pl1->v;
   pnl1->h = d.h * afAlpha[0] + pl1->h;
   pnl1->v = d.v * afAlpha[0] + pl1->v;
   pnl2->h = d.h * afAlpha[1] + pl1->h;
   pnl2->v = d.v * afAlpha[1] + pl1->v;

   return TRUE;
}


/*****************************************************************************
ThreePointsToCircle - Given three points, create a circle

inputs
   PCPoint     p1, p2, p3 - Three points in a row
   PCMatrix    pm - Filled with a matrix that takes a point. The input point's X value
               is sin(fAngle). Input point's Y value is cos(fAngle). The output is the
               location of the point in p1,p2,p3 space.
   fp          *pfAngle1, *pfAngle2 - Filled with the start and end angle. fAngle1>fAngle2
   fp          *pfRadius - Filled with the radius of the circle

returns
   BOOL - TRUE if success. If returns FALSE then it's a circle with infinite radius,
         so it's linear
*/
BOOL ThreePointsToCircle (PCPoint p1, PCPoint p2, PCPoint p3,
                          PCMatrix pm, fp *pfAngle1, fp *pfAngle2, fp *pfRadius)
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

   // calculate the angle at the 2 points averaging
   // add two-pi so that know its a positive number
   fp fAngle1, fAngle2;
   //if (dwSegCurve == SEGCURVE_CIRCLEPREV) {
   //   fAngle1 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
   //   fAngle2 = atan2((t3.p[0] - pIntersect.p[0]), (t3.p[1] - pIntersect.p[1]));
   //}
   //else {   // next
   //   fAngle1 = atan2((t1.p[0] - pIntersect.p[0]), (t1.p[1] - pIntersect.p[1]));
   //   fAngle2 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
   //}
   fAngle1 = atan2((t1.p[0] - pIntersect.p[0]), (t1.p[1] - pIntersect.p[1]));
   fAngle2 = atan2((t3.p[0] - pIntersect.p[0]), (t3.p[1] - pIntersect.p[1]));
   if (fAngle1 < fAngle2)
      fAngle1 += 2 * PI;

   *pfAngle1 = fAngle1;
   *pfAngle2 = fAngle2;
   *pfRadius = fRadius;

   // orignal code
   //fp h, v;
   //h = sin (fAngle) * fRadius + pIntersect.p[0];
   //v = cos (fAngle) * fRadius + pIntersect.p[1];
   //CPoint pNew;
   //pNew.Copy (&pH);
   //pNew.Scale (h);
   //pTemp.Copy (&pV);
   //pTemp.Scale (v);
   //pNew.Add (&pTemp);
   //pNew.Add (&pC);
   //if (j && (j < dwDivide))
   //   pSeg[j].Copy (&pNew);

#ifdef _DEBUG
   CPoint pt1, pt2;
   pt1.Zero();
   pt2.Zero();
   pt1.p[0] = sin(fAngle1);
   pt1.p[1] = cos(fAngle1);
   pt2.p[0] = sin(fAngle2);
   pt2.p[1] = cos(fAngle2);
#endif

   // do * fRadius
   CPoint pA, pB, pC2;
   pA.Zero();
   pB.Zero();
   pC2.Zero();
   pA.p[0] = fRadius;
   pB.p[1] = fRadius;
   pm->RotationFromVectors (&pA, &pB, &pC2);

   // translate by the intersect
   CMatrix m;
   m.Translation (pIntersect.p[0], pIntersect.p[1], 0);
   pm->MultiplyRight (&m);

   // another rotation to pH and pV
   m.RotationFromVectors (&pH, &pV, &pC2);
   pm->MultiplyRight (&m);

   // offset by pC
   m.Translation (pC.p[0], pC.p[1], pC.p[2]);
   pm->MultiplyRight (&m);

#ifdef _DEBUG
   pt1.MultiplyLeft (pm);
   pt2.MultiplyLeft (pm);
#endif

   // done
   return TRUE;
}
/********************************************************************************
HermiteCubic - Cubic interpolation based on 4 points, p1..p4

inputs
   fp      t - value from 0 to 1, at 0, is at point p2, and at 1 is at point p3.
   fp      fp1 - value at p1
   fp      fp2 - value at p2
   fp      fp3 - value at p3
   fp      fp4 - value at p4
returns
   fp - Value at point t
*/
fp HermiteCubic (fp t, fp fp1, fp fp2, fp fp3, fp fp4)
{
   // derivatives at the points
   fp fd2, fd3;
   fd2 = (fp3 - fp1) / 2.0;
   fd3 = (fp4 - fp2) / 2.0;

   // calculte cube and square
   fp t3,t2;
   t2 = t * t;
   t3 = t * t2;

   // done
   return (2 * t3 - 3 * t2 + 1) * fp2 +
      (-2 * t3 + 3 * t2) * fp3 +
      (t3 - 2 * t2 + t) * fd2 +
      (t3 - t2) * fd3;
}


/************************************************************************************
IntersectRaySpehere - Intersects a ray with a spehere.

inputs
   PCPoint     pStart - Start of the ray
   PCPoint     pDir - Direction of the ray. MUST be normalized
   PCPoint     pCenter - Center of the spehere
   fp          fRadius - Radius of the spehere
   fp          *pft1 - If intersects, intersection point at pStart + pDir * *pft1
                  This is ALWAYS the lowest value of T => closest
   fp          *pft2 - If intersects, intersection point at pStart + pDir * *pft2
returns
   DWORD - Number of intersections. 0 for none, 1 for only on, pft1 filled in, 2
   for both, pft1 and pft2 filled in
*/
DWORD IntersectRaySpehere (PCPoint pStart, PCPoint pDir, PCPoint pCenter,
                           fp fRadius, fp *pft1, fp *pft2)
{
   CPoint pS, pD;
   pS.Subtract (pStart, pCenter);
   pS.Scale (1.0 / fRadius);
   pD.Copy (pDir);

   fp B, C;
   B = 2 * pD.DotProd (&pS);
   C = pS.p[0] * pS.p[0] + pS.p[1] * pS.p[1] + pS.p[2] * pS.p[2] - 1;

   fp t;
   t = B * B - 4 * C;
   if (t < 0)
      return 0;

   // if only one point
   if (t < CLOSE) {
      *pft1 = -B / 2;
      return 1;
   }

   // else two points
   t = sqrt(t);
   if (pft1) {
      *pft1 = (-B - t) / 2 * fRadius;
   }
   if (pft2) {
      *pft2 = (-B + t) / 2 * fRadius;
   }

   return 2;
}


/*************************************************************************************
IntersectBoundingBox - Tests to see if a line intersects a bounding box.

inputs
   PCPoint     pStart - Starting point
   PCPoint     pDir - Direction of line. pStart + pDir = pEnd
   PCPoint     pBBMin - Low xyz values of bounding box
   PCPoint     pBBMax - High xyz values of bounding box. pBBMax.p[n] > pBBMin.p[n]
returns
   BOOL - TRUE if there's an intersection, FALSE if not
*/
BOOL IntersectBoundingBox (PCPoint pStart, PCPoint pDir, PCPoint pBBMin, PCPoint pBBMax)
{
   // scale the line so bounding box's new min is 0,0,0 and new max is 1,1,1
   CPoint pS, pD, pE, pScale;
   DWORD i;
   pS.Copy (pStart);
   pD.Copy (pDir);
   pS.Subtract (pBBMin);
   pScale.Subtract (pBBMax, pBBMin);
   for (i = 0; i < 3; i++) {
      pS.p[i] /= pScale.p[i];
      pD.p[i] /= pScale.p[i];
   }
   pE.Add (&pS, &pD);

   // calculate bits for intersection
   fp afInter[6][2];
   DWORD adwInter[2];
   PCPoint pCur;
   DWORD j;
   for (i = 0; i < 2; i++) {
      pCur = i ? &pE : &pS;
      adwInter[i] = 0;

      // set afInter to negative value if on outside
      afInter[0][i] = pCur->p[0];      // x=0
      afInter[1][i] = 1 - pCur->p[0];  // x == 1
      afInter[2][i] = pCur->p[1];      // y=0
      afInter[3][i] = 1 - pCur->p[1];  // y == 1
      afInter[4][i] = pCur->p[2];      // y=0
      afInter[5][i] = 1 - pCur->p[2];  // y == 1

      for (j = 0; j < 6; j++)
         if (afInter[j][i] <= 0)
            adwInter[i] = adwInter[i] | (1 << j);
   }

   // if any points end up being on the same side then trivial reject
   if (adwInter[0] & adwInter[1])
      return FALSE;

   // if all points within clip area then trivial accept
   DWORD dwOr;
   dwOr = adwInter[0] | adwInter[1];
   if (dwOr == 0)
      return TRUE;

   // what have are lines to intersect
   fp afAlpha[2];
   afAlpha[0] = 0;
   afAlpha[1] = 1;
   DWORD dwDim;
   fp fValue, fAlpha;
   for (i = 0; i < 6; i++) {
      // only looking for planes where crosses
      if ( !(dwOr & (1 << i)) )
         continue;
      dwDim = i / 2;
      fValue = i - (dwDim * 2);

      // if get here either adwInter[0]'s bit is true, or adwInter[1]'s bit is true
      // but not both
      fAlpha = (fValue - pS.p[dwDim]) / pD.p[dwDim];

      if (adwInter[0] & (1 << i)) {
         // know that the start was outside, so update the start
         afAlpha[0] = max(afAlpha[0], fAlpha);
      }
      else {
         // know that the end was outside
         afAlpha[1] = min(afAlpha[1], fAlpha);
      }
      if (afAlpha[1] <= afAlpha[0])
         return FALSE;  // doesn't work since ends before it begins
   } // i

   return TRUE;
}


/*************************************************************************************
IsCutoutEntireSurface - Returns TRUE if the cutout is for the entire surface - basically
from 0..1, 0..1

inputs
   DWORD          dwCutoutNum - Number of points
   PTEXTUREPOINT  ptp - Texture points
returns
   BOOL - TRUE if it is
*/
BOOL IsCutoutEntireSurface (DWORD dwCutoutNum, PTEXTUREPOINT ptp)
{
   BOOL fIsWholeThing;
   fIsWholeThing = TRUE;
   TEXTUREPOINT pMin, pMax;   // used for whole thing
   DWORD j;

   for (j = 0; j < dwCutoutNum; j++) {
      if (j) {
         pMin.h = min (pMin.h, ptp[j].h);
         pMax.h = max (pMax.h, ptp[j].h);
         pMin.v = min (pMin.v, ptp[j].v);
         pMax.v = max (pMax.v, ptp[j].v);
      }
      else {
         pMin = ptp[j];
         pMax = ptp[j];
      }
      // want to know if clipping the whole thing - because shouldnt
      // if think about clipping whole thing then  retest to see if
      // surface is entirely inside
      // Know if it's the whole thing if a) All lines on edge, and
      // b) all lines NS or EW
      if (!((ptp[j].h < EPSILON) || (ptp[j].h > 1-EPSILON) || (ptp[j].v < EPSILON) || (ptp[j].v > 1-EPSILON)))
         fIsWholeThing = FALSE;
      if (fIsWholeThing) {
         PTEXTUREPOINT pNext;
         pNext = ptp + ((j+1) % dwCutoutNum);
         fp fDeltaH, fDeltaV;
         fDeltaH = fabs(pNext->h - ptp[j].h);
         fDeltaV = fabs(pNext->v - ptp[j].v);
         if ((fDeltaH > EPSILON) && (fDeltaV > EPSILON))
            fIsWholeThing = FALSE;
      }

   }

   // only whole thing if min and max are on edge
   if ((pMin.h > EPSILON) || (pMax.h < 1-EPSILON) ||
      (pMin.v > EPSILON) || (pMax.v < 1-EPSILON))
      fIsWholeThing = FALSE;

   return fIsWholeThing;
}


/*****************************************************************************************
IntersectRayCylinder - Given a ray from RA to RB, this intersects it against an infinite
cylinder running from CA to CB.

inputs
   PCPoint        pRA, pRB - Ray start and end.
   PCPoint        pCA, pCB - Define cylinder, even though infinite length
   fp             fRadius - Radius of cylinder
   fp             *pfAlpha1 - If intersected, filled with an intersection alpha.
                  Where Intersect = pRA + fAlpha * (pRB - pRA)
   fp             *pfAlpha2 - If intersected, filled with an intersection alpha
returns
   DWORD - Number of intersections, 0 - 2
*/
DWORD IntersectRayCylinder (PCPoint pRA, PCPoint pRB, PCPoint pCA, PCPoint pCB,
                            fp fRadius, fp *pfAlpha1, fp *pfAlpha2)
{
   // find the vector
   CPoint pRD, pCD;
   fp fLen;
   pCD.Subtract (pCB, pCA);
   fLen = pCD.Length();
   if (fLen < CLOSE)
      return 0;
   pCD.Scale (1.0 / fLen);

   // create a matrix that converts from current space into one where the
   // cyclinder stands vertical (in z)
   CPoint pX, pY, pZ;
   CMatrix mToCSpace, mFromCSpace;
   pZ.Copy (&pCD);
   pX.Zero();
   pX.p[0] = 1;
   pY.CrossProd (&pZ, &pX);
   if (pY.Length() < .1) {
      pX.Zero();
      pX.p[1] = 1;
      pY.CrossProd (&pZ, &pX);
   }
   pY.Normalize();
   pX.CrossProd (&pY, &pZ);
   pX.Normalize();
   mFromCSpace.RotationFromVectors (&pX, &pY, &pZ);
   mToCSpace.Translation (pCA->p[0], pCA->p[1], pCA->p[2]);
   mFromCSpace.MultiplyRight (&mToCSpace);
   mFromCSpace.Invert4 (&mToCSpace);

   // convert the point's to C space
   CPoint p1, p2;
   p1.Copy (pRA);
   p2.Copy (pRB);
   p1.p[3] = p2.p[3] = 1;
   p1.MultiplyLeft (&mToCSpace);
   p2.MultiplyLeft (&mToCSpace);
   // at this point p1.p[2] and p2.p[2] are useless for calculating intersection
   p1.p[2] = p2.p[2] = 0;

   // figure out distance
   pRD.Subtract (&p2, &p1);
   fLen = pRD.Length();
   if (fLen < CLOSE)
      return 0;
   pRD.Scale (1.0 / fLen); // since must normalize for intesect ray sphere

   // being lazy, intersect with sphere...
   pX.Zero();
   DWORD dwRet;
   dwRet = IntersectRaySpehere (&p1, &pRD, &pX, fRadius, pfAlpha1, pfAlpha2);
   if (dwRet == 2) {
      (*pfAlpha1) /= fLen;
      (*pfAlpha2) /= fLen;
   }
   else if (dwRet == 1) {
      (*pfAlpha1) /= fLen;
   }
   return dwRet;
}

