/*************************************************************************************
CInstance.cpp - Code for the database object.

begun 21/1/06 by Mike Rozak.
Copyright 2006 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"



static PWSTR gpszSuffix = L".msg";

/*************************************************************************************
CInstanceFile::Constructor and destructor
*/
CInstanceFile::CInstanceFile (void)
{
   m_szName[0] = 0;
}

CInstanceFile::~CInstanceFile (void)
{
   // nothing for now
}


/*************************************************************************************
CInstanceFile::Init - Initializes the instance file to use the given name (stored
in m_szName, and file.

inputs
   PWSTR          pszName - Name that's referenced
   PWSTR          pszFile - File
*/
BOOL CInstanceFile::Init (PWSTR pszName, PWSTR pszFile)
{
   if (m_szName[0])
      return FALSE;

   if (!m_MegaFileInThread.Init (pszFile, &GUID_InstanceFile, TRUE, 10))
      return FALSE;

   wcscpy (m_szName, pszName);

   return TRUE;
}


/*************************************************************************************
CInstanceFile::Save - Saves the information to the sub-file.

inputs
   PCMIFLVM    pVM - Virtual machine
   PWSTR       pszSubFile - Subfile
   BOOL        fSaveAll - if TRUE then save all objects BUT exclude the objects in the list,
            else if FALSE then include only those objects in the list
   BOOL        fChildren - if TRUE then include children of the object (for inclusion or exclusion)
   GUID        *pagList - Objects on the list
   DWORD       dwNum - Number of objects in the list
returns
   BOOL - TRUE if success
*/
BOOL CInstanceFile::Save (PCMIFLVM pVM, PWSTR pszSubFile, BOOL fSaveAll, BOOL fChildren,
                          GUID *pagList, DWORD dwNum)
{
   // MML to
   PCMMLNode2 pNode = pVM->MMLTo (fSaveAll, fSaveAll, pagList, dwNum, fChildren);
   if (!pNode)
      return FALSE; // error

   // save to MIFL
   if (!m_MegaFileInThread.SaveMML (pszSubFile, pNode, TRUE))
      return FALSE;

   return TRUE;
}



/*************************************************************************************
CInstanceFile::Load - Loads the saved game from a file.

inputs
   PCMIFLVM    pVM - Virtual machine
   PWSTR       pszSubFile - Subfile
   BOOL        fRemap - If TRUE then remap any objects that already exist to new object IDs.
                  If FALSE, replace existing objects.
   PCHashGUID  *pphRemap - If fRemap is TRUE, this will be filled with a list of GUIDs
                  that are remapped.
returns
   BOOL - TRUE if success
*/
BOOL CInstanceFile::Load (PCMIFLVM pVM, PWSTR pszSubFile, BOOL fRemap, PCHashGUID *pphRemap)
{
   if (pphRemap)
      *pphRemap = NULL;

   PCMMLNode2 pNode = m_MegaFileInThread.LoadMML (pszSubFile, TRUE);
   if (!pNode)
      return FALSE;

   if (!pVM->MMLFrom (FALSE, FALSE, fRemap, FALSE, pNode, fRemap ? pphRemap : NULL)) {
      delete pNode;
      return FALSE;
   }
   delete pNode;

   return TRUE;
}


/*************************************************************************************
CInstanceFile::Num - Returns the number of files written (although this number
may change due to the asynchronous writes)
*/
DWORD CInstanceFile::Num (void)
{
   return m_MegaFileInThread.Num();
}



/*************************************************************************************
CInstanceFile::GetNum - Fills pMem in with the filename of the file at the given
index. Because of asynchronous writes, this info may change

inputs
   DWORD          dwIndex - From 0..Num()-1
   PCMem          pMem - Filled with the string of the sub-file name
returns
   BOOL - TRUE if success
*/
BOOL CInstanceFile::GetNum (DWORD dwIndex, PCMem pMem)
{
   return m_MegaFileInThread.GetNum(dwIndex, pMem);
}


/*************************************************************************************
CInstanceFile::Enum - Enumerates all the files.

inputs
   PCListVariable       plSubFiles - Filled with the subfiles
   PCListFixed          plMFFILEINFO - Filled with the file info. This can be NULL
returns
   BOOL - TRUE if success
*/
BOOL CInstanceFile::Enum (PCListVariable plSubFiles, PCListFixed plMFFILEINFO)
{
   return m_MegaFileInThread.Enum (plSubFiles, plMFFILEINFO);
}


/*************************************************************************************
CInstanceFile::Delete - Deletes a subfile. This is an asynchronous operation.

inputs
   PWSTR             pszSubFile - Sub-file to delete
returns
   BOOL - TRUE if succes
*/
BOOL CInstanceFile::Delete (PWSTR pszSubFile)
{
   return m_MegaFileInThread.Delete (pszSubFile);
}


/*************************************************************************************
CInstanceFile::Exists - Tests to see if a subfile exists (although may be off due
to the asychnoronous nature).

inputs
   PWSTR             pszFile - Sub-file
   PMFFILEINFO       pInfo - Filled with the file information. Optional
returns
   BOOL - TRUE if it exists
*/
BOOL CInstanceFile::Exists (PWSTR pszFile, PMFFILEINFO pInfo)
{
   return m_MegaFileInThread.Exists (pszFile, pInfo);
}





/*************************************************************************************
CInstance::Constructor and destructor
*/
CInstance::CInstance (void)
{
   m_szPath[0] = 0;
   m_hCached.Init (sizeof(PCInstanceFile));
   m_hFilesInDir.Init (0);
}

CInstance::~CInstance (void)
{
   DWORD i;
   for (i = 0; i < m_hCached.Num(); i++) {
      PCInstanceFile pif = *((PCInstanceFile*)m_hCached.Get (i));
      delete pif;
   } // i
}


/*************************************************************************************
CInstance::Init - Initializes to the given path.

inputs
   PWSTR          pszPath - Path where the database files are saved. This
                  does NOT include the sub-directory where the saved-games go.
returns
   BOOL - TRUE if success
*/
BOOL CInstance::Init (PWSTR pszPath)
{
   if (m_szPath[0])
      return FALSE;

   wcscpy (m_szPath, pszPath);
   DWORD dwLen = (DWORD)wcslen(m_szPath);
   if (dwLen && (m_szPath[dwLen-1] != L'\\'))
      wcscat (m_szPath, L"\\");
   wcscat (m_szPath, L"SavedGames");

   // create just in case
   CreateDirectoryW (m_szPath, NULL);

   // enumerate all the files in the saved games directory
   WCHAR szSearch[256];
   DWORD dwSuffixLen = (DWORD)wcslen(gpszSuffix);
   wcscpy (szSearch, m_szPath);
   wcscat (szSearch, L"\\*");
   wcscat (szSearch, gpszSuffix);

   // enumerate contents
   WIN32_FIND_DATAW fd;
   HANDLE hFile = FindFirstFileW (szSearch, &fd);
   if (hFile == INVALID_HANDLE_VALUE)
      goto done;
   TEXTLOGFILE tlf;
   memset (&tlf, 0, sizeof(tlf));

   while (TRUE) {
      dwLen = (DWORD)wcslen(fd.cFileName);
      if (dwLen <= dwSuffixLen)
         continue;

      WCHAR cTemp = fd.cFileName[dwLen-dwSuffixLen];
      fd.cFileName[dwLen-dwSuffixLen] = 0;
      m_hFilesInDir.Add (fd.cFileName, NULL);
      fd.cFileName[dwLen-dwSuffixLen] = cTemp;  // restore

      // get the text one
      if (!FindNextFileW (hFile, &fd))
         break;
   }
   FindClose (hFile);

   // done
done:
   return TRUE;
}



/*************************************************************************************
CInstance::Num - Returns the number of files for saved games.
*/
DWORD CInstance::Num (void)
{
   return m_hFilesInDir.Num();
}


/*************************************************************************************
CInstance::GetNum - Returns the name of the Nth index.

inputs
   DWORD          dwIndex - From 0..Num()-1
   PCMem          pMem - Filled with the name, if success
returns
   BOOL - TRUE if success
*/
BOOL CInstance::GetNum (DWORD dwIndex, PCMem pMem)
{
   PWSTR psz = m_hFilesInDir.GetString (dwIndex);
   if (!psz)
      return FALSE;

   MemZero (pMem);
   MemCat (pMem, psz);

   return TRUE;
}


/*************************************************************************************
CInstance::Enum - Returns a list of all the files

inputs
   PCListVariable       plFiles - List of files that is added to
returns
   BOOL - TRUE if usccess
*/
BOOL CInstance::Enum (PCListVariable plFiles)
{
   DWORD i;
   PWSTR psz;
   for (i = 0; i < m_hFilesInDir.Num(); i++) {
      psz = m_hFilesInDir.GetString (i);
      if (!psz)
         continue;

      plFiles->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   }

   return TRUE;
}


/*************************************************************************************
CInstance::Delete - Deletes a given file.

inputs
   PWSTR          pszFile - file name
returns
   BOOL - TRUE if deleted
*/
BOOL CInstance::Delete (PWSTR pszFile)
{
   // if if in the cached list
   PCInstanceFile *ppif = (PCInstanceFile*) m_hCached.Find (pszFile);
   if (ppif) {
      delete *ppif;
      m_hCached.Remove (m_hCached.FindIndex (pszFile));
   }

   // see if it's in the main list
   DWORD dwIndex = m_hFilesInDir.FindIndex (pszFile);
   if (dwIndex == (DWORD)-1)
      return FALSE;
   m_hFilesInDir.Remove (dwIndex);  // delete

   // delete the actual file
   WCHAR szTemp[256];
   wcscpy (szTemp, m_szPath);
   wcscat (szTemp, L"\\");
   wcscat (szTemp, pszFile);
   wcscat (szTemp, gpszSuffix);

   DeleteFileW (szTemp);

   return TRUE;
}



/*************************************************************************************
CInstance::InstanceFileGet - Like other instancefileget except takes PCMIFLVar

inputs
   PCMIFLVM       pVM - Virtual machine
   PCMIFLVar      pvFile - File name
   BOOL           fCreateIfNotExist
returns
   PCInstanceFile - File
*/
PCInstanceFile CInstance::InstanceFileGet (PCMIFLVM pVM, PCMIFLVar pvFile, BOOL fCreateIfNotExist)
{
   PCMIFLVarString ps = pvFile->GetString(pVM);
   if (!ps)
      return NULL;

   PCInstanceFile pif = InstanceFileGet (ps->Get(), fCreateIfNotExist);

   ps->Release();

   return pif;
}


/*************************************************************************************
CInstance::InstanceFileGet - Gets the file. If it doesn't exist then it's created.

inputs
   PWSTR          pszFile - Instance file name. Must be relatively short and not contain
                     bad file naming conventions.
   BOOL           fCreateIfNotExist - If TRUE then create if doesn't exist.
returns
   PCInstanceFile - Use this instance file until the next call to CInstance, which
      might invalidate it.
*/
PCInstanceFile CInstance::InstanceFileGet (PWSTR pszFile, BOOL fCreateIfNotExist)
{
   // see if already on the cached list
   PCInstanceFile *ppif = (PCInstanceFile*) m_hCached.Find (pszFile);
   if (ppif)
      return *ppif;

   // else, see if a valid file
   DWORD dwIndex = m_hFilesInDir.FindIndex (pszFile);
   if (dwIndex == (DWORD)-1) {
      // doesn't exist

      // else, not supposed to exist already, so fail if dont have create flag set
      if (!fCreateIfNotExist)
         return FALSE;

      // if the name is too long then error
      if ((wcslen(pszFile) > 64) || !pszFile[0])
         return FALSE;

      // fall through
   }

   // want to cache it
   LimitCacheSize ();
   PCInstanceFile pif = new CInstanceFile;
   if (!pif)
      return NULL;

   // create the filename
   WCHAR szTemp[256];
   wcscpy (szTemp, m_szPath);
   wcscat (szTemp, L"\\");
   wcscat (szTemp, pszFile);
   wcscat (szTemp, gpszSuffix);

   if (!pif->Init (pszFile, szTemp)) {
      delete pif;
      return NULL;
   }

   // add
   m_hCached.Add (pszFile, &pif);

   // potentially add to main
   if (dwIndex == (DWORD)-1)
      m_hFilesInDir.Add (pszFile, NULL);

   return pif;
}



/*************************************************************************************
CInstance::LimitCacheSize - Makes sure that not too many files are cached,
since this creates lots of threads and lots of open file handles
*/
void CInstance::LimitCacheSize (void)
{
   DWORD dwNum = m_hCached.Num();
   if (dwNum < 50)
      return;  // no problem

   // else, delete
   DWORD dwIndex = (DWORD)rand() % dwNum;
   // if if in the cached list
   PCInstanceFile *ppif = (PCInstanceFile*) m_hCached.Get (dwIndex);
   if (!ppif)
      return;  // shuldnt happen
   delete *ppif;
   m_hCached.Remove (dwIndex);
}

