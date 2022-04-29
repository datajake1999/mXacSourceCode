/*************************************************************************************
CMIFLMeth.cpp - Definition for a method

begun 31/12/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


/*************************************************************************************
CMIFLMeth::Constructor and destructor
*/
CMIFLMeth::CMIFLMeth (void)
{
   m_lPCMIFLProp.Init (sizeof(PCMIFLProp));
   //m_pLib = NULL;

   Clear();
}

CMIFLMeth::~CMIFLMeth (void)
{
   Clear();
}


/*************************************************************************************
CMIFLMeth::Clear - Clears out all values to their intiial value
*/
void CMIFLMeth::Clear (void)
{
   m_dwTempID = MIFLTempID ();
   m_dwLibOrigFrom = 0;

   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
   for (i = 0; i < m_lPCMIFLProp.Num(); i++)
      delete pp[i];
   m_lPCMIFLProp.Clear();

   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   MemZero (&m_aMemHelp[0]);
   MemZero (&m_aMemHelp[1]);
   m_mpRet.Clear();
   m_fParamAnyNum = FALSE;
   m_fCommonAll = FALSE;
   m_dwOverride = 0;
}



/*************************************************************************************
CMIFLMeth::CloneTo - Standard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
BOOL CMIFLMeth::CloneTo (CMIFLMeth *pTo, BOOL fKeepDocs)
{
   pTo->Clear();

   pTo->m_dwTempID = m_dwTempID;
   pTo->m_dwLibOrigFrom = m_dwLibOrigFrom;

   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   if (fKeepDocs) {
      MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
      MemCat (&pTo->m_memDescLong, (PWSTR)m_memDescLong.p);
      MemCat (&pTo->m_aMemHelp[0], (PWSTR)m_aMemHelp[0].p);
      MemCat (&pTo->m_aMemHelp[1], (PWSTR)m_aMemHelp[1].p);
   }

   pTo->m_lPCMIFLProp.Init (sizeof(PCMIFLProp), m_lPCMIFLProp.Get(0), m_lPCMIFLProp.Num());
   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*) pTo->m_lPCMIFLProp.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLProp.Num(); i++)
      pp[i] = pp[i]->Clone(fKeepDocs);

   m_mpRet.CloneTo (&pTo->m_mpRet, fKeepDocs);
   pTo->m_fParamAnyNum = m_fParamAnyNum;
   pTo->m_fCommonAll = m_fCommonAll;
   pTo->m_dwOverride = m_dwOverride;
   //pTo->m_pLib = m_pLib;

   return TRUE;
}




/*************************************************************************************
CMIFLMeth::Clone - Standard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
CMIFLMeth *CMIFLMeth::Clone (BOOL fKeepDocs)
{
   PCMIFLMeth pNew = new CMIFLMeth;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDocs)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


static PWSTR gpszMIFLMeth = L"MIFLMeth";
static PWSTR gpszName = L"Name";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszDescLong = L"DescLong";
static PWSTR gpszHelp0 = L"Help0";
static PWSTR gpszHelp1 = L"Help1";
static PWSTR gpszParamAnyNum = L"ParamAnyNum";
static PWSTR gpszCommonAll = L"CommonAll";
static PWSTR gpszOverride = L"Override";
static PWSTR gpszRet = L"Ret";
static PWSTR gpszMIFLParam = L"MIFLParam";

/*************************************************************************************
CMIFLMeth::MMLTo - Standard API
*/
PCMMLNode2 CMIFLMeth::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLMeth);

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

   if (m_fParamAnyNum)
      MMLValueSet (pNode, gpszParamAnyNum, (int)m_fParamAnyNum);
   if (m_fCommonAll)
      MMLValueSet (pNode, gpszCommonAll, (int)m_fCommonAll);
   if (m_dwOverride)
      MMLValueSet (pNode, gpszOverride, (int)m_dwOverride);

   PCMMLNode2 pSub;
   pSub = m_mpRet.MMLTo();
   if (!pSub) {
      delete pNode;
      return NULL;
   }
   pSub->NameSet (gpszRet);
   pNode->ContentAdd (pSub);

   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
   for (i = 0; i < m_lPCMIFLProp.Num(); i++) {
      pSub = pp[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLParam);
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*************************************************************************************
CMIFLMeth::MMLTo - Standard API
*/
BOOL CMIFLMeth::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);
   psz = MMLValueGet (pNode, gpszDescLong);
   if (psz)
      MemCat (&m_memDescLong, psz);
   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);
   psz = MMLValueGet (pNode, gpszHelp0);
   if (psz)
      MemCat (&m_aMemHelp[0], psz);
   HACKRENAMEALL(&m_aMemHelp[0]);
   psz = MMLValueGet (pNode, gpszHelp1);
   if (psz)
      MemCat (&m_aMemHelp[1], psz);
   HACKRENAMEALL(&m_aMemHelp[1]);

   m_fParamAnyNum = (BOOL) MMLValueGetInt (pNode, gpszParamAnyNum, FALSE);
   m_fCommonAll = (BOOL) MMLValueGetInt (pNode, gpszCommonAll, FALSE);
   m_dwOverride = (DWORD) MMLValueGetInt (pNode, gpszOverride, 0);

   PCMMLNode2 pSub;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszRet)) {
         m_mpRet.MMLFrom (pSub);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLParam)) {
         PCMIFLProp pp = new CMIFLProp;
         if (!pp)
            continue;
         pp->MMLFrom (pSub);
         m_lPCMIFLProp.Add (&pp);
         continue;
      }
   } // i

   return TRUE;
}




#if 0 // dead code
/*************************************************************************************
CMIFLMeth::ParentSet - Sets the parent of this method.

inputs
   PCMIFLLib         pLib - Parent is a library, which means this is a global method defintion
*/
void CMIFLMeth::ParentSet (PCMIFLLib pLib)
{
   m_pLib = pLib;
   // NOTE - will need to clear other parents
}



/*************************************************************************************
CMIFLMeth::ParentGetLib - Returns the library (if the parent is one) that's the parent
*/
PCMIFLLib CMIFLMeth::ParentGetLib (void)
{
   return m_pLib;
}



/*************************************************************************************
CMIFLMeth::ParentGetOnlyLib - Works its way up the chain until it gets to the library
of the parent.
*/
PCMIFLLib CMIFLMeth::ParentGetOnlyLib (void)
{
   if (m_pLib)
      return m_pLib;
   // NOTE - if other types of parents will need to work way up chain
   return NULL;
}

#endif // 0


/*************************************************************************************
CMIFLMeth::PageComboBoxSelChange - Call this when an ESCN_COMBOBOXSELCHANGE notification comes in.
It will see if it affects any of the method parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNCOMBOBOXSELCHANGE  p - Comboox information
   PCMIFLLib         pLib Library to use
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLMeth::PageComboBoxSelChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNCOMBOBOXSELCHANGE p, PCMIFLLib pLib)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   // library
   //PCMIFLLib pLib = ParentGetOnlyLib();

   if (!_wcsicmp(psz, L"override")) {
      DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
      if (dwVal == m_dwOverride)
         return TRUE;   // no change

      pLib->AboutToChange ();
      m_dwOverride = dwVal;
      pLib->Changed ();
      return TRUE;
   }

   return FALSE;
}

/*************************************************************************************
CMIFLMeth::PageButtonPress - Call this when an button press notification comes in.
It will see if it affects any of the method parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNBUTTONPRESS  p - Buttonpress information
   PCMIFLLib         pLib - Library to use
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLMeth::PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   // library
   // PCMIFLLib pLib = ParentGetOnlyLib();

   if (!_wcsicmp(psz, L"anynumparam")) {
      pLib->AboutToChange ();
      m_fParamAnyNum = p->pControl->AttribGetBOOL (Checked());
      pLib->Changed ();
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"commontoall")) {
      pLib->AboutToChange ();
      m_fCommonAll = p->pControl->AttribGetBOOL (Checked());
      pLib->Changed ();
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"addparam")) {
      pPage->Exit (MIFLRedoSamePage());
      return TRUE;
   }

   return FALSE;
}


/*************************************************************************************
CMIFLMeth::PageEditChange - Call this when an edit change notification comes in.
It will see if it affects any of the method parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNEDITCHANGE   p - Edit change information
   PCMIFLLib         pLib - Library to use
   BOOL              *pfChangedName - Will fill with TRUE if changed then method's
                     name, which would require a resort of the parent
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLMeth::PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib, BOOL *pfChangedName)
{
   // init
   *pfChangedName = FALSE;

   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   // how big is this?
   DWORD dwNeed = 0;
   p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);

   // library
   //PCMIFLLib pLib = ParentGetOnlyLib();

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
   else if (!_wcsicmp(psz, L"retdesc"))
      pMem = &m_mpRet.m_memDescShort;

   BOOL fModParam = FALSE;

   if (!pMem) {
      // if gets to here then might be one of the parameters...
      PWSTR pszParamName = L"paramname", pszParamDesc = L"paramdesc";
      DWORD dwParamNameLen = (DWORD)wcslen(pszParamName), dwParamDescLen = (DWORD)wcslen(pszParamDesc);

      DWORD dwNum = 0;
      DWORD dwParam = 0;
      if (!_wcsnicmp(psz, pszParamName, dwParamNameLen)) {
         dwNum = _wtoi (psz + dwParamNameLen);
         dwParam = 1;
         fModParam = TRUE;
      }
      else if (!_wcsnicmp(psz, pszParamDesc, dwParamDescLen)) {
         dwNum = _wtoi (psz + dwParamDescLen);
         dwParam = 2;
         fModParam = TRUE;
      }

      // if need to add extra blank parameters then do so...
      while (dwParam && (dwNum >= m_lPCMIFLProp.Num())) {
         PCMIFLProp pNew = new CMIFLProp;
         if (pNew) {
            pLib->AboutToChange ();
            m_lPCMIFLProp.Add (&pNew);
            pLib->Changed ();
         }
      }

      // memory to be filled in...
      PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
      switch (dwParam) {
      case 1: // pszParamName
         pMem = &pp[dwNum]->m_memName;
         break;
      case 2: // pszParamDesc
         pMem = &pp[dwNum]->m_memDescShort;
         break;
      }
   }

   if (pMem) {
      if (!pMem->Required (dwNeed))
         return FALSE;

      pLib->AboutToChange ();
      p->pControl->AttribGet (Text(), (PWSTR)pMem->p, (DWORD)pMem->m_dwAllocated, &dwNeed);

      // if modified a parameter will need to delete dead parameters at the end of the list
      if (fModParam) while (m_lPCMIFLProp.Num()) {
         PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
         PCMIFLProp pm = pp[m_lPCMIFLProp.Num()-1];
         if (!((PWSTR)(pm->m_memName.p))[0] && !((PWSTR)(pm->m_memDescShort.p))[0] && !((PWSTR)(pm->m_memInit.p))[0]) {
            // name was set to nothing, so delete
            delete pm;
            m_lPCMIFLProp.Remove (m_lPCMIFLProp.Num()-1);
         }
         else
            break;
      }

      pLib->Changed ();
      return TRUE;
   }


   return FALSE;  // not it
}

/*************************************************************************************
CMIFLMeth::PageInitPage - This functions handles the init-page call for a method.
This allows the code to be used for not only the method defiitions, but also functions,
and private methods.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
returns
   none
*/
void CMIFLMeth::PageInitPage (PCEscPage pPage, PMIFLPAGE pmp)
{
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLMeth pMeth = pmp->pMeth;

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

   // loop through all the paramters
   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
   WCHAR szTemp[32];
   for (i = 0; i < m_lPCMIFLProp.Num(); i++) {
      swprintf (szTemp, L"paramname%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memName.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memName.p);

      swprintf (szTemp, L"paramdesc%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memDescShort.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memDescShort.p);
   } // i

   // return value description
   pControl = pPage->ControlFind (L"retdesc");
   if (pControl && m_mpRet.m_memDescShort.p)
      pControl->AttribSet (Text(), (PWSTR)m_mpRet.m_memDescShort.p);


   // buttons
   pControl = pPage->ControlFind (L"anynumparam");
   if (pControl)
      pControl->AttribSetBOOL (Checked(), m_fParamAnyNum);

   pControl = pPage->ControlFind (L"commontoall");
   if (pControl)
      pControl->AttribSetBOOL (Checked(), m_fCommonAll);


   // combo
   ComboBoxSet (pPage, L"override", m_dwOverride);
}



/*************************************************************************************
CMIFLMeth::PageSubstitution - This functions handles the ESCM_SUBSTITUTION call for a method.
This allows the code to be used for not only the method defiitions, but also functions,
and private methods.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCMSUBSTITUTION p - Substitution info
   PCMIFLLib         pLib - Library to use
   BOOL              fRO - If TRUE then treat as read only, else allow to modify.
                     Usually pass pLib->m_fReadOnly into this
returns
   BOOL - TRUE if captured and handled this, FALSE if processing should continue
*/
BOOL CMIFLMeth::PageSubstitution (PCEscPage pPage, PMIFLPAGE pmp, PESCMSUBSTITUTION p, PCMIFLLib pLib, BOOL fRO)
{
   PCMIFLProj pProj = pmp->pProj;
   // PCMIFLLib pLib = pmp->pLib;
   PCMIFLMeth pMeth = pmp->pMeth;

   if (!_wcsicmp(p->pszSubName, L"PARAMSLOT")) {
      MemZero (&gMemTemp);

      // how many slots?
      DWORD dwNum = fRO ? m_lPCMIFLProp.Num() : max(1, m_lPCMIFLProp.Num()+1);
      DWORD i;

      // PCMIFLLib pLib = ParentGetOnlyLib();

      for (i = 0; i < dwNum; i++) {
         MemCat (&gMemTemp, L"<tr>"
            L"<td width=33%><bold><edit width=100% maxchars=64 ");
         if (fRO)
            MemCat (&gMemTemp, L"readonly=true ");
         MemCat (&gMemTemp, L"name=paramname");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold></td>"
            L"<td width=66%><bold><edit width=100% maxchars=20000 multiline=true wordwrap=true ");
         if (fRO)
            MemCat (&gMemTemp, L"readonly=true ");
         MemCat (&gMemTemp, L"name=paramdesc");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold></td></tr>");
      } //i

      p->pszSubString = (PWSTR)gMemTemp.p;
      return TRUE;
   }

   // else, didn't handle
   return FALSE;
}

/*********************************************************************************
MethDefEditPage - UI
*/

BOOL MethDefEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLMeth pMeth = pmp->pMeth;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // initialize params
         pMeth->PageInitPage (pPage, pmp);

         // will need to set movelib combo
         MIFLComboLibs (pPage, L"movelib", L"move", pProj, pLib->m_dwTempID);
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (pMeth->PageButtonPress (pPage, pmp, p, pmp->pLib))
            return TRUE;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this method?"))
               return TRUE;

            // remove the method
            pLib->MethDefRemove (pMeth->m_dwTempID);

            // set a link to go to the method list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dmethdeflist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated method.");

            // duplicate
            DWORD dwID = pLib->MethDefAddCopy (pMeth);

            // set a link to go to the method list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dmethdef:%dedit", pLib->m_dwTempID, dwID);
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
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the method?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The method will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->MethDefAddCopy (pMeth);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->MethDefRemove (pMeth->m_dwTempID);

            // set a link to go to the method list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dmethdef:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         BOOL fRename;

         if (pMeth->PageEditChange (pPage, pmp, p, pmp->pLib, &fRename)) {
            // if renamed then need to resort the list
            if (fRename) {
               pLib->AboutToChange();
               pLib->MethDefSort();
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

         if (pMeth->PageSubstitution (pPage, pmp, p, pmp->pLib, pmp->pLib->m_fReadOnly))
            return TRUE;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp,L"Modify method definition - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pMeth->m_memName.p);

            // help link
            MemCat (&gMemTemp, L" ");
            pProj->HelpMML (&gMemTemp, (PWSTR)pMeth->m_memName.p);

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MIFLTABS")) {
            PWSTR apsz[] = {L"Description", L"Parameters", NULL, L"Misc."};
            PWSTR apszHelp[] = {
               L"Lets you modify the method definition's name and description.",
               L"Lets you modify the method definition's parameters.",
               NULL,
               L"Lets you do miscellaneous changes to the method definition."
            };
            p->pszSubString = MIFLTabs (*pmp->pdwTab,
               sizeof(apsz)/sizeof(PWSTR), apsz, apszHelp);
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->MethDefOverridden (pLib->m_dwTempID, (PWSTR)pMeth->m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by method in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides method in ");
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
CMIFLMeth::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library to use. Contains the method
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dmethdef%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLMeth::DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLMETHDEFEDIT;
   PESCPAGECALLBACK pPage = MethDefEditPage;

   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLMETHDEFEDIT;
      pPage = MethDefEditPage;
   }

   PWSTR pszRet;
   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pMeth = this;
   mp.pLib = pLib;
   mp.pProj = mp.pLib->ProjectGet();
   mp.iVScroll = *piVScroll;
redo:
   if (mp.pProj->m_dwTabFunc == 2)  // if pointing to code then change to name
      mp.pProj->m_dwTabFunc = 0;
   mp.pdwTab = &mp.pProj->m_dwTabFunc;   // set the tab

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



/*************************************************************************************
CMIFLMeth::MemCatParam - Concatenates the parameters of the method onto the current
CMem using MemCat() and MemCatSanitize() functions. Used to display the method paramters
in a list.

inputs
   PCMem       pMem - To concatenate to
   BOOL        fSpaceBefore - If TRUE then put a space before
returns
   none
*/
void CMIFLMeth::MemCatParam (PCMem pMem, BOOL fSpaceBefore)
{
   MemCat (pMem, fSpaceBefore ? L" (" : L"(");

   BOOL fAdded = FALSE;
   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
   for (i = 0; i < m_lPCMIFLProp.Num(); i++) {
      PWSTR psz = (PWSTR)pp[i]->m_memName.p;
      if (!psz || !psz[0])
         continue;

      if (fAdded)
         MemCat (pMem, L", ");
      fAdded = TRUE;

      MemCatSanitize (pMem, psz);
   } // i
   
   MemCat (pMem, L")");
}

/********************************************************************************
CMIFLMeth::KillDead - Loops through all the objects in the library and a) kills dead
parameters, and b) kills resources/strings without any info.

inputs
   PCMIFLLib         pLib - Library to use. Can be NULL.
   BOOL              fLowerCase - If TRUE this lowercases all the names
*/
void CMIFLMeth::KillDead (PCMIFLLib pLib, BOOL fLowerCase)
{
   if (pLib)
      pLib->AboutToChange();

   // kill dead properties
   DWORD i;
   PCMIFLProp *ppp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
   for (i = m_lPCMIFLProp.Num()-1; i < m_lPCMIFLProp.Num(); i--) { // BUGFIX - was i++
      PWSTR psz = (PWSTR)ppp[i]->m_memName.p;
      if (!psz[0]) {
         // delete this
         delete ppp[i];
         m_lPCMIFLProp.Remove (i);
         ppp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
         continue;
      }

      // pass on
      if (fLowerCase)
         MIFLToLower ((PWSTR)ppp[i]->m_memName.p);
   } // i

   // lower case own name
   if (fLowerCase)
      MIFLToLower ((PWSTR)m_memName.p);

   if (pLib)
      pLib->Changed();
}


/********************************************************************************
CMIFLMeth::InitAsGetSet - Initializes the method definition for get/set, basically
setting up the parameters.

NOTE: This doesnt call into lib for undo/redo reasons

inputs
   PWSTR          pszPropName - Property name
   BOOL           fGet - If TRUE initialize for get, else set
   BOOL           fDocs - If TRUE then set up the documentation, else leave blank
returns
   none
*/
void CMIFLMeth::InitAsGetSet (PWSTR pszPropName, BOOL fGet, BOOL fDocs)
{
   Clear();

   MemZero (&m_memName);
   MemCat (&m_memName, pszPropName);
   MemCat (&m_memName, fGet ? L"Get" : L"Set");

   if (!fGet) {
      PCMIFLProp pNew = new CMIFLProp;
      if (!pNew)
         return;

      MemZero (&pNew->m_memName);
      MemCat (&pNew->m_memName, L"Value");
      m_lPCMIFLProp.Add (&pNew);

      if (fDocs) {
         MemCat (&pNew->m_memDescShort,
            L"This is the value that is attribute is being set to. "
            L"It is up to the set function, however, to actually set the value.");
      }
   }
   else if (fGet && fDocs) {
      MemZero (&m_mpRet.m_memDescShort);
      MemCat (&m_mpRet.m_memDescShort,
         L"This is the value that will be used when the variable/property is accessed. "
         L"The code must return a value.");
   }
}



/********************************************************************************
CMIFLMeth::InitCodeVars - This initializes the code variables based on the method
paramters.

NOTE: Assumes that the property are unique.

inputs
   PCMIFLCode        pCode - Code to initialized
returns
   none
*/
void CMIFLMeth::InitCodeVars (PCMIFLCode pCode)
{
   pCode->m_hVars.Clear();

   // add this
   pCode->m_hVars.Add (L"this", NULL, FALSE);
   pCode->m_hVars.Add (L"arguments", NULL, FALSE);
   
   DWORD i;
   PCMIFLProp *ppp = (PCMIFLProp*)m_lPCMIFLProp.Get(0);
   for (i = 0; i < m_lPCMIFLProp.Num(); i++)
      pCode->m_hVars.Add ((PWSTR)ppp[i]->m_memName.p, NULL, TRUE);   // lowercase just in case

   pCode->m_dwParamCount = m_lPCMIFLProp.Num();
}
