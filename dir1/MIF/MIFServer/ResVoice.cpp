/*************************************************************************************
ResVoice.cpp - Code for the Voice resource.

begun 25/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"

// OVD - Information for OpenVoiceDialog
typedef struct {
   WCHAR       szFile[256];     // Voice file
   WCHAR       szName[64];       // name of speaker
   fp          fVolL;            // left vol
   fp          fVolR;            // right vol
   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
   DWORD       dwSubVoice;       // sub-voice
   DWORD       dwMixVoice;       // mixed voice
   DWORD       dwMixPros;        // mixed prosody
   DWORD       dwMixPron;        // mixed pronunciation
} OVD, *POVD;




/*************************************************************************
ResVoicePage
*/
BOOL ResVoicePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POVD povd = (POVD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // disable?
         if (povd->fReadOnly) {
            if (pControl = pPage->ControlFind (L"open"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"name"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"voll"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"volr"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"subvoice"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"mixvoice"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"mixpros"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"mixpron"))
               pControl->Enable (FALSE);
         }

         pControl = pPage->ControlFind (L"file");
         if (pControl)
            pControl->AttribSet (Text(), povd->szFile);

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), povd->szName);

         DoubleToControl (pPage, L"voll", povd->fVolL);
         DoubleToControl (pPage, L"volr", povd->fVolR);

         if (povd->dwSubVoice)
            DoubleToControl (pPage, L"subvoice", povd->dwSubVoice);
         if (povd->dwMixVoice)
            DoubleToControl (pPage, L"mixvoice", povd->dwMixVoice);
         if (povd->dwMixPros)
            DoubleToControl (pPage, L"mixpros", povd->dwMixPros);
         if (povd->dwMixPron)
            DoubleToControl (pPage, L"mixpron", povd->dwMixPron);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // just get all
         PCEscControl pControl;
         DWORD dwNeed;
         pControl = pPage->ControlFind (L"name");
         povd->szName[0] = 0;
         if (pControl)
            pControl->AttribGet (Text(), povd->szName, sizeof(povd->szName), &dwNeed);

         povd->fVolL = DoubleFromControl (pPage, L"voll");
         povd->fVolR = DoubleFromControl (pPage, L"volr");
         povd->dwSubVoice = (DWORD) DoubleFromControl (pPage, L"subvoice");
         povd->dwMixVoice = (DWORD) DoubleFromControl (pPage, L"mixvoice");
         povd->dwMixPros = (DWORD) DoubleFromControl (pPage, L"mixpros");
         povd->dwMixPron = (DWORD) DoubleFromControl (pPage, L"mixpron");
         povd->fChanged = TRUE;
         return TRUE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"open")) {
            if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, povd->szFile,
               sizeof(povd->szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            povd->fChanged = TRUE;
            PCEscControl pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribSet (Text(), povd->szFile);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Voice resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


static PWSTR gpszFile = L"file";
static PWSTR gpszName = L"name";
static PWSTR gpszVolL = L"voll";
static PWSTR gpszVolR = L"volr";

/*************************************************************************
ResVoiceEdit - Modify a resource Voice. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResVoiceEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly)
{
   PCMMLNode2 pRet = NULL;
   OVD ovd;
   memset (&ovd, 0, sizeof(ovd));
   ovd.fReadOnly = fReadOnly;

   PWSTR psz;
   psz = pIn ? MMLValueGet (pIn, gpszFile) : NULL;
   if (psz)
      wcscpy (ovd.szFile, psz);
   psz = pIn ? MMLValueGet (pIn, gpszName) : NULL;
   if (psz)
      wcscpy (ovd.szName, psz);
   if (pIn) {
      ovd.fVolL = MMLValueGetDouble (pIn, gpszVolL, 1);
      ovd.fVolR = MMLValueGetDouble (pIn, gpszVolR, 1);
   }
   else
      ovd.fVolL = ovd.fVolR = 1;

   // find the subvoice
   PCMMLNode2 pSub;
   DWORD i;
   if (pIn) for (i = 0; i < pIn->ContentNum(); i++) {
      pSub = NULL;
      pIn->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, L"subvoice")) {
         int iVal;
         if (pSub->AttribGetInt (L"subvoice", &iVal))
            ovd.dwSubVoice = (DWORD)iVal;
         if (pSub->AttribGetInt (L"mixvoice", &iVal))
            ovd.dwMixVoice = (DWORD)iVal;
         if (pSub->AttribGetInt (L"mixpros", &iVal))
            ovd.dwMixPros = (DWORD)iVal;
         if (pSub->AttribGetInt (L"mixpron", &iVal))
            ovd.dwMixPron = (DWORD)iVal;
         break;
      }
   } // i

   // create the window
   RECT r;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESVOICE, ResVoicePage, &ovd);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   if (!ovd.fChanged)
      goto done;

   // create new MML
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      goto done;
   pNode->NameSet (CircumrealityVoice());
   if (ovd.szFile[0])
      MMLValueSet (pNode, gpszFile, ovd.szFile);
   if (ovd.szName[0])
      MMLValueSet (pNode, gpszName, ovd.szName);
   if (ovd.fVolL != 1)
      MMLValueSet (pNode, gpszVolL, ovd.fVolL);
   if (ovd.fVolR != 1)
      MMLValueSet (pNode, gpszVolR, ovd.fVolR);
   if (ovd.dwSubVoice || ovd.dwMixVoice || ovd.dwMixPros || ovd.dwMixPron) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      pSub->NameSet (L"subvoice");

      if (ovd.dwSubVoice)
         pSub->AttribSetInt (L"subvoice", (int)ovd.dwSubVoice);
      else {
         if (ovd.dwMixVoice)
            pSub->AttribSetInt (L"mixvoice", (int)ovd.dwMixVoice);
         if (ovd.dwMixPros)
            pSub->AttribSetInt (L"mixpros", (int)ovd.dwMixPros);
         if (ovd.dwMixPron)
            pSub->AttribSetInt (L"mixpron", (int)ovd.dwMixPron);
      }
   }

   pRet = pNode;

done:
   return pRet;
}

