/*************************************************************************************
CMegaFileInThread.cpp - Megafile in its own thread that does a lazy write.

begun 5/10/05 by Mike Rozak.
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
CMegaFileInThread::Constructor and destructor
*/
CMegaFileInThread::CMegaFileInThread (void)
{
   m_dwQueue = 1000;
   m_hThread = m_hSignalFromThread = m_hSignalToThread = NULL;
   InitializeCriticalSection (&m_CritSec);
   m_lMFITQ.Init (sizeof(MFITQ));
   memset (&m_CIRCUMREALITYTQInProgress, 0, sizeof(m_CIRCUMREALITYTQInProgress));
   m_fWantToQuit = FALSE;
   m_fNotifyWhenDoneWithProgress = FALSE;
}

CMegaFileInThread::~CMegaFileInThread (void)
{
   if (m_hThread) {
      EnterCriticalSection (&m_CritSec);
      m_fWantToQuit = TRUE;
      SetEvent (m_hSignalToThread);
      LeaveCriticalSection (&m_CritSec);

      // wait
      WaitForSingleObject (m_hThread, INFINITE);
      
      // delete
      DeleteObject (m_hThread);
   }
   if (m_hSignalFromThread)
      DeleteObject (m_hSignalFromThread);
   if (m_hSignalToThread)
      DeleteObject (m_hSignalToThread);

   // NOTE: By time get here, queue will have been emptied

   DeleteCriticalSection (&m_CritSec);
}


/*************************************************************************************
MegaFileThreadProc - Thread that handles the internet.
*/
DWORD WINAPI MegaFileThreadProc(LPVOID lpParameter)
{
   PCMegaFileInThread pThread = (PCMegaFileInThread) lpParameter;

   pThread->ThreadProc ();

   return 0;
}


/*************************************************************************************
CMegaFileInThread::Init - Initializes the megafile. Just like CMegaFile's call,
except can only be used once

inputs
   PWSTR          pszMegaFile - File
   cont GUID      *pgIDSub - ID
   BOOL           fCreateIfNotExist - If TRUE create if doens't exist
   DWORD          dwQueue - Maximum number of entries in the queue before any new entries
                  will hold up the main queue
returns
   BOOL - TRUE if success
*/
BOOL CMegaFileInThread::Init (PWSTR pszMegaFile, const GUID *pgIDSub, BOOL fCreateIfNotExist, DWORD dwQueue)
{
   if (m_hThread)
      return FALSE;
   m_dwQueue = max(dwQueue, 1);

   // initialize the mega file
   if (!m_MegaFile.Init (pszMegaFile, pgIDSub, fCreateIfNotExist))
      return FALSE;

   // thread bits
   m_hSignalFromThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);

   DWORD dwID;
   m_hThread = CreateThread (NULL, 0, MegaFileThreadProc, this, 0, &dwID);
      // NOTE: Not using ESCTHREADCOMMITSIZE because want to be as stable as possible
   if (!m_hThread)
      return FALSE;

   // lower thread priority for rendering so audio wont break up.
   SetThreadPriority (m_hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL));

   return TRUE;
}

/*************************************************************************************
CMegaFileInThread::ThreadProc - Internal function that handles the thread.
*/
void CMegaFileInThread::ThreadProc (void)
{
   while (TRUE) {
      // pick something off the queue
      EnterCriticalSection (&m_CritSec);

alreadyincritsec:
      // if no items left in queue
      if (!m_lMFITQ.Num()) {
         if (m_fWantToQuit) {
            // notify
            if (m_fNotifyWhenDoneWithProgress) {
               m_fNotifyWhenDoneWithProgress = FALSE;
               SetEvent (m_hSignalFromThread);
            }

            // signaled that want to quit
            LeaveCriticalSection (&m_CritSec);
            return;
         }

         // notify
         if (m_fNotifyWhenDoneWithProgress) {
            m_fNotifyWhenDoneWithProgress = FALSE;
            SetEvent (m_hSignalFromThread);
         }

         // else, no items, but dont want to quit, so wait
         LeaveCriticalSection (&m_CritSec);
         WaitForSingleObject (m_hSignalToThread, INFINITE);
         continue;
      }

      // else, there's something in the queue, so pull it off
      memcpy (&m_CIRCUMREALITYTQInProgress, m_lMFITQ.Get(0), sizeof(m_CIRCUMREALITYTQInProgress));
      m_lMFITQ.Remove (0);

      // notify
      if (m_fNotifyWhenDoneWithProgress) {
         m_fNotifyWhenDoneWithProgress = FALSE;
         SetEvent (m_hSignalFromThread);
      }

      LeaveCriticalSection (&m_CritSec);

      switch (m_CIRCUMREALITYTQInProgress.dwAction) {
      case MFITQ_SAVE:
         if (m_CIRCUMREALITYTQInProgress.pMemData)
            // NOTE: ignoreing return since cant do much about it
            m_MegaFile.Save ((PWSTR)m_CIRCUMREALITYTQInProgress.pMemName->p,
               m_CIRCUMREALITYTQInProgress.pMemData->p, m_CIRCUMREALITYTQInProgress.pMemData->m_dwCurPosn,
               (m_CIRCUMREALITYTQInProgress.dwFTValid & 0x01) ? &m_CIRCUMREALITYTQInProgress.ftCreated : NULL,
               (m_CIRCUMREALITYTQInProgress.dwFTValid & 0x02) ? &m_CIRCUMREALITYTQInProgress.ftModified : NULL,
               (m_CIRCUMREALITYTQInProgress.dwFTValid & 0x04) ? &m_CIRCUMREALITYTQInProgress.ftAccessed : NULL);
                     // NOTE: Not tested
         else if (m_CIRCUMREALITYTQInProgress.fCompressed)
            MMLFileSave (&m_MegaFile, (PWSTR)m_CIRCUMREALITYTQInProgress.pMemName->p, &GUID_DatabaseCompressed,
               m_CIRCUMREALITYTQInProgress.pNodeData);    // NOTE: Not passing in filetime
         else {   // non-compressed MML
            // else, raw text
            CMem mem;
            if (MMLToMem (m_CIRCUMREALITYTQInProgress.pNodeData, &mem, TRUE)) {
               mem.CharCat (0);
               m_MegaFile.Save ((PWSTR)m_CIRCUMREALITYTQInProgress.pMemName->p,
                  mem.p, mem.m_dwCurPosn,
                  (m_CIRCUMREALITYTQInProgress.dwFTValid & 0x01) ? &m_CIRCUMREALITYTQInProgress.ftCreated : NULL,
                  (m_CIRCUMREALITYTQInProgress.dwFTValid & 0x02) ? &m_CIRCUMREALITYTQInProgress.ftModified : NULL,
                  (m_CIRCUMREALITYTQInProgress.dwFTValid & 0x04) ? &m_CIRCUMREALITYTQInProgress.ftAccessed : NULL);
            }
         }
         break;
      case MFITQ_DELETE:
         m_MegaFile.Delete ((PWSTR)m_CIRCUMREALITYTQInProgress.pMemName->p);
         break;
      case MFITQ_CLEAR:
         m_MegaFile.Clear();
         break;
      } // switch

      // done
      EnterCriticalSection (&m_CritSec);

      // free up memory
      if (m_CIRCUMREALITYTQInProgress.pMemData)
         delete m_CIRCUMREALITYTQInProgress.pMemData;
      if (m_CIRCUMREALITYTQInProgress.pMemName)
         delete m_CIRCUMREALITYTQInProgress.pMemName;
      if (m_CIRCUMREALITYTQInProgress.pNodeData)
         delete m_CIRCUMREALITYTQInProgress.pNodeData;
      m_CIRCUMREALITYTQInProgress.dwAction = MFITQ_NONE;
      // notify
      if (m_fNotifyWhenDoneWithProgress) {
         m_fNotifyWhenDoneWithProgress = FALSE;
         SetEvent (m_hSignalFromThread);
      }
      // LeaveCriticalSection (&m_CritSec);
      goto alreadyincritsec;
   } // while TRUE
}



/*************************************************************************************
CMegaFileInThread::Exists - Sees if a file exists, and gets its info (potentially).

inputs
   PWSTR          pszFile - File
   PMFFILEINFO    pInfo - Info to be filled in. NOTE: If this is NOT null then
                  Exists() will IGNORE the queue of stuff being worked on.
                  If NULL, then Exists() looks through the queue.
returns
   BOOL - TRUE if it exists
*/
BOOL CMegaFileInThread::Exists (PWSTR pszFile, PMFFILEINFO pInfo)
{
   // NOTE: Not tested
   if (pInfo)
      return m_MegaFile.Exists (pszFile, pInfo);

   // else, loop through queue
   EnterCriticalSection (&m_CritSec);
   PMFITQ pq = (PMFITQ)m_lMFITQ.Get(0);
   PMFITQ pqCur;
   DWORD dwNum = m_lMFITQ.Num();
   DWORD i;
   for (i = dwNum; i <= dwNum; i--) {
      pqCur = i ? &pq[i-1] : &m_CIRCUMREALITYTQInProgress;
      if (pqCur->dwAction == MFITQ_NONE)
         continue;
      else if (pqCur->dwAction == MFITQ_CLEAR) {
         LeaveCriticalSection (&m_CritSec);
         return FALSE;  // just cleared, so doenst exist
      }

      if (!pqCur->pMemName)
         continue;

      if (_wcsicmp ((PWSTR)pqCur->pMemName->p, pszFile))
         continue;   // not same name

      // if deleted then dont exist, or if save then exist
      if (pqCur->dwAction == MFITQ_DELETE) {
         LeaveCriticalSection (&m_CritSec);
         return FALSE;  // just cleared, so doenst exist
      }
      if (pqCur->dwAction == MFITQ_SAVE) {
         LeaveCriticalSection (&m_CritSec);
         return TRUE;  // just cleared, so doenst exist
      }
   } // i

   // if get here then use main file
   BOOL fRet;
   fRet = m_MegaFile.Exists (pszFile, pInfo);
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CMegaFileInThread::LoadBinary - Like CMegaFile::Load() except no fIgnoreDir.

inputs
   PWSTR          pszFile - File name
   __int64        *piSize - Filled with the size
returns
   PVOID - Data that must be freed with MegaFileFree()
*/
PVOID CMegaFileInThread::LoadBinary (PWSTR pszFile, __int64 *piSize)
{
   // NOTE: Not tested

searchagain:
   // try to find something already in the queue
   EnterCriticalSection (&m_CritSec);
   PMFITQ pq = (PMFITQ)m_lMFITQ.Get(0);
   PMFITQ pqCur;
   DWORD dwNum = m_lMFITQ.Num();
   DWORD i;
   for (i = dwNum; i <= dwNum; i--) {
      pqCur = i ? &pq[i-1] : &m_CIRCUMREALITYTQInProgress;
      if (pqCur->dwAction == MFITQ_NONE)
         continue;
      else if (pqCur->dwAction == MFITQ_CLEAR) {
         LeaveCriticalSection (&m_CritSec);
         return NULL;  // just cleared, so doenst exist
      }

      if (!pqCur->pMemName)
         continue;

      if (_wcsicmp ((PWSTR)pqCur->pMemName->p, pszFile))
         continue;   // not same name

      // if deleted then dont exist, or if save then exist
      if (pqCur->dwAction == MFITQ_DELETE) {
         LeaveCriticalSection (&m_CritSec);
         return NULL;  // just cleared, so doenst exist
      }
      if (pqCur->dwAction == MFITQ_SAVE)
         break;
   } // i

   // if found it, and it's being acted upon then set notification that want to be
   // notified when its done and wait
   if (!i) {
      m_fNotifyWhenDoneWithProgress = TRUE;
      LeaveCriticalSection (&m_CritSec);
      WaitForSingleObject (m_hSignalFromThread, INFINITE);

      goto searchagain;
   }

   // if not found, then use main file
   if (i > dwNum) {
      PVOID pRet = m_MegaFile.Load (pszFile, piSize);
      LeaveCriticalSection (&m_CritSec);
      return pRet;
   }

   // if get here then found something being saved
   if (!pqCur->pMemData) {
      LeaveCriticalSection (&m_CritSec);
      return NULL;  // error because not binary
   }

   // allocate
   PVOID pAlloc = ESCMALLOC (pqCur->pMemData->m_dwAllocated);
   if (!pAlloc) {
      LeaveCriticalSection (&m_CritSec);
      return NULL;  // error because cant allocate
   }
   memcpy (pAlloc, pqCur->pMemData->p, pqCur->pMemData->m_dwAllocated);
   *piSize = pqCur->pMemData->m_dwAllocated;
   LeaveCriticalSection (&m_CritSec);
   return pAlloc;
}

/*************************************************************************************
CMegaFileInThread::LoadMML - Loads data from megafile.

inptus
   PWSTR          pszFile - File
   BOOL           fCompressed - TRUE if MML is saved as compressed, FALSE if uncompressed
returns
   PCMMLNode2 - Node with data
*/
PCMMLNode2 CMegaFileInThread::LoadMML (PWSTR pszFile, BOOL fCompressed)
{
searchagain:
   // try to find something already in the queue
   EnterCriticalSection (&m_CritSec);
   PMFITQ pq = (PMFITQ)m_lMFITQ.Get(0);
   PMFITQ pqCur;
   DWORD dwNum = m_lMFITQ.Num();
   DWORD i;
   for (i = dwNum; i <= dwNum; i--) {
      pqCur = i ? &pq[i-1] : &m_CIRCUMREALITYTQInProgress;
      if (pqCur->dwAction == MFITQ_NONE)
         continue;
      else if (pqCur->dwAction == MFITQ_CLEAR) {
         LeaveCriticalSection (&m_CritSec);
         return NULL;  // just cleared, so doenst exist
      }

      if (!pqCur->pMemName)
         continue;

      if (_wcsicmp ((PWSTR)pqCur->pMemName->p, pszFile))
         continue;   // not same name

      // if deleted then dont exist, or if save then exist
      if (pqCur->dwAction == MFITQ_DELETE) {
         LeaveCriticalSection (&m_CritSec);
         return NULL;  // just cleared, so doenst exist
      }
      if (pqCur->dwAction == MFITQ_SAVE)
         break;
   } // i

   // if found it, and it's being acted upon then set notification that want to be
   // notified when its done and wait
   if (!i) {
      m_fNotifyWhenDoneWithProgress = TRUE;
      LeaveCriticalSection (&m_CritSec);
      WaitForSingleObject (m_hSignalFromThread, INFINITE);

      goto searchagain;
   }

   // if not found, then use main file
   if (i > dwNum) {
      PCMMLNode2 pNode = NULL;
      if (fCompressed)
         pNode = MMLFileOpen (&m_MegaFile, pszFile, &GUID_DatabaseCompressed);
      else {
         // else, raw
         __int64 iSize;
         PWSTR psz = (PWSTR) m_MegaFile.Load (pszFile, &iSize);
         if (psz) {
            pNode = MMLFromMem(psz);
            pNode->NameSet (L"MMLLoad");
            MegaFileFree (psz);
         }
      }
      LeaveCriticalSection (&m_CritSec);
      return pNode;
   }

   // else, found in the queue
   if (!pqCur->pNodeData || (!pqCur->fCompressed != !fCompressed)) {
      LeaveCriticalSection (&m_CritSec);
      return NULL;  // error because not in right format
   }

   // clone current node
   PCMMLNode2 pNode;
   pNode = pqCur->pNodeData->Clone();
   LeaveCriticalSection (&m_CritSec);
   return pNode;
}


/*************************************************************************************
CMegaFileInThread::SaveBinary - Saves data using the lazy cache

inputs
   PWSTR          pszFile - File
   PCMem          pMem - Memory. This memory is KEPT by the megafile and freed by itself
   FILETIME       *pftCreated, *pftModified, *pftAccessed - Optional file times
returns
   BOOL - Almost always returns TRUE. It can't really tell if the data was saved because
            the data wont be saved right away
*/
BOOL CMegaFileInThread::SaveBinary (PWSTR pszFile, PCMem pMem,
   FILETIME *pftCreated, FILETIME *pftModified, FILETIME *pftAccessed)
{
   // NOTE: Not tested
   MFITQ q;
   memset (&q, 0, sizeof(q));
   q.dwAction = MFITQ_SAVE;
   q.pMemData = pMem;
   q.pMemName = new CMem;
   if (!q.pMemName) {
      delete pMem;
      return FALSE;
   }
   MemZero (q.pMemName);
   MemCat (q.pMemName, pszFile);
   if (pftCreated) {
      q.dwFTValid |= 0x01;
      q.ftCreated = *pftCreated;
   }
   if (pftModified) {
      q.dwFTValid |= 0x02;
      q.ftCreated = *pftModified;
   }
   if (pftAccessed) {
      q.dwFTValid |= 0x04;
      q.ftCreated = *pftAccessed;
   }

   EnterCriticalSection (&m_CritSec);

   // repeat until queue isn't too large
   while (m_lMFITQ.Num() > m_dwQueue) {
      m_fNotifyWhenDoneWithProgress = TRUE;
      SetEvent (m_hSignalToThread);
      LeaveCriticalSection (&m_CritSec);

      WaitForSingleObject (m_hSignalFromThread, INFINITE);

      EnterCriticalSection (&m_CritSec);
   }

   m_lMFITQ.Add (&q);
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}


/*************************************************************************************
CMegaFileInThread::SaveMML - Saves the data as MML.

inputs
   PWSTR          pszFile - file
   PCMMLNode2     pNode - MML node to save. This is KEPT by the call and later freed.
   BOOL           fCompressed - Set to TRUE to save as compressed, FALSE as raw text
   FILETIME       *pftCreated, *pftModified, *pftAccessed - Optional file times
returns
   BOOL - Almost always returns TRUE. It can't really tell if the data was saved because
            the data wont be saved right away
*/
BOOL CMegaFileInThread::SaveMML (PWSTR pszFile, PCMMLNode2 pNode, BOOL fCompressed,
   FILETIME *pftCreated, FILETIME *pftModified, FILETIME *pftAccessed)
{
   MFITQ q;
   memset (&q, 0, sizeof(q));
   q.dwAction = MFITQ_SAVE;
   q.pNodeData = pNode;
   q.fCompressed = fCompressed;
   q.pMemName = new CMem;
   if (!q.pMemName) {
      delete pNode;
      return FALSE;
   }
   MemZero (q.pMemName);
   MemCat (q.pMemName, pszFile);
   if (pftCreated) {
      q.dwFTValid |= 0x01;
      q.ftCreated = *pftCreated;
   }
   if (pftModified) {
      q.dwFTValid |= 0x02;
      q.ftModified = *pftModified;
   }
   if (pftAccessed) {
      q.dwFTValid |= 0x04;
      q.ftAccessed = *pftAccessed;
   }

   EnterCriticalSection (&m_CritSec);

   // repeat until queue isn't too large
   while (m_lMFITQ.Num() > m_dwQueue) {
      m_fNotifyWhenDoneWithProgress = TRUE;
      SetEvent (m_hSignalToThread);
      LeaveCriticalSection (&m_CritSec);

      WaitForSingleObject (m_hSignalFromThread, INFINITE);

      EnterCriticalSection (&m_CritSec);
   }

   m_lMFITQ.Add (&q);
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}


/*************************************************************************************
CMegaFileInThread::Enum - Like CMegaFile's enum. EXCEPT, this one WON'T
enum files that are STILL in the cache waiting to be written out
*/
BOOL CMegaFileInThread::Enum (PCListVariable plName, PCListFixed plMFFILEINFO, PWSTR pszPrefix)
{
   return m_MegaFile.Enum (plName, plMFFILEINFO, pszPrefix);
}


/*************************************************************************************
CMegaFileInThread::Delete - Deletes the megafile.

inputs 
   PWSTR          pszFile - file to delete
returns
   BOOL - ALWAYS returns TRUE because it doesn't know if the file exists. It just adds
         delete to the queue.
*/
BOOL CMegaFileInThread::Delete (PWSTR pszFile)
{
   MFITQ q;
   memset (&q, 0, sizeof(q));
   q.dwAction = MFITQ_DELETE;
   q.pMemName = new CMem;
   if (!q.pMemName)
      return FALSE;
   MemZero (q.pMemName);
   MemCat (q.pMemName, pszFile);

   EnterCriticalSection (&m_CritSec);
   // NOTE: Don't bother waiting for queue to be small enough since a very small amount of memory
   m_lMFITQ.Add (&q);
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}

/*************************************************************************************
CMegaFileInThread::LimitSize - like megafile
*/
void CMegaFileInThread::LimitSize (__int64 iWant, DWORD dwTooYoung)
{
   m_MegaFile.LimitSize (iWant, dwTooYoung);
}


/*************************************************************************************
CMegaFileInThread::Clear - Like megafile
*/
BOOL CMegaFileInThread::Clear (void)
{
   // NOTE: Not tested
   MFITQ q;
   memset (&q, 0, sizeof(q));
   q.dwAction = MFITQ_CLEAR;

   EnterCriticalSection (&m_CritSec);
   // NOTE: Don't bother waiting for queue to be small enough since a very small amount of memory
   m_lMFITQ.Add (&q);
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);
   return TRUE;
}


/*************************************************************************************
CMegaFileInThread::Num - Returns the number of entries. NOTE: Does NOT include
those files in the write queue
*/
DWORD CMegaFileInThread::Num (void)
{
   return m_MegaFile.Num();
}


/*************************************************************************************
CMegaFileInThread::GetNum - Like megafile. NOTE: Does NOT include the files
in the write queue, and the megafile may have changed between the time Num() was
called and GetNum() was called
*/
BOOL CMegaFileInThread::GetNum (DWORD dwIndex, PCMem pMem)
{
   return m_MegaFile.GetNum (dwIndex, pMem);
}

