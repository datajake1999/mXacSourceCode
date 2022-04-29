/*************************************************************************************
CImageStore.cpp - Code for storing an image away in the megafile.

begun 10/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"

// IMAGESTOREHDR - information
typedef struct {
   DWORD          dwStretch;     // stretch mode, same as m_dwStetch
   DWORD          dwWidth;       // width
   DWORD          dwHeight;      // height
} IMAGESTOREHDR, *PIMAGESTOREHDR;

#define IMAGESTOREHDRJPEG_VERSION      3545234

// IMAGESTOREHDRJPEG - JPEG information
typedef struct {
   DWORD          dwVersionID;   // must be IMAGESTOREHDRJPEG_VERSION
   DWORD          dwStretch;     // stretch mode, same as m_dwStetch
   DWORD          dwWidth;       // width
   DWORD          dwHeight;      // height

   DWORD          dwImageSize;   // size of the image
   DWORD          dwMaskSize;    // size of the mask
   DWORD          dwFillSize;    // number of bytes of fill
   DWORD          dwStringSize;  // MML string

   // BYTE[dwImageSize] - JPEG image
   // BYTE[dwMaskSize] - Mask (convert to 0 color)
   // BYTE[dwFillSize] - FIll
   // BYTE[dwStringSize] - MML string
} IMAGESTOREHDRJPG, *PIMAGESTOREHDRJPG;


// ISMULTITHREAD- Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   DWORD          dwPass;     // depending upon the pass type

   // for Surround360Set()
   DWORD          dwc;
   fp             fSize;
   fp             fCircular;
   fp             fCurveScaleAngle;
   fp             fCurveScale;
   fp             fCurvature;
   fp             fScaleLong;
   fp             fScaleLat;
   fp             fAddLat;
   PCMatrix       pmRot;

   // for CacheSurround360
   // DWORD          dwc;
   PCImageStore   pis;
   fp             fLong;

   // Init() with cheezy upsample
   PCImage        pImage;
   PCFImage       pFImage;
   BOOL           f360;
   BOOL           fTransparentToBlack;
} ISMULTITHREAD, *PISMULTITHREAD;


// BUGFIX - Made an array so the global cache would actually work
static CMem agMem360[NUMISCACHE];             // globals to store last 360 calcs so faster
static DWORD agdw360Width[NUMISCACHE], agdw360Height[NUMISCACHE], agdw360Half[NUMISCACHE],
   agdw360WidthSrc[NUMISCACHE], agdw360HeightSrc[NUMISCACHE];
static fp agf360Lat[NUMISCACHE], agf360FOV[NUMISCACHE];
static fp agfCurvature[NUMISCACHE];


/*************************************************************************************
CImageStore::Constructor and destructor
*/
CImageStore::CImageStore (void)
{
   // init gamma just in case
   // GammaInit ();

   m_fTransient = FALSE;
   m_dwStretch = 2;
   m_dwWidth = m_dwHeight = 0;
   m_pNode = NULL;

   DWORD dwc;
   for (dwc = 0; dwc < NUMISCACHE; dwc++) {
      m_af360FOV[dwc] = m_af360Lat[dwc] = 0;
      m_adw360Width[dwc] = m_adw360Height[dwc] = m_adw360Half[dwc] = 0;
      m_afCurvature[dwc] = 0;
   }
   m_dwRefCount = 1;

   memset (m_aISCACHE, 0, sizeof(m_aISCACHE));
}

CImageStore::~CImageStore (void)
{
   CacheClear();

   if (m_pNode)
      delete m_pNode;
}


/*************************************************************************************
CImageStore::Release - This decreases the reference count. If the new count
reaches 0 then it deletes itself
*/
void CImageStore::Release(void)
{
   if (m_dwRefCount <= 1)
      delete this;
   else
      m_dwRefCount--;
}


/*************************************************************************************
CImageStore::CacheClear - Clear anything that's cached.

inputs
   DWORD       dwItem - Item number (0..NUMISCACHE-1) to clera, or -1 if all
*/
void CImageStore::CacheClear (DWORD dwItem)
{
   DWORD i;
   DWORD dwMax = (dwItem == (DWORD)-1) ? NUMISCACHE : (dwItem+1);
   for (i = (dwItem == (DWORD)-1) ? 0 : dwItem; i < dwMax; i++) {
      if (m_aISCACHE[i].pis) {
         delete m_aISCACHE[i].pis;
         m_aISCACHE[i].pis = NULL;
      }
      if (m_aISCACHE[i].hBmp) {
         DeleteObject (m_aISCACHE[i].hBmp);
         m_aISCACHE[i].hBmp = NULL;
      }
   } // i
}

/*************************************************************************************
CImageStore::Init - initialize a blank (albeit with random values) image.

inputs
   DWORD          dwWidth - Width in pixels
   DWORD          dwHeight - Height in pixels
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::Init (DWORD dwWidth, DWORD dwHeight)
{
   // free up old stuff
   if (m_pNode)
      delete m_pNode;
   m_pNode = NULL;
   m_fTransient = FALSE;
   m_dwStretch = 2;
   m_dwWidth = dwWidth;
   m_dwHeight = dwHeight;

   CacheClear();

   DWORD dwc;
   for (dwc = 0; dwc < NUMISCACHE; dwc++)
      m_amem360[dwc].m_dwCurPosn = 0;

   return m_memRGB.Required (m_dwWidth * m_dwHeight * 3);
}


/*************************************************************************************
CImageStore::Init - Initialize the image from a bitmap

inputs
   HBITMAP        hBit - Bitmap to use
   BOOL           fDontAllowBlack - Don't allow black color because will be transparent
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::Init (HBITMAP hBit, BOOL fDontAllowBlack)
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
   COLORREF cr;
   for (x = 0; x < dwSize; x++, pdw++) {
      cr = (COLORREF)(*pdw);
      // check for black
      if (fDontAllowBlack && (cr == LAYEREDTRANSPARENTCOLOR))
         cr = LAYEREDTRANSPARENTCOLORREMAP;

      *(pab++) = GetBValue (cr);
      *(pab++) = GetGValue (cr);
      *(pab++) = GetRValue (cr);
   }


   return TRUE;

slowway:
   // IMPORTANT: I had planned on using getpixel, but I don't really seem to
   // need this. GetDIBits works perfectly well, converting everything to 32 bits
   // color, even if its a monochrome bitmap or whatnot

   return FALSE;
}



#define CHEEZEWINDOWSIZE         5     // 5 pixels, 2 on left, 1 in center, 2 on right
#define CHEEZEWINDOWSIZEMID      ((CHEEZEWINDOWSIZE-1)/2)


/*************************************************************************************
CheezyUpsampleWeights - Precalculates the weights at each of the upsample positions,
for each of the pixels

inputs
   DWORD          adwWeight[4][5][5] - [Corner][VertPixel][HorzPixel].
                     Filled in
                     Corner = 0 for UL, 1 for UR, 2 for LR, 3 for LL
                     VertPixel and HorzPixel range from 0 to 5, for -2 to 2 offset
returns
   none
*/
void CheezyUpsampleWeights (DWORD adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE])
{
   // this creates a tent/cone filter

   DWORD dwCorner;
   POINT pCorner;
   memset (adwWeight, 0, sizeof(DWORD) * 4 * CHEEZEWINDOWSIZE * CHEEZEWINDOWSIZE);
   for (dwCorner = 0; dwCorner < 4; dwCorner++) {
      pCorner.x = pCorner.y = 0;
      if ((dwCorner == 1) || (dwCorner == 2))
         pCorner.x = 1; // extra
      if ((dwCorner == 2) || (dwCorner == 3))
         pCorner.y = 1; // extra

      int iX, iY;
      int iX5, iY5;
      for (iY = -CHEEZEWINDOWSIZE; iY <= CHEEZEWINDOWSIZE; iY++) {
         iY5 = iY + pCorner.y + CHEEZEWINDOWSIZEMID*2;
         if (iY5 < 0)
            continue;   // out of range
         iY5 /= 2;   // since two simulated per pixel
         if (iY5 >= CHEEZEWINDOWSIZE)
            continue;   // out of range

         for (iX = -CHEEZEWINDOWSIZE; iX <= CHEEZEWINDOWSIZE; iX++) {
            iX5 = iX + pCorner.x + CHEEZEWINDOWSIZEMID*2;
            if (iX5 < 0)
               continue;   // out of range
            iX5 /= 2;   // since two simulated per pixel
            if (iX5 >= CHEEZEWINDOWSIZE)
               continue;   // out of range

            // distance
            double fDistance = CHEEZEWINDOWSIZE - sqrt((double)(iX * iX + iY * iY));
            if (fDistance <= 0)
               continue;   // too far
            adwWeight[dwCorner][iY5][iX5] += (DWORD)(fDistance+0.5);
         } // iX
      } // iY
   } // dwCorner
}

/*************************************************************************************
CheezyUpsample - Upsamples a pixel using cheezy algorithms.

inputs
   PCImage        pImage - Image
   BOOL           fTransparentToBlack - If TRUE then transparent (background) is black
   DWORD          dwWidth - Width of pImage, in pixels
   DWORD          dwHeight - Height of pImage, in pixels
   BOOL           f360 - Set to TRUE if this a 360-degree image and wrap left/right
   DWORD          dwX - X pixel
   DWORD          dwY - Y pixel
   BYTE           *pabTop - Where top two pixels are to be written, R1, G1, B1, R1, G1, B1
   BYTE           *pabBottom - Where bottom two pixels are written
   DWORD          adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE] - Precalculated weights
*/
__inline void CheezyUpsample (PCImage pImage, BOOL fTransparentToBlack, DWORD dwWidth, DWORD dwHeight, BOOL f360, DWORD dwX, DWORD dwY,
                              BYTE *pabTop, BYTE *pabBottom, DWORD adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE])
{
   PIMAGEPIXEL    apip[CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE]; // [y][x]

   // figure out which pixel this is at, center vertical line
   apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID] = pImage->Pixel(dwX, dwY);

   // to the left
   DWORD i, j;
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwX > i)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i-1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i] - 1;
      else if (f360)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i-1] = pImage->Pixel(dwX + dwWidth - i - 1, dwY);
      else
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i-1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i];   // keep the same
   } // i

   // to the right
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwX + i + 1 < dwWidth)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i+1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i] + 1;
      else if (f360)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i+1] = pImage->Pixel(dwX + i + 1 - dwWidth, dwY);
      else
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i+1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i];   // keep the same
   } // i

   // above
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwY <= i) {
         // nothing above so just copy
         memcpy (apip[CHEEZEWINDOWSIZEMID-i-1], apip[CHEEZEWINDOWSIZEMID-i], sizeof(apip[CHEEZEWINDOWSIZEMID-i]));
         continue;
      }

      // else, with offset
      for (j = 0; j < CHEEZEWINDOWSIZE; j++)
         apip[CHEEZEWINDOWSIZEMID-i-1][j] = apip[CHEEZEWINDOWSIZEMID-i][j] - dwWidth;
   } // i

   // below
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwY+i+1 >= dwHeight) {
         // nothing above so just copy
         memcpy (apip[CHEEZEWINDOWSIZEMID+i+1], apip[CHEEZEWINDOWSIZEMID+i], sizeof(apip[CHEEZEWINDOWSIZEMID+i]));
         continue;
      }

      // else, with offset
      for (j = 0; j < CHEEZEWINDOWSIZE; j++)
         apip[CHEEZEWINDOWSIZEMID+i+1][j] = apip[CHEEZEWINDOWSIZEMID+i][j] + dwWidth;
   } // i


   // loop around the four corners
   DWORD dwCorner;
   DWORD x,y;
   BYTE *pabWrite;
   DWORD adwObjectID[CHEEZEWINDOWSIZE*CHEEZEWINDOWSIZE], 
      adwObjectCount[CHEEZEWINDOWSIZE*CHEEZEWINDOWSIZE], 
      adwNearMapToObjectIndex[CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE];
   DWORD dwNumObjects;
   int aiColorSum[3];
   DWORD dwColorWeight;
   for (dwCorner = 0; dwCorner < 4; dwCorner++) {
      switch (dwCorner) {
      case 0: // UL
         pabWrite = pabTop;
         break;
      case 1: // UR
         pabWrite = pabTop + 3;
         break;
      case 2: // LR
         pabWrite = pabBottom + 3;
         break;
      case 3: // LL
         pabWrite = pabBottom;
         break;
      } // dwCorner

      // figure out all the unique objects
      dwNumObjects = 0;
      DWORD dwTransparent = 0;
      for (y = 0; y < CHEEZEWINDOWSIZE; y++) for (x = 0; x < CHEEZEWINDOWSIZE; x++) {
         for (j = 0; j < dwNumObjects; j++)
            if (adwObjectID[j] == apip[y][x]->dwID)
               break;
         if (j < dwNumObjects) {
            // found existing object number, so add to that
            adwNearMapToObjectIndex[y][x] = j;
            adwObjectCount[j] += adwWeight[dwCorner][y][x];
         }
         else {
            // didn't find, so new
            adwNearMapToObjectIndex[y][x] = dwNumObjects;
            adwObjectCount[dwNumObjects] = adwWeight[dwCorner][y][x];
            adwObjectID[dwNumObjects] = apip[y][x]->dwID;
            dwNumObjects++;
         }

         if (fTransparentToBlack && !apip[y][x]->dwID)
            dwTransparent++;
      } // y,x
      if (dwTransparent < CHEEZEWINDOWSIZE * CHEEZEWINDOWSIZE)
         dwTransparent = FALSE;
      else
         dwTransparent = TRUE;

      // color
      dwColorWeight = 0;
      aiColorSum[0] = aiColorSum[1] = aiColorSum[2] = 0;
      for (y = 0; y < CHEEZEWINDOWSIZE; y++) for (x = 0; x < CHEEZEWINDOWSIZE; x++) {
         DWORD dwWeight = adwWeight[dwCorner][y][x];
         dwWeight *= adwObjectCount[adwNearMapToObjectIndex[y][x]];  // the more times an object occurs, the more important

         dwColorWeight += dwWeight;
         aiColorSum[0] += (int)apip[y][x]->wRed * (int)dwWeight;
         aiColorSum[1] += (int)apip[y][x]->wGreen * (int)dwWeight;
         aiColorSum[2] += (int)apip[y][x]->wBlue * (int)dwWeight;
      } // y, x

      // write out
      pabWrite[0] = UnGamma((WORD)(aiColorSum[0] / (int)dwColorWeight));
      pabWrite[1] = UnGamma((WORD)(aiColorSum[1] / (int)dwColorWeight));
      pabWrite[2] = UnGamma((WORD)(aiColorSum[2] / (int)dwColorWeight));

      if (dwTransparent) {
         pabWrite[0] = GetRValue(LAYEREDTRANSPARENTCOLOR);
         pabWrite[1] = GetGValue(LAYEREDTRANSPARENTCOLOR);
         pabWrite[2] = GetBValue(LAYEREDTRANSPARENTCOLOR);
      }
      else if (fTransparentToBlack && (RGB(pabWrite[0], pabWrite[1], pabWrite[2]) == LAYEREDTRANSPARENTCOLOR)) {
         pabWrite[0] = GetRValue(LAYEREDTRANSPARENTCOLORREMAP);
         pabWrite[1] = GetGValue(LAYEREDTRANSPARENTCOLORREMAP);
         pabWrite[2] = GetBValue(LAYEREDTRANSPARENTCOLORREMAP);
      }

      // to test
      //if (dwCorner == 0) {
      //   pabWrite[0] = 0xff;
      //   pabWrite[1] = pabWrite[2] = 0;
      //}
   } // dwCorner
}


/*************************************************************************************
CheezyUpsample - Upsamples a pixel using cheezy algorithms.

inputs
   PCImage        pImage - Image
   BOOL           fTransparentToBlack - If TRUE then transparent (background) is black
   DWORD          dwWidth - Width of pImage, in pixels
   DWORD          dwHeight - Height of pImage, in pixels
   BOOL           f360 - Set to TRUE if this a 360-degree image and wrap left/right
   DWORD          dwX - X pixel
   DWORD          dwY - Y pixel
   BYTE           *pabTop - Where top two pixels are to be written, R1, G1, B1, R1, G1, B1
   BYTE           *pabBottom - Where bottom two pixels are written
   DWORD          adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE] - Precalculated weights
*/
__inline void CheezyUpsample (PCFImage pImage, BOOL fTransparentToBlack, DWORD dwWidth, DWORD dwHeight, BOOL f360, DWORD dwX, DWORD dwY,
                              BYTE *pabTop, BYTE *pabBottom, DWORD adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE])
{
   PFIMAGEPIXEL    apip[CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE]; // [y][x]

   // figure out which pixel this is at, center vertical line
   apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID] = pImage->Pixel(dwX, dwY);

   // to the left
   DWORD i, j;
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwX > i)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i-1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i] - 1;
      else if (f360)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i-1] = pImage->Pixel(dwX + dwWidth - i - 1, dwY);
      else
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i-1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID-i];   // keep the same
   } // i

   // to the right
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwX + i + 1 < dwWidth)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i+1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i] + 1;
      else if (f360)
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i+1] = pImage->Pixel(dwX + i + 1 - dwWidth, dwY);
      else
         apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i+1] = apip[CHEEZEWINDOWSIZEMID][CHEEZEWINDOWSIZEMID+i];   // keep the same
   } // i

   // above
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwY <= i) {
         // nothing above so just copy
         memcpy (apip[CHEEZEWINDOWSIZEMID-i-1], apip[CHEEZEWINDOWSIZEMID-i], sizeof(apip[CHEEZEWINDOWSIZEMID-i]));
         continue;
      }

      // else, with offset
      for (j = 0; j < CHEEZEWINDOWSIZE; j++)
         apip[CHEEZEWINDOWSIZEMID-i-1][j] = apip[CHEEZEWINDOWSIZEMID-i][j] - dwWidth;
   } // i

   // below
   for (i = 0; i < CHEEZEWINDOWSIZEMID; i++) {
      if (dwY+i+1 >= dwHeight) {
         // nothing above so just copy
         memcpy (apip[CHEEZEWINDOWSIZEMID+i+1], apip[CHEEZEWINDOWSIZEMID+i], sizeof(apip[CHEEZEWINDOWSIZEMID+i]));
         continue;
      }

      // else, with offset
      for (j = 0; j < CHEEZEWINDOWSIZE; j++)
         apip[CHEEZEWINDOWSIZEMID+i+1][j] = apip[CHEEZEWINDOWSIZEMID+i][j] + dwWidth;
   } // i


   // loop around the four corners
   DWORD dwCorner;
   DWORD x,y;
   BYTE *pabWrite;
   DWORD adwObjectID[CHEEZEWINDOWSIZE*CHEEZEWINDOWSIZE], 
      adwObjectCount[CHEEZEWINDOWSIZE*CHEEZEWINDOWSIZE], 
      adwNearMapToObjectIndex[CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE];
   DWORD dwNumObjects;
   fp aiColorSum[3];
   DWORD dwColorWeight;
   for (dwCorner = 0; dwCorner < 4; dwCorner++) {
      switch (dwCorner) {
      case 0: // UL
         pabWrite = pabTop;
         break;
      case 1: // UR
         pabWrite = pabTop + 3;
         break;
      case 2: // LR
         pabWrite = pabBottom + 3;
         break;
      case 3: // LL
         pabWrite = pabBottom;
         break;
      } // dwCorner

      // figure out all the unique objects
      dwNumObjects = 0;
      DWORD dwTransparent = 0;
      for (y = 0; y < CHEEZEWINDOWSIZE; y++) for (x = 0; x < CHEEZEWINDOWSIZE; x++) {
         for (j = 0; j < dwNumObjects; j++)
            if (adwObjectID[j] == apip[y][x]->dwID)
               break;
         if (j < dwNumObjects) {
            // found existing object number, so add to that
            adwNearMapToObjectIndex[y][x] = j;
            adwObjectCount[j] += adwWeight[dwCorner][y][x];
         }
         else {
            // didn't find, so new
            adwNearMapToObjectIndex[y][x] = dwNumObjects;
            adwObjectCount[dwNumObjects] = adwWeight[dwCorner][y][x];
            adwObjectID[dwNumObjects] = apip[y][x]->dwID;
            dwNumObjects++;
         }

         if (fTransparentToBlack && !apip[y][x]->dwID)
            dwTransparent++;
      } // y,x
      if (dwTransparent < CHEEZEWINDOWSIZE * CHEEZEWINDOWSIZE)
         dwTransparent = FALSE;
      else
         dwTransparent = TRUE;

      // color
      dwColorWeight = 0;
      aiColorSum[0] = aiColorSum[1] = aiColorSum[2] = 0;
      for (y = 0; y < CHEEZEWINDOWSIZE; y++) for (x = 0; x < CHEEZEWINDOWSIZE; x++) {
         DWORD dwWeight = adwWeight[dwCorner][y][x];
         dwWeight *= adwObjectCount[adwNearMapToObjectIndex[y][x]];  // the more times an object occurs, the more important

         dwColorWeight += dwWeight;
         aiColorSum[0] += apip[y][x]->fRed * (fp)dwWeight;
         aiColorSum[1] += apip[y][x]->fGreen * (fp)dwWeight;
         aiColorSum[2] += apip[y][x]->fBlue * (fp)dwWeight;
      } // y, x

      // write out
      for (i = 0; i < 3; i++) {
         aiColorSum[i] /= (fp)dwColorWeight;
         aiColorSum[i] = max(aiColorSum[i], 0);
         aiColorSum[i] = min(aiColorSum[i], (fp)0xffff);
         pabWrite[i] = UnGamma((WORD)aiColorSum[i]);
      } // i

      if (dwTransparent) {
         pabWrite[0] = GetRValue(LAYEREDTRANSPARENTCOLOR);
         pabWrite[1] = GetGValue(LAYEREDTRANSPARENTCOLOR);
         pabWrite[2] = GetBValue(LAYEREDTRANSPARENTCOLOR);
      }
      else if (fTransparentToBlack && (RGB(pabWrite[0], pabWrite[1], pabWrite[2]) == LAYEREDTRANSPARENTCOLOR)) {
         pabWrite[0] = GetRValue(LAYEREDTRANSPARENTCOLORREMAP);
         pabWrite[1] = GetGValue(LAYEREDTRANSPARENTCOLORREMAP);
         pabWrite[2] = GetBValue(LAYEREDTRANSPARENTCOLORREMAP);
      }

      // to test
      //if (dwCorner == 0) {
      //   pabWrite[0] = 0xff;
      //   pabWrite[1] = pabWrite[2] = 0;
      //}
   } // dwCorner
}


/*************************************************************************************
CImageStore::Init - Initialize the image from a CImage object

inputs
   PCImage        pImage - Image
   BOOL           fTransparentToBlack - If TRUE then convert transparent to black
   BOOL           fUpsample - Does a hackish upsampling to reduce jaggies
   BOOL           f360 - If doing upsample, this must be set to TRUE if 360 degree image
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::Init (PCImage pImage, BOOL fTransparentToBlack, BOOL fUpsample, BOOL f360)
{
   if (!Init (pImage->Width() * (fUpsample ? 2 : 1), pImage->Height() * (fUpsample ? 2 : 1)))
      return FALSE;

//    GammaInit();

   if (fUpsample) {
      // multithread this
      ISMULTITHREAD is;
      memset (&is, 0, sizeof(is));
      is.dwPass = 12;   // Init() with upsample
      is.pImage = pImage;
      is.f360 = f360;
      is.fTransparentToBlack = fTransparentToBlack;

      ThreadLoop (0, pImage->Height(), 4, &is, sizeof(is));
         // BUGFIX - Up number of passes so will time-slice better with TTS
#if 0 // old single-threaded
      // calculate the weights
      DWORD adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE];
      CheezyUpsampleWeights (adwWeight);

      DWORD dwWidth = pImage->Width();
      DWORD dwHeight = pImage->Height();
      DWORD x, y;
      PBYTE pab = (PBYTE)m_memRGB.p;
      PBYTE pabTop, pabBottom;
      for (y = 0; y < dwHeight; y++) {
         pabTop = pab + y * 2 * 2 * dwWidth * 3;
         pabBottom = pabTop + 2 * dwWidth * 3;
         for (x = 0; x < dwWidth; x++, pabTop += 6, pabBottom += 6)
            CheezyUpsample (pImage, dwWidth, dwHeight, f360, x, y,
               pabTop, pabBottom, adwWeight);
      } // y
#endif // 0

      return TRUE;
   }

   DWORD dwSize, x;
   dwSize = m_dwWidth * m_dwHeight;
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   PBYTE pab = (PBYTE)m_memRGB.p;
   COLORREF cr;
   for (x = 0; x < dwSize; x++, pip++) {
      cr = UnGamma (&pip->wRed);

      if (fTransparentToBlack) {
         if (!pip->dwID)
            cr = LAYEREDTRANSPARENTCOLOR;
         else if (cr == LAYEREDTRANSPARENTCOLOR)
            cr = LAYEREDTRANSPARENTCOLORREMAP;
      }

      *(pab++) = GetRValue(cr);
      *(pab++) = GetGValue(cr);
      *(pab++) = GetBValue(cr);
   }


   return TRUE;

}

/*************************************************************************************
CImageStore::Init - Initialize the image from a CFImage object

inputs
   PCFImage        pImage - Image
   BOOL           fTransparentToBlack - If TRUE then convert transparent to black
   BOOL           fUpsample - Does a hackish upsampling to reduce jaggies
   BOOL           f360 - If doing upsample, this must be set to TRUE if 360 degree image
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::Init (PCFImage pImage, BOOL fTransparentToBlack, BOOL fUpsample, BOOL f360)
{
   if (!Init (pImage->Width() * (fUpsample ? 2 : 1), pImage->Height() * (fUpsample ? 2 : 1)))
      return FALSE;

//    GammaInit();

   if (fUpsample) {
      // multithread this
      ISMULTITHREAD is;
      memset (&is, 0, sizeof(is));
      is.dwPass = 13;   // Init() with upsample
      is.pFImage = pImage;
      is.f360 = f360;
      is.fTransparentToBlack = fTransparentToBlack;

      ThreadLoop (0, pImage->Height(), 4, &is, sizeof(is));
         // BUGFIX - Up number of passes so will time-slice better with TTS
#if 0 // old single-threaded
      // calculate the weights
      DWORD adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE];
      CheezyUpsampleWeights (adwWeight);

      DWORD dwWidth = pImage->Width();
      DWORD dwHeight = pImage->Height();
      DWORD x, y;
      PBYTE pab = (PBYTE)m_memRGB.p;
      PBYTE pabTop, pabBottom;
      for (y = 0; y < dwHeight; y++) {
         pabTop = pab + y * 2 * 2 * dwWidth * 3;
         pabBottom = pabTop + 2 * dwWidth * 3;
         for (x = 0; x < dwWidth; x++, pabTop += 6, pabBottom += 6)
            CheezyUpsample (pImage, dwWidth, dwHeight, f360, x, y,
               pabTop, pabBottom, adwWeight);
      } // y
#endif

      return TRUE;
   }

   // GammaInit();

   DWORD dwSize, x;
   dwSize = m_dwWidth * m_dwHeight;
   PFIMAGEPIXEL pip = pImage->Pixel(0,0);
   PBYTE pab = (PBYTE)m_memRGB.p;
   float fR, fG, fB;
   COLORREF cr;
   for (x = 0; x < dwSize; x++, pip++) {
      fR = max(min(pip->fRed, (float)0xffff), 0);
      fG = max(min(pip->fGreen, (float)0xffff), 0);
      fB = max(min(pip->fBlue, (float)0xffff), 0);

      cr = RGB (UnGamma ((WORD)fR), UnGamma ((WORD)fG), UnGamma ((WORD)fB) );

      if (fTransparentToBlack) {
         if (!pip->dwID)
            cr = LAYEREDTRANSPARENTCOLOR;
         else if (cr == LAYEREDTRANSPARENTCOLOR)
            cr = LAYEREDTRANSPARENTCOLORREMAP;
      }

      *(pab++) = GetRValue(cr);
      *(pab++) = GetGValue(cr);
      *(pab++) = GetBValue(cr);
   }


   return TRUE;

}



/*************************************************************************************
CImageStore::ToBitmap - Creates a bitmap (compatible to the given HDC) with the size
of the image. THen, ungammas the image and copies it all into the bitmap.

inputs
   HDC      hDC - Create a bitmap compatible to this
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CImageStore::ToBitmap (HDC hDC)
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
CImageStore::Init - Initialize from an image file.

inputs
   PWSTR          pszFile - .jpg or .bmp file
   BOOL           fDontAllowBlack - Don't allow black color because will be transparent
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::Init (PWSTR pszFile, BOOL fDontAllowBlack)
{
   char szFile[512];
   HBITMAP hBit;
   if (wcslen(pszFile) > 250)
      return FALSE; // too long
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile, sizeof(szFile), 0, 0);
   hBit = JPegOrBitmapLoad (szFile, FALSE);
   if (!hBit)
      return FALSE;

   if (!Init (hBit, fDontAllowBlack)) {
      DeleteObject (hBit);
      return FALSE;
   }
   DeleteObject (hBit);
   return TRUE;
}


/*************************************************************************************
CImageStore::MMLSet - Every imageStore object can contain extra MML information for
storing other attributes about the image. Calling MMLSet() deletes the existing one
and replaces it with a new one.

inputs
   PCMMLNode2         pNode - This is cloned and the clone is stored
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::MMLSet (PCMMLNode2 pNode)
{
   if (m_pNode)
      delete m_pNode;
   m_pNode = NULL;
   if (!pNode)
      return TRUE;

   m_pNode = pNode->Clone();
   if (!m_pNode)
      return FALSE;
   return TRUE;
}


/*************************************************************************************
CImageStore::MMLGet - Returns a pointer to the node. You can modify this, except
you can't delete the root (returned node). This might end up being NULL.
*/
PCMMLNode2 CImageStore::MMLGet (void)
{
   return m_pNode;
}


/*************************************************************************************
CImageStore::Width - Returns the width
*/
DWORD CImageStore::Width (void)
{
   return m_dwWidth;
}

/*************************************************************************************
CImageStore::Height - Returns the height
*/
DWORD CImageStore::Height (void)
{
   return m_dwHeight;
}



/*************************************************************************************
CImageStore::CloneTo - Clones to another megafile.

NOTE: Doesn't clone the cached images and whatnot
inputs
   PCMegaFile        pTo - Clone to
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::CloneTo (CImageStore *pTo)
{
   pTo->m_dwStretch = m_dwStretch;
   pTo->m_dwWidth = m_dwWidth;
   pTo->m_dwHeight = m_dwHeight;

   if (pTo->m_pNode)
      delete pTo->m_pNode;
   pTo->m_pNode = m_pNode ? m_pNode->Clone() : NULL;

   DWORD dwNeed = m_dwWidth * m_dwHeight * 3;
   if (!pTo->m_memRGB.Required (dwNeed))
      return FALSE;
   memcpy (pTo->m_memRGB.p, m_memRGB.p, dwNeed);
   pTo->m_memRGB.m_dwCurPosn = dwNeed;

   return TRUE;
}

/*************************************************************************************
CImageStore::ToBinary - Converts this image to a binary version.

inputs
   BOOL              fJPEG - If TRUE then compress to JPEG
   PCMem             pMem - To write to. m_dwCurPosn is size
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::ToBinary (BOOL fJPEG, PCMem pMem)
{
   // BUGFIX - If there's nothing in m_memRGB then fail
   if (!m_memRGB.p)
      return FALSE;

   PIMAGESTOREHDR ph = NULL;
   PIMAGESTOREHDRJPG phj = NULL;
   if (fJPEG) {
      if (!pMem->Required (sizeof(IMAGESTOREHDRJPG)))
         return FALSE;
      pMem->m_dwCurPosn = sizeof(IMAGESTOREHDRJPG);
      phj = (PIMAGESTOREHDRJPG)pMem->p;
      memset (phj, 0, sizeof(*phj));
      phj->dwVersionID = IMAGESTOREHDRJPEG_VERSION;
      phj->dwStretch = m_dwStretch;
      phj->dwWidth = m_dwWidth;
      phj->dwHeight = m_dwHeight;
      // NOTE: NOT saving m_fTransient
   }
   else {
      if (!pMem->Required (sizeof(IMAGESTOREHDR)))
         return FALSE;
      pMem->m_dwCurPosn = sizeof(IMAGESTOREHDR);
      ph = (PIMAGESTOREHDR)pMem->p;
      ph->dwStretch = m_dwStretch;
      ph->dwWidth = m_dwWidth;
      ph->dwHeight = m_dwHeight;
      // NOTE: NOT saving m_fTransient
   }

   if (fJPEG) {
      HWND hWnd = GetDesktopWindow();
      HDC hDC = GetDC (hWnd);

      CMem memJPEG;
      HBITMAP hBmp = ToBitmap (hDC);
      if (!hBmp) {
         ReleaseDC (hWnd, hDC);
         return FALSE;
      }
      if (!BitmapToJPeg (hBmp, &memJPEG)) {
         DeleteObject (hBmp);
         ReleaseDC (hWnd, hDC);
         return FALSE;
      }
      DeleteObject (hBmp);
      if (!pMem->Required (pMem->m_dwCurPosn + memJPEG.m_dwCurPosn)) {
         ReleaseDC (hWnd, hDC);
         return FALSE;
      }
      phj = (PIMAGESTOREHDRJPG)pMem->p;
      memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, memJPEG.p, memJPEG.m_dwCurPosn);
      pMem->m_dwCurPosn += memJPEG.m_dwCurPosn;
      phj->dwImageSize = (DWORD) memJPEG.m_dwCurPosn;

      // backup existing RGB and create a mask
      if (m_dwStretch != 4) {
         CMem memMask;
         PBYTE pabRGB = (PBYTE)m_memRGB.p;
         if (!memMask.Required (m_memRGB.m_dwCurPosn)) {
            ReleaseDC (hWnd, hDC);
            return FALSE;
         }
         memcpy (memMask.p, pabRGB, m_memRGB.m_dwCurPosn);

         DWORD dwNum = m_dwWidth * m_dwHeight;
         DWORD i;
         for (i = 0; i < dwNum; i++, pabRGB += 3)
            if (pabRGB[0] || pabRGB[1] || pabRGB[2])
               pabRGB[0] = pabRGB[1] = pabRGB[2] = 0xff;

         // convert this mask to a bitmap
         HBITMAP hBmp = ToBitmap (hDC);
         pabRGB = (PBYTE)m_memRGB.p;
         memcpy (pabRGB, memMask.p, m_memRGB.m_dwCurPosn);  // copy back
         if (!hBmp) {
            ReleaseDC (hWnd, hDC);
            return FALSE;
         }
         if (!BitmapToJPeg (hBmp, &memJPEG)) {
            DeleteObject (hBmp);
            ReleaseDC (hWnd, hDC);
            return FALSE;
         }
         DeleteObject (hBmp);
         if (!pMem->Required (pMem->m_dwCurPosn + memJPEG.m_dwCurPosn)) {
            ReleaseDC (hWnd, hDC);
            return FALSE;
         }
         phj = (PIMAGESTOREHDRJPG)pMem->p;
         memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, memJPEG.p, memJPEG.m_dwCurPosn);
         pMem->m_dwCurPosn += memJPEG.m_dwCurPosn;
         phj->dwMaskSize = (DWORD) memJPEG.m_dwCurPosn;
      }

      // done with jpeg compresseion
      ReleaseDC (hWnd, hDC);
   }
   else {
      // compress the data, but only for non-360
      if (m_dwStretch == 4) {
         DWORD dwNeed  = m_dwWidth * m_dwHeight * 3;
         if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
            return FALSE;
         memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, m_memRGB.p, dwNeed);
         pMem->m_dwCurPosn += dwNeed;
      }
      else {
         if (RLEEncode ((PBYTE)m_memRGB.p, m_dwWidth * m_dwHeight, 3, pMem))
            return FALSE;
      }
   }

   // WORD align
   if (pMem->m_dwCurPosn & 0x01) {
      if (!pMem->Required (pMem->m_dwCurPosn+1))
         return FALSE;
      pMem->m_dwCurPosn++;

      if (fJPEG)
         phj->dwFillSize++;
   }


   // convert MML
   if (m_pNode) {
      size_t dwCurPosn = pMem->m_dwCurPosn;

      if (!MMLToMem (m_pNode, pMem, FALSE))
         return FALSE;
      pMem->CharCat (0);  // NULL termainte

      if (fJPEG) {
         phj = (PIMAGESTOREHDRJPG)pMem->p;
         phj->dwStringSize = (DWORD)(pMem->m_dwCurPosn - dwCurPosn);
      }
   }

   // done
   return TRUE;
}


/*************************************************************************************
CImageStore::ToMegaFile - Writes the data out to a megafile.

inputs
   PCMegaFile        pmf - Megafile to write to
   PWSTR             pszName - Name to save as
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::ToMegaFile (PCMegaFile pmf, PWSTR pszName)
{
   CMem mem;
   if (!ToBinary (FALSE, &mem))
      return FALSE;

   // write this out
   return pmf->Save (pszName, mem.p, mem.m_dwCurPosn);
}


/*************************************************************************************
CImageStore::InitFromBinary - Initializes is from binary data, created by
ToBinary().

inputs
   BOOL              fJPEG - TRUE if it's supposed to be JPEG
   PBYTE             pMem - Mem to get from
   __int64           iSize - Number of bytes in pMem
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::InitFromBinary (BOOL fJPEG, PBYTE pMem, __int64 iSize)
{
   PIMAGESTOREHDR ph = NULL;
   PIMAGESTOREHDRJPG phj = NULL;
   if (fJPEG) {
      if (iSize < sizeof(IMAGESTOREHDRJPG))
         return FALSE;
      phj = (PIMAGESTOREHDRJPG)pMem;
      iSize -= sizeof(IMAGESTOREHDRJPG);
      pMem += sizeof(IMAGESTOREHDRJPG);

      if (phj->dwVersionID != IMAGESTOREHDRJPEG_VERSION)
         return FALSE;
   }
   else {
      if (iSize < sizeof(IMAGESTOREHDR))
         return FALSE;
      ph = (PIMAGESTOREHDR)pMem;
      iSize -= sizeof(IMAGESTOREHDR);
      pMem += sizeof(IMAGESTOREHDR);
   }

   // create blank
   if (fJPEG) {
      // must be large enough
      if (iSize < phj->dwImageSize)
         return FALSE;

      // convert from jpeg to bitmap
      HBITMAP hBmp = JPegToBitmap (pMem, phj->dwImageSize);
      if (!hBmp)
         return FALSE;

      // initialize this
      if (!Init (hBmp, FALSE)) {
         DeleteObject (hBmp);
         return FALSE;
      }
      DeleteObject (hBmp);
      iSize -= phj->dwImageSize;
      pMem += phj->dwImageSize;

      // mask?
      if (phj->dwMaskSize) {
         CImageStore ImageTemp;

         if (iSize < phj->dwMaskSize)
            return FALSE;

         hBmp = JPegToBitmap (pMem, phj->dwMaskSize);
         if (!hBmp)
            return FALSE;

         // initialize this
         if (!ImageTemp.Init (hBmp, FALSE)) {
            DeleteObject (hBmp);
            return FALSE;
         }
         DeleteObject (hBmp);
         iSize -= phj->dwMaskSize;
         pMem += phj->dwMaskSize;

         // loop over and set to absoulte black
         PBYTE pabImage = (PBYTE)m_memRGB.p;
         PBYTE pabMask = (PBYTE)ImageTemp.m_memRGB.p;
         DWORD dwNum = m_dwWidth * m_dwHeight;
         if (dwNum != ImageTemp.m_dwWidth * m_dwHeight)
            return FALSE;  // shouldnt happen
         DWORD i;
         for (i = 0; i < dwNum; i++, pabImage += 3, pabMask += 3) {
            // if masked to black here then set to black
            if (pabMask[0] < 0x80)
               pabImage[0] = pabImage[1] = pabImage[2] = 0;
            else if (!pabImage[0] && !pabImage[1] && !pabImage[2])
               pabImage[0] = pabImage[1] = pabImage[2] = 1; // since not masked, can't be absolute black
         }
      }
      else {
         // make sure there's no absolute black in the image
         PBYTE pabImage = (PBYTE)m_memRGB.p;
         DWORD dwNum = m_dwWidth * m_dwHeight;
         DWORD i;
         for (i = 0; i < dwNum; i++, pabImage += 3) {
            // if masked to black here then set to black
            if (!pabImage[0] && !pabImage[1] && !pabImage[2])
               pabImage[0] = pabImage[1] = pabImage[2] = 1; // since not masked, can't be absolute black
         }
      }
   } // if jPEG
   else {
      if (!Init (ph->dwWidth, ph->dwHeight))
         return FALSE;
   }
   m_dwStretch = ph ? ph->dwStretch : phj->dwStretch;

   if (fJPEG) {
      if (iSize < phj->dwFillSize)
         return FALSE;
      iSize -= phj->dwFillSize;
      pMem += phj->dwFillSize;
   }
   else {
      // read in, expecting compressed for all but 360 degree images
      size_t dwUsed;
      m_memRGB.m_dwCurPosn = 0;
      if (m_dwStretch == 4) {
         // reading in 360. no RLE
         DWORD dwNeed = m_dwWidth * m_dwHeight * 3;
         if (iSize < dwNeed)
            return FALSE;
         if (!m_memRGB.Required (iSize))
            return FALSE;
         memcpy (m_memRGB.p, pMem, dwNeed);
         m_memRGB.m_dwCurPosn = dwNeed;
         dwUsed = dwNeed;
      }
      else {
         // reading in compressed
         if (RLEDecode (pMem, iSize, 3, &m_memRGB, &dwUsed))
            return FALSE;
      }
      pMem += dwUsed;
      iSize -= dwUsed;

      // WORD align
      if (iSize && (dwUsed & 0x01)) {
         pMem++;
         iSize--;
      }
   }

   if (m_pNode)
      delete m_pNode;
   m_pNode = NULL;
   if (fJPEG && (phj->dwStringSize >= sizeof(WCHAR))) {
      if (iSize < phj->dwStringSize)
         return FALSE;
      PWSTR pszString = (PWSTR)pMem;

      if (pszString[phj->dwStringSize/sizeof(WCHAR)-1])
         return FALSE;  // not null terminated

      m_pNode = CircumrealityParseMML (pszString);
   }
   else if (!fJPEG) {
      if (iSize >= sizeof(WCHAR))
         m_pNode = CircumrealityParseMML ((PWSTR)pMem);
   }

   return TRUE;
}

/*************************************************************************************
CImageStore::Init - Initializes from a megafile.

inputs
   PCMegaFile           pmf - Megafile
   PWSTR                pszName - Name
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::Init (PCMegaFile pmf, PWSTR pszName)
{
   __int64 iSize;
   PBYTE pabMem = (PBYTE) pmf->Load (pszName, &iSize);
   if (!pabMem)
      return FALSE;

   if (!InitFromBinary (FALSE, pabMem, iSize)) {
      MegaFileFree (pabMem);
      return FALSE;
   }
   MegaFileFree (pabMem);
   return TRUE;
}




/********************************************************************************
HermiteCubic - Cubic interpolation based on 4 points, p1..p4

inputs
   int     t - value from 0 to 0x10000, at 0, is at point p2, and at 1x10000 is at point p3.
   BYTE      bp1 - value at p1
   BYTE      bp2 - value at p2
   BYTE      bp3 - value at p3
   BYTE      bp4 - value at p4
returns
   BYTE - Value at point t
*/
__inline BYTE HermiteCubic (int t, BYTE fp1, BYTE fp2, BYTE fp3, BYTE fp4)
{
   // derivatives at the points
   int fd2, fd3;
   fd2 = ((int)fp3 - (int)fp1) / 2;
   fd3 = ((int)fp4 - (int)fp2) / 2;

   // calculte cube and square
   int t3,t2;
   t /= 256;   // lower res, so from 0..256
   t2 = (t * t) / 256;
   t3 = (t * t2) / 256;

   // done
   int iRet = (2 * t3 - 3 * t2 + 1*256) * fp2 +
      (-2 * t3 + 3 * t2) * fp3 +
      (t3 - 2 * t2 + t) * fd2 +
      (t3 - t2) * fd3;
   iRet /= 256;

   if (iRet < 0)
      return 0;
   else if (iRet > 255)
      return 255;
   else
      return (BYTE) iRet;
}


/********************************************************************************
HermiteCubicTwoD - Cubic interpolation based on 4 points, p1..p4

inputs
   int      iX - value from 0 to 0x10000, at 0, is at point p2, and at 1x10000 is at point p3.
   int      iY - Y value, like iX
   BYTE     pab[4][4];  // [y][x]
returns
   BYTE - Value at point iX, iY
*/
_inline BYTE HermiteCubicTwoD (int iX, int iY, BYTE pab[4][4])
{
   BYTE ab[4];
   DWORD i;
   for (i = 0; i < 4; i++)
      ab[i] = HermiteCubic (iX, pab[i][0], pab[i][1], pab[i][2], pab[i][3]);

   return HermiteCubic (iY, ab[0], ab[1], ab[2], ab[3]);
}

/*************************************************************************************
Surround360Pixel - Gets the value of a surroudn 360 pixel

inputs
   DWORD          dwX - Pixel in the bitmap (must be < m_adw360Width[dwc])
   DWORD          dwY - Pixel in the bitmap (must be < m_adw360Width[dwc])
   fp             fRot - Rotation in pixels (based on m_dwWidth pixels)
   int            *pai - Memory containing offset information
   DWORD          dwHalf - Half length (m_adw360Half[dwc])
   DWORD          dwBitWidth - Bitmap width
   PBYTE          *pabRGB - the image store's RGB
   DWORD          dwWidth - width of pabRGB
   DWORD          dwHeight - Height of pabRGB
returns
   COLORREF - RGB value
*/
__inline COLORREF Surround360Pixel (DWORD dwX, DWORD dwY, fp fRot, int *pai, DWORD dwHalf, DWORD dwBitWidth,
                                    PBYTE pabRGB, DWORD dwWidth, DWORD dwHeight)
{
   // see if should get value but mirror L/R
   BOOL fMirror;
   if (dwX >= dwBitWidth - dwHalf) {
      dwX -= (dwBitWidth - dwHalf);
      fMirror = FALSE;
   }
   else {
      dwX = dwHalf - dwX - 1;
      fMirror = TRUE;
   }

   // look up value in table
   pai = pai + (dwY * dwHalf + dwX)*2;

   // floating point loc
   int ifX, ifY;
   int ifWidth = (int)dwWidth << 16;
   ifX = (int)(fRot * (fp)0x10000) + pai[0] * (fMirror ? -1 : 1);
   ifY = pai[1];
   while (ifX < 0)
      ifX += ifWidth;
   while (ifX >= ifWidth)
      ifX -= ifWidth;

   // integer loc
   int iX, iY;
   iX = ifX >> 16;
   iY = ifY >> 16;

#define INTERP360
// #define INTERP360HERMITE
#ifdef INTERP360HERMITE
   ifX -= iX << 16;
   ifY -= iY << 16;
#endif // INTERP360
#ifdef INTERP360
   ifX -= iX << 16;
   ifY -= iY << 16;
#endif // INTERP360
   iY = max(iY,0);
   iY = min(iY, (int)dwHeight-1);

   // get the values and interpolate
   PBYTE pUL;
   pUL = pabRGB + (iY * dwWidth + iX)*3;

#ifdef INTERP360HERMITE
   int iXRel, iYRel, iXCur, iYCur;
   BYTE ab[3][4][4];
   PBYTE pCur;
   for (iYRel = 0; iYRel < 4; iYRel++) {
      iYCur = iY + iYRel - 1;
      if (iYCur < 0)
         iYCur = 0;
      else if (iYCur >= (int)dwHeight)
         iYCur = (int)dwHeight-1;

      pUL = pabRGB + (iYCur * dwWidth)*3;

      for (iXRel = 0; iXRel < 4; iXRel++) {
         iXCur = iX + iXRel - 1;
         if (iXCur < 0)
            iXCur += (int)dwWidth;
         else if (iXCur >= (int)dwWidth)
            iXCur -= (int)dwWidth;

         pCur = pUL + iXCur*3;
         ab[0][iYRel][iXRel] = pCur[0];
         ab[1][iYRel][iXRel] = pCur[1];
         ab[2][iYRel][iXRel] = pCur[2];
      } // iX
   } // iY

   // NOTE: not doing proper gamma correct because need to be fast
   // convert to WORDs
   DWORD i;
   BYTE abFinal[3];
   for (i = 0; i < 3; i++)
      abFinal[i] = HermiteCubicTwoD (ifX, ifY, ab[i]);

   return RGB(abFinal[0], abFinal[1], abFinal[2]);
#else
#ifdef INTERP360
   PBYTE pUR, pLL, pLR;
   pLL = (iY + 1 >= (int)dwHeight) ? pUL : (pUL + dwWidth*3);
   if (iX + 1 >= (int)dwWidth) {
      pUR = pabRGB + (iY * dwWidth)*3;
      pLR = (iY + 1 >= (int)dwHeight) ? pUR : (pUR + dwWidth*3);
   }
   else {
      // easy, just one over
      pUR = pUL + 3;
      pLR = pLL + 3;
   }

   // NOTE: not doing proper gamma correct because need to be fast
   // convert to WORDs
   BYTE ab[2][2][3];
   DWORD i, j;
   for (i = 0; i < 3; i++) {
      ab[0][0][i] = pUL[i];
      ab[0][1][i] = pUR[i];
      ab[1][0][i] = pLL[i];
      ab[1][1][i] = pLR[i];
   } // i

   // LR weighting
   DWORD dwAlpha = (DWORD)ifX;
   // not needed: dwAlpha = min(dwAlpha, 0x10000);
   DWORD dwAlphaInv = 0x10000 - dwAlpha;

   DWORD adw[2][3];
   for (i = 0; i < 2; i++) for (j = 0; j < 3; j++)
      adw[i][j] = ((DWORD)ab[i][0][j] * dwAlphaInv + (DWORD)ab[i][1][j] * dwAlpha) >> 16;

   // TB heighting
   BYTE abFinal[3];
   dwAlpha = (DWORD)ifY;
   // not needed: dwAlpha = min(dwAlpha, 0x10000);
   dwAlphaInv = 0x10000 - dwAlpha;
   for (i = 0; i < 3; i++)
      abFinal[i] = (BYTE)((adw[0][i] * dwAlphaInv + adw[1][i] * dwAlpha) >> 16);

   return RGB(abFinal[0], abFinal[1], abFinal[2]);
#else
   return RGB(pUL[0], pUL[1], pUL[2]);
#endif // INTERP360
#endif // !INTERP360HERMITE
}


/*************************************************************************************
CImageStore::EscMultiThreadedCallback - Multithreaded callback
*/
void CImageStore::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize, DWORD dwThread)
{
   PISMULTITHREAD pmt = (PISMULTITHREAD) pParams;

   switch (pmt->dwPass) {
   case 10: // Surround360Set
      {
         DWORD dwY, dwX;
         fp fLen;
         CPoint p, pOrig;
         pOrig.Zero();
         DWORD dwc = pmt->dwc;
         fp fSize = pmt->fSize;
         fp fCircular = pmt->fCircular;
         fp fCurveScaleAngle = pmt->fCurveScaleAngle;
         fp fCurveScale = pmt->fCurveScale;
         fp fCurvature = pmt->fCurvature;
         fp fScaleLong = pmt->fScaleLong;
         fp fScaleLat = pmt->fScaleLat;
         fp fAddLat = pmt->fAddLat;
         CMatrix mRot;
         mRot.Copy (pmt->pmRot);

         int *pai = (int*)m_amem360[dwc].p + (pmt->dwStart * m_adw360Half[dwc]*2);

         for (dwY = pmt->dwStart; dwY < pmt->dwEnd; dwY++) {
            pOrig.p[0] = (2*m_adw360Half[dwc] == m_adw360Width[dwc]) ? (1.0/2.0) : 0;
               // BUGFIX - Was * fSize, but changed units, so changed fSize to 1.0
            pOrig.p[2] = ((fp)m_adw360Height[dwc]/2.0 - (fp)dwY) * 1.0;
               // BUGFIX - Was * fSize, but changed units
            pOrig.p[1] = 1.0 / fSize;
               // BUGFIX - Was just 1.0, but changed units

            for (dwX = 0; dwX < m_adw360Half[dwc]; dwX++, pai+=2, pOrig.p[0] += 1.0) {
                     // BUGFIX - Was += fSize, but changed units

               p.Copy (&pOrig);

               // apply circular
               if (fCircular) {
                  CPoint pCircle;
                  fp fAngle = pOrig.p[0] * fCurveScaleAngle;
                  pCircle.p[0] = sin(fAngle) * fCurveScale;
                  pCircle.p[1] = cos(fAngle) * fCurveScale;
                  pCircle.p[2] = pOrig.p[2];

                  // normalize so they blend in better together
                  if (fCircular != 1.0) {
                     p.Normalize();
                     pCircle.Normalize();
                  }

                  p.Average (&pCircle, fCurvature);
               }

               // copy the point and then rotate
               p.MultiplyLeft (&mRot);

               // determine the angle
               fLen = p.Length();
               pai[0] = (int)(atan2(p.p[0], p.p[1]) * fScaleLong);
               pai[1] = (int)(asin (p.p[2] / fLen) * fScaleLat + fAddLat);
            } // dwX
         } // dwY
      }
      break;


   case 11: // CacheSurround360
      {
         // loop
         DWORD x, y;
         BYTE * pb;
         COLORREF cr;

         DWORD dwc = pmt->dwc;
         PCImageStore pis = pmt->pis;
         fp fLong = pmt->fLong;

         int *pai = (int*)m_amem360[dwc].p;
         PBYTE pabRGB = (PBYTE)m_memRGB.p;
         for (y = pmt->dwStart; y < pmt->dwEnd; y++) {
            pb = (BYTE*) pis->Pixel (0, y);
            for (x = 0; x < m_adw360Width[dwc]; x++) {
               cr = Surround360Pixel (x, y, fLong, pai, m_adw360Half[dwc], m_adw360Width[dwc], pabRGB, m_dwWidth, m_dwHeight);

               *(pb++) = GetRValue (cr);
               *(pb++) = GetGValue (cr);
               *(pb++) = GetBValue (cr);
            }
         }
      }
      break;

   case 12: // Init() with cheezy upsample
      {
         PCImage pImage = pmt->pImage;
         BOOL f360 = pmt->f360;
         BOOL fTransparentToBlack = pmt->fTransparentToBlack;

         // calculate the weights
         DWORD adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE];
         CheezyUpsampleWeights (adwWeight);

         DWORD dwWidth = pImage->Width();
         DWORD dwHeight = pImage->Height();
         DWORD x, y;
         PBYTE pab = (PBYTE)m_memRGB.p;
         PBYTE pabTop, pabBottom;
         for (y = pmt->dwStart; y < pmt->dwEnd; y++) {
            pabTop = pab + y * 2 * 2 * dwWidth * 3;
            pabBottom = pabTop + 2 * dwWidth * 3;
            for (x = 0; x < dwWidth; x++, pabTop += 6, pabBottom += 6)
               CheezyUpsample (pImage, fTransparentToBlack, dwWidth, dwHeight, f360, x, y,
                  pabTop, pabBottom, adwWeight);
         } // y
      }
      break;

   case 13: // init() with cheezy upsample, fp
      {
         PCFImage pImage = pmt->pFImage;
         BOOL f360 = pmt->f360;
         BOOL fTransparentToBlack = pmt->fTransparentToBlack;

         // calculate the weights
         DWORD adwWeight[4][CHEEZEWINDOWSIZE][CHEEZEWINDOWSIZE];
         CheezyUpsampleWeights (adwWeight);

         DWORD dwWidth = pImage->Width();
         DWORD dwHeight = pImage->Height();
         DWORD x, y;
         PBYTE pab = (PBYTE)m_memRGB.p;
         PBYTE pabTop, pabBottom;
         for (y = pmt->dwStart; y < pmt->dwEnd; y++) {
            pabTop = pab + y * 2 * 2 * dwWidth * 3;
            pabBottom = pabTop + 2 * dwWidth * 3;
            for (x = 0; x < dwWidth; x++, pabTop += 6, pabBottom += 6)
               CheezyUpsample (pImage, fTransparentToBlack, dwWidth, dwHeight, f360, x, y,
                  pabTop, pabBottom, adwWeight);
         } // y
      }
      break;      // calculate the weights


   } // pmt->dwPass
}

/*************************************************************************************
CImageStore::Surround360Set - This sets the parameters for 360 degree rendering and
calculates the table if necessary.

inputs
   DWORD       dwc - Cache number, 0.. NUMISCACHE-1
   fp          fFOV - Field of view, in radians
   fp          fLat - Latitude... 0 = level, PI/2 lookin straight up
   DWORD       dwWidth - Width of destination rect
   DWORD       dwHeight - Height of destination rect
   fp          fCurvature - Curvature of the of the eye, 0=plane, 1 = circular
returns
   BOOL - TRUE if sucess
*/
BOOL CImageStore::Surround360Set (DWORD dwc, fp fFOV, fp fLat, DWORD dwWidth, DWORD dwHeight,
                                  fp fCurvature)
{
   // make sure right type
   if ((m_dwStretch != 4) || !dwWidth || !dwHeight || (fFOV < CLOSE))
      return FALSE;
      // NOTE: Dont need to make sure curvature within 0 to 1

   // BUGFIX - Fix problem when 360-src image changed resolution and not redrawn

   DWORD dwHalf = (dwWidth + 1) / 2;
   DWORD dwNeed = dwHalf * dwHeight * 2 * sizeof(int);
   if ((m_amem360[dwc].m_dwCurPosn == dwNeed) && (fFOV == m_af360FOV[dwc]) && (m_af360Lat[dwc] == fLat) &&
      (m_adw360Width[dwc] == dwWidth) && (m_adw360Height[dwc] == dwHeight) &&
      (m_afCurvature[dwc] == fCurvature) &&
      (agdw360WidthSrc[dwc] == Width()) && (agdw360HeightSrc[dwc] == Height()) )
         return TRUE;   // already done

   // else need new allocated
   if (!m_amem360[dwc].Required (dwNeed))
      return FALSE;
   int *pai = (int*)m_amem360[dwc].p;
   m_amem360[dwc].m_dwCurPosn = dwNeed;
   m_af360FOV[dwc] = fFOV;
   m_af360Lat[dwc] = fLat;
   m_adw360Width[dwc] = dwWidth;
   m_adw360Height[dwc] = dwHeight;
   m_adw360Half[dwc] = dwHalf;
   m_afCurvature[dwc] = fCurvature;

   // if this matches the last one saved in globals then use it...
   if ((agMem360[dwc].m_dwCurPosn == dwNeed) && (fabs(fFOV - agf360FOV[dwc]) < CLOSE) && (fabs(agf360Lat[dwc] - fLat) < CLOSE) &&
      (agdw360Width[dwc] == dwWidth) && (agdw360Height[dwc] == dwHeight) && (fabs(fCurvature - agfCurvature[dwc]) < CLOSE) &&
      (agdw360WidthSrc[dwc] == Width()) && (agdw360HeightSrc[dwc] == Height()) ) {
         memcpy (m_amem360[dwc].p, agMem360[dwc].p, dwNeed);
         return TRUE;   // already done
      }

   // rotation matrix...
   CMatrix mRot;
   mRot.RotationX (fLat);

   // size of a pixel at 1 m
   fp fSize;
   fp fFOVLinear = min(m_af360FOV[dwc], PI* 0.99);
   fSize = tan(fFOVLinear/2) / ((fp)dwWidth/2);

   // loop through all the points
   // DWORD dwX, dwY;
   BOOL fCircular = (fCurvature > CLOSE);
   fp fCurveScaleAngle = 1.0 / (fp)m_adw360Width[dwc] * m_af360FOV[dwc];
   fp fCurveScale = (fp)m_adw360Width[dwc] / m_af360FOV[dwc];
   // CPoint p, pOrig;
   //pOrig.Zero();
   // fp fLen;
   fp fScaleLong = (fp)Width() / (2.0 * PI) * (fp)0x10000;
   fp fScaleLat = -(fp)Height() / PI * (fp)0x10000;
   fp fAddLat = (fp)Height() / 2.0 * (fp)0x10000;

   // multithread this
   ISMULTITHREAD is;
   memset (&is, 0, sizeof(is));
   is.dwPass = 10;   // multithread
   is.dwc = dwc;
   is.fSize = fSize;
   is.fCircular = fCircular;
   is.fCurveScaleAngle = fCurveScaleAngle;
   is.fCurveScale = fCurveScale;
   is.fCurvature = fCurvature;
   is.fScaleLong = fScaleLong;
   is.fScaleLat = fScaleLat;
   is.fAddLat = fAddLat;
   is.pmRot = &mRot;

   ThreadLoop (0, dwHeight, 1, &is, sizeof(is));

#if 0 // non-mulithreaded
   for (dwY = 0; dwY < dwHeight; dwY++) {
      pOrig.p[0] = (2*m_adw360Half[dwc] == m_adw360Width[dwc]) ? (1.0/2.0) : 0;
         // BUGFIX - Was * fSize, but changed units, so changed fSize to 1.0
      pOrig.p[2] = ((fp)m_adw360Height[dwc]/2.0 - (fp)dwY) * 1.0;
         // BUGFIX - Was * fSize, but changed units
      pOrig.p[1] = 1.0 / fSize;
         // BUGFIX - Was just 1.0, but changed units

      for (dwX = 0; dwX < m_adw360Half[dwc]; dwX++, pai+=2, pOrig.p[0] += 1.0) {
               // BUGFIX - Was += fSize, but changed units

         p.Copy (&pOrig);

         // apply circular
         if (fCircular) {
            CPoint pCircle;
            fp fAngle = pOrig.p[0] * fCurveScaleAngle;
            pCircle.p[0] = sin(fAngle) * fCurveScale;
            pCircle.p[1] = cos(fAngle) * fCurveScale;
            pCircle.p[2] = pOrig.p[2];

            // normalize so they blend in better together
            if (fCircular != 1.0) {
               p.Normalize();
               pCircle.Normalize();
            }

            p.Average (&pCircle, fCurvature);
         }

         // copy the point and then rotate
         p.MultiplyLeft (&mRot);

         // determine the angle
         fLen = p.Length();
         pai[0] = (int)(atan2(p.p[0], p.p[1]) * fScaleLong);
         pai[1] = (int)(asin (p.p[2] / fLen) * fScaleLat + fAddLat);
      } // dwX
   } // dwY
#endif // 0, non-multithreaded

   // store this away in globals for fast access later
   if (agMem360[dwc].Required (dwNeed)) {
      memcpy (agMem360[dwc].p, m_amem360[dwc].p, dwNeed);
      agMem360[dwc].m_dwCurPosn = dwNeed;
      agf360FOV[dwc] = m_af360FOV[dwc];
      agf360Lat[dwc] = m_af360Lat[dwc];
      agdw360Width[dwc] = m_adw360Width[dwc];
      agdw360Height[dwc] = m_adw360Height[dwc];
      agdw360Half[dwc] = m_adw360Half[dwc];
      agfCurvature[dwc] = m_afCurvature[dwc];
      agdw360WidthSrc[dwc] = Width();
      agdw360HeightSrc[dwc] = Height();
   }

   CacheClear();

   // done
   return TRUE;
}

/*************************************************************************************
CImageStore::Surround360Get - Returns the current values of 360.

inputs
   DWORD       dwc - Cache number, 0.. NUMISCACHE-1
   fp          *pfFOV - Field of view, in radians
   fp          *pfLat - Latitude... 0 = level, PI/2 lookin straight up
   DWORD       *pdwWidth - Width of destination rect
   DWORD       *pdwHeight - Height of destination rect
   fp          *pfCurvature - Curvature of the of the eye, 0=plane, 1 = circular
returns
   none
*/
void CImageStore::Surround360Get (DWORD dwc, fp *pfFOV, fp *pfLat, DWORD *pdwWidth, DWORD *pdwHeight,
                                  fp *pfCurvature)
{
   if (pfFOV)
      *pfFOV = m_af360FOV[dwc];
   if (pfLat)
      *pfLat = m_af360Lat[dwc];
   if (pdwWidth)
      *pdwWidth = m_adw360Width[dwc];
   if (pdwHeight)
      *pdwHeight = m_adw360Height[dwc];
   if (pfCurvature)
      *pfCurvature = m_afCurvature[dwc];
}



#if 0 // old code
/*************************************************************************************
CImageStore::Surround360ToBitmap - Fills in a bitmap based on the current
360 degree rotation calculatesion (called with Surround360Set)

inputs
   DWORD       dwc - Cache number, 0.. NUMISCACHE-1
   HDC      hDC - Create a bitmap compatible to this
   fp       fLong - Longitudinal rotation, 0 = n, PI/2 = e, etc.
            NOTE: fLat and others must be set by calling Surround360Set()
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CImageStore::Surround360ToBitmap (DWORD dwc, HDC hDC, fp fLong)
{
#if 0 //def _DEBUG
   DWORD dwDrawStart = GetTickCount();
#endif

   // GammaInit ();

   // determine info
   int *pai = (int*)m_amem360[dwc].p;
   fLong = fLong / (2.0 * PI) * (fp)Width();
   PBYTE pabRGB = (PBYTE)m_memRGB.p;

   HBITMAP hBit;
   DWORD x, y;
   BYTE * pb;
   COLORREF cr;

   int   iBitsPixel, iPlanes;
   iBitsPixel = GetDeviceCaps (hDC, BITSPIXEL);
   iPlanes = GetDeviceCaps (hDC, PLANES);
   BITMAP  bm;
   memset (&bm, 0, sizeof(bm));
   bm.bmWidth = m_adw360Width[dwc];
   bm.bmHeight = m_adw360Height[dwc];
   bm.bmPlanes = iPlanes;
   bm.bmBitsPixel = iBitsPixel;

   // first see if can do a 24-bit bitmap, then 16-bit, then do it the slow way
   if ( (iBitsPixel == 32) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = m_adw360Width[dwc] * 4;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      DWORD *pdw;
      pdw = (DWORD*) bm.bmBits;
      for (y = 0; y < m_adw360Height[dwc]; y++) for (x = 0; x < m_adw360Width[dwc]; x++, pdw++) {
         cr = Surround360Pixel (x, y, fLong, pai, m_adw360Half[dwc], m_adw360Width[dwc], pabRGB, m_dwWidth, m_dwHeight);
         *pdw = RGB(GetBValue(cr), GetGValue(cr), GetRValue(cr));
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 24) && (iPlanes == 1) ) {
      // 24 bit bitmap
      bm.bmWidthBytes = m_adw360Width[dwc] * 3;
      if (bm.bmWidthBytes % 2)
         bm.bmWidthBytes++;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      for (y = 0; y < m_dwHeight; y++) {
         pb = (BYTE*) bm.bmBits + (y * bm.bmWidthBytes);
         for (x = 0; x < m_dwWidth; x++) {
            cr = Surround360Pixel (x, y, fLong, pai, m_adw360Half[dwc], m_adw360Width[dwc], pabRGB, m_dwWidth, m_dwHeight);

            *(pb++) = GetBValue (cr);
            *(pb++) = GetGValue (cr);
            *(pb++) = GetRValue (cr);
         }
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else if ( (iBitsPixel == 16) && (iPlanes == 1) ) {
      // 16 bit bitmap
      bm.bmWidthBytes = m_adw360Width[dwc] * 2;
      bm.bmBits = ESCMALLOC(bm.bmWidthBytes * bm.bmHeight);
      if (!bm.bmBits)
         return NULL;   // can't do it

      // loop
      WORD *pw;
      pw = (WORD*) bm.bmBits;
      for (y = 0; y < m_adw360Height[dwc]; y++) for (x = 0; x < m_adw360Width[dwc]; x++, pw++) {
         cr = Surround360Pixel (x, y, fLong, pai, m_adw360Half[dwc], m_adw360Width[dwc], pabRGB, m_dwWidth, m_dwHeight);

         BYTE r,g,b;
         r = GetRValue(cr) >> 3;
         g = GetGValue(cr) >> 2;
         b = GetBValue(cr) >> 3;
         *pw = ((WORD)r << 11) | ((WORD)g << 5) | ((WORD)b);
      }

      hBit = CreateBitmapIndirect (&bm);
      ESCFREE (bm.bmBits);
   }
   else {
      // the slow way
      hBit = CreateCompatibleBitmap (hDC, (int) m_adw360Width[dwc], (int) m_adw360Height[dwc]);
      if (!hBit)
         return NULL;

      HDC hDCTemp;
      hDCTemp = CreateCompatibleDC (hDC);
      SelectObject (hDCTemp, hBit);

      // loop
      for (y = 0; y < m_adw360Height[dwc]; y++) for (x = 0; x < m_adw360Width[dwc]; x++)
         SetPixel (hDCTemp, (int)x, (int)y, 
            Surround360Pixel (x, y, fLong, pai, m_adw360Half[dwc], m_adw360Width[dwc], pabRGB, m_dwWidth, m_dwHeight));

      // done
      DeleteDC (hDCTemp);
   }

#if 0// def _DEBUG
   dwDrawStart = GetTickCount() - dwDrawStart;
   char szTemp[64];
   sprintf (szTemp, "\r\nDraw 360 frame = %d", (int)dwDrawStart);
   OutputDebugString (szTemp);
#endif
   return hBit;
}
#endif // 0



/*************************************************************************************
CImageStore::CacheSurround360 - Caches a bitmap based on the current
360 degree rotation calculatesion (called with Surround360Set)

inputs
   DWORD       dwc - Cache number, 0.. NUMISCACHE-1
   fp       fLong - Longitudinal rotation, 0 = n, PI/2 = e, etc.
            NOTE: fLat and others must be set by calling Surround360Set()
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::CacheSurround360 (DWORD dwc, fp fLong)
{
   // if it's already cached then do nothing
   if (m_aISCACHE[dwc].pis && (m_aISCACHE[dwc].fLong == fLong) &&
      (m_aISCACHE[dwc].iWidth == (int)m_adw360Width[dwc]) && 
      (m_aISCACHE[dwc].iHeight == (int)m_adw360Height[dwc]) )
      return TRUE;

   // BUGFIX - since muck with fLong, need to keep around so can write into cache
   fp fLongOrig = fLong;

#if 0 //def _DEBUG
   DWORD dwDrawStart = GetTickCount();
#endif

   // GammaInit ();

   // determine info
   // int *pai = (int*)m_amem360[dwc].p;
   fLong = fLong / (2.0 * PI) * (fp)Width();
   //PBYTE pabRGB = (PBYTE)m_memRGB.p;

   // fill in the cache
   CacheClear (dwc);
   PCImageStore pis;
   m_aISCACHE[dwc].fLong = fLongOrig;  // BUGFIX - was flong
   m_aISCACHE[dwc].iWidth = (int)m_adw360Width[dwc];
   m_aISCACHE[dwc].iHeight = (int)m_adw360Height[dwc];
   pis = m_aISCACHE[dwc].pis = new CImageStore;
   if (!pis)
      return FALSE;
   if (!pis->Init((DWORD)m_aISCACHE[dwc].iWidth, (DWORD)m_aISCACHE[dwc].iHeight)) {
      delete pis;
      m_aISCACHE[dwc].pis = NULL;
      return FALSE;
   }


   // multithread this
   ISMULTITHREAD is;
   memset (&is, 0, sizeof(is));
   is.dwPass = 11;   // CacheSurround360
   is.dwc = dwc;
   is.fLong = fLong;
   is.pis = pis;

   ThreadLoop (0, m_adw360Height[dwc], 1, &is, sizeof(is));

#if 0 // single threaded
   // loop
   DWORD x, y;
   BYTE * pb;
   COLORREF cr;
   for (y = 0; y < m_adw360Height[dwc]; y++) {
      pb = (BYTE*) pis->Pixel (0, y);
      for (x = 0; x < m_adw360Width[dwc]; x++) {
         cr = Surround360Pixel (x, y, fLong, pai, m_adw360Half[dwc], m_adw360Width[dwc], pabRGB, m_dwWidth, m_dwHeight);

         *(pb++) = GetRValue (cr);
         *(pb++) = GetGValue (cr);
         *(pb++) = GetBValue (cr);
      }
   }
#endif


#if 0// def _DEBUG
   dwDrawStart = GetTickCount() - dwDrawStart;
   char szTemp[64];
   sprintf (szTemp, "\r\nDraw 360 frame = %d", (int)dwDrawStart);
   OutputDebugString (szTemp);
#endif
   return TRUE;
}

#if 0 // no longer used
/*************************************************************************************
CImageStore::Surround360ToBitmap - Fills in a bitmap based on the current
360 degree rotation calculatesion (called with Surround360Set)

inputs
   DWORD       dwc - Cache number, 0.. NUMISCACHE-1
   HDC      hDC - Create a bitmap compatible to this
   fp       fLong - Longitudinal rotation, 0 = n, PI/2 = e, etc.
            NOTE: fLat and others must be set by calling Surround360Set()
returns
   HBITMAP - Bitmap that must be freed by the caller
*/
HBITMAP CImageStore::Surround360ToBitmap (DWORD dwc, HDC hDC, fp fLong)
{
   if (!CacheSurround360(dwc, fLong))
      return NULL;

   return m_aISCACHE[dwc].pis->ToBitmap (hDC);
}
#endif // 0

/*************************************************************************************
CImageStore::Surround360BitPixToOrig - This takes a bitmap in the bitmap
produced by Surround360ToBitmap() (m_adw360Width[dwc] x m_adw360Height[dwc]0 and
returns the pixel in the original image. (m_dwWidth x m_dwHeight)

inputs
   DWORD       dwc - Cache number, 0.. NUMISCACHE-1
   int            iX - X in bitmap
   int            iY - Y in bitmap
   fp             fLong - Longitudinal rotation (in radians)
   DWORD          *pdwX - X in original image
   DWORD          *pdwY - Y in original image
returns
   none
*/
void CImageStore::Surround360BitPixToOrig (DWORD dwc, int iX, int iY, fp fLong, DWORD *pdwX, DWORD *pdwY)
{
   int *pai = (int*)m_amem360[dwc].p;
   DWORD dwHalf = m_adw360Half[dwc];
   DWORD dwBitWidth = m_adw360Width[dwc];
   DWORD dwHeight = m_adw360Height[dwc];

   // make sure dont go beyond range in bitmap
   iX = min(iX, (int)dwBitWidth-1);
   iY = min(iY, (int)dwHeight-1);
   iX = max(iX, 0);
   iY = max(iY, 0);
   DWORD dwX = (DWORD)iX;
   DWORD dwY = (DWORD)iY;

   // see if should get value but mirror L/R
   BOOL fMirror;
   if (dwX >= dwBitWidth - dwHalf) {
      dwX -= (dwBitWidth - dwHalf);
      fMirror = FALSE;
   }
   else {
      dwX = dwHalf - dwX - 1;
      fMirror = TRUE;
   }

   // look up value in table
   pai = pai + (dwY * dwHalf + dwX)*2;

   // floating point loc
   int ifX, ifY;
   int ifWidth = (int)m_dwWidth << 16;
   fp fRot = fLong / (2.0 * PI) * (fp)Width();
   ifX = (int)(fRot * (fp)0x10000) + pai[0] * (fMirror ? -1 : 1);
   ifY = pai[1];
   while (ifX < 0)
      ifX += ifWidth;
   while (ifX >= ifWidth)
      ifX -= ifWidth;

   // integer loc
   iX = ifX >> 16;
   iY = ifY >> 16;
   iX = max(iX, 0);
   iY = max(iY, 0);
   *pdwX = (DWORD)iX;
   *pdwY = (DWORD)iY;


#if 0 // old code, dont use because there's an easier solution, that happens to handle round cameras
   // rotation matrix...
   CMatrix mRot;
   mRot.RotationX (m_af360Lat[dwc]);

   // size of a pixel at 1 m
   fp fSize;
   fSize = tan(m_af360FOV[dwc]/2) / ((fp)m_adw360Width[dwc]/2);
   NOTE: this won't handle round cameras

   // do only the one point
   CPoint p;
   fp fLen;
   fp fScaleLong = (fp)Width() / (2.0 * PI);
   fp fScaleLat = -(fp)Height() / PI;
   fp fAddLat = (fp)Height() / 2.0;

   p.Zero();
   p.p[0] = ((fp)iX - (fp)m_adw360Width[dwc] / 2.0);
   p.p[0] = p.p[0] * fSize;
   p.p[2] = ((fp)m_adw360Height[dwc]/2.0 - (fp)iY) * fSize;
   p.p[1] = 1;

   p.MultiplyLeft (&mRot);

   // determine the angle
   fLen = p.Length();
   fLong = (atan2(p.p[0], p.p[1]) + fLong) * fScaleLong;
   fp fLat = asin (p.p[2] / fLen) * fScaleLat + fAddLat;

   fLong = myfmod(fLong, Width());
   fLat = myfmod(fLat, Height());

   *pdwX = (DWORD)fLong;
   *pdwY = (DWORD)fLat;
#endif // 0
}



/*************************************************************************************
CImageStore::CacheStretch - This causes an stretch/string of the image to be cached.

inputs
   DWORD             dwc - Cache number, 0.. NUMISCACHE-1
   int               iWidth - Width of the image
   int               iHeight - Height of the image
   RECT              *prFrom - Where it was copied from
   COLORREF          cBack - Background color used if look beyond image boundaries
returns
   BOOL - TRUE if  cached
*/
BOOL CImageStore::CacheStretch (DWORD dwc, int iWidth, int iHeight,
                                RECT *prFrom, COLORREF cBack)
{
   // if it's already cached then do nothing
   if (m_aISCACHE[dwc].pis && EqualRect(&m_aISCACHE[dwc].rFrom, prFrom) &&
      (m_aISCACHE[dwc].iWidth == iWidth) && 
      (m_aISCACHE[dwc].iHeight == iHeight) &&
      (m_aISCACHE[dwc].cBack == cBack) )
      return TRUE;

   if ((iWidth <= 0) || (iHeight <= 0) || (prFrom->right <= prFrom->left) ||
      (prFrom->bottom <= prFrom->top))
      return FALSE;

   // else clear
   CacheClear (dwc);
   PCImageStore pis;
   m_aISCACHE[dwc].iWidth = iWidth;
   m_aISCACHE[dwc].iHeight = iHeight;
   m_aISCACHE[dwc].rFrom = *prFrom;
   m_aISCACHE[dwc].cBack = cBack;
   pis = m_aISCACHE[dwc].pis = new CImageStore;
   if (!pis)
      return FALSE;
   if (!pis->Init((DWORD)m_aISCACHE[dwc].iWidth, (DWORD)m_aISCACHE[dwc].iHeight)) {
      delete pis;
      m_aISCACHE[dwc].pis = NULL;
      return FALSE;
   }

   // figure out delta X and delta Y
   int iDeltaX = (int)((double)(prFrom->right - prFrom->left) / (double)iWidth * (double)0x10000);
   int iDeltaY = (int)((double)(prFrom->bottom - prFrom->top) / (double)iHeight * (double)0x10000);

   // loop
   DWORD x, y;
   int iCurXFixed, iCurYFixed;
   int iCurY, iCurX, iFracY, iFracX;
   PBYTE pbAbove, pbBelow, pb;
   PBYTE pbUL, pbUR, pbLL, pbLR;
   DWORD dwWeightL, dwWeightR, dwWeightU, dwWeightD;
   DWORD dwWeightUL, dwWeightLL, dwWeightUR, dwWeightLR;
   BYTE abBack[3];
   abBack[0] = GetRValue(cBack);
   abBack[1] = GetGValue(cBack);
   abBack[2] = GetBValue(cBack);
   for (y = 0, iCurYFixed = prFrom->top << 16; y < (DWORD)iHeight; y++, iCurYFixed += iDeltaY) {
      iCurY = iCurYFixed >> 16;
      iFracY = iCurYFixed & 0xffff;
      pbAbove = ((iCurY >= 0) && (iCurY < (int)m_dwHeight)) ? (BYTE*) Pixel (0, iCurY) : NULL;
      iCurY++;
      pbBelow = ((iCurY >= 0) && (iCurY < (int)m_dwHeight)) ? (BYTE*) Pixel (0, iCurY) : NULL;

      pb = pis->Pixel (0, y);
      dwWeightD = (DWORD)iFracY >> 8;
      dwWeightU = (0x100 - dwWeightD);  // so dont end up with overflow

      for (x = 0, iCurXFixed = prFrom->left << 16; x < (DWORD)iWidth; x++, iCurXFixed += iDeltaX) {
         iCurX = iCurXFixed >> 16;
         iFracX = iCurXFixed & 0xffff;

         // left
         if ((iCurX >= 0) && (iCurX < (int)m_dwWidth)) {
            pbUL = pbAbove ? (pbAbove + iCurX*3) : abBack;
            pbLL = pbBelow ? (pbBelow + iCurX*3) : abBack;
         }
         else
            pbUL = pbLL = abBack;

         // right
         iCurX++;
         if ((iCurX >= 0) && (iCurX < (int)m_dwWidth)) {
            pbUR = pbAbove ? (pbAbove + iCurX*3) : abBack;
            pbLR = pbBelow ? (pbBelow + iCurX*3) : abBack;
         }
         else
            pbUR = pbLR = abBack;

         // on the off chance they're all the same, dont bother interpolating
         if ((pbUL == pbLR) && (pbUL == pbUR) && (pbUL == pbLL)) {
            *(pb++) = *(pbUL++);	// BUGFIX - Was GetRValue (*(pbUL++))
            *(pb++) = *(pbUL++);	// BUGFIX - Was GetGValue (*(pbUL++))
            *(pb++) = *pbUL;   // BUGFIX - Was GetBValue ((*pbUL));
            continue;
         }

         // figure out the weighting
         dwWeightR = (DWORD)iFracX >> 8;
         dwWeightL = (0x100 - dwWeightR);

         dwWeightUL = dwWeightL * dwWeightU;
         dwWeightLL = dwWeightL * dwWeightD;
         dwWeightLR = dwWeightR * dwWeightD;
         dwWeightUR = dwWeightR * dwWeightU;

         // copy and interpolate bits
         // NOTE: Not gamma correcting but I don't think that'll matter much
         *(pb++) = (BYTE) ((
            (dwWeightUL * (DWORD) *(pbUL++)) +
            (dwWeightUR * (DWORD) *(pbUR++)) +
            (dwWeightLL * (DWORD) *(pbLL++)) +
            (dwWeightLR * (DWORD) *(pbLR++)) ) >> 16);
         *(pb++) = (BYTE) ((
            (dwWeightUL * (DWORD) *(pbUL++)) +
            (dwWeightUR * (DWORD) *(pbUR++)) +
            (dwWeightLL * (DWORD) *(pbLL++)) +
            (dwWeightLR * (DWORD) *(pbLR++)) ) >> 16);
         *(pb++) = (BYTE) ((
            (dwWeightUL * (DWORD) *(pbUL)) +
            (dwWeightUR * (DWORD) *(pbUR)) +
            (dwWeightLL * (DWORD) *(pbLL)) +
            (dwWeightLR * (DWORD) *(pbLR)) ) >> 16);
      }
   }

   // done
   return TRUE;
}


/*************************************************************************************
CImageStore::CacheGet - Gets the cached image. NOTE: Do NOT change this image.
Plus, its only valid until the next cache modifies it.

inputs
   DWORD             dwc - Cache number, 0.. NUMISCACHE-1
   int               iWidth - Width of the image (on the screen)
   int               iHeight - Height of the image (on the screen)
   RECT              *prFrom - Where it was copied from (only used if non-360)
   fp                fLong - Longitude (only used if 360 degree)
   COLORREF          cBack - Background color used if look beyond image boundaries
returns
   PCImageStore - Image of from the temporary store.
*/
PCImageStore CImageStore::CacheGet (DWORD dwc, int iWidth, int iHeight, RECT *prFrom,
                              fp fLong, COLORREF cBack)
{
   BOOL fRet;

   if (m_dwStretch == 4)
      fRet = CacheSurround360(dwc, fLong);
   else
      fRet = CacheStretch (dwc, iWidth, iHeight, prFrom, cBack);
   if (!fRet)
      return FALSE;

   return m_aISCACHE[dwc].pis;
}


/*************************************************************************************
CImageStore::CachePaint - Paints the image onto the screen.

inputs
   BOOL              fTransparent - If TRUE does a transparent paint, using LAYEREDTRANSPARENTCOLOR
   HDC               hDC - DC to draw to
   DWORD             dwc - Cache number, 0.. NUMISCACHE-1
   int               iOffsetX - X offset
   int               iOffsetY - Y offset
   int               iWidth - Width of the image (on the screen)
   int               iHeight - Height of the image (on the screen)
   RECT              *prFrom - Where it was copied from (only used if non-360)
   fp                fLong - Longitude (only used if 360 degree)
   COLORREF          cBack - Background color used if look beyond image boundaries
returns
   BOOL - TRUE if success
*/
BOOL CImageStore::CachePaint (BOOL fTransparent, HDC hDC, DWORD dwc, int iOffsetX, int iOffsetY,
                              int iWidth, int iHeight, RECT *prFrom,
                              fp fLong, COLORREF cBack)
{
   BOOL fRet;

   if (m_dwStretch == 4)
      fRet = CacheSurround360(dwc, fLong);
   else
      fRet = CacheStretch (dwc, iWidth, iHeight, prFrom, cBack);
   if (!fRet)
      return FALSE;

   // if no bitmap then create
   if (!m_aISCACHE[dwc].hBmp) {
      m_aISCACHE[dwc].hBmp = m_aISCACHE[dwc].pis->ToBitmap (hDC);
      if (!m_aISCACHE[dwc].hBmp)
         return FALSE;
   }

   // copy over
   HDC hDCBit = CreateCompatibleDC (hDC);
   if (!hDCBit)
      return NULL;
   SelectObject (hDCBit, m_aISCACHE[dwc].hBmp);
   if (fTransparent)
      TransparentBlt (hDC, 
         iOffsetX, iOffsetY, iWidth, iHeight,
         hDCBit,
         0, 0, iWidth, iHeight,
         LAYEREDTRANSPARENTCOLOR);
   else
      BitBlt (hDC, 
         iOffsetX, iOffsetY, iWidth, iHeight,
         hDCBit,
         0, 0,
         SRCCOPY);
   DeleteDC (hDCBit);

   return TRUE;
}



/*************************************************************************************
CImageStore::ClearToColor - Clears the image to a solid color

inputs
   COLORREF          cr - To clear to
*/
void CImageStore::ClearToColor (COLORREF cr)
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


// BUGBUG - Is gammaInit thread safe?
