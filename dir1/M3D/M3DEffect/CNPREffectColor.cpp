/*********************************************************************************
CNPREffectColor.cpp - Code for effect

begun 13/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


// EMTCOLOR- Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImage;
   PCFImage       pFImage;
} EMTCOLOR, *PEMTCOLOR;




// COLORPAGE - Page info
typedef struct {
   PCNPREffectColor pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} COLORPAGE, *PCOLORPAGE;



PWSTR gpszEffectColor = L"Color";



/*********************************************************************************
CNPREffectColor::Constructor and destructor
*/
CNPREffectColor::CNPREffectColor (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fUseHLS = m_fUseCB = m_fUseRGB = FALSE;
   m_pHLS.Zero();
   m_pHLS.p[0] = 0;
   m_pHLS.p[1] = m_pHLS.p[2] = 1;
   m_pCB.Zero();
   m_pCB.p[0] = m_pCB.p[1] = 1;
   m_pRGB.Zero();
   m_pRGB.p[0] = m_pRGB.p[1] = m_pRGB.p[2] = 1;
   memset (&m_afRGBInvert, 0 ,sizeof(m_afRGBInvert));
}

CNPREffectColor::~CNPREffectColor (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectColor::Delete - From CNPREffect
*/
void CNPREffectColor::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectColor::QueryInfo - From CNPREffect
*/
void CNPREffectColor::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Lets you control the color of the image, including contrast, brightness, "
      L"hue, saturation, and lightness.";
   pqi->pszName = L"Color";
   pqi->pszID = gpszEffectColor;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszUseHLS = L"UseHLS";
static PWSTR gpszUseCB = L"UseCB";
static PWSTR gpszUseRGB = L"Use$GB";
static PWSTR gpszHLS = L"HLS";
static PWSTR gpszRGB = L"RGB";
static PWSTR gpszCB = L"CB";

/*********************************************************************************
CNPREffectColor::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectColor::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectColor);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszUseHLS, (int)m_fUseHLS);
   MMLValueSet (pNode, gpszUseCB, (int)m_fUseCB);
   MMLValueSet (pNode, gpszUseRGB, (int)m_fUseRGB);
   MMLValueSet (pNode, gpszHLS, &m_pHLS);
   MMLValueSet (pNode, gpszCB, &m_pCB);
   MMLValueSet (pNode, gpszRGB, &m_pRGB);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"rgbinvert%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_afRGBInvert[i]);
   }
   return pNode;
}


/*********************************************************************************
CNPREffectColor::MMLFrom - From CNPREffect
*/
BOOL CNPREffectColor::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fUseHLS = (BOOL) MMLValueGetInt (pNode, gpszUseHLS, (int)0);
   m_fUseCB = (BOOL) MMLValueGetInt (pNode, gpszUseCB, (int)0);
   m_fUseRGB = (BOOL) MMLValueGetInt (pNode, gpszUseRGB, (int)0);
   MMLValueGetPoint (pNode, gpszHLS, &m_pHLS);
   MMLValueGetPoint (pNode, gpszCB, &m_pCB);
   MMLValueGetPoint (pNode, gpszRGB, &m_pRGB);

   DWORD i;
   WCHAR szTemp[32];
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"rgbinvert%d", (int)i);
      m_afRGBInvert[i] = (BOOL) MMLValueGetInt (pNode, szTemp, (int)0);
   }

   return TRUE;
}




/*********************************************************************************
CNPREffectColor::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectColor::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectColor::MMLFrom - From CNPREffect
*/
CNPREffectColor * CNPREffectColor::CloneEffect (void)
{
   PCNPREffectColor pNew = new CNPREffectColor(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_fUseHLS = m_fUseHLS;
   pNew->m_fUseRGB = m_fUseRGB;
   pNew->m_fUseCB = m_fUseCB;
   pNew->m_pHLS.Copy (&m_pHLS);
   pNew->m_pRGB.Copy (&m_pRGB);
   pNew->m_pCB.Copy (&m_pCB);
   memcpy (&pNew->m_afRGBInvert, &m_afRGBInvert, sizeof(m_afRGBInvert));

   return pNew;
}




/*********************************************************************************
CNPREffectColor::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectColor::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTCOLOR pep = (PEMTCOLOR)pParams;

   PCImage pImage = pep->pImage;
   PCFImage pFImage = pep->pFImage;


   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   DWORD i, j;
   DWORD dwMax= dwWidth * dwHeight;
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
         // BGUFIX - Opt: if it's black then do nothing
         if (!pip->wRed && !pip->wGreen && !pip->wBlue)
            continue;

         af[0] = pip->wRed;
         af[1] = pip->wGreen;
         af[2] = pip->wBlue;

      }
      else {
         // BGUFIX - Opt: if it's black then do nothing
         if (!pfp->fRed && !pfp->fGreen && !pfp->fBlue)
            continue;

         paf = &pfp->fRed;
      }

      if (m_fUseHLS) {
         float fH, fL, fS;
         ToHLS256 (paf[0] / 256.0, paf[1] / 256.0, paf[2] / 256.0,
            &fH, &fL, &fS);

         if (m_pHLS.p[0])
            fH = myfmod(fH + m_pHLS.p[0] * 256.0, 256.0);

         fL *= m_pHLS.p[1];
         fS *= m_pHLS.p[2];

         FromHLS256 (fH, fL, fS, &paf[0], &paf[1], &paf[2]);
         paf[0] *= 256.0;
         paf[1] *= 256.0;
         paf[2] *= 256.0;
      } // hls

      if (m_fUseCB) for (j = 0; j < 3; j++) {
         paf[j] /= (float)0xffff;
         paf[j] = pow((fp)paf[j], (fp)m_pCB.p[0]);
         paf[j] *= m_pCB.p[1];
         paf[j] *= (float)0xffff;
      } // use CB

      if (m_fUseRGB) for (j = 0; j < 3; j++) {
         paf[j] *= m_pRGB.p[j];

         if (m_afRGBInvert[j]) {
            paf[j] = UnGammaFloat256 (paf[j]);
            paf[j] = 255.0 - paf[j];
            paf[j] = max(0, paf[j]);
            paf[j] = GammaFloat256 (paf[j]);
         }
      }

      // write it back
      if (pip) for (j = 0; j < 3; j++) {
         f = paf[j];
         f = max(f, 0);
         f = min(f, (float)0xffff);
         (&pip->wRed)[j] = (WORD)f;
      } // j
   } // i


}

/*********************************************************************************
CNPREffectColor::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectColor::RenderAny (PCImage pImage, PCFImage pFImage)
{
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   // BUGFIX - multithreaded
   EMTCOLOR em;
   memset (&em, 0, sizeof(em));
   em.pImage = pImage;
   em.pFImage = pFImage;
   ThreadLoop (0, dwWidth * dwHeight, 1, &em, sizeof(em));

   return TRUE;
}


/*********************************************************************************
CNPREffectColor::Render - From CNPREffect
*/
BOOL CNPREffectColor::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL);
}



/*********************************************************************************
CNPREffectColor::Render - From CNPREffect
*/
BOOL CNPREffectColor::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest);
}




/****************************************************************************
EffectColorPage
*/
BOOL EffectColorPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCOLORPAGE pmp = (PCOLORPAGE)pPage->m_pUserData;
   PCNPREffectColor pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         // set the checkbox
         if (pControl = pPage->ControlFind (L"hlson"))
            pControl->AttribSetBOOL (Checked(), pv->m_fUseHLS);
         if (pControl = pPage->ControlFind (L"cbon"))
            pControl->AttribSetBOOL (Checked(), pv->m_fUseCB);
         if (pControl = pPage->ControlFind (L"rgbon"))
            pControl->AttribSetBOOL (Checked(), pv->m_fUseRGB);
         if (pControl = pPage->ControlFind (L"ignorebackground"))
            pControl->AttribSetBOOL (Checked(), pv->m_fIgnoreBackground);

         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"rgbinvert%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetBOOL (Checked(), pv->m_afRGBInvert[i]);
         }

         // scrollbars
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"hls%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pHLS.p[i] * 100));

            swprintf (szTemp, L"rgb%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pRGB.p[i] * 100));

            if (i >= 3)
               continue;   // since only 2 entries in cb

            swprintf (szTemp, L"cb%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (Pos(), (int)(pv->m_pCB.p[i] * 100));
         } // i
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

         PWSTR pszRGBInvert = L"rgbinvert";
         DWORD dwRGBInvertLen = (DWORD)wcslen(pszRGBInvert);

         if (!wcsncmp(p->pControl->m_pszName, pszRGBInvert, dwRGBInvertLen)) {
            DWORD dwDim = _wtoi(p->pControl->m_pszName + dwRGBInvertLen);
            pv->m_afRGBInvert[dwDim] = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"hlson")) {
            pv->m_fUseHLS = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"cbon")) {
            pv->m_fUseCB = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"rgbon")) {
            pv->m_fUseRGB = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"ignorebackground")) {
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

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         fp *pf = NULL;
         PWSTR pszHLS = L"hls", pszRGB = L"rgb", pszCB = L"cb";
         DWORD dwHLSLen = (DWORD)wcslen(pszHLS), dwRGBLen = (DWORD)wcslen(pszRGB), dwCBLen = (DWORD)wcslen(pszCB);

         if (!wcsncmp (psz, pszHLS, dwHLSLen))
            pf = &pv->m_pHLS.p[_wtoi(psz + dwHLSLen)];
         else if (!wcsncmp (psz, pszRGB, dwRGBLen))
            pf = &pv->m_pRGB.p[_wtoi(psz + dwRGBLen)];
         else if (!wcsncmp (psz, pszCB, dwCBLen))
            pf = &pv->m_pCB.p[_wtoi(psz + dwCBLen)];
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
            p->pszSubString = L"Color";
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
CNPREffectColor::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectColor::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectColor::Dialog - From CNPREffect
*/
BOOL CNPREffectColor::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTCOLOR, EffectColorPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

