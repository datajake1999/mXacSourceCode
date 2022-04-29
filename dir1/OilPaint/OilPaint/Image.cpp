/*************************************************************************************
Image.cpp - Class to handle an image

  Begun 9/29/99
  Copyright 1999-2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>
#include "oilpaint.h"
#include "escarpment.h"

extern HINSTANCE   ghInstance;

#define  GAMMADEF    2.2

/******************************************************************************
constructor & destructor
*/
CImage::CImage (void)
{
   m_dwX = m_dwY = 0;
   m_pawImage = 0;

   DWORD i;
   for (i = 0; i < 256; i++)
      m_awGamma[i] = (WORD) (pow(i / 255.0, GAMMADEF) * 65535);
}

CImage::~CImage (void)
{
   if (m_pawImage)
      free (m_pawImage);
}

/******************************************************************************
Clone - clones the bitmap and data

returns
   CImage * - new image. Must be deleted by caller
*/
CImage * CImage::Clone (void)
{
   CImage *p;

   p = new CImage();
   if (!p)
      return NULL;

   p->m_dwX = m_dwX;
   p->m_dwY = m_dwY;
   p->m_pawImage = (WORD*) malloc (m_dwX * m_dwY * 3 * sizeof(WORD));
   if (!p->m_pawImage) {
      delete p;
      return NULL;
   }

   memcpy (p->m_pawImage, m_pawImage, m_dwX * m_dwY * 3 * sizeof(WORD));

   return p;
}


/******************************************************************************
Clear - Clears the image to black
*/
HRESULT CImage::Clear (DWORD dwX, DWORD dwY)
{
   if (m_pawImage)
      free (m_pawImage);
   m_pawImage = (WORD*) malloc (dwX * dwY * 3 * sizeof(WORD));
   if (!m_pawImage)
      return E_OUTOFMEMORY;

   m_dwX = dwX;
   m_dwY = dwY;
   memset (m_pawImage, 0, dwX * dwY * 3 * sizeof(WORD));

   return NOERROR;
}

/******************************************************************************
PixelSet - Sets a pixel

inputs
   DWORD    dwX, dwY - XY pixel
   COLORREF      rgb - color values
*/
HRESULT CImage::PixelSet (DWORD dwX, DWORD dwY, COLORREF rgb)
{
   return PixelSet (dwX, dwY,
      m_awGamma[GetRValue(rgb)], m_awGamma[GetGValue(rgb)], m_awGamma[GetBValue(rgb)]);
}

/******************************************************************************
PixelSet - Sets a pixel

inputs
   DWORD    dwX, dwY - XY pixel
   WORD     r, g, b - already gamma adjusted
*/
HRESULT CImage::PixelSet (DWORD dwX, DWORD dwY, WORD r, WORD g, WORD b)
{
   if ((dwX >= m_dwX) || (dwY >= m_dwY))
      return E_FAIL;

   WORD *p;
   p = m_pawImage + (dwX + dwY * m_dwX) * 3;

   *(p++) = r;
   *(p++) = g;
   *p = b;

   return NOERROR;
}

/******************************************************************************
PixelGet - Gets a pixel

inputs
   DWORD    dwX, dwY - XY pixel
   WORD     *r, *g, *b - already gamma adjusted
*/
HRESULT CImage::PixelGet (DWORD dwX, DWORD dwY, WORD *r, WORD *g, WORD *b)
{
   if ((dwX >= m_dwX) || (dwY >= m_dwY))
      return E_FAIL;

   WORD *p;
   p = m_pawImage + (dwX + dwY * m_dwX) * 3;

   *r = *(p++);
   *g = *(p++);
   *b = *p;

   return NOERROR;
}

/*****************************************************************************
BMPSize - Returns the size of the bitmap (in pixels)

inputs
   HBMP     hBmp - bitmap
   int      *piWidth, int *piHeight - filled in with the size
returns
   BOOL - TRUE if succed
*/
BOOL BMPSize (HBITMAP hBmp, int *piWidth, int *piHeight)
{
   *piWidth = *piHeight = 0;

   // size of the bitmap
   BITMAP   bm;
   if (!GetObject (hBmp, sizeof(bm), &bm))
      return FALSE;

   *piWidth = bm.bmWidth;
   *piHeight = bm.bmHeight;

   return TRUE;
}


/*********************************************************************************
FromBitmapFile - loads data in from a file, bitmap or jpeg

  fShrink - Shrink to a smaller size of its too large.

inputs
   psz - File name
*/
HRESULT CImage::FromBitmapFile (char *psz, BOOL fShrink)
{
   // try opening the file
   FILE  *f = NULL;
   HRESULT hRes = NOERROR;
   PBYTE pMem = NULL;
   HDC   hDC = NULL;
   HBITMAP hBit = NULL;
   HBITMAP  hBitSmall = NULL;

#ifdef TESTMSG
   MessageBox (NULL, "Test version - FromBitmapFile", NULL, MB_OK);
#endif
   // see if it's jpeg
   int   iLen;
   iLen = strlen(psz);
   // BUGFIX - Allow to be .jpg or .jpeg
   if ( ((iLen >= 4) && !_strnicmp(psz + (iLen-4), ".jpg", 4)) ||
      ((iLen >= 5) && !_strnicmp(psz + (iLen-5), ".jpeg", 5)) ){
#ifdef TESTMSG
   MessageBox (NULL, "Test version - jpg", NULL, MB_OK);
#endif
      hBit = JPegToBitmapNoMegaFile (psz);
   }
   else {
#ifdef TESTMSG
   MessageBox (NULL, "Test version - bmp", NULL, MB_OK);
#endif
      f = fopen (psz, "rb");
      if (!f) {
         hRes = E_FAIL;
         goto done;
      }

      // how big is it?
      fseek (f, 0, SEEK_END);
      DWORD dwSize;
      dwSize = ftell (f);
      fseek (f, 0, 0);

      // read it in
      pMem = (PBYTE) malloc (dwSize);
      if (!pMem) {
         hRes = E_OUTOFMEMORY;
         goto done;
      }

      fread (pMem, 1, dwSize, f);

      // figure out where everything is
      PBYTE pMax;
      pMax = pMem + dwSize;

      BITMAPFILEHEADER  *pHeader;
      pHeader = (BITMAPFILEHEADER*) pMem;
      if ( ((PBYTE) (pHeader+1) > pMax) || pHeader->bfType != 0x4d42) {
         hRes = E_FAIL;
         goto done;
      }

      // other info
      BITMAPINFOHEADER *pbi;
      pbi = (BITMAPINFOHEADER*) (pHeader+1);
      if ((PBYTE)(pbi+1) > pMax) {
         hRes = E_FAIL;
         goto done;
      }

      if (pbi->biSize < sizeof (BITMAPINFOHEADER)) {
         hRes = E_FAIL;
         goto done;
      }

      // data bits
      PBYTE pImage;
      pImage = pMem + pHeader->bfOffBits;

      // create a bitmap out of this
      hDC = GetDC (NULL);
      if (!hDC) {
         hRes = E_FAIL;
         goto done;
      }

      hBit = CreateDIBitmap (hDC, pbi, CBM_INIT, pImage, (BITMAPINFO*) pbi, DIB_RGB_COLORS);
      if (!hBit) {
         hRes = E_FAIL;
         goto done;
      }
   }

#ifdef TESTMSG
   MessageBox (NULL, "Test version - Bitmap loaded", NULL, MB_OK);
#endif
   // BUGFIX: if it's too large then shrink it
   int   iWidth, iHeight;
   int   iScale;
   iWidth = iHeight = 0;
   iScale = 1;
   BMPSize (hBit, &iWidth, &iHeight);
   // BUGFIX - Changed from 1024 to 2048 to allow larger bitmaps
   while ((iWidth > (2048 * iScale)) || (iHeight > (2048*iScale)))
      iScale *= 2;
   if (fShrink && (iScale > 1)) {
      // get the main DC
      if (!hDC) {
         // create a bitmap out of this
         hDC = GetDC (NULL);
         if (!hDC) {
            hRes = E_FAIL;
            goto done;
         }
      }

      HDC   hDCMem, hDCMem2;
      hDCMem = CreateCompatibleDC (hDC);
      hDCMem2 = CreateCompatibleDC (hDC);
      if (!hDCMem || !hDCMem2) {
         hRes = E_FAIL;
         goto done;
      }

      SelectObject (hDCMem, hBit);
      hBitSmall = CreateCompatibleBitmap (hDC, iWidth / iScale, iHeight / iScale);
      if (!hBitSmall) {
         hRes = E_FAIL;
         goto done;
      }
      SelectObject (hDCMem2, hBitSmall);

      // shrink
      SetStretchBltMode (hDCMem2, COLORONCOLOR);
      StretchBlt (hDCMem2,
         0, 0, iWidth / iScale, iHeight / iScale,
         hDCMem,
         0, 0, iWidth, iHeight,
         SRCCOPY);

      DeleteDC (hDCMem);
      DeleteDC (hDCMem2);
   }

#ifdef TESTMSG
   MessageBox (NULL, "Test version - Calling FromBitmap", NULL, MB_OK);
#endif

   hRes = FromBitmap (hBitSmall ? hBitSmall : hBit);

done:
   if (hBitSmall)
      DeleteObject (hBitSmall);
   if (hBit)
      DeleteObject (hBit);
   if (hDC)
      ReleaseDC (NULL, hDC);
   if (f)
      fclose (f);

   if (pMem)
      free (pMem);

   return hRes;
}


/*********************************************************************************
FromBitmap - loads data in from a bitmap. This does not free up the bitmap

inputs
   HBITMAP  hBit
*/
HRESULT CImage::FromBitmap (HBITMAP hBit)
{
   // try opening the file
   HRESULT hRes = NOERROR;
   HDC   hDC = NULL;
   HDC   hDCMem = NULL;
   HBITMAP hBitOld = NULL;

   // create a bitmap out of this
   hDC = GetDC (NULL);
   if (!hDC) {
      hRes = E_FAIL;
      goto done;
   }

   // BUGBUG - what if user doens't have good number of planes
   hDCMem = CreateCompatibleDC (hDC);
   if (!hDCMem) {
      hRes = E_FAIL;
      goto done;
   }
   hBitOld = (HBITMAP) SelectObject (hDCMem, hBit);

   // get the size of the bitmap
   BITMAP   bm;
   GetObject (hBit, sizeof(bm), &bm);
   Clear (bm.bmWidth, bm.bmHeight);

   // preload bitmap into memory
   DWORD x,y;
   for (y = 0; (int) y < bm.bmHeight; y++)
      for (x = 0; (int)x < bm.bmWidth; x++)
         PixelSet (x, y, GetPixel (hDCMem, x, y));



done:

   if (hBitOld && hDCMem)
      SelectObject (hDCMem, hBitOld);
   if (hDCMem)
      DeleteDC (hDCMem);
   if (hDC)
      ReleaseDC (NULL, hDC);
   return hRes;
}


/******************************************************************************
UnGamma - Convert from linear light intensity to gamma.

inputs
   WORD  w
returns
   BYTE b
*/
BYTE CImage::UnGamma (WORD w)
{
   BYTE  bRet, bCur;

   for (bCur = 0x80, bRet = 0; bCur; bCur >>= 1)
      if (w >= m_awGamma[bRet + bCur])
         bRet += bCur;

   return bRet;
}

/******************************************************************************
UnGamma - Convert from linear light intensity to gamma.

inputs
   WORD r, g, b - gamma red, green, blue
returns
   COLORREF - rgb
*/
COLORREF CImage::UnGamma (WORD r, WORD g, WORD b)
{
   return RGB(UnGamma(r), UnGamma(g), UnGamma(b));
}

/******************************************************************************
ToJPEGFile - Saves the image as a JPEG file.

inputs
   HWND        hWnd - Window to display dialog off of.
   char*       psz - String. If NULL then display dialog off of hWnd
returns
   HRESULT
*/

HRESULT CImage::ToJPEGFile (HWND hWnd, char* psz)
{
   char  szTemp[256];
   szTemp[0] = 0;
   if (!psz) {
      // get name to save as
      OPENFILENAME   ofn;
      memset (&ofn, 0, sizeof(ofn));
      char szInitial[256];
      szInitial[0] = 0;
      ofn.lpstrInitialDir = szInitial;
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = hWnd;
      ofn.hInstance = ghInstance;
      ofn.lpstrFilter = "JPEG file (*.jpg)\0*.jpg\0\0";
      ofn.lpstrFile = szTemp;
      ofn.nMaxFile = sizeof(szTemp);
      ofn.lpstrTitle = "Save image file";
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
      ofn.lpstrDefExt = ".jpg";
      // nFileExtension 
      if (!GetSaveFileName(&ofn))
         return E_FAIL;

      psz = szTemp;
   }

   if (!hWnd)
      hWnd = GetDesktopWindow ();
   HDC hDC = GetDC (hWnd);
   HBITMAP hBit = ToBitmap (hDC, FALSE);
   ReleaseDC (hWnd, hDC);
   if (!hBit)
      return E_FAIL;

   BOOL fRet = BitmapToJPegNoMegaFile (hBit, psz);
   DeleteObject (hBit);

   return fRet ? NOERROR : E_FAIL;
}


/******************************************************************************
ToBitmap - Converts the image to a bitmap.

inputs
   HDC - DC to use
   double   fGridX, fGridY - If not 0, put in grid lines every N pixels
   int      iGridThickness - Grid thickness
   BOOL     fInvert - If TRUE then invert color
returns
   HBITMAP - bitmap. Must be freed by the caller
*/
HBITMAP CImage::ToBitmap (HDC hDCOrig, BOOL fInvert)
{
#ifdef _TIMERS
   DWORD dwStart = GetTickCount();
#endif

   HBITMAP hBit;
   DWORD x, y;
   WORD  *paw;
   paw = m_pawImage;

   //PIMAGEPIXEL pip;
   //BYTE * pb;

   HDC   hDC  =NULL;  // main HDC

   if (!hDCOrig) {
      hDC = GetDC (NULL);
      if (!hDC)
         return NULL;
   }
   else
      hDC = hDCOrig;

   int   iBitsPixel, iPlanes;
   iBitsPixel = GetDeviceCaps (hDC, BITSPIXEL);
   iPlanes = GetDeviceCaps (hDC, PLANES);
   BITMAP  bm;
   memset (&bm, 0, sizeof(bm));
   bm.bmWidth = m_dwX;
   bm.bmHeight = m_dwY;
   bm.bmPlanes = iPlanes;
   bm.bmBitsPixel = iBitsPixel;

   // BUGFIX - Always do the slow way so dont unintentionally have interaction with
   // graphics cards. Also, fInvert only in one code
#if 0
   // first see if can do a 24-bit bitmap, then 16-bit, then do it the slow way
   if ( (iBitsPixel == 32) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = m_dwX * 4;
      bm.bmBits = malloc(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits) {
         if (hDC && !hDCOrig)
            ReleaseDC (NULL, hDC);  // BUGFIX - Was hDCOrig
         return NULL;   // can't do it
      }

      // loop
      //pip = Pixel (0, 0);
      DWORD *pdw, dwMax;
      pdw = (DWORD*) bm.bmBits;
      dwMax = m_dwX * m_dwY;
      for (x = 0; x < dwMax; x++, paw += 3, pdw++)
         *pdw = RGB(UnGamma(paw[2]), UnGamma(paw[1]), UnGamma(paw[0]));

      hBit = CreateBitmapIndirect (&bm);
      free (bm.bmBits);
   }
   else if ( (iBitsPixel == 24) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = m_dwX * 3;
      if (bm.bmWidthBytes % 2)
         bm.bmWidthBytes++;
      bm.bmBits = malloc(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits) {
         if (hDC && !hDCOrig)
            ReleaseDC (NULL, hDC);  // BUGFIX - Was hDCOrig
         return NULL;   // can't do it
      }

      // loop
      //pip = Pixel (0, 0);
      for (y = 0; y < m_dwY; y++) {
         pb = (BYTE*) bm.bmBits + (y * bm.bmWidthBytes);
         for (x = 0; x < m_dwX; x++, paw += 3) {
            *(pb++) = UnGamma(paw[2]);
            *(pb++) = UnGamma(paw[1]);
            *(pb++) = UnGamma(paw[0]);
         }
      }

      hBit = CreateBitmapIndirect (&bm);
      free (bm.bmBits);
   }
   else if ( (iBitsPixel == 16) && (iPlanes == 1) ) {
      // 16 bit bitmap
      bm.bmWidthBytes = m_dwX * 2;
      bm.bmBits = malloc(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits) {
         if (hDC && !hDCOrig)
            ReleaseDC (NULL, hDC);  // BUGFIX - Was hDCOrig
         return NULL;   // can't do it
      }

      // loop
      //pip = Pixel (0, 0);
      WORD *pw;
      DWORD dwMax;
      pw = (WORD*) bm.bmBits;
      dwMax = m_dwY * m_dwX;
      for (x = 0; x < dwMax; x++, paw += 3, pw++) {
         BYTE r,g,b;
         r = UnGamma(paw[0]) >> 3;
         g = UnGamma(paw[1]) >> 2;
         b = UnGamma(paw[2]) >> 3;
         *pw = ((WORD)r << 11) | ((WORD)g << 5) | ((WORD)b);
      }

      hBit = CreateBitmapIndirect (&bm);
      free (bm.bmBits);
   }
   else {
#endif // 0
      // the slow way
      hBit = CreateCompatibleBitmap (hDC, (int) m_dwX, (int) m_dwY);
      if (!hBit) {
         if (hDC && !hDCOrig)
            ReleaseDC (NULL, hDC);  // BUGFIX - Was hDCOrig
         return NULL;
      }

      HDC hDCTemp;
      hDCTemp = CreateCompatibleDC (hDC);
      SelectObject (hDCTemp, hBit);

      // loop
      //pip = Pixel (0, 0);
      for (y = 0; y < m_dwY; y++) for (x = 0; x < m_dwX; x++, paw += 3)
         SetPixel (hDCTemp, (int)x, (int)y, UnGamma(
            fInvert ? (0xffff - paw[0]) : paw[0],
            fInvert ? (0xffff - paw[1]) : paw[1],
            fInvert ? (0xffff - paw[2]) : paw[2]));

      // done
      DeleteDC (hDCTemp);
#if 0
   }
#endif // 0

   if (hDC && !hDCOrig)
      ReleaseDC (NULL, hDC);  // BUGFIX - Was hDCOrig

#ifdef _TIMERS
   char szTemp[128];
   sprintf (szTemp, "ToBitmap time = %d\r\n", (int) (GetTickCount() - dwStart));
   OutputDebugString (szTemp);
#endif
   return hBit;
}

#if 0 // BUGFIX- Got rid of this because probably busted
HBITMAP CImage::ToBitmap (HDC hDCOrig)
{
   HDC   hDCDraw = NULL;
   HBITMAP  hBitDrawOld = NULL;
   HBITMAP  hBitDraw = NULL;
   HDC   hDC  =NULL;  // main HDC

   if (!hDCOrig) {
      hDC = GetDC (NULL);
      if (!hDC)
         goto done;
   }
   else
      hDC = hDCOrig;

   // bitmap
   hBitDraw = CreateCompatibleBitmap (hDC, m_dwX, m_dwY);
   if (!hBitDraw)
      return NULL;
   hDCDraw = CreateCompatibleDC (hDC);
   hBitDrawOld = (HBITMAP) SelectObject (hDCDraw, hBitDraw);

   DWORD x, y;
   WORD  *p;
   p = m_pawImage;
   for (y = 0; y < m_dwY; y++)
      for (x = 0; x < m_dwX; x++) {
         SetPixel (hDCDraw, x, y, UnGamma (p[0], p[1], p[2]) );
         p += 3;
      }

done:
   if (hDCDraw) {
      if (hBitDrawOld)
         SelectObject (hDCDraw, hBitDrawOld);
      DeleteDC (hDCDraw);
   }
   if (hDC && !hDCOrig)
      ReleaseDC (NULL, hDC);

   return hBitDraw;
}
#endif // 0
