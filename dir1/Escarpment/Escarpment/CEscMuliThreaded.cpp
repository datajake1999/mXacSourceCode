/*******************************************************************
CEscMultiThreaded.cpp - Object that allows for easy multithreaded
separation of tasks.

begun 30/12/2005 by Mike ROzak
Copyright 2005 by Mike Rozak. All rights reserved
*/


#include "mymalloc.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <crtdbg.h>
#include "tools.h"
#include "mmlparse.h"
#include "escarpment.h"
#include "resleak.h"


// MTOQUEUE - queu information
typedef struct {
   PCEscMultiThreaded pMulti;    // callback
   int            iPriority;     // priority to run this at
   HANDLE         hSignal;       // signal back to main thread when done
} MTOQUEUE, *PMTOQUEUE;

#define MTPRIORITYMAX            5     // 5 different thread priorities

/* globals */
static BOOL gfMultiThreadedInit = FALSE;     // set to TRUE if multthreaded has been initialized
static BOOL gfMultiThreadedWantToQuit = FALSE;  // set to TRUE when multithreaded wants to quit
static CListFixed galMTOQUEUE[MTPRIORITYMAX];                // queue
static CListVariable galQueueParams[MTPRIORITYMAX];          // queue of the parameters
static CRITICAL_SECTION gcsMultiThreaded;    // critical seciton for multithreaded list
static DWORD gadwThreadsOut[MTPRIORITYMAX];  // number of threads out for a given priority
static DWORD gdwMaxThreadsPerPriority = 0;   // maximum number of threads that a priority can have

static HANDLE gahThreads[MAXRAYTHREAD];       // handle to the threads
static HANDLE gahThreadSignal[MAXRAYTHREAD];  // signal to notify the thread to wake up and do something
static DWORD gadwThreadNum[MAXRAYTHREAD];    // so the thread knows its number


CRITICAL_SECTION gcsEscMultiThreaded;



/*******************************************************************************
ThreadPriorityToMTPRIORITY - Converts a thread priority to a number from
0..MTPRIORITYMAX-1.

inputs
   int            iPriority - Thread priority
returns
   DWORD - 0 .. MTPRIORITYMAX
*/
DWORD ThreadPriorityToMTPRIORITY (int iPriority)
{
   if (iPriority <= THREAD_PRIORITY_LOWEST)
      return 0;
   else if (iPriority <= THREAD_PRIORITY_BELOW_NORMAL)
      return 1;
   else if (iPriority <= THREAD_PRIORITY_NORMAL)
      return 2;
   else if (iPriority <= THREAD_PRIORITY_ABOVE_NORMAL)
      return 3;
   else // THREAD_PRIORITY_HIGHEST
      return 4;
}


#if 0 // not used
/*******************************************************************************
ThreadPriorityFromMTPRIORITY - Converts a priority to a number from
0..MTPRIORITYMAX-1, to a thread priority.

inputs
   DWORD          dwPriority - 0..MTPRIORITYMAX
returns
   int - Thread priorty
*/
int ThreadPriorityFromMTPRIORITY (DWORD dwPriority)
{
   switch (dwPriority) {
   case 0:
      return THREAD_PRIORITY_LOWEST;
   case 1:
      return THREAD_PRIORITY_BELOW_NORMAL;
   default:
   case 2:
      return THREAD_PRIORITY_NORMAL;
   case 3:
      return THREAD_PRIORITY_ABOVE_NORMAL;
   case 4:
      return THREAD_PRIORITY_HIGHEST;
   }
}
#endif // 0

/*******************************************************************************
HowManyProcessors - Returns the number of processors (or really, threads)
that should be used. This is limted by MAXRAYTHREAD.

inputs
   BOOL        fMaxOut - If TRUE, wont return any more than MAXRAYTHREAD processors
*/
DWORD HowManyProcessors (BOOL fMaxOut)
{
#ifdef MALLOCOPT
   // if doing malloc test, then to simplify things, claim one thread
   return 1;
#endif

   static DWORD dwProcessors = (DWORD)-1;
   if (dwProcessors != -1)
      goto done;

   // allow one thread per processor
   SYSTEM_INFO si;
   memset (&si, 0, sizeof(si));
   GetSystemInfo (&si);

   // BUGFIX - create more than one thread per processor, since draws
   // slightly quicker with 4 threads on dual core... not waiting
   // around as much
   // BUGFIX - On second thought, I'd rather not use this hack,
   // since its only a few percent quicker
   //if (si.dwNumberOfProcessors > 1)
   //   si.dwNumberOfProcessors *= 2;

   dwProcessors = max(si.dwNumberOfProcessors, 1);

done:
   if (fMaxOut)
      return min(MAXRAYTHREAD, dwProcessors);
   else
      return dwProcessors;
}


/*******************************************************************
CEscMultiThreaded::ThreadNum - Returns the number of threads supported
by the processor(s). This won't exceede MAXRAYTHREAD.
*/
DWORD CEscMultiThreaded::ThreadNum (void)
{
   return HowManyProcessors ();
}

/*******************************************************************
CEscMultiThreaded::ThreadActivate - Call this to activate a thread and
have it act on the callback. When the thread is finished, it will
trigger the hEventNotify (or the return value of ThreadActivate()).

Basically, this adds a job to the queue of threads. The job includes
this object (so that EscMultiThreadedCallback() can be called), the
parameters, the parameter size, and the hEventNotify.

When a thread pulls out the queue item, it called EscMultiThreadedCallback().
When the function returns, hEventNotify is set to TRUE/on.

inputs
   PVOID          pParams - Parameters passed to EscMultiThreadedCallback.
                     These parameters are copied to a separate location, so
                     the memory pointed to by pParams can be deleted immediately.
   DWORD          dwParamSize - Number of bytes in pParams.
   HANDLE         hEventNotify - When the callback returns, hEventNotify is set,
                     allowing the caller to WaitForMultipleObjects() or whatever.
                     It is the caller's resposiblility to delete the event.
                     If this is NULL, a new event will be created and returned.
returns
   HANDLE - NULL if there's an error. If hEventNotify is NOT null, this returns
      hEventNotify. If it is NULL, a new event object is created, and that
      is returned; it must be deleted by the caller of ThreadActivate().
*/
HANDLE CEscMultiThreaded::ThreadActivate (PVOID pParams, DWORD dwParamSize, HANDLE hEventNotify)
{
   if (!hEventNotify) {
      hEventNotify = CreateEvent (NULL, FALSE, FALSE, NULL);
      if (!hEventNotify)
         return NULL;
   }

   // add to queue
   MTOQUEUE mtq;
   memset (&mtq, 0, sizeof(mtq));
   mtq.hSignal = hEventNotify;
   mtq.iPriority = GetThreadPriority (GetCurrentThread());
   mtq.pMulti = this;

   DWORD dwPriority = ThreadPriorityToMTPRIORITY (mtq.iPriority);

   EnterCriticalSection (&gcsMultiThreaded);

   // BUGFIX - If has higher priority than current then move up to top of list
   PCListFixed plMTOQUEUE = &galMTOQUEUE[dwPriority];
   PCListVariable plQueueParams = &galQueueParams[dwPriority];
   DWORD dwLast = plMTOQUEUE->Num()-1;
   PMTOQUEUE pmt = (PMTOQUEUE) plMTOQUEUE->Get(0);
   while ((dwLast < plMTOQUEUE->Num()) && (mtq.iPriority > pmt[dwLast].iPriority))
      dwLast--;
   dwLast++;
   if (dwLast < plMTOQUEUE->Num()) {
      // NOTE: Shouldn't happen anymore since a differnet list per queue
      plMTOQUEUE->Insert (dwLast, &mtq);
      plQueueParams->Insert (dwLast, pParams, dwParamSize);
   }
   else {
      plMTOQUEUE->Add (&mtq);
      plQueueParams->Add (pParams, dwParamSize);
   }
   LeaveCriticalSection (&gcsMultiThreaded);

   // set all the events to at least one of the threads wakes up
   DWORD i;
   for (i = 0; i < MAXRAYTHREAD; i++)
      if (gahThreadSignal[i])
         SetEvent (gahThreadSignal[i]);

   // done
   return hEventNotify;
}



/*******************************************************************
CEscMultiThreaded::ThreadLoop - If the code needs to process an array
of data (basically a loop) in a multithreaded manner, then this
method will loop through all the items in the list and hand them out to
the threads in the system, waiting for completion, and returning
after all the threads have done their job.

NOTE: not forcing passes to be exactly sequential with each pass

inputs
   DWORD       dwFrom - Start of the loop (inclusive)
   DWORD       dwTo - End of the loop (exclusive)
   DWORD       dwPasses - Number of passes the loop should be divided into.
                  Basically, each pass will contain (dwTo - dwFrom) / (dwPasses * ThreadNum())
                  values to process.
   PVOID       pParams - Parameters to pass into EscMultiThreadedCallback().
                  The first two parameters are ALWAYS DWORDs, the first being the
                  start value (from dwFrom to dwTo), and the second being the
                  end value (exclusive, from dwFrom to dwTo). Both of the DWORDs
                  will be temporarily modified.
   DWORD       dwParamSize - Number of bytes in the paramters
   PCProgressSocket pProgress - If not NULL, then the progress is called to update the position.
retursn  
   BOOL - TRUE if success.
*/
BOOL CEscMultiThreaded::ThreadLoop (DWORD dwFrom, DWORD dwTo, DWORD dwPasses,
                                    PVOID pParams, DWORD dwParamSize,
                                    PCProgressSocket pProgress)
{
   if (dwParamSize < 2 * sizeof(DWORD))
      return FALSE;

   // create the signals to wait for
   DWORD dwNum = ThreadNum();
   HANDLE ahSignals[MAXRAYTHREAD];
   DWORD i;
   for (i = 0; i < dwNum; i++)
      ahSignals[i] = CreateEvent (NULL, FALSE, FALSE, NULL);

   // send them out
   BOOL afOut[MAXRAYTHREAD];
   memset (afOut, 0, sizeof(afOut));
   DWORD dwInc = (dwTo - dwFrom + dwPasses * dwNum - 1) / (dwPasses * dwNum);
   dwInc = max(dwInc, 1);
   DWORD dwFromOrig = dwFrom;
   for (; dwFrom < dwTo; dwFrom += dwInc) {
      ((DWORD*)pParams)[0] = dwFrom;
      ((DWORD*)pParams)[1] = min(dwFrom+dwInc, dwTo);

      // progress
      if (pProgress)
         pProgress->Update ((float)(dwFrom - dwFromOrig) / (float)(dwTo - dwFromOrig));

      // find a thread that isn't signaled
redo:
      for (i = 0; i < dwNum; i++)
         if (!afOut[i])
            break;
      if (i >= dwNum) {
         // they're all out, so wait
         i = WaitForMultipleObjects (dwNum, ahSignals, FALSE, INFINITE);
         if ((i < WAIT_OBJECT_0) && (i >= WAIT_OBJECT_0+dwNum))
            goto redo;  // shouldnt happen

         i -= WAIT_OBJECT_0;
         afOut[i] = FALSE;
      }

      // sent it out
      if (!ThreadActivate (pParams, dwParamSize, ahSignals[i]))
         continue;   // shouldnt appen

      afOut[i] = TRUE;
   } // dwFrom

   // wait for all the signals to be done
   // free up the signals
   for (i = 0; i < dwNum; i++) {
      if (afOut[i])
         WaitForSingleObject (ahSignals[i], INFINITE);

      CloseHandle (ahSignals[i]);
   }

   // progress
   if (pProgress)
      pProgress->Update (1);

   return TRUE;
}


/*******************************************************************
MultiThreadedInitProc - Thread proc
*/
static DWORD WINAPI MultiThreadedInitProc(LPVOID lpParameter)
{
   DWORD *pdwThread = (DWORD*)lpParameter;
   DWORD dwThread = *pdwThread;
   MTOQUEUE mtq;
   HANDLE hThread = GetCurrentThread();
   int iPriority = GetThreadPriority (hThread);
   CMem mem;
   DWORD dwPriority;

   while (TRUE) {
      EnterCriticalSection (&gcsMultiThreaded);

      // find thread that needs processing
      PCListFixed palMTOQUEUE = NULL;
      BOOL fFoundStuffToQueue = FALSE;
      for (dwPriority = MTPRIORITYMAX-1; dwPriority < MTPRIORITYMAX; dwPriority--) {
         palMTOQUEUE = &galMTOQUEUE[dwPriority];
         if (!palMTOQUEUE->Num())
            continue; // empty

         // found something
         fFoundStuffToQueue = TRUE;

         // only so many threads out of a specific priority
         if (gadwThreadsOut[dwPriority] >= gdwMaxThreadsPerPriority)
            continue;

         // if get here then can pull from this queue
         break;
      } // dwPriority, over MTPRIORITYMAX

      // if found nothing at all tot queue
      if (!fFoundStuffToQueue) {
         // nothing in the queue...

         // see if want to quit
         if (gfMultiThreadedWantToQuit) {
            LeaveCriticalSection (&gcsMultiThreaded);
            break;
         }

         // else, wait for a signal, and continue
         LeaveCriticalSection (&gcsMultiThreaded);
         WaitForSingleObject (gahThreadSignal[dwThread], INFINITE);
         continue;
      }

      // if found something, but can't act on it now because there
      // are already enough of that priority
      if ((dwPriority >= MTPRIORITYMAX) || !palMTOQUEUE) {
         // else, wait for a signal, and continue
         LeaveCriticalSection (&gcsMultiThreaded);
         WaitForSingleObject (gahThreadSignal[dwThread], INFINITE);
         continue;
      }

      PCListVariable palQueueParams = &galQueueParams[dwPriority];

      PMTOQUEUE pmtq = (PMTOQUEUE) palMTOQUEUE->Get(0);
      // else, have something that want to process
      mtq = *pmtq;
      size_t dwSize = palQueueParams->Size(0);
      mem.Required (dwSize);
      memcpy (mem.p, palQueueParams->Get(0), dwSize);
      mem.m_dwCurPosn = dwSize;
      palMTOQUEUE->Remove (0);
      palQueueParams->Remove (0);
      gadwThreadsOut[dwPriority]++;
      LeaveCriticalSection (&gcsMultiThreaded);

      // process

      // change priotity
      iPriority = GetThreadPriority (hThread);
         // BUGFIX - Get priority here just in case other callback had changed it
      if (iPriority != mtq.iPriority) {
         iPriority = mtq.iPriority;
         SetThreadPriority (hThread, iPriority);
      }

      // callback
      mtq.pMulti->EscMultiThreadedCallback (mem.p, (DWORD) mem.m_dwCurPosn, dwThread);

      // notify event
      SetEvent (mtq.hSignal);

      // restore counters
      EnterCriticalSection (&gcsMultiThreaded);
      DWORD i;
      // if had just decreased from max-use priority, then altert all the other threads
      // that they might wish to start up now
      if (gadwThreadsOut[dwPriority] >= gdwMaxThreadsPerPriority)
         for (i = 0; i < MAXRAYTHREAD; i++)
            if (gahThreadSignal[i])
               SetEvent (gahThreadSignal[i]);
      gadwThreadsOut[dwPriority]--;
      LeaveCriticalSection (&gcsMultiThreaded);


      // look for something else in the queue
   } // while TRUE

   return 0;
}


/*******************************************************************
EscMutliThreadedInit - Initializes the code that enables the
multithreaded objects.
*/
BOOL EscMultiThreadedInit (void)
{
   EnterCriticalSection (&gcsEscMultiThreaded);
   if (gfMultiThreadedInit) {
      LeaveCriticalSection (&gcsEscMultiThreaded);
      return FALSE;  // erorr
   }

   gfMultiThreadedWantToQuit = FALSE;
   DWORD i;
   for (i = 0; i < MTPRIORITYMAX; i++) {
      galMTOQUEUE[i].Init (sizeof(MTOQUEUE));
      galQueueParams[i].Clear();
      gadwThreadsOut[i] = 0;
   }

   InitializeCriticalSection (&gcsMultiThreaded);
   
   DWORD dwNum = HowManyProcessors();
   gdwMaxThreadsPerPriority = dwNum;

   // BUGFIX - Forgot to create enough threads, 2x number of processors
   dwNum = min(dwNum * 2, MAXRAYTHREAD);

   memset (gahThreads, 0, sizeof(gahThreads));
   memset (gahThreadSignal, 0, sizeof(gahThreadSignal));
   for (i = 0; i < dwNum; i++) {
      gadwThreadNum[i] = i;
      gahThreadSignal[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      DWORD dwID;
      gahThreads[i] = CreateThread (NULL, ESCTHREADCOMMITSIZE, MultiThreadedInitProc, &gadwThreadNum[i], 0, &dwID);
   } // i

   gfMultiThreadedInit = TRUE;
   LeaveCriticalSection (&gcsEscMultiThreaded);
   return TRUE;
}



/*******************************************************************
EscMutliThreadedEnd - Shuts down the code that enables multithreade
objects.
*/
void EscMultiThreadedEnd (void)
{
   EnterCriticalSection (&gcsEscMultiThreaded);

   if (!gfMultiThreadedInit) {
      LeaveCriticalSection (&gcsEscMultiThreaded);
      return;  // done
   }

   // indicate that want to shut down
   EnterCriticalSection (&gcsMultiThreaded);
   gfMultiThreadedWantToQuit = TRUE;
   LeaveCriticalSection (&gcsMultiThreaded);


   DWORD dwNum = HowManyProcessors();

   // BUGFIX - Forgot to create enough threads, 2x number of processors
   dwNum = min(dwNum * 2, MAXRAYTHREAD);

   DWORD i;


   // set the evetns so know should shut down
   for (i = 0; i < dwNum; i++)
      SetEvent (gahThreadSignal[i]);

   // wait for shutdown
   for (i = 0; i < dwNum; i++) {

      // wait for the thread to shut down
      WaitForSingleObject (gahThreads[i], INFINITE);
      CloseHandle (gahThreads[i]);
      CloseHandle (gahThreadSignal[i]);
   } // i


   DeleteCriticalSection (&gcsMultiThreaded);

   // so dont have memory leak problems
   for (i = 0; i < MTPRIORITYMAX; i++) {
      galMTOQUEUE[i].ClearCompletely();
      galQueueParams[i].ClearCompletely();
      _ASSERTE (gadwThreadsOut[i] == 0);
   }
   gfMultiThreadedInit = FALSE;

   LeaveCriticalSection (&gcsEscMultiThreaded);
}
