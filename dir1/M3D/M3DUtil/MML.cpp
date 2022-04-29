/*******************************************************************************8
MML.cpp - Functions for reading and writing MML. Used by the objects to
serialize themselves to disk.

begun 31/10/2001 by Mike Rozak
Copyright Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

static PWSTR gszValue = L"Value";
static PWSTR gszValueShort = L"v";
static PWSTR gszObjectName = L"ASPObject";
static PWSTR gszClassCode = L"ClassCode";
static PWSTR gszClassSub = L"ClassSub";
static PWSTR gszObjectsSet = L"ObjectsSet";


/**********************************************************************
MMLValueFind - Finds a sub-node with name pszName.

inputs
   PCMMLNode2   pNode - To create the subnode in
   PWSTR       pszID - Name
   BOOL        fCreateIfNotExist - If TRUE thren create if it can't be found
returns
   PCMMLNode2 - Sub node
*/
PCMMLNode2 MMLValueFind (PCMMLNode2 pNode, PWSTR pszID, BOOL fCreateIfNotExist = FALSE)
{
   // see if it exists
   DWORD dwFind;
   dwFind = pNode->ContentFind (pszID);
   if (dwFind < pNode->ContentNum()) {
      // found it
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      pNode->ContentEnum (dwFind, &psz, &pSub);
      if (pSub)
         return pSub;
   }

   if (!fCreateIfNotExist)
      return NULL;

   // else not hree so just create
   PCMMLNode2   pSub;
   pSub = pNode->ContentAddNewNode ();
   if (!pSub)
      return NULL;
   pSub->NameSet (pszID);

   return pSub;
}



/***********************************************************************
MMLValueSet - Given a MML node, set the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PCWSTR      pszValue - value
returns
   BOOL - true if successful
*/
BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PWSTR pszValue)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, TRUE);
   if (!pSub)
      return FALSE;

   return pSub->AttribSetString (gszValueShort, pszValue);
#if 0 // odl code
   return pSub->AttribSet (gszValue, pszValue);
#endif
}

/***********************************************************************
MMLValueSet - Given a MML node, set the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PBYTE       pb - Pointer to an array of bytes
   DWORD       dwSize - # of bytes in pb
returns
   BOOL - true if successful
*/
BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PBYTE pb, size_t dwSize)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, TRUE);
   if (!pSub)
      return FALSE;

   return pSub->AttribSetBinary (gszValueShort, pb, dwSize);

#if 0 // old code
   WCHAR szHuge[1024];
   if (dwSize+1 > sizeof(szHuge)/2/2) {   // BUGFIX - would crash if dwSize=512 exactly
      // BUGFIX - If large chunk than allocate memory
      CMem mem;
      if (!mem.Required ((dwSize*2+2)*sizeof(WCHAR)))
         return FALSE;
      MMLBinaryToString (pb, dwSize, (PWSTR) mem.p);
      return MMLValueSet (pNode, pszID, (PWSTR) mem.p);
   }
   MMLBinaryToString (pb, dwSize, szHuge);

   return MMLValueSet (pNode, pszID, szHuge);
#endif
}

/***********************************************************************
MMLValueSet - Given a MML node, set the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   int         iValue - Value to set
returns
   BOOL - true if successful
*/
BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, int iValue)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, TRUE);
   if (!pSub)
      return FALSE;

   return pSub->AttribSetInt (gszValueShort, iValue);

#if 0 // old code
   WCHAR szTemp[32];
   swprintf (szTemp, L"%d", iValue);

   return MMLValueSet (pNode, pszID, szTemp);
#endif // 0
}

/***********************************************************************
MMLValueSet - Given a MML node, set the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   fp      fValue - value
returns
   BOOL - true if successful
*/
BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, fp fValue)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, TRUE);
   if (!pSub)
      return FALSE;

   return pSub->AttribSetDouble (gszValueShort, fValue);

#if 0 // old code
   WCHAR szTemp[32];

   // BUGFIX - Take less space
   MMLDoubleToString (fValue, szTemp);
   return MMLValueSet (pNode, pszID, szTemp);

#if 0 // old code
   return MMLValueSet (pNode, pszID, (PBYTE) &fValue, sizeof(fValue));
#endif // new code
#endif // 0
}

/***********************************************************************
MMLValueSet - Given a MML node, set the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PCPoint     p - Point
returns
   BOOL - true if successful
*/
BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PCPoint p)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, TRUE);
   if (!pSub)
      return FALSE;

   DWORD i;
   WCHAR sz[8];
   //WCHAR szBinary[32];
   for (i = 0; i < 4; i++) {
      // dont bother writing 1.0 for last point
      if ((i == 3) && (p->p[3] == 1.0))
         continue;

      sz[0] = L'q';  // BUGFIX - changed from v to q so will be able to reload old
      sz[1] = L'0' + (WCHAR)i;
      sz[2] = 0;

      // BUGFIX - write out doubles
      // MMLDoubleToString (p->p[i], szBinary);
      if (!pSub->AttribSetDouble(sz, p->p[i]))
         return FALSE;

#if 0 // old code
      MMLBinaryToString ((PBYTE) &p->p[i], sizeof(p->p[i]), szBinary);
      if (!pSub->AttribSet(sz, szBinary))
         return FALSE;
#endif 0
   }

   return TRUE;
}

/***********************************************************************
MMLValueSet - Given a MML node, set the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PTEXTUREPOINT p
returns
   BOOL - true if successful
*/
BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PTEXTUREPOINT p)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, TRUE);
   if (!pSub)
      return FALSE;

   //WCHAR szBinary[32];
   //MMLDoubleToString (p->h, szBinary);
   if (!pSub->AttribSetDouble(L"a", p->h))  // was "h"
      return FALSE;
   //MMLDoubleToString (p->v, szBinary);
   if (!pSub->AttribSetDouble(L"b", p->v))  // vas "v"
      return FALSE;
#if 0 // old code
   MMLBinaryToString ((PBYTE) &p->h, sizeof(p->h), szBinary);
   if (!pSub->AttribSet(L"h", szBinary))
      return FALSE;
   MMLBinaryToString ((PBYTE) &p->v, sizeof(p->v), szBinary);
   if (!pSub->AttribSet(L"v", szBinary))
      return FALSE;
#endif

   return TRUE;
}

/***********************************************************************
MMLValueSet - Given a MML node, set the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PCMatrix    p - Matrix
returns
   BOOL - true if successful
*/
BOOL MMLValueSet (PCMMLNode2 pNode, PWSTR pszID, PCMatrix p)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, TRUE);
   if (!pSub)
      return FALSE;

   DWORD i,j;
   WCHAR sz[8];
   //WCHAR szBinary[32];
   for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
      sz[0] = L'q';  // BUGFIX - Was L'v';
      sz[1] = L'0' + (WCHAR)i;
      sz[2] = L'0' + (WCHAR)j;
      sz[3] = 0;

      //MMLDoubleToString (p->p[i][j], szBinary);
#if 0 // old code
      MMLBinaryToString ((PBYTE) &p->p[i][j], sizeof(p->p[i][j]), szBinary);
#endif
      if (!pSub->AttribSetDouble(sz, p->p[i][j]))
         return FALSE;
   }

   return TRUE;
}

/***********************************************************************
MMLValueGet - Given a MML node, get the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
returns
   PWSTR - String for the attribute. This si only valid until pNode is changed
      or deleted.
*/
PWSTR MMLValueGet (PCMMLNode2 pNode, PWSTR pszID)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return NULL;

   PWSTR psz;
   psz = pSub->AttribGetString (gszValueShort);
   if (psz)
      return psz;
#ifdef _IMPORTOLD
   return pSub->AttribGetString (gszValue);
#endif   // _IMPORTOLD
   return NULL;
}

#if 0// no longer used
/***********************************************************************
MMLValueGetNew - Given a MML node, get the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
returns
   PWSTR - String for the attribute. This si only valid until pNode is changed
      or deleted.
*/
PWSTR MMLValueGetNew (PCMMLNode2 pNode, PWSTR pszID)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return NULL;

   return pSub->AttribGet (gszValueShort);
}
#endif // 0

#ifdef _IMPORTOLD
/***********************************************************************
MMLValueGetOld - Given a MML node, get the attribute.

  // only used for backwards compatability 

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
returns
   PWSTR - String for the attribute. This si only valid until pNode is changed
      or deleted.
*/
PWSTR MMLValueGetOld (PCMMLNode2 pNode, PWSTR pszID)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return NULL;

   return pSub->AttribGet (gszValue);
}
#endif   // _IMPORTOLD

/***********************************************************************
MMLValueGetInt - Given a MML node, get the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   int         iDefault - Default value if cant find
returns
   int - integer value
*/
int MMLValueGetInt (PCMMLNode2 pNode, PWSTR pszID, int iDefault)
{
   //WCHAR *psz;
   int iRet;
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return iDefault;
   if (!pSub->AttribGetInt (gszValueShort, &iRet))
      return iDefault;
   return iRet;

#if 0 // old code
   psz = MMLValueGetNew (pNode, pszID);
#ifdef _IMPORTOLD
   if (!psz)
      psz = MMLValueGetOld (pNode, pszID);
#endif
   if (!psz)
      return iDefault;

   int i;
   i = -1;
   if (!AttribToDecimal (psz, &i))
      return iDefault;
   return i;
#endif // 0, old code
}


/***********************************************************************
MMLValueGetBinary - Given a MML node, get the binary value.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PBYTE       pb - Filled in with the binary data
   DWORD       dwSize - # of bytes in pb
returns
   DWORD - number of bytes filled in
*/
size_t MMLValueGetBinary (PCMMLNode2 pNode, PWSTR pszID, PBYTE pb, size_t dwSize)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return 0;
   PVOID pData;
   size_t dwSizeNeed;
   pData = pSub->AttribGetBinary (gszValueShort, &dwSizeNeed);
   if (!pData)
      return 0;
   if (dwSizeNeed > dwSize)
      return 0; // not large enough
   memcpy (pb, pData, dwSizeNeed);
   return dwSizeNeed;

#if 0 // dead code
   return pSub->AttribGet (gszValueShort);

   WCHAR *psz;
   psz = MMLValueGetNew (pNode, pszID);
#ifdef _IMPORTOLD
   if (!psz)
      psz = MMLValueGetOld (pNode, pszID);
#endif
   if (!psz)
      return 0;

   return MMLBinaryFromString (psz, pb, dwSize);
#endif // 0 - dead code
}

/***********************************************************************
MMLValueGetBinary - Given a MML node, get the binary value.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PCMe        pMem - Filled in with binary if successful. m_dwCurPosn will be the length
returns
   DWORD - number of bytes filled in, or if if no error
*/
size_t MMLValueGetBinary (PCMMLNode2 pNode, PWSTR pszID, PCMem pMem)
{
   pMem->m_dwCurPosn = 0;
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return 0;
   PVOID pData;
   size_t dwSizeNeed;
   pData = pSub->AttribGetBinary (gszValueShort, &dwSizeNeed);
   if (!pData)
      return 0;
   if (!pMem->Required (dwSizeNeed))
      return 0;
   pMem->m_dwCurPosn = dwSizeNeed;
   memcpy (pMem->p, pData, dwSizeNeed);
   return dwSizeNeed;
}

/***********************************************************************
MMLValueGetDouble - Given a MML node, get the fp value.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   fp      fDefault - Default
returns
   fp - Read in value, or default if error
*/
fp MMLValueGetDouble (PCMMLNode2 pNode, PWSTR pszID, fp fDefault)
{
   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return fDefault;
   double fRet;
   if (!pSub->AttribGetDouble (gszValueShort, &fRet))
      return fDefault;
   return fRet;

#if 0 // old code
   return pSub->AttribGet (gszValueShort);
   //DWORD dw;
   //fp f;
   PWSTR psz;
   psz = MMLValueGetNew (pNode, pszID);
   if (psz)
      return MMLDoubleFromString (psz);
#ifdef _IMPORTOLD
   dw = MMLValueGetBinary (pNode, pszID, (PBYTE) &f, sizeof(f));
   if (dw != sizeof(f))
      return fDefault;
   return f;
#endif
   return fDefault;
#endif // 0
}

/***********************************************************************
MMLValueGetPoint - Given a MML node, get the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PCPoint     p - Point. Filled in.
   PCPoint     pDefault - Use this as the default value if cant find
returns
   BOOL - true if successful
*/
BOOL MMLValueGetPoint (PCMMLNode2 pNode, PWSTR pszID, PCPoint p, PCPoint pDefault)
{
   if (pDefault)
      p->Copy(pDefault);

   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return FALSE;

   DWORD i;
   WCHAR sz[8];
   //PWSTR psz;
   BOOL fFound;
   fFound = FALSE;
   for (i = 0; i < 4; i++) {
      // try new one first
      sz[0] = L'q';
      sz[1] = L'0' + (WCHAR)i;
      sz[2] = 0;
      double fVal;
      if (pSub->AttribGetDouble (sz, &fVal)) {
         p->p[i] = fVal;
         fFound = TRUE;
         continue;
      }

#ifdef _IMPORTOLD
      sz[0] = L'v';
      sz[1] = L'0' + (WCHAR)i;
      sz[2] = 0;
      psz = pSub->AttribGet (sz);
      if (psz) {
         MMLBinaryFromString (psz, (PBYTE) &p->p[i], sizeof(p->p[i]));
         continue;
      }
#endif

      // if this is the last point and didn't find then set to 1
      if (fFound && (i == 3))
         p->p[3] = 1.0;
   }

   return TRUE;
}



/***********************************************************************
MMLValueGetTEXTUREPOINT - Given a MML node, get the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PTEXTUREPOINT     p - Point. Filled in.
   PTEXTUREPOINT     pDefault - Use this as the default value if cant find
returns
   BOOL - true if successful
*/
BOOL MMLValueGetTEXTUREPOINT (PCMMLNode2 pNode, PWSTR pszID, PTEXTUREPOINT p, PTEXTUREPOINT pDefault)
{
   if (pDefault)
      *p = *pDefault;

   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return FALSE;

   //PWSTR psz;
   double fVal;
   if (pSub->AttribGetDouble (L"a", &fVal))
      p->h = fVal;
   else {
#ifdef _IMPORTOLD
      psz = pSub->AttribGet (L"h");
      if (psz)
         MMLBinaryFromString (psz, (PBYTE) &p->h, sizeof(p->h));
#endif
   }
   if (pSub->AttribGetDouble (L"b", &fVal))
      p->v = fVal;
   else {
#ifdef _IMPORTOLD
      psz = pSub->AttribGet (L"v");
      if (psz)
         MMLBinaryFromString (psz, (PBYTE) &p->v, sizeof(p->v));
#endif
   }

   return TRUE;
}


/***********************************************************************
MMLValueGetMatrix - Given a MML node, get the attribute.

inputs
   PCMMLNode2   pNode
   PCWSTR      pszID - ID of the value
   PCMatrix     p - Matrix. Filled in.
   PCMatrix     pDefault - Use this as the default value if cant find
returns
   BOOL - true if successful
*/
BOOL MMLValueGetMatrix (PCMMLNode2 pNode, PWSTR pszID, PCMatrix p, PCMatrix pDefault)
{
   if (pDefault)
      p->Copy(pDefault);

   PCMMLNode2 pSub = MMLValueFind (pNode, pszID, FALSE);
   if (!pSub)
      return FALSE;

   DWORD i,j;
   WCHAR sz[8];
   //PWSTR psz;
   for (i = 0; i < 4; i++) for (j=0; j < 4; j++){
      // BUGFIX - less space
      sz[0] = L'q';
      sz[1] = L'0' + (WCHAR)i;
      sz[2] = L'0' + (WCHAR)j;
      sz[3] = 0;
      double fVal;
      if (pSub->AttribGetDouble (sz, &fVal)) {
         p->p[i][j] = fVal;
         continue;
      }

#ifdef _IMPORTOLD
      sz[0] = L'v';
      sz[1] = L'0' + (WCHAR)i;
      sz[2] = L'0' + (WCHAR)j;
      sz[3] = 0;
      psz = pSub->AttribGet (sz);
      if (!psz)
         continue;
      MMLBinaryFromString (psz, (PBYTE) &p->p[i][j], sizeof(p->p[i][j]));
#endif
   }

   return TRUE;
}


/**********************************************************************
MMLFromMem - Returns a MML tree (which must be deleted) from a null-terminated
unicode string.

inputs
   PCWSTR         psz - String
returns
   PCMMLNode2      - Top node
*/
PCMMLNode2 MMLFromMem (PCWSTR psz)
{
   PCMMLNode   pNew;
   PCMMLNode2 pNew2;
   CEscError   err;
   pNew = ParseMML ((PWSTR) psz, ghInstance, NULL, NULL, &err, FALSE);
   if (!pNew)
      return NULL;
   pNew2 = pNew->CloneAsCMMLNode2();
   delete pNew;

   return pNew2;
}

/**********************************************************************
MMLToMem - Writes the current MML node to memory. Recursive.

inputs
   PCMMLNode2      pNode - node
   PCMem          pMem - memory object. Written to at the current location
   BOOL           fSkipTag - If TRUE don't write tag start/end (only use TRUE for this for main node)
   DWORd          dwIndent - If fIndent == TRUE mode to indent tags so they're readable
   BOOL           fIndent - If TRUE then indent, making tags more readable
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL MMLToMem (PCMMLNode2 pNode, PCMem pMem, BOOL fSkipTag, DWORD dwIndent, BOOL fIndent)
{
   DWORD i;
   size_t dwRequired, dwNeeded;

   // BUGFIX - If we're close to filling up the memory then add more.
   // Use large chunks because these files are big
   if (pMem->m_dwCurPosn + 1024 > pMem->m_dwAllocated)
      pMem->Required (pMem->m_dwAllocated + 0x10000);
      // BUGFIX - Was adding 1024 - Changed to 0x10000 so ESCREALLOC less

   if (fIndent) {
      // to make this more readable
      pMem->StrCat (L"\r\n");
      for (i = 0; i < dwIndent; i++)
         pMem->StrCat (L" ");
   }


   if (!fSkipTag) {
      // write the intro tag
      pMem->StrCat (L"<");
      // BUGFIX - Do macro strings
      if (pNode->m_dwType == MMLCLASS_MACRO)
         pMem->StrCat (L"!");
      else if (pNode->m_dwType == MMLCLASS_PARSEINSTRUCTION)
         pMem->StrCat (L"?");

      // BUGFIX - so dont crash
      PWSTR pszMainName = pNode->NameGet();
      if (!pszMainName)
         pszMainName = L"Unknown";
      pMem->StrCat (pszMainName);

      // attributes?
      for (i = 0; i < pNode->AttribNum(); i++) {
         PWSTR pszName, pszValue;
         if (!pNode->AttribEnum(i, &pszName, &pszValue)) // NOTE: OK to use here because wont convert
            continue;   // not supposed to happen

         // do we need quotes
         DWORD j;
         BOOL fNeedQuotes;
         fNeedQuotes = FALSE;
         for (j = 0; pszValue[j]; j++) {
            if ((pszValue[j] >= L'0') && (pszValue[j] <= L'9'))
               continue;   // dont need
            if ((pszValue[j] >= L'A') && (pszValue[j] <= L'Z'))
               continue;   // dont need
            if ((pszValue[j] >= L'a') && (pszValue[j] <= L'z'))
               continue;   // dont need
            if ((pszValue[j] == L'+') || (pszValue[j] == L'-') || (pszValue[j] == L'.'))
               continue;

            // else, need quotes
            fNeedQuotes = TRUE;
            break;
         }

         // write it out
         pMem->StrCat (L" ");
         pMem->StrCat (pszName);
         if (fNeedQuotes) {
            pMem->StrCat (L"=\"");

            // IMPORTANT: This doesnt do macros <!xMacro> or <?Questions?>

            // make sure large enough for string
            dwRequired = wcslen(pszValue)*sizeof(WCHAR) * 2;   // just make sure large enough
      tryagain:
            if (!pMem->Required (dwRequired + pMem->m_dwCurPosn))
               return FALSE;  // error
            dwNeeded = 0;
            if (!StringToMMLString (pszValue, (WCHAR*) ((PBYTE)pMem->p + pMem->m_dwCurPosn),
               pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded)) {

               // need more
               dwRequired = dwNeeded;
               if (dwNeeded)
                  goto tryagain;
            }
            pMem->m_dwCurPosn += wcslen((PWSTR) ((PBYTE) pMem->p + pMem->m_dwCurPosn))*2;

            // ending quote
            pMem->StrCat (L"\"");
         }
         else {
            pMem->StrCat (L"=");
            pMem->StrCat (pszValue);
         }
      }

      // if there are no contents then end it here
      if (pNode->m_dwType == MMLCLASS_PARSEINSTRUCTION)
         pMem->StrCat (L"?");
      else if (!pNode->ContentNum()) {
         pMem->StrCat (L"/>");
         // fskiptag = FALSE, so dont null terminate
         return TRUE;
      }

      pMem->StrCat (L">");

   }  // fskiptag

   // loop through the contents
   for (i = 0; i < pNode->ContentNum(); i++) {
      PWSTR psz;
      PCMMLNode2   pSub;

      if (!pNode->ContentEnum(i, &psz, &pSub))
         continue;   // shouldnt happen

      if (pSub) {
         if (!MMLToMem (pSub, pMem, FALSE, fSkipTag ? dwIndent : (dwIndent+3), fIndent))  // BUGFIX - Was +1
            return FALSE;
         continue;
      }

      // else string
      if (!psz[0])
         continue;   // empty

      // convert
      // make sure large enough for string
      dwRequired = wcslen(psz)*sizeof(WCHAR) * 2;   // just make sure large enough
tryagain2:
      if (!pMem->Required (dwRequired + pMem->m_dwCurPosn))
         return FALSE;  // error
      dwNeeded = 0;
      if (!StringToMMLString (psz, (WCHAR*) ((PBYTE)pMem->p + pMem->m_dwCurPosn),
         pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded)) {

         // need more
         dwRequired = dwNeeded;
         if (dwNeeded)
            goto tryagain2;
      }
      pMem->m_dwCurPosn += wcslen((PWSTR) ((PBYTE) pMem->p + pMem->m_dwCurPosn))*2;

   }

   if (fIndent) {
      // to make this more readable
      pMem->StrCat (L"\r\n");
      for (i = 0; i < dwIndent; i++)
         pMem->StrCat (L" ");
   }

   if (!fSkipTag) {
      // end it
      pMem->StrCat (L"</");
      // BUGFIX - so dont crash
      PWSTR pszMainName = pNode->NameGet();
      if (!pszMainName)
         pszMainName = L"Unknown";
      pMem->StrCat (pszMainName);
      pMem->StrCat (L">");
   }

   if (fSkipTag)
      pMem->CharCat (0);   // null terminate
   return TRUE;
}



/**********************************************************************************
MMLFromObject - Given an object, this creates a MML node for it. The node
contains the object's class GUIDs so it can be recreated.

inputs
   PCObjectSocket    pObject - object
returns
   PCMMLNode2 - Node. It's up to the caller to delete this.
*/
PCMMLNode2 MMLFromObject (PCObjectSocket pObject)
{
   PCMMLNode2 pNode;
   pNode = pObject->MMLTo ();
   if (!pNode)
      return NULL;

   // set the name and some attributes
   pNode->NameSet (gszObjectName);
   
   // the the info
   OSINFO info;
   pObject->InfoGet (&info);

   // guids
   //WCHAR szHuge[128];
   //MMLBinaryToString ((PBYTE)&info.gClassCode, sizeof(info.gClassCode), szHuge);
   pNode->AttribSetBinary (gszClassCode, &info.gCode, sizeof(info.gCode));
   //MMLBinaryToString ((PBYTE)&info.gClassSub, sizeof(info.gClassSub), szHuge);
   pNode->AttribSetBinary (gszClassSub, &info.gSub, sizeof(info.gSub));

   // done
   return pNode;
}

/**********************************************************************************
MMLFromObjects - Given an list of object, this creates a MML node for them. The
returned node can be passed to MMLToObjects().

inputs
   PCWorldSocket           pWorld - World to use
   DWORD             *padwIndex - Pointer to an array of DWORDs, that are an index
                     into which objects should be used. If this is NULL then all objects
                     in the world are used.
   DWORD             dwSize - Number of elements in padwIndex.
   PCProgressSocket        pProgress - Progress meter
returns
   PCMMLNode2 - Node. It's up to the caller to delete this.
*/
PCMMLNode2 MMLFromObjects (PCWorldSocket pWorld, DWORD *padwIndex, DWORD dwSize, PCProgressSocket pProgress)
{
   PCMMLNode2 pNode, pSub;

   pNode = new CMMLNode2;
   if (!pNode)
      return pNode;
   pNode->NameSet (gszObjectsSet);

   DWORD i, dwNum;
   dwNum = (padwIndex ? dwSize : pWorld->ObjectNum());
   DWORD dwNextProgress, dwProgressSkip;
   dwProgressSkip = max(dwNum / 16,1); 
   dwNextProgress = dwProgressSkip;
   for (i = 0; i < dwNum; i++) {
      PCObjectSocket pObject = pWorld->ObjectGet(padwIndex ? padwIndex[i] : i);
      if (!pObject)
         continue;

      // update progress bar
      dwNextProgress--;
      if (!dwNextProgress && pProgress) {
         switch (pProgress->Update ((fp)i / (fp) dwNum)) {
         case 1:
            dwProgressSkip /= 2;
            dwProgressSkip = max(1,dwProgressSkip);
            break;
         case -1:
            dwProgressSkip *= 2;
            dwProgressSkip = min(dwProgressSkip, dwNum / 8);
            break;
         }
         dwNextProgress = dwProgressSkip;
      }

      pSub = MMLFromObject (pObject);
      if (!pSub)
         continue;

      // add it
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/**********************************************************************************
MMLToObject - Given MML (that's of type gszObjectName) this creates the object.
The object must be added to the world, of course, and is up to the caller to delete.

inputs
   PCMMLNode2      pNode - Node thats of type gszObjectName
returns
   PCObjectSocket - object. NULL if error
*/
PCObjectSocket MMLToObject (DWORD dwRenderShard, PCMMLNode2 pNode)
{
   // get the class code and sub
   GUID gCode, gSub;
   PVOID pBinary;
   size_t dwSize;
   pBinary = pNode->AttribGetBinary (gszClassCode, &dwSize);
   if (!pBinary || (sizeof(gCode) != dwSize))
      return NULL;
   gCode = *((GUID*)pBinary);

   pBinary = pNode->AttribGetBinary (gszClassSub, &dwSize);
   if (!pBinary || (sizeof(gCode) != dwSize))
      return NULL;
   gSub = *((GUID*)pBinary);

   PCObjectSocket pNew;
   pNew = ObjectCFCreate (dwRenderShard, &gCode, &gSub);
   // BUGFIX - Remove the back-off case
   //if (!pNew)
   //   pNew = ObjectCFCreate (&gCode, NULL);  // try empty
   if (!pNew)
      return NULL;

   // pass the MML into this
   if (!pNew->MMLFrom (pNode)) {
      pNew->Delete();
      return NULL;
   }

   // else have it so return
   return pNew;
}


/**********************************************************************************
MMLToObjects - Given MML derived from MMLFromObjects, this adds then to the world.

  NOTE: This does NOT do WorldSetFinished(), so the objects have not clue they're
  in purgatory (if that's where they're put), which is what I want.

inputs
   PCWorldSocket        pWorld - World to add to
   PCMMLNode2      pNode - Node
   BOOL           fSelect - If TRUE then select all the added ones, else don't
   BOOL           fRename - If TRUE then rename objects that add
   PCProgressSocket     pProgress - Show progress on. Can beNULL
returns
   BOOL - TRUE if OK and all objects loaded, FALSE if some objects failed to load
*/
BOOL MMLToObjects (PCWorldSocket pWorld, PCMMLNode2 pNode, BOOL fSelect, BOOL fRename, PCProgressSocket pProgress)
{
   BOOL fRet = TRUE;
   // keep a list of the old GUIDs and what they're changed to
   CListFixed  lOld, lNew, lRemove;
   lOld.Init (sizeof(GUID));
   lNew.Init (sizeof(GUID));
   lRemove.Init (sizeof(GUID));

   DWORD dwRenderShard = pWorld->RenderShardGet();

   // loop through pNode and find all the ones of type gszObjectName
   DWORD i,k;
   DWORD dwNextProgress, dwProgressSkip;
   dwProgressSkip = max(pNode->ContentNum() / 16,1); 
   dwNextProgress = dwProgressSkip;
   for (i = 0; ; i++) {
      PWSTR psz;
      PCMMLNode2 pSub;
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         break;
      if (!pSub)
         continue;
      
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (_wcsicmp(psz, gszObjectName))
         continue;

      // update progress bar
      dwNextProgress--;
      if (!dwNextProgress && pProgress) {
         switch (pProgress->Update ((fp)i / (fp) pNode->ContentNum())) {
         case 1:
            dwProgressSkip /= 2;
            dwProgressSkip = max(1,dwProgressSkip);
            break;
         case -1:
            dwProgressSkip *= 2;
            dwProgressSkip = min(dwProgressSkip, pNode->ContentNum() / 8);
            break;
         }
         dwNextProgress = dwProgressSkip;
      }

      // found one
      PCObjectSocket p;
      p = MMLToObject (dwRenderShard, pSub);
      if (!p) {
         fRet = FALSE;
         continue;   // ignore this if cant open
      }

      // old guid
      GUID gObject;
      p->GUIDGet (&gObject);
      lOld.Add (&gObject);

      // add it
      DWORD dwIndex;
      dwIndex = pWorld->ObjectAdd (p, TRUE, fRename ? NULL : &gObject);

      // and the new guid
      p->GUIDGet (&gObject);
      lNew.Add (&gObject);

      if (dwIndex == (DWORD)-1) {
         p->Delete();
         fRet = FALSE;
         continue;
      }

      // select it?
      if (fSelect)
         pWorld->SelectionAdd (dwIndex);
   }

   if (!fRename)
      return fRet;

   // loop through all the objects that added
   GUID *pCur, *pCur2, *pCur3;
   DWORD j;
   for (i = 0; i < lNew.Num(); i++) {
      pCur = (GUID*) lNew.Get(i);
      PCObjectSocket pOSCur;
      pOSCur = pWorld->ObjectGet(pWorld->ObjectFind (pCur));
      if (!pOSCur)
         continue;   // shouldnt happen

      // if it's embedded then tell it of the rename
      GUID gTemp;
      if (pOSCur->EmbedContainerGet (&gTemp)) {
         // find this in the list
         for (j = 0; j < lOld.Num(); j++) {
            pCur2 = (GUID*) lOld.Get(j);
            if (IsEqualGUID(*pCur2, gTemp)) {
               pCur2 = (GUID*) lNew.Get(j);
               pOSCur->EmbedContainerRenamed (pCur2);
               break;
            }
         }

         // if its parent didn't get transferred over then
         // it's all alone, so tell it its no longer owned
         if (j >= lOld.Num()) {
            pOSCur->EmbedContainerSet (NULL);
         }
      }

      // BUGFIX - loop through all the attached objects and rename
      for (k = 0; k < pOSCur->AttachNum(); k++) {
         if (!pOSCur->AttachEnum (k, &gTemp))
            continue;

         // just go through all the pasted objects and rename them
         for (j = 0; j < lOld.Num(); j++) {
            pCur2 = (GUID*) lOld.Get(j);
            pCur3 = (GUID*) lNew.Get(j);
            if (IsEqualGUID(*pCur2, gTemp)) {
               pOSCur->AttachRenamed (pCur2, pCur3);
               break;
            }
         }  // j
      }

      // loop through the guids
      lRemove.Clear();
      for (k = 0; k < pOSCur->ContEmbeddedNum(); k++) {
         if (!pOSCur->ContEmbeddedEnum(k, &gTemp))
            continue;

         // just go through all the pasted objects and rename them
         for (j = 0; j < lOld.Num(); j++) {
            pCur2 = (GUID*) lOld.Get(j);
            pCur3 = (GUID*) lNew.Get(j);
            if (IsEqualGUID(*pCur2, gTemp)) {
               pOSCur->ContEmbeddedRenamed (pCur2, pCur3);
               break;
            }
         }  // j

         // if we didn't find this then remove it
         if (j >= lOld.Num())
            lRemove.Add (&gTemp);
      }

      // remove any guids contained in object that no longer exist
      for (j = 0; j < lRemove.Num(); j++) {
         pCur2 = (GUID*) lRemove.Get(j);
         pOSCur->ContEmbeddedRemove (pCur2);
      }

   }


   return fRet;
}

