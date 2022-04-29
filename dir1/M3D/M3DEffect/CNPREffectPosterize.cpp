/*********************************************************************************
CNPREffectPosterize.cpp - Code for effect

begun 14/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



// EMTPOSTERIZE- Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImage;
   PCFImage       pFImage;
   float          afScale[3];
   DWORD          adwSplit[3];
} EMTPOSTERIZE, *PEMTPOSTERIZE;



// POSTERIZEPAGE - Page info
typedef struct {
   PCNPREffectPosterize pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} POSTERIZEPAGE, *PPOSTERIZEPAGE;



PWSTR gpszEffectPosterize = L"Posterize";



/*********************************************************************************
CNPREffectPosterize::Constructor and destructor
*/
CNPREffectPosterize::CNPREffectPosterize (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_pPalette.Zero();
   m_pPalette.p[0] = m_pPalette.p[1] = m_pPalette.p[2] = 3;
   m_fHueShift = 0;
}

CNPREffectPosterize::~CNPREffectPosterize (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectPosterize::Delete - From CNPREffect
*/
void CNPREffectPosterize::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectPosterize::QueryInfo - From CNPREffect
*/
void CNPREffectPosterize::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Reduces the number of colors used in an image so it looks "
      L"like a poster.";
   pqi->pszName = L"Posterize";
   pqi->pszID = gpszEffectPosterize;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszPalette = L"Palette";
static PWSTR gpszHueShift = L"HueShift";

/*********************************************************************************
CNPREffectPosterize::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectPosterize::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectPosterize);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszPalette, &m_pPalette);
   MMLValueSet (pNode, gpszHueShift, m_fHueShift);

   return pNode;
}


/*********************************************************************************
CNPREffectPosterize::MMLFrom - From CNPREffect
*/
BOOL CNPREffectPosterize::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   MMLValueGetPoint (pNode, gpszPalette, &m_pPalette);
   m_fHueShift = MMLValueGetDouble (pNode, gpszHueShift, 0);

   return TRUE;
}




/*********************************************************************************
CNPREffectPosterize::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectPosterize::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectPosterize::CloneEffect - From CNPREffect
*/
CNPREffectPosterize * CNPREffectPosterize::CloneEffect (void)
{
   PCNPREffectPosterize pNew = new CNPREffectPosterize(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_pPalette.Copy (&m_pPalette);
   pNew->m_fHueShift = m_fHueShift;

   return pNew;
}




/*********************************************************************************
CNPREffectPosterize::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectPosterize::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTPOSTERIZE pep = (PEMTPOSTERIZE)pParams;

   PCImage pImage = pep->pImage;
   PCFImage pFImage = pep->pFImage;

   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();
   float afScale[3];
   memcpy (afScale, pep->afScale, sizeof(afScale));
   DWORD adwSplit[3];
   memcpy (adwSplit, pep->adwSplit, sizeof(adwSplit));

   DWORD i, j;
   float af[3];
   float *paf = &af[0];
   float f;
   if (pip)
      pip += pep->dwStart;
   if (pfp)
      pfp += pep->dwStart;
   for (i = pep->dwStart; i < pep->dwEnd; i++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
      if (m_fIgnoreBackground) {
         WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
         if (!w)
            continue;
      }

      if (pip) {
         af[0] = UnGamma(pip->wRed);
         af[1] = UnGamma(pip->wGreen);
         af[2] = UnGamma(pip->wBlue);
      }
      else {
         paf = &pfp->fRed;
         UnGammaFloat256 (paf);  // gamma correct to 256
      }

      float fH, fL, fS;
      ToHLS256 (paf[0], paf[1], paf[2], &fH, &fL, &fS);

      fH = floor (fH * afScale[0] - m_fHueShift + 0.5);
      fH = (fH + m_fHueShift) / afScale[0];
      fH = myfmod (fH, 256);

      // lightness
      if (adwSplit[1] >= 2)
         fL = floor(fL * afScale[1] + 0.5) / afScale[1];
      else
         fL = 128;   // mid point

      // saturations
      if (adwSplit[2] >= 2)
         fS = floor(fS * afScale[2] + 0.5) / afScale[2];
      else
         fS = 256;   // full saturation

      FromHLS256 (fH, fL, fS, &paf[0], &paf[1], &paf[2]);

      // write it back
      if (pip) {
         for (j = 0; j < 3; j++) {
            f = paf[j];
            f = max(f, 0);
            f = min(f, (float)0xff);
            (&pip->wRed)[j] = Gamma ((BYTE)f);
         } // j
      }
      else
         GammaFloat256 (paf);
   } // i
}

/*********************************************************************************
CNPREffectPosterize::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectPosterize::RenderAny (PCImage pImage, PCFImage pFImage)
{
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   DWORD adwSplit[3];
   float afScale[3];
   DWORD i;
   for (i = 0; i < 3; i++) {
      adwSplit[i] = (DWORD) floor(m_pPalette.p[i] + 0.5);
      adwSplit[i] = max(adwSplit[i], 1);

      if (i == 0)
         afScale[i] = (float)adwSplit[i] / 256.0;
      else if (adwSplit[i] >= 2)
         afScale[i] = (float)(adwSplit[i] - 1) / 256.0;
      else
         afScale[i] = 1;
   }


   DWORD dwMax= dwWidth * dwHeight;

   // BUGFIX - Multithreded
   EMTPOSTERIZE em;
   memset (&em, 0, sizeof(em));
   memcpy (em.adwSplit, adwSplit, sizeof(adwSplit));
   memcpy (em.afScale, afScale, sizeof(afScale));
   em.pFImage = pFImage;
   em.pImage = pImage;
   ThreadLoop (0, dwWidth * dwHeight, 1, &em, sizeof(em));


   return TRUE;
}


/*********************************************************************************
CNPREffectPosterize::Render - From CNPREffect
*/
BOOL CNPREffectPosterize::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL);
}



/*********************************************************************************
CNPREffectPosterize::Render - From CNPREffect
*/
BOOL CNPREffectPosterize::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest);
}




/****************************************************************************
EffectPosterizePage
*/
BOOL EffectPosterizePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPOSTERIZEPAGE pmp = (PPOSTERIZEPAGE)pPage->m_pUserData;
   PCNPREffectPosterize pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         // set the checkbox
         if (pControl = pPage->ControlFind (L"ignorebackground"))
            pControl->AttribSetBOOL (Checked(), pv->m_fIgnoreBackground);

         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"palette%d", (int)i);
            pv->m_pPalette.p[i] = floor(pv->m_pPalette.p[i] + 0.5);
            DoubleToControl (pPage, szTemp, pv->m_pPalette.p[i]);
         }

         // scrollbars
         pControl = pPage->ControlFind (L"hueshift");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHueShift * 100));
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

         if (!_wcsicmp(p->pControl->m_pszName, L"ignorebackground")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fIgnoreBackground) {
               pv->m_fIgnoreBackground = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
            return TRUE;
         };
      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         // just get all values
         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"palette%d", (int)i);
            pv->m_pPalette.p[i] = DoubleFromControl (pPage, szTemp);
         }
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

         if (!_wcsicmp (psz, L"hueshift"))
            pf = &pv->m_fHueShift;
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
            p->pszSubString = L"Posterize";
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
CNPREffectPosterize::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectPosterize::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectPosterize::Dialog - From CNPREffect
*/
BOOL CNPREffectPosterize::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   POSTERIZEPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTPOSTERIZE, EffectPosterizePage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

