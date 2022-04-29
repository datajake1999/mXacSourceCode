/************************************************************************
CAnimWave.cpp - Object code

begun 18/5/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


// MI - Store attrib info
typedef struct {
   PCAnimAttrib      paa;     // attribute. may be null
   fp                fLast;   // last attrib val
   fp                fCur;    // current attrib val
   BOOL              fWasSet; // set to TRUE if a value for the attribute was set at all, FALSE if has never been set
} MI, *PMI;

/************************************************************************
CAnimWave::Constructor and destructor */
CAnimWave::CAnimWave (void)
{
   m_gClass = CLSID_AnimWave;
   m_iPriority = 40;
   m_iMinDisplayX = 16;
   m_iMinDisplayY = 32;
   m_pWave = NULL;

   m_dwPreferredView = 4;  // prefer FFT
   m_fMixVol = 1;
   m_fLoop = FALSE;
   m_fCutShort = FALSE;
   m_dwLipSync = 2;
   m_szLipSyncOnOff[0] = 0;
   m_fLipSyncScale = 1;
   
}

CAnimWave::~CAnimWave (void)
{
   // free up existing wave
   if (m_pWave) {
      WaveCacheRelease (m_pWave);
      m_pWave = NULL;
   }
}

/************************************************************************
CAnimWave::Delete - See CAnimSocket for info
*/
void CAnimWave::Delete (void)
{
   delete this;
}


/************************************************************************
SetFromMI - Loops through the list of MI structures and uses this to set
all the attributes that have changed since the last time. It also
moves fCur into fLast.

inputs
   PMI         pmi - Pointer to an array of dwNum entries
   DWORD       dwNum - Number of entries
   fp          fTime - Time to set
   fp          fTimeLast - Last time that was set
returns
   none
*/
static void SetFromMI (PMI pmi, DWORD dwNum, fp fTime, fp fTimeLast)
{
   DWORD i;
   TEXTUREPOINT tp;
   for (i = 0; i < dwNum; i++, pmi++) {
      if (!pmi->paa)
         continue;   // empty

      // if same nothing
      if (pmi->fCur == pmi->fLast) {
         pmi->fWasSet = FALSE;   // BUGFIX - Turn off bit since wasn't set last time
         continue;
      }

      // if the last value was never set then do so
      if (!pmi->fWasSet) {
         tp.h = fTimeLast;
         tp.v = pmi->fLast;
         pmi->paa->PointAdd (&tp, 2); // add
            // BUGFIX - Changed from 2 to 1 so would be linear and wouldnt get overshoot
            // BUGFIX - Changed back to 2
      }

      // set the current one
      tp.h = fTime;
      tp.v = pmi->fCur;
      pmi->paa->PointAdd (&tp, 2); // add
            // BUGFIX - Changed from 2 to 1 so would be linear and wouldnt get overshoot
            // BUGFIX - Changed back to 2

      // remember this
      pmi->fLast = pmi->fCur;
      pmi->fWasSet = TRUE;
   } // i

}

/************************************************************************
PhoneCat - Categorize the phoneme based upon the 8 or 6-phoneme category

inputs
   PWSTR       pszPhone - Phoneme
   DWORD       dwNum - Number of phonemes in the category, 8 or 6
returns
   DWORD - Categorty number
*/
static DWORD PhoneCat (PWSTR pszPhone, DWORD dwNum)
{
   // BUGBUG - Will need to get the phone catageory from the phoneme itself

   if (!_wcsicmp(pszPhone, L"m") || !_wcsicmp(pszPhone, L"b") || !_wcsicmp(pszPhone, L"p"))
      return 0; // mbp

   if (!_wcsicmp(pszPhone, L"aa") || !_wcsicmp(pszPhone, L"ay") ||
      !_wcsicmp(pszPhone, L"ao") || !_wcsicmp(pszPhone, L"aw") ||
      !_wcsicmp(pszPhone, L"h") )
      return 1; // ai

   if (!_wcsicmp(pszPhone, L"f") || !_wcsicmp(pszPhone, L"v"))
      return 2; // fv

   if (!_wcsicmp(pszPhone, L"iy"))
      return 3; // e

   if (!_wcsicmp(pszPhone, L"uh") || !_wcsicmp(pszPhone, L"uw"))
      return 4; // fv

   if (dwNum <= 6)
      return 5;   // all the rest

   if (!_wcsicmp(pszPhone, L"ow"))
      return 6; // e

   if (!_wcsicmp(pszPhone, L"l") || !_wcsicmp(pszPhone, L"d") ||
      !_wcsicmp(pszPhone, L"th") || !_wcsicmp(pszPhone, L"dh") )
      return 7; // ai

   return 5;
}

/************************************************************************
CAnimWave::AnimObjSplineApply - See CAnimSocket for info
*/
void CAnimWave::AnimObjSplineApply (PCSceneObj pSpline)
{
   // if doesn't support animation exit here
   if (!m_dwLipSync || !m_pWave || !m_pWave->m_lWVPHONEME.Num())
      return;

   // find the object
   PCSceneObj pSceneObj;
   if (!m_pScene)
      return;
   pSceneObj = m_pScene->ObjectGet (&m_gWorldObject);
   if (!pSceneObj)
      return;

   // get the actual objects
   PCObjectSocket pos;
   pos = NULL;
   if (m_pWorld)
      pos = m_pWorld->ObjectGet(m_pWorld->ObjectFind(&m_gWorldObject));
   fp fValue;
   if (!pos)
      return;

   // get all the attributes that will need
   CListFixed lMI;
   MI mi;
   PMI pmi;
   memset (&mi, 0, sizeof(mi));
   lMI.Init (sizeof(MI));
   DWORD i, dwMode;
   // BUGBUG - Need to use pos-> to determine if supports the attribute because if modify
   // the phoneme names won't detect the changes
   if (m_dwLipSync == 1) {
      dwMode = 0;
      mi.paa = pSceneObj->AnimAttribGet (m_szLipSyncOnOff[0] ? m_szLipSyncOnOff : L"Visible", TRUE);
      lMI.Add (&mi);
   }
   else {   // auto
      if (pos->AttribGet (L"LipSyncPhone8:MBP", &fValue)) {
         dwMode = 1;

         lMI.Required (8);

         // supports 8 phonemes
         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:MBP", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:AI", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:FV", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:E", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:OO", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:Rest", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:O", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone8:LDTH", TRUE);
         lMI.Add (&mi);

      }
      else if (pos->AttribGet (L"LipSyncPhone6:MBP", &fValue)) {
         dwMode = 2;

         lMI.Required (6);

         // supports 6 phonemes
         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone6:MBP", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone6:AI", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone6:FV", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone6:E", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone6:OO", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncPhone6:Rest", TRUE);
         lMI.Add (&mi);
      }
      else if (pos->AttribGet (L"LipSyncPhone:aa", &fValue)) {
         dwMode = 3;
         // supports 40 phonemes
         WCHAR szTemp[64];
         PCWSTR pszSilence = MLexiconEnglishPhoneSilence();
         PLEXENGLISHPHONE ppi;
         lMI.Required (MLexiconEnglishPhoneNum());
         for (i = 0; i < MLexiconEnglishPhoneNum(); i++) {
            ppi = MLexiconEnglishPhoneGet (i);
            if (!_wcsicmp(ppi->szPhoneLong, pszSilence)) {
               // no point getting this phoneme
               mi.paa = NULL;
               lMI.Add (&mi);
               continue;
            }
            wcscpy (szTemp, L"LipSyncPhone:");
            wcscat (szTemp, ppi->szPhoneLong);

            mi.paa = pSceneObj->AnimAttribGet (szTemp, TRUE);
            lMI.Add (&mi);
         } // i
      }
      else if (pos->AttribGet (L"LipSyncMusc:VertOpen", &fValue)) {
         dwMode = 4;

         lMI.Required (7);

         // supports open/close
         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncMusc:VertOpen", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncMusc:LatTension", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncMusc:LatPucker", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncMusc:TeethUpper", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncMusc:TeethLower", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncMusc:TongueForward", TRUE);
         lMI.Add (&mi);

         mi.paa = pSceneObj->AnimAttribGet (L"LipSyncMusc:TongueUp", TRUE);
         lMI.Add (&mi);
      }
      else
         return;  // doesnt support anything
   }
   DWORD dwNum;
   pmi = (PMI) lMI.Get(0);
   dwNum = lMI.Num();

   // loop through all the phonemes
   PWVPHONEME pp;
   fp fStart, fEnd;
   pp = (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(0);
   TimeGet (&fStart, &fEnd);
   fEnd = max(fStart, fEnd);
   if (!m_fLoop && !m_fCutShort)
      fEnd = fStart + (fp)m_pWave->m_dwSamples / (fp)m_pWave->m_dwSamplesPerSec;

   // figure out frames per second
   DWORD dwFPS;
   dwFPS = m_pScene->FPSGet ();
   if (!dwFPS)
      dwFPS = 50; // since not specified
   else {
      dwFPS *= 2; // so that get more accurate lip sync
      dwFPS = max(dwFPS, 20); // need at least this many to get any lip sync
      dwFPS = min(dwFPS, 120);
   }

   // align to frames per second and start there
   fp fCur, fInc, fCurMod;
   int iCur;
   fInc = 1.0 / (fp)dwFPS;
   DWORD dwFind;
   PCWSTR pszSilence = MLexiconEnglishPhoneSilence();
   for (fCur = fStart - myfmod(fStart,fInc); fCur < fEnd; fCur += fInc) {
      // figure out in terms of the wave
      fCurMod = fCur - fStart;
      if (fCurMod < 0)
         continue;   // before the actual wave
      fCurMod *= (fp)m_pWave->m_dwSamplesPerSec;
      iCur = (int) fCurMod;
      iCur = iCur % (int)m_pWave->m_dwSamples;  // so loop around

      // wipe out all exsiting values
      for (i = 0; i < dwNum; i++)
         pmi[i].fCur = 0;

      // get the phoneme info
      PWSTR pszLeft, pszRight;
      WCHAR szLeft[8], szRight[8];
      fp fAlpha, fLatTens, fVertOpen, fTeethTop, fTeethBottom, fTongueForward, fTongueUp;
      if (!m_pWave->PhonemeAtTime ((DWORD)iCur, &pszLeft, &pszRight, &fAlpha,
         &fLatTens, &fVertOpen, &fTeethTop, &fTeethBottom, &fTongueForward, &fTongueUp)) {

            // just write it out and continue
            SetFromMI (pmi, dwNum, fCur, fCur - fInc);
            continue;
         }
      memset (szLeft, 0 ,sizeof(szLeft));
      memset (szRight, 0, sizeof(szRight));
      memcpy (szLeft, pszLeft, 4);
      memcpy (szRight, pszRight, 4);

      switch (dwMode) {
      default:
      case 0:  // one for open and close
         pmi[0].fCur = fVertOpen * m_fLipSyncScale;
         break;
      case 1:  //  8 phonemes
      case 2:  // 6 phonemes
         if (_wcsicmp(szLeft, pszSilence))
            pmi[PhoneCat(szLeft, dwNum)].fCur += (1.0 - fAlpha) * m_fLipSyncScale;
         if (_wcsicmp(szRight, pszSilence))
            pmi[PhoneCat(szRight, dwNum)].fCur += fAlpha * m_fLipSyncScale;
         break;
      case 3:  // all phonemes
         dwFind = MLexiconEnglishPhoneFind (szLeft);
         if (dwFind < dwNum)
            pmi[dwFind].fCur += (1.0 - fAlpha) * m_fLipSyncScale;
         dwFind = MLexiconEnglishPhoneFind (szRight);
         if (dwFind < dwNum)
            pmi[dwFind].fCur += fAlpha * m_fLipSyncScale;
         break;
      case 4:  // muscle
         pmi[0].fCur = fVertOpen;
         pmi[1].fCur = max(fLatTens, 0) * m_fLipSyncScale;
         pmi[2].fCur = max(-fLatTens, 0) * m_fLipSyncScale;
         pmi[3].fCur = fTeethTop * m_fLipSyncScale;
         pmi[4].fCur = fTeethBottom * m_fLipSyncScale;
         pmi[5].fCur = fTongueForward; // note: Dont scale
         pmi[6].fCur = fTongueUp; // note: Dont scale
         break;
      }

      // write out the values
      SetFromMI (pmi, dwNum, fCur, fCur - fInc);
   } // fCur

   // finally, write 0's
   for (i = 0; i < dwNum; i++)
      pmi[i].fCur = 0;
   SetFromMI (pmi, dwNum, fCur, fCur - fInc);
}

/************************************************************************
CAnimWave::Clone - See CAnimSocket for info
*/
CAnimWave *CAnimWave::InternalClone (void)
{
   PCAnimWave pNew = new CAnimWave;
   if (!pNew)
      return NULL;
   CloneTemplate (pNew);

   pNew->m_dwLipSync = m_dwLipSync;
   wcscpy (pNew->m_szLipSyncOnOff, m_szLipSyncOnOff);
   pNew->m_fLipSyncScale = m_fLipSyncScale;

   pNew->m_dwPreferredView = m_dwPreferredView;
   pNew->m_fLoop = m_fLoop;
   pNew->m_fCutShort = m_fCutShort;
   pNew->m_fMixVol = m_fMixVol;
   pNew->m_pWave = m_pWave ? WaveCacheOpen (m_pWave->m_szFile) : NULL;

   return pNew;
}
CAnimSocket *CAnimWave::Clone (void)
{
   return InternalClone();
}

/************************************************************************
CAnimWave::MMLTo - See CAnimSocket for info
*/
static PWSTR gpszPreferredView = L"PreferredView";
static PWSTR gpszFile = L"File";
static PWSTR gpszMixVol = L"MixVol";
static PWSTR gpszLoop = L"Loop";
static PWSTR gpszCutShort = L"CutShort";
static PWSTR gpszLipSync = L"LipSync";
static PWSTR gpszLipSyncOnOff = L"LipSyncOnOff";
static PWSTR gpszLipSyncScale = L"LipSyncScale";

PCMMLNode2 CAnimWave::MMLTo (void)
{
   PCMMLNode2 pNode = MMLToTemplate ();
   if (!pNode)
      return NULL;

   MMLValueSet (pNode, gpszLipSync, (int)m_dwLipSync);
   if (m_szLipSyncOnOff[0])
      MMLValueSet (pNode, gpszLipSyncOnOff, m_szLipSyncOnOff);
   MMLValueSet (pNode, gpszLipSyncScale, m_fLipSyncScale);
   MMLValueSet (pNode, gpszPreferredView, (int)m_dwPreferredView);
   MMLValueSet (pNode, gpszMixVol, m_fMixVol);
   MMLValueSet (pNode, gpszLoop, (int)m_fLoop);
   MMLValueSet (pNode, gpszCutShort, (int)m_fCutShort);
   if (m_pWave && m_pWave->m_szFile) {
      WCHAR szTemp[256];
      MultiByteToWideChar (CP_ACP, 0, m_pWave->m_szFile, -1, szTemp, sizeof(szTemp));
      MMLValueSet (pNode, gpszFile, szTemp);
   }
   return pNode;
}

/************************************************************************
CAnimWave::MMLFrom - See CAnimSocket for info
*/
BOOL CAnimWave::MMLFrom (PCMMLNode2 pNode)
{
   // free up existing wave
   if (m_pWave) {
      WaveCacheRelease (m_pWave);
      m_pWave = NULL;
   }

   if (!MMLFromTemplate (pNode))
      return FALSE;

   PWSTR psz;
   m_dwLipSync = (DWORD) MMLValueGetInt (pNode, gpszLipSync, 2);
   psz = MMLValueGet (pNode, gpszLipSyncOnOff);
   if (psz)
      wcscpy (m_szLipSyncOnOff, psz);
   else
      m_szLipSyncOnOff[0] = 0;
   m_fLipSyncScale = MMLValueGetDouble (pNode, gpszLipSyncScale, 1);
   m_dwPreferredView = (DWORD) MMLValueGetInt (pNode, gpszPreferredView, 0);
   m_fMixVol = MMLValueGetDouble (pNode, gpszMixVol, 1);
   m_fLoop = (BOOL)MMLValueGetInt (pNode, gpszLoop, FALSE);
   m_fCutShort = (BOOL)MMLValueGetInt (pNode, gpszCutShort, FALSE);
   psz = MMLValueGet (pNode, gpszFile);
   if (psz && psz[0])
      m_pWave = WaveCacheOpen (psz);

   return TRUE;
}


/************************************************************************
CAnimWave::Draw - See CAnimSocket for info
*/
void CAnimWave::Draw (HDC hDC, RECT *prHDC, fp fLeft, fp fRight)
{
   // what view
   DWORD dwView = m_dwPreferredView;
   if (!m_pWave)
      dwView = 0;    // no wave, so just show the file name, which will result in nothing
   else {
      // if want wave view but nothing cached then dont bother caching, change display
      if ((dwView >= 3) && !m_pWave->m_psWave)
         dwView = 2; // go down to phonemes

      // if no phonemes go to text
      if ((dwView == 2) && !m_pWave->m_lWVPHONEME.Num())
         dwView = 1;

      // if no text go to file name
      if ((dwView == 1) && !wcslen((PWSTR) m_pWave->m_memSpoken.p))
         dwView = 0;
   }

   // get the time for this object
   fp fBegin;
   TimeGet (&fBegin, NULL);
   fLeft -= fBegin;
   fRight -= fBegin;

   // draw it
   if (dwView <= 1) {   // file name or text
      // clear background
      HBRUSH hBrush = CreateSolidBrush (RGB(0xe0,0xe0,0xff));
      FillRect (hDC, prHDC, hBrush);
      DeleteObject (hBrush);

      // draw the icon
      DrawIcon (hDC, prHDC->left, prHDC->top, LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_SPEAKERICON)));
      
      // new rectangle
      RECT r;
      r = *prHDC;
      r.left += 32+4;  // icon size plus a bit
      if (r.left > r.right)
         return;

      // what text?
      char *psz;
      if (!m_pWave)
         psz = "No wave file. Visit the settings dialog to specify one.";
      else if (dwView == 0)
         psz = m_pWave->m_szFile;
      else {
         gMemTemp.Required (wcslen((PWSTR)m_pWave->m_memSpoken.p)*2+2);
         psz = (char*)gMemTemp.p;
         WideCharToMultiByte (CP_ACP, 0, (PWSTR)m_pWave->m_memSpoken.p, -1,
            psz, (DWORD)gMemTemp.m_dwAllocated, 0, 0);
      }

      // create font
      // draw the text
      HFONT hFont, hFontOld;
      LOGFONT  lf;
      memset (&lf, 0, sizeof(lf));
      lf.lfHeight = -MulDiv(10, GetDeviceCaps(hDC, LOGPIXELSY), 72); 
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
      strcpy (lf.lfFaceName, "Arial");
      hFont = CreateFontIndirect (&lf);
      hFontOld = (HFONT) SelectObject (hDC, hFont);
      SetTextColor (hDC, RGB(0,0,0));
      SetBkMode (hDC, TRANSPARENT);
      DrawText(hDC, psz,
         -1, &r, DT_LEFT | DT_TOP | DT_WORDBREAK);
      SelectObject (hDC, hFontOld);
      DeleteObject (hFont);
   }
   else if (dwView == 2) { // phonemes
      // NOTE: If have loop, NOT doubling up the phonemes for the display because
      // a) it's too much work, and b) it's not really necessary

      m_pWave->PaintPhonemes (hDC, prHDC,
         (int)(fLeft * (fp)m_pWave->m_dwSamplesPerSec),
         (int)(fRight * (fp)m_pWave->m_dwSamplesPerSec), NULL);   // BUGFIX - Just show phoneme
   }
   else if (dwView == 3) { // wave
      // NOTE: If have loop, NOT doubling up the wave for the display because
      // a) it's too much work, and b) it's not really necessary

      m_pWave->PaintWave (hDC, prHDC,
         (int)(fLeft * (fp)m_pWave->m_dwSamplesPerSec),
         (int)(fRight * (fp)m_pWave->m_dwSamplesPerSec),
         32678.0, -32768.0, -1);
   }
   else { // dwView == 4, FFT
      // NOTE: If have loop, NOT doubling up the FFT for the display because
      // a) it's too much work, and b) it's not really necessary

      fp fTop = max(1.0 - 4000.0 / (fp)(m_pWave->m_dwSamplesPerSec/2), 0);

      m_pWave->PaintFFT (hDC, prHDC,
         (int)(fLeft * (fp)m_pWave->m_dwSamplesPerSec),
         (int)(fRight * (fp)m_pWave->m_dwSamplesPerSec),
         fTop, 1, -1, NULL, 5);
   }
}

/************************************************************************
CAnimWave::DialogQuery - See CAnimSocket for info
*/
BOOL CAnimWave::DialogQuery (void)
{
   return TRUE;
}


/****************************************************************************
AnimWavePage
*/
BOOL AnimWavePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCAnimWave pas = (PCAnimWave) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         WCHAR szTemp[256];
         szTemp[0] = 0;
         if (pas->m_pWave)
            MultiByteToWideChar (CP_ACP, 0, pas->m_pWave->m_szFile, -1, szTemp, sizeof(szTemp)/2);
         pControl = pPage->ControlFind (L"file");
         if (pControl)
            pControl->AttribSet (Text(), szTemp);

         ComboBoxSet (pPage, L"prefdisp", pas->m_dwPreferredView);
         DoubleToControl (pPage, L"mixvol", pas->m_fMixVol);
         DoubleToControl (pPage, L"lipscale", pas->m_fLipSyncScale);

         pControl = pPage->ControlFind (L"loop");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pas->m_fLoop);
         pControl = pPage->ControlFind (L"cutshort");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pas->m_fCutShort);

         switch (pas->m_dwLipSync) {
            case 0:  // none
               pControl = pPage->ControlFind (L"lipnone");
               break;
            case 1: // onoff
               pControl = pPage->ControlFind (L"liponoff");
               break;
            case 2:  // phonemes
               pControl = pPage->ControlFind (L"lipauto");
               break;
            default:
               pControl = NULL;
               break;
         }
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // fill in the combo box of attributes
         MemZero (&gMemTemp);
         PCObjectSocket pos;
         pos = NULL;
         if (pas->m_pWorld)
            pos = pas->m_pWorld->ObjectGet(pas->m_pWorld->ObjectFind (&pas->m_gWorldObject));
         CListFixed lAttrib;
         lAttrib.Init (sizeof(ATTRIBVAL));
         if (pos)
            pos->AttribGetAll (&lAttrib);
         DWORD i;
         PATTRIBVAL pav;
         pav = (PATTRIBVAL) lAttrib.Get(0);
         DWORD dwCurSel;
         dwCurSel = -1;
         for (i = 0; i < lAttrib.Num(); i++, pav++) {
            MemCat (&gMemTemp, L"<elem name=\"");
            MemCatSanitize (&gMemTemp, pav->szName);
            MemCat (&gMemTemp, L"\">");
            MemCatSanitize (&gMemTemp, pav->szName);
            MemCat (&gMemTemp, L"</elem>");

            if (!_wcsicmp(pas->m_szLipSyncOnOff, pav->szName))
               dwCurSel = i;
         } // i
         if ((dwCurSel == -1) && lAttrib.Num()) {
            // nothing, so pick first one
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);

            wcscpy (pas->m_szLipSyncOnOff, ((PATTRIBVAL)lAttrib.Get(0))->szName);
            dwCurSel = 0;

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
         }
         ESCMCOMBOBOXADD cba;
         memset (&cba, 0, sizeof(cba));
         cba.dwInsertBefore = 0; // BUGFIX - Don't use -1 because reverses order
         cba.pszMML = (PWSTR)gMemTemp.p;
         pControl = pPage->ControlFind (L"onoffcombo");
         if (pControl)
            pControl->Message (ESCM_COMBOBOXADD, &cba);
         if (pControl && (dwCurSel != -1))
            pControl->AttribSetInt (CurSel(), (int)dwCurSel);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"onoffcombo")) {
            if (!p->pszName)
               return TRUE;   // shouldnt happen
            if (!_wcsicmp(p->pszName, pas->m_szLipSyncOnOff))
               return TRUE;   // same

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            wcscpy (pas->m_szLipSyncOnOff, p->pszName);
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"prefdisp")) {
            if (dwVal == pas->m_dwPreferredView)
               return TRUE;   // same

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            // load in the audio while at it
            if (pas->m_pWave)
               pas->m_pWave->RequireWave (NULL);
            pas->m_dwPreferredView = dwVal;

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+83:   // load in a new copy of the wave
      {
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"file");
         if (!pControl)
            return TRUE;

         if (pas->m_pScene && pas->m_pScene->SceneSetGet())
            pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);

         // free up existing wave
         if (pas->m_pWave) {
            WaveCacheRelease (pas->m_pWave);
            pas->m_pWave = NULL;
         }

         WCHAR szTemp[256];
         DWORD dwNeeded;
         pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
         
         // cache new one
         pas->m_pWave = WaveCacheOpen (szTemp);

         // load in the audio while at it
         if (pas->m_pWave)
            pas->m_pWave->RequireWave (NULL);

         if (pas->m_pScene && pas->m_pScene->SceneSetGet())
            pas->m_pScene->SceneSetGet()->ObjectChanged (pas);

         // update the length if have a wave
         if (pas->m_pWave) {
            // update the duration
            fp fStart, fEnd;
            pas->TimeGet (&fStart, &fEnd);
            fEnd = fStart + (fp)pas->m_pWave->m_dwSamples / (fp)pas->m_pWave->m_dwSamplesPerSec;
            pas->TimeSet (fStart, fEnd);
         }
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"mixvol")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_fMixVol = DoubleFromControl (pPage, psz);
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"lipscale")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_fLipSyncScale = DoubleFromControl (pPage, psz);
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"file")) {
            pPage->Message (ESCM_USER+83);

            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"lipnone")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_dwLipSync = 0;
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"liponoff")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_dwLipSync = 1;
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"lipauto")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_dwLipSync = 2;
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"edit")) {
            if (!pas->m_pWave || !pas->m_pWave->m_szFile[0])
               pPage->MBInformation (L"Creating a new wave file.",
                  L"Because you haven't typed in a name, this will create a new file for you.");

            // create the filename
            char szRun[512];
            char szCurDir[256];
            DWORD dwLen;
            strcpy (szRun, "\"");
            dwLen = (DWORD) strlen(szRun);
            strcpy (szCurDir, gszAppDir);
            strcat (szRun, gszAppDir);
            strcat (szRun, "m3dwave.exe");
#ifdef _DEBUG
            strcpy (szCurDir, "z:\\bin");
            strcpy (szRun + dwLen, "z:\\m3d\\m3dwave\\debug\\m3dwave.exe");
#endif
            strcat (szRun, "\"");
            if (pas->m_pWave) {
               strcat (szRun, " ");
               strcat (szRun, pas->m_pWave->m_szFile);
            }

            PROCESS_INFORMATION pi;
            STARTUPINFO si;
            memset (&si, 0, sizeof(si));
            si.cb = sizeof(si);
            memset (&pi, 0, sizeof(pi));
            if (!CreateProcess (NULL, szRun, NULL, NULL, NULL, NULL, NULL, szCurDir, &si, &pi))
               return TRUE;

            // Close process and thread handles. 
            CloseHandle( pi.hProcess );
            CloseHandle( pi.hThread );

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"refresh")) {
            char szTemp[256];
            if (!pas->m_pWave) {
               pPage->MBWarning (L"You don't have a wave file to refresh.");
               return TRUE;
            }
            strcpy (szTemp, pas->m_pWave->m_szFile);

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);

            // this will update all cached items
            pas->m_pWave->Open (NULL, szTemp, TRUE);

            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);

            // update the length if have a wave
            if (pas->m_pWave) {
               // update the duration
               fp fStart, fEnd;
               pas->TimeGet (&fStart, &fEnd);
               fEnd = fStart + (fp)pas->m_pWave->m_dwSamples / (fp)pas->m_pWave->m_dwSamplesPerSec;
               pas->TimeSet (fStart, fEnd);
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"loop")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_fLoop = p->pControl->AttribGetBOOL(Checked());
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"cutshort")) {
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectAboutToChange (pas);
            pas->m_fCutShort = p->pControl->AttribGetBOOL(Checked());
            if (pas->m_pScene && pas->m_pScene->SceneSetGet())
               pas->m_pScene->SceneSetGet()->ObjectChanged (pas);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"browse")) {
            WCHAR szw[256];
            szw[0] = 0;
            if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, szw))
               return TRUE;

            // use this
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribSet (Text(), szw);
            pPage->Message (ESCM_USER+83);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Audio object settings";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/************************************************************************
CAnimWave::DialogShow - See CAnimSocket for info
*/
BOOL CAnimWave::DialogShow (PCEscWindow pWindow)
{
   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLANIMWAVE, AnimWavePage, this);

   if (!pszRet)
      return FALSE;

   return !_wcsicmp(pszRet, Back());
}


/************************************************************************
CAnimWave::Deconstruct - See CAnimSocket for details
*/
BOOL CAnimWave::Deconstruct (BOOL fAct)
{
   // get the object this is in
   if (!m_pScene)
      return FALSE;
   PCSceneObj pSceneObj;
   pSceneObj = m_pScene->ObjectGet (&m_gWorldObject);
   if (!pSceneObj)
      return FALSE;

   // if there isn't any lip sync then false
   if (!m_dwLipSync)
      return FALSE;

   if (!fAct)
      return TRUE;   // yes can deconstruct

   // do it...
   // just duplicate this wave and turn off display phonemes
   PCAnimWave pNew;
   pNew = InternalClone ();
   if (!pNew)
      return FALSE;
   pNew->m_dwLipSync = 0;
   pSceneObj->ObjectAdd (pNew);


   // finally, call into template
   return CAnimTemplate::Deconstruct (fAct);
}

/************************************************************************
CAnimWave::WaveFormatGet - See standard CAnimSocket
*/
BOOL CAnimWave::WaveFormatGet (DWORD *pdwSamplesPerSec, DWORD *pdwChannels)
{
   if (!m_pWave)
      return FALSE;

   *pdwSamplesPerSec = m_pWave->m_dwSamplesPerSec;
   *pdwChannels = m_pWave->m_dwChannels;
   return TRUE;
}

/************************************************************************
CAnimWave::WaveGet - See standard CAnimSocket
*/
BOOL CAnimWave::WaveGet (DWORD dwSamplesPerSec, DWORD dwChannels, int iTimeSec, int iTimeFrac,
                        DWORD dwSamples, short *pasSamp, fp *pfVolume)
{
   if (!m_pWave)
      return FALSE;

   // else calc
   DWORD dwMaxSamples;
   fp fStart, fEnd, fLen;
   TimeGet (&fStart, &fEnd);
   fEnd = max(fStart, fEnd);  // just incase
   fLen = (fEnd - fStart) * (fp)dwSamplesPerSec;

   // how many samples when do conversion
   dwMaxSamples = (DWORD) ((__int64)m_pWave->m_dwSamples * (__int64)dwSamplesPerSec /
      (__int64)m_pWave->m_dwSamplesPerSec);

   if (m_fLoop)
      dwMaxSamples = (DWORD) fLen;
   else if (m_fCutShort)
      dwMaxSamples = min(dwMaxSamples, (DWORD)fLen);
   *pfVolume = m_fMixVol;

   // offset
   int iFloor, iSamp;
   iFloor = floor(fStart);
   iSamp = (int)((fStart - (fp)iFloor) * (fp)dwSamplesPerSec);
   return m_pWave->WaveBufferGet (dwSamplesPerSec, dwChannels,
      iTimeSec - iFloor, iTimeFrac - iSamp,
      m_fLoop, dwMaxSamples, dwSamples, pasSamp);
}
