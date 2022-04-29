/*************************************************************************************
CMIFLProp.cpp - Definition for a parameter

begun 31/12/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


/*************************************************************************************
CMIFLProp::Constructor and destructor
*/
CMIFLProp::CMIFLProp (void)
{
   m_pCodeGet = m_pCodeSet = NULL;
   m_pObjectFrom = NULL;

   Clear();
}

CMIFLProp::~CMIFLProp (void)
{
   // clear code
   if (m_pCodeGet)
      delete m_pCodeGet;
   if (m_pCodeSet)
      delete m_pCodeSet;
}

/*************************************************************************************
CMIFLProp::Clear - Wipes the parameter to intial state
*/
void CMIFLProp::Clear()
{
   m_dwTempID = MIFLTempID ();
   m_dwLibOrigFrom = 0;

   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   MemZero (&m_aMemHelp[0]);
   MemZero (&m_aMemHelp[1]);
   MemZero (&m_memInit);

   // clear code
   if (m_pCodeGet)
      delete m_pCodeGet;
   if (m_pCodeSet)
      delete m_pCodeSet;
   m_pCodeGet = m_pCodeSet = NULL;
}

/*************************************************************************************
CMIFLProp::CloneTo - Stanrdard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
BOOL CMIFLProp::CloneTo (CMIFLProp *pTo, BOOL fKeepDocs)
{
   pTo->Clear();

   pTo->m_dwTempID = m_dwTempID;
   pTo->m_dwLibOrigFrom = m_dwLibOrigFrom;
   // NOTE: Not bothering with m_pObjectFrom

   if (((PWSTR)m_memName.p)[0])
      MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   if (fKeepDocs) {
      if (((PWSTR)m_memDescShort.p)[0])
         MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
      if (((PWSTR)m_memDescLong.p)[0])
         MemCat (&pTo->m_memDescLong, (PWSTR)m_memDescLong.p);
      if (((PWSTR)m_aMemHelp[0].p)[0])
         MemCat (&pTo->m_aMemHelp[0], (PWSTR)m_aMemHelp[0].p);
      if (((PWSTR)m_aMemHelp[1].p)[0])
         MemCat (&pTo->m_aMemHelp[1], (PWSTR)m_aMemHelp[1].p);
   }
   if (((PWSTR)m_memInit.p)[0])
      MemCat (&pTo->m_memInit, (PWSTR)m_memInit.p);

   // clone code
   if (m_pCodeGet)
      pTo->m_pCodeGet = m_pCodeGet->Clone();
   if (m_pCodeSet)
      pTo->m_pCodeSet = m_pCodeSet->Clone();

   return TRUE;
}


/*************************************************************************************
CMIFLProp::Clone - Stanrdard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
CMIFLProp *CMIFLProp::Clone (BOOL fKeepDocs)
{
   PCMIFLProp pNew = new CMIFLProp;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDocs)) {
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
static PWSTR gpszInit = L"Init";
static PWSTR gpszCodeGet = L"CodeGet";
static PWSTR gpszCodeSet = L"CodeSet";

/*************************************************************************************
CMIFLProp::MMLTo - Stanrdard API
*/
PCMMLNode2 CMIFLProp::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLParam);

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
   if (m_memInit.p && ((PWSTR)m_memInit.p)[0])
      MMLValueSet (pNode, gpszInit, (PWSTR)m_memInit.p);

   // write code
   PCMMLNode2 pSub;
   if (m_pCodeGet) {
      pSub = m_pCodeGet->MMLTo();
      if (!pSub) {
         delete pNode;
         return NULL;
      }
      pSub->NameSet (gpszCodeGet);
      pNode->ContentAdd (pSub);
   }
   if (m_pCodeSet) {
      pSub = m_pCodeSet->MMLTo();
      if (!pSub) {
         delete pNode;
         return NULL;
      }
      pSub->NameSet (gpszCodeSet);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/*************************************************************************************
CMIFLProp::MMLTo - Stanrdard API
*/
BOOL CMIFLProp::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

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

   psz = MMLValueGet (pNode, gpszInit);
   if (psz)
      MemCat (&m_memInit, psz);

   // get the code
   PCMMLNode2 pSub;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszCodeGet), &psz, &pSub);
   if (pSub) {
      m_pCodeGet = new CMIFLCode;
      if (!m_pCodeGet)
         return FALSE;
      m_pCodeGet->MMLFrom (pSub);
   }
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszCodeSet), &psz, &pSub);
   if (pSub) {
      m_pCodeSet = new CMIFLCode;
      if (!m_pCodeSet)
         return FALSE;
      m_pCodeSet->MMLFrom (pSub);
   }

   return TRUE;
}

/*************************************************************************************
CMIFLProp::PageEditChange - Call this when an edit change notification comes in.
It will see if it affects any of the Propod parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNEDITCHANGE   p - Edit change information
   PCMIFLLib         pLib - Library that this is contained in
   BOOL              *pfChangedName - Will fill with TRUE if changed then Propod's
                     name, which would require a resort of the parent
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLProp::PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p,
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
   else if (!_wcsicmp(psz, L"init"))
      pMem = &m_memInit;

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
CMIFLProp::PageInitPage - This functions handles the init-page call for a Propod.
This allows the code to be used for not only the Propod defiitions, but also functions,
and private Propods.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
returns
   none
*/
void CMIFLProp::PageInitPage (PCEscPage pPage, PMIFLPAGE pmp)
{
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLProp pProp = pmp->pProp;

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

   pControl = pPage->ControlFind (L"init");
   if (pControl && m_memInit.p)
      pControl->AttribSet (Text(), (PWSTR)m_memInit.p);

   // code buttons
   BOOL fCode = (m_pCodeGet || m_pCodeSet);
   pControl = pPage->ControlFind (L"getsetuse");
   if (pControl)
      pControl->AttribSetBOOL (Checked(), fCode);
   pControl = pPage->ControlFind (L"codeget");
   if (pControl)
      pControl->Enable (fCode);
   pControl = pPage->ControlFind (L"codeset");
   if (pControl)
      pControl->Enable (fCode);
}



/*************************************************************************************
CMIFLProp::CodeClear - Wipes out anything in the code

returns
   none
*/
void CMIFLProp::CodeClear (void)
{
   // deleting
   if (m_pCodeGet)
      delete m_pCodeGet;
   if (m_pCodeSet)
      delete m_pCodeSet;
   m_pCodeGet = m_pCodeSet = NULL;
}


/*************************************************************************************
CMIFLProp::CodeDefault - Initializes the code to the default get and set values

returns
   none
*/
void CMIFLProp::CodeDefault (void)
{
   // creating
   if (!m_pCodeGet) {
      m_pCodeGet = new CMIFLCode;
      if (m_pCodeGet) {
         // initaluze to line of code
         MemZero (&m_pCodeGet->m_memCode);
         MemCat (&m_pCodeGet->m_memCode, L"return ");
         MemCat (&m_pCodeGet->m_memCode, (PWSTR)m_memName.p);
         MemCat (&m_pCodeGet->m_memCode, L";");
      }
   }
   if (!m_pCodeSet) {
      m_pCodeSet = new CMIFLCode;
      if (m_pCodeSet) {
         // initaluze to line of code
         MemZero (&m_pCodeSet->m_memCode);
         MemCat (&m_pCodeSet->m_memCode, (PWSTR)m_memName.p);
         MemCat (&m_pCodeSet->m_memCode, L" = Value;");
      }
   }
}

/*************************************************************************************
CMIFLProp::PageButtonPress - Call this when an button press notification comes in.
It will see if it affects any of the method parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNBUTTONPRESS  p - Buttonpress information
   PCMIFLLib         pLib - Library that this is contained in
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLProp::PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   if (!_wcsicmp(psz, L"getsetuse")) {
      BOOL fCode = (m_pCodeGet || m_pCodeSet);
      fCode = !fCode;

      pLib->AboutToChange ();

      // change the object
      if (!fCode)
         CodeClear();
      else
         CodeDefault ();

      pLib->Changed ();

      // enable the windows
      PCEscControl pControl;
      pControl = pPage->ControlFind (L"codeget");
      if (pControl)
         pControl->Enable (fCode);
      pControl = pPage->ControlFind (L"codeset");
      if (pControl)
         pControl->Enable (fCode);

      return TRUE;
   }
   else if (!_wcsicmp(psz, L"codeget")) {
      WCHAR szTemp[64];
      // NOTE: ASsuming that modifying a global. since it should be the only way to get here
      swprintf (szTemp, L"lib:%dglobal:%dget", pLib->m_dwTempID, m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"codeset")) {
      WCHAR szTemp[64];
      // NOTE: ASsuming that modifying a global. since it should be the only way to get here
      swprintf (szTemp, L"lib:%dglobal:%dset", pLib->m_dwTempID, m_dwTempID);
      pPage->Exit (szTemp);
      return TRUE;
   }

   return FALSE;
}



/*********************************************************************************
PropDefEditPage - UI
*/

BOOL PropDefEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLProp pProp = pmp->pProp;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // initialize params
         pProp->PageInitPage (pPage, pmp);

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

         if (pProp->PageButtonPress (pPage, pmp, p, pLib))
            return TRUE;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this property?"))
               return TRUE;

            // remove the Propod
            pLib->PropDefRemove (pProp->m_dwTempID);

            // set a link to go to the Propod list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dpropdeflist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated property.");

            // duplicate
            DWORD dwID = pLib->PropDefAddCopy (pProp);

            // set a link to go to the Propod list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dpropdef:%dedit", pLib->m_dwTempID, dwID);
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
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the property definition?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The property definition will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->PropDefAddCopy (pProp);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->PropDefRemove (pProp->m_dwTempID);

            // set a link to go to the Propod list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dpropdef:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         BOOL fRename;

         if (pProp->PageEditChange (pPage, pmp, p, pLib, &fRename)) {
            // if renamed then need to resort the list
            if (fRename) {
               pLib->AboutToChange();
               pLib->PropDefSort();
               pLib->Changed();
            }

            return TRUE;
         }
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

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Modify property definition - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pProp->m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pProp->m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->PropDefOverridden (pLib->m_dwTempID, (PWSTR)pProp->m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by property in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides property in ");
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




/*********************************************************************************
GlobalEditPage - UI
*/

BOOL GlobalEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLProp pProp = pmp->pProp;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // initialize params
         pProp->PageInitPage (pPage, pmp);

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

         if (pProp->PageButtonPress (pPage, pmp, p, pLib))
            return TRUE;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this global variable?"))
               return TRUE;

            // remove the global
            pLib->GlobalRemove (pProp->m_dwTempID);

            // set a link to go to the global list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dgloballist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated global variable.");

            // duplicate
            DWORD dwID = pLib->GlobalAddCopy (pProp);

            // set a link to go to the global list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dglobal:%dedit", pLib->m_dwTempID, dwID);
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
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the global variable?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The global variable will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->GlobalAddCopy (pProp);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->GlobalRemove (pProp->m_dwTempID);

            // set a link to go to the global list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dglobal:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         BOOL fRename;

         if (pProp->PageEditChange (pPage, pmp, p, pLib, &fRename)) {
            // if renamed then need to resort the list
            if (fRename) {
               pLib->AboutToChange();
               pLib->GlobalSort();
               pLib->Changed();
            }

            return TRUE;
         }
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

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Modify global variable - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pProp->m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pProp->m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->GlobalOverridden (pLib->m_dwTempID, (PWSTR)pProp->m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by global in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides global in ");
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





/*********************************************************************************
PropGetSetEditPage - UI
*/

BOOL PropGetSetEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLProp pProp = pmp->pProp;
   PCMIFLCode pCode = pmp->pCode;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // set the descriptions...
         if (pmp->dwFlag) {
            // setting
            pControl = pPage->ControlFind (L"paramname0");
            if (pControl)
               pControl->AttribSet (Text(), L"Value");
            pControl = pPage->ControlFind (L"paramdesc0");
            if (pControl)
               pControl->AttribSet (Text(),
                  L"This is the value that is attribute is being set to. "
                  L"It is up to the set function, however, to actually set the value.");
         }
         else {
            // getting
            pControl = pPage->ControlFind (L"retdesc");
            if (pControl)
               pControl->AttribSet (Text(),
                  L"This is the value that will be used when the variable/property is accessed. "
                  L"The code must return a value.");
         }

         // init by code
         pCode->PageInitPage (pPage, pmp);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (pCode->PageButtonPress (pPage, pmp, p, pmp->pLib))
            return TRUE;
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         
         if (pCode->PageEditChange (pPage, pmp, p, pmp->pLib))
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

         if (pCode->PageSubstitution (pPage, pmp, p))
            return TRUE;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, pmp->dwFlag ? L"Set function - " : L"Get function - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pProp->m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pProp->m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSET")) {
            p->pszSubString = pmp->dwFlag ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSET")) {
            p->pszSubString = pmp->dwFlag ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFGET")) {
            p->pszSubString = !pmp->dwFlag ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFGET")) {
            p->pszSubString = !pmp->dwFlag ? L"" : L"</comment>";
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}




/********************************************************************************
CMIFLProp::DialogEditSinglePropDef - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library that the property is part of
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dpropdef%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLProp::DialogEditSinglePropDef (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLPROPDEFEDIT;
   PESCPAGECALLBACK pPage = PropDefEditPage;

   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLPROPDEFEDIT;
      pPage = PropDefEditPage;
   }

   PWSTR pszRet;
   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pProp = this;
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



/********************************************************************************
CMIFLProp::DialogEditSingleGlobal - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library that the global is part of
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dglobal%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLProp::DialogEditSingleGlobal (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLGLOBALEDIT;
   PESCPAGECALLBACK pPage = GlobalEditPage;
   CMIFLMeth Meth;

   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLGLOBALEDIT;
      pPage = GlobalEditPage;
   }
   else if (!_wcsicmp(pszUse, L"get") && m_pCodeGet) {
      dwPage = IDR_MMLPROPGETSETEDIT;
      pPage = PropGetSetEditPage;
      mp.pCode = m_pCodeGet;
      mp.dwFlag = 0; // so know getting
      Meth.InitAsGetSet ((PWSTR)m_memName.p, TRUE, TRUE);
      mp.pMeth = &Meth;
   }
   else if (!_wcsicmp(pszUse, L"set") && m_pCodeGet) {
      dwPage = IDR_MMLPROPGETSETEDIT;
      pPage = PropGetSetEditPage;
      mp.pCode = m_pCodeSet;
      mp.dwFlag = 1; // so know setting
      Meth.InitAsGetSet ((PWSTR)m_memName.p, FALSE, TRUE);
      mp.pMeth = &Meth;
   }

   PWSTR pszRet;
   mp.pProp = this;
   mp.pLib = pLib;
   mp.pProj = mp.pLib->ProjectGet();
   mp.pszErrLink = pszNext;   // point back to self
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

