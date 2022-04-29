/*********************************************************************************
CNPREffectGlow.cpp - Code for effect

begun 15/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


// PEMTGLOW - Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImageSrc;
   PCImage        pImageDest;
   PCFImage       pFImageSrc;
   PCFImage       pFImageDest;
   PCBlurBuf      pbb;
} EMTGLOW, *PEMTGLOW;

// GLOWPAGE - Page info
typedef struct {
   PCNPREffectGlow pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} GLOWPAGE, *PGLOWPAGE;



PWSTR gpszEffectGlow = L"Glow";



/*********************************************************************************
CNPREffectGlow::Constructor and destructor
*/
CNPREffectGlow::CNPREffectGlow (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fGlowWidth = 1;
   m_dwMode = 0;
}

CNPREffectGlow::~CNPREffectGlow (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectGlow::Delete - From CNPREffect
*/
void CNPREffectGlow::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectGlow::QueryInfo - From CNPREffect
*/
void CNPREffectGlow::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = FALSE;
   pqi->pszDesc = L"Use this to create a glowing image, or to sharpen edges.";
   pqi->pszName = L"Glow";
   pqi->pszID = gpszEffectGlow;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszGlowWidth = L"GlowWidth";
static PWSTR gpszMode = L"Mode";

/*********************************************************************************
CNPREffectGlow::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectGlow::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectGlow);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszGlowWidth, m_fGlowWidth);
   MMLValueSet (pNode, gpszMode, (int) m_dwMode);

   return pNode;
}


/*********************************************************************************
CNPREffectGlow::MMLFrom - From CNPREffect
*/
BOOL CNPREffectGlow::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fGlowWidth = MMLValueGetDouble (pNode, gpszGlowWidth, 1);
   m_dwMode = (DWORD) MMLValueGetInt (pNode, gpszMode, 0);

   return TRUE;
}




/*********************************************************************************
CNPREffectGlow::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectGlow::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectGlow::CloneEffect - From CNPREffect
*/
CNPREffectGlow * CNPREffectGlow::CloneEffect (void)
{
   PCNPREffectGlow pNew = new CNPREffectGlow(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_fGlowWidth = m_fGlowWidth;
   pNew->m_dwMode =m_dwMode;
   return pNew;
}






/*********************************************************************************
CNPREffectGlow::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectGlow::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTGLOW pep = (PEMTGLOW)pParams;

   PCImage pImageSrc = pep->pImageSrc;
   PCImage pImageDest = pep->pImageDest;
   PCFImage pFImageSrc = pep->pFImageSrc;
   PCFImage pFImageDest = pep->pFImageDest;
   PCBlurBuf pbb = pep->pbb;

   PIMAGEPIXEL pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();

   if (pip)
      pip += pep->dwStart * dwWidth;
   if (pfp)
      pfp += pep->dwStart * dwWidth;

   DWORD x,y, j;
   for (y = pep->dwStart; y < pep->dwEnd; y++) {

      for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         if (m_fIgnoreBackground) {
            WORD w = HIWORD(pip ? pip->dwID : pfp->dwID);
            if (!w)
               continue;   // ignore this
         }

         if (pip) {
            WORD aw[3];

            pbb->Blur (pImageSrc, (int)x, (int)y, m_fIgnoreBackground, NULL,
               0, &aw[0]);

            switch (m_dwMode) {
            default:
            case 0:  // glow
               for (j = 0; j < 3; j++)
                  (&pip->wRed)[j] = max((&pip->wRed)[j], aw[j]) - min((&pip->wRed)[j], aw[j]);
               break;

            case 1:  // sharpen
               for (j = 0; j < 3; j++) {
                  int iVal = 2* (int)(&pip->wRed)[j] - (int)aw[j];
                  iVal = max(iVal, 0);
                  iVal = min(iVal, 0xffff);
                  (&pip->wRed)[j] = (WORD)iVal;
               }
               break;

            case 2:  // photocopy
               {
                  WORD wOrig = pip->wRed / 10 * 3 + pip->wGreen / 2 + pip->wBlue / 10 * 2;
                  WORD wBlur = aw[0] / 10 * 3 + aw[1] / 2 + aw[2] / 10 * 2;

                  wBlur = max(wBlur, wOrig) - min(wBlur, wOrig);
                  wOrig = 0xffff - wOrig;
                  wOrig = (WORD)(((DWORD)wOrig * (DWORD)wBlur) >> 16);
                  // wMax = max(wMax, wOrig);

                  pip->wRed = pip->wGreen = pip->wBlue = wOrig;
               }
               break;
            }
         }
         else {
            float af[3];

            pbb->Blur (pFImageSrc, (int)x, (int)y, m_fIgnoreBackground, NULL,
               &af[0]);

            switch (m_dwMode) {
            default:
            case 0:  // glow
               for (j = 0; j < 3; j++)
                  (&pfp->fRed)[j] = fabs((&pfp->fRed)[j] - af[j]);
               break;

            case 1:  // shapren
               for (j = 0; j < 3; j++) {
                  (&pfp->fRed)[j] = 2*(&pfp->fRed)[j] - af[j];
                  (&pfp->fRed)[j] = max((&pfp->fRed)[j], 0);
               }
               break;

            case 2:  // photocopy
               {
                  fp fOrig = pfp->fRed *.3 + pfp->fGreen *.5 + pfp->fBlue *.2;
                  fp fBlur = af[0] *.3 + af[1] *.5 + af[2] *.2;

                  fBlur = max(fBlur, fOrig) - min(fBlur, fOrig);
                  fOrig = (fp)0xffff - fOrig;
                  fOrig = (fOrig * fBlur) / (fp)0x10000;
                  // fMax = max(fMax, fOrig);

                  pfp->fRed = pfp->fGreen = pfp->fBlue = fOrig;
               }
               break;
            } // switch
         }
      } // x
   } // y

   
}

/*********************************************************************************
CNPREffectGlow::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImageSrc - Image. If NULL then use pFImage
   PCFImage       pFImageSrc - Floating point image. If NULL then use pImage
   PCImage        pImageDest - Image. If NULL then use pFImage
   PCFImage       pFImageDest - Floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectGlow::RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                PCImage pImageDest, PCFImage pFImageDest,
                                PCProgressSocket pProgress)
{
   CBlurBuf bb;
   PIMAGEPIXEL pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   if (!bb.Init (m_fGlowWidth / 100.0 * (fp)dwWidth, 2))
      return FALSE;

   DWORD x, y, j;

   // BUGFIX - Multithreaded
   EMTGLOW em;
   memset (&em, 0, sizeof(em));
   em.pbb = &bb;
   em.pFImageDest = pFImageDest;
   em.pFImageSrc = pFImageSrc;
   em.pImageDest = pImageDest;
   em.pImageSrc = pImageSrc;
   em.pbb = &bb;
   ThreadLoop (0, dwHeight, 4, &em, sizeof(em), pProgress);


   // if it's photocopy then go through and enhance to maximum
   if (m_dwMode == 2) {
      WORD wMax = 0;
      float fMax = 0;
      pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
      pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
      for (y = 0; y < dwHeight; y++) {
         for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
            if (m_fIgnoreBackground) {
               WORD w = HIWORD(pip ? pip->dwID : pfp->dwID);
               if (!w)
                  continue;   // ignore this
            }

            if (pip)
               wMax = max(wMax, pip->wRed);
            else
               fMax = max(fMax, pfp->fRed);

         } // x
      } // y

      pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
      pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);

      wMax = max(wMax, 1);
      if (pfp && fMax)
         fMax = (float)0xffff / fMax;

      for (y = 0; y < dwHeight; y++) {
         for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
            if (m_fIgnoreBackground) {
               WORD w = HIWORD(pip ? pip->dwID : pfp->dwID);
               if (!w)
                  continue;   // ignore this
            }

            if (pip)
               for (j = 0; j < 3; j++)
                  (&pip->wRed)[j] = 0xffff - (WORD)((DWORD)(&pip->wRed)[j] * 0xffff / (DWORD)wMax);
            else
               for (j = 0; j < 3; j++)
                  (&pfp->fRed)[j] = (float)0xffff - ((&pfp->fRed)[j] * fMax);
         } // x
      } // y

   } // if mode is photocopy
   return TRUE;
}


/*********************************************************************************
CNPREffectGlow::Render - From CNPREffect
*/
BOOL CNPREffectGlow::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pOrig, NULL, pDest, NULL, pProgress);
}



/*********************************************************************************
CNPREffectGlow::Render - From CNPREffect
*/
BOOL CNPREffectGlow::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pOrig, NULL, pDest, pProgress);
}




/****************************************************************************
EffectGlowPage
*/
BOOL EffectGlowPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PGLOWPAGE pmp = (PGLOWPAGE)pPage->m_pUserData;
   PCNPREffectGlow pv = pmp->pe;

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

         DoubleToControl (pPage, L"glowwidth", pv->m_fGlowWidth);
         ComboBoxSet (pPage, L"mode", pv->m_dwMode);
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
         }
      }
      break;   // default


   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"mode")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwMode)
               return TRUE;   // no change

            pv->m_dwMode = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // just get all values
         pv->m_fGlowWidth = DoubleFromControl (pPage, L"Glowwidth");

         pPage->Message (ESCM_USER+189);  // update bitmap
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
            p->pszSubString = L"Glow";
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
CNPREffectGlow::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectGlow::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectGlow::Dialog - From CNPREffect
*/
BOOL CNPREffectGlow::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   GLOWPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTGLOW, EffectGlowPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}




