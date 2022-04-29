/*************************************************************************************
CTTSTransPros - Object used to record transplanted prosody.

begun 22/10/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "m3dwave.h"

/*************************************************************************************
CTTSTransPros::Constructor and destuctor
*/
CTTSTransPros::CTTSTransPros (void)
{
   // m_szTTS[0] = 0;
   // m_szTTSWork[0] = 0;
   //m_szVoiceFile[0] = 0;
   m_pWave = m_pWaveTP = NULL;
   m_dwQualityWaveTP = -1;
   m_fAvgPitch  =0;
   m_dwQuality = 2;
   m_dwPitchType = 1;   // relative
   m_dwVolType = 0;  // default to none because sometimes causes problems
   m_dwDurType = 2;  // absolute
   m_fPitchAdjust = m_fPitchExpress = 1.0;
   m_fVolAdjust = m_fDurAdjust = 1.0;
   m_fKeepRec = m_fKeepSeg = m_fModOrig = FALSE;

   MemZero (&m_memPreModText);

   m_pSR = NULL;
   m_pTTS = NULL;


   //HKEY  hKey = NULL;
   //DWORD dwDisp;
   //RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
   //   KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   //char sza[256];
   //sza[0] = 0;
   //if (hKey) {
   //   DWORD dw, dwType;
   //   LONG lRet;
   //   dw = sizeof(sza);
   //   lRet = RegQueryValueEx (hKey, gszKeyTrainFile, NULL, &dwType, (LPBYTE) sza, &dw);
   //   RegCloseKey (hKey);

   //   if (lRet != ERROR_SUCCESS)
   //      sza[0] = 0;

   //   MultiByteToWideChar (CP_ACP, 0, sza, -1, m_szVoiceFile, sizeof(m_szVoiceFile)/sizeof(WCHAR));
   //}
   //if (!m_szVoiceFile[0]) {
   //   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, m_szVoiceFile, sizeof(m_szVoiceFile)/sizeof(WCHAR));
   //   wcscat (m_szVoiceFile, L"EnglishMale.mtf");
   //}

   // get the defaults tts
   //RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
   //   KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   //sza[0] = 0;
   //if (hKey) {
   //   DWORD dw, dwType;
   //   LONG lRet;
   //   dw = sizeof(sza);
   //   lRet = RegQueryValueEx (hKey, gszKeyTTSFile, NULL, &dwType, (LPBYTE) sza, &dw);
   //   RegCloseKey (hKey);

   //   if (lRet != ERROR_SUCCESS)
   //      sza[0] = 0;
   //}
   //MultiByteToWideChar (CP_ACP, 0, sza, -1, m_szTTS, sizeof(m_szTTS)/sizeof(WCHAR));
}

CTTSTransPros::~CTTSTransPros (void)
{
   if (m_pSR)
      delete m_pSR;
   if (m_pTTS)
      TTSCacheClose (m_pTTS);
}

static PWSTR gpszTransPros = L"TransPros";
static PWSTR gpszTTS = L"TTS";
static PWSTR gpszTTSWork = L"TTSWork";
static PWSTR gpszVoiceFile = L"VoiceFile";
static PWSTR gpszAvgPitch = L"AvgPitch";
static PWSTR gpszQuality = L"Quality";
static PWSTR gpszPitchType = L"PitchType";
static PWSTR gpszVolType = L"VolType";
static PWSTR gpszDurType = L"DurType";
static PWSTR gpszPitchAdjust = L"PitchAdjust";
static PWSTR gpszPitchExpress = L"PitchExpress";
static PWSTR gpszVolAdjust = L"VolAdjust";
static PWSTR gpszDurAdjust = L"DurAdjust";

/*************************************************************************************
CTTSTransPros::MMLTo - Standard call
*/
PCMMLNode2 CTTSTransPros::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTransPros);

   //if (m_szTTS[0])
   //   MMLValueSet (pNode, gpszTTS, m_szTTS);
   //if (m_szTTSWork[0])
   //   MMLValueSet (pNode, gpszTTSWork, m_szTTSWork);
   //if (m_szVoiceFile[0])
   //   MMLValueSet (pNode, gpszVoiceFile, m_szVoiceFile);

   MMLValueSet (pNode, gpszAvgPitch, m_fAvgPitch);
   MMLValueSet (pNode, gpszQuality, (int) m_dwQuality);
   MMLValueSet (pNode, gpszPitchType, (int) m_dwPitchType);
   MMLValueSet (pNode, gpszVolType, (int) m_dwVolType);
   MMLValueSet (pNode, gpszDurType, (int) m_dwDurType);
   MMLValueSet (pNode, gpszPitchAdjust, m_fPitchAdjust);
   MMLValueSet (pNode, gpszPitchExpress, m_fPitchExpress);
   MMLValueSet (pNode, gpszVolAdjust, m_fVolAdjust);
   MMLValueSet (pNode, gpszDurAdjust, m_fDurAdjust);

   return pNode;
}

/*************************************************************************************
CTTSTransPros::MMLFrom - Standard call
*/
BOOL CTTSTransPros::MMLFrom (PCMMLNode2 pNode)
{
   //PWSTR psz;
   //m_szTTS[0] = 0;
   //m_szTTSWork[0] = 0;
   //m_szVoiceFile[0] = 0;
   //psz = MMLValueGet (pNode, gpszTTS);
   //if (psz)
   //   wcscpy (m_szTTS, psz);
   //psz = MMLValueGet (pNode, gpszTTSWork);
   //if (psz)
   //   wcscpy (m_szTTSWork, psz);
   //psz = MMLValueGet (pNode, gpszVoiceFile);
   //if (psz)
   //   wcscpy (m_szVoiceFile, psz);

   m_fAvgPitch = MMLValueGetDouble (pNode, gpszAvgPitch, 0);
   m_dwQuality = (DWORD) MMLValueGetInt (pNode, gpszQuality, (int) 0);
   m_dwPitchType = (DWORD) MMLValueGetInt (pNode, gpszPitchType, (int) 0);
   m_dwVolType = (DWORD) MMLValueGetInt (pNode, gpszVolType, (int) 0);
   m_dwDurType = (DWORD) MMLValueGetInt (pNode, gpszDurType, (int) 0);
   m_fPitchAdjust = MMLValueGetDouble(pNode, gpszPitchAdjust, 1);
   m_fPitchExpress = MMLValueGetDouble (pNode, gpszPitchExpress, 1);
   m_fVolAdjust = MMLValueGetDouble (pNode, gpszVolAdjust, 1);
   m_fDurAdjust = MMLValueGetDouble (pNode, gpszDurAdjust, 1);

   return TRUE;
}

/*************************************************************************************
CTTSTransPros::Clone - Standard call
*/
CTTSTransPros *CTTSTransPros::Clone (void)
{
   PCTTSTransPros pNew = new CTTSTransPros;
   if (!pNew)
      return NULL;

   //wcscpy (pNew->m_szTTS, m_szTTS);
   //wcscpy (pNew->m_szTTSWork, m_szTTSWork);
   //wcscpy (pNew->m_szVoiceFile, m_szVoiceFile);
   pNew->m_fAvgPitch = m_fAvgPitch;
   pNew->m_dwQuality = m_dwQuality;
   pNew->m_dwPitchType = m_dwPitchType;
   pNew->m_dwVolType = m_dwVolType;
   pNew->m_dwDurType = m_dwDurType;
   pNew->m_fPitchAdjust = m_fPitchAdjust;
   pNew->m_fPitchExpress = m_fPitchExpress;
   pNew->m_fVolAdjust = m_fVolAdjust;
   pNew->m_fDurAdjust = m_fDurAdjust;

   return pNew;
}



/*********************************************************************************
TransPros1Page */

static BOOL TransPros1Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCTTSTransPros ptp = (PCTTSTransPros)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set the text
         PCEscControl pControl = pPage->ControlFind (L"tospeak");
         if (pControl && ptp->m_pWave->m_memSpoken.p)
            pControl->AttribSet (Text(), (PWSTR) ptp->m_pWave->m_memSpoken.p);

         // buttons
         pControl = pPage->ControlFind (L"keeprec");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), ptp->m_fKeepRec);
         pControl = pPage->ControlFind (L"keepseg");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), ptp->m_fKeepSeg);

         // other edit
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"keeprec")) {
            ptp->m_fKeepRec =p->pControl->AttribGetBOOL (Checked());
            break;
         }
         else if (!_wcsicmp(psz, L"keepseg")) {
            ptp->m_fKeepSeg =p->pControl->AttribGetBOOL (Checked());
            break;
         }
         else if (!_wcsicmp(psz, L"newsrfile")) {
            if (VoiceFileDefaultUI (pPage->m_pWindow->m_hWnd))
               pPage->Message (ESCM_USER+82);
            //if (!VoiceFileOpenDialog (pPage->m_pWindow->m_hWnd, ptp->m_szVoiceFile,
            //   sizeof(ptp->m_szVoiceFile)/sizeof(WCHAR), FALSE))
            //   return TRUE;

            //pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newttsfile")) {
            if (TTSCacheDefaultUI (pPage->m_pWindow->m_hWnd))
               pPage->Message (ESCM_USER+82);
            //if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, ptp->m_szTTS,
            //   sizeof(ptp->m_szTTS)/sizeof(WCHAR), FALSE))
            //   return TRUE;

            //pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"next")) {
            // make sure have typed something in
            PWSTR psz = (PWSTR) ptp->m_pWave->m_memSpoken.p;
            if (!psz || !psz[0]) {
               pPage->MBWarning (L"You must type in the text you wish to have spoken.");
               return TRUE;
            }

            // load in sr
            WCHAR szVoice[256];
            VoiceFileDefaultGet (szVoice, sizeof(szVoice)/sizeof(WCHAR));
            if (!szVoice[0]) {
               pPage->MBWarning (L"You must select a speech recognition file to use.");
               return TRUE;
            }
            if (ptp->m_pSR)
               delete ptp->m_pSR;
            ptp->m_pSR = new CVoiceFile;
            if (!ptp->m_pSR)
               return TRUE;
            if (!ptp->m_pSR->Open ()) {
               pPage->MBWarning (L"The voice file can't be open.",
                  L"Please select another one.");
               return TRUE;
            }


            // load in tts
            WCHAR szTTS[256];
            TTSCacheDefaultGet (szTTS, sizeof(szTTS)/sizeof(WCHAR));
            if (!szTTS[0]) {
               pPage->MBWarning (L"You must select a text-to-speech file to use.");
               return TRUE;
            }
            if (ptp->m_pTTS)
               TTSCacheClose (ptp->m_pTTS);
            ptp->m_pTTS = TTSCacheOpen (NULL, TRUE, FALSE);
            if (!ptp->m_pTTS) {
               pPage->MBWarning (L"The text-to-speech file can't be open.",
                  L"Please select another one.");
               return TRUE;
            }

            // fall through
            break;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"tospeak")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!ptp->m_pWave->m_memSpoken.Required (dwNeed+2))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) ptp->m_pWave->m_memSpoken.p,
               (DWORD)ptp->m_pWave->m_memSpoken.m_dwAllocated, &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+82:   // set tts and sr strings
      {
         VoiceFileDefaultGet (pPage, L"srfile");
         //PCEscControl pControl;

         //pControl = pPage->ControlFind (L"srfile");
         //if (pControl)
         //   pControl->AttribSet (Text(), ptp->m_szVoiceFile[0] ? ptp->m_szVoiceFile : L"No training selected yet");

         TTSCacheDefaultGet (pPage, L"ttsfile");
         //pControl = pPage->ControlFind (L"ttsfile");
         //if (pControl)
         //   pControl->AttribSet (Text(), ptp->m_szTTS[0] ? ptp->m_szTTS : L"No TTS voice selected yet");
      }
      return TRUE;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Transplanted prosody";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFRECORDING")) {
            p->pszSubString = ptp->m_fModOrig ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFRECORDING")) {
            p->pszSubString = ptp->m_fModOrig ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSEG")) {
            p->pszSubString = ((ptp->m_pWave->m_lWVWORD.Num()>1) && (ptp->m_pWave->m_lWVPHONEME.Num()>1)) ?
               L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSEG")) {
            p->pszSubString = ((ptp->m_pWave->m_lWVWORD.Num()>1) && (ptp->m_pWave->m_lWVPHONEME.Num()>1)) ?
               L"" : L"</comment>";
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"WILDCARDWARNING")) {
            MemZero (&gMemTemp);

            PWSTR pszText = (PWSTR)ptp->m_pWave->m_memSpoken.p;

            if (pszText && wcschr (pszText, L'%'))
               MemCat (&gMemTemp,
                  L"<p><blockquote><font color=#c00000>"
                  L"Warning: Transplanted prosody text <bold>cannot include %1, %2</bold>, etc. You should"
                  L"modify the above text and replace them with the most common substitutions,"
                  L"or press \"Back\" to cancel out of this transplanted prosody."
                  L"</font></blockquote></p>");

            if (pszText && wcschr (pszText, L'/') && wcschr (pszText, L'(') && wcschr (pszText, L')'))
               MemCat (&gMemTemp,
                  L"<p><blockquote><font color=#c00000>"
                  L"Warning: Transplanted prosody text <bold>cannot include alternate verb forms,"
                  L"such as (am/are/is).</bold> You should"
                  L"modify the above text and replace them with the most common substitutions,"
                  L"or press \"Back\" to cancel out of this transplanted prosody."
                  L"</font></blockquote></p>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }

   return FALSE;  // no def page: DefPage (pPage, dwMessage, pParam);
}




/*********************************************************************************
TransPros2Page */

static BOOL TransPros2Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCTTSTransPros ptp = (PCTTSTransPros)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // calculate the pitch
         if (ptp->m_fAvgPitch < CLOSE)
            ptp->m_fAvgPitch = ptp->m_pWave->PitchOverRange (PITCH_F0, 0, ptp->m_pWave->m_dwSamples, 0, NULL, NULL, NULL, FALSE);
         ptp->m_fAvgPitch = max(ptp->m_fAvgPitch, 1);
         DoubleToControl (pPage, L"avgpitch", ptp->m_fAvgPitch);

         // set dropdowns
         ComboBoxSet (pPage, L"pitchrel", ptp->m_dwPitchType);
         ComboBoxSet (pPage, L"volrel", ptp->m_dwVolType);
         ComboBoxSet (pPage, L"durrel", ptp->m_dwDurType);

         // pitch adjust
         DoubleToControl (pPage, L"pitchadjust", ptp->m_fPitchAdjust);
         DoubleToControl (pPage, L"pitchexpressadjust", ptp->m_fPitchExpress);
         DoubleToControl (pPage, L"voladjust", ptp->m_fVolAdjust);
         DoubleToControl (pPage, L"duradjust", ptp->m_fDurAdjust);

         // reset the quality text
         pPage->Message (ESCM_USER+85, NULL);

         // set the edit control based on the type
         PCEscControl pControl;
         PWSTR psz;
         switch (ptp->m_dwQuality) {
         case 0:
         default:
            psz = L"synth";
            break;
         case 1:
            psz = L"word";
            break;
         case 2:
            psz = L"phone1";
            break;
         case 3:
            psz = L"phone2";
            break;
         case 4:
            psz = L"phone3";
            break;
         }
         pControl = pPage->ControlFind (psz);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // set text
         pPage->Message (ESCM_USER+84);
      }
      break;

   case ESCM_USER+84:   // set the text
      {
         PWSTR pszText = (PWSTR)ptp->m_aMemQuality[ptp->m_dwQuality].p;
         PCEscControl pControl = pPage->ControlFind (L"text");
         if (pControl)
            pControl->AttribSet (Text(), pszText);

         // mark current wave invalid
         ptp->m_dwQualityWaveTP = -1;

         // stop playing if playing the synthesized
         ptp->m_pWaveTP->QuickPlayStop();

      }
      return TRUE;

   case ESCM_USER+85:   // reset the quality text
      {
         // get the text

         DWORD i;
         DWORD *pdw = (DWORD*)pParam;
         PWSTR pszText = (PWSTR)ptp->m_pWave->m_memSpoken.p;
         for (i = (pdw ? pdw[0] : 0); i < (pdw ? (pdw[0]+1) : NUMTPQUAL); i++) {
            MemZero (&ptp->m_aMemQuality[i]);

            switch (i) {
            case 0: // pure synthesizes
               if (pszText)
                  MemCatSanitize (&ptp->m_aMemQuality[i], pszText);
               break;
            case 1:  // per-word
               ptp->WaveToPerWordTP (pszText, ptp->m_pWave, NULL, &ptp->m_aMemQuality[i]);
               break;
            case 2:  // 1 per phone
            case 3:  // 2 per phone
            case 4:  // 3 per phone
               ptp->WaveToPerPhoneTP (pszText, ptp->m_pWave, i - 1, ptp->m_pTTS->Lexicon(), NULL, &ptp->m_aMemQuality[i]);
               break;
            }

         } // i

      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"pitchrel")) {
            if (dwVal == ptp->m_dwPitchType)
               return TRUE;   // no change
            ptp->m_dwPitchType = dwVal;
            pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
            pPage->Message (ESCM_USER+84);   // reset displayed text
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"volrel")) {
            if (dwVal == ptp->m_dwVolType)
               return TRUE;   // no change
            ptp->m_dwVolType = dwVal;
            pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
            pPage->Message (ESCM_USER+84);   // reset displayed text
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"durrel")) {
            if (dwVal == ptp->m_dwDurType)
               return TRUE;   // no change
            ptp->m_dwDurType = dwVal;
            pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
            pPage->Message (ESCM_USER+84);   // reset displayed text
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"avgpitch")) {
            fp f = DoubleFromControl (pPage, p->pControl->m_pszName);
            if (f < CLOSE)
               return TRUE;
            ptp->m_fAvgPitch = f;
            if (ptp->m_dwPitchType == 1) {
               pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
               pPage->Message (ESCM_USER+84);   // reset displayed text
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"pitchadjust")) {
            fp f = DoubleFromControl (pPage, p->pControl->m_pszName);
            if (f < CLOSE)
               return TRUE;
            ptp->m_fPitchAdjust = f;
            if (ptp->m_dwPitchType) {
               pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
               pPage->Message (ESCM_USER+84);   // reset displayed text
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"pitchexpressadjust")) {
            fp f = DoubleFromControl (pPage, p->pControl->m_pszName);
            ptp->m_fPitchExpress = f;
            if (ptp->m_dwPitchType) {
               pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
               pPage->Message (ESCM_USER+84);   // reset displayed text
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"voladjust")) {
            fp f = DoubleFromControl (pPage, p->pControl->m_pszName);
            if (f < CLOSE)
               return TRUE;
            ptp->m_fVolAdjust = f;
            if (ptp->m_dwVolType) {
               pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
               pPage->Message (ESCM_USER+84);   // reset displayed text
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"duradjust")) {
            fp f = DoubleFromControl (pPage, p->pControl->m_pszName);
            if (f < CLOSE)
               return TRUE;
            ptp->m_fDurAdjust = f;
            if (ptp->m_dwDurType) {
               pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
               pPage->Message (ESCM_USER+84);   // reset displayed text
            }
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"text")) {
            // stop playing
            ptp->m_pWaveTP->QuickPlayStop();
            ptp->m_dwQualityWaveTP = -1;

            DWORD dwNeed = 0;
            PCMem pMem = &ptp->m_aMemQuality[ptp->m_dwQuality];
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pMem->Required (dwNeed+2))
               return TRUE;
            p->pControl->AttribGet (Text(), (PWSTR) pMem->p,
               (DWORD)pMem->m_dwAllocated, &dwNeed);
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

         if (!_wcsicmp(psz, L"synth")) {
            ptp->m_dwQuality = 0;
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"word")) {
            ptp->m_dwQuality = 1;
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"phone1")) {
            ptp->m_dwQuality = 2;
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"phone2")) {
            ptp->m_dwQuality = 3;
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"phone3")) {
            ptp->m_dwQuality = 4;
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"restore")) {
            pPage->Message (ESCM_USER+85, &ptp->m_dwQuality);
            pPage->Message (ESCM_USER+84);
            pPage->MBInformation (L"Transplanted prosody restored to its original.");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"recalcpitch")) {
            ptp->m_fAvgPitch = ptp->m_pWave->PitchOverRange (PITCH_F0, 0, ptp->m_pWave->m_dwSamples, 0, NULL, NULL, NULL, FALSE);
            ptp->m_fAvgPitch = max(ptp->m_fAvgPitch, 1);
            DoubleToControl (pPage, L"avgpitch", ptp->m_fAvgPitch);

            if ((ptp->m_dwPitchType == 1) && ptp->m_dwQuality) {
               pPage->Message (ESCM_USER+85);   // recalculate all the tp settings
               pPage->Message (ESCM_USER+84);   // reset displayed text
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"playorig")) {
            ptp->m_pWave->QuickPlayStop();
            ptp->m_pWaveTP->QuickPlayStop();
            ptp->m_pWave->QuickPlay();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"playtp")) {
            ptp->m_pWave->QuickPlayStop();
            ptp->m_pWaveTP->QuickPlayStop();

            // if current synth is not the same as what looking for then synthesize
            BOOL fRet = TRUE;
            if (ptp->m_dwQuality != ptp->m_dwQualityWaveTP) {
               ptp->m_pWaveTP->BlankWaveToSize (0, TRUE);

               // text
               PWSTR pszText = (PWSTR)ptp->m_aMemQuality[ptp->m_dwQuality].p;

               // tts it
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Synthesizing...");
               fRet = ptp->m_pTTS->SynthGenWave (ptp->m_pWaveTP, ptp->m_pWaveTP->m_dwSamplesPerSec, pszText, TRUE, 1 /*iTTSQuality*/, FALSE /* fDisablePCM */, &Progress);

               ptp->m_dwQualityWaveTP = ptp->m_dwQuality;
            }

            // play it
            if (fRet)
               ptp->m_pWaveTP->QuickPlay();
            else
               pPage->MBWarning (L"Text-to-speech couldn't speak the text.",
                  L"You may have modified the transplanted prosody text and accidentally broken it.");
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Transplanted prosody review";
            return TRUE;
         }
      }
      break;

   }

   return FALSE;  // no DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CTTSTransPros::Dialog - Pulls up the dialog for tranplanted prosody.

inputs
   PCEscWindow       pWindow - Window to use for the display
   PCM3DWave         pWave - Wave file to modify, or NULL if no wave file exists.
   PWSTR             pszText - Initial text to be spoken, or NULL if none (or use from pWave)
   PWSTR             pszSpeaker - Initial speaker, or NULL if nont (or use from pWave)
   PCMem             pMemText - Filled with the tagged text to use for the transplanted
                     prosody. (Assuming that return indicates valid text)
returns
   DWORD - 0 if user pressed cancel or closed window, 1 if pressed back, 2 if accepted
      transplanted prosody result
*/
DWORD CTTSTransPros::Dialog (PCEscWindow pWindow, PCM3DWave pWave, PWSTR pszText, PWSTR pszSpeaker, PCMem pMemText)
{
   CM3DWave Wave, WaveTP;

   // fill in the wave info
   if (pWave)
      m_pWave = pWave;
   else {
      Wave.ConvertSamplesAndChannels (22050, 1, NULL);
      m_pWave = &Wave;
   }

   // transplanted prosody wave
   WaveTP.ConvertSamplesAndChannels (22050, 1, NULL);
   m_pWaveTP = &WaveTP;
   m_dwQualityWaveTP = -1;

   m_pMemText = pMemText;

   // page
   PWSTR pszRet;

   // bring up record page
   // set up the text and speaker
   if (pszText)
      if (m_pWave->m_memSpoken.Required((wcslen(pszText)+1)*sizeof(WCHAR)))
         wcscpy ((PWSTR)m_pWave->m_memSpoken.p, pszText);
   if (pszSpeaker)
      if (m_pWave->m_memSpeaker.Required((wcslen(pszSpeaker)+1)*sizeof(WCHAR)))
         wcscpy ((PWSTR)m_pWave->m_memSpeaker.p, pszSpeaker);

   m_fModOrig = m_fKeepRec = (pWave && m_pWave->m_dwSamples) ? TRUE : FALSE;
   m_fKeepSeg = m_fModOrig && m_fKeepRec && (m_pWave->m_lWVPHONEME.Num() > 1) && (m_pWave->m_lWVWORD.Num() > 1);
prerecord:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTRANSPROS1, TransPros1Page, this);
   if (!pszRet)
      return 0;
   if (!_wcsicmp(pszRet, Back()))
      return 1;
   if (_wcsicmp(pszRet, L"next"))
      return 0;   // some sort of cancel

   if (!m_fKeepRec) {
      PCM3DWave pNew = m_pWave->Record (pWindow->m_hWnd, TRUE, NULL);
      if (!pNew)
         goto prerecord; // assume went back

      m_pWave->ReplaceSection (0, m_pWave->m_dwSamples, pNew);
      delete pNew;
   }

   // do SR
   if (!m_fKeepSeg) {
      m_pWave->m_dwSRSamples = 0;
      memset (m_pWave->m_adwPitchSamples, 0, sizeof(m_pWave->m_adwPitchSamples));
      m_pWave->m_lWVPHONEME.Clear();
      m_pWave->m_lWVWORD.Clear();
   }
   if (!m_pWave->m_dwSRSamples || !m_pWave->m_adwPitchSamples[PITCH_F0] ||
      (m_pWave->m_lWVPHONEME.Num() <= 1) ||
      (m_pWave->m_lWVWORD.Num() <= 1)) {

      CProgress Progress;
      Progress.Start (pWindow->m_hWnd, "Analyzing...", TRUE);

#ifdef _DEBUG
      DWORD dwTimeStart = GetTickCount();
#endif

      if (!m_pWave->m_adwPitchSamples[PITCH_F0] || !m_pWave->m_dwSRSamples) {
         // make sure they're all the sample rate
         m_pWave->m_dwSRSamples = 0;
         memset (m_pWave->m_adwPitchSamples, 0, sizeof(m_pWave->m_adwPitchSamples));
         m_pWave->m_dwSRSAMPLESPERSEC = VOICECHATSAMPLESPERSEC;   // so faster
         m_pWave->m_dwSRSkip = m_pWave->m_dwSamplesPerSec / m_pWave->m_dwSRSAMPLESPERSEC;  // 1/100th of a second
         DWORD dwPitchSub;
         for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
            m_pWave->m_adwPitchSkip[dwPitchSub] = pWave->m_dwSRSkip;

         if (!m_pWave->m_adwPitchSamples[PITCH_F0]) {
            Progress.Push (0, .25);
            m_pWave->CalcPitch (WAVECALC_TRANSPROS, &Progress);
            Progress.Pop();
         }

         if (!m_pWave->m_dwSRSamples) {
            Progress.Push (.25, .5);
            m_pWave->CalcSRFeatures (WAVECALC_TRANSPROS, &Progress);
            Progress.Pop();
         }
      }

      if (!m_pWave->m_lWVPHONEME.Num() || !m_pWave->m_lWVWORD.Num()) {
         Progress.Push (.5, 1);
         m_pSR->Recognize ((PWSTR)m_pWave->m_memSpoken.p, m_pWave, TRUE, &Progress);
         Progress.Pop();
      }

#ifdef _DEBUG
      WCHAR szTemp[128];
      swprintf (szTemp, L"\r\nTransPros time = %d", (int)(GetTickCount() - dwTimeStart));
      OutputDebugStringW (szTemp);
#endif
   } // done

   // pull up next dialog
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTRANSPROS2, TransPros2Page, this);
   if (!pszRet)
      return 0;
   if (!_wcsicmp(pszRet, Back())) {
      m_fModOrig = m_fKeepSeg = m_fKeepRec = FALSE;   // so record entire thing
      goto prerecord;
   }
   if (!_wcsicmp(pszRet, L"next")) {
      PWSTR psz = (PWSTR)m_aMemQuality[m_dwQuality].p;
      DWORD dwLen = (DWORD)wcslen(psz);
      if (!pMemText->Required((dwLen+1)*sizeof(WCHAR)))
         return 0;
      wcscpy ((PWSTR)pMemText->p, psz);
      return 2;
   }
   return 0;
}




/*********************************************************************************
TransProsQuick2Page */

static BOOL TransProsQuick2Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCTTSTransPros ptp = (PCTTSTransPros)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         TTSCacheDefaultGet (pPage, L"ttsfile");

         // calculate the pitch
         if (ptp->m_fAvgPitch < CLOSE)
            ptp->m_fAvgPitch = ptp->m_pWave->PitchOverRange (PITCH_F0, 0, ptp->m_pWave->m_dwSamples, 0, NULL, NULL, NULL, FALSE);
         ptp->m_fAvgPitch = max(ptp->m_fAvgPitch, 1);

         // reset the quality text
         pPage->Message (ESCM_USER+85, NULL);

         // choose a quality
         ptp->m_dwQuality = 4;   // 3 per phoneme

         // play this
         pPage->Message (ESCM_USER+86, NULL);

      }
      break;


   case ESCM_USER+85:   // reset the quality text
      {
         // get the text

         DWORD i;
         DWORD *pdw = (DWORD*)pParam;
         PWSTR pszText = (PWSTR)ptp->m_pWave->m_memSpoken.p;
         for (i = (pdw ? pdw[0] : 0); i < (pdw ? (pdw[0]+1) : NUMTPQUAL); i++) {
            MemZero (&ptp->m_aMemQuality[i]);

            switch (i) {
            case 0: // pure synthesizes
               if (pszText)
                  MemCatSanitize (&ptp->m_aMemQuality[i], pszText);
               break;
            case 1:  // per-word
               ptp->WaveToPerWordTP (pszText, ptp->m_pWave, NULL, &ptp->m_aMemQuality[i]);
               break;
            case 2:  // 1 per phone
            case 3:  // 2 per phone
            case 4:  // 3 per phone
               ptp->WaveToPerPhoneTP (pszText, ptp->m_pWave, i - 1, ptp->m_pTTS->Lexicon(), NULL, &ptp->m_aMemQuality[i]);
               break;
            }

         } // i

      }
      return TRUE;

   case ESCM_USER+86:   // play this
      {
         ptp->m_pWave->QuickPlayStop();
         ptp->m_pWaveTP->QuickPlayStop();

         // if current synth is not the same as what looking for then synthesize
         BOOL fRet = TRUE;
         if (ptp->m_dwQuality != ptp->m_dwQualityWaveTP) {
            ptp->m_pWaveTP->BlankWaveToSize (0, TRUE);

            // text
            PWSTR pszText = (PWSTR)ptp->m_aMemQuality[ptp->m_dwQuality].p;

            // tts it
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Synthesizing...");
            fRet = ptp->m_pTTS->SynthGenWave (ptp->m_pWaveTP, ptp->m_pWaveTP->m_dwSamplesPerSec, pszText, TRUE, 1 /*iTTSQuality*/, FALSE /* fDisablePCM */, &Progress);

            ptp->m_dwQualityWaveTP = ptp->m_dwQuality;
         }

         // play it
         if (fRet)
            ptp->m_pWaveTP->QuickPlay();
         else
            pPage->MBWarning (L"Text-to-speech couldn't speak the text.",
               L"You may have modified the transplanted prosody text and accidentally broken it.");
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"newttsfile")) {
            if (TTSCacheDefaultUI (pPage->m_pWindow->m_hWnd)) {
               TTSCacheDefaultGet (pPage, L"ttsfile");

               if (ptp->m_pTTS)
                  TTSCacheClose (ptp->m_pTTS);
               ptp->m_pTTS = TTSCacheOpen (NULL, TRUE, FALSE);

               // play new one
               ptp->m_dwQualityWaveTP = -1;
               pPage->Message (ESCM_USER+86);
            }
            return TRUE;
         }

         else if (!_wcsicmp(psz, L"playorig")) {
            ptp->m_pWave->QuickPlayStop();
            ptp->m_pWaveTP->QuickPlayStop();
            ptp->m_pWave->QuickPlay();
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"playtp")) {
            pPage->Message (ESCM_USER+86);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Transplanted prosody review";
            return TRUE;
         }
      }
      break;

   }

   return FALSE;  // no DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CTTSTransPros::DialogQuick - Pulls up the dialog for tranplanted prosody that's
designed for fast and easy transplanted prosody.

inputs
   PCEscWindow       pWindow - Window to use for the display
   PWSTR             pszText - Initial text to be spoken, or NULL if none (or use from pWave)
   PWSTR             pszPreModText - If not NULL, the text to write in the <PreModText> chunk
   PCMem             pMemText - Filled with the tagged text to use for the transplanted
                     prosody. (Assuming that return indicates valid text)
returns
   DWORD - 0 if user pressed cancel or closed window, 1 if pressed back, 2 if accepted
      transplanted prosody result
*/
DWORD CTTSTransPros::DialogQuick (PCEscWindow pWindow, PWSTR pszText, PWSTR pszPreModText, PCMem pMemText)
{
   CM3DWave Wave, WaveTP;

   // fill in the wave info
   Wave.ConvertSamplesAndChannels (22050, 1, NULL);
   m_pWave = &Wave;

   // transplanted prosody wave
   WaveTP.ConvertSamplesAndChannels (22050, 1, NULL);
   m_pWaveTP = &WaveTP;
   m_dwQualityWaveTP = -1;

   m_pMemText = pMemText;

   // page
   PWSTR pszRet;

   // bring up record page
   // set up the text and speaker
   if (pszText)
      if (m_pWave->m_memSpoken.Required((wcslen(pszText)+1)*sizeof(WCHAR)))
         wcscpy ((PWSTR)m_pWave->m_memSpoken.p, pszText);

   // store away the pre-mod text
   MemZero (&m_memPreModText);
   if (pszPreModText)
      MemCat (&m_memPreModText, pszPreModText);

   m_fModOrig = m_fKeepRec = FALSE;
   m_fKeepSeg = FALSE;
prerecord:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTRANSPROSQUICK1, TransPros1Page, this);
            // NOTE: Using same page handler as TransPros1Page
   if (!pszRet)
      return 0;
   if (!_wcsicmp(pszRet, Back()))
      return 1;
   if (_wcsicmp(pszRet, L"next"))
      return 0;   // some sort of cancel

   // record
   PCM3DWave pNew = m_pWave->Record (pWindow->m_hWnd, TRUE, NULL);
   if (!pNew)
      goto prerecord; // assume went back

   m_pWave->ReplaceSection (0, m_pWave->m_dwSamples, pNew);
   delete pNew;

   // do SR
   m_pWave->m_dwSRSamples = 0;
   memset (m_pWave->m_adwPitchSamples, 0, sizeof(m_pWave->m_adwPitchSamples));
   m_pWave->m_lWVPHONEME.Clear();
   m_pWave->m_lWVWORD.Clear();
   {
      CProgress Progress;
      Progress.Start (pWindow->m_hWnd, "Analyzing...", TRUE);

#ifdef _DEBUG
      DWORD dwTimeStart = GetTickCount();
#endif

      if (!m_pWave->m_adwPitchSamples[PITCH_F0] || !m_pWave->m_dwSRSamples) {
         // make sure they're all the sample rate
         m_pWave->m_dwSRSamples = 0;
         memset (m_pWave->m_adwPitchSamples, 0, sizeof(m_pWave->m_adwPitchSamples));
         m_pWave->m_dwSRSAMPLESPERSEC = VOICECHATSAMPLESPERSEC;   // so faster
         m_pWave->m_dwSRSkip = m_pWave->m_dwSamplesPerSec / m_pWave->m_dwSRSAMPLESPERSEC;  // 1/100th of a second
         DWORD dwPitchSub;
         for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
            m_pWave->m_adwPitchSkip[dwPitchSub] = m_pWave->m_dwSRSkip;

         if (!m_pWave->m_adwPitchSamples[PITCH_F0]) {
            Progress.Push (0, .25);
            m_pWave->CalcPitch (FALSE, &Progress);
            Progress.Pop();
         }

         if (!m_pWave->m_dwSRSamples) {
            Progress.Push (.25, .5);
            m_pWave->CalcSRFeatures (WAVECALC_TRANSPROS, &Progress);
            Progress.Pop();
         }
      }

      if (!m_pWave->m_lWVPHONEME.Num() || !m_pWave->m_lWVWORD.Num()) {
         Progress.Push (.5, 1);
         m_pSR->Recognize ((PWSTR)m_pWave->m_memSpoken.p, m_pWave, TRUE, &Progress);
         Progress.Pop();
      }

#ifdef _DEBUG
      WCHAR szTemp[128];
      swprintf (szTemp, L"\r\nTransPros time = %d", (int)(GetTickCount() - dwTimeStart));
      OutputDebugStringW (szTemp);
#endif
   } // done

   // pull up next dialog
   m_dwQualityWaveTP = -1; // make sure reset
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTRANSPROSQUICK2, TransProsQuick2Page, this);
   if (!pszRet)
      return 0;
   if (!_wcsicmp(pszRet, Back())) {
      m_fModOrig = m_fKeepSeg = m_fKeepRec = FALSE;   // so record entire thing
      goto prerecord;
   }

   if (!_wcsicmp(pszRet, L"next")) {
      PWSTR psz = (PWSTR)m_aMemQuality[m_dwQuality].p;
      DWORD dwLen = (DWORD)wcslen(psz);
      if (!pMemText->Required((dwLen+1)*sizeof(WCHAR)))
         return 0;
      wcscpy ((PWSTR)pMemText->p, psz);
      return 2;
   }
   return 0;

}

/*************************************************************************************
CTTSTransPros::WaveToPerWordTP - This converts from a wave file into TP that
is one value per word. It uses m_fAvgPitch, m_dwPitchType, m_dwVolType,
m_dwDurType, m_fPitchAdjust, m_fPitchExpress, m_fVolAdjust, m_fDurAdjust,
and (if need be, for relative) m_szTTS.

inputs
   PWSTR             pszOrigText - Original text. Can be null.
   PCM3DWave         pWave - Wave to anayze
   PWSTR             pszTTS - TTS file name, or NULL to use defualt
   PCMem             pMem - Filled with a tagged string
returns
   BOOL - TRUE if success
*/
// WPW - structure for storing information about the word
typedef struct {
   DWORD          dwIndex;    // word index
   PWVWORD        pWord;      // word info
   PWSTR          pszText;    // text of the word
   fp             fPitch;     // pitch of the word
   fp             fVol;       // average energy of the word, from average of SRFEATUREEnergy()
   fp             fDur;       // duration of word in seconds
   DWORD          dwPhoneStart;  // phoneme start
   DWORD          dwPhoneEnd; // ending phoneme
} WPW, *PWPW;

BOOL CTTSTransPros::WaveToPerWordTP (PWSTR pszOrigText, PCM3DWave pWave, PWSTR pszTTS, PCMem pMem)
{
   // make sure some values within boundary
   if (m_fAvgPitch < CLOSE)
      m_fAvgPitch = pWave->PitchOverRange (PITCH_F0, 0, pWave->m_dwSamples, 0, NULL, NULL, NULL, FALSE);
   m_fAvgPitch = max(m_fAvgPitch, 1);
   m_fPitchAdjust = max(m_fPitchAdjust, CLOSE);
   m_fVolAdjust = max(m_fVolAdjust, CLOSE);
   m_fDurAdjust = max(m_fDurAdjust, CLOSE);

   // pull out words into a list
   CListFixed lWPW;
   lWPW.Init (sizeof(WPW));
   DWORD i, j;
   fp fAvgVol = 0;
   DWORD dwVolCount = 0;
   lWPW.Required (pWave->m_lWVWORD.Num());
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
      PWVWORD pw2 = (PWVWORD)pWave->m_lWVWORD.Get(i+1);
      PWSTR psz = (PWSTR)(pw+1);
      
      if (!psz[0])
         continue;
      for (j = 0; psz[j]; j++)
         if (!iswpunct(psz[j]) && !iswspace (psz[j]))
            break;
      if (!psz[j])
         continue;   // all punctuation

      // pull out the info
      WPW w;
      DWORD dwNext = pw2 ? pw2->dwSample : pWave->m_dwSamples;
      if (dwNext <= pw->dwSample)
         continue;
      w.dwIndex = i;
      w.fDur = (fp)(dwNext - pw->dwSample) / (fp)pWave->m_dwSamplesPerSec;
      w.fPitch = pWave->PitchOverRange (PITCH_F0, pw->dwSample, dwNext, 0, NULL, NULL, NULL, FALSE);
      w.pszText = psz;
      w.pWord = pw;
      w.fVol = 0;

      // find the duration
      PWVPHONEME pwp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(0);
      w.dwPhoneStart = 0xffffffff;
      w.dwPhoneEnd = 0;
      for (j = 0; j < pWave->m_lWVPHONEME.Num(); j++, pwp++) {
         if ((pwp->dwSample >= pw->dwSample) && (pwp->dwSample < dwNext)) {
            w.dwPhoneStart = min (w.dwPhoneStart, j);
            w.dwPhoneEnd = max (w.dwPhoneEnd, j+1);
         }
      } // j

      // calculate energy
      DWORD dwSRStart, dwSREnd, dwCount;
      dwSRStart = (pw->dwSample + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
      dwSREnd = (dwNext + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
      dwSREnd = min(dwSREnd, pWave->m_dwSRSamples);
      dwCount = 0;
      for (j = dwSRStart; j < dwSREnd; j++) {
         dwCount++;
         w.fVol += SRFEATUREEnergy (FALSE, pWave->m_paSRFeature + j, FALSE) / (fp)SRDATAPOINTS;
            // BUGFIX - Normalize by SRDATAPOINTS so that if change later wont affect TP
      } // j
      if (dwCount)
         w.fVol /= (fp)dwCount;

      // add
      lWPW.Add (&w);

      // calculate the average vol
      fAvgVol += w.fVol;
      dwVolCount++;
   } // i, all words

   // average volume
   if (dwVolCount)
      fAvgVol /= (fp)dwVolCount;
   fAvgVol = max(fAvgVol, 1);

   // get the TTS engine
   PCMTTS pTTS = NULL;
   // BUGFIX - Always load in TTS, so doesnt crash
   //if ((m_dwPitchType == 1) || (m_dwVolType == 1) || (m_dwDurType == 1))
      pTTS = TTSCacheOpen (pszTTS, TRUE, FALSE);

   CListFixed lPCMTTSTriPhone;
   PCMLexicon pLex = pTTS ? pTTS->Lexicon() : NULL;
   if (!WaveDeterminePhone (pWave, pLex, pTTS, &lPCMTTSTriPhone))
      return NULL;
   PCMTTSTriPhonePros *pptp = (PCMTTSTriPhonePros*)lPCMTTSTriPhone.Get(0);

   // write out the words...
   MemZero (pMem);
   MemCat (pMem, L"<TransPros>\r\n");
   if (pszOrigText) {
      MemCat (pMem, L"\t<OrigText>");
      MemCatSanitize (pMem, pszOrigText);
      MemCat (pMem, L"</OrigText>\r\n\r\n");
   }

   PWSTR pszPreModText = (PWSTR)m_memPreModText.p;
   if (pszPreModText && pszPreModText[0]) {
      MemCat (pMem, L"\t<PreModText>");
      MemCatSanitize (pMem, pszPreModText);
      MemCat (pMem, L"</PreModText>\r\n\r\n");
   }

   PWPW pwp = (PWPW) lWPW.Get(0);
   DWORD dwNum = lWPW.Num();
   DWORD dwCurPWP = 0;
   WCHAR szTemp[128];
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      while ((dwCurPWP < dwNum) && (i > pwp[dwCurPWP].dwIndex))
         dwCurPWP++;

      if ((dwCurPWP < dwNum) && (i == pwp[dwCurPWP].dwIndex)) {

         // have the word...
         
         //MemCat (pMem, L"\t<Emphasis");
         MemCat (pMem, L"\t<Word text=\"");
         MemCatSanitize (pMem, pwp[dwCurPWP].pszText);
         MemCat (pMem, L"\"");

         // what is the pitch
         fp fPitch = pwp[dwCurPWP].fPitch;
         if (m_fPitchExpress != 1) {
            fPitch = log(fPitch / m_fAvgPitch);
            fPitch *= m_fPitchExpress;
            fPitch = exp(fPitch);
            fPitch *= m_fAvgPitch;
         }
         fPitch *= m_fPitchAdjust;

         if (m_dwPitchType == 1) {  // relative pitch
            swprintf (szTemp, L" PitchRel=%.2f", (double)(fPitch / m_fAvgPitch + 0.005));
            MemCat (pMem, szTemp);
         }
         else if (m_dwPitchType == 2) { // absolute pitch
            swprintf (szTemp, L" PitchAbs=%.1f", (double)fPitch + 0.05);
            MemCat (pMem, szTemp);
         }

         // volume
         fp fVol = pwp[dwCurPWP].fVol * m_fVolAdjust;
         if (m_dwVolType == 1) { // relative volume
            swprintf (szTemp, L" VolRel=%.2f", (double)(fVol / fAvgVol + 0.005));
            MemCat (pMem, szTemp);
         }
         else if (m_dwVolType == 2) { // absolute volume
            swprintf (szTemp, L" VolAbs=%.1f", (double)fVol + 0.05);
            MemCat (pMem, szTemp);
         }

         // duration
         fp fDur = pwp[dwCurPWP].fDur * m_fDurAdjust;
         PCMLexicon pLex;
         if ((m_dwDurType == 1) && pTTS && (pLex = pTTS->Lexicon())) { // relative duration
            CListVariable lForm, lDontRecurse;
            lDontRecurse.Clear();
            pLex->WordPronunciation (pwp[dwCurPWP].pszText, &lForm, TRUE, NULL, &lDontRecurse);
            if (!lForm.Num())
               goto skipdur;

            // phonemes
            PBYTE pb = ((PBYTE)lForm.Get(0)) + 1;
            DWORD dwLen = (DWORD)strlen((char*)pb);
            for (j = 0; j < dwLen; j++)
               pb[j]--;
            BYTE bSilence = (BYTE)pLex->PhonemeFindUnsort(pLex->PhonemeSilence());

            // get all the triphones
            DWORD dwSum = 0;
            DWORD dwWord = pTTS->LexWordsGet()->WordFind(pwp[dwCurPWP].pszText);
            for (j = pwp[dwCurPWP].dwPhoneStart; j < pwp[dwCurPWP].dwPhoneEnd; j++) {
               PCMTTSTriPhonePros ptp = pptp[j];
               // BUGFIX - Was
               //PCMTTSTriPhone ptp =pTTS->TriPhoneMatch (pb[j], ((j == 0) ? 0x01 : 0) | ((j+1 >= dwLen) ? 0x02 : 0),
               //   j ? pb[j-1] : bSilence, (j+1 < dwLen) ? pb[j+1] : bSilence,
               //   (j >= 2) ? pb[j-2] : bSilence, (j+2 < dwLen) ? pb[j+2] : bSilence,
               //   dwWord);
               if (ptp)
                  dwSum += ptp->m_wDuration;
            } // j
            dwSum = max(dwSum, 1);  // at least one duration

            swprintf (szTemp, L" DurRel=%.2f", (double)(fDur / (fp)dwSum * (fp)pWave->m_dwSRSAMPLESPERSEC + 0.005));
            MemCat (pMem, szTemp);
         }
         else if (m_dwDurType == 2) { // absolute duration
            swprintf (szTemp, L" DurAbs=%.2f", (double)fDur + 0.005);
            MemCat (pMem, szTemp);
         }
skipdur:

         // finish off word
         //MemCat (pMem, L">");
         //MemCatSanitize (pMem, pwp[dwCurPWP].pszText);
         //MemCat (pMem, L"</Emphasis>\r\n");
         MemCat (pMem, L"/>\r\n");
         continue;
      }
      else {
         // either punctuation or silence
         PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
         PWVWORD pw2 = (PWVWORD)pWave->m_lWVWORD.Get(i+1);
         PWSTR psz = (PWSTR)(pw+1);

         for (j = 0; psz[j]; j++)
            if (!iswspace(psz[j]))
               break;
         fp fDur = (fp)(pw2 ? pw2->dwSample : pWave->m_dwSamples) - (fp)pw->dwSample;
         fDur = fDur / (fp)pWave->m_dwSamplesPerSec;
         fDur = max(fDur, 1.0 / (fp)pWave->m_dwSRSAMPLESPERSEC);

         if (psz[j]) {
            // punctuation
            MemCat (pMem, L"\t<Punct text=\"");
            MemCatSanitize (pMem, psz);
            MemCat (pMem, L"\"");
            if (m_dwDurType) {
               swprintf (szTemp, L" DurAbs=%.2f", (double)fDur + 0.005);
               MemCat (pMem, szTemp);
            }
            MemCat (pMem, L"/>\r\n\r\n");
         }
         else if (m_dwDurType) {
            MemCat (pMem, L"\t<Break time=");
            MemCat (pMem, (int)(fDur * 1000.0));
            MemCat (pMem, L"ms/>\r\n");
         }
      }
   } // i

   MemCat (pMem, L"</TransPros>\r\n");

   // free tts
   if (pTTS)
      TTSCacheClose (pTTS);

   // done
   return TRUE;
}




/*************************************************************************************
CTTSTransPros::WaveDeterminePhone - Determine the triphones for a wave.

inputs
   PCM3DWave         pWave - Wave
   PCMLexicon        pLex - Lexicon
   PCMTTS            pTTS - TTS
   PCListFixed       plPCMTTSTriPhonePros - Will be initialized and filled with one
                     triphone per wave
returns
   BOOL - TRUE if success
*/
BOOL CTTSTransPros::WaveDeterminePhone (PCM3DWave pWave, PCMLexicon pLex, PCMTTS pTTS,
                                        PCListFixed plPCMTTSTriPhonePros)
{
   BYTE bSilence = (BYTE)pLex->PhonemeFindUnsort(pLex->PhonemeSilence());

   CListFixed lTempPhone, lTempWord;
   DWORD i, j;
   lTempPhone.Init (sizeof(DWORD));
   lTempWord.Init (sizeof(DWORD));
   PWVPHONEME pwp = (PWVPHONEME)pWave->m_lWVPHONEME.Get(0);
   lTempPhone.Required (pWave->m_lWVPHONEME.Num());
   lTempWord.Required (pWave->m_lWVPHONEME.Num());
   for (i = 0; i < pWave->m_lWVPHONEME.Num(); i++, pwp++) {
      // figure out which word it's in
      PWVWORD pw = NULL, pw2 = NULL;
      for (j = 0; j < pWave->m_lWVWORD.Num(); j++) {
         pw = (PWVWORD)pWave->m_lWVWORD.Get(j);
         pw2 = (PWVWORD)pWave->m_lWVWORD.Get(j+1);
         if ((pwp->dwSample >= pw->dwSample) && (!pw2 || (pwp->dwSample < pw2->dwSample)))
            break;
      } // j
      PWSTR pszWord = (j < pWave->m_lWVWORD.Num()) ? (PWSTR)(pw+1) : NULL;
      DWORD dwWord = (DWORD)-1;
      if (pszWord && pszWord[0])
         dwWord = pLex->WordFind (pszWord, FALSE);

      // determine phoneme
      WCHAR szPhone[sizeof(pwp->awcNameLong) / sizeof(WCHAR)];
      memset (szPhone, 0, sizeof(szPhone));
      memcpy (szPhone, pwp->awcNameLong, sizeof(pwp->awcNameLong));
      DWORD dwFind = pLex->PhonemeFindUnsort(szPhone);
      if (dwFind == -1)
         dwFind = bSilence;
      DWORD dwPhone = (BYTE) dwFind;

      // word start/end
      if (pwp->dwSample == pw->dwSample)
         dwPhone |= (1 << 24);
      if ((j+1 >= pWave->m_lWVWORD.Num()) || (i+1 >= pWave->m_lWVPHONEME.Num()) ||
         (pwp[1].dwSample >= pw2->dwSample))
         dwPhone |= (2 << 24);

      // add to lists
      lTempPhone.Add (&dwPhone);
      lTempWord.Add (&dwWord);
   } // i
   if (!pTTS || !pTTS->SynthDetermineTriPhonePros ((DWORD*)lTempPhone.Get(0), /*(DWORD*)lTempWord.Get(0),*/
      lTempPhone.Num(), plPCMTTSTriPhonePros))
      return FALSE;

   return TRUE;
}



/*************************************************************************************
CTTSTransPros::WaveToPerPhoneTP - This converts from a wave file into TP that
is one value per word. It uses m_fAvgPitch, m_dwPitchType, m_dwVolType,
m_dwDurType, m_fPitchAdjust, m_fPitchExpress, m_fVolAdjust, m_fDurAdjust,
and (if need be, for relative) m_szTTS.

inputs
   PWSTR             pszOrigText - Original text. Can be null.
   PCM3DWave         pWave - Wave to anayze
   DWORD             dwPitch - Pitch points per phoneme, 1+
   PCMLexicon        pLex - Lexicon to use
   PWSTR             pszTTS - TTS file, or NULL for default
   PCMem             pMem - Filled with a tagged string
returns
   BOOL - TRUE if success
*/
BOOL CTTSTransPros::WaveToPerPhoneTP (PWSTR pszOrigText, PCM3DWave pWave, DWORD dwPitch,
                                      PCMLexicon pLex, PWSTR pszTTS, PCMem pMem)
{
   // make sure some values within boundary
   if (m_fAvgPitch < CLOSE)
      m_fAvgPitch = pWave->PitchOverRange (PITCH_F0, 0, pWave->m_dwSamples, 0, NULL, NULL, NULL, FALSE);
   m_fAvgPitch = max(m_fAvgPitch, 1);
   m_fPitchAdjust = max(m_fPitchAdjust, CLOSE);
   m_fVolAdjust = max(m_fVolAdjust, CLOSE);
   m_fDurAdjust = max(m_fDurAdjust, CLOSE);

   BYTE bSilence = (BYTE)pLex->PhonemeFindUnsort(pLex->PhonemeSilence());

   // write out the words...
   MemZero (pMem);
   MemCat (pMem, L"<TransPros>\r\n");
   if (pszOrigText) {
      MemCat (pMem, L"\t<OrigText>");
      MemCatSanitize (pMem, pszOrigText);
      MemCat (pMem, L"</OrigText>\r\n\r\n");
   }


   PWSTR pszPreModText = (PWSTR)m_memPreModText.p;
   if (pszPreModText && pszPreModText[0]) {
      MemCat (pMem, L"\t<PreModText>");
      MemCatSanitize (pMem, pszPreModText);
      MemCat (pMem, L"</PreModText>\r\n\r\n");
   }

   // get the TTS engine
   PCMTTS pTTS = NULL;
   // BUGFIX - Always load in TTS, so doesnt crash
   // if ((m_dwPitchType == 1) || (m_dwVolType == 1) || (m_dwDurType == 1))
      pTTS = TTSCacheOpen (pszTTS, TRUE, FALSE);

   // figure out which phonemes would be used for the wave
   DWORD i, j;
   CListFixed lPCMTTSTriPhone;
   if (!WaveDeterminePhone (pWave, pLex, pTTS, &lPCMTTSTriPhone))
      return NULL;
   PCMTTSTriPhonePros *pptp = (PCMTTSTriPhonePros*)lPCMTTSTriPhone.Get(0);



   // loop through all the phonemes
   WCHAR szTemp[128];
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
      PWVWORD pw2 = (PWVWORD)pWave->m_lWVWORD.Get(i+1);
      DWORD dwStart = pw->dwSample;
      DWORD dwNext = pw2 ? pw2->dwSample : pWave->m_dwSamples;
      PWSTR pszWord = (PWSTR)(pw+1);

      // determine if punctuation
      for (j = 0; pszWord[j]; j++)
         if (!iswpunct(pszWord[j]) && !iswspace (pszWord[j]))
            break;
      if (!pszWord[j]) {
         fp fDur = (fp)(dwNext - dwStart);
         fDur = fDur / (fp)pWave->m_dwSamplesPerSec;
         fDur = max(fDur, 1.0 / (fp)pWave->m_dwSRSAMPLESPERSEC);

         if (pszWord[0]) {
            // punctuation
            MemCat (pMem, L"\t<Punct text=\"");
            MemCatSanitize (pMem, pszWord);
            MemCat (pMem, L"\"");
            if (m_dwDurType) {
               swprintf (szTemp, L" DurAbs=%.2f", (double)fDur + 0.005);
               MemCat (pMem, szTemp);
            }
            MemCat (pMem, L"/>\r\n\r\n");
         }
         else if (m_dwDurType) {
            MemCat (pMem, L"\t<Break time=");
            MemCat (pMem, (int)(fDur * 1000.0));
            MemCat (pMem, L"ms/>\r\n");
         }
         continue;   // all punctuation
      }

      // find the phonemes this encompases
      DWORD dwPhoneStart = 0xffffffff, dwPhoneEnd = 0;
      DWORD dwNumPhone = pWave->m_lWVPHONEME.Num();
      PWVPHONEME pwp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(0);
      for (j = 0; j < dwNumPhone; j++, pwp++) {
         PWVPHONEME pwp2 = (j+1 < dwNumPhone) ? (pwp + 1) : NULL;
         if (pwp->dwSample >= dwNext)
            break;   // gone beyond end

         if (pwp2 && (pwp2->dwSample <= dwStart))
            continue;   // not far enough

         // else, in word
         dwPhoneStart = min(dwPhoneStart, j);
         dwPhoneEnd = max(dwPhoneEnd, j);
      } // j
      if (dwPhoneEnd < dwPhoneStart)
         continue;   // shouldnt happen, but no phonemes assigned
      dwPhoneEnd++;  // so phonened exclusive

#define EXTRAPHONE      2     // BUGFIX - Get phonemes around for more accurate energy calc
      // get the phonemes as binary...
      BYTE abPhone[128];
      // PCMTTSTriPhone abTP[128];
      DWORD adwPhone[sizeof(abPhone)+2*EXTRAPHONE];  // keep some extras
      pwp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(0);
      if (dwPhoneEnd - dwPhoneStart + 1 >= sizeof(abPhone))
         continue;// too many phonemes
      int iLoop;
      for (iLoop = (int)dwPhoneStart - EXTRAPHONE; iLoop < (int)dwPhoneEnd + EXTRAPHONE; iLoop++) {
         if ((iLoop < 0) || (iLoop >= (int)pWave->m_lWVPHONEME.Num())) {
            adwPhone[iLoop - (int)dwPhoneStart + EXTRAPHONE] = bSilence;
            continue;
         }

         WCHAR szPhone[sizeof(pwp[iLoop].awcNameLong) / sizeof(WCHAR)];
         memset (szPhone, 0, sizeof(szPhone));
         memcpy (szPhone, pwp[iLoop].awcNameLong, sizeof(pwp[iLoop].awcNameLong));
         DWORD dwFind = pLex->PhonemeFindUnsort(szPhone);
         if (dwFind == -1)
            dwFind = bSilence;
         if ((iLoop >= (int)dwPhoneStart) && (iLoop < (int)dwPhoneEnd)) {
            abPhone[iLoop-(int)dwPhoneStart] = (BYTE)dwFind + 1;
         }
         adwPhone[iLoop-(int)dwPhoneStart+EXTRAPHONE] = (DWORD)(BYTE)dwFind +
            ((iLoop <= (int)dwPhoneStart) ? (1 << 24) : 0) + ((iLoop+1 >= (int)dwPhoneEnd) ? (2 << 24) : 0);
         // abTP[iLoop - (int)dwPhoneStart] = pptp[iLoop];
      }
      dwNumPhone = dwPhoneEnd - dwPhoneStart;
      abPhone[dwNumPhone] = 0;   // null terminate

      // set the pronunciations...
      WCHAR szPron[256];
      if (!pLex->PronunciationToText (abPhone, szPron, sizeof(szPron)/sizeof(WCHAR)))
         continue;   // error, soo large


      // write out
      MemCat (pMem, L"\t<Word text=\"");
      MemCatSanitize (pMem, pszWord);
      MemCat (pMem, L"\" ph=\"");
      MemCatSanitize (pMem, szPron);
      MemCat (pMem, L"\"");

      // loop through pitch, energy, and duration
      DWORD dwInfo;
      for (dwInfo = 0; dwInfo < 3; dwInfo++) {
         DWORD dwType;
         PWSTR pszType;
         switch (dwInfo) {
         case 0:  // pitch
         default:
            dwType = m_dwPitchType;
            pszType = (dwType == 2) ? L" TPPitchAbs=" : L" TPPitchRel=";
            break;
         case 1:  // energy
            dwType = m_dwVolType;
            pszType = (dwType == 2) ? L" TPVolAbs=" : L" TPVolRel=";
            break;
         case 2:  // duration
            dwType = m_dwDurType;
            pszType = (dwType == 2) ? L" TPDurAbs=" : L" TPDurRel=";
            break;
         }
         if (!dwType)
            continue;
         MemCat (pMem, pszType);

         // loop over all the phonemes, and if pitch, extra loop
         DWORD dwSub, dwMaxSub;
         dwMaxSub = (dwInfo == 0) ? dwPitch : 1;
         for (j = dwPhoneStart; j < dwPhoneEnd; j++) for (dwSub = 0; dwSub < dwMaxSub; dwSub++) {
            // what's the index into the phonemes
            DWORD dwIndex = j - dwPhoneStart;
            fp fVal = -1;

            // this phoneme and next
            DWORD dwSampleStart, dwSampleEnd;
            PWVPHONEME pwp2;
            pwp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(j);
            pwp2 = (PWVPHONEME) pWave->m_lWVPHONEME.Get(j+1);
            dwSampleStart = pwp->dwSample;
            dwSampleEnd = pwp2 ? pwp2->dwSample : pWave->m_dwSamples;

            // switch based on type
            switch (dwInfo) {
            default:
            case 0:  // pich
               {
                  // if it's unvoiced then don't bother
                  PLEXPHONE plp = pLex->PhonemeGetUnsort ((WORD)adwPhone[dwIndex+EXTRAPHONE]);
                  if (!plp)
                     break;
                  PLEXENGLISHPHONE ple = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
                  if (!ple)
                     break;
                  if (!(ple->dwCategory & PIC_VOICED))
                     break;   // not voiced, so no pitch

                  // if not very confident about pitch over the range then skip too
                  fp fStrength;
                  pWave->PitchOverRange (PITCH_F0, dwSampleStart, dwSampleEnd, 0, &fStrength, NULL, NULL, FALSE);
                  if (fStrength < 20)   // BUGFIX - Was 2000. Changed to 20 since was eliminating too many
                     break;

                  // get the pitch at this point
                  fp fSample = (fp)(dwSub+1) / (fp)(dwMaxSub+1) * (fp)(dwSampleEnd - dwSampleStart) +
                     (fp)dwSampleStart;
                  fVal = pWave->PitchAtSample (PITCH_F0, fSample, 0);
                  fVal = max(fVal, 1);

                  // adjust by settings
                  if (m_fPitchExpress != 1) {
                     fVal = log(fVal / m_fAvgPitch);
                     fVal *= m_fPitchExpress;
                     fVal = exp(fVal);
                     fVal *= m_fAvgPitch;
                  }
                  fVal *= m_fPitchAdjust;

                  if (dwType == 1)
                     fVal /= m_fAvgPitch;
               }
               break;

            case 1:  // vol
               {
                  // calculate the volume for the phoneme
                  fVal = pWave->CalcSREnergyRange (FALSE, dwSampleStart, dwSampleEnd);
                  fVal *= m_fVolAdjust;

                  if ((dwType != 1) || !pTTS) {
                     // BUGFIX - Scale by (fp)SRDATAPOINTS so that if change the
                     // value later on wont have to change TP
                     fVal /= (fp)SRDATAPOINTS;
                     break; // all done if looking for absolute
                  }

                  // else, figure out energy for this
                  DWORD dwCount;
                  fp fTriPhone;
                  // BUGFIX - Changed how get triphone energy
                  fTriPhone = pptp[j]->m_fEnergyAvg;
                  dwCount = pptp[j]->m_wDuration;
                  // fTriPhone = pTTS->TriPhoneEnergy (pptp[j], &dwCount);
                  // BUGFIX - Was
                  //fTriPhone = pTTS->TriPhoneEnergy (adwPhone, pTTS->LexWordsGet()->WordFind(pszWord),
                  //   dwNumPhone+EXTRAPHONE*2, dwIndex+EXTRAPHONE, &dwCount);
                  if (fTriPhone > CLOSE)
                     fVal /= fTriPhone;
                  else
                     fVal = 1;   // cant really tell
               }
               break;

            case 2:  // duration
               {
                  fVal = (fp)(dwSampleEnd - dwSampleStart) / (fp)pWave->m_dwSamplesPerSec;
                  fVal *= m_fDurAdjust;

                  if ((dwType != 1) || !pTTS)
                     break; // all done if looking for absolute

                  // figure out typical duration
                  PCMTTSTriPhonePros ptp = pptp[j];
                  // BUGFIX - Was
                  //ptp = pTTS->TriPhoneMatch (
                  //   (BYTE)adwPhone[dwIndex+EXTRAPHONE],
                  //   (BYTE)(adwPhone[dwIndex+EXTRAPHONE] >> 24),
                  //   (BYTE)adwPhone[dwIndex-1+EXTRAPHONE],  // dont need bounds check beause extraphone >= 2
                  //   (BYTE)adwPhone[dwIndex+1+EXTRAPHONE],
                  //   (BYTE)adwPhone[dwIndex-2+EXTRAPHONE],
                  //   (BYTE)adwPhone[dwIndex+2+EXTRAPHONE],
                     //dwIndex ? (BYTE)adwPhone[dwIndex-1] : bSilence,
                     //(dwIndex+1 < dwNumPhone) ? (BYTE)adwPhone[dwIndex+1] : bSilence,
                     //(dwIndex >= 2) ? (BYTE)adwPhone[dwIndex-2] : bSilence,
                     //(dwIndex+2 < dwNumPhone) ? (BYTE)adwPhone[dwIndex+2] : bSilence,
                  //   pTTS->LexWordsGet()->WordFind(pszWord));
                  if (ptp)
                     fVal /= ((fp)ptp->m_wDuration / (fp)pWave->m_dwSRSAMPLESPERSEC);
                  else
                     fVal = 1;   // since dont know
               }
               break;
            }

            // if it's not the first one then write out a comma
            if (dwIndex || dwSub)
               MemCat (pMem, L",");

            if (fVal > CLOSE) {
               swprintf (szTemp, ((dwType == 1) || (dwInfo == 2)) ? L"%.2f" : L"%.1f", (double)fVal + .005);
                  // BUGFIX - Add .005 to deal with roundoff error
               MemCat (pMem, szTemp);
            }
         } // j and dwSub
      } // dwInfo

      MemCat (pMem, L"/>\r\n");
   } // i

   MemCat (pMem, L"</TransPros>\r\n");
   // done
   return TRUE;
}

