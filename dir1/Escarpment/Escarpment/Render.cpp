/*******************************************************************
Render.cpp - rendering code.

Begun 4/30/99. Some code copied from cs174
*/

#include "mymalloc.h"
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "resource.h"
#include "tools.h"
#include "resleak.h"

#define  EPSILON  0.000001
#define  MESH(x,y,z) ((x + (y) * dwAcross)*4 + z)


HFONT MyCreateFontIndirect (LOGFONT *pl);
void MyDeleteObject (HFONT hFont);

/*******************************************************************
myfmod - Does an fmod the right way.

inputs
   double   x,y - x mod y
returns
   double - value
*/
inline double myfmod (double x, double y)
{
   return  x - floor (x/y) * y;
}


/*******************************************************************
ZeroMatrix - zeros a matrix
*/
inline void ZeroMatrix (Matrix m)
{
   memset (m, 0, sizeof(Matrix));
}

/*******************************************************************
TransposeMatix - transposes a matrix.

inputs
   Matix m - input
   Matrix out - output
*/
inline void TransposeMatrix (const Matrix m, Matrix out)
{
   int   i,j;

   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++) {
         out[i][j] = m[j][i];
         out[j][i] = m[i][j];
      }
}


/*******************************************************************
Determinant - takes the determinant of a matric
*/
inline double Determinant (const Matrix m)
{
   return   (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * m[2][0] +
            (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * m[2][1] +
            (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * m[2][2];
}

/*******************************************************************
InvertMatrix - Inverts the 3x3 part of the marix (not the homogenous)

inputs
   Matix m - to invert
   Matix out - inverted
*/
void InvertMatrix (const Matrix m, Matrix out)
{
   double temp;
   int i,j;

   out[3][0] = out[3][1] = out[3][2] = 0.0;
   out[0][3] = out[1][3] = out[2][3] = 0.0;
   out[3][3] = 1.0;

   // invert m into out
   out[0][0] = m[2][2] * m[1][1] - m[1][2] * m[2][1];
   out[0][1] = m[0][2] * m[2][1] - m[2][2] * m[0][1];
   out[0][2] = m[1][2] * m[0][1] - m[0][2] * m[1][1];

   out[1][0] = m[1][2] * m[2][0] - m[2][2] * m[1][0];
   out[1][1] = m[2][2] * m[0][0] - m[0][2] * m[2][0];
   out[1][2] = m[0][2] * m[1][0] - m[1][2] * m[0][0];

   out[2][0] = m[2][1] * m[1][0] - m[1][1] * m[2][0];
   out[2][1] = m[0][1] * m[2][0] - m[2][1] * m[0][0];
   out[2][2] = m[1][1] * m[0][0] - m[0][1] * m[1][0];

   temp = Determinant (m);
   if (temp == 0)
      return;   // cant invert

   for (i = 0; i < 3; i++)
      for (j = 0; j < 3; j++)
         out[i][j] /= temp;
}

/*******************************************************************
MultiplyMatrixPnt - Multiply a matrix by a point

inputs
   Matrix      left
   pnt         right
   pnt         new. changed. new != right
*/
inline void MultiplyMatrixPnt (const Matrix left, const pnt right, pnt newP)
{
#define  MMP(i)   newP[i] = left[0][i] * right[0] + left[1][i] * right[1] + left[2][i] * right[2] + left[3][i] * right[3]

   MMP(0);
   MMP(1);
   MMP(2);
   MMP(3);
}


/*******************************************************************
AddMatrices - Adds matrices together

inputs
   Matrix      left, right - two matrices
   Matric      newM - new one
*/
inline void AddMatrices (const Matrix left, const Matrix right, Matrix newM)
{
   int   i,j;
   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
         newM[i][j] = left[i][j] + right[i][j];
}


/*******************************************************************
CopyMatrix - copies a matric

inputs
   Matrix      source
   Matrix      dest
*/
inline void CopyMatrix (const Matrix source, Matrix dest)
{
   memcpy (dest, source, sizeof(Matrix));
}



#if 0
/*******************************************************************
CopyPnt - copies a point

inputs
   pnt         source
   pnt         dest
*/
inline void CopyPnt (const pnt source, pnt dest)
{
   memcpy (dest, source, sizeof(pnt));
}
#endif // 0

/*******************************************************************
CrossProd - takes the cross product of two vectors

inputs
   pnt      left, right - left and right points
   pnt      out - output. != left,right
*/
inline void CrossProd (const pnt left, const pnt right, pnt out)
{
   out[0] = left[1] * right[2] - left[2] * right[1];
   out[1] = left[2] * right[0] - left[0] * right[2];
   out[2] = left[0] * right[1] - left[1] * right[0];
}



/*******************************************************************
MakeMatrixSquare - Makes sure the rotation part of the matrix is all
   perpendicular, and that they're all normalized
*/
void MakeMatrixSquare (Matrix m)
{
   pnt   p[4];
   CopyPnt (m[0], p[0]);
   CopyPnt (m[1], p[1]);
   CopyPnt (m[2], p[2]);

   // normalize these
   double   l;
   DWORD i;
   for (i = 0; i < 3; i++) {
      l = sqrt(p[i][0] * p[i][0] + p[i][1] * p[i][1] + p[i][2] * p[i][2]);
      if (l == 1.0)
         continue;
      l = 1.0 / l;
      p[i][0] *= l;
      p[i][1] *= l;
      p[i][2] *= l;
   }

   // cross product to get p[3]
   CrossProd (p[0], p[1], p[3]);
   p[3][3] = p[2][3];

#ifdef _DEBUG
   char  szTemp[256];
   sprintf (szTemp, "Diff = %g, %g, %g\r\n", p[3][0]-p[2][0], p[3][1]-p[2][1], p[3][2]-p[2][2]);
   OutputDebugString (szTemp);
#endif

   // copy back
   CopyPnt (p[0], m[0]);
   CopyPnt (p[1], m[1]);
   CopyPnt (p[3], m[2]);   // get rid of old last one
}


/*******************************************************************
DotProd - takes the dot product of two vectors

inputs
   ptn      left, right - left and right points
returns
   double - dot product
*/

inline double DotProd (const pnt left, const pnt right)
{
   return (left[0] * right[0] + left[1] * right[1] + left[2] * right[2]);
}



/*******************************************************************
AddVector - adds two vectors together

inputs
   ptn      left, right - left and right vectors
   ptn      out - answer
*/
inline void AddVector (const pnt left, const pnt right, pnt out)
{
   out[0] = left[0] + right[0];
   out[1] = left[1] + right[1];
   out[2] = left[2] + right[2];
}

/*******************************************************************
AddVector4 - adds two vectors together

inputs
   ptn      left, right - left and right vectors
   ptn      out - answer
*/
inline void AddVector4 (const pnt left, const pnt right, pnt out)
{
   out[0] = left[0] + right[0];
   out[1] = left[1] + right[1];
   out[2] = left[2] + right[2];
   out[3] = left[3]; // + right[3];
   // IMPORTANT - need to do something with right, but not really sure...
}



/*******************************************************************
SubVector - subtracts two vectors together

inputs
   ptn      left, right - left and right vectors
   ptn      out - answer
*/
inline void SubVector (const pnt left, const pnt right, pnt out)
{
   out[0] = left[0] - right[0];
   out[1] = left[1] - right[1];
   out[2] = left[2] - right[2];
}



/*******************************************************************
IdentityMatrix - fills in the identity matrix parts. NOTE: This
   does not actually zero out anything else!

inputs
   Matrix   m - modify
*/
inline void IdentityMatrix (Matrix m)
{
   m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0;
}




/*******************************************************************
RotationMatrix - produces a rotation matrix

inputs
   Matrix   out - filled in
   double   xrot, yrot, zrot - angle in radians to rotate
            about the mentioned axis
*/
void RotationMatrix (Matrix m, double xrot, double yrot, double zrot)
{
   double   CosY, SinY, CosX, SinX, CosZ, SinZ;

   CosY = cos(yrot);
   SinY = sin(yrot);
   CosX = cos(xrot);
   SinX = sin(xrot);
   CosZ = cos(zrot);
   SinZ = sin(zrot);

   m[3][3] = 1;   // set scaling

   m[0][0] = CosY * CosZ;
   m[1][0] = -CosY * SinZ;
   m[2][0] = SinY;
   m[0][1] = CosX * SinZ + SinX * SinY * CosZ;
   m[1][1] = CosX * CosZ - SinX * SinY * SinZ;
   m[2][1] = -SinX * CosY;
   m[0][2] = SinX * SinZ - CosX * SinY * CosZ;
   m[1][2] = SinX * CosZ + CosX * SinY * SinZ;
   m[2][2] = CosX * CosY;

}


/*******************************************************************
TransMatrix - fills in a translation matrix

inputs
   Matrix   m - filled in
   double   x,y,z - translation
*/
void TransMatrix (Matrix m, double x, double y, double z)
{
   IdentityMatrix (m);

   m[3][0] = x;
   m[3][1] = y;
   m[3][2] = z;
}


/*******************************************************************
PerspectMatrix - fills in a perspective matrix

  |   d  0  0  0  |
  |   0  d  0  0  |
  |   0  0  a  b  |
  |   0  0  -1 0  |

  d = cot(field-of-view / 2)
  a = 1 / (1-(znear/zfar_inv))
  b = -a * znear

inputs
  Matrix    m - output
  double    fov - field of view
  double    znear - nearest z
  double    zfar_inv - inverted z-far
*/
void PerspectMatrix (Matrix m, double fov, double znear, double zfar_inv)
{
   double a,b,d;

   if (fov == 0.0)
      return;  // no 0 fov
   if (znear * zfar_inv == 1.0)
      return;  // cant have this either

   d = 1.0 / tan(fov / 2.0);
   a = 1.0 / (1.0 - (znear * zfar_inv));
   b = -a * znear;

   m[0][0] = m[1][1] = d;
   m[2][3] = -1;
   m[2][2] = a;
   m[3][2] = b;
}


/*******************************************************************
CRender::CRender
*/
CRender::CRender (void)
{
   TOS = NULL;
   izn = 0;
   izf = 1;
   aspect = 1; // BUGFIX - I think this fixes a crash that some people have.
               // was causing a divide-by-zero to happen once in awhile, creating infinity,
               // and then a multiply by ininity.

   m_hDCDraw = NULL;
   m_hBitDrawOld = NULL;
   m_hBitDraw = NULL;
   m_bm.bmBits = NULL;
   m_fWriteToMemory = FALSE;
   m_pafZBuf = NULL;
   m_padwObject = NULL;

   m_pdwColorMap = NULL;
   m_pfBumpMap = NULL;
   m_pafMeshPoints = NULL;  // array of points m_dwMeshY x m_dwMeshX x 4
   m_pafMeshNormals = NULL; // normals
   m_pafMeshEast = NULL;    // vector pointing east
   m_pafMeshNorth = NULL;   // vector pointing north
   m_fMeshCheckedBoundary = FALSE;
   m_fMeshShouldHide = FALSE;

   m_hFont = NULL;
   initl1 ();  // mscale is done ehre
   gsinit ();

}


/*******************************************************************
CRender::~CRender
*/
CRender::~CRender (void)
{
   if (m_hFont)
      MyDeleteObject (m_hFont);

   MeshFree();

   ColorMapFree();
   BumpMapFree();

   // turn off device dependent stuff
   endl1 ();

   // clear out matrix buffers
   stkinit ();

   // free the bitmap
   if (m_hDCDraw) {
      if (m_hBitDrawOld)
         SelectObject (m_hDCDraw, m_hBitDrawOld);
      DeleteDC (m_hDCDraw);
      DeleteObject (m_hBitDraw);
   }
   m_hDCDraw = NULL;
   m_hBitDrawOld = NULL;
   m_hBitDraw = NULL;
   if (m_bm.bmBits) {
      ESCFREE (m_bm.bmBits);
      m_bm.bmBits = NULL;
   }
   m_bm.bmBits = NULL;

   // free the z buffer
   if (m_pafZBuf) {
      ESCFREE (m_pafZBuf);
      m_pafZBuf = NULL; // BUGFIX - might fix crash
   }
   if (m_padwObject) {
      ESCFREE (m_padwObject);
      m_padwObject = NULL; // BUGFIX - might fix crash
   }
}


/*******************************************************************
CRender::MakeMainMatrix - makes the main matrix
*/
void CRender::MakeMainMatrix (void)
{
   Matrix   temp;

   MultiplyMatrices (scaleMatrix, persMatrix, perspNScale);
   MultiplyMatrices (perspNScale, CTM, mainMatrix);

   InvertMatrix (CTM, temp);
   TransposeMatrix (temp, normalMatrix);
}

/*******************************************************************
CRender::MatrixSet - Sets the rotation matrix (not the perspective
   or camera one though.)

inputs
   Matrix   m
*/
void CRender::MatrixSet (const Matrix m)
{
   CopyMatrix (m, CTM);
   MakeMainMatrix();
}


/*******************************************************************
CRender::MatrixMultiply - multiplies the curent rotation matrix by
   a new matrix.

inputs
   Matrix   m
*/
void CRender::MatrixMultiply (const Matrix m)
{
   Matrix   temp;

   MultiplyMatrices (CTM, m, temp);
   CopyMatrix (temp, CTM);
   MakeMainMatrix();
}

/*******************************************************************
CRender::MatrixGet - Get the contents of the rotation matrix (not
   the perspective or camera one though.

inputs
   Matrix   m - filled in
*/
void CRender::MatrixGet (Matrix m)
{
   CopyMatrix (CTM, m);
}


/*******************************************************************
CRender::mtransform - take a 3-d point, rotates it arrounds,
   and flattens it out

inputs
   pnt      old - point in rotated space
   ptn      newP - point in normal space
*/
inline void CRender::mtransform (pnt old, pnt newP)
{
   old[3] = 1.0;
   MultiplyMatrixPnt (mainMatrix, old, newP);
}


/*******************************************************************
CRender::mtrans1 - takes a 3-point and rotates it around so that viwer
   is at 0,0,0


inputs
   pnt      old - point in rotated space
   ptn      newP - point in normal space
*/
inline void CRender::mtrans1 (pnt old, pnt newP)
{
   old[3] = 1.0;
   MultiplyMatrixPnt (CTM, old, newP);
}


/*******************************************************************
CRender::mtrans2 - takes a 3-point and adds persepctive


inputs
   pnt      old - point in rotated space
   ptn      newP - point in normal space
*/
inline void CRender::mtrans2 (const pnt old, pnt newP)
{
   MultiplyMatrixPnt (perspNScale, old, newP);
}


/*******************************************************************
CRender::mtransNormal - transforms the normal from world space.
   Use this for all normals. It also normalizes
*/
inline void CRender::mtransNormal (pnt old, pnt newP, BOOL fNormalize)
{
   double   temp;

   old[3] = 1.0;

   MultiplyMatrixPnt (normalMatrix, old, newP);
   if (!fNormalize)
      return;

   temp = sqrt(newP[0] * newP[0] + newP[1] * newP[1] + newP[2] * newP[2]);


   if (temp == 0.0) {
      memset (newP, 0, sizeof(pnt));
      return;  // cant do this
   }

   temp = 1.0 / temp;
   newP[0] *= temp;
   newP[1] *= temp;
   newP[2] *= temp;
}


/*******************************************************************
CRender::mtranslate - translate the object by dx, dy, dz

inputs
   double   dx, dy, dz
*/
void CRender::mtranslate (double dx, double dy, double dz)
{
   Matrix temp, temp2;

   ZeroMatrix (temp);
   TransMatrix (temp, dx, dy, dz);
   MultiplyMatrices (CTM, temp, temp2);
   CopyMatrix (temp2, CTM);
   MakeMainMatrix ();
}

/*******************************************************************
CRender::mscale - scale the x,y,z coordinates

inputs
   double   sx,sy,sz -scale
*/
void CRender::mscale (double sx, double sy, double sz)
{
   Matrix temp, temp2;

   ZeroMatrix (temp);
   temp[0][0] = sx;
   temp[1][1] = sy;
   temp[2][2] = sz;
   temp[3][3] = 1.0;

   MultiplyMatrices (CTM, temp, temp2);
   CopyMatrix (temp2, CTM);
   MakeMainMatrix ();
}


/*******************************************************************
CRender::mrotate - rotate

inputs
   double   angle - angle in radians
   int      axis - 1 for x, 2 for y, 3 for z
*/
void CRender::mrotate (double angle, int axis)
{
   Matrix   temp, temp2;

   ZeroMatrix (temp);
   switch (axis) {
   case 1:
      RotationMatrix (temp, angle, 0, 0);
      break;
   case 2:
      RotationMatrix (temp, 0, angle, 0);
      break;
   case 3:
      RotationMatrix (temp, 0, 0, angle);
      break;
   default:
      RotationMatrix (temp, 0, 0, 0);
      break;
   }

   MultiplyMatrices (CTM, temp, temp2);
   CopyMatrix (temp2, CTM);
   MakeMainMatrix();

}



/*******************************************************************
CRender::mperspect - premultiply CTM by the perspective matrix
   and camera model

inputs
   double   znear, zfar_inv - near and far clipping
*/
void CRender::mperspect (double znear, double zfar_inv)
{
   ZeroMatrix (persMatrix);
   PerspectMatrix (persMatrix, dfov, znear, zfar_inv);

   MakeMainMatrix();
}




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
void CRender::fov (double p)
{
   dfov = p;
}


/*******************************************************************
ell - set the distance from the viewer to the foregroun object
*/
void CRender::ell (double p)
{
   dell = p;
}

/*******************************************************************
FOHBOT - actually builds the fohbot model.
*/
void CRender::FOHBOT (const pnt lup, const pnt wfore, const pnt wback, const pnt sfore,
                      const pnt sback, double fov, double ell)
{
   Matrix R1, R2, R3, T1, R4, R5, T2;
   Matrix M, m, tempM, tempM2;

   int   i,j;

   double   theta, thetaf, thetab, phi, beta, gamma, alpha, dinv;
   double   s, epsilon, eta, zeta, lenb, lenu;
   double   temp, aa, bb, cc;
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


/*******************************************************************
TransRot - Translate to p1 and then rotate so that looking at p2.

inputs
   pnt      p1 - where to move to
   pnt      p2 - point to be looking at
*/
void CRender::TransRot (const pnt p1, const pnt p2)
{
   pnt   p;
   double   f, fInv;
   SubVector (p2, p1, p);
   f = sqrt (p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
   if (f) {
      fInv = 1.0 / f;
      p[0] *= fInv;
      p[1] *= fInv;
      p[2] *= fInv;
   }

   // translate
   Translate (p1[0], p1[1], p1[2]);

   // return if cant really rotate
   if (f == 0.0)
      return;

   // rotate around y & x
   double   fAroundY, fAroundX;
   Matrix   temp, temp2, temp3;
   ZeroMatrix (temp);

#ifdef _DEBUG
   double ft[3][3];
   int x, y;
   for (x = 0; x < 3; x++) for (y = 0; y < 3; y++)
      ft[x][y] = atan2((double)(y - 1), (double)(x-1));
#endif

   if ((p[0] == 0.0) && (p[2] == 0.0))
      fAroundY = 0;
   else
      fAroundY = atan2 (p[0], -p[2]);
   if (fAroundY)
      RotationMatrix (temp, 0, fAroundY, 0);
   else
      IdentityMatrix (temp);

   // apply the translation and see how much we have to do around x
   pnt pRot;
   p[3] = 1;
   MultiplyMatrixPnt (temp, p, pRot);
   if ((pRot[1] == 0.0) && (pRot[2] == 0.0))
      fAroundX = 0;
   else
      fAroundX = atan2 (-pRot[1], -pRot[2]);
   if (fAroundX) {
      ZeroMatrix (temp2);
      RotationMatrix (temp2, fAroundX, 0, 0);

#ifdef _DEBUG
      pnt   p3;
      MultiplyMatrixPnt (temp2, pRot, p3);
#endif
      MultiplyMatrices (temp2, temp, temp3);
   }
   else {
      CopyMatrix (temp, temp3);
   }

#ifdef _DEBUG
   MultiplyMatrixPnt (temp3, p, pRot);
#endif
   InvertMatrix (temp3, temp2);

   // apply this
   MultiplyMatrices (CTM, temp2, temp);
   CopyMatrix (temp, CTM);
   MakeMainMatrix();
}


/*******************************************************************
SetScaleMatrix - for different window settings
*/
void CRender::SetScaleMatrix (void)
{
   ZeroMatrix (scaleMatrix);
   scaleMatrix[0][0] = 1.0;
   scaleMatrix[1][1] = 1.0 / aspect;
   scaleMatrix[2][2] = 1.0;
   scaleMatrix[3][3] = 1.0;

   MakeMainMatrix();
}




/*******************************************************************
mident - initialize the CTM to an identity matrix and put in scaling
*/
void CRender::mident (void)
{
   dfov = PI / 4.0;

   ZeroMatrix (persMatrix);
   ZeroMatrix (scaleMatrix);
   ZeroMatrix (camera);
   IdentityMatrix (camera);

   mperspect (0.0, 0.0);
   SetScaleMatrix ();

   ZeroMatrix (CTM);
   IdentityMatrix (CTM);

   MakeMainMatrix ();
}


/*******************************************************************
mpush - push the current CTM onto the stack
*/
void CRender::mpush (void)
{
   MatrixStruct *t;

   t = (MatrixStruct*) ESCMALLOC(sizeof(MatrixStruct));

   if (!t)
      return;  // error

   t->next =TOS;
   CopyMatrix (CTM, t->m);

   TOS = t;
}


/*******************************************************************
mpop - pop a matrix from the top of the stack and into the CTM.
*/
DWORD CRender::mpop (void)
{
   MatrixStruct *t;

   if (TOS == NULL)
      return 1;

   CopyMatrix (TOS->m, CTM);
   t = TOS->next;
   ESCFREE (TOS);
   TOS = t;
   MakeMainMatrix ();

   return 0;
}

/*******************************************************************
stkinit - initalizes the stack. If there are contents on the
   matrix then clear them out
*/
void CRender::stkinit (void)
{
   MatrixStruct *t;

   while (TOS != NULL) {
      t = TOS->next;
      ESCFREE (TOS);
      TOS = t;
   }

}


/*******************************************************************
initl1 - initialize.
*/
void CRender::initl1 (void)
{
   // I don't think there's anything for this to do
}


/*******************************************************************
gsinit - initialize
*/
void CRender::gsinit (void)
{
   // default to a font
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = 16;
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = VARIABLE_PITCH | FF_MODERN;
   lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
   strcpy (lf.lfFaceName, "Arial");
   TextFontSet (&lf, 10, TRUE);

   // remember last
   last[0] = last[1] = last[2] = last[3] = 0;
   CopyPnt (last, old);

   mident();
   stkinit();
   ColorMapFree ();
   BumpMapFree ();

   // set the light
   pnt p;   // light over the viewer's shoulder
   p[0] = -1;
   p[1] = 1;
   p[2] = 1;
   LightVector (p);
   m_fLightIntensity = .8;
   m_fLightBackground = .4;

   // to do wireframe rendering
   m_fWireFrame = FALSE;
   m_fDrawNormals = FALSE;
   m_fBackCull = TRUE;
   m_fDrawFacets = FALSE;
   m_dwColorMode = 0;   // full color
   m_fFogOn = FALSE;
   m_fFogMaxDistance = 5.0;
   m_fFogMinDistance = 2.0;

   m_dwMaxFacets = 32;
   m_fPixelsPerFacet = 10;

   m_fBumpVectorLen = 0;
   m_fBumpScale = 1.0;
   m_fVectorScale = 1.0;
   m_fDontInterpVectors = FALSE;
   m_fVectorArrows = TRUE;
   m_dwMaxAxisLines = 8;

   // default color
   m_DefColor[0] = m_DefColor[2] = 0x60;
   m_DefColor[1] = 0xc0;
   m_DefColor[3] = 0;

   m_BackColor[0] = m_BackColor[1] = m_BackColor[3] = 0;
   m_BackColor[2] = 0;  // was 0x40 - changed to 0 for transparency

   m_dwMajorObjectID = 1;
   m_dwMinorObjectID = 0;
}


/*******************************************************************
endl1 - uninitialize
*/
void CRender::endl1 (void)
{
   // i don't think there's anything to do here
}


/*******************************************************************
SetHDC - Sets the HDC and drawing rectangle values so know what
   to draw into

inputs
   HDC   hDC - device contect
   RECT  *pRect - rectangle to use to draw into. If drawing a control
         (using the full screen) then set to a rectangle covering the
         entire screen.
   RECT  *pClip - Set to NULL if clipping the same and pRect. Else,
         this is the area that will be drawn into. Note: Only enough Z buffer
         and memory will be allocated for pClip.
   float fzValue - For clear
*/
void CRender::SetHDC (HDC hDC, RECT *pRect, RECT *pClip, float fZValue)
{
   // free up old stuff
   if (m_hDCDraw) {
      if (m_hBitDrawOld)
         SelectObject (m_hDCDraw, m_hBitDrawOld);
      DeleteDC (m_hDCDraw);
      DeleteObject (m_hBitDraw);
   }
   m_hDCDraw = NULL;
   m_hBitDrawOld = NULL;
   m_hBitDraw = NULL;
   if (m_bm.bmBits) {
      ESCFREE (m_bm.bmBits);
      m_bm.bmBits = NULL;
   }
   m_bm.bmBits = NULL;

   // free z buffer
   if (m_pafZBuf)
      ESCFREE (m_pafZBuf);
   m_pafZBuf = NULL;
   if (m_padwObject)
      ESCFREE (m_padwObject);
   m_padwObject = NULL;

   // set the clipping rect
   memcpy (&m_rHDC, pRect, sizeof(m_rHDC));  // BUGFIX - moved this before used m_rHDC
   if (pClip)
      m_rClip = *pClip;
   else {
      // m_rClip = *pRect;
      m_rClip.left = m_rClip.top = 0;
      m_rClip.right = m_rHDC.right - m_rHDC.left;
      m_rClip.bottom = m_rHDC.bottom - m_rHDC.top;
   }
   m_pClipSize.x = max(m_rClip.right - m_rClip.left,0);
   m_pClipSize.y = max(m_rClip.bottom - m_rClip.top,0);
   m_pClipSize.x = max(m_pClipSize.x, 0); // BUGFIX - just in case
   m_pClipSize.y = max(m_pClipSize.y, 0); // BUGFIX - just in case
#ifdef _DEBUG
   if (!m_pClipSize.x || !m_pClipSize.y)
      m_hDC = 0;
#endif

   m_hDC = hDC;
   m_rDraw.left = m_rDraw.top = 0;
   m_rDraw.right = m_rHDC.right - m_rHDC.left;
   m_rDraw.bottom = m_rHDC.bottom - m_rHDC.top;

   // allcoate z buf
   m_pafZBuf = (float*) ESCMALLOC (m_pClipSize.x * m_pClipSize.y * sizeof(float));
   m_padwObject = (DWORD*) ESCMALLOC (m_pClipSize.x * m_pClipSize.y * sizeof(DWORD) * 2);

   // bitmap
   // BUGFIX - if it's a raster printer then we want to use the special colormode
   // so create a bitmap compatible with the screen instead of with the printer.
   // For some readon this works
   BOOL  fPrinter;
   fPrinter = FALSE;
   if (EscGetDeviceCaps(hDC, TECHNOLOGY) == DT_RASPRINTER) {
      HDC   hScreen = GetDC (NULL);
      m_hBitDraw = CreateCompatibleBitmap (hScreen, m_pClipSize.x, m_pClipSize.y);
      ReleaseDC (NULL, hScreen); // BUGFIX - Was hDC instead of hScreen and was crashing
      fPrinter = TRUE;
   }
   else
      m_hBitDraw = CreateCompatibleBitmap (m_hDC, m_pClipSize.x, m_pClipSize.y);

   // bitmap information
   BITMAP   bm;
   
   // if the video is 24-bit then do it the fast way
   m_fWriteToMemory = FALSE;
   GetObject (m_hBitDraw, sizeof(bm), &bm);
//   BYTE  *pb;
   // BUGFIX - if it's a raster printer then draw to memory
//   if ( (EscGetDeviceCaps(hDC, TECHNOLOGY) == DT_RASPRINTER) || ((bm.bmPlanes == 1) && (bm.bmBitsPixel == 24)) ) {
// if ( (bm.bmPlanes == 1) && (bm.bmBitsPixel == 24) ) {

   // BUGBUG - this is slow. need to do without setpixel()

   // BUGFIX - I think below write-directly-to-bitmap is causing some problems
   // with some video drivers
//   if ( !fPrinter && (bm.bmPlanes == 1) && (bm.bmBitsPixel == 24) ) {
//      DeleteObject (m_hBitDraw);
//      m_hBitDraw = NULL;
//
//      m_fWriteToMemory = TRUE;
//      memset (&m_bm, 0, sizeof(m_bm));
//      m_bm.bmWidth = m_pClipSize.x;
//      m_bm.bmHeight = m_pClipSize.y;
//      m_bm.bmWidthBytes = m_pClipSize.x * 3;
//      if (m_bm.bmWidthBytes % 2)
//         m_bm.bmWidthBytes++;
//      m_bm.bmPlanes = 1;
//      m_bm.bmBitsPixel = 24;
//      m_bm.bmBits = ESCMALLOC(m_bm.bmWidthBytes * m_bm.bmHeight);
//      if (!m_bm.bmBits)
//         return;   // can't do it
//      pb = (BYTE*) m_bm.bmBits;
//   }
//   else {
      HDC   hDCScreen;
      if (fPrinter)
         hDCScreen = GetDC(NULL);
      m_hDCDraw = CreateCompatibleDC (fPrinter ? hDCScreen : m_hDC);
      if (fPrinter)
         ReleaseDC (NULL, hDCScreen);
      m_hBitDrawOld = (HBITMAP) SelectObject (m_hDCDraw, m_hBitDraw);
//   }

   // set the viewport to the extreme edge
   winviewpt ();

   // wipe the display
   Clear(fZValue);
}

/*******************************************************************
DrawPixel - draws a pixel on the internal bitmap at x,y. If fZ is further
   from the front than other pixel drawn behind then the pixel isn't drawn.

inputs
   int      iX,iY - x,y - assumed to be within scope
   float    fZ - z value
   DWORD    rgb - color value
*/
inline void CRender::DrawPixel (int iX, int iY, float fZ, DWORD rgb)
{
   // fix ix, iy
   iX -= m_rClip.left;
   iY -= m_rClip.top;

   // sometimes go a bit beyond edge
   if (!m_pafZBuf || (iX < 0) || (iY < 0) || (iX >= m_pClipSize.x) || (iY >= m_pClipSize.y))
      return;

   // ignoreing z-buffer for now
   float *pf;
   DWORD *pdw;
   pf = m_pafZBuf + (iX + iY * m_pClipSize.x);
   if (fZ < *pf)
      return;
   pdw = m_padwObject + ((iX + iY * m_pClipSize.x) * 2);
   *pf = fZ;

   if (m_fWriteToMemory) {
      BYTE  *pb;
      pb = (BYTE*) m_bm.bmBits + (iX * 3 + iY * m_bm.bmWidthBytes);
      *(pb++) = GetBValue (rgb);
      *(pb++) = GetGValue (rgb);
      *pb = GetRValue(rgb);
   }
   else
      SetPixel (m_hDCDraw, iX, iY, rgb);

   // store object away
   *(pdw++) = m_dwMajorObjectID;
   *pdw = m_dwMinorObjectID;
}


/********************************************************************
DrawHorzLine - Draws a horizontal line (for purposes of rasterizing
   polygons.) This also does clipping on the x axis

inputs
   double   x1, x2 - x coord. x2 > x1
   int      y - y coordinate
   double   z1, z2 - z to interpolate
   pnt      c1, c2 - RGB color at start/end
*/
void CRender::DrawHorzLine (double x1, double x2, int y, double z1, double z2,
                   pnt c1, pnt c2)
{
   // check y
   if ((y < m_rClip.top) || (y >= m_rClip.bottom))
      return;

   // loop
   int x, xEnd;
   double   xDelta = x2 - x1;
   if (xDelta <= 0.0)
      return;  // cant do this
   double   zDelta = (z2 - z1) / xDelta;
      // note: there will be a little bit of round-off error in z, and coloring
      // because don't take into account that not starting right away
      // shouldn't be a problem

   pnt   cCur, cDelta;
   CopyPnt (c1, cCur);
   BOOL  fIsFlat;
   fIsFlat = m_fDrawFacets ||
      ((c1[0] == c2[0]) && (c1[1] == c2[1]) && (c1[2] == c2[2]) );
   if (!fIsFlat) {
      for (x = 0; x < 3; x++)
         cDelta[x] = (c2[x] - c1[x]) / xDelta;
   }

   // loop range
   x = (int) ceil(x1);
   xEnd = (int) floor(x2);

   double   fAlpha;
   if ((x < m_rClip.left) || (xEnd >= m_rClip.right)) {
      if (x < m_rClip.left) {
         // start on left
         x = m_rClip.left;
      };

      if (xEnd >= m_rClip.right) {
         xEnd = m_rClip.right - 1;
      }
   }

   // account for ceil
   fAlpha = (double) x - x1;
   z1 += zDelta * fAlpha;
   if (!fIsFlat) {
      cCur[0] += cDelta[0] * fAlpha;
      cCur[1] += cDelta[1] * fAlpha;
      cCur[2] += cDelta[2] * fAlpha;
   }

   // bail out if obvious not doing anyhting
   if (!m_pafZBuf || (x > xEnd))
      return;

   // pointers into buffers
   float *pf;
   DWORD *pdw;
   pf = m_pafZBuf + (x - m_rClip.left + (y - m_rClip.top) * m_pClipSize.x);
   pdw = m_padwObject + ((x - m_rClip.left + (y - m_rClip.top) * m_pClipSize.x) * 2);
   BYTE  *pb;
   if (m_bm.bmBits && m_fWriteToMemory)
      pb = (BYTE*) m_bm.bmBits + ((x - m_rClip.left) * 3 + (y - m_rClip.top) * m_bm.bmWidthBytes);
   else
      pb = NULL;

   float fz1;

   // do the loop
   for (; x <= xEnd; x++) {
      // only draw if fits in z-buffer
      fz1 = (float) z1;
      if (fz1 >= *pf) {
         if (pb) {
            *(pb++) = (BYTE) cCur[2];
            *(pb++) = (BYTE) cCur[1];
            *(pb++) = (BYTE) cCur[0];
         }
         else
            SetPixel (m_hDCDraw, x - m_rClip.left, y - m_rClip.top, RGB(cCur[0], cCur[1], cCur[2]));

         // store z value
         *pf = fz1;

         // store object away
         *(pdw++) = m_dwMajorObjectID;
         *(pdw++) = m_dwMinorObjectID;
      }
      else {
         if (pb)
            pb += 3;

         pdw += 2;
      }

      // increase by deltas
      if (!fIsFlat) {
         cCur[0] += cDelta[0];
         cCur[1] += cDelta[1];
         cCur[2] += cDelta[2];
      }
      z1 += zDelta;
      pf++;
   }
}


/********************************************************************
FillBetweenLines - Fills everything between two lines.

inputs
   double   y1, y2 - (y1 is top) top and bottom of the lines. Both lines
                     must start and stop at same point
   double   x1Left, x2Left - x location of first line at top & bottom, left line
   double   x1Right, x2Right - right line
   double   z1Left, z2Left - z
   double   z1Right, z2Right - z
   pnt      c1Left, c2Left - colors for left line
   pnt      c1Right, c2Right - colors for right line
*/
void CRender::FillBetweenLines (double y1, double y2,
                                double x1Left, double x2Left, double x1Right, double x2Right,
                                double z1Left, double z2Left, double z1Right, double z2Right,
                                pnt c1Left, pnt c2Left,
                                pnt c1Right, pnt c2Right)
{
   // figure out delta per Y that move down
   double   yDelta;
   double   xLeftDelta, xRightDelta;
   double   zLeftDelta, zRightDelta;
   pnt      curLeft, curRight;
   pnt      cLeftDelta, cRightDelta;
   int      y, yMax;

   yDelta = y2 - y1;
   if (yDelta <= 0.0)
      return;
   yDelta = 1.0 / yDelta;
   xLeftDelta = (x2Left - x1Left) * yDelta;
   xRightDelta = (x2Right - x1Right) * yDelta;
   zLeftDelta = (z2Left - z1Left) * yDelta;
   zRightDelta = (z2Right - z1Right) * yDelta;

   // color
   BOOL  fIsFlat;
   CopyPnt (c1Left, curLeft);
   CopyPnt (c1Right, curRight);
   fIsFlat = m_fDrawFacets ||
      ((c1Left[0] == c2Left[0]) && (c1Left[1] == c2Left[1]) && (c1Left[2] == c2Left[2]) &&
      (c1Right[0] == c2Right[0]) && (c1Right[1] == c2Right[1]) && (c1Right[2] == c2Right[2]));
   if (!fIsFlat) {
      for (y = 0; y < 3; y++) {
         cLeftDelta[y] = (c2Left[y] - c1Left[y]) * yDelta;
         cRightDelta[y] = (c2Right[y] - c1Right[y]) * yDelta;
      }
   }

   // clip in Y direction?
   double   fAlpha = -y1;
   if (y1 < 0) {
      y1 = 0.0;

      // increase x a bit
      x1Left += xLeftDelta * fAlpha;
      x1Right += xRightDelta * fAlpha;
      z1Left += zLeftDelta * fAlpha;
      z1Right += zRightDelta * fAlpha;

      // colors
      if (!fIsFlat) {
         for (y = 0; y < 3; y++) {
            curLeft[y] += cLeftDelta[y] * fAlpha;
            curRight[y] += cRightDelta[y] * fAlpha;
         }
      }
   }
   if (y2 >= m_rDraw.bottom)
      y2 = m_rDraw.bottom - 1;
   if (y2 < -1)
      y2 = -1;

   // convert the y range to integers
   y = (int) ceil(y1);
   yMax = (int) floor(y2);

   // increase x and z based upon offset
   fAlpha = y - y1;
   x1Left += xLeftDelta * fAlpha;
   x1Right += xRightDelta * fAlpha;
   z1Left += zLeftDelta * fAlpha;
   z1Right += zRightDelta * fAlpha;
   int c;
   if (!fIsFlat) {
      for (c = 0; c < 3; c++) {
         curLeft[c] += cLeftDelta[c] * fAlpha;
         curRight[c] += cRightDelta[c] * fAlpha;
      }
   }

   for (; y <= yMax; y++) {
      // draw the lines in-between
      if (x1Left < x1Right)
         DrawHorzLine (x1Left, x1Right, y, z1Left, z1Right, curLeft, curRight);
      else  // it's the other way around
         DrawHorzLine (x1Right, x1Left, y, z1Right, z1Left, curRight, curLeft);

      // increment x and z
      x1Left += xLeftDelta;
      x1Right += xRightDelta;
      z1Left += zLeftDelta;
      z1Right += zRightDelta;

      if (!fIsFlat) {
         // colors
         curLeft[0] += cLeftDelta[0];
         curLeft[1] += cLeftDelta[1];
         curLeft[2] += cLeftDelta[2];
         curRight[0] += cRightDelta[0];
         curRight[1] += cRightDelta[1];
         curRight[2] += cRightDelta[2];
      }
   }

   // done
}


/********************************************************************
DrawClippedFlatTriangle - Draws a triangle that MUST be clipped in Z, or
   may have problems.

inputs
   pnt   apVert[3] - vertices of the triangle. These should be in
               2D space. After trans1 and trans2
   pnt   apColor[3] - color at the vertices
*/

// IMPORTANT - because not interpolating w and z, I think I'm getting a little
// bit of error in the z buffer values, causing problems when objects intersect one
// another

void CRender::DrawClippedFlatTriangle (pnt apVert[3], pnt apColor[3])
{
   double   ax[4], ay[4], az[4];
   int   i;

   // first, convert to 2D
   for (i = 0; i < 3; i++) {
      if (apVert[i][3] == 0.0)
         return;  // shouldn't happen

      DevIndepToDevDep (apVert[i][0] / apVert[i][3],
         apVert[i][1] / apVert[i][3],
         ax + i, ay + i);
      az[i] = apVert[i][2]; // / apVert[i][3];
   }

   // figure out which is the highest point, begin assuming they're
   // in order
   int   top, middle, bottom, swap;
   top = 0;
   middle = 1;
   bottom = 2;

   if (ay[top] > ay[middle]) {
      swap = top;
      top = middle;
      middle = swap;
   }
   if (ay[top] > ay[bottom]) {
      swap = top;
      top = bottom;
      bottom = swap;
   }
   if (ay[middle] > ay[bottom]) {
      swap = middle;
      middle = bottom;
      bottom = swap;
   }

   // figure out how far the middle is from the top
   double   fHeight, fAlpha;
   fHeight = ay[bottom] - ay[top];
   if (fHeight == 0.0)
      return;  // no height
   fAlpha = (ay[middle] - ay[top]) / fHeight;

   // interpolate a 4th point occuring at fAlpha
   double   fInv;
   pnt      c3;
   fInv = 1.0 - fAlpha;
   ay[3] = fInv * ay[top] + fAlpha * ay[bottom];
   ax[3] = fInv * ax[top] + fAlpha * ax[bottom];
   az[3] = fInv * az[top] + fAlpha * az[bottom];
   if (m_fDrawFacets) {
      c3[0] = apColor[top][0];
      c3[1] = apColor[top][1];
      c3[2] = apColor[top][2];
   }
   else {
      c3[0] = fInv * apColor[top][0] + fAlpha * apColor[bottom][0];
      c3[1] = fInv * apColor[top][1] + fAlpha * apColor[bottom][1];
      c3[2] = fInv * apColor[top][2] + fAlpha * apColor[bottom][2];
   }

   // draw the triangles
   // top half
   if (fAlpha > 0.0)
      FillBetweenLines (ay[top], ay[3],   // top to bottom
         ax[top], ax[middle], ax[top], ax[3],   // x left & right
         az[top], az[middle], az[top], az[3], // z left & right
         apColor[top], apColor[middle], apColor[top], c3);
   // bottom half
   if (fAlpha < 1.0)
      FillBetweenLines (ay[3], ay[bottom],   // top to bottom
         ax[middle], ax[bottom], ax[3], ax[bottom],   // x left & right
         az[middle], az[bottom], az[3], az[bottom], // z left & right
         apColor[middle], apColor[bottom], c3, apColor[bottom]);

}


/********************************************************************
DrawFlatTriangle - Draws a triangle. Deals with clipping. Does not do any
   kind of shading model.

inputs
   pnt   apVert[3] - vertices of the triangle. These should be in
               2D space. After calling trans1 and trans2
   pnt   apColor[3] - color at the vertices
*/
void CRender::DrawFlatTriangle (pnt apVert[3], pnt apColor[3])
{
   // figure which points are behind the viewer
   BOOL  fBehind[3];
   int   i, j;
   for (i = j = 0; i < 3; i++) {
      fBehind[i] = (apVert[i][2] >= -EPSILON);
      if (fBehind[i])
         j++;
   }
   
   // based upon how many are behind
   switch (j) {
   case 0:  // none. trivial accept
      DrawClippedFlatTriangle (apVert, apColor);
      return;
   case 1:  // one. cut it
   case 2:  // two
      break;
   case 3:  // trivial reject
      return;
   }

   // need to cut off a vertex. figure out which one
   int   k;
   if (j == 1)
      for (k = 0; !(fBehind[k]); k++); // find which one is clipped off
   else
      for (k = 0; fBehind[k]; k++);    // find which one stays

   // clip
   int   l;
   double   alpha, oneminus;
   pnt   cut1, cut2;
   pnt   colorCut1, colorCut2;

   // first cut
   l = (k+1)%3;
   if (apVert[k][2] == apVert[l][2])
      return;  // not a triangle. reject
   alpha = (apVert[k][2] + EPSILON) / (apVert[k][2] - apVert[l][2]);
   oneminus = 1.0 - alpha;
   for (i = 0; i < 4; i++)
      cut1[i] = alpha * apVert[l][i] + oneminus * apVert[k][i];
   for (i = 0; i < 3; i++)
      colorCut1[i] = alpha * apColor[l][i] + oneminus * apColor[k][i];

   // second cut
   l = (k+2)%3;
   if (apVert[k][2] == apVert[l][2])
      return;  // not a triangle. reject
   alpha = (apVert[k][2] + EPSILON) / (apVert[k][2] - apVert[l][2]);
   oneminus = 1.0 - alpha;
   for (i = 0; i < 4; i++)
      cut2[i] = alpha * apVert[l][i] + oneminus * apVert[k][i];
   for (i = 0; i < 3; i++)
      colorCut2[i] = alpha * apColor[l][i] + oneminus * apColor[k][i];


   // draw triangles
   pnt   aNewVert[3];
   pnt   aNewColor[3];
   if (j == 1) {
      // first triangle
      l = (k+1)%3;
      CopyPnt (apVert[l], aNewVert[0]);
      CopyPnt (apColor[l], aNewColor[0]);

      l = (k+2)%3;
      CopyPnt (apVert[l], aNewVert[1]);
      CopyPnt (apColor[l], aNewColor[1]);

      CopyPnt (cut1, aNewVert[2]);
      CopyPnt (colorCut1, aNewColor[2]);

      DrawClippedFlatTriangle (aNewVert, apColor);

      // second triangle
      CopyPnt (cut2, aNewVert[2]);
      CopyPnt (colorCut2, aNewColor[2]);

      DrawClippedFlatTriangle (aNewVert, apColor);
   }
   else {
      CopyPnt (apVert[k], aNewVert[0]);
      CopyPnt (apColor[k], aNewColor[0]);

      CopyPnt (cut1, aNewVert[1]);
      CopyPnt (colorCut1, aNewColor[1]);

      CopyPnt (cut2, aNewVert[2]);
      CopyPnt (colorCut2, aNewColor[2]);

      DrawClippedFlatTriangle (aNewVert, apColor);

   }

}


/********************************************************************
DrawFlatPoly - Draws a flat polygon. Deals with clipping. Does not do any
   kind of shading model.

inputs
   DWORD dwCount - number of vertices
   pnt   apVert[] - vertices of the triangle. These should be in
               2D space. After calling trans1 and trans2
   pnt   apColor[] - color at the vertices
*/
void CRender::DrawFlatPoly (DWORD dwCount, pnt apVert[], pnt apColor[])
{
   if (dwCount < 3)
      return;

   DWORD i;
   pnt   aTri[3];
   pnt   aColor[3];
   pnt   facetColor;

   if (m_fDrawFacets) {
      memset (facetColor, 0, sizeof(facetColor));
      for (i = 0; i < dwCount; i++) {
         facetColor[0] += apColor[i][0];
         facetColor[1] += apColor[i][1];
         facetColor[2] += apColor[i][2];
      }
      facetColor[0] /= dwCount;
      facetColor[1] /= dwCount;
      facetColor[2] /= dwCount;
   }

   // first point always remains constant
   CopyPnt (apVert[0], aTri[0]);
   CopyPnt (m_fDrawFacets ? facetColor : apColor[0], aColor[0]);

   // start with second pointin place
   CopyPnt (apVert[1], aTri[1]);
   CopyPnt (m_fDrawFacets ? facetColor : apColor[1], aColor[1]);

   // loop
   DWORD dwNum;
   for (i = 2; i < dwCount; i++) {
      dwNum = (i%2) ? 1 : 2;
      CopyPnt (apVert[i], aTri[dwNum]);
      CopyPnt (m_fDrawFacets ? facetColor : apColor[i], aColor[dwNum]);

      DrawFlatTriangle (aTri, aColor);
   }
}


/********************************************************************
CalcNormal - Calculate a normal from three points (may have already had
   tran1 done to them.) The three points should be given in clockwise
   order for culling. The normal may have any length.

inputs
   pnt   a, b, c - three points
   pnt   n - filled with the normal
*/
void CRender::CalcNormal (const pnt a, const pnt b, const pnt c, pnt n)
{
   pnt   p1, p2;

   SubVector (a, b, p1);
   SubVector (c, b, p2);

   CrossProd (p1, p2, n);
}

// this second calcnormal takes (a2-a1) x (b2-b1)
void CRender::CalcNormal (const pnt a1, const pnt a2, const pnt b1, const pnt b2, pnt n)
{
   pnt   p1, p2;

   SubVector (a2, a1, p1);
   SubVector (b2, b1, p2);

   CrossProd (p1, p2, n);
}


/********************************************************************
FogRange - Changes the fog distance. Converts the current 0 space into
   trans1(), rotated. Then, adds zstart and then zend, to figure out
   where fog is.
inputs
   double   fIntensity
*/
void CRender::FogRange(double zStart, double zEnd)
{
   pnt   pZero, pZeroTrans;
   pZero[0] = pZero[1] = pZero[2] = 0;
   mtrans1 (pZero, pZeroTrans);

   m_fFogMaxDistance = -(pZeroTrans[2] + zEnd);
   m_fFogMinDistance = -(pZeroTrans[2] + zStart);
   m_fFogMaxDistance = max(m_fFogMaxDistance, EPSILON);
   m_fFogMinDistance = min(m_fFogMaxDistance - EPSILON, m_fFogMinDistance);
}


/********************************************************************
LightVector - Specify a vector in which direction the one (and only)
   light is. The light is assumed to be infinitely far away. The
   vector must happen BEFORE mtrans1.

inputs
   pnt   p
*/
void CRender::LightVector (const pnt p)
{
   pnt   pZero, pZeroTrans, pTrans, pTemp;
   pZero[0] = pZero[1] = pZero[2] = 0;
   mtrans1 (pZero, pZeroTrans);
   CopyPnt (p, pTemp);
   mtrans1 (pTemp, pTrans);
   SubVector (pTrans, pZeroTrans, pTemp);

   // normalize
   double   fTemp;
   fTemp = sqrt (pTemp[0] * pTemp[0] + pTemp[1] * pTemp[1] + pTemp[2] * pTemp[2]);
   DWORD i;
   for (i = 0; i < 3; i++)
      m_LightVector[i] = pTemp[i] / fTemp;
}


/********************************************************************
CalcColor - Given a RGB color and normal vector, this calculates
   what color it should be. It uses garaud shading (infinite light source)
   and ambient lighting.

inputs
   pnt      pColorOrig - rgb values
   pnt      n - normal. It's OK if this isn't normalized. This
               must have had mtrans1() called on it.
   double   z - z. for fog. should be negative
   pnt      pColorNew - Filled in with the color vector.
*/
void CRender::CalcColor (const pnt pColorOrig, const pnt n, double z, pnt pColorNew)
{
   // figure out length
   double   fLength;
   fLength = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

   // final color
   double   fFinal;
   // BUGFIX - Was adding m_fLightBackground twice
   fFinal = m_fLightIntensity * DotProd (n, m_LightVector) / fLength;
      // assuming that light vector is already normalized
   if (fFinal < 0)
      fFinal = 0;
   fFinal += m_fLightBackground;

   // multiply
   DWORD i;
   for (i = 0; i < 3; i++) {
      pColorNew[i] = pColorOrig[i] * fFinal;
      pColorNew[i] = min(pColorNew[i], 255);
   }

   ApplyFog (pColorNew, z);
   ApplyColorMode (pColorNew);
}


/********************************************************************
ShouldHideIt - Returns TRUE if the vector is facing away

inputs
   pnt   a, b, c - three points. shoild have gone thrugh mtrans1 and mtrans2
returns
   BOOL - true if should hide it
*/
BOOL CRender::ShouldHideIt (const pnt a, const pnt b, const pnt c)
{
   pnt   aa, bb, cc;
   pnt   p1, p2;
   pnt   n;

   aa[0] = a[0] / a[3];
   aa[1] = a[1] / a[3];
   aa[2] = a[2];
   bb[0] = b[0] / b[3];
   bb[1] = b[1] / b[3];
   bb[2] = b[2];
   cc[0] = c[0] / c[3];
   cc[1] = c[1] / c[3];
   cc[2] = c[2];

   SubVector (aa, bb, p1);
   SubVector (cc, bb, p2);

   CrossProd (p1, p2, n);

   return (n[2] <= EPSILON);
}

/********************************************************************
DrawLine - Draws a line on the screen.

inputs
   int   x1, y1, x2, y2 - x and y screen coordinates. Assumes its already clipped
   double   z1, z2 - z values
   ptn   RGB1, RGB2 - first three values are RGB colors to interpolate
*/
void CRender::DrawLine (int x1, int y1, int x2, int y2,
               double z1, double z2,
               pnt RGB1, pnt RGB2)
{
   int   iDeltaX, iDeltaY;
   iDeltaX = x2 - x1;
   iDeltaY = y2 - y1;

   if (!iDeltaX && !iDeltaY) {
      // just a point
      DrawPixel (x1, y1, (float) z1, RGB(RGB1[0], RGB1[1], RGB1[2]));
      return;
   }

   int   iAbsX, iAbsY;
   iAbsX = max(iDeltaX,-iDeltaX);
   iAbsY = max(iDeltaY, -iDeltaY);
   // if it's mostly up/down do one algorith, else another
   if (iAbsX > iAbsY) {
      // either moving right/left
      int   i;
      double   fY, fDelta, fZ, fDeltaZ;
      fY = y1;
      fZ = z1;
      fDelta = iDeltaY / (double) iDeltaX;
      fDeltaZ = (z2 - z1) / iAbsX;

      // RGB
      pnt   pntCur, pntDelta;
      CopyPnt (RGB1, pntCur);
      for (i = 0; i < 3; i++)
         pntDelta[i] = (RGB2[i] - RGB1[i]) / iAbsX;

      if (iDeltaX > 0) {
         // move to the right
         for (i = x1; i < x2; i++, fY += fDelta, fZ += fDeltaZ,
               pntCur[0] += pntDelta[0], pntCur[1] += pntDelta[1], pntCur[2] += pntDelta[2])
            DrawPixel (i, (int) fY, (float) fZ, RGB(pntCur[0], pntCur[1], pntCur[2]));
      }
      else {
         for (i = x1; i > x2; i--, fY -= fDelta, fZ += fDeltaZ,
               pntCur[0] += pntDelta[0], pntCur[1] += pntDelta[1], pntCur[2] += pntDelta[2])
            DrawPixel (i, (int) fY, (float) fZ, RGB(pntCur[0], pntCur[1], pntCur[2]));
      }

   }
   else {
      // moving up/down
      int   i;
      double   fX, fDelta, fZ, fDeltaZ;
      fX = x1;
      fZ = z1;
      fDelta = iDeltaX / (double) iDeltaY;
      fDeltaZ = (z2 - z1) / iAbsY;

      // RGB
      pnt   pntCur, pntDelta;
      CopyPnt (RGB1, pntCur);
      for (i = 0; i < 3; i++)
         pntDelta[i] = (RGB2[i] - RGB1[i]) / iAbsY;

      if (iDeltaY > 0) {
         // move to the right
         for (i = y1; i < y2; i++, fX += fDelta, fZ += fDeltaZ,
               pntCur[0] += pntDelta[0], pntCur[1] += pntDelta[1], pntCur[2] += pntDelta[2])
            DrawPixel ((int) fX, i, (float) fZ, RGB(pntCur[0], pntCur[1], pntCur[2]));
      }
      else {
         for (i = y1; i > y2; i--, fX -= fDelta, fZ += fDeltaZ,
               pntCur[0] += pntDelta[0], pntCur[1] += pntDelta[1], pntCur[2] += pntDelta[2])
            DrawPixel ((int) fX, i, (float) fZ, RGB(pntCur[0], pntCur[1], pntCur[2]));
      }
   }

}


/*******************************************************************
ApplyTransform - Applies translation and rotation transforms in total,
   to points. Apps can use this to pre-apply transforms on point and
   save resources.

inputs
   DWORD dwCount - number of vertices
   pnt   p[] - read and then modified
*/
void CRender::ApplyTransform (DWORD dwCount, pnt p[])
{
   DWORD i;
   pnt   pTemp;

   for (i = 0; i < dwCount; i++) {
      mtrans1 (p[i], pTemp);
      CopyPnt (pTemp, p[i]);
   }

}


/*******************************************************************
BoundingBox - Takes coordinates for a box around the object in question
   to be drawn, and returns TRUE if the object is trivially rejected.
   The object is trivially rejected if:
      1) It's all off the screen.
      2) Fog is on and it's all beyond the fog
      3) It's all behind another object (using z buffer)

inputs
   double   x1,y1,z1,x2,y2,z2 - xyz1 is upper,left,front. xyz2 is
               lower,right,back. (Doesn't really matter). This is
               in pre-rotated spave (before trans1)
   DWORD    *pdwFacets - This can be NULL. If the object is not
               trivially rejected, this is filled with the number
               of facets that should be used to get a good image.
               The facets are along one side of the object, so if its
               a spere, may need facets * 2. This is always at least 4
               and at most m_dwFacetsMax.
   BOOL     fAllTrans - if TRUE, do all transforms mtrans1 & mtrans2.
               if FALSE, only do mtrans2
returns
   BOOL - TRUE if trivial clip, FALSE if not
*/
BOOL CRender::BoundingBox (double x1, double y1, double z1,
                           double x2, double y2, double z2,
                           DWORD *pdwFacets, BOOL fAllTrans)
{
   pnt   pFlat[8];
   pnt   p;
   DWORD i, dwSide;
   for (i = 0; i < 8; i++) {
      // edge
      p[0] = (i & 0x01) ? x1 : x2;
      p[1] = (i & 0x02) ? y1 : y2;
      p[2] = (i & 0x04) ? z1 : z2;
      p[3] = 1;

      // transform
      if (fAllTrans)
         mtransform (p, pFlat[i]);
      else
         mtrans2 (p, pFlat[i]);
   }
      // BUGBUG 3ae3a
      //*((PBYTE)NULL) = 0;
      // BUGBUG

   // see if they're all off to one side
   // 0,1=top/bottom,2,3=left/right,4=behind,5=fogged
   double   fVal;
   double   *pp;
   for (dwSide = 0; dwSide < (DWORD) (m_fFogOn ? 6 : 5); dwSide++) {
      for (i = 0; i < 8; i++) {
         pp = &(pFlat[i][0]);
         switch (dwSide) {
         case 0:  // top
            fVal = pp[3] - pp[1];
            break;
         case 1:  // bottom
            fVal = pp[3] + pp[1];
            break;
         case 2: // left
            fVal = pp[3] - pp[0];
            break;
         case 3: // right
            fVal = pp[3] + pp[0];
            break;
         case 4: // behind user
            fVal = -pp[2];
            break;
         case 5: // fog
            fVal = m_fFogMaxDistance + pp[2];
            break;
         }

         if (fVal > 0)
            break;   // isn't in the clipping area
      }
      if (i >= 8)
         return TRUE;   // it's all the way in a clipping area so trivial clip
   }
      // BUGBUG 3aebb
      //*((PBYTE)NULL) = 0;
      // BUGBUG

   // try to figure out its screen coordinates
   double   fxMax, fyMax, fxMin, fyMin, fzMax, fx, fy;
   for (i = 0; i < 8; i++) {
      // if z >= 0 then at least one point is behind, so we can't clip based upon z
      if (pFlat[i][2] >= 0.0) {
         // claim the max number of facets because it's beyond z, so it probably needs
         // a lot of points
         if (pdwFacets)
            *pdwFacets = m_dwMaxFacets;

         return FALSE;
      }

      // convert to device (pixels)
      DevIndepToDevDep (pFlat[i][0] / pFlat[i][3], pFlat[i][1] / pFlat[i][3] * aspect, &fx, &fy);

      // remember the boundaries of these
      if (i) {
         fxMax = max(fxMax, fx);
         fxMin = min(fxMin, fx);
         fyMax = max(fyMax, fy);
         fyMin = min(fyMin, fy);
         fzMax = max(fzMax, pFlat[i][2]);
      }
      else {
         fxMax = fxMin = fx;
         fyMax = fyMin = fy;
         fzMax = pFlat[i][2];
      }

   }
      // BUGBUG 3af9d
      //*((PBYTE)NULL) = 0;
      // BUGBUG

   if (pdwFacets) {
      // how big
      double   fSize;
      fSize = max(fxMax - fxMin, fyMax - fyMin);
      fSize /= m_fPixelsPerFacet;
      fSize = max(4, fSize);
      fSize = min(m_dwMaxFacets, fSize);

      // keep the facets a power of 2
      DWORD dw;
      for (dw = (DWORD)fSize, *pdwFacets = 1; dw > 1; dw >>= 1, (*pdwFacets) <<= 1);

      *pdwFacets = min(m_dwMaxFacets, *pdwFacets);
   }

   // make sure not off edge
   fxMin = max(m_rClip.left, fxMin);
   fyMin = max(m_rClip.top, fyMin);
   fxMax = min(m_rClip.right-1, fxMax);
   fyMax = min(m_rClip.bottom-1, fyMax);
      // BUGBUG 3b0ae
      //*((PBYTE)NULL) = 0;
      // BUGBUG

   if (!m_pafZBuf)
      return TRUE;

      // BUGBUG 3b0dd
      //*((PBYTE)NULL) = 0;
      // BUGBUG

   // BUGFIX - might fix crash
   fxMax = min(m_pClipSize.x + m_rClip.left - 1, fxMax);
   fyMax = min(m_pClipSize.y + m_rClip.top - 1, fyMax);

   // loop over z buffer
   int   x, xMax, y, yMax;
   xMax = (int) fxMax;
   yMax = (int) fyMax;
   float *pf, fz;
   float *pfMax = m_pafZBuf + m_pClipSize.x * m_pClipSize.y;
   fz = (float) fzMax;
      // BUGBUG 3b12d
      //*((PBYTE)NULL) = 0;
      // BUGBUG
   for (y = (int) fyMin; y <= fyMax; y++) {
      // BUGBUG 3b161
     //*((PBYTE)NULL) = 0;
      // BUGBUG
      x = (int) fxMin;
      pf = m_pafZBuf + ((x - m_rClip.left) + (y-m_rClip.top) * m_pClipSize.x);
      for (; (x <= fxMax) && (pf < pfMax); x++, pf++) {  // BUGFIX (pf < pfMax) to try to stop crash
         if (fz > *pf)
            return FALSE;  // found at least one pixel that is not covered
      }
   }
      // BUGBUG xx
     //*((PBYTE)NULL) = 0;
      // BUGBUG

   // else, couldn't find any
   return TRUE;
}

/*******************************************************************
BoundingBox - other version which takes set of points and figures
   out min/max

inputs
   DWORD    dwCount - number of points
   double   *pafPoints - Array of pnt structures
*/
BOOL CRender::BoundingBox (DWORD dwCount, double *pafPoints, DWORD *pdwFacets, BOOL fAllTrans)
{
   DWORD i;
   double   x1,y1,z1,x2,y2,z2;

   x1 = x2 = pafPoints[0];
   y1 = y2 = pafPoints[1];
   z1 = z2 = pafPoints[2];

   for (i = 1; i < dwCount; i++, pafPoints += 4) {
      x1 = min(x1, pafPoints[0]);
      x2 = max(x2, pafPoints[0]);
      y1 = min(y1, pafPoints[1]);
      y2 = max(y2, pafPoints[1]);
      z1 = min(z1, pafPoints[2]);
      z2 = max(z2, pafPoints[2]);
   }

   return BoundingBox (x1, y1, z1, x2, y2, z2, pdwFacets, fAllTrans);
}


/*******************************************************************
Clear - Erases the display, zbuffer, and object
*/
void CRender::Clear (float fZValue)
{

   // convert the background color to RGB
   pnt   c;
   BYTE  br, bg, bb;
   CopyPnt (m_BackColor, c);
   ApplyColorMode (c, TRUE);
   br = (BYTE) c[0];
   bg = (BYTE) c[1];
   bb = (BYTE) c[2];


   // if drawing directly into bitmap, do something else
   if (m_hDCDraw) {
      HBRUSH   hb;
      hb = CreateSolidBrush (RGB(br,bg,bb));
      RECT  r;
      r.left = r.top = 0;
      r.right = m_pClipSize.x;
      r.bottom = m_pClipSize.y;

      FillRect (m_hDCDraw, &r, hb);
      DeleteObject (hb);
   }
   else if (m_bm.bmBits) {
      DWORD x, y;
      BYTE  *pb;
      for (y = 0; y < (DWORD) m_bm.bmHeight; y++) {
         pb = (LPBYTE) m_bm.bmBits + (m_bm.bmWidthBytes * y);
         for (x = 0; x < (DWORD) m_bm.bmWidth; x++) {
            *(pb++) = bb;
            *(pb++) = bg;
            *(pb++) = br;
         }
      }
   }

   if (!m_pafZBuf)
      return;

   // clear out z-buffer
   float *fCur, *fEnd;
   fCur = m_pafZBuf;
   fEnd = m_pafZBuf + (m_pClipSize.x * m_pClipSize.y);
   for (; fCur < fEnd; fCur++)
      *fCur = fZValue;

   // clear out the object store
   memset (m_padwObject, 0, m_pClipSize.x * m_pClipSize.y * sizeof(DWORD) * 2);
}


/*******************************************************************
viewpt - accepts a left, right, top, bottom, front, and back in the
   window's coordinates. This then alters ixl, ir, iyb,
   aspect, scalex, scaley, centerx in global
*/
void CRender::viewpt (double xl, double xr, double yb, double yt)
{
   double centx, centy, perpix;

   perpix = (m_rDraw.right - m_rDraw.left) / 2;
   centx = (m_rDraw.right + m_rDraw.left) / 2;
   centy = (m_rDraw.bottom + m_rDraw.top) / 2;

   // find the computer coordinate equivalents
   ixl = (int) (centx + xl * perpix);
   ixr = (int) (centx + xr * perpix);
   iyt = (int) (centy - yt * perpix);
   iyb = (int) (centy - yb * perpix);

   // don't allow draing beyond the window's borders
   ixl = max(ixl, m_rDraw.left);
   ixr = min(ixr, m_rDraw.right);
   iyt = max(iyt, m_rDraw.top);
   iyb = min(iyb, m_rDraw.bottom);

   centerx = (ixl + ixr) / 2.0;
   centery = (iyt + iyb) / 2.0;
   scalex = scaley = centerx - ixl;
      // ie: the left edge of the screen is always -1, and the right is always 1
      // scalex =scaley since pc pixels are sqaure

   if (ixr > ixl)
      aspect = (iyb - iyt) / ((double) ixr - ixl);
   else
      aspect = (iyb - iyt);   // BUGFIX - this might be causing crash
      // aspect = deltay / delta x

   // set scaling
   SetScaleMatrix();
}



/*******************************************************************
winviewpt - set the viewport to the window's edge
*/
void CRender::winviewpt (void)
{
   viewpt (-1.0, 1.0, -100, 100);   // 100's are just extreme values
}


/*******************************************************************
DevIndepToDevDep - converts from device independent coordinates
   to device dependent coordinates

inputs
   double      x,y - device independent
   double      *xx, *yy - filled with device dependent
*/
inline void CRender::DevIndepToDevDep (double x, double y, double *xx, double *yy)
{
   // the 0.5 is so thjat rounds off properly
   *xx = (centerx + (x*scalex) + 0.5);
   *yy = (centery - (y*scaley) + 0.5);
}


/*******************************************************************
mdl1 - draw a line from ix1,iy1 to ix2, iy2 - screen coordinates
*/
inline void CRender::mdl1 (int ix1, int iy1, int ix2, int iy2, pnt colorStart, pnt colorEnd)
{
//   MoveToEx (m_hDCDraw, ix1, iy1, NULL);
//   LineTo (m_hDCDraw, ix2, iy2);
   DrawLine (ix1, iy1, ix2, iy2, 0, 0, colorStart, colorEnd);
}


/*******************************************************************
ApplyFog - If there's fog set, this applies fog.

inputs
   pnt   c - color. modified in place
   double   z - z, negative z value
*/
inline void CRender::ApplyFog (pnt c, double z)
{
   if (!m_fFogOn)
      return;

   if (-z <= m_fFogMinDistance)
      return;

   double   fAlpha, fInv;
   fAlpha = (-z - m_fFogMinDistance) / (m_fFogMaxDistance - m_fFogMinDistance);
   fAlpha = max(0, fAlpha);
   fAlpha = min(1, fAlpha);
   fInv = 1.0 - fAlpha;
   c[0] = fInv * c[0] + fAlpha * m_BackColor[0];
   c[1] = fInv * c[1] + fAlpha * m_BackColor[1];
   c[2] = fInv * c[2] + fAlpha * m_BackColor[2];
}

/*******************************************************************
ApplyColorMode - If there's a color mode set, this either turns all
   colors to gray, or bnw

inputs
   pnt   c - color. modified in place
   BOOL  fWhiteInBNW - if in BnW mode, set to white. Else set to black
*/
inline void CRender::ApplyColorMode (pnt c, BOOL fWhiteInBNW)
{
   if (!m_dwColorMode)
      return;

   if (m_dwColorMode == 2) {
      // BNW
      double   fFinal = fWhiteInBNW ? 255 : 0;
      c[0] = c[1] = c[2] = fFinal;
      return;
   }

   // else
   double fFinal;
   fFinal = (c[0] + c[1] + c[2]) /3;

   switch (m_dwColorMode) {
   case 3:  // left eye
      fFinal *= 2;
      fFinal = min(255, fFinal);
      c[0] = fFinal;
      c[1] = c[2] = 0;
      break;
   case 4:  // right eye
      fFinal *= 2;
      fFinal = min(255, fFinal);
      c[2] = fFinal;
      c[0] = c[1] = 0;
      break;
   default: // gray scale
      c[0] = c[1] = c[2] = fFinal;
   }
}

/*******************************************************************
Clip - clips a 3-d line to the view port

inputs
   pnt      start, end - start and end poiunts. these are modified
   pnt      colorStart, colorEnd - starting/ending color. Also needs to be clipped
   DWORD    *pdwClipSides - if this is non null, it will be filled in informing
            which side was clipped. 0x01 bit for start, 0x02 for end
   BOOL     fOnlyZ - if TRUE, only clips Z
returns
   BOOL - 1 if should draw. 0 if shouldn draw
*/
BOOL CRender::Clip (pnt start, pnt end, pnt colorStart, pnt colorEnd, DWORD *pdwClipSides,
                    BOOL fOnlyZ)
{
   BOOL  oc[2][6];
   double   bc[2][6];
   double alpha1, alpha2, alpha;
   pnt newStart, newEnd;
   DWORD i,j;
   DWORD iStart = fOnlyZ ? 4 : 0;
   DWORD iStop = fOnlyZ ? 5 : 6;

   // first, normalize clipping space to (-1, 1, -1, 1
   // start[y] /= aspect
   // end[2] /= aspect

   bc[0][5] = start[3] + start[2];
   bc[1][5] = end[3] + end[2];
   bc[0][4] = -start[2];
   bc[1][4] = -end[2];
   bc[0][3] = start[3] + start[0];
   bc[1][3] = end[3] + end[0];
   bc[0][2] = start[3] - start[0];
   bc[1][2] = end[3] - end[0];
   bc[0][1] = start[3] + start[1];
   bc[1][1] = end[3] + end[1];
   bc[0][0] = start[3] - start[1];
   bc[1][0] = end[3] - end[1];

   for (i = iStart; i < iStop; i++)
      for (j = 0; j < 2; j++)
         oc[j][i] = (bc[j][i] < 0.0);

   // if all values are false then trivial accept
   j = 0;
   for (i = iStart; i < iStop; i++)
      if (oc[0][i] || oc[1][i])
         j++;
   if (j == 0) {
      // trivial accept
      start[1] *= aspect;
      end[1] *= aspect;
      if (pdwClipSides)
         *pdwClipSides = 0;
      return TRUE;
   }

   // if any values and to true then trivial reject
   j = 0;
   for (i = iStart; i < iStop; i++)
      if (oc[0][i] && oc[1][i])
         j++;
   if (j > 0) {
      if (pdwClipSides)
         *pdwClipSides = 0x03;
      return FALSE;  // trivial reject
      }

   // neither tribial reject nor accept
   alpha1 = 0.0;
   alpha2 = 1.0;

   for (i = iStart; i < iStop; i++)
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
      if (pdwClipSides)
         *pdwClipSides = 0x03;
      return FALSE;  // don't draw a line
   }

   for (i = 0; i < 4; i++) {
      newStart[i] = start[i] * (1.0 - alpha1) + end[i] * alpha1;
      newEnd[i] = start[i] * (1.0 - alpha2) + end[i] * alpha2;
   }

   pnt   newColorStart, newColorEnd;
   for (i = 0; i < 3; i++) {
      newColorStart[i] = colorStart[i] * (1.0 - alpha1) + colorEnd[i] * alpha1;
      newColorEnd[i] = colorStart[i] * (1.0 - alpha2) + colorEnd[i] * alpha2;
   }

   // lastly, readjust start,end back to normal
   CopyPnt (newStart, start);
   CopyPnt (newEnd, end);
   CopyPnt (newColorStart, colorStart);
   CopyPnt (newColorEnd, colorEnd);
   start[1] *= aspect;
   end[1] *= aspect;

   if (pdwClipSides)
      *pdwClipSides = ((alpha1 != 0.0) ? 0x01 : 0x00) | ((alpha2 != 1.0) ? 0x02 : 0x00);

   return TRUE;
}

/*******************************************************************
mdl2 - move/draw lines taking as inputs world coordinates, transforming
   them to 2-space, and then drawing them. without clipping
*/
inline void CRender::mdl2 (const pnt start, const pnt end, pnt colorStart, pnt colorEnd, BOOL fArrow)
{
   double   xx1, yy1, xx2, yy2;

   DevIndepToDevDep (start[0] / start[3], start[1] / start[3],
      &xx1, &yy1);
   DevIndepToDevDep (end[0] / end[3], end[1] / end[3],
      &xx2, &yy2);

   DrawLine ((int) xx1, (int) yy1, (int) xx2, (int) yy2,
      start[2], end[2],
      colorStart, colorEnd);
   // mdl1 ((int) xx1, (int) yy1, (int) xx2, (int) yy2, colorStart, colorEnd);

   // draw arrow lines?
   if (fArrow) {
      // table of arrow lengths. From 0..10, indicting a rise/run from 0 to 1.0. in .1 inc
      // all other possibilities can be generated from these. Assuming right facing, and
      // an arrow angle 90 degrees
#define  ARROWLENGTH    5
#define  ARROWPTS       11
      static pnt apArrowLeft[ARROWPTS];
      static pnt apArrowRight[ARROWPTS];
      static BOOL fCalcArrow = FALSE;

      // calculate it
      if (!fCalcArrow) {
         fCalcArrow = TRUE;

         DWORD i;
         for (i = 0; i < ARROWPTS; i++) {
            // figure out the angle with that rise over run
            double   fangle;
            fangle = asin ((double) i / (ARROWPTS-1));

            // left one
            double   f;
            f = fangle - PI * 3 / 4;
            apArrowLeft[i][0] = cos(f) * ARROWLENGTH;
            apArrowLeft[i][1] = sin(f) * ARROWLENGTH;

            // right one
            f = fangle + PI * 3 / 4;
            apArrowRight[i][0] = cos(f) * ARROWLENGTH;
            apArrowRight[i][1] = sin(f) * ARROWLENGTH;
         }
      } // done with calcarrow

      // figure out which one of the eight quadrants the point belongs to
      double   fMultX, fMultY;
      DWORD    dwXC, dwYC;
      double   fDeltaX, fDeltaY;
      double   fAbsDeltaX, fAbsDeltaY;
      double   fRiseRun;
      fDeltaX = xx2 - xx1;
      fDeltaY = yy2 - yy1;

      // make sure don't have zero point
      if ((fDeltaX == 0) && (fDeltaY == 0))
         return;

      fAbsDeltaX = max(fDeltaX, -fDeltaX);
      fAbsDeltaY = max(fDeltaY, -fDeltaY);
      fMultX = fMultY = 1.0;
      dwXC = 0;
      dwYC = 1;
      if (fDeltaX >= 0) { // on the right
         if (fDeltaY >= 0) { // upper right quadrant
            if (fAbsDeltaX > fAbsDeltaY) { // 0..45 degrees
               fRiseRun = fAbsDeltaY / fAbsDeltaX;
            }
            else { // 45..90 degrees
               fRiseRun = fAbsDeltaX / fAbsDeltaY;
               dwXC = 1;
               dwYC = 0;
            }
         }
         else { // lower right quadrant
            if (fAbsDeltaX > fAbsDeltaY) { // 0..-45 degrees
               fRiseRun = fAbsDeltaY / fAbsDeltaX;
               fMultY = -1;
            }
            else { // -45..-90 degrees
               fRiseRun = fAbsDeltaX / fAbsDeltaY;
               dwXC = 1;
               dwYC = 0;
               fMultY = -1;
            }
         }
      }
      else {   // on the left
         if (fDeltaY >= 0) { // upper legt quadrant
            if (fAbsDeltaX > fAbsDeltaY) { // 135-180 degrees
               fRiseRun = fAbsDeltaY / fAbsDeltaX;
               fMultX = -1;
            }
            else { // 90..135 degrees
               fRiseRun = fAbsDeltaX / fAbsDeltaY;
               dwXC = 1;
               dwYC = 0;
               fMultX = -1;
            }
         }
         else { // lower left quadrant
            if (fAbsDeltaX > fAbsDeltaY) { // -135..-180 degrees
               fRiseRun = fAbsDeltaY / fAbsDeltaX;
               fMultX = -1;
               fMultY = -1;
            }
            else { // -90..-135 degrees
               fRiseRun = fAbsDeltaX / fAbsDeltaY;
               dwXC = 1;
               dwYC = 0;
               fMultX = -1;
               fMultY = -1;
            }
         }
      }

      // draw two lines
      DWORD dwRiseRun;
      dwRiseRun = (DWORD) (fRiseRun * (ARROWPTS-1) + 0.5);
      DrawLine ((int) xx2, (int) yy2,
         (int) (xx2 + apArrowLeft[dwRiseRun][dwXC] * fMultX),
         (int) (yy2 + apArrowLeft[dwRiseRun][dwYC] * fMultY),
         end[2], end[2],
         colorEnd, colorEnd);

      DrawLine ((int) xx2, (int) yy2,
         (int) (xx2 + apArrowRight[dwRiseRun][dwXC] * fMultX),
         (int) (yy2 + apArrowRight[dwRiseRun][dwYC] * fMultY),
         end[2], end[2],
         colorEnd, colorEnd);

   }
}


/*******************************************************************
mdl3 - given a 3-d point (4-vector), this draws it on the screen.
   scale/W must be set correctly
*/
inline void CRender::mdl3 (pnt start, pnt end, pnt colorStart, pnt colorEnd, BOOL fArrow)
{
   DWORD dwClip;

   if (!Clip (start, end, colorStart, colorEnd, fArrow ? &dwClip : NULL))
      return;  // dont draw line

   // take out the arrow if the end has changed
   if (fArrow && (dwClip & 0x02))
      fArrow = FALSE;

   mdl2 (start, end, colorStart, colorEnd, fArrow);
}


/*******************************************************************
Move - Moves the line drawing to a point in space

inputs
   double   x,y,z - location in space
   DWORD    dwRGB - color
*/
void CRender::Move (double x, double y, double z, DWORD dwRGB)
{
   pnt   p;
   p[0] = x;
   p[1] = y;
   p[2] = z;
   p[3] = 1;

   mtransform (p, old);
   // remember this as old

   oldColor[0] = GetRValue(dwRGB);
   oldColor[1] = GetGValue(dwRGB);
   oldColor[2] = GetBValue(dwRGB);
}

/*******************************************************************
Draw - Draws a line from the old point to the new locaton

inputs
   double   x, y, z - location in space
   DWORD    dwRGB - color
*/
void CRender::Draw (double x, double y, double z, DWORD dwRGB)
{
   pnt   p;
   pnt   start, end, color;
   p[0] = x;
   p[1] = y;
   p[2] = z;
   p[3] = 1;

   color[0] = GetRValue(dwRGB);
   color[1] = GetGValue(dwRGB);
   color[2] = GetBValue(dwRGB);

   CopyPnt (old, start);
   mtransform (p, end);
   CopyPnt (end, old);

   // draw the line
   mdl3 (start, end, oldColor, color);
   CopyPnt (color, oldColor);
}


/*******************************************************************
Init - Reinitializes the system
*/
void CRender::Init (HDC hDC, RECT *pRect)
{
   gsinit();
   SetHDC (hDC, pRect);
}

/*******************************************************************
InitForControl - Intiializes the rendering system to draw for a control.
It does this by...
   1) Calling init.
   2) Creating a HDC and Z buffer, etc, but making sure they're
      appropriate for the screen size. Large enough for prScreen
         a) The Z buffer is set to clip at -screenwidth depth.
   3) Translate (0, 0, -screenwidth)
         (So that if draw Spehere(10) get one with a radius of 10 pixels)
   5) Translate (-(screenwidth/2+prScreen.left), -(screenheight/2+prscreen.top, 0)
         (So that 0,0 is at the upper left hand of the control)
   6) Double the detail level since controls are small


  NOTE: After calling this all drawing will be in screen coordinates.
  WARNING: Y coordinates are flipped, so to move down N pixels, translate(-N)

inputs
   HDC      hDC - DC that will be drawing onto. Don't worry, it doesn't draw now.
   RECT     *prHDC - Rctangle, in HDC coordinates, to draw onto
   RECT     *prScreen - Rectangle, in screen coordinates, to draw onto
   RECT     *prTotal - Size of the total screen
returns
   none
*/
void CRender::InitForControl (HDC hDC, RECT *prHDC, RECT *prScreen, RECT *prTotal)
{
   gsinit();
   double d;

   // clear the HDC
   d = 1.0 / tan(dfov / 2.0) / 2.0; // take FOV into account
   m_BackColor[0] = m_BackColor[1] = m_BackColor[2] = 0;
   SetHDC (hDC, prTotal, prScreen, (float) (-(prTotal->right - prTotal->left) * d));


   // translate
   Translate (0, 0, -(prTotal->right - prTotal->left) * d);
   // Scale (1, -1, 1);
   Translate (-(prTotal->right - prTotal->left)/2.0 + prScreen->left,
      (prTotal->bottom - prTotal->top)/2.0 - prScreen->top);
   m_fPixelsPerFacet /= 4.0;


   // adjust ambient light
   m_fLightIntensity = .6;
   m_fLightBackground = .8;
}

/*******************************************************************
Translate - translate the object by x, y, and z
*/
void CRender::Translate (double x, double y, double z)   // translates in x,y,z
{
   mtranslate (x, y, z);
}

/*******************************************************************
Rotate - rotate the object by fRadians. dwDim is the dimension,
   1 for x, 2 for y, 3 for z
*/
void CRender::Rotate (double fRadians, DWORD dwDim)  // Rotate. dwdim = 1 for x, 2 for y, 3 for z
{
   mrotate (fRadians, dwDim);
}

/*******************************************************************
Scale - scale the object by a uniform amount
*/
void CRender::Scale (double fScale)   // universally scale
{
   Scale (fScale, fScale, fScale);
}

/*******************************************************************
Scale - scale the object by a non-uniform amount
*/
void CRender::Scale (double x, double y, double z)   // scale in x, y, z
{
   mscale (x, y, z);
}


/*******************************************************************
ObjectGet - Gets the object number located at a specific pixel.
   Returns FALSE if none is there

inputs
   int      iX, iY - XY
   DWORD    *pdwMajor - fileld with major. can be null
   DWORD    *pdwMinor - filled with minor. can be null
returns
   BOOL - TRUE if object, FALSE if not
*/
BOOL CRender::ObjectGet (int iX, int iY, DWORD *pdwMajor, DWORD *pdwMinor)
{
   iX -= m_rClip.left;
   iY -= m_rClip.top;
   if (!m_pafZBuf || (iX < 0) || (iY < 0) || (iX >= m_pClipSize.x) || (iY >= m_pClipSize.y))
      return FALSE;

   // ignoreing z-buffer for now
   float *pf;
   DWORD *pdw;
   pf = m_pafZBuf + (iX + iY * m_pClipSize.x);
   if (*pf == -(float) INFINITY)
      return FALSE;

   pdw = m_padwObject + ((iX + iY * m_pClipSize.x) * 2);

   if (pdwMajor)
      *pdwMajor = pdw[0];
   if (pdwMinor)
      *pdwMinor = pdw[1];

   return TRUE;
}


/*******************************************************************
ShapeArrow - Draws a 3-d arrow, which is a combinations of tubs and
   a funnel on the end.

inputs
   DWORD    dwNum - number of points that define the arrow. The last
                     point has the pointy bit on it
   double   *pafPoint - array of points.
   double   fWidth - width of the body
   double   fWidth2 - Width at the end
   BOOL     fTip - if TRUE draw the arrow tip, FALSE dont
*/
void CRender::ShapeArrow (DWORD dwCount, double *pafPoint, double fWidth, double fWidth2, BOOL fTip)
{
   if (dwCount < 2)
      return;

   DWORD i;
   pnt   p;
   double   *p1, *p2;
   double   f;
   BOOL  fLast;
   double   fTipHeight, fTipWidth;
   fTipHeight = fWidth2 * 3;
   fTipWidth = fWidth2 * 2;

   double fDelta = fWidth2 - fWidth;

   // flatten the first one
   for (i = 0; i < (dwCount-1); i++) {
      fLast = (i == (dwCount-2));
      p1 = pafPoint + (i * 4);
      p2 = pafPoint + ((i+1) * 4);
      SubVector (p2, p1, p);
      f = sqrt (p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);

      mpush();
      TransRot (p1, p2);
      Rotate (-PI/2, 1);  // so Z translated to Y

      double fAlpha = (double)i / (double)(dwCount-1);
      double fAlpha2 = (double)(i+1) / (double)(dwCount-1);
      MeshFunnel (f - ((fTip && fLast) ? fTipHeight : 0),
         (1.0 - fAlpha) * fWidth + fAlpha * fWidth2,
         (1.0 - fAlpha2) * fWidth + fAlpha2 * fWidth2);
      ShapeMeshSurface ();

      // draw the arrow
      if (fTip && fLast) {
         Translate (0, f - fTipHeight, 0);
         MeshFunnel (fTipHeight, fTipWidth, 0);
         ShapeMeshSurface ();
      }
      mpop();
   }
}

/*******************************************************************
ShapeTeapot - Draws a teapot. The teapot uses a mesh, and if there's
   a bump map or color map will use that. The teapot is centered around
   the Y axis. It's base is at 0.
*/
void CRender::ShapeTeapot (void)
{
   DWORD x, y;

   // main part
   pnt   apMain[13] = { 1.4000, 2.25000, 0, 1,
                        1.3375, 2.38125, 0, 1,
                        1.4375, 2.38125, 0, 1,
                        1.5000, 2.25000, 0, 1,
                        1.7500, 1.72500, 0, 1,
                        2.0000, 1.20000, 0, 1,
                        2.0000, 0.75000, 0, 1,
                        2.0000, 0.30000, 0, 1,
                        1.5000, 0.07500, 0, 1,
                        1.5000, 0.00000, 0, 1,
                        1.0000, 0.00000, 0, 1,
                        0.5000, 0.00000, 0, 1,
                        0.0000, 0.00000, 0, 1
                        };

   MeshRotation (13, (double*) apMain);
   BumpMapApply ();  // if there is any
   BumpMapFree ();   // just so don't use for everything else
   ShapeMeshSurface ();

   pnt   apLid[7] = {   0.0, 3.00, 0, 1,
                        0.8, 3.00, 0, 1,
                        0.0, 2.70, 0, 1,
                        0.2, 2.55, 0, 1,
                        0.4, 2.40, 0, 1,
                        1.3, 2.40, 0, 1,
                        1.3, 2.25, 0, 1
                        };
   MeshRotation (7, (double*) apLid);
   ShapeMeshSurface ();

   // handle
   pnt   apHandleIn[7]= {  -1.60, 1.8750, 0, 1,
                           -2.30, 1.8750, 0, 1,
                           -2.70, 1.8750, 0, 1,
                           -2.70, 1.6500, 0, 1,
                           -2.70, 1.4250, 0, 1,
                           -2.50, 0.9750, 0, 1,
                           -2.00, 0.7500, 0, 1
                        };
   pnt   apHandleOut[7]= { -1.50, 2.1000, 0, 1,
                           -2.50, 2.1000, 0, 1,
                           -3.00, 2.1000, 0, 1,
                           -3.00, 1.6500, 0, 1,
                           -3.00, 1.2000, 0, 1,
                           -2.65, 0.7875, 0, 1,
                           -1.90, 0.4500, 0, 1
                          }; 

   pnt   apHandle[6][7];
   for (x = 0; x < 6; x++) {
      double   *p;

      switch (x) {
      case 0:
      case 1:
      case 5:
         p = (double*) apHandleIn;
         break;
      case 2:
      case 3:
      case 4:
         p = (double*) apHandleOut;
      }

      memcpy (apHandle[x], p, sizeof(apHandleIn));

      // set Z
      double   z;
      switch (x) {
      case 0:
      case 3:
         z = 0;
         break;
      case 1:
      case 2:
         z = 0.3;
         break;
      case 4:
      case 5:
         z = -0.3;
         break;
      }
      for (y = 0; y < 7; y++)
         apHandle[x][y][2] = z;
   }

   // draw it
   MeshBezier (2, 7, 6, (double*) apHandle);
   ShapeMeshSurface ();

   // Spout
   pnt   apSpoutIn[7]=  {  1.700, 1.27500, 0, 1,
                           2.600, 1.27500, 0, 1,
                           2.300, 1.95000, 0, 1,
                           2.700, 2.25000, 0, 1,
                           2.800, 2.32500, 0, 1,
                           2.900, 2.32500, 0, 1,
                           2.800, 2.25000, 0, 1
                        };
   pnt   apSpoutOut[7]=  { 1.700, 0.45000, 0, 1,
                           3.100, 0.67500, 0, 1,
                           2.400, 1.87500, 0, 1,
                           3.300, 2.25000, 0, 1,
                           3.525, 2.34375, 0, 1,
                           3.450, 2.36250, 0, 1,
                           3.200, 2.25000, 0, 1
                          }; 

   pnt   apSpout[6][7];
   for (x = 0; x < 6; x++) {
      double   *p;

      switch (x) {
      case 0:
      case 1:
      case 5:
         p = (double*) apSpoutIn;
         break;
      case 2:
      case 3:
      case 4:
         p = (double*) apSpoutOut;
      }

      memcpy (apSpout[x], p, sizeof(apSpoutIn));

      // set Z
      double   z;
      switch (x) {
      case 0:
      case 3:
         z = 0;
         break;
      case 1:
      case 2:
         z = 0.3;
         break;
      case 4:
      case 5:
         z = -0.3;
         break;
      }
      for (y = 0; y < 7; y++)
         apSpout[x][y][2] = z;
   }

   // draw it
   MeshBezier (2, 7, 6, (double*) apSpout);
   ShapeMeshSurface ();
}

/*******************************************************************
ShapeBox - draw a box
*/
void CRender::ShapeBox (double x, double y, double z, double *pColor)  // draws a box
{
#define  XLEFT    0x01
#define  XRIGHT   0
#define  YBOTTOM  0x02
#define  YTOP     0
#define  ZBACK    0x04
#define  ZFRONT   0

   int   i;
   pnt   apCorner[8];   // bit 0 is left/right. 1 = up/down. 2 = forward/back
   for (i = 0; i < 8; i++) {
      // values
      apCorner[i][0] = ((i & XLEFT) ? -1 : 1) * x / 2;
      apCorner[i][1] = ((i & YBOTTOM) ? -1 : 1) * y / 2;
      apCorner[i][2] = ((i & ZBACK) ? -1 : 1) * z / 2;
   }

   ShapeBox (apCorner, pColor);
}


/*******************************************************************
ShapeFlatPyramid - Draws a flattened pyramid. The base in on the XY
play, coming in the Z direction (towards viewer)

inputs
   double      fBaseX - base width
   double      fBaseY - base heigtht
   double      fTopX - top width
   double      fTopY - top height
   double      fZ - Height (from 0.0 andup)
   double      *pColor - color, or NULL
returns
   none
*/
void CRender::ShapeFlatPyramid (double fBaseX,double fBaseY, double fTopX, double fTopY,
                                double fZ, double *pColor)
{
   int   i;
   pnt   apCorner[8];   // bit 0 is left/right. 1 = up/down. 2 = forward/back
   for (i = 0; i < 8; i++) {
      // values
      apCorner[i][0] = ((i & XLEFT) ? -1 : 1) * ((i&ZBACK) ? fBaseX : fTopX) / 2;
      apCorner[i][1] = ((i & YBOTTOM) ? -1 : 1) * ((i&ZBACK) ? fBaseY : fTopY) / 2;
      apCorner[i][2] = ((i & ZBACK) ? 0 : 1) * fZ;
   }

   ShapeBox (apCorner, pColor);
}

/*******************************************************************
ShapeBox - draw a box - Takes 8 corners
   pnt      apCorner[8] -  bit 0 is left/right. 1 = up/down. 2 = forward/back
*/
void CRender::ShapeBox (pnt apCorner[8], double *pColor)  // draws a box
{
   // try for a trivial clip
   pnt   pMin, pMax;
   CopyPnt (apCorner[0], pMin);
   CopyPnt (apCorner[0], pMax);
   int   i;
   for (i = 1; i < 8; i++) {
      pMin[0] = min(pMin[0], apCorner[i][0]);
      pMin[1] = min(pMin[1], apCorner[i][1]);
      pMin[2] = min(pMin[2], apCorner[i][2]);
      pMax[0] = max(pMax[0], apCorner[i][0]);
      pMax[1] = max(pMax[1], apCorner[i][1]);
      pMax[2] = max(pMax[2], apCorner[i][2]);
   }
   if (BoundingBox (pMin[0], pMin[1], pMin[2], pMax[0], pMax[1], pMax[2]))
      return;

   if (!pColor)
      pColor = m_DefColor;

   // IMPORTANT - making a box as a set of polygons.
   // Might ultimately be too slow since doing a few extra calculations
   // that don't need to


   // default color
   pnt   aColor[4];
   for (i = 0; i < 4; i++)
      CopyPnt (pColor, aColor[i]);

   // vertices. Have to be clockwise on each size
   pnt   aVert[4];

   // front side
   CopyPnt (apCorner[XLEFT | YTOP | ZFRONT], aVert[0]);
   CopyPnt (apCorner[XRIGHT | YTOP | ZFRONT], aVert[1]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZFRONT], aVert[2]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZFRONT], aVert[3]);
   m_dwMinorObjectID = 0;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // right side
   CopyPnt (apCorner[XRIGHT | YTOP | ZFRONT], aVert[0]);
   CopyPnt (apCorner[XRIGHT | YTOP | ZBACK], aVert[1]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZBACK], aVert[2]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZFRONT], aVert[3]);
   m_dwMinorObjectID = 1;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // back side
   CopyPnt (apCorner[XRIGHT | YTOP | ZBACK], aVert[0]);
   CopyPnt (apCorner[XLEFT | YTOP | ZBACK], aVert[1]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZBACK], aVert[2]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZBACK], aVert[3]);
   m_dwMinorObjectID = 2;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // left side
   CopyPnt (apCorner[XLEFT | YTOP | ZBACK], aVert[0]);
   CopyPnt (apCorner[XLEFT | YTOP | ZFRONT], aVert[1]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZFRONT], aVert[2]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZBACK], aVert[3]);
   m_dwMinorObjectID = 3;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // top side
   CopyPnt (apCorner[XLEFT | YTOP | ZFRONT], aVert[0]);
   CopyPnt (apCorner[XLEFT | YTOP | ZBACK], aVert[1]);
   CopyPnt (apCorner[XRIGHT | YTOP | ZBACK], aVert[2]);
   CopyPnt (apCorner[XRIGHT | YTOP | ZFRONT], aVert[3]);
   m_dwMinorObjectID = 4;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // bottom side
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZFRONT], aVert[0]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZFRONT], aVert[1]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZBACK], aVert[2]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZBACK], aVert[3]);
   m_dwMinorObjectID = 5;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);
}


/*******************************************************************
ShapeDeepArrow - Creates a raised arrow pointing to the right and
comig out of the -z (towards viewer) direction.

inputs
   double      fX - XWidth
   double      dY - Y height
   double      fZ - height (0.0 and up)
   double      *pColor -c olor
returns
   none
*/
void CRender::ShapeDeepArrow (double fX, double fY, double fZ, double *pColor)
{
   // arrow
   mpush();
   Translate (fX * 1.0 / 3.0, 0, 0);
   ShapeDeepTriangle (fX * 2 / 3, fY, fX * 2 / 3, fY, fZ, pColor);
   mpop();

   // box part
   mpush();
   Translate (-fX * 2.0 / 6.0 / 2.0, 0, fZ / 2);
   ShapeBox (fX / 3, fY / 2, fZ, pColor);
   mpop();

}


/*******************************************************************
ShapeDeepFrame - Draws a picture frame. Depth is z (towards teh viewer)

inputs
   double      fX, fY, fZ - XY size, and Y is height
   doubl       fFrameBase, fFrameTop - size of the frame at the base anda t the top
   double      *pColor
*/
void CRender::ShapeDeepFrame (double fX, double fY, double fZ,
                              double fFrameBase, double fFrameTop, double *pColor)
{
   // left side
   mpush();
   Translate (-(fX/2 - fFrameBase/2), 0, 0);
   ShapeFlatPyramid (fFrameBase, fY, fFrameTop, fY - 2*fFrameTop, fZ, pColor);
   mpop();

   // right side
   mpush();
   Translate ((fX/2 - fFrameBase/2), 0, 0);
   ShapeFlatPyramid (fFrameBase, fY, fFrameTop, fY - 2*fFrameTop, fZ, pColor);
   mpop();

   // top side
   mpush();
   Translate (0, -(fY/2 - fFrameBase/2), 0);
   ShapeFlatPyramid (fX, fFrameBase, fX - 2*fFrameTop, fFrameTop, fZ, pColor);
   mpop();

   // bottom side
   mpush();
   Translate (0, (fY/2 - fFrameBase/2), 0);
   ShapeFlatPyramid (fX, fFrameBase, fX - 2*fFrameTop, fFrameTop, fZ, pColor);
   mpop();
}

/*******************************************************************
ShapeDeepTriangle - Draws a triangle width depth. Depth is in the -z (towards
the viewer) direction. Triangle points to the right. It can also be bevelled.

inputs
   double      fBaseX - Width along X direction
   double      fBaseY - Height along Y direction
   double      fTopX - Width along top X direction
   double      fTopY - Height along top Y direction
   double      fZ - Height (from 0.0 andup)
   double      *pColor - color, or NULL
returns
   none
*/
void CRender::ShapeDeepTriangle (double fBaseX,double fBaseY, double fTopX, double fTopY,
                                double fZ, double *pColor)
{
   int   i;
   pnt   apCorner[8];   // bit 0 is left/right. 1 = up/down. 2 = forward/back
   for (i = 0; i < 8; i++) {
      // values
      apCorner[i][0] = ((i & XLEFT) ? -1 : 1) * ((i&ZBACK) ? fBaseX : fTopX) / 2;
      if (i & XLEFT)
         apCorner[i][1] = ((i & YBOTTOM) ? -1 : 1) * ((i&ZBACK) ? fBaseY : fTopY) / 2;
      else
         apCorner[i][1] = 0;  // point
      apCorner[i][2] = ((i & ZBACK) ? 0 : 1) * fZ;
   }

   // try for a trivial clip
   pnt   pMin, pMax;
   CopyPnt (apCorner[0], pMin);
   CopyPnt (apCorner[0], pMax);
   for (i = 1; i < 8; i++) {
      pMin[0] = min(pMin[0], apCorner[i][0]);
      pMin[1] = min(pMin[1], apCorner[i][1]);
      pMin[2] = min(pMin[2], apCorner[i][2]);
      pMax[0] = max(pMax[0], apCorner[i][0]);
      pMax[1] = max(pMax[1], apCorner[i][1]);
      pMax[2] = max(pMax[2], apCorner[i][2]);
   }
   if (BoundingBox (pMin[0], pMin[1], pMin[2], pMax[0], pMax[1], pMax[2]))
      return;

   if (!pColor)
      pColor = m_DefColor;

   // IMPORTANT - making a box as a set of polygons.
   // Might ultimately be too slow since doing a few extra calculations
   // that don't need to


   // default color
   pnt   aColor[4];
   for (i = 0; i < 4; i++)
      CopyPnt (pColor, aColor[i]);

   // vertices. Have to be clockwise on each size
   pnt   aVert[4];

   // front side
   CopyPnt (apCorner[XLEFT | YTOP | ZFRONT], aVert[0]);
   CopyPnt (apCorner[XRIGHT | YTOP | ZFRONT], aVert[1]);
   //CopyPnt (apCorner[XRIGHT | YBOTTOM | ZFRONT], aVert[2]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZFRONT], aVert[2]);
   m_dwMinorObjectID = 0;
   ShapePolygon (3, aVert, aColor);

   // right side
//   CopyPnt (apCorner[XRIGHT | YTOP | ZFRONT], aVert[0]);
//   CopyPnt (apCorner[XRIGHT | YTOP | ZBACK], aVert[1]);
//   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZBACK], aVert[2]);
//   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZFRONT], aVert[3]);
//   m_dwMinorObjectID = 1;
//   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // back side
   CopyPnt (apCorner[XRIGHT | YTOP | ZBACK], aVert[0]);
   CopyPnt (apCorner[XLEFT | YTOP | ZBACK], aVert[1]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZBACK], aVert[2]);
   //CopyPnt (apCorner[XRIGHT | YBOTTOM | ZBACK], aVert[3]);
   m_dwMinorObjectID = 2;
   ShapePolygon (3, aVert, aColor);

   // left side
   CopyPnt (apCorner[XLEFT | YTOP | ZBACK], aVert[0]);
   CopyPnt (apCorner[XLEFT | YTOP | ZFRONT], aVert[1]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZFRONT], aVert[2]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZBACK], aVert[3]);
   m_dwMinorObjectID = 3;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // top side
   CopyPnt (apCorner[XLEFT | YTOP | ZFRONT], aVert[0]);
   CopyPnt (apCorner[XLEFT | YTOP | ZBACK], aVert[1]);
   CopyPnt (apCorner[XRIGHT | YTOP | ZBACK], aVert[2]);
   CopyPnt (apCorner[XRIGHT | YTOP | ZFRONT], aVert[3]);
   m_dwMinorObjectID = 4;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);

   // bottom side
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZFRONT], aVert[0]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZFRONT], aVert[1]);
   CopyPnt (apCorner[XRIGHT | YBOTTOM | ZBACK], aVert[2]);
   CopyPnt (apCorner[XLEFT | YBOTTOM | ZBACK], aVert[3]);
   m_dwMinorObjectID = 5;
   ShapePolygon (sizeof(aVert) / sizeof(pnt), aVert, aColor);
}


/*******************************************************************
Commit - Causes the image to be drawn to the HDC if it isn't already.
   Takes the area on the screen that drew and blits it into the given
   location

inputs
   HDC   hDCNew - Set to NULL if should use old HDC, else will use new
   RECT  *prHDC - Rectangle to blit into given in HDC coordinates.
            This should be the same size prScreen in InitForControl().
   BOOL  fObjectOnly - If TRUE, only transfer the pixels with a non-zero
            major object ID.

*/
void CRender::Commit (HDC hDCNew, RECT *prHDC, BOOL fObjectsOnly)
{
   if (!m_pafZBuf)
      return;  // no z buffer. nothing else is there
   
   if (hDCNew)
      m_hDC = hDCNew;

   if (fObjectsOnly) {
      BOOL  fPrinter = FALSE;
      if (EscGetDeviceCaps(m_hDC, TECHNOLOGY) == DT_RASPRINTER)
         fPrinter = TRUE;

      // if it's a printer then we have to create a mask because inevitably
      // the bitmap will be stretched
      HDC   hDCMask;
      hDCMask = NULL;
      HBITMAP  hBmpMask;
      hBmpMask = NULL;
      if (fPrinter) {
         HDC   hScreen;
         hScreen = GetDC(NULL);
         hDCMask = CreateCompatibleDC (hScreen);
         hBmpMask = CreateCompatibleBitmap (hScreen, m_pClipSize.x, m_pClipSize.y);
         ReleaseDC (NULL, hScreen);
         SelectObject (hDCMask, hBmpMask);
         RECT  r;
         r.left = r.top = 0;
         r.right = m_pClipSize.x;
         r.bottom = m_pClipSize.y;
         FillRect (hDCMask, &r, (HBRUSH) GetStockObject (WHITE_BRUSH));
      }
      else
         hDCMask = m_hDC;

      // loop through hdc and set pixels to 0 where objects are
      int   xDC, yDC, xBmp, yBmp;
      DWORD *pdw;
      for (yDC = prHDC->top, yBmp = 0; yBmp < m_pClipSize.y; yBmp++, yDC++) {
         pdw = m_padwObject + ((yBmp * m_pClipSize.x) * 2);
         for (xDC = prHDC->left, xBmp = 0; xBmp < m_pClipSize.x; xBmp++, xDC++, pdw += 2) {
            // if no object conintue
            if (!pdw[0])
               continue;

            // else, set pixel to black
            if (fPrinter)
               SetPixel (hDCMask, xBmp, yBmp, 0);
            else
               SetPixel (hDCMask, xDC, yDC, 0);
         }
      }

      if (fPrinter) {
         EscBitBlt (m_hDC, prHDC->left, prHDC->top, prHDC->right - prHDC->left, prHDC->bottom - prHDC->top,
            hDCMask, 0, 0, SRCAND, hBmpMask);
         DeleteDC (hDCMask);
         DeleteObject (hBmpMask);
      }
   }

   if (m_hDCDraw) {
      // bit blit
      EscBitBlt (m_hDC, prHDC->left, prHDC->top, prHDC->right - prHDC->left, prHDC->bottom - prHDC->top,
         m_hDCDraw, 0, 0, fObjectsOnly ? SRCPAINT : SRCCOPY, m_hBitDraw);
   }
   else {
      m_hBitDraw = CreateBitmapIndirect (&m_bm);

      // bit blit
      HDC   hDCCompat;
      HBITMAP  hBitOld;
      hDCCompat = CreateCompatibleDC (m_hDC);
      hBitOld = (HBITMAP) SelectObject (hDCCompat, m_hBitDraw);
      EscBitBlt (m_hDC, prHDC->left, prHDC->top, prHDC->right - prHDC->left, prHDC->bottom - prHDC->top,
         hDCCompat, 0, 0, fObjectsOnly ? SRCPAINT : SRCCOPY, m_hBitDraw);
      SelectObject (hDCCompat, hBitOld);
      DeleteDC (hDCCompat);

      // delete teh generated bitmap
      DeleteObject (m_hBitDraw);
      m_hBitDraw = NULL;
   }

}



/********************************************************************
ColorMapFree - Frees the color map if there is one.
*/
void CRender::ColorMapFree (void)
{
   if (m_pdwColorMap)
      ESCFREE (m_pdwColorMap);
   m_pdwColorMap = NULL;
}

/********************************************************************
ColorMap - Specifies a color map to use on the next surface. If a
   color map already exists then that one is cleared. Note that the
   surface will have to be width x height facets to actually draw
   the color map
inputs
   DWORD    dwWidth, dwHeight - width and ehight.
                  Arrayed like [dwWidth][dwHeight].
                  For now, the upper left corner is at 0,0
   DWORD     *padwData - data. size is width * height * 4 bytes
*/
void CRender::ColorMap (DWORD dwWidth, DWORD dwHeight, const DWORD *padwData)
{
   ColorMapFree ();

   DWORD  *pf;
   pf = (DWORD*) ESCMALLOC (dwWidth * dwHeight * 4);
   if (!pf)
      return;
   memcpy (pf, padwData, dwWidth * dwHeight * 4);

   m_pdwColorMap = pf;
   m_dwColorMapX = dwWidth;
   m_dwColorMapY = dwHeight;

}


/********************************************************************
ColorMapFromBitmap - Loads in a color map from a bitmap.

inputs
   HBITMAP  hBit - bitmap
*/
void CRender::ColorMapFromBitmap (HBITMAP hBit)
{
   ColorMapFree ();

   HDC   hDC = NULL, hDCMem = NULL;
   HBITMAP  hBitOld = NULL;
   DWORD   *pf = NULL;

   // create a bitmap out of this
   hDC = GetDC (NULL);
   if (!hDC) {
      goto done;
   }

   // what if user doens't have good number of planes?
   hDCMem = CreateCompatibleDC (hDC);
   if (!hDCMem) {
      goto done;
   }
   hBitOld = (HBITMAP) SelectObject (hDCMem, hBit);

   // get the size of the bitmap
   BITMAP   bm;
   GetObject (hBit, sizeof(bm), &bm);

   // create the database
   pf = (DWORD*) ESCMALLOC (bm.bmWidth * bm.bmHeight * sizeof(DWORD));
   if (!pf)
      goto done;


   DWORD x,y;
   for (y = 0; (int) y < bm.bmHeight; y++)
      for (x = 0; (int)x < bm.bmWidth; x++)
         pf[x+y*bm.bmWidth] = GetPixel (hDCMem, x, y /* bm.bmHeight - y - 1*/);

   m_pdwColorMap = pf;
   m_dwColorMapX = (DWORD) bm.bmWidth;
   m_dwColorMapY = (DWORD) bm.bmHeight;

done:

   if (hBitOld && hDCMem)
      SelectObject (hDCMem, hBitOld);
   if (hDCMem)
      DeleteDC (hDCMem);
   if (hDC)
      ReleaseDC (NULL, hDC);
}


/********************************************************************
BumpMapFree - Frees the color map if there is one.
*/
void CRender::BumpMapFree (void)
{
   if (m_pfBumpMap)
      ESCFREE (m_pfBumpMap);
   m_pfBumpMap = NULL;
}

/********************************************************************
BumpMap - Specifies a bump map to use on the next surface. If a
   bump map already exists then that one is cleared. Note that the
   surface should be width x height facets to actually draw
   the color map. If there's a color map, they should be the same size.
   Also, if BumpMap is called before MeshEllipsoid (or other Mesh generators)
   they will make sure their size matches the bump map
inputs
   DWORD    dwWidth, dwHeight - width and ehight.
                  Arrayed like [dwWidth][dwHeight].
                  For now, the upper left corner is at 0,0
   double   *pafData - data. size is width * height * sizeof(double).
                  Contains height in pre-rotation units.
*/
void CRender::BumpMap (DWORD dwWidth, DWORD dwHeight, const double *pafData)
{
   BumpMapFree ();

   double  *pf;
   pf = (double*) ESCMALLOC (dwWidth * dwHeight * sizeof(double));
   if (!pf)
      return;
   memcpy (pf, pafData, dwWidth * dwHeight * sizeof(double));

   m_pfBumpMap = pf;
   m_dwBumpMapX = dwWidth;
   m_dwBumpMapY = dwHeight;

}


/********************************************************************
BumpMapFromBitmap - Loads in a bumpmap map from a bitmap.

inputs
   HBITMAP  hBit - bitmap
   DWORD    dwColor - Which color to use as amplitude. 0 for red, 1 for green, 2 for blue
*/
void CRender::BumpMapFromBitmap (HBITMAP hBit, DWORD dwColor)
{
   if (dwColor > 2)
      dwColor = 2;

   BumpMapFree ();

   HDC   hDC = NULL, hDCMem = NULL;
   HBITMAP  hBitOld = NULL;
   double   *pf = NULL;

   // create a bitmap out of this
   hDC = GetDC (NULL);
   if (!hDC) {
      goto done;
   }

   // what if user doens't have good number of planes?
   hDCMem = CreateCompatibleDC (hDC);
   if (!hDCMem) {
      goto done;
   }
   hBitOld = (HBITMAP) SelectObject (hDCMem, hBit);

   // get the size of the bitmap
   BITMAP   bm;
   GetObject (hBit, sizeof(bm), &bm);

   // create the database
   pf = (double*) ESCMALLOC (bm.bmWidth * bm.bmHeight * sizeof(double));
   if (!pf)
      goto done;


   DWORD x,y;
   for (y = 0; (int) y < bm.bmHeight; y++)
      for (x = 0; (int)x < bm.bmWidth; x++) {
         DWORD dwRGB;
         dwRGB = GetPixel (hDCMem, x, y /* bm.bmHeight - y - 1*/);
//         pf[x+y*bm.bmWidth] = (GetGValue(dwRGB) > GetBValue(dwRGB)/2) ? 1.0 : 0.0;
         pf[x+y*bm.bmWidth] = (double) ((dwRGB >> (dwColor*8)) & 0xff) / 255;
      }

   m_pfBumpMap = pf;
   m_dwBumpMapX = (DWORD) bm.bmWidth;
   m_dwBumpMapY = (DWORD) bm.bmHeight;

done:

   if (hBitOld && hDCMem)
      SelectObject (hDCMem, hBitOld);
   if (hDCMem)
      DeleteDC (hDCMem);
   if (hDC)
      ReleaseDC (NULL, hDC);
}


/********************************************************************
BumpMapApply - This is separate than the other bumpmap functions because
   the bump maps affect the size of the mesh for automatically drawn
   objects. Hence, the order of calling should be:
      1) BumpMap, BumpMapFromBitmap, or BumpMapEarth
      2) MeshEllipsoid, MeshFromPoints (not adjusted), etc.
      3) BumpMapApply
      4) ShapeMeshSurface or ShapeMeshVectors
*/
void CRender::BumpMapApply (void)
{
   if ((!m_pfBumpMap || !m_pafMeshPoints))
      return;

   // make sure there are normals and they're normalized
   MeshGenerateNormals();
   BulkNormalize (m_dwMeshX * m_dwMeshY, m_pafMeshNormals);

   // add normals into mesh
   DWORD x, y, dwAcross, h, v;
   dwAcross = m_dwMeshX;
   pnt   pTemp, pNormal;
   double   f, *pf;
   for (y = 0; y < m_dwMeshY; y++) for (x = 0; x < m_dwMeshX; x++) {
      h = (m_dwMeshX == m_dwBumpMapX) ? x : (x * m_dwBumpMapX / m_dwMeshX);
      v = (m_dwMeshY == m_dwBumpMapY) ? y : (y * m_dwBumpMapY / m_dwMeshY);


      f = m_pfBumpMap[h + v * m_dwBumpMapX];
      f *= m_fBumpVectorLen * m_fBumpScale;
      if (f == 0.0)
         continue;
      pf = m_pafMeshNormals + MESH(x,y,0);
      pNormal[0] = pf[0] * f;
      pNormal[1] = pf[1] * f;
      pNormal[2] = pf[2] * f;
      // pNormal[3] = pf[3] * f; - IMPORTANT - I don't think this is right
      AddVector4 (m_pafMeshPoints + MESH(x,y,0), pNormal, pTemp);
      CopyPnt (pTemp, m_pafMeshPoints + MESH(x,y,0));
   }

   // wipe out normals and north/east since no longer valid
   if (m_pafMeshNormals)
      ESCFREE (m_pafMeshNormals);
   m_pafMeshNormals = NULL;
   if (m_pafMeshEast)
      ESCFREE (m_pafMeshEast);
   m_pafMeshEast = NULL;
   if (m_pafMeshNorth)
      ESCFREE (m_pafMeshNorth);
   m_pafMeshNorth = NULL;

   // see if it's obscured
   m_fMeshCheckedBoundary = TRUE;
   if (BoundingBox (m_dwMeshX * m_dwMeshY, m_pafMeshPoints, NULL, FALSE))
      m_fMeshShouldHide = TRUE;
}


/********************************************************************
BulkNormalize - Normalize a set of vectors to length 1

inputs
   DWORD    dwNum - number of vectors
   double*  pafNorm - pointer to the data to be read & modified. [dwNum * 4] elems
*/
void CRender::BulkNormalize (DWORD dwNum, double *pafNorm)
{
   DWORD i;
   double   f;

   for (i = 0; i < dwNum; i++, pafNorm += 4) {
      f = (pafNorm[0] * pafNorm[0] + pafNorm[1] * pafNorm[1] + pafNorm[2] * pafNorm[2]);
      if ((f == 0.0) || (f == 1.0))
         continue;
      f = 1.0 / sqrt(f);
      pafNorm[0] *= f;
      pafNorm[1] *= f;
      pafNorm[2] *= f;
      pafNorm[3] *= f;
   }
}


/********************************************************************
MapCheck - Internal functions such as MeshEllipsoid call this to
   see if there's a color map or bump-map, and adjust their
   facets accordingly.

inputs
   DWORD dwType - type. bit1 => connected on right edge, bit2=> connected on left edge
   DWORD *pdwX - start filled with X. Function may modify
   DWORD *pdwY - start filled with Y. Function may modify
*/
void CRender::MapCheck (DWORD dwFacets, DWORD *pdwX, DWORD *pdwY)
{
   // don't do anything without color/bump
   if (!m_pdwColorMap && !m_pfBumpMap)
      return;

   // figure out max
   DWORD width, height;
   width = *pdwX;
   height = *pdwY;
   if (m_pdwColorMap) {
      width = max(m_dwColorMapX, width);
      height = max(m_dwColorMapY, height);
   }
   if (m_pfBumpMap) {
      width = max(m_dwBumpMapX, width);
      height = max(m_dwBumpMapY, height);
   }

   // if haven't changed resume
   if ((width == *pdwX) && (height == *pdwY))
      return;

   // however, if facets != maxfacets, then shrink down a bit since object is
   // obviously fairly far away
   if (dwFacets != m_dwMaxFacets) {
      // try to keep odd/even relationship
      BOOL  fOddX, fOddY;
      fOddX = (width & 0x01);
      fOddY = (height & 0x01);

      while ((width > dwFacets*4) && (height > dwFacets*4)) {
         width /= 2;
         height /= 2;
         if (fOddY && !(height & 0x01))
            height += 1; // try to keep odd number vertical
         if (fOddX && !(width & 0x01))
            width += 1; // try to keep odd number vertical
      }
   }

   *pdwX = width;
   *pdwY = height;
}

/********************************************************************
ShapePolygon - draws a polygon. This is an external function that
applications will call. It does:
   1) Translates coordinates from by calling trans1() and trans2() and adjusting
         aspect. Assumes that trans1() has NOT already been called.
   2) Calculates normals
   3) Calculates color at vertices. Gauraud and ambient
   4) If in wire-frame mode then draws as wire-frame
   5) Back-face culling, if specified

inputs
   DWORD    dwCount - number of vertices. At least 3
   pnt      paVert[] - array of verticles. Should be given in clockwise
               order so back-face culling works
   ptn      paColor[] - color to interpolate at the vertices
*/


void CRender::ShapePolygon (DWORD dwCount, pnt paVert[], pnt paColor[])
{
#define  MAXPOLYSIDES   24
   if (dwCount > MAXPOLYSIDES)
      return;

   pnt   a1Vert[MAXPOLYSIDES];
   pnt   aNewVert[MAXPOLYSIDES];
   pnt   aNormal[MAXPOLYSIDES];
   pnt   aNewColor[MAXPOLYSIDES];
   DWORD i;

   // rotate around
   for (i = 0; i < dwCount; i++)
      mtrans1 (paVert[i], a1Vert[i]);

   // flatten
   for (i = 0; i < dwCount; i++) {
      mtrans2 (a1Vert[i], aNewVert[i]);
      aNewVert[i][1] *= aspect;
   }

   if (m_fBackCull) {
      // back-face culling
      for (i = 0; i < dwCount; i++)
         if (!ShouldHideIt (aNewVert[(i + dwCount - 1) % dwCount], aNewVert[i], aNewVert[(i + 1) % dwCount]))
            break;
      if (i >= dwCount)
         return;  // facing away
   }

   // calculate normals
   for (i = 0; i < dwCount; i++)
      CalcNormal (a1Vert[(i + dwCount - 1) % dwCount], a1Vert[i], a1Vert[(i + 1) % dwCount], aNormal[i]);

   // calculate color at vertices
   for (i = 0; i < dwCount; i++)
      CalcColor (paColor[i], aNormal[i], a1Vert[i][2], aNewColor[i]);

   // if wire frame then just draw frame
   if (m_fWireFrame) {
      DWORD dwNext;
      for (i = 0; i < dwCount; i++) {
         pnt   p[2];
         dwNext = (i+1) % dwCount;
         CopyPnt (aNewVert[i], p[0]);
         CopyPnt (aNewVert[dwNext], p[1]);
         p[0][1] /= aspect;
         p[1][1] /= aspect;
         mdl3 (p[0], p[1], aNewColor[i], aNewColor[dwNext]);
      }
   }
   else {
      DrawFlatPoly (dwCount, aNewVert, aNewColor);
   }
}

   
/*******************************************************************
ShapeMesh - Draws a mesh. A mesh is a 2-d array of points. The points
   are combined together to form either a bumpy plane, bumpy cylinder,
   or bumpy sphere.

   Also does:
   1) Translates coordinates from by calling trans1() and trans2() and adjusting
         aspect. Assumes that trans1() has NOT already been called.
   2) Calculates normals
   3) Calculates color at vertices. Gauraud and ambient
   4) If in wire-frame mode then draws as wire-frame
   5) Back-face culling, if specified

inputs
   Uses all the mesh information and colormap.

*/
void CRender::ShapeMeshSurface (void)
{
   DWORD    dwStyle = m_dwMeshStyle;
   DWORD    dwAcross = m_dwMeshX;
   DWORD    dwDown = m_dwMeshY;
   double   *paRotVert = m_pafMeshPoints;
   double   *paSmushVert, *paCalcColor;
   DWORD   x, y;
   paSmushVert = paCalcColor = NULL;

   // if there are no points defined then return
   if (!m_pafMeshPoints)
      return;

   // if it's already been checked then just note that it should be hidden
   if (m_fMeshCheckedBoundary && m_fMeshShouldHide)
      return;

   // allocate memory
   paSmushVert = (double*) ESCMALLOC (dwAcross * dwDown * 4 * sizeof(double));
   paCalcColor = (double*) ESCMALLOC (dwAcross * dwDown * 4 * sizeof(double));
   if (!paSmushVert || !paCalcColor)
      goto alldone;

   // flatten
   for (x = 0; x < dwAcross; x++) for (y = 0; y < dwDown; y++) {
      mtrans2 (paRotVert + MESH(x,y,0), paSmushVert + MESH(x,y,0));
      paSmushVert[MESH(x,y,1)] *= aspect;
   }

   // calculate normals
   MeshGenerateNormals();

   // calculate color at vertices
   for (x = 0; x < dwAcross; x++) for (y = 0; y < dwDown; y++) {
      double *pColor;
      double   aTemp[4];
      pColor = m_DefColor;

      // if there's a color map use this
      if (m_pdwColorMap) {
         DWORD h, v;
         h = (dwAcross == m_dwColorMapX) ? x : (x * m_dwColorMapX / dwAcross);
         v = (dwDown == m_dwColorMapY) ? y : (y * m_dwColorMapY / dwDown);

         DWORD dw;
         dw = m_pdwColorMap [h + v * m_dwColorMapX];
         aTemp[0] = GetRValue(dw);
         aTemp[1] = GetGValue(dw);
         aTemp[2] = GetBValue(dw);
         aTemp[3] = 0;

         pColor = aTemp;
      }


      CalcColor (pColor,
         m_pafMeshNormals + MESH(x,y,0),
         paRotVert[MESH(x,y,2)],
         paCalcColor + MESH(x,y,0));
   }

   // loop through all the squares
   for (x = 0; x < dwAcross; x++) for (y = 0; y < dwDown; y++) {
      // if we're at the edge, but we're not connected, then don't do
      if ((x == (dwAcross-1)) && !(dwStyle & 0x01))
         continue;
      if ((y == (dwDown-1)) && !(dwStyle & 0x02))
         continue;

      // set the minor object #
      DWORD h, v;
      if (m_pdwColorMap) {
         h = (dwAcross == m_dwColorMapX) ? x : (x * m_dwColorMapX / dwAcross);
         v = (dwDown == m_dwColorMapY) ? y : (y * m_dwColorMapY / dwDown);
      }
      else {
         h = x;
         v = y;
      }
      m_dwMinorObjectID = MAKELONG (h, v);

      // next over
      DWORD x2, y2;
      x2 = (x + 1) % dwAcross;
      y2 = (y + 1) % dwDown;

      pnt   aPoly[4];
      pnt   aPolyColor[4];
      CopyPnt (paSmushVert + MESH(x,y,0), aPoly[0]);
      CopyPnt (paSmushVert + MESH(x2,y,0), aPoly[1]);
      CopyPnt (paSmushVert + MESH(x2,y2,0), aPoly[2]);
      CopyPnt (paSmushVert + MESH(x,y2,0), aPoly[3]);

      // should we cull out?
      if (m_fBackCull) {
         DWORD i;
         for (i = 0; i < 4; i++)
            if (!ShouldHideIt (aPoly[(i+3)%4], aPoly[i], aPoly[(i+1)%4]))
               break;
         if (i >= 4)
            continue;   // skip it
      }


      if (m_fDrawNormals) {
         // draw a normal line
         pnt   aNorm[2];
         pnt   aNormColor[2];
         pnt   aFlat[2];
         CopyPnt (paRotVert + MESH(x,y,0), aNorm[0]);
         AddVector4 (aNorm[0], m_pafMeshNormals + MESH(x,y,0), aNorm[1]);
         memset (aNormColor, 0, sizeof(aNormColor));
         aNormColor[0][0] = 0xff;
         CopyPnt (m_BackColor, aNormColor[1]);
         mtrans2 (aNorm[0], aFlat[0]);
         mtrans2 (aNorm[1], aFlat[1]);
         ApplyColorMode (aNormColor[0]);
         ApplyColorMode (aNormColor[1]);
         mdl3 (aFlat[0], aFlat[1], aNormColor[0], aNormColor[1]);
      }

      CopyPnt (paCalcColor + MESH(x,y,0), aPolyColor[0]);
      CopyPnt (paCalcColor + MESH(x2,y,0), aPolyColor[1]);
      CopyPnt (paCalcColor + MESH(x2,y2,0), aPolyColor[2]);
      CopyPnt (paCalcColor + MESH(x,y2,0), aPolyColor[3]);
      // draw it
      if (m_fWireFrame) {
         pnt   p[2];
         pnt   pColor[2];

         // piece 1
         CopyPnt (aPoly[0], p[0]);
         CopyPnt (aPoly[1], p[1]);
         CopyPnt (aPolyColor[0], pColor[0]);
         CopyPnt (aPolyColor[1], pColor[1]);
         p[0][1] /= aspect;
         p[1][1] /= aspect;
         mdl3 (p[0], p[1], pColor[0], pColor[1]);

         // piece 2
         CopyPnt (aPoly[0], p[0]);
         CopyPnt (aPoly[3], p[1]);
         CopyPnt (aPolyColor[0], pColor[0]);
         CopyPnt (aPolyColor[3], pColor[1]);
         p[0][1] /= aspect;
         p[1][1] /= aspect;
         mdl3 (p[0], p[1], pColor[0], pColor[1]);

         // do right "vertical" piece if not wrap around
         CopyPnt (aPoly[1], p[0]);
         CopyPnt (aPoly[2], p[1]);
         CopyPnt (aPolyColor[1], pColor[0]);
         CopyPnt (aPolyColor[2], pColor[1]);
         p[0][1] /= aspect;
         p[1][1] /= aspect;
         mdl3 (p[0], p[1], pColor[0], pColor[1]);

         // do bottom "horitzontal" piece if not wrap around
         CopyPnt (aPoly[2], p[0]);
         CopyPnt (aPoly[3], p[1]);
         CopyPnt (aPolyColor[2], pColor[0]);
         CopyPnt (aPolyColor[2], pColor[1]);
         p[0][1] /= aspect;
         p[1][1] /= aspect;
         mdl3 (p[0], p[1], pColor[0], pColor[1]);

      }
      else {
         DrawFlatPoly (4, aPoly, aPolyColor);
      }

   }
   

alldone:
   if (paSmushVert)
      ESCFREE (paSmushVert);
   if (paCalcColor)
      ESCFREE (paCalcColor);

   ColorMapFree ();
}


/*******************************************************************
ShapeMeshVectorsFromBitmap - Load in the vectors to draw from
   a bitmap. East/West = Red, North/South = Green, Up/Down = Blue.
   Converts 0..255 to -1 to 1.

inputs
   HBITMAP  hBit - bitmap
*/
void CRender::ShapeMeshVectorsFromBitmap (HBITMAP hBit)
{
   HDC   hDC = NULL, hDCMem = NULL;
   HBITMAP  hBitOld = NULL;
   double   *pf = NULL;

   // create a bitmap out of this
   hDC = GetDC (NULL);
   if (!hDC) {
      goto done;
   }

   // what if user doens't have good number of planes?
   hDCMem = CreateCompatibleDC (hDC);
   if (!hDCMem) {
      goto done;
   }
   hBitOld = (HBITMAP) SelectObject (hDCMem, hBit);

   // get the size of the bitmap
   BITMAP   bm;
   GetObject (hBit, sizeof(bm), &bm);

   // create the database
   pf = (double*) ESCMALLOC (bm.bmWidth * bm.bmHeight * 4 * sizeof(double));
   if (!pf)
      goto done;


   DWORD x,y;
   for (y = 0; (int) y < bm.bmHeight; y++)
      for (x = 0; (int)x < bm.bmWidth; x++) {
         DWORD dwRGB = GetPixel (hDCMem, x, y);
         double   *pCur;
         pCur = pf + (x + y * bm.bmWidth) * 4;
         pCur[0] = ((double) GetRValue(dwRGB) - 128) / 128.0;
         pCur[1] = ((double) GetGValue(dwRGB) - 128) / 128.0;
         pCur[2] = ((double) GetBValue(dwRGB) - 128) / 128.0;
         pCur[3] = 1;
      }

   ShapeMeshVectors ((DWORD) bm.bmWidth, (DWORD) bm.bmHeight, pf);

done:
   if (pf)
      ESCFREE (pf);

   if (hBitOld && hDCMem)
      SelectObject (hDCMem, hBitOld);
   if (hDCMem)
      DeleteDC (hDCMem);
   if (hDC)
      ReleaseDC (NULL, hDC);
}

/*******************************************************************
ShapeMeshVectors - Draws two dimensional array of vectors. The
   vectors are positioned starting at the mesh, and radiuating out.
   See MeshFromPoints, MeshEllipse, etc. to pre-load the mesh.
   You can also use ColorMap functions to affect the vectors colors.

   Also does:
   1) Translates normals from by calling transNormal().
      Assumes that normals are in pre-rotated space.

inputs
   DWORD    dwX, dwY - size of the mesh in xy. This should ideally be the
               same size as the colormap and mesh. If not, the number of vector
               lines will be determined by the mesh, and vector lines will be
               interpolated.
   double*  pafVectors - pointer to vectors. They are arranged like [dwY][dwX][4].
            [dwY][dwX][0] is the "east" pointing amount
            [dwY][dwX][1] is the "north" pointing amount
            [dwY][dwX][2] is the "up" pointing amount
            [dwY][dwX][3] should be 1

            The length of the vector will be preserved, although ultimately
            it's multiplied by m_fVectorScale.
*/
void CRender::ShapeMeshVectors (DWORD dwX, DWORD dwY, double *pafVectors)
{
   DWORD    dwStyle = m_dwMeshStyle;
   DWORD    dwAcross = m_dwMeshX;
   DWORD    dwDown = m_dwMeshY;
   double   *pafLine, *paCalcColor;
   DWORD   x, y, i;
   pafLine = paCalcColor = NULL;

   // if there are no points defined then return
   if (!m_pafMeshPoints)
      return;

   // make sure have normals, and that they're length 1
   MeshGenerateNorthEast ();
   BulkNormalize (m_dwMeshX * m_dwMeshY, m_pafMeshEast);
   BulkNormalize (m_dwMeshX * m_dwMeshY, m_pafMeshNorth);
   MeshGenerateNormals ();
   BulkNormalize (m_dwMeshX * m_dwMeshY, m_pafMeshNormals);

   // allocate memory
   pafLine = (double*) ESCMALLOC (m_dwMeshX * m_dwMeshY * 4 * 2 * sizeof(double));
   paCalcColor = (double*) ESCMALLOC (dwAcross * dwDown * 4 * 2 * sizeof(double));
   if (!pafLine || !paCalcColor)
      goto alldone;

   // calculate all the vector lines.
   for (y = 0; y < m_dwMeshY; y++) for (x = 0; x < m_dwMeshX; x++) {
      double   *pfDest, *pfStart;
      pnt   pVect;
      pfDest = pafLine + (x + y * m_dwMeshX) * 8;
      pfStart = m_pafMeshPoints + MESH(x,y,0);

      // start of each vector
      CopyPnt (pfStart, pfDest);

      // figure out which to use
      if ((m_dwMeshX == dwX) && (m_dwMeshY == dwY))
         CopyPnt (pafVectors + MESH(x,y,0), pVect);
      else {
         // unfortunately, need to interpolate
         double   fh, fv;
         if (m_dwMeshStyle & 0x01)  // does it wrap around
            fh = x / (double) m_dwMeshX * dwX;
         else
            fh = x / (double) (m_dwMeshX-1) * (dwX-1);

         if (m_dwMeshStyle & 0x02)  // does it wrap around
            fv = y / (double) m_dwMeshY * dwY;
         else
            fv = y / (double) (m_dwMeshY-1) * (dwY-1);

         // if dont interpolate vectors then
         if (m_fDontInterpVectors) {
            fh = floor(fh);
            fv = floor(fv);
         }

         // left and right points
         int   x1, x2, y1, y2;
         x1 = (int) floor(fh);
         x2 = (int) ceil(fh) % dwX;
         y1 = (int) floor(fv);
         y2 = (int) ceil(fv) % dwY;

         // get these
         double   *p1, *p2, *p3, *p4;
         double   n1, n2, n3, n4;
         p1 = pafVectors + (x1 + y1 * dwX) * 4;
         p2 = pafVectors + (x1 + y2 * dwX) * 4;
         p3 = pafVectors + (x2 + y1 * dwX) * 4;
         p4 = pafVectors + (x2 + y2 * dwX) * 4;
         n1 = sqrt(p1[0] * p1[0] + p1[1] * p1[1] + p1[2] * p1[2]);
         n2 = sqrt(p2[0] * p2[0] + p2[1] * p2[1] + p2[2] * p2[2]);
         n3 = sqrt(p3[0] * p3[0] + p3[1] * p3[1] + p3[2] * p3[2]);
         n4 = sqrt(p4[0] * p4[0] + p4[1] * p4[1] + p4[2] * p4[2]);

         // interpolate
         double   fDeltaX, fDeltaY, fInvDeltaX, fInvDeltaY;
         fDeltaX = fh - x1;
         fInvDeltaX = 1.0 - fDeltaX;
         fDeltaY = fv - y1;
         fInvDeltaY = 1.0 - fDeltaY;
         pnt pl, pr;
         double   fNormL, fNormR;
         DWORD i;
         for (i = 0; i < 3; i++) {
            pl[i] = p1[i] * fInvDeltaY + p2[i] * fDeltaY;
            pr[i] = p3[i] * fInvDeltaY + p4[i] * fDeltaY;
         }
         fNormL = n1 * fInvDeltaY + n2 * fDeltaY;
         fNormR = n3 * fInvDeltaY + n4 * fDeltaY;

         // combine the two
         double   fNormVect;
         for (i = 0; i < 3; i++)
            pVect[i] = pl[i] * fInvDeltaX + pr[i] * fDeltaX;
         pVect[3] = 1;
         fNormVect = fNormL * fInvDeltaX + fNormR * fDeltaX;

         // figure out the length of the current vector so can normalize properly
         double   fCur;
         fCur = sqrt(pVect[0] * pVect[0] + pVect[1] * pVect[1] + pVect[2] * pVect[2]);
         if (fCur) {
            fNormVect /= fCur;

            for (i = 0; i < 3; i++)
               pVect[i] *= fNormVect;
         }
      }

      // scale pvect
      for (i = 0; i < 3; i++)
         pVect[i] *= m_fVectorScale;

      // rotate this. we can't use trans normal. Instead use the north, east, and normal
      // vectors. They're already length 1.
      pnt   pTrans;
      double   *pNorth, *pEast, *pUp;
      pNorth = m_pafMeshNorth + MESH(x,y,0);
      pEast = m_pafMeshEast + MESH(x,y,0);
      pUp = m_pafMeshNormals + MESH(x,y,0);
      for (i = 0; i < 3; i++)
         pTrans[i] = pVect[0] * pEast[i] + pVect[1] * pNorth[i] + pVect[2] * pUp[i];
      pTrans[3] = 1.0;

      // add it
      AddVector4 (pfStart, pTrans, pfDest + 4);

      // done with mesh point
   }

   // check the boundary
   if (BoundingBox (m_dwMeshX * m_dwMeshY * 2, pafLine, NULL, FALSE))
      return;  // should clip

   // calculate the color
   for (y = 0; y < m_dwMeshY; y++) for (x = 0; x < m_dwMeshX; x++) {
      double   *pfDest;
      pfDest = pafLine + (x + y * m_dwMeshX) * 8;

      // if there's a color map use this
      double   *pColor;
      pnt   pc;
      if (m_pdwColorMap) {
         DWORD h, v;
         h = (dwAcross == m_dwColorMapX) ? x : (x * m_dwColorMapX / dwAcross);
         v = (dwDown == m_dwColorMapY) ? y : (y * m_dwColorMapY / dwDown);

         DWORD dw;
         dw = m_pdwColorMap [h + v * m_dwColorMapX];
         pc[0] = GetRValue(dw);
         pc[1] = GetGValue(dw);
         pc[2] = GetBValue(dw);
         pc[3] = 0;
      }
      else
         CopyPnt (m_DefColor, pc);

      pColor = paCalcColor + (x + y * m_dwMeshX) * 8;
      for (i = 0; i < 2; i++, pColor += 4) {
         CopyPnt (pc, pColor);
         ApplyFog (pColor, pfDest[i * 4 + 2]);
         ApplyColorMode (pColor);
      }
   }


   // draw all the lines
   for (y = 0; y < m_dwMeshY; y++) for (x = 0; x < m_dwMeshX; x++) {
      double   *pfDest, *pColor;
      pfDest = pafLine + (x + y * m_dwMeshX) * 8;
      pColor = paCalcColor + (x + y * m_dwMeshX) * 8;

      // if the points are the same then don't draw
      if ((pfDest[0] == pfDest[4]) && (pfDest[1] == pfDest[5]) && (pfDest[2] == pfDest[6]))
         continue;

      pnt   aFlat[2];
      mtrans2 (pfDest, aFlat[0]);
      mtrans2 (pfDest + 4, aFlat[1]);
      mdl3 (aFlat[0], aFlat[1], pColor, pColor + 4, m_fVectorArrows);
   }

alldone:
   if (pafLine)
      ESCFREE (pafLine);

   if (paCalcColor)
      ESCFREE (paCalcColor);

   ColorMapFree ();
}


/*******************************************************************
ShapeLine - Draws a line given 2 or more points. The points are
   specified in pre-rorated space (BEFORE a mtrans1) call.

inputs
   DWORD    dwCount - number of points
   double   *pafPoint - array of points.
   double   *pafColor - colors to use. if NULL, use default system color
   BOOL     fArrow - if TRUE, the last point will terminate with an arrow

*/
void CRender::ShapeLine (DWORD dwCount, double *pafPoint, double *pafColor, BOOL fArrow)
{
   if (dwCount < 2)
      return;

   DWORD i;
   pnt   p[2];
   pnt   c[2];
  
   // flatten the first one
   for (i = 0; i < (dwCount-1); i++) {
      mtransform (pafPoint + (i * 4), p[0]);
      mtransform (pafPoint + ((i+1) * 4), p[1]);
      CopyPnt (pafColor ? (pafColor + (i*4)) : m_DefColor, c[0]);
      CopyPnt (pafColor ? (pafColor + ((i+1)*4)) : m_DefColor, c[1]);
      ApplyFog (c[0], p[0][2]);
      ApplyFog (c[1], p[1][2]);
      ApplyColorMode (c[0]);
      ApplyColorMode (c[1]);
      mdl3 (p[0], p[1], c[0], c[1], fArrow && (i == (dwCount-2)) );
   }

}

/*******************************************************************
ShapeDot - Draws a dot. The points are
   specified in pre-rotated space (BEFORE a mtrans1) call.

inputs
   pnt      p - point

*/
void CRender::ShapeDot (pnt p)
{
   pnt   ap[2];

   CopyPnt (p, ap[0]);
   CopyPnt (p, ap[1]);

   ShapeLine (2, (double*) ap, NULL);
}

/*********************************************************************************
FindNearestUnit - Given an ideal unit size (number of units between each tick mark),
   this finds the next largest rounded one.

inputs
   double   fUnit - calculated unit
return
   double - ideal size, always larger
*/
static double FindNearestUnit (double fUnit)
{
   double afIdeal[] = {
      1000000000.0,
      500000000,
      100000000,
      50000000,
      10000000,
      5000000,
      1000000,
      500000,
      100000,
      50000,
      10000,
      5000,
      1000,
      500,
      100,
      50,
      10,
      5,
      1,
      0.5,
      0.1,
      0.05,
      0.01,
      0.005,
      0.001
   };

   DWORD i;
   for (i = 0; i < sizeof(afIdeal)/sizeof(double); i++)
      if (fUnit > afIdeal[i])
         return i ? afIdeal[i-1] : afIdeal[0];
   
   // else, last one
   return afIdeal[sizeof(afIdeal)/sizeof(double)-1];

}

/*******************************************************************
ShapeAxis - Draws an axist. Most stuff not determined yet.

inputs
   pnt      p1 - lower, left, front point of axis. Actually, exactly
                  what corner doens't matter as long as it's the oppoiste
                  of the other. p1 is used as the base of the axis.
   pnt      p2 - upper, right, back point of axis
   pnt      pu1 - Unit values (such as distance, etc.) at p1.
   pnt      pu2 - Unit values at p2
   PSTR     *papszAxis - Pointer to an array of 3 points. Each pointer points
               to an axis string. This can be NULL
   DWORD    dwFlags - flags. IE: AXIS_FOG, AXIS_XXX
   PRENDERAXISCALLBACK pCallback - Callback that's passed the axis number (0..3)
            and pCallbackUser data, App returns a string to indicate value
            to display, or NULL if should use default
*/
typedef struct {
   double   fPosSpace;     // position in pre-rotated space
   double   fValue;        // value at that point
   double   fAlpha;        // 0 = all the way at p1, 1 = all the way at a p2 point
} AXISPOINT, *PAXISPOINT;

void CRender::ShapeAxis (pnt p1, pnt p2, pnt pu1, pnt pu2, PSTR *papszAxis, DWORD dwFlags,
                         PRENDERAXISCALLBACK pCallback, PVOID pCallbackUser)
{
   DWORD i, x, y;
   PAXISPOINT  pAxisX = NULL, pAxisY = NULL, pAxisZ = NULL;
   DWORD       dwSizeAxisX = 0, dwSizeAxisY = 0, dwSizeAxisZ = 0; // # of ticks on axis
   pnt   p[2];
   pnt   pColor[2];

   // make up the 8 vertices and rotate them and screen coords
   pnt   pVert[8], pScreen[8];
   double   afX[8], afY[8];
   BOOL  fOK[8];
   for (i = 0; i < 8; i++) {
      pVert[i][0] = (i & 0x01) ? p2[0] : p1[0];
      pVert[i][1] = (i & 0x02) ? p2[1] : p1[1];
      pVert[i][2] = (i & 0x04) ? p2[2] : p1[2];
   }
   double   fMinFog, fMaxFog;
   double   xCOM, yCOM; // center of mass
   DWORD    dwCount; // count for the center of mass
   int      iMaxFontHeight;
   iMaxFontHeight = 0;
   xCOM = yCOM = 0;
   dwCount = 0;
   for (i = 0; i < 8; i++) {
      mtransform (pVert[i], pScreen[i]);
      pScreen[i][1] *= aspect;

      // if we have fog remember where to put it
      if (dwFlags & AXIS_FOG) {
         // min/max
         if (i) {
            fMinFog = min(-pScreen[i][2], fMinFog);
            fMaxFog = max(-pScreen[i][2], fMaxFog);
         }
         else {
            fMinFog = fMaxFog = (-pScreen[i][2]);
         }
      }

      if (pScreen[i][2] < 0.0) {
         DevIndepToDevDep (pScreen[i][0] / pScreen[i][3], pScreen[i][1] / pScreen[i][3],
            &afX[i], &afY[i]);
         fOK[i] = TRUE;

         // calculate the center of mass
         xCOM += afX[i];
         yCOM += afY[i];
         dwCount++;
      }
      else {
         fOK[i] = FALSE;
      }
   }
   if (dwCount) {
      xCOM /= dwCount;
      yCOM /= dwCount;
   }

   // turn on the fog if requested
  if (dwFlags & AXIS_FOG) {
     m_fFogOn = TRUE;
     fMinFog = max(EPSILON, fMinFog);
     m_fFogMaxDistance = fMaxFog + (fMaxFog - fMinFog)/5 + EPSILON;
     m_fFogMinDistance = fMinFog;
  }

   // figure out for each axis if it's more horizonal than vertical
   BOOL  fAxisHorz[3];  // if TRUE, it's mostly horizontal
   BOOL  fAxisVisible[3];
   double   fAxisUnitIncrement[3];  // number of units to increment each time
   double   fBoundX[2], fBoundY[2]; // bounding box on X & Y
   pnt apFlatAxis[3][2]; // location of the axis start and stop
   dwCount = 0;
   memset (pColor, 0, sizeof(pColor));
   for (i = 0; i < 3; i++) {
      x = 0;
      y = 1 << i;
      pColor[0][0] = pu1[i];
      pColor[1][0] = pu2[i];

      mtransform (pVert[x], p[0]);
      mtransform (pVert[y], p[1]);

      // BUGFIX - If the axis has no size then not visible
      if ((p[0][0] == p[1][0]) && (p[0][1] == p[1][1]) && (p[0][2] == p[1][2])) {
         fAxisHorz[i] = FALSE;
         fAxisVisible[i] = FALSE;
         continue;
      }

      DWORD dwClipped;
      if (!Clip (p[0], p[1], pColor[0], pColor[1], &dwClipped, TRUE)) {
         // totally clipped out
         fAxisHorz[i] = FALSE;
         fAxisVisible[i] = FALSE;
         continue;
      }
      fAxisVisible[i] = TRUE;

      double   afX[2], afY[2];
      DevIndepToDevDep (p[0][0] / p[0][3], p[0][1] / p[0][3],
         &afX[0], &afY[0]);
      DevIndepToDevDep (p[1][0] / p[1][3], p[1][1] / p[1][3],
         &afX[1], &afY[1]);

      // store this away
      apFlatAxis[i][0][0] = afX[0];
      apFlatAxis[i][0][1] = afY[0];
      apFlatAxis[i][0][2] = p[0][2];
      apFlatAxis[i][1][0] = afX[1];
      apFlatAxis[i][1][1] = afY[1];
      apFlatAxis[i][1][2] = p[1][2];

      // fill in bounding box
      if (dwCount) {
         fBoundX[0] = min(fBoundX[0], min(afX[0], afX[1]));
         fBoundX[1] = max(fBoundX[1], max(afX[0], afX[1]));
         fBoundY[0] = min(fBoundY[0], min(afY[0], afY[1]));
         fBoundY[1] = max(fBoundY[1], max(afY[0], afY[1]));
      }
      else {
         fBoundX[0] = min(afX[0], afX[1]);
         fBoundX[1] = max(afX[0], afX[1]);
         fBoundY[0] = min(afY[0], afY[1]);
         fBoundY[1] = max(afY[0], afY[1]);
      }
      dwCount++;

      // look at the slope
      fAxisHorz[i] = (fabs(p[0][0] - p[1][0]) >= fabs(p[0][1] - p[1][1]));

      // what's the maximum size of drawing a number?
      char  szNumber[] = "123456";
      RECT  rSize;
      int   iMax;
      TextFontGenerate (p[0][2]);
      TextDraw (szNumber, m_hFont, 0, 0, fAxisHorz[i] ? 1 : 0, 0, 0,
         p[0][2], &rSize);
      iMax = fAxisHorz[i] ? rSize.right : rSize.bottom;
      TextFontGenerate (p[1][2]);
      TextDraw (szNumber, m_hFont, 0, 0, fAxisHorz[i] ? 1 : 0, 0, 0,
         p[1][2], &rSize);
      iMax = max(iMax, fAxisHorz[i] ? rSize.right : rSize.bottom);
      if (iMax < 10)
         iMax = 10;   // minium size so not too closely together
      iMaxFontHeight = max(iMax, iMaxFontHeight);

      // now, how many pixels do we have to work with?
      double   fPixels;
      fPixels = fAxisHorz[i] ? (afX[0] - afX[1]) : (afY[0] - afY[1]);
      fPixels = max(fPixels, -fPixels);
      if (fPixels == 0)
         fPixels = 1;

      // now how many numbers can be displays
      double   fNumbers;
      fNumbers = fPixels / (iMax+1);

      // since we may have clipped, figure out how many units per number are possible
      double   fUnitsPerNum;
      fUnitsPerNum = pColor[0][0] - pColor[1][0];
      fUnitsPerNum = max(fUnitsPerNum, -fUnitsPerNum);
      fUnitsPerNum /= fNumbers;
      fAxisUnitIncrement[i] = FindNearestUnit (fUnitsPerNum);

   }

   // determine the width
   double fWidth;
   fWidth = fabs (p1[0] - p2[0]);
   fWidth = min(fWidth, fabs(p1[1] - p2[1]));
   // BUGFIX - If don't have Z Axis then don't get width off it
   if (p1[2] != p2[2])
      fWidth = min(fWidth, fabs(p1[2] - p2[2]));
   // BUGFIX - Change from /100 to /50
   fWidth /= 50;

   // determine how much of a buffer to put between the text and axis
   double   fBuffer;
   fBuffer = max(fBoundX[1] - fBoundX[0], fBoundY[1] - fBoundY[0]);
   fBuffer /= 25;   // same as width

   

   // draw boundary lines
   CopyPnt (p1, p[0]);
   for (i = 0; i < 3; i++) {
      CopyPnt (p1, p[1]);
      p[1][i] = p2[i];

      // if same point then don't do arrow
      if ((p[0][0] == p[1][0]) && (p[0][1] == p[1][1]) && (p[0][2] == p[1][2]))
         continue;

      ShapeArrow (2, (double*) p, fWidth, fWidth, TRUE);
   }

   // figure out which side the text goes on
   int      aiTextHorz[3], aiTextVert[3];
   for (i = 0; i < 3; i++) {
      // if not visible then can't tell
      if (!fAxisVisible[i]) {
         aiTextHorz[i] = aiTextVert[i] = 0;
         continue;
      }

      // figure on which side the center of mass is. All numbers are drawn AWAY
      // from the center of mass
      double   fRise, fRun, m;
      fRise = afY[(DWORD)(1<<i)] - afY[0];
      fRun = afX[(DWORD)(1<<i)] - afX[0];
      if (fRun == 0) {
         // can't do rise over run, since there's no change in X => vertical line

         aiTextHorz[i] = (xCOM >= afX[0]) ? -1 : 1;   // -1 is text appears to the left
         aiTextVert[i] = -1; // text will never be veritcal but make something up
                         // -1 means text appears above
      }
      else if (fRise == 0) {
         //horizontal line
         aiTextHorz[i] = -1;
         aiTextVert[i] = (yCOM >= afY[0]) ? -1 : 1;
      }
      else {
         double   fAlpha, fx, fy;
         m = fRise / fRun;
         fAlpha = (yCOM - afY[0]) / fRise;   // frise - y[1]-y[0]
         // fx = (1.0 - fAlpha) * afX[0] + fAlpha * afX[1<<i];
         fx = afX[0] + fAlpha * fRun;
         aiTextHorz[i] = (xCOM >= fx) ? -1 : 1;     // -1 then text appears to left

         // figure out where it hits the y axis
         fAlpha = (xCOM - afX[0]) / fRun;   // frise - x[1]-x[0]
         // fy = (1.0 - fAlpha) * afY[0] + fAlpha * afY[1<<i];
         fy = afY[0] + fAlpha * fRise;
         aiTextVert[i] = (yCOM >= fy) ? -1 : 1;     // -1 then text appears above
      }

   }

   // draw the axis labels
   m_fFontHeight *= 1.5;   // always draw labels a bit larger
   if (papszAxis) for (i = 0; i < 3; i++) {
      // if the axis isn't visible then don't bother drawing
      if (!fAxisVisible[i])
         continue;

      // figure out the center x & y of vertex
      double   cx, cy, cz;
      cx = (apFlatAxis[i][0][0] + apFlatAxis[i][1][0]) / 2;
      cy = (apFlatAxis[i][0][1] + apFlatAxis[i][1][1]) / 2;
      cz = (apFlatAxis[i][0][2] + apFlatAxis[i][1][2]) / 2;

      if (fAxisHorz[i]) {
         if (aiTextVert[i] > 0)
            cy = max(apFlatAxis[i][0][1], apFlatAxis[i][1][1]);
         else
            cy = min(apFlatAxis[i][0][1], apFlatAxis[i][1][1]);
         cy += iMaxFontHeight * aiTextVert[i] * 3;
      }
      else {   // vertical
         if (aiTextHorz[i] > 0)
            cx = max(apFlatAxis[i][0][0], apFlatAxis[i][1][0]);
         else
            cx = min(apFlatAxis[i][0][0], apFlatAxis[i][1][0]);
         cx += iMaxFontHeight * aiTextHorz[i] * 3;
      }

      // draw the axis there
      TextFontGenerate (cz);
      TextDraw (papszAxis[i], m_hFont,
         fAxisHorz[i] ? aiTextVert[i] : aiTextHorz[i], 0,
         fAxisHorz[i] ? 0 : 1,
         (int) cx, (int) cy, cz);
   }

   m_fFontHeight /= 1.5;   // undo label scaling

   // figure out how many points need in axis based upon how much
   // text can display

   // start with some sizes for the axis, although they may shrink
   dwSizeAxisX = (DWORD) (fabs(pu2[0] - pu1[0]) / fAxisUnitIncrement[0]) + 2;
   dwSizeAxisY = (DWORD) (fabs(pu2[1] - pu1[1]) / fAxisUnitIncrement[1]) + 2;
   dwSizeAxisZ = (DWORD) (fabs(pu2[2] - pu1[2]) / fAxisUnitIncrement[2]) + 2;

   pAxisX = (PAXISPOINT) ESCMALLOC (dwSizeAxisX * sizeof(AXISPOINT));
   pAxisY = (PAXISPOINT) ESCMALLOC (dwSizeAxisY * sizeof(AXISPOINT));
   pAxisZ = (PAXISPOINT) ESCMALLOC (dwSizeAxisZ * sizeof(AXISPOINT));
   if (!pAxisX || !pAxisY || !pAxisZ)
      goto done;

   for (i = 0; i < 3; i++) {
      double   fDeltaPosn, fDeltaUnits, fDeltaAlpha;
      double   fPosn, fUnits, fAlpha;
      fPosn = p1[i];
      fUnits = pu1[i];
      fAlpha = 0;
      fDeltaPosn = fAxisUnitIncrement[i] / fabs(pu1[i] - pu2[i]) * (p2[i] - p1[i]);
      fDeltaUnits = fAxisUnitIncrement[i] * ((pu2[i] > pu1[i]) ? 1.0 : -1.0);
      fDeltaAlpha = fAxisUnitIncrement[i] / fabs(pu1[i] - pu2[i]);

      // round to the nearest faxisunitincrement
      double   fNew, fScale;
      fNew = fUnits - myfmod (fUnits, fDeltaUnits);
      if (fDeltaUnits > 0) {
         if (fNew < fUnits)
            fNew += fDeltaUnits;
         fScale = (fNew - fUnits) / fAxisUnitIncrement[i];   // so can act as scaling
      }
      else {
         if (fNew > fUnits)
            fNew += fDeltaUnits;
         fScale = (fUnits - fNew) / fAxisUnitIncrement[i];   // so can act as scaling
      }
      fUnits = fNew;
      fPosn += fScale * fDeltaPosn;
      fAlpha += fScale * fDeltaAlpha;

      DWORD *pdwMax;
      DWORD dwMax;
      PAXISPOINT  pax;
      switch (i) {
      case 0:
         pdwMax = &dwSizeAxisX;
         dwMax = dwSizeAxisX;
         pax = pAxisX;
         break;
      case 1:
         pdwMax = &dwSizeAxisY;
         pax = pAxisY;
         dwMax = dwSizeAxisY;
         break;
      case 2:
         pdwMax = &dwSizeAxisZ;
         pax = pAxisZ;
         dwMax = dwSizeAxisZ;
      }

      // fill in
      for (*pdwMax = 0; (*pdwMax < dwMax) && (fAlpha <= 1.0); (*pdwMax)++, pax++) {
         pax->fPosSpace = fPosn;
         pax->fValue = fUnits;
         pax->fAlpha = fAlpha;
         fPosn += fDeltaPosn;
         fUnits += fDeltaUnits;
         fAlpha += fDeltaAlpha;
      }
      // when get done with this will have adjusted the number of points

   }


   // draw the edge data numbers
   for (i = 0; i < 3; i++) {
      if (!fAxisVisible[i]) {
         continue;
      }

      DWORD dwMax;
      PAXISPOINT  pax;
      switch (i) {
      case 0:
         dwMax = dwSizeAxisX;
         pax = pAxisX;
         break;
      case 1:
         dwMax = dwSizeAxisY;
         pax = pAxisY;
         break;
      case 2:
         dwMax = dwSizeAxisZ;
         pax = pAxisZ;
      }

      pnt t, tRot;
      CopyPnt (p1, t);
      for (x = 0; x < dwMax; x++, pax++) {
         // location of the tick in pre-rotated space
         for (y = 0; y < 3; y++)
            t[y] = (1.0 - pax->fAlpha) * p1[y] + pax->fAlpha * pVert[(DWORD)(1<<i)][y];
         t[i] = pax->fPosSpace;

         // rotate the point
         double   fx, fy;
         mtransform (t, tRot);
         tRot[1] *= aspect;
         if ((tRot[2] >= 0) || (tRot[3] == 0))
            continue;  // clip
         DevIndepToDevDep (tRot[0] / tRot[3], tRot[1] / tRot[3], &fx, &fy);

         // offset the text so it's not right on the axis
         if (fAxisHorz[i]) // vertical text
            fy += (aiTextVert[i] * fBuffer);
         else
            fx += (aiTextHorz[i] * fBuffer);

         // text
         char  szTemp[128];
         char  *pszDisplay;
         pszDisplay = szTemp;
         sprintf (szTemp, (fAxisUnitIncrement[i] < 1.0) ? "%.3f" : "%g", pax->fValue);

         // try the callback
         // BUGFIX - Ability to have callback for displaying what's on axis
         if (pCallback) {
            char *psz;
            psz = (pCallback)(pCallbackUser, i, pax->fValue);
            if (psz)
               pszDisplay = psz;
         }

         if (pszDisplay[0]) {
            TextFontGenerate (tRot[2]);
            TextDraw (pszDisplay, m_hFont,
               0, fAxisHorz[i] ? aiTextVert[i] : aiTextHorz[i],
               fAxisHorz[i] ? 1 : 0,
               (int) fx, (int) fy, tRot[2]);
         }

      }
   }

   // draw a 3d grid so can see intersection
   for (i = 0; i < 3; i++)
      pColor[0][i] = pColor[1][i] = (m_DefColor[i] + m_BackColor[i])/2;
   if (!(dwFlags & AXIS_DISABLEGIRD)) for (i = 0; i < 3; i++) {
      // i is the direction of the lines
      CopyPnt (p1, p[0]);
      CopyPnt (p2, p[1]);

      DWORD dwMax1, dwMax2;
      DWORD dwDim1, dwDim2;
      PAXISPOINT  pA1, pA2;
      switch (i) {
      case 0: // x lines
         dwDim1 = 1;
         dwDim2 = 2;
         dwMax1 = dwSizeAxisY;
         dwMax2 = dwSizeAxisZ;
         pA1 = pAxisY;
         pA2 = pAxisZ;
         break;
      case 1: // y lines
         dwDim1 = 0;
         dwDim2 = 2;
         dwMax1 = dwSizeAxisX;
         dwMax2 = dwSizeAxisZ;
         pA1 = pAxisX;
         pA2 = pAxisZ;
         break;
      case 2: // z lines
         dwDim1 = 0;
         dwDim2 = 1;
         dwMax1 = dwSizeAxisX;
         dwMax2 = dwSizeAxisY;
         pA1 = pAxisX;
         pA2 = pAxisY;
         break;
      }
      
      // sometimes incrememnt by more than one so don't have gazillions of lines
      DWORD dwIncX, dwIncY;
      dwIncX = dwIncY = 1;
      while ((dwMax1 / dwIncX) > m_dwMaxAxisLines)
         dwIncX++;
      while ((dwMax2 / dwIncY) > m_dwMaxAxisLines)
         dwIncY++;

      // draw individual lines
      if (!(dwFlags & AXIS_DISABLEINTERNALGIRD)) {
         for (x = 0; x < dwMax1; x += dwIncX) for (y = 0; y < dwMax2; y += dwIncY) {
            p[0][dwDim1] = p[1][dwDim1] = pA1[x].fPosSpace;
            p[0][dwDim2] = p[1][dwDim2] = pA2[y].fPosSpace;

            ShapeLine (2, (double*) p, (double*) pColor);
         }
      }

      // draw the edge of the inernal matrix to "seal it off
      for (x = 0; x < 2; x++) for (y = 0; y < dwMax2; y += dwIncY) {
         p[0][dwDim1] = p[1][dwDim1] = x ? p1[dwDim1] : p2[dwDim1];
         p[0][dwDim2] = p[1][dwDim2] = pA2[y].fPosSpace;
         ShapeLine (2, (double*) p, (double*) pColor);
      }
      for (x = 0; x < dwMax1; x += dwIncX) for (y = 0; y < 2; y++) {
         p[0][dwDim1] = p[1][dwDim1] = pA1[x].fPosSpace;
         p[0][dwDim2] = p[1][dwDim2] = y ? p1[dwDim2] : p2[dwDim2];
         ShapeLine (2, (double*) p, (double*) pColor);
      }
      for (x = 0; x < 2; x++) for (y = 0; y < 2; y++) {
         p[0][dwDim1] = p[1][dwDim1] = x ? p1[dwDim1] : p2[dwDim1];
         p[0][dwDim2] = p[1][dwDim2] = y ? p1[dwDim2] : p2[dwDim2];
         ShapeLine (2, (double*) p, (double*) pColor);
      }

   }

done:
   // free
   if (pAxisX)
      ESCFREE (pAxisX);
   if (pAxisY)
      ESCFREE (pAxisY);
   if (pAxisZ)
      ESCFREE (pAxisZ);
}


/*******************************************************************
GlassesLeft,GlassesRight - Begin rendering for a 3-d glasses mode.
   To do this, an application must create two render objects. Just
   after intiialization, it must call GlassesLeft (to generate the left
   eye) or GlassesRight (to generate the right eye). Then, it does
   the rendering. Right before calling commit (on the left eye), it
   calls pRenderLeft->GlassesMerge (&pRenderRight); This merges the two
   images together.

inputs
   double   fEyeDistance - distance that the eyes are apart. (For example,
               in centimeters). The left eye has the world shifted to the
               right by distance/2, and right eve has the world shifted left.
returns
   none
*/
void CRender::GlassesLeft (double fEyeDistance)
{
   fEyeDistance /= 2;
   Translate (fEyeDistance, 0, 0);
   m_dwColorMode = 3;

   Clear();
}


void CRender::GlassesRight (double fEyeDistance)
{
   fEyeDistance /= 2;
   Translate (-fEyeDistance, 0, 0);
   m_dwColorMode = 4;

   Clear();
}


/*********************************************************************
GlassesMerge - Merges the right image from 3d glass rendering into the
left image. (Actually the way doesn't realy matter.

inputs
   CRender  *pRight -right
*/
void CRender::GlassesMerge (CRender *pRight)
{
   // just verify sizes
   if ((pRight->m_rClip.right != m_rClip.right) || (pRight->m_rClip.bottom != m_rClip.bottom))
      return;

   // merge
   int   x, y;
   for (y = 0; y < m_pClipSize.y; y++) {
      BYTE  *pbLeft, *pbRight;
      pbLeft = m_fWriteToMemory ? (BYTE*) m_bm.bmBits + (y * m_bm.bmWidthBytes) : NULL;
      pbRight = pRight->m_fWriteToMemory ? (BYTE*) pRight->m_bm.bmBits + (y * pRight->m_bm.bmWidthBytes) : NULL;

      for (x = 0; x < m_pClipSize.x; x++) {
         // get the left RGB value
         DWORD dwRGBRight, dwRGBLeft;
         if (m_fWriteToMemory)
            dwRGBLeft = RGB (pbLeft[2], pbLeft[1], pbLeft[0]);
         else
            dwRGBLeft = GetPixel (m_hDCDraw, x, y);

         // get the right RGB value
         if (pRight->m_fWriteToMemory)
            dwRGBRight = RGB (pbRight[2], pbRight[1], pbRight[0]);
         else
            dwRGBRight = GetPixel (pRight->m_hDCDraw, x, y);

         // keep red/blue channel and store away
         DWORD dwRGB;
         dwRGB = RGB (GetRValue(dwRGBLeft), 0, GetBValue(dwRGBRight));

         // stick them back in
         if (m_fWriteToMemory) {
            pbLeft[0] = GetBValue (dwRGB);
            pbLeft[1] = GetGValue (dwRGB);
            pbLeft[2] = GetRValue (dwRGB);
         }
         else
            SetPixel (m_hDCDraw, x, y, dwRGB);


         if (pbLeft)
            pbLeft += 3;
         if (pbRight)
            pbRight += 3;
      }
   }
}




/*******************************************************************
MeshFree - Frees up all the mesh data.
*/
void CRender::MeshFree (void)
{
   m_fMeshCheckedBoundary = FALSE;
   m_fMeshShouldHide = FALSE;

   if (m_pafMeshPoints)
      ESCFREE (m_pafMeshPoints);
   m_pafMeshPoints = NULL;
   if (m_pafMeshNormals)
      ESCFREE (m_pafMeshNormals);
   m_pafMeshNormals = NULL;
   if (m_pafMeshEast)
      ESCFREE (m_pafMeshEast);
   m_pafMeshEast = NULL;
   if (m_pafMeshNorth)
      ESCFREE (m_pafMeshNorth);
   m_pafMeshNorth = NULL;
}

/*******************************************************************
MeshGenerateNorthEast - If there's no info for north/east, this
   generates the values automatically. These are not normalized!!!
   And they're rorated space, so only require trans2!!!
*/
void CRender::MeshGenerateNorthEast (void)
{
   if (m_pafMeshEast || m_pafMeshNorth)
      return;

   m_pafMeshEast = (double*) ESCMALLOC (m_dwMeshX * m_dwMeshY * 4 * sizeof(double));
   m_pafMeshNorth = (double*) ESCMALLOC (m_dwMeshX * m_dwMeshY * 4 * sizeof(double));
   if (!m_pafMeshEast || !m_pafMeshNorth)
      return;

   DWORD x,y, dwAcross;
   dwAcross = m_dwMeshX;
   for (x = 0; x < m_dwMeshX; x++) for (y = 0; y < m_dwMeshY; y++) {
      DWORD x1, x2, y1, y2;   // points in mesh that will use

      // left/right
      // take entriest to either side
      x1 = (x + m_dwMeshX - 1) % m_dwMeshX;
      x2 = (x + 1) % m_dwMeshX;
      if (!(m_dwMeshStyle & 0x01)) {
         // not, connected on left right, so min/max with edge
         x1 = min(x, x1);
         x2 = max(x, x2);
      }

      // top bottom
      y1 = (y + m_dwMeshY - 1) % m_dwMeshY;
      y2 = (y + 1) % m_dwMeshY;
      if (!(m_dwMeshStyle & 0x02)) {
         // not, connected on left right, so min/max with edge
         y1 = min(y, y1);
         y2 = max(y, y2);
      }

      SubVector (m_pafMeshPoints + MESH(x2,y,0), m_pafMeshPoints + MESH(x1, y, 0),
         m_pafMeshEast + MESH(x,y,0));
      SubVector (m_pafMeshPoints + MESH(x,y1,0), m_pafMeshPoints + MESH(x,y2,0),
         m_pafMeshNorth + MESH(x,y,0));
   }

}

/*******************************************************************
MeshGenerateNormals - If there's no normal information, generates it
   from m_pafMeshPoints. These are not normalized to 1 though!!!

   The mesh vectors are in rotated space, so only require trans2!
*/
void CRender::MeshGenerateNormals (void)
{
   if (m_pafMeshNormals)
      return;

   // generate the east & north directions
   MeshGenerateNorthEast();

   m_pafMeshNormals = (double*) ESCMALLOC (m_dwMeshX * m_dwMeshY * 4 * sizeof(double));
   if (!m_pafMeshNormals)
      return;


   DWORD x,y, dwAcross;
   dwAcross = m_dwMeshX;
   for (x = 0; x < m_dwMeshX; x++) for (y = 0; y < m_dwMeshY; y++) {
      CrossProd (m_pafMeshEast + MESH(x,y,0), m_pafMeshNorth + MESH(x,y,0), 
         m_pafMeshNormals + MESH(x,y,0));
   }

}


/*******************************************************************
MeshBezier - Creates a mesh from a series of points. It uses bezier
   splines to smooth out the mesh.

inputs
   DWORD    dwType - See m_dwMeshType
   DWORD    dwX, dwY - width and height in points
               if (dwType & 0x01) then it's connected on the right side
                  and dwX = n*3. Else dwX = n*3+1
                  Where n is an integer
               if (dwType & 0x02) then it's connected on the top and bottom
                  dwY = n*3. Else dwY = n*3+1
   double   *paPoints - Points. [dwY][dwX][4]. These must be in non-rotated space!
*/
void CRender::MeshBezier (DWORD dwType, DWORD dwX, DWORD dwY, double *paPoints)
{
   long facets = 8;
   long  i,j;
   double   **ppaf = NULL;
   double   *pafTemp = NULL;
   double   *s;
   s =NULL;

   // may already have done so
   // should see if bounding box will rule out immediately
   BOOL fHidden;
   fHidden = FALSE;
   if (BoundingBox (dwX * dwY, paPoints, (DWORD*) &facets)) {
      fHidden = TRUE;
      // because might be adding bump map or vectors later, can't do any more
   }


   long  width, height;
   width = dwX;
   height = dwY;

   // adjust color colormap or bumpmap size
   MapCheck ((DWORD) facets, (DWORD *) &width, (DWORD *) &height);

   // because beziers must be a fixed size, keep on increasing until match
   DWORD dwBezX, dwBezY;
   for (dwBezX = dwX; dwBezX < max((DWORD)facets, (DWORD) width); ) {
      if (dwType & 0x01)
         dwBezX *= 2;
      else
         dwBezX = (dwBezX-1)*2+1;
   }
   for (dwBezY = dwY; dwBezY < max((DWORD)facets, (DWORD) height); ) {
      if (dwType & 0x02)
         dwBezY *= 2;
      else
         dwBezY = (dwBezY-1)*2+1;
   }

   // expand all the bezier curves along the X axis
   double   *pf;
   ppaf = (double**) ESCMALLOC(dwY * sizeof(double*));
   memset (ppaf, 0, dwY * sizeof(double*));
   if (!ppaf)
      return;
   for (i = 0; i < (long) dwY; i++) {
      pf = BezierArray (dwX, paPoints + (i * dwX)*4, dwType & 0x01, dwBezX);
      if (!pf)
         goto done;
      ppaf[i] = pf;
   }

   // now, expand all these along the Y axis
   s = (double*) ESCMALLOC (dwBezX * dwBezY * 4 * sizeof(double));
   if (!s)
      goto done;
   pafTemp = (double*) ESCMALLOC (dwY * 4 * sizeof(double));
   for (i = 0; i < (long) dwBezX; i++) {
      // fill pafTemp with the Y bezier points
      for (j = 0; j < (long) dwY; j++)
         CopyPnt (ppaf[j] + (i*4), pafTemp + (j*4));

      // expand
      double   *pBezRet;
      pBezRet = BezierArray (dwY, pafTemp, dwType & 0x02, dwBezY);
      if (!pBezRet)
         goto done;  // failed for some reason

      // while we're here, copy to s
      for (j = 0; j < (long) dwBezY; j++)
         CopyPnt (pBezRet + (j * 4), s + (i + j * dwBezX)*4);

      // free  it up
      ESCFREE (pBezRet);
   }

   MeshFromPoints (dwType, dwBezX, dwBezY, s, NULL, NULL, NULL, FALSE);
   // don't bother generate north and east because they're automaticaly calculated easily enough

   // note that already checked
   m_fMeshCheckedBoundary = TRUE;
   m_fMeshShouldHide = fHidden;

done:
   // fre memory
   if (s)
      ESCFREE (s);
   if (pafTemp)
      ESCFREE (pafTemp);

   if (ppaf) {
      // BUGFIX - memory leak
      for (i = 0; i < (long) dwY; i++) {
         if (ppaf[i])
            ESCFREE (ppaf[i]);
      }
      ESCFREE (ppaf);
   }
}


   

/*******************************************************************
MeshFromPoints - Creates a mesh from points.

inputs
   DWORD    dwType - See m_dwMeshType
   DWORD    dwX, dwY - width and height
   double*  paPoints - points. [dwY][dwX][4]. THese must be in non-rotated space.
   double*  paNormals - Same style as points. Non-rotated space. They don't have
               to be normalized. If this is null, normals are automatically calculated
   double*  paEast - East pointing normal. Same info as normals, and can be NULL
   double*  paNorth - North pointing normal. Same info as normals, and can be NULL
*/
void CRender::MeshFromPoints (DWORD dwType, DWORD dwX, DWORD dwY,
                              double *paPoints,
                              double *paNormals, double *paEast, double *paNorth,
                              BOOL fCheckBoundary)
{
   MeshFree();

   if (fCheckBoundary) {
      // may already have done so
      // should see if bounding box will rule out immediately
      if (BoundingBox (dwX * dwY, paPoints))
         m_fMeshShouldHide = TRUE;
   }

   m_dwMeshStyle = dwType;
   m_dwMeshX = dwX;
   m_dwMeshY = dwY;

   DWORD dwSize;
   dwSize = dwX * dwY * 4 * sizeof(double);
   m_pafMeshPoints = (double*) ESCMALLOC (dwSize);
   if (paNormals)
      m_pafMeshNormals = (double*) ESCMALLOC (dwSize);
   if (paEast && paNorth) {
      m_pafMeshEast = (double*) ESCMALLOC (dwSize);
      m_pafMeshNorth = (double*) ESCMALLOC (dwSize);
   }

   DWORD x, y, dwAcross;
   dwAcross = m_dwMeshX;
   if (m_pafMeshPoints)
      for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++)
         mtrans1 (paPoints + MESH(x,y,0), m_pafMeshPoints + MESH(x,y,0));
   if (m_pafMeshNormals)
      for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++)
         mtransNormal (paNormals + MESH(x,y,0), m_pafMeshNormals + MESH(x,y,0));
   if (m_pafMeshEast)
      for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++)
         mtransNormal (paEast + MESH(x,y,0), m_pafMeshEast + MESH(x,y,0));
   if (m_pafMeshNorth)
      for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++)
         mtransNormal (paNorth + MESH(x,y,0), m_pafMeshNorth + MESH(x,y,0));

   // while we're at it, remember the "scale" here, so if bumps or
   // vectors are used, we have it right
   pnt   pZero, pOne, pOut;
   memset (pZero, 0, sizeof(pZero));
   pZero[0] = 1.0;
   mtrans1 (pZero, pOne);
   memset (pOut, 0, sizeof(pOut));
   mtrans1 (pOut, pZero);
   SubVector (pOne, pZero, pOut);
   m_fBumpVectorLen = sqrt(pOut[0] * pOut[0] + pOut[1] * pOut[1] + pOut[2] * pOut[2]);

}


/*******************************************************************
MeshEllipsoid - Draws an ellipse.

inputs
   double   x,y,z - x,y,z radius
*/
void CRender::MeshEllipsoid (double x, double y, double z)
{
   // automatically scale, since this solves some normal-generation problems
   BOOL  m_fScale;
   m_fScale = ((x != y) || (x != z));
   if (m_fScale) {
      mpush();
      Scale (1.0, y / x, z / x);
   }

   long facets = 8;

   // may already have done so
   // should see if bounding box will rule out immediately
   BOOL  fHidden;
   fHidden = FALSE;
   if (BoundingBox (x, x, x, -x, -x, -x, (DWORD*) &facets)) {
      fHidden = TRUE;
      // because might be adding bump map or vectors later, can't do any more
   }

   long  i,j;
   double   *s, *p, *p2;

   long  width, height;
   width = facets * 2;
   height = (width / 2) + 1;

   // adjust color colormap or bumpmap size
   MapCheck ((DWORD) facets, (DWORD *) &width, (DWORD *) &height);

   s = (double*) ESCMALLOC (width * height * 4 * sizeof(double));
   if (!s)
      return;
   DWORD dwAcross;   // for macro
   dwAcross = (DWORD) width;

   // if there's symmedtry can optimize

   if (height & 0x01) {
      // odd number of values high. This is good
      for (i = 0; i < width; i++) for (j = 0; j <= (height/2); j++) {
         p = s + MESH(i, j, 0);
         double   fRadK, fRadI;
         fRadK = PI/2 * (1.0-EPSILON) - j / (double) (height-1) * PI * (1.0-EPSILON);
         fRadI = i / (double) width * 2.0 * PI;
         p[0] = cos(fRadI) * cos(fRadK) * x;
         p[1] = sin(fRadK) * x;
         p[2] = -sin(fRadI) * cos(fRadK) * x;
      }

      for (i = 0; i < width; i++) for (j = 0; j < (height/2); j++) {
         p = s + MESH(i, j, 0);
         p2 = s + MESH(i, height - 1 - j, 0);
         p2[0] = p[0];
         p2[1] = -p[1];
         p2[2] = p[2];

      }
   }
   else {
      // even number. do the entire loop
      for (i = 0; i < width; i++) for (j = 0; j < height; j++) {
         p = s + MESH(i, j, 0);
         double   fRadK, fRadI;
         fRadK = PI/2 * (1.0-EPSILON) - j / (double) (height-1) * PI * (1.0-EPSILON);
         fRadI = i / (double) width * 2.0 * PI;
         p[0] = cos(fRadI) * cos(fRadK) * x;
         p[1] = sin(fRadK) * x;
         p[2] = -sin(fRadI) * cos(fRadK) * x;
      }
   }

   MeshFromPoints (1, width, height, s, s, NULL, NULL, FALSE);
   // don't bother generate north and east because they're automaticaly calculated easily enough

   // note that already checked
   m_fMeshCheckedBoundary = TRUE;
   m_fMeshShouldHide = fHidden;

   // fre memory
   ESCFREE (s);

   if (m_fScale)
      mpop ();
}

/*******************************************************************
MeshPlane - Draws a flat plane. Although you can use bumpmaps to
   add dimensions. The plane's lower-left corner is 0,0,0, and upper
   right is x,y,0.

inputs
   double   x,y - x,y,z radius
*/
void CRender::MeshPlane (double x, double y)
{
   long facets = 4;

   // may already have done so
   // should see if bounding box will rule out immediately
   BOOL  fHidden;
   fHidden = FALSE;
   if (BoundingBox (0, 0, 0, x, y, 0, (DWORD*) &facets)) {
      fHidden = TRUE;
      // because might be adding bump map or vectors later, can't do any more
   }

   long  i,j;
   double   *s, *p;

   long  width, height;
   width = facets;
   height = width;

   // adjust color colormap or bumpmap size
   MapCheck ((DWORD) facets, (DWORD *) &width, (DWORD *) &height);

   s = (double*) ESCMALLOC (width * height * 4 * sizeof(double));
   if (!s)
      return;
   DWORD dwAcross;   // for macro
   dwAcross = (DWORD) width;

   // even number. do the entire loop
   for (i = 0; i < width; i++) for (j = 0; j < height; j++) {
      p = s + MESH(i, j, 0);
      p[0] = (double) i / (width - 1) * x;
      p[1] = (double) (height - j - 1) / (height - 1) * y;
      p[2] = 0;
   }

   MeshFromPoints (0, width, height, s, NULL, NULL, NULL, FALSE);
   // don't bother generate normal, north and east because they're automaticaly calculated easily enough

   // note that already checked
   m_fMeshCheckedBoundary = TRUE;
   m_fMeshShouldHide = fHidden;

   // fre memory
   ESCFREE (s);
}


/*******************************************************************
MeshRotation - Draws an rotation of a line (or bezier curve) around the Y-axis.

inputs
   DWORD    dwNum - number of points
   double   *pafPoints - Array of points, specified by dwNum. As a general
               rule, all X values should be >= 0. This code will automatically
               adjust any X values = 0 to EPSLION so that normal detection works
               z values are ignored. The points should also be from top-down
               so that culling dowesn't eliminate them.
   BOOL     fBezier - if TRUE, apply bezier curve to pafPoints. To do a bezier
               dwNum must be a multiple of 3 + 1, ie 4, 7, 10, 13, etc.
*/
void CRender::MeshRotation (DWORD dwNum, double *pafPoints, BOOL fBezier)
{
   long facets = 8;
   long  i,j;

   // may already have done so
   // should see if bounding box will rule out immediately
   BOOL  fHidden;
   double   fMaxY, fMinY, fMaxX;
   fMaxY = fMinY = pafPoints[0 * 4 + 1];
   if (pafPoints[0*4 + 0] == 0)
      pafPoints[0*4 + 0] = EPSILON;
   fMaxX = pafPoints[0*4 + 0];
   for (i = 1; i < (int) dwNum; i++) {
      fMaxY = max(fMaxY, pafPoints[i*4 + 1]);
      fMinY = min(fMinY, pafPoints[i*4 + 1]);
      if (pafPoints[i*4 + 0] == 0)
         pafPoints[i*4 + 0] = EPSILON;
      fMaxX = max(fMaxX, pafPoints[i*4 + 0]);
   }

   fHidden = FALSE;
   if (BoundingBox (fMaxX, fMaxY, fMaxX, -fMaxX, fMinY, -fMaxX, (DWORD*) &facets)) {
      fHidden = TRUE;
      // because might be adding bump map or vectors later, can't do any more
   }

   double   *s, *p, *pafBez;

   long  width, height;
   width = facets * 2;
   height = dwNum;
   pafBez = NULL;

   // adjust color colormap or bumpmap size
   MapCheck ((DWORD) facets, (DWORD *) &width, (DWORD *) &height);

   // if want a bezier curve
   if (fBezier) {
      // keep on increasing the size we want until it fits
      DWORD dwWant;
      for (dwWant = dwNum; dwWant < max((DWORD) facets/2, (DWORD) height); dwWant = (dwWant - 1)*2+1);
      pafBez = BezierArray (dwNum, pafPoints, FALSE, dwWant);

      // if we actually get a match then use this
      if (pafBez) {
         height = dwNum = dwWant;
         pafPoints = pafBez;
      }
   }
   
   s = (double*) ESCMALLOC (width * height * 4 * sizeof(double));
   if (!s)
      return;
   DWORD dwAcross;   // for macro
   dwAcross = (DWORD) width;

   // if there's symmedtry can optimize

   // even number. do the entire loop
   for (i = 0; i < width; i++) for (j = 0; j < height; j++) {
      // given location in height, figure out which points need to map to
      double   fIndex, fAlpha, fInvAlpha;
      int      x1, x2;
      fIndex = (double) j / (double) (height-1) * (double) (dwNum-1);
      x1 = (int) floor(fIndex);
      x2 = min((int)dwNum-1, x1 + 1);
      fAlpha = fIndex - x1;
      fInvAlpha = 1.0 - fAlpha;

      // interpolate x and y locations
      double   fx, fy;
      fx = pafPoints[x1*4 + 0] * fInvAlpha + pafPoints[x2*4 + 0] * fAlpha;
      fy = pafPoints[x1*4 + 1] * fInvAlpha + pafPoints[x2*4 + 1] * fAlpha;

      p = s + MESH(i, j, 0);
      double   fRad;
      fRad = i / (double) width * 2.0 * PI;
      p[0] = sin(fRad) * fx;
      p[1] = fy;
      p[2] = cos(fRad) * fx;
   }

   MeshFromPoints (1, width, height, s, NULL, NULL, NULL, FALSE);
   // don't bother generate north and east because they're automaticaly calculated easily enough

   // note that already checked
   m_fMeshCheckedBoundary = TRUE;
   m_fMeshShouldHide = fHidden;

   // fre memory
   ESCFREE (s);
   if (pafBez)
      ESCFREE (pafBez);

}

/*******************************************************************
MeshFunnel - Draws a Funnel, standing up and centered on the Y axis

inputs
   double   fHeight - height. It's base is at 0.
   double   fWidthBase - width at the base. Should be >= 0
   double   fWidthTop - width at the top. SHould be >= 0
*/
void CRender::MeshFunnel (double fHeight, double fWidthBase, double fWidthTop)
{
   pnt   p[2];
   p[0][0] = fWidthTop;
   p[0][1] = fHeight;
   p[1][0] = fWidthBase;
   p[1][1] = 0;

   MeshRotation (2, (double*) p);
}

/********************************************************************
Bezier - does a bezier sub-division.

inputs
   double      pafPointsIn - input points. [4][4]
   double      pafPointsOut - filled with output [7][4]
   DWORD       dwDepth - depth. if 0, only 3 points filld in, 1 then 7, 2 then 15, etc.
returns
   DWORD - total number of points filled in
*/
DWORD CRender::Bezier (double *pafPointsIn, double *pafPointsOut, DWORD dwDepth)
{
   pnt   pMid;
   pnt   aNew[4];
   DWORD i;
   DWORD dwTotal = 0;

   if (!dwDepth) {
      memcpy (pafPointsOut, pafPointsIn, 3 * 4 * sizeof(double));
      return 3;
   }

   // find mid-point
   for (i = 0; i < 4; i++)
      pMid[i] = (pafPointsIn[1*4 + i] + pafPointsIn[2*4 + i]) / 2;

   // start filling in
   CopyPnt (pafPointsIn + (0*4), aNew[0]);
   for (i = 0; i < 4; i++)
      aNew[1][i] = (pafPointsIn[0*4 + i] + pafPointsIn[1*4 + i]) / 2;
   for (i = 0; i < 4; i++)
      aNew[2][i] = (pafPointsIn[1*4 + i] + pMid[i]) / 2;
   CopyPnt (pMid, aNew[3]);
   dwTotal += Bezier ((double*) aNew, pafPointsOut, dwDepth-1);

   // next batch
   CopyPnt (pMid, aNew[0]);
   for (i = 0; i < 4; i++)
      aNew[1][i] = (pMid[i] + pafPointsIn[2*4 + i]) / 2;
   for (i = 0; i < 4; i++)
      aNew[2][i] = (pafPointsIn[2*4 + i] + pafPointsIn[3*4 + i]) / 2;
   CopyPnt (pafPointsIn + (3*4), aNew[3]);
   dwTotal += Bezier ((double*) aNew, pafPointsOut + (dwTotal * 4), dwDepth-1);

   return dwTotal;

}

/********************************************************************
BezierArray - Given a set of points, this generates a bezier curve (basically
   a larger set of points.)

inputs
   DWORD    dwNumIn - number of incoming points. if fLoop is FALSE,
               must be 4, 7, 10, 13, etc. If fLoop is TRUE, must be a multiple of 3.
   double*  pafPointsIn - incoming points. [dwNumIn][4]
   BOOL     fLoop - if TRUE the points represent a continuous loop, else
               a line segment
   DWORD    dwNumOut - number of points that want out. If fLoop is FALSE,
               this must be (n x (dwNumIn-1)) + 1, where n is an integer.
               Otherwise, NULL will be returned from the functon.
               If fLoop is TRUE, this must be (n x dwNumIn).
returns
   double*  - new points, dwNumOut. THis must be freed with free(). It can
            be NULL if the wrong number of bezier points are given
*/
double * CRender::BezierArray (DWORD dwNumIn, double *pafPointsIn, BOOL fLoop, DWORD dwNumOut)
{
   double   *pafRet = NULL;

   // step one, if fLoop is TRUE, then call back into self so floop is not true
   if (fLoop) {
      // basically allocate temporary memory and add the starting bezier onto the end
      double   *pafTemp;
      pafTemp = (double*) ESCMALLOC ((dwNumIn+1) * 4 * sizeof(double));
      if (!pafTemp) return NULL;
      memcpy (pafTemp, pafPointsIn, dwNumIn * 4 * sizeof(double));
      CopyPnt (pafPointsIn, pafTemp + (4 * dwNumIn));

      pafRet = BezierArray (dwNumIn+1, pafTemp, FALSE, dwNumOut+1);
      ESCFREE (pafTemp);

      return pafRet;
   }


   // make sure right number of points
   DWORD n, m;
   // make sure dwnumin right
   n = dwNumIn ? (dwNumIn - 1) / 3 : 0;
   if (!n)
      return NULL;
   if ((n*3)+1 != dwNumIn)
      return NULL;

   // make sure out it good
   m = dwNumOut ? (dwNumOut - 1) / 3 : 0;
   if (!m)
      return NULL;
   if ( (m*3 + 1) != dwNumOut)
      return NULL;

   // make sure out m, is a multiple of n
   DWORD s;
   s = m / n;
   if (s * n != m)
      return NULL;
   if (s & (s-1)) // should be a power of 2
      return NULL;

   // divide and conquer
   pafRet = (double*) ESCMALLOC(dwNumOut * 4 * sizeof(double));
   if (!pafRet)
      return NULL;
   DWORD i, dwTotal;

   // loop through this and split it into smaller chunks
   DWORD dwDepth;
   for (dwDepth = 0; s > 1; s /= 2, dwDepth++);
   for (i = dwTotal = 0; i < n; i++) {
      dwTotal += Bezier (pafPointsIn + (i * 3) * 4, pafRet + (dwTotal * 4), dwDepth);
   }

   // finish off with the last point
#ifdef _DEBUG
   if ((i*3) != dwNumIn-1)
      i = 1;
   if (dwTotal != dwNumOut-1)
      i = 1;
#endif
   CopyPnt (pafPointsIn + (i*3)*4, pafRet + (dwTotal * 4));

   // done
   return pafRet;
}



/********************************************************************
TextDraw - Given a font, justification information, x,y pixels, rotation,
   and z, this draws the text.

inputs
   char     psz - string
   HFONT    hFont - font to use
   int      iVert - +1 for above point, 0 for centered on point, -1 for below
   int      iHort - +1 for to the right, 0 for centered, -1 for to the left
   int      iRot - 0 for to the right, 1 for down
   int      iX, iY - x/y pixels
   double   z - z depth, so text gets clipped if its behind
   RECT     *prSize - If not null, this is filled with the pixels used for
               the text. As a result, no text will acutally be displayed!
               Note that it's the size, and nothing about the position. It does
               account for a vertical vs. horz though
*/
void CRender::TextDraw (const char *psz, HFONT hFont, int iVert, int iHorz, int iRot,
                        int iX, int iY, double z, RECT *prSize)
{
   HDC hDC;
   HFONT hFontOld;
   HBITMAP  hBitOld;
   HBITMAP  hBit;

   hDC = CreateCompatibleDC (m_hDC);
   hFontOld = (HFONT) SelectObject (hDC, hFont);
   SetTextColor (hDC, RGB(0xff, 0xff, 0xff));
   SetBkMode (hDC, TRANSPARENT);

   // calculate the size
   RECT  r;
   r.left = r.top = 0;
   r.right = 2000;
   r.bottom = 2000;
   // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
   DrawText (hDC, psz, -1, &r, DT_NOPREFIX | DT_CALCRECT | DT_LEFT | DT_TOP);

   if (prSize) {
      // we know how large, just fill in
      if (iRot == 0)
         CopyRect (prSize, &r);
      else {
         prSize->left = prSize->top = 0;
         prSize->right = r.bottom;
         prSize->bottom = r.right;
      }

      // get rid of stuff
      SelectObject (hDC, hFontOld);
      DeleteDC (hDC);
      return;
   }

   // create a bitmap, and make it black
   hBit = CreateCompatibleBitmap (m_hDC, r.right, r.bottom);
   hBitOld = (HBITMAP) SelectObject (hDC, hBit);
   FillRect (hDC, &r, (HBRUSH) GetStockObject (BLACK_BRUSH));

   // draw the text on top
   // BUGFIX - Put in NOPREFIX so ampersand would be properly drawn/calculated
   DrawText (hDC, psz, -1, &r, DT_NOPREFIX | DT_LEFT | DT_TOP);

   // draw it out
   int   iOffX, iOffY;
   iOffX = iX;
   iOffY = iY;
   int   x,y;
   DWORD dwRGB;
   pnt   pc;
   CopyPnt (m_DefColor, pc);
   ApplyFog (pc, z);
   ApplyColorMode (pc);
   dwRGB = RGB(pc[0], pc[1], pc[2]);

   if (iRot == 0) {
      // centering
      if (iVert < 0)
         iOffY -= r.bottom;
      else if (iVert == 0)
         iOffY -= r.bottom / 2;
      if (iHorz < 0)
         iOffX -= r.right;
      else if (iHorz == 0)
         iOffX -= r.right / 2;

      // normal orientation
      // make sure not to draw off edge
      y = 0;
      if (y + iOffY < 0)
         y = -iOffY;
      if (r.bottom + iOffY > m_rDraw.bottom-1)
         r.bottom = m_rDraw.bottom -1 - iOffY;
      if (r.right + iOffX > m_rDraw.right-1)
         r.right = m_rDraw.right - 1 - iOffX;
      for (; y < r.bottom; y++) {
         x = 0;
         if (x + iOffX < 0)
            x = -iOffX;
         for (; x < r.right; x++) {
            if (GetPixel (hDC, x, y))
               DrawPixel (x + iOffX, y + iOffY, (float) z, dwRGB);
         }
      }
   }
   else {
      int   iInv;
      iInv = r.bottom - 1;

      // up/down
      // centering
      if (iVert < 0)
         iOffX -= r.bottom;
      else if (iVert == 0)
         iOffX -= r.bottom / 2;
      if (iHorz < 0)
         iOffY -= r.right;
      else if (iHorz == 0)
         iOffY -= r.right / 2;

      // normal orientation
      // make sure not to draw off edge
      y = 0;
      if (y + iOffY < 0)
         y = -iOffY;
      if (r.right + iOffY > m_rDraw.bottom-1)
         r.right = m_rDraw.bottom -1 - iOffY;
      if (r.bottom + iOffX > m_rDraw.right-1)
         r.bottom = m_rDraw.right - 1 - iOffX;
      for (; y < r.right; y++) {
         x = 0;
         if (x + iOffX < 0)
            x = -iOffX;
         for (; x < r.bottom; x++) {
            if (GetPixel (hDC, y, iInv - x))
               DrawPixel (x + iOffX, y + iOffY, (float) z, dwRGB);
         }
      }
   }

   SelectObject (hDC, hBitOld);
   SelectObject (hDC, hFontOld);
   DeleteObject (hBit);
   DeleteDC (hDC);
}


/********************************************************************
TextFontSet - Sets the current font.

inputs
   LOGFONT     *plf - logical font to use
   double      fHeight - height. Units depend uon fPixels
   BOOL        fPixels - if TRUE, units are in pixels. Else, they're in rendering
                  units, and perspective has an effect on the size
*/
void CRender::TextFontSet (LOGFONT *plf, double fHeight, BOOL fPixels)
{
   if (m_hFont)
      MyDeleteObject (m_hFont);
   // BUGFIX - Set hfont to null
   m_hFont = NULL;

   memcpy (&m_lf, plf, sizeof(m_lf));
   m_fFontHeight = fHeight;
   m_fFontPixels = fPixels;
   m_dwFontLastHeightPixels = (DWORD) -1;
}


/********************************************************************
TextFontGenerate - Make sure there's a m_hFont with the right size

inputs
   double      z - z (should be negative). This is used if !m_fFontPixels
*/
void CRender::TextFontGenerate (double z)
{
   // how big
   DWORD dwHeight;
   if (z >= 0)
      z -= EPSILON;
   if (m_fFontPixels)
      dwHeight = (DWORD) m_fFontHeight;
   else {
      double   fHeight;
      fHeight = m_fFontHeight / (-z) * m_rDraw.right * 3 / 2;
      dwHeight = (DWORD) fHeight;
   }

   if (m_hFont && (dwHeight == m_dwFontLastHeightPixels))
      return;  // no change

   // else, create new font
   HFONT hNew;
   m_lf.lfHeight = -(int)dwHeight;
   // m_lf.lfHeight = -MulDiv(dwHeight, EscGetDeviceCaps(m_hDC, LOGPIXELSY), 72);
   hNew = MyCreateFontIndirect (&m_lf);
   if (hNew) {
      if (m_hFont)
         MyDeleteObject (m_hFont);
      m_hFont = hNew;
   }
}


/********************************************************************
Text - Draws text.

inputs
   char     *psz - text to draw
   pnt      pLeft - left point
   pnt      pRight - right point. This basically acts as a direcitonal vector saying
               which way the text should be drawn.(If the right point is signficiantly
                  lower or higher than the left point, after it's been
                  all rotated, then the text will be drawn vertical.
   int      iHorz - Horizontal alignment. -1 => left edge on left point. 0 => center on left point.
                  1 => right edge on left point
   int      iVert - Vertical alignment. -1 => draw above point,
                  0 => center. 1 => draw below point
*/
void CRender::Text (const char *psz, pnt pLeft, pnt pRight,
                        int iHorz, int iVert)
{
   double   xl,xr, yl, yr;

   // convert left & right to screen coords
   pnt   psl, psr;
   mtransform (pLeft, psl);
   psl[1] *= aspect;
   mtransform (pRight, psr);
   psr[1] *= aspect;
   if ((psl[2] >= 0) || (psr[2] >= 0) || (psl[3] == 0) || (psr[3] == 0))
      return;  // clip
   DevIndepToDevDep (psl[0] / psl[3], psl[1] / psl[3], &xl, &yl);
   DevIndepToDevDep (psr[0] / psr[3], psr[1] / psr[3], &xr, &yr);

   // figure out orientation
   int   ix, iy;
   int   iHorzJust;
   double   z;
   ix = (int) xl;
   iy = (int) yl;
   z = psl[2];

   // figure out when to draw vertical
   int iRot;
   if (fabs(xl - xr) > fabs(yl-yr))
      iRot = 0;
   else
      iRot = 1;

   if (iRot == 0) {  // horizontal text
      if (iHorz < 0)
         iHorzJust = (xl < xr) ? 1 : -1;
      else if (iHorz > 0)
         iHorzJust = (xr < xl) ? 1 : -1;
      else   // iHorz == 0
         iHorzJust = 0;
   }
   else { // vertical text
      if (iHorz < 0)
         iHorzJust = (yl < yr) ? 1 : -1;
      else if (iHorz > 0)
         iHorzJust = (yr < yl) ? 1 : -1;
      else   // iHorz == 0
         iHorzJust = 0;
   }

   // draw it
   TextFontGenerate (z);
   TextDraw (psz, m_hFont, iVert, iHorzJust, iRot, ix, iy, z);
}



//  - eventually render clouds.

//  - options for types of axis to draw

//  - eventually volume rendering, and some other objects. see tecplot

//  - option so that when draw a rendered surface, also draw a polygon border
// around it so user can see where edges of data points are

//  - streamline - like line with arrow, but draws arrow at every end

//  - maybe enable volume rendering as coloration (or transparency) based
// upon location within a volume database. When look up color, index into volume

//  - coloration key object that can be displayed - set of colors -> values
// ie: light red= 52 m/s, dark red = 103 m/s

//  - draw sets labels at sets of points. Label on top of box so it's readabble
// label is visible so long as its point is. don't get obscured by other stuff
// IE - at each vertex have a label


// BUGBUG - 2.0 - doesn't seem to render 3d objects onto 2nd monitor when have
// dual-monitor support

// BUGBUG - 2.0 - Doesn't always print 3d objects - seems like starts having
// problems on the second page

// BUGBUG - On Rita's dual-monitor display, if drawin CircumReality transcript window on
// second monitor, wouldn't render boxes properly. Can't repro on my computer. And
// no obvious reason why it's happening.