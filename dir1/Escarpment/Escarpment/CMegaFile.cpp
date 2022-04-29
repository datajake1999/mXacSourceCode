/*************************************************************************************
CMegaFile.cpp - Code for handling a combined (and large) file

begun 6/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <objbase.h>
#include <crtdbg.h>
#include "escarpment.h"


// MFHEADER - Header that occurs at start of megafile
typedef struct {
   GUID           gID;           // ID specific to a megafile, GUID_MegaFileID
   GUID           gIDSub;        // subtype ID, idenfiies info stored in file
   __int64        iVersion;      // ever time the megafile is modified, this is incremented one
   __int64        iOffsetFirstFile; // offset (form start of file) to the first file, 0 if none
} MFHEADER, *PMFHEADER;




// globals
static PCMegaFile gpMegaFile = NULL;         // megafile to use for MFOpen() calls, etc.
static PVOID gpMegaFileCallbackInfo = NULL; // parameter passed to the megafile callback (if the file doesn't exist)
static PMEGAFILECALLBACK gpMegaFileCallback; // used to call if can't find the file that looking for


/*************************************************************************************
CMegaFile::Constructor and destructor
*/
CMegaFile::CMegaFile (void)
{
   InitializeCriticalSection (&m_CritSec);
   m_iLastVersion = -1;
   m_szFile[0] = 0;
   m_iTotalSize = m_iOffsetFirstFile = 0;
   m_iSizeUsed = 0;
   m_fDontUpdateLastAccess = TRUE;
   m_fAssumeNotShared = TRUE;
}

CMegaFile::~CMegaFile (void)
{
   DeleteCriticalSection (&m_CritSec);
}




/*************************************************************************************
CMegaFile::Init - Call this to initialize the megafile object. It basically ends
up verifying that the file exists and copying the file file name over

inputs
   PWSTR             pszMegaFile - File name
   GIOD              *pgIDSub - Subtype of ID. User specific
   BOOL              fCreateIfNotExist - If it doesn't exist then the file is created
returns
   BOOL - TRUE if success. If the magafile subtypes don't match an error will be returned
*/
BOOL CMegaFile::Init (PWSTR pszMegaFile, const GUID *pgIDSub, BOOL fCreateIfNotExist)
{
   if (m_szFile[0])
      return FALSE;  // already have file

   wcscpy (m_szFile, pszMegaFile);
   m_iLastVersion = -1;
   m_gIDSub = *pgIDSub;

tryopen:
   HANDLE hf = OpenFileInternal (TRUE);
   if (hf != INVALID_HANDLE_VALUE) {
      // exists, and info would have been loaded
      CloseHandle(hf);
      return TRUE;
   }

   // else, doesn't exist...
   if (!fCreateIfNotExist) {
      m_szFile[0] = 0;
      return FALSE;  // doesn't exist
   }
   
   // else, try to create
   char szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, m_szFile, -1, szTemp, sizeof(szTemp), 0, 0);
   hf = CreateFile (szTemp, GENERIC_READ | GENERIC_WRITE, 0, NULL,
      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
      // BUGFIX - Was CREATE_NEW, but need to overwrite exiting
   if (hf == INVALID_HANDLE_VALUE) {
      m_szFile[0] = 0;
      return FALSE;
   }

   // write out the empty header
   MFHEADER hdr;
   memset (&hdr, 0, sizeof(hdr));
   hdr.gID = GUID_MegaFileID;
   hdr.gIDSub = m_gIDSub;
   DWORD dwWritten;
   if (!WriteFile (hf, &hdr, sizeof(hdr), &dwWritten, NULL) || (dwWritten != sizeof(hdr))) {
      CloseHandle (hf);
      DeleteFile (szTemp);
      return FALSE;
   }

   // done
   CloseHandle (hf);

   // re-open
   fCreateIfNotExist = FALSE;
   goto tryopen;
}

/*************************************************************************************
MySetFilePointerEx - So that Escarpment.dll will run on windows 95, 98 ME.
*/
BOOL MySetFilePointerEx(
  HANDLE hFile,                    // handle to file
  LARGE_INTEGER liDistanceToMove,  // bytes to move pointer
  PLARGE_INTEGER lpNewFilePointer, // new file pointer
  DWORD dwMoveMethod               // starting point
)
{
   lpNewFilePointer->HighPart = liDistanceToMove.HighPart;
   DWORD dwRet = SetFilePointer (hFile, liDistanceToMove.LowPart, &liDistanceToMove.HighPart, dwMoveMethod);

   lpNewFilePointer->LowPart = dwRet;

   if ((dwRet == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
      return 0;    // fail

   return 1;
}



/*************************************************************************************
CMegaFile::OpenFileInternal - This opens the file for read/write access.

It:
   1) Tries to open the file
   2) If it fails (because the file is being used by another app) then it reties for a few seconds.
   3) Verifies version numbers. If they're the same then returns
   4) If they're different then reloads EVERYTHING because the file has changed

inputs
   BOOL     fWrite - If TRUE then the file is to be written to, FALSE it's just to be read from

returns
   HANDLE - From CreateFile(), or INVALID_HANDLE_VALUE if can't open
*/
HANDLE CMegaFile::OpenFileInternal (BOOL fWrite)
{
   HANDLE hf;

   // if m_fDontUpdateLastAccess is FALSE then always set fWrite to TRUE
   if (!m_fDontUpdateLastAccess)
      fWrite = TRUE;

   // convert to ANSI
   char szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, m_szFile, -1, szTemp, sizeof(szTemp), 0, 0);

   DWORD i;
   // try 30 times before giving up
   for (i = 0; i < 30; i++) {
      hf = CreateFile (szTemp, GENERIC_READ | (fWrite ? GENERIC_WRITE : 0), 0, NULL,
         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
      if (hf== INVALID_HANDLE_VALUE) {
         DWORD dwErr = GetLastError ();
         if (dwErr != ERROR_SHARING_VIOLATION)
            return INVALID_HANDLE_VALUE;
      }
      else
         break;

      Sleep (100);   // wait since the file may yet be closed
   }
   if (hf == INVALID_HANDLE_VALUE)
      return hf;
   
   // read in the header
   MFHEADER hdr;
   DWORD dwRead;
   if (!ReadFile (hf, &hdr, sizeof(hdr), &dwRead, NULL) || (dwRead != sizeof(hdr))) {
      CloseHandle (hf);
      return INVALID_HANDLE_VALUE;
   }
   if (!IsEqualGUID (hdr.gID, GUID_MegaFileID) || !IsEqualGUID (hdr.gIDSub, m_gIDSub)) {
      CloseHandle (hf);
      return INVALID_HANDLE_VALUE;
   }

   // if version the same then done
   if (hdr.iVersion == m_iLastVersion)
      return hf;

   // else, must clear what have and reaload
   m_tMFFILECACHE.Clear ();
   m_iLastVersion = hdr.iVersion;
   m_iTotalSize = sizeof(hdr);
   m_iOffsetFirstFile = hdr.iOffsetFirstFile;

   // read in all the data
   __int64 iCur = hdr.iOffsetFirstFile;
   MFFILEINFO fi;
   MFFILECACHE fc;
   __int64 iMovedTo;
   CMem  mem;
   m_iSizeUsed = 0;
   while (iCur) {
      if (!MySetFilePointerEx (hf, *((LARGE_INTEGER*)&iCur), (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != iCur)) {
         CloseHandle (hf);
         return INVALID_HANDLE_VALUE;
      }

      // read in the info
      if (!ReadFile (hf, &fi, sizeof(fi), &dwRead, NULL) || (dwRead != sizeof(fi))) {
         CloseHandle (hf);
         return INVALID_HANDLE_VALUE;
      }

      // get the file name
      __int64 iSize = fi.iOffsetData - iCur - sizeof(fi);
      if (iSize < sizeof(WCHAR)) {
         CloseHandle (hf);
         return INVALID_HANDLE_VALUE;
      }
      if (!mem.Required ((DWORD)iSize)) {
         CloseHandle (hf);
         return INVALID_HANDLE_VALUE;
      }
      if (!ReadFile (hf, mem.p, (DWORD)iSize, &dwRead, NULL) || (dwRead != (DWORD)iSize)) {
         CloseHandle (hf);
         return INVALID_HANDLE_VALUE;
      }
      PWSTR pszName = (PWSTR)mem.p;


      // add this to the tree
      fc.Info = fi;
      fc.iOffset = iCur;
      if (m_tMFFILECACHE.Find (pszName)) {
         // somehow this got corrupted
         _ASSERTE (!m_tMFFILECACHE.Find (pszName));
         break;
      }
      m_tMFFILECACHE.Add (pszName, &fc, sizeof(fc));

      // account for the amount of data used
      m_iSizeUsed += fc.Info.iOffsetData + fc.Info.iDataSize - fc.iOffset;

      // move on
      iCur = fi.iOffsetNext;
   } // while offset

   // done
   return hf;
}


/*************************************************************************************
CMegaFile::Exists - Returns TRUE if the given file exists, FALSE if not

inputs
   PWSTR          pszFile - File to look for (within the megafile)
   PMFFILEINFO    pInfo - If not NULL, and the file is found, filled with the file info.
returns
   BOOL - TRUE if success
*/
BOOL CMegaFile::Exists (PWSTR pszFile, PMFFILEINFO pInfo)
{
   BOOL fRet = FALSE;
   EnterCriticalSection (&m_CritSec);

   HANDLE hf = INVALID_HANDLE_VALUE;
   
   // BUGFIX - only open if we're assuming it's shared or if nothing loaded
   if (!m_fAssumeNotShared || (m_iLastVersion == -1)) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
   }

   PMFFILECACHE pc = (PMFFILECACHE) m_tMFFILECACHE.Find (pszFile);
   if (!pc)
      goto done;

   if (pInfo)
      *pInfo = pc->Info;
   fRet = TRUE;

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CMegaFile::LoadUpdateHeader - This updates a block header with a new filetime so
know when last accessed the file. It also writes a new header onto the file in
general so there's a new version number.

inputs
   HANDLE               hf - Opened file. Can be INVALID_HANDLE_VALUE
   PMFFILECACHE         pc - This file has just been opened. Update it's file time.
                              Save it away
returns
   BOOL - TRUE if success
*/
BOOL CMegaFile::LoadUpdateHeader (HANDLE hf, PMFFILECACHE pc)
{
   // update some info
   __int64 iMovedTo;
   DWORD dwWritten;
   if (hf != INVALID_HANDLE_VALUE) {
      m_iLastVersion++;

      // generate the header
      MFHEADER mfh;
      memset (&mfh, 0 ,sizeof(mfh));
      mfh.gID = GUID_MegaFileID;
      mfh.gIDSub = m_gIDSub;
      mfh.iOffsetFirstFile = m_iOffsetFirstFile;
      mfh.iVersion = m_iLastVersion;

      // write out the header
      LARGE_INTEGER li;
      memset (&li, 0, sizeof(li));
      if (!MySetFilePointerEx (hf, li, (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != 0))
         return FALSE;  // shouldnt happen
      if (!WriteFile (hf, &mfh, sizeof(mfh), &dwWritten, NULL) || (dwWritten != sizeof(mfh)))
         return FALSE;  // shouldnt happen
   }

   // write out the other data block
   GetSystemTimeAsFileTime (&pc->Info.iTimeAccess);
      // updating the internal file time just in case ever write it out
   if (hf != INVALID_HANDLE_VALUE) {
      if (!MySetFilePointerEx (hf, *((LARGE_INTEGER*)&pc->iOffset), (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != pc->iOffset))
         return FALSE;  // shouldnt happen
      if (!WriteFile (hf, &pc->Info, sizeof(pc->Info), &dwWritten, NULL) || (dwWritten != sizeof(pc->Info)))
         return FALSE;  // shouldnt happen
   }
   
   return TRUE;
}



/*************************************************************************************
CMegaFile::FindIgnoreDir - Finds a file if the directories are to be ignored.
This only does name-for-name comparison.

NOTE: This is an INTERNAL function and assumes that the critical section is entered.

inputs
   WCHAR          pszFind - To find
returns
   PWSTR - Name of the file actually found (stored in the tree), so dont change this
*/
PWSTR CMegaFile::FindIgnoreDir (WCHAR *pszFind)
{
   PWSTR pszCur;
   DWORD dwLenFind;
   while (pszCur = wcschr(pszFind, L'\\'))
      pszFind = pszCur+1;
   dwLenFind = (DWORD) wcslen(pszFind);

   DWORD i;
   for (i = 0; i < m_tMFFILECACHE.Num(); i++) {
      PWSTR psz = m_tMFFILECACHE.Enum (i);
      DWORD dwLen = (DWORD)wcslen(psz);
      if (dwLen < dwLenFind)
         continue;   // too short

      // if not backslash at start then bad
      if ((dwLen > dwLenFind) && (psz[dwLen - dwLenFind - 1] != L'\\'))
         continue;   // no backslash

      // compare
      if (!_wcsicmp(psz + (dwLen - dwLenFind), pszFind))
         return psz; // found it
   } // i

   // else not
   return NULL;
}


/*************************************************************************************
CMegaFile::Load - This loads a sub-file from the megafile.

inputs
   PWSTR          pszFile - Sub-file name
   __int64        *piSize - Filled with the number of bytes allocated
   BOOL           fIgnoreDir - If TRUE then this compares the name-only (not directory) part of the file
                  against the name-only (not directory) part of the  directory elem.
returns
   PVOID - NULL if can't load, or memory allocated by EscMalloc() and which must
   be freed by MegaFileFree(). This is the contents of the entire file
*/
PVOID CMegaFile::Load (PWSTR pszFile, __int64 *piSize, BOOL fIgnoreDir)
{
   PVOID pRet = NULL;
   EnterCriticalSection (&m_CritSec);

   HANDLE hf = INVALID_HANDLE_VALUE;
   
   // BUGFIX - only open if we're assuming it's shared or if nothing loaded
   if (!m_fAssumeNotShared || (m_iLastVersion == -1)) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
   }

   // if set to ignore the directory (and just compare names) then do so
retest:
   PWSTR pszFileUse = pszFile;
   if (fIgnoreDir) {
      pszFileUse = FindIgnoreDir (pszFile);
      if (!pszFileUse)
         goto done;
   }

   PMFFILECACHE pc = (PMFFILECACHE) m_tMFFILECACHE.Find (pszFileUse);
   if (!pc)
      goto done;


   // if we haven't opened the file yet then we need to...
   if (hf == INVALID_HANDLE_VALUE) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
      goto retest;   // since the file might have changed
   }

   // seek out the location
   __int64 iMovedTo;
   if (!MySetFilePointerEx (hf, *((LARGE_INTEGER*)&pc->Info.iOffsetData), (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != pc->Info.iOffsetData))
      goto done;
   *piSize = pc->Info.iDataSize;

   // allocate info
   pRet = ESCMALLOC ((DWORD)pc->Info.iDataSize);
   if (!pRet)
      goto done;

   // read in the info
   DWORD dwRead;
   if (!ReadFile (hf, pRet, (DWORD)pc->Info.iDataSize, &dwRead, NULL) || (dwRead != (DWORD)pc->Info.iDataSize)) {
      EscFree (pRet);
      pRet = NULL;
      goto done;
   }

   // update last access time
   LoadUpdateHeader (hf, pc);

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return pRet;
}



/*************************************************************************************
CMegaFile::LoadGlobal - This loads a sub-file from the megafile. It works like
Load() exect it returns a HGLOBAL that can be used with mmio file functions.

inputs
   PWSTR          pszFile - Sub-file name
   __int64        *piSize - Filled with the number of bytes allocated
   BOOL           fIgnoreDir - If TRUE then this compares the name-only (not directory) part of the file
                  against the name-only (not directory) part of the  directory elem.
returns
   HGLOBAL - NULL if can't load, or memory allocated by GlobalAlloc() and which must
   be freed by GlobalFree(). This is the contents of the entire file
*/
HGLOBAL CMegaFile::LoadGlobal (PWSTR pszFile, __int64 *piSize, BOOL fIgnoreDir)
{
   // NOTE: Not tested
   HGLOBAL pRet = NULL;
   EnterCriticalSection (&m_CritSec);

   HANDLE hf = INVALID_HANDLE_VALUE;
   
   // BUGFIX - only open if we're assuming it's shared or if nothing loaded
   if (!m_fAssumeNotShared || (m_iLastVersion == -1)) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
   }

   // if set to ignore the directory (and just compare names) then do so
   // NOTE: Not tested
retest:
   PWSTR pszFileUse = pszFile;
   if (fIgnoreDir) {
   // NOTE: Not tested
      pszFileUse = FindIgnoreDir (pszFile);
      if (!pszFileUse)
         goto done;
   }

   PMFFILECACHE pc = (PMFFILECACHE) m_tMFFILECACHE.Find (pszFileUse);
   if (!pc)
      goto done;


   // if we haven't opened the file yet then we need to...
   if (hf == INVALID_HANDLE_VALUE) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
      goto retest;   // since the file might have changed
   }

   // seek out the location
   __int64 iMovedTo;
   if (!MySetFilePointerEx (hf, *((LARGE_INTEGER*)&pc->Info.iOffsetData), (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != pc->Info.iOffsetData))
      goto done;
   *piSize = pc->Info.iDataSize;

   // allocate info
   // NOTE: Not tested
   pRet = GlobalAlloc (GMEM_MOVEABLE, (DWORD)pc->Info.iDataSize);
   if (!pRet)
      goto done;
   PVOID p = GlobalLock (pRet);

   // read in the info
   DWORD dwRead;
   if (!ReadFile (hf, pRet, (DWORD)pc->Info.iDataSize, &dwRead, NULL) || (dwRead != (DWORD)pc->Info.iDataSize)) {
      GlobalUnlock (pRet);
      GlobalFree (pRet);
      pRet = NULL;
      goto done;
   }
   GlobalUnlock (pRet);

   // update last access time
   LoadUpdateHeader (hf, pc);

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return pRet;
}



/*************************************************************************************
CMegaFile::FindBefore - Finds the node that comes before the current one.

inputs
   __int64        iOffset - Offset of the current one
returns
   PMFFILECACHE - One before, or NULL if cant find one
*/
PMFFILECACHE CMegaFile::FindBefore (__int64 iOffset)
{
   // NOTE: Have to do this linearly 
   
   // quick check
   if (m_iOffsetFirstFile == iOffset)
      return NULL;

   // loop through all elems
   DWORD i;
   for (i = 0; i < m_tMFFILECACHE.Num(); i++) {
      PMFFILECACHE pc = (PMFFILECACHE)m_tMFFILECACHE.GetNum(i);
      if (pc->Info.iOffsetNext == iOffset)
         return pc;
   }
   
   // else, none
   return NULL;
}

/*************************************************************************************
CMegaFile::DeleteInternal - Internal function that deletes a file.

inputs
   PWSTR          pszFile - File to delete (sub-file to megafile)
   HANDLE         hf - Handle to the megafile
returns
   BOOL - TRUE if found, FALSE if didn't
*/
BOOL CMegaFile::DeleteInternal (PWSTR pszFile, HANDLE hf)
{
   PMFFILECACHE pc = (PMFFILECACHE) m_tMFFILECACHE.Find (pszFile);
   if (!pc)
      return FALSE;

   // get the one before this...
   PMFFILECACHE pBefore = FindBefore (pc->iOffset);
   if (!pBefore && (pc->iOffset != m_iOffsetFirstFile))
      return FALSE;  // shouldn happen


   // account for size freed
   m_iSizeUsed -= (pc->Info.iOffsetData + pc->Info.iDataSize - pc->iOffset);

   // incremeant heade and account for wasted
   m_iLastVersion++;

   // generate the header
   MFHEADER mfh;
   memset (&mfh, 0 ,sizeof(mfh));
   mfh.gID = GUID_MegaFileID;
   mfh.gIDSub = m_gIDSub;
   mfh.iOffsetFirstFile = m_iOffsetFirstFile;
   mfh.iVersion = m_iLastVersion;

   // deal with deletion
   DWORD dwWritten;
   __int64 iMovedTo;
   if (pBefore) {
      pBefore->Info.iOffsetNext = pc->Info.iOffsetNext;

      // write this out
      if (!MySetFilePointerEx (hf, *((LARGE_INTEGER*)&pBefore->iOffset), (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != pBefore->iOffset))
         return FALSE;  // shouldnt happen
      if (!WriteFile (hf, &pBefore->Info, sizeof(pBefore->Info), &dwWritten, NULL) || (dwWritten != sizeof(pBefore->Info)))
         return FALSE;  // shouldnt happen
   }
   else {
      mfh.iOffsetFirstFile = m_iOffsetFirstFile = pc->Info.iOffsetNext;
   }

   // write out the header
   LARGE_INTEGER li;
   memset (&li, 0, sizeof(li));
   if (!MySetFilePointerEx (hf, li, (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != 0))
      return FALSE;  // shouldnt happen
   if (!WriteFile (hf, &mfh, sizeof(mfh), &dwWritten, NULL) || (dwWritten != sizeof(mfh)))
      return FALSE;  // shouldnt happen
   
   // remove pszFIle from the list
   m_tMFFILECACHE.Remove (pszFile);

   // done
   return TRUE;
}


/*************************************************************************************
CMegaFile::Delete - Deletes the given sub-file

inputs
   PWSTR          pszFile - File to delete (sub-file to megafile)
returns
   BOOL - TRUE if success, FALSE if not found
*/
BOOL CMegaFile::Delete (PWSTR pszFile)
{
   BOOL fRet = FALSE;
   HANDLE hf = INVALID_HANDLE_VALUE;

   // if it doesnt exist then dont delete
   if (m_fAssumeNotShared && !Exists(pszFile))
      return FALSE;

   EnterCriticalSection (&m_CritSec);

   hf = OpenFileInternal (TRUE);
   if (hf == INVALID_HANDLE_VALUE)
      goto done;

   fRet = DeleteInternal (pszFile, hf);

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CMegaFile::CreateBlockForSave - This searches through data and finds an empty
slot where the information can be saved. It also changed the iOffsetNext for the
next data to be written.

inputs
   HANDLE         hf - File handle
   __int64        iNeed - Amount of space needed
   __int64        *piNext - If found a location where can save, this is filled with
                  the location for the next block (info to be written to the header)
returns
   __int64 - Location where can save, or 0 if none
*/
__int64 CMegaFile::CreateBlockForSave (HANDLE hf, __int64 iNeed, __int64 *piNext)
{
   // info about what found
   __int64 iLocToUse = 0;
   PMFFILECACHE pBefore = NULL;  // block found before this one, or NULL if at start of file

   // see if space at the very beginning
   __int64 iWasteStart = m_iOffsetFirstFile - sizeof(MFHEADER);
   if (iWasteStart >= iNeed) {
      iLocToUse = sizeof(MFHEADER);
      pBefore = NULL;
      goto allocspace;
   }

   // else, loop through all the blocks and find one with enough
   DWORD i;
   PMFFILECACHE pAtEnd = NULL;
   for (i = 0; i < m_tMFFILECACHE.Num(); i++) {
      PMFFILECACHE pCur = (PMFFILECACHE)m_tMFFILECACHE.GetNum(i);

      // keep track of last block
      if (!pAtEnd)
         pAtEnd = pCur;
      else
         if (pCur->iOffset > pAtEnd->iOffset)
            pAtEnd = pCur;

      // see how much available
      __int64 iAvail = pCur->Info.iOffsetData + pCur->Info.iDataSize;   // current end of data
      if (pCur->Info.iOffsetNext)
         iAvail = pCur->Info.iOffsetNext - iAvail;
      else
         continue;   // this is the last block
      if (iAvail <= 0)
         continue;   // no space

      if (iAvail >= iNeed) {
         // found some space
         pBefore = pCur;
         iLocToUse = pCur->Info.iOffsetData + pCur->Info.iDataSize;
         goto allocspace;
      }

   } // i

   // if get here then add to the end...
   pBefore = pAtEnd;
   iLocToUse = pAtEnd ? (pAtEnd->Info.iOffsetData + pAtEnd->Info.iDataSize) : sizeof(MFHEADER);
   // fall through


allocspace:
   // update some info
   m_iLastVersion++;
   *piNext = pBefore ? pBefore->Info.iOffsetNext : m_iOffsetFirstFile;

   // generate the header
   MFHEADER mfh;
   memset (&mfh, 0 ,sizeof(mfh));
   mfh.gID = GUID_MegaFileID;
   mfh.gIDSub = m_gIDSub;
   mfh.iOffsetFirstFile = m_iOffsetFirstFile;
   mfh.iVersion = m_iLastVersion;
   if (!pBefore)
      mfh.iOffsetFirstFile = m_iOffsetFirstFile = iLocToUse;   // since must have inserted before

   // write out the header
   LARGE_INTEGER li;
   __int64 iMovedTo;
   DWORD dwWritten;
   memset (&li, 0, sizeof(li));
   if (!MySetFilePointerEx (hf, li, (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != 0))
      return FALSE;  // shouldnt happen
   if (!WriteFile (hf, &mfh, sizeof(mfh), &dwWritten, NULL) || (dwWritten != sizeof(mfh)))
      return FALSE;  // shouldnt happen

   // write out the other data block
   if (pBefore) {
      pBefore->Info.iOffsetNext = iLocToUse;

      // write this out
      if (!MySetFilePointerEx (hf, *((LARGE_INTEGER*)&pBefore->iOffset), (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != pBefore->iOffset))
         return FALSE;  // shouldnt happen
      if (!WriteFile (hf, &pBefore->Info, sizeof(pBefore->Info), &dwWritten, NULL) || (dwWritten != sizeof(pBefore->Info)))
         return FALSE;  // shouldnt happen
   }
   
   // done
   return iLocToUse;
}



/*************************************************************************************
CMegaFile::Save - This saves the given chunk of data as the file name.

inputs
   PWSTR          pszFile - File name (as sub-file in the megafile)
   PVOID          pMem - Memory to write
   __int64        iSize - Number of bytes
   FILETIME       *pftCreated - Created file time. If NULL then use current time.
   FILETIME       *pftModified - Modified file time.  If NULL then use current time.
   FILETIME       *pftAccessed - Last accessed file time.  If NULL then use current time.
returns
   BOOL - TRUE if success
*/
BOOL CMegaFile::Save (PWSTR pszFile, PVOID pMem, __int64 iSize,
                      FILETIME *pftCreated, FILETIME *pftModified, FILETIME *pftAccessed)
{
   BOOL fRet = FALSE;
   HANDLE hf = INVALID_HANDLE_VALUE;

   EnterCriticalSection (&m_CritSec);

   hf = OpenFileInternal (TRUE);
   if (hf == INVALID_HANDLE_VALUE)
      goto done;

   // what will the header be like
   MFFILECACHE fc;
   memset (&fc, 0, sizeof(fc));
   fc.Info.iDataSize = iSize;
   GetSystemTimeAsFileTime (&fc.Info.iTimeAccess);
   fc.Info.iTimeCreate = fc.Info.iTimeModify = fc.Info.iTimeAccess;
   if (pftCreated)
      fc.Info.iTimeCreate = *pftCreated;
   if (pftModified)
      fc.Info.iTimeModify = *pftModified;
   if (pftAccessed)
      fc.Info.iTimeAccess = *pftAccessed;

   // see if already exists
   PMFFILECACHE pc = (PMFFILECACHE) m_tMFFILECACHE.Find (pszFile);
   if (pc) {
      // BUGFIX - Only change timecreate to original time create IF not pftCreated
      if (!pftCreated)
         fc.Info.iTimeCreate = pc->Info.iTimeCreate;

      // delete the current one
      DeleteInternal (pszFile, hf);
   }

   // how much space need?
   int iLen = ((int)wcslen(pszFile)+1)*sizeof(WCHAR);
   __int64 iNeed = sizeof(fc.Info) + iLen + iSize;

   // find this
   __int64 iNext;
   __int64 iLoc = CreateBlockForSave (hf, iNeed, &iNext);
   if (!iLoc)
      // BUGFIX - Missed leave criticals ect
      goto done;
   fc.iOffset = iLoc;
   fc.Info.iOffsetData = fc.iOffset + sizeof(fc.Info) + iLen;
   fc.Info.iOffsetNext = iNext;

   // add file to list
   m_tMFFILECACHE.Add (pszFile, &fc, sizeof(fc));

   // account for size used
   m_iSizeUsed += (fc.Info.iOffsetData + fc.Info.iDataSize - fc.iOffset);

   // write out the info...
   __int64 iMovedTo;
   DWORD dwWritten;
   if (!MySetFilePointerEx (hf, *((LARGE_INTEGER*)&fc.iOffset), (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != fc.iOffset))
      goto done;  // shouldnt happen
   if (!WriteFile (hf, &fc.Info, sizeof(fc.Info), &dwWritten, NULL) || (dwWritten != sizeof(fc.Info)))
      goto done;  // shouldnt happen
   if (!WriteFile (hf, pszFile, iLen, &dwWritten, NULL) || (dwWritten != (DWORD)iLen))
      goto done;  // shouldnt happen
   if (!WriteFile (hf, pMem, (DWORD)iSize, &dwWritten, NULL) || (dwWritten != (DWORD)iSize))
      goto done;  // shouldnt happen

   // NOTE: Don't need to update the header version number because
   // call to CreateBlockForSave() already did this

   fRet= TRUE;
done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CMegaFile::Save - This loads in a file from disk and adds it to the megafile.
It keeps the time/date stamp from the original file

inputs
   PWSTR          pszFile - File name (as sub-file in the megafile)
returns
   BOOL - TRUE if success
*/
BOOL CMegaFile::Save (PWSTR pszFile)
{
   return SaveAs (pszFile, pszFile);
}

/*************************************************************************************
CMegaFile::SaveAs - This loads in a file from disk and adds it to the megafile.
It keeps the time/date stamp from the original file

This allows a file to be saved as a different file on the megafile.

inputs
   PWSTR          pszFile - File name (as sub-file in the megafile)
   PWSTR          pszFileDisk - File as it appears on disk
returns
   BOOL - TRUE if success
*/
BOOL CMegaFile::SaveAs (PWSTR pszFile, PWSTR pszFileDisk)
{
   char szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, pszFileDisk, -1, szTemp, sizeof(szTemp), 0, 0);

   HANDLE hf;
   CMem mem;
   BOOL fRet = FALSE;
   hf = CreateFile (szTemp, GENERIC_READ, 0, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
   if (hf == INVALID_HANDLE_VALUE)
      goto done;  // error cant find

   // get the size
   // get the file time's
   BY_HANDLE_FILE_INFORMATION fi;
   GetFileInformationByHandle (hf, &fi);
   if (fi.nFileSizeHigh)
      goto done;  // too large

   if (!mem.Required ((DWORD)fi.nFileSizeLow))
      goto done;  // cant allocate

   // read in the data
   DWORD dwRead;
   if (!ReadFile (hf, mem.p, fi.nFileSizeLow, &dwRead, NULL) || (dwRead != fi.nFileSizeLow))
      goto done;


   // write it out
   fRet = Save (pszFile, mem.p, (__int64)fi.nFileSizeLow, &fi.ftCreationTime,
      &fi.ftLastWriteTime, &fi.ftLastAccessTime);

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   return fRet;
}


/*************************************************************************************
CMegaFile::Enum - This enumerates all the files stores in the megafile.

inputs
   PCListVariable       plName - Cleared out and then filled with all the names,
   PCListFixed          plMFFILEINFO - Cleared out, initialized to sizeof (MFFILEINFO),
                        and filled with corresponding info for file.
                        Can be NULL
   PWSTR                pszPrefix - If this is not NULL, then only files beginning
                        with this prefix will be enumerated.
returns
   BOOL - TRUE if success
*/
BOOL CMegaFile::Enum (PCListVariable plName, PCListFixed plMFFILEINFO, PWSTR pszPrefix)
{
   BOOL fRet = FALSE;
   HANDLE hf = INVALID_HANDLE_VALUE;

   EnterCriticalSection (&m_CritSec);
   
   // BUGFIX - only open if we're assuming it's shared or if nothing loaded
   if (!m_fAssumeNotShared || (m_iLastVersion == -1)) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
   }

   // clear out
   plName->Clear();
   if (plMFFILEINFO)
      plMFFILEINFO->Init (sizeof(MFFILEINFO));

   DWORD dwPrefixLen = pszPrefix ? (DWORD)wcslen(pszPrefix) : 0;

   DWORD i;
   plName->Required (m_tMFFILECACHE.Num());
   for (i = 0; i < m_tMFFILECACHE.Num(); i++) {
      PMFFILECACHE pc = (PMFFILECACHE)m_tMFFILECACHE.GetNum(i);
      PWSTR psz = m_tMFFILECACHE.Enum (i);
      DWORD dwLen = (DWORD)wcslen(psz);

      // if prefix then check
      if (pszPrefix) {
         if (dwLen < dwPrefixLen)
            continue;   // skip this
         if (_wcsnicmp (psz, pszPrefix, dwPrefixLen))
            continue;   // different prefix
      }

      plName->Add (psz, (dwLen+1)*sizeof(WCHAR));
      if (plMFFILEINFO)
         plMFFILEINFO->Add (&pc->Info);
   } // i

   fRet = TRUE;   // BUGFIX - hadnt set

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}





/*************************************************************************************
CMegaFile::GetNum - Gets the file name based on an index, from 0.. Num()-1.

inputs
   DWORD          dwIndex - Index number, from 0..Num()-1
   PCMem          pMem - Filled with the string for the file name
returns
   BOOL - TRUE if success
*/
BOOL CMegaFile::GetNum (DWORD dwIndex, PCMem pMem)
{
   BOOL fRet = FALSE;
   HANDLE hf = INVALID_HANDLE_VALUE;
   pMem->m_dwCurPosn = 0;
   pMem->CharCat (0);

   EnterCriticalSection (&m_CritSec);
   
   // BUGFIX - only open if we're assuming it's shared or if nothing loaded
   if (!m_fAssumeNotShared || (m_iLastVersion == -1)) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
   }

   PWSTR psz = m_tMFFILECACHE.Enum (dwIndex);
   if (!psz)
      goto done;
   pMem->m_dwCurPosn = 0;
   pMem->StrCat (psz, (DWORD)wcslen(psz)+1);

   fRet = TRUE;

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CMegaFile::Num - Returns the number of files in the megafile.

inputs
returns
   DWORD - Number. Or -1 if error
*/
DWORD CMegaFile::Num (void)
{
   DWORD dwRet = (DWORD) -1;
   HANDLE hf = INVALID_HANDLE_VALUE;

   EnterCriticalSection (&m_CritSec);
   
   // BUGFIX - only open if we're assuming it's shared or if nothing loaded
   if (!m_fAssumeNotShared || (m_iLastVersion == -1)) {
      hf = OpenFileInternal (FALSE);
      if (hf == INVALID_HANDLE_VALUE)
         goto done;
   }

   dwRet = m_tMFFILECACHE.Num();

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return dwRet;
}

/*************************************************************************************
CMegaFile::LimitSize - This limits the size of the file to a certain amount.
If it's too large, then old files are deleted until the the amount of data
stored in the file fits to the limit.

inputs
   __int64        iWant - Size (in bytes) that want for the file
   DWORD          dwTooYoung - If a file has been used within this many minutes
                     then don't even think about deleting it.
returns
   none
*/
void CMegaFile::LimitSize (__int64 iWant, DWORD dwTooYoung)
{
   // figure out time stamp
   __int64 iTooYoung;
   GetSystemTimeAsFileTime ((LPFILETIME) &iTooYoung);
   iTooYoung -= (__int64)dwTooYoung * 60 * 10000000; /*10 million = 100 nano-sec*/

   // open up
   HANDLE hf = INVALID_HANDLE_VALUE;
   EnterCriticalSection (&m_CritSec);

   if (m_fAssumeNotShared && (m_iLastVersion != -1) && (m_iSizeUsed <= iWant))
      goto done;  // nothing to change

   hf = OpenFileInternal (TRUE);
   if (hf == INVALID_HANDLE_VALUE)
      goto done;

   // if small enough goto done
   if (m_iSizeUsed <= iWant)
      goto done;

   // NOTE: If more than size want, then trim a bit more severly so that go
   // to 80% of that
   iWant = iWant / 10 * 8;

   // repeat until done
   while (TRUE) {
      // figure out how large
      m_iSizeUsed = 0;  // might as well recalc
      __int64 iBest = 0 ;
      PWSTR pszBest = NULL;
      PMFFILECACHE pBest = NULL;
      DWORD i;
      for (i = 0; i < m_tMFFILECACHE.Num(); i++) {
         PMFFILECACHE pc = (PMFFILECACHE)m_tMFFILECACHE.GetNum(i);
         m_iSizeUsed += pc->Info.iDataSize + pc->Info.iOffsetData - pc->iOffset;

         __int64 iAccess = *((__int64*)&pc->Info.iTimeAccess);
         if (iAccess >= iTooYoung)
            continue;   // this is too young, dont even considering deleting it

         // find the oldest
         if (!pBest || (iAccess < iBest)) {
            iBest = iAccess;
            pBest = pc;
            pszBest = m_tMFFILECACHE.Enum (i);
         }
      } // i

      // if the total size is small enough then done
      if (m_iSizeUsed <= iWant)
         break;


      // if we cant find anything to delete then done
      if (!pBest)
         break;

      // else delete
      DeleteInternal (pszBest, hf);
   } // while true

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
}



/*************************************************************************************
CMegaFile::Clear - Wipes out the contents of the file.

returns
   BOOL - TRUE if success, FALSE if didn't
*/
BOOL CMegaFile::Clear (void)
{
   // open up
   HANDLE hf = INVALID_HANDLE_VALUE;
   BOOL fRet = FALSE;

   EnterCriticalSection (&m_CritSec);
   hf = OpenFileInternal (TRUE);
   if (hf == INVALID_HANDLE_VALUE)
      goto done;

   // no size
   m_iSizeUsed = 0;
   m_tMFFILECACHE.Clear();

   // incremeant heade and account for wasted
   m_iLastVersion++;

   // generate the header
   MFHEADER mfh;
   memset (&mfh, 0 ,sizeof(mfh));
   mfh.gID = GUID_MegaFileID;
   mfh.gIDSub = m_gIDSub;
   mfh.iOffsetFirstFile = m_iOffsetFirstFile = 0;
   mfh.iVersion = m_iLastVersion;

   // write out the header
   LARGE_INTEGER li;
   memset (&li, 0, sizeof(li));
   __int64 iMovedTo;
   DWORD dwWritten;
   if (!MySetFilePointerEx (hf, li, (LARGE_INTEGER*)&iMovedTo, FILE_BEGIN) || (iMovedTo != 0))
      goto done;  // shouldnt happen
   if (!WriteFile (hf, &mfh, sizeof(mfh), &dwWritten, NULL) || (dwWritten != sizeof(mfh)))
      goto done;  // shouldnt happen

   fRet= TRUE;

done:
   if (hf != INVALID_HANDLE_VALUE)
      CloseHandle (hf);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}




/*************************************************************************************
MegaFileSet - This sets the current megafile object to use when MegaFileXXX() functions
are called.

How the callbacks work: When a file is opened from the megafile, but whichdoesn't
exist, the pCallback function is called with MFCALLBACK_NEEDFILE or MFCALLBACK_NEEDFILEIGNOREDIR. If this returns FALSE
the MegaFileOpen() call returns immediately. if it returns TRUE, the MFOpen() call waits,
occasionally calling pCallback with MFCALLBACK_STILLALIVE to see if it should still wait.
It waits as long as TRUE is retured, but exits when FALSE is returned (such as a network
shutdown)

inputs
   PCMagaFile        pMegaFile - Megafile object, or NULL if always use standard file functions
   PMEGAFILECALLBACK pCallback - If the file can't be found, this function is called to indicate
                     what type of file is wanted. If this is NULL then it won't be called
   PVOID             pCallbackInfo - parameter passed through when pCallback() is actually
                     called.

returns
   none
*/
void DLLEXPORT MegaFileSet (PCMegaFile pMegaFile, PMEGAFILECALLBACK pCallback, PVOID pCallbackInfo)
{
   gpMegaFile = pMegaFile;
   gpMegaFileCallbackInfo = pCallbackInfo;
   gpMegaFileCallback = pCallback;
}


/*************************************************************************************
MegaFileGet - Gets the current megafile
*/
PCMegaFile DLLEXPORT MegaFileGet (void)
{
   return gpMegaFile;
}


/*************************************************************************************
MegaFileOpen - This opens a file for reading or writing, like using fopen().
If there's no megafile (see MegaFileSet()), then this just ends up calling fopen().
If there is a megafile, it will read from the megafile instead.

inputs
   PWSTR          pszName - File name to open
   BOOL           fRead - If TRUE then open for reading "rb", FALSE for writing "wb".
   DWORD          dwFlags - Zero or more of the following flags:
                     MFO_IGNOREMEGAFILE - No matter what, ignore the megafile
                     MFO_IGNOREDIR - If open from a megafile, this only looks for the
                        bit after the last backslash. Used for the 3dob libraries.
                     MFO_USEGLOBAL - So can get hGlobal for reading for mmioOpen() call
returns
   PMEGAFILE - Structure containing open information, to be passed to MegaFileWrite(), etc.
               Or, NULL if error
*/
PMEGAFILE DLLEXPORT MegaFileOpen (PSTR pszName, BOOL fRead, DWORD dwFlags)
{
   WCHAR szTemp[256];
   MultiByteToWideChar (CP_ACP, 0, pszName, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));
   return MegaFileOpen (szTemp, fRead, dwFlags);
}

PMEGAFILE DLLEXPORT MegaFileOpen (PWSTR pszName, BOOL fRead, DWORD dwFlags)
{
   // allocate memory for megafile
   DWORD dwLen = ((DWORD)wcslen(pszName)+1)*sizeof(WCHAR);
   PMEGAFILE pmf = (PMEGAFILE)ESCMALLOC (sizeof(MEGAFILE) + dwLen);
   if (!pmf)
      return NULL;
   memcpy (pmf+1, pszName, dwLen);
   memset (pmf, 0, sizeof(*pmf));
   pmf->fRead = fRead;

   // if don't have megafile, or have flags to force ignore megafile, then do so
   if (!gpMegaFile || (dwFlags & MFO_IGNOREMEGAFILE)) {
#ifdef _DEBUG
      OutputDebugString ("\r\nOpen FILESYSTEM ");
      OutputDebugStringW (pszName);
#endif

      // just open directly
      char szTemp[512];
      WideCharToMultiByte (CP_ACP, 0, pszName, -1, szTemp, sizeof(szTemp), 0, 0);
      pmf->pFile = fopen(szTemp, fRead ? "rb" : "wb");
      if (!pmf->pFile) {
         EscFree (pmf);
         return NULL;
      }

      // else done
      return pmf;
   }

#ifdef _DEBUG
      OutputDebugString ("\r\nOpen MEGAFILE ");
      OutputDebugStringW (pszName);
#endif

   // if it's simulating a write, then easy
   if (!fRead) {
      pmf->iMemAllocated = 1024;
      pmf->pbMem = (PBYTE)ESCMALLOC ((size_t)pmf->iMemAllocated);
      if (!pmf->pbMem) {
         EscFree (pmf);
         return NULL;
      }

      return pmf;
   }

   // else, using megafile to read
   BOOL fFirstTime = TRUE;
   while (TRUE) {
      // try to load in the file
      if (dwFlags & MFO_USEGLOBAL) {
         // NOTE: Not tested
         // load into hGlobal
         pmf->hGlobal = gpMegaFile->LoadGlobal (pszName, &pmf->iMemSize,
            (dwFlags & MFO_IGNOREDIR) ? TRUE : FALSE);
         if (pmf->hGlobal) {
            // NOTE: Not tested
            pmf->pbMem = (PBYTE)GlobalLock (pmf->hGlobal);
            return pmf;
         }
      }
      else {
         // load into memory
         pmf->pbMem = (PBYTE) gpMegaFile->Load (pszName, &pmf->iMemSize,
            (dwFlags & MFO_IGNOREDIR) ? TRUE : FALSE);
         if (pmf->pbMem)
            return pmf;
      }

      // else, if get here, opening failed... therefore, call callback
      if (!gpMegaFileCallback)
         break;   // error

      // send message that need opened
      BOOL fRet;
      if (fFirstTime)
         fRet = gpMegaFileCallback (
            (dwFlags & MFO_IGNOREDIR) ? MFCALLBACK_NEEDFILEIGNOREDIR : MFCALLBACK_NEEDFILE,
            pszName,
            gpMegaFileCallbackInfo);
      else
         fRet = gpMegaFileCallback (
            MFCALLBACK_STILLALIVE,
            pszName,
            gpMegaFileCallbackInfo);
      if (!fRet)
         break;   // error

      // wait
      fFirstTime = FALSE;
      Sleep (50);

      // loop around and try to find data again
   } // while TRUE

   EscFree (pmf);
   return NULL;
}



/*************************************************************************************
MegaFileOpen - Returns TRUE if there's a megafile (gpMegaFile) around.
returns
   BOOL - TRUE if the megafile is in use
*/
BOOL DLLEXPORT MegaFileInUse (void)
{
   return gpMegaFile ? TRUE : FALSE;
}



/*************************************************************************************
MegaFileClose - Closes the PMEGAFILE returned by MegaFileOpen().

inputs
   PMEGAFILE         pmf - From MegaFileOpen()
returns
   noen
*/
void DLLEXPORT MegaFileClose (PMEGAFILE pmf)
{
   if (pmf->pFile)
      fclose (pmf->pFile);

   // if we're saving this to a megafile then do so now
   if (!pmf->fRead && pmf->pbMem && gpMegaFile)
      gpMegaFile->Save ((PWSTR)(pmf+1), pmf->pbMem, pmf->iMemSize);

   if (pmf->hGlobal) {
      // NOTE: Not tested
      GlobalUnlock (pmf->hGlobal);
      GlobalFree (pmf->hGlobal);
   }
   else if (pmf->pbMem)
      EscFree (pmf->pbMem);

   EscFree (pmf);
}



/*************************************************************************************
MegaFileRead - Like fread(), except uses a megafile

inputs
   void              *pMem - Memory to write to
   __int64           iSize - Size of each element
   __int64           iCount - Number of elements
   PMEGAFILE         pmf - Megafile to use
returns
   __int64 - Number of fill items actually read
*/
__int64 DLLEXPORT MegaFileRead (void *pMem, __int64 iSize, __int64 iCount, PMEGAFILE pmf)
{
   if (pmf->pFile)
      return fread (pMem, (size_t)iSize, (size_t)iCount, pmf->pFile);

   // else, from memory... see how much is left
   __int64 iLeft = (pmf->iMemSize - pmf->iMemPos) / iSize;
   iLeft = max(iLeft, 0);
   iLeft = min(iLeft, iCount);

   memcpy (pMem, pmf->pbMem + pmf->iMemPos, (size_t)iLeft * (size_t)iSize);
   pmf->iMemPos += iLeft * iSize;
   return iLeft;
}



/*************************************************************************************
MegaFileWrite - Like fwrite(), except uses a megafile

inputs
   void              *pMem - Memory to write to
   __int64           iSize - Size of each element
   __int64           iCount - Number of elements
   PMEGAFILE         pmf - Megafile to use
returns
   __int64 - Number of fill items actually read
*/
__int64 DLLEXPORT MegaFileWrite (void *pMem, __int64 iSize, __int64 iCount, PMEGAFILE pmf)
{
   if (pmf->pFile)
      return fwrite (pMem, (size_t)iSize, (size_t)iCount, pmf->pFile);

   if (pmf->fRead)
      return 0;  // can't write this

   // how large will file need to be
   __int64 iNeed = max(pmf->iMemSize, pmf->iMemPos + iSize * iCount);
   if (iNeed > pmf->iMemAllocated) {
      PBYTE pbNew = (PBYTE) ESCREALLOC (pmf->pbMem, (size_t)iNeed);
      if (!pbNew)
         return 0; // error
      pmf->iMemAllocated = iNeed;
      pmf->pbMem = pbNew;
   }

   // copy
   memcpy (pmf->pbMem + pmf->iMemPos, pMem, (size_t)iSize * (size_t)iCount);

   // update
   pmf->iMemPos = iNeed;
   pmf->iMemSize = max(pmf->iMemPos, pmf->iMemSize);

   return iCount;
}

/*************************************************************************************
MegaFileTell - Like ftell()

inputs
   PMEGAFILE         pmf - Megafile to use
returns
   __int64 - Current offset
*/
__int64 DLLEXPORT MegaFileTell (PMEGAFILE pmf)
{
   if (pmf->pFile)
      return ftell (pmf->pFile);

   // else
   return pmf->iMemPos;
}


/*************************************************************************************
MegaFileSeek - Like fseek()

inputs
   PMEGAFILE         pmf - Megafile to use
   __int64           iOffset - Amount to offset
   int               iOrigin - From what
returns
   int - 0 if success
*/
int DLLEXPORT MegaFileSeek (PMEGAFILE pmf, __int64 iOffset, int iOrigin)
{
   if (pmf->pFile)
      return _fseeki64 (pmf->pFile, (size_t)iOffset, iOrigin);

   // else
   __int64 iWant;
   if (iOrigin == SEEK_END)
      iWant = pmf->iMemSize - iOffset;
   else if (iOrigin == SEEK_CUR)
      iWant = pmf->iMemPos + iOffset;
   else
      iWant = iOffset;

   if ((iWant < 0) || (iWant > pmf->iMemSize))
      return 1;

   pmf->iMemPos = iWant;
   return 0;
}

/*************************************************************************************
MegaFileFree - Frees memory from MegaFileOpen()

NEED to do this because sometimes different heaps
*/
DLLEXPORT void MegaFileFree (PVOID pMem)
{
   EscFree (pMem);
}

// BUGBUG - may eventually want a function to load a set of files, and only open the
// the database file once
