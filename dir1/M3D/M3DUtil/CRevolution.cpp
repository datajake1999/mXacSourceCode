/*****************************************************************************
CRevolution.cpp - 3D revolutions object

begun 7/6/02 by Mike Rozak.
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define MAXS         64

/************************************************************************************
CRevolution::Constructor and destructor
*/
CRevolution::CRevolution(void)
{
   m_psProfile = NULL;
   m_psRevolution = NULL;
   m_dwProfileType = RPROF_CUSTOM;
   m_dwRevolutionType = RREV_CUSTOM;
   m_dwProfileDetail = m_dwRevolutionDetail = 1;
   m_pScale.p[0] = m_pScale.p[1] = m_pScale.p[2] = m_pScale.p[3] = 1;
   m_pBottom.Zero();
   m_pAroundVec.Zero();
   m_pAroundVec.p[2] = 1;
   m_pLeftVec.Zero();
   m_pLeftVec.p[0] = 1;
   m_fDirty = TRUE;
   m_mMatrix.Identity();
   m_dwX = m_dwY = 0;
   m_fBackface = FALSE;
   m_aiNormTip[0] = m_aiNormTip[1] = 0;
   m_adwTextRepeat[0] = m_adwTextRepeat[1] = 0;
}

CRevolution::~CRevolution(void)
{
   if (m_psProfile)
      delete m_psProfile;
   if (m_psRevolution)
      delete m_psRevolution;
}

/************************************************************************************
CRevolution::ProfileSet - Sets the spline that describes the profile revolution.
Some notes:
   - Only X and Y should be set. Z should be set to 0.
   - X should be positive
   - Rotate around Y axis
   - Y should go from negative numbers to positive numbers
   - Maximum X value should be 1 (so scale works well)
   - Y should go from 0 to 1 (although this rule can be broken)

inputs
   PCSpline    pSpline - This spline is cloned and used
   int         iNormTipStart - If not 0, then all the points at the start have
               their normals set to m_pAroundVec (if 1) or -m_pAroundVec (if -1).
               Good for drawing spehere
   int         iNormTipEnd - If not 0, then all the points at the end have
               their normals set to m_pAroundVec (if 1) or -m_pAroundVec (if -1).
               Good for drawing spehere
returns
   BOOL - TRUE if succes
*/
BOOL CRevolution::ProfileSet (PCSpline pSpline, int iNormTipStart, int iNormTipEnd)
{
   if (!m_psProfile)
      m_psProfile = new CSpline;
   if (!m_psProfile)
      return FALSE;
   if (!pSpline->CloneTo (m_psProfile))
      return FALSE;

   m_dwProfileType = RPROF_CUSTOM;
   m_aiNormTip[0] = iNormTipStart;
   m_aiNormTip[1] = iNormTipEnd;
   m_fDirty = TRUE;
   return TRUE;
}

/************************************************************************************
CRevolution::ProfileSet - Sets the profile as a single line from one location to
another - this can be use ful for doing rims.

inputs
   PCPoint     pStart, pEnd - Start and end. pStart.p[x] < pEnd.p[x]
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::ProfileSet (PCPoint pStart, PCPoint pEnd)
{
   CPoint ap[2];
   CSpline s;
   ap[0].Copy (pStart);
   ap[1].Copy (pEnd);

   if (!s.Init (FALSE, 2, ap, NULL, (DWORD*)SEGCURVE_LINEAR, 0, 0))
      return FALSE;

   if (!ProfileSet (&s))
      return FALSE;

   m_dwProfileDetail = 0;
   return TRUE;
}

/************************************************************************************
CRevolution::ProfileSet - Sets the profile using one of the built-in profiles.

inputs
   DWORD       dwType - One of RPROF_XXX, except for RPROF_CUSTOM
   DWORD       dwDetail - Passed to CSpline init for the number of divisions
returns
   BOOL - TRUE if succes
*/
BOOL CRevolution::ProfileSet (DWORD dwType, DWORD dwDetail)
{
   DWORD dwNum, i;
   DWORD adwCurve[MAXS];
   CPoint apPoint[MAXS];
   int aiNormTip[2];
   aiNormTip[0] = aiNormTip[1] = 0;
   memset (adwCurve, 0, sizeof(adwCurve));
   memset (apPoint, 0, sizeof(apPoint));
   dwNum = 0;

   switch (dwType) {
   case RPROF_LINEVERT:
      apPoint[0].p[0] = 1;
      apPoint[1].p[0] = 1;
      apPoint[1].p[1] = 1;
      dwNum = 2;
      break;

   case RPROF_LINEDIAG2:
      apPoint[0].p[0] = .5;
      apPoint[1].p[0] = 1;
      apPoint[1].p[1] = 1;
      dwNum = 2;
      break;

   case RPROF_LINEDIAG23:
      apPoint[0].p[0] = 2.0 / 3.0;
      apPoint[1].p[0] = 1;
      apPoint[1].p[1] = 1;
      dwNum = 2;
      break;

   case RPROF_LINECONE:
      apPoint[1].p[0] = 1;
      apPoint[1].p[1] = 1;
      dwNum = 2;
      break;

   case RPROF_LIGHTGLOBE:
      apPoint[1].p[0] = .4;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = .4;
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[0] = 1;
      apPoint[4].Copy (&apPoint[3]);
      apPoint[4].p[1] = (apPoint[3].p[1] + 1) / 2;
      apPoint[5].Copy (&apPoint[4]);
      apPoint[5].p[1] = 1;
      apPoint[6].Copy (&apPoint[5]);
      apPoint[6].p[0] = 0;
      adwCurve[2] = SEGCURVE_ELLIPSENEXT;
      adwCurve[3] = SEGCURVE_ELLIPSEPREV;
      adwCurve[4] = SEGCURVE_ELLIPSENEXT;
      adwCurve[5] = SEGCURVE_ELLIPSEPREV;
      dwNum = 7;

      // BUGFIX - So normals at tend ok
      aiNormTip[1] = 1;
      break;

   case RPROF_C:
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[0] = 1;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = 1;
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[0] = 0;
      dwNum = 4;
      break;

   case RPROF_L:
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[0] = 1;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = 1;
      dwNum = 3;
      break;

   case RPROF_LDIAG:
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[0] = 2.0 / 3.0;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = 1;
      apPoint[2].p[0] = 1;
      dwNum = 3;
      break;

   case RPROF_SHADEDIRECTIONAL:
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[0] = .5;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = 1.0 / 3.0;
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = 1;
      dwNum = 4;
      break;

   case RPROF_SHADEDIRECTIONAL2:
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[0] = .75;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = .45;
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = .55;
      apPoint[4].Copy (&apPoint[3]);
      apPoint[4].p[1] = 1;
      dwNum = 5;
      break;

   case RPROF_SHADECURVED:
      apPoint[0].p[0] = .5;   // ellipse next
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[1] = .8;   // ellipseprev
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[0] = 1;  // linear
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      dwNum = 4;
      break;

   case RPROF_FANCYGLASSSHADE:
      apPoint[0].p[0] = .4;   // ellipsenext
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[1] = .1;   // ellipseprev
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[0] = .65;  // ellipsenext
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[0] = 1;    // ellipseprev
      apPoint[1].p[1] = .2;
      apPoint[4].Copy (&apPoint[3]);
      apPoint[4].p[1] = .4;   // ellipsenext
      apPoint[5].Copy (&apPoint[4]);
      apPoint[5].p[1] = .6;   // ellipseprev
      apPoint[6].Copy (&apPoint[5]);
      apPoint[5].p[6] = .8;   // ellipseprev
      apPoint[6].p[0] = .65;   // ellipsenext
      apPoint[7].Copy (&apPoint[6]);
      apPoint[7].p[0] = .4;   // ellipseprev
      apPoint[8].Copy (&apPoint[7]);
      apPoint[8].p[1] = .9;   // ellipsenext
      apPoint[9].Copy (&apPoint[8]);
      apPoint[9].p[1] = 1;   // ellipseprev
      apPoint[10].Copy (&apPoint[9]);
      apPoint[10].p[0] = .6;
      dwNum = 11;
      for (i = 0; i < dwNum; i++)
         adwCurve[i] = (i % 2) ? SEGCURVE_ELLIPSEPREV : SEGCURVE_ELLIPSENEXT;
      break;

   case RPROF_LIGHTSPOT:
      apPoint[1].p[0] = .4;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = .4;
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = 1;
      apPoint[4].Copy (&apPoint[3]);
      apPoint[4].p[0] = 0;
      dwNum = 5;
      break;

   case RPROF_CIRCLE:
      apPoint[1].p[0] = 1;
      apPoint[2].p[0] = 1;
      apPoint[2].p[1] = .5;
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = 1;
      apPoint[4].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      adwCurve[2] = SEGCURVE_ELLIPSENEXT;
      adwCurve[3] = SEGCURVE_ELLIPSEPREV;
      dwNum = 5;

      // BUGFIX - So normals at tend ok
      aiNormTip[0] = -1;
      aiNormTip[1] = 1;
      break;

   case PPROF_CIRCLEQS:
      apPoint[1].p[0] = 1;
      apPoint[2].p[0] = 1;
      apPoint[2].p[1] = .5;
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      dwNum = 4;

      // BUGFIX - So normals at tend ok
      aiNormTip[0] = -1;
      break;

   case RPROF_CIRCLEOPEN:
      apPoint[0].p[0] = .3;
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[1] = .05;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[0] = 1;
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = .5;
      apPoint[4].p[0] = 1;
      apPoint[4].p[1] = 1;
      apPoint[5].p[1] = 1;
      adwCurve[1] = SEGCURVE_ELLIPSENEXT;
      adwCurve[2] = SEGCURVE_ELLIPSEPREV;
      adwCurve[3] = SEGCURVE_ELLIPSENEXT;
      adwCurve[4] = SEGCURVE_ELLIPSEPREV;
      dwNum = 6;

      // BUGFIX - So normals at tend ok
      aiNormTip[1] = 1;
      break;

   case PRROF_CIRCLEHEMI:
      apPoint[1].p[0] = 1;
      apPoint[2].p[0] = 1;
      apPoint[2].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      dwNum = 3;

      // BUGFIX - So normals at tend ok
      aiNormTip[0] = -1;
      break;

   case RPROF_RING:
      apPoint[0].p[0] = 1.0;
      apPoint[1].p[0] = 1.05;
      dwNum = 2;
      break;

   case RPROF_RINGWIDE:
      apPoint[0].p[0] = 1.0;
      apPoint[1].p[0] = 1.2;
      dwNum = 2;
      break;

   case RPROF_HORN:
      apPoint[0].p[0] = .1;
      apPoint[1].p[0] = .1;
      apPoint[1].p[1] = 1;
      apPoint[2].p[0] = 1;
      apPoint[2].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      dwNum = 3;
      break;

   case RPROF_CIRCLEQUARTER:
      apPoint[1].p[0] = .75;
      apPoint[1].p[1] = .25;
      apPoint[2].p[0] = 1;
      apPoint[2].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      dwNum = 3;

      // BUGFIX - So normals at tend ok
      aiNormTip[0] = -1;
      break;

   case PRROF_CERAMICLAMPSTAND1:
      apPoint[0].p[0] = .5;
      apPoint[1].Copy (&apPoint[0]);      // ellipsenext
      apPoint[1].p[0] = 1;
      apPoint[1].p[1] = 2.0 / 3.0;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[1] = .9;   // ellipseprv
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[0] = 1.0 / 3.0;
      apPoint[4].Copy (&apPoint[3]);
      apPoint[4].p[1] = 1;
      apPoint[5].Copy (&apPoint[4]);
      apPoint[5].p[0] = 0;
      dwNum = 6;
      adwCurve[1] = SEGCURVE_ELLIPSENEXT;
      adwCurve[2] = SEGCURVE_ELLIPSEPREV;
      break;

   case PRROF_CERAMICLAMPSTAND2:
      apPoint[0].p[0] = 1;
      apPoint[0].p[1] = .1;
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[1] = .2;         // SEGCURVE_CUBIC
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[0] = .5;
      apPoint[2].p[1] = .45;         // SEGCURVE_CUBIC
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = .7;         // SEGCURVE_CUBIC
      apPoint[4].Copy (&apPoint[3]);
      apPoint[4].p[0] = .5;
      apPoint[4].p[1] = .95;         // SEGCURVE_CUBIC
      apPoint[5].Copy (&apPoint[4]);
      apPoint[5].p[1] = 1;
      apPoint[6].Copy (&apPoint[5]);
      apPoint[6].p[0] = 0;
      dwNum = 7;
      adwCurve[1] = SEGCURVE_CUBIC;
      adwCurve[2] = SEGCURVE_CUBIC;
      adwCurve[3] = SEGCURVE_CUBIC;
      adwCurve[4] = SEGCURVE_CUBIC;
      break;

   case PRROF_CERAMICLAMPSTAND3:
      apPoint[0].p[0] = .5;
      apPoint[1].p[0] = 1;
      apPoint[2].p[0] = 1;
      apPoint[2].p[1] = .4;
      apPoint[3].p[0] = 1;
      apPoint[3].p[1] = .8;
      apPoint[4].p[0] = 1.0 / 3.0;
      apPoint[4].p[1] = .8;
      apPoint[5].Copy (&apPoint[4]);
      apPoint[5].p[1] = 1;
      apPoint[6].Copy (&apPoint[5]);
      apPoint[6].p[0] = 0;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      adwCurve[2] = SEGCURVE_ELLIPSENEXT;
      adwCurve[3] = SEGCURVE_ELLIPSEPREV;
      dwNum = 7;
      break;
   }
   if (dwNum < 2)
      return FALSE;

   CSpline s;
   if (!s.Init (FALSE, dwNum, apPoint, NULL, adwCurve, dwDetail, dwDetail))
      return FALSE;

   if (!ProfileSet (&s, aiNormTip[0], aiNormTip[1]))
      return FALSE;

   m_dwProfileType = dwType;
   m_dwProfileDetail = dwDetail;
   return TRUE;
}

/************************************************************************************
CRevolution::ProfileGet - Returns the profile type, and if detail level.

inputs
   DWORD    *pdwDetail - Filled with the detail level. Can be null
returns
   DWORD - Profile type, RPROF_XXX
*/
DWORD CRevolution::ProfileGet (DWORD *pdwDetail)
{
   if (pdwDetail)
      *pdwDetail = m_dwProfileDetail;
   return m_dwProfileType;
}


/************************************************************************************
CRevolution::ProfileGetSpline - Returns the profile spline, or NULL if
there isn't one. If the profile m_dwProfileDetail is custom, this will still
return a profile spline.
   NOTE: Do not change this spline. You can use it for reference though.

inputs
   none
returns
   PCSpline - Profile spline
*/
PCSpline CRevolution::ProfileGetSpline (void)
{
   return m_psProfile;
}


/************************************************************************************
CRevolution::RevolutionSet - Sets the spline that describes the Revolution revolution.
Some notes:
   - Only X and Y should be set. Z should be set to 0.
   - In genreal, center at 0,0
   - X and Y range from -1 to 1 so scale works properly
   - X and Y go counterclockwise when lookin down
   - Rotation will be around the Z axis

inputs
   PCSpline    pSpline - This spline is cloned and used
returns
   BOOL - TRUE if succes
*/
BOOL CRevolution::RevolutionSet (PCSpline pSpline)
{
   if (!m_psRevolution)
      m_psRevolution = new CSpline;
   if (!m_psRevolution)
      return FALSE;
   if (!pSpline->CloneTo (m_psRevolution))
      return FALSE;

   m_dwRevolutionType = RREV_CUSTOM;
   m_fDirty = TRUE;
   return TRUE;
}

/************************************************************************************
CRevolution::RevolutionSet - Sets the Revolution using one of the built-in Revolutions.

inputs
   DWORD       dwType - One of RREV_XXX, except for RREV_CUSTOM
   DWORD       dwDetail - Passed to CSpline init for the number of divisions
returns
   BOOL - TRUE if succes
*/
BOOL CRevolution::RevolutionSet (DWORD dwType, DWORD dwDetail)
{
   DWORD dwNum;
   DWORD adwCurve[MAXS];
   CPoint apPoint[MAXS];
   fp fIncPerTime = PI/2;
   DWORD dwRepeatTimes = 4;   // repeat this four times
   memset (adwCurve, 0, sizeof(adwCurve));
   memset (apPoint, 0, sizeof(apPoint));
   dwNum = 0;

   switch (dwType) {
   case RREV_SQUARE:
      // BUGFIX - So align with XY axis
      apPoint[0].p[0] = apPoint[0].p[1] = 1;
      dwNum = 1;
      break;

   case RREV_TRIANGLE:
      dwRepeatTimes = 3;
      fIncPerTime = 2 * PI / (fp)dwRepeatTimes;
      apPoint[0].p[0] = 1;
      dwNum = 1;
      break;

   case RREV_PENTAGON:
      dwRepeatTimes = 5;
      fIncPerTime = 2 * PI / (fp)dwRepeatTimes;
      apPoint[0].p[0] = 1;
      dwNum = 1;
      break;

   case RREV_HEXAGON:
      dwRepeatTimes = 6;
      fIncPerTime = 2 * PI / (fp)dwRepeatTimes;
      apPoint[0].p[0] = 1;
      dwNum = 1;
      break;

   case RREV_OCTAGON:
      dwRepeatTimes = 8;
      fIncPerTime = 2 * PI / (fp)dwRepeatTimes;
      apPoint[0].p[0] = 1;
      dwNum = 1;
      break;

   case RREV_CIRCLE:
      apPoint[0].p[0] = 1;
      apPoint[1].p[0] = 1;
      apPoint[1].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      dwNum = 2;
      break;

   case RREV_CIRCLEFAN:
      dwRepeatTimes = 16;
      fIncPerTime = 2 * PI / (fp)dwRepeatTimes;
      apPoint[0].p[0] = 1.05;
      apPoint[1].p[0] = .95 * cos(fIncPerTime/2);
      apPoint[1].p[1] = .95 * sin(fIncPerTime/2);
      dwNum = 2;
      break;

   case RREV_SQUAREHALF:
      dwRepeatTimes = 1;
      dwNum = 4;
      apPoint[0].p[0] = 1;
      apPoint[1].Copy (&apPoint[0]);
      apPoint[1].p[1] = 1;
      apPoint[2].Copy (&apPoint[1]);
      apPoint[2].p[0] = -1;
      apPoint[3].Copy (&apPoint[2]);
      apPoint[3].p[1] = 0;
      break;

   case RREV_CIRCLEHEMI:
      apPoint[0].p[0] = 1;
      apPoint[1].p[0] = 1;
      apPoint[1].p[1] = 1;
      adwCurve[0] = SEGCURVE_ELLIPSENEXT;
      adwCurve[1] = SEGCURVE_ELLIPSEPREV;
      dwNum = 2;
      dwRepeatTimes = 2;
      break;

   }
   if (dwNum < 1)
      return FALSE;

   // set w to 1
   DWORD i, j;
   for (i = 0; i < dwNum; i++)
      apPoint[i].p[3] = 1;

   // repeat
   for (i = 1; i < dwRepeatTimes; i++) {
      CMatrix m;
      m.RotationZ (fIncPerTime * (fp)i);

      // copy and rotate
      for (j = 0; j < dwNum; j++) {
         adwCurve[i*dwNum + j] = adwCurve[j];
         m.Multiply (&apPoint[j], &apPoint[i*dwNum+j]);
      }
   }

   CSpline s;
   BOOL fLooped;
   fLooped = (((fp) dwRepeatTimes * fIncPerTime + CLOSE) >= 2 * PI);
   DWORD dwPoints;
   dwPoints = dwNum * dwRepeatTimes;
   if (!fLooped && (dwRepeatTimes > 1)) {
      // one extra one
      CMatrix m;
      m.RotationZ (fIncPerTime * (fp)dwRepeatTimes);
      adwCurve[dwPoints] = adwCurve[0];
      m.Multiply (&apPoint[0], &apPoint[dwPoints]);
      dwPoints++;
   }
   if (!s.Init (fLooped, dwPoints, apPoint, NULL, adwCurve, dwDetail, dwDetail))
      return FALSE;

   if (!RevolutionSet (&s))
      return FALSE;

   m_dwRevolutionType = dwType;
   m_dwRevolutionDetail = dwDetail;
   return TRUE;
}

/************************************************************************************
CRevolution::RevolutionGet - Returns the Revolution type, and if detail level.

inputs
   DWORD    *pdwDetail - Filled with the detail level. Can be null
returns
   DWORD - Revolution type, RPROF_XXX
*/
DWORD CRevolution::RevolutionGet (DWORD *pdwDetail)
{
   if (pdwDetail)
      *pdwDetail = m_dwRevolutionDetail;
   return m_dwRevolutionType;
}


/************************************************************************************
CRevolution::RevolutionGetSpline - Returns the revolution spline, or NULL if
there isn't one. If the profile m_dwRevolutionDetail is custom, this will still
return a revolution spline.
   NOTE: Do not change this spline. You can use it for reference though.

inputs
   none
returns
   PCSpline - Profile spline
*/
PCSpline CRevolution::RevolutionGetSpline (void)
{
   return m_psRevolution;
}

/************************************************************************************
CRevolution::ScaleSet - Sets the width and height of the rotation.

inputs
   PCPoint     pScale - When looking in the direction of pAround (DirectionSet())...
                        p[0] = width from left to right
                        p[1] = width from top to bottom
                        p[2] = height from near to far - since rotating around p[2],
                              this is the height of the rotation.
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::ScaleSet (PCPoint pScale)
{
   m_pScale.Copy (pScale);
   m_pScale.p[0] = max(CLOSE, m_pScale.p[0]);
   m_pScale.p[1] = max(CLOSE, m_pScale.p[1]);
   m_pScale.p[2] = max(CLOSE, m_pScale.p[2]);
   m_fDirty = TRUE;

   return TRUE;
}

/************************************************************************************
CRevolution::ScaleGet - Regurgitates the information passed into ScaleSet

inputs
   PCPoint     pScale - Filled with scale inforamtion
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::ScaleGet (PCPoint pScale)
{
   pScale->Copy (&m_pScale);
   return TRUE;
}

/************************************************************************************
CRevolution::DirectionSet - Sets the direction that the rotation will happen around
and where.

inputs
   PCPoint     pBottom - Location (in local coords) of the bottom of the revolved object
   PCPoint     pAround - Vector (doesn't need to be normalized) pointing from pBottom
                     in a direction. Revolution will occur around pAround
   PCPoint     pLeft - When at pBottom, looking towards pAround pLeft defines the
                     left vector. In terms of RevolutuonSet(), this is the x=1 vector
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::DirectionSet (PCPoint pBottom, PCPoint pAround, PCPoint pLeft)
{
   m_pBottom.Copy (pBottom);
   m_pAroundVec.Copy (pAround);
   m_pLeftVec.Copy (pLeft);
   m_fDirty = TRUE;
   return TRUE;
}

/************************************************************************************
CRevolution::DirectionGet - Gets the direction that the rotation will happen around
and where.

inputs
   PCPoint     pBottom - Filled with Location (in local coords) of the bottom of the revolved object
   PCPoint     pAround - Filled with Vector (doesn't need to be normalized) pointing from pBottom
                     in a direction. Revolution will occur around pAround
   PCPoint     pLeft - Filled with When at pBottom, looking towards pAround pLeft defines the
                     left vector. In terms of RevolutuonSet(), this is the x=1 vector
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::DirectionGet (PCPoint pBottom, PCPoint pAround, PCPoint pLeft)
{
   if (pBottom)
      pBottom->Copy (&m_pBottom);
   if (pAround)
      pAround->Copy (&m_pAroundVec);
   if (pLeft)
      pLeft->Copy (&m_pLeftVec);
   return TRUE;
}

/************************************************************************************
CRevolution::TextRepeatSet - Sets whether or not a texture should be repeated an
integer number of times around in H and V. Use this to make sure that tiled textures
line up exactly.

inputs
   DWORD       dwRepeatH - 0 if not repeat, else positive number for the number of times repeat
   DWORD       dwRepeatV - Like dwRpeatH
returns
   none
*/
void CRevolution::TextRepeatSet (DWORD dwRepeatH, DWORD dwRepeatV)
{
   m_adwTextRepeat[0] = dwRepeatH;
   m_adwTextRepeat[1] = dwRepeatV;
}

/************************************************************************************
CRevolution::TextRepeatGet - Returns information passed to TextRepeatSet

inputs
   DWORD       *pdwRepeatH - Filled with repeat count
   DWORD       *pdwRepeatV - Like dwRpeatH
returns
   none
*/
void CRevolution::TextRepeatGet (DWORD *pdwRepeatH, DWORD *pdwRepeatV)\
{
   if (pdwRepeatH)
      *pdwRepeatH = m_adwTextRepeat[0];
   if (pdwRepeatV)
      *pdwRepeatV = m_adwTextRepeat[1];
}

/************************************************************************************
CRevolution::BackfaceCullSet - Turns backface culling on/off for the surface.

inputs
   BOOL        fBackface - if TRUE then backface cull this
returns
   BOOL - TRUe if success
*/
BOOL CRevolution::BackfaceCullSet (BOOL fBackface)
{
   m_fBackface = fBackface;
   return TRUE;
}

/************************************************************************************
CRevolution::BackfaceCullGet - Retursn the state of backface culling, TRUE if
backface culling is done
*/
BOOL CRevolution::BackfaceCullGet (void)
{
   return m_fBackface;
}

/************************************************************************************
CRevolution::Render - Draws the objects

inputs
   POBJECTRENDER     pr - Render info
   PCRenderSurface   prs - Render to
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::Render (POBJECTRENDER pr, PCRenderSurface prs)
{
   if (!CalcSurfaceIfDirty())
      return FALSE;

   // push the matrix
   prs->Push();
   prs->Multiply (&m_mMatrix);

   // allocate for all the points
   PCPoint pPoints;
   DWORD dwPoints;
   pPoints = prs->NewPoints (&dwPoints, m_dwX * m_dwY);
   if (!pPoints) {
      prs->Pop();
      return TRUE;
   }

   // copy over
   memcpy (pPoints, m_memPoints.p, m_dwX * m_dwY * sizeof(CPoint));

   // spline
   CPoint pAroundInv;
   CPoint pAround;
   PCPoint pNormStart = NULL, pNormEnd = NULL;
   if (m_aiNormTip[0] || m_aiNormTip[1]) {
      //CMatrix mInv;
      //m_mMatrix.Invert (&mInv);  // only need to inverse 3x3 part
      //mInv.Transpose ();

      //pAround.Copy (&m_pAroundVec);
      //pAround.Normalize();
      //pAround.MultiplyLeft (&mInv);
      pAround.Zero();
      pAround.p[2] = 1; // know this since applied in m_mMatrix

      if ((m_aiNormTip[0] < 0) || (m_aiNormTip[1] < 0)) {
         pAroundInv.Copy (&pAround);
         pAroundInv.Scale (-1);
      }

      if (m_aiNormTip[0] > 0)
         pNormStart = &pAround;
      else if (m_aiNormTip[0] < 0)
         pNormStart = &pAroundInv;
      if (m_aiNormTip[1] > 0)
         pNormEnd = &pAround;
      else if (m_aiNormTip[1] < 0)
         pNormEnd = &pAroundInv;
   }

   prs->ShapeSurface ( (m_psRevolution->LoopedGet() ? 1 : 0) | (m_psProfile->LoopedGet() ? 2 : 0),
      m_dwX, m_dwY,
      pPoints, dwPoints, NULL, 0, m_fBackface,
      pNormEnd, pNormStart, NULL, NULL,
      m_adwTextRepeat[0], m_adwTextRepeat[1]);

   // undo the changes
   prs->Pop();
   return TRUE;
}


/**********************************************************************************
CRevolution::QueryBoundingBox - Standard API
*/
void CRevolution::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   if (!CalcSurfaceIfDirty()) {
      pCorner1->Zero();
      pCorner2->Zero();
      return;
   }

   PCPoint p = (PCPoint)m_memPoints.p;
   pCorner1->Copy (p);
   pCorner2->Copy (p);
   DWORD i;
   for (i = 1, p++; i < m_dwX * m_dwY; i++, p++) {
      pCorner1->Min(p);
      pCorner2->Max(p);
   } // i

   BoundingBoxApplyMatrix (pCorner1, pCorner2, &m_mMatrix);
}

static PWSTR gpszRevolution = L"Revolution";
static PWSTR gpszProfile = L"Profile";
static PWSTR gpszProfileType = L"ProfileType";
static PWSTR gpszProfileDetail = L"ProfileDetail";
static PWSTR gpszRevolutionType = L"RevolutionType";
static PWSTR gpszRevolutionDetail = L"RevolutionDetail";
static PWSTR gpszBackface = L"Backface";
static PWSTR gpszScale = L"Scale";
static PWSTR gpszBottom = L"Bottom";
static PWSTR gpszAroundVec = L"AroundVec";
static PWSTR gpszLeftVec = L"LeftVec";
static PWSTR gpszNormTip0 = L"NormTip0";
static PWSTR gpszNormTip1 = L"NormTip1";
static PWSTR gpszTextRepeat0 = L"TextRepeat0";
static PWSTR gpszTextRepeat1 = L"TextRepeat1";

/************************************************************************************
CRevolution::MMLTo - Writes the revolution information to MML

inputs
   noen
retursn
   PCMMLNode2 - MML
*/
PCMMLNode2 CRevolution::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszRevolution);

   PCMMLNode2 pSub;
   if ((m_dwProfileType == RPROF_CUSTOM) && m_psProfile) {
      pSub = m_psProfile->MMLTo();
      if (pSub) {
         pSub->NameSet (gpszProfile);
         pNode->ContentAdd (pSub);
      }
   }
   if ((m_dwRevolutionType == RREV_CUSTOM) && m_psRevolution) {
      pSub = m_psRevolution->MMLTo();
      if (pSub) {
         pSub->NameSet (gpszRevolution);
         pNode->ContentAdd (pSub);
      }
   }

   MMLValueSet (pNode, gpszProfileType, (int)m_dwProfileType);
   MMLValueSet (pNode, gpszProfileDetail, (int)m_dwProfileDetail);
   MMLValueSet (pNode, gpszRevolutionType, (int)m_dwRevolutionType);
   MMLValueSet (pNode, gpszRevolutionDetail, (int)m_dwRevolutionDetail);
   MMLValueSet (pNode, gpszBackface, (int)m_fBackface);
   MMLValueSet (pNode, gpszScale, &m_pScale);
   MMLValueSet (pNode, gpszBottom, &m_pBottom);
   MMLValueSet (pNode, gpszAroundVec, &m_pAroundVec);
   MMLValueSet (pNode, gpszLeftVec, &m_pLeftVec);
   if (m_aiNormTip[0])
      MMLValueSet (pNode, gpszNormTip0, m_aiNormTip[0]);
   if (m_aiNormTip[1])
      MMLValueSet (pNode, gpszNormTip1, m_aiNormTip[1]);
   if (m_adwTextRepeat[0])
      MMLValueSet (pNode, gpszTextRepeat0, (int)m_adwTextRepeat[0]);
   if (m_adwTextRepeat[1])
      MMLValueSet (pNode, gpszTextRepeat1, (int)m_adwTextRepeat[1]);
   return pNode;
}

/************************************************************************************
CRevolution::MMLFrom - Reads a revolution object in from MML

inputs
   PCMMLNode2      pNode - To read from
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::MMLFrom (PCMMLNode2 pNode)
{
   if (m_psRevolution)
      delete m_psRevolution;
   m_psRevolution = NULL;
   if (m_psProfile)  // BUGFIX - Had repeated revolution twice
      delete m_psProfile;
   m_psProfile = NULL;

   m_fDirty = TRUE;
   m_dwProfileType = (DWORD) MMLValueGetInt (pNode, gpszProfileType, RPROF_CIRCLE);
   m_dwProfileDetail = (DWORD) MMLValueGetInt (pNode, gpszProfileDetail, 1);
   m_dwRevolutionType = (DWORD) MMLValueGetInt (pNode, gpszRevolutionType, RREV_CIRCLE);
   m_dwRevolutionDetail = (DWORD) MMLValueGetInt (pNode, gpszRevolutionDetail, 1);
   m_fBackface = (BOOL) MMLValueGetInt (pNode, gpszBackface, TRUE);
   MMLValueGetPoint (pNode, gpszScale, &m_pScale);
   MMLValueGetPoint (pNode, gpszBottom, &m_pBottom);
   MMLValueGetPoint (pNode, gpszAroundVec, &m_pAroundVec);
   MMLValueGetPoint (pNode, gpszLeftVec, &m_pLeftVec);

   m_aiNormTip[0] = MMLValueGetInt (pNode, gpszNormTip0, 0);
   m_aiNormTip[1] = MMLValueGetInt (pNode, gpszNormTip1, 0);
   m_adwTextRepeat[0] = (DWORD) MMLValueGetInt (pNode, gpszTextRepeat0, (int)0);
   m_adwTextRepeat[1] = (DWORD) MMLValueGetInt (pNode, gpszTextRepeat1, (int)0);

   if (m_dwProfileType) {
      if (!ProfileSet (m_dwProfileType, m_dwProfileDetail))
         return FALSE;
   }
   else {
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(pNode->ContentFind (gpszProfile), &psz, &pSub);
      if (pSub) {
         m_psProfile = new CSpline;
         if (m_psProfile)
            m_psProfile->MMLFrom (pSub);
      }
   }

   if (m_dwRevolutionType) {
      if (!RevolutionSet (m_dwRevolutionType, m_dwRevolutionDetail))
         return FALSE;
   }
   else {
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum(pNode->ContentFind (gpszRevolution), &psz, &pSub);
      if (pSub) {
         m_psRevolution = new CSpline;
         if (m_psRevolution)
            m_psRevolution->MMLFrom (pSub);
      }
   }

   return TRUE;
}

/************************************************************************************
CRevolution::Clone - Clones this revolution to a new one.

inputs
   none
retusn
   PCRevolution - new revolution
*/
CRevolution *CRevolution::Clone (void)
{
   PCRevolution pNew = new CRevolution;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/************************************************************************************
CRevolution::CloneTo - Overwrites the infor in pTo with what have here

inputs
   PCRevolution      pTo - Overwrite this
returns
   BOOL - TRUE if success
*/
BOOL CRevolution::CloneTo (CRevolution *pTo)
{
   if (m_psProfile) {
      if (!pTo->m_psProfile)
         pTo->m_psProfile = new CSpline;
      if (!pTo->m_psProfile)
         return FALSE;
      if (!m_psProfile->CloneTo (pTo->m_psProfile))
         return FALSE;
   }
   else {
      if (m_psProfile)
         delete m_psProfile;
      m_psProfile = NULL;
   }

   if (m_psRevolution) {
      if (!pTo->m_psRevolution)
         pTo->m_psRevolution = new CSpline;
      if (!pTo->m_psRevolution)
         return FALSE;
      if (!m_psRevolution->CloneTo (pTo->m_psRevolution))
         return FALSE;
   }
   else {
      if (m_psRevolution)
         delete m_psRevolution;
      m_psRevolution = NULL;
   }

   pTo->m_dwProfileType = m_dwProfileType;
   pTo->m_dwProfileDetail = m_dwProfileDetail;
   pTo->m_dwRevolutionType = m_dwRevolutionType;
   pTo->m_dwRevolutionDetail = m_dwRevolutionDetail;
   pTo->m_pScale.Copy (&m_pScale);
   pTo->m_pBottom.Copy (&m_pBottom);
   pTo->m_pAroundVec.Copy (&m_pAroundVec);
   pTo->m_pLeftVec.Copy (&m_pLeftVec);
   pTo->m_fBackface = m_fBackface;
   pTo->m_fDirty = m_fDirty;
   memcpy (pTo->m_aiNormTip, m_aiNormTip, sizeof(m_aiNormTip));
   memcpy (pTo->m_adwTextRepeat, m_adwTextRepeat, sizeof(m_adwTextRepeat));
   pTo->m_mMatrix.Copy (&m_mMatrix);
   if (!pTo->m_memPoints.Required (m_dwX * m_dwY * sizeof(CPoint)))
      return FALSE;
   memcpy (pTo->m_memPoints.p, m_memPoints.p, min(m_dwX * m_dwY * sizeof(CPoint), m_memPoints.m_dwAllocated));
   pTo->m_dwX = m_dwX;
   pTo->m_dwY = m_dwY;

   return TRUE;
}


/************************************************************************************
CRevolution::CalcSurfaceIfDirty - If the surface is dirty this recalculates it
*/
BOOL CRevolution::CalcSurfaceIfDirty (void)
{
   if (!m_fDirty)
      return TRUE;
   if (!m_psProfile || !m_psRevolution)
      return FALSE;

	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;

   // make two lists of points, [0] for the profile, and [1] for the revolution.
   CListFixed al[2];
   DWORD dwList;
   DWORD i, k;
   PCSpline ps;
   for (dwList = 0; dwList < 2; dwList++) {
      al[dwList].Init (sizeof(CPoint));
      ps = (dwList ? m_psRevolution : m_psProfile);

      // divide by how much
      DWORD dwOrigNum, dwNewNum, dwDivide;
      DWORD fLooped;
      fLooped = (ps->LoopedGet() ? 1 : 0);
      dwOrigNum = ps->OrigNumPointsGet();
      dwNewNum = ps->QueryNodes();
      dwDivide = (dwNewNum - (1-fLooped)) / (dwOrigNum - (1-fLooped));

      // loop through all these
      for (i = 0; i < dwOrigNum; i++) {
         // is this linear
         DWORD dwCurve = SEGCURVE_CIRCLENEXT, dwCurveNext = SEGCURVE_CIRCLENEXT;
         BOOL fLinear, fLinearNext;
         if (i < dwOrigNum - (1 - fLooped))
            ps->OrigSegCurveGet (i, &dwCurve);
         if ((i+1 < dwOrigNum - (1-fLooped)) || fLooped)
            ps->OrigSegCurveGet ((i+1)%dwOrigNum, &dwCurveNext);
         fLinear = (dwCurve == SEGCURVE_LINEAR) ? TRUE : FALSE;
         fLinearNext = (dwCurveNext == SEGCURVE_LINEAR) ? TRUE : FALSE;

         // if linear, only add one point
         if (fLinear) {
            // add this point
            CPoint pGet;
            pGet.Copy (ps->LocationGet (i * dwDivide));
            al[dwList].Add (&pGet);

            // if the next segment is linear also then insert a small buffer point
            // right next to the next so tht the normals will be correct at either
            // end
            if (fLinearNext) {
               CPoint pGet2;
               pGet2.Copy (ps->LocationGet (((i+1)*dwDivide) % dwNewNum));
               pGet.Average (&pGet2, .99);
               al[dwList].Add (&pGet);
            }
         }
         else {
            // loop over all the divide and add them
            for (k = 0; k < dwDivide; k++) {
               DWORD dwGet = (i * dwDivide) + k;
               if (dwGet >= dwNewNum)
                  continue;   // if not looped, then only get one point for the last one

               al[dwList].Add (ps->LocationGet (dwGet));
            }
         }

      }  // over dwOrigNum

   }  // over dwList

   // BUGFIX - Remove this since have code in the ShapeSurface call
   // to get rid of 0's
#if 0
   // make sure no points exactly at 0 in the profile
   PCPoint pAdd;
   DWORD dwNum;
   pAdd = (PCPoint)al[0].Get(0);
   dwNum = al[0].Num();
   for (i = 0; i < dwNum; i++, pAdd++) {
      pAdd->p[0] = max(pAdd->p[0], .001);  // so never 0
   }
#endif // 0

   // how much do we need?
   m_dwX = al[1].Num();
   m_dwY = al[0].Num();

   // allocate enough space
   if (!m_memPoints.Required (m_dwX * m_dwY * sizeof(CPoint))) {
   	MALLOCOPT_RESTORE;
      return FALSE;
   }

   // fill it in
   PCPoint paPoints;
   paPoints = (PCPoint) m_memPoints.p;

   DWORD x, y;
   for (y = 0; y < m_dwY; y++) {
      PCPoint pProf = (PCPoint) al[0].Get(m_dwY - y - 1);
      PCPoint pFill = paPoints + (y * m_dwX);
      PCPoint pRev = (PCPoint) al[1].Get(0);

      for (x = 0; x < m_dwX; x++, pFill++, pRev++) {
         pFill->p[0] = pRev->p[0] * pProf->p[0] * m_pScale.p[0] / 2.0;
         pFill->p[1] = pRev->p[1] * pProf->p[0] * m_pScale.p[1] / 2.0;
         pFill->p[2] = pProf->p[1] * m_pScale.p[2];
         pFill->p[3] = 1;
      }
   }

   // fill in the matrix
   CPoint pA, pB, pC;
   CMatrix mTrans;
   mTrans.Translation (m_pBottom.p[0], m_pBottom.p[1], m_pBottom.p[2]);
   pC.Copy (&m_pAroundVec);
   pC.Normalize();
   pA.Copy (&m_pLeftVec);
   pA.Normalize();
   pB.CrossProd (&pC, &pA);
   pB.Normalize();
   pA.CrossProd (&pB, &pC);
   pA.Normalize();
   m_mMatrix.RotationFromVectors (&pA, &pB, &pC);
   m_mMatrix.MultiplyRight (&mTrans);

	MALLOCOPT_RESTORE;

   m_fDirty = FALSE;
   return TRUE;
}

// BUGBUG - Central normal in the disc doesn't seem to be correct when do revolution
// Probably happened when tried to fix for tirangle
