/***********************************************************************
CEscSearch.cpp - Search code

begun 5/16/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "escarpment.h"
#include "mmlinterpret.h"
#include "resource.h"
#include "resleak.h"

// BUGFIX - New search ID
#define  SEARCHID       0x348afdc3

extern HINSTANCE ghInstance;

typedef struct {
   DWORD    dwDate;     // date. 0 = no date associated
   DWORD    dwCategory; // category. 0 = documentation
} DOCINFOHEADER, *PDOCINFOHEADER;

/*************************************************************************8
myiswalpha - Because doesn't seem to handle all indications of alpha correctly

BUGFIX - Because wasnt dealing with non-english characters
*/
static BOOL myiswalpha (WCHAR c)
{
   if (c >= 128)
      return TRUE;
   return iswalpha (c);
}

/****************************************************************************
CEscSearch::Constructor & Destructor
*/
CEscSearch::CEscSearch (void)
{
   m_pszLastSearch = NULL;
   m_pszCurDocument = NULL;   // BUGFIX - So doesnt gp fault
   m_pszTitle = NULL;   // BUGFIX - So doesn't crash
   m_szCurSection[0] = 0; // BUGFIX - So doesnt crash
   m_fLoaded = FALSE;
   m_pszFile[0] = 0;
   m_fNeedIndexing = FALSE;
   m_hInstance = NULL;
   m_dwAppVersion = 0;
   m_pIndexCallback = 0;
   m_dwIndexDocuments = 0;
   m_pIndexUserData = 0;

   m_hWordsInSection.Init (sizeof(DWORD));
   m_hWords.Init (sizeof(PCMem));
}

CEscSearch::~CEscSearch (void)
{
   ClearWords ();
}

/****************************************************************************
CEscSearch::ClearWords - Clear the worlds allocated in m_hWords
*/
void CEscSearch::ClearWords (void)
{
   // loop
   DWORD i;
   for (i = 0; i < m_hWords.Num(); i++) {
      PCMem pMem = *((PCMem*)m_hWords.Get(i));
      delete pMem;
   }

   m_hWords.Clear();
}


/****************************************************************************
CEscSearch::Init - Initalizes the search object.

inputs
   HINSTANCE      hInstance - Module instance, where any resources may be found
   DWORD          dwAppVersion - A number that changes with every version/build
                     of the application. That way if the user upgrades version
                     their search database will be reindexed.
   PWSTR          pszFile - File that's used to store the search data. If this
                     isn't specified one is generated from GetModuleFilename(hInstance).
returns
   BOOL - TRUE if success
*/
BOOL CEscSearch::Init (HINSTANCE hInstance, DWORD dwAppVersion, PWSTR pszFile)
{
   if (!hInstance || m_hInstance)
      return FALSE;

   m_hInstance = hInstance;
   m_dwAppVersion = dwAppVersion;

   if (pszFile)
      wcscpy (m_pszFile, pszFile);
   else {
      char  szTemp[256];
      GetModuleFileName (m_hInstance, szTemp, sizeof(szTemp));

      // look back for the "."
      char  *pCur;
      for (pCur = szTemp + strlen(szTemp); pCur >= szTemp; pCur--)
         if (*pCur == '.') {
            pCur[1] = 0;
            break;
         }

      // appened the extension
      strcat (pCur, "xsr");

      // convert
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, m_pszFile, sizeof(m_pszFile)/2);
   }

   // done
   return TRUE;
}



/****************************************************************************
NeedIndexing - Returns TRUE if the application's MML needs to be reindexed.
   This happens if it hasn't been indexed on the machine yet, the index has
   been deleted, or the application's version ID has changed

inputs
returns  
   BOOL - TRUE if the application should call Index(). FALSE if don't need to
*/
BOOL CEscSearch::NeedIndexing (void)
{
   LoadIfNotLoaded();

   return m_fNeedIndexing || (m_dwFileVersion != m_dwAppVersion);
}


/****************************************************************************
LoadIfNotLoaded - If the data isn't already loaded then it loads it in.

returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL CEscSearch::LoadIfNotLoaded (void)  // load the file if it's not already loaded
{
   if (m_fLoaded)
      return TRUE;   // already made a load attempt

   m_fLoaded = TRUE;
   m_fNeedIndexing = FALSE;

   // open the file
   FILE  *f;
   char  szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, m_pszFile, -1, szTemp, sizeof(szTemp), 0,0);
   OUTPUTDEBUGFILE (szTemp);
   f = fopen(szTemp, "rb");
   if (!f) {
      // no file, so need indexing
      m_fNeedIndexing = TRUE;
      return TRUE;
   }

   // read in signature for the search file
   DWORD    dw;
   fread(&dw, sizeof(dw), 1, f);
   if (dw != SEARCHID) {
      // invalid file
      fclose (f);
      m_fNeedIndexing = TRUE;
      return TRUE;
   }


   // read in the version number
   fread (&m_dwFileVersion, sizeof(m_dwFileVersion), 1, f);

   // read in the document list
   if (!m_listDocuments.FileRead (f)) {
      fclose (f);
      return FALSE;
   }

   // read in the words
   if (!FileRead (f)) {
      fclose (f);
      return FALSE;
   }

   // done
   fclose (f);
   return TRUE;
}



/*****************************************************************************
EnumResource - Callback to enumerate the MML resources.
*/
BOOL __stdcall CALLBACK EnumResource(  HINSTANCE hModule,   // module handle
  LPCTSTR lpszType, // pointer to resource type
  LPTSTR lpszName,  // pointer to resource name
  LONG_PTR lParam       // application-defined parameter
  )
{
   PCListFixed p = (PCListFixed) lParam;

   //if ((DWORD)lpszName < 0x10000)
   if (IS_INTRESOURCE(lpszName))
      p->Add (&lpszName);
   return TRUE;
}


/*****************************************************************************
IndexNodeInternal - Given a node. This indexes the node and its contents.

inputs
   PCMMLNode      pNode
*/
BOOL CEscSearch::IndexNodeInternal (PCMMLNode pNode)
{
   // skip macros definitions & stuff
   if (pNode->m_dwType != MMLCLASS_ELEMENT)
      return TRUE;

   // if it's a section tag then change the section
   PWSTR psz;
   psz = pNode->NameGet();
   if (psz && !_wcsicmp(psz, L"Section")) {
      // flush ouf old section
      SectionFlush();

      psz = pNode->AttribGet(L"name");
      if (psz)
         wcscpy (m_szCurSection, psz);
      else {
         // get contents
         CMem  mem;
         EscTextFromNode (pNode, &mem);
         mem.CharCat (0);

         if (mem.m_dwCurPosn < sizeof(m_szCurSection))
            wcscpy (m_szCurSection, (PWSTR)mem.p);
         else
            m_szCurSection[0] = 0;
      }
   }

   BYTE  bOldScore;
   bOldScore = m_bCurRelevence;

   // will need to look out for "Keyword" tags to affect relevence of words
   psz = pNode->NameGet();
   if (psz && !_wcsicmp(psz, L"Keyword")) {
      // if there's a new score use that
      int   iScore;
      if (AttribToDecimal (pNode->AttribGet(L"score"), &iScore)) {
         if ((iScore >= 0) && (iScore <= 255))
            m_bCurRelevence = (BYTE) iScore;
      }

      // if there are words then index them
      psz = pNode->AttribGet(L"Words");
      if (psz)
         IndexText (psz);
   }


   DWORD i;
   PCMMLNode   pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);

      if (pSub)
         IndexNodeInternal (pSub);
      else if (psz)
         IndexText (psz);
   }

   // restore the old relevence in case changed with index
   m_bCurRelevence = bOldScore;

   return TRUE;
}

/*****************************************************************************
IndexNode - Given a node. This indexes the node and its contents.

inputs
   PCMMLNode      pNode
   PWSTR          pszTitle - Title
   PWSTR          pszDocument - Document
*/
BOOL CEscSearch::IndexNode (PCMMLNode pNode, PWSTR pszTitle, PWSTR pszDocument)
{
   if (pszTitle)
      m_pszTitle = pszTitle;
   if (pszDocument)
      m_pszCurDocument = pszDocument;

   DWORD i;
   PCMMLNode   pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pSub = NULL;
      pNode->ContentEnum(i, &psz, &pSub);

      if (pSub)
         IndexNodeInternal (pSub);
      else if (psz)
         IndexText (psz);
   }

   return TRUE;
}

/*****************************************************************************
IndexText - Given a text string, this pulls out the words and adds them to
the m_hWordsInSection list

inputs
   PWSTR psz - string
*/
BOOL CEscSearch::IndexText (PWSTR psz)
{
   WCHAR  *pCur;
   WCHAR  szTemp[256];

   for (pCur = psz; *pCur; ) {
      // repeat until get to word
      while (*pCur && !myiswalpha(*pCur))
         pCur++;
      if (!pCur[0])
         break;   // EOF

      // found the start
      WCHAR  *pStart;
      pStart = pCur;
      pCur++;

      while (*pCur && (
         myiswalpha(*pCur) || (*pCur == L'’') || (*pCur == L'\'') || (*pCur == L'_') ||
         ((pCur[0] == L'-') && myiswalpha(pCur[1])) ))
         pCur++;

      // copy over
      if (pCur > (pStart + ((sizeof(szTemp)/2)-2)))
         continue;   // too large

      memcpy (szTemp, pStart, (PBYTE) pCur - (PBYTE)pStart);
      szTemp [((PBYTE) pCur - (PBYTE)pStart)/2] = 0;


      DWORD i;
      for (i = 0; szTemp[i]; i++)
         if (szTemp[i] == L'’')
            szTemp[i] = L'\'';

      // see if the word exists
      DWORD *pdw;
      pdw = (DWORD*) m_hWordsInSection.Find(szTemp);
      if (pdw) {
         // use max score that found
         *pdw = max(*pdw, (DWORD) m_bCurRelevence);
      }
      else {
         // else add
         DWORD dw;
         dw = m_bCurRelevence;
         m_hWordsInSection.Add (szTemp, &dw);

      }

   }

   // all done
   return TRUE;
}

/*****************************************************************************
SectionFlish - If there are any words in m_hWordsInSection, this
creates a new document in the document list, using the doc's file name,
section name, etc. It then visits adds all the words to m_hWords
using the new document name
*/
BOOL CEscSearch::SectionFlush (void)
{
   // make sure to append cursection[0] onto link data
   WCHAR szHuge[1000];
   if (!m_pszCurDocument)
      szHuge[0] = 0; // BUGFIX - So doesn't crash
   else if ((wcslen(m_pszCurDocument) >= (sizeof(szHuge)/4)) || (wcslen(m_szCurSection) >= (sizeof(szHuge)/4))) {
      // too large to fit
      szHuge[0] = 0;
   }
   else {
      wcscpy (szHuge, m_pszCurDocument);
      if (m_szCurSection[0]) {
         // it has a section name so append that to link name
         wcscat (szHuge, L"#");
         wcscat (szHuge, m_szCurSection);
      }
   }

   return SectionFlush (m_pszTitle, m_szCurSection, szHuge);
}


// FileWrite - Write the contents of the list to a file.
BOOL CEscSearch::FileWrite (FILE *pf)
{
   DWORD dwElem = m_hWords.Num();

   if (1 != fwrite (&dwElem, sizeof(dwElem), 1, pf))
      return FALSE;

   // write all the elements
   DWORD i;
   size_t dwSize;
   PCMem pElem;
   for (i = 0; i < dwElem; i++) {
      PWSTR psz = m_hWords.GetString(i);
      if (!psz)
         continue;
      DWORD dwSizeString = ((DWORD)wcslen(psz)+1)*sizeof(WCHAR);
      
      pElem = *((PCMem*)m_hWords.Get (i));

      dwSize = pElem->m_dwCurPosn + dwSizeString;

      // write
      if (1 != fwrite (&dwSize, sizeof(dwSize), 1, pf))
         return FALSE;
      if (dwSizeString != fwrite (psz, 1, dwSizeString, pf))
         return FALSE;
      if (pElem->m_dwCurPosn != fwrite (pElem->p, 1, pElem->m_dwCurPosn, pf))
         return FALSE;
   }

   // done
   return TRUE;
}

// FileRead - Read the contents of the file into the list.
BOOL CEscSearch::FileRead (FILE *pf)
{
   ClearWords();

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

      // string and data
      PWSTR psz = (PWSTR)memTemp.p;
      DWORD dwStringLen = ((DWORD)wcslen(psz)+1) * sizeof(WCHAR);
      dwSize -= dwStringLen;

      PCMem pMem = new CMem;
      if (!pMem)
         return FALSE;
      if (!pMem->Required (dwSize)) {
         delete pMem;
         return FALSE;
      }
      pMem->m_dwCurPosn = dwSize;
      memcpy (pMem->p, (PBYTE)memTemp.p + dwStringLen, dwSize);

      // write it
      if (!m_hWords.Add (psz, &pMem))
         return FALSE;
   }

   // all done
   return TRUE;
}

/*****************************************************************************
SectionFlush - Flushes the section, using the given page title, section
   name, and LinkData.

inputs
   PWSTR       pszDocName - Document name to show. Ends up in m_listFound
   PWSTR       pszSectionName - Section name to show. Ends up in m_listFound
   PWSTR       pszLinkData - Link data to pass on Ends up in m_listFound
   DWORD       dwDate - Date. (year << 16) | (month << 8) | (day << 0). Year = 2000+.
               Month = 1..12. Day = 1..31.
   DWORD       dwCategory - 0 = documentation. Rest are application defined.
returns
   BOOL - TRUE if successful
*/
BOOL CEscSearch::SectionFlush (PWSTR pszDocName, PWSTR pszSectionName, PWSTR pszLinkData,
                               DWORD dwDate, DWORD dwCategory)
{

   // create the document info and name
   WCHAR    szHuge[1000];
   PDOCINFOHEADER ph = (PDOCINFOHEADER) szHuge;
   DWORD    dwPosn = sizeof(DOCINFOHEADER)/sizeof(WCHAR);
   ph->dwDate = dwDate;
   ph->dwCategory = dwCategory;

   wcscpy (szHuge + dwPosn, pszDocName ? pszDocName : L"");
   dwPosn += ((DWORD)wcslen(szHuge+dwPosn)+1);
   wcscpy (szHuge + dwPosn, pszSectionName);
   dwPosn += ((DWORD)wcslen(szHuge+dwPosn)+1);
   wcscpy (szHuge + dwPosn, pszLinkData);
   dwPosn += ((DWORD)wcslen(szHuge+dwPosn)+1);
   DWORD dwNum;
   dwNum = m_listDocuments.Add (szHuge, dwPosn * 2);
   if (dwNum == (DWORD)-1)
      return FALSE;

   // loop through all the words in m_hWordsInSection and add them to
   // m_hWords
   DWORD i;
   PWSTR psz;
   DWORD dwScore;
   //DWORD dwSize;
   //DWORD dwAlloc;
   DWORD *pdwAlloc;
   for (i = 0; i < m_hWordsInSection.Num(); i++) {
      psz = m_hWordsInSection.GetString(i);
      if (!psz)
         break;
      dwScore = *((DWORD*)m_hWordsInSection.Get (i));

      // see if it's in the main one
      PCMem *ppMem = (PCMem*)m_hWords.Find (psz);
      PCMem pMem = ppMem ? *ppMem : NULL;

      // if not found the put it in
      if (!pMem) {
         pMem = new CMem;
         if (!pMem)
            continue;   // error
         if (!pMem->Required(sizeof(DWORD))) {
            delete pMem;
            continue; // error
         }

         pdwAlloc = (DWORD*) pMem->p;
         pMem->m_dwCurPosn = sizeof(DWORD);
         pdwAlloc[0] = (dwScore << 24) + dwNum;

         // add it
         m_hWords.Add (psz, &pMem);
#if 0 // def _DEBUG
         char  sz1[256], sz2[256];
         WideCharToMultiByte (CP_ACP, 0, psz, -1, sz1, sizeof(sz1), 0, 0);
         wsprintf (sz2, "Added %s\r\n", sz1);
         OutputDebugString (sz2);
#endif // _DEBUG
         continue;
      }

      // else, exists, see if need to allocate more
      size_t dwNeed = pMem->m_dwCurPosn + sizeof(DWORD);
      if (!pMem->Required (dwNeed))
         continue;   // error
      ((DWORD*)pMem->p)[pMem->m_dwCurPosn/sizeof(DWORD)] = (dwScore << 24) + dwNum;
      pMem->m_dwCurPosn = dwNeed;
   }


   // finally, clear out the words
   m_hWordsInSection.Clear();

   return TRUE;
}


/*****************************************************************************
GenerateIndexPage - Code to generate the index
*/
#define  TIMERAMT       500
BOOL GenerateIndexPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCEscSearch pSearch = (PCEscSearch) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // tell the progress bar
         PCEscControl pc;
         pc = pPage->ControlFind(L"progress");
         if (pc) {
            pc->AttribSetInt (L"max",
               pSearch->m_dwNumFileEnum + pSearch->m_plistEnum->Num() + pSearch->m_dwIndexDocuments);
            pc->AttribSetInt (L"pos", 0);
         }

         // set a timer to do scan
         pPage->m_pWindow->TimerSet (TIMERAMT, pPage);   // very short timer


         // speak
         // this doesn't work because takes too much CPU to generate search index
         //EscChime (ESCCHIME_INFORMATION);
         //if (EscSpeak (L"Please wait one moment while the search index is generated."))
         //   return IDOK;

      }
      return TRUE;

   case ESCM_TIMER:
      {
         PESCMTIMER pt = (PESCMTIMER) pParam;
         CEscError   error;

         DWORD dwLeaveTime;
         dwLeaveTime = GetTickCount();
         dwLeaveTime += TIMERAMT/5*4;

         // BUGFIX - Move update out of the loop so don't call as much
         // tell the progress bar
         PCEscControl pc;
         pc = pPage->ControlFind(L"progress");
         if (pc)
            pc->AttribSetInt (L"pos", pSearch->m_dwCurEnum);
         pPage->Update();

         // BUGFIX - Make search faster by doing many indexes per time slot.
         while (TRUE) {
            PWSTR pszText = NULL;
            PCMMLNode   pNode = NULL;
            // make sure not beyond edge already
            DWORD dwNumTotal, dwNum;
            dwNum = pSearch->m_dwNumFileEnum + pSearch->m_plistEnum->Num();
            dwNumTotal = dwNum + pSearch->m_dwIndexDocuments;
            if (pSearch->m_dwCurEnum >= dwNumTotal) {
               pPage->m_pWindow->TimerKill (pt->dwID);
               pPage->Exit (L"done");
               return TRUE;
            }

            // if we're more than dwNum (the usual list), then do the callback
            if (pSearch->m_dwCurEnum >= dwNum) {
               // set the relevence higher for user documents?
               pSearch->m_bCurRelevence = 64;
               pSearch->m_hWordsInSection.Clear();

               // if there's a callback call it
               if (pSearch->m_pIndexCallback)
                  (pSearch->m_pIndexCallback)(pSearch, pSearch->m_dwCurEnum - dwNum, pSearch->m_pIndexUserData);

               // skip the rest
               goto moveon;
            }

            // analyze the current file
            // load it in
            WCHAR szTemp[32];
            if (pSearch->m_dwCurEnum < pSearch->m_plistEnum->Num()) {
               // resource
               pszText = ResourceToUnicode (pSearch->m_hInstance, *((DWORD*)pSearch->m_plistEnum->Get(pSearch->m_dwCurEnum)));

               // current name
               swprintf (szTemp, L"r:%d", *((DWORD*)pSearch->m_plistEnum->Get(pSearch->m_dwCurEnum)));
               pSearch->m_pszCurDocument = szTemp;
            }
            else {
               // file
               pszText = FileToUnicode (pSearch->m_papszFileEnum[pSearch->m_dwCurEnum - pSearch->m_plistEnum->Num()]);

               // current document
               pSearch->m_pszCurDocument = pSearch->m_papszFileEnum[pSearch->m_dwCurEnum - pSearch->m_plistEnum->Num()];
            }
            if (!pszText)
               goto moveon;

            // parse it
            pNode = ParseMML (pszText, pSearch->m_hInstance, pSearch->m_pCallback, NULL, &error);
            if (!pNode)
               goto moveon;

            // find the title
            PCMMLNode   pPageInfo;
            pSearch->m_pszTitle = NULL;
            pPageInfo = FindPageInfo (pNode);
            if (pPageInfo) {
               pSearch->m_pszTitle = pPageInfo->AttribGet (L"title");

               // should we not indwx
               BOOL fRet;
               if (AttribToYesNo (pPageInfo->AttribGet (L"index"), &fRet)) {
                  if (!fRet)
                     goto moveon;
               }
            }

            // set some defaults
            pSearch->m_szCurSection[0] = 0;
            pSearch->m_hWordsInSection.Clear();

            // index the title
            if (pSearch->m_pszTitle) {
               pSearch->m_bCurRelevence = 128;
               pSearch->IndexText (pSearch->m_pszTitle);
            }
            pSearch->m_bCurRelevence = 32;

            // actually index
            pSearch->IndexNodeInternal (pNode);

            // flush the last m_hWordsInSection
            pSearch->SectionFlush ();

moveon:
            if (pSearch)
               pSearch->m_pszCurDocument = NULL;   // BUGFIX - in case was looking at sztemp

            if (pszText)
               ESCFREE (pszText);

            if (pNode)
               delete pNode;

            // move on
            pSearch->m_dwCurEnum++;
            if (pSearch->m_dwCurEnum >= dwNumTotal) {
               pPage->m_pWindow->TimerKill (pt->dwID);
               pPage->Exit (L"done");
               return TRUE;
            }

            // if we've overstaid our timeout then leave
            if (GetTickCount() > dwLeaveTime)
               return TRUE;
#ifdef _DEBUG
            OutputDebugString ("#");
#endif
         }
      }
      return TRUE;

   case ESCM_CLOSE:
      return TRUE;
      // dont allow the user to close
   }

   return FALSE;
}


/*****************************************************************************
Index - Indexes the application's MML.

inputs
   HWND     hWndUI - Window to bring user interface up to show percent
               complete.
   PESCPAGECALLBACK pCallback - Page callback, used for every document to
               see if any substituions should be made. This callback will
               not have a CEscPage object though.
   BOOL     fEnumMML - If TRUE (default) then enumerate all MML resources
               in the application and index those. NOTE: If <pageinfo index=false>
               is set then the page is not indexed.
   DWORD    pdwMMLExclude - If not NULL, this is a list of resoruce numbers
               that are excluded from indexing.
   DWORD    dwMMLExcludeCount - Number of elements in pdwMMLExclude
   PWSTR    papszIncludeFile - If not NULL, this is a pointer to an array of
               string-pointers of MML files to also index.
   DWORD    dwIncludeFileCount - Number of strings in papszIncludeFile
returns
   BOOL - TRUE if OK
*/
BOOL CEscSearch::Index (HWND hWndUI, PESCPAGECALLBACK pCallback,
      BOOL fEnumMML, DWORD *pdwMMLExclude, DWORD dwMMLExcludeCount,
      PWSTR *papszIncludeFile, DWORD dwIncludeFileCount)
{
   ESCINDEX i;
   memset (&i, 0, sizeof(i));

   i.hWndUI = hWndUI;
   i.pCallback = pCallback;
   i.fNotEnumMML = !fEnumMML;
   i.pdwMMLExclude = pdwMMLExclude;
   i.dwMMLExcludeCount = dwMMLExcludeCount;
   i.papszIncludeFile = papszIncludeFile;
   i.dwIncludeFileCount = dwIncludeFileCount;

   return Index (&i);
}


/***********************************************************************************
Index - Indexes the application's MML.

inputs
   PESCINDEX   pIndex - Indexing data all wrapped into a structure
returns
   BOOL - TRUE if OK
*/
BOOL CEscSearch::Index (PESCINDEX pIndex)
{
   // store some stuff away
   m_pIndexCallback = pIndex->pIndexCallback;
   m_dwIndexDocuments = pIndex->dwIndexDocuments;
   m_pIndexUserData = pIndex->pIndexUserData;

   // clear out old stuff
   m_fLoaded = TRUE;
   m_fNeedIndexing = FALSE;
   ClearWords();
   m_listDocuments.Clear();
   m_dwFileVersion = m_dwAppVersion;

   // figure out all the resources that need to index
   CListFixed     lEnum;
   lEnum.Init (sizeof(DWORD));
   if (!pIndex->fNotEnumMML)
      EnumResourceNames (m_hInstance, "MML", EnumResource, (LONG_PTR) &lEnum);

   // weed out ones not wanted
   DWORD i, j;
   DWORD *pdw;
   if (pIndex->dwMMLExcludeCount && pIndex->pdwMMLExclude) {
      for (i = lEnum.Num()-1; i < lEnum.Num(); i--) {
         pdw = (DWORD*) lEnum.Get(i);
         if (!pdw)
            break;

         // loop though exclude
         for (j = 0; j < pIndex->dwMMLExcludeCount; j++)
            if (pIndex->pdwMMLExclude[j] == *pdw)
               break;
         if (j >= pIndex->dwMMLExcludeCount)
            continue;   // not excluded

         // else, exluded. delete
         lEnum.Remove(i);

         // continue on
      }
   }

   // UI
   m_plistEnum = &lEnum;
   m_papszFileEnum = pIndex->papszIncludeFile;
   m_dwNumFileEnum = pIndex->dwIncludeFileCount;
   m_dwCurEnum = 0;
   m_pCallback = pIndex->pCallback;
   CEscWindow  cWindow;
   if (!cWindow.Init (ghInstance, pIndex->hWndUI, EWS_NOTITLE | EWS_FIXEDSIZE | EWS_NOVSCROLL | EWS_AUTOHEIGHT | EWS_FIXEDWIDTH))
      return FALSE;
   cWindow.PageDialog (ghInstance, IDR_CALCINDEX, GenerateIndexPage, this);

   // save search to disk
   // open the file
   FILE  *f;
   char  szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, m_pszFile, -1, szTemp, sizeof(szTemp), 0,0);
   OUTPUTDEBUGFILE (szTemp);
   f = fopen(szTemp, "wb");
   if (!f)
      return FALSE;

   // write in signature for the search file
   DWORD    dw;
   dw = SEARCHID;
   fwrite(&dw, sizeof(dw), 1, f);

   // Write in the version number
   fwrite (&m_dwFileVersion, sizeof(m_dwFileVersion), 1, f);

   // read in the document list
   if (!m_listDocuments.FileWrite (f)) {
      fclose (f);
      return FALSE;
   }

   // read in the words
   if (!FileWrite (f)) {
      fclose (f);
      return FALSE;
   }

   // done
   fclose (f);
   return TRUE;
}


/******************************************************************************
Search - Given keywords separated by a space, this searches through the
index looking for the best 100 matches. (You should call NeedIndexing() first,
and if necessary, Index().)

This then fills in m_listFound with the best search elements, sorted by score.
It also fills in m_pszLastSearch so your application can remember the search.

inputs
   PWSTR    pszInput - keywords separated by spaces
returns
   BOOL - TRUE if successful
*/
typedef struct {
   // DWORD    *pdwTree;   // from m_hWords.Find
   DWORD    dwNum;      // number of occurances of this word in all documents
   DWORD    *pdwDoc;    // document #
   DWORD    dwCur;      // current position in find
} WORDFIND, *PWORDFIND;

typedef struct {
   DWORD    dwNum;      // document number
   DWORD    dwScore;     // score
} DOCSCORE, *PDOCSCORE;

int __cdecl DSSort (const void *elem1, const void *elem2 )
{
   DOCSCORE *p1, *p2;
   p1 = (DOCSCORE*)elem1;
   p2 = (DOCSCORE*)elem2;

   return (int) p2->dwScore - (int) p1->dwScore;
}


/*******************************************************************8
ToDayCount - Given a DFDATE approximate a day count.

inputs
   DWORD    dwDate - (year << 16) + (month << 8) + day
returns
   double - Number representing number of days since year 0
*/
double ToDayCount (DWORD dwDate)
{
   double   fDay;

   fDay = (double) (dwDate & 0xff) + (double)((dwDate >> 8) & 0xff) * 31.0 +
      (double) ((dwDate >> 16) & 0xffff) * 31.0 * 12.0;

   return fDay;
}

BOOL CEscSearch::Search (PWSTR pszInput, PESCADVANCEDSEARCH pInfo)
{
   // wipe out the old found list
   m_listFound.Clear();
   m_memLastSearch.m_dwCurPosn = 0;
   m_memLastSearch.StrCat (pszInput, (DWORD)wcslen(pszInput)+1);
   m_pszLastSearch = (PWSTR) m_memLastSearch.p;

   // identify the keywords that really appear
   CListFixed  lWF;    // list of pointers to DWORDs - from m_hWords
   lWF.Init(sizeof(WORDFIND));
   CListFixed  lDS;     // list of docscores
   lDS.Init(sizeof(DOCSCORE));
   WCHAR  *pCur, *pStart;
   for (pCur = pszInput; *pCur; ) {
      // skip whitespace
      while (*pCur && iswspace (*pCur))
         pCur++;
      if (!(*pCur))
         break;
      pStart = pCur;

      // find next whitespace
      while (*pCur && !iswspace(*pCur))
         pCur++;


      // copy this to a temporary location
      WCHAR szTemp[256];
      DWORD i;
      for (i = 0; (i < 255) && pStart[i] && ((pStart+i) < pCur); i++)
         szTemp[i] = pStart[i];
      szTemp[i] = 0;

      PCMem *ppMem = (PCMem*)m_hWords.Find(szTemp);
      PCMem pMem = ppMem ? ppMem[0] : NULL;
      DWORD *pdw = pMem ? (DWORD*)pMem->p : NULL;
      if (!pdw)
         continue;   // not a real word

      // add it
      WORDFIND wf;
      wf.dwCur = 0;
      wf.dwNum = (DWORD) (pMem->m_dwCurPosn / sizeof(DWORD));
      // wf.pdwTree = 
      wf.pdwDoc = pdw;
      lWF.Add (&wf);
   }

   // find the max score could possibly get
   double   fMaxScore;
   fMaxScore = 0;
   WORDFIND *pwf;
   DWORD i;
   for (i = 0; i < lWF.Num(); i++) {
      pwf = (WORDFIND*)lWF.Get(i);
      fMaxScore += sqrt(1.0 / pwf->dwNum);
   }
   if (!fMaxScore)
      fMaxScore = 1.0;

   // go through all documents pointed to by all lWF and add them to the list
   while (TRUE) {
      // find the lowest document number in all of lWF
      DWORD dwLowest = (DWORD)-1;
      DWORD dwDoc;
      for (i = 0; i < lWF.Num(); i++) {
         pwf = (WORDFIND*)lWF.Get(i);
         if (pwf->dwCur >= pwf->dwNum)
            continue;   // we've reached the end of this

         // see if it's lower
         dwDoc = pwf->pdwDoc[pwf->dwCur] & 0xffffff;
         if (dwDoc < dwLowest)
            dwLowest = dwDoc;
      }

      // if no lowest then we're all done, so break
      if (dwLowest == (DWORD)-1)
         break;

      // now, go though again and compute the score of the lowest.
      // while at it, move up position if count it
      double   fScore;
      fScore = 0;
      for (i = 0; i < lWF.Num(); i++) {
         pwf = (WORDFIND*)lWF.Get(i);
         if (pwf->dwCur >= pwf->dwNum)
            continue;   // we've reached the end of this

         // see if it's lower
         dwDoc = pwf->pdwDoc[pwf->dwCur] & 0xffffff;
         if (dwDoc != dwLowest)
            continue;

         DWORD dwScore;
         dwScore = pwf->pdwDoc[pwf->dwCur] >> 24;
         if (dwScore)
            fScore += (sqrt(1.0 / pwf->dwNum) * (double) dwScore / 255.0);

         // move on
         pwf->dwCur++;
      }

      // write this
      DOCSCORE ds;
      ds.dwNum = dwLowest;
      ds.dwScore = (DWORD) (fScore / fMaxScore * 100000);

      // BUGFIX - if we have search critera for date/category then use
      if (pInfo) {
         PDOCINFOHEADER ph = (PDOCINFOHEADER) m_listDocuments.Get(ds.dwNum);
         if (!ph)
            continue;   // error.

         // category test
         if ((ph->dwCategory < pInfo->dwUseCategoryCount) && !pInfo->pafUseCategory[ph->dwCategory])
            continue;   // not an acceptable category, so don't ad

         // are there date constraints?
         if (!pInfo->dwMostRecent && !pInfo->dwOldest)
            goto keepit;
         if (!ph->dwDate) {
            if (pInfo->fExclude)
               continue;
            else
               goto keepit;
         }

         // else, see if it's in range
         if (pInfo->dwMostRecent && (ph->dwDate > pInfo->dwMostRecent))
            continue;   // later than the most recent date so toss it
         if (pInfo->dwOldest && (ph->dwDate < pInfo->dwOldest))
            continue;   // earlier than oldest date so toss it.

         // if get here want to keep it, but maybe do score scaling
         if (pInfo->fRamp && pInfo->dwMostRecent && pInfo->dwOldest && (pInfo->dwMostRecent > pInfo->dwOldest)) {
            double   fScale;
            fScale = ToDayCount(ph->dwDate) - ToDayCount(pInfo->dwOldest)+1;
            fScale /= (ToDayCount(pInfo->dwMostRecent) - ToDayCount(pInfo->dwOldest) + 1);

            ds.dwScore = (DWORD) (ds.dwScore * fScale);
         }
      }

keepit:
      lDS.Add (&ds);
   }

   // sort them
   qsort (lDS.Get(0), lDS.Num(), sizeof(DOCSCORE), DSSort);

   // take the first 100 and add them
   for (i = 0; i < min(lDS.Num(), 100); i++) {
      BYTE  abHuge[10000];

      DOCSCORE *pds;
      pds = (DOCSCORE*) lDS.Get(i);

      // get the document info
      PVOID pInfo;
      DWORD dwSize;
      pInfo = (PBYTE)m_listDocuments.Get(pds->dwNum) + sizeof(DOCINFOHEADER);
      if (!pInfo)
         continue;
      dwSize = (DWORD) m_listDocuments.Size(pds->dwNum);
      if (dwSize > sizeof(DOCINFOHEADER))
         dwSize -= sizeof(DOCINFOHEADER);
      else
         dwSize = 0;

      // fill in the score at the beginning
      DWORD dwPosn;
      dwPosn = 0;
      *((DWORD*)(abHuge+dwPosn)) = pds->dwScore;
      dwPosn += sizeof(DWORD);
      memcpy (abHuge + dwPosn, (PBYTE)pInfo, dwSize);
      dwPosn += (DWORD) dwSize;

      // add it
      m_listFound.Add (abHuge, dwPosn);
   }

   // done
   return TRUE;
}

