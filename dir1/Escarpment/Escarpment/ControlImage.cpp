/***********************************************************************
ControlImage.cpp - Code for a control

begun 3/31/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <math.h>
#include "escarpment.h"
#include "resleak.h"

typedef struct {
   DWORD          dwBitmapID; // ID from bitmap cache
   HBITMAP        hbmpImage;  // image bitmap
   HBITMAP        hbmpMask;   // mask bitmap (for transparent). NULL if not transparent
   int            iImageWidth, iImageHeight; // real width & height of image in pixels
   int            iHScale, iVScale; // % to scale image. Applies to everything except SCALETOFIT & maybe PROPORTIONAL
   DWORD          dwType;
   DWORD          dwBorderSize;  // # of pixels for border
   COLORREF       cBorderColor;  // border color
   BOOL           fDeleteMask;   // if TRUE delete the bitmap mask
   BOOL           fTransparent;  // set to TRUE if transparent
   COLORREF       cTransparentColor;   // transparent color
   DWORD          dwTransparentDist;
   BOOL           fAddColor;     // if TRUE, then transparency is ignored and adds color over background
} IMAGEINFO, *PIMAGEINFO;

#define  IMAGETYPE_PROPORTIONAL     0x0000   // fixed proportions
#define  IMAGETYPE_SCALETOFIT       0x0001   // scale to fit any shape
#define  IMAGETYPE_TILE             0x0002   // tile if doesn't fit
#define  IMAGETYPE_HTILE            0x0003   // tile a row only, left below as background
#define  IMAGETYPE_VTILE            0x0004   // tile a vertical row ownly, leave below as background
#define  IMAGETYPE_VTILESCALE       0x0005   // tile vertically, but scale to fit



/***********************************************************************
FillIMAGEINFO - Fill in image info (except for hbmpimage and hbmpmask
   and iimagewidth and iimageheight) from the attributes. Do this so an
   application can change these attributes mid-stream.

IMPORTANT - cant change the image's bitmap or transparency mid-stream.

inputs
   PEscControl pControl - control
returns
   PIMAGEINFO - just in case want
*/
static PIMAGEINFO FillIMAGEINFO (PCEscControl pControl)
{
   IMAGEINFO   *pi = (IMAGEINFO*) pControl->m_mem.p;

   // defaults
   pi->cBorderColor = RGB(0x00, 0x00, 0xff);
   pi->dwType = IMAGETYPE_PROPORTIONAL;
   pi->iHScale = pi->iVScale = 100;
   pi->dwBorderSize = 0;

   // h & v scale
   int   iScale;

   if (AttribToPercent ((WCHAR*) pControl->m_treeAttrib.Find(L"scale"), &iScale))
      pi->iHScale = pi->iVScale = iScale;
   if (AttribToPercent ((WCHAR*) pControl->m_treeAttrib.Find(L"hscale"), &iScale))
      pi->iHScale = iScale;
   if (AttribToPercent ((WCHAR*) pControl->m_treeAttrib.Find(L"vscale"), &iScale))
      pi->iVScale = iScale;


   // border
   if (AttribToDecimal ((WCHAR*) pControl->m_treeAttrib.Find(L"border"), &iScale))
      pi->dwBorderSize = (DWORD) iScale;
   COLORREF cr;
   if (AttribToColor ((WCHAR*) pControl->m_treeAttrib.Find(L"bordercolor"), &cr))
      pi->cBorderColor = cr;

   // type
   WCHAR *psz;
   psz = (WCHAR*) pControl->m_treeAttrib.Find(L"type");
   if (psz && !_wcsicmp(psz, L"stretchtofit"))
      pi->dwType = IMAGETYPE_SCALETOFIT;
   else if (psz && !_wcsicmp(psz, L"tile"))
      pi->dwType = IMAGETYPE_TILE;
   else if (psz && !_wcsicmp(psz, L"htile"))
      pi->dwType = IMAGETYPE_HTILE;
   else if (psz && !_wcsicmp(psz, L"vtile"))
      pi->dwType = IMAGETYPE_VTILE;
   else if (psz && !_wcsicmp(psz, L"vtilescale"))
      pi->dwType = IMAGETYPE_VTILESCALE;
   else
      pi->dwType = IMAGETYPE_PROPORTIONAL;

   return pi;
}

/****************************************************************************
PaintAddImage - Paints the image. If it's an ordinary operation
then goes to bitblt, etc. If additive color then special code.

inputs
   HBITMAP     hbmpImage - Image
   HBITMAP     hbmpMask - Transparent mask. Ignored if fAddColor
   HDC         hDCInto - DC that painting into
   RECT        *prInto - Rectangle that painting into
   RECT        *prFrom - Rectangle that painting from
   RECT        *prClip - Clipping rectangle. Might be NULL
   BOOL        fAddColor - Set to TRUE if adding the color
   RECT        *prHDCLimits - Rectangle of HDC limits, used if fAddColor
*/
void PaintAddImage (HBITMAP hbmpImage, HBITMAP hbmpMask, HDC hDCInto,
                     RECT *prInto, RECT *prFrom, RECT *prClip, BOOL fAddColor, RECT *prHDCLimits)
{
   // if not adding then easy
   if (!fAddColor) {
      BMPTransparentBlt (hbmpImage, hbmpMask, hDCInto, prInto, prFrom, prClip);
      return;
   }

   // clip to rectangle
   RECT rInter;
   if (prClip) {
      if (!IntersectRect (&rInter, prClip, prHDCLimits))
         return;
   }
   else
      rInter = *prHDCLimits;

   // create the HDC to read from
   HDC hDCFrom = CreateCompatibleDC (hDCInto);
   SelectObject (hDCFrom, hbmpImage);

   int iWidth = GetDeviceCaps (hDCInto, HORZRES);
   int iHeight = GetDeviceCaps (hDCInto, VERTRES);
   rInter.left = max(rInter.left, 0);
   rInter.top = max(rInter.top, 0);
   rInter.right = min(rInter.right, iWidth);
   rInter.bottom = min(rInter.bottom, iHeight);

   COLORREF crSource, crDest;

   // figure out the scale
   double fDeltaX = (double)(prFrom->right - prFrom->left) / (double)max(prInto->right - prInto->left, 1);
   double fDeltaY = (double)(prFrom->bottom - prFrom->top) / (double)max(prInto->bottom - prInto->top, 1);
   int x, y, iYRound, iXRound;
   double fY, fX;
   for (y = prInto->top, fY = prFrom->top + 0.5; y < prInto->bottom; y++, fY += fDeltaY) {
      if (y >= rInter.bottom)
         break;
      if (y < rInter.top)
         continue;

      iYRound = (int) floor (fY);

      for (x = prInto->left, fX = prFrom->left + 0.5; x < prInto->right; x++, fX += fDeltaX) {
         if (x >= rInter.right)
            break;
         if (x < rInter.left)
            continue;

         // get the pixel of the source
         iXRound = (int) floor(fX);
         crSource = GetPixel (hDCFrom, iXRound, iYRound);
         if ((crSource == CLR_INVALID) || !crSource)
            continue;   // if incalid, or black then nothing

         // get destination pixel
         crDest = GetPixel (hDCInto, x, y);
         if (crDest == CLR_INVALID)
            continue;   // skip

         crDest = RGB (
            (BYTE)min( (WORD)GetRValue(crSource) + (WORD)GetRValue(crDest), 255 ),
            (BYTE)min( (WORD)GetGValue(crSource) + (WORD)GetGValue(crDest), 255 ),
            (BYTE)min( (WORD)GetBValue(crSource) + (WORD)GetBValue(crDest), 255 )
            );
         SetPixel (hDCInto, x, y, crDest);
         // BUGBUG - this is slow. need to do without setpixel()
      } // x
   } // y

   // free temp deelte
   DeleteDC (hDCFrom);
}



/****************************************************************************
PaintControl - Paints the control

inputs
   PCEscControl   *pControl - control
   ESCMPAINT      *p - paint info
returns
   BOOL - TRUE if handles
*/
BOOL PaintControl (PCEscControl pControl, ESCMPAINT *p)
{
   // make sure to update the rect info
   PIMAGEINFO pi = FillIMAGEINFO (pControl);


   // if there's no bitmap error
   if (!pi->hbmpImage)
      return FALSE;

   // draw border?
   RECT  r;
   if (pi->dwBorderSize) {
      HBRUSH hbr;
      int   iBorder = (int) pi->dwBorderSize;
      hbr = CreateSolidBrush (pi->cBorderColor);

      // left
      r = p->rControlHDC;
      r.right = r.left + iBorder;
      FillRect (p->hDC, &r, hbr);

      // right
      r = p->rControlHDC;
      r.left = r.right - iBorder;
      FillRect (p->hDC, &r, hbr);

      // top
      r = p->rControlHDC;
      r.bottom = r.top + iBorder;
      FillRect (p->hDC, &r, hbr);

      // bottom
      r = p->rControlHDC;
      r.top = r.bottom - iBorder;
      FillRect (p->hDC, &r, hbr);

      DeleteObject (hbr);

      // shrink r to account for border
      r = p->rControlHDC;
      r.right -= iBorder;
      r.left += iBorder;
      r.top += iBorder;
      r.bottom -= iBorder;
   }
   else
      r = p->rControlHDC;

   switch (pi->dwType) {

   case IMAGETYPE_TILE:
   case IMAGETYPE_HTILE:
   case IMAGETYPE_VTILE:
   case IMAGETYPE_VTILESCALE:
      {
         // calculate how many images need to blit
         int   iImageWidth, iImageHeight;
         int   iCurAcross, iCurDown;
         iImageWidth = (pi->iImageWidth * pi->iHScale) / 100;
         iImageHeight = (pi->iImageHeight * pi->iVScale) / 100;

         if (pi->dwType == IMAGETYPE_VTILESCALE) {
            // special case
            iImageWidth = max(p->rControlHDC.right - p->rControlHDC.left, 1);
            iImageHeight = (int) ((double)iImageWidth / (double)max(pi->iImageWidth,1) * (double)pi->iImageHeight);
         }

         RECT  rFrom;
         rFrom.left = rFrom.top = 0;
         rFrom.right = pi->iImageWidth;
         rFrom.bottom = pi->iImageHeight;

         for (iCurAcross = p->rControlHDC.left; iCurAcross < p->rControlHDC.right; iCurAcross += iImageWidth) {
            for (iCurDown = p->rControlHDC.top; iCurDown < p->rControlHDC.bottom; iCurDown += iImageHeight) {
               // rectangle to draw to
               RECT  rTo;
               rTo.left = iCurAcross;
               rTo.right = rTo.left + iImageWidth;
               rTo.top = iCurDown;
               rTo.bottom = rTo.top + iImageHeight;

               // if not in refresh area then skip
               RECT  rInt;
               if (!IntersectRect (&rInt, &rTo, &p->rInvalidHDC))
                  continue;

               PaintAddImage (pi->hbmpImage, pi->hbmpMask, p->hDC,
                     &rTo, &rFrom, &p->rControlHDC, pi->fAddColor, &p->rInvalidHDC);


               // only do one if style if htile
               if (pi->dwType == IMAGETYPE_HTILE)
                  break;
            }

            // only do one is style is vtile
            if ((pi->dwType == IMAGETYPE_VTILE) || (pi->dwType == IMAGETYPE_VTILESCALE))
               break;
         }
      }
      break;

   case IMAGETYPE_PROPORTIONAL:
   case IMAGETYPE_SCALETOFIT:
   default:
      {
         RECT  rFrom;
         rFrom.left = rFrom.top = 0;
         rFrom.right = pi->iImageWidth;
         rFrom.bottom = pi->iImageHeight;
         PaintAddImage (pi->hbmpImage, pi->hbmpMask, p->hDC,
                     &r, &rFrom, NULL, pi->fAddColor, &p->rInvalidHDC);

      }

      break;
   }

   return TRUE;
}



/***********************************************************************
InitControl - Initializes the control

inputs
   PCEscControl   *pControl - control
returns
   none
*/
static void InitControl (PCEscControl pControl)
{
   // get the image info
   FillIMAGEINFO (pControl);

   IMAGEINFO   *pi = (IMAGEINFO*) pControl->m_mem.p;
   pi->fDeleteMask = FALSE;

   BOOL  fYesNo, fRet, fWantMask;
   fRet = AttribToYesNo ((WCHAR*) pControl->m_treeAttrib.Find (L"addcolor"), &pi->fAddColor);

   // generate mask
   COLORREF cMatch;
   DWORD dw;
   dw = 10;
   cMatch = (DWORD)-1;
   fWantMask = FALSE;
   fYesNo = FALSE;
   fRet = AttribToYesNo ((WCHAR*) pControl->m_treeAttrib.Find(L"transparent"), &fYesNo);
   if (fRet && fYesNo) {
      fWantMask = TRUE;

      if (!AttribToColor ((WCHAR*) pControl->m_treeAttrib.Find(L"transparentcolor"), &cMatch))
         cMatch = (DWORD)-1;
      if (!AttribToDecimal ((WCHAR*) pControl->m_treeAttrib.Find(L"transparentdistance"), (int*) &dw))
         dw = 10;
   }
   pi->fTransparent = fYesNo;
   pi->cTransparentColor = cMatch;
   pi->dwTransparentDist = dw;

   PCBitmapCache pCache;
   pCache = EscBitmapCache();

   // first, get the image
   WCHAR *psz;
   size_t dwResource;
   BOOL  f;
   psz = (WCHAR*) pControl->m_treeAttrib.Find (L"file");
   if (psz) {
      pi->dwBitmapID = pCache->CacheFile (psz, &pi->hbmpImage,
         fWantMask ? &pi->hbmpMask : NULL, cMatch, dw);
      if (!pi->hbmpImage) {
         goto fail;
      }
   }
   else {
      f = AttribToDecimal (psz = (WCHAR*) pControl->m_treeAttrib.Find (L"jpgresource"), (int*) &dwResource);
      if (f) {
         // JPEG resource
         pi->dwBitmapID = pCache->CacheResourceJPG (dwResource, pControl->m_hInstance, &pi->hbmpImage,
            fWantMask ? &pi->hbmpMask : NULL, cMatch, dw);
         if (!pi->hbmpImage) {
            goto fail;
         }
      }
      else {
         f = AttribToDecimal (psz = (WCHAR*) pControl->m_treeAttrib.Find (L"bmpresource"), (int*) &dwResource);
         if (f) { // bmp resource
            pi->dwBitmapID = pCache->CacheResourceBMP (dwResource, pControl->m_hInstance, &pi->hbmpImage,
               fWantMask ? &pi->hbmpMask : NULL, cMatch, dw);
            if (!pi->hbmpImage) {
               goto fail;
            }

         }
         else {
            // final alternative
            f = AttribToHex (psz = (WCHAR*) pControl->m_treeAttrib.Find (L"hbitmap"), &dwResource);
            // IMPORTANT: When do this hbitmap WILL be deleted
            // ??? - may not want bitmap deleted
            if (f) {
               pi->hbmpImage = (HBITMAP) dwResource;
               if (fWantMask) {
                  pi->hbmpMask = TransparentBitmap (pi->hbmpImage, cMatch, dw);
                  pi->fDeleteMask = TRUE;
               }
            }
            else {
               // just ignore the image
               goto fail;
            }

         }  // else - not bmp resource
      } // else not jpeg resource

   }  // else psz

   // get real size
   BMPSize (pi->hbmpImage, &pi->iImageWidth, &pi->iImageHeight);

   // if this has a href then want mouse
   pControl->m_fWantMouse = pControl->m_pNode->AttribGet(L"href") ? TRUE : FALSE;
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

   // done
   return;

fail:
   if (pi->dwBitmapID)
      pCache->CacheRelease(pi->dwBitmapID);
   if (pi->fDeleteMask && pi->hbmpMask)
      DeleteObject (pi->hbmpMask);
   pi->dwBitmapID = NULL;
   return;
}

/***********************************************************************
Control callback
*/
BOOL ControlImage (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      pControl->m_mem.Required(sizeof(IMAGEINFO));
      memset (pControl->m_mem.p, 0, sizeof(IMAGEINFO));
      return TRUE;


   case ESCM_DESTRUCTOR:
      if (pControl->m_mem.m_dwAllocated >= sizeof(PIMAGEINFO)) {
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;

         PCBitmapCache pCache;
         pCache = EscBitmapCache();
         if (pi->dwBitmapID)
            pCache->CacheRelease(pi->dwBitmapID);
         if (pi->fDeleteMask && pi->hbmpMask)
            DeleteObject (pi->hbmpMask);
         pi->fDeleteMask = FALSE;
         pi->dwBitmapID = NULL;
      }
      return TRUE;

   case ESCM_INITCONTROL:
      InitControl (pControl);
      return TRUE;


   case ESCM_LBUTTONDOWN:
      {
         WCHAR *psz;
         psz = (WCHAR*) pControl->m_treeAttrib.Find (L"href");
         if (psz) {
            pControl->m_pParentPage->m_pWindow->Beep(ESCBEEP_LINKCLICK);

            // must release capture or bad things happen
            pControl->m_pParentPage->MouseCaptureRelease(pControl);

            pControl->m_pParentPage->Link (psz);
         }
      }
      return TRUE;


   case ESCM_QUERYSIZE:
      {
         PIMAGEINFO pi = FillIMAGEINFO(pControl);
         ESCMQUERYSIZE  *p = (ESCMQUERYSIZE*) pParam;
         int iFW, iFH;
         int   iFinalWidth = 0, iFinalHeight = 0;
         BOOL  fPercent;
         if (AttribToDecimalOrPercent (pControl->m_pNode->AttribGet(L"width"), &fPercent, &iFW)) {
            if (fPercent)
               iFinalWidth = iFW * p->iDisplayWidth / 100;
            else
               iFinalWidth = iFW;
         };

         if (AttribToDecimalOrPercent (pControl->m_pNode->AttribGet(L"height"), &fPercent, &iFH)) {
            if (fPercent)
               iFinalHeight = iFH * p->iDisplayWidth / 100;
            else
               iFinalHeight = iFH;
         };

         // how big do we really want it to make the image fit nicely
         int   iWantWidth, iWantHeight;
         iWantWidth = (int) ((pi->iImageWidth * pi->iHScale) / 100 + (int) pi->dwBorderSize * 2);
         iWantHeight = (int) ((pi->iImageHeight * pi->iVScale) / 100 + (int) pi->dwBorderSize * 2);

         // BUGFIX - If it's proportional and there's a width set but no height than adjust height
         // if it's marked as proportional then readjust hight ot make sure it is
         if ((pi->dwType == IMAGETYPE_PROPORTIONAL) && iFinalWidth && !iFinalHeight) {
            double   fFinalWidth;

            // what's it really, without the border
            fFinalWidth = iFinalWidth - (int) pi->dwBorderSize * 2;

            // what's the ratio to what we got compared to what we wanted
            fFinalWidth = fFinalWidth / (pi->iImageWidth * pi->iHScale) * 100.0;

            // readjust
            iFinalHeight = (int) (fFinalWidth * (pi->iImageHeight * pi->iVScale) / 100) +
               (int) pi->dwBorderSize * 2;
         }


         // if the height/width are unspecified then take ideals
         if (iFinalWidth <= 0)
            iFinalWidth = iWantWidth;
         if (iFinalHeight <= 0)
            iFinalHeight = iWantHeight;

         // BUGFIX - I'm not sure what the following it supposed to do but I'll leav it
         // if it's marked as proportional then readjust hight ot make sure it is
         if ((iWantHeight != iFinalHeight) && (pi->dwType == IMAGETYPE_PROPORTIONAL)) {
            double   fFinalWidth;

            // what's it really, without the border
            fFinalWidth = iFinalWidth - (int) pi->dwBorderSize * 2;

            // what's the ratio to what we got compared to what we wanted
            fFinalWidth = fFinalWidth / (pi->iImageWidth * pi->iHScale) * 100.0;

            // readjust
            iFinalHeight = (int) (fFinalWidth * (pi->iImageHeight * pi->iVScale) / 100) +
               (int) pi->dwBorderSize * 2;
         }

         // store away
         p->iWidth = iFinalWidth;
         p->iHeight = iFinalHeight;
      }
      return TRUE;



   case ESCM_PAINT:
      return PaintControl (pControl, (PESCMPAINT) pParam);

   case ESCM_ATTRIBSET:
      {
         ESCMATTRIBSET *p  =(ESCMATTRIBSET*) pParam;

         if (p->pszValue &&p->pszAttrib && !_wcsicmp(p->pszAttrib, L"hbitmap")) {
            size_t dwResource;
            BOOL f;

            f = AttribToHex (p->pszValue, &dwResource);
            // IMPORTANT: When do this hbitmap WILL be deleted
            // ??? - may not want bitmap deleted
            if (f) {
               PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;

               if (pi->fDeleteMask && pi->hbmpMask)
                  DeleteObject (pi->hbmpMask);
               pi->hbmpMask = NULL;
               pi->fDeleteMask = FALSE;

               pi->hbmpImage = (HBITMAP) dwResource;
               if (pi->fTransparent) {
                  pi->hbmpMask = TransparentBitmap (pi->hbmpImage, pi->cTransparentColor, pi->dwTransparentDist);
                  pi->fDeleteMask = TRUE;
               }
               pControl->Invalidate();

               return TRUE;
            }
            return FALSE;
         }
      }
      return FALSE;


   }

   return FALSE;
}




// BUGBUG - 2.0 - need a way of making a watermark image.
