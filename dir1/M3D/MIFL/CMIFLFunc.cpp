/*************************************************************************************
CMIFLFunc.cpp - Code for managing a MIFL function

begun 4/1/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


/*************************************************************************************
CMIFLFunc::Constructor and destructor
*/
CMIFLFunc::CMIFLFunc (void)
{
   // do nothing for now since m_Meth and m_Code will automatically clear
   m_pObjectPrivate = NULL;
}

CMIFLFunc::~CMIFLFunc (void)
{
   // do nothing for now
}

/*************************************************************************************
CMIFLFunc::Clear - Wipes the parameter to intial state
*/
void CMIFLFunc::Clear()
{
   m_Meth.Clear();
   m_Code.Clear();
}

/*************************************************************************************
CMIFLFunc::CloneTo - Stanrdard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
BOOL CMIFLFunc::CloneTo (CMIFLFunc *pTo, BOOL fKeepDocs)
{
   // NOTE: Not calling pTo->clear() because m_Meth and m_Code will automatically do

   m_Meth.CloneTo (&pTo->m_Meth, fKeepDocs);
   m_Code.CloneTo (&pTo->m_Code);
   return TRUE;
}


/*************************************************************************************
CMIFLFunc::Clone - Stanrdard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
CMIFLFunc *CMIFLFunc::Clone (BOOL fKeepDocs)
{
   PCMIFLFunc pNew = new CMIFLFunc;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDocs)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



static PWSTR gpszMIFLFunc = L"MIFLFunc";
static PWSTR gpszCode = L"Code";
static PWSTR gpszMeth = L"Meth";

/*************************************************************************************
CMIFLFunc::MMLTo - Stanrdard API
*/
PCMMLNode2 CMIFLFunc::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLFunc);

   PCMMLNode2 pSub;

   pSub = m_Meth.MMLTo ();
   if (!pSub) {
      delete pNode;
      return FALSE;
   }
   pSub->NameSet (gpszMeth);
   pNode->ContentAdd (pSub);

   pSub = m_Code.MMLTo ();
   if (!pSub) {
      delete pNode;
      return FALSE;
   }
   pSub->NameSet (gpszCode);
   pNode->ContentAdd (pSub);

   return pNode;
}


/*************************************************************************************
CMIFLFunc::MMLTo - Stanrdard API
*/
BOOL CMIFLFunc::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   PWSTR psz;
   PCMMLNode2 pSub;

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszCode), &psz, &pSub);
   if (pSub)
      m_Code.MMLFrom (pSub);

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszMeth), &psz, &pSub);
   if (pSub)
      m_Meth.MMLFrom (pSub);

   return TRUE;
}


/*********************************************************************************
FuncEditPage - UI
*/

BOOL FuncEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLFunc pFunc = pmp->pFunc;
   PCMIFLMeth pMeth = pmp->pMeth;
   PCMIFLCode pCode = pmp->pCode;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // initialize params
         pCode->PageInitPage (pPage, pmp);
         pMeth->PageInitPage (pPage, pmp);

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

         if (pCode->PageButtonPress (pPage, pmp, p, pLib))
            return TRUE;
         if (pMeth->PageButtonPress (pPage, pmp, p, pLib))
            return TRUE;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this function?"))
               return TRUE;

            // remove the function
            pLib->FuncRemove (pFunc->m_Meth.m_dwTempID);

            // set a link to go to the Func list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dfunclist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated function.");

            // duplicate
            DWORD dwID = pLib->FuncAddCopy (pFunc);

            // set a link to go to the Func list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dfunc:%dedit", pLib->m_dwTempID, dwID);
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
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the function?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The function will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->FuncAddCopy (pFunc);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->FuncRemove (pFunc->m_Meth.m_dwTempID);

            // set a link to go to the Func list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dfunc:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         BOOL fRename;

         if (pCode->PageEditChange (pPage, pmp, p, pLib))
            return TRUE;

         if (pMeth->PageEditChange (pPage, pmp, p, pLib, &fRename)) {
            // if renamed then need to resort the list
            if (fRename) {
               pLib->AboutToChange();
               pLib->FuncSort();
               pLib->Changed();
            }

            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (pMeth->PageComboBoxSelChange (pPage, pmp, p, pmp->pLib))
            return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;

         PWSTR pszIndex = L"index:";
         DWORD dwIndexLen = (DWORD)wcslen(pszIndex);
         if (!wcsncmp (psz, pszIndex, dwIndexLen)) {
            pProj->HelpIndex (pPage, p->psz + dwIndexLen);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (pCode->PageSubstitution(pPage, pmp, p))
            return TRUE;
         if (pMeth->PageSubstitution (pPage, pmp, p, pLib, pLib->m_fReadOnly))
            return TRUE;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Modify function - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MIFLTABS")) {
            PWSTR apsz[] = {L"Description", L"Parameters", L"Code", L"Misc."};
            PWSTR apszHelp[] = {
               L"Lets you modify the function's name and description.",
               L"Lets you modify the function's parameters.",
               L"Lets you modify the function's code.",
               L"Lets you do miscellaneous changes to the function."
            };
            p->pszSubString = MIFLTabs (*pmp->pdwTab,
               sizeof(apsz)/sizeof(PWSTR), apsz, apszHelp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->FuncOverridden (pLib->m_dwTempID, (PWSTR)pFunc->m_Meth.m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by function in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides function in ");
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
CMIFLFunc::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library that the function is part of
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dfunc%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLFunc::DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLFUNCEDIT;
   PESCPAGECALLBACK pPage = FuncEditPage;

   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLFUNCEDIT;
      pPage = FuncEditPage;
   }

   PWSTR pszRet;
   mp.pFunc = this;
   mp.pCode = &m_Code;
   mp.pMeth = &m_Meth;
   mp.pLib = pLib;
   mp.pProj = mp.pLib->ProjectGet();
   mp.pszErrLink = pszNext;   // point back to self
   mp.pdwTab = &mp.pProj->m_dwTabFunc;   // set the tab
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





/*********************************************************************************
ObjMethEditPage - UI
*/

BOOL ObjMethEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLFunc pFunc = pmp->pFunc;
   PCMIFLMeth pMeth = pmp->pMeth;
   PCMIFLCode pCode = pmp->pCode;
   PCMIFLObject pObj = pmp->pObj;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // initialize params
         pCode->PageInitPage (pPage, pmp);
         pMeth->PageInitPage (pPage, pmp);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (pCode->PageButtonPress (pPage, pmp, p, pLib))
            return TRUE;
         if (pMeth->PageButtonPress (pPage, pmp, p, pLib))
            return TRUE;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this method?"))
               return TRUE;

            // remove the function
            if (pmp->dwFlag)
               pObj->MethPrivRemove (pFunc->m_Meth.m_dwTempID, pLib);
            else
               pObj->MethPubRemove (pFunc->m_Meth.m_dwTempID, pLib);

            // set a link to go to the Func list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit", pLib->m_dwTempID, pObj->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"swapprivpub")) {
            // find out which library this exists in
            DWORD i;
            PCMIFLLib pLibUse;
            PCMIFLMeth pMethDef = NULL;
            for (i = 0; i < pProj->LibraryNum(); i++) {
               pLibUse = pProj->LibraryGet(i);
               pMethDef = pLibUse->MethDefGet(pLibUse->MethDefFind((PWSTR)pFunc->m_Meth.m_memName.p, -1));
               if (pMethDef)
                  break;
            }

            if (pmp->dwFlag) {
               // it's private, so convert to public

               // make sure it exists as a public method...
               if (!pMethDef) {
                  if (IDYES != pPage->MBYesNo (L"Do you want to create a new method definition?",
                     L"The method definition doesn't exist. You must create one in order to convert "
                     L"from a private to public method."))
                     return TRUE;

                  // if no name then error
                  PWSTR pszName = (PWSTR) pFunc->m_Meth.m_memName.p;
                  if (!pszName || !pszName[0]) {
                     pPage->MBWarning (L"You must have a name for the method.");
                     return TRUE;
                  }

                  // add new method definition...
                  pLib->MethDefAddCopy (&pFunc->m_Meth);
               }

               // convert...
               DWORD dwTempID = pObj->MethPubAddCopy (pFunc, pLib);
               if (dwTempID == -1)
                  return TRUE;
               pObj->MethPrivRemove (pFunc->m_Meth.m_dwTempID, pLib);

               // swap to new link...
               WCHAR szTemp[64];
               swprintf (szTemp, L"lib:%dobject:%dmethpub:%dedit", pLib->m_dwTempID, pObj->m_dwTempID, dwTempID);

               EscChime (ESCCHIME_INFORMATION);
               pPage->Exit (szTemp);
               return TRUE;
            }
            else {
               // it's public, so convert to private
               if (!pMethDef) {
                  pPage->MBWarning (L"The method definition doesn't exist.");
                  return TRUE;
               }

               // copy this temporarily so can muck with
               CMIFLFunc fTemp;
               pFunc->CloneTo (&fTemp);
               pMethDef->CloneTo (&fTemp.m_Meth);  // so have entire method

               // add and remove
               DWORD dwTempID = pObj->MethPrivAddCopy (&fTemp, pLib);
               if (dwTempID == -1)
                  return TRUE;
               pObj->MethPubRemove (pFunc->m_Meth.m_dwTempID, pLib);

               // swap to new link...
               WCHAR szTemp[64];
               swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit", pLib->m_dwTempID, pObj->m_dwTempID, dwTempID);

               EscChime (ESCCHIME_INFORMATION);
               pPage->Exit (szTemp);
               return TRUE;
            }
            return TRUE;   // shouldnt get here
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            // only do this for private methods
            if (!pmp->dwFlag)
               return TRUE;

            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated method.");

            // duplicate
            DWORD dwID = pObj->MethPrivAddCopy (pFunc, pLib);

            // set a link to go to the Func list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit", pLib->m_dwTempID, pObj->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         BOOL fRename;

         if (pCode->PageEditChange (pPage, pmp, p, pLib))
            return TRUE;

         if (pMeth->PageEditChange (pPage, pmp, p, pLib, &fRename)) {
            // if renamed then need to resort the list
            if (fRename) {
               pLib->AboutToChange();
               pLib->FuncSort();
               pLib->Changed();
            }

            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (pMeth->PageComboBoxSelChange (pPage, pmp, p, pmp->pLib))
            return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;

         PWSTR pszIndex = L"index:";
         DWORD dwIndexLen = (DWORD)wcslen(pszIndex);
         if (!wcsncmp (psz, pszIndex, dwIndexLen)) {
            pProj->HelpIndex (pPage, p->psz + dwIndexLen);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (pCode->PageSubstitution(pPage, pmp, p))
            return TRUE;
         if (pMeth->PageSubstitution (pPage, pmp, p, pLib, pLib->m_fReadOnly || !pmp->dwFlag))
            return TRUE;

         if (!_wcsicmp(p->pszSubName, L"SWAPPRIVPUB")) {
            if (pLib->m_fReadOnly)
               return TRUE;   // do nothing

            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<xChoiceButton name=swapprivpub><bold>");
            MemCat (&gMemTemp, pmp->dwFlag ?
               L"Convert from a private to a public method" :
               L"Convert from a public to a private method");
            MemCat (&gMemTemp, L"</bold><br/>");
            MemCat (&gMemTemp, L"This lets you swap the method from being private "
               L"to public, or vice versa.");
            if (!pmp->dwFlag)
               MemCat (&gMemTemp, L" You may wish to rename the newly-private method.");
            MemCat (&gMemTemp, L"</xChoiceButton>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MIFLTABS")) {
            PWSTR apsz[] = {L"Description", L"Parameters", L"Code", L"Misc."};
            PWSTR apszHelp[] = {
               L"Lets you modify the method's name and description.",
               L"Lets you modify the method's parameters.",
               L"Lets you modify the method's code.",
               L"Lets you do miscellaneous changes to the method."
            };
            p->pszSubString = MIFLTabs (*pmp->pdwTab,
               sizeof(apsz)/sizeof(PWSTR), apsz, apszHelp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBPRIVENABLE")) {
            // if dwFlag == 0, then is a public function, so disable
            if ((pLib && pLib->m_fReadOnly) || !pmp->dwFlag)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBPRIVREADONLY")) {
            // if dwFlag == 0, then is a public function, so disable
            if ((pLib && pLib->m_fReadOnly) || !pmp->dwFlag)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFMENULIBPRIVRO")) {
            if ((pLib && pLib->m_fReadOnly) || !pmp->dwFlag)
               p->pszSubString = L"<comment>";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFMENULIBPRIVRO")) {
            if ((pLib && pLib->m_fReadOnly) || !pmp->dwFlag)
               p->pszSubString = L"</comment>";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"Modify method - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}




/********************************************************************************
CMIFLFunc::DialogEditSingleMethod - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   BOOL              fPriv - Set to TRUE if is private method, FALSE if public
   PCMIFLLib         pLib - Library that the function is part of
   PCMIFLObject      pObj - Object that this function is part of
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dfunc%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLFunc::DialogEditSingleMethod (BOOL fPriv, PCMIFLLib pLib, PCMIFLObject pObj,
                                        PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLOBJMETHPRIVEDIT;
   PESCPAGECALLBACK pPage = ObjMethEditPage;

   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLOBJMETHPRIVEDIT;
      pPage = ObjMethEditPage;
   }

   PWSTR pszRet;
   mp.pFunc = this;
   mp.pCode = &m_Code;
   mp.pMeth = &m_Meth;
   mp.pLib = pLib;
   mp.pProj = mp.pLib->ProjectGet();
   mp.pObj = pObj;
   mp.dwFlag = (DWORD)fPriv;
   mp.pszErrLink = pszNext;   // point back to self
   mp.pdwTab = &mp.pProj->m_dwTabFunc;   // set the tab
   mp.iVScroll = *piVScroll;

   // if modifying a public methoth, then mp.pMeth must point to the public method
   if (!fPriv) {
      mp.pMeth = mp.pProj->MethDefUsed ((PWSTR)m_Meth.m_memName.p);
      if (!mp.pMeth)
         mp.pMeth = &m_Meth;
   }
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


/********************************************************************************
CMIFLFunc::KillDead - Loops through all the objects in the library and a) kills dead
parameters, and b) kills resources/strings without any info.

inputs
   PCMIFLLib         pLib - Library to use. Can be NULL.
   BOOL              fLowerCase - If TRUE this lowercases all the names
*/
void CMIFLFunc::KillDead (PCMIFLLib pLib, BOOL fLowerCase)
{

   // pass this onto the method...
   m_Meth.KillDead(pLib, fLowerCase);

#if 0 // not needed because m_Meth.KillDead does
   if (pLib)
      pLib->AboutToChange();

   // kill dead properties
   DWORD i;
   PCMIFLProp *ppp = (PCMIFLProp*)m_Meth.m_lPCMIFLProp.Get(0);
   for (i = m_Meth.m_lPCMIFLProp.Num()-1; i < m_Meth.m_lPCMIFLProp.Num(); i--) { // BUGFIX - was i++
      PWSTR psz = (PWSTR)ppp[i]->m_memName.p;
      if (!psz[0]) {
         // delete this
         delete ppp[i];
         m_Meth.m_lPCMIFLProp.Remove (i);
         ppp = (PCMIFLProp*)m_Meth.m_lPCMIFLProp.Get(0);
         continue;
      }

      // pass on
      if (fLowerCase)
         MIFLToLower ((PWSTR)ppp[i]->m_memName.p);
   } // i

   // lower case own name
   if (fLowerCase)
      MIFLToLower ((PWSTR)m_Meth.m_memName.p);

   if (pLib)
      pLib->Changed();
#endif // 0
}

