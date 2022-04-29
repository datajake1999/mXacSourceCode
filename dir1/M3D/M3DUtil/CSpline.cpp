/*******************************************************************************
CSpline.cpp - Code for calculating a spline.

begun 14-11-2001 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



/***********************************************************************************
CSpline::Constructor and destructor
*/
CSpline::CSpline (void)
{
   m_fLoop = FALSE;
   m_dwPoints = 0;
   m_pPoints = NULL;
   m_pTexture = NULL;
   m_pTangentStart = NULL;
   m_pTangentEnd = NULL;

   m_dwOrigPoints = 0;
   m_dwMinDivide = 0;
   m_dwMaxDivide = 0;
   m_fDetail = .1;
}


CSpline::~CSpline (void)
{
   // do nothing, for now
}


/***********************************************************************************
SegCurve - Function that either accesses the segment curve array based on an
index, or if padwSegCurve is SEGCURVE_XXX then use that.

inputs
   DWORD       dwIndex - Index to use
   DWORD       *padwSegCurve - Passed into Init()
returns
   DWORD - Curve index
*/
DWORD SegCurve (DWORD dwIndex, DWORD *padwSegCurve)
{
   if ((DWORD*) SEGCURVE_MAX >= padwSegCurve) {
      DWORD dwRet = (DWORD)(size_t) padwSegCurve;

      // if specify circle then alternate which side
      if ((dwRet == SEGCURVE_CIRCLEPREV) && (dwIndex % 2))
         dwRet = SEGCURVE_CIRCLENEXT;
      if ((dwRet == SEGCURVE_CIRCLENEXT) && (dwIndex % 2))
         dwRet = SEGCURVE_CIRCLEPREV;

      // if specify ellipse then alternate which side
      if ((dwRet == SEGCURVE_ELLIPSEPREV) && (dwIndex % 2))
         dwRet = SEGCURVE_ELLIPSENEXT;
      if ((dwRet == SEGCURVE_ELLIPSENEXT) && (dwIndex % 2))
         dwRet = SEGCURVE_ELLIPSEPREV;

      return dwRet;
   }

   return padwSegCurve[dwIndex];
}

/********************************************************************************
HermiteCubic - Cubic interpolation based on 4 points, p1..p4

inputs
   fp      t - value from 0 to 1, at 0, is at point p2, and at 1 is at point p3.
   PCPoint     p2,p3 - Two points
   PCPoint     pt2, pt3 - tangents at the points
   CPoint      pNew - Filled with the new value.
returns
   none
*/
static void HermiteCubic (fp t, PCPoint p2, PCPoint p3, PCPoint pt2, PCPoint pt3, PCPoint pNew)
{
   // calculte cube and square
   fp t3,t2;
   t2 = t * t;
   t3 = t * t2;

   fp m2, m3, md2, md3;
   m2 = (2 * t3 - 3 * t2 + 1);
   m3 = (-2 * t3 + 3 * t2);
   md2 = (t3 - 2 * t2 + t);
   md3 = (t3 - t2);

   DWORD i;
   for (i = 0; i < 3; i++)
      pNew->p[i] = m2 * p2->p[i] + m3 * p3->p[i] + md2 * pt2->p[i] + md3 * pt3->p[i];

   // done
   //return (2 * t3 - 3 * t2 + 1) * fp2 +
   //   (-2 * t3 + 3 * t2) * fp3 +
   //   (t3 - 2 * t2 + t) * fd2 +
   //   (t3 - t2) * fd3;
}

/********************************************************************************
BeszierCubic - Cubic interpolation based on 4 points, p1..p4

NOTE: Even pNew.p[3] (W) is calculated, so that can get curve.

inputs
   fp      t - value from 0 to 1, at 0, is at point p2, and at 1 is at point p3.
   PCPoint      p1, p2, p3, p4 - Four points. Use the first 3 values.
   CPoint      pNew - Filled with the new value.
returns
   none
*/
static void BezierCubic (fp t, PCPoint p1, PCPoint p2, PCPoint p3, PCPoint p4, PCPoint pNew)
{
   // calculte cube and square
   fp t3,t2, oneminust, oneminust2;
   t2 = t * t;
   t3 = t * t2;
   oneminust = 1 - t;
   oneminust2 = oneminust * oneminust;

   fp m1, m2, m3, m4;
   m1 = oneminust2 * oneminust;
   m2 = 3 * t * oneminust2;
   m3 = 3 * t2 * oneminust;
   m4 = t3;

   DWORD i;
   for (i = 0; i < 4; i++)
      pNew->p[i] = m1 * p1->p[i] + m2 * p2->p[i] + m3 * p3->p[i] + m4 * p4->p[i];

}

static PWSTR gpszLoop = L"Loop";
static PWSTR gpszOrigPoints = L"OrigPoints";
static PWSTR gpszMinDivide = L"MinDivide";
static PWSTR gpszMaxDivide = L"MaxDivide";
static PWSTR gpszDetail = L"Detail";
static PWSTR gpszOrigPointString = L"op%d";
static PWSTR gpszOrigTextureString = L"ot%d";
static PWSTR gpszOrigSegCurveString = L"sc%d";

/*******************************************************************************
CSpline::MMLTo - Returns a PCMMLNode2 with information about the spline
so that it can be copied to the clipboard or saved to disk.

inputs
   none
returns
   PCMMLNode2 - node
*/
PCMMLNode2 CSpline::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   WCHAR szTemp[64];
   DWORD i;
   if (!pNode)
      return NULL;
   pNode->NameSet (L"CSpline");

   MMLValueSet (pNode, gpszLoop, (int) m_fLoop);
   MMLValueSet (pNode, gpszOrigPoints, (int) m_dwOrigPoints);
   MMLValueSet (pNode, gpszMinDivide, (int) m_dwMinDivide);
   MMLValueSet (pNode, gpszMaxDivide, (int) m_dwMaxDivide);
   MMLValueSet (pNode, gpszDetail, m_fDetail);

   for (i = 0; i < m_dwOrigPoints; i++) {
      swprintf (szTemp, gpszOrigPointString, (int) i);
      MMLValueSet (pNode, szTemp, ((PCPoint) m_memOrigPoints.p) + i);
   }
   if (m_memOrigTextures.m_dwCurPosn) {
      for (i = 0; i < m_dwOrigPoints + (m_fLoop ? 1 : 0); i++) {
         swprintf (szTemp, gpszOrigTextureString, (int) i);
         MMLValueSet (pNode, szTemp, ((PTEXTUREPOINT)m_memOrigTextures.p) + i);
      }
   }
   if (m_dwOrigPoints)
      for (i = 0; i < m_dwOrigPoints - (m_fLoop ? 0 : 1); i++) {
         swprintf (szTemp, gpszOrigSegCurveString, (int) i);
         MMLValueSet (pNode, szTemp, (int) ((DWORD*)m_memOrigSegCurve.p)[i]);
      }

#if 0
   DWORD       m_dwOrigPoints;      // original points
   CMem        m_memOrigPoints;     // original number of points, array of CPoint, m_dwOrigPointsWorth
   CMem        m_memOrigTextures;   // original texture values, array of TEXTUREPOINT, m_dwOrigPoints worth, +1 if m_fLoop
   CMem        m_memOrigSegCurve;   // original segment curve, array of DWORD's. m_dwOrigPoints worth, -1 if !m_fLoop
   DWORD       m_dwMinDivide;       // minimum divide setting
   DWORD       m_dwMaxDivide;       // maximum divide setting
   fp      m_fDetail;           // detail setting
#endif

   return pNode;
}


/*******************************************************************************
CSpline::MMLFrom - Reads all the parameters for the spline and initializes
member variables.

inputs
   PCMLNode    pNode - Node to read from
returns
   BOOL - TRUE if successful
*/
BOOL CSpline::MMLFrom (PCMMLNode2 pNode)
{
   BOOL fLoop;
   DWORD dwOrigPoints, dwMinDivide, dwMaxDivide;
   fp fDetail;
   fLoop = (BOOL) MMLValueGetInt (pNode, gpszLoop, FALSE);
   dwOrigPoints = (DWORD) MMLValueGetInt (pNode, gpszOrigPoints, 0);
   dwMinDivide = (DWORD) MMLValueGetInt (pNode, gpszMinDivide, 0);
   dwMaxDivide = (DWORD) MMLValueGetInt (pNode, gpszMaxDivide, 0);
   fDetail = MMLValueGetDouble (pNode, gpszDetail, .1);

   
   CMem memPoints, memSegCurve, memTextures;
   DWORD dwSegCurve, dwTextures;
   if (dwOrigPoints)
      dwSegCurve = dwOrigPoints - (fLoop ? 0 : 1);
   else
      dwSegCurve = 0;
   dwTextures = dwOrigPoints + (fLoop ? 1 : 0);
   if (!memPoints.Required (dwOrigPoints * sizeof(CPoint)))
      return FALSE;
   if (!memSegCurve.Required (dwSegCurve * sizeof(DWORD)))
      return FALSE;
   if (!memTextures.Required (dwTextures * sizeof(TEXTUREPOINT)))
      return FALSE;
   memTextures.m_dwCurPosn = dwTextures * sizeof(TEXTUREPOINT);
   PCPoint pPoints;
   DWORD *padwSegCurve;
   PTEXTUREPOINT pText;
   pPoints = (PCPoint) memPoints.p;
   padwSegCurve = (DWORD*) memSegCurve.p;
   pText = (PTEXTUREPOINT) memTextures.p;

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < dwOrigPoints; i++) {
      swprintf (szTemp, gpszOrigPointString, (int) i);
      pPoints[i].Zero();
      MMLValueGetPoint (pNode, szTemp, &pPoints[i]);
   }
   for (i = 0; i < dwSegCurve; i++) {
      swprintf (szTemp, gpszOrigSegCurveString, (int) i);
      padwSegCurve[i] = (DWORD) MMLValueGetInt (pNode, szTemp, SEGCURVE_LINEAR);
   }
   for (i = 0; i < dwTextures; i++) {
      swprintf (szTemp, gpszOrigTextureString, (int) i);
      pText[i].h = pText[i].v = -123456.78;
      if (!MMLValueGetTEXTUREPOINT (pNode, szTemp, pText + i)) {
         memTextures.m_dwCurPosn = 0;
         break;
      }
   }

   // use this to init
   return Init (fLoop, dwOrigPoints, pPoints, memTextures.m_dwCurPosn ? pText : NULL,
      padwSegCurve, dwMinDivide, dwMaxDivide, fDetail);
}



/***********************************************************************************
CSpline::CloneTo - Copies all the information in this spline to a new
one, effectively cloning all the information.

inputs
   PCSpline      pCloneTo - Copy to this
retursn
   BOOL - TRUE if success, FALSE if fail
*/
BOOL CSpline::CloneTo (CSpline *pCloneTo)
{
   return pCloneTo->Init (m_fLoop, m_dwOrigPoints, (PCPoint) m_memOrigPoints.p,
      m_memOrigTextures.m_dwCurPosn ? (PTEXTUREPOINT) m_memOrigTextures.p : NULL,
      (DWORD*) m_memOrigSegCurve.p,
      m_dwMinDivide, m_dwMaxDivide, m_fDetail);
}

/***********************************************************************************
CSpline::Recalc - Internal function that recalculates the spline once all the relevent
information has been copied over.

returns
   BOOL - TRUE if success
*/
BOOL CSpline::Recalc (void)
{

   PCPoint paPoints = (PCPoint) m_memOrigPoints.p;
   PTEXTUREPOINT paTextures = m_memOrigTextures.m_dwCurPosn ? (PTEXTUREPOINT) m_memOrigTextures.p : NULL;
   DWORD dwMinDivide = m_dwMinDivide;
   DWORD dwMaxDivide = m_dwMaxDivide;
   DWORD *padwSegCurve = (DWORD*) m_memOrigSegCurve.p;

   // convert the segment curve information

   // figure out how much detail
   DWORD dwDivide, i, j;
   DWORD dwSegments;
   dwSegments = m_dwOrigPoints - (m_fLoop ? 0 : 1);
   if (dwMinDivide < dwMaxDivide) {
      // if all the segments are linear then use dwMinDivide
      for (i = 0; i < dwSegments; i++)
         if (SegCurve(i, padwSegCurve) != SEGCURVE_LINEAR)
            break;
      if (i >= dwSegments)
         dwMaxDivide = dwMinDivide;
   };

   if (dwMinDivide < dwMaxDivide) {
      // calculate distances
      fp fMaxLen = 0;
      fp fLen;
      CPoint p;
      for (i = 0; i < (m_fLoop ? m_dwOrigPoints : (m_dwOrigPoints-1)); i++) {
         p.Subtract (paPoints + i, paPoints + ((i+1) % m_dwOrigPoints));
         fLen = p.Length();
         fMaxLen = max(fMaxLen, fLen);
      }

      for (dwDivide = dwMinDivide;
         (dwDivide < dwMaxDivide) && (fLen > m_fDetail);
         dwDivide++, fLen /= 2);
   }
   else
      dwDivide = dwMinDivide;
      // else, cant possibly divide so dont bother testing
   dwDivide = (1 << dwDivide);

   // allocate and fill mmeory
   m_dwPoints = dwDivide * (m_dwOrigPoints - (m_fLoop ? 0 : 1)) + (m_fLoop ? 0 : 1);

   if (!m_memPoints.Required(m_dwPoints * sizeof(CPoint)))
      return FALSE;
   m_pPoints = (PCPoint) m_memPoints.p;

   if (paTextures) {
      if (!m_memTexture.Required((m_dwPoints+1) * sizeof(TEXTUREPOINT)))
         return FALSE;
      m_pTexture = (PTEXTUREPOINT) m_memTexture.p;
   }
   else
      m_pTexture = NULL;

   if (!m_memTangentStart.Required(m_dwPoints * sizeof(CPoint)))
      return FALSE;
   m_pTangentStart = (PCPoint) m_memTangentStart.p;

   if (!m_memTangentEnd.Required(m_dwPoints * sizeof(CPoint)))
      return FALSE;
   m_pTangentEnd = (PCPoint) m_memTangentEnd.p;

   // BUGFIX - If this is looped and have textures, then need to make a special case for
   // copying the last texture
   if (m_fLoop && m_pTexture)
      m_pTexture[m_dwOrigPoints * dwDivide] = paTextures[m_dwOrigPoints];

   // loop through all the segments, doing those segments with automagically
   // generated tangents first
   for (i = 0; i < m_dwOrigPoints; i++) {
      // store away this point and texture
      m_pPoints[i*dwDivide].Copy (paPoints + i);
      if (m_pTexture)
         m_pTexture[i*dwDivide] = paTextures[i];

      // if it's the last point and we don't loop then exit
      DWORD dwNext = (i + 1) % m_dwOrigPoints;
      if (!m_fLoop && (dwNext <= i))
         break;


      // apply textures
      PTEXTUREPOINT pTextSeg = m_pTexture ? &m_pTexture[i*dwDivide] : NULL;
      if (pTextSeg && (dwDivide > 1)) {
         fp fdh, fdv;
         // use i+1 so if loop will take into acount the extra texture info
         fdh = (paTextures[i+1].h - paTextures[i].h) / dwDivide;
         fdv = (paTextures[i+1].v - paTextures[i].v) / dwDivide;
         for (j = 1; j < dwDivide; j++) {
            pTextSeg[j].h = pTextSeg[j-1].h + fdh;
            pTextSeg[j].v = pTextSeg[j-1].v + fdv;
         }
      }

      // if this is a cubic spline (needs to know adjacent tangents) then skip
      DWORD dwSegCurve;
      dwSegCurve = SegCurve (i, padwSegCurve);
      if (dwSegCurve == SEGCURVE_CUBIC)
         continue;

      // do the segment from i to dwNext
      PCPoint pSeg = m_pPoints + (i * dwDivide);
      PCPoint pStartTan = m_pTangentStart + (i * dwDivide);
      PCPoint pEndTan = m_pTangentEnd + (i * dwDivide);

      if ((dwSegCurve == SEGCURVE_CIRCLEPREV) || (dwSegCurve == SEGCURVE_CIRCLENEXT)) {
         // determine what p1, p2, and p3 are (points that define curve)
         PCPoint p1, p2, p3;
         if (dwSegCurve == SEGCURVE_CIRCLEPREV) {
            p1 = paPoints + ((i + m_dwOrigPoints - 1) % m_dwOrigPoints);   // back one
            p2 = paPoints + i;   // this one
            p3 = paPoints + dwNext;  // forward one

            // What do if before beginning of points?
            if ((i == 0) && !m_fLoop)
               goto dolinear;
         }
         else {   // next
            p1 = paPoints + i;   // this one
            p2 = paPoints + dwNext;  // forward one
            p3 = paPoints + ((dwNext+1) % m_dwOrigPoints);  // forward two

            // What do if get to end of points?
            if ((dwNext+1 >= m_dwOrigPoints) && !m_fLoop)
               goto dolinear;
         }

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
            dwSegCurve = SEGCURVE_LINEAR;
            goto dolinear;
         }

         // calculate radius
         fp fRadius;
         pTemp.Subtract (&t2, &pIntersect);
         fRadius = pTemp.Length();

         // calculate the angle at the 2 points averaging
         // add two-pi so that know its a positive number
         fp fAngle1, fAngle2;
         if (dwSegCurve == SEGCURVE_CIRCLEPREV) {
            fAngle1 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
            fAngle2 = atan2((t3.p[0] - pIntersect.p[0]), (t3.p[1] - pIntersect.p[1]));
         }
         else {   // next
            fAngle1 = atan2((t1.p[0] - pIntersect.p[0]), (t1.p[1] - pIntersect.p[1]));
            fAngle2 = atan2((t2.p[0] - pIntersect.p[0]), (t2.p[1] - pIntersect.p[1]));
         }
         if (fAngle1 < fAngle2)
            fAngle1 += 2 * PI;

         // loop calculating points
         for (j = 0; j <= dwDivide; j++) {
            fp fAngle = fAngle1 + (fAngle2 - fAngle1) * ((fp) j / dwDivide);

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
            if (j && (j < dwDivide))
               pSeg[j].Copy (&pNew);

            // figure out the tangent, which will be in the direction
            // of the curve, and then length will be (fAngle2 - fAngle1) * fRadius / dwDivide
            CPoint pTan;
            h = sin (fAngle-.001) * fRadius + pIntersect.p[0];
            v = cos (fAngle-.001) * fRadius + pIntersect.p[1];
            pTan.Copy (&pH);
            pTan.Scale (h);
            pTemp.Copy (&pV);
            pTemp.Scale (v);
            pTan.Add (&pTemp);
            pTan.Add (&pC);
            pTan.Subtract (&pNew);
            pTan.Normalize();
            pTan.Scale (fabs(fAngle2 - fAngle1) * fRadius / dwDivide);

            if (j < dwDivide)
               pStartTan[j].Copy (&pTan);
            if (j)
               pEndTan[j-1].Copy (&pTan);
         }

         // done with circle interp
      }
      else if ((dwSegCurve == SEGCURVE_ELLIPSEPREV) || (dwSegCurve == SEGCURVE_ELLIPSENEXT)) {
         // ellipse, approximate with a bezier curve using 1/w

         // determine what p1, p2, and p3 are (points that define curve)
         PCPoint p1, p2, p3;
         if (dwSegCurve == SEGCURVE_ELLIPSEPREV) {
            p1 = paPoints + ((i + m_dwOrigPoints - 1) % m_dwOrigPoints);   // back one
            p2 = paPoints + i;   // this one
            p3 = paPoints + dwNext;  // forward one

            // What do if before beginning of points?
            if ((i == 0) && !m_fLoop)
               goto dolinear;
         }
         else {   // next
            p1 = paPoints + i;   // this one
            p2 = paPoints + dwNext;  // forward one
            p3 = paPoints + ((dwNext+1) % m_dwOrigPoints);  // forward two

            // What do if get to end of points
            if ((dwNext+1 >= m_dwOrigPoints) && !m_fLoop)
               goto dolinear;
         }
         
         // two vectors, point from p1 and p3 to p2
         CPoint V1,V2,C;
         V1.Subtract (p2, p1);
         V2.Subtract (p2, p3);
         C.Subtract (p1, &V2);

         // loop calculating points
         for (j = 0; j <= dwDivide; j++) {
            // what angle
            fp fAngle = (fp)j / (fp)dwDivide * PI / 4;

            CPoint pLoc, pTemp;
            if (dwSegCurve == SEGCURVE_ELLIPSEPREV)
               fAngle += PI/4;   // further down the list

            pLoc.Copy (&V2);
            pLoc.Scale (cos(fAngle));
            pTemp.Copy (&V1);
            pTemp.Scale (sin(fAngle));
            pLoc.Add (&pTemp);
            pLoc.Add (&C);

            // write it?
            if (j < dwDivide)
               pSeg[j].Copy (&pLoc);

            // what is the slope?
            CPoint pLoc2;
            pLoc2.Copy (&V2);
            pLoc2.Scale (cos(fAngle+.001));
            pTemp.Copy (&V1);
            pTemp.Scale (sin(fAngle+.001));
            pLoc2.Add (&pTemp);
            pLoc2.Add (&C);
            pLoc2.Subtract (&pLoc);
            pLoc2.Normalize();

            // what is the length of the line, to scale by the slow
            fp t;
            t = (V1.Length() + V2.Length()) / dwDivide / 2;
            // approximate length
            pLoc2.Scale (t);

            if (j < dwDivide)
               pStartTan[j].Copy (&pLoc2);
            if (j)
               pEndTan[j-1].Copy (&pLoc2);
         }

         // done with bezier interp
      }
      else {   // linear
dolinear:
         // figre out a length
         CPoint pTan;
         pTan.Subtract (&paPoints[dwNext], &paPoints[i]);
         pTan.Scale (1.0 / dwDivide);

         // write in all the tangents, which are all the same
         for (j = 0; j < dwDivide; j++) {
            pStartTan[j].Copy (&pTan);
            pEndTan[j].Copy (&pTan);
         }

         // fill in the points, incrementing by tangent each time
         for (j = 1; j < dwDivide; j++) {
            pSeg[j].Copy (&pSeg[j-1]);
            pSeg[j].Add (&pTan);
         }

         // done with linear interp
      }
   }

   // deal with cubic splines (and others) that need to know the tangent
   // of the surrounding points
   for (i = 0; i < m_dwOrigPoints; i++) {
      // if it's the last point and we don't loop then exit
      DWORD dwNext = (i + 1) % m_dwOrigPoints;
      if (!m_fLoop && (dwNext <= i))
         break;

      // if this is a cubic spline (needs to know adjacent tangents) then skip
      DWORD dwSegCurve;
      dwSegCurve = SegCurve (i, padwSegCurve);
      if (dwSegCurve != SEGCURVE_CUBIC)
         continue;

      // determine the 4 points...
      PCPoint p1, p2, p3, p4;
      if (m_fLoop || i)
         p1 = paPoints + ((i + m_dwOrigPoints -1) % m_dwOrigPoints);
      else
         p1 = paPoints + 0;
      p2 = paPoints + i;
      p3 = paPoints + dwNext;
      if (m_fLoop || (dwNext+1 < m_dwOrigPoints))
         p4 = paPoints + ((dwNext + 1) % m_dwOrigPoints);
      else
         p4 = paPoints + dwNext;

      // determine the slope (tangent) at points 2 and 3
      CPoint t2, t3;
      if (i || m_fLoop) {
         if (SegCurve((i+m_dwOrigPoints-1)%m_dwOrigPoints, padwSegCurve) == SEGCURVE_CUBIC) {
            // it's the same spline, so get slope by subtracting p1 from p3 and dividing
            // by two
            t2.Subtract (p3, p1);
            t2.Scale (.5);
         }
         else {
            // it's another sort of curve, which means the tangent has already
            // been figured out
            t2.Copy (m_pTangentEnd + ((i * dwDivide + m_dwPoints - 1) % m_dwPoints));
            t2.Scale (dwDivide); // so get in the right units
         }
      }
      else {
         // it's starting at pint
//         t2.Zero();
         // BUGFIX - Get he slope by assuming it's the direction of the points
         t2.Subtract (p3, p1);
         t2.Scale (.5);
      }
      if (m_fLoop || (i+2 < m_dwOrigPoints)) {  // BUGFIX - Changed i+1 to i+2 so that curve at end will be ok
         if (SegCurve((i+1)%m_dwOrigPoints, padwSegCurve) == SEGCURVE_CUBIC) {
            // it's the same spline, so get slope by subtracting p2 from p4 and dividing
            // by two
            t3.Subtract (p4, p2);
            t3.Scale (.5);
         }
         else {
            // it's another sort of curve, which means the tangent has already
            // been figured out
            t3.Copy (m_pTangentStart + ((i+1) * dwDivide) % m_dwPoints);
            t3.Scale (dwDivide); // so get in the right units
         }
      }
      else {
         // its the last pooint

         // BUGFIX - Get he slope by assuming it's the direction of the points
         t3.Subtract (p4, p2);
         t3.Scale (.5);
         //t3.Zero();
      }

      // do the segment from i to dwNext
      PCPoint pSeg = m_pPoints + (i * dwDivide);
      PCPoint pStartTan = m_pTangentStart + (i * dwDivide);
      PCPoint pEndTan = m_pTangentEnd + (i * dwDivide);

      // loop calculating points
      fp fScale;
      fScale = DistancePointToPoint (p2, p3) / (fp) dwDivide;
      for (j = 0; j <= dwDivide; j++) {
         // where along the curve?
         fp t;
         t = ((fp)j / (fp)dwDivide);

         // where is this point
         CPoint pLoc;
         HermiteCubic (t, p2, p3, &t2, &t3, &pLoc);

         // write it?
         if (j && (j < dwDivide))
            pSeg[j].Copy (&pLoc);

         // what is the slope?
         CPoint pLoc2;
         HermiteCubic (t+.001, p2, p3, &t2, &t3, &pLoc2);
         pLoc2.Subtract (&pLoc);
         pLoc2.Normalize();

         // Scale this approximating the length
         pLoc2.Scale (fScale);

         if (j < dwDivide)
            pStartTan[j].Copy (&pLoc2);
         if (j)
            pEndTan[j-1].Copy (&pLoc2);
      }

      // done with hermite interp
   }

   return TRUE;
}

/***********************************************************************************
CSpline::OrigPointReplace - Replace the original point with a new value.

inputs
   DWORD       dwIndex - Index into the original point
   PCPoint     pNew - New value
returns
   BOOL - TRUE if success
*/
BOOL CSpline::OrigPointReplace (DWORD dwIndex, PCPoint pNew)
{
   if (dwIndex >= m_dwOrigPoints)
      return FALSE;

   // copy over
   PCPoint paPoints;
   paPoints = (PCPoint) m_memOrigPoints.p;
   paPoints[dwIndex].Copy (pNew);

   return Recalc ();
}

/***********************************************************************************
CSpline::ToMem - Fills in memory bits with all the relevent spline info. THat
way the memory bits can be modified, and then :Init() can be called again, or
the information can be passed to a different spline.

inputs
   BOOL        *pfLooped - Filled with m_fLoop
   DWORD       *pdwPoints - Filled with number of points
   PCMem       pMemPoints - Filled with point values
   PCMem       pMemTextures - Filled with point values. m_dwCurPosn=0 if not filled
   PCMem       pMemSegCurve - Filled with segcurve valued
   DWORD       *pdwMinDivide - Filled with minimum divide
   DWORD       *pdwMaxDivide - Filled with maximum divide
   fp          *pfDetail  Filled with detail
returns
   BOOL - TRUE if succes
*/
BOOL CSpline::ToMem (BOOL *pfLooped, DWORD *pdwPoints, PCMem pMemPoints, PCMem pMemTextures,
                     PCMem pMemSegCurve, DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail)
{
   *pfLooped = m_fLoop;
   *pdwPoints = m_dwOrigPoints;
   *pdwMinDivide = m_dwMinDivide;
   *pdwMaxDivide = m_dwMaxDivide;
   *pfDetail = m_fDetail;

   DWORD dwNeeded = m_dwOrigPoints * sizeof(CPoint);
   if (!pMemPoints->Required (dwNeeded))
      return FALSE;
   memcpy (pMemPoints->p, m_memOrigPoints.p, dwNeeded);

   if (m_memOrigTextures.m_dwCurPosn && pMemTextures) {
      dwNeeded = (m_dwOrigPoints+(m_fLoop ? 1 : 0)) * sizeof(TEXTUREPOINT);
      if (!pMemTextures->Required (dwNeeded))
         return FALSE;
      pMemTextures->m_dwCurPosn = dwNeeded;
      memcpy (pMemTextures->p, m_memOrigTextures.p, dwNeeded);
   }
   else {
      if (pMemTextures)
         pMemTextures->m_dwCurPosn = 0;
   }

   dwNeeded = m_dwOrigPoints * sizeof(DWORD);
   if (!pMemSegCurve->Required (dwNeeded))
      return FALSE;
   memcpy (pMemSegCurve->p, m_memOrigSegCurve.p, dwNeeded);

   return TRUE;
}

/***********************************************************************************
CSpline::Init - Initializes the spline.

inputs
   BOOL        fLooped - TRUE if the spline loops around on itself, FALSE if it starts
                  and ends at the first and last point.
   DWORD       dwPoints - Number of points in the spline.
   PCPoint     paPoints - Pointer to an array of dwPoints points for the spline control
                  points.
   PTEXTUREPOINT paTextures - Pointer to an array of dwPoints texture values. If this
                  is NULL then don't calculate textures. NOTE: If fLooped this must be
                  filled with dwPoints+1 points, so that know the texture of the first
                  point at the beginning of the loop and the end.
   DWORD       *padwSegCurve - Pointer to anarray of DWORDs describing the curve/spline
                  to use between this point and the next. Must have dwPoints elements.
                  This can also be set directly to one of the SEGCURVE_XXXX values
                  to use that SEGCURVE for all the segments. Has dwPoints unless is NOT
                  looped, then dwPoints-1.
   DWORD       dwMinDivide - Minimum number of times to divide each spline point.
                  0 => no times, 1 => halve, 2=>quarter, 3=>eiths, etc.
   DWORD       dwMaxDivide - Maximum number of times to divide each splint point
   fp      fDetail - Divide spline points until the longest length < fDetail,
                  or have divided dwMaxDivide times
returns
   BOOL - TRUE if succeded
*/
BOOL CSpline::Init (BOOL fLooped, DWORD dwPoints, PCPoint paPoints, PTEXTUREPOINT paTextures,
      DWORD *padwSegCurve, DWORD dwMinDivide, DWORD dwMaxDivide,
      fp fDetail)
{
   if (dwPoints < 2)
      return FALSE;

   // remember these parameters
   m_dwOrigPoints = dwPoints;
   m_dwMinDivide = dwMinDivide;
   m_dwMaxDivide = dwMaxDivide;
   m_fDetail = fDetail;
   DWORD dwNeeded;
   if (m_memOrigPoints.Required(dwNeeded = dwPoints * sizeof(CPoint)))
      memcpy (m_memOrigPoints.p, paPoints, dwNeeded);
   if (paTextures) {
      if (m_memOrigTextures.Required (dwNeeded = (dwPoints+(fLooped ? 1 : 0)) * sizeof(TEXTUREPOINT))) {
         memcpy (m_memOrigTextures.p, paTextures, dwNeeded);
         m_memOrigTextures.m_dwCurPosn = dwNeeded;
      }
      else
         m_memOrigTextures.m_dwCurPosn = 0;
   }
   else
      m_memOrigTextures.m_dwCurPosn = 0;
   if (m_memOrigSegCurve.Required (dwNeeded = dwPoints * sizeof(DWORD))) { // BUGFIX - Was sizeof(CPoint))
      DWORD i;
      for (i = 0; i < dwPoints - (fLooped ? 0 : 1); i++)
         ((DWORD*) m_memOrigSegCurve.p)[i] = SegCurve (i, padwSegCurve);
   }

   m_fLoop = fLooped;

   return Recalc ();
}


/*********************************************************************************
CSpline::QueryNodes - Returns the number of nodes in the interpolated spline.

returns
   DWORD - number of nodes. If looped, does NOT include the final node to loop around
*/
DWORD CSpline::QueryNodes(void)
{
   return m_dwPoints;
}

/*********************************************************************************
CSpline::QueryLooped - Returns TRUE if the spline is looped, false if it isnt
*/
BOOL  CSpline::QueryLooped (void)
{
   return m_fLoop;
}

/*********************************************************************************
CSpline::LocationGet - Returns a pointer to the CPoint given the index dwH,

inputs
   DWORD       dwH - Index from 0..QueryNodes()-1.
   
returns
   PCPoint - Point to use. Do NOT change this

*/
PCPoint CSpline::LocationGet (DWORD dwH)
{
   if (dwH >= m_dwPoints)
      return NULL;

   return m_pPoints + dwH;
}

/*********************************************************************************
CSpline::TextureGet - Returns a pointer to the TEXTUREPOINT given the index dwH,

inputs
   DWORD       dwH - Index from 0..QueryNodes()-1. If this is a loop it goes to QueryNodes()-0.
   
returns
   PTEXTUREPOINT - Point to use. Do NOT change this

*/
PTEXTUREPOINT CSpline::TextureGet (DWORD dwH)
{
   if (!m_pTexture)
      return NULL;
   if (dwH >= m_dwPoints + (m_fLoop ? 1 : 0))
      return NULL;

   return m_pTexture + dwH;
}

/*********************************************************************************
CSpline::TangentGet - Returns a pointer to the CPoint given the index dwH.

inputs
   DWORD       dwH - Index from 0..QueryNodes()-1.
   BOOL        fRight - If TRUE, returns the tangent to the right of the
               point. If FALSE, returns the tanget to the left of the point.
   
returns
   PCPoint - Tangent to the point. Do NOT change this

*/
PCPoint CSpline::TangentGet (DWORD dwH, BOOL fRight)
{
   if (dwH >= m_dwPoints)
      return NULL;

   if (fRight) {
      return m_pTangentStart + dwH;
   }
   else {
      dwH = ((dwH + m_dwPoints - 1) % m_dwPoints); // to the left
      return m_pTangentEnd + dwH;
   }
}


/*********************************************************************************
CSpline::TangentGet - Returns a tangent around the point

inputs
   fp          fH - Value
   CPoint      pVal - Filled with the tangent. This IS normalized.
   
returns
   BOOL - TRUE if success

*/
BOOL CSpline::TangentGet (fp fH, PCPoint pVal)
{
   if ((fH < 0) || (fH > 1))
      return FALSE;

   fp fLeft, fRight;
   if (m_fLoop) {
      fLeft = myfmod(fH - CLOSE, 1);
      fRight = myfmod(fH + CLOSE, 1);
   }
   else {
      fLeft = max(0, fH-CLOSE);
      fRight = min(1, fH+CLOSE);
   }

   // which node?
   DWORD dwLeft, dwRight;
   dwLeft = (DWORD) (fLeft * (fp)(m_dwPoints - (m_fLoop ? 0 : 1)));
   dwRight = (DWORD) (fRight * (fp)(m_dwPoints - (m_fLoop ? 0 : 1)));

   CPoint p1, p2;
   p1.Copy (TangentGet (dwLeft, TRUE));
   p2.Copy (TangentGet (dwRight, TRUE));
   p1.Normalize();
   p2.Normalize();
   pVal->Add (&p1, &p2);
   pVal->Normalize();

   return TRUE;
}
/*********************************************************************************
CSpline::LocationGet - Fills in a point with the location using the spline.

inputs
   fp      fH - Value from 0 to 1 (inclusive) covering the entire length of the
                     spline, including the wrap-around bit.
   PCPoint     pP - Filled in with the point
returns
   BOOL - TRUE if success, false if bad fH
*/
BOOL CSpline::LocationGet (fp fH, PCPoint pP)
{
   if ((fH < 0) || (fH > 1))
      return FALSE;

   // which point
   DWORD dwPoint;
   // BUGFIX: Was + (m_fLoop ? 1 : 0) - but this didn't work
   fH = fH * (m_dwPoints - (m_fLoop ? 0 : 1));
   dwPoint = (DWORD) fH;
   fH -= dwPoint;

   // get the point and its end point
   PCPoint p1, p2;
   p1 = m_pPoints + (dwPoint % m_dwPoints);
   p2 = m_pPoints + ((dwPoint+1) % m_dwPoints);
   
   // interpolate
   pP->Subtract (p2, p1);
   pP->Scale (fH);
   pP->Add (p1);

   return TRUE;
}

/*********************************************************************************
CSpline::TextureGet - Fills in a texture point with the location using the spline.

inputs
   fp      fH - Value from 0 to 1 (inclusive) covering the entire length of the
                     spline, including the wrap-around bit.
   PTEXTUREPOINT pT - Filled with the texture bit
returns
   BOOL - TRUE if success, false if bad fH
*/
BOOL CSpline::TextureGet (fp fH, PTEXTUREPOINT pT)
{
   if ((fH < 0) || (fH > 1) || !m_pTexture)
      return FALSE;

   // which point
   DWORD dwPoint;
   DWORD dwC = (m_dwPoints + (m_fLoop ? 1 : 0));
   fH = fH * (dwC-1);   // BUGFIX - Was dwC, but this caused crash when not looped
   dwPoint = (DWORD) fH;
   dwPoint = min(dwPoint, dwC-2);
      // BUGFIX - Was dwC-1, but must be dwC-2 so that p2 won't go beyond end
      // Fixed because was causing a crash in balustrades drawing of thumbnails
   fH -= dwPoint;

   // get the point and its end point
   PTEXTUREPOINT p1, p2;
   p1 = m_pTexture + dwPoint;
   p2 = m_pTexture + (dwPoint+1);
   
   // interpolate
   pT->h = p1->h + (p2->h - p1->h) * fH;
   pT->v = p1->v + (p2->v - p1->v) * fH;

   return TRUE;
}



/*********************************************************************************
CSpline::OrigNumPointsGet - Returns the number of points originally passed into the
spline.
*/
DWORD CSpline::OrigNumPointsGet (void)
{
   return m_dwOrigPoints;
}

/*********************************************************************************
CSpline::LoopedGet - Returns TRUE if the spline is looped, FALSE if it's not
connected on the ends
*/
BOOL CSpline::LoopedGet (void)
{
   return m_fLoop;
}

/*********************************************************************************
CSpline::LoopedSet - Sets the loop state after Init has been called
*/
BOOL CSpline::LoopedSet (BOOL fLoop)
{
   if (fLoop == m_fLoop)
      return TRUE;

   m_fLoop = fLoop;
   return Recalc();
}

/*********************************************************************************
CSpline::OrigPointGet - Fills in pP with the original point.

inputs
   DWORD       dwH - from 0 to OrigNumPointsGet()-1
   PCPoint     pP - Filled with the original point
returns
   BOOL - TRUE if successful
*/
BOOL CSpline::OrigPointGet (DWORD dwH, PCPoint pP)
{
   if (dwH >= m_dwOrigPoints)
      return FALSE;

   pP->Copy (((PCPoint) m_memOrigPoints.p) + dwH);
   return TRUE;
}

/*********************************************************************************
CSpline::OrigTextureGet - Fills in pT with the original texture point.

inputs
   DWORD       dwH - from 0 to OrigNumPointsGet(), -1 if not looped
   PTEXTUREPOINT pT - Filled with the texture point
returns
   BOOL - TRUE if successful. May fail if no textures were specified in the first place
*/
BOOL CSpline::OrigTextureGet (DWORD dwH, PTEXTUREPOINT pT)
{
   if ( (dwH >= (m_dwOrigPoints + (m_fLoop ? 1 : 0))) || !m_memOrigTextures.m_dwCurPosn)
      return FALSE;

   *pT = ((PTEXTUREPOINT) m_memOrigTextures.p)[dwH];
   return TRUE;
}

/*********************************************************************************
CSpline::OrigSegCurveGet - Returns the segcurve descriptor... SEGCURVE_XXX, for
the index into the original points.

inputs
   DWORD       dwH - From 0 to OrigNumPointsGet()-1. an extra -1 if not looped
   DWORD       *padwSegCurve - Filled in with the segment curve
returns
   BOOL - TRUE if successful
*/
BOOL CSpline::OrigSegCurveGet (DWORD dwH, DWORD *pdwSegCurve)
{
   if (dwH >= (m_dwOrigPoints - (m_fLoop ? 0 : 1)))
      return FALSE;

   *pdwSegCurve = ((DWORD*) m_memOrigSegCurve.p)[dwH];
   return TRUE;
}

/*********************************************************************************
CSpline::DivideGet - Fills in the divide information

inputs
   DWORD*      pdwMinDivide, pdwMaxDivide - If not null, filled in with min/max divide.
   fp*     pfDetail - If not null, filled in with detail
*/
void CSpline::DivideGet (DWORD *pdwMinDivide, DWORD *pdwMaxDivide, fp *pfDetail)
{
   if (pdwMinDivide)
      *pdwMinDivide = m_dwMinDivide;
   if (pdwMaxDivide)
      *pdwMaxDivide = m_dwMaxDivide;
   if (pfDetail)
      *pfDetail = m_fDetail;
}



/*********************************************************************************
CSpline::DivideSet - Sets the divide amount

inputs
   DWORD      dwMinDivide, dwMaxDivide - Min/max divide.
   fp         fDetail - detail
*/
void CSpline::DivideSet (DWORD dwMinDivide, DWORD dwMaxDivide, fp fDetail)
{
   m_dwMinDivide = dwMinDivide;
   m_dwMaxDivide = dwMaxDivide;
   m_fDetail = fDetail;
   Recalc();
}


/*********************************************************************************
CSpline::SplineExpand - Given a looped spline, this creates a new spline that is fExpand
meters larger around, on each side (fExpand can be negative to shrink.) Use this
to follow a track around the inside or outside of the spline.

inputs
   PCSpline       pSpline - Original spline
   fp         *pafExpand - Amount to expand. One fp for each edge of the spline.
         NOTE: For some reason, looks at the point to the left for expansion.
         Some code depnds on this though
   PCPoint        pUpDir - Up direction. Used for determining how to expand. Use NULL for z=1 is up
returns
   PCSpline - new spline. Must be freed by the caller
*/
PCSpline CSpline::Expand (fp *pafExpand, PCPoint pUpDir)
{
   CMem memPoints, memCurve, memTexture;
   DWORD i;
   PCPoint paPoints;
   PTEXTUREPOINT pt;
   DWORD *padwCurve;
   DWORD dwNum = OrigNumPointsGet();
   if (!memPoints.Required (sizeof(CPoint) * dwNum))
      return NULL;
   if (!memCurve.Required (sizeof(DWORD) * dwNum))
      return NULL;
   paPoints = (PCPoint) memPoints.p;
   padwCurve = (DWORD*) memCurve.p;

   // BUGFIX - get texture points
   if (m_pTexture) {
      if (!memTexture.Required (sizeof(TEXTUREPOINT) * (dwNum+1)))
         return NULL;
      pt = (PTEXTUREPOINT) memTexture.p;

      DWORD dwSize;
      dwSize = sizeof(TEXTUREPOINT) * (dwNum+1);
      dwSize = min((DWORD)m_memOrigTextures.m_dwAllocated, dwSize);
      memcpy (pt, m_memOrigTextures.p, dwSize);
   }
   else
      pt = NULL;

   DWORD dwNodes = QueryNodes ();
   DWORD dwCenter;
   PCPoint pCenter, pLeft, pRight;
   CPoint pEllipseCenter, pEllipseLeft, pEllipseRight;

   CPoint pUp;
   if (pUpDir)
      pUp.Copy (pUpDir);
   else {
      pUp.Zero();
      pUp.p[2] = 1;
   }

   for (i = 0; i < dwNum; i++) {
      // remember the curve
      OrigSegCurveGet (i, padwCurve + i);

      // for most types of curve segments get the extrapolated curve
      if (padwCurve[i] != SEGCURVE_ELLIPSEPREV) {
         if (m_fLoop)
            dwCenter = dwNodes * i / dwNum;
            // assuming it's looped when calculating dwCenter
         else
            dwCenter = (dwNodes - 1) * i / (dwNum - 1);

         pCenter = LocationGet (dwCenter);

         // BUGFIX - Deal with linear
         if (m_fLoop || dwCenter)
            pLeft = LocationGet ((dwCenter + dwNodes - 1) % dwNodes);
         else
            pLeft = NULL;

         // BUGFIX - Deal with linear
         if (m_fLoop || (dwCenter+1 < dwNodes))
            pRight = LocationGet ((dwCenter + 1) % dwNodes);
         else
            pRight = NULL;

         //if (!pCenter || !pLeft || !pRight)
         //   return NULL;
      }
      else {
         // for this one point, on the segment, because it's not on the surface,
         // do something different
         OrigPointGet (i, &pEllipseCenter);
         pCenter = &pEllipseCenter;

         // BUGFIX - Deal with linear
         if (m_fLoop || i) {
            OrigPointGet ((i+dwNum-1)%dwNum, &pEllipseLeft);
            pLeft = &pEllipseLeft;
         }
         else
            pLeft = NULL;

         if (m_fLoop || (i+1 < dwNum)) {
            OrigPointGet ((i+1)%dwNum, &pEllipseRight);
            pRight = &pEllipseRight;
         }
         else
            pRight = NULL;
      }

      // BUGFIX - Deal with linear

      // if there isnt' a center then error
      if (!pCenter)
         return NULL;

      // if there isn't a left then make sure there's a right, and vice versa
      CPoint pFlip;
      if (!pLeft) {
         if (!pRight)
            return NULL;

         pFlip.Subtract (pCenter, pRight);
         pFlip.Add (pCenter);
         pLeft = &pFlip;
      }
      if (!pRight) {
         if (!pLeft)
            return NULL;

         pFlip.Subtract (pCenter, pLeft);
         pFlip.Add (pCenter);
         pRight = &pFlip;
      }


      // use this to find the normals
      CPoint pL, pR, pLN, pRN;
      pL.Subtract (pCenter, pLeft);
      pLN.CrossProd (&pUp, &pL);
      pLN.Normalize();
      pLN.Scale (pafExpand[(i+dwNum-1)%dwNum]);

      pR.Subtract (pCenter, pRight);
      pRN.CrossProd (&pR, &pUp);
      pRN.Normalize();
      pRN.Scale (pafExpand[i]);

      // two lines
      CPoint pSideLeft1, pSideLeft2, pSideRight1, pSideRight2;
      pSideLeft1.Add (pLeft, &pLN);
      pSideLeft2.Add (pCenter, &pLN);
      pSideRight1.Add (pRight, &pRN);
      pSideRight2.Add (pCenter, &pRN);

      if (!IntersectLinePlane (&pSideLeft1, &pSideLeft2, &pSideRight1, &pRN, &paPoints[i])) {
         // they dont intersect, so just clame they do by adding center to pLN, since probably
         // they two lines are parallel.
         paPoints[i].Add (pCenter, &pLN);
         continue;
      }

   }
   
   PCSpline pNew;
   pNew = new CSpline;
   if (!pNew)
      return NULL;

   DWORD dwMin, dwMax;
   fp fDetail;
   DivideGet (&dwMin, &dwMax, &fDetail);

   pNew->Init (LoopedGet(), dwNum, paPoints, pt, padwCurve, dwMin, dwMax, fDetail);

   return pNew;
}

// This version uses the same value all around
PCSpline CSpline::Expand (fp fExpand, PCPoint pUpDir)
{
   CMem mem;
   DWORD dwNum = OrigNumPointsGet();
   DWORD i;
   fp *paf;
   if (!mem.Required (sizeof(fp) * dwNum))
      return NULL;
   paf = (fp*) mem.p;
   for (i = 0; i < dwNum; i++)
      paf[i] = fExpand;

   return Expand (paf, pUpDir);
}


/*********************************************************************************
CSpline::Flip - Keeps drawing the exact same spline, but flips it from back to
front, so the start becomes the end and vice versa

returns
   PCSpline - New spline that's flipped
*/
CSpline * CSpline::Flip (void)
{
   CMem memPoints, memCurve, memTexture;
   DWORD i;
   PCPoint paPoints;
   PTEXTUREPOINT pt;
   DWORD *padwCurve;
   DWORD dwNum = OrigNumPointsGet();
   if (!memPoints.Required (sizeof(CPoint) * dwNum))
      return NULL;
   if (!memCurve.Required (sizeof(DWORD) * dwNum))
      return NULL;
   paPoints = (PCPoint) memPoints.p;
   padwCurve = (DWORD*) memCurve.p;

   // BUGFIX - get texture points
   if (m_pTexture) {
      if (!memTexture.Required (sizeof(TEXTUREPOINT) * (dwNum+1)))
         return NULL;
      pt = (PTEXTUREPOINT) memTexture.p;

      DWORD dwSize;
      dwSize = sizeof(TEXTUREPOINT) * (dwNum+1);
      dwSize = min((DWORD)m_memOrigTextures.m_dwAllocated, dwSize);
      memcpy (pt, m_memOrigTextures.p, dwSize);
   }
   else
      pt = NULL;

   DWORD dwVal;

   for (i = 0; i < dwNum; i++) {
      // swap the points and textures
      OrigPointGet (i, &paPoints[dwNum - i - 1]);
      if (pt)
         OrigTextureGet (i, &pt[dwNum - i - 1]);

      // get the curve
      dwVal = 0;
      if (m_fLoop || (i+1 < dwNum)) {
         OrigSegCurveGet (i, &dwVal);
         switch (dwVal) {
         case SEGCURVE_CIRCLEPREV:
            dwVal = SEGCURVE_CIRCLENEXT;
            break;
         case SEGCURVE_CIRCLENEXT:
            dwVal = SEGCURVE_CIRCLEPREV;
            break;
         case SEGCURVE_ELLIPSEPREV:
            dwVal = SEGCURVE_ELLIPSENEXT;
            break;
         case SEGCURVE_ELLIPSENEXT:
            dwVal = SEGCURVE_ELLIPSEPREV;
            break;
         }
         padwCurve[i] = dwVal;   // NOTE: Not flipping this yet
      }
   }

   // flip the segvment curve
   DWORD dwNumCurve;
   dwNumCurve = dwNum;
   if (!m_fLoop)
      dwNumCurve--;
   for (i = 0; i < dwNumCurve/2; i++) {
      dwVal = padwCurve[i];
      padwCurve[i] = padwCurve[dwNumCurve-i-1];
      padwCurve[dwNumCurve-i-1] = dwVal;
   }
   if (m_fLoop) {
      // BUGFIX - If looped need to rotate around 1...
      dwVal = padwCurve[0];
      memmove (padwCurve + 0, padwCurve + 1, (dwNumCurve-1) * sizeof(DWORD));
      padwCurve[dwNumCurve-1] = dwVal;
   }

   
   PCSpline pNew;
   pNew = new CSpline;
   if (!pNew)
      return NULL;

   DWORD dwMin, dwMax;
   fp fDetail;
   DivideGet (&dwMin, &dwMax, &fDetail);

   pNew->Init (LoopedGet(), dwNum, paPoints, pt, padwCurve, dwMin, dwMax, fDetail);

   return pNew;
}


/*********************************************************************************
CSpline::ExtendEndpoint - Either extends the start or end by the given distance,
keeping the same curve profile.

inputs
   BOOL     fStart - if TRUE extend the start, FALSE extend the end
   fp       fDist - Distance to extend by
returns
   BOOL - TRUE if succcess
*/
BOOL CSpline::ExtendEndpoint (BOOL fStart, fp fDist)
{
   // cant extned if looped
   if (m_fLoop)
      return FALSE;

   // get the dirction vector
   PCPoint pTan;
   CPoint pDir;
   pTan = TangentGet (fStart ? 0 : QueryNodes()-1, fStart);
   if (!pTan)
      return FALSE;
   pDir.Copy (pTan);
   pDir.Normalize();
   if (fStart)
      fDist *= -1;   // since tangent going the oppoiste way
   pDir.Scale (fDist);

   PCPoint paPoints;
   paPoints = (PCPoint) m_memOrigPoints.p + (fStart ? 0 : (OrigNumPointsGet()-1));
   paPoints->Add (&pDir);

   return Recalc ();

}



/*********************************************************************************
CSpline::FindCrossing - Given a plane parralel to X, Y, or Z, this finds where
along the spline the plane is first crossed.

inputs
   DWORD    dwPlane - 0..2, for X..Z
   fp       fValue - Value lookin for in in X..Z
returns
   fp - Where it crosses, from 0..1 (along length of spline), or -1 if doesn't cross
*/
fp CSpline::FindCrossing (DWORD dwPlane, fp fValue)
{
   DWORD i, dwNum;
   dwNum = QueryNodes();

   // look through info
   for (i = 0; i < dwNum - (m_fLoop ? 0 : 1); i++) {
      PCPoint p1, p2;
      p1 = LocationGet (i);
      p2 = LocationGet ((i+1)%dwNum);

      // min and max
      fp fMin, fMax;
      fMin = min(p1->p[dwPlane], p2->p[dwPlane]);
      fMax = max(p1->p[dwPlane], p2->p[dwPlane]);
      if ((fValue + EPSILON < fMin) || (fValue - EPSILON > fMax))
         continue;   // not here

      // elese it is
      if (fMax - fMin <= EPSILON) {
         // straight up and down
         return (fp) i / (fp) (dwNum - (m_fLoop ? 0 : 1));
      }

      // intersect
      fp fAlpha;
      fAlpha = p2->p[dwPlane] - p1->p[dwPlane];
      fAlpha = (fValue - p1->p[dwPlane]) / fAlpha;
      fAlpha = max(0, fAlpha);
      fAlpha = min(1, fAlpha);

      return ((fp) i+fAlpha) / (fp) (dwNum - (m_fLoop ? 0 : 1));
   }

   return -1;
}

