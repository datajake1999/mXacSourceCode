/*******************************************************************
CMMLNode.cpp - Parses mML files for the data tree.
   mML files are very similar to XML/MHTML.

begun3/19/200 by Mike ROzak
Separated 29/7/04
Copyright 2000 by Mike Rozak. All rights reserved
*/


#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <crtdbg.h>
#include "tools.h"
#include "mmlparse.h"
#include "escarpment.h"
#include "resleak.h"


/********************************************************************
CMMLNode2 - Class that describes the contents of a MML node after its
been extracted. When you delete the CMMLNode object, it also deletes
all its children.
*/



// MMNCONTENT - Storing list of content for the node
typedef struct {
   DWORD          dwType;           // data type, of type MMNC_XXX
   size_t          dwDataOffset;     // if MMNC_STRING then this is the data offset for the
                                    // string. if MMNC_NODE then both dwDataOffset and dwDataAlloc
                                    // combine to form the pointer to the other node
   size_t          dwDataSize;       // size of data allocated for the string, or the
                                    // other half of the MMNC_NODE pointer
} MMNCONTENT, *PMMNCONTENT;

#define MMNC_STRING           0     // data is a WCHAR string
#define MMNC_NODE             1     // data is a a subnode


/********************************************************************
CMMLNode2 - Constructor & destructor
*/
CMMLNode2::CMMLNode2 (void)
{
   m_dwType = MMLCLASS_ELEMENT;
   //m_fDirty = FALSE;
   //m_pParent = NULL;
   m_listMMNATTRIB.Init (sizeof(MMNATTRIB));
   m_listMMNCONTENT.Init (sizeof(MMNCONTENT));
   m_pszName = NULL;
}

CMMLNode2::~CMMLNode2 (void)
{
   // free the name
   if (m_pszName)
      EscAtomicStringFree (m_pszName);

   // free the attributes
   DWORD i;
   PMMNATTRIB pa = (PMMNATTRIB)m_listMMNATTRIB.Get(0);
   for (i = 0; i < m_listMMNATTRIB.Num(); i++, pa++)
      EscAtomicStringFree (pa->pszName);

   // if the content has any nodes then delete them
   PMMNCONTENT pc = (PMMNCONTENT)m_listMMNCONTENT.Get(0);
   for (i = 0; i < m_listMMNCONTENT.Num(); i++, pc++) {
      // if it's another node and delete is true, then delete
      if (pc->dwType == MMNC_NODE) {
         PCMMLNode2 *ppn = (PCMMLNode2*)(&pc->dwDataOffset);
         delete *ppn;
      }
   }

}


/********************************************************************
CMMLNode2::Clone - Clones the current node and all its children.

returns
   CMMLNode2* - Cloned node. NULL if error
*/
CMMLNode2* CMMLNode2::Clone (void)
{
   PCMMLNode2   pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   // clone name
   pNode->m_dwType = m_dwType;
   //pNode->m_fDirty = m_fDirty;
   //pNode->m_pParent = NULL;
   if (pNode->m_pszName)
      EscAtomicStringFree (pNode->m_pszName);
   pNode->m_pszName = EscAtomicStringAdd (m_pszName);

   // free the existing attribute names
   PMMNATTRIB pa = (PMMNATTRIB)pNode->m_listMMNATTRIB.Get(0);
   DWORD i;
   for (i = 0; i < pNode->m_listMMNATTRIB.Num(); i++, pa++)
      EscAtomicStringFree (pa->pszName);

   if (!pNode->m_memHeap.Required (m_memHeap.m_dwCurPosn)) {
      delete pNode;
      return NULL;
   }
   pNode->m_memHeap.m_dwCurPosn = m_memHeap.m_dwCurPosn;
   memcpy (pNode->m_memHeap.p, m_memHeap.p, m_memHeap.m_dwCurPosn);

   pNode->m_listMMNATTRIB.Init (sizeof(MMNATTRIB), m_listMMNATTRIB.Get(0), m_listMMNATTRIB.Num());
   pNode->m_listMMNCONTENT.Init (sizeof(MMNCONTENT), m_listMMNCONTENT.Get(0), m_listMMNCONTENT.Num());

   // clone sub-nodes
   PMMNCONTENT pc = (PMMNCONTENT)pNode->m_listMMNCONTENT.Get(0);
   for (i = 0; i < pNode->m_listMMNCONTENT.Num(); i++, pc++) {
      // if it's another node and delete is true, then delete
      if (pc->dwType == MMNC_NODE) {
         PCMMLNode2 *ppn = (PCMMLNode2*)(&pc->dwDataOffset);
         *ppn = (*ppn)->Clone();
         //if (*ppn)
         //   (*ppn)->m_pParent = pNode;
      }
   }

   // clone the attribute names
   pa = (PMMNATTRIB)pNode->m_listMMNATTRIB.Get(0);
   for (i = 0; i < pNode->m_listMMNATTRIB.Num(); i++, pa++)
      pa->pszName = EscAtomicStringAdd (pa->pszName); // which will cause an addref


   // done
   return pNode;
}



/********************************************************************
CMMLNode2::CloneHeader - Clones the current node but DOESN'T clone any of
the children.

returns
   CMMLNode2* - Cloned node. NULL if error
*/
CMMLNode2* CMMLNode2::CloneHeader (void)
{
   PCMMLNode2   pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   // clone name
   pNode->m_dwType = m_dwType;
   //pNode->m_fDirty = m_fDirty;
   //pNode->m_pParent = NULL;
   if (pNode->m_pszName)
      EscAtomicStringFree (pNode->m_pszName);
   pNode->m_pszName = EscAtomicStringAdd (m_pszName);

   // free the existing attribute names
   PMMNATTRIB pa = (PMMNATTRIB)pNode->m_listMMNATTRIB.Get(0);
   DWORD i;
   for (i = 0; i < pNode->m_listMMNATTRIB.Num(); i++, pa++)
      EscAtomicStringFree (pa->pszName);

   if (!pNode->m_memHeap.Required (m_memHeap.m_dwCurPosn)) {
      delete pNode;
      return NULL;
   }
   pNode->m_memHeap.m_dwCurPosn = m_memHeap.m_dwCurPosn;
   memcpy (pNode->m_memHeap.p, m_memHeap.p, m_memHeap.m_dwCurPosn);

   pNode->m_listMMNATTRIB.Init (sizeof(MMNATTRIB), m_listMMNATTRIB.Get(0), m_listMMNATTRIB.Num());

   // dont do the following for this type of clone
#if 0
   pNode->m_listMMNCONTENT.Init (sizeof(MMNCONTENT), m_listMMNCONTENT.Get(0), m_listMMNCONTENT.Num());

   // clone sub-nodes
   PMMNCONTENT pc = (PMMNCONTENT)pNode->m_listMMNCONTENT.Get(0);
   for (i = 0; i < pNode->m_listMMNCONTENT.Num(); i++, pc++) {
      // if it's another node and delete is true, then delete
      if (pc->dwType == MMNC_NODE) {
         PCMMLNode2 *ppn = (PCMMLNode2*)(&pc->dwDataOffset);
         *ppn = (*ppn)->Clone();
         //if (*ppn)
         //   (*ppn)->m_pParent = pNode;
      }
   }
#endif // 0

   // clone the attribute names
   pa = (PMMNATTRIB)pNode->m_listMMNATTRIB.Get(0);
   for (i = 0; i < pNode->m_listMMNATTRIB.Num(); i++, pa++)
      pa->pszName = EscAtomicStringAdd (pa->pszName); // which will cause an addref


   // done
   return pNode;
}


/********************************************************************
CMMLNode2::CloneAsCMMLNode - Clones the current node and all its children.

returns
   CMMLNOde* - Cloned node. NULL if error
*/
CMMLNode* CMMLNode2::CloneAsCMMLNode (void)
{
   PCMMLNode   pNode;
   pNode = new CMMLNode;
   if (!pNode)
      return NULL;

   // clone name
   pNode->NameSet(NameGet());
   pNode->m_dwType = m_dwType;

   // clone the attributes
   DWORD i;
   for (i = 0; i < AttribNum(); i++) {
      WCHAR *pszAttrib, *pszValue;
      if (!AttribEnum (i, &pszAttrib, &pszValue))
         continue;
      if (!pNode->AttribSet (pszAttrib, pszValue)) {
         delete pNode;
         return NULL;
      }
   }

   // clone the contents
   for (i = 0; i < ContentNum(); i++) {
      WCHAR *psz;
      PCMMLNode2   pSubNode;
      if (!ContentEnum (i, &psz, &pSubNode))
         continue;

      // if it's a string easy
      if (psz) {
         if (!pNode->ContentAdd (psz)) {
            delete pNode;
            return NULL;
         }
         continue;
      }

      // else node, clone
      PCMMLNode   pClone;
      pClone = pSubNode->CloneAsCMMLNode();
      if (!pClone) {
         delete pNode;
         return NULL;
      }
      if (!pNode->ContentAdd (pClone)) {
         delete pNode;
         return NULL;
      }
   }

   // done
   return pNode;
}


/********************************************************************
CMMLNode2::NodeHeapAlloc - Allocates enough space on the node for
the new data.

inputs
   DWORD       dwSize - Size requested. This will be rounded up to the nearest DWORD
returns
   DWORD - Offset (in bytes) to location in the heap, or -1 if error.
*/
size_t CMMLNode2::NodeHeapAlloc (size_t dwSize)
{
   size_t dwRound = dwSize & 0x03;
   if (dwRound)
      dwSize += 4-dwRound;

   if (m_memHeap.m_dwCurPosn + dwSize <= m_memHeap.m_dwAllocated) {
      dwRound = m_memHeap.m_dwCurPosn;  // keep this
      m_memHeap.m_dwCurPosn += dwSize;
      return dwRound;
   }

   // else, allocate more
   if (!m_memHeap.Required (m_memHeap.m_dwCurPosn + dwSize))
      return -1;
   dwRound = m_memHeap.m_dwCurPosn;
   m_memHeap.m_dwCurPosn += dwSize;
   return dwRound;
}


/********************************************************************
CMMLNode2::AttribAlloc - This makes sure there's enough space for the
attribute on the heap, and allocates as necessary.

inputs
   PMMNATTRIB        pa - Information known about the attribute. If the name
                           and data have NOT yet been filled in, the
                           dwNameOffset and dwDataOffset values should be -1.
   PWSTR             pszName - Name to use. If dwNameOffset == -1 then memory will
                           be allocatd for the name and it will be copied.
                           Otherwise, nothing will be done with pszName.
   PVOID             pData - Data to write in the attrbiute
   DWORD             dwSize - Size of the data
   DWORD             dwType - Type of the data. MMNA_XXX types
returns
   BOOL - TRUE if success
*/
BOOL CMMLNode2::AttribAlloc (PMMNATTRIB pa, PWSTR pszName, PVOID pData, size_t dwSize, DWORD dwType)
{
   // name
   if (!pa->pszName)
      pa->pszName = EscAtomicStringAdd (pszName);

   // data
   if ((pa->dwDataOffset == -1) || (pa->dwDataAlloc < dwSize)) {
      // allocate if not there, or need larger
      pa->dwDataOffset = NodeHeapAlloc (dwSize);
      if (pa->dwDataOffset == -1)
         return FALSE;
      pa->dwDataAlloc = dwSize;
   }

   // copy it
   memcpy ((PBYTE)m_memHeap.p + pa->dwDataOffset, pData, dwSize);

   // if this is binary, then always set the allocated size to what's there
   if (dwType == MMNA_BINARY)
      pa->dwDataAlloc = dwSize;

   // write data type
   pa->dwType = dwType;

   return TRUE;
}


/********************************************************************
CMMLNode2::AttribFind - Given an attribute string, this tries
to find a match.

inputs
   PWSTR          pszAttrib - Attribute looking for. Case insensative
returns
   DWORD - Index, or -1 if cant find
*/
DWORD CMMLNode2::AttribFind (PWSTR pszAttrib)
{
   DWORD i;
   PMMNATTRIB pa = (PMMNATTRIB) m_listMMNATTRIB.Get(0);
   for (i = 0; i < m_listMMNATTRIB.Num(); i++, pa++)
      if (pa->pszName && !_wcsicmp(pa->pszName, pszAttrib))
            return i;

   // else cant find
   return -1;
}



/********************************************************************
CMMLNode2::AttribSetUniversal - Universal attribute set, for any type.

inputs
   DWORD          dwIndex - Index into attribute list if known, else -1 must add..
                     Just pass in results from AttribFind()
   PWSTR          pszAttrib - Attribute
   PVOID          pData - Data
   DWORD          dwSize - Data size
   DWORD          dwType - Data type
returns
   BOOL - TRUE if set, FALSE if error
*/
BOOL CMMLNode2::AttribSetUniveral (DWORD dwIndex, PWSTR pszAttrib, PVOID pData, size_t dwSize, DWORD dwType)
{
   //DirtySet ();

   // if if already exists
   MMNATTRIB ma;
   PMMNATTRIB pa;
   if (dwIndex == -1) {
      // need to create new one
      ma.dwDataOffset = -1;
      ma.pszName = NULL;
      pa = &ma;
   }
   else
      pa = (PMMNATTRIB) m_listMMNATTRIB.Get(dwIndex);

   if (!AttribAlloc(pa, pszAttrib, pData, dwSize, dwType))
      return FALSE;  // error

   // if new index then add to end
   if (pa == &ma)
      m_listMMNATTRIB.Add (pa);
   return TRUE;
}


/********************************************************************
CMMLNode2::AttribConvert - Convert an attribute from one type
to another.

inputs
   PMMNATTRIB     pa - Attribute to modify
   DWORD          dwType - New type that want
returns
   BOOL - TRUE if success
*/
BOOL CMMLNode2::AttribConvert (PMMNATTRIB pa, DWORD dwType)
{
   if (pa->dwType == dwType)
      return TRUE; // nothing to convert

   // convert first to string
   if (pa->dwType == MMNA_BINARY) {
#ifdef _DEBUG
      //if (pa->dwDataAlloc > 10)
      //   OutputDebugString ("\r\n\r\nCMMLNode2 WARNING! Doing slow binary to string conversion.\r\n\r\n");
#endif
      CMem mem;
      size_t dwNeed = (pa->dwDataAlloc*2+1)*sizeof(WCHAR);
      if (!mem.Required (dwNeed))
         return FALSE;

      MMLBinaryToString ((PBYTE)m_memHeap.p + pa->dwDataOffset, pa->dwDataAlloc, (PWSTR)mem.p);
      if (!AttribAlloc (pa, NULL, mem.p, dwNeed, MMNA_STRING))
         return FALSE;
   }
   else if (pa->dwType == MMNA_INT) {
      // if converting to double then do conversion right here...
      if (dwType == MMNA_DOUBLE) {
         double fVal = *((int*)((PBYTE)m_memHeap.p + pa->dwDataOffset));
         return AttribAlloc (pa, NULL, &fVal, sizeof(fVal), dwType);
      }
#ifdef _DEBUG
      /// OutputDebugString ("\r\n\r\nCMMLNode2 WARNING! Doing slow integer to string conversion.\r\n\r\n");
#endif
      WCHAR szTemp[32];
      int iValue = *((int*)((PBYTE)m_memHeap.p + pa->dwDataOffset));
      swprintf (szTemp, L"%d", iValue);
      if (!AttribAlloc (pa, NULL, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR), MMNA_STRING))
         return FALSE;
   }
   else if (pa->dwType == MMNA_DOUBLE) {
#ifdef _DEBUG
      // OutputDebugString ("\r\n\r\nCMMLNode2 WARNING! Doing slow double to string conversion.\r\n\r\n");
#endif
      WCHAR szTemp[32];
      double fValue = *((double*)((PBYTE)m_memHeap.p + pa->dwDataOffset));
      MMLDoubleToString (fValue, szTemp);
      if (!AttribAlloc (pa, NULL, szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR), MMNA_STRING))
         return FALSE;
   }
   else if (pa->dwType != MMNA_STRING)
      return FALSE;

   // if type matches then good
   if (pa->dwType == dwType)
      return TRUE;

   // convert from a string to the type of data
   PWSTR psz = (PWSTR)((PBYTE)m_memHeap.p + pa->dwDataOffset);
   if (dwType == MMNA_BINARY) {
#ifdef _DEBUG
      //if (wcslen(psz) > 25)
      //   OutputDebugString ("\r\n\r\nCMMLNode2 WARNING! Doing slow string to binary conversion.\r\n\r\n");
#endif
      CMem mem;
      size_t dwNeed = wcslen(psz) / 2;
      if (!mem.Required (dwNeed))
         return FALSE;

      dwNeed = MMLBinaryFromString (psz, (PBYTE)mem.p, mem.m_dwAllocated);
      if (dwNeed && !AttribAlloc (pa, NULL, mem.p, dwNeed, dwType))
         return FALSE;
   }
   else if (dwType == MMNA_INT) {
#ifdef _DEBUG
      //OutputDebugString ("\r\n\r\nCMMLNode2 WARNING! Doing slow string to integer conversion.\r\n\r\n");
#endif
      int iVal;
      if (!AttribToDecimal (psz, &iVal))
         return iVal = 0;  // shouldnt happen

      if (!AttribAlloc (pa, NULL, &iVal, sizeof(iVal), dwType))
         return FALSE;
   }
   else if (dwType == MMNA_DOUBLE) {
#ifdef _DEBUG
      //OutputDebugString ("\r\n\r\nCMMLNode2 WARNING! Doing slow strint to double conversion.\r\n\r\n");
#endif
      double fVal = MMLDoubleFromString (psz);

      if (!AttribAlloc (pa, NULL, &fVal, sizeof(fVal), dwType))
         return FALSE;
   }
   else
      return FALSE;

   return TRUE;
}


/********************************************************************
CMMLNode2::AttribGetUniversal - Universal attribute get, for any type.

inputs
   PWSTR          pszAttrib - Attribute
   DWORD          dwType - Data type. NOTE: If this data type does NOT match
                  what's there then the data will be converted to a string.
                  If this is -1 then it WON'T convert the data type.
returns
   PMMLNATTRIB - Attribute found, converted to type. NULL if can't find
*/
PMMNATTRIB CMMLNode2::AttribGetUniversal (PWSTR pszAttrib, DWORD dwType)
{
   DWORD dwIndex = AttribFind (pszAttrib);
   if (dwIndex == -1)
      return NULL;

   PMMNATTRIB pa = (PMMNATTRIB) m_listMMNATTRIB.Get(dwIndex);

   // if don't want to convert data types, or it's the same, then return
   if ((dwType == pa->dwType) || (dwType == -1))
      return pa;

   // else need to convert
   if (!AttribConvert (pa, dwType))
      return NULL;
   return pa;
}



/********************************************************************
CMMLNode2::AttribSetString - Sets an attribute of the MML node. If the attribute
is already set the old one is overwritten.

inputs
   WCHAR    *pszAttrib - attribute
   WCHAR    *pszValue - value
returns
   BOOL - TRUE if success
*/
BOOL CMMLNode2::AttribSetString (WCHAR *pszAttrib, WCHAR *pszValue)
{
   return AttribSetUniveral (AttribFind(pszAttrib), pszAttrib, pszValue,
      (wcslen(pszValue)+1)*sizeof(WCHAR), MMNA_STRING);
}

/********************************************************************
CMMLNode2::AttribGetString - Returns a pointer to the string for the attribute.

inputs
   WCHAR    *pszAttrib - attribute
returns
   WCHAR * - Pointer to the string for the value. Do not change this.
               This pointer is valid until the attribute is changed or
               deleted. If the attribute is not found, returns NULL
*/
WCHAR* CMMLNode2::AttribGetString (WCHAR *pszAttrib)
{
   PMMNATTRIB pa = AttribGetUniversal (pszAttrib, MMNA_STRING);
   if (pa)
      return (PWSTR)((PBYTE)m_memHeap.p + pa->dwDataOffset);
   else
      return NULL;
}


/********************************************************************
CMMLNode2::AttribSetInt- Sets an attribute of the MML node. If the attribute
is already set the old one is overwritten.

inputs
   WCHAR    *pszAttrib - attribute
   int      iValue
returns
   BOOL - TRUE if success
*/
BOOL CMMLNode2::AttribSetInt (WCHAR *pszAttrib, int iValue)
{
   return AttribSetUniveral (AttribFind(pszAttrib), pszAttrib, &iValue,
      sizeof(iValue), MMNA_INT);
}

/********************************************************************
CMMLNode2::AttribGetInt- Gets an atribute

inputs
   WCHAR    *pszAttrib - attribute
   int      *piValue - Filled in if gets the value
returns
   BOOL - TRUE if success, FALSE if cant find
*/
BOOL CMMLNode2::AttribGetInt (WCHAR *pszAttrib, int *piValue)
{
   PMMNATTRIB pa = AttribGetUniversal (pszAttrib, MMNA_INT);
   if (pa) {
      *piValue = *((int*)((PBYTE)m_memHeap.p + pa->dwDataOffset));
      return TRUE;
   }
   else
      return FALSE;
}

/********************************************************************
CMMLNode2::AttribSetDouble- Sets an attribute of the MML node. If the attribute
is already set the old one is overwritten.

inputs
   WCHAR    *pszAttrib - attribute
   double   fValue
returns
   BOOL - TRUE if success
*/
BOOL CMMLNode2::AttribSetDouble (WCHAR *pszAttrib, double fValue)
{
   return AttribSetUniveral (AttribFind(pszAttrib), pszAttrib, &fValue,
      sizeof(fValue), MMNA_DOUBLE);
}

/********************************************************************
CMMLNode2::AttribGetDouble- Gets an atribute

inputs
   WCHAR    *pszAttrib - attribute
   double   *pfValue - Filled in if gets the value
returns
   BOOL - TRUE if success, FALSE if cant find
*/
BOOL CMMLNode2::AttribGetDouble (WCHAR *pszAttrib, double *pfValue)
{
   PMMNATTRIB pa = AttribGetUniversal (pszAttrib, MMNA_DOUBLE);
   if (pa) {
      *pfValue = *((double*)((PBYTE)m_memHeap.p + pa->dwDataOffset));
      return TRUE;
   }
   else
      return FALSE;
}


/********************************************************************
CMMLNode2::AttribSetBinary- Sets an attribute of the MML node. If the attribute
is already set the old one is overwritten.

inputs
   WCHAR    *pszAttrib - attribute
   PVOID    pData - Data
   DWORD    dwSize - Size of the data
returns
   BOOL - TRUE if success
*/
BOOL CMMLNode2::AttribSetBinary (WCHAR *pszAttrib, PVOID pData, size_t dwSize)
{
   return AttribSetUniveral (AttribFind(pszAttrib), pszAttrib, pData,
      dwSize, MMNA_BINARY);
}

/********************************************************************
CMMLNode2::AttribGetDouble- Gets an atribute

inputs
   WCHAR    *pszAttrib - attribute
   DWORD    *pdwSize - Filled in with the size
returns
   PVOID - Binary data, or NULL if can't find
*/
PVOID CMMLNode2::AttribGetBinary (WCHAR *pszAttrib, size_t *pdwSize)
{
   PMMNATTRIB pa = AttribGetUniversal (pszAttrib, MMNA_BINARY);
   if (pa) {
      *pdwSize = pa->dwDataAlloc;
      return (PBYTE)m_memHeap.p + pa->dwDataOffset;
   }
   else
      return NULL;
}

/********************************************************************
CMMLNode2::AttribDelete - Deletes an attribute.

inputs
   WCHAR    *pszAttrib - attribute
returns
   BOOL - TRUE/FALSE
*/
BOOL CMMLNode2::AttribDelete (WCHAR *pszAttrib)
{
   // set the dirty flag
   //DirtySet ();

   // NOTE: Not tested, although CMMLNode2::AttributeDelete doesn't seem to be called anywhere

   DWORD dwIndex = AttribFind (pszAttrib);
   if (dwIndex == -1)
      return FALSE;

   // free up the attribute string
   PMMNATTRIB pa = (PMMNATTRIB)m_listMMNATTRIB.Get(dwIndex);
   EscAtomicStringFree (pa->pszName);


   m_listMMNATTRIB.Remove (dwIndex);
   // cant really release the heap space

   return TRUE;
}

/********************************************************************
CMMLNode2::AttribEnum - Enumerates an attributes.

inputs
   DWORD    dwNum - Attribute number starting at 0 and increasing by 1.
               If dwNum >= # of attributes, then the function will return an
               error.
   PWSTR    *ppszAttrib - Filled with a pointer to the attribute name.
               Valid until attribute deleted/changed.
   PWSTR    *ppszValue - Filled with a pointer to the attribute value.
               Valid until attribute dleteed/changed
returns
   BOOL - TRUE if found. FALSE if dwNum is too high
*/
BOOL CMMLNode2::AttribEnum (DWORD dwNum, PWSTR *ppszAttrib, PWSTR *ppszValue)
{
   PMMNATTRIB pa = (PMMNATTRIB)m_listMMNATTRIB.Get(dwNum);
   if (!pa) {
      if (ppszValue)
         *ppszValue = NULL;
      if (ppszAttrib)
         *ppszAttrib = NULL;
      return FALSE;
   }

   // if have a ppszValue request then may need to convert
   if (ppszValue) {
      // NOTE: This could be really slow
      if (!AttribConvert (pa, MMNA_STRING)) {
         if (ppszValue)
            *ppszValue = NULL;
         if (ppszAttrib)
            *ppszAttrib = NULL;
         return FALSE;
      }

      // store away
      *ppszValue = (PWSTR)((PBYTE)m_memHeap.p + pa->dwDataOffset);
   }

   if (ppszAttrib)
      *ppszAttrib = pa->pszName;

   return TRUE;
}


/********************************************************************
CMMLNode2::AttribEnumNoConvert - Enumerates an attribute, but DOESN'T convert
the type.

inputs
   DWORD    dwNum - Attribute number starting at 0 and increasing by 1.
               If dwNum >= # of attributes, then the function will return an
               error.
   PWSTR    *ppszAttrib - Filled with a pointer to the attribute name.
               Valid until attribute deleted/changed.
   PVOID    *ppValue - Filled with a pointer to the attribute value.
               Valid until attribute dleteed/changed
   DWORD    *pdwType - Filled with the type of attribute
   DWORD    *pdwSize - Filled with the size... really only useful for binary
returns
   BOOL - TRUE if found. FALSE if dwNum is too high
*/
BOOL CMMLNode2::AttribEnumNoConvert (DWORD dwNum, PWSTR *ppszAttrib, PVOID *ppValue, DWORD *pdwType, size_t *pdwSize)
{
   PMMNATTRIB pa = (PMMNATTRIB)m_listMMNATTRIB.Get(dwNum);
   if (!pa) {
      if (ppValue)
         *ppValue = NULL;
      if (ppszAttrib)
         *ppszAttrib = NULL;
      return FALSE;
   }

   if (ppszAttrib)
      *ppszAttrib = pa->pszName;
   if (ppValue)
      *ppValue = (PBYTE)m_memHeap.p + pa->dwDataOffset;
   if (pdwType)
      *pdwType = pa->dwType;
   if (pdwSize)
      *pdwSize = pa->dwDataAlloc;

   return TRUE;
}


/********************************************************************
CMMLNode2::AttribNum - Returns the number of attributes.

inputs
   none
returns
   DWORD - number
*/
DWORD CMMLNode2::AttribNum (void)
{
   return m_listMMNATTRIB.Num();
}

/********************************************************************
CMMLNode2::NameGet - Returns a pointer to the element's name. It's
   valid until the node is renamed or deleted.

inputs
   none
returns
   WCHAR* - Name
*/
WCHAR *CMMLNode2::NameGet (void)
{
   return m_pszName;
}


/********************************************************************
CMMLNode2::NameSet - Changes the name of the node.

inputs
   WCHAR    *pszName - new name
returns
   BOOL - FALSE if error
*/
BOOL CMMLNode2::NameSet (WCHAR *pszName)
{
   // set the dirty flag
   //DirtySet ();

   PWSTR pszOld = m_pszName;
   m_pszName = EscAtomicStringAdd (pszName);
   if (pszOld)
      EscAtomicStringFree (pszOld);

   return TRUE;
}

/********************************************************************
CMMLNode2 - Some useful public variables

  m_dwType - Type of node
      MMLCLASS_ELEMENT - Element that affects the text.
      MMLCLASS_MACRO - Macro definition <!Macro></>
      MMLCLASS_PARSEINSTRUCTION - Parser instruction <?Instruction?>

*/

/********************************************************************
CMMLNode2::ContentAdd - Add a string to the content of the node.

inputs
   WCHAR    *psz - string. (this already has &amp; etc. preparsed)
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode2::ContentAdd (WCHAR *psz)
{
   return ContentInsert (m_listMMNCONTENT.Num(), psz);
}

/********************************************************************
CMMLNode2::ContentAdd - Add another node to the content. Note: When
   a node is part of the content and the object is deleted, the node
   will also be deleted

  NOTE: This has problems across EXE/DLL boundary. Use ContentAddClone()
  or ContentAddNew() across boundary.
inputs
   CMMLNode2*   pNode - Node to add
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode2::ContentAdd (CMMLNode2 *pNode)
{
   return ContentInsert (m_listMMNCONTENT.Num(), pNode);
}

/********************************************************************
CMMLNode2::ContentAddCloneNode - Add another node to the content.
   The node is cloned so it ill not be dleted.

inputs
   CMMLNode2*   pNode - Node to add
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode2::ContentAddCloneNode (CMMLNode2 *pNode)
{
   return ContentInsert (m_listMMNCONTENT.Num(), pNode->Clone());
}

/********************************************************************
CMMLNode2::ContentAddNewNode - Add another node to the content.
   It's a new node

inputs  
   none
returns
   CMMLNode2*   pNode - Node to add
*/
PCMMLNode2 CMMLNode2::ContentAddNewNode (void)
{
   PCMMLNode2   pNew;
   pNew = new CMMLNode2;
   if (!pNew)
      return NULL;

   if (!ContentAdd (pNew)) {
      delete pNew;
      return FALSE;
   }
   return pNew;
}

/********************************************************************
CMMLNode2::ContentInsert - Inserts a string into the node.

inputs
   DWORD    dwIndex - Index to insert before, starting at 0. See CMMLNode2::Enum.
   WCHAR    *psz - string. (this already has &amp; etc. preparsed)
returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CMMLNode2::ContentInsert (DWORD dwIndex, WCHAR *psz)
{
   // set the dirty flag
   //DirtySet ();

   // allocate the memory needed
   MMNCONTENT mc;
   mc.dwType = MMNC_STRING;
   mc.dwDataSize = (wcslen(psz)+1)*sizeof(WCHAR);
   mc.dwDataOffset = NodeHeapAlloc (mc.dwDataSize);
   if (mc.dwDataOffset == -1)
      return FALSE;
   memcpy ((PBYTE)m_memHeap.p + mc.dwDataOffset, psz, mc.dwDataSize);

   return m_listMMNCONTENT.Insert (dwIndex, &mc);
}


/********************************************************************
CMMLNode2::ContentInsert - Inserts another node to the content. Note: When
   a node is part of the content and the object is deleted, the node
   will also be deleted

inputs
   DWORD    dwIndex - Index to insert before, starting at 0. See CMMLNode2::Enum.
   CMMLNode2*   pNode - Node to insert
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode2::ContentInsert (DWORD dwIndex, CMMLNode2 *pNode)
{
   // set the dirty flag
   //DirtySet ();

   // allocate the memory needed
   MMNCONTENT mc;
   mc.dwType = MMNC_NODE;
   PCMMLNode2 *ppn = (PCMMLNode2*)(&mc.dwDataOffset);
   *ppn = pNode;

   // set it's parent
   //pNode->m_pParent = this;

   // add
   return m_listMMNCONTENT.Insert (dwIndex, &mc);
}


/********************************************************************
CMMLNode2::ContentRemove - Removes an element of content.

inputs
   DWORD    dwIndex - See CMMLNode2::Enum
   BOOL     fDelete - If remove another node and this is TRUE (default)
               then delete the other node object. Else, don't dlete it
               and assume the caller will deal with it.
               NOTE: Having problems with new/delete across DLL/EXE,
               so suggest don't use FALSE.
returns
   BOOL - TRUE if successful, FALSE if error
*/
BOOL CMMLNode2::ContentRemove (DWORD dwIndex, BOOL fDelete)
{
   PMMNCONTENT   pc = (PMMNCONTENT) m_listMMNCONTENT.Get (dwIndex);
   if (!pc)
      return FALSE;

   // set the dirty flag
   //DirtySet ();

   // if it's another node and delete is true, then delete
   if (pc->dwType == MMNC_NODE) {
      PCMMLNode2 *ppn = (PCMMLNode2*)(&pc->dwDataOffset);
      if (*ppn) {
         if (fDelete)
            delete *ppn;
         //else
         //   (*ppn)->m_pParent = NULL;   // disconnect from main tree
      }
   }

   return m_listMMNCONTENT.Remove (dwIndex);
}

/********************************************************************
CMMLNode2::ContentEnum - Enumerates an element in the content.

inputs
   DWORD    dwIndex - Starting at 0. If >= num of content then returns error
   PWSTR    *ppsz - If the content item is a string, this is filled in.
               The string is valid until its deleted or changed
   PCMMLNode2   *ppNode - If the content item is a node, this is filled in.
returns
   BOOL - TRUE if successful, FALSE if error
*/
BOOL CMMLNode2::ContentEnum (DWORD dwIndex, PWSTR *ppsz, PCMMLNode2 *ppNode)
{
   *ppsz = NULL;
   *ppNode = NULL;

   PMMNCONTENT   pc;
   pc = (PMMNCONTENT) m_listMMNCONTENT.Get(dwIndex);
   if (!pc)
      return FALSE;

   if (pc->dwType == MMNC_STRING)
      *ppsz = (PWSTR)((PBYTE)m_memHeap.p + pc->dwDataOffset);
   else
      *ppNode = *((PCMMLNode2*)(&pc->dwDataOffset));

   return TRUE;
}

/********************************************************************
CMMLNode2::ContentFind - Returns the index number where an element with the
   given name occurs.

inputs
   PWSTR    psz - Element name looking for
   PWSTR    pszAttrib - Find the node containing this attribute name. If
               NULL then just finds the first node with name psz
   PWSTR    pszValue - Find the first node containing pszAttrib with
               this value. If NULL then doesn't check value.
returns
   DWORD - Inedx number. -1 if cant find
*/
DWORD CMMLNode2::ContentFind (PWSTR psz, PWSTR pszAttrib, PWSTR pszValue)
{
   PWSTR pszString;
   PCMMLNode2   pSub;

   DWORD i;
   for (i = 0; i < ContentNum(); i++) {
      if (!ContentEnum(i, &pszString, &pSub))
         break;
      if (!pSub)
         continue;

      // make sure the name string matches
      // BUGFIX - Allow to pass in null psz, so that way can search for any contents
      // with a specific attribute
      if (psz && _wcsicmp(pSub->NameGet(), psz))
         continue;

      // make sure attribute string matches
      if (pszAttrib) {
         PWSTR pszv;
         pszv = pSub->AttribGetString(pszAttrib);
         if (!pszv)
            continue;   // doesn't contain right attribute

         if (pszValue && _wcsicmp(pszv, pszValue))
            continue;   // values don't match
      }

      // else found it
      return i;
   }

   return (DWORD) -1;
}

/********************************************************************
CMMLNode2::DirtySet - Finds the root node of the tree by following
the curent node's parent up until there's a NULL m_pParent. It then
sets m_fDirty to TRUE.

inputs
   void
returns
   void
*/
#if 0 // no longer used
void CMMLNode2::DirtySet (void)
{
   PCMMLNode2   pCur;

   pCur = this;
   while (pCur->m_pParent)
      pCur = pCur->m_pParent;

   pCur->m_fDirty = TRUE;
}
#endif // 0

/********************************************************************
CMMLNode2::ContentNum - Returns the number of content elements

inputs
   none
returns
   DWORD - number
*/
DWORD CMMLNode2::ContentNum (void)
{
   return m_listMMNCONTENT.Num();
}













/********************************************************************
CMMLNode - Constructor & destructor
*/
CMMLNode::CMMLNode (void)
{
   m_dwType = MMLCLASS_ELEMENT;
   m_fDirty = FALSE;
   m_pParent = NULL;
   m_pszName = NULL;
   m_lAttribName.Init (sizeof(PWSTR));
}

CMMLNode::~CMMLNode (void)
{
   // if the content has any nodes then delete them
   DWORD i;
   for (i = 0; i < m_listContent.Num(); i++) {
      MMCONTENT   *p;
      p = (MMCONTENT*) m_listContent.Get (i);
      if (!p)
         continue;

      // if it's another node and delete is true, then delete
      if (p->dwType == 1) {
         CMMLNode *pnode;
         pnode = *((PCMMLNode*) (p+1));
         if (pnode)
            delete pnode;
      }
   }

   // free up the attributes
   PWSTR *ppsz = (PWSTR*)m_lAttribName.Get(0);
   for (i = 0; i < m_lAttribName.Num(); i++)
      EscAtomicStringFree (ppsz[i]);

   // free up name
   if (m_pszName)
      EscAtomicStringFree (m_pszName);

}


/********************************************************************
CMMLNode::Clone - Clones the current node and all its children.

returns
   CMMLNOde* - Cloned node. NULL if error
*/
CMMLNode* CMMLNode::Clone (void)
{
   PCMMLNode   pNode;
   pNode = new CMMLNode;
   if (!pNode)
      return NULL;

   // clone name
   pNode->m_dwType = m_dwType;
   pNode->m_fDirty = m_fDirty;
   pNode->m_pParent = NULL;
   pNode->NameSet(NameGet());

   // clone the attributes
   DWORD i;
   for (i = 0; i < AttribNum(); i++) {
      WCHAR *pszAttrib, *pszValue;
      if (!AttribEnum (i, &pszAttrib, &pszValue))
         continue;
      if (!pNode->AttribSet (pszAttrib, pszValue)) {
         delete pNode;
         return NULL;
      }
   }

   // clone the contents
   for (i = 0; i < ContentNum(); i++) {
      WCHAR *psz;
      PCMMLNode   pSubNode;
      if (!ContentEnum (i, &psz, &pSubNode))
         continue;

      // if it's a string easy
      if (psz) {
         if (!pNode->ContentAdd (psz)) {
            delete pNode;
            return NULL;
         }
         continue;
      }

      // else node, clone
      PCMMLNode   pClone;
      pClone = pSubNode->Clone();
      if (!pClone) {
         delete pNode;
         return NULL;
      }
      if (!pNode->ContentAdd (pClone)) {
         delete pNode;
         return NULL;
      }
   }

   // done
   return pNode;
}


/********************************************************************
CMMLNode::AttribFind - Internal function to find an attribute.

inputs
   PWSTR       pszName - attribute name
returns
   DWORD - Index in attributes, or -1 if error
*/
DWORD CMMLNode::AttribFind (PWSTR pszName)
{
   PWSTR *ppsz = (PWSTR*)m_lAttribName.Get(0);
   DWORD i;
   for (i = 0; i < m_lAttribName.Num(); i++)
      if (!_wcsicmp(ppsz[i], pszName))
         return i;
   
   return (DWORD)-1; // cant find

}


/********************************************************************
CMMLNode::AttribSet - Sets an attribute of the MML node. If the attribute
is already set the old one is overwritten.

inputs
   WCHAR    *pszAttrib - attribute
   WCHAR    *pszValue - value
returns
   BOOL - TRUE if success
*/
BOOL CMMLNode::AttribSet (WCHAR *pszAttrib, WCHAR *pszValue)
{
   // set the dirty flag
   DirtySet ();

   DWORD dwFind = AttribFind (pszAttrib);
   if (dwFind != (DWORD)-1) {
      m_lAttribData.Set (dwFind, pszValue, (wcslen(pszValue)+1)*sizeof(WCHAR));
      return TRUE;
   }

   // else add
   PWSTR psz = EscAtomicStringAdd (pszAttrib);
   if (!psz)
      return FALSE;
   m_lAttribName.Add (&psz);
   m_lAttribData.Add (pszValue, (wcslen(pszValue)+1)*sizeof(WCHAR));

   return TRUE;
}

/********************************************************************
CMMLNode::AttribGet - Returns a pointer to the string for the attribute.

inputs
   WCHAR    *pszAttrib - attribute
returns
   WCHAR * - Pointer to the string for the value. Do not change this.
               This pointer is valid until the attribute is changed or
               deleted. If the attribute is not found, returns NULL
*/
WCHAR* CMMLNode::AttribGet (WCHAR *pszAttrib)
{
   DWORD dwFind = AttribFind (pszAttrib);
   if (dwFind == (DWORD)-1)
      return NULL;

   return (PWSTR)m_lAttribData.Get (dwFind);
}

/********************************************************************
CMMLNode::AttribDelete - Deletes an attribute.

inputs
   WCHAR    *pszAttrib - attribute
returns
   BOOL - TRUE/FALSE
*/
BOOL CMMLNode::AttribDelete (WCHAR *pszAttrib)
{
   // set the dirty flag
   DirtySet ();

   DWORD dwFind = AttribFind (pszAttrib);
   if (dwFind == (DWORD)-1)
      return FALSE;

   PWSTR *ppsz = (PWSTR*)m_lAttribName.Get(0);
   EscAtomicStringFree (ppsz[dwFind]);
   m_lAttribName.Remove (dwFind);
   m_lAttribData.Remove (dwFind);

   return TRUE;
}

/********************************************************************
CMMLNode::AttribEnum - Enumerates an attributes.

inputs
   DWORD    dwNum - Attribute number starting at 0 and increasing by 1.
               If dwNum >= # of attributes, then the function will return an
               error.
   PWSTR    *ppszAttrib - Filled with a pointer to the attribute name.
               Valid until attribute deleted/changed.
   PWSTR    *ppszValue - Filled with a pointer to the attribute value.
               Valid until attribute dleteed/changed
returns
   BOOL - TRUE if found. FALSE if dwNum is too high
*/
BOOL CMMLNode::AttribEnum (DWORD dwNum, PWSTR *ppszAttrib, PWSTR *ppszValue)
{
   PWSTR *ppsz = (PWSTR*)m_lAttribName.Get(dwNum);
   if (!ppsz) {
      *ppszValue = NULL;
      return FALSE;
   }

   *ppszAttrib = ppsz[0];

   // get the value
   *ppszValue = (PWSTR)m_lAttribData.Get(dwNum);

   return (*ppszValue) ? TRUE : FALSE;
}


/********************************************************************
CMMLNode::AttribNum - Returns the number of attributes.

inputs
   none
returns
   DWORD - number
*/
DWORD CMMLNode::AttribNum (void)
{
   return m_lAttribName.Num();
}

/********************************************************************
CMMLNode::NameGet - Returns a pointer to the element's name. It's
   valid until the node is renamed or deleted.

inputs
   none
returns
   WCHAR* - Name
*/
WCHAR *CMMLNode::NameGet (void)
{
   return m_pszName;
}


/********************************************************************
CMMLNode::NameSet - Changes the name of the node.

inputs
   WCHAR    *pszName - new name
returns
   BOOL - FALSE if error
*/
BOOL CMMLNode::NameSet (WCHAR *pszName)
{
   // set the dirty flag
   DirtySet ();

   PWSTR pszOld = m_pszName;
   m_pszName = EscAtomicStringAdd (pszName);
   if (pszOld)
      EscAtomicStringFree (pszOld);

   return TRUE;
}

/********************************************************************
CMMLNode - Some useful public variables

  m_dwType - Type of node
      MMLCLASS_ELEMENT - Element that affects the text.
      MMLCLASS_MACRO - Macro definition <!Macro></>
      MMLCLASS_PARSEINSTRUCTION - Parser instruction <?Instruction?>

*/

/********************************************************************
CMMLNode::ContentAdd - Add a string to the content of the node.

inputs
   WCHAR    *psz - string. (this already has &amp; etc. preparsed)
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode::ContentAdd (WCHAR *psz)
{
   return ContentInsert (m_listContent.Num(), psz);
}

/********************************************************************
CMMLNode::ContentAdd - Add another node to the content. Note: When
   a node is part of the content and the object is deleted, the node
   will also be deleted

  NOTE: This has problems across EXE/DLL boundary. Use ContentAddClone()
  or ContentAddNew() across boundary.
inputs
   CMMLNode*   pNode - Node to add
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode::ContentAdd (CMMLNode *pNode)
{
   return ContentInsert (m_listContent.Num(), pNode);
}

/********************************************************************
CMMLNode::ContentAddCloneNode - Add another node to the content.
   The node is cloned so it ill not be dleted.

inputs
   CMMLNode*   pNode - Node to add
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode::ContentAddCloneNode (CMMLNode *pNode)
{
   return ContentInsert (m_listContent.Num(), pNode->Clone());
}

/********************************************************************
CMMLNode::ContentAddNewNode - Add another node to the content.
   It's a new node

inputs  
   none
returns
   CMMLNode*   pNode - Node to add
*/
PCMMLNode CMMLNode::ContentAddNewNode (void)
{
   PCMMLNode   pNew;
   pNew = new CMMLNode;
   if (!pNew)
      return NULL;

   if (!ContentAdd (pNew)) {
      delete pNew;
      return FALSE;
   }
   return pNew;
}

/********************************************************************
CMMLNode::ContentInsert - Inserts a string into the node.

inputs
   DWORD    dwIndex - Index to insert before, starting at 0. See CMMLNode::Enum.
   WCHAR    *psz - string. (this already has &amp; etc. preparsed)
returns
   BOOL - TRUE if OK. FALSE if error
*/
BOOL CMMLNode::ContentInsert (DWORD dwIndex, WCHAR *psz)
{
   // memory
   CMem  mem;
   size_t dwNeeded;
   dwNeeded = sizeof(MMCONTENT) + (wcslen(psz)+1)*sizeof(WCHAR);
   if (!mem.Required (dwNeeded))
      return FALSE;

   // set the dirty flag
   DirtySet ();

   MMCONTENT   *p;
   p = (MMCONTENT*) mem.p;
   p->dwType = 0;

   wcscpy ((WCHAR*) (p+1), psz);

   return m_listContent.Insert (dwIndex, mem.p, dwNeeded);
}


/********************************************************************
CMMLNode::ContentInsert - Inserts another node to the content. Note: When
   a node is part of the content and the object is deleted, the node
   will also be deleted

inputs
   DWORD    dwIndex - Index to insert before, starting at 0. See CMMLNode::Enum.
   CMMLNode*   pNode - Node to insert
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CMMLNode::ContentInsert (DWORD dwIndex, CMMLNode *pNode)
{
   // memory
   CMem  mem;
   DWORD dwNeeded;
   dwNeeded = sizeof(MMCONTENT) + sizeof(PCMMLNode);
   if (!mem.Required (dwNeeded))
      return FALSE;

   // set the dirty flag
   DirtySet ();

   MMCONTENT   *p;
   p = (MMCONTENT*) mem.p;
   p->dwType = 1;

   *((CMMLNode**) (p+1)) = pNode;

   // set it's parent
   pNode->m_pParent = this;

   return m_listContent.Insert (dwIndex, mem.p, dwNeeded);
}


/********************************************************************
CMMLNode::ContentRemove - Removes an element of content.

inputs
   DWORD    dwIndex - See CMMLNode::Enum
   BOOL     fDelete - If remove another node and this is TRUE (default)
               then delete the other node object. Else, don't dlete it
               and assume the caller will deal with it.
               NOTE: Having problems with new/delete across DLL/EXE,
               so suggest don't use FALSE.
returns
   BOOL - TRUE if successful, FALSE if error
*/
BOOL CMMLNode::ContentRemove (DWORD dwIndex, BOOL fDelete)
{
   MMCONTENT   *p;
   p = (MMCONTENT*) m_listContent.Get (dwIndex);
   if (!p)
      return FALSE;

   // set the dirty flag
   DirtySet ();

   // if it's another node and delete is true, then delete
   if (p->dwType == 1) {
      CMMLNode *pnode;
      pnode = *((PCMMLNode*) (p+1));
      if (pnode)
         pnode->m_pParent = NULL;   // disconnect from main tree

      if (pnode && fDelete)
         delete pnode;
   }

   return m_listContent.Remove (dwIndex);
}

/********************************************************************
CMMLNode::ContentEnum - Enumerates an element in the content.

inputs
   DWORD    dwIndex - Starting at 0. If >= num of content then returns error
   PWSTR    *ppsz - If the content item is a string, this is filled in.
               The string is valid until its deleted or changed
   PCMMLNode   *ppNode - If the content item is a node, this is filled in.
returns
   BOOL - TRUE if successful, FALSE if error
*/
BOOL CMMLNode::ContentEnum (DWORD dwIndex, PWSTR *ppsz, PCMMLNode *ppNode)
{
   *ppsz = NULL;
   *ppNode = NULL;

   MMCONTENT   *p;
   p = (MMCONTENT*) m_listContent.Get(dwIndex);
   if (!p)
      return FALSE;

   if (p->dwType == 0) {
      *ppsz = (WCHAR*)(p+1);
   }
   else {
      *ppNode = *((PCMMLNode*)(p+1));
   }

   return TRUE;
}

/********************************************************************
CMMLNode::ContentFind - Returns the index number where an element with the
   given name occurs.

inputs
   PWSTR    psz - Element name looking for
   PWSTR    pszAttrib - Find the node containing this attribute name. If
               NULL then just finds the first node with name psz
   PWSTR    pszValue - Find the first node containing pszAttrib with
               this value. If NULL then doesn't check value.
returns
   DWORD - Inedx number. -1 if cant find
*/
DWORD CMMLNode::ContentFind (PWSTR psz, PWSTR pszAttrib, PWSTR pszValue)
{
   PWSTR pszString;
   PCMMLNode   pSub;

   DWORD i;
   for (i = 0; i < ContentNum(); i++) {
      if (!ContentEnum(i, &pszString, &pSub))
         break;
      if (!pSub)
         continue;

      // make sure the name string matches
      // BUGFIX - Allow to pass in null psz, so that way can search for any contents
      // with a specific attribute
      if (psz && _wcsicmp(pSub->NameGet(), psz))
         continue;

      // make sure attribute string matches
      if (pszAttrib) {
         PWSTR pszv;
         pszv = pSub->AttribGet(pszAttrib);
         if (!pszv)
            continue;   // doesn't contain right attribute

         if (pszValue && _wcsicmp(pszv, pszValue))
            continue;   // values don't match
      }

      // else found it
      return i;
   }

   return (DWORD) -1;
}

/********************************************************************
CMMLNode::DirtySet - Finds the root node of the tree by following
the curent node's parent up until there's a NULL m_pParent. It then
sets m_fDirty to TRUE.

inputs
   void
returns
   void
*/
void CMMLNode::DirtySet (void)
{
   PCMMLNode   pCur;

   pCur = this;
   while (pCur->m_pParent)
      pCur = pCur->m_pParent;

   pCur->m_fDirty = TRUE;
}

/********************************************************************
CMMLNode::ContentNum - Returns the number of content elements

inputs
   none
returns
   DWORD - number
*/
DWORD CMMLNode::ContentNum (void)
{
   return m_listContent.Num();
}




/********************************************************************
CMMLNode::CloneAsCMMLNode2 - Clones the current node and all its children.

returns
   CMMLNOde2* - Cloned node. NULL if error
*/
CMMLNode2* CMMLNode::CloneAsCMMLNode2 (void)
{
   PCMMLNode2   pNode;
   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   // clone name
   pNode->NameSet(NameGet());
   pNode->m_dwType = m_dwType;

   // clone the attributes
   DWORD i;
   for (i = 0; i < AttribNum(); i++) {
      WCHAR *pszAttrib, *pszValue;
      if (!AttribEnum (i, &pszAttrib, &pszValue))
         continue;
      if (!pNode->AttribSetString (pszAttrib, pszValue)) {
         delete pNode;
         return NULL;
      }
   }

   // clone the contents
   for (i = 0; i < ContentNum(); i++) {
      WCHAR *psz;
      PCMMLNode   pSubNode;
      if (!ContentEnum (i, &psz, &pSubNode))
         continue;

      // if it's a string easy
      if (psz) {
         if (!pNode->ContentAdd (psz)) {
            delete pNode;
            return NULL;
         }
         continue;
      }

      // else node, clone
      PCMMLNode2   pClone;
      pClone = pSubNode->CloneAsCMMLNode2();
      if (!pClone) {
         delete pNode;
         return NULL;
      }
      if (!pNode->ContentAdd (pClone)) {
         delete pNode;
         return NULL;
      }
   }

   // done
   return pNode;
}



/**********************************************************************
MMLDoubleToString - Given a fp value, writes out a string.

inputs
   fp      fVal - Value
   PWSTR       psz - String
*/
void MMLDoubleToString (double fVal, PWSTR psz)
{
   swprintf (psz, L"%g", (double)fVal);   // BUGFIX - Was .9, but switch to juset %g so 6 digits preceision
}

/**********************************************************************
MMLDoubleFromString - Given a string, fills in a fp vale

inputs
   PWSTR       psz - String
*/
double MMLDoubleFromString (PWSTR psz)
{
   char sza[64];
   double fVal;
   size_t dwNeeded;
   dwNeeded = wcslen(psz);
   if (dwNeeded > sizeof(sza)/2)
      return 0;
   WideCharToMultiByte (CP_ACP,0,psz,-1,sza,sizeof(sza),0,0);
   fVal = atof (sza);

   return fVal;
}

/**********************************************************************
MMLHexDigit - Converts a number from 0 to 15 to a hex digits.

inputs
   BYTE     bVal - Value from 0 to 15
returns
   WCHAR - '0'..'9', 'a'..'f'
*/
WCHAR MMLHexDigit (BYTE bVal)
{
   if (bVal < 10)
      return L'0' + bVal;
   else
      return L'a' + bVal - 10;
}

/**********************************************************************
MMLUnHexDigit - Converts a hex digit 0..9,a..f to a number from 0 to 15.

inputs
   WCHAR       c- '0'..'9', 'a'..'f'
returns
   BYTE     -Value from 0 to 15
*/
BYTE MMLUnHexDigit (WCHAR c)
{
   if ((c >= L'0') && (c <= L'9'))
      return (BYTE)(c - L'0');
   else if ((c >= L'a') && (c <= L'f'))
      return (BYTE)(c - L'a' + 10);
   else if ((c >= L'A') && (c <= L'F'))
      return (BYTE)(c - L'A' + 10);
   else
      return 0;
}


/**********************************************************************
MMLBinaryToString - Converts binary data to a string of hex.

inputs
   PBYTE       pb - data
   DWORD       dwSize - # of byptes
   PWSTR       psz - String to be filled with data. Must bw dwSize*2+1 chars
returns
   none
*/
void MMLBinaryToString (PBYTE pb, size_t dwSize, PWSTR psz)
{
   size_t i;
   for (i = 0; i < dwSize; i++) {
      psz[i*2+0] = MMLHexDigit(pb[i] >> 4);
      psz[i*2+1] = MMLHexDigit(pb[i] & 0x0f);
   }
   psz[i*2] = 0;
}


/***********************************************************************
MMLBinaryFromString - Converts a string of hex digits into binary data

inputs
   PWSTR       psz - Null-terminated string
   PBYTE       pb - Filled with the data
   DWORD       dwSize - size of pb.
returns
   DWORD - Number of bytes actually filled in. 0 if error (such as dwSize not large enough)
*/
size_t MMLBinaryFromString (PWSTR psz, PBYTE pb, size_t dwSize)
{
   size_t dwLen = wcslen(psz);
   if ((dwLen > dwSize*2) || (dwLen % 2))
      return 0;   // not large enough

   DWORD i;
   for (i = 0; i < dwLen; i+=2) {
      pb[i/2] = (MMLUnHexDigit(psz[i]) << 4) + MMLUnHexDigit(psz[i+1]);
   }

   return dwLen/2;
}
