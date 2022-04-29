/*********************************************************************************
CNPREffectAutoExposure.cpp - Code for effect

begun 13/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"




// EMTAUTOEXPOSURE- Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImage;
   PCFImage       pFImage;
   DWORD          dwCount;
   fp             *pafValues;
   PCRenderSuper  pRender;
} EMTAUTOEXPOSURE, *PEMTAUTOEXPOSURE;


// COLORPAGE - Page info
typedef struct {
   PCNPREffectAutoExposure pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} COLORPAGE, *PCOLORPAGE;



PWSTR gpszEffectAutoExposure = L"AutoExposure";



/*********************************************************************************
CNPREffectAutoExposure::Constructor and destructor
*/
CNPREffectAutoExposure::CNPREffectAutoExposure (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_tpExposureMinMax.h = 0;
   m_tpExposureMinMax.v = 7;  // moonlight

   m_tpExposureRealLight.h = 0;
   m_tpExposureRealLight.v = 11;  // moonlight

   m_fGrayAt = 5;
   m_fNoiseAt = 7;

}

CNPREffectAutoExposure::~CNPREffectAutoExposure (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectAutoExposure::Delete - From CNPREffect
*/
void CNPREffectAutoExposure::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectAutoExposure::QueryInfo - From CNPREffect
*/
void CNPREffectAutoExposure::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Automatically adjusts the exposure of an image for, as well as "
      L"simulating the optical effects of low light levels.";
   pqi->pszName = L"Autoexposure";
   pqi->pszID = gpszEffectAutoExposure;
}



static PWSTR gpszExposureMinMax = L"ExposureMinMax";
static PWSTR gpszExposureRealLight = L"ExposureRealLight";
static PWSTR gpszGrayAt = L"GrayAt";
static PWSTR gpszNoiseAt = L"NoiseAt";

/*********************************************************************************
CNPREffectAutoExposure::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectAutoExposure::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectAutoExposure);

   MMLValueSet (pNode, gpszExposureMinMax, &m_tpExposureMinMax);
   MMLValueSet (pNode, gpszExposureRealLight, &m_tpExposureRealLight);
   MMLValueSet (pNode, gpszGrayAt, m_fGrayAt);
   MMLValueSet (pNode, gpszNoiseAt, m_fNoiseAt);

   return pNode;
}


/*********************************************************************************
CNPREffectAutoExposure::MMLFrom - From CNPREffect
*/
BOOL CNPREffectAutoExposure::MMLFrom (PCMMLNode2 pNode)
{
   MMLValueGetTEXTUREPOINT (pNode, gpszExposureMinMax, &m_tpExposureMinMax);
   MMLValueGetTEXTUREPOINT (pNode, gpszExposureRealLight, &m_tpExposureRealLight);
   m_fGrayAt = MMLValueGetDouble (pNode, gpszGrayAt, 5);
   m_fNoiseAt = MMLValueGetDouble (pNode, gpszNoiseAt, 7);

   return TRUE;
}




/*********************************************************************************
CNPREffectAutoExposure::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectAutoExposure::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectAutoExposure::MMLFrom - From CNPREffect
*/
CNPREffectAutoExposure * CNPREffectAutoExposure::CloneEffect (void)
{
   PCNPREffectAutoExposure pNew = new CNPREffectAutoExposure(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_tpExposureMinMax = m_tpExposureMinMax;
   pNew->m_tpExposureRealLight = m_tpExposureRealLight;
   pNew->m_fGrayAt = m_fGrayAt;
   pNew->m_fNoiseAt = m_fNoiseAt;

   return pNew;
}





/*********************************************************************************
CNPREffectAutoExposure::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectAutoExposure::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTAUTOEXPOSURE pep = (PEMTAUTOEXPOSURE)pParams;

   PCImage pImage = pep->pImage;
   PCFImage pFImage = pep->pFImage;
   DWORD dwCount = pep->dwCount;
   fp *pafValues = pep->pafValues;
   PCRenderSuper pRender = pep->pRender;


   srand (GetTickCount() + 1000 * dwThread);

   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   // remember intensity of 10% from the top, and plan for those to be washed out
   fp fMax = dwCount ? (pafValues[dwCount/10] / 3.0) : 0.0; // NOTE: is * 3.0
   fMax = max(fMax, CLOSE);


   // go through and find the average, but ignore the top 10%, since some might
   // be really really bright
   fp fAverage = 0;
   DWORD x;
   for (x = dwCount/10; x < dwCount; x++)
      fAverage += pafValues[x];
   if (dwCount)
      fAverage /= ((fp)(DWORD)(dwCount - dwCount/10) * 3.0);
   fAverage = max(fAverage, CLOSE);

#ifdef _DEBUG
   fp fMoon = CM_LUMENSSUN / CM_LUMENSMOON;  // just to test
#endif

   // adjust max and average by the exposure, so know the real brightness
   fp fExposure = pRender->ExposureGet() / CM_LUMENSSUN;
   fMax *= fExposure;
   fAverage *= fExposure;

   // do an invert so know how things should be scaled
   // BUGFIX - Scale by exp(0.5) so slightly brigher
   fp fInvMax = (fp)0x10000 * exp(0.5) / fMax; // so anything at 10% would get washed out to white
   fp fInvAverage = (fp)0x6000 * exp(0.5) / fAverage;   // so anything at average would get an average color

   // weight these two to come up with scaling
   // this scaling, if applied, would bring the image to optimum
   // fp fScale = (fInvMax*2 + fInvAverage*1) / 3;
   fp fScale = pow ((fp)(fInvMax * fInvMax * fInvAverage), (fp)(1.0 / 3.0));
      // BUGFIX - Weight max a bit more, was *1 each
      // BUGFIX - Changed from addition to power

   // conver this to log scale, which makes the "real light" setting.
   fp fRealLight = log(fScale);

   // map the real light to a scaled light
   fp fDeltaMinMax = m_tpExposureMinMax.v - m_tpExposureMinMax.h;
   fp fDeltaRealLight = m_tpExposureRealLight.v - m_tpExposureRealLight.h;
   fp fMinMax;
   if (fDeltaRealLight > CLOSE)
      fMinMax = (fRealLight - m_tpExposureRealLight.h) / fDeltaRealLight * fDeltaMinMax + m_tpExposureMinMax.h;
   else
      fMinMax = m_tpExposureMinMax.h;  // pick one

   // cap min and max
   fp fCap = fMinMax;
   fCap = max(fCap, m_tpExposureMinMax.h);
   fCap = min(fCap, m_tpExposureMinMax.v);

   // make new scale
   fScale = exp(fCap) * fExposure;

   // determine if will make black and white
   float fGrayReal, fStartGraying = 0, fAllGray = 0;
   if (fDeltaMinMax > CLOSE)
      fGrayReal = (m_fGrayAt - m_tpExposureMinMax.h) / fDeltaMinMax * fDeltaRealLight + m_tpExposureRealLight.h;
   else
      fGrayReal = m_tpExposureRealLight.v;  // pick one

   fStartGraying = (fp)0x10000 / exp(fGrayReal) * exp(fCap) * 3;   // multiply by 3 since sum of RGB
   fAllGray = fStartGraying / exp(m_tpExposureRealLight.v - fGrayReal);
   float fGrayScale = fStartGraying - fAllGray;
   if (fGrayScale > 0)
      fGrayScale = 1.0 / fGrayScale;
   else
      fGrayScale = 1;

   // determine the noise amount
   fp fNoiseReal;
   if (fDeltaMinMax > CLOSE)
      fNoiseReal = (m_fNoiseAt - m_tpExposureMinMax.h) / fDeltaMinMax * fDeltaRealLight + m_tpExposureRealLight.h;
   else
      fNoiseReal = m_tpExposureRealLight.v;  // pick one
   fp fNoiseAmt = (fp)0x8000 / exp(fNoiseReal) * exp(fCap);
   BOOL fUseNoise = (fNoiseAmt > 10);

#define NOISEDIM        128
   // generate noise
   fp afNoise[NOISEDIM][NOISEDIM];
   DWORD i, j;
   for (i = 0; i < NOISEDIM; i++) for (j = 0; j < NOISEDIM; j++)
      afNoise[i][j] = randf(-fNoiseAmt, fNoiseAmt);

   // loop through and scale everything
   // DWORD dwMax = dwWidth * dwHeight;
   float af[3];
   float *paf = &af[0];
   float f, fGray, fAlpha, fAlphaInv;
   if (pip)
      pip = pImage->Pixel (0, pep->dwStart); // += pep->dwStart;
   if (pfp)
      pfp = pFImage->Pixel (0, pep->dwStart); // += pep->dwStart;
   DWORD y;
   for (y = pep->dwStart; y < pep->dwEnd; y++) {
      DWORD dwYMod = y % NOISEDIM;

      for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         if (pip) {
            if (!pip->dwID && !pip->wRed && !pip->wGreen && !pip->wBlue)
               continue;   // all zeros, so wont do anothing

            af[0] = pip->wRed;
            af[1] = pip->wGreen;
            af[2] = pip->wBlue;
         }
         else {
            if (!pfp->dwID && !pfp->fRed && !pfp->fGreen && !pfp->fBlue)
               continue;   // all zeros so wont do anothing

            paf = &pfp->fRed;
         }

         for (j = 0; j < 3; j++)
            paf[j] *= fScale;

         // apply gray
         fGray = paf[0] + paf[1] + paf[2];
         if (fGray < fStartGraying) {
            if (fGray <= fAllGray) {
               fGray /= 3.0;
               for (j = 0; j < 3; j++)
                  paf[j] = fGray;
            }
            else {
               fAlpha = (fGray - fAllGray) * fGrayScale;
               fAlphaInv = 1.0 - fAlpha;
               fGray = fGray / 3.0 * fAlphaInv;
               for (j = 0; j < 3; j++)
                  paf[j] = fAlpha * paf[j] + fGray;
            }
         }

         // is use noise then apply
         if (fUseNoise) {
            fGray = afNoise[dwYMod][x % NOISEDIM];
            for (j = 0; j < 3; j++)
               paf[j] += fGray;
               // no point doing since max later: paf[j] = max(paf[j], 0);   // so no negative colors
         }


         // write it back
         if (pip)
            for (j = 0; j < 3; j++) {
               f = paf[j];
               f = max(f, 0);
               f = min(f, (float)0xffff);
               (&pip->wRed)[j] = (WORD)f;
            } // j
         else {
            pfp->fRed = max(pfp->fRed, 0);
            pfp->fGreen = max(pfp->fGreen, 0);
            pfp->fBlue = max(pfp->fBlue, 0);
         }
      } // i
   }  // y
}

/*********************************************************************************
CNPREffectAutoExposure::RenderAny - This renders both integer and FP image.

inputs
   PCRenderSuper  pRender - Rendering engine used
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/

static int _cdecl fpCompare (const void *elem1, const void *elem2)
{
   fp *pdw1, *pdw2;
   pdw1 = (fp*) elem1;
   pdw2 = (fp*) elem2;

   if (*pdw1 > *pdw2)
      return -1;
   else if (*pdw1 < *pdw2)
      return 1;
   else
      return 0;
}

BOOL CNPREffectAutoExposure::RenderAny (PCRenderSuper pRender, PCImage pImage, PCFImage pFImage)
{
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   BOOL f360 = pImage ? pImage->m_f360 : pFImage->m_f360;

   // get a scattering of pixels to determine the color range
   fp fPixels = sqrt((fp)dwWidth * (fp)dwHeight / 1000); // get 1000 pixels
   DWORD dwSkip = (DWORD)fPixels;
   dwSkip = max(dwSkip, 1);

   // how many pixels actually need
   DWORD dwWidthSkip = (dwWidth + dwSkip-1) / dwSkip;
   DWORD dwHeightSkip = (dwHeight + dwSkip-1) / dwSkip;
   DWORD dwTotal = dwWidthSkip * dwHeightSkip;
   CMem mem;
   if (!mem.Required (dwTotal * sizeof(fp)))
      return FALSE;  // error
   fp *pafValues = (fp*)mem.p;
   DWORD dwCount = 0;
   DWORD x, y;
   PIMAGEPIXEL pipTemp = pip;
   PFIMAGEPIXEL pfpTemp = pfp;
   for (y = 0; y < dwHeight; y += dwSkip) {
      DWORD dwYToUse = y;
      if (f360) {
         // if 360 image, weight more towards the center
         fp f = (fp)y / ((fp)dwHeight/2) - 1;
         f = (f >= 0) ? (f*f) : -(f*f);
         f = (f + 1) * ((fp)dwHeight/2);
         f = max(f, 0);
         dwYToUse = (DWORD)f;
         dwYToUse = min(dwYToUse, dwHeight-1);
      }

      if (pip)
         pipTemp = pip + dwWidth * dwYToUse;
      else
         pfpTemp = pfp + dwWidth * dwYToUse;

      for (x = 0; x < dwWidth; x += dwSkip, pip ? (pipTemp += dwSkip) : (PIMAGEPIXEL)(pfpTemp += dwSkip) ) {
         if (pipTemp) {
            if (!pipTemp->dwID)
               continue;   // no object

            pafValues[dwCount++] = (fp)pipTemp->wRed + (fp)pipTemp->wGreen + (fp)pipTemp->wBlue;
         }
         else {
            if (!pfpTemp->dwID)
               continue;   // no object

            pafValues[dwCount++] = pfpTemp->fRed + pfpTemp->fGreen + pfpTemp->fBlue;
         }
      } // x
   } // y

   qsort (pafValues, dwCount, sizeof(fp), fpCompare);

   DWORD dwMax = dwHeight;

   // BUFIX - Multithreaded
   EMTAUTOEXPOSURE em;
   memset (&em, 0, sizeof(em));
   em.dwCount = dwCount;
   em.pafValues = pafValues;
   em.pFImage = pFImage;
   em.pImage = pImage;
   em.pRender = pRender;
   ThreadLoop (0, dwMax, 1, &em, sizeof(em));


   return TRUE;
}


/*********************************************************************************
CNPREffectAutoExposure::Render - From CNPREffect
*/
BOOL CNPREffectAutoExposure::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pRender, pDest, NULL);
}



/*********************************************************************************
CNPREffectAutoExposure::Render - From CNPREffect
*/
BOOL CNPREffectAutoExposure::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pRender, NULL, pDest);
}




/****************************************************************************
EffectAutoExposurePage
*/
BOOL EffectAutoExposurePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCOLORPAGE pmp = (PCOLORPAGE)pPage->m_pUserData;
   PCNPREffectAutoExposure pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         DoubleToControl (pPage, L"exposureminmax0", pv->m_tpExposureMinMax.h);
         DoubleToControl (pPage, L"exposureminmax1", pv->m_tpExposureMinMax.v);
         DoubleToControl (pPage, L"exposurereallight0", pv->m_tpExposureRealLight.h);
         DoubleToControl (pPage, L"exposurereallight1", pv->m_tpExposureRealLight.v);
         DoubleToControl (pPage, L"grayat", pv->m_fGrayAt);
         DoubleToControl (pPage, L"noiseat", pv->m_fNoiseAt);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         pv->m_tpExposureMinMax.h = DoubleFromControl (pPage, L"exposureminmax0");
         pv->m_tpExposureMinMax.v = DoubleFromControl (pPage, L"exposureminmax1");
         pv->m_tpExposureRealLight.h = DoubleFromControl (pPage, L"exposurereallight0");
         pv->m_tpExposureRealLight.v = DoubleFromControl (pPage, L"exposurereallight1");
         pv->m_fGrayAt = DoubleFromControl (pPage, L"grayat");
         pv->m_fNoiseAt = DoubleFromControl (pPage, L"noiseat");

         pPage->Message (ESCM_USER+189);  // update bitmap
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         // see about all effects checked or unchecked
         if (!_wcsicmp(p->pControl->m_pszName, L"alleffects")) {
            pmp->fAllEffects = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }

      }
      break;   // default


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
            p->pszSubString = L"Autoexposure";
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
CNPREffectAutoExposure::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectAutoExposure::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectAutoExposure::Dialog - From CNPREffect
*/
BOOL CNPREffectAutoExposure::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   COLORPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTAUTOEXPOSURE, EffectAutoExposurePage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

