/*********************************************************************************
CNPREffectFilmGrain.cpp - Code for effect

begun 15/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


// EMTFILMGRAIN- Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImage;
   PCFImage       pFImage;
} EMTFILMGRAIN, *PEMTFILMGRAIN;




// FILMGRAINPAGE - Page info
typedef struct {
   PCNPREffectFilmGrain pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} FILMGRAINPAGE, *PFILMGRAINPAGE;



PWSTR gpszEffectFilmGrain = L"FilmGrain";



/*********************************************************************************
CNPREffectFilmGrain::Constructor and destructor
*/
CNPREffectFilmGrain::CNPREffectFilmGrain (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fNoise = 0.2;
}

CNPREffectFilmGrain::~CNPREffectFilmGrain (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectFilmGrain::Delete - From CNPREffect
*/
void CNPREffectFilmGrain::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectFilmGrain::QueryInfo - From CNPREffect
*/
void CNPREffectFilmGrain::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Adds noise to the image so it looks like it was filmed.";
   pqi->pszName = L"FilmGrain";
   pqi->pszID = gpszEffectFilmGrain;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszNoise = L"Noise";

/*********************************************************************************
CNPREffectFilmGrain::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectFilmGrain::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectFilmGrain);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszNoise, m_fNoise);

   return pNode;
}


/*********************************************************************************
CNPREffectFilmGrain::MMLFrom - From CNPREffect
*/
BOOL CNPREffectFilmGrain::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fNoise = MMLValueGetDouble (pNode, gpszNoise, 0);

   return TRUE;
}




/*********************************************************************************
CNPREffectFilmGrain::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectFilmGrain::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectFilmGrain::CloneEffect - From CNPREffect
*/
CNPREffectFilmGrain * CNPREffectFilmGrain::CloneEffect (void)
{
   PCNPREffectFilmGrain pNew = new CNPREffectFilmGrain (m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;
   pNew->m_fNoise = m_fNoise;

   return pNew;
}




/*********************************************************************************
CNPREffectFilmGrain::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectFilmGrain::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTFILMGRAIN pep = (PEMTFILMGRAIN)pParams;

   PCImage pImage = pep->pImage;
   PCFImage pFImage = pep->pFImage;


   srand (GetTickCount() + 1000 * dwThread);


   PIMAGEPIXEL pip = pImage ? pImage->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImage ? NULL : pFImage->Pixel(0,0);
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   DWORD dwMax= dwWidth * dwHeight;
   float af[3];
   float *paf = &af[0];
   float f;
   DWORD i, j;
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
         af[0] = pip->wRed;
         af[1] = pip->wGreen;
         af[2] = pip->wBlue;
      }
      else
         paf = &pfp->fRed;

      for (j = 0; j < 3; j++) {
         f = randf(-m_fNoise, m_fNoise);
         if (f < 0)
            f = 1.0 / (1.0 + f);
         else
            f += 1.0;

         paf[j] *= f;
      } // j

      // write it back
      if (pip) {
         for (j = 0; j < 3; j++) {
            f = paf[j];
            f = max(f, 0);
            f = min(f, (float)0xffff);
            (&pip->wRed)[j] = (WORD)f;
         } // j
      }
   } // i

}

/*********************************************************************************
CNPREffectFilmGrain::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImage - Image. If NULL then use pFImage
   PCFImage       pFImage - Floating point image. If NULL then use pImage
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectFilmGrain::RenderAny (PCImage pImage, PCFImage pFImage)
{
   DWORD dwWidth = pImage ? pImage->Width() : pFImage->Width();
   DWORD dwHeight = pImage ? pImage->Height() : pFImage->Height();

   // BUGFIX - multithreaded
   EMTFILMGRAIN em;
   memset (&em, 0, sizeof(em));
   em.pImage = pImage;
   em.pFImage = pFImage;
   ThreadLoop (0, dwWidth * dwHeight, 1, &em, sizeof(em));

   return TRUE;
}


/*********************************************************************************
CNPREffectFilmGrain::Render - From CNPREffect
*/
BOOL CNPREffectFilmGrain::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pDest, NULL);
}



/*********************************************************************************
CNPREffectFilmGrain::Render - From CNPREffect
*/
BOOL CNPREffectFilmGrain::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pDest);
}




/****************************************************************************
EffectFilmGrainPage
*/
BOOL EffectFilmGrainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PFILMGRAINPAGE pmp = (PFILMGRAINPAGE)pPage->m_pUserData;
   PCNPREffectFilmGrain pv = pmp->pe;

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
            p->pszSubString = L"Film grain";
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
CNPREffectFilmGrain::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectFilmGrain::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectFilmGrain::Dialog - From CNPREffect
*/
BOOL CNPREffectFilmGrain::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   FILMGRAINPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTFILMGRAIN, EffectFilmGrainPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

