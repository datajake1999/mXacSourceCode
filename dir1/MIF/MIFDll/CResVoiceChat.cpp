/*************************************************************************************
CResVoiceChatInfo.cpp - Code for UI to modify a verb-window resource.

begun24/8/05 by Mike Rozak.
Copyright 2005 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"







/*************************************************************************************
CResVoiceChatInfo::Constructor and destructor
*/
CResVoiceChatInfo::CResVoiceChatInfo (void)
{
   m_lWAVEBASECHOICE.Init (sizeof(WAVEBASECHOICE));
   m_dwQuality = VCH_ID_MED;
   m_fAllowVoiceChat = TRUE;
}

CResVoiceChatInfo::~CResVoiceChatInfo (void)
{
   // do nothing
}


static PWSTR gpszQuality = L"Quality";
static PWSTR gpszAllowVoiceChat = L"AllowVoiceChat";
static PWSTR gpszWaveBase = L"WaveBase";
static PWSTR gpszFile = L"File";
static PWSTR gpszName = L"Name";
static PWSTR gpszPitch = L"Pitch";

/*************************************************************************************
CResVoiceChatInfo::MMLTo - Standard API
*/
PCMMLNode2 CResVoiceChatInfo::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityVoiceChatInfo());

   MMLValueSet (pNode, gpszQuality, (int)m_dwQuality);
   MMLValueSet (pNode, gpszAllowVoiceChat, (int) m_fAllowVoiceChat);
   
   DWORD i;
   PWAVEBASECHOICE pwbc = (PWAVEBASECHOICE) m_lWAVEBASECHOICE.Get(0);
   for (i = 0; i < m_lWAVEBASECHOICE.Num(); i++, pwbc++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         break;
      pSub->NameSet (gpszWaveBase);

      if (pwbc->szFile[0])
         MMLValueSet (pSub, gpszFile, pwbc->szFile);
      if (pwbc->szName[0])
         MMLValueSet (pSub, gpszName, pwbc->szName);
      MMLValueSet (pSub, gpszPitch, pwbc->fPitch);
   } // i


   return pNode;
}


/*************************************************************************************
CResVoiceChatInfo::MMLFrom - Standard API
*/
BOOL CResVoiceChatInfo::MMLFrom (PCMMLNode2 pNode)
{
   // clear existing
   m_lWAVEBASECHOICE.Clear();

   m_dwQuality = (DWORD) MMLValueGetInt (pNode, gpszQuality, (int)VCH_ID_MED);
   m_fAllowVoiceChat = (BOOL) MMLValueGetInt (pNode, gpszAllowVoiceChat, (int) TRUE);

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszWaveBase)) {
         WAVEBASECHOICE wbc;
         memset (&wbc, 0, sizeof(wbc));

         psz = MMLValueGet (pSub, gpszFile);
         if (psz && wcslen(psz) < sizeof(wbc.szFile)/sizeof(WCHAR)-1)
            wcscpy (wbc.szFile, psz);

         psz = MMLValueGet (pSub, gpszName);
         if (psz && wcslen(psz) < sizeof(wbc.szName)/sizeof(WCHAR)-1)
            wcscpy (wbc.szName, psz);

         wbc.fPitch = MMLValueGetDouble (pSub, gpszPitch, 0);

         m_lWAVEBASECHOICE.Add (&wbc);
      }
   } // i

   return TRUE;
}


/*************************************************************************************
FillListBoxWithFiles - Fills the listbox with files

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control name
   PCListFixed    plWAVEBASECHOICE - List of files to display
   DWORD          dwSel - Index to select
returns
   BOOL - TRUE if found and set
*/
static BOOL FillListBoxWithFiles (PCEscPage pPage, PWSTR pszControl,
                                  PCListFixed plWAVEBASECHOICE, DWORD dwSel)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // wipe out
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   // elements
   CMem mem;
   MemZero (&mem);
   DWORD i;
   PWAVEBASECHOICE pwbc = (PWAVEBASECHOICE)plWAVEBASECHOICE->Get(0);
   for (i = 0; i < plWAVEBASECHOICE->Num(); i++, pwbc++) {
      MemCat (&mem, L"<elem name=");
      MemCat (&mem, (int)i);
      MemCat (&mem, L">");

      MemCat (&mem, L"<bold>");
      MemCatSanitize (&mem, pwbc->szName);
      MemCat (&mem, L"</bold><br/>");

      MemCatSanitize (&mem, pwbc->szFile);
      MemCat (&mem, L" (");
      WCHAR szTemp[32];
      swprintf (szTemp, L"%.2f", (double)pwbc->fPitch);
      MemCatSanitize (&mem, szTemp);
      MemCat (&mem, L" hz)");

      MemCat (&mem, L"</elem>");
   } // i

   // write
   ESCMLISTBOXADD ca;
   memset (&ca, 0, sizeof(ca));
   ca.dwInsertBefore = -1;
   ca.pszMML = (PWSTR)mem.p;
   pControl->Message (ESCM_LISTBOXADD, &ca);

   // select
   pControl->AttribSetInt (CurSel(), (int)dwSel);

   return TRUE;
}



/*************************************************************************
ResVoiceChatInfoPage
*/
BOOL ResVoiceChatInfoPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCResVoiceChatInfo pv = (PCResVoiceChatInfo)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         FillListBoxWithFiles (pPage, L"verblist", &pv->m_lWAVEBASECHOICE, 0);

         // set other bits
         ComboBoxSet (pPage, L"quality", pv->m_dwQuality);
         if (pControl = pPage->ControlFind (L"allowvoicechat"))
            pControl->AttribSetBOOL (Checked(), pv->m_fAllowVoiceChat);

         // disable controls?
         if (pv->m_fReadOnly) {
            if (pControl = pPage->ControlFind (L"deleteverb"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"allowchat"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"quality"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"tooltip"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"name"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"pitch"))
               pControl->Enable (FALSE);
         }

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"quality")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pv->m_dwQuality)
               return TRUE;

            // else changed
            pv->m_dwQuality = dwVal;
            pv->m_fChanged = TRUE;
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

         if (!_wcsicmp(psz, L"allowvoicechat")) {
            pv->m_fAllowVoiceChat = p->pControl->AttribGetBOOL (Checked());
            pv->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"delverb")) {
            PCEscControl pControl = pPage->ControlFind (L"verblist");
            if (!pControl)
               return TRUE;
            DWORD dwSel = pControl->AttribGetInt (CurSel());

            if (dwSel >= pv->m_lWAVEBASECHOICE.Num()) {
               pPage->MBInformation (L"You must select an entry to delete.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this entry?"))
               return TRUE;

            // else, move
            pv->m_lWAVEBASECHOICE.Remove (dwSel);
            pv->m_fChanged = TRUE;
            FillListBoxWithFiles (pPage, L"verblist", &pv->m_lWAVEBASECHOICE, dwSel ? (dwSel-1) : 0);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addverb")) {
            WAVEBASECHOICE wbc;
            memset (&wbc, 0, sizeof(wbc));
            DWORD dwNeed;
            PCEscControl pControl;

            if (pControl = pPage->ControlFind (L"name"))
               pControl->AttribGet (Text(), wbc.szName, sizeof(wbc.szName), &dwNeed);
            if (!wbc.szName[0]) {
               pPage->MBWarning (L"You must type in a name.");
               return TRUE;
            }

            wbc.fPitch = DoubleFromControl (pPage, L"pitch");


            // open file dialog
            if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, FALSE, wbc.szFile))
               return TRUE;   // cancelled

            // add it
            pv->m_lWAVEBASECHOICE.Add (&wbc);
            pv->m_fChanged = TRUE;
            FillListBoxWithFiles (pPage, L"verblist", &pv->m_lWAVEBASECHOICE, pv->m_lWAVEBASECHOICE.Num()-1);

            pPage->MBSpeakInformation (L"Wave added.");

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice chat info resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CResVoiceChatInfo::Edit - Brings up UI for editing the resource.

inputs
   HWND        hWnd - To display from
   BOOL        fReadOnly - TRUE if read only
returns
   BOOL - TRUE if changed
*/
BOOL CResVoiceChatInfo::Edit (HWND hWnd, BOOL fReadOnly)
{
   m_fReadOnly = fReadOnly;
   m_fChanged = FALSE;

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESVOICECHATINFO, ResVoiceChatInfoPage, this);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   return m_fChanged;
}


