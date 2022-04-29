/***********************************************************************
ControlImageDrag.cpp - Code for a control

begun 3/31/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"
//#include "resleak.h"

typedef struct {
   DWORD          dwBitmapID; // ID from bitmap cache
   HBITMAP        hbmpImage;  // image bitmap
   HBITMAP        hbmpMask;   // mask bitmap (for transparent). NULL if not transparent
   int            iImageWidth, iImageHeight; // real width & height of image in pixels
   int            iHScale, iVScale; // % to scale image. Applies to everything except SCALETOFIT & maybe PROPORTIONAL
   DWORD          dwType;
   DWORD          dwBorderSize;  // # of pixels for border
   COLORREF       cBorderColor;  // border color
   COLORREF       cSelColor;     // draw selection in this color
   DWORD          dwClickMode;   // 0 for none, 1 for just clicks, 2 for drags
   RECT           rSelDraw;      // where the selction is drawn
   RECT           rAreaDraw;     // area of control's HDC that draw over
   PCListFixed    plIDR;         // list of CONTROLIMAGEDRAGRECT
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
   pi->dwClickMode = 0;

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
   if (AttribToDecimal ((WCHAR*) pControl->m_treeAttrib.Find(L"clickmode"), &iScale))
      pi->dwClickMode = (DWORD) iScale;
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


/***********************************************************************
RectScreenPixelToBitmap - Converts from a location as it appears on the
screen to what it would be on the original bitmap (taking into account stretching)

inputs
   RECT        *pr - Originall filled in, and then changed with values
   RECT        *prad - Area that drawed on. pi->rAreaDraw
   int         iWidth - Width of original bitmap
   int         iHeight - Height or original bitmap
*/
void RectScreenPixelToBitmap (RECT *pr, RECT *prad, int iWidth, int iHeight)
{
   double ix = (double)iWidth / (double)max(prad->right - prad->left, 1);
   double iy = (double)iHeight / (double)max(prad->bottom - prad->top, 1);

   pr->left = (int)((double)(pr->left - prad->left) * ix);
   pr->right = (int)((double)(pr->right - prad->left) * ix);
   pr->top = (int)((double)(pr->top - prad->top) * iy);
   pr->bottom = (int)((double)(pr->bottom - prad->top) * iy);

   int iTemp;
   if (pr->right < pr->left) {
      iTemp = pr->right;
      pr->right = pr->left;
      pr->left = iTemp;
   }
   if (pr->bottom < pr->top) {
      iTemp = pr->bottom;
      pr->bottom = pr->top;
      pr->top = iTemp;
   }
}


/***********************************************************************
RectBitmapPixelToScreen - Converts from a location as it appears on the
screen to what it would be on the original bitmap (taking into account stretching)

inputs
   RECT        *pr - Originall filled in, and then changed with values
   RECT        *prad - Area that drawed on. pi->rAreaDraw
   int         iWidth - Width of original bitmap
   int         iHeight - Height or original bitmap
*/
void RectBitmapPixelToScreen (RECT *pr, RECT *prad, int iWidth, int iHeight)
{
   double ix = (double) (prad->right - prad->left) / (double)max(iWidth, 1);
   double iy = (double) (prad->bottom - prad->top) / (double)max(iHeight, 1);

   pr->left = (int)((double)pr->left * ix) + prad->left;
   pr->right = (int)((double)pr->right * ix) + prad->left;
   pr->top = (int)((double)pr->top * iy) + prad->top;
   pr->bottom = (int)((double)pr->bottom * iy) + prad->top;
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

   // store away the rectangle that stretched out to so that can use this for clicks
   pi->rAreaDraw = p->rControlPage;
   if (pi->dwBorderSize) {
      int iBorder = (int)pi->dwBorderSize;

      pi->rAreaDraw.right -= iBorder;
      pi->rAreaDraw.left += iBorder;
      pi->rAreaDraw.top += iBorder;
      pi->rAreaDraw.bottom -= iBorder;
   }

   // paint the rectangles
   DWORD i, j;
   PCONTROLIMAGEDRAGRECT pr = pi->plIDR ? (PCONTROLIMAGEDRAGRECT)pi->plIDR->Get(0) : NULL;
   if (pr) for (i = 0; i < pi->plIDR->Num(); i++, pr++) {
      // create pen and bursh
      HPEN hPen = CreatePen (PS_SOLID, 2, pr->cColor);
      HPEN hPenOld = (HPEN)SelectObject (p->hDC, hPen);
      HBRUSH hBrush = CreateHatchBrush (HS_DIAGCROSS, pr->cColor);
      HBRUSH hBrushOld = (HBRUSH)SelectObject (p->hDC, hBrush);
      int iMode = SetBkMode (p->hDC, TRANSPARENT);

      // draw each modulo version
      for (j = 0; j < (DWORD)(pr->fModulo ? 4 : 1); j++) {
         RECT rDraw = pr->rPos;

         if ((j==1) || (j==3)) {
            // try right version
            if (min(rDraw.left,rDraw.right) < 0) {
               rDraw.left += pi->iImageWidth;
               rDraw.right += pi->iImageWidth;
            }
            else if (max(rDraw.left, rDraw.right) >= pi->iImageWidth) {
               rDraw.left -= pi->iImageWidth;
               rDraw.right -= pi->iImageWidth;
            }
         }
         if ((j==2) || (j==3)) {
            // try down version
            if (min(rDraw.top,rDraw.bottom) < 0) {
               rDraw.top += pi->iImageHeight;
               rDraw.bottom += pi->iImageHeight;
            }
            else if (max(rDraw.top, rDraw.bottom) >= pi->iImageHeight) {
               rDraw.top -= pi->iImageHeight;
               rDraw.bottom -= pi->iImageHeight;
            }
         }

         // if no change then ignore
         if ((j >= 1) &&
            (rDraw.left == pr->rPos.left) &&
            (rDraw.right == pr->rPos.right) &&
            (rDraw.top == pr->rPos.top) &&
            (rDraw.bottom == pr->rPos.bottom))
            continue;

         // convert
         RectBitmapPixelToScreen (&rDraw, &pi->rAreaDraw, pi->iImageWidth, pi->iImageHeight);
         rDraw.left += (p->rControlHDC.left - p->rControlPage.left);
         rDraw.right += (p->rControlHDC.right - p->rControlPage.right);
         rDraw.top += (p->rControlHDC.top - p->rControlPage.top);
         rDraw.bottom += (p->rControlHDC.bottom - p->rControlPage.bottom);

         // bound to rectangle
         RECT rTemp;
         rTemp = rDraw;
         if (!IntersectRect (&rDraw, &rTemp, &p->rControlHDC))
            continue;
         
         Rectangle (p->hDC, rDraw.left, rDraw.top, rDraw.right, rDraw.bottom);
         //FillRect (p->hDC, &rDraw, hBrush);
         //MoveToEx (p->hDC, rDraw.left, rDraw.top, NULL);
         //LineTo (p->hDC, rDraw.right, rDraw.top);
         //LineTo (p->hDC, rDraw.right, rDraw.bottom);
         //LineTo (p->hDC, rDraw.left, rDraw.bottom);
         //LineTo (p->hDC, rDraw.left, rDraw.top);
      } // j

      // free pen and brush
      SetBkMode (p->hDC, iMode);
      SelectObject (p->hDC, hPenOld);
      DeleteObject (hPen);
      SelectObject (p->hDC, hBrushOld);
      DeleteObject (hBrush);
   } // i

   // draw the selection
   if (pControl->m_fLButtonDown && (pi->dwClickMode == 2)) {
      HPEN hPen = CreatePen (PS_DOT, 0, pi->cSelColor);
      HPEN hPenOld = (HPEN)SelectObject (p->hDC, hPen);
      RECT rDraw;

      rDraw = pi->rSelDraw;
      rDraw.left += (p->rControlHDC.left - p->rControlPage.left);
      rDraw.right += (p->rControlHDC.right - p->rControlPage.right);
      rDraw.top += (p->rControlHDC.top - p->rControlPage.top);
      rDraw.bottom += (p->rControlHDC.bottom - p->rControlPage.bottom);

      MoveToEx (p->hDC, rDraw.left, rDraw.top, NULL);
      LineTo (p->hDC, rDraw.right, rDraw.top);
      LineTo (p->hDC, rDraw.right, rDraw.bottom);
      LineTo (p->hDC, rDraw.left, rDraw.bottom);
      LineTo (p->hDC, rDraw.left, rDraw.top);

      SelectObject (p->hDC, hPenOld);
      DeleteObject (hPen);
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
   pControl->m_fWantMouse = pi->dwClickMode ? TRUE : FALSE;
   //if (pControl->m_fWantMouse)
   //   pControl->m_dwWantFocus = 1;
   pControl->m_dwWantFocus = 0;

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
BOOL ControlImageDrag (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required(sizeof(IMAGEINFO));
         memset (pControl->m_mem.p, 0, sizeof(IMAGEINFO));
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
         pi->cSelColor = RGB(0xff,0,0);
         pi->plIDR = new CListFixed;
         if (pi->plIDR)
            pi->plIDR->Init (sizeof(CONTROLIMAGEDRAGRECT));
         pControl->AttribListAddColor (L"selcolor", &pi->cSelColor, NULL, TRUE);
      }
      return TRUE;


   case ESCM_DESTRUCTOR:
      if (pControl->m_mem.m_dwAllocated >= sizeof(PIMAGEINFO)) {
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;

         if (pi->plIDR)
            delete pi->plIDR;

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

   case ESCM_IMAGERECTSET:
      {
         PESCMIMAGERECTSET ps = (PESCMIMAGERECTSET) pParam;
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
         if (!pi->plIDR)
            return FALSE;
         pi->plIDR->Init (sizeof(CONTROLIMAGEDRAGRECT), ps->pRect, ps->dwNum);

         pControl->Invalidate();
      }
      return TRUE;

   case ESCM_IMAGERECTGET:
      {
         PESCMIMAGERECTGET ps = (PESCMIMAGERECTGET) pParam;
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
         if (!pi->plIDR)
            return FALSE;
         ps->dwNum = pi->plIDR->Num();
         ps->pRect = (PCONTROLIMAGEDRAGRECT) pi->plIDR->Get(0);
      }
      return TRUE;

   case ESCM_LBUTTONDOWN:
      {
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
         PESCMLBUTTONDOWN p = (PESCMLBUTTONDOWN) pParam;

         // store away location
         pi->rSelDraw.left = pi->rSelDraw.right = p->pPosn.x;
         pi->rSelDraw.top = pi->rSelDraw.bottom = p->pPosn.y;

         // message
         ESCNIMAGEDRAGGED id;
         id.pControl = pControl;
         id.rPos = pi->rSelDraw;
         RectScreenPixelToBitmap (&id.rPos, &pi->rAreaDraw, pi->iImageWidth, pi->iImageHeight);

         if (pi->dwClickMode != 2) {
            // clicked or wasn't supposed to
            pControl->m_pParentPage->m_pWindow->Beep(
               (pi->dwClickMode == 1) ? ESCBEEP_LINKCLICK : ESCBEEP_DONTCLICK);

            // must release capture or bad things happen
            pControl->m_pParentPage->MouseCaptureRelease(pControl);

            // will send message that clicked
            pControl->m_pParentPage->Message (ESCN_IMAGEDRAGGED, &id);
            return TRUE;
         }

         // redraw
         pControl->Invalidate ();

         // send message that dragging
         pControl->m_pParentPage->Message (ESCN_IMAGEDRAGGING, &id);
      }
      return TRUE;

   case ESCM_MOUSEMOVE:
      if (pControl->m_fLButtonDown) {
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
         PESCMMOUSEMOVE p = (PESCMMOUSEMOVE) pParam;

         // store away location
         pi->rSelDraw.right = p->pPosn.x;
         pi->rSelDraw.bottom = p->pPosn.y;

         // redraw
         pControl->Invalidate ();

         // message
         ESCNIMAGEDRAGGED id;
         id.pControl = pControl;
         id.rPos = pi->rSelDraw;
         RectScreenPixelToBitmap (&id.rPos, &pi->rAreaDraw, pi->iImageWidth, pi->iImageHeight);
         pControl->m_pParentPage->Message (ESCN_IMAGEDRAGGING, &id);
      }
      return TRUE;


   case ESCM_LBUTTONUP:
      {
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
         PESCMLBUTTONUP p = (PESCMLBUTTONUP) pParam;

         if (pi->dwClickMode != 2)
            return TRUE;

         // store away location
         pi->rSelDraw.right = p->pPosn.x;
         pi->rSelDraw.bottom = p->pPosn.y;

         // redraw
         pControl->Invalidate ();


         // message
         ESCNIMAGEDRAGGED id;
         id.pControl = pControl;
         id.rPos = pi->rSelDraw;
         RectScreenPixelToBitmap (&id.rPos, &pi->rAreaDraw, pi->iImageWidth, pi->iImageHeight);
         pControl->m_pParentPage->Message (ESCN_IMAGEDRAGGED, &id);
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
      {
         PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
         switch (pi->dwClickMode) {
         case 1:  // click
            pControl->m_pParentPage->SetCursor ((DWORD)(size_t)IDC_ARROW);
            break;
         case 2:  // drag
            pControl->m_pParentPage->SetCursor ((DWORD)(size_t)IDC_CROSS);
            break;
         default:
            pControl->m_pParentPage->SetCursor ((DWORD)(size_t)IDC_NOCURSOR);
            break;
         }
      }
      return TRUE;

   case ESCM_ATTRIBSET:
      {
         ESCMATTRIBSET *p  =(ESCMATTRIBSET*) pParam;

         if (!p->pszValue || !p->pszAttrib)
            return FALSE;

         if (!_wcsicmp(p->pszAttrib, L"hbitmap")) {
            size_t dwResource;
            BOOL f;

            f = AttribToHex (p->pszValue, &dwResource);
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
         else if (!_wcsicmp(p->pszAttrib, L"clickmode")) {
            int iScale;
            PIMAGEINFO pi = (PIMAGEINFO)pControl->m_mem.p;
            if (AttribToDecimal (p->pszValue, &iScale))
               pi->dwClickMode = (DWORD) iScale;
            pControl->m_fWantMouse = pi->dwClickMode ? TRUE : FALSE;
            return TRUE;
         }
      }
      return FALSE;


   }

   return FALSE;
}




