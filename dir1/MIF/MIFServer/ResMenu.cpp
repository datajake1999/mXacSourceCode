/*************************************************************************************
ResMenu.cpp - Code for the MML general menu resource.

begun 30/5/04 by Mike Rozak.
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

// OMD - Information for OpenTextDialog
typedef struct {
   PCListVariable plShow;        // list of strings to show
   PCListVariable plExtraText;   // extra text
   PCListVariable plDo;          // list of strins to do (when menu item pressed)
   CPoint      pWindowLoc;       // window location
   LANGID      lid;              // language to send the commands as
   DWORD       dwDefault;        // which menu item (indexed from 0) is the default if timer
                                 // goes out, or -1 if none
   BOOL        fExclusive;       // if TRUE then user will only be able to choose from this menu
   fp          fTimeOut;         // number of seconds before timer goes off, 0 if none

   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
   int         iVScroll;         // amount to scroll
} OMD, *POMD;


static PWSTR gpszReadOnly = L" readonly=true ";

/*************************************************************************
ResMenuPage
*/
BOOL ResMenuPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POMD pomd = (POMD)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (pomd->iVScroll > 0) {
            pPage->VScroll (pomd->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            pomd->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;

         // write out menu
         DWORD j;
         WCHAR szTemp[64];
         for (j = 0; j < pomd->plShow->Num(); j++) {
            swprintf (szTemp, L"hotmenushow%d", (int)j);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), (PWSTR)pomd->plShow->Get(j));
         } // j

         for (j = 0; j < pomd->plExtraText->Num(); j++) {
            swprintf (szTemp, L"hotmenuextratext%d", (int)j);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), (PWSTR)pomd->plExtraText->Get(j));
         } // j

         // write out do
         for (j = 0; j < pomd->plDo->Num(); j++) {
            swprintf (szTemp, L"hotmenudo%d", (int)j);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), (PWSTR)pomd->plDo->Get(j));
         } // j


         DWORD i;
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"WindowLoc%d", (int)i);
            DoubleToControl (pPage, szTemp, pomd->pWindowLoc.p[i]);

            if (!pomd->fReadOnly)
               continue;
            if (pControl = pPage->ControlFind (szTemp))
               pControl->Enable (FALSE);
         } // i
         
         if (pomd->fReadOnly) {
            if (pControl = pPage->ControlFind (L"langid"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"timeout"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"default"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"exclusive"))
               pControl->Enable (FALSE);
         }

         // set the language for the hotspots
         MIFLLangComboBoxSet (pPage, L"langid", pomd->lid, gpMIFLProj);
         
         // text
         DoubleToControl (pPage, L"timeout", pomd->fTimeOut);
         DoubleToControl (pPage, L"default", (pomd->dwDefault == -1) ? 1 : (pomd->dwDefault + 1));


         if (pControl = pPage->ControlFind (L"exclusive"))
            pControl->AttribSetBOOL (Checked(), pomd->fExclusive);

      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"langid")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            LANGID *padw = (LANGID*)gpMIFLProj->m_lLANGID.Get(dwVal);
            dwVal = padw ? padw[0] : pomd->lid;
            if (dwVal == pomd->lid)
               return TRUE;

            // else changed
            pomd->lid = (LANGID)dwVal;
            pomd->fChanged = TRUE;
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         // just get values
         pomd->fTimeOut = DoubleFromControl (pPage, L"timeout");
         pomd->dwDefault = (DWORD) DoubleFromControl (pPage, L"default") - 1;
         pomd->fChanged = TRUE;

         // just get values
         DWORD i;
         WCHAR szTemp[64];
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"WindowLoc%d", (int)i);
            pomd->pWindowLoc.p[i] = DoubleFromControl (pPage, szTemp);
         }

         PWSTR pszHotMenuShow = L"hotmenushow", pszHotMenuDo = L"hotmenudo",
            pszHotMenuExtraText = L"hotmenuextratext";
         DWORD dwHotMenuShowLen = (DWORD)wcslen(pszHotMenuShow), dwHotMenuDoLen = (DWORD)wcslen(pszHotMenuDo),
            dwHotMenuExtraTextLen = (DWORD)wcslen(pszHotMenuExtraText);

         if (!wcsncmp(psz, pszHotMenuShow, dwHotMenuShowLen)) {
            DWORD dwItem = _wtoi(psz + dwHotMenuShowLen);

            // add blanks to fill in
            PWSTR pszBlank = L"";
            while (pomd->plShow->Num() <= dwItem)
               pomd->plShow->Add (pszBlank, (wcslen(pszBlank)+1)*sizeof(WCHAR));

            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            pomd->plShow->Set (dwItem, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
            pomd->fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsncmp(psz, pszHotMenuExtraText, dwHotMenuExtraTextLen)) {
            DWORD dwItem = _wtoi(psz + dwHotMenuExtraTextLen);

            // add blanks to fill in
            PWSTR pszBlank = L"";
            while (pomd->plExtraText->Num() <= dwItem)
               pomd->plExtraText->Add (pszBlank, (wcslen(pszBlank)+1)*sizeof(WCHAR));

            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            pomd->plExtraText->Set (dwItem, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
            pomd->fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsncmp(psz, pszHotMenuDo, dwHotMenuDoLen)) {
            DWORD dwItem = _wtoi(psz + dwHotMenuDoLen);

            // add blanks to fill in
            PWSTR pszBlank = L"";
            while (pomd->plDo->Num() <= dwItem)
               pomd->plDo->Add (pszBlank, (wcslen(pszBlank)+1)*sizeof(WCHAR));

            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            pomd->plDo->Set (dwItem, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
            pomd->fChanged = TRUE;
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

         if (!_wcsicmp(psz, L"Exclusive")) {
            pomd->fExclusive = p->pControl->AttribGetBOOL (Checked());
            pomd->fChanged = TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"General menu resource";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENUITEMS")) {
            MemZero (&gMemTemp);

            DWORD dwNum = max(pomd->plDo->Num(), pomd->plShow->Num())+1;
            DWORD j;
            dwNum = max(dwNum, 4);

            for (j = 0; j < dwNum; j++) {
               // show
               MemCat (&gMemTemp,
                  L"<tr><td>"
                  L"<edit width=100% maxchars=256 ");
               if (pomd->fReadOnly)
                  MemCat (&gMemTemp, gpszReadOnly);
               MemCat (&gMemTemp, L"name=hotmenushow");
               MemCat (&gMemTemp, (int)j);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</td>");

               // extra text
               MemCat (&gMemTemp,
                  L"<td>"
                  L"<edit width=100% maxchars=256 ");
               if (pomd->fReadOnly)
                  MemCat (&gMemTemp, gpszReadOnly);
               MemCat (&gMemTemp, L"name=hotmenuextratext");
               MemCat (&gMemTemp, (int)j);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</td>");

               // do
               MemCat (&gMemTemp,
                  L"<td>"
                  L"<edit width=100% maxchars=256 ");
               if (pomd->fReadOnly)
                  MemCat (&gMemTemp, gpszReadOnly);
               MemCat (&gMemTemp, L"name=hotmenudo");
               MemCat (&gMemTemp, (int)j);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</td></tr>");

            } // j

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


static PWSTR gpszExclusive = L"Exclusive";
static PWSTR gpszTimeOut = L"TimeOut";
static PWSTR gpszWindowLoc = L"WindowLoc";

/*************************************************************************
ResMenuEdit - Modify a resource Text. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResMenuEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly)
{
   PCMMLNode2 pRet = NULL;
   CListVariable lShow, lDo, lExtraText;
   OMD omd;
   memset (&omd, 0, sizeof(omd));
   omd.fReadOnly = fReadOnly;
   omd.dwDefault = -1;
   omd.lid = lid;
   omd.plDo = &lDo;
   omd.plShow = &lShow;
   omd.plExtraText = &lExtraText;
   omd.pWindowLoc.Zero4();

   if (pIn) {
      omd.fExclusive = (BOOL) MMLValueGetInt (pIn, gpszExclusive, 0);
      omd.fTimeOut = MMLValueGetDouble (pIn, gpszTimeOut, 0);
      MMLFromContextMenu (pIn, &lShow, &lExtraText, &lDo, &omd.lid, &omd.dwDefault);
      MMLValueGetPoint (pIn, gpszWindowLoc, &omd.pWindowLoc);
   }


   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESMENU, ResMenuPage, &omd);
   omd.iVScroll = Window.m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   if (!omd.fChanged)
      goto done;

   // create new MML
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      goto done;
   pNode->NameSet (CircumrealityGeneralMenu());
   if (omd.fExclusive)
      MMLValueSet (pNode, gpszExclusive, (int)omd.fExclusive);
   if (omd.fTimeOut > 0)
      MMLValueSet (pNode, gpszTimeOut, omd.fTimeOut);
   MMLToContextMenu (pNode, &lShow, &lExtraText, &lDo, omd.lid, omd.dwDefault);
   if ((omd.pWindowLoc.p[0] != omd.pWindowLoc.p[1]) && (omd.pWindowLoc.p[2] != omd.pWindowLoc.p[3]))
      MMLValueSet (pNode, gpszWindowLoc, &omd.pWindowLoc);

   pRet = pNode;

done:
   return pRet;
}

