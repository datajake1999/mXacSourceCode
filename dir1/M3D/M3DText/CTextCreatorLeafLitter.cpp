/********************************************************************************
CTextCreatorLeafLitter.cpp - Code for drawing leaf litter.

begun 9/4/07 by Mike Rozak
Copyright 2007 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"




/****************************************************************************
LeafLitterPage
*/
BOOL LeafLitterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorLeafLitter pv = (PCTextCreatorLeafLitter) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   // call sub-textures for trap
   if (pv->m_Background.PageTrapMessages (pv->m_dwRenderShard, L"Background", pPage, dwMessage, pParam))
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

         DoubleToControl (pPage, L"numx", pv->m_dwNumX);
         DoubleToControl (pPage, L"numy", pv->m_dwNumY);
         DoubleToControl (pPage, L"seed", pv->m_iSeed);

         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[64];
         for (i = 0; i < LEAFLITTERVARIETIES; i++) {
            swprintf (szTemp, L"leafuse%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetBOOL (Checked(), pv->m_afLeafUse[i]);
         } // i
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszLeafUse = L"leafuse";
         DWORD dwLeafUseLen = (DWORD)wcslen(pszLeafUse);
         if (!wcsncmp (psz, pszLeafUse, dwLeafUseLen)) {
            DWORD dwIndex = _wtoi(psz + dwLeafUseLen);

            pv->m_afLeafUse[dwIndex] = p->pControl->AttribGetBOOL(Checked());
         }
         //if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
         //   if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
         //      pPage->Exit (L"[close]");
         //   return TRUE;
         //}
      }
      break;

#if 0
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
      }
      break;
#endif // 0

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         pv->m_dwNumX = (DWORD) DoubleFromControl (pPage, L"numx");
         pv->m_dwNumY = (DWORD) DoubleFromControl (pPage, L"numy");
         pv->m_iSeed = (int) DoubleFromControl (pPage, L"seed");
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Leaf-litter textures";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorLeafLitter::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

   PWSTR pszLeafMod = L"leafmod";
   DWORD dwLeafModLen = (DWORD)wcslen(pszLeafMod);
back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURELEAFLITTER, LeafLitterPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   if (!wcsncmp (pszRet, pszLeafMod, dwLeafModLen)) {
      DWORD dwIndex = _wtoi(pszRet + dwLeafModLen);
      BOOL fRet = m_aLeaf[dwIndex].Dialog (m_dwRenderShard, pWindow);
      if (!fRet)
         return FALSE;
      goto back;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorLeafLitter::CTextCreatorLeafLitter (DWORD dwRenderShard, DWORD dwType)
{
   //m_Material.InitFromID (MATERIAL_FLAT);
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;

   m_dwNumX = m_dwNumY = 10;
   m_iSeed = 42;

   memset (m_afLeafUse, 0, sizeof(m_afLeafUse));

   m_fPixelLen = .01;
   m_fPatternWidth = 1;
   m_fPatternHeight = 1;

   m_Background.StretchToFitSet (TRUE); // so that doesn't reapply texture stretch

}

void CTextCreatorLeafLitter::Delete (void)
{
   delete this;
}

static PWSTR gpszPatternWidth = L"PatternWidth";
static PWSTR gpszPatternHeight = L"PatternHeight";
static PWSTR gpszLeafLitter = L"LeafLitter";
static PWSTR gpszType = L"Type";
static PWSTR gpszPixelLen = L"PixelLen";
static PWSTR gpszBackground = L"Background";
static PWSTR gpszNumX = L"NumX";
static PWSTR gpszNumY = L"NumY";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszLeaf = L"Leaf";
static PWSTR gpszLeafUse = L"LeafUse";

PCMMLNode2 CTextCreatorLeafLitter::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLeafLitter);

   //m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);

   MMLValueSet (pNode, gpszNumX, (int)m_dwNumX);
   MMLValueSet (pNode, gpszNumY, (int)m_dwNumY);
   MMLValueSet (pNode, gpszSeed, m_iSeed);

   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < LEAFLITTERVARIETIES; i++) {
      pSub = m_aLeaf[i].MMLTo ();
      if (!pSub)
         continue;
      MMLValueSet (pSub, gpszLeafUse, (int)m_afLeafUse[i]);

      pSub->NameSet (gpszLeaf);
      pNode->ContentAdd (pSub);
   } // i

   pSub = m_Background.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszBackground);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorLeafLitter::MMLFrom (PCMMLNode2 pNode)
{
   m_Background.Clear();
   memset (m_afLeafUse, 0, sizeof(m_afLeafUse));

   //m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);

   m_dwNumX = (DWORD) MMLValueGetInt (pNode, gpszNumX, 10);
   m_dwNumY = (DWORD) MMLValueGetInt (pNode, gpszNumY, 10);
   m_iSeed = MMLValueGetInt (pNode, gpszSeed, 42);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   DWORD dwLeafIndex = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszBackground)) {
         m_Background.MMLFrom (pSub);
         continue;
      }
      else if (!_wcsicmp(psz, gpszLeaf) && (dwLeafIndex < LEAFLITTERVARIETIES)) {
         m_aLeaf[dwLeafIndex].MMLFrom (pSub);

         m_afLeafUse[dwLeafIndex] = (BOOL) MMLValueGetInt (pSub, gpszLeafUse, TRUE);
         dwLeafIndex++;
         continue;
      }
   } // i
   return TRUE;
}

BOOL CTextCreatorLeafLitter::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwX = max(dwX,1);
   dwY = max(dwY,1);
   dwScale = max(dwX, dwY);

   // use the material from the Background
   memcpy (pMaterial, m_Background.MaterialGet(0), sizeof(*pMaterial));

   memset (pTextInfo, 0, sizeof(TEXTINFO));

   m_Background.ScaleSet (m_fPatternWidth, m_fPatternHeight);

   // initially fill the texture with what's in A
   fp fGlowScaleA;
   m_Background.FillTexture (m_dwRenderShard, m_fPatternWidth, m_fPatternHeight, fPixelLen, TRUE,
      pImage, pImage2, pMaterial, &fGlowScaleA);
   pTextInfo->fGlowScale = fGlowScaleA;

   // glow
   if (pTextInfo->fGlowScale)
      pTextInfo->dwMap |= 0x08;

   // set random seed
   srand (m_iSeed);

   // draw all the leaves
   m_dwNumX = max(m_dwNumX, 1);
   m_dwNumX = min(m_dwNumX, 0xffff);
   m_dwNumY = max(m_dwNumY, 1);
   m_dwNumY = min(m_dwNumY, 0xffff);

   // random order
   CListFixed lOrdering;
   lOrdering.Init (sizeof(DWORD));
   DWORD i, x, y;
   DWORD dwCoords;
   lOrdering.Required (m_dwNumX * m_dwNumY);
   for (y = 0; y < m_dwNumY; y++) for (x = 0; x < m_dwNumX; x++) {
      dwCoords = (y << 16) | x;
      lOrdering.Add (&dwCoords);
   } // y, x
   DWORD *padwOrdering = (DWORD*)lOrdering.Get(0);
   for (i = 0; i < lOrdering.Num(); i++) {
      DWORD dwIndex = (DWORD)rand() % lOrdering.Num();
      DWORD dwTemp = padwOrdering[i];
      padwOrdering[i] = padwOrdering[dwIndex];
      padwOrdering[dwIndex] = dwTemp;
   } // i

   // cache leaves
   DWORD dwLeafVarieties = 0;
   fp afLeafZMin[LEAFLITTERVARIETIES];
   memset (afLeafZMin, 0, sizeof(afLeafZMin));
   for (i = 0; i < LEAFLITTERVARIETIES; i++)
      if (m_afLeafUse[i]) {
         m_aLeaf[i].TextureCache(m_dwRenderShard);
         afLeafZMin[i] = m_aLeaf[i].FindMinZInTexture ();   // height in meters

         afLeafZMin[i] -= max(m_aLeaf[i].m_fWidth, m_aLeaf[i].m_fHeight) * m_fPixelLen / 20.0;
               // max leaves a bit thick, still in meters

         // however, since painting in, must be in pixels
         afLeafZMin[i] /= fPixelLen;

         dwLeafVarieties++;   // so know how many have
      }

   // draw all the leaves
   if (dwLeafVarieties) for (i = 0; i < lOrdering.Num(); i++) {
      fp fX = ((fp)(padwOrdering[i] & 0xffff) + randf(-0.5, 0.5)) / (fp)m_dwNumX;
      fp fY = ((fp)(padwOrdering[i] >> 16) + randf(-0.5, 0.5)) / (fp)m_dwNumY;
      fX = myfmod(fX, 1.0) * (fp)pImage->Width();
      fY = myfmod(fY, 1.0) * (fp)pImage->Height();

      // determine which variety
      DWORD dwVarietyLeft = (DWORD)rand() % dwLeafVarieties;
      DWORD j;
      for (j = 0; j < LEAFLITTERVARIETIES; j++) {
         if (!m_afLeafUse[j])
            continue;   // not used

         if (!dwVarietyLeft)
            break;
         dwVarietyLeft--;
      } // j

      CPoint pStart, pDir;
      pStart.Zero();
      pDir.Zero();
      fp fAngle = randf(0, 2.0 * PI);
      pStart.p[0] = fX;
      pStart.p[1] = fY;
      pDir.p[0] = sin(fAngle);
      pDir.p[1] = cos(fAngle);

      // get the height
      fp fHeight = m_aLeaf[j].FindMaxZ (pImage, m_fPixelLen / fPixelLen, &pStart, &pDir);

      m_aLeaf[j].Render (pImage, 1.0, m_fPixelLen / fPixelLen, &pStart, &pDir, fHeight - afLeafZMin[j]);
   } // i

   // uncache leaves
   for (i = 0; i < LEAFLITTERVARIETIES; i++)
      if (m_afLeafUse[i])
         m_aLeaf[i].TextureRelease();

   // moved earlier memcpy (pMaterial, &m_Material, sizeof(m_Material));
   // moved earlier memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   return TRUE;
}




/***********************************************************************************
CTextCreatorLeafLitter::TextureQuery - Adds this object's textures (and it's sub-textures)
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
BOOL CTextCreatorLeafLitter::TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;

   
   // add itself
   plText->Add (pagThis);
   pTree->Add (szTemp, NULL, 0);

   // NEW CODE: go through all sub-textures
   m_Background.TextureQuery (m_dwRenderShard, plText, pTree);

   // leaves
   // cache leaves
   DWORD i;
   for (i = 0; i < LEAFLITTERVARIETIES; i++)
      if (m_afLeafUse[i])
         m_aLeaf[i].TextureQuery (m_dwRenderShard, plText, pTree);


   return TRUE;
}


/***********************************************************************************
CTextCreatorLeafLitter::SubTextureNoRecurse - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if they recurse
*/
BOOL CTextCreatorLeafLitter::SubTextureNoRecurse (PCListFixed plText, GUID *pagThis)
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
   fRet = m_Background.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;

   // leaves
   for (i = 0; i < LEAFLITTERVARIETIES; i++)
      if (m_afLeafUse[i]) {
         fRet = m_aLeaf[i].SubTextureNoRecurse (m_dwRenderShard, plText);
         if (!fRet)
            goto done;
      }


done:
   // restore list
   plText->Truncate (dwNum);

   return fRet;
}
