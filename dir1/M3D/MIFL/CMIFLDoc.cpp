/*************************************************************************************
CMIFLDoc.cpp - Definition for a documentation entry

begun 14/1/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


/*************************************************************************************
CMIFLDoc::Constructor and destructor
*/
CMIFLDoc::CMIFLDoc (void)
{
   Clear();
}

CMIFLDoc::~CMIFLDoc (void)
{
   // nothing for now
}

/*************************************************************************************
CMIFLDoc::Clear - Wipes the parameter to intial state
*/
void CMIFLDoc::Clear()
{
   m_dwTempID = MIFLTempID ();
   m_dwLibOrigFrom = 0;

   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   MemZero (&m_aMemHelp[0]);
   MemZero (&m_aMemHelp[1]);

   m_fUseMML = FALSE;
}

/*************************************************************************************
CMIFLDoc::CloneTo - Stanrdard API
*/
BOOL CMIFLDoc::CloneTo (CMIFLDoc *pTo)
{
   pTo->Clear();

   pTo->m_dwTempID = m_dwTempID;
   pTo->m_dwLibOrigFrom = m_dwLibOrigFrom;
   pTo->m_fUseMML = m_fUseMML;

   if (((PWSTR)m_memName.p)[0])
      MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   if (((PWSTR)m_memDescShort.p)[0])
      MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
   if (((PWSTR)m_memDescLong.p)[0])
      MemCat (&pTo->m_memDescLong, (PWSTR)m_memDescLong.p);
   if (((PWSTR)m_aMemHelp[0].p)[0])
      MemCat (&pTo->m_aMemHelp[0], (PWSTR)m_aMemHelp[0].p);
   if (((PWSTR)m_aMemHelp[1].p)[0])
      MemCat (&pTo->m_aMemHelp[1], (PWSTR)m_aMemHelp[1].p);

   return TRUE;
}


/*************************************************************************************
CMIFLDoc::Clone - Stanrdard API
*/
CMIFLDoc *CMIFLDoc::Clone (void)
{
   PCMIFLDoc pNew = new CMIFLDoc;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



static PWSTR gpszMIFLParam = L"MIFLParam";
static PWSTR gpszName = L"Name";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszDescLong = L"DescLong";
static PWSTR gpszHelp0 = L"Help0";
static PWSTR gpszHelp1 = L"Help1";
static PWSTR gpszUseMML = L"UseMML";

/*************************************************************************************
CMIFLDoc::MMLTo - Stanrdard API
*/
PCMMLNode2 CMIFLDoc::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLParam);

   MMLValueSet (pNode, gpszUseMML, (int)m_fUseMML);

   if (m_memName.p && ((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR)m_memName.p);
   if (m_memDescShort.p && ((PWSTR)m_memDescShort.p)[0])
      MMLValueSet (pNode, gpszDescShort, (PWSTR)m_memDescShort.p);
   if (m_memDescLong.p && ((PWSTR)m_memDescLong.p)[0])
      MMLValueSet (pNode, gpszDescLong, (PWSTR)m_memDescLong.p);
   if (m_aMemHelp[0].p && ((PWSTR)m_aMemHelp[0].p)[0])
      MMLValueSet (pNode, gpszHelp0, (PWSTR)m_aMemHelp[0].p);
   if (m_aMemHelp[1].p && ((PWSTR)m_aMemHelp[1].p)[0])
      MMLValueSet (pNode, gpszHelp1, (PWSTR)m_aMemHelp[1].p);

   return pNode;
}


/*************************************************************************************
CMIFLDoc::MMLTo - Stanrdard API
*/
BOOL CMIFLDoc::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   m_fUseMML = (BOOL)MMLValueGetInt (pNode, gpszUseMML, 0);

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);

   psz = MMLValueGet (pNode, gpszDescLong);
   if (psz)
      MemCat (&m_memDescLong, psz);

   psz = MMLValueGet (pNode, gpszHelp0);
   if (psz)
      MemCat (&m_aMemHelp[0], psz);
   HACKRENAMEALL(&m_aMemHelp[0]);

   psz = MMLValueGet (pNode, gpszHelp1);
   if (psz)
      MemCat (&m_aMemHelp[1], psz);
   HACKRENAMEALL(&m_aMemHelp[1]);



   return TRUE;
}

/*************************************************************************************
CMIFLDoc::PageEditChange - Call this when an edit change notification comes in.
It will see if it affects any of the docs parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNEDITCHANGE   p - Edit change information
   PCMIFLLib         pLib - Library that this is contained in
   BOOL              *pfChangedName - Will fill with TRUE if changed then doc's
                     name, which would require a resort of the parent
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLDoc::PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p,
                                PCMIFLLib pLib, BOOL *pfChangedName)
{
   // init
   *pfChangedName = FALSE;

   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   // how big is this?
   DWORD dwNeed = 0;
   p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);

   PCMem pMem = NULL;

   if (!_wcsicmp(psz, L"name")) {
      *pfChangedName = TRUE;
      pMem = &m_memName;
   }
   else if (!_wcsicmp(psz, L"helpcat0"))
      pMem = &m_aMemHelp[0];
   else if (!_wcsicmp(psz, L"helpcat1"))
      pMem = &m_aMemHelp[1];
   else if (!_wcsicmp(psz, L"descshort"))
      pMem = &m_memDescShort;
   else if (!_wcsicmp(psz, L"desclong"))
      pMem = &m_memDescLong;

   if (pMem) {
      if (!pMem->Required (dwNeed))
         return FALSE;

      pLib->AboutToChange ();
      p->pControl->AttribGet (Text(), (PWSTR)pMem->p, (DWORD)pMem->m_dwAllocated, &dwNeed);

      pLib->Changed ();
      return TRUE;
   }


   return FALSE;  // not it
}

/*************************************************************************************
CMIFLDoc::PageInitPage - This functions handles the init-page call for a doc.
This allows the code to be used for not only the doc defiitions, but also functions,
and private doc.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
returns
   none
*/
void CMIFLDoc::PageInitPage (PCEscPage pPage, PMIFLPAGE pmp)
{
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLDoc pDoc = pmp->pDoc;

   PCEscControl pControl;

   pControl = pPage->ControlFind (L"name");
   if (pControl && m_memName.p)
      pControl->AttribSet (Text(), (PWSTR)m_memName.p);

   pControl = pPage->ControlFind (L"helpcat0");
   if (pControl && m_aMemHelp[0].p)
      pControl->AttribSet (Text(), (PWSTR)m_aMemHelp[0].p);

   pControl = pPage->ControlFind (L"helpcat1");
   if (pControl && m_aMemHelp[1].p)
      pControl->AttribSet (Text(), (PWSTR)m_aMemHelp[1].p);

   pControl = pPage->ControlFind (L"descshort");
   if (pControl && m_memDescShort.p)
      pControl->AttribSet (Text(), (PWSTR)m_memDescShort.p);

   pControl = pPage->ControlFind (L"desclong");
   if (pControl && m_memDescLong.p)
      pControl->AttribSet (Text(), (PWSTR)m_memDescLong.p);

   pControl = pPage->ControlFind (L"usemml");
   if (pControl)
      pControl->AttribSetBOOL (Checked(), m_fUseMML);

}



// globals used by test page
static DWORD gdwErrorNum = 0;
static DWORD gdwErrorSurroundChar = 0;
static CMem gMemErrorString;
static CMem gMemErrorSurround;
static CMem gMemErrorSurroundChar;

/***********************************************************************
TestPage callback - All it really does is store the error info away
*/
BOOL TestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // add am accelerator for escape just in case there's no title bar
      // as set by preferences
      ESCACCELERATOR a;
      memset (&a, 0, sizeof(a));
      a.c = VK_ESCAPE;
      a.dwMessage = ESCM_CLOSE;
      pPage->m_listESCACCELERATOR.Add (&a);
      return TRUE;

   case ESCM_INTERPRETERROR:
      {
         PESCMINTERPRETERROR p = (PESCMINTERPRETERROR) pParam;

         gdwErrorNum = p->pError->m_dwNum;
         gMemErrorString.Required ((wcslen(p->pError->m_pszDesc)+1)*2);
         wcscpy ((PWSTR)gMemErrorString.p, p->pError->m_pszDesc);

         if (p->pError->m_pszSurround) {
            gMemErrorSurround.Required ((wcslen(p->pError->m_pszSurround)+1)*2);
            wcscpy ((PWSTR) gMemErrorSurround.p, p->pError->m_pszSurround);
            gdwErrorSurroundChar = p->pError->m_dwSurroundChar;
         }
         else {
            gdwErrorSurroundChar = (DWORD)-1;
         }
      }
      return TRUE;
   }
   return FALSE;
}


/*************************************************************************************
CMIFLDoc::PageButtonPress - Call this when an button press notification comes in.
It will see if it affects any of the method parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNBUTTONPRESS  p - Buttonpress information
   PCMIFLLib         pLib - Library that this is contained in
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLDoc::PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;


   if (!_wcsicmp(psz, L"usemml")) {
      pLib->AboutToChange();
      m_fUseMML = p->pControl->AttribGetBOOL (Checked());
      pLib->Changed();

      pPage->Exit (MIFLRedoSamePage());
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"testmml")) {
      if (!m_fUseMML) {
         pPage->MBWarning (L"You must check the \"Use MML tags\" checkbox first.");
         return TRUE;
      }

      MemZero (&gMemErrorString);
      MemZero (&gMemErrorSurround);
      MemZero (&gMemErrorSurroundChar);

      // try to compile this
      WCHAR *psz;
      CMem memRet;
      {
         CEscWindow  cWindow;

         cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, 0);
         psz = cWindow.PageDialog (ghInstance, (PWSTR) m_memDescLong.p, TestPage);
         if (psz) {
            MemZero (&memRet);
            MemCat (&memRet, psz);
            psz = (PWSTR)memRet.p;
         }
      }
      
      // if return string then report that
      if (psz) {
         CMem memTemp;
         MemZero (&memTemp);
         MemCat (&memTemp, L"The page returned, ");
         MemCat (&memTemp, psz);
         pPage->MBSpeakInformation ((PWSTR)memTemp.p);
      }
      else {
         // error
         pPage->MBError (L"The compile failed.",
            gMemErrorString.p ? (PWSTR) gMemErrorString.p : L"No reason given.");

         // see if can find the location of the error and set the caret there
         PWSTR pszErr;
         pszErr = (PWSTR) gMemErrorSurround.p;
         if ((gdwErrorSurroundChar != (DWORD)-1) && pszErr) {
            PWSTR pszFind, pszFind2;
            PWSTR pszSrc = (WCHAR*) m_memDescLong.p;
            pszFind = wcsstr (pszSrc, pszErr);

            // keep on looking for the last occurance
            pszFind2 = pszFind;
            while (pszFind2) {
               pszFind2 = wcsstr (pszFind+1, pszErr);
               if (pszFind2)
                  pszFind = pszFind2;
            }

            if (pszFind) {
               PCEscControl pc = pPage->ControlFind (L"desclong");

               // found it, set the attribute
               WCHAR szTemp[16];
               swprintf (szTemp, L"%d", ((PBYTE) pszFind - (PBYTE) pszSrc)/2 + gdwErrorSurroundChar);
               pc->AttribSet (L"selstart", szTemp);
               pc->AttribSet (L"selend", szTemp);
               pc->Message (ESCM_EDITSCROLLCARET);
               pPage->FocusSet (pc);   // BUGFIX - set focus
            }
         }
      }

      return TRUE;
   }

   return FALSE;
}



/*********************************************************************************
DocEditPage - UI
*/

BOOL DocEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLDoc pDoc = pmp->pDoc;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // initialize params
         pDoc->PageInitPage (pPage, pmp);

         // will need to set movelib combo
         MIFLComboLibs (pPage, L"movelib", L"move", pProj, pLib->m_dwTempID);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (pDoc->PageButtonPress (pPage, pmp, p, pLib))
            return TRUE;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this document?"))
               return TRUE;

            // remove the doc
            pLib->DocRemove (pDoc->m_dwTempID);

            // set a link to go to the doc list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%ddoclist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated document.");

            // duplicate
            DWORD dwID = pLib->DocAddCopy (pDoc);

            // set a link to go to the Doc list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%ddoc:%dedit", pLib->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"move")) {
            // find where moved to
            PCEscControl pControl = pPage->ControlFind (L"movelib");
            if (!pControl)
               return TRUE;
            ESCMCOMBOBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            if (!gi.pszName)
               return FALSE;

            // find the library
            DWORD dwLibIndex = _wtoi(gi.pszName);
            PCMIFLLib pLibTo = pProj->LibraryGet (dwLibIndex);
            if (!pLibTo)
               return TRUE;

            if (pLib->m_fReadOnly) {
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the documentation?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The documentation will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->DocAddCopy (pDoc);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->DocRemove (pDoc->m_dwTempID);

            // set a link to go to the doc list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%ddoc:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         BOOL fRename;

         if (pDoc->PageEditChange (pPage, pmp, p, pLib, &fRename)) {
            // if renamed then need to resort the list
            if (fRename) {
               pLib->AboutToChange();
               pLib->DocSort();
               pLib->Changed();
            }

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Modify documentation - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pDoc->m_memName.p);
               // BUGFIX - Was MemCatSanitize, but dont need to since the PAGETITLE
               // is alredy sanitized
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DOCEDIT")) {
            MemZero (&gMemTemp);
            if (pDoc->m_fUseMML)
               MemCat (&gMemTemp, L"<font face=courier>");
            MemCat (&gMemTemp, L"<align tab=16><edit width=90% height=40% maxchars=200000 ");
            MemCat (&gMemTemp, pDoc->m_fUseMML ?
               L"prelineindent=true" :
               L"wordwrap=true");
            if (pLib->m_fReadOnly)
               MemCat (&gMemTemp, L" readonly=true");
            MemCat (&gMemTemp, L" multiline=true capturetab=true vscroll=desclongscroll name=desclong/></align>");
            if (pDoc->m_fUseMML)
               MemCat (&gMemTemp, L"</font>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->DocOverridden (pLib->m_dwTempID, (PWSTR)pDoc->m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by document in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides document in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}





/********************************************************************************
CMIFLDoc::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library that the doc is part of
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%ddoc%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLDoc::DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLDOCEDIT;
   PESCPAGECALLBACK pPage = DocEditPage;

   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLDOCEDIT;
      pPage = DocEditPage;
   }

   PWSTR pszRet;
   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pDoc = this;
   mp.pLib = pLib;
   mp.pProj = mp.pLib->ProjectGet();
   mp.iVScroll = *piVScroll;
redo:
   pszRet = pWindow->PageDialog (ghInstance, dwPage, pPage, &mp);
   *piVScroll = mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, MIFLRedoSamePage()))
      goto redo;

   if (pszRet)
      wcscpy (pszNext, pszRet);
   else
      pszNext[0] = 0;
   return TRUE;
}

// BUGBUG - the help search-library takes too long to build. Part of the problem
// might be that indecies are included in the help database even though dont need to

// BUGBUG - the help search-library takes too long to build. Part of the problem
// might be that escarpment is doing something very slow
