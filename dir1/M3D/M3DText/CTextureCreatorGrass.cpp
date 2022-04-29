/********************************************************************************
CTextCreatorGrass.cpp - Code for handling grass.

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
CLeafForTexture::Constructor and destructor
*/
CLeafForTexture::CLeafForTexture (void)
{
   Clear();
}

CLeafForTexture::~CLeafForTexture (void)
{
   // do nothing
}


/****************************************************************************
CLeafForTexture::CloneTo - Standard API
*/
BOOL CLeafForTexture::CloneTo (CLeafForTexture *pTo)
{
   pTo->Clear();

   // NOTE: Not tested

   pTo->m_fUseText = m_fUseText;
   pTo->m_fWidth = m_fWidth;
   pTo->m_fHeight = m_fHeight;
   pTo->m_fSizeVar = m_fSizeVar;
   pTo->m_fWidthVar = m_fWidthVar;
   pTo->m_fHeightVar = m_fHeightVar;
   pTo->m_fHueVar = m_fHueVar;
   pTo->m_fSatVar = m_fSatVar;
   pTo->m_fLightVar = m_fLightVar;
   pTo->m_fDarkEdge = m_fDarkEdge;
   pTo->m_dwShape = m_dwShape;
   pTo->m_cShapeColor = m_cShapeColor;
   m_Text.CloneTo (&pTo->m_Text);
   pTo->m_fAllowMirror = m_fAllowMirror;

   return TRUE;
}



/****************************************************************************
CLeafForTexture::Clone - Standard API
*/
CLeafForTexture *CLeafForTexture::Clone (void)
{
   // NOTE: Not tested

   PCLeafForTexture pNew = (PCLeafForTexture) new CLeafForTexture;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


static PWSTR gpszLeafForTexture = L"LeafForTexture";
static PWSTR gpszUseText = L"UseText";
static PWSTR gpszWidth = L"Width";
static PWSTR gpszHeight = L"Height";
static PWSTR gpszSizeVar = L"SizeVar";
static PWSTR gpszWidthVar = L"WidthVar";
static PWSTR gpszHeightVar = L"HeightVar";
static PWSTR gpszHueVar = L"HueVar";
static PWSTR gpszSatVar = L"SatVar";
static PWSTR gpszLightVar = L"LightVar";
static PWSTR gpszAllowMirror = L"AllowMirror";
static PWSTR gpszText = L"Text";
static PWSTR gpszShape = L"Shape";
static PWSTR gpszShapeColor = L"ShapeColor";
static PWSTR gpszDarkEdge = L"DarkEdge";

/****************************************************************************
CLeafForTexture::MMLTo - Standard API
*/
PCMMLNode2 CLeafForTexture::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLeafForTexture);

   MMLValueSet (pNode, gpszUseText, (int)m_fUseText);
   MMLValueSet (pNode, gpszWidth, m_fWidth);
   MMLValueSet (pNode, gpszHeight, m_fHeight);
   MMLValueSet (pNode, gpszSizeVar, m_fSizeVar);
   MMLValueSet (pNode, gpszWidthVar, m_fWidthVar);
   MMLValueSet (pNode, gpszHeightVar, m_fHeightVar);
   MMLValueSet (pNode, gpszHueVar, m_fHueVar);
   MMLValueSet (pNode, gpszSatVar, m_fSatVar);
   MMLValueSet (pNode, gpszLightVar, m_fLightVar);
   MMLValueSet (pNode, gpszDarkEdge, m_fDarkEdge);
   MMLValueSet (pNode, gpszAllowMirror, (int)m_fAllowMirror);

   if (m_fUseText) {
      // texture
      PCMMLNode2 pSub = m_Text.MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszText);
         pNode->ContentAdd (pSub);
      }
   }
   else {
      //shape
      MMLValueSet (pNode, gpszShape, (int)m_dwShape);
      MMLValueSet (pNode, gpszShapeColor, (int)m_cShapeColor);
   }

   return pNode;
}


/****************************************************************************
CLeafForTexture::MMLFrom - Standard API
*/
BOOL CLeafForTexture::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   m_fUseText = (BOOL) MMLValueGetInt (pNode, gpszUseText, (int)m_fUseText);
   m_fWidth = MMLValueGetDouble (pNode, gpszWidth, m_fWidth);
   m_fHeight = MMLValueGetDouble (pNode, gpszHeight, m_fHeight);
   m_fSizeVar = MMLValueGetDouble (pNode, gpszSizeVar, m_fSizeVar);
   m_fWidthVar = MMLValueGetDouble (pNode, gpszWidthVar, m_fWidthVar);
   m_fHeightVar = MMLValueGetDouble (pNode, gpszHeightVar, m_fHeightVar);
   m_fHueVar = MMLValueGetDouble (pNode, gpszHueVar, m_fHueVar);
   m_fSatVar = MMLValueGetDouble (pNode, gpszSatVar, m_fSatVar);
   m_fLightVar = MMLValueGetDouble (pNode, gpszLightVar, m_fLightVar);
   m_fDarkEdge = MMLValueGetDouble (pNode, gpszDarkEdge, m_fDarkEdge);
   m_fAllowMirror = (BOOL) MMLValueGetInt (pNode, gpszAllowMirror, (int)m_fAllowMirror);

   if (m_fUseText) {
      // texture
      PCMMLNode2 pSub = NULL;
      PWSTR psz;
      pNode->ContentEnum (pNode->ContentFind (gpszText), &psz, &pSub);
      if (pSub)
         m_Text.MMLFrom (pSub);
   }
   else {
      //shape
      m_dwShape = (DWORD) MMLValueGetInt (pNode, gpszShape, (int)m_dwShape);
      m_cShapeColor = (COLORREF) MMLValueGetInt (pNode, gpszShapeColor, (int)m_cShapeColor);
   }

   return TRUE;
}



/****************************************************************************
CLeafForTexture::Clear - Standard API
*/
void CLeafForTexture::Clear (void)
{
   m_fUseText = FALSE;
   m_fWidth = 10;
   m_fHeight = 20;
   m_fSizeVar = m_fWidthVar = m_fHeightVar = 0.25;
   m_fHueVar = m_fSatVar = m_fLightVar = 0.25;
   m_fDarkEdge = 0.25;  // BUGFIX - Changed from 0.5 to 0.25

   m_dwShape = 0;
   m_cShapeColor = RGB (0xff, 0xff, 0x80);

   m_fAllowMirror = TRUE;
   m_Text.Clear (TRUE);
}

/****************************************************************************
CLeafForTexture::TextureCache - Causes the texture to be cached. Call this
before calling the renders.

returns
   BOOL - TRUE if acutally cached
*/
BOOL CLeafForTexture::TextureCache (DWORD dwRenderShard)
{
   return m_Text.TextureCache (dwRenderShard);
}

/****************************************************************************
CLeafForTexture::TextureRelease - Causes the texture cache to be released. Call this
before calling the renders.

returns
   BOOL - TRUE if acutally cached
*/
BOOL CLeafForTexture::TextureRelease (void)
{
   return m_Text.TextureRelease ();
}


/****************************************************************************
CLeafForTexture::TextureQuery - Standard API for determining what textures
this texture handles
*/
BOOL CLeafForTexture::TextureQuery (DWORD dwRenderShard, PCListFixed plText, PCBTree pTree)
{
   // NOTE: Not tested

   if (m_fUseText)
      return m_Text.TextureQuery (dwRenderShard, plText, pTree);
   else
      return TRUE;
}



/****************************************************************************
CLeafForTexture::SubTextureNoRecurse - Standard API for determining what textures
this texture handles
*/
BOOL CLeafForTexture::SubTextureNoRecurse (DWORD dwRenderShard, PCListFixed plText)
{
   // NOTE: Not tested

   if (m_fUseText)
      return m_Text.SubTextureNoRecurse (dwRenderShard, plText);
   else
      return TRUE;
}


/****************************************************************************
CLeafForTexture::AverageColor - Returns the average color of the leaf.
*/
//COLORREF CLeafForTexture::AverageColor (void)
//{
   // NOTE - will need to do this for texture

   // NOTE - may not need average color

   // if not texture, then just return color
//   return m_cShapeColor;
//}

/****************************************************************************
ApplyHLS - Apply HLS to a color

inputs
   WORD        *pawRGB - R, G, B color. Changed in place
   fp          fHue - Hue change, -0.5 to 0.5.
   fp          fSat - Saturation change, 0 to 2
   fp          fBright - Brightness scale
returns
   none
*/
void ApplyHLS (WORD *pawRGB, fp fHue, fp fSat, fp fBright)
{
   // apply hue and saturation
   WORD awHLS[3];
   ToHLS (pawRGB[0], pawRGB[1], pawRGB[2], &awHLS[0], &awHLS[1], &awHLS[2]);

   // hue
   if (fHue < 0)
      fHue += 1.0;
   awHLS[0] += (WORD)(fHue * (fp)0x10000);

   // saturation
   fSat *= (fp)awHLS[2];
   if (fSat < (fp)0xffff)
      awHLS[2] = (WORD)fSat;
   else
      awHLS[2] = 0xffff;

   // return back
   FromHLS (awHLS[0], awHLS[1], awHLS[2], &pawRGB[0], &pawRGB[1], &pawRGB[2]);

   // scale
   DWORD i;
   fp f;
   for (i = 0; i < 3; i++) {
      f = fBright * (fp)pawRGB[i];
      if (f < (fp)0xffff)
         pawRGB[i] = (WORD)f;
      else
         pawRGB[i] = 0xffff;
   } // i
}


/****************************************************************************
CLeafForTexture::FindMaxZ - Looks at the area where the leaf would
be rendered and finds the maximum Z value. Used to ensure that leaves stick
above other leaves.

inputs
   PCImage           pImage - Image to draw onto
   fp                fScale - Scale for width and height. Used with high/low res textures
   PCPoint           pStart - Starting pixel
   PCPoint           pDirection - Direction
returns
   fp - Maximum z
*/
fp CLeafForTexture::FindMaxZ (PCImage pImage,fp fScale, PCPoint pStart, PCPoint pDirection)
{
   // easy out
   //if (!m_fUseText && !m_dwShape)
   //   return;

   fp fSize = 1.0 /*randf (1.0 - m_fSizeVar, 1.0 + m_fSizeVar)*/ * fScale;
      // NOTE: because call this before, cant use random
   fp fWidth = m_fWidth * fSize;
   fp fHeight = m_fHeight * fSize;

   fp fWidthVar = 1; // cant use random sqrt(randf (1.0 - m_fWidthVar, 1.0));
   fp fHeightVar = 1; // cant use random sqrt(randf (1.0 - m_fHeightVar, 1.0));
   fWidth *= fWidthVar;
   fHeight *= fHeightVar;



   // create a matrix that goes from texture 0..1, 0..1 into pixels
   CMatrix mHVToPixel, mTrans, mScale;
   mTrans.Translation (-0.5, -1, 0);   // translate so centered on LR
   mScale.Scale (fWidth /** ((m_fAllowMirror && (rand()%2)) ? -1 : 1) */, -fHeight, 1);
      // so goes y is positive at top, and to pixel size
      // also do mirror of L/R
   mHVToPixel.Multiply (&mScale, &mTrans);

   // rotation
   CPoint pX, pY, pZ;
   pY.Copy (pDirection);
   pY.p[1] *= -1; // negative y
   pY.Normalize();
   pZ.Zero();
   pZ.p[2] = 1;
   pX.CrossProd (&pY, &pZ);
   CMatrix mRot;
   mRot.RotationFromVectors (&pX, &pY, &pZ);
   mHVToPixel.MultiplyRight (&mRot);

   // translate to start
   mTrans.Translation (pStart->p[0], -pStart->p[1], pStart->p[2]);
   mHVToPixel.MultiplyRight (&mTrans);

   // scale back -1 x y
   mScale.Scale (1, -1, 1);
   mHVToPixel.MultiplyRight (&mScale);

   // invert this
   CMatrix mPixelToHV;
   mHVToPixel.Invert4 (&mPixelToHV);

   // find the limits of where need to draw
   CPoint pMin, pMax, p;
   DWORD i;
   for (i = 0; i < 4; i++) {
      p.Zero();
      p.p[0] = (i%2) ? 1.0 : 0.0;
      p.p[1] = (i/2) ? 1.0 : 0.0;
      p.MultiplyLeft (&mHVToPixel);
      if (i) {
         pMin.Min (&p);
         pMax.Max (&p);
      }
      else {
         pMin.Copy (&p);
         pMax.Copy (&p);
      }
   } // i

   // rectangle to cover
   RECT r;
   r.left = floor (pMin.p[0]);
   r.right = ceil (pMax.p[0]);
   r.top = floor (pMin.p[1]);
   r.bottom = ceil (pMax.p[1]);

   // loop
   int iX, iY, iDrawX, iDrawY;

   int iWidth = (int)pImage->Width();
   int iHeight = (int)pImage->Height();

   fp fZMax = -ZINFINITE;
   for (iY = r.top; iY < r.bottom; iY++) {
      // mod
      iDrawY = iY;
      if (iDrawY < 0)
         iDrawY += iHeight * (-iDrawY / iHeight + 1);
      iDrawY = iDrawY % iHeight;

      for (iX = r.left; iX < r.right; iX++) {
         // mod
         iDrawX = iX;
         if (iDrawX < 0)
            iDrawX += iWidth * (-iDrawY / iWidth + 1);
         iDrawX = iDrawX % iWidth;

         // draw
         PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iDrawX, (DWORD)iDrawY);
         fZMax = max(fZMax, pip->fZ);
      } // iX

   } // iY

   if (fZMax == -ZINFINITE)
      return 0;
   else
      return fZMax;
}


/****************************************************************************
CLeafForTexture::FindMinZInTexture - Looks at the texture and finds the mimum
opaque Z-value, to ensure that the leaf stick above the grass.

Before calling this, make sure the texture has been cached.

inputs
returns
   fp - Minimum z, or 0 if is flat. In meters, NOT pixels
*/
fp CLeafForTexture::FindMinZInTexture (void)
{
   // easy out
   if (!m_fUseText)
      return 0;

   TEXTPOINT5 tpText, tpMax;
   fp fRet = ZINFINITE;
   fp fPixelHeight;
   memset (&tpText, 0, sizeof(tpText));
   memset (&tpMax, 0, sizeof(tpMax));
   WORD awRGB[3];
   CMaterial mat;
   for (tpText.hv[0] = 0.0; tpText.hv[0] < 1.0; tpText.hv[0] += 0.1)
      for (tpText.hv[1] = 0.0; tpText.hv[1] < 1.0; tpText.hv[1] += 0.1) {
         if (!m_Text.FillPixel (0, TMFP_TRANSPARENCY, awRGB, &tpText, &tpMax, &mat, NULL, FALSE))
            continue;   // out of range
         if (mat.m_wTransparency >= 0x8000)
            continue;   // skip

         m_Text.PixelBump (0, &tpText, &tpMax, &tpMax, NULL, &fPixelHeight, FALSE); // dont bother with high quality
         fRet = min(fRet, fPixelHeight);
      } // hv[0] and hv[1]

   if (fRet == ZINFINITE)
      return 0;   // since nothing
   else
      return fRet;   // NOTE: no need to scale since PixelBump should already be in meters
}

/****************************************************************************
CLeafForTexture::Render - Draws the leaf.

Before calling this:
- Set SRand()
- Make sure TextureCache() has been called.

inputs
   PCImage           pImage - Image to draw onto
   fp                fBrightness - Brightness to use, so that leaves lower down can be darker
   fp                fScale - Scale for width and height. Used with high/low res textures
   PCPoint           pStart - Starting pixel
   PCPoint           pDirection - Direction
   fp                fZDelta - Change the Z amount of the drawn leaf by this much. if this is -ZINFINITE then ignore
*/
void CLeafForTexture::Render (PCImage pImage, fp fBrightness, fp fScale, PCPoint pStart, PCPoint pDirection,
                              fp fZDelta)
{
   // easy out
   if (!m_fUseText && !m_dwShape)
      return;

   fp fSize = randf (1.0 - m_fSizeVar, 1.0 + m_fSizeVar) * fScale;
   fp fWidth = m_fWidth * fSize;
   fp fHeight = m_fHeight * fSize;

   fp fWidthVar = sqrt(randf (1.0 - m_fWidthVar, 1.0));
   fp fHeightVar = sqrt(randf (1.0 - m_fHeightVar, 1.0));
   fp fAngleBright = sqrt((1.0 - fWidthVar) * (1.0 - fHeightVar));
   fBrightness *= (1.0 + ((rand()%2) ? 1 : -1) * fAngleBright * 0.75);
      // either make brighter or darker according to rotation, but only by 0.75, assuming shadow = 1/4 full light
   fWidth *= fWidthVar;
   fHeight *= fHeightVar;

   // determine hue, saturation, lightness
   fp fHue = randf (-m_fHueVar, m_fHueVar) / 2.0;
   fp fSat = randf (1.0 - m_fSatVar, 1.0 + m_fSatVar);
   fBrightness *= randf (1.0 - m_fLightVar, 1.0 + m_fLightVar);

   // color
   WORD awRGB[3];

   if (!m_fUseText) {
      Gamma (m_cShapeColor, awRGB);
      ApplyHLS (awRGB, fHue, fSat, fBrightness);

      // potentially change shape
      DWORD dwShape = m_dwShape;
      if (m_fAllowMirror) switch (dwShape) {
      case 8:
         if (rand() % 2)
            dwShape = 9;   // flip left
         break;
      case 9:
         if (rand() % 2)
            dwShape = 8;   // flip right
         break;
      } // m_dwShape

      // draw the leave
      DrawGrassTip (pImage, UnGamma(awRGB), pStart, pDirection, fWidth, fHeight, dwShape, fZDelta);
      return;
   }

   // else, drawing texture

   // create a matrix that goes from texture 0..1, 0..1 into pixels
   CMatrix mHVToPixel, mTrans, mScale;
   mTrans.Translation (-0.5, -1, 0);   // translate so centered on LR
   mScale.Scale (fWidth * ((m_fAllowMirror && (rand()%2)) ? -1 : 1), -fHeight, 1);
      // so goes y is positive at top, and to pixel size
      // also do mirror of L/R
   mHVToPixel.Multiply (&mScale, &mTrans);

   // rotation
   CPoint pX, pY, pZ;
   pY.Copy (pDirection);
   pY.p[1] *= -1; // negative y
   pY.Normalize();
   pZ.Zero();
   pZ.p[2] = 1;
   pX.CrossProd (&pY, &pZ);
   CMatrix mRot;
   mRot.RotationFromVectors (&pX, &pY, &pZ);
   mHVToPixel.MultiplyRight (&mRot);

   // translate to start
   mTrans.Translation (pStart->p[0], -pStart->p[1], pStart->p[2]);
   mHVToPixel.MultiplyRight (&mTrans);

   // scale back -1 x y
   mScale.Scale (1, -1, 1);
   mHVToPixel.MultiplyRight (&mScale);

   // invert this
   CMatrix mPixelToHV;
   mHVToPixel.Invert4 (&mPixelToHV);

   // find the limits of where need to draw
   CPoint pMin, pMax, p;
   DWORD i;
   for (i = 0; i < 4; i++) {
      p.Zero();
      p.p[0] = (i%2) ? 1.0 : 0.0;
      p.p[1] = (i/2) ? 1.0 : 0.0;
      p.MultiplyLeft (&mHVToPixel);
      if (i) {
         pMin.Min (&p);
         pMax.Max (&p);
      }
      else {
         pMin.Copy (&p);
         pMax.Copy (&p);
      }
   } // i

   // rectangle to cover
   RECT r;
   r.left = floor (pMin.p[0]);
   r.right = ceil (pMax.p[0]);
   r.top = floor (pMin.p[1]);
   r.bottom = ceil (pMax.p[1]);

   // determine the deltas
   CPoint pUL, pRight, pDown;
   pUL.Zero();
   pUL.p[0] = r.left;
   pUL.p[1] = r.top;
   pRight.Copy (&pUL);
   pRight.p[0] += 1;
   pDown.Copy (&pUL);
   pDown.p[1] += 1;
   pUL.MultiplyLeft (&mPixelToHV);
   pRight.MultiplyLeft (&mPixelToHV);
   pDown.MultiplyLeft (&mPixelToHV);
   pRight.Subtract (&pUL);
   pDown.Subtract (&pUL);

   // loop
   int iX, iY, iDrawX, iDrawY;
   CMaterial mat;
   TEXTPOINT5 tpText, tpMax, tpMaxBlurry, tpZero;
   memset (&tpText, 0, sizeof(tpText));
   memset (&tpMax, 0, sizeof(tpMax));
   memset (&tpZero, 0, sizeof(tpZero));
   tpMax.hv[0] = max(fabs(pRight.p[0]), fabs(pDown.p[0]));
   tpMax.hv[1] = max(fabs(pRight.p[1]), fabs(pDown.p[1]));
   tpMaxBlurry = tpMax;
   tpMaxBlurry.hv[0] *= 8.0;  // BUGFIX - Was *4.0
   tpMaxBlurry.hv[1] *= 8.0;  // BUGFIX - Was *4.0

   int iWidth = (int)pImage->Width();
   int iHeight = (int)pImage->Height();
   fp fCurHeight;

   for (iY = r.top; iY < r.bottom; iY++) {
      p.Copy (&pUL);
      
      // mod
      iDrawY = iY;
      if (iDrawY < 0)
         iDrawY += iHeight * (-iDrawY / iHeight + 1);
      iDrawY = iDrawY % iHeight;

      for (iX = r.left; iX < r.right; iX++) {
         // get the pixel
         mat.m_wTransparency = 0;   // default to no transparency, since only care about that
         tpText.hv[0] = p.p[0];
         tpText.hv[1] = p.p[1];

         if (!m_Text.FillPixel (0, TMFP_TRANSPARENCY, awRGB, &tpText, &tpMaxBlurry, &mat, NULL, FALSE))
            goto inc;   // out of range

         // if transparency > half then skip
         if (mat.m_wTransparency >= 0xe000)  // BUGFIX - Was 0x8000
            goto inc;

         // dark edges based on transparency
         fp fDark = (fp)mat.m_wTransparency / (fp)0xffff;
         fDark *= 2.0;  // since if >= 0x8000 then cut
         fDark = min(fDark, 1.0);
         fDark = (1.0 - fDark) * 1.0 + fDark * m_fDarkEdge;

         // get sharper version
         if (!m_Text.FillPixel (0, TMFP_TRANSPARENCY, awRGB, &tpText, &tpMax, &mat, NULL, FALSE))
            goto inc;   // out of range

         if (mat.m_wTransparency >= 0x8000)  // intentionally 0x8000
            goto inc;

         // apply coloration
         ApplyHLS (awRGB, fHue, fSat, fBrightness * fDark);

         // mod
         iDrawX = iX;
         if (iDrawX < 0)
            iDrawX += iWidth * (-iDrawY / iWidth + 1);
         iDrawX = iDrawX % iWidth;

         // draw
         PIMAGEPIXEL pip = pImage->Pixel ((DWORD)iDrawX, (DWORD)iDrawY);
         pip->wRed = awRGB[0];
         pip->wGreen = awRGB[1];
         pip->wBlue = awRGB[2];

         // potentially get the pixel height change
         if (fZDelta != -ZINFINITE) {
            fCurHeight = 0;
            m_Text.PixelBump (0, &tpText, &tpZero, &tpZero, NULL, &fCurHeight, TRUE);
            pip->fZ = fZDelta + fCurHeight;
         }


inc:
         // increment
         p.Add (&pRight);
      } // iX

      // increment
      pUL.Add (&pDown);
   } // iY
}



/****************************************************************************
LeafForTexturePage
*/
BOOL LeafForTexturePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCLeafForTexture pv = (PCLeafForTexture) pPage->m_pUserData;

   // call sub-textures for trap
   if (pv->m_Text.PageTrapMessages (pv->m_dwRenderShardTemp, L"Text", pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // checkboxes
         if (pControl = pPage->ControlFind (L"usetext"))
            pControl->AttribSetBOOL (Checked(), pv->m_fUseText);
         if (pControl = pPage->ControlFind (L"allowmirror"))
            pControl->AttribSetBOOL (Checked(), pv->m_fAllowMirror);

         // values
         DoubleToControl (pPage, L"width", pv->m_fWidth);
         DoubleToControl (pPage, L"height", pv->m_fHeight);

         // scrollbars
         if (pControl = pPage->ControlFind (L"sizevar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSizeVar * 100.0));
         if (pControl = pPage->ControlFind (L"widthvar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fWidthVar * 100.0));
         if (pControl = pPage->ControlFind (L"heightvar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHeightVar * 100.0));
         if (pControl = pPage->ControlFind (L"huevar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fHueVar * 100.0));
         if (pControl = pPage->ControlFind (L"satvar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fSatVar * 100.0));
         if (pControl = pPage->ControlFind (L"lightvar"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fLightVar * 100.0));
         if (pControl = pPage->ControlFind (L"darkedge"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDarkEdge * 100.0));

         FillStatusColor (pPage, L"shapecolor", pv->m_cShapeColor);
         ComboBoxSet (pPage, L"shape", pv->m_dwShape);

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
         if (!_wcsicmp(p->pControl->m_pszName, L"sizevar")) {
            pv->m_fSizeVar = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"widthvar")) {
            pv->m_fWidthVar = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"heightvar")) {
            pv->m_fHeightVar = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"huevar")) {
            pv->m_fHueVar = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"satvar")) {
            pv->m_fSatVar = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"lightvar")) {
            pv->m_fLightVar = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"darkedge")) {
            pv->m_fDarkEdge = fVal;
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszGrassButton = L"grassbutton";
         DWORD dwGrassButtonLen = (DWORD)wcslen(pszGrassButton);

         if (!_wcsicmp(p->pControl->m_pszName, L"usetext")) {
            pv->TextureRelease();   // just in case
            pv->m_fUseText = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"allowmirror")) {
            pv->m_fAllowMirror = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shapecolorbutton")) {
            pv->m_cShapeColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cShapeColor,
               pPage, L"shapecolor");
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"shape")) {
            DWORD dwVal;
            dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwShape)
               break; // unchanged

            pv->m_dwShape = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:  // get all the control values
      {
         pv->m_fWidth = DoubleFromControl (pPage, L"width");
         pv->m_fWidth = max(pv->m_fWidth, 0.1);
         pv->m_fHeight = DoubleFromControl (pPage, L"height");
         pv->m_fHeight = max(pv->m_fHeight, 0.1);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Leaf/flower";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


/****************************************************************************
CLeafForTexture::Dialog - Dialog to modify the leaf

inputs
   PCEscWindow       pWindow - Window to display it in
returns
   BOOL - TRUE if user pressed back. FALSE if closed.
*/
BOOL CLeafForTexture::Dialog (DWORD dwRenderShard, PCEscWindow pWindow)
{
   m_dwRenderShardTemp = dwRenderShard;

   PWSTR pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEAFFORTEXTURE, LeafForTexturePage, this);

   return pszRet && !_wcsicmp(pszRet, Back());
}




/****************************************************************************
DrawGrassBlade - Draws a grass blade.

inputs
   PCImage        pImage - Image
   COLORREF       cColor - Color
   PCPoint        pStart - Will be modified in place to indicate final location for end of grass
   PCPoint        pDirection - Direction that the grass-blade heads. Will be modified in place
                     to indicate where pointing to
   DWORD          dwSegments - Number of segments
   fp             fSegmentLen - Length of each segment, in pixels
   fp             fThicknessStart - Thickness of the grass at the bottom, in pixels
   fp             fThicknessMiddle - Thickness in the middle of the grass, in pixels
   fp             fThicknessEnd - Thickenss of the grass at the end, in pixels
   fp             fGravity - How much gravity pulls down on direction each go, 0 to 1
   fp             fDarkEdge - How much darker to make the edge, from 0..2.0, 1.0 being no difference
returns
   none
*/
void DrawGrassBlade (PCImage pImage, COLORREF cColor, PCPoint pStart, PCPoint pDirection,
                     DWORD dwSegments, fp fSegmentLen, fp fThicknessStart, fp fThicknessMiddle, fp fThicknessEnd,
                     fp fGravity, fp fDarkEdge)
{
   CListFixed lPoints;
   lPoints.Init (sizeof(CPoint), pStart, 1);

   pDirection->Normalize();   // just in case

   // loop over all the segments, creating a list of points
   DWORD i;
   CPoint pCur;
   lPoints.Required (lPoints.Num() + dwSegments);
   for (i = 0; i < dwSegments; i++) {
      pCur.Copy (pDirection);
      pCur.Scale (fSegmentLen);
      pCur.Add (pStart);
      lPoints.Add (&pCur);

      // new start
      pStart->Copy (&pCur);

      // adjust direction
      pDirection->p[1] += fGravity;
      pDirection->Normalize();
      if (pDirection->Length() < 0.5)
         pDirection->p[1] = -1;  // ended up being 0 lenght. make sure not to do this
   } // i

   // draw this
#define NUMDARKEDGE     3
   fp fCenter = (fp)(NUMDARKEDGE-1) / 2.0;
   for (i = 0; i < NUMDARKEDGE; i++) {
      fp fScaleColor = fCenter ? (1.0 - (fp)i / fCenter) : 1.0;
      fScaleColor = fScaleColor * (fDarkEdge - 1.0) + 1.0;

      WORD awColor[3];
      Gamma (cColor, awColor);
      DWORD dwC;
      fp fC;
      for (dwC = 0; dwC < 3; dwC++) {
         fC = (fp)awColor[dwC] * fScaleColor;
         fC = max(fC, 0);
         fC = min (fC, (fp)0xffff);
         awColor[dwC] = (WORD)fC;
      } // dwC
      COLORREF cDark = UnGamma (awColor);

      fp fScaleThick = 1.0 - (fp)i / (fp)NUMDARKEDGE;

      DrawCurvedLineSegment (pImage, lPoints.Num(), (PCPoint) lPoints.Get(0),
         fThicknessStart * fScaleThick, fThicknessMiddle * fScaleThick, fThicknessEnd * fScaleThick, cDark, cDark, -ZINFINITE);
   } // i
}


/****************************************************************************
DrawGrassTip - Draws the tip of grass or a flower

inputs
   PCImage        pImage - Image
   COLORREF       cColor - Color
   PCPoint        pStart - Start of the tip
   PCPoint        pDirection - Direction that the grass-blade heads. MUST be normalized
   fp             fWidth - Width in pixels
   fp             fLength - Length in pixels
   DWORD          dwNum - Tip number
   fp             fZDelta - Amount fo save the height as. If -ZINFINITE then ignore
returns
   none
*/
#define NUMPINENEEDLE   8
#define SICKLENUM       5

void DrawGrassTip (PCImage pImage, COLORREF cColor, PCPoint pStart, PCPoint pDirection,
                     fp fWidth, fp fLength, DWORD dwNum, fp fZDelta)
{
   CPoint pPerp, pUp;
   pUp.Zero();
   pUp.p[2] = 1;
   pPerp.CrossProd (pDirection, &pUp);
   pPerp.Normalize();   // just in case


   DWORD i;
   CPoint p1, p2, p3, p4, p5;
   CPoint apSickle[SICKLENUM];

   switch (dwNum) {
   case 1:  // pointy leaf
      // p2 = pStart + fLength*pDirection
      p2.Copy (pDirection);
      p2.Scale (fLength);
      p2.Add (pStart);

      // p1 = (pStart + p2) / 2
      p1.Average (pStart, &p2);

      // two lines
      DrawLineSegment (pImage, pStart, &p1, fWidth/10, fWidth, cColor, fZDelta);
      DrawLineSegment (pImage, &p1, &p2, fWidth, fWidth/10, cColor, fZDelta);
      break;

   case 2:  // leaf, diamond on end
   case 3:  // leaf, diamon at start
      // p2 = pStart + fLength*pDirection
      p2.Copy (pDirection);
      p2.Scale (fLength);
      p2.Add (pStart);
      DrawLineSegment (pImage, pStart, &p2,
         fWidth / ((dwNum==2) ? 10 : 1),
         fWidth / ((dwNum==2) ? 1 : 10),
         cColor, fZDelta);
      break;

   case 4:  // pine
      // p2 = pStart + fLength*pDirection
      p2.Copy (pDirection);
      p2.Scale (fLength);
      p2.Add (pStart);
      DrawLineSegment (pImage, pStart, &p2, fWidth/NUMPINENEEDLE/2, fWidth/NUMPINENEEDLE/2, cColor, fZDelta);

      p3.Copy (&pPerp);
      p3.Scale (fWidth/2);

      for (i = 0; i < NUMPINENEEDLE; i++) {
         p1.Average (pStart, &p2, (fp)i / (NUMPINENEEDLE));

         p4.Copy (pDirection);
         p4.Scale (fWidth/2);
         p4.Add (&p1);

         // needles on either side
         p5.Add (&p4, &p3);
         DrawLineSegment (pImage, &p1, &p5, fWidth/NUMPINENEEDLE/2, fWidth/NUMPINENEEDLE/2, cColor, fZDelta);
         p5.Subtract (&p4, &p3);
         DrawLineSegment (pImage, &p1, &p5, fWidth/NUMPINENEEDLE/2, fWidth/NUMPINENEEDLE/2, cColor, fZDelta);
      } // i
      break;

   case 5:  // 3 prong
   case 6:  // 5 prong
   case 7:  // 7 prong
      DWORD dwProngsSide;
      dwProngsSide = (dwNum - 4);

      fp fProngWidth;
      fProngWidth = fWidth / 2; // (fp)(dwProngsSide+1);

      // p2 = pStart + fLength*pDirection
      p2.Copy (pDirection);
      p2.Scale (fLength);
      p2.Add (pStart);
      DrawLineSegment (pImage, pStart, &p2, fProngWidth/2, fProngWidth, cColor, fZDelta);

      // perp
      p3.Copy (&pPerp);
      p3.Scale (fWidth/2);

      // L/R prongs
      for (i = 0; i < dwProngsSide; i++) {
         p1.Average (&p2, pStart, (fp)i / (fp)(dwProngsSide+1));
         p4.Average (&p2, pStart, (fp)(i+1) / (fp)(dwProngsSide+1));
         
         p5.Add (&p4, &p3);
         DrawLineSegment (pImage, &p1, &p5, fProngWidth/2, fProngWidth, cColor, fZDelta);
         p5.Subtract (&p4, &p3);
         DrawLineSegment (pImage, &p1, &p5, fProngWidth/2, fProngWidth, cColor, fZDelta);
      } // i
      break;

   case 8:  // sickle lefeaf, left
   case 9:  // sickle lefeaf, right
      // p2 = pStart + fLength*pDirection
      p2.Copy (pDirection);
      p2.Scale (fLength);
      p2.Add (pStart);

      // p1 = (pStart + p2) / 2 + pPerp
      p1.Average (pStart, &p2);
      p3.Copy (&pPerp);
      p3.Scale (0.5 * fWidth * ((dwNum==8) ? -1 : 1));

      for (i = 0; i < SICKLENUM; i++) {
         apSickle[i].Copy (&p3);
         apSickle[i].Scale (sin((fp)i / (fp)(SICKLENUM-1) * PI));

         p4.Average (&p2, pStart, (fp)i / (fp)(SICKLENUM-1));
         apSickle[i].Add (&p4);
      }

      DrawCurvedLineSegment (pImage, SICKLENUM, apSickle, fWidth/10, fWidth, fWidth/10,
         cColor, cColor, -ZINFINITE);
      break;

   case 21: // round
   case 22: // 3 flower
   case 23: // 4 flower
   case 24: // 5 flower
   case 25: // 6 flower
      DWORD dwPetals;
      dwPetals = dwNum - 19;
      fp fPetalWidth;
      fPetalWidth = (fWidth+fLength) / (fp)(dwPetals+1);

      if (dwPetals < 3) {
         dwPetals = 16;
         fPetalWidth = (fWidth+fLength) / (fp)(5+1);
      }

      // p1 = lengthwise
      p1.Copy (pDirection);
      p1.Scale (fLength/2);
      
      // p2 = perp
      p2.Copy (&pPerp);
      p2.Scale (fWidth/2);

      for (i = 0; i < dwPetals; i++) {
         // figure out sin and cos based on angle
         fp fAngle = (fp)i / (fp)dwPetals * 2.0 * PI;
         p3.Copy (&p1);
         p3.Scale (sin(fAngle));
         p4.Copy (&p2);
         p4.Scale (cos(fAngle));

         p3.Add (&p4);
         p3.Add (pStart);
         DrawLineSegment (pImage, pStart, &p3, fPetalWidth/4, fPetalWidth, cColor, fZDelta);

      } // i

      break;

   } // switch
}


/****************************************************************************
GrassPage
*/
BOOL GrassPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorGrass pv = (PCTextCreatorGrass) pt->pThis;

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


         
         //ComboBoxSet (pPage, L"tipshape", pv->m_dwTipShape);

         DoubleToControl (pPage, L"patternwidth", pv->m_iWidth);
         DoubleToControl (pPage, L"patternheight", pv->m_iHeight);
         DoubleToControl (pPage, L"numblades", pv->m_dwNumBlades);

         pControl = pPage->ControlFind (L"cluster");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fCluster * 100.0));

         pControl = pPage->ControlFind (L"angleout");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fAngleOut * 100.0));

         pControl = pPage->ControlFind (L"angleskew");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fAngleSkew * 100.0));

         pControl = pPage->ControlFind (L"colorvariation");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fColorVariation * 100.0));

         pControl = pPage->ControlFind (L"anglevariation");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fAngleVariation * 100.0));

         pControl = pPage->ControlFind (L"shorteredge");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fShorterEdge * 100.0));

         pControl = pPage->ControlFind (L"widthtop");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fWidthTop * 100.0));

         pControl = pPage->ControlFind (L"widthbase");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fWidthBase * 100.0));

         pControl = pPage->ControlFind (L"widthmid");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fWidthMid * 100.0));

         pControl = pPage->ControlFind (L"gravity");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fGravity * 100.0));

         pControl = pPage->ControlFind (L"darkedge");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDarkEdge * 100.0));

         pControl = pPage->ControlFind (L"transbase");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTransBase * 100.0));

         pControl = pPage->ControlFind (L"darkenatbottom");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fDarkenAtBottom * 100.0));

         //pControl = pPage->ControlFind (L"tipwidth");
         //if (pControl)
         //   pControl->AttribSetInt (Pos(), (int)(pv->m_fTipWidth * 100.0));

         //pControl = pPage->ControlFind (L"tiplength");
         //if (pControl)
         //   pControl->AttribSetInt (Pos(), (int)(pv->m_fTipLength * 100.0));

         pControl = pPage->ControlFind (L"tipprob");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTipProb * 100.0));

         pControl = pPage->ControlFind (L"tipprobshort");
         if (pControl)
            pControl->AttribSetInt (Pos(), (int)(pv->m_fTipProbShort * 100.0));

         WCHAR szTemp[64];
         DWORD i;
         for (i = 0; i < 3; i++) {
            swprintf (szTemp, L"grasscolor%d", (int)i);
            FillStatusColor (pPage, szTemp, pv->m_acStem[i]);
         }

         //FillStatusColor (pPage, L"tipcolor", pv->m_cTipColor);
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
         if (!_wcsicmp(p->pControl->m_pszName, L"cluster")) {
            pv->m_fCluster = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"colorvariation")) {
            pv->m_fColorVariation = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"angleout")) {
            pv->m_fAngleOut = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"angleskew")) {
            pv->m_fAngleSkew = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"anglevariation")) {
            pv->m_fAngleVariation = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"shorteredge")) {
            pv->m_fShorterEdge = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"widthbase")) {
            pv->m_fWidthBase = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"widthmid")) {
            pv->m_fWidthMid = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"widthtop")) {
            pv->m_fWidthTop = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"gravity")) {
            pv->m_fGravity = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"darkedge")) {
            pv->m_fDarkEdge = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"transbase")) {
            pv->m_fTransBase = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"darkenatbottom")) {
            pv->m_fDarkenAtBottom = fVal;
            return TRUE;
         }
         //else if (!_wcsicmp(p->pControl->m_pszName, L"tipwidth")) {
         //   pv->m_fTipWidth = fVal;
         //   return TRUE;
         //}
         //else if (!_wcsicmp(p->pControl->m_pszName, L"tiplength")) {
         //   pv->m_fTipLength = fVal;
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pControl->m_pszName, L"tipprob")) {
            pv->m_fTipProb = fVal;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"tipprobshort")) {
            pv->m_fTipProbShort = fVal;
            return TRUE;
         }

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         PWSTR pszGrassButton = L"grassbutton";
         DWORD dwGrassButtonLen = (DWORD)wcslen(pszGrassButton);

         if (!_wcsicmp(p->pControl->m_pszName, L"seed")) {
            pv->m_iSeed += GetTickCount();
            pPage->MBSpeakInformation (L"New variation created.");
            return TRUE;
         }
         else if (!wcsncmp(p->pControl->m_pszName, pszGrassButton, dwGrassButtonLen)) {
            DWORD dwNum = _wtoi(p->pControl->m_pszName + dwGrassButtonLen);
            WCHAR szTemp[64];
            swprintf (szTemp, L"grasscolor%d", (int)dwNum);

            pv->m_acStem[dwNum] = AskColor (pPage->m_pWindow->m_hWnd, pv->m_acStem[dwNum],
               pPage, szTemp);
            return TRUE;
         }
         //else if (!_wcsicmp(p->pControl->m_pszName, L"tipbutton")) {
         //   pv->m_cTipColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cTipColor,
         //      pPage, L"tipcolor");
         //   return TRUE;
         //}
         else if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
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
         //else if (!_wcsicmp(p->pControl->m_pszName, L"tipshape")) {
         //   DWORD dwVal;
         //   dwVal = p->pszName ? _wtoi(p->pszName) : 0;
         //   if (dwVal == pv->m_dwTipShape)
         //      break; // unchanged

         //   pv->m_dwTipShape = dwVal;
         //   return TRUE;
         //}
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         pv->m_iWidth = (int) DoubleFromControl (pPage, L"patternwidth");
         pv->m_iWidth = max(pv->m_iWidth, 1);

         pv->m_iHeight = (int) DoubleFromControl (pPage, L"patternheight");
         pv->m_iHeight = max(pv->m_iHeight, 1);

         pv->m_dwNumBlades = (DWORD) DoubleFromControl (pPage, L"numblades");
         pv->m_dwNumBlades = max(pv->m_dwNumBlades, 1);
         pv->m_dwNumBlades = min(pv->m_dwNumBlades, 10000);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Grass tussock";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorGrass::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;
   ti.fDrawFlat = TRUE;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREGRASS, GrassPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }
   if (pszRet && !_wcsicmp(pszRet, L"tip")) {
      BOOL fRet = m_Leaf.Dialog (m_dwRenderShard, pWindow);
      if (!fRet)
         return FALSE;
      goto redo;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorGrass::CTextCreatorGrass (DWORD dwRenderShard, DWORD dwType)
{
   m_dwRenderShard = dwRenderShard;
   m_Material.InitFromID (MATERIAL_FLAT);
   m_dwType = dwType;
   m_iSeed = 0;
   m_iWidth = 512;
   m_iHeight = m_iWidth / 2;
   m_dwNumBlades = 100;
   m_fCluster = 0.5;
   m_fColorVariation = 0.5;
   m_fAngleVariation = 0.5;
   m_fAngleSkew = 0;
   m_fAngleOut = 0.5;
   m_fShorterEdge = 0.5;
   m_acStem[0] = RGB(0, 0x80, 0x20);
   m_acStem[1] = RGB(0x40, 0xa0, 0);
   m_acStem[2] = RGB(0x40, 0xff, 0);
   m_fWidthBase = 0;
   m_fWidthMid = 0.1;
   m_fWidthTop = 0.03;
   m_fGravity = 0.5;
   m_fDarkEdge = 0.5;
   m_fTransBase = 0.5;
   m_fDarkenAtBottom = 0.5;

   //m_dwTipShape = 0;
   //m_cTipColor = RGB(0xff, 0xc0, 0x80);
   //m_fTipWidth = 0.2;
   //m_fTipLength = 0.5;
   m_fTipProb = 1;
   m_fTipProbShort = 0.1;
}

void CTextCreatorGrass::Delete (void)
{
   delete this;
}

static PWSTR gpszNoise = L"Noise";
static PWSTR gpszType = L"Type";
static PWSTR gpszSeed = L"Seed";
static PWSTR gpszAngleVariation = L"AngleVariation";
static PWSTR gpszNumBlades = L"NumBlades";
static PWSTR gpszCluster = L"Cluster";
static PWSTR gpszColorVariation = L"ColorVariation";
static PWSTR gpszAngleOut = L"AngleOut";
static PWSTR gpszAngleSkew = L"AngleSkew";
static PWSTR gpszShorterEdge = L"ShorterEdge";
static PWSTR gpszWidthBase = L"WidthBase";
static PWSTR gpszWidthMid = L"WidthMid";
static PWSTR gpszWidthTop = L"WidthTop";
static PWSTR gpszGravity = L"Gravity";
static PWSTR gpszTransBase = L"TransBase";
static PWSTR gpszTipShape = L"TipShape";
static PWSTR gpszTipColor = L"TipColor";
static PWSTR gpszTipWidth = L"TipWidth";
static PWSTR gpszTipLength = L"TipLength";
static PWSTR gpszTipProb = L"TipProb";
static PWSTR gpszTipProbShort = L"TipProbShort";
static PWSTR gpszDarkenAtBottom = L"DarkenAtBottom";
// static PWSTR gpszDarkEdge = L"DarkEdge";

PCMMLNode2 CTextCreatorGrass::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNoise);

   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszSeed, (int)m_iSeed);
   MMLValueSet (pNode, gpszWidth, m_iWidth);
   MMLValueSet (pNode, gpszHeight, m_iHeight);
   MMLValueSet (pNode, gpszNumBlades, (int)m_dwNumBlades);
   MMLValueSet (pNode, gpszCluster, m_fCluster);
   MMLValueSet (pNode, gpszColorVariation, m_fColorVariation);
   MMLValueSet (pNode, gpszAngleOut, m_fAngleOut);
   MMLValueSet (pNode, gpszAngleSkew, m_fAngleSkew);
   MMLValueSet (pNode, gpszAngleVariation, m_fAngleVariation);
   MMLValueSet (pNode, gpszShorterEdge, m_fShorterEdge);
   MMLValueSet (pNode, gpszWidthBase, m_fWidthBase);
   MMLValueSet (pNode, gpszWidthMid, m_fWidthMid);
   MMLValueSet (pNode, gpszWidthTop, m_fWidthTop);
   MMLValueSet (pNode, gpszGravity, m_fGravity);
   MMLValueSet (pNode, gpszDarkEdge, m_fDarkEdge);
   MMLValueSet (pNode, gpszTransBase, m_fTransBase);
   MMLValueSet (pNode, gpszDarkenAtBottom, m_fDarkenAtBottom);

   //MMLValueSet (pNode, gpszTipShape, (int)m_dwTipShape);
   //MMLValueSet (pNode, gpszTipColor, (int)m_cTipColor);
   //MMLValueSet (pNode, gpszTipWidth, m_fTipWidth);
   //MMLValueSet (pNode, gpszTipLength, m_fTipLength);
   MMLValueSet (pNode, gpszTipProb, m_fTipProb);
   MMLValueSet (pNode, gpszTipProbShort, m_fTipProbShort);

   PCMMLNode2 pSub = m_Leaf.MMLTo();
   if (pSub) {
      pSub->NameSet (gpszLeafForTexture);
      pNode->ContentAdd (pSub);
   }

   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"StemColor%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)m_acStem[i]);
   } // i

   return pNode;
}

BOOL CTextCreatorGrass::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_iSeed = (int) MMLValueGetInt (pNode, gpszSeed, (int)0);
   m_iWidth = MMLValueGetInt (pNode, gpszWidth, 512);
   m_iHeight = MMLValueGetInt (pNode, gpszHeight, 256);
   m_dwNumBlades = (DWORD)MMLValueGetInt (pNode, gpszNumBlades, 1);
   m_fCluster = MMLValueGetDouble (pNode, gpszCluster, 0.5);
   m_fColorVariation = MMLValueGetDouble (pNode, gpszColorVariation, 0.5);
   m_fAngleVariation = MMLValueGetDouble (pNode, gpszAngleVariation, 0.5);
   m_fAngleOut = MMLValueGetDouble (pNode, gpszAngleOut, 0.5);
   m_fAngleSkew = MMLValueGetDouble (pNode, gpszAngleSkew, 0);
   m_fShorterEdge = MMLValueGetDouble (pNode, gpszShorterEdge, 0.5);
   m_fWidthBase = MMLValueGetDouble (pNode, gpszWidthBase, 0);
   m_fWidthMid = MMLValueGetDouble (pNode, gpszWidthMid, 0.1);
   m_fWidthTop = MMLValueGetDouble (pNode, gpszWidthTop, 0.03);
   m_fGravity = MMLValueGetDouble (pNode, gpszGravity, 0.5);
   m_fDarkEdge = MMLValueGetDouble (pNode, gpszDarkEdge, 0.5);
   m_fTransBase = MMLValueGetDouble (pNode, gpszTransBase, 0.5);
   m_fDarkenAtBottom = MMLValueGetDouble (pNode, gpszDarkenAtBottom, 0.5);

   //m_dwTipShape = (DWORD)MMLValueGetInt (pNode, gpszTipShape, 0);
   //m_cTipColor = (COLORREF)MMLValueGetInt (pNode, gpszTipColor, RGB(0xff, 0xc0, 0x80));
   //m_fTipWidth = MMLValueGetDouble (pNode, gpszTipWidth, 0.2);
   //m_fTipLength = MMLValueGetDouble (pNode, gpszTipLength, 0.5);
   m_fTipProb = MMLValueGetDouble (pNode, gpszTipProb, 1);
   m_fTipProbShort = MMLValueGetDouble (pNode, gpszTipProbShort, 0.1);

   PCMMLNode2 pSub = NULL;
   PWSTR psz;
   m_Leaf.Clear();
   pNode->ContentEnum (pNode->ContentFind (gpszLeafForTexture), &psz, &pSub);
   if (pSub)
      m_Leaf.MMLFrom (pSub);

   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < 3; i++) {
      swprintf (szTemp, L"StemColor%d", (int)i);
      m_acStem[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)0);
   } // i

   return TRUE;
}

#define SEGMENTSPERBLADE         10       // # of segments that grass blade divided into

BOOL CTextCreatorGrass::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   srand (m_iSeed);

   // size
   DWORD    dwX, dwY, dwScale;
   TextureDetailApply (m_dwRenderShard, (DWORD)m_iWidth, (DWORD)m_iHeight, &dwX, &dwY);
   dwScale = max(dwX, dwY);
   fp fScaleCounterDetail = (fp)dwX / (fp)max(m_iWidth,1);


   // determine average color
   WORD awOrig[3];
   WORD awAverage[3];
   memset (awAverage, 0, sizeof(awAverage));
   DWORD i, j;
   for (i = 0; i < 3; i++) {
      Gamma (m_acStem[i], awOrig);
      
      for (j = 0; j < 3; j++)
         awAverage[j] += awOrig[j] / 3;
   } // i

   // create the surface
   pImage->Init (dwX, dwY, BACKCOLORRGB, 0);

   // clear to a strange color so can then set that strange color to green, and transparent
   //PIMAGEPIXEL pip = pImage->Pixel(0, 0);
   //for (i = 0; i < dwX * dwY; i++, pip++) {
   //   pip->wRed = 0;
   //   pip->wGreen = 1;
   //   pip->wBlue = 2;
   //}

   // ungamma all the colors
   WORD awStem[3][3], awColorToUse[3];
   for (i = 0; i < 3; i++)
      Gamma (m_acStem[i], awStem[i]);

   // loop over all the blades
   CPoint pStart, pDir;
   m_Leaf.TextureCache(m_dwRenderShard);
   for (i = 0; i < m_dwNumBlades; i++) {
      // pick a color
      fp fColor = (fp)i / (fp)m_dwNumBlades + MyRand(-0.5, 0.5) * m_fColorVariation;
      fColor = max(fColor, 0);
      fColor = min(fColor, 1);

      DWORD dwColor = (DWORD)(fColor * 2);
      DWORD dwColorNext = (dwColor+1)%3;
      DWORD dwWeight = (DWORD)((fColor - (fp)dwColor/2) * 2.0 * 256);
      for (j = 0; j < 3; j++)
         awColorToUse[j] = (WORD) (((256 - dwWeight) * (DWORD)awStem[dwColor][j] +
            dwWeight * (DWORD)awStem[dwColorNext][j]) / 256);

      // random location left/right
      fp fLoc = MyRand (0, 1);
      fp fAngle = fLoc;
      fp fLength = 1.0 - fLoc;
      fLoc = pow (fLoc, (fp) (1.0 + m_fCluster*2.0));
      if (rand()%2) {
         fLoc *= -1; // other side
         fAngle *= -1;
      }
      fLoc *= (fp)dwX/2.0 * (1.0 - m_fCluster) * 0.9;
      fLoc += (fp)dwX/2.0;

      pStart.Zero();
      pStart.p[0] = fLoc;
      pStart.p[1] = dwY;

      // direction
      fAngle *= m_fAngleOut;
      fAngle += MyRand (-1, 1) * m_fAngleVariation + m_fAngleSkew/2.0;
      pDir.Zero();
      pDir.p[0] = sin(fAngle * PI/2);
      pDir.p[1] = -sqrt(1.0 - pDir.p[0] * pDir.p[0]);
      pDir.p[1] *= cos((fp)i / (fp)m_dwNumBlades*PI/2);  // simulate drooping further towards

      // length of the blade
      fLength = pow (fLength, m_fShorterEdge); // shorter towards the edge
      fLength *= cos((fp)i / (fp)m_dwNumBlades*PI/2);  // length affected by how close
      fLength = max(fLength, 0.01);
      DWORD dwSegments = (fLength * SEGMENTSPERBLADE)+1;
      dwSegments = min(SEGMENTSPERBLADE, dwSegments);
      fp fSegmentLength = fLength / (fp)dwSegments * (fp)dwY * 0.8;  // never quite as long as top

      // width at base
      fp fWidthMax = (fp)dwX / 10.0;

      DrawGrassBlade (pImage, UnGamma(awColorToUse), &pStart, &pDir, dwSegments,
         fSegmentLength,
         fWidthMax * max(m_fWidthBase, 0.01),
         fWidthMax * max(m_fWidthMid, 0.01),
         fWidthMax * max(m_fWidthTop, 0.01),
         m_fGravity * 4.0 / (fp)SEGMENTSPERBLADE * ((fp)i / (fp)m_dwNumBlades +1),
         m_fDarkEdge);
            // NOTE: closer grass blades are droopier

      // draw bits at end of grass
      //if (!m_dwTipShape)
      //   continue;

      // probability?
      fp fProb = m_fTipProb * pow (fLength, (fp)(1.0 / max(m_fTipProbShort, 0.01)));
      if (MyRand(0, 1) >= fProb)
         continue;   // not going to draw

      // else, draw tip
      m_Leaf.Render (pImage, 1, fScaleCounterDetail, &pStart, &pDir);
      //fp fTipScale = (fp)dwY / 4;
      //DrawGrassTip (pImage, m_cTipColor, &pStart, &pDir,
      //   m_fTipWidth * fTipScale,
      //   m_fTipLength * fTipScale,
      //   m_dwTipShape);
   } // i
   m_Leaf.TextureRelease();

   // final bits
   memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = 1.0 / (fp) max(dwX, dwY);
   // diable since doing transparent: MassageForCreator (pImage, pMaterial, pTextInfo);

   // darken the grass at the bottom
   ColorDarkenAtBottom (pImage, BACKCOLORRGB, m_fDarkenAtBottom);

   // look for the strange color and revert to gree
   ColorToTransparency (pImage, BACKCOLORRGB, 0, pMaterial);
   pTextInfo->dwMap |= (0x04 | 0x01);  // transparency map and specularity map
   //pip = pImage->Pixel(0, 0);
   //for (i = 0; i < dwX * dwY; i++, pip++)
   //   if ((pip->wRed == 0) && (pip->wGreen == 1) && (pip->wBlue == 2)) {
   //      pip->wRed = awAverage[0];
   //      pip->wGreen = awAverage[1];
   //      pip->wBlue = awAverage[2];
   //      pip->dwIDPart |= 0xff;  // lo byte is transparency
   //      pip->dwID &= 0xffff; // turn off speculatity
   //   }
   //pTextInfo->dwMap |= 0x04;   // transparency map

   // transparency at the base
   PIMAGEPIXEL pip;
   DWORD x, y;
   int iTrans;
   fp fTrans;
   if (m_fTransBase >= 0.01) for (y = 0; y < dwY; y++) {
      fTrans = (fp)(dwY - y) / (fp)dwY;   // so 0 at bottom and 1 at top
      fTrans = 1.0 - fTrans / m_fTransBase * 2.0;
         // The * 2.0 ensures can only be transparent half way up
      if (fTrans <= 0)
         continue;   // not transparent
      iTrans = (int)(fTrans * 256.0);

      pip = pImage->Pixel(0, y);
      for (x = 0; x < dwX; x++, pip++) {
         DWORD dwTrans = pip->dwIDPart & 0xff;
         dwTrans = (dwTrans * (256 - (DWORD)iTrans) + (DWORD)iTrans * 255) / 256;
         pip->dwIDPart = (pip->dwIDPart & ~0xff) | dwTrans;
      } // x
   } // y

   return TRUE;
}



/***********************************************************************************
CTextCreatorGrass::TextureQuery - Adds this object's textures (and it's sub-textures)
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
BOOL CTextCreatorGrass::TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   // NOTE: Not tested

   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;

   
   // add itself
   plText->Add (pagThis);
   pTree->Add (szTemp, NULL, 0);

   // NEW CODE: go through all sub-textures
   m_Leaf.TextureQuery (m_dwRenderShard, plText, pTree);

   return TRUE;
}


/***********************************************************************************
CTextCreatorGrass::SubTextureNoRecurse - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if they recurse
*/
BOOL CTextCreatorGrass::SubTextureNoRecurse (PCListFixed plText, GUID *pagThis)
{
   // NOTE: Not tested

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
   fRet = m_Leaf.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;

done:
   // restore list
   plText->Truncate (dwNum);

   return fRet;
}
