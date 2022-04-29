/***********************************************************************
Util.cpp*/

#include <windows.h>
#include "escarpment.h"
#include "resource.h"
#include "oilpaint.h"

/***********************************************************************
MemCat - Concatenated a unicode string onto a CMem object. It also
adds null-termination, although the m_dwCurPosn is decreased by 2 so
then next MemCat() will overwrite the NULL.

inputs
   PCMem    pMem - memory object
   PWSTR    psz - string
*/
void MemCat (PCMem pMem, PWSTR psz)
{
   pMem->StrCat (psz);
   DWORD dwTemp;
   dwTemp = pMem->m_dwCurPosn;
   pMem->CharCat(0);
   pMem->m_dwCurPosn = dwTemp;
}

/***********************************************************************
MemCat - Concatenated a number onto a CMem object. It also
adds null-termination, although the m_dwCurPosn is decreased by 2 so
then next MemCat() will overwrite the NULL.

inputs
   PCMem    pMem - memory object
   int      iNum - number
*/
void MemCat (PCMem pMem, int iNum)
{
   WCHAR szTemp[32];
   swprintf (szTemp, L"%d", iNum);
   MemCat (pMem, szTemp);
}


/***********************************************************************
MemCat - Concatenated a number onto a CMem object. It also
adds null-termination, although the m_dwCurPosn is decreased by 2 so
then next MemCat() will overwrite the NULL.

inputs
   PCMem    pMem - memory object
   double   fNum - number
   BOOL     fDollars - If fDollars TRUE then it shows to 2 decimal places
*/
void MemCat (PCMem pMem, double fNum, BOOL fDollars)
{
   WCHAR szTemp[64];
   swprintf (szTemp, fDollars ? L"%.2f" : L"%g", fNum);
   MemCat (pMem, szTemp);
}

/***********************************************************************
MemZero - Zeros out memory
inputs
   PCMem    pMem - memory object
*/
void MemZero (PCMem pMem)
{
   pMem->m_dwCurPosn = 0;
   pMem->CharCat(0);
   pMem->m_dwCurPosn = 0;
}

/***********************************************************************
MemCatSanitize - Concatenates a memory string, but sanitizes it for MML
first.

inputs
   PCMem    pMem - memory object
   PWSTR    psz - Needs to be sanitized
*/
void MemCatSanitize (PCMem pMem, PWSTR psz)
{
   // assume santizied will be about the same size
   pMem->Required(pMem->m_dwCurPosn + (wcslen(psz)+1)*2);
   size_t dwNeeded;
   if (StringToMMLString(psz, (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn),
      pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded)) {

      pMem->m_dwCurPosn += wcslen((PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn))*2;
      return;  // worked
   }

   // else need larger
   pMem->Required(pMem->m_dwCurPosn + dwNeeded + 2);
   StringToMMLString(psz, (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn),
      pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded);
   pMem->m_dwCurPosn += wcslen((PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn))*2;
}

