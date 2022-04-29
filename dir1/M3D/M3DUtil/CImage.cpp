/************************************************************************
CImage - Handles the image buffer for rendering, and some of the drawing
routines.

begun 1/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#define  GAMMADEF    2.0      // BUGFIX - Make 2.0 from 2.2 since my monitor (which is a standard
                              // flat panel display) has a gamma around 2.0)
#define  COLORFRAC   14       // # of bits shift color left when doing blend from one side to another


/* globals */
static BOOL gfImageDither = FALSE;     // set tp true if dithering set up
int      gaImageDither[IMAGEDITHERBINS][IMAGETRANSSIZE][IMAGETRANSSIZE];  // for dithering
static BOOL gfImageGamma = FALSE;      // set to true if gamma and ungamma set up
BYTE     gaImageUnGamma[0x10000]; // un-gamma value LUT
WORD     gaImageGamma[0x100];      // gamma values


/************************************************************************
Gamma and UnGamma functions
*/

WORD Gamma (BYTE bColor)   // gamma correct
{
   return gaImageGamma[bColor];
};
void Gamma (COLORREF cr, WORD *pRGB)  // gamma correct
{
   pRGB[0] = Gamma(GetRValue(cr));
   pRGB[1] = Gamma(GetGValue(cr));
   pRGB[2] = Gamma(GetBValue(cr));
};
BYTE UnGamma (WORD wColor) // un-gamma correct
{
   return gaImageUnGamma[wColor];
};
COLORREF UnGamma (const WORD *pRGB)     // un-gamma correect
{
   return RGB (UnGamma(pRGB[0]), UnGamma(pRGB[1]), UnGamma(pRGB[2]));
};

/************************************************************************
GammaFloat256 - Converts a floating point non-linear color, where
0xff is white, to a linear color, where 0xffff is
white.
*/

DLLEXPORT float GammaFloat256 (float fColor)
{
   return pow ((fp)(fColor / (float)0xff), (fp)GAMMADEF) * (float)0xffff;
}

DLLEXPORT void GammaFloat256 (float *pafColor)
{
   DWORD i;
   for (i = 0; i < 3; i++, pafColor)
      pafColor[i] = GammaFloat256 (pafColor[i]);
}

/************************************************************************
UnGammaFloat256 - Converts a floating point linear color, where 0xffff
is white, to a non-linear color, where 0xff is white.
*/
DLLEXPORT float UnGammaFloat256 (float fColor)
{
   return pow ((fp)(fColor / (float)0xffff), (fp)(1.0 / GAMMADEF)) * (float)0xff;
}

DLLEXPORT void UnGammaFloat256 (float *pafColor)
{
   DWORD i;
   for (i = 0; i < 3; i++, pafColor)
      pafColor[i] = UnGammaFloat256 (pafColor[i]);
}

/************************************************************************
GammaInit - initialize gamma values if not already initialized
*/
void GammaInit (void)
{
   // generate gamma tables
   if (gfImageGamma)
      return;

   gfImageGamma = TRUE;

   // create gamma values
   DWORD i;
   gaImageGamma[0] = 0;
   for (i = 1; i < 256; i++)
      gaImageGamma[i] = (WORD) (pow(i / 255.0, GAMMADEF) * 65534 + 1);

   // and in reverse
   BYTE g;
   for (i = 0, g=0; i < 0x10000; i++) {
      if ((g < 255) && (i >= gaImageGamma[g+1])) 
         g++;

      gaImageUnGamma[i] = g;
   }
}

/************************************************************************
Constructor and destructor
*/
CImage::CImage (void)
{
   m_dwWidth = m_dwHeight = 0;
   m_hBitCache = NULL;
   m_f360 = FALSE;

   m_fCritSecInitialized = FALSE;

   GammaInit();

   // generate dither tables
   if (!gfImageDither) {
      gfImageDither = TRUE;

      DWORD dwTrans, dwX, dwY;
      for (dwTrans = 0; dwTrans < IMAGEDITHERBINS; dwTrans++) {
         BOOL  fBit = (dwTrans < 4) ? TRUE : FALSE;

         // wipe out
         memset (&gaImageDither[dwTrans][0][0], !fBit, sizeof(gaImageDither[dwTrans]));

         switch (dwTrans) {
         case 0:
         case 7:
            // only do 2 bits since 4x4=16, divided by 8 = 2
            gaImageDither[dwTrans][0][0] = fBit;
            gaImageDither[dwTrans][2][2] = fBit;
            break;
         case 1:
         case 6:
            // do 4 bits
            for (dwY = 0; dwY < IMAGETRANSSIZE; dwY+=2) for (dwX = 0; dwX < IMAGETRANSSIZE; dwX+=2)
               gaImageDither[dwTrans][dwY][dwX] = fBit;
            break;
         case 2:
         case 5:
            // do 6 bits
            for (dwY = 0; dwY < IMAGETRANSSIZE; dwY++) for (dwX = 0; dwX < IMAGETRANSSIZE; dwX++)
               if (!((dwY + dwX) % 3)) // BUGFIX - flip this do doesnt dither as much
                  gaImageDither[dwTrans][dwY][dwX] = fBit;
            break;
         case 3:  // BUGFIX - was left out
         case 4:
            // every other bit
            for (dwY = 0; dwY < IMAGETRANSSIZE; dwY++) for (dwX = 0; dwX < IMAGETRANSSIZE; dwX++)
               if ((dwY + dwX) % 2)
                  gaImageDither[dwTrans][dwY][dwX] = fBit;
            break;
         }
      } // dwtrans
   } // imagedither

#if 0 // old version that doesn't work too well
   // generate dither tables
   if (!gfImageDither) {
      gfImageDither = TRUE;

      DWORD dwTrans, dwX, dwY;
      for (dwTrans = 0; dwTrans < IMAGEDITHERBINS; dwTrans++) {
         // wipe it out with one value which is the transparency
         int iVal;
         iVal = (int) (dwTrans+1) * 0x10000 / (IMAGEDITHERBINS+1);
         for (dwY = 0; dwY < IMAGETRANSSIZE; dwY++) for (dwX = 0; dwX < IMAGETRANSSIZE; dwX++)
            gaImageDither[dwTrans][dwY][dwX] = iVal;

         // do error diffusion
         for (dwY = 0; dwY < IMAGETRANSSIZE; dwY++) for (dwX = 0; dwX < IMAGETRANSSIZE; dwX++) {
            iVal = gaImageDither[dwTrans][dwY][dwX];

            // set the gaImageDither pixel. If iVal >= 0x10000 then set to 0x10000, else 0
            if (iVal >= 0x10000) {
               gaImageDither[dwTrans][dwY][dwX] = 0x10000;
               iVal -= 0x10000;
            }
            else {
               gaImageDither[dwTrans][dwY][dwX] = 0;
               // leave iVal
            }

            // move the error on, to pixel to right, below, and right-below
            iVal /= 2;
            if (dwX+1 < IMAGETRANSSIZE) {
               gaImageDither[dwTrans][dwY][dwX+1] += iVal;

               if (dwY + 1 < IMAGETRANSSIZE)
                  gaImageDither[dwTrans][dwY+1][dwX+1] += iVal*0;
            }
            if (dwY + 1 < IMAGETRANSSIZE)
               gaImageDither[dwTrans][dwY+1][dwX] += iVal;

         } // xy loop
      } // dwtrans
   } // imagedither
#endif // 0
}

CImage::~CImage (void)
{
   PaintCacheClear();

   // free up critical sections
   DWORD i;
   if (m_fCritSecInitialized)
      for (i = 0; i < IMAGECRITSEC; i++)
         DeleteCriticalSection (&m_aCritSec[i]);
}



/*************************************************************************
CImage::CritSectionInitialize() - Call this to make sure the scanline
critical sections are initialized. This makes for safer rendering of
lines and filled polys on the image, ensuring that the threads don't
overwrite each others scanlines.

If this has already been called for this object, then it does nothning
*/
void CImage::CritSectionInitialize (void)
{
   if (m_fCritSecInitialized)
      return;

   DWORD i;
   for (i = 0; i < IMAGECRITSEC; i++)
      InitializeCriticalSection (&m_aCritSec[i]);

   m_fCritSecInitialized = TRUE;
}



/*************************************************************************
CImage::Init - Initializes the data structures for the image but does NOT
blank out the pixels or Z buffer. You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
returns
   BOOL - TRUE if successful
*/
BOOL CImage::Init (DWORD dwX, DWORD dwY)
{
   // clear the old cache
   PaintCacheClear();

   if (!m_memBuffer.Required(dwX * dwY * sizeof(IMAGEPIXEL)))
      return FALSE;
   DWORD i;
   for (i = 0; i < MAXRAYTHREAD; i++)
      if (!m_amemZTemp[i].Required (dwX * sizeof(fp)))
         return FALSE;
   m_dwWidth = dwX;
   m_dwHeight = dwY;
   return TRUE;
}

/*************************************************************************
CImage::Init - Initializes the data structures for the image. It also
CLEARS the image color to cColor and sets the ZBuffer to INFINITY.
You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
   COLORREF    cColor - Color to clear to
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CImage::Init (DWORD dwX, DWORD dwY, COLORREF cColor, fp fZ)
{
   if (!Init(dwX, dwY))
      return FALSE;

   return Clear (cColor, fZ);
}

/*************************************************************************
CImage::Clear - Clears the image color to cColor, and sets the ZBuffer to
infinity.

inputs
   COLORREF    cColor - Color to clear to
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CImage::Clear (COLORREF cColor, fp fZ)
{
   // clear cached bitmap
   PaintCacheClear();

   // fill in prototype
   IMAGEPIXEL ip;
   memset (&ip, 0, sizeof(ip));
   if (fZ >= ZINFINITE)
      ip.fZ = ZINFINITE;
   else
      ip.fZ = fZ;
   Gamma (cColor, &ip.wRed);

   // do the first line
   DWORD    i;
   PIMAGEPIXEL pip;
   pip = Pixel (0, 0);
   for (i = 0; i < m_dwWidth; i++)
      pip[i] = ip;

   // all the other lines
   for (i = 1; i < m_dwHeight; i++)
      memcpy (Pixel(0,i), Pixel(0,0), sizeof(IMAGEPIXEL) * m_dwWidth);

   return TRUE;
}

/*************************************************************************
CImage::Clone - Creates a new CImage object which is a clone of this one.
It even includes the same bitmap information and Z Buffer

inputs
   none
returns
   PCImage  - Image. NULL if cant create
*/
CImage *CImage::Clone (void)
{
   PCImage  p = new CImage;
   if (!p)
      return FALSE;

   if (!p->Init (m_dwWidth, m_dwHeight)) {
      delete p;
      return NULL;
   }

   // copy over the bitmap
   memcpy (p->Pixel(0,0), Pixel(0,0), m_dwWidth * m_dwHeight * sizeof(IMAGEPIXEL));
   p->m_f360 = m_f360;

   return p;
}


/*************************************************************************
CImage::DrawBlock - Fills the rectangle, pr, with a block of color.
This color is blended from the four corners. It does NOT affect the object
ID or Z buffer.

inputs
   RECT     *pr - Rectangle (must be within bounds)
   COLORREF crUL, crUR, crLL, crLR - colors at the corners
returns
   none
*/
void CImage::DrawBlock (RECT *pr, COLORREF crUL, COLORREF crUR, COLORREF crLL, COLORREF crLR)
{
   // clear cached bitmap
   PaintCacheClear();

   // gamma correct
   WORD  awUL[3], awLL[3], awUR[3], awLR[3];
   Gamma (crUL, awUL);
   Gamma (crUR, awUR);
   Gamma (crLL, awLL);
   Gamma (crLR, awLR);

   // figure out delta on the left and right side
   int   iHeight, iWidth;
   iWidth = pr->right - pr->left;
   iHeight = pr->bottom - pr->top;
   int aiDeltaLeft[3], aiDeltaRight[3];
   DWORD i;
   for (i = 0; i < 3; i++) {
      aiDeltaLeft[i] = ( ((int)(DWORD)awLL[i]- (int)(DWORD)awUL[i]) << COLORFRAC) / iHeight;
      aiDeltaRight[i] = ( ((int)(DWORD)awLR[i] - (int)(DWORD)awUR[i]) << COLORFRAC) / iHeight;
   }

   // loop
   DWORD x, y;
   DWORD adwL[3], adwR[3];
   for (i = 0; i < 3; i++) {
      adwL[i] = (DWORD)awUL[i] << COLORFRAC;
      adwR[i] = (DWORD)awUR[i] << COLORFRAC;
   }
   for (y = (DWORD) pr->top; y < (DWORD)pr->bottom; y++) {
      // figure out delta and current
      int aiDelta[3];
      DWORD adwCur[3];
      for (i = 0; i < 3; i++) {
         aiDelta[i] = ((int) adwR[i] - (int)adwL[i]) / iWidth;
         adwCur[i] = adwL[i];
      }

      PIMAGEPIXEL pip;
      pip = Pixel((DWORD) pr->left, y);

      for (x = 0; x < (DWORD)iWidth; x++, pip++) {
         pip->wRed = (WORD) (adwCur[0] >> COLORFRAC);
         pip->wGreen = (WORD) (adwCur[1] >> COLORFRAC);
         pip->wBlue = (WORD) (adwCur[2] >> COLORFRAC);

         // increment values
         for (i = 0; i < 3; i++)
            adwCur[i] += (DWORD)aiDelta[i];
      }

      // increment the color
      for (i = 0; i < 3; i++) {
         adwL[i] += (DWORD)aiDeltaLeft[i];
         adwR[i] += (DWORD)aiDeltaRight[i];
      }
   }
}


/****************************************************************************************
CImage::ToBitmap - Creates a bitmap (compatible to the given HDC) with the size
of the image. THen, ungammas the image and copies it all into the bitmap.

inputs
   HDC      hDC - Create a bitmap compatible to this
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CImage::ToBitmap (HDC hDC)
{
#ifdef _TIMERS
   DWORD dwStart = GetTickCount();
#endif
   MALLOCOPT_INIT;

   // BUGBUG - I don't know that should use HDC for this. Maybe should
   // always return 24-bit bitmap?

   HBITMAP hBit;
   DWORD x, y;
   PIMAGEPIXEL pip;
   BYTE * pb;

   int   iBitsPixel, iPlanes;
   iBitsPixel = GetDeviceCaps (hDC, BITSPIXEL);
   iPlanes = GetDeviceCaps (hDC, PLANES);
   BITMAP  bm;
   memset (&bm, 0, sizeof(bm));
   bm.bmWidth = Width();
   bm.bmHeight = Height();
   bm.bmPlanes = iPlanes;
   bm.bmBitsPixel = iBitsPixel;

   // first see if can do a 24-bit bitmap, then 16-bit, then do it the slow way
   if ( (iBitsPixel == 32) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = Width() * 4;
	   MALLOCOPT_OKTOMALLOC;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
	   MALLOCOPT_RESTORE;
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      DWORD *pdw, dwMax;
      pdw = (DWORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (x = dwMax; x; x--, pip++, pdw++)
         *pdw = RGB(UnGamma(pip->wBlue), UnGamma(pip->wGreen), UnGamma(pip->wRed));

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 24) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = Width() * 3;
      if (bm.bmWidthBytes % 2)
         bm.bmWidthBytes++;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      for (y = 0; y < m_dwHeight; y++) {
         pb = (BYTE*) bm.bmBits + (y * bm.bmWidthBytes);
         for (x = 0; x < m_dwWidth; x++, pip++) {
            *(pb++) = UnGamma(pip->wBlue);
            *(pb++) = UnGamma(pip->wGreen);
            *(pb++) = UnGamma(pip->wRed);
         }
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 16) && (iPlanes == 1) ) {
      // 16 bit bitmap
      bm.bmWidthBytes = Width() * 2;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      WORD *pw;
      DWORD dwMax;
      pw = (WORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (x = 0; x < dwMax; x++, pip++, pw++) {
         BYTE r,g,b;
         r = UnGamma(pip->wRed) >> 3;
         g = UnGamma(pip->wGreen) >> 2;
         b = UnGamma(pip->wBlue) >> 3;
         *pw = ((WORD)r << 11) | ((WORD)g << 5) | ((WORD)b);
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else {
      // the slow way
      hBit = CreateCompatibleBitmap (hDC, (int) m_dwWidth, (int) m_dwHeight);
      if (!hBit)
         return NULL;

      HDC hDCTemp;
      hDCTemp = CreateCompatibleDC (hDC);
      SelectObject (hDCTemp, hBit);

      // loop
      pip = Pixel (0, 0);
      for (y = 0; y < m_dwHeight; y++) for (x = 0; x < m_dwWidth; x++, pip++)
         SetPixel (hDCTemp, (int)x, (int)y, UnGamma(&pip->wRed));

      // done
      DeleteDC (hDCTemp);
   }

#ifdef _TIMERS
   char szTemp[128];
   sprintf (szTemp, "ToBitmap time = %d\r\n", (int) (GetTickCount() - dwStart));
   OutputDebugString (szTemp);
#endif
   return hBit;
}


/****************************************************************************************
DownsamplePixel - Given a pixel X and Y, takes dwDown-1 to the right and bottom
and averages together. NOTE: Doesn't do boundary checking

inputs
   PCImage     pImage - image
   DWORD       dwDown - amount to downsample
   DWORD       dwX - X
   DWORD       dwY - Y
   WORD        *pwRed, *pwGreen, *pwBlue - Filled with red, green, blue
*/
static __inline void DownsamplePixel (PCImage pImage, DWORD dwDown, DWORD dwX, DWORD dwY,
                                      WORD *pwRed, WORD *pwGreen, WORD *pwBlue)
{
   DWORD dwRed, dwGreen, dwBlue;
   dwRed = dwGreen = dwBlue = 0;

   DWORD x,y;
   for (y = 0; y < dwDown; y++) {
      PIMAGEPIXEL pip = pImage->Pixel(dwX, y+dwY);
      for (x = 0; x < dwDown; x++, pip++) {
         dwRed += pip->wRed;
         dwGreen += pip->wGreen;
         dwBlue += pip->wBlue;
      }
   }

   DWORD dwSquare;
   dwSquare = dwDown * dwDown;
   *pwRed = (WORD) (dwRed / dwSquare);
   *pwGreen = (WORD) (dwGreen / dwSquare);
   *pwBlue = (WORD) (dwBlue / dwSquare);
}


/****************************************************************************************
CImage::ToBitmapAntiAlias - Creates a bitmap (compatible to the given HDC) with the size
of the image. THen, ungammas the image and copies it all into the bitmap.

inputs
   HDC      hDC - Create a bitmap compatible to this
   DWORD    dwDown - Amount to downsample by. 2x = 2x2 pixels, etc.
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CImage::ToBitmapAntiAlias (HDC hDC, DWORD dwDown)
{
#ifdef _TIMERS
   DWORD dwStart = GetTickCount();
#endif

   HBITMAP hBit;
   DWORD x, y;
   PIMAGEPIXEL pip;
   BYTE * pb;

   int   iBitsPixel, iPlanes;
   iBitsPixel = GetDeviceCaps (hDC, BITSPIXEL);
   iPlanes = GetDeviceCaps (hDC, PLANES);
   BITMAP  bm;
   memset (&bm, 0, sizeof(bm));
   bm.bmWidth = Width() / dwDown;
   bm.bmHeight = Height() / dwDown;
   bm.bmPlanes = iPlanes;
   bm.bmBitsPixel = iBitsPixel;

   WORD wRed, wGreen, wBlue;

   // first see if can do a 24-bit bitmap, then 16-bit, then do it the slow way
   if ( (iBitsPixel == 32) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = Width() * 4 / dwDown;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      DWORD *pdw, dwMax;
      pdw = (DWORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (y = 0; y < m_dwHeight; y += dwDown) for (x = 0; x < m_dwWidth; x += dwDown, pdw++) {
         DownsamplePixel (this, dwDown, x, y, &wRed, &wGreen, &wBlue);
         *pdw = RGB(UnGamma(wBlue), UnGamma(wGreen), UnGamma(wRed));
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 24) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = Width() * 3 / dwDown;
      if (bm.bmWidthBytes % 2)
         bm.bmWidthBytes++;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      for (y = 0; y < m_dwHeight; y += dwDown) {
         pb = (BYTE*) bm.bmBits + (y * bm.bmWidthBytes);
         for (x = 0; x < m_dwWidth; x += dwDown) {
            DownsamplePixel (this, dwDown, x, y, &wRed, &wGreen, &wBlue);
            *(pb++) = UnGamma(wBlue);
            *(pb++) = UnGamma(wGreen);
            *(pb++) = UnGamma(wRed);
         }
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 16) && (iPlanes == 1) ) {
      // 16 bit bitmap
      bm.bmWidthBytes = Width() * 2 / dwDown;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      WORD *pw;
      DWORD dwMax;
      pw = (WORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (y = 0; y < m_dwHeight; y += dwDown) for (x = 0; x < m_dwWidth; x += dwDown, pw++) {
         BYTE r,g,b;
         DownsamplePixel (this, dwDown, x, y, &wRed, &wGreen, &wBlue);
         r = UnGamma(wRed) >> 3;
         g = UnGamma(wGreen) >> 2;
         b = UnGamma(wBlue) >> 3;
         *pw = ((WORD)r << 11) | ((WORD)g << 5) | ((WORD)b);
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else {
      // the slow way
      hBit = CreateCompatibleBitmap (hDC, (int) m_dwWidth / dwDown, (int) m_dwHeight / dwDown);
      if (!hBit)
         return NULL;

      HDC hDCTemp;
      hDCTemp = CreateCompatibleDC (hDC);
      SelectObject (hDCTemp, hBit);

      // loop
      pip = Pixel (0, 0);
      for (y = 0; y < m_dwHeight; y += dwDown) for (x = 0; x < m_dwWidth; x += dwDown) {
         DownsamplePixel (this, dwDown, x, y, &wRed, &wGreen, &wBlue);
         SetPixel (hDCTemp, (int)(x/dwDown), (int)(y/dwDown), RGB(UnGamma(wRed),UnGamma(wGreen),UnGamma(wBlue)));
      }

      // done
      DeleteDC (hDCTemp);
   }

#ifdef _TIMERS
   char szTemp[128];
   sprintf (szTemp, "ToBitmap time = %d\r\n", (int) (GetTickCount() - dwStart));
   OutputDebugString (szTemp);
#endif
   return hBit;
}


/****************************************************************************************
CImage::Enlarge - Creates a bitmap (compatible to the given HDC) with the size
of the image. THen, ungammas the image and copies it all into the bitmap.

inputs
   HDC      hDC - Create a bitmap compatible to this
   DWORD    dwDown - Amount to downsample by. 2x = 2x2 pixels, etc.
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CImage::ToBitmapEnlarge (HDC hDC, DWORD dwScale)
{
#ifdef _TIMERS
   DWORD dwStart = GetTickCount();
#endif

   HBITMAP hBit;
   DWORD x, y;
   PIMAGEPIXEL pip;
   BYTE * pb;

   int   iBitsPixel, iPlanes;
   iBitsPixel = GetDeviceCaps (hDC, BITSPIXEL);
   iPlanes = GetDeviceCaps (hDC, PLANES);
   BITMAP  bm;
   memset (&bm, 0, sizeof(bm));
   bm.bmWidth = Width() * dwScale;
   bm.bmHeight = Height() * dwScale;
   bm.bmPlanes = iPlanes;
   bm.bmBitsPixel = iBitsPixel;


   // first see if can do a 24-bit bitmap, then 16-bit, then do it the slow way
   if ( (iBitsPixel == 32) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = Width() * 4 * dwScale;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      DWORD *pdw, dwMax;
      pdw = (DWORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;

      if (dwScale == 2) {
         // special optimiziation
         for (y = 0; y < (DWORD) bm.bmHeight; y++) {
            pip = Pixel(0, y / dwScale);
            for (x = m_dwWidth; x; x--) {
               pdw[0] = pdw[1] = RGB(UnGamma(pip->wBlue), UnGamma(pip->wGreen), UnGamma(pip->wRed));
               pdw += 2;
               pip++;
            }
         }
      }
      else {
         for (y = 0; y < (DWORD) bm.bmHeight; y ++) for (x = 0; x < (DWORD) bm.bmWidth; x++, pdw++) {
            pip = Pixel (x / dwScale, y / dwScale);
            *pdw = RGB(UnGamma(pip->wBlue), UnGamma(pip->wGreen), UnGamma(pip->wRed));
         }
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 24) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = Width() * 3 * dwScale;
      if (bm.bmWidthBytes % 2)
         bm.bmWidthBytes++;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      for (y = 0; y < (DWORD) bm.bmHeight; y ++) {
         pb = (BYTE*) bm.bmBits + (y * bm.bmWidthBytes);
         for (x = 0; x < (DWORD) bm.bmWidth; x++) {
            pip = Pixel (x / dwScale, y / dwScale);
            *(pb++) = UnGamma(pip->wBlue);
            *(pb++) = UnGamma(pip->wGreen);
            *(pb++) = UnGamma(pip->wRed);
         }
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 16) && (iPlanes == 1) ) {
      // 16 bit bitmap
      bm.bmWidthBytes = Width() * 2 * dwScale;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      WORD *pw;
      DWORD dwMax;
      pw = (WORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (y = 0; y < (DWORD) bm.bmHeight; y ++) for (x = 0; x < (DWORD) bm.bmWidth; x ++, pw++) {
         BYTE r,g,b;
         pip = Pixel (x / dwScale, y / dwScale);
         r = UnGamma(pip->wRed) >> 3;
         g = UnGamma(pip->wGreen) >> 2;
         b = UnGamma(pip->wBlue) >> 3;
         *pw = ((WORD)r << 11) | ((WORD)g << 5) | ((WORD)b);
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else {
      // the slow way
      hBit = CreateCompatibleBitmap (hDC, (int) m_dwWidth * dwScale, (int) m_dwHeight * dwScale);
      if (!hBit)
         return NULL;

      HDC hDCTemp;
      hDCTemp = CreateCompatibleDC (hDC);
      SelectObject (hDCTemp, hBit);

      // loop
      pip = Pixel (0, 0);
      for (y = 0; y < (DWORD) bm.bmHeight; y++) for (x = 0; x < (DWORD) bm.bmWidth; x ++) {
         pip = Pixel (x / dwScale, y / dwScale);
         SetPixel (hDCTemp, (int)x, (int)y, RGB(UnGamma(pip->wRed),UnGamma(pip->wGreen),UnGamma(pip->wBlue)));
      }

      // done
      DeleteDC (hDCTemp);
   }

#ifdef _TIMERS
   char szTemp[128];
   sprintf (szTemp, "ToBitmap time = %d\r\n", (int) (GetTickCount() - dwStart));
   OutputDebugString (szTemp);
#endif
   return hBit;
}

/****************************************************************************************
CImage::PaintCacheClear - Clears the bitmap from the paint cache.Call this if the
contents of the CImage have been changed since the last paint, or will be changed before
the next paint. Some functions, like clear, call this automatically. The paint functions
that are frequently calls (like drawpolygon) do not, for optimization reaons.
*/
void CImage::PaintCacheClear (void)
{
   if (m_hBitCache) {
      DeleteObject (m_hBitCache);
      m_hBitCache = NULL;
   }
}

/****************************************************************************************
CImage::Paint - Paints an image onto the given DC. Intentally, this is done by
creating a temporary bitmap with the image and the bit-bliting it

inputs
   HDC      hDC - to Draw on
   RECT     *prSrc - Portion of CImage to be drawn. This must be within bound.
               If NULL, then the entire image is drawn
   POINT    pDest - destination to paint UL corner to on HDC
returns
   BOOL - TRUE if successful
*/
BOOL CImage::Paint (HDC hDC, RECT *prSrc, POINT pDest)
{
   if (!m_hBitCache)
      m_hBitCache = ToBitmap(hDC);
   if (!m_hBitCache)
      return FALSE;

   HDC hDCTemp;
   hDCTemp = CreateCompatibleDC (hDC);
   if (!hDCTemp) {
      return FALSE;
   }
   SelectObject (hDCTemp, m_hBitCache);

   RECT rTemp;
   if (!prSrc) {
      rTemp.left = rTemp.top = 0;
      rTemp.right = (int) m_dwWidth;
      rTemp.bottom = (int) m_dwHeight;
      prSrc = &rTemp;
   }

   BitBlt (hDC, pDest.x, pDest.y, prSrc->right - prSrc->left, prSrc->bottom - prSrc->top,
      hDCTemp, prSrc->left, prSrc->top, SRCCOPY);


   DeleteDC (hDCTemp);

   return TRUE;
}


/****************************************************************************************
CImage::Paint - Paints an image onto the given DC. Intentally, this is done by
creating a temporary bitmap with the image and the bit-bliting it

inputs
   HDC      hDC - to Draw on
   RECT     *prSrc - Portion of CImage to be drawn. This must be within bound.
               If NULL, then the entire image is drawn
   RECT     *prDest - destination rectangle. The image will be streched
returns
   BOOL - TRUE if successful
*/
BOOL CImage::Paint (HDC hDC, RECT *prSrc, RECT *prDest)
{
   RECT rTemp;
   if (!prSrc) {
      rTemp.left = rTemp.top = 0;
      rTemp.right = (int) m_dwWidth;
      rTemp.bottom = (int) m_dwHeight;
      prSrc = &rTemp;
   }

   // see if it's the same size
   if ( ((prSrc->right - prSrc->left) == (prDest->right - prDest->left) &&
      ((prSrc->bottom - prSrc->top) == prDest->bottom - prDest->top)) ) {

      POINT Dest;
      RECT SrcAnti;
      Dest.x = prDest->left;
      Dest.y = prDest->top;
      SrcAnti.left = prSrc->left;
      SrcAnti.right = prSrc->right;
      SrcAnti.top = prSrc->top;
      SrcAnti.bottom = prSrc->bottom;

      return Paint (hDC, &SrcAnti, Dest);
   }

   // can this be antialiased
   int iScale;
   iScale = 0;
   if (prDest->right > prDest->left) {
      iScale = (prSrc->right - prSrc->left) / (prDest->right - prDest->left);
      if (iScale * (prDest->right - prDest->left) != (prSrc->right - prSrc->left))
         iScale = 0;
      if (iScale * (prDest->bottom - prDest->top) != (prSrc->bottom - prSrc->top))
         iScale = 0;
   }
   if (iScale > 1) {
      if (!m_hBitCache)
         m_hBitCache = ToBitmapAntiAlias (hDC, iScale);
      POINT Dest;
      RECT SrcAnti;
      Dest.x = prDest->left;
      Dest.y = prDest->top;
      SrcAnti.left = prSrc->left / iScale;
      SrcAnti.right = prSrc->right / iScale;
      SrcAnti.top = prSrc->top / iScale;
      SrcAnti.bottom = prSrc->bottom / iScale;

      return Paint (hDC, &SrcAnti, Dest);
   }

   // see if it's a small version
   iScale = 0;
   if (prSrc->right > prSrc->left) {
      iScale = (prDest->right - prDest->left) / (prSrc->right - prSrc->left);
      if (iScale * (prSrc->right - prSrc->left) != (prDest->right - prDest->left))
         iScale = 0;
      if (iScale * (prSrc->bottom - prSrc->top) != (prDest->bottom - prDest->top))
         iScale = 0;
   }
   if (iScale > 1) {
      if (!m_hBitCache)
         m_hBitCache = ToBitmapEnlarge (hDC, iScale);
      POINT Dest;
      RECT SrcAnti;
      Dest.x = prDest->left;
      Dest.y = prDest->top;
      SrcAnti.left = prSrc->left * iScale;
      SrcAnti.right = prSrc->right * iScale;
      SrcAnti.top = prSrc->top * iScale;
      SrcAnti.bottom = prSrc->bottom * iScale;

      return Paint (hDC, &SrcAnti, Dest);
   }


   if (!m_hBitCache)
      m_hBitCache = ToBitmap(hDC);
   if (!m_hBitCache)
      return FALSE;

   HDC hDCTemp;
   hDCTemp = CreateCompatibleDC (hDC);
   if (!hDCTemp) {
      return FALSE;
   }
   SelectObject (hDCTemp, m_hBitCache);

   //BitBlt (hDC, prDest->left, prDest->top, prSrc->right - prSrc->left, prSrc->bottom - prSrc->top,
   //   hDCTemp, prSrc->left, prSrc->top, SRCCOPY);

   SetStretchBltMode (hDC, COLORONCOLOR);
   StretchBlt (hDC, prDest->left, prDest->top,
      prDest->right - prDest->left, prDest->bottom - prDest->top,
      hDCTemp,
      prSrc->left, prSrc->top,
      prSrc->right - prSrc->left, prSrc->bottom - prSrc->top,
      SRCCOPY);


   DeleteDC (hDCTemp);

   return TRUE;
}

/****************************************************************************
CImage::DrawPixel - Draws a pixel on the screen.

inputs
   DWORD    dwX, dwY - XY location. ASSUMES that the points are within the drawing area
   fp       fZ - Depth in meters. Positive value
   DWORD    *pwColor - Color to use
   PIDMISC  pm - Use the object ID information from here, not transparency or color
returns
   none
*/
__inline void CImage::DrawPixel (DWORD dwX, DWORD dwY, fp fZ, const WORD *pwColor, const PIDMISC pm)
{
   PIMAGEPIXEL pip = Pixel(dwX, dwY);

   // zbuffer
   if (fZ >= pip->fZ)
      return;

   pip->dwID = pm->dwID;
   pip->fZ = fZ;
   pip->wRed = pwColor[0];
   pip->wGreen = pwColor[1];
   pip->wBlue = pwColor[2];
   pip->dwIDPart = pm->dwIDPart;
}


/********************************************************************
DrawLine - Draws a line on the screen.

inputs
   PHILEND     pStart, pEnd - Start and end points. NOTE: While this has
                  safety clipping, it's not terribly fast for long lines off
                  the screen area.
   PIDMISC     pm - Miscellaneous info. Only uses the object ID stuff.
*/
void CImage::DrawLine (PIHLEND pStart, PIHLEND pEnd, PIDMISC pm)
{
   // for citical seciton
   DWORD dwCritSec = (DWORD)-1;  // not using any right now


   // NOTE - Ignoring the /w effect on drawing.
   fp   fDeltaX, fDeltaY;
   fp   x1, x2, y1, y2;
doagain:
   x1 = pStart->x;
   x2 = pEnd->x;
   y1 = pStart->y;
   y2 = pEnd->y;

   fDeltaX = x2 - x1;
   fDeltaY = y2 - y1;

   if (!fDeltaX && !fDeltaY) {
      // just a point
      if ((x1 >= 0) && ((DWORD) x1 < m_dwWidth) && (y1 >= 0) && ((DWORD)y1 < m_dwHeight)) {
         // potentially enter a critical seciton
         if (m_fCritSecInitialized && (y1 >= 0)) {
            DWORD dwCritSecWant = ((DWORD)y1 / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
            if (dwCritSecWant != dwCritSec) {
               if (dwCritSec != (DWORD)-1)
                  LeaveCriticalSection (&m_aCritSec[dwCritSec]);
               dwCritSec = dwCritSecWant;
               EnterCriticalSection (&m_aCritSec[dwCritSec]);
            }
         }

         DrawPixel ((DWORD)x1, (DWORD)y1, pStart->z, pStart->aColor, pm);
      }
      goto done;
   }

   fp   fAbsX, fAbsY;
   fAbsX = max(fDeltaX,-fDeltaX);
   fAbsY = max(fDeltaY, -fDeltaY);

   // determine if it's mostly up/down
   BOOL  fRightLeft;
   int   i;
   fp   fY, fX, fDelta, fZ, fDeltaZ;
   fRightLeft = (fAbsX > fAbsY);

   // rearrange this data so if we're mostly up/down draw from top to bottom
   // and if we're mostly right/left draw from left to right
   if ( (fRightLeft && (fDeltaX < 0)) || (!fRightLeft && (fDeltaY < 0)) ) {
      PIHLEND t;
      t = pStart;
      pStart = pEnd;
      pEnd = t;
      goto doagain;
   }

#ifdef _TIMERS
   gRenderStats.dwNumDiagLine++;
#endif

   fDeltaZ = (pEnd->z - pStart->z) / (fRightLeft ? fAbsX : fAbsY);
   fZ = pStart->z;

   // RGB
   fp   afColor[3], afColorDelta[3];
   WORD     awColor[3];
   for (i = 0; i < 3; i++) {
      afColor[i] = pStart->aColor[i];
      afColorDelta[i] = ((fp)pEnd->aColor[i] - (fp)pStart->aColor[i]) /
         (fRightLeft ? fAbsX : fAbsY);
   }


   // if it's mostly up/down do one algorith, else another
   if (fRightLeft) {
      // either moving right/left
      fY = y1;
      fDelta = fDeltaY / fDeltaX;

      // NOTE: fDeltaX is always more than 0, so can move in right direction

      int   ix1, ix2;
      ix1 = (int) x1;
      ix2 = (int) x2;
      for (i = ix1; i < ix2; i++, fY += fDelta, fZ += fDeltaZ,
         afColor[0] += afColorDelta[0], afColor[1] += afColorDelta[1], afColor[2] += afColorDelta[2]) {

         if ( (i < 0) || (i >= (int) m_dwWidth) || (fY < 0) || (fY >= m_dwHeight))
            continue;

         awColor[0] = (WORD) afColor[0];
         awColor[1] = (WORD) afColor[1];
         awColor[2] = (WORD) afColor[2];

         // potentially enter a critical seciton
         if (m_fCritSecInitialized && (fY >= 0)) {
            DWORD dwCritSecWant = ((DWORD)fY / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
            if (dwCritSecWant != dwCritSec) {
               if (dwCritSec != (DWORD)-1)
                  LeaveCriticalSection (&m_aCritSec[dwCritSec]);
               dwCritSec = dwCritSecWant;
               EnterCriticalSection (&m_aCritSec[dwCritSec]);
            }
         }

         DrawPixel ((DWORD) i, (DWORD) fY, (int) fZ, awColor, pm);
      }

   }
   else {
      // moving up/down
      fX = x1;
      fDelta = fDeltaX / fDeltaY;

      // NOTE: fDeltaY is always more than 0, so can move in down direction

      int   iy1, iy2;
      iy1 = (int) y1;
      iy2 = (int) y2;

      // move to the right
      for (i = iy1; i < iy2; i++, fX += fDelta, fZ += fDeltaZ,
         afColor[0] += afColorDelta[0], afColor[1] += afColorDelta[1], afColor[2] += afColorDelta[2]) {

         if ( (i < 0) || (i >= (int) m_dwHeight) || (fX < 0) || (fX >= m_dwWidth))
            continue;

         awColor[0] = (WORD) afColor[0];
         awColor[1] = (WORD) afColor[1];
         awColor[2] = (WORD) afColor[2];


         // potentially enter a critical seciton
         if (m_fCritSecInitialized && (i >= 0)) {
            DWORD dwCritSecWant = ((DWORD)i / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
            if (dwCritSecWant != dwCritSec) {
               if (dwCritSec != (DWORD)-1)
                  LeaveCriticalSection (&m_aCritSec[dwCritSec]);
               dwCritSec = dwCritSecWant;
               EnterCriticalSection (&m_aCritSec[dwCritSec]);
            }
         }

         DrawPixel ((DWORD) fX, (DWORD) i, (int) fZ, awColor, pm);
      }
   }

done:
   // free up the critical seciton
   if (dwCritSec != (DWORD)-1)
      LeaveCriticalSection (&m_aCritSec[dwCritSec]);

}

/**************************************************************************
CImage::TransparencyDither - Returns a pointer to an array of *pdwElem
integers that indicate a dither patther for a transparency, wTransparency.

inputs
   WORD     wTransparency - 0 is opaque (although this will still provide some
               transparency), 65545 is clear (although this still provides some
               opaqueness)
   int      iY - Y raster
   DWORD    *pdwElem - Filled with the number of elements
reutrsn
   int * - Pointer to an arrya of *pdwElem integers. A non-zero value means its transparent
*/
int *CImage::TransparencyDither (WORD wTransparency, int iY, DWORD *pdwElem)
{
   *pdwElem = IMAGETRANSSIZE;

   wTransparency = wTransparency >> (16 - IMAGEDITHERBITS);

   return gaImageDither[wTransparency][iY % IMAGETRANSSIZE];
}


/**************************************************************************
CImage::DrawHorzLine - Draws a horontal line that has a color blend or solid.

inputs
   int      y - y scan line
   PIHLEND  pLeft, pRight - Left and right edges. Notes:
               pLeft must be left, and pRight must be right
               Ignore pXXXX.y.
               Ignore pXXXX.wColor if fSolid is set.
   PIDMISC  pm - Miscellaneous info. Ignores pm.wColor if !fSolid
   BOOL     fSolid - Set to true if should fill with a solid color, FALSE if color blend
   PCTextureMap pTexture - If not NULL then use the texture map
   fp       fGlowScale - Scale any glow by this amount - to account for exposure
   DWORD    dwTextInfo - Texture info... one or more of the DHLTEXT_XXX flags
   DWORD       dwThread - Thread rendering with, from 0 to MAXRAYTHREAD-1
   BOOL        fFinalRender - Set to TRUE if this is for the final render
returns
   none
*/
#define  REJECT      (-ZINFINITE)
#define  SHADOWTRANSCUTOFF    0xf000      // if transparency less than this this then entirely
                                          // transparent for calculating shadows
void CImage::DrawHorzLine (int y, const PIHLEND pLeft, const PIHLEND pRight, const PIDMISC pm, BOOL fSolid,
                           const PCTextureMapSocket pTexture, fp fGlowScale, DWORD dwTextInfo,
                           DWORD dwThread, BOOL fFinalRender)
{
   // check y
   if ((y < 0) || (y >= (int)m_dwHeight))
      return;

#ifdef _TIMERS
   gRenderStats.dwNumHLine++;
#endif
   DWORD dwNewID;
   dwNewID = pm->dwIDPart;

   // find out where it starts on the screen
   int   xStart, xEnd;
   fp fFloor;
   xStart = (int) ceil(pLeft->x);
   fFloor = floor(pRight->x);
   // if the right ends exactly on an integer, don't count it because count the
   // left if it's exactly on an integer. Counting both causes problems
   xEnd = (int) ((fFloor == pRight->x) ? (fFloor-1) : fFloor);
   //xEnd = (int) floor(pRight->x);
   xStart = max(0,xStart);
   xEnd = min((int)m_dwWidth-1,xEnd);

   // figure out z delta
   fp   xDelta = pRight->x - pLeft->x;
   if ((xEnd < xStart) || (xDelta <= 0))
      return;  // cant do this
   fp fSkip;
   fSkip = xStart - pLeft->x;


#ifdef _TIMERS
   gRenderStats.dwPixelRequest += (xEnd - xStart + 1);
#endif
   // simple trivial reject of the entire line by seeing if none
   // of the line couldnt possibly be in front
   DWORD dw, dwPixels, dwCount;
   PIMAGEPIXEL pip, pipOrig;
   fp fMinZ;
   pip = pipOrig = Pixel(xStart, y);
   dwPixels = (DWORD) (xEnd - xStart + 1);
   fMinZ = min(pLeft->z, pRight->z);
   for (dw = dwPixels; dw; dw--, pip++)
      if (fMinZ < pip->fZ)
         break;
   if (!dw) {
#ifdef _TIMERS
      gRenderStats.dwPixelTrivial += dwPixels;
#endif
      return;  // trivial reject
   }


   // figure out alpha
   m_amemLineDivide[dwThread].m_dwCurPosn = 0;
   DWORD dwNum;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   dwNum = GenerateAlphaInPerspective (pLeft->x, pRight->x, pLeft->w, pRight->w,
      (int) xStart, (int) xEnd - (int) xStart + 1, &m_amemLineDivide[dwThread]);
	MALLOCOPT_RESTORE;
   if (!dwNum)
      return;
   fp *paf;
   paf = (fp*) m_amemLineDivide[dwThread].p;

   // more complex calculation of Z buffer that takes into account
   // the W-interpolation
   fp zDelta;
   fp   *pf, *pfOrig, fStartZ;
   fp fX;
   pf = pfOrig = (fp*) m_amemZTemp[dwThread].p;
   pip = pipOrig = Pixel(xStart, y);
   dwPixels = (DWORD) (xEnd - xStart + 1);
   dwCount = 0;
   fp *pafCur;
   zDelta = pRight->z - pLeft->z;
   for (pafCur = paf,dw = dwPixels, fX = xStart; dw; dw--, pip++, pf++, pafCur++) {

      fStartZ = (pLeft->z + zDelta * pafCur[0]);

      // compare the z buffer
      if (pip->fZ >= fStartZ) {
         // the horizontal line is definitely in front
         *pf = fStartZ;
         dwCount++;
      }
      else {
         // the horizontal line is behind
         *pf = REJECT;
#ifdef _TIMERS
         gRenderStats.dwPixelBehind++;
#endif
         // dont change dwcount
      }
   }

#if 0 // dead code
   fp   fSkip = xStart - pLeft->x;
   fp   zDelta = (pRight->z - pLeft->z) / xDelta;
   int   izDelta = (int) (zDelta * 0x10000);
   int   iStartZ = (int) ((pLeft->z + zDelta * fSkip) * 0x10000);

   // loop and figure out trivial accept/reject due to z buffer
   int   *pi, *piOrig;
   pip = pipOrig = Pixel(xStart, y);
   dwPixels = (DWORD) (xEnd - xStart + 1);
   dwCount = 0;
   pi = piOrig = (int*) m_amemZTemp[dwThread].p;
   for (dw = dwPixels; dw; dw--, pip++, pi++, iStartZ += izDelta) {
      if (pip->iZ >= iStartZ) {
         // the horizontal line is definitely in front
         *pi = iStartZ;
         dwCount++;
      }
      else {
         // the horizontal line is behind
         *pi = REJECT;
         // dont change dwcount
      }
   }
#endif // 0

   // if !dwCount then trivial reject
   if (!dwCount) {
      return;
   }

   // if dwCount == dwPixels and not transparent then trivial accept
   BOOL fTrivialAccept;
   fTrivialAccept = ((dwCount == dwPixels) && !pm->Material.m_wTransparency);

   // BUGFIX - If contains transparencies and drawing textures then dont do transparency here
   BOOL fSkipTrans;
   fSkipTrans = (!fSolid && (dwTextInfo & DHLTEXT_PERPIXTRANS));

   // if transparent then do that now
   if (!fSkipTrans && pm->Material.m_wTransparency) {
      int *pt;
      DWORD dwElem;
      WORD wTrans = pm->Material.m_wTransparency;

      // BUGFIX - If this is for the final render and there's an invisible (highly transparent)
      // surface, then skip
      if (fFinalRender && (wTrans >= 0xff00))
         return;

      pf = (fp*) m_amemZTemp[dwThread].p;
      pt = TransparencyDither(wTrans, y, &dwElem);
      dwNewID |= IDPARTBITS_TRANSPARENT;
      for (dw = 0; dw < dwPixels; dw++, pf++)
         if (pt[(dw+xStart)%dwElem]) {
            *pf= REJECT;
#ifdef _TIMERS
            gRenderStats.dwPixelTransparent++;
#endif
         }
   }

   // if it's solid do something differnet than blend
   if (fSolid) {
      // figure out what this pixel is
      IMAGEPIXEL ip;
      ip.wRed = pm->wRed;
      ip.wGreen = pm->wGreen;
      ip.wBlue = pm->wBlue;
      ip.dwIDPart = dwNewID;
      ip.dwID = pm->dwID;

      // if it's trivial accept it's even easier
      pip = pipOrig;
      pf = pfOrig;
      if (fTrivialAccept) {
         for (dw = dwPixels; dw; dw--) {
            ip.fZ = *(pf++);
#ifdef _TIMERS
            if (pip[0].dwID)
               gRenderStats.dwPixelOverwritten++;
            gRenderStats.dwPixelWritten++;
#endif
            *(pip++) = ip;
         }
      }
      else {   // !trivial accept
         for (dw = dwPixels; dw; dw--) {
            if (*pf == (fp) REJECT) {
               pip++;
               pf++;
               continue;
            }
            ip.fZ = *(pf++);
#ifdef _TIMERS
            if (pip[0].dwID)
               gRenderStats.dwPixelOverwritten++;
            gRenderStats.dwPixelWritten++;
#endif
            *(pip++) = ip;
         }
      }
   } // fSolid

#ifdef SLOWBYACCURATE
   else {   // it's not solid color, so it's blended
      // color blend
      IMAGEPIXEL ip;
      ip.dwIDPart = pm->dwNewID;
      ip.dwID = pm->dwID;
      
      // calculate colors and deltas
      fp afColorDelta[3], afColor[3];
      DWORD i;
      for (i = 0; i < 3; i++) {
         afColorDelta[i] = (fp) pRight->aColor[i] - (fp)pLeft->aColor[i];
         afColor[i] = (fp) pLeft->aColor[i];
      }

      // if it's trivial accept it's even easier
      pip = pipOrig;
      pi = piOrig;
      fp fAlpha;
      for (pafCur = paf, dw = dwPixels; dw; dw--, pip++, pi++, pafCur++) {
         // deal with z
         if (*pi == REJECT) {
            continue;
         }
         ip.iZ = *pi;

         // colors
         fAlpha = pafCur[0];
         ip.wRed = (WORD)(afColor[0] + afColorDelta[0] * fAlpha);
         ip.wGreen = (WORD)(afColor[1] + afColorDelta[1] * fAlpha);
         ip.wBlue = (WORD)(afColor[2] + afColorDelta[2] * fAlpha);

NOTE: Does not calculate texture points.
         // copy over
         *pip = ip;
      }
   } // !fSolid
#else // FASTBYSLIGHTYINACCURATE
   else {   // it's not solid color, so it's blended
      // color blend
      IMAGEPIXEL ip;
      ip.dwIDPart = dwNewID;
      ip.dwID = pm->dwID;
      
      // calculate colors and deltas
      fp afColorDelta[3];
      int aiColorDelta[3], aiColor[3];
      DWORD i;
      for (i = 0; i < 3; i++) {
         afColorDelta[i] = ((fp) pRight->aColor[i] - (fp)pLeft->aColor[i]) / xDelta;
         aiColorDelta[i] = (int) (afColorDelta[i] * (1 << COLORFRAC));
         aiColor[i] = (int) ((pLeft->aColor[i] + afColorDelta[i] * fSkip) * (1 << COLORFRAC));
      }

      // texture map?
      PGCOLOR pacTexture;
      float *pafGlow;
      WORD *pawTrans;
      BOOL fGlow, fTrans;
      pacTexture = NULL;
      pafGlow = NULL;
      pawTrans = NULL;
      fGlow = fTrans = FALSE;
      if (pTexture) {
	      MALLOCOPT_OKTOMALLOC;
         if (!m_amemGlow[dwThread].Required (dwPixels * 3 * sizeof(float))) {
   	      MALLOCOPT_RESTORE;
            return;  // error
         }
	      MALLOCOPT_RESTORE;
         pafGlow = (float*) m_amemGlow[dwThread].p;

	      MALLOCOPT_OKTOMALLOC;
         if (!m_amemTrans[dwThread].Required (dwPixels * sizeof(WORD))) {
   	      MALLOCOPT_RESTORE;
            return;  // error
         }
	      MALLOCOPT_RESTORE;
         pawTrans = (WORD*) m_amemTrans[dwThread].p;

	      MALLOCOPT_OKTOMALLOC;
         if (m_amemTexture[dwThread].Required (dwPixels * sizeof(GCOLOR))) {
   	      MALLOCOPT_RESTORE;
            pacTexture = (PGCOLOR) m_amemTexture[dwThread].p;
            pTexture->FillLine (dwThread, pacTexture, dwPixels, &pLeft->tp, &pRight->tp,
               pafGlow, &fGlow, pawTrans, &fTrans, paf);
            // IMPORTANT - This may not be correct since not taking into account skip

#if 0 // to test which thread renders which polygons
            DWORD k;
            for (k = 0; k < dwPixels; k++)
               pacTexture[k].wRed = pacTexture[k].wGreen = pacTexture[k].wBlue = (WORD)(dwThread*0x3000);
#endif
         }
	      MALLOCOPT_RESTORE;
      }

      // if it's trivial accept it's even easier
      pip = pipOrig;
      pf = pfOrig;
      if (pacTexture) {
         // loop using a color map. Only use one color because the rest
         // should all be the same - for this version at lease
         // loop using the colors
         DWORD dwVal;
         for (dw = dwPixels; dw; dw--, pip++, pf++, pacTexture++) {
            // deal with z
            if (*pf == (fp) REJECT) {
               aiColor[0] += aiColorDelta[0];

               // BUGFIX - Wasn't incrementing everything
               if (fGlow)
                  pafGlow += 3;
               if (fTrans)
                  pawTrans++;

               continue;
            }
            ip.fZ = *pf;

            // increment intesnity
            dwVal = (aiColor[0] >> COLORFRAC);
            aiColor[0] += aiColorDelta[0];

            // BUGFIX - So textures can be saturates
            DWORD dw2;
            dw2 = (dwVal * (WORD)pacTexture->wRed ) >> (16 - 2);   // the -2 is a x4 used earlier for colors
            dw2 = min(0xffff, dw2);
            ip.wRed = (WORD) dw2;
            dw2 = (dwVal * (WORD)pacTexture->wGreen ) >> (16 - 2);   // the -2 is a x4 used earlier for colors
            dw2 = min(0xffff, dw2);
            ip.wGreen = (WORD) dw2;
            dw2 = (dwVal * (WORD)pacTexture->wBlue ) >> (16 - 2);   // the -2 is a x4 used earlier for colors
            dw2 = min(0xffff, dw2);
            ip.wBlue = (WORD) dw2;

            // if there's a glow then add colors
            if (fGlow) {
               // scale by expsoure
               // BUGFIX - Reordered so wouldnt get internal compiler error
               pafGlow[0] = min((fp)0xffff, pafGlow[0] * fGlowScale + (fp)ip.wRed);
               ip.wRed = (WORD) (DWORD) pafGlow[0];
               pafGlow[1] = min((fp)0xffff, pafGlow[1] * fGlowScale + (fp)ip.wGreen);
               ip.wGreen = (WORD) (DWORD) pafGlow[1];
               pafGlow[2] = min((fp)0xffff, pafGlow[2] * fGlowScale + (fp)ip.wBlue);
               ip.wBlue = (WORD) (DWORD) pafGlow[2];

               pafGlow += 3;
            }

            // if there's transparency then use it
            if (fTrans) {
               int *pt;
               DWORD dwElem;
               if (pawTrans[0] == 0) {
                  pawTrans++;
               }
               else if (pawTrans[0] == 0xffff) {
      #ifdef _TIMERS
                  gRenderStats.dwPixelTransparent++;
      #endif
                  pawTrans++;
                  continue;
               }
               else {
                  pt = TransparencyDither(pawTrans[0], y, &dwElem);
                  pawTrans++;
                  if (pt[(dwPixels - dw+xStart)%dwElem]) {
         #ifdef _TIMERS
                     gRenderStats.dwPixelTransparent++;
         #endif
                     continue;   // skip since transparent
                  }
               }
            }

#ifdef _TIMERS
            if (pip[0].dwID)
               gRenderStats.dwPixelOverwritten++;
            gRenderStats.dwPixelWritten++;
#endif

            // copy over
            *pip = ip;
         }
      }
      else {   // ! pacTexture
         // loop using the colors
         for (dw = dwPixels; dw; dw--, pip++, pf++) {
            // deal with z
            if (*pf == (fp) REJECT) {
               aiColor[0] += aiColorDelta[0];
               aiColor[1] += aiColorDelta[1];
               aiColor[2] += aiColorDelta[2];
               continue;
            }
            ip.fZ = *pf;

            // colors
            ip.wRed = (WORD)(aiColor[0] >> COLORFRAC);
            aiColor[0] += aiColorDelta[0];
            ip.wGreen = (WORD)(aiColor[1] >> COLORFRAC);
            aiColor[1] += aiColorDelta[1];
            ip.wBlue = (WORD)(aiColor[2] >> COLORFRAC);
            aiColor[2] += aiColorDelta[2];

#ifdef _TIMERS
            if (pip[0].dwID)
               gRenderStats.dwPixelOverwritten++;
            gRenderStats.dwPixelWritten++;
#endif
            // copy over
            *pip = ip;
         }
      }

   } // !fSolid
#endif

}

#if 0
/**********************************************************************************
CImage::DrawFillBetween2Lines - Fills from raster yTop to raster yBottom (inclusive).
   The right/left edges are two diagonal lines. They CAN cross over.

inputs
   fp      yTop - starting raster
   PIHLEND     pLeftTop, pRightTop - Starting points.
                  pXXXX.y is ignored
                  pXXXX.wColor is ignored if fSolid is set. Otherwise it's used
   fp      yBottom - Finishing raster
   PIHLEND     pLeftBottom, pRightBottom - Ending points.
                  pXXXX.y is ignored
                  pXXXX.wColor is ignored if fSolid is set. Otherwise it's used
   PIDMISC     pm - Miscellaneous information.
                  pm.wColor is ignored if !fSolid. Used if fSolid is true
   BOOL        fSolid - If TRUE then fill is a solid color. Else it's a blend
returns
   none
*/
void CImage::DrawFillBetween2Lines (fp yTop, PIHLEND pLeftTop, PIHLEND pRightTop,
      fp yBottom, PIHLEND pLeftBottom, PIHLEND pRightBottom,
      PIDMISC pm, BOOL fSolid)
{
   // figure out delta per Y that move down
   fp   yDelta;
   fp   xLeftDelta, xRightDelta;
   fp   zLeftDelta, zRightDelta;
   fp   curLeft[3], curRight[3];
   fp   cLeftDelta[3], cRightDelta[3];
   int      y, yMax;
   IHLEND   left, right;
   left = *pLeftTop;
   right = *pRightTop;

   yDelta = yBottom - yTop;
   if (yDelta <= 0.0)
      return;
   yDelta = 1.0 / yDelta;
   xLeftDelta = (pLeftBottom->x - left.x) * yDelta;
   xRightDelta = (pRightBottom->x - right.x) * yDelta;
   zLeftDelta = (pLeftBottom->z - left.z) * yDelta;
   zRightDelta = (pRightBottom->z - right.z) * yDelta;

   // color
   if (!fSolid) {
      for (y = 0; y < 3; y++) {
         curLeft[y] = left.aColor[y];
         cLeftDelta[y] = (pLeftBottom->aColor[y] - left.aColor[y]) * yDelta;
         curRight[y] = right.aColor[y];
         cRightDelta[y] = (pRightBottom->aColor[y] - right.aColor[y]) * yDelta;
      }
   }

   // clip in Y direction?
   fp   fNewY;
   fNewY = ceil(yTop);
   if (fNewY < 0)
      fNewY = 0;
   fp   fAlpha;
   if (fNewY != yTop) {
      fAlpha = (fNewY - yTop);
      yTop = fNewY;

      // increase x a bit
      left.x += xLeftDelta * fAlpha;
      right.x += xRightDelta * fAlpha;
      left.z += zLeftDelta * fAlpha;
      right.z += zRightDelta * fAlpha;

      // colors
      if (!fSolid) {
         for (y = 0; y < 3; y++) {
            curLeft[y] += cLeftDelta[y] * fAlpha;
            curRight[y] += cRightDelta[y] * fAlpha;
         }
      }
   }

   // convert the y range to integers
   y = (int) yTop;
   yMax = (int) floor(yBottom);
   if (yMax >= (int) m_dwHeight)
      yMax = (int) m_dwHeight - 1;


   DWORD i;
   for (; y <= yMax; y++) {
      // stick the current colors in
      if (!fSolid) {
         for (i = 0; i < 3; i++) {
            left.aColor[i] = (WORD) curLeft[i];
            curLeft[i] += cLeftDelta[i];
            right.aColor[i] = (WORD) curRight[i];
            curRight[i] += cRightDelta[i];
         }
      }

      // draw the lines in-between
      if (left.x < right.x)
         DrawHorzLine (y, &left, &right, pm, fSolid);
      else  // it's the other way around
         DrawHorzLine (y, &right, &left, pm, fSolid);

      // increment x and z
      left.x += xLeftDelta;
      right.x += xRightDelta;
      left.z += zLeftDelta;
      right.z += zRightDelta;

   }

   // done
}
#endif // 0

/*********************************************************************************
DrawPolygon - Draws a polygon, clipping against the screen border.

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PIHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  if fSolid then ppEnd[x]->wColor is ignored
   PIDMISC     pm - Miscellaneous info
                  if !fSolid the pm->wColor is ignored
   BOOL        fSolid - Set to TRUE if polygon is one solid color, FALSE if it's blended.
   PCTextureMap pTexture - If not NULL then use this texture map for color
   fp          fGlowScale - Amount to scale glow - affected by exposure
   DWORD       dwThread - Thread rendering with, from 0 to MAXRAYTHREAD-1
   DWORD          dwTextInfo - Set of DHLTEXT_XXX flags.
   BOOL        fFinalRender - Set to TRUE if this is for the final render
returns
   none
*/
// draw polygon structure, one per vertex
typedef struct {
   fp   fX;               // current X. initialized to the X at iTop
   fp   fXDelta;          // amount X is increased every scan line down
   fp   fZ;               // current Z. initialized to the Z at iTop
   fp   fZDelta;          // amount Z is increased every scan line down
   fp   fW;               // current W value
   fp   fWDelta;          // amount W changes every scna line down
   fp   afColor[3];       // Current RGB color, from 0..65535. Initialized to color at iTop
   fp   afColorDelta[3];  // RGB color delta per scanline
   TEXTPOINT5 tpStart;      // starting point for the texture
   TEXTPOINT5 tpDelta;      // end - start

   int      iState;           // current state. -1 = already used, 0 = active, 1 is waiting to be used
   int      iTop, iBottom;    // top and bottom Y, inclusive
   DWORD    dwMemAlpha;       // index in m_memPolyDivide where alpha is stored
   DWORD    dwNum;            // number of points in dwMemAlpha
   fp   *pafAlpha;        // point to precalculated alpha values
} DPS, *PDPS;

void CImage::DrawPolygonInt (DWORD dwNum, const PIHLEND ppEnd[], const PIDMISC pm, BOOL fSolid,
                          const PCTextureMapSocket pTexture, fp fGlowScale,
                          DWORD dwThread, DWORD dwTextInfo, BOOL fFinalRender)
{
   DWORD i;
	MALLOCOPT_INIT;

   PCMem pmemPolyDivide = &m_amemPolyDivide[dwThread];
   PCListFixed plistPoly = &m_alistPoly[dwThread];

   // for citical seciton
   DWORD dwCritSec = (DWORD)-1;  // not using any right now

   // reset memory for pixels
   pmemPolyDivide->m_dwCurPosn = 0;

   // initialize the polygon stuff, filling in the lines
   int   iNextLook;
   iNextLook = (int) m_dwHeight;
   plistPoly->Init (sizeof(DPS));
   // redundant plistPoly->Clear();
   PDPS pd;
   int y;

   // first pass, create structures for all the sides of the triangle
   // and then subdivide enough for perspective
   PIHLEND p1, p2, t;
   DWORD j;
   for (i = 0; i < dwNum; i++) {
      DPS dps;
      p1 = ppEnd[i];
      p2 = ppEnd[((i+1) < dwNum) ? (i+1) : 0];

      // p1 should be higher (lower y) than p2
      if (p1->y > p2->y) {
         t = p1;
         p1 = p2;
         p2 = t;
      }

      // find the delta
      fp yDelta;
      yDelta = p2->y - p1->y;
      if (yDelta <= 0.0)
         continue;
      yDelta = 1.0 / yDelta;

      // clip in Y direction?
      fp   fNewY;
      fNewY = ceil(p1->y);
      if (fNewY < 0)
         fNewY = 0;

      // convert the y range to integers
      dps.iTop = (int) fNewY;
      fp fAlpha;
      fAlpha = floor(p2->y);
      // if the bottom ends exactly on an integer, don't count it because count the
      // top if it's exactly on an integer. Counting both causes problems
      dps.iBottom = (int) ((fAlpha == p2->y) ? (fAlpha-1) : fAlpha);
      if (dps.iBottom >= (int) m_dwHeight)
         dps.iBottom = (int) m_dwHeight - 1;

      // if bottom < top then nothing here to draw so skip
      if (dps.iBottom < dps.iTop) {
         continue;
      }
      // if this is the first line so far then remember where things start
      if (dps.iTop < iNextLook)
         iNextLook = dps.iTop;

      // how much does x and the color change?
      dps.iState = 1;   // not yet done

      // calculate alpha values
      dps.dwMemAlpha = (DWORD)pmemPolyDivide->m_dwCurPosn;
	   MALLOCOPT_OKTOMALLOC;
      dps.dwNum = GenerateAlphaInPerspective (p1->y, p2->y, p1->w, p2->w,
         dps.iTop, dps.iBottom - dps.iTop + 1, pmemPolyDivide);
   	MALLOCOPT_RESTORE;
      dps.dwNum /= sizeof(fp);
      if (!dps.dwNum)
         continue;

      // and the deltas
      dps.fX = p1->x;
      dps.fXDelta = (p2->x - dps.fX) * yDelta;
      dps.fX += (fNewY - p1->y) * dps.fXDelta;

      dps.fZ = p1->z;
      dps.fZDelta = p2->z - dps.fZ;   // since will multiply this by alpha
      dps.fW = p1->w;
      dps.fWDelta = p2->w - dps.fW;   // since will multiply this by alpha

      // color
      DWORD dwc;
      if (!fSolid) {
         for (dwc = 0; dwc < 3; dwc++) {
            dps.afColor[dwc] = p1->aColor[dwc];
            dps.afColorDelta[dwc] = p2->aColor[dwc] - dps.afColor[dwc];   // since will multiply by alpha
         }
      }

      if (dwTextInfo & DHLTEXT_HV)
         for (j = 0; j < 2; j++) {
            dps.tpStart.hv[j] = p1->tp.hv[j];
            dps.tpDelta.hv[j] = p2->tp.hv[j] - p1->tp.hv[j];
         }
      if (dwTextInfo & DHLTEXT_XYZ)
         for (j = 0; j < 3; j++) {
            dps.tpStart.xyz[j] = p1->tp.xyz[j];
            dps.tpDelta.xyz[j] = p2->tp.xyz[j] - p1->tp.xyz[j];
         }

      // else, add
   	MALLOCOPT_OKTOMALLOC;
      plistPoly->Add (&dps);
   	MALLOCOPT_RESTORE;

   } // i

   // go back over the points
   DWORD dwpn;
   dwpn = plistPoly->Num();
   for (i = 0; i < dwpn; i++) {
      pd = (PDPS) plistPoly->Get(i);
      
      // it's OK to use the memory now
      pd->pafAlpha = (fp*) (((PBYTE) pmemPolyDivide->p) + pd->dwMemAlpha);
   }

   // find first scanline that need to worry about
   DWORD dwNumLine;
   DWORD dwNumActive;
   dwNumActive = 0;
   pd = (PDPS) plistPoly->Get(0);
   dwNumLine = plistPoly->Num();

   // mark those on the next-look scanline as active and then find the one below
   y = iNextLook;
   while (TRUE) {
      // if iNextLook is beyond then end of the screen then skip
      if (iNextLook >= (int) m_dwHeight)
         break;

      iNextLook = (int) m_dwHeight;
      for (i = 0; i < dwNumLine; i++) {
         if (pd[i].iState < 1)
            continue;

         // see if should make this active
         if (pd[i].iTop <= y) {
            pd[i].iState = 0;
            dwNumActive++;
            continue;
         }

         // check
         if (pd[i].iTop < iNextLook)
            iNextLook = pd[i].iTop;
      }

      // if there's nothing active then we're all done
      if (!dwNumActive)
         goto done;


      PDPS pLast, pCur;
      IHLEND left, right;
      DWORD k;
      // loop from current y down to iNextLook
      for (; y < iNextLook; y++) {
         // go through all the lines and see if any should be deactivated
         // also, while at it, do a bubble sort so X appears on the ledt
   bubble:
         pLast = NULL;
         for (i = 0; i < dwNumLine; i++) if (!pd[i].iState) {
            // consider deactivating it
            if (y > pd[i].iBottom) {
               pd[i].iState = -1;
               dwNumActive--;
               continue;
            }

            // if there's a last value, then check to make sure
            if (pLast && (pLast->fX > pd[i].fX)) {
               // bubble sort
               DPS   temp;
               temp = *pLast;
               *pLast = pd[i];
               pd[i] = temp;
               goto bubble;
            }
            pLast = &pd[i];
         }

         // if there aren't any active AND iNextLook >= end, then done
         if (!dwNumActive && (iNextLook >= (int) m_dwHeight))
            goto done;

#ifdef _DEBUG
         if (dwNumActive % 2)
            OutputDebugString ("Odd number of lines in polygon!\r\n");
#endif
         // when we get here we have an even number of lines, and they're
         // sorted. Therefore, draw them
         pLast = NULL;
         for (i = 0; i < dwNumLine; i++) if (!pd[i].iState) {
            if (!pLast) {
               pLast = &pd[i];
               continue;
            }
            pCur = &pd[i];

            // if get here have a pLast, and a new point, draw a line between them
            // while we're at it increment

            // left values
            fp fAlpha;
            fAlpha = pLast->pafAlpha[0];
            left.x = pLast->fX;
            pLast->fX += pLast->fXDelta;
            left.z = pLast->fZ + pLast->fZDelta * fAlpha;
            left.w = pLast->fW + pLast->fWDelta * fAlpha;
            if (!fSolid) {
               for (k = 0; k < 3; k++)
                  left.aColor[k] = (WORD) (pLast->afColor[k] + pLast->afColorDelta[k] * fAlpha);
            }
            if (dwTextInfo & DHLTEXT_HV)
               for (j = 0; j < 2; j++)
                  left.tp.hv[j] = pLast->tpStart.hv[j] + pLast->tpDelta.hv[j] * fAlpha;
            if (dwTextInfo & DHLTEXT_XYZ)
               for (j = 0; j < 3; j++)
                  left.tp.xyz[j] = pLast->tpStart.xyz[j] + pLast->tpDelta.xyz[j] * fAlpha;

            fAlpha = pCur->pafAlpha[0];
            right.x = pCur->fX;
            pCur->fX += pCur->fXDelta;
            right.z = pCur->fZ + pCur->fZDelta * fAlpha;
            right.w = pCur->fW + pCur->fWDelta * fAlpha;
            if (!fSolid) {
               for (k = 0; k < 3; k++)
                  right.aColor[k] = (WORD) (pCur->afColor[k] + pCur->afColorDelta[k] * fAlpha);
            }
            if (dwTextInfo & DHLTEXT_HV)
               for (j = 0; j < 2; j++)
                  right.tp.hv[j] = pCur->tpStart.hv[j] + pCur->tpDelta.hv[j] * fAlpha;
            if (dwTextInfo & DHLTEXT_XYZ)
               for (j = 0; j < 3; j++)
                  right.tp.xyz[j] = pCur->tpStart.xyz[j] + pCur->tpDelta.xyz[j] * fAlpha;

            // increaste index into alphas
            if (pLast->dwNum) {
               pLast->dwNum--;
               pLast->pafAlpha++;
            }
            if (pCur->dwNum) {
               pCur->dwNum--;
               pCur->pafAlpha++;
            }

            pLast = NULL;  // wipe it out for next time

            // potentially enter a critical seciton
            if (m_fCritSecInitialized && (y >= 0)) {
               DWORD dwCritSecWant = ((DWORD)y / IMAGESCANLINESPERCRITSEC) % IMAGECRITSEC;
               if (dwCritSecWant != dwCritSec) {
                  if (dwCritSec != (DWORD)-1)
                     LeaveCriticalSection (&m_aCritSec[dwCritSec]);
                  dwCritSec = dwCritSecWant;
                  EnterCriticalSection (&m_aCritSec[dwCritSec]);
               }
            }

            // draw the line
            DrawHorzLine (y, &left, &right, pm, fSolid, pTexture, fGlowScale, dwTextInfo, dwThread, fFinalRender);

         }
      }

   }

done:
   // free up the critical seciton
   if (dwCritSec != (DWORD)-1)
      LeaveCriticalSection (&m_aCritSec[dwCritSec]);
   
}


/*********************************************************************************
DrawPolygon - Draws a polygon, clipping against the screen border.

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PIHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  if fSolid then ppEnd[x]->wColor is ignored
   PIDMISC     pm - Miscellaneous info
                  if !fSolid the pm->wColor is ignored
   BOOL        fSolid - Set to TRUE if polygon is one solid color, FALSE if it's blended.
   PCTextureMap pTexture - If not NULL then use this texture map for color
   fp          fGlowScale - Amount to scale glow - affected by exposure
   BOOL        fTriangle - If TRUE, and the polygon isnt a triangle, then convert into one
   DWORD       dwThread - Thread rendering with, from 0 to MAXRAYTHREAD-1
   BOOL        fFinalRender - Set to TRUE if this is for the final render
returns
   none
*/

void CImage::DrawPolygon (DWORD dwNum, const PIHLEND ppEnd[], const PIDMISC pm, BOOL fSolid,
                          const PCTextureMapSocket pTexture, fp fGlowScale, BOOL fTriangle,
                          DWORD dwThread, BOOL fFinalRender)
{
   // BUGFIX - Made a DrawPolygonInt() method that should make the code run slightly faster

   if (dwNum < 3)
      return;

#ifdef _TIMERS
   gRenderStats.dwNumTri++;
#endif
   DWORD i;

   // BUGFIX: see if can trivially reject this because entirely hidden
   CPoint pMin, pMax;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         pMin.p[0] = min(pMin.p[0], ppEnd[i]->x);
         pMax.p[0] = max(pMax.p[0], ppEnd[i]->x);
         pMin.p[1] = min(pMin.p[1], ppEnd[i]->y);
         pMax.p[1] = max(pMax.p[1], ppEnd[i]->y);
         pMin.p[2] = min(pMin.p[2], ppEnd[i]->z);
      }
      else {
         pMax.p[0] = pMin.p[0] = ppEnd[i]->x;
         pMax.p[1] = pMin.p[1] = ppEnd[i]->y;
         pMin.p[2] = ppEnd[i]->z;
      }
   }
   RECT r;
   r.left = (int) floor (pMin.p[0]);
   r.right = (int) ceil (pMax.p[0]);
   r.top = (int) floor (pMin.p[1]);
   r.bottom = (int) ceil (pMax.p[1]);
   if (IsCompletelyCovered (&r, pMin.p[2])) {
#ifdef _TIMERS
      gRenderStats.dwTriCovered++;
#endif
      return;
   }

   // see if it's a subpixel polygon
   // dont worry about checking that max is on the integer because drawing goes
   // from min (inclusive) to max (exclusive)
   if (((r.left+1 == r.right) && (pMin.p[0] != (fp)r.left)) ||
      ((r.top+1 == r.bottom) && (pMin.p[1] != (fp)r.top)) )
      return;

   // will need to check for transparency per pixel
   DWORD dwTextInfo;
   if (pTexture) {
      dwTextInfo = (pTexture->MightBeTransparent(dwThread) & 0x02) ? (DHLTEXT_TRANSPARENT | DHLTEXT_PERPIXTRANS) : 0;
      DWORD dw = pTexture->DimensionalityQuery (dwThread);
      if (dw & 0x01)
         dwTextInfo |= DHLTEXT_HV;
      if (dw & 0x02)
         dwTextInfo |= DHLTEXT_XYZ;
   }
   else
      dwTextInfo = 0;

   if (fTriangle && (dwNum > 3)) {
      // if want it converted into triangles and it isn't, then convert it
      PIHLEND ap[3];

      DWORD i;
#if 0 // slower, but better - cant do this because messes up textures, and doesnt seem to work better
      IHLEND   iCenter;
      memset (&iCenter, 0, sizeof(iCenter));
      for (i = 0; i < dwNum; i++) {
         iCenter.aColor[0] += ppEnd[i]->aColor[0] / (WORD) dwNum;
         iCenter.aColor[1] += ppEnd[i]->aColor[1] / (WORD) dwNum;
         iCenter.aColor[2] += ppEnd[i]->aColor[2] / (WORD) dwNum;

         iCenter.tp.h += ppEnd[i]->tp.h;
         iCenter.tp.v += ppEnd[i]->tp.v;

         iCenter.x += ppEnd[i]->x;
         iCenter.y += ppEnd[i]->y;
         iCenter.z += ppEnd[i]->z;
         iCenter.w += ppEnd[i]->w;
      }
      iCenter.tp.h /= (fp)dwNum;
      iCenter.tp.v /= (fp)dwNum;
      iCenter.x /= (fp)dwNum;
      iCenter.y /= (fp)dwNum;
      iCenter.z /= (fp)dwNum;
      iCenter.w /= (fp)dwNum;

      for (i = 0; i < dwNum; i++) {
         ap[0] = ppEnd[i];
         ap[1] = ppEnd[(i+1)%dwNum];
         ap[2] = &iCenter;

         DrawPolygonInt (3, ap, pm, fSolid, pTexture, fGlowScale, dwThread, dwTextInfo);
      }
#else // doesnt work well because end up with crackes when intersect
      for (i = 0; i < (dwNum - 2); i++) {
         ap[0] = ppEnd[(dwNum - i / 2) % dwNum];
         ap[1] = ppEnd[(((i+1)/2) + 1) % dwNum];
         if (i % 2)
            ap[2] = ppEnd[dwNum - (i+1) / 2];
         else
            ap[2] = ppEnd[(i / 2 + 2) % dwNum];

         DrawPolygonInt (3, ap, pm, fSolid, pTexture, fGlowScale, dwThread, dwTextInfo, fFinalRender);
      }
#endif // 0
   }
   else
      DrawPolygonInt (dwNum, ppEnd, pm, fSolid, pTexture, fGlowScale, dwThread, dwTextInfo, fFinalRender);
   
}


/**************************************************************************
CImage::IsCompletelyCovered - Returns TRUE if all points in the rectangle have
z-buffer values less than iZ. Use this to determine if painting an object would
be useless because it's completely covered by other objects.

inputs
   RECT     *pRect - rectangle. This can be beyond the end of the CImage screen.
   fp   fZ - Depth in meters
returns
   BOOL - TRUE if it's completely coverd
*/
BOOL CImage::IsCompletelyCovered (const RECT *pRect, fp fZ)
{
   RECT  r;
   r.left = max(0,pRect->left);
   r.top = max(0,pRect->top);
   r.right = min((int)m_dwWidth-1, pRect->right);
   r.bottom = min((int)m_dwHeight-1, pRect->bottom);

   // if negative size then return true
   if ((r.right < r.left) || (r.bottom < r.top))
      return TRUE;

   int y;
   PIMAGEPIXEL pip;
   DWORD dwCount;
   for (y = r.top; y <= r.bottom; y++) {
      pip = Pixel(r.left, y);
      dwCount = (DWORD) (r.right - r.left + 1);
      for (; dwCount; dwCount--, pip++)
         if (pip->fZ > fZ)
            return FALSE;
   }

   return TRUE;
}

/********************************************************************************
CImage::InitBitmap - Initialized an image object from the bitmap, filling the image
with the bitmap.

inputs
   HINSTANCE   hInstance - Where to get resource
   DWORD       dwID - Bitmap resource
   fp      fZ - Default Z buffer depth to use, in meters
returns
   BOOL - true if succede
*/
BOOL CImage::InitBitmap (HINSTANCE hInstance, DWORD dwID, fp fZ)
{
   HBITMAP hBmp = LoadBitmap (hInstance, MAKEINTRESOURCE(dwID));
   if (!hBmp)
      return FALSE;

   BOOL fRet;
   fRet = Init (hBmp, fZ);
   DeleteObject (hBmp);
   return fRet;
}

/********************************************************************************
CImage::InitJPEG - Initialized an image object from the bitmap, filling the image
with the bitmap.

inputs
   HINSTANCE   hInstance - Where to get resource
   DWORD       dwID - Bitmap resource
   fp      fZ - Default Z buffer depth to use, in meters
returns
   BOOL - true if succede
*/
BOOL CImage::InitJPEG (HINSTANCE hInstance, DWORD dwID, fp fZ)
{
   HBITMAP hBmp = JPegToBitmap(dwID, hInstance);
   if (!hBmp)
      return FALSE;


   BOOL fRet;
   fRet = Init (hBmp, fZ);
   DeleteObject (hBmp);
   return fRet;
}

/********************************************************************************
CImage::Init - Reads in a file

inputs
   PWSTR       pszFile - file
   fp      fZ - Default Z buffer depth to use, in meters
   BOOL        fIngoreMegaFile - If TRUE then ignore the megafile. Default is FALSE.
returns
   BOOL - true if succede
*/
BOOL CImage::Init (PWSTR pszFile, fp fZ, BOOL fIgnoreMegaFile)
{
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);

   HBITMAP hBmp = JPegOrBitmapLoad(szTemp, fIgnoreMegaFile);
   if (!hBmp)
      return FALSE;


   BOOL fRet;
   fRet = Init (hBmp, fZ);
   DeleteObject (hBmp);
   return fRet;
}


/********************************************************************************
CImage::Init - Initialized an image object from the bitmap, filling the image
with the bitmap.

inputs
   HBITMAP     hBit - bitmap
   fp      fZ - Default Z buffer depth to use, in meters
returns
   BOOL - true if succede
*/
BOOL CImage::Init (HBITMAP hBit, fp fZ)
{
   // size
   CMem   mem;
   // get the size of the bitmap
   BITMAP   bm;
   GetObject (hBit, sizeof(bm), &bm);
   SIZE s;
   s.cx = bm.bmWidth;
   s.cy = bm.bmHeight;
   if (!Init ((DWORD)s.cx, (DWORD) s.cy))
      return FALSE;

   // convert to 32 bits
   BITMAPINFOHEADER bi;
   memset (&bi, 0, sizeof(bi));
   bi.biSize = sizeof(bi);
   bi.biHeight = -s.cy;
   bi.biWidth = s.cx;
   bi.biPlanes = 1;
   bi.biBitCount = 32;
   bi.biCompression = BI_RGB;
   HDC hDC;
   hDC = GetDC (NULL);
   if (!GetDIBits (hDC, hBit, 0, (DWORD) s.cy, NULL, (LPBITMAPINFO)&bi, DIB_RGB_COLORS)) {
      ReleaseDC (NULL, hDC);
      goto slowway;
   }

   // if it's not getting the right size or memory then do th eslow getpixel way
   if (!bi.biSizeImage || (bi.biPlanes != 1) || (bi.biBitCount != 32)) {
      ReleaseDC (NULL, hDC);
      goto slowway;
   }

   mem.Required (bi.biSizeImage);
   if (!GetDIBits (hDC, hBit, 0, (DWORD) s.cy, mem.p, (LPBITMAPINFO)&bi, DIB_RGB_COLORS)) {
      ReleaseDC (NULL, hDC);
      goto slowway;
   }

   ReleaseDC (NULL, hDC);

   // pull out
   DWORD dwSize, x;
   PIMAGEPIXEL pip;
   IMAGEPIXEL p;
   DWORD *pdw;
   memset (&p, 0, sizeof(p));
   p.fZ = fZ;
   dwSize = m_dwWidth * m_dwHeight;
   pdw = (DWORD*) mem.p;
   pip = Pixel(0,0);
   for (x = 0; x < dwSize; x++, pip++, pdw++) {
      p.wRed = Gamma(GetBValue ((COLORREF)(*pdw)));
      p.wGreen = Gamma(GetGValue ((COLORREF)(*pdw)));
      p.wBlue = Gamma(GetRValue ((COLORREF)(*pdw)));

      *pip = p;
   }


   return TRUE;

slowway:
   // IMPORTANT: I had planned on using getpixel, but I don't really seem to
   // need this. GetDIBits works perfectly well, converting everything to 32 bits
   // color, even if its a monochrome bitmap or whatnot

   return FALSE;
}



/********************************************************************************
CImage:: Overlay - Copy the image from the source (this object) to the destination
   CImage object. This code will do checking againt boundry conditions so as not
   to overwrite area.

inputs
   RECT        *prSrc - Source rectangle. If NULL use the entire source
   CImage      *pDestImage - To copy onto
   POINT       pDest - Where to copy the upper left corner onto
   BOOL        fOverlayZObject - If TRUE, copy over the Z depth information from
               the frist. If FALSE, leave the original Z information.
*/
BOOL CImage::Overlay (RECT *prSrc, CImage *pDestImage, POINT pDest, BOOL fOverlayZObject)
{
   RECT r;

   // dont go beyond source image
   if (prSrc) {
      r.left = max(0, prSrc->left);
      r.top = max(0, prSrc->top);
      r.right = min((int)m_dwWidth, prSrc->right);
      r.bottom = min((int)m_dwHeight, prSrc->bottom);
   }
   else {
      r.top = r.left = 0;
      r.right = (int) m_dwWidth;
      r.bottom = (int) m_dwHeight;
   }

   // don't go beyond dest image
   int iWidth, iHeight;
   iWidth = r.right - r.left;
   iHeight = r.bottom - r.top;
   if (pDest.x < 0) {
      iWidth += pDest.x;
      r.left -= pDest.x;
      pDest.x = 0;
   }
   if (pDest.y < 0) {
      iHeight += pDest.y;
      r.top -= pDest.y;
      pDest.y = 0;
   }
   if (pDest.x + iWidth > (int) pDestImage->m_dwWidth)
      iWidth = (int)pDestImage->m_dwWidth - pDest.x;
   if (pDest.y + iHeight > (int) pDestImage->m_dwHeight)
      iHeight = (int)pDestImage->m_dwHeight - pDest.y;
   if ((iWidth <= 0) || (iHeight <= 0))
      return TRUE;
   r.right = r.left + iWidth;
   r.bottom = r.top + iHeight;

   // copy
   int x, y;
   PIMAGEPIXEL ps, pd;
   for (y = 0; y < iHeight; y++) {
      ps = Pixel(r.left, r.top + y);
      pd = pDestImage->Pixel (pDest.x, pDest.y + y);

      for (x = iWidth; x; x--, ps++, pd++) {
         pd->wRed = ps->wRed;
         pd->wGreen = ps->wGreen;
         pd->wBlue = ps->wBlue;
         if (fOverlayZObject)
            pd->fZ = ps->fZ;
      }
   }

   return TRUE;
}




/********************************************************************************
CImage:: Blend - Copy the image from the source (this object) to the destination
   CImage object. This code will do checking againt boundry conditions so as not
   to overwrite area. The color from the two images are blended together.

inputs
   RECT        *prSrc - Source rectangle. If NULL use the entire source
   CImage      *pDestImage - To copy onto
   POINT       pDest - Where to copy the upper left corner onto
   WORD        wTransparency - Amount of transparency in the source image. 0 =
                  opaque, 0xffff is full transparency
*/
BOOL CImage::Blend (RECT *prSrc, CImage *pDestImage, POINT pDest, WORD wTransparency)
{
   RECT r;

   // dont go beyond source image
   if (prSrc) {
      r.left = max(0, prSrc->left);
      r.top = max(0, prSrc->top);
      r.right = min((int)m_dwWidth, prSrc->right);
      r.bottom = min((int)m_dwHeight, prSrc->bottom);
   }
   else {
      r.top = r.left = 0;
      r.right = (int) m_dwWidth;
      r.bottom = (int) m_dwHeight;
   }

   // don't go beyond dest image
   int iWidth, iHeight;
   iWidth = r.right - r.left;
   iHeight = r.bottom - r.top;
   if (pDest.x < 0) {
      iWidth += pDest.x;
      r.left -= pDest.x;
      pDest.x = 0;
   }
   if (pDest.y < 0) {
      iHeight += pDest.y;
      r.top -= pDest.y;
      pDest.y = 0;
   }
   if (pDest.x + iWidth > (int) pDestImage->m_dwWidth)
      iWidth = (int)pDestImage->m_dwWidth - pDest.x;
   if (pDest.y + iHeight > (int) pDestImage->m_dwHeight)
      iHeight = (int)pDestImage->m_dwHeight - pDest.y;
   if ((iWidth <= 0) || (iHeight <= 0))
      return TRUE;
   r.right = r.left + iWidth;
   r.bottom = r.top + iHeight;

   // copy
   int x, y;
   PIMAGEPIXEL ps, pd;
   DWORD dwSrc, dwDst;
   dwSrc = 0x10000 - wTransparency;
   dwDst = wTransparency;
   for (y = 0; y < iHeight; y++) {
      ps = Pixel(r.left, r.top + y);
      pd = pDestImage->Pixel (pDest.x, pDest.y + y);

      for (x = iWidth; x; x--, ps++, pd++) {
         pd->wRed = HIWORD(dwSrc * ps->wRed + dwDst * pd->wRed);
         pd->wGreen = HIWORD(dwSrc * ps->wGreen + dwDst * pd->wGreen);
         pd->wBlue = HIWORD(dwSrc * ps->wBlue + dwDst * pd->wBlue);
      }
   }

   return TRUE;
}





/********************************************************************************
CImage::MergeSelected - Merges this image (which is selected objects) in with
the main image (pDestImage), of unselected objects. In the merging: If the
selected object is in front, use that color. If it's behind, then blend the colors
with what's behind.

NOTE: This assumes that both images are the same size.

inputs
   CImage      *pDestImage - To merge into
   BOOL        fBlendSelAbove - If TRUE then blend the selection above the objects
               behind, else blend normal to z
*/
void CImage::MergeSelected (CImage *pDestImage, BOOL fBlendSelAbove)
{
   PIMAGEPIXEL pSrc, pDest;
   pSrc = Pixel(0,0);
   pDest = pDestImage->Pixel(0,0);
   DWORD dwNum, i;
   dwNum = Width() * Height();
   // BUGFIX - Was dwNum+1
   for (i = dwNum; i; i--, pSrc++, pDest++) {
      if (pSrc->fZ < pDest->fZ) {
         *pDest = *pSrc;
         continue;
      }
      if (!pSrc->dwID)
         continue;   // dont bother because nothing in the source

      // if blendabove then just copy over the extra info
      if (fBlendSelAbove) {
         pDest->dwID = pSrc->dwID;
         pDest->dwIDPart = pSrc->dwIDPart;
         pDest->fZ = pSrc->fZ;
      }

      // else, the source is more than the destination, so will
      // want to combine colors
      pDest->wRed = pSrc->wRed / 4 + pDest->wRed / 4 * 3;
      pDest->wGreen = pSrc->wGreen / 4 + pDest->wGreen / 4 * 3;
      pDest->wBlue = pSrc->wBlue / 4 + pDest->wBlue / 4 * 3;
   }
}



/********************************************************************************
CImage:: Merge - Merge the image from the source (this object) to the destination
   CImage object. This code will do checking againt boundry conditions so as not
   to overwrite area. If the source Z height is < the destination height then the
   pixel is transferred (object and all), otherwise the destination pixel is left.

inputs
   RECT        *prSrc - Source rectangle. If NULL use the entire source
   CImage      *pDestImage - To copy onto
   POINT       pDest - Where to copy the upper left corner onto
*/
BOOL CImage::Merge (RECT *prSrc, CImage *pDestImage, POINT pDest)
{
   RECT r;

   // dont go beyond source image
   if (prSrc) {
      r.left = max(0, prSrc->left);
      r.top = max(0, prSrc->top);
      r.right = min((int)m_dwWidth, prSrc->right);
      r.bottom = min((int)m_dwHeight, prSrc->bottom);
   }
   else {
      r.top = r.left = 0;
      r.right = (int) m_dwWidth;
      r.bottom = (int) m_dwHeight;
   }

   // don't go beyond dest image
   int iWidth, iHeight;
   iWidth = r.right - r.left;
   iHeight = r.bottom - r.top;
   if (pDest.x < 0) {
      iWidth += pDest.x;
      r.left -= pDest.x;
      pDest.x = 0;
   }
   if (pDest.y < 0) {
      iHeight += pDest.y;
      r.top -= pDest.y;
      pDest.y = 0;
   }
   if (pDest.x + iWidth > (int) pDestImage->m_dwWidth)
      iWidth = (int)pDestImage->m_dwWidth - pDest.x;
   if (pDest.y + iHeight > (int) pDestImage->m_dwHeight)
      iHeight = (int)pDestImage->m_dwHeight - pDest.y;
   if ((iWidth <= 0) || (iHeight <= 0))
      return TRUE;
   r.right = r.left + iWidth;
   r.bottom = r.top + iHeight;

   // copy
   int x, y;
   PIMAGEPIXEL ps, pd;

   for (y = 0; y < iHeight; y++) {
      ps = Pixel(r.left, r.top + y);
      pd = pDestImage->Pixel (pDest.x, pDest.y + y);

      for (x = iWidth; x; x--, ps++, pd++) {
         if (ps->fZ < pd->fZ)
            *pd = *ps;
      }
   }

   return TRUE;
}


/**************************************************************************
CImage::RBGlassesMerge - Merge two images, this one, and the image for
the non-diminant eye together into one image. It keeps the object and z-buffer
info for this image, assuming its the dominant eye.

inputs
   CImage      *pNonDom - Image for the non-domaint eye. This must be the same
                  size as the dominant image.
   BOOL        fNonDomRed - If TRUE then the pNonDom should be red and this
                  image blue. If FALSE, pNonDom is blue and this is red.
returns
   BOOL - TRUE if successful
*/
BOOL CImage::RBGlassesMerge (CImage *pNonDom, BOOL fNonDomRed)
{
   if ((Width() != pNonDom->Width()) || (Height() != pNonDom->Height()))
      return FALSE;

   DWORD dw;
   PIMAGEPIXEL pThis, pNon;
   dw = Width() * Height();
   pThis = Pixel(0, 0);
   pNon = pNonDom->Pixel(0, 0);

   for (; dw; dw--, pThis++, pNon++) {
      DWORD dwDomColor, dwNonColor;

      // calculate dominant color
      dwDomColor = (DWORD)pThis->wRed + (DWORD)pThis->wGreen + (DWORD)pThis->wBlue;
      dwDomColor /= 2;
      dwDomColor = min(0xffff, dwDomColor);

      // calculate non-dominant color
      dwNonColor = (DWORD)pNon->wRed + (DWORD)pNon->wGreen + (DWORD)pNon->wBlue;
      dwNonColor /= 2;
      dwNonColor = min(0xffff, dwNonColor);

      // merge the non-dominant in
      pThis->wRed = (WORD) (fNonDomRed ? dwNonColor : dwDomColor);
      pThis->wGreen = 0;
      pThis->wBlue = (WORD) (fNonDomRed ? dwDomColor : dwNonColor);
   }

   // done
   return TRUE;
}


/*********************************************************************************
CImage::Downsample - Downsample from the current image into a new image.

inputs
   PCImage     pDown - Image which is downsamples
   DWORD       dwAnti - Amount to downsample, from 1..N
returns
   none
*/
void CImage::Downsample (PCImage pDown, DWORD dwAnti)
{
   if (dwAnti <= 1) {
      pDown->Init (m_dwWidth, m_dwHeight);
      // copy over the bitmap
      memcpy (pDown->Pixel(0,0), Pixel(0,0), m_dwWidth * m_dwHeight * sizeof(IMAGEPIXEL));
      return;
   }

   // else
   pDown->Init (m_dwWidth / dwAnti, m_dwHeight / dwAnti);
   DWORD x,y;
   int xx, yy;
   PIMAGEPIXEL pipDown;
   PIMAGEPIXEL pipOrig;
   int iLeft, iRight;
   iLeft = 0;
   iRight = (int) dwAnti;
   if (dwAnti >= 3) {
      // take one extra pixel around
      iLeft--;
      iRight++;
   }
   for (y = 0; y < pDown->m_dwHeight; y++) for (x = 0; x < pDown->m_dwWidth; x++) {
      pipDown = pDown->Pixel(x,y);
      pipOrig = Pixel (x * dwAnti, y * dwAnti);
      *pipDown = *pipOrig; // so keep object ID, etc.

      // sum of rgb
      DWORD adw[3];
      adw[0] = adw[1] = adw[2] = 0;

      int iXMin, iXMax, iYMin, iYMax;
      iXMin = (int) x * (int) dwAnti;
      iXMax = min(iXMin + iRight, (int)m_dwWidth-1);
      iXMin = max(iXMin + iLeft, 0);
      iYMin = (int) y * (int) dwAnti;
      iYMax = min(iYMin + iRight, (int)m_dwHeight-1);
      iYMin = max(iYMin + iLeft, 0);

      for (yy = iYMin; yy < iYMax; yy++) {
         pipOrig = Pixel (iXMin, yy);
         for (xx = iXMin; xx < iXMax; xx++, pipOrig++) {
            adw[0] += (DWORD) pipOrig->wRed;
            adw[1] += (DWORD) pipOrig->wGreen;
            adw[2] += (DWORD) pipOrig->wBlue;
         }
      }

      // scale
      DWORD dwScale;
      dwScale = (DWORD) ((iXMax - iXMin) * (iYMax - iYMin));
      pipDown->wRed = (WORD)(adw[0] / dwScale);
      pipDown->wGreen = (WORD)(adw[1] / dwScale);
      pipDown->wBlue = (WORD)(adw[2] / dwScale);
   }
}



/**************************************************************************
CImage::TGDrawHorzLine - Texture generation function. Draws a horizontal
line filling it with a color blend AND/OR Z. Also has the option of
affecting specularity.

It also does modulo the image size.

inputs
   int      y - y scan line
   PIHLEND  pLeft, pRight - Left and right edges. Notes:
               pLeft must be left, and pRight must be right
               Ignore pXXXX.y. pXXXX.z are in units of pixels.
   DWORD    dwZFunc - 0 => dont change z, 1=> set Z to the specified value,
               2=> change Z to the min(curZ, line)
   BOOL     fApplyColor - If TRUE, change the color.
   BOOL     fApplySpecularity - If TRUE, change specularity component
   WORD     wSpecReflection - 0 to 0xffff for the brightness of the specular reflection.
               Only used of fApplySpecularity.
   WORD     wSpecDirection - 0 to 0xfff for the directionality of specular reflection.
               Only used if fApplySpecularity
returns
   none
*/
void CImage::TGDrawHorzLine (int y, PIHLEND pLeft, PIHLEND pRight, DWORD dwZFunc,
                             BOOL fApplyColor, BOOL fApplySpecularity,
                             WORD wSpecReflection, WORD wSpecDirection)
{
   // find out where it starts on the screen
   int   xStart, xEnd;
   fp fFloor;
   xStart = (int) ceil(pLeft->x);
   fFloor = floor(pRight->x);
   // if the right ends exactly on an integer, don't count it because count the
   // left if it's exactly on an integer. Counting both causes problems
   xEnd = (int) ((fFloor == pRight->x) ? (fFloor-1) : fFloor);
   //xEnd = (int) floor(pRight->x);

   // figure out z delta
   fp   xDelta = pRight->x - pLeft->x;
   if ((xEnd < xStart) || (xDelta <= 0))
      return;  // cant do this
   fp fSkip;
   fSkip = xStart - pLeft->x;

   // calculate starting values and deltas of all values
   fp fZ, fZDelta;
   fp afColor[3], afColorDelta[3];
   fZ = pLeft->z;
   fZDelta = (pRight->z - pLeft->z) / xDelta;
   fZ += fZDelta * fSkip;
   DWORD i;
   for (i = 0; i < 3; i++) {
      afColor[i] = pLeft->aColor[i];
      afColorDelta[i] = (pRight->aColor[i] - pLeft->aColor[i]) / xDelta;
      afColor[i] += afColorDelta[i] * fSkip;
   }

   // loop
   int x;
   PIMAGEPIXEL pip;
   for (x = xStart; x <= xEnd; x++) {
      pip = Pixel ((DWORD)myimod(x, (int)m_dwWidth), (DWORD)myimod(y, (int)m_dwHeight));

      if (dwZFunc) {
         fp   fVal;
         fVal = fZ;
         if ((dwZFunc == 1) || (fVal < pip->fZ))
            pip->fZ = fVal;
      }

      if (fApplyColor) {
         pip->wRed = (WORD) afColor[0];
         pip->wGreen = (WORD) afColor[1];
         pip->wBlue = (WORD) afColor[2];
      }

      if (fApplySpecularity) {
         pip->dwID = (((DWORD) wSpecReflection) << 16) | wSpecDirection;
      }

      // increment
      fZ += fZDelta;
      for (i = 0; i < 3; i++)
         afColor[i] += afColorDelta[i];
   }

}

/*********************************************************************************
CImage::TGDrawPolygonCurvedExtra - Draws a polygon for purposes of generating a texture
map. The polygon's z values are curved to it has a smooth edge - for rocks.

NOTE: THis also cuts off the corners so the rock is even smoother

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PIHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  if fSolid then ppEnd[x]->wColor is ignored
                  NOTE: The Z values may be changed
   WORD        wSpecReflection - 0 to 0xffff for the brightness of the specular reflection.
   WORD        wSpecDirection - 0 to 0xfff for the directionality of specular reflection.
   DWORD       dwCurvePix - Curve goes this many pixels into the polygon then maxes out
   fp          fZEdge - Z value at the edge, in pixels height
   fp          fZCenter - Curve in the center (assuming that dwCurvePix is mached)
   fp          fCutCorner - Value from 0 to .33, for how much of the corner to cut off.
                  0 means nothing
returns
   none
*/
void CImage::TGDrawPolygonCurvedExtra (DWORD dwNum, PIHLEND ppEnd[],
                            WORD wSpecReflection, WORD wSpecDirection,
                            DWORD dwCurvePix, fp fZEdge, fp fZCenter,
                            fp fCutCorner)
{
   fCutCorner = min(1.0/3.0, fCutCorner);

   if (fCutCorner <= 0) {
      // no corner cutting so easy
      TGDrawPolygonCurved (dwNum, ppEnd, wSpecReflection, wSpecDirection,
         dwCurvePix, fZEdge, fZCenter);
      return;
   }

   // allocate for all the extra points
   CListFixed lIHLEND, lPIHLEND;
   lIHLEND.Init (sizeof(IHLEND));
   lPIHLEND.Init (sizeof(PIHLEND));

   DWORD i;
   IHLEND hl;
   PIHLEND ph;
   lIHLEND.Required (dwNum*2);
   for (i = 0; i < dwNum; i++) {
      // first point
      hl = *(ppEnd[i]);
      ph = ppEnd[(i+1)%dwNum];
      hl.x = hl.x * (1.0 - fCutCorner) + ph->x * fCutCorner;
      hl.y = hl.y * (1.0 - fCutCorner) + ph->y * fCutCorner;
      hl.z = hl.z * (1.0 - fCutCorner) + ph->z * fCutCorner;
      lIHLEND.Add (&hl);

      // second point
      hl = *(ppEnd[i]);
      ph = ppEnd[(i+1)%dwNum];
      hl.x = hl.x * fCutCorner + ph->x * (1.0 - fCutCorner);
      hl.y = hl.y * fCutCorner + ph->y * (1.0 - fCutCorner);
      hl.z = hl.z * fCutCorner + ph->z * (1.0 - fCutCorner);
      lIHLEND.Add (&hl);
   }

   // fill in all the pointers
   lPIHLEND.Required (lIHLEND.Num());
   for (i = 0; i < lIHLEND.Num(); i++) {
      ph = (PIHLEND) lIHLEND.Get(i);
      lPIHLEND.Add (&ph);
   }
   
   // draw
   TGDrawPolygonCurved (lPIHLEND.Num(), (PIHLEND*) lPIHLEND.Get(0), wSpecReflection, wSpecDirection,
      dwCurvePix, fZEdge, fZCenter);
}

/*********************************************************************************
CImage::TGDrawPolygonCurved - Draws a polygon for purposes of generating a texture
map. The polygon's z values are curved to it has a smooth edge - for rocks.

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PIHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  if fSolid then ppEnd[x]->wColor is ignored
                  NOTE: The Z values are modified
   WORD        wSpecReflection - 0 to 0xffff for the brightness of the specular reflection.
   WORD        wSpecDirection - 0 to 0xfff for the directionality of specular reflection.
   DWORD       dwCurvePix - Curve goes this many pixels into the polygon then maxes out
   fp          fZEdge - Z value at the edge, in pixels height
   fp          fZCenter - Curve in the center (assuming that dwCurvePix is mached)
returns
   none
*/
void CImage::TGDrawPolygonCurved (DWORD dwNum, PIHLEND ppEnd[],
                            WORD wSpecReflection, WORD wSpecDirection,
                            DWORD dwCurvePix, fp fZEdge, fp fZCenter)
{
   dwCurvePix = max(1,dwCurvePix);

   DWORD i;
   int iMinX, iMaxX, iMinY, iMaxY;
   for (i = 0; i < dwNum; i++) {
      ppEnd[i]->z = 0;

      if (i) {
         iMinY = min(iMinY, (int)floor(ppEnd[i]->y));
         iMaxY = max(iMaxY, (int)ceil(ppEnd[i]->y));
         iMinX = min(iMinX, (int)floor(ppEnd[i]->x));
         iMaxX = max(iMaxX, (int)ceil(ppEnd[i]->x));
      }
      else {
         iMinY = (int)floor(ppEnd[i]->y);
         iMaxY = (int)ceil(ppEnd[i]->y);
         iMinX = (int)floor(ppEnd[i]->x);
         iMaxX = (int)ceil(ppEnd[i]->x);
      }
   }

   TGDrawPolygon (dwNum, ppEnd, 1, TRUE, TRUE, wSpecReflection, wSpecDirection);

   // loop over, figuring out how far from edge
   DWORD dwDist;
   int x,y, x2, y2;
   for (dwDist = 1; dwDist <= dwCurvePix; dwDist++) {
      BOOL fFound;
      fFound = FALSE;

      for (x = iMinX; x <= iMaxX; x++) for (y = iMinY; y <= iMaxY; y++) {
         PIMAGEPIXEL pip = Pixel(myimod (x, m_dwWidth), myimod(y, m_dwHeight));
         if (fabs(pip->fZ) > CLOSE)
            continue;   // only filling in zeros
         fFound = TRUE;

         // see what can find in surrounding pixels
         int iLowest;
         iLowest = 0x1000;
         for (x2 = x-1; x2 <= x+1; x2++) for (y2 = y-1; y2 <= y+1; y2++) {
            PIMAGEPIXEL pip2 = Pixel(myimod (x2, m_dwWidth), myimod(y2, m_dwHeight));
            if ((x2 == x) && (y2 == y))
               continue;   // dont count this
            if (fabs(pip2->fZ - 0) < CLOSE)
               continue;   // not yet calculated
            iLowest = min(iLowest, (int) pip2->fZ);
         }

         // if there isn't a lowest then skip or set depending upon dwDist
         if (iLowest >= (int) dwCurvePix) {
            if (dwDist == dwCurvePix) {
               pip->fZ = dwCurvePix;
            }
            continue;
         }

         // if the lowest <= 0 then pretend it's 0
         iLowest = max(0,iLowest) + 1;

         // only lookin for lowest == dwDst
         if ((DWORD)iLowest != dwDist)
            continue;

         // else set it
         pip->fZ = dwDist;
      }  // over x and y

      // if no more left to change then break
      if (!fFound)
         break;
   } // over distances

   // precalculate sine values
   CListFixed  lSin;
   lSin.Init (sizeof(fp));
   fp fVal;
   lSin.Required (dwCurvePix+1);
   for (i = 0; i <= dwCurvePix; i++) {
      fVal = ((sin((fp) i / (fp)dwCurvePix * PI / 2) * (fZCenter - fZEdge) + fZEdge));
      lSin.Add (&fVal);
   }
   fp *pf;
   pf = (fp*) lSin.Get(0);

   // fill in
   for (x = iMinX; x <= iMaxX; x++) for (y = iMinY; y <= iMaxY; y++) {
      PIMAGEPIXEL pip = Pixel(myimod (x, m_dwWidth), myimod(y, m_dwHeight));
      if ((pip->fZ >= 0) && ((int)pip->fZ <= (int) dwCurvePix))
         pip->fZ = pf[(DWORD) pip->fZ];
   }

   // another loop to average values so don't get bumps
   for (x = iMinX; x <= iMaxX; x++) for (y = iMinY; y <= iMaxY; y++) {
      PIMAGEPIXEL pip = Pixel(myimod (x, m_dwWidth), myimod(y, m_dwHeight));
      if (pip->fZ <= CLOSE)
         continue;

      fp   fSum;
      int iDivide;
      fSum = 0;
      iDivide = 0;
      for (x2 = x-1; x2 <= x+1; x2++) for (y2 = y-1; y2 <= y+1; y2++) {
         PIMAGEPIXEL pip2 = Pixel(myimod (x2, m_dwWidth), myimod(y2, m_dwHeight));
         if (pip2->fZ <= CLOSE)
            continue;
         fSum += pip2->fZ;
         iDivide++;
      }
      pip->fZ = fSum / (fp)iDivide;
   }

   // done
}


/*********************************************************************************
CImage::TGDrawPolygon - Draws a polygon for purposes of generating a texture
map. These polygons are modulo the borders of the image, and have options
about affecting z values or not

inputs
   DWORD       dwNum - Number of vertices. Must be 3 or more.
   PIHLEND     ppEnd[] - Array of pointers to endpoints of the polygon. dwNum elements.
                  if fSolid then ppEnd[x]->wColor is ignored
   DWORD    dwZFunc - 0 => dont change z, 1=> set Z to the specified value,
               2=> change Z to the min(curZ, line)
   BOOL     fApplyColor - If TRUE, change the color.
   BOOL     fApplySpecularity - If TRUE, change specularity component
   WORD     wSpecReflection - 0 to 0xffff for the brightness of the specular reflection.
               Only used of fApplySpecularity.
   WORD     wSpecDirection - 0 to 0xfff for the directionality of specular reflection.
               Only used if fApplySpecularity
   BOOL     fModulo - If TRUE then draw modulo Y, else clip y below/above top
returns
   none
*/
void CImage::TGDrawPolygon (DWORD dwNum, PIHLEND ppEnd[], DWORD dwZFunc,
                            BOOL fApplyColor, BOOL fApplySpecularity,
                            WORD wSpecReflection, WORD wSpecDirection,
                            BOOL fModulo)
{
   if (dwNum < 3)
      return;

   DWORD dwThread = 0;  // assume 0
   PCListFixed plistPoly = &m_alistPoly[dwThread];

   // initialize the polygon stuff, filling in the lines
   int   iNextLook;
   iNextLook = (int) 32767;
   plistPoly->Init (sizeof(DPS));
   plistPoly->Clear();
   PDPS pd;
   DWORD i;
   int y;

   // first pass, create structures for all the sides of the triangle
   // and then subdivide enough for perspective
   PIHLEND p1, p2, t;
   for (i = 0; i < dwNum; i++) {
      DPS dps;
      p1 = ppEnd[i];
      p2 = ppEnd[((i+1) < dwNum) ? (i+1) : 0];

      // p1 should be higher (lower y) than p2
      if (p1->y > p2->y) {
         t = p1;
         p1 = p2;
         p2 = t;
      }

      // find the delta
      fp yDelta;
      yDelta = p2->y - p1->y;
      if (yDelta <= 0.0)
         continue;
      yDelta = 1.0 / yDelta;

      // clip in Y direction?
      fp   fNewY;
      fNewY = ceil(p1->y);

      // convert the y range to integers
      dps.iTop = (int) fNewY;
      fp fAlpha;
      fAlpha = floor(p2->y);
      // if the bottom ends exactly on an integer, don't count it because count the
      // top if it's exactly on an integer. Counting both causes problems
      dps.iBottom = (int) ((fAlpha == p2->y) ? (fAlpha-1) : fAlpha);

      // if bottom < top then nothing here to draw so skip
      if (dps.iBottom < dps.iTop) {
         continue;
      }
      // if this is the first line so far then remember where things start
      if (dps.iTop < iNextLook)
         iNextLook = dps.iTop;

      // how much does x and the color change?
      dps.iState = 1;   // not yet done

      // and the deltas
      dps.fX = p1->x;
      dps.fXDelta = (p2->x - dps.fX) * yDelta;
      dps.fX += (fNewY - p1->y) * dps.fXDelta;

      dps.fZ = p1->z;
      dps.fZDelta = (p2->z - dps.fZ) * yDelta;   // since will multiply this by alpha

      // color
      DWORD dwc;
      for (dwc = 0; dwc < 3; dwc++) {
         dps.afColor[dwc] = p1->aColor[dwc];
         dps.afColorDelta[dwc] = (p2->aColor[dwc] - dps.afColor[dwc]) * yDelta;   // since will multiply by alpha
      }

      // else, add
      plistPoly->Add (&dps);

   }

   // find first scanline that need to worry about
   DWORD dwNumLine;
   DWORD dwNumActive;
   dwNumActive = 0;
   pd = (PDPS) plistPoly->Get(0);
   dwNumLine = plistPoly->Num();

   // mark those on the next-look scanline as active and then find the one below
   y = iNextLook;
   while (TRUE) {
      // if iNextLook is beyond then end of the screen then skip
      if (iNextLook >= (int) 32767)
         break;

      iNextLook = (int) 32767;
      for (i = 0; i < dwNumLine; i++) {
         if (pd[i].iState < 1)
            continue;

         // see if should make this active
         if (pd[i].iTop <= y) {
            pd[i].iState = 0;
            dwNumActive++;
            continue;
         }

         // check
         if (pd[i].iTop < iNextLook)
            iNextLook = pd[i].iTop;
      }

      // if there's nothing active then we're all done
      if (!dwNumActive)
         return;


      PDPS pLast, pCur;
      IHLEND left, right;
      DWORD k;
      // loop from current y down to iNextLook
      for (; y < iNextLook; y++) {
         // go through all the lines and see if any should be deactivated
         // also, while at it, do a bubble sort so X appears on the ledt
   bubble:
         pLast = NULL;
         for (i = 0; i < dwNumLine; i++) if (!pd[i].iState) {
            // consider deactivating it
            if (y > pd[i].iBottom) {
               pd[i].iState = -1;
               dwNumActive--;
               continue;
            }

            // if there's a last value, then check to make sure
            if (pLast && (pLast->fX > pd[i].fX)) {
               // bubble sort
               DPS   temp;
               temp = *pLast;
               *pLast = pd[i];
               pd[i] = temp;
               goto bubble;
            }
            pLast = &pd[i];
         }

         // if there aren't any active AND iNextLook >= end, then done
         if (!dwNumActive && (iNextLook >= (int) 32767))
            return;

#ifdef _DEBUG
         if (dwNumActive % 2)
            OutputDebugString ("Odd number of lines in polygon!\r\n");
#endif
         // when we get here we have an even number of lines, and they're
         // sorted. Therefore, draw them
         pLast = NULL;
         for (i = 0; i < dwNumLine; i++) if (!pd[i].iState) {
            if (!pLast) {
               pLast = &pd[i];
               continue;
            }
            pCur = &pd[i];

            // if get here have a pLast, and a new point, draw a line between them
            // while we're at it increment

            // left values
            left.x = pLast->fX;
            pLast->fX += pLast->fXDelta;
            left.z = pLast->fZ;
            pLast->fZ += pLast->fZDelta;
            for (k = 0; k < 3; k++) {
               left.aColor[k] = (WORD) pLast->afColor[k];
               pLast->afColor[k] += pLast->afColorDelta[k];
            }

            right.x = pCur->fX;
            pCur->fX += pCur->fXDelta;
            right.z = pCur->fZ;
            pCur->fZ += pCur->fZDelta;
            for (k = 0; k < 3; k++) {
               right.aColor[k] = (WORD) pCur->afColor[k];
               pCur->afColor[k] += pCur->afColorDelta[k];
            }

            pLast = NULL;  // wipe it out for next time

            // consider skipping if no modulo
            if (!fModulo && ((y < 0) || (y >= (int)m_dwHeight)))
               continue;

            // draw the line
            TGDrawHorzLine (y, &left, &right, dwZFunc, fApplyColor, fApplySpecularity,
               wSpecReflection, wSpecDirection);

         }
      }

   }
   
}

/********************************************************************************
CImage:: TGMerge - For use in generating textures.
   Merge the image from the source (this object) to the destination
   CImage object. All positions are modulo the width and height of the image.
   If the source Z height is > the destination height then the
   pixel is transferred (object and all), otherwise the destination pixel is left.

inputs
   RECT        *prSrc - Source rectangle. If NULL use the entire source
   CImage      *pDestImage - To copy onto
   POINT       pDest - Where to copy the upper left corner onto
*/
BOOL CImage::TGMerge (RECT *prSrc, CImage *pDestImage, POINT pDest)
{
   // IMPORTANT: No yet tested

   RECT r;

   // dont go beyond source image
   if (prSrc) {
      r = *prSrc;
   }
   else {
      r.top = r.left = 0;
      r.right = (int) m_dwWidth;
      r.bottom = (int) m_dwHeight;
   }

   // don't go beyond dest image
   int iWidth, iHeight;
   iWidth = r.right - r.left;
   iHeight = r.bottom - r.top;

   // copy
   int x, y;
   PIMAGEPIXEL ps, pd;

   for (y = 0; y < iHeight; y++) for (x = 0; x < iWidth; x++) {
      ps = Pixel(myimod(r.left + x, (int)m_dwWidth), myimod(r.top + y, (int)m_dwHeight));
      pd = pDestImage->Pixel (myimod(pDest.x + x, (int)pDestImage->Width()),
         myimod(pDest.y + y, (int)pDestImage->Height()) );

      if (ps->fZ > pd->fZ)
         *pd = *ps;
   }

   return TRUE;
}


/********************************************************************************
CImage:: TGMergeRotate - For use in generating textures.
   Merge the image from the source (this object) to the destination
   CImage object. All positions are modulo the width and height of the image.
   If the source Z height is > the destination height then the
   pixel is transferred (object and all), otherwise the destination pixel is left.

  NOTE: This rotates the image left 90 degrees (counterclockwise) around the UL corner.
  So UL corner of source becomes the LL corner on the destination.

inputs
   RECT        *prSrc - Source rectangle. If NULL use the entire source
   CImage      *pDestImage - To copy onto
   POINT       pDest - Where to copy the upper left corner onto
*/
BOOL CImage::TGMergeRotate (RECT *prSrc, CImage *pDestImage, POINT pDest)
{
   // IMPORTANT: No yet tested

   RECT r;

   // dont go beyond source image
   if (prSrc) {
      r = *prSrc;
   }
   else {
      r.top = r.left = 0;
      r.right = (int) m_dwWidth;
      r.bottom = (int) m_dwHeight;
   }

   // don't go beyond dest image
   int iWidth, iHeight;
   iWidth = r.right - r.left;
   iHeight = r.bottom - r.top;

   // copy
   int x, y;
   PIMAGEPIXEL ps, pd;

   for (y = 0; y < iHeight; y++) for (x = 0; x < iWidth; x++) {
      ps = Pixel(myimod(r.left + x, (int)m_dwWidth), myimod(r.top + y, (int)m_dwHeight));
      pd = pDestImage->Pixel (myimod(pDest.x + y, (int)pDestImage->Width()),
         myimod(pDest.y - x, (int)pDestImage->Height()) );

      if (ps->fZ > pd->fZ)
         *pd = *ps;
   }

   return TRUE;
}


/********************************************************************************
CImage::TGMarbling - Generates a noise grid and then applies marbling.

inputs
   int      iNoiseSize - Lowest wavelength of noise. Will include higher wavelengths
               until < 2 pixels
   fp       fDecay - Each time double the frequency, multiply scale by this. Should
            be around .5.
   fp       fNoiseContibution - Multiply sum of noise values x fNoiseConribution x 2PI.
            Should be from 0 to 1
   DWORD    dwNumLevels - Numver of levels of marble, which are mapped from -1 to 1,
               the result of sin
   COLORREF *pacLevel - Color for each level. dwNum Levels values.
   BOOL     fNoiseMax - If TRUE, the noise will max out.
returns
   none
*/
void CImage::TGMarbling (int iNoiseSize, fp fDecay, fp fNoiseContribution,
                         DWORD dwNumLevels, COLORREF *pacLevel, BOOL fNoiseMax)
{
   if (dwNumLevels < 2)
      return;  // cant handle this

   // figure out some important bits...
   CListFixed  lPNoise;
   DWORD i, dwCur;
   PCNoise2D pNoise;
   lPNoise.Init (sizeof(PCNoise2D));
   for (dwCur = (DWORD) iNoiseSize; dwCur > 2; dwCur /= 2) {
      pNoise = new CNoise2D;
      if (!pNoise)
         break;
      if (!pNoise->Init (max(m_dwWidth/dwCur,1), max(m_dwHeight/dwCur,1)))
         break;
      lPNoise.Add (&pNoise);
   }

   fp fOffset;
   fOffset = fNoiseMax ? 0 : randf(0, 2 * PI);

   // loop and overwrite base color
   DWORD x,y, z;
   fp f, h, v;

   for (y = 0; y < m_dwHeight; y++) {
      for (x = 0; x < m_dwWidth; x++) {
         PIMAGEPIXEL pip = Pixel(x,y);

         f = 0;
         h = (fp)x / (fp) m_dwWidth;
         v = (fp)y / (fp) m_dwHeight;

         for (z = 0; z < lPNoise.Num(); z++) {
            pNoise = *((PCNoise2D*) lPNoise.Get(z));
            f += fabs(pNoise->Value (h, v) * pow((fp)fDecay, (fp)z));
         }
         f *= fNoiseContribution * 2 * PI;
         // BUGFIX - Have to take out sin or pattern doenst repeat f += fSin * (fp)x + fCos * (fp)y;

         if (fNoiseMax) {
            f = max(0,f);
            f = min(PI/2,f);
         }
         f = sin (f + fOffset);

         // convert to color number
         f = (f + 1) / 2.0 * (fp)(dwNumLevels - 1);
         f = max(0,f);
         DWORD dwC;
         dwC = (DWORD) f;
         dwC = min(dwNumLevels-2, f);
         f = (f - (fp) dwC); // so from 0 to 1
         f = min(1,f);

         // average
         WORD aw1[3], aw2[3];
         Gamma (pacLevel[dwC], aw1);
         Gamma (pacLevel[dwC+1], aw2);
         for (z = 0; z < 3; z++)
            (&pip->wRed)[z] = (WORD) ((fp)aw1[z] * (1.0 - f) + (fp)aw2[z] * f);
      }  // x
   } // y

   for (i = 0; i < lPNoise.Num(); i++) {
      pNoise = *((PCNoise2D*) lPNoise.Get(i));
      delete pNoise;
   }
}

/********************************************************************************
CImage::TGNoise - Generate a noise grid of the specified size and then use
it to apply noise (in z buffer or colors) to the image. Used for texture map
creation. The noise generated is modulo x and y so the image can wrap around.

inputs
   int      iNoiseXSize, iNoiseYSize - Number of pixels in x and y for the
               noise block. The "wavelength" of the noise is basically this
               number of pixels. NOTE: The wavelength will be adjusted up/down
               slightly so that an integer number fits into the widrh and height
               of the image.
   fp   fZDeltaMax, fZDeltaMin -  Min and max to increase (or maybe decrease) Z
               by based on the noise. This is the range of elevation changes.
   COLORREF cMax, cMin - Color to use at maximum and minimum.
   fp   fTransMax, fTransMin - Transparency of cMax and cMin when overlaid over
               the existing colors. 1.0 = completely transparent. 0 = opaque
returns
   none
*/
// BUGFIX - Changed the whole noise calculations so it works properly
// BUGFIX - No optimizations on this since optimizer causes bug that causes noise
// function to mess up
// #pragma optimize ("", off)
void CImage::TGNoise (int iNoiseXSize, int iNoiseYSize, fp fZDeltaMax, fp fZDeltaMin,
                      COLORREF cMax, COLORREF cMin, fp fTransMax, fp fTransMin)
{
   // figure out size of grid to make an integer multiple
   DWORD dwGridX, dwGridY;
   CNoise2D Noise;
   fp fNoiseXSize, fNoiseYSize;
   iNoiseXSize = max(2,iNoiseXSize);
   iNoiseYSize = max(2,iNoiseYSize);
   dwGridX = (m_dwWidth + (DWORD)iNoiseXSize/2) / (DWORD)iNoiseXSize;
   dwGridY = (m_dwHeight + (DWORD)iNoiseYSize/2) / (DWORD)iNoiseYSize;
   dwGridX = max(1, dwGridX); // was 4
   dwGridY = max(1, dwGridY); //  as 4
   fNoiseXSize = (fp) m_dwWidth / (fp) dwGridX;
   fNoiseYSize = (fp) m_dwHeight / (fp) dwGridY;
   if (!Noise.Init (dwGridX, dwGridY))
      return;

   WORD awMin[3], awMax[3];
   Gamma (cMin, awMin);
   Gamma (cMax, awMax);
   // loop
   PIMAGEPIXEL pip;
   DWORD x,y, z;
   fp f;
   for (y = 0; y < m_dwHeight; y++) {

      for (x = 0; x < m_dwWidth; x++) {
         f = Noise.Value ((fp)x / (fp) m_dwWidth, (fp)y / (fp) m_dwHeight);

         //f = sin((x+y) / (fp) m_dwWidth * 2 * PI * 2); test

         // this is rougly a value between -1 and 1, although it may go slightly
         // over or under. Change to a value between 0 and 1
         f = (f + 1) / 2.0;


         // change the pixel
         pip = Pixel(x, y);

         // z value
         fp fZ;
         fZ = fZDeltaMin + f * (fZDeltaMax - fZDeltaMin);
         pip->fZ += fZ;


         // color
         fp afInterp[3];
         fp fTrans;
         fTrans = fTransMin + f * (fTransMax - fTransMin);
         fTrans = min(1,fTrans);
         fTrans = max(0,fTrans);
         for (z = 0; z < 3; z++) {
            afInterp[z] = (fp) awMin[z] + ((fp)awMax[z] - (fp)awMin[z]) * f;
            afInterp[z] = min(0xffff, afInterp[z]);
            afInterp[z] = max(0, afInterp[z]);
         }
         pip->wRed = (WORD) ((fp) pip->wRed * fTrans + (1.0 - fTrans) * afInterp[0]);
         pip->wGreen = (WORD) ((fp) pip->wGreen * fTrans + (1.0 - fTrans) * afInterp[1]);
         pip->wBlue = (WORD) ((fp) pip->wBlue * fTrans + (1.0 - fTrans) * afInterp[2]);
      }
   }
}
// #pragma optimize ("", on)


#ifdef DEADCODE
/********************************************************************************
NoiseToValue - Called by TGNoise to calcualte ax + by + c.

inputs
   PCPoint     pGrid - Pointer to the beginning of the grid
   DWORD       dwGridX, dwGridY - X and Y width of the grid
   DWORD       dwX, dwY - Item in the grid
   fp      x, y - x and y, from 0 to 1 (over course of screen)

returns
   fp
*/
#define  GRID(x,y)   (myimod((int)(x),(int)dwGridX) + myimod((int)(y),(int)dwGridY) * dwGridX)
static fp NoiseToValue (PCPoint pGrid, DWORD dwGridX, DWORD dwGridY,
                            int dwX, int dwY, fp x, fp y)
{
   // pointer to the point
   dwX = myimod(dwX, dwGridX);
   dwY = myimod(dwY, dwGridY);
   PCPoint p = pGrid + GRID(dwX, dwY);

   // fing out where this point is from 0..1
   fp h,v;
   h = (fp)dwX / (fp)dwGridX;
   v = (fp)dwY / (fp)dwGridY;

   // distance
   x -= h;
   y -= v;

#if 1
   // already know that x is between 0, and 1, find distance
   x = fabs(myfmod(x+.5,1) - .5);
   y = fabs(myfmod(y+.5,1) - .5);

   // calculate values for for corners
   DWORD cx, cy;
   fp afRet[2][2];
   for (cx = 0; cx < 2; cx++) for (cy = 0; cy < 2; cy++) {
      fp fx, fy;
      fx = cx ? 1 : 0;
      fy = cy ? 1 : 0;
      afRet[cx][cy] = p->p[0] * fx * (fp)dwGridX + p->p[1] * fy * (fp)dwGridY + p->p[2];
   }
   fp fLeft, fRight;
   fp fScaleY, fScaleX;
   //fScaleY = cos (y * PI/2);
   //fScaleY *= fScaleY;
   //fScaleX = cos (x * PI/2);
   //fScaleX *= fScaleX;
   fScaleX = x;
   fScaleY = y;
   fLeft = afRet[0][0] * (1.0 - fScaleY) + afRet[0][1] * fScaleY;
   fRight = afRet[1][0] * (1.0 - fScaleY) + afRet[1][1] * fScaleY;

   fp fRet;
   fRet = fLeft * (1.0 - fScaleX) + fRight * fScaleX;
   return fRet;
#endif 
#if 0 // old code
   // if more than .5 then subtract number so all values are from -.5 to .5
   if (x > .5)
      x -= 1;
   if (y > .5)
      y -= 1;
   if (x <= -.5)
      x += 1;
   if (y <= -.5)
      y += 1;
   // NOTE: This doesnt work well because creates lines through texture

   // return p->p[0] * x + p->p[1] * y + p->p[2];
   return p->p[0] * x * (fp)dwGridX + p->p[1] * y * (fp)dwGridY + p->p[2];
      // BUGFIX - Multiply by dwGridX and dwGridY so that p[0] and p[1] have more effect
#endif // 0
}




/********************************************************************************
CImage::TGNoise - Generate a noise grid of the specified size and then use
it to apply noise (in z buffer or colors) to the image. Used for texture map
creation. The noise generated is modulo x and y so the image can wrap around.

inputs
   int      iNoiseXSize, iNoiseYSize - Number of pixels in x and y for the
               noise block. The "wavelength" of the noise is basically this
               number of pixels. NOTE: The wavelength will be adjusted up/down
               slightly so that an integer number fits into the widrh and height
               of the image.
   fp   fZDeltaMax, fZDeltaMin -  Min and max to increase (or maybe decrease) Z
               by based on the noise. This is the range of elevation changes.
   COLORREF cMax, cMin - Color to use at maximum and minimum.
   fp   fTransMax, fTransMin - Transparency of cMax and cMin when overlaid over
               the existing colors. 1.0 = completely transparent. 0 = opaque
returns
   none
*/
void CImage::TGNoise (int iNoiseXSize, int iNoiseYSize, fp fZDeltaMax, fp fZDeltaMin,
                      COLORREF cMax, COLORREF cMin, fp fTransMax, fp fTransMin)
{
   // figure out size of grid to make an integer multiple
   DWORD dwGridX, dwGridY;
   fp fNoiseXSize, fNoiseYSize;
   iNoiseXSize = max(2,iNoiseXSize);
   iNoiseYSize = max(2,iNoiseYSize);
   dwGridX = (m_dwWidth + (DWORD)iNoiseXSize/2) / (DWORD)iNoiseXSize;
   dwGridY = (m_dwHeight + (DWORD)iNoiseYSize/2) / (DWORD)iNoiseYSize;
   dwGridX = max(1, dwGridX); // was 4
   dwGridY = max(1, dwGridY); //  as 4
   fNoiseXSize = (fp) m_dwWidth / (fp) dwGridX;
   fNoiseYSize = (fp) m_dwHeight / (fp) dwGridY;

   // allocate for the grid and calculate
   CMem mGrid;
   if (!mGrid.Required (dwGridX * dwGridY * sizeof(CPoint)))
      return;
   PCPoint paGrid;
   paGrid = (PCPoint) mGrid.p;
   
   // fill all the points in
   DWORD x,y,z;
   PCPoint p;
   fp f;
   for (y = 0; y < dwGridY; y++) for (x = 0; x < dwGridX; x++) {
      p = paGrid + GRID(x,y);
      for (z = 0; z < 3; z++) {
         p->p[z] = randf(-1,1);
      }

#if 0
      // readjust the 3rd parameter so if the point is given relative to
      // 0,0 it comes out correct
      f = p->p[0] * (fp) x / (fp) dwGridX +
         p->p[1] * (fp) y / (fp) dwGridY;
      p->p[2] = p->p[2] - f;
#endif //0
   }

   WORD awMin[3], awMax[3];
   Gamma (cMin, awMin);
   Gamma (cMax, awMax);
   // loop
   PIMAGEPIXEL pip;
   for (y = 0; y < m_dwHeight; y++) {

#if 0
      FILE *file;
      file = NULL;
      if (y == 2) {
         file = fopen ("c:\\test.txt", "wt");
      }
#endif
      for (x = 0; x < m_dwWidth; x++) {
         // which grid are we in
         fp gX, gY;
         DWORD dwLeft, dwRight, dwTop, dwBottom;
         gX = (fp) x / fNoiseXSize;
         gY = (fp) y / fNoiseYSize;
         dwLeft = (DWORD) gX;
         dwRight = (dwLeft+1) % dwGridX;
         dwTop = (DWORD) gY;
         dwBottom = (dwTop + 1) % dwGridY;
         gX -= dwLeft;  // value from 0 to 1
         gY -= dwTop;   // value from 0 to 1

         // position relative to entire map
         fp fh, fv;
         fh = (fp) x / (fp) m_dwWidth;
         fv = (fp) y / (fp) m_dwHeight;

#if 0
         // do a cubic interpolation to generate 4 points above and below
         CPoint pNew[4];
         int i;
         for (i = 0; i < 4; i++)
            HermiteCubic (gX,
               &paGrid[GRID((int)dwLeft-1, (int)dwTop-1+i)],
               &paGrid[GRID((int)dwLeft-0, (int)dwTop-1+i)],
               &paGrid[GRID((int)dwLeft+1, (int)dwTop-1+i)],
               &paGrid[GRID((int)dwLeft+2, (int)dwTop-1+i)],
               &pNew[i]
               );

         CPoint pFinal;
         HermiteCubic (gY, &pNew[0], &pNew[1], &pNew[2], &pNew[3], &pFinal);

         // get noise at fh,fv AND fh+1,fv, fh,fv+1, fh.fv+1, and interpolate
         // need to do this so that wraps around properly
         fp f00,f10,f01,f11;

         f00 = NoiseToValue (&pFinal, fh, fv);
         f01 = NoiseToValue (&pFinal, fh, 1-fv);
         f10 = NoiseToValue (&pFinal, 1-fh, fv);
         f11 = NoiseToValue (&pFinal, 1-fh, 1-fv);

         // interpolate those
         fp ft, fb;
         ft = (1.0 - fh) * f00 + fh * f10;
         fb = (1.0 - fh) * f01 + fh * f11;
         f = (1.0 - fv) * ft + fv * ft;
#endif

         // do a cubic interpolation to generate 4 points above and below
         fp af[4];
         int i;
         for (i = 0; i < 4; i++)
            af[i] = HermiteCubic (gX,
               NoiseToValue(paGrid, dwGridX, dwGridY, (int)dwLeft-1, (int)dwTop-1+i, fh, fv),
               NoiseToValue(paGrid, dwGridX, dwGridY, (int)dwLeft-0, (int)dwTop-1+i, fh, fv),
               NoiseToValue(paGrid, dwGridX, dwGridY, (int)dwLeft+1, (int)dwTop-1+i, fh, fv),
               NoiseToValue(paGrid, dwGridX, dwGridY, (int)dwLeft+2, (int)dwTop-1+i, fh, fv)
               );
         f = HermiteCubic (gY, af[0], af[1], af[2], af[3]);

         //f = sin((x+y) / (fp) m_dwWidth * 2 * PI * 2); test

   #if 0 // dead code
         // get the value at the 4 points
         fp fUL, fUR, fLL, fLR;
         fUL = paGrid[GRID(dwLeft,dwTop)].p[0] * fh +
            paGrid[GRID(dwLeft,dwTop)].p[1] * fv +
            paGrid[GRID(dwLeft,dwTop)].p[2];
         fUR = paGrid[GRID(dwRight,dwTop)].p[0] * fh +
            paGrid[GRID(dwRight,dwTop)].p[1] * fv +
            paGrid[GRID(dwRight,dwTop)].p[2];
         fLL = paGrid[GRID(dwLeft,dwBottom)].p[0] * fh +
            paGrid[GRID(dwLeft,dwBottom)].p[1] * fv +
            paGrid[GRID(dwLeft,dwBottom)].p[2];
         fLR = paGrid[GRID(dwRight,dwBottom)].p[0] * fh +
            paGrid[GRID(dwRight,dwBottom)].p[1] * fv +
            paGrid[GRID(dwRight,dwBottom)].p[2];

         // interpolate on top and bottom
         fp fT, fB;
         fT = (1.0 - gX) * fUL + gX * fUR;
         fB = (1.0 - gX) * fLL + gX * fLR;
      
         // interpolate between those tow
         f = (1.0 - gY) * fT + gY * fB;
   #endif //0

         // this is rougly a value between -1 and 1, although it may go slightly
         // over or under. Change to a value between 0 and 1
         f = (f + 1) / 2.0;

#if 0
         if (file)
            fprintf (file, "%g\n", (double)f);
#endif

         // change the pixel
         pip = Pixel(x, y);

         // z value
         fp fZ;
         int iZ;
         fZ = fZDeltaMin + f * (fZDeltaMax - fZDeltaMin);
         iZ = (int) (fZ * (fp) 0x10000);
         pip->iZ += iZ;


         // color
         fp afInterp[3];
         fp fTrans;
         fTrans = fTransMin + f * (fTransMax - fTransMin);
         fTrans = min(1,fTrans);
         fTrans = max(0,fTrans);
         for (z = 0; z < 3; z++) {
            afInterp[z] = (fp) awMin[z] + ((fp)awMax[z] - (fp)awMin[z]) * f;
            afInterp[z] = min(0xffff, afInterp[z]);
            afInterp[z] = max(0, afInterp[z]);
         }
         pip->wRed = (WORD) ((fp) pip->wRed * fTrans + (1.0 - fTrans) * afInterp[0]);
         pip->wGreen = (WORD) ((fp) pip->wGreen * fTrans + (1.0 - fTrans) * afInterp[1]);
         pip->wBlue = (WORD) ((fp) pip->wBlue * fTrans + (1.0 - fTrans) * afInterp[2]);
      }
#if 0
      if (file)
         fclose (file);
#endif
   }
}
#endif // DEADCODE


/********************************************************************************
CImage::TGBumpMapApply - Uses the Z values (0x10000 = 1 pixel in height) to determine
the normals at each pixel. This is used for diffuse and specular shading.
Specular shading intensities come from HIWORD(pip->dwID) = strength,
LOWORD(pip->dwID) = directionality.

inputs
   fp      fAmbient - Ambient light strength. Ambient + Light = 1.0 (approx)
   fp      fLight - Strength of the light.
   PCPoint     pLight - Pointer to the light vector. X is to the right, postiive Y
                           is top, and Z is up. If NULL default choisen.
   PCPoint     pViewer - Pointer to the viewer. If NULL default is chosen.
returns
   none
*/
void CImage::TGBumpMapApply (fp fAmbient, fp fLight, PCPoint pLight, PCPoint pViewer)
{
   // generate light and viwer vectors as normalized
   CPoint pL, pView;
   if (pLight)
      pL = *pLight;
   else {
      pL.Zero();
      pL.p[0] = -1;
      pL.p[1] = 1;
      pL.p[2] = 1;
   }
   if (pViewer)
      pView = *pViewer;
   else {
      pView.Zero();
      pView.p[0] = 0;
      pView.p[1] = -1;
      pView.p[2] = 2;
   }
   pL.Normalize();
   pView.Normalize();

   // calculate reflection vector
   CPoint pR;
   // BUGFIX - Change to use the Half vector like in call shading
   //pR.Copy (&pN);
   //pR.Scale (2.0 * pN.DotProd(&pL));
   //pR.Subtract (&pL);
   pR.Add (&pL, &pView);
   pR.Normalize();

   // loop through all the points
   DWORD x,y;
   for (y = 0; y < m_dwHeight; y++) for (x = 0; x < m_dwWidth; x++) {
      // find this pixel, and the ones above, below, left, right
      PIMAGEPIXEL pip, pAbove, pBelow, pLeft, pRight;
      pip = Pixel(x,y);
      pLeft = Pixel(x ? (x-1) : (m_dwWidth-1), y);
      pRight = Pixel((x+1) % m_dwWidth, y);
      pAbove = Pixel(x, y ? (y-1) : (m_dwHeight-1));
      pBelow = Pixel(x, (y+1) % m_dwHeight);

      // find the normals
      CPoint pH, pV, pN;
      pH.Zero();
      pV.Zero();
      pH.p[0] = 2;
      pH.p[2] = (pRight->fZ - pLeft->fZ);
      pV.p[1] = 2;
      pV.p[2] = (pAbove->fZ - pBelow->fZ);
      pH.Normalize();
      pV.Normalize();
      pN.CrossProd (&pH, &pV);

      // dot-product normal with light direction for ambient light intensity
      fp fDiffuse;
      fDiffuse = pN.DotProd (&pL);
      fDiffuse = max(0,fDiffuse);

      // and R dot V to dinf the intensity
      fp fSpecular;
      fSpecular = pR.DotProd (&pN); // BUGFIX - Half way vector - pR.DotProd (&pView);
      if (fSpecular > 0) {
         fSpecular = pow (fSpecular, (fp) LOWORD(pip->dwID) / (fp) 100) *
            (fp) HIWORD(pip->dwID); // dont divide so goes from 0 to 0xffff / (fp)0x10000;
         fSpecular *= fLight;
      }
      else
         fSpecular = 0;

      // total light
      fp fTotal;
      fTotal = fAmbient + fDiffuse * fLight;
      DWORD i;
      for (i = 0; i < 3; i++) {
         fp fc;
         fc = *((&pip->wRed) + i);

         fc *= fTotal;
         fc += fSpecular;  // note that specular light is white
         fc = max(0,fc);
         fc = min(0xffff,fc);
         *((&pip->wRed)+i) = (WORD) fc;
      }
   }
}


/********************************************************************************
CImage::TGColorByZ - Used for texture map generation. Looks at the Z value
and changes the color based on that.

inputs
   fp   fZMin, fZMax - Definition of two Z levels, in units of pixels
   COLORREF cZMin, cZMax - Color to use if depth is zMin or zMax respectively.
                           If less than zMin then use cZMin solely, between
                           fZMin and fZMax then interpolate, and above fZMax
                           use cZMax solely
   fp   fTransMin, fTransMax - Transparency of color at fZMin and fZMax.
                           1.0 is completely transparent. 0 is opaque
returns
   none
*/
void CImage::TGColorByZ (fp fZMin, fp fZMax, COLORREF cZMin, COLORREF cZMax,
                         fp fTransMin, fp fTransMax)
{
   // IMPORTANT: No yet tested
   WORD awMin[3], awMax[3];
   Gamma (cZMin, awMin);
   Gamma (cZMax, awMax);
   fTransMin = min(1,fTransMin);
   fTransMin = max(0,fTransMin);
   fTransMax = min(1,fTransMax);
   fTransMax = max(0,fTransMax);

   // loop
   DWORD x,y,z;
   PIMAGEPIXEL pip;
   WORD wc[3];
   fp fAlpha;
   for (y = 0; y < m_dwHeight; y++) for (x = 0; x < m_dwWidth; x++) {
      pip = Pixel(x,y);

      // look at Z and calculate what the color should be
      fp fZ;
      fZ = pip->fZ;
      if (fZ <= fZMin) {
         memcpy (wc, awMin, sizeof(wc));
         fAlpha = 0;
      }
      else if (fZ >= fZMax) {
         memcpy (wc, awMax, sizeof(wc));
         fAlpha = 1;
      }
      else {
         fAlpha = (fZ - fZMin) / (fZMax - fZMin);
         for (z = 0; z < 3; z++) {
            wc[z] = (WORD)((1.0 - fAlpha) * awMin[z] + fAlpha * awMax[z]);
         }
      }

      // what's the transparency
      fp ft;
      ft = fTransMin + fAlpha * (fTransMax - fTransMin);

      // blend with existing colors
      for (z = 0; z < 3; z++) {
         fp fc;
         fc = (1.0 - ft) * (*((&pip->wRed) + z)) + ft * wc[z];
         fc = max(0,fc);
         fc = min(0xffff,fc);
         *((&pip->wRed)+z) = (WORD) fc;
      }
   }
}


/***************************************************************************************
CImage::ToIHLEND - Fills in an IHLEND structure for use with textures.

inputps
   PIHLEND     p - Fill this in
   fp      x - x
   fp      y - y
   fp      z - z
   COLORREF    c - color
returns
   PIHLEND - The same value as p
*/
PIHLEND CImage::ToIHLEND (PIHLEND p, fp x, fp y, fp z, COLORREF c)
{
   memset (p, 0, sizeof(*p));
   p->x = x;
   p->y = y;
   p->z = z;
   p->w = 1;
   Gamma (c, p->aColor);

   return p;
}

/*********************************************************************************
CImage::TGDrawQuad - Draws a quadrilateral for purposes of generating a texture
map. These polygons are modulo the borders of the image, and have options
about affecting z values or not

inputs
   PIHLEND     p1, p2, p3, p4 - 4 points in clockwise order
   DWORD    dwZFunc - 0 => dont change z, 1=> set Z to the specified value,
               2=> change Z to the min(curZ, line)
   BOOL     fApplyColor - If TRUE, change the color.
   BOOL     fApplySpecularity - If TRUE, change specularity component
   WORD     wSpecReflection - 0 to 0xffff for the brightness of the specular reflection.
               Only used of fApplySpecularity.
   WORD     wSpecDirection - 0 to 0xfff for the directionality of specular reflection.
               Only used if fApplySpecularity
returns
   none
*/

void CImage::TGDrawQuad (PIHLEND p1,PIHLEND p2,PIHLEND p3,PIHLEND p4, DWORD dwZFunc,
                            BOOL fApplyColor, BOOL fApplySpecularity,
                            WORD wSpecReflection, WORD wSpecDirection)
{
   PIHLEND pp[4];
   pp[0] = p1;
   pp[1] = p2;
   pp[2] = p3;
   pp[3] = p4;

   TGDrawPolygon (4, pp, dwZFunc, fApplyColor, fApplySpecularity, wSpecReflection, wSpecDirection);
}

/*********************************************************************************
CImage::TGFilterZ - Filters the Z values of this image.

inputs
   DWORD       dwFilterSize - 0 = only one pixel (no filter), 1=3 pixels across, 2=5 pixels across, etc.
   fp      fZInc - Increment Z by this amount while you're at it
returns
   none
*/
void CImage::TGFilterZ (DWORD dwFilterSize, fp fZInc)
{
   fp fInc = fZInc;

   // allocate enough space for the filter - be cheap and just create a new image
   CImage iNew;
   iNew.Init (m_dwWidth, m_dwHeight);

   // filter
   int x,y,x2,y2;
   for (y = 0; y < (int)m_dwHeight; y++) for (x=0; x < (int)m_dwWidth; x++) {
      // filter size
      double fTotal;
      fTotal = 0;

      for (y2 = -(int)dwFilterSize; y2 <= (int)dwFilterSize; y2++)
         for (x2 = -(int)dwFilterSize; x2 <= (int)dwFilterSize; x2++)
            fTotal += Pixel(myimod(x+x2, m_dwWidth),myimod(y+y2,m_dwHeight))->fZ;

      // average out
      fTotal /= (double) ((dwFilterSize*2+1) * (dwFilterSize*2+1));

      iNew.Pixel(x,y)->fZ = fTotal + fInc;
   }

   // readjust height
   for (y = 0; y < (int)m_dwHeight; y++) for (x=0; x < (int)m_dwWidth; x++) {
      PIMAGEPIXEL pip = Pixel(x,y);
      pip->fZ = iNew.Pixel(x,y)->fZ;
   }
}



/*********************************************************************************
CImage::TGDirtAndPaint - Applies "dirt/paint" to the surface. It does a box filter
on the Z-height to smooth things out. The new Z values are then the max of the
old and the filtered - since dirt and paint settle in the low areas. The color
of the dirt/paint is then blended in with the existing color in some ratio based
on the amount of dirt added.

inputs
   DWORD       dwFilterSize - 0 = only one pixel (no filter), 1=3 pixels across, 2=5 pixels across, etc.
   COLORREF    cDirt - Color of the dirt or the paint.
   fp      fTransNone - Transparency if nothing there's no change in height
   fp      fTransA - Transparency if there's a change in height denoted by A
   fp      fAHeight - Height (in pixels) of A. For example, dirt is completely opaque
               at .1 pixel (and 1 pixel=10mm, => 1mm of dirt is opaque) then use
               fAHeight = .1, fTransA = 0.
   WORD        wSpecReflection - 0 to 0xffff for the brightness of the specular reflection.
               Specularity of the dirt/paint.
   WORD        wSpecDirection - 0 to 0xfff for the directionality of specular reflection.
               Specularity of the dirt/paint
returns
   none
*/
void CImage::TGDirtAndPaint (DWORD dwFilterSize, COLORREF cDirt, fp fTransNone,
                             fp fTransA, fp fAHeight,
                             WORD wSpecReflection, WORD wSpecDirection)
{
   // color of dirt
   WORD wc[3];
   Gamma (cDirt, wc);

   PCImage pNew;
   pNew = Clone ();
   if (!pNew)
      return;
   pNew->TGFilterZ (dwFilterSize);

   // filter
   int x,y,x2,y2;
   for (y = 0; y < (int)m_dwHeight; y++) for (x=0; x < (int)m_dwWidth; x++) {
      // filter size
      double fTotal;
      fTotal = 0;

      for (y2 = -(int)dwFilterSize; y2 <= (int)dwFilterSize; y2++)
         for (x2 = -(int)dwFilterSize; x2 <= (int)dwFilterSize; x2++)
            fTotal += Pixel(myimod(x+x2, m_dwWidth),myimod(y+y2,m_dwHeight))->fZ;

      // average out
      fTotal /= (double) ((dwFilterSize*2+1) * (dwFilterSize*2+1));

      pNew->Pixel(x,y)->fZ = fTotal;
   }

   // readjust height
   for (y = 0; y < (int)m_dwHeight; y++) for (x=0; x < (int)m_dwWidth; x++) {
      PIMAGEPIXEL pip = Pixel(x,y);
      fp fMax;
      fMax = max(pip->fZ, pNew->Pixel(x,y)->fZ);

      // amount of dirt added
      fp fDelta;
      fDelta = (fMax - pip->fZ);

      // convert to height that wanted
      fDelta /= fAHeight;
      fDelta = min(1,fDelta);
      fDelta = max(0,fDelta);

      // figure out transparency
      fp ft;
      ft = fTransNone + (fTransA - fTransNone) * fDelta;
      ft = min(1,ft);
      ft = max(0,ft);

      // merge in the colors
      // blend with existing colors
      DWORD z;
      for (z = 0; z < 3; z++) {
         fp fc;
         fc = ft * (*((&pip->wRed) + z)) + (1.0 - ft) * wc[z];
         fc = max(0,fc);
         fc = min(0xffff,fc);
         *((&pip->wRed)+z) = (WORD) fc;
      }

      // specularity merge
      WORD wSpec, wDir;
      wSpec = HIWORD(pip->dwID);
      wDir = LOWORD(pip->dwID);
      wSpec = (WORD)(ft * wSpec + (1.0 - ft) * wSpecReflection);
      wDir = (WORD)(ft * wDir + (1.0 - ft) * wSpecDirection);
      pip->dwID = ((DWORD)wSpec << 16) | wDir;
   }

   delete pNew;
}


/*********************************************************************************
CImage::TGMergeForChipping - Merge a source image (this) onto the destionation
(must be the same size as this one), but it only changes Z. If the Z value of the source is less
than the Z value of this's pixel then use the lower Z value in the dest. Used to create
texture maps and simulate chips in bricks and tiles. The chip map will have a lower
value than this.

inputs
   CImage      *pDestImage - Destination image.
   COLORREF    cChip - Color of the chipped away surface
   fp      fColorTrans - Transparency when applying cChip to chipped away surface.
                  0 = none, 1 = full
   WORD        wSpecReflection - 0 to 0xffff for the brightness of the specular reflection.
               Specularity of the chipped away surface.
   WORD        wSpecDirection - 0 to 0xfff for the directionality of specular reflection.
               Specularity of the chipped away surface

returns
   none
*/
void CImage::TGMergeForChipping (CImage *pDest, COLORREF cChip, fp fColorTrans,
                                 WORD wSpecReflection, WORD wSpecDirection)
{
   if ((m_dwWidth != pDest->m_dwWidth) || (m_dwHeight != pDest->m_dwHeight))
      return;

   WORD wc[3];
   Gamma (cChip, wc);

   PIMAGEPIXEL pSrc, pDst;
   pSrc = Pixel(0,0);
   pDst = pDest->Pixel(0,0);
   DWORD dwNum;
   dwNum = m_dwWidth * m_dwHeight;
   for (; dwNum; dwNum--, pSrc++, pDst++) {
      if (pDst->fZ <= pSrc->fZ)
         continue;

      // copy over
      pDst->fZ = pSrc->fZ;

      // merge in the colors
      // blend with existing colors
      DWORD z;
      fp ft;
      ft = fColorTrans;
      for (z = 0; z < 3; z++) {
         fp fc;
         fc = ft * (*((&pDst->wRed) + z)) + (1.0 - ft) * wc[z];
         fc = max(0,fc);
         fc = min(0xffff,fc);
         *((&pDst->wRed)+z) = (WORD) fc;
      }
      
      // specularity
      pDst->dwID = ((DWORD)wSpecReflection << 16) | wSpecDirection;
   }
}





#if 0
/********************************************************************************
HermiteCubic - Cubic interpolation based on 4 points, p1..p4

inputs
   fp      t - value from 0 to 1, at 0, is at point p2, and at 1 is at point p3.
   PCPoint      p1, p2, p3, p4 - Four points. Use the first 3 values.
   CPoint      pNew - Filled with the new value.
returns
   none
*/
void HermiteCubic (fp t, PCPoint p1, PCPoint p2, PCPoint p3, PCPoint p4, PCPoint pNew)
{
   // derivatives at the points
   CPoint fd2, fd3;
   fd2.Subtract (p3, p1);
   fd2.Scale(.5);
   fd3.Subtract (p4, p2);
   fd3.Scale (.5);

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
      pNew->p[i] = m2 * p2->p[i] + m3 * p3->p[i] + md2 * fd2.p[i] + md3 * fd3.p[i];

   // done
   //return (2 * t3 - 3 * t2 + 1) * fp2 +
   //   (-2 * t3 + 3 * t2) * fp3 +
   //   (t3 - 2 * t2 + t) * fd2 +
   //   (t3 - t2) * fd3;
}


/********************************************************************************
NoiseToValue - Called by TGNoise to calcualte ax + by + c.

inputs
   PCPoint     p - Point the a, b, c
   fp      x, y - x and y, from 0 to 1 (over course of screen)
returns
   fp
*/
static fp NoiseToValue (PCPoint p, fp x, fp y)
{
   return p->p[0] * x + p->p[1] * y + p->p[2];
}
#endif // 0


#define  GRID(x,y)   (myimod((int)(x),(int)dwGridX) + myimod((int)(y),(int)dwGridY) * dwGridX)

#ifdef SLOWNOISE
/********************************************************************************
NoiseToValue - Called by TGNoise to return a, b, and c.

inputs
   PCPoint     pGrid - Pointer to the beginning of the grid
   DWORD       dwGridX, dwGridY - X and Y width of the grid
   int         dwX, dwY - Item in the grid, may excede the dwGridX and dwGridY
   PCPoint     pVals - Fills in p[0]=a, p[1]=b,p[2]=c, ax + by + c
returns
   none
*/
static void NoiseToValue (PCPoint pGrid, DWORD dwGridX, DWORD dwGridY,
                            int dwX, int dwY, PCPoint pVals)
{
   int iModX, iModY;
   iModX = myimod(dwX, dwGridX);
   iModY = myimod(dwY, dwGridY);
   PCPoint p = pGrid + GRID(iModX, iModY);

   pVals->p[0] = p->p[0] * (fp) dwGridX;
   pVals->p[1] = p->p[1] * (fp) dwGridY;
   pVals->p[2] = p->p[2] -
      ((fp) dwX / (fp) dwGridX) * pVals->p[0] -
      ((fp) dwY / (fp) dwGridY) * pVals->p[1];
}
#else // !SLOWNOISE
/********************************************************************************
NoiseXToValue - Called by to return a hermite-interpolated X given the location

inputs
   float       pGrid - Pointer to the beginning of the grid
   DWORD       dwGridX, dwGridY - X and Y width of the grid
   fp          fX - Location of X, from 0..dwGridX-EPSILON (cannot exceed!)
   DWORD       dwY - Location in Y, from 0..dwGridY-1 (cannot exceed)
returns
   float - Value
*/
static float NoiseXToValue (float *pGrid, DWORD dwGridX, DWORD dwGridY,
                            fp fX, DWORD dwY)
{
   DWORD dwX = (DWORD)fX;
   fX -= (DWORD)dwX;

   // find the line in the grid
   pGrid += (dwY * dwGridX);

   // get values
   return (float) HermiteCubic (fX, pGrid[(dwX+dwGridX-1)%dwGridX],
      pGrid[dwX], pGrid[(dwX+1)%dwGridX], pGrid[(dwX+2)%dwGridX]);
}
#endif // SLOWNOISE

/********************************************************************************
CNoise2D::Constructor and destructo */
CNoise2D::CNoise2D (void)
{
   m_dwGridX = m_dwGridY = 0;
   m_pRand = NULL;
}
CNoise2D::~CNoise2D (void)
{
   // intentionally left blank
}

/********************************************************************************
CNoise2D::Init - Creates a new set of random values (using the current random seed)
for the noise field.

inputs
   DWORD       dwGridX, dwGridY - Width and height
returns
   BOOL - TRUE if success
*/
BOOL CNoise2D::Init (DWORD dwGridX, DWORD dwGridY)
{
   if (!dwGridX || !dwGridY)
      return FALSE;
   // allocate for the grid and calculate
   if (!m_memData.Required (dwGridX * dwGridY * sizeof(*m_pRand)))
      return FALSE;
   m_dwGridX = dwGridX;
   m_dwGridY = dwGridY;
#ifdef SLOWNOISE
   m_pRand = (PCPoint) m_memData.p;
   PCPoint p;
#else
   m_pRand = (float*) m_memData.p;
   float* p;
#endif SLOWNOISE
   
   // fill all the points in
   DWORD x,y;//,z;
   for (y = 0; y < dwGridY; y++) for (x = 0; x < dwGridX; x++) {
      p = m_pRand + GRID(x,y);
#ifdef SLOWNOISE
      for (z = 0; z < 3; z++) {
         p->p[z] = randf(-1,1);
      }
#else
      p[0] = randf(-1,1);
#endif

   }

   return TRUE;
}

/********************************************************************************
CNoise2D::Value - Given and h and v, running from 0 to 1, or beyond that, in which
case it's module, resturns a value from the nosse grid, which roughly runs
from -1 to 1.

inputs
   fp       h,v - Location in noise grid
returns
   fp - Value
*/
fp CNoise2D::Value (fp h, fp v)
{
   if (!m_pRand)
      return 0;

   h = myfmod(h,1);
   v = myfmod(v,1);

   // which grid are we in
   fp gY, gX;
   DWORD dwTop;
   gX = h * (fp) m_dwGridX;
   gY = v * (fp) m_dwGridY;
   dwTop = (DWORD) gY;
   gY -= dwTop;   // value from 0 to 1

#ifdef SLOWNOISE
   fp f;
   DWORD dwLeft;
   dwLeft = (DWORD) gX;
   gX -= dwLeft;  // value from 0 to 1

   // do a cubic interpolation to generate 4 points above and below
   CPoint aUD[4];
   int i, j, k;
   for (k = 0; k < 4; k++) {
      for (i = 0; i < 4; i++) {
         CPoint ap[4];
         // get the values across
         NoiseToValue (m_pRand, m_dwGridX, m_dwGridY, (int)dwLeft-1+i, (int)dwTop-1+k, &ap[i]);

         // interpolate them
         for (j = 0; j < 3; j++)
            aUD[k].p[j] = HermiteCubic (gX,
               ap[0].p[j], ap[1].p[j], ap[2].p[j], ap[3].p[j]);
      }
   }

   // cubic down
   CPoint pParam;
   for (j = 0; j < 3; j++)
      pParam.p[j] = HermiteCubic (gY,
               aUD[0].p[j], aUD[1].p[j], aUD[2].p[j], aUD[3].p[j]);
   f = pParam.p[0] * h + pParam.p[1] * v + pParam.p[2];

   return f;
#else //SLOWNOISE
   DWORD i;
   fp af[4];
   for (i = 0; i < 4; i++)
      af[i] = NoiseXToValue (m_pRand, m_dwGridX, m_dwGridY, gX,
         (dwTop + m_dwGridY - 1 + i) % m_dwGridY);
   return HermiteCubic (gY, af[0], af[1], af[2], af[3]);
#endif // SLOWNOISE
}



/********************************************************************************
CNoise3D::Constructor and destructor
*/
CNoise3D::CNoise3D (void)
{
   m_dwGridZ = 0;
   m_lPCNoise2D.Init (sizeof(PCNoise2D));
}

CNoise3D::~CNoise3D (void)
{
   PCNoise2D *ppn = (PCNoise2D*)m_lPCNoise2D.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCNoise2D.Num(); i++)
      delete ppn[i];
}

/********************************************************************************
CNoise3D::Init - Initializes the 3d noise object to the given xyz grid size.
*/
BOOL CNoise3D::Init (DWORD dwGridX, DWORD dwGridY, DWORD dwGridZ)
{
   if (m_dwGridZ || !dwGridZ)
      return FALSE;  // either initialized already or no z value

   m_dwGridZ = dwGridZ;
   DWORD i;
   for (i = 0; i < dwGridZ; i++) {
      PCNoise2D pNew = new CNoise2D;
      if (!pNew)
         return FALSE;
      if (!pNew->Init (dwGridX, dwGridY))
         return FALSE;
      m_lPCNoise2D.Add (&pNew);
   } // i

   return TRUE;
}

/********************************************************************************
CNoise3D::Value - Given and xyz, running from 0 to 1, or beyond that, in which
case it's module, resturns a value from the nosse grid, which roughly runs
from -1 to 1.

inputs
   fp       x,y,z - Location in noise grid
returns
   fp - Value
*/
fp CNoise3D::Value (fp x, fp y, fp z)
{
   if (!m_dwGridZ)
      return 0;

   z = myfmod(z,1);

   // which grid are we in
   fp gZ;
   DWORD dwTop;
   gZ = z * (fp) m_dwGridZ;
   dwTop = (DWORD) gZ;
   gZ -= dwTop;   // value from 0 to 1

   // noise list
   PCNoise2D *ppn = (PCNoise2D*) m_lPCNoise2D.Get(0);

   // do a cubic interpolation to generate 4 points above and below
   fp af[4];
   DWORD i;
   for (i = 0; i < 4; i++)
      af[i] = ppn[(dwTop + m_dwGridZ - 1 + i)%m_dwGridZ]->Value (x, y);

   return HermiteCubic (gZ, af[0], af[1], af[2], af[3]);
}



/**************************************************************************************
DrawTextOnImage - Draws text on an image.

inputs
   PCImage        pImage - Image
   int            iHorz - Horizontal location, -1 = left, 0 = center, 1 = right
   int            iVert - Veritcal location, -1 = top, 0 = center, 1 = right
   HFONT          hFont - Font to use, or NULL to create one
   PWSTR          psz - String to write
returns
   BOOL - TRUE if success
*/
BOOL DrawTextOnImage (PCImage pImage, int iHorz, int iVert, HFONT hFont, PWSTR psz)
{
   // convert to ansi
   CMem mem;
   if (!mem.Required ((wcslen(psz)+1)*2))
      return FALSE;
   char *psza = (char*)mem.p;
   WideCharToMultiByte (CP_ACP, 0, psz, -1, psza, (DWORD)mem.m_dwAllocated, 0, 0);

   // how large?
   RECT r;
   r.top = r.left = 0;
   r.right = (int)pImage->Width();
   r.bottom = (int)pImage->Height();

   // find the image size and create bitmap
   HDC hDCDesk = GetDC (GetDesktopWindow ());
   HDC hDC = CreateCompatibleDC (hDCDesk);
   HBITMAP hBit = CreateCompatibleBitmap (hDCDesk, r.right, r.bottom);
   ReleaseDC (GetDesktopWindow(), hDCDesk);
   SelectObject (hDC, hBit);

   // fill with black
   FillRect (hDC, &r, (HBRUSH) GetStockObject (BLACK_BRUSH));

   // create font?
   HFONT hFontCreate = NULL;
   if (!hFont) {
      LOGFONT lf;
      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = -12;   // 10 pixels high MulDiv(iPointSize, EscGetDeviceCaps(hDC, LOGPIXELSY), 72); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfWeight = FW_NORMAL;   // BUGFIX - Adjust the weight of all fonts to normal
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      strcpy (lf.lfFaceName, "Arial");
      lf.lfWeight = FW_BOLD;
      hFont = hFontCreate = CreateFontIndirect (&lf);
   }

   // draw text
   SelectObject (hDC, hFont);
   SetTextColor (hDC, RGB(0xff,0xff,0xff));
   SetBkColor (hDC, RGB(0,0,0));
   DWORD dwFlags = DT_TOP | DT_WORDBREAK | DT_LEFT;
   DrawText (hDC, psza, -1, &r, DT_CALCRECT | dwFlags);
   DrawText (hDC, psza, -1, &r, dwFlags);

   // x offset
   int iXOffset = 0;
   if (iHorz == 0)
      iXOffset = ((int)pImage->Width() - r.right) / 2;
   else if (iHorz > 0)
      iXOffset = (int)pImage->Width() - r.right;
   iXOffset = max(iXOffset, 0);

   // y offset
   int iYOffset = 0;
   if (iVert == 0)
      iYOffset = ((int)pImage->Height() - r.bottom) / 2;
   else if (iVert > 0)
      iYOffset = (int)pImage->Height() - r.bottom;
   iYOffset = max(iYOffset, 0);

   // figure out color where text goes
   DWORD dwCount = 0;
   double afColor[3];
   memset (afColor, 0, sizeof(afColor));
   int x,y,yMax,xMax;
   yMax = min(r.bottom, (int)pImage->Height() - iYOffset);
   xMax = min(r.right, (int)pImage->Width() - iXOffset);
   for (y = 0; y < yMax; y++) for (x = 0; x < xMax; x++) {
      if (!GetPixel (hDC, x, y))
         continue;   // not text

      // else, see what's in the image
      PIMAGEPIXEL pip = pImage->Pixel ((DWORD)(x+iXOffset), (DWORD)(y+iYOffset));
      afColor[0] += (double)pip->wRed;
      afColor[1] += (double)pip->wGreen;
      afColor[2] += (double)pip->wBlue;
      dwCount++;
   }
   if (!dwCount)
      goto done;  // shouldnt happen
   DWORD i;
   BOOL afWhite[3];
   for (i = 0; i < 3; i++) {
      afColor[i] /= (double)dwCount;
      afWhite[i] = (afColor[i] < (double)0x8000);
   }

   // transfer over
   for (y = 0; y < yMax; y++) for (x = 0; x < xMax; x++) {
      COLORREF cr = GetPixel (hDC, x, y);
      if (!cr)
         continue;   // not text

      // convert color
      WORD aw[3];
      Gamma (cr, aw);

      // else, see what's in the image
      PIMAGEPIXEL pip = pImage->Pixel ((DWORD)(x+iXOffset), (DWORD)(y+iYOffset));
      pip->dwID = 1; // BUGFIX - So that when draw on top of object image will get
               // transferred over

      WORD *pwMod = &pip->wRed;
      for (i = 0; i < 3; i++)
         pwMod[i] = (WORD)(((DWORD)(0xffff-aw[i]) * (DWORD)pwMod[i] +
            (DWORD)aw[i] * (afWhite[i] ? 0xffff : 0)) / 0x10000);
   }


done:
   DeleteDC (hDC);
   DeleteObject (hBit);
   if (hFontCreate)
      DeleteObject (hFontCreate);

   return TRUE;
}

