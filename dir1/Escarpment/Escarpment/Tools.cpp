/*******************************************************************8
Tools.cpp - useful c++ tools.

begun 3/16/2000 by Mike Rozak
Copyright 2000 Mike Rozka. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "resleak.h"

#define SMALLREQUIRED         // dont allocate as much for required
#ifdef SMALLREQUIRED
#define ROUNDREQUIREDSIZE     4        // 4 bytes
#define MEMFRACTION           4        // fraction of extra memory to expand by.
#define MEMEXTRA              32       // memory to allocate above and beyond whats asked for
#else
#define ROUNDREQUIREDSIZE     32        // 32 bytes, old value
#define MEMFRACTION           2        // fraction or memory to expand by
#define MEMEXTRA              128      // memory to allocate above and beyond whats asked for
#endif
// BUGFIX - upped memextra because changed realloc code

// #define  MEMEXTRA          32      // memory to allocate above and beyond whats asked for
// BUGFIX - Lowered to 32 to save memory
// BUGFIX - changed from 128 to 64 because when was 128 crashed when loading a large datafile for house app. Seems like memory overrun someplace

typedef struct {
   PVOID    pMem;
   size_t    dwSize;
} LISTINFO, *PLISTINFO;

/****************************************************************
CMem - memory object */

CMem::CMem (void)
{
   p = NULL;
   m_dwAllocated = m_dwCurPosn = 0;
}

CMem::~CMem (void)
{
   if (p)
      ESCFREE (p);
}


void CMem::ClearCompletely (void)
{
   if (p)
      ESCFREE (p);
   p = NULL;
   m_dwAllocated = m_dwCurPosn = 0;

}



BOOL CMem::Required (size_t dwSize)
{
   if (dwSize <= m_dwAllocated)
      return TRUE;

   size_t dwNeeded;
   // BUGFIX - Changed so that the first time it matches exactly. After
   // that it does incremental increases
   // BUGFIX - So it's not too slow, if it's the second time around always
   // allocate 50% more
   dwNeeded = dwSize;
   if (p && (m_dwAllocated > 20))   // BUGFIX - If still small memory then dont add large chunks for awhile, keep memory low
      dwNeeded += (dwSize / MEMFRACTION + MEMEXTRA);

   // BUGFIX - Try to allocate on 32-byte boundaries. Hope that this will crash
   // a memory overwrite (or maybe bug in realloc) that's causing a crash
   // when realloc is called. Have spent much time stepping through the code and
   // can't find overwritten memory. I don't think this worked but I'll leave it in anyway
   size_t dwMore;
   dwMore = dwNeeded % ROUNDREQUIREDSIZE;
   if (dwMore && p)
      dwNeeded += ROUNDREQUIREDSIZE - dwMore;

   PVOID p2;
   // BUGFIX - Put hack in and see if fixes problems...
#define USEREALLOC
#ifndef USEREALLOC // take out hack now that on WinNT
   // Temporary hack to prevent crashes that seem to occur when I use
   // realloc. They don't happen when I use malloc.
   p2 = ESCMALLOC (dwNeeded);
   if (!p2)
      return FALSE;
   if (p) {  
      memcpy (p2, p, m_dwAllocated);
#ifdef _DEBUG
      memset (p, 0xdfdfdfdf, m_dwAllocated);
#endif
      ESCFREE (p);
   }
#else // USEREALLOC
   // old code
   p2 = ESCREALLOC (p, dwNeeded);

   if (!p2)
      return FALSE;
#endif // 0

//#ifdef _DEBUG
//   if (dwNeeded == 416)
//      dwNeeded = 416;
//#endif

   p = p2;
   m_dwAllocated = dwNeeded;

   return TRUE;
}

/* returns an offset from CMem.p */
size_t CMem::Malloc (size_t dwSize)
{
   size_t dwNeeded, dwStart;
   // BUGFIX - Changed "% ~0x03" to "& 0x03"
   dwStart = (m_dwCurPosn + 3) & ~0x03; // DWORD align
   dwNeeded = dwStart + dwSize;
   if (!Required (dwNeeded))
      return NULL;

   m_dwCurPosn = dwNeeded;

   return dwStart;
}

/* Add a string to the dwCur. Don't do terminating NULL though */
BOOL CMem::StrCat (PCWSTR psz, DWORD dwCount)
{
//#ifdef _DEBUG // hack to test memory lea
//   if ((sizeof(PVOID) > sizeof(DWORD)) && p)
//      EscMemoryIntegrity(p);
//#endif

   if (dwCount == (DWORD)-1) {
      dwCount = (DWORD)wcslen(psz);
   }

   size_t dwNeeded;
   dwNeeded = m_dwCurPosn + dwCount * sizeof(WCHAR);
   if (!Required(dwNeeded))
      return FALSE;

   // else ad
   memcpy ((PBYTE) p + m_dwCurPosn, psz, dwCount * sizeof(WCHAR));
   m_dwCurPosn += dwCount * sizeof(WCHAR);

//#ifdef _DEBUG // hack to test memory lea
//   if ((sizeof(PVOID) > sizeof(DWORD)) && p)
//      EscMemoryIntegrity(p);
//#endif

   return TRUE;
}

/* Add a character */
BOOL CMem::CharCat (WCHAR c)
{
   return StrCat (&c, 1);
}

/*********************************************************8
CListVariable
*/
CListVariable::CListVariable (void)
{
   m_dwElem = 0;
}

CListVariable::~CListVariable (void)
{
   Clear ();
}


BOOL    CListVariable::Insert (DWORD dwElem, PVOID pMem, size_t dwSize)
{
   if (dwElem > m_dwElem)
      dwElem = m_dwElem;

   // BUGFIX - Try to root out crash
   size_t dwNeed;
   dwNeed = sizeof(LISTINFO) * (m_dwElem+1);
//#ifdef _DEBUG
//   if ((dwNeed >= m_apv.m_dwAllocated) && (m_apv.m_dwAllocated == 168)) {
      //_CrtCheckMemory ();
//      dwNeed = dwNeed;
//  }
//#endif
   if (!m_apv.Required (dwNeed))
      return FALSE;


   // allocate memory
   PVOID p;
   p = ESCMALLOC (dwSize);
   if (!p)
      return FALSE;
   // BUGFIX - Allow a NULL for the add/insert into list, in which case blank
   if (pMem)
      memcpy (p, pMem, dwSize);

   // move memory
   memmove ((PLISTINFO)m_apv.p + (dwElem+1), (PLISTINFO)m_apv.p + dwElem, (m_dwElem - dwElem) * sizeof(LISTINFO));

   // put in new stuff
   ((PLISTINFO)m_apv.p)[dwElem].pMem = p;
   ((PLISTINFO)m_apv.p)[dwElem].dwSize = dwSize;

   m_dwElem++;

   return TRUE;
}


/* Required - Pre-allocate memory for that many elements */
BOOL     CListVariable::Required (DWORD dwElem)
{
   return m_apv.Required (sizeof(LISTINFO) * dwElem);
}


DWORD    CListVariable::Add (PVOID pMem, size_t dwSize)
{
   DWORD dw;
   dw = m_dwElem;

   if (!Insert (dw, pMem, dwSize))
      return (DWORD)-1;
   return dw;
}

BOOL    CListVariable::Set (DWORD dwElem, PVOID pMem, size_t dwSize)
{
   if (dwElem >= m_dwElem)
      return FALSE;

   PLISTINFO   pi;
   pi = (PLISTINFO)m_apv.p + dwElem;

   if (pi->pMem)
      ESCFREE (pi->pMem);

   pi->pMem = ESCMALLOC (dwSize);
   if (!pi->pMem) {
      pi->dwSize = 0;
      return FALSE;
   }
   memcpy (pi->pMem, pMem, dwSize);
   pi->dwSize = dwSize;

   return TRUE;
}

BOOL     CListVariable::Remove (DWORD dwElem)
{
   if (dwElem >= m_dwElem)
      return FALSE;

   PLISTINFO pi;
   pi = (PLISTINFO) m_apv.p + dwElem;
   if (pi->pMem)
      ESCFREE (pi->pMem);

   memmove (pi, pi + 1, (m_dwElem - dwElem - 1) * sizeof(LISTINFO));
   m_dwElem--;

   return TRUE;
}

#if 0 // inlined
DWORD    CListVariable::Num (void)
{
   return m_dwElem;
}
#endif

PVOID    CListVariable::Get (DWORD dwElem)
{
   if (dwElem >= m_dwElem)
      return NULL;

   return ((PLISTINFO)m_apv.p)[dwElem].pMem;
}

size_t    CListVariable::Size (DWORD dwElem)
{
   if (dwElem >= m_dwElem)
      return NULL;

   return ((PLISTINFO)m_apv.p)[dwElem].dwSize;
}

void     CListVariable::Clear (void)
{
   DWORD i;

   for (i = 0; i < m_dwElem; i++)
      if (((PLISTINFO) m_apv.p)[i].pMem)
         ESCFREE (((PLISTINFO) m_apv.p)[i].pMem);
   m_dwElem = 0;
}

void CListVariable::ClearCompletely (void)
{
   Clear();
   m_apv.ClearCompletely();
}

/* merge - mergest pMerge onto the end of the current list. pMerge
is then left with nothing. The list elements retain the same pointers */
BOOL CListVariable::Merge (CListVariable *pMerge)
{
   // needed
   if (!m_apv.Required (sizeof(LISTINFO) * (m_dwElem + pMerge->m_dwElem)))
      return FALSE;

   // copy
   memcpy ( ((LISTINFO*) m_apv.p) + m_dwElem, pMerge->m_apv.p, sizeof(LISTINFO)*pMerge->m_dwElem);
   m_dwElem += pMerge->m_dwElem;
   pMerge->m_dwElem = 0;

   return TRUE;
}

// FileWrite - Write the contents of the list to a file.
BOOL CListVariable::FileWrite (FILE *pf)
{
   if (1 != fwrite (&m_dwElem, sizeof(m_dwElem), 1, pf))
      return FALSE;

   // write all the elements
   DWORD i;
   size_t dwSize;
   PVOID pElem;
   for (i = 0; i < m_dwElem; i++) {
      pElem = Get (i);
      dwSize = Size (i);

      // write
      if (1 != fwrite (&dwSize, sizeof(dwSize), 1, pf))
         return FALSE;
      if (dwSize != fwrite (pElem, 1, dwSize, pf))
         return FALSE;
   }

   // done
   return TRUE;
}

// FileRead - Read the contents of the file into the list.
BOOL CListVariable::FileRead (FILE *pf)
{
   Clear();

   DWORD dwNum;
   if (1 != fread(&dwNum, sizeof(dwNum), 1, pf))
      return FALSE;

   DWORD i, dwSize;
   CMem  memTemp;
   for (i = 0; i < dwNum; i++) {
      // size
      if (1 != fread (&dwSize, sizeof(dwSize), 1, pf))
         return FALSE;

      // make sure have that much memory
      if (!memTemp.Required (dwSize))
         return FALSE;

      // read it in
      if (dwSize != fread (memTemp.p, 1, dwSize, pf))
         return FALSE;

      // write it
      if ((DWORD)-1 == Add (memTemp.p, dwSize))
         return FALSE;
   }

   // all done
   return TRUE;
}


/***********************************************************8
CListFixed - List of fixed size elements
*/
CListFixed::CListFixed (void)
{
   m_dwElemSize = 1;
   m_dwElem = 0;
}

CListFixed::~CListFixed (void)
{
   // do nothing
}


BOOL     CListFixed::Init (DWORD dwElemSize)
{
   m_dwElemSize = dwElemSize;
   m_dwElem = 0;

   return TRUE;
}

BOOL     CListFixed::Init (DWORD dwElemSize, PVOID paElems, DWORD dwElems)
{
   m_dwElemSize = dwElemSize;
   if (!m_apv.Required (dwElems * dwElemSize))
      return FALSE;

   m_dwElem = dwElems;
   memcpy (m_apv.p, paElems, m_dwElem * m_dwElemSize);
   return TRUE;
}


BOOL     CListFixed::Insert (DWORD dwElem, PVOID pMem)
{
   if (dwElem > m_dwElem)
      dwElem = m_dwElem;

   if (!m_apv.Required (m_dwElemSize * (m_dwElem+1)))
      return FALSE;

   // move memory
   memmove ((PBYTE)m_apv.p + (dwElem+1) * m_dwElemSize,
      (PBYTE)m_apv.p + dwElem * m_dwElemSize, (m_dwElem - dwElem) * m_dwElemSize);

   // put in new stuff
   memcpy ((PBYTE)m_apv.p + dwElem*m_dwElemSize, pMem, m_dwElemSize);

   m_dwElem++;

   return TRUE;
}

/* Truncate - cut off the end elements */
BOOL     CListFixed::Truncate (DWORD dwElems)
{
   m_dwElem = min(m_dwElem, dwElems);
   return TRUE;
}

/* Required - Pre-allocate memory for that many elements */
BOOL     CListFixed::Required (DWORD dwElem)
{
   return m_apv.Required (m_dwElemSize * dwElem);
}

DWORD    CListFixed::Add (PVOID pMem)
{
   DWORD dw;
   dw = m_dwElem;

   if (!Insert (dw, pMem))
      return (DWORD)-1;
   return dw;
}

BOOL    CListFixed::Set (DWORD dwElem, PVOID pMem)
{
   if (dwElem >= m_dwElem)
      return FALSE;

   memcpy ((PBYTE)m_apv.p + dwElem*m_dwElemSize, pMem, m_dwElemSize);

   return TRUE;
}

BOOL     CListFixed::Remove (DWORD dwElem)
{
   if (dwElem >= m_dwElem)
      return FALSE;

   PBYTE pi;
   pi = (PBYTE) m_apv.p + dwElem * m_dwElemSize;

   memmove (pi, pi + m_dwElemSize, (m_dwElem - dwElem - 1) * m_dwElemSize);
   m_dwElem--;

   return TRUE;
}

#if 0 // inlined
DWORD    CListFixed::Num (void)
{
   return m_dwElem;
}
#endif 0

/* note - return is only valid until add/insert/remove an item*/
#if 0 // inlined
PVOID    CListFixed::Get (DWORD dwElem)
{
   if (dwElem >= m_dwElem)
      return NULL;

   return (PVOID) ((PBYTE)m_apv.p + m_dwElemSize * dwElem);
}
#endif // 0

void     CListFixed::Clear (void)
{
   m_dwElem = 0;
}


void CListFixed::ClearCompletely (void)
{
   Clear();
   m_apv.ClearCompletely();
}



/* merge - mergest pMerge onto the end of the current list. pMerge
is then left with nothing. */
BOOL CListFixed::Merge (CListFixed *pMerge)
{
   if (m_dwElemSize != pMerge->m_dwElemSize)
      return FALSE;

   // needed
   if (!m_apv.Required (m_dwElemSize * (m_dwElem + pMerge->m_dwElem)))
      return FALSE;

   // copy
   memcpy ( ((PBYTE) m_apv.p) + m_dwElem * m_dwElemSize, pMerge->m_apv.p, m_dwElemSize*pMerge->m_dwElem);
   m_dwElem += pMerge->m_dwElem;
   pMerge->m_dwElem = 0;

   return TRUE;
}

/************************************************************************
CBTree - An add-only binary tree.

Usefule variables
   m_fIgnoreCase - If TRUE ignore case in string compare. Default true.
*/

typedef struct {
   DWORD    dwLeft;     // left index. -1 if no left
   DWORD    dwRight;    // right branch. (wscicmp > 0) -1 if no right
   size_t    dwSize;     // size of user data.
   // null-terminated WCHAR follows
   // and immediately after user data, of size dwSize
} TREENODE, *PTREENODE;

/************************************************************************
CBTree:: constructor & destructor
*/
CBTree::CBTree (void)
{
   m_fIgnoreCase = TRUE;
}

CBTree::~CBTree (void)
{
   // intentionally do nothing
}


/************************************************************************
CBTree::AddToNodes - Realigns the node structure so the element is added

inputs
   DWORD    dwElemNum - where the node exists
   WCHAR    *pszKey - key, being added
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CBTree::AddToNodes (DWORD dwElemNum, WCHAR *pszKey)
{
   // if it's node 0 then trivial
   if (!dwElemNum)
      return TRUE;

   // else, figure out which node to add it to
   DWORD dwCurNode, *pdwModify;
   PTREENODE   ptn2;
   dwCurNode = 0;
   pdwModify = &dwCurNode;
   while (*pdwModify != (DWORD) -1) {
      ptn2 = (PTREENODE) m_list.Get (*pdwModify);
      if (!ptn2)
         return FALSE;  // some sort of error

      // left/right
      int   iRet;
      if (m_fIgnoreCase)
         iRet = _wcsicmp ((WCHAR*) (ptn2+1), pszKey);
      else
         iRet = wcscmp ((WCHAR*) (ptn2+1), pszKey);

      if (iRet == 0) {
         // This shouldn't happen for AddToNodes()
         return FALSE;
      }

      // else move on
      if (iRet >= 0)
         pdwModify = &ptn2->dwRight;
      else
         pdwModify = &ptn2->dwLeft;

   }

   // if got here, at the end, so we're done
   *pdwModify = dwElemNum;

   // done
   return TRUE;
}

/************************************************************************
CBTree::Add - Add an element. If the name already exists then replace.

inputs
   WCHAR    *pszKey - key
   PVOID    pData - Data
   DWORD    dwSize - data size
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CBTree::Add (WCHAR *pszKey, PVOID pData, size_t dwSize)
{
   // make up the data
   CMem  mem;
   size_t dwNeeded;
   dwNeeded = sizeof (TREENODE) + (wcslen(pszKey)+1) * sizeof(WCHAR) + dwSize;
   if (!mem.Required (dwNeeded))
      return FALSE;
   PTREENODE   ptn;
   ptn = (PTREENODE) mem.p;
   ptn->dwLeft = ptn->dwRight = -1;
   ptn->dwSize = dwSize;
   wcscpy ((WCHAR*) (ptn + 1), pszKey);
   memcpy ((PBYTE) ptn + (dwNeeded - dwSize), pData, dwSize);

   // the the element onto the end
   DWORD dwElemNum;
   dwElemNum = m_list.Num();

   // see if can find the element already
   DWORD dwFind;
   dwFind = FindNum (pszKey);
   if (dwFind != (DWORD) -1) {
      // found it, so we basically end up doing a set
      PTREENODE   ptn2;
      ptn2 = (PTREENODE) m_list.Get (dwFind);
      if (!ptn2)
         return FALSE;

      ptn->dwLeft = ptn2->dwLeft;
      ptn->dwRight = ptn2->dwRight;

      return m_list.Set (dwFind, mem.p, dwNeeded);
   }

   // else add it
   m_list.Add (mem.p, dwNeeded);

   // if there are no elements this is trivial
   if (!dwElemNum)
      return TRUE;

   // readjust the tree
   return AddToNodes (dwElemNum, pszKey);
}

/************************************************************************
CBTree::FindNum - Find an element and return the element number.

inputs
   WCHAR    *pszKey - key
   DWORD    *pdwSize - filled in with the size
reutnrs
   DWORD - element num. -1 if cant find
*/
DWORD CBTree::FindNum (WCHAR *pszKey)
{
   // else, figure out which node to add it to
   DWORD dwCurNode, *pdwModify;
   PTREENODE   ptn2;
   dwCurNode = 0;
   pdwModify = &dwCurNode;
   while (*pdwModify != (DWORD) -1) {
      ptn2 = (PTREENODE) m_list.Get (*pdwModify);
      if (!ptn2)
         return (DWORD) -1;  // cant find

      // left/right
      int   iRet;
      if (m_fIgnoreCase)
         iRet = _wcsicmp ((WCHAR*) (ptn2+1), pszKey);
      else
         iRet = wcscmp ((WCHAR*) (ptn2+1), pszKey);

      if (iRet == 0) {
         // found it
         return *pdwModify;
      }

      // else move on
      if (iRet >= 0)
         pdwModify = &ptn2->dwRight;
      else
         pdwModify = &ptn2->dwLeft;

   }

   // if get here cant find
   return (DWORD) -1;
}


/************************************************************************
CBTree::GetNum - Get an element based on an index.

inputs
   DWORD    dwIndex - Index to get
   DWORD    *pdwSize - filled in with the size.  Can be null
reutnrs
   PVOID - pointer to the element data. NULL if error
*/
PVOID CBTree::GetNum (DWORD dwIndex, size_t *pdwSize)
{
   // else, figure out which node to add it to
   PTREENODE   ptn2;
   ptn2 = (PTREENODE) m_list.Get (dwIndex);
   if (!ptn2)
      return NULL;  // cant find

   if (pdwSize)
      *pdwSize = ptn2->dwSize;
   return (PVOID) ((PBYTE) (ptn2+1) + (wcslen((WCHAR*) (ptn2+1)) +1) * sizeof(WCHAR));
}


/************************************************************************
CBTree::Find - Find an element.

inputs
   WCHAR    *pszKey - key
   DWORD    *pdwSize - filled in with the size.  Can be null
reutnrs
   PVOID - pointer to the element data. NULL if error
*/
PVOID CBTree::Find (WCHAR *pszKey, size_t *pdwSize)
{
   if (pdwSize)
      *pdwSize = 0;

   DWORD dwNode;
   dwNode = FindNum (pszKey);
   if (dwNode == (DWORD) -1)
      return NULL;

   return GetNum(dwNode, pdwSize);
}

/************************************************************************
CBTree::Num - Returns the number of elements

inputs
   none
returns
   DWORD - number
*/
#if 0 // inlined
DWORD CBTree::Num (void)
{
   return m_list.Num();
}
#endif // 0

/************************************************************************
CBTree::Enum - Enumerate the Nth element

inputs
   DWORD dwNum
returns
   WCHAR * - Name of the element. NULL if dwNum is <= number elements.
         The name is valid until the element is changed.
*/
WCHAR * CBTree::Enum (DWORD dwNum)
{
   PTREENODE   ptn;
   ptn = (PTREENODE) m_list.Get (dwNum);
   if (!ptn)
      return NULL;

   return (WCHAR*) (ptn+1);
}

/************************************************************************
CBTree::Remove. Removes an element from the tree. Because I'm lazy
the remove function is actually somewhat slow, so don't call it too often.

inputs
   WCHAR    *psz - string to remove
returns
   BOOL - TRUE if found and removed, FALSE if not found
*/
BOOL CBTree::Remove (WCHAR *pszKey)
{
   DWORD dwFind;
   dwFind = FindNum (pszKey);
   if (dwFind == (DWORD)-1)
      return FALSE;

   // delete the element
   m_list.Remove (dwFind);

   // readjust all the existing elements
   PTREENODE   ptn;
   DWORD i;
   for (i = 0; i < m_list.Num(); i++) {
      ptn = (PTREENODE) m_list.Get(i);
      if (!ptn)
         return NULL;

      // set the left and right to -1
      ptn->dwLeft = ptn->dwRight = (DWORD)-1;

      // re-add it
      if (!AddToNodes (i, (WCHAR*) (ptn+1)))
         return FALSE;
   }

   // done
   return TRUE;
}

/*******************************************************************
CBTree::Clear - Clears out the list
*/
void CBTree::Clear (void)
{
   m_list.Clear();
}


void CBTree::ClearCompletely (void)
{
   Clear();
   m_list.ClearCompletely();
}

/********************************************************************
CBTree::FileWrite - write the tree to a file
*/
BOOL CBTree::FileWrite (FILE *pf)
{
   return m_list.FileWrite(pf);
}


/********************************************************************
CBTree::FileRead - read the tree from a file
*/
BOOL CBTree::FileRead (FILE *pf)
{
   return m_list.FileRead(pf);
}






/*****************************************************************************************
CHashString::Constructor and destructor
*/
CHashString::CHashString (void)
{
   m_dwElemSize = 0;
   Clear();
}

CHashString::~CHashString (void)
{
   // do nothing for now
}

/*****************************************************************************************
CHashString::Init - Initializes the hash table.

inputs
   DWORD       dwElemSize - Number of bytes used for each element
   DWORD       dwTableSize - Optionally, initial size for the lookup table
*/

// NOTE: OLDSTRINGHASH is NOT used because new one is needed to ensure that
// the atmoic string functions work

void CHashString::Init (DWORD dwElemSize, DWORD dwTableSize)
{
   m_dwElemSize = dwElemSize;
#ifdef OLDSTRINGHASH
   m_lElem.Init (m_dwElemSize + sizeof(DWORD));
#else
   m_lElem.Init (m_dwElemSize);
#endif // OLDSTRINGHASH
   Clear();

   if (dwTableSize)
      TableResize (dwTableSize);
}


/*****************************************************************************************
CHashString::CloneTo - Clones the hash to a destination

inputs
   PCHashString         pTo - Clone to
returns
   BOOL - TRUE if success
*/
BOOL CHashString::CloneTo (CHashString *pTo)
{
   pTo->m_dwElemSize = m_dwElemSize;
   pTo->m_dwTableSize = m_dwTableSize;
#ifdef OLDSTRINGHASH
   pTo->m_lElem.Init (m_dwElemSize + sizeof(DWORD), m_lElem.Get(0), m_lElem.Num());

   if (!pTo->m_memString.Required (m_memString.m_dwCurPosn))
      return FALSE;
   memcpy (pTo->m_memString.p, m_memString.p, m_memString.m_dwCurPosn);
   pTo->m_memString.m_dwCurPosn = m_memString.m_dwCurPosn;
#else
   pTo->m_lElem.Init (m_dwElemSize, m_lElem.Get(0), m_lElem.Num());

   DWORD i;
   pTo->m_lStrings.Clear();
   pTo->m_lStrings.Required (m_lStrings.Num());
   for (i = 0; i < m_lStrings.Num(); i++)
      pTo->m_lStrings.Add (m_lStrings.Get(i), m_lStrings.Size(i));
      // NOTE: m_lStrings clone not tested

#endif

   if (!pTo->m_memTable.Required (m_dwTableSize * sizeof(DWORD)))
      return FALSE;
   memcpy (pTo->m_memTable.p, m_memTable.p, m_dwTableSize * sizeof(DWORD));

   return TRUE;
}


/*****************************************************************************************
CHashString::Clone - Clones the hash and returns a new one
*/
CHashString *CHashString::Clone (void)
{
   PCHashString pNew = new CHashString;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;

}

/*****************************************************************************************
CHashString::Clear - Clears out the hash table
*/
void CHashString::Clear (void)
{
   // NOTE: Not changing elem size
   m_dwTableSize = 0;
   m_lElem.Clear();
#ifdef OLDSTRINGHASH
   m_memString.m_dwCurPosn = 0;
#else
   m_lStrings.Clear();
#endif
}


/*****************************************************************************************
CHashString::Num - Returns the number of elements stored in the hash table
*/
DWORD CHashString::Num (void)
{
   return m_lElem.Num();
}


/*********************************************************************************
MIFLToLower - Lowercases a string

inputs
   PWSTR       psz
*/
static void MIFLToLower (PWSTR psz)
{
   for (; psz[0]; psz++)
      psz[0] = towlower(psz[0]);
}



/*****************************************************************************************
CHashString::Add - Adds a new element to the hash table.

inputs
   PWSTR          pszString - String to identify the element
   PVOID          pData - Data being added.
   BOOL           fToLower - If TRUE then the string is automatically converted to lower case
                     so that searching can be case insensative
returns
   BOOL - TRUE if success
*/
BOOL CHashString::Add (PWSTR pszString, PVOID pData, BOOL fToLower)
{
#ifdef OLDSTRINGHASH
   BYTE abScratch[256];
   DWORD dwNeed = sizeof(DWORD) + m_dwElemSize;
   CMem memScratch;
   DWORD *pdw;
   if (dwNeed <= sizeof(abScratch)) {
      pdw = (DWORD*) &abScratch[0];
   }
   else {
      if (!memScratch.Required (dwNeed))
         return FALSE;
      pdw = (DWORD*)memScratch.p;
   }
   DWORD dwOld = m_memString.m_dwCurPosn;
   memcpy (pdw+1, pData, m_dwElemSize);
#else
   DWORD *pdw = (DWORD*)pData;
#endif

   // allocate for the string
#ifdef OLDSTRINGHASH
   dwNeed = (wcslen(pszString)+1)*sizeof(WCHAR);
   pdw[0] = m_memString.Malloc (dwNeed);
   if (dwOld && !pdw[0] || !m_memString.p)
      return FALSE;
   PWSTR psz = (PWSTR)((PBYTE)m_memString.p + pdw[0]);
   memcpy (psz, pszString, dwNeed);
#else
   m_lStrings.Add (pszString, (wcslen(pszString)+1)*sizeof(WCHAR));
   PWSTR psz = (PWSTR) m_lStrings.Get(m_lStrings.Num()-1);
#endif

   // add element
   m_lElem.Add (pdw);

   // convert to lower case
   if (fToLower)
      MIFLToLower (psz);

   // if has table is too small then enlarge
   if (m_lElem.Num() *2 > m_dwTableSize)
      return TableResize (m_lElem.Num()*4+1);   // add one to try not to be nice integer multiple
         // make a fairly sparse table to low chance of collision
         //Calling table resize will automatically add the hash element in the right place

   // else, need to add
   DWORD dwHash = Hash(psz);
   DWORD *padwTable = (DWORD*)m_memTable.p;
   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }
   padwTable[dwHash] = m_lElem.Num();  // since storing index+1

   return TRUE;
}


/*****************************************************************************************
CHashString::Get - Returns the data used by the element at the given index

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   PVOID - Data
*/
PVOID CHashString::Get (DWORD dwIndex)
{
   DWORD *pdw = (DWORD*)m_lElem.Get(dwIndex);
   if (!pdw)
      return NULL;
#ifdef OLDSTRINGHASH
   return (pdw+1);
#else
   return pdw;
#endif
}


/*****************************************************************************************
CHashString::GetString - Returns the string used by the element at the given index

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   PWSTR - string. Do not change
*/
PWSTR CHashString::GetString (DWORD dwIndex)
{
#ifdef OLDSTRINGHASH
   DWORD *pdw = (DWORD*)m_lElem.Get(dwIndex);
   if (!pdw)
      return NULL;
   PWSTR psz = (PWSTR)((PBYTE)m_memString.p + pdw[0]);
   return psz;
#else
   return (PWSTR)m_lStrings.Get(dwIndex);
#endif
}

/*****************************************************************************************
CHashString::Remove - Removed an item based on its index... note: This will force the
table to be rebuilt...

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   BOOL - TRUE if success
*/
BOOL CHashString::Remove (DWORD dwIndex)
{
   if (dwIndex >= m_lElem.Num())
      return FALSE;

   m_lElem.Remove (dwIndex);

#ifndef OLDSTRINGHASH
   m_lStrings.Remove (dwIndex);
#endif

   // NOTE: Not tested, but since cut and paste of other hash remove, should
   // be ok

   // look through the table and reduce indecies that are higher by one,
   // and note where this index occurred
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i;
   DWORD dwLookFor = dwIndex+1;
   DWORD dwOldLoc = (DWORD)-1;
   for (i = 0; i < m_dwTableSize; i++, padwTable++) {
      if (*padwTable < dwLookFor)
         continue;   // either 0, or less than
      else if (*padwTable > dwLookFor)
         *padwTable = *padwTable - 1;  // since deleted it
      else {
         dwOldLoc = i;
         *padwTable = 0;
      }
   } // i

   // find all the elements occurring sequentially after dwOldLoc and remap
   CListFixed lRemap;
   DWORD j;
   lRemap.Init (sizeof(DWORD));
   // Don't use - lRemap.Required (m_dwTableSize);
   padwTable = (DWORD*)m_memTable.p;
   for (i = 1; i < m_dwTableSize; i++) {
      j = (dwOldLoc+i)%m_dwTableSize;
      if (padwTable[j]) {
         lRemap.Add (&padwTable[j]);
         padwTable[j] = 0;
      }
      else
         break;   // got to blank so broke the chain
   }

   // readd these
   DWORD *pdwRemap = (DWORD*) lRemap.Get(0);
   DWORD dwHash;
   for (j = 0; j < lRemap.Num(); j++) {
      i = pdwRemap[j] - 1;

#ifdef OLDSTRINGHASH
      DWORD *pdw = (DWORD*)m_lElem.Get(i);
      PWSTR psz = (PWSTR)((PBYTE)m_memString.p + pdw[0]);
#else
      PWSTR psz = (PWSTR)m_lStrings.Get(i);
#endif

      dwHash = Hash(psz);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   }

   return TRUE;
}


/*****************************************************************************************
CHashString::FindIndex - Searches through the hash table for the given string.

inputs
   PWSTR          pszString - String to look for
   BOOL           fToLower - If TRUE then the string needs to be lower cased
returns
   DWORD - Index, or -1 if error
*/
DWORD CHashString::FindIndex (PWSTR pszString, BOOL fToLower)
{
   // lowercase?
   CMem memLower;
   WCHAR szTemp[128];
   if (fToLower) {
      DWORD dwNeed = ((int)wcslen(pszString)+1)*sizeof(WCHAR);
      if (dwNeed <= sizeof(szTemp)) {
         memcpy (szTemp, pszString, dwNeed);
         pszString = szTemp;
      }
      else {
         if (!memLower.Required (dwNeed))
            return -1;
         memcpy (memLower.p, pszString, dwNeed);
         pszString = (PWSTR)memLower.p;
      }

      // lower case
      MIFLToLower (pszString);
   }

   // create a hash for this
   DWORD dwHash = Hash (pszString);
   DWORD *padwTable = (DWORD*)m_memTable.p;

   // BUGFIX - If empty table then nothing
   if (!m_dwTableSize)
      return -1;

   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
#ifdef OLDSTRINGHASH
      DWORD *pdw = (DWORD*)m_lElem.Get(padwTable[dwHash]-1);
      PWSTR psz = (PWSTR)((PBYTE)m_memString.p + pdw[0]);
#else
      PWSTR psz = (PWSTR)m_lStrings.Get(padwTable[dwHash]-1);
#endif
      if (!wcscmp(psz, pszString))
         return padwTable[dwHash]-1;  // found it

      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }

   // else, didnt find
   return -1;
}



/*****************************************************************************************
CHashString::Find - Searches through the hash table for the given string.

inputs
   PWSTR          pszString - String to look for
   BOOL           fToLower - If TRUE then the string needs to be lower cased
returns
   PVOID - Data element
*/
PVOID CHashString::Find (PWSTR pszString, BOOL fToLower)
{
   return Get(FindIndex(pszString, fToLower));
}


/*****************************************************************************************
CHashString::TableSize - Returns the amount allocated for the table.
*/
DWORD CHashString::TableSize (void)
{
   return m_dwTableSize;
}



/*****************************************************************************************
CHashString::TableResize - Resizes the table to the new size. If the resize is too
small or not enough memory then an error occurs.

inputs
   DWORD          dwTableSize - New table size (in elements)
returns
   BOOL - TRUE if success
*/
BOOL CHashString::TableResize (DWORD dwTableSize)
{
   if (dwTableSize < m_lElem.Num())
      return FALSE;

   DWORD dwNeed = dwTableSize * sizeof(DWORD);
   if (!m_memTable.Required (dwNeed))
      return FALSE;
   m_dwTableSize = dwTableSize;
   
   // clear
   memset (m_memTable.p, 0, dwNeed);

   // re-add
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i, dwHash;
   for (i = 0; i < m_lElem.Num(); i++) {
#ifdef OLDSTRINGHASH
      DWORD *pdw = (DWORD*)m_lElem.Get(i);
      PWSTR psz = (PWSTR)((PBYTE)m_memString.p + pdw[0]);
#else
      PWSTR psz = (PWSTR)m_lStrings.Get(i);
#endif

      dwHash = Hash(psz);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   } // i

   return TRUE;
}




/*****************************************************************************************
CHashString::Hash - Creates a hash from a string. The hash is case sensative.

inputs
   PWSTR       pszString - string
returns
   DWORD - Hash
*/
DWORD CHashString::Hash (PWSTR pszString)
{
   DWORD dwHash = 0;
   BYTE *pabHash = (BYTE*)&dwHash;

   DWORD dwSum = 0;
   DWORD dwLen = (DWORD) wcslen(pszString);
   DWORD i;
   for (i = 0; i < dwLen; i++) {
      pabHash[i%sizeof(dwHash)] ^= (BYTE)pszString[i];
      dwSum += pszString[i];
   }
   dwHash ^= (dwSum << 16);
   dwHash += (dwLen << 8);

   return dwHash;

#if 0 // old hash code

   WCHAR cSum, cXor;
   cSum = cXor = 0;
   DWORD dwCount = 0;

   for (; pszString[0]; pszString++) {
      cSum += pszString[0];
      cXor ^= pszString[0];
      dwCount++;
   }

   // combine together
   dwCount = (dwCount << 24) + (DWORD)cSum + ((DWORD)cXor << 16);

   return dwCount;
#endif
}














/*****************************************************************************************
CHashGUID::Constructor and destructor
*/
CHashGUID::CHashGUID (void)
{
   m_dwElemSize = 0;
   Clear();
}

CHashGUID::~CHashGUID (void)
{
   // do nothing for now
}

/*****************************************************************************************
CHashGUID::Init - Initializes the hash table.

inputs
   DWORD       dwElemSize - Number of bytes used for each element
   DWORD       dwTableSize - Optionally, initial size for the lookup table
*/
void CHashGUID::Init (DWORD dwElemSize, DWORD dwTableSize)
{
   m_dwElemSize = dwElemSize;
   m_lElem.Init (m_dwElemSize + sizeof(GUID));
   Clear();

   if (dwTableSize)
      TableResize (dwTableSize);
}


/*****************************************************************************************
CHashGUID::CloneTo - Clones the hash to a destination

inputs
   PCHashGUID         pTo - Clone to
returns
   BOOL - TRUE if success
*/
BOOL CHashGUID::CloneTo (CHashGUID *pTo)
{
   pTo->m_dwElemSize = m_dwElemSize;
   pTo->m_dwTableSize = m_dwTableSize;
   pTo->m_lElem.Init (m_dwElemSize + sizeof(GUID), m_lElem.Get(0), m_lElem.Num());

   if (!pTo->m_memTable.Required (m_dwTableSize * sizeof(DWORD)))
      return FALSE;
   memcpy (pTo->m_memTable.p, m_memTable.p, m_dwTableSize * sizeof(DWORD));

   return TRUE;
}


/*****************************************************************************************
CHashGUID::Clone - Clones the hash and returns a new one
*/
CHashGUID *CHashGUID::Clone (void)
{
   PCHashGUID pNew = new CHashGUID;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;

}


/*****************************************************************************************
CHashGUID::Clear - Clears out the hash table
*/
void CHashGUID::Clear (void)
{
   // NOTE: Not changing elem size
   m_dwTableSize = 0;
   m_lElem.Clear();
}


/*****************************************************************************************
CHashGUID::Num - Returns the number of elements stored in the hash table
*/
DWORD CHashGUID::Num (void)
{
   return m_lElem.Num();
}


/*****************************************************************************************
CHashGUID::Add - Adds a new element to the hash table.

inputs
   GUID           *pg - GUID to identify the element
   PVOID          pData - Data being added.
returns
   BOOL - TRUE if success
*/
BOOL CHashGUID::Add (GUID *pg, PVOID pData)
{
   BYTE abScratch[256];
   DWORD dwNeed = sizeof(GUID) + m_dwElemSize;
   CMem memScratch;
   GUID *pdw;
   if (dwNeed <= sizeof(abScratch)) {
      pdw = (GUID*) &abScratch[0];
   }
   else {
      if (!memScratch.Required (dwNeed))
         return FALSE;
      pdw = (GUID*)memScratch.p;
   }
   pdw[0] = *pg;
   memcpy (pdw+1, pData, m_dwElemSize);

   // add element
   m_lElem.Add (pdw);

   // if has table is too small then enlarge
   if (m_lElem.Num() *2 >= m_dwTableSize)
      return TableResize (m_lElem.Num()*4+1);   // BUGFIX - +1 so not completely even
         // make a fairly sparse table to low chance of collision
         //Calling table resize will automatically add the hash element in the right place

   // else, need to add
   DWORD dwHash = Hash(pg);
   DWORD *padwTable = (DWORD*)m_memTable.p;
   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }
   padwTable[dwHash] = m_lElem.Num();  // since storing index+1

   return TRUE;
}


/*****************************************************************************************
CHashGUID::Get - Returns the data used by the element at the given index

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   PVOID - Data
*/
PVOID CHashGUID::Get (DWORD dwIndex)
{
   GUID *pdw = (GUID*)m_lElem.Get(dwIndex);
   if (!pdw)
      return NULL;
   return (pdw+1);
}


/*****************************************************************************************
CHashGUID::GetGUID - Returns a pointer to the GUID for the element. DO NOT CHANGE THIS.

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   GUID * - GUID for the object. Do NOT change this.
*/
GUID *CHashGUID::GetGUID (DWORD dwIndex)
{
   GUID *pdw = (GUID*)m_lElem.Get(dwIndex);
   if (!pdw)
      return NULL;
   return pdw;
}

/*****************************************************************************************
CHashGUID::Remove - Removed an item based on its index... note: This will force the
table to be rebuilt...

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   BOOL - TRUE if success
*/
BOOL CHashGUID::Remove (DWORD dwIndex)
{
   if (dwIndex >= m_lElem.Num())
      return FALSE;

   m_lElem.Remove (dwIndex);

   // look through the table and reduce indecies that are higher by one,
   // and note where this index occurred
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i;
   DWORD dwLookFor = dwIndex+1;
   DWORD dwOldLoc = (DWORD)-1;
   for (i = 0; i < m_dwTableSize; i++, padwTable++) {
      if (*padwTable < dwLookFor)
         continue;   // either 0, or less than
      else if (*padwTable > dwLookFor)
         *padwTable = *padwTable - 1;  // since deleted it
      else {
         dwOldLoc = i;
         *padwTable = 0;
      }
   } // i

   // find all the elements occurring sequentially after dwOldLoc and remap
   CListFixed lRemap;
   DWORD j;
   lRemap.Init (sizeof(DWORD));
   padwTable = (DWORD*)m_memTable.p;
   lRemap.Required (m_dwTableSize);
   for (i = 1; i < m_dwTableSize; i++) {
      j = (dwOldLoc+i)%m_dwTableSize;
      if (padwTable[j]) {
         lRemap.Add (&padwTable[j]);
         padwTable[j] = 0;
      }
      else
         break;   // got to blank so broke the chain
   }

   // readd these
   DWORD *pdwRemap = (DWORD*) lRemap.Get(0);
   DWORD dwHash;
   for (j = 0; j < lRemap.Num(); j++) {
      i = pdwRemap[j] - 1;

      GUID *pdw = (GUID*)m_lElem.Get(i);

      dwHash = Hash(pdw);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   }

   return TRUE;
}


/*****************************************************************************************
CHashGUID::FindIndex - Searches through the hash table for the given GUID.

inputs
   GUID           *pg - GUID to look for
returns
   DWORD - Index, or -1 if error
*/
DWORD CHashGUID::FindIndex (GUID *pg)
{
   // create a hash for this
   DWORD dwHash = Hash (pg);
   DWORD *padwTable = (DWORD*)m_memTable.p;

   // BUGFIX - If empty table then nothing
   if (!m_dwTableSize)
      return -1;

   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
      GUID *pdw = (GUID*)m_lElem.Get(padwTable[dwHash]-1);
      if (IsEqualGUID(*pg, pdw[0]))
         return padwTable[dwHash]-1;  // found it

      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }

   // else, didnt find
   return -1;
}



/*****************************************************************************************
CHashGUID::Find - Searches through the hash table for the given GUID.

inputs
   GUID           *pg - GUID to look for
returns
   PVOID - Data element
*/
PVOID CHashGUID::Find (GUID *pg)
{
   return Get(FindIndex(pg));
}


/*****************************************************************************************
CHashGUID::TableSize - Returns the amount allocated for the table.
*/
DWORD CHashGUID::TableSize (void)
{
   return m_dwTableSize;
}



/*****************************************************************************************
CHashGUID::TableResize - Resizes the table to the new size. If the resize is too
small or not enough memory then an error occurs.

inputs
   DWORD          dwTableSize - New table size (in elements)
returns
   BOOL - TRUE if success
*/
BOOL CHashGUID::TableResize (DWORD dwTableSize)
{
   if (dwTableSize < m_lElem.Num())
      return FALSE;

   DWORD dwNeed = dwTableSize * sizeof(DWORD);
   if (!m_memTable.Required (dwNeed))
      return FALSE;
   m_dwTableSize = dwTableSize;
   
   // clear
   memset (m_memTable.p, 0, dwNeed);

   // re-add
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i, dwHash;
   for (i = 0; i < m_lElem.Num(); i++) {
      GUID *pdw = (GUID*)m_lElem.Get(i);

      dwHash = Hash(pdw);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   } // i

   return TRUE;
}




/*****************************************************************************************
CHashGUID::Hash - Creates a hash from a GUID. The hash is case sensative.

inputs
   GUID           *pg - GUID to look for
returns
   DWORD - Hash
*/
DWORD CHashGUID::Hash (GUID *pg)
{
   DWORD dwXor = 0;
   DWORD *pdw = (DWORD*)pg;
   DWORD i;
   for (i = 0; i < sizeof(*pg)/sizeof(DWORD); i++)
      dwXor ^= pdw[i];

   return dwXor;
}












/*****************************************************************************************
CHashDWORD::Constructor and destructor
*/
CHashDWORD::CHashDWORD (void)
{
   m_dwElemSize = 0;
   Clear();
}

CHashDWORD::~CHashDWORD (void)
{
   // do nothing for now
}

/*****************************************************************************************
CHashDWORD::Init - Initializes the hash table.

inputs
   DWORD       dwElemSize - Number of bytes used for each element
   DWORD       dwTableSize - Optionally, initial size for the lookup table
*/
void CHashDWORD::Init (DWORD dwElemSize, DWORD dwTableSize)
{
   m_dwElemSize = dwElemSize;
   m_lElem.Init (m_dwElemSize + sizeof(DWORD));
   Clear();

   if (dwTableSize)
      TableResize (dwTableSize);
}


/*****************************************************************************************
CHashDWORD::CloneTo - Clones the hash to a destination

inputs
   PCHashDWORD         pTo - Clone to
returns
   BOOL - TRUE if success
*/
BOOL CHashDWORD::CloneTo (CHashDWORD *pTo)
{
   pTo->m_dwElemSize = m_dwElemSize;
   pTo->m_dwTableSize = m_dwTableSize;
   pTo->m_lElem.Init (m_dwElemSize + sizeof(DWORD), m_lElem.Get(0), m_lElem.Num());

   if (!pTo->m_memTable.Required (m_dwTableSize * sizeof(DWORD)))
      return FALSE;
   memcpy (pTo->m_memTable.p, m_memTable.p, m_dwTableSize * sizeof(DWORD));

   return TRUE;
}


/*****************************************************************************************
CHashDWORD::Clone - Clones the hash and returns a new one
*/
CHashDWORD *CHashDWORD::Clone (void)
{
   PCHashDWORD pNew = new CHashDWORD;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;

}


/*****************************************************************************************
CHashDWORD::Clear - Clears out the hash table
*/
void CHashDWORD::Clear (void)
{
   // NOTE: Not changing elem size
   m_dwTableSize = 0;
   m_lElem.Clear();
}


/*****************************************************************************************
CHashDWORD::Num - Returns the number of elements stored in the hash table
*/
DWORD CHashDWORD::Num (void)
{
   return m_lElem.Num();
}


/*****************************************************************************************
CHashDWORD::Add - Adds a new element to the hash table.

inputs
   DWORD          dwID - DWORD to identify the element
   PVOID          pData - Data being added.
returns
   BOOL - TRUE if success
*/
BOOL CHashDWORD::Add (DWORD dwID, PVOID pData)
{
   BYTE abScratch[256];
   DWORD dwNeed = sizeof(DWORD) + m_dwElemSize;
   CMem memScratch;
   DWORD *pdw;
   if (dwNeed <= sizeof(abScratch)) {
      pdw = (DWORD*) &abScratch[0];
   }
   else {
      if (!memScratch.Required (dwNeed))
         return FALSE;
      pdw = (DWORD*)memScratch.p;
   }
   pdw[0] = dwID;
   memcpy (pdw+1, pData, m_dwElemSize);

   // add element
   m_lElem.Add (pdw);

   // if has table is too small then enlarge
   if (m_lElem.Num() *2 >= m_dwTableSize)
      return TableResize (m_lElem.Num()*4+1);   // BUGFIX - +1 so not completely even
         // make a fairly sparse table to low chance of collision
         //Calling table resize will automatically add the hash element in the right place

   // else, need to add
   DWORD dwHash = Hash(dwID);
   DWORD *padwTable = (DWORD*)m_memTable.p;
   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }
   padwTable[dwHash] = m_lElem.Num();  // since storing index+1

   return TRUE;
}


/*****************************************************************************************
CHashDWORD::Get - Returns the data used by the element at the given index

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   PVOID - Data
*/
PVOID CHashDWORD::Get (DWORD dwIndex)
{
   DWORD *pdw = (DWORD*)m_lElem.Get(dwIndex);
   if (!pdw)
      return NULL;
   return (pdw+1);
}


/*****************************************************************************************
CHashDWORD::Remove - Removed an item based on its index... note: This will force the
table to be rebuilt...

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   BOOL - TRUE if success
*/
BOOL CHashDWORD::Remove (DWORD dwIndex)
{
   if (dwIndex >= m_lElem.Num())
      return FALSE;

   m_lElem.Remove (dwIndex);

   // NOTE: Not tested, but since cut and paste of other hash remove, should
   // be ok

   // look through the table and reduce indecies that are higher by one,
   // and note where this index occurred
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i;
   DWORD dwLookFor = dwIndex+1;
   DWORD dwOldLoc = (DWORD)-1;
   for (i = 0; i < m_dwTableSize; i++, padwTable++) {
      if (*padwTable < dwLookFor)
         continue;   // either 0, or less than
      else if (*padwTable > dwLookFor)
         *padwTable = *padwTable - 1;  // since deleted it
      else {
         dwOldLoc = i;
         *padwTable = 0;
      }
   } // i

   // find all the elements occurring sequentially after dwOldLoc and remap
   CListFixed lRemap;
   DWORD j;
   lRemap.Init (sizeof(DWORD));
   padwTable = (DWORD*)m_memTable.p;
   lRemap.Required (m_dwTableSize);
   for (i = 1; i < m_dwTableSize; i++) {
      j = (dwOldLoc+i)%m_dwTableSize;
      if (padwTable[j]) {
         lRemap.Add (&padwTable[j]);
         padwTable[j] = 0;
      }
      else
         break;   // got to blank so broke the chain
   }

   // readd these
   DWORD *pdwRemap = (DWORD*) lRemap.Get(0);
   DWORD dwHash;
   for (j = 0; j < lRemap.Num(); j++) {
      i = pdwRemap[j] - 1;

      DWORD *pdw = (DWORD*)m_lElem.Get(i);

      dwHash = Hash(pdw[0]);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   }

   return TRUE;
}


/*****************************************************************************************
CHashDWORD::FindIndex - Searches through the hash table for the given DWORD.

inputs
   DWORD          dwID - DWORD to look for
returns
   DWORD - Index, or -1 if error
*/
DWORD CHashDWORD::FindIndex (DWORD dwID)
{
   // create a hash for this
   DWORD dwHash = Hash (dwID);
   DWORD *padwTable = (DWORD*)m_memTable.p;

   // BUGFIX - If empty table then nothing
   if (!m_dwTableSize)
      return -1;

   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
      DWORD *pdw = (DWORD*)m_lElem.Get(padwTable[dwHash]-1);
      if (dwID == pdw[0])
         return padwTable[dwHash]-1;  // found it

      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }

   // else, didnt find
   return -1;
}



/*****************************************************************************************
CHashDWORD::Find - Searches through the hash table for the given DWORD.

inputs
   DWORD          dwID - DWORD to look for
returns
   PVOID - Data element
*/
PVOID CHashDWORD::Find (DWORD dwID)
{
   return Get(FindIndex(dwID));
}


/*****************************************************************************************
CHashDWORD::TableSize - Returns the amount allocated for the table.
*/
DWORD CHashDWORD::TableSize (void)
{
   return m_dwTableSize;
}



/*****************************************************************************************
CHashDWORD::TableResize - Resizes the table to the new size. If the resize is too
small or not enough memory then an error occurs.

inputs
   DWORD          dwTableSize - New table size (in elements)
returns
   BOOL - TRUE if success
*/
BOOL CHashDWORD::TableResize (DWORD dwTableSize)
{
   if (dwTableSize < m_lElem.Num())
      return FALSE;

   DWORD dwNeed = dwTableSize * sizeof(DWORD);
   if (!m_memTable.Required (dwNeed))
      return FALSE;
   m_dwTableSize = dwTableSize;
   
   // clear
   memset (m_memTable.p, 0, dwNeed);

   // re-add
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i, dwHash;
   for (i = 0; i < m_lElem.Num(); i++) {
      DWORD *pdw = (DWORD*)m_lElem.Get(i);

      dwHash = Hash(pdw[0]);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   } // i

   return TRUE;
}




/*****************************************************************************************
CHashDWORD::Hash - Creates a hash from a DWORD. The hash is case sensative.

inputs
   DWORD          dwID - DWORD to look for
returns
   DWORD - Hash
*/
DWORD CHashDWORD::Hash (DWORD dwID)
{
   DWORD dwFlip = 0;
   DWORD dwSrc = 0x01;
   DWORD dwDest = 0x80000000;
   
   for (; dwDest; dwDest >>= 1, dwSrc <<= 1)
      if (dwID & dwSrc)
         dwFlip |= dwDest;

   return dwFlip;
}






/*****************************************************************************************
CHashPVOID::Constructor and destructor
*/
CHashPVOID::CHashPVOID (void)
{
   m_dwElemSize = 0;
   Clear();
}

CHashPVOID::~CHashPVOID (void)
{
   // do nothing for now
}

/*****************************************************************************************
CHashPVOID::Init - Initializes the hash table.

inputs
   DWORD       dwElemSize - Number of bytes used for each element
   DWORD       dwTableSize - Optionally, initial size for the lookup table
*/
void CHashPVOID::Init (DWORD dwElemSize, DWORD dwTableSize)
{
   m_dwElemSize = dwElemSize;
   m_lElem.Init (m_dwElemSize + sizeof(PVOID));
   Clear();

   if (dwTableSize)
      TableResize (dwTableSize);
}


/*****************************************************************************************
CHashPVOID::CloneTo - Clones the hash to a destination

inputs
   PCHashPVOID         pTo - Clone to
returns
   BOOL - TRUE if success
*/
BOOL CHashPVOID::CloneTo (CHashPVOID *pTo)
{
   pTo->m_dwElemSize = m_dwElemSize;
   pTo->m_dwTableSize = m_dwTableSize;
   pTo->m_lElem.Init (m_dwElemSize + sizeof(PVOID), m_lElem.Get(0), m_lElem.Num());

   if (!pTo->m_memTable.Required (m_dwTableSize * sizeof(DWORD)))
      return FALSE;
   memcpy (pTo->m_memTable.p, m_memTable.p, m_dwTableSize * sizeof(DWORD));

   return TRUE;
}


/*****************************************************************************************
CHashPVOID::Clone - Clones the hash and returns a new one
*/
CHashPVOID *CHashPVOID::Clone (void)
{
   PCHashPVOID pNew = new CHashPVOID;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;

}


/*****************************************************************************************
CHashPVOID::Clear - Clears out the hash table
*/
void CHashPVOID::Clear (void)
{
   // NOTE: Not changing elem size
   m_dwTableSize = 0;
   m_lElem.Clear();
}


/*****************************************************************************************
CHashPVOID::Num - Returns the number of elements stored in the hash table
*/
DWORD CHashPVOID::Num (void)
{
   return m_lElem.Num();
}


/*****************************************************************************************
CHashPVOID::Add - Adds a new element to the hash table.

inputs
   PVOID          dwID - PVOID to identify the element
   PVOID          pData - Data being added.
returns
   BOOL - TRUE if success
*/
BOOL CHashPVOID::Add (PVOID dwID, PVOID pData)
{
   BYTE abScratch[256];
   DWORD dwNeed = sizeof(PVOID) + m_dwElemSize;
   CMem memScratch;
   PVOID *pdw;
   if (dwNeed <= sizeof(abScratch)) {
      pdw = (PVOID*) &abScratch[0];
   }
   else {
      if (!memScratch.Required (dwNeed))
         return FALSE;
      pdw = (PVOID*)memScratch.p;
   }
   pdw[0] = dwID;
   memcpy (pdw+1, pData, m_dwElemSize);

   // add element
   m_lElem.Add (pdw);

   // if has table is too small then enlarge
   if (m_lElem.Num() *2 >= m_dwTableSize)
      return TableResize (m_lElem.Num()*4+1);   // BUGFIX - +1 so not completely even
         // make a fairly sparse table to low chance of collision
         //Calling table resize will automatically add the hash element in the right place

   // else, need to add
   DWORD dwHash = Hash(dwID);
   DWORD *padwTable = (DWORD*)m_memTable.p;
   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }
   padwTable[dwHash] = m_lElem.Num();  // since storing index+1

   return TRUE;
}


/*****************************************************************************************
CHashPVOID::Get - Returns the data used by the element at the given index

inputs
   PVOID          dwIndex - Index, from 0..Num()-1
retursn     
   PVOID - Data
*/
PVOID CHashPVOID::Get (DWORD dwIndex)
{
   PVOID *pdw = (PVOID*)m_lElem.Get(dwIndex);
   if (!pdw)
      return NULL;
   return (pdw+1);
}



/*****************************************************************************************
CHashPVOID::GetPVOID - Returns the PVOID used to index the data

inputs
   PVOID          dwIndex - Index, from 0..Num()-1
retursn     
   PVOID - Data
*/
PVOID CHashPVOID::GetPVOID (DWORD dwIndex)
{
   PVOID *pdw = (PVOID*)m_lElem.Get(dwIndex);
   if (!pdw)
      return NULL;
   return pdw[0];
}


/*****************************************************************************************
CHashPVOID::Remove - Removed an item based on its index... note: This will force the
table to be rebuilt...

inputs
   DWORD          dwIndex - Index, from 0..Num()-1
retursn     
   BOOL - TRUE if success
*/
BOOL CHashPVOID::Remove (DWORD dwIndex)
{
   if (dwIndex >= m_lElem.Num())
      return FALSE;

   m_lElem.Remove (dwIndex);

   // NOTE: Not tested, but since cut and paste of other hash remove, should
   // be ok

   // look through the table and reduce indecies that are higher by one,
   // and note where this index occurred
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i;
   DWORD dwLookFor = dwIndex+1;
   DWORD dwOldLoc = (DWORD)-1;
   for (i = 0; i < m_dwTableSize; i++, padwTable++) {
      if (*padwTable < dwLookFor)
         continue;   // either 0, or less than
      else if (*padwTable > dwLookFor)
         *padwTable = *padwTable - 1;  // since deleted it
      else {
         dwOldLoc = i;
         *padwTable = 0;
      }
   } // i

   // find all the elements occurring sequentially after dwOldLoc and remap
   CListFixed lRemap;
   DWORD j;
   lRemap.Init (sizeof(DWORD));
   padwTable = (DWORD*)m_memTable.p;
   lRemap.Required (m_dwTableSize);
   for (i = 1; i < m_dwTableSize; i++) {
      j = (dwOldLoc+i)%m_dwTableSize;
      if (padwTable[j]) {
         lRemap.Add (&padwTable[j]);
         padwTable[j] = 0;
      }
      else
         break;   // got to blank so broke the chain
   }

   // readd these
   DWORD *pdwRemap = (DWORD*) lRemap.Get(0);
   DWORD dwHash;
   for (j = 0; j < lRemap.Num(); j++) {
      i = pdwRemap[j] - 1;

      PVOID *pdw = (PVOID*)m_lElem.Get(i);

      dwHash = Hash(pdw[0]);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   }

   return TRUE;
}


/*****************************************************************************************
CHashPVOID::FindIndex - Searches through the hash table for the given PVOID.

inputs
   PVOID          dwID - PVOID to look for
returns
   DWORD - Index, or -1 if error
*/
DWORD CHashPVOID::FindIndex (PVOID dwID)
{
   // create a hash for this
   DWORD dwHash = Hash (dwID);
   DWORD *padwTable = (DWORD*)m_memTable.p;

   // BUGFIX - If empty table then nothing
   if (!m_dwTableSize)
      return -1;

   dwHash = dwHash % m_dwTableSize;
   while (padwTable[dwHash]) {
      PVOID *pdw = (PVOID*)m_lElem.Get(padwTable[dwHash]-1);
      if (dwID == pdw[0])
         return padwTable[dwHash]-1;  // found it

      dwHash++;
      if (dwHash >= m_dwTableSize)
         dwHash = 0;
   }

   // else, didnt find
   return -1;
}



/*****************************************************************************************
CHashPVOID::Find - Searches through the hash table for the given PVOID.

inputs
   PVOID          dwID - PVOID to look for
returns
   PVOID - Data element
*/
PVOID CHashPVOID::Find (PVOID dwID)
{
   return Get(FindIndex(dwID));
}


/*****************************************************************************************
CHashPVOID::TableSize - Returns the amount allocated for the table.
*/
DWORD CHashPVOID::TableSize (void)
{
   return m_dwTableSize;
}



/*****************************************************************************************
CHashPVOID::TableResize - Resizes the table to the new size. If the resize is too
small or not enough memory then an error occurs.

inputs
   DWORD          dwTableSize - New table size (in elements)
returns
   BOOL - TRUE if success
*/
BOOL CHashPVOID::TableResize (DWORD dwTableSize)
{
   if (dwTableSize < m_lElem.Num())
      return FALSE;

   DWORD dwNeed = dwTableSize * sizeof(DWORD);
   if (!m_memTable.Required (dwNeed))
      return FALSE;
   m_dwTableSize = dwTableSize;
   
   // clear
   memset (m_memTable.p, 0, dwNeed);

   // re-add
   DWORD *padwTable = (DWORD*)m_memTable.p;
   DWORD i, dwHash;
   for (i = 0; i < m_lElem.Num(); i++) {
      PVOID *pdw = (PVOID*)m_lElem.Get(i);

      dwHash = Hash(pdw[0]);
      dwHash = dwHash % m_dwTableSize;
      while (padwTable[dwHash]) {
         dwHash++;
         if (dwHash >= m_dwTableSize)
            dwHash = 0;
      }
      padwTable[dwHash] = i+1;  // since storing index+1
   } // i

   return TRUE;
}




/*****************************************************************************************
CHashPVOID::Hash - Creates a hash from a PVOID. The hash is case sensative.

inputs
   PVOID          dwID - PVOID to look for
returns
   DWORD - Hash
*/
DWORD CHashPVOID::Hash (PVOID dwID)
{
   DWORD dwFlip = 0;
   size_t dwSrc = 0x01;
   DWORD dwDest = 0x80000000;
   
   // NOTE: Only hashing lower 32 bits, but should be OK
   // for most cases
   for (; dwDest; dwDest >>= 1, dwSrc <<= 1)
      if ((size_t)dwID & dwSrc)
         dwFlip |= dwDest;

   return dwFlip;
}


