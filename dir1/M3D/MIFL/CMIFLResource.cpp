/*************************************************************************************
CMIFLResource.cpp - Definition for a parameter

begun 11/01/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"

static PWSTR gpszTransPros = L"TransPros";
static PWSTR gpszOrigText = L"OrigText";
static PWSTR gpszPreModText = L"PreModText";
static PWSTR gpszTransProsQuick = L"rTransProsQuick_";

/*************************************************************************************
CMIFLResource::Constructor and destructor
*/
CMIFLResource::CMIFLResource (void)
{
   m_lLANGID.Init (sizeof(LANGID));
   m_lPCMMLNode2.Init (sizeof(PCMMLNode2));

   Clear();
}

CMIFLResource::~CMIFLResource (void)
{
   DWORD i;
   PCMMLNode2 *ppn = (PCMMLNode2*) m_lPCMMLNode2.Get(0);
   for (i = 0; i < m_lPCMMLNode2.Num(); i++)
      if (ppn[i])
         delete ppn[i];
}

/*************************************************************************************
CMIFLResource::Clear - Wipes the parameter to intial state
*/
void CMIFLResource::Clear()
{
   m_dwTempID = MIFLTempID ();
   m_dwLibOrigFrom = 0;

   DWORD i;
   PCMMLNode2 *ppn = (PCMMLNode2*) m_lPCMMLNode2.Get(0);
   for (i = 0; i < m_lPCMMLNode2.Num(); i++)
      if (ppn[i])
         delete ppn[i];
   m_lPCMMLNode2.Clear();

   m_lAsString.Clear();

   MemZero (&m_memName);
   MemZero (&m_memDescShort);
   MemZero (&m_memType);
   m_lLANGID.Clear();
}

/*************************************************************************************
CMIFLResource::CloneTo - Stanrdard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
   LANGID      lidKeep - If -1 then keep all languages, else only keep the language specified
*/
BOOL CMIFLResource::CloneTo (CMIFLResource *pTo, BOOL fKeepDocs, LANGID lidKeep)
{
   pTo->Clear();

   pTo->m_dwTempID = m_dwTempID;
   pTo->m_dwLibOrigFrom = m_dwLibOrigFrom;

   if (((PWSTR)m_memName.p)[0])
      MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   if (fKeepDocs) {
      if (((PWSTR)m_memDescShort.p)[0])
         MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
   }
   if (((PWSTR)m_memType.p)[0])
      MemCat (&pTo->m_memType, (PWSTR)m_memType.p);

   if ((lidKeep == (LANGID)-1) || (m_lLANGID.Num() < 2)) {
      pTo->m_lLANGID.Init (sizeof(LANGID), m_lLANGID.Get(0), m_lLANGID.Num());
      
      pTo->m_lPCMMLNode2.Init (sizeof(PCMMLNode2), m_lPCMMLNode2.Get(0), m_lPCMMLNode2.Num());
      PCMMLNode2 *ppn = (PCMMLNode2*) pTo->m_lPCMMLNode2.Get(0);
      DWORD i;
      for (i = 0; i < pTo->m_lPCMMLNode2.Num(); i++)
         ppn[i] = ppn[i]->Clone();

      pTo->m_lAsString.Clear();// clear as string and recalc when necessary
   }
   else {
      // find the closest match and keep that
      DWORD dwFind = MIFLLangMatch (&m_lLANGID, lidKeep, FALSE);
      LANGID *pl = (LANGID*) m_lLANGID.Get(0);
      PCMMLNode2 *ppn = (PCMMLNode2*) m_lPCMMLNode2.Get(0);
      PCMMLNode2 pClone = ppn[dwFind]->Clone();

      pTo->m_lLANGID.Add (&pl[dwFind]);
      pTo->m_lPCMMLNode2.Add (&pClone);

      pTo->m_lAsString.Clear();  // clear as string and recalc when necessary
   }

   return TRUE;
}

/*************************************************************************************
CMIFLResource::Get - Gets the resource value by finding the cloest language.

inputs
   LANGID         lid - Language
returns
   PCMMLNode2 - Resource from the closest matching language
*/
PCMMLNode2 CMIFLResource::Get (LANGID lid)
{
   PCMMLNode2 *ppn = (PCMMLNode2*) m_lPCMMLNode2.Get(0);
   switch (m_lLANGID.Num()) {
   case 0:
      return NULL;
   case 1:
      return ppn[0]; // only one language
   }

   // else...
   DWORD dwFind = MIFLLangMatch (&m_lLANGID, lid, FALSE);
   return ppn[dwFind];
}


/*************************************************************************************
CMIFLResource::GetAsString - Gets the resource value by finding the cloest language.
This returns a string that's been precaclulated, to improve performance.

inputs
   LANGID         lid - Language
   DWORD          *pdwLength - Length of the stirng in characters (excluding the NULL)
returns
   PWSTR - String
*/
PWSTR CMIFLResource::GetAsString (LANGID lid, DWORD *pdwLength)
{
   DWORD dwIndex;

   switch (m_lLANGID.Num()) {
   case 0:
      return NULL;
   case 1:
      dwIndex = 0;
      break;
   default:
      dwIndex = MIFLLangMatch (&m_lLANGID, lid, FALSE);
      break;
   }

   // if there aren't enough entries then calculat for all langauges
   if (m_lAsString.Num() < m_lPCMMLNode2.Num()) {
      PCMMLNode2 *ppn = (PCMMLNode2*) m_lPCMMLNode2.Get(0);
      m_lAsString.Clear();
      DWORD i;
      CMem mem;
      m_lAsString.Required (m_lPCMMLNode2.Num());
      for (i = 0; i < m_lPCMMLNode2.Num(); i++) {
         PCMMLNode2 pn = ppn[i]; // always get 1st one
         mem.m_dwCurPosn = 0;

         if (pn && MMLToMem (pn, &mem, FALSE, 0, FALSE)) {
            mem.CharCat (0);
            m_lAsString.Add (mem.p, mem.m_dwCurPosn);
         }
         else {
            PWSTR psz = L"error";
            m_lAsString.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
         }
      } // i
   } // recalc

   if (pdwLength)
      *pdwLength = (DWORD)m_lAsString.Size (dwIndex) / sizeof(WCHAR) - 1;
   return (PWSTR)m_lAsString.Get(dwIndex);
}

/*************************************************************************************
CMIFLResource::Clone - Stanrdard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
   LANGID      lidKeep - If -1 then keep all languages, else only keep the language specified
*/
CMIFLResource *CMIFLResource::Clone (BOOL fKeepDocs, LANGID lidKeep)
{
   PCMIFLResource pNew = new CMIFLResource;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDocs, lidKeep)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



static PWSTR gpszMIFLResource = L"MIFLResource";
static PWSTR gpszName = L"Name";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszType = L"Type";
static PWSTR gpszMMLNode = L"MMLNode";

/*************************************************************************************
CMIFLResource::MMLTo - Stanrdard API
*/
PCMMLNode2 CMIFLResource::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLResource);

   if (m_memName.p && ((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR)m_memName.p);
   if (m_memDescShort.p && ((PWSTR)m_memDescShort.p)[0])
      MMLValueSet (pNode, gpszDescShort, (PWSTR)m_memDescShort.p);
   if (m_memType.p && ((PWSTR)m_memType.p)[0])
      MMLValueSet (pNode, gpszType, (PWSTR)m_memType.p);

   DWORD i;
   WCHAR szTemp[64];
   LANGID *pl = (LANGID*)m_lLANGID.Get(0);
   PCMMLNode2 *ppn = (PCMMLNode2*)m_lPCMMLNode2.Get(0);
   for (i = 0; i < m_lLANGID.Num(); i++) {
      swprintf (szTemp, L"ID%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)pl[i]);

      PCMMLNode2 pSub = ppn[i]->Clone();
      pSub->NameSet (gpszMMLNode);
      pNode->ContentAdd (pSub);
   } // i


   return pNode;
}


/*************************************************************************************
CMIFLResource::MMLTo - Stanrdard API
*/
BOOL CMIFLResource::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);
   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);
   psz = MMLValueGet (pNode, gpszType);
   if (psz)
      MemCat (&m_memType, psz);

   DWORD i;
   WCHAR szTemp[64];
   LANGID *pl = (LANGID*)m_lLANGID.Get(0);
   for (i = 0; ; i++) {
      LANGID lid;
      swprintf (szTemp, L"ID%d", (int)i);
      lid = (LANGID)MMLValueGetInt (pNode, szTemp, -1);
      if (lid == (LANGID)-1)
         break;

      // add them
      m_lLANGID.Add (&lid);
   } // i

   // get the mmlnodes
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszMMLNode)) {
         pSub = pSub->Clone();
         if (!pSub)
            continue;

         m_lPCMMLNode2.Add (&pSub);
         continue;
      }
   } // i

   if (m_lLANGID.Num() != m_lPCMMLNode2.Num())
      return FALSE;

   return TRUE;
}

/*************************************************************************************
CMIFLResource::LangRemove - Removes an occurance of the given language, if it appears.

inputs
   LANGID            lid - Language
   PCMIFLLib         pLib - Library to use for undo/redo
returns
   BOOL - TRUE if had
*/
BOOL CMIFLResource::LangRemove (LANGID lid, PCMIFLLib pLib)
{
   // see if it's in the list
   LANGID *pl = (LANGID*)m_lLANGID.Get(0);
   DWORD i;
   for (i = 0; i < m_lLANGID.Num(); i++)
      if (pl[i] == lid)
         break;
   if (i >= m_lLANGID.Num())
      return FALSE;

   // else, delte
   pLib->AboutToChange();
   m_lLANGID.Remove (i);

   PCMMLNode2 *ppn = (PCMMLNode2*) m_lPCMMLNode2.Get(0);
   if (ppn[i])
      delete ppn[i];
   m_lPCMMLNode2.Remove (i);
   m_lAsString.Clear(); // so recalc later
   pLib->Changed();
   return TRUE;
}





/*********************************************************************************
ResourceEditPage - UI
*/

BOOL ResourceEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLResource pResource = pmp->pResource;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pResource->m_memName.p);
         pControl = pPage->ControlFind (L"descshort");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pResource->m_memDescShort.p);

         // indicate if resource name is valid
         pPage->Message (ESCM_USER+83);

         // enable/disable resource buttons...
         pPage->Message (ESCM_USER+84);

         // will need to set movelib combo
         MIFLComboLibs (pPage, L"movelib", L"move", pProj, pLib->m_dwTempID);
      }
      return TRUE;

   case ESCM_USER+84:   // enable/disable buttons
      {
         DWORD i;
         WCHAR szTemp[64];
         PCEscControl pControl;
         LANGID *pl = (LANGID*)pResource->m_lLANGID.Get(0);
         for (i = 0; i < pResource->m_lLANGID.Num(); i++) {
            swprintf (szTemp, L"add:%d", (int)pl[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (FALSE);

            swprintf (szTemp, L"edit:%d", (int)pl[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (TRUE);

            swprintf (szTemp, L"text:%d", (int)pl[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (TRUE);

            swprintf (szTemp, L"del:%d", (int)pl[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (!pLib->m_fReadOnly);

            swprintf (szTemp, L"dup:%d", (int)pl[i]);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (!pLib->m_fReadOnly);
         } // i
      }
      return TRUE;

   case ESCM_USER+83:   // test for unique
      {
         // see if the name is unqiue
         DWORD i;
         for (i = 0; i < pProj->LibraryNum(); i++) {
            PCMIFLLib pLib = pProj->LibraryGet(i);
            DWORD dwFind = pLib->ResourceFind ((PWSTR)pResource->m_memName.p, pResource->m_dwTempID);
            if (dwFind != -1)
               break;
         } // i

         // set
         PCEscControl pControl = pPage->ControlFind (L"isunique");
         if (!pControl)
            return TRUE;
         ESCMSTATUSTEXT st;
         memset (&st, 0, sizeof(st));

         MemZero (&gMemTemp);
         if (i < pProj->LibraryNum())
            MemCat (&gMemTemp, L"<font color=#800000>(Name already exists)</font>");
         else
            MemCat (&gMemTemp, L"<null>(Unique name)</null>");
         st.pszMML = (PWSTR)gMemTemp.p;
         pControl->Message (ESCM_STATUSTEXT, &st);
         return TRUE;
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this resource?"))
               return TRUE;

            // remove the Resource
            pLib->ResourceRemove (pResource->m_dwTempID);

            // set a link to go to the Resource list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dresourcelist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated resource.");

            // duplicate
            DWORD dwID = pLib->ResourceAddCopy (pResource);

            // set a link to go to the Resource list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dresource:%dedit", pLib->m_dwTempID, dwID);
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
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the resource?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The resource will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->ResourceAddCopy (pResource);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->ResourceRemove (pResource->m_dwTempID);

            // set a link to go to the Resource list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dresource:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }

         PWSTR pszAdd = L"add:", pszEdit = L"edit:", pszDel = L"del:", pszDup = L"dup:", pszText = L"text:";
         DWORD dwAddLen = (DWORD)wcslen(pszAdd), dwEditLen = (DWORD)wcslen(pszEdit), dwDelLen = (DWORD)wcslen(pszDel),
            dwDupLen = (DWORD)wcslen(pszDup), dwTextLen = (DWORD)wcslen(pszText);
         if (!wcsncmp (psz, pszAdd, dwAddLen)) {
            LANGID lid = _wtoi(psz + dwAddLen);

            // just to be paranoid, if it already exists in the list then ignore
            DWORD i;
            i = MIFLLangMatch (&pResource->m_lLANGID, lid, TRUE);
               // BUGFIX - Okay so long as primary matches
            //LANGID *pl = (LANGID*)pResource->m_lLANGID.Get(0);
            //for (i = 0; i < pResource->m_lLANGID.Num(); i++)
            //   if (pl[i] == lid)
            //      break;
            if (i < pResource->m_lLANGID.Num())
               return TRUE;   // shouldnt happen...

            // paranoid, if RO then fail
            if (pLib->m_fReadOnly)
               return TRUE;

            // add...
            PCMMLNode2 pNode = new CMMLNode2;
            if (!pNode)
               return TRUE;
            pNode->NameSet (gpszMMLNode); // for nothing better

            // should pull up edit UI
            PCMMLNode2 pNew;
            pNew = pProj->m_pSocket->ResourceEdit (pPage->m_pWindow->m_hWnd,
               (PWSTR)pResource->m_memType.p, lid,
               pNode, pLib->m_fReadOnly, pProj, pLib, pResource);

            if (pNew) {
               delete pNode;
               pNode = pNew;
            }

            pLib->AboutToChange();
            pResource->m_lLANGID.Add (&lid);
            pResource->m_lPCMMLNode2.Add (&pNode);
            pResource->m_lAsString.Clear();  // so recalc later
            pLib->Changed();

            // enable/disable buttons
            WCHAR szTemp[64];
            PCEscControl pControl;
            swprintf (szTemp, L"add:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (FALSE);

            swprintf (szTemp, L"edit:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (TRUE);

            // BUGFIX - was missing
            swprintf (szTemp, L"text:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (TRUE);

            swprintf (szTemp, L"del:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (!pLib->m_fReadOnly);

            swprintf (szTemp, L"dup:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (!pLib->m_fReadOnly);


            return TRUE;
         }
         else if (!wcsncmp (psz, pszEdit, dwEditLen)) {
            LANGID lid = _wtoi(psz + dwEditLen);

            // make sure exists
            DWORD i;
            i = MIFLLangMatch (&pResource->m_lLANGID, lid, TRUE);
               // BUGFIX - Okay so long as primary matches
            //LANGID *pl = (LANGID*)pResource->m_lLANGID.Get(0);
            //for (i = 0; i < pResource->m_lLANGID.Num(); i++)
            //   if (pl[i] == lid)
            //      break;
            if (i >= pResource->m_lLANGID.Num())
               return TRUE;   // shouldnt happen...

            PCMMLNode2 *ppn = (PCMMLNode2*)pResource->m_lPCMMLNode2.Get(0);
            // should pull up edit UI
            PCMMLNode2 pNew;
            pNew = pProj->m_pSocket->ResourceEdit (pPage->m_pWindow->m_hWnd,
               (PWSTR)pResource->m_memType.p, lid,
               ppn[i], pLib->m_fReadOnly, pProj, pLib, pResource);

            // BUGFIX - If it's a read only lib then ignore changes
            if (pNew && pLib->m_fReadOnly)
               delete pNew;
            else if (pNew) {
               pLib->AboutToChange();
               if (ppn[i])
                  delete ppn[i];
               ppn[i] = pNew;
               pLib->Changed();
            }

            return TRUE;
         }
         else if (!wcsncmp (psz, pszText, dwTextLen)) {
            LANGID lid = _wtoi(psz + dwEditLen);

            // make sure exists
            DWORD i;
            i = MIFLLangMatch (&pResource->m_lLANGID, lid, TRUE);
               // BUGFIX - Okay so long as primary matches
            //LANGID *pl = (LANGID*)pResource->m_lLANGID.Get(0);
            //for (i = 0; i < pResource->m_lLANGID.Num(); i++)
            //   if (pl[i] == lid)
            //      break;
            if (i >= pResource->m_lLANGID.Num())
               return TRUE;   // shouldnt happen...

            PCMMLNode2 *ppn = (PCMMLNode2*)pResource->m_lPCMMLNode2.Get(0);

            CMem mem;
            PCEscControl pControl = pPage->ControlFind (L"viewtext");
            ppn[i]->NameSet ((PWSTR)pResource->m_memType.p);
            if (pControl && MMLToMem (ppn[i], &mem, FALSE, 0, TRUE)) {
               mem.CharCat (0);  // BUGFIX
               pControl->AttribSet (Text(), (PWSTR)mem.p);
               EscChime (ESCCHIME_INFORMATION);
            }
            return TRUE;
         }
         else if (!wcsncmp (psz, pszDel, dwDelLen)) {
            LANGID lid = _wtoi(psz + dwDelLen);

            // just to be paranoid, if it already exists in the list then ignore
            DWORD i;
            i = MIFLLangMatch (&pResource->m_lLANGID, lid, TRUE);
               // BUGFIX - Okay so long as primary matches
            //LANGID *pl = (LANGID*)pResource->m_lLANGID.Get(0);
            //for (i = 0; i < pResource->m_lLANGID.Num(); i++)
            //   if (pl[i] == lid)
            //      break;
            if (i >= pResource->m_lLANGID.Num())
               return TRUE;   // shouldnt happen...

            // paranoid, if RO then fail
            if (pLib->m_fReadOnly)
               return TRUE;

            // verify
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the resource?"))
               return TRUE;

            // Delete...
            pLib->AboutToChange();
            PCMMLNode2 *ppn = (PCMMLNode2*)pResource->m_lPCMMLNode2.Get(0);
            if (ppn[i])
               delete ppn[i];
            pResource->m_lLANGID.Remove (i);
            pResource->m_lPCMMLNode2.Remove (i);
            pResource->m_lAsString.Clear();  // so recalc later
            pLib->Changed();

            // enable/disable buttons
            WCHAR szTemp[64];
            PCEscControl pControl;
            swprintf (szTemp, L"add:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (!pLib->m_fReadOnly);

            swprintf (szTemp, L"edit:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (FALSE);

            swprintf (szTemp, L"text:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (FALSE);

            swprintf (szTemp, L"del:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (FALSE);

            swprintf (szTemp, L"dup:%d", (int)lid);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->Enable (FALSE);


            return TRUE;
         }
         else if (!wcsncmp (psz, pszDup, dwDupLen)) {
            LANGID lid = _wtoi(psz + dwDupLen);

            // just to be paranoid, if it already exists in the list then ignore
            DWORD i;
            i = MIFLLangMatch (&pResource->m_lLANGID, lid, TRUE);
               // BUGFIX - Okay so long as primary matches
            //LANGID *pl = (LANGID*)pResource->m_lLANGID.Get(0);
            //for (i = 0; i < pResource->m_lLANGID.Num(); i++)
            //   if (pl[i] == lid)
            //      break;
            if (i >= pResource->m_lLANGID.Num())
               return TRUE;   // shouldnt happen...
            PCMMLNode2 pThis = *((PCMMLNode2*)pResource->m_lPCMMLNode2.Get(i));

            // paranoid, if RO then fail
            if (pLib->m_fReadOnly)
               return TRUE;

            // find all the ones that need to add
            DWORD j;
            LANGID *plProj = (LANGID*)pProj->m_lLANGID.Get(0);
            for (i = 0; i < pProj->m_lLANGID.Num(); i++) {
               j = MIFLLangMatch (&pResource->m_lLANGID, plProj[i], TRUE);
                  // BUGFIX - Okay so long as primary matches
               //for (j = 0; j < pResource->m_lLANGID.Num(); j++)
               //   if (pl[j] == plProj[i])
               //      break;
               if (j < pResource->m_lLANGID.Num())
                  continue;   // already exsits

               // else, needs to be added

               // clone it
               PCMMLNode2 pClone = pThis->Clone();
               if (!pClone)
                  continue;

               pLib->AboutToChange();
               pResource->m_lLANGID.Add (&plProj[i]);
               pResource->m_lPCMMLNode2.Add (&pClone);
               pResource->m_lAsString.Clear();  // so recalc later
               pLib->Changed();

               // update since just changed the list
               // pl = (LANGID*)pResource->m_lLANGID.Get(0);
            } // i

            // enable/disable resource buttons...
            pPage->Message (ESCM_USER+84);


            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"name")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pResource->m_memName.Required (dwNeed))
               return FALSE;
            pLib->AboutToChange();
            p->pControl->AttribGet (Text(), (PWSTR)pResource->m_memName.p, (DWORD)pResource->m_memName.m_dwAllocated, &dwNeed);
            pLib->ResourceSort();
            pLib->Changed();

            // indicate if resource name is valid
            pPage->Message (ESCM_USER+83);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"descshort")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pResource->m_memDescShort.Required (dwNeed))
               return FALSE;
            pLib->AboutToChange();
            p->pControl->AttribGet (Text(), (PWSTR)pResource->m_memDescShort.p, (DWORD)pResource->m_memDescShort.m_dwAllocated, &dwNeed);
            pLib->Changed();

            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"Modify resource - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pResource->m_memName.p);

            // NOTE: Resources dont have associated help

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RESTYPE")) {
            p->pszSubString = (PWSTR)pResource->m_memType.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RESTYPEDESC")) {
            // find the resource
            DWORD i;
            PCMIFLAppSocket pSocket = pProj->m_pSocket;
            MASRES res;
            for (i = 0; i < pSocket->ResourceNum(); i++) {
               if (!pSocket->ResourceEnum (i, &res))
                  continue;

               if (!_wcsicmp(res.pszName, (PWSTR)pResource->m_memType.p))
                  break;
            }
            if (i >= pSocket->ResourceNum())
               return FALSE;

            p->pszSubString = res.pszDescShort;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->ResourceOverridden (pLib->m_dwTempID, (PWSTR)pResource->m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by resource in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides resource in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"STRINGLANG")) {
            MemZero (&gMemTemp);

            DWORD i;
            LANGID *pl = (LANGID*)pProj->m_lLANGID.Get(0);
            for (i = 0; i < pProj->m_lLANGID.Num(); i++) {
               // find the closest language
               LANGID lid = pl[i];
               DWORD dwClosestLang = MIFLLangMatch (&pResource->m_lLANGID, lid, TRUE);
               if (dwClosestLang < pResource->m_lLANGID.Num())
                  lid = *((LANGID*)pResource->m_lLANGID.Get(dwClosestLang));

               // get the resource
               DWORD dwIndex = MIFLLangFind (lid);
               PWSTR psz = MIFLLangGet (dwIndex, NULL);
               if (!psz)
                  psz = L"Unknown";

               MemCat (&gMemTemp, L"<tr><td width=33%><bold>");
               MemCatSanitize (&gMemTemp, psz);
               if (!i)
                  MemCat (&gMemTemp, L" (default)");
               MemCat (&gMemTemp, L"</bold></td><td width=22% align=center><button name=add:");
               MemCat (&gMemTemp, (int)lid);
               if (pLib->m_fReadOnly)
                  MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><bold>Add</bold><xHoverHelp>Press this to add a resource for the language.</xHoverHelp></button></td>");
               
               MemCat (&gMemTemp, L"<td width=22% align=center><button name=edit:");
               MemCat (&gMemTemp, (int)lid);
               MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><bold>Modify</bold><xHoverHelp>Press this to modify the resource for the language.</xHoverHelp></button><br/>");
               MemCat (&gMemTemp, L"<button name=text:");
               MemCat (&gMemTemp, (int)lid);
               MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><bold>Text</bold><xHoverHelp>Press this to view the text for the resource.</xHoverHelp></button>");
               MemCat (&gMemTemp, L"</td>");

               MemCat (&gMemTemp, L"<td width=22% align=center>");
               MemCat (&gMemTemp, L"<button name=del:");
               MemCat (&gMemTemp, (int)lid);
               MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><bold>Delete</bold><xHoverHelp>This deletes the resource for the given language.</xHoverHelp></button>");
               MemCat (&gMemTemp, L"<br/><button name=dup:");
               MemCat (&gMemTemp, (int)lid);
               MemCat (&gMemTemp, L" enabled=false");
               MemCat (&gMemTemp, L"><bold>Duplicate</bold>"
                  L"<xHoverHelp>Duplicate the resource for this language to all currently empty langauge slots. "
                  L"Duplication lets you create and fine-tune a main resource, and then customize it for each language.</xHoverHelp>"
                  L"</button>");
               MemCat (&gMemTemp, L"</td></tr>");
            };

            // if not lang's
            if (!pProj->m_lLANGID.Num())
               MemCat (&gMemTemp, L"<tr><td>Your current project doesn't include any languages.</td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/********************************************************************************
CMIFLResource::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library that the resource is part of
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a resource for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dresource%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new resource.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLResource::DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLRESOURCEEDIT;
   PESCPAGECALLBACK pPage = ResourceEditPage;

   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLRESOURCEEDIT;
      pPage = ResourceEditPage;
   }

   PWSTR pszRet;
   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pResource = this;
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
CMIFLResource::TransProsQuickText - Get the original and premod text from
a verified <TransPros> resource.

inputs
   LANGID         lid - Language
   PWSTR          *ppszOrigText - Filled in with a pointer to the original text
   PWSTR          *ppszPreModText - Filled in with a pointer to the premod text
returns  
   BOOL - TRUE on usccess
*/
BOOL CMIFLResource::TransProsQuickText (LANGID lid, PWSTR *ppszOrigText, PWSTR *ppszPreModText)
{
   if (ppszOrigText)
      *ppszOrigText = NULL;
   if (ppszPreModText)
      *ppszPreModText = NULL;

   // verify the type
   if (_wcsicmp (gpszTransPros, (PWSTR)m_memType.p))
      return FALSE;

   // make sure it begins with "rTransProsQuick"
   if (_wcsnicmp(gpszTransProsQuick, (PWSTR)m_memName.p, wcslen(gpszTransProsQuick)))
      return FALSE;

   // find an existing match
   DWORD dwMatch = MIFLLangMatch (&m_lLANGID, lid, TRUE);
      // BUGFIX - Okay so long as primary matches
   //LANGID *plid = (LANGID*) m_lLANGID.Get(0);
   //DWORD i, dwMatch = (DWORD)-1;
   //for (i = 0; i < m_lLANGID.Num(); i++)
   //   if (plid[i] == lid) {
   //      dwMatch = i;
   //      break;
   //   }
   if (dwMatch == (DWORD)-1)
      return FALSE;
   
   // if have match, then look for the original text
   PCMMLNode2 pNodeOrig = *((PCMMLNode2*)m_lPCMMLNode2.Get(dwMatch));

   // get orig text
   PCMMLNode2 pSub, pSub2;
   PWSTR psz;
   if (ppszOrigText) {
      pSub = NULL;
      psz = NULL;
      pNodeOrig->ContentEnum (pNodeOrig->ContentFind (gpszOrigText), &psz, &pSub);
      if (pSub) {
         psz = NULL;
         pSub->ContentEnum (0, &psz, &pSub2);
         if (psz)
            *ppszOrigText = psz;
      }
   }

   // get the premodtext
   if (ppszPreModText) {
      pSub = NULL;
      psz = NULL;
      pNodeOrig->ContentEnum (pNodeOrig->ContentFind (gpszPreModText), &psz, &pSub);
      if (pSub) {
         psz = NULL;
         pSub->ContentEnum (0, &psz, &pSub2);
         if (psz)
            *ppszPreModText = psz;
      }
   }

   return TRUE;
}



/*************************************************************************************
CircumrealityParseMML - This takes a string with MML and parses it. It returns the node.
It assumes format is "<mainnode>stuff</mainnode>"

inputs
   PWSTR       psz - Null-terminated string
returns
   PCMMLNode2 - Node that must be freed
*/
static PCMMLNode2 CircumrealityParseMML (PWSTR psz)
{
   CEscError err;

   PCMMLNode pRet1;
   PCMMLNode2 pRet;
   pRet1 = ParseMML (psz, ghInstance, NULL, NULL, &err, FALSE);
   if (!pRet1)
      return NULL;
   pRet = pRet1->CloneAsCMMLNode2();
   delete pRet1;
   if (pRet) {
      // parse MML will put a wrapper around this, so take first node
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      pRet->ContentEnum (0, &psz, &pSub);
      if (pSub)
         pRet->ContentRemove (0, FALSE);
      delete pRet;
      pRet = pSub;
   }

   return pRet;
}


/********************************************************************************
CMIFLResource::TransProsQuickDialog - Pulls up a dialog for editing/adding a transplanted
prosody resource.

The resource type MUST be "TransPros". It doesn't need to contain any resource
info though.

inputs
   PCMIFLLib         pLib - Library that the resource is part of. Used for undo/redo.
   PCEscWindow       pWindow - Window to use. If NULL then create off of hWnd
   HWND              hWnd - Usedif !pWindow, create a dialog window off of this.
   LANGID            lid - Language ID to use.
   PWSTR             pszCreatedBy - For the comments, so know which string/property created this.
                        Can be NULL.
   PWSTR             pszText - Text that is to be transplanted
returns
   DWORD - 0 if user pressed cancel or closed window, 1 if pressed back, 2 if accepted
      transplanted prosody result
*/
DWORD CMIFLResource::TransProsQuickDialog (PCMIFLLib pLib, PCEscWindow pWindow, HWND hWnd,
                                      LANGID lid, PWSTR pszCreatedBy, PWSTR pszText)
{
   // verify the type
   if (_wcsicmp (gpszTransPros, (PWSTR)m_memType.p))
      return 1;

   CEscWindow Window;

   if (!pWindow) {
      // create the window
      RECT r;
      DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
      Window.Init (ghInstance, hWnd, 0, &r);
      pWindow = &Window;
   }

   // find an existing match
   DWORD dwMatch = MIFLLangMatch (&m_lLANGID, lid, TRUE);
      // BUGFIX - Okay so long as primary matches
   //LANGID *plid = (LANGID*) m_lLANGID.Get(0);
   //DWORD i, dwMatch = (DWORD)-1;
   //for (i = 0; i < m_lLANGID.Num(); i++)
   //   if (plid[i] == lid) {
   //      dwMatch = i;
   //      break;
   //   }

   // get orig text
   PWSTR pszOrigText, pszOrigPreMod;
   TransProsQuickText (lid, &pszOrigText, &pszOrigPreMod);
   if (!pszOrigPreMod || _wcsicmp(pszOrigPreMod, pszText))
      pszOrigText = NULL;   // reset premod since either didn't exist, or text has changed
   if (!pszOrigText)
      pszOrigText = pszText;

   // pull up the dialog
   CTTSTransPros TransPros;
   CMem memText;
   DWORD dwRet = TransPros.DialogQuick (pWindow, pszOrigText, pszText, &memText);
   if (dwRet != 2)
      return dwRet;  // cancelled or whatever


   PCMMLNode2 pRet = CircumrealityParseMML ((PWSTR)memText.p);
   if (!pRet) {
      // was just text
      pRet = new CMMLNode2;
      pRet->ContentAdd ((PWSTR)memText.p);
   }
   if (pRet)
      pRet->NameSet (gpszTransPros);

   // make changes
   pLib->AboutToChange ();

   // if existing one then delete
   if (dwMatch != (DWORD)-1) {
      // replace
      PCMMLNode2 *ppNode = (PCMMLNode2*)m_lPCMMLNode2.Get(dwMatch);
      delete ppNode[0];
      ppNode[0] = pRet;
   }
   else {
      // add
      m_lPCMMLNode2.Add (&pRet);
      m_lLANGID.Add (&lid);
   }
   if (pszCreatedBy) {
      MemZero (&m_memDescShort);
      MemCat (&m_memDescShort, L"\"");
      MemCat (&m_memDescShort, pszText);
      MemCat (&m_memDescShort, L"\", automatically created by ");
      MemCat (&m_memDescShort, pszCreatedBy);
   }
   pLib->Changed ();

   return dwRet;
}

   
// BUGBUG - When compile the project, should keep string versions of the resources
// around so that will be faster, since most of the time convert the resources
// to strings anyway
