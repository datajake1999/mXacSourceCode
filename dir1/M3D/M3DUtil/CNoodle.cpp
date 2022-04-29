/**********************************************************************************
CNoodle.cpp - Draw extrusions

begun 22/2/2002 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



/***************************************************************************************
CNoodle::Constructor and destructor
*/
CNoodle::CNoodle (void)
{
   m_psPath = NULL;
   m_psFront = NULL;
   m_pFront.Zero();
   m_pFront.p[1] = -1;
   m_psScale = NULL;
   m_pScale.Zero();
   m_pScale.p[0] = m_pScale.p[1] = 1;
   m_dwShape = 0;
   m_psShape = NULL;
   m_pssShape = NULL;
   m_pOffset.Zero();
   m_apBevel[0].Zero();
   m_apBevel[0].p[2] = 1;
   m_apBevel[1].Zero();
   m_apBevel[1].p[2] = 1;
   memset (m_adwBevelMode, 0, sizeof(m_adwBevelMode));
   m_fDrawEnds = FALSE;
   m_dwTextureWrap = m_dwTextureWrapVert = 0;
   m_fBackfaceCull = TRUE;
   m_mMatrix.Identity();
   m_fDirty = TRUE;
   m_fLength = m_fCircum = 0;
}

CNoodle::~CNoodle (void)
{
   if (m_psPath)
      delete m_psPath;
   if (m_psFront)
      delete m_psFront;
   if (m_psScale)
      delete m_psScale;
   if (m_psShape)
      delete m_psShape;
   if (m_pssShape)
      delete m_pssShape;
}


/***************************************************************************************
CNoodle::PathSpline - Sets the path for the noodle, using a spline. Either
PathSpline() or PathLinear() must be called.

inputs
   BOOL     fLooped - TRUE if the noodle's ends are connected, FALSE if not
   DWORD    dwPoints - # of points
   PCPoint  paPoints - Point for path of the noodle
   DWORD    *padwSegCurve - dwPoints curves (or dwPoints-1 if !fLooped). Or, can
            just be (DWORD*)SEGCURVE_XXX
   DWORD    dwDivide - Number of times to divide curved sections. See CSpline::Init()
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::PathSpline (BOOL fLooped, DWORD dwPoints, PCPoint paPoints,
   DWORD *padwSegCurve, DWORD dwDivide)
{
   m_fDirty = TRUE;
   if (!m_psPath)
      m_psPath = new CSpline;
   if (!m_psPath)
      return FALSE;

   // because some noodles will end up with duplicate points in the spline (coming
   // from the staircase for example), and these wreak havock on the noodles (which
   // wont draw), eliminate duplicate points

   // test to see if duplicates
   DWORD j;
   for (j = 0; j+1 < dwPoints; j++) {
      if (paPoints[j].AreClose(&paPoints[j+1]))
         break;
   }
   CMem memPoints, memCurve;
   if (j < dwPoints) {
      // duplicates
      PCPoint pNew;
      PDWORD pdwNew;
      if (!memPoints.Required(dwPoints * sizeof(CPoint))) {
         delete m_psPath;
         m_psPath = NULL;
         return NULL;
      }
      else {
         pNew = (PCPoint) memPoints.p;
         memcpy (pNew, paPoints, dwPoints * sizeof(CPoint));
         paPoints = pNew;
      }

      if (((DWORD)(size_t)padwSegCurve < 1000) || !memCurve.Required(dwPoints * sizeof(CPoint)))
         pdwNew = NULL;
      else {
         pdwNew = (DWORD*) memCurve.p;
         memcpy (pdwNew, padwSegCurve, (dwPoints - (fLooped ? 0 : 1)) * sizeof(DWORD));
         padwSegCurve = pdwNew;
      }


      // eleiminate duplicates
      for (j = 0; j+1 < dwPoints; j++) {
         if (!paPoints[j].AreClose(&paPoints[j+1]))
            continue;

         // else, are close. Delete this one
         memmove (paPoints + j, paPoints + (j+1), (dwPoints - j - 1) * sizeof(CPoint));
         if (pdwNew)
            memmove (pdwNew + j, pdwNew + (j+1), (dwPoints - j - 1) * sizeof(DWORD));
         dwPoints--;
         j--;  // so repeat this one
      }
   }

   // if not enough points then error
   if (dwPoints < 2) {
      delete m_psPath;
      m_psPath = NULL;
      return FALSE;
   }

   // BUGFIX - Set looped for front and scale
   if (m_psFront)
      m_psFront->LoopedSet (fLooped);
   if (m_psScale)
      m_psScale->LoopedSet (fLooped);

   return m_psPath->Init (fLooped, dwPoints, paPoints, NULL, padwSegCurve,
      dwDivide, dwDivide);
}

/***************************************************************************************
CNoodle::PathLinear - Sets the path for the noodles as a straight line from one point
to another.

inputs
   PCPoint     pStart - Starting point
   PCPoint     pEnd - Finishing point
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::PathLinear (PCPoint pStart, PCPoint pEnd)
{
   CPoint aPoints[2];
   aPoints[0].Copy (pStart);
   aPoints[1].Copy (pEnd);
   return PathSpline (FALSE, 2, aPoints, (DWORD*)SEGCURVE_LINEAR, 0);
}


/***************************************************************************************
CNoodle::PathSplineGet - Returns a pointer to the noodle's path. This can be
interrogated for parameters, but should not be changed.

returns
   PCSpline - Spline.
*/
PCSpline CNoodle::PathSplineGet (void)
{
   return m_psPath;
}


/***************************************************************************************
CNoodle::FrontSpline - Sets the "front" (where vector (0,-1,0) points) of the noodle.
This is in the same coordinate space as PathSpline(), but MUST NOT ever overlap. The
vector between from the path to the front spline is used to determine the direction
of y=-1.

Either FrontSpline() or FrontVector() must be called.

inputs
   // NOTE: Looped information gotten from PathSpline
   DWORD    dwPoints - # of points
   PCPoint  paPoints - Point for front path of the noodle
   DWORD    *padwSegCurve - dwPoints curves (or dwPoints-1 if !fLooped). Or, can
            just be (DWORD*)SEGCURVE_XXX
   DWORD    dwDivide - Number of times to divide curved sections. See CSpline::Init()
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::FrontSpline (DWORD dwPoints, PCPoint paPoints,
   DWORD *padwSegCurve, DWORD dwDivide)
{
   m_fDirty = TRUE;
   if (!m_psPath)
      return FALSE;  // must have path set

   if (!m_psFront)
      m_psFront = new CSpline;
   if (!m_psFront)
      return FALSE;
   return m_psFront->Init (m_psPath->LoopedGet(), dwPoints, paPoints, NULL, padwSegCurve,
      dwDivide, dwDivide);
}


/***************************************************************************************
CNoodle::FrontVector - Sets the vector describing which way is front. The coordinates
are the same space as PathSpline() or PathLinear() - noodle space.

Either FrontSpline() or FrontVector() must be called.

inputs
   PCPoint     pFront - Vector (doesnt need to be normalized) pointing in the direction
      of the front (y=-1) of the noodle
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::FrontVector (PCPoint pFront)
{
   m_pFront.Copy (pFront);
   m_fDirty = TRUE;
   if (m_psFront)
      delete m_psFront;
   m_psFront = NULL;
   return TRUE;
}

/***************************************************************************************
CNoodle::FrontSplineGet - Returns a pointer to the noodle's front path. This can be
interrogated for parameters, but should not be changed.

returns
   PCSpline - Spline.
*/
PCSpline CNoodle::FrontSplineGet (void)
{
   return m_psFront;
}

/***************************************************************************************
CNoodle::FrontVectorGet - Fills pFront in the the vector being used for the front.

inputs
   PCPoint     pFront - Filled in with the front.
retursn
   BOOL - TRUE if using this vector, FALSE if have a FrontSplineGet();
*/
BOOL CNoodle::FrontVectorGet (PCPoint pFront)
{
   pFront->Copy (&m_pFront);
   return (m_psFront ? FALSE : TRUE);
}


/***************************************************************************************
CNoodle::ScaleSpline - Sets the scaling spline - x and y values represent width and 
depth. Z is ignored. Scaling is in meters.

Either ScaleSpline() or ScaleVector() must be called.

  NOTE: This will MODIFY paPaints so that z goes from 0 to 1 over the length. Otherwise
  curves don't work properly.

inputs
   // NOTE: Looped information gotten from PathSpline
   DWORD    dwPoints - # of points
   PCPoint  paPoints - Point for scale of the noodle
   DWORD    *padwSegCurve - dwPoints curves (or dwPoints-1 if !fLooped). Or, can
            just be (DWORD*)SEGCURVE_XXX
   DWORD    dwDivide - Number of times to divide curved sections. See CSpline::Init()
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::ScaleSpline (DWORD dwPoints, PCPoint paPoints,
   DWORD *padwSegCurve, DWORD dwDivide)
{
   m_fDirty = TRUE;
   if (!m_psPath)
      return FALSE;  // must have path set

   if (!m_psScale)
      m_psScale = new CSpline;
   if (!m_psScale)
      return FALSE;

   // BUGFIX - Fill z in from 0 to 1
   DWORD i;
   for (i = 0; i < dwPoints; i++)
      paPoints[i].p[2] = (fp) i / (fp)(dwPoints-1);

   return m_psScale->Init (m_psPath->LoopedGet(), dwPoints, paPoints, NULL, padwSegCurve,
      dwDivide, dwDivide);
}

/***************************************************************************************
CNoodle::ScaleVector - Instead of using a spline for scaling, this has constant width
and depth for scaling.

Either ScaleSpline() or ScaleVector() must be called.

inputs
   PCPoint     pScale - Vector. only x and y are used. In meters.
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::ScaleVector (PCPoint pScale)
{
   m_pScale.Copy (pScale);
   m_fDirty = TRUE;
   if (m_psScale)
      delete m_psScale;
   m_psScale = NULL;
   return TRUE;
}

/***************************************************************************************
CNoodle::ScaleSplineGet -  Returns a pointer to the noodle's scale spline. This can be
interrogated for parameters, but should not be changed.

returns
   PCSpline - Spline.
*/
PCSpline CNoodle::ScaleSplineGet (void)
{
   return m_psScale;
}

/***************************************************************************************
CNoodle::ScaleVectorGet - Fills pScale in the the vector being used for the scale.

inputs
   PCPoint     pScale - Filled in with the scale.
retursn
   BOOL - TRUE if using this vector, FALSE if have a ScaleSplineGet();
*/
BOOL CNoodle::ScaleVectorGet (PCPoint pScale)
{
   pScale->Copy (&m_pScale);
   return (m_psScale ? FALSE : TRUE);
}


/***************************************************************************************
CNoodle::ShapeDefault - Sets the shape based on one of the default shapes possible.

inputs
   DWORD    dwShape - One of NS_XXX
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::ShapeDefault (DWORD dwShape)
{
   CPoint      paPoints[8];
   DWORD i;
   memset (paPoints, 0, sizeof(paPoints));

   switch (dwShape) {
   case NS_RECTANGLE:
      paPoints[0].p[0] = -0.5;
      paPoints[0].p[1] = .5;
      paPoints[1].p[0] = .5;
      paPoints[1].p[1] = .5;
      paPoints[2].p[0] = .5;
      paPoints[2].p[1] = -.5;
      paPoints[3].p[0] = -.5;
      paPoints[3].p[1] = -.5;
      if (!ShapeSpline (TRUE, 4, paPoints, (DWORD*)SEGCURVE_LINEAR, 0))
         return FALSE;
      break;

   case NS_CBEAMFRONT:
   case NS_CBEAMBACK:
   case NS_CBEAMRIGHT:
   case NS_CBEAMLEFT:
      // for cbeam right
      paPoints[0].p[0] = .5;
      paPoints[0].p[1] = -.5;
      paPoints[1].p[0] = -.5;
      paPoints[1].p[1] = -.5;
      paPoints[2].p[0] = -.5;
      paPoints[2].p[1] = .5;
      paPoints[3].p[0] = .5;
      paPoints[3].p[1] = .5;

      // flip sign of X
      if ((dwShape == NS_CBEAMLEFT) || (dwShape == NS_CBEAMFRONT))
         for (i = 0; i < 4; i++)
            paPoints[i].p[0] *= -1;

      // flip direction
      if ((dwShape == NS_CBEAMLEFT) || (dwShape == NS_CBEAMBACK))
         for (i = 0; i < 2; i++) {
            CPoint p;
            p.Copy (&paPoints[i]);
            paPoints[i].Copy (&paPoints[3-i]);
            paPoints[3-i].Copy (&p);
         };

      // swap x and y
      if ((dwShape == NS_CBEAMFRONT) || (dwShape == NS_CBEAMBACK))
         for (i = 0; i < 4; i++) {
            fp f;
            f = paPoints[i].p[0];
            paPoints[i].p[0] = paPoints[i].p[1];
            paPoints[i].p[1] = f;
         };

      if (!ShapeSpline (FALSE, 4, paPoints, (DWORD*)SEGCURVE_LINEAR, 0))
         return FALSE;
      break;

   case NS_ZBEAMFRONT:
   case NS_ZBEAMBACK:
   case NS_ZBEAMRIGHT:
   case NS_ZBEAMLEFT:
      // for ZBEAM right
      paPoints[0].p[0] = -.5;
      paPoints[0].p[1] = -.5;
      paPoints[1].p[0] = 0;
      paPoints[1].p[1] = -.5;
      paPoints[2].p[0] = 0;
      paPoints[2].p[1] = .5;
      paPoints[3].p[0] = .5;
      paPoints[3].p[1] = .5;

      // flip sign of X
      if ((dwShape == NS_ZBEAMLEFT) || (dwShape == NS_ZBEAMFRONT))
         for (i = 0; i < 4; i++)
            paPoints[i].p[0] *= -1;

      // flip direction
      if ((dwShape == NS_ZBEAMLEFT) || (dwShape == NS_ZBEAMBACK))
         for (i = 0; i < 2; i++) {
            CPoint p;
            p.Copy (&paPoints[i]);
            paPoints[i].Copy (&paPoints[3-i]);
            paPoints[3-i].Copy (&p);
         };

      // swap x and y
      if ((dwShape == NS_ZBEAMFRONT) || (dwShape == NS_ZBEAMBACK))
         for (i = 0; i < 4; i++) {
            fp f;
            f = paPoints[i].p[0];
            paPoints[i].p[0] = paPoints[i].p[1];
            paPoints[i].p[1] = f;
         };

      if (!ShapeSpline (FALSE, 4, paPoints, (DWORD*)SEGCURVE_LINEAR, 0))
         return FALSE;
      break;

   case NS_IBEAMLR:
   case NS_IBEAMFB:
      // for ZBEAM right
      paPoints[0].p[0] = -.5;
      paPoints[0].p[1] = .5;
      paPoints[1].p[0] = .5;
      paPoints[1].p[1] = .5;
      paPoints[2].p[0] = 0;
      paPoints[2].p[1] = .5;
      paPoints[3].p[0] = 0;
      paPoints[3].p[1] = -.5;
      paPoints[4].p[0] = -.5;
      paPoints[4].p[1] = -.5;
      paPoints[5].p[0] = .5;
      paPoints[5].p[1] = -.5;

      // swap x and y
      if (dwShape == NS_IBEAMFB)
         for (i = 0; i < 6; i++) {
            fp f;
            f = paPoints[i].p[0];
            paPoints[i].p[0] = paPoints[i].p[1];
            paPoints[i].p[1] = f;
         };

      if (!ShapeSpline (FALSE, 6, paPoints, (DWORD*)SEGCURVE_LINEAR, 0))
         return FALSE;
      break;

   case NS_TRIANGLE:
   case NS_PENTAGON:
   case NS_HEXAGON:
      {
         DWORD dwSides;
         if (dwShape == NS_TRIANGLE)
            dwSides = 3;
         else if (dwShape == NS_PENTAGON)
            dwSides = 5;
         else
            dwSides = 6;
         for (i = 0; i < dwSides; i++) {
            fp fAngle = (fp) i / (fp) dwSides * 2.0 * PI;
            paPoints[i].p[0] = sin(fAngle) / 2.0;
            paPoints[i].p[1] = cos(fAngle) / 2.0;
         }
         if (!ShapeSpline (TRUE, dwSides, paPoints, (DWORD*)SEGCURVE_LINEAR, 0))
            return FALSE;
      }
      break;

   case NS_CIRCLE:
   case NS_CIRCLEFAST:
      paPoints[0].p[0] = 0;
      paPoints[0].p[1] = .5;
      paPoints[1].p[0] = .5;
      paPoints[1].p[1] = .5;
      paPoints[2].p[0] = .5;
      paPoints[2].p[1] = 0;
      paPoints[3].p[0] = .5;
      paPoints[3].p[1] = -.5;
      paPoints[4].p[0] = 0;
      paPoints[4].p[1] = -.5;
      paPoints[5].p[0] = -.5;
      paPoints[5].p[1] = -.5;
      paPoints[6].p[0] = -.5;
      paPoints[6].p[1] = 0;
      paPoints[7].p[0] = -0.5;
      paPoints[7].p[1] = .5;
      if (!ShapeSpline (TRUE, 8, paPoints, (DWORD*)SEGCURVE_ELLIPSENEXT, (dwShape == NS_CIRCLE) ? 2 : 0))
         return FALSE;
      break;

   case NS_CUSTOM:
   default:
      return FALSE;
   }

   // use for the standard shapes
   m_dwShape = dwShape;

   return TRUE;
}

/**************************************************************************************
CNoodle::ShapeSpline - Sets the shape spline, assuming that the shape doesnt change
over the length of the noodle. The z value is ignored. x and y should be between -.5 and
.5 so that when scale is applied the with width will be equal to the scale.

Either ShapeSpline() or ShapeDefault() or ShapeSplineSurface() must be called.

inputs
   BOOL     fLooped - If TRUE the spline is looped around on itself. FALSE it
            doesn't reconnect.
   DWORD    dwPoints - # of points
   PCPoint  paPoints - Point for scale of the noodle. Points go clockwise looking
            down on X and Y
   DWORD    *padwSegCurve - dwPoints curves (or dwPoints-1 if !fLooped). Or, can
            just be (DWORD*)SEGCURVE_XXX
   DWORD    dwDivide - Number of times to divide curved sections. See CSpline::Init()
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::ShapeSpline (BOOL fLooped, DWORD dwPoints, PCPoint paPoints,
   DWORD *padwSegCurve, DWORD dwDivide)
{
   m_fDirty = TRUE;
   if (m_pssShape)
      delete m_pssShape;
   m_pssShape = NULL;
   m_dwShape = NS_CUSTOM;
   if (!m_psShape)
      m_psShape = new CSpline;
   if (!m_psShape)
      return FALSE;

   return m_psShape->Init (fLooped, dwPoints, paPoints, NULL, padwSegCurve, dwDivide, dwDivide);
}


/**************************************************************************************
CNoodle::ShapeSplineSurface - Sets the shape of the spline, but sets it as a changing
shape over the course of the length of the spline.

inputs
   BOOL     fLoopedH - If TRUE, looped around the shape. Else, the shape does not reconnect.
   // fLoopedV - Calculated from the m_psPath->LoopedQuery()
   DWORD    dwH - Number of points in the spline around the shape
   DWORD    dwV - Number of points in the spline along the length
   PCPoint  paPoints - 2-dimensiona array of (y * dwH) + x. Leave z=0. The x and y
         points should be from -.5 to .5 so that after scaling is applied the width
         and depth of the noodle will be ScaleSpline() or ScaleVector() values
         Points along H should go in clockwise, and V from bottom to top.

         NOTE - Z value will be modified by calling this

   DWORD    *padwSegCurveH - Curve around the shape.
   DWORD    *padwSegCurveV - Curve (of the shape) along the length of the noodle.
   DWORD    dwDivideH - Divide H by this many
   DWORD    dwDivideV - Divide V by this many
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::ShapeSplineSurface (BOOL fLoopedH, DWORD dwH, DWORD dwV, PCPoint paPoints,
   DWORD *padwSegCurveH, DWORD *padwSegCurveV, DWORD dwDivideH, DWORD dwDivideV)
{
   m_fDirty = TRUE;
   if (m_psShape)
      delete m_pssShape;
   m_psShape = NULL;
   m_dwShape = NS_CUSTOM;
   if (!m_pssShape)
      m_pssShape = new CSplineSurface;
   if (!m_pssShape)
      return FALSE;

   m_pssShape->DetailSet (dwDivideH, dwDivideH, dwDivideV, dwDivideV);

   // BUGFIX - Modify Z values so can extract normals
   DWORD x, y;
   for (y = 0; y < dwV; y++) for (x = 0; x < dwH; x++)
      paPoints[x + y * dwH].p[2] = y;

   // BUGFIX - to prevent a crash
   if (!m_psPath) {
      delete m_pssShape;
      m_pssShape = NULL;
      return FALSE;
   }

   return m_pssShape->ControlPointsSet (fLoopedH, m_psPath->LoopedGet(), dwH, dwV,
      paPoints, padwSegCurveH, padwSegCurveV);
}


/**************************************************************************************
CNoodle::ShapeDefaultGet - Returns the current shape being used as default.

inputs
   DWORD    *pdwShape - Filled in with the shape
returns
   BOOL - True if using a default shape, FALSE if using a custom shape
*/
BOOL CNoodle::ShapeDefaultGet (DWORD *pdwShape)
{
   *pdwShape = m_dwShape;
   return (m_dwShape != NS_CUSTOM);
}

/**************************************************************************************
CNoodle::ShapeSplineGet - If the shape is just made out of a constant spline (and
not a spline surface that changes over the length) then this returns a pointer to
the spline object. The caller can look at the values but should not change them

returns
   PCSpline - spline. NULL if using ShapeSplineSurface()
*/
PCSpline CNoodle::ShapeSplineGet (void)
{
   return m_psShape;
}

/**************************************************************************************
CNoodle::ShapeSplineGet - If the shape is just made out of a spline surface
that changes over the length (and
not a constant spline) then this returns a pointer to
the spline object. The caller can look at the values but should not change them

returns
   PCSplineSurface - spline. NULL if using ShapeSpline()
*/
PCSplineSurface CNoodle::ShapeSplineSurfaceGet (void)
{
   return m_pssShape;
}


/**************************************************************************************
CNoodle::OffsetSet - Sets an offset that's applied to the spline's shape AFTER
it has been scaled, but BEFORE it's rotated into place.

For example: This this to create a roof bream that trackes the ceiling, but is offset
from the ceiling so that it's under it.

inputs
   PCPoint     pOffset - Only the x and y points are used. They are in meters.
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::OffsetSet (PCPoint pOffset)
{
   m_fDirty = TRUE;
   m_pOffset.Copy (pOffset);
   return TRUE;
}

/**************************************************************************************
CNoodle::OffsetGet - Gets the current value of the offset. See OffsetSet()

inputs
   PCPoint     pOffset - filled with the current value
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::OffsetGet (PCPoint pOffset)
{
   pOffset->Copy (&m_pOffset);
   return TRUE;
}

/**************************************************************************************
CNoodle::BevelSet - Sets the bevel plane, either on the start (buttom) or end (top)
of the noodle. The bevel plane is in the coordinates of the noodle's start or end...
so a (0,0,1) value is a perpendicular end, while (.5,0,.5) is an angle slice along X.

The bevel point (on the plane) is at the start/end's (0,0). (Offset included???)

inputs
   BOOL     fStart - If TRUE, using the start/bttom. FALSE the end/top.
   DWORD    dwMode - Mode to use.
                     0 => just perpendicular to direction. Ingore pBevel
                     1 => pBevel is relative to the direction of the noodle at the end
                     2 => pBevel is relative to the object coordinates (although will be modified by MatrixSet)
   PCPoint  pBevel - Value to use. NOTE: THis does NOT have to be normalized.
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::BevelSet (BOOL fStart, DWORD dwMode, PCPoint pBevel)
{
   m_fDirty = TRUE;
   if (pBevel)
      m_apBevel[fStart ? 0 : 1].Copy (pBevel);
   m_adwBevelMode[fStart ? 0 : 1] = dwMode;
   return TRUE;
}

/**************************************************************************************
CNoodle::BevelSet - Gets the bevel plane. See BevelSet().

inputs
   BOOL     fStart - If TRUE, using the start/bttom. FALSE the end/top.
   DWORD    *pdwMode - Mode to use.
                     0 => just perpendicular to direction. Ingore pBevel
                     1 => pBevel is relative to the direction of the noodle at the end
                     2 => pBevel is relative to the object coordinates (although will be modified by MatrixSet)
   PCPoint  pBevel - Value to use. NOTE: THis does NOT have to be normalized.
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::BevelGet (BOOL fStart, DWORD *pdwMode, PCPoint pBevel)
{
   if (pBevel)
      pBevel->Copy (&m_apBevel[fStart ? 0 : 1]);
   if (pdwMode)
      *pdwMode = m_adwBevelMode[fStart ? 0 : 1];
   return TRUE;
}


/**************************************************************************************
CNoodle::TextureWrapSet - Sets the texture wrap information.

inputs
   DWORD       dwWrapHorz - If 0 then the texture is wrapped around the noodle according
                  to the given measurement; seems will form. If 1+, then the texture
                  is wrapped around the noodle this many times (integer amount) so
                  no seems form. THis is good to ensure that seems dont appear in the
                  bark of trees, etc.
*/
void CNoodle::TextureWrapSet (DWORD dwWrapHorz, DWORD dwWrapVert)
{
   if ((dwWrapHorz == m_dwTextureWrap) && (dwWrapVert == m_dwTextureWrapVert))
      return;
   m_dwTextureWrap = dwWrapHorz;
   m_dwTextureWrapVert = dwWrapVert;
   m_fDirty = TRUE;
}

/**************************************************************************************
CNoodle::TextureWrapGet - Returns the texture wrap infomation. See TextureWrapSet

inputs
   BOOL        fHorz - If TRUE then return horizontal wrap, FALSE vertical
*/
DWORD CNoodle::TextureWrapGet (BOOL fHorz)
{
   return fHorz ? m_dwTextureWrap : m_dwTextureWrapVert;
}


/**************************************************************************************
CNoodle::DrawEndsSet - Sets the flag that indicates if should draw the ends of the
noodle (cap them) or just leave the noodle a tube.

inputs
   BOOL        fDraw - If TRUE will cap the ends
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::DrawEndsSet (BOOL fDraw)
{
   m_fDrawEnds = fDraw;
   m_fDirty = TRUE;
   return TRUE;
}

/**************************************************************************************
CNoodle::DrawEndsGet - Returns TRUE if the ends of the noodles are capped, false if left
open to form a tube.
*/
BOOL CNoodle::DrawEndsGet (void)
{
   return m_fDrawEnds;
}

/**************************************************************************************
CNoodle::BackfaceCullSet - Sets the flag that indicates if the noodle should be backface
culled when drawing.

inputs
   BOOL        fCull - If TRUE will allow backface culling
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::BackfaceCullSet (BOOL fCull)
{
   m_fBackfaceCull = fCull;   // BUGFIX - Was always setting backface cull to true
   // dont set dirty flag because dont need to recalculate
   return TRUE;
}

/**************************************************************************************
CNoodle::BackfaceCullGet - Returns the backface cull flag.

returns
   BOOL - TRUE if backface culling is used
*/
BOOL CNoodle::BackfaceCullGet (void)
{
   return m_fBackfaceCull;
}

/**************************************************************************************
CNoodle::MatrixSet - Sets the rotation and translation matrix used when drawing the
noodle. This is applied to the noodle's path - and hence everything else.

inputs
   PCMatrix    pm - Matrix to use
*/
BOOL CNoodle::MatrixSet (PCMatrix pm)
{
   m_mMatrix.Copy (pm);
   m_fDirty = TRUE;
   return TRUE;
}

/**************************************************************************************
CNoodle::MatrixGet - Gets the matrix.

inputs
   PCMatrix    pm - Matrix to use
*/
BOOL CNoodle::MatrixGet (PCMatrix pm)
{
   pm->Copy (&m_mMatrix);
   return TRUE;
}

/***************************************************************************************
CNoodle::CloneTo - Overwrites the pTo noodle with new information.

inputs
   PCNoodle    pTo - Overwrite this one with infomration from this
returns
   BOOL - TRUE if successful
*/
BOOL CNoodle::CloneTo (CNoodle *pTo)
{
   if (m_psPath) {
      if (!pTo->m_psPath)
         pTo->m_psPath = new CSpline;
      if (!pTo->m_psPath) {
         return FALSE;
      }
      m_psPath->CloneTo(pTo->m_psPath);
   }
   else {
      if (pTo->m_psPath)
         delete pTo->m_psPath;
      pTo->m_psPath = NULL;
   }

   if (m_psFront) {
      if (!pTo->m_psFront)
         pTo->m_psFront = new CSpline;
      if (!pTo->m_psFront) {
         return FALSE;
      }
      m_psFront->CloneTo(pTo->m_psFront);
   }
   else {
      if (pTo->m_psFront)
         delete pTo->m_psFront;
      pTo->m_psFront = NULL;
   }

   if (m_psScale) {
      if (!pTo->m_psScale)
         pTo->m_psScale = new CSpline;
      if (!pTo->m_psScale) {
         return FALSE;
      }
      m_psScale->CloneTo(pTo->m_psScale);
   }
   else {
      if (pTo->m_psScale)
         delete pTo->m_psScale;
      pTo->m_psScale = NULL;
   }

   if (m_psShape) {
      if (!pTo->m_psShape)
         pTo->m_psShape = new CSpline;
      if (!pTo->m_psShape) {
         return FALSE;
      }
      m_psShape->CloneTo(pTo->m_psShape);
   }
   else {
      if (pTo->m_psShape)
         delete pTo->m_psShape;
      pTo->m_psShape = NULL;
   }

   if (m_pssShape) {
      if (!pTo->m_pssShape)
         pTo->m_pssShape = new CSplineSurface;
      if (!pTo->m_pssShape) {
         return FALSE;
      }
      m_pssShape->CloneTo(pTo->m_pssShape);
   }
   else {
      if (pTo->m_pssShape)
         delete pTo->m_pssShape;
      pTo->m_pssShape = NULL;
   }

   pTo->m_pFront.Copy (&m_pFront);
   pTo->m_pScale.Copy (&m_pScale);
   pTo->m_pOffset.Copy (&m_pOffset);

   DWORD i;
   for (i = 0; i < 2; i++) {
      pTo->m_apBevel[i].Copy (&m_apBevel[i]);
      pTo->m_adwBevelMode[i] = m_adwBevelMode[i];
   }
   pTo->m_dwShape = m_dwShape;
   pTo->m_fDrawEnds = m_fDrawEnds;
   pTo->m_dwTextureWrap = m_dwTextureWrap;
   pTo->m_dwTextureWrapVert = m_dwTextureWrapVert;
   pTo->m_fBackfaceCull = m_fBackfaceCull;
   pTo->m_mMatrix.Copy (&m_mMatrix);
   pTo->m_fDirty = TRUE;

   return TRUE;
}


/**************************************************************************************
CNoodle::Clone - Clones the noodle.

inputs
   none
returns
   PCNoodle - New noodle
*/
CNoodle *CNoodle::Clone (void)
{
   PCNoodle pNew = new CNoodle;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return FALSE;
   }

   return pNew;
}

static PWSTR gpszNoodle = L"Noodle";
static PWSTR gpszPath = L"Path";
static PWSTR gpszFront = L"Front";
static PWSTR gpszScale = L"Scale";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszsShape = L"sShape";
static PWSTR gpszFrontPoint = L"FrontPoint";
static PWSTR gpszScalePoint = L"ScalePoint";
static PWSTR gpszOffsetPoint = L"OffsetPoint";
static PWSTR gpszBevel = L"Bevel%d";
static PWSTR gpszBevelMode = L"BevelMode%d";
static PWSTR gpszMatrix = L"Matrix";
static PWSTR gpszShapeValue = L"ShapeValue";
static PWSTR gpszDrawEnds = L"DrawEnds";
static PWSTR gpszBackfaceCull = L"BackfaceCull";
static PWSTR gpszTextureWrap = L"TextureWrap";
static PWSTR gpszTextureWrapVert = L"TextureWrapVert";

/***************************************************************************************
CNoodle::MMLTo - Writes the Noodle to MML

inputs
   none
returns
   PCMMLNode2 - node
*/
PCMMLNode2 CNoodle::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   PCMMLNode2 pSub;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoodle);

   if (m_psPath) {
      pSub = m_psPath->MMLTo();
      pSub->NameSet (gpszPath);
      pNode->ContentAdd (pSub);
   }
   if (m_psFront) {
      pSub = m_psFront->MMLTo();
      pSub->NameSet (gpszFront);
      pNode->ContentAdd (pSub);
   }
   if (m_psScale) {
      pSub = m_psScale->MMLTo();
      pSub->NameSet (gpszScale);
      pNode->ContentAdd (pSub);
   }
   if (m_psShape && (m_dwShape == NS_CUSTOM)) { // BUGFIX - Make == NS_CUSTOM
      pSub = m_psShape->MMLTo();
      pSub->NameSet (gpszShape);
      pNode->ContentAdd (pSub);
   }
   if (m_pssShape) {
      pSub = m_pssShape->MMLTo();
      pSub->NameSet (gpszsShape);
      pNode->ContentAdd (pSub);
   }

   MMLValueSet (pNode, gpszFrontPoint, &m_pFront);
   MMLValueSet (pNode, gpszScalePoint, &m_pScale);
   MMLValueSet (pNode, gpszOffsetPoint, &m_pOffset);

   DWORD i;
   for (i = 0; i < 2; i++) {
      WCHAR szTemp[32];
      swprintf (szTemp, gpszBevelMode, (int) i);
      MMLValueSet (pNode, szTemp, (int) m_adwBevelMode[i]);

      if (m_adwBevelMode[i]) {
         swprintf (szTemp, gpszBevel, (int) i);
         MMLValueSet (pNode, szTemp, &m_apBevel[i]);
      }
   }
   MMLValueSet (pNode, gpszMatrix, &m_mMatrix);
   MMLValueSet (pNode, gpszShapeValue, (int) m_dwShape);
   MMLValueSet (pNode, gpszDrawEnds, (int) m_fDrawEnds);
   MMLValueSet (pNode, gpszBackfaceCull, (int) m_fBackfaceCull);
   if (m_dwTextureWrap)
      MMLValueSet (pNode, gpszTextureWrap, (int) m_dwTextureWrap);
   if (m_dwTextureWrapVert)
      MMLValueSet (pNode, gpszTextureWrapVert, (int) m_dwTextureWrapVert);

   return pNode;
}

/************************************************************************************
CNoodle::MMLFrom - Reads the noodle in from a MML node

inptus
   PCMMLNode2      pNode - Node written by CNoodle::MMLTo()
retursn  
   BOOL - TRUE if success
*/
BOOL CNoodle::MMLFrom (PCMMLNode2 pNode)
{
   if (m_psPath)
      delete m_psPath;
   m_psPath = NULL;
   if (m_psFront)
      delete m_psFront;
   m_psFront = NULL;
   if (m_psScale)
      delete m_psScale;
   m_psScale = NULL;
   if (m_psShape)
      delete m_psShape;
   m_psShape = NULL;
   if (m_pssShape)
      delete m_pssShape;
   m_pssShape = NULL;

   // set dirty
   m_fDirty = TRUE;

   CPoint pZero;
   pZero.Zero();
   MMLValueGetPoint (pNode, gpszOffsetPoint, &m_pOffset, &pZero);
   pZero.p[1] = -1;
   MMLValueGetPoint (pNode, gpszFrontPoint, &m_pFront, &pZero);
   pZero.p[0] = pZero.p[1] = 1;
   MMLValueGetPoint (pNode, gpszScalePoint, &m_pScale, &pZero);
   pZero.Zero();
   pZero.p[2] = 1;

   DWORD i;
   for (i = 0; i < 2; i++) {
      WCHAR szTemp[32];
      swprintf (szTemp, gpszBevelMode, (int) i);
      m_adwBevelMode[i] = (DWORD) MMLValueGetInt (pNode, szTemp, 0);
      m_apBevel[i].Zero();
      m_apBevel[i].p[2] = 1;

      if (m_adwBevelMode[i]) {
         swprintf (szTemp, gpszBevel, (int) i);
         MMLValueGetPoint (pNode, szTemp, &m_apBevel[i], &pZero);
      }
   }

   CMatrix mIdent;
   mIdent.Identity();
   MMLValueGetMatrix (pNode, gpszMatrix, &m_mMatrix, &mIdent);

   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShapeValue, 0);
   m_fDrawEnds = (BOOL) MMLValueGetInt (pNode, gpszDrawEnds, TRUE);
   m_dwTextureWrap = (DWORD) MMLValueGetInt (pNode, gpszTextureWrap, 0);
   m_dwTextureWrapVert = (DWORD) MMLValueGetInt (pNode, gpszTextureWrapVert, 0);
   m_fBackfaceCull = (BOOL) MMLValueGetInt (pNode, gpszBackfaceCull, TRUE);

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszPath)) {
         m_psPath = new CSpline;
         if (!m_psPath)
            return FALSE;
         if (!m_psPath->MMLFrom (pSub))
            return FALSE;
      }
      else if (!_wcsicmp(psz, gpszFront)) {
         m_psFront = new CSpline;
         if (!m_psFront)
            return FALSE;
         if (!m_psFront->MMLFrom (pSub))
            return FALSE;
      }
      else if (!_wcsicmp(psz, gpszScale)) {
         m_psScale = new CSpline;
         if (!m_psScale)
            return FALSE;
         if (!m_psScale->MMLFrom (pSub))
            return FALSE;
      }
      else if (!_wcsicmp(psz, gpszShape)) {
         m_psShape = new CSpline;
         if (!m_psShape)
            return FALSE;
         if (!m_psShape->MMLFrom (pSub))
            return FALSE;
      }
      else if (!_wcsicmp(psz, gpszsShape)) {
         m_pssShape = new CSplineSurface;
         if (!m_pssShape)
            return FALSE;
         if (!m_pssShape->MMLFrom (pSub))
            return FALSE;
      }
   }

   // regenrate the sahpe
   if (m_dwShape != NS_CUSTOM)
      ShapeDefault (m_dwShape);

   return TRUE;
}


/************************************************************************************
IntersectTwoLines - Given two lines that don't necessarily intersect, this finds
out where they intersect - or the average of where they intersect. Used to ensure
that frames come out the right width on corners, etc.

inputs
   PCPoint     pS1, pE1 - Start and end of line 1
   PCPoint     pS2, pR2 - Start and end of line 2
   PCPoint     pRes - Filled with the result, which is the new value to be used for pE1 and pE2
returns
   none
*/
void IntersectTwoLines (PCPoint pS1, PCPoint pE1, PCPoint pS2, PCPoint pE2, PCPoint pRes)
{
   // vectors for the two lines
   CPoint pV1, pV2;
   pV1.Subtract (pE1, pS1);
   pV2.Subtract (pE2, pS2);
   pV1.Normalize();
   pV2.Normalize();

   // create the normal for the plane they're on
   CPoint pN;
   pN.CrossProd (&pV1, &pV2);
   if (pN.Length() < CLOSE) {
      // they're basically running into each other, so just average E1 and E2
      pRes->Average (pE1, pE2);
      return;
   };
   pN.Normalize();

   // come up with plane that both lines are on
   pV2.CrossProd (&pV1, &pN); // now perpendicular to V1 and normal
   pV2.Normalize();

   // make a matrix out of this
   CMatrix m, mTrans, mInv;
   m.RotationFromVectors (&pV2, &pV1, &pN);
   mTrans.Translation (pS1->p[0], pS1->p[1], pS1->p[2]);
   m.MultiplyRight (&mTrans);
   m.Invert4 (&mInv);

   // if multiple the line from pS1 to pE1 by mInv then pS1 is moved to 0,0,0, and
   // pE1 is moved to 0,?,0.
#ifdef _DEBUG
   CPoint pNewS1, pNewE1;
   pNewS1.Copy (pS1);
   pNewS1.p[3] = 1;
   pNewS1.MultiplyLeft (&mInv);
   pNewE1.Copy (pE1);
   pNewE1.p[3] = 1;
   pNewE1.MultiplyLeft (&mInv);
#endif

   // multiupley line from pS2 to pE2, then just need to find out where the line
   // crosses the x=0 plane
   CPoint pNewS2, pNewE2;
   pNewS2.Copy (pS2);
   pNewS2.p[3] = 1;
   pNewS2.MultiplyLeft (&mInv);
   pNewE2.Copy (pE2);
   pNewE2.p[3] = 1;
   pNewE2.MultiplyLeft (&mInv);

   // see where the line intersects y = 0
   fp fDeltaX, fDeltaY, fm, b, y;
   fDeltaX = pNewE2.p[0] - pNewS2.p[0];
   fDeltaY = pNewE2.p[1] - pNewS2.p[1];
   if (fabs(fDeltaX) < CLOSE) {
      // shouldn't happen, but basically, there isn't any slope
      pRes->Average (pE1, pE2);
      return;
   }
   fm = fDeltaY / fDeltaX;
   b = pNewS2.p[1] - fm * pNewS2.p[0];
   y = fm * 0.0 + b;

   // use this
   pRes->p[0] = 0;
   pRes->p[1] = y;
   pRes->p[2] = (pNewE2.p[2] + 0.0) / 2;
   pRes->p[3] = 1;
   pRes->MultiplyLeft (&m);

   // done
}


/************************************************************************************
SplitAt - Given a CListFist containing doubles, this inserts a number into its
proper place in the list. If the number (or something close) already exists then
nothing is changed.

inputs
   PCListFixed    pl - List already initialied to sizeof(fp). List is sorted
                  from lowest to highest value.
   fp         fVal - Value
returns
   none
*/
void SplitAt (PCListFixed pl, fp fVal)
{
   fp *paf = (fp*) pl->Get(0);
   DWORD dwNum = pl->Num();
   DWORD i;

   for (i = 0; i < dwNum; i++) {
      // check to see if it's less
      if (fVal < (paf[i]-CLOSE)) {
         // insert before this
         pl->Insert (i, &fVal);
         return;
      }

      // check to see if it's the same
      if (fVal < (paf[i]+ CLOSE))
         return;
   }

   // add it
   pl->Add (&fVal);

}

typedef struct {
   CPoint      pPoint;           // point
   CPoint      pNorm[2];         // normal, [0]=right side of point, [1]=left side of point
} NVERT, *PNVERT;

typedef struct {
   // the [2] is for, [0] = bottom, [1]=top

   // location
   fp      fLoc[2];          // location from 0 .. 1
   DWORD       dwControl[2];     // converted back to initial control point
   fp      fAvgLen;          // average length of segment, from bottom to top
   fp      afAvgCirc[2];     // average circumfrence

   // vectors
   CPoint      apTangent[2];     // point in direction that curve going, also the "up" vector
   CPoint      apFront[2];       // front vector, perpendicular to tangent
   CPoint      apRight[2];       // right vector, perpendicular to tangent and front

   // scale and offset
   CPoint      apScale[2];       // scale (x and y used only) at this point
   CPoint      apOffset[2];      // offset (x and y used only) at this point

   // verticies
   PCListFixed aplNVERT[2];      // list of NVERT structures
} NSEG, *PNSEG;

/************************************************************************************
CNoodle::LocationAndDirection - Given a location (from 0..1) along the spline, this
returns its location (in rotated space), and the vectors for forward, front, and left.

inputs
   fp          fLoc - Location from 0..1
   PCPoint     pLoc - If not NULL, filled in with spacial location of center
   PCPoint     pForward - If not NULL, filled in with forward direction
   PCPoint     pFront - If not NULL, filled in with front
   PCPoint     pRight - If not NULL, filled in with right
   PCPoint     pScale - If not NULL, filled in with the scale for the location. .p[0] and .p[1]
returns
   BOOL - TRUE if success
*/
BOOL CNoodle::LocationAndDirection (fp fLoc, PCPoint pLoc, PCPoint pForward,
                                    PCPoint pFront, PCPoint pRight, PCPoint pScale)
{
   // what's the location along the splines
   // find the tangent at this point
   CPoint pStart, pEnd, pF;
   DWORD dwEnd = FALSE;
   if (fLoc + CLOSE >= 1.0)
      dwEnd = TRUE;
   pStart.Zero();
   pEnd.Zero();
   m_psPath->LocationGet (fLoc - (dwEnd ? CLOSE : 0), &pStart);
   m_psPath->LocationGet (fLoc + (dwEnd ? 0 : CLOSE), &pEnd);
   if (pLoc)
      pLoc->Copy (dwEnd ? &pEnd : &pStart);
   pEnd.Subtract (&pStart);
   pEnd.Normalize();
   if (pEnd.Length() < EPSILON) {
      return FALSE;  // cant tell
   }
   pF.Copy (&pEnd);

   // find front
   pStart.Zero();
   if (m_psFront)
      m_psFront->LocationGet (fLoc, &pStart);
   else
      pStart.Copy (&m_pFront);

   // find right
   pEnd.CrossProd (&pF, &pStart);
   pEnd.Normalize();
   if (pEnd.Length() < CLOSE) {
      // obviously front didn't work
      DWORD dwDir;
      for (dwDir = 0; dwDir < 3; dwDir++) {
         pStart.Zero();
         pStart.p[dwDir] = 1;
         pEnd.CrossProd (&pF, &pStart);
         pEnd.Normalize();
         if (pEnd.Length() > CLOSE)
            break;
      } // dwDir
   }

   // set parameters
   if (pForward)
      pForward->Copy (&pF);
   if (pRight)
      pRight->Copy (&pEnd);
   if (pFront) {
      pFront->CrossProd (&pEnd, &pF);
      pFront->Normalize();
   }

   // what's the scale here
   if (pScale) {
      pScale->Copy (&m_pScale);
      if (m_psScale)
         m_psScale->LocationGet (fLoc, pScale);
   }

   // Rotate by globla matrix if not identity
   CMatrix mIdent;
   mIdent.Identity();
   if (!mIdent.AreClose (&m_mMatrix)) {
      CMatrix mVect;
      m_mMatrix.Invert(&mVect);
      mVect.Transpose();

      if (pLoc)
         pLoc->MultiplyLeft (&m_mMatrix);
      if (pForward)
         pForward->MultiplyLeft (&mVect);
      if (pFront)
         pFront->MultiplyLeft (&mVect);
      if (pRight)
         pRight->MultiplyLeft (&mVect);
   }

   return TRUE;
}

/************************************************************************************
CNoodle::CalcPolygonsIfDirty - Calculate all the polygons to do if it's dirty.

inputs
   none
returns
   BOOL - TRUE if all OK, FALSE if error
*/
BOOL CNoodle::CalcPolygonsIfDirty (void)
{
   if (!m_psPath)
      return FALSE;
   if (!m_fDirty)
      return TRUE;

   // Do we backface cull? Dont cull if shape is only partial - not looped
   BOOL fBackfaceCull;
   fBackfaceCull = m_fBackfaceCull;
   if (m_psShape && !m_psShape->LoopedGet())
      fBackfaceCull = FALSE;
   if (m_pssShape && !m_pssShape->LoopGet (TRUE))
      fBackfaceCull = FALSE;

   // set up some variables
   CListFixed     lSplit, lSegment;
   BOOL fRet = TRUE;
   lSplit.Init (sizeof(fp));
   lSegment.Init (sizeof(NSEG));
   DWORD    i, dwCount;

   // loop over slow and fast versions
   DWORD dwFast;
   PNSEG pns;
   // BUGFIX 0 Was dwFast=0..1, changed to dwFast = 0 because dwFast==1 (the
   // optimized version of noodle) sometimes has drawing problems, and
   // just slows things down
   for (dwFast = 0; dwFast < 1; dwFast++) {

      // clear the segments
      // Will need to free up some contents
      for (i = 0; i < lSegment.Num(); i++) {
         pns = (PNSEG) lSegment.Get(i);
         if (pns->aplNVERT[0])
            delete pns->aplNVERT[0];
         if (pns->aplNVERT[1])
            delete pns->aplNVERT[1];
      }
      lSegment.Clear();

      // find where want to split this
      lSplit.Clear();
      SplitAt (&lSplit, 0);
      SplitAt (&lSplit, 1);

      if (dwFast) {
         // split on the path, but only at control points
         dwCount = m_psPath->OrigNumPointsGet() - (m_psPath->QueryLooped() ? 0 : 1);
         for (i = 0; i < m_psPath->OrigNumPointsGet(); i++)
            SplitAt (&lSplit, (fp)i / (fp) dwCount);
      }
      else {   // !dwFast
         // split on the path
         dwCount = m_psPath->QueryNodes() - (m_psPath->QueryLooped() ? 0 : 1);
         // BUGFIX - If this is a linear segment then only split at the start and
         // end of the linear segment
         DWORD dwDivide, dwCurve;
         dwDivide = dwCount / (m_psPath->OrigNumPointsGet() - (m_psPath->QueryLooped() ? 0 : 1));
         for (i = 0; i < m_psPath->QueryNodes (); i++) {
            dwCurve = SEGCURVE_CIRCLENEXT;
            m_psPath->OrigSegCurveGet (i / dwDivide, &dwCurve);
            if ((i % dwDivide) && (dwCurve == SEGCURVE_LINEAR))
               continue;   // if linear dont put in intermittent points
            SplitAt (&lSplit, (fp)i / (fp) dwCount);
         }

         // split on the front
         if (m_psFront) {
            dwCount = m_psFront->QueryNodes() - (m_psFront->QueryLooped() ? 0 : 1);
            dwDivide = dwCount / (m_psFront->OrigNumPointsGet() - (m_psFront->QueryLooped() ? 0 : 1));
            for (i = 0; i < m_psFront->QueryNodes (); i++) {
               dwCurve = SEGCURVE_CIRCLENEXT;
               m_psFront->OrigSegCurveGet (i / dwDivide, &dwCurve);
               if ((i % dwDivide) && (dwCurve == SEGCURVE_LINEAR))
                  continue;   // if linear dont put in intermittent points

               SplitAt (&lSplit, (fp)i / (fp) dwCount);
            }
         }

         // split at scale
         if (m_psScale) {
            dwCount = m_psScale->QueryNodes() - (m_psScale->QueryLooped() ? 0 : 1);
            dwDivide = dwCount / (m_psScale->OrigNumPointsGet() - (m_psScale->QueryLooped() ? 0 : 1));
            for (i = 0; i < m_psScale->QueryNodes (); i++) {
               dwCurve = SEGCURVE_CIRCLENEXT;
               m_psScale->OrigSegCurveGet (i / dwDivide, &dwCurve);
               if ((i % dwDivide) && (dwCurve == SEGCURVE_LINEAR))
                  continue;   // if linear dont put in intermittent points

               SplitAt (&lSplit, (fp)i / (fp) dwCount);
            }
         }

         // shape
         if (m_pssShape) {
            dwCount = m_pssShape->QueryNodes(FALSE) - (m_pssShape->LoopGet (FALSE) ? 0 : 1);
            for (i = 0; i < m_pssShape->QueryNodes (FALSE); i++)
               SplitAt (&lSplit, (fp)i / (fp) dwCount);
         }
      }

      // calculate all the semgnets
      DWORD dwSeg, dwEnd;
      NSEG ns;
      fp fScaleToControl;
      fScaleToControl = (fp) (m_psPath->OrigNumPointsGet() - (m_psPath->LoopedGet() ? 0 : 1));
      CMatrix mAtStartEnd[2], mInvAtStartEnd[2];      // matrix (and invert) at start and end, used for bevelling
      for (dwSeg = 0; dwSeg+1 < lSplit.Num(); dwSeg++) {
         memset (&ns, 0, sizeof(ns));

         // which end of the segment
         for (dwEnd = 0; dwEnd < 2; dwEnd++) {
            // if dwEnd == 0, its the start/bottom, dwEnd==1 end/top

            // what's the location along the splines
            fp fLoc;
            fLoc = *((fp*) lSplit.Get(dwSeg + dwEnd));

            ns.fLoc[dwEnd] = fLoc;
            ns.dwControl[dwEnd] = (DWORD)((fLoc + (dwEnd ? (-CLOSE) : CLOSE)) * fScaleToControl) % m_psPath->OrigNumPointsGet();

            // find the tangent at this point
            CPoint pStart, pEnd;
            pStart.Zero();
            pEnd.Zero();
            m_psPath->LocationGet (fLoc - (dwEnd ? CLOSE : 0), &pStart);
            m_psPath->LocationGet (fLoc + (dwEnd ? 0 : CLOSE), &pEnd);
            pEnd.Subtract (&pStart);
            pEnd.Normalize();
            if (pEnd.Length() < EPSILON) {
               fRet = FALSE;
               goto done;
            }
            ns.apTangent[dwEnd].Copy (&pEnd);

            // find front
            pStart.Zero();
            if (m_psFront)
               m_psFront->LocationGet (fLoc, &pStart);
            else
               pStart.Copy (&m_pFront);

            // find right
            pEnd.CrossProd (&ns.apTangent[dwEnd], &pStart);
            pEnd.Normalize();
            if (pEnd.Length() < CLOSE) {
               // obviously front didn't work
               DWORD dwDir;
               for (dwDir = 0; dwDir < 3; dwDir++) {
                  pStart.Zero();
                  pStart.p[dwDir] = 1;
                  pEnd.CrossProd (&ns.apTangent[dwEnd], &pStart);
                  pEnd.Normalize();
                  if (pEnd.Length() > CLOSE)
                     break;
               } // dwDir
            }
            ns.apRight[dwEnd].Copy (&pEnd);
            ns.apFront[dwEnd].CrossProd (&ns.apRight[dwEnd], &ns.apTangent[dwEnd]);
            ns.apFront[dwEnd].Normalize();

            // what's the scale here
            ns.apScale[dwEnd].Copy (&m_pScale);
            if (m_psScale)
               m_psScale->LocationGet (fLoc, &ns.apScale[dwEnd]);

            // and offset?
            ns.apOffset[dwEnd].Copy (&m_pOffset);

            // allocate lista
            ns.aplNVERT[dwEnd] = new CListFixed;
            if (!ns.aplNVERT[dwEnd]) {
               if (ns.aplNVERT[0])
                  delete ns.aplNVERT[0];
               fRet = FALSE;
               goto done;
            }
            ns.aplNVERT[dwEnd]->Init (sizeof(NVERT));

            // get the points and normals from the shape
            DWORD dwVert;
            NVERT nv;
            if (dwFast) {
               // in the fast version just use a rectangle
               for (dwVert = 0; dwVert < 4; dwVert++) {
                  memset (&nv, 0, sizeof(nv));
                  switch (dwVert) {
                  case 0:
                     nv.pPoint.p[0] = -.5;
                     nv.pPoint.p[1] = .5;
                     nv.pNorm[0].p[1] = 1;
                     nv.pNorm[1].p[0] = -1;
                     break;
                  case 1:
                     nv.pPoint.p[0] = .5;
                     nv.pPoint.p[1] = .5;
                     nv.pNorm[1].p[1] = 1;
                     nv.pNorm[0].p[0] = 1;
                     break;
                  case 2:
                     nv.pPoint.p[0] = .5;
                     nv.pPoint.p[1] = -.5;
                     nv.pNorm[0].p[1] = -1;
                     nv.pNorm[1].p[0] = 1;
                     break;
                  case 3:
                     nv.pPoint.p[0] = -.5;
                     nv.pPoint.p[1] = -.5;
                     nv.pNorm[1].p[1] = -1;
                     nv.pNorm[0].p[0] = -1;
                     break;
                  }

                  ns.aplNVERT[dwEnd]->Add (&nv);
               }
            }
            else if (m_pssShape) {
               // have a shape that changes, so use that
               DWORD dwNodes = m_pssShape->QueryNodes(TRUE);
               BOOL fLooped = m_pssShape->LoopGet(TRUE);
               DWORD dwDivide = dwNodes - (fLooped ? 0 : 1);
               fp   fH, fNorm;
               for (dwVert = 0; dwVert < dwNodes; dwVert++) {
                  memset (&nv, 0, sizeof(nv));

                  fH = (fp)dwVert / (fp)dwDivide;
                  m_pssShape->HVToInfo (fH, fLoc, &nv.pPoint);
                  nv.pPoint.p[2] = 0; // BUGFIX - Was messing up without

                  // just use the normals produced by point
                  fNorm = fH + CLOSE;
                  if (fNorm > 1) {
                     if (fLooped)
                        fNorm -= 1;
                     else
                        fNorm = 1;
                  }
                  m_pssShape->HVToInfo (fNorm, fLoc, NULL, &nv.pNorm[0]);
                  nv.pNorm[0].p[2] = 0;

                  fNorm = fH - CLOSE;
                  if (fNorm < 0) {
                     if (fLooped)
                        fNorm += 1;
                     else
                        fNorm = 0;
                  }
                  m_pssShape->HVToInfo (fNorm, fLoc, NULL, &nv.pNorm[1]);
                        // BUGFIX - Chasnged fH-CLOSE to fNorm
                  nv.pNorm[1].p[2] = 0;

                  ns.aplNVERT[dwEnd]->Add (&nv);
               }
            }
            else if (m_psShape) {
               // have a shape that's constant
               DWORD dwNodes = m_psShape->QueryNodes();
               BOOL fLooped = m_psShape->QueryLooped();
               CPoint pUp;
               pUp.Zero();
               pUp.p[2] = 1;

               for (dwVert = 0; dwVert < dwNodes; dwVert++) {
                  memset (&nv, 0, sizeof(nv));

                  nv.pPoint.Copy (m_psShape->LocationGet (dwVert));

                  // get the tangent and cross by up
                  PCPoint pTan;
                  CPoint p2;
                  pTan = m_psShape->TangentGet (dwVert, TRUE);
                  if (pTan) {
                     p2.Copy (pTan);
                     p2.p[2] = 0;
                     nv.pNorm[0].CrossProd (&pUp, pTan);
                  }
                  pTan = m_psShape->TangentGet (dwVert, FALSE);
                  if (pTan) {
                     p2.Copy (pTan);
                     p2.p[2] = 0;
                     nv.pNorm[1].CrossProd (&pUp, pTan);
                  }

                  ns.aplNVERT[dwEnd]->Add (&nv);
               }
            }
            else {
               // error
               fRet = FALSE;
               goto done;
            }

            // come up with a matrix to scale, rotate and translate
            CMatrix mRot, mScale, mTrans, mTrans2, m, mInv;
            CPoint pBack, pPosn;
            pBack.Copy (&ns.apFront[dwEnd]);
            pBack.Scale (-1);
            mRot.RotationFromVectors (&ns.apRight[dwEnd], &pBack, &ns.apTangent[dwEnd]);
            mScale.Identity();
            mScale.p[0][0] = ns.apScale[dwEnd].p[0];
            mScale.p[1][1] = ns.apScale[dwEnd].p[1];
            mTrans.Translation (ns.apOffset[dwEnd].p[0], ns.apOffset[dwEnd].p[1], 0);
            m.Multiply (&mTrans, &mScale);
            m.MultiplyRight (&mRot);
            pPosn.Zero();
            m_psPath->LocationGet (fLoc, &pPosn);
            mTrans2.Translation (pPosn.p[0], pPosn.p[1], pPosn.p[2]);
            m.MultiplyRight (&mTrans2);
            m.MultiplyRight (&m_mMatrix);

            // and to apply to the normals
            m.Invert (&mInv); // dont need Invert4 since dont care about translation
            mInv.Transpose ();

            // keep track for start and end
            if (dwSeg + dwEnd == 0) {
               mAtStartEnd[0].Multiply (&mRot, &mTrans);
               mAtStartEnd[0].MultiplyRight (&mTrans2);
               mAtStartEnd[0].MultiplyRight (&m_mMatrix);
               mAtStartEnd[0].Invert (&mInvAtStartEnd[0]);  // will only use this for vectors
               mInvAtStartEnd[0].Transpose();
            }
            if (dwSeg + dwEnd == (lSplit.Num()-1)) {
               mAtStartEnd[1].Multiply (&mRot, &mTrans);
               mAtStartEnd[1].MultiplyRight (&mTrans2);
               mAtStartEnd[1].MultiplyRight (&m_mMatrix);
               mAtStartEnd[1].Invert (&mInvAtStartEnd[1]);  // will only use this for vectors
               mInvAtStartEnd[1].Transpose();
            }

            // loop through all the points again
            PNVERT pnv;
            for (dwVert = 0; dwVert < ns.aplNVERT[dwEnd]->Num(); dwVert++) {
               pnv = (PNVERT) ns.aplNVERT[dwEnd]->Get(dwVert);
               
               // point
               pnv->pPoint.p[3] = 1;
               pnv->pPoint.MultiplyLeft (&m);

               // normals
               pnv->pNorm[0].p[3] = 1;
               pnv->pNorm[0].MultiplyLeft (&mInv);
               pnv->pNorm[0].Normalize();
               pnv->pNorm[1].p[3] = 1;
               pnv->pNorm[1].MultiplyLeft (&mInv);
               pnv->pNorm[1].Normalize();
            }


         } // dwEnd

         // add this
         lSegment.Add (&ns);
      }  // dwSeg

      // is this looped around the shape
      BOOL fLoopShape;
      fLoopShape = FALSE;
      if (m_pssShape)
         fLoopShape = m_pssShape->LoopGet (TRUE);
      else if (m_psShape)
         fLoopShape = m_psShape->LoopedGet();

      // Go back and merge
      PNSEG pns2;
      DWORD dwNumSeg;
      dwNumSeg = lSegment.Num() - (m_psPath->LoopedGet() ? 0 : 1);
      for (dwSeg = 0; dwSeg < dwNumSeg; dwSeg++) {
         pns = (PNSEG) lSegment.Get(dwSeg);
         pns2 = (PNSEG) lSegment.Get((dwSeg+1) % lSegment.Num());

         // max
         DWORD dwMax, dwNum, dwVert;
         PNVERT pB1, pT1, pB2, pT2;
         CPoint pMerge;
         dwNum = pns->aplNVERT[0]->Num();
         dwMax = dwNum;
         pB1 = (PNVERT) pns->aplNVERT[0]->Get(0);
         pT1 = (PNVERT) pns->aplNVERT[1]->Get(0);
         pB2 = (PNVERT) pns2->aplNVERT[0]->Get(0);
         pT2 = (PNVERT) pns2->aplNVERT[1]->Get(0);
         for (dwVert = 0; dwVert < dwMax; dwVert++) {
            IntersectTwoLines (&pB1[dwVert].pPoint, &pT1[dwVert].pPoint, &pT2[dwVert].pPoint, &pB2[dwVert].pPoint, &pMerge);
            pT1[dwVert].pPoint.Copy (&pMerge);
            pB2[dwVert].pPoint.Copy (&pMerge);

            // if they're transitioning a major conrol point and any bits are linear then
            // special blending case
            BOOL fAverage;
            fAverage = TRUE;
            if (pns->dwControl[1] != pns2->dwControl[0]) {
               DWORD dwType1, dwType2;
               m_psPath->OrigSegCurveGet (pns->dwControl[1], &dwType1);
               m_psPath->OrigSegCurveGet (pns2->dwControl[0], &dwType2);

               if ((dwType1 == SEGCURVE_LINEAR) || (dwType2 == SEGCURVE_LINEAR)) {
                  fAverage = FALSE;
                  // will ahve a hard edge
               }

#if 0 // dont do this because if come to any linear segment wont be smooth
               else if ((dwType1 == SEGCURVE_LINEAR) && (dwType2 != SEGCURVE_LINEAR)) {
                  // use the linear normals in the non-linear section
                  pB2[dwVert].pNorm[0].Copy (&pT1[dwVert].pNorm[0]);
                  pB2[dwVert].pNorm[1].Copy (&pT1[dwVert].pNorm[1]);
                  fAverage = FALSE;
               }
               else if ((dwType1 != SEGCURVE_LINEAR) && (dwType2 == SEGCURVE_LINEAR)) {
                  // use the linera normals in the non-linear section
                  pT1[dwVert].pNorm[0].Copy (&pB2[dwVert].pNorm[0]);
                  pT1[dwVert].pNorm[1].Copy (&pB2[dwVert].pNorm[1]);
                  fAverage = FALSE;
               };
#endif // 0
            };

            if (fAverage) {
               // average normals
               CPoint pAvg;
               pAvg.Average (&pT1[dwVert].pNorm[0], &pB2[dwVert].pNorm[0]);
               pAvg.Normalize();
               pT1[dwVert].pNorm[0].Copy (&pAvg);
               pB2[dwVert].pNorm[0].Copy (&pAvg);
               pAvg.Average (&pT1[dwVert].pNorm[1], &pB2[dwVert].pNorm[1]);
               pAvg.Normalize();
               pT1[dwVert].pNorm[1].Copy (&pAvg);
               pB2[dwVert].pNorm[1].Copy (&pAvg);
            }
         }  // dwVert
      }  // dwSeg

      // Bevelling - if not looped
      CPoint aEndBevel[2];
      for (dwEnd = 0; dwEnd < 2; dwEnd++) {
         if (m_psPath->LoopedGet())
            continue;

         PCPoint pBevel;
         DWORD  dwMode;
         CPoint pDef;
         pDef.Zero();
         pDef.p[2] = 1;
         pBevel = &m_apBevel[dwEnd];
         dwMode = m_adwBevelMode[dwEnd];
         if (!dwMode)
            pBevel = &pDef;

         // find out where this point is and normal - in object space, not at edge of spline
         CPoint pLoc, pNorm, pInter;
         pLoc.Zero();
         m_psPath->LocationGet (dwEnd ? 1.0 : 0.0, &pLoc);
         pLoc.p[3] = 1;
         pLoc.MultiplyLeft (&m_mMatrix);
         pNorm.Copy (pBevel);
         pNorm.p[3] = 1;
         if (dwMode == 2) {
            // need to just take main rotation matrix and invert there
            CMatrix mInv;
            m_mMatrix.Invert (&mInv);  // will only use this for vectors
            mInv.Transpose();
            pNorm.MultiplyLeft (&mInv);
         }
         else  // just multiply by start conversion for start/end
            pNorm.MultiplyLeft (&mInvAtStartEnd[dwEnd]);

         // BUGFIX - Store the normal away
         aEndBevel[dwEnd].Copy (&pNorm);

         // BUGFIX - Move this to afte cal norm so can get the end normal right
         if ((dwMode < 2) && (pBevel->p[0] == 0) && (pBevel->p[1] == 0))
            continue;

         // get the points
         pns = (PNSEG) lSegment.Get(dwEnd ? (lSegment.Num()-1) : 0);
         DWORD dwNum, dwVert;
         PNVERT pS, pE;
         dwNum = pns->aplNVERT[0]->Num();
         pS = (PNVERT) pns->aplNVERT[!dwEnd]->Get(0);
         pE = (PNVERT) pns->aplNVERT[dwEnd]->Get(0);
         
         for (dwVert = 0; dwVert < dwNum; dwVert++) {
            if (IntersectLinePlane (&pS[dwVert].pPoint, &pE[dwVert].pPoint, &pLoc, &pNorm, &pInter))
               pE[dwVert].pPoint.Copy (&pInter);
         }

      }

      // clear out the memory for normals and whatnot
      m_amemPoints[dwFast].m_dwCurPosn = 0;
      m_amemNormals[dwFast].m_dwCurPosn = 0;
      m_amemTextures[dwFast].m_dwCurPosn = 0;
      m_amemVertices[dwFast].m_dwCurPosn = 0;
      m_amemPolygons[dwFast].m_dwCurPosn = 0;
      m_adwPoints[dwFast] = 0;
      m_adwNormals[dwFast] = 0;
      m_adwTextures[dwFast] = 0;
      m_adwVertices[dwFast] = 0;
      m_adwPolygons[dwFast] = 0;

      // calculate the length long each vertex (up/down) and circumfrence at each vertex
      CPoint pt;
      fp fAvgLength, fAvgCirc;
      fAvgLength = fAvgCirc = 0;
      CMem  memAvgCirc;
      DWORD dwNumCirc;
      pns = (PNSEG) lSegment.Get(0);
      dwNumCirc = pns->aplNVERT[0]->Num() - (fLoopShape ? 0 : 1);
      if (!memAvgCirc.Required (dwNumCirc * sizeof(fp))) {
         fRet = FALSE;
         goto done;
      }
      fp *pafCirc;
      pafCirc = (fp*) memAvgCirc.p;
      memset (pafCirc, 0, sizeof(fp) * dwNumCirc);
      for (dwSeg = 0; dwSeg < lSegment.Num(); dwSeg++) {
         pns = (PNSEG) lSegment.Get(dwSeg);

         DWORD dwMax, dwNum, dwVert;
         PNVERT pB, pT;
         dwNum = pns->aplNVERT[0]->Num();
         dwMax = dwNum - (fLoopShape ? 0 : 1);
         pB = (PNVERT) pns->aplNVERT[0]->Get(0);
         pT = (PNVERT) pns->aplNVERT[1]->Get(0);
         for (dwVert = 0; dwVert < dwNum; dwVert++) {
            pt.Subtract (&pB[dwVert].pPoint, &pT[dwVert].pPoint);
            pns->fAvgLen += pt.Length();

            if (dwVert >= dwMax)
               continue;

            if (m_dwTextureWrap) {
               pns->afAvgCirc[0] = pns->afAvgCirc[1] = (fp) m_dwTextureWrap;
               pafCirc[dwVert] =  pns->afAvgCirc[0] / (fp) dwMax;
            }
            else {
               // calculate
               fp fLen;
               pt.Subtract (&pB[(dwVert+1)%dwNum].pPoint, &pB[dwVert].pPoint);
               fLen = pt.Length();
               pns->afAvgCirc[0] += fLen;
               pafCirc[dwVert] += fLen / 2.0 / lSegment.Num();
               pt.Subtract (&pT[(dwVert+1)%dwNum].pPoint, &pT[dwVert].pPoint);
               fLen = pt.Length();
               pns->afAvgCirc[1] += fLen;
               pafCirc[dwVert] += fLen / 2.0 / lSegment.Num();
            }
         }  // dwVert

         pns->fAvgLen /= (fp)dwNum;
         //pns->afAvgCirc[0] /= (fp) dwMax;
         //pns->afAvgCirc[1] /= (fp) dwMax;

         fAvgLength += pns->fAvgLen;
         fAvgCirc += (pns->afAvgCirc[0] + pns->afAvgCirc[1]) / 2.0;
      }
      fAvgCirc /= (fp) lSegment.Num();

      if (!dwFast) {
         m_fLength = fAvgLength / (fp)lSegment.Num();   // so know average length per segment
         m_fCircum = fAvgCirc;
      }

      // loop through all of the segments, then shapes vertices within the segments
      // and create polygons
      fp fTextVBottom, fWrapInc;
      if (m_dwTextureWrapVert) {
         fTextVBottom = 0;
         fWrapInc = (fp)m_dwTextureWrapVert / (fp)lSegment.Num();
      }
      else
         fTextVBottom = fAvgLength; // texture at the bottom of the segment

      for (dwSeg = 0; dwSeg < lSegment.Num(); dwSeg++) {
         pns = (PNSEG) lSegment.Get(dwSeg);

         // max
         DWORD dwMax, dwNum, dwVert;
         PNVERT pB, pT;
         dwNum = pns->aplNVERT[0]->Num();
         dwMax = dwNum - (fLoopShape ? 0 : 1);
         pB = (PNVERT) pns->aplNVERT[0]->Get(0);
         pT = (PNVERT) pns->aplNVERT[1]->Get(0);

         // textures
         fp fTextHBottom, fTextHTop;
         fTextHBottom = fAvgCirc;
         fTextHTop = fAvgCirc;

         fp fTextVTop = fTextVBottom - (m_dwTextureWrapVert ? fWrapInc : pns->fAvgLen);

         for (dwVert = 0; dwVert < dwMax; dwVert++) {
            // texture length
            fp fLenBottom, fLenTop;
            fLenBottom = fLenTop = pafCirc[dwVert];

            // loop through the 4 corners
            DWORD dwCorner;
            DWORD adwVertex[4];
            TEXTPOINT5 tp;
            for (dwCorner = 0; dwCorner < 4; dwCorner++) {
               // this translates into
               PNVERT pC;
               DWORD dwNorm;
               switch (dwCorner) {
               case 0:
                  pC = pT + (dwVert % dwNum);
                  dwNorm = 0;
                  tp.hv[0] = fTextHTop;
                  tp.hv[1] = fTextVTop;
                  break;
               case 1:
                  pC = pB + (dwVert % dwNum);
                  dwNorm = 0;
                  tp.hv[0] = fTextHBottom;
                  tp.hv[1] = fTextVBottom;
                  break;
               case 2:
                  pC = pB + ((dwVert+1) % dwNum);
                  dwNorm = 1;
                  tp.hv[0] = fTextHBottom - fLenBottom;
                  tp.hv[1] = fTextVBottom;
                  break;
               case 3:
                  pC = pT + ((dwVert+1) % dwNum);
                  dwNorm = 1;
                  tp.hv[0] = fTextHTop - fLenTop;
                  tp.hv[1] = fTextVTop;
                  break;
               }


               // copy over the point
               tp.xyz[0] = pC->pPoint.p[0];
               tp.xyz[1] = pC->pPoint.p[1];
               tp.xyz[2] = pC->pPoint.p[2];

               // find the point this is closest to
               DWORD dwPoint;
               DWORD dwNeed;
               for (dwPoint = 0; dwPoint < m_adwPoints[dwFast]; dwPoint++)
                  if (pC->pPoint.AreClose ( ((PCPoint)(m_amemPoints[dwFast].p)) + dwPoint))
                     break;
               if (dwPoint >= m_adwPoints[dwFast]) {
                  // didn't find it already in the list so add
                  dwNeed = (dwPoint+1) * sizeof(CPoint);
                  if (dwNeed > m_amemPoints[dwFast].m_dwAllocated) {
                     if (!m_amemPoints[dwFast].Required(dwNeed + 512)) {
                        fRet = FALSE;
                        goto done;
                     }
                  }

                  PCPoint pp;
                  pp = (PCPoint) m_amemPoints[dwFast].p + dwPoint;
                  pp->Copy (&pC->pPoint);
                  m_adwPoints[dwFast]++;
               }

               // use the same point as a texture
               DWORD dwTexture;
               for (dwTexture = 0; dwTexture < m_adwTextures[dwFast]; dwTexture++)
                  if (AreClose (&tp, ((PTEXTPOINT5)(m_amemTextures[dwFast].p)) + dwTexture))
                     break;
               if (dwTexture >= m_adwTextures[dwFast]) {
                  // in this case, use the same for the texture
                  dwNeed = (dwTexture+1) * sizeof(TEXTPOINT5);
                  if (dwNeed > m_amemTextures[dwFast].m_dwAllocated) {
                     if (!m_amemTextures[dwFast].Required(dwNeed + 512)) {
                        fRet = FALSE;
                        goto done;
                     }
                  }

                  PTEXTPOINT5 pt;
                  pt = (PTEXTPOINT5) m_amemTextures[dwFast].p + dwTexture;
                  *pt = tp;
                  m_adwTextures[dwFast]++;
               }


               // find the normal
               DWORD dwNormal;
               for (dwNormal = 0; dwNormal < m_adwNormals[dwFast]; dwNormal++)
                  if (pC->pNorm[dwNorm].AreClose ( ((PCPoint)(m_amemNormals[dwFast].p)) + dwNormal))
                     break;
               if (dwNormal >= m_adwNormals[dwFast]) {
                  // didn't find it already in the list so add
                  dwNeed = (dwNormal+1) * sizeof(CPoint);
                  if (dwNeed > m_amemNormals[dwFast].m_dwAllocated) {
                     if (!m_amemNormals[dwFast].Required(dwNeed + 512)) {
                        fRet = FALSE;
                        goto done;
                     }
                  }

                  PCPoint pp;
                  pp = (PCPoint) m_amemNormals[dwFast].p + dwNormal;
                  pp->Copy (&pC->pNorm[dwNorm]);
                  m_adwNormals[dwFast]++;
               }

               // find a matching vertex
               DWORD dwVertex;
               PVERTEX pver;
               for (dwVertex = 0; dwVertex < m_adwVertices[dwFast]; dwVertex++) {
                  pver = (PVERTEX) m_amemVertices[dwFast].p + dwVertex;
                  if ((pver->dwNormal == dwNormal) && (pver->dwPoint == dwPoint) &&
                     (pver->dwTexture == dwTexture))
                     break;
               }
               if (dwVertex >= m_adwVertices[dwFast]) {
                  // didnt find in the list so add
                  dwNeed = (dwVertex+1) * sizeof(VERTEX);
                  if (dwNeed > m_amemVertices[dwFast].m_dwAllocated) {
                     if (!m_amemVertices[dwFast].Required(dwNeed+512)) {
                        fRet = FALSE;
                        goto done;
                     }
                  }

                  pver = (PVERTEX) m_amemVertices[dwFast].p + dwVertex;
                  memset (pver, 0, sizeof(VERTEX));
                  pver->dwNormal = dwNormal;
                  pver->dwPoint = dwPoint;
                  pver->dwTexture = dwTexture;
                  m_adwVertices[dwFast]++;
               }

               adwVertex[dwCorner] = dwVertex;
            } // dwCorner

            // make a polygon out of the vertices
            BYTE abTemp[sizeof(POLYDESCRIPT) + 4 * sizeof(DWORD)];
            memset (abTemp, 0, sizeof(abTemp));
            PPOLYDESCRIPT ppd;
            ppd = (PPOLYDESCRIPT) abTemp;
            DWORD *pdw;
            pdw = (DWORD*) (ppd + 1);
            ppd->dwSurface = 0;
            ppd->fCanBackfaceCull = fBackfaceCull;
            ppd->dwIDPart = 0;
            ppd->wNumVertices = 4;
            memcpy (pdw, adwVertex, 4 * sizeof(DWORD));


            // BUGFIX - Look for degenerate sides
            DWORD dwSides = 4;
            PVERTEX pver = (PVERTEX) m_amemVertices[dwFast].p;
            PCPoint pPoints = (PCPoint) m_amemPoints[dwFast].p;
            DWORD k;
            for (k = dwSides-1; k < dwSides; k--) {
               PCPoint p1 = pPoints + pver[pdw[k]].dwPoint;
               PCPoint p2 = pPoints + pver[pdw[(k+1)%dwSides]].dwPoint;
               if (!p1->AreClose (p2))
                  continue;   // different

               // else, remove
               memmove (pdw + k, pdw + (k+1), (dwSides - k - 1) * sizeof(DWORD));
               dwSides--;
            } // k


            if (dwSides >= 3) {
               ppd->wNumVertices = dwSides;

               // add this
               size_t dwNeed;
               dwNeed = m_amemPolygons[dwFast].m_dwCurPosn + sizeof(POLYDESCRIPT) + dwSides * sizeof(DWORD);
               if (dwNeed > m_amemPolygons[dwFast].m_dwAllocated) {
                  if (!m_amemPolygons[dwFast].Required(dwNeed+512)) {
                     fRet = FALSE;
                     goto done;
                  }
               }
               memcpy ((PBYTE) m_amemPolygons[dwFast].p + m_amemPolygons[dwFast].m_dwCurPosn,
                  abTemp, sizeof(POLYDESCRIPT) + dwSides * sizeof(DWORD));
               m_amemPolygons[dwFast].m_dwCurPosn = dwNeed;
               m_adwPolygons[dwFast]++;
            }

            // update texture
            fTextHBottom -= fLenBottom;
            fTextHTop -= fLenTop;

         } // dwVert

         fTextVBottom = fTextVTop;
      } // dwSeg

      // Do the end caps
      for (dwEnd = 0; dwEnd < 2; dwEnd++) {
         // dont draw ends if not set to, or if the shape is a thin slice, or if it's the fast mode
         // BUGFIX - Do the end-points even if fast, was || dwFast
         if (!m_fDrawEnds || m_psPath->LoopedGet())
            continue;
         if (m_psShape && !m_psShape->LoopedGet())
            continue;   // no end caps if not looped
         if (m_pssShape && !m_pssShape->LoopGet (TRUE))
            continue;   // only end caps if looped

         pns = (PNSEG) lSegment.Get(dwEnd ? (lSegment.Num()-1) : 0);

         // figure out a slot for the normal
         size_t dwNeed;
         DWORD dwNormal;
         dwNormal = m_adwNormals[dwFast];
         // didn't find it already in the list so add
         dwNeed = (dwNormal+1) * sizeof(CPoint);
         if (dwNeed > m_amemNormals[dwFast].m_dwAllocated) {
            if (!m_amemNormals[dwFast].Required(dwNeed + 512)) {
               fRet = FALSE;
               goto done;
            }
         }
         m_adwNormals[dwFast]++;

         // BUGFIX - Use previously calculated normal here
         PCPoint pn;
         pn = (PCPoint) m_amemNormals[dwFast].p + dwNormal;
         pn->Copy (&aEndBevel[dwEnd]);
         if (!dwEnd)
            pn->Scale(-1); // since normal pointing in wrong direction


         // remember start vertex #
         DWORD dwStartVert;
         dwStartVert = m_adwVertices[dwFast];

         // max
         DWORD dwNum, dwVert;
         PNVERT pv;
         dwNum = pns->aplNVERT[dwEnd]->Num();
         pv = (PNVERT) pns->aplNVERT[dwEnd]->Get(0);
         for (dwVert = 0; dwVert <= dwNum; dwVert++) {
            DWORD dwUse;

            // BUGFIX - Create a center point that's an average of all the other
            // points, so end caps work better

            if (dwVert >= dwNum)
               dwUse = dwNum;
            else {
               if (dwEnd)
                  dwUse = dwVert;
               else
                  dwUse = dwNum - dwVert - 1;   // reverse clockwise
            }

            // get the point
            CPoint pUse;
            DWORD dwPoint;
            if (dwUse != dwNum)
               pUse.Copy (&pv[dwUse].pPoint);
            else {
               // average
               pUse.Zero();
               for (dwPoint = 0; dwPoint < dwNum; dwPoint++)
                  pUse.Add (&pv[dwPoint].pPoint);
               pUse.Scale (1.0 / (fp) dwNum);
            }

            // add the point
            for (dwPoint = 0; dwPoint < m_adwPoints[dwFast]; dwPoint++)
               if (pUse.AreClose ( ((PCPoint)(m_amemPoints[dwFast].p)) + dwPoint))
                  break;
            if (dwPoint >= m_adwPoints[dwFast]) {
               // didn't find it already in the list so add
               dwNeed = (dwPoint+1) * sizeof(CPoint);
               if (dwNeed > m_amemPoints[dwFast].m_dwAllocated) {
                  if (!m_amemPoints[dwFast].Required(dwNeed + 512)) {
                     fRet = FALSE;
                     goto done;
                  }
               }

               PCPoint pp;
               pp = (PCPoint) m_amemPoints[dwFast].p + dwPoint;
               pp->Copy (&pUse);
               m_adwPoints[dwFast]++;
            }

            DWORD dwTexture;
            TEXTPOINT5 tp;
            memset (&tp, 0, sizeof(tp));

            // BUGFIX - So caps have 3d textures
            tp.xyz[0] = pUse.p[0];
            tp.xyz[1] = pUse.p[1];
            tp.xyz[2] = pUse.p[2];
            for (dwTexture = 0; dwTexture < m_adwTextures[dwFast]; dwTexture++)
               if (AreClose (&tp, ((PTEXTPOINT5)(m_amemTextures[dwFast].p)) + dwTexture))
                  break;
            if (dwTexture >= m_adwTextures[dwFast]) {
               // in this case, use the same for the texture
               dwNeed = (dwTexture+1) * sizeof(TEXTPOINT5);
               if (dwNeed > m_amemTextures[dwFast].m_dwAllocated) {
                  if (!m_amemTextures[dwFast].Required(dwNeed + 512)) {
                     fRet = FALSE;
                     goto done;
                  }
               }

               PTEXTPOINT5 pt;
               pt = (PTEXTPOINT5) m_amemTextures[dwFast].p + dwTexture;
               *pt = tp;
               m_adwTextures[dwFast]++;
            }


            // just max a vertex
            DWORD dwVertex;
            PVERTEX pver;
            dwVertex = m_adwVertices[dwFast];
            // didnt find in the list so add
            dwNeed = (dwVertex+1) * sizeof(VERTEX);
            if (dwNeed > m_amemVertices[dwFast].m_dwAllocated) {
               if (!m_amemVertices[dwFast].Required(dwNeed+512)) {
                  fRet = FALSE;
                  goto done;
               }
            }
            pver = (PVERTEX) m_amemVertices[dwFast].p + dwVertex;
            memset (pver, 0, sizeof(VERTEX));
            pver->dwNormal = dwNormal;
            pver->dwPoint = dwPoint;
            pver->dwTexture = dwTexture;
            m_adwVertices[dwFast]++;
         } // dwVert

         // BUGFIX - Adding one triangle per side
         for (dwVert = 0; dwVert < dwNum; dwVert++) {
            // add the polygon
            POLYDESCRIPT pd;
            DWORD dwNumVert;
            dwNumVert = 3; //m_adwVertices[dwFast] - dwStartVert;
            pd.dwSurface = 0;
            pd.fCanBackfaceCull = fBackfaceCull;
            pd.dwIDPart = 1;
            pd.wNumVertices = 3; //(WORD) dwNumVert;

            DWORD adw[3];
            adw[0] = dwStartVert + dwVert;
            adw[1] = dwStartVert + (dwVert+1) % dwNum;
            adw[2] = dwStartVert + dwNum;

            // BUGFIX - Look for degenerate sides
            DWORD k;
            PVERTEX pver = (PVERTEX) m_amemVertices[dwFast].p;
            PCPoint pPoints = (PCPoint) m_amemPoints[dwFast].p;
            for (k = 0; k < 3; k++) {
               PCPoint p1 = pPoints + pver[adw[k]].dwPoint;
               PCPoint p2 = pPoints + pver[adw[(k+1)%3]].dwPoint;
               if (p1->AreClose (p2))
                  break;   // same
            } // k

            if (k >= 3) {  // if k >= 3 then no degenerate points
               // add this
               dwNeed = m_amemPolygons[dwFast].m_dwCurPosn + sizeof(POLYDESCRIPT) + dwNumVert * sizeof(DWORD);
               if (dwNeed > m_amemPolygons[dwFast].m_dwAllocated) {
                  if (!m_amemPolygons[dwFast].Required(dwNeed+512)) {
                     fRet = FALSE;
                     goto done;
                  }
               }
               memcpy ((PBYTE) m_amemPolygons[dwFast].p + m_amemPolygons[dwFast].m_dwCurPosn,
                  &pd, sizeof(pd));
               DWORD *pdw;
               pdw = (DWORD*) ((PBYTE) m_amemPolygons[dwFast].p + (m_amemPolygons[dwFast].m_dwCurPosn+sizeof(pd)));
               memcpy (pdw, adw, sizeof(DWORD)*3);
               //for (i = dwStartVert; i < m_adwVertices[dwFast]; i++, pdw++)
               //   *pdw = i;
               m_amemPolygons[dwFast].m_dwCurPosn = dwNeed;
               m_adwPolygons[dwFast]++;
            }
         }  // vertex for polygon

#if 0 // DEAD code
         // calculate the normal
         PCPoint pn, pp, ap[3], pt;
         PVERTEX pver;
         pn = (PCPoint) m_amemNormals[dwFast].p + dwNormal;
         pp = (PCPoint) m_amemPoints[dwFast].p;
         pver = (PVERTEX) m_amemVertices[dwFast].p;
         DWORD dwCur;
         for (i = 0, dwCur = 0; (i < dwNumVert) && (dwCur < 3); i++) {
            pt = pp + pver[i + dwStartVert].dwPoint;
            if (i && pt->AreClose (ap[dwCur-1]))
               continue;   // dont take ones that are close
            ap[dwCur] = pt;
            dwCur++;
         }
         if (dwCur >= 3) {
            // vert
            CPoint p1, p2;
            p1.Subtract (ap[2], ap[1]);
            p2.Subtract (ap[0], ap[1]);
            pn->CrossProd (&p2, &p1);
            pn->Normalize();
         }
         else {
            // not enough points
            pn->Zero();
            pn->p[0] = 1;
         }
#endif // 0
         
      }  // dwEnd - for end caps

   }  // dwFast


done:
   // Free up memory from the segments
   for (i = 0; i < lSegment.Num(); i++) {
      pns = (PNSEG) lSegment.Get(i);
      if (pns->aplNVERT[0])
         delete pns->aplNVERT[0];
      if (pns->aplNVERT[1])
         delete pns->aplNVERT[1];
   }

   // at the end
   m_fDirty = !fRet;
   return fRet;
}


/************************************************************************************
CNoodle::Render - Call this to draw using the OBJECTRENDER info provided.

inputs
   POBJECTRENDER     pr - Rendering info
   PCRenderSurface   prs - Render surface already set up for the object. This
                     means that the default surface should be set, which will be
                     used for drawing.
returns
   BOOL - TRUE if succes
*/
BOOL CNoodle::Render (POBJECTRENDER pr, PCRenderSurface prs)
{
   // make sure the poygons are calculated
   if (!CalcPolygonsIfDirty())
      return FALSE;

   // what version
   DWORD dwFast;
   // BUGFIX - Always using slow mode now
   // dwFast = (prs->m_fDetail > max(m_fLength, m_fCircum/PI));
   dwFast = 0;

   // call commit() so that all indecies will start at 0 again
   prs->Commit();

   // points
   PCPoint pNew;
   DWORD dwIndex;
   pNew = prs->NewPoints (&dwIndex, m_adwPoints[dwFast]);
   if (!pNew)
      return FALSE;
   memcpy (pNew, m_amemPoints[dwFast].p, m_adwPoints[dwFast] * sizeof(CPoint));

   // normals
   pNew = prs->NewNormals (TRUE, &dwIndex, m_adwNormals[dwFast]);
   if (pNew)
      memcpy (pNew, m_amemNormals[dwFast].p, m_adwNormals[dwFast] * sizeof(CPoint));

   // textures
   PTEXTPOINT5 ptNew;
   ptNew = prs->NewTextures (&dwIndex, m_adwTextures[dwFast]);
   if (ptNew) {
      memcpy (ptNew, m_amemTextures[dwFast].p, m_adwTextures[dwFast] * sizeof (TEXTPOINT5));
      prs->ApplyTextureRotation (ptNew, m_adwTextures[dwFast]);

      // if we have the texture wrap than replace .h with the original values
      if (m_dwTextureWrap || m_dwTextureWrapVert) {
         DWORD i;
         PTEXTPOINT5 pSrc, pDest;
         pSrc = (PTEXTPOINT5) m_amemTextures[dwFast].p;
         pDest = ptNew;
         for (i = 0; i <  m_adwTextures[dwFast]; i++, pSrc++, pDest++) {
            if (m_dwTextureWrap)
               pDest->hv[0] = pSrc->hv[0];
            if (m_dwTextureWrapVert)
               pDest->hv[1] = pSrc->hv[1];
         }
      }
   }

   // vertices
   PVERTEX pv;
   pv = prs->NewVertices (&dwIndex, m_adwVertices[dwFast]);
   if (!pv)
      return FALSE;
   memcpy (pv, m_amemVertices[dwFast].p, m_adwVertices[dwFast] * sizeof (VERTEX));

   // make sure have default surface and color
   prs->DefSurface();
   prs->DefColor();

   // polygons
   PPOLYDESCRIPT pPoly;
   pPoly = prs->NewPolygons ((DWORD)m_amemPolygons[dwFast].m_dwCurPosn, m_adwPolygons[dwFast]);
   if (!pPoly)
      return FALSE;
   memcpy (pPoly, m_amemPolygons[dwFast].p, m_amemPolygons[dwFast].m_dwCurPosn);

   // dont need to force draw

   return TRUE;
}



/**********************************************************************************
CNoodle::QueryBoundingBox - Standard API
*/
void CNoodle::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2)
{
   // make sure the poygons are calculated
   if (!CalcPolygonsIfDirty()) {
      pCorner1->Zero();
      pCorner2->Zero();
      return;
   }

   DWORD dwFast;
   // BUGFIX - Always using slow mode now
   // dwFast = (prs->m_fDetail > max(m_fLength, m_fCircum/PI));
   dwFast = 0;

   PCPoint pap = (PCPoint)m_amemPoints[dwFast].p;
   pCorner1->Copy (&pap[0]);
   pCorner2->Copy (&pap[0]);

   DWORD i;
   for (i = 1, pap++; i < m_adwPoints[dwFast]; i++, pap++) {
      pCorner1->Min (pap);
      pCorner2->Max (pap);
   }
}

