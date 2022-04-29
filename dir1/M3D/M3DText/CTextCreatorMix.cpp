/********************************************************************************
CTextCreatorMix.cpp - Code for handling faces.

begun 12/1/06 by Mike Rozak
Copyright 2006 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"




/****************************************************************************
MixPage
*/
BOOL MixPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorMix pv = (PCTextCreatorMix) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   // call sub-textures for trap
   if (pv->m_TextA.PageTrapMessages (pv->m_dwRenderShard, L"TextureA", pPage, dwMessage, pParam))
      return TRUE;
   if (pv->m_TextB.PageTrapMessages (pv->m_dwRenderShard, L"TextureB", pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         //PCEscControl pControl;
         //ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         //pControl = pPage->ControlFind (L"editmaterial");
         //if (pControl)
         //   pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

         DoubleToControl (pPage, L"mixbumpscale", pv->m_fMixBumpScale);
         DoubleToControl (pPage, L"mixglowscale", pv->m_fMixGlowScale);
         ComboBoxSet (pPage, L"mixcolor", pv->m_dwMixColor);
         ComboBoxSet (pPage, L"mixspec", pv->m_dwMixSpec);
         ComboBoxSet (pPage, L"mixtrans", pv->m_dwMixTrans);
         ComboBoxSet (pPage, L"mixbump", pv->m_dwMixBump);
         ComboBoxSet (pPage, L"mixglow", pv->m_dwMixGlow);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         //if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
         //   if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
         //      pPage->Exit (L"[close]");
         //   return TRUE;
         //}
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         //if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
         //   DWORD dwVal;
         //   dwVal = p->pszName ? _wtoi(p->pszName) : 0;
         //   if (dwVal == pv->m_Material.m_dwID)
         //      break; // unchanged
         //   if (dwVal)
         //      pv->m_Material.InitFromID (dwVal);
         //   else
         //      pv->m_Material.m_dwID = MATERIAL_CUSTOM;

         //   // eanble/disable button to edit
         //   PCEscControl pControl;
         //   pControl = pPage->ControlFind (L"editmaterial");
         //   if (pControl)
         //      pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         //   return TRUE;
         //}
         //else
         if (!_wcsicmp(p->pControl->m_pszName, L"mixcolor")) {
            pv->m_dwMixColor = p->pszName ? _wtoi(p->pszName) : 0;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"mixspec")) {
            pv->m_dwMixSpec = p->pszName ? _wtoi(p->pszName) : 0;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"mixtrans")) {
            pv->m_dwMixTrans = p->pszName ? _wtoi(p->pszName) : 0;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"mixbump")) {
            pv->m_dwMixBump = p->pszName ? _wtoi(p->pszName) : 0;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"mixglow")) {
            pv->m_dwMixGlow = p->pszName ? _wtoi(p->pszName) : 0;
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

         pv->m_fMixBumpScale = DoubleFromControl (pPage, L"mixbumpscale");
         pv->m_fMixGlowScale = DoubleFromControl (pPage, L"mixglowscale");
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Mix textures";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorMix::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

//back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREMIX, MixPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorMix::CTextCreatorMix (DWORD dwRenderShard, DWORD dwType)
{
   //m_Material.InitFromID (MATERIAL_FLAT);
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;

   m_fPixelLen = .01;
   m_fPatternWidth = 1;
   m_fPatternHeight = 1;

   m_dwMixColor = m_dwMixSpec = m_dwMixTrans = m_dwMixBump = m_dwMixGlow = 0;
   m_fMixBumpScale = m_fMixGlowScale = 1;

   m_TextA.StretchToFitSet (TRUE); // so that doesn't reapply texture stretch
   m_TextB.StretchToFitSet (TRUE);

}

void CTextCreatorMix::Delete (void)
{
   delete this;
}

static PWSTR gpszPatternWidth = L"PatternWidth";
static PWSTR gpszPatternHeight = L"PatternHeight";
static PWSTR gpszMix = L"Mix";
static PWSTR gpszType = L"Type";
static PWSTR gpszPixelLen = L"PixelLen";
static PWSTR gpszTextA = L"TextA";
static PWSTR gpszTextB = L"TextB";
static PWSTR gpszMixColor = L"MixColor";
static PWSTR gpszMixSpec = L"MixSpec";
static PWSTR gpszMixTrans = L"MixTrans";
static PWSTR gpszMixBump = L"MixBump";
static PWSTR gpszMixGlow = L"MixGlow";
static PWSTR gpszMixBumpScale = L"MixBumpScale";
static PWSTR gpszMixGlowScale = L"MixGlowScale";

PCMMLNode2 CTextCreatorMix::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMix);

   //m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);

   MMLValueSet (pNode, gpszMixColor, (int)m_dwMixColor);
   MMLValueSet (pNode, gpszMixSpec, (int)m_dwMixSpec);
   MMLValueSet (pNode, gpszMixTrans, (int)m_dwMixTrans);
   MMLValueSet (pNode, gpszMixBump, (int)m_dwMixBump);
   MMLValueSet (pNode, gpszMixGlow, (int)m_dwMixGlow);
   MMLValueSet (pNode, gpszMixBumpScale, m_fMixBumpScale);
   MMLValueSet (pNode, gpszMixGlowScale, m_fMixGlowScale);

   PCMMLNode2 pSub;
   pSub = m_TextA.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszTextA);
      pNode->ContentAdd (pSub);
   }
   pSub = m_TextB.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszTextB);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}

BOOL CTextCreatorMix::MMLFrom (PCMMLNode2 pNode)
{
   m_TextA.Clear();
   m_TextB.Clear();

   //m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);

   m_dwMixColor = (DWORD) MMLValueGetInt (pNode, gpszMixColor, 0);
   m_dwMixSpec = (DWORD) MMLValueGetInt (pNode, gpszMixSpec, 0);
   m_dwMixTrans = (DWORD) MMLValueGetInt (pNode, gpszMixTrans, 0);
   m_dwMixBump = (DWORD) MMLValueGetInt (pNode, gpszMixBump, 0);
   m_dwMixGlow = (DWORD) MMLValueGetInt (pNode, gpszMixGlow, 0);
   m_fMixBumpScale = MMLValueGetDouble (pNode, gpszMixBumpScale, 1);
   m_fMixGlowScale = MMLValueGetDouble (pNode, gpszMixGlowScale, 1);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszTextA)) {
         m_TextA.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszTextB)) {
         m_TextB.MMLFrom (pSub);
         continue;
      }
   } // i
   return TRUE;
}

BOOL CTextCreatorMix::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwX = max(dwX,1);
   dwY = max(dwY,1);
   dwScale = max(dwX, dwY);

   //memcpy (pMaterial, &m_Material, sizeof(m_Material));
   switch (m_dwMixSpec) {
   default:
      memcpy (pMaterial, m_TextA.MaterialGet(0), sizeof(*pMaterial));
      break;
   case 1:  // from B
      memcpy (pMaterial, m_TextB.MaterialGet(0), sizeof(*pMaterial));
      break;
   }// m_dwMixMaterial
   memset (pTextInfo, 0, sizeof(TEXTINFO));

   m_TextA.ScaleSet (m_fPatternWidth, m_fPatternHeight);
   m_TextB.ScaleSet (m_fPatternWidth, m_fPatternHeight);

   // initially fill the texture with what's in A
   fp fGlowScaleA;
   m_TextA.FillTexture (m_dwRenderShard, m_fPatternWidth, m_fPatternHeight, fPixelLen, TRUE,
      pImage, pImage2, pMaterial, &fGlowScaleA);

   // also generate texture B
   CImage ImageB, ImageB2;
   fp fGlowScaleB;
   CMaterial MatB;
   m_TextB.FillTexture (m_dwRenderShard, m_fPatternWidth, m_fPatternHeight, fPixelLen, TRUE,
      &ImageB, &ImageB2, &MatB, &fGlowScaleB);

   // do all the twiddling
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   PIMAGEPIXEL pip2 = pImage2->Pixel(0,0);
   PIMAGEPIXEL pipB = ImageB.Pixel(0,0);
   PIMAGEPIXEL pipB2 = ImageB2.Pixel(0,0);
   DWORD i, j;
   DWORD dwNum = pImage->Width() * pImage->Height();
   fp fMixBumpScale = m_fMixBumpScale;
   fp fGlowScaleTotalInv;
   if ((m_dwMixBump == 10) || (m_dwMixBump == 11))
      fMixBumpScale *= 0.01 / fPixelLen;
   switch (m_dwMixGlow) {
      case 0:  // from A
      default:
         pTextInfo->fGlowScale = fGlowScaleA;
         break;
      case 1:  // from B
         pTextInfo->fGlowScale = fGlowScaleB;
         break;
      case 2:  // (A+B)/2
         pTextInfo->fGlowScale = (fGlowScaleA + fGlowScaleB) / 2;
         fGlowScaleTotalInv = fGlowScaleA + fGlowScaleB;
         if (fGlowScaleTotalInv)
            fGlowScaleTotalInv = 1.0 / fGlowScaleTotalInv;
         break;
      case 3:  // none
         pTextInfo->fGlowScale = 0;
         break;
      case 10: // A's color
      case 11: // B's color
         pTextInfo->fGlowScale = 1;
         break;
   } // m_dwMixGlow
   pTextInfo->fGlowScale *= m_fMixGlowScale;

   for (i = 0; i < dwNum; i++, pip++, pip2++, pipB++, pipB2++) {
      // specularity
      WORD wHigh, wLow;
      switch (m_dwMixSpec) {
      case 0:  // from A
      default:
         // do nothing
         break;
      case 1:  // from B
         pip->dwID = pipB->dwID;
         break;
      case 2:  // (A+B)/2
         wLow = LOWORD(pip->dwID)/2 + LOWORD(pipB->dwID)/2;
         wHigh = HIWORD(pip->dwID)/2 + HIWORD(pipB->dwID)/2;
         pip->dwID = MAKELONG (wLow, wHigh);
         break;
      case 3:  // none
         wLow = 100;
         wHigh = 0;
         pip->dwID = MAKELONG (wLow, wHigh);
         break;
      } // switch specularity

      // bump
      fp fBump;
      switch (m_dwMixBump) {
      case 0:  // from A
      default:
         fBump = pip->fZ;
         break;
      case 1:  // from B
         fBump = pipB->fZ;
         break;
      case 2:  // (A+B)/2
         fBump = (pip->fZ + pipB->fZ) / 2;
         break;
      case 3:  // none
         fBump = 0;
         break;
      case 10: // A's color
         fBump = (fp)((DWORD)pip->wRed + (DWORD)pip->wGreen + (DWORD)pip->wBlue) / (fp)(3 * 0xffff);
         break;
      case 11: // B's color
         fBump = (fp)((DWORD)pipB->wRed + (DWORD)pipB->wGreen + (DWORD)pipB->wBlue) / (fp)(3 * 0xffff);
         break;
      } // switch bump
      pip->fZ = fBump * fMixBumpScale;

      // glow
      fp fGlow;
      switch (m_dwMixGlow) {
      case 0:  // from A
      default:
         // do nothing
         break;
      case 1:  // from B
         pip2->wRed = pipB2->wRed;
         pip2->wGreen = pipB2->wGreen;
         pip2->wBlue = pipB2->wBlue;
         break;
      case 2:  // (A+B)/2
         for (j = 0; j < 3; j++) {
            fGlow = (fp)((&pip2->wRed)[j]) * fGlowScaleA + (fp)((&pipB2->wRed)[j]) * fGlowScaleB;
            fGlow *= fGlowScaleTotalInv;
            fGlow = max(fGlow, 0);
            fGlow = min(fGlow, (fp)0xffff);
            (&pip2->wRed)[j] = (WORD)fGlow;
         } // j
         break;
      case 3:  // none
         pip2->wRed = pip2->wGreen = pip2->wBlue = 0;
         break;
      case 10: // A's color
         pip2->wRed = pip->wRed;
         pip2->wGreen = pip->wGreen;
         pip2->wBlue = pip->wBlue;
         break;
      case 11: // B's color
         pip2->wRed = pipB->wRed;
         pip2->wGreen = pipB->wGreen;
         pip2->wBlue = pipB->wBlue;
         break;
      } // switch bump

      // transparency
      BYTE bTrans;
      switch (m_dwMixTrans) {
      case 0:  // from A
      default:
         // do nothing
         break;
      case 1:  // from B
         bTrans = LOBYTE(pipB->dwIDPart);
         pip->dwIDPart = (pip->dwIDPart & ~0xff) | bTrans;
         break;
      case 2:  // (A+B)/2
         bTrans = (BYTE) (((WORD)LOBYTE(pip->dwIDPart) + (WORD)LOBYTE(pipB->dwIDPart) + 1) / 2);
         pip->dwIDPart = (pip->dwIDPart & ~0xff) | bTrans;
         break;
      case 3:  // none
         pip->dwIDPart = (pip->dwIDPart & ~0xff);
         break;

      case 10: // A's color
      case 12: // A's color inv
         bTrans = (BYTE) (((DWORD)pip->wRed + (DWORD)pip->wGreen + (DWORD)pip->wBlue) / (3 * 256));
         if (m_dwMixTrans == 12)
            bTrans = 255 - bTrans;
         pip->dwIDPart = (pip->dwIDPart & ~0xff) | bTrans;
         break;
      case 11: // B's color
      case 13: // B's color inv
         bTrans = (BYTE) (((DWORD)pipB->wRed + (DWORD)pipB->wGreen + (DWORD)pipB->wBlue) / (3 * 256));
         if (m_dwMixTrans == 13)
            bTrans = 255 - bTrans;
         pip->dwIDPart = (pip->dwIDPart & ~0xff) | bTrans;
         break;
      } // switch specularity

      // color
      // NOTE: Doing color last since some others depend on it
      switch (m_dwMixColor) {
      case 0:  // from A
      default:
         // do nothing
         break;
      case 1:  // from B
         pip->wRed = pipB->wRed;
         pip->wGreen = pipB->wGreen;
         pip->wBlue = pipB->wBlue;
         break;
      case 2:  // (A+B)/2
         pip->wRed = pip->wRed / 2 + pipB->wRed / 2;
         pip->wGreen = pip->wGreen / 2 + pipB->wGreen / 2;
         pip->wBlue = pip->wBlue / 2 + pipB->wBlue / 2;
         break;
      case 3:  // none
         pip->wRed = pip->wGreen = pip->wBlue = 0;
         break;
      } // switch color
   } // i

   // moved earlier memcpy (pMaterial, &m_Material, sizeof(m_Material));
   // moved earlier memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   // glow
   if (pTextInfo->fGlowScale)
      pTextInfo->dwMap |= 0x08;


   return TRUE;
}




/***********************************************************************************
CTextCreatorMix::TextureQuery - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If the texture is already on here,
                     this returns. If not, the texture is added, and sub-textures
                     are also added.
   PCBTree           pTree - Also added to
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success
*/
BOOL CTextCreatorMix::TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;

   
   // add itself
   plText->Add (pagThis);
   pTree->Add (szTemp, NULL, 0);

   // NEW CODE: go through all sub-textures
   m_TextA.TextureQuery (m_dwRenderShard, plText, pTree);
   m_TextB.TextureQuery (m_dwRenderShard, plText, pTree);

   return TRUE;
}


/***********************************************************************************
CTextCreatorMix::SubTextureNoRecurse - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if they recurse
*/
BOOL CTextCreatorMix::SubTextureNoRecurse (PCListFixed plText, GUID *pagThis)
{
   GUID *pag = (GUID*)plText->Get(0);
   DWORD i;
   for (i = 0; i < plText->Num(); i++, pag += 2)
      if (!memcmp (pag, pagThis, sizeof(GUID)*2))
         return FALSE;  // found itself

   // remember count
   DWORD dwNum = plText->Num();

   // add itself
   plText->Add (pagThis);

   // loop through all sub-texts
   BOOL fRet = TRUE;
   fRet = m_TextA.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;
   fRet = m_TextB.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;

done:
   // restore list
   plText->Truncate (dwNum);

   return fRet;
}
