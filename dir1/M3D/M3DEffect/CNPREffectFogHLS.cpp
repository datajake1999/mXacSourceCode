/*********************************************************************************
CNPREffectFogHLS.cpp - Code for effect

begun 14/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



// EMTFOGHLS- Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImage;
   PCFImage       pFImage;
   WORD           wSkydome;
   float          afFog[3];
   CPoint         pScale;
} EMTFOGHLS, *PEMTFOGHLS;



// FOGHLSPAGE - Page info
typedef struct {
   PCNPREffectFogHLS pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} FOGHLSPAGE, *PFOGHLSPAGE;



PWSTR gpszEffectFogHLS = L"FogHLS";



/*********************************************************************************
CNPREffectFogHLS::Constructor and destructor
*/
CNPREffectFogHLS::CNPREffectFogHLS (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_cFogHLSColor = RGB(0,0,0);
   m_pDist.Zero();
   m_pDist.p[0] = 0;
   m_pDist.p[1] = 50;
   m_pDist.p[2] = 10;
   m_fIgnoreSkydome = FALSE;
   m_fIgnoreBackground = TRUE;
}

CNPREffectFogHLS::~CNPREffectFogHLS (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectFogHLS::Delete - From CNPREffect
*/
void CNPREffectFogHLS::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectFogHLS::QueryInfo - From CNPREffect
*/
void CNPREffectFogHLS::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Creates a \"fog\" that causes distant objects to fade to grey.";
   pqi->pszName = L"Fog (hue, lighness, saturation)";
   pqi->pszID = gpszEffectFogHLS;
}



static PWSTR gpszFogHLSColor = L"FogHLSColor";
static PWSTR gpszFogHLS = L"FogHLS";
static PWSTR gpszDist = L"Dist";
static PWSTR gpszSkydomeColor = L"SkydomeColor";
static PWSTR gpszIgnoreSkydome = L"IgnoreSkydome";
static PWSTR gpszIgnoreBackground = L"IgnoreBackground";

/*********************************************************************************
CNPREffectFogHLS::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectFogHLS::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectFogHLS);

   MMLValueSet (pNode, gpszFogHLSColor, (int)m_cFogHLSColor);

   MMLValueSet (pNode, gpszDist, &m_pDist);
   MMLValueSet (pNode, gpszIgnoreSkydome, (int)m_fIgnoreSkydome);
   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   return pNode;
}


/*********************************************************************************
CNPREffectFogHLS::MMLFrom - From CNPREffect
*/
BOOL CNPREffectFogHLS::MMLFrom (PCMMLNode2 pNode)
{
   m_cFogHLSColor = (COLORREF) MMLValueGetInt (pNode, gpszFogHLSColor, RGB(0xff,0xff,0xff));

   MMLValueGetPoint (pNode, gpszDist, &m_pDist);
   m_fIgnoreSkydome = (BOOL) MMLValueGetInt (pNode, gpszIgnoreSkydome, (int)TRUE);
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   return TRUE;
}




/*********************************************************************************
CNPREffectFogHLS::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectFogHLS::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectFogHLS::MMLFrom - From CNPREffect
*/
CNPREffectFogHLS * CNPREffectFogHLS::CloneEffect (void)
{
   PCNPREffectFogHLS pNew = new CNPREffectFogHLS(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_cFogHLSColor = m_cFogHLSColor;
   pNew->m_pDist.Copy (&m_pDist);
   pNew->m_fIgnoreSkydome = m_fIgnoreSkydome;
   pNew->m_fIgnoreBackground = m_fIgnoreBackground;

   return pNew;
}





/*********************************************************************************
CNPREffectFogHLS::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectFogHLS::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTFOGHLS pep = (PEMTFOGHLS)pParams;

   PCImage pImage = pep->pImage;
   PCFImage pFImage = pep->pFImage;
   WORD wSkydome = pep->wSkydome;
   float afFog[3];
   memcpy (afFog, pep->afFog, sizeof(afFog));
   CPoint pScale;
   pScale.Copy (&pep->pScale);

   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   DWORD dwMax= dwWidth * dwHeight;
   DWORD i, j;
   float af[3];
   float *paf = &af[0];
   float f;
   if (pip)
      pip += pep->dwStart;
   if (pfp)
      pfp += pep->dwStart;
   for (i = pep->dwStart; i < pep->dwEnd; i++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
      WORD wID = HIWORD(pip ? pip->dwID : pfp->dwID);
      fp fZ = pip ? pip->fZ : pfp->fZ;

      // cases to ignore
      if (!wID && m_fIgnoreBackground)
         continue;
      if ((wID == wSkydome) && wSkydome && m_fIgnoreSkydome)
         continue;
      if (fZ <= 0)
         continue;

      if (pip) {
         af[0] = pip->wRed;
         af[1] = pip->wGreen;
         af[2] = pip->wBlue;
      }
      else
         paf = &pfp->fRed;

      float afHLS[3];
      ToHLS256 (paf[0] / 256.0, paf[1] / 256.0, paf[2] / 256.0,
         &afHLS[0], &afHLS[1], &afHLS[2]);

      fp fDist, fDistInv;
      BOOL fModulo;
      for (j = 0; j < 3; j++) {
         if (m_pDist.p[j] <= 0)
            continue;   // if 0 or negative then dont modify

         if (!j) {
            if (fabs(afHLS[0] - afFog[0]) >= 128) {
               // modulo color
               if (afFog[0] < afHLS[0])
                  afHLS[0] -= 256;
               else
                  afHLS[0] += 256;
               fModulo = TRUE;
            }
            else
               fModulo = FALSE;
         }

         fDist = fZ * pScale.p[j];
         fDist = exp(fDist);
         fDistInv = 1.0 - fDist;

         afHLS[j] = fDistInv * afFog[j] + fDist * afHLS[j];

         // do modulo
         if (!j && fModulo)
            afHLS[0] = myfmod(afHLS[0], 256);
      } // i


      FromHLS256 (afHLS[0], afHLS[1], afHLS[2], &paf[0], &paf[1], &paf[2]);
      paf[0] *= 256.0;
      paf[1] *= 256.0;
      paf[2] *= 256.0;

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
CNPREffectFogHLS::RenderAny - This renders both integer and FP image.

inputs
   PCWorldSocket  pWorld - World
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectFogHLS::RenderAny (PCWorldSocket pWorld, PCImage pImage, PCFImage pFImage)
{
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   DWORD dwSkydome = -1;
   if (m_fIgnoreSkydome)
      dwSkydome = FindSkydome (pWorld);
   WORD wSkydome = (dwSkydome == -1) ? 0 : ((WORD)dwSkydome + 1);

   WORD awFogRGB[3];
   float afFog[3];
   Gamma (m_cFogHLSColor, awFogRGB);
   ToHLS256 ((float)awFogRGB[0] / 256.0, (float)awFogRGB[1] / 256.0, (float)awFogRGB[2] / 256.0,
            &afFog[0], &afFog[1], &afFog[2]);
   
   // scaling for Z
   CPoint pScale;
   DWORD i;
   for (i = 0; i < 3; i++)
      pScale.p[i] = 1.0 / max(m_pDist.p[i], 0.0001) * log(0.5);


   // BUGFIX - Multithreded
   EMTFOGHLS em;
   memset (&em, 0, sizeof(em));
   memcpy (em.afFog, afFog, sizeof(afFog));
   em.pFImage = pFImage;
   em.pImage = pImage;
   em.pScale.Copy (&pScale);
   em.wSkydome = wSkydome;
   ThreadLoop (0, dwWidth * dwHeight, 1, &em, sizeof(em));

   return TRUE;
}



/*********************************************************************************
CNPREffectFogHLS::Render - From CNPREffect
*/
BOOL CNPREffectFogHLS::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pWorld, pDest, NULL);
}



/*********************************************************************************
CNPREffectFogHLS::Render - From CNPREffect
*/
BOOL CNPREffectFogHLS::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pWorld, NULL, pDest);
}




/****************************************************************************
EffectFogHLSPage
*/
BOOL EffectFogHLSPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFOGHLSPAGE pmp = (PFOGHLSPAGE)pPage->m_pUserData;
   PCNPREffectFogHLS pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         FillStatusColor (pPage, L"color", pv->m_cFogHLSColor);

         // set the checkbox
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
            pv->m_cFogHLSColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cFogHLSColor, pPage, L"color");
            pPage->Message (ESCM_USER+189);  // update bitmap
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
            p->pszSubString = L"Fog (hue, lightness, saturation)";
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
CNPREffectFogHLS::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectFogHLS::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectFogHLS::Dialog - From CNPREffect
*/
BOOL CNPREffectFogHLS::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   FOGHLSPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTFOGHLS, EffectFogHLSPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

