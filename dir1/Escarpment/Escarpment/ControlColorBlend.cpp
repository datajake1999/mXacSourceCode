/***********************************************************************
ControlColorBlend.cpp - Code for a control

begun 3/31/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "escarpment.h"
#include "resleak.h"

typedef struct {
   PWSTR       pszHRef;
   double      fTransparent;
   BOOL        fSkipIfBackground;      // skip drawing if there's a m_hbmpBackground
} COLORBLEND, *PCOLORBLEND;


class CImageRGB {
public:
   CImageRGB (void);
   ~CImageRGB (void);

   BOOL Init (DWORD dwWidth, DWORD dwHeight);
   BOOL Init (HBITMAP hBit);
   BOOL Init (HDC hDCSrc, RECT *prSrc);
   void ClearToColor (COLORREF cr);

   HBITMAP ToBitmap (HDC hDC);
   BOOL ToBitmap (HDC hDCTo, int iX, int iY);

   __inline PBYTE Pixel (DWORD dwX, DWORD dwY)
      {
         return ((PBYTE)m_memRGB.p) + ((dwX + dwY * m_dwWidth)*3);
      };

   DWORD Width (void);
   DWORD Height (void);

private:
   DWORD             m_dwWidth;        // width
   DWORD             m_dwHeight;       // height
   CMem              m_memRGB;         // mem with RGB bytes
};
typedef CImageRGB *PCImageRGB;



/***********************************************************************
TransparentFill - Fill with a transparent color

inputs
   HDC            hDC - To paint in
   COLORREF       c - Color
   float          fTrans - Transparency, from 0 to 1.
   RECT           *prPaint - Rectangle to paint
   RECT           *prInvalid - Invalid part on HDC
returns
   none
*/
void TransparentFill (HDC hDC, COLORREF c, double fTrans, RECT *prPaint, RECT *prInvalid)
{
   if (fTrans >= 1.0)
      return;  // totally transparent

   if (fTrans <= 0.0) {
      HBRUSH hbr = CreateSolidBrush (c);
      FillRect (hDC, prPaint, hbr);
      DeleteObject (hbr);
      return;
   }

   // BUGBUG
   // *((PBYTE)NULL) = 0;
   // BUGBUG

   // else, make sure not out of bounds
   RECT rInter;
   if (!IntersectRect (&rInter, prPaint, prInvalid))
      return;

   int iWidth = GetDeviceCaps (hDC, HORZRES);
   int iHeight = GetDeviceCaps (hDC, VERTRES);
   rInter.left = max(rInter.left, 0);
   rInter.top = max(rInter.top, 0);
   rInter.right = min(rInter.right, iWidth);
   rInter.bottom = min(rInter.bottom, iHeight);

   DWORD dwTrans = (DWORD)(fTrans * 256.0);
   DWORD dwOneMinus = 256 - dwTrans;
   DWORD dwRed = (DWORD) GetRValue(c) * dwOneMinus;
   DWORD dwGreen = (DWORD) GetGValue(c) * dwOneMinus;
   DWORD dwBlue = (DWORD) GetBValue(c) * dwOneMinus;

   CImageRGB Image;

   if (!Image.Init (hDC, &rInter))
      return;

   int x, y;
   // COLORREF cPixel;
   for (y = rInter.top; y < rInter.bottom; y++) {
      PBYTE pPixel = Image.Pixel (0, (DWORD)(y - rInter.top));
      for (x = rInter.left; x < rInter.right; x++, pPixel += 3) {

         pPixel[0] = (BYTE) (((DWORD) pPixel[0] * dwTrans + dwRed) / 256);
         pPixel[1] = (BYTE) (((DWORD) pPixel[1] * dwTrans + dwRed) / 256);
         pPixel[2] = (BYTE) (((DWORD) pPixel[2] * dwTrans + dwRed) / 256);

#if 0 // old, slow code
         cPixel = GetPixel (hDC, x, y);
         if (cPixel == CLR_INVALID)
            continue;

         cPixel = RGB (
            (BYTE) (((DWORD) GetRValue(cPixel) * dwTrans + dwRed) / 256),
            (BYTE) (((DWORD) GetGValue(cPixel) * dwTrans + dwGreen) / 256),
            (BYTE) (((DWORD) GetBValue(cPixel) * dwTrans + dwBlue) / 256),
            );

         SetPixel (hDC, x, y, cPixel);
#endif
      } // x,y
   }

   // write it out
   Image.ToBitmap (hDC, rInter.left, rInter.top);
}



/***********************************************************************
Control callback
*/
BOOL ControlColorBlend (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   COLORBLEND  *p = (COLORBLEND*) pControl->m_mem.p;
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(COLORBLEND));
         p = (COLORBLEND*) pControl->m_mem.p;
         memset (p, 0, sizeof(COLORBLEND));
         p->pszHRef = NULL;
         p->fTransparent = 0;
         p->fSkipIfBackground = FALSE;
         pControl->AttribListAddString (L"href", &p->pszHRef);
         pControl->AttribListAddDouble (L"transparent", &p->fTransparent);
         pControl->AttribListAddBOOL (L"skipifbackground", &p->fSkipIfBackground);
      }
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // if this has a href then want mouse
         pControl->m_fWantMouse = p->pszHRef ? TRUE : FALSE;
         if (pControl->m_fWantMouse)
            pControl->m_dwWantFocus = 1;

         // secify that accept space or enter
         if (pControl->m_dwWantFocus) {
            ESCACCELERATOR a;
            memset (&a, 0, sizeof(a));
            a.c = L' ';
            a.dwMessage = ESCM_SWITCHACCEL;
            pControl->m_listAccelFocus.Add (&a);
            a.c = L'\n';
            pControl->m_listAccelFocus.Add (&a);
         }
      }
      return TRUE;


   case ESCM_LBUTTONDOWN:
      {
         if (p->pszHRef) {
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_LINKCLICK);

            // must release capture or bad things happen
            pControl->m_pParentPage->MouseCaptureRelease(pControl);

            pControl->m_pParentPage->Link (p->pszHRef);
         }
      }
      return TRUE;

   case ESCM_PAINT:
      {
         PESCMPAINT pp = (PESCMPAINT) pParam;
         COLORREF ul, ll, ur, lr;
         COLORREF cr;

         // potentially skip if there's a background
         if (p->fSkipIfBackground)
            if (pControl->m_pParentPage->m_pWindow->m_hbmpBackground)
               return TRUE;

         // get the color from the attributes
         ul = ll = ur = lr = RGB(0,0,0xff);
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"color"), &cr))
            ul = ll = ur = lr = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"lcolor"), &cr))
            ul = ll = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"rcolor"), &cr))
            ur = lr = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"tcolor"), &cr))
            ul = ur = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"bcolor"), &cr))
            ll = lr = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"ulcolor"), &cr))
            ul = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"llcolor"), &cr))
            ll = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"urcolor"), &cr))
            ur = cr;
         if (AttribToColor ((PWSTR) pControl->m_treeAttrib.Find (L"lrcolor"), &cr))
            lr = cr;

         // paint
         //HBRUSH   hbr;

      #define  COLORDIST(x,y)    (DWORD)(max(x,y)-min(x,y))

         // figure out how many divions across & down need
         DWORD dwAcross, dwDown;
         dwAcross = COLORDIST(GetRValue(ul), GetRValue(ur));
         dwAcross = max(dwAcross, COLORDIST(GetGValue(ul), GetGValue(ur)));
         dwAcross = max(dwAcross, COLORDIST(GetBValue(ul), GetBValue(ur)));
         dwAcross = max(dwAcross, COLORDIST(GetRValue(ll), GetRValue(lr)));
         dwAcross = max(dwAcross, COLORDIST(GetGValue(ll), GetGValue(lr)));
         dwAcross = max(dwAcross, COLORDIST(GetBValue(ll), GetBValue(lr)));

         dwDown = COLORDIST(GetRValue(ul), GetRValue(ll));
         dwDown = max(dwDown, COLORDIST(GetGValue(ul), GetGValue(ll)));
         dwDown = max(dwDown, COLORDIST(GetBValue(ul), GetBValue(ll)));
         dwDown = max(dwDown, COLORDIST(GetRValue(ur), GetRValue(lr)));
         dwDown = max(dwDown, COLORDIST(GetGValue(ur), GetGValue(lr)));
         dwDown = max(dwDown, COLORDIST(GetBValue(ur), GetBValue(lr)));

         // BUGFIX - Allow more subdivisions to less mach banding
         if (((dwAcross+1)*(dwDown+1)) > 100) {
            // divide by 4 - mach banding
            dwAcross /= 2;
            dwDown /= 2;
         }
         // dwAcross /= 2;
         // dwDown /= 2;

         // also, no more than a trasition every 4? pixels
         // BUGFIX - Make the transition every 2 pixels
         dwAcross = min((DWORD) (pp->rControlHDC.right - pp->rControlHDC.left) / 2, dwAcross);
         dwDown = min((DWORD) (pp->rControlHDC.bottom - pp->rControlHDC.top) / 2, dwDown);

         // at least 2
         dwAcross = max(dwAcross,2);
         dwDown = max(dwDown, 2);

         // loop
         DWORD x, y;
         double   iLeft, iTop;
         double   iDeltaX, iDeltaY;
         RECT  r;
         iTop = pp->rControlHDC.top;
         iDeltaX = (double) (pp->rControlHDC.right - pp->rControlHDC.left) / (double) dwAcross;
         iDeltaY = (double) (pp->rControlHDC.bottom - pp->rControlHDC.top) / (double) dwDown;

      #define ALPHA(x,y,amt)     (BYTE)((1.0-(amt))*(x) + (amt)*(y))
         for (y = 0; y < dwDown; y++, iTop += iDeltaY ) {
            // rectangle
            iLeft = pp->rControlHDC.left;
            r.top = (int) iTop;
            r.bottom = (int) (iTop + iDeltaY);
            if (y+1 == dwDown)
               r.bottom = pp->rControlHDC.bottom;  // ensure theres no roundoff error

            // if not in refresh area then skip
            if ((r.bottom < pp->rInvalidHDC.top) || (r.top > pp->rInvalidHDC.bottom))
               continue;

            COLORREF cLeft, cRight;
            double   fAlpha;
            fAlpha = (double) y / (double) (dwDown-1);
            cLeft = RGB(
               ALPHA(GetRValue(ul),GetRValue(ll),fAlpha),
               ALPHA(GetGValue(ul),GetGValue(ll),fAlpha),
               ALPHA(GetBValue(ul),GetBValue(ll),fAlpha)
               );
            cRight = RGB(
               ALPHA(GetRValue(ur),GetRValue(lr),fAlpha),
               ALPHA(GetGValue(ur),GetGValue(lr),fAlpha),
               ALPHA(GetBValue(ur),GetBValue(lr),fAlpha)
               );

            for (x = 0; x < dwAcross; x++, iLeft += iDeltaX) {
               // rectangle
               r.left = (int) iLeft;
               r.right = (int) (iLeft + iDeltaX);
               if (x+1 == dwAcross)
                  r.right = pp->rControlHDC.right;  // ensure theres no roundoff error

               // if not in refresh area then skip
               if ((r.right < pp->rInvalidHDC.left) || (r.left > pp->rInvalidHDC.right))
                  continue;

               COLORREF c;
               double   fAlpha;
               fAlpha = (double) x / (double) (dwAcross-1);
               c = RGB(
                  ALPHA(GetRValue(cLeft),GetRValue(cRight),fAlpha),
                  ALPHA(GetGValue(cLeft),GetGValue(cRight),fAlpha),
                  ALPHA(GetBValue(cLeft),GetBValue(cRight),fAlpha)
               );

               // paint it
               // BUGFIX - To allow for partial transparency
               TransparentFill (pp->hDC, c, p->fTransparent, &r, &pp->rInvalidHDC);
               //hbr = CreateSolidBrush (c);
               //FillRect (p->hDC, &r, hbr);
               //DeleteObject (hbr);
            }
         }
      }
      return TRUE;


   }

   return FALSE;
}






/*************************************************************************************
CImageRGB::Constructor and destructor
*/
CImageRGB::CImageRGB (void)
{
   // init gamma just in case
   // GammaInit ();

   m_dwWidth = m_dwHeight = 0;

}

CImageRGB::~CImageRGB (void)
{
   // do nothing
}

/*************************************************************************************
CImageRGB::Init - initialize a blank (albeit with random values) image.

inputs
   DWORD          dwWidth - Width in pixels
   DWORD          dwHeight - Height in pixels
returns
   BOOL - TRUE if success
*/
BOOL CImageRGB::Init (DWORD dwWidth, DWORD dwHeight)
{
   // free up old stuff
   m_dwWidth = dwWidth;
   m_dwHeight = dwHeight;

   return m_memRGB.Required (m_dwWidth * m_dwHeight * 3);
}


/*************************************************************************************
CImageRGB::Init - Initialize the image from a bitmap

inputs
   HBITMAP        hBit - Bitmap to use
returns
   BOOL - TRUE if success
*/
BOOL CImageRGB::Init (HBITMAP hBit)
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
   DWORD *pdw;
   dwSize = m_dwWidth * m_dwHeight;
   pdw = (DWORD*) mem.p;
   PBYTE pab = (PBYTE)m_memRGB.p;
   for (x = 0; x < dwSize; x++, pdw++) {
      *(pab++) = GetBValue ((COLORREF)(*pdw));
      *(pab++) = GetGValue ((COLORREF)(*pdw));
      *(pab++) = GetRValue ((COLORREF)(*pdw));
   }


   return TRUE;

slowway:
   // IMPORTANT: I had planned on using getpixel, but I don't really seem to
   // need this. GetDIBits works perfectly well, converting everything to 32 bits
   // color, even if its a monochrome bitmap or whatnot

   return FALSE;
}




/*************************************************************************************
CImageRGB::ToBitmap - Creates a bitmap (compatible to the given HDC) with the size
of the image. THen, ungammas the image and copies it all into the bitmap.

inputs
   HDC      hDC - Create a bitmap compatible to this
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CImageRGB::ToBitmap (HDC hDC)
{
   HBITMAP hBit;
   DWORD x, y;
   PBYTE pabRGB = (PBYTE)m_memRGB.p;
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
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      DWORD *pdw, dwMax;
      pdw = (DWORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (x = dwMax; x; x--, pdw++, pabRGB += 3)
         *pdw = RGB(pabRGB[2], pabRGB[1], pabRGB[0]);

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
      for (y = 0; y < m_dwHeight; y++) {
         pb = (BYTE*) bm.bmBits + (y * bm.bmWidthBytes);
         for (x = 0; x < m_dwWidth; x++, pabRGB += 3) {
            *(pb++) = pabRGB[2];
            *(pb++) = pabRGB[1];
            *(pb++) = pabRGB[0];
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
      WORD *pw;
      DWORD dwMax;
      pw = (WORD*) bm.bmBits;
      dwMax = m_dwHeight * m_dwWidth;
      for (x = 0; x < dwMax; x++, pw++, pabRGB+=3) {
         BYTE r,g,b;
         r = pabRGB[0] >> 3;
         g = pabRGB[1] >> 2;
         b = pabRGB[2] >> 3;
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
      for (y = 0; y < m_dwHeight; y++) for (x = 0; x < m_dwWidth; x++, pabRGB += 3)
         SetPixel (hDCTemp, (int)x, (int)y, RGB(pabRGB[0], pabRGB[1], pabRGB[2]));

      // done
      DeleteDC (hDCTemp);
   }

   return hBit;
}


/*************************************************************************************
CImageRGB::Width - Returns the width
*/
DWORD CImageRGB::Width (void)
{
   return m_dwWidth;
}

/*************************************************************************************
CImageRGB::Height - Returns the height
*/
DWORD CImageRGB::Height (void)
{
   return m_dwHeight;
}



/*************************************************************************************
CImageRGB::ClearToColor - Clears the image to a solid color

inputs
   COLORREF          cr - To clear to
*/
void CImageRGB::ClearToColor (COLORREF cr)
{
   DWORD x, y;
   PBYTE pb = Pixel (0,0);
   for (x = 0; x < m_dwWidth; x++) {
      *(pb++) = GetRValue(cr);
      *(pb++) = GetGValue(cr);
      *(pb++) = GetBValue(cr);
   } // x

   PBYTE pbFrom = Pixel(0,0);
   for (y = 0; y < m_dwHeight; y++) {
      pb = Pixel (0, y);
      memcpy (pb, pbFrom, m_dwWidth * 3);
   }
}


/*************************************************************************************
CImageRGB::Init - Initialize from a HDC by using bitblit.

inputs
   HDC         hDCSrc - Source HDC
   RECT        rSrc - Rectangle for the source. Must be valid.
returns
   BOOL - TRUE if success
*/
BOOL CImageRGB::Init (HDC hDCSrc, RECT *prSrc)
{
   if ((prSrc->right <= prSrc->left) || (prSrc->bottom <= prSrc->top))
      return FALSE;

   HDC hDC = CreateCompatibleDC (hDCSrc);
   if (!hDC)
      return FALSE;
   HBITMAP hBmp = CreateCompatibleBitmap (hDCSrc, prSrc->right - prSrc->left, prSrc->bottom - prSrc->top);
   if (!hBmp) {
      DeleteDC (hDC);
      return FALSE;
   }
   SelectObject (hDC, hBmp);

   // bitblt it over
   BitBlt (
      hDC, 0, 0, prSrc->right - prSrc->left, prSrc->bottom - prSrc->top,
      hDCSrc, prSrc->left, prSrc->top,
      SRCCOPY);
   DeleteDC (hDC);

   // init from this bitmap
   BOOL fRet = Init (hBmp);
   DeleteObject (hBmp);
   return fRet;
}


/*************************************************************************************
CImageRGB::ToBitmap - Bitblts the image onto a HDC

inputs
   HDC         hDCTo - To copy to
   int         iX - UL corner
   int         iY - UL corner
returns
   BOOL - TRUE if success
*/
BOOL CImageRGB::ToBitmap (HDC hDCTo, int iX, int iY)
{
   HBITMAP hBmp = ToBitmap (hDCTo);
   if (!hBmp)
      return FALSE;

   HDC hDC = CreateCompatibleDC (hDCTo);
   if (!hDC) {
      DeleteObject (hBmp);
      return FALSE;
   }

   SelectObject (hDC, hBmp);

   // bitblt over
   BitBlt (
      hDCTo, iX, iY, (int)m_dwWidth, (int)m_dwHeight,
      hDC, 0, 0,
      SRCCOPY);

   // free up
   DeleteDC (hDC);
   DeleteObject (hBmp);

   return TRUE;
}

// BUGBUG - 2.0 - need a way of making a watermark color blend
