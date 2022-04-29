/***************************************************************************
Phonemes.cpp - Code for phoneme reference

begun 13/5/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"

#define SILENCE      L"<s>"

static WCHAR gszSilence[4] = SILENCE;

static PHONEMEINFO gaPhonemeInfo[] = { // NOTE: Must be alphabetical
   PIC_MISC, SILENCE, L"silence", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED,
   PIC_VOWEL, L"aa\0", L"fAther", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MAX,
   PIC_VOWEL, L"ae\0", L"At", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_MAX | PIS_VERTOPN_SLIGHT,
   PIC_VOWEL, L"ah\0", L"cUt", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MID,
   PIC_VOWEL, L"ao\0", L"lAW", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_FULL | PIS_LATTEN_REST | PIS_VERTOPN_MAX,
   PIC_VOWEL, L"aw\0", L"Out", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_FULL | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MAX,
   PIC_VOWEL, L"ax\0", L"About", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MID,
   PIC_VOWEL, L"ay\0", L"tIE", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MAX,
   PIC_CONSONANT, L"b\0\0", L"Bit", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_KEEPSHUT | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED,
   PIC_CONSONANT, L"ch\0", L"CHin", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"d\0\0", L"Dip", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"dh\0", L"THis", PIS_TONGUEFRONT_TEETH | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_TEETHBOT_MID | PIS_LATTEN_REST | PIS_VERTOPN_SLIGHT,
   PIC_VOWEL, L"eh\0", L"bEt", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_FULL | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MID,
   PIC_VOWEL, L"er\0", L"hUrt", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID,
   PIC_VOWEL, L"ey\0", L"AId", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_TEETH | PIS_TEETHBOT_MID | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"f\0\0", L"Fat", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_KEEPSHUT | PIS_TEETHTOP_FULL |PIS_LATTEN_SLIGHT | PIS_VERTOPN_CLOSED,
   PIC_CONSONANT, L"g\0\0", L"Give", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"h\0\0", L"Hit", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MAX,
   PIC_VOWEL, L"ih\0", L"bIt", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MID,
   PIC_VOWEL, L"iy\0", L"bEET", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"jh\0", L"Joy", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"k\0\0", L"Kiss", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_LATTEN_MAX | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"l\0\0", L"Lip", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_LATTEN_REST | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"m\0\0", L"Map", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_KEEPSHUT | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED,
   PIC_CONSONANT, L"n\0\0", L"Nip", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"nx\0", L"kiNG", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_TEETHBOT_FULL | PIS_LATTEN_MAX | PIS_VERTOPN_CLOSED,
   PIC_VOWEL, L"ow\0", L"tOE", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID,
   PIC_VOWEL, L"oy\0", L"tOY", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID,
   PIC_CONSONANT, L"p\0\0", L"Pin", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_KEEPSHUT | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED,
   PIC_CONSONANT, L"r\0\0", L"Red", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"s\0\0", L"Sip", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_TEETH | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"sh\0", L"SHe", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_FULL | PIS_TEETHBOT_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"t\0\0", L"Talk", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED,
   PIC_CONSONANT, L"th\0", L"THin", PIS_TONGUEFRONT_TEETH | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT,
   PIC_VOWEL, L"uh\0", L"fOOt", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID,
   PIC_VOWEL, L"uw\0", L"fOOd", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_TEETHBOT_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MAX,
   PIC_CONSONANT, L"v\0\0", L"Vat", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_FULL | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED,
   PIC_CONSONANT, L"w\0\0", L"Wit", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT,
   PIC_CONSONANT, L"y\0\0", L"Yet", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_LATTEN_MAX | PIS_VERTOPN_MID,
   PIC_CONSONANT, L"z\0\0", L"Zip", PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_TEETHBOT_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_CLOSED,
   PIC_CONSONANT, L"zh\0", L"aZure", PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_CLOSED
};

/*************************************************************************
PhonemeSilence - Returns a pointer to the string (which is 4 bytes long)
for the silence phoneme
*/
WCHAR *PhonemeSilence (void)
{
   return gszSilence;
}


/*************************************************************************
PhonemeFind - Given a phoneme string, this finds the index to it.

inputs
   WCHAR        *pszName - Phoneme name
returns
   DWORD - Index, or -1 if cant find
*/
static int __cdecl PHONEMEINFOCompare (const void *p1, const void *p2)
{
   PPHONEMEINFO pp1 = (PPHONEMEINFO) p1;
   PPHONEMEINFO pp2 = (PPHONEMEINFO) p2;
   int iRet = wcsnicmp(pp1->szPhone, pp2->szPhone, sizeof(pp1->szPhone)/sizeof(WCHAR));
   return iRet;
}

DWORD PhonemeFind (WCHAR *pszName)
{
   PHONEMEINFO pi;
   wcscpy (pi.szPhone, pszName);  // can overrun a bit because followed by sample string

   PPHONEMEINFO ppi;
   ppi = (PPHONEMEINFO) bsearch (&pi, gaPhonemeInfo, PhonemeNum(), sizeof(PHONEMEINFO), PHONEMEINFOCompare);
   if (!ppi)
      return -1;
   return (DWORD)((PBYTE)ppi - (PBYTE)&gaPhonemeInfo[0]) / sizeof(PHONEMEINFO);
}


/*************************************************************************
PhonemeGet - Returns the phoneme information for a phoneme based on the
index.

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-1
returns
   PPHONEMEINFO  - information. Do NOT change th einfor
*/
PPHONEMEINFO PhonemeGet (DWORD dwIndex)
{
   if (dwIndex >= PhonemeNum())
      return NULL;

   return &gaPhonemeInfo[dwIndex];
}


/*************************************************************************
PhonemeGet - Returns the phoneme information for a phoneme based on the
string.

inputs
   char        *pszName - Phoneme name
returns
   PPHONEMEINFO  - information. Do NOT change th einfor
*/
PPHONEMEINFO PhonemeGet (WCHAR *pszName)
{
   DWORD dwIndex = PhonemeFind (pszName);
   return PhonemeGet (dwIndex);
}

/*************************************************************************
PhonemeNum - Returns the number of phonemes
*/
DWORD PhonemeNum (void)
{
   return sizeof(gaPhonemeInfo) / sizeof(PHONEMEINFO);
}

/*************************************************************************
PhonemeLetterToPhonemes - Given a letter (or letters) this fills in
a buffer with the possible phoneme numbers that it can become.

inputs
   PWSTR          psz - Pointer to the first string
   DWORD          dwChars - Number of chars to check
   DWORD          *padwPhoneIndex - Pointer to an index where fills in phonemes
                  NOTE: Uses phoneme 1000 to indicate skip (but not silent).
                  If this is multiple phonemes in a row (such as q = k w) then the
                  phonemes will have the high-bit set until the last one. hence, k
                  would have the high bit set, but not w
   DWORD          dwNumLeft - Number of entries in padwPhoneIndex that can fill in
returns
   DWORD - Number of phonemes filled in. This might be 0
*/
DWORD PhonemeLetterToPhonemes (PWSTR psz, DWORD dwChars, DWORD *padwPhoneIndex, DWORD dwNumLeft)
{
   static BOOL fLoaded = FALSE;
   static CBTree Tree;

   // if not loaded the open
   if (!fLoaded) {
      fLoaded = TRUE;
      Tree.m_fIgnoreCase = TRUE;

      FILE *f;
      char szTemp[256];
      strcpy (szTemp, gszAppDir);
      strcat (szTemp, "TextToPhone.txt");
      f = fopen (szTemp, "rt"); dead code
      if (!f)
         return 0;   // no info

      DWORD adwPhonemes[256];
      DWORD dwCount;

      while (fgets(szTemp, sizeof(szTemp), f)) {
         // find the first space, indicating the phoneme
         char *pChars;
         for (pChars = szTemp; pChars[0] && !iswspace (pChars[0]); pChars++);
         if (!pChars[0])
            continue;   // empty line
         pChars[0] = 0;
         pChars++;

         dwCount = 0;
         while (pChars[0]) {
            // skip white space
            while (pChars[0] && !(isalpha(pChars[0]) || (pChars[0] == '.')) )
               pChars++;
            if (!pChars[0])
               break;   // EOF

            // find the end
            char *pEnd;
            for (pEnd = pChars + 1; isalpha(pEnd[0]) || (pEnd[0] == '.'); pEnd++);

            // phoneme match
            char cTemp;
            cTemp = *pEnd;
            *pEnd = 0;
            WCHAR szwTemp[128];
            MultiByteToWideChar (CP_ACP, 0, pChars, -1, szwTemp, sizeof(szwTemp)/sizeof(WCHAR));
            adwPhonemes[dwCount] = PhonemeFind (szwTemp);
            *pEnd = cTemp;

            // if the end is a ',' then continue on
            BOOL fContinue;
            fContinue = (cTemp == ',');

            // if was negative one then accept only if it's a ".", which indicates
            // that can be skipped entirely
            if (adwPhonemes[dwCount] == -1) {
               if ((pChars[0] == '.') && (pEnd == pChars+1)) {
                  adwPhonemes[dwCount] = 1000;
                  dwCount++;
               }
               else
                  fContinue = FALSE;
            }
            else
               dwCount++;
            if (fContinue)
               adwPhonemes[dwCount-1] |= 0x80000000;

            // continue
            pChars = pEnd;
         }

         // convert to unicode
         WCHAR szw[256];
         MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);

         // add to tree
         if (dwCount)
            Tree.Add (szw, &adwPhonemes[0], dwCount * sizeof(DWORD));
      } // while fgets()
      fclose (f);
   } // if not loaded

   // try to find the character
   WCHAR szTemp[128];
   memcpy (szTemp, psz, dwChars * sizeof(WCHAR));
   szTemp[dwChars] = 0;

   DWORD *padw;
   DWORD dwSize;
   padw = (DWORD*) Tree.Find (szTemp, &dwSize);
   if (!padw || (dwSize * sizeof(DWORD) > dwNumLeft))
      return 0;

   // copy over
   memcpy (padwPhoneIndex, padw, dwSize);
   return dwSize / sizeof(DWORD);
}




/*************************************************************************
PhonemeDictionary - Given a set of letters that are limited to an entire word,
this looks it up in the dictionary.

inputs
   PWSTR          psz - Pointer to the first string
   DWORD          dwChars - Number of chars to check
   DWORD          *padwPhoneIndex - Pointer to an index where fills in phonemes
                  NOTE: Uses phoneme 1000 to indicate skip (but not silent).
                  If this is multiple phonemes in a row (such as q = k w) then the
                  phonemes will have the high-bit set until the last one. hence, k
                  would have the high bit set, but not w
   DWORD          dwNumLeft - Number of entries in padwPhoneIndex that can fill in
returns
   DWORD - Number of phonemes filled in. This might be 0
*/
DWORD PhonemeDictionary (PWSTR psz, DWORD dwChars, DWORD *padwPhoneIndex, DWORD dwNumLeft)
{
   static BOOL fLoaded = FALSE;
   static CBTree Tree;
   if (dwChars > 64)
      return 0;  // too large

   // if not loaded the open
   if (!fLoaded) {
      fLoaded = TRUE;
      Tree.m_fIgnoreCase = TRUE;

      FILE *f;
      char szTemp[512];
      strcpy (szTemp, gszAppDir);
      strcat (szTemp, "cmudict.txt");
      f = fopen (szTemp, "rt"); dead code
      if (!f)
         return 0;   // no info

#if 0 // HACK to randomize CMU dictionary so loads acceptably fast into tree
      CListVariable lv;
      while (fgets(szTemp, sizeof(szTemp), f)) {
         lv.Add (szTemp, strlen(szTemp)+1);
      }
      fclose (f);
      fopen ("c:\\out.txt", "wt");
      while (lv.Num()) {
         DWORD dwIndex = rand() % lv.Num();
         fputs ((char*)lv.Get(dwIndex), f);
         lv.Remove (dwIndex);
      }
      fclose (f);
#endif

      DWORD adwPhonemes[256];
      DWORD dwCount;
      char *pEnd;

      while (fgets(szTemp, sizeof(szTemp), f)) {
         if (szTemp[0] == '#')
            continue;   // comment

         // find the first space, indicating the phoneme
         char *pChars;
         for (pChars = szTemp; pChars[0] && !iswspace (pChars[0]); pChars++);
         if (!pChars[0])
            continue;   // empty line
         pChars[0] = 0;
         pChars++;

         // the name ends with a bracket and a number
         pEnd = szTemp + (strlen(szTemp) - 1);
         if ((pChars >= szTemp+3) && (pEnd[0] == ')') && (pEnd[-2] == '(') &&
            (pEnd[-1] >= '1') && (pEnd[-1] <= '9')) {
               pEnd[-2] = 0;  // so have multiple pronunciations
            }

         // convert to unicode
         WCHAR szw[256];
         MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);

         // if there's already a pronduncaiton get it
         DWORD *padwExist;
         DWORD dwSize;
         dwCount = 0;
         padwExist = (DWORD*)Tree.Find (szw, &dwSize);
         if (padwExist) {
            dwCount = dwSize / sizeof(DWORD);
            memcpy (adwPhonemes, padwExist, dwSize);
         }

         // convert all numbers (phoneme stresses) to spaces
         for (pEnd = pChars; *pEnd; pEnd++)
            if ((pEnd[0] >= '0') && (pEnd[0] <= '9'))
               pEnd[0] = ' ';

         while (pChars[0]) {
            // skip white space
            while (pChars[0] && !isalpha(pChars[0]) )
               pChars++;
            if (!pChars[0])
               break;   // EOF

            // find the end
            for (pEnd = pChars + 1; isalpha(pEnd[0]); pEnd++);

            // phoneme match
            char cTemp;
            char *pszUse;
            cTemp = *pEnd;
            *pEnd = 0;
            // convert
            pszUse = pChars;
            if (!stricmp(pszUse, "hh"))
               pszUse = "h";
            else if (!stricmp(pszUse, "ng"))
               pszUse = "nx";
            WCHAR szwTemp[128];
            MultiByteToWideChar (CP_ACP, 0, pszUse, -1, szwTemp, sizeof(szwTemp)/sizeof(WCHAR));
            adwPhonemes[dwCount] = PhonemeFind (szwTemp);
            *pEnd = cTemp;

            // if was negative one shouldn have happend
            if (adwPhonemes[dwCount] == -1)
               continue;

            // else, increase
            dwCount++;
            adwPhonemes[dwCount-1] |= 0x80000000;  // so know continuation

            // continue
            pChars = pEnd;
         }

         // set last pronunciation so not continuation
         if (dwCount)
            adwPhonemes[dwCount-1] &= ~0x80000000;


         // add to tree
         if (dwCount)
            Tree.Add (szw, &adwPhonemes[0], dwCount * sizeof(DWORD));
      } // while fgets()
      fclose (f);
   } // if not loaded

   // try to find the character
   WCHAR szTemp[128];
   memcpy (szTemp, psz, dwChars * sizeof(WCHAR));
   szTemp[dwChars] = 0;

   DWORD *padw;
   DWORD dwSize;
   padw = (DWORD*) Tree.Find (szTemp, &dwSize);
   if (!padw || (dwSize > dwNumLeft * sizeof(DWORD)))
      return 0;

   // copy over
   memcpy (padwPhoneIndex, padw, dwSize);
   return dwSize / sizeof(DWORD);
}
