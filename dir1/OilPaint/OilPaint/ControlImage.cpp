/***********************************************************************
ControlImage.cpp - Code for a control

begun 3/31/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include <escarpment.h>
#include <math.h>
//#include "resleak.h"
extern BOOL        gfKnowGridArt;     // set to TRUE if know its gridart

typedef struct {
   DWORD          dwBitmapID; // ID from bitmap cache
   HBITMAP        hbmpImage;  // image bitmap
   HBITMAP        hbmpMask;   // mask bitmap (for transparent). NULL if not transparent
   int            iImageWidth, iImageHeight; // real width & height of image in pixels
   int            iHScale, iVScale; // % to scale image. Applies to everything except SCALETOFIT & maybe PROPORTIONAL
   DWORD          dwType;
   DWORD          dwBorderSize;  // # of pixels for border
   COLORREF       cBorderColor;  // border color
   double         fGridX, fGridY;   // grid size in pixels
} IMAGEINFO, *PIMAGEINFO;

#define  IMAGETYPE_PROPORTIONAL     0x0000   // fixed proportions
#define  IMAGETYPE_SCALETOFIT       0x0001   // scale to fit any shape
#define  IMAGETYPE_TILE             0x0002   // tile if doesn't fit
#define  IMAGETYPE_HTILE            0x0003   // tile a row only, left below as background
#define  IMAGETYPE_VTILE            0x0004   // tile a vertical row ownly, leave below as background



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
   else
      pi->dwType = IMAGETYPE_PROPORTIONAL;

   return pi;
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
      {
         // calculate how many images need to blit
         int   iImageWidth, iImageHeight;
         int   iCurAcross, iCurDown;
         iImageWidth = (pi->iImageWidth * pi->iHScale) / 100;
         iImageHeight = (pi->iImageHeight * pi->iVScale) / 100;

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

               BMPTransparentBlt (pi->hbmpImage, pi->hbmpMask, p->hDC,
                     &rTo, &rFrom, &p->rControlHDC);


               // only do one if style if htile
               if (pi->dwType == IMAGETYPE_HTILE)
                  break;
            }

            // only do one is style is vtile
            if (pi->dwType == IMAGETYPE_VTILE)
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
         BMPTransparentBlt (pi->hbmpImage, pi->hbmpMask, p->hDC,
                     &r, &rFrom, NULL);

      }

      break;
   }

   // draw grid
   if (pi->fGridX || pi->fGridY) {
      double   f, fMax;
      double   fScaleX, fScaleY;

      COLORREF cOld = GetBkColor (p->hDC);
      SetBkColor (p->hDC, RGB(255,255,255));
      int   iOldMode;
      iOldMode = SetBkMode (p->hDC, OPAQUE);

      fScaleX = (double)(r.right - r.left) / pi->iImageWidth;
      fScaleY = (double)(r.bottom - r.top) / pi->iImageHeight;

      // BUGFIX - Offset
      double fOffsetX, fOffsetY;
      fOffsetX = fOffsetY = 0;
      if (gfKnowGridArt) {
         double f;
         f = fmod(pi->iImageWidth, pi->fGridX);
         if ((f > 1) && (f < pi->fGridX-1))
            fOffsetX = -(pi->fGridX - f / 2.0);
         f = fmod(pi->iImageHeight, pi->fGridY);
         if ((f > 1) && (f < pi->fGridY-1))
            fOffsetY = -(pi->fGridY - f / 2.0);
         fOffsetX *= fScaleX;
         fOffsetY *= fScaleY;
      }

      fScaleX *= pi->fGridX;
      fScaleY *= pi->fGridY;

      // draw lines around it
      HPEN  hPen, hOld;
      hPen = CreatePen (PS_DOT, 1, RGB(0,0,0));
      hOld = (HPEN) SelectObject (p->hDC, hPen);

      // horizontal lines
      fMax = r.bottom;
      if (fScaleY) for (f = r.top + fScaleY + (int)fOffsetY; f < fMax; f += fScaleY) {
         MoveToEx (p->hDC, r.left, (int) f, NULL);
         LineTo (p->hDC, r.right, (int) f);
      }

      // vertical lines
      fMax = r.right;
      if (fScaleX) for (f = r.left + fScaleX + (int) fOffsetX; f < fMax; f += fScaleX) {
         MoveToEx (p->hDC, (int) f, r.top, NULL);
         LineTo (p->hDC, (int) f, r.bottom);
      }

      SelectObject (p->hDC, hOld);
      DeleteObject (hPen);

      SetBkColor (p->hDC, cOld);
      SetBkMode (p->hDC, iOldMode);
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

   // generate mask
   BOOL  fYesNo, fRet, fWantMask;
   COLORREF cMatch;
   DWORD dw;
   dw = 10;
   cMatch = (DWORD)-1;
   fWantMask = FALSE;
   fRet = AttribToYesNo ((WCHAR*) pControl->m_treeAttrib.Find(L"transparent"), &fYesNo);
   if (fRet && fYesNo) {
      fWantMask = TRUE;

      if (!AttribToColor ((WCHAR*) pControl->m_treeAttrib.Find(L"transparentcolor"), &cMatch))
         cMatch = (DWORD)-1;
      if (!AttribToDecimal ((WCHAR*) pControl->m_treeAttrib.Find(L"transparentdistance"), (int*) &dw))
         dw = 10;
   }
   PCBitmapCache pCache;
   pCache = EscBitmapCache();

   // first, get the image
   WCHAR *psz;
   DWORD dwResource;
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
            size_t iRes;
            f = AttribToHex (psz = (WCHAR*) pControl->m_treeAttrib.Find (L"hbitmap"), &iRes);
            dwResource = (DWORD)iRes;
            // IMPORTANT: When do this hbitmap WILL be deleted
            // ??? - may not want bitmap deleted
            if (f)
               pi->hbmpImage = (HBITMAP) dwResource;
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
   pControl->m_fWantMouse = TRUE;   // so can change cursor

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

   case ESCM_MOUSEENTER:
      pControl->m_pParentPage->SetCursor (/*IDC_HANDCURSOR*/(DWORD)IDC_CROSS);
      return TRUE;

   case ESCM_ATTRIBSET:
      {
         ESCMATTRIBSET *p  =(ESCMATTRIBSET*) pParam;

         if (!p->pszValue || !p->pszAttrib)
            return FALSE;

         if (!_wcsicmp(p->pszAttrib, L"hbitmap")) {
            DWORD dwResource;
            BOOL f;
            size_t iRes;
            f = AttribToHex (p->pszValue, &iRes);
            dwResource = (DWORD)iRes;
            // IMPORTANT: When do this hbitmap WILL be deleted
            // ??? - may not want bitmap deleted
            if (f) {
               PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
               pi->hbmpImage = (HBITMAP) dwResource;
               pControl->Invalidate();

               return TRUE;
            }
            return FALSE;
         }
         else if (!_wcsicmp(p->pszAttrib, L"gridx")) {
            double dwResource;
            BOOL f;
            f = AttribToDouble (p->pszValue, &dwResource);
            if (f) {
               PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
               pi->fGridX = dwResource;
               pControl->Invalidate();
               return TRUE;
            }
            return FALSE;
         }
         else if (!_wcsicmp(p->pszAttrib, L"gridy")) {
            double dwResource;
            BOOL f;
            f = AttribToDouble (p->pszValue, &dwResource);
            if (f) {
               PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
               pi->fGridY = dwResource;
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
