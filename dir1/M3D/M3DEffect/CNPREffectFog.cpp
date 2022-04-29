/*********************************************************************************
CNPREffectFog.cpp - Code for effect

begun 7/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



// EMTFOG - Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImageSrc;
   PCImage        pImageDest;
   PCFImage       pFImageSrc;
   PCFImage       pFImageDest;
   WORD           wSkydome;
   CPoint         pScale;
   CPoint         pColor;
   WORD           awFog[3];
} EMTFOG, *PEMTFOG;



// FOGPAGE - Page info
typedef struct {
   PCNPREffectFog pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} FOGPAGE, *PFOGPAGE;



PWSTR gpszEffectFog = L"Fog";


/*********************************************************************************
FindSkydome - Finds the skydome object in the world.

inputs
   PCWorldSocket     pWorld - World to look through
returns
   DWORD - Object number, or -1 if cant find
*/
DWORD FindSkydome (PCWorldSocket pWorld)
{
   if (!pWorld)
      return -1;

   DWORD i;
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      PCObjectSocket pos = pWorld->ObjectGet(i);

      if (pos->Message (OSM_SKYDOME, NULL))
         return i;
   } // i

   return -1;
}
/*********************************************************************************
GetSkydomeRGB - Determine the RGB for the skydome.

inputs
   PCWorldSocket        pWorld - World
   DWORD                dwSkydome - Skydome object
   PCRenderSuper        pRender - Used to get the exposure setting
   PCPoint              pColor - Filled with the color
   BOOL                 fMakeMono - If TRUE then make this monocrhomatic for fog
returns
   BOOL - TRUE if success, FALSE if couldnt find skydome
*/
BOOL GetSkydomeRGB (PCWorldSocket pWorld, DWORD dwSkydome, PCRenderSuper pRender,
                    PCPoint pColor, BOOL fMakeMono)
{
   if (!pWorld || (dwSkydome == -1))
      return FALSE;

   PCObjectSocket pos = pWorld->ObjectGet(dwSkydome);
   if (!pos)
      return FALSE;

   pColor->Zero();
   pos->AttribGet (L"Atmosphere: Ground, red", &pColor->p[0]);
   pos->AttribGet (L"Atmosphere: Ground, green", &pColor->p[1]);
   pos->AttribGet (L"Atmosphere: Ground, blue", &pColor->p[2]);

   DWORD i;
   for (i = 0; i < 3; i++) {
      pColor->p[i] *= 255.0;
      pColor->p[i] = max(pColor->p[i], 0);
      pColor->p[i] = min(pColor->p[i], 255);
      pColor->p[i] = Gamma((BYTE)pColor->p[i]);
   }

   if (pRender) {
      fp fExposure = pRender->ExposureGet();
      pColor->Scale (CM_LUMENSSUN / max(fExposure, CLOSE));
         // BUGFIX - Was CM_BESTEXPOSURE
   }

   if (fMakeMono) {
      fp fAverage = (pColor->p[0] + pColor->p[1] + pColor->p[2]) / 3.0;
      pColor->p[0] = pColor->p[1] = pColor->p[2] = fAverage;
   }

   return TRUE;
}


/*********************************************************************************
GetSkydomeRGB - Determine the RGB for the skydome.

inputs
   PCWorldSocket        pWorld - World
   DWORD                dwSkydome - Skydome object
   PCRenderSuper        pRender - Used to get the exposure setting
   WORD                 *pawRGB - Filled with the fog color
   BOOL                 fMakeMono - If TRUE then make this monocrhomatic for fog
returns
   BOOL - TRUE if success, FALSE if couldnt find skydome
*/
BOOL GetSkydomeRGB (PCWorldSocket pWorld, DWORD dwSkydome, PCRenderSuper pRender,
                    WORD *pawRGB, BOOL fMakeMono)
{
   CPoint pColor;
   if (!GetSkydomeRGB (pWorld, dwSkydome, pRender, &pColor, fMakeMono))
      return FALSE;

   DWORD i;
   for (i = 0; i < 3; i++) {
      pColor.p[i] = max(pColor.p[i], 0);
      pColor.p[i] = min(pColor.p[i], (fp)0xffff);
      pawRGB[i] = (WORD)pColor.p[i];
   } // i
   return TRUE;
}

/*********************************************************************************
CNPREffectFog::Constructor and destructor
*/
CNPREffectFog::CNPREffectFog (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_cFogColor = RGB(0xff,0xff,0xff);
   m_fFog = TRUE;
   m_pDist.Zero();
   m_pDist.p[0] = m_pDist.p[1] = m_pDist.p[2] = 10;
   m_fSkydomeColor = FALSE;
   m_fIgnoreSkydome = TRUE;
   m_fIgnoreBackground = TRUE;
}

CNPREffectFog::~CNPREffectFog (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectFog::Delete - From CNPREffect
*/
void CNPREffectFog::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectFog::QueryInfo - From CNPREffect
*/
void CNPREffectFog::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Adds fog or haze to the image.";
   pqi->pszName = L"Fog";
   pqi->pszID = gpszEffectFog;
}



static PWSTR gpszFogColor = L"FogColor";
static PWSTR gpszFog = L"Fog";
static PWSTR gpszDist = L"Dist";
static PWSTR gpszSkydomeColor = L"SkydomeColor";
static PWSTR gpszIgnoreSkydome = L"IgnoreSkydome";
static PWSTR gpszIgnoreBackground = L"IgnoreBackground";

/*********************************************************************************
CNPREffectFog::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectFog::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectFog);

   MMLValueSet (pNode, gpszFogColor, (int)m_cFogColor);
   MMLValueSet (pNode, gpszFog, (int) m_fFog);

   MMLValueSet (pNode, gpszDist, &m_pDist);
   MMLValueSet (pNode, gpszSkydomeColor, (int)m_fSkydomeColor);
   MMLValueSet (pNode, gpszIgnoreSkydome, (int)m_fIgnoreSkydome);
   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   return pNode;
}


/*********************************************************************************
CNPREffectFog::MMLFrom - From CNPREffect
*/
BOOL CNPREffectFog::MMLFrom (PCMMLNode2 pNode)
{
   m_cFogColor = (COLORREF) MMLValueGetInt (pNode, gpszFogColor, RGB(0xff,0xff,0xff));
   m_fFog = (BOOL)MMLValueGetInt (pNode, gpszFog, TRUE);

   MMLValueGetPoint (pNode, gpszDist, &m_pDist);
   m_fSkydomeColor = (BOOL) MMLValueGetInt (pNode, gpszSkydomeColor, (int)FALSE);
   m_fIgnoreSkydome = (BOOL) MMLValueGetInt (pNode, gpszIgnoreSkydome, (int)TRUE);
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   return TRUE;
}




/*********************************************************************************
CNPREffectFog::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectFog::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectFog::MMLFrom - From CNPREffect
*/
CNPREffectFog * CNPREffectFog::CloneEffect (void)
{
   PCNPREffectFog pNew = new CNPREffectFog(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_cFogColor = m_cFogColor;
   pNew->m_fFog =m_fFog;
   pNew->m_pDist.Copy (&m_pDist);
   pNew->m_fSkydomeColor = m_fSkydomeColor;
   pNew->m_fIgnoreSkydome = m_fIgnoreSkydome;
   pNew->m_fIgnoreBackground = m_fIgnoreBackground;

   return pNew;
}





/*********************************************************************************
CNPREffectFog::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectFog::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTFOG pep = (PEMTFOG)pParams;

   PCImage pImageSrc = pep->pImageSrc;
   PCImage pImageDest = pep->pImageDest;
   PCFImage pFImageSrc = pep->pFImageSrc;
   PCFImage pFImageDest = pep->pFImageDest;
   WORD wSkydome = pep->wSkydome;
   CPoint pScale, pColor;
   pScale.Copy (&pep->pScale);
   pColor.Copy (&pep->pColor);
   WORD awFog[3];
   memcpy (awFog, pep->awFog, sizeof(awFog));

   DWORD i;

   // BUGFIX - Optimize. If distance too close for fog to effect then ignore
#define NONOTICABLEFOGEFFECT        (0.99)
   fp afTooClose[3];
   fp fTooClose;
   BOOL fAllSame = TRUE;
   for (i = 0; i < 3; i++) {
      // NONOTICABLEFOGEFFECT = exp(fX * pScale.p[i])
      afTooClose[i] = log(NONOTICABLEFOGEFFECT) / (pScale.p[i] ? pScale.p[i] : CLOSE);
      if (i) {
         fTooClose = min(fTooClose, afTooClose[i]);
         if (pScale.p[i] != pScale.p[i-1])
            fAllSame = FALSE;
      }
      else
         fTooClose = afTooClose[i];
   } // i
   fTooClose = max(fTooClose, 0.0);

   if (pImageSrc || pImageDest) {
      // integer
      DWORD x, dwMax;
      PIMAGEPIXEL p;
      DWORD dwScaleFog, dwScalePixel;
      fp fDist;
      dwMax = pImageDest->Width() * pImageDest->Height();
      for (x = pep->dwStart, p = pImageDest->Pixel(0,0) + pep->dwStart; x < pep->dwEnd; x++, p++) {
         WORD wID = HIWORD(p->dwID);

         // cases to ignore
         if (!wID && m_fIgnoreBackground)
            continue;
         if ((wID == wSkydome) && wSkydome && m_fIgnoreSkydome)
            continue;
         if (p->fZ <= fTooClose)
            continue;

         WORD *paw = &p->wRed;
         if (fAllSame) {
            fDist = p->fZ * pScale.p[0];
            fDist = exp(fDist);

            dwScalePixel = (DWORD) (fDist * 0xffff);
            dwScaleFog = 0x10000 - dwScalePixel;
            for (i = 0; i < 3; i++)
               paw[i] =(WORD)((dwScaleFog * awFog[i] + dwScalePixel * paw[i]) >> 16);
         }
         else  // different
            for (i = 0; i < 3; i++) {
               if (p->fZ <= afTooClose[i])
                  continue;      // no change

               fDist = p->fZ * pScale.p[i];
               fDist = exp(fDist);

               dwScalePixel = (DWORD) (fDist * 0xffff);
               dwScaleFog = 0x10000 - dwScalePixel;

               paw[i] =(WORD)((dwScaleFog * awFog[i] + dwScalePixel * paw[i]) >> 16);
            } // i
      } // x

   }
   else {
      // floating point
      DWORD x, dwMax;
      PFIMAGEPIXEL p;
      fp fDist, fDistInv;
      dwMax = pFImageDest->Width() * pFImageDest->Height();
      for (x = pep->dwStart, p = pFImageDest->Pixel(0,0) + pep->dwStart; x < pep->dwEnd; x++, p++) {
         WORD wID = HIWORD(p->dwID);

         // cases to ignore
         if (!wID && m_fIgnoreBackground)
            continue;
         if ((wID == wSkydome) && wSkydome && m_fIgnoreSkydome)
            continue;
         if (p->fZ <= fTooClose)
            continue;

         float *paf = &p->fRed;
         if (fAllSame) {
            fDist = p->fZ * pScale.p[0];
            fDist = exp(fDist);
            fDistInv = 1.0 - fDist;

            for (i = 0; i < 3; i++)
               paf[i] = fDistInv * pColor.p[i] + fDist * paf[i];
         }
         else // different values
            for (i = 0; i < 3; i++) {
               if (p->fZ <= afTooClose[i])
                  continue;      // no change

               fDist = p->fZ * pScale.p[i];
               fDist = exp(fDist);
               fDistInv = 1.0 - fDist;

               paf[i] = fDistInv * pColor.p[i] + fDist * paf[i];
            } // i
      } // x
   }


}

/*********************************************************************************
CNPREffectFog::Render - From CNPREffect
*/
BOOL CNPREffectFog::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   if (!m_fFog)
      return TRUE;

   WORD awFog[3];

   // find skydome
   DWORD i;
   DWORD dwSkydome = -1;
   if (m_fIgnoreSkydome || m_fSkydomeColor)
      dwSkydome = FindSkydome (pWorld);
   if (!m_fSkydomeColor || !GetSkydomeRGB (pWorld, dwSkydome, pRender, awFog, m_fFog)) {
      Gamma (m_cFogColor, awFog);

      if (pRender) {
         fp fExposure = pRender->ExposureGet();
         fExposure = (CM_BESTEXPOSURE / max(fExposure, CLOSE));
         for (i = 0; i < 3; i++) {
            fp f = awFog[i] * fExposure;
            f = min(f, (fp)0xffff);
            f = max(f, (fp)0);
            awFog[i] = (WORD)f;
         }
      }
   }
   WORD wSkydome = (dwSkydome == -1) ? 0 : ((WORD)dwSkydome + 1);


   // scaling for Z
   CPoint pScale;
   for (i = 0; i < 3; i++) {
      m_pDist.p[i] = max(m_pDist.p[i], 0.0001);
      pScale.p[i] = 1.0 / m_pDist.p[i] * log(0.5);
   }

   // BUGFIX - Multithreaded
   EMTFOG em;
   memset (&em, 0, sizeof(em));
   memcpy (em.awFog, awFog, sizeof(awFog));
   // em.pColor.Copy (&pColor);
   em.pImageDest = pDest;
   em.pImageSrc = pOrig;
   em.pScale.Copy (&pScale);
   em.wSkydome = wSkydome;
   ThreadLoop (0, pDest->Width() * pDest->Height(), 1, &em, sizeof(em));

   return TRUE;
}



/*********************************************************************************
CNPREffectFog::Render - From CNPREffect
*/
BOOL CNPREffectFog::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   if (!m_fFog)
      return TRUE;

   CPoint pColor;

   // find skydome
   DWORD i;
   DWORD dwSkydome = -1;
   if (m_fIgnoreSkydome || m_fSkydomeColor)
      dwSkydome = FindSkydome (pWorld);
   if (!m_fSkydomeColor || !GetSkydomeRGB (pWorld, dwSkydome, pRender, &pColor, m_fFog)) {
      WORD awFog[3];
      Gamma (m_cFogColor, awFog);
      fp fExposure = 1;

      if (pRender) {
         fp fExposure = pRender->ExposureGet();
         fExposure = (CM_BESTEXPOSURE / max(fExposure, CLOSE));
      }

      for (i = 0; i < 3; i++)
         pColor.p[i] = (fp)awFog[i] * fExposure;
   }
   WORD wSkydome = (dwSkydome == -1) ? 0 : ((WORD)dwSkydome + 1);


   // scaling for Z
   CPoint pScale;
   for (i = 0; i < 3; i++) {
      m_pDist.p[i] = max(m_pDist.p[i], 0.0001);
      pScale.p[i] = 1.0 / m_pDist.p[i] * log(0.5);
   }


   // BUGFIX - Multithreaded
   EMTFOG em;
   memset (&em, 0, sizeof(em));
   // memcpy (em.awFog, awFog, sizeof(awFog));
   em.pColor.Copy (&pColor);
   em.pFImageDest = pDest;
   em.pFImageSrc = pOrig;
   em.pScale.Copy (&pScale);
   em.wSkydome = wSkydome;
   ThreadLoop (0, pDest->Width() * pDest->Height(), 1, &em, sizeof(em));


   return TRUE;
}




/****************************************************************************
EffectFogPage
*/
BOOL EffectFogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFOGPAGE pmp = (PFOGPAGE)pPage->m_pUserData;
   PCNPREffectFog pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         FillStatusColor (pPage, L"color", pv->m_cFogColor);

         // set the checkbox
         if (pControl = pPage->ControlFind (L"showfog"))
            pControl->AttribSetBOOL (Checked(), pv->m_fFog);
         if (pControl = pPage->ControlFind (L"skydomecolor"))
            pControl->AttribSetBOOL (Checked(), pv->m_fSkydomeColor);
         if (pControl = pPage->ControlFind (L"ignoreskydome"))
            pControl->AttribSetBOOL (Checked(), pv->m_fIgnoreSkydome);
         if (pControl = pPage->ControlFind (L"ignorebackground"))
            pControl->AttribSetBOOL (Checked(), pv->m_fIgnoreBackground);

         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"dist%d", (int)i);
            MeasureToString (pPage, szTemp, pv->m_pDist.p[i]);
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // if any edit changed then just get values
         DWORD i;
         WCHAR szTemp[32];
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"dist%d", (int)i);
            MeasureParseString (pPage, szTemp, &pv->m_pDist.p[i]);
            pv->m_pDist.p[i] = max(pv->m_pDist.p[i], 0.001);
         }
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

         // if it's for the colors then do those
         if (!_wcsicmp(p->pControl->m_pszName, L"changecolor")) {
            pv->m_cFogColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cFogColor, pPage, L"color");
            pPage->Message (ESCM_USER+189);  // update bitmap
         }
         // if it's for the colors then do those
         else if (!_wcsicmp(p->pControl->m_pszName, L"showfog")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fFog) {
               pv->m_fFog = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"skydomecolor")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fSkydomeColor) {
               pv->m_fSkydomeColor = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"ignoreskydome")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fIgnoreSkydome) {
               pv->m_fIgnoreSkydome = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"ignorebackground")) {
            BOOL fNew;
            fNew = p->pControl->AttribGetBOOL (Checked());
            if (fNew != pv->m_fIgnoreBackground) {
               pv->m_fIgnoreBackground = fNew;
               pPage->Message (ESCM_USER+189);  // update bitmap
            }
         };
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
            p->pszSubString = L"Fog";
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
CNPREffectFog::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectFog::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectFog::Dialog - From CNPREffect
*/
BOOL CNPREffectFog::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   FOGPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTFOG, EffectFogPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

