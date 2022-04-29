/*************************************************************************************
CRenderTitle.cpp - Code for UI to modify a title to be rendered, and for rendering it.

begun 13/3/04 by Mike Rozak.
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





#define SCALEPREVIEW          0      // resolution of preview

static PWSTR gpszEnabledFalse = L" enabled=false ";


// TITLEITEM - Information on item to display
typedef struct {
   RECT                 rPosn;   // position, where values are from 0..1000
   PCMIFLVarString      ps;      // string to be used. Must be freed
   LOGFONT              lf;      // font to use
   COLORREF             cColor;  // color
   int                  iLRAlign;// left/right align
   int                  iTBAlign;// top/bottom align

   // extras for image
   BOOL                 fIsImage;   // set to TRUE if is an image
   BOOL                 fStretchToFit; // set to TRUE if stretch to fit
   fp                   fTransparent;  // how transparent to make the UL pixel, 0.0 to 1.0
} TITLEITEM, *PTITLEITEM;



/*************************************************************************************
RenderSceneAspectToPixelsScaleForText - If this isn't large enough, will adjust
dwWidth and dwHeight slightly larger so that small text shows up clearly

inputs
   DWORD          *pdwWidth - Initially filled in. May be scaled
   DWORD          *pdwHeight - Initially filled in. May be scaled
*/
void RenderSceneAspectToPixelsScaleForText (DWORD *pdwWidth, DWORD *pdwHeight)
{
   // BUGFIX - make sure high enough so that small text is visible
   if ((*pdwWidth < 1024) && (*pdwHeight < 1024)) {
      *pdwWidth += *pdwWidth/2;
      *pdwHeight += *pdwHeight/2;
   }
}

/*************************************************************************************
CRenderTitle::Constructor and destructor
*/
CRenderTitle::CRenderTitle (void)
{
   m_iAspect = 2;
   m_dwTab = 0;
   m_lPCCircumrealityHotSpot.Init (sizeof(PCCircumrealityHotSpot));
   m_lTITLEITEM.Init (sizeof(TITLEITEM));
   m_iVScroll = 0;

   m_acBackColor[0] = RGB(0, 0, 0x20);
   m_acBackColor[1] = RGB(0,0,0);
   m_fBackBlendLR = FALSE;
   m_szBackFile[0] = 0;
   m_pBackRend = NULL;
   m_dwBackMode = 0; // color blend
}

CRenderTitle::~CRenderTitle (void)
{
   if (m_pBackRend)
      delete m_pBackRend;
   m_pBackRend = NULL;

   // free up hotspots
   DWORD i;
   PCCircumrealityHotSpot *pphs = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   for (i = 0; i < m_lPCCircumrealityHotSpot.Num(); i++, pphs++)
      delete *pphs;
   m_lPCCircumrealityHotSpot.Clear();

   // free up titles
   PTITLEITEM pti = (PTITLEITEM) m_lTITLEITEM.Get(0);
   for (i = 0; i < m_lTITLEITEM.Num(); i++, pti++)
      pti->ps->Release();
   m_lTITLEITEM.Clear();
}


/*************************************************************************************
CRenderTitle::MMLTo - Standard API
*/
static PWSTR gpszFile = L"File";
static PWSTR gpszAspect = L"Aspect";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszBackFile = L"BackFile";
static PWSTR gpszBackColor0 = L"BackColor0";
static PWSTR gpszBackColor1 = L"BackColor1";
static PWSTR gpszBackBlendLR = L"BackBlendLR";
static PWSTR gpszHotSpot = L"HotSpot";
static PWSTR gpszLeft = L"Left";
static PWSTR gpszRight = L"Right";
static PWSTR gpszTop = L"Top";
static PWSTR gpszBottom = L"Bottom";
static PWSTR gpszMessage = L"Message";
static PWSTR gpszTitle = L"Title";
static PWSTR gpszColor = L"Color";
static PWSTR gpszLRAlign = L"LRAlign";
static PWSTR gpszTBAlign = L"TBAlign";
static PWSTR gpszFontHeight = L"FontHeight";
static PWSTR gpszFontCharSet = L"FontCharSet";
static PWSTR gpszFontItalic = L"FontItalic";
static PWSTR gpszFontStrikeOut = L"FontStrikeOut";
static PWSTR gpszFontUnderline = L"FontUnderline";
static PWSTR gpszFontWeight = L"FontWeight";
static PWSTR gpszFontFace = L"FontFace";
static PWSTR gpszFontPitchAndFamily = L"FontPitchAndFamily";
static PWSTR gpszIsImage = L"IsImage";
static PWSTR gpszStretchToFit = L"StretchToFit";
static PWSTR gpszTransparent = L"Transparent";

PCMMLNode2 CRenderTitle::MMLTo (void)
{
   DWORD i;
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityTitle());

   if (m_iAspect != 2)
      MMLValueSet (pNode ,gpszAspect, (int)m_iAspect);

   if ((m_dwBackMode == 2) && m_pBackRend) {
      PCMMLNode2 pSub = m_pBackRend->MMLTo ();
      pNode->ContentAdd (pSub);
   }
   else if ((m_dwBackMode == 1) && m_szBackFile[0])
      MMLValueSet (pNode, gpszBackFile, m_szBackFile);
   
   // NOTE: always save background color
   MMLValueSet (pNode, gpszBackColor0, (int)m_acBackColor[0]);
   MMLValueSet (pNode, gpszBackColor1, (int)m_acBackColor[1]);
   if (m_fBackBlendLR)
      MMLValueSet (pNode, gpszBackBlendLR, (int)m_fBackBlendLR);

   // write out the title items
   PTITLEITEM pti = (PTITLEITEM) m_lTITLEITEM.Get(0);
   for (i = 0; i < m_lTITLEITEM.Num(); i++, pti++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszTitle);

      MMLValueSet (pSub, gpszLeft, pti->rPosn.left);
      MMLValueSet (pSub, gpszRight, pti->rPosn.right);
      MMLValueSet (pSub, gpszTop, pti->rPosn.top);
      MMLValueSet (pSub, gpszBottom, pti->rPosn.bottom);

      PWSTR psz = pti->ps->Get();
      if (psz[0])
         MMLValueSet (pSub, gpszMessage, psz);

      if (pti->fIsImage) {
         MMLValueSet (pSub, gpszIsImage, (int)pti->fIsImage);
         if (pti->fStretchToFit)
            MMLValueSet (pSub, gpszStretchToFit, (int)pti->fStretchToFit);
         if (pti->fTransparent)
            MMLValueSet (pSub, gpszTransparent, pti->fTransparent);
      }
      else {
         // text
         MMLValueSet (pSub, gpszColor, (int) pti->cColor);
         if (pti->iLRAlign)
            MMLValueSet (pSub, gpszLRAlign, (int) pti->iLRAlign);
         if (pti->iTBAlign)
            MMLValueSet (pSub, gpszTBAlign, (int) pti->iTBAlign);

         MMLValueSet (pSub, gpszFontHeight, (int) pti->lf.lfHeight);
         if (pti->lf.lfCharSet != ANSI_CHARSET)
            MMLValueSet (pSub, gpszFontCharSet, (int) pti->lf.lfCharSet);
         if (pti->lf.lfItalic)
            MMLValueSet (pSub, gpszFontItalic, (int) pti->lf.lfItalic);
         if (pti->lf.lfStrikeOut)
            MMLValueSet (pSub, gpszFontStrikeOut, (int) pti->lf.lfStrikeOut);
         if (pti->lf.lfUnderline)
            MMLValueSet (pSub, gpszFontUnderline, (int) pti->lf.lfUnderline);
         if (pti->lf.lfWeight != FW_REGULAR)
            MMLValueSet (pSub, gpszFontWeight, (int) pti->lf.lfWeight);
         MMLValueSet (pSub, gpszFontPitchAndFamily, (int) pti->lf.lfPitchAndFamily);

         WCHAR szFace[LF_FACESIZE];
         MultiByteToWideChar (CP_ACP, 0, pti->lf.lfFaceName, -1, szFace, sizeof(szFace)/sizeof(WCHAR));
         if (szFace[0])
            MMLValueSet (pSub, gpszFontFace, szFace);
      }
   }  // i


   // write out the hot spots
   PCCircumrealityHotSpot *pph;
   pph = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   for (i = 0; i < m_lPCCircumrealityHotSpot.Num(); i++, pph++) {
      (*pph)->m_lid = m_lid;
      PCMMLNode2 pSub = (*pph)->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszHotSpot);
      pNode->ContentAdd (pSub);
   } // i

   // write out transition node
   PCMMLNode2 pSub = m_Transition.MMLTo ();
   if (pSub)
      pNode->ContentAdd (pSub);


   return pNode;
}



/*************************************************************************************
CRenderTitle:MMLFrom - Standard API
*/
BOOL CRenderTitle::MMLFrom (PCMMLNode2 pNode)
{
   // free up hotspots
   DWORD i;
   PCCircumrealityHotSpot *pphs = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   for (i = 0; i < m_lPCCircumrealityHotSpot.Num(); i++, pphs++)
      delete (*pphs);
   m_lPCCircumrealityHotSpot.Clear();

   // free up titles
   PTITLEITEM pti = (PTITLEITEM) m_lTITLEITEM.Get(0);
   for (i = 0; i < m_lTITLEITEM.Num(); i++, pti++)
      pti->ps->Release();
   m_lTITLEITEM.Clear();

   if (m_pBackRend)
      delete m_pBackRend;
   m_pBackRend = NULL;

   PWSTR psz;
   m_iAspect = (int) MMLValueGetInt (pNode ,gpszAspect, 2);

   PCMMLNode2 pSub;
   psz = MMLValueGet (pNode, gpszBackFile);
   m_dwBackMode = 0;
   if (psz) {
      wcscpy (m_szBackFile, psz);
      m_dwBackMode = 1;
   }
   else
      m_szBackFile[0] = 0;

   m_acBackColor[0] = MMLValueGetInt (pNode, gpszBackColor0, (int)m_acBackColor[0]);
   m_acBackColor[1] = MMLValueGetInt (pNode, gpszBackColor1, (int)m_acBackColor[1]);
   m_fBackBlendLR = (BOOL) MMLValueGetInt (pNode, gpszBackBlendLR, FALSE);


   // fill in the hot spots
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszTitle)) {
         TITLEITEM ti;
         memset (&ti, 0, sizeof(ti));
         ti.ps = new CMIFLVarString;
         if (!ti.ps)
            continue;

         ti.rPosn.left = MMLValueGetInt (pSub, gpszLeft, 0);
         ti.rPosn.right = MMLValueGetInt (pSub, gpszRight, 1000);
         ti.rPosn.top = MMLValueGetInt (pSub, gpszTop, 0);
         ti.rPosn.bottom = MMLValueGetInt (pSub, gpszBottom, 1000);

         PWSTR psz = MMLValueGet (pSub, gpszFontFace);
         if (psz)
            WideCharToMultiByte (CP_ACP, 0, psz, -1, ti.lf.lfFaceName, sizeof(ti.lf.lfFaceName), 0, 0);

         psz = MMLValueGet (pSub, gpszMessage);
         ti.ps->Set (psz ? psz : L"");

         ti.fIsImage = MMLValueGetInt (pSub, gpszIsImage, (int)0);
         if (ti.fIsImage) {
            ti.fStretchToFit = (BOOL) MMLValueGetInt (pSub, gpszStretchToFit, (int)FALSE);
            ti.fTransparent = MMLValueGetDouble (pSub, gpszTransparent, 0);
         }
         else {
            ti.cColor = MMLValueGetInt (pSub, gpszColor, (int) RGB(0xff,0xff,0xff));
            ti.iLRAlign = MMLValueGetInt (pSub, gpszLRAlign, (int) 0);
            ti.iTBAlign = MMLValueGetInt (pSub, gpszTBAlign, (int) 0);

            ti.lf.lfHeight = MMLValueGetInt (pSub, gpszFontHeight, (int) 24);
            ti.lf.lfCharSet = MMLValueGetInt (pSub, gpszFontCharSet, (int) ANSI_CHARSET);
            ti.lf.lfItalic = MMLValueGetInt (pSub, gpszFontItalic, (int) 0);
            ti.lf.lfStrikeOut = MMLValueGetInt (pSub, gpszFontStrikeOut, (int) 0);
            ti.lf.lfUnderline = MMLValueGetInt (pSub, gpszFontUnderline, (int) 0);
            ti.lf.lfWeight = MMLValueGetInt (pSub, gpszFontWeight, (int) FW_REGULAR);
            ti.lf.lfPitchAndFamily = MMLValueGetInt (pSub, gpszFontPitchAndFamily, (int) FF_SWISS | VARIABLE_PITCH);
         }
   

         // add it
         m_lTITLEITEM.Add (&ti);
         continue;
      }
      else if (!_wcsicmp(psz, gpszHotSpot)) {
         PCCircumrealityHotSpot pNew = new CCircumrealityHotSpot;
         if (!pNew)
            continue;
         pNew->MMLFrom (pSub, m_lid);
         m_lid = pNew->m_lid; // in case different

         // add it
         m_lPCCircumrealityHotSpot.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, Circumreality3DScene()) || !_wcsicmp(psz, Circumreality3DObjects())) {
            // BUGFIX - Also allow 3d objects
         if (m_pBackRend)
            delete m_pBackRend;
         m_pBackRend = new CRenderScene;
         if (!m_pBackRend)
            continue;
         m_pBackRend->m_fLoadFromFile = !_wcsicmp(psz, Circumreality3DScene());
         m_pBackRend->MMLFrom (pSub);

         m_dwBackMode = 2;
         continue;
      }
   } // i

   // read in the transition
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(CircumrealityTransition()), &psz, &pSub);
   m_Transition.MMLFrom (pSub);

   return TRUE;
}

/*************************************************************************************
DrawImageToImage - This draws an image to an image.

inputs
   PWSTR       pszText - File to paint
   PCImage     pImage - To draw to
   RECT        *prBound - Bounding area for the text
   BOOL        fStretchToFit - If TRUE then stretch to fit. Else, size to fit.
   fp          fTransparency - Amount of transparency
   HWND        hWnd - Window to get DC from
returns
   BOOL - TRUE if success
*/

BOOL DrawImageToImage (PWSTR pszText, PCImage pImage, RECT *prBound, BOOL fStretchToFit,
                      fp fTransparency, HWND hWnd)
{
   // source image
   CImage Source;
   if (!pszText || !pszText[0] || !Source.Init (pszText))
      return FALSE;
   if (!Source.Width() || !Source.Height())
      return FALSE;
   if ((prBound->right <= prBound->left) || (prBound->bottom <= prBound->top))
      return TRUE;   // nothing to draw to

   // figure out transparent color and amount
   PIMAGEPIXEL pip = Source.Pixel (0,0);
   WORD awTransMin[3], awTransMax[3];
   memcpy (awTransMin, &pip->wRed, sizeof(awTransMin));
   DWORD i;
   WORD wDist = (WORD)(fTransparency * (fp)0xffff);
   for (i = 0; i < 3; i++) {
      if (!wDist) {
         // make sure not transparent
         awTransMin[i] = 0xffff;
         awTransMax[i] = 0;
         continue;
      }

      awTransMax[i] = (WORD) min((DWORD)awTransMin[i] + (DWORD)wDist, 0xffff);
      if (awTransMin[i] >= wDist)
         awTransMin[i] -= wDist;
      else
         awTransMin[i] = 0;
   } // i

   // width of destination
   int iWidthDest = prBound->right - prBound->left;
   int iHeightDest = prBound->bottom - prBound->top;

   // scale amount
   fp fScaleX = 1, fScaleY = 1;
   fp fDeltaX = 0, fDeltaY = 0;
   if (fStretchToFit) {
      fScaleX = (fp)iWidthDest / (fp)Source.Width();
      fScaleY = (fp)iHeightDest / (fp)Source.Height();
   }
   else {
      // aspect ratio
      fp fAspectDest = (fp)iWidthDest / (fp)iHeightDest;
      fp fAspectSource = (fp)Source.Width() / (fp)Source.Height();
      if (fAspectDest >= fAspectSource)
         fScaleX = fScaleY = (fp)iHeightDest / (fp)Source.Height();
      else
         fScaleX = fScaleY = (fp)iWidthDest / (fp)Source.Width();

      fDeltaX = ((fp)iWidthDest - (fp)Source.Width() * fScaleX) / 2.0;
      fDeltaY = ((fp)iHeightDest - (fp)Source.Height() * fScaleY) / 2.0;
   }

   // figure out the antialiasing amount
   int iAntiX = (int) ceil(1.0 / fScaleX / 2.0 - 0.5);
   int iAntiY = (int) ceil(1.0 / fScaleX / 2.0 - 0.5);
   iAntiX = max(iAntiX, 0);
   iAntiY = max(iAntiY, 0);

   // loop over all the pixels on the destination
   int iDestX, iDestY, iSourceX, iSourceY;
   int iImageWidth = (int)pImage->Width();
   int iSourceWidth = (int)Source.Width();
   RECT rRange;
   POINT pRange;
   PIMAGEPIXEL pipSource, pipDest;
   DWORD dwTransparentCount, dwCount;
   DWORD adwColor[3];
   for (iDestY = prBound->top; iDestY < prBound->bottom; iDestY++) {
      if ((iDestY < 0) || (iDestY >= (int)pImage->Height()))
         continue;

      // BUGBUG - need to test with scale-to-fit center vs. left

      pRange.y = (fp)(iDestY - prBound->top - fDeltaY) / fScaleY;
      rRange.top = max (pRange.y - iAntiY, 0);
      rRange.bottom = min (pRange.y + iAntiY + 1, (int)Source.Height());
      if (rRange.bottom <= rRange.top)
         continue;

      for (iDestX = prBound->left; iDestX < prBound->right; iDestX++) {
         if ((iDestX < 0) || (iDestX >= iImageWidth))
            continue;

         // figure out the source range
         pRange.x = (fp)(iDestX - prBound->left - fDeltaX) / fScaleX;
         rRange.left = max (pRange.x - iAntiX, 0);
         rRange.right = min (pRange.x + iAntiX + 1, iSourceWidth);
         if (rRange.right <= rRange.left)
            continue;

         // loop over all these points to antialias
         dwTransparentCount = 0;
         memset (adwColor, 0, sizeof(adwColor));
         for (iSourceY = rRange.top; iSourceY < rRange.bottom; iSourceY++) {
            pipSource = Source.Pixel ((DWORD)rRange.left, (DWORD)iSourceY);
            for (iSourceX = rRange.left; iSourceX < rRange.right; iSourceX++, pipSource++) {
               if ((pipSource->wRed >= awTransMin[0]) && (pipSource->wRed <= awTransMax[0]) &&
                  (pipSource->wGreen >= awTransMin[1]) && (pipSource->wGreen <= awTransMax[2]) &&
                  (pipSource->wBlue >= awTransMin[1]) && (pipSource->wBlue <= awTransMax[2]) ) {
                     dwTransparentCount++;
                     continue;
                  }

               // else, color
               adwColor[0] += (DWORD)pipSource->wRed;
               adwColor[1] += (DWORD)pipSource->wGreen;
               adwColor[2] += (DWORD)pipSource->wBlue;
            } // iSourceX
         } // iSourceY

         dwCount = (DWORD)((rRange.right - rRange.left) * (rRange.bottom - rRange.top));
         if (dwTransparentCount >= dwCount)
            continue;   // all transparent

         // else, add destination color
         pipDest = pImage->Pixel ((DWORD)iDestX, (DWORD)iDestY);
         if (dwTransparentCount) {
            // add transparent background
            adwColor[0] += (DWORD)pipDest->wRed * dwTransparentCount;
            adwColor[1] += (DWORD)pipDest->wGreen * dwTransparentCount;
            adwColor[2] += (DWORD)pipDest->wBlue * dwTransparentCount;
         }

         // write
         pipDest->wRed = (WORD)(adwColor[0] / dwCount);
         pipDest->wGreen = (WORD)(adwColor[1] / dwCount);
         pipDest->wBlue = (WORD)(adwColor[2] / dwCount);

         // make sure have an ID so keep around
         if (!pip->dwID)
            pip->dwID = 1;
      } // iDestX
   } // iDestY

   return TRUE;
}

/*************************************************************************************
DrawTextToImage - This draws antialiased text to an image.

inputs
   PWSTR       pszText - Text to draw
   PCImage     pImage - To draw to
   RECT        *prBound - Bounding area for the text
   int         iLRAlign - -1 = left align, 0 = center, 1 = right align
   int         iTBAlign - -1 = top align, 0 = center, 1 = bottom align
   PLOGFONT    plf - Font to use
   COLORREF    cColor - Color to use
   HWND        hWnd - Window to get DC from
returns
   BOOL - TRUE if success
*/
#define TEXTANTI     2        // amount to antialias text on image

BOOL DrawTextToImage (PWSTR pszText, PCImage pImage, RECT *prBound, int iLRAlign, int iTBAlign,
                      PLOGFONT plf, COLORREF cColor, HWND hWnd)
{
   // color
   WORD awColor[3];
   GammaInit ();
   Gamma (cColor, awColor);

   // convert the text
   CMem memAnsi;
   if (!memAnsi.Required ((wcslen(pszText)+1)*sizeof(WCHAR)))
      return FALSE;
   PSTR psz = (PSTR)memAnsi.p;
   WideCharToMultiByte (CP_ACP, 0, pszText, -1, psz, (DWORD)memAnsi.m_dwAllocated, 0, 0);

   // width?
   if ((prBound->right < prBound->left) || (prBound->bottom < prBound->top))
      return TRUE;   // cant draw there
   DWORD dwWidth = (DWORD)(prBound->right - prBound->left) * TEXTANTI;
   DWORD dwHeight = (DWORD)(prBound->bottom - prBound->top) * TEXTANTI;

   // create compatible DC
   CImage ImageText;
   HDC hDC = GetDC (hWnd);
   if (!hDC)
      return FALSE;
   HDC hDCDraw = CreateCompatibleDC (hDC);
   HBITMAP hBit = CreateCompatibleBitmap (hDC, dwWidth, dwHeight);
   ReleaseDC (hWnd, hDC);
   if (!hDCDraw || !hBit) {
      if (hDCDraw)
         DeleteDC (hDCDraw);
      if (hBit)
         DeleteObject (hBit);
      return FALSE;
   }
   SelectObject (hDCDraw, hBit);

   // create the font
   plf->lfHeight *= TEXTANTI;
   plf->lfWidth *= TEXTANTI;
   HFONT hFont = CreateFontIndirect (plf);
   plf->lfHeight /= TEXTANTI;
   plf->lfWidth /= TEXTANTI;
   if (!hFont) {
      DeleteDC (hDCDraw);
      DeleteObject (hBit);
      return FALSE;
   }

   // wipe background and draw font
   RECT r;
   DWORD dwFlags = DT_WORDBREAK;
   if (iLRAlign < 0)
      dwFlags |= DT_LEFT;
   else if (iLRAlign > 0)
      dwFlags |= DT_RIGHT;
   else
      dwFlags |= DT_CENTER;
   if (iTBAlign < 0)
      dwFlags |= DT_TOP;
   else if (iTBAlign > 0)
      dwFlags |= DT_BOTTOM;
   else
      dwFlags |= DT_VCENTER;

   r.top = r.left = 0;
   r.right = (int)dwWidth;
   r.bottom = (int)dwHeight;
   FillRect (hDCDraw, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
   SetTextColor (hDCDraw, RGB(0xff,0xff,0xff));
   SetBkColor (hDCDraw, RGB(0,0,0));
   SelectObject (hDCDraw, hFont);
   DrawText (hDCDraw, psz, -1, &r, DT_WORDBREAK | DT_LEFT | DT_TOP | DT_CALCRECT);
   int iOffX = 0, iOffY = 0;
   if (iLRAlign > 0)
      iOffX = (int)dwWidth - r.right;
   else if (iLRAlign == 0)
      iOffX = ((int)dwWidth - r.right)/2;
   if (iTBAlign > 0)
      iOffY = (int)dwHeight - r.bottom;
   else if (iTBAlign == 0)
      iOffY = ((int)dwHeight - r.bottom)/2;
   r.left += iOffX;
   r.right += iOffX;
   r.top += iOffY;
   r.bottom += iOffY;
   DrawText (hDCDraw, psz, -1, &r, dwFlags);

   DeleteDC (hDCDraw);
   DeleteObject (hFont);
   if (!ImageText.Init (hBit)) {
      DeleteObject (hBit);
      return FALSE;
   }
   DeleteObject (hBit);

   // transfer these pixels over
   int x, y;
   RECT rTemp;
   DWORD dwImageWidth = pImage->Width();
   DWORD dwImageHeight = pImage->Height();
   rTemp = *prBound;
   rTemp.top = max(rTemp.top, r.top/TEXTANTI - 1 + prBound->top);
   rTemp.bottom = min(rTemp.bottom, r.bottom/TEXTANTI + 1 + prBound->top);
   rTemp.top = max(rTemp.top, 0);
   rTemp.bottom = min(rTemp.bottom, (int)dwImageHeight);
   rTemp.left = max(rTemp.left, r.left/TEXTANTI - 1 + prBound->left);
   rTemp.right = min(rTemp.right, r.right/TEXTANTI + 1 + prBound->right);
   for (y = rTemp.top; y < rTemp.bottom; y++) {
      int iYOrig = (y - prBound->top) * TEXTANTI;
      // Below is not needed because included in rTemp
      //if ((iYOrig + TEXTANTI-1 < r.top) || (iYOrig >= r.bottom))
      //   continue;   // not bounded

      for (x = rTemp.left; x < rTemp.right; x++) {
         // get the pixels from the original
         DWORD dwSum = 0;
         int iXOrig = (x - prBound->left) * TEXTANTI;
         // Below is not needed because included in rTemp
         //if ((iXOrig + TEXTANTI-1 < r.left) || (iXOrig >= r.right))
         //   continue;   // not bounded

         int xx, yy;
         for (yy = 0; yy < TEXTANTI; yy++) {
            PIMAGEPIXEL pipText = ImageText.Pixel ((DWORD)iXOrig, (DWORD)iYOrig + yy);
            for (xx = 0; xx < TEXTANTI; xx++, pipText++)
               dwSum += (DWORD)pipText->wRed;
         } // yy
         if (!dwSum)
            continue;   // nothing there
         dwSum /= (TEXTANTI * TEXTANTI);
         DWORD dwSumInv = (DWORD)0xffff - dwSum;

         xx = x;
         if ((xx < 0) || (xx >= (int)dwImageWidth)) {
            if (!pImage->m_f360)
               continue;   // out of bounds

            while (xx < 0)
               xx += (int)dwImageWidth;
            while (xx >= (int)dwImageWidth)
               xx -= (int)dwImageWidth;
         } // if out of range

         PIMAGEPIXEL pipTo = pImage->Pixel ((DWORD)xx,(DWORD)y);
         pipTo->wRed = (WORD)(((DWORD)pipTo->wRed * dwSumInv + (DWORD)awColor[0] * dwSum) >> 16);
         pipTo->wGreen = (WORD)(((DWORD)pipTo->wGreen * dwSumInv + (DWORD)awColor[1] * dwSum) >> 16);
         pipTo->wBlue = (WORD)(((DWORD)pipTo->wBlue * dwSumInv + (DWORD)awColor[2] * dwSum) >> 16);

         // make sure have object ID
         if (!pipTo->dwID)
            pipTo->dwID = 1;
      } // x
   } // y

   return TRUE;
}


/*************************************************************************************
CRenderTitle::Render - Draws the image.

inputs
   fp          fScale - Amount to scale up/down from the standard image size.
                        1.0 = no change, 2.0 = 2x as large (4x the pixels), etc.
   BOOL        fFinalRender - If TRUE draw this as the final render, if FALSE
                        then use using a test render, which means one step lower quality
                        and no final-render flag sent for rendering
   BOOL        fForceReload - If TRUE then reload the scene even if it has already been loaded
   BOOL        fBlankIfFail - If the image fails to load, and this is TRUE, will create
                        a blank image instead. Otherwise, if fails will return null.
   DWORD       dwShadowsFlags - A set of SF_XXX flags from CRenderTraditional
   PCProgressSocket pProgres - Progress bar
   PCMegaFile  pMegaFile - This is the megafile where to get the background 3d scenes from.
                        If NULL then not used.
   BOOL        *pf360 - Filled with TRUE if this is a 360 degree image, FALSE if ordinary camera
   PCMem       pMem360Calc - If not NULL, used for 360 render optimization
returns
   PCImage     pImage - Image. NOTE: If the file can't be loaded the image will be a blank one.
                        It should never return NULL (except out of memory)
*/
PCImage CRenderTitle::Render (DWORD dwRenderShard, fp fScale, BOOL fFinalRender, BOOL fForceReload,
                              BOOL fBlankIfFail, DWORD dwShadowsFlags, PCProgressSocket pProgress,
                              PCMegaFile pMegaFile, BOOL *pf360, PCMem pMem360Calc)
{
   PCImage pImage = NULL;
   PCImageStore pIStore = NULL;
   PCImage pImageTemp = NULL;
   DWORD i;
   BOOL fExtraPush = FALSE;

   if (pf360)
      *pf360 = (m_iAspect == 10);

   // if have jpeg image then load that
   if (m_szBackFile[0]) {
      pIStore = new CImageStore;
      if (pIStore && !pIStore->Init (m_szBackFile, TRUE)) {
         delete pIStore;
         pIStore = NULL;
      }
   }
   else if (m_pBackRend) {
      // if there's a megafile see if can get from there...
      CMem mem;
      PCMMLNode2 pNode = NULL;
      BOOL fLoaded = FALSE;
      pIStore = new CImageStore;
      if (pMegaFile) {
         pNode = m_pBackRend->MMLTo();
         if (pNode && MMLToMem (pNode, &mem))
            mem.CharCat (0);
         else
            mem.m_dwCurPosn = 0; // error
      }
      if (pMegaFile && pIStore && pIStore->Init (pMegaFile, (PWSTR)mem.p))
         fLoaded = TRUE;

      // if we haven't loaded image then render one
      if (!fLoaded) {
         fExtraPush = TRUE;

         if (pProgress)
            pProgress->Push (0, 0.9);
         PCImageStore pRend = (PCImageStore) m_pBackRend->Render (dwRenderShard, fScale, fFinalRender, fForceReload,
            FALSE, dwShadowsFlags, pProgress, TRUE, pMegaFile, NULL, NULL, pMem360Calc);
         if (pProgress) {
            pProgress->Pop();
            pProgress->Push (0.9, 1);
         }
         if (pRend) {
            if (pIStore)
               delete pIStore;
            pIStore = pRend;
         }
         else {
            delete pIStore;
            pIStore = NULL;
         }
      }


      // save the image to megafile?
      if (!fLoaded && pMegaFile && pIStore && mem.m_dwCurPosn) {
         pIStore->MMLSet (pNode);
         pIStore->ToMegaFile (pMegaFile, (PWSTR)mem.p);
      }

      // delete
      if (pNode)
         delete pNode;
   }

   //determine the real width and height
   DWORD dwWidth, dwHeight;
   RenderSceneAspectToPixelsInt (m_iAspect, fScale * m_Transition.Scale() * 2, &dwWidth, &dwHeight);
      // BUGFIX - Titles always have doubled resolution because if the text is small,
      // it needs to be readable

   RenderSceneAspectToPixelsScaleForText (&dwWidth, &dwHeight);


   // determine the average background color
   GammaInit();
   WORD awBack[2][3];
   COLORREF cBack;
   Gamma (m_acBackColor[0], awBack[0]);
   Gamma (m_acBackColor[1], awBack[1]);
   for (i = 0; i < 3; i++)
      awBack[0][i] = awBack[0][i] / 2 + awBack[1][i] / 2;
   cBack = UnGamma (awBack[0]);

   // create the integer image where ultimately goes
   pImage = new CImage;
   if (!pImage) {
      if (pProgress) pProgress->Pop();
      goto blankimage;
   }
   if (!pImage->Init (dwWidth, dwHeight, cBack)) {
      if (pProgress) pProgress->Pop();
      goto blankimage;
   }
   if (m_iAspect == 10)
      pImage->m_f360 = TRUE;

   // blended color background, if no jpeg or background image
   // or blend in jpeg or image
   if (pIStore)
      ImageBlendImageStore (pImage, pIStore, m_iAspect == 10, TRUE);
   else
      ImageBlendedBack (pImage, m_acBackColor[0], m_acBackColor[1], m_fBackBlendLR, TRUE);
         // BUGFIX - Titles will include blended color

   if (pProgress)
      pProgress->Pop();
   if (pProgress && fExtraPush)
      pProgress->Pop();

   // draw all the bits of text
   PTITLEITEM pti = (PTITLEITEM)m_lTITLEITEM.Get(0);
   RECT rBound;
   LOGFONT lf;
   for (i = 0; i < m_lTITLEITEM.Num(); i++, pti++) {
      rBound = pti->rPosn;
      RenderSceneHotSpotToImage (&rBound, dwWidth, dwHeight);
      lf = pti->lf;
      lf.lfHeight = lf.lfHeight * (int)dwWidth / 512;
      lf.lfWidth = lf.lfWidth * (int) dwWidth / 512;

      if (pti->fIsImage)
         DrawImageToImage (pti->ps->Get(), pImage, &rBound, pti->fStretchToFit, pti->fTransparent,
            GetDesktopWindow());
      else
         DrawTextToImage (pti->ps->Get(), pImage, &rBound, pti->iLRAlign, pti->iTBAlign,
            &lf, pti->cColor, GetDesktopWindow());
   } // i

   if (pImageTemp)
      delete pImageTemp;
   if (pIStore)
      delete pIStore;


   return pImage;

blankimage:
   if (pImageTemp)
      delete pImageTemp;

   if (pIStore)
      delete pIStore;

   if (!fBlankIfFail) {
      // in some cases may want to know that error actually occured
      if (pImage)
         delete pImage;
      return NULL;
   }

   if (pProgress && fExtraPush)
      pProgress->Pop();

   if (!pImage)
      pImage = new CImage;
   if (!pImage)
      return NULL;
   pImage->Init (dwWidth, dwHeight, RGB(0,0,0));
   return pImage;
}

/*************************************************************************
RenderTitlePage
*/
BOOL RenderTitlePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCRenderTitle prs = (PCRenderTitle)pPage->m_pUserData;   // node to modify
   DWORD dwRenderShard = prs->m_dwRenderShardTemp;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (prs->m_iVScroll > 0) {
            pPage->VScroll (prs->m_iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            prs->m_iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;
         DWORD i, j;
         WCHAR szTemp[64];

         // diable rotation and FOV controls if 360 degree
         if (prs->m_iAspect == 10) {
            pControl = pPage->ControlFind (L"fov");
            if (pControl)
               pControl->Enable (FALSE);

            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"rot%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (FALSE);
            }
         }



         ComboBoxSet (pPage, L"aspect", (DWORD) max(prs->m_iAspect, 0) );

         // image width and height
         DWORD dwWidth, dwHeight;
         RenderSceneAspectToPixelsInt (prs->m_iAspect, SCALEPREVIEW, &dwWidth, &dwHeight);
         RenderSceneAspectToPixelsScaleForText (&dwWidth, &dwHeight);

         // fill in the controls
         if (prs->m_dwTab == 0) {
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(0);
            DWORD i;
            WCHAR szTemp[64];
            PCEscControl pControl;
            CListFixed lCD;
            lCD.Init (sizeof(CONTROLIMAGEDRAGRECT));
            CONTROLIMAGEDRAGRECT cd;
            memset (&cd, 0, sizeof(cd));
            for (i = 0; i < prs->m_lTITLEITEM.Num(); i++, pti++) {
               swprintf (szTemp, L"titletext%d", (int)i);
               if (pControl = pPage->ControlFind (szTemp))
                  pControl->AttribSet (Text(), pti->ps->Get());

               // combo
               swprintf (szTemp, L"titlelr%d", (int)i);
               ComboBoxSet (pPage, szTemp, (DWORD)(pti->iLRAlign + 1));
               swprintf (szTemp, L"titletb%d", (int)i);
               ComboBoxSet (pPage, szTemp, (DWORD)(pti->iTBAlign + 1));

               // stretch to fit
               swprintf (szTemp, L"stretchtofit%d", (int)i);
               ComboBoxSet (pPage, szTemp, (DWORD)pti->fStretchToFit);

               swprintf (szTemp, L"transparency%d", (int)i);
               if (pControl = pPage->ControlFind (szTemp))
                  pControl->AttribSetInt (Pos(), (int) (pti->fTransparent * 100.0));

               // title posn
               for (j = 0; j < 4; j++) {
                  swprintf (szTemp, L"titleposn%d%d", (int)j, (int)i);
                  int iVal;
                  switch (j) {
                  case 0:
                     iVal = pti->rPosn.left;
                     break;
                  case 1:
                     iVal = pti->rPosn.right;
                     break;
                  case 2:
                     iVal = pti->rPosn.top;
                     break;
                  case 3:
                     iVal = pti->rPosn.bottom;
                     break;
                  }
                  DoubleToControl (pPage, szTemp, (fp)iVal / 10.0);
               } // j

               // add to list that will send to bitmap
               cd.cColor = HotSpotColor(i);
               cd.fModulo = (prs->m_iAspect == 10);
               cd.rPos = pti->rPosn;
               if (dwWidth || dwHeight)
                  RenderSceneHotSpotToImage (&cd.rPos, dwWidth, dwHeight);
               lCD.Add (&cd);
            } // i

            // send message to image to show hotspots
            ESCMIMAGERECTSET is;
            memset (&is, 0, sizeof(is));
            is.dwNum = lCD.Num();
            is.pRect = (PCONTROLIMAGEDRAGRECT)lCD.Get(0);
            pControl = pPage->ControlFind (L"image");
            if (pControl)
               pControl->Message (ESCM_IMAGERECTSET, &is);
         }
         else if (prs->m_dwTab == 5)
            HotSpotInitPage (pPage, &prs->m_lPCCircumrealityHotSpot, prs->m_iAspect == 10,
               dwWidth, dwHeight);

         // set the language for the hotspots
         MIFLLangComboBoxSet (pPage, L"langid", prs->m_lid, prs->m_pProj);

         if (prs->m_dwTab == 9) {   // background
            // check the radio button
            swprintf (szTemp, L"backmode%d", (int)prs->m_dwBackMode);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);

            // fill in the colors
            for (i = 0; i < 2; i++) {
               swprintf (szTemp, L"backcolor%d", (int) i);
               FillStatusColor (pPage, szTemp, prs->m_acBackColor[i]);
            }

            // check the blended LR
            pControl = pPage->ControlFind (L"backblendlr");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), prs->m_fBackBlendLR);

            // fill in the background file
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);
         } // tab==9
      }
      break;

   case ESCN_SCROLL:
   // case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszTransparency = L"transparency";
         DWORD dwTransparencyLen = (DWORD)wcslen(pszTransparency);
         if (!wcsncmp(psz, pszTransparency, dwTransparencyLen)) {
            DWORD dwIndex = _wtoi(psz + dwTransparencyLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            pti->fTransparent = (fp) p->iPos / 100.0;

            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszRSAttribNum = L"rsattribnum";
         DWORD dwRSAttribNumLen = (DWORD)wcslen(pszRSAttribNum);

         PWSTR pszTitleLR = L"titlelr", pszTitleTB = L"titletb", pszStretchToFit = L"stretchtofit";
         DWORD dwTitleLRLen = (DWORD)wcslen(pszTitleLR), dwTitleTBLen = (DWORD)wcslen(pszTitleTB),
            dwStretchToFitLen = (DWORD)wcslen(pszStretchToFit);

         if (!wcsncmp(psz, pszStretchToFit, dwStretchToFitLen)) {
            DWORD dwIndex = _wtoi(psz + dwStretchToFitLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (iVal == (int) pti->fStretchToFit)
               return TRUE;

            // else changed
            pti->fStretchToFit = (BOOL) iVal;
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszTitleLR, dwTitleLRLen)) {
            DWORD dwIndex = _wtoi(psz + dwTitleLRLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            iVal -= 1;
            if (iVal == pti->iLRAlign)
               return TRUE;

            // else changed
            pti->iLRAlign = iVal;
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszTitleTB, dwTitleTBLen)) {
            DWORD dwIndex = _wtoi(psz + dwTitleTBLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            iVal -= 1;
            if (iVal == pti->iTBAlign)
               return TRUE;

            // else changed
            pti->iTBAlign = iVal;
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (HotSpotComboBoxSelChanged (pPage, p, &prs->m_lPCCircumrealityHotSpot, &prs->m_fChanged))
            return TRUE;
         else if (!_wcsicmp(psz, L"langid")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            LANGID *padw = (LANGID*)prs->m_pProj->m_lLANGID.Get(dwVal);
            dwVal = padw ? padw[0] : prs->m_lid;
            if (dwVal == prs->m_lid)
               return TRUE;

            // else changed
            prs->m_lid = (LANGID)dwVal;
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"aspect")) {
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (iVal == prs->m_iAspect)
               return TRUE;

            prs->m_iAspect = iVal;
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage()); // need to do with aspect change
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszTitleText = L"titletext", pszTitlePosn = L"titleposn";
         DWORD dwTitleTextLen = (DWORD)wcslen(pszTitleText),
            dwTitlePosnLen = (DWORD)wcslen(pszTitlePosn);

         if (!wcsncmp(psz, pszTitlePosn, dwTitlePosnLen)) {
            DWORD dwDim = psz[dwTitlePosnLen] - L'0';
            DWORD dwIndex = _wtoi(psz + (dwTitlePosnLen+1));
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            fp f = DoubleFromControl (pPage, psz) * 10.0;

            switch (dwDim) {
            case 0:
               pti->rPosn.left = (int)f;
               break;
            case 1:
               pti->rPosn.right = (int)f;
               break;
            case 2:
               pti->rPosn.top = (int)f;
               break;
            case 3:
               pti->rPosn.bottom = (int)f;
               break;
            }

            prs->m_fChanged = TRUE;

            // NOTE: Don't redraw image because might be too slow

            // set the border though
            DWORD dwWidth, dwHeight;
            RenderSceneAspectToPixelsInt (prs->m_iAspect, SCALEPREVIEW, &dwWidth, &dwHeight);
            RenderSceneAspectToPixelsScaleForText (&dwWidth, &dwHeight);
            pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(0);
            DWORD i;
            PCEscControl pControl;
            CListFixed lCD;
            lCD.Init (sizeof(CONTROLIMAGEDRAGRECT));
            CONTROLIMAGEDRAGRECT cd;
            memset (&cd, 0, sizeof(cd));
            for (i = 0; i < prs->m_lTITLEITEM.Num(); i++, pti++) {
               // add to list that will send to bitmap
               cd.cColor = HotSpotColor(i);
               cd.fModulo = (prs->m_iAspect == 10);
               cd.rPos = pti->rPosn;
               if (dwWidth || dwHeight)
                  RenderSceneHotSpotToImage (&cd.rPos, dwWidth, dwHeight);
               lCD.Add (&cd);
            } // i

            // send message to image to show hotspots
            ESCMIMAGERECTSET is;
            memset (&is, 0, sizeof(is));
            is.dwNum = lCD.Num();
            is.pRect = (PCONTROLIMAGEDRAGRECT)lCD.Get(0);
            pControl = pPage->ControlFind (L"image");
            if (pControl)
               pControl->Message (ESCM_IMAGERECTSET, &is);

            return TRUE;
         }
         else if (!wcsncmp(psz, pszTitleText, dwTitleTextLen)) {
            DWORD dwIndex = _wtoi(psz + dwTitleTextLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            WCHAR szTemp[10000];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            pti->ps->Set(szTemp);
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (HotSpotEditChanged (pPage, p, &prs->m_lPCCircumrealityHotSpot, &prs->m_fChanged))
            return TRUE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszBackMode = L"backmode", pszChangeBackColor = L"changebackcolor",
            pszImageOpen = L"imageopen";
         DWORD dwBackModeLen = (DWORD)wcslen(pszBackMode), dwChangeBackColorLen = (DWORD)wcslen(pszChangeBackColor),
            dwImageOpenLen = (DWORD)wcslen(pszImageOpen);

         PWSTR pszTitleDel = L"titledel", pszTitleFont = L"titlefont",
            pszTitleColor = L"titlecolor", pszIsImage = L"isimage";
         DWORD dwTitleDelLen = (DWORD)wcslen(pszTitleDel), dwTitleFontLen = (DWORD)wcslen(pszTitleFont),
            dwTitleColorLen = (DWORD)wcslen(pszTitleColor), dwIsImageLen = (DWORD)wcslen(pszIsImage);

         if (!wcsncmp(psz, pszImageOpen, dwImageOpenLen)) {
            DWORD dwIndex = _wtoi(psz + dwImageOpenLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            WCHAR szFile[256];
            wcscpy (szFile, pti->ps->Get());
            if (!OpenImageDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE))
               return TRUE;
            pti->ps->Set(szFile);

            // set the control
            WCHAR szTemp[64];
            PCEscControl pControl;
            swprintf (szTemp, L"titletext%d", (int)dwIndex);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), pti->ps->Get());

            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszIsImage, dwIsImageLen)) {
            DWORD dwIndex = _wtoi(psz + dwIsImageLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            pti->fIsImage = p->pControl->AttribGetBOOL(Checked());
            pti->ps->Set (L"");
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszTitleDel, dwTitleDelLen)) {
            DWORD dwIndex = _wtoi(psz + dwTitleDelLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            // delete
            pti->ps->Release();
            prs->m_lTITLEITEM.Remove (dwIndex);
            prs->m_fChanged = TRUE;
            
            
            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszTitleFont, dwTitleFontLen)) {
            DWORD dwIndex = _wtoi(psz + dwTitleFontLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            CHOOSEFONT cf;
            memset (&cf, 0, sizeof(cf));

            cf.lStructSize = sizeof(cf);
            cf.hwndOwner = pPage->m_pWindow->m_hWnd;
            cf.lpLogFont = &pti->lf;
            cf.Flags = /*CF_EFFECTS |*/ CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
            cf.rgbColors = pti->cColor;

            if (ChooseFont (&cf)) {
               // pti->cColor = cf.rgbColors;

               prs->m_fChanged = TRUE;
               // refresh
               BOOL af[2];
               memset (af, 0, sizeof(af));
               pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            }

            return TRUE;
         }
         else if (!wcsncmp(psz, pszTitleColor, dwTitleColorLen)) {
            DWORD dwIndex = _wtoi(psz + dwTitleColorLen);
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(dwIndex);
            if (!pti)
               return TRUE;

            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pti->cColor, NULL, NULL);
            if (cr != pti->cColor) {
               pti->cColor = cr;
               prs->m_fChanged = TRUE;

               BOOL af[2];
               memset (af, 0, sizeof(af));
               pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            }

            return TRUE;
         }
         else if (!_wcsicmp (psz, L"backfiledialog")) {
            if (!OpenImageDialog (pPage->m_pWindow->m_hWnd, prs->m_szBackFile,
               sizeof(prs->m_szBackFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            prs->m_fChanged = TRUE;

            // check the button if it isnt' already
            PCEscControl pControl;
            WCHAR szTemp[64];
            DWORD i;
            prs->m_dwBackMode = 1;
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"backmode%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSetBOOL (Checked(), (i == prs->m_dwBackMode));
            }

            // write file
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);

            // wipe out the render scene
            if (prs->m_pBackRend)
               delete prs->m_pBackRend;
            prs->m_pBackRend = NULL;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"backrenddialog")) {
            // make sure it's created
            if (!prs->m_pBackRend) {
               prs->m_pBackRend = new CRenderScene;
               if (!prs->m_pBackRend)
                  return TRUE;
               prs->m_pBackRend->m_fLoadFromFile = TRUE;
               prs->m_fChanged = TRUE;
            }

            // set radio buttons
            // check the button if it isnt' already
            PCEscControl pControl;
            WCHAR szTemp[64];
            DWORD i;
            prs->m_dwBackMode = 2;
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"backmode%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSetBOOL (Checked(), (i == prs->m_dwBackMode));
            }

            // wipe out file name
            prs->m_szBackFile[0] = 0;
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);

            // edit
            if (prs->m_pBackRend->Edit (dwRenderShard, pPage->m_pWindow->m_hWnd, prs->m_lid,
               prs->m_fReadOnly, prs->m_pProj, TRUE)) {

               prs->m_fChanged = TRUE;

               // refresh
               BOOL af[2];
               memset (af, 0, sizeof(af));
               pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            }

            return TRUE;
         }
         else if (!_wcsicmp (psz, L"backblendlr")) {
            prs->m_fBackBlendLR = p->pControl->AttribGetBOOL(Checked());
            prs->m_fChanged = TRUE;

            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!wcsncmp(psz, pszBackMode, dwBackModeLen)) {
            DWORD dwIndex = _wtoi(psz + dwBackModeLen);

            prs->m_dwBackMode = dwIndex;
            prs->m_fChanged = TRUE;

            switch (dwIndex) {
            case 0:  // color blend
               prs->m_szBackFile[0] = 0;  // so none
               if (prs->m_pBackRend)
                  delete prs->m_pBackRend;
               prs->m_pBackRend = 0;
               break;

            case 1:  // back file
               if (prs->m_pBackRend)
                  delete prs->m_pBackRend;
               prs->m_pBackRend = 0;
               break;

            case 2:  // back rend
               prs->m_szBackFile[0] = 0;  // so none
               if (!prs->m_pBackRend) {
                  prs->m_pBackRend = new CRenderScene;
                  if (prs->m_pBackRend)
                     prs->m_pBackRend->m_fLoadFromFile = TRUE;
               }
               break;
            }

            // set edit control
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);

            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!wcsncmp(psz, pszChangeBackColor, dwChangeBackColorLen)) {
            DWORD dwIndex = _wtoi(psz + dwChangeBackColorLen);
            if (dwIndex >= 2)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"backcolor%d", (int) dwIndex);

            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, prs->m_acBackColor[dwIndex], pPage, szTemp);
            if (cr != prs->m_acBackColor[dwIndex]) {
               prs->m_acBackColor[dwIndex] = cr;
               prs->m_fChanged = TRUE;

               BOOL af[2];
               memset (af, 0, sizeof(af));
               pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            }
            return TRUE;
         }
         else if (HotSpotButtonPress (pPage, p, &prs->m_lPCCircumrealityHotSpot, &prs->m_fChanged))
            return TRUE;
         else if (!_wcsicmp(psz, L"refresh")) {
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"finalquality")) {
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[0] = af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCM_USER+102:  // called to indicate that should redraw
      {
         CProgress Progress;
         BOOL *paf = (BOOL*)pParam;

         Progress.Start (pPage->m_pWindow->m_hWnd, "Drawing...", paf[0] || paf[1]);

         if (prs->m_pImage)
            delete prs->m_pImage;
         prs->m_pImage = prs->Render (dwRenderShard, SCALEPREVIEW, paf[0], paf[1], TRUE, 0, &Progress, NULL, NULL, NULL);
         if (!prs->m_pImage)
            return TRUE;   // unlikely

         if (prs->m_hBmp)
            DeleteObject (prs->m_hBmp);
         HDC hDC = GetDC (pPage->m_pWindow->m_hWnd);
         prs->m_hBmp = prs->m_pImage->ToBitmap (hDC);
         ReleaseDC (pPage->m_pWindow->m_hWnd, hDC);

         PCEscControl pControl = pPage->ControlFind (L"image");
         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64)prs->m_hBmp);
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      return TRUE;

   case ESCN_IMAGEDRAGGED:
      {
         if (prs->m_fReadOnly)
            return TRUE;   // cant change

         PESCNIMAGEDRAGGED p = (PESCNIMAGEDRAGGED)pParam;

         // image width and height
         DWORD dwWidth, dwHeight;
         RenderSceneAspectToPixelsInt (prs->m_iAspect, SCALEPREVIEW, &dwWidth, &dwHeight);
         RenderSceneAspectToPixelsScaleForText (&dwWidth, &dwHeight);

         if (prs->m_dwTab == 0) { // title tiems
            TITLEITEM ti;
            memset (&ti, 0, sizeof(ti));
            ti.cColor = RGB(0xff,0xff,0xff);
            ti.lf.lfHeight = 24;
            ti.lf.lfCharSet = DEFAULT_CHARSET;
            ti.lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
            strcpy (ti.lf.lfFaceName, "Arial");

            // perhaps copy from an existing item?
            if (prs->m_lTITLEITEM.Num())
               ti = *((PTITLEITEM)prs->m_lTITLEITEM.Get(prs->m_lTITLEITEM.Num()-1));

            ti.rPosn = p->rPos;
            RenderSceneHotSpotFromImage (&ti.rPosn, dwWidth, dwHeight);

            ti.ps = new CMIFLVarString;
            if (!ti.ps)
               return TRUE;
            ti.ps->Set (L"New title");

            prs->m_lTITLEITEM.Add (&ti);
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            // refresh
            pPage->Exit (RedoSamePage());
         }
         else if (prs->m_dwTab == 5) {   // hot spots
            HotSpotImageDragged (pPage, p, &prs->m_lPCCircumrealityHotSpot,
                                    dwWidth, dwHeight, &prs->m_fChanged);
         }
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:";
         DWORD dwLen = (DWORD)wcslen(pszTab);

         if (!wcsncmp(p->psz, pszTab, dwLen)) {
            prs->m_dwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = (DWORD)wcslen(pszIfTab), dwEndIfTabLen = (DWORD)wcslen(pszEndIfTab);

         if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"Text",
               L"Aspect",
               L"Background",
               L"Hot spots",
               L"Transition"
            };
            PWSTR apszHelp[] = {
               L"Lets you set the text to display.",
               L"Changes the size of the image.",
               L"Changes the background for the scene.",
               L"Select which areas can be clicked on.",
               L"Controls the transition (fade, pan, and zoom) of the image.",
            };
            DWORD adwID[] = {
               0, // text
               1, //L"Quality",
               9, // background
               5, // L"Hot spots",
               20 // transition
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));

            p->pszSubString = RenderSceneTabs (prs->m_dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Title resource";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IMAGEDRAG")) {
            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<imagedrag name=image clickmode=");
            switch (prs->m_fReadOnly ? 10000 : prs->m_dwTab) {
            case 0:  // text
            case 5:  // hot spots
               // click and drag
               MemCat (&gMemTemp, L"2");
               break;
            default: // others (including RO)... cant click
               MemCat (&gMemTemp, L"0");
               break;
            }
            MemCat (&gMemTemp, L" border=2 width=");
            int iWidth = 90;  // percent
            DWORD dwWidth, dwHeight;
            RenderSceneAspectToPixelsInt (prs->m_iAspect, 1, &dwWidth, &dwHeight);
            RenderSceneAspectToPixelsScaleForText (&dwWidth, &dwHeight);
            if (dwWidth < dwHeight)
               iWidth = iWidth * (int)dwWidth / (int)dwHeight;

            MemCat (&gMemTemp, iWidth);

            MemCat (&gMemTemp, L"% hbitmap=");
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)prs->m_hBmp);
            MemCat (&gMemTemp, szTemp);

            MemCat (&gMemTemp, L"/>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBENABLE")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBREADONLY")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TITLEITEMS")) {
            MemZero (&gMemTemp);

            DWORD i;
            PTITLEITEM pti = (PTITLEITEM)prs->m_lTITLEITEM.Get(0);
            WCHAR szColor[32];
            for (i = 0; i < prs->m_lTITLEITEM.Num(); i++, pti++) {
               ColorToAttrib (szColor, HotSpotColor(i));

               MemCat (&gMemTemp, L"<tr><td bgcolor=");
               MemCat (&gMemTemp, szColor);
               MemCat (&gMemTemp, L">");
               
               MemCat (&gMemTemp, L"Left: <bold><edit width=50% maxchars=32 name=titleposn0");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></bold><br/>");

               MemCat (&gMemTemp, L"Right: <bold><edit width=50% maxchars=32 name=titleposn1");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></bold><br/>");

               MemCat (&gMemTemp, L"Top: <bold><edit width=50% maxchars=32 name=titleposn2");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></bold><br/>");

               MemCat (&gMemTemp, L"Bottom: <bold><edit width=50% maxchars=32 name=titleposn3");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"/></bold><br/>");


               MemCat (&gMemTemp,
                  L"Location is a percentage of the width or height. (Press \"Refresh\" after changing the text or settings to "
                  L"see your changes.)");
               MemCat (&gMemTemp,
		                  L"</td>"
		                  L"<td bgcolor=");
               MemCat (&gMemTemp, szColor);
               MemCat (&gMemTemp, L">");

               // single entry

               // checkbox for image
               MemCat (&gMemTemp, L"<button style=x checkbox=true name=isimage");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L" checked=");
               MemCat (&gMemTemp, pti->fIsImage ? L"true " : L"false ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"><bold>Display an image (instead of text)</bold></button><br/>");

               if (pti->fIsImage) {
                  MemCat (&gMemTemp, L"<bold><edit width=50% maxchars=128 readonly=true name=titletext");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/><button name=imageopen");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L" ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L">Select file...</button></bold>");
               
                  // stretch to fit
                  MemCat (&gMemTemp, L"<br/>");
                  MemCat (&gMemTemp, L"<xComboStretchToFit ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L"name=stretchtofit");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");

                  // transparency
                  MemCat (&gMemTemp, L"<br/>");
                  MemCat (&gMemTemp,
                     L"<bold>Transparency: </bold>"
                     L"<scrollbar orient=horz width=50% min=0 max=100 ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L"name=transparency");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");
               }
               else {
                  // is text
                  MemCat (&gMemTemp, L"<bold><edit multiline=true wordwrap=true width=100% maxchars=10000 ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L"name=titletext");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");

                  MemCat (&gMemTemp, L"</bold>");

                  // left align
                  MemCat (&gMemTemp, L"<br/>");
                  MemCat (&gMemTemp, L"<xComboLRAlign ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L"name=titlelr");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");

                  // top align
                  MemCat (&gMemTemp, L"<xComboTBAlign ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L"name=titletb");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");

                  // change font
                  MemCat (&gMemTemp, L"<br/>");
			         MemCat (&gMemTemp, L"<button style=righttriangle ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L"name=titlefont");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Change font</bold></button>");

                  // change the color
			         MemCat (&gMemTemp, L"<button style=righttriangle ");
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp, L"name=titlecolor");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Change color</bold></button>");
               }

               // delete
               MemCat (&gMemTemp, L"<br/>");
			      MemCat (&gMemTemp, L"<button style=righttriangle ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=titledel");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Remove this</bold></button>");


               MemCat (&gMemTemp, L"</td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (HotSpotSubstitution (pPage, p, &prs->m_lPCCircumrealityHotSpot, prs->m_fReadOnly))
            return TRUE;
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CRenderTitle::Edit - This brings up a dialog box for editing the object.

inputs
   HWND           hWnd - Window to bring dialog up from
   LANGID         lid - Language ID to use as default
   BOOL           fReadOnly - If TRUE then data is read only and cant be changed
   PCMIFLProj     pProj - Project it's it
returns
   BOOL - TRUE if changed, FALSE if didnt
*/
BOOL CRenderTitle::Edit (DWORD dwRenderShard, HWND hWnd, LANGID lid, BOOL fReadOnly, PCMIFLProj pProj)
{
   m_fChanged = FALSE;
   m_hBmp = NULL;
   m_pImage = NULL;
   m_fReadOnly  = fReadOnly;
   m_lid = lid;
   m_iVScroll = 0;
   m_pProj = pProj;
   m_dwTab = 0;
   CEscWindow Window;

   // if any hotspots then fix the language id
   PCCircumrealityHotSpot *pphs = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   if (m_lPCCircumrealityHotSpot.Num())
      m_lid = pphs[0]->m_lid;


   // render an initial pass
   {
      CProgress Progress;
      Progress.Start (hWnd, "Drawing...", TRUE);
      m_pImage = Render (dwRenderShard, SCALEPREVIEW, FALSE, TRUE, TRUE, 0, &Progress, NULL, NULL, NULL);
      if (!m_pImage)
         goto done;
   }
   HDC hDC = GetDC (hWnd);
   m_hBmp = m_pImage->ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);
   if (!m_hBmp)
      goto done;

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   m_dwRenderShardTemp = dwRenderShard;
   psz = Window.PageDialog (ghInstance, IDR_MMLRENDERTITLE, RenderTitlePage, this);
   m_iVScroll = Window.m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;
   if (psz && !_wcsicmp(psz, L"TransitionUI")) {
      DWORD dwWidth, dwHeight;
      RenderSceneAspectToPixelsInt (m_iAspect, SCALEPREVIEW, &dwWidth, &dwHeight);
      RenderSceneAspectToPixelsScaleForText (&dwWidth, &dwHeight);
      BOOL fChanged;
      BOOL fRet = m_Transition.Dialog (&Window, m_hBmp, (fp)dwWidth / (fp)dwHeight,
         (m_iAspect == 10), fReadOnly, &fChanged);
      if (fChanged)
         m_fChanged = TRUE;
      if (fRet)
         goto redo;
      else
         goto done;
   }


done:
   if (m_hBmp)
      DeleteObject (m_hBmp);
   if (m_pImage)
      delete m_pImage;
   return m_fChanged;
}

