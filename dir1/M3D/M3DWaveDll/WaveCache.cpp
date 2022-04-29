/***************************************************************************
WaveCache.cpp - C++ object for caching wave data

begun 18/5/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"

typedef struct {
   DWORD          dwRefCount;       // reference count, when this reaches 0 delete the wave
   PCM3DWave      pWave;            // wave object
} WAVECACHE, *PWAVECACHE;


/* globals */
CListFixed     glWAVECACHE;      // list of cached waves
CRITICAL_SECTION gcsWaveCache;   // critical section for wave cache

BOOL           fWAVECACHEInit = FALSE; // set to TRUE when the wave cache is intiialized

/*****************************************************************************************
WaveCacheOpen - Opens a wave file. This increases the reference count by 1.

inputs
   char        *pszFile - file to open
returns
   PCM3DWave - Wave to use. DONT delete this; instead, call WaveCacheRelease().
                  Returns NULL if cant open wave. NOTE: This file will be opened
                  without necessarily loading wave data, so if you want that you'll
                  need to tell the wave object to load it
*/

PCM3DWave WaveCacheOpen (PWSTR pszFile)
{
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   return WaveCacheOpen (szTemp);
}

PCM3DWave WaveCacheOpen (char *pszFile)
{
   EnterCriticalSection (&gcsWaveCache);

   // see if there's a file that matches
   DWORD dwNum = glWAVECACHE.Num();
   PWAVECACHE pwc = (PWAVECACHE) glWAVECACHE.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++, pwc++)
      if (!_stricmp(pszFile, pwc->pWave->m_szFile)) {
         pwc->dwRefCount++;
         LeaveCriticalSection (&gcsWaveCache);
         return pwc->pWave;
      }

   // else cant find
   if (!fWAVECACHEInit) {
      fWAVECACHEInit = TRUE;
      glWAVECACHE.Init (sizeof(WAVECACHE));
   }

   PCM3DWave pNew;
   pNew = new CM3DWave;
   if (!pNew) {
      LeaveCriticalSection (&gcsWaveCache);
      return NULL;
   }
   if (!pNew->Open (NULL, pszFile, FALSE)) {
      delete pNew;
      LeaveCriticalSection (&gcsWaveCache);
      return NULL;   // cant open
   }

   // else opened
   WAVECACHE wc;
   memset (&wc, 0, sizeof(wc));
   wc.dwRefCount = 1;
   wc.pWave = pNew;
   glWAVECACHE.Add (&wc);

   LeaveCriticalSection (&gcsWaveCache);
   return pNew;
}


/*****************************************************************************************
WaveCacheRelease - Releases a wave cache. For every call the WaveCacheOpen() there must
be a call to WaveCacheRelease()

inputs
   PCM3DWave         pWave - To Release
returns
   BOOL - TRUE if found and released. FALSE if cant find
*/
BOOL WaveCacheRelease (PCM3DWave pWave)
{
   EnterCriticalSection (&gcsWaveCache);

   // see if there's a file that matches
   DWORD dwNum = glWAVECACHE.Num();
   PWAVECACHE pwc = (PWAVECACHE) glWAVECACHE.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++, pwc++) {
      if (pwc->pWave == pWave) {
         // found it
         pwc->dwRefCount--;
         if (!pwc->dwRefCount) {
            delete pwc->pWave;
            glWAVECACHE.Remove (i);
         }
         LeaveCriticalSection (&gcsWaveCache);
         return TRUE;
      }
   } // i

   LeaveCriticalSection (&gcsWaveCache);
   return FALSE;  // cant find
}
