from numerical recipies in c

void FFTCalc (float data[], DWORD n, int iSign);



/**********************************************************************************88
four1 - does a fourier transform.

inputs
   float    data[] - array of 2 *nn. Note it's 1..2nn (starting at 1!!!)
   DWORD    nn - count
   int      iSign - if Positive, does FFT, negative inverse FFT
*/
#define  SWAP(a,b) tempr=(a);(a)=(b);(b)=tempr

void four1 (float data[], DWORD nn, int iSign)
{
   DWORD n, mmax, m, j, istep, i;
   float wtemp, wr, wpr, wpi, wi, theta;
   float tempr, tempi;

   n = nn<<1;
   j = 1;
   for (i = 1; i < n; i+= 2) {
      if (j > i) {
         SWAP (data[j], data[i]);
         SWAP (data[j+1], data[i+1]);
      }
      m = n >> 1;
      while (m >= 2 && j > m) {
         j -= m;
         m >>= 1;
      }
      j += m;
   }


   mmax = 2;
   while (n > mmax) {
      istep = mmax << 1;
      theta = iSign * (6.28318530717959/mmax);
      wtemp = sin (0.5 * theta);
      wpr = -2.0*wtemp*wtemp;
      wpi = sin(theta);
      wr = 1.0;
      wi = 0.0;
      for (m = 1; m < mmax; m += 2) {
         for (i = m; i <= n; i+= istep) {
            j = i+mmax;
            tempr = wr*data[j] - wi*data[j+1];
            tempi = wr*data[j+1] + wi*data[j];
            data[j] = data[i] - tempr;
            data[j+1] = data[i+1] - tempi;
            data[i] += tempr;
            data[i+1] += tempi;
         }
         wr = (wtemp = wr) * wpr - wi*wpi + wr;
         wi = wi*wpr + wtemp*wpi + wi;
      }
      mmax = istep;
   }

}

/**********************************************************************************88
FFTCalc - does a fourier transform.

inputs
   float    data[] - array of 1..n (NOTE that is starts at 1!!!) I don't quite
               get the return form yet
   DWORD    nn - count
   int      iSign - if Positive, does FFT, negative inverse FFT
*/
void FFTCalc (float data[], DWORD n, int iSign)
{
   DWORD i, i1, i2, i3, i4, np3;
   float c1=0.5, c2, h1r, h1i, h2r, h2i;
   float wr, wi, wpr, wpi, wtemp,theta;

   theta = 3.141592653589793/(float) (n>>1);

   if (iSign == 1) {
      c2 = -0.5;
      four1(data, n>>1, 1);
   }
   else {
      c2 = 0.5;
      theta = -theta;
   }

   wtemp = sin(0.5*theta);
   wpr = -2.0 * wtemp * wtemp;
   wpi = sin(theta);
   wr = 1.0 + wpr;
   wi = wpi;
   np3 = n+ 3;
   for (i = 2; i <= (n>>2); i++) {
      i4 = 1 + (i3 = np3 - (i2 = 1 + (i1 = i + i - 1)));
      h1r = c1 * (data[i1]+data[i3]);
      h1i = c1 * (data[i2]-data[i4]);
      h2r = -c2 * (data[i2] + data[i4]);
      h2i = c2 * (data[i1] - data[i3]);
      data[i1] = h1r + wr*h2r - wi*h2i;
      data[i2] = h1i + wr*h2i + wi*h2r;
      data[i3] = h1r - wr*h2r + wi*h2i;
      data[i4] = -h1i + wr*h2i + wi*h2r;
      wr = (wtemp = wr) * wpr - wi *wpi+wr;
      wi = wi * wpr + wtemp * wpi + wi;
   }

   if (iSign == 1) {
      data[1] = (h1r = data[1]) + data[2];
      data[2] = h1r - data[2];
   }
   else {
      data[1] = c1 * ((h1r = data[1]) + data[2]);
      data[2] = c1 * (h1r - data[2]);
      four1 (data, n>>1, -1);
   }

}
