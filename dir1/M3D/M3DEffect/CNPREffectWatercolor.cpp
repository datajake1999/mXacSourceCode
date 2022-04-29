/*********************************************************************************
CNPREffectWatercolor.cpp - Code for effect

begun 29/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"





// WATERCOLORPAGE - Page info
typedef struct {
   PCNPREffectWatercolor pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} WATERCOLORPAGE, *PWATERCOLORPAGE;



PWSTR gpszEffectWatercolor = L"Watercolor";


#define NUMMASKS           16     // varieties of watercolor splatters

/*********************************************************************************
CNPREffectWatercolor::Constructor and destructor
*/
CNPREffectWatercolor::CNPREffectWatercolor (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fNoise = 0.5;

   m_fMaskSize = 10;        // as a percent of screen width
   m_fNoiseSize = 2;        // as a percent of screen width
   m_fStainedGlass = FALSE;  // if stained glass then new ID for each
   m_fColorError = 0.25;     // amount of color error acceptable, from 0.. 1
   m_fDarkenAmount = 0.5;  // amount to darken around edges
   m_fEdgeSize = 1;  // how much paint groups up against edge (in percent of screen width)
   m_fNoiseDarken = 0.2;  // amount that noise darkens

   m_pColorVar.Zero();
   m_pColorVar.p[0] = m_pColorVar.p[1] = m_pColorVar.p[2] = .1;
   m_pColorPalette.Zero();
   m_fColorHueShift = 0;
   m_fColorUseFixed = 0;
   m_acColorFixed[0] = RGB(0,0,0);
   m_acColorFixed[1] = RGB(0xff,0xff,0xff);
   m_acColorFixed[2] = RGB(0xff,0,0);
   m_acColorFixed[3] = RGB(0,0xff,0);
   m_acColorFixed[4] = RGB(0,0,0xff);
}

CNPREffectWatercolor::~CNPREffectWatercolor (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectWatercolor::Delete - From CNPREffect
*/
void CNPREffectWatercolor::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectWatercolor::QueryInfo - From CNPREffect
*/
void CNPREffectWatercolor::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Modifies an image so it looks like a watercolor painting.";
   pqi->pszName = L"Watercolor";
   pqi->pszID = gpszEffectWatercolor;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszNoise = L"Noise";
static PWSTR gpszColorVar = L"ColorVar";
static PWSTR gpszColorPalette = L"ColorPalette";
static PWSTR gpszColorHueShift = L"ColorHueShift";
static PWSTR gpszColorUseFixed = L"ColorUseFixed";
static PWSTR gpszMaskSize = L"MaskSize";
static PWSTR gpszNoiseSize = L"NoiseSize";
static PWSTR gpszColorError = L"ColorError";
static PWSTR gpszDarkenAmount = L"DarkenAmount";
static PWSTR gpszEdgeSize = L"EdgeSize";
static PWSTR gpszNoiseDarken = L"NoiseDarken";
static PWSTR gpszStainedGlass = L"StainedGlass";

/*********************************************************************************
CNPREffectWatercolor::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectWatercolor::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectWatercolor);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszNoise, m_fNoise);

   MMLValueSet (pNode, gpszMaskSize, m_fMaskSize);
   MMLValueSet (pNode, gpszNoiseSize, m_fNoiseSize);
   MMLValueSet (pNode, gpszColorError, m_fColorError);
   MMLValueSet (pNode, gpszDarkenAmount, m_fDarkenAmount);
   MMLValueSet (pNode, gpszEdgeSize, m_fEdgeSize);
   MMLValueSet (pNode, gpszNoiseDarken, m_fNoiseDarken);
   MMLValueSet (pNode, gpszStainedGlass, (int)m_fStainedGlass);


   MMLValueSet (pNode, gpszColorVar, &m_pColorVar);
   MMLValueSet (pNode, gpszColorPalette, &m_pColorPalette);
   MMLValueSet (pNode, gpszColorHueShift, m_fColorHueShift);
   MMLValueSet (pNode, gpszColorUseFixed, (int)m_fColorUseFixed);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"ColorFixed%d", (int) i);
      MMLValueSet (pNode, szTemp, (int)m_acColorFixed[i]);
   }

   return pNode;
}


/*********************************************************************************
CNPREffectWatercolor::MMLFrom - From CNPREffect
*/
BOOL CNPREffectWatercolor::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fNoise = MMLValueGetDouble (pNode, gpszNoise, 0);

   m_fMaskSize = MMLValueGetDouble (pNode, gpszMaskSize, 10);
   m_fNoiseSize = MMLValueGetDouble (pNode, gpszNoiseSize, 2);
   m_fColorError = MMLValueGetDouble (pNode, gpszColorError, .25);
   m_fDarkenAmount = MMLValueGetDouble (pNode, gpszDarkenAmount, .5);
   m_fEdgeSize = MMLValueGetDouble (pNode, gpszEdgeSize, 2);
   m_fNoiseDarken = MMLValueGetDouble (pNode, gpszNoiseDarken, .2);
   m_fStainedGlass = (BOOL) MMLValueGetInt (pNode, gpszStainedGlass, (int)0);

   MMLValueGetPoint (pNode, gpszColorVar, &m_pColorVar);
   MMLValueGetPoint (pNode, gpszColorPalette, &m_pColorPalette);
   m_fColorHueShift = MMLValueGetDouble (pNode, gpszColorHueShift, 0);
   m_fColorUseFixed = (BOOL) MMLValueGetInt (pNode, gpszColorUseFixed, (int)0);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < 5; i++) {
      swprintf (szTemp, L"ColorFixed%d", (int) i);
      m_acColorFixed[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)m_acColorFixed[i]);
   }

   return TRUE;
}




/*********************************************************************************
CNPREffectWatercolor::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectWatercolor::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectWatercolor::CloneEffect - From CNPREffect
*/
CNPREffectWatercolor * CNPREffectWatercolor::CloneEffect (void)
{
   PCNPREffectWatercolor pNew = new CNPREffectWatercolor(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_fNoise = m_fNoise;

   pNew->m_pColorVar.Copy (&m_pColorVar);
   pNew->m_pColorPalette.Copy (&m_pColorPalette);
   pNew->m_fColorHueShift = m_fColorHueShift;
   pNew->m_fColorUseFixed = m_fColorUseFixed;
   memcpy (&pNew->m_acColorFixed, &m_acColorFixed, sizeof(m_acColorFixed));

   pNew->m_fMaskSize = m_fMaskSize;
   pNew->m_fNoiseSize = m_fNoiseSize;
   pNew->m_fStainedGlass = m_fStainedGlass;
   pNew->m_fColorError = m_fColorError;
   pNew->m_fDarkenAmount = m_fDarkenAmount;
   pNew->m_fEdgeSize = m_fEdgeSize;
   pNew->m_fNoiseDarken = m_fNoiseDarken;
   return pNew;
}




/*********************************************************************************
CNPREffectWatercolor::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectWatercolor::RenderAny (PCImage pImage, PCFImage pFImage, PCProgressSocket pProgress)
{
   CNoise2D aNoise[NUMMASKS];

   srand (GetTickCount());  // just pick a seed

   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   BOOL f360 = pImage ? pImage->m_f360 : pFImage->m_f360;

   // allocate the masks
   DWORD dwSize = (DWORD)(m_fMaskSize * (fp)dwWidth / 100.0);
   DWORD i;
   dwSize = max(dwSize,3);
   dwSize = min(dwSize,200);  // dont make to large
   DWORD dwNoise = (DWORD)(m_fNoiseSize ? (m_fMaskSize / m_fNoiseSize) : 1);
   dwNoise = max(dwNoise, 2);
   if (!m_memMask.Required (NUMMASKS * dwSize * dwSize * sizeof(int)))
      return FALSE;
   for (i = 0; i < NUMMASKS; i++) {
      aNoise[i].Init (dwNoise, dwNoise);
      FillMask ((int*)m_memMask.p + i * dwSize * dwSize, dwSize, &aNoise[i]);
   }

   // allocate bytes so know what's covered
   CMem memCov;
   if (!memCov.Required (dwWidth * dwHeight))
      return FALSE;
   BYTE *pbCov = (PBYTE)memCov.p;
   memset (pbCov, 0, dwWidth * dwHeight);

   if (m_fIgnoreBackground) {
      // find all pixels with background ID
      DWORD dwNum = dwWidth * dwHeight;
      BYTE *pb = pbCov;
      for (i = 0; i < dwNum; i++, pb++, (pip ? (pip++) : (PIMAGEPIXEL)(pfp++))) {
         if (pip && !pip->dwID)
            *pb = 1;
         else if (pfp && !pfp->dwID)
            *pb = 1;
      }
   }

   // allocate bytes for coverage of area
   CMem memCoverage;
   if (!memCoverage.Required (dwSize * dwSize * sizeof(int)*2))
      return FALSE;
      // BUGFIX - Doubling memory allocated since if don't seem to get memory overflow
   int *paiCoverage = (int*)memCoverage.p;

   // loop
   DWORD dwStep, x, y;
   WORD awColor[3];
   DWORD dwCount = 0;
   CListFixed lOrder;
   lOrder.Init (sizeof(POINT));
   WORD wID = 0;
   for (dwStep = dwSize; dwStep; dwStep /= 2) {
      // do progress
      if (pProgress)
         pProgress->Update ((fp)dwCount / (fp)(dwWidth * dwHeight));

      // create a list of all points that examine
      lOrder.Clear();
      for (y = 0; y < dwHeight; y += dwStep) {
         for (x = 0; x < dwWidth; x += dwStep) {
            DWORD xx, yy;
            if (dwStep > 1) {
               xx = x + ((DWORD)rand()%dwStep);
               yy = y + ((DWORD)rand()%dwStep);

               if ((xx >= dwWidth) || (yy >= dwHeight))
                  continue;   // out of range
            }
            else {
               xx = x;
               yy = y;
            }

            // if this is already occupied then ignore
            if (pbCov[yy * dwWidth + xx])
               continue;

            // add it
            POINT p;
            p.x = (int)xx;
            p.y = (int)yy;
            lOrder.Add (&p);
         } // x
      } // y

      // randomize the oder
      POINT *pp = (POINT*) lOrder.Get(0);
      DWORD dwNum = lOrder.Num();
      if (!dwNum)
         break;   // no more
      for (i = 0; i < dwNum; i++) {
         DWORD dwSwap = ((DWORD)rand()) % dwNum;
#if 0  // just to test overwite case that was happeing
            if ((pp[i].x < 0) || (pp[i].x >= (int)dwWidth) || (pp[i].y < 0) || (pp[i].y >= (int)dwHeight))
               continue;
            if ((pp[dwSwap].x < 0) || (pp[dwSwap].x >= (int)dwWidth) || (pp[dwSwap].y < 0) || (pp[dwSwap].y >= (int)dwHeight))
               continue;
#endif
         POINT pt = pp[i];
         pp[i] = pp[dwSwap];
         pp[dwSwap] = pt;
      }

      // loop through all the points
      for (; dwNum; dwNum--, pp++) {
         DWORD xx,yy;
         xx = (DWORD)pp->x;
         yy = (DWORD)pp->y;

#if 0 // just to test overwrite case that was happening
         if ((xx >= dwWidth) || (yy >= dwHeight))
            continue;
#endif // _DEBUG

         // get the color
         DWORD dwID;
         if (pImage) {
            pip = pImage->Pixel(xx,yy);
            dwID = pip->dwID;
            memcpy (awColor, &(pip->wRed), sizeof(awColor));
         }
         else {
            pfp = pFImage->Pixel(xx, yy);
            dwID = pfp->dwID;
            for (i = 0; i < 3; i++) {
               float f = (&pfp->fRed)[i];
               f = max(f,0);
               f = min(f,(float)0xffff);
               awColor[i] = (WORD)f;
            } // i
         }

         // figurte out which mask
         DWORD dwMask = rand() % NUMMASKS;
         int *paiMask = (int*)m_memMask.p + (dwMask * dwSize * dwSize);

         // NOTE: Could eventually run the mask a few times so that
         // can get more average color for the area, but doesnt seem necessary

         MaskEval (paiMask, dwSize, dwWidth, dwHeight, f360, pImage, pFImage,
            xx, yy, awColor, pbCov, paiCoverage, TRUE);

         // affect the color
         Color (awColor);

         // if stained glass then change ID
         if (m_fStainedGlass) {
            dwID = ((DWORD)HIWORD(dwID) << 16) | wID;
            wID = (wID + 1) % 0xefff;
         }
         else
            dwID = 0;

         // write this new color to the screen
         dwCount += MaskPaint (dwSize, dwWidth, dwHeight, f360, pImage, pFImage,
            xx, yy, awColor, paiCoverage, &aNoise[rand() % NUMMASKS], dwID);

      } // i
   } // dwStep


   return TRUE;
}


/*********************************************************************************
CNPREffectWatercolor::Render - From CNPREffect
*/
BOOL CNPREffectWatercolor::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL, pProgress);
}



/*********************************************************************************
CNPREffectWatercolor::Render - From CNPREffect
*/
BOOL CNPREffectWatercolor::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest, pProgress);
}




/****************************************************************************
EffectWatercolorPage
*/
BOOL EffectWatercolorPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWATERCOLORPAGE pmp = (PWATERCOLORPAGE)pPage->m_pUserData;
   PCNPREffectWatercolor pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         if (pControl = pPage->ControlFind (L"stainedglass"))
            pControl->AttribSetBOOL (Checked(), pv->m_fStainedGlass);

         // set the checkbox
         if (pControl = pPage->ControlFind (L"ignorebackground"))
            pControl->AttribSetBOOL (Checked(), pv->m_fIgnoreBackground);
         if (pControl = pPage->ControlFind (L"colorusefixed"))
            pControl->AttribSetBOOL (Checked(), pv->m_fColorUseFixed);

         DoubleToControl (pPage, L"masksize", pv->m_fMaskSize);
         DoubleToControl (pPage, L"noisesize", pv->m_fNoiseSize);
         DoubleToControl (pPage, L"edgesize", pv->m_fEdgeSize);

         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"colorpalette%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pColorPalette.p[i]);
         } // i

         for (i = 0; i < 5; i++) {
            swprintf (szTemp, L"colorfixed%d", (int) i);
            FillStatusColor (pPage, szTemp, pv->m_acColorFixed[i]);
         } // i

         // scrollbars
         pControl = pPage->ControlFind (L"noise");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoise * 100));
         if (pControl = pPage->ControlFind (L"colorhueshift"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fColorHueShift * 100));
         if (pControl = pPage->ControlFind (L"ColorError"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fColorError * 100));
         if (pControl = pPage->ControlFind (L"DarkenAmount"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDarkenAmount * 100));
         if (pControl = pPage->ControlFind (L"NoiseDarken"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseDarken * 100));
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"colorvar%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetInt (Pos(), (int)(pv->m_pColorVar.p[i] * 100));
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszColorChangeFixed = L"changecolorfixed";
         DWORD dwColorChangeFixedLen = (DWORD)wcslen(pszColorChangeFixed);

         // see about all effects checked or unchecked
         if (!_wcsicmp(p->pControl->m_pszName, L"alleffects")) {
            pmp->fAllEffects = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }

         if (!_wcsicmp(p->pControl->m_pszName, L"ignorebackground")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fIgnoreBackground) {
               pv->m_fIgnoreBackground = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorusefixed")) {
            pv->m_fColorUseFixed = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"stainedglass")) {
            pv->m_fStainedGlass = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszColorChangeFixed, dwColorChangeFixedLen)) {
            DWORD dwNum = _wtoi(p->pControl->m_pszName + dwColorChangeFixedLen);
            WCHAR szTemp[64];
            swprintf (szTemp, L"colorfixed%d", (int) dwNum);
            pv->m_acColorFixed[dwNum] = AskColor (pPage->m_pWindow->m_hWnd,
               pv->m_acColorFixed[dwNum], pPage, szTemp);
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
      }
      break;   // default


   case ESCN_EDITCHANGE:
      {
         pv->m_fMaskSize = DoubleFromControl (pPage, L"masksize");
         pv->m_fNoiseSize = DoubleFromControl (pPage, L"noisesize");
         pv->m_fEdgeSize = DoubleFromControl (pPage, L"edgesize");

         // just get all values
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"colorpalette%d", (int)i);
            pv->m_pColorPalette.p[i] = DoubleFromControl (pPage, szTemp);
         } // i

         pPage->Message (ESCM_USER+189);  // update bitmap
      }
      break;

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         fp *pf = NULL;
         PWSTR pszColorVar = L"colorvar";
         DWORD dwColorVarLen = (DWORD)wcslen(pszColorVar);

         if (!_wcsicmp (psz, L"noise"))
            pf = &pv->m_fNoise;
         else if (!_wcsicmp (psz, L"colorhueshift"))
            pf = &pv->m_fColorHueShift;
         else if (!_wcsicmp (psz, L"ColorError"))
            pf = &pv->m_fColorError;
         else if (!_wcsicmp (psz, L"DarkenAmount"))
            pf = &pv->m_fDarkenAmount;
         else if (!_wcsicmp (psz, L"NoiseDarken"))
            pf = &pv->m_fNoiseDarken;

         else if (!wcsncmp(psz, pszColorVar, dwColorVarLen))
            pf = &pv->m_pColorVar.p[_wtoi(psz + dwColorVarLen)];
         if (!pf)
            break;   // not known

         *pf = (fp)p->iPos / 100.0;
         pPage->Message (ESCM_USER+189);  // update bitmap
         return TRUE;
      }
      break;

   case ESCM_USER+189:  // update image
      {
         if (pmp->hBit)
            DeleteObject (pmp->hBit);
         if (pmp->fAllEffects)
            pmp->hBit = EffectImageToBitmap (pmp->pTest, pmp->pAllEffects, NULL, pmp->pRender, pmp->pWorld);
         else
            pmp->hBit = EffectImageToBitmap (pmp->pTest, NULL, pmp->pe, pmp->pRender, pmp->pWorld);

         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64)pmp->hBit);
         PCEscControl pControl = pPage->ControlFind (L"image");
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Watercolor";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"HBITMAP")) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)pmp->hBit);
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*********************************************************************************
CNPREffectWatercolor::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectWatercolor::IsPainterly (void)
{
   return TRUE;
}

/*********************************************************************************
CNPREffectWatercolor::Dialog - From CNPREffect
*/
BOOL CNPREffectWatercolor::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   WATERCOLORPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pe = this;
   mp.pTest = pTest;
   mp.pRender = pRender;
   mp.pWorld = pWorld;
   mp.pAllEffects = pAllEffects;

   // delete existing
   if (mp.hBit)
      DeleteObject (mp.hBit);
   if (mp.fAllEffects)
      mp.hBit = EffectImageToBitmap (pTest, pAllEffects, NULL, pRender, pWorld);
   else
      mp.hBit = EffectImageToBitmap (pTest, NULL, this, pRender, pWorld);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTWATERCOLOR, EffectWatercolorPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}


/*********************************************************************************
CNPREffectWatercolor::FillMask - This fills a dwSize x dwSize mask with an
array of integers. If the color distance between the "average" and the pixel
are greater than this value then the point should be excluded.

inputs
   int            *paiBuf - dwSize x dwSize integers
   DWORD          dwSize - Number of pixels across
   PCNoise2D      pNoise - Noise to use;
returns
   none
*/
void CNPREffectWatercolor::FillMask (int *paiBuf, DWORD dwSize, PCNoise2D pNoise)
{
   if (!dwSize)
      return;


   m_fColorError = max(m_fColorError, 0.01);

   // loop
   DWORD x,y;
   for (y = 0; y < dwSize; y++) {
      fp fY = 1.0 - (fp)y / ((fp)dwSize/2);
      fY *= fY;

      for (x = 0; x < dwSize; x++, paiBuf++) {
         fp fX = 1.0 - (fp)x / ((fp)dwSize/2);
         fX *= fX;
         fp f = sqrt(fX + fY);
         f = cos ((fp)(f * PI/2 * 2.0 / sqrt((fp)2)));
            // BUGFIX - Multiple 2.0 / sqrt(2) so corners are at lowest possible values

         fp fNoise = (pNoise->Value ((fp)x / (fp)dwSize, (fp)y / (fp)dwSize) +
            pNoise->Value ((fp)x*2 / (fp)dwSize, (fp)y*2 / (fp)dwSize) * 0.5
            )* m_fNoise * 0.75;  // multiply by 0.5 so center is guaranteed to be max
         fNoise = max(fNoise, -0.95);
         f += fNoise;
         f *= m_fColorError * (fp)0xffff;
         if (f > 0)
            f = max(f, 1); // at least some
         *paiBuf = (int)f;
      } // x
   } // y
}


/*********************************************************************************
CNPREffectWatercolor::MaskEval - Given a max, and a pixel it's over, this
evaluates the pixel and determines what other pixels are part of it.

inputs
   int         *paiMask - From FillMask()
   DWORD       dwSize - Size of the mask
   DWORD       dwWidth - Width of the image
   DWORD       dwHeight - Height of the image
   BOOL        f360 - If 360 flag
   PCImage     pImage - Image to use
   PCFImage    pFImage - Image to use
   DWORD       dwX - X pixel
   DWORD       dwY - Y pixel
   WORD        *pawColor - Color that is the "average" of the pixels.
                  NOTE: This will be modified with the final color
   BYTE        *pabCovered - Byte values of dwWidth x dwHeight. Set to TRUE
                  if area already covered by mask.
   int         *paiCoverage - Will be filled with non-zero values for pixels
               that are to be painted on. dwSize x dwSize x sizeof(int).
               If fMaskOver the data will be filled with the number of pixels
               distance from an edge.
   BOOL        fMaskOver - If TRUE then when have figured out the area will
               modify pabCovered to include the used points
returns
   none
*/
void CNPREffectWatercolor::MaskEval (int *paiMask, DWORD dwSize, DWORD dwWidth,
                                     DWORD dwHeight, BOOL f360, PCImage pImage,
                                     PCFImage pFImage, DWORD dwX, DWORD dwY,
                                     WORD *pawColor, BYTE *pabCovered, int *paiCoverage,
                                     BOOL fMaskOver)
{
   memset (paiCoverage, 0, dwSize * dwSize * sizeof(int));

   BYTE abColor[3];
   DWORD i;
   for (i = 0; i < 3; i++)
      abColor[i] = UnGamma(pawColor[i]);

   // get the object ID
   DWORD dwID = pImage ? pImage->Pixel(dwX, dwY)->dwID : pFImage->Pixel(dwX,dwY)->dwID;

   // loop over mask
   DWORD x,y;
   int *paic = paiCoverage + dwSize; // so leave top line
   paiMask += dwSize;   // so leave top line
   // NOTE: Very specifically leaving the points around the edge blank
   for (y = 1; y+1 < dwSize; y++) {
      int iY = (int)y + (int)dwY - (int)dwSize/2;
      if ((iY < 0) || (iY >= (int)dwHeight)) {
         paiMask += dwSize;
         paic += dwSize;
         continue;   // out of range
      }

      for (x = 0; x < dwSize; x++, paiMask++, paic++) {
         if (!x || (x+1 == dwSize))
            continue;   // leave points around edge blank
         int iX = (int)x + (int)dwX - (int)dwSize/2;
         if ((iX < 0) || (iX >= (int)dwWidth)) {
            if (!f360)
               continue;

            while (iX < 0)
               iX += (int)dwWidth;
            while (iX >= (int)dwWidth)
               iX -= (int)dwWidth;
         }

         // if pixel already used then skip
         if (pabCovered[iY * (int)dwWidth + iX])
            continue;

         // if the color is out of range then problem
         if (pImage) {
            PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iX, (DWORD)iY);
            if (pip->dwID != dwID)
               continue;   // not the same object as the center
            int iDist = 0;
            for (i = 0; i < 3; i++)
               iDist += abs((int)UnGamma((&pip->wRed)[i]) - (int)abColor[i]);
            if (iDist*256 >= paiMask[0])
               continue;   // out of range
         }
         else {   // float
            PFIMAGEPIXEL pip = pFImage->Pixel ((DWORD)iX, (DWORD)iY);
            if (pip->dwID != dwID)
               continue;   // not the same object as the center
            int iDist = 0;
            for (i = 0; i < 3; i++) {
               float f = min(0xffff,(&pip->fRed)[i]);
               f = max(f, 0);
               iDist += abs((int)UnGamma((WORD)f) - (int) abColor[i]);
            }
            if (iDist * 256 >= (float)paiMask[0])
               continue;   // out of range
         }

         // else, matches
         *paic = 0x7ffffff;
      } // x
   } // y

   // mark the center of the coverage as 1 to indicate the distance from the center
   RECT r;
   r.left = r.right = (int)dwSize/2;
   r.top = r.bottom = (int)dwSize/2;
   paiCoverage[r.top*(int)dwSize + r.left] = 1;

   // repeat, marking up distance between points
   int iDist;
   RECT rNew;
   for (iDist = 1; ; iDist++) {
      rNew.left = rNew.top = (int)dwSize;
      rNew.right = rNew.bottom = 0;

      for (y = (DWORD)r.top; y <= (DWORD)r.bottom; y++) {
         paic = paiCoverage + (y * dwSize + (DWORD)r.left);
         for (x = (DWORD)r.left; x <= (DWORD)r.right; x++, paic++) {
            if (*paic != iDist)
               continue;   // not interest in this

            // extend the bounds
            // look left
            if (x) {
               // up
               if (y && (paic[-(int)dwSize - 1] > iDist)) {
                  paic[-(int)dwSize - 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x-1);
                  rNew.right = max(rNew.right, (int)x-1);
                  rNew.top = min(rNew.top, (int)y-1);
                  rNew.bottom = max(rNew.bottom, (int)y-1);
               }

               // center
               if (paic[-1] > iDist) {
                  paic[-1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x-1);
                  rNew.right = max(rNew.right, (int)x-1);
                  rNew.top = min(rNew.top, (int)y);
                  rNew.bottom = max(rNew.bottom, (int)y);
               }


               // down
               if ((y+1 < dwSize) && (paic[(int)dwSize - 1] > iDist)) {
                  paic[(int)dwSize - 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x-1);
                  rNew.right = max(rNew.right, (int)x-1);
                  rNew.top = min(rNew.top, (int)y+1);
                  rNew.bottom = max(rNew.bottom, (int)y+1);
               }
            }

            // center
            // up
            if (y && (paic[-(int)dwSize] > iDist)) {
               paic[-(int)dwSize] = iDist+1;
               rNew.left = min(rNew.left, (int)x);
               rNew.right = max(rNew.right, (int)x);
               rNew.top = min(rNew.top, (int)y-1);
               rNew.bottom = max(rNew.bottom, (int)y-1);
            }
            // down
            if ((y+1 < dwSize) && (paic[(int)dwSize] > iDist)) {
               paic[(int)dwSize] = iDist+1;
               rNew.left = min(rNew.left, (int)x);
               rNew.right = max(rNew.right, (int)x);
               rNew.top = min(rNew.top, (int)y+1);
               rNew.bottom = max(rNew.bottom, (int)y+1);
            }

            // look right
            if (x+1 < dwSize) {
               // up
               if (y && (paic[-(int)dwSize + 1] > iDist)) {
                  paic[-(int)dwSize + 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x+1);
                  rNew.right = max(rNew.right, (int)x+1);
                  rNew.top = min(rNew.top, (int)y-1);
                  rNew.bottom = max(rNew.bottom, (int)y-1);
               }

               // center
               if (paic[1] > iDist) {
                  paic[1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x+1);
                  rNew.right = max(rNew.right, (int)x+1);
                  rNew.top = min(rNew.top, (int)y);
                  rNew.bottom = max(rNew.bottom, (int)y);
               }


               // down
               if ((y+1 < dwSize) && (paic[(int)dwSize + 1] > iDist)) {
                  paic[(int)dwSize + 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x+1);
                  rNew.right = max(rNew.right, (int)x+1);
                  rNew.top = min(rNew.top, (int)y+1);
                  rNew.bottom = max(rNew.bottom, (int)y+1);
               }
            }
         } // x
      } // y

      // if nothing to extend then done
      if (rNew.left > rNew.right)
         break;
      r = rNew;
   } // iDist

   // loop through... any points maked as 7fffffff are not in contact
   // with the central element and should be zeroed
   paic = paiCoverage;
   DWORD adwColor[3];
   DWORD dwCount = 0;
   float afColor[3];
   memset (adwColor, 0, sizeof(adwColor));
   memset (afColor, 0, sizeof(afColor));
   paic = paiCoverage;
   for (y = 0; y < dwSize; y++) {
      int iY = (int)y + (int)dwY - (int)dwSize/2;
      if ((iY < 0) || (iY >= (int)dwHeight)) {
         paic += dwSize;
         continue;   // out of range
      }

      for (x = 0; x < dwSize; x++, paic++) {
         if (!paic[0])
            continue;   // nothing here
         else if (paic[0] == 0x7fffffff) {
            *paic = 0;
            continue;
         }

         // else, it's a pixel to use

         // get the pixel in original image
         int iX = (int)x + (int)dwX - (int)dwSize/2;
         if ((iX < 0) || (iX >= (int)dwWidth)) {
            if (!f360)
               continue;

            while (iX < 0)
               iX += (int)dwWidth;
            while (iX >= (int)dwWidth)
               iX -= (int)dwWidth;
         }

         // potentially set flag so know that is used
         if (fMaskOver)
            pabCovered[iY * (int)dwWidth + iX] = 1;

         // sum incolors
         dwCount++;

         paic[0] = 0x7fffffff;   // so can figure out edges
         if (pImage) {
            PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iX, (DWORD)iY);
            for (i = 0; i < 3; i++)
               adwColor[i] += (DWORD)(&pip->wRed)[i];
         }
         else {   // float
            PFIMAGEPIXEL pip = pFImage->Pixel ((DWORD)iX, (DWORD)iY);
            for (i = 0; i < 3; i++)
               afColor[i] += (&pip->fRed)[i];
         }
      } // x
   } // y

   // fill in the average color
   if (dwCount) for (i = 0; i < 3; i++) {
      if (pImage)
         pawColor[i] = (WORD)(adwColor[i] / dwCount);
      else {
         float f = afColor[i] / (float)dwCount;
         f = max(0,f);
         f = min((float)0xffff,f);
         pawColor[i] = (WORD)f;
      }
   } // i

   // go back through an figure out distance from edges?
   if (!fMaskOver)
      return;  // done

   r.left = r.top = 0;
   r.right = r.bottom = (int)dwSize-1;
   for (iDist = 0; ; iDist++) {
      rNew.left = rNew.bottom = (int)dwSize;
      rNew.right = rNew.top = 0;

      for (y = (DWORD)r.top; y <= (DWORD)r.bottom; y++) {
         paic = paiCoverage + (y * dwSize + (DWORD)r.left);
         for (x = (DWORD)r.left; x <= (DWORD)r.right; x++, paic++) {
            // NOTE: Algorithm works because all around edge gauranteed to be blank
            if (*paic != iDist)
               continue;   // not interest in this

            // extend the bounds
            // look left
            if (x) {
               // up
               if (y && (paic[-(int)dwSize - 1] > iDist)) {
                  paic[-(int)dwSize - 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x-1);
                  rNew.right = max(rNew.right, (int)x-1);
                  rNew.top = min(rNew.top, (int)y-1);
                  rNew.bottom = max(rNew.bottom, (int)y-1);
               }

               // center
               if (paic[-1] > iDist) {
                  paic[-1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x-1);
                  rNew.right = max(rNew.right, (int)x-1);
                  rNew.top = min(rNew.top, (int)y);
                  rNew.bottom = max(rNew.bottom, (int)y);
               }


               // down
               if ((y+1 < dwSize) && (paic[(int)dwSize - 1] > iDist)) {
                  paic[(int)dwSize - 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x-1);
                  rNew.right = max(rNew.right, (int)x-1);
                  rNew.top = min(rNew.top, (int)y+1);
                  rNew.bottom = max(rNew.bottom, (int)y+1);
               }
            }

            // center
            // up
            if (y && (paic[-(int)dwSize] > iDist)) {
               paic[-(int)dwSize] = iDist+1;
               rNew.left = min(rNew.left, (int)x);
               rNew.right = max(rNew.right, (int)x);
               rNew.top = min(rNew.top, (int)y-1);
               rNew.bottom = max(rNew.bottom, (int)y-1);
            }
            // down
            if ((y+1 < dwSize) && (paic[(int)dwSize] > iDist)) {
               paic[(int)dwSize] = iDist+1;
               rNew.left = min(rNew.left, (int)x);
               rNew.right = max(rNew.right, (int)x);
               rNew.top = min(rNew.top, (int)y+1);
               rNew.bottom = max(rNew.bottom, (int)y+1);
            }

            // look right
            if (x+1 < dwSize) {
               // up
               if (y && (paic[-(int)dwSize + 1] > iDist)) {
                  paic[-(int)dwSize + 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x+1);
                  rNew.right = max(rNew.right, (int)x+1);
                  rNew.top = min(rNew.top, (int)y-1);
                  rNew.bottom = max(rNew.bottom, (int)y-1);
               }

               // center
               if (paic[1] > iDist) {
                  paic[1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x+1);
                  rNew.right = max(rNew.right, (int)x+1);
                  rNew.top = min(rNew.top, (int)y);
                  rNew.bottom = max(rNew.bottom, (int)y);
               }


               // down
               if ((y+1 < dwSize) && (paic[(int)dwSize + 1] > iDist)) {
                  paic[(int)dwSize + 1] = iDist+1;
                  rNew.left = min(rNew.left, (int)x+1);
                  rNew.right = max(rNew.right, (int)x+1);
                  rNew.top = min(rNew.top, (int)y+1);
                  rNew.bottom = max(rNew.bottom, (int)y+1);
               }
            }
         } // x
      } // y

      // if nothing to extend then done
      if (rNew.left > rNew.right)
         break;
      r = rNew;
   } // iDist

   // done
}



/*********************************************************************************
CNPREffectWatercolor::MaskPaint - Looks at paiCoverage and paints the image
based on this mask. Non-zero values are paintied.

inputs
   DWORD       dwSize - Size of the mask
   DWORD       dwWidth - Width of the image
   DWORD       dwHeight - Height of the image
   BOOL        f360 - If 360 flag
   PCImage     pImage - Image to use
   PCFImage    pFImage - Image to use
   DWORD       dwX - X pixel
   DWORD       dwY - Y pixel
   WORD        *pawColor - Color that is the "average" of the pixels.
   int         *paiCoverage - Filled with non-zero values for pixels
               that are to be painted on. dwSize x dwSize x sizeof(int).
   PCNoise2D   pNoise - Noise to use
   DWORD       dwID - If not 0 then the ID for the pixels is set to this
returns
   DWORD - Number of pixels filled
*/
DWORD CNPREffectWatercolor::MaskPaint (DWORD dwSize, DWORD dwWidth,
                                     DWORD dwHeight, BOOL f360, PCImage pImage,
                                     PCFImage pFImage, DWORD dwX, DWORD dwY,
                                     WORD *pawColor, int *paiCoverage,
                                     PCNoise2D pNoise, DWORD dwID)
{

   DWORD dwCount = 0;

   float afColor[3], afThis[3];
   DWORD i;
   for (i = 0; i < 3; i++)
      afColor[i] = pawColor[i];

   // find the highest value
   DWORD dwDarkenSize = 1;
   int *paic = paiCoverage;
   for (i = 0; i < dwSize * dwSize; i++, paic++)
      dwDarkenSize = max(dwDarkenSize, (DWORD)paic[0]);
   DWORD dwEdge = (DWORD)(m_fEdgeSize * (fp)dwWidth / 100.0);
   dwDarkenSize = min(dwDarkenSize, dwEdge);
   dwDarkenSize = max(dwDarkenSize, 1);

   // loop over mask
   DWORD x,y;
   paic = paiCoverage;
   for (y = 0; y < dwSize; y++) {
      int iY = (int)y + (int)dwY - (int)dwSize/2;
      if ((iY < 0) || (iY >= (int)dwHeight)) {
         paic += dwSize;
         continue;   // out of range
      }

      for (x = 0; x < dwSize; x++, paic++) {
         if (!paic[0])
            continue;   // no coverage

         int iX = (int)x + (int)dwX - (int)dwSize/2;
         if ((iX < 0) || (iX >= (int)dwWidth)) {
            if (!f360)
               continue;

            while (iX < 0)
               iX += (int)dwWidth;
            while (iX >= (int)dwWidth)
               iX -= (int)dwWidth;
         }

         // darken this
         float fDarkness;
         if (paic[0] < (int) dwDarkenSize) {
            fDarkness = (float) (dwDarkenSize - paic[0] - 1) / (float)dwDarkenSize *
               m_fDarkenAmount;
            fDarkness *= fDarkness;
         }
         else
            fDarkness = 0;

         fDarkness += m_fNoiseDarken * (
            pNoise->Value ((fp)x / (fp)dwSize, (fp)y / (fp)dwSize) +
            pNoise->Value ((fp)x*2 / (fp)dwSize, (fp)y*2 / (fp)dwSize) * 0.5
            );;

         if (fDarkness > 0)
            for (i = 0; i < 3; i++)
               afThis[i] = afColor[i] * (1.0 - fDarkness);
         else // fDarkness < 0
            for (i = 0; i < 3; i++)
               afThis[i] = afColor[i] / (1.0 - fDarkness);


         // draw pixel
         dwCount++;
         if (pImage) {
            PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iX, (DWORD)iY);
            if (dwID)
               pip->dwID = dwID;
            for (i = 0; i < 3; i++) {
               float f = afThis[i];
               f = max(0, f);
               f = min((float)0xffff, f);
               (&pip->wRed)[i] = (WORD)f;
            }
         }
         else {   // float
            PFIMAGEPIXEL pip = pFImage->Pixel ((DWORD)iX, (DWORD)iY);
            if (dwID)
               pip->dwID = dwID;
            for (i = 0; i < 3; i++)
               (&pip->fRed)[i] = afThis[i];
         }
      } // x
   } // y

   return dwCount;
}




/*********************************************************************************
CNPREffectWatercolor::Color - Modifies the stroke color based on settings.

inputs
   WORD        *pawColor - RGB color. Should be filled with initial value and
                     modified in place
*/
void CNPREffectWatercolor::Color (WORD *pawColor)
{

   // if using fixed colors then different code
   DWORD i;
   if (m_fColorUseFixed) {
      BYTE abColor[3];
      for (i = 0; i < 3; i++)
         abColor[i] = UnGamma(pawColor[i]);

      DWORD dwDist, dwBestDist = 0;
      DWORD dwBest = -1;
      for (i = 0; i < 5; i++) {
         BYTE r = GetRValue(m_acColorFixed[i]);
         BYTE g = GetGValue(m_acColorFixed[i]);
         BYTE b = GetBValue(m_acColorFixed[i]);

         dwDist = (r >= abColor[0]) ? (r - abColor[0]) : (abColor[0] - r);
         dwDist += (g >= abColor[1]) ? (g - abColor[1]) : (abColor[1] - g);
         dwDist += (b >= abColor[2]) ? (b - abColor[2]) : (abColor[2] - b);

         if ((dwBest == -1) || (dwDist < dwBestDist)) {
            dwBestDist = dwDist;
            dwBest = i;
         }
      } // i

      // save this out
      Gamma (m_acColorFixed[dwBest], pawColor);
      return;
   }

   DWORD adwSplit[3];
   float afScale[3];
   for (i = 0; i < 3; i++) {
      adwSplit[i] = (DWORD) floor(m_pColorPalette.p[i] + 0.5);
      adwSplit[i] = max(adwSplit[i], 1);

      if (i == 0)
         afScale[i] = (float)adwSplit[i] / 256.0;
      else if (adwSplit[i] >= 2)
         afScale[i] = (float)(adwSplit[i] - 1) / 256.0;
      else
         afScale[i] = 1;
   }

   // convert this to HLS
   float fH, fL, fS;
   ToHLS256 (UnGamma(pawColor[0]), UnGamma(pawColor[1]), UnGamma(pawColor[2]),
      &fH, &fL, &fS);

   // add variations
   if (!fS && !fH)
      fH = randf(0, 256);   // if no saturation, then random hue
   if (m_pColorVar.p[0])
      fH = myfmod(fH + randf(-128, 128)*m_pColorVar.p[0], 256);
   if (m_pColorVar.p[1]) {
      fL += randf(-128, 128) * m_pColorVar.p[1];
      fL = max(fL, 0);
      fL = min(fL, 255);
   }
   if (m_pColorVar.p[2]) {
      fS += randf(-128, 128) * m_pColorVar.p[2];
      fS = max(fS, 0);
      fS = min(fS, 255);
   }

   // apply quantization
   if (m_pColorPalette.p[0]) {
      fH = floor (fH * afScale[0] - m_fColorHueShift + 0.5);
      fH = (fH + m_fColorHueShift) / afScale[0];
      fH = myfmod (fH, 256);
   }

   // lightness
   if (m_pColorPalette.p[1]) {
      if (adwSplit[1] >= 2)
         fL = floor(fL * afScale[1] + 0.5) / afScale[1];
      else
         fL = 128;   // mid point
   }

   // saturations
   if (m_pColorPalette.p[2]) {
      if (adwSplit[2] >= 2)
         fS = floor(fS * afScale[2] + 0.5) / afScale[2];
      else
         fS = 256;   // full saturation
   }


   // convert back to HLS
   float afColor[3];
   FromHLS256 (fH, fL, fS, &afColor[0], &afColor[1], &afColor[2]);
   GammaFloat256 (afColor);
   for (i = 0; i < 3; i++) {
      afColor[i] = max(afColor[i], 0);
      afColor[i] = min(afColor[i], (float)0xffff);
      pawColor[i] = (WORD)afColor[i];
   }
}

