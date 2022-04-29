/*********************************************************************************
CNPREffectRelief.cpp - Code for effect

begun 17/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"



// EMTEFFECTRELIEF - Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImageSrc;
   PCImage        pImageDest;
   PCFImage       pFImageSrc;
   PCFImage       pFImageDest;
   PCTextureMapSocket pTexture;
} EMTEFFECTRELIEF, *PEMTEFFECTRELIEF;



// RELIEFPAGE - Page info
typedef struct {
   PCNPREffectRelief pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} RELIEFPAGE, *PRELIEFPAGE;



PWSTR gpszEffectRelief = L"Relief";



/*********************************************************************************
CNPREffectRelief::Constructor and destructor
*/
CNPREffectRelief::CNPREffectRelief (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_fIgnoreBackground = TRUE;

   m_fWidth = 1.0;
   m_fDirection = -PI/4;
   m_fIncidence = PI/8;
   m_fAmbient = 0;
   m_fSpec = 0.2;
   m_fTransDist = 0.01;
   m_fTransparent = FALSE;

   m_pSurf = new CObjectSurface;
   DefaultSurfFromNewTexture (m_dwRenderShard, (GUID*)&GTEXTURECODE_SidingStucco, (GUID*)&GTEXTURESUB_SidingStucco, m_pSurf);
}

CNPREffectRelief::~CNPREffectRelief (void)
{
   if (m_pSurf)
      delete m_pSurf;
}


/*********************************************************************************
CNPREffectRelief::Delete - From CNPREffect
*/
void CNPREffectRelief::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectRelief::QueryInfo - From CNPREffect
*/
void CNPREffectRelief::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = FALSE;
   pqi->pszDesc = L"Makes the image look like it was painted/drawn on a rough surface.";
   pqi->pszName = L"Relief";
   pqi->pszID = gpszEffectRelief;
}



static PWSTR gpszIgnoreBackground = L"IgnoreBackground";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszDirection = L"Direction";
static PWSTR gpszIncidence = L"Incidence";
static PWSTR gpszAmbient = L"Ambient";
static PWSTR gpszSurf = L"Surf";
static PWSTR gpszSpec = L"Spec";
static PWSTR gpszTransDist = L"TransDist";
static PWSTR gpszTransparent = L"Transparent";

/*********************************************************************************
CNPREffectRelief::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectRelief::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectRelief);

   MMLValueSet (pNode, gpszIgnoreBackground, (int)m_fIgnoreBackground);

   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszDirection, m_fDirection);
   MMLValueSet (pNode, gpszIncidence, m_fIncidence);
   MMLValueSet (pNode, gpszAmbient, m_fAmbient);
   MMLValueSet (pNode, gpszSpec, m_fSpec);
   MMLValueSet (pNode, gpszTransDist, m_fTransDist);
   MMLValueSet (pNode, gpszTransparent, (int)m_fTransparent);

   PCMMLNode2 pSub = m_pSurf->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszSurf);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/*********************************************************************************
CNPREffectRelief::MMLFrom - From CNPREffect
*/
BOOL CNPREffectRelief::MMLFrom (PCMMLNode2 pNode)
{
   m_fIgnoreBackground = (BOOL) MMLValueGetInt (pNode, gpszIgnoreBackground, (int)TRUE);

   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, 1);
   m_fDirection = MMLValueGetDouble (pNode, gpszDirection, 0);
   m_fIncidence = MMLValueGetDouble (pNode, gpszIncidence, 1);
   m_fAmbient = MMLValueGetDouble (pNode, gpszAmbient, 0);
   m_fSpec = MMLValueGetDouble (pNode, gpszSpec, 0);
   m_fTransDist = MMLValueGetDouble (pNode, gpszTransDist, 0.01);
   m_fTransparent = (BOOL) MMLValueGetInt (pNode, gpszTransparent, (int)0);

   PCMMLNode2 pSub = NULL;
   PWSTR psz;
   pNode->ContentEnum (pNode->ContentFind (gpszSurf), &psz, &pSub);
   if (pSub) {
      if (m_pSurf)
         delete m_pSurf;
      m_pSurf = new CObjectSurface;
      m_pSurf->MMLFrom (pSub);
   }

   return TRUE;
}




/*********************************************************************************
CNPREffectRelief::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectRelief::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectRelief::CloneEffect - From CNPREffect
*/
CNPREffectRelief * CNPREffectRelief::CloneEffect (void)
{
   PCNPREffectRelief pNew = new CNPREffectRelief(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fIgnoreBackground = m_fIgnoreBackground;

   pNew->m_fWidth = m_fWidth;
   pNew->m_fDirection = m_fDirection;
   pNew->m_fIncidence = m_fIncidence;
   pNew->m_fAmbient = m_fAmbient;
   pNew->m_fSpec = m_fSpec;
   pNew->m_fTransDist = m_fTransDist;
   pNew->m_fTransparent = m_fTransparent;

   if (pNew->m_pSurf)
      delete pNew->m_pSurf;
   pNew->m_pSurf = m_pSurf->Clone();


   return pNew;
}




/*********************************************************************************
CNPREffectRelief::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectRelief::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectRelief::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectRelief::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTEFFECTRELIEF pep = (PEMTEFFECTRELIEF)pParams;

   PCImage pImageSrc = pep->pImageSrc;
   PCImage pImageDest = pep->pImageDest;
   PCFImage pFImageSrc = pep->pFImageSrc;
   PCFImage pFImageDest = pep->pFImageDest;
   PCTextureMapSocket pTexture = pep->pTexture;
   
   BOOL f360 = pImageDest ? pImageDest->m_f360 : pFImageDest->m_f360;
   PIMAGEPIXEL pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   fp fWidthPerPixel = m_fWidth / (fp)dwWidth;

   // see delta for right...
   TEXTPOINT5 tpRightDelta, tpDownDelta;
   TEXTUREPOINT tp;
   DWORD i;
   CPoint pZero, p;
   tp.h = fWidthPerPixel;
   tp.v = 0;
   TextureMatrixMultiply2D (m_pSurf->m_afTextureMatrix, &tp);
   tpRightDelta.hv[0] = tp.h;
   tpRightDelta.hv[1] = tp.v;
   tp.h = 0;
   tp.v = fWidthPerPixel;
   TextureMatrixMultiply2D (m_pSurf->m_afTextureMatrix, &tp);
   tpDownDelta.hv[0] = tp.h;
   tpDownDelta.hv[1] = tp.v;
   pZero.Zero();
   p.Zero();
   p.p[0] = fWidthPerPixel;
   p.MultiplyLeft (&m_pSurf->m_mTextureMatrix);
   pZero.MultiplyLeft  (&m_pSurf->m_mTextureMatrix);
   p.Subtract (&pZero);
   for (i = 0; i < 3; i++)
      tpRightDelta.xyz[i] = p.p[i];
   p.Zero();
   p.p[1] = -fWidthPerPixel;
   p.MultiplyLeft (&m_pSurf->m_mTextureMatrix);
   p.Subtract (&pZero);
   for (i = 0; i < 3; i++)
      tpDownDelta.xyz[i] = p.p[i];

   // figure out light vector
   CPoint pL;
   pL.Zero();
   pL.p[0] = sin(m_fDirection) * cos(m_fIncidence);
   pL.p[2] = cos(m_fDirection) * cos(m_fIncidence);
   pL.p[1] = -sin(m_fIncidence);

   CPoint pEye;
   pEye.Zero();
   pEye.p[1] = -1;  // eye
   m_fTransDist = max(m_fTransDist, 0.0001);

   float af[3];
   float *paf = &af[0];
   float f;
   DWORD x, y, j;
   TEXTPOINT5 tpCurY, tpCurX;
   TEXTUREPOINT tpSlope;
   memset (&tpCurY, 0, sizeof(tpCurY));
   if (pip)
      pip += pep->dwStart * dwWidth;
   if (pfp)
      pfp += pep->dwStart * dwWidth;
   // increase by delta
   for (i = 0; i < 2; i++)
      tpCurY.hv[i] += tpDownDelta.hv[i] * (fp)pep->dwStart;
   for (i = 0; i < 3; i++)
      tpCurY.xyz[i] += tpDownDelta.xyz[i] * (fp)pep->dwStart;
   for (y = pep->dwStart; y < pep->dwEnd; y++) {
      PIMAGEPIXEL pipStart = pip;
      PFIMAGEPIXEL pfpStart = pfp;

      // increase by delta
      for (i = 0; i < 2; i++)
         tpCurY.hv[i] += tpDownDelta.hv[i];
      for (i = 0; i < 3; i++)
         tpCurY.xyz[i] += tpDownDelta.xyz[i];

      tpCurX = tpCurY;

      for (x = 0; x < dwWidth; x++, (pip ? pip++ : (PIMAGEPIXEL)(pfp++))) {
         // increase by delta so don't forget
         for (i = 0; i < 2; i++)
            tpCurX.hv[i] += tpRightDelta.hv[i];
         for (i = 0; i < 3; i++)
            tpCurX.xyz[i] += tpRightDelta.xyz[i];

         // ignore background?
         if (m_fIgnoreBackground) {
            WORD w = pip ? HIWORD(pip->dwID) : HIWORD(pfp->dwID);
            if (!w)
               continue;
         }

         if (pip) {
            paf[0] = pip->wRed;
            paf[1] = pip->wGreen;
            paf[2] = pip->wBlue;
         }
         else
            paf = &pfp->fRed;

         if (!pTexture->PixelBump (dwThread, &tpCurX, &tpRightDelta, &tpDownDelta, &tpSlope))
               // NOTE: Passing in fHighQuality = FALSE into PixelBump
            continue;

         // figure out normal
         CPoint pRight, pDown, pN;
         pRight.Zero();
         pRight.p[0] = fWidthPerPixel;
         pRight.p[1] = -tpSlope.h;
         pDown.Zero();
         pDown.p[2] = -fWidthPerPixel;
         pDown.p[1] = -tpSlope.v;
         pN.CrossProd (&pDown, &pRight);
         pN.Normalize();

         // determine brighness of light
         fp fLight = pL.DotProd (&pN);
         fLight = max(fLight, 0);   // so not negative

         // do specularity
         fp fSpec = 0;
         if (fLight && m_fSpec) {
            CPoint pH;
            pH.Add (&pEye, &pL);
            pH.Normalize();
            fSpec = pN.DotProd (&pH);
            fSpec = max(fSpec, 0);
            fSpec = pow(fSpec, 10) * (fp)0xffff * m_fSpec;
         }

         fLight = fLight * (1.0 - m_fAmbient) + m_fAmbient;
         if (m_fTransparent)
            fLight = 0; // doesnt matter

         for (j = 0; j < 3; j++)
            paf[j] = paf[j] * fLight + fSpec;

         // transparency?
         if (m_fTransparent) {
            // calculate info
            fp fCi = fabs(pEye.DotProd (&pN));
            
            // angle of incidence
            fp fNit;
            fNit = 1.0 / 1.44;   // glass, so can never get internal refracton

            // sqrt value
            fp fRoot;
            fRoot = 1 + fNit * fNit * (fCi * fCi - 1);
            if (fRoot > 0) {
               // refraction
               fRoot = fNit * fCi - sqrt(fRoot);

               CPoint pRefractDir;
               pRefractDir.Copy (&pN);
               pRefractDir.Scale (fRoot);
               CPoint pN2;
               pN2.Copy (&pEye);
               pN2.Scale (-fNit);
               pRefractDir.Add (&pN2);
               // dont need pRefractDir.Normalize();  // just in case

               // scale to see how many pixels off
               fp fScale = (m_fTransDist * (fp)dwWidth / (pRefractDir.p[1] * m_fWidth));
               fp fX = fScale * pRefractDir.p[0] + (fp)x;
               fp fY = -fScale * pRefractDir.p[2] + (fp)y;
               int iX, iY, iX2, iY2;

               fY = max(fY, 0);
               fY = min(fY, (fp)dwHeight-1);
               iY = (int)fY;
               fY -= (fp)iY;
               iY2 = min(iY+1, (int)dwHeight-1);

               if (f360) {
                  fX = myfmod(fX, (fp)dwWidth);
                  iX = fX;
                  fX -= (fp)iX;
                  iX2 = (iX+1) % (int)dwWidth;
               }
               else {
                  fX = max(fX, 0);
                  fX = min(fX, (fp)dwWidth-1);
                  iX = fX;
                  fX -= (fp)iX;
                  iX2 = min(iX+1, (int)dwWidth-1);
               }

               float afColor[2][3];
               if (pImageSrc) {
                  PIMAGEPIXEL apip[2][2];
                  for (i = 0; i < 2; i++) for (j = 0; j < 2; j++)
                     apip[i][j] = pImageSrc->Pixel (i ? (DWORD)iX2 : (DWORD)iX, j ? (DWORD)iY2 : (DWORD)iY);

                  for (i = 0; i < 3; i++) {
                     afColor[0][i] = (1.0 - fX) * (fp)(&apip[0][0]->wRed)[i] +
                        fX * (fp)(&apip[1][0]->wRed)[i];
                     afColor[1][i] = (1.0 - fX) * (fp)(&apip[0][1]->wRed)[i] +
                        fX * (fp)(&apip[1][1]->wRed)[i];
                  } // i
               }
               else {
                  PFIMAGEPIXEL apip[2][2];
                  for (i = 0; i < 2; i++) for (j = 0; j < 2; j++)
                     apip[i][j] = pFImageSrc->Pixel (i ? (DWORD)iX2 : (DWORD)iX, j ? (DWORD)iY2 : (DWORD)iY);

                  for (i = 0; i < 3; i++) {
                     afColor[0][i] = (1.0 - fX) * (&apip[0][0]->fRed)[i] +
                        fX * (&apip[1][0]->fRed)[i];
                     afColor[1][i] = (1.0 - fX) * (&apip[0][1]->fRed)[i] +
                        fX * (&apip[1][1]->fRed)[i];
                  } // i
               }

               // sum together
               for (i = 0; i < 3; i++)
                  paf[i] += (1.0 - fY) * afColor[0][i] + fY * afColor[1][i];
            } // if fRoot > 0
         } // if transparent

         // write it back
         if (pip) {
            for (j = 0; j < 3; j++) {
               f = af[j];
               f = max(f, 0);
               f = min(f, (float)0xffff);
               (&pip->wRed)[j] = (WORD)f;
            } // j
         }
      } // x

   } // y
}

/*********************************************************************************
CNPREffectRelief::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImageSrc - Image. If NULL then use pFImage
   PCFImage       pFImageSrc - Floating point image. If NULL then use pImage
   PCImage        pImageDest - Dest image. If NULL then use pFImage
   PCFImage       pFImageDest - Dest floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/
BOOL CNPREffectRelief::RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                  PCImage pImageDest, PCFImage pFImageDest,
                                  PCProgressSocket pProgress)
{
   PIMAGEPIXEL pip = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   BOOL f360 = pImageDest ? pImageDest->m_f360 : pFImageDest->m_f360;

   m_fWidth = max(m_fWidth, 0.0001);

   // try creating the texture
   PCTextureMapSocket pTexture;
   RENDERSURFACE rs;
   memset (&rs, 0, sizeof(rs));
   memcpy (&rs.abTextureMatrix, &m_pSurf->m_mTextureMatrix, sizeof(m_pSurf->m_mTextureMatrix));
   memcpy (&rs.afTextureMatrix, &m_pSurf->m_afTextureMatrix, sizeof(m_pSurf->m_afTextureMatrix));
   rs.fUseTextureMap = m_pSurf->m_fUseTextureMap;
   rs.gTextureCode = m_pSurf->m_gTextureCode;
   rs.gTextureSub = m_pSurf->m_gTextureSub;
   memcpy (&rs.Material, &m_pSurf->m_Material, sizeof(m_pSurf->m_Material));
   wcscpy (rs.szScheme, m_pSurf->m_szScheme);
   rs.TextureMods = m_pSurf->m_TextureMods;
   pTexture = TextureCacheGet (m_dwRenderShard, &rs, NULL, NULL);
   if (!pTexture)
      return TRUE;   // couldnt create, so no changes
   
   // BUGFIX - Cache texture
   pTexture->ForceCache(FORCECACHE_ALL);


   // BUGFIX - Multithreaded
   EMTEFFECTRELIEF em;
   memset (&em, 0, sizeof(em));
   em.pFImageDest = pFImageDest;
   em.pFImageSrc = pFImageSrc;
   em.pImageDest = pImageDest;
   em.pImageSrc = pImageSrc;
   em.pTexture = pTexture;
   ThreadLoop (0, dwHeight, 4, &em, sizeof(em), pProgress);

   // finally, release the texture
   TextureCacheRelease (m_dwRenderShard, pTexture);

   return TRUE;
}


/*********************************************************************************
CNPREffectRelief::Render - From CNPREffect
*/
BOOL CNPREffectRelief::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pOrig, NULL, pDest, NULL, pProgress);
}



/*********************************************************************************
CNPREffectRelief::Render - From CNPREffect
*/
BOOL CNPREffectRelief::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pOrig, NULL, pDest, pProgress);
}




/****************************************************************************
EffectReliefPage
*/
BOOL EffectReliefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PRELIEFPAGE pmp = (PRELIEFPAGE)pPage->m_pUserData;
   PCNPREffectRelief pv = pmp->pe;

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
         if (pControl = pPage->ControlFind (L"transparent"))
            pControl->AttribSetBOOL (Checked(), pv->m_fTransparent);

         MeasureToString (pPage, L"width", pv->m_fWidth);
         MeasureToString (pPage, L"transdist", pv->m_fTransDist);
         AngleToControl (pPage, L"direction", pv->m_fDirection);
         AngleToControl (pPage, L"incidence", pv->m_fIncidence);

         // scrollbars
         pControl = pPage->ControlFind (L"ambient");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fAmbient * 100));
         pControl = pPage->ControlFind (L"spec");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSpec * 100));
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

         if (!_wcsicmp(p->pControl->m_pszName, L"changetext")) {
            CWorld World;
            World.RenderShardSet (pv->m_dwRenderShard);
            if (TextureSelDialog (pv->m_dwRenderShard, pPage->m_pWindow->m_hWnd, pv->m_pSurf, pmp->pWorld ? pmp->pWorld : &World))
               pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
         if (!_wcsicmp(p->pControl->m_pszName, L"transparent")) {
            pv->m_fTransparent = p->pControl->AttribGetBOOL (Checked());
            pPage->Message (ESCM_USER+189);  // update bitmap
            return TRUE;
         }
      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         // just get all values
         MeasureParseString (pPage, L"width", &pv->m_fWidth);
         MeasureParseString (pPage, L"transdist", &pv->m_fTransDist);
         pv->m_fDirection = AngleFromControl (pPage, L"direction");
         pv->m_fIncidence = AngleFromControl (pPage, L"incidence");

         pPage->Message (ESCM_USER+189);  // update bitmap
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

         if (!_wcsicmp (psz, L"ambient"))
            pf = &pv->m_fAmbient;
         else if (!_wcsicmp (psz, L"spec"))
            pf = &pv->m_fSpec;
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
            p->pszSubString = L"Relief";
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
CNPREffectRelief::Dialog - From CNPREffect
*/
BOOL CNPREffectRelief::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   RELIEFPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTRELIEF, EffectReliefPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}


// BUGBUG - when transfer effects, will need a way to transfer textures
// along with effects, since relief relies on textures
