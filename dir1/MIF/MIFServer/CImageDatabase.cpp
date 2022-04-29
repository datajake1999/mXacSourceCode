/*************************************************************************************
CImageDatabase.cpp - Stores images and wave files (from the player's computers) to disk.

begun 2/3/09 by Mike Rozak.
Copyright 2009 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"


#define FILESPERDATABASEDIR         1000        // number of files stored in each database directory

#define MINTIMESREQUESTED           3           // at least 3 players must have requested this before say want

#define IMAGEDATABASEINDEXHEADER_ID       52545    // random number to make sure opening right info

// IMAGEDATABASEINDEXHEADER - Header file for the index
typedef struct {
   DWORD                dwID;       // must be IMAGEDATABASEINDEXHEADER_ID
   DWORD                dwEntries;  // number of entries in the database
   __int64              iFileTime;  // file time of last modification to ensure that if data changes
                                    // the cache is cleared
} IMAGEDATABASEINDEXHEADER, *PIMAGEDATABASEINDEXHEADER;

// IMAGEDATABASEINDEXENTRY - Entry information
typedef struct {
   // same as IMAGEDATABASEENTRY
   __int64              iTimesRequested;  // number of times this file was requested
   __int64              iTimesDelivered;  // number of times this file was delivered
   DWORD                dwType;           // IMAGEDATABASETYPE_IMAGE or IMAGEDATABASETYPE_WAVE
   IMAGEDATABASEQUALITYINFO aQuality[IMAGEDATABASE_MAXQUALITIES]; // array of differnet qualities

   // specific
   DWORD                dwSizeString;  // size in bytes of the string (immediately following as WCHARs)
} IMAGEDATABASEINDEXENTRY, *PIMAGEDATABASEINDEXENTRY;


// IDTENTRYQUEUE - Entry queue information
typedef struct {
   DWORD          dwAction;         // 0 to add an entry, 1 request data be sent (not tested), 2 data queued to send
   PCWSTR         pszString;        // string memory, allocated by ESCMALLOC(), and must be freed
   DWORD          dwQuality;
   DWORD          dwQualityMax;     // max quality, used for "request data" action
   DWORD          dwType;
   DWORD          dwConnectionID;   // connection that sent
   PVOID          pData;            // data, allocated by ESCMALLOC()
   size_t         iDataSize;        // data size
} IDTENTRYQUEUE, *PIDTENTRYQUEUE;

// IDTCONNECTIONDATA - Store last time that accepted data from a connection
typedef struct {
   DWORD          dwConnectionID;   // connection ID that sent the data
   __int64        iTime;            // time-stamp
} IDTCONNECTIONDATA, *PIDTCONNECTIONDATA;

/*************************************************************************************
CImageDatabaseThread::Constructor and destructor
*/

CImageDatabaseThread::CImageDatabaseThread (void)
{
   InitializeCriticalSection (&m_CritSec);
   m_fRunning = FALSE;
   m_pIT = NULL;
   m_hThread = NULL;
   m_hSignalToThread = NULL;

   m_iDataMaximum = (__int64)100 * (__int64)1000000000;  // 100 gig
   m_iDataMinimumHardDrive = (__int64)1 * (__int64)1000000000;  // 100 gig
   m_iDataGreedy = (__int64)1 * (__int64)1000000000;  // 100 gig
   m_dwDataMaxEntries = (sizeof(PVOID) > sizeof(DWORD)) ? 1000000 : 10000; // no more than a million

   m_lIDTENTRYQUEUE.Init (sizeof(IDTENTRYQUEUE));
   m_lIDTCONNECTIONDATA.Init (sizeof(IDTCONNECTIONDATA));
}

CImageDatabaseThread::~CImageDatabaseThread (void)
{
   EnterCriticalSection (&m_CritSec);
   if (m_fRunning) {
      LeaveCriticalSection (&m_CritSec);
      ThreadStop ();
   }
   else
      LeaveCriticalSection (&m_CritSec);

   _ASSERTE (!m_hThread); // shouyldnt be a thread now, nor a signal to a thread
   _ASSERTE (!m_hSignalToThread); // shouyldnt be a thread now, nor a signal to a thread
   _ASSERTE (!m_lIDTENTRYQUEUE.Num());
}


/*************************************************************************************
ImageDatabaseThreadProc - Thread that handles the internet.
*/
DWORD WINAPI ImageDatabaseThreadProc(LPVOID lpParameter)
{
   PCImageDatabaseThread pThread = (PCImageDatabaseThread) lpParameter;

   pThread->ThreadProc ();

   return 0;
}

/*************************************************************************************
CImageDatabaseThread::ThreadStart - Starts the thread.

inputs
   PCWSTR         pszPath - Path where to put the database, without the last
                  backslash, like "c:\ashtariempire\imagedata".
   __int64        iFileTime - The file time of when this MIF file was created so
                  that will erase render cache if the data is changed
   PCInternetThread   pIT - Internet thread
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CImageDatabaseThread::ThreadStart (PCWSTR pszPath, __int64 iFileTime, PCInternetThread pIT)
{
   EnterCriticalSection (&m_CritSec);
   if (m_fRunning) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }
   LeaveCriticalSection (&m_CritSec);

   if (!m_ImageDatabase.Init (pszPath, iFileTime))
      return FALSE;

   // create the bits
   m_pIT = pIT;
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_fRunning = TRUE;
   DWORD dwID;
   m_hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, ImageDatabaseThreadProc, this, 0, &dwID);
   if (!m_hThread) {
      CloseHandle (m_hSignalToThread);
      m_hSignalToThread = NULL;
      m_fRunning = FALSE;
      return FALSE;
   }

   return TRUE;
}


/*************************************************************************************
CImageDatabaseThread::ThreadStop - Turn off the thread and prevent future calls
into the thread from happening
*/
BOOL CImageDatabaseThread::ThreadStop (void)
{
   EnterCriticalSection (&m_CritSec);
   if (!m_fRunning) {
      LeaveCriticalSection (&m_CritSec);
      return FALSE;
   }
   m_fRunning = FALSE;
   m_pIT = FALSE;
   SetEvent (m_hSignalToThread);
   LeaveCriticalSection (&m_CritSec);

   WaitForSingleObject (m_hThread, INFINITE);
   CloseHandle (m_hThread);
   CloseHandle (m_hSignalToThread);
   m_hSignalToThread = NULL;
   m_hThread = NULL;
   
   // free up the queue
   DWORD i;
   PIDTENTRYQUEUE peq = (PIDTENTRYQUEUE) m_lIDTENTRYQUEUE.Get(0);
   for (i = 0; i < m_lIDTENTRYQUEUE.Num(); i++, peq++) {
      if (peq->pData)
         ESCFREE (peq->pData);
      if (peq->pszString)
         ESCFREE ((PVOID) peq->pszString);
   } // i
   m_lIDTENTRYQUEUE.Clear();

   return TRUE;
}



/*************************************************************************************
CImageDatabaseThread::ThreadProc - Handles the thread's actions, such as:
   - Cleaning up the database from time to time
   - Sending data out when a client is ready for it
   - Saving data that came in from a client
*/
void CImageDatabaseThread::ThreadProc (void)
{
   // how many ticks per second
   LARGE_INTEGER iCur;
   memset (&iCur, 0, sizeof(iCur));
   __int64 iTicksPerSec;
   QueryPerformanceFrequency (&iCur);
   iTicksPerSec = *((__int64*)&iCur);

   // get the current time
   __int64 iTime;
   QueryPerformanceCounter (&iCur);
   iTime = *((__int64*)&iCur);

   __int64 iLastCleanUp = iTime;
   __int64 iLastSave = iTime;
   __int64 iCleanUpInterval = iTicksPerSec * 30; // 30 seconds
   __int64 iSaveInterval = iTicksPerSec * 600; // 10 minutes
   __int64 iDataAcceptTime = iTicksPerSec * 15; // only accept data from players once
            // every 15 seconds

   PIMAGEDATABASEENTRY pde;
   DWORD dwQuality;

   while (TRUE) {
      // wait for a notification
      WaitForSingleObject (m_hSignalToThread, 100);
         // NOTE: Waking up every 100 milliseconds and doing housekeeping

      // get the current time
      QueryPerformanceCounter (&iCur);
      iTime = *((__int64*)&iCur);

      EnterCriticalSection (&m_CritSec);

      // see if want to quit
      if (!m_fRunning) {
         // want to quit
         LeaveCriticalSection (&m_CritSec);
         break;
      }

      // clear the data-accepted from players list
      __int64 iDataOld = iTime - iDataAcceptTime;
      PIDTCONNECTIONDATA pcd;
      while (pcd = (PIDTCONNECTIONDATA) m_lIDTCONNECTIONDATA.Get(0)) {
         // repeat until newer
         if ((pcd->iTime >= iDataOld) && (pcd->iTime <= iTime))
            break;

         // delete this
         m_lIDTCONNECTIONDATA.Remove (0);
      }


      // pull stuff off the queue
      DWORD dwEntry;
      DWORD dwLastConnectionIDDataRender = (DWORD)-1;
      DWORD dwLastConnectionIDDataTTS = (DWORD)-1;
      for (dwEntry = 0; dwEntry < m_lIDTENTRYQUEUE.Num(); dwEntry++) {
         PIDTENTRYQUEUE peq = (PIDTENTRYQUEUE) m_lIDTENTRYQUEUE.Get(dwEntry);
         if (!peq)
            break;

         if (peq->dwAction == 0) { // player wants to save some data in the database
            // save this

            // make sure quality within limits
            if (peq->dwQuality >= IMAGEDATABASE_MAXQUALITIES)
               goto deletethisone;

            // check if really want to save this for this particular
            // connection
            if (!EntryAddAccept (TRUE, peq->pszString, peq->dwQuality, peq->dwType,
               peq->dwConnectionID, peq->pData, peq->iDataSize))
               goto deletethisone;

            // remember that have gotten data from this user
            IDTCONNECTIONDATA cd;
            memset (&cd, 0, sizeof(cd));
            cd.dwConnectionID = peq->dwConnectionID;
            cd.iTime = iTime;
            m_lIDTCONNECTIONDATA.Add (&cd);

            // if get here, want to save it... assuming there isn't already data
            pde = m_ImageDatabase.Find (peq->pszString, FALSE);
            if (!pde)
               pde = m_ImageDatabase.Add (peq->pszString);
            if (!pde)
               goto deletethisone;
            pde->dwType = peq->dwType;
            if (pde->aQuality[peq->dwQuality].dwIndexFile)
               goto deletethisone; // already exists

            // delete now, since SaveFileNum() may re-order list
            IDTENTRYQUEUE eq;
            eq = *peq;
            peq = NULL;
            m_lIDTENTRYQUEUE.Remove (dwEntry);
            dwEntry--;

            DWORD dwFileNum = m_ImageDatabase.SaveFileNum (eq.pData, eq.iDataSize, &m_CritSec);

#ifdef _DEBUG
            OutputDebugStringW (L"\r\nCImageDatabaseThread::Saved data from client.");
#endif

            // since have left critical section, need to re-load values
            pde = m_ImageDatabase.Find (eq.pszString, FALSE);
            if (pde) {
               // save this
               pde->aQuality[eq.dwQuality].dwIndexFile = dwFileNum;
               if (pde->aQuality[eq.dwQuality].dwIndexFile)
                  pde->aQuality[eq.dwQuality].iFileSize = eq.iDataSize;
            }
            else {
               // somehow disappeared
               if (dwFileNum)
                  m_ImageDatabase.DeleteFileNum (dwFileNum - 1, eq.iDataSize, &m_CritSec);
            }

            // free the memory
            if (eq.pszString)
               ESCFREE ((PVOID) eq.pszString);
            if (eq.pData)
               ESCFREE (eq.pData);

         } // dwAction==0
         else if (peq->dwAction == 1) {  // player requesting if a download is possible
            // see if can find
            peq->dwQualityMax = min(peq->dwQualityMax, IMAGEDATABASE_MAXQUALITIES);
            peq->dwQualityMax = max(peq->dwQualityMax, 1);
            pde = m_ImageDatabase.Find (peq->pszString, FALSE);
            if (!pde) {   // if cant find then add a blank one
               pde = m_ImageDatabase.Add (peq->pszString);
               pde->dwType = peq->dwType;
            }
            BOOL fFound = FALSE;
            if (pde) {
               // rememeber that requested
               pde->iTimesRequested++;

               for (dwQuality = peq->dwQualityMax-1; (dwQuality < peq->dwQualityMax) && (dwQuality >= peq->dwQuality); dwQuality--)
                  if (pde->aQuality[dwQuality].dwIndexFile) {
                     fFound = TRUE; // found a file that can send
                     break;
                  }
            }

#ifdef _DEBUG
            OutputDebugStringW (L"\r\nCImageDatabaseThread::Client requests data.");
#endif
            // if we've found, then modify this as a different action
            if (fFound) {
               peq->dwAction = 2;  // so actually sending

               // want to re-process this one
               dwEntry--;

               // fall through, since not deleting
            }
            else {
               // send a message back saying that couldnt find
               CMem mem;
               size_t dwStringSize = (wcslen(peq->pszString)+1)*sizeof(WCHAR);
               PCIRCUMREALITYPACKETCLIENTCACHE pcc;
               if (!mem.Required (sizeof(*pcc) + dwStringSize))
                  goto deletethisone;
               mem.m_dwCurPosn = sizeof(*pcc) + dwStringSize;
               pcc = (PCIRCUMREALITYPACKETCLIENTCACHE) mem.p;
               memset (pcc, 0, sizeof(*pcc));
               pcc->dwQuality = peq->dwQuality;
               pcc->dwDataSize = 0;
               pcc->dwStringSize = (DWORD) dwStringSize;
               memcpy ((PWSTR)(pcc+1), peq->pszString, pcc->dwStringSize);

               PCCircumrealityPacket pPacket = m_pIT->ConnectionGet (peq->dwConnectionID);
               if (pPacket) {
                  pPacket->PacketSend (
                     (peq->dwType == IMAGEDATABASETYPE_IMAGE) ? CIRCUMREALITYPACKET_CLIENTCACHEFAILRENDER : CIRCUMREALITYPACKET_CLIENTCACHEFAILTTS,
                     mem.p, (DWORD) mem.m_dwCurPosn);
                  m_pIT->ConnectionGetRelease ();
               }

               // delete this
               goto deletethisone;
            }

         } // dwAction==1
         else if (peq->dwAction == 2) {   // discovered that can send, waiting for the connection to be free
            // if we've dealt with this connection already on this pass then skip now
            if (peq->dwType == IMAGEDATABASETYPE_IMAGE) {
               if (dwLastConnectionIDDataRender == peq->dwConnectionID)
                  continue;
               dwLastConnectionIDDataRender = peq->dwConnectionID;
            }
            else { // IMAGEDATABASETYPE_TTS
               if (dwLastConnectionIDDataTTS == peq->dwConnectionID)
                  continue;
               dwLastConnectionIDDataTTS = peq->dwConnectionID;
            }

            // see if can find a connection
            PCCircumrealityPacket pPacket = m_pIT->ConnectionGet (peq->dwConnectionID);
            if (!pPacket)
               goto deletethisone;  // player has disconnected

            if (pPacket->QueuedToSend ((peq->dwType == IMAGEDATABASETYPE_IMAGE) ? PACKCHAN_RENDER : PACKCHAN_TTS)) {
               m_pIT->ConnectionGetRelease();
               continue;   // there's already TTS/image stuff going out, so dont sent this
            }
            // even if success, temporarily release while load the data and build the message
            m_pIT->ConnectionGetRelease();

            // find the quality
            pde = m_ImageDatabase.Find (peq->pszString, FALSE);
            DWORD dwFoundImage = 0;
            DWORD dwFoundQuality = peq->dwQuality;
            __int64  iFoundSize = 0;
            if (pde)
               for (dwQuality = peq->dwQualityMax-1; (dwQuality < peq->dwQualityMax) && (dwQuality >= peq->dwQuality); dwQuality--)
                  if (pde->aQuality[dwQuality].dwIndexFile) {
                     dwFoundImage = pde->aQuality[dwQuality].dwIndexFile;
                     iFoundSize = pde->aQuality[dwQuality].iFileSize;
                     dwFoundQuality = dwQuality;
                     break;
                  }
            if (dwFoundImage && pde)
               // rememeber that delivered
               pde->iTimesDelivered++;

            // delete now, since when leave critical section, may re-order list
            IDTENTRYQUEUE eq;
            eq = *peq;
            peq = NULL;
            m_lIDTENTRYQUEUE.Remove (dwEntry);
            dwEntry--;
            
            // leaving the critical section so can load and send a message to the client
            LeaveCriticalSection (&m_CritSec);

            // allocate the necessary memory to send
            CMem mem;
            size_t dwStringSize = (wcslen(eq.pszString)+1)*sizeof(WCHAR);
            PCIRCUMREALITYPACKETCLIENTCACHE pcc;
            if (!mem.Required (sizeof(*pcc) + dwStringSize + (size_t)iFoundSize))
               goto deletethisone;
            pcc = (PCIRCUMREALITYPACKETCLIENTCACHE) mem.p;
            memset (pcc, 0, sizeof(*pcc));
            pcc->dwQuality = dwFoundQuality;
            pcc->dwStringSize = (DWORD) dwStringSize;
            memcpy ((PWSTR)(pcc+1), eq.pszString, pcc->dwStringSize);

#ifdef _DEBUG
            OutputDebugStringW (L"\r\nCImageDatabaseThread::Send data to client.");
#endif
            // try to load in the file
            if (dwFoundImage) {
               WCHAR szTemp[512];
               m_ImageDatabase.FileName (dwFoundImage-1, szTemp);
               FILE *pf = _wfopen (szTemp, L"rb");
               if (pf) {
                  if (iFoundSize != fread((PBYTE)(pcc+1) + pcc->dwStringSize, 1, iFoundSize, pf))
                     iFoundSize = 0;   // couldnt read it all
                  fclose (pf);
               }
               else
                  iFoundSize = 0;   // cant open
            }

            pcc->dwDataSize = (DWORD) iFoundSize;
            mem.m_dwCurPosn = sizeof(*pcc) + pcc->dwStringSize + pcc->dwDataSize;

            // get the connection again and send the message
            pPacket = m_pIT->ConnectionGet (eq.dwConnectionID);
            DWORD dwMessage;
            if (pcc->dwDataSize)
               dwMessage = (eq.dwType == IMAGEDATABASETYPE_IMAGE) ? CIRCUMREALITYPACKET_CLIENTCACHERENDER : CIRCUMREALITYPACKET_CLIENTCACHETTS;
            else
               dwMessage = (eq.dwType == IMAGEDATABASETYPE_IMAGE) ? CIRCUMREALITYPACKET_CLIENTCACHEFAILRENDER : CIRCUMREALITYPACKET_CLIENTCACHEFAILTTS;
            if (pPacket) {
               pPacket->PacketSend (
                  dwMessage,
                  mem.p, (DWORD) mem.m_dwCurPosn);
               m_pIT->ConnectionGetRelease ();
            }

            // free the memory
            if (eq.pszString)
               ESCFREE ((PVOID) eq.pszString);
            if (eq.pData)
               ESCFREE (eq.pData);

            // re-enter the critical seciton
            EnterCriticalSection (&m_CritSec);

         } // dwAction==2

         // if get here don't delete
         continue;

deletethisone:
         if (peq->pszString)
            ESCFREE ((PVOID) peq->pszString);
         if (peq->pData)
            ESCFREE (peq->pData);
         m_lIDTENTRYQUEUE.Remove (dwEntry);
         dwEntry--;

      } // dwEntry

      // clean up
      if ((iTime > iLastCleanUp + iCleanUpInterval) || (iTime < iLastCleanUp - iCleanUpInterval)) {
         CleanUp (TRUE, iTime);
         iLastCleanUp = iTime;
      }

      // save once in awhile, even though it's a bit slow
      // just in case crash
      if ((iTime > iLastSave + iSaveInterval) || (iTime < iLastSave - iSaveInterval)) {
         m_ImageDatabase.IndexSave ();
         iLastSave = iTime;
      }

      LeaveCriticalSection (&m_CritSec);
   } // while TRUE
}



/*************************************************************************************
CImageDatabaseThread::EntryAddAccept - See if the server will accept an entry from
this player.

inputs
   BOOL              fAlreadyInCritSec - If TRUE then already in the critical seciton.
                        If FALSE, then will enter critical section by self.
   PCWSTR            pszString - String
   DWORD             dwQuality - Quality setting
   DWORD             dwType - IMAGEDATABASETYPE_XXX
   DWORD             dwConnectionID - Connection ID, so can see if the connection
                     has been trying to add lots of data already, etc.
   PVOID             pData - Data
   size_t            iDataSize - Size of pData.
returns
   BOOL - TRUE if success
*/

BOOL CImageDatabaseThread::EntryAddAccept (BOOL fAlreadyInCritSec, PCWSTR pszString,
                                     DWORD dwQuality, DWORD dwType, DWORD dwConnectionID,
                                     PVOID pData, size_t iDataSize)
{
   BOOL fRet = TRUE;
   if (!fAlreadyInCritSec)
      EnterCriticalSection (&m_CritSec);


   // if not running then error
   if (!m_fRunning || !m_iDataMaximum) {
      fRet = FALSE;
      goto done;
   }

   // if it already exists then don't accept
   if (dwQuality >= IMAGEDATABASE_MAXQUALITIES) {
      fRet = FALSE;
      goto done;
   }
   PIMAGEDATABASEENTRY pde = m_ImageDatabase.Find (pszString, FALSE);
   if (pde && pde->aQuality[dwQuality].dwIndexFile) {
      // already exists
      fRet = FALSE;
      goto done;
   }

   // if not much data then always accept
   if (m_ImageDatabase.m_iTotalFileSize < m_iDataGreedy)
      goto done;

   // if not requested enough then don't bother
   if (!pde || pde->iTimesRequested < MINTIMESREQUESTED) {
      fRet = FALSE;
      goto done;
   }

   // see if already have accepted something from the player
   PIDTCONNECTIONDATA pcd = (PIDTCONNECTIONDATA) m_lIDTCONNECTIONDATA.Get(0);
   DWORD i;
   for (i = 0; i < m_lIDTCONNECTIONDATA.Num(); i++, pcd++)
      if (pcd->dwConnectionID == dwConnectionID) {
         fRet = FALSE;
         goto done;
      }

done:
   if (!fAlreadyInCritSec)
      LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CImageDatabaseThread::LimitsSet - Set the maximum limits for disk space usage, etc.

inputs
   __int64           iDataMaximum - MAximum data, in bytes
   __int64           iDataMinimumHardDrive - Minimum hard drive free, in bytes
   __int64           iDataGreedy - If have less data than this then accept all incoming
   DWORD             dwDataMaxEntries - Maximum number of entries in the database
returns
   none
*/
void CImageDatabaseThread::LimitsSet (__int64 iDataMaximum, __int64 iDataMinimumHardDrive,
                                      __int64 iDataGreedy, DWORD dwDataMaxEntries)
{
   EnterCriticalSection (&m_CritSec);
   m_iDataMaximum = iDataMaximum;
   m_iDataMinimumHardDrive = iDataMinimumHardDrive;
   m_iDataGreedy = iDataGreedy;
   m_dwDataMaxEntries = dwDataMaxEntries;
   LeaveCriticalSection (&m_CritSec);
}

/*************************************************************************************
CImageDatabaseThread::CleanUp - Called occasionally to delete files if there are too
many, etc.

inputs
   BOOL              fAlreadyInCritSec - If TRUE then already in the critical seciton.
                        If FALSE, then will enter critical section by self.
   __int64           iTime - Time stamp, used to encourage randomness
returns
   none
*/
void CImageDatabaseThread::CleanUp (BOOL fAlreadyInCritSec, __int64 iTime)
{
   // get the free space on the hard drive
   ULARGE_INTEGER iFree, iTotal, iTotal2;
   memset (&iFree, 0, sizeof(iFree));
   GetDiskFreeSpaceExW (m_ImageDatabase.m_szPath, &iFree, &iTotal, &iTotal2);
   __int64 iFreeDisk = *((__int64*)&iFree);
   
   
   if (!fAlreadyInCritSec)
      EnterCriticalSection (&m_CritSec);

   // how much hard drive need to free up
   __int64 iTargetSize = m_iDataMaximum;
   if (iFreeDisk < m_iDataMinimumHardDrive)
      iTargetSize = m_ImageDatabase.m_iTotalFileSize - (m_iDataMinimumHardDrive - iFreeDisk);

   DWORD dwMaxTries = 1000;   // don't get stuck in infinite loop, so only so much
            // deleting per go
   DWORD i, j;
   __int64 iRand;
   for (i = 0; i < dwMaxTries; i++) {
      // if meet requirements, then done
      if (!m_ImageDatabase.Num())
         break;
      BOOL fTooMuchDisk = (m_ImageDatabase.m_iTotalFileSize > iTargetSize);
      BOOL fTooManyEntries = (m_ImageDatabase.Num() > m_dwDataMaxEntries);
      if (!fTooMuchDisk  &&  !fTooManyEntries)
         break;

      // if too many entries, then emphasize deleting only "empty" entries without associated data
      DWORD dwTries = (fTooManyEntries ? 10 : 1);
      for (; dwTries >= 1; dwTries--) {
         // else, pick an entry at random
         iRand = ( (__int64)rand() << 16) + (__int64)rand();
         iRand += iTime;
         if (iRand < 0)
            iRand = -iRand;
         iRand = iRand % (__int64)m_ImageDatabase.Num();

         DWORD dwIndex = (DWORD)iRand;

         PIMAGEDATABASEENTRY pde = m_ImageDatabase.Get (dwIndex);
         if (!pde)
            continue;

         // make sure this is empty
         if (dwTries > 1) {
            for (j = 0; j < IMAGEDATABASE_MAXQUALITIES; j++)
               if (pde->aQuality[j].dwIndexFile)
                  break;
            if (j < IMAGEDATABASE_MAXQUALITIES)
               continue;   // has data, so don't delete
         }

         // if get here, can remove, either because has no data, or because
         // have given up and allowed to delete one with data
         m_ImageDatabase.Remove (pde->pszStringFromHash, TRUE, &m_CritSec);
         break;
      }
   } // i

   if (!fAlreadyInCritSec)
      LeaveCriticalSection (&m_CritSec);
}


/*************************************************************************************
CImageDatabaseThread::EntryExists - See if an entry exists.

inputs
   BOOL              fAlreadyInCritSec - If TRUE then already in the critical seciton.
                        If FALSE, then will enter critical section by self.
   PCWSTR            pszString - String
   DWORD             dwQuality - Quality setting
   DWORD             dwQualityMax - Quality can get up to
   DWORD             dwType - IMAGEDATABASETYPE_XXX
returns
   int - Best quality value that found (0+), or -1 if can't find a match
*/
int CImageDatabaseThread::EntryExists (BOOL fAlreadyInCritSec, PCWSTR pszString,
                                       DWORD dwQuality, DWORD dwQualityMax, DWORD dwType)
{
   int iRet = -1;
   if (!fAlreadyInCritSec)
      EnterCriticalSection (&m_CritSec);

   // if not running then error
   if (!m_fRunning || !m_iDataMaximum)
      goto done;

   // find it
   PIMAGEDATABASEENTRY pde = m_ImageDatabase.Find (pszString, FALSE);
   if (!pde || (pde->dwType != dwType))
      goto done;

   // find the quality
   dwQualityMax = min(dwQualityMax, IMAGEDATABASE_MAXQUALITIES);
   dwQualityMax = max(dwQualityMax, 1);

   // rememeber that requested
   pde->iTimesRequested++;

   DWORD dwQualityCur;
   for (dwQualityCur = dwQualityMax-1; (dwQualityCur < dwQualityMax) && (dwQualityCur >= dwQuality); dwQualityCur--)
      if (pde->aQuality[dwQualityCur].dwIndexFile) {
         iRet = (int)dwQualityCur; // found a file that can send
         break;
      }


done:
   if (!fAlreadyInCritSec)
      LeaveCriticalSection (&m_CritSec);
   return iRet;
}


/*************************************************************************************
CImageDatabaseThread::EntryAdd - Adds an entry to the database. This ends up
adding it to the queue.

inputs
   BOOL              fAlreadyInCritSec - If TRUE then already in the critical seciton.
                        If FALSE, then will enter critical section by self.
   DWORD             dwAction - As per IDTENTRYQUEUE.dwAction. 0 = add entry,
                           1 = requesting data, 2 = waiting to download data
   PCWSTR            pszString - String
   DWORD             dwQuality - Quality setting
   DWORD             dwQualityMax - Used if "requesting data" action
   DWORD             dwType - IMAGEDATABASETYPE_XXX
   DWORD             dwConnectionID - Connection ID, so can see if the connection
                     has been trying to add lots of data already, etc.
   PVOID             pData - Data
   size_t            iDataSize - Size of pData.
returns
   BOOL - TRUE if success
*/

BOOL CImageDatabaseThread::EntryAdd (BOOL fAlreadyInCritSec, DWORD dwAction, PCWSTR pszString,
                                     DWORD dwQuality, DWORD dwQualityMax, DWORD dwType, DWORD dwConnectionID,
                                     PVOID pData, size_t iDataSize)
{
   BOOL fRet = TRUE;
   if (!fAlreadyInCritSec)
      EnterCriticalSection (&m_CritSec);

   // if not running then error
   if (!m_fRunning || !m_iDataMaximum) {
      fRet = FALSE;
      goto done;
   }

   // add this to the list
   IDTENTRYQUEUE eq;
   memset (&eq, 0, sizeof(eq));
   DWORD dwSize = (DWORD)((wcslen(pszString)+1) * sizeof(WCHAR));
   eq.pszString = (PCWSTR) ESCMALLOC(dwSize);
   if (!eq.pszString) {
      fRet = FALSE;
      goto done;
   }
   if (pData) {
      eq.pData = ESCMALLOC(iDataSize);
      if (!eq.pData) {
         ESCFREE ((PVOID) eq.pszString);
         fRet = FALSE;
         goto done;
      }
   }
   memcpy ((PVOID) eq.pszString, pszString, dwSize);
   if (eq.pData)
      memcpy (eq.pData, pData, iDataSize);
   eq.iDataSize = iDataSize;
   eq.dwAction = dwAction;
   eq.dwConnectionID = dwConnectionID;
   eq.dwQuality = dwQuality;
   eq.dwQualityMax = dwQualityMax;
   eq.dwType = dwType;

   // insert so that dwConnectionID in order, and at end of the current list
   PIDTENTRYQUEUE peq = (PIDTENTRYQUEUE)m_lIDTENTRYQUEUE.Get(0);
   DWORD i;
   DWORD dwInQueueForThisConnection = 0;
   for (i = 0; i < m_lIDTENTRYQUEUE.Num(); i++, peq++) {
      if (eq.dwConnectionID == peq->dwConnectionID)
         dwInQueueForThisConnection++;
      if (eq.dwConnectionID < peq->dwConnectionID)
         break;
   }

   // if the player it trying to queue up too many things (perhaps trying to crash
   // the system), then don't add this
   if (dwInQueueForThisConnection > 100)
      goto done;

   if (i < m_lIDTENTRYQUEUE.Num())
      m_lIDTENTRYQUEUE.Insert (i, &eq);
   else
      m_lIDTENTRYQUEUE.Add (&eq);

   SetEvent (m_hSignalToThread);

done:
   if (!fAlreadyInCritSec)
      LeaveCriticalSection (&m_CritSec);
   return fRet;
}



/*************************************************************************************
CImageDatabase::Constructor and destructor
*/
CImageDatabase::CImageDatabase (void)
{
   m_szPath[0] = 0;
   m_iTotalFileSize = 0;
   m_iFileTime = 0;
}

CImageDatabase::~CImageDatabase (void)
{
   // save
   if (m_szPath[0])
      IndexSave ();

   // free up the memory
   DWORD dwEntry;
   PIMAGEDATABASEENTRY pide;
   for (dwEntry = 0; dwEntry < m_hEntries.Num(); dwEntry++) {
      pide = *((PIMAGEDATABASEENTRY*) m_hEntries.Get (dwEntry));
      if (!pide)
         continue;   // error, but ignore

      // NOTE: Don't free pide->pszStringFromHash
      ESCFREE (pide);
   } // i
   m_hEntries.Clear();
}



/*************************************************************************************
CImageDatabase::Init - Call this once to initialize the image database.

inputs
   PCWSTR         pszPath - Directory where the image database will be,
                  excluding the last backslash. Example: "c:\AshtariEmpire\ImageDatabase".
                  If the directory doesn't exist, it will be created.
   __int64        iFileTime - If the file time is different than the last file time
                  then the cache is autmoatically deleted
returns
   BOOL - TRUE if success
*/
BOOL CImageDatabase::Init (PCWSTR pszPath, __int64 iFileTime)
{
   // copy over the path name
   if (wcslen(pszPath)+1 > sizeof(m_szPath)/sizeof(WCHAR))
      return FALSE;
   if (m_szPath[0])
      return FALSE;
   wcscpy (m_szPath, pszPath);

   // try creating the directory just in case
   if (!CreateDirectoryW (m_szPath, NULL)) {
      if (GetLastError() != ERROR_ALREADY_EXISTS)
         return FALSE;
   }

   m_iFileTime = iFileTime;

   // enum contents of subdirectories
   CMem memSizes;
   if (!EnumSubDirectories(&memSizes))
      return FALSE;

   // read in the file
   if (!IndexLoad (&memSizes))
      return FALSE;

   // loop through all the file sizes and create a bit-wise field of what
   // file numbers used, as well as deleting files that no longer exist
   __int64 *paiSizes = (__int64*)memSizes.p;
   DWORD dwNumSizes = (DWORD)(memSizes.m_dwCurPosn / sizeof(__int64));
   DWORD dwBitNum = (dwNumSizes + 31) / 32;
   if (!m_memFilesUsed.Required (dwBitNum * sizeof(DWORD)))
      return FALSE;
   m_memFilesUsed.m_dwCurPosn = dwBitNum * sizeof(DWORD);
   DWORD *padwBitField = (DWORD*)m_memFilesUsed.p;
   memset (padwBitField, 0, m_memFilesUsed.m_dwCurPosn);

   DWORD i;
   WCHAR szFileName[512];
   for (i = 0; i < dwNumSizes; i++) {
      if (paiSizes[i] < 0) {
         // this was used, so note in the bit field
         padwBitField[i / 32] |= (1 << (i % 32));
         continue;
      }
      else if (paiSizes[i] > 0) {
         // this file is no longer in the database, so delete
         FileName (i, szFileName);
         DeleteFileW (szFileName);
      }
   } // i

   // done
   return TRUE;
}


/*************************************************************************************
CImageDatabase::EnumSubDirectories - This looks through all the directories and sub-directories
where the files are stored and sees what's still around. The file sizes are filled into
pMemSizes.

inputs
   PCMem       pMemSized - Filled with an __int64 for each file (by index number) when a
                           file is found. 0 if no file is found. m_dwCurPosn is filled in
                           with NumFiles * sizeof(__int64)
returns
   BOOL - TRUE if success
*/
BOOL CImageDatabase::EnumSubDirectories (PCMem pMemSizes)
{
   // enumerate the directories in the path
   DWORD dwMaxDir = 0;
   DWORD dwNum;
   WCHAR szSearchPath[512];
   WCHAR szTemp[256];
   WIN32_FIND_DATAW  fd;
   swprintf (szSearchPath, L"%s\\dir*", m_szPath);
   HANDLE hFind = FindFirstFileW (szSearchPath, &fd);
   if (hFind == INVALID_HANDLE_VALUE)
      return TRUE;   // nothing there
   
   while (TRUE) {
      // must be directory
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
         goto nextfile;

      // get the number
      dwNum = _wtoi (fd.cFileName + 3 /* "dir" */);

      // go the opposite direction and make sure it still exists
      swprintf (szTemp, L"dir%d", (int)dwNum);
      if (_wcsicmp(szTemp, fd.cFileName)) {
         _ASSERTE (!_wcsicmp(szTemp, fd.cFileName));
         goto nextfile;   // not the same
      }

      // add to the list
      dwMaxDir = max(dwMaxDir, dwNum+1);

nextfile:
      if (!FindNextFileW (hFind, &fd))
         break;

      // else, found
      continue;
   }

   // close this search
   FindClose (hFind);

   // make sure enough memory allocated
   size_t dwNeed = FILESPERDATABASEDIR * dwMaxDir * sizeof(__int64);
   if (!pMemSizes->Required (dwNeed))
      return FALSE;
   pMemSizes->m_dwCurPosn = dwNeed;
   memset (pMemSizes->p, 0, dwNeed);
   __int64 *paiSizes = (__int64*)pMemSizes->p;

   // loop through and enumerate the contents
   DWORD dwCurDir;
   for (dwCurDir = 0; dwCurDir < dwMaxDir; dwCurDir++) {
      swprintf (szSearchPath, L"%s\\dir%d\\dat*.dat", m_szPath, (int)dwCurDir);
      hFind = FindFirstFileW (szSearchPath, &fd);
      if (hFind == INVALID_HANDLE_VALUE)
         continue;   // nothing there

      while (TRUE) {
         // must NOT be directory
         if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            goto nextfile2;

         // get the number
         dwNum = _wtoi (fd.cFileName + 3 /* "dat" */);

         // go the opposite direction and make sure it still exists
         swprintf (szTemp, L"dat%d.dat", (int)dwNum);
         if (_wcsicmp(szTemp, fd.cFileName)) {
            _ASSERTE (!_wcsicmp(szTemp, fd.cFileName));
            goto nextfile2;   // not the same
         }

         // make sure not too large/small
         if ((dwNum < dwCurDir * FILESPERDATABASEDIR) || (dwNum >= (dwCurDir+1) * FILESPERDATABASEDIR))
            goto nextfile2;   // out of range, shouldnt happen

         // add to the list
         paiSizes[dwNum] = ((__int64)fd.nFileSizeHigh << 32) + (__int64)fd.nFileSizeLow;

nextfile2:
         if (!FindNextFileW (hFind, &fd))
            break;

         // else, found
         continue;
      }

      // close this search
      FindClose (hFind);
   } // dwCurDIr

   // done
   return TRUE;
}


/*************************************************************************************
CImageDatabase::ToLowerForHash - Lower-cases for the purpose of the hash.
Copies the string into pMemTemp.

inputs
   PCWSTR         psz - String
   PCMem          pMemTemp - Temporary memory to use
returns
   PCWSTR - Lower cased version, in pMemTemp
*/
PCWSTR CImageDatabase::ToLowerForHash (PCWSTR psz, PCMem pMemTemp)
{
   DWORD dwLen = (DWORD) wcslen(psz);

   if (!pMemTemp->Required ((dwLen+1) * sizeof(WCHAR)))
      return NULL;

   PWSTR pszLower = (PWSTR)pMemTemp->p;
   memcpy (pszLower, psz, (dwLen+1) * sizeof(WCHAR));
   psz = pszLower;
   
   DWORD i;
   for (i = 0; i < dwLen; i++, pszLower++)
      pszLower[0] = towlower(pszLower[0]);
   
   return (PCWSTR)psz;
}

#if 0 // no longer used
/*************************************************************************************
CImageDatabase::Hash - Creates a hash of the string, used to index m_hEntries.

inputs
   PCWSTR         psz - String
   BOOL           fAlreadyLower - If this string is already known to be lower, then
                  set to TRUE. If FALSE, the string will be copied to a new memory
                  location and lower-cased.
returns
   DWORD - Hash value
*/
DWORD CImageDatabase::Hash (PCWSTR psz, BOOL fAlreadyLower)
{
   DWORD dwLen = (DWORD) wcslen(psz);
   DWORD i;

   if (!fAlreadyLower) {
      PCWSTR pszLower = ToLowerForHash (psz);
      if (pszLower)
         psz = pszLower;
   }

   DWORD dwHash = 0;
   BYTE *pabHash = (BYTE*)&dwHash;

   DWORD dwSum = 0;
   for (i = 0; i < dwLen; i++) {
      pabHash[i%sizeof(dwHash)] ^= (BYTE)psz[i];
      dwSum += psz[i];
   }
   dwHash ^= (dwSum << 16);

   return dwHash;
}
#endif // 0

/*************************************************************************************
CImageDatabase::IndexLoad - Load in the index file. This assumes that the object
is new and Init() is being called.

Any files NOT mathching pMemSizes are deleted. Any files which match have pMemSizes->p[dwNum]
set to -1.

This also keeps track of m_iTotalFileSize.

inputs
   PCMem       pMemSizes - From EnumSubDirectories()
returns
   BOOL - TRUE if success
*/
BOOL CImageDatabase::IndexLoad (PCMem pMemSizes)
{
   DWORD dwNumSizes = (DWORD)(pMemSizes->m_dwCurPosn / sizeof(__int64));
   __int64 *paiSizes = (__int64*)pMemSizes->p;
   m_iTotalFileSize = 0;

   // name of the index file
   WCHAR szIndexName[512];
   swprintf (szIndexName, L"%s\\Index.dat", m_szPath);
   FILE *pf = _wfopen (szIndexName, L"rb");
   if (!pf) {
      m_hEntries.Init (sizeof(PIMAGEDATABASEENTRY), 1000);
      return TRUE;   // can't open the index file, so assume blank
   }

   // read in the header
   IMAGEDATABASEINDEXHEADER ih;
   memset (&ih, 0, sizeof(ih));
   if (1 != fread (&ih, sizeof(ih), 1, pf)) {
      // cant read in the index header, so must be a bad file
      fclose (pf);
      m_hEntries.Init (sizeof(PIMAGEDATABASEENTRY), 1000);
      return TRUE;
   }
   if (ih.dwID != IMAGEDATABASEINDEXHEADER_ID) {
      // bad ID, so assume nothing
      fclose (pf);
      m_hEntries.Init (sizeof(PIMAGEDATABASEENTRY), 1000);
      return TRUE;
   }
   if (ih.iFileTime != m_iFileTime) {
      // bad ID, so assume nothing
      fclose (pf);
      m_hEntries.Init (sizeof(PIMAGEDATABASEENTRY), 1000);
      return TRUE;
   }

   // initialize the hash
   m_hEntries.Init (sizeof(PIMAGEDATABASEENTRY), ih.dwEntries * 2);

   // loop through all the entries
   DWORD dwIndex, dwQuality;
   IMAGEDATABASEINDEXENTRY ide;
   for (dwIndex = 0; dwIndex < ih.dwEntries; dwIndex++) {
      if (1 != fread (&ide, sizeof(ide), 1, pf))
         break;   // error in the file, so stopping now

      // verify all the entries
      for (dwQuality = 0; dwQuality < IMAGEDATABASE_MAXQUALITIES; dwQuality++) {
         if (!ide.aQuality[dwQuality].dwIndexFile)
            continue;   // empty

         // if value too high, then file must not exist, so delete
         DWORD dwIndexFile = ide.aQuality[dwQuality].dwIndexFile - 1;
         if (dwIndexFile >= dwNumSizes) {
            ide.aQuality[dwQuality].dwIndexFile = 0;
            ide.aQuality[dwQuality].iFileSize = 0;
            continue;
         }

         // make sure memory matches up
         if (ide.aQuality[dwQuality].iFileSize != paiSizes[dwIndexFile]) {
            // wrong size, so assume some sort of corruption and delete
            ide.aQuality[dwQuality].dwIndexFile = 0;
            ide.aQuality[dwQuality].iFileSize = 0;
            continue;
         }
      } // dwQuality

      // read in the string
      PWSTR pszString = (PWSTR) ESCMALLOC (ide.dwSizeString);
      if (!pszString || (ide.dwSizeString < sizeof(WCHAR)) )
         // shouldnt happen unless file, was corrupt, so assume everything is good
         // before this
         break;
      if (ide.dwSizeString != fread (pszString, 1, ide.dwSizeString, pf)) {
         // out of file. assume that everything was good before this.
         ESCFREE (pszString);
         break;
      }

      // make sure NULL terminated so don't crash
      if (pszString[ide.dwSizeString / sizeof(WCHAR)-1]) {
         ESCFREE (pszString);
         break;
      }

      // allocate memory for index entry
      PIMAGEDATABASEENTRY pide = (PIMAGEDATABASEENTRY) ESCMALLOC(sizeof(IMAGEDATABASEENTRY));
      if (!pide) {
         ESCFREE (pszString);
         break;
      }

      // copy over
      memset (pide, 0, sizeof(*pide));
      pide->iTimesRequested = ide.iTimesRequested;
      pide->iTimesDelivered = ide.iTimesDelivered;
      pide->dwType = ide.dwType;
      _ASSERTE (sizeof(ide.aQuality) == sizeof(pide->aQuality));
      memcpy (&pide->aQuality[0], &ide.aQuality[0], sizeof(ide.aQuality));

      // NOTE: Assuming the string is already lower-cased
      if (!m_hEntries.Add (pszString, &pide, FALSE)) {
         // memory error, shouldnt happen
         ESCFREE (pszString);
         ESCFREE (pide);
         continue;
      }

      // get the string number
      // NOTE: Not bothering with error checking because has to succede
      DWORD dwIndex = m_hEntries.FindIndex (pszString, FALSE);
      ESCFREE (pszString);
      pide->pszStringFromHash = m_hEntries.GetString (dwIndex);

      // update some info now that have added
      for (dwQuality = 0; dwQuality < IMAGEDATABASE_MAXQUALITIES; dwQuality++) {
         if (!pide->aQuality[dwQuality].dwIndexFile)
            continue;   // empty

         // if value too high, then file must not exist, so delete
         DWORD dwIndexFile = pide->aQuality[dwQuality].dwIndexFile - 1;

         paiSizes[dwIndexFile] = -1;
         m_iTotalFileSize += pide->aQuality[dwQuality].iFileSize;
      }
   } // dwIndex


   fclose (pf);

   return TRUE;
}


/*************************************************************************************
CImageDatabase::IndexSave - Saves the index file.

inputs
   none
returns
   BOOL - TRUE if success
*/
BOOL CImageDatabase::IndexSave (void)
{
   if (!m_szPath[0])
      return FALSE;  // not initialized

   // name of the index file
   WCHAR szIndexName[512];
   swprintf (szIndexName, L"%s\\Index.dat", m_szPath);
   FILE *pf = _wfopen (szIndexName, L"wb");
   if (!pf)
      return FALSE;   // can't open the index file, so assume blank

   IMAGEDATABASEINDEXHEADER ih;
   memset (&ih, 0, sizeof(ih));
   ih.dwID = IMAGEDATABASEINDEXHEADER_ID;
   ih.dwEntries = m_hEntries.Num();
   ih.iFileTime = m_iFileTime;
   if (1 != fwrite (&ih, sizeof(ih), 1, pf)) {
      fclose (pf);
      return FALSE;
   }

   // for all the entries
   DWORD dwEntry;
   IMAGEDATABASEINDEXENTRY ide;
   PIMAGEDATABASEENTRY pide;
   for (dwEntry = 0; dwEntry < m_hEntries.Num(); dwEntry++) {
      pide = *((PIMAGEDATABASEENTRY*) m_hEntries.Get (dwEntry));
      if (!pide)
         continue;   // error, but ignore

      // copy over and write
      memset (&ide, 0, sizeof(ide));
      ide.dwType = pide->dwType;
      ide.dwSizeString = (DWORD)(wcslen(pide->pszStringFromHash) + 1) * sizeof(WCHAR);
      ide.iTimesDelivered = pide->iTimesDelivered;
      ide.iTimesRequested = pide->iTimesRequested;
      memcpy (&ide.aQuality[0], &pide->aQuality[0], sizeof(pide->aQuality));
      if (1 != fwrite (&ide, sizeof(ide), 1, pf)) {
         fclose (pf);
         return FALSE;
      }

      // write string
      if (ide.dwSizeString != fwrite (pide->pszStringFromHash, 1, ide.dwSizeString, pf)) {
         fclose (pf);
         return FALSE;
      }
   } // dwEntry

   fclose (pf);
   return TRUE;
}


/*************************************************************************************
CImageDatabase::FileName - Given a file number, this generates a name for it.

inputs
   DWORD          dwFileNumber - File number, 0+
   PWSTR          pszFileName - Filled in with the file name, like "c:\ashtariempire\index\dir1\dat123.dat".
                     This should be 512 chars long (approx)
returns
   none
*/
void CImageDatabase::FileName (DWORD dwFileNumber, PWSTR pszFileName)
{
   swprintf (pszFileName, L"%s\\dir%d\\dat%d.dat", m_szPath, (int)(dwFileNumber / FILESPERDATABASEDIR),
      (int)dwFileNumber);
}


/*************************************************************************************
CImageDatabase::Add - Adds an entry to the database.

inputs
   PCWSTR         pszString - String for identification.
returns
   PIMAGEDATABASEENTRY - Where the entry is stored
*/
PIMAGEDATABASEENTRY CImageDatabase::Add (PCWSTR pszString)
{
   // lower case this
   CMem memTemp;
   PCWSTR pszLower = ToLowerForHash (pszString, &memTemp);
   if (!pszLower)
      return NULL;

   // make sure it doesn't already exist
   if (m_hEntries.Find ((PWSTR) pszLower, FALSE))
      return NULL;   // already exists

   // create string
   DWORD dwLen = (DWORD) wcslen(pszLower);

   // create memory
   PIMAGEDATABASEENTRY pEntry = (PIMAGEDATABASEENTRY) ESCMALLOC(sizeof(IMAGEDATABASEENTRY));
   if (!pEntry)
      return NULL;
   memset (pEntry, 0, sizeof(*pEntry));

   // add it
   if (!m_hEntries.Add ((PWSTR) pszLower, &pEntry, FALSE)) {
      ESCFREE (pEntry);
      return FALSE;
   }

   // NOTE: Know that will find the entry because just added
   DWORD dwIndex = m_hEntries.FindIndex ((PWSTR) pszLower, FALSE);
   pEntry->pszStringFromHash = m_hEntries.GetString (dwIndex);

   return pEntry;
}


/*************************************************************************************
CImageDatabase::SaveFileNum - Saves a chunk of memory as a new file number.

inputs
   PVOID          pMem - Memory to save
   size_t         iSize - Size to save
   CRITICAL_SECTION  *pCritSec - If not NULL, then SaveFileNum() will ExitCriticalSection(pCritSec)
                  right before saving the file, and EnterCriticalSection(pCritSec) immediately
                  afterwards.
returns
   DWORD - File index number + 1. Returns 0 if can't save
*/
DWORD CImageDatabase::SaveFileNum (PVOID pMem, size_t iSize, CRITICAL_SECTION *pCritSec)
{
   // see if can find an empty slot
   DWORD dwMaxLook = (DWORD) m_memFilesUsed.m_dwCurPosn / sizeof(DWORD);
   DWORD *padwBits = (DWORD*)m_memFilesUsed.p;
   DWORD i;
   for (i = 0; i < dwMaxLook; i++)
      if (padwBits[i] != (DWORD)-1)
         break;

   // if can't find a slot then extend memory
   if (i >= dwMaxLook) {
      if (!m_memFilesUsed.Required ((dwMaxLook+1) * sizeof(DWORD)))
         return 0;   // error
      m_memFilesUsed.m_dwCurPosn = (dwMaxLook+1) * sizeof(DWORD);
      padwBits = (DWORD*)m_memFilesUsed.p;
      padwBits[dwMaxLook] = 0;   // to clear
      i = dwMaxLook;
   }

   // within this one, find an empty slot
   DWORD dwBit;
   for (dwBit = 0; dwBit < 32; dwBit++)
      if (!(padwBits[i] & (1 << dwBit)))
         break;
   if (dwBit >= 32)
      return 0;   // shouldnt happen

   DWORD dwIndex = i * 32 + dwBit;

   if (pCritSec)
      LeaveCriticalSection (pCritSec);

   // write the file
   WCHAR szFileName[512];
   FileName (dwIndex, szFileName);
   FILE *pf = _wfopen (szFileName, L"wb");
   if (!pf) {
      // try creating the directory
      WCHAR szDir[512];
      swprintf (szDir, L"%s\\dir%d", m_szPath, (int)dwIndex / FILESPERDATABASEDIR);
      CreateDirectoryW (szDir, NULL);

      pf = _wfopen (szFileName, L"wb");
      if (!pf) {
         if (pCritSec)
            EnterCriticalSection (pCritSec);
         return 0;   // error, can't save
      }
   }

   // save
   if (1 != fwrite (pMem, iSize, 1, pf)) {
      fclose (pf);
      if (pCritSec)
         EnterCriticalSection (pCritSec);
      return 0; // error
   }
   fclose (pf);   // saved successfully
   if (pCritSec)
      EnterCriticalSection (pCritSec);

   // mark as having been used
   padwBits[i] |= (1 << dwBit);
   m_iTotalFileSize += iSize;
   return dwIndex + 1;
}

/*************************************************************************************
CImageDatabase::DeleteFileNum - Given a file number, delete it, and reduce the memory used.

inputs
   DWORD          dwFileNum - File number, 0+
   __int64        iFileSize - File size
   CRITICAL_SECTION  *pCritSec - If not NULL, then SaveFileNum() will ExitCriticalSection(pCritSec)
                  right before deleting the file, and EnterCriticalSection(pCritSec) immediately
                  afterwards.
returns
   BOOL - TRUE if deleted
*/
BOOL CImageDatabase::DeleteFileNum (DWORD dwFileNum, __int64 iFileSize, CRITICAL_SECTION  *pCritSec)
{
   DWORD dwMaxFiles = (DWORD) m_memFilesUsed.m_dwCurPosn * 8;
   if (dwFileNum >= dwMaxFiles)
      return FALSE;  // out of range

   // check the bit
   DWORD *padwBits = (DWORD*)m_memFilesUsed.p + (dwFileNum / 32);
   DWORD dwBit = (1 << (dwFileNum % 32));
   if (!(padwBits[0] & dwBit))
      return FALSE;  // doesn't exist

   // delete the file
   WCHAR szTemp[512];
   FileName (dwFileNum, szTemp);
   BOOL fRet;
   if (pCritSec)
      LeaveCriticalSection (pCritSec);
   fRet = DeleteFileW (szTemp);
   if (pCritSec)
      EnterCriticalSection (pCritSec);
   _ASSERTE (fRet);

   // reduce memory
   padwBits[0] &= ~dwBit;  // remove
   m_iTotalFileSize -= iFileSize;
   _ASSERTE (m_iTotalFileSize >= 0);

   return TRUE;
}

/*************************************************************************************
CImageDatabase::Remove - Removes an entry from the database.

inputs
   PCWSTR         pszString - String for identification.
   BOOL           fAlreadyLower - If this string is already known to be lower, then
                  set to TRUE. If FALSE, the string will be copied to a new memory
                  location and lower-cased.
   CRITICAL_SECTION  *pCritSec - If not NULL, then SaveFileNum() will ExitCriticalSection(pCritSec)
                  right before deleting the file, and EnterCriticalSection(pCritSec) immediately
                  afterwards.
returns
   BOOL - TRUE if found and removed. FALSE if couldnt find
*/
BOOL CImageDatabase::Remove (PCWSTR pszString, BOOL fAlreadyLower, CRITICAL_SECTION *pCritSec)
{
   // BUGFIX - Always lower, so have string copied into temporary memory,
   // since may pass in pszStringFromHash, causing potential of memory change
   // in between when leave critical sction
   CMem memTemp;
   pszString = ToLowerForHash (pszString, &memTemp);

rescan:
   DWORD dwIndex = m_hEntries.FindIndex ((PWSTR) pszString, FALSE);
   if (dwIndex == (DWORD)-1)
      return FALSE;  // not found
   
   // delete all the files it points to
   PIMAGEDATABASEENTRY *ppde = (PIMAGEDATABASEENTRY*)m_hEntries.Get (dwIndex);
   PIMAGEDATABASEENTRY pde = ppde ? *ppde : NULL;
   DWORD dwQuality;
   if (pde) {
      // delete files
      for (dwQuality = 0; dwQuality < IMAGEDATABASE_MAXQUALITIES; dwQuality++)
         if (pde->aQuality[dwQuality].dwIndexFile) {
            DWORD dwIndexFile = pde->aQuality[dwQuality].dwIndexFile-1;
            __int64 iFileSize = pde->aQuality[dwQuality].iFileSize;
            pde->aQuality[dwQuality].dwIndexFile = 0;
            pde->aQuality[dwQuality].iFileSize = 0;

            // delete
            DeleteFileNum (dwIndexFile, iFileSize, pCritSec);

            // if passed a pCritSec, then need to re-do the whole search because
            // data may have changed
            if (pCritSec)
               goto rescan;
         }

      // NOTE: Not freeing pde->pszStringFromHash

      ESCFREE (pde);
   }

   // delete the entry
   return m_hEntries.Remove (dwIndex);
}

/*************************************************************************************
CImageDatabase::Find - Finds an entry in the database.

inputs
   PCWSTR            pszString - String.
   BOOL           fAlreadyLower - If this string is already known to be lower, then
                  set to TRUE. If FALSE, the string will be copied to a new memory
                  location and lower-cased.
returns
   PIMAGEDATABASEENTRY - Entry
*/
PIMAGEDATABASEENTRY CImageDatabase::Find (PCWSTR pszString, BOOL fAlreadyLower)
{
   // if not already lower, make it so
   CMem memTemp;
   if (!fAlreadyLower)
      pszString = ToLowerForHash (pszString, &memTemp);

   PIMAGEDATABASEENTRY *ppde = (PIMAGEDATABASEENTRY*)m_hEntries.Find ((PWSTR)pszString, FALSE);
   PIMAGEDATABASEENTRY pde = ppde ? *ppde : NULL;

   return pde;
}


/*************************************************************************************
CImageDatabase::Num - Returns the number of entries in the database
*/
DWORD CImageDatabase::Num (void)
{
   return m_hEntries.Num();
}


/*************************************************************************************
CImageDatabase::Get - Given an index number, from 0..Num(), this returns the
database entry

returns
   PIMAGEDATABASEENTRY - Entry. NULL if error
*/
PIMAGEDATABASEENTRY CImageDatabase::Get (DWORD dwIndex)
{
   PIMAGEDATABASEENTRY *ppde = (PIMAGEDATABASEENTRY*)m_hEntries.Get (dwIndex);
   PIMAGEDATABASEENTRY pde = ppde ? *ppde : NULL;

   return pde;
}


