/************************************************************************
CFImage - Handles the image buffer for rendering, and some of the drawing
routines.

begun 14/4/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



/************************************************************************
Constructor and destructor
*/
CFImage::CFImage (void)
{
   m_dwWidth = m_dwHeight = 0;
   m_f360 = FALSE;

   GammaInit();
}

CFImage::~CFImage (void)
{
   // do nothing
}



/*************************************************************************
CFImage::Init - Initializes the data structures for the image but does NOT
blank out the pixels or Z buffer. You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
returns
   BOOL - TRUE if successful
*/
BOOL CFImage::Init (DWORD dwX, DWORD dwY)
{
   if (!m_memBuffer.Required(dwX * dwY * sizeof(FIMAGEPIXEL)))
      return FALSE;
   m_dwWidth = dwX;
   m_dwHeight = dwY;
   return TRUE;
}

/*************************************************************************
CFImage::Init - Initializes the data structures for the image. It also
CLEARS the image color to cColor and sets the ZBuffer to INFINITY.
You can call this to reinitialze the size.

inputs
   DWORD       dwX, dwY - X Y dimensions
   COLORREF    cColor - Color to clear to
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CFImage::Init (DWORD dwX, DWORD dwY, COLORREF cColor, fp fZ)
{
   if (!Init(dwX, dwY))
      return FALSE;

   return Clear (cColor, fZ);
}

/*************************************************************************
CFImage::Clear - Clears the image color to cColor, and sets the ZBuffer to
infinity.

inputs
   COLORREF    cColor - Color to clear to
   fp      fZ - Z height to use. If deeper than max depth then set to that
returns
   BOOL - TRUE if successful
*/
BOOL CFImage::Clear (COLORREF cColor, fp fZ)
{
   // fill in prototype
   FIMAGEPIXEL ip;
   memset (&ip, 0, sizeof(ip));
   if (fZ >= ZINFINITE)
      ip.fZ = ZINFINITE;
   else
      ip.fZ = fZ;
   WORD awc[3];
   Gamma (cColor, awc);
   ip.fRed = awc[0];
   ip.fGreen  = awc[1];
   ip.fBlue = awc[2];

   // do the first line
   DWORD    i;
   PFIMAGEPIXEL pip;
   pip = Pixel (0, 0);
   for (i = 0; i < m_dwWidth; i++)
      pip[i] = ip;

   // all the other lines
   for (i = 1; i < m_dwHeight; i++)
      memcpy (Pixel(0,i), Pixel(0,0), sizeof(FIMAGEPIXEL) * m_dwWidth);

   return TRUE;
}

/*************************************************************************
CFImage::Clone - Creates a new CFImage object which is a clone of this one.
It even includes the same bitmap information and Z Buffer

inputs
   none
returns
   PCFImage  - Image. NULL if cant create
*/
CFImage *CFImage::Clone (void)
{
   PCFImage  p = new CFImage;
   if (!p)
      return FALSE;

   if (!p->Init (m_dwWidth, m_dwHeight)) {
      delete p;
      return NULL;
   }

   // copy over the bitmap
   memcpy (p->Pixel(0,0), Pixel(0,0), m_dwWidth * m_dwHeight * sizeof(FIMAGEPIXEL));
   p->m_f360 = m_f360;

   return p;
}



/****************************************************************************************
CFImage::ToBitmap - Creates a bitmap (compatible to the given HDC) with the size
of the image. THen, ungammas the image and copies it all into the bitmap.

inputs
   HDC      hDC - Create a bitmap compatible to this
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CFImage::ToBitmap (HDC hDC)
{
#ifdef _TIMERS
   DWORD dwStart = GetTickCount();
#endif

   // BUGBUG - I don't know that should use HDC for this. Maybe should
   // always return 24-bit bitmap?

   HBITMAP hBit;
   DWORD x, y;
   PFIMAGEPIXEL pip;
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

   float afc[3];

   // first see if can do a 24-bit bitmap, then 16-bit, then do it the slow way
   if ( (iBitsPixel == 32) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = Width() * 4;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      pip = Pixel (0, 0);
      DWORD *pdw, dwMax;
      pdw = (DWORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (x = dwMax; x; x--, pip++, pdw++) {
         afc[0] = max (min(pip->fRed, (float)0xffff), 0);
         afc[1] = max (min(pip->fGreen, (float)0xffff), 0);
         afc[2] = max (min(pip->fBlue, (float)0xffff), 0);
         *pdw = RGB(UnGamma((WORD)afc[2]), UnGamma((WORD)afc[1]), UnGamma((WORD)afc[0]));
      }

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
            afc[0] = max (min(pip->fRed, (float)0xffff), 0);
            afc[1] = max (min(pip->fGreen, (float)0xffff), 0);
            afc[2] = max (min(pip->fBlue, (float)0xffff), 0);
            *(pb++) = UnGamma((WORD)afc[2]);
            *(pb++) = UnGamma((WORD)afc[1]);
            *(pb++) = UnGamma((WORD)afc[0]);
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
         afc[0] = max (min(pip->fRed, (float)0xffff), 0);
         afc[1] = max (min(pip->fGreen, (float)0xffff), 0);
         afc[2] = max (min(pip->fBlue, (float)0xffff), 0);
         r = UnGamma((WORD)afc[0]) >> 3;
         g = UnGamma((WORD)afc[1]) >> 2;
         b = UnGamma((WORD)afc[2]) >> 3;
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
      for (y = 0; y < m_dwHeight; y++) for (x = 0; x < m_dwWidth; x++, pip++) {
         afc[0] = max (min(pip->fRed, (float)0xffff), 0);
         afc[1] = max (min(pip->fGreen, (float)0xffff), 0);
         afc[2] = max (min(pip->fBlue, (float)0xffff), 0);
         SetPixel (hDCTemp, (int)x, (int)y,
            RGB(UnGamma((WORD)afc[0]), UnGamma((WORD)afc[1]), UnGamma((WORD)afc[2])) );
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


/********************************************************************************
CFImage::InitJPEG - Initialized an image object from the bitmap, filling the image
with the bitmap.

inputs
   HINSTANCE   hInstance - Where to get resource
   DWORD       dwID - Bitmap resource
   fp      fZ - Default Z buffer depth to use, in meters
returns
   BOOL - true if succede
*/
BOOL CFImage::InitJPEG (HINSTANCE hInstance, DWORD dwID, fp fZ)
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
CFImage::Init - Reads in a file

inputs
   PWSTR       pszFile - file
   fp      fZ - Default Z buffer depth to use, in meters
returns
   BOOL - true if succede
*/
BOOL CFImage::Init (PWSTR pszFile, fp fZ)
{
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);

   HBITMAP hBmp = JPegOrBitmapLoad(szTemp, FALSE);
   if (!hBmp)
      return FALSE;


   BOOL fRet;
   fRet = Init (hBmp, fZ);
   DeleteObject (hBmp);
   return fRet;
}


/********************************************************************************
CFImage::Init - Initialized an image object from the bitmap, filling the image
with the bitmap.

inputs
   HBITMAP     hBit - bitmap
   fp      fZ - Default Z buffer depth to use, in meters
returns
   BOOL - true if succede
*/
BOOL CFImage::Init (HBITMAP hBit, fp fZ)
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
   PFIMAGEPIXEL pip;
   FIMAGEPIXEL p;
   DWORD *pdw;
   memset (&p, 0, sizeof(p));
   p.fZ = fZ;
   dwSize = m_dwWidth * m_dwHeight;
   pdw = (DWORD*) mem.p;
   pip = Pixel(0,0);
   for (x = 0; x < dwSize; x++, pip++, pdw++) {
      p.fRed = Gamma(GetBValue ((COLORREF)(*pdw)));
      p.fGreen = Gamma(GetGValue ((COLORREF)(*pdw)));
      p.fBlue = Gamma(GetRValue ((COLORREF)(*pdw)));

      *pip = p;
   }


   return TRUE;

slowway:
   // IMPORTANT: I had planned on using getpixel, but I don't really seem to
   // need this. GetDIBits works perfectly well, converting everything to 32 bits
   // color, even if its a monochrome bitmap or whatnot

   return FALSE;
}




/*********************************************************************************
CFImage::Downsample - Downsample from the current image into a new image.

inputs
   PCFImage     pDown - Image which is downsamples
   DWORD       dwAnti - Amount to downsample, from 1..N
returns
   none
*/
void CFImage::Downsample (PCFImage pDown, DWORD dwAnti)
{
   if (dwAnti <= 1) {
      pDown->Init (m_dwWidth, m_dwHeight);
      // copy over the bitmap
      memcpy (pDown->Pixel(0,0), Pixel(0,0), m_dwWidth * m_dwHeight * sizeof(FIMAGEPIXEL));
      return;
   }

   // else
   pDown->Init (m_dwWidth / dwAnti, m_dwHeight / dwAnti);
   DWORD x,y;
   int xx, yy;
   PFIMAGEPIXEL pipDown;
   PFIMAGEPIXEL pipOrig;
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
      fp af[3];
      af[0] = af[1] = af[2] = 0;

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
            af[0] += (DWORD) pipOrig->fRed;
            af[1] += (DWORD) pipOrig->fGreen;
            af[2] += (DWORD) pipOrig->fBlue;
         }
      }

      // scale
      DWORD dwScale;
      dwScale = (DWORD) ((iXMax - iXMin) * (iYMax - iYMin));
      pipDown->fRed = af[0] / (fp) dwScale;
      pipDown->fGreen = af[1] / (fp) dwScale;
      pipDown->fBlue = af[2] / (fp) dwScale;
   }
}


/********************************************************************************
CFImage::MergeSelected - Merges this image (which is selected objects) in with
the main image (pDestImage), of unselected objects. In the merging: If the
selected object is in front, use that color. If it's behind, then blend the colors
with what's behind.

NOTE: This assumes that both images are the same size.

inputs
   CFImage      *pDestImage - To merge into
   BOOL        fBlendSelAbove - If TRUE then blend the selection above the objects
               behind, else blend normal to z
*/
void CFImage::MergeSelected (CFImage *pDestImage, BOOL fBlendSelAbove)
{
   PFIMAGEPIXEL pSrc, pDest;
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
      pDest->fRed = pSrc->fRed / 4 + pDest->fRed / 4 * 3;
      pDest->fGreen = pSrc->fGreen / 4 + pDest->fGreen / 4 * 3;
      pDest->fBlue = pSrc->fBlue / 4 + pDest->fBlue / 4 * 3;
   }
}

