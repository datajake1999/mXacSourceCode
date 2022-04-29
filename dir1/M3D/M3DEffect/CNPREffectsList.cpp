/*********************************************************************************
CNPREffectsLeft.cpp - Code for effects list

begun 7/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


// ELPAGE - Page info
typedef struct {
   PCNPREffectsList pel;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
} ELPAGE, *PELPAGE;


/*********************************************************************************
EffectImageToBitmap - Given an image, this clones it and applies an effect to
it. The bitmap is returned (the copy of the image deleted)

inputs
   PCImage           pImage - Image to copy and then apply the effect to
   PCNRPEffectsList  pEffectsList - If not NULL then this is applied to the image
   PCNPREffect       pEffect - if pEffectsList is NULL then this is applied.
   PCRenderSuper     pRender - Render engine useed
   PCWorldSocket     pWorld - World used
returns
   HBITMAP - Bitmap, or NULL if error
*/
HBITMAP EffectImageToBitmap (PCImage pImage, PCNPREffectsList pEffectsList,
                             PCNPREffect pEffect, PCRenderSuper pRender,
                             PCWorldSocket pWorld)
{
   PCImage pNew = pImage->Clone();
   if (!pNew)
      return NULL;

   if (pEffectsList)
      pEffectsList->Render (pNew, pRender, pWorld, TRUE, NULL);
   else {
      NPRQI qi;
      pEffect->QueryInfo (&qi);
      pEffect->Render (qi.fInPlace ? pNew : pImage, pNew, pRender, pWorld, TRUE, NULL);
   }

   HDC hDC = GetDC (GetDesktopWindow());
   HBITMAP hBit = pNew->ToBitmap (hDC);
   ReleaseDC (GetDesktopWindow(), hDC);
   delete pNew;

   return hBit;
}

/*********************************************************************************
CNPREffectsList::Constructor and destructor
*/
CNPREffectsList::CNPREffectsList (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_lPCNPREffect.Init (sizeof(PCNPREffect));
}

CNPREffectsList::~CNPREffectsList (void)
{
   // clear effects
   DWORD i;
   PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
   for (i = 0; i < m_lPCNPREffect.Num(); i++)
      ppe[i]->Delete(); // call Delete() instead of delete X;
   m_lPCNPREffect.Clear();
}



/*********************************************************************************
CNPREffectsList::Clone - Standard function
*/
CNPREffectsList *CNPREffectsList::Clone (void)
{
   PCNPREffectsList pNew = new CNPREffectsList(m_dwRenderShard);
   if (!pNew)
      return FALSE;


   // pClone->m_lPCNPREffect should already be empty

   DWORD i;
   PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
   PCNPREffect pe;
   pNew->m_lPCNPREffect.Required (m_lPCNPREffect.Num());
   for (i = 0; i < m_lPCNPREffect.Num(); i++) {
      pe = ppe[i]->Clone();
      if (pe)
         pNew->m_lPCNPREffect.Add (&pe);
   }

   return pNew;
}


static PWSTR gpszEffectsList = L"EffectsList";
static PWSTR gpszEffect = L"Effect";
static PWSTR gpszEffectName = L"EffectName";

/*********************************************************************************
CNPREffectsList::MMLTo - Standard API
*/
PCMMLNode2 CNPREffectsList::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectsList);

   DWORD i;
   PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
   for (i = 0; i < m_lPCNPREffect.Num(); i++) {
      PCMMLNode2 pSub = ppe[i]->MMLTo ();
      if (!pSub)
         continue;
      NPRQI qi;
      ppe[i]->QueryInfo (&qi);

      pSub->NameSet (gpszEffect);
      MMLValueSet (pSub, gpszEffectName, (PWSTR) qi.pszID);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/*********************************************************************************
CNPREffectsList::MMLTo - Standard API
*/
BOOL CNPREffectsList::MMLFrom (PCMMLNode2 pNode)
{
   // clear effects
   DWORD i;
   PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
   for (i = 0; i < m_lPCNPREffect.Num(); i++)
      ppe[i]->Delete(); // call Delete() instead of delete X;
   m_lPCNPREffect.Clear();

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp (psz, gpszEffect)) {
         psz = MMLValueGet (pSub, gpszEffectName);
         if (!psz)
            continue;   // shouldnt happen

         PCNPREffect pe;
         if (!_wcsicmp(psz, gpszEffectFog))
            pe = new CNPREffectFog(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectFogHLS))
            pe = new CNPREffectFogHLS(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectPosterize))
            pe = new CNPREffectPosterize(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectOutlineSketch))
            pe = new CNPREffectOutlineSketch(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectRelief))
            pe = new CNPREffectRelief(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectPainting))
            pe = new CNPREffectPainting(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectSpyglass))
            pe = new CNPREffectSpyglass(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectGlow))
            pe = new CNPREffectGlow(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectHalftone))
            pe = new CNPREffectHalftone(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectTV))
            pe = new CNPREffectTV(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectFilmGrain))
            pe = new CNPREffectFilmGrain(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectWatercolor))
            pe = new CNPREffectWatercolor(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectBlur))
            pe = new CNPREffectBlur(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectDOF))
            pe = new CNPREffectDOF(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectColor))
            pe = new CNPREffectColor(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectAutoExposure))
            pe = new CNPREffectAutoExposure(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectColorBlind))
            pe = new CNPREffectColorBlind(m_dwRenderShard);
         else if (!_wcsicmp(psz, gpszEffectOutline))
            pe = new CNPREffectOutline(m_dwRenderShard);
         if (!pe)
            continue;   // shouldnt happen

         if (!pe->MMLFrom (pSub)) {
            pe->Delete();
            continue;
         }
         m_lPCNPREffect.Add (&pe);
         continue;
      }

   } // i

   return TRUE;
}



/*********************************************************************************
CNPREffectsList::Render - This applies the effects to the given image.
The image is modified in place.

inputs
   PCImage        pImage - Image to be modified
   PCRenderSuper  pRender - Rendering engine used to produce the image. Can be NULL.
   PCWorldSocket  pWorld - World used to create the image. Can be NULL;
   BOOL           fFinal - TRUE if final render (better quality effects needed),
                           FALSE if in-prgress render
   PCRenderSocket pProgress - Progress bar. Can be NULL
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectsList::Render (PCImage pImage, PCRenderSuper pRender,
            PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
	MALLOCOPT_INIT;
   PCImage pTemp = NULL;
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   BOOL fRet = TRUE;
   DWORD dwCurImage = 0;   // indicates which image has the value data, 0=pImage, 1=pTemp

   DWORD i;
   PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
   NPRQI qi;
   for (i = 0; i < m_lPCNPREffect.Num(); i++) {
      if (pProgress)
         pProgress->Push ((fp)i / (fp)m_lPCNPREffect.Num(),
            (fp)(i+1) / (fp)m_lPCNPREffect.Num());

      PCNPREffect pe = ppe[i];
      PCImage pCurImage = dwCurImage ? pTemp : pImage;

      // see if overwrites same image
      pe->QueryInfo (&qi);

      if (qi.fInPlace) {
         // in-place rendering
         fRet = pe->Render (pCurImage, pCurImage, pRender, pWorld, fFinal, pProgress);
         if (!fRet) {
            if (pProgress)
               pProgress->Pop();
            break;
         }
      }
      else {
         // ping-pong
         PCImage pToImage;
         if (!pTemp) {
	         MALLOCOPT_OKTOMALLOC;
            pTemp = pImage->Clone();
	         MALLOCOPT_RESTORE;
            if (!pTemp)
               continue;
            pToImage = pTemp;
         }
         else {
            // copy over ping-pong
            pToImage = dwCurImage ? pImage : pTemp;
            memcpy (pToImage->Pixel(0,0), pCurImage->Pixel(0,0),
               pToImage->Width() * pToImage->Height() * sizeof(*pip));
         }

         // render
         fRet = pe->Render (pCurImage, pToImage, pRender, pWorld, fFinal, pProgress);
         if (!fRet) {
            if (pProgress)
               pProgress->Pop();
            break;
         }
         dwCurImage = !dwCurImage;
      } // ping-pong rendering

      if (pProgress) {
         pProgress->Update (1);
         pProgress->Pop();
      }

   } // i

   if (pTemp) {
      // if valid data is in pTemp then copy over
      if (dwCurImage)
         memcpy (pip, pTemp->Pixel(0,0),
            pImage->Width() * pImage->Height() * sizeof(*pip));
      delete pTemp;
   }
   return fRet;
}


/*********************************************************************************
CNPREffectsList::Render - This works just like the other render except it
uses a floating-point image.
*/
BOOL CNPREffectsList::Render (PCFImage pImage, PCRenderSuper pRender,
            PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
	MALLOCOPT_INIT;
   PCFImage pTemp = NULL;
   PFIMAGEPIXEL pip = pImage->Pixel(0,0);
   BOOL fRet = TRUE;
   DWORD dwCurImage = 0;   // indicates which image has the value data, 0=pImage, 1=pTemp

   DWORD i;
   PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
   NPRQI qi;
   for (i = 0; i < m_lPCNPREffect.Num(); i++) {
      if (pProgress)
         pProgress->Push ((fp)i / (fp)m_lPCNPREffect.Num(),
            (fp)(i+1) / (fp)m_lPCNPREffect.Num());

      PCNPREffect pe = ppe[i];
      PCFImage pCurImage = dwCurImage ? pTemp : pImage;

      // see if overwrites same image
      pe->QueryInfo (&qi);

      if (qi.fInPlace) {
         // in-place rendering
         fRet = pe->Render (pCurImage, pCurImage, pRender, pWorld, fFinal, pProgress);
         if (!fRet) {
            if (pProgress)
               pProgress->Pop();
            break;
         }
      }
      else {
         // ping-pong
         PCFImage pToImage;
         if (!pTemp) {
	         MALLOCOPT_OKTOMALLOC;
            pTemp = pImage->Clone();
	         MALLOCOPT_RESTORE;
            if (!pTemp)
               continue;
            pToImage = pTemp;
         }
         else {
            // copy over ping-pong
            pToImage = dwCurImage ? pImage : pTemp;
            memcpy (pToImage->Pixel(0,0), pCurImage->Pixel(0,0),
               pToImage->Width() * pToImage->Height() * sizeof(*pip));
         }

         // render
         fRet = pe->Render (pCurImage, pToImage, pRender, pWorld, fFinal, pProgress);
         if (!fRet) {
            if (pProgress)
               pProgress->Pop();
            break;
         }
         dwCurImage = !dwCurImage;
      } // ping-pong rendering

      if (pProgress) {
         pProgress->Update (1);
         pProgress->Pop();
      }

   } // i

   if (pTemp) {
      // if valid data is in pTemp then copy over
      if (dwCurImage)
         memcpy (pip, pTemp->Pixel(0,0),
            pImage->Width() * pImage->Height() * sizeof(*pip));
      delete pTemp;
   }
   return fRet;
}




/*********************************************************************************
EffectsListPage - UI
*/

BOOL EffectsListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PELPAGE pmp = (PELPAGE) pPage->m_pUserData;
   PCNPREffectsList pel = pmp->pel;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      // scroll to right position
      if (pmp->iVScroll > 0) {
         pPage->VScroll (pmp->iVScroll);

         // when bring up pop-up dialog often they're scrolled wrong because
         // iVScoll was left as valeu, and they use defpage
         pmp->iVScroll = 0;

         // BUGFIX - putting this invalidate in to hopefully fix a refresh
         // problem when add or move a task in the ProjectView page
         pPage->Invalidate();
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if ((psz[0] == L'l') && ((psz[1] == L'd') || (psz[1] == L'u')) && (psz[2] == L':')) {
            DWORD dwNum = _wtoi(psz+3);

            BOOL fUp = (psz[1] == L'u');
            DWORD dwSwap = dwNum;
            if (fUp && dwSwap)
               dwSwap--;
            else if (!fUp && (dwSwap+1 < pel->m_lPCNPREffect.Num()))
               dwSwap++;
            PCNPREffect *ppe = (PCNPREffect*)pel->m_lPCNPREffect.Get(0);
            PCNPREffect pTemp;
            pTemp = ppe[dwNum];
            ppe[dwNum] = ppe[dwSwap];
            ppe[dwSwap] = pTemp;

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'l') && (psz[1] == L'r') && (psz[2] == L':')) {
            DWORD dwNum = _wtoi(psz+3);

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove this effect?"))
               return TRUE;

            // delete it
            PCNPREffect *ppe = (PCNPREffect*)pel->m_lPCNPREffect.Get(0);
            ppe[dwNum]->Delete();
            pel->m_lPCNPREffect.Remove (dwNum);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify an effect";
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
         else if (!_wcsicmp(p->pszSubName, L"LIBS")) {
            MemZero (&gMemTemp);

            DWORD i;
            PCNPREffect *ppe = (PCNPREffect*)pel->m_lPCNPREffect.Get(0);
            NPRQI qi;
            for (i = 0; i < pel->m_lPCNPREffect.Num(); i++) {
               PCNPREffect pe = ppe[i];
               pe->QueryInfo (&qi);

               MemCat (&gMemTemp, L"<tr><td width=66%><xChoiceButton href=lib:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>");
               MemCatSanitize (&gMemTemp, (PWSTR) qi.pszName);
               MemCat (&gMemTemp, L"</bold><br/>");
               MemCatSanitize (&gMemTemp, (PWSTR) qi.pszDesc);
               MemCat (&gMemTemp, L"</xChoiceButton></td><td width=33% align=center>");
               MemCat (&gMemTemp, L"<button style=uptriangle margintopbottom=4 marginleftright=4 buttonheight=16 buttonwidth=16 name=lu:");
               MemCat (&gMemTemp, (int)i);
               if (!i)
                  MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><xhoverhelpshort>Move up</xhoverhelpshort></button>"
                  L"<button style=righttriangle margintopbottom=4 marginleftright=4 buttonheight=16 buttonwidth=16 color=#c02020 name=lr:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><xhoverhelpshort>Delete</xhoverhelpshort></button>"
                  L"<br/>"
                  L"<button style=downtriangle margintopbottom=4 marginleftright=4 buttonheight=16 buttonwidth=16 name=ld:");
               MemCat (&gMemTemp, (int)i);
               if (i+1 >= pel->m_lPCNPREffect.Num())
                  MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><xhoverhelpshort>Move down</xhoverhelpshort></button><br/>"
                  L"</td></tr>");

            } // i

            // if no libraries where added then say so
            if (!i)
               MemCat (&gMemTemp, L"<tr><td>The effect does not contain any sub-effects.</td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/*********************************************************************************
EffectsAddPage - UI
*/

BOOL EffectsAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PELPAGE pmp = (PELPAGE) pPage->m_pUserData;
   PCNPREffectsList pel = pmp->pel;

   switch (dwMessage) {


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add an effect";
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/*********************************************************************************
CNPREffectsList::IsPainterly - Returns true if any of the effects in the
list return IsPainterly()
*/
BOOL CNPREffectsList::IsPainterly (void)
{
   DWORD i;
   PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
   for (i = 0; i < m_lPCNPREffect.Num(); i++)
      if (ppe[i]->IsPainterly())
         return TRUE;
   return FALSE;
}


/*********************************************************************************
CNPREffectsList::Dialog - Brings up a dialog that lets the user add/remove
effects.

inputs
   PCEscWindow       pWindow - Window
   PCImage           pTest - Image that the sample is tested on
   PCRenderSuper     pRender - Engine that rendered test image
   PCWorldSocket     pWorld - World that came from
returns
   BOOL - TRUE if user pressed back, FALSE if cancel
*/
BOOL CNPREffectsList::Dialog (PCEscWindow pWindow, PCImage pTest,
                              PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   PWSTR pszLib = L"lib:";
   DWORD dwLibLen = (DWORD)wcslen(pszLib);
   ELPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pel = this;


redo:

   // delete existing
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = EffectImageToBitmap (pTest, this, NULL, pRender, pWorld);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTSLIST, EffectsListPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   mp.iVScroll = pWindow->m_iExitVScroll;

   if (!pszRet)
      return FALSE;

   if (!wcsncmp(pszRet, pszLib, dwLibLen)) {
      DWORD dwNum = _wtoi(pszRet + dwLibLen);
      PCNPREffect *ppe = (PCNPREffect*)m_lPCNPREffect.Get(0);
      BOOL fRet = ppe[dwNum]->Dialog (pWindow, this, pTest, pRender, pWorld);
      if (fRet)
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"addeffect")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTSADD, EffectsAddPage, &mp);
      if (!pszRet)
         return FALSE;
      if (!_wcsicmp(pszRet, Back()))
         goto redo;

      PCNPREffect pNew = NULL;
      if (!_wcsicmp (pszRet, L"XXXfog"))
         pNew = new CNPREffectFog(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXfoghls"))
         pNew = new CNPREffectFogHLS(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXfilmgrain"))
         pNew = new CNPREffectFilmGrain(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXwatercolor"))
         pNew = new CNPREffectWatercolor(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXposterize"))
         pNew = new CNPREffectPosterize(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXoutlinesketch"))
         pNew = new CNPREffectOutlineSketch(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXrelief"))
         pNew = new CNPREffectRelief(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXpainting"))
         pNew = new CNPREffectPainting(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXspyglass"))
         pNew = new CNPREffectSpyglass(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXglow"))
         pNew = new CNPREffectGlow(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXhalftone"))
         pNew = new CNPREffectHalftone(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXtv"))
         pNew = new CNPREffectTV(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXblur"))
         pNew = new CNPREffectBlur(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXdof"))
         pNew = new CNPREffectDOF(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXcolor"))
         pNew = new CNPREffectColor(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXautoexposure"))
         pNew = new CNPREffectAutoExposure(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXcolorblind"))
         pNew = new CNPREffectColorBlind(m_dwRenderShard);
      else if (!_wcsicmp (pszRet, L"XXXoutline"))
         pNew = new CNPREffectOutline(m_dwRenderShard);
      if (!pNew)
         goto redo;

      // add it
      m_lPCNPREffect.Add (&pNew);

      // edit
      BOOL fRet = pNew->Dialog (pWindow, this, pTest, pRender, pWorld);

      if (fRet)
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   return FALSE;
}


