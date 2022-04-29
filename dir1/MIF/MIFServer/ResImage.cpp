/*************************************************************************************
ResImage.cpp - Code for the image resource.

begun 4/3/04 by Mike Rozak.
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

// OID - Information for OpenImageDialog
typedef struct {
   int         m_iVScroll;       // scroll
   WCHAR       szImage[256];     // image file
   DWORD       dwPos;            // position, 0 for no resize, 1 for stretch to fit,
                                 // 2 for fit entire in window, 3 for cover entire window
   BOOL        fReadOnly;        // set to TRUE if is read only file
   BOOL        fChanged;         // set to TRUE if changed
   LANGID      lid;              // langauge to use
   PCListFixed plPCCircumrealityHotSpot;       // where can modify hot spots, array of PCCircumrealityHotSpot
} OID, *POID;




/*************************************************************************
ResImagePage
*/
BOOL ResImagePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   POID poid = (POID)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (poid->m_iVScroll > 0) {
            pPage->VScroll (poid->m_iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            poid->m_iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;

         // disable?
         DWORD i;
         WCHAR szTemp[64];
         if (poid->fReadOnly) {
            for (i = 0; i < 4; i++) {
               swprintf (szTemp, L"s%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (FALSE);
            }

            pControl = pPage->ControlFind (L"langid");
            if (pControl)
               pControl->Enable (FALSE);
         }

         // set the current one
         swprintf (szTemp, L"s%d", (int)poid->dwPos);
         pControl = pPage->ControlFind (szTemp);
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         HotSpotInitPage (pPage, poid->plPCCircumrealityHotSpot, FALSE, 0, 0);

#if 0 // replaced by HotSpotInitPage
         // fill in the controls
         PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)poid->plPCCircumrealityHotSpot->Get(0);
         CListFixed lCD;
         lCD.Init (sizeof(CONTROLIMAGEDRAGRECT));
         CONTROLIMAGEDRAGRECT cd;
         memset (&cd, 0, sizeof(cd));
         for (i = 0; i < poid->plPCCircumrealityHotSpot->Num(); i++, pph++) {
            swprintf (szTemp, L"hotmsg%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSet (Text(), pph[0]->m_ps->Get());

            swprintf (szTemp, L"hotcursor%d", (int)i);
            ComboBoxSet (pPage, szTemp, pph[0]->m_dwCursor);

            // add to list that will send to bitmap
            cd.cColor = HotSpotColor(i);
            cd.fModulo = FALSE;
            cd.rPos = pph[0]->m_rPosn;
            lCD.Add (&cd);
         } // i

         // send message to image to show hotspots
         ESCMIMAGERECTSET is;
         memset (&is, 0, sizeof(is));
         is.dwNum = lCD.Num();
         is.pRect = (PCONTROLIMAGEDRAGRECT)lCD.Get(0);
         pControl = pPage->ControlFind (L"image");
         if (pControl)
            pControl->Message (ESCM_IMAGERECTSET, &is);
#endif // 0

         // set the language for the hotspots
         MIFLLangComboBoxSet (pPage, L"langid", poid->lid,gpMIFLProj);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (HotSpotComboBoxSelChanged (pPage, p, poid->plPCCircumrealityHotSpot, &poid->fChanged))
            return TRUE;
#if 0 // replaced by HotSpotComboBoxSelChanged
         PWSTR pszHotCursor = L"hotcursor";
         DWORD dwHotCursorLen = wcslen(pszHotCursor);
         if (!wcsncmp(psz, pszHotCursor, dwHotCursorLen)) {
            DWORD dwIndex = _wtoi(psz + dwHotCursorLen);
            PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)poid->plPCCircumrealityHotSpot->Get(dwIndex);
            if (!pph)
               break;
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pph[0]->m_dwCursor)
               return TRUE;

            // else changed
            pph[0]->m_dwCursor = dwVal;
            poid->fChanged = TRUE;
            return TRUE;
         }
#endif // 0
         else if (!_wcsicmp(psz, L"langid")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            LANGID *padw = (LANGID*)gpMIFLProj->m_lLANGID.Get(dwVal);
            dwVal = padw ? padw[0] : poid->lid;
            if (dwVal == poid->lid)
               return TRUE;

            // else changed
            poid->lid = (LANGID)dwVal;
            poid->fChanged = TRUE;
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
          
         if (HotSpotEditChanged (pPage, p, poid->plPCCircumrealityHotSpot, &poid->fChanged))
            return TRUE;

#if 0 // replaced by HotSpotEditChanged
         PWSTR pszHotMsg = L"hotmsg";
         DWORD dwHotMsgLen = wcslen(pszHotMsg);
         if (!wcsncmp(psz, pszHotMsg, dwHotMsgLen)) {
            DWORD dwIndex = _wtoi(psz + dwHotMsgLen);
            PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)poid->plPCCircumrealityHotSpot->Get(dwIndex);
            if (!pph)
               break;

            WCHAR szTemp[512];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
            pph[0]->m_ps->Set(szTemp);
            poid->fChanged = TRUE;
            return TRUE;
         }
#endif // 0

      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;


         if (HotSpotButtonPress (pPage, p, poid->plPCCircumrealityHotSpot, &poid->fChanged))
            return TRUE;

#if 0 // replace by HotSpotButtonPress
         PWSTR pszHotDel = L"hotdel";
         DWORD dwHotDelLen = wcslen(pszHotDel);
         if (!wcsncmp(psz, pszHotDel, dwHotDelLen)) {
            DWORD dwIndex = _wtoi(psz + dwHotDelLen);
            PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)poid->plPCCircumrealityHotSpot->Get(dwIndex);
            if (!pph)
               break;

            // delete
            delete pph[0];
            poid->plPCCircumrealityHotSpot->Remove (dwIndex);
            poid->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
#endif // 0

         if (!_wcsicmp(psz, L"open")) {
            if (!OpenImageDialog (pPage->m_pWindow->m_hWnd, poid->szImage, sizeof(poid->szImage)/sizeof(WCHAR), FALSE))
               return TRUE;

            poid->fChanged = TRUE;
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L's') && (psz[1] >= '0') && (psz[1] <= '3') && !psz[2]) {
            poid->fChanged = TRUE;
            poid->dwPos = psz[1] - L'0';
            return TRUE;
         }
      }
      break;

   case ESCN_IMAGEDRAGGED:
      {
         if (poid->fReadOnly)
            return TRUE;   // cant change

         PESCNIMAGEDRAGGED p = (PESCNIMAGEDRAGGED)pParam;

         HotSpotImageDragged (pPage, p, poid->plPCCircumrealityHotSpot, 0, 0, &poid->fChanged);

#if 0 // replaced by HotSpotImageDragged
         PCCircumrealityHotSpot pNew = new CCircumrealityHotSpot;
         if (!pNew)
            return TRUE;
         pNew->m_rPosn = p->rPos;
         if (!pNew->m_ps) {
            pNew->m_ps = new CMIFLVarString;
            if (!pNew->m_ps)
               return TRUE;
         }

         poid->plPCCircumrealityHotSpot->Add (&pNew);
         poid->fChanged = TRUE;

         // refresh
         pPage->Exit (RedoSamePage());
#endif // 0
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Image resource";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OPENBUTTON")) {
            p->pszSubString = poid->fReadOnly ? L"" :
               L"<tr><td><xChoiceButton name=open><bold>Select image file</bold><br/>"
               L"Select this to select the image file to use, either .jpg or .bmp."
               L"(.jpg are recommended because they're much smaller than .bmp. You can use"
               L"your favorite paint program to convert from .bmp to .jpg.)"
               L"</xChoiceButton></td></tr>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IMAGEVIEW")) {
            MemZero (&gMemTemp);

            if (poid->szImage[0]) {
               MemCat (&gMemTemp, L"<tr><td><p align=center><imagedrag name=image clickmode=2 width=90% border=2 file=\"");
               MemCatSanitize (&gMemTemp, poid->szImage);
               MemCat (&gMemTemp, L"\"/><br/>");
               MemCatSanitize (&gMemTemp, poid->szImage);
               MemCat (&gMemTemp, L"</p></td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (HotSpotSubstitution (pPage, p, poid->plPCCircumrealityHotSpot, poid->fReadOnly))
            return TRUE;

#if 0 // replaced by HotSpotSubstitution
         else if (!_wcsicmp(p->pszSubName, L"HOTSPOTS")) {
            MemZero (&gMemTemp);

            DWORD i;
            PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)poid->plPCCircumrealityHotSpot->Get(0);
            WCHAR szColor[32];
            for (i = 0; i < poid->plPCCircumrealityHotSpot->Num(); i++, pph++) {
               ColorToAttrib (szColor, HotSpotColor(i));

               MemCat (&gMemTemp, L"<tr><td bgcolor=");
               MemCat (&gMemTemp, szColor);
               MemCat (&gMemTemp, L"><bold>Message to send for pixels (");
               MemCat (&gMemTemp, pph[0]->m_rPosn.left);
               MemCat (&gMemTemp, L", ");
               MemCat (&gMemTemp, pph[0]->m_rPosn.top);
               MemCat (&gMemTemp, L") to (");
               MemCat (&gMemTemp, pph[0]->m_rPosn.right);
               MemCat (&gMemTemp, L", ");
               MemCat (&gMemTemp, pph[0]->m_rPosn.bottom);
               MemCat (&gMemTemp, L")</bold> - If the user "
			               L"clicks on this range then send the following message. The drop-down listbox "
			               L"lets you control the icon over the region."
		                  L"</td>"
		                  L"<td bgcolor=");
               MemCat (&gMemTemp, szColor);
               MemCat (&gMemTemp, L"><bold><edit width=100% maxchars=256 ");
               if (poid->fReadOnly)
                  MemCat (&gMemTemp, L"enabled=false ");
               MemCat (&gMemTemp, L"name=hotmsg");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold><br/>"
			               L"<xComboCursor ");
               if (poid->fReadOnly)
                  MemCat (&gMemTemp, L"enabled=false ");
               MemCat (&gMemTemp, L"name=hotcursor");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/><br/>");
			      MemCat (&gMemTemp, L"<button ");
               if (poid->fReadOnly)
                  MemCat (&gMemTemp, L"enabled=false ");
               MemCat (&gMemTemp, L"name=hotdel");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Remove this</bold></button>"
		                  L"</td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
#endif // 0
      }
      break;

   }; // dwMessage

   return FALSE;
}


static PWSTR gpszFile = L"file";
static PWSTR gpszScale = L"scale";
static PWSTR gpszNone = L"none";
static PWSTR gpszStretchToFit = L"stretchtofit";
static PWSTR gpszScaleToFit = L"scaletofit";
static PWSTR gpszScaleToCover = L"scaletocover";
static PWSTR gpszHotSpot = L"HotSpot";
static PWSTR gpszLangID = L"LangID";

/*************************************************************************
ResImageEdit - Modify a resource image. Uses standard API from mifl.h, ResourceEdit.
*/
PCMMLNode2 ResImageEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly)
{
   PCMMLNode2 pRet = NULL;
   OID oid;
   CListFixed lHotSpot;
   CTransition Transition;
   lHotSpot.Init (sizeof(PCCircumrealityHotSpot));
   memset (&oid, 0, sizeof(oid));
   oid.fReadOnly = fReadOnly;
   oid.dwPos = 2;
   oid.plPCCircumrealityHotSpot = &lHotSpot;
   oid.lid = lid;
   PWSTR psz;
   psz = pIn ? MMLValueGet (pIn, gpszFile) : NULL;
   if (psz)
      wcscpy (oid.szImage, psz);
   psz = pIn ? MMLValueGet (pIn, gpszScale) : NULL;
   if (!psz)
      oid.dwPos = 2;
   else if (!_wcsicmp(psz, gpszStretchToFit))
      oid.dwPos = 1;
   else if (!_wcsicmp(psz, gpszNone))
      oid.dwPos = 0;
   else if (!_wcsicmp(psz, gpszScaleToCover))
      oid.dwPos = 3;

   // get the transition
   PCMMLNode2 pSub = NULL;
   pIn->ContentEnum (pIn->ContentFind(CircumrealityTransition()), &psz, &pSub);
   Transition.MMLFrom (pSub);

   // fill in the hot spots
   DWORD i;
   for (i = 0; i < pIn->ContentNum(); i++) {
      pSub = NULL;
      pIn->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszHotSpot)) {
         PCCircumrealityHotSpot pNew = new CCircumrealityHotSpot;
         if (!pNew)
            continue;
         pNew->MMLFrom (pSub, lid);
         oid.lid = pNew->m_lid; // in case different

         // add it
         lHotSpot.Add (&pNew);
         continue;
      }
   } // i

   // create the window
   RECT r;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, IDR_MMLRESIMAGE, ResImagePage, &oid);
   oid.m_iVScroll = Window.m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;
   if (psz && !_wcsicmp(psz, L"TransitionUI")) {
      // load in the image
      CImage Image;
      if (!Image.Init (oid.szImage))
         goto redo;
      HDC hDC = GetDC (Window.m_hWnd);
      HBITMAP hBmp = Image.ToBitmap (hDC);
      ReleaseDC (Window.m_hWnd, hDC);

      BOOL fChanged;
      BOOL fRet = Transition.Dialog (&Window, hBmp, (fp)Image.Width() / (fp)Image.Height(),
         FALSE, fReadOnly, &fChanged);
      DeleteObject (hBmp);
      if (fChanged)
         oid.fChanged = TRUE;
      if (fRet)
         goto redo;
      // else fall through
   }

   if (!oid.fChanged)
      goto done;

   // create new MML
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      goto done;
   pNode->NameSet (CircumrealityImage());
   if (oid.szImage[0])
      MMLValueSet (pNode, gpszFile, oid.szImage);
   switch (oid.dwPos) {
   case 0:
      MMLValueSet (pNode, gpszScale, gpszNone);
      break;
   case 1:
      MMLValueSet (pNode, gpszScale, gpszStretchToFit);
      break;
   case 2:
      MMLValueSet (pNode, gpszScale, gpszScaleToFit);
      break;
   case 3:
      MMLValueSet (pNode, gpszScale, gpszScaleToCover);
      break;
   }

   // write out the hot spots
   PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)lHotSpot.Get(0);
   for (i = 0; i < lHotSpot.Num(); i++,pph++) {
      pph[0]->m_lid = oid.lid;
         // NOTE: Setting the default language ID for all of them
      PCMMLNode2 pSub = pph[0]->MMLTo ();
      if (!pSub)
         continue;
      pNode->ContentAdd (pSub);
      pSub->NameSet (gpszHotSpot);

   } // i

   // write out the transition
   pSub = Transition.MMLTo ();
   if (pSub)
      pNode->ContentAdd (pSub);

   pRet = pNode;

done:
   // free up all the hot spots
   pph = (PCCircumrealityHotSpot*)lHotSpot.Get(0);
   for (i = 0; i < lHotSpot.Num(); i++,pph++)
      delete pph[0];

   return pRet;
}

