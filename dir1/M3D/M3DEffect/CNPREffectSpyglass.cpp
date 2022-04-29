/*********************************************************************************
CNPREffectSpyglass.cpp - Code for effect

begun 16/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"





// SPYGLASSPAGE - Page info
typedef struct {
   PCNPREffectSpyglass pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} SPYGLASSPAGE, *PSPYGLASSPAGE;



PWSTR gpszEffectSpyglass = L"Spyglass";

/*********************************************************************************
CNPREffectSpyglass::Constructor and destructor
*/
CNPREffectSpyglass::CNPREffectSpyglass (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fScale = 1;
   m_fBlur = .1;
   m_fStickOut = 0;
   m_dwShape = 0;
   m_cSpyglassColor = RGB(0,0,0);
}

CNPREffectSpyglass::~CNPREffectSpyglass (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectSpyglass::Delete - From CNPREffect
*/
void CNPREffectSpyglass::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectSpyglass::QueryInfo - From CNPREffect
*/
void CNPREffectSpyglass::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Creates a circular mask so the image looks like it is seen "
      L"through a spyglass or binoculars.";
   pqi->pszName = L"Spyglass";
   pqi->pszID = gpszEffectSpyglass;
}



static PWSTR gpszSpyglassColor = L"SpyglassColor";
static PWSTR gpszScale = L"Scale";
static PWSTR gpszStickOut = L"StickOut";
static PWSTR gpszBlur = L"Blur";
static PWSTR gpszShape = L"Shape";

/*********************************************************************************
CNPREffectSpyglass::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectSpyglass::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectSpyglass);

   MMLValueSet (pNode, gpszSpyglassColor, (int)m_cSpyglassColor);

   MMLValueSet (pNode, gpszScale, m_fScale);
   MMLValueSet (pNode, gpszBlur, m_fBlur);
   MMLValueSet (pNode, gpszStickOut, m_fStickOut);
   MMLValueSet (pNode, gpszShape, (int)m_dwShape);
   MMLValueSet (pNode, gpszSpyglassColor, (int)m_cSpyglassColor);

   return pNode;
}


/*********************************************************************************
CNPREffectSpyglass::MMLFrom - From CNPREffect
*/
BOOL CNPREffectSpyglass::MMLFrom (PCMMLNode2 pNode)
{
   m_cSpyglassColor = (COLORREF) MMLValueGetInt (pNode, gpszSpyglassColor, RGB(0xff,0xff,0xff));

   m_fScale = MMLValueGetDouble (pNode, gpszScale, 1);
   m_fBlur = MMLValueGetDouble (pNode, gpszBlur, 0);
   m_fStickOut = MMLValueGetDouble (pNode, gpszStickOut, 0);
   m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int)0);
   m_cSpyglassColor = (COLORREF) MMLValueGetInt (pNode, gpszSpyglassColor, (int)0);



   return TRUE;
}




/*********************************************************************************
CNPREffectSpyglass::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectSpyglass::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectSpyglass::MMLFrom - From CNPREffect
*/
CNPREffectSpyglass * CNPREffectSpyglass::CloneEffect (void)
{
   PCNPREffectSpyglass pNew = new CNPREffectSpyglass(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fScale = m_fScale;
   pNew->m_fBlur = m_fBlur;
   pNew->m_fStickOut = m_fStickOut;
   pNew->m_dwShape = m_dwShape;
   pNew->m_cSpyglassColor = m_cSpyglassColor;

   return pNew;
}




/*********************************************************************************
CNPREffectSpyglass::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectSpyglass::RenderAny (PCImage pImage, PCFImage pFImage)
{
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   // determine circle height/width
   DWORD dwMin = min(dwWidth, dwHeight);
   DWORD dwCircleWidth = dwMin;
   DWORD dwCircleHeight = dwMin;
   if (m_dwShape == 1) {
      // elliptical
      dwCircleWidth = dwWidth;
      dwCircleHeight = dwHeight;
   }
   dwCircleWidth = (DWORD)((fp)dwCircleWidth * m_fScale / 2.0);   // radius
   dwCircleHeight = (DWORD)((fp)dwCircleHeight * m_fScale / 2.0);
   dwCircleWidth = max(dwCircleWidth, 1);
   dwCircleHeight = max(dwCircleHeight, 1);

   // center...
   TEXTUREPOINT afCenter[2];
   afCenter[0].h = (int)dwWidth/2;
   afCenter[0].v = (int)dwHeight/2;
   if (m_dwShape == 2) {
      // binoculars
      afCenter[1] = afCenter[0];
      afCenter[0].h = (fp)dwMin/2;
      afCenter[1].h = (fp)dwWidth - (fp)dwMin/2;
   }

   DWORD x,y, j;

   fp fScaleX = 1.0 / (fp)dwCircleWidth;
   fp fScaleY = 1.0 / (fp)dwCircleHeight;

   // color of background
   WORD awBack[3];
   Gamma (m_cSpyglassColor, awBack);

   // blur square
   m_fBlur = max(m_fBlur, 0.00001);
   fp fBlurSquare = (1.0 - m_fBlur) * (1.0 - m_fBlur);

   for (y = 0; y < dwHeight; y++) {
      for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         // if out too close then dont affect
         if (pip) {
            if (pip->fZ < m_fStickOut)
               continue;
         }
         else {
            if (pfp->fZ < m_fStickOut)
               continue;
         }

         // distance from the center...
         fp fX = ((fp)x - afCenter[0].h) * fScaleX;
         fp fY = ((fp)y - afCenter[0].v) * fScaleY;
         fp fDist = fX * fX + fY * fY;
         if (m_dwShape == 2) {
            // binoculars
            fX = ((fp)x - afCenter[1].h) * fScaleX;
            fY = ((fp)y - afCenter[1].v) * fScaleY;
            fp fDist2 = fX * fX + fY * fY;
            fDist = min(fDist, fDist2);
         }

         // if closer than blue then all visible
         if (fDist <= fBlurSquare)
            continue;

         // if further away then all black
         if (fDist >= 1) {
            if (pip)
               memcpy (&pip->wRed, awBack, sizeof(awBack));
            else {
               pfp->fRed = awBack[0];
               pfp->fGreen = awBack[1];
               pfp->fBlue = awBack[2];
            }
            continue;
         }

         // else, blend
         // note: Dealing with square space so get rid of mach band
         fDist -= fBlurSquare;
         fDist /= (1.0 - fBlurSquare);
         //fDist = sqrt(fDist);
         //fDist -= (1.0 - m_fBlur);
         //fDist /= m_fBlur;
         fDist = min(fDist, 1);

         if (pip) {
            WORD wScale = (WORD)(fDist * (fp)0xffff);
            WORD wScaleInv = 0xffff - wScale;
            for (j = 0; j < 3; j++)
               (&pip->wRed)[j] = (WORD)(((DWORD)((&pip->wRed)[j]) * (DWORD)wScaleInv +
                  (DWORD)awBack[j] * (DWORD)wScale) >> 16);
         }
         else {
            fp fScaleInv = 1.0 - fDist;
            for (j = 0; j < 3; j++)
               (&pfp->fRed)[j] = (float)((&pfp->fRed)[j] * fScaleInv +
                  (fp)awBack[j] * fDist);
         }
      } // x
   } // y

   return TRUE;
}




/*********************************************************************************
CNPREffectSpyglass::Render - From CNPREffect
*/
BOOL CNPREffectSpyglass::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL);
}



/*********************************************************************************
CNPREffectSpyglass::Render - From CNPREffect
*/
BOOL CNPREffectSpyglass::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest);
}




/****************************************************************************
EffectSpyglassPage
*/
BOOL EffectSpyglassPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSPYGLASSPAGE pmp = (PSPYGLASSPAGE)pPage->m_pUserData;
   PCNPREffectSpyglass pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         FillStatusColor (pPage, L"color", pv->m_cSpyglassColor);

         ComboBoxSet (pPage, L"shape", pv->m_dwShape);
         MeasureToString (pPage, L"stickout", pv->m_fStickOut);

         // scrollbars
         pControl = pPage->ControlFind (L"scale");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fScale * 100));
         pControl = pPage->ControlFind (L"blur");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBlur * 100));
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE)pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"shape")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwShape)
               return TRUE;   // no change

            pv->m_dwShape = dwVal;
            pPage->Message (ESCM_USER+189);  // update bitmap

            return TRUE;
         } // pattern
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

         if (!_wcsicmp (psz, L"scale"))
            pf = &pv->m_fScale;
         else if (!_wcsicmp (psz, L"blur"))
            pf = &pv->m_fBlur;
         if (!pf)
            break;   // not known

         *pf = (fp)p->iPos / 100.0;
         pPage->Message (ESCM_USER+189);  // update bitmap
         return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         // if any edit changed then just get values
         MeasureParseString (pPage, L"stickout", &pv->m_fStickOut);

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
            pv->m_cSpyglassColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cSpyglassColor, pPage, L"color");
            pPage->Message (ESCM_USER+189);  // update bitmap
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
            p->pszSubString = L"Spyglass";
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
CNPREffectSpyglass::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectSpyglass::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectSpyglass::Dialog - From CNPREffect
*/
BOOL CNPREffectSpyglass::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   SPYGLASSPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTSPYGLASS, EffectSpyglassPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

