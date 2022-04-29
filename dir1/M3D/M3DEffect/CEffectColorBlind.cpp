/*********************************************************************************
CNPREffectColorBlind.cpp - Code for effect

begun 22/4/2005
Copyright 2005 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"





// COLORPAGE - Page info
typedef struct {
   PCNPREffectColorBlind pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} COLORPAGE, *PCOLORPAGE;



PWSTR gpszEffectColorBlind = L"ColorBlind";



/*********************************************************************************
CNPREffectColorBlind::Constructor and destructor
*/
CNPREffectColorBlind::CNPREffectColorBlind (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_dwRed = 0x01;
   m_dwGreen = 0x02;
   m_dwBlue = 0x04;

}

CNPREffectColorBlind::~CNPREffectColorBlind (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectColorBlind::Delete - From CNPREffect
*/
void CNPREffectColorBlind::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectColorBlind::QueryInfo - From CNPREffect
*/
void CNPREffectColorBlind::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Simulates color blindness, eliminating one or more of the chroma.";
   pqi->pszName = L"Color blindness";
   pqi->pszID = gpszEffectColorBlind;
}



static PWSTR gpszRed = L"Red";
static PWSTR gpszGreen = L"Green";
static PWSTR gpszBlue = L"Blue";

/*********************************************************************************
CNPREffectColorBlind::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectColorBlind::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectColorBlind);

   MMLValueSet (pNode, gpszRed, (int)m_dwRed);
   MMLValueSet (pNode, gpszGreen, (int)m_dwGreen);
   MMLValueSet (pNode, gpszBlue, (int)m_dwBlue);

   return pNode;
}


/*********************************************************************************
CNPREffectColorBlind::MMLFrom - From CNPREffect
*/
BOOL CNPREffectColorBlind::MMLFrom (PCMMLNode2 pNode)
{
   m_dwRed = (DWORD) MMLValueGetInt (pNode, gpszRed, (int)0x01);
   m_dwGreen = (DWORD) MMLValueGetInt (pNode, gpszGreen, (int)0x02);
   m_dwBlue = (DWORD) MMLValueGetInt (pNode, gpszBlue, (int)0x04);

   return TRUE;
}




/*********************************************************************************
CNPREffectColorBlind::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectColorBlind::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectColorBlind::MMLFrom - From CNPREffect
*/
CNPREffectColorBlind * CNPREffectColorBlind::CloneEffect (void)
{
   PCNPREffectColorBlind pNew = new CNPREffectColorBlind(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_dwRed = m_dwRed;
   pNew->m_dwGreen = m_dwGreen;
   pNew->m_dwBlue = m_dwBlue;

   return pNew;
}




/*********************************************************************************
CNPREffectColorBlind::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectColorBlind::RenderAny (PCImage pImage, PCFImage pFImage)
{
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   DWORD i, j;
   DWORD dwMax= dwWidth * dwHeight;
   WORD aw[3];
   float af[3];
   DWORD adwBlind[3];
   adwBlind[0] = m_dwRed;
   adwBlind[1] = m_dwGreen;
   adwBlind[2] = m_dwBlue;

   if (pip) {
      // integer bitmap
      for (i = 0; i < dwMax; i++, pip++) {
         for (j = 0; j < 3; j++) {
            switch (adwBlind[j]) {
               case 0: // nothing
                  aw[j] = 0;
                  break;
               case 1: // r
                  aw[j] = pip->wRed;
                  break;
               case 2: // g
                  aw[j] = pip->wGreen;
                  break;
               case 3: // r + g
                  aw[j] = pip->wRed/2 + pip->wGreen/2;
                  break;
               case 4: // b
                  aw[j] = pip->wBlue;
                  break;
               case 5: // r + b
                  aw[j] = pip->wRed/2 + pip->wBlue/2;
                  break;
               case 6: // g + b
                  aw[j] = pip->wGreen/2 + pip->wBlue/2;
                  break;
               case 7: // r + g + b
                  aw[j] = pip->wRed/3 + pip->wGreen/3 + pip->wBlue/3;
                  break;
            } // switch
         } // j

         // copy over
         pip->wRed = aw[0];
         pip->wGreen = aw[1];
         pip->wBlue = aw[2];
      } // i
   }
   else {
      // floating point
      for (i = 0; i < dwMax; i++, pfp++) {
         for (j = 0; j < 3; j++) {
            switch (adwBlind[j]) {
               case 0: // nothing
                  af[j] = 0;
                  break;
               case 1: // r
                  af[j] = pfp->fRed;
                  break;
               case 2: // g
                  af[j] = pfp->fGreen;
                  break;
               case 3: // r + g
                  af[j] = pfp->fRed/2 + pfp->fGreen/2;
                  break;
               case 4: // b
                  af[j] = pfp->fBlue;
                  break;
               case 5: // r + b
                  af[j] = pfp->fRed/2 + pfp->fBlue/2;
                  break;
               case 6: // g + b
                  af[j] = pfp->fGreen/2 + pfp->fBlue/2;
                  break;
               case 7: // r + g + b
                  af[j] = pfp->fRed/3 + pfp->fGreen/3 + pfp->fBlue/3;
                  break;
            } // switch
         } // j

         // copy over
         pfp->fRed = af[0];
         pfp->fGreen = af[1];
         pfp->fBlue = af[2];
      } // i
   }

   return TRUE;
}


/*********************************************************************************
CNPREffectColorBlind::Render - From CNPREffect
*/
BOOL CNPREffectColorBlind::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL);
}



/*********************************************************************************
CNPREffectColorBlind::Render - From CNPREffect
*/
BOOL CNPREffectColorBlind::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest);
}




/****************************************************************************
EffectColorBlindPage
*/
BOOL EffectColorBlindPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCOLORPAGE pmp = (PCOLORPAGE)pPage->m_pUserData;
   PCNPREffectColorBlind pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         DWORD i, j;
         WCHAR szTemp[64];
         PWSTR apszColor[3] = {L"red", L"green", L"blue"};
         DWORD *pdwVal;
         for (i = 0; i < 3; i++) {
            switch (i) {
            case 0:
               pdwVal = &pv->m_dwRed;
               break;
            case 1:
               pdwVal = &pv->m_dwGreen;
               break;
            case 2:
               pdwVal = &pv->m_dwBlue;
               break;
            } // switch

            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"%s%d", apszColor[i], (int)j);
               if (pControl = pPage->ControlFind (szTemp))
                  pControl->AttribSetBOOL (Checked(), (*pdwVal & (1 << j)) ? TRUE : FALSE);
            } // j
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

         PWSTR apszColor[3] = {L"red", L"green", L"blue"};
         DWORD adwColorLen[3] = {(DWORD)wcslen(apszColor[0]), (DWORD)wcslen(apszColor[1]), (DWORD)wcslen(apszColor[2])};

         DWORD i;
         for (i = 0; i < 3; i++)
            if (!wcsncmp (p->pControl->m_pszName, apszColor[i], adwColorLen[i])) {
               DWORD dwDim = _wtoi(p->pControl->m_pszName + adwColorLen[i]);
               DWORD *pdwVal;
               switch (i) {
               case 0:
                  pdwVal = &pv->m_dwRed;
                  break;
               case 1:
                  pdwVal = &pv->m_dwGreen;
                  break;
               case 2:
                  pdwVal = &pv->m_dwBlue;
                  break;
               }

               if (p->pControl->AttribGetBOOL (Checked()))
                  *pdwVal = *pdwVal | (1 << dwDim);
               else
                  *pdwVal = *pdwVal & (~(1 << dwDim));

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
            p->pszSubString = L"Color blindness";
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
CNPREffectColorBlind::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectColorBlind::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectColorBlind::Dialog - From CNPREffect
*/
BOOL CNPREffectColorBlind::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTCOLORBLIND, EffectColorBlindPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

