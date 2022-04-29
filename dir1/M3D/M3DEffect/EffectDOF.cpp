/*********************************************************************************
CNPREffectDOF.cpp - Code for effect

begun 22/4/2004
Copyright 2004 by Mike Rozak
All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// DOFPIXEL - Information for depth of field
typedef struct {
   float    afColor[3];    // color sums
   float    fWeight;       // weight summed in
   short    sDepth;        // depth, >0 => long-distance blue, <0 => near-blur
} DOFPIXEL, *PDOFPIXEL;

// PEMTDOF - Multhtreaded outline info
typedef struct {
   DWORD          dwStart;
   DWORD          dwEnd;
   PCImage        pImageSrc;
   PCImage        pImageDest;
   PCFImage       pFImageSrc;
   PCFImage       pFImageDest;
   PDOFPIXEL      pdop;
   PCBlurBuf      *ppbb;
} EMTDOF, *PEMTDOF;


// DOFPAGE - Page info
typedef struct {
   PCNPREffectDOF pe;   // effects list
   int            iVScroll;      // inital scroll
   HBITMAP        hBit;    // bitmap for the image
   PCImage        pTest;   // test image
   PCRenderSuper  pRender; // renderer that generated test image
   PCWorldSocket  pWorld;  // world for image
   BOOL           fAllEffects;   // if TRUE then show all effects combined
   PCNPREffectsList  pAllEffects;   // shows all effects
} DOFPAGE, *PDOFPAGE;



PWSTR gpszEffectDOF = L"DOF";



/*********************************************************************************
CNPREffectDOF::Constructor and destructor
*/
CNPREffectDOF::CNPREffectDOF (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;
   m_tpBlurDistMinMax.h = 5;
   m_tpBlurDistMinMax.v = 20;
   m_tpBlurNearMinMax.h = 0.5;
   m_tpBlurNearMinMax.v = 1;
   m_fBlurWidth = 1;
   m_dwBlurSize = 2;

}

CNPREffectDOF::~CNPREffectDOF (void)
{
   // do nothing
}


/*********************************************************************************
CNPREffectDOF::Delete - From CNPREffect
*/
void CNPREffectDOF::Delete (void)
{
   delete this;
}


/*********************************************************************************
CNPREffectDOF::QueryInfo - From CNPREffect
*/
void CNPREffectDOF::QueryInfo (PNPRQI pqi)
{
   memset (pqi, 0, sizeof(*pqi));
   pqi->fInPlace = TRUE;
   pqi->pszDesc = L"Creates depth of field (blurring at a distance), designed to be "
      L"as fast as possible.";
   pqi->pszName = L"Depth of field";
   pqi->pszID = gpszEffectDOF;
}



static PWSTR gpszBlurWidth = L"BlurWidth";
static PWSTR gpszBlurSize = L"BlurSize";
static PWSTR gpszBlurNearMinMax = L"BlurNearMinMax";
static PWSTR gpszBlurDistMinMax = L"BlurDistMinMax";

/*********************************************************************************
CNPREffectDOF::MMLTo - From CNPREffect
*/
PCMMLNode2 CNPREffectDOF::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszEffectDOF);

   MMLValueSet (pNode, gpszBlurWidth, m_fBlurWidth);
   MMLValueSet (pNode, gpszBlurSize, (int) m_dwBlurSize);

   MMLValueSet (pNode, gpszBlurNearMinMax, &m_tpBlurNearMinMax);
   MMLValueSet (pNode, gpszBlurDistMinMax, &m_tpBlurDistMinMax);

   return pNode;
}


/*********************************************************************************
CNPREffectDOF::MMLFrom - From CNPREffect
*/
BOOL CNPREffectDOF::MMLFrom (PCMMLNode2 pNode)
{
   m_fBlurWidth = MMLValueGetDouble (pNode, gpszBlurWidth, 1);
   m_dwBlurSize = (DWORD) MMLValueGetInt (pNode, gpszBlurSize, (int) 2);

   MMLValueGetTEXTUREPOINT (pNode, gpszBlurNearMinMax, &m_tpBlurNearMinMax);
   MMLValueGetTEXTUREPOINT (pNode, gpszBlurDistMinMax, &m_tpBlurDistMinMax);

   return TRUE;
}




/*********************************************************************************
CNPREffectDOF::MMLFrom - From CNPREffect
*/
CNPREffect * CNPREffectDOF::Clone (void)
{
   return CloneEffect ();
}

/*********************************************************************************
CNPREffectDOF::CloneEffect - From CNPREffect
*/
CNPREffectDOF * CNPREffectDOF::CloneEffect (void)
{
   PCNPREffectDOF pNew = new CNPREffectDOF(m_dwRenderShard);
   if (!pNew)
      return NULL;

   pNew->m_fBlurWidth = m_fBlurWidth;
   pNew->m_dwBlurSize = m_dwBlurSize;

   pNew->m_tpBlurNearMinMax = m_tpBlurNearMinMax;
   pNew->m_tpBlurDistMinMax = m_tpBlurDistMinMax;

   return pNew;
}





/*********************************************************************************
CNPREffectDOF::EscMultiThreadedCallback - Handles multithreaded code

Standard API
*/
void CNPREffectDOF::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread)
{
   PEMTDOF pep = (PEMTDOF)pParams;

   PCImage pImageSrc = pep->pImageSrc;
   PCImage pImageDest = pep->pImageDest;
   PCFImage pFImageSrc = pep->pFImageSrc;
   PCFImage pFImageDest = pep->pFImageDest;
   PDOFPIXEL pdop = pep->pdop;
   PCBlurBuf *ppbb = pep->ppbb;

   PIMAGEPIXEL pip = pImageDest ? pImageSrc->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageSrc->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   BOOL f360 = pImageDest ? pImageDest->m_f360 : pFImageDest->m_f360;

   // loop over all points
   PCBlurBuf pbb;
   DWORD x, y;
   PIMAGEPIXEL pipCur = pip;
   PFIMAGEPIXEL pfpCur = pfp;
   float af[3];
   float *paf = &af[0];
   DWORD dwIndex;
   DWORD dwBlurSize;
   short sDepth;
   if (pipCur)
      pipCur += pep->dwStart * dwWidth;
   if (pfpCur)
      pfpCur += pep->dwStart * dwWidth;
   dwIndex = pep->dwStart * dwWidth;
   for (y = pep->dwStart; y < pep->dwEnd; y++) {
      for (x = 0; x < dwWidth; x++, pipCur ? pipCur++ : (PIMAGEPIXEL)(pfpCur++), dwIndex++ ) {
         // if not blurry at all then ignore
         sDepth = pdop[dwIndex].sDepth;
         if (!sDepth)
            continue;   // no depth bluring, and already included point
         else if (sDepth < 0)
            dwBlurSize = (DWORD)(-sDepth);
         else
            dwBlurSize = (DWORD)sDepth;
         pbb = ppbb[dwBlurSize];

         // get the color
         if (pipCur) {
            af[0] = pipCur->wRed;
            af[1] = pipCur->wGreen;
            af[2] = pipCur->wBlue;
         }
         else
            paf = &pfpCur->fRed;

         // apply blurring
         float *pafBlur = (float*)pbb->m_memFloat.p;
         DWORD xx, yy;
         int iX, iY;
         int iXStart = (int)x - (int)pbb->m_dwWidth/2;
         DWORD dwLineIndex, dwCurIndex;
         for (yy = 0, iY = (int)y - (int)pbb->m_dwHeight/2; yy < pbb->m_dwHeight; yy++, iY++) {
            if ((iY < 0) || (iY >= (int) dwHeight)) {
               pafBlur += pbb->m_dwWidth;
               continue;   // beyond range
            }
            dwLineIndex = (DWORD)iY * dwWidth;
            dwCurIndex = (DWORD) ((int)dwLineIndex + iXStart);

            for (xx = 0, iX = iXStart; xx < pbb->m_dwWidth; xx++, iX++, dwCurIndex++, pafBlur++) {
               // if nothing to blur then skip
               if (!pafBlur[0])
                  continue;

               // do modulo math
               if ((iX < 0) || (iX >= (int) dwWidth)) {
                  if (!f360)
                     continue;   // not 360 degree image, so ignore

                  // else modulo
                  dwCurIndex = dwLineIndex + (DWORD) ((iX + (int)dwWidth) % (int)dwWidth);
               }

               // if the pixel that looking over is closer than our center pixel
               // then don't blur over
               DWORD dwBlurThis;
               short sDepthThis = pdop[dwCurIndex].sDepth;
               if (sDepthThis < 0)
                  dwBlurThis = (DWORD)(-sDepthThis);
               else
                  dwBlurThis = (DWORD)sDepthThis;
               // BUGFIX - If object in front and blurrier, then can blur over
               if ((dwBlurThis >= dwBlurSize) || (sDepth < 0)) {
                  // else, blur over, using blurring amount as weight
                  pdop[dwCurIndex].afColor[0] += af[0] * pafBlur[0];
                  pdop[dwCurIndex].afColor[1] += af[1] * pafBlur[0];
                  pdop[dwCurIndex].afColor[2] += af[2] * pafBlur[0];
                  pdop[dwCurIndex].fWeight += pafBlur[0];
               }
               else if (!dwBlurThis)
                  continue;   // perfectly in focus, so dont bother
               else {
                  // allowed to blur over, but can only blur over as much as
                  // dwBlurThis, which is dwBlurSize less
                  // Know that dwBlurThis >= 1, and < dwBlurSize
                  PCBlurBuf pbbThis = ppbb[dwBlurThis];
                  float *pafBlurThis = (float*)pbbThis->m_memFloat.p;

                  int iXXThis = (int)xx - (int)(pbb->m_dwWidth - pbbThis->m_dwWidth) / 2;
                  int iYYThis = (int)yy - (int)(pbb->m_dwHeight - pbbThis->m_dwHeight) / 2;
                  if ((iXXThis < 0) || (iYYThis < 0) || (iXXThis >= (int)pbbThis->m_dwWidth)
                     || (iYYThis >= (int)pbbThis->m_dwHeight))
                     continue;   // out of range
                  pafBlurThis += iXXThis + iYYThis * (int) pbbThis->m_dwWidth;
                  if (!pafBlurThis[0])
                     continue;   // nothing there

                  // else, blur over, using blurring amount as weight
                  pdop[dwCurIndex].afColor[0] += af[0] * pafBlurThis[0];
                  pdop[dwCurIndex].afColor[1] += af[1] * pafBlurThis[0];
                  pdop[dwCurIndex].afColor[2] += af[2] * pafBlurThis[0];
                  pdop[dwCurIndex].fWeight += pafBlurThis[0];
               }
            } // xx
         } // yy
      } // i
   } // y

}

/*********************************************************************************
CNPREffectDOF::RenderAny - This renders both integer and FP image.

inputs
   PCImage        pImageSrc - Image. If NULL then use pFImage
   PCFImage       pFImageSrc - Floating point image. If NULL then use pImage
   PCImage        pImageDest - Image. If NULL then use pFImage
   PCFImage       pFImageDest - Floating point image. If NULL then use pImage
   PCProgressSocket pProgress - Progress
returns
   BOOL - TRUE if success
*/

BOOL CNPREffectDOF::RenderAny (PCImage pImageSrc, PCFImage pFImageSrc,
                                PCImage pImageDest, PCFImage pFImageDest,
                                PCProgressSocket pProgress)
{
   PIMAGEPIXEL pip = pImageDest ? pImageSrc->Pixel(0,0) : NULL;
   PFIMAGEPIXEL pfp = pImageDest ? NULL : pFImageSrc->Pixel(0,0);
   DWORD dwWidth = pImageDest ? pImageDest->Width() : pFImageDest->Width();
   DWORD dwHeight = pImageDest ? pImageDest->Height() : pFImageDest->Height();
   BOOL f360 = pImageDest ? pImageDest->m_f360 : pFImageDest->m_f360;

   CMem mem;
   DWORD dwMaxBlur = (DWORD)(m_fBlurWidth / 100.0 * (fp)dwWidth / 2.0);
     // note: dwMaxBlur / 2
   dwMaxBlur = max(dwMaxBlur, 1);
   dwMaxBlur = min(dwMaxBlur, 126); // cant be too large

   BOOL fRet = TRUE;

   // allocate the memory for the blurs
   CListFixed lPCBlurBuf;
   lPCBlurBuf.Init (sizeof(PCBlurBuf));
   DWORD i, j;
   PCBlurBuf pbb;
   lPCBlurBuf.Required (dwMaxBlur+1);
   for (i = 0; i <= dwMaxBlur; i++) {  // intentionally using <=
      pbb = new CBlurBuf;
      if (!pbb) {
         fRet = FALSE;
         goto done;
      }
      if (!pbb->Init (i, m_dwBlurSize)) {
         delete pbb;
         goto done;
      }

      // go through and scale the floating point values so the highest is 1.0
      // this way when average blur energies together will blur better
      float *paf = (float*)pbb->m_memFloat.p;
      float fMax = 0;
      DWORD dwTotal = pbb->m_dwWidth * pbb->m_dwHeight;
      for (j = 0; j < dwTotal; j++)
         fMax = max(fMax, paf[j]);
      if (fMax)
         fMax = 1.0 / sqrt(fMax); // use sqrt() as hack to make blurring of
                                             // near objects work over far
      for (j = 0; j < dwTotal; j++)
         paf[j] *= fMax;

      lPCBlurBuf.Add (&pbb);
   } // i
   PCBlurBuf *ppbb;
   ppbb = (PCBlurBuf*)lPCBlurBuf.Get(0);

   // allocate memory for blur buffer
   DWORD dwTotal = dwWidth * dwHeight;
   if (!mem.Required (dwTotal * sizeof(DOFPIXEL))) {
      fRet = FALSE;
      goto done;
   }
   PDOFPIXEL pdop = (PDOFPIXEL) mem.p;
   memset (pdop, 0, dwTotal * sizeof(DOFPIXEL));

   // fill in the Z values
   fp fDeltaNear = m_tpBlurNearMinMax.v - m_tpBlurNearMinMax.h;
   fp fDeltaDist = m_tpBlurDistMinMax.v - m_tpBlurDistMinMax.h;
   fp fAlpha;
   BOOL fUseDeltaDist = (fDeltaDist > 0);
   for (i = 0; i < dwTotal; i++) {
      fp fZ = pip ? pip[i].fZ : pfp[i].fZ;

      if (fZ <= m_tpBlurNearMinMax.h)
         pdop[i].sDepth = -(short)dwMaxBlur; // near
      else if (fZ < m_tpBlurNearMinMax.v) {
         fAlpha = (fZ - m_tpBlurNearMinMax.h) / fDeltaNear;
         pdop[i].sDepth = -(short) ((1.0 - fAlpha) * (fp)dwMaxBlur);
      }
      else if ((fZ >= m_tpBlurDistMinMax.v) && fUseDeltaDist)
         pdop[i].sDepth = (short)dwMaxBlur;  // far away
      else if (fZ > m_tpBlurDistMinMax.h && fUseDeltaDist) {
         fAlpha = (fZ - m_tpBlurDistMinMax.h) / fDeltaDist;
         pdop[i].sDepth = (short) (fAlpha * (fp)dwMaxBlur);
      }
      else
         pdop[i].sDepth = 0;  // right in focus

      // if it's in focus then fill right now
      if (!pdop[i].sDepth) {
         if (pip) {
            pdop[i].afColor[0] = pip[i].wRed;
            pdop[i].afColor[1] = pip[i].wGreen;
            pdop[i].afColor[2] = pip[i].wBlue;
         }
         else {
            pdop[i].afColor[0] = pfp[i].fRed;
            pdop[i].afColor[1] = pfp[i].fGreen;
            pdop[i].afColor[2] = pfp[i].fBlue;
         }

         pdop[i].fWeight = 1;
      }

   } // i

   // BUGFIX - Multithreaded
   EMTDOF em;
   memset (&em, 0, sizeof(em));
   em.ppbb = ppbb;
   em.pFImageDest = pFImageDest;
   em.pFImageSrc = pFImageSrc;
   em.pImageDest = pImageDest;
   em.pImageSrc = pImageSrc;
   em.pdop = pdop;
   ThreadLoop (0, dwHeight, 2, &em, sizeof(em), pProgress);
      // NOTE: Some of the code for blurring isn't entirely thread safe, so
      // to minimize the change of impact, only 2 divisions


   PIMAGEPIXEL pipCur;
   PFIMAGEPIXEL pfpCur;

   // copy the points into the destination
   pipCur = pImageDest ? pImageDest->Pixel(0,0) : NULL;
   pfpCur = pImageDest ? NULL : pFImageDest->Pixel(0,0);
   fp f;
   for (i = 0; i < dwTotal; i++, pipCur ? pipCur++ : (PIMAGEPIXEL)(pfpCur++), pdop++) {
      fp fScale = 1.0 / pdop->fWeight;

      if (pipCur) {
         for (j = 0; j < 3; j++) {
            f = pdop->afColor[j] * fScale;
            f = max(f, 0);
            f = min(f, (fp)0xffff);
            (&pipCur->wRed)[j] = (WORD)f;
         } // j
      }
      else {
         pfpCur->fRed = pdop->afColor[0] * fScale;
         pfpCur->fGreen = pdop->afColor[1] * fScale;
         pfpCur->fBlue = pdop->afColor[2] * fScale;
      }
   } // i

done:
   // free blur buf
   ppbb = (PCBlurBuf*)lPCBlurBuf.Get(0);
   for (i = 0; i < lPCBlurBuf.Num(); i++)
      delete ppbb[i];

   return fRet;



}


/*********************************************************************************
CNPREffectDOF::Render - From CNPREffect
*/
BOOL CNPREffectDOF::Render (PCImage pOrig, PCImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (pOrig, NULL, pDest, NULL, pProgress);
}



/*********************************************************************************
CNPREffectDOF::Render - From CNPREffect
*/
BOOL CNPREffectDOF::Render (PCFImage pOrig, PCFImage pDest, PCRenderSuper pRender,
                     PCWorldSocket pWorld, BOOL fFinal, PCProgressSocket pProgress)
{
   return RenderAny (NULL, pOrig, NULL, pDest, pProgress);
}




/****************************************************************************
EffectDOFPage
*/
BOOL EffectDOFPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDOFPAGE pmp = (PDOFPAGE)pPage->m_pUserData;
   PCNPREffectDOF pv = pmp->pe;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set button for show all effects
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"alleffects");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pmp->fAllEffects);

         DoubleToControl (pPage, L"blurwidth", pv->m_fBlurWidth);
         DoubleToControl (pPage, L"blursize", pv->m_dwBlurSize);

         MeasureToString (pPage, L"blurnearmin", pv->m_tpBlurNearMinMax.h);
         MeasureToString (pPage, L"blurnearmax", pv->m_tpBlurNearMinMax.v);
         MeasureToString (pPage, L"blurdistmin", pv->m_tpBlurDistMinMax.h);
         MeasureToString (pPage, L"blurdistmax", pv->m_tpBlurDistMinMax.v);
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

      }
      break;   // default

   case ESCN_EDITCHANGE:
      {
         // just get all values
         pv->m_fBlurWidth = DoubleFromControl (pPage, L"blurwidth");
         pv->m_dwBlurSize = (DWORD) DoubleFromControl (pPage, L"blursize");

         fp fTemp;
         MeasureParseString (pPage, L"blurnearmin", &fTemp);
         pv->m_tpBlurNearMinMax.h = fTemp;
         MeasureParseString (pPage, L"blurnearmax", &fTemp);
         pv->m_tpBlurNearMinMax.v = fTemp;
         MeasureParseString (pPage, L"blurdistmin", &fTemp);
         pv->m_tpBlurDistMinMax.h = fTemp;
         MeasureParseString (pPage, L"blurdistmax", &fTemp);
         pv->m_tpBlurDistMinMax.v = fTemp;

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
            p->pszSubString = L"Depth of field";
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
CNPREffectDOF::IsPainterly - Returns TRUE if is a painterly effect
and doesn't need as high a resolution
*/
BOOL CNPREffectDOF::IsPainterly (void)
{
   return FALSE;
}

/*********************************************************************************
CNPREffectDOF::Dialog - From CNPREffect
*/
BOOL CNPREffectDOF::Dialog (PCEscWindow pWindow, PCNPREffectsList pAllEffects, PCImage pTest, 
                            PCRenderSuper pRender, PCWorldSocket pWorld)
{
   PWSTR pszRet;
   DOFPAGE mp;
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

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLEFFECTDOF, EffectDOFPage, &mp);

   // elete
   if (mp.hBit)
      DeleteObject (mp.hBit);
   mp.hBit = NULL;

   if (pszRet && !_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}




