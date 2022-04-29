/********************************************************************************
CRenderMatrix - Handles matrix functions and point functions used in rednering.

begun 5/9/2001 by Mike Rozak.
Copyrighg 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"



/**********************************************************************************
CPoint::IfCloseToZeroMakeZero - If it's close to zero then make it zero, to take
care of roundoff error.

inputs
   none
returns
   none
*/
void CPoint::IfCloseToZeroMakeZero (void)
{
   DWORD i;
   for (i = 0; i < 3; i++)
      if (fabs(p[i]) < CLOSE)
         p[i] = 0;
}

/**********************************************************************************
CPoint::AreClose - Returns TRUE if the two points are close.

inputs
   PCPoint p - other point
retursn
   BOOL - TRUE if close, FALSE if not
*/
BOOL CPoint::AreClose (const CPoint *pp)
{
   return (fabs(pp->p[0] - p[0]) < CLOSE) && (fabs(pp->p[1] - p[1]) < CLOSE) &&
      (fabs(pp->p[2] - p[2]) < CLOSE);
}

/**********************************************************************************
CPoint::Average - Averathe this point in with another one.

inputs
   PCPoint     pOther - Other point
   fp      fContribOther - Amount of contribution of the other, from 0 to 1
returns
   none
*/
void CPoint::Average (PCPoint pOther, fp fContribOther)
{
   DWORD i;
   for (i = 0; i < 4; i++) {
      p[i] = p[i] * (1 - fContribOther) + pOther->p[i] * fContribOther;
   }
}


/**********************************************************************************
CPoint::Average - Averathe two points and stick in this one.

inputs
   PCPoint     pA, pB - Two points
   fp      fContribA - Amount of contribution of the other, from 0 to 1
returns
   none
*/
void CPoint::Average (CPoint *pA, CPoint *pB, fp fContribA)
{
   DWORD i;
   for (i = 0; i < 4; i++)
      p[i] = pA->p[i] * fContribA + pB->p[i] * (1 - fContribA);
}

/**********************************************************************************
CPoint::MakeANormal - Take three points and figure out the normal based on the three
points. Then, stick the normal in this point.

inputs
   PCPoint     pCounterClock - Point to the left of center, counter-clockwise
   PCPoint     pCenter - Center point.
   PCPoint     pClock - Point to the right of center, clockwise
   BOOL        fNormalize - if TRUE then normalize the vector, else just leave any length
returns
   none
*/
void CPoint::MakeANormal (CPoint *pCounterClock, CPoint *pCenter, CPoint *pClock, BOOL fNormalize)
{
   CPoint a, b;
   a.Subtract (pCounterClock, pCenter);
   b.Subtract (pClock, pCenter);

   CrossProd (&a, &b);
   if (fNormalize)
      Normalize();
}


/**********************************************************************************
CPoint::MultiplyLeft - Multiply by a matrix to the left.

inputs
   CMatrix     *pLeft - matrix
returns
   none
*/
void CPoint::MultiplyLeft (CMatrix *pLeft)
{
   CPoint   t;
   t.Copy (this);
   pLeft->Multiply (&t, this);
}


/****************************************************************************
CrossProd4 - Does a cross-product of 3 4-dimensional vectors so we can do proper clipping
*/
void CPoint::CrossProd4 (CPoint *p1, CPoint *p2, CPoint *p3)
{
   DWORD i, dw1, dw2, dw3;
   CMatrix m;
   for (i = 0; i < 4; i++) {
      dw1 = 0;
      if (i == dw1)
         dw1++;
      dw2 = dw1 + 1;
      if (i == dw2)
         dw2++;
      dw3 = dw2 + 1;
      if (i == dw3)
         dw3++;

      // copy those cells over
#if 0
      m.p[0][0] = p1->p[dw1];
      m.p[1][0] = p1->p[dw2];
      m.p[2][0] = p1->p[dw3];

      m.p[0][1] = p2->p[dw1];
      m.p[1][1] = p2->p[dw2];
      m.p[2][1] = p2->p[dw3];

      m.p[0][2] = p3->p[dw1];
      m.p[1][2] = p3->p[dw2];
      m.p[2][2] = p3->p[dw3];
#endif
      m.p[0][0] = p1->p[dw1];
      m.p[0][1] = p1->p[dw2];
      m.p[0][2] = p1->p[dw3];

      m.p[1][0] = p2->p[dw1];
      m.p[1][1] = p2->p[dw2];
      m.p[1][2] = p2->p[dw3];

      m.p[2][0] = p3->p[dw1];
      m.p[2][1] = p3->p[dw2];
      m.p[2][2] = p3->p[dw3];

      p[i] = m.Determinant();
      if (i % 2)
         p[i] = -p[i];
   }
}



/****************************************************************************
CMatrix::MakeSquare - Call this after a matrix is transposed to use
for normals. This will make sure that normals multiplied by this stay the
same length, even if the original matrix is doing some scaling.
*/
void CMatrix::MakeSquare (void)
{
   CPoint pLen;
   DWORD i;
   for (i = 0; i < 3; i++)
      pLen.p[i] = p[i][0]*p[i][0] + p[i][1]*p[i][1] + p[i][2]*p[i][2];

// from MultiplywPoint      #define  MMP(i)   pDest->p[i] = p[0][i] * pRight->p[0] + p[1][i] * pRight->p[1] + p[2][i] * pRight->p[2] + p[3][i] * pRight->p[3]
   for (i = 0; i < 3; i++) {
      if (fabs(pLen.p[i] - 1.0) < CLOSE)
         continue;   // no change
      pLen.p[i] = sqrt(pLen.p[i]);
      if (pLen.p[i] < EPSILON)
         continue;   // 0 length

      p[i][0] /= pLen.p[i];
      p[i][1] /= pLen.p[i];
      p[i][2] /= pLen.p[i];
   }
}

/****************************************************************************
CMatrix::AreClose - Returns TRUE if the matricies are essentially the same.

inputs
   PCMatrix    pm - other matrix
returns
   BOOl TRUE if close
*/
BOOL CMatrix::AreClose (const CMatrix* pm)
{
   DWORD x,y;
   for (x = 0; x < 4; x++) for (y = 0; y < 4; y++)
      if (fabs(p[x][y] - pm->p[x][y]) > EPSILON)
         return FALSE;
   return TRUE;
}

/****************************************************************************
CMatrix::RotationFromVectors - Given three vectors, A, B, and C, this generates
a rotation matrix such that any point with (1,0,0) gets rotated to A, (0,1,0)
gets rotated to B, and (0,0,1) gets rotated to C.

inputs
   PCPoint     pA, pB, pC - three points
returns
   none
*/
void CMatrix::RotationFromVectors (PCPoint pA, PCPoint pB, PCPoint pC)
{
   Identity();

   DWORD i, j;
   PCPoint pCur;
   for (i = 0; i < 3; i++) {
      switch (i) {
      case 0:
         pCur = pA;
         break;
      case 1:
         pCur = pB;
         break;
      case 2:
         pCur = pC;
         break;
      }
      for (j = 0; j < 3; j++) {
         p[i][j] = pCur->p[j];
      }

   } // i

}

/****************************************************************************
CMatrix::RotationBasedOnVector - Rotated so that the vector pOrig is pointing
in the pAfter direction.

inputs
   PCPoint     pOrig - Original vector. Does NOT have to be normalized
   PCPoint     pAfter - Where it should be pointing to. Does NOT have to be normalized
returns
   none
*/
void CMatrix::RotationBasedOnVector (PCPoint pOrig, PCPoint pAfter)
{
   // normalize each vector
   CPoint pON, pAN;
   pON.Copy (pOrig);
   pON.Normalize();
   pAN.Copy (pAfter);
   pAN.Normalize();

   // find the vector orthogonal to both
   CPoint pOrth;
   pOrth.CrossProd (&pAN, &pON);
   if (pOrth.Length() < .0001) {
      // virtually identical, so make up a new vector
      CPoint pNew;
      pNew.Zero();
      pNew.p[0] = 1;
      pOrth.CrossProd(&pAN, &pNew);
      if (pOrth.Length() < .0001) {
         // X vector didnt work so use Y vetor. that has to work
         pNew.Zero();
         pNew.p[1] = 1;
         pOrth.CrossProd (&pAN, &pNew);
      }
   }
   pOrth.Normalize();

   // find the vector orthogonal to pOrth and pON/pAN
   CPoint pON2, pAN2;
   pON2.CrossProd (&pON, &pOrth);
   pAN2.CrossProd (&pAN, &pOrth);
   // there's no need to normalize

   // make a matrix out of the two, translplanting the vectors
   CMatrix mON, mAN;
   mON.Identity();
   mAN.Identity();
   DWORD i,j, k;
   PCPoint pCur;
   PCMatrix pm;
   for (k = 0; k < 2; k++) {
      pm = k ? &mAN : &mON;

      for (i = 0; i < 3; i++) {
         switch (i) {
         case 0:
            pCur = k ? &pAN : &pON;
            break;
         case 1:
            pCur = &pOrth;
            break;
         case 2:
            pCur = k ? &pAN2 : &pON2;
            break;
         }
         for (j = 0; j < 3; j++) {
            pm->p[i][j] = pCur->p[j];
         }

      } // j
   } // k

   // invert the original so it that if a vector is in the original direction
   // it will be rotated to a default 1,0,0, and then rotate that using the other
   CMatrix mONInv;
   mON.Invert (&mONInv);

   // multiply
   Multiply (&mAN, &mONInv);

#if 0 // test
   // test
   CPoint   pt;
   pt.Zero ();
   pt.p[0] = 1;
   pt.MultiplyLeft (&mON);
   pt.Zero ();
   pt.p[0] = 1;
   pt.MultiplyLeft (&mAN);
   pt.Copy (&pON);
   pt.MultiplyLeft (&mONInv);
   pt.Copy (&pON);
   pt.MultiplyLeft (this);
#endif
}


/****************************************************************************
CMatrix::FromXYZLLT - Fills in the matrix based upon a translation
   and series of rotations.
   this = Trans(pXYZ) x RotZ(pfLong) x RotX(pfLat) x RotY(pfTilt)

inputs
   PCPoint     pXYZ -The translation
   fp      fLongitude - Longitude
   fp      fLatitude - Latitude
   fp      fTilt - Tilt
returns
   none
*/
void CMatrix::FromXYZLLT (PCPoint pXYZ, fp fLongitude, fp fLatitude, fp fTilt)
{
   CMatrix m;

   // translate
   Translation(pXYZ->p[0], pXYZ->p[1], pXYZ->p[2]);

   // rotate
   if (fLongitude) {
      m.RotationZ (fLongitude);
      MultiplyLeft (&m);
   }
   if (fLatitude) {
      m.RotationX (fLatitude);  // flip sign because want positive latitude up, not down
      MultiplyLeft (&m);
   }
   if (fTilt) {
      m.RotationY (fTilt);
      MultiplyLeft (&m);
   }

}

/****************************************************************************
CMatrix::ToXYZLLT - Converts a matrix into its invividual translation
and rotation parts. The matirx is:
   this = Trans(pXYZ) x RotZ(pfLong) x RotX(pfLat) x RotY(pfTilt)

inputs
   PCPoint     pXYZ - Filled with the translation
   fp      *pfLongitude - Filled with the longitude
   fp      *pfLatitude - Filled with the latitude
   fp      *pfTilt - Filled with the tile
returns
   none
*/
void CMatrix::ToXYZLLT (PCPoint pXYZ, fp *pfLongitude, fp *pfLatitude, fp *pfTilt)
{
   // get translation
   pXYZ->Zero();
   pXYZ->MultiplyLeft (this);

   // pull out the translation
   CMatrix m,m2;
   m.Copy (this);
   m.p[0][3] = m.p[1][3] = m.p[2][3] = m.p[3][2] = m.p[3][1] = m.p[3][0] = 0;
   m.p[3][3] = 1;

   // figure out x, y, and z vectors
   CPoint x;
   x.Zero();
   x.p[1] = 1;
   x.MultiplyLeft (&m);
   x.p[2] = 0; // since don't use in the calculations
   // if it's close to zero then assume it's zero
   if (x.Length() < EPSILON)
      *pfLongitude = 0;
   else {
      *pfLongitude = -atan2 (x.p[0], x.p[1]);   // flip the sign

      // remove this from the matrix
      m2.RotationZ (-*pfLongitude);
      m.MultiplyRight (&m2);
   }

   // Figure out rotation around latitude
   x.Zero();
   x.p[1] = 1;
   x.MultiplyLeft (&m);
   x.p[0] = 0; // since dont use in the calculations
   // if it's close to zero then assume it's zero
   if (x.Length() < EPSILON)
      *pfLatitude = 0;
   else {
      *pfLatitude = atan2 (x.p[2], x.p[1]);  // flip sign because want positive latitude up, not down

      // remove this from the matrix
      m2.RotationX (-*pfLatitude);
      m.MultiplyRight (&m2);
   }

   // Figure out the tilt
   x.Zero();
   x.p[0] = 1; // need to use a different dimension since if used X vector know where it would end up
   x.MultiplyLeft (&m);
   x.p[1] = 0;
   // if it's close to zero then assume it's zero
   if (x.Length() < EPSILON)
      *pfTilt = 0;
   else {
      *pfTilt = -atan2 (x.p[2], x.p[0]);
      // NOTE: When set long=1.5, lat=PI/2, tilt=2.4, and then did inverse, got -2.4 tilt. Don't know why
   }

   // done
}



/*******************************************************************
CMatrix::Invert - Inverts the 3x3 part of the marix (not the homogenous)

inputs
   CMatrix     *pDest - destination
*/
void CMatrix::Invert (CMatrix *pDest)
{
   fp temp;
   int i,j;

   pDest->p[3][0] = pDest->p[3][1] = pDest->p[3][2] = 0.0;
   pDest->p[0][3] = pDest->p[1][3] = pDest->p[2][3] = 0.0;
   pDest->p[3][3] = 1.0;

   // invert p into pDest->p
   pDest->p[0][0] = p[2][2] * p[1][1] - p[1][2] * p[2][1];
   pDest->p[0][1] = p[0][2] * p[2][1] - p[2][2] * p[0][1];
   pDest->p[0][2] = p[1][2] * p[0][1] - p[0][2] * p[1][1];

   pDest->p[1][0] = p[1][2] * p[2][0] - p[2][2] * p[1][0];
   pDest->p[1][1] = p[2][2] * p[0][0] - p[0][2] * p[2][0];
   pDest->p[1][2] = p[0][2] * p[1][0] - p[1][2] * p[0][0];

   pDest->p[2][0] = p[2][1] * p[1][0] - p[1][1] * p[2][0];
   pDest->p[2][1] = p[0][1] * p[2][0] - p[2][1] * p[0][0];
   pDest->p[2][2] = p[1][1] * p[0][0] - p[0][1] * p[1][0];

   temp = Determinant ();
   if (temp == 0)
      return;   // cant invert

   for (i = 0; i < 3; i++)
      for (j = 0; j < 3; j++)
         pDest->p[i][j] /= temp;
}


/*******************************************************************
CMatrix::CoFactor - Returns the cofactor of the matrix

inputs
   DWORD    dwRow - Row
   DWORD    dwColumn
returns
   fp - CoFactor
*/

/*******************************************************************
CMatrix::CoFactor - Returns the cofactor of the matrix

inputs
   DWORD    dwRow - Row
   DWORD    dwColumn
returns
   fp - CoFactor
*/
fp CMatrix::CoFactor (DWORD dwRow, DWORD dwColumn)
{
   CMatrix m;

   DWORD x,y, dwUseY, dwUseX;
   for (y = 0; y < 4; y++) {
      if (y < dwRow)
         dwUseY = y;
      else if (y > dwRow)
         dwUseY = y-1;
      else
         continue;   // not used

      for (x = 0; x < 4; x++) {
         if (x < dwColumn)
            dwUseX = x;
         else if (x > dwColumn)
            dwUseX = x-1;
         else
            continue;   // not used

         m.p[dwUseY][dwUseX] = p[y][x];
      } // x
   } // y

   return m.Determinant() * (((dwRow + dwColumn) % 2) ? -1.0 : 1.0);
}

/*******************************************************************
CMatrix::Invert4 - Inverts the whole 4x4 matrix

PROBLEM: While these seems to be working, it's not possible to invert
a matrix with perspective because a[icol][icol] == 0 in the W case.
May have to think approach to finding where point that looking at interestcts
what object.

inputs
   CMatrix     *pDest - destination
*/

void CMatrix::Invert4 (CMatrix *pDest)
{

   // determine the adjuunct
   CMatrix m;
   DWORD x,y;
   for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
      m.p[y][x] = CoFactor (y, x);

   // figure out the determinant
   fp fDet = 0;
   for (y = 0; y < 4; y++)
      fDet += p[y][0] * m.p[y][0];// * ((y%2) ? -1 : 1);
   if (fDet)
      fDet = 1.0 / fDet;

#if 0 // def _DEBUG  // to test
   CMatrix mOldMethod;
   mOldMethod.Copy (this);
   Invert4Old (&mOldMethod);
#endif

   // scale and replace
   for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) {
      pDest->p[y][x] = m.p[x][y] * fDet; // NOTE: Transposing it
         // BUGFIX - need to muliply by determinant

#if 0 //def _DEBUG  // to test
      if (fabs(pDest->p[y][x] - mOldMethod.p[y][x]) > 0.01) {
         char szTemp[256];
         sprintf (szTemp, "\r\n\t%g: %g to %g", (double)(pDest->p[y][x] - mOldMethod.p[y][x]),
            (double)mOldMethod.p[y][x], (double)pDest->p[y][x]);
         OutputDebugString (szTemp);
      }
#endif
   }

#if 0 // def _DEBUG
   CMatrix mOne;
   mOne.Multiply (this, pDest);
   for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
      if (fabs ( ((y == x) ? 1.0 : 0.0) - mOne.p[y][x]) > 0.01)
         OutputDebugString ("Not inverse");
   
#endif
}


void CMatrix::MakeSquareOld (void)
{
   CPoint   ap[4];
   ap[0].Copy ((PCPoint) p[0]);
   ap[1].Copy ((PCPoint) p[1]);
   ap[2].Copy ((PCPoint) p[2]);

   // normalize these
   fp   l;
   DWORD i;
   for (i = 0; i < 3; i++) {
      l = sqrt(ap[i].p[0] * ap[i].p[0] + ap[i].p[1] * ap[i].p[1] + ap[i].p[2] * ap[i].p[2]);
      if (l == 1.0)
         continue;
      l = 1.0 / l;
      ap[i].p[0] *= l;
      ap[i].p[1] *= l;
      ap[i].p[2] *= l;
   }

   // cross product to get p[3]
   ap[3].CrossProd (&ap[0], &ap[1]);
   ap[3].p[3] = ap[2].p[3];

   // copy back
   ((PCPoint)p[0])->Copy (&ap[0]);
   ((PCPoint)p[1])->Copy (&ap[1]);
   ((PCPoint)p[2])->Copy (&ap[3]); // get rid of old last one
}

/*******************************************************************
CMatrix::Rotation - produces a rotation matrix

inputs
   fp   xrot, yrot, zrot - angle in radians to rotate
            about the mentioned axis
*/
void CMatrix::Rotation (fp xrot, fp yrot, fp zrot)
{
   fp   CosY, SinY, CosX, SinX, CosZ, SinZ;

   Zero();

   CosY = cos(yrot);
   SinY = sin(yrot);
   CosX = cos(xrot);
   SinX = sin(xrot);
   CosZ = cos(zrot);
   SinZ = sin(zrot);

   p[0][0] = CosY * CosZ;
   p[1][0] = -CosY * SinZ;
   p[2][0] = SinY;

   p[0][1] = CosX * SinZ + SinX * SinY * CosZ;
   p[1][1] = CosX * CosZ - SinX * SinY * SinZ;
   p[2][1] = -SinX * CosY;

   p[0][2] = SinX * SinZ - CosX * SinY * CosZ;
   p[1][2] = SinX * CosZ + CosX * SinY * SinZ;
   p[2][2] = CosX * CosY;

   p[3][3] = 1;   // set scaling

}

void CMatrix::RotationX (fp xrot)
{
   fp   CosY, SinY, CosX, SinX, CosZ, SinZ;

   Zero();

   CosY = 1;
   SinY = 0;
   CosX = cos(xrot);
   SinX = sin(xrot);
   CosZ = 1;
   SinZ = 0;

   p[0][0] = CosY * CosZ;
   p[1][0] = -CosY * SinZ;
   p[2][0] = SinY;

   p[0][1] = CosX * SinZ + SinX * SinY * CosZ;
   p[1][1] = CosX * CosZ - SinX * SinY * SinZ;
   p[2][1] = -SinX * CosY;

   p[0][2] = SinX * SinZ - CosX * SinY * CosZ;
   p[1][2] = SinX * CosZ + CosX * SinY * SinZ;
   p[2][2] = CosX * CosY;

   p[3][3] = 1;   // set scaling


}
void CMatrix::RotationY (fp yrot)
{
   fp   CosY, SinY, CosX, SinX, CosZ, SinZ;

   Zero();

   CosY = cos(yrot);
   SinY = sin(yrot);
   CosX = 1;
   SinX = 0;
   CosZ = 1;
   SinZ = 0;

   p[0][0] = CosY * CosZ;
   p[1][0] = -CosY * SinZ;
   p[2][0] = SinY;

   p[0][1] = CosX * SinZ + SinX * SinY * CosZ;
   p[1][1] = CosX * CosZ - SinX * SinY * SinZ;
   p[2][1] = -SinX * CosY;

   p[0][2] = SinX * SinZ - CosX * SinY * CosZ;
   p[1][2] = SinX * CosZ + CosX * SinY * SinZ;
   p[2][2] = CosX * CosY;

   p[3][3] = 1;   // set scaling

}
void CMatrix::RotationZ (fp zrot)
{
   fp   CosY, SinY, CosX, SinX, CosZ, SinZ;

   Zero();

   CosY = 1;
   SinY = 0;
   CosX = 1;
   SinX = 0;
   CosZ = cos(zrot);
   SinZ = sin(zrot);

   p[0][0] = CosY * CosZ;
   p[1][0] = -CosY * SinZ;
   p[2][0] = SinY;

   p[0][1] = CosX * SinZ + SinX * SinY * CosZ;
   p[1][1] = CosX * CosZ - SinX * SinY * SinZ;
   p[2][1] = -SinX * CosY;

   p[0][2] = SinX * SinZ - CosX * SinY * CosZ;
   p[1][2] = SinX * CosZ + CosX * SinY * SinZ;
   p[2][2] = CosX * CosY;

   p[3][3] = 1;   // set scaling

}

/*******************************************************************
CMatrix::Translation - fills in a translation matrix

inputs
   fp   x,y,z - translation
*/
void CMatrix::Translation (fp x, fp y, fp z)
{
   Identity ();

   p[3][0] = x;
   p[3][1] = y;
   p[3][2] = z;
}


/*******************************************************************
CMatrix::Scale - fills in a scale matrix

inputs
   fp   x,y,z - translation
*/
void CMatrix::Scale (fp x, fp y, fp z)
{
   Identity ();

   p[0][0] = x;
   p[1][1] = y;
   p[2][2] = z;
}


/*******************************************************************
CMatrix::Perspect - fills in a perspective matrix

  |   d  0  0  0  |
  |   0  d  0  0  |
  |   0  0  a  b  |
  |   0  0  -1 0  |

  d = cot(field-of-view / 2)
  a = 1 / (1-(znear/zfar_inv))
  b = -a * znear

inputs
  fp    fov - field of view
  fp    znear - nearest z
  fp    zfar_inv - inverted z-far
*/
void CMatrix::Perspect (fp fov, fp znear, fp zfar_inv)
{
   fp a,b,d;

   Zero();

   if (fov == 0.0)
      return;  // no 0 fov
   if (znear * zfar_inv == 1.0)
      return;  // cant have this either

   d = 1.0 / tan(fov / 2.0);
   a = 1.0 / (1.0 - (znear * zfar_inv));
   b = -a * znear;

   p[0][0] = p[1][1] = d;
   p[2][3] = -1;
   p[2][2] = a;
   p[3][2] = b;
}



/*******************************************************************************
CRenderMatrix - C++ class for doing common matrix operations for the renderer.
*/

/*******************************************************************************
CRenderMatrix::Constructor and destructor
*/
CRenderMatrix::CRenderMatrix (void)
{
#ifdef MALLOCOPT
   DWORD i;
   m_listStack.Init (sizeof(i), &i, 1);   // to find malloc problems
#endif

   ClearAll();
}

CRenderMatrix::~CRenderMatrix (void)
{
   // this space intentionally left blank
}


/*******************************************************************************
CRenderMatrix::ClearAll - just to make sure it's all clear
*/
void CRenderMatrix::ClearAll (void)
{
   m_listStack.Init (sizeof(CMatrix));
   m_CTM.Identity();
   m_mainMatrix.Identity();
   m_persMatrix.Identity();
   m_scaleMatrix.Identity();
   m_transMatrixGlasses.Identity();
   m_perspNScale.Identity();
   m_normalMatrix.Identity();
   m_CTMInverse4.Identity();
   m_fCTMInverse4Valid = FALSE;
   m_fMatrixPerspective = FALSE;
   m_fMatrixCamera = FALSE;
   m_fMatrixScale = FALSE;
   m_fMatrixPerspNScale = FALSE;
   m_fMatrixNormalValid = FALSE;
   m_fCTMDirty = TRUE;
}

/*******************************************************************************
CRenderMatrix::CloneTo - Copy all the information to another render matrix,
effectively cloinin

inputs
   CRenderMatrix     *pClone - Copy to
*/
void CRenderMatrix::CloneTo (CRenderMatrix *pClone)
{
   pClone->m_listStack.Init (sizeof(CMatrix), m_listStack.Get(0), m_listStack.Num());
   pClone->m_CTM.Copy (&m_CTM);
   pClone->m_mainMatrix.Copy (&m_mainMatrix);
   pClone->m_persMatrix.Copy (&m_persMatrix);
   pClone->m_scaleMatrix.Copy (&m_scaleMatrix);
   pClone->m_transMatrixGlasses.Copy (&m_transMatrixGlasses);
   pClone->m_perspNScale.Copy (&m_perspNScale);
   pClone->m_normalMatrix.Copy (&m_normalMatrix);
   pClone->m_CTMInverse4.Copy (&m_CTMInverse4);
   pClone->m_fCTMInverse4Valid = m_fCTMInverse4Valid;
   pClone->m_fCTMDirty = m_fCTMDirty;
   pClone->m_fMatrixPerspective = m_fMatrixPerspective;
   pClone->m_fMatrixCamera = m_fMatrixCamera;
   pClone->m_fMatrixScale = m_fMatrixScale;
   pClone->m_fMatrixPerspNScale = m_fMatrixPerspNScale;
   pClone->m_fMatrixNormalValid = m_fMatrixNormalValid;

}

static PWSTR gpszCTM = L"CTM";
static PWSTR gpszMainMatrix = L"MainMatrix";
static PWSTR gpszPersMatrix = L"PersMatrix";
static PWSTR gpszScaleMatrix = L"ScaleMatrix";
static PWSTR gpszTransMatrixGlasses = L"TransMatrixGlasses";
static PWSTR gpszPerspNScale = L"PerspNScale";
static PWSTR gpszNormalMatrix = L"NormalMatrix";
static PWSTR gpszCTMInverse4 = L"CTMInverse4";
static PWSTR gpszCTMInverse4Valid = L"CTMInverse4Valid";
static PWSTR gpszCTMDirty = L"CTMDirty";
static PWSTR gpszMatrixPerspective = L"MatrixPerspective";
static PWSTR gpszMatrixCamera = L"MatrixCamera";
static PWSTR gpszMatrixScale = L"MatrixScale";
static PWSTR gpszMatrixPerspNScale = L"MatrixPerspNScale";
static PWSTR gpszMatrixNormalValid = L"MatrixNormalValid";

/*******************************************************************************
CRenderMatrix::MMLTo - writes out information to MML node

returns
   PCMMLNode2      - node
*/
PCMMLNode2 CRenderMatrix::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   DWORD i;
   WCHAR szTemp[64];
   // NOTE: Nead to clear stack in MMLFrom
   for (i = 0; i < m_listStack.Num(); i++) {
      swprintf (szTemp, L"Stack%d", (int) i);
      PCMatrix pm;
      pm = (PCMatrix) m_listStack.Get (i);
      MMLValueSet (pNode, szTemp, pm);
   }

   MMLValueSet (pNode, gpszCTM, &m_CTM);
   MMLValueSet (pNode, gpszMainMatrix, &m_mainMatrix);
   MMLValueSet (pNode, gpszPersMatrix, &m_persMatrix);
   MMLValueSet (pNode, gpszScaleMatrix, &m_scaleMatrix);
   MMLValueSet (pNode, gpszTransMatrixGlasses, &m_transMatrixGlasses);
   MMLValueSet (pNode, gpszPerspNScale, &m_perspNScale);
   MMLValueSet (pNode, gpszNormalMatrix, &m_normalMatrix);
   MMLValueSet (pNode, gpszCTMInverse4, &m_CTMInverse4);
   MMLValueSet (pNode, gpszCTMInverse4Valid, (int) m_fCTMInverse4Valid);
   MMLValueSet (pNode, gpszCTMDirty, (int) m_fCTMDirty);
   MMLValueSet (pNode, gpszMatrixPerspective, (int) m_fMatrixPerspective);
   MMLValueSet (pNode, gpszMatrixCamera, (int) m_fMatrixCamera);
   MMLValueSet (pNode, gpszMatrixScale, (int) m_fMatrixScale);
   MMLValueSet (pNode, gpszMatrixPerspNScale, (int) m_fMatrixPerspNScale);
   MMLValueSet (pNode, gpszMatrixNormalValid, (int) m_fMatrixNormalValid);

   return pNode;
}


/*******************************************************************************
CRenderMatrix::MMLFrom - Reads matrix information from MML

inputs
   PCMMLNode2      pNode - node
returns
   BOOL - TRUE if successful
*/
BOOL CRenderMatrix::MMLFrom (PCMMLNode2 pNode)
{

   DWORD i;
   WCHAR szTemp[64];
   m_listStack.Clear();
   CMatrix Ident;
   Ident.Identity();
   for (i = 0; ; i++) {
      swprintf (szTemp, L"Stack%d", (int) i);
      CMatrix m;
      if (!MMLValueGetMatrix (pNode, szTemp, &m, &Ident))
         break;
      m_listStack.Add (&m);
   }

   MMLValueGetMatrix (pNode, gpszCTM, &m_CTM, &Ident);
   MMLValueGetMatrix (pNode, gpszMainMatrix, &m_mainMatrix, &Ident);
   MMLValueGetMatrix (pNode, gpszPersMatrix, &m_persMatrix, &Ident);
   MMLValueGetMatrix (pNode, gpszScaleMatrix, &m_scaleMatrix, &Ident);
   MMLValueGetMatrix (pNode, gpszTransMatrixGlasses, &m_transMatrixGlasses, &Ident);
   MMLValueGetMatrix (pNode, gpszPerspNScale, &m_perspNScale, &Ident);
   MMLValueGetMatrix (pNode, gpszNormalMatrix, &m_normalMatrix, &Ident);
   MMLValueGetMatrix (pNode, gpszCTMInverse4, &m_CTMInverse4, &Ident);
   m_fCTMInverse4Valid = (BOOL) MMLValueGetInt (pNode, gpszCTMInverse4Valid, (int) 0);
   m_fCTMDirty = (BOOL) MMLValueGetInt (pNode, gpszCTMDirty, (int) 0);
   m_fMatrixPerspective = (BOOL) MMLValueGetInt (pNode, gpszMatrixPerspective, (int) 0);
   m_fMatrixCamera = (BOOL) MMLValueGetInt (pNode, gpszMatrixCamera, (int) 0);
   m_fMatrixScale = (BOOL) MMLValueGetInt (pNode, gpszMatrixScale, (int) 0);
   m_fMatrixPerspNScale = (BOOL) MMLValueGetInt (pNode, gpszMatrixPerspNScale, (int) 0);
   m_fMatrixNormalValid = (BOOL) MMLValueGetInt (pNode, gpszMatrixNormalValid, (int) 0);

   return TRUE;
}
/*******************************************************************************
CRenderMatrix::MakeMainMatrix - Regenerates the main matrix if it hasn't
already been generated with the recent data.
*/
void CRenderMatrix::MakeMainMatrix (void)
{
   // if clean then don't do anything
   if (!m_fCTMDirty)
      return;

   // if no perspective and scale matrix then just use the CTM
   if (!m_fMatrixPerspNScale)
      m_mainMatrix.Copy (&m_CTM);
   else {
      m_mainMatrix.Multiply (&m_perspNScale, &m_CTM);
   }
   m_fCTMDirty = FALSE;
   m_fMatrixNormalValid = FALSE;
   m_fCTMInverse4Valid = FALSE;

}

/*******************************************************************************
CRenderMatrix::MakeNormalMatrix - Regenerates the normals matrix if it hasn't
already been generated with the recent data.
*/
void CRenderMatrix::MakeNormalMatrix (void)
{
   // build the main matrix just in case. Also, if m_fCTM dirty was set this
   // will rebuild the main matrix and set m_fMatrixNormalValid to false
   MakeMainMatrix ();

   // if clean then don't do anything
   if (m_fMatrixNormalValid)
      return;

   m_fMatrixNormalValid = TRUE;

   m_CTM.Invert (&m_normalMatrix);
   m_normalMatrix.Transpose();

   // normalize the 3 vectors so rotations of normal vectors don't change the length
   // BUGFIX - I think the old code is wrong, so fixing
   m_normalMatrix.MakeSquare();
   //DWORD x;
   //for (x = 0; x < 3; x++) {
   //   ((PCPoint)m_normalMatrix.p[x])->Normalize();
   //}
}


/*******************************************************************************
CRenderMatrix::TransformViewSpaceToWorldSpace - Multiplies the point (assumed
to be in viewer space (x,y correlating to screen x,y, z being negative, and w correlating
to z) to world space. Use this as part of a process to go from screen coordinates
into world coordinates.

inputs
   PCPoint     pSrc - source point
   PCPoint     pDest - Write out world-space point
*/
void CRenderMatrix::TransformViewSpaceToWorldSpace (const PCPoint pSrc, PCPoint pDest)
{
   MakeInverse4Matrix();

   m_CTMInverse4.Multiply (pSrc, pDest);
}


/*******************************************************************************
CRenderMatrix::MakeInverse4Matrix - Regenerates the inverse4 matrix if it hasn't
already been generated with the recent data.
*/
void CRenderMatrix::MakeInverse4Matrix (void)
{
   // build the main matrix just in case. Also, if m_fCTM dirty was set this
   // will rebuild the main matrix and set m_fMatrixNormalValid to false
   MakeMainMatrix ();

   // if clean then don't do anything
   if (m_fCTMInverse4Valid)
      return;

   m_fCTMInverse4Valid = TRUE;

   m_CTM.Invert4 (&m_CTMInverse4);
   //m_CTMInverse4.Transpose();

#if 0 // dont think I need this for this inversion
   // normalize the 3 vectors so rotations of normal vectors don't change the length
   DWORD x;
   for (x = 0; x < 3; x++) {
      ((PCPoint)m_normalMatrix.p[x])->Normalize();
   }
#endif // 0
}

/*******************************************************************************
CRenderMatrix::Push - Pushes the current m_CTM onto the stack.

inputs
   none
returns
   none
*/
void CRenderMatrix::Push (void)
{
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_listStack.Add (&m_CTM);
	MALLOCOPT_RESTORE;
}

/*******************************************************************************
CRenderMatrix::Pop - Pops the last pushed m_CTM matrix from the stack.

inputs
   none
returns
   BOOL - TRUE if something was on the stack to pop
*/
BOOL CRenderMatrix::Pop (void)
{
   DWORD dwNum = m_listStack.Num();
   if (!dwNum)
      return FALSE;

   PCMatrix p;
   p = (PCMatrix) m_listStack.Get(dwNum-1);
   Set (p);
   m_listStack.Remove (dwNum-1);

   return TRUE;
}

/*******************************************************************************
CRenderMatrix::Set - Sets the current CTM matrix to whatever is point at in pSet.

inputs
   PCMatrix    pSet - what to set the CTM to. Note: This does not change the perspective
                  matrix, etc.
returns
   none
*/
void CRenderMatrix::Set (const PCMatrix pSet)
{
   m_CTM.Copy (pSet);
   m_fCTMDirty = TRUE;
}


/*******************************************************************************
CRenderMatrix::Get - Copies the current matrix (excluding perspective and such)
   to pGet.

inputs
   PCMatrix    pGet - Fills this in
returns
   none
*/
void CRenderMatrix::Get (PCMatrix pGet)
{
   pGet->Copy (&m_CTM);
}

/*******************************************************************************
CRenderMatrix::Multiply - Multiplies the current matrix, CTM, with the new matrix
   pm. CTM = CTM x pm.

inputs
   PCMatrix    pm - Matrix on the right
returns
   none
*/
void CRenderMatrix::Multiply (const PCMatrix pm)
{
   CMatrix temp;

   temp.Multiply (&m_CTM, pm);
   Set (&temp);
}


/*******************************************************************************
CRenderMatrix::Translate - Multiplies a translation matrix on the right hand side
of the current CTM.

inputs
   fp   x,y,z - XYZ translation
returns
   none
*/
void CRenderMatrix::Translate (fp x, fp y, fp z)
{
   CMatrix temp, trans;

   trans.Translation (x, y,z);
   temp.Multiply (&m_CTM, &trans);
   Set (&temp);
}

/*******************************************************************************
CRenderMatrix::Scale - Right-multiplies by a scale matrix.

inputs
   fp      x,y,z - XYZ scaling
   or
   fp      fScale - Scale x, y, and z by the same amount
returns
   none
*/
void CRenderMatrix::Scale (fp x, fp y, fp z)
{
   CMatrix temp, scale;

   scale.Zero();
   scale.p[0][0] = x;
   scale.p[1][1] = y;
   scale.p[2][2] = z;
   scale.p[3][3] = 1.0;

   temp.Multiply (&m_CTM, &scale);
   Set (&temp);
}
void CRenderMatrix::Scale (fp fScale)
{
   Scale (fScale, fScale, fScale);
}

/*******************************************************************
CRenderMatrix::Rotate - rotate

inputs
   fp   angle - angle in radians
   int      axis - 1 for x, 2 for y, 3 for z
*/
void CRenderMatrix::Rotate (fp angle, DWORD axis)
{
   CMatrix   temp, temp2;

   switch (axis) {
   case 1:
      temp.RotationX (angle);
      break;
   case 2:
      temp.RotationY (angle);
      break;
   case 3:
      temp.RotationZ (angle);
      break;
   default:
      return;
   }

   temp2.Multiply (&m_CTM, &temp);
   Set (&temp2);

}

/*******************************************************************
CRenderMatrix::TransRot - Translate to p1 and then rotate so that looking at p2.

inputs
   pnt      p1 - where to move to
   pnt      p2 - point to be looking at
*/
void CRenderMatrix::TransRot (const PCPoint p1, const PCPoint p2)
{
   CPoint p;
   BOOL fNorm;
   p.Subtract (p2, p1);
   fNorm = p.Normalize();

   // translate
   Translate (p1->p[0], p1->p[1], p1->p[2]);

   // return if cant really rotate
   if (!fNorm)
      return;

   // rotate around y & x
   fp   fAroundY, fAroundX;
   CMatrix   temp, temp2, temp3;

#if 0
   fp ft[3][3];
   int x, y;
   for (x = 0; x < 3; x++) for (y = 0; y < 3; y++)
      ft[x][y] = atan2(y - 1, x-1);
#endif

   if ((p.p[0] == 0.0) && (p.p[2] == 0.0))
      fAroundY = 0;
   else
      fAroundY = atan2 (p.p[0], -p.p[2]);
   if (fAroundY)
      temp.RotationY (fAroundY);
   else
      temp.Identity ();

   // apply the translation and see how much we have to do around x
   CPoint pRot;
   p.p[3] = 1;
   temp.Multiply (&p, &pRot);
   if ((pRot.p[1] == 0.0) && (pRot.p[2] == 0.0))
      fAroundX = 0;
   else
      fAroundX = atan2 (-pRot.p[1], -pRot.p[2]);
   if (fAroundX) {
      temp2.RotationX (fAroundX);

#if 0
      pnt   p3;
      MultiplyMatrixPnt (temp2, pRot, p3);
#endif
      temp3.Multiply (&temp2, &temp);
   }
   else {
      temp3.Copy (&temp);
   }

#if 0
   MultiplyMatrixPnt (temp3, p, pRot);
#endif
   temp3.Invert (&temp2);

   // apply this
   Multiply (&temp2);
}



#if 0 // tried to fix transrot but it didn't seem to get fixed
/*******************************************************************
CRenderMatrix::TransRot - Translate to p1 and then rotate so that looking at p2.

inputs
   pnt      p1 - where to move to
   pnt      p2 - point to be looking at
*/
void CRenderMatrix::TransRot (const PCPoint p1, const PCPoint p2)
{
   CPoint p;
   BOOL fNorm;
   p.Subtract (p1, p2); // BUGFIX - I flipped these since it seems to work
   fNorm = p.Normalize();

   // translate
   Translate (p1->p[0], p1->p[1], p1->p[2]);

   // return if cant really rotate
   if (!fNorm)
      return;

   // rotate around y & x
   fp   fAroundY, fAroundX;
   CMatrix   temp, tempInv, temp2, temp3;

#if 0
   fp ft[3][3];
   int x, y;
   for (x = 0; x < 3; x++) for (y = 0; y < 3; y++)
      ft[x][y] = atan2(y - 1, x-1);
#endif

   // BUGFIX - translate vector
   TransformNormal (1, &p, FALSE);

   if ((p.p[0] == 0.0) && (p.p[2] == 0.0))
      fAroundY = 0;
   else
      fAroundY = atan2 (p.p[0], -p.p[2]);
   if (fAroundY) {
      temp.RotationY (fAroundY);
      tempInv.RotationY (-fAroundY);
   }
   else {
      temp.Identity ();
      tempInv.Identity();
   }

   // apply the translation and see how much we have to do around x
   CPoint pRot;
   p.p[3] = 1;
   tempInv.Multiply (&p, &pRot);
   if ((pRot.p[1] == 0.0) && (pRot.p[2] == 0.0))
      fAroundX = 0;
   else
      fAroundX = atan2 (-pRot.p[1], -pRot.p[2]);
   if (fAroundX) {
      temp2.RotationX (fAroundX);

#if 0
      pnt   p3;
      MultiplyMatrixPnt (temp2, pRot, p3);
#endif
      temp3.Multiply (&temp2, &temp);
   }
   else {
      temp3.Copy (&temp);
   }

#if 0
   MultiplyMatrixPnt (temp3, p, pRot);
#endif
   temp3.Invert (&temp2);

   // apply this
   Multiply (&temp2);
}
#endif // 0


/************************************************************************************
CRenderMatrix::GetPerspScale - Get the Matrix that goes from world coords to post-perspective.

inputs
   PCMatrix    pPerspScale - Filled with the perspective and scaling
returns
   none
*/
void CRenderMatrix::GetPerspScale (PCMatrix pPerspScale)
{
   MakeMainMatrix ();

   // if no perspective and scale matrix then just use the CTM
   if (!m_fMatrixPerspNScale)
      pPerspScale->Identity();
   else {
      pPerspScale->Copy (&m_perspNScale);
   }
}

/************************************************************************************
CRenderMatrix::Transform - Transform a point from the source to destination. It applies the entire
   transformation matrix (perspective, scaling, CTM, etc.) to the point.

inputs
   PCPoint     pSrc - source point. This sets pSrc->p[3] to 1.0
   PCPoint     pDest - destination point. Different from source.
returns
   none
*/
void CRenderMatrix::Transform (const PCPoint pSrc, PCPoint pDest)
{
   MakeMainMatrix ();

   pSrc->p[3] = 1;
   m_mainMatrix.Multiply (pSrc, pDest);
}

/************************************************************************************
CRenderMatrix::Transform - Transforms points in bulk. They are modified in place.
   Does transformation of perspective, scaling, CTM, etc. all combined.

inputs
   DWORD       dwNum - number of points
   PCPoint     apPoints - An array of dwNum points
returns
   none
*/
void CRenderMatrix::Transform (DWORD dwNum, PCPoint apPoints)
{
   MakeMainMatrix();

   DWORD i;
   CPoint   temp;
   PCPoint  p;
   for (i = 0, p = apPoints; i < dwNum; i++, p++) {
      p->p[3] = 1.0;
      m_mainMatrix.Multiply (p, &temp);
      p->Copy (&temp);
   }
}

/************************************************************************************
CRenderMatrix::TransformNormal - Transform a vector from the source to destination. It applies the entire
   surface-normal transformation matrix to the vector

inputs
   PCPoint     pSrc - source point. This sets pSrc->p[3] to 1.0
   PCPoint     pDest - destination point. Different from source.
   BOOL        fNormalize - If TRUE, all the points are normalized
returns
   none
*/
void CRenderMatrix::TransformNormal (const PCPoint pSrc, PCPoint pDest, BOOL fNormalize)
{
   MakeNormalMatrix ();

   pSrc->p[3] = 1;
   m_normalMatrix.Multiply (pSrc, pDest);
   if (fNormalize)
      pDest->Normalize();
}

/************************************************************************************
CRenderMatrix::TransformNormal - Transforms normal vectors in bulk. They are modified in place.
   Does the surface-normal transformation.

inputs
   DWORD       dwNum - number of points
   PCPoint     apPoints - An array of dwNum points
   BOOL        fNormalize - If TRUE, all the points are normalized
returns
   none
*/
void CRenderMatrix::TransformNormal (DWORD dwNum, PCPoint apPoints, BOOL fNormalize)
{
   MakeNormalMatrix();

   DWORD i;
   CPoint   temp;
   PCPoint  p;
   for (i = 0, p = apPoints; i < dwNum; i++, p++) {
      p->p[3] = 1.0;
      m_normalMatrix.Multiply (p, &temp);
      if (fNormalize)
         temp.Normalize();
      p->Copy (&temp);
   }
}

/************************************************************************************
CRenderMatrix::Perspective - Sets the perspective matrix. If this is not called
then no perspective matrix is used.

inputs
   fp         fFOV - Field of view in radians.
   fp         fZNear - Z Near, for clipping purposes I think
   fp         fZFarInv - Inverse of Z far, for clipping purpsoes.
returns
   none
*/
void CRenderMatrix::Perspective (fp fFOV, fp fZNear, fp fZFarInv)
{
   m_persMatrix.Perspect (fFOV, fZNear, fZFarInv);
   if (m_fMatrixScale) {
      m_perspNScale.Multiply (&m_scaleMatrix, &m_persMatrix);
      m_perspNScale.MultiplyLeft (&m_transMatrixGlasses);   // include 3d glasses
   }
   else
      m_perspNScale.Copy (&m_persMatrix);
   m_fMatrixPerspective = TRUE;
   m_fCTMDirty = TRUE;
   m_fMatrixPerspNScale = TRUE;
}

/************************************************************************************
CRenderMatrix::PerspectiveScale - Used by the flat renderer. Instead of a 1/w matrix,
does x, y, z scaling.

inputs
   fp         fX, fY, fZ - XYZ scaling
returns
   none
*/
void CRenderMatrix::PerspectiveScale (fp fX, fp fY, fp fZ)
{
   m_persMatrix.Zero();
   m_persMatrix.p[0][0] = fX;
   m_persMatrix.p[1][1] = fY;
   m_persMatrix.p[2][2] = fZ;
   m_persMatrix.p[3][3] = 1.0;


   if (m_fMatrixScale) {
      m_perspNScale.Multiply (&m_scaleMatrix, &m_persMatrix);
      m_perspNScale.MultiplyLeft (&m_transMatrixGlasses);  // include 3d glasses
   }
   else
      m_perspNScale.Copy (&m_persMatrix);
   m_fMatrixPerspective = TRUE;
   m_fCTMDirty = TRUE;
   m_fMatrixPerspNScale = TRUE;
}

/************************************************************************************
CRenderMatrix::PerspectiveClear - Clears perspecive to the identity.
*/
void CRenderMatrix::PerspectiveClear (void)
{
   m_persMatrix.Identity();
   if (m_fMatrixScale) {
      m_perspNScale.Multiply (&m_scaleMatrix, &m_persMatrix);
      m_perspNScale.MultiplyLeft (&m_transMatrixGlasses);  // 3d glasses
   }
   else
      m_perspNScale.Copy (&m_persMatrix);
   m_fMatrixPerspective = TRUE;
   m_fCTMDirty = TRUE;
   m_fMatrixPerspNScale = TRUE;
}
/************************************************************************************
CRenderMatrix::Clear - Sets m_CTM to the identity.
*/
void CRenderMatrix::Clear (void)
{
   CMatrix temp;
   temp.Identity();
   Set (&temp);
}


/************************************************************************************
CRenderMatrix::ScreenInfo - Call this (if you're using the perspective matrix)
to tell the CRenderMatrix object what the X,Y resultion of the screen is. This will cause
the X dimension, from 0 to xWidth, to be scaled from -1 to 1. And the Y dimesnion,
from 0 to yWidth to be scaled from 1 to -1. This scaling is useful for clipping later on.

You should call ScreenInfo everytime the screen resoltion changes (assuming that you've
called Perspective()).

inputs
   DWORD       dwWidth - width in pixels
   DWORD       dwHeight - height in pixels
   fp      fXOffset - Offset the camera to the right (or negative for left)
               by this many meters. Do this when doing 3d glasses, offsetting
               to the right 10cm and left 10cm, or so.
returns
   none
*/
void CRenderMatrix::ScreenInfo (DWORD dwWidth, DWORD dwHeight, fp fXOffset)
{
   dwWidth = max(1,dwWidth);
   dwHeight = max(1, dwHeight);
   fp aspect;
   aspect = (fp)dwHeight / (fp)dwWidth;

   m_scaleMatrix.Identity();
   m_scaleMatrix.p[1][1] = 1.0 / aspect;

   // if there's an xOffset may be doing 3d glasses
   if (fXOffset)
      m_transMatrixGlasses.Translation (fXOffset, 0, 0);
   else
      m_transMatrixGlasses.Identity();

   if (m_fMatrixPerspective)
      m_perspNScale.Multiply (&m_scaleMatrix, &m_persMatrix);
   else
      m_perspNScale.Copy (&m_scaleMatrix);
   m_perspNScale.MultiplyLeft (&m_transMatrixGlasses);  // 3d glasses

   m_fMatrixScale = TRUE;
   m_fCTMDirty = TRUE;
   m_fMatrixPerspNScale = TRUE;
}


#if 0 // FOHBOT
/*******************************************************************
lfrom - set the new look-from point
*/
void CRender::lfrom (const pnt p)
{
   CopyPnt (p, plfrom);
}


/*******************************************************************
lat - set new look-at point
*/
void CRender::lat (const pnt p)
{
   CopyPnt (p, plat);
}

/*******************************************************************
lup - set new look-up point
*/
void CRender::lup (const pnt p)
{
   CopyPnt (p, plup);
}


/*******************************************************************
wfore - set the window foreground point
*/
void CRender::wfore (const pnt p)
{
   CopyPnt (p, pwfore);
}


/*******************************************************************
wback - set the window backgroun point
*/
void CRender::wback (const pnt p)
{
   CopyPnt (p, pwback);
}

/*******************************************************************
sfore - set the screen foregroun point
*/
void CRender::sfore (const pnt p)
{
   psfore[0] = p[0];
   psfore[1] = p[1];
}

/*******************************************************************
sback - set the screen background point
*/
void CRender::sback (const pnt p)
{
   psback[0] = p[0];
   psback[1] = p[1];
}

/*******************************************************************
fov - set the field of view
*/
void CRender::fov (fp p)
{
   dfov = p;
}


/*******************************************************************
ell - set the distance from the viewer to the foregroun object
*/
void CRender::ell (fp p)
{
   dell = p;
}

/*******************************************************************
FOHBOT - actually builds the fohbot model.
*/
void CRender::FOHBOT (const pnt lup, const pnt wfore, const pnt wback, const pnt sfore,
                      const pnt sback, fp fov, fp ell)
{
   Matrix R1, R2, R3, T1, R4, R5, T2;
   Matrix M, m, tempM, tempM2;

   int   i,j;

   fp   theta, thetaf, thetab, phi, beta, gamma, alpha, dinv;
   fp   s, epsilon, eta, zeta, lenb, lenu;
   fp   temp, aa, bb, cc;
   pnt      b, u;

   // compute R1 which is a rotation about the z axis by theta
   thetaf = atan2 (sfore[1], sfore[0]);
   ZeroMatrix (R1);
   IdentityMatrix (R1);
   R1[0][0] = R1[1][1] = cos(thetaf);
   R1[1][0] = -(R1[0][1] = sin(thetaf));

   // compute R2
   dinv = tan(fov/2);
   phi = atan(sqrt(sfore[0]*sfore[0] + sfore[1] * sfore[1]) * dinv);
   ZeroMatrix (R2);
   IdentityMatrix (R2);
   R2[0][0] = R2[2][2] = cos(phi);
   R2[2][0] = -(R2[0][2] = sin(phi));

   // compute R3
   thetab = atan2 (sback[1], sback[0]);
   beta = atan(sqrt(sback[0] * sback[0] + sback[1] * sback[1]) * dinv);
   theta = thetaf - thetab;
   temp = cos(phi) * cos(beta) + sin(phi) * sin(beta) * cos(theta);
   if ((temp > 1.0) || (temp < -1.0))
      return;  // error
   gamma = acos(temp);
   alpha = atan2(sin(beta) * sin(theta) * sin(phi),
      cos(beta) - cos(gamma) * cos(phi));
   if (cos(beta) == (cos(gamma) * cos(phi)))
      alpha = PI / 2.0;
   ZeroMatrix (R3);
   IdentityMatrix (R3);
   R3[0][0] = R3[1][1] = -cos(alpha);
   R3[0][1] = -(R3[1][0] = sin(alpha));

   // compute T1
   ZeroMatrix (T1);
   IdentityMatrix (T1);
   T1[3][2] = -ell;

   // compute R4
   s = sqrt (  (wback[0] - wfore[0]) * (wback[0] - wfore[0]) +
               (wback[1] - wfore[1]) * (wback[1] - wfore[1]) +
               (wback[2] - wfore[2]) * (wback[2] - wfore[2]));
   if (s == 0.0)
      return;
   temp = (ell / s * sin(gamma));
   if ((temp < -1.0) || (temp > 1.0))
      epsilon = PI / 2.0;  // hack
   else
      epsilon = asin(temp);
   ZeroMatrix (R4);
   IdentityMatrix (R4);
   R4[0][0] = R4[2][2] = sin(gamma + epsilon);
   R4[0][2] = -(R4[2][0] = cos(gamma + epsilon));

   // compute R5
   MultiplyMatrices (R1, R2, tempM);
   MultiplyMatrices (tempM, R3, M);
   MultiplyMatrices (M, T1, tempM);
   MultiplyMatrices (tempM, R4, M);
   SubVector (wback, wfore, b);
   b[3] = 0.0;
   lenb = sqrt(DotProd(b, b));
   if (lenb == 0.0)
      return;
   for (i = 0; i <= 2; b[i++] /= lenb);
   CopyPnt (lup, u);
   u[3] = 0.0;
   lenu = sqrt(DotProd(u, u));
   if (lenu == 0.0)
      return;
   for (i = 0; i <= 2; u[i++] /= lenu);
   zeta = atan2(M[0][2], M[0][1]);
   temp = sqrt(M[0][1] * M[0][1] + M[0][2] * M[0][2]);
   if (temp == 0.0)
      return;
   temp = DotProd(b, u) / temp;
   if ((temp < -1.0) || (temp > 1.0))
      return;
   temp = acos (temp);
   eta = max(zeta + temp, zeta - temp);
   aa = M[0][1] * cos(eta) + M[0][2] * sin(eta);
   bb = M[1][1] * cos(eta) + M[1][2] * sin(eta);
   cc = M[2][1] * cos(eta) + M[2][2] * sin(eta);
   ZeroMatrix (tempM);
   IdentityMatrix (tempM);
   CopyMatrix (tempM, m);
   CopyMatrix (tempM, tempM2);
   tempM[1][0] = aa;
   tempM[1][1] = tempM[2][2] = bb;
   tempM[2][1] = -(tempM[1][2] = cc);
   for (i = 0; i <= 2; i++) {
      m[0][i] = b[i];
      m[1][i] = u[i];
   }
   m[2][0] = b[1]*u[2] - b[2]*u[1];
   m[2][1] = b[2]*u[0] - b[0]*u[2];
   m[2][2] = b[0]*u[1] - b[1]*u[0];

   // invert m into tempM2
   tempM2[0][0] = m[2][2] * m[1][1] - m[1][2] * m[2][1];
   tempM2[0][1] = m[0][2] * m[2][1] - m[2][2] * m[0][1];
   tempM2[0][2] = m[1][2] * m[0][1] - m[0][2] * m[1][1];

   tempM2[1][0] = m[1][2] * m[2][0] - m[2][2] * m[1][0];
   tempM2[1][1] = m[2][2] * m[0][0] - m[0][2] * m[2][0];
   tempM2[1][2] = m[0][2] * m[1][0] - m[1][2] * m[0][0];

   tempM2[2][0] = m[2][1] * m[1][0] - m[1][1] * m[2][0];
   tempM2[2][1] = m[0][1] * m[2][0] - m[2][1] * m[0][0];
   tempM2[2][2] = m[1][1] * m[0][0] - m[0][1] * m[1][0];

   temp = Determinant (m);
   if (temp == 0.0)
      return;
   for (i = 0; i <= 2; i++)
      for (j = 0; j <= 2; j++)
         tempM2[i][j] /= temp;
   MultiplyMatrices (tempM, tempM2, R5);

   // compute T2
   ZeroMatrix (T2);
   IdentityMatrix (T2);
   for (i = 0; i <= 2; i++)
      T2[3][i] = -wfore[i];

   // compute camera
   MultiplyMatrices (M, R5, tempM);
   MultiplyMatrices (tempM, T2, camera);
   MultiplyMatrices (CTM, camera, tempM);
   CopyMatrix (tempM, CTM);
   MakeMainMatrix ();
   
}


/*******************************************************************
build_camera - builds a look-at/look-from camera model
*/
void CRender::build_camera (void)
{
   pnt   zero;

   zero[0] = zero[1] = zero[2] = 0.0;

   FOHBOT (plup, plfrom, plat, zero, zero, dfov, 0.0);
}


/*******************************************************************
build_FOHBOT - vuilds a new FOHBOT camera model
*/
void CRender::build_FOHBOT (void)
{
   FOHBOT (plup, pwfore, pwback, psfore, psback, dfov, dell);
}

#endif 0


