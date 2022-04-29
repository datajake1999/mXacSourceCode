/*******************************************************************
MMLParse.cpp - Parses mML files for the data tree.
   mML files are very similar to XML/MHTML.

begun3/19/200 by Mike ROzak
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

// errors
static PWSTR   gaszErrorText[] = {
   L"",   // error 0
   L"Invalid < or > in string.", // error 1
   L"Invalid escape character.", // error 2
   L"Invalid character in name.", // error 3
   L"Invalid character in variable value.", // error 4
   L"Matching end-quote expected.", // erro 5
   L"Angle-bracket (< or >) expected.", // error 6
   L"Equals (=) expected.", // error 7
   L"Out of memory.", // error 8
   L"Invalid character in element content string.", // error 9
   L"No end to substitution string.", // error 10
   L"<?Template?> tag missing file or resource attribute.", // error 11
   L"The template MML does not contain a <?TemplateSubst?> element.", // error 12
   L"<?Include?> tag missing file or resource attribute.", // error 13
   L"No end-tag found for the start-tag.", // error 14
   L"End-tag found when a begin-tag expected. You may have a typo in the end-tag name.", // error 15
   L"" // empty
};
CListVariable     glistMML;     // list of MML resource to file

#define  ISWHITESPACE(x)   (((x)==L'\r') || ((x)==L'\n') || ((x)==L'\t') || ((x)==L' '))
#define  ISANGLEBRACKET(x) ( ((x)==L'<') || ((x)==L'>') )


/********************************************************************
DataToUnicode - Takes a pointer to unknown data, either from a file
or from a resource, and determines if it's a unciode text. If it's
unicode, the data is copied and null-terminated. If it's ANSI it's
converted and NULL terminated. Use the magic unicode tag to determine

inputs
   PVOID       pData - data
   DWORD       dwSize - data size
   BOOL        *pfWasUnicode - if not NULL, this is filled in with TRUE
               if the original data was unciode, FLASE if its ANSI
returns
   PWSTR - Unicode string. Must bee freeed()
*/
PWSTR DataToUnicode (PVOID pData, DWORD dwSize, BOOL *pfWasUnicode)
{
   CMem  mem;
   WCHAR *pc;

   // might be unicode
   if (dwSize >= 2) {
      pc = (WCHAR*) pData;
      if (pc[0] == 0xfeff) {
         // it's unicode
         if (!mem.Required(dwSize))
            return NULL;
         memcpy (mem.p, pc + 1, dwSize - 2);
         pc = (WCHAR*) mem.p;
         pc[dwSize/2 - 1] = 0;   // null terminate

         mem.p = NULL;

         if (pfWasUnicode)
            *pfWasUnicode = TRUE;
         return pc;
      }
   }

   // else it's ANSI
   int   iLen;
   if (!mem.Required(dwSize*2 + 2))
      return NULL;
   pc = (PWSTR) mem.p;
   iLen = MultiByteToWideChar (CP_ACP, 0, (char*) pData, dwSize, pc, dwSize+1);
   pc[iLen] = 0;  // NULL terminate

   mem.p = NULL;

   if (pfWasUnicode)
      *pfWasUnicode = FALSE;
   return pc;
}

/********************************************************************
FileToUnicode - Reads a file and returns a unicode string. The
file is converted from ANSI if necessary.

inputs
   PWSTR       pszFile - file
   BOOL        *pfWasUnicode - if not NULL, this is filled in with TRUE
               if the original data was unciode, FLASE if its ANSI
returns
   PWSTR - Unicode string. Must be freed(). NULL if error
*/
PWSTR FileToUnicode (PWSTR pszFile, BOOL *pfWasUnicode)
{
   // try opening the file
   PMEGAFILE f = NULL;
   PBYTE pMem = NULL;
   PWSTR pszUnicode = NULL;

   // convert to ANSI name
   char  szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   f = MegaFileOpen (szTemp);
   if (!f)
      return NULL;

   // how big is it?
   MegaFileSeek (f, 0, SEEK_END);
   DWORD dwSize;
   dwSize = (DWORD) MegaFileTell (f);
   MegaFileSeek (f, 0, 0);

   // read it in
   pMem = (PBYTE) ESCMALLOC (dwSize);
   if (!pMem) {
      MegaFileClose (f);
      return NULL;
   }

   MegaFileRead (pMem, 1, dwSize, f);
   MegaFileClose (f);


   // convert
   pszUnicode = DataToUnicode (pMem, dwSize, pfWasUnicode);

   if (pMem)
      ESCFREE (pMem);

   return pszUnicode;
}

/********************************************************************
ResourceToUnicode - Reads in a 'MML' resource element. Determines if
it's unicde, and converts to ANSI if necessary

inputs
   HINSTANCE   hInstance - module
   DWORD       dwID - resource ID
   char        *pszResType - Resource type. if NULL use "MML"
returns
   PWSTR - Unicode string. Must be freed(). NULL if error
*/
PWSTR ResourceToUnicode (HINSTANCE hInstance, DWORD dwID, char *pszResType)
{
   if (!pszResType)
      pszResType = "MML";

   // if this ID matches one of the maps the load it from a file
   DWORD i;
   for (i = 0; i < glistMML.Num(); i++) {
      DWORD *pdw = (DWORD*) glistMML.Get(i);
      if (*pdw == dwID) {
         return FileToUnicode ((WCHAR*) (pdw+1));
      }
   }

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

   // convert
   return DataToUnicode (pMem, dwSize);
}


/********************************************************************
CEscError - Constructor & destructor
*/
CEscError::CEscError (void)
{
   m_dwNum = 0;
   m_pszDesc = m_pszSurround = m_pszSource = NULL;
   m_dwSurroundChar = 0;
}

CEscError::~CEscError (void)
{
   // intentionally blank
}

/********************************************************************
CEscError::Set - Sets the error for parsing.

inputs
   DWORD    dwNum - Error number. set to 0 to clear.
   WCHAR    *pszDesc - text descriptin
   WCHAR    *pszSurround - Surrounding text. If this is > 256 chars it will be
               truncted down, assuming plenty of space is left around dwSurroundChar
   DWORD    dwSurroundChar - Index into the surround text where the error occured.
   WCHAR    *pszSource - Pointer of error into original source string.
returns
   none
*/
void CEscError::Set (DWORD dwNum, WCHAR *pszDesc, WCHAR *pszSurround,
                     DWORD dwSurroundChar, WCHAR *pszSource)
{
   m_dwNum = dwNum;
   m_pszSource = pszSource;
   m_dwSurroundChar = dwSurroundChar;
   m_pszDesc = NULL;
   m_pszSurround = NULL;

   DWORD dwNeeded;
   if (pszDesc) {
      dwNeeded = ((DWORD)wcslen(pszDesc) +1) * sizeof(WCHAR);
      if (m_memDesc.Required(dwNeeded)) {
         m_pszDesc = (WCHAR*) m_memDesc.p;
         wcscpy ((WCHAR*) m_pszDesc, pszDesc);
      }
   }

   // copy text around
   if (pszSurround) {
      DWORD dwLen;
      dwLen = (DWORD)wcslen (pszSurround);
      dwLen = min (dwSurroundChar + 256, dwLen);
      dwNeeded = (dwLen+1) * sizeof(WCHAR);
      if (m_memSurround.Required(dwNeeded)) {
         m_pszSurround = (WCHAR*) m_memSurround.p;
         memcpy ((PVOID) m_pszSurround, pszSurround, dwLen * sizeof(WCHAR));
         ((WCHAR*)m_pszSurround)[dwLen] = 0;
      }
   }

   // done
}

/********************************************************************
CEscError - useful public variables

  m_dwNum - error number. 0 if none
  m_pszDesc - Description. NULL if none
  m_pszSurround - Surrounding text
  m_dwSurroundChar - Index into surrounding text wher occured
  m_pszSource - Pointer to source. May no logner be calid.
*/



/********************************************************************
CharToMMLChar - Converts a character to a MML character, if necessary.
For stuff like '&' to "&amp;"

inputs
   WCHAR    cChar - character
returns
   PCWSTR - pointer to a string if a substitution is required. NULL
         if no substitution is required
*/
PCWSTR CharToMMLChar (WCHAR cChar)
{
   switch (cChar) {
   case L'&':
      return L"&amp;";
   case L'<':
      return L"&lt;";
   case L'>':
      return L"&gt;";
   case L'\"':
      return L"&quot;";
   case L'\n':
      return L"&cr;";
   case L'\r':
      return L"&lf;";
   // don't do nbsp;
   case L'\t':
      return L"&tab;";
   default:
      return NULL;
   }

}


/********************************************************************
MMLCharToChar - Converts a MML character, starting with & and ending
with a ; to a unciode character

inputs
   PWSTR    psz - String that starts with '&' and ends with ';'
returns
   WCHAR - character. 0 if can't figure out
*/
WCHAR MMLCharToChar (PWSTR psz)
{
   int   iLen;
   iLen = (int)wcslen(psz);
   if ((psz[0] != L'&') || (psz[iLen-1] != L';'))
      return 0;   // cant figur eout

   switch (psz[1]) {
   case L'#':
      {
         // it's a number
         WCHAR wValue = 0;
         BOOL  fHex = FALSE;
         int   i;
         if ((psz[2] == L'x') || (psz[2] == L'X')) {
            i = 3;
            fHex = TRUE;
         }
         else
            i = 2;
         for (; i < iLen-1; i++) {
            if (fHex)
               wValue = wValue * 16;
            else
               wValue = wValue * 10;
            if ((psz[i] >= L'0') && (psz[i] <= L'9'))
               wValue += (psz[i] - L'0');
            else {
               if (fHex && ((psz[i] >= L'a') && (psz[i] <= L'f'))) {
                  wValue += (psz[i] - L'a' + 10);
               }
               else if (fHex && ((psz[i] >= L'A') && (psz[i] <= L'F'))) {
                  wValue += (psz[i] - L'A' + 10);
               }
               else
                  return 0;
            }
         }

         return wValue;
      }
      break;

   case L'a':
   case L'A':
      if (!_wcsicmp(psz, L"&amp;"))
         return L'&';
      break;

   case L'c':
   case L'C':
      if (!_wcsicmp(psz, L"&cr;"))
         return L'\n';
      break;

   case L'l':
   case L'L':
      if (!_wcsicmp(psz, L"&lt;"))
         return L'<';
      else if (!_wcsicmp(psz, L"&lf;"))
         return L'\r';
      break;

   case L'g':
   case L'G':
      if (!_wcsicmp(psz, L"&gt;"))
         return L'>';
      break;

   case L'n':
   case L'N':
      if (!_wcsicmp(psz, L"&nbsp;"))
         return L' ';
      break;

   case L'q':
   case L'Q':
      if (!_wcsicmp(psz, L"&quot;"))
         return L'\"';
      break;


   case L't':
   case L'T':
      if (!_wcsicmp(psz, L"&tab;"))
         return L'\t';
      break;
   }

   return 0;
}

/********************************************************************
StringToMMLString - Converts a normal Unicode string to a string
that's safe to have in MML. That means converting stuff like '<', etc.

inputs
   WCHAR    *pszString - string
   PCEscError  pError - error object
returns
   WCHAR * - MMLString. The caller must ESCFREE() this
*/
WCHAR *StringToMMLString (WCHAR *pszString)
{
   CMem  mem;
   DWORD i;
   PCWSTR   pConv;
   WCHAR    szNBSP[] = L"&nbsp;";

   // BUGFIX - require at least this length for speed
   mem.Required ((wcslen(pszString) * 4 / 3 +1)*2);

   for (i = 0; pszString[i]; i++) {
      // BUGFIX - If larger than 2K then make sure always have extra K
      if ((mem.m_dwAllocated > 2048) && (mem.m_dwCurPosn + 100 > mem.m_dwAllocated))
         mem.Required (mem.m_dwAllocated + 1024);

      // special case, if it's the first character and get a space, or
      // if it's the last character and get space, then use &sbsp;
      if ((pszString[i] == L' ') && ( (i == 0) || (!pszString[i+1]) )) {
         mem.StrCat (szNBSP);
         continue;
      }

      // also, if it's a space and the ast character was space then use NBSP
      if ((pszString[i] == L' ') && (i > 0) && (pszString[i-1] == L' ')) {
         mem.StrCat (szNBSP);
         continue;
      }

      // see if it should be converted
      pConv = CharToMMLChar(pszString[i]);
      if (pConv)
         mem.StrCat (pConv);
      else
         mem.CharCat (pszString[i]);
   }

   // add NULL-termination
   mem.CharCat (0);

   WCHAR *p;
   p = (WCHAR*) mem.p;
   mem.p = NULL;
   return p;
}


/********************************************************************
StringToMMLString - Converts a normal Unicode string to a string
that's safe to have in MML. That means converting stuff like '<', etc.

inputs
   WCHAR    *pszString - string
   WCHAR    *pszMML - Filled in with MML
   DWORD    dwSize - Number of BYTES available in pszMML
   DWORD    *pdwNeeded - filled in with the number of bytes needed
returns
   BOOL - TRUE if converted, else buffer not large enough
*/
BOOL StringToMMLString (WCHAR *pszString, WCHAR *pszMML, size_t dwSize, size_t *pdwNeeded)
{
   PWSTR psz;
   psz = StringToMMLString (pszString);
   if (!psz) {
      *pdwNeeded = 0;
      return FALSE;
   }

   *pdwNeeded = ((DWORD)wcslen(psz)+1)*2;
   if (*pdwNeeded <= dwSize)
      wcscpy (pszMML, psz);
   ESCFREE (psz);
   return (*pdwNeeded <= dwSize);
}
/********************************************************************
MMLStringToString - Converts a MML string (containing &amp; etc.)
into a normal Unicode string.

inputs
   WCHAR    *pszMMLString - string
   BOOL     fEatWhitespace - If TRUE, eat whitespace at beginning of line
            and simulate HTML's main text. Basically if it sees a CR, then
            eats whitespace until no cr. It will if thi is teh end of the
            line then it deletes it all. Else, if there's another word it leave
            one space.
   PCEscError  pError - error object. Can be NULL
   PCMem    pMem - Memory to fill retults in.
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL MMLStringToString (WCHAR *pszMMLString, BOOL fEatWhitespace, PCEscError pError, PCMem pMem)
{
   DWORD i;
   pMem->m_dwCurPosn = 0;

   // BUGFIX - Since have a pretty goo idea how much will need
   pMem->Required ((wcslen(pszMMLString)+1)*2);


   for (i = 0; pszMMLString[i]; i++) {
      // illegal characters
      if ((pszMMLString[i] == L'>') || (pszMMLString[i] == L'<')) {
         if (pError)
            pError->Set (1, gaszErrorText[1], pszMMLString, i, pszMMLString + i);
         return FALSE;
      }

      // if this is an ampersand seek out the end
      DWORD j;
      if (pszMMLString[i] == L'&') {
         for (j = i+1; pszMMLString[j] && (pszMMLString[j] != L';'); j++);
         if (pszMMLString[j] == 0) {
            if (pError)
               pError->Set (2, gaszErrorText[2], pszMMLString, i, pszMMLString + i);
            return FALSE;
         }

         // see what it is
         WCHAR c, c2;
         c = pszMMLString[j+1];
         pszMMLString[j+1] = 0;
         c2 = MMLCharToChar(pszMMLString + i);
         pszMMLString[j+1] = c;
         if (!c2) {
            if (pError)
               pError->Set (2, gaszErrorText[2], pszMMLString, i, pszMMLString + i);
            return FALSE;
         }

         // else, have valid charcter
         pMem->CharCat (c2);

         i = j; // increase

         continue;
      }

      // if see a CR/LF then eat whitespace until get to nonwhitepsace.
      // if get to non whitepsace then append space. If get to end of string
      // then append nothing
      if (fEatWhitespace && ((pszMMLString[i] == L'\r') || (pszMMLString[i] == L'\n'))) {
         BOOL  fBegin = (i == 0);

         // eat while white space
         for (; pszMMLString[i] && ISWHITESPACE(pszMMLString[i]); i++);

         // if was character at end then put a space in. As long as no CR at beginning
         if (!fBegin && pszMMLString[i])
            pMem->CharCat (L' ');

         // repeat
         i--;  // just to get the right character when do i++
         continue;
      }
      // below is the old version of eating. Disable it
#if 0
      // else, see if it's a whitespace character & we're ignoring
      if (fEatWhitespace && ISWHITESPACE(pszMMLString[i])) {
         // if it's not the first character in the string then add one space
         // BUGFIX - Take this out so spaces around tags within text
         // if (i)
         pMem->CharCat (L' ');

         // now, skip all whitespace characters
         for (; pszMMLString[i] && ISWHITESPACE(pszMMLString[i]); i++);
         i--;  // go back one because continue will increase again
         continue;
      }
#endif
      // add the character
      pMem->CharCat (pszMMLString[i]);
   };

   // null terminate
   pMem->CharCat (0);

   return TRUE;
}


/********************************************************************
MMLStringToString - Converts a MML string (containing &amp; etc.)
into a normal Unicode string.

inputs
   WCHAR    *pszMMLString - string
   BOOL     fEatWhitespace - If TRUE, eat whitespace at beginning of line
            and simulate HTML's main text. Basically if it sees a CR, then
            eats whitespace until no cr. It will if thi is teh end of the
            line then it deletes it all. Else, if there's another word it leave
            one space.
   PCEscError  pError - error object
returns
   WCHAR * - String. The caller must ESCFREE() this.
*/
WCHAR *MMLStringToString (WCHAR *pszMMLString, BOOL fEatWhitespace, PCEscError pError)
{
   // BUGBUG - split the function into to so could use the stringtostring in
   // other code

   CMem  mem;

   if (!MMLStringToString (pszMMLString, fEatWhitespace, pError, &mem))
      return NULL;


   WCHAR *p;
   p = (WCHAR*) mem.p;
   mem.p = NULL;
   return p;
}


/********************************************************************
ParseWhitespace - Skips whitespace (if any)

inputs
   WCHAR    *psz - string
   DWORD    dwCount - number of characters left in the string
   PCEscError  pError - error object
returns
   DWORD - Number of characters used from dwCount
*/
DWORD ParseWhitespace (WCHAR *psz, DWORD dwCount, PCEscError pError)
{
   DWORD i;

   for (i = 0; (i < dwCount) && ISWHITESPACE(psz[i]); i++);

   return i;
}

/********************************************************************
IsLetter - Returns TRUE if it's a letter. NOTE: XML has a very complex
definition of a letter. Mine is simple: Use XML rules < 128, and everything
above is a letter.

inputs
   WCHAR c - character
returns
   BOOL - TRUE if its a letter
*/
BOOL IsLetter (WCHAR c)
{
   return ( ((c >= 0x0041) && (c <= 0x005A)) ||
      ((c >= 0x0061) && (c <= 0x007A)) ||
      (c >= 0x0080));
}

/********************************************************************
ParseName - Parses an element name. It stops at a non-name
character or whitespace. This is good for element or variable names.

  NOTE: Names start with a letter, _, or :. After that they may
  include a letter, digit, ., -, _, or :.

inputs
   WCHAR    *psz - string
   DWORD    dwCount - number of characters left
   PCEscError  pError - error object
returns
   DWORD - Number of characters in the element name
*/
DWORD ParseName (WCHAR *psz, DWORD dwCount, PCEscError pError)
{
   DWORD i = 0;

   if (!dwCount || (!IsLetter (psz[0]) && (psz[0] != L'_') && (psz[0] != L':'))) {
      pError->Set (3, gaszErrorText[3], psz, i, psz + i);
      return NULL;
   }
   i++;  // accept this

   for (i; i < dwCount; i++) {

      // stop at a whitespace
      if (ISWHITESPACE(psz[i]))
         break;

      // stop at non-name character
      if (!IsLetter (psz[i]) && !((psz[i] >= L'0') && (psz[i] <= L'9')) &&
         (psz[i] != L'_') && (psz[i] != L':') && (psz[i] != L'.') && (psz[i] != L'-'))
         break;
   }

   return i;   // got up to the end
}

/********************************************************************
ParseVariableValue - Parses a variable value. It's assumed that
all whitespace before has already been parsed. If this starts with a quote
it parses until just after the next quote. Else, it parses until the
next white space or invalid character.

inputs
   WCHAR    *psz - string
   DWORD    dwCount - number of characters left
   PCEscError  pError - error object
returns
   DWORD - Number of characters in the element name
*/
DWORD ParseVariableValue (WCHAR *psz, DWORD dwCount, PCEscError pError)
{
   DWORD i = 0;

   if (!dwCount) {
      pError->Set (4, gaszErrorText[4], psz, i, psz + i);
      return NULL;
   }

   if (psz[0] == L'\"') {
      // starts with quote. Parse until endquote
      i++;
      BOOL  fFoundEnd;
      fFoundEnd = FALSE;
      for (; i < dwCount; i++) {
         if (psz[i] == L'\"') {
            i++;
            fFoundEnd = TRUE;
            break;
         }

         // look for illegal characters
         if (ISANGLEBRACKET(psz[i])) {
            pError->Set (4, gaszErrorText[4], psz, i, psz + i);
            return NULL;
         }
      }

      // check to make sure don't come to end before quote
      if (!fFoundEnd) {
         pError->Set (5, gaszErrorText[5], psz, 0, psz + 0);
         return NULL;
      }
      // done
   }
   else {
      // no quote, parse until whitepspace
      for (; i < dwCount; i++) {
         // ways can get out
         if (ISWHITESPACE(psz[i]) || ISANGLEBRACKET(psz[i]) ||
            (psz[i] == L'?') || (psz[i] == L'/') )
            break;
      }

      // done
   }

   return i;
}

/********************************************************************
ParseInterpretVariableValue - Once the variable value has been parsed,
this interprets it and returns a string that has been demunged (&amp; etc.)
and quotes taken out. If it starts with a quote its assumed to end with a quote.

inputs
   WCHAR    *psz - start of variable
   DWORD    dwCount - number of characters in it
   PCEscError  pError - error object
returns
   WCHAR * - Unciode string. Caller must ESCFREE() it
*/
WCHAR *ParseInterpVariableValue (WCHAR *psz, DWORD dwCount, PCEscError pError)
{
   WCHAR c;
   WCHAR *pRet = NULL;

   if ((psz[0]) == L'\"') {
      c = psz[dwCount-1];
      psz[dwCount-1] = 0;
      pRet = MMLStringToString (psz+1, FALSE, pError);
      psz[dwCount-1] = c;
   }
   else {
      c = psz[dwCount];
      psz[dwCount] = 0;
      pRet = MMLStringToString (psz, FALSE, pError);
      psz[dwCount] = c;
   }

   return pRet;
}


/********************************************************************
ParseContentString - Parses a content string. Call this right after the '>'.
If goes until it finds a '<', basically.

inputs
   WCHAR    *psz -s tring
   DWORD    dwCount - number of characters left
   PCEscError  pError - error object
returns
   DWORD - Number of characters in the content string
*/
DWORD ParseContentString (WCHAR *psz, DWORD dwCount, PCEscError pError)
{
   // loop until there's an anglebracket
   DWORD i;
   for (i = 0; i < dwCount; i++)
      if (ISANGLEBRACKET(psz[i]))
         break;

   return i;
}

/********************************************************************
ParseInterpretContentString - Once the content string has been parsed out,
this demunges it (takes out &amp; etc.)

inputs
   WCHAR    *psz - start of the content string
   DWORD    dwCount - number of characters in the content string
   PCEscError  pError - error object
returns
   WCHAR * - Unicode string. Caller must ESCFREE() it.
*/
WCHAR * ParseInterpretContentString (WCHAR *psz, DWORD dwCount, PCEscError pError)
{
   WCHAR *pRet;
   WCHAR c;

#if 0 //def _DEBUG
   static WCHAR * pszLast = NULL;
   static int ic = 0;
   //char szHuge[100000];
   ///WideCharToMultiByte (CP_ACP, 0, pszMMLString, -1, szHuge, sizeof(szHuge), 0, 0);
   //OutputDebugString (szHuge);
   if (!wcsncmp(psz, L"QS : P10628 &nbsp;&lf", 20)) {
      ic++;
      _ASSERTE( _CrtCheckMemory( ) );

      if (ic == 2) {
         // Get current flag
         int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

         // Turn on leak-checking bit
         tmpFlag |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;

         // Set flag to the new value
         _CrtSetDbgFlag( tmpFlag );
      }

   }
   pszLast = psz;
#endif

   c = psz[dwCount];
   psz[dwCount] = 0;
   pRet = MMLStringToString (psz, TRUE, pError);
   psz[dwCount] = c;

   return pRet;
}

/********************************************************************
ParseElementDecl - Parses an element declaration. It is assumed that
this is called at the "<" character when it appears.

inputs
   WCHAR    *psz - start of string
   DWORD    dwCount - left in the text string
   DWORD    *pdwType - filled with the type. MMLELEMENT_XXX
   PWSTR    *ppszName - filled with a pointer to the name
   DWORD    *pdwNameCount - number of characters in the name
   PWSTR    *ppszVariables - filled with a pointer to the variables
   DWORD    *pdwVariableCount - Number of characters in the variable section
   BOOL     *pfHasContent - If TRUE, has content, else it ended in '/>' and has no content
   PCEscError  pError - error object
returns
   DWORD - Num characters used total
*/
DWORD ParseElementDecl (WCHAR *psz, DWORD dwCount, DWORD *pdwType,
                        PWSTR *ppszName, DWORD *pdwNameCount,
                        PWSTR *ppszVariable, DWORD *pdwVariableCount,
                        BOOL *pfHasContent, PCEscError pError)
{
   DWORD i = 0;

   *pdwType = *pdwNameCount = *pdwVariableCount = 0;
   *ppszName = *ppszVariable = 0;
   *pfHasContent = TRUE;

   // test angle bracket
   if ((psz[i] != L'<') || (dwCount<3)) {
      pError->Set (6, gaszErrorText[6], psz, i, psz + i);
      return 0;
   }
   i++;

   // does it have a questionmark or excalamtion next?
   if (psz[i] == L'?') {
      i++;
      *pdwType = MMLCLASS_PARSEINSTRUCTION;
   }
   else if (psz[i] == L'!') {
      i++;
      *pdwType = MMLCLASS_MACRO;
   }
   else {
      // don't increase i here
      *pdwType = MMLCLASS_ELEMENT;
   }

   // get the name
   *pdwNameCount = ParseName (psz + i, dwCount - i, pError);
   *ppszName = psz + i;
   i += *pdwNameCount;
   if (pError->m_dwNum)
      return 0;
   
   // skip whitepsace
   i += ParseWhitespace (psz + i, dwCount - i, pError);

   *ppszVariable = psz+i;

   // look until there's an angle bracket
   DWORD j;
   for (j = i; j < dwCount; j++)
      if (psz[j] == L'>')
         break;
   if (j >= dwCount) {
      pError->Set (6, gaszErrorText[6], psz, i, psz + i);
      return 0;
   }

   // set the variable count. See if a ? or / precedes the andle bracket, indicatin
   // that there's no content
   *pdwVariableCount = j - i;
   if ((*pdwType == MMLCLASS_PARSEINSTRUCTION) && (psz[j-1] == L'?')) {
      (*pdwVariableCount)--;
      *pfHasContent = FALSE;
   }
   else if (psz[j-1] == L'/') {
      (*pdwVariableCount)--;
      *pfHasContent = FALSE;
   }

   return j+1; // total characters used
}


/********************************************************************
ParseElementAttributes - Parse all the attributes within the element

inptus
   WCHAR    *psz - Start of the attributes section
   DWORD    dwCount - number of characters int he attributes section
   PCMMLNode   pNode - Node to add the attributes to
   PCEscError  pError - error object
returns
   BOOL - TRUE if success
*/
BOOL ParseElementAttributes (WCHAR *psz, DWORD dwCount, PCMMLNode pNode, PCEscError pError)
{
   DWORD i = 0;

   while (i < dwCount) {
      // skip whitespace
      i += ParseWhitespace (psz + i, dwCount - i, pError);
      if (i >= dwCount)
         break;

      // parse out the variable name
      DWORD dwNameStart, dwNameCount;
      dwNameStart = i;
      dwNameCount = ParseName (psz + i, dwCount - i, pError);
      if (!dwNameCount) {
         if (!pError->m_dwNum)
            pError->Set (4, gaszErrorText[4], psz, i, psz + i);
         return FALSE;
      }
      i += dwNameCount;
      
      // skip whitespace
      i += ParseWhitespace (psz + i, dwCount - i, pError);
      if (i >= dwCount) {
         pError->Set (6, gaszErrorText[5], psz, i, psz + i);
         return FALSE;
      }

      // expect and equals
      if (psz[i] != L'=') {
         pError->Set (6, gaszErrorText[5], psz, i, psz + i);
         return FALSE;
      }
      i++;

      i += ParseWhitespace (psz + i, dwCount - i, pError);

      // parse out variable value
      DWORD dwValueStart, dwValueCount;
      dwValueStart = i;
      dwValueCount = ParseVariableValue (psz + i, dwCount - i, pError);
      i += dwValueCount;
      if (!dwValueCount) {
         if (!pError->m_dwNum)
            pError->Set (4, gaszErrorText[4], psz, i, psz + i);
         return FALSE;
      }

      // interpret the value
      PWSTR pInterp;
      pInterp = ParseInterpVariableValue (psz + dwValueStart, dwValueCount, pError);
      if (!pInterp)
         return FALSE;

      // add it
      WCHAR c;
      BOOL fRet;
      c = psz[dwNameStart + dwNameCount];
      psz[dwNameStart + dwNameCount] = 0;
      fRet = pNode->AttribSet (psz + dwNameStart, pInterp);
      psz[dwNameStart + dwNameCount] = c;
      // free interp
      ESCFREE (pInterp);

      if (!fRet) {
         pError->Set (8, gaszErrorText[8], psz, i, psz + i);
         return FALSE;
      }
   }
   
   return TRUE;
}

/********************************************************************
ParseElement - Parses an element and produces a CMMLNode.

inputs
   WCHAR    *psz - start of the string. Pointing at a "<"
   DWORD    dwCount - left in the text string
   PCMMLNode   *ppNode - Filled in with a pointer to the node.
   PCEscError  pError - error object
returns
   DWORD - Num characters used totla
*/
DWORD ParseElement (WCHAR *psz, DWORD dwCount, PCMMLNode *ppNode, PCEscError pError)
{

   DWORD i = 0;
   *ppNode = NULL;

   // first, figure out where everything is in the element header
   DWORD dwType, dwNameCount, dwVariableCount;
   WCHAR *pszName, *pszVariable;
   BOOL  fHasContent;
   DWORD dwInfoSize;

   dwInfoSize = ParseElementDecl (psz + i, dwCount - i, &dwType,
                        &pszName, &dwNameCount,
                        &pszVariable, &dwVariableCount,
                        &fHasContent, pError);
   if (!dwInfoSize)
      return 0;   // some sort of error
   i += dwInfoSize;

   // create a new node
   CMMLNode *pNode;
   pNode = new CMMLNode();
   if (!pNode) {  // out of memory
      pError->Set (8, gaszErrorText[8], psz, i, psz + i);
      return FALSE;
   }
   pNode->m_dwType = dwType;

   // keep its name
   WCHAR c;
   c = pszName[dwNameCount];
   pszName[dwNameCount] = 0;
   pNode->NameSet (pszName);
   pszName[dwNameCount] = c;


   // do all the attributes
   if (!ParseElementAttributes (pszVariable, dwVariableCount, pNode, pError)) {
      delete pNode;
      return 0;
   }

   // if this element declaration is already terminated then we're done
   if (!fHasContent) {
      *ppNode = pNode;
      return i;
   }


   // figure out the end string
   CMem  memEnd;
   memEnd.Required (100);  // BUGFIX - Since have pretty good idea already
   memEnd.CharCat ('<');
   memEnd.CharCat ('/');
   memEnd.StrCat (pNode->NameGet());
   memEnd.CharCat ('>');
   memEnd.CharCat (0);
   WCHAR *pszEnd;
   pszEnd = (WCHAR*) memEnd.p;
   DWORD dwLenEnd;
   dwLenEnd = (DWORD)wcslen(pszEnd);

   // repeat, looking for content, a different node, or and end
   while (i < dwCount) {
      // end of element?
      if ((psz[i] == L'<') && ((i + dwLenEnd) <= dwCount)) {
         int   iRet;
         c = psz[i + dwLenEnd];
         psz[i+dwLenEnd] = 0;
         iRet = _wcsicmp (psz + i, pszEnd);
         psz[i+dwLenEnd] = c;

         if (!iRet) {
            *ppNode = pNode;
            return i + dwLenEnd;
         }

      }

      // if it's a left bracket then we've alredy checked for the
      // ending of this, so it must be a subselemnt
      DWORD dwUsed;
      if (psz[i] == L'<') {
         if ((i+1 < dwCount) && (psz[i+1] == L'/')) {
            delete pNode;
            pError->Set (15, gaszErrorText[15], psz, i, psz + i);
            return 0;
         }

         PCMMLNode   pSub;
//#ifdef _DEBUG
//         if (i == 712842) {
//            i = i;
//            _CrtCheckMemory ();
//         }
//#endif
         dwUsed = ParseElement (psz + i, dwCount - i, &pSub, pError);

         if (!dwUsed || !pSub) {
            // error
            delete pNode;
            return 0;
         }

         // else, add as a sub-node
         if (!pNode->ContentAdd (pSub)) {
            delete pSub;
            delete pNode;
            pError->Set (8, gaszErrorText[8], psz, i, psz + i);
            return 0;
         }

         // move on
         i += dwUsed;
         continue;
      }


      // else if it gets here it's a string
      dwUsed = ParseContentString (psz + i, dwCount - i, pError);
      if (!dwUsed) {
         delete pNode;
         pError->Set (9, gaszErrorText[9], psz, i, psz + i);
         return 0;
      }

      // convert it
      WCHAR *pszContent;
      pszContent = ParseInterpretContentString (psz + i, dwUsed, pError);
      if (!pszContent) {
         delete pNode;
         if (!pError->m_dwNum)
            pError->Set (9, gaszErrorText[9], psz, i, psz + i);
         return 0;
      }

      // add it
      if (!pNode->ContentAdd (pszContent)) {
         ESCFREE (pszContent);
         delete pNode;
         pError->Set (8, gaszErrorText[8], psz, i, psz + i);
         return 0;
      }

      // move on
      ESCFREE (pszContent);
      i += dwUsed;
   }

   // error if get here. Unexpected end
   delete pNode;
   pError->Set (14, gaszErrorText[14], psz, 0, psz);

   return 0;
}

/********************************************************************
ParsePureMML - Parse a pure MML file (which doesn't have any substitutions).
   It doesn't do anything with MACROS or INCLUDES.

  NOTE: Don't expect any content in the top layer!

inputs
   WCHAR    *psz - start of the string
   DWORD    dwCount - number of characters
   PCEscError  pError - error object
returns
   PCMMLNode - Top node.
*/
PCMMLNode ParsePureMML (WCHAR *psz, DWORD dwCount, PCEscError pError)
{
   DWORD i = 0;

   // create the top node
   PCMMLNode   pNode;
   pNode = new CMMLNode;
   if (!pNode) {
      pError->Set (8, gaszErrorText[8], psz, i, psz + i);
      return NULL;
   }
   pNode->m_dwType = MMLCLASS_ELEMENT;
   pNode->NameSet (L"<Main>");

   // repeat
   while (i < dwCount) {
      // skip whitespace
      i += ParseWhitespace (psz + i, dwCount - i, pError);
      if (i >= dwCount)
         break;

#if 0// def _DEBUG
   if (i >= 1490569) {
      // Get current flag
      int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

      // Turn on leak-checking bit
      tmpFlag |=  _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF;

      // Set flag to the new value
      _CrtSetDbgFlag( tmpFlag );
   }
#endif

   // expect an element
      DWORD dwUsed;
      PCMMLNode   pSub;
      dwUsed = ParseElement (psz + i, dwCount - i, &pSub, pError);
      if (pError->m_dwNum) {
         delete pNode;
         return NULL;
      }
      i += dwUsed;
      if (!pSub)
         continue;

      // add it
      if (!pNode->ContentAdd (pSub)) {
         delete pSub;
         delete pNode;
         pError->Set (8, gaszErrorText[8], psz, i, psz + i);
         return 0;
      }
   }

   // done
   return pNode;
}



/********************************************************************
MMLPreProcess - Preprocesses the unicode text string to remove
   substitutions.

inputs
   PWSTR       psz - string, null terminated
   ESCCALLBACK pCallback - Callback for substutions.
   PVOID       pCallbackParam - parameter for the callback
   PCEscError  pError - error object
returns
   PWSTR - string that's preprocessed. The caller must ESCFREE() this.
         NOTE: returns psz if there's nothing to preprocess, in which case dont free

*/
PWSTR MMLPreProcess (PWSTR psz, PVOID pCallback, PVOID pCallbackParam, PCEscError pError)
{
   PESCPAGECALLBACK pC = (PESCPAGECALLBACK) pCallback;
   PCEscPage pPage = (PCEscPage) pCallbackParam;

   // loop through it
   CMem  mem;
   PWSTR pCur;
   for (pCur = psz; *pCur; ) {
      // find <<<
      WCHAR *pNext;
      pNext = wcsstr (pCur, L"<<<");
      if (!pNext) {
         // all done
         if (pCur == psz)
            return psz; // nothing to process so just return htis

         mem.StrCat (pCur);
         break;
      }

      // else, cat up to pNext
      mem.StrCat (pCur, (DWORD)(((PBYTE) pNext - (PBYTE)pCur)/2));

      // skip the brackets
      pNext += 3;

      // else, see if can find end brackets
      WCHAR *pEnd;
      pEnd = wcsstr (pNext, L">>>");
      if (!pEnd) {
         pError->Set (10, gaszErrorText[10], psz, (DWORD)(((PBYTE)pNext - (PBYTE)pCur)/2), pNext);
         return NULL;
      }

      // see if it has a $
      BOOL  fLiteral;
      if (*pNext == L'$') {
         fLiteral = TRUE;
         pNext++;
      }
      else
         fLiteral = FALSE;

      // copy to string
      CMem  memsz;
      memsz.m_dwCurPosn = 0;
      memsz.StrCat (pNext, (DWORD)(((PBYTE) pEnd - (PBYTE) pNext)/2));
      memsz.CharCat (0);

      // BUGFIX - copy string to temporary memory instead of blanking out
      // temporarily blank out end
      //WCHAR c;
      //c = pEnd[0];
      //pEnd[0] = 0;
      if (pCallback) {
         ESCMSUBSTITUTION  s;
         s.fMustFree = FALSE;
         s.pszSubName = (WCHAR*) memsz.p; // BUGFIX pNext;
         s.pszSubString = NULL;

         pC (pPage, ESCM_SUBSTITUTION, &s);

         if (s.pszSubString) {
            WCHAR *pTrans = NULL;

            if (!fLiteral)
               pTrans = StringToMMLString(s.pszSubString);

            mem.StrCat (pTrans ? pTrans : s.pszSubString);
            if (s.fMustFree)
               HeapFree (s.hMemHandle, 0, s.pszSubString);
            if (pTrans)
               ESCFREE (pTrans);
         }
      }
      // else subsittue with nothing

//      pEnd[0] = c;

      // move on
      pCur = pEnd+3;
   }

   // append null
   mem.CharCat (0);

   // done
   PWSTR pszRet;
   pszRet = (PWSTR) mem.p;
   mem.p = NULL;
   return pszRet;
}

/********************************************************************
SubstituteContent - Replaces dwContent in pTemplate with all of the
content elements in pNode. The node, pNode, and not including it's
children, is then deleted.

inputs
   PCMMLNode      pTemplate - to modify
   DWORD          dwContent - index into the content to replace
   PCMMLNOde      pNode - replace with what
returns
   none
*/
void SubstituteContent (PCMMLNode pTemplate, DWORD dwContent, PCMMLNode pNode)
{
   // first off,delete the current content
   pTemplate->ContentRemove (dwContent);

   // go through pNode & insert
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   PWSTR psz;
   PCMMLNode pSub;
   for (i = dwNum-1; i < dwNum; i--) {
      if (pNode->ContentEnum (i, &psz, &pSub)) {
         if (psz)
            pTemplate->ContentInsert (dwContent, psz);
         else
            pTemplate->ContentInsert (dwContent, pSub);
      }

      // delete from pnode
      pNode->ContentRemove (i, FALSE);
   }

   //finally,delete pNode since nothing left
   delete pNode;
}


/********************************************************************
ReplaceTemplateSubst - Searches through the content nodes looking for
<?TemplateSubst?>. If it finds it then the contents are replaced
by the contents of pNode.

inputs
   PCMMLNode      pTemplate - Template
   PCMMLNoed      pNode - node
returns
   TRUE - if it has found a templatesubst. FALSE if not
*/
BOOL ReplaceTemplateSubst (PCMMLNode pTemplate, PCMMLNode pNode)
{
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pTemplate->ContentNum(); i++) {
      pSub = NULL;
      pTemplate->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // make sure it's a template
      if ((pSub->m_dwType == MMLCLASS_PARSEINSTRUCTION) && !_wcsicmp(pSub->NameGet(), L"TemplateSubst"))
         break;

      // else, didn't find, so check below
      if (ReplaceTemplateSubst (pSub, pNode))
         return TRUE;
   }

   if (i >= pTemplate->ContentNum())
      return FALSE;

   // else found it
   SubstituteContent (pTemplate, i, pNode);

   return TRUE;
}


/********************************************************************
IncludeRecurse - Searches through this node and sub nodes for <?Include?>
   and replaces the occurances with included files/resources.

inputs
   HINSTANCE   hInstance - for includes
   ESCCALLBACK pCallback - Callback for substutions.
   PVOID       pCallbackParam - parameter for the callback
   PCEscError  pError - error object
   PCMMLNode   pNode - node.
returns
   BOOL - TRUE if OK. FALSE if error

*/
BOOL IncludeRecurse (HINSTANCE hInstance, PVOID pCallback,
                    PVOID pCallbackParam, PCEscError pError, PCMMLNode pNode)
{
   // go through pNode & insert
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   PWSTR psz;
   PCMMLNode pSub;
   // go backwards so can insert the contents easier
   for (i = dwNum-1; i < dwNum; i--) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // make sure it's an include
      if ((pSub->m_dwType != MMLCLASS_PARSEINSTRUCTION) || _wcsicmp(pSub->NameGet(), L"Include")) {
         // its not an include so burrow in
         if (!IncludeRecurse (hInstance, pCallback, pCallbackParam, pError, pSub))
            return FALSE;
         continue;
      };

      // found one
      int   iRet;
      PWSTR pszFile;
      PWSTR pszUnicode;
      pszUnicode = NULL;
      if (AttribToDecimal(pSub->AttribGet (L"Resource"), &iRet))
         pszUnicode = ResourceToUnicode (hInstance, (DWORD) iRet);
      else if (pszFile = pSub->AttribGet (L"File")) {
         pszUnicode = FileToUnicode (pszFile);
      }

      // if no text then error
      if (!pszUnicode) {
         pError->Set (13, gaszErrorText[13]);
         return FALSE;
      }

      // parse this
      PCMMLNode   pTemplate;
      pTemplate = ParseMML (pszUnicode, hInstance, pCallback, pCallbackParam, pError, FALSE);
      ESCFREE (pszUnicode);
      if (!pTemplate) {
         return FALSE;
      }

      // replace the contens
      SubstituteContent (pNode, i, pTemplate);
   }

   // all done
   return TRUE;
}

/********************************************************************
ParseTemplate - Searches through the top MML node for a <?Template?>
parameter. If it doesn't find one then just returns pNode. If it finds
one then it loads the template from a resurce/file. It then goes through
that and finds <?TemplateSubst?> and replaces it with the contents of pNode.
It then returns the root node to the template.

inputs
   HINSTANCE   hInstance - for includes
   ESCCALLBACK pCallback - Callback for substutions.
   PVOID       pCallbackParam - parameter for the callback
   PCEscError  pError - error object
   PCMMLNode   pNode - node. If an error occurs an NULL is returned, this is deleted
returns
   PCMMLNode - new node

*/
PCMMLNode ParseTemplate (HINSTANCE hInstance, PVOID pCallback,
                    PVOID pCallbackParam, PCEscError pError, PCMMLNode pNode)
{
   // see if can find template
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // make sure it's a template
      if ((pSub->m_dwType != MMLCLASS_PARSEINSTRUCTION) || _wcsicmp(pSub->NameGet(), L"Template"))
         continue;

      // found it
      break;
   }
   if (i >= pNode->ContentNum())
      return pNode;  // found nothing

   // else have it in pSub
   int   iRet;
   PWSTR pszFile;
   PWSTR pszUnicode;
   pszUnicode = NULL;
   if (AttribToDecimal(pSub->AttribGet (L"Resource"), &iRet))
      pszUnicode = ResourceToUnicode (hInstance, (DWORD) iRet);
   else if (pszFile = pSub->AttribGet (L"File")) {
      pszUnicode = FileToUnicode (pszFile);
   }

   // if no text then error
   if (!pszUnicode) {
      pError->Set (11, gaszErrorText[11]);
      delete pNode;
      return NULL;
   }

   // parse this
   PCMMLNode   pTemplate;
   pTemplate = ParseMML (pszUnicode, hInstance, pCallback, pCallbackParam, pError, FALSE);
   ESCFREE (pszUnicode);
   if (!pTemplate) {
      delete pNode;
      return NULL;
   }

   // search through pTemplate for <?TemplateSubst?> and substitute.
   if (!ReplaceTemplateSubst (pTemplate, pNode)) {
      pError->Set (12, gaszErrorText[12]);
      delete pTemplate;
      delete pNode;
      return NULL;
   }

   // else dne
   return pTemplate;
}


/********************************************************************
MacroSubstitution - Looks through a cloned macro definition for:
   a) Attributes of MACROATTRIBUTE
   b) Content ndoes of <?MacroContent?>

Attribute info it gotten from pNode->AttribGet(). Content information
is from the content of pNode. Note that any content is cloned so pNode
is not affected.

inputs
   PCMMLNode   pClone - to modify
   PCMMLNode   pNode - source node
returns
   BOOL - TRUE if OK, else error
*/
BOOL MacroSubstitution (PCMMLNode pClone, PCMMLNode pNode)
{
   WCHAR szMacroAttribute[] = L"MACROATTRIBUTE";

   // look for the attribute
   if (pClone->AttribGet(szMacroAttribute)) {
      pClone->AttribDelete (szMacroAttribute);

      // enumerate everything in pNode and add
      PWSTR pszName, pszValue;
      DWORD i;
      for (i = 0; i < pNode->AttribNum(); i++) {
         if (!pNode->AttribEnum (i, &pszName, &pszValue))
            continue;
         pClone->AttribSet (pszName, pszValue);
      }
   }

   // look through content
   DWORD i, dwNum;
   dwNum = pClone->ContentNum();
   PWSTR psz;
   PCMMLNode   pSub;
   // go backwards so if convert dont get into trouble
   for (i = dwNum-1; i < dwNum; i--) {
      pSub = NULL;
      pClone->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // skip unless it's a macrosubst
      if ((pSub->m_dwType != MMLCLASS_PARSEINSTRUCTION) || _wcsicmp(pSub->NameGet(), L"MacroContent")) {
         // recurse into this
         if (!MacroSubstitution (pSub, pNode))
            return FALSE;
         continue;
      }

      // replace
      PCMMLNode   pCloneNode;
      pCloneNode = pNode->Clone();
      if (!pCloneNode) {
         return FALSE;
      }
      SubstituteContent (pClone, i, pCloneNode);
   }

   // done
   return TRUE;
}


/********************************************************************
ConvertMacro - Converts a macro. Converts it within the parent.

inputs
   PCEscError  pError - error
   PCMMLNode   pNode - node that's really a macro
   PCMMLNode   pDef- macro definition
   PCMMLNode   pParent - parent of pNode
   DWORD       dwIndex - index of content in pParent where pNode is.
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL ConvertMacro (PCEscError pError, PCMMLNode pNode, PCMMLNode pDef, PCMMLNode pParent,
                   DWORD dwIndex)
{
   // clone the definition because we'll be mucking with it
   PCMMLNode   pNew;
   pNew = pDef->Clone();
   if (!pNew) {
      pError->Set (8, gaszErrorText[8]);
      return FALSE;
   }

   // loop through the clone looking for substitutions
   if (!MacroSubstitution (pNew, pNode)) {
      delete pNew;
      return FALSE;
   }

   // replace the occurance of pNode with all the content elements of pNew. Note that
   // pNew->NameGet() is the macro name, and should not be put back in
   SubstituteContent (pParent, dwIndex, pNew);
   // pNew has been freed buy substitute content

   // done
   return TRUE;
}


/********************************************************************
ConvertMacros - Go through looking for occurances of macros and convert them.

inputs
   PCEscError  pError - error
   PCMMLNode   pNode - node
   PCBTree     pTree - Macro defintiions
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL ConvertMacros (PCEscError pError, PCMMLNode pNode, PCBTree pTree)
{
   // else, go through sub elemends
   DWORD i, dwNum;
   dwNum = pNode->ContentNum();
   PWSTR psz;
   PCMMLNode   pSub;
   // go backwards so if convert dont get into trouble
   for (i = dwNum-1; i < dwNum; i--) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // if it's macro definition then ignore
      if (pSub->m_dwType != MMLCLASS_ELEMENT)
         continue;

      // see if it's a macro
      PCMMLNode   *ppMacro;
      ppMacro = (PCMMLNode*) pTree->Find(pSub->NameGet());
      if (!ppMacro || !(*ppMacro)) {
         // it's not a macro, so recurse
         if (!ConvertMacros (pError, pSub, pTree))
            return FALSE;
         continue;
      }

      // else it is

      // because macros might be within macros, remember how many elements
      // there are now, and after convert macro. Then, realign so that we reparse
      // what's just been converted for more macrso
      DWORD dwPre;
      dwPre = pNode->ContentNum();

      PCMMLNode   pMacro;
      pMacro = *ppMacro;
      // convert
      if (!ConvertMacro (pError, pSub, pMacro, pNode, i))
         return FALSE;

      // how many nodes now?
      dwNum = pNode->ContentNum();

      // increase i by the delta
      i += (dwNum + 1 - dwPre);
         // hence - if 1 for 1 replacement, we'll retest our results, because i+=1
         // if replace macro with 0 elements, then don't change i

   }


   // done
   return TRUE;
}

/********************************************************************
LearnMacroDeinfition - recursive function that looks for a macro definition
   and remembers it.

inputs
   PCEscError  pError - error
   PCMMLNode   pNode - node
   PCBTree     pTree - Macro defintiions are added
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL LearnMacroDefinition (PCEscError pError, PCMMLNode pNode, PCBTree pTree)
{
   if (pNode->m_dwType == MMLCLASS_MACRO) {
      // this is a macro definition
      pTree->Add (pNode->NameGet(), &pNode, sizeof(PCMMLNode));
      return TRUE;

      // NOTE: Cannot have macro definitions within macro definitions
   }

   // else, go through sub elemends
   DWORD i;
   PWSTR psz;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // recurse
      if (!LearnMacroDefinition (pError, pSub, pTree))
         return FALSE;
   }

   // done
   return TRUE;

}

/********************************************************************
InterpretMacros - Looks through the pNode for macro defintions and
leans them. Then looks through again and converts macros.

inputs
   PCEscError  pError - error
   PCMMLNode   pNode - node
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL InterpretMacros (PCEscError pError, PCMMLNode pNode)
{
   CBTree treeMacro;

   // learn macro definitions
   if (!LearnMacroDefinition (pError, pNode, &treeMacro))
      return FALSE;

   // if no macro definitions then a fast exit
   if (!treeMacro.Num())
      return TRUE;

   // convert
   return ConvertMacros (pError, pNode, &treeMacro);
}


/********************************************************************
ParseMML - Parses MML. This:
   1) Preprocesses.
   2) Parses the pure stuff.
   3) Handles includes.
   4) Handles macros.

inputs
   PWSTR       psz - string, null terminated
   HINSTANCE   hInstance - for includes
   ESCCALLBACK pCallback - Callback for substutions.
   PVOID       pCallbackParam - parameter for the callback
   PCEscError  pError - error object
   BOOL        fDoMacros - if TRUE, interpret/convert macros. if FALSE leave them in
returns
   PCMMLNode - main node

*/
PCMMLNode ParseMML (PWSTR psz, HINSTANCE hInstance, PVOID pCallback,
                    PVOID pCallbackParam, PCEscError pError, BOOL fDoMacros)
{
   // preprocess
   PWSTR pszPre;
   pszPre = MMLPreProcess (psz, pCallback, pCallbackParam, pError);
   if (!pszPre)
      return NULL;
   
   // parse the purse stuff
   PCMMLNode   pNode;
   pNode = ParsePureMML (pszPre, (DWORD)wcslen(pszPre), pError);
   if (pszPre != psz)
      ESCFREE (pszPre);
   if (!pNode)
      return NULL;

   // handle templates
   pNode = ParseTemplate (hInstance, pCallback, pCallbackParam, pError, pNode);
   if (!pNode)
      return FALSE;

   // handle includes
   if (!IncludeRecurse (hInstance, pCallback, pCallbackParam, pError, pNode)) {
      delete pNode;
      return NULL;
   }

   // handle macros
   // only handle macros for parse from page. Dont do for includes & templates
   if (fDoMacros)
      if (!InterpretMacros (pError, pNode)) {
            delete pNode;
            return NULL;
         }

   return pNode;
}




/******************************************************************************
EscRemapMML - Given a resource ID specified in a MML file, this causes any
access to the resouce to be remapped to a file on disk. Useful for debugging in
the test applicaton.

inputs
   DWORD    dwID - resource ID
   PWSTR    pszFile - file name to remap to. If NULL, then deletes previous occurance
returns
   none
*/
void EscRemapMML (DWORD dwID, PWSTR pszFile)
{
   // if it exists already then dlete it
   DWORD i;
   for (i = 0; i < glistMML.Num(); i++) {
      DWORD *pdw = (DWORD*) glistMML.Get(i);
      if (*pdw == dwID) {
         glistMML.Remove(i);
         break;
      }
   }

   // if null then don't add
   if (!pszFile)
      return;

   // else add
   BYTE     abTemp[512];
   memcpy (abTemp, &dwID, sizeof(dwID));
   wcscpy ((WCHAR*) (abTemp + sizeof(DWORD)), pszFile);
   glistMML.Add (abTemp, sizeof(DWORD) + (wcslen(pszFile)+1)*2);
}


// BUGBUG - Make MML nodes faster by including the name string as an array of [12]
// chars, unless it's longer, then use memory

// BUGBUG - make MML nodes faster by using linear search for attrib name instead
// of tree