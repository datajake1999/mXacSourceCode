/********************************************************************************
CTextCreatorText.cpp - Code for handling faces.

begun 13/1/06 by Mike Rozak
Copyright 2006 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include <crtdbg.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"
#include "texture.h"




CTextEffectText::CTextEffectText (void)
{
   m_Material.InitFromID (MATERIAL_FLAT);
   m_fTurnOn = FALSE;

   MemZero (&m_memText);
   MemCat (&m_memText, L"Text goes here");
   m_pPosn.p[0] = m_pPosn.p[1] = 0;
   m_pPosn.p[2] = m_pPosn.p[3] = 1;
   m_cColor = RGB(0,0,0);
   m_iLRAlign = m_iTBAlign = 0;
   memset (&m_lf, 0, sizeof(m_lf));
   m_lf.lfHeight = 24;
   m_lf.lfCharSet = ANSI_CHARSET;
   m_lf.lfWeight = FW_REGULAR;
   m_lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
   strcpy (m_lf.lfFaceName, "Arial");

   m_cGlow = 0;
   m_fTextRaiseAdd = TRUE;
   m_fColorOpacity = 1;
   m_fGlowOpacity = 1;
   m_fGlowScale = 0;
   m_fTextRaise = 0;
}

static PWSTR gpszText = L"Text";
static PWSTR gpszTurnOn = L"TurnOn";
static PWSTR gpszPosn = L"Posn";
static PWSTR gpszColor = L"Color";
static PWSTR gpszMessage = L"Message";
static PWSTR gpszLRAlign = L"LRAlign";
static PWSTR gpszTBAlign = L"TBAlign";
static PWSTR gpszFontHeight = L"FontHeight";
static PWSTR gpszFontCharSet = L"FontCharSet";
static PWSTR gpszFontItalic = L"FontItalic";
static PWSTR gpszFontStrikeOut = L"FontStrikeOut";
static PWSTR gpszFontUnderline = L"FontUnderline";
static PWSTR gpszFontWeight = L"FontWeight";
static PWSTR gpszFontFace = L"FontFace";
static PWSTR gpszFontPitchAndFamily = L"FontPitchAndFamily";
static PWSTR gpszGlow = L"Glow";
static PWSTR gpszGlowScale = L"GlowScale";
static PWSTR gpszGlowOpacity = L"GlowOpacity";
static PWSTR gpszColorOpacity = L"ColorOpacity";
static PWSTR gpszTextRaise = L"TextRaise";
static PWSTR gpszTextRaiseAdd = L"TextRaiseAdd";

PCMMLNode2 CTextEffectText::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszText);
   MMLValueSet (pNode, gpszTurnOn, (int) m_fTurnOn);
   m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszPosn, &m_pPosn);
   MMLValueSet (pNode, gpszColor, (int) m_cColor);
   if (m_iLRAlign)
      MMLValueSet (pNode, gpszLRAlign, (int) m_iLRAlign);
   if (m_iTBAlign)
      MMLValueSet (pNode, gpszTBAlign, (int) m_iTBAlign);

   MMLValueSet (pNode, gpszFontHeight, (int) m_lf.lfHeight);
   if (m_lf.lfCharSet != ANSI_CHARSET)
      MMLValueSet (pNode, gpszFontCharSet, (int) m_lf.lfCharSet);
   if (m_lf.lfItalic)
      MMLValueSet (pNode, gpszFontItalic, (int) m_lf.lfItalic);
   if (m_lf.lfStrikeOut)
      MMLValueSet (pNode, gpszFontStrikeOut, (int) m_lf.lfStrikeOut);
   if (m_lf.lfUnderline)
      MMLValueSet (pNode, gpszFontUnderline, (int) m_lf.lfUnderline);
   if (m_lf.lfWeight != FW_REGULAR)
      MMLValueSet (pNode, gpszFontWeight, (int) m_lf.lfWeight);
   MMLValueSet (pNode, gpszFontPitchAndFamily, (int) m_lf.lfPitchAndFamily);

   WCHAR szFace[LF_FACESIZE];
   MultiByteToWideChar (CP_ACP, 0, m_lf.lfFaceName, -1, szFace, sizeof(szFace)/sizeof(WCHAR));
   if (szFace[0])
      MMLValueSet (pNode, gpszFontFace, szFace);

   PWSTR psz = (PWSTR)m_memText.p;
   if (psz[0])
      MMLValueSet (pNode, gpszMessage, psz);

   MMLValueSet (pNode, gpszGlow, (int)m_cGlow);
   MMLValueSet (pNode, gpszTextRaiseAdd, (int)m_fTextRaiseAdd);
   MMLValueSet (pNode, gpszGlowOpacity, m_fGlowOpacity);
   MMLValueSet (pNode, gpszColorOpacity, m_fColorOpacity);
   MMLValueSet (pNode, gpszGlowScale, m_fGlowScale);
   MMLValueSet (pNode, gpszTextRaise, m_fTextRaise);

   return pNode;
}

BOOL CTextEffectText::MMLFrom (PCMMLNode2 pNode)
{
   m_Material.MMLFrom (pNode);
   m_fTurnOn = (BOOL)MMLValueGetInt (pNode, gpszTurnOn, (int) FALSE);

   MemZero (&m_memText);
   m_pPosn.p[0] = m_pPosn.p[1] = 0;
   m_pPosn.p[2] = m_pPosn.p[3] = 1;
   MMLValueGetPoint (pNode, gpszPosn, &m_pPosn);

   m_cColor = MMLValueGetInt (pNode, gpszColor, (int) RGB(0,0,0));
   m_iLRAlign = MMLValueGetInt (pNode, gpszLRAlign, (int) 0);
   m_iTBAlign = MMLValueGetInt (pNode, gpszTBAlign, (int) 0);

   m_lf.lfHeight = MMLValueGetInt (pNode, gpszFontHeight, (int) 24);
   m_lf.lfCharSet = MMLValueGetInt (pNode, gpszFontCharSet, (int) ANSI_CHARSET);
   m_lf.lfItalic = MMLValueGetInt (pNode, gpszFontItalic, (int) 0);
   m_lf.lfStrikeOut = MMLValueGetInt (pNode, gpszFontStrikeOut, (int) 0);
   m_lf.lfUnderline = MMLValueGetInt (pNode, gpszFontUnderline, (int) 0);
   m_lf.lfWeight = MMLValueGetInt (pNode, gpszFontWeight, (int) FW_REGULAR);
   m_lf.lfPitchAndFamily = MMLValueGetInt (pNode, gpszFontPitchAndFamily, (int) FF_SWISS | VARIABLE_PITCH);

   PWSTR psz = MMLValueGet (pNode, gpszFontFace);
   if (psz)
      WideCharToMultiByte (CP_ACP, 0, psz, -1, m_lf.lfFaceName, sizeof(m_lf.lfFaceName), 0, 0);

   psz = MMLValueGet (pNode, gpszMessage);
   if (psz)
      MemCat (&m_memText, psz);

   m_cGlow = (int)MMLValueGetInt (pNode, gpszGlow, 0);
   m_fTextRaiseAdd = (BOOL) MMLValueGetInt (pNode, gpszTextRaiseAdd, TRUE);
   m_fGlowOpacity = MMLValueGetDouble (pNode, gpszGlowOpacity, 1);
   m_fColorOpacity = MMLValueGetDouble (pNode, gpszColorOpacity, 1);
   m_fGlowScale = MMLValueGetDouble (pNode, gpszGlowScale, 0);
   m_fTextRaise = MMLValueGetDouble (pNode, gpszTextRaise, 0);

   return TRUE;
}


/*************************************************************************************
CTextEffectText::DrawTextToImage - This draws antialiased text to an image.

inputs
   PWSTR       pszText - Text to draw
   PCImage     pImage - To draw to
   RECT        *prBound - Bounding area for the text
   int         iLRAlign - -1 = left align, 0 = center, 1 = right align
   int         iTBAlign - -1 = top align, 0 = center, 1 = bottom align
   PLOGFONT    plf - Font to use
   COLORREF    cColor - Color to use
   HWND        hWnd - Window to get DC from
   fp          fPixelLen - Pixel length
returns
   BOOL - TRUE if success
*/
#define TEXTANTI     2        // amount to antialias text on image

BOOL CTextEffectText::DrawTextToImage (PWSTR pszText, PCImage pImage,
                                       PCImage pImage2, fp *pfGlowScale,
                                       RECT *prBound, int iLRAlign, int iTBAlign,
                                       PLOGFONT plf, COLORREF cColor, HWND hWnd, fp fPixelLen)
{
   // color
   WORD awColor[3];
   GammaInit ();
   Gamma (cColor, awColor);

   // convert the text
   CMem memAnsi;
   if (!memAnsi.Required ((wcslen(pszText)+1)*sizeof(WCHAR)))
      return FALSE;
   PSTR psz = (PSTR)memAnsi.p;
   WideCharToMultiByte (CP_ACP, 0, pszText, -1, psz, (DWORD)memAnsi.m_dwAllocated, 0, 0);

   // width?
   if ((prBound->right < prBound->left) || (prBound->bottom < prBound->top))
      return TRUE;   // cant draw there
   DWORD dwWidth = (DWORD)(prBound->right - prBound->left) * TEXTANTI;
   DWORD dwHeight = (DWORD)(prBound->bottom - prBound->top) * TEXTANTI;

   // glow
   fp fOldGlowScale = *pfGlowScale;
   fp fNewGlowScale = max(fOldGlowScale, fOldGlowScale * (1.0 - m_fGlowOpacity) + m_fGlowScale /** m_fGlowOpacity*/);
   fp fGlowScaleInv = fNewGlowScale ? (1.0 / fNewGlowScale) : 0;
   *pfGlowScale = fNewGlowScale;
   WORD awGlow[3];
   Gamma (m_cGlow, awGlow);
   fp afGlow[3];
   DWORD i;
   for (i = 0; i < 3; i++)
      afGlow[i] = (fp)awGlow[i] * m_fGlowScale * fGlowScaleInv;
   fp afGlowUnderneathScaleText = fOldGlowScale * (1.0 - m_fGlowOpacity) * fGlowScaleInv;
   fp afGlowUnderneathScaleNoText = fOldGlowScale * fGlowScaleInv;
   fp fBumpScale = m_fTextRaise / fPixelLen;
   BOOL fChangedGlow = (fNewGlowScale != fOldGlowScale);

   // create compatible DC
   CImage ImageText;
   HDC hDC = GetDC (hWnd);
   if (!hDC)
      return FALSE;
   HDC hDCDraw = CreateCompatibleDC (hDC);
   HBITMAP hBit = CreateCompatibleBitmap (hDC, dwWidth, dwHeight);
   ReleaseDC (hWnd, hDC);
   if (!hDCDraw || !hBit) {
      if (hDCDraw)
         DeleteDC (hDCDraw);
      if (hBit)
         DeleteObject (hBit);
      return FALSE;
   }
   SelectObject (hDCDraw, hBit);

   // create the font
   plf->lfHeight *= TEXTANTI;
   plf->lfWidth *= TEXTANTI;
   HFONT hFont = CreateFontIndirect (plf);
   plf->lfHeight /= TEXTANTI;
   plf->lfWidth /= TEXTANTI;
   if (!hFont) {
      DeleteDC (hDCDraw);
      DeleteObject (hBit);
      return FALSE;
   }

   // wipe background and draw font
   RECT r;
   DWORD dwFlags = DT_WORDBREAK;
   if (iLRAlign < 0)
      dwFlags |= DT_LEFT;
   else if (iLRAlign > 0)
      dwFlags |= DT_RIGHT;
   else
      dwFlags |= DT_CENTER;
   if (iTBAlign < 0)
      dwFlags |= DT_TOP;
   else if (iTBAlign > 0)
      dwFlags |= DT_BOTTOM;
   else
      dwFlags |= DT_VCENTER;

   r.top = r.left = 0;
   r.right = (int)dwWidth;
   r.bottom = (int)dwHeight;
   FillRect (hDCDraw, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
   SetTextColor (hDCDraw, RGB(0xff,0xff,0xff));
   SetBkColor (hDCDraw, RGB(0,0,0));
   SelectObject (hDCDraw, hFont);
   DrawText (hDCDraw, psz, -1, &r, DT_WORDBREAK | DT_LEFT | DT_TOP | DT_CALCRECT);
   int iOffX = 0, iOffY = 0;
   if (iLRAlign > 0)
      iOffX = (int)dwWidth - r.right;
   else if (iLRAlign == 0)
      iOffX = ((int)dwWidth - r.right)/2;
   if (iTBAlign > 0)
      iOffY = (int)dwHeight - r.bottom;
   else if (iTBAlign == 0)
      iOffY = ((int)dwHeight - r.bottom)/2;
   r.left += iOffX;
   r.right += iOffX;
   r.top += iOffY;
   r.bottom += iOffY;
   DrawText (hDCDraw, psz, -1, &r, dwFlags);

   DeleteDC (hDCDraw);
   DeleteObject (hFont);
   if (!ImageText.Init (hBit)) {
      DeleteObject (hBit);
      return FALSE;
   }
   DeleteObject (hBit);

   // transfer these pixels over
   int x, y;
   RECT rTemp;
   DWORD dwImageWidth = pImage->Width();
   DWORD dwImageHeight = pImage->Height();
   rTemp = *prBound;
   rTemp.top = max(rTemp.top, r.top/TEXTANTI - 1 + prBound->top);
   rTemp.bottom = min(rTemp.bottom, r.bottom/TEXTANTI + 1 + prBound->top);
   rTemp.top = max(rTemp.top, 0);
   rTemp.bottom = min(rTemp.bottom, (int)dwImageHeight);
   rTemp.left = max(rTemp.left, r.left/TEXTANTI - 1 + prBound->left);
   rTemp.right = min(rTemp.right, r.right/TEXTANTI + 1 + prBound->right);
   for (y = rTemp.top; y < rTemp.bottom; y++) {
      int iYOrig = (y - prBound->top) * TEXTANTI;
      // Below is not needed because included in rTemp
      //if ((iYOrig + TEXTANTI-1 < r.top) || (iYOrig >= r.bottom))
      //   continue;   // not bounded

      for (x = rTemp.left; x < rTemp.right; x++) {
         // get the pixels from the original
         DWORD dwSum = 0;
         int iXOrig = (x - prBound->left) * TEXTANTI;
         // Below is not needed because included in rTemp
         //if ((iXOrig + TEXTANTI-1 < r.left) || (iXOrig >= r.right))
         //   continue;   // not bounded

         int xx, yy;
         for (yy = 0; yy < TEXTANTI; yy++) {
            PIMAGEPIXEL pipText = ImageText.Pixel ((DWORD)iXOrig, (DWORD)iYOrig + yy);
            for (xx = 0; xx < TEXTANTI; xx++, pipText++)
               dwSum += (DWORD)pipText->wRed;
         } // yy
         BOOL fExit = FALSE;
         if (!dwSum)
            fExit = TRUE;   // nothing there

         xx = x;
         if ((xx < 0) || (xx >= (int)dwImageWidth)) {
            // BUGFIX - dont do wrap around
            fExit = TRUE;
            //if (!pImage->m_f360)
            //   continue;   // out of bounds

            //while (xx < 0)
            //   xx += (int)dwImageWidth;
            //while (xx >= (int)dwImageWidth)
            //   xx -= (int)dwImageWidth;
         } // if out of range

         if (fExit) {
            // if changed glow scale then must adapt
            if (fChangedGlow) {
               PIMAGEPIXEL pipTo2 = pImage2->Pixel ((DWORD)xx,(DWORD)y);
               for (i = 0; i < 3; i++) {
                  fp f = (fp)((&pipTo2->wRed)[i]);
                  f = f * afGlowUnderneathScaleNoText;
                  f = max(f, 0);
                  f = min(f, (fp)0xffff);
                  (&pipTo2->wRed)[i] = (WORD)f;
               } // i
            }
            continue;
         }

         dwSum /= (TEXTANTI * TEXTANTI);
         DWORD dwSumInv = (DWORD)0xffff - dwSum;
         fp fSum = (fp)dwSum / (fp)0xffff;
         fp fSumInv = 1.0 - fSum;

         // color
         DWORD dwColor = (DWORD) ((fSum * m_fColorOpacity) * (fp)0xffff);
         DWORD dwColorInv = (DWORD)0xffff - dwColor;
         PIMAGEPIXEL pipTo = pImage->Pixel ((DWORD)xx,(DWORD)y);
         pipTo->wRed = (WORD)(((DWORD)pipTo->wRed * dwColorInv + (DWORD)awColor[0] * dwColor) >> 16);
         pipTo->wGreen = (WORD)(((DWORD)pipTo->wGreen * dwColorInv + (DWORD)awColor[1] * dwColor) >> 16);
         pipTo->wBlue = (WORD)(((DWORD)pipTo->wBlue * dwColorInv + (DWORD)awColor[2] * dwColor) >> 16);

         // specularity and transparency
         if (dwSum) {
            WORD wReflect = HIWORD(pipTo->dwID);
            WORD wExponent = LOWORD(pipTo->dwID);
            wReflect = (WORD)(((DWORD)wReflect * dwSumInv + (DWORD)m_Material.m_wSpecReflect * dwSum) >> 16);
            wExponent = (WORD)(((DWORD)wExponent * dwSumInv + (DWORD)m_Material.m_wSpecExponent * dwSum) >> 16);
            pipTo->dwID = MAKELONG(wExponent, wReflect);

            BYTE bTrans = LOBYTE(pipTo->dwIDPart);
            bTrans = (BYTE) (((DWORD)bTrans * dwSumInv + (DWORD)(m_Material.m_wTransparency >> 8) * dwSum) >> 16);
            pipTo->dwIDPart = (pipTo->dwIDPart & ~0xff) | bTrans;
         }

         // glow
         if (fNewGlowScale) {
            PIMAGEPIXEL pipTo2 = pImage2->Pixel ((DWORD)xx,(DWORD)y);
            for (i = 0; i < 3; i++) {
               fp f = (fp)((&pipTo2->wRed)[i]);
               f = f * afGlowUnderneathScaleText * fSum + f * afGlowUnderneathScaleNoText * fSumInv +
                  fSum * afGlow[i];
               f = max(f, 0);
               f = min(f, (fp)0xffff);
               (&pipTo2->wRed)[i] = (WORD)f;
            } // i
         } // if glow

         // height
         if (dwSum) {
            if (m_fTextRaiseAdd)
               pipTo->fZ += fBumpScale * fSum;
            else
               pipTo->fZ = pipTo->fZ * fSumInv + fBumpScale * fSum;
         }
      } // x
   } // y

   return TRUE;
}

void CTextEffectText::Apply (PCImage pImage, PCImage pImage2, fp *pfGlowScale, fp fPixelLen)
{
   if (!m_fTurnOn)
      return;

   HWND hWnd = GetDesktopWindow ();
   RECT rBound;
   LOGFONT lf;
   rBound.left = (fp)pImage->Width() * m_pPosn.p[0];
   rBound.top = (fp)pImage->Height() * m_pPosn.p[1];
   rBound.right = (fp)pImage->Width() * m_pPosn.p[2];
   rBound.bottom = (fp)pImage->Height() * m_pPosn.p[3];
   lf = m_lf;
   lf.lfHeight = lf.lfHeight * (int)pImage->Height() / 256;
   lf.lfWidth = lf.lfWidth * (int)pImage->Height() / 256;
   DrawTextToImage ((PWSTR)m_memText.p, pImage, pImage2, pfGlowScale,
      &rBound, m_iLRAlign, m_iTBAlign, &lf, m_cColor, hWnd, fPixelLen);

}

BOOL EffectTextPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextEffectText pv = (PCTextEffectText) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pv->m_fTurnOn);

         // set the material
         ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         pControl = pPage->ControlFind (L"editmaterial");
         if (pControl)
            pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);


         if (pControl = pPage->ControlFind (L"titletext"))
            pControl->AttribSet (Text(), (PWSTR)pv->m_memText.p);

         // combo
         ComboBoxSet (pPage, L"titlelr", (DWORD)(pv->m_iLRAlign + 1));
         ComboBoxSet (pPage, L"titletb", (DWORD)(pv->m_iTBAlign + 1));

         // title posn
         DWORD j;
         WCHAR szTemp[64];
         for (j = 0; j < 4; j++) {
            swprintf (szTemp, L"titleposn%d", (int)j);
            DoubleToControl (pPage, szTemp, pv->m_pPosn.p[j] * 100.0);
         }

         if (pControl = pPage->ControlFind (L"textraiseadd"))
            pControl->AttribSetBOOL (Checked(), pv->m_fTextRaiseAdd);
         if (pControl = pPage->ControlFind (L"coloropacity"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fColorOpacity * 100));
         if (pControl = pPage->ControlFind (L"glowopacity"))
            pControl->AttribSetInt (Pos(), (int)(pv->m_fGlowOpacity * 100));
         DoubleToControl (pPage, L"glowscale", pv->m_fGlowScale);
         MeasureToString (pPage, L"textraise", pv->m_fTextRaise, TRUE);

         FillStatusColor (pPage, L"colorcolor", pv->m_cColor);
         FillStatusColor (pPage, L"glowcolor", pv->m_cGlow);
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

         if (!_wcsicmp(p->pControl->m_pszName, L"colorbutton")) {
            pv->m_cColor = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cColor, pPage, L"colorcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"glowbutton")) {
            pv->m_cGlow = AskColor (pPage->m_pWindow->m_hWnd, pv->m_cGlow, pPage, L"glowcolor");
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"titlefont")) {
            CHOOSEFONT cf;
            memset (&cf, 0, sizeof(cf));

            cf.lStructSize = sizeof(cf);
            cf.hwndOwner = pPage->m_pWindow->m_hWnd;
            cf.lpLogFont = &pv->m_lf;
            cf.Flags = /*CF_EFFECTS |*/ CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
            cf.rgbColors = 0; // pv->m_cColor;

            ChooseFont (&cf);

            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

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
         else if (!_wcsicmp(psz, L"titlelr")) {
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            iVal -= 1;
            if (iVal == pv->m_iLRAlign)
               return TRUE;

            pv->m_iLRAlign = iVal;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"titletb")) {
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            iVal -= 1;
            if (iVal == pv->m_iTBAlign)
               return TRUE;

            pv->m_iTBAlign = iVal;

            return TRUE;
         }
      }
      break;

   case ESCM_USER+186:  // get all the control values
      {
         // set the material
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"turnon");
         if (pControl)
            pv->m_fTurnOn = pControl->AttribGetBOOL (Checked());

         // title posn
         DWORD j;
         WCHAR szTemp[10000];
         for (j = 0; j < 4; j++) {
            swprintf (szTemp, L"titleposn%d", (int)j);
            pv->m_pPosn.p[j] = DoubleFromControl (pPage, szTemp) / 100.0;
         }

         DWORD dwNeeded;
         pControl = pPage->ControlFind (L"titletext");
         if (pControl) {
            szTemp[0] = 0;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            MemZero (&pv->m_memText);
            MemCat (&pv->m_memText, szTemp);
         }

         if (pControl = pPage->ControlFind (L"textraiseadd"))
            pv->m_fTextRaiseAdd = pControl->AttribGetBOOL (Checked());
         if (pControl = pPage->ControlFind (L"coloropacity"))
            pv->m_fColorOpacity = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         if (pControl = pPage->ControlFind (L"glowopacity"))
            pv->m_fGlowOpacity = (fp) pControl->AttribGetInt (Pos()) / 100.0;
         pv->m_fGlowScale = DoubleFromControl (pPage, L"glowscale");
         MeasureParseString (pPage, L"textraise", &pv->m_fTextRaise);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Text to display";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}

BOOL CTextEffectText::Dialog (PCEscWindow pWindow, PCTextCreatorSocket pCreator)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = pCreator;
   ti.pThis = this;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTUREEFFECTTEXT, EffectTextPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}







/****************************************************************************
TextPage
*/
BOOL TextPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTEXTEDITINFO pt = (PTEXTEDITINFO) pPage->m_pUserData;
   PCTextCreatorText pv = (PCTextCreatorText) pt->pThis;

   if (TEHelperMessageHook (pPage, dwMessage, pParam))
      return TRUE;

   // call sub-textures for trap
   if (pv->m_Texture.PageTrapMessages (pv->m_dwRenderShard, L"TextureA", pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         MeasureToString (pPage, L"pixellen", pv->m_fPixelLen, TRUE);

         // set the material
         //PCEscControl pControl;
         //ComboBoxSet (pPage, L"material", pv->m_Material.m_dwID);
         //pControl = pPage->ControlFind (L"editmaterial");
         //if (pControl)
         //   pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         MeasureToString (pPage, L"patternwidth", pv->m_fPatternWidth, TRUE);
         MeasureToString (pPage, L"patternheight", pv->m_fPatternHeight, TRUE);

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         //if (!_wcsicmp(p->pControl->m_pszName, L"editmaterial")) {
         //   if (!pv->m_Material.Dialog (pPage->m_pWindow->m_hWnd))
         //      pPage->Exit (L"[close]");
         //   return TRUE;
         //}
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;

         // only care about transparency
         //if (!_wcsicmp(p->pControl->m_pszName, L"material")) {
         //   DWORD dwVal;
         //   dwVal = p->pszName ? _wtoi(p->pszName) : 0;
         //   if (dwVal == pv->m_Material.m_dwID)
         //      break; // unchanged
         //   if (dwVal)
         //      pv->m_Material.InitFromID (dwVal);
         //   else
         //      pv->m_Material.m_dwID = MATERIAL_CUSTOM;

         //   // eanble/disable button to edit
         //   PCEscControl pControl;
         //   pControl = pPage->ControlFind (L"editmaterial");
         //   if (pControl)
         //      pControl->Enable (pv->m_Material.m_dwID ? FALSE : TRUE);

         //   return TRUE;
         //}
         //else
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
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Text";
            return TRUE;
         }
      }
      break;
   };

   return DefPage (pPage, dwMessage, pParam);
}


BOOL CTextCreatorText::Dialog (PCEscWindow pWindow)
{
   TEXTEDITINFO ti;
   PWSTR pszRet;
   memset (&ti, 0, sizeof(ti));
   ti.hBit = NULL;
   ti.pCreator = this;
   ti.pThis = this;

back:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTEXTURETEXT, TextPage, &ti);
   if (ti.hBit) {
      DeleteObject (ti.hBit);
      ti.hBit = NULL;
   }

   PWSTR pszText = L"Text";
   DWORD dwLen = (DWORD)wcslen(pszText);
   if (pszRet && !wcsncmp(pszRet, pszText, dwLen)) {
      DWORD dwVal = _wtoi(pszRet + dwLen);
      dwVal = min(NUMTEXTLINES-1, dwVal);
      if (m_aText[dwVal].Dialog (pWindow, this))
         goto back;
      else
         return FALSE;
   }

   return pszRet && !_wcsicmp(pszRet, Back());
}

CTextCreatorText::CTextCreatorText (DWORD dwRenderShard, DWORD dwType)
{
   //m_Material.InitFromID (MATERIAL_FLAT);
   m_dwRenderShard = dwRenderShard;
   m_dwType = dwType;
   m_fPixelLen = m_fPatternWidth = m_fPatternHeight = 0;

   m_fPixelLen = .01;
   m_fPatternWidth = 1;
   m_fPatternHeight = 1;

   m_Texture.StretchToFitSet (TRUE); // so that doesn't reapply texture stretch
   m_aText[0].m_fTurnOn = TRUE;
}

void CTextCreatorText::Delete (void)
{
   delete this;
}

static PWSTR gpszPatternWidth = L"PatternWidth";
static PWSTR gpszPatternHeight = L"PatternHeight";
// static PWSTR gpszText = L"Text";
static PWSTR gpszType = L"Type";
static PWSTR gpszPixelLen = L"PixelLen";
static PWSTR gpszTextA = L"TextA";

PCMMLNode2 CTextCreatorText::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszText);

   //m_Material.MMLTo(pNode);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   MMLValueSet (pNode, gpszPixelLen, m_fPixelLen);
   MMLValueSet (pNode, gpszPatternWidth, m_fPatternWidth);
   MMLValueSet (pNode, gpszPatternHeight, m_fPatternHeight);

   PCMMLNode2 pSub;
   pSub = m_Texture.MMLTo ();
   if (pSub) {
      pSub->NameSet (gpszTextA);
      pNode->ContentAdd (pSub);
   }

   WCHAR szTemp[64];
   DWORD i;
   for (i = 0; i < NUMTEXTLINES; i++) {
      PCMMLNode2 pSub = m_aText[i].MMLTo();
      if (!pSub)
         continue;
      swprintf (szTemp, L"Text%d", (int) i);
      pSub->NameSet (szTemp);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CTextCreatorText::MMLFrom (PCMMLNode2 pNode)
{
   m_Texture.Clear();

   //m_Material.MMLFrom (pNode);

   m_dwType = (DWORD) MMLValueGetInt (pNode, gpszType, (int)0);
   m_fPixelLen = MMLValueGetDouble (pNode, gpszPixelLen, 0);
   m_fPatternWidth = MMLValueGetDouble (pNode, gpszPatternWidth, 0);
   m_fPatternHeight = MMLValueGetDouble (pNode, gpszPatternHeight, 0);

   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszTextA)) {
         m_Texture.MMLFrom (pSub);
         continue;
      }
   } // i

   WCHAR szTemp[64];
   DWORD dwFind;
   for (i = 0; i < NUMTEXTLINES; i++) {
      swprintf (szTemp, L"Text%d", (int)i);
      dwFind = pNode->ContentFind (szTemp);
      if (dwFind == -1)
         continue;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (!pSub)
         continue;
      m_aText[i].MMLFrom (pSub);
   }

   return TRUE;
}

BOOL CTextCreatorText::Render (PCImage pImage, PCImage pImage2, PCMaterial pMaterial, PTEXTINFO pTextInfo)
{
   // size
   DWORD    dwX, dwY, dwScale;
   fp fPixelLen = TextureDetailApply (m_dwRenderShard, m_fPixelLen);
   dwX = (DWORD) ((m_fPatternWidth + fPixelLen/2) / fPixelLen);
   dwY = (DWORD) ((m_fPatternHeight + fPixelLen/2) / fPixelLen);
   dwX = max(dwX,1);
   dwY = max(dwY,1);
   dwScale = max(dwX, dwY);

   //memcpy (pMaterial, &m_Material, sizeof(m_Material));
   memcpy (pMaterial, m_Texture.MaterialGet(0), sizeof(*pMaterial));
   memset (pTextInfo, 0, sizeof(TEXTINFO));

   m_Texture.ScaleSet (m_fPatternWidth, m_fPatternHeight);

   // initially fill the texture with what's in A
   fp fGlowScaleA;
   m_Texture.FillTexture (m_dwRenderShard, m_fPatternWidth, m_fPatternHeight, fPixelLen, TRUE,
      pImage, pImage2, pMaterial, &fGlowScaleA);
   pTextInfo->fGlowScale = fGlowScaleA;

   // average out Z
   DWORD i;
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   DWORD dwNum = pImage->Width() * pImage->Height();
   double fSum = 0;
   for (i = 0; i < dwNum; i++, pip++)
      fSum += pip->fZ;
   fSum /= (double)dwNum;
   pip = pImage->Pixel(0,0);
   for (i = 0; i < dwNum; i++, pip++)
      pip->fZ -= fSum;

   // draw text
   for (i = 0; i < NUMTEXTLINES; i++) {
      fp fTemp = pTextInfo->fGlowScale;
      m_aText[i].Apply (pImage, pImage2, &fTemp, fPixelLen);
      pTextInfo->fGlowScale = fTemp;
   }


   // moved earlier memcpy (pMaterial, &m_Material, sizeof(m_Material));
   // moved earlier memset (pTextInfo, 0, sizeof(TEXTINFO));
   pTextInfo->fFloor = FALSE;
   pTextInfo->fPixelLen = fPixelLen;
   MassageForCreator (pImage, pMaterial, pTextInfo);

   // glow
   if (pTextInfo->fGlowScale)
      pTextInfo->dwMap |= 0x08;


   return TRUE;
}




/***********************************************************************************
CTextCreatorText::TextureQuery - Adds this object's textures (and it's sub-textures)
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
BOOL CTextCreatorText::TextureQuery (PCListFixed plText, PCBTree pTree, GUID *pagThis)
{
   WCHAR szTemp[sizeof(GUID)*4+2];
   MMLBinaryToString ((PBYTE)pagThis, sizeof(GUID)*2, szTemp);
   if (pTree->Find (szTemp))
      return TRUE;

   
   // add itself
   plText->Add (pagThis);
   pTree->Add (szTemp, NULL, 0);

   // NEW CODE: go through all sub-textures
   m_Texture.TextureQuery (m_dwRenderShard, plText, pTree);

   return TRUE;
}


/***********************************************************************************
CTextCreatorText::SubTextureNoRecurse - Adds this object's textures (and it's sub-textures)
if it's not already on the list.

inputs
   PCListFixed       plText - List of 2-GUIDs (major & minor) for the textures
                     that are already known. If not, the texture is TEMPORARILY added, and sub-textures
                     are also TEMPORARILY added.
   GUID              *pagThis - Pointer to an array of 2 guids. pagThis[0] = code, pagThis[1] = sub
returns
   BOOL - TRUE if success. FALSE if they recurse
*/
BOOL CTextCreatorText::SubTextureNoRecurse (PCListFixed plText, GUID *pagThis)
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
   fRet = m_Texture.SubTextureNoRecurse (m_dwRenderShard, plText);
   if (!fRet)
      goto done;

done:
   // restore list
   plText->Truncate (dwNum);

   return fRet;
}

// BUGBUG - at some point may wish to have blurring of text so can get more subtle z-changes
// and also do flouro effects
