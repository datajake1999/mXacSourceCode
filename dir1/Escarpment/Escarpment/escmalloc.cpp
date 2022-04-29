/************************************************************************
EscMalloc.cpp - Custom memory allocation code

begun 14/5/2005 by Mike Rozak
Copyright 2005 Mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "resleak.h"

#define MALLOCBINS            (MAXRAYTHREAD/2)      // number of bins for memory
#define THREADSPERBIN         4        // remember this many threads per bin

// #define MALLOCBINSZEROS       {0}
#define MALLOCBINSZEROS       {0, 0, 0, 0, 0, 0, 0, 0}

#define MALLOCLEVELS          4        // number of malloc levels
                                       // 0 is based off VirtualAlloc()
                                       // MALLOCLEVELS is based of an array for very small items
#define MAXSUBBLOCKS          8       // maximum number of sub-blocks at each malloc-level
#define VIRTUALBLOCKSIZE      (1024*1024)  // 1-megabyte virtual blocks
#define MEMTINYSIZE           256      // fill in one array if smaller than this
#define MEMSMALLSIZE          1024     // fill in second array if smaller than this
#define MEMTINYSIZEQUANT      8       // quanitization for MEMTINYSIZE
#define MEMSMALLSIZEQUANT     64      // quantization for MEMSMALLSIZE
#define MEMTINYSIZEBLOCK      (MEMTINYSIZE*50) // size of the block looking for
#define MEMSMALLSIZEBLOCK     MEMTINYSIZEBLOCK  // BUGFIX - so larger block sizes the same (MEMSMALLSIZE*20) // size of the block looking for

#ifdef _DEBUG
// #define USESLOWMEMORYCHECKS         // use really slow memory checks
   // BUGBUG - should disable this
   // BUGBUG - have disabled it, as it should be, since seems to be working
#endif


#ifdef USESLOWMEMORYCHECKS
#define OVERRUNSIZE           800       // number of bytes for overrun tests. Must be multiple of 4
#else
#define OVERRUNSIZE           16       // number of bytes for overrun tests. Must be multiple of 4
#endif

#define OVERRUNBYTE           0xcd     // so can identify overrun
#define FREEMEMBYTE           0xdc     // free memory byte
#define UNIQUEBLOCKID         1234567
#define UNIQUESUBBLOCKID      7654321

// BLOCKHDR - header at the top of every memory block
typedef struct {
   DWORD             dwType;           // 0..MALLOCLEVELS-1
   size_t            dwOrigSize;       // orignal size requested for sub-block size... used for tiny and small blocks
   size_t            dwTotalSize;      // total size of the block
   size_t            dwSubBlockSizeWithHeader;  // how large each sub-block is, including headers
   size_t            dwSubBlockSizeNoHeader; // usable memory in sub-block
   DWORD             dwMax;            // maximum number of subblocks
   DWORD             dwFree;           // number of free subblocks
   DWORD             dwLastFree;       // index to last freed sub-block, or -1 if must look for one
   PVOID             pPrev;            // prvious blockhdr. NULL if at start of list
   PVOID             pNext;            // next blockhdr. NULL if at end of list
#ifdef _DEBUG
   DWORD             dwUniqueID;       // so can be sure freeing the right stuff
#endif
} BLOCKHDR, *PBLOCKHDR;

// SUBBLOCKHDR - Header information for a subblock
typedef struct {
   DWORD             dwBackToHdr;      // number of bytes to move back to get to the BLOCKHDR
                                       // NOTE: this is intentionally a DWORD
                                       // a value of 0 indicates that sub-block is free
                                       // If -1 then indicated allocated with malloc(), free().
   DWORD             dwAllocated;      // number of bytes allocated of out dwSubBlockSizeNoHeader.
                                       // NOTE: Intentionally use DWORD since dont expect to alloc > 4 gb in one chunk
         // BUGFIX - Move dwAllocated to non-debug so can keep track of

   DWORD             dwMallocBin;      // which malloc bin it's in. Only need a byte, but use more

#ifdef _DEBUG
   DWORD             dwUniqueID;       // so can be sure freeing the right stuff
   DWORD             dwAllocatedBy;    // who allocated this
   PCSTR             pszFile;          // file name
   PCSTR             pszFunction;      // function name
   DWORD             dwLine;           // line allocated from
   BYTE              abOverrun[OVERRUNSIZE]; // to test overruns
#endif
} SUBBLOCKHDR, *PSUBBLOCKHDR;


// CLAIMMEMORY
typedef struct {
   PVOID             pMem;             // memory claimed
   size_t            dwSize;           // size of the memory, from dwTotalSize
} CLAIMMEMORY, *PCLAIMMEMORY;


#ifdef _DEBUG
// SUBBLOCKTAIL - Pasted at the end of a sub-block
typedef struct {
   BYTE              abOverrun[OVERRUNSIZE]; // to test overruns
} SUBBLOCKTAIL, *PSUBBLOCKTAIL;
#endif


PVOID InternalMalloc (DWORD dwMallocBin, DWORD dwLevel, DWORD dwSubBlocks, size_t dwSize
#ifdef _DEBUG
                   , DWORD dwAllocatedBy, PCSTR pszFile, PCSTR pszFunction, DWORD dwLine
#endif
                      );
BOOL InternalFree (DWORD dwMallocBin, PVOID pMem, BOOL fIntegrity);
size_t InternalMemorySize (PVOID pMem, BOOL fMax);
DWORD InternalMemoryBin (PVOID pMem);
PVOID InternalMalloc (DWORD dwMallocBin, size_t dwSize
#ifdef _DEBUG
                   , DWORD dwAllocatedBy, PCSTR pszFile, PCSTR pszFunction, DWORD dwLine
#endif
                      );
void InternalCheckMemoryIntegrity (PVOID pMem, BOOL fRequiredBackToHdr);
void EscFreeNoCache (PVOID pMem);
PVOID InternalRealloc (DWORD dwMallocBin, PVOID pMem, size_t dwSize);
void InternalReportUsage (DWORD dwMallocBin);

// globals
CRITICAL_SECTION     gcsAtomicString;     // access to the atomic string
PCHashString         gphAtomicString = NULL;     // hash for the atomic string

CRITICAL_SECTION     gcsMallocBinChoose;  // critical section for thread ID
CRITICAL_SECTION     gaMemoryCritSec[MALLOCBINS];      // critical section for accessing the mmeory
BOOL                 gafInMemoryCritSec[MALLOCBINS] = MALLOCBINSZEROS;  // set to TRUE if in the critical seciton
PBLOCKHDR            gaPBLOCKHDR[MALLOCBINS][MALLOCLEVELS][MAXSUBBLOCKS]; // linked list of block headers
PBLOCKHDR            gaPBLOCKHDRTiny[MALLOCBINS][MEMTINYSIZE/MEMTINYSIZEQUANT];
PBLOCKHDR            gaPBLOCKHDRSmall[MALLOCBINS][MEMSMALLSIZE/MEMSMALLSIZEQUANT];
size_t               gadwMaxAllocatable[MALLOCBINS][MALLOCLEVELS][MAXSUBBLOCKS];  // maximum allocat
DWORD                gadwAllocatedBy[MALLOCBINS] = MALLOCBINSZEROS;  // so can set allocated by bits
BOOL                 afUseSubBlock[MAXSUBBLOCKS] = {TRUE, TRUE, FALSE, TRUE,
                                                   FALSE, FALSE, FALSE, TRUE};
size_t               gaiAllocated[MALLOCBINS] = MALLOCBINSZEROS;     // total memory allocated, with virtualalloc()
size_t               gaiUsed[MALLOCBINS] = MALLOCBINSZEROS;          // total memory used out of the allocated
size_t               gaiNumMalloc[MALLOCBINS] = MALLOCBINSZEROS;     // total number of mallocs

FILE                 *gpfEscarpmentDebug = NULL; // debug file
BOOL                 gfEscOutputDebugStringAlwaysFlush = FALSE;   // set to TRUE if shoudl always flush
static BOOL          gfOptimizeMemory = TRUE;      // optimize for low memory usage

#ifdef _DEBUG
size_t               gaiMaxAllocated[MALLOCBINS] = MALLOCBINSZEROS;  // maximum total allocated
size_t               gaiMaxUsed[MALLOCBINS] = MALLOCBINSZEROS;       // total maximum used
#endif // _DEBUG

DWORD                gadwBinThreadID[MALLOCBINS][THREADSPERBIN];  // threads associated with each bin
__int64              gaiBinLastUsed[MALLOCBINS][THREADSPERBIN];
__int64              gaiBinLastUsedMax[MALLOCBINS] = MALLOCBINSZEROS;      // "time stamp" when the bin was last used
__int64              giBinTimeStamp = 1;  // current time stamp
#ifdef _DEBUG
__int64              giMallocBinCollisions = 0;
#endif

// free-memory threads
HANDLE               gahFreeMemoryThread[MALLOCBINS] = MALLOCBINSZEROS;          // frees up memory
HANDLE               gahFreeMemoryEvent[MALLOCBINS] = MALLOCBINSZEROS;           // event to tell freeup memory thread do so something
BOOL                 gfFreeMemoryWantToQuit = TRUE;                              // set to TRUE if want to quit
__int64              gaiFreeMemoryQueue[MALLOCBINS] = MALLOCBINSZEROS;           // how much is queued up to free. If too much then change thread priority
__int64              giFreeMemoryQueueHigh = 0;                                  // if more than this memory allocated then should be high priority
int                  gaiFreeMemoryPriority[MALLOCBINS];                          // thread priorities currently assigned
PVOID                gapFreeMemoryArray[MALLOCBINS] = MALLOCBINSZEROS;           // array of memory
size_t               gaiFreeMemoryArrayAlloc[MALLOCBINS] = MALLOCBINSZEROS;      // amount allocated in gapFreeMemoryArray
size_t               gaiFreeMemoryArrayUsed[MALLOCBINS] = MALLOCBINSZEROS;       // amount used in gapFreeMemoryArray
CRITICAL_SECTION     gacsFreeMemory[MALLOCBINS];                                 // critical section for memory array

// lazy memory
HANDLE               ghClaimMemoryThread = NULL;            // claims memory
HANDLE               ghClaimMemoryEvent = NULL;             // set to cause thread to wake up
BOOL                 gfClaimMemoryWantToQuit = TRUE;        // if want to quit
CRITICAL_SECTION     gcsClaimMemory;                        // critical section to access list of claimed memory
PCLAIMMEMORY         gaClaimMemoryArray = NULL;             // array of memory claimed
size_t               giClaimMemoryAlloc = 0;                // memory allocated in gaClaimMemoryArray
size_t               giClaimMemoryUsed = 0;                 // memory used (in bytes) in gaClaimMemoryArray
size_t               giClaimMemoryChunkSize = 0;            // chunk size to use
#define              CLAIMMEMORYMIN       (MAXRAYTHREAD)    // minimum number of memory chunks to keep around
#define              CLAIMMEMORYMAX       (CLAIMMEMORYMIN*4)   // maximum number of memory chunks to keep around
#define              CLAIMMEMORYSIZEMAX    (VIRTUALBLOCKSIZE * 16) // won't allow for too large of memory in claim memory



/************************************************************************
MyVirtualAlloc - VirtualAlloc that gets memory from claim memory thread proc

inputs
      size_t          iSize - Size
returns
   PVOID - memory
*/
PVOID MyVirtualAlloc (size_t iSize)
{
   EnterCriticalSection (&gcsClaimMemory);

   if (gfClaimMemoryWantToQuit)
      goto allocself;

   // remember the minimum size
   if (!giClaimMemoryChunkSize || (iSize < giClaimMemoryChunkSize)) {
      giClaimMemoryChunkSize = iSize;

      // set the event just in case will need to alloc more memory
      SetEvent (ghClaimMemoryEvent);
   }

   // if there isn't a thread then try to get
   if (!ghClaimMemoryThread)
      goto allocself;

   // only get from memory if correct size
   DWORD i;
   DWORD dwNum = (DWORD)(giClaimMemoryUsed / sizeof(CLAIMMEMORY));
   if (iSize == giClaimMemoryChunkSize)
      for (i = 0; i < dwNum; i++)
         if (gaClaimMemoryArray[i].dwSize == iSize) {
            // NOTE: Will claim the oldest one

            // found a memory match, so take it
            PVOID pRet = gaClaimMemoryArray[i].pMem;
            size_t iRetSize = gaClaimMemoryArray[i].dwSize;

            memmove (gaClaimMemoryArray + i, gaClaimMemoryArray + (i+1), (dwNum - i - 1) * sizeof(CLAIMMEMORY));
            giClaimMemoryUsed -= sizeof(CLAIMMEMORY);
            LeaveCriticalSection (&gcsClaimMemory);

#ifdef USESLOWMEMORYCHECKS
            // make sure that wipes
            size_t iCur;
            for (iCur = 0; iCur < iRetSize; iCur++)
               _ASSERTE (((PBYTE)pRet)[iCur] == OVERRUNBYTE);
#endif


            // set the event just in case will need to alloc more memory
            SetEvent (ghClaimMemoryEvent);

            return pRet;
         }

allocself:
   LeaveCriticalSection (&gcsClaimMemory);

#if 0 // def _DEBUG
   if (iSize != giClaimMemoryChunkSize) {
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nMyVirtualAlloc = %d (def = %d)", (int) iSize, (int)giClaimMemoryChunkSize);
      OutputDebugStringW (szTemp);
   }
#endif

   // if get to here, then alloc self
   PVOID pRet = VirtualAlloc (NULL, iSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
   if (!pRet)
      return NULL;

#ifdef USESLOWMEMORYCHECKS
   // wipe it out
   memset (pRet, OVERRUNBYTE, iSize);
#endif

   return pRet;
}



/************************************************************************
MyVirtualFree - Usually adds the memory to the queue, but often does a virtual
free call

inputs
   PVOID          pMem - Memory to free
   size_t         iSize - Memory that was allocated
returns
   BOOL - TRUE if freed
*/
BOOL MyVirtualFree (PVOID pMem, size_t iSize)
{
   EnterCriticalSection (&gcsClaimMemory);

   if (gfClaimMemoryWantToQuit)
      goto freeself;

   // don't check for odd-sized chunks, just add them and let the thread free
   // if (iSize != giClaimMemoryChunkSize)
   //   goto freeself;

   // if there's no thread then free self
   if (!ghClaimMemoryThread)
      goto freeself;

   // make sure the array is large enough
   size_t dwNeed = giClaimMemoryUsed + sizeof(CLAIMMEMORY);
   if (dwNeed > giClaimMemoryAlloc) {
      PCLAIMMEMORY pNew = gaClaimMemoryArray ? (PCLAIMMEMORY)realloc(gaClaimMemoryArray, dwNeed) : (PCLAIMMEMORY)malloc(dwNeed);
      if (!pNew)
         goto freeself; // error occurred

      gaClaimMemoryArray = pNew;
      giClaimMemoryAlloc = dwNeed;
   }

   // fill it in
   PCLAIMMEMORY pClaim = (PCLAIMMEMORY)((PBYTE)gaClaimMemoryArray + giClaimMemoryUsed);
   pClaim->pMem = pMem;
   pClaim->dwSize = iSize;
   giClaimMemoryUsed += sizeof(CLAIMMEMORY);

#ifdef USESLOWMEMORYCHECKS
   // completely wipe the memory
   memset (pClaim->pMem, OVERRUNBYTE, pClaim->dwSize);
#endif

   LeaveCriticalSection (&gcsClaimMemory);

   // set the event just in case will need to alloc more memory
   SetEvent (ghClaimMemoryEvent);

   return TRUE;

freeself:
   LeaveCriticalSection (&gcsClaimMemory);

   // if get to here, then free self
   return VirtualFree (pMem, 0, MEM_RELEASE);
}


/************************************************************************
ClaimMemoryThreadProc - Thread proc that frees up the memory
*/
DWORD WINAPI ClaimMemoryThreadProc(LPVOID lpParameter)
{
   BOOL fRet;
   DWORD i;

   while (TRUE) {
      // see if there's anything that should do
      EnterCriticalSection (&gcsClaimMemory);

      // see if want to shut down
      if (gfClaimMemoryWantToQuit && !giClaimMemoryUsed ) {
         // done
         LeaveCriticalSection (&gcsClaimMemory);
         break;
      }

      // min and max, take into account if optimizing memory, then don't claim
      DWORD dwMin = gfOptimizeMemory ? 0 : CLAIMMEMORYMIN;
      DWORD dwMax = CLAIMMEMORYMAX;

      // how many are used
      DWORD dwUsed = (DWORD)(giClaimMemoryUsed / sizeof(CLAIMMEMORY));

      // if there are any odd-sized chunks then free them right away
      for (i = 0; i < dwUsed; i++)
         if (gaClaimMemoryArray[i].dwSize > CLAIMMEMORYSIZEMAX)
            break;
      if (i < dwUsed) {
         PVOID pToFree = gaClaimMemoryArray[i].pMem;
         size_t iSizeFree = gaClaimMemoryArray[i].dwSize;

         memmove (gaClaimMemoryArray + i, gaClaimMemoryArray + (i+1), (dwUsed - i - 1) * sizeof(CLAIMMEMORY));
         giClaimMemoryUsed -= sizeof(CLAIMMEMORY);
         LeaveCriticalSection (&gcsClaimMemory);

#ifdef USESLOWMEMORYCHECKS
         // make sure that wipes
         size_t iCur;
         for (iCur = 0; iCur < iSizeFree; iCur++)
            _ASSERTE (((PBYTE)pToFree)[iCur] == OVERRUNBYTE);
#endif

         // memory free occurs outside the critical section
         fRet = VirtualFree (pToFree, 0, MEM_RELEASE);
         _ASSERTE (fRet);

         // done
         continue;
      } // odd-sized

      // if there aren't enough then alloc
      if ((dwUsed < dwMin) && giClaimMemoryChunkSize && !gfClaimMemoryWantToQuit) {
         size_t iSize = giClaimMemoryChunkSize;
         LeaveCriticalSection (&gcsClaimMemory);

         // left critical section while doing virtual alloc
         PVOID pAlloc = VirtualAlloc (NULL, iSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);;

         EnterCriticalSection (&gcsClaimMemory);
         if (!pAlloc)
            goto wait;  // shouldnt have happened

#ifdef USESLOWMEMORYCHECKS
         // completely wipe this
         memset (pAlloc, OVERRUNBYTE, iSize);
#endif

         // allocate one
         size_t dwNeed = giClaimMemoryUsed + sizeof(CLAIMMEMORY);
         if (dwNeed > giClaimMemoryAlloc) {
            PCLAIMMEMORY pNew = gaClaimMemoryArray ? (PCLAIMMEMORY)realloc(gaClaimMemoryArray, dwNeed) : (PCLAIMMEMORY)malloc(dwNeed);
            if (!pNew) {
               fRet = VirtualFree (pAlloc, 0, MEM_RELEASE);
               _ASSERTE (fRet);
               goto wait;  // error, shouldnt happen
            }

            gaClaimMemoryArray = pNew;
            giClaimMemoryAlloc = dwNeed;
         }

         // fill it in
         PCLAIMMEMORY pClaim = (PCLAIMMEMORY)((PBYTE)gaClaimMemoryArray + giClaimMemoryUsed);
         pClaim->pMem = pAlloc;
         pClaim->dwSize = iSize;
         giClaimMemoryUsed += sizeof(CLAIMMEMORY);
         
         // repeat
         LeaveCriticalSection (&gcsClaimMemory);
         continue;
      }

      // if too many then free the oldest one
      if (dwUsed && ((dwUsed >= dwMax) || gfClaimMemoryWantToQuit)) {
         // always free the last one
         i = 0;

         PVOID pToFree = gaClaimMemoryArray[i].pMem;

         memmove (gaClaimMemoryArray + i, gaClaimMemoryArray + (i+1), (dwUsed - i - 1) * sizeof(CLAIMMEMORY));
         giClaimMemoryUsed -= sizeof(CLAIMMEMORY);
         LeaveCriticalSection (&gcsClaimMemory);

         // memory free occurs outside the critical section
         fRet = VirtualFree (pToFree, 0, MEM_RELEASE);
         _ASSERTE (fRet);

         // done
         continue;
      }

      // else, just wait
wait:
      LeaveCriticalSection (&gcsClaimMemory);
      WaitForSingleObject (ghClaimMemoryEvent, INFINITE);
   } // while TRUE

   return 0;
}


/************************************************************************
FreeMemoryThreadProc - Thread proc that frees up the memory
*/
DWORD WINAPI FreeMemoryThreadProc(LPVOID lpParameter)
{
   // remember the bin
   DWORD dwMallocBin = (DWORD)(QWORD)lpParameter;

   while (TRUE) {
      // see if there's anything that should do
      EnterCriticalSection (&gacsFreeMemory[dwMallocBin]);

      // see if want to shut down
      if (gfFreeMemoryWantToQuit && !gaiFreeMemoryArrayUsed[dwMallocBin] ) {
         // done
         LeaveCriticalSection (&gacsFreeMemory[dwMallocBin]);
         break;
      }

      // see if any files to load
      if (!gaiFreeMemoryArrayUsed[dwMallocBin]) {
         _ASSERTE (!gaiFreeMemoryQueue[dwMallocBin]);
         LeaveCriticalSection (&gacsFreeMemory[dwMallocBin]);
         WaitForSingleObject (gahFreeMemoryEvent[dwMallocBin], INFINITE);
         continue;
      }

      // free the last bit of memory
      gaiFreeMemoryArrayUsed[dwMallocBin] -= sizeof(PVOID);
      PVOID *ppFree = (PVOID*) ((PBYTE) gapFreeMemoryArray[dwMallocBin] + gaiFreeMemoryArrayUsed[dwMallocBin]);
      PVOID pFree = *ppFree;
#ifdef _DEBUG
      _ASSERTE (pFree);
      *ppFree = NULL;
#endif
      gaiFreeMemoryQueue[dwMallocBin] -= InternalMemorySize (pFree, FALSE);
      int iNewPri = (gaiFreeMemoryQueue[dwMallocBin] >= giFreeMemoryQueueHigh) ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_BELOW_NORMAL;

      // get out of the critical section as quickly as posible
      LeaveCriticalSection (&gacsFreeMemory[dwMallocBin]);

      // free this
      EscFreeNoCache (pFree);

      // set the thread priority
      if (iNewPri != gaiFreeMemoryPriority[dwMallocBin]) {
         gaiFreeMemoryPriority[dwMallocBin] = iNewPri;
         SetThreadPriority (gahFreeMemoryThread[dwMallocBin], gaiFreeMemoryPriority[dwMallocBin]);
      }

   } // while TRUE

   return 0;
}



/************************************************************************
EscFree - Frees memory allocated by EscMalloc(), BUT caches it

inputs
   PVOID          pMem - Memory
returns
   None
*/
void EscFree (PVOID pMem)
{
#ifdef _DEBUG  // check memory
   // check the integrity
   InternalCheckMemoryIntegrity (pMem, FALSE);
#endif

#ifdef USESLOWMEMORYCHECKS
   // zero out memory
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;
   memset (pMem, FREEMEMBYTE, psbh->dwAllocated);
#endif

#ifdef _DEBUG // so report errors sooner
#ifndef USESLOWMEMORYCHECKS
   // keep enabled to try and find overwrite: 
   EscFreeNoCache (pMem);
   return;
#endif
#endif

   DWORD dwMallocBin = InternalMemoryBin (pMem);
   if (!gahFreeMemoryThread[dwMallocBin]) {
      // no thread, so just free
      EscFreeNoCache (pMem);
      return;
   }

   // add this to the thread
   EnterCriticalSection (&gacsFreeMemory[dwMallocBin]);

   // make sure enough memory
   size_t iNeed = gaiFreeMemoryArrayUsed[dwMallocBin] + sizeof(PVOID);
   if (!gapFreeMemoryArray[dwMallocBin] || (iNeed > gaiFreeMemoryArrayAlloc[dwMallocBin])) {
      size_t iNeedExtra = iNeed + sizeof(PVOID) * 100;

      PVOID pNew = gapFreeMemoryArray[dwMallocBin] ? realloc(gapFreeMemoryArray[dwMallocBin], iNeedExtra) : malloc(iNeedExtra);
      
      // if failed, free right now
      if (!pNew) {
         EscFreeNoCache (pMem);
         return;
      }

      // else, succeded
      gapFreeMemoryArray[dwMallocBin] = pNew;
      gaiFreeMemoryArrayAlloc[dwMallocBin] = iNeedExtra;
   }

   // write
   PVOID *ppFree = (PVOID*) ((PBYTE) gapFreeMemoryArray[dwMallocBin] + gaiFreeMemoryArrayUsed[dwMallocBin]);
   *ppFree = pMem;
   gaiFreeMemoryArrayUsed[dwMallocBin] = iNeed;
   gaiFreeMemoryQueue[dwMallocBin] += InternalMemorySize (pMem, FALSE);
   int iNewPri = (gaiFreeMemoryQueue[dwMallocBin] >= giFreeMemoryQueueHigh) ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_BELOW_NORMAL;

   LeaveCriticalSection (&gacsFreeMemory[dwMallocBin]);

   // set the thread priority
   if (iNewPri != gaiFreeMemoryPriority[dwMallocBin]) {
      gaiFreeMemoryPriority[dwMallocBin] = iNewPri;
      SetThreadPriority (gahFreeMemoryThread[dwMallocBin], gaiFreeMemoryPriority[dwMallocBin]);
   }

   // set the event so will process
   SetEvent (gahFreeMemoryEvent[dwMallocBin]);
}

/************************************************************************
MallocBinChoose - Choose a malloc bin appropriate to the thread.

returns
   DWORD - Malloc bin
*/
DWORD MallocBinChoose (void)
{
   // if win32 then always use bin 0
   if (sizeof(PVOID) <= sizeof(DWORD))
      return 0;

   // find best match
   DWORD dwID = GetCurrentThreadId();

   // find a match
   EnterCriticalSection (&gcsMallocBinChoose);

   giBinTimeStamp++;

   // see if can find a match for this
   DWORD dwMallocBin, i, j;
   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      for (i = 0; i < THREADSPERBIN; i++) {
         if (gadwBinThreadID[dwMallocBin][i] == dwID) {
            if (gafInMemoryCritSec[dwMallocBin]) {
               // found where the thread was used, but this is busy, so pick a new one
               gadwBinThreadID[dwMallocBin][i] = 0;
               gaiBinLastUsed[dwMallocBin][i] = 0; // to clear

#ifdef _DEBUG
               giMallocBinCollisions++;
#endif
               goto newbin;
            }

            // else, found the bin that was last used, and it's not busy
            // now, so keep on using it
            goto usebin;
         }
      } // i
   } // dwMallocBin

newbin:
   // if get here, then couldn't find
   dwMallocBin = 0;
   for (i = 1; i < MALLOCBINS; i++)
      if (gaiBinLastUsedMax[i] < gaiBinLastUsedMax[dwMallocBin])
         dwMallocBin = i;  // least used
   i = 0;
   for (j = 1; j < THREADSPERBIN; j++)
      if (gaiBinLastUsed[dwMallocBin][j] < gaiBinLastUsed[dwMallocBin][i])
         i = j;
   gadwBinThreadID[dwMallocBin][i] = dwID;

usebin:  // dwMallocBin is valid, i has index
   gaiBinLastUsedMax[dwMallocBin] = giBinTimeStamp; // remember the time stamp
   gaiBinLastUsed[dwMallocBin][i] = giBinTimeStamp;   // remember the time stamp

   LeaveCriticalSection (&gcsMallocBinChoose);

   return dwMallocBin;
}

/************************************************************************
MaxAllocatable - Returns the maximum usable-memory that can be gotten from
a block of a given size. Generally, dont call this directly, since the
values will be cached in adwMaxAllocateable.

inputs
   DWORD             dwLevel - Block level
   DWORD             dwSubBlocks - Number of divisions, from 1 to MAXSUBBLOCKS (inclusive)
return
   size_t - Maximum size
*/
size_t MaxAllocatable (DWORD dwLevel, DWORD dwSubBlocks)
{
   size_t      iPrev;


   if (!dwLevel)
      return 0xffffffff;   // NOTE: May wish to make this larger for 64-bit windows
   else if (dwLevel == 1) {
      iPrev = VIRTUALBLOCKSIZE - (sizeof(BLOCKHDR) + sizeof(SUBBLOCKHDR));
#ifdef _DEBUG
      iPrev -= sizeof(SUBBLOCKTAIL);
#endif
   }
   else
      iPrev = MaxAllocatable (dwLevel-1, MAXSUBBLOCKS);

   iPrev -= (sizeof(BLOCKHDR) + dwSubBlocks * sizeof(SUBBLOCKHDR));
#ifdef _DEBUG
   iPrev -= dwSubBlocks * sizeof(SUBBLOCKTAIL);
#endif

   iPrev /= dwSubBlocks;
   iPrev -= (iPrev % sizeof(DWORD));   // align

   return iPrev;
}

/************************************************************************
FindFirstPBLOCKHDR - Returns a pointer to a PBLOCKHDR pointer thats in the
global for the first blockheader.

inputs
   DWORD          dwMallocBin - Malloc bin to use
   DWORD          dwLevel - Level to use, 0..MALLOCLEVELS
   DWORD          dwSubBlocks - Number of divisions. (Used if dwLevel < MAXSUBBLOCKS)
   size_t         dwSize - Size of each division. (Used if dwLevel == MALLOCLEVELS)
returns
   PBLOCKHDR * - Where can write to to change back
*/
__inline PBLOCKHDR * FindFirstBLOCKHDR (DWORD dwMallocBin, DWORD dwLevel, DWORD dwSubBlocks, size_t dwSize)
{
   _ASSERTE (!gaPBLOCKHDRTiny[dwMallocBin][0]);
   _ASSERTE (dwLevel <= MALLOCLEVELS);
   if (dwLevel < MALLOCLEVELS) {
      _ASSERTE ((dwSubBlocks <=MAXSUBBLOCKS) && (dwSubBlocks >= 1));
      return &gaPBLOCKHDR[dwMallocBin][dwLevel][dwSubBlocks-1];
   }
   
   // else, smaller memory
   _ASSERTE (dwSize < MEMSMALLSIZE);
   if (dwSize < MEMTINYSIZE) {
      _ASSERTE (!(dwSize % MEMTINYSIZEQUANT));
      _ASSERTE (dwSize);
      _ASSERTE (dwSize / MEMTINYSIZEQUANT);
      return &gaPBLOCKHDRTiny[dwMallocBin][dwSize / MEMTINYSIZEQUANT];
   }
   else {
      _ASSERTE (!(dwSize % MEMSMALLSIZEQUANT));
      return &gaPBLOCKHDRSmall[dwMallocBin][dwSize / MEMSMALLSIZEQUANT];
   }
}

/************************************************************************
BlockAlloc - Allocates a new block and puts it at the head of the
gaPBLOCKHDR list.

inputs
   DWORD          dwMallocBin - Malloc bin to use
   DWORD          dwLevel - Level to use, 0..MALLOCLEVELS
   DWORD          dwSubBlocks - Number of divisions, from 1 to MAXSUBBLOCKS (inclusive)
                        (Used if dwLevel < MALLOCLEVELS)
   size_t         dwSize - Size of the block (only used if dwLevel > 0).
                        If dwLevel == MALLOCLEVELS then this is the size of an individual
                        element to appear in the block.
returns
   PBLOCKHDR - New block. Pointer to PBLOCKHDR
*/
PBLOCKHDR BlockAlloc (DWORD dwMallocBin, DWORD dwLevel, DWORD dwSubBlocks, size_t dwSize)
{
   PBLOCKHDR pNew;
   size_t dwOrigSize = dwSize;

   if (!dwLevel) {
      // determine the size to allocate, which is rounded up
      dwSize += sizeof(BLOCKHDR) + dwSubBlocks * sizeof(SUBBLOCKHDR);
#ifdef _DEBUG
      dwSize += dwSubBlocks * sizeof(SUBBLOCKTAIL);
#endif
      dwSize = ((dwSize + VIRTUALBLOCKSIZE - 1) / VIRTUALBLOCKSIZE) * VIRTUALBLOCKSIZE;
      // PVOID pStart = NULL;

      pNew = (PBLOCKHDR) MyVirtualAlloc (dwSize);

      if (pNew) {
         gaiAllocated[dwMallocBin] += dwOrigSize;
#ifdef _DEBUG
         gaiMaxAllocated[dwMallocBin] = max(gaiMaxAllocated[dwMallocBin], gaiAllocated[dwMallocBin]);
#endif
      }
   }
   else if (dwLevel == MALLOCLEVELS) {
      _ASSERTE((dwSize < MEMTINYSIZE) ? !(dwSize % MEMTINYSIZEQUANT) : !(dwSize % MEMSMALLSIZEQUANT));

      size_t dwSubSize = dwSize;
      dwSize = (dwSubSize < MEMTINYSIZE) ? MEMTINYSIZEBLOCK : MEMSMALLSIZEBLOCK;
      pNew = (PBLOCKHDR) InternalMalloc (dwMallocBin, dwSize
#ifdef _DEBUG
         , (DWORD)-1 ,__FILE__, __FUNCTION__, __LINE__
#endif
         );
      if (!pNew)
         return NULL;

      // figure out howlarge can make this
      dwSize = InternalMemorySize (pNew, TRUE);
      pNew = (PBLOCKHDR) InternalRealloc (dwMallocBin, pNew, dwSize);

      // calculate how many sub-blocks should keep in order to be large enough
      // for allocating required blocks
      dwSubSize += sizeof(SUBBLOCKHDR);
#ifdef _DEBUG
      dwSubSize += sizeof(SUBBLOCKTAIL);
#endif
      dwSubBlocks = (DWORD)((dwSize - sizeof(BLOCKHDR)) / dwSubSize);
   }
   else {   // 1..MALLOCLEVELS-1
      // allocate from the next level above
      dwSize = gadwMaxAllocatable[dwMallocBin][dwLevel][0] + sizeof(BLOCKHDR) + sizeof(SUBBLOCKHDR);
#ifdef _DEBUG
      dwSize += sizeof(SUBBLOCKTAIL);
#endif

      pNew = (PBLOCKHDR) InternalMalloc (dwMallocBin, dwLevel-1, (dwLevel <= 1) ? 1 : MAXSUBBLOCKS, dwSize
#ifdef _DEBUG
         ,(DWORD)-1, __FILE__, __FUNCTION__, __LINE__
#endif
         );
         // This should fit perfectly in a sub-block division of MAXSUBBLOCKS,
         // except for the last malloc, where only do one sub-block
   }
   if (!pNew)
      return NULL;

#ifdef _DEBUG
   // zero out all memory
   memset (pNew, OVERRUNBYTE, dwSize);
#endif

   pNew->dwType = dwLevel;
   pNew->dwOrigSize = dwOrigSize;
   pNew->dwTotalSize = dwSize;
   pNew->dwSubBlockSizeWithHeader = (dwSize - sizeof(BLOCKHDR)) / dwSubBlocks;
   pNew->dwSubBlockSizeWithHeader -= (pNew->dwSubBlockSizeWithHeader % sizeof(DWORD));
   pNew->dwSubBlockSizeNoHeader = pNew->dwSubBlockSizeWithHeader - sizeof(SUBBLOCKHDR);
#ifdef _DEBUG
   pNew->dwSubBlockSizeNoHeader -= sizeof(SUBBLOCKTAIL);
   pNew->dwUniqueID = UNIQUEBLOCKID;
#endif
   _ASSERTE (pNew->dwSubBlockSizeNoHeader < 1000000000); // to make sure not wrong size
   pNew->pNext = pNew->pPrev = NULL;
   pNew->dwLastFree = 0;
   pNew->dwMax = dwSubBlocks;
   pNew->dwFree = pNew->dwMax;
   
   // fill in all the headers
   DWORD i;
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)(pNew+1);
   for (i = 0; i < pNew->dwMax; i++, psbh = (PSUBBLOCKHDR)((PBYTE)psbh + pNew->dwSubBlockSizeWithHeader)) {
      psbh->dwBackToHdr = 0; // BUGFIX - Was (DWORD)((PBYTE)psbh - (PBYTE)pNew);
      psbh->dwAllocated = 0;
      psbh->dwMallocBin = dwMallocBin;

#ifdef _DEBUG
      psbh->dwUniqueID = UNIQUESUBBLOCKID;
#endif
   } // i

   // add this to the list
   PBLOCKHDR *pUse = FindFirstBLOCKHDR(dwMallocBin, dwLevel, dwSubBlocks, dwOrigSize);
   PBLOCKHDR pNext = *pUse;
   *pUse = pNew;
   pNew->pNext = pNext;
   if (pNext)
      pNext->pPrev = pNew;

   return pNew;
}

/************************************************************************
BlockFree - Frees a block.

inputs
   DWORD          dwMallocBin - Malloc bin
   PBLOCKHDR      pBlockHdr - Header info
returns
   BOOL - TRUE if success
*/
BOOL BlockFree (DWORD dwMallocBin, PBLOCKHDR pBlockHdr)
{
   // make sure a valid block
   _ASSERT (pBlockHdr->dwUniqueID == UNIQUEBLOCKID);

   // remove this from the list
   if (pBlockHdr->pPrev)
      ((PBLOCKHDR)pBlockHdr->pPrev)->pNext = pBlockHdr->pNext;
   else
      *(FindFirstBLOCKHDR(dwMallocBin, pBlockHdr->dwType, pBlockHdr->dwMax, pBlockHdr->dwOrigSize)) = (PBLOCKHDR) pBlockHdr->pNext;

   if (pBlockHdr->pNext)
      ((PBLOCKHDR)pBlockHdr->pNext)->pPrev = pBlockHdr->pPrev;

   if (!pBlockHdr->dwType) {
      // this is a virtual free
      gaiAllocated[dwMallocBin] -= pBlockHdr->dwOrigSize;

      BOOL fRet = MyVirtualFree (pBlockHdr, pBlockHdr->dwTotalSize);
      _ASSERTE (fRet);
      return fRet;
   }
   else
      return InternalFree (dwMallocBin, pBlockHdr, FALSE);
}

/************************************************************************
BlockSlideLeft - Slides the current block left.

inputs
   DWORD             dwMallocBin - Malloc bin
   PBLOCKHDR         pbh - Current block
returns
   BOOL - TRUE if success
*/
BOOL BlockSlideLeft (DWORD dwMallocBin, PBLOCKHDR pbh)
{
   if (!pbh->pPrev)
      return FALSE;  // cant slide left

   // else, move up
   PBLOCKHDR pPrev = (PBLOCKHDR)pbh->pPrev;
   PBLOCKHDR pPrevPrev = (PBLOCKHDR)pPrev->pPrev;
   PBLOCKHDR pNext = (PBLOCKHDR)pbh->pNext;
   if (pPrevPrev)
      pPrevPrev->pNext = pbh;
   else
      *(FindFirstBLOCKHDR(dwMallocBin, pbh->dwType, pbh->dwMax, pbh->dwOrigSize)) = pbh;
      //gaPBLOCKHDR[pbh->dwType][pbh->dwMax-1] = pbh;
   pbh->pPrev = pPrevPrev;
   pbh->pNext = pPrev;
   pPrev->pPrev = pbh;
   pPrev->pNext = pNext;
   if (pNext)
      pNext->pPrev = pPrev;


   return TRUE;
}


/************************************************************************
BlockSlideRight - Slides the current block right.

inputs
   DWORD             dwMallocBin - Malloc bin
   PBLOCKHDR         pbh - Current block
returns
   BOOL - TRUE if success
*/
BOOL BlockSlideRight (DWORD dwMallocBin, PBLOCKHDR pbh)
{
   if (!pbh->pNext)
      return FALSE;  // cant slide left

   // else, move right
   PBLOCKHDR pNext = (PBLOCKHDR)pbh->pNext;
   PBLOCKHDR pNextNext = (PBLOCKHDR)pNext->pNext;
   PBLOCKHDR pPrev = (PBLOCKHDR)pbh->pPrev;
   if (pPrev)
      pPrev->pNext = pNext;
   else
      *(FindFirstBLOCKHDR(dwMallocBin, pbh->dwType, pbh->dwMax, pbh->dwOrigSize)) = pNext;
      // gaPBLOCKHDR[pbh->dwType][pbh->dwMax-1] = pNext;
   pNext->pPrev = pPrev;
   pNext->pNext = pbh;
   pbh->pPrev = pNext;
   pbh->pNext = pNextNext;
   if (pNextNext)
      pNextNext->pPrev = pbh;

   return TRUE;
}


#ifdef _DEBUG
/************************************************************************
BlockSlideCheck - Check to make sure sorted properly.

inputs
   PBLOCKHDR         pbh - Current block
*/
void BlockSlideCheck (PBLOCKHDR pbh)
{
   PBLOCKHDR pPrev = (PBLOCKHDR)pbh->pPrev;
   PBLOCKHDR pPrevPrev = pPrev ? (PBLOCKHDR)pPrev->pPrev : NULL;
   PBLOCKHDR pNext = (PBLOCKHDR)pbh->pNext;
   PBLOCKHDR pNextNext = pNext ? (PBLOCKHDR)pNext->pNext : NULL;

   DWORD adw[5];
   adw[0] = pPrevPrev ? (pPrevPrev->dwFree ? pPrevPrev->dwFree : (DWORD)-1) : 0;
   adw[1] = pPrev ? (pPrev->dwFree ? pPrev->dwFree : (DWORD)-1) : 0;
   adw[2] = pbh->dwFree ? pbh->dwFree : (DWORD)-1;
   adw[3] = pNext ? (pNext->dwFree ? pNext->dwFree : (DWORD)-1) : (DWORD)-1;
   adw[4] = pNextNext ? (pNextNext->dwFree ? pNextNext->dwFree : (DWORD)-1) : (DWORD)-1;

   _ASSERTE ((adw[0] <= adw[1]) && (adw[1] <= adw[2]) && (adw[2] <= adw[3]) && (adw[3] <= adw[4]));
}
#endif // _DEBUG

/************************************************************************
BlockSlideSort- Slides the blocks left or right until its in proper sort order.

inputs
   DWORD             dwMallocBin - Malloc bin
   PBLOCKHDR         pbh - Current block
returns
   none
*/
void BlockSlideSort (DWORD dwMallocBin, PBLOCKHDR pbh)
{
   // want the blocks at the beginngin sorted so that the fewest number of
   // free blocks are first... that way when do malloc, will malloc into the
   // fewest number of free blocks.
   // move this up the chain until it's at the top or the left has at least one freed
   // NOTE: Only slide left if this has anything free
   if (pbh->dwFree)
      while (pbh->pPrev) {
         PBLOCKHDR pPrev = (PBLOCKHDR)pbh->pPrev;
         if (pPrev->dwFree && (pPrev->dwFree <= pbh->dwFree))
            break;   // the left has one that's freed

         BlockSlideLeft (dwMallocBin, pbh);
      } // while have previous

   // move right to proper position
   while (pbh->pNext) {
      PBLOCKHDR pNext = (PBLOCKHDR)pbh->pNext;
      if (!pNext->dwFree || ((pNext->dwFree >= pbh->dwFree) && pbh->dwFree) )
         break;   // the left has one that's freed

      BlockSlideRight (dwMallocBin, pbh);
   } // while have previous

#ifdef _DEBUG
   BlockSlideCheck (pbh);
#endif
}

#ifdef _DEBUG

/************************************************************************
InternalCheckSubBlockIntegrity - Checks the integrity of a sub-block.
If fails, this asserts.

inputs
   PSUBBLOCKHDR psbh - Sub-block
   BOOL        fRequiredBackToHdr - If TRUE then requires backtohdr NOT be -1.
               If FALSE, can be
returns
   none
*/
void InternalCheckSubBlockIntegrity (PSUBBLOCKHDR psbh, BOOL fRequiredBackToHdr)
{
   _ASSERTE (psbh->dwUniqueID == UNIQUESUBBLOCKID);

   if (fRequiredBackToHdr)
      _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);

   DWORD i;
   if (psbh->dwBackToHdr != (DWORD)-1) {
      // use this to find the header
      PBLOCKHDR pbh = (PBLOCKHDR) ((PBYTE)psbh - psbh->dwBackToHdr);

      // make sure it's the right stuff
      _ASSERTE (pbh->dwUniqueID == UNIQUEBLOCKID);

      PSUBBLOCKTAIL psbt = (PSUBBLOCKTAIL) ((PBYTE)(psbh+1) + pbh->dwSubBlockSizeNoHeader);

      for (i = 0; i < sizeof(psbt->abOverrun); i++)
         _ASSERTE (psbt->abOverrun[i] == OVERRUNBYTE);
      PBYTE pb = (PBYTE)(psbh+1);
      for (i = psbh->dwAllocated; i < pbh->dwSubBlockSizeNoHeader; i++)
         _ASSERTE (pb[i] == OVERRUNBYTE);
   }

   for (i = 0; i < sizeof(psbh->abOverrun); i++)
      _ASSERTE (psbh->abOverrun[i] == OVERRUNBYTE);
}



/************************************************************************
InternalCheckBlockIntegrity - Checks the integrity of a block.
If fails, this asserts.

inputs
   PBLOCKHDR psbh - Block
returns
   none
*/
void InternalCheckBlockIntegrity (PBLOCKHDR pbh)
{
   // make sure it's the right stuff
   _ASSERTE (pbh->dwUniqueID == UNIQUEBLOCKID);

   DWORD i;
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR) (pbh+1);
   for (i = 0; i < pbh->dwMax; i++, psbh = (PSUBBLOCKHDR)((PBYTE)psbh + pbh->dwSubBlockSizeWithHeader)) {
      _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);
      if (psbh->dwBackToHdr)  // BUGFIX - Was checking for dwAllocated not 0
         InternalCheckSubBlockIntegrity (psbh, TRUE);
   }
}


/************************************************************************
InternalCheckSystemIntegrity - Lazily loops over blocks and checks for integrtiy.
If fails, this asserts.

inputs
   DWORD       dwMallocBin - Malloc bin
returns
   none
*/
void InternalCheckSystemIntegrity (DWORD dwMallocBin)
{
   return;  // BUGFIX - Don't do this because too slow, but can put back in when need

   static DWORD dwLevel = 0;
   static DWORD dwSubBlock = 0;
   static DWORD dwIndex = 0;

newlevel:
   // loop level
   if (dwLevel > MALLOCLEVELS+1) {
      dwLevel = 0;
      dwSubBlock = 0;
      dwIndex = 0;
   }

   // maximmu sub-blocks
   DWORD dwMax = MAXSUBBLOCKS;
   if (dwLevel == MALLOCLEVELS)
      dwMax = MEMTINYSIZE / MEMTINYSIZEQUANT;
   else if (dwLevel == MALLOCLEVELS+1)
      dwMax = MEMSMALLSIZE / MEMSMALLSIZEQUANT;
   if (dwSubBlock >= dwMax) {
      dwSubBlock = 0;
      dwIndex = 0;
      dwLevel++;
      goto newlevel;
   }

   // try to find the given index
   PBLOCKHDR pCur;
   if (dwLevel == MALLOCLEVELS)
      pCur = gaPBLOCKHDRTiny[dwMallocBin][dwSubBlock];
   else if (dwLevel == MALLOCLEVELS+1)
      pCur = gaPBLOCKHDRSmall[dwMallocBin][dwSubBlock];
   else
      pCur = gaPBLOCKHDR[dwMallocBin][dwLevel][dwSubBlock];

   // count down
   DWORD i;
   for (i = 0; pCur && (i < dwIndex); pCur = (PBLOCKHDR)pCur->pNext, i++);
   if (!pCur) {
      // overran
      dwIndex = 0;
      dwSubBlock++;
      return;  // since dont want to get into infinite loop
   }

   // else, found it
   InternalCheckBlockIntegrity (pCur);

   if (pCur->pNext)
      dwIndex++;
   else {
      dwIndex = 0;
      dwSubBlock++;
   }
}

/************************************************************************
InternalCheckMemoryIntegrity - Checks the integrity of allocated memory.
If fails, this asserts.

inputs
   PVOID       pMem - Memory
   BOOL        fRequiredBackToHdr - If TRUE then requires backtohdr NOT be -1.
               If FALSE, can be
returns
   none
*/
void InternalCheckMemoryIntegrity (PVOID pMem, BOOL fRequiredBackToHdr)
{
   if (!pMem)
      return;

   InternalCheckSubBlockIntegrity ((PSUBBLOCKHDR)pMem - 1, fRequiredBackToHdr);
}

#endif // _DEBUG

/************************************************************************
InternalFree - Internal function that frees memory.

inputs
   DWORD          dwMallocBin - Malloc bin
   PVOID          pMem - Memory
   BOOL           fIntegiry - If TRUE then do an integrity check on it
returns
   BOOL - TRUE if success
*/
BOOL InternalFree (DWORD dwMallocBin, PVOID pMem, BOOL fIntegrity)
{
   if (!pMem)
      return FALSE;

   // pull out the header
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;

   // make sure it's the right stuff
   _ASSERTE (psbh->dwUniqueID == UNIQUESUBBLOCKID);

   // integrity check
#ifdef _DEBUG
   if (fIntegrity)
      InternalCheckSubBlockIntegrity (psbh, TRUE);
#endif

   // use this to find the header
   _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);
   PBLOCKHDR pbh = (PBLOCKHDR) ((PBYTE)psbh - psbh->dwBackToHdr);

   // make sure it's the right stuff
   _ASSERTE (pbh->dwUniqueID == UNIQUEBLOCKID);

#ifdef _DEBUG
   // wipe out all the memory
   memset (psbh + 1, OVERRUNBYTE, pbh->dwSubBlockSizeNoHeader);
   BlockSlideCheck (pbh);
#endif
   psbh->dwAllocated = 0;
   psbh->dwMallocBin = (DWORD)-1;   // in case use again

   // set value indicating it's now free
   _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);
   DWORD dwIndex = (DWORD)((psbh->dwBackToHdr - sizeof(BLOCKHDR)) / pbh->dwSubBlockSizeWithHeader);
   psbh->dwBackToHdr = 0;  // BUGFIX - Added
   if (pbh->dwLastFree == (DWORD)-1)
      pbh->dwLastFree = dwIndex; // know what's free
   else
      pbh->dwLastFree = min(pbh->dwLastFree, dwIndex);   // store lowest free
   pbh->dwFree++;
   _ASSERTE (pbh->dwFree <= pbh->dwMax);

   // if just freed the last sub-block then free this
   if (pbh->dwFree >= pbh->dwMax)
      return BlockFree (dwMallocBin, pbh);

   BlockSlideSort (dwMallocBin, pbh);
   // else, this is freed
   return TRUE;
}

/************************************************************************
InternalMalloc - Internally allocate memory in a block given a level.

inputs
   DWORD          dwMallocBin - Malloc bin
   DWORD          dwLevel - Level to use, 0..MALLOCLEVELS-1, also MALLOCLEVELS
   DWORD          dwSubBlocks - Number of divisions, from 1 to MAXSUBBLOCKS (inclusive).
                     For dwLevel==MALLOCLEVELS, this is ignored
   size_t         dwSize - Number of bytes.
   DWORD          dwAllocatedBy - Allocated by information
returns
   PVOID - Memory, or NULL if error
*/
PVOID InternalMalloc (DWORD dwMallocBin, DWORD dwLevel, DWORD dwSubBlocks, size_t dwSize
#ifdef _DEBUG
                   , DWORD dwAllocatedBy, PCSTR pszFile, PCSTR pszFunction, DWORD dwLine
#endif
                      )
{
   _ASSERT (dwSubBlocks <= MAXSUBBLOCKS);
   _ASSERT (dwLevel <= MALLOCLEVELS);

   // try to find a block in the queue with a free element

   // if have a tiny or small piece of memory then round up to the next largest
   // size and claim that
   size_t dwSizeWant = dwSize;
   if ((dwSize < MEMTINYSIZE) && (((dwSize + MEMTINYSIZEQUANT-1) / MEMTINYSIZEQUANT) * MEMTINYSIZEQUANT < MEMTINYSIZE) ) {
      dwSizeWant = ((dwSize + MEMTINYSIZEQUANT-1) / MEMTINYSIZEQUANT) * MEMTINYSIZEQUANT;
      dwSizeWant = max(dwSizeWant, MEMTINYSIZEQUANT);
   }
   else if ((dwSize < MEMSMALLSIZE) && (((dwSize + MEMSMALLSIZEQUANT-1) / MEMSMALLSIZEQUANT) * MEMSMALLSIZEQUANT < MEMSMALLSIZE) )
      dwSizeWant = ((dwSize + MEMSMALLSIZEQUANT-1) / MEMSMALLSIZEQUANT) * MEMSMALLSIZEQUANT;

   PBLOCKHDR pbh = *(FindFirstBLOCKHDR(dwMallocBin, dwLevel, dwSubBlocks, dwSizeWant));
   // PBLOCKHDR pbh = gaPBLOCKHDR[dwLevel][dwSubBlocks-1];
   if (!pbh || !pbh->dwFree) {
      // none of the blocks have anything free, so create a new block
      pbh = BlockAlloc (dwMallocBin, dwLevel, dwSubBlocks, (dwLevel == MALLOCLEVELS) ? dwSizeWant : dwSize);
      if (!pbh)
         return NULL;   // error

      // NOTE: It's automatically placed as the first entry
   }

   // make sure not too large
   _ASSERTE (pbh->dwSubBlockSizeNoHeader < 1000000000);  // to make sure not too large
   _ASSERTE(dwSize <= pbh->dwSubBlockSizeNoHeader);

   // if there isn't any free index specified then try to find
   PSUBBLOCKHDR psbh;
   if (pbh->dwLastFree == (DWORD)-1) {
      psbh = (PSUBBLOCKHDR) (pbh+1);
      pbh->dwLastFree = 0;

      _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);
      while (psbh->dwBackToHdr) {   // BUGFIX - Was checking for dwAllocated !=0
         psbh = (PSUBBLOCKHDR) ((PBYTE)psbh + pbh->dwSubBlockSizeWithHeader);
         pbh->dwLastFree++;
         _ASSERT (pbh->dwLastFree < pbh->dwMax);
      }
   }
   else
      psbh = (PSUBBLOCKHDR) ((PBYTE)(pbh+1) + pbh->dwLastFree * pbh->dwSubBlockSizeWithHeader);

#ifdef _DEBUG
   psbh->dwAllocatedBy = dwAllocatedBy;
   psbh->pszFile = pszFile;
   psbh->pszFunction = pszFunction;
   psbh->dwLine = dwLine;
   BlockSlideCheck (pbh);
#endif
   psbh->dwAllocated = (DWORD)max(1, dwSize);
   psbh->dwMallocBin = dwMallocBin;

   // note it's allocated
   psbh->dwBackToHdr = (DWORD)((PBYTE)psbh - (PBYTE)pbh);   // BUGFIX - didnt set before
   pbh->dwFree--;
   if (pbh->dwFree && (pbh->dwLastFree+1 < pbh->dwMax)) {
      // see if the next one is free
      PSUBBLOCKHDR pSubNext = (PSUBBLOCKHDR) ((PBYTE)psbh + pbh->dwSubBlockSizeWithHeader);
      if (pSubNext->dwBackToHdr) // BUGFIX - Was checkign for dwallocated
         pbh->dwLastFree = (DWORD)-1;
      else
         pbh->dwLastFree++;   // for next malloc
   }
   else
      pbh->dwLastFree = (DWORD)-1;

   // put the block in its proper place
   BlockSlideSort(dwMallocBin, pbh);

   PVOID pRet = psbh+1;
#ifdef USESLOWMEMORYCHECKS
   // make sure that it's zeroed out
   DWORD i;
   for (i = 0; i < pbh->dwSubBlockSizeNoHeader; i++)
      _ASSERTE ( ((PBYTE)pRet)[i] == OVERRUNBYTE);
#endif

   // done
   return pRet;
}


/************************************************************************
InternalMallocOptimized - Intenal malloc for optimized memory.

inputs
   DWORD          dwMallocBin - malloc bin
   size_t         dwSize - Number of bytes
returns
   PVOID - Memory, or NULL if error
*/
PVOID InternalMallocOptimized (DWORD dwMallocBin, size_t dwSize
#ifdef _DEBUG
                   , DWORD dwAllocatedBy,PCSTR pszFile, PCSTR pszFunction, DWORD dwLine
#endif
                      )
{
   size_t dwAlloc = dwSize + sizeof(SUBBLOCKHDR);
   PVOID pRet = malloc (dwAlloc);
   if (pRet) {
      PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pRet;
      psbh->dwAllocated = (DWORD) dwSize;
      psbh->dwMallocBin = dwMallocBin;
      psbh->dwBackToHdr = (DWORD)-1;
#ifdef _DEBUG
      psbh->dwUniqueID = UNIQUESUBBLOCKID;
      psbh->dwAllocatedBy = dwAllocatedBy;
      psbh->pszFile = pszFile;
      psbh->pszFunction = pszFunction;
      psbh->dwLine = dwLine;
      DWORD i;
      for (i = 0; i < sizeof(psbh->abOverrun); i++)
         psbh->abOverrun[i] = OVERRUNBYTE;
#endif

      pRet = (PVOID) (psbh+1);
   }

   return pRet;
}


/************************************************************************
InternalMalloc - Internally allocate memory

inputs
   DWORD          dwMallocBin - Malloc bin
   size_t         dwSize - Number of bytes
returns
   PVOID - Memory, or NULL if error
*/
PVOID InternalMalloc (DWORD dwMallocBin, size_t dwSize
#ifdef _DEBUG
                   , DWORD dwAllocatedBy,PCSTR pszFile, PCSTR pszFunction, DWORD dwLine
#endif
                      )
{
   // if it's a small or tiny block do something different
   if ((dwSize < MEMSMALLSIZE) &&
      ((dwSize + MEMSMALLSIZEQUANT - 1) / MEMSMALLSIZEQUANT * MEMSMALLSIZEQUANT < MEMSMALLSIZE) )
      return InternalMalloc (dwMallocBin, MALLOCLEVELS, 0, dwSize
#ifdef _DEBUG
         , dwAllocatedBy ,pszFile, pszFunction, dwLine
#endif
         );

   // loop through all the max allocatables
   DWORD i, j;
   for (i = MALLOCLEVELS-1; i; i--)
      if (dwSize <= gadwMaxAllocatable[dwMallocBin][i][0])
         break;
   if (!i)
      // else, too large, so allocate at elvel to
      return InternalMalloc (dwMallocBin, 0, 1, dwSize
#ifdef _DEBUG
         , dwAllocatedBy,pszFile, pszFunction, dwLine
#endif
         );

   // else, can fit in at least one, so figure out which one
   for (j = MAXSUBBLOCKS-1; j; j--) {
      if (!afUseSubBlock[j])
         continue;   // dont want to use this scaling fo sub-blocks
      if (dwSize <= gadwMaxAllocatable[dwMallocBin][i][j])
         break;
   }
   // NOTE: Will never have the problem of j going below 0

   return InternalMalloc (dwMallocBin, i, j+1, dwSize
#ifdef _DEBUG
      , dwAllocatedBy,pszFile, pszFunction, dwLine
#endif
      );
}




/************************************************************************
InternalMemorySize - Returns the size of the memory

inputs
   PVOID          pMem - Memory being reallocated
   BOOL           fMax - If TRUE returns the maximum that can be allocated.
                  FALSE the actual allocated
returns
   size_t - Size
*/
size_t InternalMemorySize (PVOID pMem, BOOL fMax)
{
   if (!pMem)
      return NULL;

   // pull out the header
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;

   // make sure it's the right stuff
   _ASSERTE (psbh->dwUniqueID == UNIQUESUBBLOCKID);

   // Can only tell how much is allocated based on fMax
   if (!fMax)
      return psbh->dwAllocated;

   // use this to find the header
   _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);
   PBLOCKHDR pbh = (PBLOCKHDR) ((PBYTE)psbh - psbh->dwBackToHdr);

   // make sure it's the right stuff
   _ASSERTE (pbh->dwUniqueID == UNIQUEBLOCKID);

   return pbh->dwSubBlockSizeNoHeader;

}


/************************************************************************
InternalMemoryBin - Returns the memory bin used

inputs
   PVOID          pMem - Memory being reallocated
returns
   size_t - Size
*/
DWORD InternalMemoryBin (PVOID pMem)
{
   if (!pMem)
      return 0;

   // pull out the header
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;

   // make sure it's the right stuff
   _ASSERTE (psbh->dwUniqueID == UNIQUESUBBLOCKID);
   _ASSERTE (psbh->dwMallocBin < MALLOCBINS);

   return psbh->dwMallocBin;
}

/************************************************************************
InternalRealloc - Internally reallocate memory

inputs
   DWORD          dwMallocBin - Malloc bin
   PVOID          pMem - Memory being reallocated
   size_t         dwSize - Number of bytes
returns
   PVOID - Memory, or NULL if error
*/
PVOID InternalRealloc (DWORD dwMallocBin, PVOID pMem, size_t dwSize)
{
   if (!pMem)
      return NULL;

   dwSize = max(dwSize, 1);   // always allocate at least 1 byte

   // pull out the header
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;

   // make sure it's the right stuff
   _ASSERTE (psbh->dwUniqueID == UNIQUESUBBLOCKID);

#ifdef _DEBUG
   // check integrity
   InternalCheckSubBlockIntegrity (psbh, TRUE);
#endif

   // use this to find the header
   _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);
   PBLOCKHDR pbh = (PBLOCKHDR) ((PBYTE)psbh - psbh->dwBackToHdr);

   // make sure it's the right stuff
   _ASSERTE (pbh->dwUniqueID == UNIQUEBLOCKID);

   if (dwSize <= pbh->dwSubBlockSizeNoHeader) {
      // FUTURE RELEASE - if the size is a lot smaller than it was before then consider
      // shrinking the memory used too? However, dont ever try to shrink memory
      // using my code

      // can just reallocate in place
#ifdef _DEBUG
      if (dwSize < psbh->dwAllocated)
         // wipe out all the memory that reduced
         memset ((PBYTE)pMem + psbh->dwAllocated, OVERRUNBYTE, psbh->dwAllocated - dwSize);
#endif
      psbh->dwAllocated = (DWORD)dwSize;

      return pMem;
   }

   // else, need to allocate new memory and copy into it
   PVOID pNew = InternalMalloc (dwMallocBin, dwSize
#ifdef _DEBUG
      , psbh->dwAllocatedBy, psbh->pszFile, psbh->pszFunction, psbh->dwLine
#endif
      );
   if (!pNew)
      return NULL;   // error

   // copy over.. when dealing with
   size_t dwCopyOver = min(dwSize, pbh->dwSubBlockSizeNoHeader);
#ifdef _DEBUG
   // Dont do so can test better in debug: dwCopyOver = min(dwCopyOver, psbh->dwAllocated);
#endif
   memcpy (pNew, pMem, dwCopyOver);   // BUGFIX - was copying over based on allocated

   // free old
   InternalFree (dwMallocBin, pMem, TRUE);

   // done
   return pNew;
}

/************************************************************************
EscMalloc - Allocate memory in a block.

inputs
   size_t          dwSize - Size in bytes
   PCSTR          pszFile, pszFunction - Where the malloc occrred from
   DWORD          dwLine - Line number
returns
   PVOID - Memory that's allocated, or NULL if error
*/
PVOID EscMalloc (size_t dwSize, PCSTR pszFile, PCSTR pszFunction, DWORD dwLine)
{
   DWORD dwMallocBin = MallocBinChoose();

#ifdef MALLOCOPT
   if (EscAllocatedByGet(0) == 1)
      OutputDebugStringW (L"\r\nMALLOCOPT - Bad");
#endif

   EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   gafInMemoryCritSec[dwMallocBin] = TRUE;

   // increase number of mallocs
   gaiNumMalloc[dwMallocBin]++;

#ifdef _DEBUG
   InternalCheckSystemIntegrity (dwMallocBin);

   BOOL fReportUsage = FALSE;
   if (fReportUsage)
      InternalReportUsage (dwMallocBin);
#endif
   _ASSERTE (!gaPBLOCKHDRTiny[dwMallocBin][0]);

   PVOID pRet;
   if (gfOptimizeMemory)
      pRet = InternalMallocOptimized (dwMallocBin, dwSize
   #ifdef _DEBUG
         , gadwAllocatedBy[dwMallocBin], pszFile, pszFunction, dwLine
   #endif
         );
   else {
      // alloc so don't get fragmented
      pRet = InternalMalloc (dwMallocBin, dwSize
   #ifdef _DEBUG
         , gadwAllocatedBy[dwMallocBin], pszFile, pszFunction, dwLine
   #endif
         );
      _ASSERTE (!gaPBLOCKHDRTiny[dwMallocBin][0]);

      _ASSERTE (InternalMemoryBin (pRet) == dwMallocBin);
   }

   if (pRet)
      gaiUsed[dwMallocBin] += dwSize;

#ifdef _DEBUG
   gaiMaxUsed[dwMallocBin] = max(gaiMaxUsed[dwMallocBin], gaiUsed[dwMallocBin]);
#endif

   gafInMemoryCritSec[dwMallocBin] = FALSE;
   LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);

   return pRet;
}


/************************************************************************
EscRealloc - Reallocate memory in a block.

inputs
   PVOID          pMem - Memory
   size_t          dwSize - Size in bytes
returns
   PVOID - New memory location, or NULL if couldn't reallocate the chunk.
*/
PVOID EscRealloc (PVOID pMem, size_t dwSize
                  , PCSTR pszFile, PCSTR pszFunction, DWORD dwLine
                  )
{
   DWORD dwMallocBin = InternalMemoryBin (pMem);

#ifdef MALLOCOPT
   if (EscAllocatedByGet(0) == 1)
      OutputDebugStringW (L"\r\nMALLOCOPT - Bad");
#endif

   EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   gafInMemoryCritSec[dwMallocBin] = TRUE;


   // increase number of mallocs
   gaiNumMalloc[dwMallocBin]++;

#ifdef _DEBUG
   InternalCheckSystemIntegrity (dwMallocBin);
#endif

   PVOID pRet;
   if (pMem) {
      size_t iCur;
      PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;
      _ASSERTE (psbh->dwUniqueID == UNIQUESUBBLOCKID);
      if (psbh->dwBackToHdr == (DWORD)-1) {
         // optimize for minimum memory
         iCur = psbh->dwAllocated;
         pRet = realloc (psbh, dwSize + sizeof(SUBBLOCKHDR));
         if (pRet) {
            psbh = (PSUBBLOCKHDR)pRet;
            psbh->dwAllocated = (DWORD) dwSize;
            pRet = psbh + 1;
         }
      }
      else {
         // don't fragment
         iCur = InternalMemorySize (pMem, FALSE);
         pRet = InternalRealloc (dwMallocBin, pMem, dwSize);
      }
      if (pRet)
         gaiUsed[dwMallocBin] = gaiUsed[dwMallocBin] - iCur + dwSize;
   }
   else {
      if (gfOptimizeMemory)
         pRet = InternalMallocOptimized (dwMallocBin, dwSize
   #ifdef _DEBUG
            , gadwAllocatedBy[dwMallocBin],pszFile, pszFunction, dwLine
   #endif
         );
      else
         pRet = InternalMalloc (dwMallocBin, dwSize
   #ifdef _DEBUG
            , gadwAllocatedBy[dwMallocBin],pszFile, pszFunction, dwLine
   #endif
         );
      if (pRet)
         gaiUsed[dwMallocBin] += dwSize;
   }

#ifdef _DEBUG
   gaiMaxUsed[dwMallocBin] = max(gaiMaxUsed[dwMallocBin], gaiUsed[dwMallocBin]);
#endif

   gafInMemoryCritSec[dwMallocBin] = FALSE;
   LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   return pRet;
}


/************************************************************************
EscFreeNoCache - Frees memory allocated by EscMalloc()

inputs
   PVOID          pMem - Memory
returns
   None
*/
void EscFreeNoCache (PVOID pMem)
{
#ifdef _DEBUG
   // check the integrity
   InternalCheckMemoryIntegrity (pMem, FALSE);
#endif

#ifdef USESLOWMEMORYCHECKS
   DWORD i;
   // make sure memory is zeroed
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;
   for (i = 0; i < psbh->dwAllocated; i++)
      _ASSERTE ( ((PBYTE)pMem)[i] == FREEMEMBYTE);
#endif

   DWORD dwMallocBin = InternalMemoryBin (pMem);

   EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   gafInMemoryCritSec[dwMallocBin] = TRUE;

#ifdef _DEBUG
   InternalCheckSystemIntegrity (dwMallocBin);
#endif

   if (pMem) {
      // see if fast allocated
      PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)pMem - 1;
      _ASSERTE (psbh->dwUniqueID == UNIQUESUBBLOCKID);

      size_t iCur;

      if (psbh->dwBackToHdr == (DWORD)-1) {
         iCur = psbh->dwAllocated;
         free (psbh);
      }
      else {
         // low-fragment alloc
         iCur = InternalMemorySize (pMem, FALSE);
   
         InternalFree (dwMallocBin, pMem, TRUE);
      }

      gaiUsed[dwMallocBin] -= iCur;
   }

   gafInMemoryCritSec[dwMallocBin] = FALSE;
   LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
}



/************************************************************************
EscMemoryAllocated - Returns the amount of memory allocated.

inputs
   BOOL           fTotal - If TRUE returns the amount of memory allocated
                  total, including buffer memory allocated by the code.
                  If FALSE, only returns the amount of memory the application
                  has asked for.
returns
   size_t - Memory
*/
size_t EscMemoryAllocated (BOOL fTotal)
{
   DWORD dwMallocBin;
   size_t iRet = 0;

   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
      gafInMemoryCritSec[dwMallocBin] = TRUE;

      iRet += fTotal ? gaiAllocated[dwMallocBin] : gaiUsed[dwMallocBin];

      gafInMemoryCritSec[dwMallocBin] = FALSE;
      LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   } // dwMallocBin;

   return iRet;
}



/************************************************************************
EscMemoryAllocateTimes - Returns the number of times that malloc/realloc
have been called.

inputs
   none

returns
   size_t - Count
*/
size_t EscMemoryAllocateTimes (void)
{
   DWORD dwMallocBin;
   size_t iRet = 0;

   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
      gafInMemoryCritSec[dwMallocBin] = TRUE;

      iRet += gaiNumMalloc[dwMallocBin];

      gafInMemoryCritSec[dwMallocBin] = FALSE;
      LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   }

   return iRet;
}

/************************************************************************
EscAllocatedBy - Call this to set one of the 4 dwords so can idenitfy
what function this was allocated by.

inputs
   DWORD          dwIndex - Number from 0..4
   BYTE           bVal - Value to set
returns
   None
*/
void EscAllocatedBySet (DWORD dwIndex, BYTE bVal)
{
   DWORD dwMallocBin;

   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
      gafInMemoryCritSec[dwMallocBin] = TRUE;
      ((BYTE*)(&gadwAllocatedBy[dwMallocBin]))[dwIndex] = bVal;
      gafInMemoryCritSec[dwMallocBin] = FALSE;
      LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   } // dwMallocBin
}


/************************************************************************
EscAllocatedByGet - Call this to get one of the 4 dwords so can idenitfy
what function this was allocated by.

inputs
   DWORD          dwIndex - Number from 0..4
returns
   BYTE - Value
*/
BYTE EscAllocatedByGet (DWORD dwIndex)
{
   DWORD dwMallocBin = 0;  // just get one of them

   EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   gafInMemoryCritSec[dwMallocBin] = TRUE;
   BYTE bRet = ((BYTE*)(&gadwAllocatedBy[dwMallocBin]))[dwIndex];
   gafInMemoryCritSec[dwMallocBin] = FALSE;
   LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   return bRet;
}



#ifdef _DEBUG
/************************************************************************
EscMemoryIntegrity - Checks the integrity of memory allocated by
escarpment, in debug mode only. This will assert if there's an error

inputs
   PVOID pMem
returns
   None
*/
void EscMemoryIntegrity (PVOID pMem)
{
   InternalCheckMemoryIntegrity (pMem, FALSE);

#if 0 // BUGFIX: the following code is wrong
   DWORD dwMallocBin;

   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
      gafInMemoryCritSec[dwMallocBin] = TRUE;
      if (pMem)
         InternalCheckMemoryIntegrity (pMem);
      gafInMemoryCritSec[dwMallocBin] = FALSE;
      LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   } // dwMallocBin
#endif
}

#endif

/************************************************************************
EscMallocInit - Initialize the structures. Called by the DLL load
*/
void EscMallocInit (void)
{
   // how much physical memory
   MEMORYSTATUSEX ms;
   memset (&ms, 0, sizeof(ms));
   ms.dwLength = sizeof(ms);
   GlobalMemoryStatusEx (&ms);
   giFreeMemoryQueueHigh  = ms.ullTotalPhys / 10 / MALLOCBINS; // if too much memory out then free right away

   // initialize the free threads
   DWORD dwMallocBin;
   DWORD i, j;
   DWORD dwID;

   gfFreeMemoryWantToQuit  = FALSE;
   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      if (gahFreeMemoryThread[dwMallocBin])
         continue;   // already allocated

      InitializeCriticalSection (&gacsFreeMemory[dwMallocBin]);

      gaiFreeMemoryPriority[dwMallocBin] = THREAD_PRIORITY_BELOW_NORMAL;
      gahFreeMemoryEvent[dwMallocBin] = CreateEvent (NULL, FALSE, FALSE, NULL);
      gahFreeMemoryThread[dwMallocBin] = CreateThread (NULL, ESCTHREADCOMMITSIZE, FreeMemoryThreadProc, (PVOID) (QWORD) dwMallocBin, 0, &dwID);
      SetThreadPriority (gahFreeMemoryThread[dwMallocBin], gaiFreeMemoryPriority[dwMallocBin]);
   } // dwMallocBin

   // fill in the lazy memory claimer
   gfClaimMemoryWantToQuit = FALSE;
   InitializeCriticalSection (&gcsClaimMemory); // BUGFIX - move out of claim memory
   if (!ghClaimMemoryThread) {

      ghClaimMemoryEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
      ghClaimMemoryThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, ClaimMemoryThreadProc, NULL, 0, &dwID);
      SetThreadPriority (ghClaimMemoryThread, THREAD_PRIORITY_BELOW_NORMAL);
   }

   InitializeCriticalSection (&gcsMallocBinChoose);
   memset (gadwBinThreadID, 0, sizeof(gadwBinThreadID));
   memset (gaiBinLastUsed, 0, sizeof(gaiBinLastUsed));

   // fill in maxallocatable
   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      for (i = 0; i < MALLOCLEVELS; i++)
         for (j = 0; j < MAXSUBBLOCKS; j++)
            gadwMaxAllocatable[dwMallocBin][i][j] = MaxAllocatable(i, j+1);

      InitializeCriticalSection (&gaMemoryCritSec[dwMallocBin]);
      gafInMemoryCritSec[dwMallocBin] = FALSE;
   }

   memset (gaPBLOCKHDR, 0, sizeof(gaPBLOCKHDR));
   memset (gaPBLOCKHDRSmall, 0, sizeof(gaPBLOCKHDRSmall));
   memset (gaPBLOCKHDRTiny, 0, sizeof(gaPBLOCKHDRTiny));
}


/************************************************************************
EscMallocOptimizeMemory - If TRUE (default), optimize for memory.
If FALSE, then optimize so memory not fractured.

inputs
   BOOL        fOptimizeMemory - See above
returns
   none
*/
void EscMallocOptimizeMemory (BOOL fOptimizeMemory)
{
   DWORD dwMallocBin = 0;  // to pick one

   EnterCriticalSection (&gaMemoryCritSec[dwMallocBin]);
   gafInMemoryCritSec[dwMallocBin] = TRUE;
   gfOptimizeMemory = fOptimizeMemory;
   gafInMemoryCritSec[dwMallocBin] = FALSE;
   LeaveCriticalSection (&gaMemoryCritSec[dwMallocBin]);
}

/************************************************************************
InternalReportAndFreeLeak - Given a header, report and free one leak in
it.

inputs
   DWORD          dwMallocBin - Malloc bin
   PBLOCKHDR      pbh - Header
*/
#ifdef _DEBUG
void InternalReportAndFreeLeak (DWORD dwMallocBin, PBLOCKHDR pbh)
{
   char szTemp[1024];

   if (!pbh)
      return;

   // if there aren't any allocated sub-blocks left in the header
   // then report this, and free the header
   if (pbh->dwFree >= pbh->dwMax) {
      sprintf (szTemp, "\r\nESCMEMORY LEAK: Dangling header at %x (%d bytes).",
         (int)(size_t)pbh, (int)pbh->dwOrigSize);
      OutputDebugString (szTemp);

      // free this
      BlockFree (dwMallocBin, pbh);

      return;
   }

   // find the first un-freed element
   DWORD i, j;
   PSUBBLOCKHDR psbh = (PSUBBLOCKHDR)(pbh+1);
   for (i = 0; i < pbh->dwMax; i++, psbh = (PSUBBLOCKHDR)((PBYTE)psbh + pbh->dwSubBlockSizeWithHeader)) {
      _ASSERTE (psbh->dwBackToHdr != (DWORD)-1);
      if (!psbh->dwBackToHdr) // BUGFIX - Was testing dwAllocated
         continue;   // already freed

      // else, found one that needs to be freed
      sprintf (szTemp, "\r\nESCMEMORY LEAK: %s, %s, line %d, ID=%x (%d bytes)",
         psbh->pszFile ? psbh->pszFile : "UnkFile",
         psbh->pszFunction ? psbh->pszFunction : "UnkFunc",
         (int) psbh->dwLine,
         (int)psbh->dwAllocatedBy, (int)psbh->dwAllocated);
      OutputDebugString (szTemp);

      // display first so many bytes
      DWORD dwBytes = min(psbh->dwAllocated, 256);
      PBYTE pb = (PBYTE)(psbh+1);
      OutputDebugString ("\r\n\t");
      szTemp[0] = 0;
      for (j = 0; j < dwBytes; j += 2, pb += 2) {
         if (j+1 < dwBytes)
            sprintf (szTemp + strlen(szTemp), "0x%2x(%c) ", (int)*((WORD*)pb), isprint(*pb) ? (char)(*pb) : '.');
         else
            sprintf (szTemp + strlen(szTemp), "0x%2x(%c) ", (int)*pb, isprint(*pb) ? (char)(*pb) : '.');

         // make sure sdoesn't overflow
         if (strlen(szTemp) > sizeof(szTemp)/2) {
            OutputDebugString (szTemp);
            szTemp[0] = 0;
         }
      }
      OutputDebugString (szTemp);

      // free this
      InternalFree (dwMallocBin, psbh+1, TRUE);
      return;  // so can get called again
   } // i
}


/************************************************************************
InternalReportUsageBlock - Report number of different sized blocks of memory used
*/
void InternalReportUsageBlock (DWORD dwMallocBin, PBLOCKHDR pbh)
{
   if (!pbh)
      return;

   BOOL fDisplayAlloc = (pbh == gaPBLOCKHDRTiny[dwMallocBin][1]);
   if (fDisplayAlloc)
      OutputDebugString ("\r\nESCMEMORY REPORT: Alloc per block: ");

   char szTemp[256];
   DWORD dwUsed = 0, dwMax = 0;
   size_t dwOrig = pbh->dwSubBlockSizeNoHeader;
   for (; pbh; pbh = (PBLOCKHDR)pbh->pNext) {
      dwMax += pbh->dwMax;
      dwUsed += pbh->dwMax - pbh->dwFree;

      if (fDisplayAlloc) {
         sprintf (szTemp, "%d/%d, ", (int)(pbh->dwMax - pbh->dwFree), (int)pbh->dwMax);
         OutputDebugString (szTemp);
      }
   }

   sprintf (szTemp, "\r\nESCMEMORY REPORT: %d byte blocks, %d used out of %d",
      (int)dwOrig, (int)dwUsed, (int)dwMax);
   OutputDebugString (szTemp);
}

/************************************************************************
InternalReportUsage - Report number of different sized blocks of memory used
*/
void InternalReportUsage (DWORD dwMallocBin)
{
   // loop through blocks looking for leaks
   DWORD i, j;
   for (i = 0; i < MEMTINYSIZE/MEMTINYSIZEQUANT; i++)
      InternalReportUsageBlock (dwMallocBin, gaPBLOCKHDRTiny[dwMallocBin][i]);
   for (i = 0; i < MEMSMALLSIZE/MEMSMALLSIZEQUANT; i++)
      InternalReportUsageBlock (dwMallocBin, gaPBLOCKHDRSmall[dwMallocBin][i]);
   for (i = MALLOCLEVELS-1; i < MALLOCLEVELS; i--)
      for (j = MAXSUBBLOCKS-1; j < MAXSUBBLOCKS; j--)
         InternalReportUsageBlock (dwMallocBin, gaPBLOCKHDR[dwMallocBin][i][j]);
}

#endif // _DEBUG


/************************************************************************
EscMemoryFreeThreads - Called by EscUninitialize() to make sure that the
freeing and allocating threads are freed.
*/
void EscMemoryFreeThreads (void)
{
   DWORD dwMallocBin;

   // make sure all the memory is freed in the threads
   gfFreeMemoryWantToQuit = TRUE;
   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
      if (!gahFreeMemoryThread[dwMallocBin])
         continue;

      SetEvent (gahFreeMemoryEvent[dwMallocBin]);

      WaitForSingleObject (gahFreeMemoryThread[dwMallocBin], INFINITE);

      _ASSERTE (!gaiFreeMemoryArrayUsed[dwMallocBin]);
      _ASSERTE (!gaiFreeMemoryQueue[dwMallocBin]);

      CloseHandle (gahFreeMemoryThread[dwMallocBin]);
      CloseHandle (gahFreeMemoryEvent[dwMallocBin]);
      gahFreeMemoryThread[dwMallocBin] = NULL;
      gahFreeMemoryEvent[dwMallocBin] = NULL;

      DeleteCriticalSection (&gacsFreeMemory[dwMallocBin]);

      if (gapFreeMemoryArray[dwMallocBin]) {
         free (gapFreeMemoryArray[dwMallocBin]);
         gapFreeMemoryArray[dwMallocBin] = NULL;
      }
   } // dwMallocBin


   // free up memory claimer
   gfClaimMemoryWantToQuit = TRUE;
   if (ghClaimMemoryThread) {
      EnterCriticalSection (&gcsClaimMemory);
      gfClaimMemoryWantToQuit = TRUE;  // to make sure protected
      LeaveCriticalSection (&gcsClaimMemory);

      SetEvent (ghClaimMemoryEvent);

      WaitForSingleObject (ghClaimMemoryThread, INFINITE);

      _ASSERTE (!giClaimMemoryUsed);

      CloseHandle (ghClaimMemoryThread);
      CloseHandle (ghClaimMemoryEvent);
      ghClaimMemoryThread = NULL;
      ghClaimMemoryEvent = NULL;
      
      // BUGFIX - Moved delete critical section to EscMallocEnd()
      // DeleteCriticalSection (&gcsClaimMemory);

      if (gaClaimMemoryArray) {
         free (gaClaimMemoryArray);
         gaClaimMemoryArray = NULL;
      }
   }

}

/************************************************************************
EscMallocEnd - Shuts down the malloc code. Called by DLL unload.
*/
void EscMallocEnd (void)
{
   EscMemoryFreeThreads ();   // just in case


   DWORD dwMallocBin, i;


#ifdef _DEBUG
   char szTemp[64];
   sprintf (szTemp, "\r\nMalloc collisions: %d", (int)giMallocBinCollisions);
   OutputDebugString (szTemp);
#endif

   for (dwMallocBin = 0; dwMallocBin < MALLOCBINS; dwMallocBin++) {
   #ifdef _DEBUG
      sprintf (szTemp, "\r\nMax alloc in bin %d: %d", (int)dwMallocBin, (int)gaiMaxAllocated[dwMallocBin]);
      OutputDebugString (szTemp);
      sprintf (szTemp, "\r\nMax used in bin %d: %d", (int)dwMallocBin, (int)gaiMaxUsed[dwMallocBin]);
      OutputDebugString (szTemp);

      // loop through blocks looking for leaks
      DWORD j;
      for (i = 0; i < MEMTINYSIZE/MEMTINYSIZEQUANT; i++)
         while (gaPBLOCKHDRTiny[dwMallocBin][i])
            InternalReportAndFreeLeak (dwMallocBin, gaPBLOCKHDRTiny[dwMallocBin][i]);
      for (i = 0; i < MEMSMALLSIZE/MEMSMALLSIZEQUANT; i++)
         while (gaPBLOCKHDRSmall[dwMallocBin][i])
            InternalReportAndFreeLeak (dwMallocBin, gaPBLOCKHDRSmall[dwMallocBin][i]);
      for (i = MALLOCLEVELS-1; i < MALLOCLEVELS; i--)
         for (j = MAXSUBBLOCKS-1; j < MAXSUBBLOCKS; j--)
            while (gaPBLOCKHDR[dwMallocBin][i][j])
               InternalReportAndFreeLeak (dwMallocBin, gaPBLOCKHDR[dwMallocBin][i][j]);
   #endif

      DeleteCriticalSection (&gaMemoryCritSec[dwMallocBin]);

      // look through the level 0 memories and delete them... this will
      // free up everything else
      for (i = 0; i < MAXSUBBLOCKS; i++)
         while (gaPBLOCKHDR[dwMallocBin][0][i])
            BlockFree (dwMallocBin, gaPBLOCKHDR[dwMallocBin][0][i]);
   } // dwMallocBin

   // free up the critical section
   DeleteCriticalSection (&gcsMallocBinChoose);

   DeleteCriticalSection (&gcsClaimMemory);
   return;
}


#if 0 // no longer use because doesn't provide right info
/**************************************************************************************
CEscObject - To make sure ESCMALLOC is all ok
*/
void* CEscObject::operator new (size_t s)
{
   return ESCMALLOC(s);
}

void  CEscObject::operator delete (void* p)
{
   ESCFREE (p);
}
#endif // 0


/************************************************************************
EscOutputDebugStringAlwaysFlush - Set the flag for whether outputdebugstring
should call fflush() on everything

inputs
   BOOL           fAlwaysFlush - Set to TRUE if should always flush
*/
void EscOutputDebugStringAlwaysFlush (BOOL fAlwaysFlush)
{
   gfEscOutputDebugStringAlwaysFlush = fAlwaysFlush;
}

/************************************************************************
EscOutputDebugString - Output debug string that writes to a file
in "c:\EscarpmentDebug.txt" when in release mode... in debug
mode does OutputDebugString.

inputs
   PWSTR          psz - String
returns
   none
*/
void EscOutputDebugString (PCWSTR psz)
{
#ifdef _DEBUG
   OutputDebugStringW (L"EODS:");
   OutputDebugStringW (psz);
#else
   if (!gpfEscarpmentDebug) {
      gpfEscarpmentDebug = fopen ("c:\\EscarpmentDebug.txt", "wb");
      if (!gpfEscarpmentDebug)
         return;

      WORD w = 0xfeff;
      fwrite (&w, sizeof(w), 1, gpfEscarpmentDebug);
   }

   fwrite (psz, sizeof(WCHAR), wcslen(psz), gpfEscarpmentDebug);

   if (gfEscOutputDebugStringAlwaysFlush)
      fflush (gpfEscarpmentDebug);
#endif
}


/************************************************************************
EscOutputDebugStringClose - Forces the debug to close
*/
void EscOutputDebugStringClose (void)
{
   if (gpfEscarpmentDebug) {
      fclose (gpfEscarpmentDebug);
      gpfEscarpmentDebug = NULL;
   }
}


/************************************************************************
EscAtomicStringInit - Initializes the bits necessary for the atomic
strings to work
*/
void EscAtomicStringInit (void)
{
   if (gphAtomicString)
      return;  // error

   gphAtomicString = new CHashString;
   if (!gphAtomicString)
      return;
   gphAtomicString->Init (sizeof(DWORD));

   InitializeCriticalSection (&gcsAtomicString);
}


/************************************************************************
EscAtomicStringEnd - Clears the bits for the atomic strings
*/
void EscAtomicStringEnd (void)
{
   EnterCriticalSection (&gcsAtomicString);
   if (gphAtomicString) {
#ifdef _DEBUG
      DWORD i;
      WCHAR szTemp[64];
      for (i = 0; i < gphAtomicString->Num(); i++) {
         OutputDebugStringW (L"\r\n");
         OutputDebugStringW (gphAtomicString->GetString(i));
         swprintf (szTemp, L" leaked %d times", (int)*((DWORD*)gphAtomicString->Get(i)) );
         OutputDebugStringW (szTemp);
      } // i
#endif // _DEBUG

      delete gphAtomicString;
      gphAtomicString = NULL;
   }
   LeaveCriticalSection (&gcsAtomicString);

   DeleteCriticalSection (&gcsAtomicString);
}

/************************************************************************
EscAtomicStringAdd - Adds an atomic string.

inputs
   PWSTR       psz - String
returns
   PWSTR - String. Do NOT change or free the contents. Call EscAtomicStringFree().
*/
PWSTR EscAtomicStringAdd (PWSTR psz)
{
   PWSTR pszRet;

   if (!psz)
      return NULL;

   EnterCriticalSection (&gcsAtomicString);
   DWORD dwIndex = gphAtomicString->FindIndex (psz, FALSE);
   if (dwIndex != (DWORD)-1) {
      DWORD *pdw = (DWORD*) gphAtomicString->Get (dwIndex);
      // already have, so use existing
      *pdw = *pdw + 1;
      pszRet = gphAtomicString->GetString (dwIndex);
   }
   else {
      // need to add
      DWORD dw = 1;
      if (!gphAtomicString->Add (psz, &dw, FALSE)) {
         pszRet = NULL;
         goto done;
      }

      dwIndex = gphAtomicString->FindIndex (psz, FALSE);
      if (dwIndex == (DWORD)-1) {
         pszRet = NULL;
         goto done;
      }

      pszRet = gphAtomicString->GetString(dwIndex);
   }

done:
   LeaveCriticalSection (&gcsAtomicString);
   return pszRet;
}


/************************************************************************
EscAtomicStringFree - Fress an atomic string, releasing the reference
count by 1.

inputs
   PWSTR       psz - String
returns
   none
*/
void EscAtomicStringFree (PWSTR psz)
{
   if (!psz)
      return;

   EnterCriticalSection (&gcsAtomicString);
   DWORD dwIndex = gphAtomicString->FindIndex (psz, FALSE);
   if (dwIndex == (DWORD)-1) {
#ifdef _DEBUG
      OutputDebugString ("\r\nEscAtomicStringFree() when nothing to frer");
#endif
      goto done;  // error
   }

   DWORD *pdw = (DWORD*)gphAtomicString->Get(dwIndex);
   *pdw = *pdw - 1;
   if (!(*pdw))
      gphAtomicString->Remove (dwIndex);

done:
   LeaveCriticalSection (&gcsAtomicString);

}

