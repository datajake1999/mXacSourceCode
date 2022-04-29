/*******************************************************************
CMatrix::Invert4 - Inverts the whole 4x4 matrix

PROBLEM: While these seems to be working, it's not possible to invert
a matrix with perspective because a[icol][icol] == 0 in the W case.
May have to think approach to finding where point that looking at interestcts
what object.

inputs
   CMatrix     *pDest - destination
*/
#define SWAP(a,b) {temp=(a);(a)=(b);(b)=temp;}

void CMatrix::Invert4Old (CMatrix *pDest)
{
   int indxc[4], indxr[4], ipiv[4];
   int i, icol = 0, irow = 0, j, k, l, ll;   // BUGFIX - need to set to 0
   fp a[4][4], b[4][4];
   int n, m;
   n = 4;
   m = 4;
   ((PCMatrix) a)->Copy (this);
   ((PCMatrix) b)->Identity();
   
   fp big, dum, pivinv, temp;

   memset (ipiv, 0, sizeof(ipiv));
   for (i = 0; i < n; i++) {
      big = 0;
      for (j = 0; j < n; j++)
         if (ipiv[j] != 1)
            for (k = 0; k < n; k++) {
               if (ipiv[k] == 0) {
                  if (fabs(a[j][k]) >= big) {
                     big = fabs(a[j][k]);
                     irow = j;
                     icol = k;
                  }
               }
               else if (ipiv[k] > 1)
                  return;  // error
            }
      ++(ipiv[icol]);

      if (irow != icol) {
         for (l = 0; l < n; l++) SWAP(a[irow][l],a[icol][l]);
         for (l = 0; l < m; l++) SWAP(b[irow][l],b[icol][l]);
      }

      indxr[i] = irow;
      indxc[i] = icol;
      if (a[icol][icol] == 0)
         return;  // error
      pivinv = 1.0 / a[icol][icol];

      a[icol][icol] = 1.0;
      for (l = 0; l < n; l++) a[icol][l] *= pivinv;
      for (l = 0; l < m; l++) b[icol][l] *= pivinv;

      for (ll = 0; ll < n; ll++)
         if (ll != icol) {
            dum = a[ll][icol];
            a[ll][icol] = 0;
            for (l = 0; l < n; l++) a[ll][l] -= a[icol][l] * dum;
            for (l = 0; l < m; l++) b[ll][l] -= b[icol][l] * dum;
         }
   }

   for (l = n-1; l >= 0; l--) {
      if (indxr[l] != indxc[l])
         for (k = 0; k < n; k++)
            SWAP(a[k][indxr[l]],a[k][indxc[l]]);
   }


   // copy to dest
   pDest->Copy ((PCMatrix) a);

}
