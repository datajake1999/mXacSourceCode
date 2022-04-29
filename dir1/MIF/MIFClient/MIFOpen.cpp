/***************************************************************************
MIFOpen.cpp - Code that opens a CRF file.

begun 1/26/2006
Copyright 2006 Mike Rozak. All rights reserved
*/


#include <windows.h>
//#include <winsock2.h>
//#include <winerror.h>
#include <objbase.h>
#include <dsound.h>
#include <shlwapi.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"



// CCircumrealityDirCache - Caches information about the various directories that have
// looked throug, remember what Circumreality file information there is
class CCircumrealityDirCache {
public:
   ESCNEWDELETE;

   CCircumrealityDirCache (void);
   ~CCircumrealityDirCache (void);

   PCCircumrealityDirInfo DirGet (PWSTR pszDir, PCProgressSocket pProgress);

   BOOL Open (void);
   BOOL Save (void);

private:
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   CListFixed        m_lPCCircumrealityDirInfo;       // pointer of Circumreality dirs
};
typedef CCircumrealityDirCache *PCCircumrealityDirCache;


// WOD
typedef struct {
   PCCircumrealityDirCache       pCache;     // file cache
   WCHAR                szDir[256]; // current directory
   WCHAR                szFile[256];   // file
   BOOL                 fSave;      // if TRUE then saving file
   LANGID               lid;        // preferred language to use
} WOD, *PWOD;


/******************************************************************************
CCircumrealityFileInfo::Constuctor and destructor*/
CCircumrealityFileInfo::CCircumrealityFileInfo (void)
{
   m_pszFile = NULL;
   memset (&m_FileTime, 0 ,sizeof(m_FileTime));
   m_iFileSize = 0;
   m_fCircumreality = TRUE;
   m_pTitleInfoSet = NULL;
   m_ServerLoad.dwConnections = m_ServerLoad.dwMaxConnections = (DWORD)-1;
}

CCircumrealityFileInfo::~CCircumrealityFileInfo (void)
{
   if (m_pTitleInfoSet)
      delete m_pTitleInfoSet;
}

/******************************************************************************
CCircumrealityFileInfo::SetText - Sets the text for the file name, text spoken, and
speaker. This basically makes sure the memory is large enough and allocates
it all.

inputs
   PWSTR          pszFile - File name
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityFileInfo::SetText (PWSTR pszFile)
{
   DWORD dwNeedFile = (DWORD)wcslen(pszFile)+1;
   DWORD dwNeed = (dwNeedFile) * sizeof(WCHAR);
   if (!m_memInfo.Required (dwNeed))
      return FALSE;

   m_pszFile = (PWSTR) m_memInfo.p;
   memcpy (m_pszFile, pszFile, dwNeedFile * sizeof(WCHAR));

   return TRUE;
}

/******************************************************************************
CCircumrealityFileInfo::FillFromFile - Reads in a Circumreality file and uses it to fill in the
given information

NOTE: This doesn't fill in file size or last modified time.

inputs
   PWSTR          pszDir - Directory, such as "c:\hello"... no last slash...
   PWSTR          pszFile - File such as "hello.wav"
returns
   BOOL - TRUE if success. FALSE if failed
*/
BOOL CCircumrealityFileInfo::FillFromFile (PWSTR pszDir, PWSTR pszFile)
{
   WCHAR szFile[512];
   if (pszDir) {
      wcscpy (szFile, pszDir);
      wcscat (szFile, L"\\");
   }
   else
      szFile[0] = 0;
   wcscat (szFile, pszFile);
   
   BOOL fCircumreality;
   PCResTitleInfoSet pNew = PCResTitleInfoSetFromFile (szFile, &fCircumreality);
   if (!pNew)
      return FALSE;
   if (m_pTitleInfoSet)
      delete m_pTitleInfoSet;
   m_pTitleInfoSet = pNew;
   m_fCircumreality = fCircumreality;

   m_ServerLoad.dwConnections = m_ServerLoad.dwMaxConnections = (DWORD)-1;

   // store the info
   if (!SetText (pszFile))
      return FALSE;

   return TRUE;
}

/******************************************************************************
CCircumrealityFileInfo::FillFromFindFile - Fills in the information about the file from
the FindNextFile() call's return.

NOTE: Becayse there isn't any info about duration, etc, that is blanked out.

inputs
   PWIN32_FIND_DATA     pFF - Info about the file
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityFileInfo::FillFromFindFile (PWIN32_FIND_DATA pFF)
{
   WCHAR szName[256];
   MultiByteToWideChar (CP_ACP, 0, pFF->cFileName, -1, szName, sizeof(szName)/sizeof(WCHAR));

   if (!SetText (szName))
      return FALSE;

   if (m_pTitleInfoSet)
      delete m_pTitleInfoSet;
   m_fCircumreality = TRUE;
   m_pTitleInfoSet = NULL;
   m_FileTime = pFF->ftLastWriteTime;
   m_iFileSize = ((__int64)pFF->nFileSizeHigh << 32) + (__int64)(DWORD)pFF->nFileSizeLow;
   m_ServerLoad.dwConnections = m_ServerLoad.dwMaxConnections = (DWORD)-1;
   return TRUE;
}

/*********************************************************************************
CCircumrealityFileInfo::Clone - standard clone
*/
CCircumrealityFileInfo *CCircumrealityFileInfo::Clone (void)
{
   PCCircumrealityFileInfo pNew = new CCircumrealityFileInfo;
   if (!pNew)
      return NULL;

   pNew->SetText (m_pszFile);
   pNew->m_fCircumreality = m_fCircumreality;
   if (pNew->m_pTitleInfoSet)
      delete pNew->m_pTitleInfoSet;
   pNew->m_pTitleInfoSet = m_pTitleInfoSet ? m_pTitleInfoSet->Clone() : NULL;
   pNew->m_FileTime = m_FileTime;
   pNew->m_iFileSize = m_iFileSize;
   pNew->m_ServerLoad = m_ServerLoad;

   return pNew;
}


static PWSTR gpszCircumrealityFileInfo = L"CircumrealityFileInfo";
static PWSTR gpszFile = L"File";
static PWSTR gpszText = L"Text";
static PWSTR gpszFileTime = L"FileTime";
static PWSTR gpszFileSize = L"FileSize";
static PWSTR gpszTitleInfoSet = L"TitleInfoSet";
static PWSTR gpszCircumreality = L"Circumreality";

/******************************************************************************
CCircumrealityFileInfo::MMLTo - Standard MML to
*/
PCMMLNode2 CCircumrealityFileInfo::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszCircumrealityFileInfo);
   
   if (!m_pszFile[0]) {
      delete pNode;
      return FALSE;
   }
   MMLValueSet (pNode, gpszFile, m_pszFile);

   MMLValueSet (pNode, gpszFileTime, (PBYTE)&m_FileTime, sizeof(m_FileTime));
   MMLValueSet (pNode, gpszFileSize, (PBYTE)&m_iFileSize, sizeof(m_iFileSize));
   MMLValueSet (pNode, gpszCircumreality, (int)m_fCircumreality);
   
   PCMMLNode2 pSub = m_pTitleInfoSet ? m_pTitleInfoSet->MMLTo() : NULL;
   if (pSub) {
      pSub->NameSet (gpszTitleInfoSet);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/******************************************************************************
CCircumrealityFileInfo::MMLFrom - Standard MML from
*/
BOOL CCircumrealityFileInfo::MMLFrom (PCMMLNode2 pNode)
{
   m_ServerLoad.dwConnections = m_ServerLoad.dwMaxConnections = (DWORD)-1;

   if (m_pTitleInfoSet)
      delete m_pTitleInfoSet;
   m_pTitleInfoSet =  NULL;

   PWSTR pszFile;
   pszFile = MMLValueGet (pNode, gpszFile);
   if (!pszFile)
      return NULL;
   if (!SetText (pszFile))
      return FALSE;
   

   MMLValueGetBinary (pNode, gpszFileTime, (PBYTE)&m_FileTime, sizeof(m_FileTime));
   MMLValueGetBinary (pNode, gpszFileSize, (PBYTE)&m_iFileSize, sizeof(m_iFileSize));
   m_fCircumreality = (BOOL) MMLValueGetInt (pNode, gpszCircumreality, 0);
   
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      psz = NULL;
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszTitleInfoSet)) {
         if (m_pTitleInfoSet)
            delete m_pTitleInfoSet;
         m_pTitleInfoSet = new CResTitleInfoSet;
         if (m_pTitleInfoSet)
            m_pTitleInfoSet->MMLFrom (pSub);
      }
   } // i

   return TRUE;
}



/******************************************************************************
CCircumrealityDirInfo::Contructor and destructor
*/
CCircumrealityDirInfo::CCircumrealityDirInfo(void)
{
   m_szDir[0] = 0;
   m_lPCCircumrealityFileInfo.Init (sizeof(PCCircumrealityFileInfo));
}

CCircumrealityDirInfo::~CCircumrealityDirInfo (void)
{
   DWORD i;
   PCCircumrealityFileInfo *ppfi = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityFileInfo.Num(); i++)
      delete ppfi[i];
}


static PWSTR gpszCircumrealityDirInfo = L"CircumrealityDirInfo";
static PWSTR gpszDir = L"Dir";

/******************************************************************************
CCircumrealityDirInfo::MMLTo - Standard MML To
*/
PCMMLNode2 CCircumrealityDirInfo::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return pNode;
   pNode->NameSet (gpszCircumrealityDirInfo);

   if (m_szDir && m_szDir[0])
      MMLValueSet (pNode, gpszDir, m_szDir);

   DWORD i;
   PCCircumrealityFileInfo *ppfi = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityFileInfo.Num(); i++) {
      PCMMLNode2 pSub = ppfi[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   return pNode;
}

/******************************************************************************
CCircumrealityDirInfo::Clone - Clones this

returns
   PCCircumrealityDirInfo - clone
*/
PCCircumrealityDirInfo CCircumrealityDirInfo::Clone (void)
{
   PCCircumrealityDirInfo pClone = new CCircumrealityDirInfo;
   if (!pClone)
      return NULL;

   wcscpy (pClone->m_szDir, m_szDir);
   pClone->m_lPCCircumrealityFileInfo.Init (sizeof(PCCircumrealityFileInfo), m_lPCCircumrealityFileInfo.Get(0), m_lPCCircumrealityFileInfo.Num());
   PCCircumrealityFileInfo *ppfi = (PCCircumrealityFileInfo*) pClone->m_lPCCircumrealityFileInfo.Get(0);
   DWORD i;
   for (i = 0; i < pClone->m_lPCCircumrealityFileInfo.Num(); i++)
      ppfi[i] = ppfi[i]->Clone();

   return pClone;
}


/******************************************************************************
CCircumrealityDirInfo::MMLFrom - Standard
*/
BOOL CCircumrealityDirInfo::MMLFrom (PCMMLNode2 pNode)
{
   // clear out old
   DWORD i;
   PCCircumrealityFileInfo *ppfi = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityFileInfo.Num(); i++)
      delete ppfi[i];
   m_lPCCircumrealityFileInfo.Clear();

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszDir);
   if (psz)
      wcscpy (m_szDir, psz);
   else
      m_szDir[0] = 0;

   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszCircumrealityFileInfo)) {
         PCCircumrealityFileInfo pwi = new CCircumrealityFileInfo;
         if (!pwi)
            return FALSE;
         if (!pwi->MMLFrom (pSub))
            return FALSE;
         m_lPCCircumrealityFileInfo.Add (&pwi);
      }
   } // i

   return TRUE;
}


static int _cdecl PCCircumrealityFileInfoSort (const void *elem1, const void *elem2)
{
   PCCircumrealityFileInfo *pdw1, *pdw2;
   pdw1 = (PCCircumrealityFileInfo*) elem1;
   pdw2 = (PCCircumrealityFileInfo*) elem2;

   return _wcsicmp(pdw1[0]->m_pszFile, pdw2[0]->m_pszFile);
}

/******************************************************************************
CCircumrealityDirInfo::SyncFiles - This goes through the existing files in the
list and synchronizes them compared against themselves. It's different from SyncWithDirectory()
because it doesn't add or remove any files AND the files can be in different
directories is pszDir is NULL

inputs
   PCProgressSocket     pProgress - To show the progress in case there are 1000s of files
   PCWSTR               pszDir - Directory to prepend, or NULL if non
returns
   BOOL - TRUE if any files have changed, FALSE if not
*/
BOOL CCircumrealityDirInfo::SyncFiles (PCProgressSocket pProgress, PCWSTR pszDir)
{
   BOOL fChanged = FALSE;

   // loop through each of the files
   DWORD i;
   PCCircumrealityFileInfo *ppwi = (PCCircumrealityFileInfo*)m_lPCCircumrealityFileInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityFileInfo.Num(); i++) {
      PCCircumrealityFileInfo pwi = ppwi[i];
      if (pProgress && ((i%10) == 0))
         pProgress->Update ((fp)i / (fp)m_lPCCircumrealityFileInfo.Num());

      // create the file name
      char szTemp[512];
      if (pszDir) {
         WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szTemp, sizeof(szTemp), 0, 0);
         strcat (szTemp, "\\");
      }
      else
         szTemp[0] = 0;
      WideCharToMultiByte (CP_ACP, 0, pwi->m_pszFile, -1, szTemp + strlen(szTemp),
         (DWORD)(sizeof(szTemp) - strlen(szTemp)), 0 ,0);

      WIN32_FIND_DATA ffd;
      HANDLE hFile;
      hFile = CreateFile (szTemp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
         NULL, OPEN_EXISTING, 0, 0);
      if (!hFile)
         continue;
      memset (&ffd, 0, sizeof(ffd));
      GetFileTime (hFile, &ffd.ftCreationTime, &ffd.ftLastAccessTime, &ffd.ftLastWriteTime);
      ffd.nFileSizeLow = GetFileSize (hFile, &ffd.nFileSizeHigh);
      __int64 iSize = ((__int64)ffd.nFileSizeHigh << 32) + (__int64)(DWORD)ffd.nFileSizeLow;
      CloseHandle (hFile);

      // if not save time or size then resync
      if (memcmp(&pwi->m_FileTime, &ffd.ftLastWriteTime, sizeof(pwi->m_FileTime)) || (iSize != pwi->m_iFileSize)) {
         WCHAR szFile[256];
         wcscpy (szFile, pwi->m_pszFile);
         pwi->FillFromFile ((PWSTR)pszDir, szFile);
         pwi->m_FileTime = ffd.ftLastWriteTime;
         pwi->m_iFileSize = iSize;

         fChanged = TRUE;
      }
   } // i

   return fChanged;
}



/******************************************************************************
CCircumrealityDirInfo::RemoveFile - Searches through the list for the given file name
and removes it from the list; the file is NOT deleted

inputs
   PCWSTR         pszFile - File to remove
retursn
   BOOL - TRUE if success
*/
BOOL CCircumrealityDirInfo::RemoveFile (PCWSTR pszFile)
{
   DWORD j;
   PCCircumrealityFileInfo *ppwi;
   PCCircumrealityFileInfo pwi;
   ppwi = (PCCircumrealityFileInfo*)m_lPCCircumrealityFileInfo.Get(0);
   for (j = 0; j < m_lPCCircumrealityFileInfo.Num(); j++) {
      pwi = ppwi[j];
      if (!_wcsicmp(pwi->m_pszFile, pszFile)) {
         delete pwi;
         m_lPCCircumrealityFileInfo.Remove (j);
         return TRUE;
      }
   } // j

   return FALSE; // cant find
}

/******************************************************************************
CCircumrealityDirInfo::ClearFiles - Clears all the files from the list
*/
void CCircumrealityDirInfo::ClearFiles (void)
{
   // delete
   DWORD i;
   PCCircumrealityFileInfo *ppfi = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityFileInfo.Num(); i++)
      delete ppfi[i];
   m_lPCCircumrealityFileInfo.Clear();
}



/******************************************************************************
CCircumrealityDirInfo::SyncWithDirectory - This enunmerates all the files in the directory
(m_szDir) and finds the Circumreality files. This list of Circumreality files is then compared against
the list in m_lPCCircumrealityFileInfo. And files which no longer exist are removed, those
that have changed are reloaded, and new ones are added.

inputs
   PCProgressSocket     pProgress - To show the progress in case there are 1000s of files
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityDirInfo::SyncWithDirectory (PCProgressSocket pProgress)
{
   char szDir[256];

   // make a list of Circumreality files in the directory
   CListFixed lFound;
   lFound.Init (sizeof(PCCircumrealityFileInfo));
   WIN32_FIND_DATA FindFileData;
   HANDLE hFind;

   DWORD dwType;
   for (dwType = 0; dwType < 2; dwType++) {
      WideCharToMultiByte (CP_ACP, 0, m_szDir, -1, szDir, sizeof(szDir), 0, 0);
      strcat (szDir, dwType ? "\\*.crf" : "\\*.crk");

      hFind = FindFirstFile(szDir, &FindFileData);
      if (hFind != INVALID_HANDLE_VALUE) {
         while (TRUE) {
            PCCircumrealityFileInfo pInfo = new CCircumrealityFileInfo;
            if (!pInfo)
               break;
            if (!pInfo->FillFromFindFile (&FindFileData))
               delete pInfo;
            else
               lFound.Add (&pInfo);

            if (!FindNextFile (hFind, &FindFileData))
               break;
         }

         FindClose(hFind);
      }
   } // dwType

   // sort the list
   PCCircumrealityFileInfo *ppfFound = (PCCircumrealityFileInfo*) lFound.Get(0);
   DWORD dwFound = lFound.Num();
   qsort (ppfFound, dwFound, sizeof(PCCircumrealityFileInfo), PCCircumrealityFileInfoSort);

   // existing list doest need to be sorted because already is
   PCCircumrealityFileInfo *ppfList = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
   DWORD dwList = m_lPCCircumrealityFileInfo.Num();


   // loop over them
   DWORD dwCurFound = 0, dwCurList = 0;
   WCHAR szTemp[256];
   while ((dwCurFound < dwFound) || (dwCurList < dwList)) {
      if (pProgress && (dwFound + dwList))
         pProgress->Update ((fp)(dwCurFound + dwCurList) / (fp)(dwFound+dwList));

      int iRet;

      if ((dwCurFound < dwFound) && (dwCurList >= dwList))
         iRet = -1;
      else if ((dwCurFound >= dwFound) && (dwCurList < dwList))
         iRet = 1;
      else
         iRet = _wcsicmp(ppfFound[dwCurFound]->m_pszFile, ppfList[dwCurList]->m_pszFile);

      // if they're the same the just make sure the file is up to date
      if (iRet == 0) {
         if (!memcmp(&ppfFound[dwCurFound]->m_FileTime, &ppfList[dwCurList]->m_FileTime, sizeof(ppfList[dwCurList]->m_FileTime)) &&
            (ppfFound[dwCurFound]->m_iFileSize == ppfList[dwCurList]->m_iFileSize)) {

            dwCurFound++;
            dwCurList++;
            continue;   // no change so dont chare
            }

         // else, changed
         wcscpy (szTemp, ppfList[dwCurList]->m_pszFile);
         if (!ppfList[dwCurList]->FillFromFile (m_szDir, szTemp)) {
            // cant see to open anymore
            delete ppfList[dwCurList];

            m_lPCCircumrealityFileInfo.Remove (dwCurList);

            // reset...
            ppfList = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
            dwList = m_lPCCircumrealityFileInfo.Num();
            continue;
         }

         // copy over some info
         ppfList[dwCurList]->m_FileTime = ppfFound[dwCurFound]->m_FileTime;
         ppfList[dwCurList]->m_iFileSize = ppfFound[dwCurFound]->m_iFileSize;

         dwCurList++;
         dwCurFound++;
         continue;
      } // if same name

      // if the current found is less than the current list then need to insert
      if (iRet < 0) {
         // load info
         wcscpy (szTemp, ppfFound[dwCurFound]->m_pszFile);
         if (!ppfFound[dwCurFound]->FillFromFile (m_szDir, szTemp)) {
            dwCurFound++;
            continue;   // error
         }

         // add to the current list
         m_lPCCircumrealityFileInfo.Insert (dwCurList, &ppfFound[dwCurFound]);
         ppfFound[dwCurFound] = 0;
         dwCurList++;   // since don't want to look at the one just inserted

         // reset...
         ppfList = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
         dwList = m_lPCCircumrealityFileInfo.Num();

         dwCurFound++;
         continue;
      } // less than

      // else, if get here then list exists, but not in found... therefore remove
      delete ppfList[dwCurList];
      m_lPCCircumrealityFileInfo.Remove (dwCurList);

      // reset...
      ppfList = (PCCircumrealityFileInfo*) m_lPCCircumrealityFileInfo.Get(0);
      dwList = m_lPCCircumrealityFileInfo.Num();
      continue;
   }

   // delete everything in the fFound list
   DWORD i;
   PCCircumrealityFileInfo *ppfi = (PCCircumrealityFileInfo*) lFound.Get(0);
   for (i = 0; i < lFound.Num(); i++)
      if (ppfi[i])
         delete ppfi[i];


   return TRUE;
}


/******************************************************************************
CCircumrealityDirInfo::FillListBox - Fills a list box with the file names
in CCircumrealityDirInfo. Each item has a name of "f:xxx" where xxx is the file name.
Directories can optionally be filled in using "d:xxx" for the relative directory

inputs
   PCEscPage         pPage - Page containing the list box
   PCWSTR            pszControl - Control name
   PCWSTR            pszDir - Directory to use for enumeration. If NULL then dont fill directory into list box
   LANGID            lid - Preferred language
returns
   DWORD - Number of Circumreality files in the directory
*/
DWORD CCircumrealityDirInfo::FillListBox (PCEscPage pPage, PCWSTR pszControl, PCWSTR pszDir, LANGID lid)
{
   DWORD dwRet = 0;

   PCEscControl pControl = pPage->ControlFind ((PWSTR)pszControl);
   if (!pControl)
      return dwRet;
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   // sort the list of files alphabetically
   CListFixed lSort;
   lSort.Init (sizeof(PCCircumrealityFileInfo), this ? m_lPCCircumrealityFileInfo.Get(0) : NULL,
      this ? m_lPCCircumrealityFileInfo.Num() : 0);
   qsort (lSort.Get(0), lSort.Num(), sizeof(PCCircumrealityFileInfo), PCCircumrealityFileInfoSort);

   // add the names
   MemZero (&gMemTemp);

   DWORD i;
   DWORD dwNum = lSort.Num();
   PCCircumrealityFileInfo *ppfi = (PCCircumrealityFileInfo*) lSort.Get(0);
   WCHAR szTemp[256];
   for (i = 0; i < dwNum; i++) {
      PCCircumrealityFileInfo pfi = ppfi[i];

      MemCat (&gMemTemp, L"<elem name=\"f:");
      MemCatSanitize (&gMemTemp, pfi->m_pszFile);
      MemCat (&gMemTemp, L"\"><small><table border=0 innerlines=0 tbmargin=2 width=100%%>");

      PCResTitleInfo pTI = pfi->m_pTitleInfoSet ? pfi->m_pTitleInfoSet->Find (lid) : NULL;
      if (pTI) {
         MemCat (&gMemTemp, L"<tr><td width=75%><bold><big><big>");
         MemCatSanitize (&gMemTemp, (PWSTR) pTI->m_memName.p);
         MemCat (&gMemTemp,
            L"</big></big></bold></td>"
            L"<td width=25%>");
         MemCat (&gMemTemp,
            pfi->m_fCircumreality ?
               L"<font color=#008000>(No Internet, single player)</font>" :
               L"<font color=#800000>(Internet required, multiple players)</font>"
               );
         MemCat (&gMemTemp,
            L"</td></tr>"
            L"<tr><td width=10%/><td width=90%>");
         MemCatSanitize (&gMemTemp, (PWSTR) pTI->m_memDescShort.p);
         MemCat (&gMemTemp,
            L"<p/></td></tr>");
      }

      if ((pfi->m_ServerLoad.dwConnections != (DWORD)-1) && !pfi->m_fCircumreality) {
         // show number of users
         MemCat (&gMemTemp, L"<tr><td width=10%/><td width=90%%>");
         if (pfi->m_ServerLoad.dwMaxConnections) {
            MemCat (&gMemTemp, L"<bold>");
            MemCat (&gMemTemp, (int)pfi->m_ServerLoad.dwConnections);
            MemCat (&gMemTemp, L"</bold> people playing out of a maximum of <bold>");
            MemCat (&gMemTemp, (int)pfi->m_ServerLoad.dwMaxConnections);
            MemCat (&gMemTemp, L"</bold> players");
         }
         else
            MemCat (&gMemTemp, L"The world couldn't be contacted to retrieve the number of players.");

         MemCat (&gMemTemp, L"</td></tr>");
      }

      MemCat (&gMemTemp, L"<tr><td width=10%/><td width=65%%><bold>");
      MemCatSanitize (&gMemTemp, pfi->m_pszFile);
      MemCat (&gMemTemp, L"</bold></td><td width=25%%>");
      swprintf (szTemp, L"%.2f", (double)(int)(pfi->m_iFileSize / 1000 / 10) / 100.0);
      MemCat (&gMemTemp, szTemp);
      MemCat (&gMemTemp, L" MB</td></tr>");

      MemCat (&gMemTemp, L"</table></small><br/></elem>");

      // another item
      dwRet++;
   } // i

   if (pszDir) {
      // will need directory names
      WIN32_FIND_DATA ffd;
      HANDLE hFind;
      char szSearch[256];
      WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szSearch, sizeof(szSearch), 0 ,0);
      strcat (szSearch, "\\*.*");

      hFind = FindFirstFile(szSearch, &ffd);
      if (hFind != INVALID_HANDLE_VALUE) {
         while (TRUE) {
            // only want directories
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
               goto next;

            // get rid of . and ..
            if (!strcmp(ffd.cFileName, ".") || !strcmp(ffd.cFileName, ".."))
               goto next;

            // convert name to unicode
            MultiByteToWideChar (CP_ACP, 0, ffd.cFileName, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));

            // add
            MemCat (&gMemTemp, L"<elem name=\"d:");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\">");
            MemCat (&gMemTemp, L"<image transparent=true bmpresource=");
            MemCat (&gMemTemp, (int)IDB_FOLDER);
            MemCat (&gMemTemp, L"/><bold><small>");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</small></bold></elem>");

   next:
            if (!FindNextFile (hFind, &ffd))
               break;
         }

         FindClose(hFind);
      }
   } // fDir


   // add this to the list box
   ESCMLISTBOXADD lba;
   memset (&lba, 0, sizeof(lba));
   lba.dwInsertBefore = -1;
   lba.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_LISTBOXADD, &lba);

   // set the selection
   pControl->AttribSetInt (CurSel(), 0);

   return dwRet;
}




/******************************************************************************
CCircumrealityDirCache::Constructor and destructor
*/
CCircumrealityDirCache::CCircumrealityDirCache (void)
{
   m_lPCCircumrealityDirInfo.Init (sizeof(PCCircumrealityDirInfo));
}

CCircumrealityDirCache::~CCircumrealityDirCache (void)
{
   // delete
   DWORD i;
   PCCircumrealityDirInfo *ppwi = (PCCircumrealityDirInfo*) m_lPCCircumrealityDirInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityDirInfo.Num(); i++)
      delete ppwi[i];
   m_lPCCircumrealityDirInfo.Clear();
}

/******************************************************************************
CCircumrealityDirCache::DirGet - Returns a pointer to the PCCircumrealityDirInfo for the given
directory. It synchronizes in the process. If it can't be found in the internal
list then it's created. If it ends up not having any files then NULL is returned

inputs
   PWSTR          pszDir - Directory, like "c:\hello\bye"... not final \
   PCProgressSocket pProgress - Show progress in loading
*/
PCCircumrealityDirInfo CCircumrealityDirCache::DirGet (PWSTR pszDir, PCProgressSocket pProgress)
{
   // see if can find it
   DWORD i;
   PCCircumrealityDirInfo *ppwi = (PCCircumrealityDirInfo*) m_lPCCircumrealityDirInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityDirInfo.Num(); i++)
      if (!_wcsicmp(ppwi[i]->m_szDir, pszDir))
         break;
   
   // pointer
   PCCircumrealityDirInfo pwi;
   if (i < m_lPCCircumrealityDirInfo.Num())
      pwi = ppwi[i];
   else {
      pwi = new CCircumrealityDirInfo;
      if (!pwi)
         return NULL;
      wcscpy (pwi->m_szDir, pszDir);
   }

   // sync
   pwi->SyncWithDirectory (pProgress);

   // if empty then delete and return
   if (!pwi->m_lPCCircumrealityFileInfo.Num()) {
      delete pwi;
      if (i < m_lPCCircumrealityDirInfo.Num())
         m_lPCCircumrealityDirInfo.Remove (i);
      return NULL;
   }

   // may need to add
   if (i >= m_lPCCircumrealityDirInfo.Num()) {
      // addd....

      // dlete and old one
      while (m_lPCCircumrealityDirInfo.Num() > 100) {
         DWORD dwRand = (DWORD)rand() % m_lPCCircumrealityDirInfo.Num();
         delete ppwi[dwRand];
         m_lPCCircumrealityDirInfo.Remove (dwRand);
         ppwi = (PCCircumrealityDirInfo*) m_lPCCircumrealityDirInfo.Get(0);
      } // delete if too many cached

      // add it
      m_lPCCircumrealityDirInfo.Add (&pwi);
   }

   // done
   return pwi;
}

/******************************************************************************
CCircumrealityDirCache::MMLTo - Standard
*/
static PWSTR gpszCircumrealityDirCache = L"CircumrealityDirCache";

PCMMLNode2 CCircumrealityDirCache::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszCircumrealityDirCache);

   // write out entries
   DWORD i;
   PCCircumrealityDirInfo *ppwi = (PCCircumrealityDirInfo*) m_lPCCircumrealityDirInfo.Get(0);
   PCMMLNode2 pSub;
   for (i = 0; i < m_lPCCircumrealityDirInfo.Num(); i++) {
      pSub = ppwi[i]->MMLTo();
      if (!pSub)
         continue;
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/******************************************************************************
CCircumrealityDirCache::MMLFrom -Stanrdar
*/
BOOL CCircumrealityDirCache::MMLFrom (PCMMLNode2 pNode)
{
   // clear the cache...
   DWORD i;
   PCCircumrealityDirInfo *ppwi = (PCCircumrealityDirInfo*) m_lPCCircumrealityDirInfo.Get(0);
   for (i = 0; i < m_lPCCircumrealityDirInfo.Num(); i++)
      delete ppwi[i];
   m_lPCCircumrealityDirInfo.Clear();

   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszCircumrealityDirInfo)) {
         PCCircumrealityDirInfo pwi = new CCircumrealityDirInfo;
         if (!pwi)
            return FALSE;
         if (!pwi->MMLFrom (pSub))
            return FALSE;
         m_lPCCircumrealityDirInfo.Add (&pwi);
      }
   } // i

   return TRUE;

}


/******************************************************************************
CCircumrealityDirCache::Open - reads the Circumreality dir cache from disk.

returns
   BOOL - TRUE if succeess
*/
BOOL CCircumrealityDirCache::Open (void)
{
   // make the name
   // char szFile[256];
   WCHAR szw[MAX_PATH];
   AppDataDirGet (szw);
   // strcpy (szFile, gszAppDir);
   wcscat (szw, L"CircumrealityDirCache.mml");

   // MultiByteToWideChar (CP_ACP, 0, szFile, -1, szw, sizeof(szw)/sizeof(WCHAR));
   PCMMLNode2 pNode = MMLFileOpen (szw, &GUID_CircumrealityCache);
   if (!pNode)
      return FALSE;

   if (!MMLFrom (pNode)) {
      delete pNode;
      return FALSE;
   }

   delete pNode;
   return TRUE;
}

/******************************************************************************
CCircumrealityDirCache::Save - Writes the cache of Circumreality file info out
*/
BOOL CCircumrealityDirCache::Save (void)
{
   // make the name
   //char szFile[256];
   // WCHAR szw[256];
   WCHAR szw[MAX_PATH];
   AppDataDirGet (szw);
   wcscat (szw, L"CircumrealityDirCache.mml");
   //strcpy (szFile, gszAppDir);
   //strcat (szFile, "CircumrealityDirCache.mml");
   //MultiByteToWideChar (CP_ACP, 0, szFile, -1, szw, sizeof(szw)/sizeof(WCHAR));

   PCMMLNode2 pNode = MMLTo();
   if (!pNode)
      return FALSE;
   BOOL fRet;
   fRet = MMLFileSave (szw, &GUID_CircumrealityCache, pNode);
   delete pNode;
   return fRet;
}


/*********************************************************************************
CleanDirName - This takes a directory name and a) verifies its valid. If it
isn't then a valid name is put in place. and b) removes any "\" at the end

inputs
   PWSTR       pszDir - Directory to modify
*/
static void CleanDirName (PWSTR pszDir)
{
   DWORD dwLen = (DWORD)wcslen(pszDir);
   if (dwLen && (pszDir[dwLen-1] == '\\'))
      pszDir[dwLen-1] = 0;

   // convert to ANSI
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szTemp, sizeof(szTemp), 0,0);
   if (!PathIsDirectory (szTemp))
      wcscpy (pszDir, L"c:");
}

/*********************************************************************************
CircumrealityOpenPage */

static BOOL CircumrealityOpenPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWOD pw = (PWOD) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"edit");

         // fill the list box
         pPage->Message (ESCM_USER+82);

         // BUGFIX - Move after ESCM_USER+83
         if (pControl && pw->szFile[0])
            pControl->AttribSet (Text(), pw->szFile);
      }
      break;

   case ESCM_USER+82:   // fill the listbox
      {
         // clear the description
         PCEscControl pControl;
         if (pControl = pPage->ControlFind (L"description"))
            pControl->AttribSet (Text(), L"");
         if (pControl = pPage->ControlFind (L"serverload"))
            pControl->Enable (FALSE);

         CleanDirName (pw->szDir);

         // get the directory
         PCCircumrealityDirInfo pwd;
         {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Scanning files...");
            pwd = pw->pCache->DirGet(pw->szDir, &Progress);
         }

         DWORD dwFilled = pwd->FillListBox (pPage, L"list", pw->szDir, pw->lid);

         // enable/disable the serverall button
         if (pControl = pPage->ControlFind (L"serverloadall"))
            pControl->Enable (dwFilled ? TRUE : FALSE);


         // create a list of absolute paths...
         CListVariable lPath;
         char szaTemp[256];
         WideCharToMultiByte (CP_ACP, 0, pw->szDir, -1, szaTemp, sizeof(szaTemp), 0, 0);
         char *pc, *pLast;
         pc = szaTemp + strlen(szaTemp);
         pc[0] = '\\';
         pc[1] = 0;
         lPath.Add (szaTemp, strlen(szaTemp)+1);
         pc[0] = 0;

         while (TRUE) {
            // find th elast backslash and cut off
            pLast = NULL;
            for (pc = szaTemp; *pc; pc++)
               if (pc[0] == '\\')
                  pLast = pc;
            if (!pLast)
               break;

            // wipe out
            pLast[1] = 0;

            // make sure valid directory
            if (!szaTemp[0] || !PathIsDirectory (szaTemp))
               break;

            // if its a root dont add
            if (PathIsRoot(szaTemp) && !PathIsUNC(szaTemp))
               break;

            // else, add
            lPath.Add (szaTemp, strlen(szaTemp)+1);

            pLast[0] = 0;
         }
         // loop through all the the possible roots and add these
         DWORD i;
         for (i = 0; i < 26; i++) {
            szaTemp[0] = 0;
            PathBuildRoot (szaTemp, i);
            if (!szaTemp[0])
               continue;
            if (GetDriveType (szaTemp) == DRIVE_NO_ROOT_DIR)
               continue;

            // if it's the same as the first entry then skip
            if (lPath.Num() && !_stricmp(szaTemp, (char*)lPath.Get(0)))
               continue;

            lPath.Add (szaTemp, strlen(szaTemp)+1);
         }

         // add these
         MemZero (&gMemTemp);
         WCHAR szTemp[256];
         for (i = 0; i < lPath.Num(); i++) {
            char *psz = (char*)lPath.Get(i);
            MultiByteToWideChar (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));

            MemCat (&gMemTemp, L"<elem name=\"a:");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\"><bold>");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</bold></elem>");
         }


         // add this to the combo box
         // PCEscControl pControl;
         pControl = pPage->ControlFind (L"dir");
         if (!pControl)
            return TRUE;

         ESCMCOMBOBOXADD cba;
         memset (&cba, 0, sizeof(cba));
         cba.dwInsertBefore = 0; // to add
         cba.pszMML = (PWSTR) gMemTemp.p;
         pControl->Message (ESCM_COMBOBOXRESETCONTENT);
         pControl->Message (ESCM_COMBOBOXADD, &cba);

         // set the selection
         pControl->AttribSetInt (CurSel(), 0);

         // get the 0th item
         pControl = pPage->ControlFind (L"list");
         ESCMLISTBOXGETITEM gi;
         memset (&gi, 0, sizeof(gi));
         gi.dwIndex = 0;
         if (!pControl || !pControl->Message (ESCM_LISTBOXGETITEM, &gi))
            gi.pszName = NULL;

         // if not "f:" then nothing
         PWSTR pszFile = gi.pszName;
         PWSTR pszTextToUse = L"";
         if (pszFile && (pszFile[0] == L'f') && (pszFile[1] == L':'))
            pszTextToUse = pszFile + 2;

         // clear edit
         pControl = pPage->ControlFind (L"edit");
         if (pControl)
            pControl->AttribSet (Text(), pszTextToUse);

         // update the file description
         pPage->Message (ESCM_USER+88);
      }
      return TRUE;

   case ESCM_USER+88:   // fill in the description
      {
         PCEscControl pDesc = pPage->ControlFind (L"description");
         if (!pDesc)
            return TRUE;
         pDesc->AttribSet (Text(), L"");  // to clear

         PCEscControl pServerLoad = pPage->ControlFind (L"serverload");
         if (pServerLoad)
            pServerLoad->Enable (FALSE);

         PCEscControl pControl = pPage->ControlFind (L"list");
         if (!pControl)
            return TRUE;

         int iSel = pControl->AttribGetInt (CurSel());
         if (iSel < 0)
            return TRUE;   // nothing

         // get the item
         ESCMLISTBOXGETITEM gi;
         memset (&gi, 0, sizeof(gi));
         gi.dwIndex = (DWORD)iSel;
         if (!pControl->Message (ESCM_LISTBOXGETITEM, &gi))
            return TRUE;   // cant get

         // if not "f:" then nothing
         PWSTR pszFile = gi.pszName;
         if (!pszFile)
            return TRUE;
         if ((pszFile[0] != L'f')|| (pszFile[1] != L':'))
            return TRUE;
         pszFile += 2;

         // find the entry
         // get the directory
         PCCircumrealityDirInfo pwd;
         PCResTitleInfo pti = NULL;
         PCCircumrealityFileInfo pCFI = NULL;
         DWORD i;
         {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Scanning files...");
            pwd = pw->pCache->DirGet(pw->szDir, &Progress);
         }
         if (pwd) {
            PCCircumrealityFileInfo *ppFI = (PCCircumrealityFileInfo*)pwd->m_lPCCircumrealityFileInfo.Get(0);
            for (i = 0; i < pwd->m_lPCCircumrealityFileInfo.Num(); i++) {
               PCCircumrealityFileInfo pCur = ppFI[i];
               if (!_wcsicmp(pCur->m_pszFile, pszFile)) {
                  pti = pCur->m_pTitleInfoSet->Find (pw->lid);
                  pCFI = pCur;
                  break;
               }
            } // i
         }

         MemZero (&gMemTemp);
         if (pti) {
            MemCat (&gMemTemp, (PWSTR) pti->m_memName.p);
            MemCat (&gMemTemp, L"\r\n\r\n");
            MemCat (&gMemTemp, (PWSTR) pti->m_memDescLong.p);

            if (!pCFI->m_fCircumreality && pServerLoad)
               pServerLoad->Enable (TRUE);
         }

         // set the description
         pDesc->AttribSet (Text(), (PWSTR)gMemTemp.p);
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         // only care about list
         if (!psz || _wcsicmp(psz, L"dir"))
            break;
         if (!p->pszName)
            return TRUE;

         // if item 0 ignore since just set that
         if (p->dwCurSel == 0)
            break;

         if ((p->pszName[0] == L'a') && (p->pszName[1] == L':')) {
            wcscpy (pw->szDir, p->pszName+2);

            // fill the list box
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;

         // only care about list
         if (!psz || _wcsicmp(psz, L"list"))
            break;
         if (!p->pszName)
            return TRUE;

         // if click on file the play that file
         if ((p->pszName[0] == L'f') && (p->pszName[1] == L':')) {
            //char szFile[512];
            //WideCharToMultiByte (CP_ACP, 0, pw->szDir, -1, szFile, sizeof(szFile), 0 ,0);
            //strcat (szFile, "\\");
            //WideCharToMultiByte (CP_ACP, 0, p->pszName + 2, -1, szFile + strlen(szFile), sizeof(szFile) - strlen(szFile), 0 ,0);

            // play
            //sndPlaySound (NULL, 0);
            //sndPlaySound (szFile, SND_ASYNC | SND_NODEFAULT);
            
            // set the text
            PWSTR pszFile = p->pszName+2;
            PCEscControl pControl = pPage->ControlFind (L"edit");
            if (pControl)
               pControl->AttribSet (Text(), pszFile);


            // update the file description
            pPage->Message (ESCM_USER+88);

            return TRUE;
         }
         if ((p->pszName[0] == L'd') && (p->pszName[1] == L':')) {
            DWORD dwLen = (DWORD)wcslen(pw->szDir) + (DWORD)wcslen(p->pszName+2) + 2;
            if (dwLen >= sizeof(pw->szDir)/sizeof(WCHAR))
               return FALSE;

            // else, new dir
            wcscat (pw->szDir, L"\\");
            wcscat (pw->szDir, p->pszName+2);

            // fill the list box
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"ok")) {
            // get the text
            WCHAR szText[256];
            DWORD dwNeed;
            PCEscControl pControl = pPage->ControlFind (L"edit");
            if (!pControl)
               return FALSE;
            szText[0] = 0;
            pControl->AttribGet (Text(), szText, sizeof(szText), &dwNeed);

            // see if typed in a directory...
            char szTemp[512];
            WideCharToMultiByte (CP_ACP, 0, szText, -1, szTemp, sizeof(szTemp), 0,0);
            if (PathIsDirectory (szTemp)) {
               wcscpy (pw->szDir, szText);
               pPage->Message (ESCM_USER+82);
               return TRUE;
            }


            // see if it's an extension onto existing directory
            WideCharToMultiByte (CP_ACP, 0, pw->szDir, -1, szTemp, sizeof(szTemp), 0,0);
            strcat (szTemp, "\\");
            WideCharToMultiByte (CP_ACP, 0, szText, -1, szTemp + strlen(szTemp), sizeof(szTemp) - (DWORD)strlen(szTemp), 0,0);
            DWORD dwLen;
            if (PathIsDirectory (szTemp)) {
               dwLen = (DWORD)wcslen(pw->szDir) + (DWORD)wcslen(szText) + 2;
               if (dwLen * sizeof(WCHAR) < sizeof(pw->szDir)) {
                  wcscat (pw->szDir, L"\\");
                  wcscat (pw->szDir, szText);
               }
               pPage->Message (ESCM_USER+82);
               return TRUE;
            }

            PCResTitleInfoSet pOpen = NULL;
            DWORD dwTry;
            for (dwTry = 0; !pOpen && (dwTry < 3); dwTry++) {
               dwLen = (DWORD)wcslen(pw->szDir) + (DWORD)wcslen(szText) + 7;
               if (dwLen * sizeof(WCHAR) >= sizeof(pw->szFile)) {
                  pPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
                  return TRUE;
               }
               wcscpy (pw->szFile, pw->szDir);
               wcscat (pw->szFile, L"\\");
               wcscat (pw->szFile, szText);

               if (dwTry) {
                  PWSTR pszAppend = (dwTry == 1) ? L".crk" : L".crf";
                  // try appending .crf or .crk to it...
                  dwLen = (DWORD)wcslen(pw->szFile);
                  if ((dwLen < 4) || _wcsicmp(szText+(dwLen-wcslen(pszAppend)), pszAppend))
                     wcscat (pw->szFile, pszAppend);
               }

               WideCharToMultiByte (CP_ACP, 0, pw->szFile, -1, szTemp, sizeof(szTemp), 0,0);

               // try opening this
               BOOL fCircumreality;
               pOpen = PCResTitleInfoSetFromFile (pw->szFile, &fCircumreality);
            }
            if (pOpen)
               delete pOpen;

            if (pOpen) {
               if (pw->fSave) {
                  int iRet;
                  iRet = pPage->MBYesNo (L"Do you want to overwrite the existing file?",
                     L"The file already exists.");
                  if (iRet != IDYES)
                     return TRUE;
               }

               pPage->Exit (L"ok");
               return TRUE;
            }

            // if saving then accept the name
            if (pw->fSave) {
               // see if can save something
               OUTPUTDEBUGFILE (szTemp);
               FILE *f = fopen(szTemp, "wb");
               if (!f) {
                  pPage->MBWarning (L"The file couldn't be saved.",
                     L"You may not have typed in a correct name for the file.");
                  return TRUE;
               }
               else {
                  fclose (f);
                  DeleteFile (szTemp);
               }
               pPage->Exit (L"ok");
               return TRUE;
            }

            // else dont know
            pPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"serverloadall")) {
            PCCircumrealityDirInfo pwd;
            PCResTitleInfo pti = NULL;
            PCCircumrealityFileInfo pCFI = NULL;
            DWORD i, j;
            {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Scanning files...");
               pwd = pw->pCache->DirGet(pw->szDir, &Progress);
            }
            if (!pwd)
               return TRUE;   // shouldnt happen

            // list of files
            SERVERLOADQUERY slq;
            memset (&slq, 0, sizeof(slq));
            CListFixed lSLQ;
            lSLQ.Init (sizeof(SERVERLOADQUERY));
            
            PCCircumrealityFileInfo *ppFI = (PCCircumrealityFileInfo*)pwd->m_lPCCircumrealityFileInfo.Get(0);
            for (i = 0; i < pwd->m_lPCCircumrealityFileInfo.Num(); i++) {
               PCCircumrealityFileInfo pCFI = ppFI[i];
               PCResTitleInfo pti = pCFI->m_pTitleInfoSet->Find (pw->lid);
               if (!pti)
                  continue;

               if (pCFI->m_fCircumreality)
                  continue;   // not a link

               memset (&pCFI->m_ServerLoad, 0, sizeof(pCFI->m_ServerLoad));

               for (j = 0; j < pti->m_lPCResTitleInfoShard.Num(); j++) {
                  PCResTitleInfoShard *ppis = (PCResTitleInfoShard*)pti->m_lPCResTitleInfoShard.Get(j);
                  PCResTitleInfoShard pis = ppis[0];

                  PWSTR pszDomain = (PWSTR)pis->m_memAddr.p;
                  if (wcslen(pszDomain)+1 >= sizeof(slq.szDomain)/sizeof(WCHAR))
                     continue;   // name too long
                  wcscpy (slq.szDomain, pszDomain);
                  slq.dwPort = pis->m_dwPort;
                  slq.pUserData = pCFI;
                  lSLQ.Add (&slq);
               }
            } // i

            ServerLoadQuery ((PSERVERLOADQUERY) lSLQ.Get(0), lSLQ.Num(), pPage->m_pWindow->m_hWnd);

            // write all the numbers in place
            PSERVERLOADQUERY pslq = (PSERVERLOADQUERY)lSLQ.Get(0);
            CIRCUMREALITYSERVERLOAD ServerLoad;
            memset (&ServerLoad, 0, sizeof(ServerLoad));
            for (i = 0; i < lSLQ.Num(); i++, pslq++) {
               PCCircumrealityFileInfo pCFI = (PCCircumrealityFileInfo) pslq->pUserData;

               pCFI->m_ServerLoad.dwConnections += pslq->ServerLoad.dwConnections;
               pCFI->m_ServerLoad.dwMaxConnections += pslq->ServerLoad.dwMaxConnections;
            }

            // update the list
            pPage->Message (ESCM_USER+82);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"serverload")) {
            PCEscControl pControl = pPage->ControlFind (L"list");
            if (!pControl)
               return TRUE;

            // NOTE: Dont report some errors because button should be disabled

            int iSel = pControl->AttribGetInt (CurSel());
            if (iSel < 0)
               return TRUE;   // nothing

            // get the item
            ESCMLISTBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = (DWORD)iSel;
            if (!pControl->Message (ESCM_LISTBOXGETITEM, &gi))
               return TRUE;   // cant get

            // if not "f:" then nothing
            PWSTR pszFile = gi.pszName;
            if (!pszFile)
               return TRUE;
            if ((pszFile[0] != L'f')|| (pszFile[1] != L':'))
               return TRUE;
            pszFile += 2;

            // find the entry
            // get the directory
            PCCircumrealityDirInfo pwd;
            PCResTitleInfo pti = NULL;
            PCCircumrealityFileInfo pCFI = NULL;
            DWORD i;
            {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Scanning files...");
               pwd = pw->pCache->DirGet(pw->szDir, &Progress);
            }
            if (pwd) {
               PCCircumrealityFileInfo *ppFI = (PCCircumrealityFileInfo*)pwd->m_lPCCircumrealityFileInfo.Get(0);
               for (i = 0; i < pwd->m_lPCCircumrealityFileInfo.Num(); i++) {
                  PCCircumrealityFileInfo pCur = ppFI[i];
                  if (!_wcsicmp(pCur->m_pszFile, pszFile)) {
                     pti = pCur->m_pTitleInfoSet->Find (pw->lid);
                     pCFI = pCur;
                     break;
                  }
               } // i
            }
            if (!pti || pCFI->m_fCircumreality )
               return TRUE;

            // create a list of all the relevent info
            SERVERLOADQUERY slq;
            memset (&slq, 0, sizeof(slq));
            CListFixed lSLQ;
            lSLQ.Init (sizeof(SERVERLOADQUERY));
            for (i = 0; i < pti->m_lPCResTitleInfoShard.Num(); i++) {
               PCResTitleInfoShard *ppis = (PCResTitleInfoShard*)pti->m_lPCResTitleInfoShard.Get(i);
               PCResTitleInfoShard pis = ppis[0];

               PWSTR pszDomain = (PWSTR)pis->m_memAddr.p;
               if (wcslen(pszDomain)+1 >= sizeof(slq.szDomain)/sizeof(WCHAR))
                  continue;   // name too long
               wcscpy (slq.szDomain, pszDomain);
               slq.dwPort = pis->m_dwPort;
               lSLQ.Add (&slq);
            }
            ServerLoadQuery ((PSERVERLOADQUERY) lSLQ.Get(0), lSLQ.Num(), pPage->m_pWindow->m_hWnd);

            // sum all the numbers up
            PSERVERLOADQUERY pslq = (PSERVERLOADQUERY)lSLQ.Get(0);
            CIRCUMREALITYSERVERLOAD ServerLoad;
            memset (&ServerLoad, 0, sizeof(ServerLoad));
            for (i = 0; i < lSLQ.Num(); i++, pslq++) {
               ServerLoad.dwConnections += pslq->ServerLoad.dwConnections;
               ServerLoad.dwMaxConnections += pslq->ServerLoad.dwMaxConnections;
            }

            WCHAR szTemp[256];
            if (ServerLoad.dwMaxConnections)
               swprintf (szTemp, L"The world has %d players out of a maximum of %d.",
                  (int) ServerLoad.dwConnections, (int) ServerLoad.dwMaxConnections);
            else
               wcscpy (szTemp, L"The world couldn't be contacted to retrieve the number of players.");
            pPage->MBInformation (szTemp);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Circumreality file open";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OPENSAVE")) {
            p->pszSubString = pw->fSave ? L"<font color=#ff8080>Save</font>" : L"Play";
            return TRUE;
         }
      }
      break;

   }

   return FALSE;
   // BUGFIX - Dont use DefPage (pPage, dwMessage, pParam); because trps enter
}



/*********************************************************************************
CircumrealityFileIsDefault - Tests to see if there's only one file in the current
working directory. If there is, then that file is returned. Otherwise, FALSE
is returned.

inputs
   HWND              hWnd - To display progress off of
   PWSTR             pszFile - Filled with the file name if the user selects open
                     Must be 256 chars long. Iniitally, this is filled in with a
                     file name. If saving then the directory and text will come from
                     the original file name
returns
   BOOL - TRUE if single file was found, FALSE if not
*/
BOOL CircumrealityFileIsDefault (HWND hWnd, PWSTR pszFile)
{
   BOOL fRet = FALSE;
   CCircumrealityDirCache DirCache;
   DirCache.Open();

   // look in the directory and get the current working dir...
   char szTemp[256];
   WCHAR szwTemp[256];
   GetLastDirectory (szTemp, sizeof(szTemp));
   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, sizeof(szwTemp)/sizeof(WCHAR));

   // get the directory
   PCCircumrealityDirInfo pwd;
   CProgress Progress;
   Progress.Start (hWnd, "Scanning files...");
   pwd = DirCache.DirGet(szwTemp, &Progress);
   if (!pwd)
      goto done;

   if (pwd->m_lPCCircumrealityFileInfo.Num() != 1)
      goto done;  // more or less than one file

   // else, only one file, so use that
   PCCircumrealityFileInfo pFI = *((PCCircumrealityFileInfo*)pwd->m_lPCCircumrealityFileInfo.Get(0));
   wcscpy (pszFile, szwTemp);
   DWORD dwLen = (DWORD)wcslen(pszFile);
   if (dwLen && (pszFile[dwLen-1] != L'\\'))
      wcscat (pszFile, L"\\");
   wcscat (pszFile, pFI->m_pszFile);
   fRet = TRUE;


done:
   // save cahce
   DirCache.Save();

   return fRet;
}
   

/*********************************************************************************
CircumrealityFileOpen - This opens a Circumreality file.

inputs
   PCEscWindow       pWindow - Window to use
   BOOL              fSave - If TRUE then saving a file, else opening.
   LANGID            lid - Preferred language to use
   PWSTR             pszFile - Filled with the file name if the user selects open
                     Must be 256 chars long. Iniitally, this is filled in with a
                     file name. If saving then the directory and text will come from
                     the original file name
   BOOL              fSkipIfOnlyOneFile - If there's only one file in the directory,
                     then that one is automatically selected.
returns
   BOOL - TRUE if open, FALSE if not
*/
BOOL CircumrealityFileOpen (PCEscWindow pWindow, BOOL fSave, LANGID lid, PWSTR pszFile, BOOL fSkipIfOnlyOneFile)
{
   // if there is only one file then use that
   if (!fSave && fSkipIfOnlyOneFile && CircumrealityFileIsDefault (pWindow->m_hWnd, pszFile))
      return TRUE;

   // load in the directory cache
   WOD wod;
   memset (&wod, 0, sizeof(wod));
   CCircumrealityDirCache DirCache;
   DirCache.Open();
   wod.pCache = &DirCache;
   wod.fSave = fSave;
   wod.lid = lid;

   // look in the directory and get the current working dir...
   // BUGFIX - Always start in application directory so users can't get lost
   char szTemp[256];
   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, wod.szDir, sizeof(wod.szDir)/sizeof(WCHAR)); // NOTE: OK to start in app dir
   //GetLastDirectory (szTemp, sizeof(szTemp));
   //MultiByteToWideChar (CP_ACP, 0, szTemp, -1, wod.szDir, sizeof(wod.szDir)/sizeof(WCHAR));

   // if saving or opening then get info from name
   if (pszFile[0]) {
      // find the last slash...
      WCHAR *pc, *pcLast;
      pcLast = pszFile;
      for (pc = pszFile; *pc; pc++)
         if (pc[0] == L'\\')
            pcLast = pc;
      wcscpy (wod.szFile, pcLast+1);

      if (pcLast != pszFile) {
         pcLast[0] = 0;
         wcscpy (wod.szDir, pszFile);
      }
   }

   // display the window
   // CEscWindow cWindow;
   PWSTR pszRet;
   BOOL fOpen = FALSE;
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLMIFOPEN, CircumrealityOpenPage, &wod);
   if (pszRet && !_wcsicmp(pszRet, L"ok")) {
      fOpen = TRUE;
      wcscpy (pszFile, wod.szFile);
   }

   // set directory
   WideCharToMultiByte (CP_ACP, 0, wod.szDir, -1, szTemp, sizeof(szTemp), 0, 0);
   SetLastDirectory (szTemp);

   // save cahce
   DirCache.Save();

   return fOpen;
}



/*********************************************************************************
CircumrealityFileOpen - This opens a Circumreality file.

inputs
   HWND              hWnd - Window to display from
   BOOL              fSave - If TRUE then saving a file, else opening.
   LANGID            lid - Preferred language to use
   PWSTR             pszFile - Filled with the file name if the user selects open
                     Must be 256 chars long. Iniitally, this is filled in with a
                     file name. If saving then the directory and text will come from
                     the original file name
   BOOL              fSkipIfOnlyOneFile - If there's only one file in the directory,
                     then that one is automatically selected.
returns
   BOOL - TRUE if open, FALSE if not
*/
BOOL CircumrealityFileOpen (HWND hWnd, BOOL fSave, LANGID lid, PWSTR pszFile, BOOL fSkipIfOnlyOneFile)
{
   // display the window
   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation2 (hWnd, &r);
   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   EscWindowFontScaleByScreenSize (&cWindow);
   return CircumrealityFileOpen (&cWindow, fSave, lid, pszFile, fSkipIfOnlyOneFile);
}




