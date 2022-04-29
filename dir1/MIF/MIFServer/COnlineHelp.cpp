/*************************************************************************************
COnlineHelp.cpp - Code for a collection of help resources

begun 26/11/04 by Mike Rozak.
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

/*************************************************************************************
COnlineHelp::Constructor and destructor
*/
COnlineHelp::COnlineHelp (void)
{
   m_pVM = NULL;
   m_lid = 0;
   memset (&m_OHCATEGORY, 0, sizeof(m_OHCATEGORY));
}

COnlineHelp::~COnlineHelp (void)
{
   // free all the CResHelp structures
   DWORD i;
   for (i = 0; i < m_tTitles.Num(); i++) {
      PCResHelp *pph = (PCResHelp*)m_tTitles.GetNum (i);
      if (pph)
         delete pph[0];
   } // i

   OHCATEGORYFree (&m_OHCATEGORY);
}


/*************************************************************************************
COnlineHelp::OHCATEGORYFree - Frees up the contents of a category and all its children.

inputs
   POHCATEGORY       pCat - Category to free
*/
void COnlineHelp::OHCATEGORYFree (POHCATEGORY pCat)
{
   if (pCat->plPCResHelp)
      delete pCat->plPCResHelp;

   if (pCat->ptOHCATEGORY) {
      DWORD i;
      for (i = 0; i < pCat->ptOHCATEGORY->Num(); i++) {
         POHCATEGORY pSub = (POHCATEGORY) pCat->ptOHCATEGORY->GetNum(i);
         OHCATEGORYFree (pSub);
      } // i

      delete pCat->ptOHCATEGORY;
   }
}


/*************************************************************************************
COnlineHelp::OHCATEGORYAdd - Adds a title to the category.

inputs
   PWSTR             pszPath - Path for the category, with '/' indicating category breaks
   PCResHelp         pHelp - Help to add
   POHCATEGORY       pCat - Category to add to
returns
   BOOL - TRUE if success
*/
BOOL COnlineHelp::OHCATEGORYAdd (PWSTR pszPath, PCResHelp pHelp, POHCATEGORY pCat)
{
   // if there isn't anything left to the path then add it
   if (!pszPath[0]) {
      if (!pCat->plPCResHelp) {
         pCat->plPCResHelp = new CListFixed;
         if (!pCat->plPCResHelp)
            return FALSE;
         pCat->plPCResHelp->Init (sizeof(PCResHelp));
      }

      pCat->plPCResHelp->Add (&pHelp);
      return TRUE;
   }

   // else, walk down node path to right spot...

   // make sure there's a tree
   if (!pCat->ptOHCATEGORY) {
      pCat->ptOHCATEGORY = new CBTree;
      if (!pCat->ptOHCATEGORY)
         return FALSE;
   }

   // get the end of the node
   PWSTR pszEnd = wcschr (pszPath, L'/');
   if (!pszEnd)
      pszEnd = pszPath + wcslen(pszPath);
   WCHAR cCur = *pszEnd;
   *pszEnd = 0;

   // find the tree element
   POHCATEGORY pSub = (POHCATEGORY) pCat->ptOHCATEGORY->Find (pszPath);
   if (!pSub) {
      // add it
      OHCATEGORY cat;
      memset (&cat, 0, sizeof(cat));
      pCat->ptOHCATEGORY->Add (pszPath, &cat, sizeof(cat));

      pSub = (POHCATEGORY) pCat->ptOHCATEGORY->Find (pszPath);
      if (!pSub)
         return FALSE;
   }

   // restore the slash
   *pszEnd = cCur;   // restore

   // recurse
   return OHCATEGORYAdd (cCur ? (pszEnd+1) : pszEnd, pHelp, pSub);
}


/*****************************************************************************
HelpDefPage - Default page callback. It handles standard operations
*/
BOOL HelpDefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   // do nothing

   return FALSE;
}


/*****************************************************************************
SearchCallback - Called by search engine
*/
BOOL __cdecl SearchCallback (CEscSearch *pSearch, DWORD dwDocument, PVOID pUserData)
{
   PCBTree pTree = (PCBTree) pUserData;
   PCResHelp *pph = (PCResHelp*) pTree->GetNum(dwDocument);
   PCResHelp pHelp = pph ? pph[0] : NULL;
   if (!pHelp)
      return TRUE;

   // convert the MML
   CEscError err;
   PCMMLNode pNode = ParseMML ((PWSTR)pHelp->m_memDescLong.p, ghInstance, NULL, NULL, &err, TRUE);
   if (!pNode)
      return TRUE;

   BOOL fRet;

   // BUGFIX - find the title up here
   WCHAR szTemp[64];
   swprintf (szTemp, L"s:%d", (int) dwDocument);

   fRet = pSearch->IndexNode (pNode, (PWSTR)pHelp->m_memName.p, szTemp);

   // BUGFIX - Make sure the title and description have extra relevence
   BYTE bOld = pSearch->m_bCurRelevence;
   pSearch->m_bCurRelevence = 128;
   pSearch->IndexText ((PWSTR)pHelp->m_memName.p);
   pSearch->m_bCurRelevence = 64;
   pSearch->IndexText ((PWSTR)pHelp->m_memDescShort.p);
   pSearch->m_bCurRelevence = bOld;

   fRet = pSearch->SectionFlush ();

   // finally
   delete pNode;
   return TRUE;

}


/*************************************************************************************
COnlineHelp::Init - Initializes the online help to the virtual machien and a
given language.

inputs
   PCMIFLVM          pVM - Virtual machien
   LANGID            lid - Language to use
   PWSTR             pszDir - Online directory where help files should be written
   HWND              hWnd - Window to display search indexing off of
returns
   BOOL - TRUE if success
*/
BOOL COnlineHelp::Init (PCMIFLVM pVM, LANGID lid, PWSTR pszDir, HWND hWnd)
{
   // set
   if (m_lid || m_pVM)
      return FALSE;
   m_pVM = pVM;
   m_lid = lid;

   // loop through all the resoruces looking for help resource
   PCMIFLLib pLib = m_pVM->m_pCompiled->m_pLib; // get the merged library
   DWORD i;
   DWORD dwCheckSum = 0;
   for (i = 0; i < pLib->ResourceNum(); i++) {
      // only want help resoruces
      PCMIFLResource pRes = pLib->ResourceGet(i);
      if (_wcsicmp((PWSTR)pRes->m_memType.p, CircumrealityHelp()))
         continue;

      // find the closest matching language
      DWORD dwIndex = MIFLLangMatch (&pRes->m_lLANGID, m_lid, FALSE);
      if (dwIndex >= pRes->m_lLANGID.Num())
         continue;   // shouldnt happen

      // create a CResHelp from this
      PCResHelp pHelp = new CResHelp;
      if (!pHelp)
         continue;
      if (!pHelp->MMLFrom (*((PCMMLNode2*)pRes->m_lPCMMLNode2.Get(dwIndex)), m_lid))
         continue;

      // if the title already exists in the tree then delete this entry and
      // dont add
      if (m_tTitles.Find ((PWSTR)pHelp->m_memName.p)) {
         delete pHelp;
         continue;
      }

      // add to tree
      m_tTitles.Add ((PWSTR)pHelp->m_memName.p, &pHelp, sizeof(pHelp));

      // index properly... always gets added to at least one category,
      // maybe more
      OHCATEGORYAdd ((PWSTR)pHelp->m_aMemHelp[0].p, pHelp, &m_OHCATEGORY);
      if (((PWSTR)pHelp->m_aMemHelp[1].p)[0])
         OHCATEGORYAdd ((PWSTR)pHelp->m_aMemHelp[1].p, pHelp, &m_OHCATEGORY);

      // include this in the checksum
      DWORD dwCS = pHelp->CheckSum();
      DWORD dwRot = (i % 32); // so location dependent
      dwCS = (dwCS << i) + (dwCS >> (32 - i));
      dwCheckSum ^= dwCS;
   } // i

   // come up with the name
   WCHAR szTemp[256];
   wcscpy (szTemp, pszDir);
   DWORD dwLen = (DWORD)wcslen(szTemp);
   if (!dwLen || (szTemp[dwLen-1] != L'\\'))
      wcscat (szTemp, L"\\");
   swprintf (szTemp + wcslen(szTemp), L"OnlineHelp%d.xsr", (int)m_lid);

   // search database
   m_Search.Init (ghInstance, dwCheckSum, szTemp);

   if (m_Search.NeedIndexing()) {
      ESCINDEX i;
      memset (&i, 0, sizeof(i));
      i.hWndUI = hWnd;
      i.pCallback = HelpDefPage;
      i.fNotEnumMML = TRUE;
      i.pIndexCallback = SearchCallback;
      i.dwIndexDocuments = m_tTitles.Num();
      i.pIndexUserData = &m_tTitles;

      m_Search.Index (&i);
   }

   return TRUE;
}
