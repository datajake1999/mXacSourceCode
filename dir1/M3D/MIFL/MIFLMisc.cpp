/*************************************************************************************
MIFLMisc.cpp - Misc code for mifl

begun 27/12/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"



#define MENUWIDTHSCALE(x)  ((x) * 3 / 2)     // BUGFIX - make menus longer so can see more text
#define MENUROWS           ((DWORD)MENUWIDTHSCALE(fSmallText ? (fVerySmallText ? 25 : 20) : 15))
#define MENUCOLUMNWIDTH    ((DWORD)MENUWIDTHSCALE(fSmallText ? (fVerySmallText ? 75 : 100) : 150))      // number of pixels per menu column
#define TOOMANYITEMS       50
#define WAYTOOMANYITEMS    (TOOMANYITEMS*2)

#define VMTOK_NAME               4        // name of the object... used when converting object to string
         // BUGFIX - Had changed VMTOK_NAME elsewhere from 3 to 4

// LANGIDTONAME - Convert from language ID to name
typedef struct {
   LANGID            lid;     // language ID
   PWSTR             psz;     // string for language ID
} LANGIDTONAME, *PLANGIDTONAME;


/* globals */
static LANGIDTONAME gaLangIDToName[] = {
	1078, L"Afrikaans",
	1052, L"Albanian",
	1025, L"Arabic (Saudi Arabia)",
	2049, L"Arabic (Iraq)",
	3073, L"Arabic (Egypt)",
	4097, L"Arabic (Libya)",
	5121, L"Arabic (Algeria)",
	6145, L"Arabic (Morocco)",
	7169, L"Arabic (Tunisia)",
	8193, L"Arabic (Oman)",
	9217, L"Arabic (Yemen)",
	10241, L"Arabic (Syria)",
	11265, L"Arabic (Jordan)",
	12289, L"Arabic (Lebanon)",
	13313, L"Arabic (Kuwait)",
	14337, L"Arabic (U.A.E.)",
	15361, L"Arabic (Bahrain)",
	16385, L"Arabic (Quatar)",
	1068, L"Armenian",
	1068, L"Azeri (Latin)",
	2092, L"Azeri (Cyrillic)",
	1069, L"Basque",
	1059, L"Belarusian",
	1026, L"Bulgarian",
	1109, L"Burmese",
	1027, L"Catlan",
	1028, L"Chinese (Taiwan)",
	2052, L"Chinese (PRC)",
	3076, L"Chinese (Hong Kong SAR, PRC)",
	4100, L"Chinese (Singapore)",
	5124, L"Chinese (Macau SAR)",
	1050, L"Croatian",
	1029, L"Czech",
	1030, L"Danish",
	1125, L"Divehi",
	1043, L"Dutch (Netherlands)",
	2067, L"Dutch (Belgium)",
	1033, L"English (United States)",
	2057, L"English (United Kinddom)",
	3081, L"English (Australian)",
	4105, L"English (Canadian)",
	5129, L"English (New Zealand)",
	6153, L"English (Ireland)",
	7177, L"English (South Africa)",
	8201, L"English (Jamaica)",
	9225, L"English (Caribbean)",
	10249, L"English (Belize)",
	11273, L"English (Trinidad)",
	12297, L"English (Zimbabwe)",
	13321, L"English (Philippines)",
	1061, L"Estonian",
	1081, L"Faeroese",
	1065, L"Farsi",
	1045, L"Finnish",
	1036, L"French (Standard)",
	2060, L"French (Belgian)",
	3084, L"French (Canadian)",
	4108, L"French (Switzerland)",
	5132, L"French (Luxembourg)",
	6156, L"French (Monaco)",
	1110, L"Galician",
	1079, L"Georgian",
	1031, L"German (Standard)",
	2055, L"German (Switzerland)",
	3079, L"German (Austria)",
	4103, L"German (Luxembourg)",
	5127, L"German (Liechtenstein)",
	1032, L"Greek",
	1095, L"Gujarati",
	1037, L"Hebrew",
	1081, L"Hindi",
	1038, L"Hungarian",
	1039, L"Icelandic",
	1057, L"Indonesian",
	1040, L"Italian (Standard)",
	2064, L"Italian (Switzerland)",
	1041, L"Japanese",
	1099, L"Kannada",
	1111, L"Konkani",
	1042, L"Korean",
	2066, L"Korean (Johab)",
	1088, L"Kyrgyz",
	1062, L"Latvian",
	1063, L"Lithuanian",
	2087, L"Lithuanian (Classic)",
	1071, L"Macedonian",
	1086, L"Malay (Malaysian)",
	2110, L"Malay (Bruinei Darussalam)",
	1102, L"Marathi",
	1104, L"Mongolian",
	1044, L"Norwegian (Bokmal)",
	2068, L"Norwegian (Nynorsk)",
	1045, L"Polish",
	1046, L"Portuguese (Brazil)",
	2070, L"Portuguese (Portugal)",
	1094, L"Punjabi",
	1048, L"Romanian",
	1049, L"Russian",
	1103, L"Sanskrit",
	3098, L"Serbian (Cyrillic)",
	2074, L"Serbian (Latin)",
	1051, L"Slovak",
	1060, L"Slovenian",
	1034, L"Spanish (Spain, Traditional Sort)",
	2058, L"Spanish (Mexican)",
	3082, L"Spanish (Spain, International Sort)",
	4106, L"Spanish (Guatemala)",
	5130, L"Spanish (Costa Rica)",
	6154, L"Spanish (Panama)",
	7178, L"Spanish (Dominican Republic)",
	8202, L"Spanish (Venezuela)",
	9226, L"Spanish (Colombia)",
	10250, L"Spanish (Peru)",
	11274, L"Spanish (Argentina)",
	12298, L"Spanish (Ecuador)",
	13322, L"Spanish (Chile)",
	14346, L"Spanish (Uruguay)",
	15370, L"Spanish (Paraguay)",
	16394, L"Spanish (Bolivia)",
	17418, L"Spanish (El Salvador)",
	18442, L"Spanish (Honduras)",
	19466, L"Spanish (Nicaragua)",
	20490, L"Spanish (Puerto Rico)",
	1072, L"Sutu",
	1089, L"Swahili (Kenya)",
	1053, L"Swedish",
	2077, L"Swedish (Findland)",
	1114, L"Syriac",
	1097, L"Tamil",
	1092, L"Tatar (Tatarstan)",
	1098, L"Telugu",
	1054, L"Thai",
	1055, L"Turkish",
	1058, L"Ukranian",
	1046, L"Urdu (Pakistan)",
	2080, L"Urdu (India)",
	1091, L"Uzbek (Latin)",
	2115, L"Uzbek (Cyrillic)",
	1066, L"Vietnamese"
};


/*********************************************************************************
MIFLLangNum - Returns the number of languages.
*/
DWORD MIFLLangNum (void)
{
   return sizeof(gaLangIDToName) / sizeof(LANGIDTONAME);
}

/*********************************************************************************
MIFLLangGet - Returns the language string and ID based on an index

inputs
   DWORD          dwIndex - From 0..MIFLLangNum()-1
   LANGID         *pLang - Filled with the LangID (if valid index)
returns
   PWSTR - String. Do NOT change this
*/
PWSTR MIFLLangGet (DWORD dwIndex, LANGID *pLang)
{
   if (dwIndex >= MIFLLangNum())
      return NULL;

   if (pLang)
      *pLang = gaLangIDToName[dwIndex].lid;

   return gaLangIDToName[dwIndex].psz;
}


/*********************************************************************************
MIFLLangFind - Given the language ID, returns the index

inputs
   LANGID         lid - Language ID looking for
returns
   DWORD - index
*/
DWORD MIFLLangFind (LANGID lid)
{
   DWORD i;
   DWORD dwNum = MIFLLangNum();
   for (i = 0; i < dwNum; i++)
      if (gaLangIDToName[i].lid == lid)
         return i;

   return -1;
}


/*********************************************************************************
MIFLLangComboBoxSet - Fills a combobox with a list of languages, and sets the
given language.

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control
   LANGID         lid - Language
   PCMIFLProj     pProj - Project
returns
   BOOL - TRUE if set
*/
DLLEXPORT BOOL MIFLLangComboBoxSet (PCEscPage pPage, PWSTR pszControl, LANGID lid,
                                    PCMIFLProj pProj)
{
   CMem mem;

   // clear the existing combo
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;
   pControl->Message (ESCM_COMBOBOXRESETCONTENT);

   MemZero (&mem);

   DWORD i;
   LANGID *pl = (LANGID*)pProj->m_lLANGID.Get(0);
   DWORD dwSel = 0;
   for (i = 0; i < pProj->m_lLANGID.Num(); i++) {
      if (lid == pl[i])
         dwSel = i;
      DWORD dwIndex = MIFLLangFind (pl[i]);
      PWSTR psz = MIFLLangGet (dwIndex, NULL);
      if (!psz)
         psz = L"Unknown";
      
      MemCat (&mem, L"<elem name=");
      MemCat (&mem, (int)i);
      MemCat (&mem, L"><bold>");
      MemCatSanitize (&mem, psz);
      MemCat (&mem, L"</bold>");
      MemCat (&mem, L"</elem>");
   }

   ESCMCOMBOBOXADD lba;
   memset (&lba, 0,sizeof(lba));
   lba.pszMML = (PWSTR)mem.p;

   pControl->Message (ESCM_COMBOBOXADD, &lba);

   pControl->AttribSetInt (CurSel(), (int)dwSel);

   return TRUE;
}

/*********************************************************************************
LibNameExtract - Given a library, extracts a name that's can be
passed into MemCatSanMaxlen.

inputs
   PCMIFLLib         pLib - Library
   PCMem             pMem - Filled with the string
returns
   none
*/
void LibNameExtract (PCMIFLLib pLib, PCMem pMem)
{
   MemZero (pMem);

   PWSTR pszName = LibraryDisplayName ((PWSTR)pLib->m_szFile);
   if (!pszName)
      return;

   MemCat (pMem, pszName);
   pszName = (PWSTR)pMem->p;
   size_t dwLen = wcslen(pszName);
   PCWSTR pszEnd = L".mfl";
   size_t dwLenEnd = wcslen(pszEnd);
   if ((dwLen >= dwLenEnd) && !_wcsicmp(pszName + (dwLen - dwLenEnd), pszEnd))
      pszName[dwLen-dwLenEnd] = 0;  // get rid of

}


/*********************************************************************************
MemCatSanMaxLen - Concatenate a string, with a maximum length.

inputs
   PCMem       pMem - To concatenate to
   PWSTR       psz - String to append
   PWSTR       pszReplaceWithEllipsis - If this target string is found in
                  psz then it's replaced by ellipsis so that
                  the strings aren't so long. Can be NULL.
returns
   none
*/
void MemCatSanMaxLen (PCMem pMem, PWSTR psz, PCWSTR pszReplaceWithEllipsis)
{
   PCWSTR pszFind;
   CMem memTemp;
   if (pszReplaceWithEllipsis && pszReplaceWithEllipsis[0] && (pszFind = MyStrIStr(psz, pszReplaceWithEllipsis))) {
      size_t dwLenPSZ = wcslen(psz);
      size_t dwLenReplace = wcslen(pszReplaceWithEllipsis);
      PWSTR pszEllipsis = L"...";
      size_t dwLenEllipsis = wcslen(pszEllipsis);
      if (!memTemp.Required ((dwLenPSZ + dwLenReplace + dwLenEllipsis + 1) * sizeof(WCHAR)))
         return;  // error

      PWSTR pszNew = (PWSTR)memTemp.p;
      size_t iSize = (size_t)((PBYTE)pszFind - (PBYTE)psz) / sizeof(WCHAR);
      memcpy (pszNew, psz, iSize * sizeof(WCHAR));
      wcscpy (pszNew + iSize, pszEllipsis);
      wcscat (pszNew, psz + (iSize + dwLenReplace));
      psz = (PWSTR)memTemp.p;
   }

   WCHAR szMax[MENUWIDTHSCALE(18)];
   DWORD dwMaxCount = sizeof(szMax)/sizeof(WCHAR)-4;
   DWORD dwLen = (DWORD)wcslen(psz);
   if (dwLen < sizeof(szMax)/sizeof(WCHAR)-1)
      MemCatSanitize (pMem, psz);
   else {
      memcpy (szMax, psz, min(dwLen*sizeof(WCHAR), sizeof(szMax)));
      wcscpy (szMax + dwMaxCount, L"...");
      MemCatSanitize (pMem, szMax);
   }
}

/*********************************************************************************
LibraryDisplayName - This takes a library name (with a full path), and returns
a pointer to the file name (excluding the the directory)

inputs
   PWSTR          pszName - Name
returns
   PWSTR - Without the slash
*/
PWSTR LibraryDisplayName (PWSTR pszName)
{
   PWSTR pszRet = pszName;
   PWSTR pszCur;

   while (pszCur = wcschr(pszRet, L'\\'))
      pszRet = pszCur+1;

   return pszRet;
}


/*********************************************************************************
LinkExtractNum - Extracts a number from the link and sets the string pointer to the
next non-number character

inputs
   PWSTR          pszString - Location in the string where the number should start
   DWORD          *pdwNum - Filled in with the number
returns
   PWSTR - Pointer in string pointing to just after the number, or NULL if no number
*/
PWSTR LinkExtractNum (PWSTR pszString, DWORD *pdwNum)
{
   PWSTR pszOrig = pszString;
   *pdwNum = 0;

   while ((pszString[0] >= L'0') && (pszString[0] <= L'9')) {
      pdwNum[0] = pdwNum[0] * 10 + (pszString[0] - L'0');
      pszString++;
   }

   return (pszString == pszOrig) ? NULL : pszString;
}

/*********************************************************************************
MIFLTempID - This creates a temporary (and unique) ID for an object. Used to manage
back links for previous pages. Can't use the object index since the objects
may be re-arranged

returns
   DWORD - Unique ID
*/
DWORD MIFLTempID (void)
{
   static DWORD dwCount = 1;
   return dwCount++;
}


/*********************************************************************************
MIFLFilteredListSeach - This searches for a name match in a CListVariable creted
to fill in a filtered list

inputs
   PWSTR             pszName - Name that looking for
   PCListVariable    pList - List of strings to compare to
returns
   DWORD - INdex, into pList, or -1 if cant find
*/
DWORD MIFLFilteredListSearch (PWSTR pszName, PCListVariable pList)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = pList->Num();
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // get it
      PWSTR psz = (PWSTR) pList->Get(dwTry);
      if (!psz)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, psz);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0)
         return dwTry;
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // find match
   PWSTR psz = (PWSTR) pList->Get(dwCur);
   if (!_wcsicmp(pszName, psz))
      return dwCur;

   return -1;
}



/*********************************************************************************
MIFLFilteredListObject - Fill in the object filtered list.

inputs
   PMIFLPage      pmpv - Info
   BOOL           fSkipClasses - If TRUE then skip classes when creating the list of
                  objects, and only show objects with autocreate. If FALSE then all/
returns
   none
*/
void MIFLFilteredListObject (PMIFLPAGE pmp, BOOL fSkipClasses)
{
   if (pmp->plObject->Num())
      return;

   pmp->pProj->ObjectEnum (pmp->plObject, TRUE, fSkipClasses);
}



/*********************************************************************************
MIFLFilteredListClass - Fill in the object filtered list.

inputs
   PMIFLPage      pmpv - Info
   BOOL           fSkipClasses - If TRUE then skip classes when creating the list of
                  objects, and only show objects with autocreate. If FALSE then all/
returns
   none
*/
void MIFLFilteredListClass (PMIFLPAGE pmp, BOOL fSkipClasses)
{
   if (pmp->plClass->Num())
      return;

   pmp->pProj->ObjectEnum (pmp->plClass, TRUE, fSkipClasses);
}



/*********************************************************************************
MIFLFilteredListPropPub - Fill in the public property filtered list.

inputs
   PMIFLPage      pmpv - Info
returns
   none
*/
void MIFLFilteredListPropPub (PMIFLPAGE pmp)
{
   if (pmp->plPropPub->Num())
      return;

   PCMIFLProj pProj = pmp->pProj;
   CMem mem;
   CListFixed lIndex;
   lIndex.Init (sizeof(DWORD));

   // figure out index into each of the libraries
   DWORD i;
   DWORD dwIndex = 0;
   lIndex.Required (pProj->LibraryNum());
   for (i = 0; i < pProj->LibraryNum(); i++)
      lIndex.Add (&dwIndex);
   DWORD *padwIndex = (DWORD*)lIndex.Get(0);
   DWORD dwNum = lIndex.Num();

   // loop while have indecies
   while (TRUE) {
      DWORD dwBest = -1;
      PCMIFLLib pLibBest = NULL;
      for (i = 0; i < dwNum; i++) {
         PCMIFLLib pLib = pProj->LibraryGet (i);
         if (padwIndex[i] >= pLib->PropDefNum())
            continue;   // out of range

         // if no current best then use this
         if (dwBest == -1) {
            dwBest = i;
            pLibBest = pLib;
            continue;
         }

         // else compare
         if (_wcsicmp( (PWSTR) pLib->PropDefGet(padwIndex[i])->m_memName.p,
            (PWSTR)pLibBest->PropDefGet(padwIndex[dwBest])->m_memName.p) < 0) {
               // found a better match
               dwBest = i;
               pLibBest = pLib;
            }
      } // i

      // if didn't find a best match then done
      if (dwBest == -1)
         break;

      // remember this property index
      DWORD dwBestIndex = padwIndex[dwBest];

      // loop through and incrememnt all indecies that match the best,
      // eliminating dupliucates
      for (i = 0; i < dwNum; i++) {
         if (i == dwBest) {
            padwIndex[dwBest] += 1;
            continue;
         }

         PCMIFLLib pLib = pProj->LibraryGet (i);
         if (padwIndex[i] >= pLib->PropDefNum())
            continue;   // out of range

         if (_wcsicmp( (PWSTR) pLib->PropDefGet(padwIndex[i])->m_memName.p,
            (PWSTR)pLibBest->PropDefGet(dwBestIndex)->m_memName.p) == 0) {
               // match
               padwIndex[i] += 1;
               continue;
            }
      } // i, eliminate dups

      // now, create memory to store this
      PCMIFLProp pProp = pLibBest->PropDefGet(dwBestIndex);
      DWORD dwLenName = ((DWORD)wcslen((PWSTR)pProp->m_memName.p)+1)*sizeof(WCHAR);
      DWORD dwLenDesc = ((DWORD)wcslen((PWSTR)pProp->m_memDescShort.p)+1)*sizeof(WCHAR);
      if (!mem.Required (dwLenName + dwLenDesc))
         continue;   // error
      memcpy (mem.p, pProp->m_memName.p, dwLenName);
      memcpy ((PBYTE)mem.p + dwLenName, pProp->m_memDescShort.p, dwLenDesc);

      // add
      pmp->plPropPub->Add (mem.p, dwLenName + dwLenDesc);
   } // while true
}


/*********************************************************************************
MIFLFilteredListMethPub - Fill in the public method filtered list.

inputs
   PMIFLPage      pmpv - Info
   PCMIFLObject   pObjExclude - If not null then exclude any public methods already here
returns
   none
*/
void MIFLFilteredListMethPub (PMIFLPAGE pmp, PCMIFLObject pObjExclude)
{
   if (pmp->plMethPub->Num())
      return;

   PCMIFLProj pProj = pmp->pProj;
   CMem mem;
   CListFixed lIndex;
   lIndex.Init (sizeof(DWORD));

   // figure out index into each of the libraries
   DWORD i;
   DWORD dwIndex = 0;
   lIndex.Required (pProj->LibraryNum());
   for (i = 0; i < pProj->LibraryNum(); i++)
      lIndex.Add (&dwIndex);
   DWORD *padwIndex = (DWORD*)lIndex.Get(0);
   DWORD dwNum = lIndex.Num();

   // loop while have indecies
   while (TRUE) {
      DWORD dwBest = -1;
      PCMIFLLib pLibBest = NULL;
      for (i = 0; i < dwNum; i++) {
         PCMIFLLib pLib = pProj->LibraryGet (i);
         if (padwIndex[i] >= pLib->MethDefNum())
            continue;   // out of range

         // if no current best then use this
         if (dwBest == -1) {
            dwBest = i;
            pLibBest = pLib;
            continue;
         }

         // else compare
         if (_wcsicmp( (PWSTR) pLib->MethDefGet(padwIndex[i])->m_memName.p,
            (PWSTR)pLibBest->MethDefGet(padwIndex[dwBest])->m_memName.p) < 0) {
               // found a better match
               dwBest = i;
               pLibBest = pLib;
            }
      } // i

      // if didn't find a best match then done
      if (dwBest == -1)
         break;

      // remember this method index
      DWORD dwBestIndex = padwIndex[dwBest];

      // loop through and incrememnt all indecies that match the best,
      // eliminating dupliucates
      for (i = 0; i < dwNum; i++) {
         if (i == dwBest) {
            padwIndex[dwBest] += 1;
            continue;
         }

         PCMIFLLib pLib = pProj->LibraryGet (i);
         if (padwIndex[i] >= pLib->MethDefNum())
            continue;   // out of range

         if (_wcsicmp( (PWSTR) pLib->MethDefGet(padwIndex[i])->m_memName.p,
            (PWSTR)pLibBest->MethDefGet(dwBestIndex)->m_memName.p) == 0) {
               // match
               padwIndex[i] += 1;
               continue;
            }
      } // i, eliminate dups

      // now, create memory to store this
      PCMIFLMeth pMeth = pLibBest->MethDefGet(dwBestIndex);

      // consider excluding it if it's already in the object
      if (pObjExclude && (-1 != pObjExclude->MethPubFind ((PWSTR)pMeth->m_memName.p,-1)))
         continue;

      DWORD dwLenName = ((DWORD)wcslen((PWSTR)pMeth->m_memName.p)+1)*sizeof(WCHAR);
      DWORD dwLenDesc = ((DWORD)wcslen((PWSTR)pMeth->m_memDescShort.p)+1)*sizeof(WCHAR);
      if (!mem.Required (dwLenName + dwLenDesc))
         continue;   // error
      memcpy (mem.p, pMeth->m_memName.p, dwLenName);
      memcpy ((PBYTE)mem.p + dwLenName, pMeth->m_memDescShort.p, dwLenDesc);

      // add
      pmp->plMethPub->Add (mem.p, dwLenName + dwLenDesc);
   } // while true
}

static int _cdecl PWSTRIndexCompare (const void *elem1, const void *elem2)
{
   PWSTR pdw1, pdw2;
   pdw1 = *((PWSTR*) elem1);
   pdw2 = *((PWSTR*) elem2);

   return _wcsicmp(pdw1, pdw2);
}

/*********************************************************************************
MIFLDefPage - Default page that has substitution for the menu
*/

BOOL MIFLDefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib ? pmp->pLib : pProj->LibraryGet(pProj->LibraryCurGet());
   PCMIFLObject pObj = pmp->pObj;

   switch (dwMessage) {
   case ESCM_DESTRUCTOR:
      // remember the page
      if (pProj)
         pProj->m_pPageCurrent = NULL;
      break;

   case ESCM_INITPAGE:
      // remember the page
      if (pProj)
         pProj->m_pPageCurrent = pPage;

      // scroll to right position
      if (pmp->iVScroll > 0) {
         pPage->VScroll (pmp->iVScroll);

         // when bring up pop-up dialog often they're scrolled wrong because
         // iVScoll was left as valeu, and they use defpage
         pmp->iVScroll = 0;

         // BUGFIX - putting this invalidate in to hopefully fix a refresh
         // problem when add or move a task in the ProjectView page
         pPage->Invalidate();
      }

      // set up listbox for errors
      PCEscControl pControl;
      if (pProj && pProj->m_pErrors && (pControl = pPage->ControlFind (L"errlist"))) {
         MemZero (&gMemTemp);

         DWORD i;
         PCMIFLErrors pe = pProj->m_pErrors;

         for (i = 0; i < pe->Num(); i++) {
            PMIFLERR perr = pe->Get(i);

            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L"><bold>");
            MemCatSanitize (&gMemTemp, perr->pszObject);
            MemCat (&gMemTemp, L"</bold>");
            if (pProj->m_fErrorsAreSearch) {
               MemCat (&gMemTemp, L": ");
            }
            else {
               MemCat (&gMemTemp, L": (");
               if (perr->fError)
                  MemCat (&gMemTemp, L"<font color=#ff0000><bold>Error</bold></font>");
               else
                  MemCat (&gMemTemp, L"Warning");
               MemCat (&gMemTemp, L") ");
            }
            MemCatSanitize (&gMemTemp, perr->pszDisplay);
            MemCat (&gMemTemp, L"</elem>");
         }

         // if there aren't any errors then report that
         if (!pe->Num())
            MemCat (&gMemTemp, L"<elem name=0>The compile was <bold>successful</bold>.</elem>");

         ESCMLISTBOXADD lba;
         memset (&lba, 0,sizeof(lba));
         lba.pszMML = (PWSTR)gMemTemp.p;

         pControl->Message (ESCM_LISTBOXADD, &lba);

         pControl->AttribSetInt (CurSel(), ((DWORD)pProj->m_iErrCur < pe->Num()) ? pProj->m_iErrCur : -1);

      } // if found list

      break;   // fall through

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;

         // only care about the search list control
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;
         
         if (!_wcsicmp(psz, L"errlist")) {
            if (!pProj->m_pErrors)
               return TRUE;   // shouldnt happen

            if (p->dwCurSel >= pProj->m_pErrors->Num())
               return TRUE;   // do nothing

            // else, link
            pProj->m_iErrCur = (int)p->dwCurSel;
            PMIFLERR perr = pProj->m_pErrors->Get(p->dwCurSel);

            // if the link is the same then dont change page, just
            // set edit control
            // BUGFIX - If not on the right tab then require switch
            if (pmp->pszErrLink && (pProj->m_dwTabFunc == 2) && !_wcsicmp(pmp->pszErrLink, perr->pszLink)) {
               PCEscControl pControl;

               pControl = pPage->ControlFind (L"code");
               if (pControl) {
                  // clicked on one with link
                  pControl->AttribSetInt (L"selstart", perr->dwStartChar);
                  pControl->AttribSetInt (L"selend", perr->dwEndChar);
                  pControl->Message (ESCM_EDITSCROLLCARET);
               }
            }
            else {
               pProj->m_dwTabFunc = 2; // BUGFIX - When click switch to code
               pPage->Exit (perr->pszLink);
            }
            return TRUE;
         }
         break;
      }

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:", pszNewObject1 = L"newobject1:", pszNewObject2 = L"newobject2:",
            pszNewObject3 = L"newobject3:";
         DWORD dwLen = (DWORD)wcslen(pszTab), dwNewObject1Len = (DWORD)wcslen(pszNewObject1),
            dwNewObject2Len = (DWORD)wcslen(pszNewObject2), dwNewObject3Len = (DWORD)wcslen(pszNewObject3);

         if (!wcsncmp(p->psz, pszTab, dwLen) && pmp->pdwTab) {
            *pmp->pdwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(p->psz, pszNewObject1, dwNewObject1Len) ||
            !wcsncmp(p->psz, pszNewObject2, dwNewObject2Len) ||
            !wcsncmp(p->psz, pszNewObject3, dwNewObject3Len)) {

            BOOL fAsClass = !wcsncmp(p->psz, pszNewObject2, dwNewObject2Len);
            BOOL fAsSubObject = !wcsncmp(p->psz, pszNewObject1, dwNewObject1Len);
            PWSTR pszType = p->psz + dwNewObject1Len;
            DWORD dwObjID = pLib->ObjectNew ();
            if (dwObjID == -1)
               return TRUE;

            // tweak some stuff
            DWORD dwIndex = pLib->ObjectFind (dwObjID);
            PCMIFLObject pObjCur = pLib->ObjectGet(dwIndex);
            if (!pObjCur)
               return TRUE;

            // if we're actually in and object, and not creating a class, then make the
            // parent of the new object be the object we're in
            if (pObj && pObj->m_fAutoCreate && ((PWSTR)pObj->m_memName.p)[0] && fAsSubObject) {
               MemZero (&pObjCur->m_memContained);
               MemCat (&pObjCur->m_memContained, (PWSTR)pObj->m_memName.p);
            }
            
            pObjCur->m_fAutoCreate = !fAsClass;
            
            // create the type
            CMem memTemp, memTemp2;
            MemZero (&memTemp);
            if (!fAsClass && ((pszType[0] == L'c') || (pszType[0] == L'C'))) {
               MemCat (&memTemp, L"o");
               MemCat (&memTemp, pszType+1);
            }
            else
               MemCat (&memTemp, pszType); // since doens't start with 'c', assume an 'o'

            // try counting up numbers
            DWORD dwNum, i;
            for (dwNum = 1; ; dwNum++) {
               MemZero (&memTemp2);
               MemCat (&memTemp2, (PWSTR)memTemp.p);
               MemCat (&memTemp2, (int)dwNum);

               for (i = 0; i < pProj->LibraryNum(); i++) {
                  PCMIFLLib pLibCur = pProj->LibraryGet(i);
                  if (-1 != pLibCur->ObjectFind ((PWSTR)memTemp2.p, (DWORD)-1))
                     break;   // found match
               } // i
               if (i >= pProj->LibraryNum())
                  break;   // found unique name
            } // i

            // keep name
            MemZero (&pObjCur->m_memName);
            MemCat (&pObjCur->m_memName, (PWSTR)memTemp2.p);

            // need to resort since have new name
            pLib->ObjectSort ();

            // fill in the class
            pObjCur->ClassAddWithRecommend (pszType, TRUE, pProj);

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit", (int) pLib->m_dwTempID, (int) dwObjID);
            pPage->Exit (szTemp);
            return TRUE;
         }

         if (!_wcsicmp(p->psz, L"exit")) {
            // see if it's dirty
            BOOL fDirty = FALSE;
            fDirty |= pProj->m_fDirty;
            DWORD i;
            for (i = 0; i < pProj->LibraryNum(); i++) {
               PCMIFLLib pLib = pProj->LibraryGet(i);
               fDirty |= pLib->m_fDirty;
            } // i
            
            if (!fDirty)
               break;
            int iRet;
            iRet = pPage->MBYesNo (L"Some libraries have changed since you last save. Do you wish to save them?",
               NULL, TRUE);
            if (iRet == IDYES)
               pProj->Save();
            else if (iRet == IDCANCEL)
               return TRUE;

            // else, fall through
            break;
         }
         else if (!_wcsicmp(p->psz, L"compall")) {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Compiling...");

            pProj->Compile(&Progress);

            if (pProj->m_pErrors->m_dwNumError)
               EscChime (ESCCHIME_ERROR);
            else if (pProj->m_pErrors->Num())
               EscChime (ESCCHIME_WARNING);
            else
               EscChime (ESCCHIME_INFORMATION);

            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"proj:libsave")) {
            if (pProj->Save(TRUE))
               EscChime (ESCCHIME_INFORMATION);
            else
               pPage->MBWarning (L"The save failed.", L"One of the files failed to write. The file may be write protected.");
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"errhide")) {
            if (pProj->m_pErrors)
               delete pProj->m_pErrors;
            pProj->m_pErrors = NULL;

            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"miflundo") || !_wcsicmp(p->psz, L"miflredo")) {
            BOOL fRedo;
            BOOL fDoingUndo = !_wcsicmp(p->psz, L"miflundo");
            BOOL fUndo = pProj->UndoQuery (&fRedo);
            if (fDoingUndo && !fUndo) {
               pPage->MBInformation (L"There aren't any more undo's.");
               return TRUE;
            }
            else if (!fDoingUndo && !fRedo) {
               pPage->MBInformation (L"There aren't any more redo's.");
               return TRUE;
            }

            pProj->Undo(fDoingUndo);
            EscChime (ESCCHIME_INFORMATION);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menumethadd")) {
            DWORD dwMethID = pLib->MethDefNew ();
            if (dwMethID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dmethdef:%dedit", (int) pLib->m_dwTempID, (int) dwMethID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menumischelp")) {
            pProj->Help (pPage, 0);

            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menuobjmethprivadd")) {
            DWORD dwPropID = pObj->MethPrivNew (pLib);
            if (dwPropID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit", (int) pLib->m_dwTempID, (int)pObj->m_dwTempID, (int) dwPropID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menuobjadd")) {
            DWORD dwObjID = pLib->ObjectNew ();
            if (dwObjID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit", (int) pLib->m_dwTempID, (int) dwObjID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menumiscstringlist")) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dstringlist", (int) pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menumiscdoc")) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%ddoclist", (int) pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menumiscstring")) {
            DWORD dwObjID = pLib->StringNew ();
            if (dwObjID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dstring:%dedit", (int) pLib->m_dwTempID, (int) dwObjID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menumiscresourcelist")) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dresourcelist", (int) pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menumiscresource")) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dresourceadd", (int) pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menupropadd")) {
            DWORD dwPropID = pLib->PropDefNew ();
            if (dwPropID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dpropdef:%dedit", (int) pLib->m_dwTempID, (int) dwPropID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menuglobaladd")) {
            DWORD dwPropID = pLib->GlobalNew ();
            if (dwPropID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dglobal:%dedit", (int) pLib->m_dwTempID, (int) dwPropID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"menufuncadd")) {
            DWORD dwPropID = pLib->FuncNew ();
            if (dwPropID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dfunc:%dedit", (int) pLib->m_dwTempID, (int) dwPropID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_CLOSE:
      {
         // see if it's dirty
         BOOL fDirty = FALSE;
         fDirty |= pProj->m_fDirty;
         DWORD i;
         for (i = 0; i < pProj->LibraryNum(); i++) {
            PCMIFLLib pLib = pProj->LibraryGet(i);
            fDirty |= pLib->m_fDirty;
         } // i
         
         if (!fDirty)
            break;
         int iRet;
         iRet = pPage->MBYesNo (L"Some libraries have changed since you last save. Do you wish to save them?",
            NULL, TRUE);
         if (iRet == IDYES)
            pProj->Save();
         else if (iRet == IDCANCEL)
            return TRUE;

         // else, fall through
      }
      break;

   case ESCN_FILTEREDLISTQUERY:
      {
         PESCNFILTEREDLISTQUERY p = (PESCNFILTEREDLISTQUERY)pParam;
         if (!p->pszListName)
            break;

         if (!_wcsicmp(p->pszListName, L"proppub") && pmp->plPropPub) {
            MIFLFilteredListPropPub (pmp);

            p->pList = pmp->plPropPub;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, L"methpub") && pmp->plMethPub) {
            MIFLFilteredListMethPub (pmp, pmp->pObj);

            p->pList = pmp->plMethPub;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, L"object") && pmp->plObject) {
            MIFLFilteredListObject (pmp, TRUE);
                  // BUGFIX - Change this to always
                  // skip classes. May not be right, since assuming that skip
                  // classes when chosing object to create within

            p->pList = pmp->plObject;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszListName, L"class") && pmp->plClass) {
            MIFLFilteredListClass (pmp, FALSE /* so gets all classes*/);
            p->pList = pmp->plClass;
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = (DWORD)wcslen(pszIfTab), dwEndIfTabLen = (DWORD)wcslen(pszEndIfTab);

         if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == *pmp->pdwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == *pmp->pdwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBENABLE")) {
            if (pLib && pLib->m_fReadOnly)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBREADONLY")) {
            if (pLib && pLib->m_fReadOnly)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ERRLIST")) {
            if (pProj && pProj->m_pErrors) {
               MemZero (&gMemTemp);
               MemCat (&gMemTemp, 
                  L"<tr><td align=center>"
                  );
               MemCat (&gMemTemp, 
                  pProj->m_fErrorsAreSearch ?
                     L"<font color=#ffffff>Search results:</font><a color=#8080ff href=errhide>(Hide search)</a><br/>" :
                     L"<font color=#ffffff>Compile errors:</font><a color=#8080ff href=errhide>(Hide errors)</a><br/>"
                  );
               MemCat (&gMemTemp, 
                  L"<listbox sort=false vscroll=errscroll width=80% height=20% name=errlist/>"
                  L"<scrollbar orient=vert height=20% name=errscroll/>"
                  L"</td></tr>"
               );
               p->pszSubString = (PWSTR)gMemTemp.p;
            }
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBNAME")) {
            MemZero (&gMemTemp);
            if (pLib) {
               MemCat (&gMemTemp, L"<font color=#ffff80>");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pLib->m_szFile));
               if (pObj)
                  MemCat (&gMemTemp, L"<br/>");
               MemCat (&gMemTemp, L"</font>");
            }
            if (pObj) {
               MemCat (&gMemTemp, L"<font color=#ff8080>");
               MemCatSanitize (&gMemTemp, (PWSTR)pObj->m_memName.p);
               MemCat (&gMemTemp, L"</font>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"READONLY")) {
            if (pLib && pLib->m_fReadOnly)
               p->pszSubString = L"<p align=right><bold><big><big><font color=#800000>(Read only)</font></big></big></bold></p>";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"TESTMENUSHOW")) {
            if (!pProj || !pProj->m_pSocket->TestQuery())
               p->pszSubString = L"";
            else
               p->pszSubString = L"<xMenuButton href=proj:test accel=t><u>T</u>est compiled code</xMenuButton>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENUMISC")) {
            MemZero (&gMemTemp);
            CListVariable lMenu;
            if (pProj && pProj->m_pSocket)
               pProj->m_pSocket->MenuEnum (pProj, pLib, &lMenu);

            DWORD i;
            for (i = 0; i < lMenu.Num(); i++) {
               MemCat (&gMemTemp, L"<xMenuButton href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"menumisc:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, (PWSTR)lMenu.Get(i));
               MemCat (&gMemTemp, L"</xMenuButton>");
            } // i

            if (lMenu.Num())
               MemCat (&gMemTemp, L"<xMenuSeperator/>");

            p->pszSubString = (PWSTR)gMemTemp.p;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFMENULIBRO")) {
            if (pLib && pLib->m_fReadOnly)
               p->pszSubString = L"<comment>";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFMENULIBRO")) {
            if (pLib && pLib->m_fReadOnly)
               p->pszSubString = L"</comment>";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENUOBJSPECIFIC")) {
            // if no object then nothing
            if (!pObj)
               return TRUE;

            MemZero (&gMemTemp);

            // get library name
            CMem memLib;
            LibNameExtract (pLib, &memLib);
            PWSTR pszLibName = (PWSTR)memLib.p;



            // public methods...
            if (pObj->MethPubNum() || !pLib->m_fReadOnly) {
               MemCat (&gMemTemp, L"<menu><xMenutext>Methods (public)</xMenutext><xMenucontents>");

               DWORD i;
               DWORD dwNum = pObj->MethPubNum();
               BOOL fSmallText = (dwNum >= TOOMANYITEMS);
               BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
               DWORD dwRows = min(dwNum, MENUROWS);
               DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
               if (dwColumns) {
                  if (fVerySmallText)
                     MemCat (&gMemTemp, L"<small>");
                  if (fSmallText)
                     MemCat (&gMemTemp, L"<small>");

                  MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
                  MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
                  MemCat (&gMemTemp, L">");

                  DWORD dwRow, dwColumn;
                  for (dwRow = 0; dwRow < dwRows; dwRow++) {
                     MemCat (&gMemTemp, L"<tr>");

                     for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                        i = dwColumn * MENUROWS + dwRow;
                        if (i >= dwNum)
                           MemCat (&gMemTemp, L"<td/>");

                        PCMIFLFunc pFunc = pObj->MethPubGet(i);
                        if (!pFunc)
                           continue;

                        MemCat (&gMemTemp, L"<td>");

                        MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                        MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                        MemCat (&gMemTemp, L"object:");
                        MemCat (&gMemTemp, (int)pObj->m_dwTempID);
                        MemCat (&gMemTemp, L"methpub:");
                        MemCat (&gMemTemp, (int)pFunc->m_Meth.m_dwTempID);
                        MemCat (&gMemTemp, L"edit>");
                        MemCatSanMaxLen (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p, pszLibName);
                        MemCat (&gMemTemp, L"</xMenuButton>");

                        MemCat (&gMemTemp, L"</td>");
                     } // dwColumn

                     MemCat (&gMemTemp, L"</tr>");
                  } // dwRow

                  MemCat (&gMemTemp, L"</table><br/>");
                  if (fSmallText)
                     MemCat (&gMemTemp, L"</small>");
                  if (fVerySmallText)
                     MemCat (&gMemTemp, L"</small>");

               } // if rows

               // return to main object
               if (pObj->MethPubNum())
                  MemCat (&gMemTemp, L"<xMenuSeperator/>");
               MemCat (&gMemTemp, L"<xMenuButton accel=m href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"object:");
               MemCat (&gMemTemp, (int)pObj->m_dwTempID);
               MemCat (&gMemTemp, L"><u>M</u>ain object page</xMenuButton>");
               // NOTE: Cant add public method from menu because need dropdown


               MemCat (&gMemTemp, L"</xMenucontents></menu>");
            }


            // private methods...
            if (pObj->MethPrivNum() || !pLib->m_fReadOnly) {
               MemCat (&gMemTemp, L"<menu><xMenutext>Methods (private)</xMenutext><xMenucontents>");

               DWORD i;
               DWORD dwNum = pObj->MethPrivNum();
               BOOL fSmallText = (dwNum >= TOOMANYITEMS);
               BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
               DWORD dwRows = min(dwNum, MENUROWS);
               DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
               if (dwColumns) {
                  if (fVerySmallText)
                     MemCat (&gMemTemp, L"<small>");
                  if (fSmallText)
                     MemCat (&gMemTemp, L"<small>");

                  MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
                  MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
                  MemCat (&gMemTemp, L">");

                  DWORD dwRow, dwColumn;
                  for (dwRow = 0; dwRow < dwRows; dwRow++) {
                     MemCat (&gMemTemp, L"<tr>");

                     for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                        i = dwColumn * MENUROWS + dwRow;
                        if (i >= dwNum)
                           MemCat (&gMemTemp, L"<td/>");

                        PCMIFLFunc pFunc = pObj->MethPrivGet(i);
                        if (!pFunc)
                           continue;

                        MemCat (&gMemTemp, L"<td>");

                        MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                        MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                        MemCat (&gMemTemp, L"object:");
                        MemCat (&gMemTemp, (int)pObj->m_dwTempID);
                        MemCat (&gMemTemp, L"methpriv:");
                        MemCat (&gMemTemp, (int)pFunc->m_Meth.m_dwTempID);
                        MemCat (&gMemTemp, L"edit>");
                        MemCatSanMaxLen (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p, pszLibName);
                        MemCat (&gMemTemp, L"</xMenuButton>");

                        MemCat (&gMemTemp, L"</td>");
                     } // dwColumn

                     MemCat (&gMemTemp, L"</tr>");
                  } // dwRow

                  MemCat (&gMemTemp, L"</table><br/>");
                  if (fSmallText)
                     MemCat (&gMemTemp, L"</small>");
                  if (fVerySmallText)
                     MemCat (&gMemTemp, L"</small>");

               } // if rows

               // return to main object
               if (pObj->MethPrivNum())
                  MemCat (&gMemTemp, L"<xMenuSeperator/>");
               MemCat (&gMemTemp, L"<xMenuButton accel=m href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"object:");
               MemCat (&gMemTemp, (int)pObj->m_dwTempID);
               MemCat (&gMemTemp, L"><u>M</u>ain object page</xMenuButton>");
               if (!pLib->m_fReadOnly)
                  MemCat (&gMemTemp, L"<xMenuButton accel=a href=menuobjmethprivadd><u>A</u>dd new private method</xMenuButton>");

               MemCat (&gMemTemp, L"</xMenucontents></menu>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENULIB")) {
            MemZero (&gMemTemp);

            DWORD i;
            DWORD dwNum = pProj->LibraryNum();
            BOOL fSmallText = (dwNum >= TOOMANYITEMS);
            BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
            DWORD dwRows = min(dwNum, MENUROWS);
            DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
            if (dwColumns) {
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"<small>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"<small>");

               MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
               MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
               MemCat (&gMemTemp, L">");

               DWORD dwRow, dwColumn;
               for (dwRow = 0; dwRow < dwRows; dwRow++) {
                  MemCat (&gMemTemp, L"<tr>");

                  for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                     i = dwColumn * MENUROWS + dwRow;
                     if (i >= dwNum)
                        MemCat (&gMemTemp, L"<td/>");

                     PCMIFLLib pLib = pProj->LibraryGet(i);
                     if (!pLib)
                        continue;

                     MemCat (&gMemTemp, L"<td>");

                     MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                     MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                     MemCat (&gMemTemp, L">");
                     if (i == pProj->LibraryCurGet())
                        MemCat (&gMemTemp, L"<font color=#ffff80>");
                     MemCatSanMaxLen (&gMemTemp, LibraryDisplayName(pLib->m_szFile), NULL);
                     if (i == pProj->LibraryCurGet())
                        MemCat (&gMemTemp, L"</font>");

                     if (pLib->m_fReadOnly)
                        MemCat (&gMemTemp, L" (built-in)");

                     MemCat (&gMemTemp, L"</xMenuButton>");

                     MemCat (&gMemTemp, L"</td>");
                  } // dwColumn

                  MemCat (&gMemTemp, L"</tr>");
               } // dwRow

               MemCat (&gMemTemp, L"</table><br/>");
               // put in separator
               MemCat (&gMemTemp, L"<xMenuSeperator/>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"</small>");
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"</small>");

            } // if rows

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENUMETH")) {
            MemZero (&gMemTemp);

            // get library name
            CMem memLib;
            LibNameExtract (pLib, &memLib);
            PWSTR pszLibName = (PWSTR)memLib.p;


            DWORD i;
            DWORD dwNum = pLib ? pLib->MethDefNum() : 0;
            BOOL fSmallText = (dwNum >= TOOMANYITEMS);
            BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
            DWORD dwRows = min(dwNum, MENUROWS);
            DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
            if (dwColumns) {
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"<small>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"<small>");

               MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
               MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
               MemCat (&gMemTemp, L">");

               DWORD dwRow, dwColumn;
               for (dwRow = 0; dwRow < dwRows; dwRow++) {
                  MemCat (&gMemTemp, L"<tr>");

                  for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                     i = dwColumn * MENUROWS + dwRow;
                     if (i >= dwNum)
                        MemCat (&gMemTemp, L"<td/>");

                     PCMIFLMeth pMeth = pLib ? pLib->MethDefGet(i) : NULL;
                     if (!pMeth)
                        continue;

                     MemCat (&gMemTemp, L"<td>");

                     MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                     MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                     MemCat (&gMemTemp, L"methdef:");
                     MemCat (&gMemTemp, (int)pMeth->m_dwTempID);
                     MemCat (&gMemTemp, L"edit>");
                     PWSTR psz =(PWSTR)pMeth->m_memName.p;
                     MemCatSanMaxLen (&gMemTemp, (psz && psz[0]) ? psz : L"No name", pszLibName);

                     // see if this is overridden
                     PWSTR pszHigher, pszLower;
                     pProj->MethDefOverridden (pLib->m_dwTempID, (PWSTR)pMeth->m_memName.p, &pszHigher, &pszLower);
                     if (pszHigher)
                        MemCat (&gMemTemp, L" <italic>(Overridden)</italic>");
                     else if (pszLower)
                        MemCat (&gMemTemp, L" <italic>(Overrides)</italic>");

                     MemCat (&gMemTemp, L"</xMenuButton>");

                     MemCat (&gMemTemp, L"</td>");
                  } // dwColumn

                  MemCat (&gMemTemp, L"</tr>");
               } // dwRow

               MemCat (&gMemTemp, L"</table><br/>");
               // put in separator
               MemCat (&gMemTemp, L"<xMenuSeperator/>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"</small>");
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"</small>");

            } // if rows

            // menu for library list
            MemCat (&gMemTemp, L"<xMenuButton accel=l href=lib:");
            MemCat (&gMemTemp, (int) (pLib ? pLib->m_dwTempID : 0));
            MemCat (&gMemTemp, L"methdeflist><u>L</u>ist of methods</xMenuButton>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENUNEWOBJECT")) {
            MemZero (&gMemTemp);

            // get library name
            CMem memLib;
            LibNameExtract (pLib, &memLib);
            PWSTR pszLibName = (PWSTR)memLib.p;

            // loop through all the libraries, and all the objects in them,
            // looking for entires
            CHashString hNewObjects;
            CListFixed lToSort;
            hNewObjects.Init (sizeof(DWORD));
            lToSort.Init (sizeof(PWSTR));
            DWORD i, j;
            PWSTR psz;
            for (i = 0; i < pProj->LibraryNum(); i++) {
               PCMIFLLib pLibCur = pProj->LibraryGet(i);
               for (j = 0; j < pLibCur->ObjectNum(); j++) {
                  PCMIFLObject pObjCur = pLibCur->ObjectGet(j);
                  if (!pObjCur->m_dwInNewObjectMenu)
                     continue; // not in the menu

                  // if this is supposed to be created off a sub-object, but
                  // not currently in an object, then dont add
                  if ((pObjCur->m_dwInNewObjectMenu == 1) && !(pObj && pObj->m_fAutoCreate))
                     continue;

                  // make sure not alreayd there
                  psz = (PWSTR)pObjCur->m_memName.p;
                  if (!psz || !psz[0] || hNewObjects.Find(psz))
                     continue;

                  // add it
                  hNewObjects.Add (psz, &pObjCur->m_dwInNewObjectMenu);
                  lToSort.Add (&psz);
               } // j
            } // i

            // if empty then indicate so
            if (!lToSort.Num()) {
               MemCat (&gMemTemp, L"<italic>List empty</italic>");
               p->pszSubString = (PWSTR)gMemTemp.p;
               return TRUE;
            }

            // sort this list
            qsort (lToSort.Get(0), lToSort.Num(), sizeof(PWSTR), PWSTRIndexCompare);
            
            // list
            DWORD dwNum = lToSort.Num();
            BOOL fSmallText = (dwNum >= TOOMANYITEMS);
            BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
            DWORD dwRows = min(dwNum, MENUROWS);
            DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
            if (dwColumns) {
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"<small>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"<small>");

               MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
               MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
               MemCat (&gMemTemp, L">");

               DWORD dwRow, dwColumn;
               for (dwRow = 0; dwRow < dwRows; dwRow++) {
                  MemCat (&gMemTemp, L"<tr>");

                  for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                     i = dwColumn * MENUROWS + dwRow;
                     if (i >= dwNum) {
                        MemCat (&gMemTemp, L"<td/>");
                        continue;
                     }

                     MemCat (&gMemTemp, L"<td>");

                     psz = *((PWSTR*)lToSort.Get(i));

                     // get the type
                     DWORD dwType = *((DWORD*)hNewObjects.Find(psz));

                     MemCat (&gMemTemp, L"<xMenuButton href=\"newobject");
                     MemCat (&gMemTemp, (int) dwType);
                     MemCat (&gMemTemp, L":");
                     MemCatSanitize (&gMemTemp, psz);
                     MemCat (&gMemTemp, L"\">");
                     if (dwType == 2)  // if it's for creating a class
                        MemCat (&gMemTemp, L"<font color=#ff8080>");
                     else if (dwType == 3)   // object, but no parent
                        MemCat (&gMemTemp, L"<font color=#80ff80>");
                     MemCatSanMaxLen (&gMemTemp, psz, pszLibName);
                     if ((dwType == 2) || (dwType == 3))
                        MemCat (&gMemTemp, L"</font>");
                     MemCat (&gMemTemp, L"</xMenuButton>");

                     MemCat (&gMemTemp, L"</td>");
                  } // dwColumn

                  MemCat (&gMemTemp, L"</tr>");
               } // dwRow

               MemCat (&gMemTemp, L"</table><br/>");
               // put in separator
               //MemCat (&gMemTemp, L"<xMenuSeperator/>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"</small>");
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"</small>");

            } // if rows

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENUOBJECT")) {
            MemZero (&gMemTemp);

            // get library name
            CMem memLib;
            LibNameExtract (pLib, &memLib);
            PWSTR pszLibName = (PWSTR)memLib.p;

            DWORD i;
            DWORD dwNum = pLib ? pLib->ObjectNum() : 0;
            BOOL fSmallText = (dwNum >= TOOMANYITEMS);
            BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
            DWORD dwRows = min(dwNum, MENUROWS);
            DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
            if (dwColumns) {
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"<small>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"<small>");

               MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
               MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
               MemCat (&gMemTemp, L">");

               DWORD dwRow, dwColumn;
               for (dwRow = 0; dwRow < dwRows; dwRow++) {
                  MemCat (&gMemTemp, L"<tr>");

                  for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                     i = dwColumn * MENUROWS + dwRow;
                     if (i >= dwNum)
                        MemCat (&gMemTemp, L"<td/>");

                     PCMIFLObject pObj = pLib ? pLib->ObjectGet(i) : NULL;
                     if (!pObj)
                        continue;

                     MemCat (&gMemTemp, L"<td>");

                     MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                     MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                     MemCat (&gMemTemp, L"object:");
                     MemCat (&gMemTemp, (int)pObj->m_dwTempID);
                     MemCat (&gMemTemp, L"edit>");
                     PWSTR psz =(PWSTR)pObj->m_memName.p;
                     MemCatSanMaxLen (&gMemTemp, (psz && psz[0]) ? psz : L"No name", pszLibName);

                     // see if this is overridden
                     PWSTR pszHigher, pszLower;
                     pProj->ObjectOverridden (pLib->m_dwTempID, (PWSTR)pObj->m_memName.p, &pszHigher, &pszLower);
                     if (pszHigher)
                        MemCat (&gMemTemp, L" <italic>(Overridden)</italic>");
                     else if (pszLower)
                        MemCat (&gMemTemp, L" <italic>(Overrides)</italic>");

                     MemCat (&gMemTemp, L"</xMenuButton>");

                     MemCat (&gMemTemp, L"</td>");
                  } // dwColumn

                  MemCat (&gMemTemp, L"</tr>");
               } // dwRow

               MemCat (&gMemTemp, L"</table><br/>");
               // put in separator
               MemCat (&gMemTemp, L"<xMenuSeperator/>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"</small>");
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"</small>");

            } // if rows

            // menu for library list
            MemCat (&gMemTemp, L"<xMenuButton accel=l href=lib:");
            MemCat (&gMemTemp, (int) (pLib ? pLib->m_dwTempID : 0));
            MemCat (&gMemTemp, L"objectlist><u>L</u>ist of objects</xMenuButton>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MENUGLOBAL")) {
            MemZero (&gMemTemp);

            // get library name
            CMem memLib;
            LibNameExtract (pLib, &memLib);
            PWSTR pszLibName = (PWSTR)memLib.p;

            DWORD i;
            DWORD dwNum = pLib ? pLib->GlobalNum() : 0;
            BOOL fSmallText = (dwNum >= TOOMANYITEMS);
            BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
            DWORD dwRows = min(dwNum, MENUROWS);
            DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
            if (dwColumns) {
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"<small>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"<small>");

               MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
               MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
               MemCat (&gMemTemp, L">");

               DWORD dwRow, dwColumn;
               for (dwRow = 0; dwRow < dwRows; dwRow++) {
                  MemCat (&gMemTemp, L"<tr>");

                  for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                     i = dwColumn * MENUROWS + dwRow;
                     if (i >= dwNum)
                        MemCat (&gMemTemp, L"<td/>");

                     PCMIFLProp pProp = pLib ? pLib->GlobalGet(i) : NULL;
                     if (!pProp)
                        continue;

                     MemCat (&gMemTemp, L"<td>");

                     MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                     MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                     MemCat (&gMemTemp, L"global:");
                     MemCat (&gMemTemp, (int)pProp->m_dwTempID);
                     MemCat (&gMemTemp, L"edit>");
                     PWSTR psz =(PWSTR)pProp->m_memName.p;
                     MemCatSanMaxLen (&gMemTemp, (psz && psz[0]) ? psz : L"No name", pszLibName);

                     // see if this is overridden
                     PWSTR pszHigher, pszLower;
                     pProj->GlobalOverridden (pLib->m_dwTempID, (PWSTR)pProp->m_memName.p, &pszHigher, &pszLower);
                     if (pszHigher)
                        MemCat (&gMemTemp, L" <italic>(Overridden)</italic>");
                     else if (pszLower)
                        MemCat (&gMemTemp, L" <italic>(Overrides)</italic>");

                     MemCat (&gMemTemp, L"</xMenuButton>");

                     MemCat (&gMemTemp, L"</td>");
                  } // dwColumn

                  MemCat (&gMemTemp, L"</tr>");
               } // dwRow

               MemCat (&gMemTemp, L"</table><br/>");
               // put in separator
               MemCat (&gMemTemp, L"<xMenuSeperator/>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"</small>");
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"</small>");

            } // if rows

            // menu for library list
            MemCat (&gMemTemp, L"<xMenuButton accel=l href=lib:");
            MemCat (&gMemTemp, (int) (pLib ? pLib->m_dwTempID : 0));
            MemCat (&gMemTemp, L"globallist><u>L</u>ist of variables</xMenuButton>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"MENUFUNC")) {
            MemZero (&gMemTemp);

            // get library name
            CMem memLib;
            LibNameExtract (pLib, &memLib);
            PWSTR pszLibName = (PWSTR)memLib.p;

            DWORD i;
            DWORD dwNum = pLib ? pLib->FuncNum() : 0;
            BOOL fSmallText = (dwNum >= TOOMANYITEMS);
            BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
            DWORD dwRows = min(dwNum, MENUROWS);
            DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
            if (dwColumns) {
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"<small>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"<small>");

               MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
               MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
               MemCat (&gMemTemp, L">");

               DWORD dwRow, dwColumn;
               for (dwRow = 0; dwRow < dwRows; dwRow++) {
                  MemCat (&gMemTemp, L"<tr>");

                  for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                     i = dwColumn * MENUROWS + dwRow;
                     if (i >= dwNum)
                        MemCat (&gMemTemp, L"<td/>");

                     PCMIFLFunc pFunc = pLib ? pLib->FuncGet(i) : NULL;
                     if (!pFunc)
                        continue;

                     MemCat (&gMemTemp, L"<td>");

                     MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                     MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                     MemCat (&gMemTemp, L"func:");
                     MemCat (&gMemTemp, (int)pFunc->m_Meth.m_dwTempID);
                     MemCat (&gMemTemp, L"edit>");
                     PWSTR psz =(PWSTR)pFunc->m_Meth.m_memName.p;
                     MemCatSanMaxLen (&gMemTemp, (psz && psz[0]) ? psz : L"No name", pszLibName);

                     // see if this is overridden
                     PWSTR pszHigher, pszLower;
                     pProj->FuncOverridden (pLib->m_dwTempID, (PWSTR)pFunc->m_Meth.m_memName.p, &pszHigher, &pszLower);
                     if (pszHigher)
                        MemCat (&gMemTemp, L" <italic>(Overridden)</italic>");
                     else if (pszLower)
                        MemCat (&gMemTemp, L" <italic>(Overrides)</italic>");

                     MemCat (&gMemTemp, L"</xMenuButton>");

                     MemCat (&gMemTemp, L"</td>");
                  } // dwColumn

                  MemCat (&gMemTemp, L"</tr>");
               } // dwRow

               MemCat (&gMemTemp, L"</table><br/>");
               // put in separator
               MemCat (&gMemTemp, L"<xMenuSeperator/>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"</small>");
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"</small>");

            } // if rows

            // menu for library list
            MemCat (&gMemTemp, L"<xMenuButton accel=l href=lib:");
            MemCat (&gMemTemp, (int) (pLib ? pLib->m_dwTempID : 0));
            MemCat (&gMemTemp, L"funclist><u>L</u>ist of functions</xMenuButton>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

         else if (!_wcsicmp(p->pszSubName, L"MENUPROP")) {
            MemZero (&gMemTemp);

            // get library name
            CMem memLib;
            LibNameExtract (pLib, &memLib);
            PWSTR pszLibName = (PWSTR)memLib.p;

            DWORD i;
            DWORD dwNum = pLib ? pLib->PropDefNum() : 0;
            BOOL fSmallText = (dwNum >= TOOMANYITEMS);
            BOOL fVerySmallText = (dwNum >= WAYTOOMANYITEMS);
            DWORD dwRows = min(dwNum, MENUROWS);
            DWORD dwColumns = (dwNum + MENUROWS-1) / MENUROWS;
            if (dwColumns) {
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"<small>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"<small>");

               MemCat (&gMemTemp, L"<table innerlines=0 border=0 lrmargin=0 tbmargin=0 width=");
               MemCat (&gMemTemp, (int)dwColumns * MENUCOLUMNWIDTH);
               MemCat (&gMemTemp, L">");

               DWORD dwRow, dwColumn;
               for (dwRow = 0; dwRow < dwRows; dwRow++) {
                  MemCat (&gMemTemp, L"<tr>");

                  for (dwColumn = 0; dwColumn < dwColumns; dwColumn++) {
                     i = dwColumn * MENUROWS + dwRow;
                     if (i >= dwNum)
                        MemCat (&gMemTemp, L"<td/>");

                     PCMIFLProp pProp = pLib ? pLib->PropDefGet(i) : NULL;
                     if (!pProp)
                        continue;

                     MemCat (&gMemTemp, L"<td>");

                     MemCat (&gMemTemp, L"<xMenuButton href=lib:");
                     MemCat (&gMemTemp, (int)pLib->m_dwTempID);
                     MemCat (&gMemTemp, L"propdef:");
                     MemCat (&gMemTemp, (int)pProp->m_dwTempID);
                     MemCat (&gMemTemp, L"edit>");
                     PWSTR psz =(PWSTR)pProp->m_memName.p;
                     MemCatSanMaxLen (&gMemTemp, (psz && psz[0]) ? psz : L"No name", pszLibName);

                     // see if this is overridden
                     PWSTR pszHigher, pszLower;
                     pProj->PropDefOverridden (pLib->m_dwTempID, (PWSTR)pProp->m_memName.p, &pszHigher, &pszLower);
                     if (pszHigher)
                        MemCat (&gMemTemp, L" <italic>(Overridden)</italic>");
                     else if (pszLower)
                        MemCat (&gMemTemp, L" <italic>(Overrides)</italic>");

                     MemCat (&gMemTemp, L"</xMenuButton>");

                     MemCat (&gMemTemp, L"</td>");
                  } // dwColumn

                  MemCat (&gMemTemp, L"</tr>");
               } // dwRow

               MemCat (&gMemTemp, L"</table><br/>");
               // put in separator
               MemCat (&gMemTemp, L"<xMenuSeperator/>");
               if (fSmallText)
                  MemCat (&gMemTemp, L"</small>");
               if (fVerySmallText)
                  MemCat (&gMemTemp, L"</small>");

            } // if rows

            // menu for library list
            MemCat (&gMemTemp, L"<xMenuButton accel=l href=lib:");
            MemCat (&gMemTemp, (int) (pLib ? pLib->m_dwTempID : 0));
            MemCat (&gMemTemp, L"propdeflist><u>L</u>ist of properties</xMenuButton>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }

      }
      break;
   }

   // else, dont deal with
   return FALSE;
}

/*********************************************************************************
MIFLComboLibs - Fill in a combo-box with a list of libs that can transfer to.

inputs
   PCEscPage         pPage - Page for CB
   PWSTR             pszName - Control name
   PWSTR             pszButton - Button that press to transfer. If cant transfer
                        anywhere then wil ldisable
   PCMIFLProj        pProj - Project
   DWORD             dwExclude - Library ID to exclude (cant transfer to self)
returns
   BOOL - TRUE if success
*/
BOOL MIFLComboLibs (PCEscPage pPage, PWSTR pszName, PWSTR pszButton, PCMIFLProj pProj, DWORD dwExclude)
{
   PCEscControl pControl = pPage->ControlFind (pszName);
   if (!pControl)
      return TRUE;

   // create list
   MemZero (&gMemTemp);
   DWORD dwNum = 0;
   DWORD dwFirst = 0;
   DWORD i;
   for (i = 0; i < pProj->LibraryNum(); i++) {
      PCMIFLLib pLib = pProj->LibraryGet(i);
      if (!pLib || pLib->m_fReadOnly || (pLib->m_dwTempID == dwExclude))
         continue;

      // else, add this
      MemCat (&gMemTemp, L"<elem name=");
      MemCat (&gMemTemp, (int)i);
      MemCat (&gMemTemp, L">");
      MemCatSanitize (&gMemTemp, LibraryDisplayName(pLib->m_szFile));
      MemCat (&gMemTemp, L"</elem>");
      if (!dwNum)
         dwFirst = i;
      dwNum++;
   } // i

   // set the combo
   ESCMCOMBOBOXADD add;
   memset (&add, 0, sizeof(add));
   add.dwInsertBefore = 0; // BUGFIX - Use instead of -1 so that in correct order
   add.pszMML = (PWSTR)gMemTemp.p;
   pControl->Message (ESCM_COMBOBOXRESETCONTENT);
   pControl->Message (ESCM_COMBOBOXADD, &add);

   // default selection
   pControl->AttribSetInt (CurSel(), 0);

   // disable?
   if (!dwNum) {
      pControl->Enable (FALSE);

      pControl = pPage->ControlFind (pszButton);
      if (pControl)
         pControl->Enable (FALSE);
   }

   return TRUE;
}

/*********************************************************************************
MIFLRedoSamePage - This is a special redo-page that maintains the current scroll
loc. It can only be called if the current object is guaranteed to not go out of
content
*/
PWSTR MIFLRedoSamePage (void)
{
   return L"MIFLRedoSamePage";
}


/*********************************************************************************
MIFLStringArrayComp - Takes an array of PWSTR and returns the element (index)
that comes first in sorted order. In the process, it also zeros out all pointers in
the array except those matching the lowest string. (So it's easy to tell if several
strings are duplicates on different lists)

inputs
   PWSTR       *ppsz - Pointert to an array of strings. Can initially be NULL.
                        This list will be changed so everything will be set to NULL
                        except entries that are the same as the lowest string
   DWORD       dwNum - Number in ppsz
returns
   DWORD - Index for the lowest string, or -1 if no lowest string (they're all null)
*/
DWORD MIFLStringArrayComp (PWSTR *ppsz, DWORD dwNum)
{
   DWORD dwIndex = -1;
   DWORD i;

   for (i = 0; i < dwNum; i++) {
      if (!ppsz[i])
         continue;   // NULL

      // if index == -1 then snap this up
      if (dwIndex == -1) {
         dwIndex = i;
         continue;
      }

      // compare
      int iRet = _wcsicmp (ppsz[i], ppsz[dwIndex]);
      if (iRet > 0) {
         // this is higher than the lowest rank, so zero it out and contnue
         ppsz[i] = NULL;
         continue;
      }
      else if (iRet == 0)
         continue;   // equal to the lowest rank so do nothing

      // else, less than the lowest rank, so new lowest
      dwIndex = i;
      memset (ppsz, 0, (dwIndex) * sizeof(PWSTR));
   } // i

   return dwIndex;
}



/*********************************************************************************
MIFLLangMatch - Given a PCListFixed containing LANGID's, this searches through
all the languages and finds the one closest to what looking for. The index for
this one is returned.

inputs
   PCListFixed       plLANGID - List of language IDs
   LANGID            lid - Looking for this
   BOOL              fExactPrimary - If TRUE then require exact primary ID match
returns
   DWORD - Index into plLANGID for closest match
*/
DWORD MIFLLangMatch (PCListFixed plLANGID, LANGID lid, BOOL fExactPrimary)
{
   DWORD dwNum = plLANGID->Num();
   if (dwNum == 0)
      return -1;
   else if (dwNum == 1)
      return 0;   // no choice

   LANGID *pl = (LANGID*)plLANGID->Get(0);
   DWORD i, dwScore;
   DWORD dwBest = -1;
   DWORD dwBestScore = 0;

   WORD wPrim = PRIMARYLANGID(lid);

   for (i = 0; i < dwNum; i++) {
      if (lid == pl[i]) {
         return i;   // since this is always the best
      }
      else if (wPrim == PRIMARYLANGID(pl[i]))
         dwScore = 3;
      else if (i == 0)
         dwScore = 2;
      else
         dwScore = 0;

      if ((dwBest == -1) || (dwScore > dwBestScore)) {
         dwBest  = i;
         dwBestScore = dwScore;
      }
   } // i

   if (fExactPrimary && (dwBestScore < 3))
      return (DWORD)-1;

   return dwBest;
}

/*********************************************************************************
MIFLToLower - Lowercases a string

inputs
   PWSTR       psz
*/
void MIFLToLower (PWSTR psz)
{
   for (; psz[0]; psz++)
      psz[0] = towlower(psz[0]);
}


/*********************************************************************************
MIFLIsNameValid - Returns TRUE if the name is valid for a function, var, etc. name,
FALSE if it isn't.

inputs
   PWSTR       pszName - Name
returns
   BOOL - TRUE if valid
*/
BOOL MIFLIsNameValid (PWSTR pszName)
{
   // must be at least something
   if (!pszName[0])
      return FALSE;

   if (iswdigit (pszName[0]))
      return FALSE;  // first cant be a digit

   for (; pszName[0]; pszName++) {
      if (!iswalpha(pszName[0]) && !iswdigit(pszName[0]) && (pszName[0] != L'_'))
         return FALSE;
   }

   return TRUE;
}




/*****************************************************************************
CMIFLVarString::Constructor and destructor

NOTE: Don't call delete directly
*/
CMIFLVarString::CMIFLVarString (void)
{
   m_dwRefCount = 1;
}


/*****************************************************************************
CMIFLVarString::Length - Returns the length, in characters
*/
//DWORD CMIFLVarString::Length (void)
//{
//   return m_memString.m_dwCurPosn / sizeof(WCHAR);
//}

/*****************************************************************************
CMIFLVarString::Release - Reduces the reference count by one. If the reference
count goes to 0 then it's deleted.

retursn
   DWORD - Reference count after the release
*/
DWORD CMIFLVarString::Release (void)
{
   m_dwRefCount--;

   if (!m_dwRefCount) {
      delete this;
      return 0;
   }

   return m_dwRefCount;
}


/*****************************************************************************
CMIFLVarString::AddRef - Increases the reference count by 1.

returns
   DWORD - New reference count after it's increased
*/
DWORD CMIFLVarString::AddRef (void)
{
   m_dwRefCount++;
   return m_dwRefCount;
}


/*****************************************************************************
CMIFLVarString::Clone - Creates a new copy, but with a refcount of 1
*/
PCMIFLVarString CMIFLVarString::Clone (void)
{
   PCMIFLVarString pNew = new CMIFLVarString;
   if (!pNew)
      return NULL;

   PWSTR psz = Get();
   pNew->Set (psz, Length());
   return pNew;
}

/*****************************************************************************
CMIFLVarString::MMLTo - Writes the string to a MMLNode.

inputs
   none
returns
   PCMMLNode2         pNode - New node
*/
static PWSTR gpszMIFLVarString = L"MIFLVarString";
static PWSTR gpszString = L"String";
PCMMLNode2 CMIFLVarString::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLVarString);

   PWSTR psz = (PWSTR)m_memString.p;
   if (psz[0])
      MMLValueSet (pNode, gpszString, psz);

   return pNode;
}


/*****************************************************************************
CMIFLVarString::MMLFrom - Reads MML into string

inputs
   PCMMLNode2         pNode - NOde to read from
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarString::MMLFrom (PCMMLNode2 pNode)
{
   PWSTR psz = MMLValueGet (pNode, gpszString);
   Set (psz ? psz : L"", (DWORD)-1);
   return TRUE;
}

/*****************************************************************************
CMIFLVarString::Set - Sets the string based on on a C++ string

inputs
   PWSTR          psz - String
   DWORD          dwLength - Length of psz in characters, or (DWORD)-1 if unknown
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarString::Set (PWSTR psz, DWORD dwLength)
{
   if (dwLength == (DWORD)-1)
      dwLength = (DWORD)wcslen(psz);
   DWORD dwCopy = dwLength * sizeof(WCHAR);
   DWORD dwNeed = dwCopy + sizeof(WCHAR);

   if (!m_memString.Required (dwNeed))
      return FALSE;

   memcpy (m_memString.p, psz, dwCopy);
   ((PWSTR)m_memString.p)[dwLength] = 0;
   m_memString.m_dwCurPosn = dwCopy;
   return TRUE;
}




/*****************************************************************************
CMIFLVarString::Set - Sets the string at a specific index.

inputs
   DWORD             dwIndex - Index
   PCMIFLVar         pVar - To set it to. NOTE: This is converted to a character in place
   PCMIFLVM          pVM - SO can convert from string table to string
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarString::Set (DWORD dwIndex, PCMIFLVar pVar, PCMIFLVM pVM)
{
   DWORD dwLen = (dwIndex+2)*sizeof(WCHAR);  // BUGFIX - Was dwIndex+1, should be dwIndex+2 for NULL-term
   if ((dwLen > m_memString.m_dwAllocated) && !m_memString.Required (dwLen))
      return FALSE;
   PWSTR psz = (PWSTR) m_memString.p;

   BOOL fPastEOS = FALSE;
   DWORD i;
   for (i = 0; i <= dwIndex; i++) {
      if (fPastEOS)
         psz[i] = L' ';
      else if (!psz[i]) {
         fPastEOS = TRUE;
         psz[i] = L' ';
      }
   }

   pVar->ToChar(pVM);
   psz[dwIndex] = pVar->GetChar(pVM);
   if (fPastEOS)
      psz[dwIndex+1] = 0;

   // get new length
   m_memString.m_dwCurPosn = wcslen(psz) * sizeof(WCHAR);

   return TRUE;
}


/*****************************************************************************
CMIFLVarString::Get - Gets the string. Note: Do not change
*/
PWSTR CMIFLVarString::Get (void)
{
   if (!m_memString.p)
      Set (L"", (DWORD)-1);  // so have something

   return (PWSTR)m_memString.p;
}



/*****************************************************************************
CMIFLVarString::Prepend - Adds the string to the beginning.

inputs
   PWSTR          psz - String
   DWORD          dwLength - Length of psz in characers, or -1 if unknown
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarString::Prepend (PWSTR psz, DWORD dwLength)
{
   if (!m_memString.p) {
      Set (psz, dwLength);
      return TRUE;
   }

   // else move...
   DWORD dwNew = (dwLength == (DWORD)-1) ? (DWORD)wcslen(psz) : dwLength;
   DWORD dwOrig = (DWORD)m_memString.m_dwCurPosn / sizeof(WCHAR);
   if (!m_memString.Required ((dwNew+dwOrig+1)*sizeof(WCHAR)))
      return FALSE;
   memmove ((PWSTR)m_memString.p + dwNew, m_memString.p, (dwOrig+1)*sizeof(WCHAR));
   memcpy (m_memString.p, psz, dwNew * sizeof(WCHAR));

   m_memString.m_dwCurPosn = (dwNew + dwOrig) * sizeof(WCHAR);

   return TRUE;
}




/*****************************************************************************
CMIFLVarString::Append - Adds the string to the end.

inputs
   PWSTR          psz - String
   DWORD          dwLength - Length of psz in characers, or -1 if unknown
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarString::Append (PWSTR psz, DWORD dwLength)
{
   if (!m_memString.p) {
      Set (psz, dwLength);
      return TRUE;
   }

   // else move...
   DWORD dwNew = (dwLength == (DWORD)-1) ? (DWORD)wcslen(psz) : dwLength;
   DWORD dwOrig = (DWORD)m_memString.m_dwCurPosn / sizeof(WCHAR);
   if (!m_memString.Required ((dwNew+dwOrig+1)*sizeof(WCHAR)))
      return FALSE;
   memcpy ((PWSTR)m_memString.p + dwOrig, psz, (dwNew+1)*sizeof(WCHAR));

   m_memString.m_dwCurPosn = (dwNew + dwOrig) * sizeof(WCHAR);

   return TRUE;
}


/*****************************************************************************
CMIFLVarString::Replace - Replaces the range (from iStart to iEnd) with the
given string in psz.

inputs
   PWSTR          psz - To repalce with. Can be NULL
   DWORD          dwLength - Length of psz, or -1 if unknown
   int            iStart - start character. Defaults to 0
   int            iEnd - End character. Defaults to 1000000000
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarString::Replace (PWSTR psz, DWORD dwLength, int iStart, int iEnd)
{
   int iLen = (int)m_memString.m_dwCurPosn / sizeof(WCHAR);
   int iLen2 = (dwLength == (DWORD)-1) ? (psz ? (DWORD)wcslen(psz) : 0) : (int)dwLength;

   iStart = max(iStart, 0);
   iStart = min(iStart, iLen);
   iEnd = max(iEnd, iStart);
   iEnd = min(iEnd, iLen);

   int iDelta = iLen2 - (iEnd - iStart);

   if (!m_memString.Required ((BOOL) (iLen+1+iDelta)*sizeof(WCHAR)))
      return FALSE;

   memmove ((PWSTR)m_memString.p + (iEnd+iDelta), (PWSTR)m_memString.p + iEnd,
      (iLen+1 - iEnd)*sizeof(WCHAR));
   if (iLen2)
      memcpy ((PWSTR)m_memString.p + iStart, psz, iLen2 * sizeof(WCHAR));

   m_memString.m_dwCurPosn = (DWORD)(iLen+iDelta) * sizeof(WCHAR);

   return TRUE;
}

/*****************************************************************************
CMIFLVarString::Trim - Trims whitespace off the left/right.

inputs
   int         iTrim - If 0 both left and right, -1 then lenft only, 1 then right only
*/
void CMIFLVarString::Trim (int iTrim)
{
   PWSTR psz = (PWSTR)m_memString.p;

   DWORD dwLen = (DWORD)wcslen(psz);
   if (iTrim >= 0) {
      // trim the end
      while (dwLen && iswspace (psz[dwLen-1]))
         dwLen--;

      psz[dwLen] = 0;
   }

   if (iTrim <= 0) {
      // trim the start
      DWORD i;
      for (i = 0; psz[i] && iswspace(psz[i]); i++);

      memmove (psz, psz + i, (dwLen - i + 1)*sizeof(WCHAR));
   }

   m_memString.m_dwCurPosn = wcslen(psz) * sizeof(WCHAR);
}


/*****************************************************************************
CMIFLVarString::Truncate - Truncates to the given lenth if it's longer.

inputs
   DWORD       dwChar - Number of characters
   BOOL        fEllipsis - If true add "..." to end if truncate
*/
void CMIFLVarString::Truncate (DWORD dwChar, BOOL fEllipses)
{
   if (dwChar * sizeof(WCHAR) >= m_memString.m_dwAllocated)
      return;  // too far

   PWSTR psz = (PWSTR)m_memString.p;
   DWORD dwLen = (DWORD)wcslen(psz);
   if (dwLen <= dwChar)
      return;

   // else truncate
   psz[dwChar] = 0;

   m_memString.m_dwCurPosn = dwChar * sizeof(WCHAR);

   if (fEllipses)
      Append (L"...", (DWORD)-1);
}




/*****************************************************************************
CMIFLVarString::CharGet - Gets a specific character.

inputs
   DWORD       dwChar - Character number
returns
   PWSTR - Character, or NULL if outside teh string
*/
PWSTR CMIFLVarString::CharGet (DWORD dwChar)
{
   if (dwChar >= m_memString.m_dwCurPosn / sizeof(WCHAR))
      return NULL;  // too far

   PWSTR psz = (PWSTR)m_memString.p;
   return psz + dwChar;
}





/*****************************************************************************
CMIFLVarLValue::Constructor and destructor
*/
CMIFLVarLValue::CMIFLVarLValue (void)
{
   Clear();
}

//CMIFLVarLValue::~CMIFLVarLValue (void)
//{
//   // nothing for now
//}


/*****************************************************************************
CMIFLVarLValue::Clear - Wipes out the m_dwLValue
*/
//__inline void CMIFLVarLValue::Clear (void)
//{
//   m_dwLValue = MLV_NONE;
//}

/*****************************************************************************
CMIFLVar::Constructor and destructor
*/
CMIFLVar::CMIFLVar (void)
{
   InitInt();
}

CMIFLVar::~CMIFLVar (void)
{
   ReleaseInt();
}

/*****************************************************************************
IsPrivateMethod - Returns String if the given ID is a private method

inputs
   DWORD       dwValue - From m_dwValue
   PCMIFLVM    pVM - VM
returns
   PWSTR - String of the object's name if it's a private method to an object, else
         NULL if it's not
*/
PWSTR IsPrivateMethod (DWORD dwValue, PCMIFLVM pVM)
{
   PMIFLIDENT pmi = (PMIFLIDENT) pVM->m_pCompiled->m_hUnIdentifiers.Find (dwValue);
   if (!pmi)
      return NULL;  // assume not

   if (pmi->dwType != MIFLI_METHPRIV)
      return NULL;

   // else, it's private
   PCMIFLFunc pFunc = (PCMIFLFunc) pmi->pEntity;
   if (!pFunc || !pFunc->m_pObjectPrivate)
      return NULL;

   return (PWSTR)pFunc->m_pObjectPrivate->m_memName.p;
}

/*****************************************************************************
CMIFLVar::MMLTo - Writes the variable information to MML.

NOTE: Because strings and lists are shared amongst objects, this won't
write the actual string, but will put an entry in a string/list hash, and
then use that index. The entry for the string/list will need to be written
later.

inputs
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length
   PCHashPVoid       phList - Hash of list pointer to ID. 0-lenght
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszMIFLVar = L"MIFLVar";
static PWSTR gpszType = L"Type";
static PWSTR gpszValue = L"Value";
static PWSTR gpszValueS = L"ValueS";
static PWSTR gpszPrivate = L"Private"; // set to string of object if this is a private method

PCMMLNode2 CMIFLVar::MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLVar);

   MMLValueSet (pNode, gpszType, (int)m_dwType);
   switch (m_dwType) {
   case MV_UNDEFINED:
   case MV_NULL:
      break;

   case MV_BOOL:
   case MV_CHAR:
      MMLValueSet (pNode, gpszValue, (int)m_dwValue);
      break;

   case MV_DOUBLE:
      MMLValueSet (pNode, gpszValue, (fp) m_fValue);
      // NOTE: MMLSet() will only store a limited accuracy, but shouldn't be aproblem
      // DOCUMENT: When save session may losed some accuracy (<.0001) in numbers
      break;

   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGMETH:
      {
         DWORD dwIndex = phString->FindIndex (m_pValue);
         if (dwIndex == -1) {
            phString->Add (m_pValue, NULL);
            dwIndex = phString->FindIndex (m_pValue);
         }
         if (dwIndex == -1) {
            delete pNode;
            return NULL;
         }
         MMLValueSet (pNode, gpszValue, (int)dwIndex);

         if (m_dwType == MV_STRINGMETH) {
            CMIFLVar vMeth;
            vMeth.SetMeth (m_dwValue);
            PCMIFLVarString ps = vMeth.GetString(pVM);
            MMLValueSet (pNode, gpszValueS, ps->Get());
            ps->Release();
         }
      }
      break;   // do nothing

   case MV_LIST:       // memory points to PCMIFLVarList
   case MV_LISTMETH:
      {
         DWORD dwIndex = phList->FindIndex (m_pValue);
         if (dwIndex == -1) {
            phList->Add (m_pValue, NULL);
            dwIndex = phList->FindIndex (m_pValue);
         }
         if (dwIndex == -1) {
            delete pNode;
            return NULL;
         }
         MMLValueSet (pNode, gpszValue, (int)dwIndex);

         if (m_dwType == MV_LISTMETH) {
            CMIFLVar vMeth;
            vMeth.SetMeth (m_dwValue);
            PCMIFLVarString ps = vMeth.GetString(pVM);
            MMLValueSet (pNode, gpszValueS, ps->Get());
            ps->Release();
         }
      }
      break;   // do nothing

   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         PCMIFLString ps = pVM->m_pCompiled->m_pLib->StringGet (m_dwValue);
         if (!ps) {
            delete pNode;
            return NULL;
         }
         MMLValueSet (pNode, gpszValueS, (PWSTR)ps->m_memName.p);
      }
      break;

   case MV_RESOURCE:       // resource in table. dwValue is the ID
      {
         PCMIFLResource ps = pVM->m_pCompiled->m_pLib->ResourceGet (m_dwValue);
         if (!ps) {
            delete pNode;
            return NULL;
         }
         MMLValueSet (pNode, gpszValueS, (PWSTR)ps->m_memName.p);
      }
      break;

   case MV_METH:       // public/private method, no object. dwValue is the ID
   case MV_FUNC:       // function call. dwValue is the ID
      {
         // see if it's private
         PWSTR psz = IsPrivateMethod (m_dwValue, pVM);
         if (psz)
            MMLValueSet (pNode, gpszPrivate, psz);


         PCMIFLVarString ps = GetString(pVM);
         MMLValueSet (pNode, gpszValueS, ps->Get());
         ps->Release();
      }
      break;

   case MV_OBJECT:       // object GUID stored
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
      {
         // write out the object
         MMLValueSet (pNode, gpszValue, (PBYTE)&m_gValue, sizeof(m_gValue));

         if (m_dwType == MV_OBJECTMETH) {
            // see if it's private
            PWSTR psz = IsPrivateMethod (m_dwValue, pVM);
            if (psz)
               MMLValueSet (pNode, gpszPrivate, psz);

            // next, get the name of the method
            CMIFLVar vMeth;
            vMeth.SetMeth (m_dwValue);
            PCMIFLVarString ps = vMeth.GetString(pVM);
            MMLValueSet (pNode, gpszValueS, ps->Get());
            ps->Release();
         }
      }
      break;


   default:
      // shouldnt happen
      delete pNode;
      return NULL;
   }

   return pNode;
}



/*****************************************************************************
CMIFLVar::MMLFrom - Reads the variable information from MML.

NOTE: Because strings and lists are shared amongst objects, this won't
write the actual string, but will put an entry in a string/list hash, and
then use that index. The entry for the string/list will need to be written
later.

inputs
   PCMMLNode2         pNode - Node
   PCMIFLVM          pVM - VM
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
returns
   BOOL- TRUE if success
*/
BOOL CMIFLVar::MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phList,
                        PCHashGUID phObjectRemap)
{
   DWORD dwType = (int)MMLValueGetInt (pNode, gpszType, MV_UNDEFINED);

   switch (dwType) {
   case MV_UNDEFINED:
      SetUndefined();
      break;

   case MV_NULL:
      SetNULL();
      break;

   case MV_BOOL:
      SetBOOL ((BOOL) MMLValueGetInt (pNode, gpszValue, (int)0));
      break;
   case MV_CHAR:
      SetChar ((WORD) MMLValueGetInt (pNode, gpszValue, (int)0));
      break;

   case MV_DOUBLE:
      SetDouble (MMLValueGetDouble (pNode, gpszValue, (fp) 0));
      break;

   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGMETH:
      {
         DWORD dwIndex = (DWORD) MMLValueGetInt (pNode, gpszValue, 0);
         PCMIFLVarString *pps = (PCMIFLVarString*)phString->Find(dwIndex);
         if (!pps) {
            SetUndefined();
            return FALSE;
         }

         if (dwType == MV_STRINGMETH) {
            PWSTR psz = MMLValueGet (pNode, gpszValueS);
            DWORD dwID = psz ? pVM->ToMethodID (psz, NULL, TRUE) : -1;
            if (dwID == -1) {
               SetUndefined();
               return FALSE;
            }
            SetStringMeth (pps[0], dwID);
         }
         else
            SetString (pps[0]);
      }
      break;

   case MV_LIST:       // memory points to PCMIFLVarList
   case MV_LISTMETH:       // memory points to PCMIFLVarList
      {
         DWORD dwIndex = (DWORD) MMLValueGetInt (pNode, gpszValue, 0);
         PCMIFLVarList *pps = (PCMIFLVarList*)phList->Find(dwIndex);
         if (!pps) {
            SetUndefined();
            return FALSE;
         }

         if (dwType == MV_LISTMETH) {
            PWSTR psz = MMLValueGet (pNode, gpszValueS);
            DWORD dwID = psz ? pVM->ToMethodID (psz, NULL, TRUE) : -1;
            if (dwID == -1) {
               SetUndefined();
               return FALSE;
            }
            SetListMeth (pps[0], dwID);
         }
         else
            SetList (pps[0]);
      }
      break;   // do nothing

   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         PWSTR psz = MMLValueGet (pNode, gpszValueS);
         PMIFLIDENT pmi = psz ? (PMIFLIDENT)pVM->m_pCompiled->m_hIdentifiers.Find (psz, TRUE) : NULL;
         if (!pmi || (pmi->dwType != MIFLI_STRING)) {
            SetUndefined();
            return FALSE;
         }
         SetStringTable (pmi->dwID);
      }
      break;

   case MV_RESOURCE:       // resource in table. dwValue is the ID
      {
         PWSTR psz = MMLValueGet (pNode, gpszValueS);
         PMIFLIDENT pmi = psz ? (PMIFLIDENT)pVM->m_pCompiled->m_hIdentifiers.Find (psz, TRUE) : NULL;
         if (!pmi || (pmi->dwType != MIFLI_RESOURCE)) {
            SetUndefined();
            return FALSE;
         }
         SetResource (pmi->dwID);
      }
      break;

   case MV_METH:       // public/private method, no object. dwValue is the ID
      {
         DWORD dwID;
         PWSTR pszPrivate = MMLValueGet (pNode, gpszPrivate);

         PWSTR psz = MMLValueGet (pNode, gpszValueS);
         dwID = psz ? pVM->ToMethodID (psz, pszPrivate, TRUE) : -1;
         if (dwID == -1) {
            SetUndefined();
            return FALSE;
         }
         SetMeth (dwID);
      }
      break;

   case MV_FUNC:       // function call. dwValue is the ID
      {
         PWSTR psz = MMLValueGet (pNode, gpszValueS);
         PMIFLIDENT pmi = psz ? (PMIFLIDENT)pVM->m_pCompiled->m_hIdentifiers.Find (psz, TRUE) : NULL;
         if (!pmi || (pmi->dwType != MIFLI_FUNC)) {
            SetUndefined();
            return FALSE;
         }
         SetFunc (pmi->dwID);
      }
      break;

   case MV_OBJECT:       // object GUID stored
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
      {
         // write out the object
         GUID g;
         if (sizeof(g) != MMLValueGetBinary (pNode, gpszValue, (PBYTE)&g, sizeof(g))) {
            SetUndefined();
            return FALSE;
         }

         GUID *pgRemap = phObjectRemap ? (GUID*)phObjectRemap->Find (&g) : NULL;
            // BUGFIX - Allow for NULL phObjectRemap
         if (pgRemap)
            g = *pgRemap;

         if (dwType == MV_OBJECTMETH) {

            PWSTR pszPrivate = MMLValueGet (pNode, gpszPrivate);
            DWORD dwID;

            PWSTR psz = MMLValueGet (pNode, gpszValueS);
            dwID = psz ? pVM->ToMethodID (psz, pszPrivate, TRUE) : -1;
            if (dwID == -1) {
               SetUndefined();
               return FALSE;
            }
            SetObjectMeth (&g, dwID);
         }
         else
            SetObject (&g);
      }
      break;


   default:
      // shouldnt happen
      return FALSE;
   }

   return TRUE;
}

/*****************************************************************************
CMIFLVar::InitInt - Call init if the var is instantiated without going through
the constructor
*/
void CMIFLVar::InitInt (void)
{
   m_dwType = MV_UNDEFINED;
   // moved to CMIFLVarLValue m_dwLValue = MLV_NONE;
}


/*****************************************************************************
CMIFLVar::ReleaseInt - This internal function release the refreence count for the
string or list, if they're there
*/
void CMIFLVar::ReleaseInt (void)
{
   // Moved to CMIFLVarLValue... home not important for running... m_dwLValue = MLV_NONE;

   switch (m_dwType) {
   case MV_STRING:
   case MV_STRINGMETH:
      ((PCMIFLVarString)m_pValue)->Release();
      break;
   case MV_LIST:
   case MV_LISTMETH:
      ((PCMIFLVarList)m_pValue)->Release();
      break;
   }
}

/*****************************************************************************
CMIFLVar::AddRef - Increases the reference count on the strings/lists if it
is one.
*/
void CMIFLVar::AddRef (void)
{
   switch (m_dwType) {
   case MV_STRING:
   case MV_STRINGMETH:
      ((PCMIFLVarString)m_pValue)->AddRef();
      break;
   case MV_LIST:
   case MV_LISTMETH:
      ((PCMIFLVarList)m_pValue)->AddRef();
      break;
   }
}



/*****************************************************************************
CMIFLVar::Fracture - This is kind of the opposite of AddRef(). What it does
is assume that CMIFLVar has been copied, so new versions of the strings and
lists must be created.

BUGBUG - Need to modify Fracture(FALSE) so it doesn't muck with the addrefs
at all, allowing for a VM to be initialized from a different thread without
a chance of two VMs (that are being initialized) stepping on one another's
ref counts

inputs
   BOOL           fReleaseOld - If TRUE then release the old version, FALSE then
                     keep old ref-count the same
*/
void CMIFLVar::Fracture (BOOL fReleaseOld)
{
   switch (m_dwType) {
   case MV_STRING:
   case MV_STRINGMETH:
      {
         PCMIFLVarString pOrig = (PCMIFLVarString)m_pValue;
         m_pValue = pOrig->Clone();
         if (fReleaseOld)
            pOrig->Release();
         if (!m_pValue)
            m_dwType = MV_UNDEFINED;   // error
      }
      break;
   case MV_LIST:
   case MV_LISTMETH:
      {
         PCMIFLVarList pOrig = (PCMIFLVarList)m_pValue;
         m_pValue = pOrig->Clone();
         if (fReleaseOld)
            pOrig->Release();
         if (!m_pValue)
            m_dwType = MV_UNDEFINED;   // error
      }
      break;
   }
}




/*****************************************************************************
CMIFLVar::Remap - Remaps from old list of object GUIDs to new GUID.
This is used along with fracture to clone objects.

inputs
   PCHashGUID        phOrigToNew - Map of the original GUID to the new GUID
returns
   none
*/
void CMIFLVar::Remap (PCHashGUID phOrigToNew)
{
   GUID *pgNew;

   switch (m_dwType) {
   case MV_OBJECT:
   case MV_OBJECTMETH:
      pgNew = (GUID*) phOrigToNew->Find (&m_gValue);
      if (pgNew)
         m_gValue = *pgNew;
      break;

   case MV_LIST:
   case MV_LISTMETH:
      {
         PCMIFLVarList pOrig = (PCMIFLVarList)m_pValue;
         PCMIFLVar pv;
         DWORD i;
         DWORD dwNum = pOrig->Num();
         for (i = 0; i < dwNum; i++) {
            pv = pOrig->Get (i);
            if (pv)
               pv->Remap (phOrigToNew);
         } // i
      }
      break;
   }
}


/*****************************************************************************
CMIFLVar::OperXXX - Does various operators on the data
*/
void CMIFLVar::OperNegation (PCMIFLVM pVM)
{
   // DOCUMENT: Negation converts to double then flips sign
   SetDouble(GetDouble(pVM) * -1);
}

void CMIFLVar::OperBitwiseNot (PCMIFLVM pVM)
{
   // DOCUMENT: Bitwise not converts all to numbers
   SetDouble((double) (DWORD) (~((DWORD)GetDouble(pVM))) );
}

void CMIFLVar::OperLogicalNot (PCMIFLVM pVM)
{
   // DOCUMENT: Logical not converts to bool first
   SetBOOL(!GetBOOL(pVM));
}

void CMIFLVar::OperMultiply (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Multiply converts to double first
   SetDouble (pVarL->GetDouble(pVM) * pVarR->GetDouble(pVM));
}

void CMIFLVar::OperDivide (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Divide to double first
   // DOCUMENT: Dibide by 0
   SetDouble (pVarL->GetDouble(pVM) / pVarR->GetDouble(pVM));
}


void CMIFLVar::OperModulo (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Modulo converts to double
   // DOCUMENT: Divide by 0
   // DOCUMENT: Fixed so correct modulo for negative numbers
   SetDouble (myfmod(pVarL->GetDouble(pVM), pVarR->GetDouble(pVM)));
}

void CMIFLVar::OperAdd (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   switch (pVarL->TypeGet()) {
   case MV_STRING:
      // DOCUMENT: String add - note that doesn't add to original
      {
         Set (pVarL);
         Fracture ();

         PCMIFLVarString psL = GetString (pVM);
         PCMIFLVarString psR = pVarR->GetString (pVM);
         if (!psL || !psR) {
            if (psL)
               psL->Release();
            if (psR)
               psR->Release();
            return;
         }

         psL->Append (psR);

         psL->Release();
         psR->Release();
      }
      break;

   case MV_LIST:
      // DOCUMENT: List add - note how doesn't add to original, but copies
      {
         Set (pVarL);
         Fracture ();

         PCMIFLVarList ps = GetList ();
         ps->Add (pVarR, FALSE);  // note: deconstructing any sublists
         ps->Release();
      }
      break;

   default:
      // DOCUMENT: Add to double first
      SetDouble (pVarL->GetDouble(pVM) + pVarR->GetDouble(pVM));
   }
}

void CMIFLVar::OperSubtract (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Subtract to double first
   SetDouble (pVarL->GetDouble(pVM) - pVarR->GetDouble(pVM));
}


void CMIFLVar::OperBitwiseLeft (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Bitwise left converts all to DWORD
   SetDouble((DWORD)pVarL->GetDouble(pVM) << (DWORD)pVarR->GetDouble(pVM));
}

void CMIFLVar::OperBitwiseRight (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Bitwise right converts all to DWORD
   SetDouble((DWORD)pVarL->GetDouble(pVM) >> (DWORD)pVarR->GetDouble(pVM));
}

void CMIFLVar::OperBitwiseAnd (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Bitwise and converts all to DWORD
   SetDouble((DWORD)pVarL->GetDouble(pVM) & (DWORD)pVarR->GetDouble(pVM));
}

void CMIFLVar::OperBitwiseXOr (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Bitwise xor converts all to DWORD
   SetDouble((DWORD)pVarL->GetDouble(pVM) ^ (DWORD)pVarR->GetDouble(pVM));
}

void CMIFLVar::OperBitwiseOr (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Bitwise or converts all to DWORD
   SetDouble((DWORD)pVarL->GetDouble(pVM) | (DWORD)pVarR->GetDouble(pVM));
}

void CMIFLVar::OperLogicalAnd (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Local and converts all to bool
   SetBOOL(pVarL->GetBOOL(pVM) && pVarR->GetBOOL(pVM));
}

void CMIFLVar::OperLogicalOr (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR)
{
   // DOCUMENT: Local or converts all to bool
   SetBOOL(pVarL->GetBOOL(pVM) || pVarR->GetBOOL(pVM));
}


/*****************************************************************************
CMIFLVar::SetXXX - Sets the variable to the given value. It automatically
called Release() on the old value, and clears the LValue. (NOTE: Calling Set(),
doesn't reset the LValue() since it just copied the existing value directly)
*/
void CMIFLVar::SetUndefined (void)
{
   ReleaseInt();
   m_dwType = MV_UNDEFINED;
}

void CMIFLVar::SetNULL (void)
{
   ReleaseInt();
   m_dwType = MV_NULL;
}

void CMIFLVar::SetBOOL (BOOL fValue)
{
   ReleaseInt();
   m_dwType = MV_BOOL;
   m_dwValue = (DWORD)fValue;
}

void CMIFLVar::SetChar (WCHAR cValue)
{
   ReleaseInt();
   m_dwType = MV_CHAR;
   m_dwValue = (DWORD)cValue;
}

void CMIFLVar::SetDouble (double fValue)
{
   ReleaseInt();
   m_dwType = MV_DOUBLE;
   m_fValue = fValue;
}

// dwLength can be -1 to indicate unknown
BOOL CMIFLVar::SetString (PWSTR psz, DWORD dwLength)
{
   PCMIFLVarString pString = new CMIFLVarString;
   if (!pString)
      return FALSE;
   if (!pString->Set (psz, dwLength)) {
      delete pString;
      return FALSE;
   }

   ReleaseInt();
   m_dwType = MV_STRING;
   m_pValue = pString;

   return TRUE;
}

void CMIFLVar::SetString (PCMIFLVarString pString)
{
   ReleaseInt();
   m_dwType = MV_STRING;
   m_pValue = pString;
   pString->AddRef();
}

BOOL CMIFLVar::SetListNew (void)
{
   PCMIFLVarList pList = new CMIFLVarList;
   if (!pList)
      return FALSE;

   ReleaseInt();
   m_dwType = MV_LIST;
   m_pValue = pList;

   return TRUE;
}

void CMIFLVar::SetList (PCMIFLVarList pList)
{
   ReleaseInt();
   m_dwType = MV_LIST;
   m_pValue = pList;
   pList->AddRef();
}

void CMIFLVar::SetStringTable (DWORD dwID)
{
   ReleaseInt();
   m_dwType = MV_STRINGTABLE;
   m_dwValue = dwID;
}

void CMIFLVar::SetResource (DWORD dwID)
{
   ReleaseInt();
   m_dwType = MV_RESOURCE;
   m_dwValue = dwID;
}

void CMIFLVar::SetObject (GUID *pgObject)
{
   ReleaseInt();
   m_dwType = MV_OBJECT;
   m_gValue = *pgObject;
}

void CMIFLVar::SetMeth (DWORD dwID)
{
   ReleaseInt();
   m_dwType = MV_METH;
   m_dwValue = dwID;
}

void CMIFLVar::SetObjectMeth (GUID *pgObject, DWORD dwID)
{
   ReleaseInt();
   m_dwType = MV_OBJECTMETH;
   m_gValue = *pgObject;
   m_dwValue = dwID;
}

void CMIFLVar::SetStringMeth (PCMIFLVarString ps, DWORD dwID)
{
   ReleaseInt();
   m_dwType = MV_STRINGMETH;
   m_pValue = ps;
   m_dwValue = dwID;
   ps->AddRef();
}

void CMIFLVar::SetListMeth (PCMIFLVarList pl, DWORD dwID)
{
   ReleaseInt();
   m_dwType = MV_LISTMETH;
   m_pValue = pl;
   m_dwValue = dwID;
   pl->AddRef();
}


void CMIFLVar::SetFunc (DWORD dwID)
{
   ReleaseInt();
   m_dwType = MV_FUNC;
   m_dwValue = dwID;
}

void CMIFLVar::Set (PCMIFLVar pFrom)
{
   ReleaseInt();
   memcpy (this, pFrom, sizeof(CMIFLVar));
   switch (m_dwType) {
   case MV_STRING:
   case MV_STRINGMETH:
      ((PCMIFLVarString)m_pValue)->AddRef();
      break;
   case MV_LIST:
   case MV_LISTMETH:
      ((PCMIFLVarList)m_pValue)->AddRef();
      break;
   }
}





/*****************************************************************************
CMIFLVar::ToXXX - Converts from one type of variable to another.
*/
void CMIFLVar::ToBOOL (PCMIFLVM pVM)
{
   SetBOOL (GetBOOL(pVM));
}

void CMIFLVar::ToChar (PCMIFLVM pVM)
{
   SetChar (GetChar(pVM));
}

void CMIFLVar::ToDouble (PCMIFLVM pVM)
{
   SetDouble (GetDouble(pVM));
}

void CMIFLVar::ToString (PCMIFLVM pVM)
{
   // DOCUMENT: Conversion of different types to a string

   switch (m_dwType) {
   case MV_UNDEFINED:
      SetString (L"", (DWORD)-1);
      break;

   case MV_NULL:
      SetString (L"null", (DWORD)-1);
      break;

   case MV_BOOL:
      SetString (m_dwValue ? L"true" : L"false", (DWORD)-1);
      break;

   case MV_CHAR:
      {
         WCHAR szTemp[2];
         szTemp[0] = (WCHAR)m_dwValue;
         szTemp[1] = 0;
         SetString (szTemp, 1);
      }
      break;

   case MV_DOUBLE:
      {
         WCHAR szTemp[64];
         swprintf (szTemp, L"%.12g", m_fValue);
         SetString (szTemp, (DWORD)-1);
      }
      break;

   case MV_STRING:       // memory points to a PCMIFLVarString
      break;   // do nothing

   case MV_LIST:       // memory points to PCMIFLVarList
      {
         CMem mem;
         PCMIFLVarList pl = (PCMIFLVarList) m_pValue;

         pl->ToString (&mem, pVM);
         if (!mem.Required (mem.m_dwCurPosn + sizeof(WCHAR))) {
            SetString (L"error", (DWORD)-1);
            return;
         }

         ((PWSTR)((PBYTE)mem.p + mem.m_dwCurPosn))[0] = 0;
         SetString ((PWSTR)mem.p, (DWORD)mem.m_dwCurPosn / sizeof(WCHAR));
      }
      break;   

   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         if (!pVM) {
            SetString (L"error", (DWORD)-1);
            break;
            }

         PCMIFLString ps = pVM->m_pCompiled->m_pLib->StringGet (m_dwValue);
         DWORD dwLength = (DWORD)-1;
         PWSTR psz = ps ? ps->Get(pVM->m_LangID, &dwLength) : NULL;   // always get 1st one
         SetString (psz ? psz : L"error", psz ? dwLength : (DWORD)-1);
      }
      break;

   case MV_RESOURCE:       // resource in table. dwValue is the ID
      {
         if (!pVM) {
            SetString (L"error", (DWORD)-1);
            break;
         }

         // BUGFIX - Have resoruces precalculated as strings so faster to access
         PCMIFLResource pr = pVM->m_pCompiled->m_pLib->ResourceGet (m_dwValue);
         DWORD dwLength = (DWORD)-1;
         PWSTR psz = pr ? pr->GetAsString (pVM->m_LangID, &dwLength) : NULL;

         if (psz)
            SetString (psz, dwLength);
         else
            SetString (L"error", (DWORD)-1);
      }
      break;

   case MV_STRINGMETH:
   case MV_LISTMETH:
      {
         SetString ((m_dwType == MV_STRINGMETH) ? L"string." : L"list.", (DWORD)-1);

         PCMIFLVarString ps = GetString(pVM);

         PWSTR psz = NULL;

         if (m_dwValue >= VM_CUSTOMIDRANGE) {
            DWORD dwIndex = pVM->m_hUnIdentifiersCustomMethod.FindIndex (m_dwValue);
            psz = pVM->m_hIdentifiersCustomMethod.GetString (dwIndex);
         }
         else {
            PMIFLIDENT pmi = (PMIFLIDENT) pVM->m_pCompiled->m_hUnIdentifiers.Find (m_dwValue);

            PCMIFLMeth pMeth;
            if (pmi && (pmi->dwType == MIFLI_METHDEF)) {
               pMeth = (PCMIFLMeth) pmi->pEntity;
               psz = (PWSTR)pMeth->m_memName.p;
            }
         }

         ps->Append (psz ? psz : L"error", (DWORD)-1);
         ps->Release();
      }
      break;

   case MV_OBJECT:       // object GUID stored
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
   case MV_METH:       // public/private method, no object. dwValue is the ID
      {
         if (!pVM) {
            SetString (L"error", (DWORD)-1);
            break;
         }

         BOOL fObject = ((m_dwType == MV_OBJECT) || (m_dwType == MV_OBJECTMETH));
         BOOL fMeth = ((m_dwType == MV_METH) || (m_dwType == MV_OBJECTMETH));
         DWORD dwValue = m_dwValue; // because may to ToString() which will wipe this

         // DOCUMENT: If ask for string of object, will first try calling object.name()
         // to see if supports a name

         // get the object?
         if (fObject) {
            // see if has a name?
            CMIFLVarLValue vName;

            pVM->MethodCallVMTOK (&m_gValue, VMTOK_NAME, NULL, 0, 0, &vName);
            DWORD dwType = vName.m_Var.TypeGet();
            if ((dwType != MV_UNDEFINED) && (dwType!= MV_OBJECT) && (dwType != MV_OBJECTMETH) && (dwType != MV_METH)){
               Set (&vName.m_Var);
               Fracture(); // fracture it so don't get multiple copies
            }
            else {
               // see if can come up with name
               PWSTR psz = L"Unknown object";
               PCMIFLVMObject po = pVM->ObjectFind (&m_gValue);
               //if (po && po->m_pObject)
               //   psz = (PWSTR) po->m_pObject->m_memName.p;
               //else
               if (po) {
                  PCMIFLVMLayer pl = po->LayerGet(po->LayerNum()-1);
                  if (pl)
                     psz = (PWSTR)pl->m_pObject->m_memName.p;
               }
               SetString (psz, (DWORD)-1);
            }
         } // fObject

         if (fMeth) {
            PCMIFLVarString ps = fObject ? GetString(pVM) : NULL;
            if (ps)
               ps->Append (L".", (DWORD)-1);   // separate method

            PWSTR psz = NULL;
            if (dwValue >= VM_CUSTOMIDRANGE) {
               DWORD dwIndex = pVM->m_hUnIdentifiersCustomMethod.FindIndex (dwValue);
               psz = pVM->m_hIdentifiersCustomMethod.GetString (dwIndex);
            }
            else {
               PMIFLIDENT pmi = (PMIFLIDENT) pVM->m_pCompiled->m_hUnIdentifiers.Find (dwValue);

               PCMIFLMeth pMeth;
               PCMIFLFunc pFunc;
               if (pmi) switch (pmi->dwType) {
               case MIFLI_METHDEF:
                  pMeth = (PCMIFLMeth) pmi->pEntity;
                  psz = (PWSTR)pMeth->m_memName.p;
                  break;
               case MIFLI_METHPRIV:
                  pFunc = (PCMIFLFunc) pmi->pEntity;
                  psz = (PWSTR)pFunc->m_Meth.m_memName.p;
                  break;
               }
            }
            if (!psz)
               psz = L"error";

            if (ps)
               ps->Append (psz, (DWORD)-1);
            else
               SetString (psz, (DWORD)-1);

            if (ps)
               ps->Release();
         } // fMeth
      }
      break;


   case MV_FUNC:       // function call. dwValue is the ID
      {
         if (!pVM) {
            SetString (L"error", (DWORD)-1);
            break;
         }

         PCMIFLFunc pFunc = pVM->m_pCompiled->m_pLib->FuncGet (m_dwValue);
         if (!pFunc) {
            SetString (L"error", (DWORD)-1);
            break;
         }

         SetString ((PWSTR)pFunc->m_Meth.m_memName.p, (DWORD)-1);
      }
      break;

   default:
      SetString (L"error", (DWORD)-1);
      break;
   }
}

void CMIFLVar::ToMeth (PCMIFLVM pVM)
{
   // DOCUMENT: Conversion of different types to a string

   switch (m_dwType) {
   default:
      SetUndefined();
      break;

   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         PCMIFLVarString ps = GetString(pVM);

         // see if is main method
         PMIFLIDENT pmi = (PMIFLIDENT) pVM->m_pCompiled->m_hIdentifiers.Find (ps->Get(), TRUE);
         // BUGFIX - since changed the way custom methods stored
         if (pmi && ((pmi->dwType != MIFLI_METHDEF) && (pmi->dwType != MIFLI_METHPRIV)))
            pmi = NULL;
         if (!pmi)
            pmi = (PMIFLIDENT) pVM->m_hIdentifiersCustomMethod.Find (ps->Get(), TRUE);
         ps->Release();

         // DOCUMENT: ToMethod() will only work for public methods or run-time added ones...

         if (pmi)
            SetMeth (pmi->dwID);
         else
            SetUndefined ();
      }
      break;

   case MV_METH:       // public/private method, no object. dwValue is the ID
      break;   // do nothing

   case MV_STRINGMETH:
   case MV_LISTMETH:
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
      // m_dwValue already contains, so use that
      SetMeth (m_dwValue);
      break;
   }
}

void CMIFLVar::ToFunction (PCMIFLVM pVM)
{
   // DOCUMENT: Conversion of different types to a function

   switch (m_dwType) {
   default:
      SetUndefined();
      break;

   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         PCMIFLVarString ps = GetString(pVM);

         // see if is main method
         PMIFLIDENT pmi = (PMIFLIDENT) pVM->m_pCompiled->m_hIdentifiers.Find (ps->Get(), TRUE);
         ps->Release();

         if (pmi && (pmi->dwType == MIFLI_FUNC))
            SetFunc (pmi->dwID);
         else
            SetUndefined ();
      }
      break;

   case MV_FUNC:
      break;   // do nothing
   }
}


/*****************************************************************************
CMIFLVar::GetXXX - Get the value as a certain type of object

DOCUMENT: Converting from one type to another.
*/
BOOL CMIFLVar::GetBOOL (PCMIFLVM pVM)
{
   switch (m_dwType) {
   case MV_BOOL:
      return (BOOL)m_dwValue;

   case MV_UNDEFINED:
   case MV_NULL:
      return FALSE;

   case MV_CHAR:
      return (m_dwValue ? TRUE : FALSE);

   case MV_DOUBLE:
      return (m_fValue ? TRUE : FALSE);

   case MV_STRING:       // memory points to a PCMIFLVarString
      {
         PCMIFLVarString ps = (PCMIFLVarString)m_pValue;
         PWSTR psz = ps->Get ();
         return (psz && psz[0]) ? TRUE : FALSE;
      }

   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         if (!pVM)
            return TRUE;

         PCMIFLString ps = pVM->m_pCompiled->m_pLib->StringGet (m_dwValue);
         PWSTR psz = ps ? ps->Get(pVM->m_LangID) : NULL;   // always get 1st one
         return (psz && psz[0]) ? TRUE : FALSE;
      }
      break;

   case MV_LIST:       // memory points to PCMIFLVarList
   case MV_RESOURCE:       // resource in table. dwValue is the ID
   case MV_OBJECT:       // object GUID stored
   case MV_METH:       // public/private method, no object. dwValue is the ID
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
   case MV_FUNC:       // function call. dwValue is the ID
   case MV_STRINGMETH:
   case MV_LISTMETH:
   default:
      return TRUE;
   }
}

WCHAR CMIFLVar::GetChar (PCMIFLVM pVM)
{
   switch (m_dwType) {
   case MV_CHAR:
      return (WCHAR)m_dwValue;

   case MV_UNDEFINED:
   case MV_NULL:
      return 0;

   case MV_BOOL:
      return m_dwValue ? L't' : L'f';

   case MV_DOUBLE:
      return (WCHAR)m_fValue;

   case MV_STRING:       // memory points to a PCMIFLVarString
      {
         PCMIFLVarString ps = (PCMIFLVarString)m_pValue;
         PWSTR psz = ps->Get ();
         return psz ? psz[0] : 0;
      }

   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         if (!pVM)
            return 0;

         PCMIFLString ps = pVM->m_pCompiled->m_pLib->StringGet (m_dwValue);
         PWSTR psz = ps ? ps->Get(pVM->m_LangID) : NULL;   // always get 1st one
         return (psz && psz[0]) ? psz[0] : 0;
      }
      break;

   case MV_LIST:       // memory points to PCMIFLVarList
   case MV_RESOURCE:       // resource in table. dwValue is the ID
   case MV_OBJECT:       // object GUID stored
   case MV_METH:       // public/private method, no object. dwValue is the ID
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
   case MV_FUNC:       // function call. dwValue is the ID
   case MV_STRINGMETH:
   case MV_LISTMETH:
   default:
      return 0;
   }
}


double CMIFLVar::GetDouble (PCMIFLVM pVM)
{
   switch (m_dwType) {
   case MV_DOUBLE:
      return m_fValue;

   case MV_CHAR:
      return (double)m_dwValue;

   case MV_UNDEFINED:
   case MV_NULL:
      return 0;

   case MV_BOOL:
      return (double)m_dwValue;

   case MV_STRING:       // memory points to a PCMIFLVarString
      {
         PCMIFLVarString ps = (PCMIFLVarString)m_pValue;
         PWSTR psz = ps->Get ();
         return psz ? _wtof(psz) : 0;
      }
   case MV_STRINGTABLE:       // string table. dwValue is the ID
      {
         if (!pVM)
            return 0;

         PCMIFLString ps = pVM->m_pCompiled->m_pLib->StringGet (m_dwValue);
         PWSTR psz = ps ? ps->Get(pVM->m_LangID) : NULL;   // always get 1st one
         return (psz && psz[0]) ? _wtof(psz) : 0;
      }
      break;

   case MV_LIST:       // memory points to PCMIFLVarList
   case MV_RESOURCE:       // resource in table. dwValue is the ID
   case MV_OBJECT:       // object GUID stored
   case MV_METH:       // public/private method, no object. dwValue is the ID
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
   case MV_FUNC:       // function call. dwValue is the ID
   case MV_STRINGMETH:
   case MV_LISTMETH:
   default:
      return 0;
   }
}



// NOTE: With GetString() the caller will need to call Release() on PCMIFLVarString
PCMIFLVarString CMIFLVar::GetString (PCMIFLVM pVM)
{
   switch (m_dwType) {
   case MV_STRING:
   // BUGFIX - disable: case MV_STRINGMETH:
      {
         PCMIFLVarString ps = (PCMIFLVarString) m_pValue;
         ps->AddRef();
         return ps;
      }

   default:
      {
         CMIFLVar v;
         v.Set (this);
         v.ToString (pVM);
         PCMIFLVarString ps = (PCMIFLVarString) v.m_pValue;
         ps->AddRef();
         return ps;
      }
   }
}

PCMIFLVarString CMIFLVar::GetStringNoMod (void)
{
   switch (m_dwType) {
   case MV_STRING:
   case MV_STRINGMETH:
      {
         PCMIFLVarString ps = (PCMIFLVarString) m_pValue;
         ps->AddRef();
         return ps;
      }

   default:
      return NULL;   // since no mod
   }
}

// NOTE: With GetList() the caller will need to call Release() on PCMIFLVarList
PCMIFLVarList CMIFLVar::GetList (void)
{
   if ((m_dwType == MV_LIST) || (m_dwType == MV_LISTMETH)) {
      PCMIFLVarList ps = (PCMIFLVarList) m_pValue;
      ps->AddRef();
      return ps;
   }
   else
      return NULL;   // shouldnt happen
}

DWORD CMIFLVar::GetValue (void)
{
   switch (m_dwType) {
   case MV_DOUBLE:
      return (DWORD)m_fValue;

   case MV_UNDEFINED:
   case MV_NULL:
      return 0;

   case MV_STRING:       // memory points to a PCMIFLVarString
      {
         PCMIFLVarString ps = (PCMIFLVarString)m_pValue;
         PWSTR psz = ps->Get ();
         return (DWORD) (psz ? _wtoi(psz) : 0);
      }

   case MV_OBJECT:       // object GUID stored
      return 0;   // cant get for a GUID

   case MV_CHAR:
   case MV_BOOL:
   case MV_LIST:       // memory points to PCMIFLVarList
   case MV_RESOURCE:       // resource in table. dwValue is the ID
   case MV_METH:       // public/private method, no object. dwValue is the ID
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
   case MV_FUNC:       // function call. dwValue is the ID
   case MV_STRINGTABLE:       // string table. dwValue is the ID
   case MV_STRINGMETH:
   case MV_LISTMETH:
   default:
      return m_dwValue;
   }
}


GUID CMIFLVar::GetGUID (void)
{
   switch (m_dwType) {
   case MV_DOUBLE:
   case MV_UNDEFINED:
   case MV_NULL:
   case MV_STRING:       // memory points to a PCMIFLVarString
   case MV_CHAR:
   case MV_BOOL:
   case MV_LIST:       // memory points to PCMIFLVarList
   case MV_RESOURCE:       // resource in table. dwValue is the ID
   case MV_METH:       // public/private method, no object. dwValue is the ID
   case MV_FUNC:       // function call. dwValue is the ID
   case MV_STRINGTABLE:       // string table. dwValue is the ID
   case MV_STRINGMETH:
   case MV_LISTMETH:
   default:
      return GUID_NULL;

   case MV_OBJECT:       // object GUID stored
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
      return m_gValue;
   }
}

/*****************************************************************************
CMIFLVar::TypeGet - Returns the type, as in MV_XXX
*/
DWORD CMIFLVar::TypeGet (void)
{
   return m_dwType;
}


/*****************************************************************************
CMIFLVar::TypeAsString - Returns the type as a string

DOCUMENT: TypeOf() call
*/
PWSTR CMIFLVar::TypeAsString (void)
{
   switch (m_dwType) {
   case MV_DOUBLE:
      return L"number";


   case MV_NULL:
      return L"null";

   case MV_STRING:       // memory points to a PCMIFLVarString
      return L"string";

   case MV_STRINGMETH:
      return L"string.method";

   case MV_LISTMETH:
      return L"list.method";

   case MV_STRINGTABLE:       // string table. dwValue is the ID
      return L"stringtable";

   case MV_CHAR:
      return L"character";

   case MV_BOOL:
      return L"boolean";

   case MV_LIST:       // memory points to PCMIFLVarList
      return L"list";

   case MV_RESOURCE:       // resource in table. dwValue is the ID
      return L"resource";
   case MV_OBJECT:       // object GUID stored
      return L"object";
   case MV_METH:       // public/private method, no object. dwValue is the ID
      return L"method";
   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
      return L"object.method";

   case MV_FUNC:       // function call. dwValue is the ID
      return L"function";

   default:
   case MV_UNDEFINED:
      return L"undefined";
   }
}


/*****************************************************************************
CMIFLVar::Compare - Compares the this value with another one. It returns 0 if
the values are equal, -1 if this is < pWith, 1 if this > pWith., -2 or 2 if
the values cant be compared at all

DOCUMENT: How comparison works. Tend to convert to number

inputs
   PCMIFLVar         pWith - Compare with
   BOOL              fExact - If TRUE looking for exact match only. Matters
                     when comparing null/undef.
   PCMIFLVM          pVM - Necessary for some string comparisons
*/
int CMIFLVar::Compare (PCMIFLVar pWith, BOOL fExact, PCMIFLVM pVM)
{
   PCMIFLVar pA, pB;
   int iMult;
   if (m_dwType <= pWith->m_dwType) {
      pA = this;
      pB = pWith;
      iMult = 1;
   }
   else {
      pA = pWith;
      pB = this;
      iMult = -1;
   }

   switch (pA->m_dwType) {
   case MV_UNDEFINED:
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         // exact test then eactly with undefined
         return (pB->m_dwType == MV_UNDEFINED) ? 0 : (-2*iMult);
      }

      // else...
      switch (pB->m_dwType) {
         case MV_NULL:
            return 0;

         case MV_BOOL:
         case MV_CHAR:
         case MV_DOUBLE:
         case MV_STRING:
            return pB->GetBOOL (pVM) ? -iMult : 0;

         default:
         case MV_LIST:
         case MV_STRINGTABLE:
         case MV_RESOURCE:
         case MV_OBJECT:
         case MV_METH:
         case MV_OBJECTMETH:
         case MV_FUNC:
         case MV_STRINGMETH:
         case MV_LISTMETH:
            return -2*iMult;
         }

   case MV_NULL:
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         // exact test then eactly with undefined
         return (pB->m_dwType == MV_NULL) ? 0 : -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_BOOL:
      case MV_CHAR:
      case MV_DOUBLE:
      case MV_STRING:
         return pB->GetBOOL (pVM) ? -iMult : 0;

      default:
      case MV_LIST:
      case MV_STRINGTABLE:
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      case MV_STRINGMETH:
      case MV_LISTMETH:
         return -2*iMult;
      }

   case MV_BOOL:
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_BOOL) {
            int iRet = ((int)pA->m_dwValue - (int)pB->m_dwValue);
            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_CHAR:
      case MV_DOUBLE:
      case MV_STRING:
         {
            int iRet = ((int)pA->GetBOOL (pVM) - (int)pB->GetBOOL(pVM));
            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }

      default:
      case MV_LIST:
      case MV_STRINGTABLE:
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      case MV_STRINGMETH:
      case MV_LISTMETH:
         return -2*iMult;
      }
      break;

   case MV_CHAR:
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_CHAR) {
            int iRet = (int)pA->m_dwValue - (int)pB->m_dwValue;
            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_DOUBLE:
         {
            int iRet = (int)GetChar (pVM) - (int)GetChar(pVM);
            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         break;

      case MV_STRING:
         // BUGFIX - Compare character with string, treat like string
         {
            WCHAR sz[2];
            sz[0] = pA->GetChar(pVM);
            sz[1] = 0;

            int iRet = wcscmp (sz, ((PCMIFLVarString)pB->m_pValue)->Get());
            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         break;

      default:
      case MV_LIST:
      case MV_STRINGTABLE:
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      case MV_STRINGMETH:
      case MV_LISTMETH:
         return -2*iMult;
      }
      break;

   case MV_DOUBLE:
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_DOUBLE) {
            if (pA->m_fValue < pB->m_fValue)
               return -iMult;
            else if (pA->m_fValue > pB->m_fValue)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_STRING:
         {
            double fa = pA->GetDouble (pVM);
            double fb = pB->GetDouble (pVM);
            if (fa < fb)
               return -iMult;
            else if (fa > fb)
               return iMult;
            else
               return 0;
         }
                    
      default:
      case MV_LIST:
      case MV_STRINGTABLE:
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      case MV_STRINGMETH:
      case MV_LISTMETH:
         return -2*iMult;
      }
      break;

   case MV_STRING:       // memory points to a PCMIFLVarString
      // NOTE: String comparisons ARE CASE SENSATIVE
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_STRING) {
            int iRet = wcscmp (
               ((PCMIFLVarString)pA->m_pValue)->Get(),
               ((PCMIFLVarString)pB->m_pValue)->Get());
            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_LIST:
      case MV_STRINGTABLE:
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
         {
            PCMIFLVarString sa = pA->GetString(pVM);
            PCMIFLVarString sb = pB->GetString(pVM);

            int iRet = wcscmp(sa->Get(), sb->Get());

            sa->Release();
            sb->Release();

            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
                    
      default:
      case MV_STRINGMETH:
      case MV_LISTMETH:
         return -2*iMult;
      }
      break;

   case MV_LIST:       // memory points to PCMIFLVarList
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_LIST) {
            PCMIFLVarList sa = pA->GetList();
            PCMIFLVarList sb = pB->GetList();

            int iRet = sa->Compare (sb, fExact, pVM);

            sa->Release();
            sb->Release();
            return iRet;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_STRINGTABLE:
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      case MV_STRINGMETH:
      case MV_LISTMETH:
      default:
         return -2*iMult;
      }
      break;

   case MV_STRINGMETH:
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_STRINGMETH) {
            if (pA->m_dwValue < pB->m_dwValue)
               return -iMult;
            else if (pA->m_dwValue > pB->m_dwValue)
               return iMult;

            int iRet = wcscmp (
               ((PCMIFLVarString)pA->m_pValue)->Get(),
               ((PCMIFLVarString)pB->m_pValue)->Get());
            if (iRet < 0)
               return  -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      default:
         return -2*iMult;
      }
      break;

   case MV_LISTMETH:
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_LISTMETH) {
            if (pA->m_dwValue < pB->m_dwValue)
               return -iMult;
            else if (pA->m_dwValue > pB->m_dwValue)
               return iMult;

            PCMIFLVarList sa = pA->GetList();
            PCMIFLVarList sb = pB->GetList();

            int iRet = sa->Compare (sb, fExact, pVM);

            sa->Release();
            sb->Release();
            return iRet;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      default:
         return -2*iMult;
      }
      break;


   case MV_STRINGTABLE:       // string table. dwValue is the ID
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_STRINGTABLE) {
            if (pA->m_dwValue < pB->m_dwValue)
               return -iMult;
            else if (pA->m_dwValue > pB->m_dwValue)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_RESOURCE:
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      default:
         return -2*iMult;
      }
      break;

   case MV_RESOURCE:       // resource in table. dwValue is the ID
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_RESOURCE) {
            if (pA->m_dwValue < pB->m_dwValue)
               return -iMult;
            else if (pA->m_dwValue > pB->m_dwValue)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_OBJECT:
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      default:
         return -2*iMult;
      }
      break;

   case MV_OBJECT:       // object GUID stored
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_OBJECT) {
            int iRet = memcmp (&pA->m_gValue, &pB->m_gValue, sizeof(m_gValue));
            if (iRet < 0)
               return -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_METH:
      case MV_OBJECTMETH:
      case MV_FUNC:
      default:
         return -2*iMult;
      }
      break;

   case MV_METH:       // public/private method, no object. dwValue is the ID
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_METH) {
            if (pA->m_dwValue < pB->m_dwValue)
               return -iMult;
            else if (pA->m_dwValue > pB->m_dwValue)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_OBJECTMETH:
      case MV_FUNC:
      default:
         return -2*iMult;
      }
      break;

   case MV_OBJECTMETH:       // public/private method with associated object, dwValue is ID, plus GUID
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_OBJECTMETH) {
            if (pA->m_dwValue < pB->m_dwValue)
               return -iMult;
            else if (pA->m_dwValue > pB->m_dwValue)
               return iMult;

            // else, if match go onto object
            int iRet = memcmp (&pA->m_gValue, &pB->m_gValue, sizeof(m_gValue));
            if (iRet < 0)
               return -iMult;
            else if (iRet > 0)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      switch (pB->m_dwType) {
      case MV_FUNC:
      default:
         return -2*iMult;
      }
      break;

   case MV_FUNC:       // function call. dwValue is the ID
      if (fExact || (pA->m_dwType == pB->m_dwType)) {
         if (pB->m_dwType == MV_FUNC) {
            if (pA->m_dwValue < pB->m_dwValue)
               return -iMult;
            else if (pA->m_dwValue > pB->m_dwValue)
               return iMult;
            else
               return 0;
         }
         else
            return -2*iMult;
      }

      // else...
      //switch (pB->m_dwType) {
      //default:
         return -2*iMult;
      //}
      break;

   default:
      return -2*iMult;
   }

}




/*****************************************************************************
CMIFLVar::Constructor and destructor
*/
CMIFLVarList::CMIFLVarList (void)
{
   m_lCMIFLVar.Init (sizeof(CMIFLVar));
   m_dwRefCount = 1;
}

CMIFLVarList::~CMIFLVarList (void)
{
   // Dont call delete() directly. Call Release()
   Clear();
}

/*****************************************************************************
CMIFLVarList::Release - Reduces the reference count by one. If the reference
count goes to 0 then it's deleted.

retursn
   DWORD - Reference count after the release
*/
DWORD CMIFLVarList::Release (void)
{
   m_dwRefCount--;

   if (!m_dwRefCount) {
      delete this;
      return 0;
   }

   return m_dwRefCount;
}


/*****************************************************************************
CMIFLVarList::AddRef - Increases the reference count by 1.

returns
   DWORD - New reference count after it's increased
*/
DWORD CMIFLVarList::AddRef (void)
{
   m_dwRefCount++;
   return m_dwRefCount;
}


/*****************************************************************************
CMIFLVarList::Clone - Creates a new copy, but with a refcount of 1

NOTE: All the contents are also cloned and fractured! This means that there wont
be anything in common with the remainin lists
*/
PCMIFLVarList CMIFLVarList::Clone (void)
{
   PCMIFLVarList pNew = new CMIFLVarList;
   if (!pNew)
      return NULL;

   // basic copy over
   pNew->m_lCMIFLVar.Init (sizeof(CMIFLVar), m_lCMIFLVar.Get(0), m_lCMIFLVar.Num());

   PCMIFLVar pv = (PCMIFLVar) pNew->m_lCMIFLVar.Get(0);
   DWORD i;
   for (i = 0; i < pNew->m_lCMIFLVar.Num(); i++, pv++)
      pv->Fracture (FALSE);  // so get unique copy of all contents

   return pNew;
}





/*****************************************************************************
CMIFLVarList::Clear - Clears out the list, releasing all the pointers it calls.
*/
void CMIFLVarList::Clear (void)
{
   DWORD i;
   PCMIFLVar ppv = (PCMIFLVar) m_lCMIFLVar.Get(0);
   for (i = 0; i < m_lCMIFLVar.Num(); i++)
      ppv[i].SetUndefined();  // to release
   m_lCMIFLVar.Clear();
}


/*****************************************************************************
CMIFLVarList::VerifyNotRecurse - Given a PCMIFLVar that might be added/set to
the list, this verifies that the variable doens't end up pointing back to
the list itself. If a list points to itself won't ever release.

DOCUMENT: Cant add sub-lists that point to the master list

inputs
   PCMIFLVar         pTest - To test
returns
   BOOL - TRUE if ok, FALSE if would point to itseld
*/
BOOL CMIFLVarList::VerifyNotRecurse (PCMIFLVar pTest)
{
   DWORD dwType = pTest->TypeGet();
   if ((dwType != MV_LIST) && (dwType != MV_LISTMETH))
      return TRUE;   // no problem

   PCMIFLVarList pList = pTest->GetList ();
   if (pList == this) {
      // adds itself directly
      pList->Release();
      return FALSE;
   }

   // recurse
   DWORD i;
   PCMIFLVar ppv = (PCMIFLVar) pList->m_lCMIFLVar.Get(0);
   for (i = 0; i < pList->m_lCMIFLVar.Num(); i++) {
      DWORD dwType = ppv[i].TypeGet();
      if ((dwType != MV_LIST) && (dwType != MV_LISTMETH))
         continue;

      // else, recurse
      if (!VerifyNotRecurse (&ppv[i])) {
         // adds itself eventually
         pList->Release();
         return FALSE;
      }
   }

   // else, doesnt seem to add self
   pList->Release();
   return TRUE;
}


/*****************************************************************************
CMIFLVarList::Add - Adds the list onto the end of this list. NOTE: pAdd (or
its contents) are addreffed in the process.

inputs
   PCMIFLVar         pAdd - Variable to add
   BOOL              fListAsSublist - If pAdd is a list, and this is TRUE, then
                     it will be added as a sub-list to the end. If pAdd is a list
                     and this is FALSE, then the individual items of pAdd will be
                     appended
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarList::Add (PCMIFLVar pAdd, BOOL fListAsSublist)
{
   // verify that wont cause recursion problems
   if (!VerifyNotRecurse (pAdd))
      return FALSE;

   if ((pAdd->TypeGet() == MV_LIST) && !fListAsSublist) {
      // break out and add subitems
      DWORD i;
      PCMIFLVarList pList = pAdd->GetList();
      PCMIFLVar ppv = (PCMIFLVar) pList->m_lCMIFLVar.Get(0);
      m_lCMIFLVar.Required (m_lCMIFLVar.Num() + pList->m_lCMIFLVar.Num());
      for (i = 0; i < pList->m_lCMIFLVar.Num(); i++) {
         ppv[i].AddRef ();
         m_lCMIFLVar.Add (&ppv[i]);
      }
      pList->Release();
      return TRUE;
   }

   // else, add just one item
   pAdd->AddRef();
   m_lCMIFLVar.Add (pAdd);

   return TRUE;
}


/*****************************************************************************
CMIFLVarList::Insert - Adds the list before the given index

inputs
   PCMIFLVar         pAdd - Variable to add
   BOOL              fListAsSublist - If pAdd is a list, and this is TRUE, then
                     it will be added as a sub-list to the end. If pAdd is a list
                     and this is FALSE, then the individual items of pAdd will be
                     appended
   DWORD             dwBefore - What index to insert before.
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarList::Insert (PCMIFLVar pAdd, BOOL fListAsSublist, DWORD dwBefore)
{
   // if insering before the end, then just add
   if (dwBefore >= Num())
      return Add (pAdd, fListAsSublist);

   // verify that wont cause recursion problems
   if (!VerifyNotRecurse (pAdd))
      return FALSE;

   if ((pAdd->TypeGet() == MV_LIST) && !fListAsSublist) {
      // break out and add subitems
      DWORD i;
      PCMIFLVarList pList = pAdd->GetList();
      PCMIFLVar ppv = (PCMIFLVar) pList->m_lCMIFLVar.Get(0);
      m_lCMIFLVar.Required (m_lCMIFLVar.Num() + pList->m_lCMIFLVar.Num());
      for (i = 0; i < pList->m_lCMIFLVar.Num(); i++) {
         ppv[i].AddRef ();
         m_lCMIFLVar.Insert (dwBefore, &ppv[i]);
         dwBefore++; // so keep moving up
      }
      pList->Release();
      return TRUE;
   }

   // else, add just one item
   pAdd->AddRef();
   m_lCMIFLVar.Insert (dwBefore, pAdd);

   return TRUE;
}


/*****************************************************************************
CMIFLVarList::Remove - Removes the given index from the list.

inputs
   DWORD          dwStart - From 0..Num()-1
   DWORD          dwEnd - End index, exclusive
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarList::Remove (DWORD dwStart, DWORD dwEnd)
{
   dwStart = min(dwStart, m_lCMIFLVar.Num());
   dwEnd = max(dwEnd, dwStart);
   dwEnd = min(dwEnd, m_lCMIFLVar.Num());

   PCMIFLVar pv = (PCMIFLVar)m_lCMIFLVar.Get(0);

   DWORD i;
   for (i = dwStart; i < dwEnd; i++)
      pv[i].SetUndefined();

   for (i = dwStart; i < dwEnd; i++)
      m_lCMIFLVar.Remove (dwStart); // just keep removing the same one

   return TRUE;
}



/*****************************************************************************
CMIFLVarList::Sublist - Creates a sublist from the given element range

inputs
   DWORD          dwStart - From 0..Num()-1
   DWORD          dwEnd - End index, exclusive
returns
   BOOL - TRUE if success
*/
PCMIFLVarList CMIFLVarList::Sublist (DWORD dwStart, DWORD dwEnd)
{
   dwStart = min(dwStart, m_lCMIFLVar.Num());
   dwEnd = max(dwEnd, dwStart);
   dwEnd = min(dwEnd, m_lCMIFLVar.Num());

   PCMIFLVar pv = (PCMIFLVar)m_lCMIFLVar.Get(0);

   PCMIFLVarList pNew = new CMIFLVarList;
   if (!pNew)
      return NULL;
   pNew->m_lCMIFLVar.Init (sizeof(CMIFLVar), pv + dwStart, dwEnd - dwStart);

   // addref
   pv = (PCMIFLVar)pNew->m_lCMIFLVar.Get(0);
   DWORD i;
   for (i = 0; i < pNew->m_lCMIFLVar.Num(); i++, pv++)
      pv->AddRef ();

   return pNew;
}


/*****************************************************************************
CMIFLVarList::Reverse - Reverses the order of the list
*/
void CMIFLVarList::Reverse (void)
{
   DWORD dwNum = m_lCMIFLVar.Num();
   PCMIFLVar pv = (PCMIFLVar)m_lCMIFLVar.Get(0);

   DWORD i;
   for (i = 0; i < dwNum/2; i++) {
      BYTE abTemp[sizeof(CMIFLVar)];

      memcpy (abTemp, pv + i, sizeof (CMIFLVar));
      memcpy (pv+i, pv + (dwNum - 1 - i), sizeof(CMIFLVar));
      memcpy (pv + (dwNum-1-i), abTemp, sizeof(CMIFLVar));
   }
}




/*****************************************************************************
CMIFLVarList::Randomize - Randomizes the list

inputs
   PCMIFLVM             pVM - VM. Neeed for the random function
*/
void CMIFLVarList::Randomize (PCMIFLVM pVM)
{
   DWORD dwNum = m_lCMIFLVar.Num();
   PCMIFLVar pv = (PCMIFLVar)m_lCMIFLVar.Get(0);

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      BYTE abTemp[sizeof(CMIFLVar)];

      DWORD iWith = (DWORD)pVM->Rand() % dwNum;
      if (iWith == i)
         continue;   // trivial

      memcpy (abTemp, pv + i, sizeof (CMIFLVar));
      memcpy (pv+i, pv + iWith, sizeof(CMIFLVar));
      memcpy (pv + iWith, abTemp, sizeof(CMIFLVar));
   }
}


/*****************************************************************************
CMIFLVarList::Get - Returns a PCMIFLVar from the list, or NULL if beyond
the edge. NOTE: AddRef has NOT been done to the var

inputs
   DWORD          dwIndex - Index
returns
   PCMIFLVar - Variable that is NOT addrefed extra times, or NULL if beyond
*/
PCMIFLVar CMIFLVarList::Get (DWORD dwIndex)
{
   if (dwIndex >= m_lCMIFLVar.Num())
      return NULL;

   PCMIFLVar pv = (PCMIFLVar)m_lCMIFLVar.Get(dwIndex);
   return pv;
}


/*****************************************************************************
CMIFLVarList::Set - Sets an item in a list. If this list isn't long enough it's
expanded. Addref is USED with the new var

inputs
   DWORD          dwIndex - Index
   PCMIFLVar      pVar - To Set
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarList::Set (DWORD dwIndex, PCMIFLVar pVar)
{
   // cant set to self
   if (!VerifyNotRecurse (pVar))
      return FALSE;

   // epand list
   CMIFLVar vAdd;
   while (dwIndex >= m_lCMIFLVar.Num())
      if (m_lCMIFLVar.Add (&vAdd) == -1)
         return FALSE;  // out of memory

   PCMIFLVar pv = Get (dwIndex);
   if (!pv)
      return FALSE;  // shouldnt hapen
   pv->Set (pVar);
   return TRUE;
}



/*****************************************************************************
CMIFLVarList::MMLTo - Writes the string to a MMLNode.

inputs
   PCMIFLVM          pVM - VM, may need to write out some values
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length
   PCHashPVoid       phList - Hash of list pointer to ID. 0-lenght
returns
   PCMMLNode2         pNode - New node
*/
static PWSTR gpszMIFLVarList = L"MIFLVarList";
PCMMLNode2 CMIFLVarList::MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLVarList);

   // go through all the elements
   DWORD i;
   PCMIFLVar pv = (PCMIFLVar)m_lCMIFLVar.Get(0);
   for (i = 0; i < m_lCMIFLVar.Num(); i++, pv++) {
      PCMMLNode2 pSub = pv->MMLTo (pVM, phString, phList);
      if (!pSub) {
         delete pNode;
         return NULL;
      }
      pNode->ContentAdd (pSub);  // already has name
   } // i

   return pNode;
}


/*****************************************************************************
CMIFLVarList::MMLFrom - Reads MML into string

inputs
   PCMMLNode2         pNode - NOde to read from
   PCMIFLVM          pVM - VM
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashDWORD       phList - Hash of list pointer to ID. Each element is a pointer
                     to a PCMIFLVarString
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
returns
   BOOL - TRUE if success
*/
BOOL CMIFLVarList::MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phList,
                            PCHashGUID phObjectRemap)
{
   // clear the list
   Clear();

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   CMIFLVar var;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, gpszMIFLVar))
         continue;

      // else, variable
      if (!var.MMLFrom (pSub, pVM, phString, phList, phObjectRemap))
         return FALSE;

      // ad dit
      var.AddRef();
      m_lCMIFLVar.Add (&var);
   } // i

   return TRUE;
}


/*****************************************************************************
CMIFLVarList::Num - Returns the number of items in the list
*/
DWORD CMIFLVarList::Num (void)
{
   return m_lCMIFLVar.Num();
}



/*****************************************************************************
CMIFLVarList::Compare - Compares two lists.

inputs
   PCMIFLVarList        pWith - Compare with
   BOOL                 fExact - If TRUE want exact comparison
   PCMIFLVM             pVM - Useful for string comparisons
returns
   int - 0 if the same. -1 if this < pWith, 1 if this > pWith. Or +/- 2 if
      not the same, but cant tell if +/-
*/
int CMIFLVarList::Compare (PCMIFLVarList pWith, BOOL fExact, PCMIFLVM pVM)
{
   // get the elements
   DWORD dwNum = Num();
   DWORD dwNumWith = pWith->Num();
   DWORD dwMin = min(dwNum, dwNumWith);
   DWORD i;
   CMIFLVar vUndef;
   int iRet;

   // BUGFIX - Was using dwMax, but caused a crash if wrong number of elems
   for (i = 0; i < dwMin; i++) {
      PCMIFLVar pa = Get(i);
      PCMIFLVar pb = pWith->Get(i);

      iRet = pa->Compare (pb, fExact, pVM);
      if (iRet)
         return iRet;
   } // i

   // if get here chance that all matched, but one is longer than the other
   if (dwNum < dwNumWith)
      return -1;
   else if (dwNum > dwNumWith)
      return 1;

   // else, same
   return 0;
}



/*****************************************************************************
CMIFLVarList::ToString - Converts the list to a string. It appends starting
at m_dwCurPosn, and increments m_dwCurPosn. NOTE: It does NOT null-terminate

inputs
   PCMem          pMem - To cat to
   PCMIFLVM       pVM - compiled, used for resource
   BOOL           fPrependComma - If TRUE then prepends a comma before adding
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CMIFLVarList::ToString (PCMem pMem, PCMIFLVM pVM, BOOL fPrependComma)
{
   DWORD i;
   PCMIFLVar ppv = (PCMIFLVar) m_lCMIFLVar.Get(0);
   for (i = 0; i < m_lCMIFLVar.Num(); i++) {
      // stick in a comma?
      if (fPrependComma) {
         if (!pMem->Required (pMem->m_dwCurPosn + 2*sizeof(WCHAR)))
            return FALSE;
         PWSTR psz = (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn);
         psz[0] = L',';
         psz[1] = L' ';
         pMem->m_dwCurPosn += 2*sizeof(WCHAR);
      }
      else
         fPrependComma = TRUE;   // so will do the next one

      PCMIFLVarList pSub = NULL;
      if (ppv[i].TypeGet() == MV_LIST)
         pSub = ppv[i].GetList();
      if (pSub) {
         // add brackets...
         if (!pMem->Required (pMem->m_dwCurPosn + sizeof(WCHAR)))
            return FALSE;
         PWSTR psz = (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn);
         psz[0] = L'[';
         pMem->m_dwCurPosn += sizeof(WCHAR);

         // call into self
         pSub->ToString (pMem, pVM, FALSE);
         pSub->Release();

         // add finishing brackets
         if (!pMem->Required (pMem->m_dwCurPosn + sizeof(WCHAR)))
            return FALSE;
         psz = (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn);
         psz[0] = L']';
         pMem->m_dwCurPosn += sizeof(WCHAR);

         continue;
      } // if sub-list


      // else, convert to string
      PCMIFLVarString ps = ppv[i].GetString (pVM);
      if (!ps)
         continue;   // shouldnt happen

      PWSTR pszFrom = ps->Get();
      DWORD dwLen = (DWORD)wcslen(pszFrom)*sizeof(WCHAR);
      if (!pMem->Required (pMem->m_dwCurPosn + dwLen)) {
         ps->Release();
         return FALSE;
      }
      PWSTR psz = (PWSTR)((PBYTE)pMem->p + pMem->m_dwCurPosn);
      memcpy (psz, pszFrom, dwLen);
      pMem->m_dwCurPosn += dwLen;

      // release
      ps->Release();
   } // i

   // done
   return TRUE;
}

/*****************************************************************************
CMIFLVarStringMMLTo - This writes all the CMIFLVarString referenced by hString
into the MMLNode.

inputs
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length. This
                        has been added to by calls for CMIFLVarString::MMLTo
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVarStringList = L"VarStringList";

PCMMLNode2 CMIFLVarStringMMLTo (PCHashPVOID phString)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVarStringList);

   DWORD i;
   for (i = 0; i < phString->Num(); i++) {
      PCMIFLVarString ps = (PCMIFLVarString)(phString->GetPVOID(i));
      PCMMLNode2 pSub = ps->MMLTo();
      if (!pSub) {
         delete pNode;
         return FALSE;
      }
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*****************************************************************************
CMIFLVarStringMMLFrom - This reads the output from CMIFLVarStringMMLTo()
and creates all the strings.

inputs
   PCMMLNode2      pNode - From CMIFLVarStringMMLTo()
returns
   PCHashDWORD - Hash of PCMIFLVarString elements, all initially with reference
                  count 1. This is then passed into CMIFLVar::MMLFrom, etc.
                  They will addref as needed.
                  NOTE: When all those functions are done, LOOP THROUGH ALL
                  the elements and release() once just to make sure the
                  reference count is right. This has must be freed by the caller.
*/
PCHashDWORD CMIFLVarStringMMLFrom (PCMMLNode2 pNode)
{
   // create the list
   PCHashDWORD ph = new CHashDWORD;
   if (!ph)
      return NULL;
   ph->Init (sizeof(PCMIFLVarString), pNode->ContentNum()*2);

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwCount = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, gpszMIFLVarString))
         continue;

      // get this
      PCMIFLVarString ps = new CMIFLVarString;
      if (!ps) {
         delete ph;
         return NULL;
      }
      if (!ps->MMLFrom (pSub)) {
         delete ph;
         return NULL;
      }
      ph->Add (dwCount, &ps);
      dwCount++;
   } // i

   return ph;
}


/*****************************************************************************
CMIFLVarListMMLTo - This writes all the CMIFLVarList referenced by hList
into the MMLNode.

NOTE: Make sure to call CMIFLVarListMMLTo BEFORE CMIFLVarStringMMLTo

inputs
   PCHashPVOID       phList - Hash of List pointer to ID. 0-length. This
                        has been added to by calls for CMIFLVarList::MMLTo
   PCMIFLVM          pVM - VM to use
   PCHashPVOID       phString - Hash of string pointer to ID. 0-length. This
                        may be added to.
returns
   PCMMLNode2 - Node
*/
static PWSTR gpszVarListList = L"VarListList";

PCMMLNode2 CMIFLVarListMMLTo (PCHashPVOID phList, PCMIFLVM pVM, PCHashPVOID phString)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszVarListList);

   DWORD i;
   for (i = 0; i < phList->Num(); i++) {
      // NOTE: As go through and to list MLLTo, new lists might be added, so
      // phList->Num() will grow in size

      PCMIFLVarList ps = (PCMIFLVarList)(phList->GetPVOID(i));
      PCMMLNode2 pSub = ps->MMLTo(pVM, phString, phList);
      if (!pSub) {
         delete pNode;
         return FALSE;
      }
      pNode->ContentAdd (pSub);
   } // i

   return pNode;
}


/*****************************************************************************
CMIFLVarListMMLFrom - This reads the output from CMIFLVarListMMLTo()
and creates all the Lists.

NOTE: Make sure to call CMIFLVarListMMLFrom AFTER CMIFLVarStringMMLFrom

inputs
   PCMMLNode2      pNode - From CMIFLVarListMMLTo()
   PCMIFLVM          pVM - VM
   PCHashDWORD       phString - Hash of string pointer to ID. Each element is
                     a PCMIFLVarString.
   PCHashGUID        phObjectRemap - Hash is of original object ID. Contents are GUID of new ID
returns
   PCHashDWORD - Hash of PCMIFLVarList elements, all initially with reference
                  count 1. This is then passed into CMIFLVar::MMLFrom, etc.
                  They will addref as needed.
                  NOTE: When all those functions are done, LOOP THROUGH ALL
                  the elements and release() once just to make sure the
                  reference count is right. This has must be freed by the caller.
*/
PCHashDWORD CMIFLVarListMMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString,
                                 PCHashGUID phObjectRemap)
{
   // create the list
   PCHashDWORD ph = new CHashDWORD;
   if (!ph)
      return NULL;
   ph->Init (sizeof(PCMIFLVarList), pNode->ContentNum()*2);

   // first pass, create a blank list for every element in the node, because
   // lists will reference others further down the line
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      PCMIFLVarList ps = new CMIFLVarList;
      if (!ps)
         goto freeall;
      ph->Add (i, &ps); // adding blank one
   }

   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD dwCount = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, gpszMIFLVarList))
         continue;

      // get this
      PCMIFLVarList ps = *((PCMIFLVarList*)ph->Get(dwCount));
      if (!ps->MMLFrom (pSub, pVM, phString, ph, phObjectRemap))
         goto freeall;
      dwCount++;
   } // i

   return ph;

freeall:
   // if there's an error
   for (i = 0; i < ph->Num(); i++) {
      PCMIFLVarList ps =  *((PCMIFLVarList*)ph->Get(dwCount));
      if (ps)
         ps->Release(); // need to do release so if sublists keep right reference count
   }
   delete ph;
   return NULL;
}



/*****************************************************************************
CodeGetSetMMLTo - Writes the information about where a piece of code came from
to MML.

inputs
   PCMIFLCode        pCode - Code. If NULL writes out empty entry
returns
   PCMMLNode2 - node
*/
static PWSTR gpszCodeGetSet = L"CodeGetSet";
static PWSTR gpszCodeName = L"CodeName";
static PWSTR gpszCodeFrom = L"CodeFrom";
static PWSTR gpszObject = L"Object";

PCMMLNode2 CodeGetSetMMLTo (PCMIFLCode pCode)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszCodeGetSet);

   if (pCode) {
      MMLValueSet (pNode, gpszCodeFrom, (int) pCode->m_dwCodeFrom);
      PWSTR psz;
      if (pCode->m_pszCodeName && pCode->m_pszCodeName[0])
         MMLValueSet (pNode, gpszCodeName, pCode->m_pszCodeName);
      psz = pCode->m_pObjectLayer ? (PWSTR) pCode->m_pObjectLayer->m_memName.p : NULL;
      if (psz)
         MMLValueSet (pNode, gpszObject, psz);
   }

   return pNode;
}


/*****************************************************************************
CodeGetGetMMLFrom - Reads the information about where a piece of code came from
to MML.

inputs
   PCMMLNode2         pNode - To read from
   PCMIFLVM          pVM - To search through
   BOOL              fFunc - If TRUE then returns a PCMIFLFunc, FALSE then PCMIFLCode
returns
   PCMIFLCode - Code to use, or NULL if cant find
*/
PVOID CodeGetSetMMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, BOOL fFunc)
{
   DWORD dwFrom = (int)MMLValueGetInt (pNode, gpszCodeFrom, 6);
   PWSTR pszName = MMLValueGet (pNode, gpszCodeName);
   PWSTR pszObject = MMLValueGet (pNode, gpszObject);
   PMIFLIDENT pmi = pszObject ? (PMIFLIDENT)pVM->m_pCompiled->m_hIdentifiers.Find (pszObject, TRUE) : NULL;
   PCMIFLObject po = (pmi && (pmi->dwType == MIFLI_OBJECT)) ? (PCMIFLObject)pmi->pEntity : NULL;
   pmi = pszName ? (PMIFLIDENT)pVM->m_pCompiled->m_hIdentifiers.Find (pszName, TRUE) : NULL;
   if (!pmi)
      return NULL;   // must have this

   switch (dwFrom) {
   case 0:  // function
      {
         if (pmi->dwType != MIFLI_FUNC)
            return NULL;

         PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;

         return fFunc ? (PVOID)pFunc : (PVOID)&pFunc->m_Code;
      }

#if 0 // DIsabled because it never gets called and not sure it's right. Suspect that
      // need pmi from m_hGlobals instead, but not sure
   case 1:  // global var get
   case 2:  // global var set
      {
         if (pmi->dwType != MIFLI_GLOBAL) // note: ignoring tests for objects, since cant default to override get/set
            return NULL;

         PCMIFLProp pProp = (PCMIFLProp)pmi->pEntity;
         if (fFunc)
            return NULL;   // cant do this
         return (dwFrom == 1) ? pProp->m_pCodeGet : pProp->m_pCodeSet;
      }
      break;
#endif // 0

   case 3:  // method
      {
         if (!po || ((pmi->dwType != MIFLI_METHDEF) && (pmi->dwType != MIFLI_METHPRIV)))
            return NULL;

         pmi = (PMIFLIDENT)po->m_hMethJustThis.Find (pmi->dwID);
         if (!pmi)
            return NULL;

         PCMIFLFunc pFunc = (PCMIFLFunc)pmi->pEntity;
         return fFunc ? (PVOID)pFunc : (PVOID)&pFunc->m_Code;
      }
      break;

#if 0 // disabled this because don't think it will ever get called
      // NOTE: havent tested it
   case 4:  // method var get
   case 5:  // method var set
      {
         if (!po || ((pmi->dwType != MIFLI_PROPDEF) && (pmi->dwType != MIFLI_PROPPRIV)))
            return NULL;

         PCMIFLVarProp pvp = (PCMIFLVarProp) po->m_hPropDefaultJustThis.Find (pmi->dwID);
         if (!pvp)
            return NULL;

         if (fFunc)
            return NULL;   // cant do this
         return (dwFrom == 4) ? pvp->m_pCodeGet : pvp->m_pCodeSet;
      }
      break;
#endif // 0

   }

   // default. shouldnt happen
   return NULL;
}


/*****************************************************************************
MIFLTabs - This code displays the tabs used for the MIFLTABS macro.

inputs
   DWORD          dwTab - Tab number that's currently selected
   DWORD          dwNum - Number of tabs
   PWSTR          *ppsz - Pointer to an array of dwNum tabs
   PWSTR          *ppszHelp - Pointer to an arrya of dwNum help entries
returns
   PWSTR - gMemTemp.p with text
*/
PWSTR MIFLTabs (DWORD dwTab, DWORD dwNum, PWSTR *ppsz, PWSTR *ppszHelp)
{
   MemCat (&gMemTemp, L"<tr>");

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      if (!ppsz[i]) {
         MemCat (&gMemTemp, L"<td/>");
         continue;
      }

      MemCat (&gMemTemp, L"<td align=center");
      if (i == dwTab)
         MemCat (&gMemTemp, L" bgcolor=#e0ffe0");
      MemCat (&gMemTemp,
         L">"
         L"<a href=tabpress:");
      MemCat (&gMemTemp, (int)i);
      MemCat (&gMemTemp, L">"
         L"<bold>");
      MemCatSanitize (&gMemTemp, ppsz[i]);
      MemCat (&gMemTemp, L"</bold>"
         L"<xHoverHelp>");
      MemCatSanitize (&gMemTemp, ppszHelp[i]);
      MemCat (&gMemTemp,
         L"</xHoverHelp>"
         L"</a>"
         L"</td>");
   } // i

   MemCat (&gMemTemp, L"</tr>");
   return (PWSTR)gMemTemp.p;
}

// BUGBUG - When get around to full IF will need to put in special string functions
// like parsing, etc. so it's call handled in C++




#if 0 // def _DEBUG
/************************************************************************************
HackRenameAll - Given a CMem with a string, this searches for szOrig and replaces
it with szNew. Used to change "cMobile" to "cCharacter".

inputs
   PCMem          pMem - String
   PWSTR          pszOrig - Original string
   PWSTR          pszNew - New string
returns
   none
*/
void HackRenameAll (PCMem pMem, PWSTR pszOrig, PWSTR pszNew)
{
   PCWSTR pszFind = MyStrIStr ((PWSTR)pMem->p, pszOrig);
   if (!pszFind)
      return;

   DWORD dwLenOrig = (DWORD)wcslen(pszOrig);
   DWORD dwLenNew = (DWORD)wcslen(pszNew);
   DWORD dwOffset;
   PWSTR pszMem;
   CMem memTemp;

   while (pszFind) {
      pszMem = (PWSTR)pMem->p;
      dwOffset = ((PBYTE)pszFind - (PBYTE)pszMem) / sizeof(WCHAR);
      
      // set to zero and copy up until string
      pszMem[dwOffset] = 0;
      MemZero (&memTemp);
      MemCat (&memTemp, pszMem);
      
      // replacement
      MemCat (&memTemp, pszNew);

      // and later stuff
      MemCat (&memTemp, pszMem + (dwOffset + dwLenOrig));

      // copy over
      MemZero (pMem);
      MemCat (pMem, (PWSTR)memTemp.p);

      // re-search
      pszFind = MyStrIStr ((PWSTR)pMem->p, pszOrig);
   } // while find
}

#endif
