/*************************************************************************************
CTextLogs.cpp - Handles automatic logging.

begun 26/10/05 by Mike Rozak.
Copyright 2005 by Mike Rozak. All rights reserved
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


/*************************************************************************************
CTextLogFile::Constructor and destructor
*/
CTextLogFile::CTextLogFile (void)
{
   m_hThread = NULL;
   InitializeCriticalSection (&m_CritSec);
   m_hSignalToThread = m_hSignalFromThread = NULL;
   m_fReadOnly = FALSE;
   m_fWantToQuit = FALSE;
   m_File = NULL;
   m_lLOGLINE.Init (sizeof(LOGLINE));
   m_iSize = m_iLoc = 0;
   m_dwCacheValid = 0;
   m_iCacheLoc = 0;
   m_fWantSignal = FALSE;
   m_alLOGQUEUE[0].Init (sizeof(LOGQUEUE));
   m_alLOGQUEUE[1].Init (sizeof(LOGQUEUE));
   memset (&m_ftLastUsed, 0, sizeof(m_ftLastUsed));
   m_fWorking = FALSE;
   m_fNeedToFlush = FALSE;
   m_fJustRead = m_fJustWrote = FALSE;
}

CTextLogFile::~CTextLogFile (void)
{
   if (m_hThread) {
      EnterCriticalSection (&m_CritSec);
      m_fWantToQuit = TRUE;
      SetEvent (m_hSignalToThread);
      LeaveCriticalSection (&m_CritSec);

      WaitForSingleObject (m_hThread, INFINITE);
      DeleteObject (m_hThread);
   }

   DeleteCriticalSection (&m_CritSec);
   if (m_hSignalToThread)
      DeleteObject (m_hSignalToThread);
   if (m_hSignalFromThread)
      DeleteObject (m_hSignalFromThread);

   // free up the queues
   DWORD i,j;
   for (j = 0; j < 2; j++) {
      PLOGQUEUE plq = (PLOGQUEUE) m_alLOGQUEUE[j].Get(0);
      for (i = 0; i < m_alLOGQUEUE[j].Num(); i++, plq++)
         DeleteQueue (plq);
   } // j
}

/*************************************************************************************
LogFileThreadProc - Thread that handles the internet.
*/
DWORD WINAPI LogFileThreadProc(LPVOID lpParameter)
{
   PCTextLogFile pThread = (PCTextLogFile) lpParameter;

   pThread->ThreadProc ();

   return 0;
}



/*************************************************************************************
CTextLogFile::Init - Initializes a file

inputs
   PWSTR          pszFile - File to use
   DWORD          dwIDDate - Combined with dwIDTime, uniquely identifies the file.
                  Used so that search can easily note matches
   DWORD          dwIDTime - See above.
returns
   BOOL - TRUE if success (or at least not immediate failure), FALSE if error
*/
BOOL CTextLogFile::Init (PWSTR pszFile, DWORD dwIDDate, DWORD dwIDTime)
{
   if (m_hThread)
      return FALSE;

   m_dwIDDate = dwIDDate;
   m_dwIDTime = dwIDTime;

   FILE *f = _wfopen (pszFile, L"rb");
   if (f) {
      WORD w = 0;
      fread (&w, sizeof(w), 1, f);
      fclose (f);
      if (w != 0xfeff)
         return FALSE;  // file exists, but not unicode
   }

   // fill in the name
   MemZero (&m_memScratch);
   MemCat (&m_memScratch, pszFile);

   DWORD dwID;
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalFromThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hThread = CreateThread (NULL, 0, LogFileThreadProc, this, 0, &dwID);
      // NOTE: Not using ESCTHREADCOMMITSIZE because want to be as stable as possible
      // assume created

   // make sure has lower thread priority
   SetThreadPriority (m_hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL));

   return TRUE;
}


/*************************************************************************************
CTextLogFile::DeleteQueue - Frees up the memory pointed to by a queue object.

inputs
   PLOGQUEUE         plq - Info
returns
   noen
*/
void CTextLogFile::DeleteQueue (PLOGQUEUE plq)
{
   if (plq->pMem)
      delete plq->pMem;
}

/*************************************************************************************
CTextLogFile::InternalFRead - Internal function that simulates FRead.

inputs
   __int64        iLoc - From what location
   DWORD          dwSize - Number of bytes
   PVOID          pMem - Memory to read to
returns
   DWORD - Number of bytes actually read
*/
size_t CTextLogFile::InternalFRead (__int64 iLoc, size_t dwSize, PVOID pMem)
{
   __int64 iLeft = m_iSize - iLoc;
   if (iLeft <= 0)
      return 0;   // beyond IF
   if (iLeft <= (__int64) dwSize)
      dwSize = (DWORD)iLeft;

   if (m_fJustWrote || (iLoc != m_iLoc)) {
      fflush (m_File);  // BUGFIX - Adding thse just in case affects fseek() or ftell()
      fseek (m_File, iLoc, SEEK_SET);
      m_iLoc = iLoc;
      m_fJustWrote = FALSE;
   }

   // read
   size_t dwRet = fread (pMem, 1, dwSize, m_File);
   m_iLoc += (__int64)dwRet;
   m_fJustRead = TRUE;
   return dwRet;
}


/*************************************************************************************
CTextLogFile::InternalFWrite - Appends data onto the end of the file.

inputs
   DWORD          dwSize - Number of bytes
   PVOID          pMem - Memory to write to
returns
   DWORD - Number of bytes written
*/
size_t CTextLogFile::InternalFWrite (size_t dwSize, PVOID pMem)
{
   if (m_fJustRead || (m_iLoc != m_iSize)) {
      fflush (m_File);  // BUGFIX - Adding thse just in case affects fseek() or ftell()
      fseek (m_File, m_iSize, SEEK_SET);
      m_iLoc = m_iSize;
      m_fJustRead = FALSE;
   }

   // write
   size_t dwRet = fwrite (pMem, 1, dwSize, m_File);
   m_iLoc += (__int64)dwRet;
   m_iSize = m_iLoc; // since at the end

   m_fNeedToFlush = TRUE;
   m_fJustWrote = TRUE;

   return dwRet;
}

/*************************************************************************************
CTextLogFile::InternalFWrite - Appends data onto the end of the file.

inputs
   PWSTR          psz - String
returns
   DWORD - Number of bytes written
*/
size_t CTextLogFile::InternalFWrite (PWSTR psz)
{
   return InternalFWrite (wcslen(psz)*sizeof(WCHAR), (PVOID)psz);
}

/*************************************************************************************
CTextLogFile::MemoryFromFile - If necessary, reads part of the file into
m_memCache, and returns a pointer to the requested memory. If the memory
is already there just returns the pointer.

inputs
   __int64           iLoc - Location to start at
   DWORD             dwSize - Number of bytes
   DWORD             *pdwAvail - If this is not NULL, fills in the number of bytes
                     available at iLoc
returns
   PVOID - Memory. If pdwAvail is NULL and dwSize bytes cannot be read in, then
            this will return NULL
*/
PVOID CTextLogFile::MemoryFromFile (__int64 iLoc, size_t dwSize, size_t *pdwAvail)
{
   // see if it's already there
   __int64 iEndWant = iLoc + (__int64)dwSize;
   __int64 iEndHave = m_iCacheLoc + (__int64)m_dwCacheValid;
   if ((iLoc >= m_iCacheLoc) && (iEndWant <= iEndHave)) {
      if (pdwAvail)
         *pdwAvail = dwSize;
      return (PBYTE)m_memCache.p + (iLoc - m_iCacheLoc);
   }

   // else, see if trying to read beyond the end of the file
   if (iEndWant > m_iSize) {
      if (!pdwAvail)
         return NULL;   // always going to be error
   }

   // else, read in
   size_t dwLoad = max(dwSize, 1024);   // minimum sized chunk to load
   if (!m_memCache.Required(dwLoad))
      return NULL;
   size_t dwLoaded = InternalFRead (iLoc, dwLoad, m_memCache.p);
   m_dwCacheValid = dwLoaded;
   m_iCacheLoc = iLoc;

   if (pdwAvail)
      *pdwAvail = min(dwLoaded, dwSize);
   else if (dwLoaded < dwSize) // && dwAvail == NULL
      return NULL;   // cant load enough

   // else done
   return m_memCache.p;
}


/*************************************************************************************
CTextLogFile::ReadLineHeader - Reads the header from the start of a "line" of data.
It will add the line information to the m_lLOGLINE list.

The header is "@YYYYMMDDHHMMSSHH" to indicate the time stamp...
Followed by zero or more ",X=GUIDDIGITS", where X is "u" for user, "a" for actor,
   "r" for room, "o" for object
Followed by a ":" and then the actual data

inputs
   __int64           iLoc - Location
returns
   __int64 - New location to start from next, or -1 seems to have reached the EOF
            and no more data
*/
__int64 CTextLogFile::ReadLineHeader (__int64 iLoc)
{
   DWORD dwSize = 17 * sizeof(WCHAR);
   PWSTR psz = (PWSTR)MemoryFromFile (iLoc, dwSize);
   if (!psz)
      return -1;  // couldnt read what expected

   if (psz[0] != L'@')
      return iLoc+sizeof(WCHAR); // error in symbol
   __int64 iCur = iLoc + (__int64)dwSize;

   // get all the digits
   WORD awValue[7];
   DWORD adwDigitIndex[16] = {0,0,0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6};
   DWORD i;
   psz++;
   memset (awValue, 0, sizeof(awValue));
   for (i = 0; i < 16; i++, psz++) {
      if ((*psz < L'0') || (*psz > L'9'))
         return iLoc+sizeof(WCHAR); // error, since expected digits
      DWORD dwIndex = adwDigitIndex[i];
      awValue[dwIndex] = (awValue[dwIndex] * 10) + (WORD)(*psz - L'0');
   } // i

   // fill in some of the structure
   LOGLINE ll;
   memset (&ll, 0, sizeof(ll));
   ll.dwDate = ((DWORD)awValue[0] << 16) | ((DWORD)awValue[1] << 8) | (DWORD)awValue[2]; // YMD
   ll.dwTime = ((DWORD)awValue[3] << 24) | ((DWORD)awValue[4] << 16) | ((DWORD)awValue[5] << 8) | (DWORD)awValue[6]; // HMSH
   ll.iOffset = iLoc;

   // repeat until find colon
   GUID *pg;
   WCHAR szTemp[sizeof(GUID)*2+1];
   while (TRUE) {
      psz = (PWSTR)MemoryFromFile (iCur, sizeof(WCHAR));
      if (!psz)
         return iCur;   // error
      if (psz[0] == L':') {
         // found ending
         iCur += sizeof(WCHAR);
         break;
      }

      // else, must have comma
      if (psz[0] != L',')
         return iCur;   // error
      iCur += sizeof(WCHAR);

      // read in more chars
      dwSize = (2 + sizeof(GUID)*2) * sizeof(WCHAR);
      psz = (PWSTR)MemoryFromFile (iCur, dwSize);
      if (!psz)
         return iCur;   // error
      if (psz[1] != L'=')
         return iCur;   // error
      switch (psz[0]) {
      case 'a':   // actor
         pg = &ll.gActor;
         break;
      case 'o':   // object
         pg = &ll.gObject;
         break;
      case 'r':   // room
         pg = &ll.gRoom;
         break;
      case 'u':   // user
         pg = &ll.gUser;
         break;
      default:
         return iCur;   // error
      } // swtich
      psz+= 2;

      // get the binary
      memcpy (szTemp, psz, sizeof(GUID)*2*sizeof(WCHAR));
      szTemp[sizeof(GUID)*2] = 0;
      if (sizeof(GUID) != MMLBinaryFromString (szTemp, (PBYTE) pg, sizeof(GUID)))
         return iCur;   // error

      // accept
      iCur += dwSize;
   } // while TRUE

   ll.dwOffsetData = (DWORD) (iCur - ll.iOffset);

   // add it
   m_lLOGLINE.Add (&ll);

   // done
   return iCur;
}


/*************************************************************************************
CTextLogFile::FindNextLineStart - Searches from the current position to find the
start of the next line, which is "\r\n@"

inputs
   __int64        iLoc - Current location
returns
   __int64 - New location, or -1 if EOF
*/
__int64 CTextLogFile::FindNextLineStart (__int64 iLoc)
{
   size_t dwSize;
tryagain:
   PWSTR psz = (PWSTR)MemoryFromFile (iLoc, 256, &dwSize);  // make smaller than min load size so don't load as often
   if (!psz)
      return -1;  // end of data
   dwSize /= sizeof(WCHAR);   // convert to WCHAR
   if (dwSize < 3)
      return -1;  // not enough data
   DWORD i;
   for (i = 0; i < dwSize; i++) {
      if (psz[i] != L'\r')
         continue;   // need to start with \r 

      // if not enough data then reload from here
      if (i+3 >= dwSize) {
         iLoc += (__int64)(i*sizeof(WCHAR));
         goto tryagain;
      }

      // look for \n and @
      if ((psz[i+1] != L'\n') || (psz[i+2] != L'@'))
         continue;   // not right

      // else, have a match
      return iLoc + (__int64)((i+2) * sizeof(WCHAR));
   } // i

   // if get here and can't find then load in some more
   // NOTE: Dont expect this to get called
   iLoc += (__int64)(dwSize*sizeof(WCHAR));
   goto tryagain;
}


/*************************************************************************************
CTextLogFile::ThreadProc - Handles the thread
*/
void CTextLogFile::ThreadProc (void)
{
   // open the file
   WORD wHeader;
   m_fReadOnly = FALSE;
   m_File = _wfopen ((PWSTR)m_memScratch.p, L"r+b");   // try to open for reading and writing
   if (!m_File) {
      // try to open for readinf only
      m_File = _wfopen ((PWSTR)m_memScratch.p, L"rb");
      if (m_File)
         m_fReadOnly = TRUE;  // since exists, but cant open for writing
   }
   if (!m_File) {
      // create
      m_File = _wfopen ((PWSTR)m_memScratch.p, L"w+b");
      if (m_File) {
         wHeader = 0xfeff;
         fwrite (&wHeader, sizeof(wHeader), 1, m_File);
         m_fJustWrote = TRUE;
      }
   }
   if (!m_File)
      m_fReadOnly = TRUE;  // since cant write

   // make sure there's a header
   if (m_File) {
      fflush (m_File);  // BUGFIX - Adding thse just in case affects fseek() or ftell()
      fseek (m_File, 0, SEEK_SET);
      wHeader = 0;
      fread (&wHeader, sizeof(wHeader), 1, m_File);
      m_fJustRead = TRUE;
      if (wHeader != 0xfeff) {
         // error
         fclose (m_File);
         m_File = NULL;
      }
   }

   // find out how large the file is
   if (m_File) {
      fflush (m_File);  // BUGFIX - Adding thse just in case affects fseek() or ftell()
      fseek (m_File, 0, SEEK_END);
      m_iSize = ftell (m_File);

      m_iLoc = sizeof(wHeader);
      fseek (m_File, m_iLoc, SEEK_SET);
   }

   // fill in info about line locs
   __int64 iCur = sizeof(wHeader);
   while (iCur > 0) {
      iCur = ReadLineHeader (iCur);
      if (iCur < 0)
         break;

      // find start of next line
      iCur = FindNextLineStart (iCur);
   } // while can read in lines


   // wait for notifications of events and handle
   LOGQUEUE lqCur;
   PLOGQUEUE plq;
   DWORD i,j;
   while (TRUE) {
      EnterCriticalSection (&m_CritSec);
      m_fWorking = FALSE;
      
      // siganl that have moved on
      if (m_fWantSignal) {
         SetEvent (m_hSignalFromThread);
         m_fWantSignal = FALSE;
      }

      // get from queues
      for (j = 0; j < 2; j++) {
         if (!m_alLOGQUEUE[j].Num())
            continue;

         // found one
         plq = (PLOGQUEUE)m_alLOGQUEUE[j].Get(0);
         lqCur = *plq;
         m_alLOGQUEUE[j].Remove (0);

         // if want to quit then ignore searches
         if (m_fWantToQuit && (lqCur.dwAction == 2)) {
            DeleteQueue (&lqCur);
            j--;
            continue;
         }

         break;   // since found what looking for
      } // j

      // if didn't find anything then...
      if (j >= 2) {
         if (m_fWantToQuit) {
            LeaveCriticalSection (&m_CritSec);
            break;   // from while
         }

         // else, wait
         LeaveCriticalSection (&m_CritSec);
         if (WAIT_TIMEOUT == WaitForSingleObject (m_hSignalToThread, m_fNeedToFlush ? 1000 : INFINITE)) {
            if (m_fNeedToFlush) {
               fflush (m_File);
               m_fNeedToFlush = FALSE;
            }
         }
         continue;
      }

      m_fWorking = TRUE;
      LeaveCriticalSection (&m_CritSec);

      if (m_File) switch (lqCur.dwAction) {
      case 0:  // write data
         {
            // make sure to add to list
            LOGLINE ll;
            memset (&ll, 0, sizeof(ll));
            ll.dwDate = lqCur.dwDate;
            ll.dwTime = lqCur.dwTime;
            ll.gActor = lqCur.gActor;
            ll.gObject = lqCur.gObject;
            ll.gRoom = lqCur.gRoom;
            ll.gUser = lqCur.gUser;
            ll.iOffset = m_iSize;

            // write out info about date, time, specific object info
            if (m_iSize > sizeof(WCHAR))
               InternalFWrite (L"\r\n");
            WCHAR szTemp[64];
            WORD wYear = ll.dwDate >> 16;
            WORD wMonth = (ll.dwDate >> 8) & 0xff;
            WORD wDay = ll.dwDate & 0xff;
            WORD wHour = (ll.dwTime >> 24) & 0xff;
            WORD wMinute = (ll.dwTime >> 16) & 0xff;
            WORD wSecond = (ll.dwTime >> 8) & 0xff;
            WORD wHund = ll.dwTime && 0xff;
            swprintf (szTemp, L"@%.4d%.2d%.2d%.2d%.2d%.2d%.2d",
               (int)wYear, (int)wMonth, (int)wDay, (int)wHour, (int)wMinute, (int)wSecond, (int)wHund);
            InternalFWrite (szTemp);

            if (!IsEqualGUID(GUID_NULL, ll.gActor)) {
               szTemp[0] = L',';
               szTemp[1] = L'a';
               szTemp[2] = L'=';
               MMLBinaryToString ((PBYTE)&ll.gActor, sizeof(GUID), szTemp+3);
               InternalFWrite (szTemp);
            }

            if (!IsEqualGUID(GUID_NULL, ll.gObject)) {
               szTemp[0] = L',';
               szTemp[1] = L'o';
               szTemp[2] = L'=';
               MMLBinaryToString ((PBYTE)&ll.gObject, sizeof(GUID), szTemp+3);
               InternalFWrite (szTemp);
            }

            if (!IsEqualGUID(GUID_NULL, ll.gRoom)) {
               szTemp[0] = L',';
               szTemp[1] = L'r';
               szTemp[2] = L'=';
               MMLBinaryToString ((PBYTE)&ll.gRoom, sizeof(GUID), szTemp+3);
               InternalFWrite (szTemp);
            }

            if (!IsEqualGUID(GUID_NULL, ll.gUser)) {
               szTemp[0] = L',';
               szTemp[1] = L'u';
               szTemp[2] = L'=';
               MMLBinaryToString ((PBYTE)&ll.gUser, sizeof(GUID), szTemp+3);
               InternalFWrite (szTemp);
            }

            InternalFWrite (L":");
            ll.dwOffsetData = (DWORD)(m_iSize - ll.iOffset);
            InternalFWrite ((PWSTR)lqCur.pMem->p);

            // BUGFIX - Move add until afterwards
            m_lLOGLINE.Add (&ll);

         }
         break;

      case 1:  // reading line of data
         {
            // if beyond edge then error
            PLOGLINE pll = (PLOGLINE)m_lLOGLINE.Get(lqCur.dwLine);
            if (!pll) {
               lqCur.pLOGLINE->iOffset = -1;  // to report error
               lqCur.pMem = NULL; // so dont try to delete
               SetEvent (lqCur.hSignalFromThread);
               break;
            }

            // copy info over
            memcpy (lqCur.pLOGLINE, pll, sizeof(*pll));

            // how large is string
            __int64 iStart = pll->iOffset + (__int64)pll->dwOffsetData;
            __int64 iEnd;
            if (lqCur.dwLine+1 < m_lLOGLINE.Num())
               iEnd = pll[1].iOffset - sizeof(WCHAR)*2;  // remove \r\n
            else
               iEnd = m_iSize;
            DWORD dwSize = (iEnd > iStart) ? (DWORD)(iEnd - iStart) : 0;
               // BUGFIX - Seemed to be a problem with offset, so safety
            if (!lqCur.pMem->Required (dwSize+sizeof(WCHAR))) {
               lqCur.pLOGLINE->iOffset = -1;  // to report error
               SetEvent (lqCur.hSignalFromThread);
               break;
            }

            // read in
            InternalFRead (iStart, dwSize, lqCur.pMem->p);
            ((WCHAR*)lqCur.pMem->p)[dwSize/sizeof(WCHAR)] = 0;  // NULL terminate
            lqCur.pMem = NULL; // so dont try to delete


            // send notification to even that done
            SetEvent (lqCur.hSignalFromThread);
         }
         break;

      case 3:  // request number of lines
         // send notification with number of lines
         *lqCur.pdwNumLines = m_lLOGLINE.Num();
         SetEvent (lqCur.hSignalFromThread);
         break;

      case 2:  // searching
         {
            DWORD dwSlowOperations = 0;
            BOOL  fNotYetDone = FALSE;

            BOOL fWantActor = !IsEqualGUID(lqCur.gActor, GUID_NULL);
            BOOL fWantObject = !IsEqualGUID(lqCur.gObject, GUID_NULL);
            BOOL fWantRoom = !IsEqualGUID(lqCur.gRoom, GUID_NULL);
            BOOL fWantUser = !IsEqualGUID(lqCur.gUser, GUID_NULL);

            PLOGLINE pll = (PLOGLINE)m_lLOGLINE.Get(lqCur.dwLine);
            if (pll) for (; lqCur.dwLine < m_lLOGLINE.Num(); lqCur.dwLine++, pll++) {
               // if time is less than starting time then continue
               if (pll->dwDate < lqCur.dwDate)
                  continue;
               if ((pll->dwDate == lqCur.dwDate) && (pll->dwTime < lqCur.dwTime))
                  continue;

               // if time is more then starting time then stop here, since
               // time should be sequentiall
               if (pll->dwDate > lqCur.dwDateEnd)
                  break;
               if ((pll->dwDate == lqCur.dwDateEnd) && (pll->dwTime >= lqCur.dwTimeEnd))
                  break;

               // if want guid matches
               if (fWantActor && !IsEqualGUID(lqCur.gActor, pll->gActor))
                  continue;
               if (fWantObject && !IsEqualGUID(lqCur.gObject, pll->gObject))
                  continue;
               if (fWantRoom && !IsEqualGUID(lqCur.gRoom, pll->gRoom))
                  continue;
               if (fWantUser && !IsEqualGUID(lqCur.gUser, pll->gUser))
                  continue;

               // if want to compare string then do so
               if (lqCur.pMem) {
                  dwSlowOperations++;
                  DWORD dwSize;
                  if (lqCur.dwLine+1 < m_lLOGLINE.Num())
                     dwSize = (DWORD) (pll[1].iOffset - pll[0].iOffset);
                  else
                     dwSize = (DWORD) (m_iSize - pll[0].iOffset);
                  if (!m_memScratch.Required (dwSize+sizeof(WCHAR)))
                     continue;   // error
                  InternalFRead (pll[0].iOffset, dwSize, m_memScratch.p);
                  ((PWSTR)m_memScratch.p)[dwSize/sizeof(WCHAR)] = 0; // null terminate

                  if (!MyStrIStr ((PWSTR)m_memScratch.p, (PWSTR)lqCur.pMem->p))
                     continue; // not found
               }

               // if get here, then found, so add
               LOGFILESEARCH lfs;
               memset (&lfs, 0, sizeof(lfs));
               lfs.dwIDDate = m_dwIDDate;
               lfs.dwIDTime = m_dwIDTime;
               lfs.dwLine = lqCur.dwLine;
               lqCur.plLOGFILESEARCH->Add (&lfs);


               // if too many slow operations then break to allow other processing
               if (dwSlowOperations > 100) {
                  // see if anything queued up in primary queue
                  EnterCriticalSection (&m_CritSec);
                  if (m_alLOGQUEUE[0].Num()) {
                     fNotYetDone = TRUE;
                     LeaveCriticalSection (&m_CritSec);
                     break;
                  }
                  LeaveCriticalSection (&m_CritSec);

                  // else, nothing so keep going
                  dwSlowOperations = 0;
                  break;
               } // check for too many slow operations
            } // while TRUE

            // if not yet done, add self back into queue and continue later
            if (fNotYetDone) {
               EnterCriticalSection (&m_CritSec);
               m_alLOGQUEUE[1].Insert (0, &lqCur);
               LeaveCriticalSection (&m_CritSec);
               memset (&lqCur, 0, sizeof(lqCur));  // so dont free up any memory
               break;
            }

            // if get here then we're done and need to send it out
            PostMessage (lqCur.hWndNotify, lqCur.uMsg, NULL, (LPARAM)lqCur.pUserData);
         }
         break;
      } // switch action

      // free up queue memory
      DeleteQueue (&lqCur);
   } // while TRUE
   

   // done
   if (m_File)
      fclose (m_File);

   // free up the queues
   EnterCriticalSection (&m_CritSec);
   for (j = 0; j < 2; j++) {
      PLOGQUEUE plq = (PLOGQUEUE) m_alLOGQUEUE[j].Get(0);
      for (i = 0; i < m_alLOGQUEUE[j].Num(); i++, plq++)
         DeleteQueue (plq);
      m_alLOGQUEUE[j].Clear();
   } // j
   LeaveCriticalSection (&m_CritSec);
}


/*************************************************************************************
CTextLogFile::Write - Writes a new line of info onto the end.

inputs
   DWORD          dwDate - Date (year in high word, then month, then day)
   DWORD          dwTime - Hour in high byte, then minute, second, 100th second
   PWSTR          psz - String to write
   GUID           *pgActor - If not NULL, actor guid
   GUID           *pgObject - If not NULL, object guid
   GUID           *pgRoom - If not NULL, room guid
   GUID           *pgUser - If not NULL, user guid
returns
   BOOL - TRUE if success
*/
BOOL CTextLogFile::Write (DWORD dwDate, DWORD dwTime, PWSTR psz, GUID *pgActor,
                          GUID *pgObject, GUID *pgRoom, GUID *pgUser)
{
redo:
   EnterCriticalSection (&m_CritSec);
   if (m_alLOGQUEUE[0].Num() > 1000) {
      // if too much then just spin
      m_fWantSignal = TRUE;
      LeaveCriticalSection (&m_CritSec);
      WaitForSingleObject (m_hSignalFromThread, INFINITE);
      goto redo;
   }

   // at it
   LOGQUEUE lq;
   memset (&lq, 0, sizeof(lq));
   lq.dwAction = 0;
   lq.dwDate = dwDate;
   lq.dwTime = dwTime;
   lq.pMem = new CMem;
   if (!lq.pMem) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }
   MemZero (lq.pMem);
   MemCat (lq.pMem, psz);
   if (pgActor)
      lq.gActor = *pgActor;
   if (pgObject)
      lq.gObject = *pgObject;
   if (pgRoom)
      lq.gRoom = *pgRoom;
   if (pgUser)
      lq.gUser = *pgUser;
   m_alLOGQUEUE[0].Add (&lq);

   // set the event
   SetEvent (m_hSignalToThread);

   LeaveCriticalSection (&m_CritSec);

   return TRUE;

}


/*************************************************************************************
CTextLogFile::Read - Reads a line of data.

inputs
   DWORD          dwLine - Line number to read
   DWORD          *pdwDate - Date (year in high word, then month, then day)
   DWORD          *pdwTime - Hour in high byte, then minute, second, 100th second
   PCMem          pMemText - Received the text for the line.
   GUID           *pgActor - If not NULL, filled with actor guid
   GUID           *pgObject - If not NULL, filled with object guid
   GUID           *pgRoom - If not NULL, filled with room guid
   GUID           *pgUser - If not NULL, filled with user guid
returns
   BOOL - TRUE if success
*/
BOOL CTextLogFile::Read (DWORD dwLine, DWORD *pdwDate, DWORD *pdwTime, PCMem pMemText, GUID *pgActor,
                          GUID *pgObject, GUID *pgRoom, GUID *pgUser)
{
   HANDLE hSignal = CreateEvent (NULL, FALSE, FALSE, NULL);
   if (!hSignal)
      return FALSE;

   EnterCriticalSection (&m_CritSec);

   // add it
   LOGQUEUE lq;
   LOGLINE ll;
   memset (&lq, 0, sizeof(lq));
   lq.dwAction = 1;
   lq.dwLine = dwLine;
   lq.hSignalFromThread = hSignal;
   lq.pLOGLINE = &ll;
   lq.pMem = pMemText;
   m_alLOGQUEUE[0].Add (&lq);

   // set the event
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);

   // wait for signal back
   WaitForSingleObject (hSignal, INFINITE);
   DeleteObject (hSignal);

   if (ll.iOffset < 0)
      return FALSE;  // cant find
   if (pdwDate)
      *pdwDate = ll.dwDate;
   if (pdwTime)
      *pdwTime = ll.dwTime;
   if (pgActor)
      *pgActor = ll.gActor;
   if (pgObject)
      *pgObject = ll.gObject;
   if (pgRoom)
      *pgRoom = ll.gRoom;
   if (pgUser)
      *pgUser = ll.gUser;

   return TRUE;
}


/*************************************************************************************
CTextLogFile::NumLines - Request the number of lines. NOTE: This is a slow-ish
function so don't use too often.

returns
   DWORD - Number of lines, or -1 if error
*/
DWORD CTextLogFile::NumLines (void)
{
   HANDLE hSignal = CreateEvent (NULL, FALSE, FALSE, NULL);
   if (!hSignal)
      return FALSE;

   EnterCriticalSection (&m_CritSec);

   // add it
   LOGQUEUE lq;
   DWORD dw;
   memset (&lq, 0, sizeof(lq));
   lq.dwAction = 3;
   lq.pdwNumLines = &dw;
   lq.hSignalFromThread = hSignal;
   m_alLOGQUEUE[0].Add (&lq);

   // set the event
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);

   // wait for signal back
   WaitForSingleObject (hSignal, INFINITE);
   DeleteObject (hSignal);

   return dw;
}


/*************************************************************************************
CTextLogFile::Search - Initiates a search through the file.

inputs
   DWORD          dwDateStart - starting date
   DWORD          dwTimeStart - starting time, >= this
   DWORD          dwDateEnd - ending date
   DWORD          dwTimeEnd - ending time. Entries < this
   GUID           *pgActor - If not NULL, actor match required     
   GUID           *pgObject - If not NULL, object match required
   GUID           *pgRoom - If not NULL, room match required 
   GUID           *pgUser - If not NULL, user match required
   PWSTR          pszString - String to match. Case INsensative.
   PCListFixed    plLOGFILESEARCH - Matches are added here. Make sure only one thread at a timea accessses this.
   HWND           hWndNotify - When done with search, post a message to this window
   UINT           uMsg - Message sent. LPARAM will be filled with pUserData.
   PVOID          pUserData - User data associated with request.
returns
   BOOL - TRUE if successfully added, FALSE if failed to add to queue
*/
BOOL CTextLogFile::Search (DWORD dwDateStart, DWORD dwTimeStart, DWORD dwDateEnd, DWORD dwTimeEnd,
                           GUID *pgActor, GUID *pgObject, GUID *pgRoom, GUID *pgUser,
                           PWSTR pszString, PCListFixed plLOGFILESEARCH,
                           HWND hWndNotify, UINT uMsg, PVOID pUserData)
{
   // fill in into
   LOGQUEUE lq;
   memset (&lq, 0, sizeof(lq));
   lq.dwAction = 2;
   lq.dwDate = dwDateStart;
   lq.dwTime = dwTimeStart;
   lq.dwDateEnd = dwDateEnd;
   lq.dwTimeEnd = dwTimeEnd;
   lq.dwLine = 0;
   if (pgActor)
      lq.gActor = *pgActor;
   if (pgObject)
      lq.gObject = *pgObject;
   if (pgRoom)
      lq.gRoom = *pgRoom;
   if (pgUser)
      lq.gUser = *pgUser;
   if (pszString && pszString[0]) {
      lq.pMem = new CMem;
      if (!lq.pMem)
         return FALSE;
      MemZero (lq.pMem);
      MemCat (lq.pMem, pszString);
   }
   lq.plLOGFILESEARCH = plLOGFILESEARCH;
   lq.hWndNotify = hWndNotify;
   lq.uMsg = uMsg;
   lq.pUserData = pUserData;

   EnterCriticalSection (&m_CritSec);
   m_alLOGQUEUE[0].Add (&lq);
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);

   return TRUE;
}



/*************************************************************************************
CTextLogFile::IsBusy - Detects if the log file is busy doing processing. Generally,
don't delete if busy when clearing out old log files.
*/
BOOL CTextLogFile::IsBusy (void)
{
   EnterCriticalSection (&m_CritSec);
   BOOL fBusy = m_fWorking || m_alLOGQUEUE[0].Num() || m_alLOGQUEUE[1].Num();
   LeaveCriticalSection (&m_CritSec);
   return fBusy;
}





/*************************************************************************************
CTextLog::Constructor and destructor */
CTextLog::CTextLog (void)
{
   m_lTEXTLOGFILE.Init (sizeof(TEXTLOGFILE));
   m_gAutoActor = GUID_NULL;
   m_gAutoRoom = GUID_NULL;
   m_gAutoUser = GUID_NULL;
   m_fEnabled = TRUE;
   InitializeCriticalSection (&m_CritSec);
}

CTextLog::~CTextLog (void)
{
   EnterCriticalSection (&m_CritSec);
   DWORD i;
   PTEXTLOGFILE ptlf = (PTEXTLOGFILE)m_lTEXTLOGFILE.Get(0);
   for (i = 0; i < m_lTEXTLOGFILE.Num(); i++, ptlf++)
      if (ptlf->pTLF)
         delete ptlf->pTLF;
   m_lTEXTLOGFILE.Clear();
   LeaveCriticalSection (&m_CritSec);
   DeleteCriticalSection (&m_CritSec);
}

/*************************************************************************************
CTextLog::Init - Initializes the text log to use the given directory.

inputs
   PWSTR       pszDir - Directory.
returns
   BOOL - TRUE if success
*/
BOOL CTextLog::Init (PWSTR pszDir)
{
   if (m_lTEXTLOGFILE.Num())
      return FALSE;  // already intiialized

   // remember the directory
   MemZero (&m_memDir);
   MemCat (&m_memDir, pszDir);
   DWORD dwLen = (DWORD)wcslen((PWSTR)m_memDir.p);
   if (dwLen && (((PWSTR)m_memDir.p)[dwLen-1] != '\\')) {
      MemCat (&m_memDir, L"\\");
      dwLen++;
   }

   // create just in case
   CreateDirectoryW (pszDir, NULL);

   // enumerate contents
   MemCat (&m_memDir, L"log*.txt");
   WIN32_FIND_DATAW fd;
   HANDLE hFile = FindFirstFileW ((PWSTR)m_memDir.p, &fd);
   if (hFile == INVALID_HANDLE_VALUE)
      goto done;
   TEXTLOGFILE tlf;
   memset (&tlf, 0, sizeof(tlf));
   while (TRUE) {
      // try to get the digits from this
      if (wcslen(fd.cFileName) != (7 + 10))
         goto next;  // wrong len

      if (IDStringToDateTime (fd.cFileName + 3, 10, &tlf.dwIDDate, &tlf.dwIDTime))
         m_lTEXTLOGFILE.Add (&tlf);

next:
      // get the text one
      if (!FindNextFileW (hFile, &fd))
         break;
   }
   FindClose (hFile);


done:
   // restore name
   ((PWSTR)m_memDir.p)[dwLen] = 0;

   return TRUE;
}


/*************************************************************************************
CTextLog::GenerateFileName - Given the date and time, generates the filename.

inputs
   DWORD          dwIDDate - Date ID
   DWORD          dwIDTime - Time ID. Ignores minutes, seconds, hundredths
   PCMem          pMem - Filled in with file name
returns
   BOOL - TRUE if success
*/
BOOL CTextLog::GenerateFileName (DWORD dwIDDate, DWORD dwIDTime, PCMem pMem)
{
   WCHAR szTemp[32];
   wcscpy (szTemp, L"log");
   DateTimeToIDString (dwIDDate, dwIDTime, szTemp + wcslen(szTemp));
   wcscat (szTemp, L".txt");

   MemZero (pMem);
   MemCat (pMem, (PWSTR)m_memDir.p);
   MemCat (pMem, szTemp);
   return TRUE;
}


/*************************************************************************************
CTextLog::Delete - Deletes a file. This will fail if the file is in use (being
written to, for example)

inputs
   DWORD          dwIDDate - Date
   DWORD          dwIDTime - Time. minutes, seconds, hundredths are ignored
returns
   BOOL - TRUE if deleted. FALSE if couldnt' delete, or wasn't found
*/
BOOL CTextLog::Delete (DWORD dwIDDate, DWORD dwIDTime)
{
   EnterCriticalSection (&m_CritSec);

   dwIDTime &= 0xff000000; // to ignore low stuff
   DWORD i;
   PTEXTLOGFILE ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
   for (i = 0; i < m_lTEXTLOGFILE.Num(); i++, ptlf++)
      if ((ptlf->dwIDDate == dwIDDate) && (ptlf->dwIDTime == dwIDTime))
         break;
   if (i >= m_lTEXTLOGFILE.Num()) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;  // couldnt find
   }

   // else, found

   // if there's an object, make sure can delete
   if (ptlf->pTLF) {
      if (ptlf->pTLF->IsBusy()) {
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }

      delete ptlf->pTLF;
      ptlf->pTLF = NULL;
   }

   // try to delete the file
   CMem mem;
   if (!GenerateFileName (dwIDDate, dwIDTime, &mem)) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;  // shouldnt happen
   }
   BOOL fRet = DeleteFileW ((PWSTR)mem.p) ? TRUE : FALSE;
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}



/*************************************************************************************
CTextLog::Enum - Initializes plDATETIME to 2*sizeof(DWORD), and fills it in
with the date (1st DWORD) and time (2nd DWORD) of each file.

inputs
   PCListFixed       plDATETIME - Initialized and then filled i
returns
   BOOL - TRUE if success
*/
BOOL CTextLog::Enum (PCListFixed plDATETIME)
{
   EnterCriticalSection (&m_CritSec);

   plDATETIME->Init (2 * sizeof(DWORD));
   DWORD adw[2];
   DWORD i;
   PTEXTLOGFILE ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
   for (i = 0; i < m_lTEXTLOGFILE.Num(); i++, ptlf++) {
      adw[0] = ptlf->dwIDDate;
      adw[1] = ptlf->dwIDTime;
      plDATETIME->Add (&adw[0]);
   } // i

   LeaveCriticalSection (&m_CritSec);

   return TRUE;
}


/*************************************************************************************
CTextLog::UncacheOld - This looks through old text logs and uncaches any that haven't
been used for awhile. Call this whenever data is logged, and occasionally for
other functions.

NOTE: Assume already in critical section

inputs
   PFILETIME         pft - Current file time (GMT)
returns
   none
*/
#define MAXFILECACHE     4     // dont cache too many
void CTextLog::UncacheOld (PFILETIME pft)
{
  __int64 iTime = *((__int64*)pft);
  iTime -= (__int64) 10000000 /* 10mil => 1 sec */ * (__int64) 60 * (__int64) 10;   // 10 minutes
  PFILETIME pftOld = (PFILETIME) &iTime;

  // loop
  DWORD i;
  PTEXTLOGFILE ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
  FILETIME ftOldest;
  PTEXTLOGFILE pOldest = NULL;
  BOOL fDeleted = FALSE;
  DWORD dwCached = 0;
  for (i = 0; i < m_lTEXTLOGFILE.Num(); i++, ptlf++) {
     if (!ptlf->pTLF)
        continue;

     // compare to the oldest too
     dwCached++;
     if (!pOldest || CompareFileTime(&ptlf->pTLF->m_ftLastUsed, &ftOldest) < 0) {
        pOldest = ptlf;
        ftOldest = ptlf->pTLF->m_ftLastUsed;
     }

     if (CompareFileTime (&ptlf->pTLF->m_ftLastUsed, pftOld) >= 0)
        continue; // not old enough

     // else, old, so remove
     if (ptlf->pTLF->IsBusy())
        continue; // still busy, so dont remove
     delete ptlf->pTLF;
     ptlf->pTLF = NULL;
     fDeleted = TRUE;
  } // i

  // if didn't delete then consider deleting one of the oldest
  // just to make sure don't have too many lying around
  if (!fDeleted && (dwCached > MAXFILECACHE) && pOldest && !pOldest->pTLF->IsBusy()) {
     delete pOldest->pTLF;
     pOldest->pTLF = NULL;
  }
}


/*************************************************************************************
CTextLog::FILETIMEToDateTime - Converts a filetime to a date and time.

inputs
   FILETIME       *pft - File time
   DWORD          *pdwDate - Filled with Date
   DWORD          *pdwTime - Filled with Time
returns
   none
*/
void CTextLog::FILETIMEToDateTime (FILETIME *pft, DWORD *pdwDate, DWORD *pdwTime)
{
   // figure out date and time
   SYSTEMTIME st;
   FileTimeToSystemTime (pft, &st);
   *pdwDate = ((DWORD)st.wYear << 16) | ((DWORD)st.wMonth << 8) | (DWORD)st.wDay;
   *pdwTime = ((DWORD)st.wHour << 24) | ((DWORD)st.wMinute << 16) | ((DWORD)st.wSecond << 8) | (DWORD)(st.wMilliseconds/10);
}


/*************************************************************************************
CTextLog::DateTimeToFILETIME - Converts a date and time to filetime.

inputs
   DWORD          dwDate - Filled with Date
   DWORD          dwTime - Filled with Time
   FILETIME       *pft - File time
returns
   none
*/
void CTextLog::DateTimeToFILETIME (DWORD dwDate, DWORD dwTime, FILETIME *pft)
{
   SYSTEMTIME st;
   memset (&st, 0, sizeof(st));
   st.wYear = (WORD)(dwDate >> 16);
   st.wMonth = (WORD)((dwDate >> 8) & 0xff);
   st.wDay = (WORD)(dwDate & 0xff);
   st.wHour = (WORD)(dwTime >> 24);
   st.wMinute = (WORD)((dwTime >> 16) & 0xff);
   st.wSecond = (WORD)((dwTime >> 8) & 0xff);
   st.wMilliseconds = (WORD)(dwTime & 0xff) * 10;

   SystemTimeToFileTime (&st, pft);
}


/*************************************************************************************
CTextLog::IDStringToDateTime - Converts a file ID string to date/time.

inputs
   PWSTR       pszID - ID string
   DWORD       dwLen - Number of characters in pszID. If 0 then does wclen
   DWORD       *pdwDate - Filled with date
   DWORD       *pdwTime - Filled with time
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CTextLog::IDStringToDateTime (PWSTR pszID, DWORD dwLen, DWORD *pdwDate, DWORD *pdwTime)
{
   if (!dwLen)
      dwLen = (DWORD)wcslen(pszID);

   if (dwLen != 10)
      return FALSE;

   DWORD i, dwIndex;
   WORD aw[4];
   WCHAR c;
   DWORD adwIndex[10] = {0,0,0,0, 1,1, 2,2, 3,3};
   memset (&aw, 0, sizeof(aw));
   for (i = 0; i < 10; i++) {
      c = pszID[i];
      if ((c < L'0') || (c > L'9'))
         return FALSE;  // error in name
      dwIndex = adwIndex[i];
      aw[dwIndex] = aw[dwIndex] * 10 + (WORD)(c - L'0');
   } // i

   // convert to date and time
   *pdwDate = ((DWORD)aw[0] << 16) | ((DWORD)aw[1] << 8) | (DWORD)aw[2];
   *pdwTime = ((DWORD)aw[3] << 24);   // only use hours, not minutes, seconds, or hundreths of second

   return TRUE;
}


/*************************************************************************************
CTextLog::DateTimeToIDString - Converts a date/time to a file ID string.

inputs
   DWORD       dwDate - Date
   DWORD       dwTime - Time. Minutes, seconds, hundredths ignored
   PWSTR       pszID - ID string filled in. Need 11 chars.
returns
   none
*/
void CTextLog::DateTimeToIDString (DWORD dwDate, DWORD dwTime, PWSTR pszID)
{
   swprintf (pszID, L"%.4d%.2d%.2d%.2d",
      (int)(dwDate >> 16), (int)((dwDate >> 8) & 0xff), (int)(dwDate & 0xff),
      (int)(dwTime >> 24));
}

/*************************************************************************************
CTextLog::Log - Logs a string of data

inputs
   PWSTR          psz - String
   GUID           pgActor - If not NULL, actor associated with
   GUID           pgObject - Same
   GUID           pgRoom - Same
   GUID           pgUser - Same
returns
   BOOL - TRUE if success
*/
BOOL CTextLog::Log (PWSTR psz, GUID *pgActor, GUID *pgObject, GUID *pgRoom, GUID *pgUser)
{
   EnterCriticalSection (&m_CritSec);

   // get the time
   FILETIME ft;
   GetSystemTimeAsFileTime (&ft);

   // clear old cache out
   UncacheOld (&ft);

   if (!m_fEnabled) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }

   // figure out date and time
   DWORD dwDate, dwTime;
   FILETIMEToDateTime (&ft, &dwDate, &dwTime);
   DWORD dwIDTime = dwTime & 0xff000000;  // only the high bits

   // find the file
   DWORD i;
   PTEXTLOGFILE ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
   for (i = 0; i < m_lTEXTLOGFILE.Num(); i++)
      if ((ptlf[i].dwIDDate == dwDate) && (ptlf[i].dwIDTime == dwIDTime))
         break;
   if (i >= m_lTEXTLOGFILE.Num()) {
      // create it
      TEXTLOGFILE tlf;
      memset (&tlf, 0, sizeof(tlf));
      tlf.dwIDDate = dwDate;
      tlf.dwIDTime = dwIDTime;
      m_lTEXTLOGFILE.Insert (0, &tlf);
      ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
   }
   else if (i) {
      // move to the top of the list
      TEXTLOGFILE lftemp = ptlf[0];
      ptlf[0] = ptlf[i];
      ptlf[i] = lftemp;
   }

   // if there's no object then create one and initialize
   if (!ptlf->pTLF) {
      CMem mem;

      ptlf->pTLF = new CTextLogFile;
      if (!ptlf->pTLF) {
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }
      if (!GenerateFileName (ptlf->dwIDDate, ptlf->dwIDTime, &mem)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }

      // initialize
      if (!ptlf->pTLF->Init ((PWSTR)mem.p, ptlf->dwIDDate, ptlf->dwIDTime)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }
   }

   // store the last used
   ptlf->pTLF->m_ftLastUsed = ft;

   // write
   BOOL fRet = ptlf->pTLF->Write (dwDate, dwTime, psz, pgActor, pgObject, pgRoom, pgUser);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CTextLog::LogAuto - Logs the string, but uses the actor, room, and user from
the AutoSet() command

inputs
   PWSTR          psz - String to set
   GUID           *pgObject - If not NULL, object assocated
returns
   BOOL - TRUE if success
*/
BOOL CTextLog::LogAuto (PWSTR psz, GUID *pgObject)
{
   return Log (psz, &m_gAutoActor, pgObject, &m_gAutoRoom, &m_gAutoUser);
}



/*************************************************************************************
CTextLog::AutoSet - Sets the GUID for the automatic log.

inputs
   DWORD       dwType - 0 = actor, 1 = room, 2 = user
   GUID        *pg - GUID to use. If NULL, then GUID_NULL will be used
returns
   BOOL - TRUE if success
*/
BOOL CTextLog::AutoSet (DWORD dwType, GUID *pg)
{
   EnterCriticalSection (&m_CritSec);

   GUID gNull = GUID_NULL;
   if (!pg)
      pg = &gNull;

   switch (dwType) {
   case 0: // actor
      m_gAutoActor = *pg;
      break;
   case 1: // room
      m_gAutoRoom = *pg;
      break;
   case 2: // user
      m_gAutoUser = *pg;
      break;
   default:
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }

   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}



/*************************************************************************************
CTextLog::AutoGet - Gets the GUID for the automatic log.

inputs
   DWORD       dwType - 0 = actor, 1 = room, 2 = user
   GUID        *pg - Filled with the GUID, or GUID_NULL if set to blank
returns
   BOOL - TRUE if success
*/
BOOL CTextLog::AutoGet (DWORD dwType, GUID *pg)
{
   EnterCriticalSection (&m_CritSec);

   switch (dwType) {
   case 0: // actor
      *pg = m_gAutoActor;
      break;
   case 1: // room
      *pg = m_gAutoRoom;
      break;
   case 2: // user
      *pg = m_gAutoUser;
      break;
   default:
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }

   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}


/*************************************************************************************
CTextLog::EnableSet - Sets the enable flag. (on by default)

inputs
   BOOL        fEnable - If TRUE, then log data. If FALSE, then ignore all calls
               to log.
returns
   none
*/
void CTextLog::EnableSet (BOOL fEnable)
{
   EnterCriticalSection (&m_CritSec);
   m_fEnabled = fEnable;
   LeaveCriticalSection (&m_CritSec);
}


/*************************************************************************************
CTextLog::EnableGet - Returns the enable flag
*/
BOOL CTextLog::EnableGet (void)
{
   EnterCriticalSection (&m_CritSec);
   BOOL fRet = m_fEnabled;
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CTextLog::Read - Reads a line from the file.

inputs
   DWORD          dwDate - Date ID of the file
   DWORD          dwTime - Time ID of the file. Minutes, seconds, hundredths ignored
   DWORD          dwLine - Line of the file
   DWORD          *pdwDate, *pdwTime - Filled in with the date and time
   PCMem          pMemText - Filled in with the string
   GUID           *pgActor, *pgObject, *pgRoom, *pgUser - Filled in with info
returns
   BOOL - TRUE if success, FALSE if failed to read
*/
BOOL CTextLog::Read (DWORD dwDate, DWORD dwTime, DWORD dwLine, DWORD *pdwDate, DWORD *pdwTime, PCMem pMemText, GUID *pgActor,
                        GUID *pgObject, GUID *pgRoom, GUID *pgUser)
{
   EnterCriticalSection (&m_CritSec);

   dwTime &= 0xff000000;   // to only keep hours

   // get the time and uncache
   FILETIME ft;
   GetSystemTimeAsFileTime (&ft);
   UncacheOld (&ft);

   // find a match
   DWORD i;
   PTEXTLOGFILE ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
   for (i = 0; i < m_lTEXTLOGFILE.Num(); i++, ptlf++)
      if ((ptlf->dwIDDate == dwDate) && (ptlf->dwIDTime == dwTime))
         break;
   if (i >= m_lTEXTLOGFILE.Num()) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }

   // make sure loaded
   if (!ptlf->pTLF) {
      CMem mem;

      ptlf->pTLF = new CTextLogFile;
      if (!ptlf->pTLF) {
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }
      if (!GenerateFileName (ptlf->dwIDDate, ptlf->dwIDTime, &mem)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }

      // initialize
      if (!ptlf->pTLF->Init ((PWSTR)mem.p, ptlf->dwIDDate, ptlf->dwIDTime)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         LeaveCriticalSection (&m_CritSec);
         return FALSE;
      }
   }

   // remember when last used
   ptlf->pTLF->m_ftLastUsed = ft;

   BOOL fRet = ptlf->pTLF->Read (dwLine, pdwDate, pdwTime, pMemText, pgActor, pgObject, pgRoom, pgUser);
   LeaveCriticalSection (&m_CritSec);

   return fRet;
}


/*************************************************************************************
CTextLog::NumLines - Returns the number of lines in the file. NOTE: This is slow
   so don't use it too often.

inputs
   DWORD          dwDate - Date ID of the file
   DWORD          dwTime - Time ID of the file. Minutes, seconds, hundredths ignored
returns
   DWORD - Number of lines, or -1 if error
*/
DWORD CTextLog::NumLines (DWORD dwDate, DWORD dwTime)
{
   EnterCriticalSection (&m_CritSec);

   dwTime &= 0xff000000;   // to only keep hours

   // get the time and uncache
   FILETIME ft;
   GetSystemTimeAsFileTime (&ft);
   UncacheOld (&ft);


   // find a match
   DWORD i;
   PTEXTLOGFILE ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
   for (i = 0; i < m_lTEXTLOGFILE.Num(); i++, ptlf++)
      if ((ptlf->dwIDDate == dwDate) && (ptlf->dwIDTime == dwTime))
         break;
   if (i >= m_lTEXTLOGFILE.Num()) {
      LeaveCriticalSection (&m_CritSec);
      return (DWORD)-1;
   }

   // make sure loaded
   if (!ptlf->pTLF) {
      CMem mem;

      ptlf->pTLF = new CTextLogFile;
      if (!ptlf->pTLF) {
         LeaveCriticalSection (&m_CritSec);
         return (DWORD)-1;
      }
      if (!GenerateFileName (ptlf->dwIDDate, ptlf->dwIDTime, &mem)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         LeaveCriticalSection (&m_CritSec);
         return (DWORD)-1;
      }

      // initialize
      if (!ptlf->pTLF->Init ((PWSTR)mem.p, ptlf->dwIDDate, ptlf->dwIDTime)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         LeaveCriticalSection (&m_CritSec);
         return (DWORD)-1;
      }
   }

   // remember when last used
   ptlf->pTLF->m_ftLastUsed = ft;

   DWORD dwRet = ptlf->pTLF->NumLines ();
   LeaveCriticalSection (&m_CritSec);

   return dwRet;
}


/*************************************************************************************
CTextLog::Search - Initiates a search through the databases. This is an asynchronous
call that will eventually call the callback object, gVMObject, in pVM.

inputs
   DWORD       dwDateStart, dwTimeStart - Starting date/time of the search
   DWORD       dwDateEnd, dwTimeEnd - Ending date/time of the search
   GUID        *pgActor, *pgObject, *pgRoom, *pgUser - Find matches
   PWSTR       pszString - Find case INsensative match for this. Can be NULL.
   HWND        hWndNotify - Window to notify when done with immediate search
   UINT        uMsg - Message. When window receives this message, should call OnMessage() with lParam
   PCMIFLVM    pVM - Virtual machine to eventually call when finished
   PCMIFLVar   pCallback - Callback when done
   PCMIFLVar   pParam - Parameter for callback
returns
   BOOL - TRUE if started search
*/
BOOL CTextLog::Search (DWORD dwDateStart, DWORD dwTimeStart, DWORD dwDateEnd, DWORD dwTimeEnd,
                        GUID *pgActor, GUID *pgObject, GUID *pgRoom, GUID *pgUser,
                        PWSTR pszString,
                        HWND hWndNotify, UINT uMsg, PCMIFLVM pVM, PCMIFLVar pCallback,
                        PCMIFLVar pParam)
{
   EnterCriticalSection (&m_CritSec);

   TEXTLOGSEARCH tls;
   memset (&tls, 0, sizeof(tls));
   tls.dwDateStart = dwDateStart;
   tls.dwTimeStart = dwTimeStart;
   tls.dwDateEnd = dwDateEnd;
   tls.dwTimeEnd = dwTimeEnd;
   if (pgActor)
      tls.gActor = *pgActor;
   if (pgObject)
      tls.gObject = *pgObject;
   if (pgRoom)
      tls.gRoom = *pgRoom;
   if (pgUser)
      tls.gUser = *pgUser;
   tls.plLOGFILESEARCH = new CListFixed;
   if (!tls.plLOGFILESEARCH) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }
   tls.plLOGFILESEARCH->Init (sizeof(LOGFILESEARCH));
   if (pszString) {
      tls.pMemString = new CMem;
      if (tls.pMemString) {
         MemZero (tls.pMemString);
         MemCat (tls.pMemString, pszString);
      }
   }
   tls.hWndNotify = hWndNotify;
   tls.uMsg = uMsg;
   tls.pVM = pVM;
   tls.pCallback = new CMIFLVar;
   tls.pCallback->Set (pCallback);
   tls.pParam = new CMIFLVar;
   tls.pParam->Set (pParam);

   m_lTEXTLOGSEARCH.Add (&tls, sizeof(tls));
   PTEXTLOGSEARCH ptls = (PTEXTLOGSEARCH)m_lTEXTLOGSEARCH.Get(m_lTEXTLOGSEARCH.Num()-1);

   // do processing
   OnMessage (ptls, TRUE);
   LeaveCriticalSection (&m_CritSec);

   return TRUE;
}


/*************************************************************************************
CTextLog::OnMessage - Handles a message coming in indicating that the search
has been completed for a log. This then goes to the next file in the list, and
repeats until there are no more.

inputs
   PTEXTLOGSEARCH       pTLS - Info
   BOOL                 fInCritSec - Set to TRUE if already in the critical section
*/
void CTextLog::OnMessage (PTEXTLOGSEARCH pTLS, BOOL fInCritSec)
{
   if (!fInCritSec)
      EnterCriticalSection (&m_CritSec);

   // uncache
   FILETIME ft;
   PCMIFLVarList pl = NULL;
   GetSystemTimeAsFileTime (&ft);
   UncacheOld (&ft);

tryagain:
   // if the last date/time searched through > the end then done
   if (pTLS->dwDateLast > pTLS->dwDateEnd)
      goto donewithsearch;
   if ((pTLS->dwDateLast == pTLS->dwDateEnd) && (pTLS->dwTimeLast >= pTLS->dwTimeEnd))
      goto donewithsearch;

   // search through the list, finding the oldest file whose date is more
   // recent than last date/time, and start date/time
   __int64 iLast = ((__int64)pTLS->dwDateLast << 32) | (__int64)pTLS->dwTimeLast;
   __int64 iStart = ((__int64)pTLS->dwDateStart << 32) | (__int64)(pTLS->dwTimeStart & 0xff000000);
   __int64 iCur;
   __int64 iMatch = -1;
   DWORD i;
   PTEXTLOGFILE ptlf = (PTEXTLOGFILE) m_lTEXTLOGFILE.Get(0);
   PTEXTLOGFILE ptlfBest;
   for (i = 0; i < m_lTEXTLOGFILE.Num(); i++, ptlf++) {
      iCur = ((__int64)ptlf->dwIDDate << 32) | (__int64)ptlf->dwIDTime;

      if (iCur <= iLast)
         continue;   // already did
      if (iCur < iStart)
         continue;   // before start time

      if ((iMatch == -1) || (iCur < iMatch)) {
         iMatch = iCur;
         ptlfBest = ptlf;
      }
   } // i
   if (iMatch == -1)
      goto donewithsearch;
   ptlf = ptlfBest;

   // remember this as last
   pTLS->dwDateLast = ptlf->dwIDDate;
   pTLS->dwTimeLast = ptlf->dwIDTime;

   // make sure loaded
   if (!ptlf->pTLF) {
      CMem mem;

      ptlf->pTLF = new CTextLogFile;
      if (!ptlf->pTLF)
         goto tryagain;
      if (!GenerateFileName (ptlf->dwIDDate, ptlf->dwIDTime, &mem)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         goto tryagain;
      }

      // initialize
      if (!ptlf->pTLF->Init ((PWSTR)mem.p, ptlf->dwIDDate, ptlf->dwIDTime)) {
         delete ptlf->pTLF;
         ptlf->pTLF = NULL;
         goto tryagain;
      }
   }

   // remember when last used
   ptlf->pTLF->m_ftLastUsed = ft;

   // call in search for this
   if (!ptlf->pTLF->Search (pTLS->dwDateStart, pTLS->dwTimeStart, pTLS->dwDateEnd, pTLS->dwTimeEnd,
      &pTLS->gActor, &pTLS->gObject, &pTLS->gRoom, &pTLS->gUser,
      pTLS->pMemString ? (PWSTR)pTLS->pMemString->p : NULL,
      pTLS->plLOGFILESEARCH, pTLS->hWndNotify, pTLS->uMsg, pTLS))
      goto tryagain;
   if (!fInCritSec)
      LeaveCriticalSection (&m_CritSec);
   return;  // done

donewithsearch:
   // create the list of results, which are string followed by line
   CMIFLVar varString;
   WCHAR szTemp[32];
   DWORD dwStringDate = 0, dwStringTime = 0;
   CMIFLVar varList;
   CMIFLVarLValue var;
   PCMIFLVarList pSub;
   pl = new CMIFLVarList;
   if (!pl)
      goto cleanup;
   varList.SetList (pl);

   // loop through all the results
   PLOGFILESEARCH plfs = (PLOGFILESEARCH) pTLS->plLOGFILESEARCH->Get(0);
   for (i = 0; i < pTLS->plLOGFILESEARCH->Num(); i++, plfs++) {
      if ((plfs->dwIDDate != dwStringDate) || (plfs->dwIDTime != dwStringTime)) {
         // new string ID
         dwStringDate = plfs->dwIDDate;
         dwStringTime = plfs->dwIDTime;
         DateTimeToIDString (dwStringDate, dwStringTime, szTemp);
         varString.SetString (szTemp);
      }

      // sublist
      pSub = new CMIFLVarList;
      if (!pSub)
         continue;

      // add string
      pSub->Add (&varString, TRUE);

      // add line number
      var.m_Var.SetDouble (plfs->dwLine);
      pSub->Add (&var.m_Var, TRUE);

      var.m_Var.SetList (pSub);
      pl->Add (&var.m_Var, TRUE);
      pSub->Release();
   } // i

   // verify the vm
   if (!gpMainWindow || (gpMainWindow->VMGet() != pTLS->pVM))
      goto cleanup;

   // wrap up the parameters
   pSub = new CMIFLVarList;
   if (!pSub)
      goto cleanup;

   // first is list
   var.m_Var.SetList (pl);
   pSub->Add (&var.m_Var, TRUE);

   // second is param
   pSub->Add (pTLS->pParam, TRUE);

   DWORD dwRet;
   GUID g;
   switch (pTLS->pCallback->TypeGet()) {
   case MV_FUNC:
      if (gpMainWindow)
         gpMainWindow->CPUMonitor(NULL); // for accurate CPU monitoring
      dwRet = pTLS->pVM->FunctionCall (pTLS->pCallback->GetValue(), pSub, &var);
      if (gpMainWindow)
         gpMainWindow->CPUMonitor(NULL); // for accurate CPU monitoring
      break;
   case MV_OBJECTMETH:
      g = pTLS->pCallback->GetGUID ();
      if (gpMainWindow)
         gpMainWindow->CPUMonitor(NULL); // for accurate CPU monitoring
      dwRet = pTLS->pVM->MethodCall (&g, pTLS->pCallback->GetValue(), pSub, 0, 0, &var);
      if (gpMainWindow)
         gpMainWindow->CPUMonitor(NULL); // for accurate CPU monitoring
      break;
   default:
      // ignore
      break;
   } // switch
   pSub->Release();


cleanup:
   pl->Release();

   // done
   if (pTLS->pCallback)
      delete pTLS->pCallback;
   if (pTLS->pParam)
      delete pTLS->pParam;
   if (pTLS->pMemString)
      delete pTLS->pMemString;
   if (pTLS->plLOGFILESEARCH)
      delete pTLS->plLOGFILESEARCH;

   // clear
   for (i = 0; i < m_lTEXTLOGSEARCH.Num(); i++)
      if (m_lTEXTLOGSEARCH.Get(i) == pTLS) {
         m_lTEXTLOGSEARCH.Remove (i);
         break;
      }

   if (!fInCritSec)
      LeaveCriticalSection (&m_CritSec);
}

// BUGBUG - search doesn't seem to be able to find anything in a file that was just
// logged... implies that somehow the ll.dwOffsetData is wrong (which it doesn't seem to be)
// or something else with search. Or might have to do with slow operations setting