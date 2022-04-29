/*************************************************************************************
CRenderThread.cpp - Code for managing the renderingthread.

begun 10/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <Psapi.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"

#define MAXTWORENDERSHARDS             // limit number of render shards to two
// BUGFIX - Limit number of render shards to two

#define PROCESSORSPERRENDERSHARD       8     // one extra render shard per 8 processors

#define SILENCE               L"<s>"         // silence string
#define MAXIMAGECACHE         (giTotalPhysicalMemory / 2)     // 1 gigbyte max image cache
      // BUGFIX - Was 500000000
            // BUGFIX - Make 500 MB
#define MAXMEMORY32           1500000000     // 1.5 gig on 32-bit windows
#define MINMEMORY             512000000      // 512 meg
         // BUGFIX - Was 768

#define PRERENDERTIMERANGE    1000           // 1 second time-range for selecting which prerender to generate


// POSTIMAGERENDERED - Parameters for PostImageRendered() so can do this in a separate thread
typedef struct {
   PCImageStore         pis;     // image store
   BOOL                 fSave;   // as per call
   BOOL                 fPostToMain;   // as per call
   DWORD                dwQuality;     // as per call
   DWORD                dwFinalQualityImage; // set to TRUE if this is as good as the image will get
} POSTIMAGERENDERED, *PPOSTIMAGERENDERED;

// RESOLUTIONFILEINFO - To store what kind of detail
typedef struct {
   int         iResolution;      // type of resolution used
   DWORD       dwShadowsFlags;   // type of shadow flags used
   int         iTextureDetail;   // type of texture detail used
} RESOLUTIONFILEINFO, *PRESOLUTIONFILEINFO;

PWSTR gpszResolutionFileInfo = L"ResolutionFileInfo";



/*************************************************************************************
CRenderThreadProgress:: ... - For progress calls
*/
BOOL CRenderThreadProgress::Push (float fMin, float fMax)
{
   return m_pRT->MyPush (m_dwRenderShard, fMin, fMax);
}

BOOL CRenderThreadProgress::Pop (void)
{
   return m_pRT->MyPop (m_dwRenderShard);
}

int CRenderThreadProgress::Update (float fProgress)
{
   return m_pRT->MyUpdate (m_dwRenderShard, fProgress);
}

BOOL CRenderThreadProgress::WantToCancel (void)
{
   return m_pRT->MyWantToCancel (m_dwRenderShard);
}

void CRenderThreadProgress::CanRedraw (void)
{
   m_pRT->MyCanRedraw (m_dwRenderShard);
}

void CRenderThreadProgress::RS360LongLatGet (fp *pfLong, fp *pfLat)
{
   return m_pRT->MyRS360LongLatGet (m_dwRenderShard, pfLong, pfLat);
}

void CRenderThreadProgress::RS360Update (PCImageStore pImage)
{
   m_pRT->MyRS360Update (m_dwRenderShard, pImage);
}


/*************************************************************************************
CRenderThread::Constructor and destructor
*/
CRenderThread::CRenderThread (void)
{
#if 0 // def _DEBUG
   if (sizeof(PVOID) > sizeof(DWORD))
      EscMemoryIntegrity(this);
#endif

   // figure out how many render shards
   DWORD dwProcessors = HowManyProcessors(FALSE);
   m_dwRenderShards = 1;   // at least one
#ifdef MAXTWORENDERSHARDS
   if (dwProcessors >= 4)
      m_dwRenderShards++;
#else
   m_dwRenderShards += dwProcessors / PROCESSORSPERRENDERSHARD;
      // so 8 or more processors gets extra
   if (dwProcessors >= 2)
      m_dwRenderShards++;  // so double/quad cores will get at least two
#endif
   if (sizeof(PVOID) <= sizeof(DWORD))
      m_dwRenderShards = 1;   // on 32 bit, because of memory contraints, only one shard
   m_dwRenderShards = min(m_dwRenderShards, MAXRENDERSHARDS);

   // BUGBUG - to see if can avoid crash
   // BUGBUG - since seems to work now, comment this out, as it should be
   // m_dwRenderShards = 1;
   // BUGBUG

   DWORD i;
   for (i = 0; i < m_dwRenderShards; i++) {
      m_aRTP[i].m_dwRenderShard = i;
      m_aRTP[i].m_pRT = this;

      m_ahThread[i] = NULL;
      m_ahSignalFromThread[i] = NULL;
      m_ahSignalToThread[i] = NULL;
      m_afConnected[i] = FALSE;
      m_afM3DCanStart[i] = FALSE;
      m_afM3DInitialized[i] = FALSE;
      m_apRS360Node[i] = NULL;

      memset (&m_artCurrent[i], 0, sizeof(m_artCurrent[i]));
      m_adwLastUpdateTime[i] = 0;
      m_afProgress[i] = 0;
      m_alStack[i].Init (sizeof(TEXTUREPOINT));
      m_afRendering360[i] = FALSE;
      m_afDontAbort360[i] = FALSE;
      m_afRenderingPreRender[i] = FALSE;
      m_adwRenderingHighQuality[i] = 0;
      m_afWantToQuit[i] = FALSE;

      m_adwCacheFree[i] = 0;
      m_adwCacheFreeLast[i] = 0;
      m_afCacheFreeBig[i] = 0;
   } // i


   m_hThreadSave = NULL;
   m_hSignalFromThreadSave = NULL;
   m_hSignalToThreadSave = NULL;

   m_hWndNotify = NULL;
   m_dwMessage = NULL;
   m_fWantToQuitSave = FALSE;
   m_iResolution = 0;
   m_iResolutionLow = m_iResolution - 2;
   m_iTextureDetail = 0;
   m_fLipSync = TRUE;
   m_fLipSyncLow = FALSE;
   m_dwShadowsFlags = 0; // BUGFIX - Have two-pass off by default SF_TWOPASS360;
   m_dwShadowsFlagsLow = 0;
   m_f360Long = m_f360Lat = 0;


   for (i = 0; i < RTQUEUENUM; i++)
      m_alRTQUEUE[i].Init (sizeof(RTQUEUE));
   m_dw360InQueue = m_dw360InQueueQuality = 0;

   // find out how much memory is in the computer
   MEMORYSTATUSEX ms;
   memset (&ms, 0, sizeof(ms));
   ms.dwLength = sizeof(ms);
   GlobalMemoryStatusEx (&ms);
   m_iPhysicalTotal = (__int64)ms.ullTotalPhys;
   m_iPhysicalTotal = max(m_iPhysicalTotal, MINMEMORY); // at least treat as 512 meg, or will be freeing up all the time
   m_iPhysicalTotal = max(m_iPhysicalTotal, 100 * 1000000); // 100 mb min
   m_iPhysicalTotalHalf = m_iPhysicalTotal * 2 / 3; // a bit more than half
   // if 32-bit windows then limit to 1.5 gig
   if (sizeof(PVOID) < 8) {
      m_iPhysicalTotal = min (m_iPhysicalTotal, MAXMEMORY32);
      m_iPhysicalTotalHalf = min (m_iPhysicalTotalHalf, MAXMEMORY32);
   }

   InitializeCriticalSection (&m_CritSec);
   InitializeCriticalSection (&m_CritSecSave);
   m_lPOSTIMAGERENDERED.Init (sizeof(POSTIMAGERENDERED));

#if 0 // def _DEBUG
   if (sizeof(PVOID) > sizeof(DWORD))
      EscMemoryIntegrity(this);
#endif
}

CRenderThread::~CRenderThread (void)
{
#if 0 // def _DEBUG
   if (sizeof(PVOID) > sizeof(DWORD))
      EscMemoryIntegrity(this);
#endif

   BOOL fFound = FALSE;
   DWORD i;
   for (i = 0; i < m_dwRenderShards; i++)
      if (m_ahThread[i])
         fFound = TRUE;
   if (fFound || m_hThreadSave)
      Disconnect();

#if 0 // def _DEBUG
   if (sizeof(PVOID) > sizeof(DWORD))
      EscMemoryIntegrity(this);
#endif

   DeleteCriticalSection (&m_CritSec);
   DeleteCriticalSection (&m_CritSecSave);
   // the act of disconnecting should free everything up

   // free up the queues
   DWORD j;
   for (i = 0; i < RTQUEUENUM; i++) {
      PRTQUEUE prt = (PRTQUEUE)m_alRTQUEUE[i].Get(0);
      for (j = 0; j < m_alRTQUEUE[i].Num(); j++, prt++) {
         if (prt->pNode)
            delete prt->pNode;
         if (prt->pMem)
            delete prt->pMem;
         prt->pNode = NULL;
         prt->pMem = NULL;
         if (prt->fIs360 && (i < RTQUEUE_PRERENDER) ) {
            if (i & RTQUEUE_HIGHQUALITYBIT)
               m_dw360InQueueQuality--;
            else
               m_dw360InQueue--;
         }
      }
      m_alRTQUEUE[i].Clear();
   }

#if 0 // def _DEBUG
   if (sizeof(PVOID) > sizeof(DWORD))
      EscMemoryIntegrity(this);
#endif
}


/*************************************************************************************
RenderThreadProc - Thread that handles the Render.
*/
DWORD WINAPI RenderThreadProc(LPVOID lpParameter)
{
   PCRenderThreadProgress pThreadProgress = (PCRenderThreadProgress) lpParameter;

   pThreadProgress->m_pRT->ThreadProc (pThreadProgress->m_dwRenderShard);

   return 0;
}


/*************************************************************************************
RenderThreadSaveProc - Thread that handles the Render.
*/
DWORD WINAPI RenderThreadSaveProc(LPVOID lpParameter)
{
   PCRenderThread pThread = (PCRenderThread) lpParameter;

   pThread->ThreadProcSave ();

   return 0;
}


/*************************************************************************************
CRenderThread::ThreadProc - This internal function handles the thread procedure.
It first creates the packet sending object and tries to log on. If it fails
it sets m_afConnected to FALSE and sets a flag. If it succedes it sets m_afConnected
to true, and loops until m_ahSignalToThread is set.
*/
void CRenderThread::ThreadProc (DWORD dwRenderShard)
{
   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   m_afConnected[dwRenderShard] = FALSE;

   // else, it went through...
   m_afConnected[dwRenderShard] = TRUE;
   SetEvent (m_ahSignalFromThread[dwRenderShard]);

   // wait, either taking message from the queue, or an event, or doing processing
   MSG msg;
   BOOL fLastRenderSuccessful = FALSE;
   while (TRUE) {
#if 0 // def _DEBUG
   if (sizeof(PVOID) > sizeof(DWORD))
      EscMemoryIntegrity(this);
#endif

      // handle message queue
      while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
         TranslateMessage (&msg);
         DispatchMessage(&msg); 
      }

      // shouldnt start doing the rendering until actually get
      // a message that all the files have been deleted, so can then load
      // in the rendering libraries, etc.
      if (!m_afM3DInitialized[dwRenderShard] && m_afM3DCanStart[dwRenderShard]) {
         // NOTE: Don't worry about thread safe access to m_afM3DCanStart because
         // can only be set or not. so ok
         m_afM3DInitialized[dwRenderShard] = M3DInit (dwRenderShard, TRUE, &m_aRTP[dwRenderShard]);
         
         MyCacheM3DFileInit (dwRenderShard);
         m_aRTP[dwRenderShard].Update (0); // so clears
      }

      // pull rendering off the queue. Note; Only doing render once so can
      // shut down faster, and 50 ms isn't that long of a gap unless trying
      // to animate everything
      BOOL fRendered = FALSE;
      if (m_afM3DInitialized[dwRenderShard]) {
         fRendered = DoRender (dwRenderShard);

         // if the last render was successful, and this one can't find
         // any, then sent the I'm bored message
         if (fLastRenderSuccessful && !fRendered)
            PostMessage (m_hWndNotify, m_dwMessage, RENDERTHREAD_IMBORED, 0);
         fLastRenderSuccessful = fRendered;
      }

      // wait for a signalled semaphore, or 50 millisec, so can see if any new message
      // BUGFIX - Changed from 50 ms to 10 ms to waste as little time as possible between renders
      // BUGFIX - If had just rendered then very short wait to minimize pause
      if (WAIT_OBJECT_0 == WaitForSingleObject (m_ahSignalToThread[dwRenderShard], fRendered ? 1 : 10))
         break;   // just received notification that should quit

      // else, 50 milliseconds has ellapsed, so repeat and see if any new messages
      // in queue
   }


   // if have loaded in rendering libraries then will need to shut them
   // down here
   if (m_afM3DInitialized[dwRenderShard]) {
      MyCacheM3DFileEnd (dwRenderShard);
      M3DEnd (dwRenderShard, &m_aRTP[dwRenderShard]);
   }

   // all done
}



/*************************************************************************************
CRenderThread::ThreadProcSave - This internal function handles the thread procedure.
It first creates the packet sending object and tries to log on. If it fails
it sets m_fConnected to FALSE and sets a flag. If it succedes it sets m_fConnected
to true, and loops until m_hSignalToThread is set.
*/
void CRenderThread::ThreadProcSave (void)
{
   SetEvent (m_hSignalFromThreadSave);

   // wait, either taking message from the queue, or an event, or doing processing
   POSTIMAGERENDERED pir;
   while (TRUE) {
      EnterCriticalSection (&m_CritSecSave);
      if (!m_lPOSTIMAGERENDERED.Num()) {
         LeaveCriticalSection (&m_CritSecSave);
         if (m_fWantToQuitSave)
            break;

         WaitForSingleObject (m_hSignalToThreadSave, INFINITE);
         continue;
      }

      // copy first entry
      pir = *((PPOSTIMAGERENDERED)m_lPOSTIMAGERENDERED.Get(0));
      m_lPOSTIMAGERENDERED.Remove (0);
      LeaveCriticalSection (&m_CritSecSave);

      // save this
#ifdef _DEBUG
      DWORD dwTime = GetTickCount();
#endif

      // see if the server wants a copy of this file
      // Must have turned on and registered for this to work
      if (gfRenderCache && RegisterMode() &&
         gpMainWindow && gpMainWindow->m_pIT && gpMainWindow->m_pIT->m_pPacket &&
         pir.pis && pir.fSave && pir.dwFinalQualityImage) {

         // NOTE: NOT testing for gpMainWindow->m_fConnectRemote since if this is
         // a local setup, then query will return FALSE anyway.

         // make up the filename
         CMem mem;
         PWSTR pszFile;
         if (MMLToMem (pir.pis->MMLGet(), &mem)) {
            mem.CharCat (0);   // so null terminated
            pszFile = (PWSTR)mem.p;
         }
         else
            pszFile = NULL;
         size_t dwLen = (wcslen(pszFile)+1) * sizeof(WCHAR);

         // if queued to send then exit
         BOOL fQueuedToSend = FALSE;
         // BUGFIX - Was stopping automatically, but now, let up to 2 meg be queued up
         if (gpMainWindow->m_pIT->m_pPacket->QueuedToSend (PACKCHAN_RENDER) > MAXQUEUETOSEND_RENDER)
            fQueuedToSend = TRUE;

         int iQualityMono = gpMainWindow->QualityMonoGet ();
         if (iQualityMono < 0)
            fQueuedToSend = TRUE;   // so doesn't send

         // combine the two
         CMem memAll;
         size_t dwNeed = sizeof(CIRCUMREALITYPACKETSEVERQUERYWANT) + dwLen; //  + memData.m_dwCurPosn;
         if (memAll.Required (dwNeed) && gpMainWindow->m_pIT && !fQueuedToSend) {
            PCIRCUMREALITYPACKETSEVERQUERYWANT prw = (PCIRCUMREALITYPACKETSEVERQUERYWANT) memAll.p;
            memset (prw, 0, sizeof(*prw));
            prw->dwQuality = (DWORD)iQualityMono;
            prw->dwStringSize = (DWORD) dwLen;
            // prw->dwDataSize = (DWORD) memData.m_dwCurPosn;

            memcpy (prw+1, pszFile, prw->dwStringSize);
            // memcpy ((PBYTE)(prw+1) + prw->dwStringSize, memData.p, prw->dwDataSize);

            gpMainWindow->m_pIT->m_pPacket->PacketSend (CIRCUMREALITYPACKET_SERVERQUERYWANTRENDER, prw, (DWORD) dwNeed);
         }
      }

      PostImageRendered (pir.pis, pir.fSave, pir.fPostToMain, pir.dwQuality, TRUE);
#ifdef _DEBUG
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nPostImageRendered = %d", (int)(GetTickCount() - dwTime));
      OutputDebugStringW (szTemp);
#endif
   }

   // all done
}

/*************************************************************************************
CRenderThread::Calc360InQueue - Figure out how many 360's in the queue.
MUST be in m_CritSec.
*/
void CRenderThread::Calc360InQueue (void)
{
#ifdef _DEBUG
   DWORD dwOldQueue = m_dw360InQueue;
   DWORD dwOldQueueQuality = m_dw360InQueueQuality;
#endif

   DWORD i, j;
   m_dw360InQueue = m_dw360InQueueQuality = 0;

   // look already render
   for (i = 0; i < m_dwRenderShards; i++)
      if (m_afRendering360[i] && !m_afRenderingPreRender[i]) {
         if (m_adwRenderingHighQuality[i])
            m_dw360InQueueQuality++;
         else
            m_dw360InQueue++;
      }

   PRTQUEUE prt;
   for (i = 0; i < RTQUEUE_PRERENDER; i++) {
      prt = (PRTQUEUE) m_alRTQUEUE[i].Get(0);
      for (j = 0; j < m_alRTQUEUE[i].Num(); j++, prt++)
         if (prt->fIs360) {
            if (i & RTQUEUE_HIGHQUALITYBIT)
               m_dw360InQueueQuality++;
            else
               m_dw360InQueue++;
         }
   } // i

   // BUGFIX - Dont do because will be hit even if OK _ASSERTE (dwOldQueue == m_dw360InQueue);
   // BUGFIX - Dont do because will be hit even if OK _ASSERTE (dwOldQueueQuality == m_dw360InQueueQuality);
}


/*************************************************************************************
CRenderThread::Connect- This is an initialization function. It creates the thread.
If it fails it returns FALSE. If it succedes, it returns
TRUE.

NOTE: Call this from the MAIN thread.

inputs
   HWND           hWndNotify - When an image is completed, this will receive a post
                     indicating there's a new image. wParam contains the message
                     type of RENDERTHREAD_XXX.
   DWORD          dwMessage - This is the WM_ message that's posted to indicate the
                     new message.
returns
   BOOL - TRUE if connected and thread created. FALSE if failed to connect and
      no thread created.
*/
BOOL CRenderThread::Connect (HWND hWndNotify, DWORD dwMessage)
{
   BOOL fFound = FALSE;
   DWORD i;
   for (i = 0; i < m_dwRenderShards; i++)
      if (m_ahThread[i])
         fFound = TRUE;
   if (fFound || m_hThreadSave)
      return FALSE;  // cant call a second time

   m_hWndNotify = hWndNotify;
   m_dwMessage = dwMessage;
   for (i = 0; i < m_dwRenderShards; i++)
      m_afConnected[i] = FALSE;

   // create the save thread
   m_hSignalFromThreadSave = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalToThreadSave = CreateEvent (NULL, FALSE, FALSE, NULL);
   DWORD dwID;
   m_hThreadSave = CreateThread (NULL, ESCTHREADCOMMITSIZE, RenderThreadSaveProc, this, 0, &dwID);
   if (!m_hThreadSave) {
      CloseHandle (m_hSignalFromThreadSave);
      CloseHandle (m_hSignalToThreadSave);
      m_hSignalFromThreadSave = m_hSignalToThreadSave = NULL;
      return FALSE;
   }
   // BUGFIX - Lower priority of rendering
   SetThreadPriority (m_hThreadSave, VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));

   // wait for the signal from thread, to know if initialization succeded
   WaitForSingleObject (m_hSignalFromThreadSave, INFINITE);


   for (i = 0; i < m_dwRenderShards; i++) {
      // create all the events
      m_ahSignalFromThread[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_ahSignalToThread[i] = CreateEvent (NULL, FALSE, FALSE, NULL);
      m_ahThread[i] = CreateThread (NULL, ESCTHREADCOMMITSIZE, RenderThreadProc,
         &m_aRTP[i], 0, &dwID);
      if (!m_ahThread[i]) {
         CloseHandle (m_ahSignalFromThread[i]);
         CloseHandle (m_ahSignalToThread[i]);
         m_ahSignalFromThread[i] = m_ahSignalToThread[i] = NULL;
         return FALSE;
      }
      // BUGFIX - Lower priority of rendering
      SetThreadPriority (m_ahThread[i], VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL /* BUGFIX - NOt dependent on render version , -(int)i */));

      // wait for the signal from thread, to know if initialization succeded
      WaitForSingleObject (m_ahSignalFromThread[i], INFINITE);
      if (!m_afConnected[i]) {
         // error. which means thread it shutting down. wait for it
         WaitForSingleObject (m_ahThread[i], INFINITE);
         CloseHandle (m_ahThread[i]);
         CloseHandle (m_ahSignalFromThread[i]);
         CloseHandle (m_ahSignalToThread[i]);
         m_ahSignalFromThread[i] = m_ahSignalToThread[i] = NULL;
         m_ahThread[i] = NULL;
         return FALSE;
      }
   } // i, MAXREDERSHARDS

   // else, all ok
   return TRUE;
}


/*************************************************************************************
CRenderThread::Disconnect - Call this to cancel a connection made by Connect().
This is autoamtically called if the CRenderThread object is deleted.
It also deletes the thread it creates.
*/
BOOL CRenderThread::Disconnect (void)
{
   DWORD i;
   for (i = 0; i < m_dwRenderShards; i++)
      if (m_ahThread[i]) {
         m_afWantToQuit[i] = TRUE;
         // signal
         SetEvent (m_ahSignalToThread[i]);
      }

   for (i = 0; i < m_dwRenderShards; i++)
      if (m_ahThread[i]) {
         // wait, since already sent the signal
         WaitForSingleObject (m_ahThread[i], INFINITE);

         // delete all
         CloseHandle (m_ahThread[i]);
         CloseHandle (m_ahSignalFromThread[i]);
         CloseHandle (m_ahSignalToThread[i]);
         m_ahSignalFromThread[i] = m_ahSignalToThread[i] = NULL;
         m_ahThread[i] = NULL;
      }

   // shut down the save thread
   if (m_hThreadSave) {
      m_fWantToQuitSave = TRUE;

      // signal
      SetEvent (m_hSignalToThreadSave);

      // wait
      WaitForSingleObject (m_hThreadSave, INFINITE);

      // delete all
      CloseHandle (m_hThreadSave);
      CloseHandle (m_hSignalFromThreadSave);
      CloseHandle (m_hSignalToThreadSave);
      m_hSignalFromThreadSave = m_hSignalToThreadSave = NULL;
      m_hThreadSave = NULL;
   }

   return TRUE;
}


/*************************************************************************************
CRenderThread::BumpRenderPriority - Ups the render order/priority of a specific
image. Called in the main/image windows when fail to load.

inputs
   PCWSTR            pszImageString - MML string that described entire image
   BOOL              fCreateIfNotExist - If can't find, create it?
returns
   BOOL - TRUE if found
*/
BOOL CRenderThread::BumpRenderPriority (PCWSTR pszImageString, BOOL fCreateIfNotExist)
{
   BOOL fRet = FALSE;
   EnterCriticalSection (&m_CritSec);

   DWORD i, j;
   PRTQUEUE prt;
   RTQUEUE rt;
   for (i = 0; (i < RTQUEUE_PRERENDER) && !fRet; i++) {
      prt = (PRTQUEUE)m_alRTQUEUE[i].Get(0);

      DWORD dwQueueTo = RTQUEUE_HIGH + (i & RTQUEUE_HIGHQUALITYBIT);

      for (j = 0; j < m_alRTQUEUE[i].Num(); j++, prt++) {
         if (!prt->pMem)
            continue;

         if (_wcsicmp ((PWSTR)prt->pMem->p, pszImageString))
            continue;   // not the same

         // if get here, match, so bump up
         fRet = TRUE;
         if (!j && (i == dwQueueTo)) {
            // found it, but would be bumping up self, so don't bother
            break;
         }

         // move to top, of highest priority render
         rt = *prt;
         m_alRTQUEUE[i].Remove (j);
         m_alRTQUEUE[dwQueueTo].Insert (0, &rt);
         // fRet is already set to true
         break;
      } // j

   } // i

   // if found then done
   LeaveCriticalSection (&m_CritSec);
   if (fRet || !fCreateIfNotExist)
      return fRet;

   // if can't find, then add
   PCMMLNode2 pNode = CircumrealityParseMML ((PWSTR)pszImageString);
   if (pNode) {
      RequestRender (pNode, RTQUEUE_HIGH, -1, TRUE);
      delete pNode;
   }

   return fRet;
}

/*************************************************************************************
CRenderThread::RequestRender - Call this to request that a command from the
server be rendered.

THREAD SAFE - Call from any thread.

inputs
   PCMMLNode2         pNode - This is duplicated by the call. NOte: It looks
                           at pNode->NameGet(), so if the node isn't specifically
                           for the renderer then it will be ignored. Likewise,
                           if the node is alreay queued up this will be ignored.
   DWORD             dwPriority - RTQUEUE_HIGH, RTQUEUE_MEDIUM, or RTQUEUE_LOW
                           RTQUEUE_PRERENDER is a special case, set internally when a <PreRender> message comes in
   int               iDirection - Direction that the pre-render is in. Used so that if
                           the player is looking in a specific direction, that will be prerendered first.
                           Use 0..1 (N, NE, E, etc.), or -1 for no known direction
   BOOL              fNotInList - Adding because it isn't already in the list, but should be
returns
   BOOL - TRUE if the node was added, FALSE if it wasn't
*/
BOOL CRenderThread::RequestRender (PCMMLNode2 pNode, DWORD dwPriority, int iDirection, BOOL fNotInList)
{
   BOOL fRet = FALSE;
   BOOL fInCritical = FALSE;
   CMem MemOrig;

   dwPriority &= ~RTQUEUE_HIGHQUALITYBIT; // to make sure bit isn't set

   // see if it's a node that we can handle
   PWSTR psz = pNode->NameGet();
   if (!psz)
      goto done;

   // if it's a queue then disect the queue
   if (!_wcsicmp(psz, CircumrealityQueue()) || !_wcsicmp(psz, CircumrealityDelay()) ) {
      BOOL fRet = FALSE;

      PCMMLNode2 pSub;
      DWORD i;
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);

         if (pSub)
            fRet |= RequestRender (pSub, dwPriority, iDirection, fNotInList);
      } // i

      return fRet;
   }

   // if it's a Pre-Render then dissect individual bits
   if (!_wcsicmp(psz, CircumrealityPreRender()) ) {
      // if this is low-power mode then ignore pre-render messages
      if (gpMainWindow->m_fPowerSaver)
         goto done;

      // get the direction
      int iDirection = MMLValueGetInt (pNode, L"Direction", -1);

      BOOL fRet = FALSE;

      PCMMLNode2 pSub;
      DWORD i;
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);

         if (pSub)
            fRet |= RequestRender (pSub, RTQUEUE_PRERENDER, iDirection, fNotInList);
      } // i

      return fRet;
   }

   // if it's an object display then dissect and pull out right bits
   if (!_wcsicmp(psz, CircumrealityObjectDisplay())) {
      BOOL fRet = FALSE;

      PCMMLNode2 pSub;
      DWORD i;
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);
         if (!pSub)
            continue;
         psz = pSub->NameGet();
         if (!psz)
            continue;

         if (!_wcsicmp(psz, CircumrealityImage()) || !_wcsicmp(psz, Circumreality3DScene()) ||
            !_wcsicmp(psz, Circumreality3DObjects()) || !_wcsicmp(psz, CircumrealityTitle()))

            fRet |= RequestRender (pSub, dwPriority, iDirection, fNotInList);
      } // i

      return fRet;
   }

   // BUGFIX: if it's a display window then pull out <ObjectDisplays>
   if (!_wcsicmp(psz, CircumrealityDisplayWindow())) {
      BOOL fRet = FALSE;

      PCMMLNode2 pSub;
      DWORD i;
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);
         if (!pSub)
            continue;
         psz = pSub->NameGet();
         if (!psz)
            continue;

         // if find an object display then pass that on
         if (!_wcsicmp(psz, CircumrealityObjectDisplay()))
            fRet |= RequestRender (pSub, dwPriority, iDirection, fNotInList);
      } // i

      return fRet;
   }

   // if it's an iconwindow, then deconstruct and pull out <ObjectDisplay>s
   if (!_wcsicmp(psz, CircumrealityIconWindow())) {
      BOOL fRet = FALSE;

      PCMMLNode2 pSub;
      DWORD i, j;
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);
         if (!pSub)
            continue;
         psz = pSub->NameGet();
         if (!psz)
            continue;

         if (!_wcsicmp(psz, L"Group")) {
            PCMMLNode2 pSub2;
            for (j = 0; j < pSub->ContentNum(); j++) {
               pSub2 = NULL;
               pSub->ContentEnum (j, &psz, &pSub2);
               if (!pSub2)
                  continue;
               psz = pSub2->NameGet();
               if (!psz)
                  continue;

               // if find an object display then pass that on
               if (!_wcsicmp(psz, CircumrealityObjectDisplay()))
                  fRet |= RequestRender (pSub2, dwPriority, iDirection, fNotInList);
            } // j
         } // if group
      } // i

      return fRet;
   }

   // only care about rendering certain types
   if (_wcsicmp(psz, CircumrealityImage()) && _wcsicmp(psz, Circumreality3DScene()) &&
      _wcsicmp(psz, Circumreality3DObjects()) && _wcsicmp(psz, CircumrealityTitle()))
      goto done;

   // get the string for this
   if (!MMLToMem (pNode, &MemOrig))
      goto done;
   MemOrig.CharCat (0);   // so null terminated

   fInCritical = TRUE;
   EnterCriticalSection (&m_CritSec);

   // queueu location to start and end
   DWORD dwQueueStart = 0, dwQueueEnd = RTQUEUE_PRERENDER;
   if (dwPriority == RTQUEUE_PRERENDER) {
      dwQueueStart = RTQUEUE_PRERENDER;
      dwQueueEnd = RTQUEUE_PRERENDER+2;   // BUGFIX - Since two pre-renders
   }
   // BUGFIX - Potentially add all of the visemes to the queue to
   DWORD i, j;
   DWORD dwViseme;
   DWORD dwQuality;
   DWORD dwMaxQuality = NUMIMAGECACHE;
   if ((m_iResolution == m_iResolutionLow) && (m_dwShadowsFlags == m_dwShadowsFlagsLow) && (m_fLipSync == m_fLipSyncLow))
      dwMaxQuality = 1; // no difference so only one render
   for (dwQuality = 0; dwQuality < dwMaxQuality; dwQuality++) {
      DWORD dwPriorityPlusQuality = dwPriority + dwQuality;

      DWORD dwVisemes = ( (dwQuality ? m_fLipSync : m_fLipSyncLow) && VisemeModifyMML(0, (PWSTR)MemOrig.p, NULL)) ? NUMVISEMES : 1;
         // NOTE: Testing for lip sync on
      for (dwViseme = 0; dwViseme < dwVisemes; dwViseme++) {
         // make a copy for the viseme MML
         PCMem pMemViseme = new CMem;
         if (!pMemViseme)
            continue;
         if (!VisemeModifyMML (dwViseme, (PWSTR)MemOrig.p, pMemViseme)) {
            MemZero (pMemViseme);
            MemCat (pMemViseme, (PWSTR)MemOrig.p);
         }

         // see if this is already in the queue
         PRTQUEUE prt = NULL;
         for (i = dwQueueStart + dwQuality; i < dwQueueEnd; i += 2) {
            prt = (PRTQUEUE)m_alRTQUEUE[i].Get(0);
            for (j = 0; j < m_alRTQUEUE[i].Num(); j++, prt++) {
               // BUGFIX - Make sure to test that prt->pMem is valid
               if (!prt || !prt->pMem || !pMemViseme || (prt->pMem->m_dwCurPosn != pMemViseme->m_dwCurPosn))
                  continue;   // not same length

               if (!wcscmp((PWSTR)prt->pMem->p, (PWSTR)pMemViseme->p))
                  break;
            } // j
            if (j < m_alRTQUEUE[i].Num())
               break;
         } // i
         if (i < dwQueueEnd) {
            // found already...
            delete pMemViseme;   // since no longer need
            pMemViseme = NULL;

            fRet = TRUE;

            // if it's a prerender then move it to the top of the list
            if (dwPriority == RTQUEUE_PRERENDER) {
               RTQUEUE rq = *prt;
               rq.iDirection = iDirection;
               rq.dwTime = GetTickCount();
               m_alRTQUEUE[i].Remove (j); // remove
               m_alRTQUEUE[i].Insert (0, &rq); // insert to the top of the queue
               continue;
            }

            if (i <= dwPriorityPlusQuality)
               continue;  // already exists, and is of right ranking

            // else, may need to re-rank
            m_alRTQUEUE[dwPriorityPlusQuality].Add (prt);  // since prt left pointing to it
            m_alRTQUEUE[i].Remove (j); // remove from old list
            continue;
         }

         // see if it's already in the cache... if it is then no point redrawing
         // BUGFIX - If there's a quality higher, use that too
         DWORD dwQualityTest;
         for (dwQualityTest = NUMIMAGECACHE-1; (dwQualityTest < NUMIMAGECACHE) && (dwQualityTest >= dwQuality); dwQualityTest--) {
            if (gpMainWindow->m_amfImages[dwQualityTest].Exists ((PWSTR)pMemViseme->p)) {
               // send this off to the main thread so it will try to load it
               PostMessage (m_hWndNotify, m_dwMessage, RENDERTHREAD_PRELOAD, (LPARAM)pMemViseme);
               // delete pMemViseme;
               pMemViseme = NULL;
               break;
            }
         }
         if (!pMemViseme)
            continue;

#ifdef _DEBUG
         if (fNotInList)
            fNotInList = 2;
#endif
         // else, it's not there
         RTQUEUE rt;
         memset (&rt, 0, sizeof(rt));
         rt.pMem = pMemViseme;
         rt.iDirection = iDirection;
         rt.dwTime = GetTickCount();
         if (dwVisemes == 1)
            rt.pNode = pNode->Clone();
         else
            rt.pNode = CircumrealityParseMML ((PWSTR)pMemViseme->p);
         rt.fIs360 = IsNode360 (rt.pNode);
         if (rt.fIs360 && (dwPriorityPlusQuality < RTQUEUE_PRERENDER) ) {
            // NOTE: Already in the critical section

            DWORD dwHighQuality = (dwPriorityPlusQuality & RTQUEUE_HIGHQUALITYBIT);
            // if rendering 360, and rendering the same quality, then see if memory match
            DWORD dwRS;
            BOOL fFound = FALSE;
            for (dwRS = 0; dwRS < m_dwRenderShards; dwRS++) {
               // see if find rendering 360
               if (!m_afRendering360[dwRS])
                  continue;

               // make sure same quality
               if ((!m_adwRenderingHighQuality[dwRS]) != (!dwHighQuality))
                  continue;

               // check that memory matches
               if (!m_artCurrent[dwRS].pMem || !m_artCurrent[dwRS].pMem->p || _wcsicmp((PWSTR)m_artCurrent[dwRS].pMem->p, (PWSTR) rt.pMem->p))
                  continue;

               // if get here, already rendering this

               // if pre-rendering, then make a full render
               if (m_afRenderingPreRender[dwRS]) {
                  // set flag to false
                  m_afRenderingPreRender[dwRS] = FALSE;

#if 0 // no longer used
                  // up the number in the queue since wasn't included before
                  if (dwHighQuality)
                     m_dw360InQueueQuality++;
                  else
                     m_dw360InQueue++;
#endif // 0

                  Calc360InQueue ();
               }

               // done
               delete pMemViseme;
               fFound = TRUE;
               break;
            } // dwRS
            if (fFound)
               continue;

#if 0 // old code
            if (m_afRendering360[dwRenderShard]) {
               if (!m_dwRenderingHighQuality == !dwHighQuality) {
                  // make sure memory match
                  if (m_artCurrent[dwRenderShard].pMem && m_artCurrent[dwRenderShard].pMem->p && rt.pMem && rt.pMem->p && !_wcsicmp((PWSTR)m_artCurrent[dwRenderShard].pMem->p, (PWSTR) rt.pMem->p)) {
                     // if get here, already rendering this

                     // if pre-rendering, then make a full render
                     if (m_fRenderingPreRender) {
                        // set flag to false
                        m_fRenderingPreRender = FALSE;

#if 0 // no longer used
                        // up the number in the queue since wasn't included before
                        if (dwHighQuality)
                           m_dw360InQueueQuality++;
                        else
                           m_dw360InQueue++;
#endif // 0

                        Calc360InQueue ();
                     }

                     // done
                     delete pMemViseme;
                     continue;
                  }
               } // quality test
            } // prerendering 360 test
#endif // 0

#if 0 // no longer used
            // else, add
            if (dwHighQuality)
               m_dw360InQueueQuality++;
            else
               m_dw360InQueue++;
#endif // 0
         }
         if (!rt.pNode) {
            delete pMemViseme;
            continue;
         }
         if (dwPriorityPlusQuality >= RTQUEUE_PRERENDER) {
            // pre-rendered are inserted at the top of the list so the most recent ones are done first
            m_alRTQUEUE[dwPriorityPlusQuality].Insert (0, &rt);

            // if too many in the pre-render queue then delete some
            while (m_alRTQUEUE[dwPriorityPlusQuality].Num() > 100) {
               prt = (PRTQUEUE)m_alRTQUEUE[dwPriorityPlusQuality].Get(m_alRTQUEUE[dwPriorityPlusQuality].Num()-1);
               if (prt->pNode)
                  delete prt->pNode;
               if (prt->pMem)
                  delete prt->pMem;
               prt->pNode = NULL;
               prt->pMem = NULL;
               m_alRTQUEUE[dwPriorityPlusQuality].Remove(m_alRTQUEUE[dwPriorityPlusQuality].Num()-1);
            } // while queue
         }
         else
            m_alRTQUEUE[dwPriorityPlusQuality].Add (&rt);

         Calc360InQueue();

         fRet = TRUE;
      } // dwVisemes
   } // dwQuality


done:
   if (fInCritical)
      LeaveCriticalSection (&m_CritSec);
   return fRet;
}


/*************************************************************************************
CRenderThread::PostImageRenderedAsThread - Has a thread do this.

inputs
   PCImageStore         pis - Image. This will be taken over my the main thread.
   BOOL                 fSave - If TRUE then save this image to the database too
   BOOL                 fPostToMain - If TRUE then post to main window. If FALSE
                        then don't post and delete right here
   DWORD                dwQuality - Quality of the image, 0 for low quick-render, 1 for high slow-quality
   DWORD                dwFinalQualityImage - Set to TRUE if this is as good as the image will get
returns
   none
*/
void CRenderThread::PostImageRenderedAsThread (PCImageStore pis, BOOL fSave, BOOL fPostToMain, DWORD dwQuality,
                                               DWORD dwFinalQualityImage)
{
   POSTIMAGERENDERED pir;
   memset (&pir, 0, sizeof(pir));

   pir.pis = pis;
   pir.fSave = fSave;
   pir.fPostToMain = fPostToMain;
   pir.dwQuality = dwQuality;
   _ASSERTE (dwQuality < NUMIMAGECACHE);
   pir.dwFinalQualityImage = dwFinalQualityImage;

   EnterCriticalSection (&m_CritSecSave);
   m_lPOSTIMAGERENDERED.Add (&pir);
   LeaveCriticalSection (&m_CritSecSave);
   SetEvent (m_hSignalToThreadSave);
}

/*************************************************************************************
CRenderThread::PostImageRendered - Call this to pass notification to the
main thread that an image has been rendered.

inputs
   PCImageStore         pis - Image. This will be taken over my the main thread.
   BOOL                 fSave - If TRUE then save this image to the database too
   BOOL                 fPostToMain - If TRUE then post to main window. If FALSE
                        then don't post and delete right here
   DWORD                dwQuality - Quality of the image, 0 for low quick-render, 1 for high slow-quality
   BOOL                 fDontPostIfFileExists - If TRUE and the image already exists in a file,
                           then DON'T post.
returns
   none
*/
void CRenderThread::PostImageRendered (PCImageStore pis, BOOL fSave, BOOL fPostToMain, DWORD dwQuality,
                                       BOOL fDontPostIfFileExists)
{
   CImageStore isClone;
   BOOL fPostingThis = m_hWndNotify && fPostToMain;
   BOOL fSavingThis = fSave && gpMainWindow;

   PCImageStore pisPost = NULL, pisSave = NULL;
   BOOL fDeletePIS = TRUE;
   if (fPostingThis && fSavingThis) {
      // make a clone
      pis->CloneTo (&isClone);
      pisPost = pis;
      pisSave = &isClone;
      fDeletePIS = FALSE;
   }
   else if (fPostingThis) {
      pisPost = pis;
      fDeletePIS = FALSE;
   }
   else if (fSavingThis) {
      pisSave = pis;
   }
   else {   // not posting and not saving
      // do nothing
   }

   CMem mem;
   PWSTR pszFile;
   // get the string for this
   if (MMLToMem (pis->MMLGet(), &mem)) {
      mem.CharCat (0);   // so null terminated
      pszFile = (PWSTR)mem.p;
   }
   else
      pszFile = NULL;

   // see if this exists, including better-quality versions
   DWORD dwQualityTest;
   BOOL fImageExists = FALSE;
   if (fDontPostIfFileExists && pszFile) {
      for (dwQualityTest = NUMIMAGECACHE-1; (dwQualityTest < NUMIMAGECACHE) && (dwQualityTest >= dwQuality); dwQualityTest--)
         if (gpMainWindow->m_amfImages[dwQualityTest].Exists (pszFile)) {
            fImageExists = TRUE;
            break;
         }
   }
   else
      fImageExists = FALSE;


   if (fPostingThis && pisPost) {
      // store transient away
      pisPost->m_fTransient = !fSave;

      // don't post if file already exists, since may have loaded in

      if (!fImageExists)
         PostMessage (m_hWndNotify, m_dwMessage, dwQuality ? RENDERTHREAD_RENDEREDHIGH : RENDERTHREAD_RENDERED, (LPARAM)pisPost);
      else
         // dont wantt post
         delete pisPost;
   }
   
   // BUGFIX - Move save until after post so that post is gotten to faster
   if (fSavingThis && pisSave) {
      if (pszFile) {
         if (!fImageExists)
            pisSave->ToMegaFile (&gpMainWindow->m_amfImages[dwQuality], pszFile);
#ifdef _DEBUG // to test fo breakpoint
         else {
            pszFile = 0;
         }
#endif
      }
   }

   if (fDeletePIS)
      delete pis;
}


/*************************************************************************************
CRenderThread::FindCandidateToDownload - Finds a candidate that might be downloaded
from the server instead of being rendered.

If the candidate is found, it is marked with RTQUEUE.fDownloadRequested, and not used again

inputs
   PCMem       pMemString - Where the string for the file is copied to
   BOOL        *pfExpectHighQuality - If filled with TRUE then should look in the high-quality
                  image save, FALSE then low-quality, to verify the file
returns
   BOOL - TRUE if success
*/
BOOL CRenderThread::FindCanddiateToDownload (PCMem pMemString, BOOL *pfExpectHighQuality)
{
   pMemString->m_dwCurPosn = 0;  // since testing this later on

   // if hasn't paid, or feature turned off, then return FALSE
   if (!gfRenderCache || !RegisterMode())
      return FALSE;

   EnterCriticalSection (&m_CritSec);

   if ((m_iResolution == m_iResolutionLow) && (m_dwShadowsFlags == m_dwShadowsFlagsLow) && (m_fLipSync == m_fLipSyncLow))
      *pfExpectHighQuality = FALSE;
   else
      *pfExpectHighQuality = TRUE;

   PRTQUEUE prt = NULL;
   // RTQUEUE rt;
   // BUGFIX - Render in a slightly different order, doing all low quality images EXCEPT
   // the pre-render first

   DWORD i, j;
   // low-quality, non prerender
   for (i = 0; i < RTQUEUE_PRERENDER; i++) {
      if (i & RTQUEUE_HIGHQUALITYBIT)
         continue;

      prt = (PRTQUEUE)m_alRTQUEUE[i].Get(0);
      for (j = 0; j < m_alRTQUEUE[i].Num(); j++, prt++) {
         if (prt->fDownloadRequested)
            continue;

         // else, found
         prt->fDownloadRequested = TRUE;
         MemZero (pMemString);
         MemCat (pMemString, (PWSTR)prt->pMem->p);
         goto done;
      } // j
   } // i

   // all the rest
   for (i = 0; i < RTQUEUENUM; i++) {
      if ((i < RTQUEUE_PRERENDER) && !(i & RTQUEUE_HIGHQUALITYBIT))
         continue;

      prt = (PRTQUEUE)m_alRTQUEUE[i].Get(0);
      for (j = 0; j < m_alRTQUEUE[i].Num(); j++, prt++) {
         if (prt->fDownloadRequested)
            continue;

         // else, found
         prt->fDownloadRequested = TRUE;
         MemZero (pMemString);
         MemCat (pMemString, (PWSTR)prt->pMem->p);
         goto done;
      } // j
   } // i

done:
   LeaveCriticalSection(&m_CritSec);
   return pMemString->m_dwCurPosn ? TRUE : FALSE;
}

/*************************************************************************************
CRenderThread::DoRender - This renders the highest priority image in the queue.

NOTE: Only call from the rendering thread.

returns
   BOOL - TRUE if found something to work with, FALSE if the queue is empty
*/
BOOL CRenderThread::DoRender (DWORD dwRenderShard)
{
#if 0 // def _DEBUG
   CMem memOut;
#endif

   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   // before doing anything, send a download request
   gpMainWindow->SendDownloadRequests();


   // find one
   DWORD i;
   // DWORD dwCopiedTo = (DWORD)-1; // priority queue that was copied to
   BOOL fRemoved = FALSE;  // know if removed from original queue
   PCImageStore pis = NULL;
   BOOL fImageExists = FALSE;

   EnterCriticalSection (&m_CritSec);
   PRTQUEUE prt = NULL;
   // RTQUEUE rt;
   // BUGFIX - Render in a slightly different order, doing all low quality images EXCEPT
   // the pre-render first

   // low-quality, non prerender
   for (i = 0; i < RTQUEUE_PRERENDER; i++) {
      if (i & RTQUEUE_HIGHQUALITYBIT)
         continue;

      prt = (PRTQUEUE)m_alRTQUEUE[i].Get(0);
      if (m_alRTQUEUE[i].Num())
         goto found;
   } // i

   // all the rest
   for (i = 0; i < RTQUEUENUM; i++) {
      if ((i < RTQUEUE_PRERENDER) && !(i & RTQUEUE_HIGHQUALITYBIT))
         continue;

      prt = (PRTQUEUE)m_alRTQUEUE[i].Get(0);
      if (m_alRTQUEUE[i].Num())
         goto found;
   } // i

   // if get here, nothing
   memset (&m_artCurrent[dwRenderShard], 0, sizeof(m_artCurrent[dwRenderShard]));
   LeaveCriticalSection(&m_CritSec);
   return FALSE;

found:
   DWORD dwPriority = i;
   DWORD dwQuality = (dwPriority & RTQUEUE_HIGHQUALITYBIT) ? (NUMIMAGECACHE-1) : 0;
      // BUGFIX - Was just dwQuality = (dwPriority & RTQUEUE_HIGHQUALITYBIT)
   _ASSERTE (dwQuality < NUMIMAGECACHE);
   DWORD dwFinalQualityImage = dwQuality ? TRUE : FALSE;
   if ((m_iResolution == m_iResolutionLow) && (m_dwShadowsFlags == m_dwShadowsFlagsLow) && (m_fLipSync == m_fLipSyncLow))
      dwFinalQualityImage = TRUE; // no difference so only one render

   // BUGFIX - if this is pre-render then need to find pre-render
   // in direction that facing, that occurred roughly the same time as
   // the pre-render
   if (dwPriority >= RTQUEUE_PRERENDER) {
      // know that prt is the 0'th element
      DWORD dwTime = prt->dwTime;
      DWORD dwTimeMin = (dwTime > PRERENDERTIMERANGE) ? (dwTime - PRERENDERTIMERANGE) : 0;
      DWORD dwTimeMax = (dwTime < ((DWORD)-1 - PRERENDERTIMERANGE)) ? (dwTime + PRERENDERTIMERANGE) : (DWORD)-1;

      // what direction are we looking
      CPoint pLook, pCompare;
      pLook.Zero();
      pCompare.Zero();
      pLook.p[0] = sin(m_f360Long);
      pLook.p[1] = cos(m_f360Long);
      // NOTE: Ingoring up/down in this comparison, but easy enough to do later if need

      // loop through all matches and find the best
      prt = (PRTQUEUE)m_alRTQUEUE[dwPriority].Get(0);
      DWORD dwBestMatch = (DWORD)-1;
      fp fBestDistance = 0;
      for (i = 0; i < m_alRTQUEUE[dwPriority].Num(); i++, prt++) {
         if ((prt->dwTime < dwTimeMin) || (prt->dwTime > dwTimeMax))
            continue;   // out of range timewise

         fp fDistance;
         if ((prt->iDirection >= 0) && (prt->iDirection < 8)) {
            pCompare.p[0] = sin((fp)prt->iDirection / 8.0 * 2.0 * PI);
            pCompare.p[1] = cos((fp)prt->iDirection / 8.0 * 2.0 * PI);
            fDistance = pLook.DotProd (&pCompare); // will be 1.0 if right on, -1.0 if reverse
         }
         else  // up, down, in, out
            fDistance = -2;   // since doing dotprod

         if ((dwBestMatch == (DWORD)-1) || (fDistance > fBestDistance)) {
            // found match
            dwBestMatch = i;
            fBestDistance = fDistance;
         }
      } // over all possible matches

      // insert this
      prt = (PRTQUEUE)m_alRTQUEUE[dwPriority].Get(0); // since use this later
      if (dwBestMatch) {
         m_artCurrent[dwRenderShard] = prt[dwBestMatch];
         m_alRTQUEUE[dwPriority].Remove (dwBestMatch);
         m_alRTQUEUE[dwPriority].Insert (0, &m_artCurrent[dwRenderShard]);
         prt = (PRTQUEUE)m_alRTQUEUE[dwPriority].Get(0); // since use this later
      }

#ifdef _DEBUG
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nPrerendering in direction %d", (int)prt->iDirection);
      OutputDebugStringW (szTemp);
#endif
   } // if prerender

   m_artCurrent[dwRenderShard] = *prt;

#if 0 // not used
   // if this isn't the top priority then move it there, so know that
   // when come back, this will still be a top priotiy item
   if (dwPriority >= RTQUEUE_MEDIUM) {
      if (dwPriority < RTQUEUE_PRERENDER) {
         dwCopiedTo = RTQUEUE_HIGH + dwQuality;
         m_alRTQUEUE[dwCopiedTo].Insert (0, prt);  // so will be elem 0 since top priority is empty
      }
      m_alRTQUEUE[dwPriority].Remove (0);
      fRemoved = TRUE;  // so know that removed from main priority queue
   }
   else
      dwCopiedTo = dwPriority;   // so know to delete this priority
#endif

   // remove from the current queue
   m_alRTQUEUE[dwPriority].Remove (0);
   fRemoved = TRUE;  // so know that removed from main priority queue

   // if already exists then done
   // BUGFIX - If there's a quality higher, use that too
   DWORD dwQualityTest;
   for (dwQualityTest = NUMIMAGECACHE-1; (dwQualityTest < NUMIMAGECACHE) && (dwQualityTest >= dwQuality); dwQualityTest--) {
      if (gpMainWindow->m_amfImages[dwQualityTest].Exists ((PWSTR)m_artCurrent[dwRenderShard].pMem->p)) {
         // post this off to the main thread so it can load it
         PCMem pNew = new CMem;
         if (pNew) {
            MemZero (pNew);
            MemCat (pNew, (PWSTR)m_artCurrent[dwRenderShard].pMem->p);
            PostMessage (m_hWndNotify, m_dwMessage, RENDERTHREAD_PRELOAD, (LPARAM)pNew);
         }

         // since in done LeaveCriticalSection (&m_CritSec);
         fImageExists = TRUE;
         goto done;
      }
   }

   // act
   PWSTR psz = m_artCurrent[dwRenderShard].pNode->NameGet();
   if (!psz) {
      fImageExists = TRUE; // so don't re-add
      goto done;
   }

   m_afRendering360[dwRenderShard] = m_artCurrent[dwRenderShard].fIs360;  // so know if rendering a 360 degree mimage

   // see if flag set for not aborting 360 render so that can do
   // <CutScene> properly
   m_afDontAbort360[dwRenderShard] = FALSE;
   if (m_artCurrent[dwRenderShard].pNode && MMLValueGetInt(m_artCurrent[dwRenderShard].pNode, L"DontAbort360", 0))
      m_afDontAbort360[dwRenderShard] = TRUE;

   m_afRenderingPreRender[dwRenderShard] = (dwPriority >= RTQUEUE_PRERENDER);
   m_adwRenderingHighQuality[dwRenderShard] = dwQuality;

   LeaveCriticalSection (&m_CritSec);

   // clear progress stack just in case
   m_alStack[dwRenderShard].Clear();   // BUGFIX - Was using i
   m_afProgress[dwRenderShard] = 0;

#if 0 // def _DEBUG
   MMLToMem (m_artCurrent[dwRenderShard].pNode, &memOut);
   memOut.CharCat (0);   // so null terminated
   OutputDebugStringW (L"\r\n");
   OutputDebugStringW ((PWSTR)memOut.p);
#endif

   // clear the texture cache to make sure dont overflow
   // BUGFIX - base the number of textures based on the current working set
   size_t iMemEsc = EscMemoryAllocated (TRUE);
   //PROCESS_MEMORY_COUNTERS pmk;
   // HANDLE hProcess = GetCurrentProcess();
   //memset (&pmk, 0, sizeof(pmk));
   //pmk.cb = sizeof(pmk);
   //GetProcessMemoryInfo (hProcess, &pmk, sizeof(pmk));
   DWORD dwTime = GetTickCount();
   if ((iMemEsc >= (size_t)m_iPhysicalTotal) || (iMemEsc >= (size_t)m_iPhysicalTotalHalf))
      for (i = 0; i < m_dwRenderShards; i++) {
         m_adwCacheFree[i] = dwTime;
         if (iMemEsc >= (size_t)m_iPhysicalTotal)
            m_afCacheFreeBig[i] = TRUE;
      };
   if (m_adwCacheFree[dwRenderShard]) {
      // only free once every 30 seconds
      if ((m_adwCacheFreeLast[dwRenderShard] < m_adwCacheFree[dwRenderShard] - 30000) || (m_adwCacheFreeLast[dwRenderShard] > m_adwCacheFree[dwRenderShard] + 30000)) {
         // wipe out part
         DWORD dwNum = TextureCacheNum(dwRenderShard) / 2;
            // wipe out half of them
         if (m_afCacheFreeBig[dwRenderShard])
            dwNum = 0;  // if big wipe, then clear everything
      
         TextureCacheClear (dwRenderShard, dwNum);
         
         m_adwCacheFreeLast[dwRenderShard] = dwTime;
      }

      m_adwCacheFree[dwRenderShard] = 0;  // blank this
      m_afCacheFreeBig[dwRenderShard] = FALSE;
   }


   // set the detail level. If it's changed since the last time, this will
   // clear the cache as well as clearing the cache megafile
   TextureDetailSet (dwRenderShard, m_iTextureDetail);


#ifdef _DEBUG
   if (m_afRenderingPreRender[dwRenderShard])
      OutputDebugString ("\r\nRendering PreRender");
   else
      OutputDebugString ("\r\nRendering normal render");

   if (m_adwRenderingHighQuality[dwRenderShard])
      OutputDebugString ("\r\nRendering high quality");
   else
      OutputDebugString ("\r\nRendering low quality");
#endif

   if (!_wcsicmp(psz, CircumrealityImage()))
      pis = DoRenderImage (dwRenderShard, m_artCurrent[dwRenderShard].pNode);
   else if (!_wcsicmp(psz, Circumreality3DScene()))
      pis = DoRenderScene (dwRenderShard, m_artCurrent[dwRenderShard].pNode, TRUE, m_afRenderingPreRender[dwRenderShard], dwQuality);
   else if (!_wcsicmp(psz, CircumrealityTitle()))
      pis = DoRenderTitle (dwRenderShard, m_artCurrent[dwRenderShard].pNode, dwQuality);
   else if (!_wcsicmp(psz, Circumreality3DObjects()))
      pis = DoRenderScene (dwRenderShard, m_artCurrent[dwRenderShard].pNode, FALSE, m_afRenderingPreRender[dwRenderShard], dwQuality);
   else {
      EnterCriticalSection (&m_CritSec);
      fImageExists = TRUE; // so don't readd
      goto done;
   }

   // if had rendered image, go through cache and make sure not too large
   DWORD dwQualityLoop;
   for (dwQualityLoop = 0; dwQualityLoop < NUMIMAGECACHE; dwQualityLoop++)
      gpMainWindow->m_amfImages[dwQualityLoop].LimitSize (MAXIMAGECACHE, 60*24);

   EnterCriticalSection (&m_CritSec);
done:
   // send message to main window so knows that another queue item is done
   if (pis)
      PostImageRenderedAsThread (pis, TRUE, !m_afRenderingPreRender[dwRenderShard], dwQuality, dwFinalQualityImage);
         // BUGFIX - Was (dwPriority < RTQUEUE_PRERENDER) but want to dynamically change
         // this, so just use !m_afRenderingPreRender

   // moved before done: EnterCriticalSection (&m_CritSec);

   // if didn't get a return image and removed from existing queue, then add back in
   // AND, slightly random chance of dropping so don't get in infinite loop
   // BUGFIX - Only add back if it was a pre-render
   // BUGFIX - Allow to add back if NOT pre-rendering
   BOOL fReAdded = FALSE;
   if (!pis && !fImageExists && /* (dwPriority >= RTQUEUE_PRERENDER) && */ fRemoved && (m_artCurrent[dwRenderShard].dwTimesFailed < 1000)) {
      m_artCurrent[dwRenderShard].dwTimesFailed++;

      // add back
      if (fRemoved) {
         // BUGFIX - Add to queue in time-sorted order. Lastest time at the top
         PCListFixed plQueue = (dwPriority < RTQUEUE_PRERENDER) ? &m_alRTQUEUE[RTQUEUE_PRERENDER + dwQuality] : &m_alRTQUEUE[dwPriority];
            // if not pre-rendering, add onto prerender so don't hog others
            // if prerendering, put back in list
         PRTQUEUE prt = (PRTQUEUE) plQueue->Get(0);

         for (i = 0; i < plQueue->Num(); i++, prt++)
            if (m_artCurrent[dwRenderShard].dwTime >= prt->dwTime)
               break;

         plQueue->Insert (i, &m_artCurrent[dwRenderShard]);
      }
#if 0 // not used
      else if (dwCopiedTo != (DWORD)-1)
         // make sure that don't delete
         dwCopiedTo = (DWORD)-1;
#endif // 0

      fReAdded = TRUE;
   }
   else {
      // delete this item
      delete m_artCurrent[dwRenderShard].pNode;
      delete m_artCurrent[dwRenderShard].pMem;
      m_artCurrent[dwRenderShard].pNode = NULL;
      m_artCurrent[dwRenderShard].pMem = NULL;
   }

#if 0 // no longer used
   if (!m_fRenderingPreRender) {  // BUGFIX - Was testing dwPriority < RTQUEUE_PRERENDER, but need to change to !m_fRenderingPreRender
      if (m_artCurrent[dwREnderShard].fIs360 && !fReAdded) {
         if (dwQuality) {
            // BUGFIX - Dont do because will be hit even if OK _ASSERTE (m_dw360InQueueQuality > 0);
            if (m_dw360InQueueQuality) // BUGFIX - So don't go below 0
               m_dw360InQueueQuality--;
         }
         else {
            // BUGFIX - Dont do because will be hit even if OK _ASSERTE (m_dw360InQueue > 0);
            if (m_dw360InQueue) // BUGFIX - So don't go below 0
               m_dw360InQueue--;
         }
      }

#if 0 // not used
      if (dwCopiedTo != (DWORD)-1)
         m_alRTQUEUE[dwCopiedTo].Remove (0); // since know it's at 0
#endif // 0
   }
#endif // 0

   Calc360InQueue();

   m_afRendering360[dwRenderShard] = FALSE;   // clear the note about rendering 360
   m_afRenderingPreRender[dwRenderShard] = FALSE;
   m_adwRenderingHighQuality[dwRenderShard] = 0;

   // will need to post progress bar to indicate that done, unless
   // there are any more elements on the queue, in which case don't bother
   for (i = 0; i < RTQUEUENUM; i++)
      if (m_alRTQUEUE[i].Num())
         break;
   if (i >= RTQUEUENUM)
      m_aRTP[dwRenderShard].Update (0); // set progress to 0 so goes back to nothing

   memset (&m_artCurrent[dwRenderShard], 0, sizeof(m_artCurrent[dwRenderShard]));
   LeaveCriticalSection (&m_CritSec);

   // done
   return TRUE;
}

/*************************************************************************************
ResolutionToRenderScale - Converts a resoltion number to a scale, that can
be passed into RenderSceneAspectToPixelsInt ().

inputs
   int         iResolution - from ResolutionQualityToRenderSettings()
returns
   fp - Scale, passed into RenderSceneAspectToPixelsInt()
*/
fp ResolutionToRenderScale (int iResolution)
{
   return pow (2.0, (fp)iResolution / 2.0);
}

/*************************************************************************************
CRenderThread::DoRenderScene - This renders an <image> queue.

inputs
   PCMMLNode2         pNode - Node that describes it
   BOOL              fScene - If TRUE it's a scene, FALSE it's a set of objects
   BOOL              fPreRender - If TRUE, this is being rendered for pre-render,
                        so dont update as much, etc.
   DWORD             dwQuality - Set to 1 for high-quality image, 0 for low-quality quick image
returns
   PCImageStore - Image store with the image, or NULL if failed
*/
PCImageStore CRenderThread::DoRenderScene (DWORD dwRenderShard, PCMMLNode2 pNode, BOOL fScene, BOOL fPreRender, DWORD dwQuality)
{
#if 0 // def _DEBUG
   CMem memOut;
   MMLToMem (pNode, &memOut);
   memOut.CharCat (0);   // so null terminated
   OutputDebugStringW (L"\r\n");
   OutputDebugStringW ((PWSTR)memOut.p);
#endif

   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   // get the scene from MML
   CRenderScene rs;
   PCImageStore pIS;
   rs.m_fLoadFromFile = fScene;
   PCMMLNode2 pNodeTemp = pNode->Clone(); // BUGFIX - pass in clone since MMLFrom seems to distort some numbers
   if (!rs.MMLFrom(pNodeTemp)) {
      delete pNodeTemp;
      return NULL;
   }
   delete pNodeTemp;

#if 0 // def _DEBUG
   memOut.m_dwCurPosn = 0;
   MMLToMem (pNode, &memOut);
   memOut.CharCat (0);   // so null terminated
   OutputDebugStringW (L"\r\n");
   OutputDebugStringW ((PWSTR)memOut.p);
#endif

   // draw it
   BOOL f360;
   fp fImageRes = ResolutionToRenderScale (dwQuality ? m_iResolution : m_iResolutionLow);
   m_apRS360Node[dwRenderShard] = pNode;
   DWORD dwShadowFlags = (dwQuality ? m_dwShadowsFlags : m_dwShadowsFlagsLow);
   if (fPreRender &&  (dwShadowFlags & SF_TWOPASS360))
      dwShadowFlags &= ~SF_TWOPASS360;
   pIS = (PCImageStore) rs.Render (dwRenderShard, fImageRes, TRUE, TRUE, FALSE, dwShadowFlags,
      &m_aRTP[dwRenderShard], TRUE, &gpMainWindow->m_amfImages[dwQuality],
      &f360, dwQuality ? NULL : &m_aRTP[dwRenderShard], &m_amem360Calc[dwRenderShard]);
         // NOTE: For quality image, not intermittent updates
         // BUGFIX : Was using (fPreRender || dwQuality) ? ..., but only dwQuality
         // since now capaturing the callbacks and checking m_fRenderingPreRender
   m_apRS360Node[dwRenderShard] = NULL;
   if (!pIS)
      return NULL;  // error

   // initialize image store
   if (f360)
      pIS->m_dwStretch = 4;

   // set the node
   pIS->MMLSet (pNode);

   return pIS;
}



/*************************************************************************************
CRenderThread::DoRenderTitle - This renders an <title> queue.

inputs
   PCMMLNode2         pNode - Node that describes it
   DWORD             dwQuality - Set to 1 for high-quality image, 0 for low-quality quick image
returns
   PCImageStore - Image that was renedered, or NULL
*/
PCImageStore CRenderThread::DoRenderTitle (DWORD dwRenderShard, PCMMLNode2 pNode, DWORD dwQuality)
{
   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   // get the title from MML
   CRenderTitle rs;
   PCImageStore pis;
   PCMMLNode2 pNodeTemp = pNode->Clone(); // BUGFIX - pass in clone since MMLFrom seems to distort some numbers
   if (!rs.MMLFrom(pNodeTemp)) {
      delete pNodeTemp;
      return NULL;
   }
   delete pNodeTemp;

   // draw it
   BOOL f360;
   DWORD dwShadowsFlags = (dwQuality ? m_dwShadowsFlags : m_dwShadowsFlagsLow);
   fp fImageRes = ResolutionToRenderScale (dwQuality ? m_iResolution : m_iResolutionLow);
   PCImage pImage = rs.Render (dwRenderShard, fImageRes, TRUE, TRUE, FALSE, dwShadowsFlags,
      &m_aRTP[dwRenderShard], &gpMainWindow->m_amfImages[dwQuality],
      &f360, &m_amem360Calc[dwRenderShard]);
   if (!pImage)
      return FALSE;  // error

   // initialize image store
   pis = new CImageStore;
   if (!pis) {
      delete pImage;
      return NULL;
   }
   BOOL fRet = pis->Init (pImage, TRUE, !(dwShadowsFlags & SF_NOSUPERSAMPLE), f360);
   delete pImage;
   if (!fRet) {
      delete pis;
      return NULL;
   }
   if (f360)
      pis->m_dwStretch = 4;

   // set the node
   pis->MMLSet (pNode);

   return pis;
}


/*************************************************************************************
CRenderThread::DoRenderImage - This renders an <image> queue.

inputs
   PCMMLNode2         pNode - Node that describes it
returns
   PCImageStore - Image that's loaded
*/
PCImageStore CRenderThread::DoRenderImage (DWORD dwRenderShard, PCMMLNode2 pNode)
{
   // will need to update progress bar
   m_aRTP[dwRenderShard].Update (0.5);  // since cant be fined grained about this

   // NOTE: The <image/> tag accepts MMLValueSet() "File"=filename, and
   // "scale"="stretchtofit", "none", "scaletofit", "scaletocover"
   PCImageStore pis;
   PWSTR psz;
   // figure out stretch
   DWORD dwStretch = 2;
   psz = MMLValueGet (pNode, L"Scale");
   if (psz && !_wcsicmp(psz, L"stretchtofit"))
      dwStretch = 1;
   else if (psz && !_wcsicmp(psz, L"none"))
      dwStretch = 0;
   else if (psz && !_wcsicmp(psz, L"scaletocover"))
      dwStretch = 3;
   
   psz = MMLValueGet (pNode, L"File");
   if (!psz)
      return NULL;

   // try to open this...
   char szFile[512];
   HBITMAP hBit;
   if (wcslen(psz) > 250)
      return NULL; // too long
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szFile, sizeof(szFile), 0, 0);
   hBit = JPegOrBitmapLoad (szFile, FALSE);
   if (!hBit)
      return NULL;

   pis = new CImageStore;
   if (!pis) {
      DeleteObject (hBit);
      return NULL;
   }
   if (!pis->Init (hBit, TRUE)) {
      delete pis;
      DeleteObject (hBit);
      return FALSE;
   }
   DeleteObject (hBit);

   // set the node
   pis->MMLSet (pNode);
   pis->m_dwStretch = dwStretch;

   return pis;
}


/******************************************************************************
CRenderThread::Push - Pushes a new minimum and maximum for the progress meter
so that any code following will think min and max are going from 0..1, but it
might really be going from .2 to .6 (if that's what fMin and fMax) are.

inputs
   fp      fMin,fMax - New min and max, from 0..1. (Afected by previous min and
                  max already on stack.
reutrns
   BOOL - TRUE if success
*/
BOOL CRenderThread::MyPush (DWORD dwRenderShard, float fMin, float fMax)
{
   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   PTEXTUREPOINT ptp = m_alStack[dwRenderShard].Num() ? (PTEXTUREPOINT) (m_alStack[dwRenderShard].Get(m_alStack[dwRenderShard].Num()-1)) : NULL;

   if (ptp) {
      fMin = fMin * (ptp->v - ptp->h) + ptp->h;
      fMax = fMax * (ptp->v - ptp->h) + ptp->h;
   }

   TEXTUREPOINT tp;
   tp.h = fMin;
   tp.v = fMax;
   m_alStack[dwRenderShard].Add (&tp);

   return TRUE;
}

/******************************************************************************
CRenderThread::Pop - Removes the last changes for percent complete off the stack -
as added by Push()
*/
BOOL CRenderThread::MyPop (DWORD dwRenderShard)
{
   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   if (!m_alStack[dwRenderShard].Num())
      return FALSE;
   m_alStack[dwRenderShard].Remove (m_alStack[dwRenderShard].Num()-1);
   return TRUE;
}


/******************************************************************************
CRenderThread::Update - Updates the progress bar. Should be called once in awhile
by the code.

inputs
   fp         fProgress - Value from 0..1 for the progress from 0% to 100%
returns
   int - 1 if in the future give progress indications more often (which means they
         were too far apart), 0 for the same amount, -1 for less often
*/
int  CRenderThread::MyUpdate (DWORD dwRenderShard, float fProgress)
{
   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   // get the current time
   DWORD dwTime;
   int   iRet; // what should return
   dwTime = GetTickCount();
   if (dwTime - m_adwLastUpdateTime[dwRenderShard] > 1000)
      iRet = 1;
   else if (dwTime - m_adwLastUpdateTime[dwRenderShard] < 250)
      iRet = -1;
   else
      iRet = 0;
   m_adwLastUpdateTime[dwRenderShard] = dwTime;

   // progress
   if (m_alStack[dwRenderShard].Num()) {
      PTEXTUREPOINT ptp = (PTEXTUREPOINT) m_alStack[dwRenderShard].Get(m_alStack[dwRenderShard].Num()-1);
      fProgress = fProgress * (ptp->v - ptp->h) + ptp->h;
   }

   // remember this
   m_afProgress[dwRenderShard] = fProgress;

   // send message
   DWORD i;
   if (m_hWndNotify) {
      // NOTE: Not worrying about being thread safe here
      fp fMax = 0.0;
      for (i = 0; i < m_dwRenderShards; i++)
         fMax = max(fMax, m_afProgress[dwRenderShard]);

      PostMessage (m_hWndNotify, m_dwMessage, RENDERTHREAD_PROGRESS, (LPARAM)(fMax * 1000000));
   }

   return iRet;
}


/******************************************************************************
CRenderThread::WantToCancel - Returns TRUE if the user has pressed cancel and the
operation should exit.
*/
BOOL CRenderThread::MyWantToCancel (DWORD dwRenderShard)
{
   _ASSERTE (dwRenderShard <= MAXRENDERSHARDS);

   // if want to quit (in general), then TRUE
   if (m_afWantToQuit[dwRenderShard])
      return TRUE;

   // if not rendering 360 then finish the image
   if (!m_afRendering360[dwRenderShard])
      return FALSE;

   // if don't abort on 360 then done
   if (m_afDontAbort360[dwRenderShard])
      return FALSE;

   // if get here, m_fRender360 is TRUE

   // if rending a 360 degree image, and there's another one in the queue already,
   // then just abort this

   // if pre-render and rendering a 360 image then quit if anything comes in
   EnterCriticalSection (&m_CritSec);
   DWORD dwThisPriority = (m_afRenderingPreRender[dwRenderShard] ? 2 : 0) + (m_adwRenderingHighQuality[dwRenderShard] ? 1 : 0);

   // NOTE: Dont really need to worry about thread protection here since just reading
   DWORD i;
   for (i = 0; i < RTQUEUE_PRERENDER; i++) {
      // figure out a priority for this
      DWORD dwTestPriority = ((i >= RTQUEUE_PRERENDER) ? 2 : 0) + (i & RTQUEUE_HIGHQUALITYBIT);
      if (dwTestPriority >= dwThisPriority)
         continue;   // a lower priority in the queue, so ignore

      if (!m_alRTQUEUE[i].Num())
         continue;   // nothing in here, so ignore

      // if get here then found something higher
      LeaveCriticalSection (&m_CritSec);
      return TRUE;
   }

   // break on 360 if more than one 360 queued up, so dont get too many in queue
   if ( (m_adwRenderingHighQuality[dwRenderShard] ? m_dw360InQueueQuality : m_dw360InQueue) > m_dwRenderShards) {
            // BUGFIX - Was >= 2
      LeaveCriticalSection (&m_CritSec);
      return TRUE;
   }
   LeaveCriticalSection (&m_CritSec);


   return FALSE;
}

/******************************************************************************
CRenderThread::CanRedraw - Called by the renderer (or whatever) to tell the caller
that the display can be updated with a new image. This is used by the ray-tracer
to indicate it has finished drawing another bucket.
*/
void CRenderThread::MyCanRedraw (DWORD dwRenderShard)
{
   // do nothing
}


/******************************************************************************
CRenderThread::CanLoad3D - Called by the internet thread when all the deletion
is completed. This allows the 3d libraries to be loaded.
*/
void CRenderThread::CanLoad3D (void)
{
   EnterCriticalSection (&m_CritSec);
   DWORD i;
   for (i = 0; i < m_dwRenderShards; i++)
      m_afM3DCanStart[i] = TRUE;
   LeaveCriticalSection (&m_CritSec);
}


/******************************************************************************
CRenderThread::ResolutionSet - Sets a new resolution to use.

inputs
   int         iResolution - New resolution.
   DWORD       dwShadowsFlags - Flags to pass into shadows mode
   int         iTextureDetail - Texture detail to use
   BOOL        fLipSync - If TRUE then lip sync is enabled
   int         iResolutionLow - For low-quality quick render
   DWORD       dwShadowsFlagsLow - For low-quality quick render
   BOOL        fLipSyncLow - For low-quality quick render
   BOOL        fClear - If TRUE then clear any images in the cache when set
returns
   none
*/
void CRenderThread::ResolutionSet (int iResolution, DWORD dwShadowsFlags, int iTextureDetail, BOOL fLipSync,
                                   int iResolutionLow, DWORD dwShadowsFlagsLow, BOOL fLipSyncLow, BOOL fClear)
{
   // see about registration
   if (!RegisterMode()) {
      // didn't pay and not in trial, so limit quality
      iResolution = min (iResolution, -2);
      iTextureDetail = min (iTextureDetail, -2);   // BUGFIX - If not paid then extra low texture detail
      fLipSync = FALSE;
      dwShadowsFlags &= ~(SF_TWOPASS360);
      dwShadowsFlags |= (SF_NOSHADOWS | SF_NOSPECULARITY | SF_LOWTRANSPARENCY | SF_NOBUMP | SF_LOWDETAIL);
      // allow SF_TEXTURESONLY to pass through
   }

   // NOTE: Not really bother about thread safety here because wont have any impact
   if ((iResolution == m_iResolution) && (dwShadowsFlags == m_dwShadowsFlags) && (iTextureDetail == m_iTextureDetail) &&
      (fLipSync == m_fLipSync) &&
      (iResolutionLow == m_iResolutionLow) && (dwShadowsFlagsLow == m_dwShadowsFlagsLow) && (fLipSyncLow == m_fLipSyncLow) )
      return;  // no change

   m_iResolution = iResolution;
   m_iResolutionLow = iResolutionLow;
   m_dwShadowsFlags = dwShadowsFlags;
   m_dwShadowsFlagsLow = dwShadowsFlagsLow;
   m_iTextureDetail = iTextureDetail;
   m_fLipSync = fLipSync;
   m_fLipSyncLow = fLipSyncLow;
   if (fClear) {
      DWORD dwQuality;
      for (dwQuality = 0; dwQuality < NUMIMAGECACHE; dwQuality ++)
         gpMainWindow->m_amfImages[dwQuality].Clear();

      // store the info
      RESOLUTIONFILEINFO rfi;
      memset (&rfi, 0, sizeof(rfi));
      rfi.dwShadowsFlags = m_dwShadowsFlags;
      rfi.iResolution = m_iResolution;
      rfi.iTextureDetail = m_iTextureDetail;
      // NOTE: Not bothering to save away m_iResolutionLow and m_dwShadowsFlagsLow

      gpMainWindow->m_amfImages[0].Save (gpszResolutionFileInfo, &rfi, sizeof(rfi));
   }
}


/******************************************************************************
CRenderThread::ResolutionCheck - Checks to make sure that the resolution stored
in the file is the same as the requested resolution. If not, then
the file is cleared.
*/
void CRenderThread::ResolutionCheck (void)
{
   PRESOLUTIONFILEINFO prfi;
   __int64 iSize;
   prfi = (PRESOLUTIONFILEINFO) gpMainWindow->m_amfImages[0].Load (gpszResolutionFileInfo, &iSize);
   BOOL fTheSame = FALSE;
   if (prfi) {
      if ( (iSize == sizeof(*prfi)) && (prfi->dwShadowsFlags == m_dwShadowsFlags) &&
         (prfi->iResolution == m_iResolution) && (prfi->iTextureDetail == m_iTextureDetail))
         fTheSame = TRUE;
      // NOTE: Not bothing to check m_iResolutionLow or m_iShadowsFlagsLow
      MegaFileFree (prfi);
   }

   // if the same then do nothing
   if (fTheSame)
      return;

   // else, clear and save
   DWORD dwQuality;
   for (dwQuality = 0; dwQuality < NUMIMAGECACHE; dwQuality++)
      gpMainWindow->m_amfImages[dwQuality].Clear ();

   // store the info
   RESOLUTIONFILEINFO rfi;
   memset (&rfi, 0, sizeof(rfi));
   rfi.dwShadowsFlags = m_dwShadowsFlags;
   rfi.iResolution = m_iResolution;
   rfi.iTextureDetail = m_iTextureDetail;
   // NOTE: Not bothering to save m_iResolutionLow or m_dwShadowsFlagsLow

   gpMainWindow->m_amfImages[0].Save (gpszResolutionFileInfo, &rfi, sizeof(rfi));
}


/******************************************************************************
CRenderThread::Vis360Changed - Sets a new latitude and longitude to be used
as an optimization in case doing 360 degree renders.

inputs
   fp          fLong - Longitude, in radians
   fp          fLat - Latitude, in radians
returns
   none
*/
void CRenderThread::Vis360Changed (fp fLong, fp fLat)
{
   // NOTE: Not really bother about thread safety here because wont have any impact
   m_f360Lat = fLat;
   m_f360Long = fLong;
}



/******************************************************************************
CRenderThread::RS360LongLatGet - Called to get the current longitude and latitude)

inputs
   fp       *pfLong - To be filled with longitude in radians
   fp       *pfLat - To be filled with latitude in radians
*/
void CRenderThread::MyRS360LongLatGet (DWORD dwRenderShard, fp *pfLong, fp *pfLat)
{
   *pfLong = m_f360Long;
   *pfLat = m_f360Lat;
}


/******************************************************************************
CRenderThread::RS360Update - Called when there's an image to be sent

inputs
   PCImageStore         pImage - Image to pass down
*/
void CRenderThread::MyRS360Update (DWORD dwRenderShard, PCImageStore pImage)
{
   // BUGFIX - because pass in 360 callback for pre-render, but don't really want
   // to do anything if only pre-rendering, then need to check
   EnterCriticalSection (&m_CritSec);
   if (m_afRenderingPreRender[dwRenderShard]) {
      delete pImage;
      LeaveCriticalSection (&m_CritSec);
      return;
   }
   LeaveCriticalSection (&m_CritSec);

   // know that it's a 360 image if this is called
   pImage->m_dwStretch = 4;

   // set the node
   if (!m_apRS360Node[dwRenderShard]) {
      delete pImage;
      return;
   }
   pImage->MMLSet (m_apRS360Node[dwRenderShard]);

   PostImageRenderedAsThread (pImage, FALSE, TRUE, m_adwRenderingHighQuality[dwRenderShard], FALSE);
}


/******************************************************************************
CRenderThread::IsNode360 - Returns TRUE if the node describes a 360-degree image.
THis is used to ensure that only one 360 degree image is on the queue at a time,
so they don't get queued up when walking between rooms.

inputs
   PCMMLNode2         pNode - node
returns
   BOOL - TRUE if is is, FALSE if not
*/
BOOL CRenderThread::IsNode360 (PCMMLNode2 pNode)
{
   PWSTR psz = pNode->NameGet();
   if (_wcsicmp(psz, Circumreality3DScene()) && _wcsicmp(psz, Circumreality3DObjects()))
      return FALSE;

   DWORD dwAspect = (DWORD) MMLValueGetInt (pNode , L"Aspect", 2);
   return (dwAspect == 10);
}



/******************************************************************************
EnglishPhoneToViseme - Converts an English phoneme (as per MLexiconEnglishPhoneGet) into
a viseme number.

inputs
   DWORD          dwPhone - English phoneme, as per MLexiconEnglishPhoneGet.
                           Use 0 for silence.
                           Use -1 for blink.
returns
   DWORD - Visme number that can be baseed in VisemeModifyMML.
         // NUMVISEMES-1 = VISEME_BLINK = blink
         // 0 = silence, m, b, p
         // 1 = c, d, g, k, n, r, s, th, y, and z
         // 2 = a, i
         // 3 = e
         // 4 = oe
         // 5 = food, u
         // 6 = l, th
         // 7 = f, v
*/
DWORD EnglishPhoneToViseme (DWORD dwPhone)
{
   // get from the lexicon
   PLEXENGLISHPHONE pep;
   if (dwPhone == (DWORD)-1)
      pep = NULL;
   else
      pep = MLexiconEnglishPhoneGet (dwPhone);

   // if nothing, then default to blink
   if (!pep)
      return VISEME_BLINK;

   // else, look at the name
   switch (pep->szPhoneLong[0]) {
   case L'a':
      switch (pep->szPhoneLong[1]) {
      case L'a': // father
      case L'e': // at
      case L'o': // law
      case L'x': // About
      case L'y': // tIE
         return 2;
      case L'h': // cut
         return 5;
      case L'w': // out
         return 3;
      } // switch
      break;

   case L'b': // Bat
      return 0;

   case L'c':  // CHin
      return 1;

   case L'd':
      switch (pep->szPhoneLong[1]) {
      case 0: // Dip
      case L'h': // THis
         return 6;
      } // switch
      break;

   case L'e':
      switch (pep->szPhoneLong[1]) {
      case L'e': // bEt
         return 3;
      case L'r': // hUrt
         return 5;
      case L'y': // AId
         return 2;
      } // switch
      break;

   case L'f':  // Fat
      return 7;

   case L'g':  // Give
      return 1;

   case L'h':  // Hit
      return 3;

   case L'i':
      switch (pep->szPhoneLong[1]) {
      case L'h': // bIt
         return 2;
      case L'y': // bEEt
         return 3;
      } // switch
      break;

   case L'j':  // Joy
   case L'k':  // Kiss
      return 1;

   case L'l':  // Lip
      return 6;

   case L'm':  // Map
      return 0;

   case L'n':
      switch (pep->szPhoneLong[1]) {
      case 0: // Nip
         return 1;
      case L'x': // kiNG
         return 2;
      } // switch
      break;

   case L'o':
      switch (pep->szPhoneLong[1]) {
      case L'w': // tOE
      case L'y': // tOY
         return 4;
      } // switch
      break;

   case L'p':  // Pin
      return 0;

   case L'r':  // Red
      return 1;

   case L's':
      switch (pep->szPhoneLong[1]) {
      case 0: // Sip
         return 1;
      case L'h': // SHe
         return 3;
      } // switch
      break;

   case L't':
      switch (pep->szPhoneLong[1]) {
      case 0: // Talk
         return 1;
      case L'h': // THin
         return 6;
      } // switch
      break;

   case L'u':
      switch (pep->szPhoneLong[1]) {
      case L'h': // fOOt
      case L'w': // fOOd
         return 5;
      } // switch
      break;

   case L'v':  // Vat
      return 7;

   case L'w':  // Wit
      return 5;

   case L'y':  // Yet
      return 3;

   case L'z':
      switch (pep->szPhoneLong[1]) {
      case 0: // Zip
      case L'h': // aZure
         return 1;
      } // switch
      break;

   case L'<': // SILENCE[0]:
   default: // alsume silence
      return 0;   // viseme 0 is silence
   } // switch

   // if gets here is unknown
   return 0;
}


/******************************************************************************
VisemeModifyMML - This takes a viseme from EnglishPhoneToViseme, along with a MML
string, and copies it to a new memory block where it has lip-sync phonemes
inserted.

inputs
   DWORD       dwViseme - From EnglishPhoneToViseme()
   PWSTR       pszMML - MML string
   PCMem       pMem - Cleared and filled with the MML string, with inserted LipSync:XXX attributes.
                  If this is NULL the function acts as a check for <LipSync v=1/> only.
returns
   BOOL - TRUE if success. FALSE if coudln't find <LipSync v=1/> in the MML
*/
BOOL VisemeModifyMML (DWORD dwViseme, PWSTR pszMML, PCMem pMem)
{
   // find the string
   PWSTR pszStart = (PWSTR) MyStrIStr (pszMML, L"<lipsync ");
   if (!pszStart)
      return FALSE;
   
   //find the end
   PWSTR pszEnd = wcschr (pszStart, L'>');
   if (!pszEnd)
      return FALSE;

   // if no pMem then done
   if (!pMem)
      return TRUE;

   // viseme 0 is unchanged
   if (!dwViseme) {
      // NOTE: Need to handle this way, so includes <LipSync v=1/>, so that works ok with cvisimage
      MemZero (pMem);
      MemCat (pMem, pszMML);
      return TRUE;
   }

   // copy over first part
   WCHAR cTemp = *pszStart;
   *pszStart = 0;
   MemZero (pMem);
   MemCat (pMem, pszMML);
   *pszStart = cTemp;

   // figure out the string to substitute
   PLEXENGLISHPHONE pep = NULL;
   LEXENGLISHPHONE lep;
   switch (dwViseme) {
   case VISEME_BLINK: // blink
      pep = NULL;

      break;
   case 0: // silence, m, b, p
   default:
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED;
      break;

   case 1: // c, d, g, k, n, r, s, th, y, and z
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED;
      break;

   case 2: // a, i
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MID;
      break;

   case 3: // e
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_MAX | PIS_VERTOPN_SLIGHT;
      break;

   case 4: // oe
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MAX;
      break;

   case 5: // food, u
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID;
      break;

   case 6: // l, th
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT;
      break;

   case 7: // f, v
      pep = &lep;
      lep.dwShape = PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_KEEPSHUT | PIS_TEETHTOP_FULL |PIS_LATTEN_SLIGHT | PIS_VERTOPN_CLOSED;
      break;

   } // swtich

   // animation params
   fp fVertOpen = 0, fLatTension = 0, fLatPucker = 0, fTeethTop = 0, fTeethBottom = 0, fTongueForward = 0, fTongueUp = 0, fBlink = 0;
   if (pep) switch (pep->dwShape & PIS_LATTEN_MASK) {
   case PIS_LATTEN_PUCKER:
      fLatPucker = 1;
      break;
   default:
   case PIS_LATTEN_REST:
      fLatTension = 0;
      break;
   case PIS_LATTEN_SLIGHT:
      fLatTension = .5;
      break;
   case PIS_LATTEN_MAX:
      fLatTension = 1;
      break;
   }

   if (pep) switch (pep->dwShape & PIS_VERTOPN_MASK) {
   case PIS_VERTOPN_CLOSED:
      fVertOpen = 0;
      break;
   default:
   case PIS_VERTOPN_SLIGHT:
      fVertOpen = .33;
      break;
   case PIS_VERTOPN_MID:
      fVertOpen = .66;
      break;
   case PIS_VERTOPN_MAX:
      fVertOpen = 1;
      break;
   }

   if (pep) switch (pep->dwShape & PIS_TEETHTOP_MASK) {
   case PIS_TEETHTOP_MID:
      fTeethTop = .5;
      break;
   case PIS_TEETHTOP_FULL:
      fTeethTop = 1;
      break;
   default:
      fTeethTop = 0;
      break;
   }
   if (pep) switch (pep->dwShape & PIS_TEETHBOT_MASK) {
   case PIS_TEETHBOT_MID:
      fTeethBottom = .5;
      break;
   case PIS_TEETHBOT_FULL:
      fTeethBottom = 1;
      break;
   default:
      fTeethBottom = 0;
      break;
   }

   if (pep) switch (pep->dwShape & PIS_TONGUETOP_MASK) {
   case PIS_TONGUETOP_ROOF:
      fTongueUp = 1;
      break;
   case PIS_TONGUETOP_TEETH:
      fTongueUp = .5;
      break;
   default:
   case PIS_TONGUETOP_BOTTOM:
      fTongueUp = 0;
      break;
   }


   if (pep) switch (pep->dwShape & PIS_TONGUEFRONT_MASK) {
   case PIS_TONGUEFRONT_TEETH:
      fTongueForward = 1;
      break;
   case PIS_TONGUEFRONT_BEHINDTEETH:
      fTongueForward = .5;
      break;
   default:
   case PIS_TONGUEFRONT_PALATE:
      fTongueForward = 0;
      break;
   }

   if (!pep)
      fBlink = 1;

   WCHAR szTemp[128];
   DWORD i;
   // add these attributes
   for (i = fBlink ? 7 : 0; i < 8; i++) {
      fp fValue = 0;
      PWSTR pszAttrib = NULL;

      switch (i) {
      case 0:
         fValue = fVertOpen;
         pszAttrib = L"LipSyncMusc:VertOpen";
         break;
      case 1:
         fValue = fLatTension;
         pszAttrib = L"LipSyncMusc:LatTension";
         break;
      case 2:
         fValue = fLatPucker;
         pszAttrib = L"LipSyncMusc:LatPucker";
         break;
      case 3:
         fValue = fTeethTop;
         pszAttrib = L"LipSyncMusc:TeethUpper";
         break;
      case 4:
         fValue = fTeethBottom;
         pszAttrib = L"LipSyncMusc:TeethLower";
         break;
      case 5:
         fValue = fTongueForward;
         pszAttrib = L"LipSyncMusc:TongueForward";
         break;
      case 6:
         fValue = fTongueUp;
         pszAttrib = L"LipSyncMusc:TongueUp";
         break;
      case 7:
         if (!fBlink)
            continue;   // dont bother with the blink attribute for every one
         fValue = fBlink;
         pszAttrib = L"Blink";
         break;
      } // switch(i)

      // NOTE: need quotes around ones with colon, otherwise doens't convert right, but not on blink.
      // Has to do with tests for putting quotes around when to MML string
      if (i == 7)
         swprintf (szTemp, L"<Attrib><Object v=0/><Value v=%g/><Attrib v=%s/></Attrib>", (double)fValue, pszAttrib);
      else
         swprintf (szTemp, L"<Attrib><Object v=0/><Value v=%g/><Attrib v=\"%s\"/></Attrib>", (double)fValue, pszAttrib);
      MemCat (pMem, szTemp);
   } // i
   
   // finish off
   MemCat (pMem, pszEnd+1);

   return TRUE;
}

