/*************************************************************************************
CResVerb.cpp - Code for UI to modify a verb-window resource.

begun 2/5/04 by Mike Rozak.
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





static DWORD gadwIconResource[] = {
   IDB_VERB0, IDB_VERB1, IDB_VERB2, IDB_VERB3, IDB_VERB4,
   IDB_VERBMESSAGEBOARD, IDB_VERBMAIL, IDB_VERBIDENTIFYSELF, IDB_VERBWORLD,
   IDB_VERBMAP,
   IDB_VERBFRIENDS, IDB_VERBPARTY,
   IDB_VERBMAGNIFYINGGLASS, IDB_VERBMAGNIFYINGGLASSCROSS,
   IDB_VERBREPORTBUG,
   IDB_VERBWAVE,
   IDB_VERBLAUGH, IDB_VERBSMILE, IDB_VERBSMILEMILD, IDB_VERBWINK, IDB_VERBNEUTRAL,
   IDB_VERBANGRY, IDB_VERBFROWN, IDB_VERBPURSELIPS,
   IDB_VERBSADMILD, IDB_VERBSAD, IDB_VERBCRY,
   IDB_VERBAFRAID, IDB_VERBSURPRISED, IDB_VERBWORRIED,
   IDB_VERBSHIFTYLOOKLEFT, IDB_VERBSHIFTYLOOKRIGHT,
   IDB_VERBWHISTLE, IDB_VERBCROSSEYED, IDB_VERBFUNNYFACE, IDB_VERBSTICKOUTTONGUE,
   IDB_VERBDAZED, IDB_VERBUNCONSCIOUS, IDB_VERBDEAD,
   IDB_VERBNODYES, IDB_VERBNODNO,
   IDB_VERBJOURNAL, IDB_VERBQUESTS, IDB_VERBINVENTORY, IDB_VERBSKILLS,
   IDB_VERBABUSE, IDB_VERBHELP, IDB_VERBPLAYERHELP, IDB_VERBTUTORIALOFF,
   IDB_VERBVOTERANK, IDB_VERBEXIT, IDB_VERBEXIT2};
   // BUGBUG - add other verbs in the future

/*************************************************************************************
CResVerbIcon::Constructor and destructor
*/
CResVerbIcon::CResVerbIcon (void)
{
   m_dwIcon = 0;
   m_lid = 0;
   MemZero (&m_memDo);
   MemZero (&m_memShow);
   m_fHasClick = FALSE;
   m_pButton = NULL;
}

CResVerbIcon::~CResVerbIcon (void)
{
   if (m_pButton) {
      delete m_pButton;
      m_pButton = NULL;
   }
}


/*************************************************************************************
CResVerbIcon::Clone - Clones the object, except for the button
*/
CResVerbIcon *CResVerbIcon::Clone (void)
{
   PCResVerbIcon pNew = new CResVerbIcon;
   if (!pNew)
      return NULL;

   pNew->m_dwIcon = m_dwIcon;
   pNew->m_lid = m_lid;
   pNew->m_fHasClick = m_fHasClick;

   MemCat (&pNew->m_memDo, (PWSTR)m_memDo.p);
   MemCat (&pNew->m_memShow, (PWSTR)m_memShow.p);

   return pNew;
}

static PWSTR gpszVerb = L"Verb";
static PWSTR gpszIcon = L"Icon";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszDo = L"Do";
static PWSTR gpszShow = L"Show";

/*************************************************************************************
CResVerbIcon::MMLTo - Standard API
*/
PCMMLNode2 CResVerbIcon::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVerb);

   if (m_dwIcon >= IconNum())
      m_dwIcon = 0;   // just in case
   MMLValueSet (pNode, gpszIcon, (int)gadwIconResource[m_dwIcon]);

   MMLValueSet (pNode, gpszLangID, (int)m_lid);
   PWSTR psz = (PWSTR)m_memDo.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDo, psz);
   psz = (PWSTR)m_memShow.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszShow, psz);

   return pNode;
}



/*************************************************************************************
CResVerbIcon::MMLFrom - Standard API
*/
BOOL CResVerbIcon::MMLFrom (PCMMLNode2 pNode)
{
   MemZero (&m_memDo);
   MemZero (&m_memShow);

   m_dwIcon = (DWORD) MMLValueGetInt (pNode, gpszIcon, (int)0);
   DWORD dwNum = IconNum();
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (m_dwIcon == gadwIconResource[i])
         break;
   m_dwIcon = (i < dwNum) ? i : 0;

   m_lid = (LANGID) MMLValueGetInt (pNode, gpszLangID, (int)DEFLANGID);
   
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszDo);
   if (psz)
      MemCat (&m_memDo, psz);

   if (psz && MyStrIStr (psz, L"<Click>"))
      m_fHasClick = TRUE;
   else
      m_fHasClick = FALSE;

   psz = MMLValueGet (pNode, gpszShow);
   if (psz)
      MemCat (&m_memShow, psz);
   return TRUE;
}


/*************************************************************************************
CResVerbIcon::IconNum - Returns the number of icons supported by the system.
Icon numbers are from 0..IconNum()-1.
*/
__inline DWORD CResVerbIcon::IconNum (void)
{
   return sizeof(gadwIconResource) / sizeof(gadwIconResource[0]);
}


/*************************************************************************************
CResVerbIcon::IconResourceID - Returns the bitmap resoruce number for the given
icon, m_dwIcon
*/
DWORD CResVerbIcon::IconResourceID (void)
{
   if (m_dwIcon >= IconNum())
      return gadwIconResource[0];   // just return 1st one

   return gadwIconResource[m_dwIcon];
}


/*************************************************************************************
CResVerbIcon::IconResourceInstance - Returns the HINSTANCE for the module with the
icon
*/
HINSTANCE CResVerbIcon::IconResourceInstance (void)
{
   return ghInstance;
}




/*************************************************************************************
CResVerb::Constructor and destructor
*/
CResVerb::CResVerb (void)
{
   m_lPCResVerbIcon.Init (sizeof(PCResVerbIcon));
   MemZero (&m_memVersion);
   m_pWindowLoc.Zero4();
   m_fHidden = FALSE;
   m_fDelete = FALSE;
}

CResVerb::~CResVerb (void)
{
   DWORD i;
   PCResVerbIcon *ppr = (PCResVerbIcon*)m_lPCResVerbIcon.Get(0);
   for (i = 0; i < m_lPCResVerbIcon.Num(); i++)
      delete ppr[i];
   m_lPCResVerbIcon.Clear();
}


static PWSTR gpszIconSize = L"IconSize";
static PWSTR gpszVersion = L"Version";
static PWSTR gpszWindowLoc = L"WindowLoc";
static PWSTR gpszHidden = L"Hidden";
static PWSTR gpszDelete = L"Delete";

/*************************************************************************************
CResVerb::MMLTo - Standard API
*/
PCMMLNode2 CResVerb::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (CircumrealityVerbWindow());

   PWSTR psz = (PWSTR)m_memVersion.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszVersion, psz);
   if ((m_pWindowLoc.p[0] != m_pWindowLoc.p[1]) && (m_pWindowLoc.p[2] != m_pWindowLoc.p[3]))
      MMLValueSet (pNode, gpszWindowLoc, &m_pWindowLoc);
   if (m_fHidden)
      MMLValueSet (pNode, gpszHidden, (int)m_fHidden);
   if (m_fDelete)
      MMLValueSet (pNode, gpszDelete, (int)m_fDelete);

   // write verbs
   DWORD i;
   PCResVerbIcon *ppr = (PCResVerbIcon*)m_lPCResVerbIcon.Get(0);
   for (i = 0; i < m_lPCResVerbIcon.Num(); i++) {
      PCMMLNode2 pSub = ppr[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   return pNode;
}


/*************************************************************************************
CResVerb::MMLFrom - Standard API
*/
BOOL CResVerb::MMLFrom (PCMMLNode2 pNode)
{
   // clear existing
   DWORD i;
   PCResVerbIcon *ppr = (PCResVerbIcon*)m_lPCResVerbIcon.Get(0);
   for (i = 0; i < m_lPCResVerbIcon.Num(); i++)
      delete ppr[i];
   m_lPCResVerbIcon.Clear();

   m_pWindowLoc.Zero4();
   MemZero (&m_memVersion);

   // read in
   MMLValueGetPoint (pNode, gpszWindowLoc, &m_pWindowLoc);
   m_fHidden = (BOOL) MMLValueGetInt (pNode, gpszHidden, (int)FALSE);
   m_fDelete = (BOOL) MMLValueGetInt (pNode, gpszDelete, (int)FALSE);
   PWSTR psz = MMLValueGet (pNode, gpszVersion);
   if (psz)
      MemCat (&m_memVersion, psz);

   // read verbs
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszVerb)) {
         PCResVerbIcon pv = new CResVerbIcon;
         if (!pv)
            continue;
         pv->MMLFrom (pSub);
         m_lPCResVerbIcon.Add (&pv);
         continue;
      }
   } // i

   return TRUE;
}


/*************************************************************************************
MemCatImage - Concatenates the icon image on the memory.

inputs
   PCMem       pMem - Memory to append to
   DWORD       dwID - Resource ID
   BOOL        fToRight - If TRUE then image will be to the right
returns
   none
*/
static void MemCatImage (PCMem pMem, DWORD dwID, BOOL fToRight)
{
   MemCat (pMem, L"<image bmpresource=");
   MemCat (pMem, (int)dwID);

   if (fToRight)
      MemCat (pMem, L" posn=edgeright");

   MemCat (pMem, L" scale=50% transparent=true transparentdistance=1/>");
}

/*************************************************************************************
FillComboWithIcons - Fills the combobox with icons

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control name
returns
   BOOL - TRUE if found and set
*/
static BOOL FillComboWithIcons (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // wipe out
   pControl->Message (ESCM_COMBOBOXRESETCONTENT);

   // elements
   CMem mem;
   MemZero (&mem);
   DWORD i;
   CResVerbIcon rvi;
   for (i = 0; i < rvi.IconNum(); i++) {
      MemCat (&mem, L"<elem name=");
      MemCat (&mem, (int)i);
      MemCat (&mem, L">");

      rvi.m_dwIcon = i;
      MemCatImage (&mem, rvi.IconResourceID(), FALSE);

      MemCat (&mem, L"</elem>");
   } // i

   // write
   ESCMCOMBOBOXADD ca;
   memset (&ca, 0, sizeof(ca));
   ca.dwInsertBefore = 0;
   ca.pszMML = (PWSTR)mem.p;
   pControl->Message (ESCM_COMBOBOXADD, &ca);

   // set the select
   pControl->AttribSetInt (CurSel(), 0);

   return TRUE;
}


/*************************************************************************************
FillListBoxWithIcons - Fills the listbox with icons

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control name
   PCListFixed    plPCResVerbIcon - List of verbs to display
   DWORD          dwSel - Index to select
returns
   BOOL - TRUE if found and set
*/
static BOOL FillListBoxWithIcons (PCEscPage pPage, PWSTR pszControl,
                                  PCListFixed plPCResVerbIcon, DWORD dwSel)
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
   PCResVerbIcon *ppr = (PCResVerbIcon*)plPCResVerbIcon->Get(0);
   for (i = 0; i < plPCResVerbIcon->Num(); i++) {
      MemCat (&mem, L"<elem name=");
      MemCat (&mem, (int)i);
      MemCat (&mem, L">");

      MemCatImage (&mem, ppr[i]->IconResourceID(), TRUE);

      MemCat (&mem, L"<bold>");
      MemCatSanitize (&mem, (PWSTR) ppr[i]->m_memDo.p);
      MemCat (&mem, L"</bold><br/>");
      if (((PWSTR)ppr[i]->m_memShow.p)[0])
         MemCatSanitize (&mem, (PWSTR) ppr[i]->m_memShow.p);

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
ResVerbPage
*/
BOOL ResVerbPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCResVerb pv = (PCResVerb)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         DWORD i;
         WCHAR szTemp[64];

         FillListBoxWithIcons (pPage, L"verblist", &pv->m_lPCResVerbIcon, 0);
         FillComboWithIcons (pPage, L"icon");

         // set the language for the hotspots
         if (pv->m_pMIFLProj)
            MIFLLangComboBoxSet (pPage, L"langid", pv->m_lidDefault, pv->m_pMIFLProj);
         ComboBoxSet (pPage, L"icon", 0);

         if (pControl = pPage->ControlFind (L"version"))
            pControl->AttribSet (Text(), (PWSTR)pv->m_memVersion.p);


         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"WindowLoc%d", (int)i);
            DoubleToControl (pPage, szTemp, pv->m_pWindowLoc.p[i]);

            if (!pv->m_fReadOnly)
               continue;
            if (pControl = pPage->ControlFind (szTemp))
               pControl->Enable (FALSE);
         } // i
         

         if (pControl = pPage->ControlFind (L"hidden"))
            pControl->AttribSetBOOL (Checked(), pv->m_fHidden);
         if (pControl = pPage->ControlFind (L"delete"))
            pControl->AttribSetBOOL (Checked(), pv->m_fDelete);

         if (!pv->m_pRevert)
            if (pControl = pPage->ControlFind (L"revert"))
               pControl->Enable (FALSE);

         // disable controls?
         if (pv->m_fReadOnly) {
            if (pControl = pPage->ControlFind (L"moveup"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"movedown"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"deleteverb"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"icon"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"command"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"tooltip"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"langid"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"addverb"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"iconsize"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"version"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"hidden"))
               pControl->Enable (FALSE);
            if (pControl = pPage->ControlFind (L"delete"))
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

         if (!_wcsicmp(psz, L"icon")) {
            // just store this away
            pv->m_dwCurIcon = p->pszName ? _wtoi(p->pszName) : 0;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"langid")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            LANGID *padw = (LANGID*)pv->m_pMIFLProj->m_lLANGID.Get(dwVal);
            dwVal = padw ? padw[0] : pv->m_lidDefault;
            if (dwVal == pv->m_lidDefault)
               return TRUE;

            // else changed
            pv->m_lidDefault = (LANGID)dwVal;
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
         DWORD i;
         WCHAR szTemp[128];
         pv->m_fChanged = TRUE;
         for (i = 0; i < 4; i++) {
            swprintf (szTemp, L"WindowLoc%d", (int)i);
            pv->m_pWindowLoc.p[i] = DoubleFromControl (pPage, szTemp);
         }

         DWORD dwNeeded;
         szTemp[0] = 0;
         p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);

         if (!_wcsicmp(psz, L"version")) {
            MemZero (&pv->m_memVersion);
            MemCat (&pv->m_memVersion, szTemp);
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

         if (!_wcsicmp(psz, L"hidden")) {
            pv->m_fHidden = p->pControl->AttribGetBOOL (Checked());
            pv->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"delete")) {
            pv->m_fDelete = p->pControl->AttribGetBOOL (Checked());
            pv->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"revert")) {
            if (!pv->m_pRevert)
               return TRUE;

            // delete existing ones and copy over
            DWORD i;
            PCResVerbIcon *ppr = (PCResVerbIcon*)pv->m_lPCResVerbIcon.Get(0);
            for (i = 0; i < pv->m_lPCResVerbIcon.Num(); i++)
               delete ppr[i];
            pv-> m_lPCResVerbIcon.Clear();

            // copy over
            ppr = (PCResVerbIcon*)pv->m_pRevert->m_lPCResVerbIcon.Get(0);
            for (i = 0; i < pv->m_pRevert->m_lPCResVerbIcon.Num(); i++) {
               PCResVerbIcon pNew = ppr[i]->Clone();
               if (!pNew)
                  continue;
               pv->m_lPCResVerbIcon.Add (&pNew);
            }

            pPage->Exit (RedoSamePage());
            pv->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"moveup")) {
            PCEscControl pControl = pPage->ControlFind (L"verblist");
            if (!pControl)
               return TRUE;
            DWORD dwSel = pControl->AttribGetInt (CurSel());

            if ((dwSel < 1) || (dwSel >= pv->m_lPCResVerbIcon.Num())) {
               pPage->MBInformation (L"You cannot move the top entry up.");
               return TRUE;
            }

            // else, move
            PCResVerbIcon *ppr = (PCResVerbIcon*)pv->m_lPCResVerbIcon.Get(0);
            PCResVerbIcon pTemp = ppr[dwSel-1];
            ppr[dwSel-1] = ppr[dwSel];
            ppr[dwSel] = pTemp;

            pv->m_fChanged = TRUE;
            FillListBoxWithIcons (pPage, L"verblist", &pv->m_lPCResVerbIcon, dwSel-1);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"movedown")) {
            PCEscControl pControl = pPage->ControlFind (L"verblist");
            if (!pControl)
               return TRUE;
            DWORD dwSel = pControl->AttribGetInt (CurSel());

            if ((dwSel+1 >= pv->m_lPCResVerbIcon.Num())) {
               pPage->MBInformation (L"You cannot move the bottom entry down.");
               return TRUE;
            }

            // else, move
            PCResVerbIcon *ppr = (PCResVerbIcon*)pv->m_lPCResVerbIcon.Get(0);
            PCResVerbIcon pTemp = ppr[dwSel+1];
            ppr[dwSel+1] = ppr[dwSel];
            ppr[dwSel] = pTemp;

            pv->m_fChanged = TRUE;
            FillListBoxWithIcons (pPage, L"verblist", &pv->m_lPCResVerbIcon, dwSel+1);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"delverb")) {
            PCEscControl pControl = pPage->ControlFind (L"verblist");
            if (!pControl)
               return TRUE;
            DWORD dwSel = pControl->AttribGetInt (CurSel());

            if (dwSel >= pv->m_lPCResVerbIcon.Num()) {
               pPage->MBInformation (L"You must select an entry to delete.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this button?"))
               return TRUE;

            // else, move
            PCResVerbIcon *ppr = (PCResVerbIcon*)pv->m_lPCResVerbIcon.Get(0);
            delete ppr[dwSel];
            pv->m_lPCResVerbIcon.Remove (dwSel);
            pv->m_fChanged = TRUE;
            FillListBoxWithIcons (pPage, L"verblist", &pv->m_lPCResVerbIcon, dwSel ? (dwSel-1) : 0);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addverb")) {
            WCHAR szCommand[256], szTooltip[128];
            DWORD dwNeed;
            PCEscControl pControl;
            szCommand[0] = szTooltip[0] = 0;

            if (pControl = pPage->ControlFind (L"command"))
               pControl->AttribGet (Text(), szCommand, sizeof(szCommand), &dwNeed);
            if (!szCommand[0]) {
               pPage->MBWarning (L"You must type in a command to send.");
               return TRUE;
            }
            if (pControl = pPage->ControlFind (L"tooltip"))
               pControl->AttribGet (Text(), szTooltip, sizeof(szTooltip), &dwNeed);

            // add it
            PCResVerbIcon pvi = new CResVerbIcon;
            if (!pvi)
               return TRUE;
            pvi->m_dwIcon = pv->m_dwCurIcon;
            pvi->m_lid = pv->m_lidDefault;
            MemZero (&pvi->m_memDo);
            MemCat (&pvi->m_memDo, szCommand);
            MemZero (&pvi->m_memShow);
            MemCat (&pvi->m_memShow, szTooltip);
            if (MyStrIStr ((PWSTR)pvi->m_memDo.p, L"<Click>"))
               pvi->m_fHasClick = TRUE;

            pv->m_lPCResVerbIcon.Add (&pvi);
            pv->m_fChanged = TRUE;
            FillListBoxWithIcons (pPage, L"verblist", &pv->m_lPCResVerbIcon,
               pv->m_lPCResVerbIcon.Num()-1);

            pPage->MBSpeakInformation (L"Button added.");

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = pv->m_fClientUI ? L"Toolbar customization" : L"Verb window resource";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CResVerb::Edit - Brings up UI for editing the resource.

inputs
   HWND        hWnd - To display from
   LANGID      lid - Default language ID
   BOOL        fReadOnly - TRUE if read only
   PCMIFLProj  pMIFLProj - Project use for list of available languages.
               This can be NULL.
   BOOL        fClientUI - IF TRUE then display this with the MIF client UI.
   PCResVerb   pRevert - Used in the client UI... if this is not NULL, then
               the revert button allows the user to restore the original settings.
returns
   BOOL - TRUE if changed
*/
BOOL CResVerb::Edit (HWND hWnd, LANGID lid, BOOL fReadOnly, PCMIFLProj pMIFLProj,
                     BOOL fClientUI, PCResVerb pRevert)
{
   m_lidDefault = lid;
   m_pMIFLProj = pMIFLProj;
   m_fReadOnly = fReadOnly;
   m_fChanged = FALSE;
   m_dwCurIcon = 0;
   m_fClientUI = fClientUI;
   m_pRevert = pRevert;

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   CEscWindow Window;
   Window.Init (ghInstance, hWnd, fClientUI ? EWS_FIXEDSIZE : 0, &r);
redo:
   psz = Window.PageDialog (ghInstance, fClientUI ? IDR_MMLRESVERBCLIENT : IDR_MMLRESVERB, ResVerbPage, this);
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   return m_fChanged;
}


