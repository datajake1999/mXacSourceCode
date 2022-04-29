/********************************************************************************
CTextCreatorTreeBark.cpp - Code for handling faces.

begun 11/1/06 by Mike Rozak
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
TreeBarkPage
*/
BOOL TreeBarkPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorTreeBark pv = (PCTextCreatorTreeBark) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         PCEscControl pControl;
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);
         MeasureToString (pPage, L"noisex", pv->m_tpNoise.h, TRUE);
         MeasureToString (pPage, L"noisey", pv->m_tpNoise.v, TRUE);
         MeasureToString (pPage, L"depth", pv->m_fDepth, TRUE);
         MeasureToString (pPage, L"noiseribx", pv->m_tpNoiseRib.h, TRUE);
         MeasureToString (pPage, L"noiseriby", pv->m_tpNoiseRib.v, TRUE);
         MeasureToString (pPage, L"ribwidth", pv->m_fRibWidth, TRUE);
         MeasureToString (pPage, L"ribnoise", pv->m_fRibNoise, TRUE);

         if (pControl = pPage->ControlFind (L"ribstrength"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fRibStrength * 100));
         if (pControl = pPage->ControlFind (L"depthsquare"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDepthSquare * 100));
         if (pControl = pPage->ControlFind (L"barkchannel"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBarkChannel * 100));
         if (pControl = pPage->ControlFind (L"noisedetail"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fNoiseDetail * 100));
         if (pControl = pPage->ControlFind (L"blendedge"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBlendEdge * 100));
         if (pControl = pPage->ControlFind (L"blendchannel"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fBlendChannel * 100));

         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);
         FillStatusColor (pPage, L"colorchannelcolor", pv->m_cColorChannel);
         FillStatusColor (pPage, L"coloredgecolor", pv->m_cColorEdge);

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorchannelbutton")) {
            pv->m_cColorChannel = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColorChannel, pPage, L"colorchannelcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"coloredgebutton")) {
            pv->m_cColorEdge = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColorEdge, pPage, L"coloredgecolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
            if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
               pPage->Exit (L"[close]");
            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   // dont bother case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;

         // only do one scroll bar
         if (!p->pControl->m_pszName)
            break;

         // set value
         int iVal;
         iVal = p->pControl->AttribGetInt (Pos());
         fp fVal = (fp)iVal / 100.0;
         if (!_wcsicmp(p->pControl->m_pszName, L"depthsquare")) {
            pv->m_fDepthSquare = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"barkchannel")) {
            pv->m_fBarkChannel = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"noisedetail")) {
            pv->m_fNoiseDetail = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"blendedge")) {
            pv->m_fBlendEdge = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"blendchannel")) {
            pv->m_fBlendChannel = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"ribstrength")) {
            pv->m_fRibStrength = fVal;
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

         MeasureParseString (pPage, L"patternwidth", &pv->m_fPatternWidth);
         pv->m_fPatternWidth = max(pv->m_fPixelLen, pv->m_fPatternWidth);

         MeasureParseString (pPage, L"patternheight", &pv->m_fPatternHeight);
         pv->m_fPatternHeight = max(pv->m_fPixelLen, pv->m_fPatternHeight);

         fp fTemp;
         MeasureParseString (pPage, L"noisex", &fTemp);
         pv->m_tpNoise.h = fTemp;
         pv->m_tpNoise.h = max(pv->m_fPixelLen, pv->m_tpNoise.h);

         MeasureParseString (pPage, L"noisey", &fTemp);
         pv->m_tpNoise.v = fTemp;
         pv->m_tpNoise.v = max(pv->m_fPixelLen, pv->m_tpNoise.v);

         MeasureParseString (pPage, L"depth", &pv->m_fDepth);

         MeasureParseString (pPage, L"noiseribx", &fTemp);
         pv->m_tpNoiseRib.h = fTemp;
         pv->m_tpNoiseRib.h = max(pv->m_fPixelLen, pv->m_tpNoiseRib.h);

         MeasureParseString (pPage, L"noiseriby", &fTemp);
         pv->m_tpNoiseRib.v = fTemp;
         pv->m_tpNoiseRib.v = max(pv->m_fPixelLen, pv->m_tpNoiseRib.v);

         MeasureParseString (pPage, L"ribwidth", &pv->m_fRibWidth);
         pv->m_fRibWidth = max(pv->m_fPixelLen, pv->m_fRibWidth);

         MeasureParseString (pPage, L"ribnoise", &pv->m_fRibNoise);
         pv->m_fRibNoise = max(pv->m_fRibNoise, pv->m_fRibNoise);

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Tree bark";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorTreeBark::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURETREEBARK, TreeBarkPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   PWSTR pszNoise = L"noise";
   DWORD dwLen = (DWORD)wcslen(pszNoise);
   if (pszRet && !wcsncmp(pszRet, pszNoise, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(1, dwVal);
      if (m_aNoise[dwVal].Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }
   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorTreeBark::CTextCreatorTreeBark (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_FLAT);
   m_dwType = dwType;
   m_iSeed = 0;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;
   m_cColor = 0;

   m_fPixelLen = .002;
   m_iSeed = 12345;
   m_fPatternWidth = 0.5;
   m_fPatternHeight = 0.5;
   m_cColor = RGB(0x80, 0x80, 0x30);
   m_cColorChannel = RGB(0x70, 0x70, 0x60);
   m_cColorEdge = RGB(0x30, 0x30, 0x20);

   m_tpNoise.h = .02;
   m_tpNoise.v = .10;
   m_fDepth = 0.01;
   m_fDepthSquare = 0.8;
   m_fNoiseDetail = 0.5;
   m_fBarkChannel = 0.2;
   m_fBlendEdge = m_fBlendChannel = 0.5;

   m_tpNoiseRib.h = m_tpNoiseRib.v = 0.2;
   m_fRibWidth = .03;
   m_fRibNoise = .04;
   m_fRibStrength = 0.5;

   DWORD i;
   for (i =0 ; i < 2; i++) {
      m_aNoise[i].m_fTurnOn = FALSE;
      m_aNoise[i].m_fNoiseX = .2;
      m_aNoise[i].m_fNoiseY = .2;
      m_aNoise[i].m_cMax = RGB(0xff,0xff,0);
      m_aNoise[i].m_cMin = RGB(0xff,0xff,0);
      m_aNoise[i].m_fTransMax = .5;
      m_aNoise[i].m_fTransMin = 1;
      m_aNoise[i].m_fZDeltaMax = 0.01;
      m_aNoise[i].m_fZDeltaMin = -0.01;
   } // i

}

void CTextCreatorTreeBark::Delete (void)
{
   delete this;
}

static PWSTR gpszPatternWidth = L"PatternWidth";
static PWSTR gpszPatternHeight = L"PatternHeight";
static PWSTR gpszTreeBark = L"TreeBark";
static PWSTR gpszType = L"Type";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszPixelLen = L"PixelLen";
static PWSTR gpszColor = L"Color";
static PWSTR gpszColorChannel = L"ColorChannel";
static PWSTR gpszColorEdge = L"ColorEdge";
static PWSTR gpszNoise = L"Noise";
static PWSTR gpszNoiseRib = L"NoiseRib";
static PWSTR gpszDepth = L"Depth";
static PWSTR gpszDepthSquare = L"DepthSquare";
static PWSTR gpszNoiseDetail = L"NoiseDetail";
static PWSTR gpszBarkChannel = L"BarkChannel";
static PWSTR gpszRibWidth = L"RibWidth";
static PWSTR gpszRibNoise = L"RibNoise";
static PWSTR gpszRibStrength = L"RibStrength";
static PWSTR gpszBlendEdge = L"BlendEdge";
static PWSTR gpszBlendChannel = L"BlendChannel";

PCMMLNode2 CTextCreatorTreeBark::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTreeBark);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);
   MMLValueSet (pNode, gpszColor, (int)m_cColor);
   MMLValueSet (pNode, gpszColorEdge, (int)m_cColorEdge);
   MMLValueSet (pNode, gpszColorChannel, (int)m_cColorChannel);

   MMLValueSet (pNode, gpszNoise, &m_tpNoise);
   MMLValueSet (pNode, gpszDepth, m_fDepth);
   MMLValueSet (pNode, gpszDepthSquare, m_fDepthSquare);
   MMLValueSet (pNode, gpszNoiseDetail, m_fNoiseDetail);
   MMLValueSet (pNode, gpszBarkChannel, m_fBarkChannel);
   MMLValueSet (pNode, gpszBlendEdge, m_fBlendEdge);
   MMLValueSet (pNode, gpszBlendChannel, m_fBlendChannel);

   MMLValueSet (pNode, gpszNoiseRib, &m_tpNoiseRib);
   MMLValueSet (pNode, gpszRibWidth, m_fRibWidth);
   MMLValueSet (pNode, gpszRibNoise, m_fRibNoise);
   MMLValueSet (pNode, gpszRibStrength, m_fRibStrength);

   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 2; i++) {
      PCMMLNode2 pSub = m_aNoise[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Noise%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorTreeBark::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);
   m_cColor = (COLORREF) MMLValueGetInt (pNode, gpszColor, (int)0);
   m_cColorChannel = (COLORREF) MMLValueGetInt (pNode, gpszColorChannel, (int)0);
   m_cColorEdge = (COLORREF) MMLValueGetInt (pNode, gpszColorEdge, (int)0);

   MMLValueGetTEXTUREPOINT (pNode, gpszNoise, &m_tpNoise);
   m_fDepth = MMLValueGetDouble (pNode, gpszDepth, .01);
   m_fDepthSquare = MMLValueGetDouble (pNode, gpszDepthSquare, .8);
   m_fNoiseDetail = MMLValueGetDouble (pNode, gpszNoiseDetail, .5);
   m_fBarkChannel = MMLValueGetDouble (pNode, gpszBarkChannel, .2);
   m_fBlendEdge = MMLValueGetDouble (pNode, gpszBlendEdge, .5);
   m_fBlendChannel = MMLValueGetDouble (pNode, gpszBlendChannel, .5);

   MMLValueGetTEXTUREPOINT (pNode, gpszNoiseRib, &m_tpNoiseRib);
   m_fRibWidth = MMLValueGetDouble (pNode, gpszRibWidth, .03);
   m_fRibNoise = MMLValueGetDouble (pNode, gpszRibNoise, .04);
   m_fRibStrength = MMLValueGetDouble (pNode, gpszRibStrength, .5);

   DWORD i;
   WCHAR szTemp[64];
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwFind;
   for (i = 0; i < 2; i++) {
      swprintf (szTemp, L"Noise%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aNoise[i].MMLFrom (pSub);
   }

   return TRUE;
}

BOOL CTextCreatorTreeBark::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwScale = max(dwX, dwY);


   // create the surface
   pImage->Init (dwX, dwY, RGB(0xff,0,0), 0);

   // grout texture
   // draw the tile at at height and specularity
   COLORREF cGrout = m_cColor;
   IHLEND end[4];
   pImage->TGDrawQuad (
      pImage->ToIHLEND(&end[0], 0, 0, 0, cGrout),
      pImage->ToIHLEND(&end[1], dwX , 0, 0, cGrout),
      pImage->ToIHLEND(&end[2], dwX, dwY, 0, cGrout),
      pImage->ToIHLEND(&end[3], 0, dwY, 0, cGrout),
      1, TRUE, TRUE, m_Material.m_wSpecReflect, m_Material.m_wSpecExponent
      );

   DWORD i;


   // initialize the noise
   CNoise2D Noise, NoiseWave;
   DWORD dwNoiseX = m_fPatternWidth / max(m_tpNoise.h, fPixelLen);
   DWORD dwNoiseY = m_fPatternHeight / max(m_tpNoise.v, fPixelLen);
   dwNoiseX = max(dwNoiseX, 1);
   dwNoiseY = max(dwNoiseY, 1);
   Noise.Init (dwNoiseX, dwNoiseY);
   dwNoiseX = m_fPatternWidth / max(m_tpNoiseRib.h, fPixelLen);
   dwNoiseY = m_fPatternHeight / max(m_tpNoiseRib.v, fPixelLen);
   dwNoiseX = max(dwNoiseX, 1);
   dwNoiseY = max(dwNoiseY, 1);
   NoiseWave.Init (dwNoiseX, dwNoiseY);

   for (i = 0; i < 2; i++)
      m_aNoise[i].Apply (pImage, fPixelLen);

   // number of times rib repeats
   fp fRibRepeat = m_fPatternWidth / max(m_fRibWidth, fPixelLen);
   fRibRepeat = floor(fRibRepeat+0.5);
   fRibRepeat = max(fRibRepeat, 1);
   fp fRibNoise =  m_fRibNoise / m_fPatternWidth;

   // colors
   WORD awColor[3], awChannel[3], awEdge[3];
   Gamma (m_cColor, awColor);
   Gamma (m_cColorChannel, awChannel);
   Gamma (m_cColorEdge, awEdge);

   // adjust the depth
   fp fPow = 1.0 - (m_fDepthSquare - 0.5) * 2;
   fp fDepthScale = 0.5 * m_fDepth / fPixelLen;
   fp fOffset = (0.5 - m_fBarkChannel) * 2;
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   DWORD x,y;
   for (y = 0; y < dwY; y++) {
      fp fY = (fp)y / (fp)dwY;
      fp fX;
      fp fXDelta = 1.0 / (fp)dwX;

      for (x = 0, fX = 0; x < dwX; x++, fX += fXDelta, pip++) {
         fp fVal = fOffset;
         fp fDetailScale = 1.0 - m_fRibStrength;
         fp fRes = 1;

         // ribs
         fp fNoiseBend = fRibNoise * NoiseWave.Value (fX, fY);
         fVal += sin((fX + fNoiseBend) * fRibRepeat * 2 * PI) * m_fRibStrength;

         for (i = 0; i < 4; i++, fDetailScale *= m_fNoiseDetail, fRes *= 2)
            fVal += fDetailScale * Noise.Value ((fX + fNoiseBend) * fRes, fY * fRes);

         fp fWeightEdge, fWeightChannel, fWeightBase;
         fWeightEdge = fWeightChannel = fWeightBase = 0;

         if (fVal >= 0)
            fVal = pow(fVal, fPow);
         else
            fVal = -pow(-fVal, fPow);

         // depth
         pip->fZ += fVal * fDepthScale;;

         // determine the color
         if (fVal > 0) {
            fVal = sqrt(fVal);
            fWeightEdge = (1.0 - fVal) * m_fBlendEdge;
            fWeightBase = 1.0 - fWeightEdge;
         }
         else {
            fVal = sqrt(-fVal);
            fWeightEdge = (1.0 - fVal) * m_fBlendEdge;
            fWeightChannel = fVal * m_fBlendChannel;
            fWeightBase = 1.0 - (fWeightEdge + fWeightChannel);
         }
         for (i = 0; i < 3; i++) {
            fRes = fWeightEdge * (fp) awEdge[i] +
               fWeightChannel * (fp)awChannel[i] +
               fWeightBase * (fp)((&pip->wRed)[i]);
            fRes = max(fRes, 0);
            fRes = min(fRes, (fp)0xffff);
            (&pip->wRed)[i] = (WORD)fRes;
         } // i
      } // x
   } // y


   // apply bump map
   // pImage->TGBumpMapApply (); - dont do this
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);


   return TRUE;
}



