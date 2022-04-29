/*************************************************************************************
CMIFLCode.cpp - Definition for a chunk of code

begun 3/1/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


/*************************************************************************************
CMIFLCode::Constructor and destructor
*/
CMIFLCode::CMIFLCode (void)
{
   // m_lPCMIFLPropVar.Init (sizeof(PCMIFLProp));

   Clear();
}

CMIFLCode::~CMIFLCode (void)
{
   // dont' bother calling since no properties... Clear(); // so frees up all properties
}

/*************************************************************************************
CMIFLCode::Clear - Wipes the parameter to intial state
*/
void CMIFLCode::Clear()
{
   MemZero (&m_memCode);
   m_hVars.Init (0);
   m_memMIFLCOMP.m_dwCurPosn = 0;

   m_pObjectLayer = 0;
   m_pszCodeName = NULL;
   m_dwCodeFrom = 6;

   //DWORD i;
   //PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropVar.Get(0);
   //for (i = 0; i < m_lPCMIFLPropVar.Num(); i++)
   //   delete pp[i];
   //m_lPCMIFLPropVar.Clear();
}

/*************************************************************************************
CMIFLCode::CloneTo - Stanrdard API
*/
BOOL CMIFLCode::CloneTo (CMIFLCode *pTo)
{
   pTo->Clear();

   MemCat (&pTo->m_memCode, (PWSTR) m_memCode.p);

   // note: Dont set m_pObjectLayer
   // note: Dont set m_pszCodeName
   // note: Dont set m_dwCodeFrom

   // clone the variables
   //pTo->m_lPCMIFLPropVar.Init (sizeof(PCMIFLProp), m_lPCMIFLPropVar.Get(0), m_lPCMIFLPropVar.Num());
   //DWORD i;
   //PCMIFLProp *pp = (PCMIFLProp*)pTo->m_lPCMIFLPropVar.Get(0);
   //for (i = 0; i < pTo->m_lPCMIFLPropVar.Num(); i++)
   //   pp[i] = pp[i]->Clone();

   return TRUE;
}


/*************************************************************************************
CMIFLCode::Clone - Stanrdard API
*/
CMIFLCode *CMIFLCode::Clone (void)
{
   PCMIFLCode pNew = new CMIFLCode;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



static PWSTR gpszMIFLCode = L"MIFLCode";
static PWSTR gpszCode = L"Code";
static PWSTR gpszVar = L"Var";

/*************************************************************************************
CMIFLCode::MMLTo - Stanrdard API
*/
PCMMLNode2 CMIFLCode::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLCode);

   if (m_memCode.p && ((PWSTR)m_memCode.p)[0])
      MMLValueSet (pNode, gpszCode, (PWSTR)m_memCode.p);

   // write out vars
   //DWORD i;
   //PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropVar.Get(0);
   //for (i = 0; i < m_lPCMIFLPropVar.Num(); i++) {
   //   PCMMLNode2 pSub = pp[i]->MMLTo();
   //   if (!pSub) {
   //      delete pNode;
   //      return NULL;
   //   }
   //   pSub->NameSet (gpszVar);
   //   pNode->ContentAdd (pSub);
   //}

   return pNode;
}


/*************************************************************************************
CMIFLCode::MMLFrom - Stanrdard API
*/
BOOL CMIFLCode::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   // get code
   PWSTR psz = MMLValueGet (pNode, gpszCode);
   if (psz)
      MemCat (&m_memCode, psz);

#if 0 // no more variables
   // variables
   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet ();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszVar)) {
         PCMIFLProp pNew = new CMIFLProp;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         m_lPCMIFLPropVar.Add (&pNew);
      }
   } // i
#endif // 0

   return TRUE;
}


/*************************************************************************************
CMIFLCode::PageSubstitution - This functions handles the ESCM_SUBSTITUTION call for a method.
This allows the code to be used for not only the method defiitions, but also functions,
and private methods.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCMSUBSTITUTION p - Substitution info
returns
   BOOL - TRUE if captured and handled this, FALSE if processing should continue
*/
BOOL CMIFLCode::PageSubstitution (PCEscPage pPage, PMIFLPAGE pmp, PESCMSUBSTITUTION p)
{
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

#if 0 // no more variables
   if (!_wcsicmp(p->pszSubName, L"VARSLOT")) {
      MemZero (&gMemTemp);

      // how many slots?
      DWORD dwNum = pLib->m_fReadOnly ? m_lPCMIFLPropVar.Num() : max(1, m_lPCMIFLPropVar.Num()+1);
      DWORD i;

      for (i = 0; i < dwNum; i++) {
         MemCat (&gMemTemp, L"<tr>"
            L"<td width=33%><bold><edit width=100% maxchars=64 ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"readonly=true ");
         MemCat (&gMemTemp, L"name=varname");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold></td>"
            L"<td width=33%><bold><edit width=100% maxchars=1000 ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"readonly=true ");
         MemCat (&gMemTemp, L"name=varinit");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold></td>"
            L"<td width=33%><bold><edit width=100% maxchars=1000 multiline=true wordwrap=true ");
         if (pLib->m_fReadOnly)
            MemCat (&gMemTemp, L"readonly=true ");
         MemCat (&gMemTemp, L"name=vardesc");
         MemCat (&gMemTemp, (int)i);
         MemCat (&gMemTemp, L"/></bold></td></tr>");
      } //i

      p->pszSubString = (PWSTR)gMemTemp.p;
      return TRUE;
   }
#endif // 0

   // else, didn't handle
   return FALSE;
}


/*************************************************************************************
CMIFLCode::PageInitPage - This functions handles the init-page call for a method.
This allows the code to be used for not only the method defiitions, but also functions,
and private methods.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
returns
   none
*/
void CMIFLCode::PageInitPage (PCEscPage pPage, PMIFLPAGE pmp)
{
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   PCEscControl pControl;

   pControl = pPage->ControlFind (L"code");
   if (pControl && m_memCode.p) {
      pControl->AttribSet (Text(), (PWSTR)m_memCode.p);

      // see if there's a current error. If so then will want to set the text
      if (pProj->m_pErrors && ((DWORD)pProj->m_iErrCur < pProj->m_pErrors->Num())) {
         PMIFLERR perr = pProj->m_pErrors->Get((DWORD)pProj->m_iErrCur);
         if (perr && pmp->pszErrLink && !_wcsicmp(perr->pszLink, pmp->pszErrLink)) {
            // clicked on one with link
            pControl->AttribSetInt (L"selstart", perr->dwStartChar);
            pControl->AttribSetInt (L"selend", perr->dwEndChar);
            pControl->Message (ESCM_EDITSCROLLCARET);
         }
      }
   }


#if 0 // no more variables
   // loop through all the paramters
   DWORD i;
   PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropVar.Get(0);
   WCHAR szTemp[32];
   for (i = 0; i < m_lPCMIFLPropVar.Num(); i++) {
      swprintf (szTemp, L"varname%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memName.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memName.p);

      swprintf (szTemp, L"varinit%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memInit.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memInit.p);

      swprintf (szTemp, L"vardesc%d", (int)i);
      pControl = pPage->ControlFind (szTemp);
      if (pControl && pp[i]->m_memDescShort.p)
         pControl->AttribSet (Text(), (PWSTR)pp[i]->m_memDescShort.p);
   } // i
#endif // 0

}

/*************************************************************************************
CMIFLCode::PageEditChange - Call this when an edit change notification comes in.
It will see if it affects any of the method parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNEDITCHANGE   p - Edit change information
   PCMIFLLib         pLib - Library that is ultimately parent of the code
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLCode::PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   // how big is this?
   DWORD dwNeed = 0;
   p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);

   PCMem pMem = NULL;

   if (!_wcsicmp(psz, L"code"))
      pMem = &m_memCode;

#if 0 // no more variables
   BOOL fModVar = FALSE;

   if (!pMem) {
      // if gets to here then might be one of the parameters...
      PWSTR pszVarName = L"varname", pszVarDesc = L"vardesc", pszVarInit = L"varinit";
      DWORD dwVarNameLen = wcslen(pszVarName), dwVarDescLen = wcslen(pszVarDesc), dwVarInitLen = wcslen(pszVarInit);

      DWORD dwNum = 0;
      DWORD dwVar = 0;
      if (!_wcsnicmp(psz, pszVarName, dwVarNameLen)) {
         dwNum = _wtoi (psz + dwVarNameLen);
         dwVar = 1;
         fModVar = TRUE;
      }
      else if (!_wcsnicmp(psz, pszVarDesc, dwVarDescLen)) {
         dwNum = _wtoi (psz + dwVarDescLen);
         dwVar = 2;
         fModVar = TRUE;
      }
      else if (!_wcsnicmp(psz, pszVarInit, dwVarInitLen)) {
         dwNum = _wtoi (psz + dwVarInitLen);
         dwVar = 3;
         fModVar = TRUE;
      }

      // if need to add extra blank Vareters then do so...
      while (dwVar && (dwNum >= m_lPCMIFLPropVar.Num())) {
         PCMIFLProp pNew = new CMIFLProp;
         if (pNew) {
            pLib->AboutToChange ();
            m_lPCMIFLPropVar.Add (&pNew);
            pLib->Changed ();
         }
      }

      // memory to be filled in...
      PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropVar.Get(0);
      switch (dwVar) {
      case 1: // pszVarName
         pMem = &pp[dwNum]->m_memName;
         break;
      case 2: // pszVarDesc
         pMem = &pp[dwNum]->m_memDescShort;
         break;
      case 3: // pszVarInit
         pMem = &pp[dwNum]->m_memInit;
         break;
      }
   }
#endif // 0

   if (pMem) {
      if (!pMem->Required (dwNeed))
         return FALSE;

      pLib->AboutToChange ();
      p->pControl->AttribGet (Text(), (PWSTR)pMem->p, (DWORD)pMem->m_dwAllocated, &dwNeed);

#if 0 // no more variables
      // if modified a Vareter will need to delete dead Vareters at the end of the list
      if (fModVar) while (m_lPCMIFLPropVar.Num()) {
         PCMIFLProp *pp = (PCMIFLProp*)m_lPCMIFLPropVar.Get(0);
         PCMIFLProp pm = pp[m_lPCMIFLPropVar.Num()-1];
         if (!((PWSTR)(pm->m_memName.p))[0] && !((PWSTR)(pm->m_memDescShort.p))[0] && !((PWSTR)(pm->m_memInit.p))[0]) {
            // name was set to nothing, so delete
            delete pm;
            m_lPCMIFLPropVar.Remove (m_lPCMIFLPropVar.Num()-1);
         }
         else
            break;
      }
#endif // 0

      pLib->Changed ();
      return TRUE;
   }


   return FALSE;  // not it
}


/*************************************************************************************
CMIFLCode::PageButtonPress - Call this when an button press notification comes in.
It will see if it affects any of the method parameters and update them.

inputs
   PCEscPage         pPage - Page
   PMIFLPAGE         pmp - Data
   PESCNBUTTONPRESS  p - Buttonpress information
   PCMIFLLib         pLib - Library that is ultimately parent of the code
returns
   BOOL - TRUE if captured and handled, FALSE if not
*/
BOOL CMIFLCode::PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib)
{
   if (!p->pControl || !p->pControl->m_pszName)
      return FALSE;
   PWSTR psz = p->pControl->m_pszName;

   if (!_wcsicmp(psz, L"addvar")) {
      pPage->Exit (MIFLRedoSamePage());
      return TRUE;
   }
   else if (!_wcsicmp(psz, L"compthis")) {
      // NOTE: "compall" should never get called because part of the menu
      CProgress Progress;
      Progress.Start (pPage->m_pWindow->m_hWnd, "Compiling...");

      pmp->pProj->Compile(&Progress, pmp->pMeth, pmp->pCode, pmp->pObj, pmp->pszErrLink);

      if (pmp->pProj->m_pErrors->m_dwNumError)
         EscChime (ESCCHIME_ERROR);
      else if (pmp->pProj->m_pErrors->Num())
         EscChime (ESCCHIME_WARNING);
      else
         EscChime (ESCCHIME_INFORMATION);

      pPage->Exit (MIFLRedoSamePage());
      return TRUE;
   }

   else if (!_wcsicmp(psz, L"codesearchthis") || !_wcsicmp(psz, L"codesearchall")) {
      PCEscControl pControl = pPage->ControlFind (L"codesearch");
      WCHAR szTemp[256];
      DWORD dwNeeded;
      szTemp[0] = 0;
      if (pControl)
         pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);
      if (!szTemp[0]) {
         pPage->MBWarning (L"You must type in some text before you can search.");
         return TRUE;
      }
      pControl = pPage->ControlFind (L"code");
      if (!pControl)
         return TRUE;

      if (!_wcsicmp(psz, L"codesearchthis")) {
         ESCMEDITFINDTEXT ft;
         memset (&ft, 0, sizeof(ft));
         ft.dwEnd = (DWORD)wcslen((PWSTR) m_memCode.p);
         ft.dwStart = (DWORD) pControl->AttribGetInt (L"selend");
         ft.pszFind = szTemp;

         pControl->Message (ESCM_EDITFINDTEXT, &ft);
         if ((ft.dwFoundStart == -1) && ft.dwStart) {
            // try again from the beginning
            ft.dwStart = 0;
            pControl->Message (ESCM_EDITFINDTEXT, &ft);
         }

         if (ft.dwFoundStart != -1) {
            pControl->AttribSetInt (L"selstart", ft.dwFoundStart);
            pControl->AttribSetInt (L"selend", ft.dwFoundEnd);
            pControl->Message (ESCM_EDITSCROLLCARET);
            pPage->FocusSet (pControl);   // BUGFIX - Set focus to control when search locally
         }
         else
            EscChime (ESCCHIME_WARNING);

         return TRUE;
      }

      // search through all code...
      pmp->pProj->SearchAllCode (szTemp);
      pPage->Exit (MIFLRedoSamePage());
      return TRUE;
   }

   return FALSE;
}




