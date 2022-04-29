/********************************************************************************
AlgoText.cpp - Code for handling algorithmic texture maps.

begun 3/11/01 by Mike Rozak
Copyright 2001-3 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"


#define BACKCOLORRGB    RGB(0,1,2)


/******************************************************************************
TextureDetailApply - Applies the texture detail scaling to pixel len.

inputs
   fp       fPixelLen - Original pixel len
returns
   fp - Filled with the modified length
*/
fp TextureDetailApply (DWORD dwRenderShard, fp fPixelLen)
{
   int iDetail = TextureDetailGet(dwRenderShard);
   if (iDetail)
      return fPixelLen * pow(0.5, iDetail);
   else
      return fPixelLen;
}


/******************************************************************************
TextureDetailApply - Applies the texture detail scaling width and height.

inputs
   DWORD      dwWidth - Original width
   DWORD      dwHeight - Original height
   DWORD      *pdwWidth - Filled with new width (can be dwWidth)
   DWORD      *pdwHeight - Filled with new height (can be dwHeight)
returns
   none
*/
void TextureDetailApply (DWORD dwRenderShard, DWORD dwWidth, DWORD dwHeight, DWORD *pdwWidth, DWORD *pdwHeight)
{
   int iDetail = TextureDetailGet(dwRenderShard);
   if (iDetail > 0) {
      dwWidth <<= iDetail;
      dwHeight <<= iDetail;
   }
   else if (iDetail < 0) {
      dwWidth >>= (-iDetail);
      dwHeight >>= (-iDetail);
   }
   dwWidth = max(dwWidth, 1);
   dwHeight = max(dwHeight, 1);
   *pdwWidth = dwWidth;
   *pdwHeight = dwHeight;
}


/******************************************************************************
ColorDarkenAtBottom - Darkens the color at the bottom of the image, while
lightening it at the top.

inputs
   PCImage           pImage - That contains the bitmap or whatever
   COLORREF          cTrans - Transparent color
   fp                fScaleAmt - Amount to scale... 0 = none, 1.0 = max amount
returns
   none
*/
void ColorDarkenAtBottom (PCImage pImage, COLORREF cTrans, fp fScaleAmt)
{
   if (!fScaleAmt)
      return;  // done

   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   DWORD x, y, j;
   fp f;
   for (y = 0; y < pImage->Height(); y++) {
      pip = pImage->Pixel (0, y);

      fp fScale = ((1.0 - (fp)y / (fp)pImage->Height()) - 0.5) * fScaleAmt * 2.0 + 1.0;

      for (x = 0; x < pImage->Width(); x++, pip++) {
         // if colors match then ignore
         COLORREF cTemp = UnGamma (&pip->wRed);
         if ( (GetRValue(cTrans) == GetRValue(cTemp)) &&
            (GetGValue(cTrans) == GetGValue(cTemp)) &&
            (GetBValue(cTrans) == GetBValue(cTemp)) )
            continue;

         for (j = 0; j < 3; j++) {
            f = (&pip->wRed)[j];
            f *= fScale;
            f = max(f, 0);
            f = min(f, (fp)0xffff);
            (&pip->wRed)[j] = (WORD)f;
         } // j
      } // x
   } // y

}

/******************************************************************************
ColorToTransparency - Looks through the image. If the color matches the transparency
color then the transparency byte (LOBYTE(dwIDPart) is set to 255, else 0.

Also fills in the specularity regions.

NOTE: In the process, this sets the transparent portions of the image to the average
color of the non-transparent parts. In transparent areas will set specularity intensity to 0.

inputs
   PCImage           pImage - That contains the bitmap or whatever
   COLORREF          cTrans - Transparent color
   DWORD             dwDist - Acceptable distance for r + g + b
   PCMaterial        pMaterial - Used for specularity, and stuff.
returns
   none
*/
void ColorToTransparency (PCImage pImage, COLORREF cTrans, DWORD dwDist, PCMaterial pMat)
{
   DWORD i;
   PIMAGEPIXEL pip;
   COLORREF cTemp;
   DWORD dw;

   BYTE bNotTrans = (BYTE)(pMat->m_wTransparency >> 8);

   __int64 aiNon[3];
   DWORD dwCount;
   aiNon[0] = aiNon[1] = aiNon[2] = 0;
   dwCount = 0;

   pip = pImage->Pixel(0,0);
   for (i = 0; i < pImage->Width() * pImage->Height(); i++, pip++) {
      cTemp = UnGamma (&pip->wRed);
      dw = (DWORD) abs((int)(WORD)GetRValue(cTrans) - (int)(WORD)GetRValue(cTemp)) +
         (DWORD) abs((int)(WORD)GetGValue(cTrans) - (int)(WORD)GetGValue(cTemp)) +
         (DWORD) abs((int)(WORD)GetBValue(cTrans) - (int)(WORD)GetBValue(cTemp));

      pip->dwID = pMat->m_wSpecExponent;

      if (dw <= dwDist) {
         pip->dwIDPart |= 0xff;      // transparent
         // dont bother setting reflection since will want 0
      }
      else {
         pip->dwIDPart = (pip->dwIDPart & ~(0xff)) | bNotTrans;   // not transparent
         pip->dwID |= ((DWORD) pMat->m_wSpecReflect << 16);

         // keep track of non-transprante
         aiNon[0] += (__int64) (DWORD) pip->wRed;
         aiNon[1] += (__int64) (DWORD) pip->wGreen;
         aiNon[2] += (__int64) (DWORD) pip->wBlue;
         dwCount++;
      }
   }

   // BUGFIX - fill the transpraent areas with average color so that leaves
   // at a distance dont go grey
   pip = pImage->Pixel(0,0);
   WORD aw[3];
   for (i = 0; i < 3; i++)
      aw[i] = dwCount ? (WORD)(aiNon[i] / (int)dwCount) : 0;
   for (i = 0; i < pImage->Width() * pImage->Height(); i++, pip++) {
      if (LOBYTE(pip->dwIDPart) == bNotTrans)
         continue;   // opaque

      // else transparent
      pip->wRed = aw[0];
      pip->wGreen = aw[1];
      pip->wBlue = aw[2];
   }

   // BUGFIX - always set transparency angle
   pMat->m_wTransAngle = 0;
}



/****************************************************************************
DrawLineSegment - Draws a line segment on the image.

inputs
   PCImage        pImage - Image to draw on
   PCPoint        pStart - Starting point
   PCPoint        pEnd - Ending point
   fp             fWidthStart - Width at the start
   fp             fWidthEnd - Width at the end
   COLORREF       cColor - Color
   fp             fZDelta - Z value to use. If is -ZINFINITE then ignore
*/
void DrawLineSegment (PCImage pImage, PCPoint pStart, PCPoint pEnd, fp fWidthStart, fp fWidthEnd,
                      COLORREF cColor, fp fZDelta)
{
   // determine direction vector, normalize
   CPoint pDir, pPerp, pVert;
   pDir.Subtract (pEnd, pStart);
   pDir.Normalize ();
   pVert.Zero();
   pVert.p[2] = 1;
   pPerp.CrossProd (&pDir, &pVert);
   pPerp.Normalize();

   // fix width start and end to be half
   fWidthStart /= 2;
   fWidthEnd /= 2;

   // need a polygon with 6 points, so get somewhat rounded end points
   IHLEND aih[6];
   PIHLEND paih[6];
   DWORD i;
   for (i = 0; i < 6; i++)
      paih[i] = &aih[i];
   memset (aih, 0, sizeof(aih));
   Gamma (cColor, aih[0].aColor);
   aih[0].w = 1;
   aih[0].x = pStart->p[0] - pDir.p[0] * fWidthStart;
   aih[0].y = pStart->p[1] - pDir.p[1] * fWidthStart;
   aih[0].z = (fZDelta == -ZINFINITE) ? (pStart->p[2] - pDir.p[2] * fWidthStart) : fZDelta;

   aih[1] = aih[0];
   aih[1].x = pStart->p[0] - pPerp.p[0] * fWidthStart;
   aih[1].y = pStart->p[1] - pPerp.p[1] * fWidthStart;
   aih[1].z = (fZDelta == -ZINFINITE) ? (pStart->p[2] - pPerp.p[2] * fWidthStart) : fZDelta;

   aih[2] = aih[1];
   aih[2].x = pEnd->p[0] - pPerp.p[0] * fWidthEnd;
   aih[2].y = pEnd->p[1] - pPerp.p[1] * fWidthEnd;
   aih[2].z = (fZDelta == -ZINFINITE) ? (pEnd->p[2] - pPerp.p[2] * fWidthEnd) : fZDelta;

   aih[3] = aih[2];
   aih[3].x = pEnd->p[0] + pDir.p[0] * fWidthEnd;
   aih[3].y = pEnd->p[1] + pDir.p[1] * fWidthEnd;
   aih[3].z = (fZDelta == -ZINFINITE) ? (pEnd->p[2] + pDir.p[2] * fWidthEnd) : fZDelta;

   aih[4] = aih[3];
   aih[4].x = pEnd->p[0] + pPerp.p[0] * fWidthEnd;
   aih[4].y = pEnd->p[1] + pPerp.p[1] * fWidthEnd;
   aih[4].z = (fZDelta == -ZINFINITE) ? (pEnd->p[2] + pPerp.p[2] * fWidthEnd) : fZDelta;

   aih[5] = aih[4];
   aih[5].x = pStart->p[0] + pPerp.p[0] * fWidthStart;
   aih[5].y = pStart->p[1] + pPerp.p[1] * fWidthStart;
   aih[5].z = (fZDelta == -ZINFINITE) ? (pStart->p[2] + pPerp.p[2] * fWidthStart) : fZDelta;

   pImage->TGDrawPolygon (6, paih, (fZDelta == -ZINFINITE) ? 0 : 1, TRUE, FALSE, 0x1000, 1000, FALSE);
}

/****************************************************************************
DrawCurvedLineSegment - Draws a curved line segment on the image.

inputs
   PCImage        pImage - Image to draw on
   DWORD          dwNum - Number of points in paPoint
   PCPoint        paPoint - Pointer to an array of points
   fp             fWidthStart - Width at the start
   fp             fWidthMiddle - Width in the middle
   fp             fWidthEnd - Width at the end
   COLORREF       cColorStart - Color at the start
   COLORREF       cColorEnd - Color at the end
   fp             fZDelta - Z to use. If -ZINFINITE then ignore
*/
void DrawCurvedLineSegment (PCImage pImage, DWORD dwNum, PCPoint pPoint,
                            fp fWidthStart, fp fWidthMiddle, fp fWidthEnd,
                            COLORREF cColorStart, COLORREF cColorEnd,
                            fp fZDelta)
{
   WORD awStart[3], awEnd[3], awUse[3];
   Gamma (cColorStart, awStart);
   Gamma (cColorEnd, awEnd);

   DWORD i, j;
   for (i = 0; i+1 < dwNum; i++) {
      // determine the color to use
      fp fWeight = ((fp)i+0.5) / (fp)(dwNum-1);
      for (j = 0; j < 3; j++)
         awUse[j] = (WORD) ((1.0 - fWeight) * (fp)awStart[j] + fWeight * (fp)awEnd[j]);


      fp fWeightStart = (fp)i / (fp)(dwNum-1);
      fp fWeightEnd = (fp)(i+1) / (fp)(dwNum-1);

      // width at start and end
      DrawLineSegment (pImage, pPoint + i, pPoint + (i+1),
         (fWeightStart < 0.5) ?
            ((0.5 - fWeightStart) * fWidthStart + fWeightStart * fWidthMiddle)*2 :
            ((1.0 - fWeightStart) * fWidthMiddle + (fWeightStart - 0.5) * fWidthEnd)*2,
         (fWeightEnd < 0.5) ?
            ((0.5 - fWeightEnd) * fWidthStart + fWeightEnd * fWidthMiddle)*2 :
            ((1.0 - fWeightEnd) * fWidthMiddle + (fWeightEnd-0.5) * fWidthEnd)*2,
         UnGamma (awUse),
         fZDelta);
   } // i
}




/**************************************************************************************
SpecularityToMaterial - Given a specularity exponent and reflection, returns the closest
material ID.

inputs
   WORD     wExponent - Specularity exponent
   WORD     wReflection - Specularity reflection
returns
   DWORD - material ID that's closest
*/
DWORD SpecularityToMaterial (WORD wExponent, WORD wReflection)
{
   CMaterial Mat;

   DWORD i;
   DWORD dwDistClosest, dwDist, dwClosest;
   BOOL fFound;

   fFound = FALSE;
   dwClosest = MATERIAL_FLAT; // some sort of default
   for (i = 1; i < MATERIAL_MAX; i++) {
      if (!Mat.InitFromID (i))
         continue;   // not valid

      // else found
      dwDist = (DWORD)(max(wExponent, Mat.m_wSpecExponent) - min(wExponent, Mat.m_wSpecExponent)) +
         (DWORD)(max(wReflection, Mat.m_wSpecReflect) - min(wReflection, Mat.m_wSpecReflect));
      if (!fFound || (dwDist < dwDistClosest)) {
         fFound = TRUE;
         dwDistClosest = dwDist;
         dwClosest = i;
      }
   }

   return dwClosest;
}


/**************************************************************************************
TEHelperCreateBitmap - Given a TEXTEDITINFO structure filled with a pCreator, this
creates a bitmap (filled into hBit) to be displayed

inputs  
   PTEXTEDITINFO     pt - information
   HWND              hWnd - For bitmap informaiton
returns
   BOOL - TRUE if success
*/
BOOL TEHelperCreateBitmap (PTEXTEDITINFO pt, HWND hWnd)
{
   BOOL fEncourageSphere = !pt->fDrawFlat;

   CImage Image;
   Image.Init (200, 200, 0);
   if (!pt->pCreator->FillImageWithTexture (&Image, fEncourageSphere, pt->fStretchToFit))
      return FALSE;

   HDC hDC;
   hDC = GetDC (hWnd);
   if (pt->hBit)
      DeleteObject (pt->hBit);
   pt->hBit = Image.ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);

   return TRUE;
}


/**************************************************************************************
TEHelperMessageHook - Call this from the beginning of a CTextEffectXXX or
CTextCreateXXX message proc. If returns TRUE, just return TRUE from the mesage
proc.

This does:
   ESCM_SUBSTITUTION - If "HBITMAP" is passed in, it returns a pointer to
      the bitmap.
   ESCM_LINK - Sends messes ESCM_USER+186 to the window. You should trap this and
      get values from the controls t modify the control "image".

      Also, if link is "refresh" then it recreates the bitmap using the latest
      bits and redraws it.
*/
BOOL TEHelperMessageHook (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   static WCHAR sTextureTemp[16];

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            TEHelperCreateBitmap (pt, pPage->m_pWindow->m_hWnd);
            swprintf (sTextureTemp, L"%lx", (__int64) pt->hBit);
            p->pszSubString = sTextureTemp;
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // save everything
         pPage->Message (ESCM_USER+186);

         // maybe update bitmap
         if (p->psz && !_wcsicmp(p->psz, L"refresh")) {
            TEHelperCreateBitmap (pt, pPage->m_pWindow->m_hWnd);

            WCHAR szTemp[32];
            PCEscControl pControl;
            swprintf (szTemp, L"%lx", (__int64) pt->hBit);
            pControl = pPage->ControlFind (L"image");
            if (pControl)
               pControl->AttribSet (L"hbitmap", szTemp);
            EscChime (ESCCHIME_INFORMATION);
            return TRUE;
         }
      }
      break;
   }


   return FALSE;
}


CTextEffectNoise::CTextEffectNoise (void)
{
   m_fTurnOn = FALSE;
   m_fNoiseX = m_fNoiseY = 0;
   m_fZDeltaMin = m_fZDeltaMax = 0;
   m_cMax = m_cMin = 0;
   m_fTransMax = m_fTransMin = 0;
}

static PWSTR gpszNoise = L"Noise";
static PWSTR gpszTurnOn = L"TurnOn";
static PWSTR gpszNoiseX = L"NoiseX";
static PWSTR gpszNoiseY = L"NoiseY";
static PWSTR gpszZDeltaMin = L"ZDeltaMin";
static PWSTR gpszZDeltaMax = L"ZDeltaMax";
static PWSTR gpszMax = L"Max";
static PWSTR gpszMin = L"Min";
static PWSTR gpszTransMax = L"TransMax";
static PWSTR gpszTransMin = L"TransMin";

PCMMLNode2 CTextEffectNoise::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszNoise);
   MMLValueSet (pNode, gpszTurnOn, (int) m_fTurnOn);
   MMLValueSet (pNode, gpszNoiseX, m_fNoiseX);
   MMLValueSet (pNode, gpszNoiseY, m_fNoiseY);
   MMLValueSet (pNode, gpszZDeltaMax, m_fZDeltaMax);
   MMLValueSet (pNode, gpszZDeltaMin, m_fZDeltaMin);
   MMLValueSet (pNode, gpszMax, (int) m_cMax);
   MMLValueSet (pNode, gpszMin, (int) m_cMin);
   MMLValueSet (pNode, gpszTransMax, m_fTransMax);
   MMLValueSet (pNode, gpszTransMin, m_fTransMin);

   return pNode;
}

BOOL CTextEffectNoise::MMLFrom (PCMMLNode2 pNode)
{
   m_fTurnOn = (BOOL)MMLValueGetInt (pNode, gpszTurnOn, (int) FALSE);
   m_fNoiseX = MMLValueGetDouble (pNode, gpszNoiseX, 0);
   m_fNoiseY = MMLValueGetDouble (pNode, gpszNoiseY, 0);
   m_fZDeltaMax = MMLValueGetDouble (pNode, gpszZDeltaMax, 0);
   m_fZDeltaMin = MMLValueGetDouble (pNode, gpszZDeltaMin, 0);
   m_cMax = (COLORREF) MMLValueGetInt (pNode, gpszMax, (int) 0);
   m_cMin = (COLORREF) MMLValueGetInt (pNode, gpszMin, (int) 0);
   m_fTransMax = MMLValueGetDouble (pNode, gpszTransMax, 0);
   m_fTransMin = MMLValueGetDouble (pNode, gpszTransMin, 0);
   return TRUE;
}

void CTextEffectNoise::Apply (PCImage pImage, fp fPixelLen)
{
   if (!m_fTurnOn)
      return;

   pImage->TGNoise ((int)(m_fNoiseX / fPixelLen), (int)(m_fNoiseY / fPixelLen),
      m_fZDeltaMax / fPixelLen, m_fZDeltaMin / fPixelLen,
      m_cMax, m_cMin, m_fTransMax, m_fTransMin);
}

BOOL NoisePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectNoise pv = (PCTextEffectNoise) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTurnOn);

         MeasureToString (pPage, L"noisex", pv->m_fNoiseX, TRUE);
         MeasureToString (pPage, L"noisey", pv->m_fNoiseY, TRUE);
         MeasureToString (pPage, L"zdeltamax", pv->m_fZDeltaMax, TRUE);
         MeasureToString (pPage, L"zdeltamin", pv->m_fZDeltaMin, TRUE);

         pControl = pPage->ControlFind (L"transmax");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTransMax * 100));
         pControl = pPage->ControlFind (L"transmin");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTransMin * 100));

         FillStatusColor (pPage, L"maxcolor", pv->m_cMax);
         FillStatusColor (pPage, L"mincolor", pv->m_cMin);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"maxbutton")) {
            pv->m_cMax = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cMax, pPage, L"maxcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"minbutton")) {
            pv->m_cMin = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cMin, pPage, L"mincolor");
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pv->m_fTurnOn = pControl->AttribGetBOOL (Checked());

         MeasureParseString (pPage, L"noisex", &pv->m_fNoiseX);
         pv->m_fNoiseX = max(.001, pv->m_fNoiseX);
         MeasureParseString (pPage, L"noisey", &pv->m_fNoiseY);
         pv->m_fNoiseY = max(.001, pv->m_fNoiseY);
         MeasureParseString (pPage, L"zdeltamax", &pv->m_fZDeltaMax);
         MeasureParseString (pPage, L"zdeltamin", &pv->m_fZDeltaMin);

         pControl = pPage->ControlFind (L"transmax");
         if (pControl)
            pv->m_fTransMax = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"transmin");
         if (pControl)
            pv->m_fTransMin = (fp)pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Noise";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectNoise::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURENOISE, NoisePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}






/***************************************************************************************
CTextEffectThreads - Threads texture effect
*/
class CTextEffectThreads : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectThreads (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   BOOL        m_fTurnOn;          // if true the turn this on, ele sdisabled
   fp          m_fVarMax;        // Transparency at maximum. 0=opaque,1=transparent
   fp          m_fVarAmt;        // transparency at minimum
};
typedef CTextEffectThreads *PCTextEffectThreads;

CTextEffectThreads::CTextEffectThreads (void)
{
   m_fTurnOn = FALSE;
   m_fVarMax = .2;
   m_fVarAmt = .2;
}

static PWSTR gpszThreads = L"Threads";
static PWSTR gpszVarMax = L"VarMax";
static PWSTR gpszVarAmt = L"VarAmt";

PCMMLNode2 CTextEffectThreads::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszThreads);
   MMLValueSet (pNode, gpszTurnOn, (int) m_fTurnOn);
   MMLValueSet (pNode, gpszVarMax, m_fVarMax);
   MMLValueSet (pNode, gpszVarAmt, m_fVarAmt);

   return pNode;
}

BOOL CTextEffectThreads::MMLFrom (PCMMLNode2 pNode)
{
   m_fTurnOn = (BOOL)MMLValueGetInt (pNode, gpszTurnOn, (int) FALSE);
   m_fVarMax = MMLValueGetDouble (pNode, gpszVarMax, 0);
   m_fVarAmt = MMLValueGetDouble (pNode, gpszVarAmt, 0);
   return TRUE;
}

void CTextEffectThreads::Apply (PCImage pImage, fp fPixelLen)
{
   if (!m_fTurnOn)
      return;

   DWORD dwDim, i;
   for (dwDim = 0; dwDim < 2; dwDim++) {
      // dwDim is the dimension that thread runs along, 0 for x, 1 for y

      DWORD dwThread, dwDist;
      DWORD dwNumThreads, dwNumDist;
      dwNumThreads = dwDim ? pImage->Width() : pImage->Height();
      dwNumDist = dwDim ? pImage->Height() : pImage->Width();

      // loop over each thread
      for (dwThread = 0; dwThread < dwNumThreads; dwThread++) {
         // figure out how much this will vary
         fp fRand[2];
         for (i = 0; i < 2; i++) {
            BOOL fNeg;
            fRand[i] = randf(-1,1);
            if (fRand[i] < 0) {
               fNeg = TRUE;
               fRand[i] *= -1;
            }
            else
               fNeg = FALSE;
            fRand[i] = pow(fRand[i], (fp)(1.0 / max(.01,m_fVarAmt)));
            fRand[i] *= m_fVarMax;
            if (fNeg)
               fRand[i] *= -1;
            fRand[i] += 1.0;
         }

         // figure out where max goes
         fp fOffset;
         fOffset = randf(0,1);

         // loop over dist
         for (dwDist = 0; dwDist < dwNumDist; dwDist++) {
            fp fAmt;
            fAmt = (fp) dwDist / fp(dwNumDist) + fOffset;
            fAmt = myfmod(fAmt, 1);
            fAmt *= 2;
            if (fAmt > 1)
               fAmt = 2.0 - fAmt;   // so get saw tooth

            fAmt = fRand[0] * (1.0 - fAmt) + fRand[1] * fAmt;

            // brighten or darken pixel
            fp fTemp;
            PIMAGEPIXEL pip;
            pip = dwDim ? (pImage->Pixel(dwThread, dwDist)) : (pImage->Pixel (dwDist, dwThread));
            for (i = 0; i < 3; i++) {
               fTemp = (fp) ((&pip->wRed)[i]);
               fTemp *= fAmt;
               if (fTemp > 0xffff)
                  fTemp = 0xffff;
               (&pip->wRed)[i] = (WORD) fTemp;
            }
         }

      } // dwThread
   } // dwDim
}

BOOL ThreadsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectThreads pv = (PCTextEffectThreads) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTurnOn);

         pControl = pPage->ControlFind (L"VarMax");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarMax * 100));
         pControl = pPage->ControlFind (L"VarAmt");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarAmt * 100));

      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pv->m_fTurnOn = pControl->AttribGetBOOL (Checked());

         pControl = pPage->ControlFind (L"VarMax");
         if (pControl)
            pv->m_fVarMax = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"VarAmt");
         if (pControl)
            pv->m_fVarAmt = (fp)pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Threads";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectThreads::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURETHREADS, ThreadsPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}




/***************************************************************************************
CTextEffectMarbling - Marbling texture effect
*/
class CTextEffectMarbling : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectMarbling (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   BOOL        m_fTurnOn;          // if true the turn this on, ele sdisabled
   BOOL        m_fNoiseMax;         // TRUE if noise maxes out
   fp          m_fNoiseSize;        // size of basic noise
   fp          m_fNoiseDetail;      // decay, around .5
   fp          m_fNoiseContrib;     // noise contribution
   COLORREF    m_acColor[8];        // colors
};
typedef CTextEffectMarbling *PCTextEffectMarbling;

CTextEffectMarbling::CTextEffectMarbling (void)
{
   m_fTurnOn = FALSE;
   m_fNoiseMax = FALSE;
   m_fNoiseSize = .5;
   m_fNoiseDetail = .5;
   m_fNoiseContrib = .2;

   DWORD i;
   for (i = 0; i < 8; i ++)
      m_acColor[i] = RGB(i * 32, i * 32, i * 32);
}

static PWSTR gpszMarbling = L"Marbling";
static PWSTR gpszLayerDist = L"LayerDist";
static PWSTR gpszNoiseSize = L"NoiseSize";
static PWSTR gpszNoiseDetail = L"NoiseDetail";
static PWSTR gpszNoiseContrib = L"NoiseContrib";
static PWSTR gpszNoiseMax = L"NoiseMax";

PCMMLNode2 CTextEffectMarbling::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszMarbling);
   MMLValueSet (pNode, gpszTurnOn, (int) m_fTurnOn);
   MMLValueSet (pNode, gpszNoiseMax, (int) m_fNoiseMax);
   MMLValueSet (pNode, gpszNoiseSize, m_fNoiseSize);
   MMLValueSet (pNode, gpszNoiseDetail, m_fNoiseDetail);
   MMLValueSet (pNode, gpszNoiseContrib, m_fNoiseContrib);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      MMLValueSet (pNode, szTemp, (int) m_acColor[i]);
   }

   return pNode;
}

BOOL CTextEffectMarbling::MMLFrom (PCMMLNode2 pNode)
{
   m_fTurnOn = (BOOL)MMLValueGetInt (pNode, gpszTurnOn, (int) FALSE);
   m_fNoiseMax = (BOOL)MMLValueGetInt (pNode, gpszNoiseMax, (int) FALSE);
   m_fNoiseSize = MMLValueGetDouble (pNode, gpszNoiseSize, .1);
   m_fNoiseDetail = MMLValueGetDouble (pNode, gpszNoiseDetail, .1);
   m_fNoiseContrib = MMLValueGetDouble (pNode, gpszNoiseContrib, .1);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 8; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      m_acColor[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, 0);
   }

   return TRUE;
}

void CTextEffectMarbling::Apply (PCImage pImage, fp fPixelLen)
{
   if (!m_fTurnOn)
      return;

   pImage->TGMarbling (max((int)(m_fNoiseSize / fPixelLen),1), m_fNoiseDetail,
      m_fNoiseContrib, 8, m_acColor, m_fNoiseMax);
}

BOOL MarblingPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectMarbling pv = (PCTextEffectMarbling) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTurnOn);
         pControl = pPage->ControlFind (L"noisemax");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fNoiseMax);

         MeasureToString (pPage, L"noisesize", pv->m_fNoiseSize, TRUE);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseDetail * 100));
         pControl = pPage->ControlFind (L"noisecontrib");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseContrib * 100));

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 8; i++) {
            swprintf (szTemp, L"cstatus%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColor[i]);
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszColor = L"cbutton";
         DWORD dwSizeColor = (DWORD)wcslen(pszColor);

         if (!wcsncmp(p->pControl->m_pszName, pszColor, dwSizeColor)) {
            DWORD dwNum = (DWORD)_wtoi(p->pControl->m_pszName + dwSizeColor);
            dwNum = min(7,dwNum);
            WCHAR szTemp[64];
            swprintf (szTemp, L"cstatus%d", (int) dwNum);
            pv->m_acColor[dwNum] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColor[dwNum], pPage, szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pv->m_fTurnOn = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"noisemax");
         if (pControl)
            pv->m_fNoiseMax = pControl->AttribGetBOOL (Checked());

         MeasureParseString (pPage, L"noisesize", &pv->m_fNoiseSize);
         pv->m_fNoiseSize = max(.001, pv->m_fNoiseSize);

         pControl = pPage->ControlFind (L"noisedetail");
         if (pControl)
            pv->m_fNoiseDetail = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"noisecontrib");
         if (pControl)
            pv->m_fNoiseContrib = (fp)pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Marbling";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectMarbling::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREMARBLING, MarblingPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}








/***************************************************************************************
CTextEffectStripes - Stripes texture effect
*/
class CTextEffectStripes : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectStripes (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   BOOL        m_fTurnOn;          // if true the turn this on, ele sdisabled
   BOOL        m_fVertical;        // set to true if stripe is vertical
   fp          m_fSGCenter;         // center of stripe group
   fp          m_fSGWidth;          // width of stripe group
   fp          m_fStripeWidth;      // stipe width
   DWORD       m_dwStripeNum;       // number of stripes in a stipe group
   DWORD       m_dwSGNum;           // number of stripe groups
   DWORD       m_dwInteract;        // how color interacts with underneat, 0=overwrite,1=average,2=add,3=subtract
   COLORREF    m_cColor;        // colors
};
typedef CTextEffectStripes *PCTextEffectStripes;

CTextEffectStripes::CTextEffectStripes (void)
{
   m_fTurnOn = FALSE;
   m_fVertical = TRUE;
   m_fSGCenter = .5;
   m_fSGWidth = .25;
   m_fStripeWidth = 1.0;
   m_dwStripeNum = 1;
   m_dwSGNum = 1;
   m_dwInteract = 0;

   m_cColor = RGB(0xff,0x40,0x40);
}

static PWSTR gpszStripes = L"Stripes";
static PWSTR gpszVertical = L"Vertical";
static PWSTR gpszSGCenter = L"SGCenter";
static PWSTR gpszSGWidth = L"SGWidth";
static PWSTR gpszStripeWidth = L"StripeWidth";
static PWSTR gpszStripeNum = L"StripeNum";
static PWSTR gpszSGNum = L"SGNum";
static PWSTR gpszInteract = L"Interact";
static PWSTR gpszColor = L"Color";

PCMMLNode2 CTextEffectStripes::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszStripes);
   MMLValueSet (pNode, gpszTurnOn, (int) m_fTurnOn);
   MMLValueSet (pNode, gpszVertical, (int) m_fVertical);
   MMLValueSet (pNode, gpszSGCenter, m_fSGCenter);
   MMLValueSet (pNode, gpszSGWidth, m_fSGWidth);
   MMLValueSet (pNode, gpszStripeWidth, m_fStripeWidth);
   MMLValueSet (pNode, gpszStripeNum, (int) m_dwStripeNum);
   MMLValueSet (pNode, gpszSGNum, (int) m_dwSGNum);
   MMLValueSet (pNode, gpszInteract, (int) m_dwInteract);
   MMLValueSet (pNode, gpszColor, (int) m_cColor);

   return pNode;
}

BOOL CTextEffectStripes::MMLFrom (PCMMLNode2 pNode)
{
   m_fTurnOn = (BOOL)MMLValueGetInt (pNode, gpszTurnOn, (int) FALSE);
   m_fVertical = (BOOL)MMLValueGetInt (pNode, gpszVertical, (int) FALSE);
   m_fSGCenter = MMLValueGetDouble (pNode, gpszSGCenter, 0);
   m_fSGWidth = MMLValueGetDouble (pNode, gpszSGWidth, 0);
   m_fStripeWidth = MMLValueGetDouble (pNode, gpszStripeWidth, 0);
   m_dwStripeNum = (DWORD) MMLValueGetInt (pNode, gpszStripeNum, 1);
   m_dwSGNum = (DWORD) MMLValueGetInt (pNode, gpszSGNum, 1);
   m_dwInteract = (DWORD) MMLValueGetInt (pNode, gpszInteract, 1);
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, 0);

   return TRUE;
}

void CTextEffectStripes::Apply (PCImage pImage, fp fPixelLen)
{
   if (!m_fTurnOn)
      return;

   // convert the color to linear
   WORD aw[3];
   pImage->Gamma (m_cColor, aw);

   // dimension - 0 => stipes run along X axis, 1=> y axis
   // dwWidth and dwHeight are image width and height IF vertcal stripes
   DWORD dwDim, dwWidth, dwHeight;
   dwDim = m_fVertical ? 1 : 0;
   dwWidth = dwDim ? pImage->Width() : pImage->Height();
   dwHeight = dwDim ? pImage->Height() : pImage->Width();

   // loop over all stripes
   m_dwSGNum = max(m_dwSGNum,1);
   DWORD dwSG;
   for (dwSG = 0; dwSG < m_dwSGNum; dwSG++) {
      // left and right boundaries
      fp fSGLeft, fSGRight;
      fSGLeft = (fp) dwSG / (fp)m_dwSGNum * (fp)dwWidth;
      fSGRight = (fp) (dwSG+1) / (fp)m_dwSGNum * (fp)dwWidth;

      // apply the stripe
      fp fSGWidth, fSGCenter;
      fSGWidth = (fSGRight - fSGLeft) * m_fSGWidth;
      fSGCenter = (fSGRight - fSGLeft) * m_fSGCenter + fSGLeft;
      fSGLeft = fSGCenter - fSGWidth/2;
      fSGRight = fSGLeft + fSGWidth;

      // loop over sub-stripes
      DWORD dwStripe;
      m_dwStripeNum = max(m_dwStripeNum,1);
      for (dwStripe = 0; dwStripe < m_dwStripeNum; dwStripe++) {
         fp fStripeLeft, fStripeRight;
         fStripeLeft = fSGLeft + fSGWidth * (fp) dwStripe / (fp) m_dwStripeNum;
         fStripeRight = fStripeLeft + fSGWidth / (fp) m_dwStripeNum;

         // size these
         fp fStripeWidth, fStripeCenter;
         fStripeWidth = fStripeRight - fStripeLeft;
         fStripeCenter = (fStripeRight + fStripeLeft) / 2; 
         if (m_dwStripeNum > 1)
            fStripeWidth *= m_fStripeWidth;

         // recalc left and right
         fStripeLeft = fStripeCenter - fStripeWidth/2;
         fStripeRight = fStripeLeft + fStripeWidth;

         // integeer equivalens
         int iLeft, iWidth;
         iLeft = (int) fStripeLeft;
         iWidth = (int) fStripeWidth;
         iWidth = max(1, iWidth);

         // loop
         DWORD xx, yy;
         for (xx = 0; xx < (DWORD)iWidth; xx++) for (yy = 0; yy < dwHeight; yy++) {
            // calculate realy x and y
            DWORD x,y;
            x = (DWORD) myimod(iLeft + (int) xx, dwWidth);
            y = yy;

            // get pixel
            PIMAGEPIXEL pip;
            pip = dwDim ? pImage->Pixel(x,y) : pImage->Pixel(y,x);

            // do whatever
            DWORD i;
            fp fTemp;
            switch (m_dwInteract) {
            default:
            case 0: // overwrire
               memcpy (&pip->wRed, aw, sizeof(aw));
               break;

            case 1:  // average
               for (i = 0; i < 3; i++)
                  (&pip->wRed)[i] = (&pip->wRed)[i] / 2 + aw[i] / 2;
               break;

            case 2:  // add
               for (i = 0; i < 3; i++) {
                  fTemp = (fp) (&pip->wRed)[i] + (fp)aw[i];
                  fTemp = min(0xffff,fTemp);
                  (&pip->wRed)[i] = (WORD)fTemp;
               }
               break;

            case 3:  // subtract
               for (i = 0; i < 3; i++)
                  (&pip->wRed)[i] = HIWORD((DWORD)(&pip->wRed)[i] * (DWORD) aw[i]);
               break;
            }

         }  // over xx and yy

      }  // dwStripe
   }  // dwSG
}

BOOL StripesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectStripes pv = (PCTextEffectStripes) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTurnOn);
         pControl = pPage->ControlFind (L"vertical");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fVertical);

         DoubleToControl (pPage, L"stripenum", pv->m_dwStripeNum);
         DoubleToControl (pPage, L"sgnum", pv->m_dwSGNum);

         pControl = pPage->ControlFind (L"sgwidth");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSGWidth * 100));
         pControl = pPage->ControlFind (L"sgcenter");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSGCenter * 100));
         pControl = pPage->ControlFind (L"stripewidth");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fStripeWidth * 100));

         FillStatusColor (pPage, L"cstatus", pv->m_cColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"cbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"cstatus");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"interact")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            pv->m_dwInteract = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pv->m_fTurnOn = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"vertical");
         if (pControl)
            pv->m_fVertical = pControl->AttribGetBOOL (Checked());

         pv->m_dwStripeNum = (DWORD) DoubleFromControl (pPage, L"stripenum");
         pv->m_dwStripeNum = max(1,pv->m_dwStripeNum);

         pv->m_dwSGNum = (DWORD) DoubleFromControl (pPage, L"sgnum");
         pv->m_dwSGNum = max(1,pv->m_dwSGNum);

         pControl = pPage->ControlFind (L"sgwidth");
         if (pControl)
            pv->m_fSGWidth = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"sgcenter");
         if (pControl)
            pv->m_fSGCenter = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"stripewidth");
         if (pControl)
            pv->m_fStripeWidth = (fp)pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Stripes";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectStripes::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURESTRIPES, StripesPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}



/***************************************************************************************
TextEffectChip - Simulate chipping of tiles
*/
class CTextEffectChip : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectChip (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   BOOL          m_fTurnOn;       // must be TRUE to use chipping
   fp            m_fFilterSize;   // filter size, in meters
   fp            m_fTileHeight;   // tile height, in meters
   CTextEffectNoise m_aNoise[3];     // noise to apply to simulate chipping
   COLORREF      m_cChip;         // color of the material under the chips
   fp            m_fChipTrans;    // transparency from 0..1 to merge cChip in
   WORD          m_wSpecReflection;  // specularity reflection value of chipped area
   WORD          m_wSpecDirection;   // specularity directionality of chipped area
};
typedef CTextEffectChip *PCTextEffectChip;

CTextEffectChip::CTextEffectChip (void)
{
   m_fTurnOn = FALSE;
   m_fFilterSize = 0;
   m_fTileHeight = 0;
   m_cChip = 0;
   m_fChipTrans = 0;
   m_wSpecReflection = m_wSpecDirection = 0;
}

static PWSTR gpszChip = L"Chip";
static PWSTR gpszFilterSize = L"FilterSize";
static PWSTR gpszTileHeight = L"TileHeight";
static PWSTR gpszChipTrans = L"ChipTrans";
static PWSTR gpszSpecReflection = L"SpecReflection";
static PWSTR gpszSpecDirection = L"SpecDirection";

PCMMLNode2 CTextEffectChip::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszChip);

   MMLValueSet (pNode, gpszTurnOn, (int) m_fTurnOn);
   MMLValueSet (pNode, gpszFilterSize, m_fFilterSize);
   MMLValueSet (pNode, gpszTileHeight, m_fTileHeight);
   MMLValueSet (pNode, gpszChip, (int) m_cChip);
   MMLValueSet (pNode, gpszChipTrans, m_fChipTrans);
   MMLValueSet (pNode, gpszSpecReflection, (int) m_wSpecReflection);
   MMLValueSet (pNode, gpszSpecDirection, (int) m_wSpecDirection);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 3; i++) {
      PCMMLNode2 pSub = m_aNoise[i].MMLTo ();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextEffectChip::MMLFrom (PCMMLNode2 pNode)
{
   m_fTurnOn = (BOOL) MMLValueGetInt (pNode, gpszTurnOn, (int) 0);
   m_fFilterSize = MMLValueGetDouble (pNode, gpszFilterSize, 0);
   m_fTileHeight = MMLValueGetDouble (pNode, gpszTileHeight, 0);
   m_cChip = MMLValueGetInt (pNode, gpszChip, (int) 0);
   m_fChipTrans = MMLValueGetDouble (pNode, gpszChipTrans, 0);
   m_wSpecReflection = MMLValueGetInt (pNode, gpszSpecReflection, (int) 0);
   m_wSpecDirection = MMLValueGetInt (pNode, gpszSpecDirection, (int) 0);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 3; i++) {
      DWORD dwFind;
      PCMMLNode2 pSub;
      PWSTR psz;
      swprintf (szTemp, L"Noise%d", (int) i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (pSub)
         m_aNoise[i].MMLFrom (pSub);
   }

   return TRUE;
}

void CTextEffectChip::Apply (PCImage pImage, fp fPixelLen)
{
   if (!m_fTurnOn)
      return;

   fp fTileHeight = m_fTileHeight / fPixelLen;

   // put in chips by creating a duplicate, filtering it and raising it a few pixels,
   // and then adding a lot of noise, and remerging that back in
   PCImage pChip;
   pChip = NULL;
   pChip = pImage->Clone();
   if (pChip) {
      pChip->TGFilterZ ((DWORD)(m_fFilterSize / fPixelLen), fTileHeight);

      DWORD i;
      for (i = 0; i < 3; i++)
         m_aNoise[i].Apply (pChip, fPixelLen);

      pChip->TGMergeForChipping (pImage, m_cChip, m_fChipTrans,
         m_wSpecReflection, m_wSpecDirection);
      //POINT pd;
      //pd.x = pd.y = 0;
      //pChip->Overlay (NULL, pImage, pd, TRUE);
      delete pChip;
   }
}


BOOL ChipPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectChip pv = (PCTextEffectChip) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         ComboBoxSet (pPage, L"material", SpecularityToMaterial (pv->m_wSpecDirection, pv->m_wSpecReflection));

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTurnOn);

         pControl = pPage->ControlFind (L"chiptrans");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int) (pv->m_fChipTrans * 100.0));

         MeasureToString (pPage, L"filtersize", pv->m_fFilterSize, TRUE);
         MeasureToString (pPage, L"tileheight", pv->m_fTileHeight, TRUE);
         FillStatusColor (pPage, L"chipcolor", pv->m_cChip);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"chipbutton")) {
            pv->m_cChip = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cChip, pPage, L"chipcolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            CMaterial Mat;
            Mat.InitFromID (dwVal);
            pv->m_wSpecDirection = Mat.m_wSpecExponent;
            pv->m_wSpecReflection = Mat.m_wSpecReflect;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pv->m_fTurnOn = pControl->AttribGetBOOL (Checked());

         pControl = pPage->ControlFind (L"chiptrans");
         if (pControl)
            pv->m_fChipTrans = (fp)pControl->AttribGetInt (Pos()) / 100.0;

         MeasureParseString (pPage, L"filtersize", &pv->m_fFilterSize);
         pv->m_fFilterSize = max(.001, pv->m_fFilterSize);
         MeasureParseString (pPage, L"tileheight", &pv->m_fTileHeight);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Chipping";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectChip::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURECHIP, ChipPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(2, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}



/***************************************************************************************
CTextEffectDirtPaint - Simulate dirt or paint
*/
class CTextEffectDirtPaint : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectDirtPaint (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   BOOL           m_fTurnOn;          // must be TRUE for this to work
   fp             m_fFilterSize;      // filter size in meters
   COLORREF       m_cColor;           // color of dirt or paint
   fp             m_fTransNone;       // transparency of color if no change height
   fp             m_fTransAtA;        // transparency of color at point A
   fp             m_fAHeight;         // how high is A (in meters)
   WORD           m_wSpecReflection;  // specularity reflection value of chipped area
   WORD           m_wSpecDirection;   // specularity directionality of chipped area
};
typedef CTextEffectDirtPaint *PCTextEffectDirtPaint;

CTextEffectDirtPaint::CTextEffectDirtPaint (void)
{
   m_fTurnOn = FALSE;
   m_fFilterSize = 0;
   m_cColor = 0;
   m_fTransNone = m_fTransAtA = m_fAHeight = 0;
   m_wSpecReflection = m_wSpecDirection = 0;
}

static PWSTR gpszDirtPaint = L"DirtPaint";
//static PWSTR gpszColor = L"Color";
static PWSTR gpszTransNone = L"TransNone";
static PWSTR gpszTransAtA = L"TransAtA";
static PWSTR gpszAHeight = L"AHeight";

PCMMLNode2 CTextEffectDirtPaint::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszDirtPaint);

   MMLValueSet (pNode, gpszTurnOn, (int)m_fTurnOn);
   MMLValueSet (pNode, gpszFilterSize, m_fFilterSize);
   MMLValueSet (pNode, gpszColor, (int)m_cColor);
   MMLValueSet (pNode, gpszTransNone, m_fTransNone);
   MMLValueSet (pNode, gpszTransAtA, m_fTransAtA);
   MMLValueSet (pNode, gpszAHeight, m_fAHeight);
   MMLValueSet (pNode, gpszSpecReflection, (int)m_wSpecReflection);
   MMLValueSet (pNode, gpszSpecDirection, (int)m_wSpecDirection);

   return pNode;
}

BOOL CTextEffectDirtPaint::MMLFrom (PCMMLNode2 pNode)
{
   m_fTurnOn = (int)MMLValueGetInt (pNode, gpszTurnOn, (int)0);
   m_fFilterSize = MMLValueGetDouble (pNode, gpszFilterSize, 0);
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int)0);
   m_fTransNone = MMLValueGetDouble (pNode, gpszTransNone, 0);
   m_fTransAtA = MMLValueGetDouble (pNode, gpszTransAtA, 0);
   m_fAHeight = MMLValueGetDouble (pNode, gpszAHeight, 0);
   m_wSpecReflection = (WORD) MMLValueGetInt (pNode, gpszSpecReflection, (int)0);
   m_wSpecDirection = (WORD) MMLValueGetInt (pNode, gpszSpecDirection, (int)0);
   return TRUE;
}

void CTextEffectDirtPaint::Apply (PCImage pImage, fp fPixelLen)
{
   if (!m_fTurnOn)
      return;

   pImage->TGDirtAndPaint ((DWORD)(m_fFilterSize / fPixelLen),
      m_cColor, m_fTransNone, m_fTransAtA,
      m_fAHeight / fPixelLen, m_wSpecReflection, m_wSpecDirection);
}

BOOL DirtPaintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectDirtPaint pv = (PCTextEffectDirtPaint) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", SpecularityToMaterial (pv->m_wSpecDirection, pv->m_wSpecReflection));

         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTurnOn);

         MeasureToString (pPage, L"filtersize", pv->m_fFilterSize, TRUE);
         MeasureToString (pPage, L"aheight", pv->m_fAHeight, TRUE);

         pControl = pPage->ControlFind (L"transatnone");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTransNone * 100));
         pControl = pPage->ControlFind (L"transata");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTransAtA * 100));

         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            CMaterial Mat;
            Mat.InitFromID (dwVal);
            pv->m_wSpecDirection = Mat.m_wSpecExponent;
            pv->m_wSpecReflection = Mat.m_wSpecReflect;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pv->m_fTurnOn = pControl->AttribGetBOOL (Checked());

         MeasureParseString (pPage, L"filtersize", &pv->m_fFilterSize);
         pv->m_fFilterSize = max(pv->m_fFilterSize, .001);
         MeasureParseString (pPage, L"aheight", &pv->m_fAHeight);
         pv->m_fAHeight = max(pv->m_fAHeight, .001);

         pControl = pPage->ControlFind (L"transatnone");
         if (pControl)
            pv->m_fTransNone = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"transata");
         if (pControl)
            pv->m_fTransAtA = (fp)pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Dirt or paint";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextEffectDirtPaint::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREDIRTPAINT, DirtPaintPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}



/***********************************************************************************
CTextEffectMakeTile - Makes a tile of the specified size and mergest into the image.
0 is assumed to be the grout line.
*/

class CTextEffectMakeTile : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectMakeTile (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen, int iX, int iY);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   fp             m_fWidth;     // width of tile in meters
   fp             m_fHeight;    // height of tile in meters
   fp             m_fThickness; // thickness of tile (vertically) in meters
   fp             m_fDiagCorner; // amount to cut off of corner
   fp             m_fBevel;     // width of bevel on tile, in meters
   DWORD          m_dwBevelMode;   // 0 = square edge, 1=shave off, 2=rounded
   fp             m_fRockLR;    // meters (up/down) that can rock along left/right edge due to poor placement
   fp             m_fRockTB;    // meters (up/down) that can rock along top/bottom edge due to poor placement
   COLORREF       m_cSurfA;     // color of tile varies from cSurfA to cSurfB
   COLORREF       m_cSurfB;     // color of tile varies from cSurfA to cSurfB
   WORD           m_wSpecReflection;  // specularity reflection value of chipped area
   WORD           m_wSpecDirection;   // specularity directionality of chipped area
   CTextEffectNoise m_aNoise[5];  // apply Z-noise and color noise to tile
   CTextEffectChip  m_Chip;       // chipping information
   CTextEffectMarbling m_Marble; // draw marbling
};
typedef CTextEffectMakeTile *PCTextEffectMakeTile;

CTextEffectMakeTile::CTextEffectMakeTile (void)
{
   m_fWidth = m_fHeight = m_fThickness = m_fBevel = 0;
   m_fDiagCorner = 0;
   m_dwBevelMode = 0;
   m_fRockLR = m_fRockTB = 0;
   m_cSurfA = m_cSurfB = 0;
   m_wSpecReflection = m_wSpecDirection = 0;
}

static PWSTR gpszWidth = L"Width";
static PWSTR gpszHeight = L"Height";
static PWSTR gpszThickness = L"Thickness";
static PWSTR gpszBevel = L"Bevel";
static PWSTR gpszBevelMode = L"BevelMode";
static PWSTR gpszRockLR = L"RockLR";
static PWSTR gpszRockTB = L"RockTB";
static PWSTR gpszSurfA = L"SurfA";
static PWSTR gpszSurfB = L"SurfB";
static PWSTR gpszMakeTile = L"MakeTile";
static PWSTR gpszDiagCorner = L"DiagCorner";

PCMMLNode2 CTextEffectMakeTile::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszMakeTile);

   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszThickness, m_fThickness);
   MMLValueSet (pNode, gpszDiagCorner, m_fDiagCorner);
   MMLValueSet (pNode, gpszBevel, m_fBevel);
   MMLValueSet (pNode, gpszBevelMode, (int) m_dwBevelMode);
   MMLValueSet (pNode, gpszRockLR, m_fRockLR);
   MMLValueSet (pNode, gpszRockTB, m_fRockTB);
   MMLValueSet (pNode, gpszSurfA, (int) m_cSurfA);
   MMLValueSet (pNode, gpszSurfB, (int) m_cSurfB);
   MMLValueSet (pNode, gpszSpecReflection, (WORD) m_wSpecReflection);
   MMLValueSet (pNode, gpszSpecDirection, (WORD) m_wSpecDirection);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      PCMMLNode2 pSub = m_aNoise[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int)i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   PCMMLNode2 pSub;
   pSub = m_Chip.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszChip);
      pNode->ContentAdd (pSub);
   }

   pSub = m_Marble.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMarbling);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextEffectMakeTile::MMLFrom (PCMMLNode2 pNode)
{
   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, 0);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, 0);
   m_fThickness = MMLValueGetDouble (pNode, gpszThickness, 0);
   m_fDiagCorner = MMLValueGetDouble (pNode, gpszDiagCorner, 0);
   m_fBevel = MMLValueGetDouble (pNode, gpszBevel, 0);
   m_dwBevelMode = (DWORD) MMLValueGetInt (pNode, gpszBevelMode, (int) 0);
   m_fRockLR = MMLValueGetDouble (pNode, gpszRockLR, 0);
   m_fRockTB = MMLValueGetDouble (pNode, gpszRockTB, 0);
   m_cSurfA = (COLORREF) MMLValueGetInt (pNode, gpszSurfA, (int) 0);
   m_cSurfB = (COLORREF) MMLValueGetInt (pNode, gpszSurfB, (int) 0);
   m_wSpecReflection = (WORD) MMLValueGetInt (pNode, gpszSpecReflection, (WORD) 0);
   m_wSpecDirection = (WORD) MMLValueGetInt (pNode, gpszSpecDirection, (WORD) 0);

   DWORD i;
   WCHAR szTemp[64];
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwFind;
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Noise%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }

   // chip
   dwFind = pNode->ContentFind (gpszChip);
   pSub = NULL;
   if (dwFind != -1) // BUGFIX - Changed to != from ==
      pNode->ContentEnum (dwFind, &psz, &pSub);
   if (pSub)
      m_Chip.MMLFrom (pSub);

   dwFind = pNode->ContentFind (gpszMarbling);
   pSub = NULL;
   if (dwFind != -1)
      pNode->ContentEnum (dwFind, &psz, &pSub);
   if (pSub)
      m_Marble.MMLFrom (pSub);

   return TRUE;
}

//   int         iX, iY - Position of upper, left-hand corner of tile as placed in main
void CTextEffectMakeTile::Apply (PCImage pMain, fp fPixelLen, int iX, int iY)
{

   DWORD dwWidth, dwHeight;
   dwWidth = (DWORD) (m_fWidth / fPixelLen);
   dwHeight = (DWORD) (m_fHeight / fPixelLen);

   // leave space around for grout and working for chips, and so one end of
   // tile doesnt repeat on other end
   DWORD dwBorder;
   dwBorder = max(dwWidth,dwHeight) / 4;

   PCImage pImage;
   pImage = new CImage;
   if (!pImage)
      return;

   DWORD dwX, dwY, dwScale;
   dwX = dwWidth + 2 * dwBorder;
   dwY = dwHeight + 2 * dwBorder;
   dwScale = max(dwX, dwY);
   fp fTileHeight = m_fThickness / fPixelLen;     // pixels deep
   pImage->Init (dwX, dwY, RGB(0,0, 0), -fTileHeight);   // 0 is assumed to be grout level

   // bevelled edges
   IHLEND end[4];
   DWORD dwBevelXY = (DWORD) (m_fBevel / fPixelLen); // number of pixels wide tile is

   // tile rocks to left/right and up/down
   fp fRockLR, fRockUD;
   fRockLR = randf(-m_fRockLR/fPixelLen,m_fRockLR/fPixelLen);
   fRockUD = randf(-m_fRockTB/fPixelLen,m_fRockTB/fPixelLen);

   // color of surface
   COLORREF cSurf;
   fp fAlpha;
   fAlpha = randf(0,1);
   cSurf = RGB(
         (BYTE) (fAlpha * GetRValue(m_cSurfA) + (1.0-fAlpha)*GetRValue(m_cSurfB)),
         (BYTE) (fAlpha * GetGValue(m_cSurfA) + (1.0-fAlpha)*GetGValue(m_cSurfB)),
         (BYTE) (fAlpha * GetBValue(m_cSurfA) + (1.0-fAlpha)*GetBValue(m_cSurfB))
      );

   // draw the tile at at height and specularity
   pImage->TGDrawQuad (
      pImage->ToIHLEND(&end[0], dwBorder, dwBorder, fTileHeight - fRockLR - fRockUD, cSurf),
      pImage->ToIHLEND(&end[1], dwX - dwBorder, dwBorder, fTileHeight + fRockLR - fRockUD, cSurf),
      pImage->ToIHLEND(&end[2], dwX - dwBorder, dwY - dwBorder, fTileHeight + fRockLR + fRockUD, cSurf),
      pImage->ToIHLEND(&end[3], dwBorder, dwY - dwBorder, fTileHeight - fRockLR + fRockUD, cSurf),
      1, TRUE, TRUE, m_wSpecReflection, m_wSpecDirection
      );

   // put the bevelling around
   DWORD i,x,y;
   for (i = 0; i < dwBevelXY; i++) {
      // what's the height at this point
      fp fZ;
      //int iZ;
      if (m_dwBevelMode == 0)
         fZ = 0;
      else if (m_dwBevelMode == 1)
         fZ = fTileHeight - (fp) i / (fp) dwBevelXY * fTileHeight;
      else
         fZ = fTileHeight - sqrt(sin((fp) i / (fp)dwBevelXY * PI / 2)) * fTileHeight;  // rounded corner

      //iZ = (int) (fZ * 0x10000);

      // left and right edges
      for (y = i + dwBorder; y < dwY-i-dwBorder; y++) {
         pImage->Pixel(i+dwBorder, y)->fZ -= fZ; // BUGFIX - WasiZ;
         pImage->Pixel(dwX - i - 1 - dwBorder, y)->fZ -= fZ; // BUGFIX - Was iZ;
      }

      // tob and bottom edges, dont overlay same points
      for (x = i + dwBorder+1; x < dwX-i-dwBorder-1; x++) {
         pImage->Pixel(x, i+dwBorder)->fZ -= fZ; // BUGFIX 0 Was iZ;
         pImage->Pixel(x, dwY - i - 1 - dwBorder)->fZ -= fZ; // BUGFIX - Was iZ;
      }
   }
   
   // apply noise to the coloration
   fp fTileHeightMeters = m_fThickness;

   m_Marble.Apply (pImage, fPixelLen);

   for (i = 0; i < 5; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);

   m_Chip.Apply (pImage, fPixelLen);



   // cut out corners
   DWORD dwDiag;
   dwDiag = max(0,m_fDiagCorner) / fPixelLen;
   if (dwDiag) {
      dwDiag = min(dwDiag, min(dwX,dwY)-dwBorder);
      for (x = dwBorder; x < dwDiag + dwBorder; x++) for (y = dwBorder; (y+x) < dwDiag + 2*dwBorder; y++) {
         for (i = 0; i < 4; i++) {
            PIMAGEPIXEL pip;
            switch (i) {
            default:
            case 0:
               // UL
               pip = pImage->Pixel(x,y);
               break;
            case 1:
               // UR
               pip = pImage->Pixel(dwX - x - 1,y);
               break;
            case 2:
               // LR
               pip = pImage->Pixel(dwX - x - 1,dwY - y - 1);
               break;
            case 3:
               // LL
               pip = pImage->Pixel(x,dwY - y - 1);
               break;
            }

            pip->fZ = -2;// BUGFIX - Was iZ=-0x20000;  // BUGFIX - 2 pixels deep
            pip->dwID = 0;
         }

      }
   }

   // merge into original image
   RECT rSrc;
   POINT pm;
   rSrc.left = dwBorder;
   rSrc.top = dwBorder;
   rSrc.right = rSrc.left + dwWidth;
   rSrc.bottom = rSrc.top + dwHeight;
   pm.x = iX;
   pm.y = iY;
   pImage->TGMerge (&rSrc, pMain, pm);

   delete pImage;
}


BOOL MakeTilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectMakeTile pv = (PCTextEffectMakeTile) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         ComboBoxSet (pPage, L"material", SpecularityToMaterial (pv->m_wSpecDirection, pv->m_wSpecReflection));

         MeasureToString (pPage, L"width", pv->m_fWidth, TRUE);
         MeasureToString (pPage, L"height", pv->m_fHeight, TRUE);
         MeasureToString (pPage, L"thickness", pv->m_fThickness, TRUE);
         MeasureToString (pPage, L"diagcorner", pv->m_fDiagCorner, TRUE);
         MeasureToString (pPage, L"bevel", pv->m_fBevel, TRUE);
         MeasureToString (pPage, L"rocklr", pv->m_fRockLR, TRUE);
         MeasureToString (pPage, L"rocktb", pv->m_fRockTB, TRUE);

         ComboBoxSet (pPage, L"bevelmode", pv->m_dwBevelMode);

         FillStatusColor (pPage, L"surfacolor", pv->m_cSurfA);
         FillStatusColor (pPage, L"surfbcolor", pv->m_cSurfB);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"surfabutton")) {
            pv->m_cSurfA = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfA, pPage, L"surfacolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"surfbbutton")) {
            pv->m_cSurfB = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfB, pPage, L"surfbcolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            CMaterial Mat;
            Mat.InitFromID (dwVal);
            pv->m_wSpecDirection = Mat.m_wSpecExponent;
            pv->m_wSpecReflection = Mat.m_wSpecReflect;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"bevelmode")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            pv->m_dwBevelMode = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"width", &pv->m_fWidth);
         pv->m_fWidth = max(.001,pv->m_fWidth);
         MeasureParseString (pPage, L"height", &pv->m_fHeight);
         pv->m_fHeight = max(.001, pv->m_fHeight);
         MeasureParseString (pPage, L"thickness", &pv->m_fThickness);
         pv->m_fThickness = max(.001, pv->m_fThickness);
         MeasureParseString (pPage, L"diagcorner", &pv->m_fDiagCorner);
         pv->m_fDiagCorner = max(0, pv->m_fDiagCorner);
         MeasureParseString (pPage, L"bevel", &pv->m_fBevel);
         pv->m_fBevel = max(0, pv->m_fBevel);
         MeasureParseString (pPage, L"rocklr", &pv->m_fRockLR);
         MeasureParseString (pPage, L"rocktb", &pv->m_fRockTB);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Tile or brick";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectMakeTile::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREMAKETILE, MakeTilePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(4, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"chip")) {
      if (m_Chip.Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"marble")) {
      if (m_Marble.Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}


/***********************************************************************************
CTextEffectMakeStone - Makes a stone of the specified size and mergest into the image.
0 is assumed to be the grout line.
*/

class CTextEffectMakeStone : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectMakeStone (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen, DWORD dwNum, PTEXTUREPOINT pVertex,
      fp fExtraZ = 0);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   fp             m_fThickness; // thickness of Stone (vertically) in meters
   fp             m_fFlatness;   // how flat this is. 0 = flat, 1 = entirely curved
   fp             m_fRounded;    // how rounded. 0 => sharp edges, .3333 => rounded
   fp             m_fRockLR;    // meters (up/down) that can rock along left/right edge due to poor placement
   fp             m_fRockTB;    // meters (up/down) that can rock along top/bottom edge due to poor placement
   COLORREF       m_cSurfA;     // color of Stone varies from cSurfA to cSurfB
   COLORREF       m_cSurfB;     // color of Stone varies from cSurfA to cSurfB
   WORD           m_wSpecReflection;  // specularity reflection value of chipped area
   WORD           m_wSpecDirection;   // specularity directionality of chipped area
   CTextEffectNoise m_aNoise[5];  // apply Z-noise and color noise to Stone
   CTextEffectMarbling m_Marble;   // apply marbling to the stone
};
typedef CTextEffectMakeStone *PCTextEffectMakeStone;

CTextEffectMakeStone::CTextEffectMakeStone (void)
{
   m_fThickness = 0;
   m_fFlatness = .5;
   m_fRounded = .1;
   m_fRockLR = m_fRockTB = 0;
   m_cSurfA = m_cSurfB = 0;
   m_wSpecReflection = m_wSpecDirection = 0;
}

static PWSTR gpszMakeStone = L"MakeStone";
static PWSTR gpszFlatness = L"Flatness";
static PWSTR gpszRounded = L"Rounded";

PCMMLNode2 CTextEffectMakeStone::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszMakeStone);

   MMLValueSet (pNode, gpszThickness, m_fThickness);
   MMLValueSet (pNode, gpszFlatness, m_fFlatness);
   MMLValueSet (pNode, gpszRounded, m_fRounded);
   MMLValueSet (pNode, gpszRockLR, m_fRockLR);
   MMLValueSet (pNode, gpszRockTB, m_fRockTB);
   MMLValueSet (pNode, gpszSurfA, (int) m_cSurfA);
   MMLValueSet (pNode, gpszSurfB, (int) m_cSurfB);
   MMLValueSet (pNode, gpszSpecReflection, (WORD) m_wSpecReflection);
   MMLValueSet (pNode, gpszSpecDirection, (WORD) m_wSpecDirection);

   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      PCMMLNode2 pSub = m_aNoise[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int)i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }


   PCMMLNode2 pSub;
   pSub = m_Marble.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMarbling);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextEffectMakeStone::MMLFrom (PCMMLNode2 pNode)
{
   m_fThickness = MMLValueGetDouble (pNode, gpszThickness, 0);
   m_fFlatness = MMLValueGetDouble (pNode, gpszFlatness, 0);
   m_fRounded = MMLValueGetDouble (pNode, gpszRounded, 0);
   m_fRockLR = MMLValueGetDouble (pNode, gpszRockLR, 0);
   m_fRockTB = MMLValueGetDouble (pNode, gpszRockTB, 0);
   m_cSurfA = (COLORREF) MMLValueGetInt (pNode, gpszSurfA, (int) 0);
   m_cSurfB = (COLORREF) MMLValueGetInt (pNode, gpszSurfB, (int) 0);
   m_wSpecReflection = (WORD) MMLValueGetInt (pNode, gpszSpecReflection, (WORD) 0);
   m_wSpecDirection = (WORD) MMLValueGetInt (pNode, gpszSpecDirection, (WORD) 0);

   DWORD i;
   WCHAR szTemp[64];
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwFind;
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Noise%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }

   // marbling
   dwFind = pNode->ContentFind (gpszMarbling);
   pSub = NULL;
   if (dwFind != -1)
      pNode->ContentEnum (dwFind, &psz, &pSub);
   if (pSub)
      m_Marble.MMLFrom (pSub);

   return TRUE;
}

//   DWORD dwNum - Number of vertices
//   PTEXTUREPOINT pVertex - .h = x, .v = y locations of vertices
//   fp fExtraZ - Add this much to Z - useful for pebbles, in meters
void CTextEffectMakeStone::Apply (PCImage pMain, fp fPixelLen, DWORD dwNum, PTEXTUREPOINT pVertex,
                                  fp fExtraZ)
{
   // find the min and max
   DWORD i;
   int iMinX, iMaxX, iMinY, iMaxY;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         iMinY = min(iMinY, (int)floor(pVertex[i].v));
         iMaxY = max(iMaxY, (int)ceil(pVertex[i].v));
         iMinX = min(iMinX, (int)floor(pVertex[i].h));
         iMaxX = max(iMaxX, (int)ceil(pVertex[i].h));
      }
      else {
         iMinY = (int)floor(pVertex[i].v);
         iMaxY = (int)ceil(pVertex[i].v);
         iMinX = (int)floor(pVertex[i].h);
         iMaxX = (int)ceil(pVertex[i].h);
      }
   }
   iMaxX++;
   iMaxY++;

   // size of image
   DWORD dwWidth, dwHeight;
   dwWidth = (DWORD) (iMaxX - iMinX);
   dwHeight = (DWORD) (iMaxY - iMinY);

   // leave space around for grout and working for chips, and so one end of
   // Stone doesnt repeat on other end
   DWORD dwBorder;
   dwBorder = max(dwWidth,dwHeight) / 4;

   PCImage pImage;
   pImage = new CImage;
   if (!pImage)
      return;

   DWORD dwX, dwY, dwScale;
   dwX = dwWidth + 2 * dwBorder;
   dwY = dwHeight + 2 * dwBorder;
   dwScale = max(dwX, dwY);
   fp fStoneHeight = m_fThickness / fPixelLen;     // pixels deep
   fExtraZ /= fPixelLen;
   pImage->Init (dwX, dwY, RGB(0,0, 0), -fStoneHeight);   // 0 is assumed to be grout level

   // color of surface
   COLORREF cSurf;
   fp fAlpha;
   fAlpha = randf(0,1);
   cSurf = RGB(
         (BYTE) (fAlpha * GetRValue(m_cSurfA) + (1.0-fAlpha)*GetRValue(m_cSurfB)),
         (BYTE) (fAlpha * GetGValue(m_cSurfA) + (1.0-fAlpha)*GetGValue(m_cSurfB)),
         (BYTE) (fAlpha * GetBValue(m_cSurfA) + (1.0-fAlpha)*GetBValue(m_cSurfB))
      );

   // create the IHLEND for the stone
   CListFixed lIHLEND, lPIHLEND;
   lIHLEND.Init (sizeof(IHLEND));
   lPIHLEND.Init (sizeof(PIHLEND));
   IHLEND hl;
   memset (&hl, 0, sizeof(hl));
   lIHLEND.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      pImage->ToIHLEND(&hl, pVertex[i].h - (fp)iMinX + (fp) dwBorder,
         pVertex[i].v - (fp) iMinY + (fp)dwBorder, 0, cSurf);
      lIHLEND.Add (&hl);
   }

   PIHLEND ph;
   lPIHLEND.Required (lIHLEND.Num());
   for (i = 0; i < lIHLEND.Num(); i++) {
      ph = (PIHLEND) lIHLEND.Get(i);
      lPIHLEND.Add (&ph);
   }

   // draw it
   DWORD dwCurvePix;
   dwCurvePix = (DWORD)(min(dwWidth, dwHeight) / 2 * m_fFlatness);
   pImage->TGDrawPolygonCurvedExtra (lPIHLEND.Num(), (PIHLEND*) lPIHLEND.Get(0),
      m_wSpecReflection, m_wSpecDirection, dwCurvePix, fExtraZ, fStoneHeight + fExtraZ, m_fRounded);

   // Stone rocks to left/right and up/down
   fp fRockLR, fRockUD;
   fRockLR = randf(-m_fRockLR/fPixelLen,m_fRockLR/fPixelLen);
   fRockUD = randf(-m_fRockTB/fPixelLen,m_fRockTB/fPixelLen);

   PIMAGEPIXEL pip;
   DWORD x,y;
   for (y = 0; y < dwHeight; y++) for (x = 0; x < dwWidth; x++) {
      fp fAmt = fRockLR * ((fp)x / (fp) dwWidth * 2.0 - 1.0) +
         fRockUD * ((fp)y / (fp) dwHeight * 2.0 - 1.0);
      //int iAmt = (int) (fAmt * (fp)0x10000);
      pip = pImage->Pixel (dwBorder + x, dwBorder + y);
      if (pip->fZ > 0)  // so dont rock nothingness out
         pip->fZ += fAmt; // BUGFIX - Was iAmt;
   }


   m_Marble.Apply (pImage, fPixelLen);

   // apply noise to the coloration
   for (i = 0; i < 5; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);



   // merge into original image
   RECT rSrc;
   POINT pm;
   rSrc.left = dwBorder;
   rSrc.top = dwBorder;
   rSrc.right = rSrc.left + dwWidth;
   rSrc.bottom = rSrc.top + dwHeight;
   pm.x = iMinX;
   pm.y = iMinY;
   pImage->TGMerge (&rSrc, pMain, pm);

   delete pImage;
}


BOOL MakeStonePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectMakeStone pv = (PCTextEffectMakeStone) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         ComboBoxSet (pPage, L"material", SpecularityToMaterial (pv->m_wSpecDirection, pv->m_wSpecReflection));

         MeasureToString (pPage, L"thickness", pv->m_fThickness, TRUE);
         MeasureToString (pPage, L"rocklr", pv->m_fRockLR, TRUE);
         MeasureToString (pPage, L"rocktb", pv->m_fRockTB, TRUE);

         FillStatusColor (pPage, L"surfacolor", pv->m_cSurfA);
         FillStatusColor (pPage, L"surfbcolor", pv->m_cSurfB);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"flatness");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fFlatness * 100));
         pControl = pPage->ControlFind (L"rounded");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fRounded * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"surfabutton")) {
            pv->m_cSurfA = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfA, pPage, L"surfacolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"surfbbutton")) {
            pv->m_cSurfB = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfB, pPage, L"surfbcolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            CMaterial Mat;
            Mat.InitFromID (dwVal);
            pv->m_wSpecDirection = Mat.m_wSpecExponent;
            pv->m_wSpecReflection = Mat.m_wSpecReflect;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"thickness", &pv->m_fThickness);
         pv->m_fThickness = max(.001, pv->m_fThickness);
         MeasureParseString (pPage, L"rocklr", &pv->m_fRockLR);
         MeasureParseString (pPage, L"rocktb", &pv->m_fRockTB);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"flatness");
         if (pControl)
            pv->m_fFlatness = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"rounded");
         if (pControl)
            pv->m_fRounded = (fp)pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Stone";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectMakeStone::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREMAKESTONE, MakeStonePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(4, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"marble")) {
      if (m_Marble.Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

/***********************************************************************************
CTextEffectGrout - Draws grout
*/
class CTextEffectGrout : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectGrout (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pImage, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   COLORREF       m_cColor;     // color of the grout
   CTextEffectNoise m_aNoise[2];  // noise to apply to grout
};
typedef CTextEffectGrout *PCTextEffectGrout;

CTextEffectGrout::CTextEffectGrout (void)
{
   m_cColor = 0;
}

static PWSTR gpszGrout = L"Grout";

PCMMLNode2 CTextEffectGrout::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszGrout);

   MMLValueSet (pNode, gpszColor, (int)m_cColor);

   DWORD i;
   WCHAR szTemp[64];
   PCMMLNode2 pSub;
   for (i = 0; i < 2; i++) {
      pSub = m_aNoise[i].MMLTo ();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextEffectGrout::MMLFrom (PCMMLNode2 pNode)
{
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int)0);

   DWORD i, dwFind;
   WCHAR szTemp[64];
   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Noise%d", (int) i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }
   return TRUE;
}

void CTextEffectGrout::Apply (PCImage pImage, fp fPixelLen)
{
   DWORD dwX, dwY;
   dwX = pImage->Width();
   dwY = pImage->Height();
   // draw the tile at at height and specularity
   COLORREF cGrout;
   cGrout = m_cColor;
   IHLEND end[4];
   pImage->TGDrawQuad (
      pImage->ToIHLEND(&end[0], 0, 0, 0, cGrout),
      pImage->ToIHLEND(&end[1], dwX , 0, 0, cGrout),
      pImage->ToIHLEND(&end[2], dwX, dwY, 0, cGrout),
      pImage->ToIHLEND(&end[3], 0, dwY, 0, cGrout),
      1, TRUE, TRUE, 0x800, 100
      );

   // grout

   DWORD i;
   for (i = 0; i < 2; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);
}


BOOL GroutPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectGrout pv = (PCTextEffectGrout) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Grout";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextEffectGrout::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREGROUT, GroutPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(1, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}




/***********************************************************************************
CTextEffectGenerateTree - Draws grout
*/
class CTextEffectGenerateTree : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectGenerateTree (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   PCImage Apply (DWORD dwX, DWORD dwY, fp fPixelLen);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   fp          m_fRingThickness;   // thickness of a tree ring, in meters
   fp          m_fRadius;    // radius in meters
   fp          m_fLogCenterXOffsetMin;     // offset cut (left edge) from the center of the log, less radius tree
   fp          m_fLogCenterXOffsetMax;     // offset cut (left edge) from the center of the log, less radius tree
   CTextEffectNoise m_aNoise[2];              // put bumps in the tree trunk
   fp          m_fNumKnotsPerMeter;      // average number of knots per square meter
   fp          m_fKnotWidthMin;          // if knot, minimium width in meters
   fp          m_fKnotWidthMax;          // if knot, maximum width in meters
};
typedef CTextEffectGenerateTree *PCTextEffectGenerateTree;

CTextEffectGenerateTree::CTextEffectGenerateTree (void)
{
   m_fRingThickness = m_fRadius = 0;
   m_fLogCenterXOffsetMin = m_fLogCenterXOffsetMax = 0;
   m_fNumKnotsPerMeter = m_fKnotWidthMin = m_fKnotWidthMax = 0;
}

static PWSTR gpszGenerateTree = L"GenerateTree";
static PWSTR gpszRingThickness = L"RingThickness";
static PWSTR gpszRadius = L"Radius";
static PWSTR gpszLogCenterXOffsetMin = L"LogCenterXOffsetMin";
static PWSTR gpszLogCenterXOffsetMax = L"LogCenterXOffsetMax";
static PWSTR gpszNumKnotsPerMeter = L"NumKnotsPerMeter";
static PWSTR gpszKnotWidthMin = L"KnotWidthMin";
static PWSTR gpszKnotWidthMax = L"KnotWidthMax";

PCMMLNode2 CTextEffectGenerateTree::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszGenerateTree);

   MMLValueSet (pNode, gpszRingThickness, m_fRingThickness);
   MMLValueSet (pNode, gpszRadius, m_fRadius);
   MMLValueSet (pNode, gpszLogCenterXOffsetMin, m_fLogCenterXOffsetMin);
   MMLValueSet (pNode, gpszLogCenterXOffsetMax, m_fLogCenterXOffsetMax);
   MMLValueSet (pNode, gpszNumKnotsPerMeter, m_fNumKnotsPerMeter);
   MMLValueSet (pNode, gpszKnotWidthMin, m_fKnotWidthMin);
   MMLValueSet (pNode, gpszKnotWidthMax, m_fKnotWidthMax);

   DWORD i;
   WCHAR szTemp[64];
   PCMMLNode2 pSub;
   for (i = 0; i < 2; i++) {
      pSub = m_aNoise[i].MMLTo ();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextEffectGenerateTree::MMLFrom (PCMMLNode2 pNode)
{
   m_fRingThickness = MMLValueGetDouble (pNode, gpszRingThickness, 0);
   m_fRadius = MMLValueGetDouble (pNode, gpszRadius, 0);
   m_fLogCenterXOffsetMin = MMLValueGetDouble (pNode, gpszLogCenterXOffsetMin, 0);
   m_fLogCenterXOffsetMax = MMLValueGetDouble (pNode, gpszLogCenterXOffsetMax, 0);
   m_fNumKnotsPerMeter = MMLValueGetDouble (pNode, gpszNumKnotsPerMeter, 0);
   m_fKnotWidthMin = MMLValueGetDouble (pNode, gpszKnotWidthMin, 0);
   m_fKnotWidthMax = MMLValueGetDouble (pNode, gpszKnotWidthMax, 0);

   DWORD i, dwFind;
   WCHAR szTemp[64];
   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Noise%d", (int) i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }
   return TRUE;
}

PCImage CTextEffectGenerateTree::Apply (DWORD dwX, DWORD dwY, fp fPixelLen)
{
   PCImage pImage;
   pImage = new CImage;
   if (!pImage)
      return NULL;
   pImage->Init (dwX, dwY, 0);

   // make a tree
   fp   fRadius = m_fRadius / fPixelLen; // radius in the tree in pixels
   fp   fLogCenterXOffset = randf(m_fLogCenterXOffsetMin,m_fLogCenterXOffsetMax) / fPixelLen;  // offset cut (left edge) from the center of the log, in pixels
   DWORD x, y;
   PIMAGEPIXEL pip;
   for (x = 0, pip = pImage->Pixel(0,0); x < dwX; x++, pip++) {
      fp fZ;
      fZ = fRadius * fRadius - (x - fLogCenterXOffset) * (x - fLogCenterXOffset);
      if (fZ < 0)
         fZ *= -1;
      fZ = sqrt (fZ);

      pip->fZ = fZ; // BUGFIX - Was (int) (fZ * 0x10000);

      // these don't really matter but put them in so can test what log looks like
      pip->dwID = (0x8000 << 16) | 1000;
      pip->wRed = pip->wGreen = pip->wBlue = 0x4000;
   }

   // rest of the log. Have it slope up or down a bit
   fp   fInc, fDelta;
   fInc = 0;
   fDelta = randf(-1, 1) / 100;

   for (y = 0; y < dwY; y++) {
      for (x = 0; x < dwX; x++) {
         PIMAGEPIXEL pip = pImage->Pixel(x,y);
         *pip = *(pImage->Pixel(x,0));
         pip->fZ += fInc; // BUGFIX - Was (int) (fInc * 0x10000);
      }
      fInc += fDelta;
   }

   DWORD i;
   for (i = 0; i < 2; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);

   // put in knots
   fp   fNumKnotsPerPixel = m_fNumKnotsPerMeter * fPixelLen * fPixelLen;
   fNumKnotsPerPixel *= dwX * dwY;
   DWORD dwNumKnots;
   if (fNumKnotsPerPixel > .5)
      dwNumKnots = (DWORD) randf(0, 2 * fNumKnotsPerPixel);
   else
      dwNumKnots = (randf(0,1) < fNumKnotsPerPixel) ? 1 : 0;

   for (i = 0; i < dwNumKnots; i++) {
      // random location
      int iKnotX, iKnotY;
      iKnotX = (int) (randf(0,dwX));
      iKnotY = (int) (randf(0,dwY));

      // size
      int iKnotWidth, iKnotHeight;
      iKnotWidth = (int) randf (m_fKnotWidthMin / fPixelLen, m_fKnotWidthMax / fPixelLen);
      iKnotHeight = (int) (randf (.8, 1.5) * iKnotWidth);

      int iX, iY;
      fp   fLWidth, fLHeight;
      fLHeight = iKnotHeight * iKnotHeight;
      fLWidth = iKnotWidth * iKnotWidth;
      for (iY = -iKnotHeight; iY <= iKnotHeight; iY++)
         for (iX = -iKnotWidth; iX <= iKnotWidth; iX++) {
            fp f;
            f = iY * iY / fLHeight + iX * iX / fLWidth;
            if (f > 1)
               continue;   // beyond the edge
            f = sqrt(f);
            f = cos(f * PI / 2);
            if (f < 0)
               continue;   // beyond edge
            f = pow(f,8);  // cos to the eighth

            // add it
            PIMAGEPIXEL pip;
            pip = pImage->Pixel(myimod(iKnotX + iX,dwX),
               myimod(iKnotY+iY, dwY));
            pip->fZ += f * (fp) iKnotHeight * 5;   // BUGFIX - Was (int) (f * iKnotHeight * 5 * 0x10000);
         }
   }

   return pImage;
}




BOOL GenerateTreePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectGenerateTree pv = (PCTextEffectGenerateTree) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         MeasureToString (pPage, L"ringthickness", pv->m_fRingThickness, TRUE);
         DoubleToControl (pPage, L"numknotspermeter", pv->m_fNumKnotsPerMeter);
         MeasureToString (pPage, L"knotwidthmin", pv->m_fKnotWidthMin, TRUE);
         MeasureToString (pPage, L"knotwidthmax", pv->m_fKnotWidthMax, TRUE);
         MeasureToString (pPage, L"radius", pv->m_fRadius, TRUE);
         MeasureToString (pPage, L"logcenterxoffsetmin", pv->m_fLogCenterXOffsetMin, TRUE);
         MeasureToString (pPage, L"logcenterxoffsetmax", pv->m_fLogCenterXOffsetMax, TRUE);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"ringthickness", &pv->m_fRingThickness);
         pv->m_fRingThickness = max(.001, pv->m_fRingThickness);
         pv->m_fNumKnotsPerMeter = DoubleFromControl (pPage, L"numknotspermeter");
         pv->m_fNumKnotsPerMeter = max(0,pv->m_fNumKnotsPerMeter);
         MeasureParseString (pPage, L"knotwidthmin", &pv->m_fKnotWidthMin);
         pv->m_fKnotWidthMin = max (.001, pv->m_fKnotWidthMin);
         MeasureParseString (pPage, L"knotwidthmax", &pv->m_fKnotWidthMax);
         pv->m_fKnotWidthMax = max (pv->m_fKnotWidthMin, pv->m_fKnotWidthMax);
         MeasureParseString (pPage, L"radius", &pv->m_fRadius);
         pv->m_fRadius = max(.1, pv->m_fRadius);
         MeasureParseString (pPage, L"logcenterxoffsetmin", &pv->m_fLogCenterXOffsetMin);
         MeasureParseString (pPage, L"logcenterxoffsetmax", &pv->m_fLogCenterXOffsetMax);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Tree";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextEffectGenerateTree::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREGENERATETREE, GenerateTreePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"noise0")) {
      if (m_aNoise[0].Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"noise1")) {
      if (m_aNoise[1].Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}



/***********************************************************************************
CTextEffectGeneratePlank - Draws grout
*/
class CTextEffectGeneratePlank : public CTextEffectSocket {
public:
   ESCNEWDELETE;

   CTextEffectGeneratePlank (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCMMLNode2 MMLTo (void);
   void Apply (PCImage pMain, fp fPixelLen, DWORD dwX, DWORD dwY, int iX, int iY);
   BOOL Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator);

   fp             m_fRingBump;        // number of meters up to create a bump if there's a ring transition
   fp             m_fBevelHeight;     // height (vertical) of bevel in meters
   fp             m_fBevelWidth;      // width of bevel along sides, in meters
   fp             m_fBoardBend;       // amount board can bend up/down over length of board, in m
   COLORREF       m_acColors[5];      // colors of ring at 5 points, progressing from lighter to darker
   COLORREF       m_cTransitionColor; // use this color for a year transition in the ring
   fp             m_fBrightMin;       // apply darker/lighter across length for variation. 1.0 = normal brightness
   fp             m_fBrightMax;       // 1.0 = normal bright
   WORD           m_wSpecDirection;   // specularity info
   WORD           m_wSpecReflection;  // specularity info
   CTextEffectNoise m_Noise;            // noise (discoloring) to apply
   CTextEffectGenerateTree m_Tree;           // tree information
};
typedef CTextEffectGeneratePlank *PCTextEffectGeneratePlank;

CTextEffectGeneratePlank::CTextEffectGeneratePlank (void)
{
   m_fRingBump = m_fBevelHeight = m_fBevelWidth = m_fBoardBend = 0;
   memset (m_acColors, 0, sizeof(m_acColors));
   m_cTransitionColor = 0;
   m_fBrightMin = m_fBrightMax = 0;
   m_wSpecDirection = m_wSpecReflection = 0;
}

static PWSTR gpszGeneratePlank = L"GeneratePlank";
static PWSTR gpszRingBump = L"RingBump";
static PWSTR gpszBevelHeight = L"BevelHeight";
static PWSTR gpszBevelWidth = L"BevelWidth";
static PWSTR gpszBoardBend = L"BoardBend";
static PWSTR gpszTransitionColor = L"TransitionColor";
static PWSTR gpszBrightMin = L"BrightMin";
static PWSTR gpszBrightMax = L"BrightMax";

PCMMLNode2 CTextEffectGeneratePlank::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszGeneratePlank);

   MMLValueSet (pNode, gpszRingBump, m_fRingBump);
   MMLValueSet (pNode, gpszBevelHeight, m_fBevelHeight);
   MMLValueSet (pNode, gpszBevelWidth, m_fBevelWidth);
   MMLValueSet (pNode, gpszBoardBend, m_fBoardBend);
   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Colors%d", (int) i);
      MMLValueSet (pNode, szTemp, (int) m_acColors[i]);
   }
   MMLValueSet (pNode, gpszTransitionColor, (int)m_cTransitionColor);
   MMLValueSet (pNode, gpszBrightMin, m_fBrightMin);
   MMLValueSet (pNode, gpszBrightMax, m_fBrightMax);
   MMLValueSet (pNode, gpszSpecDirection, (int)m_wSpecDirection);
   MMLValueSet (pNode, gpszSpecReflection, (int)m_wSpecReflection);

   PCMMLNode2 pSub;
   pSub = m_Noise.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszNoise);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Tree.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGenerateTree);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextEffectGeneratePlank::MMLFrom (PCMMLNode2 pNode)
{
   m_fRingBump = MMLValueGetDouble (pNode, gpszRingBump, 0);
   m_fBevelHeight = MMLValueGetDouble (pNode, gpszBevelHeight, 0);
   m_fBevelWidth = MMLValueGetDouble (pNode, gpszBevelWidth, 0);
   m_fBoardBend = MMLValueGetDouble (pNode, gpszBoardBend, 0);
   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"Colors%d", (int) i);
      m_acColors[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int) 0);
   }
   m_cTransitionColor = (COLORREF) MMLValueGetInt (pNode, gpszTransitionColor, (int)0);
   m_fBrightMin = MMLValueGetDouble (pNode, gpszBrightMin, 0);
   m_fBrightMax = MMLValueGetDouble (pNode, gpszBrightMax, 0);
   m_wSpecDirection = (WORD) MMLValueGetInt (pNode, gpszSpecDirection, (int)0);
   m_wSpecReflection = (WORD) MMLValueGetInt (pNode, gpszSpecReflection, (int)0);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD dwFind;
   dwFind = pNode->ContentFind (gpszNoise);
   pSub = NULL;
   if (dwFind != -1)
      pNode->ContentEnum (dwFind, &psz, &pSub);
   if (pSub)
      m_Noise.MMLFrom (pSub);

   dwFind = pNode->ContentFind (gpszGenerateTree);
   pSub = NULL;
   if (dwFind != -1)
      pNode->ContentEnum (dwFind, &psz, &pSub);
   if (pSub)
      m_Tree.MMLFrom (pSub);

   return TRUE;
}

// Apply - Generates a wood plank and pastes it into the image
//
//inputs
//   DWORD    dwX, dwY - Width and height of pland
//   PCImage  pMain - Where to write it over
//   int      iX, iY - upper left-corner to write to
//   fp   fPixelLen - meters per pixel
void CTextEffectGeneratePlank::Apply (PCImage pMain, fp fPixelLen, DWORD dwX, DWORD dwY, int iX, int iY)
{
   PCImage pImage = new CImage;
   if (!pImage)
      return;
   pImage->Init (dwX, dwY, 0);
   fp fBevelHeight = m_fBevelHeight / fPixelLen;

   fp   fRingHeight = m_Tree.m_fRingThickness / fPixelLen;   // size of the ring in pixels
   
   // to colors
   WORD awColors[5][3];
   WORD awTransition[3];
   DWORD i, j;
   pImage->Gamma (m_cTransitionColor, awTransition);
   for (i = 0; i < 5; i++)
      pImage->Gamma (m_acColors[i], &awColors[i][0]);

   // average the color for antialiasing purposes
   WORD awAverage[3];
   for (i = 0; i < 3; i++) {
      awAverage[i] = 0;
      for (j = 0; j < 5; j++)
         awAverage[i] += awColors[j][i] / 5;
   }

   // size
   DWORD    dwScale;
   dwScale = max(dwX, dwY);


   // create a tree of the same size
   PCImage pTree;
   pTree = m_Tree.Apply (dwX, dwY, fPixelLen);


   // loop through the tree and use the to calcualte where in the ring a pixel is
   DWORD x,y;
   for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++) {
      PIMAGEPIXEL pipTree, pipDraw;

      pipTree = pTree->Pixel(x,y);
      pipDraw = pImage->Pixel(x,y);

      // find the minimum and maximum positions between here and the neighboring
      // pixels so we can antialias
      fp fMin, fMax;
      // BUGFIX - Converted all to float
      fMin = fMax = pipTree->fZ;
      pipTree = pTree->Pixel(myimod(x+1,dwX),y);
      fMin = min(fMin, pipTree->fZ);
      fMax = max(fMax, pipTree->fZ);
      pipTree = pTree->Pixel(myimod(x+1,dwX),myimod(y+1,dwY));
      fMin = min(fMin, pipTree->fZ);
      fMax = max(fMax, pipTree->fZ);
      pipTree = pTree->Pixel(x,myimod(y+1,dwY));
      fMin = min(fMin, pipTree->fZ);
      fMax = max(fMax, pipTree->fZ);

      // convert this to ring positions
      fp fRingMin, fRingMax, fRingDelta;
      fRingMin = fMin / fRingHeight;
      fRingMax = fMax / fRingHeight;
      fRingDelta = fRingMax - fRingMin;   // know how many rings crossed

      // do some clever tricks to average
      fp fTransitions;
      BOOL fCrossedRing;
      fRingDelta = floor(fRingMax) - ceil(fRingMin) + 1;
      fCrossedRing = (fRingDelta >= 1);
      fTransitions = (DWORD) max(fRingDelta,1) - 1;  // to use later
      fRingMin = myfmod(fRingMin, 1);
      fRingMax = myfmod(fRingMax, 1);
      if (fRingMax < fRingMin) {
         fRingDelta--;
         fRingMax += 1;
      }

#ifdef _DEBUG
      static fp sfTrans = 0;
      if (fTransitions > sfTrans) {
         sfTrans = fTransitions;
         char szTemp[64];
         sprintf (szTemp, "Max tree ring transitions=%g\r\n",(double) sfTrans);
         OutputDebugString (szTemp);
      }
#endif
      
      // square the number of transitions to ensure it takes over
      fTransitions *= fTransitions;

      // at this point fRingDelta is the number of times to include
      // the average, and all we need to do is average the colors from
      // min to max
      fp afColor[3], fRing;
      afColor[0] = afColor[1] = afColor[2] = 0;
      for (i = 0; i < 3; i++) {
         fRing = fRingMin + ((fp) i / (3-1)) * (fRingMax - fRingMin);
         fRing = myfmod(fRing, 1) * 5;
         DWORD dwLow = (DWORD) (fRing);
         DWORD dwHigh = (dwLow+1) % 5;
         fRing -= dwLow;

         for (j = 0; j < 3; j++)
            afColor[j] += ((1.0 - fRing) * awColors[dwLow][j] + fRing * awColors[dwHigh][j]);
      }
      fp fDistAverage, fTotal;
      fDistAverage = fRingMax - fRingMin;
      fTotal = fRingDelta + fDistAverage + fTransitions;
      for (j = 0; j < 3; j++) {
         afColor[j] /= 3;

         if (fTotal > fDistAverage) {
            afColor[j] = afColor[j] *  // part of ring covered
               fDistAverage + fRingDelta * awAverage[j] +   // average of rings
               fTransitions * awTransition[j];  // extra color for a transition
            afColor[j] /= fTotal;
         }

         afColor[j] = min(0xffff, afColor[j]);
         afColor[j] = max(0, afColor[j]);
         *(&pipDraw->wRed + j) = (WORD) afColor[j];
      }


      // specularity and stuff
      pipDraw->dwID = ((DWORD)m_wSpecReflection << 16) | m_wSpecDirection;
      pipDraw->fZ = fBevelHeight+5; // BUGFIX - Was (int) ((fBevelHeight + 5) * (fp)0x10000);;

      // adjust Z based upon crossing boundary?
      if (fCrossedRing) // BUGFIX - Was fTransitions
         pipDraw->fZ += m_fRingBump / fPixelLen; // BUGFIX - Was (int) (m_fRingBump / fPixelLen * (fp)0x10000);

   }
   delete pTree;

   // extra effects
   m_Noise.Apply (pImage, fPixelLen);

   // change color over length?
   fp fBrightTop, fBrightBottom;
   fBrightTop = randf(m_fBrightMin, m_fBrightMax);
   fBrightBottom = randf(m_fBrightMin, m_fBrightMax);

   // is there a height difference
   fp fZTop, fZBottom;
   fp fBend = m_fBoardBend / fPixelLen;
   fZTop = randf (-fBend, fBend);
   fZBottom = randf (-fBend, fBend);

   // do the cahnges
   for (y = 0; y < dwY; y++) {
      fp fAlpha = (fp) y / (fp)dwY;
      fp fBright, fZ;
      for (x = 0; x < dwX; x++) {
         PIMAGEPIXEL pip = pImage->Pixel(x,y);


         // do z
         fZ = fZTop + (fZBottom - fZTop) * fAlpha;
         pip->fZ += fZ; // BUGFIX - Was (int) (fZ * 0x10000);

         // merge colors
         fBright =  fBrightTop + (fBrightBottom - fBrightTop) * fAlpha;
         for (i = 0; i < 3; i++) {
            fp fc;
            fc = (&pip->wRed)[i] * fBright;
            fc = min(0xffff,fc);
            fc = max(0, fc);
            (&pip->wRed)[i] = (WORD)fc;
         }
      }
   }


   // Bevel sides
   DWORD dwBevelXY = (DWORD) (m_fBevelWidth / fPixelLen);
   for (i = 0; i < dwBevelXY; i++) {
      // what's the height at this point
      fp fZ;
      //int iZ;
      fZ = fBevelHeight - (fp) i / (fp) dwBevelXY * fBevelHeight;

      //iZ = (int) (fZ * 0x10000);

      // left and right edges
      for (y = 0; y < dwY; y++) {
         pImage->Pixel(i, y)->fZ -= fZ;
         pImage->Pixel(dwX - i - 1, y)->fZ -= fZ;
      }

   }

   // merge into original image
   POINT pm;
   pm.x = iX;
   pm.y = iY;
   pImage->TGMerge (NULL, pMain, pm);

   delete pImage;
}


BOOL GeneratePlankPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectGeneratePlank pv = (PCTextEffectGeneratePlank) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", SpecularityToMaterial (pv->m_wSpecDirection, pv->m_wSpecReflection));

         MeasureToString (pPage, L"bevelheight", pv->m_fBevelHeight, TRUE);
         MeasureToString (pPage, L"bevelwidth", pv->m_fBevelWidth, TRUE);
         MeasureToString (pPage, L"boardbend", pv->m_fBoardBend, TRUE);
         MeasureToString (pPage, L"ringbump", pv->m_fRingBump, TRUE);

         pControl = pPage->ControlFind (L"brightmin");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBrightMin * 100));
         pControl = pPage->ControlFind (L"brightmax");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBrightMax * 100));

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 5; i++) {
            swprintf (szTemp, L"colorcolor%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColors[i]);
         }
         FillStatusColor (pPage, L"transcolor", pv->m_cTransitionColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR szColor = L"colorbutton";
         DWORD dwLen = (DWORD)wcslen(szColor);
         if (!wcsncmp(p->pControl->m_pszName, szColor, dwLen)) {
            DWORD i = _wtoi(p->pControl->m_pszName + dwLen);
            i = min(4,i);
            WCHAR szTemp[64];
            swprintf (szTemp, L"colorcolor%d", i);
            pv->m_acColors[i] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColors[i], pPage, szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"transbutton")) {
            pv->m_cTransitionColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cTransitionColor, pPage, L"transcolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            CMaterial Mat;
            Mat.InitFromID (dwVal);
            pv->m_wSpecDirection = Mat.m_wSpecExponent;
            pv->m_wSpecReflection = Mat.m_wSpecReflect;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // set the material
         PCEscControl pControl;
         MeasureParseString (pPage, L"bevelheight", &pv->m_fBevelHeight);
         pv->m_fBevelHeight = max(0,pv->m_fBevelHeight);
         MeasureParseString (pPage, L"bevelwidth", &pv->m_fBevelWidth);
         pv->m_fBevelWidth = max(0,pv->m_fBevelWidth);
         MeasureParseString (pPage, L"boardbend", &pv->m_fBoardBend);
         pv->m_fBoardBend = max(0,pv->m_fBoardBend);
         MeasureParseString (pPage, L"ringbump", &pv->m_fRingBump);

         pControl = pPage->ControlFind (L"brightmin");
         if (pControl)
            pv->m_fBrightMin = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"brightmax");
         if (pControl)
            pv->m_fBrightMax = (fp) pControl->AttribGetInt (Pos()) / 100.0;

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Planks";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextEffectGeneratePlank::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREGENERATEPLANK, GeneratePlankPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"noise")) {
      if (m_Noise.Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"tree")) {
      if (m_Tree.Dialog (pWindow, pCreator))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}









/***********************************************************************************
MassageForCreator - Called at the end of the render. It looks at the image and overrides
some settings in CMaterial and TEXTINFO based upon the image.

inputs
   PCImage        pImage - Image to look at
   PCMaterial     pMat - material
   PTEXTINFO      pInfo - info
   BOOL           fFlatten - If TRUE, the bump map is pre-applied and any Z is removed.
                     Use this to reduce the computation when rendering
returns
   none
*/
void MassageForCreator (PCImage pImage, PCMaterial pMat, PTEXTINFO pInfo, BOOL fFlatten)
{
   WORD     awSpecPow[2];   // min and max specularity power
   WORD     awSpecScale[2]; // min and max specularity scale
   WORD     awTrans[2];     // min and max transparency
   fp      afZ[2];         // min and max depth

   // find min and max
   PIMAGEPIXEL pip;
   pip = pImage->Pixel(0,0);
   DWORD dwNum, i;
   dwNum = pImage->Width() * pImage->Height();
   double fZAverage = 0;
   for (i = 0; i < dwNum; i++, pip++) {
      if (!i) {
         awSpecPow[0] = awSpecPow[1] = LOWORD(pip->dwID);
         awSpecScale[0] = awSpecScale[1] = HIWORD(pip->dwID);
         awTrans[0] = awTrans[1] = LOBYTE(pip->dwIDPart);
         afZ[0] = afZ[1] = pip->fZ; // BUGFIX - Use fZ not iZ
         continue;
      }

      awSpecPow[0] = min(awSpecPow[0], LOWORD(pip->dwID));
      awSpecPow[1] = max(awSpecPow[1], LOWORD(pip->dwID));
      awSpecScale[0] = min(awSpecScale[0], HIWORD(pip->dwID));
      awSpecScale[1] = max(awSpecScale[1], HIWORD(pip->dwID));
      awTrans[0] = min(awTrans[0], LOBYTE(pip->dwIDPart));
      awTrans[1] = max(awTrans[1], LOBYTE(pip->dwIDPart));
      afZ[0] = min(afZ[0], pip->fZ);
      afZ[1] = max(afZ[1], pip->fZ);
      fZAverage += pip->fZ;
   }

   // if any of these are identical set set the default material, else set
   // a flag indicating that there's a map
   if ((awSpecPow[0] == awSpecPow[1]) && (awSpecScale[0] == awSpecScale[1])) {
      // BUGFIX - Do nothing
      //pMat->m_wSpecExponent = awSpecPow[0];
      //pMat->m_wSpecReflect = awSpecScale[0];
   }
   else {
      pMat->m_wSpecExponent = awSpecPow[0]/2 + awSpecPow[1]/2;
      pMat->m_wSpecReflect = awSpecScale[0]/2 + awSpecScale[1]/2;
      pInfo->dwMap |= 0x01;   // specularity map
   }

   if (awTrans[0] == awTrans[1]) {
      if (awTrans[0])   // only override if slightly transparent
         pMat->m_wTransparency = awTrans[0] | (awTrans[0] << 8);  // ensures that 0xff -> 0xffff
   }
   else {
      pMat->m_wTransparency = awTrans[0] | (awTrans[0] << 8);  // ensures that 0xff -> 0xffff
         // NOTE: Only taking lowest so will default to lowest transparency - ensures
         // that if it's mostly opqaue it's drawn as shadows
      pInfo->dwMap |= 0x04;   // transparency map
   }

   // flatten?
   if (fFlatten) {
      // apply the bump map
      pImage->TGBumpMapApply ();

      pip = pImage->Pixel(0,0);
      for (i = 0; i < dwNum; i++, pip++)
         pip->fZ = 0;

      afZ[0] = afZ[1] = 0;

   }

   if (afZ[0] != afZ[1]) {
      pInfo->dwMap |= 0x02;   // bump map

      // while at it, make average z value 0
      // BUGFIX - Use fp instead of int
      fp fAverage;
      //fAverage = afZ[0]/2 + afZ[1] / 2;
      fAverage = fZAverage / (double)dwNum;   // dont use other average
      pip = pImage->Pixel(0,0);
      for (i = 0; i < dwNum; i++, pip++) {
         pip->fZ -= fAverage;
      }

   }
}

/***********************************************************************************
CTextCreatorTiles - Draw tiles.
*/

class CTextCreatorTiles : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorTiles (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD            m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   DWORD            m_dwTilesAcross; // number of tiles across in pattern, 1+
   DWORD            m_dwTilesDown;   // number of tiles down in pattern, 1+
   DWORD            m_dwShiftInverse; // amount to shift tile over every row, inverted.
      // if 1 then one above the next, 2 then staggered 50%, 3 stagger 33%, etc.
   fp               m_fSpacing;   // spacing (in meters) between tiles
   fp               m_fCornerSize;  // size of corner tiles
   BOOL             m_fAltColor;  // if TRUE then alternte tile colors using cAltA and cAltB
   COLORREF         m_cAltA;   // alternatining color, ranges from cAltA to cAltB
   COLORREF         m_cAltB;
   CTextEffectGrout  m_Grout; // grout information
   CTextEffectMakeTile m_Tile; // tile color
   CTextEffectMakeTile m_CornerTile;   // corner tile info
   CTextEffectDirtPaint m_DirtPaint;  // dirt or paint to add
};
typedef CTextCreatorTiles *PCTextCreatorTiles;



/****************************************************************************
TilesPage
*/
BOOL TilesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorTiles pv = (PCTextCreatorTiles) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         DoubleToControl (pPage, L"tilesacross", pv->m_dwTilesAcross);
         DoubleToControl (pPage, L"tilesdown", pv->m_dwTilesDown);
         MeasureToString (pPage, L"spacing", pv->m_fSpacing, TRUE);
         MeasureToString (pPage, L"cornersize", pv->m_fCornerSize, TRUE);

         ComboBoxSet (pPage, L"shiftinverse", pv->m_dwShiftInverse);

         pControl = pPage->ControlFind (L"altcolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fAltColor);
         FillStatusColor (pPage, L"altacolor", pv->m_cAltA);
         FillStatusColor (pPage, L"altbcolor", pv->m_cAltB);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"altabutton")) {
            pv->m_cAltA = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cAltA, pPage, L"altacolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"altbbutton")) {
            pv->m_cAltB = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cAltB, pPage, L"altbcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shiftinverse")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            dwVal = max(1,dwVal);   // cant have 0
            if (dwVal == pv->m_dwShiftInverse)
               break; // unchanged
            pv->m_dwShiftInverse = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         pv->m_dwTilesAcross = (DWORD)DoubleFromControl (pPage, L"tilesacross");
         pv->m_dwTilesAcross = max(1,pv->m_dwTilesAcross);
         pv->m_dwTilesDown = (DWORD) DoubleFromControl (pPage, L"tilesdown");
         pv->m_dwTilesDown = max(1,pv->m_dwTilesDown);
         MeasureParseString (pPage, L"spacing", &pv->m_fSpacing);
         pv->m_fSpacing = max(0,pv->m_fSpacing);
         MeasureParseString (pPage, L"cornersize", &pv->m_fCornerSize);
         pv->m_fCornerSize = max(0,pv->m_fCornerSize);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"altcolor");
         if (pControl)
            pv->m_fAltColor = pControl->AttribGetBOOL (Checked());
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Tiles";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorTiles::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURETILES, TilesPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"tile")) {
      if (m_Tile.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   if (pszRet && !_wcsicmp(pszRet, L"cornertile")) {
      if (m_CornerTile.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"grout")) {
      if (m_Grout.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}


CTextCreatorTiles::CTextCreatorTiles (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = 0;
   m_dwTilesAcross = m_dwTilesDown = m_dwShiftInverse = 0;
   m_fCornerSize = 0;
   m_fSpacing = 0;
   m_fAltColor = FALSE;
   m_cAltA = m_cAltB = 0;
   m_Material.InitFromID (MATERIAL_TILEGLAZED);
   m_Tile.m_fDiagCorner = 0;

   if (HIWORD(m_dwType) == 0) {  // tiles
      m_iSeed = 24354;
      m_fPixelLen = 0.005;
      m_fSpacing = 0.005;
      m_dwTilesAcross = 2;
      m_dwTilesDown = 2;
      m_dwShiftInverse = 1;
      m_fAltColor = FALSE;
      m_cAltA = RGB(0xff,00,00);
      m_cAltB = RGB(0xc0,00,00);
      m_Tile.m_fBevel = 0.01;
      m_Tile.m_fThickness = 0.005;
      m_Tile.m_fHeight = .30;
      m_Tile.m_fWidth = .30;
      m_Tile.m_fRockLR = .001;
      m_Tile.m_fRockTB = .001;
      m_Tile.m_cSurfA = RGB(0x40,0x40,0xff);
      m_Tile.m_cSurfB = RGB(0x40,0x40,0xff);
      m_Tile.m_dwBevelMode = 2;
      m_Tile.m_wSpecReflection = 0x4000;
      m_Tile.m_wSpecDirection = 4000;

      m_Tile.m_aNoise[0].m_fTurnOn = FALSE;
      m_Tile.m_aNoise[0].m_fNoiseX = .1;
      m_Tile.m_aNoise[0].m_fNoiseY = .1;
      m_Tile.m_aNoise[0].m_cMax = RGB(0x80, 0xff, 0x80);
      m_Tile.m_aNoise[0].m_cMin = RGB(0xff, 0x80, 0x80);
      m_Tile.m_aNoise[0].m_fTransMax = .6;
      m_Tile.m_aNoise[0].m_fTransMin = 1;

      m_Tile.m_aNoise[1].m_fTurnOn = FALSE;
      m_Tile.m_aNoise[1].m_fNoiseX = .025;
      m_Tile.m_aNoise[1].m_fNoiseY = .025;
      m_Tile.m_aNoise[1].m_cMax = RGB(0x80, 0x80, 0xff);
      m_Tile.m_aNoise[1].m_cMin = RGB(0x80, 0xff, 0x80);
      m_Tile.m_aNoise[1].m_fTransMax = .6;
      m_Tile.m_aNoise[1].m_fTransMin = 1;

      m_Tile.m_aNoise[2].m_fTurnOn = TRUE;
      m_Tile.m_aNoise[2].m_fNoiseX = .1;
      m_Tile.m_aNoise[2].m_fNoiseY = .1;
      m_Tile.m_aNoise[2].m_fZDeltaMax = 0;
      m_Tile.m_aNoise[2].m_fZDeltaMin = .001;
      m_Tile.m_aNoise[2].m_fTransMax = 1;
      m_Tile.m_aNoise[2].m_fTransMin = 1;

      m_Tile.m_aNoise[3].m_fTurnOn = TRUE;
      m_Tile.m_aNoise[3].m_fNoiseX = .05;
      m_Tile.m_aNoise[3].m_fNoiseY = .05;
      m_Tile.m_aNoise[3].m_fZDeltaMax = 0;
      m_Tile.m_aNoise[3].m_fZDeltaMin = .001;
      m_Tile.m_aNoise[3].m_fTransMax = 1;
      m_Tile.m_aNoise[3].m_fTransMin = 1;

      m_Tile.m_aNoise[4].m_fTurnOn = TRUE;
      m_Tile.m_aNoise[4].m_fNoiseX = .01;
      m_Tile.m_aNoise[4].m_fNoiseY = .01;
      m_Tile.m_aNoise[4].m_fZDeltaMax = 0;
      m_Tile.m_aNoise[4].m_fZDeltaMin = .0005;
      m_Tile.m_aNoise[4].m_fTransMax = 1;
      m_Tile.m_aNoise[4].m_fTransMin = 1;

      m_Tile.m_Chip.m_fTurnOn = FALSE;
      m_Tile.m_Chip.m_fTileHeight = m_Tile.m_fThickness;
      m_Tile.m_Chip.m_fFilterSize = .015;
      m_Tile.m_Chip.m_cChip = RGB(0xb0,0xb0,0xb0);
      m_Tile.m_Chip.m_fChipTrans = 0;
      m_Tile.m_Chip.m_wSpecDirection = 100;
      m_Tile.m_Chip.m_wSpecReflection = 0x500;
      m_Tile.m_Chip.m_aNoise[0].m_fTurnOn = TRUE;
      m_Tile.m_Chip.m_aNoise[0].m_fNoiseX = .025;
      m_Tile.m_Chip.m_aNoise[0].m_fNoiseY = .025;
      m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[0].m_fTransMax = 1;
      m_Tile.m_Chip.m_aNoise[0].m_fTransMin = 1;

      m_Tile.m_Chip.m_aNoise[1].m_fTurnOn = TRUE;
      m_Tile.m_Chip.m_aNoise[1].m_fNoiseX = .01;
      m_Tile.m_Chip.m_aNoise[1].m_fNoiseY = .01;
      m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[1].m_fTransMax = 1;
      m_Tile.m_Chip.m_aNoise[1].m_fTransMin = 1;

      m_Tile.m_Chip.m_aNoise[2].m_fTurnOn = TRUE;
      m_Tile.m_Chip.m_aNoise[2].m_fNoiseX = .005;
      m_Tile.m_Chip.m_aNoise[2].m_fNoiseY = .005;
      m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/4;
      m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/4;
      m_Tile.m_Chip.m_aNoise[2].m_fTransMax = 1;
      m_Tile.m_Chip.m_aNoise[2].m_fTransMin = 1;

      m_Grout.m_cColor = RGB(0xf0,0xf0,0xf0);
      m_Grout.m_aNoise[0].m_fTurnOn = TRUE;
      m_Grout.m_aNoise[0].m_fNoiseX = .1;
      m_Grout.m_aNoise[0].m_fNoiseY = .1;
      m_Grout.m_aNoise[0].m_fZDeltaMax = 0;
      m_Grout.m_aNoise[0].m_fZDeltaMin = -.003;
      m_Grout.m_aNoise[0].m_fTransMax = 1;
      m_Grout.m_aNoise[0].m_fTransMin = 1;

      m_Grout.m_aNoise[1].m_fTurnOn = TRUE;
      m_Grout.m_aNoise[1].m_fNoiseX = .025;
      m_Grout.m_aNoise[1].m_fNoiseY = .025;
      m_Grout.m_aNoise[1].m_fZDeltaMax = 0;
      m_Grout.m_aNoise[1].m_fZDeltaMin = -.001;
      m_Grout.m_aNoise[1].m_fTransMax = 1;
      m_Grout.m_aNoise[1].m_fTransMin = 1;

      m_DirtPaint.m_fTurnOn = FALSE;
      m_DirtPaint.m_cColor = RGB(0x60,0x60,0);
      m_DirtPaint.m_fAHeight = .003;
      m_DirtPaint.m_fFilterSize = .01;
      m_DirtPaint.m_fTransAtA = .25;
      m_DirtPaint.m_fTransNone = 1;
      m_DirtPaint.m_wSpecDirection = 100;
      m_DirtPaint.m_wSpecReflection = 0;

      switch (LOWORD(m_dwType)) {
      case 0:  // white tiles
         m_Tile.m_cSurfA = m_Tile.m_cSurfB = RGB(0xff,0xff,0xff);
         break;

      case 1:  // smooth white tiles
         m_Tile.m_cSurfA = m_Tile.m_cSurfB = RGB(0xff,0xff,0xff);
         m_Tile.m_aNoise[3].m_fTurnOn = FALSE;
         m_Tile.m_aNoise[4].m_fTurnOn = FALSE;
         m_Tile.m_aNoise[5].m_fTurnOn = FALSE;
         break;

      case 2:  // red
         m_Tile.m_cSurfA = RGB(0xc0, 0x40, 0x40);
         m_Tile.m_cSurfB = RGB(0xd0,0x30,0x50);
         m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMin = RGB(0x40, 0, 0);
         m_Tile.m_aNoise[1].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[1].m_cMax = m_Tile.m_aNoise[1].m_cMin = RGB(0xff, 0x80, 0x80);
         break;

      case 3:  // blue
         m_Tile.m_cSurfA = RGB(0x80, 0x80, 0xff);
         m_Tile.m_cSurfB = RGB(0x70,0x90,0xf0);
         m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMin = RGB(0x80, 0xff, 0x80);
         m_Tile.m_aNoise[1].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[1].m_cMax = m_Tile.m_aNoise[1].m_cMin = RGB(0x40, 0x40, 0xc0);
         break;

      case 4:  // green
         m_Tile.m_cSurfA = RGB(0, 0xc0, 0x10);
         m_Tile.m_cSurfB = RGB(0x10, 0xd0, 0x00);
         m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMin = RGB(0, 0, 0x40);
         m_Tile.m_aNoise[1].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[1].m_cMax = m_Tile.m_aNoise[1].m_cMin = RGB(0xff, 0xff, 0xff);
         break;

      case 5:  // checkerboard
         m_Tile.m_cSurfA = m_Tile.m_cSurfB = RGB(0xff,0xff,0xff);
         m_fAltColor = TRUE;
         m_cAltA = m_cAltB = 0;
         break;

      case 6:  // cyan checkerboard
         m_Tile.m_cSurfA = RGB(0x80, 0xff, 0xff);
         m_Tile.m_cSurfB = RGB(0x90, 0xf0, 0xf0);
         m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMin = RGB(0x80, 0xff, 0x80);
         m_Tile.m_aNoise[1].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[1].m_cMax = m_Tile.m_aNoise[1].m_cMin = RGB(0xc0, 0xc0, 0xff);

         m_fAltColor = TRUE;
         m_cAltA = m_cAltB = RGB(0xff,0xff,0xff);
         break;

      case 7:  // slate
         m_dwTilesAcross = 3;
         m_dwTilesDown = 3;
         m_Tile.m_cSurfA = RGB(0x40,0x50,0x60);
         m_Tile.m_cSurfB = RGB(0x20,0x40,0x30);
         m_Tile.m_aNoise[0].m_fTurnOn;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMin = RGB(0x30, 0x30, 0x30);
         m_Tile.m_aNoise[3].m_fZDeltaMin = .3;
         //m_Tile.m_aNoise[4].m_fTurnOn = FALSE;
         m_Tile.m_aNoise[5].m_fTurnOn = FALSE;
         m_dwShiftInverse = 2;
         m_fSpacing = .005;
         m_Grout.m_cColor = RGB(0x80, 0x80, 0x80);
         m_Tile.m_fHeight = .20;
         m_Tile.m_fRockLR = .002;
         m_Tile.m_fRockTB = .002;
         m_Tile.m_dwBevelMode = 0;
         m_Tile.m_wSpecReflection = 0x8000;
         m_Tile.m_wSpecDirection = 1000;

         m_Tile.m_Chip.m_fTurnOn = FALSE; // was true
         m_Tile.m_Chip.m_fChipTrans = 1;
         m_Tile.m_Chip.m_fFilterSize = .03;
         m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMax = .01;
         m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMin = -.01;
         m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMax = .01;
         m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMin = -.01;
         break;

      case 8:  // teracotta
         m_fSpacing = .005;
         m_dwTilesAcross = 3;
         m_dwTilesDown = 3;

         m_Grout.m_cColor = RGB(0x80,0x80,0x80);
         m_Tile.m_wSpecReflection = 0x2000;
         m_Tile.m_cSurfA = RGB(0xe0,0x90,0x40);
         m_Tile.m_cSurfB = RGB(0xff,0xa0,0x60);
         m_Tile.m_dwBevelMode = 1;
         m_Tile.m_aNoise[2].m_fZDeltaMin = .002;
         m_Tile.m_aNoise[4].m_fZDeltaMin = .001;
         m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMax = RGB(0x90, 0x50, 0x50);

         m_Tile.m_Chip.m_fTurnOn = TRUE;
         m_Tile.m_Chip.m_fChipTrans = 0;
         m_Tile.m_Chip.m_cChip = RGB(0xe0,0x80,0x60);
         m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMin *= 2;
         m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMax *= 2;
         break;
      }
   }
   else if (HIWORD(m_dwType) == 1) {   // bricks
      m_iSeed = 24354;
      m_fPixelLen = 0.0025;
      m_fSpacing = 0.005;
      m_dwTilesAcross = 4;
      m_dwTilesDown = 4;
      m_dwShiftInverse = 2;
      m_fAltColor = FALSE;
      m_cAltA = RGB(0xff,00,00);
      m_cAltB = RGB(0xc0,00,00);
      m_Tile.m_fBevel = 0.005;
      m_Tile.m_fThickness = 0.01;
      m_Tile.m_fHeight = .05;
      m_Tile.m_fWidth = .15;
      m_Tile.m_fRockLR = .005;
      m_Tile.m_fRockTB = 0;
      m_Tile.m_wSpecDirection = 1000;
      m_Tile.m_wSpecReflection = 0x400;
      m_Tile.m_cSurfA = RGB(0xe0,0x90,0x40);
      m_Tile.m_cSurfB = RGB(0xff,0xa0,0x60);
      m_Tile.m_dwBevelMode = 0;
      m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
      m_Tile.m_aNoise[0].m_fNoiseX = .1;
      m_Tile.m_aNoise[0].m_fNoiseY = .1;
      m_Tile.m_aNoise[0].m_cMax = RGB(0x80, 0x40, 0x30);
      m_Tile.m_aNoise[0].m_cMin = RGB(0x80, 0x40, 0x30);
      m_Tile.m_aNoise[0].m_fTransMax = .6;
      m_Tile.m_aNoise[0].m_fTransMin = 1;

      m_Tile.m_aNoise[1].m_fTurnOn = FALSE;
      m_Tile.m_aNoise[1].m_fNoiseX = .025;
      m_Tile.m_aNoise[1].m_fNoiseY = .025;
      m_Tile.m_aNoise[1].m_cMax = RGB(0x80, 0x80, 0xff);
      m_Tile.m_aNoise[1].m_cMin = RGB(0x80, 0xff, 0x80);
      m_Tile.m_aNoise[1].m_fTransMax = .6;
      m_Tile.m_aNoise[1].m_fTransMin = 1;

      m_Tile.m_aNoise[2].m_fTurnOn = FALSE;
      m_Tile.m_aNoise[2].m_fNoiseX = .1;
      m_Tile.m_aNoise[2].m_fNoiseY = .1;
      m_Tile.m_aNoise[2].m_fZDeltaMax = 0;
      m_Tile.m_aNoise[2].m_fZDeltaMin = m_Tile.m_fThickness / 2;
      m_Tile.m_aNoise[2].m_fTransMax = 1;
      m_Tile.m_aNoise[2].m_fTransMin = 1;

      m_Tile.m_aNoise[3].m_fTurnOn = TRUE;
      m_Tile.m_aNoise[3].m_fNoiseX = .005;
      m_Tile.m_aNoise[3].m_fNoiseY = .005;
      m_Tile.m_aNoise[3].m_fZDeltaMax = -.001;
      m_Tile.m_aNoise[3].m_fZDeltaMin = .002;
      m_Tile.m_aNoise[3].m_fTransMax = 1;
      m_Tile.m_aNoise[3].m_fTransMin = 1;

      m_Tile.m_aNoise[4].m_fTurnOn = TRUE;
      m_Tile.m_aNoise[4].m_fNoiseX = .002;
      m_Tile.m_aNoise[4].m_fNoiseY = .002;
      m_Tile.m_aNoise[4].m_fZDeltaMax = 0;
      m_Tile.m_aNoise[4].m_fZDeltaMin = .002;
      m_Tile.m_aNoise[4].m_fTransMax = 1;
      m_Tile.m_aNoise[4].m_fTransMin = 1;

      m_Tile.m_Chip.m_fTurnOn = TRUE;
      m_Tile.m_Chip.m_fTileHeight = m_Tile.m_fThickness;
      m_Tile.m_Chip.m_fFilterSize = .015;
      m_Tile.m_Chip.m_cChip = RGB(0x80,0x40,0x30);
      m_Tile.m_Chip.m_fChipTrans = .8;
      m_Tile.m_Chip.m_wSpecDirection = 100;
      m_Tile.m_Chip.m_wSpecReflection = 0x500;
      m_Tile.m_Chip.m_aNoise[0].m_fTurnOn = TRUE;
      m_Tile.m_Chip.m_aNoise[0].m_fNoiseX = .025;
      m_Tile.m_Chip.m_aNoise[0].m_fNoiseY = .025;
      m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[0].m_fTransMax = 1;
      m_Tile.m_Chip.m_aNoise[0].m_fTransMin = 1;

      m_Tile.m_Chip.m_aNoise[1].m_fTurnOn = TRUE;
      m_Tile.m_Chip.m_aNoise[1].m_fNoiseX = .01;
      m_Tile.m_Chip.m_aNoise[1].m_fNoiseY = .01;
      m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/1;
      m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/1;
      m_Tile.m_Chip.m_aNoise[1].m_fTransMax = 1;
      m_Tile.m_Chip.m_aNoise[1].m_fTransMin = 1;

      m_Tile.m_Chip.m_aNoise[2].m_fTurnOn = TRUE;
      m_Tile.m_Chip.m_aNoise[2].m_fNoiseX = .005;
      m_Tile.m_Chip.m_aNoise[2].m_fNoiseY = .005;
      m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
      m_Tile.m_Chip.m_aNoise[2].m_fTransMax = 1;
      m_Tile.m_Chip.m_aNoise[2].m_fTransMin = 1;

      m_Grout.m_cColor = RGB(0xf0,0xf0,0xf0);
      m_Grout.m_aNoise[0].m_fTurnOn = TRUE;
      m_Grout.m_aNoise[0].m_fNoiseX = .1;
      m_Grout.m_aNoise[0].m_fNoiseY = .1;
      m_Grout.m_aNoise[0].m_fZDeltaMax = 0;
      m_Grout.m_aNoise[0].m_fZDeltaMin = -.01;
      m_Grout.m_aNoise[0].m_fTransMax = 1;
      m_Grout.m_aNoise[0].m_fTransMin = 1;

      m_Grout.m_aNoise[1].m_fTurnOn = TRUE;
      m_Grout.m_aNoise[1].m_fNoiseX = .025;
      m_Grout.m_aNoise[1].m_fNoiseY = .025;
      m_Grout.m_aNoise[1].m_fZDeltaMax = 0;
      m_Grout.m_aNoise[1].m_fZDeltaMin = -.002;
      m_Grout.m_aNoise[1].m_fTransMax = 1;
      m_Grout.m_aNoise[1].m_fTransMin = 1;

      m_DirtPaint.m_fTurnOn = TRUE;
      m_DirtPaint.m_cColor = RGB(0x40,0x30,0);
      m_DirtPaint.m_fAHeight = .003;
      m_DirtPaint.m_fFilterSize = .01;
      m_DirtPaint.m_fTransAtA = .50;
      m_DirtPaint.m_fTransNone = 1;
      m_DirtPaint.m_wSpecDirection = 100;
      m_DirtPaint.m_wSpecReflection = 0;

      switch (LOWORD(m_dwType)) {
      case 0:  // brick (default)
         break;

      case 1:  // dark bricks
         m_Tile.m_fWidth = m_Tile.m_fHeight * 4;
         m_Tile.m_cSurfA = RGB(0xb0,0x90,0x70);
         m_Tile.m_cSurfB = RGB(0xff,0xe0,0xa0);
         m_Tile.m_dwBevelMode = 2;
         m_Tile.m_fBevel = .01;
         m_Tile.m_aNoise[0].m_cMax = RGB(0x60, 0x40, 0x30);
         m_Tile.m_aNoise[0].m_cMin = RGB(0x60, 0x40, 0x30);
         break;

      case 2:  // cement blocks
         m_fPixelLen = .005;
         m_dwTilesAcross = 2;
         m_dwTilesDown = 2;
         m_Tile.m_fHeight = .20;
         m_Tile.m_fWidth = .40;
         m_Tile.m_fThickness = .015;
         m_Tile.m_fRockLR = .01;
         m_Tile.m_fRockTB = .0025;
         m_fSpacing = .01;
         m_Tile.m_cSurfA = RGB(0xf0,0xf0,0xf0);
         m_Tile.m_cSurfB = RGB(0xe0,0xe0,0xe0);
         m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMin = RGB(0xd0,0xd0,0xd0);
         m_Tile.m_aNoise[1].m_fTurnOn = FALSE;
         m_Tile.m_aNoise[2].m_fTurnOn = FALSE;
         m_Tile.m_aNoise[3].m_fTurnOn = FALSE;
         m_Tile.m_aNoise[4].m_fTurnOn = TRUE;

         m_Tile.m_Chip.m_fChipTrans = 1;
         break;

      case 3:  // painted bricks
         m_DirtPaint.m_fTurnOn = TRUE;
         m_DirtPaint.m_cColor = RGB(0x40,0xff,0xff);
         m_DirtPaint.m_fFilterSize = .01;
         m_DirtPaint.m_fTransAtA = 0;
         m_DirtPaint.m_fTransNone = 0;
         m_DirtPaint.m_wSpecDirection = 5000;
         m_DirtPaint.m_wSpecReflection = 0x4000;
         break;

      case 4:  // stone
         m_fPixelLen = .005;
         m_dwTilesAcross = 4;
         m_dwTilesDown = 4;
         m_Grout.m_cColor = RGB(0xc0, 0xc0, 0xc0);
         m_Grout.m_aNoise[0].m_fZDeltaMin = -.02;
         m_Grout.m_aNoise[1].m_fZDeltaMin = -.005;
         m_Tile.m_dwBevelMode = 2;
         m_Tile.m_fBevel = .03;
         m_Tile.m_fHeight = .20;
         m_Tile.m_fWidth = .40;
         m_Tile.m_fThickness = .02;
         m_Tile.m_fRockLR = .04;
         m_Tile.m_fRockTB = .04;
         m_fSpacing = .02;
         m_Tile.m_cSurfA = RGB(0xc0,0xa0,0xb0);
         m_Tile.m_cSurfB = RGB(0xf0,0xe0,0xd0);
         m_Tile.m_aNoise[0].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[0].m_cMax = m_Tile.m_aNoise[0].m_cMin = RGB(0xa0,0xb0,0x70);
         m_Tile.m_aNoise[1].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[1].m_cMax = m_Tile.m_aNoise[1].m_cMin = RGB(0x50,0x60,0x70);
         m_Tile.m_aNoise[2].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[2].m_fZDeltaMin = -.01;
         m_Tile.m_aNoise[2].m_fZDeltaMax = .02;
         m_Tile.m_aNoise[3].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[3].m_fZDeltaMin = -.01;
         m_Tile.m_aNoise[3].m_fZDeltaMin = .01;
         m_Tile.m_aNoise[4].m_fTurnOn = TRUE;
         m_Tile.m_aNoise[4].m_fZDeltaMin = 0;
         m_Tile.m_aNoise[4].m_fZDeltaMin = .001;
         m_Tile.m_wSpecDirection = 1000;
         m_Tile.m_wSpecReflection = 0x4000;

         m_Tile.m_Chip.m_fTurnOn = TRUE;
         m_Tile.m_Chip.m_fChipTrans = 1;
         m_Tile.m_Chip.m_fTileHeight = .02;
         m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMax *= 4;
         m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMin *= 4;
         m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMax *= 4;
         m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMin *= 4;
         m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMax *= 4;
         m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMin *= 4;
         break;
      }
   }

   memcpy (&m_CornerTile, &m_Tile, sizeof(m_Tile));
}

void CTextCreatorTiles::Delete (void)
{
   delete this;
}

static PWSTR gpszTiles = L"Tiles";
static PWSTR gpszType = L"Type";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszPixelLen = L"PixelLen";
static PWSTR gpszTilesAcross = L"TilesAcross";
static PWSTR gpszTilesDown = L"TilesDown";
static PWSTR gpszShiftInverse = L"ShiftInverse";
static PWSTR gpszSpacing = L"Spacing";
static PWSTR gpszAltColor = L"AltColor";
static PWSTR gpszAltA = L"AltA";
static PWSTR gpszAltB = L"AltB";
static PWSTR gpszCornerTile = L"CornerTile";
static PWSTR gpszCornerSize = L"CornerSize";

PCMMLNode2 CTextCreatorTiles::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTiles);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszSeed, (int) m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszTilesAcross, (int)m_dwTilesAcross);
   MMLValueSet (pNode, gpszTilesDown, (int)m_dwTilesDown);
   MMLValueSet (pNode, gpszShiftInverse, (int)m_dwShiftInverse);
   MMLValueSet (pNode, gpszCornerSize, m_fCornerSize);
   MMLValueSet (pNode, gpszSpacing, m_fSpacing);
   MMLValueSet (pNode, gpszAltColor, (BOOL) m_fAltColor);
   MMLValueSet (pNode, gpszAltA, (int) m_cAltA);
   MMLValueSet (pNode, gpszAltB, (int) m_cAltB);

   PCMMLNode2 pSub;
   pSub = m_Grout.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGrout);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Tile.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMakeTile);
      pNode->ContentAdd (pSub);
   }
   pSub = m_CornerTile.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszCornerTile);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorTiles::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int) 0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_dwTilesAcross = (DWORD) MMLValueGetInt (pNode, gpszTilesAcross, (int)0);
   m_dwTilesDown = (DWORD) MMLValueGetInt (pNode, gpszTilesDown, (int)0);
   m_dwShiftInverse = (DWORD) MMLValueGetInt (pNode, gpszShiftInverse, (int)0);
   m_fCornerSize = MMLValueGetDouble (pNode, gpszCornerSize, 0);
   m_fSpacing = MMLValueGetDouble (pNode, gpszSpacing, 0);
   m_fAltColor = (BOOL) MMLValueGetInt (pNode, gpszAltColor, (BOOL) 0);
   m_cAltA = (COLORREF) MMLValueGetInt (pNode, gpszAltA, (int) 0);
   m_cAltB = (COLORREF) MMLValueGetInt (pNode, gpszAltB, (int) 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGrout), &psz, &pSub);
   if (pSub)
      m_Grout.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszMakeTile), &psz, &pSub);
   if (pSub)
      m_Tile.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszCornerTile), &psz, &pSub);
   if (pSub)
      m_CornerTile.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);

   return TRUE;
}

BOOL CTextCreatorTiles::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // pixels per tile
   DWORD dwXTile, dwYTile, dwGrout, dwTilesAcross, dwTilesDown, dwShiftInverse;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwXTile = (DWORD)((m_Tile.m_fWidth + fPixelLen/2) / fPixelLen);
   dwYTile = (DWORD) ((m_Tile.m_fHeight + fPixelLen/2) / fPixelLen);
   dwGrout = (DWORD) (m_fSpacing / fPixelLen);
   dwTilesAcross = m_dwTilesAcross;
   dwTilesDown = m_dwTilesDown;
   dwShiftInverse = m_dwShiftInverse;
   fp fTileHeight = m_Tile.m_fThickness / fPixelLen;     // pixels deep
      // BUGFIX - fTileHeight was converted to (DWORD) - take off
   
   // recalc tiles down to be multiple of shift inverse
   DWORD dwRealDown;
   dwRealDown =  ((dwTilesDown + dwShiftInverse - 1) / dwShiftInverse) * dwShiftInverse;

   // how high
   DWORD dwX, dwY, dwScale;
   dwX = dwTilesAcross * (dwXTile + dwGrout);
   dwY = dwRealDown * (dwYTile + dwGrout);
   dwScale = max(dwX, dwY);

   // create the grout
   pImage->Init (dwX, dwY, 0);   // 0 is assumed to be grout level

   fp fTileHeightMeters = fPixelLen * fTileHeight;

   // grout texture
   m_Grout.Apply (pImage, fPixelLen);

   // figure out cutout for corner tile
   fp fCutoutMain;
   fCutoutMain = 0;
   if (m_fCornerSize) {
      // cut out of main
      fCutoutMain = m_fCornerSize / 2 + m_fSpacing; // length from center of corner tile to where main tile starts
      fCutoutMain *= sqrt((fp)2);  // amount to cout out of main tile + 1/2 grout
      fCutoutMain -= m_fSpacing/2;  // amount to cut out of main tile
      fCutoutMain = max(0,fCutoutMain);

      // set size of corner tile
      m_CornerTile.m_fWidth = m_CornerTile.m_fHeight = m_fCornerSize * sqrt((fp)2);

      // corner cutouts of corner tile
      m_CornerTile.m_fDiagCorner = m_CornerTile.m_fWidth / 2;
   }

   // do the tiles
   DWORD h, v;
   DWORD dwc;
   dwc = 0;
   for (v = 0; v < dwRealDown; v++) for (h = 0; h < dwTilesAcross; h++) {
      // position that want
      int iX, iY;
      iX = (v + h * dwShiftInverse) * (dwXTile + dwGrout) / dwShiftInverse + dwGrout/2;
      iY = v * (dwYTile + dwGrout) + dwGrout/2;

      // alternate?
      BOOL fAlt;
      fAlt = m_fAltColor && (dwc%2);

      COLORREF cOldA, cOldB;
      if (fAlt) {
         cOldA = m_Tile.m_cSurfA;
         cOldB = m_Tile.m_cSurfB;
         m_Tile.m_cSurfA = m_cAltA;
         m_Tile.m_cSurfB = m_cAltB;
      }
      fp fOldDiag;
      if (m_fCornerSize) {
         fOldDiag = m_Tile.m_fDiagCorner;
         m_Tile.m_fDiagCorner = fCutoutMain;
      }
      // draw the tile
      m_Tile.Apply(pImage, fPixelLen, iX, iY);
      if (m_fCornerSize)
         m_Tile.m_fDiagCorner = fOldDiag;
      if (fAlt) {
         m_Tile.m_cSurfA = cOldA;
         m_Tile.m_cSurfB = cOldB;
      }
      dwc++;

      // draw the corner tile?
      if (m_fCornerSize) {
         iX -= (int) (m_CornerTile.m_fWidth / 2 / fPixelLen) + (int) dwGrout/2;
         iY -= (int) (m_CornerTile.m_fHeight / 2 / fPixelLen) + (int) dwGrout/2;

         m_CornerTile.Apply (pImage, fPixelLen, iX, iY);
      }
   }


   // apply dirt
   m_DirtPaint.Apply (pImage, fPixelLen);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = TRUE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}




/***********************************************************************************
CTextCreatorStonesStacked - Draw Stones.
*/

class CTextCreatorStonesStacked : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorStonesStacked (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of Stone - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fWidth;    // width in meters
   fp               m_fHeight;   // height in emters of pattern
   fp               m_fVarWidth; // variability in width, 0..1
   fp               m_fVarHeight;   // variability in height, 0 ..1
   DWORD            m_dwStonesAcrossMin; // number of Stones across in pattern, 1+
   DWORD            m_dwStonesAcrossMax; // number of Stones across in pattern, 1+
   DWORD            m_dwStonesDown;   // number of Stones down in pattern, 1+
   fp               m_fSpacing;   // spacing (in meters) between Stones
   CTextEffectGrout  m_Grout; // grout information
   CTextEffectMakeStone m_Stone; // Stone color
   CTextEffectDirtPaint m_DirtPaint;  // dirt or paint to add
};
typedef CTextCreatorStonesStacked *PCTextCreatorStonesStacked;



/****************************************************************************
StonesPage
*/
BOOL StonesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorStonesStacked pv = (PCTextCreatorStonesStacked) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         DoubleToControl (pPage, L"Stonesacrossmin", pv->m_dwStonesAcrossMin);
         DoubleToControl (pPage, L"Stonesacrossmax", pv->m_dwStonesAcrossMax);
         DoubleToControl (pPage, L"Stonesdown", pv->m_dwStonesDown);
         MeasureToString (pPage, L"spacing", pv->m_fSpacing, TRUE);
         MeasureToString (pPage, L"width", pv->m_fWidth, TRUE);
         MeasureToString (pPage, L"height", pv->m_fHeight, TRUE);
         pControl = pPage->ControlFind (L"varwidth");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarWidth * 100));
         pControl = pPage->ControlFind (L"varheight");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarHeight * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);


         pv->m_dwStonesAcrossMin = (DWORD)DoubleFromControl (pPage, L"Stonesacrossmin");
         pv->m_dwStonesAcrossMin = max(1,pv->m_dwStonesAcrossMin);
         pv->m_dwStonesAcrossMax = (DWORD)DoubleFromControl (pPage, L"Stonesacrossmax");
         pv->m_dwStonesAcrossMax = max(1,pv->m_dwStonesAcrossMax);
         pv->m_dwStonesDown = (DWORD) DoubleFromControl (pPage, L"Stonesdown");
         pv->m_dwStonesDown = max(1,pv->m_dwStonesDown);
         MeasureParseString (pPage, L"spacing", &pv->m_fSpacing);
         pv->m_fSpacing = max(0,pv->m_fSpacing);
         MeasureParseString (pPage, L"width", &pv->m_fWidth);
         pv->m_fWidth = max(0.01,pv->m_fWidth);
         MeasureParseString (pPage, L"height", &pv->m_fHeight);
         pv->m_fHeight = max(0.01,pv->m_fHeight);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"varwidth");
         if (pControl)
            pv->m_fVarWidth = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"varheight");
         if (pControl)
            pv->m_fVarHeight = (fp) pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Stone wall";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorStonesStacked::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURESTONES, StonesPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"Stone")) {
      if (m_Stone.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"grout")) {
      if (m_Grout.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}


CTextCreatorStonesStacked::CTextCreatorStonesStacked (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = 0;
   m_dwStonesAcrossMin = m_dwStonesAcrossMax = m_dwStonesDown = 0;
   m_fSpacing = 0;
   m_Material.InitFromID (MATERIAL_TILEMATTE);

   m_fWidth = m_fHeight = .5;
   m_fVarWidth = .1;
   m_fVarHeight = .1;
   m_dwStonesAcrossMin = 3;
   m_dwStonesAcrossMax = 5;
   m_iSeed = 24354;
   m_fPixelLen = 0.005;
   m_fSpacing = 0.005;
   m_dwStonesDown = 5;
   m_Stone.m_fThickness = 0.005;
   m_Stone.m_fRockLR = .001;
   m_Stone.m_fRockTB = .001;
   m_Stone.m_cSurfA = RGB(0x40,0x40,0x40);
   m_Stone.m_cSurfB = RGB(0x80,0x80,0x80);
   m_Stone.m_wSpecReflection = 0x4000;
   m_Stone.m_wSpecDirection = 4000;

   m_Stone.m_aNoise[0].m_fTurnOn = FALSE;
   m_Stone.m_aNoise[0].m_fNoiseX = .1;
   m_Stone.m_aNoise[0].m_fNoiseY = .1;
   m_Stone.m_aNoise[0].m_cMax = RGB(0x80, 0xff, 0x80);
   m_Stone.m_aNoise[0].m_cMin = RGB(0xff, 0x80, 0x80);
   m_Stone.m_aNoise[0].m_fTransMax = .6;
   m_Stone.m_aNoise[0].m_fTransMin = 1;

   m_Stone.m_aNoise[1].m_fTurnOn = FALSE;
   m_Stone.m_aNoise[1].m_fNoiseX = .025;
   m_Stone.m_aNoise[1].m_fNoiseY = .025;
   m_Stone.m_aNoise[1].m_cMax = RGB(0x80, 0x80, 0xff);
   m_Stone.m_aNoise[1].m_cMin = RGB(0x80, 0xff, 0x80);
   m_Stone.m_aNoise[1].m_fTransMax = .6;
   m_Stone.m_aNoise[1].m_fTransMin = 1;

   m_Stone.m_aNoise[2].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[2].m_fNoiseX = .1;
   m_Stone.m_aNoise[2].m_fNoiseY = .1;
   m_Stone.m_aNoise[2].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[2].m_fZDeltaMin = .001;
   m_Stone.m_aNoise[2].m_fTransMax = 1;
   m_Stone.m_aNoise[2].m_fTransMin = 1;

   m_Stone.m_aNoise[3].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[3].m_fNoiseX = .05;
   m_Stone.m_aNoise[3].m_fNoiseY = .05;
   m_Stone.m_aNoise[3].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[3].m_fZDeltaMin = .001;
   m_Stone.m_aNoise[3].m_fTransMax = 1;
   m_Stone.m_aNoise[3].m_fTransMin = 1;

   m_Stone.m_aNoise[4].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[4].m_fNoiseX = .01;
   m_Stone.m_aNoise[4].m_fNoiseY = .01;
   m_Stone.m_aNoise[4].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[4].m_fZDeltaMin = .0005;
   m_Stone.m_aNoise[4].m_fTransMax = 1;
   m_Stone.m_aNoise[4].m_fTransMin = 1;

   m_Grout.m_cColor = RGB(0xf0,0xf0,0xf0);
   m_Grout.m_aNoise[0].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[0].m_fNoiseX = .1;
   m_Grout.m_aNoise[0].m_fNoiseY = .1;
   m_Grout.m_aNoise[0].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[0].m_fZDeltaMin = -.003;
   m_Grout.m_aNoise[0].m_fTransMax = 1;
   m_Grout.m_aNoise[0].m_fTransMin = 1;

   m_Grout.m_aNoise[1].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[1].m_fNoiseX = .025;
   m_Grout.m_aNoise[1].m_fNoiseY = .025;
   m_Grout.m_aNoise[1].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[1].m_fZDeltaMin = -.001;
   m_Grout.m_aNoise[1].m_fTransMax = 1;
   m_Grout.m_aNoise[1].m_fTransMin = 1;

   m_DirtPaint.m_fTurnOn = FALSE;
   m_DirtPaint.m_cColor = RGB(0x60,0x60,0);
   m_DirtPaint.m_fAHeight = .003;
   m_DirtPaint.m_fFilterSize = .01;
   m_DirtPaint.m_fTransAtA = .25;
   m_DirtPaint.m_fTransNone = 1;
   m_DirtPaint.m_wSpecDirection = 100;
   m_DirtPaint.m_wSpecReflection = 0;

}

void CTextCreatorStonesStacked::Delete (void)
{
   delete this;
}

static PWSTR gpszStones = L"Stones";
static PWSTR gpszStonesAcrossMin = L"StonesAcrossMin";
static PWSTR gpszStonesAcrossMax = L"StonesAcrossMax";
static PWSTR gpszStonesDown = L"StonesDown";
static PWSTR gpszVarWidth = L"VarWidth";
static PWSTR gpszVarHeight = L"VarHeight";

PCMMLNode2 CTextCreatorStonesStacked::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszStones);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszSeed, (int) m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszStonesAcrossMax, (int)m_dwStonesAcrossMax);
   MMLValueSet (pNode, gpszStonesAcrossMin, (int)m_dwStonesAcrossMin);
   MMLValueSet (pNode, gpszStonesDown, (int)m_dwStonesDown);
   MMLValueSet (pNode, gpszSpacing, m_fSpacing);
   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszVarWidth, m_fVarWidth);
   MMLValueSet (pNode, gpszVarHeight, m_fVarHeight);

   PCMMLNode2 pSub;
   pSub = m_Grout.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGrout);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Stone.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMakeStone);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorStonesStacked::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int) 0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_dwStonesAcrossMin = (DWORD) MMLValueGetInt (pNode, gpszStonesAcrossMin, (int)1);
   m_dwStonesAcrossMax = (DWORD) MMLValueGetInt (pNode, gpszStonesAcrossMax, (int)1);
   m_dwStonesDown = (DWORD) MMLValueGetInt (pNode, gpszStonesDown, (int)0);
   m_fSpacing = MMLValueGetDouble (pNode, gpszSpacing, 0);

   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, .1);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, .1);
   m_fVarWidth = MMLValueGetDouble (pNode, gpszVarWidth, 0);
   m_fVarHeight = MMLValueGetDouble (pNode, gpszVarHeight, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGrout), &psz, &pSub);
   if (pSub)
      m_Grout.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszMakeStone), &psz, &pSub);
   if (pSub)
      m_Stone.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);

   return TRUE;
}

BOOL CTextCreatorStonesStacked::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // pixels per Stone
   fp fGrout;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   fGrout = (m_fSpacing / fPixelLen);
   fp fStoneHeight = m_Stone.m_fThickness / fPixelLen;     // pixels deep
      // BUGFIX - fStoneHeight was converted to (DWORD) - take off
   
   // how high
   DWORD dwX, dwY, dwScale;
   dwX = max((DWORD) ((m_fWidth + fPixelLen/2) / fPixelLen), 1);
   dwY = max((DWORD) ((m_fHeight + fPixelLen/2) / fPixelLen), 1);
   dwScale = max(dwX, dwY);

   // create the grout
   pImage->Init (dwX, dwY, 0);   // 0 is assumed to be grout level

   // grout texture
   m_Grout.Apply (pImage, fPixelLen);

   // figure out where each row of stones go
   CListFixed lRow;
   DWORD i;
   fp fLoc;
   fp *pfRow;
   lRow.Init (sizeof(fp));
   lRow.Required (m_dwStonesDown);
   for (i = 0; i < m_dwStonesDown; i++) {
      fLoc = (fp)i / (fp) m_dwStonesDown;
      fLoc += randf (m_fVarHeight / 2.01 / (fp) m_dwStonesDown,
         -m_fVarHeight / 2.01 / (fp)m_dwStonesDown);
      fLoc *= (fp)dwY;
      lRow.Add (&fLoc);
   }
   pfRow = (fp*) lRow.Get(0);


   // do the Stones
   DWORD h, v;
   CListFixed lColumn;
   lColumn.Init (sizeof(fp));
   for (v = 0; v < m_dwStonesDown; v++) {
      // figure out where columns go
      DWORD dwNumC;
      if (m_dwStonesAcrossMax >= m_dwStonesAcrossMin)
         dwNumC = (rand() % (m_dwStonesAcrossMax - m_dwStonesAcrossMin + 1)) + m_dwStonesAcrossMin;
      else
         dwNumC = m_dwStonesAcrossMin;

      lColumn.Clear();
      lColumn.Required (dwNumC);
      for (i = 0; i < dwNumC; i++) {
         fLoc = (fp) i / (fp) dwNumC;
         if (v % 2)
            fLoc += .5 / (fp) dwNumC;  // alternate
         fLoc += randf (m_fVarWidth / 2.01 / (fp) dwNumC,
            -m_fVarWidth / 2.01 / (fp)dwNumC);
         fLoc *= (fp)dwX;
         lColumn.Add (&fLoc);
      }

      // have column locations
      fp *pfCol;
      pfCol = (fp*) lColumn.Get(0);

      for (h = 0; h < dwNumC; h++) {
         TEXTUREPOINT ap[4];
         ap[0].h = pfCol[h] + fGrout/2;
         ap[0].v = pfRow[v] + fGrout/2;
         ap[1] = ap[0];
         ap[1].h = ((h+1 < dwNumC) ? pfCol[h+1] : (pfCol[0] + dwX)) - fGrout/2;
         ap[2] = ap[1];
         ap[2].v = ((v+1 < m_dwStonesDown) ? pfRow[v+1] : (pfRow[0] + dwY)) - fGrout/2;
         ap[3] = ap[2];
         ap[3].h = pfCol[h] + fGrout/2;

         m_Stone.Apply (pImage, fPixelLen, 4, ap);
      }
   }


   // apply dirt
   m_DirtPaint.Apply (pImage, fPixelLen);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = TRUE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}





/***********************************************************************************
CTextCreatorStonesRandom - Draw Stones.
*/

class CTextCreatorStonesRandom : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorStonesRandom (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of Stone - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fWidth;    // width in meters
   fp               m_fHeight;   // height in emters of pattern
   fp               m_fVarWidth; // variability in width, 0..1
   fp               m_fVarHeight;   // variability in height, 0 ..1
   BOOL             m_fPebbles;     // stone on stone
   DWORD            m_dwStonesAcross; // number of Stones across in pattern, 1+
   DWORD            m_dwStonesDown;   // number of Stones down in pattern, 1+
   fp               m_fSpacing;   // spacing (in meters) between Stones
   CTextEffectGrout  m_Grout; // grout information
   CTextEffectMakeStone m_Stone; // Stone color
   CTextEffectDirtPaint m_DirtPaint;  // dirt or paint to add
};
typedef CTextCreatorStonesRandom *PCTextCreatorStonesRandom;



/****************************************************************************
StonesRandomPage
*/
BOOL StonesRandomPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorStonesRandom pv = (PCTextCreatorStonesRandom) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         DoubleToControl (pPage, L"Stonesacross", pv->m_dwStonesAcross);
         DoubleToControl (pPage, L"Stonesdown", pv->m_dwStonesDown);
         MeasureToString (pPage, L"spacing", pv->m_fSpacing, TRUE);
         MeasureToString (pPage, L"width", pv->m_fWidth, TRUE);
         MeasureToString (pPage, L"height", pv->m_fHeight, TRUE);
         pControl = pPage->ControlFind (L"varwidth");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarWidth * 100));
         pControl = pPage->ControlFind (L"varheight");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarHeight * 100));
         pControl = pPage->ControlFind (L"pebbles");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fPebbles);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);


         pv->m_dwStonesAcross = (DWORD)DoubleFromControl (pPage, L"Stonesacross");
         pv->m_dwStonesAcross = max(1,pv->m_dwStonesAcross);
         pv->m_dwStonesDown = (DWORD) DoubleFromControl (pPage, L"Stonesdown");
         pv->m_dwStonesDown = max(1,pv->m_dwStonesDown);
         MeasureParseString (pPage, L"spacing", &pv->m_fSpacing);
         pv->m_fSpacing = max(0,pv->m_fSpacing);
         MeasureParseString (pPage, L"width", &pv->m_fWidth);
         pv->m_fWidth = max(0.01,pv->m_fWidth);
         MeasureParseString (pPage, L"height", &pv->m_fHeight);
         pv->m_fHeight = max(0.01,pv->m_fHeight);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"varwidth");
         if (pControl)
            pv->m_fVarWidth = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"varheight");
         if (pControl)
            pv->m_fVarHeight = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"pebbles");
         if (pControl)
            pv->m_fPebbles = (BOOL) pControl->AttribGetBOOL (Checked());
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Stone wall";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorStonesRandom::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURESTONESRANDOM, StonesRandomPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"Stone")) {
      if (m_Stone.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"grout")) {
      if (m_Grout.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}


CTextCreatorStonesRandom::CTextCreatorStonesRandom (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = 0;
   m_dwStonesAcross = m_dwStonesDown = 0;
   m_fSpacing = 0;
   m_Material.InitFromID (MATERIAL_TILEMATTE);

   m_fWidth = m_fHeight = .5;
   m_fVarWidth = .1;
   m_fVarHeight = .1;
   m_fPebbles = FALSE;
   m_dwStonesAcross = 5;
   m_iSeed = 24354;
   m_fPixelLen = 0.005;
   m_fSpacing = 0.005;
   m_dwStonesDown = 5;
   m_Stone.m_fThickness = 0.005;
   m_Stone.m_fRockLR = .001;
   m_Stone.m_fRockTB = .001;
   m_Stone.m_cSurfA = RGB(0x40,0x40,0x40);
   m_Stone.m_cSurfB = RGB(0x80,0x80,0x80);
   m_Stone.m_wSpecReflection = 0x4000;
   m_Stone.m_wSpecDirection = 4000;

   m_Stone.m_aNoise[0].m_fTurnOn = FALSE;
   m_Stone.m_aNoise[0].m_fNoiseX = .1;
   m_Stone.m_aNoise[0].m_fNoiseY = .1;
   m_Stone.m_aNoise[0].m_cMax = RGB(0x80, 0xff, 0x80);
   m_Stone.m_aNoise[0].m_cMin = RGB(0xff, 0x80, 0x80);
   m_Stone.m_aNoise[0].m_fTransMax = .6;
   m_Stone.m_aNoise[0].m_fTransMin = 1;

   m_Stone.m_aNoise[1].m_fTurnOn = FALSE;
   m_Stone.m_aNoise[1].m_fNoiseX = .025;
   m_Stone.m_aNoise[1].m_fNoiseY = .025;
   m_Stone.m_aNoise[1].m_cMax = RGB(0x80, 0x80, 0xff);
   m_Stone.m_aNoise[1].m_cMin = RGB(0x80, 0xff, 0x80);
   m_Stone.m_aNoise[1].m_fTransMax = .6;
   m_Stone.m_aNoise[1].m_fTransMin = 1;

   m_Stone.m_aNoise[2].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[2].m_fNoiseX = .1;
   m_Stone.m_aNoise[2].m_fNoiseY = .1;
   m_Stone.m_aNoise[2].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[2].m_fZDeltaMin = .001;
   m_Stone.m_aNoise[2].m_fTransMax = 1;
   m_Stone.m_aNoise[2].m_fTransMin = 1;

   m_Stone.m_aNoise[3].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[3].m_fNoiseX = .05;
   m_Stone.m_aNoise[3].m_fNoiseY = .05;
   m_Stone.m_aNoise[3].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[3].m_fZDeltaMin = .001;
   m_Stone.m_aNoise[3].m_fTransMax = 1;
   m_Stone.m_aNoise[3].m_fTransMin = 1;

   m_Stone.m_aNoise[4].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[4].m_fNoiseX = .01;
   m_Stone.m_aNoise[4].m_fNoiseY = .01;
   m_Stone.m_aNoise[4].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[4].m_fZDeltaMin = .0005;
   m_Stone.m_aNoise[4].m_fTransMax = 1;
   m_Stone.m_aNoise[4].m_fTransMin = 1;

   m_Grout.m_cColor = RGB(0xf0,0xf0,0xf0);
   m_Grout.m_aNoise[0].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[0].m_fNoiseX = .1;
   m_Grout.m_aNoise[0].m_fNoiseY = .1;
   m_Grout.m_aNoise[0].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[0].m_fZDeltaMin = -.003;
   m_Grout.m_aNoise[0].m_fTransMax = 1;
   m_Grout.m_aNoise[0].m_fTransMin = 1;

   m_Grout.m_aNoise[1].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[1].m_fNoiseX = .025;
   m_Grout.m_aNoise[1].m_fNoiseY = .025;
   m_Grout.m_aNoise[1].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[1].m_fZDeltaMin = -.001;
   m_Grout.m_aNoise[1].m_fTransMax = 1;
   m_Grout.m_aNoise[1].m_fTransMin = 1;

   m_DirtPaint.m_fTurnOn = FALSE;
   m_DirtPaint.m_cColor = RGB(0x60,0x60,0);
   m_DirtPaint.m_fAHeight = .003;
   m_DirtPaint.m_fFilterSize = .01;
   m_DirtPaint.m_fTransAtA = .25;
   m_DirtPaint.m_fTransNone = 1;
   m_DirtPaint.m_wSpecDirection = 100;
   m_DirtPaint.m_wSpecReflection = 0;

}

void CTextCreatorStonesRandom::Delete (void)
{
   delete this;
}

static PWSTR gpszStonesAcross = L"StonesAcross";
static PWSTR gpszPebbles = L"Pebbles";

PCMMLNode2 CTextCreatorStonesRandom::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszStones);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszSeed, (int) m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszStonesAcross, (int)m_dwStonesAcross);
   MMLValueSet (pNode, gpszStonesDown, (int)m_dwStonesDown);
   MMLValueSet (pNode, gpszSpacing, m_fSpacing);
   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszVarWidth, m_fVarWidth);
   MMLValueSet (pNode, gpszVarHeight, m_fVarHeight);
   MMLValueSet (pNode, gpszPebbles, (int) m_fPebbles);

   PCMMLNode2 pSub;
   pSub = m_Grout.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGrout);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Stone.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMakeStone);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorStonesRandom::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int) 0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_dwStonesAcross = (DWORD) MMLValueGetInt (pNode, gpszStonesAcross, (int)1);
   m_dwStonesDown = (DWORD) MMLValueGetInt (pNode, gpszStonesDown, (int)0);
   m_fSpacing = MMLValueGetDouble (pNode, gpszSpacing, 0);

   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, .1);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, .1);
   m_fVarWidth = MMLValueGetDouble (pNode, gpszVarWidth, 0);
   m_fVarHeight = MMLValueGetDouble (pNode, gpszVarHeight, 0);
   m_fPebbles = (BOOL) MMLValueGetInt (pNode, gpszPebbles, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGrout), &psz, &pSub);
   if (pSub)
      m_Grout.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszMakeStone), &psz, &pSub);
   if (pSub)
      m_Stone.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);

   return TRUE;
}

BOOL CTextCreatorStonesRandom::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // pixels per Stone
   fp fGrout;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   fGrout = (m_fSpacing / fPixelLen);
   fp fStoneHeight = m_Stone.m_fThickness / fPixelLen;     // pixels deep
      // BUGFIX - fStoneHeight was converted to (DWORD) - take off
   
   // how high
   DWORD dwX, dwY, dwScale;
   dwX = max((DWORD) ((m_fWidth + fPixelLen/2) / fPixelLen), 1);
   dwY = max((DWORD) ((m_fHeight + fPixelLen/2) / fPixelLen), 1);
   dwScale = max(dwX, dwY);

   // create the grout
   pImage->Init (dwX, dwY, 0);   // 0 is assumed to be grout level

   // grout texture
   m_Grout.Apply (pImage, fPixelLen);

   // figure out where each stone corner goes
   CMem  mem;
   if (!mem.Required (m_dwStonesAcross * m_dwStonesDown * sizeof(TEXTUREPOINT)))
      return FALSE;
   PTEXTUREPOINT pLoc;
   pLoc = (PTEXTUREPOINT) mem.p;

   DWORD dwPebble;
   for (dwPebble = 0; dwPebble < (DWORD) (m_fPebbles ? 2 : 1); dwPebble++) {
      DWORD h, v;
      PTEXTUREPOINT pCur;
      for (v = 0; v < m_dwStonesDown; v++) for (h = 0; h < m_dwStonesAcross; h++) {
         pCur = pLoc + (v * m_dwStonesAcross + h);

         pCur->h = (fp) h / (fp) m_dwStonesAcross;
         if (dwPebble)
            pCur->h += .5 / (fp) m_dwStonesAcross;
         pCur->h += randf (m_fVarWidth / 2.01 / (fp) m_dwStonesAcross,
            -m_fVarWidth / 2.01 / (fp)m_dwStonesAcross);
         pCur->h *= (fp)dwX;

         pCur->v = (fp)v / (fp) m_dwStonesDown;
         if (dwPebble)
            pCur->v += .5 / (fp) m_dwStonesDown;
         pCur->v += randf (m_fVarHeight / 2.01 / (fp) m_dwStonesDown,
            -m_fVarHeight / 2.01 / (fp)m_dwStonesDown);
         pCur->v *= (fp)dwY;
      }

      // do the Stones
      TEXTUREPOINT ap[4];
      for (v = 0; v < m_dwStonesDown; v++) {
         for (h = 0; h < m_dwStonesAcross; h++) {
            ap[0] = pLoc[(v) * m_dwStonesAcross + h];
            if (h+1 < m_dwStonesAcross)
               ap[1] = pLoc[(v) * m_dwStonesAcross + h+1];
            else {
               ap[1] = pLoc[(v) * m_dwStonesAcross + 0];
               ap[1].h += dwX;
            }

            if ((h+1 < m_dwStonesAcross) && (v+1 < m_dwStonesDown))
               ap[2] = pLoc[(v+1) * m_dwStonesAcross + h+1];
            else if ((h+1 < m_dwStonesAcross) && (v+1 >= m_dwStonesDown)){
               ap[2] = pLoc[(0) * m_dwStonesAcross + h+1];
               ap[2].v += dwY;
            }
            else if ((h+1 >= m_dwStonesAcross) && (v+1 < m_dwStonesDown)) {
               ap[2] = pLoc[(v+1) * m_dwStonesAcross + 0];
               ap[2].h += dwX;
            }
            else {   // both beyond edge
               ap[2] = pLoc[(0) * m_dwStonesAcross + 0];
               ap[2].h += dwX;
               ap[2].v += dwY;
            }

            if (v+1 < m_dwStonesDown)
               ap[3] = pLoc[(v+1) * m_dwStonesAcross + h];
            else {
               ap[3] = pLoc[(0) * m_dwStonesAcross + h];
               ap[3].v += dwY;
            }

            // include grout
            DWORD i;
            for (i = 0; i < 4; i++) {
               ap[i].h += (((i == 0) || (i == 3)) ? (fGrout/2) : (-fGrout/2));
               ap[i].v += (((i == 0) || (i == 1)) ? (fGrout/2) : (-fGrout/2));
            }

            m_Stone.Apply (pImage, fPixelLen, 4, ap, dwPebble ? m_Stone.m_fThickness : 0);
         }
      }
   }  // dwPebble


   // apply dirt
   m_DirtPaint.Apply (pImage, fPixelLen);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = TRUE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}




/***********************************************************************************
CTextCreatorPavers - Draw Stones.
*/

class CTextCreatorPavers : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorPavers (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of Stone - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fWidth;    // width in meters
   fp               m_fHeight;   // height in emters of PAVER
   DWORD            m_dwPaversAcross; // number of Stones across in pattern, 1+
   DWORD            m_dwPaversDown;   // number of Stones down in pattern, 1+
   DWORD            m_dwShape;         // shape of the paver
   CTextEffectGrout  m_Grout; // grout information
   CTextEffectMakeStone m_Stone; // Stone color
   CTextEffectDirtPaint m_DirtPaint;  // dirt or paint to add
};
typedef CTextCreatorPavers *PCTextCreatorPavers;



/****************************************************************************
PaversPage
*/
BOOL PaversPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorPavers pv = (PCTextCreatorPavers) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         ComboBoxSet (pPage, L"shape", pv->m_dwShape);

         DoubleToControl (pPage, L"stonesAcross", pv->m_dwPaversAcross);
         DoubleToControl (pPage, L"stonesDown", pv->m_dwPaversDown);
         MeasureToString (pPage, L"width", pv->m_fWidth, TRUE);
         MeasureToString (pPage, L"height", pv->m_fHeight, TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shape")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwShape)
               return TRUE; // unchanged
            pv->m_dwShape = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);


         pv->m_dwPaversAcross = (DWORD)DoubleFromControl (pPage, L"stonesAcross");
         pv->m_dwPaversAcross = max(1,pv->m_dwPaversAcross);
         pv->m_dwPaversDown = (DWORD) DoubleFromControl (pPage, L"stonesDown");
         pv->m_dwPaversDown = max(1,pv->m_dwPaversDown);
         MeasureParseString (pPage, L"width", &pv->m_fWidth);
         pv->m_fWidth = max(0.01,pv->m_fWidth);
         MeasureParseString (pPage, L"height", &pv->m_fHeight);
         pv->m_fHeight = max(0.01,pv->m_fHeight);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pavers";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorPavers::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREPAVERS, PaversPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"Stone")) {
      if (m_Stone.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"grout")) {
      if (m_Grout.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}


CTextCreatorPavers::CTextCreatorPavers (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = 0;
   m_dwPaversAcross = m_dwPaversDown = 0;
   m_dwShape = 0;
   m_Material.InitFromID (MATERIAL_TILEMATTE);

   m_fWidth = .2;
   m_fHeight = .1;
   m_dwPaversAcross = 5;
   m_iSeed = 24354;
   m_fPixelLen = 0.005;
   m_dwPaversDown = 5;
   m_Stone.m_fThickness = 0.005;
   m_Stone.m_fRockLR = .001;
   m_Stone.m_fRockTB = .001;
   m_Stone.m_cSurfA = RGB(0x40,0x40,0x40);
   m_Stone.m_cSurfB = RGB(0x80,0x80,0x80);
   m_Stone.m_wSpecReflection = 0x4000;
   m_Stone.m_wSpecDirection = 4000;

   m_Stone.m_aNoise[0].m_fTurnOn = FALSE;
   m_Stone.m_aNoise[0].m_fNoiseX = .1;
   m_Stone.m_aNoise[0].m_fNoiseY = .1;
   m_Stone.m_aNoise[0].m_cMax = RGB(0x80, 0xff, 0x80);
   m_Stone.m_aNoise[0].m_cMin = RGB(0xff, 0x80, 0x80);
   m_Stone.m_aNoise[0].m_fTransMax = .6;
   m_Stone.m_aNoise[0].m_fTransMin = 1;

   m_Stone.m_aNoise[1].m_fTurnOn = FALSE;
   m_Stone.m_aNoise[1].m_fNoiseX = .025;
   m_Stone.m_aNoise[1].m_fNoiseY = .025;
   m_Stone.m_aNoise[1].m_cMax = RGB(0x80, 0x80, 0xff);
   m_Stone.m_aNoise[1].m_cMin = RGB(0x80, 0xff, 0x80);
   m_Stone.m_aNoise[1].m_fTransMax = .6;
   m_Stone.m_aNoise[1].m_fTransMin = 1;

   m_Stone.m_aNoise[2].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[2].m_fNoiseX = .1;
   m_Stone.m_aNoise[2].m_fNoiseY = .1;
   m_Stone.m_aNoise[2].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[2].m_fZDeltaMin = .001;
   m_Stone.m_aNoise[2].m_fTransMax = 1;
   m_Stone.m_aNoise[2].m_fTransMin = 1;

   m_Stone.m_aNoise[3].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[3].m_fNoiseX = .05;
   m_Stone.m_aNoise[3].m_fNoiseY = .05;
   m_Stone.m_aNoise[3].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[3].m_fZDeltaMin = .001;
   m_Stone.m_aNoise[3].m_fTransMax = 1;
   m_Stone.m_aNoise[3].m_fTransMin = 1;

   m_Stone.m_aNoise[4].m_fTurnOn = TRUE;
   m_Stone.m_aNoise[4].m_fNoiseX = .01;
   m_Stone.m_aNoise[4].m_fNoiseY = .01;
   m_Stone.m_aNoise[4].m_fZDeltaMax = 0;
   m_Stone.m_aNoise[4].m_fZDeltaMin = .0005;
   m_Stone.m_aNoise[4].m_fTransMax = 1;
   m_Stone.m_aNoise[4].m_fTransMin = 1;

   m_Grout.m_cColor = RGB(0xf0,0xf0,0xf0);
   m_Grout.m_aNoise[0].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[0].m_fNoiseX = .1;
   m_Grout.m_aNoise[0].m_fNoiseY = .1;
   m_Grout.m_aNoise[0].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[0].m_fZDeltaMin = -.003;
   m_Grout.m_aNoise[0].m_fTransMax = 1;
   m_Grout.m_aNoise[0].m_fTransMin = 1;

   m_Grout.m_aNoise[1].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[1].m_fNoiseX = .025;
   m_Grout.m_aNoise[1].m_fNoiseY = .025;
   m_Grout.m_aNoise[1].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[1].m_fZDeltaMin = -.001;
   m_Grout.m_aNoise[1].m_fTransMax = 1;
   m_Grout.m_aNoise[1].m_fTransMin = 1;

   m_DirtPaint.m_fTurnOn = FALSE;
   m_DirtPaint.m_cColor = RGB(0x60,0x60,0);
   m_DirtPaint.m_fAHeight = .003;
   m_DirtPaint.m_fFilterSize = .01;
   m_DirtPaint.m_fTransAtA = .25;
   m_DirtPaint.m_fTransNone = 1;
   m_DirtPaint.m_wSpecDirection = 100;
   m_DirtPaint.m_wSpecReflection = 0;

}

void CTextCreatorPavers::Delete (void)
{
   delete this;
}

static PWSTR gpszPaversAcross = L"PaversAcross";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszPaversDown = L"PaversDown";

PCMMLNode2 CTextCreatorPavers::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszStones);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszSeed, (int) m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPaversAcross, (int)m_dwPaversAcross);
   MMLValueSet (pNode, gpszShape, (int)m_dwShape);
   MMLValueSet (pNode, gpszPaversDown, (int)m_dwPaversDown);
   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszHeight, m_fHeight);

   PCMMLNode2 pSub;
   pSub = m_Grout.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGrout);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Stone.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMakeStone);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorPavers::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int) 0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_dwPaversAcross = (DWORD) MMLValueGetInt (pNode, gpszPaversAcross, (int)1);
   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, 0);
   m_dwPaversDown = (DWORD) MMLValueGetInt (pNode, gpszPaversDown, (int)0);

   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, .1);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, .1);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGrout), &psz, &pSub);
   if (pSub)
      m_Grout.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszMakeStone), &psz, &pSub);
   if (pSub)
      m_Stone.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);

   return TRUE;
}

BOOL CTextCreatorPavers::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // how big is each paver?
   DWORD dwPaverX, dwPaverY;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwPaverX = max((DWORD)((m_fWidth + fPixelLen/2) / fPixelLen), 1);
   dwPaverY = max((DWORD)((m_fHeight + fPixelLen/2) / fPixelLen), 1);

   // real number of pavers up and down depends upon type
   m_dwPaversAcross = max(1,m_dwPaversAcross);
   DWORD dwRealDown;
   fp fDown, fOver;
   fDown = fOver = 1.0;
   dwRealDown = m_dwPaversDown;
   switch (m_dwShape) {
   default:
   case 0:  // angled brick
      dwRealDown = (dwRealDown+1) & (~0x1);  // even
      fOver = .5;
      break;
   case 1:  // hexagon
      dwRealDown = (dwRealDown+1) & (~0x1);  // even
      fDown = .75;
      fOver = .5;
      break;
   case 2:  // hexagon, tripple
      dwRealDown = (dwRealDown+1) & (~0x1);  // even
      fDown = .75;
      fOver = -1.5 / 3;
      break;
   case 3:  // brick, I
      dwRealDown = (dwRealDown+1) & (~0x1);  // even
      fOver = .5;
      break;
   case 4:  // cross, open
      break;
   case 5:  // cross, interlocked
      dwRealDown = (dwRealDown+1) & (~0x1);  // even
      fOver = .5;
      fDown = .5 + .25;
      break;
   }

   // pixels per Stone
   fp fGrout;
   fGrout = 0;
   fp fStoneHeight = m_Stone.m_fThickness / fPixelLen;     // pixels deep
      // BUGFIX - fStoneHeight was converted to (DWORD) - take off
   
   // how high
   DWORD dwX, dwY;
   dwX = m_dwPaversAcross * dwPaverX;
   dwY = (DWORD) (dwRealDown * dwPaverY * fDown);

   // create the grout
   pImage->Init (dwX, dwY, 0);   // 0 is assumed to be grout level

   // grout texture
   m_Grout.Apply (pImage, fPixelLen);

   DWORD h, v;
   // do the Stones
   TEXTUREPOINT ap[14];
   TEXTUREPOINT tpFirstUL, tpUL;
   tpFirstUL.h = tpFirstUL.v = 0;
   DWORD dwNum;
   for (v = 0; v < dwRealDown; v++) {
      tpUL = tpFirstUL;
      for (h = 0; h < m_dwPaversAcross; h++, tpUL.h += dwPaverX) {

         dwNum = 0;

         switch (m_dwShape) {
         default:
         case 0:  // angled paver
            // top row
            ap[dwNum] = tpUL;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 4.0;
            ap[dwNum].v -= dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 2.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 4.0;
            ap[dwNum].v -= dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            dwNum++;

            // right side
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 1.05;
            ap[dwNum].v += dwPaverY * .45;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * .95;
            ap[dwNum].v += dwPaverY * .55;
            dwNum++;

            // bottom row
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            ap[dwNum].v += dwPaverY;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 4.0;
            ap[dwNum].v += dwPaverY - dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 2.0;
            ap[dwNum].v += dwPaverY;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 4.0;
            ap[dwNum].v += dwPaverY - dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].v += dwPaverY;
            dwNum++;

            // left side
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * -.05;
            ap[dwNum].v += dwPaverY * .55;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 0.05;
            ap[dwNum].v += dwPaverY * .45;
            dwNum++;

            break;

         case 1:  // hexagon
            for (dwNum = 0; dwNum < 6; dwNum++) {
               ap[dwNum] = tpUL;
               ap[dwNum].h += dwPaverX / 2.0;
               ap[dwNum].v += dwPaverY / 2.0;
               ap[dwNum].h += cos ((fp)(dwNum+.5) / 6.0 * 2.0 * PI) / sqrt(.75) * (fp) dwPaverX / 2.0;
               ap[dwNum].v += sin ((fp)(dwNum+.5) / 6.0 * 2.0 * PI) * (fp) dwPaverY / 2.0;
            }
            break;
         case 2:  // hexagon, tripple
            // far left hexagon
            ap[dwNum] = tpUL;
            ap[dwNum].v += dwPaverY * 3.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h -= dwPaverX / 6.0;
            ap[dwNum].v += dwPaverY / 2.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].v += dwPaverY * 1.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 3.0;
            ap[dwNum].v += dwPaverY * 1.0 / 4.0;
            dwNum++;

            // top hexagon
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 2.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 5.0 / 6.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            ap[dwNum].v += dwPaverY * 1.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 5.0 / 6.0;
            ap[dwNum].v += dwPaverY / 2.0;
            dwNum++;

            // bottom hexagon
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            ap[dwNum].v += dwPaverY * 3.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 5.0 / 6.0;
            ap[dwNum].v += dwPaverY;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 2.0;
            ap[dwNum].v += dwPaverY;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX / 3.0;
            ap[dwNum].v += dwPaverY * 3.0 / 4.0;
            dwNum++;
            break;
         case 3:  // brick, I
            // top
            ap[dwNum] = tpUL;
            ap[dwNum].v -= dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 16.0;
            ap[dwNum].v -= dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 5.0 / 16.0;
            ap[dwNum].v += dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 11.0 / 16.0;
            ap[dwNum].v += dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 13.0 / 16.0;
            ap[dwNum].v -= dwPaverY / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            ap[dwNum].v -= dwPaverY / 8.0;
            dwNum++;

            // bottom
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            ap[dwNum].v += dwPaverY * 9.0 / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 13.0 / 16.0;
            ap[dwNum].v += dwPaverY * 9.0 / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 11.0 / 16.0;
            ap[dwNum].v += dwPaverY * 7.0 / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 5.0 / 16.0;
            ap[dwNum].v += dwPaverY * 7.0 / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 16.0;
            ap[dwNum].v += dwPaverY * 9.0 / 8.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].v += dwPaverY * 9.0 / 8.0;
            dwNum++;
            break;
         case 4:  // cross, open
         case 5:  // cross, interlocked
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 1.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 4.0;
            ap[dwNum].v += dwPaverY * 1.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            ap[dwNum].v += dwPaverY * 1.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX;
            ap[dwNum].v += dwPaverY * 3.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 4.0;
            ap[dwNum].v += dwPaverY * 3.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 3.0 / 4.0;
            ap[dwNum].v += dwPaverY;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 1.0 / 4.0;
            ap[dwNum].v += dwPaverY;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 1.0 / 4.0;
            ap[dwNum].v += dwPaverY * 3.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].v += dwPaverY * 3.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].v += dwPaverY * 1.0 / 4.0;
            dwNum++;
            ap[dwNum] = tpUL;
            ap[dwNum].h += dwPaverX * 1.0 / 4.0;
            ap[dwNum].v += dwPaverY * 1.0 / 4.0;
            dwNum++;
            break;
         }

         m_Stone.Apply (pImage, fPixelLen, dwNum, ap);
      }

      tpFirstUL.h += fOver * (fp) dwPaverX;
      tpFirstUL.v += fDown * (fp) dwPaverY;
   }


   // apply dirt
   m_DirtPaint.Apply (pImage, fPixelLen);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = TRUE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}


/***********************************************************************************
CTextCreatorParquet - Draw tiles.
*/

class CTextCreatorParquet : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorParquet (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   DWORD            m_dwTileGroups; // number of tiles groups across and down, 1+
   DWORD            m_dwPattern; // 0 for parquet, 1 for herringbone, 2 for box
   BOOL             m_fUseTiles; // if TRUE use the tiles, FALSE use the planks
   fp               m_fSizeLong; // length of the long-end of tile piece
   DWORD            m_dwSizeShort;  // short end is this fraction 1/2, etc. of long end

   fp               m_fSpacing;   // spacing (in meters) between tiles
   BOOL             m_fAltColor;  // if TRUE then alternte tile colors using cAltA and cAltB

   // for tiles
   COLORREF         m_cAltA;   // alternatining color, ranges from cAltA to cAltB
   COLORREF         m_cAltB;

   // for planks - alternate brightness
   fp                m_fBrightMin;       // apply darker/lighter across length for variation. 1.0 = normal brightness
   fp                m_fBrightMax;       // 1.0 = normal bright

   CTextEffectGrout  m_Grout; // grout information
   CTextEffectMakeTile m_Tile; // tile color
   CTextEffectGeneratePlank m_Plank;   // description of a plank
   CTextEffectDirtPaint m_DirtPaint;  // dirt or paint to add
};

typedef CTextCreatorParquet *PCTextCreatorParquet;



/****************************************************************************
ParquetPage
*/
BOOL ParquetPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorParquet pv = (PCTextCreatorParquet) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         DoubleToControl (pPage, L"tilegroups", pv->m_dwTileGroups);
         DoubleToControl (pPage, L"sizeshort", pv->m_dwSizeShort);
         MeasureToString (pPage, L"spacing", pv->m_fSpacing, TRUE);
         MeasureToString (pPage, L"sizelong", pv->m_fSizeLong, TRUE);

         ComboBoxSet (pPage, L"pattern", pv->m_dwPattern);

         pControl = pPage->ControlFind (L"altcolor");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fAltColor);
         pControl = pPage->ControlFind (L"usetiles");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fUseTiles);
         FillStatusColor (pPage, L"altacolor", pv->m_cAltA);
         FillStatusColor (pPage, L"altbcolor", pv->m_cAltB);

         pControl = pPage->ControlFind (L"brightmin");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBrightMin * 100));
         pControl = pPage->ControlFind (L"brightmax");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBrightMax * 100));

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"altabutton")) {
            pv->m_cAltA = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cAltA, pPage, L"altacolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"altbbutton")) {
            pv->m_cAltB = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cAltB, pPage, L"altbcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"pattern")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwPattern)
               break; // unchanged
            pv->m_dwPattern = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         pv->m_dwTileGroups = (DWORD)DoubleFromControl (pPage, L"tilegroups");
         pv->m_dwTileGroups = max(1,pv->m_dwTileGroups);
         pv->m_dwSizeShort = (DWORD) DoubleFromControl (pPage, L"sizeshort");
         pv->m_dwSizeShort = max(1,pv->m_dwSizeShort);
         MeasureParseString (pPage, L"spacing", &pv->m_fSpacing);
         pv->m_fSpacing = max(0,pv->m_fSpacing);
         MeasureParseString (pPage, L"sizelong", &pv->m_fSizeLong);
         pv->m_fSizeLong = max(0,pv->m_fSizeLong);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"altcolor");
         if (pControl)
            pv->m_fAltColor = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"usetiles");
         if (pControl)
            pv->m_fUseTiles = pControl->AttribGetBOOL (Checked());

         pControl = pPage->ControlFind (L"brightmin");
         if (pControl)
            pv->m_fBrightMin = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"brightmax");
         if (pControl)
            pv->m_fBrightMax = (fp) pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Parquet tiles";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorParquet::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREPARQUET, ParquetPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"tile")) {
      if (m_Tile.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"grout")) {
      if (m_Grout.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"plank")) {
      if (m_Plank.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}


CTextCreatorParquet::CTextCreatorParquet (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = 0;
   m_fSpacing = 0;
   m_fAltColor = FALSE;
   m_cAltA = m_cAltB = 0;
   m_Material.InitFromID (MATERIAL_PAINTGLOSS);
   m_dwTileGroups = 3;
   m_dwPattern = 0;
   m_fUseTiles = FALSE;
   m_fSizeLong = .15;
   m_dwSizeShort = 4;
   m_fBrightMin = m_fBrightMax = 1;

   m_iSeed = 24354;
   m_fPixelLen = 0.005;
   m_cAltA = RGB(0xff,00,00);
   m_cAltB = RGB(0xc0,00,00);
   m_Tile.m_fBevel = 0.01;
   m_Tile.m_fThickness = 0.005;
   m_Tile.m_fHeight = .30;
   m_Tile.m_fWidth = .30;
   m_Tile.m_fRockLR = .001;
   m_Tile.m_fRockTB = .001;
   m_Tile.m_cSurfA = RGB(0x40,0x40,0xff);
   m_Tile.m_cSurfB = RGB(0x40,0x40,0xff);
   m_Tile.m_dwBevelMode = 2;
   m_Tile.m_wSpecReflection = 0x4000;
   m_Tile.m_wSpecDirection = 4000;

   m_Tile.m_aNoise[0].m_fTurnOn = FALSE;
   m_Tile.m_aNoise[0].m_fNoiseX = .1;
   m_Tile.m_aNoise[0].m_fNoiseY = .1;
   m_Tile.m_aNoise[0].m_cMax = RGB(0x80, 0xff, 0x80);
   m_Tile.m_aNoise[0].m_cMin = RGB(0xff, 0x80, 0x80);
   m_Tile.m_aNoise[0].m_fTransMax = .6;
   m_Tile.m_aNoise[0].m_fTransMin = 1;

   m_Tile.m_aNoise[1].m_fTurnOn = FALSE;
   m_Tile.m_aNoise[1].m_fNoiseX = .025;
   m_Tile.m_aNoise[1].m_fNoiseY = .025;
   m_Tile.m_aNoise[1].m_cMax = RGB(0x80, 0x80, 0xff);
   m_Tile.m_aNoise[1].m_cMin = RGB(0x80, 0xff, 0x80);
   m_Tile.m_aNoise[1].m_fTransMax = .6;
   m_Tile.m_aNoise[1].m_fTransMin = 1;

   m_Tile.m_aNoise[2].m_fTurnOn = TRUE;
   m_Tile.m_aNoise[2].m_fNoiseX = .1;
   m_Tile.m_aNoise[2].m_fNoiseY = .1;
   m_Tile.m_aNoise[2].m_fZDeltaMax = 0;
   m_Tile.m_aNoise[2].m_fZDeltaMin = .001;
   m_Tile.m_aNoise[2].m_fTransMax = 1;
   m_Tile.m_aNoise[2].m_fTransMin = 1;

   m_Tile.m_aNoise[3].m_fTurnOn = TRUE;
   m_Tile.m_aNoise[3].m_fNoiseX = .05;
   m_Tile.m_aNoise[3].m_fNoiseY = .05;
   m_Tile.m_aNoise[3].m_fZDeltaMax = 0;
   m_Tile.m_aNoise[3].m_fZDeltaMin = .001;
   m_Tile.m_aNoise[3].m_fTransMax = 1;
   m_Tile.m_aNoise[3].m_fTransMin = 1;

   m_Tile.m_aNoise[4].m_fTurnOn = TRUE;
   m_Tile.m_aNoise[4].m_fNoiseX = .01;
   m_Tile.m_aNoise[4].m_fNoiseY = .01;
   m_Tile.m_aNoise[4].m_fZDeltaMax = 0;
   m_Tile.m_aNoise[4].m_fZDeltaMin = .0005;
   m_Tile.m_aNoise[4].m_fTransMax = 1;
   m_Tile.m_aNoise[4].m_fTransMin = 1;

   m_Tile.m_Chip.m_fTurnOn = FALSE;
   m_Tile.m_Chip.m_fTileHeight = m_Tile.m_fThickness;
   m_Tile.m_Chip.m_fFilterSize = .015;
   m_Tile.m_Chip.m_cChip = RGB(0xb0,0xb0,0xb0);
   m_Tile.m_Chip.m_fChipTrans = 0;
   m_Tile.m_Chip.m_wSpecDirection = 100;
   m_Tile.m_Chip.m_wSpecReflection = 0x500;
   m_Tile.m_Chip.m_aNoise[0].m_fTurnOn = TRUE;
   m_Tile.m_Chip.m_aNoise[0].m_fNoiseX = .025;
   m_Tile.m_Chip.m_aNoise[0].m_fNoiseY = .025;
   m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[0].m_fTransMax = 1;
   m_Tile.m_Chip.m_aNoise[0].m_fTransMin = 1;

   m_Tile.m_Chip.m_aNoise[1].m_fTurnOn = TRUE;
   m_Tile.m_Chip.m_aNoise[1].m_fNoiseX = .01;
   m_Tile.m_Chip.m_aNoise[1].m_fNoiseY = .01;
   m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[1].m_fTransMax = 1;
   m_Tile.m_Chip.m_aNoise[1].m_fTransMin = 1;

   m_Tile.m_Chip.m_aNoise[2].m_fTurnOn = TRUE;
   m_Tile.m_Chip.m_aNoise[2].m_fNoiseX = .005;
   m_Tile.m_Chip.m_aNoise[2].m_fNoiseY = .005;
   m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/4;
   m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/4;
   m_Tile.m_Chip.m_aNoise[2].m_fTransMax = 1;
   m_Tile.m_Chip.m_aNoise[2].m_fTransMin = 1;

   m_Grout.m_cColor = RGB(0xf0,0xf0,0xf0);
   m_Grout.m_aNoise[0].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[0].m_fNoiseX = .1;
   m_Grout.m_aNoise[0].m_fNoiseY = .1;
   m_Grout.m_aNoise[0].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[0].m_fZDeltaMin = -.003;
   m_Grout.m_aNoise[0].m_fTransMax = 1;
   m_Grout.m_aNoise[0].m_fTransMin = 1;

   m_Grout.m_aNoise[1].m_fTurnOn = TRUE;
   m_Grout.m_aNoise[1].m_fNoiseX = .025;
   m_Grout.m_aNoise[1].m_fNoiseY = .025;
   m_Grout.m_aNoise[1].m_fZDeltaMax = 0;
   m_Grout.m_aNoise[1].m_fZDeltaMin = -.001;
   m_Grout.m_aNoise[1].m_fTransMax = 1;
   m_Grout.m_aNoise[1].m_fTransMin = 1;

   m_DirtPaint.m_fTurnOn = FALSE;
   m_DirtPaint.m_cColor = RGB(0x60,0x60,0);
   m_DirtPaint.m_fAHeight = .003;
   m_DirtPaint.m_fFilterSize = .01;
   m_DirtPaint.m_fTransAtA = .25;
   m_DirtPaint.m_fTransNone = 1;
   m_DirtPaint.m_wSpecDirection = 100;
   m_DirtPaint.m_wSpecReflection = 0;

   m_Tile.m_cSurfA = m_Tile.m_cSurfB = RGB(0xff,0xff,0xff);
   m_Plank.m_fBevelHeight = .005;
   m_Plank.m_fBevelWidth = .003;
   m_Plank.m_fBoardBend = 0;
   m_Plank.m_wSpecReflection = 0x2000;
   m_Plank.m_wSpecDirection = 5000;
   m_Plank.m_acColors[0] = RGB(0xd0, 0xb0, 0x40);
   m_Plank.m_acColors[1] = RGB(0xd0, 0xb0, 0x30);
   m_Plank.m_acColors[2] = RGB(0xb0, 0x90, 0x30);
   m_Plank.m_acColors[3] = RGB(0x90, 0x70, 0x00);
   m_Plank.m_acColors[4] = RGB(0x50,0x30,0);
   m_Plank.m_cTransitionColor = RGB(0x50,0x20,0);
   m_Plank.m_fRingBump = .001;
   m_Plank.m_fBrightMin = .9;
   m_Plank.m_fBrightMax = 1.2;

   m_Plank.m_Noise.m_fTurnOn = TRUE;
   m_Plank.m_Noise.m_fNoiseX = .05;
   m_Plank.m_Noise.m_fNoiseY = .05;
   m_Plank.m_Noise.m_cMax = RGB(0xb0,0x70,0x30);
   m_Plank.m_Noise.m_cMin = RGB(0xb0,0x80,0x30);
   m_Plank.m_Noise.m_fTransMax = .7;
   m_Plank.m_Noise.m_fTransMin = 1;

   m_Plank.m_Tree.m_fRadius = .3;
   m_Plank.m_Tree.m_fLogCenterXOffsetMin = -.1;
   m_Plank.m_Tree.m_fLogCenterXOffsetMax = .1;
   m_Plank.m_Tree.m_fNumKnotsPerMeter = 20;
   m_Plank.m_Tree.m_fKnotWidthMin = .005;
   m_Plank.m_Tree.m_fKnotWidthMax = .03;
   m_Plank.m_Tree.m_fRingThickness = .001;
   m_Plank.m_Tree.m_aNoise[0].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseX = .03;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseY = .12;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMin = .006;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMin = 1;

   m_Plank.m_Tree.m_aNoise[1].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseX = .01;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseY = .04;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMin = .001;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMin = 1;

   m_Plank.m_Tree.m_fRingThickness = .001;
   m_Plank.m_Tree.m_fNumKnotsPerMeter /= 2;;
}

void CTextCreatorParquet::Delete (void)
{
   delete this;
}

static PWSTR gpszTileGroups = L"TileGroups";
static PWSTR gpszPattern = L"Pattern";
static PWSTR gpszUseTiles = L"UseTiles";
static PWSTR gpszSizeLong = L"SizeLong";
static PWSTR gpszSizeShort = L"SizeShort";
static PWSTR gpszPlank = L"Plank";

PCMMLNode2 CTextCreatorParquet::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTiles);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszSeed, (int) m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszSpacing, m_fSpacing);
   MMLValueSet (pNode, gpszAltColor, (BOOL) m_fAltColor);
   MMLValueSet (pNode, gpszAltA, (int) m_cAltA);
   MMLValueSet (pNode, gpszAltB, (int) m_cAltB);

   MMLValueSet (pNode, gpszTileGroups, (int) m_dwTileGroups);
   MMLValueSet (pNode, gpszPattern, (int)m_dwPattern);
   MMLValueSet (pNode, gpszUseTiles, (int)m_fUseTiles);
   MMLValueSet (pNode, gpszSizeLong, m_fSizeLong);
   MMLValueSet (pNode, gpszSizeShort, (int)m_dwSizeShort);
   MMLValueSet (pNode, gpszBrightMin, m_fBrightMin);
   MMLValueSet (pNode, gpszBrightMax, m_fBrightMax);
   MMLValueSet (pNode, gpszUseTiles, (int)m_fUseTiles);

   PCMMLNode2 pSub;
   pSub = m_Grout.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGrout);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Tile.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMakeTile);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Plank.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszPlank);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorParquet::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int) 0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fSpacing = MMLValueGetDouble (pNode, gpszSpacing, 0);
   m_fAltColor = (BOOL) MMLValueGetInt (pNode, gpszAltColor, (BOOL) 0);
   m_cAltA = (COLORREF) MMLValueGetInt (pNode, gpszAltA, (int) 0);
   m_cAltB = (COLORREF) MMLValueGetInt (pNode, gpszAltB, (int) 0);

   m_dwTileGroups = (DWORD) MMLValueGetInt (pNode, gpszTileGroups, (int) 1);
   m_dwPattern = (DWORD) MMLValueGetInt (pNode, gpszPattern, (int)0);
   m_fUseTiles = (BOOL) MMLValueGetInt (pNode, gpszUseTiles, (int)0);
   m_fSizeLong = MMLValueGetDouble (pNode, gpszSizeLong, .1);
   m_dwSizeShort = (DWORD) MMLValueGetInt (pNode, gpszSizeShort, (int)4);
   m_fBrightMin = MMLValueGetDouble (pNode, gpszBrightMin, 1);
   m_fBrightMax = MMLValueGetDouble (pNode, gpszBrightMax, 1);
   m_fUseTiles = (BOOL) MMLValueGetInt (pNode, gpszUseTiles, (int)0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGrout), &psz, &pSub);
   if (pSub)
      m_Grout.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszMakeTile), &psz, &pSub);
   if (pSub)
      m_Tile.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszPlank), &psz, &pSub);
   if (pSub)
      m_Plank.MMLFrom (pSub);

   return TRUE;
}

BOOL CTextCreatorParquet::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // pixels per tile
   DWORD dwTileLength, dwTileWidth, dwGrout, dwTileGroupSize;
   m_dwSizeShort = max(1, m_dwSizeShort);
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   m_fSizeLong = max(m_fSizeLong, fPixelLen);
   dwGrout = (DWORD) (m_fSpacing / fPixelLen);
   dwTileWidth = ((DWORD) ((m_fSizeLong + fPixelLen/2) / fPixelLen) / m_dwSizeShort);
   dwTileWidth = max(1,dwTileWidth);
   switch (m_dwPattern) {
   default:
   case 0:  // parquet
      dwTileLength = (dwTileWidth + dwGrout) * m_dwSizeShort - dwGrout;
      dwTileGroupSize = dwTileLength + dwGrout;
      break;
   case 1:  // herringbone
      dwTileLength = (dwTileWidth + dwGrout) * m_dwSizeShort - dwGrout;
      dwTileGroupSize = (dwTileLength + dwGrout) * 2;
      break;
   case 2:  // box
      dwTileLength = dwTileWidth * m_dwSizeShort;
      dwTileGroupSize = dwTileLength + dwTileWidth + 2 * dwGrout;
      break;
   }

   fp fTileHeight;
   if (m_fUseTiles)
      fTileHeight = m_Tile.m_fThickness / fPixelLen;     // pixels deep
   else
      fTileHeight = .001 / fPixelLen;
   

   // image
   DWORD dwX, dwY;
   dwX = dwY = dwTileGroupSize * m_dwTileGroups;
   pImage->Init (dwX, dwY, 0);   // 0 is assumed to be grout level

   fp fTileHeightMeters = fPixelLen * fTileHeight;

   // grout texture
   m_Grout.Apply (pImage, fPixelLen);

   // create a temporary image to contain the bits
   CImage ImageTemp;

   // do the tiles
   int h, v;
   DWORD dwSub;
   DWORD dwNum;
   BOOL fAlt, fAltColor;
   POINT pDest;
   COLORREF cOldA, cOldB;
   fp fOldMin, fOldMax;
   int iX, iY;
   if (m_dwPattern == 1) { // herringbone
      DWORD dwGroupX, dwGroupY;
      for (dwGroupX = 0; dwGroupX < m_dwTileGroups; dwGroupX++) for (dwGroupY = 0; dwGroupY < m_dwTileGroups; dwGroupY++) {
         DWORD dwSize;
         dwSize = 2 * m_dwSizeShort;
         dwNum = m_dwSizeShort * 2;

         h = v = 0;
         DWORD i;
         for (i = 0; i < dwNum; i++, h += 1, v -= 1) {
            h = (h+(int)dwSize) % dwSize;
            v = (v+(int)dwSize) % dwSize;

            for (dwSub = 0; dwSub < 2; dwSub++) {
               fAlt = fAltColor = (i % 2);
               if (!m_fAltColor) // BUGFIX - If no alt color then dont alternatre
                  fAlt = fAltColor = FALSE;
               ImageTemp.Init (dwTileWidth, dwTileLength, 0, -10000);

               if (fAltColor && m_fUseTiles) {
                  cOldA = m_Tile.m_cSurfA;
                  cOldB = m_Tile.m_cSurfB;
                  m_Tile.m_cSurfA = m_cAltA;
                  m_Tile.m_cSurfB = m_cAltB;
               }
               else if (fAltColor && !m_fUseTiles) {
                  fOldMin = m_Plank.m_fBrightMin;
                  fOldMax = m_Plank.m_fBrightMax;
                  m_Plank.m_fBrightMin = m_fBrightMin;
                  m_Plank.m_fBrightMax = m_fBrightMax;
               }

               // draw the tile
               if (m_fUseTiles) {
                  m_Tile.m_fWidth = (fp)ImageTemp.Width() * fPixelLen;
                  m_Tile.m_fHeight = (fp)ImageTemp.Height() * fPixelLen;
                  m_Tile.Apply(&ImageTemp, fPixelLen, 0, 0);
               }
               else {
                  m_Plank.Apply (&ImageTemp, fPixelLen, (int)ImageTemp.Width(), (int)ImageTemp.Height(), 0, 0);
               }

               // undo change
               if (fAltColor && m_fUseTiles) {
                  m_Tile.m_cSurfA = cOldA;
                  m_Tile.m_cSurfB = cOldB;
               }
               else if (fAltColor && !m_fUseTiles) {
                  m_Plank.m_fBrightMin = fOldMin;
                  m_Plank.m_fBrightMax = fOldMax;
               }

               // put in place
               iX = (h + (int)dwSize * (int) dwGroupX) * (int) (dwTileWidth + dwGrout);
               iY = (v + (int)dwSize * (int) dwGroupY) * (int) (dwTileWidth + dwGrout);
               if (dwSub) {   // horizontal
                  pDest.x = iX;
                  pDest.y = iY;
                  pDest.y += (int) (dwTileWidth-1);
                     // the +dwTileWidth accounts for rotation
                  ImageTemp.TGMergeRotate (NULL, pImage, pDest);
               }
               else { // vertical
                  pDest.x = iX + dwTileLength + dwGrout;
                  pDest.y = iY;
                  ImageTemp.TGMerge (NULL, pImage, pDest);
               }

            }  // over dwSub
         }  // over herringbone
      }  // over groups
   }
   else {
      for (v = 0; v < (int) m_dwTileGroups; v++) for (h = 0; h < (int) m_dwTileGroups; h++) {
         // position that want
         iX = h * dwTileGroupSize + dwGrout/2;
         iY = v * dwTileGroupSize + dwGrout/2;

         // do all the sub tiles
         dwNum = m_dwSizeShort;
         if (m_dwPattern == 2)
            dwNum = 5;  // box pattern always has 5
         for (dwSub = 0; dwSub < dwNum; dwSub++) {
            // alternate?

            switch (m_dwPattern) {
            default:
            case 0:  // parquet
               fAlt = ((h+v)%2);
               fAltColor = m_fAltColor & fAlt;
               break;
            case 2:  // box
               fAlt = fAltColor = (dwSub == 4); // last one, cetner, is alternate
               if (!m_fAltColor) // BUGFIX - If no alt color then dont alternatre
                  fAlt = fAltColor = FALSE;
               break;
            }

            // initialize the image
            if ((m_dwPattern == 2) && (dwSub == 4)) {
               // have one in center... find the width
               int iX;
               iX = (int) dwTileGroupSize - (int)dwTileWidth * 2 - (int)dwGrout * 3;
               if (iX < 1)
                  continue;   // too small, so dont bother
               ImageTemp.Init (iX, iX, 0, -10000);
            }
            else
               ImageTemp.Init (dwTileWidth, dwTileLength, 0, -10000);

            if (fAltColor && m_fUseTiles) {
               cOldA = m_Tile.m_cSurfA;
               cOldB = m_Tile.m_cSurfB;
               m_Tile.m_cSurfA = m_cAltA;
               m_Tile.m_cSurfB = m_cAltB;
            }
            else if (fAltColor && !m_fUseTiles) {
               fOldMin = m_Plank.m_fBrightMin;
               fOldMax = m_Plank.m_fBrightMax;
               m_Plank.m_fBrightMin = m_fBrightMin;
               m_Plank.m_fBrightMax = m_fBrightMax;
            }

            // draw the tile
            if (m_fUseTiles) {
               m_Tile.m_fWidth = (fp)ImageTemp.Width() * fPixelLen;
               m_Tile.m_fHeight = (fp)ImageTemp.Height() * fPixelLen;
               m_Tile.Apply(&ImageTemp, fPixelLen, 0, 0);
            }
            else {
               m_Plank.Apply (&ImageTemp, fPixelLen, (int)ImageTemp.Width(), (int)ImageTemp.Height(), 0, 0);
            }

            // undo change
            if (fAltColor && m_fUseTiles) {
               m_Tile.m_cSurfA = cOldA;
               m_Tile.m_cSurfB = cOldB;
            }
            else if (fAltColor && !m_fUseTiles) {
               m_Plank.m_fBrightMin = fOldMin;
               m_Plank.m_fBrightMax = fOldMax;
            }

            // merge it in
            switch (m_dwPattern) {
            default:
            case 0:  // parquet
               if (fAlt) {
                  pDest.x = iX;
                  pDest.y = iY + (int)dwSub * (dwTileWidth + dwGrout);
                  pDest.y += (int) (dwTileWidth-1);
                     // the +dwTileWidth accounts for rotation
                  ImageTemp.TGMergeRotate (NULL, pImage, pDest);
               }
               else {
                  pDest.x = iX + dwSub * (dwTileWidth + dwGrout);
                  pDest.y = iY;
                  ImageTemp.TGMerge (NULL, pImage, pDest);
               }
               break;
            case 2:  // box
               switch (dwSub) {
               case 0:  // left vertial
                  pDest.x = iX;
                  pDest.y = iY + dwTileWidth + dwGrout;
                  ImageTemp.TGMerge (NULL, pImage, pDest);
                  break;
               case 1:  // right vertical
                  pDest.x = iX + dwTileGroupSize - dwTileWidth - dwGrout;
                  pDest.y = iY;
                  ImageTemp.TGMerge (NULL, pImage, pDest);
                  break;
               case 2:  // top horizontal
                  pDest.x = iX;
                  pDest.y = iY;
                  pDest.y += (int) (dwTileWidth-1);
                     // the +dwTileWidth accounts for rotation
                  ImageTemp.TGMergeRotate (NULL, pImage, pDest);
                  break;
               case 3:  // bottom horizontal
                  pDest.x = iX + dwTileWidth + dwGrout;
                  pDest.y = iY + dwTileGroupSize - dwTileWidth - dwGrout;
                  pDest.y += (int) (dwTileWidth-1);
                     // the +dwTileWidth accounts for rotation
                  ImageTemp.TGMergeRotate (NULL, pImage, pDest);
                  break;
               case 4:  // center
                  pDest.x = iX + dwTileWidth + dwGrout;
                  pDest.y = iY + dwTileWidth + dwGrout;
                  ImageTemp.TGMerge (NULL, pImage, pDest);
                  break;
               }
               break;

            }  // merge
         }  // over all sub

      } // over tile groups
   }  // non-herringbone


   // apply dirt
   m_DirtPaint.Apply (pImage, fPixelLen);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = TRUE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}






/***********************************************************************************
CTextCreatorNoise - Noise functions
*/

class CTextCreatorNoise : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorNoise (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in meters
   COLORREF         m_cColor;  // basic color
   CTextEffectNoise  m_aNoise[4];  // introduce noise to surface

   // puff
   BOOL              m_fPuffUse;    // set to TRUE then use the puff of smoke effect
   TEXTUREPOINT      m_tpPuffNoise; // size of noise blob in meters
   fp                m_fPuffDetail; // amount of detail in puff, from 0..1
   fp                m_fPuffStrength; // strength of noise, from 0 .. 1
   fp                m_fPuffFlatness; // how flat puff appears, 0..1
};
typedef CTextCreatorNoise *PCTextCreatorNoise;


/****************************************************************************
CrNoisePage
*/
BOOL CrNoisePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorNoise pv = (PCTextCreatorNoise) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);

         // puff
         pControl = pPage->ControlFind (L"puffuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fPuffUse);

         MeasureToString (pPage, L"puffnoise0", pv->m_tpPuffNoise.h, TRUE);
         MeasureToString (pPage, L"puffnoise1", pv->m_tpPuffNoise.v, TRUE);

         pControl = pPage->ControlFind (L"puffdetail");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fPuffDetail * 100));

         pControl = pPage->ControlFind (L"puffstrength");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fPuffStrength * 100));

         pControl = pPage->ControlFind (L"puffflat");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fPuffFlatness * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"puffuse")) {
            pv->m_fPuffUse = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         int iVal;
         iVal = p->pControl->AttribGetInt (Pos());
         fp fVal = (fp)iVal / 100.0;
         if (!_wcsicmp(p->pControl->m_pszName, L"puffdetail")) {
            pv->m_fPuffDetail = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"puffstrength")) {
            pv->m_fPuffStrength = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"puffflat")) {
            pv->m_fPuffFlatness = fVal;
            return TRUE;
         }
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         fp fTemp;
         MeasureParseString (pPage, L"puffnoise0", &fTemp);
         pv->m_tpPuffNoise.h = fTemp;
         pv->m_tpPuffNoise.h = max(pv->m_tpPuffNoise.h, CLOSE);

         MeasureParseString (pPage, L"puffnoise1", &fTemp);
         pv->m_tpPuffNoise.v = fTemp;
         pv->m_tpPuffNoise.v = max(pv->m_tpPuffNoise.v, CLOSE);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Noise";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorNoise::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURECRNOISE, CrNoisePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(3, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorNoise::CTextCreatorNoise (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;
   m_cColor = 0;

   m_fPuffUse = FALSE;
   m_tpPuffNoise.h = m_tpPuffNoise.v = 0.1;
   m_fPuffDetail = 0.5;
   m_fPuffStrength = 0.5;
   m_fPuffFlatness = 0.5;

   m_fPixelLen = .01;
   m_iSeed = 12345;
   m_fPatternWidth = 1;
   m_fPatternHeight = 1;
   m_cColor = RGB(0xf0, 0xf0, 0xf0);
   m_aNoise[0].m_fTurnOn = FALSE;
   m_aNoise[0].m_fNoiseX = .2;
   m_aNoise[0].m_fNoiseY = .2;
   m_aNoise[0].m_cMax = RGB(0xff,0xff,0);
   m_aNoise[0].m_cMin = RGB(0xff,0xff,0);
   m_aNoise[0].m_fTransMax = .8;
   m_aNoise[0].m_fTransMin = 1;

   m_aNoise[1].m_fTurnOn = FALSE;
   m_aNoise[1].m_fNoiseX = .05;
   m_aNoise[1].m_fNoiseY = .05;
   m_aNoise[1].m_cMax = RGB(0, 0, 0xff);
   m_aNoise[1].m_cMin = RGB(0xff,0xff,0);
   m_aNoise[1].m_fTransMax = .8;
   m_aNoise[1].m_fTransMin = 1;

   m_aNoise[2].m_fTurnOn = FALSE;
   m_aNoise[2].m_fNoiseX = .2;
   m_aNoise[2].m_fNoiseY = .2;
   m_aNoise[2].m_fZDeltaMax = 0;
   m_aNoise[2].m_fZDeltaMin = .01;
   m_aNoise[2].m_fTransMax = 1;
   m_aNoise[2].m_fTransMin = 1;

   m_aNoise[3].m_fTurnOn = FALSE;
   m_aNoise[3].m_fNoiseX = .025;
   m_aNoise[3].m_fNoiseY = .025;
   m_aNoise[3].m_fZDeltaMax = 0;
   m_aNoise[3].m_fZDeltaMin = .005;
   m_aNoise[3].m_fTransMax = 1;
   m_aNoise[3].m_fTransMin = 1;

   switch (m_dwType) {
   case 0:  // stucco
      m_cColor = RGB(0xff, 0xff, 0x80);
      m_aNoise[2].m_fTurnOn = TRUE;
      m_aNoise[2].m_fNoiseX = m_aNoise[2].m_fNoiseY = .3;
      m_aNoise[3].m_fTurnOn = TRUE;
      m_aNoise[3].m_fNoiseX = m_aNoise[3].m_fNoiseY = .005;
      m_aNoise[3].m_fZDeltaMin = .001;
      break;

   case 1:  // rendered cement
      m_cColor = RGB(0xe0,0xe0,0xe0);
      m_aNoise[0].m_fTurnOn = TRUE;
      m_aNoise[0].m_cMax = RGB(0xc0, 0xc0, 0xc0);
      m_aNoise[0].m_cMin = RGB(0xc0, 0xc0, 0xc0);
      m_aNoise[2].m_fTurnOn = TRUE;
      m_aNoise[3].m_fTurnOn = TRUE;
      m_aNoise[3].m_fNoiseX = m_aNoise[3].m_fNoiseY = .005;
      m_aNoise[3].m_fZDeltaMin = .001;
      break;

   case 2:  // speckled ceiling
      m_cColor = RGB(0xff, 0xff, 0xff);
      m_aNoise[3].m_fTurnOn = TRUE;
      m_aNoise[3].m_fNoiseX = m_aNoise[3].m_fNoiseY = .005;
      m_aNoise[3].m_fZDeltaMin = .001;
      break;

   case 3:  // grass
      m_cColor = RGB(0x20, 0xc0, 0x10);
      m_aNoise[0].m_fTurnOn = TRUE;
      m_aNoise[0].m_cMax = m_aNoise[0].m_cMin = RGB(0x20, 0xff, 0x20);
      m_aNoise[1].m_fTurnOn = TRUE;
      m_aNoise[1].m_fNoiseX = .01;
      m_aNoise[1].m_fNoiseY = .01;
      m_aNoise[1].m_cMax = m_aNoise[1].m_cMin = RGB(0x40, 0xff, 0x50);
      m_aNoise[1].m_fTransMax = .2;
      m_aNoise[1].m_fTransMin = 1;

      m_aNoise[2].m_fTurnOn = TRUE;
      m_aNoise[2].m_fNoiseX = .1;
      m_aNoise[2].m_fNoiseY = .1;
      m_aNoise[2].m_fZDeltaMax = 2;
      m_aNoise[2].m_fZDeltaMin = -2;
      m_aNoise[2].m_fTransMax = 1;
      m_aNoise[2].m_fTransMin = 1;

      m_aNoise[3].m_fTurnOn = TRUE;
      m_aNoise[3].m_fNoiseX = .01;
      m_aNoise[3].m_fNoiseY = .01;
      m_aNoise[3].m_fZDeltaMax = -2;
      m_aNoise[3].m_fZDeltaMin = 2;
      m_aNoise[3].m_fTransMax = 1;
      m_aNoise[3].m_fTransMin = 1;
      break;

   case 4:  // carpet
      m_cColor = RGB(0xf0, 0xf0, 0xc0);
      m_aNoise[3].m_fTurnOn = TRUE;
      m_aNoise[3].m_fNoiseX = m_aNoise[3].m_fNoiseY = .01;
      m_aNoise[3].m_fZDeltaMin = .003;
      break;
   }
}

void CTextCreatorNoise::Delete (void)
{
   delete this;
}

static PWSTR gpszPatternWidth = L"PatternWidth";
static PWSTR gpszPatternHeight = L"PatternHeight";
static PWSTR gpszPuffDetail = L"PuffDetail";
static PWSTR gpszPuffFlatness = L"PuffFlatness";
static PWSTR gpszPuffNoise = L"PuffNoise";
static PWSTR gpszPuffStrength = L"PuffStrength";
static PWSTR gpszPuffUse = L"PuffUse";

PCMMLNode2 CTextCreatorNoise::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszColor, (int)m_cColor);

   MMLValueSet (pNode, gpszPuffUse, (int)m_fPuffUse);
   MMLValueSet (pNode, gpszPuffNoise, &m_tpPuffNoise);
   MMLValueSet (pNode, gpszPuffDetail, m_fPuffDetail);
   MMLValueSet (pNode, gpszPuffStrength, m_fPuffStrength);
   MMLValueSet (pNode, gpszPuffFlatness, m_fPuffFlatness);

   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 4; i++) {
      PCMMLNode2 pSub = m_aNoise[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorNoise::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int)0);

   m_fPuffUse = (BOOL) MMLValueGetInt (pNode, gpszPuffUse, FALSE);
   MMLValueGetTEXTUREPOINT (pNode, gpszPuffNoise, &m_tpPuffNoise);
   m_fPuffDetail = MMLValueGetDouble (pNode, gpszPuffDetail, 0.5);
   m_fPuffStrength = MMLValueGetDouble (pNode, gpszPuffStrength, 0.5);
   m_fPuffFlatness = MMLValueGetDouble (pNode, gpszPuffFlatness, 0.5);

   DWORD i;
   WCHAR szTemp[64];
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwFind;
   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"Noise%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }

   return TRUE;
}

BOOL CTextCreatorNoise::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwScale = max(dwX, dwY);


   // create the surface
   pImage->Init (dwX, dwY, RGB(0xff,0,0), 0);

   // grout texture
   // draw the tile at at height and specularity
   COLORREF cGrout = m_cColor;
   IHLEND end[4];
   pImage->TGDrawQuad (
      pImage->ToIHLEND(&end[0], 0, 0, 0, cGrout),
      pImage->ToIHLEND(&end[1], dwX , 0, 0, cGrout),
      pImage->ToIHLEND(&end[2], dwX, dwY, 0, cGrout),
      pImage->ToIHLEND(&end[3], 0, dwY, 0, cGrout),
      1, TRUE, TRUE, m_Material.m_wSpecReflect, m_Material.m_wSpecExponent
      );


   DWORD i;
   for (i = 0; i < 4; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   if (m_fPuffUse) {
      // put in the puff
      CNoise2D Noise;
      DWORD dwNoiseX, dwNoiseY;
      dwNoiseX = m_fPatternWidth / max(m_tpPuffNoise.h, CLOSE);
      dwNoiseX = max(dwNoiseX, 1);
      dwNoiseX = min(dwNoiseX, 100);
      dwNoiseY = m_fPatternHeight / max(m_tpPuffNoise.v, CLOSE);
      dwNoiseY = max(dwNoiseY, 1);
      dwNoiseY = min(dwNoiseY, 100);
      Noise.Init (dwNoiseX, dwNoiseY);

      // if there's no transparency map currently then set transparency bits
      DWORD x,y;
      if (!(pTextInfo->dwMap & 0x04)) {
         BYTE bTrans = (BYTE) (pMaterial->m_wTransparency >> 8);
         PIMAGEPIXEL pip = pImage->Pixel (0, 0);
         for (y = 0; y < dwY*dwX; y++, pip++)
            pip->dwIDPart = (pip->dwIDPart & ~0xff) | bTrans;
      }

      // loop
      fp fStrength = m_fPuffStrength * 2.0;
      for (y = 0; y < dwY; y++) {
         PIMAGEPIXEL pip = pImage->Pixel (0, y);
         fp fY = ((fp)y - (fp)dwY/2) / ((fp)dwY / 2); // -1 to 1

         for (x = 0; x < dwX; x++, pip++) {
            fp fX = ((fp)x - (fp)dwX/2) / ((fp)dwX / 2); // -1 to 1
            fp fZ = 1.0 - fX * fX - fY * fY;
            fp fTrans;

            if (fZ < 0)
               fTrans = 1.0;  // beyond edge of puff
            else {
               // flatten it out
               fZ = fTrans = 1.0 - pow (fZ, (fp)(1.0 - m_fPuffFlatness));
                  // NOTE: Should be using sqrt fZ, but works better without

               // add noise
               fTrans += Noise.Value (fX / 2, fY / 2) * fStrength; // since need to range over -0.5 to 0.5
               fTrans += Noise.Value (fX, fY) * fStrength * m_fPuffDetail;
               fTrans += Noise.Value (fX*2, fY*2) * fStrength * m_fPuffDetail * m_fPuffDetail;
               fTrans += Noise.Value (fX*4, fY*4) * fStrength * m_fPuffDetail * m_fPuffDetail * m_fPuffDetail;
               // NOTE: Trans may be < 0 or > 1

               // don't allow to be less transparent than original
               if (fTrans < fZ)
                  fTrans = (fTrans + fZ) / 2;
            }

            // if not transparent at all do nothing
            if (fTrans <= 0)
               continue;

            // if totally transparent...
            if (fTrans >= 1) {
               pip->dwIDPart |= 0xff;     // so transparent
               pip->dwID &= 0x0000ffff;   // to get rid of the specular reflection in transparent
               continue;
            }

            // else, interpolate
            DWORD dwTrans = (int)(pip->dwIDPart & 0xff) + (int)(fTrans * (fp)0xff);
            dwTrans = min(dwTrans, 0xff);
            pip->dwIDPart = (pip->dwIDPart & ~0xff) | dwTrans;

            dwTrans = HIWORD(pip->dwID);
            dwTrans = (dwTrans * (DWORD)((1.0 - fTrans) * (fp)0xffff));
            pip->dwID = (dwTrans & 0xffff0000) | LOWORD(pip->dwID);
               // can do this trick since dwTrans is 16-bit fixed point quantity
         } // x
      } // y

      // set flags
      pTextInfo->dwMap |= 0x01 | 0x04; // specularity and transparency
   }

   return TRUE;
}









/***********************************************************************************
CTextCreatorFabric - Noise functions
*/

class CTextCreatorFabric : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorFabric (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in meters
   COLORREF         m_cColor;  // basic color
   CTextEffectNoise  m_aNoise[4];  // introduce noise to surface
   CTextEffectStripes m_aStripe[6]; // put stripes in fabric
   CTextEffectThreads m_Threads;
};
typedef CTextCreatorFabric *PCTextCreatorFabric;


/****************************************************************************
FabricPage
*/
BOOL FabricPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorFabric pv = (PCTextCreatorFabric) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Fabric";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorFabric::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFABRIC, FabricPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   PWSTR pszStripe = L"stripe";
   DWORD dwLenStripe = (DWORD)wcslen(pszStripe);
   if (pszRet && !_wcsicmp(pszRet, L"Threads")) {
      if (m_Threads.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(3, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !wcsncmp(pszRet, pszStripe, dwLenStripe)) {
      DWORD dwVal = _wtoi(pszRet + dwLenStripe);
      dwVal = min(5, dwVal);
      if (m_aStripe[dwVal].Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorFabric::CTextCreatorFabric (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_CLOTHSMOOTH);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;
   m_cColor = 0;

   m_fPixelLen = .01;
   m_iSeed = 12345;
   m_fPatternWidth = 1;
   m_fPatternHeight = 1;
   m_cColor = RGB(0x30, 0x30, 0xf0);
   m_aNoise[0].m_fTurnOn = FALSE;
   m_aNoise[0].m_fNoiseX = .2;
   m_aNoise[0].m_fNoiseY = .2;
   m_aNoise[0].m_cMax = RGB(0xff,0xff,0);
   m_aNoise[0].m_cMin = RGB(0xff,0xff,0);
   m_aNoise[0].m_fTransMax = .8;
   m_aNoise[0].m_fTransMin = 1;

   m_aNoise[1].m_fTurnOn = FALSE;
   m_aNoise[1].m_fNoiseX = .05;
   m_aNoise[1].m_fNoiseY = .05;
   m_aNoise[1].m_cMax = RGB(0, 0, 0xff);
   m_aNoise[1].m_cMin = RGB(0xff,0xff,0);
   m_aNoise[1].m_fTransMax = .8;
   m_aNoise[1].m_fTransMin = 1;

   m_aNoise[2].m_fTurnOn = FALSE;
   m_aNoise[2].m_fNoiseX = .2;
   m_aNoise[2].m_fNoiseY = .2;
   m_aNoise[2].m_fZDeltaMax = 0;
   m_aNoise[2].m_fZDeltaMin = .01;
   m_aNoise[2].m_fTransMax = 1;
   m_aNoise[2].m_fTransMin = 1;

   m_aNoise[3].m_fTurnOn = FALSE;
   m_aNoise[3].m_fNoiseX = .025;
   m_aNoise[3].m_fNoiseY = .025;
   m_aNoise[3].m_fZDeltaMax = 0;
   m_aNoise[3].m_fZDeltaMin = .005;
   m_aNoise[3].m_fTransMax = 1;
   m_aNoise[3].m_fTransMin = 1;

}

void CTextCreatorFabric::Delete (void)
{
   delete this;
}

PCMMLNode2 CTextCreatorFabric::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszColor, (int)m_cColor);

   WCHAR szTemp[64];
   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < 4; i++) {
      pSub = m_aNoise[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   for (i = 0; i < 6; i++) {
      pSub = m_aStripe[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"stripe%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   pSub = m_Threads.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszThreads);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorFabric::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int)0);

   DWORD i;
   WCHAR szTemp[64];
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwFind;
   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"Noise%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }

   for (i = 0; i < 6; i++) {
      swprintf (szTemp, L"stripe%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aStripe[i].MMLFrom (pSub);
   }

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszThreads), &psz, &pSub);
   if (pSub)
      m_Threads.MMLFrom (pSub);

   return TRUE;
}

BOOL CTextCreatorFabric::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwScale = max(dwX, dwY);


   // create the surface
   pImage->Init (dwX, dwY, RGB(0xff,0,0), 0);

   // grout texture
   // draw the tile at at height and specularity
   COLORREF cGrout = m_cColor;
   IHLEND end[4];
   pImage->TGDrawQuad (
      pImage->ToIHLEND(&end[0], 0, 0, 0, cGrout),
      pImage->ToIHLEND(&end[1], dwX , 0, 0, cGrout),
      pImage->ToIHLEND(&end[2], dwX, dwY, 0, cGrout),
      pImage->ToIHLEND(&end[3], 0, dwY, 0, cGrout),
      1, TRUE, TRUE, m_Material.m_wSpecReflect, m_Material.m_wSpecExponent
      );



   DWORD i;
   for (i = 0; i < 6; i++)
      m_aStripe[i].Apply (pImage, fPixelLen);
   m_Threads.Apply (pImage, fPixelLen);
   for (i = 0; i < 4; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}




/***********************************************************************************
CTextCreatorMarble - Noise functions
*/

class CTextCreatorMarble : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorMarble (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in meters
   COLORREF         m_cColor;  // basic color
   CTextEffectNoise  m_aNoise[4];  // introduce noise to surface
   CTextEffectMarbling m_Marble;
};
typedef CTextCreatorMarble *PCTextCreatorMarble;


/****************************************************************************
MarblePage
*/
BOOL MarblePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorMarble pv = (PCTextCreatorMarble) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Marbled surface";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorMarble::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREMARBLE, MarblePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   if (pszRet && !_wcsicmp(pszRet, L"marble")) {
      if (m_Marble.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(3, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorMarble::CTextCreatorMarble (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_TILEGLAZED);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;
   m_cColor = 0;

   m_fPixelLen = .01;
   m_iSeed = 12345;
   m_fPatternWidth = 1;
   m_fPatternHeight = 1;
   m_cColor = RGB(0xf0, 0xf0, 0xf0);
   m_aNoise[0].m_fTurnOn = FALSE;
   m_aNoise[0].m_fNoiseX = .2;
   m_aNoise[0].m_fNoiseY = .2;
   m_aNoise[0].m_cMax = RGB(0xff,0xff,0);
   m_aNoise[0].m_cMin = RGB(0xff,0xff,0);
   m_aNoise[0].m_fTransMax = .8;
   m_aNoise[0].m_fTransMin = 1;

   m_aNoise[1].m_fTurnOn = FALSE;
   m_aNoise[1].m_fNoiseX = .05;
   m_aNoise[1].m_fNoiseY = .05;
   m_aNoise[1].m_cMax = RGB(0, 0, 0xff);
   m_aNoise[1].m_cMin = RGB(0xff,0xff,0);
   m_aNoise[1].m_fTransMax = .8;
   m_aNoise[1].m_fTransMin = 1;

   m_aNoise[2].m_fTurnOn = FALSE;
   m_aNoise[2].m_fNoiseX = .2;
   m_aNoise[2].m_fNoiseY = .2;
   m_aNoise[2].m_fZDeltaMax = 0;
   m_aNoise[2].m_fZDeltaMin = .01;
   m_aNoise[2].m_fTransMax = 1;
   m_aNoise[2].m_fTransMin = 1;

   m_aNoise[3].m_fTurnOn = FALSE;
   m_aNoise[3].m_fNoiseX = .025;
   m_aNoise[3].m_fNoiseY = .025;
   m_aNoise[3].m_fZDeltaMax = 0;
   m_aNoise[3].m_fZDeltaMin = .005;
   m_aNoise[3].m_fTransMax = 1;
   m_aNoise[3].m_fTransMin = 1;

   m_Marble.m_fTurnOn = TRUE;
}

void CTextCreatorMarble::Delete (void)
{
   delete this;
}

PCMMLNode2 CTextCreatorMarble::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszColor, (int)m_cColor);

   WCHAR szTemp[64];
   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < 4; i++) {
      pSub = m_aNoise[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   pSub = m_Marble.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMarbling);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorMarble::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int)0);

   DWORD i;
   WCHAR szTemp[64];
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwFind;
   for (i = 0; i < 4; i++) {
      swprintf (szTemp, L"Noise%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszMarbling), &psz, &pSub);
   if (pSub)
      m_Marble.MMLFrom (pSub);

   return TRUE;
}

BOOL CTextCreatorMarble::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwScale = max(dwX, dwY);


   // create the surface
   pImage->Init (dwX, dwY, RGB(0xff,0,0), 0);

   // grout texture
   // draw the tile at at height and specularity
   COLORREF cGrout = m_cColor;
   IHLEND end[4];
   pImage->TGDrawQuad (
      pImage->ToIHLEND(&end[0], 0, 0, 0, cGrout),
      pImage->ToIHLEND(&end[1], dwX , 0, 0, cGrout),
      pImage->ToIHLEND(&end[2], dwX, dwY, 0, cGrout),
      pImage->ToIHLEND(&end[3], 0, dwY, 0, cGrout),
      1, TRUE, TRUE, m_Material.m_wSpecReflect, m_Material.m_wSpecExponent
      );

   m_Marble.Apply (pImage, fPixelLen);

   DWORD i;
   for (i = 0; i < 4; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}



/***********************************************************************************
CTextCreatorCorrogated - Noise functions
*/

class CTextCreatorCorrogated : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorCorrogated (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   DWORD            m_dwShape; // 0 = sine wave, 1=square
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fRippleWidth; // width of ripple
   fp               m_fRippleHeight;   // height of ripple
   DWORD            m_dwRipples;  // number of ripples across
   COLORREF         m_cColor;  // basic color
   CTextEffectNoise m_Noise;  // introduce noise to surface
};
typedef CTextCreatorCorrogated *PCTextCreatorCorrogated;


/****************************************************************************
CorrPage
*/
BOOL CorrPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorCorrogated pv = (PCTextCreatorCorrogated) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         DoubleToControl (pPage, L"ripples", pv->m_dwRipples);
         MeasureToString (pPage, L"ripplewidth", pv->m_fRippleWidth, TRUE);
         MeasureToString (pPage, L"rippleheight", pv->m_fRippleHeight, TRUE);

         ComboBoxSet (pPage, L"shape", pv->m_dwShape);
         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shape")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwShape)
               break; // unchanged
            pv->m_dwShape = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);
         pv->m_dwRipples = (DWORD) DoubleFromControl (pPage, L"ripples");
         pv->m_dwRipples = max(1, pv->m_dwRipples);
         MeasureParseString (pPage, L"ripplewidth", &pv->m_fRippleWidth);
         pv->m_fRippleWidth = max(.001, pv->m_fRippleWidth);
         MeasureParseString (pPage, L"rippleheight", &pv->m_fRippleHeight);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Corrogated iron";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorCorrogated::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURECORR, CorrPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"noise")) {
      if (m_Noise.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}


CTextCreatorCorrogated::CTextCreatorCorrogated (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_dwShape = 0;
   m_fPixelLen = m_fRippleWidth = m_fRippleHeight = 0;
   m_dwRipples = 0;
   m_cColor = 0;

   m_iSeed = 12345;
   m_dwShape = 0;
   m_fPixelLen = 0.0025;
   m_fRippleWidth = .05;
   m_fRippleHeight = .025;
   m_dwRipples = 5;
   m_cColor = RGB(0xe0,0xe0,0xe0);
   m_Noise.m_fTurnOn = TRUE;
   m_Noise.m_fNoiseX = .001;
   m_Noise.m_fNoiseY = .001;
   m_Noise.m_fZDeltaMax = 0;
   m_Noise.m_fZDeltaMin = .002;
   m_Noise.m_fTransMax = 1;
   m_Noise.m_fTransMin = 1;

   switch (LOWORD(m_dwType)) {
   case 0: // zincalum
      m_cColor = RGB(0xe0, 0xe0, 0xe0);
      break;
   case 1: // red
      m_cColor = RGB(0xc0, 0x0, 0x0);
      m_Noise.m_fTurnOn = FALSE;
      break;
   case 2: // green
      m_cColor = RGB(0, 0xc0, 0x0);
      m_Noise.m_fTurnOn = FALSE;
      break;
   case 3: // blue
      m_cColor = RGB(0, 0, 0xc0);
      m_Noise.m_fTurnOn = FALSE;
      break;
   case 4: // creme
      m_cColor = RGB(0xc0, 0xc0, 0x80);
      m_Noise.m_fTurnOn = FALSE;
      break;
   case 5: // grey
      m_cColor = RGB(0x80, 0x80, 0x80);
      m_Noise.m_fTurnOn = FALSE;
      break;
   case 6: // black
      m_cColor = RGB(0,0,0);
      m_Noise.m_fTurnOn = FALSE;
      break;
   case 7: // cyan
      m_cColor = RGB(0x80, 0xff, 0xff);
      m_Noise.m_fTurnOn = FALSE;
      break;
   case 8: // white
      m_cColor = RGB(0xff, 0xff, 0xff);
      m_Noise.m_fTurnOn = FALSE;
      break;
   }

   if (HIWORD(m_dwType))
      m_dwShape = 1;  // squree
}

void CTextCreatorCorrogated::Delete (void)
{
   delete this;
}

static PWSTR gpszCorrogated = L"Corrogated";
static PWSTR gpszRippleWidth = L"RippleWidth";
static PWSTR gpszRippleHeight = L"RippleHeight";
static PWSTR gpszRipples = L"Ripples";

PCMMLNode2 CTextCreatorCorrogated::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszCorrogated);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszShape, (int)m_dwShape);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszRippleWidth, m_fRippleWidth);
   MMLValueSet (pNode, gpszRippleHeight, m_fRippleHeight);
   MMLValueSet (pNode, gpszRipples, (int) m_dwRipples);
   MMLValueSet (pNode, gpszColor, (int) m_cColor);

   PCMMLNode2 pSub;
   pSub = m_Noise.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszNoise);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorCorrogated::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fRippleWidth = MMLValueGetDouble (pNode, gpszRippleWidth, 0);
   m_fRippleHeight = MMLValueGetDouble (pNode, gpszRippleHeight, 0);
   m_dwRipples = (DWORD) MMLValueGetInt (pNode, gpszRipples, (int) 0);
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int) 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszNoise), &psz, &pSub);
   if (pSub)
      m_Noise.MMLFrom (pSub);

   return TRUE;
}


BOOL CTextCreatorCorrogated::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_dwRipples * m_fRippleWidth + fPixelLen/2) / fPixelLen);
   dwY = dwX;
   dwScale = max(dwX, dwY);
   // adjust the pixel length so that 

   srand (m_iSeed);

   WORD wc[3];
   pImage->Gamma (m_cColor, wc);


   // create the surface
   pImage->Init (dwX, dwY, RGB(0xff,0,0), 0);

   DWORD x, y;
   PIMAGEPIXEL pip;
   for (x = 0, pip = pImage->Pixel(0,0); x < dwX; x++, pip++) {
      fp f;
      pip->dwID = ((DWORD)m_Material.m_wSpecReflect << 16) | m_Material.m_wSpecExponent;

      f = (fp)x / (fp)dwX * (fp)m_dwRipples;
      f = myfmod(f, 1);
      switch (m_dwShape) {
      default:
      case 0:  // sine wave
         f = sin(f * 2 * PI);
         break;
      case 1:  // one major bend
         if (f < .5) {
            BOOL fNeg;
            f = -cos(f * 4 * PI);
            fNeg = FALSE;
            if (f < 0) {
               fNeg = TRUE;
               f *= -1;
            }
            f = pow (f, (fp).5);
            if (fNeg)
               f *= -1;
         }
         else if (f < .75 - .05)
            f = -1;
         else if (f < .75 + .05)
            f = -1.1;   // just slightly deeper
         else
            f = -1;
         break;
      }
      
      // pip->iZ = (int) (f * ((fp)dwX / 2) / 2 / (fp) m_dwRipples * 0x10000);
      pip->fZ = (f * m_fRippleHeight / fPixelLen / 2.0);
         // height of a ripple is about 1/2 length of ripple
      pip->wRed = wc[0];
      pip->wGreen = wc[1];
      pip->wBlue = wc[2];
   }

   for (y = 0; y < dwY; y++) {
      memcpy (pImage->Pixel(0,y), pImage->Pixel(0,0), dwX * sizeof(IMAGEPIXEL));
   }

   // some bumps
   m_Noise.Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}


/***********************************************************************************
CTextCreatorWoodPlanks - Noise functions
*/

class CTextCreatorWoodPlanks : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorWoodPlanks (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in metetrs
   fp               m_fPlankSpacing;    // distance (in meters) between planks
   COLORREF         m_cSpacing;         // coloration of the spacing
   BOOL             m_fSpacingTransparent;   // if TRUE, spacing between boards is transparent
   DWORD            m_dwPlanksAcross;   // number of planks across
   DWORD            m_dwPlanksUpDownMin;   // minimum number of plansk up and down
   DWORD            m_dwPlanksUpDownMax;   // maximum number of plansk up and down
   CTextEffectGeneratePlank m_Plank;   // description of a plank
   CTextEffectDirtPaint m_DirtPaint;        // paint or add dirt
};
typedef CTextCreatorWoodPlanks *PCTextCreatorWoodPlanks;


/****************************************************************************
WoodPlanksPage
*/
BOOL WoodPlanksPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorWoodPlanks pv = (PCTextCreatorWoodPlanks) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);
         DoubleToControl (pPage, L"planksacross", pv->m_dwPlanksAcross);
         DoubleToControl (pPage, L"planksupdownmin", pv->m_dwPlanksUpDownMin);
         DoubleToControl (pPage, L"planksupdownmax", pv->m_dwPlanksUpDownMax);
         MeasureToString (pPage, L"plankspacing", pv->m_fPlankSpacing, TRUE);
         FillStatusColor (pPage, L"spacingcolor", pv->m_cSpacing);

         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSpacingTransparent);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"spacingbutton")) {
            pv->m_cSpacing = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSpacing, pPage, L"spacingcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwPlanksAcross = (DWORD) DoubleFromControl (pPage, L"planksacross");
         pv->m_dwPlanksAcross = max(1,pv->m_dwPlanksAcross);

         pv->m_dwPlanksUpDownMin = (DWORD) DoubleFromControl (pPage, L"planksupdownmin");
         pv->m_dwPlanksUpDownMin = max(1, pv->m_dwPlanksUpDownMin);

         pv->m_dwPlanksUpDownMax = (DWORD) DoubleFromControl (pPage, L"planksupdownmax");
         pv->m_dwPlanksUpDownMax = max(pv->m_dwPlanksUpDownMin, pv->m_dwPlanksUpDownMax);

         MeasureParseString (pPage, L"plankspacing", &pv->m_fPlankSpacing);
         pv->m_fPlankSpacing = max(0,pv->m_fPlankSpacing);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pv->m_fSpacingTransparent = pControl->AttribGetBOOL (Checked());
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Wood planks";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorWoodPlanks::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREWOODPLANKS, WoodPlanksPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"plank")) {
      if (m_Plank.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorWoodPlanks::CTextCreatorWoodPlanks (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = m_fPlankSpacing = 0;
   m_cSpacing = 0;
   m_fSpacingTransparent = FALSE;
   m_dwPlanksAcross = m_dwPlanksUpDownMin = m_dwPlanksUpDownMax = 0;

   m_iSeed = 1234;
   m_fPixelLen = .005;
   m_fPatternWidth = .5;
   m_fPatternHeight = 2;
   m_dwPlanksAcross = 5;
   m_dwPlanksUpDownMin = 2;
   m_dwPlanksUpDownMax = 4;
   m_fPlankSpacing = 0;
   m_cSpacing = 0;


   m_Plank.m_fBevelHeight = .005;
   m_Plank.m_fBevelWidth = .003;
   m_Plank.m_fBoardBend = 0;
   m_Plank.m_wSpecReflection = 0x2000;
   m_Plank.m_wSpecDirection = 5000;
   m_Plank.m_acColors[0] = RGB(0xd0, 0xb0, 0x40);
   m_Plank.m_acColors[1] = RGB(0xd0, 0xb0, 0x30);
   m_Plank.m_acColors[2] = RGB(0xb0, 0x90, 0x30);
   m_Plank.m_acColors[3] = RGB(0x90, 0x70, 0x00);
   m_Plank.m_acColors[4] = RGB(0x50,0x30,0);
   m_Plank.m_cTransitionColor = RGB(0x50,0x20,0);
   m_Plank.m_fRingBump = .001;
   m_Plank.m_fBrightMin = .9;
   m_Plank.m_fBrightMax = 1.2;

   m_Plank.m_Noise.m_fTurnOn = TRUE;
   m_Plank.m_Noise.m_fNoiseX = .05;
   m_Plank.m_Noise.m_fNoiseY = .05;
   m_Plank.m_Noise.m_cMax = RGB(0xb0,0x70,0x30);
   m_Plank.m_Noise.m_cMin = RGB(0xb0,0x80,0x30);
   m_Plank.m_Noise.m_fTransMax = .7;
   m_Plank.m_Noise.m_fTransMin = 1;

   m_Plank.m_Tree.m_fRadius = .3;
   m_Plank.m_Tree.m_fLogCenterXOffsetMin = -.1;
   m_Plank.m_Tree.m_fLogCenterXOffsetMax = .1;
   m_Plank.m_Tree.m_fNumKnotsPerMeter = 20;
   m_Plank.m_Tree.m_fKnotWidthMin = .005;
   m_Plank.m_Tree.m_fKnotWidthMax = .03;
   m_Plank.m_Tree.m_fRingThickness = .001;
   m_Plank.m_Tree.m_aNoise[0].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseX = .03;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseY = .12;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMin = .006;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMin = 1;

   m_Plank.m_Tree.m_aNoise[1].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseX = .01;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseY = .04;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMin = .001;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMin = 1;

   switch (m_dwType) {
   case 0:  // dark hardwood
      m_dwPlanksAcross = 8;
      m_Plank.m_Tree.m_fRingThickness = .001;
      m_dwPlanksUpDownMax = 2;
      m_Plank.m_Tree.m_fNumKnotsPerMeter /= 2;;
      // use this
      break;

   case 1:  // light hardwood
      m_dwPlanksAcross = 8;
      m_Plank.m_fBrightMin = .6;
      m_Plank.m_fBrightMax = 1.4;
      m_dwPlanksUpDownMin = 3;
      m_dwPlanksUpDownMax = 5;
      m_Plank.m_Tree.m_fRadius = .5;
      m_Plank.m_acColors[0] = RGB(0xff, 0xe0, 0x80);
      m_Plank.m_acColors[1] = RGB(0xff, 0xe0, 0x80);
      m_Plank.m_acColors[2] = RGB(0xf0, 0xd0, 0x80);
      m_Plank.m_acColors[3] = RGB(0xe0, 0xc0, 0x70);
      m_Plank.m_acColors[4] = RGB(0x90,0x90,0x40);
      m_Plank.m_Noise.m_cMax = m_Plank.m_Noise.m_cMin = RGB(0xb0,0xb0,0x80);
      m_Plank.m_cTransitionColor = RGB(0x90,0x80,0x30);
      m_Plank.m_Tree.m_fRingThickness *= 3;
      m_Plank.m_Tree.m_fNumKnotsPerMeter *= 4;
      m_Plank.m_fBevelWidth = 0;
      break;

   case 2:  // decking
      m_Plank.m_fBoardBend = 0.003;
      m_cSpacing = 0;
      m_fPlankSpacing = .005;
      m_Plank.m_fBrightMin = .6;
      m_Plank.m_fBrightMax = 1.4;
      m_dwPlanksUpDownMax = 1;
      m_dwPlanksUpDownMin = 1;
      m_Plank.m_Tree.m_fRadius = .5;
      m_Plank.m_acColors[0] = RGB(0xff, 0xe0, 0x80);
      m_Plank.m_acColors[1] = RGB(0xff, 0xe0, 0x80);
      m_Plank.m_acColors[2] = RGB(0xf0, 0xd0, 0x80);
      m_Plank.m_acColors[3] = RGB(0xe0, 0xc0, 0x70);
      m_Plank.m_acColors[4] = RGB(0x90,0x90,0x40);
      m_Plank.m_Noise.m_cMax = m_Plank.m_Noise.m_cMin = RGB(0xb0,0xb0,0x80);
      m_Plank.m_cTransitionColor = RGB(0x90,0x80,0x30);
      m_Plank.m_Tree.m_fRingThickness *= 2;
      m_Plank.m_Tree.m_fNumKnotsPerMeter *= 2;
      m_Plank.m_fBevelWidth = .005;
      m_Plank.m_fBevelHeight = .003;
      break;

   case 3:  // plywood
      m_dwPlanksAcross = 1;
      m_fPlankSpacing = 0;
      m_Plank.m_fBrightMin = 1;
      m_Plank.m_fBrightMax = 1;
      m_dwPlanksUpDownMax = 1;
      m_dwPlanksUpDownMin = 1;
      m_Plank.m_Tree.m_fRadius = 1;
      m_Plank.m_Tree.m_fLogCenterXOffsetMax = -.5;
      m_Plank.m_Tree.m_fLogCenterXOffsetMax = -.4;
      m_Plank.m_acColors[0] = RGB(0xff, 0xe0, 0x70);
      m_Plank.m_acColors[1] = RGB(0xff, 0xe0, 0x70);
      m_Plank.m_acColors[2] = RGB(0xf0, 0xd0, 0x70);
      m_Plank.m_acColors[3] = RGB(0xe0, 0xc0, 0x60);
      m_Plank.m_acColors[4] = RGB(0xb0,0xa0,0x40);
      m_Plank.m_Noise.m_cMax = m_Plank.m_Noise.m_cMin = RGB(0xe0,0xe0,0x80);
      m_Plank.m_cTransitionColor = RGB(0xb0,0xa0,0x40);
      m_Plank.m_Tree.m_fRingThickness *= 2;
      m_Plank.m_Tree.m_fNumKnotsPerMeter *= 4;
      m_Plank.m_fBevelWidth = 0;
      break;

   case 4:  // painted planking
      m_Plank.m_fBoardBend = 0.005;
      m_dwPlanksAcross = 2;
      m_fPlankSpacing = 0;
      m_Plank.m_fBrightMin = 1;
      m_Plank.m_fBrightMax = 1;
      m_dwPlanksUpDownMax = 1;
      m_dwPlanksUpDownMin = 1;
      m_Plank.m_Tree.m_fRadius = .5;
      m_Plank.m_Tree.m_fLogCenterXOffsetMax = -.2;
      m_Plank.m_Tree.m_fLogCenterXOffsetMax = -.3;
      m_Plank.m_acColors[0] = RGB(0xff, 0xe0, 0x70);
      m_Plank.m_acColors[1] = RGB(0xff, 0xe0, 0x70);
      m_Plank.m_acColors[2] = RGB(0xf0, 0xd0, 0x70);
      m_Plank.m_acColors[3] = RGB(0xe0, 0xc0, 0x60);
      m_Plank.m_acColors[4] = RGB(0xb0,0xa0,0x40);
      m_Plank.m_Noise.m_cMax = m_Plank.m_Noise.m_cMin = RGB(0xe0,0xe0,0x80);
      m_Plank.m_cTransitionColor = RGB(0xb0,0xa0,0x40);
      m_Plank.m_Tree.m_fRingThickness *= 2;
      m_Plank.m_Tree.m_fNumKnotsPerMeter *= 4;
      m_Plank.m_fBevelWidth = .005;
      m_Plank.m_fBevelHeight = .005;
      m_Plank.m_fRingBump = 0.001;

      // paint it
      m_DirtPaint.m_cColor = RGB(0xc0,0xff,0xff);
      m_DirtPaint.m_fAHeight = .1;
      m_DirtPaint.m_fFilterSize = 0;
      m_DirtPaint.m_fTransAtA = 0;
      m_DirtPaint.m_fTransNone = 0;
      m_DirtPaint.m_fTurnOn = TRUE;
      m_DirtPaint.m_wSpecDirection = 1000;
      m_DirtPaint.m_wSpecReflection = 0x4000;
      break;
   }
}

void CTextCreatorWoodPlanks::Delete (void)
{
   delete this;
}


static PWSTR gpszWoodPlanks = L"WoodPlanks";
static PWSTR gpszPlankSpacing = L"PlankSpacing";
static PWSTR gpszPlanksAcross = L"PlanksAcross";
static PWSTR gpszPlanksUpDownMin = L"PlanksUpDownMin";
static PWSTR gpszPlanksUpDownMax = L"PlanksUpDownMax";
static PWSTR gpszSpacingTransparent = L"SpacingTransparent";

PCMMLNode2 CTextCreatorWoodPlanks::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszWoodPlanks);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszPlankSpacing, m_fPlankSpacing);
   MMLValueSet (pNode, gpszSpacing, (int) m_cSpacing);
   MMLValueSet (pNode, gpszSpacingTransparent, (int)m_fSpacingTransparent);
   MMLValueSet (pNode, gpszPlanksAcross, (int)m_dwPlanksAcross);
   MMLValueSet (pNode, gpszPlanksUpDownMin, (int)m_dwPlanksUpDownMin);
   MMLValueSet (pNode, gpszPlanksUpDownMax, (int)m_dwPlanksUpDownMax);

   PCMMLNode2 pSub;
   pSub = m_Plank.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGeneratePlank);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorWoodPlanks::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_fPlankSpacing = MMLValueGetDouble (pNode, gpszPlankSpacing, 0);
   m_cSpacing = (COLORREF) MMLValueGetInt (pNode, gpszSpacing, (int) 0);
   m_fSpacingTransparent = (BOOL) MMLValueGetInt (pNode, gpszSpacingTransparent, 0);
   m_dwPlanksAcross = (DWORD) MMLValueGetInt (pNode, gpszPlanksAcross, (int)0);
   m_dwPlanksUpDownMin = (DWORD) MMLValueGetInt (pNode, gpszPlanksUpDownMin, (int)0);
   m_dwPlanksUpDownMax = (DWORD) MMLValueGetInt (pNode, gpszPlanksUpDownMax, (int)0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGeneratePlank), &psz, &pSub);
   if (pSub)
      m_Plank.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);
   return TRUE;
}

BOOL CTextCreatorWoodPlanks::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // create the surface
   DWORD dwX, dwY;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD)((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD)((m_fPatternHeight + fPixelLen/2) / fPixelLen);

   // number of plans across and space between planks
   DWORD dwPlanksAcross = m_dwPlanksAcross;
   DWORD dwSpaceBetweenPlanks = (DWORD)((m_fPlankSpacing + fPixelLen/2) / fPixelLen);
   DWORD dwPlankWidth = (dwX - dwPlanksAcross * dwSpaceBetweenPlanks) / dwPlanksAcross;
   DWORD dwPlanksUpDownMin = m_dwPlanksUpDownMin;
   DWORD dwPlanksUpDownMax = m_dwPlanksUpDownMax;
   
   DWORD dwPWidth = dwPlankWidth + dwSpaceBetweenPlanks;
   dwX = dwPWidth * dwPlanksAcross;
   dwPlanksUpDownMin = max(1,dwPlanksUpDownMin);
   dwPlanksUpDownMax = max(dwPlanksUpDownMax, dwPlanksUpDownMin);


   // clear the background
   COLORREF cBackColor = m_cSpacing;
   pImage->Init (dwX, dwY, cBackColor, 0);

   // if the spacing is transparent then set that
   if (m_fSpacingTransparent) {
      PIMAGEPIXEL pip = pImage->Pixel(0,0);
      DWORD dwNum;
      for (dwNum = dwX * dwY; dwNum; dwNum--, pip++)
         pip->dwIDPart |= 0xff;
   }

   // make some planks
   DWORD x,i;
   for (x = 0; x < dwPlanksAcross; x++) {
      int iYOffset = (int) randf(0, dwY);

      DWORD dwPlanksUpDown;
      if (dwPlanksUpDownMax > dwPlanksUpDownMin)
         dwPlanksUpDown = (rand() % (dwPlanksUpDownMax - dwPlanksUpDownMin + 1)) + dwPlanksUpDownMin;
      else
         dwPlanksUpDown = dwPlanksUpDownMax;

      int   iLeft;
      iLeft = dwY;
      int iHeight;
      DWORD dwBoardsLeft;
      for (i = 0; i < dwPlanksUpDown; i++) {
         dwBoardsLeft = dwPlanksUpDown - i;
         if (dwBoardsLeft <= 1)
            iHeight = iLeft;
         else
            iHeight = (int) randf(iLeft / dwBoardsLeft * .5, iLeft / dwBoardsLeft * 1.5);
         iHeight = min(iLeft, iHeight);

         // create the plan
         m_Plank.Apply (pImage, fPixelLen, dwPlankWidth,
            (DWORD) iHeight,
            x * dwPWidth + dwSpaceBetweenPlanks,
            iYOffset);

         // move forward
         iYOffset += iHeight;
         iLeft -= iHeight;

      }
   }

   // paint it
   m_DirtPaint.Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}




/***********************************************************************************
CTextCreatorShingles - Noise functions
*/

class CTextCreatorShingles : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorShingles (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in metetrs

   DWORD            m_dwAcrossMin;     // minimum across
   DWORD            m_dwAcrossMax;     // maximum across
   DWORD            m_dwUpDown;        // number up down
   fp               m_fVarWidth;       // variability in width, from 0 to 1
   fp               m_fVarHeight;      // variability in height, from 0 to 1
   fp               m_fShingleThick;   // how thick shingle is, in meters
   BOOL             m_fUseTiles;       // if TRUE then draw with tiles, else wood shingles
   fp               m_fCutCorner;      // amount of corner to cut off, in meters
   fp               m_fHumpHeight;     // height of hump in meters
   fp               m_fHumpWidth;      // width of hump as percent of max, from 0 .. 1
   fp               m_fHumpPersp;      // amount of perspective to apply, from 0..1

   fp               m_fShingleSpacing;    // distance (in meters) between planks
   COLORREF         m_cSpacing;         // coloration of the spacing
   CTextEffectGeneratePlank m_Plank;   // description of a plank
   CTextEffectMakeTile m_Tile;

   CTextEffectDirtPaint m_DirtPaint;        // paint or add dirt
};
typedef CTextCreatorShingles *PCTextCreatorShingles;


/****************************************************************************
ShinglesPage
*/
BOOL ShinglesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorShingles pv = (PCTextCreatorShingles) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);
         MeasureToString (pPage, L"shinglethick", pv->m_fShingleThick, TRUE);
         MeasureToString (pPage, L"humpheight", pv->m_fHumpHeight, TRUE);
         MeasureToString (pPage, L"cutcorner", pv->m_fCutCorner, TRUE);
         DoubleToControl (pPage, L"UpDown", pv->m_dwUpDown);
         DoubleToControl (pPage, L"Acrossmin", pv->m_dwAcrossMin);
         DoubleToControl (pPage, L"Acrossmax", pv->m_dwAcrossMax);
         MeasureToString (pPage, L"ShingleSpacing", pv->m_fShingleSpacing, TRUE);
         FillStatusColor (pPage, L"spacingcolor", pv->m_cSpacing);

         pControl = pPage->ControlFind (L"usetiles");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fUseTiles);

         pControl = pPage->ControlFind (L"varwidth");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarWidth * 100));
         pControl = pPage->ControlFind (L"varheight");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fVarHeight * 100));
         pControl = pPage->ControlFind (L"humpwidth");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHumpWidth * 100));
         pControl = pPage->ControlFind (L"humppersp");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHumpPersp * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"spacingbutton")) {
            pv->m_cSpacing = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSpacing, pPage, L"spacingcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"shinglethick", &pv->m_fShingleThick);
         pv->m_fShingleThick = max(0.001, pv->m_fShingleThick);

         MeasureParseString (pPage, L"humpheight", &pv->m_fHumpHeight);
         pv->m_fHumpHeight = max(0, pv->m_fHumpHeight);

         MeasureParseString (pPage, L"cutcorner", &pv->m_fCutCorner);
         pv->m_fCutCorner = max(0, pv->m_fCutCorner);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwUpDown = (DWORD) DoubleFromControl (pPage, L"UpDown");
         pv->m_dwUpDown = max(1,pv->m_dwUpDown);

         pv->m_dwAcrossMin = (DWORD) DoubleFromControl (pPage, L"Acrossmin");
         pv->m_dwAcrossMin = max(1, pv->m_dwAcrossMin);

         pv->m_dwAcrossMax = (DWORD) DoubleFromControl (pPage, L"Acrossmax");
         pv->m_dwAcrossMax = max(pv->m_dwAcrossMin, pv->m_dwAcrossMax);

         MeasureParseString (pPage, L"ShingleSpacing", &pv->m_fShingleSpacing);
         pv->m_fShingleSpacing = max(0,pv->m_fShingleSpacing);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"usetiles");
         if (pControl)
            pv->m_fUseTiles = pControl->AttribGetBOOL (Checked());

         pControl = pPage->ControlFind (L"varwidth");
         if (pControl)
            pv->m_fVarWidth = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"varheight");
         if (pControl)
            pv->m_fVarHeight = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"humpwidth");
         if (pControl)
            pv->m_fHumpWidth = (fp)pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"humppersp");
         if (pControl)
            pv->m_fHumpPersp = (fp)pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Shingles";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorShingles::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURESHINGLES, ShinglesPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"plank")) {
      if (m_Plank.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   if (pszRet && !_wcsicmp(pszRet, L"tile")) {
      if (m_Tile.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorShingles::CTextCreatorShingles (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTMATTE);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = m_fShingleSpacing = 0;
   m_fShingleThick = .01;
   m_fHumpHeight = 0;
   m_fHumpWidth = .5;
   m_fHumpPersp = .5;
   m_fCutCorner = 0;
   m_fVarWidth = m_fVarHeight = 0;
   m_cSpacing = 0;
   m_dwUpDown = m_dwAcrossMin = m_dwAcrossMax = 0;
   m_fUseTiles = FALSE;

   m_iSeed = 1234;
   m_fPixelLen = .005;
   m_fPatternWidth = 1;
   m_fPatternHeight = .5;
   m_dwUpDown = 4;
   m_dwAcrossMin = 6;
   m_dwAcrossMax = 6;
   m_fShingleSpacing = 0.01;
   m_cSpacing = 0;


   m_Plank.m_fBevelHeight = .005;
   m_Plank.m_fBevelWidth = .003;
   m_Plank.m_fBoardBend = 0;
   m_Plank.m_wSpecReflection = 0x2000;
   m_Plank.m_wSpecDirection = 5000;
   m_Plank.m_acColors[0] = RGB(0xd0, 0xb0, 0x40);
   m_Plank.m_acColors[1] = RGB(0xd0, 0xb0, 0x30);
   m_Plank.m_acColors[2] = RGB(0xb0, 0x90, 0x30);
   m_Plank.m_acColors[3] = RGB(0x90, 0x70, 0x00);
   m_Plank.m_acColors[4] = RGB(0x50,0x30,0);
   m_Plank.m_cTransitionColor = RGB(0x50,0x20,0);
   m_Plank.m_fRingBump = .001;
   m_Plank.m_fBrightMin = .9;
   m_Plank.m_fBrightMax = 1.2;

   m_Plank.m_Noise.m_fTurnOn = TRUE;
   m_Plank.m_Noise.m_fNoiseX = .05;
   m_Plank.m_Noise.m_fNoiseY = .05;
   m_Plank.m_Noise.m_cMax = RGB(0xb0,0x70,0x30);
   m_Plank.m_Noise.m_cMin = RGB(0xb0,0x80,0x30);
   m_Plank.m_Noise.m_fTransMax = .7;
   m_Plank.m_Noise.m_fTransMin = 1;

   m_Plank.m_Tree.m_fRadius = .3;
   m_Plank.m_Tree.m_fLogCenterXOffsetMin = -.1;
   m_Plank.m_Tree.m_fLogCenterXOffsetMax = .1;
   m_Plank.m_Tree.m_fNumKnotsPerMeter = 20;
   m_Plank.m_Tree.m_fKnotWidthMin = .005;
   m_Plank.m_Tree.m_fKnotWidthMax = .03;
   m_Plank.m_Tree.m_fRingThickness = .001;
   m_Plank.m_Tree.m_aNoise[0].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseX = .03;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseY = .12;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMin = .006;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMin = 1;

   m_Plank.m_Tree.m_aNoise[1].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseX = .01;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseY = .04;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMin = .001;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMin = 1;

   m_Tile.m_fBevel = 0.01;
   m_Tile.m_fThickness = 0.005;
   m_Tile.m_fHeight = .30;
   m_Tile.m_fWidth = .30;
   m_Tile.m_fRockLR = .001;
   m_Tile.m_fRockTB = .001;
   m_Tile.m_cSurfA = RGB(0x40,0x40,0xff);
   m_Tile.m_cSurfB = RGB(0x40,0x40,0xff);
   m_Tile.m_dwBevelMode = 2;
   m_Tile.m_wSpecReflection = 0x4000;
   m_Tile.m_wSpecDirection = 4000;

   m_Tile.m_aNoise[0].m_fTurnOn = FALSE;
   m_Tile.m_aNoise[0].m_fNoiseX = .1;
   m_Tile.m_aNoise[0].m_fNoiseY = .1;
   m_Tile.m_aNoise[0].m_cMax = RGB(0x80, 0xff, 0x80);
   m_Tile.m_aNoise[0].m_cMin = RGB(0xff, 0x80, 0x80);
   m_Tile.m_aNoise[0].m_fTransMax = .6;
   m_Tile.m_aNoise[0].m_fTransMin = 1;

   m_Tile.m_aNoise[1].m_fTurnOn = FALSE;
   m_Tile.m_aNoise[1].m_fNoiseX = .025;
   m_Tile.m_aNoise[1].m_fNoiseY = .025;
   m_Tile.m_aNoise[1].m_cMax = RGB(0x80, 0x80, 0xff);
   m_Tile.m_aNoise[1].m_cMin = RGB(0x80, 0xff, 0x80);
   m_Tile.m_aNoise[1].m_fTransMax = .6;
   m_Tile.m_aNoise[1].m_fTransMin = 1;

   m_Tile.m_aNoise[2].m_fTurnOn = TRUE;
   m_Tile.m_aNoise[2].m_fNoiseX = .1;
   m_Tile.m_aNoise[2].m_fNoiseY = .1;
   m_Tile.m_aNoise[2].m_fZDeltaMax = 0;
   m_Tile.m_aNoise[2].m_fZDeltaMin = .001;
   m_Tile.m_aNoise[2].m_fTransMax = 1;
   m_Tile.m_aNoise[2].m_fTransMin = 1;

   m_Tile.m_aNoise[3].m_fTurnOn = TRUE;
   m_Tile.m_aNoise[3].m_fNoiseX = .05;
   m_Tile.m_aNoise[3].m_fNoiseY = .05;
   m_Tile.m_aNoise[3].m_fZDeltaMax = 0;
   m_Tile.m_aNoise[3].m_fZDeltaMin = .001;
   m_Tile.m_aNoise[3].m_fTransMax = 1;
   m_Tile.m_aNoise[3].m_fTransMin = 1;

   m_Tile.m_aNoise[4].m_fTurnOn = TRUE;
   m_Tile.m_aNoise[4].m_fNoiseX = .01;
   m_Tile.m_aNoise[4].m_fNoiseY = .01;
   m_Tile.m_aNoise[4].m_fZDeltaMax = 0;
   m_Tile.m_aNoise[4].m_fZDeltaMin = .0005;
   m_Tile.m_aNoise[4].m_fTransMax = 1;
   m_Tile.m_aNoise[4].m_fTransMin = 1;

   m_Tile.m_Chip.m_fTurnOn = FALSE;
   m_Tile.m_Chip.m_fTileHeight = m_Tile.m_fThickness;
   m_Tile.m_Chip.m_fFilterSize = .015;
   m_Tile.m_Chip.m_cChip = RGB(0xb0,0xb0,0xb0);
   m_Tile.m_Chip.m_fChipTrans = 0;
   m_Tile.m_Chip.m_wSpecDirection = 100;
   m_Tile.m_Chip.m_wSpecReflection = 0x500;
   m_Tile.m_Chip.m_aNoise[0].m_fTurnOn = TRUE;
   m_Tile.m_Chip.m_aNoise[0].m_fNoiseX = .025;
   m_Tile.m_Chip.m_aNoise[0].m_fNoiseY = .025;
   m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[0].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[0].m_fTransMax = 1;
   m_Tile.m_Chip.m_aNoise[0].m_fTransMin = 1;

   m_Tile.m_Chip.m_aNoise[1].m_fTurnOn = TRUE;
   m_Tile.m_Chip.m_aNoise[1].m_fNoiseX = .01;
   m_Tile.m_Chip.m_aNoise[1].m_fNoiseY = .01;
   m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[1].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/2;
   m_Tile.m_Chip.m_aNoise[1].m_fTransMax = 1;
   m_Tile.m_Chip.m_aNoise[1].m_fTransMin = 1;

   m_Tile.m_Chip.m_aNoise[2].m_fTurnOn = TRUE;
   m_Tile.m_Chip.m_aNoise[2].m_fNoiseX = .005;
   m_Tile.m_Chip.m_aNoise[2].m_fNoiseY = .005;
   m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMax = m_Tile.m_Chip.m_fTileHeight/4;
   m_Tile.m_Chip.m_aNoise[2].m_fZDeltaMin = -m_Tile.m_Chip.m_fTileHeight/4;
   m_Tile.m_Chip.m_aNoise[2].m_fTransMax = 1;
   m_Tile.m_Chip.m_aNoise[2].m_fTransMin = 1;

}

void CTextCreatorShingles::Delete (void)
{
   delete this;
}


static PWSTR gpszShingles = L"Shingles";
static PWSTR gpszShingleSpacing = L"ShingleSpacing";
static PWSTR gpszUpDown = L"UpDown";
static PWSTR gpszAcrossMin = L"AcrossMin";
static PWSTR gpszAcrossMax = L"AcrossMax";
static PWSTR gpszShingleThick = L"ShingleThick";
static PWSTR gpszCutCorner = L"CutCorner";
static PWSTR gpszHumpHeight = L"HumpHeight";
static PWSTR gpszHumpPersp = L"HumpPersp";
static PWSTR gpszHumpWidth = L"HumpWidth";

PCMMLNode2 CTextCreatorShingles::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszShingles);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszShingleThick, m_fShingleThick);
   MMLValueSet (pNode, gpszCutCorner, m_fCutCorner);
   MMLValueSet (pNode, gpszVarWidth, m_fVarWidth);
   MMLValueSet (pNode, gpszVarHeight, m_fVarHeight);
   MMLValueSet (pNode, gpszShingleSpacing, m_fShingleSpacing);
   MMLValueSet (pNode, gpszSpacing, (int) m_cSpacing);
   MMLValueSet (pNode, gpszUpDown, (int)m_dwUpDown);
   MMLValueSet (pNode, gpszAcrossMin, (int)m_dwAcrossMin);
   MMLValueSet (pNode, gpszAcrossMax, (int)m_dwAcrossMax);
   MMLValueSet (pNode, gpszUseTiles, (int)m_fUseTiles);
   MMLValueSet (pNode, gpszHumpHeight, m_fHumpHeight);
   MMLValueSet (pNode, gpszHumpPersp, m_fHumpPersp);
   MMLValueSet (pNode, gpszHumpWidth, m_fHumpWidth);

   PCMMLNode2 pSub;
   pSub = m_Plank.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGeneratePlank);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Tile.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMakeTile);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorShingles::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_fShingleThick = MMLValueGetDouble (pNode, gpszShingleThick, 0);
   m_fCutCorner = MMLValueGetDouble (pNode, gpszCutCorner, 0);
   m_fVarWidth = MMLValueGetDouble (pNode, gpszVarWidth, 0);
   m_fVarHeight = MMLValueGetDouble (pNode, gpszVarHeight, 0);
   m_fShingleSpacing = MMLValueGetDouble (pNode, gpszShingleSpacing, 0);
   m_cSpacing = (COLORREF) MMLValueGetInt (pNode, gpszSpacing, (int) 0);
   m_dwUpDown = (DWORD) MMLValueGetInt (pNode, gpszUpDown, (int)0);
   m_dwAcrossMin = (DWORD) MMLValueGetInt (pNode, gpszAcrossMin, (int)0);
   m_dwAcrossMax = (DWORD) MMLValueGetInt (pNode, gpszAcrossMax, (int)0);
   m_fUseTiles = (BOOL) MMLValueGetInt (pNode, gpszUseTiles, 0);
   m_fHumpHeight = MMLValueGetDouble (pNode, gpszHumpHeight, 0);
   m_fHumpPersp = MMLValueGetDouble (pNode, gpszHumpPersp, .2);
   m_fHumpWidth = MMLValueGetDouble (pNode, gpszHumpWidth, .1);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGeneratePlank), &psz, &pSub);
   if (pSub)
      m_Plank.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszMakeTile), &psz, &pSub);
   if (pSub)
      m_Tile.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);
   return TRUE;
}

BOOL CTextCreatorShingles::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // create the surface
   DWORD dwX, dwY;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD)((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD)((m_fPatternHeight + fPixelLen/2) / fPixelLen);

   // number of plans across and space between planks
   DWORD dwHeight;
   m_dwUpDown = (m_dwUpDown + 1) & ~(0x01);  // even align
   dwHeight = (DWORD) (dwY / m_dwUpDown);
   dwHeight = max(1,dwHeight);
   dwY = dwHeight * m_dwUpDown;

   // clear the background
   COLORREF cBackColor = m_cSpacing;
   pImage->Init (dwX, dwY, cBackColor, 0);

   // make some planks
   DWORD y,i, dwCorner;
   CListFixed lLoc;
   lLoc.Init (sizeof(fp));
   CImage ITemp;
   fp fAngle;
   fAngle = (m_fShingleThick / fPixelLen );
   dwCorner = (DWORD) (m_fCutCorner / fPixelLen);
   for (y = 0; y < m_dwUpDown; y++) {

      // shingles across
      DWORD dwAcross;
      if (m_dwAcrossMax > m_dwAcrossMin)
         dwAcross = (rand() % (m_dwAcrossMax - m_dwAcrossMin+1)) + m_dwAcrossMin;
      else
         dwAcross = m_dwAcrossMax;

      // figure out where they are
      lLoc.Clear();
      fp f;
      lLoc.Required (dwAcross);
      for (i = 0; i < dwAcross; i++) {
         f = (fp) i / (fp) dwAcross;

         if (y % 2)
            f += .5 / (fp)dwAcross;

         f += randf(m_fVarWidth / (fp)dwAcross / 2.01, -m_fVarWidth / (fp)dwAcross / 2.01);
         f *= (fp)dwX;
         lLoc.Add (&f);
      }

      // loop through them
      for (i = 0; i < dwAcross; i++) {
         fp fLeft, fRight;
         fLeft = *((fp*) lLoc.Get(i));
         fRight = *((fp*) lLoc.Get((i+1)%lLoc.Num()));
         if (fRight < fLeft)
            fRight += dwX;

         // spacing
         fLeft += m_fShingleSpacing / 2 / fPixelLen;
         

         // width
         int iWidth;
         iWidth = (int) (fRight - fLeft);
         if (iWidth < 1)
            continue;   // dont bother with this one

         // height is always the same, but location for top varies
         int iTop;
         iTop = (int) (((fp)y + randf(m_fVarHeight/2.01, -m_fVarHeight/2.01)) * (fp)dwHeight);

         // create temporary buffer and draw into that
         if (!ITemp.Init ((DWORD)iWidth, dwHeight*2, cBackColor, -0x80000))
            continue;

         // To tiles
         if (m_fUseTiles) {
            m_Tile.m_fWidth = (fp) ITemp.Width() * fPixelLen;
            m_Tile.m_fHeight = (fp) ITemp.Height() * fPixelLen;
            m_Tile.Apply (&ITemp, fPixelLen, 0, 0);
         }
         else
            m_Plank.Apply (&ITemp, fPixelLen, ITemp.Width(), ITemp.Height(), 0, 0);

         // humps
         DWORD xx,yy;
         if (m_fHumpHeight > 0) {
            int iWidth = (int)(m_fHumpWidth / 2.0 * (fp) ITemp.Width());
            iWidth = max(1,iWidth);

            // loop
            for (xx = 0; xx < ITemp.Width(); xx++) {
               // how much to adjust this row?
               fp fScale;
               fScale = xx % (ITemp.Width()/2);
               if (fScale >= iWidth)
                  continue;   // nothing to change
               fScale = fScale / (fp) iWidth * 2 - 1;
               fScale = sqrt (1.0 - fScale * fScale);
               //fScale = sin (fScale / (fp) iWidth * PI);

               // height and perspective offset
               fp fHeight;
               int iPersp;
               //iHeight = (int) (m_fHumpHeight / fPixelLen * (fp)0x10000 * fScale);
               fHeight = (m_fHumpHeight / fPixelLen * fScale);
               iPersp = (int) (m_fHumpHeight / fPixelLen * m_fHumpPersp * fScale);

               // increase the heights
               for (yy = 0; yy < ITemp.Height(); yy++) {
                  PIMAGEPIXEL pip = ITemp.Pixel(xx, yy);
                  if (pip->fZ > 0)
                     pip->fZ += fHeight;
               }

               // apply perspective?
               if (!iPersp)
                  continue;   // no bother
               for (yy = 0; yy+(DWORD)iPersp < ITemp.Height(); yy++)
                  *(ITemp.Pixel(xx,yy)) = *(ITemp.Pixel(xx,yy+(DWORD)iPersp));
               for (yy = ITemp.Height() - (DWORD)iPersp; yy < ITemp.Height(); yy++)
                  ITemp.Pixel(xx,yy)->fZ = -ZINFINITE;   // BUGFIX - Was -0x80000;
            }
         }

         // angle it
         fp fTempAngle;
         for (yy = 0; yy < ITemp.Height(); yy++) {
            fTempAngle = fAngle / (fp) ITemp.Height() * (fp) yy;
            PIMAGEPIXEL pip = ITemp.Pixel(0, yy);
            for (xx = 0; xx < ITemp.Width(); xx++, pip++)
               if (pip->fZ > 0)
                  pip->fZ += fTempAngle;
         }

         // cut off corner
         if (dwCorner) {
            PIMAGEPIXEL pip;
            for (xx = 0; xx < dwCorner; xx++) for (yy = 0; yy+xx < dwCorner; yy++) {
               pip = ITemp.Pixel(ITemp.Width()-xx-1,ITemp.Height()-yy-1);
               pip->fZ = -ZINFINITE; // BUGFIX - Was -0x10000;
               pip = ITemp.Pixel(xx,ITemp.Height()-yy-1);
               pip->fZ = -ZINFINITE; // BUGFIX - Was -0x10000;
            }
         }

         // merge it in
         POINT pDest;
         pDest.x = (int) fLeft;
         pDest.y = iTop;
         ITemp.TGMerge (NULL, pImage, pDest);
      }
   } // over y

   // paint it
   m_DirtPaint.Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}




/***********************************************************************************
CTextCreatorBoardBatten - Noise functions
*/

class CTextCreatorBoardBatten : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorBoardBatten (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in metetrs
   DWORD            m_dwPlanksAcross;   // number of planks across
   fp               m_fBattenWidth; // how wide batten is
   fp               m_fBattenDepth; // how deep batten is
   CTextEffectGeneratePlank m_Plank;   // description of a plank
   CTextEffectDirtPaint m_DirtPaint;        // paint or add dirt
};
typedef CTextCreatorBoardBatten *PCTextCreatorBoardBatten;


/****************************************************************************
BoardBattenPage
*/
BOOL BoardBattenPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorBoardBatten pv = (PCTextCreatorBoardBatten) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);
         MeasureToString (pPage, L"battenwidth", pv->m_fBattenWidth, TRUE);
         MeasureToString (pPage, L"battendepth", pv->m_fBattenDepth, TRUE);
         DoubleToControl (pPage, L"planksacross", pv->m_dwPlanksAcross);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"battenwidth", &pv->m_fBattenWidth);
         pv->m_fBattenWidth = max(.001, pv->m_fBattenWidth);

         MeasureParseString (pPage, L"battendepth", &pv->m_fBattenDepth);
         pv->m_fBattenDepth = max(.001, pv->m_fBattenDepth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwPlanksAcross = (DWORD) DoubleFromControl (pPage, L"planksacross");
         pv->m_dwPlanksAcross = max(1,pv->m_dwPlanksAcross);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Board and batten";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorBoardBatten::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREBOARDBATTEN, BoardBattenPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"plank")) {
      if (m_Plank.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorBoardBatten::CTextCreatorBoardBatten (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;
   m_fBattenWidth = .05;
   m_fBattenDepth = .025;
   m_dwPlanksAcross = 0;

   m_iSeed = 1234;
   m_fPixelLen = .005;
   m_fPatternWidth = .5;
   m_fPatternHeight = 2;
   m_dwPlanksAcross = 2;


   m_Plank.m_fBevelHeight = .005;
   m_Plank.m_fBevelWidth = .003;
   m_Plank.m_fBoardBend = 0;
   m_Plank.m_wSpecReflection = 0x2000;
   m_Plank.m_wSpecDirection = 5000;
   m_Plank.m_acColors[0] = RGB(0xd0, 0xb0, 0x40);
   m_Plank.m_acColors[1] = RGB(0xd0, 0xb0, 0x30);
   m_Plank.m_acColors[2] = RGB(0xb0, 0x90, 0x30);
   m_Plank.m_acColors[3] = RGB(0x90, 0x70, 0x00);
   m_Plank.m_acColors[4] = RGB(0x50,0x30,0);
   m_Plank.m_cTransitionColor = RGB(0x50,0x20,0);
   m_Plank.m_fRingBump = .001;
   m_Plank.m_fBrightMin = .9;
   m_Plank.m_fBrightMax = 1.2;

   m_Plank.m_Noise.m_fTurnOn = TRUE;
   m_Plank.m_Noise.m_fNoiseX = .05;
   m_Plank.m_Noise.m_fNoiseY = .05;
   m_Plank.m_Noise.m_cMax = RGB(0xb0,0x70,0x30);
   m_Plank.m_Noise.m_cMin = RGB(0xb0,0x80,0x30);
   m_Plank.m_Noise.m_fTransMax = .7;
   m_Plank.m_Noise.m_fTransMin = 1;

   m_Plank.m_Tree.m_fRadius = .3;
   m_Plank.m_Tree.m_fLogCenterXOffsetMin = -.1;
   m_Plank.m_Tree.m_fLogCenterXOffsetMax = .1;
   m_Plank.m_Tree.m_fNumKnotsPerMeter = 20;
   m_Plank.m_Tree.m_fKnotWidthMin = .005;
   m_Plank.m_Tree.m_fKnotWidthMax = .03;
   m_Plank.m_Tree.m_fRingThickness = .001;
   m_Plank.m_Tree.m_aNoise[0].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseX = .03;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseY = .12;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMin = .006;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMin = 1;

   m_Plank.m_Tree.m_aNoise[1].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseX = .01;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseY = .04;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMin = .001;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMin = 1;

}

void CTextCreatorBoardBatten::Delete (void)
{
   delete this;
}


static PWSTR gpszBoardBatten = L"BoardBatten";
static PWSTR gpszBattenWidth = L"BattenWidth";
static PWSTR gpszBattenDepth = L"BattenDepth";

PCMMLNode2 CTextCreatorBoardBatten::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszBoardBatten);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszBattenWidth, m_fBattenWidth);
   MMLValueSet (pNode, gpszBattenDepth, m_fBattenDepth);
   MMLValueSet (pNode, gpszPlanksAcross, (int)m_dwPlanksAcross);

   PCMMLNode2 pSub;
   pSub = m_Plank.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGeneratePlank);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorBoardBatten::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_fBattenWidth = MMLValueGetDouble (pNode, gpszBattenWidth, 0.01);
   m_fBattenDepth = MMLValueGetDouble (pNode, gpszBattenDepth, 0.01);
   m_dwPlanksAcross = (DWORD) MMLValueGetInt (pNode, gpszPlanksAcross, (int)0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGeneratePlank), &psz, &pSub);
   if (pSub)
      m_Plank.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);
   return TRUE;
}

BOOL CTextCreatorBoardBatten::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // create the surface
   DWORD dwX, dwY;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD)((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD)((m_fPatternHeight + fPixelLen/2) / fPixelLen);

   // number of plans across and space between planks
   m_dwPlanksAcross = max(1,m_dwPlanksAcross);
   DWORD dwPlanksAcross = m_dwPlanksAcross;
   DWORD dwPlankWidth = dwX / dwPlanksAcross;
   
   DWORD dwPWidth = dwPlankWidth;
   dwX = dwPWidth * dwPlanksAcross;


   // clear the background
   pImage->Init (dwX, dwY, RGB(0,0,0), 0);

   // make some planks
   DWORD x,i;
   CImage ITemp;
   DWORD dwBatten;
   fp fBattenZ;
   dwBatten = m_fBattenWidth / fPixelLen;
   dwBatten = max(1,dwBatten);
   fBattenZ = (m_fBattenDepth / fPixelLen);
   for (x = 0; x < dwPlanksAcross; x++) {
      int iYOffset = (int) randf(0, dwY);

      // create the plan
      m_Plank.Apply (pImage, fPixelLen, dwPlankWidth, dwY,
         x * dwPWidth, iYOffset);

      // batten
      iYOffset = (int) randf(0, dwY);
      if (!ITemp.Init (dwBatten, dwY, 0, 0))
         continue;
      m_Plank.Apply (&ITemp, fPixelLen, dwBatten, dwY, 0, 0);

      // raise batter
      PIMAGEPIXEL pip;
      pip = ITemp.Pixel(0,0);
      for (i = 0; i < dwBatten * dwY; i++, pip++)
         pip->fZ += fBattenZ;
      POINT pDest;
      pDest.x = (int)(x*dwPWidth) - (int)dwBatten/2;
      pDest.y = iYOffset;
      ITemp.TGMerge (NULL, pImage, pDest);
   }

   // paint it
   m_DirtPaint.Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}




/***********************************************************************************
CTextCreatorClapboards - Noise functions
*/

class CTextCreatorClapboards : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorClapboards (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in metetrs
   fp               m_fPlankSpacing;    // distance (in meters) between planks
   COLORREF         m_cSpacing;         // coloration of the spacing
   BOOL             m_fSpacingTransparent;   // if TRUE, spacing between boards is transparent
   DWORD            m_dwPlanksUpDown;   // minimum number of plansk up and down
   DWORD            m_dwShape;      // 0 for clapboard, 1 for log
   fp               m_fPlankDepth;     // depth of the plank
   fp               m_fExtraShading;   // how much extra shading to apply
   CTextEffectGeneratePlank m_Plank;   // description of a plank
   CTextEffectDirtPaint m_DirtPaint;        // paint or add dirt
};
typedef CTextCreatorClapboards *PCTextCreatorClapboards;


/****************************************************************************
ClapboardsPage
*/
BOOL ClapboardsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorClapboards pv = (PCTextCreatorClapboards) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"plankdepth", pv->m_fPlankDepth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);
         DoubleToControl (pPage, L"planksupdown", pv->m_dwPlanksUpDown);
         MeasureToString (pPage, L"plankspacing", pv->m_fPlankSpacing, TRUE);
         FillStatusColor (pPage, L"spacingcolor", pv->m_cSpacing);

         pControl = pPage->ControlFind (L"extrashading");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fExtraShading * 100));

         ComboBoxSet (pPage, L"shape", pv->m_dwShape);

         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSpacingTransparent);
      }
      break;

   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         int iVal;
         iVal = p->pControl->AttribGetInt (Pos());
         fp fVal = (fp)iVal / 100.0;
         if (!_wcsicmp(p->pControl->m_pszName, L"extrashading")) {
            pv->m_fExtraShading = fVal;
            return TRUE;
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"spacingbutton")) {
            pv->m_cSpacing = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSpacing, pPage, L"spacingcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shape")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwShape)
               return TRUE; // unchanged
            pv->m_dwShape = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"plankdepth", &pv->m_fPlankDepth);
         pv->m_fPlankDepth = max(0, pv->m_fPlankDepth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwPlanksUpDown = (DWORD) DoubleFromControl (pPage, L"planksupdown");
         pv->m_dwPlanksUpDown = max(1, pv->m_dwPlanksUpDown);

         MeasureParseString (pPage, L"plankspacing", &pv->m_fPlankSpacing);
         pv->m_fPlankSpacing = max(0,pv->m_fPlankSpacing);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pv->m_fSpacingTransparent = pControl->AttribGetBOOL (Checked());
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Clapboards";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorClapboards::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURECLAPBOARDS, ClapboardsPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"plank")) {
      if (m_Plank.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorClapboards::CTextCreatorClapboards (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = m_fPlankSpacing = 0;
   m_fPlankDepth = .02;
   m_fExtraShading = 0.5;  // half
   m_cSpacing = 0;
   m_fSpacingTransparent = FALSE;
   m_dwPlanksUpDown = 0;
   m_dwShape = 0;


   m_iSeed = 1234;
   m_fPixelLen = .005;
   m_fPatternWidth = 1;
   m_fPatternHeight = .5;
   m_dwPlanksUpDown = 4;
   m_fPlankSpacing = 0;
   m_cSpacing = 0;


   m_Plank.m_fBevelHeight = .005;
   m_Plank.m_fBevelWidth = 0;
   m_Plank.m_fBoardBend = 0;
   m_Plank.m_wSpecReflection = 0x2000;
   m_Plank.m_wSpecDirection = 5000;
   m_Plank.m_acColors[0] = RGB(0xd0, 0xb0, 0x40);
   m_Plank.m_acColors[1] = RGB(0xd0, 0xb0, 0x30);
   m_Plank.m_acColors[2] = RGB(0xb0, 0x90, 0x30);
   m_Plank.m_acColors[3] = RGB(0x90, 0x70, 0x00);
   m_Plank.m_acColors[4] = RGB(0x50,0x30,0);
   m_Plank.m_cTransitionColor = RGB(0x50,0x20,0);
   m_Plank.m_fRingBump = .001;
   m_Plank.m_fBrightMin = .9;
   m_Plank.m_fBrightMax = 1.2;

   m_Plank.m_Noise.m_fTurnOn = TRUE;
   m_Plank.m_Noise.m_fNoiseX = .05;
   m_Plank.m_Noise.m_fNoiseY = .05;
   m_Plank.m_Noise.m_cMax = RGB(0xb0,0x70,0x30);
   m_Plank.m_Noise.m_cMin = RGB(0xb0,0x80,0x30);
   m_Plank.m_Noise.m_fTransMax = .7;
   m_Plank.m_Noise.m_fTransMin = 1;

   m_Plank.m_Tree.m_fRadius = .3;
   m_Plank.m_Tree.m_fLogCenterXOffsetMin = -.1;
   m_Plank.m_Tree.m_fLogCenterXOffsetMax = .1;
   m_Plank.m_Tree.m_fNumKnotsPerMeter = 20;
   m_Plank.m_Tree.m_fKnotWidthMin = .005;
   m_Plank.m_Tree.m_fKnotWidthMax = .03;
   m_Plank.m_Tree.m_fRingThickness = .001;
   m_Plank.m_Tree.m_aNoise[0].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseX = .03;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseY = .12;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMin = .006;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMin = 1;

   m_Plank.m_Tree.m_aNoise[1].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseX = .01;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseY = .04;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMin = .001;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMin = 1;
}

void CTextCreatorClapboards::Delete (void)
{
   delete this;
}


static PWSTR gpszClapboards = L"Clapboards";
static PWSTR gpszPlanksUpDown = L"PlanksUpDown";
static PWSTR gpszPlankDepth = L"plankdepth";
static PWSTR gpszExtraShading = L"ExtraShading";

PCMMLNode2 CTextCreatorClapboards::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszClapboards);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszPlankSpacing, m_fPlankSpacing);
   MMLValueSet (pNode, gpszSpacing, (int) m_cSpacing);
   MMLValueSet (pNode, gpszSpacingTransparent, (int)m_fSpacingTransparent);
   MMLValueSet (pNode, gpszPlanksUpDown, (int)m_dwPlanksUpDown);
   MMLValueSet (pNode, gpszPlankDepth, m_fPlankDepth);
   MMLValueSet (pNode, gpszExtraShading, m_fExtraShading);
   MMLValueSet (pNode, gpszShape, (int) m_dwShape);

   PCMMLNode2 pSub;
   pSub = m_Plank.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGeneratePlank);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorClapboards::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_fPlankSpacing = MMLValueGetDouble (pNode, gpszPlankSpacing, 0);
   m_cSpacing = (COLORREF) MMLValueGetInt (pNode, gpszSpacing, (int) 0);
   m_fSpacingTransparent = (BOOL) MMLValueGetInt (pNode, gpszSpacingTransparent, 0);
   m_dwPlanksUpDown = (DWORD) MMLValueGetInt (pNode, gpszPlanksUpDown, (int)3);
   m_fPlankDepth = MMLValueGetDouble (pNode, gpszPlankDepth, .01);
   m_fExtraShading = MMLValueGetDouble (pNode, gpszExtraShading, 0.5);
   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGeneratePlank), &psz, &pSub);
   if (pSub)
      m_Plank.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);
   return TRUE;
}

BOOL CTextCreatorClapboards::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // create the surface
   DWORD dwX, dwY;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD)((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD)((m_fPatternHeight + fPixelLen/2) / fPixelLen);

   // number of plans across and space between planks
   m_dwPlanksUpDown = max(m_dwPlanksUpDown, 1);
   DWORD dwSpaceBetweenPlanks = (DWORD)(m_fPlankSpacing / fPixelLen);
   DWORD dwPlankWidth = (dwY - m_dwPlanksUpDown * dwSpaceBetweenPlanks) / m_dwPlanksUpDown;
   
   DWORD dwPWidth = dwPlankWidth + dwSpaceBetweenPlanks;
   dwY = dwPWidth * m_dwPlanksUpDown;


   // clear the background
   COLORREF cBackColor = m_cSpacing;
   pImage->Init (dwX, dwY, cBackColor, -ZINFINITE);

   // if the spacing is transparent then set that
   if (m_fSpacingTransparent) {
      PIMAGEPIXEL pip = pImage->Pixel(0,0);
      DWORD dwNum;
      for (dwNum = dwX * dwY; dwNum; dwNum--, pip++)
         pip->dwIDPart |= 0xff;
   }

   // make some planks
   DWORD x, i;
   CImage ITemp;
   fp   fAngle;
   fAngle = (m_fPlankDepth / fPixelLen);
   for (x = 0; x < m_dwPlanksUpDown; x++) {
      int iXOffset = (int) randf(0, dwX);

      if (!ITemp.Init (dwPlankWidth, dwX, cBackColor, -ZINFINITE))
         continue;

      // create the plank
      m_Plank.Apply (&ITemp, fPixelLen, dwPlankWidth, dwX, 0, 0);

      // BUGFIX - Apply dirt and paint here so shading will work
      // paint it
      m_DirtPaint.Apply (&ITemp, fPixelLen);

      // do Z adjust
      DWORD xx, yy;
      fp fShading;
      switch (m_dwShape) {
      default:
      case 0:  // clapboards
         for (yy = 0; yy < ITemp.Height(); yy++) for (xx = 0; xx < ITemp.Width(); xx++) {
            PIMAGEPIXEL pip = ITemp.Pixel(xx,yy);

            pip->fZ += fAngle / (fp) ITemp.Width() * (fp) (ITemp.Width() - xx);

            fShading = 1.0 - (fp)xx / (fp)ITemp.Width();
            fShading = sqrt(fShading);
            fShading = 1.0 - (1.0 - fShading) * m_fExtraShading;
            for (i = 0; i < 3; i++) // over colors
               (&pip->wRed)[i] = (WORD)(fShading * (fp)((&pip->wRed)[i]));
         }
         break;
      case 1:  // log
         for (xx = 0; xx < ITemp.Width(); xx++) {
            fp fVal;
            fp fElev;
            fVal = (fp) xx / (fp)ITemp.Width() * 2.0 - 1;
            fVal = sqrt(1.0 - fVal * fVal);
            fElev = (fVal * fAngle);   // BUGFIX - Changed from iZ to fZ

            fShading = -(fp)xx / (fp)ITemp.Width() * 2.0 + 1.0;
            if (fShading > 0) {
               fShading = 1.0 - fShading;
               fShading = 1.0 - (1.0 - fShading) * m_fExtraShading;
            }
            else
               fShading = 1;

            for (yy = 0; yy < ITemp.Height(); yy++) {
               PIMAGEPIXEL pip = ITemp.Pixel(xx,yy);
               pip->fZ += fElev;

               for (i = 0; i < 3; i++) // over colors
                  (&pip->wRed)[i] = (WORD)(fShading * (fp)((&pip->wRed)[i]));
            }
         }
         break;
      }

      // merge and rotate
      POINT pDest;
      pDest.x = iXOffset;
      pDest.y = (x+1) * dwPWidth + dwSpaceBetweenPlanks;
      ITemp.TGMergeRotate (NULL, pImage, pDest);
   }

   // BUGFIX - Moved apply dirt/paint above
   // paint it
   //m_DirtPaint.Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}






/***********************************************************************************
CTextCreatorLattice - Noise functions
*/

class CTextCreatorLattice : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorLattice (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in metetrs
   fp               m_fPlankWidth;  // width of plank in meters
   DWORD            m_dwPatternSize;   // number of repeats in horizontal and vertical
   COLORREF         m_cSpacing;         // coloration of the spacing
   BOOL             m_fSpacingTransparent;   // if TRUE, spacing between boards is transparent
   fp               m_fPlankDepth;     // depth of the plank
   CTextEffectGeneratePlank m_Plank;   // description of a plank
   CTextEffectDirtPaint m_DirtPaint;        // paint or add dirt
};
typedef CTextCreatorLattice *PCTextCreatorLattice;


/****************************************************************************
LatticePage
*/
BOOL LatticePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorLattice pv = (PCTextCreatorLattice) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"plankdepth", pv->m_fPlankDepth, TRUE);
         MeasureToString (pPage, L"plankwidth", pv->m_fPlankWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);
         DoubleToControl (pPage, L"patternsize", pv->m_dwPatternSize);
         FillStatusColor (pPage, L"spacingcolor", pv->m_cSpacing);

         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSpacingTransparent);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"spacingbutton")) {
            pv->m_cSpacing = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSpacing, pPage, L"spacingcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"plankdepth", &pv->m_fPlankDepth);
         pv->m_fPlankDepth = max(0, pv->m_fPlankDepth);

         MeasureParseString (pPage, L"plankwidth", &pv->m_fPlankWidth);
         pv->m_fPlankWidth = max(0.01, pv->m_fPlankWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwPatternSize = (DWORD) DoubleFromControl (pPage, L"patternsize");
         pv->m_dwPatternSize = max(1, pv->m_dwPatternSize);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pv->m_fSpacingTransparent = pControl->AttribGetBOOL (Checked());
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lattice";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorLattice::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURELATTICE, LatticePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"plank")) {
      if (m_Plank.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   else if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorLattice::CTextCreatorLattice (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight =  0;
   m_dwPatternSize = 2;
   m_fPlankDepth = .02;
   m_fPlankWidth = .04;
   m_cSpacing = RGB(0xc0,0xc0,0xc0);
   m_fSpacingTransparent = TRUE;


   m_iSeed = 1234;
   m_fPixelLen = .005;
   m_fPatternWidth = m_fPatternHeight = m_fPlankWidth * 2;


   m_Plank.m_fBevelHeight = .005;
   m_Plank.m_fBevelWidth = 0;
   m_Plank.m_fBoardBend = 0;
   m_Plank.m_wSpecReflection = 0x2000;
   m_Plank.m_wSpecDirection = 5000;
   m_Plank.m_acColors[0] = RGB(0xd0, 0xb0, 0x40);
   m_Plank.m_acColors[1] = RGB(0xd0, 0xb0, 0x30);
   m_Plank.m_acColors[2] = RGB(0xb0, 0x90, 0x30);
   m_Plank.m_acColors[3] = RGB(0x90, 0x70, 0x00);
   m_Plank.m_acColors[4] = RGB(0x50,0x30,0);
   m_Plank.m_cTransitionColor = RGB(0x50,0x20,0);
   m_Plank.m_fRingBump = .001;
   m_Plank.m_fBrightMin = .9;
   m_Plank.m_fBrightMax = 1.2;

   m_Plank.m_Noise.m_fTurnOn = TRUE;
   m_Plank.m_Noise.m_fNoiseX = .05;
   m_Plank.m_Noise.m_fNoiseY = .05;
   m_Plank.m_Noise.m_cMax = RGB(0xb0,0x70,0x30);
   m_Plank.m_Noise.m_cMin = RGB(0xb0,0x80,0x30);
   m_Plank.m_Noise.m_fTransMax = .7;
   m_Plank.m_Noise.m_fTransMin = 1;

   m_Plank.m_Tree.m_fRadius = .3;
   m_Plank.m_Tree.m_fLogCenterXOffsetMin = -.1;
   m_Plank.m_Tree.m_fLogCenterXOffsetMax = .1;
   m_Plank.m_Tree.m_fNumKnotsPerMeter = 20;
   m_Plank.m_Tree.m_fKnotWidthMin = .005;
   m_Plank.m_Tree.m_fKnotWidthMax = .03;
   m_Plank.m_Tree.m_fRingThickness = .001;
   m_Plank.m_Tree.m_aNoise[0].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseX = .03;
   m_Plank.m_Tree.m_aNoise[0].m_fNoiseY = .12;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[0].m_fZDeltaMin = .006;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[0].m_fTransMin = 1;

   m_Plank.m_Tree.m_aNoise[1].m_fTurnOn = TRUE;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseX = .01;
   m_Plank.m_Tree.m_aNoise[1].m_fNoiseY = .04;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMax = 0;
   m_Plank.m_Tree.m_aNoise[1].m_fZDeltaMin = .001;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMax = 1;
   m_Plank.m_Tree.m_aNoise[1].m_fTransMin = 1;
}

void CTextCreatorLattice::Delete (void)
{
   delete this;
}


static PWSTR gpszLattice = L"Lattice";
static PWSTR gpszPatternSize = L"patternsize";
static PWSTR gpszPlankWidth = L"plankwidth";

PCMMLNode2 CTextCreatorLattice::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLattice);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszPatternSize, (int) m_dwPatternSize);
   MMLValueSet (pNode, gpszSpacing, (int) m_cSpacing);
   MMLValueSet (pNode, gpszSpacingTransparent, (int)m_fSpacingTransparent);
   MMLValueSet (pNode, gpszPlankDepth, m_fPlankDepth);
   MMLValueSet (pNode, gpszPlankWidth, m_fPlankWidth);

   PCMMLNode2 pSub;
   pSub = m_Plank.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszGeneratePlank);
      pNode->ContentAdd (pSub);
   }
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorLattice::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_dwPatternSize = (DWORD) MMLValueGetInt (pNode, gpszPatternSize, 1);
   m_cSpacing = (COLORREF) MMLValueGetInt (pNode, gpszSpacing, (int) 0);
   m_fSpacingTransparent = (BOOL) MMLValueGetInt (pNode, gpszSpacingTransparent, 0);
   m_fPlankDepth = MMLValueGetDouble (pNode, gpszPlankDepth, .01);
   m_fPlankWidth = MMLValueGetDouble (pNode, gpszPlankWidth, .01);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszGeneratePlank), &psz, &pSub);
   if (pSub)
      m_Plank.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);
   return TRUE;
}

BOOL CTextCreatorLattice::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // create the surface
   DWORD dwX, dwY;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD)((m_fPatternWidth + fPixelLen/2) / fPixelLen * (fp) m_dwPatternSize);
   dwY = (DWORD)((m_fPatternHeight + fPixelLen/2) / fPixelLen * (fp) m_dwPatternSize);

   // number of plans across and space between planks
   DWORD dwPlankWidth = (DWORD) (m_fPlankWidth / fPixelLen);
   dwPlankWidth = max(1,dwPlankWidth);


   // clear the background
   COLORREF cBackColor = m_cSpacing;
   pImage->Init (dwX, dwY, cBackColor, -ZINFINITE);

   // if the spacing is transparent then set that
   if (m_fSpacingTransparent) {
      PIMAGEPIXEL pip = pImage->Pixel(0,0);
      DWORD dwNum;
      for (dwNum = dwX * dwY; dwNum; dwNum--, pip++)
         pip->dwIDPart |= 0xff;
   }

   // make some planks
   DWORD x;
   CImage ITemp;
   fp   fDepth;
   fDepth = (m_fPlankDepth / fPixelLen);
   // vertical ones
   for (x = 0; x < m_dwPatternSize; x++) {
      m_Plank.Apply (pImage, fPixelLen, dwPlankWidth, dwY,
         (DWORD) ((fp)x / (fp) m_dwPatternSize * (fp)dwX), 0);
   }

   // horizontal ones
   DWORD xx, yy;
   for (x = 0; x < m_dwPatternSize; x++) {
      if (!ITemp.Init (dwPlankWidth, dwX, cBackColor, -ZINFINITE))
         continue;

      // create the plank
      m_Plank.Apply (&ITemp, fPixelLen, dwPlankWidth, dwX, 0, 0);

      // elevate it
      for (yy = 0; yy < ITemp.Height(); yy++) for (xx = 0; xx < ITemp.Width(); xx++) {
         PIMAGEPIXEL pip = ITemp.Pixel(xx,yy);
         pip->fZ += fDepth;
      }

      // merge and rotate
      POINT pDest;
      pDest.x = 0;
      pDest.y = (int)((fp)x / (fp) m_dwPatternSize * (fp)dwY);
      ITemp.TGMergeRotate (NULL, pImage, pDest);
   }

   // paint it
   m_DirtPaint.Apply (pImage, fPixelLen);


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}







/***********************************************************************************
CTextCreatorWicker - Noise functions
*/

class CTextCreatorWicker : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorWicker (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   void DrawVertWicker (PCImage pImage, fp fWavelength, fp fOffset);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in metetrs
   fp               m_fPlankWidth;  // width of plank in meters
   DWORD            m_dwPatternSize;   // number of repeats in horizontal and vertical
   COLORREF         m_cSpacing;         // coloration of the spacing
   BOOL             m_fSpacingTransparent;   // if TRUE, spacing between boards is transparent
   BOOL             m_fStraightVert;    // if true the verticals are straight
   BOOL             m_fStraightHorz;   // if true the horizontals are straight
   //BOOL             m_fDiagonal;       // if true draw as diagonal
   fp               m_fPlankDepth;     // depth of the plank
   COLORREF          m_cSurfA;     // color of tile varies from cSurfA to cSurfB
   COLORREF          m_cSurfB;     // color of tile varies from cSurfA to cSurfB
   CTextEffectDirtPaint m_DirtPaint;        // paint or add dirt
};
typedef CTextCreatorWicker *PCTextCreatorWicker;


/****************************************************************************
WickerPage
*/
BOOL WickerPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorWicker pv = (PCTextCreatorWicker) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"plankdepth", pv->m_fPlankDepth, TRUE);
         MeasureToString (pPage, L"plankwidth", pv->m_fPlankWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);
         DoubleToControl (pPage, L"patternsize", pv->m_dwPatternSize);
         FillStatusColor (pPage, L"spacingcolor", pv->m_cSpacing);
         FillStatusColor (pPage, L"surfacolor", pv->m_cSurfA);
         FillStatusColor (pPage, L"surfbcolor", pv->m_cSurfB);

         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSpacingTransparent);
         pControl = pPage->ControlFind (L"straightvert");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fStraightVert);
         pControl = pPage->ControlFind (L"straighthorz");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fStraightHorz);
         //pControl = pPage->ControlFind (L"diagonal");
         //if (pControl)
         //   pControl->AttribSetBOOL (Checked(), pv->m_fDiagonal);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"spacingbutton")) {
            pv->m_cSpacing = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSpacing, pPage, L"spacingcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"surfabutton")) {
            pv->m_cSurfA = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfA, pPage, L"surfacolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"surfbbutton")) {
            pv->m_cSurfB = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfB, pPage, L"surfbcolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"plankdepth", &pv->m_fPlankDepth);
         pv->m_fPlankDepth = max(0, pv->m_fPlankDepth);

         MeasureParseString (pPage, L"plankwidth", &pv->m_fPlankWidth);
         pv->m_fPlankWidth = max(0.001, pv->m_fPlankWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwPatternSize = (DWORD) DoubleFromControl (pPage, L"patternsize");
         pv->m_dwPatternSize = max(1, pv->m_dwPatternSize);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pv->m_fSpacingTransparent = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"straightvert");
         if (pControl)
            pv->m_fStraightVert = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"straighthorz");
         if (pControl)
            pv->m_fStraightHorz = pControl->AttribGetBOOL (Checked());
         //pControl = pPage->ControlFind (L"diagonal");
         //if (pControl)
         //   pv->m_fDiagonal = pControl->AttribGetBOOL (Checked());
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Wicker";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorWicker::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREWICKER, WickerPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"dirtpaint")) {
      if (m_DirtPaint.Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorWicker::CTextCreatorWicker (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight =  0;
   m_dwPatternSize = 8;
   m_fPlankDepth = .005;
   m_fPlankWidth = .005;
   m_cSpacing = RGB(0xc0,0xc0,0xc0);
   m_fSpacingTransparent = TRUE;
   m_fStraightVert = m_fStraightHorz = FALSE;
   //m_fDiagonal = FALSE;
   m_cSurfA = RGB(0x80,0x80,0x40);
   m_cSurfB = RGB(0xa0,0xa0,0x20);;


   m_iSeed = 1234;
   m_fPixelLen = .001;
   m_fPatternWidth = m_fPatternHeight = m_fPlankWidth * 2;


}

void CTextCreatorWicker::Delete (void)
{
   delete this;
}


static PWSTR gpszWicker = L"Wicker";
static PWSTR gpszStraightVert = L"straightvert";
static PWSTR gpszStraightHorz = L"straighthorz";
static PWSTR gpszDiagonal = L"diagonal";

PCMMLNode2 CTextCreatorWicker::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszWicker);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszPatternSize, (int) m_dwPatternSize);
   MMLValueSet (pNode, gpszSpacing, (int) m_cSpacing);
   MMLValueSet (pNode, gpszSpacingTransparent, (int)m_fSpacingTransparent);
   MMLValueSet (pNode, gpszPlankDepth, m_fPlankDepth);
   MMLValueSet (pNode, gpszPlankWidth, m_fPlankWidth);
   MMLValueSet (pNode, gpszSurfA, (int) m_cSurfA);
   MMLValueSet (pNode, gpszSurfB, (int) m_cSurfB);
   MMLValueSet (pNode, gpszStraightVert, (int)m_fStraightVert);
   MMLValueSet (pNode, gpszStraightHorz, (int)m_fStraightHorz);
   //MMLValueSet (pNode, gpszDiagonal, (int)m_fDiagonal);

   PCMMLNode2 pSub;
   pSub = m_DirtPaint.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDirtPaint);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorWicker::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_dwPatternSize = (DWORD) MMLValueGetInt (pNode, gpszPatternSize, 1);
   m_cSpacing = (COLORREF) MMLValueGetInt (pNode, gpszSpacing, (int) 0);
   m_fSpacingTransparent = (BOOL) MMLValueGetInt (pNode, gpszSpacingTransparent, 0);
   m_fPlankDepth = MMLValueGetDouble (pNode, gpszPlankDepth, .01);
   m_fPlankWidth = MMLValueGetDouble (pNode, gpszPlankWidth, .01);
   m_cSurfA = (COLORREF) MMLValueGetInt (pNode, gpszSurfA, (int) 0);
   m_cSurfB = (COLORREF) MMLValueGetInt (pNode, gpszSurfB, (int) 0);
   m_fStraightVert = (BOOL) MMLValueGetInt (pNode, gpszStraightVert, 0);
   m_fStraightHorz = (BOOL) MMLValueGetInt (pNode, gpszStraightHorz, 0);
   //m_fDiagonal = (BOOL) MMLValueGetInt (pNode, gpszDiagonal, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(gpszDirtPaint), &psz, &pSub);
   if (pSub)
      m_DirtPaint.MMLFrom (pSub);
   return TRUE;
}

/****************************************************************************************
CTextCreatorWicker::DrawVertWicker - Draws a vertical length of wicker.

inputs
   PCImage     pImage - Vertical strip to draw to, uses entier width (which is narrow)
               and height (long)
   fp          fWavelength - Wavelength, in pixels. Use 0 to make straight
   fp          fOffset - Initial offset of wavelength. Since wave starts at cos(y/fWavelength*2PI)
returns
   none
*/
void CTextCreatorWicker::DrawVertWicker (PCImage pImage, fp fWavelength, fp fOffset)
{
   DWORD xx,yy;
   DWORD dwWidth = pImage->Width();
   DWORD dwHeight = pImage->Height();

   // random color variation and offset
   fp fAlpha1, fAlpha2, fAlphaOffset;
   fAlpha1 = randf (0, 1);
   fAlpha2 = randf (0, 1);
   fAlphaOffset = randf (0, 2 * PI);

   WORD  aw[2][3];
   DWORD i;
   pImage->Gamma (m_cSurfA, aw[0]);
   pImage->Gamma (m_cSurfB, aw[1]);

   for (yy = 0; yy < dwHeight; yy++) {
      // calculate the color
      fp fAlpha;
      WORD awUse[3];
      fAlpha = (sin((fp)yy / (fp) dwHeight * 2.0 * PI + fAlphaOffset) + 1) / 2.0;
      fAlpha = fAlpha * (fAlpha2 - fAlpha1) + fAlpha1;
      for (i = 0; i < 3; i++)
         awUse[i] = (WORD) ((fp)aw[0][i] * (1.0 - fAlpha) + (fp)aw[1][i] * fAlpha);


      fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
      for (xx = 0; xx < dwWidth; xx++) {
         PIMAGEPIXEL pip = pImage->Pixel(xx,yy);

         // set some params
         pip->dwID = ((DWORD) m_Material.m_wSpecReflect << 16) | m_Material.m_wSpecExponent;
         pip->dwIDPart = 0;

         // color
         memcpy (&pip->wRed, &awUse[0], sizeof(awUse));

         // calculate the Z value
         fp f;
         f = ((fp) xx / (fp) dwWidth) * 2 - 1;
         f = sqrt(1.0 - f * f);  // so find elevation

         if (fWavelength)
            f += cos (fOffset + (fp)yy / fWavelength * 2.0 * PI);

         f *= m_fPlankDepth / fPixelLen;

         pip->fZ = f;
      }
   }
}

BOOL CTextCreatorWicker::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // create the surface
   DWORD dwX, dwY;
   m_dwPatternSize = (m_dwPatternSize+1) & ~(0x01);   // DWORD align
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD)(m_fPatternWidth / fPixelLen * (fp) m_dwPatternSize + 0.5);
   dwY = (DWORD)(m_fPatternHeight / fPixelLen * (fp) m_dwPatternSize + 0.5);

   // number of plans across and space between planks
   DWORD dwPlankWidth = (DWORD) (m_fPlankWidth / fPixelLen);
   dwPlankWidth = max(1,dwPlankWidth);


   // clear the background
   COLORREF cBackColor = m_cSpacing;
   pImage->Init (dwX, dwY, cBackColor, -(int) (m_fPlankDepth / fPixelLen * 1.1));
   DWORD x;
   PIMAGEPIXEL pip;
   for (x = 0, pip = pImage->Pixel(0,0); x < pImage->Width() * pImage->Height(); x++, pip++) {
      pip->dwID = ((DWORD) m_Material.m_wSpecReflect << 16) | m_Material.m_wSpecExponent;
      pip->dwIDPart = 0;
   }

   // if the spacing is transparent then set that
   if (m_fSpacingTransparent) {
      PIMAGEPIXEL pip = pImage->Pixel(0,0);
      DWORD dwNum;
      for (dwNum = dwX * dwY; dwNum; dwNum--, pip++)
         pip->dwIDPart |= 0xff;
   }

   // make some planks
   CImage ITemp;
   fp   fDepth;
   fDepth = (m_fPlankDepth / fPixelLen);
   // vertical ones
   for (x = 0; x < m_dwPatternSize; x++) {
      if (!ITemp.Init (dwPlankWidth, dwY, cBackColor, -(int) (m_fPlankDepth/ fPixelLen * 1.1)))
         continue;

      DrawVertWicker (&ITemp, m_fStraightVert ? 0 : (m_fPatternHeight / fPixelLen * 2), (x%2) ? PI : 0);

      // merge and rotate
      POINT pDest;
      pDest.y = 0;
      pDest.x = (int)((fp)x / (fp) m_dwPatternSize * (fp)dwX) - (int)dwPlankWidth / 2;
      ITemp.TGMerge (NULL, pImage, pDest);
   }

   // horizontal ones
   for (x = 0; x < m_dwPatternSize; x++) {
      if (!ITemp.Init (dwPlankWidth, dwX, cBackColor, -(int) (m_fPlankDepth / fPixelLen * 1.1)))
         continue;

      DrawVertWicker (&ITemp, m_fStraightHorz ? 0 : (m_fPatternWidth / fPixelLen * 2), (x%2) ? 0 : PI);

      // merge and rotate
      POINT pDest;
      pDest.x = 0;
      pDest.y = (int)((fp)x / (fp) m_dwPatternSize * (fp)dwY) + (int)dwPlankWidth / 2;
      ITemp.TGMergeRotate (NULL, pImage, pDest);
   }

   // paint it
   m_DirtPaint.Apply (pImage, fPixelLen);

#if 0 // DEAD code
   // if diagonal and width and height the same then do so
   if (m_fDiagonal && (m_fPatternWidth == m_fPatternHeight)) {
      ITemp.Init (pImage->Width(), pImage->Height());
      POINT pDest;
      DWORD xx,yy;
      pDest.x = pDest.y = 0;
      pImage->Overlay (NULL, &ITemp, pDest, TRUE);

      // matrix
      CMatrix  m;
      m.RotationZ (PI/4);
      CPoint p;

      // copy diagonal
      for (xx = 0; xx < pImage->Width(); xx++) for (yy = 0; yy < pImage->Height(); yy++) {
         // rotate
         p.Zero();
         p.p[0] = xx;
         p.p[1] = yy;
         p.p[3] = 1;
         p.MultiplyLeft (&m);

         // modulo
         p.p[0] = myfmod(p.p[0], pImage->Width());
         p.p[1] = myfmod(p.p[1], pImage->Width());

         *(pImage->Pixel(xx,yy)) = *(ITemp.Pixel((DWORD)p.p[0], (DWORD)p.p[1]));
      }
   }
#endif

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}












/***********************************************************************************
CTextCreatorChainmail - Noise functions
*/

class CTextCreatorChainmail : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorChainmail (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);
   void DrawLink (PCImage pImage, fp fCenterX, fp fCenterY,
                                      fp fDiameterX, fp fDiameterY, fp fThickness,
                                      fp fAngle, WORD *pawColor, DWORD dwID);

   DWORD             m_dwRenderShard;
   CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   int               m_iSeed;   // seed for the random
   fp                m_fPixelLen;  // meters per pixel
   fp                m_fWidth;      // total pattern width in meters
   fp                m_fHeight;     // total pattern height in metetrs

   DWORD             m_dwPatternWidth;    // number of patterns across width
   DWORD             m_dwPatternHeight;   // number of patterns high
   fp                m_fLinkScale;        // scaling of the link (0..1)
   fp                m_fLinkThickness;    // thickness of the link (0..1)
   fp                m_fLinkAngle;        // how much link angles (0..1)
   fp                m_fLinkVariation;    // how much link varies in size (0..1)

   COLORREF          m_cSpacing;         // coloration of the spacing
   BOOL              m_fSpacingTransparent;   // if TRUE, spacing between boards is transparent
   COLORREF          m_cSurfA;     // color of tile varies from cSurfA to cSurfB
   COLORREF          m_cSurfB;     // color of tile varies from cSurfA to cSurfB
};
typedef CTextCreatorChainmail *PCTextCreatorChainmail;


/****************************************************************************
ChainmailPage
*/
BOOL ChainmailPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorChainmail pv = (PCTextCreatorChainmail) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"width", pv->m_fWidth, TRUE);
         MeasureToString (pPage, L"height", pv->m_fHeight, TRUE);

         DoubleToControl (pPage, L"patternwidth", pv->m_dwPatternWidth);
         DoubleToControl (pPage, L"patternheight", pv->m_dwPatternHeight);
         FillStatusColor (pPage, L"spacingcolor", pv->m_cSpacing);
         FillStatusColor (pPage, L"surfacolor", pv->m_cSurfA);
         FillStatusColor (pPage, L"surfbcolor", pv->m_cSurfB);

         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fSpacingTransparent);

         pControl = pPage->ControlFind (L"linkscale");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fLinkScale * 100));
         pControl = pPage->ControlFind (L"linkthickness");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fLinkThickness * 100));
         pControl = pPage->ControlFind (L"linkangle");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fLinkAngle * 100));
         pControl = pPage->ControlFind (L"linkvariation");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fLinkVariation * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"spacingbutton")) {
            pv->m_cSpacing = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSpacing, pPage, L"spacingcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"surfabutton")) {
            pv->m_cSurfA = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfA, pPage, L"surfacolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"surfbbutton")) {
            pv->m_cSurfB = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSurfB, pPage, L"surfbcolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"width", &pv->m_fWidth);
         pv->m_fWidth = max(pv->m_fPixelLen, pv->m_fWidth);

         MeasureParseString (pPage, L"height", &pv->m_fHeight);
         pv->m_fHeight = max(pv->m_fPixelLen, pv->m_fHeight);

         pv->m_dwPatternWidth = (DWORD) DoubleFromControl (pPage, L"patternwidth");
         pv->m_dwPatternWidth = max(1, pv->m_dwPatternWidth);

         pv->m_dwPatternHeight = (DWORD) DoubleFromControl (pPage, L"patternheight");
         pv->m_dwPatternHeight = max(1, pv->m_dwPatternHeight);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"spacingtransparent");
         if (pControl)
            pv->m_fSpacingTransparent = pControl->AttribGetBOOL (Checked());
      }
      break;

   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         int iVal;
         iVal = p->pControl->AttribGetInt (Pos());
         fp fVal = (fp)iVal / 100.0;
         if (!_wcsicmp(p->pControl->m_pszName, L"linkscale")) {
            pv->m_fLinkScale = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"linkthickness")) {
            pv->m_fLinkThickness = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"linkangle")) {
            pv->m_fLinkAngle = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"linkvariation")) {
            pv->m_fLinkVariation = fVal;
            return TRUE;
         }

      }
      break;
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Chainmail";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorChainmail::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURECHAINMAIL, ChainmailPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorChainmail::CTextCreatorChainmail (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_METALROUGH);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = .001;
   m_fWidth = m_fHeight =  0.05;
   m_dwPatternWidth = m_dwPatternHeight = 5;
   m_fLinkScale = m_fLinkThickness = m_fLinkAngle = 0.5;
   m_fLinkVariation = 0.1;

   m_cSpacing = RGB(0x40,0x40,0x40);
   m_fSpacingTransparent = TRUE;
   m_cSurfA = RGB(0x80,0x80,0x80);
   m_cSurfB = RGB(0xc0,0xc0,0xc0);;


   m_iSeed = 1234;


}

void CTextCreatorChainmail::Delete (void)
{
   delete this;
}


static PWSTR gpszChainmail = L"Chainmail";
static PWSTR gpszLinkScale = L"LinkScale";
static PWSTR gpszLinkThickness = L"LinkThickness";
static PWSTR gpszLinkVariation = L"LinkVariation";
static PWSTR gpszLinkAngle = L"LinkAngle";

PCMMLNode2 CTextCreatorChainmail::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszChainmail);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, (int)m_dwPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, (int)m_dwPatternHeight);
   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszLinkScale, m_fLinkScale);
   MMLValueSet (pNode, gpszLinkThickness, m_fLinkThickness);
   MMLValueSet (pNode, gpszLinkAngle, m_fLinkAngle);
   MMLValueSet (pNode, gpszLinkVariation, m_fLinkVariation);

   MMLValueSet (pNode, gpszSpacing, (int) m_cSpacing);
   MMLValueSet (pNode, gpszSpacingTransparent, (int)m_fSpacingTransparent);
   MMLValueSet (pNode, gpszSurfA, (int) m_cSurfA);
   MMLValueSet (pNode, gpszSurfB, (int) m_cSurfB);

   return pNode;
}

BOOL CTextCreatorChainmail::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);

   m_dwPatternWidth = (int) MMLValueGetInt (pNode, gpszPatternWidth, 5);
   m_dwPatternHeight = (int) MMLValueGetInt (pNode, gpszPatternHeight, 5);
   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, .05);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, .05);
   m_fLinkScale = MMLValueGetDouble (pNode, gpszLinkScale, .5);
   m_fLinkThickness = MMLValueGetDouble (pNode, gpszLinkThickness, .5);
   m_fLinkAngle = MMLValueGetDouble (pNode, gpszLinkAngle, .5);
   m_fLinkVariation = MMLValueGetDouble (pNode, gpszLinkVariation, .5);

   m_cSpacing = (COLORREF) MMLValueGetInt (pNode, gpszSpacing, (int) 0);
   m_fSpacingTransparent = (BOOL) MMLValueGetInt (pNode, gpszSpacingTransparent, 0);
   m_cSurfA = (COLORREF) MMLValueGetInt (pNode, gpszSurfA, (int) 0);
   m_cSurfB = (COLORREF) MMLValueGetInt (pNode, gpszSurfB, (int) 0);

   return TRUE;
}


/**********************************************************************************
CTextCreatorChainmail::DrawLink - Draws a link on the image

inputs
   PCImage           pImage - To draw on
   fp                fCenterX - X center, in pixels
   fp                fCenterY - Y cender, in pixels
   fp                fDiameterX - X diamter
   fp                fDiameterY - Y diameter
   fp                fThickness - Thickness of wire, diameter in pixels
   fp                fAngle - Angle, 0..1
   WORD              *pawColor - Array of 3 color values
   DWORD             dwID - Value to write for specularity
returns
   none
*/
void CTextCreatorChainmail::DrawLink (PCImage pImage, fp fCenterX, fp fCenterY,
                                      fp fDiameterX, fp fDiameterY, fp fThickness,
                                      fp fAngle, WORD *pawColor, DWORD dwID)
{
   // determine pixels that want
   RECT r;
   fp fRadX = fDiameterX/2;
   fp fRadY = fDiameterY/2;
   fp fRingRad = fThickness/2;
   r.left = floor(fCenterX - fRadX - fRingRad);
   r.right = ceil(fCenterX + fRadX + fRingRad);
   r.top = floor(fCenterY - fRadY - fRingRad);
   r.bottom = ceil(fCenterY + fRadY + fRingRad);

   int iHeight = (int)pImage->Height();
   int iWidth = (int)pImage->Width();

   // loop
   int iX, iY;
   for (iY = r.top; iY <= r.bottom; iY++) {
      DWORD dwY = (DWORD)((iY + iHeight) % iHeight);  // modulo coords
      fp fDistY = ((fp)iY - fCenterY) / fRadY;

      for (iX = r.left; iX <= r.right; iX++) {
         DWORD dwX = (DWORD)((iX + iWidth) % iWidth); // modulo coords

         // see the distance away from the center
         fp fDistX = ((fp)iX - fCenterX) / fRadX;
         fp fDist = sqrt(fDistX * fDistX + fDistY * fDistY);
         if (fDist < CLOSE)
            continue;
         
         // try to figure out where the "center" of the ring is
         fp fRingX = (fDistX / fDist) * fRadX + fCenterX;
         fp fRingY = (fDistY / fDist) * fRadY + fCenterY;

         // find the distance of the pixel from the center of the ring
         fp fRingDistX = fRingX - (fp)iX;
         fp fRingDistY = fRingY - (fp)iY;
         fp fRingDist = fRingRad * fRingRad - fRingDistX * fRingDistX - fRingDistY * fRingDistY;

         // if too far from ring then nothing to draw
         if (fRingDist < 0)
            continue;

         // else, can draw
         fp fZ = sqrt(fRingDist) + ((fp)iX - fCenterX) * fAngle;  // take angle into account

         // get the pixel
         PIMAGEPIXEL pip = pImage->Pixel (dwX, dwY);
         if (pip->fZ >= fZ)
            continue;   // obscured

         // else, draw
         pip->dwID = dwID;
         pip->dwIDPart &= ~0xff; // so remove transparency
         pip->wRed = pawColor[0];
         pip->wGreen = pawColor[1];
         pip->wBlue = pawColor[2];
         pip->fZ = fZ;
      } // iX
   } // iY
}

BOOL CTextCreatorChainmail::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // create the surface
   DWORD dwX, dwY;
   m_dwPatternWidth = max(m_dwPatternWidth, 1);
   m_dwPatternHeight = max(m_dwPatternHeight, 1);
   m_fPixelLen = max(m_fPixelLen, .0001);
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   m_fWidth = max(m_fWidth, fPixelLen);
   m_fHeight = max(m_fHeight, fPixelLen);

   dwX = (DWORD)((m_fWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD)((m_fHeight + fPixelLen/2) / fPixelLen);

   fp fLinkWidth = (fp)dwX / (fp)m_dwPatternWidth * m_fLinkScale; // diameter, in pixels
   fp fLinkHeight = (fp)dwY / (fp)m_dwPatternWidth * m_fLinkScale; // diameter, in pixels
   fp fLinkThickness = max(fLinkWidth, fLinkHeight) * m_fLinkThickness / 2.0;   // diameter, in pixels

   // clear the background
   COLORREF cBackColor = m_cSpacing;
   pImage->Init (dwX, dwY, cBackColor, - (fLinkWidth/2 * m_fLinkAngle + fLinkThickness/2) );
   DWORD x, y, i;
   PIMAGEPIXEL pip;
   DWORD dwID = ((DWORD) m_Material.m_wSpecReflect << 16) | m_Material.m_wSpecExponent;
   for (x = 0, pip = pImage->Pixel(0,0); x < pImage->Width() * pImage->Height(); x++, pip++) {
      pip->dwID = dwID;
      pip->dwIDPart = 0;
   }

   // if the spacing is transparent then set that
   if (m_fSpacingTransparent) {
      PIMAGEPIXEL pip = pImage->Pixel(0,0);
      DWORD dwNum;
      for (dwNum = dwX * dwY; dwNum; dwNum--, pip++) {
         pip->dwID = m_Material.m_wSpecExponent;   // no specular reflection at all for transparent bit
         pip->dwIDPart |= 0xff;
      }
   }

   // draw links
   WORD awColor[3], awColorA[3], awColorB[3];
   Gamma (m_cSurfA, awColorA);
   Gamma (m_cSurfB, awColorB);
   fp fVar = m_fLinkVariation / 2;
   DWORD dwSub;
   for (y = 0; y < m_dwPatternHeight; y++)
      for (x = 0; x < m_dwPatternWidth; x++)
         for (dwSub = 0; dwSub < 2; dwSub++) {  // for opposite link
            // random color
            fp fAlpha = MyRand (0, 1);
            for (i = 0; i < 3; i++)
               awColor[i] = (WORD)(fAlpha * (fp)awColorA[i] + (1.0-fAlpha) * (fp)awColorB[i]);

            // draw link
            DrawLink (pImage,
               ((fp)x+MyRand(fVar,-fVar/2)+(dwSub ? 0.5 : 0)) / (fp)m_dwPatternWidth * (fp)dwX,
               ((fp)y+MyRand(fVar,-fVar/2)+(dwSub ? 0.5 : 0)) / (fp)m_dwPatternHeight * (fp)dwY,
               fLinkWidth * MyRand (1 + fVar, 1 - fVar),
               fLinkHeight * MyRand (1 + fVar, 1 - fVar),
               fLinkThickness * MyRand (1 + fVar, 1 - fVar),
               m_fLinkAngle * MyRand (1 + fVar, 1 - fVar) * (dwSub ? -1 : 1),
               awColor, dwID);
         } // dwSub,dwX, dwY

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}









/***********************************************************************************
CTextCreatorTestPattern - Create a test pattern
*/

class CTextCreatorTestPattern : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorTestPattern (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
};
typedef CTextCreatorTestPattern *PCTextCreatorTestPattern;


/****************************************************************************
TestPatternPage
*/
BOOL TestPatternPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorTestPattern pv = (PCTextCreatorTestPattern) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Test Pattern";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorTestPattern::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;


   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURETESTPATTERN, TestPatternPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorTestPattern::CTextCreatorTestPattern (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);


}

void CTextCreatorTestPattern::Delete (void)
{
   delete this;
}


static PWSTR gpszTestPattern = L"TestPattern";

PCMMLNode2 CTextCreatorTestPattern::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTestPattern);

   m_Material.MMLTo(pNode);

   return pNode;
}

BOOL CTextCreatorTestPattern::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   return TRUE;
}

#define TESTPATGRID     8     // 8 x 8
#define SUBGRIDSIZE     8
#define TESTPATPIX      (TESTPATGRID * TESTPATGRID * SUBGRIDSIZE)  // width and height in pixels

BOOL CTextCreatorTestPattern::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   // do this by createing a bitmap and drawing in it
   HDC hDCScreen = GetDC (GetDesktopWindow());
   HDC hDC = CreateCompatibleDC (hDCScreen);
   HBITMAP hBit = CreateCompatibleBitmap (hDCScreen, TESTPATPIX, TESTPATPIX);
   ReleaseDC (GetDesktopWindow(), hDCScreen);
   SelectObject (hDC, hBit);

   // create the font
   HFONT hFont;
   LOGFONT  lf;
   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -TESTPATPIX / TESTPATGRID / 2; 
   lf.lfCharSet = DEFAULT_CHARSET;
   lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (lf.lfFaceName, "Arial");
   hFont = CreateFontIndirect (&lf);
   HFONT hFontOld;
   hFontOld = (HFONT) SelectObject (hDC, hFont);
   SetBkMode (hDC, TRANSPARENT);
   SetTextColor (hDC, 0);

   // loop through all the cells
   DWORD x,y, i;
   for (x = 0; x < TESTPATGRID; x++) for (y = 0; y < TESTPATGRID; y++) {
      RECT r;
      r.left = x * TESTPATPIX / TESTPATGRID;
      r.right = (x+1) * TESTPATPIX / TESTPATGRID;
      r.top = y * TESTPATPIX / TESTPATGRID;
      r.bottom = (y+1) * TESTPATPIX / TESTPATGRID;

      // background
      COLORREF cr = MapColorPicker((y + x) % TESTPATGRID);
      HBRUSH hBrush = CreateSolidBrush (cr);
      FillRect (hDC, &r, hBrush);
      DeleteObject (hBrush);

      // lines behind
      SelectObject (hDC, GetStockObject (WHITE_PEN));
      for (i = 0; i < TESTPATGRID; i++) {
         int iVal = (r.right - r.left) * (int)i / TESTPATGRID + r.left;
         MoveToEx (hDC, iVal, r.top, NULL);
         LineTo (hDC, iVal, r.bottom);

         iVal = (r.bottom - r.top) * (int)i / TESTPATGRID + r.top;
         MoveToEx (hDC, r.left, iVal, NULL);
         LineTo (hDC, r.right, iVal);
      }

      // bit of a border
      cr = RGB(GetRValue(cr)/2, GetGValue(cr)/2, GetBValue(cr)/2);
      hBrush = CreateSolidBrush (cr);
      RECT rb;
      rb = r;
      rb.right = rb.left + SUBGRIDSIZE / 2;
      FillRect (hDC, &rb, hBrush);
      rb = r;
      rb.left = rb.right - SUBGRIDSIZE / 2;
      FillRect (hDC, &rb, hBrush);
      rb = r;
      rb.bottom = rb.top + SUBGRIDSIZE / 2;
      FillRect (hDC, &rb, hBrush);
      rb = r;
      rb.top = rb.bottom - SUBGRIDSIZE / 2;
      FillRect (hDC, &rb, hBrush);
      DeleteObject (hBrush);

      // text
      char sz[8];
      sz[0] = (char)x + '1';
      sz[1] = (char)y + 'a';
      sz[2] = 0;
      DrawTextEx (hDC, sz, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, NULL);
   } // x,y

   SelectObject (hDC, hFontOld);
   DeleteDC (hDCScreen);
   DeleteObject (hFont);

   pImage->Init (hBit, 0); // initialize from bitmap
   DeleteObject (hBit);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = 1.0 / (fp)TESTPATPIX;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}





/***********************************************************************************
CTextCreatorBitmap - Bitmap image
*/

class CTextCreatorBitmap : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorBitmap (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   fp               m_fPixelLen; // length of a pixel
   COLORREF         m_cTransColor;  // transparent color
   DWORD            m_dwTransDist;  // acceptable transparent distance
   BOOL             m_fTransUse;    // set to TRUE if use transparency
};
typedef CTextCreatorBitmap *PCTextCreatorBitmap;



/****************************************************************************
BitmapPage
*/
BOOL BitmapPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorBitmap pv = (PCTextCreatorBitmap) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);
         DoubleToControl (pPage, L"resource", pv->m_dwType);

         pControl = pPage->ControlFind (L"transuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTransUse);
         FillStatusColor (pPage, L"transcolor", pv->m_cTransColor);
         pControl = pPage->ControlFind (L"transdist");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwTransDist);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changetrans")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cTransColor, pPage, L"transcolor");
            if (cr != pv->m_cTransColor) {
               pv->m_cTransColor = cr;
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"transuse")) {
            pv->m_fTransUse = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         DWORD dwVal;
         dwVal = (DWORD) p->pControl->AttribGetInt (Pos());
         if (!_wcsicmp(p->pControl->m_pszName, L"transdist")) {
            if (dwVal != pv->m_dwTransDist) {
               pv->m_dwTransDist = dwVal;
            }
            return TRUE;
         }
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         pv->m_dwType = (DWORD) DoubleFromControl (pPage, L"resource");
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Bitmap or JPEG";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorBitmap::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREBMP, BitmapPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorBitmap::CTextCreatorBitmap (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_FLAT);
   if (dwType == 0)
      dwType = IDB_CUT;
   m_dwType = dwType;
   m_fPixelLen = .005;

   m_cTransColor = RGB(0xff,0xff,0xff);
   m_dwTransDist = 10;
   m_fTransUse = FALSE;
}

void CTextCreatorBitmap::Delete (void)
{
   delete this;
}

static PWSTR gpszBitmap = L"Bitmap";
static PWSTR gpszTransColor = L"TransColor";
static PWSTR gpszTransDist = L"TransDist";
static PWSTR gpszTransUse = L"TransUse";

PCMMLNode2 CTextCreatorBitmap::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszBitmap);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszTransColor, (int)m_cTransColor);
   MMLValueSet (pNode, gpszTransDist, (int)m_dwTransDist);
   MMLValueSet (pNode, gpszTransUse, (int)m_fTransUse);

   return pNode;
}

BOOL CTextCreatorBitmap::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_cTransColor = (COLORREF) MMLValueGetInt (pNode, gpszTransColor, (int)0xffffff);
   m_dwTransDist = (DWORD) MMLValueGetInt (pNode, gpszTransDist, (int)10);
   m_fTransUse = (BOOL) MMLValueGetInt (pNode, gpszTransUse, (int)0);
   return TRUE;
}

BOOL CTextCreatorBitmap::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   if (!pImage->InitBitmap (ghInstance, m_dwType))
      pImage->InitBitmap (ghInstance, IDB_CUT);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = m_fPixelLen;
   pTextInfo->dwMap = 0;
   // dont call this because no maps - MassageForCreator (pImage, pMaterial, pTextInfo);

   // if transparency then apply that
   if (m_fTransUse) {
      ColorToTransparency (pImage, m_cTransColor, m_dwTransDist, &m_Material);
      pTextInfo->dwMap |= (0x04 | 0x01);  // transparency map and specularity map
   }

   return TRUE;
}


/***********************************************************************************
CTextCreatorJPEG - JPEG image
*/

class CTextCreatorJPEG : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorJPEG (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   fp               m_fPixelLen; // length of a pixel
   COLORREF         m_cTransColor;  // transparent color
   DWORD            m_dwTransDist;  // acceptable transparent distance
   BOOL             m_fTransUse;    // set to TRUE if use transparency
};
typedef CTextCreatorJPEG *PCTextCreatorJPEG;


/****************************************************************************
JPEGPage
*/
BOOL JPEGPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorJPEG pv = (PCTextCreatorJPEG) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen);
         DoubleToControl (pPage, L"resource", pv->m_dwType);

         pControl = pPage->ControlFind (L"transuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTransUse);
         FillStatusColor (pPage, L"transcolor", pv->m_cTransColor);
         pControl = pPage->ControlFind (L"transdist");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwTransDist);
      }
      break;


   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         DWORD dwVal;
         dwVal = (DWORD) p->pControl->AttribGetInt (Pos());
         if (!_wcsicmp(p->pControl->m_pszName, L"transdist")) {
            if (dwVal != pv->m_dwTransDist) {
               pv->m_dwTransDist = dwVal;
            }
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changetrans")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cTransColor, pPage, L"transcolor");
            if (cr != pv->m_cTransColor) {
               pv->m_cTransColor = cr;
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"transuse")) {
            pv->m_fTransUse = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         pv->m_dwType = (DWORD) DoubleFromControl (pPage, L"resource");
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Bitmap or JPEG";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorJPEG::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREBMP, JPEGPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
   return TRUE;
}

CTextCreatorJPEG::CTextCreatorJPEG (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_FLAT);
   if (dwType == 0)
      m_dwType = IDR_EAGLEEYE;

   m_dwType = dwType;
   m_fPixelLen = .005;

   m_cTransColor = RGB(0xff,0xff,0xff);
   m_dwTransDist = 10;
   m_fTransUse = FALSE;
}

void CTextCreatorJPEG::Delete (void)
{
   delete this;
}

static PWSTR gpszJPEG = L"JPEG";

BOOL CTextCreatorJPEG::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int) 0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_cTransColor = (COLORREF) MMLValueGetInt (pNode, gpszTransColor, (int)0xffffff);
   m_dwTransDist = (DWORD) MMLValueGetInt (pNode, gpszTransDist, (int)10);
   m_fTransUse = (BOOL) MMLValueGetInt (pNode, gpszTransUse, (int)0);
   return TRUE;
}

PCMMLNode2 CTextCreatorJPEG::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszJPEG);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszTransColor, (int)m_cTransColor);
   MMLValueSet (pNode, gpszTransDist, (int)m_dwTransDist);
   MMLValueSet (pNode, gpszTransUse, (int)m_fTransUse);

   return pNode;
}

BOOL CTextCreatorJPEG::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   if (!pImage->InitJPEG (ghInstance, m_dwType))
      pImage->InitJPEG (ghInstance, IDR_LOGO);

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = m_fPixelLen;
   pTextInfo->dwMap = 0;
   // dont call this because no maps - MassageForCreator (pImage, pMaterial, pTextInfo);

   // if transparency then apply that
   if (m_fTransUse) {
      ColorToTransparency (pImage, m_cTransColor, m_dwTransDist, &m_Material);
      pTextInfo->dwMap |= (0x04 | 0x01);  // transparency map and specularity map
   }

   return TRUE;
}


/***********************************************************************************
CTextCreatorImageFile - Bitmap image
*/

/****************************************************************************
CTextCreatorImageFile::CacheToImage - Takes whatever is in the cache and converts
it to an image.

inputs
   PCImage     pImage - Will be filled in with the image from the cache.
   PCMem       pCache - Containing the cache. m_dwCurPosn is set to the size of the cache
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorImageFile::CacheToImage (PCImage pImage, PCMem pCache)
{
   // must at least contain header
   if (pCache->m_dwCurPosn <= sizeof(DWORD))
      return FALSE;

   // the header tells if it's bitmap or JPEG
   DWORD dwType;
   dwType = *((DWORD*) pCache->p);

   if (dwType == 0) { // bitmap
      // decompress if bitmap

      // width and height
      DWORD dwWidth, dwHeight;
      if (pCache->m_dwCurPosn <= 3 * sizeof(DWORD))
         return FALSE;
      dwWidth = ((DWORD*) pCache->p)[1];
      dwHeight = ((DWORD*) pCache->p)[2];

      DWORD dwRet;
      size_t dwUsed;
      CMem memDecomp;
      dwRet = RLEDecode ((PBYTE) (((DWORD*) pCache->p)+3), pCache->m_dwCurPosn - 3* sizeof(DWORD),
         3, &memDecomp, &dwUsed);
      if (dwRet)
         return FALSE;
      if (memDecomp.m_dwCurPosn < dwWidth * dwHeight * 3)
         return FALSE;

      // read in
      PIMAGEPIXEL pip;
      PBYTE pb;
      DWORD i;
      pImage->Init (dwWidth, dwHeight);
      pip = pImage->Pixel(0,0);
      pb = (PBYTE) memDecomp.p;
      for (i = 0; i < dwWidth * dwHeight; i++, pip++, pb += 3) {
         pip->wRed = Gamma (pb[0]);
         pip->wGreen = Gamma (pb[1]);
         pip->wBlue = Gamma (pb[2]);
      }

      return TRUE;
   }
   else if (dwType == 1) { // jpeg
      // write it out to a temporary file
      char szPath[256], szFile[256];
      GetTempPath (sizeof(szPath), szPath);
      if (!GetTempFileName (szPath, "m3d", 0, szFile))
         return FALSE;

      // make sure to append .jpg to end
      int iLen;
      iLen = (DWORD)strlen(szFile);
      if ((iLen >= 4) && (szFile[iLen-4] == '.'))
         strcpy (szFile + (iLen-3), "jpg");

      // write out the file
      FILE *f;
      OUTPUTDEBUGFILE (szFile);
      f = fopen (szFile, "wb");
      if (!f)
         return FALSE;
      fwrite (((DWORD*) pCache->p)+1, 1, pCache->m_dwCurPosn - sizeof(DWORD), f);
      fclose (f);

      BOOL fRet;
      WCHAR szTemp[256];
      MultiByteToWideChar (CP_ACP, 0, szFile, -1, szTemp, sizeof(szTemp)/2);
      fRet = pImage->Init (szTemp, ZINFINITE, TRUE);  // BUGFIX - Ignore megafile

      DeleteFile (szFile);
      return fRet;
   }
   else
      return FALSE;

}

/****************************************************************************
CTextCreatorImageFile::ImageToCache - Converts from an image to a cache item.

inputs
   PCMem       pCache - Will be allocated with enough space for a header and the
                  memory. m_dwCurPosn will be filled with the amount of memory used
   PCImage     pImage - Original image.
   DWORD       dwType - 0 for bitmap, 1 for jpeg
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorImageFile::ImageToCache (PCMem pCache, PCImage pImage, DWORD dwType)
{
   DWORD dwNeeded;

   if (dwType == 0) {   // bitmap
      dwNeeded = 3 * sizeof(DWORD) + pImage->Width() * pImage->Height() * 3;
      if (!pCache->Required (dwNeeded))
         return FALSE;

      DWORD *padw;
      padw = (DWORD*) pCache->p;
      padw[0] = dwType;
      padw[1] = pImage->Width();
      padw[2] = pImage->Height();

      // bytes
      PBYTE pb;
      DWORD i, dwNum;
      PIMAGEPIXEL pip;
      pb = (BYTE*) (padw+3);
      dwNum = pImage->Width() * pImage->Height();
      pip = pImage->Pixel(0,0);
      for (i = 0; i < dwNum; i++, pip++, pb+=3) {
         pb[0] = UnGamma(pip->wRed);
         pb[1] = UnGamma(pip->wGreen);
         pb[2] = UnGamma(pip->wBlue);
      }

      // rle
      CMem mem;
      mem.m_dwCurPosn = 0;
      if (RLEEncode ((PBYTE) (padw+3), dwNum, 3, &mem))
         return FALSE;
      if (!pCache->Required (sizeof(DWORD)*3 + mem.m_dwCurPosn))
         return FALSE;
      padw = (DWORD*) pCache->p;
      memcpy (padw + 3, mem.p, mem.m_dwCurPosn);
      pCache->m_dwCurPosn = sizeof(DWORD)*3 + mem.m_dwCurPosn;
      return TRUE;
   }
   else if (dwType == 1) { // jpeg
      // write it out to a temporary file
      char szPath[256], szFile[256];
      GetTempPath (sizeof(szPath), szPath);
      if (!GetTempFileName (szPath, "m3d", 0, szFile))
         return FALSE;

      // write out the file
      WCHAR szTemp[256];
      MultiByteToWideChar (CP_ACP, 0, szFile, -1, szTemp, sizeof(szTemp)/2);

      HDC hDC;
      HBITMAP hBit;
      hDC = GetDC (GetDesktopWindow());
      hBit = pImage->ToBitmap (hDC);
      ReleaseDC (GetDesktopWindow (), hDC);
      if (!hBit)
         return FALSE;

      if (!BitmapToJPegNoMegaFile (hBit, szFile)) {
         DeleteObject (hBit);
         return FALSE;
      }
      DeleteObject (hBit);


      // read in the file
      FILE *f;
      int iSize;
      OUTPUTDEBUGFILE (szFile);
      f = fopen (szFile, "rb");
      if (!f)
         return FALSE;
      fseek (f, 0, SEEK_END);
      iSize = ftell (f);
      fseek (f, 0, SEEK_SET);

      if (!pCache->Required (sizeof(DWORD) + (DWORD)iSize)) {
         fclose (f);
         DeleteFile (szFile);
         return FALSE;
      }
      pCache->m_dwCurPosn = sizeof(DWORD) + (DWORD) iSize;
      *((DWORD*)pCache->p) = dwType;

      fread (((DWORD*) pCache->p)+1, 1, iSize, f);
      fclose (f);
      DeleteFile (szFile);

      return TRUE;
   }
   else
      return FALSE;
}

/****************************************************************************
CTextCreatorImageFile::CacheToFiles - Takes whatever is in the cache and writes
it to a file.

inputs
   PWSTR       pszFile - File
   PCMem       pCache - Containing the cache. m_dwCurPosn is set to the size of the cache
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorImageFile::CacheToFile (PWSTR pszFile, PCMem pCache)
{
   // see if want bmp of jpeg
   DWORD dwLen = (DWORD)wcslen(pszFile);
   DWORD dwType;
   if (dwLen <= 4)
      return FALSE;
   if (!_wcsnicmp(pszFile + (dwLen-4), L".bmp", 4))
      dwType = 0;
   else if (!_wcsnicmp(pszFile + (dwLen-4), L".jpg", 4))
      dwType = 1;
   else
      return FALSE; // dont know the extension
   if (dwType != ((DWORD*) pCache->p)[0])
      return FALSE;  // wrong types

   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0,0);

   if (dwType == 0) { // bitmap
      // convert this to an image and save the image
      CImage Image;
      if (!CacheToImage (&Image, pCache))
         return FALSE;

      HDC hDC;
      HBITMAP hBit;
      hDC = GetDC (GetDesktopWindow());
      hBit = Image.ToBitmap (hDC);
      ReleaseDC (GetDesktopWindow (), hDC);
      if (!hBit)
         return FALSE;

      if (!BitmapSave (hBit, szTemp)) {
         DeleteObject (hBit);
         return FALSE;
      }

      DeleteObject (hBit);
      return TRUE;
   }
   else { // dwType == 1, jpeg
      // ignore the first byte and save the rest
      PMEGAFILE f = MegaFileOpen (szTemp, FALSE);
      if (!f)
         return FALSE;
      MegaFileWrite (((DWORD*)pCache->p) + 1, 1, pCache->m_dwCurPosn - sizeof(DWORD), f);
      MegaFileClose (f);
      return TRUE;
   }

   return TRUE;
}

/****************************************************************************
CTextCreatorImageFile::FileToCache - Converts from a file to a cache item.

inputs
   PCMem       pCache - Will be allocated with enough space for a header and the
                  memory. m_dwCurPosn will be filled with the amount of memory used
   PWSTR       pszFile - file
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorImageFile::FileToCache (PCMem pCache, PWSTR pszFile)
{
   // see if want bmp of jpeg
   DWORD dwLen = (DWORD)wcslen(pszFile);
   DWORD dwType;
   if (dwLen <= 4)
      return FALSE;
   if (!_wcsnicmp(pszFile + (dwLen-4), L".bmp", 4))
      dwType = 0;
   else if (!_wcsnicmp(pszFile + (dwLen-4), L".jpg", 4))
      dwType = 1;
   else
      return FALSE; // dont know the extension

   if (dwType == 0) { // bitmap
      CImage Image;
      if (!Image.Init (pszFile))
         return FALSE;

      return ImageToCache (pCache, &Image, 0);
   }
   else if (dwType == 1) { // jpeg
      //char szTemp[256];
      //WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0,0);

      // read it in
      // read in the file
      PMEGAFILE f;
      int iSize;
      f = MegaFileOpen (pszFile);
      if (!f)
         return FALSE;
      MegaFileSeek (f, 0, SEEK_END);
      iSize = MegaFileTell (f);
      MegaFileSeek (f, 0, SEEK_SET);

      if (!pCache->Required (sizeof(DWORD) + (DWORD)iSize)) {
         MegaFileClose (f);
         return FALSE;
      }
      pCache->m_dwCurPosn = sizeof(DWORD) + (DWORD) iSize;
      *((DWORD*)pCache->p) = dwType;

      MegaFileRead (((DWORD*) pCache->p)+1, 1, iSize, f);
      MegaFileClose (f);
      return TRUE;
   }

   return TRUE;
}


/****************************************************************************
ImageFilePage
*/
BOOL ImageFilePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorImageFile pv = (PCTextCreatorImageFile) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"width", pv->m_fWidth, TRUE);

         pControl = pPage->ControlFind (L"file");
         if (pControl)
            pControl->AttribSet (Text(), pv->m_szFile);

         if (pControl = pPage->ControlFind (L"transfile"))
            pControl->AttribSet (Text(), pv->m_szTransFile);
         //if (pControl = pPage->ControlFind (L"lucenfile"))
         //   pControl->AttribSet (Text(), pv->m_szLucenFile);
         if (pControl = pPage->ControlFind (L"glossfile"))
            pControl->AttribSet (Text(), pv->m_szGlossFile);
         if (pControl = pPage->ControlFind (L"bumpfile"))
            pControl->AttribSet (Text(), pv->m_szBumpFile);
         if (pControl = pPage->ControlFind (L"glowfile"))
            pControl->AttribSet (Text(), pv->m_szGlowFile);

         ComboBoxSet (pPage, L"transrgb", pv->m_dwTransRGB);
         ComboBoxSet (pPage, L"glossrgb", pv->m_dwGlossRGB);
         //ComboBoxSet (pPage, L"lucenrgb", pv->m_dwLucenRGB);
         ComboBoxSet (pPage, L"bumprgb", pv->m_dwBumpRGB);

         MeasureToString (pPage, L"bumpheight", pv->m_fBumpHeight, TRUE);

         pControl = pPage->ControlFind (L"cached");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fCached);

         pControl = pPage->ControlFind (L"transuse");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTransUse);
         pControl = pPage->ControlFind (L"transdist");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)pv->m_dwTransDist);

         FillStatusColor (pPage, L"transcolor", pv->m_cTransColor);
         FillStatusColor (pPage, L"defcolor", pv->m_cDefColor);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         DWORD dwBrowse = 0;
         PWSTR pszBrowse = NULL;
         if (!_wcsicmp(p->pControl->m_pszName, L"browse")) {
            dwBrowse = 1;
            pszBrowse = pv->m_szFile;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"transbrowse")) {
            dwBrowse = 2;
            pszBrowse = pv->m_szTransFile;
         }
         //else if (!_wcsicmp(p->pControl->m_pszName, L"lucenbrowse")) {
         //   dwBrowse = 3;
         //   pszBrowse = pv->m_szLucenFile;
         // }
         else if (!_wcsicmp(p->pControl->m_pszName, L"glossbrowse")) {
            dwBrowse = 4;
            pszBrowse = pv->m_szGlossFile;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"bumpbrowse")) {
            dwBrowse = 5;
            pszBrowse = pv->m_szBumpFile;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"glowbrowse")) {
            dwBrowse = 6;
            pszBrowse = pv->m_szGlowFile;
         };

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"cached")) {
            pv->m_fCached = p->pControl->AttribGetBOOL (Checked());

            if (pv->m_fCached)
               pv->LoadImageFromDisk();
            return TRUE;
         }
         else if (dwBrowse) {
            OPENFILENAME   ofn;
            char  szTemp[256];
            szTemp[0] = 0;
            memset (&ofn, 0, sizeof(ofn));
   
            // BUGFIX - Set directory
            char szInitial[256];
            GetLastDirectory(szInitial, sizeof(szInitial));
            if (pv->m_szFile[0]) {
               WideCharToMultiByte (CP_ACP, 0, pszBrowse, -1, szInitial, sizeof(szInitial), 0, 0);
            }
            ofn.lpstrInitialDir = szInitial;

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
            ofn.hInstance = ghInstance;
            ofn.lpstrFilter = "All (*.bmp,*.jpg)\0*.bmp;*.jpg\0JPEG (*.jpg)\0*.jpg\0Bitmap (*.bmp)\0*.bmp\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Open image file";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "jpg";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;   // failed to specify file so go back

            // convert to unicode
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, pszBrowse, sizeof(pv->m_szFile)/2);

            PCEscControl pControl;
            PWSTR pszEdit;
            switch (dwBrowse) {
            case 1:
               pszEdit = L"file";
               break;
            case 2:
               pszEdit = L"transfile";
               break;
            //case 3:
            //   pszEdit = L"lucenfile";
            //   break;
            case 4:
               pszEdit = L"glossfile";
               break;
            case 5:
               pszEdit = L"bumpfile";
               break;
            case 6:
               pszEdit = L"glowfile";
               break;
            }
            pControl = pPage->ControlFind (pszEdit);
            if (pControl)
               pControl->AttribSet (Text(), pszBrowse);

            if (pv->m_fCached)
               pv->LoadImageFromDisk();
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changetrans")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cTransColor, pPage, L"transcolor");
            if (cr != pv->m_cTransColor) {
               pv->m_cTransColor = cr;
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"changedef")) {
            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cDefColor, pPage, L"defcolor");
            if (cr != pv->m_cDefColor) {
               pv->m_cDefColor = cr;
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"transuse")) {
            pv->m_fTransUse = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;
   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         DWORD dwVal;
         dwVal = (DWORD) p->pControl->AttribGetInt (Pos());
         if (!_wcsicmp(p->pControl->m_pszName, L"transdist")) {
            if (dwVal != pv->m_dwTransDist) {
               pv->m_dwTransDist = dwVal;
            }
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;   // default

         DWORD dwFile = 0;
         PWSTR pszFile = NULL;
         if (!_wcsicmp(p->pControl->m_pszName, L"File")) {
            dwFile = 1;
            pszFile = pv->m_szFile;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"transFile")) {
            dwFile = 2;
            pszFile = pv->m_szTransFile;
         }
         //else if (!_wcsicmp(p->pControl->m_pszName, L"lucenFile")) {
         //   dwFile = 3;
         //   pszFile = pv->m_szLucenFile;
         //}
         else if (!_wcsicmp(p->pControl->m_pszName, L"glossFile")) {
            dwFile = 4;
            pszFile = pv->m_szGlossFile;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"bumpFile")) {
            dwFile = 5;
            pszFile = pv->m_szBumpFile;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"glowFile")) {
            dwFile = 6;
            pszFile = pv->m_szGlowFile;
         };

         if (dwFile) {
            DWORD dwNeeded;
            p->pControl->AttribGet (Text(), pszFile, sizeof(pv->m_szFile), &dwNeeded);
            if (pv->m_fCached)
               pv->LoadImageFromDisk();
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"bumpheight")) {
            MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_fBumpHeight);
            pv->m_fBumpHeight = max(pv->m_fBumpHeight, CLOSE);
         }
      }
      break;


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         DWORD dwVal;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(p->pControl->m_pszName, L"transrgb")) {
            pv->m_dwTransRGB = dwVal;
            return TRUE;
         }
         //else if (!_wcsicmp(p->pControl->m_pszName, L"lucenrgb")) {
         //   pv->m_dwLucenRGB = dwVal;
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pControl->m_pszName, L"bumprgb")) {
            pv->m_dwBumpRGB = dwVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"glossrgb")) {
            pv->m_dwGlossRGB = dwVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"width", &pv->m_fWidth);
         pv->m_fWidth = max(.001, pv->m_fWidth);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Bitmap or JPEG image";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextCreatorImageFile::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREIMAGEFILE, ImageFilePage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorImageFile::CTextCreatorImageFile (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_FLAT);
   memset (m_szFile, 0, sizeof(m_szFile));
   m_fCached = FALSE;
   DWORD i;
   for (i = 0; i < TIFCACHE; i++)
      m_amemCache[i].m_dwCurPosn = 0;
   m_fWidth = 1.0;
   m_dwX = m_dwY = 0;

   m_cDefColor = RGB(0x80, 0x80,0x80);
   m_szTransFile[0] = 0;
   m_szLucenFile[0] = 0;
   m_szGlossFile[0] = 0;
   m_szBumpFile[0] = 0;
   m_szGlowFile[0] = 0;
   m_dwTransRGB = m_dwLucenRGB = m_dwBumpRGB = m_dwGlossRGB = 0;
   m_fBumpHeight = .001;

   m_cTransColor = RGB(0xff,0xff,0xff);
   m_dwTransDist = 10;
   m_fTransUse = FALSE;
}

void CTextCreatorImageFile::Delete (void)
{
   delete this;
}

static PWSTR gpszImageFile = L"ImageFile";
static PWSTR gpszFile = L"File";
static PWSTR gpszCached = L"Cached";
static PWSTR gpszCache = L"Cache";
static PWSTR gpszX = L"X";
static PWSTR gpszY = L"Y";
static PWSTR gpszDefColor = L"DefColor";
static PWSTR gpszTransFile = L"TransFile";
static PWSTR gpszLucenFile = L"LucenFile";
static PWSTR gpszGlossFile = L"GlossFile";
static PWSTR gpszBumpFile = L"BumpFile";
static PWSTR gpszGlowFile = L"GlowFile";
static PWSTR gpszTransRGB = L"TransRGB";
static PWSTR gpszLucenRGB = L"LucenRGB";
static PWSTR gpszBumpRGB = L"BumpRGB";
static PWSTR gpszGlossRGB = L"GlossRGB";
static PWSTR gpszBumpHeight = L"BumpHeight";

PCMMLNode2 CTextCreatorImageFile::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszImageFile);

   m_Material.MMLTo(pNode);

   if (m_szFile[0])
      MMLValueSet (pNode, gpszFile, m_szFile);

   MMLValueSet (pNode, gpszDefColor, (int) m_cDefColor);
   if (m_szTransFile)
      MMLValueSet (pNode, gpszTransFile, m_szTransFile);
   if (m_szLucenFile)
      MMLValueSet (pNode, gpszLucenFile, m_szLucenFile);
   if (m_szGlossFile)
      MMLValueSet (pNode, gpszGlossFile, m_szGlossFile);
   if (m_szBumpFile)
      MMLValueSet (pNode, gpszBumpFile, m_szBumpFile);
   if (m_szGlowFile)
      MMLValueSet (pNode, gpszGlowFile, m_szGlowFile);
   MMLValueSet (pNode, gpszTransRGB, (int) m_dwTransRGB);
   MMLValueSet (pNode, gpszGlossRGB, (int) m_dwGlossRGB);
   MMLValueSet (pNode, gpszLucenRGB, (int) m_dwLucenRGB);
   MMLValueSet (pNode, gpszBumpRGB, (int) m_dwBumpRGB);
   MMLValueSet (pNode, gpszBumpHeight, m_fBumpHeight);


   MMLValueSet (pNode, gpszCached, (int) m_fCached);
   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < TIFCACHE; i++) if (m_fCached && m_amemCache[i].m_dwCurPosn) {
      // CMem  mem;
      swprintf (szTemp, L"Cache%d", (int) i);

      // BUGFIX - Use binary to string MMLValueSetBinary directly
      MMLValueSet (pNode, szTemp, (PBYTE) m_amemCache[i].p, m_amemCache[i].m_dwCurPosn);
      //if (mem.Required (m_amemCache[i].m_dwCurPosn * 4 + 4)) {
      //   MMLBinaryToString ((PBYTE) m_amemCache[i].p, m_amemCache[i].m_dwCurPosn, (PWSTR) mem.p);
      //   MMLValueSet (pNode, szTemp, (PWSTR) mem.p);
      //}
   }

   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszX, (int) m_dwX);
   MMLValueSet (pNode, gpszY, (int) m_dwY);
   MMLValueSet (pNode, gpszTransColor, (int)m_cTransColor);
   MMLValueSet (pNode, gpszTransDist, (int)m_dwTransDist);
   MMLValueSet (pNode, gpszTransUse, (int)m_fTransUse);

   return pNode;
}

BOOL CTextCreatorImageFile::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_szFile[0] = 0;
   m_szTransFile[0] = 0;
   m_szLucenFile[0] = 0;
   m_szGlossFile[0] = 0;
   m_szBumpFile[0] = 0;
   m_szGlowFile[0] = 0;

   PWSTR psz;
   if (psz = MMLValueGet (pNode, gpszFile))
      wcscpy (m_szFile, psz);
   if (psz = MMLValueGet (pNode, gpszTransFile))
      wcscpy (m_szTransFile, psz);
   if (psz = MMLValueGet (pNode, gpszLucenFile))
      wcscpy (m_szLucenFile, psz);
   if (psz = MMLValueGet (pNode, gpszGlossFile))
      wcscpy (m_szGlossFile, psz);
   if (psz = MMLValueGet (pNode, gpszBumpFile))
      wcscpy (m_szBumpFile, psz);
   if (psz = MMLValueGet (pNode, gpszGlowFile))
      wcscpy (m_szGlowFile, psz);

   m_cDefColor = (COLORREF) MMLValueGetInt (pNode, gpszDefColor, (int) 0);
   m_dwTransRGB = (DWORD) MMLValueGetInt (pNode, gpszTransRGB, (int) 0);
   m_dwGlossRGB = (DWORD) MMLValueGetInt (pNode, gpszGlossRGB, (int) 0);
   m_dwLucenRGB = (DWORD) MMLValueGetInt (pNode, gpszLucenRGB, (int) 0);
   m_dwBumpRGB = (DWORD) MMLValueGetInt (pNode, gpszBumpRGB, (int) 0);
   m_fBumpHeight = MMLValueGetDouble (pNode, gpszBumpHeight, .0001);

   m_fCached = (BOOL) MMLValueGetInt (pNode, gpszCached, (int) 0);
   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, 1);
   m_dwX = MMLValueGetInt (pNode, gpszX, (int) 0);
   m_dwY = MMLValueGetInt (pNode, gpszY, (int) 0);
   m_cTransColor = (COLORREF) MMLValueGetInt (pNode, gpszTransColor, (int)0xffffff);
   m_dwTransDist = (DWORD) MMLValueGetInt (pNode, gpszTransDist, (int)10);
   m_fTransUse = (BOOL) MMLValueGetInt (pNode, gpszTransUse, (int)0);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < TIFCACHE; i++){
      //CMem  mem;
      m_amemCache[i].m_dwCurPosn = 0;

      swprintf (szTemp, L"Cache%d", (int) i);

      // BUGFIX - Use MMLValueGetBinary directly
      MMLValueGetBinary (pNode, szTemp, &m_amemCache[i]);

      //psz = MMLValueGet (pNode, szTemp);
      //if (!psz)
      //   continue;

      //DWORD dwNeed;
      //dwNeed = wcslen(psz) / 2;
      //if (!m_amemCache[i].Required (dwNeed))
      //   continue;

      //m_amemCache[i].m_dwCurPosn = dwNeed;
      //MMLBinaryFromString (psz, (PBYTE) m_amemCache[i].p, dwNeed);
   }

   return TRUE;
}

// Use m_szFile and try to load from disk
// if fChangeTrans is TRUE then it uses pixel 0,0 to determine the transparency color
BOOL CTextCreatorImageFile::LoadImageFromDisk (void)
{
   // clear the cache
   DWORD i;
   for (i = 0; i < TIFCACHE; i++)
      m_amemCache[i].m_dwCurPosn = 0;

   // list of files
   PWSTR papsz[TIFCACHE];
   papsz[0] = m_szFile;
   papsz[1] = m_szTransFile;
   papsz[2] = m_szLucenFile;
   papsz[3] = m_szGlossFile;
   papsz[4] = m_szBumpFile;
   papsz[5] = m_szGlowFile;

   // cache all that can
   DWORD j;
   for (i = 0; i < TIFCACHE; i++) {
      if ( (papsz[i])[0] == 0)
         continue;

      // if name matches any of the files above it don't load since already loaded once
      for (j = 0; j < i; j++)
         if (!_wcsicmp(papsz[i], papsz[j]))
            break;
      if (j < i)
         continue;   // match

      FileToCache (&m_amemCache[i], papsz[i]);
   }

   // find the first one and get it's size
   for (i = 0; i < TIFCACHE; i++)
      if (m_amemCache[i].m_dwCurPosn)
         break;
   m_dwX = m_dwY = 10;
   if (i < TIFCACHE) {
      CImage Image;
      if (CacheToImage (&Image, &m_amemCache[i])) {
         m_dwX = Image.Width();
         m_dwY = Image.Height();
      }
   }
   
   return TRUE;
}

BOOL CTextCreatorImageFile::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   // if not cached then load from disk
   if (!m_fCached)
      LoadImageFromDisk ();

   m_dwX = max(1,m_dwX);
   m_dwY = max(1,m_dwY);

   // main image
   BOOL fRet;
   fRet = FALSE;
   if (m_amemCache[0].m_dwCurPosn)
      fRet = CacheToImage (pImage, &m_amemCache[0]);
   if (!fRet)
      pImage->Init (m_dwX, m_dwY, m_cDefColor);

   m_fWidth = max(.001, m_fWidth);

   // strings for matches
   PWSTR papsz[TIFCACHE];
   papsz[0] = m_szFile;
   papsz[1] = m_szTransFile;
   papsz[2] = m_szLucenFile;
   papsz[3] = m_szGlossFile;
   papsz[4] = m_szBumpFile;
   papsz[5] = m_szGlowFile;

   // load in all the other caches
   CImage   aiCache[TIFCACHE];
   PCImage  paiCache[TIFCACHE];
   paiCache[0] = fRet ? pImage : NULL; // use fRet from loading in main image
   DWORD i, j;
   for (i = 1; i < TIFCACHE; i++) {
      paiCache[i] = NULL;  // assume none

      if (!(papsz[i])[0])
         continue;   // no entry

      if (m_amemCache[i].m_dwCurPosn) {
         if (!CacheToImage (&aiCache[i], &m_amemCache[i]))
            continue;   // error
         paiCache[i] = &aiCache[i];
         continue;
      }

      // else, see if already loaded
      for (j = 0; j < i; j++)
         if (!_wcsicmp(papsz[i], papsz[j])) {
            paiCache[i] = paiCache[j];
            break;
         }
   }

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = m_fWidth / (fp) m_dwX;
   pTextInfo->dwMap = 0;
   // dont call this because no maps - MassageForCreator (pImage, pMaterial, pTextInfo);

   // if transparency then apply that
   if (m_fTransUse) {
      ColorToTransparency (pImage, m_cTransColor, m_dwTransDist, &m_Material);
      pTextInfo->dwMap |= (0x04 | 0x01);  // transparency map and specularity map
   }

   // Transparency from image?
   PIMAGEPIXEL pipS, pipD;
   DWORD dwNum;
   dwNum = pImage->Width() * pImage->Height();
   if (paiCache[1] && (paiCache[1]->Width() == pImage->Width())&& (paiCache[1]->Height() == pImage->Height()) ) {
      pipS = paiCache[1]->Pixel(0,0);
      pipD = pImage->Pixel(0,0);
      BYTE bTrans;

      for (i = 0; i < dwNum; i++, pipS++, pipD++) {
         bTrans = UnGamma((&pipS->wRed)[m_dwTransRGB]);
         pipD->dwIDPart = (pipD->dwIDPart & ~(0xff)) | bTrans;
      }

      // note have map
      pTextInfo->dwMap |= 0x04;
   }


   // Specularity
   if (paiCache[3] && (paiCache[3]->Width() == pImage->Width())&& (paiCache[3]->Height() == pImage->Height()) ) {
      pipS = paiCache[3]->Pixel(0,0);
      pipD = pImage->Pixel(0,0);
      WORD wSpec;

      for (i = 0; i < dwNum; i++, pipS++, pipD++) {
         wSpec = (&pipS->wRed)[m_dwTransRGB];
         pipD->dwID = pMaterial->m_wSpecExponent; // since no map for this
         pipD->dwID = (pipD->dwID & 0xffff) | ((DWORD) wSpec << 16);
      }

      // note have map
      pTextInfo->dwMap |= 0x01;
   }

   // Bump map
   if (paiCache[4] && (paiCache[4]->Width() == pImage->Width())&& (paiCache[4]->Height() == pImage->Height()) ) {
      pipS = paiCache[4]->Pixel(0,0);
      pipD = pImage->Pixel(0,0);
      BYTE bBump;
      fp fScale;
      fScale = m_fBumpHeight / pTextInfo->fPixelLen / 256.0;

      for (i = 0; i < dwNum; i++, pipS++, pipD++) {
         bBump = UnGamma((&pipS->wRed)[m_dwTransRGB]);
         pipD->fZ = ((fp) bBump - (fp)0x80) * fScale;
      }

      // note have map
      pTextInfo->dwMap |= 0x02;
   }

   // Glow
   if (paiCache[5] && (paiCache[5]->Width() == pImage->Width())&& (paiCache[5]->Height() == pImage->Height()) ) {
      pImage2->Init (m_dwX, m_dwY, 0);
      pipS = paiCache[5]->Pixel(0,0);
      pipD = pImage2->Pixel(0,0);

      for (i = 0; i < dwNum; i++, pipS++, pipD++) {
         pipD->wRed = pipS->wRed;
         pipD->wGreen = pipS->wGreen;
         pipD->wBlue = pipS->wBlue;
      }

      // note have map
      pTextInfo->dwMap |= 0x08;
      pTextInfo->fGlowScale = 1.0;  // so as in sunlight
   }

   return TRUE;
}







/***********************************************************************************
CTextCreatorIris - Noise functions
*/

class CTextCreatorIris : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorIris (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in meters

   DWORD            m_dwNoiseSizeH; // number of noise elements in H
   DWORD            m_dwNoiseSizeV; // number of noise elements in V
   COLORREF         m_acColor[3];   // colors
   TEXTUREPOINT     m_atpAdjust[3]; // adjust random value on different edge of iris, for each color
};
typedef CTextCreatorIris *PCTextCreatorIris;


/****************************************************************************
IrisPage
*/
BOOL IrisPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorIris pv = (PCTextCreatorIris) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

         DoubleToControl (pPage, L"noisesizeh", pv->m_dwNoiseSizeH);
         DoubleToControl (pPage, L"noisesizev", pv->m_dwNoiseSizeV);

         FillStatusColor (pPage, L"colorcolor0", pv->m_acColor[0]);
         FillStatusColor (pPage, L"colorcolor1", pv->m_acColor[1]);
         FillStatusColor (pPage, L"colorcolor2", pv->m_acColor[2]);

         WCHAR szTemp[32];
         DWORD i, j;
         for (i = 0; i < 3; i++) for (j = 0; j < 2; j++) {
            if ((i == 2) && j)
               continue;   // dont have this scrollbar

            swprintf (szTemp, L"adjust%d%d", (int)i, (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)((j ? pv->m_atpAdjust[i].v : pv->m_atpAdjust[i].h) * 100));
         } // i,j
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton0")) {
            pv->m_acColor[0] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColor[0], pPage, L"colorcolor0");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton1")) {
            pv->m_acColor[1] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColor[1], pPage, L"colorcolor1");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton2")) {
            pv->m_acColor[2] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acColor[2], pPage, L"colorcolor2");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwNoiseSizeH = (DWORD) DoubleFromControl (pPage, L"noisesizeh");
         pv->m_dwNoiseSizeH = max(pv->m_dwNoiseSizeH, 1);

         pv->m_dwNoiseSizeV = (DWORD) DoubleFromControl (pPage, L"noisesizev");
         pv->m_dwNoiseSizeV = max(pv->m_dwNoiseSizeV, 1);

         PCEscControl pControl;
         WCHAR szTemp[32];
         DWORD i, j;
         for (i = 0; i < 3; i++) for (j = 0; j < 2; j++) {
            if ((i == 2) && j)
               continue;   // dont have this scrollbar

            swprintf (szTemp, L"adjust%d%d", (int)i, (int)j);
            pControl = pPage->ControlFind (szTemp);
            fp f = 0;
            if (pControl)
               f = pControl->AttribGetInt (Pos()) / 100.0;
            if (j)
               pv->m_atpAdjust[i].v = f;
            else
               pv->m_atpAdjust[i].h = f;
         } // i,j
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Iris";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorIris::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREIRIS, IrisPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorIris::CTextCreatorIris (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_acColor[0] = RGB(0x80, 0xb0, 0xe0);
   m_acColor[2] = RGB(0x80, 0xe0, 0xd0);
   m_acColor[1] = RGB(0x40, 0x40, 0xff);

   m_fPixelLen = .0001;
   m_iSeed = 12345;
   m_fPatternWidth = .01;
   m_fPatternHeight = .01;

   m_dwNoiseSizeH = 50;
   m_dwNoiseSizeV = 10;
   m_atpAdjust[0].h = 1;
   m_atpAdjust[0].v = -1;
   m_atpAdjust[1].h = -1;
   m_atpAdjust[1].v = 1;
   m_atpAdjust[2].h = m_atpAdjust[2].v = 0.75;
}

void CTextCreatorIris::Delete (void)
{
   delete this;
}


static PWSTR gpszNoiseSizeH = L"NoiseSizeH";
static PWSTR gpszNoiseSizeV = L"NoiseSizeV";

PCMMLNode2 CTextCreatorIris::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);

   MMLValueSet (pNode, gpszNoiseSizeH, (int)m_dwNoiseSizeH);
   MMLValueSet (pNode, gpszNoiseSizeV, (int)m_dwNoiseSizeV);

   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_acColor[i]);

      swprintf (szTemp, L"Adjust%d", (int)i);
      MMLValueSet (pNode, szTemp, &m_atpAdjust[i]);
   } // i

   return pNode;
}

BOOL CTextCreatorIris::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);

   m_dwNoiseSizeH = (DWORD) MMLValueGetInt (pNode, gpszNoiseSizeH, (int)1);
   m_dwNoiseSizeV = (DWORD) MMLValueGetInt (pNode, gpszNoiseSizeV, (int)1);

   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"color%d", (int)i);
      m_acColor[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)m_acColor[i]);

      swprintf (szTemp, L"Adjust%d", (int)i);
      MMLValueGetTEXTUREPOINT (pNode, szTemp, &m_atpAdjust[i]);
   } // i

   return TRUE;
}

BOOL CTextCreatorIris::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwScale = max(dwX, dwY);


   // create the surface
   pImage->Init (dwX, dwY, RGB(0xff,0,0), 0);

   // create the noise
   CNoise2D aNoise[2];
   aNoise[0].Init (m_dwNoiseSizeH, m_dwNoiseSizeV);
   aNoise[1].Init (m_dwNoiseSizeH, m_dwNoiseSizeV);

   // gamma correct the color
   WORD awOrig[3][3];
   DWORD x,y, i;
   GammaInit();
   for (i = 0; i < 3; i++)
      Gamma (m_acColor[i], awOrig[i]);
   
   // loop through all the pixels
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++, pip++) {
      // get the three scores
      fp afScore[3];
      fp fAlpha = (fp)y / (fp)dwY;
      afScore[0] = aNoise[0].Value ((fp)x / (fp)dwX, fAlpha) +
         (1.0 - fAlpha) * m_atpAdjust[0].v + fAlpha * m_atpAdjust[0].h;
      afScore[1] = aNoise[1].Value ((fp)x / (fp)dwX, fAlpha) +
         (1.0 - fAlpha) * m_atpAdjust[1].v + fAlpha * m_atpAdjust[1].h;
      afScore[2] = m_atpAdjust[2].h;

      // find the max
      fp fMax = max(max(afScore[0], afScore[1]), afScore[2]);

      // calculate blending funciton
#define BLENDAMT     .5
      fMax -= BLENDAMT;
      fp fSum = 0;
      for (i = 0; i < 3; i++) {
         if (afScore[i] > fMax)
            afScore[i] -= fMax;
         else
            afScore[i] = 0;
         fSum += afScore[i];
      }

      // blend the colors
      WORD awColor[3];
      fp f;
      for (i = 0; i < 3; i++) {
         f = afScore[0] * (fp)awOrig[0][i] + afScore[1] * (fp)awOrig[1][i]
            + afScore[2] * (fp)awOrig[2][i];
         f /= fSum;
         f = min(f, (fp)0xffff);
         f = max(f, 0);

         awColor[i] = (WORD)f;
      }

      pip->wRed = awColor[0];
      pip->wGreen = awColor[1];
      pip->wBlue = awColor[2];
   } // x,y


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}





/***********************************************************************************
CTextCreatorBloodVessels - Noise functions
*/

class CTextCreatorBloodVessels : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorBloodVessels (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   DWORD             m_dwRenderShard;
   CMaterial        m_Material;  // material to use
   DWORD            m_dwType; // initial type of tile - when constructed
   int              m_iSeed;   // seed for the random
   fp               m_fPixelLen;  // meters per pixel
   fp               m_fPatternWidth; // pattern width in meters
   fp               m_fPatternHeight;   // pattern height in meters

   COLORREF         m_cWhite;       // color of eye whites
   COLORREF         m_cBlood;       // color of blood
   fp               m_fBloodAmt;    // amount of blood showing. 0 = none, 1 = max
};
typedef CTextCreatorBloodVessels *PCTextCreatorBloodVessels;


/****************************************************************************
BloodVesselsPage
*/
BOOL BloodVesselsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorBloodVessels pv = (PCTextCreatorBloodVessels) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

         FillStatusColor (pPage, L"colorcolor0", pv->m_cWhite);
         FillStatusColor (pPage, L"colorcolor1", pv->m_cBlood);

         pControl = pPage->ControlFind (L"bloodamt");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBloodAmt * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton0")) {
            pv->m_cWhite = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cWhite, pPage, L"colorcolor0");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton1")) {
            pv->m_cBlood = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cBlood, pPage, L"colorcolor1");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"bloodamt");
         if (pControl)
               pv->m_fBloodAmt = pControl->AttribGetInt (Pos()) / 100.0;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Blood Vessels";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorBloodVessels::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREBLOODVESSELS, BloodVesselsPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorBloodVessels::CTextCreatorBloodVessels (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_cWhite = RGB(0xff, 0xff, 0xf0);
   m_cBlood = RGB(0xc0, 0x20, 0x20);

   m_fPixelLen = .0001;
   m_iSeed = 12345;
   m_fPatternWidth = .01;
   m_fPatternHeight = .01;
   m_fBloodAmt = .25;
}

void CTextCreatorBloodVessels::Delete (void)
{
   delete this;
}


static PWSTR gpszWhite = L"White";
static PWSTR gpszBlood = L"Blood";
static PWSTR gpszBloodAmt = L"BloodAmt";

PCMMLNode2 CTextCreatorBloodVessels::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);

   MMLValueSet (pNode, gpszWhite, (int)m_cWhite);
   MMLValueSet (pNode, gpszBlood, (int)m_cBlood);
   MMLValueSet (pNode, gpszBloodAmt, m_fBloodAmt);

   return pNode;
}

BOOL CTextCreatorBloodVessels::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);

   m_cWhite = (COLORREF) MMLValueGetInt (pNode, gpszWhite, 0);
   m_cBlood = (COLORREF) MMLValueGetInt (pNode, gpszBlood, 0);
   m_fBloodAmt = MMLValueGetDouble (pNode, gpszBloodAmt, 0);

   return TRUE;
}

BOOL CTextCreatorBloodVessels::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwScale = max(dwX, dwY);


   // create the surface
   pImage->Init (dwX, dwY, RGB(0xff,0,0), 0);

   // create a buffer of floats for the score
   CMem mem;
   float *paf, *pafFinal;
   DWORD *pdwTrickleTo;
   if (!mem.Required (dwX * dwY * sizeof(float) * 2 + dwX * sizeof(DWORD)))
      return FALSE;
   paf = (float*) mem.p;
   pafFinal = paf + dwX*dwY;
   pdwTrickleTo = (DWORD*) (pafFinal + dwX*dwY);

   // fill buffer with random values between 0 and 1
   DWORD x,y, i;
   int iVal;
   CNoise2D Noise;
   Noise.Init (10, 10);
   for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++) {
      fp fx = (fp)x / (fp)dwX;
      fp fy = (fp)y / (fp)dwY;
      paf[x+y*dwX] = fabs(Noise.Value (fx, fy) +
         0.5 * Noise.Value (fx * 2.0, fy * 2.0) +
         0.25 * Noise.Value (fx * 2.0, fy * 4.0));
   }


   // trickle down...
   for (y = 0; y+1 < dwY; y++) {
      // loop over all the values and see which direction they want to trickle
      // will trickle towards the greater blood supply
      float *pafLine = paf + y*dwX;
      float *pafLineNext = paf + (y+1)*dwX;
      for (x = 0; x < dwX; x++) for (i = 0; i < 3; i++) {
         iVal = ((int)x + (int) i - 1 + (int)dwX) % (int)dwX;
         if (!i || (pafLineNext[iVal] > pafLineNext[pdwTrickleTo[x]]))
            pdwTrickleTo[x] = (DWORD)iVal;
      } // x, i

      // incrememnt next line with greater blood supply
      for (x = 0; x < dwX; x++)
         pafLineNext[pdwTrickleTo[x]] += pafLine[x];
   } // y

   // find the maximum blood supply at the end
   fp fMax = 0;
   for (x = 0; x < dwX; x++)
      fMax = max(fMax, paf[x + (dwY-1)*dwX]);

   // ungamma
   WORD awWhite[3], awBlood[3];
   GammaInit();
   Gamma (m_cWhite, awWhite);
   Gamma (m_cBlood, awBlood);

   // fill in the pixels
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   int ix, iy;
   fp fScore;
   fMax /= (m_fBloodAmt * m_fBloodAmt * 32.0 + 0.5);
   for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++) {
      fScore = paf[x+y*dwX];

      // look for overglow
      for (iy = -2; iy <= 2; iy++) for (ix = -2; ix <= 2; ix++) {
         if ((iy + (int)y < 0) || (iy + (int)y >= (int)dwY))
            continue;   // out of range
         if (!ix && !iy)
            continue;   // dont count self

         fp fDist = (iy*iy + ix*ix);   // BUGFIX - take out sqrt
         fp fOver = paf[((int)x+(int)dwX+ix)%(int)dwX + ((int)y + (int)iy) * (int)dwX] / fDist;
         if (fOver > fMax)
            fScore += (fOver - fMax);
      } // ix,iy

      fScore /= fMax;
      fScore = min(fScore, 1);   // cant be any more than 1

      pafFinal[x+y*dwX] = fScore;
   } // y,x

   // do some filtering
   for (y = 0; y < dwY; y++) for (x = 0; x < dwX; x++, pip++) {
      fScore = 0;
      DWORD dwCount = 0;
      DWORD dwWeight = 0;

      // filter
      for (iy = -1; iy <= 1; iy++) for (ix = -1; ix <= 1; ix++) {
         if ((iy + (int)y < 0) || (iy + (int)y >= (int)dwY))
            continue;   // out of range
         
         dwWeight = (DWORD)(2-abs(iy) + 2-abs(ix));
         dwCount += dwWeight;
         fScore += pafFinal[((int)x+(int)dwX+ix)%(int)dwX + ((int)y + (int)iy) * (int)dwX] * (fp)dwWeight;
      } // ix,iy

      fScore /= (fp)dwCount;
      fScore = min(fScore, 1);   // cant be any more than 1

      // blend in colors
      pip->wRed = (WORD)(fScore * (fp)awBlood[0] + (1.0 - fScore) * (fp)awWhite[0]);
      pip->wGreen = (WORD)(fScore * (fp)awBlood[1] + (1.0 - fScore) * (fp)awWhite[1]);
      pip->wBlue = (WORD)(fScore * (fp)awBlood[2] + (1.0 - fScore) * (fp)awWhite[2]);
   } // y,x


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}








/***********************************************************************************
CTextCreatorHair - Noise functions
*/

class CTextCreatorHair : public CTextCreatorSocket {
public:
   ESCNEWDELETE;

   CTextCreatorHair (DWORD dwRenderShard, DWORD dwType);
   virtual void Delete (void);
   virtual BOOL MMLFrom (PCMMLNode2 pNode);
   virtual PCMMLNode2 MMLTo (void);
   virtual BOOL Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo);
   virtual BOOL Dialog (PCEscWindow pWindow);

   //void DrawHair (PCImage pImage, WORD *pawColor, DWORD dwNum,
   //                              PCPoint papPoint, fp fWidth, BOOL fAutoDepth,
   //                              BOOL fHair);

   DWORD             m_dwRenderShard;
   CMaterial         m_Material;  // material to use
   DWORD             m_dwType; // initial type of tile - when constructed
   int               m_iSeed;   // seed for the random
   fp                m_fPixelLen;  // meters per pixel
   fp                m_fPatternWidth; // pattern width in meters
   fp                m_fPatternHeight;   // pattern height in meters
   BOOL              m_fFlatten;       // if TRUE flatten so renders faster

   BOOL              m_fFur;           // if TRUE then draw as fur, else hair
   DWORD             m_dwHairPerLayer; // draw this many hairs in a row at a time
   DWORD             m_dwHairWavePoint;// number of points of waviness in hair
   fp                m_fHairWaviness;  // how wavy, in meters
   TEXTUREPOINT      m_tpFurLength;    // .h is min fur length, .v is max, in meters
   TEXTUREPOINT      m_tpFurAngleUD;   // .h is min angle (radians) hair is up, .v is max
   fp                m_fFurAngleLR;    // the scope of the LR angle (in radians)

   DWORD             m_dwNumHairs;     // number of hairs to draw
   fp                m_afHairThick[2]; // hair thickness, in m
   COLORREF          m_acHairColor[2]; // hair color range
   COLORREF          m_cHairOdd;       // odd-hair colors (like grey within black)
   fp                m_fHairOddProb;   // probability of odd hairs, 0..1
   COLORREF          m_cBackground;    // background color
   BOOL              m_fTransparent;   // set to TRUE if background transparent
   fp                m_fBlendBackground;  // amount to blend in hairs with background as get deeper, 0=none, 1=full
};
typedef CTextCreatorHair *PCTextCreatorHair;


/****************************************************************************
HairPage
*/
BOOL HairPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorHair pv = (PCTextCreatorHair) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

         MeasureToString (pPage, L"furlength0", pv->m_tpFurLength.h, TRUE);
         MeasureToString (pPage, L"furlength1", pv->m_tpFurLength.v, TRUE);
         MeasureToString (pPage, L"hairthick0", pv->m_afHairThick[0], TRUE);
         MeasureToString (pPage, L"hairthick1", pv->m_afHairThick[1], TRUE);
         MeasureToString (pPage, L"hairwaviness", pv->m_fHairWaviness, TRUE);

         AngleToControl (pPage, L"furangleud0", pv->m_tpFurAngleUD.h);
         AngleToControl (pPage, L"furangleud1", pv->m_tpFurAngleUD.v);
         AngleToControl (pPage, L"furanglelr", pv->m_fFurAngleLR);

         DoubleToControl (pPage, L"hairperlayer", pv->m_dwHairPerLayer);
         DoubleToControl (pPage, L"hairwavepoint", pv->m_dwHairWavePoint);
         DoubleToControl (pPage, L"numhairs", pv->m_dwNumHairs);

         pControl = pPage->ControlFind (L"fur");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fFur);
         pControl = pPage->ControlFind (L"transparent");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTransparent);
         pControl = pPage->ControlFind (L"flatten");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fFlatten);

         FillStatusColor (pPage, L"colorcolor0", pv->m_acHairColor[0]);
         FillStatusColor (pPage, L"colorcolor1", pv->m_acHairColor[1]);
         FillStatusColor (pPage, L"colorcolor2", pv->m_cHairOdd);
         FillStatusColor (pPage, L"colorcolor3", pv->m_cBackground);

         pControl = pPage->ControlFind (L"hairoddprob");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHairOddProb * 100));
         pControl = pPage->ControlFind (L"blendbackground");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBlendBackground * 100));
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton0")) {
            pv->m_acHairColor[0] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acHairColor[0], pPage, L"colorcolor0");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton1")) {
            pv->m_acHairColor[1] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acHairColor[1], pPage, L"colorcolor1");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton2")) {
            pv->m_cHairOdd = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cHairOdd, pPage, L"colorcolor2");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton3")) {
            pv->m_cBackground = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cBackground, pPage, L"colorcolor3");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.00001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         fp fTemp;
         MeasureParseString (pPage, L"furlength0", &fTemp);
         pv->m_tpFurLength.h = fTemp;
         pv->m_tpFurLength.h = max(pv->m_tpFurLength.h, CLOSE);
         MeasureParseString (pPage, L"furlength1", &fTemp);
         pv->m_tpFurLength.v = fTemp;
         pv->m_tpFurLength.v = max(pv->m_tpFurLength.v, CLOSE);
         MeasureParseString (pPage, L"hairthick0", &pv->m_afHairThick[0]);
         pv->m_afHairThick[0] = max(pv->m_afHairThick[0], CLOSE);
         MeasureParseString (pPage, L"hairthick1", &pv->m_afHairThick[1]);
         pv->m_afHairThick[1] = max(pv->m_afHairThick[1], CLOSE);

         pv->m_tpFurAngleUD.h = AngleFromControl (pPage, L"furangleud0");
         pv->m_tpFurAngleUD.h = max(pv->m_tpFurAngleUD.h, 0);
         pv->m_tpFurAngleUD.h = min(pv->m_tpFurAngleUD.h, PI);
         pv->m_tpFurAngleUD.v = AngleFromControl (pPage, L"furangleud1");
         pv->m_tpFurAngleUD.v = max(pv->m_tpFurAngleUD.v, 0);
         pv->m_tpFurAngleUD.v = min(pv->m_tpFurAngleUD.v, PI);
         pv->m_fFurAngleLR = AngleFromControl (pPage, L"furanglelr");

         pv->m_dwHairPerLayer = (DWORD)DoubleFromControl (pPage, L"hairperlayer");
         pv->m_dwHairPerLayer = max(pv->m_dwHairPerLayer, 1);
         pv->m_dwHairWavePoint = (DWORD)DoubleFromControl (pPage, L"hairwavepoint");
         pv->m_dwHairWavePoint = max(pv->m_dwHairWavePoint, 1);
         pv->m_dwNumHairs = (DWORD)DoubleFromControl (pPage, L"numhairs");
         pv->m_dwNumHairs = max(pv->m_dwNumHairs, 1);
         MeasureParseString (pPage, L"hairwaviness", &pv->m_fHairWaviness);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"fur");
         if (pControl)
            pv->m_fFur = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"transparent");
         if (pControl)
            pv->m_fTransparent = pControl->AttribGetBOOL (Checked());
         pControl = pPage->ControlFind (L"flatten");
         if (pControl)
            pv->m_fFlatten = pControl->AttribGetBOOL (Checked());

         pControl = pPage->ControlFind (L"hairoddprob");
         if (pControl)
            pv->m_fHairOddProb = pControl->AttribGetInt (Pos()) / 100.0;
         pControl = pPage->ControlFind (L"blendbackground");
         if (pControl)
            pv->m_fBlendBackground = pControl->AttribGetInt (Pos()) / 100.0;

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Hair and fur";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorHair::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREHAIR, HairPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorHair::CTextCreatorHair (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTGLOSS);
   m_dwType = dwType;
   m_iSeed = 0;
   m_iSeed = 12345;
   m_fPatternWidth = .02;
   m_fPatternHeight = .04;
   m_fPixelLen = .00005;

   m_fFur = FALSE;
   m_fFlatten = FALSE;
   m_dwHairPerLayer = 10;
   m_dwHairWavePoint = 5;
   m_fHairWaviness = m_fPatternWidth / (fp)m_dwHairPerLayer;
   m_tpFurLength.h = .015;
   m_tpFurLength.v = .020;
   m_tpFurAngleUD.h = PI/6;
   m_tpFurAngleUD.v = PI/4;
   m_fFurAngleLR = PI/4;
   m_dwNumHairs = 100;
   m_afHairThick[0] = 0.00020;
   m_afHairThick[1] = 0.00025;
   m_acHairColor[0] = RGB(0xc0, 0x80, 0);
   m_acHairColor[1] = RGB(0x80, 0x60, 0);
   m_cHairOdd = RGB(0xc0, 0xc0, 0xc0);
   m_fHairOddProb = 0;
   m_cBackground = RGB(0, 0, 0);
   m_fTransparent = FALSE;
   m_fBlendBackground = 0.5;

}

void CTextCreatorHair::Delete (void)
{
   delete this;
}

static PWSTR gpszFur = L"Fur";
static PWSTR gpszTransparent = L"Transparent";
static PWSTR gpszHairPerLayer = L"HairPerLayer";
static PWSTR gpszHairWavePoint = L"HairWavePoint";
static PWSTR gpszNumHairs = L"NumHairs";
static PWSTR gpszHairColor0 = L"HairColor0";
static PWSTR gpszHairColor1 = L"HairColor1";
static PWSTR gpszHairOdd = L"HairOdd";
static PWSTR gpszBackground = L"Background";
static PWSTR gpszHairOddProb = L"HairOddProb";
static PWSTR gpszHairWaviness = L"HairWaviness";
static PWSTR gpszFurAngleLR = L"FurAngleLR";
static PWSTR gpszHairThick0 = L"HairThick0";
static PWSTR gpszHairThick1 = L"HairThick1";
static PWSTR gpszBlendBackground = L"BlendBackground";
static PWSTR gpszFurLength = L"FurLength";
static PWSTR gpszFurAngleUD = L"FurAngleUD";
static PWSTR gpszFlatten = L"Flatten";

PCMMLNode2 CTextCreatorHair::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);

   MMLValueSet (pNode, gpszFur, (int)m_fFur);
   MMLValueSet (pNode, gpszFlatten, (int)m_fFlatten);
   MMLValueSet (pNode, gpszTransparent, (int)m_fTransparent);
   MMLValueSet (pNode, gpszHairPerLayer, (int)m_dwHairPerLayer);
   MMLValueSet (pNode, gpszHairWavePoint, (int)m_dwHairWavePoint);
   MMLValueSet (pNode, gpszNumHairs, (int)m_dwNumHairs);
   MMLValueSet (pNode, gpszHairColor0, (int)m_acHairColor[0]);
   MMLValueSet (pNode, gpszHairColor1, (int)m_acHairColor[1]);
   MMLValueSet (pNode, gpszHairOdd, (int)m_cHairOdd);
   MMLValueSet (pNode, gpszBackground, (int)m_cBackground);
   MMLValueSet (pNode, gpszHairOddProb, m_fHairOddProb);
   MMLValueSet (pNode, gpszHairWaviness, m_fHairWaviness);
   MMLValueSet (pNode, gpszFurAngleLR, m_fFurAngleLR);
   MMLValueSet (pNode, gpszHairThick0, m_afHairThick[0]);
   MMLValueSet (pNode, gpszHairThick1, m_afHairThick[1]);
   MMLValueSet (pNode, gpszBlendBackground, m_fBlendBackground);
   MMLValueSet (pNode, gpszFurLength, &m_tpFurLength);
   MMLValueSet (pNode, gpszFurAngleUD, &m_tpFurAngleUD);

   return pNode;
}

BOOL CTextCreatorHair::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);

   m_fFur = (BOOL) MMLValueGetInt (pNode, gpszFur, (int)0);
   m_fFlatten = (BOOL) MMLValueGetInt (pNode, gpszFlatten, (int)0);
   m_fTransparent = (BOOL) MMLValueGetInt (pNode, gpszTransparent, (int)0);
   m_dwHairPerLayer = (DWORD) MMLValueGetInt (pNode, gpszHairPerLayer, (int)1);
   m_dwHairWavePoint = (DWORD) MMLValueGetInt (pNode, gpszHairWavePoint, (int)1);
   m_dwNumHairs = (DWORD) MMLValueGetInt (pNode, gpszNumHairs, (int)1);
   m_acHairColor[0] = (COLORREF) MMLValueGetInt (pNode, gpszHairColor0, (int)0);
   m_acHairColor[1] = (COLORREF) MMLValueGetInt (pNode, gpszHairColor1, (int)0);
   m_cHairOdd = (COLORREF) MMLValueGetInt (pNode, gpszHairOdd, (int)0);
   m_cBackground = (COLORREF) MMLValueGetInt (pNode, gpszBackground, (int)0);
   m_fHairOddProb = MMLValueGetDouble (pNode, gpszHairOddProb, 0);
   m_fHairWaviness = MMLValueGetDouble (pNode, gpszHairWaviness, 0);
   m_fFurAngleLR = MMLValueGetDouble (pNode, gpszFurAngleLR, 0);
   m_afHairThick[0] = MMLValueGetDouble (pNode, gpszHairThick0, 0);
   m_afHairThick[1] = MMLValueGetDouble (pNode, gpszHairThick1, 0);
   m_fBlendBackground = MMLValueGetDouble (pNode, gpszBlendBackground, 0);
   MMLValueGetTEXTUREPOINT (pNode, gpszFurLength, &m_tpFurLength);
   MMLValueGetTEXTUREPOINT (pNode, gpszFurAngleUD, &m_tpFurAngleUD);

   return TRUE;
}

BOOL CTextCreatorHair::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwScale = max(dwX, dwY);


   // create the surface
   pImage->Init (dwX, dwY, m_cBackground, 0);

   DWORD i, j;
   WORD awColorRange[2][3];
   GammaInit();
   Gamma (m_acHairColor[0], awColorRange[0]);
   Gamma (m_acHairColor[1], awColorRange[1]);
   if (m_fFur) {
      fp fArea = (fp)dwX * (fp)dwY;
      fArea /= (fp)m_dwNumHairs;
      fArea = sqrt(fArea); // results in number of pixels per hair...

      fp fx, fy;
      CPoint ap[2];
      // loop over all hairs
      for (fx = 0; fx < (fp)dwX; fx += fArea) for (fy = 0; fy < (fp)dwY; fy += fArea) {
         // root position
         ap[0].p[0] = fx + randf(-fArea/2, fArea/2);
         ap[0].p[1] = fy + randf(-fArea/2, fArea/2);
         ap[0].p[2] = 0;

         // hair length and angle
         fp fLen = randf(m_tpFurLength.h, m_tpFurLength.v) / fPixelLen;
         fp fAngleLR = randf(-1, 1);
         if (fAngleLR >= 0)  // encourage to be more straight
            fAngleLR = fAngleLR * fAngleLR;
         else
            fAngleLR = -(fAngleLR * fAngleLR);
         fAngleLR *= m_fFurAngleLR/2;
         fp fAngleUD = randf(m_tpFurAngleUD.h, m_tpFurAngleUD.v);
         fAngleUD = max(fAngleUD, 0);
         fAngleUD = min(fAngleUD, PI);

         // figure out direction of hair...
         ap[1].p[0] = sin(fAngleLR) * cos(fAngleUD) * fLen;
         ap[1].p[1] = cos(fAngleLR) * cos(fAngleUD) * fLen;
         ap[1].p[2] = sin(fAngleUD) * fLen;
         ap[1].Add (&ap[0]);

         // figure out the color and thickness
         WORD awColor[3];
         if (randf(0,1) < m_fHairOddProb) 
            Gamma (m_cHairOdd, awColor);
         else {
            fp fAlpha = randf(0,1);
            for (j = 0; j < 3; j++)
               awColor[j] = (WORD)(fAlpha * (fp)awColorRange[0][j] + (1.0-fAlpha)*(fp)awColorRange[1][j]);
         }
         fp fThick = randf(m_afHairThick[0], m_afHairThick[1]);

         // potentially swap
         if (ap[1].p[1] < ap[0].p[1]) {
            CPoint pTemp;
            pTemp.Copy (&ap[0]);
            ap[0].Copy (&ap[1]);
            ap[1].Copy (&pTemp);
         }

         // draw the hair
         DrawHair (pImage, awColor, 2, ap, fThick / fPixelLen, FALSE, FALSE, NULL);
      } // fx, fy
   } // draw fur
   else {
      // hairs

      // allocate enough memory to store points
      CMem mem;
      m_dwHairWavePoint = max(m_dwHairWavePoint, 1);
      DWORD dwTotalPoint = m_dwHairWavePoint + 1;
      if (!mem.Required (sizeof(CPoint) * dwTotalPoint))
         return FALSE;
      PCPoint papPoint = (PCPoint) mem.p;

      DWORD dwLeft = m_dwNumHairs;

      while (dwLeft) {
         // do all the hairs in a row
         DWORD dwRow = min(m_dwHairPerLayer, dwLeft);
         fp fCombOffset = randf(0, (fp)pImage->Width()); // just random offset

         for (i = 0; i < dwRow; i++) {
            // come up with the locations...
            for (j = 0; j < dwTotalPoint; j++) {
               papPoint[j].p[0] = (fp)i / (fp)dwRow * (fp)pImage->Width();
               papPoint[j].p[1] = (fp)j / (fp)dwTotalPoint * (fp)pImage->Height();

               // add some noise
               papPoint[j].p[0] += randf(-m_fHairWaviness/2.0, m_fHairWaviness/2.0) / fPixelLen;

               // add comb offset
               papPoint[j].p[0] += fCombOffset;
            } // j

            // figure out the color and thickness
            WORD awColor[3];
            if (randf(0,1) < m_fHairOddProb) 
               Gamma (m_cHairOdd, awColor);
            else {
               fp fAlpha = randf(0,1);
               for (j = 0; j < 3; j++)
                  awColor[j] = (WORD)(fAlpha * (fp)awColorRange[0][j] + (1.0-fAlpha)*(fp)awColorRange[1][j]);
            }
            fp fThick = randf(m_afHairThick[0], m_afHairThick[1]);

            // draw the hair
            DrawHair (pImage, awColor, dwTotalPoint, papPoint,
               fThick / fPixelLen, TRUE, TRUE, NULL);
         } // i


         // continue
         dwLeft -= dwRow;
      } // while dwLeft
   } // draw hairs

   // do blend background
   fp fMax, fMin;
   DWORD dwNum = pImage->Width() * pImage->Height();
   PIMAGEPIXEL pip = pImage->Pixel(0, 0);
   for (i = 0; i < dwNum; i++, pip++) {
      if (!i) {
         fMin = fMax = pip->fZ;
      }
      else {
         fMin = min(fMin, pip->fZ);
         fMax = max(fMax, pip->fZ);
      }
   }
   fMax = max(fMax, fMin+CLOSE); // so no divide by zero
   fMax -= fMin;
   pip = pImage->Pixel(0, 0);
   WORD awBack[3];
   Gamma (m_cBackground, awBack);
   for (i = 0; i < dwNum; i++, pip++) {
      fp fAlpha = (pip->fZ - fMin) / fMax;   // 0..1 over height
      fAlpha = m_fBlendBackground * fAlpha + (1.0 - m_fBlendBackground) * 1.0;

      pip->wRed = (WORD)((fp)pip->wRed * fAlpha + (fp)awBack[0] * (1.0 - fAlpha));
      pip->wGreen = (WORD)((fp)pip->wGreen * fAlpha + (fp)awBack[1] * (1.0 - fAlpha));
      pip->wBlue = (WORD)((fp)pip->wBlue * fAlpha + (fp)awBack[2] * (1.0 - fAlpha));
   }

   // do transparency
   if (m_fTransparent) {
      pip = pImage->Pixel(0,0);
      for (i = 0; i < dwNum; i++, pip++) {
         pip->dwID = m_Material.m_wSpecExponent;

         if (pip->fZ == 0) {
            pip->dwIDPart |= 0xff;      // transparent
            // dont bother setting reflection since will want 0
         }
         else {
            pip->dwIDPart = (pip->dwIDPart & ~(0xff)) | 0;   // not transparent
            pip->dwID |= ((DWORD) m_Material.m_wSpecReflect << 16);
         }
      }
   }

   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo, m_fFlatten);

   return TRUE;
}

/******************************************************************************
DrawHair - This draws an individual hair.

inputs
   PCImage     pImage - Image to draw to
   WORD        *pawColor - Pointer to an array of 3 colors
   DWORD       dwNum - Number of points on patpPoint
   PCPoint     papPoint - Pointer to an array of CPoint. p[0] is the X
               value (in pixels), p[1] is the Y value in pixels. .y must be continually
               increasing. Both H and V are modulo the width/height of the image
               p[2] is the z value. (may be changed if fAutoDepth is used)
   fp          fWidth - Width of the hair in pixels
   BOOL        fAutoDepth - if checked then depth is automatically calculated assuming
               layering of hairs.
   BOOL        fHair - if checked then the hair is drawn modulo with a line
               bentween papPoint[dwNum-1] and papPoint[0]
   PCMaterial  pMaterial - If not NULL then used for the hair material
returns
   none
*/
void DrawHair (PCImage pImage, WORD *pawColor, DWORD dwNum,
                                 PCPoint papPoint, fp fWidth, BOOL fAutoDepth,
                                 BOOL fHair, PCMaterial pMaterial)
{
   // step one, figure out for each row (Y) where the hair is.
   CMem mem;
   DWORD dwWidth = pImage->Width();
   DWORD dwHeight = pImage->Height();
   DWORD i;
   if (!mem.Required (dwHeight * sizeof(fp) * 3))
      return;
   fp *pafLoc = (fp*)mem.p;
   fp *pafLocZ = pafLoc + dwHeight;
   fp *pafWidth = pafLocZ + dwHeight;
   for (i = 0; i < dwHeight; i++) {
      pafLoc[i] = pafLocZ[i] = -2000000;   // so know there's nothing
      pafWidth[i] = 0;
   }

   // if this is autodepth then set all the depths to a very small number
   if (fAutoDepth) for (i = 0; i < dwNum; i++)
      papPoint[i].p[2] = -1000;

#define HAIRTIP      .7
   // loop to figure out where hair goes, first the x location and then the depth
   DWORD dwLoop;
   int y;
   for (dwLoop = 0; dwLoop < 2; dwLoop++) {
      for (i = 0; i < (dwNum - (fHair ? 0 : 1)); i++) {
         DWORD adw[4];
         adw[1] = i;
         adw[2] = (i+1)%dwNum;
         if (fHair) {
            // hair loops around
            adw[0] = (adw[1] + dwNum - 1) % dwNum;
            adw[3] = (adw[2] + 1) % dwNum;
         }
         else {
            // fur hair has start and stop
            adw[0] = (adw[1] ? (adw[1]-1) : 0);
            adw[3] = min(adw[2]+1, dwNum-1);
         }

         // figure out starting/stopping y
         fp fStartY = ceil(papPoint[adw[1]].p[1]);
         fp fStopYOrig = papPoint[adw[2]].p[1];
         if (adw[2] < adw[1])
            fStopYOrig += (fp)dwHeight; // so have modulo
         fp fStopY = ceil(fStopYOrig);
         int iStartY = (int)fStartY;
         int iStopY = (int)fStopY;

         // loop over the Ys
         fp fMaxHeight = -1000000;
         for (y = iStartY; y < iStopY; y++) {
            int iRealY = y;
            while (iRealY < 0)   // because modulo busted for negative numbers
               iRealY += (int)dwHeight;
            iRealY = iRealY % (int)dwHeight;

            if (dwLoop == 1) {
               // second pass, looking to fill in Z
               pafLocZ[iRealY] = HermiteCubic (
                  ((fp)y - papPoint[adw[1]].p[1]) / (fStopYOrig - papPoint[adw[1]].p[1]),
                  papPoint[adw[0]].p[2], papPoint[adw[1]].p[2], papPoint[adw[2]].p[2], papPoint[adw[3]].p[2]);
               continue;
            }

            // else, loop == 0, so looking to the X location
            pafLoc[iRealY] = HermiteCubic (
               ((fp)y - papPoint[adw[1]].p[1]) / (fStopYOrig - papPoint[adw[1]].p[1]),
               papPoint[adw[0]].p[0], papPoint[adw[1]].p[0], papPoint[adw[2]].p[0], papPoint[adw[3]].p[0]);

            // a little bit of a hack. if it's fur, put a bit of a tip on the end...
            if (!fHair) {
               fp fAlpha = ((fp)y - papPoint[0].p[1]) / (papPoint[1].p[1] - papPoint[0].p[1]);
               if (papPoint[0].p[2] > papPoint[1].p[2])
                  fAlpha = 1.0 - fAlpha;

               if (fAlpha < HAIRTIP)
                  fAlpha = 0;
               else
                  fAlpha = (fAlpha - HAIRTIP) / (1.0 - HAIRTIP);

               pafWidth[iRealY] = fWidth * (1.0 - fAlpha);
               pafWidth[iRealY] = max(pafWidth[iRealY], 1);
            }
            else
               pafWidth[iRealY] = fWidth;

            // consider calculating the maximum depth
            if (!fAutoDepth)
               continue;
            int iXMin, iXMax;
            iXMin = (int)(pafLoc[iRealY] - fWidth/2);
            iXMax = (int)(pafLoc[iRealY] + fWidth/2);
            iXMax = max(iXMax, iXMin+1);
            int x;
            for (x = iXMin; x < iXMax; x++) {
               int iRealX = x;
               while (iRealX < 0)   // because modulo busted for negative numbers
                  iRealX += (int)dwWidth;
               iRealX = iRealX % (int)dwWidth;

               PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iRealX,(DWORD)iRealY);
               fMaxHeight = max(fMaxHeight, pip->fZ);
            } // x
         } // y
         if (fAutoDepth) {
            // set maxmimums
            papPoint[adw[1]].p[2] = max(papPoint[adw[1]].p[2], fMaxHeight + fWidth/2);
            papPoint[adw[2]].p[2] = max(papPoint[adw[2]].p[2], fMaxHeight + fWidth/2);
         }
      } // i
   } // dwLoop

   // draw the hair...
   fp fWidthThis;
   for (i = 0; i < dwHeight; i++) {
      if ((pafLoc[i] < -1000000) || (pafLocZ[i] < -1000000))
         continue;   // no hair actually drawn

      fWidthThis = pafWidth[i];

      int x;
      for (x = ceil(pafLoc[i] - fWidthThis/2); x <= (int)(pafLoc[i] + fWidthThis/2); x++) {
         fp fz = (fp)x - pafLoc[i];
         fz = fWidthThis * fWidthThis / 4.0 - fz * fz;
         if (fz <= 0)
            continue;   // out of bounds
         fz = sqrt(fz);
         fz += pafLocZ[i];

         // get the pixel
         PIMAGEPIXEL pip = pImage->Pixel ((DWORD)myfmod(x, (fp)dwWidth), i);

         // if less than what's there don't draw...
         if (fz <= pip->fZ)
            continue;

         // else, draw
         pip->wRed = pawColor[0];
         pip->wGreen = pawColor[1];
         pip->wBlue = pawColor[2];
         pip->fZ = fz;

         if (pMaterial)
            pip->dwID = MAKELONG (pMaterial->m_wSpecExponent, pMaterial->m_wSpecReflect);
      } // x
   } // y

   // done
}







/******************************************************************************
CreateTextureCreator - Based on the texture creator's GUID, and maybe MML or ID,
create it.

inputs
   GUID        *pgMajor - Major ID. GTEXTURECODE_XXX
   DWORD       dwType - Type parameter
   PCMMLNode2   pNode - Node containing the information to load, or NULL if just default based on paramter
   BOOL        fCanCreateVol - If TRUE then can create volumetric textures, if FALSE fail if
               pass in volumetric texture
returns
   PCTextCreatorSocket - Texture creator socket. The caller must call ->Delete().
            DO NOT CALL "delete xyz;"
*/
PCTextCreatorSocket CreateTextureCreator (DWORD dwRenderShard, const GUID *pgMajor, DWORD dwType, PCMMLNode2 pNode,
                                          BOOL fCanCreateVol)
{
   PCTextCreatorSocket pNew = NULL;

   // try to create volumetic one
   if (fCanCreateVol) {
      pNew = TextureCreateVol (pgMajor);
      if (pNew)
         goto created;
   }

   if (IsEqualGUID(*pgMajor, GTEXTURECODE_Tiles))
      pNew = new CTextCreatorTiles (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Parquet))
      pNew = new CTextCreatorParquet (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_StonesStacked))
      pNew = new CTextCreatorStonesStacked (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_StonesRandom))
      pNew = new CTextCreatorStonesRandom (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Pavers))
      pNew = new CTextCreatorPavers (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_TextureAlgoNoise))
      pNew = new CTextCreatorNoise (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Marble))
      pNew = new CTextCreatorMarble (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Fabric))
      pNew = new CTextCreatorFabric (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_GrassTussock))
      pNew = new CTextCreatorGrass (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Faceomatic))
      pNew = new CTextCreatorFaceomatic (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Mix))
      pNew = new CTextCreatorMix (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_LeafLitter))
      pNew = new CTextCreatorLeafLitter (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Text))
      pNew = new CTextCreatorText (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_TreeBark))
      pNew = new CTextCreatorTreeBark (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Branch))
      pNew = new CTextCreatorBranch (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Iris))
      pNew = new CTextCreatorIris (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_BloodVessels))
      pNew = new CTextCreatorBloodVessels (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Hair))
      pNew = new CTextCreatorHair (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Corrogated))
      pNew = new CTextCreatorCorrogated (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_WoodPlanks))
      pNew = new CTextCreatorWoodPlanks (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Shingles))
      pNew = new CTextCreatorShingles (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_BoardBatten))
      pNew = new CTextCreatorBoardBatten (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Clapboards))
      pNew = new CTextCreatorClapboards (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Lattice))
      pNew = new CTextCreatorLattice (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Wicker))
      pNew = new CTextCreatorWicker (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Chainmail))
      pNew = new CTextCreatorChainmail (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_TestPattern))
      pNew = new CTextCreatorTestPattern (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_Bitmap))
      pNew = new CTextCreatorBitmap (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_JPEG))
      pNew = new CTextCreatorJPEG (dwRenderShard, dwType);
   else if (IsEqualGUID(*pgMajor, GTEXTURECODE_ImageFile))
      pNew = new CTextCreatorImageFile (dwRenderShard, dwType);
   else
      return FALSE;
   if (!pNew)
      return FALSE;

created:
   if (pNode)
      pNew->MMLFrom (pNode);

   return pNew;
}


// BUGBUG - Logs texture seems to have wrong height

