/*******************************************************************************
MMLCompress.cpp - Functions for compressing and decompressing MML.

begun 9/10/2002 by Mike Rozak
Copyright Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

#ifdef _WIN64     // BUGBUG - hack
#define _WIN64MEMDEBUG
#endif



#define TT_STRING             MMNA_STRING  // UNICODE string
#define TT_BINARY             MMNA_BINARY  // binary value
#define TT_INT                MMNA_INT  // integer value
#define TT_FLOAT              MMNA_DOUBLE  // floating point value
#define TT_BUILTIN            4  // Built in string
#define TT_COMMONBUILTIN      5  // special for storing the most common built in


// TOKENTYPE - For returning token info
typedef struct {
   DWORD       dwType;     // one of MMNA_XXX
} TOKENTYPE, *PTOKENTYPE;

#define PATENCODEGROUPHDR     0xf284f043     // random number used for header of patternencodegroup so know if latest version


typedef struct {
   PWSTR       pszString;     // string
   DWORD       dwLeft;        // left branch index into list. Use 0 if not valid
   DWORD       dwRight;       // right branch index into list. Use 0 if not valid
   DWORD       dwTimesUsed;   // number of times this string used - for optimization later on and debug
   DWORD       dwTokenType;   // type of token... see TT_XXX
   DWORD       dwMapTo;       // what token ID this is mapped to
   int         iValue;        // 32-bit integer valud (if it's a TT_INT)
   fp          fValue;        // floating-point value (if it's a TT_FP)
   PVOID       pBinary;       // binary data. If use this then iValue is the size
} MMLTOK, *PMMLTOK;

// reocrde encode & decode
typedef struct {
   DWORD       dwValue;       // value
   DWORD       dwLeft;        // left branch index into list. Use 0 if not valid
   DWORD       dwRight;       // right branch index into list. Use 0 if not valid
   DWORD       dwTimesUsed;   // number of times this string used - for optimization later on and debug
   DWORD       dwRemappedTo;  // value it's remapped to
} RETOK, *PRETOK;

// pattern header
typedef struct {
   DWORD       dwRemapIndex;  // where remaps start (was dwMax in PatternEncode)
   DWORD       dwNumOrig;     // number of original data points
   DWORD       dwNumNew;      // new data points (DWORDs)
   DWORD       dwConvert;     // number of conversions, each one is 2 DWORDs
} PATTERNHDR, *PPATTERNHDR;


// token map
#define TMVERSION       1234
typedef struct {
   DWORD       dwVersion;     // version check, just to make sure
   DWORD       dwCommonBuiltInID;   // starting ID for common built in
   DWORD       dwStringID;    // token ID that the strings start at
   DWORD       dwBinaryID;    // token ID that the binary values start at
   DWORD       dwIntID;       // toke ID that the integers start at
   DWORD       dwFloatID;     // token ID that the floating points start at
   DWORD       dwBuiltInStringID;   // where the built-in strings do
   DWORD       dwMaxBuiltIn;  // maximum built in stirng
} TMHDR, *PTMHDR;

static CBTree  gTreeBuiltIn;  // tree of built-in strings
static BOOL    gfBuiltInValid = FALSE; // set to true if valid
static BOOL    gfBuiltInDirty = FALSE; // set to true if build in tiry
BOOL gfCompressMax = FALSE;   // if TRUE, will use maximimum compression on files, which will slow down save


/*******************************************************************************
MMLCompressMaxSet - Sets the compress max flag

inputs
   BOOL - TRUE if should compress as much as possible
*/
void MMLCompressMaxSet (BOOL fCompress)
{
   gfCompressMax = fCompress;
}

/*******************************************************************************
MMLCompressMaxGet - Gets the compress max flag

inputs
   BOOL - TRUE if should compress as much as possible
*/
BOOL MMLCompressMaxGet (void)
{
   return gfCompressMax;
}

/*******************************************************************************
BuiltInRead - Reads the built-in compression strings if they're not
already read in.
*/
void BuiltInRead (void)
{
   if (gfBuiltInValid)
      return;

   gfBuiltInValid = TRUE;
   gTreeBuiltIn.m_fIgnoreCase = FALSE;


   // load from resource
   HRSRC    hr;

   hr = FindResource (ghInstance, MAKEINTRESOURCE (IDR_TEXTCOMPRESSSTRING), "TEXT");
   if (!hr)
      return;

   HGLOBAL  hg;
   hg = LoadResource (ghInstance, hr);
   if (!hg)
      return;


   PVOID pMem;
   pMem = LockResource (hg);
   if (!pMem)
      return;

   DWORD dwSize;
   dwSize = SizeofResource (ghInstance, hr);

   CMem mem;
   DWORD dwIndex, dwCR;
   char *pszRes = (char*)pMem;
   char *pszCur;
   WCHAR szw[256];
   DWORD dwIndexNum = 0;
   for (dwIndex = 0; dwIndex < dwSize; dwIndex++) {
      // find CR
      for (dwCR = dwIndex; dwIndex < dwSize; dwCR++)
         if ((pszRes[dwCR] == '\r') || (pszRes[dwCR] == '\n'))
            break;
      if (dwCR == dwIndex) {
         // hit CR right away
         dwIndex = dwCR;
         continue;
      }

      // copy over
      if (!mem.Required (dwCR - dwIndex + 1))
         break;   // error
      pszCur = (char*)mem.p;
      memcpy (pszCur, pszRes + dwIndex, dwCR - dwIndex);
      pszCur[dwCR - dwIndex] = 0;   // NULL terminate

      // else add
      MultiByteToWideChar (CP_ACP, 0, pszCur, -1, szw, sizeof(szw)/2);
      gTreeBuiltIn.Add (szw, &dwIndexNum, sizeof(dwIndexNum));
      dwIndexNum++;
      
      dwIndex = dwCR;
   }

#if 0 // no longer used
   // read in
   char  sza[256], szFile[256];
   WCHAR szw[256];
   FILE *f;
   strcpy (szFile, gszAppDir);
   strcat (szFile, "CompressString." LIBRARYFILEEXTA);
   OUTPUTDEBUGFILE (szFile);
   f = fopen (szFile, "rt");
   if (!f)
      return;
   DWORD dwIndex;
   dwIndex = 0;
   while (fgets(sza, sizeof(sza), f)) {
      char *psz;
      psz = strchr (sza, '\n');
      if (psz)
         *psz = 0;
      psz = strchr (sza, '\r');
      if (psz)
         *psz = 0;
      if (!sza[0])
         continue;

      // else add
      MultiByteToWideChar (CP_ACP, 0, sza, -1, szw, sizeof(szw)/2);
      gTreeBuiltIn.Add (szw, &dwIndex, sizeof(dwIndex));
      dwIndex++;
   }
   fclose (f);
#endif // 0
}

/*******************************************************************************
BuiltInSave - If the built in is dirty then saves.
*/
void BuiltInSave (void)
{
#ifdef _DEBUG
   if (!gfBuiltInDirty)
      return;

   BuiltInRead();
   gfBuiltInDirty = FALSE;

   // write out
   char  sza[256], szFile[256];
   FILE *f;
   strcpy (szFile, gszAppDir);
   strcat (szFile, "CompressString." LIBRARYFILEEXTA);
   OUTPUTDEBUGFILE (szFile);
   f = fopen (szFile, "wt");
   if (!f)
      return;
   DWORD i;
   for (i = 0; i < gTreeBuiltIn.Num(); i++) {
      PWSTR psz = gTreeBuiltIn.Enum(i);
      WideCharToMultiByte (CP_ACP, 0, psz, -1, sza, sizeof(sza), 0, 0);
      strcat (sza, "\n");
      fputs (sza, f);
   }
   fclose (f);
#endif
}

/*******************************************************************************
BuiltInFind - Returns the index to a string. If cant find it as a build in then
returns -1.

NOTE: In special debug version adds new strings to built-in library
*/
DWORD BuiltInFind (PWSTR psz)
{
   // make sure already read in
   BuiltInRead();

   // see if can find
   DWORD *pdw;
   pdw = (DWORD*) gTreeBuiltIn.Find (psz);
   if (pdw)
      return *pdw;

#ifdef _DEBUG
   // NOTE - disabled
   // NOTE checks to make sure dont have guids (for names) written in, or other bad characters
   if (FALSE && (wcslen(psz) < 128) && psz[0] && !wcschr(psz, L'\r') && !wcschr(psz, L'\n') &&
      (psz[0] != L'{') && (psz[wcslen(psz)-1] != L'}')) {
      // else, add it
      DWORD dwIndex;
      dwIndex = gTreeBuiltIn.Num();
      gTreeBuiltIn.Add (psz, &dwIndex, sizeof(dwIndex));
      gfBuiltInDirty = TRUE;
      return dwIndex;
   }
#endif

   return -1;
}


/*******************************************************************************
RLEEncode - Does a run-length encoding on the data

inputs
   PVOID          pOrigMem - Memory to encode
   DWORD          dwNum - Number of elements (NOT the number of bytes)
   DWORD          dwElemSize - Element size (in bytes, 1..N).
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
returns
   DWORD - 0 if OK, 1 if error
*/

DWORD RLEEncode (PBYTE pOrigMem, size_t dwNum, size_t dwElemSize, PCMem pMem)
{
   // BUGFIX - just in case pass in null, because ran out of memory
   if (!pOrigMem)
      return 1;

   // write the number of elements that will result in
   // BUGFIX - Dont think want this
   //if (dwNum < sizeof(DWORD))
   //   return 1;
   if (!pMem->Required (pMem->m_dwCurPosn + 4))
      return 1;
   *((DWORD*)((PBYTE) pMem->p + pMem->m_dwCurPosn)) = (DWORD)dwNum;
   pMem->m_dwCurPosn += sizeof(DWORD);

   // loop while there's data left
   size_t dwRepeat;
   size_t dwNeed;
   while (dwNum) {
      // see how many times this repeats
      if (dwElemSize == 1) {
         // faster for bytes
         for (dwRepeat = 0; (dwRepeat+1 < dwNum) && (dwRepeat < 128); dwRepeat++)
            if (pOrigMem[0] != pOrigMem[dwRepeat+1])
               break;
      }
      else {
         for (dwRepeat = 0; (dwRepeat+1 < dwNum) && dwRepeat < 128; dwRepeat++)
            if (memcmp(pOrigMem, pOrigMem + (dwRepeat+1) * dwElemSize, dwElemSize))
               break;
      }
      if (dwRepeat) {
         // it repeats
         dwNeed = dwElemSize + 1;
         if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
            return 1;
         *((PBYTE) pMem->p + pMem->m_dwCurPosn) = (BYTE) (dwRepeat - 1) | 0x80;
         memcpy ((PBYTE) pMem->p + (pMem->m_dwCurPosn+1), pOrigMem, dwElemSize);
         pMem->m_dwCurPosn += dwNeed;
         pOrigMem += dwElemSize * (dwRepeat+1);
         dwNum -= (dwRepeat+1);
         continue;
      }

      // else, doesn't repeat
      // loop until find repeat
      if (dwElemSize == 1) {
         // faster for bytes
         for (dwRepeat = min(3,dwNum); (dwRepeat < dwNum) && (dwRepeat < 128); dwRepeat++)
            if ((dwRepeat+1 < dwNum) && (pOrigMem[dwRepeat] == pOrigMem[dwRepeat+1])) {
               break;
            }
      }
      else {
         for (dwRepeat = min(3,dwNum); (dwRepeat < dwNum) && dwRepeat < 128; dwRepeat++)
            if ((dwRepeat+1 < dwNum) && (memcmp(pOrigMem + dwRepeat * dwElemSize, pOrigMem + (dwRepeat+1) * dwElemSize, dwElemSize))) {
               break;
            }
      }


      // add this
      dwNeed = dwElemSize * dwRepeat + 1;
      if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
         return 1;
      *((PBYTE) pMem->p + pMem->m_dwCurPosn) = (BYTE) (dwRepeat - 1);
      memcpy ((PBYTE) pMem->p + (pMem->m_dwCurPosn+1), pOrigMem, dwElemSize * dwRepeat);
      pMem->m_dwCurPosn += dwNeed;
      pOrigMem += dwElemSize * dwRepeat;
      dwNum -= dwRepeat;
   }

   // done

   return 0;
}

/*******************************************************************************
RLEDecode - Decodes the encoding from RLEEncode

inputs
   PBYTE          pRLE - RLE encoded
   DWORD          dwSize - Number of bytes left in all the memory
   DWORD          dwElemSize - Element size. 1+
   PCMem          pMem - Filled with the elements. pMem->m_dwCurPosn set to #elems x dwElemSize
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD RLEDecode (PBYTE pRLE, size_t dwSize, size_t dwElemSize,
                     PCMem pMem, size_t *pdwUsed)
{
   *pdwUsed = 0;
   size_t dwOrigSize = dwSize;
   pMem->m_dwCurPosn = 0;

   // get the first DWORD to indicate how many elem
   DWORD dwNum;
   if (dwSize < sizeof(DWORD))
      return 2;
   dwNum = *((DWORD*) pRLE);
   pRLE += sizeof(DWORD);
   dwSize -= sizeof(DWORD);
   if (!pMem->Required (pMem->m_dwCurPosn + dwNum * dwElemSize))
      return 2;

   // decompress
   while (dwNum) {
      // pull out a byte
      if (dwSize < 1)
         return 2;   // error
      BYTE bVal;
      bVal = pRLE[0];
      pRLE++;
      dwSize--;

      if (bVal & 0x80) {   // repeat next segment
         bVal = (bVal & 0x7f) + 2;
         if (!pMem->Required (pMem->m_dwCurPosn + (DWORD)bVal * dwElemSize))
            return 2;
         if (dwNum < (DWORD) bVal)
            return 2;
         dwNum -= (DWORD) bVal;
         while (bVal) {
            memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, pRLE, dwElemSize);
            pMem->m_dwCurPosn += dwElemSize;
            bVal--;
         }
         dwSize -= dwElemSize;
         pRLE += dwElemSize;
      }
      else { // copy next block
         bVal++;
         if (!pMem->Required (pMem->m_dwCurPosn + (DWORD)bVal * dwElemSize))
            return 2;
         if (dwNum < (DWORD) bVal)
            return 2;
         dwNum -= (DWORD) bVal;
         memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, pRLE, dwElemSize * (DWORD)bVal);
         pMem->m_dwCurPosn += dwElemSize * (DWORD) bVal;
         dwSize -= dwElemSize * (DWORD)bVal;
         pRLE += dwElemSize * (DWORD)bVal;
      }
   }

   *pdwUsed = dwOrigSize - dwSize;


   return 0;
}




/*******************************************************************************
PatternEncode - Encodes a sequence by finding repeating pairs of elements and
combing them together. Repeat until no more repeats.

inputs
   PVOID          pOrigMem - Memory to encode
   DWORD          dwNum - Number of elements
   DWORD          dwElemSize - Element size (in bytes, either 1, 2, or 4)
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
   PCProgressSocket     pProgress - Progress bar
returns
   DWORD - 0 if OK, 1 if error
*/

static int _cdecl PatternSort (const void *elem1, const void *elem2)
{
   DWORD *p1, *p2;
   p1 = *((DWORD**) elem1);
   p2 = *((DWORD**) elem2);


   // higher numbers earlier
   if (p1[0] > p2[0])
      return -1;
   else if (p1[0] < p2[0])
      return 1;

   // else compare next
   if (p1[1] > p2[1])
      return -1;
   else if (p1[1] < p2[1])
      return 1;
   else
      return 0;   // dont carea
}

DWORD PatternEncode (PBYTE pOrigMem, size_t dwNum, size_t dwElemSize, PCMem pMem, PCProgressSocket pProgress)
{
   size_t dwOrigNum = dwNum;

   // create memory large enough
   CMem memData;     // data, converted to DWORD blocks
   CMem memPointer;  // pointer to data
   CListFixed lConvert; // list of conversions. Which is a list of 2 DWORDs
   if (!memData.Required ((dwNum+1) * sizeof(DWORD)))
      return 1;
   if (!memPointer.Required (dwNum * sizeof(DWORD*)))
      return 1;
   lConvert.Init (2 * sizeof(DWORD));

   DWORD *padwData;
   DWORD **ppData;
   padwData = (DWORD*) memData.p;
   ppData = (DWORD**) memPointer.p;

   // transfer over
   DWORD i, dwVal, dwMax;
   dwMax = 0;
   for (i = 0; i < dwNum; i++) {
      if (dwElemSize == 4)
         dwVal = *((DWORD*)(pOrigMem + i * sizeof(DWORD)));
      else if (dwElemSize == 2)
         dwVal = *((WORD*)(pOrigMem + i * sizeof(WORD)));
      else
         dwVal = *((BYTE*)(pOrigMem + i * sizeof(BYTE)));


      // keep the max
      dwMax = max(dwMax, dwVal);

      // store away
      padwData[i] = dwVal;
   }
   padwData[dwNum] = -1;   // EOF
   memData.m_dwCurPosn = dwNum * sizeof(DWORD);
   dwMax++;

   // progress bar
   fp fProgress;
   fProgress = 0;
   if (pProgress)
      pProgress->Update (fProgress);

   DWORD j, k, dwMatch, dwConvert, dwMatches, dwSortedAt;
   // BUGFIX - Was while(TRUE) but changed to while(gfCompressMax) so that if max
   // compression isn't set this wont do the pattern recognition part
   while (gfCompressMax) {
      // if there aren't any entries then dont add anything
      if (!dwNum)
         break;

      // create all pointers in sequence so will be able to sort
      for (i = 0; i < dwNum-1; i++)
         ppData[i] = &padwData[i];

      // sort this - Sort 1 less because the element can never be recombined at
      // the beginning of a sequence since it's at the end of the file
      qsort (ppData, dwNum-1, sizeof(DWORD*), PatternSort);

      dwMatches = 0;
      dwSortedAt = lConvert.Num() + dwMax;

      // progress bar
      fProgress = (fProgress + 1.0) / 2.0;
      if (pProgress)
         pProgress->Update (fProgress);

      // loop from start to finish
      for (i = 0; i < dwNum-1; i++) {
         // if this node has been changed then skip
         // NOTE: Dont need to do the following because of dwSortedAt check
         //if ( ((ppData[i])[0] == -1) || ((ppData[i])[1] == -1))
         //   continue;

         // if just changed this this go around then don't process

         if ( ((ppData[i])[0] >= dwSortedAt) || ((ppData[i])[1] >= dwSortedAt))
            continue;

         // loop ahead and find out how many matches there are
         dwMatch = 0;
         for (j = i + 1; j < dwNum-1; j++) {
            // if this node has been changed then skip
            if ( ((ppData[j])[0] == -1) || ((ppData[j])[1] == -1))
               continue;

            if ( ((ppData[i])[0] != (ppData[j])[0]) || ((ppData[i])[1] != (ppData[j])[1]) )
               break;   // they're not the same so have come to the end of the sort

            // else, the same
            dwMatch++;
         }  // over j

         // if there weren't enough matches then just skip this section and move on
         // BUGFIX - Chouse this number because reduced the size the most
         if (dwMatch < 3) {   // 4 or more. since dwMatch doens't include first one.
            i = j-1; // skip far enough ahead
            continue;
         }

         // else, compress these
         DWORD adw[2];
         DWORD dwNum;
         BOOL fAdd;
         if (dwMax > 0x7ffffffe - lConvert.Num())
            break;   // too much data
         dwNum = lConvert.Num();
         dwConvert = dwNum + dwMax;
         adw[0] = (ppData[i])[0];
         adw[1] = (ppData[i])[1];

         // BUGFIX - Check to see if it's the same as the one immediately previous
         fAdd = TRUE;
         if (dwNum) {
            DWORD *padwC = (DWORD*) lConvert.Get(0);
            DWORD *padw;
            for (k = dwNum-1; k < dwNum; k--) {
               padw = padwC + (k*2);
               if ((padw[0] == adw[0]) && (padw[1] == adw[1])) {
                  fAdd = FALSE;
                  dwConvert = dwMax + k;
                  break;
               }
            }
         }

         if (fAdd)
            lConvert.Add (adw);  // add the 2 dwords

         // loop through again
         k = j;
         for (j = i; j < k; j++) {
            if ( (adw[0] != (ppData[j])[0]) || (adw[1] != (ppData[j])[1]) )
               continue;   // they're not the same so have come to the end of the sort

            // else, the same
            (ppData[j])[0] = dwConvert;
            (ppData[j])[1] = -1; // so know its blank
         }  // over j


         // note that had a match and conversion here
         dwMatches++;

         // skip ahead
         i = k-1;
      }  // over i

      // go back over the data and get rid of the -1's
      for (i = j = 0; i < dwNum; i++) {
         if (padwData[i] == -1)
            continue;   // should be removed
         if (i != j)
            padwData[j] = padwData[i];
         j++;
      }
      dwNum = j;
      padwData[dwNum] = -1;

      // progress bar
      fProgress = (fProgress + 1.0) / 2.0;
      if (pProgress)
         pProgress->Update (fProgress);

      // if there weren't any matches then quick
      if (!dwMatches)
         break;
   }  // repeat resort

   // what's left is a list of data in padwData[], with higher values being remaps
   // and a list of remaps in lConvert

   // how much space is needed?
   size_t dwNeed;
   dwNeed = sizeof(PATTERNHDR) + dwNum * sizeof(DWORD) + lConvert.Num() * 2 * sizeof(DWORD);
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 1;

   PPATTERNHDR pph;
   DWORD *padwCompress, *padwConvert;
   pph = (PPATTERNHDR) ((PBYTE)pMem->p + pMem->m_dwCurPosn);
   padwCompress = (DWORD*) (pph+1);
   padwConvert = padwCompress + dwNum;
   pMem->m_dwCurPosn += dwNeed;

   // copy over
   memset (pph, 0, sizeof(PATTERNHDR));
   pph->dwConvert = lConvert.Num();
   pph->dwNumNew = (DWORD)dwNum;
   pph->dwNumOrig = (DWORD) dwOrigNum;
   pph->dwRemapIndex = dwMax;
   memcpy (padwCompress, padwData, dwNum * sizeof(DWORD));
   //memcpy (padwConvert, lConvert.Get(0), lConvert.Num() * 2 * sizeof(DWORD));

   // to subtractive conversion - which will make huffman work better here
   // can subtract because know sorting is from highest to lowest

   // also, resort convert so all first entries, then all second entries so can RLE later
   DWORD *padwOC;
   padwOC = (DWORD*) lConvert.Get(0);
   dwNum = lConvert.Num();
   BOOL fShowOrig;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         padwConvert[i] = padwOC[(i-1)*2] - padwOC[i*2];
         fShowOrig = (padwConvert[i] != 0);
            // if no change then also looping down on 2nd case, so show delta there
      }
      else {
         padwConvert[i] = padwOC[i*2];
         fShowOrig = TRUE;
      }

      if (fShowOrig)
         padwConvert[i + dwNum] = padwOC[i*2+1];
      else
         padwConvert[i + dwNum] = padwOC[(i-1)*2+1] - padwOC[i*2+1];
   }


   return 0;
}
/*******************************************************************************
PatternDecodeConvert - Given a compressed index, this decompresses it.

inputs
   DWORD          dwVal - Potentially compressed value
   DWORD          dwMax - If dwVal >= dwMax it's compressed
   DWORD          *padwConvert - Pointer to list of conversions. Each conversion is 2 DWORDs
   PBYTE          pabDecompTo - Pointer to the START of where decompression happend
   DWORD          dwElemSize - Element size to decompress to
   DWORD          *pdwCurElem - Filled with a pointer to the current element. This
                     is incremeented whena  new element is written
returns
   none
*/
void PatternDecodeConvert (DWORD dwVal, DWORD dwMax, DWORD *padwConvert, PBYTE pabDecompTo,
                           DWORD dwElemSize, DWORD *pdwCurElem)
{
   // if it is a conversion then recurse
   if (dwVal >= dwMax) {
      dwVal -= dwMax;
      PatternDecodeConvert (padwConvert[dwVal*2], dwMax, padwConvert, pabDecompTo, dwElemSize, pdwCurElem);
      PatternDecodeConvert (padwConvert[dwVal*2+1], dwMax, padwConvert, pabDecompTo, dwElemSize, pdwCurElem);
      return;
   }

   // else, it's converted to a single byte/word/etc
   if (dwElemSize == 4)
      ((DWORD*)pabDecompTo)[*pdwCurElem] = dwVal;
   else if (dwElemSize == 2)
      ((WORD*)pabDecompTo)[*pdwCurElem] = (WORD)dwVal;
   else
      ((BYTE*)pabDecompTo)[*pdwCurElem] = (BYTE)dwVal;
   (*pdwCurElem)++;
}

/*******************************************************************************
PatternDecode - Decodes the encoding from PatternEncode

inputs
   PBYTE          pEnc - Pattern encoded
   DWORD          dwSize - Number of bytes left in all the memory
   DWORD          dwElemSize - Element size. 1 = byte, 2 = word, or 4 = dword
   PCMem          pMem - Filled with the decompressed data. m_dwCurPosn = dwNumEelem * dwElemSize
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD PatternDecode (PBYTE pEnc, size_t dwSize, DWORD dwElemSize,
                     PCMem pMem, size_t *pdwUsed)
{
   *pdwUsed = 0;
   size_t dwOrigSize = dwSize;
   pMem->m_dwCurPosn = 0;

   // pull out the header
   PPATTERNHDR pph;
   if (dwSize < sizeof(PATTERNHDR))
      return 2;
   pph = (PPATTERNHDR) pEnc;
   pEnc += sizeof(PATTERNHDR);
   dwSize -= sizeof(PATTERNHDR);

   // other values
   DWORD dwNeed;
   dwNeed = pph->dwNumNew * sizeof(DWORD) + pph->dwConvert * sizeof(DWORD)*2;
   if (dwNeed > dwSize)
      return 2;   // error
   *pdwUsed = dwNeed + sizeof(PATTERNHDR);
   DWORD *padwCompress, *padwConvert;
   padwCompress = (DWORD*) (pph+1);
   padwConvert = padwCompress + pph->dwNumNew;

   // reorder
   CMem memOrder;
   if (!memOrder.Required (pph->dwConvert * sizeof(DWORD)*2))
      return 2;
   DWORD *padwReorder;
   padwReorder = (DWORD*) memOrder.p;
   DWORD i;
   for (i = 0; i < pph->dwConvert; i++) {
      padwReorder[i*2] = padwConvert[i];
      padwReorder[i*2+1] = padwConvert[i+pph->dwConvert];
   }
   padwConvert = padwReorder;

   // rebuild the conversion
   BOOL fShowOrig;
   for (i = 0; i < pph->dwConvert; i++) {
      if (i) {
         fShowOrig = (padwConvert[i*2] != 0);
            // if no change then also looping down on 2nd case, so show delta there
         padwConvert[i*2] = padwConvert[(i-1)*2] - padwConvert[i*2];
      }
      else {
         // no change to padwConvert
         fShowOrig = TRUE;
      }

      if (!fShowOrig)
         padwConvert[i*2+1] = padwConvert[(i-1)*2+1] - padwConvert[i*2+1];
   }

   // make sure enoiugh memory
   if (!pMem->Required (pMem->m_dwCurPosn + pph->dwNumOrig * dwElemSize))
      return 2;
   PBYTE pDecompTo;
   pDecompTo = (PBYTE) pMem->p + pMem->m_dwCurPosn;
   pMem->m_dwCurPosn += pph->dwNumOrig * dwElemSize;

   // loop
   DWORD dwCurElem;
   dwCurElem = 0;
   for (i = 0; i < pph->dwNumNew; i++) {
      PatternDecodeConvert (padwCompress[i], pph->dwRemapIndex, padwConvert,
         pDecompTo, dwElemSize, &dwCurElem);
   }

   return 0;
}




/*******************************************************************************
PatternEncodeGroup - Encodes a sequence by finding repeating pairs of elements and
combing them together. Repeat until no more repeats. This is a superset of PatternEncode
because it divides these into chunks (of 50,000 units) and encodes each chunk by
itself. This prevents the compression from going non-linear.

inputs
   PVOID          pOrigMem - Memory to encode
   DWORD          dwNum - Number of elements
   DWORD          dwElemSize - Element size (in bytes, either 1, 2, or 4)
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
   PCProgressSocket     pProgress - Progress bar
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD PatternEncodeGroup (PBYTE pOrigMem, size_t dwNum, size_t dwElemSize, PCMem pMem, PCProgressSocket pProgress)
{
   // write the total number of elements and the encoding along with the size of each block
   DWORD dwBlockSize = 50000; // seems like a reasonable number to encode
   DWORD dwSize = 3 * sizeof(DWORD);
   if (!pMem->Required (pMem->m_dwCurPosn + dwSize))
      return 1;
   DWORD *pdwWrite = (DWORD*)((PBYTE)pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwSize;
   pdwWrite[0] = PATENCODEGROUPHDR;
   pdwWrite[1] = (DWORD)dwNum;
   pdwWrite[2] = dwBlockSize;

   // encode while have memory left
   DWORD dwCur;
   for (dwCur = 0; dwCur < dwNum; dwCur += dwBlockSize) {
      if (pProgress)
         pProgress->Push ((fp)dwCur / (fp)dwNum, (fp)min(dwCur+dwBlockSize,dwNum) / (fp)dwNum);

      if (PatternEncode (pOrigMem + dwCur * dwElemSize, min(dwNum-dwCur,dwBlockSize), dwElemSize, pMem, pProgress)) {
         if (pProgress)
            pProgress->Pop();
         return 1;
      }

      if (pProgress)
         pProgress->Pop();
   } // while

   return 0;
}

/*******************************************************************************
PatternDecodeGroup - Decodes the encoding from PatternEncodeGroup

inputs
   PBYTE          pEnc - Pattern encoded
   DWORD          dwSize - Number of bytes left in all the memory
   DWORD          dwElemSize - Element size. 1 = byte, 2 = word, or 4 = dword
   PCMem          pMem - Filled with the decompressed data. m_dwCurPosn = dwNumEelem * dwElemSize
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD PatternDecodeGroup (PBYTE pEnc, size_t dwSize, DWORD dwElemSize,
                     PCMem pMem, size_t *pdwUsed)
{
   *pdwUsed = 0;
   pMem->m_dwCurPosn = 0;

   DWORD dwHeaderSize = 3 * sizeof(DWORD);
   if (dwSize < dwHeaderSize)
      return 2;
   DWORD *padwHeader = (DWORD*)pEnc;

   // for backwards compatability, if dont find the magic header number then
   // use single pattern decode
   if (padwHeader[0] != PATENCODEGROUPHDR)
      return PatternDecode (pEnc, dwSize, dwElemSize, pMem, pdwUsed);

   DWORD dwNum = padwHeader[1];
   DWORD dwBlockSize = padwHeader[2];
   (*pdwUsed) += dwHeaderSize;
   pEnc += dwHeaderSize;
   dwSize -= dwHeaderSize;

   if (!pMem->Required (dwNum * dwElemSize))
      return 2; // error
   
   // loop through all the blocks
   CMem memDecode;
   DWORD dwRet;
   size_t dwCur, dwUsed;
   for (dwCur = 0; dwCur < dwNum; dwCur += dwBlockSize) {
      memDecode.m_dwCurPosn = 0;
      dwRet = PatternDecode (pEnc, dwSize, dwElemSize, &memDecode, &dwUsed);
      if (dwRet)
         return dwRet;

      // copy over
      if (!pMem->Required (pMem->m_dwCurPosn + memDecode.m_dwCurPosn))
         return 2;
      memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, memDecode.p, memDecode.m_dwCurPosn);
      pMem->m_dwCurPosn += memDecode.m_dwCurPosn;

      // advance
      pEnc += dwUsed;
      dwSize -= dwUsed;
      (*pdwUsed) += dwUsed;
   } // dwCur

   return 0;
}

/*******************************************************************************
DWORDTree - Given a value, finds an index into the tree. If it doesn't exist
then add

inputs
   DWORD          dwVal - Value
   PCListFixed    pList - List of RETOK
returns
   DWORD - Found. -1 if error

*/
DWORD DWORDTree (DWORD dwVal, PCListFixed pList)
{
   PRETOK pTok = (PRETOK) pList->Get(0);
   DWORD dwNum = pList->Num();

   DWORD dwCur = 0;
   DWORD *pdwCur = &dwCur;

   while (*pdwCur || ((pdwCur == &dwCur) && dwNum )) {
      PRETOK pCur = pTok + (*pdwCur);
      int iRet;
      if (dwVal > pCur->dwValue)
         iRet = 1;
      else if (dwVal < pCur->dwValue)
         iRet = -1;
      else {   // same
         // found it
         pCur->dwTimesUsed++;
         return (*pdwCur);
      }
   
      // left or right?
      pdwCur = (iRet < 0) ? &pCur->dwLeft : &pCur->dwRight;
   }

   // if get here, haven't found it

   // link old pointer in to the new location
   (*pdwCur) = dwNum;

   RETOK tk;
   memset (&tk, 0, sizeof(tk));
   tk.dwTimesUsed = 1;
   tk.dwValue = dwVal;
   pList->Add (&tk);

   return dwNum;
}

/*******************************************************************************
ReorderEncode - Creates a list of all the unique IDs and the number of occurances
of each. These are then reordered by the number of occurances (actually, the number
of bytes it would take if it were compressed using Huffman compress) and the previous
number.

inputs
   PVOID          pOrigMem - Memory to encode
   DWORD          dwNum - Number of elements
   DWORD          dwElemSize - Element size (in bytes, either 2, or 4). No point encoding 1
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
returns
   DWORD - 0 if OK, 1 if error
*/
static int _cdecl ReorderSort1 (const void *elem1, const void *elem2)
{
   PRETOK p1, p2;
   p1 = *((PRETOK*) elem1);
   p2 = *((PRETOK*) elem2);


   // reorder by most common
   if (p1->dwTimesUsed > p2->dwTimesUsed)
      return -1;
   else if (p1->dwTimesUsed < p2->dwTimesUsed)
      return 1;

   // else, by delta
   if (p1->dwValue > p2->dwValue)
      return 1;
   else if (p1->dwValue < p2->dwValue)
      return -1;
   else
      return 0;
}


static int _cdecl ReorderSort2 (const void *elem1, const void *elem2)
{
   PRETOK p1, p2;
   p1 = *((PRETOK*) elem1);
   p2 = *((PRETOK*) elem2);

   // else, by delta
   if (p1->dwValue > p2->dwValue)
      return 1;
   else if (p1->dwValue < p2->dwValue)
      return -1;
   else
      return 0;
}

DWORD ReorderEncode (PBYTE pOrigMem, DWORD dwNum, DWORD dwElemSize, PCMem pMem)
{
   // first of all, create a tree and figure out number of occurances
   DWORD i;
   DWORD dwVal;
   DWORD *padw = (DWORD*) pOrigMem;
   WORD *paw = (WORD*) pOrigMem;
   CListFixed lTree;
   lTree.Init (sizeof(RETOK));
   for (i = 0; i < dwNum; i++) {
      // get the value
      if (dwElemSize == 4)
         dwVal = padw[i];
      else
         dwVal = paw[i];

      DWORDTree (dwVal, &lTree);
   }  // to figure out number of entries

   // create a list of remaps
   CListFixed lRemap;
   lRemap.Init (sizeof(PRETOK));
   lRemap.Required (lTree.Num());
   for (i = 0; i < lTree.Num(); i++) {
      PRETOK pr = (PRETOK) lTree.Get(i);
      lRemap.Add (&pr);
   }

   // now go through and sort by the most common
   qsort (lRemap.Get(0), lRemap.Num(), sizeof(PRETOK), ReorderSort1);

   // Go back and resort them in groups, according to how they're going to
   // be huffman coded so that will (as much as possible) have increasing
   // least. That way, when the list of remaps is huffman coded it will be fairly
   // small
   for (i = 0; i < lRemap.Num(); i = (i << 7) + (1<<7)) {
      qsort (lRemap.Get(i), min(lRemap.Num(), (i<<7)+(1<<7)) - i, sizeof(PRETOK), ReorderSort2);
   }

   // go through the list again and note what gets remapped to what
   for (i = 0; i < lRemap.Num(); i++) {
      PRETOK pr = *((PRETOK*) lRemap.Get(i));
      pr->dwRemappedTo = i;
   }

   // make sure enough memory
   DWORD dwNeed;
   dwNeed = (1 + lRemap.Num() + dwNum) * dwElemSize + sizeof(DWORD);
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 1;
   DWORD *padwComp;
   WORD *pawComp;
   padwComp = (DWORD*) ((PBYTE) pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   // write out the number of points
   *padwComp = dwNum;
   padwComp++;
   pawComp = (WORD*) padwComp;

   // write out the number of remaps
   if (dwElemSize == 4)
      *(padwComp++) = lRemap.Num();
   else
      *(pawComp++) = (WORD)lRemap.Num();

   // write out all the points
   for (i = 0; i < dwNum; i++) {
      if (dwElemSize == 4)
         dwVal = padw[i];
      else
         dwVal = paw[i];

      dwVal = DWORDTree (dwVal, &lTree);
      PRETOK pr = (PRETOK) lTree.Get (dwVal);

      if (dwElemSize == 4)
         *(padwComp++) = pr->dwRemappedTo;
      else
         *(pawComp++) = (WORD)pr->dwRemappedTo;
   }

   // write out the remaps
   DWORD dwLastRemap;
   dwLastRemap = 0;
   for (i = 0; i < lRemap.Num(); i++) {
      PRETOK pr = *((PRETOK*) lRemap.Get(i));
      dwVal = pr->dwValue;

      // NOTE: Subtracting the previous. Ultimately, storing deltas, which will work
      // well for the huffman coding since the deletas are alsmot always postiive, and
      // will result in low valued numbers

      if (dwElemSize == 4)
         *(padwComp++) = dwVal - dwLastRemap;
      else
         *(pawComp++) = (WORD)dwVal - (WORD)dwLastRemap;
      dwLastRemap = dwVal;
   }


   return 0;
}

/*******************************************************************************
ReorderDecode - Decodes the encoding from ReorderEncode

inputs
   PBYTE          pHuff - Reorder encoded
   DWORD          dwSize - Number of bytes left in all the memory
   DWORD          dwElemSize - Element size. 2 = word, or 4 = dword
   PCMem          pMem - Filled with the elements. pMem->m_dwCurPosn set to #elems x dwElemSize
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD ReorderDecode (PBYTE pHuff, DWORD dwSize, DWORD dwElemSize,
                     PCMem pMem, DWORD *pdwUsed)
{
   *pdwUsed = 0;
   DWORD dwOrigSize = dwSize;
   pMem->m_dwCurPosn = 0;

   // get the first DWORD to indicate how many elem
   DWORD dwNum;
   if (dwSize < sizeof(DWORD))
      return 2;
   dwNum = *((DWORD*) pHuff);
   pHuff += sizeof(DWORD);
   dwSize -= sizeof(DWORD);
   if (!pMem->Required (dwNum * dwElemSize))
      return 2;
   DWORD *padwFill;
   WORD *pawFill;
   padwFill = (DWORD*) ((PBYTE) pMem->p + pMem->m_dwCurPosn);
   pawFill = (WORD*) padwFill;
   pMem->m_dwCurPosn += dwNum * dwElemSize;

   // get the next word (or dword) to find out how many remaps
   DWORD dwRemaps;
   if (dwSize < dwElemSize)
      return 2;
   if (dwElemSize == 4)
      dwRemaps = *((DWORD*) pHuff);
   else
      dwRemaps = *((WORD*) pHuff);
   pHuff += dwElemSize;
   dwSize -= dwElemSize;

   // make sure large enough
   if ((dwNum + dwRemaps) * dwElemSize > dwSize)
      return 2;   // not right size
   *pdwUsed = dwOrigSize - dwSize + (dwNum + dwRemaps) * dwElemSize;

   // location for the data
   DWORD i;
   DWORD *padw;
   WORD *paw;
   padw = (DWORD*) pHuff;
   paw = (WORD*) pHuff;

   // Need to delta-decode the remap
   CListFixed lRemap;
   lRemap.Init (dwElemSize, (dwElemSize == 4) ? (PVOID)(padw+dwNum) : (PVOID)(paw+dwNum), dwRemaps);
   DWORD *padwRemap;
   WORD *pawRemap;
   padwRemap = (DWORD*) lRemap.Get(0);
   pawRemap = (WORD*) padwRemap;
   if (dwElemSize == 4) {
      for (i = 1; i < dwRemaps; i++)
         padwRemap[i] += padwRemap[i-1];
   }
   else {
      for (i = 1; i < dwRemaps; i++)
         pawRemap[i] += pawRemap[i-1];
   }

   // get them
   for (i = 0; i < dwNum; i++) {
#ifdef _DEBUG
      if (dwElemSize == 4) {
         if (padw[i] >= dwRemaps)
            i = 1000000;
      }
      else {
         if ((DWORD) paw[i] >= dwRemaps)
            i = 1000000;
      }
#endif
      if (dwElemSize == 4)
         padwFill[i] = padwRemap[padw[i]];
      else
         pawFill[i] = pawRemap[paw[i]];
   }

   return 0;
}



/*******************************************************************************
HuffmanEncode - Does a simple huffman encoding by splitting the number into
7-bit slices and using the high bit of a byte to indicate if there's more. 0x00 =
no more data, 0x80 = more data

inputs
   PVOID          pOrigMem - Memory to encode
   DWORD          dwNum - Number of elements
   DWORD          dwElemSize - Element size (in bytes, either 2, or 4, or 8). No point encoding 1
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD HuffmanEncode (PBYTE pOrigMem, size_t dwNum, size_t dwElemSize, PCMem pMem)
{
   // write out the number of types
   size_t dwSizeIndex = pMem->m_dwCurPosn;
   if (!pMem->Required(pMem->m_dwCurPosn + sizeof(DWORD)))
      return 1;
   pMem->m_dwCurPosn += sizeof(DWORD);

   // loop
   DWORD dwVal, i;
   PBYTE pCur;
   for (i = 0; i < dwNum; i++, pOrigMem += dwElemSize) {
      // make sure at least have 5 bytes
      if (!pMem->Required(pMem->m_dwCurPosn + 15))
         return 1;
      pCur = (PBYTE) pMem->p + pMem->m_dwCurPosn;

      if (dwElemSize == 8) {
         unsigned __int64 qw;
         qw = *((unsigned __int64 *) pOrigMem);

         // NOTE: being laszy here and not subtracting

         DWORD dwNumBytes, j;
         for (dwNumBytes = 0; !dwNumBytes || qw; qw >>= 7, dwNumBytes++)
            pCur[dwNumBytes] = ((BYTE) qw & 0x7f) | (dwNumBytes ? 0 : 0x80);

         // flip
         BYTE bTemp;
         for (j = 0; j < dwNumBytes/2; j++) {
            bTemp = pCur[j];
            pCur[j] = pCur[dwNumBytes-j-1];
            pCur[dwNumBytes-j-1] = bTemp;
         }
         pCur += dwNumBytes;
         pMem->m_dwCurPosn += dwNumBytes;
      }
      else {
         if (dwElemSize == 4)
            dwVal = *((DWORD*) pOrigMem);
         else
            dwVal = *((WORD*) pOrigMem);

         // write out
         if (dwVal < (1<<7)) {
            *pCur = dwVal | 0x80;
            pMem->m_dwCurPosn += 1;
         }
         else if (dwVal < (1<<7) + (1<<14)) {
            dwVal -= (1 << 7);
            *(pCur++) = (dwVal >> 7) & 0x7f;
            *pCur = (dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 2;
         }
         else if (dwVal < (1<<7) + (1<<14) + (1 << 21)) {
            dwVal -= ((1 << 7) + (1<<14));
            *(pCur++) = (dwVal >> 14) & 0x7f;
            *(pCur++) = (dwVal >> 7) & 0x7f;
            *pCur = (dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 3;
         }
         else if (dwVal < (1<<7) + (1<<14) + (1 << 21) + (1 << 28)) {
            dwVal -= ((1 << 7) + (1<<14) + (1<<21));
            *(pCur++) = (dwVal >> 21) & 0x7f;
            *(pCur++) = (dwVal >> 14) & 0x7f;
            *(pCur++) = (dwVal >> 7) & 0x7f;
            *pCur = (dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 4;
         }
         else {   // all 5 bytes
            dwVal -= ((1 << 7) + (1<<14) + (1<<21) + (1 << 28));
            *(pCur++) = (dwVal >> 28) & 0x7f;
            *(pCur++) = (dwVal >> 21) & 0x7f;
            *(pCur++) = (dwVal >> 14) & 0x7f;
            *(pCur++) = (dwVal >> 7) & 0x7f;
            *pCur = (dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 5;
         }
      }  // 2 or 4 byte
   }

   // fill in the size
   DWORD *pdw;
   pdw = (DWORD*) ((BYTE*) pMem->p + dwSizeIndex);
   *pdw = (DWORD)dwNum;  //pMem->m_dwCurPosn - (dwSizeIndex + sizeof(DWORD));

   return 0;
}

/*******************************************************************************
HuffmanDecode - Decodes the encoding from above

inputs
   PBYTE          pHuff - Huffman encoded
   DWORD          dwSize - Number of bytes left in all the memory
   DWORD          dwElemSize - Element size. 2 = word, or 4 = dword, 8=qword
   PCMem          pMem - Filled with the elements. pMem->m_dwCurPosn set to #elems x dwElemSize
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD HuffmanDecode (PBYTE pHuff, size_t dwSize, size_t dwElemSize,
                     PCMem pMem, size_t *pdwUsed)
{
   *pdwUsed = 0;
   size_t dwOrigSize = dwSize;
   pMem->m_dwCurPosn = 0;

   // get the first DWORD to indicate how many elem
   DWORD dwNum;
   if (dwSize < sizeof(DWORD))
      return 2;
   dwNum = *((DWORD*) pHuff);
   pHuff += sizeof(DWORD);
   dwSize -= sizeof(DWORD);
   if (!pMem->Required (dwNum * dwElemSize))
      return 2;
   pMem->m_dwCurPosn = dwNum * dwElemSize;

   // fill it in
   DWORD i, dwVal, dwOffset;
   unsigned __int64 qwVal;
   for (i = 0; i < dwNum; i++) {
      if (dwElemSize == 8) {
         qwVal = 0;

         while (TRUE) {
            if (!dwSize)
               return 2;   // error
            qwVal = (qwVal << 7) | (pHuff[0] & 0x7f);
            if (pHuff[0] & 0x80) {
               pHuff++;
               dwSize--;
               break;
            }

            pHuff++;
            dwSize--;
         }

         // write the value out
         ((unsigned __int64*) pMem->p)[i] = qwVal;
      }
      else {   // elem size is 4 or 2
         // get bytes until have high bit
         dwVal = dwOffset = 0;
         while (TRUE) {
            if (!dwSize)
               return 2;   // error
            dwVal = (dwVal << 7) | (pHuff[0] & 0x7f);
            if (pHuff[0] & 0x80) {
               pHuff++;
               dwSize--;
               dwVal += dwOffset;
               break;
            }

            pHuff++;
            dwSize--;
            dwOffset = (dwOffset << 7) + (1 << 7);
         }

         // write the value out
         if (dwElemSize == 4)
            ((DWORD*) pMem->p)[i] = dwVal;
         else
            ((WORD*) pMem->p)[i] = (WORD) dwVal;
      }
   }

   // done
   *pdwUsed = dwOrigSize - dwSize;
   return 0;
}


/*******************************************************************************
TokenAdd - Given data (that could points to a string in a MML node (and this
string must be valid for some time)), if this exists in the tree then the token
index for the tree is used. If it doesn't exist in the tree then it's added to the
tree and the new index is returned.

inputs
   PVOID       pData - Data pointed to. If this is NULL an empty string is used.
   DWORD       dwSize - Size of the data (only used for binary)
   DWORD       dwType - Of MMNA_XXX type
   PCListFixed pList - List of MMLTOK structures
returns
   DWORD - Token index, from 0+. or -1 if error
*/
size_t TokenAdd (PVOID pData, size_t dwSize, DWORD dwType, PCListFixed pList)
{
   if (!pData) {
      pData = L"";  // should be a harmless conversion
      dwType = MMNA_STRING;
   }

   PMMLTOK pTok = (PMMLTOK) pList->Get(0);
   DWORD dwNum = pList->Num();

   DWORD dwCur = 0;
   DWORD *pdwCur = &dwCur;

   while (*pdwCur || ((pdwCur == &dwCur) && dwNum )) {
      PMMLTOK pCur = pTok + (*pdwCur);
      int iRet;

      if (pCur->dwTokenType < dwType)
         iRet = -1;
      else if (pCur->dwTokenType > dwType)
         iRet = 1;
      else {
         switch (dwType) {
         case TT_BINARY:
            if (pCur->iValue < (int)dwSize)
               iRet = -1;
            else if (pCur->iValue > (int)dwSize)
               iRet = 1;
            else
               iRet = memcmp (pData, pCur->pBinary, dwSize);
            break;

         case TT_INT:
            {
               int iVal = *((int*)pData);
               if (pCur->iValue < iVal)
                  iRet = -1;
               else if (pCur->iValue > iVal)
                  iRet = 1;
               else
                  iRet = 0;
            }
            break;
         case TT_FLOAT:
            {
               double dVal = *((double*)pData);
               float fVal = (float)dVal;
               if (pCur->fValue < fVal)
                  iRet = -1;
               else if (pCur->fValue > fVal)
                  iRet = 1;
               else
                  iRet = 0;
            }
            break;

         default:
         //case TT_STRING:
         //case TT_BUILTIN:  // shouldn get
         //case TT_COMMONBUILTIN: // shouldn get
            iRet = wcscmp ((PWSTR)pData, pCur->pszString);
            break;
         } // switch
      }

      // if exact match done
      if (iRet == 0) {
         // found it
         pCur->dwTimesUsed++;
         return (*pdwCur);
      }
      
      // left or right?
      pdwCur = (iRet < 0) ? &pCur->dwLeft : &pCur->dwRight;
   }

   // if get here, haven't found it

   (*pdwCur) = dwNum;

   MMLTOK tk;
   memset (&tk, 0, sizeof(tk));
   tk.dwTokenType = dwType;
   tk.dwTimesUsed = 1;
   switch (dwType) {
   case TT_BINARY:
      tk.pBinary = pData;
      tk.iValue = (int)dwSize;
      break;
   case TT_INT:
      tk.iValue = *((int*)pData);
      break;
   case TT_FLOAT:
      tk.fValue = (fp) *((double*)pData);
      break;
   default:
      tk.pszString = (PWSTR)pData;
      break;
   }
   pList->Add (&tk);

   return dwNum;
}


/*******************************************************************************
TokenizeNode - Tokenizes the node and sub-nodes.

inputs
   PCMMLNode2      pNode - node
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
   PCListFixed    plTokens - List of tokens to add to or use
   PCListVariable pListTemp - BUGBUG - temporary for storing strings until get full info in
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD TokenizeNode (PCMMLNode2 pNode, PCMem pMem, PCListFixed plTokens, PCListVariable pListTemp)
{
   // NOTE: Because of the way doing this, don't write pNode's name here, since
   // write all the names in a sequence

   // how much required
   DWORD dwRequired, *pdw;
   dwRequired = (1 + (pNode->ContentNum() ? 1 : 0) + pNode->AttribNum() * 2 +
      pNode->ContentNum()) * sizeof(DWORD);
   if (!pMem->Required (pMem->m_dwCurPosn + dwRequired))
      return 1;
   pdw = (DWORD*) (((BYTE*) pMem->p) + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwRequired;

   // leading DWORD indicating the number of attributes, and whether this has
   // contents or not. Basically, (# attributes) << 1 | (contents ? 1 : 0)
   DWORD dwHeader;
   dwHeader = (pNode->AttribNum() << 1);
   if (pNode->ContentNum())
      dwHeader |= 1;
   *(pdw++) = dwHeader;

   // write out number of contents, IF there are any
   dwHeader = pNode->ContentNum();
   if (dwHeader) {
      *(pdw++) = dwHeader;
   }

   // write out all the attribute names first, then all the values
   DWORD i, dwNum;
   PWSTR pszAttrib;
   PVOID pValue;
   DWORD dwType;
   size_t dwSize;
   dwNum = pNode->AttribNum();
   for (i = 0; i < dwNum; i++) {
      pszAttrib = NULL;
      pValue = NULL;
      pNode->AttribEnumNoConvert (i, &pszAttrib, &pValue, &dwType, &dwSize);

      // write out
      pdw[i] = (DWORD)TokenAdd (pszAttrib, NULL, MMNA_STRING, plTokens);
      pdw[i+dwNum] = (DWORD)TokenAdd (pValue, dwSize, dwType, plTokens);
      if ((pdw[i] == -1) || (pdw[i+dwNum] == -1))
         return 1;
   }
   pdw += (dwNum*2);

   // write out all the contents. DWORD for the string's token, either psz or pSub->NameGet()
   // NOTE: << 1. Low bit is set if it's a pSub, in which case will also follow with
   // a recursive call into TokenizeNode()
   PWSTR psz;
   PCMMLNode2 pSub;
   dwNum = pNode->ContentNum();
   for (i = 0; i < dwNum; i++) {
      psz = NULL;
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);

      pdw[i] = (DWORD)TokenAdd (psz ? psz : pSub->NameGet(), 0, MMNA_STRING, plTokens);
      if (pdw[i] == -1)
         return 1;
      pdw[i] = (pdw[i] << 1) | (psz ? 0 : 1);
   }

   // do all the tokens
   for (i = 0; i < dwNum; i++) {
      psz = NULL;
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (psz)
         continue;

      if (TokenizeNode (pSub, pMem, plTokens, pListTemp))
         return 1;
   }

   // done
   return 0;
}

/*******************************************************************************
TokenGet - Given a toekn number, and plTokens, this returns a pointer to the
string or data. It also looks in the built-in strings.

inputs
   DWORD          dwToken - token to use
   PCListFixed    plTokensType - List of TOKENTYPE correlating to plTokens
   PCListVariable plTokens - Tokns list
   DWORD          *pdwType - Filled with the type of data, MMNA_XXX
   size_t          *pdwSize - Filled with the size of data (only relevent for binary)
returns
   PVOID - Data located at the token, or NULL if error
*/
PWSTR TokenGet (DWORD dwToken, PCListFixed plTokensType, PCListVariable plTokens,
                DWORD *pdwType, size_t *pdwSize)
{
   DWORD dwNum = plTokens->Num();
   if (dwToken < dwNum) {
      PTOKENTYPE ptt = (PTOKENTYPE) plTokensType->Get(dwToken);
      *pdwType = ptt->dwType;
      if (ptt->dwType == MMNA_BINARY)
         *pdwSize = plTokens->Size(dwToken);
      else
         *pdwSize = 0;
      return (PWSTR) plTokens->Get(dwToken);
   }

   // else, need to increase
   dwToken -= dwNum;
   PWSTR psz;
   BuiltInRead();
   psz = gTreeBuiltIn.Enum (dwToken);
   *pdwType = MMNA_STRING;
   *pdwSize = 0;
   if (psz)
      return psz;
   else
      return L"";
}
   

/*******************************************************************************
DeTokenizeNode - Given memory containing the tokenized MML, and a list of the the
token strings, this detokenizes it.

inputs
   DWORD          *padwTokens - Memory containg a list of tokens
   DWORD          dwNum - Number of tokens left
   PCListFixed    plTokensType - List of TOKENTYPE for each plTokens
   PCListVariable plTokens - List of what the tokens mean. Each item is a wide string
   DWORD          *pdwUsed - Filled with number of tokens used
   PCMMLNode2      *ppNode - Filled with the node created by the tokens
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD DeTokenizeNode (DWORD *padwTokens, size_t dwNum, PCListFixed plTokensType, PCListVariable plTokens,
                      size_t *pdwUsed, PCMMLNode2 *ppNode)
{
   *pdwUsed = 0;
   *ppNode = 0;
   size_t dwOrigNum = dwNum;

   // header - which is the number of attributes << 1, ored with 1 if has contents
   if (!dwNum)
      goto error;
   DWORD dwNumAttrib, dwNumContent;
   BOOL fContent;
   dwNumAttrib = padwTokens[0] >> 1;
   dwNumContent = 0;
   fContent = (padwTokens[0] & 0x01) ? TRUE : FALSE;
   dwNum--;
   padwTokens++;

   if (fContent) {
      if (!dwNum)
         goto error;
      dwNumContent = padwTokens[0];
      dwNum--;
      padwTokens++;
   }

   // make sure enough
   DWORD dwRequired;
   dwRequired = dwNumAttrib*2 + dwNumContent;
   if (dwNum < dwRequired)
      goto error;

   // create new node
   PCMMLNode2 pNode;
   pNode = *ppNode = new CMMLNode2;
   if (!pNode)
      goto error;

   // attributes
   DWORD i;
   DWORD dwType;
   size_t dwSize;
   for (i = 0; i < dwNumAttrib; i++) {
      PWSTR pszAttrib = TokenGet(padwTokens[i], plTokensType, plTokens, &dwType, &dwSize);
      if (dwType != MMNA_STRING)
         goto error; // shouldnt happen

      // value
      PVOID pData = TokenGet (padwTokens[i+dwNumAttrib], plTokensType, plTokens, &dwType, &dwSize);
      switch (dwType) {
      case MMNA_STRING:
         if (!pNode->AttribSetString (pszAttrib, (PWSTR)pData))
            goto error;
         break;
      case MMNA_BINARY:
         if (!pNode->AttribSetBinary (pszAttrib, pData, dwSize))
            goto error;
         break;
      case MMNA_INT:
         if (!pNode->AttribSetInt (pszAttrib, *((int*)pData) ))
            goto error;
         break;
      case MMNA_DOUBLE:
         if (!pNode->AttribSetDouble (pszAttrib, *((double*)pData) ))
            goto error;
         break;
      default:
         goto error; // shouldnt happen
      } // swithc
   }
   dwNum -= dwNumAttrib*2;
   padwTokens += (dwNumAttrib*2);

   // content
   size_t dwNumAtContent;
   DWORD *pdwCurContent;
   DWORD dwVal;
   pdwCurContent = padwTokens + dwNumContent;
   dwNumAtContent = dwNum - dwNumContent;
   for (i = 0; i < dwNumContent; i++) {
      dwVal = padwTokens[i];

      // if 1 bit is set then recurse on that
      if (dwVal & 0x01) {  // content with subnode
         DWORD dwRet;
         size_t dwUsed;
         PCMMLNode2 pSub;
         dwRet = DeTokenizeNode (pdwCurContent, dwNumAtContent, plTokensType, plTokens, &dwUsed, &pSub);
         if (dwRet || !pSub) {
            if (*ppNode)
               delete *ppNode;
            *ppNode = NULL;
            return dwRet;
         }
         pdwCurContent += dwUsed;
         dwNumAtContent -= dwUsed;

         // set the name
         PWSTR pszName = (PWSTR) TokenGet (dwVal >> 1, plTokensType, plTokens, &dwType, &dwSize);
         if (!pszName || (dwType != MMNA_STRING)) {
            delete pSub;
            goto error;
         }
         pSub->NameSet (pszName);
         pNode->ContentAdd (pSub);
      }
      else {   // sjust string
         PWSTR pszName = (PWSTR) TokenGet (dwVal >> 1, plTokensType, plTokens, &dwType, &dwSize);
         if (!pszName)
            goto error;

         switch (dwType) {
         case MMNA_STRING:
         case TT_BUILTIN:
         case TT_COMMONBUILTIN:
            pNode->ContentAdd (pszName);
            break;
         case MMNA_BINARY:
            {
               CMem mem;
               if (!mem.Required ((dwSize*2+1)*sizeof(WCHAR)))
                  goto error;
               MMLBinaryToString ((PBYTE) pszName, dwSize, (PWSTR)mem.p);
               pNode->ContentAdd ((PWSTR)mem.p);
            }
            break;
         case MMNA_INT:
            {
               WCHAR szTemp[32];
               swprintf (szTemp, L"%d", *((int*)pszName));
               pNode->ContentAdd (szTemp);
            }
            break;
         case MMNA_DOUBLE:
            {
               WCHAR szTemp[32];
               MMLDoubleToString (*((double*)pszName), szTemp);
               pNode->ContentAdd (szTemp);
            }
            break;
         default:
            goto error;
         }  // swithc
      } // if just string

   }

   *pdwUsed = dwOrigNum - dwNumAtContent;

   return 0;   // success

error:
   if (*ppNode)
      delete *ppNode;
   *ppNode = NULL;
   return 2;
}



/*******************************************************************************
RemapTokenNode - Given memory containing the tokenized MML, and a list of the the
token strings, this remaps the token node using PMMLTOK->dwMapTo

inputs
   DWORD          *padwTokens - Memory containg a list of tokens
   DWORD          dwNum - Number of tokens left
   PMMLTOK        pTok - List of tokens, with dwMapTo set
   DWORD          *pdwUsed - Filled with number of tokens used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD RemapTokenNode (DWORD *padwTokens, size_t dwNum, PMMLTOK pTok,
                      size_t *pdwUsed)
{
   *pdwUsed = 0;
   size_t dwOrigNum = dwNum;

   // header - which is the number of attributes << 1, ored with 1 if has contents
   if (!dwNum)
      goto error;
   DWORD dwNumAttrib, dwNumContent;
   BOOL fContent;
   dwNumAttrib = padwTokens[0] >> 1;
   dwNumContent = 0;
   fContent = (padwTokens[0] & 0x01) ? TRUE : FALSE;
   dwNum--;
   padwTokens++;

   if (fContent) {
      if (!dwNum)
         goto error;
      dwNumContent = padwTokens[0];
      dwNum--;
      padwTokens++;
   }

   // make sure enough
   DWORD dwRequired;
   dwRequired = dwNumAttrib*2 + dwNumContent;
   if (dwNum < dwRequired)
      goto error;

   // attributes
   DWORD i;
   for (i = 0; i < dwNumAttrib; i++) {
      padwTokens[i] = pTok[padwTokens[i]].dwMapTo;
      padwTokens[i+dwNumAttrib] = pTok[padwTokens[i+dwNumAttrib]].dwMapTo;
   }
   dwNum -= dwNumAttrib*2;
   padwTokens += (dwNumAttrib*2);

   // content
   size_t dwNumAtContent;
   DWORD *pdwCurContent;
   DWORD dwVal;
   pdwCurContent = padwTokens + dwNumContent;
   dwNumAtContent = dwNum - dwNumContent;
   for (i = 0; i < dwNumContent; i++) {
      dwVal = padwTokens[i];

      // remap this token
      padwTokens[i] = (pTok[dwVal >> 1].dwMapTo) << 1;
      if (dwVal & 0x01)
         padwTokens[i] |= 0x01;

      // if 1 bit is set then recurse on that
      if (dwVal & 0x01) {  // content with subnode
         DWORD dwRet;
         size_t dwUsed;
         dwRet = RemapTokenNode (pdwCurContent, dwNumAtContent, pTok, &dwUsed);
         if (dwRet)
            return dwRet;
         pdwCurContent += dwUsed;
         dwNumAtContent -= dwUsed;
      }

   }

   *pdwUsed = dwOrigNum - dwNumAtContent;

   return 0;   // success

error:
   return 2;
}

/*******************************************************************************
RemapMMLTOKToID - Remaps the MMLTOK list IDs to new values. This is done so that
all numbers are in the same range, while all ANSI strings are in a different range, etc.

This then goes through the padwTokens data (and dwNumTokens) and remaps the token numbers.

inputs
   PMMLTOK        pTok - Pointer to an array of tokens. THe dwMapTo parameter is changed.
   DWORD          dwNum - Number of tokens
   DWORD*         padwTokens - Pointer to memory containing the MML compressed by tokens
   DWORD          dwNumTokens - Number of elements in padwTokens.
   PTMHDR         pth - Filled with a token-map header that can be used later on
returns
   DWORD - 0 if ok
*/

static int _cdecl SortInt (const void *elem1, const void *elem2)
{
   MMLTOK *p1, *p2;
   p1 = *((MMLTOK**) elem1);
   p2 = *((MMLTOK**) elem2);


   if (p1->iValue > p2->iValue)
      return 1;
   else if (p1->iValue < p2->iValue)
      return -1;
   else
      return 0;
}

static int _cdecl SortFloat (const void *elem1, const void *elem2)
{
   MMLTOK *p1, *p2;
   p1 = *((MMLTOK**) elem1);
   p2 = *((MMLTOK**) elem2);


   if (p1->fValue > p2->fValue)
      return 1;
   else if (p1->fValue < p2->fValue)
      return -1;
   else
      return 0;
}

static int _cdecl SortString (const void *elem1, const void *elem2)
{
   MMLTOK *p1, *p2;
   p1 = *((MMLTOK**) elem1);
   p2 = *((MMLTOK**) elem2);


   if (p1->dwTimesUsed > p2->dwTimesUsed)
      return -1;
   else if (p1->dwTimesUsed < p2->dwTimesUsed)
      return 1;
   else
      return 0;
}

static int _cdecl SortByRemap (const void *elem1, const void *elem2)
{
   MMLTOK *p1, *p2;
   p1 = (MMLTOK*) elem1;
   p2 = (MMLTOK*) elem2;


   if (p1->dwMapTo > p2->dwMapTo)
      return 1;
   else if (p1->dwMapTo < p2->dwMapTo)
      return -1;
   else
      return 0;
}


DWORD RemapMMLTOKToID (PMMLTOK pTok, size_t dwNum, DWORD *padwTokens, size_t dwNumTokens,
                       PTMHDR pth)
{
   // maintain lists of the integers, floats, binary, etc.
   CListFixed lInt, lFloat, lBinary, lANSI, lString, lCommon;  // except built-in
   lInt.Init (sizeof(PMMLTOK));
   lFloat.Init (sizeof(PMMLTOK));
   lBinary.Init (sizeof(PMMLTOK));
   lString.Init (sizeof(PMMLTOK));
   lCommon.Init (sizeof(PMMLTOK));

   // loop through all the entries
   DWORD i, dwMaxBuiltIn;
   //WCHAR szTemp[64];
   //char szaTemp[64];
   PWSTR psz;
   PMMLTOK pCur;
   dwMaxBuiltIn = 0; // keep track of the maximum used built-in string
   for (i = 0; i < dwNum; i++) {
      pCur = pTok + i;
      psz = pCur->pszString;

      // is it an integer?
      if (pCur->dwTokenType == TT_INT) {
         // integer result
         lInt.Add (&pCur);
         continue;
      }

      // see if it's a floating point vlaue
      if (pCur->dwTokenType == TT_FLOAT) {
         lFloat.Add (&pCur);
         continue;
      }

      // see if it's a binary
      if (pCur->dwTokenType == TT_BINARY) {
         // binary
         lBinary.Add (&pCur);
         continue;
      }

      // else, must be a string

      // Compare to built in strings
      DWORD dwFind;
      dwFind = BuiltInFind (psz);
      if (dwFind != -1) {
         pCur->dwTokenType = TT_BUILTIN;
         pCur->dwMapTo = dwFind; // will update this later
         dwMaxBuiltIn = max(dwFind, dwMaxBuiltIn);
         lCommon.Add (&pCur);
         continue;
      }

      // if get here it's just a string
      pCur->dwTokenType = TT_STRING;
      lString.Add (&pCur);
   }  // over i

   // save any changes
   BuiltInSave ();

   // sort some of the lists into a specific order
   qsort (lInt.Get(0), lInt.Num(), sizeof(PMMLTOK), SortInt);
   qsort (lFloat.Get(0), lFloat.Num(), sizeof(PMMLTOK), SortFloat);
   qsort (lString.Get(0), lString.Num(), sizeof(PMMLTOK), SortString);
   qsort (lBinary.Get(0), lBinary.Num(), sizeof(PMMLTOK), SortString);  // doing sort string since sort by # occurances
   qsort (lCommon.Get(0), lCommon.Num(), sizeof(PMMLTOK), SortString);  // doing sort string since sort by # occurances

#define MAXCOMMON 128

   // come up with ID starts for each of them
   TMHDR th;
   memset (&th, 0, sizeof(th));
   th.dwVersion = TMVERSION;
   th.dwCommonBuiltInID = 0;
   th.dwStringID = th.dwCommonBuiltInID + min(MAXCOMMON,lCommon.Num());
   th.dwBinaryID = th.dwStringID + lString.Num();
   th.dwIntID = th.dwBinaryID + lBinary.Num();
   th.dwFloatID = th.dwIntID + lInt.Num();
   th.dwBuiltInStringID = th.dwFloatID + lFloat.Num();
   th.dwMaxBuiltIn = dwMaxBuiltIn;

   // loop through them all and set the current tokens
   for (i = 0; i < min(MAXCOMMON,lCommon.Num()); i++) {
      // take the 128 most common built-in strings and store a number for them at the
      // top of the list. This will help the huffman coding later on
      pCur = *((PMMLTOK*) lCommon.Get(i));
      pCur->dwMapTo = th.dwCommonBuiltInID + i;
      pCur->dwTokenType = TT_COMMONBUILTIN;
   }
   for (i = 0; i < lInt.Num(); i++) {
      pCur = *((PMMLTOK*) lInt.Get(i));
      pCur->dwMapTo = th.dwIntID + i;
   }
   for (i = 0; i < lFloat.Num(); i++) {
      pCur = *((PMMLTOK*) lFloat.Get(i));
      pCur->dwMapTo = th.dwFloatID + i;
   }
   for (i = 0; i < lString.Num(); i++) {
      pCur = *((PMMLTOK*) lString.Get(i));
      pCur->dwMapTo = th.dwStringID + i;
   }
   for (i = 0; i < lBinary.Num(); i++) {
      pCur = *((PMMLTOK*) lBinary.Get(i));
      pCur->dwMapTo = th.dwBinaryID + i;
   }
   for (i = 0; i < dwNum; i++) {
      if (pTok[i].dwTokenType != TT_BUILTIN)
         continue;
      pTok[i].dwMapTo += th.dwBuiltInStringID;
   }

   // remap all the token nodes now that have figured out the mapping
   size_t dwUsed;
   DWORD dwRet;
   dwUsed = 0;
   dwRet = RemapTokenNode (padwTokens, dwNumTokens, pTok, &dwUsed);
   if (dwRet)
      return dwRet;

   // now that things have been remapped, make things easier for later and resort
   // all the tokens by their new map ID
   qsort (pTok, dwNum, sizeof(MMLTOK), SortByRemap);
   *pth = th;

   return 0;
}


/*******************************************************************************
TokenStringsToMem - Take a list of MMLTOK that are known to be strings, and serialize
them to memory.

inputs
   PMMLTOK        pTok - Pointer to the start of the list
   DWORD          dwNum - Number
   PCMem          pMem - Memory to append onto. Just adds onto m_dwCurPosn
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD TokenStringsToMem (PMMLTOK pTok, DWORD dwNum, PCMem pMem)
{
   // copy all the strings into a unified memory
   CMem memWCHAR;
   size_t dwNeed;
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      dwNeed = (wcslen(pTok[i].pszString)+1)*2;
      if (!memWCHAR.Required(memWCHAR.m_dwCurPosn + dwNeed))
         return 2;
      memcpy ((PBYTE)memWCHAR.p + memWCHAR.m_dwCurPosn, pTok[i].pszString, dwNeed);
      memWCHAR.m_dwCurPosn += dwNeed;
   }

   // pattern sort these
   CMem memPattern;
   DWORD dwRet;
      // BUGFIX - Using patternencodegroup instead of patternencode
   dwRet = PatternEncodeGroup ((PBYTE) memWCHAR.p, memWCHAR.m_dwCurPosn / sizeof(WCHAR), sizeof(WCHAR),
      &memPattern, NULL);
   if (dwRet)
      return dwRet;

   // huffman encode thse
   CMem memHuff;
   dwRet = HuffmanEncode ((PBYTE) memPattern.p, memPattern.m_dwCurPosn / sizeof(DWORD),
      sizeof(DWORD), &memHuff);
   if (dwRet)
      return dwRet;

   // required
   DWORD *pdw;
   dwNeed = 2 * sizeof(DWORD) + memHuff.m_dwCurPosn;
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 2;
   pdw = (DWORD*) ((PBYTE) pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   // write this out
   *(pdw++) = (DWORD)memHuff.m_dwCurPosn;
   *(pdw++) = dwNum;
   memcpy (pdw, memHuff.p, memHuff.m_dwCurPosn);

   return 0;
}

/*******************************************************************************
TokenStringsFromMem - Given memory containing the output of TokenStringsToMem(),
this decodes it and add the elements sequentiall to plTokens.

inputs
   PBYTE          pTree - Memory containing the tree
   DWORD          dwSize - Number of bytes left in all the memory
   PCListFixed    plTokensType - Filled with the TOKENTYPE structure
   PCListVariable plTokens - Filled in with binary data for each of the tokens
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD TokenStringsFromMem (PBYTE pTree, size_t dwSize, PCListFixed plTokensType, PCListVariable plTokens,
                      size_t *pdwUsed)
{
   // make sure enough space
   if (dwSize < 8)
      return 2;   // cant be this small

   DWORD dwNum, dwHuff;
   DWORD *pdw;
   pdw = (DWORD*) pTree;
   dwHuff = *(pdw++);
   dwNum = *(pdw++);
   dwSize -= 8;
   if (dwSize < dwHuff)
      return 2;   // too small
   *pdwUsed = 2 * sizeof(DWORD) + dwHuff;

   // huffman decode
   CMem memHuff;
   DWORD dwRet;
   size_t dwUsed;
   dwRet = HuffmanDecode ((PBYTE) pdw, dwHuff, sizeof(DWORD), &memHuff, &dwUsed);
   if (dwRet)
      return dwRet;

   // pattern decode
   CMem memPattern;
      // BUGFIX - Using patterndecodegroup instead of patterndecode
   dwRet = PatternDecodeGroup ((PBYTE) memHuff.p, memHuff.m_dwCurPosn, sizeof(WCHAR),
      &memPattern, &dwUsed);
   if (dwRet)
      return dwRet;

   // go through the loop
   PWSTR psz;
   size_t dwNeed;
   DWORD i;
   for (psz = (WCHAR*) memPattern.p, i = 0; i < dwNum; i++) {
      TOKENTYPE tt;
      tt.dwType = MMNA_STRING;
      plTokensType->Add (&tt);

      dwNeed = (wcslen(psz)+1)*2;
      plTokens->Add (psz, dwNeed);

      psz += dwNeed/2;
   }

   // done
   return 0;
}


/*******************************************************************************
TokenBinaryToMem - Take a list of MMLTOK that are known to be Binary, and serialize
them to memory.

inputs
   PMMLTOK        pTok - Pointer to the start of the list
   DWORD          dwNum - Number
   PCMem          pMem - Memory to append onto. Just adds onto m_dwCurPosn
   BOOL           fDontEncodeBinary - Set to TRUE if don't want to encode binary
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD TokenBinaryToMem (PMMLTOK pTok, size_t dwNum, PCMem pMem, BOOL fDontEncodeBinary)
{
   // copy all the Binary into a unified memory
   CMem memWCHAR;
   size_t dwNeed;
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      dwNeed = (DWORD)pTok[i].iValue + sizeof(DWORD);
      if (!memWCHAR.Required(memWCHAR.m_dwCurPosn + dwNeed)) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenBinaryToMem Required %ld", (__int64) memWCHAR.m_dwCurPosn + dwNeed);
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
         return 2;
      }
      *((DWORD*) ((PBYTE)memWCHAR.p + memWCHAR.m_dwCurPosn)) = (DWORD)dwNeed - sizeof(DWORD);
      memcpy ((PBYTE)memWCHAR.p + (memWCHAR.m_dwCurPosn+sizeof(DWORD)),
         pTok[i].pBinary, dwNeed-sizeof(DWORD));
      //MMLBinaryFromString (pTok[i].pszString, bugbug
      //   (PBYTE)memWCHAR.p + (memWCHAR.m_dwCurPosn+sizeof(DWORD)),
      //   dwNeed - sizeof(DWORD));
      memWCHAR.m_dwCurPosn += dwNeed;
   }

   // BUGFIX - Cant have flag so dont encode binary
   PCMem pFinal;
   CMem memHuff;
   CMem memPattern;
   if (fDontEncodeBinary) {
      pFinal = &memWCHAR;
      goto skipencode;
   }

   // pattern sort these
   DWORD dwRet;
      // BUGFIX - Using patternencodegroup instead of patternencode
   dwRet = PatternEncodeGroup ((PBYTE) memWCHAR.p, memWCHAR.m_dwCurPosn / sizeof(BYTE), sizeof(BYTE),
      &memPattern, NULL);
   if (dwRet) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenBinaryToMem PatternEncode group %ld", (__int64) memWCHAR.m_dwCurPosn);
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return dwRet;
   }

   // huffman encode thse
   dwRet = HuffmanEncode ((PBYTE) memPattern.p, memPattern.m_dwCurPosn / sizeof(DWORD),
      sizeof(DWORD), &memHuff);
   if (dwRet) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenBinaryToMem HuffmanEncode %ld", (__int64) memWCHAR.m_dwCurPosn);
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return dwRet;
   }
   pFinal = &memHuff;

skipencode:
   // required
   DWORD *pdw;
   dwNeed = 2 * sizeof(DWORD) + pFinal->m_dwCurPosn;
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed)) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenBinaryToMem Required2 %ld", (__int64) memWCHAR.m_dwCurPosn + dwNeed);
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return 2;
   }
   pdw = (DWORD*) ((PBYTE) pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   // write this out
   *(pdw++) = (DWORD)pFinal->m_dwCurPosn;
   *(pdw++) = (DWORD)dwNum;
   memcpy (pdw, pFinal->p, pFinal->m_dwCurPosn);

   return 0;
}

/*******************************************************************************
TokenBinaryFromMem - Given memory containing the output of TokenBinaryToMem(),
this decodes it and add the elements sequentiall to plTokens.

inputs
   PBYTE          pTree - Memory containing the tree
   DWORD          dwSize - Number of bytes left in all the memory
   PCListFixed    plTokensType - Filled with the TOKENTYPE structure
   PCListVariable plTokens - Filled in with wide Binary for each of the tokens
   DWORD          *pdwUsed - Filled with number of bytes used
   BOOL           fDontEncodeBinary - Set to TRUE if don't want to encode binary
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD TokenBinaryFromMem (PBYTE pTree, size_t dwSize, PCListFixed plTokensType, PCListVariable plTokens,
                      size_t *pdwUsed, BOOL fDontEncodeBinary)
{
   // make sure enough space
   if (dwSize < 8)
      return 2;   // cant be this small

   DWORD dwNum, dwHuff;
   DWORD *pdw;
   pdw = (DWORD*) pTree;
   dwHuff = *(pdw++);
   dwNum = *(pdw++);
   dwSize -= 8;
   if (dwSize < dwHuff)
      return 2;   // too small
   *pdwUsed = 2 * sizeof(DWORD) + dwHuff;

   PBYTE pb;
   CMem memPattern;
   CMem memHuff;
   if (fDontEncodeBinary) {
      pb = (PBYTE)pdw;

      goto skipdecode;
   }

   // huffman decode
   DWORD dwRet;
   size_t dwUsed;
   dwRet = HuffmanDecode ((PBYTE) pdw, dwHuff, sizeof(DWORD), &memHuff, &dwUsed);
   if (dwRet)
      return dwRet;

   // pattern decode
      // BUGFIX - Using patterndecodegroup instead of patterndecode
   dwRet = PatternDecodeGroup ((PBYTE) memHuff.p, memHuff.m_dwCurPosn, sizeof(BYTE),
      &memPattern, &dwUsed);
   if (dwRet)
      return dwRet;
   pb = (PBYTE) memPattern.p;

skipdecode:
   // go through the loop
   DWORD dwNeed, i;
   // NOTE: pb must be set
   plTokensType->Required (plTokensType->Num() + dwNum);
   plTokens->Required (plTokens->Num() + dwNum);
   for (i = 0; i < dwNum; i++) {
      TOKENTYPE tt;
      tt.dwType = MMNA_BINARY;
      plTokensType->Add (&tt);
      
      dwNeed = *((DWORD*)pb);
      plTokens->Add (pb + sizeof(DWORD), dwNeed);

      pb += (dwNeed + sizeof(DWORD));
   }

   // done
   return 0;
}


/*******************************************************************************
TokenFloatsToMem - Take a list of MMLTOK that are known to be Floats, and serialize
them to memory.

inputs
   PMMLTOK        pTok - Pointer to the start of the list
   DWORD          dwNum - Number
   PCMem          pMem - Memory to append onto. Just adds onto m_dwCurPosn
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD TokenFloatsToMem (PMMLTOK pTok, size_t dwNum, PCMem pMem)
{
   DWORD dwDetail = 100000; // to 1/100,000 of a meter
      // BUGFIX - Upped detail from 1/10,000 meter to 1/100,000 meter so can do hair

   // convert all the floats into __int64
   CMem memInt;
   size_t dwNeed;
   DWORD i;
   if (!memInt.Required (dwNum * sizeof(__int64)))
      return 1;
   __int64 iPrev, iNew, *pi;
   pi = (__int64*) memInt.p;
   iPrev = 0;
#if 0//def _DEBUG
   FILE *f;
   f = fopen("c:\\floatsorig.txt", "wt");
#endif
   for (i = 0; i < dwNum; i++) {
#if 0//def _DEBUG
      char szaTemp[64];
      WideCharToMultiByte (CP_ACP, 0, pTok[i].pszString, -1, szaTemp, sizeof(szaTemp), 0, 0);
      fprintf (f, "%s\n", szaTemp);
#endif
      iNew = (__int64) floor((double)pTok[i].fValue * (double) dwDetail + .5);
         // added floor(XXX+.5) so that would round properly - less error
      pi[i] = iNew - iPrev;
      iPrev = iNew;
   }
#if 0//def _DEBUG
   if (f)
      fclose (f);
#endif
   memInt.m_dwCurPosn = dwNum * sizeof(__int64);

   // huffman encode thse
   CMem memHuff;
   DWORD dwRet;
   dwRet = HuffmanEncode ((PBYTE) memInt.p, memInt.m_dwCurPosn / sizeof(__int64),
      sizeof(__int64), &memHuff);
   if (dwRet)
      return dwRet;

   // required
   DWORD *pdw;
   dwNeed = 3 * sizeof(DWORD) + memHuff.m_dwCurPosn;
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 2;
   pdw = (DWORD*) ((PBYTE) pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   // write this out
   *(pdw++) = (DWORD)memHuff.m_dwCurPosn;
   *(pdw++) = (DWORD)dwNum;
   *(pdw++) = dwDetail;
   memcpy (pdw, memHuff.p, memHuff.m_dwCurPosn);

   return 0;
}

/*******************************************************************************
TokenFloatsFromMem - Given memory containing the output of TokenFloatsToMem(),
this decodes it and add the elements sequentiall to plTokens.

inputs
   PBYTE          pTree - Memory containing the tree
   DWORD          dwSize - Number of bytes left in all the memory
   PCListFixed    plTokensType - Filled with the TOKENTYPE structure
   PCListVariable plTokens - Filled in with wide Floats for each of the tokens
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD TokenFloatsFromMem (PBYTE pTree, size_t dwSize, PCListFixed plTokensType, PCListVariable plTokens,
                      size_t *pdwUsed)
{
   // BUGBUG - may eventually wish to store floats as just float, not this complex
   // compression scheme

   // make sure enough space
   if (dwSize < 3*sizeof(DWORD))
      return 2;   // cant be this small

   DWORD dwNum, dwHuff, dwDetail;
   DWORD *pdw;
   pdw = (DWORD*) pTree;
   dwHuff = *(pdw++);
   dwNum = *(pdw++);
   dwDetail = *(pdw++);
   dwSize -= 3 * sizeof(DWORD);
   if (dwSize < dwHuff)
      return 2;   // too small
   *pdwUsed = 3 * sizeof(DWORD) + dwHuff;

   // huffman decode
   CMem memHuff;
   DWORD dwRet;
   size_t dwUsed;
   dwRet = HuffmanDecode ((PBYTE) pdw, dwHuff, sizeof(__int64), &memHuff, &dwUsed);
   if (dwRet)
      return dwRet;

   // loop through elements
   DWORD i;
   __int64 iCur, *pi;
   double f;
   //WCHAR szTemp[64];
   pi = (__int64*) memHuff.p;
   iCur = 0;
#if 0//def _DEBUG
   FILE *f2;
   f2 = fopen("c:\\floatsout.txt", "wt");
#endif
   plTokensType->Required (plTokensType->Num() + dwNum);
   plTokens->Required (plTokens->Num() + dwNum);
   for (i = 0; i < dwNum; i++) {
      TOKENTYPE tt;
      tt.dwType = MMNA_DOUBLE;
      plTokensType->Add (&tt);

      iCur += pi[i];
      f = (double) iCur / (double) dwDetail;

      plTokens->Add (&f, sizeof(f));

      //MMLDoubleToString (f, szTemp);
#if 0//def _DEBUG
      char szaTemp[64];
      WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
      fprintf (f2, "%s\n", szaTemp);
#endif
      //plTokens->Add (szTemp, (wcslen(szTemp)+1)*2);
   }
#if 0//def _DEBUG
   if (f2)
      fclose (f2);
#endif

   // done
   return 0;
}


/*******************************************************************************
TokenIntsToMem - Take a list of MMLTOK that are known to be Ints, and serialize
them to memory.

inputs
   PMMLTOK        pTok - Pointer to the start of the list
   DWORD          dwNum - Number
   PCMem          pMem - Memory to append onto. Just adds onto m_dwCurPosn
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD TokenIntsToMem (PMMLTOK pTok, size_t dwNum, PCMem pMem)
{
   // write out all the ints
   CMem memInt;
   size_t dwNeed;
   DWORD i;
   if (!memInt.Required (dwNum * sizeof(int)))
      return 1;
   int iPrev, iNew, *pi;
   pi = (int*) memInt.p;
   iPrev = 0;
   for (i = 0; i < dwNum; i++) {
      iNew = pTok[i].iValue;
      pi[i] = iNew - iPrev;
      iPrev = iNew;
   }
   memInt.m_dwCurPosn = dwNum * sizeof(int);

   // huffman encode thse
   CMem memHuff;
   DWORD dwRet;
   dwRet = HuffmanEncode ((PBYTE) memInt.p, memInt.m_dwCurPosn / sizeof(int),
      sizeof(int), &memHuff);
   if (dwRet)
      return dwRet;

   // required
   DWORD *pdw;
   dwNeed = 2 * sizeof(DWORD) + memHuff.m_dwCurPosn;
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 2;
   pdw = (DWORD*) ((PBYTE) pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   // write this out
   *(pdw++) = (DWORD)memHuff.m_dwCurPosn;
   *(pdw++) = (DWORD)dwNum;
   memcpy (pdw, memHuff.p, memHuff.m_dwCurPosn);

   return 0;
}

/*******************************************************************************
TokenIntsFromMem - Given memory containing the output of TokenIntsToMem(),
this decodes it and add the elements sequentiall to plTokens.

inputs
   PBYTE          pTree - Memory containing the tree
   DWORD          dwSize - Number of bytes left in all the memory
   PCListFixed    plTokensType - Filled with the TOKENTYPE structure
   PCListVariable plTokens - Filled in with wide Ints for each of the tokens
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD TokenIntsFromMem (PBYTE pTree, size_t dwSize, PCListFixed plTokensType, PCListVariable plTokens,
                      size_t *pdwUsed)
{
   // make sure enough space
   if (dwSize < 2*sizeof(DWORD))
      return 2;   // cant be this small

   DWORD dwNum, dwHuff;
   DWORD *pdw;
   pdw = (DWORD*) pTree;
   dwHuff = *(pdw++);
   dwNum = *(pdw++);
   dwSize -= 2 * sizeof(DWORD);
   if (dwSize < dwHuff)
      return 2;   // too small
   *pdwUsed = 2 * sizeof(DWORD) + dwHuff;

   // huffman decode
   CMem memHuff;
   DWORD dwRet;
   size_t dwUsed;
   dwRet = HuffmanDecode ((PBYTE) pdw, dwHuff, sizeof(int), &memHuff, &dwUsed);
   if (dwRet)
      return dwRet;

   // loop through elements
   DWORD i;
   int iCur, *pi;
   //WCHAR szTemp[64];
   pi = (int*) memHuff.p;
   iCur = 0;
   plTokensType->Required (plTokensType->Num() + dwNum);
   plTokens->Required (plTokens->Num() + dwNum);
   for (i = 0; i < dwNum; i++) {
      TOKENTYPE tt;
      tt.dwType = MMNA_INT;
      plTokensType->Add (&tt);

      iCur += pi[i];
      plTokens->Add (&iCur, sizeof(iCur));
      //_itow (iCur, szTemp, 10);
      //plTokens->Add (szTemp, (wcslen(szTemp)+1)*2);
   }

   // done
   return 0;
}




/*******************************************************************************
TokenCommonToMem - Take a list of MMLTOK that are known to be Common builtin, and serialize
them to memory.

inputs
   PMMLTOK        pTok - Pointer to the start of the list
   DWORD          dwNum - Number
   PCMem          pMem - Memory to append onto. Just adds onto m_dwCurPosn
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD TokenCommonToMem (PMMLTOK pTok, size_t dwNum, PCMem pMem)
{
   // write out all the Common
   CMem memInt;
   size_t dwNeed;
   DWORD i;
   if (!memInt.Required (dwNum * sizeof(int)))
      return 1;
   int *pi;
   pi = (int*) memInt.p;
   for (i = 0; i < dwNum; i++) {
      pi[i] = (int) BuiltInFind (pTok[i].pszString);
   }
   memInt.m_dwCurPosn = dwNum * sizeof(int);

   // huffman encode thse
   CMem memHuff;
   DWORD dwRet;
   dwRet = HuffmanEncode ((PBYTE) memInt.p, memInt.m_dwCurPosn / sizeof(int),
      sizeof(int), &memHuff);
   if (dwRet)
      return dwRet;

   // required
   DWORD *pdw;
   dwNeed = 2 * sizeof(DWORD) + memHuff.m_dwCurPosn;
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 2;
   pdw = (DWORD*) ((PBYTE) pMem->p + pMem->m_dwCurPosn);
   pMem->m_dwCurPosn += dwNeed;

   // write this out
   *(pdw++) = (DWORD)memHuff.m_dwCurPosn;
   *(pdw++) = (DWORD)dwNum;
   memcpy (pdw, memHuff.p, memHuff.m_dwCurPosn);

   return 0;
}

/*******************************************************************************
TokenCommonFromMem - Given memory containing the output of TokenCommonToMem(),
this decodes it and add the elements sequentiall to plTokens.

inputs
   PBYTE          pTree - Memory containing the tree
   DWORD          dwSize - Number of bytes left in all the memory
   PCListFixed    plTokensType - Filled with the TOKENTYPE structure
   PCListVariable plTokens - Filled in with wide Common for each of the tokens
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD TokenCommonFromMem (PBYTE pTree, size_t dwSize, PCListFixed plTokensType, PCListVariable plTokens,
                      size_t *pdwUsed)
{
   // make sure enough space
   if (dwSize < 2*sizeof(DWORD))
      return 2;   // cant be this small

   DWORD dwNum, dwHuff;
   DWORD *pdw;
   pdw = (DWORD*) pTree;
   dwHuff = *(pdw++);
   dwNum = *(pdw++);
   dwSize -= 2 * sizeof(DWORD);
   if (dwSize < dwHuff)
      return 2;   // too small
   *pdwUsed = 2 * sizeof(DWORD) + dwHuff;

   // huffman decode
   CMem memHuff;
   DWORD dwRet;
   size_t dwUsed;
   dwRet = HuffmanDecode ((PBYTE) pdw, dwHuff, sizeof(int), &memHuff, &dwUsed);
   if (dwRet)
      return dwRet;

   BuiltInRead ();

   // loop through elements
   DWORD i;
   int *pi;
   pi = (int*) memHuff.p;
   plTokensType->Required (plTokensType->Num() + dwNum);
   plTokens->Required (plTokens->Num() + dwNum);
   for (i = 0; i < dwNum; i++) {
      TOKENTYPE tt;
      tt.dwType = MMNA_STRING;
      plTokensType->Add (&tt);

      PWSTR psz = gTreeBuiltIn.Enum ((DWORD)pi[i]);
      if (!psz)
         return 2;   // error
      plTokens->Add (psz, (wcslen(psz)+1)*2);
   }

   // done
   return 0;
}

/*******************************************************************************
TokenTreeToMem - Take a list of MMLTOK and serialize it to memory.

inputs
   PCListFixed    plTokens - List of MMLTOK
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
   PTMHDR         pth - token-map header generated when remapped tokens
   BOOL           fDontEncodeBinary - Set to TRUE if don't want to encode binary
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD TokenTreeToMem (PCListFixed plTokens, PCMem pMem, PTMHDR pth, BOOL fDontEncodeBinary)
{
   DWORD dwNum = plTokens->Num();
   PMMLTOK pTok = (PMMLTOK) plTokens->Get(0);

   // first off, make sure enough space for the header
   if (!pMem->Required (pMem->m_dwCurPosn + sizeof(TMHDR))) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenTreeToMem Required %ld", (__int64) pMem->m_dwCurPosn + sizeof(TMHDR));
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return 1;
   }
   memcpy ((PBYTE)pMem->p + pMem->m_dwCurPosn, pth, sizeof(*pth));
   pMem->m_dwCurPosn += sizeof(*pth);

   // write out the common tokens
   DWORD dwRet;
   dwRet = TokenCommonToMem (pTok + pth->dwCommonBuiltInID, pth->dwStringID - pth->dwCommonBuiltInID, pMem);
   if (dwRet) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenTreeToMem TokenCommonToMem");
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return dwRet;
   }


   // write all the strings out
   dwRet = TokenStringsToMem (pTok + pth->dwStringID, pth->dwBinaryID - pth->dwStringID, pMem);
   if (dwRet) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenTreeToMem TokenStringsToMem");
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return dwRet;
   }

   // write out the binary
   dwRet = TokenBinaryToMem (pTok + pth->dwBinaryID, pth->dwIntID - pth->dwBinaryID, pMem, fDontEncodeBinary);
   if (dwRet) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenTreeToMem TokenBinaryToMem");
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return dwRet;
   }


   // write out the ints
   dwRet = TokenIntsToMem (pTok + pth->dwIntID, pth->dwFloatID - pth->dwIntID, pMem);
   if (dwRet) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenTreeToMem TokenIntsToMem");
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return dwRet;
   }


   // write out the floats
   dwRet = TokenFloatsToMem (pTok + pth->dwFloatID, pth->dwBuiltInStringID - pth->dwFloatID, pMem);
   if (dwRet) {
#ifdef _WIN64MEMDEBUG
         char szTemp[64];
         sprintf (szTemp, "TokenTreeToMem TokenFloatsToMem");
         MessageBox (NULL, szTemp, szTemp, MB_OK);
#endif // _WIN64MEMDEBUG
      return dwRet;
   }

   // done
   return 0;
}


/*******************************************************************************
TokenTreeFromMem - Given memory containing the token tree (output by TokenTreeToMem),
this fills in plTokens.

inputs
   PBYTE          pTree - Memory containing the tree
   DWORD          dwSize - Number of bytes left in all the memory
   PCListFixed    plTokensType - Filled with the TOKENTYPE structure
   PCListVariable plTokens - Filled in with wide strings for each of the tokens
   DWORD          *pdwUsed - Filled with number of bytes used
   BOOL           fDontEncodeBinary - Set to TRUE if don't want to encode binary
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD TokenTreeFromMem (PBYTE pTree, size_t dwSize, PCListFixed plTokensType, PCListVariable plTokens,
                      size_t *pdwUsed, BOOL fDontEncodeBinary)
{
   *pdwUsed = 0;
   size_t dwOrigSize = dwSize;
   plTokens->Clear();
   plTokensType->Init (sizeof(TOKENTYPE));

   // make sure enough space for the header
   PTMHDR pth;
   if (dwSize < sizeof(TMHDR))
      return 2;
   pth = (PTMHDR) pTree;
   pTree += sizeof(TMHDR);
   dwSize -= sizeof(TMHDR);

   // if wrong version note this
   if (pth->dwVersion != TMVERSION)
      return 2;
   BuiltInRead();
   if (pth->dwMaxBuiltIn > gTreeBuiltIn.Num())
      return 2;   // added new built ins since loaded

   DWORD dwRet;
   size_t dwUsed;

   // common built in
   dwUsed = 0;
   dwRet = TokenCommonFromMem (pTree, dwSize, plTokensType, plTokens, &dwUsed);
   if (dwRet)
      return dwRet;
   pTree += dwUsed;
   dwSize -= dwUsed;

   // read in strings
   dwUsed = 0;
   dwRet = TokenStringsFromMem (pTree, dwSize, plTokensType, plTokens, &dwUsed);
   if (dwRet)
      return dwRet;
   pTree += dwUsed;
   dwSize -= dwUsed;

   // binary
   dwUsed = 0;
   dwRet = TokenBinaryFromMem (pTree, dwSize, plTokensType, plTokens, &dwUsed, fDontEncodeBinary);
   if (dwRet)
      return dwRet;
   pTree += dwUsed;
   dwSize -= dwUsed;

   // int
   dwUsed = 0;
   dwRet = TokenIntsFromMem (pTree, dwSize, plTokensType, plTokens, &dwUsed);
   if (dwRet)
      return dwRet;
   pTree += dwUsed;
   dwSize -= dwUsed;

   // float
   dwUsed = 0;
   dwRet = TokenFloatsFromMem (pTree, dwSize, plTokensType, plTokens, &dwUsed);
   if (dwRet)
      return dwRet;
   pTree += dwUsed;
   dwSize -= dwUsed;

   // NOTE: No point going through built-ins because will index those separately

   *pdwUsed = dwOrigSize - dwSize;
   return 0;
}



/*******************************************************************************
MMLCompressToToken - Takes MML and outputs a file containing compressed tokens.

inputs
   PCMMLNode2      pNode - node and subnodes to use
   PCProgressSocket     pProgress - Progress measurement. Can be NULL
   PCMem          pMem - Memory to put the compressed data in. m_dwCurPosn is filled with length of memory
   BOOL           fDontEncodeBinary - Set to TRUE if don't want to encode binary
returns
   DWORD - Error. 0 if no error, 1 if error
*/
DWORD MMLCompressToToken (PCMMLNode2 pNode, PCProgressSocket pProgress, PCMem pMem, BOOL fDontEncodeBinary)
{
   CListVariable lTemp; // BUGBUG - wont need eventually
   if (pProgress)
      pProgress->Update (0);

   // allocate a DWORD for storing the size of the tokensize node
   size_t dwSize;
   dwSize = pMem->m_dwCurPosn;
   if (!pMem->Required (pMem->m_dwCurPosn + sizeof(DWORD)))
      return 1;
   pMem->m_dwCurPosn += sizeof(DWORD);

   // create the tokens
   CListFixed lTokens;
   DWORD dwRet;
   CMem memTokens;
   lTokens.Init (sizeof(MMLTOK));
   dwRet = TokenizeNode (pNode, &memTokens, &lTokens, &lTemp);
   if (dwRet)
      return dwRet;

   // remaps the IDs
   TMHDR th;
   dwRet = RemapMMLTOKToID ((PMMLTOK)lTokens.Get(0), lTokens.Num(),
      (DWORD*) memTokens.p, memTokens.m_dwCurPosn / sizeof(DWORD), &th);
   if (dwRet)
      return dwRet;

   if (pProgress) {
      pProgress->Update (.2);
      pProgress->Push (.2, .8);
   }

   // pattern compress
   CMem memPattern;
      // BUGFIX - Using pattern encode group instead of pattern encode
   dwRet = PatternEncodeGroup ((PBYTE) memTokens.p, memTokens.m_dwCurPosn / sizeof(DWORD),
      sizeof(DWORD), &memPattern, pProgress);
   if (dwRet)
      return dwRet;

   if (pProgress) {
      pProgress->Pop();
      pProgress->Update (.8);
   }

   // encode by recordering
   // BUGFIX - Take out reorder encode because makes data larger afgter pattern encode
   //CMem  memReorder;
   //dwRet = ReorderEncode ((PBYTE) memPattern.p, memPattern.m_dwCurPosn / sizeof(DWORD),
   //   sizeof(DWORD), &memReorder);
   //if (dwRet)
   //   return dwRet;

   // huffman encode the token list
   //CMem memHuff;
   dwRet = HuffmanEncode ((PBYTE) memPattern.p, memPattern.m_dwCurPosn / sizeof(DWORD),
      sizeof(DWORD), pMem);
   if (dwRet)
      return dwRet;

   // RLE encode huffman
   // NOTE: No RLE since doesn't save enough
   //dwRet = RLEEncode ((PBYTE) memHuff.p, memHuff.m_dwCurPosn, 1, pMem);
   //if (dwRet)
   //   return dwRet;

   // store away this size
   DWORD *pdw;
   pdw = (DWORD*) (((PBYTE) pMem->p) + dwSize);
   *pdw = (DWORD)(pMem->m_dwCurPosn - (dwSize + sizeof(DWORD)));

#ifdef _DEBUG
   char szTemp[256];
   DWORD dwUsed;
   dwUsed = *pdw;
   sprintf (szTemp, "List-of-token IDs size (bytes): %d\r\n", (int) dwUsed);
   OutputDebugString (szTemp);
#endif

   if (pProgress)
      pProgress->Update (.8);

   // add on the token strings
   dwRet = TokenTreeToMem (&lTokens, pMem, &th, fDontEncodeBinary);
   if (dwRet)
      return dwRet;

   if (pProgress)
      pProgress->Update (1);

#ifdef _DEBUG
   sprintf (szTemp, "List of tokens size (byte): %d\r\n", (int) (pMem->m_dwCurPosn - dwUsed));
   OutputDebugString (szTemp);

   sprintf (szTemp, "Number of unique tokens: %d\r\n", (int) lTokens.Num());
   OutputDebugString (szTemp);

   DWORD i, dwCount1,dwCount2;
   PMMLTOK pTok;
   dwCount1 = dwCount2 = 0;
   pTok = (PMMLTOK) lTokens.Get(0);
   for (i = 0; i < lTokens.Num(); i++) {
      dwCount1 += pTok[i].dwTimesUsed;
      if (pTok[i].dwTimesUsed > 1)
         dwCount2++;
   }

   sprintf (szTemp, "Percent of non-unique tokens: %g\r\nNumber of tokens total: %d\r\n",
      (double) dwCount2 / (double) lTokens.Num() * 100.0, (int) dwCount1);
   OutputDebugString (szTemp);

   FILE *f;
   f = fopen ("c:\\m3dcomp.bin", "wb");
   if (f) {
      fwrite (pMem->p, dwUsed, 1, f);
      fclose (f);
   }
#endif

   return 0;
}



/*******************************************************************************
MMLDeompressFromToken - Takes a token comression and returns MML.

inputs
   PVOID          pMem - Original data
   DWORD          dwSize - Number of bytes in original data
   PCProgressSocket     pProgress - Progress measurement. Can be NULL
   PCMMLNode2      *ppNode - Filled with a pointer to the MML node, which MUST
                  be freed by the caller.
   BOOL           fDontEncodeBinary - Set to TRUE if don't want to encode binary
returns
   DWORD - Error. 0 if no error, 1 if need a newer version of app to load, 2 if unknown error
*/
DWORD MMLDecompressFromToken (PVOID pMem, size_t dwSize, PCProgressSocket pProgress, PCMMLNode2 *ppNode, BOOL fDontEncodeBinary)
{
   *ppNode = NULL;

   if (pProgress)
      pProgress->Update (0);

   // find the size so can skip ahead
   CListVariable lTokens;
   CListFixed lTokensType;
   lTokensType.Init (sizeof(TOKENTYPE));
   DWORD dwSkip;
   if (dwSize < sizeof(DWORD))
      return 2;   // not lare enough
   dwSkip = *((DWORD*) pMem);
   pMem = (PVOID) ((PBYTE) pMem + sizeof(DWORD));
   dwSize -= sizeof(DWORD);
   if (dwSkip >= dwSize)
      return 2;   // no large enough

   // load in the token strings
   DWORD dwRet;
   size_t dwUsed;
   dwRet =TokenTreeFromMem ((PBYTE) pMem + dwSkip, dwSize - dwSkip, &lTokensType, &lTokens, &dwUsed, fDontEncodeBinary);
   if (dwRet)
      return dwRet;
   /// ignore dwUsed from TokenFreeFromMem() because should be at the EOF


   if (pProgress)
      pProgress->Update (.3);

   // remove the RLE encomding
   // NOTE: No RLE coding since doesnt save enough
   //CMem memRLE;
   //dwRet = RLEDecode ((PBYTE) pMem, dwSkip, 1, &memRLE, &dwUsed);
   //if (dwRet)
   //   return dwRet;

   // remove huffman coding from list of tokens
   CMem memHuff;
   dwRet = HuffmanDecode ((PBYTE) pMem, dwSkip, sizeof(DWORD), &memHuff, &dwUsed);
   if (dwRet)
      return dwRet;

   // remove the reordering
   // BUGFIX - Take out Reorder because makes data larger
   //CMem memReorder;
   //dwRet = ReorderDecode ((PBYTE) memHuff.p, memHuff.m_dwCurPosn, sizeof(DWORD), &memReorder, &dwUsed);
   //if (dwRet)
   //   return dwRet;

   if (pProgress)
      pProgress->Update (.5);

   CMem memPattern;
      // BUGFIX - Using patterndecodegroup instead of patterndecode
   dwRet = PatternDecodeGroup ((PBYTE) memHuff.p, memHuff.m_dwCurPosn,
      sizeof(DWORD), &memPattern, &dwUsed);
   if (dwRet)
      return dwRet;

   if (pProgress)
      pProgress->Update (.8);

   // create the nodes
   // NOTE: using dwSkip
   dwRet = DeTokenizeNode ((DWORD*) memPattern.p, memPattern.m_dwCurPosn / sizeof(DWORD),
      &lTokensType, &lTokens, &dwUsed, ppNode);
   if (dwRet)
      return dwRet;
   (*ppNode)->NameSet (L"Detoken");


   if (pProgress)
      pProgress->Update (1);

   return 0;
}

/*******************************************************************************
MMLCompressToANSI - Simple compression, from MML to ANSI text.

inputs
   PCMMLNode2      pNode - node and subnodes to use
   PCProgressSocket     pProgress - Progress measurement. Can be NULL
   PCMem          pMem - Memory to put the compressed data in. m_dwCurPosn is filled with length of memory
returns
   DWORD - Error. 0 if no error, 1 if error
*/
DWORD MMLCompressToANSI (PCMMLNode2 pNode, PCProgressSocket pProgress, PCMem pMem)
{
   if (pProgress)
      pProgress->Update (0);

   // conver to a string
   CMem mem;
   // write out
   if (!MMLToMem (pNode, &mem, TRUE)) {
      delete pNode;
      return 1;
   }

   if (pProgress)
      pProgress->Update (.50);

   // convert from unicode to ANSI
   if (!pMem->Required (pMem->m_dwCurPosn + mem.m_dwCurPosn+2))   // BUGFIX - offset from pMem->m_dwCurPosn
      return 1;
   int iLen;
   iLen = WideCharToMultiByte (CP_ACP, 0, (PWSTR) mem.p, -1,
      (char*) pMem->p + pMem->m_dwCurPosn, (DWORD)(pMem->m_dwAllocated - pMem->m_dwCurPosn), 0, 0);
      // BUGFIX - Offset pMem by m_dwCurPosn
   pMem->m_dwCurPosn += (DWORD) iLen;

   if (pProgress)
      pProgress->Update (1);

   return 0;
}



/*******************************************************************************
MMLDeompressFromANSI - Simple decompression, from ANSI to MML.

inputs
   PVOID          pMem - Original data
   DWORD          dwSize - Number of bytes in original data
   PCProgressSocket     pProgress - Progress measurement. Can be NULL
   PCMMLNode2      *ppNode - Filled with a pointer to the MML node, which MUST
                  be freed by the caller.
returns
   DWORD - Error. 0 if no error, 1 if need a newer version of app to load, 2 if unknown error
*/
DWORD MMLDecompressFromANSI (PVOID pMem, size_t dwSize, PCProgressSocket pProgress, PCMMLNode2 *ppNode)
{
   *ppNode = NULL;

   if (pProgress)
      pProgress->Update (0);

   // convert from ANSI to unicode
   CMem memUni;
   if (!memUni.Required ((dwSize+1) * 2))
      return 2;
   int iRet;
   iRet = MultiByteToWideChar (CP_ACP, 0, (char*)pMem, (DWORD)dwSize, (PWSTR) memUni.p, (DWORD)memUni.m_dwAllocated/2);
   ((PWSTR) memUni.p)[iRet]= 0;  // null terminate

   // get from world
   *ppNode = MMLFromMem((PWSTR) memUni.p);
   if (!(*ppNode))
      return 2;

   if (pProgress)
      pProgress->Update (1);

   return 0;
}


/******************************************************************************
MMLFileOpen - Opens a MML file using a standard file format, from memory

inputs
   PBYTE       pMem - Pointer to an array of memory
   DWORD       dwSize - Number of bytes in the memory
   GUID        *pgID - File ID
               NOTE: if thisis NULL then any file ID is accepted, used for
               scanning files for dependencies.
   PCProgress  pProgress - If not NULL then show progress
returns
   PCMMLNode2 - MMLNode contianing the info, or NULL if fail
*/
PCMMLNode2 MMLFileOpen (PBYTE pMem, size_t dwSize, const GUID *pgID, PCProgressSocket pProgress)
{
   // need 2 guids at the beginning
   if (dwSize < 2 * sizeof(GUID)) {
      return NULL;
   }
   dwSize -= 2*sizeof(GUID);

   GUID ag[2];
   memcpy (&ag[0], pMem, 2 * sizeof(GUID));
   pMem += 2*sizeof(GUID);

   if (pgID && !IsEqualGUID(ag[1], *pgID)) { // BUGFIX - If pgID==NULL then dont check
      return NULL;
   }
   DWORD dwFormat;
   if (IsEqualGUID(ag[0],GUID_FileHeader))
      dwFormat = 0;
   else if (IsEqualGUID(ag[0],GUID_FileHeaderComp))
      dwFormat = 1;
   else {
      return NULL;
   }


   // error
   PCMMLNode2 pNode;
   DWORD dwRet;
   if (dwFormat == 0)
      dwRet = MMLDecompressFromANSI (pMem, dwSize, pProgress, &pNode);
   else
      dwRet = MMLDecompressFromToken (pMem, dwSize, pProgress, &pNode, TRUE);

   if (dwRet)
      return NULL;

   return pNode;
}



/******************************************************************************
MMLFileOpen - Opens a MML file using a standard file format, from a resource

inputs
   HINSTANCE   hInstance - Instance to load the resource from
   DWORD       dwID - Resource ID
   PSTR        pszResType - Type of resource, passed into FindResource
   GUID        *pgID - File ID
returns
   PCMMLNode2 - MMLNode contianing the info, or NULL if fail
*/
PCMMLNode2 MMLFileOpen (HINSTANCE hInstance, DWORD dwID, PSTR pszResType, const GUID *pgID,
                       PCProgressSocket pProgress)
{
   HRSRC    hr;

   hr = FindResource (hInstance, MAKEINTRESOURCE (dwID), pszResType);
   if (!hr)
      return NULL;

   HGLOBAL  hg;
   hg = LoadResource (hInstance, hr);
   if (!hg)
      return NULL;


   PVOID pMem;
   pMem = LockResource (hg);
   if (!pMem)
      return NULL;

   DWORD dwSize;
   dwSize = SizeofResource (hInstance, hr);

   return MMLFileOpen ((PBYTE)pMem, dwSize, pgID, pProgress);
   // NOTe: No need to unlock
}

/******************************************************************************
MMLFileOpen - Opens a MML file using a standard file format.

inputs
   WCHAR       *pszFile - file
   GUID        *pgID - File ID.
               NOTE: if thisis NULL then any file ID is accepted, used for
               scanning files for dependencies.
   PCProgress  pProgress - If not NULL then show progress
   BOOL        fIgnoreDir - If this flag is set AND a megafile is used, the
               fileopen will ignore the directory and look for the file specifically
   BOOL        fBypassGlobalMegafile - If TRUE then bypass any global megafile settings
returns
   PCMMLNode2 - MMLNode contianing the info, or NULL if fail
*/
PCMMLNode2 MMLFileOpen (WCHAR *pszFile, const GUID *pgID, PCProgressSocket pProgress,
                       BOOL fIgnoreDir, BOOL fBypassGlobalMegafile)
{
   if (!pszFile[0])
      return NULL;   // so dont get assert

   // make the name
   char szFile[256];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile, sizeof(szFile), 0, 0);

   PMEGAFILE f = MegaFileOpen(szFile, TRUE,
      (fIgnoreDir ? MFO_IGNOREDIR : 0) | (fBypassGlobalMegafile ? MFO_IGNOREMEGAFILE : 0));
   if (!f)
      return NULL;
   int iSize;
   MegaFileSeek (f, 0, SEEK_END);
   iSize = MegaFileTell (f);
   MegaFileSeek (f, 0, SEEK_SET);

   CMem memANSI;
   if (!memANSI.Required ((DWORD)iSize)) {
      MegaFileClose (f);
      return NULL;
   }
   MegaFileRead (memANSI.p, 1, iSize, f);
   MegaFileClose (f);

   return MMLFileOpen ((PBYTE) memANSI.p, (DWORD)iSize, pgID, pProgress);
}


/******************************************************************************
MMLFileOpen - Opens a MML file using a standard file format.

inputs
   PCMegaFile  pMegaFile - Megafile to use
   WCHAR       *pszFile - file
   GUID        *pgID - File ID.
               NOTE: if thisis NULL then any file ID is accepted, used for
               scanning files for dependencies.
   PCProgress  pProgress - If not NULL then show progress
   BOOL        fIgnoreDir - If this flag is set AND a megafile is used, the
               fileopen will ignore the directory and look for the file specifically
returns
   PCMMLNode2 - MMLNode contianing the info, or NULL if fail
*/
PCMMLNode2 MMLFileOpen (PCMegaFile pMegaFile, WCHAR *pszFile, const GUID *pgID, PCProgressSocket pProgress, 
                       BOOL fIgnoreDir)
{
   if (!pszFile[0])
      return NULL;   // so dont get assert

   __int64 iSize;
   PBYTE pb = (PBYTE) pMegaFile->Load (pszFile, &iSize, fIgnoreDir);
   if (!pb)
      return NULL;

   PCMMLNode2 pNode = MMLFileOpen (pb, (DWORD)iSize, pgID, pProgress);
   MegaFileFree (pb);
   return pNode;
}

/******************************************************************************
MMLFileSave - Saves a MML file using a standard file format.

inputs
   WCHAR       *pszFile - file
   GUID        *pgID - File ID
   PCMMLNode2   pNode - MML node to save
   DWORD       dwFormat - Format to save as. 0 = ansi, 1 = compressed tokens
   PCProgress  pProgress - If not NULL then show progress
   BOOL        fBypassGlobalMegafile - If TRUE then bypass any global megafile settings
returns
   BOOL TRUE if success
*/
BOOL MMLFileSave (WCHAR *pszFile, const GUID *pgID, PCMMLNode2 pNode, DWORD dwFormat, PCProgressSocket pProgress,
                  BOOL fBypassGlobalMegafile)
{
   // make the name
   char szFile[256];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile, sizeof(szFile), 0, 0);

   CMem memANSI;
   DWORD dwRet;
   GUID gHeader;
   switch (dwFormat) {
   case 0:  // ansi
      dwRet = MMLCompressToANSI (pNode, pProgress, &memANSI);
      gHeader= GUID_FileHeader;
      break;
   case 1:  // compressed
   default:
      dwRet = MMLCompressToToken (pNode, pProgress, &memANSI, TRUE);
      gHeader = GUID_FileHeaderComp;
      break;
   }
   if (dwRet)
      return FALSE;

   PMEGAFILE f;
   f = MegaFileOpen (szFile, FALSE,  (fBypassGlobalMegafile ? MFO_IGNOREMEGAFILE : 0));
   if (!f)
      return FALSE;

   MegaFileWrite (&gHeader, sizeof(gHeader), 1, f);
   MegaFileWrite ((const PVOID) pgID, sizeof(*pgID), 1, f);
   MegaFileWrite (memANSI.p, 1, memANSI.m_dwCurPosn, f);
   MegaFileClose (f);
   return TRUE;
}


/******************************************************************************
MMLFileSave - Saves a MML file using a standard file format.

inputs
   PCMem       pMem - Memory to fill up
   GUID        *pgID - File ID
   PCMMLNode2   pNode - MML node to save
   DWORD       dwFormat - Format to save as. 0 = ansi, 1 = compressed tokens
   PCProgress  pProgress - If not NULL then show progress
returns
   BOOL TRUE if success
*/
BOOL MMLFileSave (PCMem pMem, const GUID *pgID, PCMMLNode2 pNode, DWORD dwFormat, PCProgressSocket pProgress)
{
   DWORD dwRet;
   GUID gHeader;

   DWORD dwNeed = sizeof(gHeader) + sizeof(*pgID);
   if (!pMem->Required (dwNeed))
      return FALSE;
   pMem->m_dwCurPosn = dwNeed;

   switch (dwFormat) {
   case 0:  // ansi
      dwRet = MMLCompressToANSI (pNode, pProgress, pMem);
      gHeader= GUID_FileHeader;
      break;
   case 1:  // compressed
   default:
      dwRet = MMLCompressToToken (pNode, pProgress, pMem, TRUE);
      gHeader = GUID_FileHeaderComp;
      break;
   }
   if (dwRet)
      return FALSE;

   // fill in header
   GUID *pg = (GUID*)pMem->p;
   pg[0] = gHeader;
   pg[1] = *pgID;

   return TRUE;
}


/******************************************************************************
MMLFileSave - Saves a MML file using a standard file format.

inputs
   PCMegaFile  pMegaFile - Mega file to use
   WCHAR       *pszFile - file
   GUID        *pgID - File ID
   PCMMLNode2   pNode - MML node to save
   DWORD       dwFormat - Format to save as. 0 = ansi, 1 = compressed tokens
   PCProgress  pProgress - If not NULL then show progress
   FILETIME    *pftCreate - If not NULL< specify creation time
   FILETIME    *pftModify - If not NULL< specify last write time
   FILETIME    *pftAccess - If not NULL< specify last Access time
returns
   BOOL TRUE if success
*/
BOOL MMLFileSave (PCMegaFile pMegaFile, WCHAR *pszFile, const GUID *pgID, PCMMLNode2 pNode, DWORD dwFormat, PCProgressSocket pProgress,
                  FILETIME *pftCreate, FILETIME *pftModify, FILETIME *pftAccess)
{
   CMem mem;
   if (!MMLFileSave (&mem, pgID, pNode, dwFormat, pProgress))
      return FALSE;


   return pMegaFile->Save (pszFile, mem.p, mem.m_dwCurPosn, pftCreate, pftModify, pftAccess);
}

// BUGBUG - might want to try a version that saves floating point values as floats
// rather than trying to save as deltas. Still need version that saves small
// integers (shorts) as 2-byte values.

// BUGBUG - provide a MMLFileSave and MMLFileOpen that takes a megafile and
// writes directly to that.

// BUGBUG - modify MMLFileOpen so that can bypass MFFileRead routines
// and access the memory in MFFileOpen directly, saving extra memory
// copies. THis will speed up TTS load, etc. in MIF client.
// Need to watch out for code that calls into megafile callback and
// downloads it over the net


