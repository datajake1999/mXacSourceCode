/*********************************************************************************
CNPREffectTV.cpp - Code for effect

begun 15/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"





// TVPAGE - Page info
typedef struct {
   PCNPREffectTV pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} TVPAGE, *PTVPAGE;



PWSTR gpszEffectTV = L"TV";



/*********************************************************************************
CNPREffectTV::Constructor and destructor
*/
CNPREffectTV::CNPREffectTV (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fNoise = 0.2;
}

CNPREffectTV::~CNPREffectTV (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectTV::Delete - From CNPREffect
*/
void CNPREffectTV::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectTV::QueryInfo - From CNPREffect
*/
void CNPREffectTV::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Modifies the image so it looks like it came from a color television.";
   pqi->pszName = L"TV";
   pqi->pszID = gpszEffectTV;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszNoise = L"Noise";

/*********************************************************************************
CNPREffectTV::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectTV::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectTV);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszNoise, m_fNoise);

   return pNode;
}


/*********************************************************************************
CNPREffectTV::MMLFrom - From CNPREffect
*/
BOOL CNPREffectTV::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fNoise = MMLValueGetDouble (pNode, gpszNoise, 0);

   return TRUE;
}




/*********************************************************************************
CNPREffectTV::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectTV::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectTV::CloneEffect - From CNPREffect
*/
CNPREffectTV * CNPREffectTV::CloneEffect (void)
{
   PCNPREffectTV pNew = new CNPREffectTV(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_fNoise = m_fNoise;

   return pNew;
}




/*********************************************************************************
CNPREffectTV::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectTV::RenderAny (PCImage pImage, PCFImage pFImage)
{
   srand (GetTickCount());  // just pick a seed

   PCNoise2D pNoise = NULL;
   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   CMem memBlur;
   DWORD dwNeed = sizeof(float)*dwWidth * 3;
   if (!memBlur.Required (dwNeed * 2))
      return FALSE;
   float *pafBlur = (float*)memBlur.p;
   float *pafNoise = pafBlur + (dwWidth * 3);

   DWORD dwMax= dwWidth * dwHeight;
   float af[3];
   float *paf = &af[0];
   float *pafCurNoise;
   float f;
   DWORD x, y, j;
   for (y = 0; y < dwHeight; y++) {
      PIMAGEPIXEL pipStart = pip;
      PFIMAGEPIXEL pfpStart = pfp;
      paf = pafBlur;
      memset (pafBlur, 0, dwNeed);

#define NOISEX          100         // resoultion in x
      if (!(y%4)) {
         // generate new noise
         if (pNoise)
            delete pNoise;
         pNoise = new CNoise2D;
         pNoise->Init (NOISEX, 3);

         memset (pafNoise, 0, dwNeed);
         pafCurNoise = pafNoise;
         for (x = 0; x < dwWidth; x++, pafCurNoise+=3) {
            for (j = 0; j < 3; j++) {
               f = pNoise->Value ((fp)x / (fp)dwWidth, (fp)j / 3.0) +
                  pNoise->Value ((fp)x * 4.0 / (fp)dwWidth, (fp)j / 3.0) / 4.0;
               f *= m_fNoise;

               pafCurNoise[j] = f;  // since RGB noise has less effect than white
            } // j, colors
         } // x
      } // if new noise

      pafCurNoise = pafNoise;
      for (x = 0; x < dwWidth; x++, paf += 3, pafCurNoise+=3, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         if (pip) {
            paf[0] = pip->wRed;
            paf[1] = pip->wGreen;
            paf[2] = pip->wBlue;
         }
         else {
            paf[0] = pfp->fRed;
            paf[1] = pfp->fGreen;
            paf[2] = pfp->fBlue;
         }

         j = x % 4;
         paf[0] = (j == 0) ? (paf[0] * 3) : 0;
         paf[1] = (j == 1) ? (paf[1] * 3) : 0;
         paf[2] = (j == 2) ? (paf[2] * 3) : 0;


         if (j < 3) {
            paf[j] += pafCurNoise[j] * (fp)0xffff;
            paf[j] = max(paf[j], 0);
         } // j
      } // x

      // write it back
      pip = pipStart;
      pfp = pfpStart;
      paf = pafBlur;
      for (x = 0; x < dwWidth; x++, paf += 3, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         if (m_fIgnoreBackground) {
            WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
            if (!w)
               continue;
         }

         for (j = 0; j < 3; j++)
            af[j] = paf[j] * 2.0 / 3.0 +
               (x ? paf[(int)j-3] / 6.0 : 0) +   // some from left
               ((x+1 < dwWidth) ? paf[j+3] / 6.0 : 0);

         // write it back
         if (pip) {
            for (j = 0; j < 3; j++) {
               f = af[j];
               // f = max(f, 0); - max used elsewhere
               f = min(f, (float)0xffff);
               (&pip->wRed)[j] = (WORD)f;
            } // j
         }
         else {
            pfp->fRed = af[0];
            pfp->fGreen = af[1];
            pfp->fBlue = af[2];
         }

      } // x
   } // y

   if (pNoise)
      delete pNoise;

   return TRUE;
}


/*********************************************************************************
CNPREffectTV::Render - From CNPREffect
*/
BOOL CNPREffectTV::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL);
}



/*********************************************************************************
CNPREffectTV::Render - From CNPREffect
*/
BOOL CNPREffectTV::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest);
}




/****************************************************************************
EffectTVPage
*/
BOOL EffectTVPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTVPAGE pmp = (PTVPAGE)pPage->m_pUserData;
   PCNPREffectTV pv = pmp->pe;

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

         // scrollbars
         pControl = pPage->ControlFind (L"noise");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoise * 100));
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

   case ESCN_SCROLL:
   //case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         fp *pf = NULL;

         if (!_wcsicmp (psz, L"noise"))
            pf = &pv->m_fNoise;
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
            p->pszSubString = L"TV";
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
CNPREffectTV::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectTV::IsPainterly (void)
{
   return TRUE;
}

/*********************************************************************************
CNPREffectTV::Dialog - From CNPREffect
*/
BOOL CNPREffectTV::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   TVPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTTV, EffectTVPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

