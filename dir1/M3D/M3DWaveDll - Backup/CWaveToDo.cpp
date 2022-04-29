/***************************************************************************
CWaveToDo.cpp - Code to handle the to-do recording list for speech recognition
and text-to-speech.

begun 20/9/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <shlwapi.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


/***************************************************************************
CWaveToDo::Constructor and destructor
*/
CWaveToDo::CWaveToDo (void)
{
   // nothing for now
}

CWaveToDo::~CWaveToDo (void)
{
   // nothing for no
}

/***************************************************************************
CWaveToDo::MMLTo - Standard api
*/
static PWSTR gpszToDo = L"ToDo";
static PWSTR gpszItem = L"Item";
PCMMLNode2 CWaveToDo::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszToDo);

   DWORD i;
   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < m_lToDo.Num(); i++) {
      psz = (PWSTR)m_lToDo.Get(i);
      if (!psz || !psz[0])
         continue;   // blank so dont add

      pSub = pNode->ContentAddNewNode();
      if (!pSub)
         continue;
      pSub->NameSet (gpszItem);

      MMLValueSet (pSub, Text(), psz);
   } // i

   return pNode;
}

/***************************************************************************
CWaveToDo::MMLFrom - Standard api
*/
BOOL CWaveToDo::MMLFrom (PCMMLNode2 pNode)
{
   m_lToDo.Clear();

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszItem)) {
         psz = MMLValueGet (pSub, Text());
         if (!psz)
            continue;
         m_lToDo.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
         continue;
      }
   } // i

   return TRUE;
}

/***************************************************************************
CWaveToDo::Clone - Standard API
*/
CWaveToDo *CWaveToDo::Clone (void)
{
   PCWaveToDo pNew = new CWaveToDo;
   if (!pNew)
      return NULL;

   DWORD i;
   PWSTR psz;
   pNew->m_lToDo.Required (m_lToDo.Num());
   for (i = 0; i < m_lToDo.Num(); i++) {
      psz = (PWSTR)m_lToDo.Get(i);
      if (!psz)
         continue;
      pNew->m_lToDo.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   }

   return pNew;
}


/******************************************************************************
CWaveToDo::FillListBox - Fills a list box with the to-do list.
Each item has a name of "f:xxx" where xxx is the full text string.

inputs
   PCEscPage         pPage - Page containing the list box
   PCWSTR            pszControl - Control name
   PCWSTR            pszTextFilter - Filter text out by this
returns
   BOOL - TRUE if success
*/

static int _cdecl PWSTRSort (const void *elem1, const void *elem2)
{
   PWSTR *pdw1, *pdw2;
   pdw1 = (PWSTR*) elem1;
   pdw2 = (PWSTR*) elem2;

   return _wcsicmp(pdw1[0], pdw2[0]);
}

BOOL CWaveToDo::FillListBox (PCEscPage pPage, PCWSTR pszControl, PCWSTR pszTextFilter)
{
   PCEscControl pControl = pPage->ControlFind ((PWSTR)pszControl);
   if (!pControl)
      return TRUE;
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   // sort the list of phrases alphabetically
   CListFixed lSort;
   lSort.Init (sizeof(PWSTR));
   DWORD i;
   PWSTR psz;
   lSort.Required (m_lToDo.Num());
   for (i = 0; i < m_lToDo.Num(); i++) {
      psz = (PWSTR)m_lToDo.Get(i);
      if (!psz)
         continue;
      lSort.Add (&psz);
   }

   qsort (lSort.Get(0), lSort.Num(), sizeof(PWSTR), PWSTRSort);


   // add the names
   MemZero (&gMemTemp);

   DWORD dwNum = lSort.Num();
   PWSTR *ppsz = (PWSTR*)lSort.Get(0);
   for (i = 0; i < dwNum; i++) {
      psz = ppsz[i];

      // filter by text/speaker
      if (pszTextFilter[0]) {
         if (!MyStrIStr(psz, pszTextFilter))
            continue;
      }

      MemCat (&gMemTemp, L"<elem name=\"f:");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"\">");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"<p/></elem>");
   } // i


   // add this to the list box
   ESCMLISTBOXADD lba;
   memset (&lba, 0, sizeof(lba));
   lba.dwInsertBefore = -1;
   lba.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_LISTBOXADD, &lba);

   // set the selection
   pControl->AttribSetInt (CurSel(), 0);

   return TRUE;
}

/******************************************************************************
CWaveToDo::RemoveToDo - Removes an item from the to-do list

inputs
   PCWSTR         pszText - Text to find and then remobe
returns
   BOOL - TRUE if found text, FALSE if not
*/
BOOL CWaveToDo::RemoveToDo (PCWSTR pszText)
{
   DWORD i;
   for (i = 0; i < m_lToDo.Num(); i++) {
      PWSTR psz = (PWSTR) m_lToDo.Get(i);
      if (!psz)
         continue;
      if (!_wcsicmp(psz, pszText)) {
         m_lToDo.Remove (i);
         return TRUE;
      }
   } // i
   return FALSE;
}


/******************************************************************************
CWaveToDo::AddToDo - Adds a string onto the to-do list.

inputs
   PWSTR       pszText - Text string to add
returns
   BOOL - TRUE if success
*/
BOOL CWaveToDo::AddToDo (PCWSTR pszText)
{
   return -1 != m_lToDo.Add ((PVOID)pszText, (wcslen(pszText)+1)*sizeof(WCHAR));
}

/******************************************************************************
CWaveToDo::ClearToDo - Clears the entire to-do list
*/
void CWaveToDo::ClearToDo (void)
{
   m_lToDo.Clear();
}


