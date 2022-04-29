/*************************************************************************************
CMIFHotSpot.cpp - Code for UI to modify a scene to be rendered, and for rendering it.

begun 19/5/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"


static PWSTR gpszEnabledFalse = L" enabled=false ";

/*************************************************************************************
CCircumrealityHotSpot::Constructor and destructor
*/
CCircumrealityHotSpot::CCircumrealityHotSpot (void)
{
   memset (&m_rPosn, 0, sizeof(m_rPosn));
   m_dwCursor = 0;
   m_ps = NULL;
   m_lid = 0;
}

CCircumrealityHotSpot::~CCircumrealityHotSpot (void)
{
   if (m_ps)
      m_ps->Release();
}

static PWSTR gpszHotSpot = L"HotSpot";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszCursor = L"Cursor";
static PWSTR gpszMessage = L"Message";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszMenu = L"Menu";
static PWSTR gpszShow = L"Show";
static PWSTR gpszDo = L"Do";
static PWSTR gpszExtraText = L"ExtraText";
static PWSTR gpszDefault = L"Default";

/*************************************************************************************
MMLToContextMenu - This takes menu information and writes it out to the pNode.

inputs
   PCMMLNode2         pNode - Node WRITTEN out to
   PCListVariable    plShow - List of show strings
   PCListVariable    plExtraText - Extra text displayed
   PCListVariable    plDo - List of Do strings
   LANGID            lid - Language ID. If 0 then no known language
   DWORD             dwDefault - Which menu item (indexed from 0) is default.
                        If -1 then no known default.
*/
void MMLToContextMenu (PCMMLNode2 pNode, PCListVariable plShow, PCListVariable plExtraText, PCListVariable plDo,
                       LANGID lid, DWORD dwDefault)
{
   // write out all the commands
   DWORD dwNum = (max(plShow->Num(), plDo->Num()), plExtraText->Num());
   DWORD i;
   PWSTR psz1, psz2, psz3;
   for (i = 0; i < dwNum; i++) {
      psz1 = (PWSTR) plShow->Get(i);
      psz2 = (PWSTR) plDo->Get(i);
      psz3 = (PWSTR) plExtraText->Get(i);

      if (!(psz1 && psz1[0]) && !(psz2 && psz2[0]))   // NOTE: Dont care about psz3
         continue;   // both blank, so ignore

      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMenu);

      if (psz1 && psz1[0])
         MMLValueSet (pSub, gpszShow, psz1);
      if (psz2 && psz2[0])
         MMLValueSet (pSub, gpszDo, psz2);
      if (psz3 && psz3[0])
         MMLValueSet (pSub, gpszExtraText, psz3);

      if (dwDefault == i)
         MMLValueSet (pSub, gpszDefault, 1);
   } // i

   if (lid)
      MMLValueSet (pNode, gpszLangID, (int)lid);
}

/*************************************************************************************
MMLFromContextMenu - This takes a pNode of type <ContextMenu> and parses it to fill
in the lists for the menu items and commands when click on.

inputs
   PCMMLNode2         pNode - Node
   PCListVariable    plShow - List to show, first clear, then added to
   PCListVariable    plExtraText - List of extra text show, first clear, then added to
   PCListVariable    plDo - List to do, first cleared, then added to
   LANGID            *plid - Filled in with the language ID, can be NULL.
                        If using, have pre-filled with a default language
   DWORD             *pdwDefault - Filled with the menu index that's marked
                        as the default one, or -1 if none are. This
                        can be NULL.
returns
   none
*/
void MMLFromContextMenu (PCMMLNode2 pNode, PCListVariable plShow, PCListVariable plExtraText, PCListVariable plDo,
                         LANGID *plid, DWORD *pdwDefault)
{
   plShow->Clear();
   plExtraText->Clear();
   plDo->Clear();

   if (plid)
      *plid = (LANGID) MMLValueGetInt (pNode, gpszLangID, (int) *plid);
   if (pdwDefault)
      *pdwDefault = -1;

   PCMMLNode2 pSub;
   DWORD i;
   PWSTR pszEmpty = L"";
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (psz && !_wcsicmp(psz, gpszMenu)) {
         // found a menu item

         // show
         psz = MMLValueGet (pSub, gpszShow);
         if (!psz)
            psz = pszEmpty;
         plShow->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

         // extra text
         psz = MMLValueGet (pSub, gpszExtraText);
         if (!psz)
            psz = pszEmpty;
         plExtraText->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

         // do
         psz = MMLValueGet (pSub, gpszDo);
         if (!psz)
            psz = pszEmpty;
         plDo->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

         if (pdwDefault && MMLValueGetInt (pSub, gpszDefault, 0))
            *pdwDefault = plShow->Num()-1;
      }
   } // i
}

/*************************************************************************************
CCircumrealityHotSpot::MMLTo - Standard API
*/
PCMMLNode2 CCircumrealityHotSpot::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszHotSpot);

   // BUGFIX - Store all in one
   CPoint p;
   p.p[0] = m_rPosn.left;
   p.p[1] = m_rPosn.right;
   p.p[2] = m_rPosn.top;
   p.p[3] = m_rPosn.bottom;
   MMLValueSet (pNode, gpszLoc, &p);
   //MMLValueSet (pNode, gpszLeft, m_rPosn.left);
   //MMLValueSet (pNode, gpszRight, m_rPosn.right);
   //MMLValueSet (pNode, gpszTop, m_rPosn.top);
   //MMLValueSet (pNode, gpszBottom, m_rPosn.bottom);
   MMLValueSet (pNode, gpszCursor, (int) m_dwCursor);
   MMLValueSet (pNode, gpszLangID, (int) m_lid);

   if (m_dwCursor == 10)
      MMLToContextMenu (pNode, &m_lMenuShow, &m_lMenuExtraText, &m_lMenuDo, 0, -1);
   else {
      // just one command

      PWSTR psz = m_ps->Get();
      if (psz[0])
         MMLValueSet (pNode, gpszMessage, psz);
   }

   return pNode;
}

/*************************************************************************************
CCircumrealityHotSpot::MMLFrom - Standard API

inputs
   LANGID         lid - Language to default to
*/
BOOL CCircumrealityHotSpot::MMLFrom (PCMMLNode2 pNode, LANGID lid)
{
   m_lMenuShow.Clear();
   m_lMenuExtraText.Clear();
   m_lMenuDo.Clear();

   m_dwCursor = (DWORD)MMLValueGetInt (pNode, gpszCursor, 0);

   // BUGFIX - Store all in one
   CPoint p;
   p.Zero4();
   MMLValueGetPoint (pNode, gpszLoc, &p);
   m_rPosn.left = (int)p.p[0];
   m_rPosn.right = (int)p.p[1];
   m_rPosn.top = (int)p.p[2];
   m_rPosn.bottom = (int)p.p[3];
   //m_rPosn.left = MMLValueGetInt (pNode, gpszLeft, 0);
   //m_rPosn.right = MMLValueGetInt (pNode, gpszRight, 0);
   //m_rPosn.top = MMLValueGetInt (pNode, gpszTop, 0);
   //m_rPosn.bottom = MMLValueGetInt (pNode, gpszBottom, 0);
   m_lid = (LANGID) MMLValueGetInt (pNode, gpszLangID, lid);
   m_lid = m_lid; // in case different

   if (!m_ps) {
      m_ps = new CMIFLVarString;
      if (!m_ps)
         return FALSE;
   }
   PWSTR psz = MMLValueGet (pNode, gpszMessage);
   m_ps->Set (psz ? psz : L"");

   // if it's a menu then load
   if (m_dwCursor == 10)
      MMLFromContextMenu (pNode, &m_lMenuShow, &m_lMenuExtraText, &m_lMenuDo, NULL, NULL);

   return TRUE;
}


/*************************************************************************************
HotSpotSubstitution - Handles ESCM_SUBSTITUTION for hotspots. Looks for
"HOTSPOTS"

inputs
   PCEscPage         pPage - Page
   PESCMSUBSTITUTION p - Info passed into substitution
   PCListFixed       plPCCircumrealityHotSpot - List of hot spots
   BOOL              fReadOnly - If TRUE then readonly flag is set
returns
   BOOL - TRUE if captured.
*/
DLLEXPORT BOOL HotSpotSubstitution (PCEscPage pPage, PESCMSUBSTITUTION p,
                                    PCListFixed plCPCircumrealityHotSpot, BOOL fReadOnly)
{
   if (!_wcsicmp(p->pszSubName, L"HOTSPOTS")) {
      MemZero (&gMemTemp);

      DWORD i, j;
      PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plCPCircumrealityHotSpot->Get(0);
      WCHAR szColor[32];
      for (i = 0; i < plCPCircumrealityHotSpot->Num(); i++, pph++) {
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
         MemCat (&gMemTemp, L"><bold>");

         if (pph[0]->m_dwCursor == 10) {
            // menu
            MemCat (&gMemTemp,
               L"<table width=100%>"
               L"<tr>"
                  L"<td>Displayed on menu</td>"
                  L"<td>Extra text</td>"
                  L"<td>Message sent</td>"
               L"</tr>");

            DWORD dwNum = max(max(pph[0]->m_lMenuDo.Num(), pph[0]->m_lMenuShow.Num()),pph[0]->m_lMenuExtraText.Num())+2;
            dwNum = max(dwNum, 6);

            for (j = 0; j < dwNum; j++) {
               // show
               MemCat (&gMemTemp,
                  L"<tr><td>"
                  L"<edit width=100% maxchars=256 ");
               if (fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=hotmenushow");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L":");
               MemCat (&gMemTemp, (int)j);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</td>");

               // extra text
               MemCat (&gMemTemp,
                  L"<td>"
                  L"<edit width=100% maxchars=256 ");
               if (fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=hotmenuextratext");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L":");
               MemCat (&gMemTemp, (int)j);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</td>");

               // do
               MemCat (&gMemTemp,
                  L"<td>"
                  L"<edit width=100% maxchars=256 ");
               if (fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=hotmenudo");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L":");
               MemCat (&gMemTemp, (int)j);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</td></tr>");

            } // j

            MemCat (&gMemTemp,
               L"</table>");
         }
         else {
            // single entry

            MemCat (&gMemTemp, L"<edit width=100% maxchars=256 ");
            if (fReadOnly)
               MemCat (&gMemTemp, gpszEnabledFalse);
            MemCat (&gMemTemp, L"name=hotmsg");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L"/>");
         }

         MemCat (&gMemTemp, L"</bold><br/>"
			         L"<xComboCursor ");
         if (fReadOnly)
            MemCat (&gMemTemp, gpszEnabledFalse);
         MemCat (&gMemTemp, L"name=hotcursor");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/><br/>");
			MemCat (&gMemTemp, L"<button ");
         if (fReadOnly)
            MemCat (&gMemTemp, gpszEnabledFalse);
         MemCat (&gMemTemp, L"name=hotdel");
         MemCat (&gMemTemp, (int) i);
         MemCat (&gMemTemp, L"><bold>Remove this</bold></button>"
		            L"</td></tr>");
      }

      p->pszSubString = (PWSTR)gMemTemp.p;
      return TRUE;
   }

   return FALSE;
}


/*************************************************************************************
HotSpotInitPage - Called when init page, to set up hotspots.

inputs
   PCEscPage         pPage - Page
   PCListFixed       plPCCircumrealityHotSpot - List of hot spots
   BOOL              f360 - If TRUE then 360 degree image
   DWORD             dwWidth - Width in pixels of image. If both 0 then then dont call RenderSceneHotSpotToImage
   DWORD             dwHeight - Height in pixels of image. If both 0...
returns
   BOOL - TRUE if captured.
*/
DLLEXPORT BOOL HotSpotInitPage (PCEscPage pPage, PCListFixed plCPCircumrealityHotSpot, BOOL f360,
                                DWORD dwWidth, DWORD dwHeight)
{
   PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plCPCircumrealityHotSpot->Get(0);
   DWORD i, j;
   WCHAR szTemp[64];
   PCEscControl pControl;
   CListFixed lCD;
   lCD.Init (sizeof(CONTROLIMAGEDRAGRECT));
   CONTROLIMAGEDRAGRECT cd;
   memset (&cd, 0, sizeof(cd));
   lCD.Required (plCPCircumrealityHotSpot->Num());
   for (i = 0; i < plCPCircumrealityHotSpot->Num(); i++, pph++) {
      swprintf (szTemp, L"hotmsg%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl)
         pControl->AttribSet (Text(), pph[0]->m_ps->Get());

      swprintf (szTemp, L"hotcursor%d", (int)i);
      ComboBoxSet (pPage, szTemp, pph[0]->m_dwCursor);

      // add to list that will send to bitmap
      cd.cColor = HotSpotColor(i);
      cd.fModulo = f360;
      cd.rPos = pph[0]->m_rPosn;
      if (dwWidth || dwHeight)
         RenderSceneHotSpotToImage (&cd.rPos, dwWidth, dwHeight);
      lCD.Add (&cd);

      // write out menu
      for (j = 0; j < pph[0]->m_lMenuShow.Num(); j++) {
         swprintf (szTemp, L"hotmenushow%d:%d", (int)i, (int)j);
         if (pControl = pPage->ControlFind (szTemp))
            pControl->AttribSet (Text(), (PWSTR)pph[0]->m_lMenuShow.Get(j));
      } // j

      // write out the extra text
      for (j = 0; j < pph[0]->m_lMenuExtraText.Num(); j++) {
         swprintf (szTemp, L"hotmenuextratext%d:%d", (int)i, (int)j);
         if (pControl = pPage->ControlFind (szTemp))
            pControl->AttribSet (Text(), (PWSTR)pph[0]->m_lMenuExtraText.Get(j));
      } // j

      // write out do
      for (j = 0; j < pph[0]->m_lMenuDo.Num(); j++) {
         swprintf (szTemp, L"hotmenudo%d:%d", (int)i, (int)j);
         if (pControl = pPage->ControlFind (szTemp))
            pControl->AttribSet (Text(), (PWSTR)pph[0]->m_lMenuDo.Get(j));
      } // j
   } // i

   // send message to image to show hotspots
   ESCMIMAGERECTSET is;
   memset (&is, 0, sizeof(is));
   is.dwNum = lCD.Num();
   is.pRect = (PCONTROLIMAGEDRAGRECT)lCD.Get(0);
   pControl = pPage->ControlFind (L"image");
   if (pControl)
      pControl->Message (ESCM_IMAGERECTSET, &is);

   return TRUE;
}



/*************************************************************************************
HotSpotComboBoxSelChanged - Called when combobox changed

inputs
   PCEscPage         pPage - Page
   PESCNCOMBOBOXSELCHANGE p - Combobox info
   PCListFixed       plPCCircumrealityHotSpot - List of hot spots
   BOOL              *pfChanged - Set to TRUE if changed
returns
   BOOL - TRUE if captured.
*/
DLLEXPORT BOOL HotSpotComboBoxSelChanged (PCEscPage pPage, PESCNCOMBOBOXSELCHANGE p,
                                          PCListFixed plPCCircumrealityHotSpot, BOOL *pfChanged)
{
   PWSTR pszHotCursor = L"hotcursor";
   DWORD dwHotCursorLen = (DWORD)wcslen(pszHotCursor);
   PWSTR psz = p->pControl->m_pszName;
   if (!psz)
      return FALSE;

   if (!wcsncmp(psz, pszHotCursor, dwHotCursorLen)) {
      DWORD dwIndex = _wtoi(psz + dwHotCursorLen);
      PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plPCCircumrealityHotSpot->Get(dwIndex);
      if (!pph)
         return TRUE;
      DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
      if (dwVal == pph[0]->m_dwCursor)
         return TRUE;

      // else changed
      DWORD dwOld = pph[0]->m_dwCursor;
      pph[0]->m_dwCursor = dwVal;
      *pfChanged = TRUE;

      // if changed to type 10 (menu) then redraw
      if ((dwVal == 10) || (dwOld == 10))
         pPage->Exit (RedoSamePage());

      return TRUE;
   }
   else
      return FALSE;
}


/*************************************************************************************
HotSpotEditChanged - Called when combobox changed

inputs
   PCEscPage         pPage - Page
   PESCNEDITCHANGE p - Combobox info
   PCListFixed       plPCCircumrealityHotSpot - List of hot spots
   BOOL              *pfChanged - Set to TRUE if changed
returns
   BOOL - TRUE if captured.
*/
DLLEXPORT BOOL HotSpotEditChanged (PCEscPage pPage, PESCNEDITCHANGE p,
                                          PCListFixed plPCCircumrealityHotSpot, BOOL *pfChanged)
{
   PWSTR psz = p->pControl->m_pszName;
   if (!psz)
      return FALSE;

   PWSTR pszHotMsg = L"hotmsg", pszHotMenuShow = L"hotmenushow", pszHotMenuDo = L"hotmenudo",
      pszHotMenuExtraText = L"hotmenuextratext";
   DWORD dwHotMsgLen = (DWORD)wcslen(pszHotMsg),
      dwHotMenuShowLen = (DWORD)wcslen(pszHotMenuShow), dwHotMenuDoLen = (DWORD)wcslen(pszHotMenuDo),
      dwHotMenuExtraTextLen = (DWORD)wcslen(pszHotMenuExtraText);

   if (!wcsncmp(psz, pszHotMsg, dwHotMsgLen)) {
      DWORD dwIndex = _wtoi(psz + dwHotMsgLen);
      PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plPCCircumrealityHotSpot->Get(dwIndex);
      if (!pph)
         return TRUE;

      WCHAR szTemp[512];
      DWORD dwNeeded;
      szTemp[0] = 0;
      p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
      pph[0]->m_ps->Set(szTemp);
      *pfChanged = TRUE;
      return TRUE;
   }
   else if (!wcsncmp(psz, pszHotMenuShow, dwHotMenuShowLen)) {
      DWORD dwIndex = _wtoi(psz + dwHotMenuShowLen);
      PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plPCCircumrealityHotSpot->Get(dwIndex);
      if (!pph)
         return TRUE;
      PWSTR pszColon = wcschr (psz + dwHotMenuShowLen, L':');
      if (!pszColon)
         return TRUE;
      DWORD dwItem = _wtoi(pszColon+1);

      // add blanks to fill in
      PWSTR pszBlank = L"";
      while (pph[0]->m_lMenuShow.Num() <= dwItem)
         pph[0]->m_lMenuShow.Add (pszBlank, (wcslen(pszBlank)+1)*sizeof(WCHAR));

      WCHAR szTemp[512];
      DWORD dwNeeded;
      szTemp[0] = 0;
      p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
      pph[0]->m_lMenuShow.Set (dwItem, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
      *pfChanged = TRUE;
      return TRUE;
   }
   else if (!wcsncmp(psz, pszHotMenuExtraText, dwHotMenuExtraTextLen)) {
      DWORD dwIndex = _wtoi(psz + dwHotMenuExtraTextLen);
      PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plPCCircumrealityHotSpot->Get(dwIndex);
      if (!pph)
         return TRUE;
      PWSTR pszColon = wcschr (psz + dwHotMenuExtraTextLen, L':');
      if (!pszColon)
         return TRUE;
      DWORD dwItem = _wtoi(pszColon+1);

      // add blanks to fill in
      PWSTR pszBlank = L"";
      while (pph[0]->m_lMenuExtraText.Num() <= dwItem)
         pph[0]->m_lMenuExtraText.Add (pszBlank, (wcslen(pszBlank)+1)*sizeof(WCHAR));

      WCHAR szTemp[512];
      DWORD dwNeeded;
      szTemp[0] = 0;
      p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
      pph[0]->m_lMenuExtraText.Set (dwItem, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
      *pfChanged = TRUE;
      return TRUE;
   }
   else if (!wcsncmp(psz, pszHotMenuDo, dwHotMenuDoLen)) {
      DWORD dwIndex = _wtoi(psz + dwHotMenuDoLen);
      PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plPCCircumrealityHotSpot->Get(dwIndex);
      if (!pph)
         return TRUE;
      PWSTR pszColon = wcschr (psz + dwHotMenuDoLen, L':');
      if (!pszColon)
         return TRUE;
      DWORD dwItem = _wtoi(pszColon+1);

      // add blanks to fill in
      PWSTR pszBlank = L"";
      while (pph[0]->m_lMenuDo.Num() <= dwItem)
         pph[0]->m_lMenuDo.Add (pszBlank, (wcslen(pszBlank)+1)*sizeof(WCHAR));

      WCHAR szTemp[512];
      DWORD dwNeeded;
      szTemp[0] = 0;
      p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
      pph[0]->m_lMenuDo.Set (dwItem, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
      *pfChanged = TRUE;
      return TRUE;
   }

   else
      return FALSE;
}


/*************************************************************************************
HotSpotButtonPress - Called when combobox changed

inputs
   PCEscPage         pPage - Page
   PESCNBUTTONPRESS p - Combobox info
   PCListFixed       plPCCircumrealityHotSpot - List of hot spots
   BOOL              *pfChanged - Set to TRUE if changed
returns
   BOOL - TRUE if captured.
*/
DLLEXPORT BOOL HotSpotButtonPress (PCEscPage pPage, PESCNBUTTONPRESS p,
                                          PCListFixed plPCCircumrealityHotSpot, BOOL *pfChanged)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz;
   psz = p->pControl->m_pszName;

   PWSTR pszHotDel = L"hotdel";
   DWORD dwHotDelLen = (DWORD)wcslen(pszHotDel);

   if (!wcsncmp(psz, pszHotDel, dwHotDelLen)) {
      DWORD dwIndex = _wtoi(psz + dwHotDelLen);
      PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)plPCCircumrealityHotSpot->Get(dwIndex);
      if (!pph)
         return TRUE;

      // delete
      delete pph[0];
      plPCCircumrealityHotSpot->Remove (dwIndex);
      *pfChanged = TRUE;
      pPage->Exit (RedoSamePage());
      return TRUE;
   }
   else
      return FALSE;
}



/*************************************************************************************
HotSpotImageDragged - Called when combobox changed

inputs
   PCEscPage         pPage - Page
   PESCNIMAGEDRAGGED p - Combobox info
   PCListFixed       plPCCircumrealityHotSpot - List of hot spots
   DWORD             dwWidth - Width in pixels of image. If both 0 then dont call renderscenehotspotfromimage...
   DWORD             dwHeight - Height in pixels of image. If both  0...
   BOOL              *pfChanged - Set to TRUE if changed
returns
   BOOL - TRUE if captured.
*/
DLLEXPORT BOOL HotSpotImageDragged (PCEscPage pPage, PESCNIMAGEDRAGGED p,
                                    PCListFixed plPCCircumrealityHotSpot,
                                    DWORD dwWidth, DWORD dwHeight, BOOL *pfChanged)
{
   PCCircumrealityHotSpot pNew = new CCircumrealityHotSpot;
   if (!pNew)
      return TRUE;
   pNew->m_rPosn = p->rPos;
   if (dwWidth || dwHeight)
      RenderSceneHotSpotFromImage (&pNew->m_rPosn, dwWidth, dwHeight);
   if (!pNew->m_ps) {
      pNew->m_ps = new CMIFLVarString;
      if (!pNew->m_ps)
         return TRUE;
   }

   plPCCircumrealityHotSpot->Add (&pNew);
   *pfChanged = TRUE;

   // refresh
   pPage->Exit (RedoSamePage());

   return TRUE;
}

