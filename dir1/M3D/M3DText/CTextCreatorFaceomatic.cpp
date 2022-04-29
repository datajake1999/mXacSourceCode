/********************************************************************************
CTextCreatorFaceomatic.cpp - Code for handling faces.

begun 15/9/05 by Mike Rozak
Copyright 2005 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"


#define DEFGLOWSCALE       1.0      // default glow scale

#define SUBTEXTHEIGHT               64
#define SUBTEXTWIDTH(stretch)       ((stretch) ? (SUBTEXTHEIGHT*2) : SUBTEXTHEIGHT)

void DrawHairStraight (PCImage pImage, PCImage pImage2, WORD *pawColor, float *pafGlow, PCPoint p1, PCPoint p2, fp fWidth,
                       PCMaterial pMaterial);




// MTFACEINFO - Pass into multithreaded code
typedef struct {
   DWORD       dwStart;    // start, inclusive
   DWORD       dwEnd;      // end, exclsuive
   DWORD       dwPass;     // depending upon the pass type
   PCTextCreatorFaceomatic pFace;   // face
   PCImage     pImage;     // main image
   PCImage     pImage2;    // glow and whatnot
   PCImage     pImageZ;    // copy of original image with z in tact
   DWORD       dwRenderShard; // shard to render
} MTFACEINFO, *PMTFACEINFO;



/****************************************************************************
CFaceFur::Constructor and destructor
*/
CFaceFur::CFaceFur (void)
{
   Clear();
}

CFaceFur::~CFaceFur (void)
{
   // do nothing for now
}


static PWSTR gpszFur = L"Fur";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszCoverageAmt = L"CoverageAmt";
static PWSTR gpszCoverage = L"Coverage";
static PWSTR gpszLenVal = L"LenVal";
static PWSTR gpszHair = L"Hair";
static PWSTR gpszVariation = L"Variation";
static PWSTR gpszColor = L"Color";
static PWSTR gpszLength = L"Length";
static PWSTR gpszHairCenter = L"HairCenter";
static PWSTR gpszUse = L"Use";
static PWSTR gpszStrength = L"Strength";
static PWSTR gpszCenter = L"Center";
static PWSTR gpszSize = L"Size";

/****************************************************************************
CFaceFur::MMLTo - Standard API
*/
PCMMLNode2 CFaceFur::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszFur);

   MMLValueSet (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   MMLValueSet (pNode, gpszLenVal, &m_pLenVal);
   MMLValueSet (pNode, gpszHair, &m_pHair);
   MMLValueSet (pNode, gpszVariation, &m_pVariation);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);


   PCMMLNode2 pSub;
   pSub = m_Coverage.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszCoverage);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Color.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszColor);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Length.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszLength);
      pNode->ContentAdd (pSub);
   }

   DWORD i;
   for (i = 0; i < MAXHAIR; i++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszHairCenter);
      MMLValueSet (pSub, gpszUse, (int) m_aHAIRCENTER[i].fUse);
      MMLValueSet (pSub, gpszStrength, m_aHAIRCENTER[i].fStrength);
      MMLValueSet (pSub, gpszCenter, &m_aHAIRCENTER[i].tpCenter);
      MMLValueSet (pSub, gpszSize, &m_aHAIRCENTER[i].tpSize);
   } // i

   return pNode;
}


/****************************************************************************
CFaceFur::MMLFrom - Standard API
*/
BOOL CFaceFur::MMLFrom (PCMMLNode2 pNode)
{
   // clear existing
   Clear();

   MMLValueGetPoint (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   MMLValueGetPoint (pNode, gpszLenVal, &m_pLenVal);
   MMLValueGetPoint (pNode, gpszHair, &m_pHair);
   MMLValueGetPoint (pNode, gpszVariation, &m_pVariation);
   m_iSeed = MMLValueGetInt (pNode, gpszSeed, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   DWORD dwCurHair = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszCoverage)) {
         m_Coverage.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszColor)) {
         m_Color.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszLength)) {
         m_Length.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszHairCenter)) {
         if (dwCurHair >= MAXHAIR)
            continue;   // out of range

         m_aHAIRCENTER[dwCurHair].fUse = MMLValueGetInt (pSub, gpszUse, FALSE);
         m_aHAIRCENTER[dwCurHair].fStrength = MMLValueGetDouble (pSub, gpszStrength, 1);
         MMLValueGetTEXTUREPOINT (pSub, gpszCenter, &m_aHAIRCENTER[dwCurHair].tpCenter);
         MMLValueGetTEXTUREPOINT (pSub, gpszSize, &m_aHAIRCENTER[dwCurHair].tpSize);

         dwCurHair++;
         continue;
      }
   } // i


   return TRUE;
}


/****************************************************************************
CFaceFur::Clear - Standard API
*/
void CFaceFur::Clear (void)
{
   m_Coverage.Clear();
   m_Color.Clear();
   m_Length.Clear();
   m_Coverage.StretchToFitSet (TRUE);
   m_Length.StretchToFitSet (TRUE);
   m_Color.StretchToFitSet (TRUE);

   m_pCoverageAmt.Zero();
   m_pCoverageAmt.p[0] = m_pCoverageAmt.p[2] = 0;
   m_pCoverageAmt.p[1] = 1;

   m_pLenVal.Zero();
   m_pLenVal.p[0] = 0.001;
   m_pLenVal.p[1] = 0.01;
   m_pLenVal.p[2] = 0.001;

   m_pHair.Zero();
   m_pHair.p[0] = 0.004;   // a bit far apart, but will draw more quickly
   m_pHair.p[1] = 0.001;
   m_pHair.p[2] = PI/16; // 12 degrees

   m_pVariation.Zero();
   m_pVariation.p[0] = .2;
   m_pVariation.p[1] = .2;
   m_pVariation.p[2] = .2;
   m_pVariation.p[3] = .2;

   m_iSeed = 0;

   m_tpScale.h = m_tpScale.v = 0;

   DWORD i;
   memset (&m_aHAIRCENTER, 0, sizeof(m_aHAIRCENTER));
   for (i = 0; i < MAXHAIR; i++) {
      m_aHAIRCENTER[i].fStrength = 1;
      m_aHAIRCENTER[i].fUse = !i;
      m_aHAIRCENTER[i].tpCenter.h = m_aHAIRCENTER[i].tpCenter.v = 0.5;
      m_aHAIRCENTER[i].tpSize.h = m_aHAIRCENTER[i].tpSize.v = 1;
   } // i
}

/****************************************************************************
CFaceFur::ScaleSet - Sets the width and height of the master texture
in meters. You should call this to ensure any textures that need distance
information will use the right scale.

inputs
   fp       fMasterWidth, fMasterHeight - Width and height in meters
*/
void CFaceFur::ScaleSet (fp fMasterWidth, fp fMasterHeight)
{
   m_Coverage.ScaleSet (fMasterWidth, fMasterHeight);
   m_Color.ScaleSet (fMasterWidth, fMasterHeight);
   m_Length.ScaleSet (fMasterWidth, fMasterHeight);

   m_tpScale.h = fMasterWidth;
   m_tpScale.v = fMasterHeight;
}

/****************************************************************************
CFaceFur::Clone - Standard API
*/
CFaceFur *CFaceFur::Clone (void)
{
   // NOTE: Not tested
   PCFaceFur pNew = new CFaceFur;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/****************************************************************************
CFaceFur::CloneTo - Standard API
*/
BOOL CFaceFur::CloneTo (CFaceFur *pTo)
{
   m_Coverage.CloneTo (&pTo->m_Coverage);
   m_Color.CloneTo (&pTo->m_Color);
   m_Length.CloneTo (&pTo->m_Length);
   pTo->m_pCoverageAmt.Copy (&m_pCoverageAmt);
   pTo->m_pLenVal.Copy (&m_pLenVal);
   pTo->m_pHair.Copy (&m_pHair);
   pTo->m_pVariation.Copy (&m_pVariation);
   pTo->m_iSeed = m_iSeed;
   memcpy (pTo->m_aHAIRCENTER, m_aHAIRCENTER, sizeof(m_aHAIRCENTER));

   pTo->m_tpScale = m_tpScale;
   return TRUE;
}


/****************************************************************************
CFaceFur::TextureQuery - Standard API
*/
BOOL CFaceFur::TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree)
{
   m_Coverage.TextureQuery (dwRenderShard, plText, pTree);
   m_Color.TextureQuery (dwRenderShard, plText, pTree);
   m_Length.TextureQuery (dwRenderShard, plText, pTree);
   return TRUE;
}


/****************************************************************************
CFaceFur::SubTextureNoRecurse - Standard API
*/
BOOL CFaceFur::SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText)
{
   if (!m_Coverage.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   if (!m_Color.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   if (!m_Length.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   return TRUE;
}



/****************************************************************************
CFaceFur::EscMultiThreadedCallback - Multithreaded rendering.
*/
void CFaceFur:: EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
   DWORD dwThread)
{
   PMTFACEINFO pmti = (PMTFACEINFO) pParams;
   PCTextCreatorFaceomatic pFace = pmti->pFace;
   PCImage pImage = pmti->pImage;
   PCImage pImage2 = pmti->pImage2;
   PCImage pImageZ = pmti->pImageZ;
   DWORD dwRenderShard = pmti->dwRenderShard;


   // see random AFTER cache textures
   srand (m_iSeed);

   fp fHVar = m_pHair.p[0] / pFace->m_fWidth / 2.0;
   fp fVVar = m_pHair.p[0] / pFace->m_fHeight / 2.0;

   fp fX, fY;
   fp fH, fV;
   DWORD dwY;
   for (dwY = pmti->dwStart; dwY < pmti->dwEnd; dwY++) {
      fY = (fp)dwY * m_pHair.p[0];
      if (fY >= pFace->m_fHeight)
         break;

      fV = fY / pFace->m_fHeight;

      for (fX = 0.0; fX < pFace->m_fWidth; fX += m_pHair.p[0]) {
         fH = fX / pFace->m_fWidth;

         // draw the hair
         RenderHair (dwRenderShard, fH + MyRand(-fHVar,fHVar), fV + MyRand(-fVVar, fVVar), pImage, pImage2, pFace, pImageZ);
      } // fX
   } // fY
}


/****************************************************************************
CFaceFur::Render - Draws the Fur on

inputs
   PCImage        pImage - Main color image, from main Render() call
   PCImage        pImage2 - Glow image, from main Render() call.
   PCTextCreatorFaceomatic pFace - Face object, in case need it
   PCImage        pImageZ - A copy of the original image that keeps the Z values in tact.
*/
BOOL CFaceFur::Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace, PCImage pImageZ)
{
   // if not on dont bother
   if (!m_Coverage.m_fUse)
      return TRUE;


   // cachj
   m_Coverage.TextureCache (dwRenderShard);
   m_Color.TextureCache(dwRenderShard);
   m_Length.TextureCache(dwRenderShard);

   // make sure all the scars are loaded in
   DWORD i;
   for (i = 0; i < MAXSCARS; i++)
      if (pFace->m_apScars[i]->m_Coverage.m_fUse && pFace->m_apScars[i]->m_fAffectFur)
         pFace->m_apScars[i]->m_Coverage.TextureCache(dwRenderShard);

   // BUGFIX - Doing multithreaded hair

   // how many rows
   DWORD dwRows = m_pHair.p[0] ? (DWORD) (pFace->m_fHeight / m_pHair.p[0]) : 1;
   dwRows++;   // just in case
   MTFACEINFO mtfi;
   memset (&mtfi, 0, sizeof(mtfi));
   mtfi.pFace = pFace;
   mtfi.pImage = pImage;
   mtfi.pImage2 = pImage2;
   mtfi.pImageZ = pImageZ;
   mtfi.dwRenderShard = dwRenderShard;
   ThreadLoop (0, dwRows, 1, &mtfi, sizeof(mtfi));
      // NOTE: potential for hairs to overwrite one another, but I think is so rare that
      // not worth worrying about, especially since only doing one pass


   // uncache
   for (i = 0; i < MAXSCARS; i++)
      if (pFace->m_apScars[i]->m_Coverage.m_fUse && pFace->m_apScars[i]->m_fAffectFur)
         pFace->m_apScars[i]->m_Coverage.TextureRelease();
   m_Coverage.TextureRelease();
   m_Color.TextureRelease();
   m_Length.TextureRelease();


   return TRUE;
}


/****************************************************************************
FaceomaticFurPage
*/
BOOL FaceomaticFurPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCFaceFur pv = (PCFaceFur) pt->pThis;

   // call sub-textures for trap
   if (pv->m_Coverage.PageTrapMessages (pt->dwRenderShard, L"Coverage", pPage, dwMessage, pParam))
      return TRUE;
   if (pv->m_Color.PageTrapMessages (pt->dwRenderShard, L"Color", pPage, dwMessage, pParam))
      return TRUE;
   if (pv->m_Length.PageTrapMessages (pt->dwRenderShard, L"Length", pPage, dwMessage, pParam))
      return TRUE;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[64];
         DWORD i, j;
         PCEscControl pControl;

         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"shiny%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pCoverageAmt.p[i]);

            swprintf (szTemp, L"lenval%d", (int) i);
            MeasureToString (pPage, szTemp, pv->m_pLenVal.p[i], TRUE);

            swprintf (szTemp, L"hair%d", (int) i);
            if (i < 2)
               MeasureToString (pPage, szTemp, pv->m_pHair.p[i], TRUE);
            else
               AngleToControl (pPage, szTemp, pv->m_pHair.p[i]);

            swprintf (szTemp, L"variation%d", (int) i);
            DoubleToControl (pPage, szTemp, pv->m_pVariation.p[i]);
         } // i

         for (i = 0; i < MAXHAIR; i++) {
            swprintf (szTemp, L"radoutuse%d", (int)i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSetBOOL (Checked(), pv->m_aHAIRCENTER[i].fUse);

            swprintf (szTemp, L"radoutstrength%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_aHAIRCENTER[i].fStrength);

            for (j = 0; j < 2; j++) {
               swprintf (szTemp, L"radoutcent%d%d", (int)j, (int)i);
               DoubleToControl (pPage, szTemp, j ? pv->m_aHAIRCENTER[i].tpCenter.v : pv->m_aHAIRCENTER[i].tpCenter.h);

               swprintf (szTemp, L"radoutsize%d%d", (int)j, (int)i);
               DoubleToControl (pPage, szTemp, j ? pv->m_aHAIRCENTER[i].tpSize.v : pv->m_aHAIRCENTER[i].tpSize.h);
            }
         } // i
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // do nothing
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszUse = L"radoutuse";
         DWORD dwUseSize = (DWORD)wcslen (pszUse);

         if (!_wcsnicmp (psz, pszUse, dwUseSize)) {
            DWORD dwNum = _wtoi(psz + dwUseSize);
            pv->m_aHAIRCENTER[dwNum].fUse = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         if (!_wcsicmp(psz, L"NewSeed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszShiny = L"shiny", pszLenVal = L"lenval", pszHair = L"hair", pszVariation = L"variation",
            pszRadOutCent = L"radoutcent", pszRadOutSize = L"radoutsize", pszRadOutStrength = L"radoutstrength";
         DWORD dwShinyLen = (DWORD)wcslen(pszShiny), dwLenValLen = (DWORD)wcslen(pszLenVal),
            dwHairLen = (DWORD)wcslen(pszHair), dwVariationLen = (DWORD)wcslen(pszVariation),
            dwRadOutCentLen = (DWORD)wcslen(pszRadOutCent), dwRadOutSizeLen = (DWORD)wcslen(pszRadOutSize),
            dwRadOutStrengthLen = (DWORD)wcslen(pszRadOutStrength);
         if (!_wcsnicmp (psz, pszShiny, dwShinyLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwShinyLen);
            if (dwNum >= 3)
               break;    // error
            pv->m_pCoverageAmt.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszRadOutCent, dwRadOutCentLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + (dwRadOutCentLen+1));
            DWORD dwV = (psz[dwRadOutCentLen] == L'1');
            if (dwV)
               pv->m_aHAIRCENTER[dwNum].tpCenter.v = DoubleFromControl (pPage, p->pControl->m_pszName);
            else
               pv->m_aHAIRCENTER[dwNum].tpCenter.h = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszRadOutSize, dwRadOutSizeLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + (dwRadOutSizeLen+1));
            DWORD dwV = (psz[dwRadOutSizeLen] == L'1');
            if (dwV)
               pv->m_aHAIRCENTER[dwNum].tpSize.v = DoubleFromControl (pPage, p->pControl->m_pszName);
            else
               pv->m_aHAIRCENTER[dwNum].tpSize.h = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszRadOutStrength, dwRadOutStrengthLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwRadOutStrengthLen);
            pv->m_aHAIRCENTER[dwNum].fStrength = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszLenVal, dwLenValLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwLenValLen);
            if (dwNum >= 3)
               break;    // error
            MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_pLenVal.p[dwNum]);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszHair, dwHairLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwHairLen);
            if (dwNum >= 3)
               break;    // error
            if (dwNum < 2)
               MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_pHair.p[dwNum]);
            else
               pv->m_pHair.p[dwNum] = AngleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszVariation, dwVariationLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwVariationLen);
            if (dwNum >= 4)
               break;    // error
            pv->m_pVariation.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Fur and eyebrows";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
CFaceFur::Dialog - Brings up a dialog for modifying the Fur.

inputs
   PCEscWindow       pWindow - Window
   PCTextCreatorFaceomatic pFace - Face texture
returns
   BOOL - TRUE if pressed back. FALSE if closed texture
*/
BOOL CFaceFur::Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pFace;
   ti.pThis = this;
   ti.fDrawFlat = TRUE;
   ti.fStretchToFit = TRUE;
   ti.dwRenderShard = dwRenderShard;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFACEOMATICFUR, FaceomaticFurPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   return pszRet && !_wcsicmp(pszRet, Back());
}



/****************************************************************************
CFaceFur::HairDirection - Determines the hair direction given a HV location.

inputs
   fp       fH - H value, 0..1
   fp       fV
   PCPoint  pDir - Filled in with the normalized direction, IN PIXELS, not HV
returns
   none
*/

void CFaceFur::HairDirection (fp fH, fp fV, PCPoint pDir)
{
   pDir->Zero();

   DWORD i;
   HAIRCENTER *phc = &m_aHAIRCENTER[0];
   DWORD dwNum = MAXHAIR;
   TEXTUREPOINT tpTemp, tpDist;
   CPoint pTemp;
   fp fLen;
   for (i = 0; i < dwNum; i++, phc++) {
      if (!phc->fUse)
         continue;   // not used

      tpTemp.h = fH - phc->tpCenter.h;
      tpTemp.v = fV - phc->tpCenter.v;

      if (!phc->tpSize.h || !phc->tpSize.v)
         continue;   // no size

      tpDist.h = fabs(tpTemp.h / phc->tpSize.h);
      tpDist.v = fabs(tpTemp.v / phc->tpSize.v);
      if ((tpDist.h >= 1.0) || (tpDist.v >= 1.0))
         continue;   // too far away
      fLen = sqrt (tpDist.h * tpDist.h + tpDist.v * tpDist.v);
      if (fLen >= 1.0)
         continue;   // too far away

      // make softer with cos
      fLen = cos(fLen * PI / 2.0);  // also converts center point to 1.0, and far to 0
      fLen *= fLen;  // so cos-squared
      fLen *= phc->fStrength;   // include strength in factor

      // new direction
      pTemp.Zero();
      pTemp.p[0] = tpTemp.h * m_tpScale.h;
      pTemp.p[1] = tpTemp.v * m_tpScale.v;
      fp fLen2 = pTemp.Length();
      if (fLen2)
         pTemp.Scale(fLen / fLen2); // so scale by strength, but normalized at full strength

      // append onto direction
      pDir->Add (&pTemp);
   } // i

   // normalize final direction
   fLen = pDir->Length();
   if (fLen)
      pDir->Scale (1.0 / fLen);
   else
      pDir->p[1] = -1;  // so at least have something
}


/****************************************************************************
CFaceFur::RenderHair - Draws a hair at the given point.

For this to work, all the textures must be cached

inputs
   fp          fH - H value, 0..1
   fp          fV - V value, 0..1
   PCImage     pImage - Main color image
   PCImage     pImage2 - For glow
   PCTextCreatorFaceomatic pFace - Just in case need
   PCImage     pImageZ - To get original Z data from
returns
   BOOL - TRUE if succes
*/
BOOL CFaceFur::RenderHair (DWORD dwRenderShard, fp fH, fp fV, PCImage pImage, PCImage pImage2,
                           PCTextCreatorFaceomatic pFace, PCImage pImageZ)
{
   // fist, see if there's fur there at all
   TEXTPOINT5 tpCur, tpMax;
   memset (&tpCur, 0, sizeof(tpCur));
   memset (&tpMax, 0, sizeof(tpMax));
   tpCur.hv[0] = fH;
   tpCur.hv[1] = fV;
   tpCur.xyz[0] = fH * m_tpScale.h;
   tpCur.xyz[1] = fV * m_tpScale.h;
   fp fCoverage;
   WORD awColor[3];
   CMaterial MatTemp;
   float afGlow[3];
   if (m_Coverage.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], FALSE)) {   // low res for this one
      fCoverage = ((fp)awColor[0] + (fp)awColor[1] + (fp)awColor[2]) / ((fp)0xffff * 3.0);
      fCoverage = fCoverage * m_pCoverageAmt.p[1] + (1.0 - fCoverage) * m_pCoverageAmt.p[0];
   }
   else
      fCoverage = m_pCoverageAmt.p[2];
   if (fCoverage <= 0)
      return FALSE;  // nothing there

   // loop through all the scars and reduce coverate
   DWORD i;
   fp f;
   for (i = 0; i < MAXSCARS; i++) {
      if (!pFace->m_apScars[i]->m_Coverage.m_fUse || !pFace->m_apScars[i]->m_fAffectFur)
         continue;   // not used

      f = pFace->m_apScars[i]->IsOverScar (&tpCur);
      if (f)
         fCoverage *= (1.0 - f);
   } // i
   if (fCoverage <= 0)
      return FALSE;  // nothing there since over scar

   // make sure not too high
   fCoverage = min(fCoverage, 1);

   // probability that wont be rendered
   if (MyRand(0, 1) > fCoverage)
      return FALSE;  // dont draw hair

   // get length
   fp fLength;
   if (m_Length.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], FALSE)) {   // low res for this one
      fLength = ((fp)awColor[0] + (fp)awColor[1] + (fp)awColor[2]) / ((fp)0xffff * 3.0);
      fLength = fLength * m_pLenVal.p[1] + (1.0 - fLength) * m_pLenVal.p[0];
   }
   else
      fLength = m_pLenVal.p[2];
   
   // vary the length
   fLength *= MyRand (1.0 - m_pVariation.p[1], 1.0 + m_pVariation.p[1]);
   if (fLength <= 0)
      return FALSE;  // nothing there
   fp fPixelLen = TextureDetailApply(dwRenderShard, pFace->m_fPixelLen);
   fLength /= fPixelLen;   // so length is in pixels


   // get color
   if (!m_Color.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], FALSE))   // low res for this one
      return FALSE;  // no color specified

   // vary the color
   for (i = 0; i < 3; i++) {
      f = awColor[i];
      f *= MyRand (1.0 - m_pVariation.p[0], 1.0 + m_pVariation.p[0]);
      f = max (f, 0);
      f = min (f, (fp)0xffff);
      awColor[i] = (WORD)f;

      if (afGlow[i]) {
         afGlow[i] *= MyRand (1.0 - m_pVariation.p[0], 1.0 + m_pVariation.p[0]);
         afGlow[i] = max(afGlow[i], 0);
      }
   } // i

   // determine the direction
   CPoint pDir;
   HairDirection (fH, fV, &pDir);
   fp fDir = atan2(pDir.p[0], pDir.p[1]);
   fDir += MyRand (- m_pVariation.p[2], m_pVariation.p[2]) * PI;
   pDir.p[0] = sin(fDir);
   pDir.p[1] = cos(fDir);

   // determine the angle
   fp fAngle = m_pHair.p[2] * MyRand (1.0 - m_pVariation.p[3], 1.0 + m_pVariation.p[3]);

   // starting point
   CPoint p1, p2;
   p1.Zero();
   int iX = (int)(fH * (fp)pImageZ->Width());
   int iY = (int)(fV * (fp)pImageZ->Height());
   while (iX < 0)
      iX += (int)pImageZ->Width();
   while (iY < 0)
      iY += (int)pImageZ->Height();
   iX = iX % (int)pImageZ->Width();
   iY = iY % (int)pImageZ->Height();
   PIMAGEPIXEL pip = pImageZ->Pixel ((DWORD)iX, (DWORD)iY);
   p1.p[0] = fH * (fp)pImage->Width();
   p1.p[1] = fV * (fp)pImage->Height();
   p1.p[2] = pip->fZ;   // start at current skin level

   // ending point
   p2.Copy (&p1);
   p2.p[2] += fLength * sin(fAngle);
   pDir.Scale (cos(fAngle) * fLength);
   p2.Add (&pDir);

   // draw the hair
   DrawHairStraight (pImage, pImage2, awColor, afGlow, &p1, &p2, m_pHair.p[1] / fPixelLen, &MatTemp);

   return TRUE;
}




/******************************************************************************
DrawHairStraight - This draws a straight piece of hair

inputs
   PCImage     pImage - Image to draw to
   PCImage2    pImage2 - For glow
   WORD        *pawColor - Pointer to an array of 3 colors
   float       *pafGlow - Glow colors
   PCPoint     p1 - First point. p[0] is the X
               value (in pixels), p[1] is the Y value in pixels.
               H is modulo the width of the image. V is NOT.
               p[2] is the z value.
   PCPoint     p2 - Second point. Like p1.
   fp          fWidth - Width of the hair in pixels
   PCMaterial  pMaterial - If not NULL then used for the hair material
returns
   none
*/
void DrawHairStraight (PCImage pImage, PCImage pImage2, WORD *pawColor, float *pafGlow, PCPoint p1, PCPoint p2, fp fWidth,
                       PCMaterial pMaterial)
{
   // which is the longest dimension
   CPoint pDelta;
   pDelta.Subtract (p2, p1);
   DWORD dwLongest = (fabs(pDelta.p[0]) > fabs(pDelta.p[1])) ? 0 : 1;
   DWORD dwShortest = 1 - dwLongest;

   // get the width and height
   DWORD dwWidth = pImage->Width();
   DWORD dwHeight = pImage->Height();
   BOOL fGlow = pafGlow[0] || pafGlow[1] || pafGlow[2];

   // always go from left to right or top bot bottom... increasing X or Y
   BOOL fFlipped = FALSE;
   if (p1->p[dwLongest] > p2->p[dwLongest]) {
      PCPoint pTemp;
      pTemp = p1;
      p1 = p2;
      p2 = pTemp;
      fFlipped = TRUE;
   }

   // new delta
   pDelta.Subtract (p2, p1);

   // loop over the longest dimension
   int iL, iLMax;
   DWORD i;
   fp fStart = p1->p[dwLongest];
   fp fLengthInv = pDelta.p[dwLongest] ? (1.0 / pDelta.p[dwLongest]) : 1;
   iLMax = (int)floor(p2->p[dwLongest]);
   fWidth /= 2.0; // so have something
   for (iL = (int)ceil(fStart); iL <= iLMax; iL++) {
      fp fAlpha = ((fp)iL - fStart) * fLengthInv;

      fp fOtherDim = fAlpha * pDelta.p[dwShortest] + p1->p[dwShortest];
      fp fZ = fAlpha * pDelta.p[2] + p1->p[2];

      // make it come to a point
      fp fWidthCur = fWidth;
      if (fFlipped && (fAlpha < 0.3))
         fWidthCur *= (fAlpha / 0.3);  // so comes to point
      else if (!fFlipped && (fAlpha > 0.7))
         fWidthCur *= (1.0 - fAlpha) / 0.3;  // so comes to a point
      
      // loop across the width
      int iS, iSMax;
      fp fSStart = fOtherDim - fWidthCur;
      fp fSEnd = fOtherDim + fWidthCur;
      fp fSEndMinusOne = fSEnd - 1.0;
      iSMax = (int)ceil(fSEnd);
      for (iS = (int)floor(fSStart); iS <= iSMax; iS++) {
         // determine the pixel
         int iX = dwLongest ? iS : iL;
         int iY = dwLongest ? iL : iS;
         if ((iY < 0) || (iY >= (int)dwHeight))
            continue;   // dont modulo vertical
         while (iX < 0)
            iX += (int)dwWidth;
         iX = iX % (int)dwWidth;

         // how much weight
         fp fWeight, fZWant;
         fp fis = (fp)iS;
         if (fis < fSStart) {
            fWeight = 1.0 - (fSStart - fis);
            if (fWeight <= 0)
               continue;
         } else if (fis <= fSEndMinusOne)
            fWeight = 1.0; // fuly drawn
         else {
            fWeight = fSEnd - fis; // 1.0 - (fis - fSEndMinusOne);
            if (fWeight <= 0)
               continue;
         }

         // figure out z, not doing full round equation though, but should be good enough
         fZWant = fabs(fis - fOtherDim);
         fZWant = min(fZWant, fWidthCur); // never more than cur with
         fZWant = fZ - fZWant;

         // get the pixel
         PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iX, (DWORD)iY);

         // if less than what's there don't draw...
         if (fZWant <= pip->fZ)
            continue;   // below level

         // else, draw
         pip->fZ = fZWant; // keep z
         if (fWeight >= 1.0) {
            pip->wRed = pawColor[0];
            pip->wGreen = pawColor[1];
            pip->wBlue = pawColor[2];
         }
         else {
            // blend in
            pip->wRed = (WORD)(fWeight * (fp)pawColor[0] + (1.0 - fWeight) * (fp)pip->wRed);
            pip->wGreen = (WORD)(fWeight * (fp)pawColor[1] + (1.0 - fWeight) * (fp)pip->wGreen);
            pip->wBlue = (WORD)(fWeight * (fp)pawColor[2] + (1.0 - fWeight) * (fp)pip->wBlue);
         }

         // glow
         if (fGlow) {
            PIMAGEPIXEL pip2 = pImage2->Pixel ((DWORD)iX, (DWORD)iY);
            WORD *paw = &pip2->wRed;
            fp f;
            for (i = 0; i < 3; i++) {
               f = (1.0 - fWeight) * (fp)paw[i] + fWeight * pafGlow[i] / DEFGLOWSCALE;
               f = max(f, 0);
               f = min(f, (fp)0xffff);
               paw[i] = (WORD)f;
            } // i
         } // if fGlow

         // NOTE: Always set to material, even if blurring in edges
         if (pMaterial)
            pip->dwID = MAKELONG (pMaterial->m_wSpecExponent, pMaterial->m_wSpecReflect);
      } // iS
   } // iL

   // done
}





/****************************************************************************
CFaceScars::Constructor and destructor
*/
CFaceScars::CFaceScars (void)
{
   Clear();
}

CFaceScars::~CFaceScars (void)
{
   // do nothing for now
}


static PWSTR gpszScars = L"Scars";
static PWSTR gpszAffectSpecularity = L"AffectSpecularity";
static PWSTR gpszAffectFur = L"AffectFur";
static PWSTR gpszHeight = L"Height";
static PWSTR gpszColoration = L"Coloration";
static PWSTR gpszDiscolor = L"Discolor";

/****************************************************************************
CFaceScars::MMLTo - Standard API
*/
PCMMLNode2 CFaceScars::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszScars);

   MMLValueSet (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   MMLValueSet (pNode, gpszAffectSpecularity, (int)m_fAffectSpecularity);
   MMLValueSet (pNode, gpszAffectFur, (int)m_fAffectFur);

   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszColoration, m_fColoration);


   PCMMLNode2 pSub;
   pSub = m_Coverage.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszCoverage);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Discolor.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDiscolor);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/****************************************************************************
CFaceScars::MMLFrom - Standard API
*/
BOOL CFaceScars::MMLFrom (PCMMLNode2 pNode)
{
   // clear existing
   Clear();

   MMLValueGetPoint (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   m_fAffectSpecularity = (BOOL) MMLValueGetInt (pNode, gpszAffectSpecularity, TRUE);
   m_fAffectFur = (BOOL) MMLValueGetInt (pNode, gpszAffectFur, FALSE);

   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, -.001);
   m_fColoration = MMLValueGetDouble (pNode, gpszColoration, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszCoverage)) {
         m_Coverage.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszDiscolor)) {
         m_Discolor.MMLFrom (pSub);
         continue;
      }
   } // i


   return TRUE;
}


/****************************************************************************
CFaceScars::Clear - Standard API
*/
void CFaceScars::Clear (void)
{
   m_Coverage.Clear();
   m_Discolor.Clear();
   m_Coverage.StretchToFitSet (TRUE);
   m_Discolor.StretchToFitSet (FALSE);
   m_pCoverageAmt.Zero();
   m_pCoverageAmt.p[0] = m_pCoverageAmt.p[2] = 0;
   m_pCoverageAmt.p[1] = 1;
   m_fAffectSpecularity = FALSE;
   m_fAffectFur = FALSE;

   m_fHeight = -0.001;
   m_fColoration = 0;
}

/****************************************************************************
CFaceScars::ScaleSet - Sets the width and height of the master texture
in meters. You should call this to ensure any textures that need distance
information will use the right scale.

inputs
   fp       fMasterWidth, fMasterHeight - Width and height in meters
*/
void CFaceScars::ScaleSet (fp fMasterWidth, fp fMasterHeight)
{
   m_Coverage.ScaleSet (fMasterWidth, fMasterHeight);
   m_Discolor.ScaleSet (fMasterWidth, fMasterHeight);
}

/****************************************************************************
CFaceScars::Clone - Standard API
*/
CFaceScars *CFaceScars::Clone (void)
{
   // NOTE: Not tested
   PCFaceScars pNew = new CFaceScars;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/****************************************************************************
CFaceScars::CloneTo - Standard API
*/
BOOL CFaceScars::CloneTo (CFaceScars *pTo)
{
   m_Coverage.CloneTo (&pTo->m_Coverage);
   m_Discolor.CloneTo (&pTo->m_Discolor);
   pTo->m_pCoverageAmt.Copy (&m_pCoverageAmt);
   pTo->m_fAffectSpecularity = m_fAffectSpecularity;
   pTo->m_fAffectFur = m_fAffectFur;
   pTo->m_fHeight = m_fHeight;
   pTo->m_fColoration = m_fColoration;
   return TRUE;
}


/****************************************************************************
CFaceScars::TextureQuery - Standard API
*/
BOOL CFaceScars::TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree)
{
   m_Coverage.TextureQuery (dwRenderShard, plText, pTree);
   m_Discolor.TextureQuery (dwRenderShard, plText, pTree);
   return TRUE;
}


/****************************************************************************
CFaceScars::SubTextureNoRecurse - Standard API
*/
BOOL CFaceScars::SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText)
{
   if (!m_Coverage.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   if (!m_Discolor.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   return TRUE;
}


/****************************************************************************
CFaceScars::IsOverScar - Returns a value indicating how much the XY
texture is ove a scar.

NOTE: Assumes the m_Coverage texture is cached.

inputs
   PTEXTPOINT5       pLoc - .hv[0] and .hv[1] need to be filled in with 0..1
returns
   fp - How much over scar. 1.0 = fully over scar. 0 = not
*/
fp CFaceScars::IsOverScar (PTEXTPOINT5 pLoc)
{
   TEXTPOINT5 tpMax;
   memset (&tpMax, 0, sizeof(tpMax));

   // get the color at this point
   fp fCoverage;
   WORD awColor[3];
   CMaterial MatTemp;
   float afGlow[3];
   if (m_Coverage.FillPixel (0, TMFP_ALL, &awColor[0], pLoc, &tpMax, &MatTemp, &afGlow[0], TRUE)) {   // want high quality for thin lines
      fCoverage = ((fp)awColor[0] + (fp)awColor[1] + (fp)awColor[2]) / ((fp)0xffff * 3.0);
      fCoverage = fCoverage * m_pCoverageAmt.p[1] + (1.0 - fCoverage) * m_pCoverageAmt.p[0];
   }
   else
      fCoverage = m_pCoverageAmt.p[2];
   fCoverage = max(fCoverage, 0);
   fCoverage = min(fCoverage, 1);
   return fCoverage;
}

/****************************************************************************
CFaceScars::EscMultiThreadedCallback - Multithreaded rendering.
*/
void CFaceScars:: EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
   DWORD dwThread)
{
   PMTFACEINFO pmti = (PMTFACEINFO) pParams;
   PCTextCreatorFaceomatic pFace = pmti->pFace;
   PCImage pImage = pmti->pImage;
   PCImage pImage2 = pmti->pImage2;
   PCImage pImageZ = pmti->pImageZ;
   DWORD dwRenderShard = pmti->dwRenderShard;


   // loop over all the wart locations
   TEXTPOINT5 tpCur, tpMax;
   CMaterial MatTemp;
   float afGlow[3];
   WORD awColor[3];
   DWORD x,y, i;
   fp f;
   PIMAGEPIXEL pip, pip2;
   memset (&tpMax, 0, sizeof(tpMax));
   memset (&tpCur, 0, sizeof(tpCur));
   pip = pImage->Pixel(0,pmti->dwStart);
   pip2 = pImage2->Pixel(0,pmti->dwStart);
   fp fPixelLen = TextureDetailApply(dwRenderShard, pFace->m_fPixelLen);
   fp fPixelLenInv = 1.0 / fPixelLen;

   for (y = pmti->dwStart; y < pmti->dwEnd; y++) {
      tpCur.hv[1] = (fp)y / (fp)pImage->Height();
      tpCur.xyz[1] = tpCur.hv[1] * pFace->m_fHeight;

      for (x = 0; x < pImage->Width(); x++, pip++, pip2++) {
         tpCur.hv[0] = (fp)x / (fp)pImage->Width();
         tpCur.xyz[0] = tpCur.hv[0] * pFace->m_fWidth;

         fp fCoverage = IsOverScar (&tpCur);
         if (fCoverage <= 0.0)
            continue;   // nothing

         // affect z
         pip->fZ += fCoverage * m_fHeight * fPixelLenInv;

         // get the pixel for the discoloration
         if (m_fColoration <= 0)
            continue;
         fCoverage *= m_fColoration;
         m_Discolor.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], TRUE);

         // blend in the discoloration
         WORD *paw = &pip->wRed;
         WORD *paw2 = &pip2->wRed;
         for (i = 0; i < 3; i++) {
            paw[i] = (WORD)(fCoverage * (fp)awColor[i] + (1.0 - fCoverage) * (fp)paw[i]);

            // glow
            f = (fp)paw2[i] + afGlow[i] / DEFGLOWSCALE;
            f = max(f, 0);
            f = min(f, (fp)0xffff);
            paw2[i] = (WORD)f;
         } // i

         // blend in the specularity
         if (m_fAffectSpecularity) {
            WORD wExponent = LOWORD(pip->dwID);
            WORD wReflect = HIWORD(pip->dwID);

            wExponent = (WORD)(fCoverage * (fp)MatTemp.m_wSpecExponent + (1.0 - fCoverage) * (fp)wExponent);
            wReflect = (WORD)(fCoverage * (fp)MatTemp.m_wSpecReflect + (1.0 - fCoverage) * (fp)wReflect);

            pip->dwID = MAKELONG (wExponent, wReflect);
         } // affect speculary
      } // x
   } // y
}




/****************************************************************************
CFaceScars::Render - Draws the Scars on

inputs
   PCImage        pImage - Main color image, from main Render() call
   PCImage        pImage2 - Glow image, from main Render() call.
   PCTextCreatorFaceomatic pFace - Face object, in case need it
*/
BOOL CFaceScars::Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace)
{
   // if not on dont bother
   if (!m_Coverage.m_fUse)
      return TRUE;


   // cachj
   m_Coverage.TextureCache (dwRenderShard);
   m_Discolor.TextureCache(dwRenderShard);

   // multithreaded makeup
   MTFACEINFO mtfi;
   memset (&mtfi, 0, sizeof(mtfi));
   mtfi.pFace = pFace;
   mtfi.pImage = pImage;
   mtfi.pImage2 = pImage2;
   mtfi.dwRenderShard = dwRenderShard;
   // mtfi.pImageZ = pImageZ;
   ThreadLoop (0, pImage->Height(), 3, &mtfi, sizeof(mtfi));



   // uncache
   m_Coverage.TextureRelease();
   m_Discolor.TextureRelease();


   return TRUE;
}


/****************************************************************************
FaceomaticScarsPage
*/
BOOL FaceomaticScarsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCFaceScars pv = (PCFaceScars) pt->pThis;

   // call sub-textures for trap
   if (pv->m_Coverage.PageTrapMessages (pt->dwRenderShard, L"Coverage", pPage, dwMessage, pParam))
      return TRUE;
   if (pv->m_Discolor.PageTrapMessages (pt->dwRenderShard, L"Discolor", pPage, dwMessage, pParam))
      return TRUE;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;


   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[64];
         DWORD i;

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"shiny%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pCoverageAmt.p[i]);
         } // i

         MeasureToString (pPage, L"height", pv->m_fHeight, TRUE);
         DoubleToControl (pPage, L"coloration", pv->m_fColoration);

         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"AffectSpecularity"))
            pControl->AttribSetBOOL (Checked(), pv->m_fAffectSpecularity);
         if (pControl = pPage->ControlFind (L"AffectFur"))
            pControl->AttribSetBOOL (Checked(), pv->m_fAffectFur);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // do nothing
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"AffectSpecularity")) {
            pv->m_fAffectSpecularity = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         if (!_wcsicmp(psz, L"AffectFur")) {
            pv->m_fAffectFur = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszShiny = L"shiny";
         DWORD dwShinyLen = (DWORD)wcslen(pszShiny);
         if (!_wcsnicmp (psz, pszShiny, dwShinyLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwShinyLen);
            if (dwNum >= 3)
               break;    // error
            pv->m_pCoverageAmt.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"height")) {
            MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_fHeight);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"coloration")) {
            pv->m_fColoration = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Scars and wrinkles";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
CFaceScars::Dialog - Brings up a dialog for modifying the Scars.

inputs
   PCEscWindow       pWindow - Window
   PCTextCreatorFaceomatic pFace - Face texture
returns
   BOOL - TRUE if pressed back. FALSE if closed texture
*/
BOOL CFaceScars::Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pFace;
   ti.pThis = this;
   ti.fDrawFlat = TRUE;
   ti.fStretchToFit = TRUE;
   ti.dwRenderShard = dwRenderShard;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFACEOMATICSCARS, FaceomaticScarsPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   return pszRet && !_wcsicmp(pszRet, Back());
}








/****************************************************************************
CFaceWarts::Constructor and destructor
*/
CFaceWarts::CFaceWarts (void)
{
   Clear();
}

CFaceWarts::~CFaceWarts (void)
{
   // do nothing for now
}


static PWSTR gpszWarts = L"Warts";
//static PWSTR gpszCoverageAmt = L"CoverageAmt";
//static PWSTR gpszAffectSpecularity = L"AffectSpecularity";
//static PWSTR gpszCoverage = L"Coverage";
// static PWSTR gpszSize = L"Size";
//static PWSTR gpszHeight = L"Height";
//static PWSTR gpszColoration = L"Coloration";
// static PWSTR gpszSeed = L"Seed";
//static PWSTR gpszDiscolor = L"Discolor";

/****************************************************************************
CFaceWarts::MMLTo - Standard API
*/
PCMMLNode2 CFaceWarts::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszWarts);

   MMLValueSet (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   MMLValueSet (pNode, gpszSize, &m_pSize);
   MMLValueSet (pNode, gpszHeight, &m_pHeight);
   MMLValueSet (pNode, gpszColoration, &m_pColoration);
   MMLValueSet (pNode, gpszAffectSpecularity, (int)m_fAffectSpecularity);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);


   PCMMLNode2 pSub;
   pSub = m_Coverage.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszCoverage);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Discolor.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszDiscolor);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/****************************************************************************
CFaceWarts::MMLFrom - Standard API
*/
BOOL CFaceWarts::MMLFrom (PCMMLNode2 pNode)
{
   // clear existing
   Clear();

   MMLValueGetPoint (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   MMLValueGetPoint (pNode, gpszSize, &m_pSize);
   MMLValueGetPoint (pNode, gpszHeight, &m_pHeight);
   MMLValueGetPoint (pNode, gpszColoration, &m_pColoration);
   m_fAffectSpecularity = (BOOL) MMLValueGetInt (pNode, gpszAffectSpecularity, TRUE);
   m_iSeed = MMLValueGetInt (pNode, gpszSeed, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszCoverage)) {
         m_Coverage.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszDiscolor)) {
         m_Discolor.MMLFrom (pSub);
         continue;
      }
   } // i


   return TRUE;
}


/****************************************************************************
CFaceWarts::Clear - Standard API
*/
void CFaceWarts::Clear (void)
{
   m_Coverage.Clear();
   m_Discolor.Clear();
   m_Coverage.StretchToFitSet (TRUE);
   m_Discolor.StretchToFitSet (FALSE);
   m_pCoverageAmt.Zero();
   m_pCoverageAmt.p[0] = m_pCoverageAmt.p[2] = 0;
   m_pCoverageAmt.p[1] = 1;
   m_fAffectSpecularity = FALSE;

   m_pSize.Zero();
   m_pSize.p[0] = .01;
   m_pSize.p[1] = .005;
   m_pSize.p[2] = .5;

   m_pHeight.Zero();
   m_pHeight.p[0] = 0.003;
   m_pHeight.p[1] = 1;

   m_pColoration.Zero();
   m_pColoration.p[0] = 1;
   m_pColoration.p[1] = 1;

   m_iSeed = 0;
}

/****************************************************************************
CFaceWarts::ScaleSet - Sets the width and height of the master texture
in meters. You should call this to ensure any textures that need distance
information will use the right scale.

inputs
   fp       fMasterWidth, fMasterHeight - Width and height in meters
*/
void CFaceWarts::ScaleSet (fp fMasterWidth, fp fMasterHeight)
{
   m_Coverage.ScaleSet (fMasterWidth, fMasterHeight);
   m_Discolor.ScaleSet (fMasterWidth, fMasterHeight);
}

/****************************************************************************
CFaceWarts::Clone - Standard API
*/
CFaceWarts *CFaceWarts::Clone (void)
{
   // NOTE: Not tested
   PCFaceWarts pNew = new CFaceWarts;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/****************************************************************************
CFaceWarts::CloneTo - Standard API
*/
BOOL CFaceWarts::CloneTo (CFaceWarts *pTo)
{
   m_Coverage.CloneTo (&pTo->m_Coverage);
   m_Discolor.CloneTo (&pTo->m_Discolor);
   pTo->m_pCoverageAmt.Copy (&m_pCoverageAmt);
   pTo->m_pSize.Copy (&m_pSize);
   pTo->m_pHeight.Copy (&m_pHeight);
   pTo->m_pColoration.Copy (&m_pColoration);
   pTo->m_fAffectSpecularity = m_fAffectSpecularity;
   pTo->m_iSeed = m_iSeed;
   return TRUE;
}


/****************************************************************************
CFaceWarts::TextureQuery - Standard API
*/
BOOL CFaceWarts::TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree)
{
   m_Coverage.TextureQuery (dwRenderShard, plText, pTree);
   m_Discolor.TextureQuery (dwRenderShard, plText, pTree);
   return TRUE;
}


/****************************************************************************
CFaceWarts::SubTextureNoRecurse - Standard API
*/
BOOL CFaceWarts::SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText)
{
   if (!m_Coverage.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   if (!m_Discolor.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   return TRUE;
}


/****************************************************************************
CFaceWarts::RenderWart - Draws a wart

inputs
   PCImage        pImage - Main color image, from main Render() call
   PCImage        pImage2 - Glow image, from main Render() call.
   PCTextCreatorFaceomatic pFace - Face object, in case need it
   fp             fX - X location, in meters
   fp             fY - Y location, in meters
   fp             fDiameter - Diameter of the wart, in meters
   fp             fHeight - Height of the wart, in meters
   fp             fColoration - Coloration density (1.0 = max) in meters.
returns
   BOOL - TRUE if success
*/
BOOL CFaceWarts::RenderWart (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace,
                             fp fX, fp fY, fp fDiameter, fp fHeight, fp fColoration)
{
   // convert the x, y, and diameter into pixels
   fp fPixelLen = TextureDetailApply(dwRenderShard, pFace->m_fPixelLen);
   fX /= fPixelLen;
   fY /= fPixelLen;
   fDiameter /= fPixelLen;
   fp fRadius = fDiameter / 2;
   fp fRadiusInv = fRadius ? (1.0 / fRadius) : 1;
   fp fPixelLenInv = 1.0 / fPixelLen;

   // determine the pixels its in
   RECT r;
   r.left = floor(fX - fRadius);
   r.right = ceil(fX + fRadius);
   r.top = floor(fY - fRadius);
   r.bottom = ceil(fY + fRadius);

   // loop
   int iX, iY;
   DWORD i;
   WORD awColor[3];
   float afGlow[3];
   TEXTPOINT5 tpCur, tpMax;
   CMaterial MatTemp;
   memset (&tpCur, 0, sizeof(tpCur));
   memset (&tpMax, 0, sizeof(tpMax));
   for (iY = r.top; iY <= r.bottom; iY++) for (iX = r.left; iX <= r.right; iX++) {
      fp fXDist = ((fp)iX - fX) * fRadiusInv;
      fp fYDist = ((fp)iY - fY) * fRadiusInv;
      fp fDist = sqrt(fXDist * fXDist + fYDist * fYDist);
      if (fDist >= 1.0)
         continue;   // out of range
      fDist = cos(fDist * PI/2.0); // so is 1.0 at center, and is smooth
      fDist *= fDist;   // so use cos-square, and nice and smooth

      // figure out height and coloration by pointyness
      fp fCurHeight, fCurColoration;
      fCurHeight = fHeight * pow(fDist, m_pHeight.p[1]);
      fCurColoration = fColoration * pow(fDist, m_pColoration.p[1]);

      // figure out x,y in image
      int iRealX, iRealY;
      iRealX = iX;
      while (iRealX < 0)
         iRealX += (int)pImage->Width();
      iRealX = iRealX % (int)pImage->Width();
      iRealY = iY;
      while (iRealY < 0)
         iRealY += (int)pImage->Height();
      iRealY = iRealY % (int)pImage->Height();

      // get the pixels
      PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iRealX, (DWORD)iRealY);
      PIMAGEPIXEL pip2 = pImage2->Pixel ((DWORD)iRealX, (DWORD)iRealY);

      // adjust the height
      pip->fZ += fPixelLenInv * fCurHeight;

      // adjust the coloration
      if (fCurColoration <= 0)
         continue;   // no coloration
      fCurColoration = min(fCurColoration, 1);

      // get the pixel for the Warts
      tpCur.hv[0] = (fp)iRealX / (fp)pImage->Width();
      tpCur.hv[1] = (fp)iRealY / (fp)pImage->Height();
      tpCur.xyz[0] = tpCur.hv[0] * pFace->m_fWidth;
      tpCur.xyz[1] = tpCur.hv[1] * pFace->m_fHeight;
      m_Discolor.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], TRUE);

      // blend in the Warts
      WORD *paw = &pip->wRed;
      WORD *paw2 = &pip2->wRed;
      for (i = 0; i < 3; i++) {
         paw[i] = (WORD)(fCurColoration * (fp)awColor[i] + (1.0 - fCurColoration) * (fp)paw[i]);

         // glow
         fp f;
         f = (fp)paw2[i] + afGlow[i] / DEFGLOWSCALE;
         f = max(f, 0);
         f = min(f, (fp)0xffff);
         paw2[i] = (WORD)f;
      } // i

      // blend in the specularity
      if (m_fAffectSpecularity) {
         WORD wExponent = LOWORD(pip->dwID);
         WORD wReflect = HIWORD(pip->dwID);

         wExponent = (WORD)(fCurColoration * (fp)MatTemp.m_wSpecExponent + (1.0 - fCurColoration) * (fp)wExponent);
         wReflect = (WORD)(fCurColoration * (fp)MatTemp.m_wSpecReflect + (1.0 - fCurColoration) * (fp)wReflect);

         pip->dwID = MAKELONG (wExponent, wReflect);
      } // affect speculary
   } // iX, iY

   return TRUE;
}



/****************************************************************************
CFaceWarts::Render - Draws the Warts on

inputs
   PCImage        pImage - Main color image, from main Render() call
   PCImage        pImage2 - Glow image, from main Render() call.
   PCTextCreatorFaceomatic pFace - Face object, in case need it
*/
BOOL CFaceWarts::Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace)
{
   // if not on dont bother
   if (!m_Coverage.m_fUse)
      return TRUE;


   // cachj
   m_Coverage.TextureCache (dwRenderShard);
   m_Discolor.TextureCache(dwRenderShard);

   // see random AFTER cache textures
   srand (m_iSeed);

   // loop over all the wart locations
   TEXTPOINT5 tpCur, tpMax;
   CMaterial MatTemp;
   float afGlow[3];
   WORD awColor[3];
   m_pSize.p[0] = max(m_pSize.p[0], CLOSE);
   memset (&tpMax, 0, sizeof(tpMax));
   memset (&tpCur, 0, sizeof(tpCur));
   fp fX, fY;
   for (fY = 0.0; fY < pFace->m_fHeight; fY += m_pSize.p[0]) {
      tpCur.hv[1] = fY / pFace->m_fHeight;
      tpCur.xyz[1] = fY;

      for (fX = 0.0; fX < pFace->m_fWidth; fX += m_pSize.p[0]) {
         tpCur.hv[0] = fX / pFace->m_fWidth;
         tpCur.xyz[0] = fX;

         // get the color at this point
         fp fCoverage;
         if (m_Coverage.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], FALSE)) {   // fast
            fCoverage = ((fp)awColor[0] + (fp)awColor[1] + (fp)awColor[2]) / ((fp)0xffff * 3.0);
            fCoverage = fCoverage * m_pCoverageAmt.p[1] + (1.0 - fCoverage) * m_pCoverageAmt.p[0];
         }
         else
            fCoverage = m_pCoverageAmt.p[2];
         if (fCoverage <= 0.0)
            continue;   // nothing

         // random
         if (((fp)(rand()%1000) / 1000.0) >= fCoverage)
            continue;   // nothing here

         // else, have wart, figure out some info
         fp fHeight = m_pHeight.p[0] * MyRand (1.0 - m_pSize.p[2], 1.0 + m_pSize.p[2]);
         fp fColoration = m_pColoration.p[0] * MyRand (1.0 - m_pSize.p[2], 1.0 + m_pSize.p[2]);
         fp fDiameter = m_pSize.p[1] * MyRand (1.0 - m_pSize.p[2], 1.0 + m_pSize.p[2]);

         // call in
         RenderWart (dwRenderShard, pImage, pImage2, pFace,
            fX + MyRand(-m_pSize.p[0]/2.0, m_pSize.p[0]/2.0),
            fY + MyRand(-m_pSize.p[0]/2.0, m_pSize.p[0]/2.0),
            fDiameter, fHeight, fColoration);
      } // fX
   } // fY

   // uncache
   m_Coverage.TextureRelease();
   m_Discolor.TextureRelease();


   return TRUE;
}


/****************************************************************************
FaceomaticWartsPage
*/
BOOL FaceomaticWartsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCFaceWarts pv = (PCFaceWarts) pt->pThis;

   // call sub-textures for trap
   if (pv->m_Coverage.PageTrapMessages (pt->dwRenderShard, L"Coverage", pPage, dwMessage, pParam))
      return TRUE;
   if (pv->m_Discolor.PageTrapMessages (pt->dwRenderShard, L"Discolor", pPage, dwMessage, pParam))
      return TRUE;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[64];
         DWORD i;

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"shiny%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pCoverageAmt.p[i]);

            swprintf (szTemp, L"size%d", (int) i);
            if (i <= 1)
               MeasureToString (pPage, szTemp, pv->m_pSize.p[i], TRUE);
            else
               DoubleToControl (pPage, szTemp, pv->m_pSize.p[i]);

            swprintf (szTemp, L"height%d", (int) i);
            if (!i)
               MeasureToString (pPage, szTemp, pv->m_pHeight.p[i], TRUE);
            else
               DoubleToControl (pPage, szTemp, pv->m_pHeight.p[i]);

            swprintf (szTemp, L"coloration%d", (int) i);
            DoubleToControl (pPage, szTemp, pv->m_pColoration.p[i]);
         } // i

         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"AffectSpecularity"))
            pControl->AttribSetBOOL (Checked(), pv->m_fAffectSpecularity);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // do nothing
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"NewSeed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         if (!_wcsicmp(psz, L"AffectSpecularity")) {
            pv->m_fAffectSpecularity = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszShiny = L"shiny", pszSize = L"size", pszHeight = L"height", pszColoration = L"coloration";
         DWORD dwShinyLen =(DWORD) wcslen(pszShiny), dwSizeLen = (DWORD)wcslen(pszSize),
            dwHeightLen = (DWORD)wcslen(pszHeight), dwColorationLen = (DWORD)wcslen(pszColoration);
         if (!_wcsnicmp (psz, pszShiny, dwShinyLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwShinyLen);
            if (dwNum >= 3)
               break;    // error
            pv->m_pCoverageAmt.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszSize, dwSizeLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwSizeLen);
            if (dwNum >= 3)
               break;    // error
            if (dwNum <= 1)
               MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_pSize.p[dwNum]);
            else
               pv->m_pSize.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszHeight, dwHeightLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwHeightLen);
            if (dwNum >= 3)
               break;    // error
            if (!dwNum)
               MeasureParseString (pPage, p->pControl->m_pszName, &pv->m_pHeight.p[dwNum]);
            else
               pv->m_pHeight.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
         else if (!_wcsnicmp (psz, pszColoration, dwColorationLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwColorationLen);
            if (dwNum >= 3)
               break;    // error
            pv->m_pColoration.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Warts and pores";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
CFaceWarts::Dialog - Brings up a dialog for modifying the Warts.

inputs
   PCEscWindow       pWindow - Window
   PCTextCreatorFaceomatic pFace - Face texture
returns
   BOOL - TRUE if pressed back. FALSE if closed texture
*/
BOOL CFaceWarts::Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pFace;
   ti.pThis = this;
   ti.fDrawFlat = TRUE;
   ti.fStretchToFit = TRUE;
   ti.dwRenderShard = dwRenderShard;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFACEOMATICWARTS, FaceomaticWartsPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   return pszRet && !_wcsicmp(pszRet, Back());
}








/****************************************************************************
CFaceMakeup::Constructor and destructor
*/
CFaceMakeup::CFaceMakeup (void)
{
   Clear();
}

CFaceMakeup::~CFaceMakeup (void)
{
   // do nothing for now
}


static PWSTR gpszMakeup = L"Makeup";
static PWSTR gpszBeforeWarts = L"BeforeWarts";
static PWSTR gpszDiscoloration = L"Discoloration";
static PWSTR gpszDiscolorationScale = L"DiscolorationScale";

/****************************************************************************
CFaceMakeup::MMLTo - Standard API
*/
PCMMLNode2 CFaceMakeup::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszMakeup);

   MMLValueSet (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   MMLValueSet (pNode, gpszBeforeWarts, (int)m_fBeforeWarts);
   MMLValueSet (pNode, gpszAffectSpecularity, (int)m_fAffectSpecularity);
   MMLValueSet (pNode, gpszDiscoloration, (int)m_fDiscoloration);
   MMLValueSet (pNode, gpszDiscolorationScale, m_fDiscolorationScale);

   PCMMLNode2 pSub;
   pSub = m_Coverage.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszCoverage);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Makeup.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszMakeup);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/****************************************************************************
CFaceMakeup::MMLFrom - Standard API
*/
BOOL CFaceMakeup::MMLFrom (PCMMLNode2 pNode)
{
   // clear existing
   Clear();

   MMLValueGetPoint (pNode, gpszCoverageAmt, &m_pCoverageAmt);
   m_fBeforeWarts = (BOOL) MMLValueGetInt (pNode, gpszBeforeWarts, FALSE);
   m_fAffectSpecularity = (BOOL) MMLValueGetInt (pNode, gpszAffectSpecularity, TRUE);
   m_fDiscoloration = (BOOL) MMLValueGetInt (pNode, gpszDiscoloration, FALSE);
   m_fDiscolorationScale = MMLValueGetDouble (pNode, gpszDiscolorationScale, 1.0);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszCoverage)) {
         m_Coverage.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszMakeup)) {
         m_Makeup.MMLFrom (pSub);
         continue;
      }
   } // i


   return TRUE;
}


/****************************************************************************
CFaceMakeup::Clear - Standard API
*/
void CFaceMakeup::Clear (void)
{
   m_Coverage.Clear();
   m_Makeup.Clear();
   m_Coverage.StretchToFitSet (TRUE);
   m_Makeup.StretchToFitSet (FALSE);
   m_pCoverageAmt.Zero();
   m_pCoverageAmt.p[0] = m_pCoverageAmt.p[2] = 0;
   m_pCoverageAmt.p[1] = 1;
   m_fBeforeWarts = FALSE;
   m_fAffectSpecularity = TRUE;
   m_fDiscoloration = FALSE;
   m_fDiscolorationScale = 1.0;
}

/****************************************************************************
CFaceMakeup::ScaleSet - Sets the width and height of the master texture
in meters. You should call this to ensure any textures that need distance
information will use the right scale.

inputs
   fp       fMasterWidth, fMasterHeight - Width and height in meters
*/
void CFaceMakeup::ScaleSet (fp fMasterWidth, fp fMasterHeight)
{
   m_Coverage.ScaleSet (fMasterWidth, fMasterHeight);
   m_Makeup.ScaleSet (fMasterWidth, fMasterHeight);
}

/****************************************************************************
CFaceMakeup::Clone - Standard API
*/
CFaceMakeup *CFaceMakeup::Clone (void)
{
   // NOTE: Not tested
   PCFaceMakeup pNew = new CFaceMakeup;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/****************************************************************************
CFaceMakeup::CloneTo - Standard API
*/
BOOL CFaceMakeup::CloneTo (CFaceMakeup *pTo)
{
   m_Coverage.CloneTo (&pTo->m_Coverage);
   m_Makeup.CloneTo (&pTo->m_Makeup);
   pTo->m_pCoverageAmt.Copy (&m_pCoverageAmt);
   pTo->m_fBeforeWarts = m_fBeforeWarts;
   pTo->m_fAffectSpecularity = m_fAffectSpecularity;
   pTo->m_fDiscoloration = m_fDiscoloration;
   pTo->m_fDiscolorationScale = m_fDiscolorationScale;

   return TRUE;
}


/****************************************************************************
CFaceMakeup::TextureQuery - Standard API
*/
BOOL CFaceMakeup::TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree)
{
   m_Coverage.TextureQuery (dwRenderShard, plText, pTree);
   m_Makeup.TextureQuery (dwRenderShard, plText, pTree);
   return TRUE;
}


/****************************************************************************
CFaceMakeup::SubTextureNoRecurse - Standard API
*/
BOOL CFaceMakeup::SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText)
{
   if (!m_Coverage.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   if (!m_Makeup.SubTextureNoRecurse (dwRenderShard, plText))
      return FALSE;
   return TRUE;
}

/****************************************************************************
CFaceMakeup::EscMultiThreadedCallback - Multithreaded rendering.
*/
void CFaceMakeup:: EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
   DWORD dwThread)
{
   PMTFACEINFO pmti = (PMTFACEINFO) pParams;
   PCTextCreatorFaceomatic pFace = pmti->pFace;
   PCImage pImage = pmti->pImage;
   PCImage pImage2 = pmti->pImage2;
   PCImage pImageZ = pmti->pImageZ;
   DWORD dwRenderShard = pmti->dwRenderShard;


   DWORD x,y;
   TEXTPOINT5 tpCur, tpMax;
   PIMAGEPIXEL pip, pip2;
   CMaterial MatTemp;
   float afGlow[3];
   WORD awColor[3];
   fp f;
   memset (&tpMax, 0, sizeof(tpMax));
   memset (&tpCur, 0, sizeof(tpCur));

   // get the average color of the makeup
   WORD awAverage[3];
   CPoint pAverage, pDiff;
   Gamma (m_Makeup.AverageColorGet (dwThread, FALSE), awAverage);
   DWORD i;
   for (i = 0; i < 3; i++)
      pAverage.p[i] = awAverage[i];

   pip = pImage->Pixel(0,pmti->dwStart);
   pip2 = pImage2->Pixel(0,pmti->dwStart);
   for (y = pmti->dwStart; y < pmti->dwEnd; y++) {
      tpCur.hv[1] = (fp)y / (fp)pImage->Height();
      tpCur.xyz[1] = tpCur.hv[1] * pFace->m_fHeight;

      for (x = 0; x < pImage->Width(); x++, pip++, pip2++) {
         tpCur.hv[0] = (fp)x / (fp)pImage->Width();
         tpCur.xyz[0] = tpCur.hv[0] * pFace->m_fWidth;

         fp fCoverage;
         if (m_Coverage.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], TRUE)) {
            fCoverage = ((fp)awColor[0] + (fp)awColor[1] + (fp)awColor[2]) / ((fp)0xffff * 3.0);
            fCoverage = fCoverage * m_pCoverageAmt.p[1] + (1.0 - fCoverage) * m_pCoverageAmt.p[0];
         }
         else
            fCoverage = m_pCoverageAmt.p[2];
         if (fCoverage <= 0.0)
            continue;   // nothing

         // get the pixel for the makeup
         m_Makeup.FillPixel (0, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], TRUE);

         if (m_fDiscoloration) {
            for (i = 0; i < 3; i++)
               pDiff.p[i] = awColor[i];
            pDiff.Subtract (&pAverage);
            fp fLen = pDiff.Length() / (fp)0xffff;
            fCoverage *= fLen * m_fDiscolorationScale;
         } // distance
         if (fCoverage <= 0.0)
            continue;   // nothing
         fCoverage = min(fCoverage, 1.0);

         // blend in the makeup
         WORD *paw = &pip->wRed;
         WORD *paw2 = &pip2->wRed;
         for (i = 0; i < 3; i++) {
            paw[i] = (WORD)(fCoverage * (fp)awColor[i] + (1.0 - fCoverage) * (fp)paw[i]);

            // glow
            f = (fp)paw2[i] + afGlow[i] / DEFGLOWSCALE;
            f = max(f, 0);
            f = min(f, (fp)0xffff);
            paw2[i] = (WORD)f;
         } // i

         // blend in the specularity
         if (m_fAffectSpecularity) {
            WORD wExponent = LOWORD(pip->dwID);
            WORD wReflect = HIWORD(pip->dwID);

            wExponent = (WORD)(fCoverage * (fp)MatTemp.m_wSpecExponent + (1.0 - fCoverage) * (fp)wExponent);
            wReflect = (WORD)(fCoverage * (fp)MatTemp.m_wSpecReflect + (1.0 - fCoverage) * (fp)wReflect);

            pip->dwID = MAKELONG (wExponent, wReflect);
         } // affect speculary
      } // x
   } // y
}



/****************************************************************************
CFaceMakeup::Render - Draws the makeup on

inputs
   PCImage        pImage - Main color image, from main Render() call
   PCImage        pImage2 - Glow image, from main Render() call.
   PCTextCreatorFaceomatic pFace - Face object, in case need it
*/
BOOL CFaceMakeup::Render (DWORD dwRenderShard, PCImage pImage, PCImage pImage2, PCTextCreatorFaceomatic pFace)
{
   // if not on dont bother
   if (!m_Coverage.m_fUse)
      return TRUE;

   // loop over all pixels
   m_Coverage.TextureCache (dwRenderShard);
   m_Makeup.TextureCache(dwRenderShard);

   // multithreaded makeup
   MTFACEINFO mtfi;
   memset (&mtfi, 0, sizeof(mtfi));
   mtfi.pFace = pFace;
   mtfi.pImage = pImage;
   mtfi.pImage2 = pImage2;
   mtfi.dwRenderShard = dwRenderShard;
   // mtfi.pImageZ = pImageZ;
   ThreadLoop (0, pImage->Height(), 3, &mtfi, sizeof(mtfi));


   m_Coverage.TextureRelease();
   m_Makeup.TextureRelease();

   return TRUE;
}


/****************************************************************************
FaceomaticMakeupPage
*/
BOOL FaceomaticMakeupPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCFaceMakeup pv = (PCFaceMakeup) pt->pThis;

   // call sub-textures for trap
   if (pv->m_Coverage.PageTrapMessages (pt->dwRenderShard, L"Coverage", pPage, dwMessage, pParam))
      return TRUE;
   if (pv->m_Makeup.PageTrapMessages (pt->dwRenderShard, L"Makeup", pPage, dwMessage, pParam))
      return TRUE;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[64];
         DWORD i;

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"shiny%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pCoverageAmt.p[i]);
         } // i
         DoubleToControl (pPage, L"discolorationscale", pv->m_fDiscolorationScale);

         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"BeforeWarts"))
            pControl->AttribSetBOOL (Checked(), pv->m_fBeforeWarts);
         if (pControl = pPage->ControlFind (L"AffectSpecularity"))
            pControl->AttribSetBOOL (Checked(), pv->m_fAffectSpecularity);
         if (pControl = pPage->ControlFind (L"Discoloration"))
            pControl->AttribSetBOOL (Checked(), pv->m_fDiscoloration);
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // do nothing
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"BeforeWarts")) {
            pv->m_fBeforeWarts = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         if (!_wcsicmp(psz, L"AffectSpecularity")) {
            pv->m_fAffectSpecularity = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         if (!_wcsicmp(psz, L"Discoloration")) {
            pv->m_fDiscoloration = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszShiny = L"shiny";
         DWORD dwShinyLen = (DWORD)wcslen(pszShiny);
         if (!_wcsnicmp (psz, pszShiny, dwShinyLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwShinyLen);
            if (dwNum >= 3)
               break;    // error
            pv->m_pCoverageAmt.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         } // shunk
         else if (!_wcsicmp (psz, L"discolorationscale")) {
            pv->m_fDiscolorationScale = DoubleFromControl (pPage, p->pControl->m_pszName);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Makeup";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
CFaceMakeup::Dialog - Brings up a dialog for modifying the makeup.

inputs
   PCEscWindow       pWindow - Window
   PCTextCreatorFaceomatic pFace - Face texture
returns
   BOOL - TRUE if pressed back. FALSE if closed texture
*/
BOOL CFaceMakeup::Dialog (DWORD dwRenderShard, PCEscWindow pWindow, PCTextCreatorFaceomatic pFace)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pFace;
   ti.pThis = this;
   ti.fDrawFlat = TRUE;
   ti.fStretchToFit = TRUE;
   ti.dwRenderShard = dwRenderShard;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFACEOMATICMAKEUP, FaceomaticMakeupPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   return pszRet && !_wcsicmp(pszRet, Back());
}


/****************************************************************************
CSubTexture::Constructor and destructor
*/
CSubTexture::CSubTexture (void)
{
   m_pTexture = NULL;
   m_hBmp = NULL;
   m_pObjectSurface = NULL;
   Clear (TRUE);
}

CSubTexture::~CSubTexture (void)
{
   // calling clear will free stuff up
   Clear (TRUE, TRUE);
}


/****************************************************************************
CSubTexture::Clone - Standard API
*/
CSubTexture *CSubTexture::Clone (void)
{
   // NOTE: Not tested
   PCSubTexture pNew = new CSubTexture;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/****************************************************************************
CSubTexture::CloneTo - Standard API
*/
BOOL CSubTexture::CloneTo (CSubTexture *pTo)
{
   // NOTE: Not tested
   pTo->Clear (TRUE, TRUE);

   //if (pTo->m_pObjectSurface)  - wont need since cleared for delete
   //   delete pTo->m_pObjectSurface;
   pTo->m_pObjectSurface = m_pObjectSurface->Clone();

   pTo->m_atpBoundary[0] = m_atpBoundary[0];
   pTo->m_atpBoundary[1] = m_atpBoundary[1];
   pTo->m_dwMirror = m_dwMirror;
   pTo->m_fUse = m_fUse;

   pTo->m_fStretchToFit = m_fStretchToFit;
   pTo->m_tpMaster = m_tpMaster;

   // leave temporary stuff to be initialized later

   return TRUE;
}


static PWSTR gpszSubTexture = L"SubTexture";
static PWSTR gpszObjectSurface = L"ObjectSurface";
static PWSTR gpszBoundary0 = L"Boundary0";
static PWSTR gpszBoundary1 = L"Boundary1";
static PWSTR gpszMirror = L"Mirror";
// static PWSTR gpszUse = L"Use";


/****************************************************************************
CSubTexture::MMLTo - Standard API
*/
PCMMLNode2 CSubTexture::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszSubTexture);

   PCMMLNode2 pSub = m_pObjectSurface->MMLTo ();
   if (!pSub)
      return NULL;
   pSub->NameSet (gpszObjectSurface);
   pNode->ContentAdd (pSub);

   MMLValueSet (pNode, gpszBoundary0, &m_atpBoundary[0]);
   MMLValueSet (pNode, gpszBoundary1, &m_atpBoundary[1]);
   MMLValueSet (pNode, gpszMirror, (int)m_dwMirror);
   MMLValueSet (pNode, gpszUse, (int)m_fUse);

   return pNode;
}


/****************************************************************************
CSubTexture::MMLFrom - Standard API
*/
BOOL CSubTexture::MMLFrom (PCMMLNode2 pNode)
{
   // wipe out what have
   Clear ();

   PCMMLNode2 pSub = NULL;
   PWSTR psz;
   pNode->ContentEnum (pNode->ContentFind(gpszObjectSurface), &psz, &pSub);
   if (pSub)
      m_pObjectSurface->MMLFrom (pSub);

   MMLValueGetTEXTUREPOINT (pNode, gpszBoundary0, &m_atpBoundary[0]);
   MMLValueGetTEXTUREPOINT (pNode, gpszBoundary1, &m_atpBoundary[1]);
   m_dwMirror = (DWORD) MMLValueGetInt (pNode, gpszMirror, 0);
   m_fUse = (BOOL) MMLValueGetInt (pNode, gpszUse, TRUE);

   return TRUE;
}



/****************************************************************************
CSubTexture::Clear - Wipes out the contents of the sub-texture.

inputs
   BOOL        fClearAll - If TRUE even clears non-texture specific stuff like m_tpMaster
   BOOL        fShutDown - If TRUE then shutting down, so dont recreate
*/
void CSubTexture::Clear (BOOL fClearAll, BOOL fShutDown)
{
   if (m_pObjectSurface)
      delete m_pObjectSurface;
   if (fShutDown)
      m_pObjectSurface = NULL;
   else
      m_pObjectSurface = new CObjectSurface;  // to clear

   m_atpBoundary[0].h = m_atpBoundary[0].v = 0;
   m_atpBoundary[1].h = m_atpBoundary[1].v = 1;
   m_dwMirror = 0;
   m_fUse = FALSE; // not used by default

   if (m_pTexture) {
      m_pTexture->Delete();
      m_pTexture = NULL;
   }
   m_fBmpStretched = FALSE;
   m_fBmpSphere = FALSE;
   m_fBmpDirty = TRUE;
   m_dwBmpWidth = m_dwBmpHeight = 1;
   if (m_hBmp) {
      DeleteObject (m_hBmp);
      m_hBmp = NULL;
   }

   if (fClearAll) {
      m_fStretchToFit = TRUE;
      m_tpMaster.h = m_tpMaster.v = 1;
   }
}


/****************************************************************************
CSubTexture::DialogSelect - Pulls up a dialog that lets the user select
the texture to use.

inputs
   HWND           hWnd - Window
returns
   BOOL - TRUE if the texture was changed, FALSE if no change
*/
BOOL CSubTexture::DialogSelect (DWORD dwRenderShard, HWND hWnd)
{
   CWorld World;  // temporary world
   World.RenderShardSet (dwRenderShard);
   if (!TextureSelDialog (dwRenderShard, hWnd, m_pObjectSurface, &World))
      return FALSE; // no change

   // else, invalidate bitmap and cache
   TextureRelease();
   m_fBmpDirty = TRUE;
   return TRUE;
}


/****************************************************************************
CSubTexture::BoundaryGet - Gets the current boundary of the texture within
the master texture.

inputs
   PTEXTUREPOINT        ptpUL, ptpLR - Upper/left, lower/right texture
returns
   none
*/
void CSubTexture::BoundaryGet (PTEXTUREPOINT ptpUL, PTEXTUREPOINT ptpLR)
{
   if (ptpUL)
      *ptpUL = m_atpBoundary[0];
   if (ptpLR)
      *ptpLR = m_atpBoundary[1];
}



/****************************************************************************
CSubTexture::BoundarySet - Sets the current boundary of the texture within
the master texture.

inputs
   PTEXTUREPOINT        ptpUL, ptpLR - Upper/left, lower/right texture
returns
   none
*/
void CSubTexture::BoundarySet (PTEXTUREPOINT ptpUL, PTEXTUREPOINT ptpLR)
{
   m_atpBoundary[0] = *ptpUL;
   m_atpBoundary[1] = *ptpLR;
}



/****************************************************************************
CSubTexture::MirrorGet - Gets the current mirroring status of the
texture.

returns
   See MirrorSet()
*/
DWORD CSubTexture::MirrorGet (void)
{
   return m_dwMirror;
}



/****************************************************************************
CSubTexture::MirrorSet - Sets the current mirroring status.

inputs
   DWORD          dwMirror - If 0 then no mirroring, 1 then left side is a
                  mirror of the right, 2 then right is a mirror of the left,
                  3 mirrored left to right
returns
   BOOL - TRUE if success
*/
BOOL CSubTexture::MirrorSet (DWORD dwMirror)
{
   if (dwMirror > 3)
      return FALSE;

   m_dwMirror = dwMirror;
   return TRUE;
}


/****************************************************************************
CSubTexture::ScaleSet - Sets the width and height of the master texture
in meters. You should call this to ensure any textures that need distance
information will use the right scale.

inputs
   fp       fMasterWidth, fMasterHeight - Width and height in meters
*/
void CSubTexture::ScaleSet (fp fMasterWidth, fp fMasterHeight)
{
   m_tpMaster.h = fMasterWidth;
   m_tpMaster.v = fMasterHeight;
}


/****************************************************************************
CSubTexture::StretchToFitSet - Controls how the texture will be drawn onto
the master. If TRUE the texture is stretched out to fit within the boundary.
If FALSE, the measurements from ScaleSet() are used.

inputs
   BOOL           fStretchToFit - Whether to stretch or not
*/
void CSubTexture::StretchToFitSet (BOOL fStretchToFit)
{
   m_fStretchToFit = fStretchToFit;
}



/****************************************************************************
CSubTexture::TextureCache - Caches the texture specified in m_pObjectSurface.
You need to cache the surface before calling FillPixel() or PixelBump().

The texture should be released after it's cached

returns
   BOOL - TRUE if the texture was cached, or if it's a color and doesnt need
         to be cached
*/
BOOL CSubTexture::TextureCache (DWORD dwRenderShard)
{
   if (m_pTexture || !m_pObjectSurface->m_fUseTextureMap)
      return TRUE;

   // before create, make sure won't recurse
   CListFixed lText;
   lText.Init (sizeof(GUID)*2);
   if (!SubTextureNoRecurse (dwRenderShard, &lText))
      return FALSE;

   m_pTexture = TextureCreate (dwRenderShard, &m_pObjectSurface->m_gTextureCode, &m_pObjectSurface->m_gTextureSub);
   if (!m_pTexture)
      return FALSE;
   m_pTexture->TextureModsSet (&m_pObjectSurface->m_TextureMods);

   // force a cache, otherwise might get a deadlock with CEscMultiThreaded
   m_pTexture->ForceCache(FORCECACHE_ALL);

   return TRUE;
}


/****************************************************************************
CSubTexture::TextureRelease - Releases a texture loaded by texturecache.

returns
   BOOL - TRUE if there was something to release. FALSE if nothing
         was cached, so nothing released
*/
BOOL CSubTexture::TextureRelease (void)
{
   if (m_pTexture) {
      m_pTexture->Delete();
      m_pTexture = NULL;
      return TRUE;
   }
   return FALSE;
}


/****************************************************************************
CSubTexture::FillPixel - Like the standard FillPixel, except this takes
into account the stretching and mirroring. For this to work,
the texture must be cached.

NOTE: Ignoring pMax and not roating/scaling it appropriately.

returns
   BOOL - TRUE if success, FALSE if the texutre is out of range and not covered
            by the sub-texture
*/
BOOL CSubTexture::FillPixel (DWORD dwThread, DWORD dwFlags, WORD *pawColor, PTEXTPOINT5 pText, PTEXTPOINT5 pMax,
   PCMaterial pMat, float *pafGlow, BOOL fHighQuality)
{
   // make sure it's in range
   TEXTPOINT5 tp;
   if (!PixelInRangeAndMirror (pText, &tp))
      return FALSE;

   // if no texture map then use standard color
   if (!m_pObjectSurface->m_fUseTextureMap) {
      if (pawColor)
         Gamma (m_pObjectSurface->m_cColor, pawColor);
      if (pMat)
         memcpy (pMat, &m_pObjectSurface->m_Material, sizeof(m_pObjectSurface->m_Material));
      if (pafGlow)
         pafGlow[0] = pafGlow[1] = pafGlow[2] = 0;
      return TRUE;
   }

   // if no texture then error
   if (!m_pTexture)
      return FALSE;

   m_pTexture->FillPixel (dwThread, dwFlags, pawColor, &tp, pMax, pMat, pafGlow, fHighQuality);
   return TRUE;
}



/****************************************************************************
CSubTexture::AverageColorGet - Just like the standard API except this
requires the texture be cached
*/
COLORREF CSubTexture::AverageColorGet (DWORD dwThread, BOOL fGlow)
{
   if (m_pTexture)
      return m_pTexture->AverageColorGet (dwThread, fGlow);

   // else
   return fGlow ? RGB(0,0,0) : m_pObjectSurface->m_cColor;
}


/****************************************************************************
CSubTexture::PixelBump - Like the standard FillPixel, except this takes
into account the stretching and mirroring. For this to work,
the texture must be cached.

NOTE: Ignoring pRight and pDown and not roating/scaling it appropriately.

returns
   BOOL - TRUE if success, FALSE if the texutre is out of range and not covered
            by the sub-texture
*/
BOOL CSubTexture::PixelBump (DWORD dwThread, PTEXTPOINT5 pText, PTEXTPOINT5 pRight,
                           PTEXTPOINT5 pDown, PTEXTUREPOINT pSlope, fp *pfHeight, BOOL fHighQuality)
{
   // NOTE: Not tested

   // make sure it's in range
   TEXTPOINT5 tp;
   if (!PixelInRangeAndMirror (pText, &tp))
      return FALSE;

   // if no texture map then use standard color
   if (!m_pObjectSurface->m_fUseTextureMap) {
      pSlope->h = pSlope->v = 0;
      return TRUE;
   }

   // if no texture then error
   if (!m_pTexture)
      return FALSE;

   return m_pTexture->PixelBump (dwThread, &tp, pRight, pDown, pSlope, pfHeight, fHighQuality);
}



/****************************************************************************
CSubTexture::PixelInRangeAndMirror - First, seees if the pOrig point is within
the boundary. If it isnt't then returns FALSE. If it is, the point is converted
and rotated accordingly.

inputs
   PTEXTPOINT5       pOrig - Point in the master texture, usually from 0..1 in each
   PTEXTPOINT5       pMirror - Filled with the point in the sub-texture, including
                     mirrors.
returns
   BOOL - TRUE if the point is within the boundary, FALSE if not
*/
BOOL CSubTexture::PixelInRangeAndMirror (PTEXTPOINT5 pOrig, PTEXTPOINT5 pMirror)
{
   if ((pOrig->hv[0] < m_atpBoundary[0].h) || (pOrig->hv[0] > m_atpBoundary[1].h) ||
      (pOrig->hv[1] < m_atpBoundary[0].v) || (pOrig->hv[1] > m_atpBoundary[1].v))
      return FALSE;  // outside

   // scale
   TEXTUREPOINT tpDelta;
   tpDelta.h = m_atpBoundary[1].h - m_atpBoundary[0].h;
   tpDelta.v = m_atpBoundary[1].v - m_atpBoundary[0].v;
   pMirror->hv[0] = (pOrig->hv[0] - m_atpBoundary[0].h) / ((tpDelta.h > 0) ? tpDelta.h : 1);
   pMirror->hv[1] = (pOrig->hv[1] - m_atpBoundary[0].v) / ((tpDelta.v > 0) ? tpDelta.v : 1);

   // also copy over 3D points, no change
   pMirror->xyz[0] = pOrig->xyz[0];
   pMirror->xyz[1] = pOrig->xyz[1];
   pMirror->xyz[2] = pOrig->xyz[2];

   // mirror?
   if ((m_dwMirror & 0x01) && (pMirror->hv[0] < 0.5)) {
      pMirror->hv[0] = 1.0 - pMirror->hv[0];
      pMirror->xyz[0] *= -1;  // not sure what to do with this
   }
   else if ((m_dwMirror & 0x02) && (pMirror->hv[0] > 0.5)) {
      pMirror->hv[0] = 1.0 - pMirror->hv[0];
      pMirror->xyz[0] *= -1;
   }

   if (!m_fStretchToFit) {
      // BUGFIX - if not stretched, get as per normal, ignoring mirror.
      // Note that including scale of master size
      pMirror->hv[0] = pOrig->hv[0] * m_tpMaster.h;
      pMirror->hv[1] = pOrig->hv[1] * m_tpMaster.v;

      TextureMatrixMultiply (m_pObjectSurface->m_afTextureMatrix, &m_pObjectSurface->m_mTextureMatrix, pMirror);
   }

   return TRUE;
}


/****************************************************************************
CSubTexture::BitmapGet - Gets a bitmap with the given height and width.

inputs
   DWORD          dwWidth - Width in pixels.
   DWORD          dwHeight - Height in pixels
   BOOL           fSphere - Get a sphere as image
   BOOL           fStretchToFit - Stretch the texture out to full width and height
   BOOL           fForceRecalc - Even if one already cached, regenerate
returns
   HBITMAP - Bitmap to use. Do NOT delete, since this is cached
*/
HBITMAP CSubTexture::BitmapGet (DWORD dwRenderShard, DWORD dwWidth, DWORD dwHeight, BOOL fSphere, BOOL fStretchToFit, BOOL fForceRecalc)
{
   dwWidth = max(dwWidth, 1);
   dwHeight = max(dwHeight, 1);
   if (fForceRecalc || (dwWidth != m_dwBmpWidth) || (dwHeight != m_dwBmpHeight) || (fSphere != m_fBmpSphere) ||
      (fStretchToFit != m_fBmpStretched) || !m_hBmp)
      m_fBmpDirty = TRUE;

   if (!m_fBmpDirty)
      return m_hBmp; // already cached

   // else, recalc
   if (m_hBmp)
      DeleteObject (m_hBmp);
   m_hBmp = NULL;
   m_dwBmpWidth = dwWidth;
   m_dwBmpHeight = dwHeight;
   m_fBmpSphere = fSphere;
   m_fBmpStretched = fStretchToFit;

   CImage Image;
   if (!Image.Init (m_dwBmpWidth, m_dwBmpHeight, m_pObjectSurface->m_cColor))
      return NULL;   // error

   // precached?
   PCTextureMapSocket pExist = m_pTexture;
   TextureCache (dwRenderShard);  // NOTE: Ignore texture cache error, so fill with solid

   // draw it if have texture, else using the basic color
   if (m_pTexture)
      m_pTexture->FillImage (&Image, m_fBmpSphere, m_fBmpStretched, m_pObjectSurface->m_afTextureMatrix,
         &m_pObjectSurface->m_mTextureMatrix, &m_pObjectSurface->m_Material);

   // if wasn't preexisting then release
   if (!pExist)
      TextureRelease();

   // get this bitmap
   HWND hWnd = GetDesktopWindow ();
   HDC hDC = GetDC (hWnd);
   m_hBmp = Image.ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);

   return m_hBmp;
}


/****************************************************************************
CSubTexture::TextureQuery - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If the texture is already on here,
                     this returns. If not, the texture is added, and sub-textures
                     are also added.
   PCBTree           pTree - Also added to
returns
   BOOL - TRUE if success
*/
BOOL CSubTexture::TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree)
{
   if (!m_pObjectSurface->m_fUseTextureMap)
      return TRUE;   // just color

   // cache this and sub-textures
   GUID ag[2];
   ag[0] = m_pObjectSurface->m_gTextureCode;
   ag[1] = m_pObjectSurface->m_gTextureSub;
   return TextureAlgoTextureQuery (dwRenderShard, plText, pTree, ag);
}

/****************************************************************************
CSubTexture::SubTextureNoRecurse - Stndard API
*/
BOOL CSubTexture::SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText)
{
   if (!m_pObjectSurface->m_fUseTextureMap)
      return TRUE;   // just color

   // cache this and sub-textures
   GUID ag[2];
   ag[0] = m_pObjectSurface->m_gTextureCode;
   ag[1] = m_pObjectSurface->m_gTextureSub;
   return TextureAlgoSubTextureNoRecurse (dwRenderShard, plText, ag);
}

/****************************************************************************
CSubTexture::PageTrapMessages - This traps page messages dealing with controls
specific to the texture.

NOTE: The controls that are handles are... (XXX = prefix)
   "XXXbutton" - Press this to change object surface
   "XXXboundaryAB" - Controls for bounary, A="0" for upper-left, "1" for lower-right,
                     B = "0" for horz, "1" for vert
   "XXXmirrorA" - Checkbox for mirrors. A = "0" for right-to-left, "1" for left-to-right
   "XXXimage" - Bitmap image of the sub-tetures, automagically inserted by "<<<$XXXimage>>>" substitution
   "XXXUse" - Checked to use this feature.
   "<<<$XXXimage>>>" - Substitution that places a bitmap image in.
   
inputs
   PWSTR          pszPrefix - Prefix used for all controls assigned to this
                     sub-texture
   PCEscPage      pPage - Standard
   DWORD          dwMessage - Standard
   PVOID          pParam - Stanrdar
returns
   BOOL - TRUE if the message was trapped, FALSE if should pass on
*/
BOOL CSubTexture::PageTrapMessages (DWORD dwRenderShard, PWSTR pszPrefix, PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[128];
         PCEscControl pControl;
         DWORD i,j;

         for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
            swprintf (szTemp, L"%sboundary%d%d", pszPrefix, (int)i, (int)j);
            DoubleToControl (pPage, szTemp, j ? m_atpBoundary[i].v : m_atpBoundary[i].h);
         }

         for (i = 0; i < 2; i++) {
            swprintf (szTemp, L"%smirror%d", pszPrefix, (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (m_dwMirror & (1 << i)) ? TRUE : FALSE);
         }

         swprintf (szTemp, L"%sUse", pszPrefix);
         pControl = pPage->ControlFind (szTemp);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), m_fUse);
      }
      break; // pass through

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;
         DWORD dwLen = (DWORD)wcslen(pszPrefix);
         if (_wcsnicmp (psz, pszPrefix, dwLen))
            break;   // no match to prefix
         psz += dwLen;  // skip over prefix

         PWSTR pszMirror = L"mirror";
         DWORD dwMirrorLen = (DWORD)wcslen(pszMirror);

         if (!_wcsnicmp (psz, pszMirror, dwMirrorLen)) {
            DWORD i = (DWORD) _wtoi (psz + dwMirrorLen);
            if (i >= 2)
               break;   // error

            if (p->pControl->AttribGetBOOL(Checked()))
               MirrorSet (m_dwMirror | (1 << i));
            else
               MirrorSet (m_dwMirror & ~(1 << i));
            return TRUE;
         }
         if (!_wcsicmp(psz, L"button")) {
            // pull up dialog to change
            if (!DialogSelect (dwRenderShard, pPage->m_pWindow->m_hWnd))
               return TRUE;

            // need to set bitmap
            WCHAR szTemp[128];
            PCEscControl pControl;
            swprintf (szTemp, L"%simage", pszPrefix);
            pControl = pPage->ControlFind (szTemp);
            swprintf (szTemp, L"%lx", (__int64) BitmapGet (dwRenderShard, SUBTEXTWIDTH(m_fStretchToFit), SUBTEXTHEIGHT, FALSE, m_fStretchToFit, FALSE));
            if (pControl)
               pControl->AttribSet (L"hbitmap", szTemp);
            return TRUE;
         }
         if (!_wcsicmp(psz, L"use")) {
            m_fUse = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;
         DWORD dwLen = (DWORD)wcslen(pszPrefix);
         if (_wcsnicmp (psz, pszPrefix, dwLen))
            break;   // no match to prefix
         psz += dwLen;  // skip over prefix

         PWSTR pszBoundary = L"boundary";
         DWORD dwBoundaryLen = (DWORD)wcslen(pszBoundary);

         if (!_wcsnicmp (psz, pszBoundary, dwBoundaryLen)) {
            DWORD i = (DWORD) (psz[dwBoundaryLen] - L'0');
            DWORD j = (DWORD) (psz[dwBoundaryLen+1] - L'0');
            if ((i >= 2) || (j >= 2))
               break;   // error

            fp f = DoubleFromControl (pPage, p->pControl->m_pszName);
            if (j)
               m_atpBoundary[i].v = f;
            else
               m_atpBoundary[i].h = f;
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         WCHAR szTemp[128];
         swprintf (szTemp, L"%simage", pszPrefix);
         if (!_wcsicmp(p->pszSubName, szTemp)) {
            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<image posn=edgeright hbitmap=");
            WCHAR szHex[32];
            swprintf (szHex, L"%lx", (__int64) BitmapGet (dwRenderShard, SUBTEXTWIDTH(m_fStretchToFit), SUBTEXTHEIGHT, FALSE, m_fStretchToFit, FALSE));
            MemCat (&gMemTemp, szHex);

            MemCat (&gMemTemp, L" name=");
            MemCat (&gMemTemp, szTemp);

            MemCat (&gMemTemp, L" border=2 bordercolor=#000000/>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   } // switch

   return FALSE;
}



/****************************************************************************
CSubTexture::FillTexture - This fills the PCImage's used for generating
a texture based on the sub-texture and its stretching.

inputs
   fp                fWidth - Width in meters
   fp                fHeight - Height in meters
   fp                fPixelLen - Size of pixel in meters
   BOOL              fHighQuality - If TRUE will fill the texture with high quality. If FALSE then low
   PCImage           pImage - Image with RGB, Z, transparency
   PCImage           pImage2 - Filled with glow info
   PCMaterial        pMaterial - Filled with the texture's material
   fp                *pfGlowScale - Filled with the glow scale.

      // tells object to render itself. It will initialize pImage to the appropriate
      // size. The image is then filled in with RGB for the color (not affected by
      // shading), iZ is the depth (0x10000 == 1 pixel in height), LOWORD(dwID) / 100 is
      // the power component for specularity, HIWORD(dwID) is brightness (0 to 0xffff) of
      // specularity. dwIDPart has the following set.... LOBYTE(dwIDPart) is the transparency
      // of the surface at the point.

returns
   none
*/
void CSubTexture::FillTexture (DWORD dwRenderShard, fp fWidth, fp fHeight, fp fPixelLen, BOOL fHighQuality,
                               PCImage pImage, PCImage pImage2, PCMaterial pMaterial, fp *pfGlowScale)
{
   DWORD    dwWidth, dwHeight;
   dwWidth = (DWORD) ((fWidth + fPixelLen/2) / fPixelLen);
   dwHeight = (DWORD) ((fHeight + fPixelLen/2) / fPixelLen);
   dwWidth = max(dwWidth, 1);
   dwHeight = max(dwHeight, 1);

   // clear them out
   COLORREF cGrout = RGB(0,0,0);
   pImage->Init (dwWidth, dwHeight, cGrout, 0);
   pImage2->Init (dwWidth, dwHeight, RGB(0,0,0));  // zero glow
   *pfGlowScale = 0;
   *pMaterial = m_pObjectSurface->m_Material;

   if (!m_pObjectSurface->m_fUseTextureMap) {
      // just a solid color, so special case
      IHLEND end[4];
      pImage->TGDrawQuad (
         pImage->ToIHLEND(&end[0], 0, 0, 0, m_pObjectSurface->m_cColor),
         pImage->ToIHLEND(&end[1], dwWidth , 0, 0, m_pObjectSurface->m_cColor),
         pImage->ToIHLEND(&end[2], dwWidth, dwHeight, 0, m_pObjectSurface->m_cColor),
         pImage->ToIHLEND(&end[3], 0, dwHeight, 0, m_pObjectSurface->m_cColor),
         1, TRUE, TRUE, m_pObjectSurface->m_Material.m_wSpecReflect, m_pObjectSurface->m_Material.m_wSpecExponent
         );

      return;
   }

   // temporary memory for glow color so can determine max and min
   CMem mem;
   DWORD dwNeed = dwWidth * dwHeight * sizeof(float) * 3;
   if (!mem.Required (dwNeed))
      return;  // error
   float *paf = (float*)mem.p;
   float afMax[3];
   afMax[0] = afMax[1] = afMax[2] = 0;

   // else, stretch
   TextureCache (dwRenderShard);

   DWORD x,y, i;
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   TEXTPOINT5 tpZero;
   memset (&tpZero, 0, sizeof(tpZero));
   CMaterial Mat;
   fp fPixelHeight;
   fp fPixelLenInv = 1.0 / fPixelLen;
   for (y = 0; y < dwHeight; y++) {
      fp fY = (fp)y * fPixelLen;
      fp fX = 0;

      for (x = 0; x < dwWidth; x++, paf += 3, pip++, fX += fPixelLen) {
         TEXTPOINT5 tp;
         tp.hv[0] = fX;
         tp.hv[1] = fY;
         tp.xyz[0] = fX;
         tp.xyz[1] = -fY;
         tp.xyz[2] = 0;
         TextureMatrixMultiply (m_pObjectSurface->m_afTextureMatrix, &m_pObjectSurface->m_mTextureMatrix, &tp);

         // make sure doesnt go beyond 0 or 1, so that texture get wont present problem
         tp.hv[0] = myfmod(tp.hv[0], 1);
         tp.hv[1] = myfmod(tp.hv[1], 1);

         // get the pixel
         memcpy (&Mat, &m_pObjectSurface->m_Material, sizeof(Mat));
         FillPixel (0, TMFP_ALL, &pip->wRed, &tp, &tpZero, &Mat, paf, fHighQuality);
         PixelBump (0, &tp, &tpZero, &tpZero, NULL, &fPixelHeight, fHighQuality);

         // max
         for (i = 0; i < 3; i++)
            afMax[i] = max(afMax[i], paf[i]);

         // store info away
         pip->fZ = fPixelHeight * fPixelLenInv;
         pip->dwID = (DWORD)Mat.m_wSpecExponent | ((DWORD)Mat.m_wSpecReflect << 16);
         pip->dwIDPart = (Mat.m_wTransparency >> 8);
      } // x
   } // y

   // loop through again and fill in the glow
   if (afMax[0] || afMax[1] || afMax[2]) {
      fp fScale = max(max(afMax[0], afMax[1]), afMax[2]);
      *pfGlowScale = fScale / (fp)0xffff;
      fp f;
      fScale = (fp)0xffff / fScale;

      pip = pImage2->Pixel(0,0);
      paf = (float*)mem.p;
      for (x = 0; x < dwWidth * dwHeight; x++, pip++, paf += 3)
         for (i = 0; i < 3; i++) {
            f = paf[i] * fScale;
            (&pip->wRed)[i] = (f < (fp)0xffff) ? (WORD)f : (WORD)0xffff;
         } // i
   } // if glow
   TextureRelease();
}



/****************************************************************************
FaceomaticPage
*/
BOOL FaceomaticPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorFaceomatic pv = (PCTextCreatorFaceomatic) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);
         MeasureToString (pPage, L"patternwidth", pv->m_fWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fHeight, TRUE);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_Material.m_dwID)
               break; // unchanged
            if (dwVal)
               pv->m_Material.InitFromID (dwVal);
            else
               pv->m_Material.m_dwID = MATERIAL_CUSTOM;

            // eanble/disable button to edit
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"editmaterial");
            if (pControl)
               pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         MeasureParseString (pPage, L"pixellen", &pv->m_fPixelLen);
         pv->m_fPixelLen = max(.0001, pv->m_fPixelLen);

         MeasureParseString (pPage, L"patternwidth", &pv->m_fWidth);
         pv->m_fWidth = max(pv->m_fPixelLen, pv->m_fWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fHeight);
         pv->m_fHeight = max(pv->m_fPixelLen, pv->m_fHeight);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Face-o-matic";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
FaceomaticBasePage
*/
BOOL FaceomaticBasePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorFaceomatic pv = (PCTextCreatorFaceomatic) pt->pThis;

   // call sub-textures for trap
   if (pv->m_Base.PageTrapMessages (pt->dwRenderShard, L"Base", pPage, dwMessage, pParam))
      return TRUE;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_USER+186:  // get all the control values
      {
         // do nothing
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Base color";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
FaceomaticOilyPage
*/
BOOL FaceomaticOilyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorFaceomatic pv = (PCTextCreatorFaceomatic) pt->pThis;

   // call sub-textures for trap
   if (pv->m_Oily.PageTrapMessages (pt->dwRenderShard, L"Oily", pPage, dwMessage, pParam))
      return TRUE;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         WCHAR szTemp[64];
         DWORD i;

         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"shiny%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pOilyShiny.p[i]);
         } // i
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // do nothing
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszShiny = L"shiny";
         DWORD dwShinyLen = (DWORD)wcslen(pszShiny);
         if (!_wcsnicmp (psz, pszShiny, dwShinyLen)) {
            DWORD dwNum = (DWORD)_wtoi(psz + dwShinyLen);
            if (dwNum >= 3)
               break;    // error
            pv->m_pOilyShiny.p[dwNum] = DoubleFromControl (pPage, p->pControl->m_pszName);
         } // shunk
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Oily skin";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}



BOOL CTextCreatorFaceomatic::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   PWSTR pszMakeup = L"makeup", pszWarts = L"warts", pszScars = L"scars", pszFur = L"fur";
   DWORD dwMakeupLen = (DWORD)wcslen(pszMakeup), dwWartsLen = (DWORD)wcslen(pszWarts), dwScarsLen = (DWORD)wcslen(pszScars),
      dwFurLen = (DWORD)wcslen(pszFur);
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;
   ti.fDrawFlat = TRUE;
   ti.fStretchToFit = TRUE;
   ti.dwRenderShard = m_dwRenderShard;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFACEOMATIC, FaceomaticPage, &ti);
   if (!pszRet)
      goto done;
   if (!_wcsicmp(pszRet, L"base")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFACEOMATICBASE, FaceomaticBasePage, &ti);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto redo;
   }
   else if (!_wcsicmp(pszRet, L"oily")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREFACEOMATICOILY, FaceomaticOilyPage, &ti);
      if (pszRet && !_wcsicmp(pszRet, Back()))
         goto redo;
   }
   else if (!_wcsnicmp(pszRet, pszMakeup, dwMakeupLen)) {
      DWORD dwNum = _wtoi(pszRet+dwMakeupLen);
      if (m_apMakeup[dwNum]->Dialog (m_dwRenderShard, pWindow, this))
         goto redo;
      else
         pszRet = NULL; // so exits
   }
   else if (!_wcsnicmp(pszRet, pszWarts, dwWartsLen)) {
      DWORD dwNum = _wtoi(pszRet+dwWartsLen);
      if (m_apWarts[dwNum]->Dialog (m_dwRenderShard, pWindow, this))
         goto redo;
      else
         pszRet = NULL; // so exits
   }
   else if (!_wcsnicmp(pszRet, pszScars, dwScarsLen)) {
      DWORD dwNum = _wtoi(pszRet+dwScarsLen);
      if (m_apScars[dwNum]->Dialog (m_dwRenderShard, pWindow, this))
         goto redo;
      else
         pszRet = NULL; // so exits
   }
   else if (!_wcsnicmp(pszRet, pszFur, dwFurLen)) {
      DWORD dwNum = _wtoi(pszRet+dwFurLen);
      if (m_apFur[dwNum]->Dialog (m_dwRenderShard, pWindow, this))
         goto redo;
      else
         pszRet = NULL; // so exits
   }

done:
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorFaceomatic::CTextCreatorFaceomatic (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_PAINTGLOSS); // BUGFIX - Was plastic
   m_fWidth = .6;
   m_fHeight = m_fWidth / 2.0;
   m_fPixelLen = 0.001;

   DWORD i;
   for (i = 0; i < MAXMAKEUP; i++)
      m_apMakeup[i] = new CFaceMakeup;
   for (i = 0; i < MAXWARTS; i++)
      m_apWarts[i] = new CFaceWarts;
   for (i = 0; i < MAXSCARS; i++)
      m_apScars[i] = new CFaceScars;
   for (i = 0; i < MAXFUR; i++)
      m_apFur[i] = new CFaceFur;

   m_Base.StretchToFitSet (TRUE);   // this one is stretched
   m_Oily.StretchToFitSet (TRUE);
   m_pOilyShiny.Zero();
   m_pOilyShiny.p[0] = m_pOilyShiny.p[2] = 0.1;
   m_pOilyShiny.p[1] = 0.5;

}
CTextCreatorFaceomatic::~CTextCreatorFaceomatic (void)
{
   DWORD i;
   for (i = 0; i < MAXMAKEUP; i++)
      delete m_apMakeup[i];
   for (i = 0; i < MAXWARTS; i++)
      delete m_apWarts[i];
   for (i = 0; i < MAXSCARS; i++)
      delete m_apScars[i];
   for (i = 0; i < MAXFUR; i++)
      delete m_apFur[i];
}

void CTextCreatorFaceomatic::Delete (void)
{
   delete this;
}

static PWSTR gpszFaceomatic = L"Faceomatic";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszPixelLen = L"PixelLen";
static PWSTR gpszBase = L"Base";
static PWSTR gpszOily = L"Oily";
static PWSTR gpszOilyShiny = L"OilyShiny";

PCMMLNode2 CTextCreatorFaceomatic::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszFaceomatic);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszOilyShiny, &m_pOilyShiny);

   PCMMLNode2 pSub;
   pSub = m_Base.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszBase);
      pNode->ContentAdd (pSub);
   }
   pSub = m_Oily.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszOily);
      pNode->ContentAdd (pSub);
   }

   DWORD i;
   for (i = 0; i < MAXMAKEUP; i++) {
      pSub = m_apMakeup[i]->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMakeup);
      pNode->ContentAdd (pSub);
   } // i
   for (i = 0; i < MAXWARTS; i++) {
      pSub = m_apWarts[i]->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszWarts);
      pNode->ContentAdd (pSub);
   } // i
   for (i = 0; i < MAXSCARS; i++) {
      pSub = m_apScars[i]->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszScars);
      pNode->ContentAdd (pSub);
   } // i
   for (i = 0; i < MAXFUR; i++) {
      pSub = m_apFur[i]->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszFur);
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}

BOOL CTextCreatorFaceomatic::MMLFrom (PCMMLNode2 pNode)
{
   // clear
   m_Base.Clear();
   m_Oily.Clear();
   DWORD i;
   for (i = 0; i < MAXMAKEUP; i++)
      m_apMakeup[i]->Clear();
   for (i = 0; i < MAXWARTS; i++)
      m_apWarts[i]->Clear();
   for (i = 0; i < MAXSCARS; i++)
      m_apScars[i]->Clear();
   for (i = 0; i < MAXFUR; i++)
      m_apFur[i]->Clear();


   m_Material.MMLFrom (pNode);

   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, .6);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, .3);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, .001);
   MMLValueGetPoint (pNode, gpszOilyShiny, &m_pOilyShiny);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD dwCurMakeup = 0, dwCurWarts = 0, dwCurScars = 0, dwCurFur = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszBase)) {
         m_Base.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszOily)) {
         m_Oily.MMLFrom (pSub);
         continue;
      }
      if (!_wcsicmp(psz, gpszMakeup)) {
         if (dwCurMakeup >= MAXMAKEUP)
            continue;   // ignore
         m_apMakeup[dwCurMakeup]->MMLFrom (pSub);
         dwCurMakeup++;
         continue;
      }
      if (!_wcsicmp(psz, gpszWarts)) {
         if (dwCurWarts >= MAXWARTS)
            continue;   // ignore
         m_apWarts[dwCurWarts]->MMLFrom (pSub);
         dwCurWarts++;
         continue;
      }
      if (!_wcsicmp(psz, gpszScars)) {
         if (dwCurScars >= MAXSCARS)
            continue;   // ignore
         m_apScars[dwCurScars]->MMLFrom (pSub);
         dwCurScars++;
         continue;
      }
      if (!_wcsicmp(psz, gpszFur)) {
         if (dwCurFur >= MAXFUR)
            continue;   // ignore
         m_apFur[dwCurFur]->MMLFrom (pSub);
         dwCurFur++;
         continue;
      }
   } // i
   return TRUE;
}

/****************************************************************************
CTextCreatorFaceomatic::EscMultiThreadedCallback - Multithreaded rendering.
*/
void CTextCreatorFaceomatic:: EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
   DWORD dwThread)
{
   PMTFACEINFO pmti = (PMTFACEINFO) pParams;
   PCTextCreatorFaceomatic pFace = pmti->pFace;
   PCImage pImage = pmti->pImage;
   PCImage pImage2 = pmti->pImage2;
   PCImage pImageZ = pmti->pImageZ;
   DWORD dwRenderShard = pmti->dwRenderShard;

#ifdef _DEBUG
   WCHAR szTemp[64];
   swprintf (szTemp, L"\r\nThreadStart %d, %d to %d", (int)dwThread, (int)pmti->dwStart, (int)pmti->dwEnd);
   OutputDebugStringW (szTemp);
#endif

   PIMAGEPIXEL pip, pip2;
   DWORD x, y, i;
   TEXTPOINT5 tpCur, tpMax;
   CMaterial MatTemp;
   float afGlow[3];
   WORD awColor[3];
   fp f;
   fp fOily;
   memset (&tpMax, 0, sizeof(tpMax));
   memset (&tpCur, 0, sizeof(tpCur));

   if (pmti->dwPass == 0) {   // base color
      pip = pImage->Pixel(0,pmti->dwStart);
      pip2 = pImage2->Pixel(0,pmti->dwStart);
      for (y = pmti->dwStart; y < pmti->dwEnd; y++) {
         tpCur.hv[1] = (fp)y / (fp)pImage->Height();
         tpCur.xyz[1] = tpCur.hv[1] * m_fHeight;

         for (x = 0; x < pImage->Width(); x++, pip++, pip2++) {
            tpCur.hv[0] = (fp)x / (fp)pImage->Width();
            tpCur.xyz[0] = tpCur.hv[0] * m_fWidth;

            if (!m_Base.FillPixel (dwThread, TMFP_ALL, &pip->wRed, &tpCur, &tpMax, &MatTemp, &afGlow[0], TRUE))
               continue;   // nothing there

            // fill in glow
            for (i = 0; i < 3; i++) {
               if (!afGlow[i])
                  continue;

               WORD *pw = &pip2->wRed + i;
               f = afGlow[i] / DEFGLOWSCALE + (fp)(*pw);
               f = max(f, 0);
               f = min (f, (fp)0xffff);
               *pw = (WORD)f;
            }
         } // x
      } // y
   } // dwPass == 0

   else if (pmti->dwPass == 1) { // oily
      pip = pImage->Pixel(0,pmti->dwStart);
      for (y = pmti->dwStart; y < pmti->dwEnd; y++) {
         tpCur.hv[1] = (fp)y / (fp)pImage->Height();
         tpCur.xyz[1] = tpCur.hv[1] * m_fHeight;

         for (x = 0; x < pImage->Width(); x++, pip++) {
            tpCur.hv[0] = (fp)x / (fp)pImage->Width();
            tpCur.xyz[0] = tpCur.hv[0] * m_fWidth;

            if (m_Oily.FillPixel (dwThread, TMFP_ALL, &awColor[0], &tpCur, &tpMax, &MatTemp, &afGlow[0], FALSE)) { // fast
               fOily = ((fp)awColor[0] + (fp)awColor[1] + (fp)awColor[2]) / ((fp)0xffff * 3.0);
               fOily = fOily * m_pOilyShiny.p[1] + (1.0 - fOily) * m_pOilyShiny.p[0];
            }
            else
               fOily = m_pOilyShiny.p[2];
            fOily *= (fp)0xffff;
            fOily = max(fOily, 0);
            fOily = min(fOily, (fp)0xfffff);

            pip->dwID = MAKELONG (LOWORD(pip->dwID), (WORD)fOily);
         } // x
      } // y
   }

#ifdef _DEBUG
   swprintf (szTemp, L"\r\nThreadFinished %d, %d to %d", (int)dwThread, (int)pmti->dwStart, (int)pmti->dwEnd);
   OutputDebugStringW (szTemp);
#endif

}


BOOL CTextCreatorFaceomatic::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   // size
   DWORD    dwX, dwY;
   m_fPixelLen = max(m_fPixelLen, 0.0001);
   fp fPixelLen = TextureDetailApply(m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) max((DWORD)((m_fWidth + fPixelLen/2) / fPixelLen), 1);
   dwY = (DWORD) max((DWORD)((m_fHeight + fPixelLen/2) / fPixelLen), 1);

   // go through all the sub-textures and say the size
   m_Base.ScaleSet (m_fWidth, m_fHeight);
   m_Oily.ScaleSet (m_fWidth, m_fHeight);
   DWORD i;
   for (i = 0; i < MAXMAKEUP; i++)
      m_apMakeup[i]->ScaleSet (m_fWidth, m_fHeight);
   for (i = 0; i < MAXWARTS; i++)
      m_apWarts[i]->ScaleSet (m_fWidth, m_fHeight);
   for (i = 0; i < MAXSCARS; i++)
      m_apScars[i]->ScaleSet (m_fWidth, m_fHeight);
   for (i = 0; i < MAXFUR; i++)
      m_apFur[i]->ScaleSet (m_fWidth, m_fHeight);

   // initialize the surface, making sure to fill in specularity
   pImage->Init (dwX, dwY, RGB(128, 128, 0), 0);
   pImage2->Init (dwX, dwY, RGB(0,0,0), 0);  // for glow

   // fill in specularity
   PIMAGEPIXEL pip;
   pip = pImage->Pixel(0,0);
   DWORD dwNum;
   dwNum = pImage->Width() * pImage->Height();
   for (i = 0; i < dwNum; i++, pip++)
      pip->dwID = MAKELONG (m_Material.m_wSpecExponent, m_Material.m_wSpecReflect);

   // fill in the base color
   m_Base.TextureCache (m_dwRenderShard);

   // BUGFIX - base color calculated multithreaded
   MTFACEINFO mtfi;
   memset (&mtfi, 0, sizeof(mtfi));
   mtfi.pFace = this;
   mtfi.pImage = pImage;
   mtfi.pImage2 = pImage2;
   mtfi.dwRenderShard = m_dwRenderShard;
   // mtfi.pImageZ = pImageZ;
   mtfi.dwPass = 0;
   ThreadLoop (0, pImage->Height(), 3, &mtfi, sizeof(mtfi));

   m_Base.TextureRelease();

   // do the oily face
   if (m_Oily.m_fUse) {
      m_Oily.TextureCache (m_dwRenderShard);

      mtfi.dwPass = 1;
      ThreadLoop (0, pImage->Height(), 3, &mtfi, sizeof(mtfi));

      m_Oily.TextureRelease();
   } // if oily

   // apply makeup (pre warts)
   for (i = 0; i < MAXMAKEUP; i++) {
      // make sure before warts
      if (!m_apMakeup[i]->m_fBeforeWarts)
         continue;

      // render
      m_apMakeup[i]->Render (m_dwRenderShard, pImage, pImage2, this);
   } // i

   // draw all the scars
   for (i = 0; i < MAXSCARS; i++)
      m_apScars[i]->Render (m_dwRenderShard, pImage, pImage2, this);

   // draw all the warts
   for (i = 0; i < MAXWARTS; i++)
      m_apWarts[i]->Render (m_dwRenderShard, pImage, pImage2, this);

   // apply makeup (post warts)
   for (i = 0; i < MAXMAKEUP; i++) {
      // make sure before warts
      if (m_apMakeup[i]->m_fBeforeWarts)
         continue;

      // render
      m_apMakeup[i]->Render (m_dwRenderShard, pImage, pImage2, this);
   } // i

   // draw all the fur
   PCImage pImageZ = pImage->Clone();
   if (pImageZ) {
      for (i = 0; i < MAXFUR; i++)
         m_apFur[i]->Render (m_dwRenderShard, pImage, pImage2, this, pImageZ);
      delete pImageZ;
   }


   // final bits
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   // figure if need glow
   pip = pImage2->Pixel(0,0);
   dwNum = pImage2->Width() * pImage2->Height();
   for (i = 0; i < dwNum; i++, pip++)
      if (pip->wRed || pip->wGreen || pip->wBlue) {
         pTextInfo->fGlowScale = DEFGLOWSCALE;
         pTextInfo->dwMap |= 0x08;  // glow
         break;
      }

   return TRUE;
}



/***********************************************************************************
CTextCreatorFaceomatic::TextureQuery - Adds this object's textures (and it's sub-textures)
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
BOOL CTextCreatorFaceomatic::TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;

   
   // add itself
   plText->Add (pagThis);
   pTree->Add (szTemp, NULL, 0);

   // NEW CODE: go through all sub-textures
   m_Base.TextureQuery (m_dwRenderShard, plText, pTree);
   m_Oily.TextureQuery (m_dwRenderShard, plText, pTree);
   DWORD i;
   for (i = 0; i < MAXMAKEUP; i++)
      m_apMakeup[i]->TextureQuery (m_dwRenderShard, plText, pTree);
   for (i = 0; i < MAXWARTS; i++)
      m_apWarts[i]->TextureQuery (m_dwRenderShard, plText, pTree);
   for (i = 0; i < MAXSCARS; i++)
      m_apScars[i]->TextureQuery (m_dwRenderShard, plText, pTree);
   for (i = 0; i < MAXFUR; i++)
      m_apFur[i]->TextureQuery (m_dwRenderShard, plText, pTree);

   return TRUE;
}


/***********************************************************************************
CTextCreatorFaceomatic::SubTextureNoRecurse - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if they recurse
*/
BOOL CTextCreatorFaceomatic::SubTextureNoRecurse (PCListFixed plText, GUID *pagThis)
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
   fRet = m_Base.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;
   fRet = m_Oily.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;
   for (i = 0; i < MAXMAKEUP; i++) {
      fRet = m_apMakeup[i]->SubTextureNoRecurse (m_dwRenderShard, plText);
      if (!fRet)
         goto done;
   }
   for (i = 0; i < MAXWARTS; i++) {
      fRet = m_apWarts[i]->SubTextureNoRecurse (m_dwRenderShard, plText);
      if (!fRet)
         goto done;
   }
   for (i = 0; i < MAXSCARS; i++) {
      fRet = m_apScars[i]->SubTextureNoRecurse (m_dwRenderShard, plText);
      if (!fRet)
         goto done;
   }
   for (i = 0; i < MAXFUR; i++) {
      fRet = m_apFur[i]->SubTextureNoRecurse (m_dwRenderShard, plText);
      if (!fRet)
         goto done;
   }

done:
   // restore list
   plText->Truncate (dwNum);

   return fRet;
}

