/*************************************************************************************
CAudioThread.cpp - Code for managing the renderingthread.

begun 24/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"



#define DXBUFSIZE(x)                   ((x)/1)    // keep 1 second around in DX buffer
#define RECOMMENDURGENT                8        // 1/8th sec
#define RECOMMENDNORMAL                2        // 1/2 sec

#define AUDIOTHREAD_CHANNELS           2        // # channels to use
#define AUDIOTHREAD_SAMPLESPERSEC      22050    // default samples per sec
#define AUDIOTHREAD_BUFSAMPLES         ((DWORD) (AUDIOTHREAD_SAMPLESPERSEC / WVPLAYBUF / 4))
                                                // keep 1/2 second of audio queued up
                                                // BUGFIX - Changed BUFSAMPLES to 1/4 secon
#define AUDIOTHREAD_BUFSIZE            (AUDIOTHREAD_BUFSAMPLES * AUDIOTHREAD_CHANNELS * sizeof(short))
#define AUDIOTHREAD_LOOKAHEADSAMP      (AUDIOTHREAD_SAMPLESPERSEC/2) // calculate this much audio ahead of time
            // BUGFIX - Changed lookahead to 0.5 seconds

#define WM_MYMIDIMESSAGE               (WM_USER+183)
#define MIDITIMERID                    1245
#define TIMER_DIRECTX                  1246

// ATWAVE - Extra information shored in the wave's m_pUserData
typedef struct {
   WCHAR          szName[256];   // name of the file. Use blank to indicate that it's unique
   BOOL           fDelWhenNoRef; // if TRUE then automatically delete when there's no reference because wont be used again
   PCAudioThread  pThis;         // audio thread
   PCMem          pMemVoiceChat; // store voice chat info to decompress
   PCVoiceDisguise pVoiceDisguise;  // voice disguise used for voice chat, not deleted with wave

   // critical section
   BOOL           fUseCritical;  // if TRUE then use the critical section flag to access, else dont need
   CRITICAL_SECTION CritSec;     // critical section that protects this area

   // protected by the critical section IF the fUseCritical flag is true
   DWORD          dwRefCount;    // refrence count. When reaches 0 is no longer used
   __int64        iLastUsed;     // last used time
   BOOL           fFinishedLoading; // if TRUE then have completely finished loading this wave in
   DWORD          dwSampleValid; // wave is valid up to this sample
   DWORD          dwSampleProcessTo;   // when loading, fine to load up to this sample. If get beyond, wait
   DWORD          dwSampleExpected; // expected number of samples. -1 if not yet calculated
   LARGE_INTEGER  iTTSTime;      // TTS time stamp, so can mute. If 0 then no time stamp
} ATWAVE, *PATWAVE;


// TTSQUEUE - For quing up TTS to speak
typedef struct {
   PCM3DWave      pWave;         // wave to write to (with the correct sampling rate)
   PCMMLNode2      pNode;         // node that has all the info in it.
   LARGE_INTEGER  iTime;         // time stamp that got speaking message, from QueryPerformanceCounter()
} TTSQUEUE, *PTTSQUEUE;

// DELAYINFO - Info about delayed action
typedef struct {
   DWORD          dwSample;      // occur at this sample (in the next audio wave), or -1 if dont know
   fp             fValue;        // value from MMLValueGet("time" or "percent")
   BOOL           fPercent;      // TRUE if fValue is a percent (100 = 100%)
   LARGE_INTEGER  iTime;         // time stamp
   PCMMLNode2      pNode;         // node to act on
} DELAYINFO, *PDELAYINFO;


PWSTR gpszRecentTTSFile = L"RecentTTSFile.cac";

#ifdef USEDIRECTX
/*************************************************************************************
CDXBuffer::Constructor and destructor
*/
CDXBuffer::CDXBuffer (void)
{
   m_pDSB = NULL;
   m_p3D = NULL;
   m_f3D = FALSE;
   m_fStartedPlaying = m_fFinishedPlaying = FALSE;
   m_dwSamplesPerSec = 0;
   m_dwChannels = 0;
   m_iTotalPlay = m_iTotalWrite = m_iLastWriteNotSilence = 0;
   m_fPaused = FALSE;
   m_iVolumeLeft = m_iVolumeLeft = -1; // so will set first time
   m_pVolume3D.Zero4();
}

CDXBuffer::~CDXBuffer (void)
{
   Release();
}

/*************************************************************************************
CDXBuffer::Init - Creates a direct sound buffer.

inputs
   LPDIRECTSOUND8 pDirectSound - Direct sound object to use
   DWORD       dwSamplesPerSec - Samples per sec
   DWORD       dwChannels - Channels
   // assumed to be 16 bits
   BOOL        f3D - If TRUE want a 3D sound buffer
returns
   LPDIRECTSOUNDBUFFER8 - Buffer, or NULL if fail
*/
BOOL CDXBuffer::Init (LPDIRECTSOUND8 pDirectSound, DWORD dwSamplesPerSec, DWORD dwChannels, BOOL f3D)
{
   if (!pDirectSound)
      return NULL;
   if (m_pDSB)
      return FALSE;

   WAVEFORMATEX wfex; 
   DSBUFFERDESC dsbdesc; 
   HRESULT hr; 

   // Set up wave format structure. 
   memset(&wfex, 0, sizeof(PCMWAVEFORMAT)); 
   wfex.wFormatTag = WAVE_FORMAT_PCM; 
   wfex.nChannels = (WORD)dwChannels; 
   wfex.nSamplesPerSec = dwSamplesPerSec; 
   wfex.wBitsPerSample = 16; 
   wfex.nBlockAlign = wfex.nChannels * wfex.wBitsPerSample / 8; 
   wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign; 

   // Set up DSBUFFERDESC structure. 

   memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); 
   dsbdesc.dwSize = sizeof(DSBUFFERDESC); 
   dsbdesc.dwFlags = (f3D ? DSBCAPS_CTRL3D : DSBCAPS_CTRLPAN) | DSBCAPS_CTRLVOLUME |
      DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS; 
   if (f3D)
      dsbdesc.guid3DAlgorithm = DS3DALG_DEFAULT;

   dsbdesc.dwBufferBytes = m_dwBufSize = DXBUFSIZE(wfex.nAvgBytesPerSec);   // 1 second worth, since recommend is 1/2 sec
   dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&wfex; 

   // Create buffer. 
   LPDIRECTSOUNDBUFFER lpDsb;
   hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &lpDsb, NULL); 
   if (FAILED(hr))
      return NULL;

   hr = lpDsb->QueryInterface (IID_IDirectSoundBuffer8, (LPVOID*) &m_pDSB);
   lpDsb->Release();
   lpDsb = NULL;
   if (FAILED(hr))
      return NULL;

   // get 3D interface
   m_p3D = NULL;
   if (f3D) {
      hr = m_pDSB->QueryInterface (IID_IDirectSound3DBuffer8, (LPVOID*) &m_p3D);
      if (FAILED(hr))
         m_p3D = NULL;
   }
   if (m_p3D) {
      DS3DBUFFER sb;
      memset (&sb, 0, sizeof(sb));
      sb.dwSize = sizeof(sb);
      m_p3D->GetAllParameters (&sb);
      sb.vPosition.x = 0;
      sb.vPosition.y = 1;
      sb.vPosition.z = 0;
      sb.vVelocity.x = 0;
      sb.vVelocity.y = 0;
      sb.vVelocity.z = 0;
      sb.dwInsideConeAngle = DS3D_DEFAULTCONEANGLE;
      sb.dwOutsideConeAngle = DS3D_DEFAULTCONEANGLE;
      sb.lConeOutsideVolume = DS3D_DEFAULTCONEOUTSIDEVOLUME ;
      sb.dwMode = DS3DMODE_NORMAL;
      sb.flMinDistance = 0.5;
      sb.flMaxDistance = DS3D_DEFAULTMAXDISTANCE;
      hr = m_p3D->SetAllParameters (&sb, DS3D_IMMEDIATE);
   }

   m_f3D = f3D;
   m_dwSamplesPerSec = dwSamplesPerSec;
   m_dwChannels = dwChannels;
   m_fStartedPlaying = m_fFinishedPlaying = FALSE;
   // NOTE: Dont set m_fPaused here, do in release

   // get the last play and record position
   m_pDSB->GetCurrentPosition (&m_dwLastPlayPosn, NULL);
   m_dwLastWritePosn = 0;
   m_iTotalPlay = m_iTotalWrite = m_iLastWriteNotSilence = 0;   // in butes

   // set the volums
   if (m_iVolumeLeft != -1) {
      m_iVolumeLeft = -1;
      SetVolume (m_iVolumeLeft, m_iVolumeRight);
   }
   if (m_pVolume3D.p[3] != 0) {
      CPoint pTemp;
      pTemp.Copy (&m_pVolume3D);
      m_pVolume3D.p[3] = 0;
      SetVolume (&pTemp, pTemp.p[3]);
   }

   // else success
   return TRUE;
}


/*************************************************************************************
CDXBuffer::PauseResume - Pauses or resumes the wave

inputs
   BOOL        fPause - Set to TRUE to pause, FALSE to resume
returns
   none
*/
void CDXBuffer::PauseResume (BOOL fPause)
{
   if (!fPause == !m_fPaused)
      return;  // nothing
   m_fPaused = fPause;
   if (!m_pDSB)
      return;

   // start/stop accordinglu
   if (m_fStartedPlaying && !m_fFinishedPlaying && !m_fPaused)
      m_pDSB->Play (0, 0, DSBPLAY_LOOPING);
   else
      m_pDSB->Stop ();

}


/*************************************************************************************
CDXBuffer::GetCurrentPosition - Gets the current play and record position.

inputs
   __int64           *piPlay - Current play position, total. Can be NULL
   __int64           *piWrite - Current write position, total. Can be NULL
   DWORD             *pdwPlay - Current play position in DirectX. Can be NULL
   DWORD             *pdwWrite - Current write position in DirectX. Can be NULL
returns
   BOOL - TRUE if succes
*/
BOOL CDXBuffer::GetCurrentPosition (__int64 *piPlay, __int64 *piWrite, DWORD *pdwPlay, DWORD *pdwWrite)
{
   if (!m_pDSB)
      return FALSE;

   // remember last position
   DWORD dwLastPlay = m_dwLastPlayPosn;

   HRESULT hRes = m_pDSB->GetCurrentPosition (&m_dwLastPlayPosn, NULL);
   if (FAILED(hRes))
      return FALSE;

   // figure out the delta for play
   if (dwLastPlay <= m_dwLastPlayPosn)
      m_iTotalPlay += (__int64) (m_dwLastPlayPosn - dwLastPlay);
   else
      m_iTotalPlay += (__int64) (m_dwLastPlayPosn + m_dwBufSize - dwLastPlay);   // wrapped around

   // NOTE: Not doing this for iTotalWrite, since handle that seperately

   // fill in values
   if (piPlay)
      *piPlay = m_iTotalPlay;
   if (piWrite)
      *piWrite = m_iTotalWrite;
   if (pdwPlay)
      *pdwPlay = m_dwLastPlayPosn;
   if (pdwWrite)
      *pdwWrite = m_dwLastWritePosn;

   return TRUE;

}


/*************************************************************************************
CDXBuffer::FinishedPlaying - This tests to see if the wave is finished playing (IE:
everything has been played except for tacked on silence.) If it has, then this
returns TRUE. If FALSE, it tacks on some more silence.

If it is finished, this automatically stops.

returns
   BOOL - TRUE if finished playing, FALSE then tack on more silence
*/
BOOL CDXBuffer::FinishedPlaying (void)
{
   if (!m_pDSB || m_fFinishedPlaying)
      return TRUE;

   // call recommend so know how much silence to tack on AND where the current play position is
   DWORD dwRecommend = Recommend(NULL);

   if (m_iTotalPlay >= m_iLastWriteNotSilence) {
      m_pDSB->Stop ();
      m_fFinishedPlaying = TRUE;
      return TRUE;
   }

   // else, tack on silence
   if (dwRecommend)
      Write (NULL, dwRecommend);
   return FALSE;
}

/*************************************************************************************
CDXBuffer::Recommend - Returns the number of bytes recommended to fill in,
so always fill in about a second ahead

inputs
   DWORD       *pdwUrgent - If not NULL, this is filled with the number of bytes that
               urgently need to be filled.
returns
   DWORD - Number of recommended bytes. This is always aligned with the blocksize.
      (Aka: Doesn't cut sample in half)
*/
DWORD CDXBuffer::Recommend (DWORD *pdwUrgent)
{
   __int64 iPlay, iWrite;
   if (!GetCurrentPosition (&iPlay, &iWrite))
      return 0;

   DWORD dwRet; 
   __int64 iRecommend;
   if (pdwUrgent) {
      iRecommend = iPlay - iWrite + (__int64) (m_dwSamplesPerSec * m_dwChannels * 2) / RECOMMENDURGENT;  // 1/16th of a second is urgent
         // BUGFIX - Make 1/8th second. 1/16th seems to cause stuttering with one TTS following another
      if (iRecommend < 0)
         dwRet = 0;
      else if (iRecommend < 0x10000000)
         dwRet = (DWORD)iRecommend;
      else
         dwRet = 0x10000000;   // not likely to happen

      // make sure aligned
      dwRet &= ~(m_dwChannels*2-1);
      *pdwUrgent = dwRet;
   }

   iRecommend = iPlay - iWrite + (__int64) DXBUFSIZE(m_dwSamplesPerSec * m_dwChannels * 2) / RECOMMENDNORMAL;  // keep 1/2 of recommended buf
   if (iRecommend < 0)
      dwRet = 0;
   else if (iRecommend < 0x10000000)
      dwRet = (DWORD)iRecommend;
   else
      dwRet = 0x10000000;   // not likely to happen

   // make sure aligned
   dwRet &= ~(m_dwChannels*2-1);
   return dwRet;
}


/*************************************************************************************
CDXBuffer::Write - Writes a number of bytes into the buffer. NOTE: Don't write
more bytes than recommended.

inputs
   PBYTE          pbData - Data. If NULL then write silence
   DWORD          dwSize - Number of bytes
returns
   BOOL - TRUE if success
*/
BOOL CDXBuffer::Write (PBYTE pbData, DWORD dwSize)
{
   if (!m_pDSB)
      return FALSE;

   if (!dwSize)
      return TRUE;

   // if size > one second of data then break into smaller sections
   DWORD dwOneSec = DXBUFSIZE(m_dwSamplesPerSec * m_dwChannels * 2);
   while (dwSize > dwOneSec) {
      if (!Write (pbData, dwOneSec))
         return FALSE;  // error
      if (pbData)
         pbData += dwOneSec;
      dwSize -= dwOneSec;
   }

   // lock the data to send
   HRESULT hRes;
   DWORD dwWritten = dwSize;
   PVOID p1, p2;
   DWORD dw1, dw2;
   hRes = m_pDSB->Lock (m_dwLastWritePosn, dwSize, &p1, &dw1, &p2, &dw2, 0);
   m_dwLastWritePosn = (m_dwLastWritePosn + dwWritten) % m_dwBufSize;
      // NOTE: even if fail, want to update this write position
   if (FAILED(hRes))
      return FALSE;

   DWORD dwCopy;
   if (p1) {
      dwCopy = min(dwSize, dw1);
      if (pbData) {
         memcpy (p1, pbData, dwCopy);
         pbData += dwCopy;
      }
      else
         memset (p1, 0, dwCopy);
      dwSize -= dwCopy;
      m_iTotalWrite += (__int64)dwCopy;
   }
   if (p2) {
      dwCopy = min(dwSize, dw2);
      if (pbData) {
         memcpy (p2, pbData, dwCopy);
         pbData += dwCopy;
      }
      else
         memset (p2, 0, dwCopy);
      dwSize -= dwCopy;
      m_iTotalWrite += (__int64)dwCopy;
   }

   if (pbData)
      m_iLastWriteNotSilence = m_iTotalWrite;   // remember the last non-silence

   hRes = m_pDSB->Unlock (p1, dw1, p2, dw2);
   if (FAILED(hRes))
      return FALSE;

   // potentially start playing
   if (!m_fStartedPlaying && !m_fFinishedPlaying && !m_fPaused) {
      hRes = m_pDSB->Play (0, 0, DSBPLAY_LOOPING);
      if (SUCCEEDED(hRes))
         m_fStartedPlaying = TRUE;
   }

   return TRUE;
}


/*************************************************************************************
CDXBuffer::Release - Call this to release the buffer. Usually dont need to call this
because shutdown will.
*/
BOOL CDXBuffer::Release (void)
{
   if (!m_pDSB)
      return FALSE;

   if (m_p3D) {
      m_p3D->Release();
      m_p3D = NULL;
   }

   m_pDSB->Release();
   m_pDSB = NULL;

   m_fPaused = FALSE;
   m_iVolumeLeft = m_iVolumeRight = -1;   // so will set first time
   m_pVolume3D.Zero4();

   return TRUE;
}

/*************************************************************************************
CDXBuffer::SetFormatGlobal - Call this once to set the global format.
*/
BOOL CDXBuffer::SetFormatGlobal (void)
{
   WAVEFORMATEX wfex;
   HRESULT hRes;

   // set format to 16 bit if it isn't by default
   memset(&wfex, 0, sizeof(PCMWAVEFORMAT)); 
   wfex.wFormatTag = WAVE_FORMAT_PCM; 
   wfex.nChannels = 2; 
   wfex.nSamplesPerSec = AUDIOTHREAD_SAMPLESPERSEC; 
   wfex.wBitsPerSample = 16; 
   wfex.nBlockAlign = wfex.nChannels * wfex.wBitsPerSample / 8; 
   wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign; 
   hRes = m_pDSB->SetFormat (&wfex);  // NOTE - this is failing on my sound card, but poses no problem

   return (SUCCEEDED(hRes)) ? TRUE : FALSE;
}


/*************************************************************************************
CDXBuffer::SetVolume - Sets the 2D volume

inputs
   int         iLeft - Left, AQ_NOVOLCHANGE = no change over played volume
   int         iRight - Righr, AQ_NOVOLCHANGE = no change over played volume
returns
   BOOL - TRUE if success
*/
BOOL CDXBuffer::SetVolume (int iLeft, int iRight)
{
   if (m_f3D)
      return FALSE;

   // take out any surround sound attempts
   iLeft = abs(iLeft);
   iRight = abs(iRight);

   // if they're the same as last time then no change
   if ((iLeft == m_iVolumeLeft) && (iRight == m_iVolumeRight))
      return TRUE;   // no change

   m_iVolumeLeft = iLeft;
   m_iVolumeRight = iRight;

   // else, figure out dB for both
   iLeft = max(iLeft, 1);
   iRight = max(iRight, 1);
   fp fDbLeft = log10((fp)iLeft / (fp)AQ_NOVOLCHANGE) * 20.0;
   fp fDbRight = log10((fp)iRight / (fp)AQ_NOVOLCHANGE) * 20.0;

   // convert this to 10000 scale
   fDbLeft *= 100.0;
   fDbRight *= 100.0;
   fDbLeft = max(fDbLeft, -10000);
   fDbLeft = min(fDbLeft, 0);
   fDbRight = max(fDbRight, -10000);
   fDbRight = min(fDbRight, 0);
   iLeft = (int)fDbLeft;
   iRight = (int)fDbRight;

   // find max
   int iMax = max(iLeft, iRight);

   // set the volume
   HRESULT hRes;
   if (m_pDSB) {
      hRes = m_pDSB->SetVolume (iMax);
      if (FAILED(hRes))
         return FALSE;
   }

   // set the pan
   int iPan;
   if (iLeft < iMax)
      iPan = (iMax - iLeft);
   else
      iPan = -(iMax - iRight);
   if (m_pDSB) {
      hRes = m_pDSB->SetPan (iPan);
      if (FAILED(hRes))
         return FALSE;
   }

   return TRUE;
}


/*************************************************************************************
CDXBuffer::SetVolume - Sets the 3D volume

inputs
   PCPoint     pVolume - XYZ volume
   fp          fDB - Decibels. 60 = normal speaking
returns
   BOOL - TRUE if success
*/
BOOL CDXBuffer::SetVolume (PCPoint pVolume, fp fDB)
{
   if (!m_f3D)
      return FALSE;

   // see if they're the same
   if ((m_pVolume3D.p[0] == pVolume->p[0]) && (m_pVolume3D.p[1] == pVolume->p[1]) &&
      (m_pVolume3D.p[2] == pVolume->p[2]) && (m_pVolume3D.p[3] == fDB))
      return TRUE;

   // else, set
   m_pVolume3D.Copy (pVolume);
   m_pVolume3D.p[3] = fDB;
#if 0 // def _DEBUG
   char szTemp[128];
   sprintf (szTemp, "\r\nPos = %f, %f, %f, %f", (double)m_pVolume3D.p[0], (double)m_pVolume3D.p[1], (double)m_pVolume3D.p[2],
      (double)m_pVolume3D.p[3]);
   OutputDebugString (szTemp);
#endif

   HRESULT hRes;
   CPoint p;
   p.Copy (&m_pVolume3D);
   // adjust by volume
#define HACKFORDIRECTSOUND    -6 // junk - for creative this is -18, for headphones this is 0!!!!!
      // so that 60Db = volume of wave playing without 3d sound
   fDB = fDB - 60 + HACKFORDIRECTSOUND;
   if (fDB <= 0) {
      fDB *= 100.0;  // for setvolume levels
      fDB = max(fDB, -10000); // minimum
      hRes = m_pDSB->SetVolume ((long)fDB);
   }
   else {
      // this is suppsoed to be louder than one would guess from the distance
      hRes = m_pDSB->SetVolume (0);  // no attentuation

      fDB = pow (10, fDB / 20.0);   // so know how many times louder/quieter, linearly
      fDB = sqrt(fDB);  // so know distance calculation
      p.Scale (1.0 / fDB);
   }
   if (m_p3D) {
      hRes = m_p3D->SetPosition (p.p[0], p.p[1], p.p[2], DS3D_IMMEDIATE);
#if 0 // def _DEBUG
      char szTemp[128];
      sprintf (szTemp, "\r\nPos = %f, %f, %f", (double)p.p[0], (double)p.p[1], (double)p.p[2]);
      OutputDebugString (szTemp);
#endif

      if (FAILED(hRes))
         return FALSE;
   }

   return TRUE;
}

#endif // USEDIRECTX



/*************************************************************************************
IsBlacklisted - Returns TRUE if a voice is blacklisted.

inputs
   GUID        *pgID - SPeaker's guid
returns
   BOOL - TRUE if is blacklisted, FALSE if not
*/
BOOL IsBlacklisted (GUID *pgID)
{
   BOOL fBlacklist = FALSE;
   DWORD dwItem;
   GUID *pgItem;
   EnterCriticalSection (&gpMainWindow->m_crSpeakBlacklist);

   // see if it's blacklisted while in critical seciton
   pgItem = (GUID*)gpMainWindow->m_lSpeakBlacklist.Get(0);
   for (dwItem = 0; dwItem < gpMainWindow->m_lSpeakBlacklist.Num(); dwItem++, pgItem++)
      if (IsEqualGUID(*pgItem, *pgID)) {
         fBlacklist = TRUE;
         break;
      }

   // BUGFIX - muteall only affects tts
   // if (gpMainWindow->m_fMuteAll)
   //   fBlacklist = TRUE;

   LeaveCriticalSection (&gpMainWindow->m_crSpeakBlacklist);

   // if it's on the blacklist then false
   return fBlacklist;
}

/*************************************************************************************
CAudioThread::Constructor and destructor
*/
CAudioThread::CAudioThread (void)
{
   m_hThread = NULL;
   m_hSignalFromThread = NULL;
   m_hSignalToThread = NULL;
   m_fConnected = FALSE;
   m_hWndNotify = NULL;
   m_dwMessage = NULL;
   m_fWantToQuit = FALSE;
   m_fCanLoadTTS = FALSE;

   m_fWantToMute = FALSE;
   m_fMuted = FALSE;
   m_dwMidiVol = FALSE;

   m_lPCM3DWave.Init (sizeof(PCM3DWave));
   m_lPCMidiFile.Init (sizeof(PCMidiFile));
   m_lPCAudioQueue.Init (sizeof(PCAudioQueue));
   m_lTTSQUEUE.Init (sizeof(TTSQUEUE));
   m_lTTSVoiceLoc.Init (sizeof(GUID));
   m_lPCAmbient.Init (sizeof(PCAmbient));

   m_fLong = m_fLat = 0;

   m_fTTSLowPriority = TRUE;
   m_pWaveTTS = NULL;

   m_hThreadTTS = NULL;
   m_hSignalToThreadTTS = CreateEvent (NULL, FALSE, FALSE, NULL);
   memset (&m_iTTSMuteTimeStart, 0, sizeof(m_iTTSMuteTimeStart));
   memset (&m_iTTSMuteTimeEnd, 0, sizeof(m_iTTSMuteTimeEnd));
   memset (&m_iTTSTime, 0, sizeof(m_iTTSTime));
   m_fTTSTimeIgnore = FALSE;

   m_pMidiMix = NULL;
   m_pMidiInstanceEscarpment = NULL;

   m_fEarsSpeakerSep = 1.0;
   m_fEarsBackAtten = 0.75;
   m_fEarsPowDistance = 2.0;
   m_fEarsScale = 1.0;

#ifdef USEDIRECTX
   m_pDirectSound = NULL;
#else
   m_hWnd = NULL;
   m_hWaveOut = NULL;
   memset (m_aPlayWaveHdr, 0, sizeof(m_aPlayWaveHdr));
#endif
   m_iTime = 0;

   m_lVISEMEMESSAGE.Init (sizeof(VISEMEMESSAGE));
   QueryPerformanceFrequency (&m_iPerformanceFrequency);

   InitializeCriticalSection (&m_CritSecWave);
   InitializeCriticalSection (&m_CritSecQueue);
   InitializeCriticalSection (&m_CritSecTTS);

}

CAudioThread::~CAudioThread (void)
{
   if (m_hThread)
      Disconnect();

   CloseHandle (m_hSignalFromThread);
   CloseHandle (m_hSignalToThread);

   DeleteCriticalSection (&m_CritSecWave);
   DeleteCriticalSection (&m_CritSecQueue);
   DeleteCriticalSection (&m_CritSecTTS);

   // the act of disconnecting should free everything up
}


/*************************************************************************************
AudioThreadProc - Thread that handles the Render.
*/
DWORD WINAPI AudioThreadProc(LPVOID lpParameter)
{
   PCAudioThread pThread = (PCAudioThread) lpParameter;

#ifdef USEDIRECTX
   CoInitialize (NULL); // for ex, COINIT_APARTMENTTHREADED);
#endif

   pThread->ThreadProc ();

#ifdef USEDIRECTX
   CoUninitialize ();
#endif

   return 0;
}


/*************************************************************************************
TTSThreadProc - Thread that handles TTS.
*/
DWORD WINAPI TTSThreadProc(LPVOID lpParameter)
{
   PCAudioThread pThread = (PCAudioThread) lpParameter;

   pThread->ThreadProcTTS ();

   return 0;
}


/************************************************************************************
AudioThreadWndProcWndProc - internal windows callback for socket simulator
*/
LRESULT CALLBACK AudioThreadWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCAudioThread p = (PCAudioThread) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCAudioThread p = (PCAudioThread) (LONG_PTR) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCAudioThread) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/*************************************************************************************
CAudioThread::WndProc - Manages the window calls for the server window.
This basically accepts data requests and creates new connections.
*/
LRESULT CAudioThread::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_CREATE:
      SetTimer (hWnd, MIDITIMERID, 5, NULL);
#ifdef USEDIRECTX
      SetTimer (hWnd, TIMER_DIRECTX, 25, NULL); // BUGFIX - 25 ms
#endif
      break;

   case WM_DESTROY:
      KillTimer (hWnd, MIDITIMERID);
#ifdef USEDIRECTX
      KillTimer (hWnd, TIMER_DIRECTX);
#endif
      break;

   case WM_TIMER:
#ifdef USEDIRECTX
      if (wParam == TIMER_DIRECTX) {
         // mute, just in case
         HandleMute ();

         // do work
         PlayAddBuffer ();

         return 0;
      }
#endif //  USEDIRECTX
      if (wParam == MIDITIMERID) {
         // mute, just in case
         HandleMute ();

         // go through and find timer procs and update them
         // loop through all the buffers and add
         EnterCriticalSection (&m_CritSecQueue);
         DWORD i;

         // post lipsync messages
         PVISEMEMESSAGE pvm = (PVISEMEMESSAGE) m_lVISEMEMESSAGE.Get(0);
         LARGE_INTEGER iTime;
         QueryPerformanceCounter (&iTime);
         for (i = 0; i < m_lVISEMEMESSAGE.Num(); i++) {
            if (iTime.QuadPart < pvm[i].iTime.QuadPart)
               continue;   // not yet

            // else, post
            VisemePost (&pvm[i]);
            m_lVISEMEMESSAGE.Remove (i);
            i--;
            pvm = (PVISEMEMESSAGE) m_lVISEMEMESSAGE.Get(0);
         } // i


         for (i = m_lPCAudioQueue.Num()-1; i < m_lPCAudioQueue.Num(); i--) {
            PCAudioQueue *ppq = (PCAudioQueue*)m_lPCAudioQueue.Get(i);
            if (!ppq)
               continue;
            PCAudioQueue pq = *ppq;

            pq->MidiTimer();
         } // i
         LeaveCriticalSection (&m_CritSecQueue);
         return 0;
      }
      break;

#ifndef USEDIRECTX
   case MM_WOM_DONE:
      // mute
      HandleMute ();

      // add it again
      PlayAddBuffer ((PWAVEHDR)lParam);
      return 0;
#endif // USEDIRECTX

   case WM_MYMIDIMESSAGE:
      switch (wParam) {
      case 0: // create new midi instace
         if (!m_pMidiInstanceEscarpment && m_pMidiMix)
            m_pMidiInstanceEscarpment = m_pMidiMix->InstanceNew ();
         break;
      case 1:  // release midi instance
         if (m_pMidiInstanceEscarpment) {
            delete m_pMidiInstanceEscarpment;
            m_pMidiInstanceEscarpment = NULL;
         }
         break;
      case 2:  // message to midi instance
         if (m_pMidiInstanceEscarpment) {
            DWORD dw = (DWORD)lParam;
            PBYTE pb = (PBYTE)&dw;
            m_pMidiInstanceEscarpment->MidiMessage (pb[0], pb[1], pb[2]);
         }
         break;
      } // switch
      return 0;

   } // switch uMsg


   // else
   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}



/**********************************************************************************
CAudioThread::AmbientSounds - Called when an <AmbientSounds> message comes into
a queue.

inputs
   PCMMLNode2        pNode - Node of the <AmbientSounds>...</AmbientSounds>
   BOOL              fUseCritSec - If TRUE then need to turn critical section on
returns
   none
*/
void CAudioThread::AmbientSounds (PCMMLNode2 pNode, BOOL fUseCritSec)
{
   // keep track of which of current ambient sounds should be kept
   CMem memKeep;
   if (!memKeep.Required (m_lPCAmbient.Num()*sizeof(BOOL)))
      return;
   memset (memKeep.p, 0, m_lPCAmbient.Num()*sizeof(BOOL));
   BOOL *pafKeep = (BOOL*)memKeep.p;
   PCAmbient *ppaCur = (PCAmbient*) m_lPCAmbient.Get(0);

   // find all the the nodes with ambient sounds in them
   // that should keep
   CListFixed lPCAmbient;
   lPCAmbient.Init (sizeof(PCAmbient));
   PCAmbient *ppaAdd;
   DWORD i, j;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, CircumrealityAmbient())) {
         // get the name
         psz = MMLValueGet (pSub, L"Name");
         if (!psz)
            psz = L"";  // blank name

         // see if it's already there
         for (j = 0; j < m_lPCAmbient.Num(); j++)
            if (!_wcsicmp((PWSTR)ppaCur[j]->m_memName.p, psz))
               break;
         if (j < m_lPCAmbient.Num()) {
            pafKeep[j] = TRUE;   // already have
            continue;
         }

         // make sure it's not on the too-add list
         ppaAdd = (PCAmbient*) lPCAmbient.Get(0);
         for (j = 0; j < lPCAmbient.Num(); j++)
            if (!_wcsicmp((PWSTR)ppaAdd[j]->m_memName.p, psz))
               break;
         if (j < lPCAmbient.Num())
            continue;   // duplicate entry so ignore

         // else, put it on the too-add list
         PCAmbient pNew = new CAmbient;
         if (!pNew)
            continue;
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            continue;
         }
         lPCAmbient.Add (&pNew);

         continue;
      } // if found ambient entry
   } // i

   // loop through the existing ambient that are not used and removed
   for (i = m_lPCAmbient.Num()-1; i < m_lPCAmbient.Num(); i--) {
      if (pafKeep[i])
         continue;

      // Will need to cause some sort of slow fade/shut-down of all
      // the audio bits opened for the ambient. Do this when get loops in
      PCAudioQueue *ppq = (PCAudioQueue*)m_lPCAudioQueue.Get(0);
      for (j = 0; j < this->m_lPCAudioQueue.Num(); j++)
         ppq[j]->AmbientDeleted (ppaCur[i]);

      delete ppaCur[i];
      m_lPCAmbient.Remove (i);
      ppaCur = (PCAmbient*) m_lPCAmbient.Get(0);
   } // i

   if (fUseCritSec)
      EnterCriticalSection (&m_CritSecQueue);

   // append new ambient
   ppaAdd = (PCAmbient*) lPCAmbient.Get(0);
   for (i = 0; i < lPCAmbient.Num(); i++) {
      PCAmbient pa = ppaAdd[i];

      m_lPCAmbient.Add (&pa);

      // will need to initialize random playback
      pa->TimeInit ();

      // create new queues for these
      for (j = 0; j < pa->LoopNum(); j++) {
         PCAudioQueue pq = NewQueue (FALSE);
         if (!pq)
            continue;
         pq->m_pAmbient = pa;
         pq->m_dwAmbientLoop = j;
      } // j
   } // i

   if (fUseCritSec)
      LeaveCriticalSection (&m_CritSecQueue);

   // done
}



/**********************************************************************************
CAudioThread::AmbientSoundsElapsed - Call this when time has ellapsed and
the ambient sounds may wish to update themselves.

inputs
   double         fDelta - Time elapsed
   BOOL           fUseCritSec - If TRUE then use the enter-leave critical seciton
*/
void CAudioThread::AmbientSoundsElapsed (double fDelta, BOOL fUseCritSec)
{
   if (!m_lPCAmbient.Num())
      return;

   PCAmbient *ppa = (PCAmbient*) m_lPCAmbient.Get(0);
   CListFixed lEvent;
   lEvent.Init (sizeof(PCMMLNode2));
   DWORD i;
   for (i = 0; i < m_lPCAmbient.Num(); i++)
      ppa[i]->TimeElapsed (fDelta, &lEvent);

   // see if anything added
   PCMMLNode2 *ppn = (PCMMLNode2*) lEvent.Get(0);
   for (i = 0; i < lEvent.Num(); i++) {
      PCMMLNode2 pn = ppn[i];

      // create new audio queue
      PCAudioQueue paq = NewQueue (fUseCritSec);
      if (!paq)
         continue;

      LARGE_INTEGER iTime;
      QueryPerformanceCounter (&iTime);
      paq->QueueAdd (pn, NULL, iTime, fUseCritSec);
   } // i
}




/**********************************************************************************
CAudioThread::PlayAddBuffer - Adds a buffer to the playlist. NOTE: This also increases
the number of buffers out counter.

inputs
   PWAVEHDR          pwh - Wave header to use. This is assumed to already be returned
returns
   BOOL - TRUE if sent out. FALSE if didnt
*/
#ifdef USEDIRECTX
BOOL CAudioThread::PlayAddBuffer (void)
#else
BOOL CAudioThread::PlayAddBuffer (PWAVEHDR pwh)
#endif
{
#ifdef USEDIRECTX
   if (!m_pDirectSound)
      return FALSE;  // not opened
#else
   if (!m_hWaveOut)
      return FALSE;  // not opened
#endif
   if (!m_hWnd || m_fWantToQuit)
      return FALSE;  // not opened


#ifdef USEDIRECTX
   CMem memScratch;

   // silent drone
   DWORD dwBytes = m_bufDrone.Recommend (NULL);
   m_bufDrone.Write (NULL, dwBytes);

   // update global itime from drone
   __int64 iNewTime = 0;
   if (m_bufDrone.GetCurrentPosition (&iNewTime))
      iNewTime /= (m_bufDrone.m_dwChannels * 2); // since want in samples
   int iTimeElapsed = (int)(iNewTime - m_iTime);
   iTimeElapsed = max(iTimeElapsed, 0);   // just in case
#else
   // clear out the contents
   pwh->dwBufferLength = AUDIOTHREAD_BUFSIZE;
   memset (pwh->lpData, 0, pwh->dwBufferLength);
#endif

   // loop through all the buffers and add
   EnterCriticalSection (&m_CritSecQueue);
   DWORD i;
   for (i = m_lPCAudioQueue.Num()-1; i < m_lPCAudioQueue.Num(); i--) {
      PCAudioQueue *ppq = (PCAudioQueue*)m_lPCAudioQueue.Get(i);
      if (!ppq)
         continue;
      PCAudioQueue pq = *ppq;

#ifdef USEDIRECTX
      if (!pq->AudioSum (m_pDirectSound, &memScratch, AUDIOTHREAD_BUFSAMPLES, FALSE) && i) {
#else
      if (!pq->AudioSum ((short*)pwh->lpData, AUDIOTHREAD_BUFSAMPLES, FALSE) && i) {
#endif
         // this queue is done. delete it
         delete pq;
         m_lPCAudioQueue.Remove (i);
      }

   } // i

   // loop through all the ambientsounds and update
#ifdef USEDIRECTX
   AmbientSoundsElapsed ((double)iTimeElapsed / (double)AUDIOTHREAD_SAMPLESPERSEC, FALSE);
#else
   AmbientSoundsElapsed ((double)AUDIOTHREAD_BUFSAMPLES / (double)AUDIOTHREAD_SAMPLESPERSEC, FALSE);
#endif

   LeaveCriticalSection (&m_CritSecQueue);


#ifdef USEDIRECTX
   m_iTime += (__int64)iTimeElapsed;
#else
   // else, add
   MMRESULT mm;
   mm = waveOutWrite (m_hWaveOut, pwh, sizeof(WAVEHDR));
   if (mm) {
      return FALSE;
   }

   m_iTime += (__int64)AUDIOTHREAD_BUFSAMPLES;
#endif

   // done
   return TRUE;
}

/*************************************************************************************
ExtractSpokenFromSpeakTags - This extracts what's spoken from the SpeakTags, and
filled in a pMem.

inputs
   PCMMLNode2     pNode - Node to extract from
   PCMem          pMem - Append to this memory
returns
   none
*/
void ExtractSpokenFromSpeakTags (PCMMLNode2 pNode, PCMem pMem)
{
   PWSTR psz = pNode->NameGet();
   PCMMLNode2 pSub;
   DWORD dwIndex;
   if (psz && !_wcsicmp(psz, L"TransPros")) {
      pSub = NULL;
      pNode->ContentEnum (pNode->ContentFind (L"OrigText"), &psz, &pSub);
      if (pSub)
         ExtractSpokenFromSpeakTags (pSub, pMem);
      return;
   }

   // recurse
   for (dwIndex = 0; dwIndex < pNode->ContentNum(); dwIndex++) {
      pNode->ContentEnum (dwIndex, &psz, &pSub);
      if (psz)
         MemCat (pMem, psz);
      else if (pSub)
         ExtractSpokenFromSpeakTags (pSub, pMem);
   } // dwIndex
}


/*************************************************************************************
CAudioThread::TTSMuteTime - Sets the autommute time.

inputs
   LARGE_INTEGER        iTimeStart - Mute TTS if comes after this time. If this and iTimeStop are -1
                        then fill in automatically with current time, and previous 60 seconds
   LARGE_INTEGER        iTimeEnd - Mute TTS if comes before this time
returns
   none
*/
void CAudioThread::TTSMuteTime (LARGE_INTEGER iTimeStart, LARGE_INTEGER iTimeEnd)
{
   // automatic fill in
   if ((iTimeStart.QuadPart == -1) && (iTimeEnd.QuadPart == -1)) {
      QueryPerformanceCounter (&iTimeEnd);

      // 60 seconds
      iTimeStart.QuadPart = iTimeEnd.QuadPart - m_iPerformanceFrequency.QuadPart * 60;
   }

   // set this
   EnterCriticalSection (&m_CritSecTTS);
   m_iTTSMuteTimeStart = iTimeStart;
   m_iTTSMuteTimeEnd = iTimeEnd;
   LeaveCriticalSection (&m_CritSecTTS);
}

/*************************************************************************************
CAudioThread::ThreadProcTTS - This internal function handles the thread procedure
for TTS.

*/

#define THREAD_PRIORITY_TTS      THREAD_PRIORITY_HIGHEST     // TTS is higher than normal so is fast
// #define THREAD_PRIORITY_TTSLOW      THREAD_PRIORITY_NORMAL     // thread priority when TTS is low
// #define THREAD_PRIORITY_TTSHIGH     THREAD_PRIORITY_ABOVE_NORMAL     // thread priority when TTS is low
// BUGFIX - Change TTS thread priority from THREAD_PRIORITY_ABOVE_NORMAL to THREAD_PRIORITY_HIGHEST

void CAudioThread::ThreadProcTTS (void)
{
   // lower the thread priority here so doesn't stutter
   // BUGFIX - Lowered priority so that gpMainWindow->m_fMuteAll would get set early on
   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_TTS));
   m_fTTSLowPriority = FALSE;

   // go through TTS queue and generate
   CMem memLastGoodTTS;
   MemZero (&memLastGoodTTS);

   CMem mem;
   CBTree tTTS;   // tree of TTS used this go around
   PWSTR psz;
   BOOL fLoadedTTS = FALSE;
   while (TRUE) {
      // BUGFIX - Was waiting INFINITE, but changed:
      // If nothing is spoken, then once a second, poll some TTS memory to make sure that
      // TTS isn't cached out
      WaitForSingleObject (m_hSignalToThreadTTS, 1000);

      // if want to shut down then break
      if (m_fWantToQuit)
         break;

      // if cant load TTS yet then just loop
      if (!m_fCanLoadTTS)
         continue;

      // if get here and haven't loaded any of the TTS voices that used last
      // time then try to load them in
      if (!fLoadedTTS) {
         __int64 iSize;
         PWSTR psz, pszOrig;
         DWORD dwSize;

         fLoadedTTS = TRUE;
         psz = pszOrig = (PWSTR) gpMainWindow->m_mfUser.Load (gpszRecentTTSFile, &iSize);
         dwSize = pszOrig ? ((DWORD)iSize / sizeof(WCHAR)) : 0;
         DWORD dwTotal = dwSize;

         // BUGFIX - If muteall goes on then don't load anymore
         while ((dwSize >= 2) && !gpMainWindow->m_fMuteAll) {
            if (m_fWantToQuit)
               break;   // if want to quit while loading then stop

            // inform main page that TTS loading
            DWORD dwProgress = (dwTotal - dwSize) * 100 / dwTotal;
            dwProgress = max(dwProgress, 1);
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTTSLOADPROGRESS, 0, dwProgress);


            DWORD dwLen = (DWORD)wcslen(psz);
            if (dwLen+1 > dwSize)
               break;   // shouldnt happen

            PCMTTS pTTS = TTSCacheOpen (psz, TRUE, FALSE);
            if (pTTS)
               TTSCacheClose (pTTS);

            psz += (dwLen+1);
            dwSize -= (dwLen+1);
         } // dwSize

         // inform main page that TTS all loaded
         PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTTSLOADPROGRESS, 0, 0);
         

         if (pszOrig)
            MegaFileFree (pszOrig);
      }

      // if not muted, then poll some TTS memory
      if (!gpMainWindow->m_fMuteAll)
         TTSCacheMemoryTouch ();

      // once a second, free up some memory
      TTSCacheMemoryFree (1.0, 0.1);  // 1% of one voice every second

      while (TRUE) {
         PCMMLNode2 pNode, pNodeOrig;
         PCM3DWave pWave;
         PATWAVE pat;
         PCMTTS pTTS = NULL;

         if (m_fWantToQuit)
            break;

         EnterCriticalSection (&m_CritSecTTS);
         PTTSQUEUE pq = (PTTSQUEUE) m_lTTSQUEUE.Get(0);
         if (!pq) {
            LeaveCriticalSection (&m_CritSecTTS);
            break;
         }
         pNode = pq->pNode;
         pNodeOrig = pNode->Clone();
         pWave = pq->pWave;
         m_iTTSTime = pq->iTime;
         m_fTTSTimeIgnore = FALSE;
         _ASSERTE (m_iTTSTime.QuadPart);  // make sure have something in the time
         pat = (PATWAVE)pq->pWave->m_pUserData;
         m_lTTSQUEUE.Remove (0);

         // before leave critical section see if want to mute
         BOOL fWantToMute = gpMainWindow->m_fMuteAll;
         if ((m_iTTSTime.QuadPart > m_iTTSMuteTimeStart.QuadPart) && (m_iTTSTime.QuadPart < m_iTTSMuteTimeEnd.QuadPart))
            fWantToMute = TRUE;

         LeaveCriticalSection (&m_CritSecTTS);

         // look for the voice file
         PCMMLNode2 pSub;
         DWORD dwIndex;
         dwIndex = pNode->ContentFind (L"voice");
         pSub = NULL;
         pNode->ContentEnum (dwIndex, &psz, &pSub);
         if (!pSub)
            goto done;

         // try to find a subvoice tag
         DWORD dwIndex2 = pSub->ContentFind (L"subvoice");
         PCMMLNode2 pSubVoice = NULL;
         if (dwIndex2 != (DWORD)-1) {
            pSub->ContentEnum (dwIndex2, &psz, &pSubVoice);
            pSub->ContentRemove (dwIndex2, FALSE); // NOTE: Not really deleting
         }


         psz = MMLValueGet (pSub, L"file");
         if (!psz)
            goto done;  // no file ,so cant load

         // BUGFIX - If mute all, and have last good TTS, then use that since going
         // to mute it anyway
         if (fWantToMute)
            if ( ((PWSTR)memLastGoodTTS.p)[0] )
               psz = (PWSTR)memLastGoodTTS.p;

         // delete the voice reference and delete that so it doens't
         // appear in the tags
         PCMMLNode2 pVoiceClone = pSub;
         pNode->ContentRemove (dwIndex, FALSE); // NOTE: Not really deleting

         // isolate what said and add to the voice clone
         MemZero (&mem);
         ExtractSpokenFromSpeakTags (pNode, &mem);
         MMLValueSet (pVoiceClone, L"Spoken", (PWSTR)mem.p);

         PCMMLNode2 pDoneSpeaking = pVoiceClone->Clone();


         // load TTS
         pTTS = TTSCacheOpen (psz, FALSE, FALSE);
         if (!pTTS) {
            // inform main page that TTS loading
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTTSLOADPROGRESS, 0, 50);

            pTTS = TTSCacheOpen (psz, TRUE, FALSE);

            // inform main page that TTS all loaded
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTTSLOADPROGRESS, 0, 0);
         }
         if (!pTTS) {
            delete pNode;
            delete pNodeOrig;

            // BUGFIX - Warn user that TTS couldn't be opened
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSCRIPT, 0, (LPARAM)pVoiceClone);
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSCRIPTEND, 0, (LPARAM)pDoneSpeaking);
            pDoneSpeaking = NULL;

            // also make an error message
            PCMMLNode2 pErr, pErrDone;
            pErr = new CMMLNode2;
            // assuming succedes
            pErr->NameSet (L"voice");
            MMLValueSet (pErr, L"Spoken",
               L"The text-to-speech voice failed to load. You should reinstall CircumReality "
               L"from www.CircumReality.com to fix the problem, and/or contact the world's "
               L"development team about the problem.");
            pErrDone = pErr->Clone();

            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSCRIPT, 0, (LPARAM)pErr);
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSCRIPTEND, 0, (LPARAM)pErrDone);

            goto done;
         }

         // remember fQualityTTS for use with getting waves from server
         fp fQualityTTS = log((fp)max(1, pTTS->m_dwUnits)) / log((fp)4) -
            log((fp)2000 /* units */) / log((fp)4);
         fQualityTTS = floor(fQualityTTS + 0.5);
         fQualityTTS = max(fQualityTTS, 0.0);

         // remember last good TTS
         if (psz != memLastGoodTTS.p) {
            MemZero (&memLastGoodTTS);
            MemCat (&memLastGoodTTS, psz);
         }

         if (-1 == tTTS.FindNum(psz))
            tTTS.Add (psz, 0, 0);   // keep tree of TTS used this time

         // get the ID, since might be blacklisted
         GUID gID;
         if  (pSub && (sizeof(gID) != MMLValueGetBinary (pSub, L"ID", (PBYTE)&gID, sizeof(gID))))
            gID = GUID_NULL;


         // put the subvoice tag back in
         if (pSubVoice)
            pNode->ContentInsert (0, pSubVoice);

         // convert this to a string
         mem.m_dwCurPosn = 0;

         // prepend tags to speak faster
         // NOTE: need to keep blacklist here because ends up sending a transcript of speech up
         int iSpeed;
         BOOL fBlacklist = FALSE;
         DWORD dwItem;
         GUID *pgItem;
         EnterCriticalSection (&gpMainWindow->m_crSpeakBlacklist);
         iSpeed = gpMainWindow->m_iSpeakSpeed;

         // see if it's blacklisted while in critical seciton
         pgItem = (GUID*)gpMainWindow->m_lSpeakBlacklist.Get(0);
         for (dwItem = 0; dwItem < gpMainWindow->m_lSpeakBlacklist.Num(); dwItem++, pgItem++)
            if (IsEqualGUID(*pgItem, gID)) {
               fBlacklist = TRUE;
               iSpeed = 0; // might as well clear
            }

         // BUGFIX - If mute all then clear
         if (fWantToMute) {
            fBlacklist = TRUE;
            iSpeed = 0; // might as well clear
         }
         LeaveCriticalSection (&gpMainWindow->m_crSpeakBlacklist);


         // pull out quality, if there is one
         int iQualityOrig = -1;
         if (pNodeOrig->AttribGetInt (L"quality", &iQualityOrig))
            pNodeOrig->AttribDelete (L"quality");
         else
            iQualityOrig = -1;

         // if the quality isn't good enough then pretend is nothing
         if ((iQualityOrig >= 0) && (iQualityOrig < (int)fQualityTTS))
            iQualityOrig = -1;

         // get the speaking speed
         int iSpeedOrig = -100;
         if (!pNodeOrig->AttribGetInt (L"speed", &iSpeedOrig))
            iSpeedOrig = -100;
         // if the speaking speed isn't what we want then set it
         if (iSpeedOrig != iSpeed) {
            pNodeOrig->AttribSetInt (L"speed", iSpeed);
            iQualityOrig = -1;   // no matter what, dont speak this since wrong speed
         }

         // if not registered, or have audio turned off, then ignore
         if (!gfRenderCache || !RegisterMode())
            iQualityOrig = -1;

#define PROSODYSTRING   L"<prosody "       // prefix for prosody
#define LOWERVOLUME     L" volume=75>"     // this tag reduces the volume of TTS so that shouting wont clip
               // BUGFIX - Was volume=50, but changed to volume=75 since shouting is rare, and
               // notebooks don't have large volume range
         switch (iSpeed) {
            case 2:
               mem.StrCat (PROSODYSTRING L"rate=x-fast"  LOWERVOLUME);
               break;
            case 1:
               mem.StrCat (PROSODYSTRING L"rate=fast" LOWERVOLUME);
               break;
            case -1:
               mem.StrCat (PROSODYSTRING L"rate=slow" LOWERVOLUME);
               break;
            case -2:
               mem.StrCat (PROSODYSTRING L"rate=x-slow" LOWERVOLUME);
               break;
            default:
               iSpeed = 0;
               mem.StrCat (PROSODYSTRING LOWERVOLUME);
               break;
         }

         if (fBlacklist) {
            // blacklisted, so just put in a short silence
            mem.StrCat (L"<break time=x-small/>");

            // since just speaking very quick silence, ignore calls to exit
            m_fTTSTimeIgnore = TRUE;

            // don't bother with cache
            iQualityOrig = -1;
         }
         else {
            // speak it
            if (!MMLToMem (pNode, &mem, TRUE)) {
               delete pVoiceClone;
               delete pDoneSpeaking;
               pDoneSpeaking = NULL;
               goto done;
            }
         }

         // end speak speed
         //if (iSpeed) { - BUGFIX - always sticking in prosody so that can have quieter
            // BUGFIX - Remove NULL terminates at the end so can append
            while (((PWSTR)mem.p + (mem.m_dwCurPosn/sizeof(WCHAR)-1))[0] == 0)
               mem.m_dwCurPosn -= sizeof(WCHAR);
            mem.StrCat (L"</prosody>");
         //}

         mem.CharCat (0);

#ifdef _DEBUG
         OutputDebugStringW (L"\r\n");
         OutputDebugStringW ((PWSTR)mem.p);
#endif

         PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSCRIPT, 0, (LPARAM)pVoiceClone);

         // consider waiting for audio to arrive
         if (iQualityOrig >= 0) {
            CMem memString;
            if (!MMLToMem (pNodeOrig, &memString))
               goto nothingfromserver;
            memString.CharCat (0);  // just to null terminate
            PWSTR pszStringMem = (PWSTR)memString.p;

            DWORD dwWaitTime;
            // wait up to a second for the audio to arrive. If it takes longer
            // then that then synthesize
            PCM3DWave pWaveServer = NULL;
            for (dwWaitTime = 0; dwWaitTime < 10; dwWaitTime++) {
               pWaveServer = gpMainWindow->GetTTSCacheWave (pszStringMem);
               
               // may have found it
               if (pWaveServer)
                  break;

               // if not, sleep for 100 ms and re-try
               Sleep (100);
            } // dwWaitTimes

            if (pWaveServer) {
               if (pWave->ReplaceSection (0, pWave->m_dwSamples, pWaveServer)) {
#if 0 // def _DEBUG  // hack to test
                  DWORD dwSample;
                  for (dwSample = 0; dwSample < pWave->m_dwSamples; dwSample++)
                     pWave->m_psWave[dwSample] /= 10;
#endif
               }
               else
                  iQualityOrig = -1;
               delete pWaveServer;
            }
            else
               iQualityOrig = -1;
         }
nothingfromserver:

         if (iQualityOrig >= 0) {
            _ASSERTE (pat == (PATWAVE)pWave->m_pUserData);

            EnterCriticalSection (&pat->CritSec);

            DWORD dwSampleExpect = pWave->m_dwSamples;
            DWORD dwSampleValid = pWave->m_dwSamples;

            // see if want to quite
            //BOOL fWantToQuit = m_fWantToQuit;
            //if (!m_fTTSTimeIgnore && (m_iTTSTime.QuadPart > m_iTTSMuteTimeStart.QuadPart) && (m_iTTSTime.QuadPart < m_iTTSMuteTimeEnd.QuadPart)) {
            //   fWantToQuit = TRUE;
            //   dwSampleExpect = dwSampleValid;
            //}

            // indicate where we've processed to
            pat->dwSampleValid = dwSampleValid;
            pat->dwSampleExpected = dwSampleExpect;

            LeaveCriticalSection (&pat->CritSec);
         }
         else {   // TTS
            // lower the thread priority here so doesn't stutter
            SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_TTS));
            m_fTTSLowPriority = TRUE;


            m_pWaveTTS = pWave;
            BOOL fSynSuccess = pTTS->SynthGenWave (NULL, pWave->m_dwSamplesPerSec,
               (PWSTR)mem.p, TRUE, gpMainWindow->m_iTTSQuality, gpMainWindow->m_fDisablePCM, NULL, NULL, this);
            m_pWaveTTS = NULL;

#if 0 // to test def _DEBUG
            HMMIO hmmio = mmioOpen ("c:\\temp\\Testing.wav", NULL, MMIO_WRITE | MMIO_CREATE | MMIO_EXCLUSIVE );
            if (hmmio) {
               pWave->Save (TRUE, NULL, hmmio);
               mmioClose (hmmio, 0);
            }
#endif
            
            // lower the thread priority here so doesn't stutter
            //if (m_fTTSLowPriority) {
            //   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_TTSHIGH));
            //   m_fTTSLowPriority = FALSE;
            //}

            // notify when done
            // BUGFIX - Make this notification as part of the wave playback
            //PostMessage (gpMainWindow->m_hWnd, WM_MAINWINDOWNOTIFYSTOPSPEAKING, 0, 0);

            // if succeded speaking, then potentially cache this wave
            // wave must also be more than 1 sec (which keeps the "." pauses from being cached on startup
            if (fSynSuccess && (pWave->m_dwSamples >= pWave->m_dwSamplesPerSec) )
               gpMainWindow->OfferWaveToServer (pNodeOrig, pWave, (DWORD)fQualityTTS);
         } // if TTS


         // free up the node
         delete pNode;
         delete pNodeOrig;

done:
         if (pTTS)
            TTSCacheClose (pTTS);

         BOOL fCritSec = pat->fUseCritical;
         EnterCriticalSection (&m_CritSecTTS);
         if (fCritSec)
            EnterCriticalSection (&pat->CritSec);
         if (!m_fTTSTimeIgnore) {
            if ((m_iTTSTime.QuadPart > m_iTTSMuteTimeStart.QuadPart) && (m_iTTSTime.QuadPart < m_iTTSMuteTimeEnd.QuadPart))
               // if asked to quit, stop playing now, dont process any futher
               pWave->m_dwSamples = min(pWave->m_dwSamples, pat->dwSampleProcessTo);

            pat->iTTSTime = m_iTTSTime;
         }
         pat->dwSampleExpected = pat->dwSampleValid = pWave->m_dwSamples;
         pat->fFinishedLoading = TRUE;

         int iWait = ((int)pWave->m_dwSamples - (int)pat->dwSampleProcessTo) / (int)(pWave->m_dwSamplesPerSec / 1000);
         iWait = max(iWait, 0);

         if (fCritSec)
            LeaveCriticalSection (&pat->CritSec);
         pat->fUseCritical = FALSE;
         LeaveCriticalSection (&m_CritSecTTS);

         // sleep, waiting for entire sentence to stop speaking, so don't post the done message
         // too soon
         if (iWait)
            Sleep ((DWORD)iWait);

         // post done speaking
         if (pDoneSpeaking)
            // make sure to notify when done
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYTRANSCRIPTEND, 0, (LPARAM)pDoneSpeaking);
            // NOTE: Don't delete because posted it
      }
   }


   // shut down the TTS cache
   TTSCacheShutDown ();

   // clear tts queue
   EnterCriticalSection (&m_CritSecTTS);
   PTTSQUEUE pq = (PTTSQUEUE) m_lTTSQUEUE.Get(0);
   DWORD i;
   for (i = 0; i < m_lTTSQUEUE.Num(); i++, pq++) {
      delete pq->pNode;
      PATWAVE pat = (PATWAVE)pq->pWave->m_pUserData;

      BOOL fCritSec = pat->fUseCritical;
      if (fCritSec)
         EnterCriticalSection (&pat->CritSec);
      pat->dwSampleExpected = pat->dwSampleValid;
      pat->fFinishedLoading = TRUE;
      if (fCritSec)
         LeaveCriticalSection (&pat->CritSec);
      pat->fUseCritical = FALSE;
   } // i
   m_lTTSQUEUE.Clear();
   LeaveCriticalSection (&m_CritSecTTS);

   // save a list of TTS voices used
   mem.m_dwCurPosn = 0;
   for (i = 0; i < tTTS.Num(); i++) {
      psz = tTTS.Enum (i);
      mem.StrCat (psz, (DWORD)wcslen(psz)+1);
   } // i
   if (mem.m_dwCurPosn)
      gpMainWindow->m_mfUser.Save (gpszRecentTTSFile, mem.p, mem.m_dwCurPosn);
}



/*************************************************************************************
CAudioThread::ThreadProc - This internal function handles the thread procedure.
It first creates the packet sending object and tries to log on. If it fails
it sets m_fConnected to FALSE and sets a flag. If it succedes it sets m_fConnected
to true, and loops until m_hSignalToThread is set.
*/
void CAudioThread::ThreadProc (void)
{
   m_fConnected = FALSE;

   // create a window for the server's "request connect"...
   WNDCLASS wc;
   memset (&wc, 0, sizeof(wc));
   wc.hInstance = ghInstance;
   wc.lpfnWndProc = AudioThreadWndProc;
   wc.lpszClassName = "CircumrealityAudioThread"; // NOTE: Server class
   RegisterClass (&wc);
   m_hWnd = CreateWindow (
      wc.lpszClassName, wc.lpszClassName,  // NOTE: Server name
      0, 0,0,0,0,
      NULL, NULL, ghInstance, (PVOID) this);
   if (!m_hWnd) {
      m_fConnected = FALSE;
      SetEvent (m_hSignalFromThread);
      return;
   }

   DWORD i;

#ifdef USEDIRECTX
   HRESULT hRes;
   hRes = DirectSoundCreate8 (&DSDEVID_DefaultPlayback, &m_pDirectSound, NULL);
   if (FAILED(hRes))
      m_pDirectSound = NULL; // just to make sure
   if (m_pDirectSound) {
      hRes = m_pDirectSound->SetCooperativeLevel (gpMainWindow->m_hWndPrimary, DSSCL_PRIORITY );

      if (!m_bufDrone.Init (m_pDirectSound, AUDIOTHREAD_SAMPLESPERSEC, 1, FALSE)) {
         m_pDirectSound->Release();
         m_pDirectSound = NULL;
      }

      m_bufDrone.SetFormatGlobal ();
   }
   else if (gpMainWindow)  // had an error with creating direct sound, so error
      gpMainWindow->DirectSoundError();

#else
      // keep the buffers small so can hear any changes that make right away
   if (!m_memPlay.Required (AUDIOTHREAD_BUFSIZE * WVPLAYBUF)) {
      m_fConnected = FALSE;
      SetEvent (m_hSignalFromThread);
      return;
   }

   // create the wave
   MMRESULT mm;
   WAVEFORMATEX WFEX;
   memset (&WFEX, 0, sizeof(WFEX));
   WFEX.cbSize = 0;
   WFEX.wFormatTag = WAVE_FORMAT_PCM;
   WFEX.nChannels = AUDIOTHREAD_CHANNELS;
   WFEX.nSamplesPerSec = AUDIOTHREAD_SAMPLESPERSEC;
   WFEX.wBitsPerSample = 16;
   WFEX.nBlockAlign  = WFEX.nChannels * WFEX.wBitsPerSample / 8;
   WFEX.nAvgBytesPerSec = WFEX.nBlockAlign * WFEX.nSamplesPerSec;
   mm = waveOutOpen (&m_hWaveOut, WAVE_MAPPER, &WFEX, (DWORD_PTR) m_hWnd,
      0, CALLBACK_WINDOW);
   if (mm) {
      DestroyWindow (m_hWnd);
      m_fConnected = FALSE;
      SetEvent (m_hSignalFromThread);
      return;
   }

   // prepare the headers
   memset (m_aPlayWaveHdr, 0, sizeof(m_aPlayWaveHdr));
   for (i = 0; i < WVPLAYBUF; i++) {
      m_aPlayWaveHdr[i].dwBufferLength = AUDIOTHREAD_BUFSIZE;
      m_aPlayWaveHdr[i].lpData = (PSTR) ((PBYTE) m_memPlay.p + i * AUDIOTHREAD_BUFSIZE);
      mm = waveOutPrepareHeader (m_hWaveOut, &m_aPlayWaveHdr[i], sizeof(m_aPlayWaveHdr[i]));
   }

   // write them out
   waveOutPause(m_hWaveOut);
   for (i = 0; i < WVPLAYBUF; i++)
      PlayAddBuffer (&m_aPlayWaveHdr[i]);
   waveOutRestart (m_hWaveOut);
#endif

   // set up Midi
   m_pMidiMix = new CMidiMix;

   // set up TTS
   DWORD dwID;
   m_hThreadTTS = CreateThread (NULL, ESCTHREADCOMMITSIZE, TTSThreadProc, this, 0, &dwID);
   SetThreadPriority (m_hThreadTTS, VistaThreadPriorityHack(THREAD_PRIORITY_TTS));
      // BUGFIX - Make sure TTS is above-normal priority so dont get skips
      // BUGFIX - Raise and lower thread priority when speaking
      // BUGFIX - Use TTS thread priority


   // else, it went through...
   m_fConnected = TRUE;
   SetEvent (m_hSignalFromThread);

   // wait, either taking message from the queue, or an event, or doing processing
   MSG msg;
   while (TRUE) {
      // handle message queue
      while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
         TranslateMessage (&msg);
         DispatchMessage(&msg); 
      }

      // wait for a signalled semaphore, or 50 millisec, so can see if any new message
      if (WAIT_OBJECT_0 == WaitForSingleObject (m_hSignalToThread, 25))
         break;   // just received notification that should quit

      // else, 25 milliseconds has ellapsed, so repeat and see if any new messages
      // in queue
   }

   // delete all the ambient sounds
   PCAmbient *ppa = (PCAmbient*) m_lPCAmbient.Get(0);
   for (i = 0; i < m_lPCAmbient.Num(); i++)
      delete ppa[i];
   m_lPCAmbient.Clear();

   // need to shut down any audio
#ifndef USEDIRECTX
   if (m_hWaveOut) {
      waveOutReset (m_hWaveOut);

      for (i = 0; i < WVPLAYBUF; i++)
         waveOutUnprepareHeader (m_hWaveOut, &m_aPlayWaveHdr[i], sizeof(m_aPlayWaveHdr[i]));

      waveOutClose (m_hWaveOut);

      m_hWaveOut = NULL;
   }
#endif

   if (m_hWnd)
      DestroyWindow (m_hWnd);
   m_hWnd = NULL;

   // shut down TTS
   m_fWantToQuit = TRUE;   // make sure want to quit is set
   if (m_hThreadTTS) {
      SetEvent (m_hSignalToThreadTTS);

      // wait
      WaitForSingleObject (m_hThreadTTS, INFINITE);

      // delete all
      CloseHandle (m_hThreadTTS);
   }

   // delete all the audio queues
   EnterCriticalSection (&m_CritSecQueue);
   PCAudioQueue *paq = (PCAudioQueue*)m_lPCAudioQueue.Get(0);
   for (i = 0; i < m_lPCAudioQueue.Num(); i++)
      delete paq[i];
   m_lPCAudioQueue.Clear();
   LeaveCriticalSection (&m_CritSecQueue);

   // delete all the waves
   EnterCriticalSection (&m_CritSecWave);
   PCM3DWave *ppw = (PCM3DWave*)m_lPCM3DWave.Get(0);
   for (i = 0; i < m_lPCM3DWave.Num(); i++)
      WaveDelete (ppw[i]);
   m_lPCM3DWave.Clear();
   LeaveCriticalSection (&m_CritSecWave);

   // delete all the midi files
   EnterCriticalSection (&m_CritSecWave);
   PCMidiFile *ppmf = (PCMidiFile*)m_lPCMidiFile.Get(0);
   for (i = 0; i < m_lPCMidiFile.Num(); i++)
      MidiDelete (ppmf[i]);
   m_lPCMidiFile.Clear();
   LeaveCriticalSection (&m_CritSecWave);

   // free up midi
   if (m_pMidiInstanceEscarpment)
      delete m_pMidiInstanceEscarpment;
   m_pMidiInstanceEscarpment = NULL;
   if (m_pMidiMix)
      delete m_pMidiMix;
   m_pMidiMix = NULL;

#ifdef USEDIRECTX
   m_bufDrone.Release();
   
   if (m_pDirectSound)
      m_pDirectSound->Release();
#endif

   // all done
}


/*************************************************************************************
CAudioThread::Connect- This is an initialization function. It creates the thread.
If it fails it returns FALSE. If it succedes, it returns
TRUE.

NOTE: Call this from the MAIN thread.

inputs
   HWND           hWndNotify - When an image is completed, this will receive a post
                     indicating there's a new image. wParam contains the message
                     type of BUGUBG
   DWORD          dwMessage - This is the WM_ message that's posted to indicate the
                     new message.
returns
   BOOL - TRUE if connected and thread created. FALSE if failed to connect and
      no thread created.
*/
BOOL CAudioThread::Connect (HWND hWndNotify, DWORD dwMessage)
{
   if (m_hThread)
      return FALSE;  // cant call a second time

   m_hWndNotify = hWndNotify;
   m_dwMessage = dwMessage;
   m_fConnected = FALSE;

   // create all the events
   m_hSignalFromThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   m_hSignalToThread = CreateEvent (NULL, FALSE, FALSE, NULL);
   DWORD dwID;
   m_hThread = CreateThread (NULL, ESCTHREADCOMMITSIZE, AudioThreadProc, this, 0, &dwID);
   if (!m_hThread) {
      CloseHandle (m_hSignalFromThread);
      CloseHandle (m_hSignalToThread);
      m_hSignalFromThread = m_hSignalToThread = NULL;
      return FALSE;
   }
   // BUGFIX - Make this highest so dont get audio skips
   SetThreadPriority (m_hThread, VistaThreadPriorityHack(THREAD_PRIORITY_HIGHEST));

   // wait for the signal from thread, to know if initialization succeded
   WaitForSingleObject (m_hSignalFromThread, INFINITE);
   if (!m_fConnected) {
      // error. which means thread it shutting down. wait for it
      WaitForSingleObject (m_hThread, INFINITE);
      CloseHandle (m_hThread);
      CloseHandle (m_hSignalFromThread);
      CloseHandle (m_hSignalToThread);
      m_hSignalFromThread = m_hSignalToThread = NULL;
      m_hThread = NULL;
      return FALSE;
   }

   // else, all ok
   return TRUE;
}


/*************************************************************************************
CAudioThread::StartDisconnecting - Starts the audiothread shutdown.
*/
BOOL CAudioThread::StartDisconnect (void)
{
   if (!m_hThread)
      return FALSE;

   m_fWantToQuit = TRUE;

   // signal
   SetEvent (m_hSignalToThread);

   return TRUE;
}

/*************************************************************************************
CAudioThread::Disconnect - Call this to cancel a connection made by Connect().
This is autoamtically called if the CAudioThread object is deleted.
It also deletes the thread it creates.
*/
BOOL CAudioThread::Disconnect (void)
{
   if (!StartDisconnect())
      return FALSE;
   //if (!m_hThread)
   //   return FALSE;

   //m_fWantToQuit = TRUE;

   // signal
   //SetEvent (m_hSignalToThread);

   // wait
   WaitForSingleObject (m_hThread, INFINITE);

   // delete all
   CloseHandle (m_hThread);
   CloseHandle (m_hSignalFromThread);
   CloseHandle (m_hSignalToThread);
   m_hSignalFromThread = m_hSignalToThread = NULL;
   m_hThread = NULL;

   return TRUE;
}



/*************************************************************************************
CAudioThread::NewQueue - Creates a new CAudioQueue object and adds it to the queue.

inputs
   BOOL           fUseCritSec - If TRUE use the critical section for the queue.
returns
   PCAudioQueue - New audio queue object (also added to the lsit), or NULL
*/
PCAudioQueue CAudioThread::NewQueue (BOOL fUseCritSec)
{
   PCAudioQueue pq = new CAudioQueue (this);
   if (!pq)
      return NULL;

   if (fUseCritSec)
      EnterCriticalSection (&m_CritSecQueue);

   m_lPCAudioQueue.Add (&pq);

   if (fUseCritSec)
      LeaveCriticalSection (&m_CritSecQueue);
   return pq;
}

/*************************************************************************************
CAudioThread::RequestAudio - Call this to request that a command from the
server be played.

THREAD SAFE - Call from any thread.

inputs
   PCMMLNode2         pNode - This is USED by the call, and afterwards should
                        NOT be used by the caller.
   LARGE_INTEGER     iTime - Timestamp, from QueryPerformanceCounter()
   BOOL              fMainQueue - If TRUE uses the main queue, if FALSE adds.
   BOOL              fUseCritSec - If TRUE then use the critical section,
                           m_CritSecQueue. This should be TRUE unless already
                           calling from within a function that's known to be
                           in the critical section.
returns
   BOOL - TRUE if success, FALSE if failed (but node deleted)
*/
BOOL CAudioThread::RequestAudio (PCMMLNode2 pNode, LARGE_INTEGER iTime, BOOL fMainQueue, BOOL fUseCritSec)
{
   BOOL fRet = TRUE;

   // if it shouldn't be going into the audio queue then pass it on
   // NOTE: if any node is marked for the main queue then put it in so it
   // gets activated at the appropriate time
   PWSTR psz = pNode->NameGet();
   if (!psz)
      psz = L"";
   if (!fMainQueue && !( 
      !_wcsicmp(psz, CircumrealityWave()) ||
      !_wcsicmp(psz, CircumrealityMusic()) ||
      !_wcsicmp(psz, CircumrealitySilence()) ||
      !_wcsicmp(psz, CircumrealitySpeak()) ||
      !_wcsicmp(psz, CircumrealityDelay()) ||  // BUGFIX - added this so would add to queue
      !_wcsicmp(psz, CircumrealityAmbientSounds()) ||
      !_wcsicmp(psz, CircumrealityAmbientLoopVar()) ||
      !_wcsicmp(psz, CircumrealityQueue()) )) {
      // else, unknown, so send it on

      // send a render request just in case
      if (gpMainWindow->m_pRT)
         gpMainWindow->m_pRT->RequestRender (pNode, RTQUEUE_MEDIUM, -1, FALSE);  // BUGFIX - Remove ->Clone()

      // post it into the main thread
      if (m_hWndNotify) {
         // put in a time/date stamp
         pNode->AttribSetBinary (L"time", &iTime, sizeof(iTime));

         PostMessage (m_hWndNotify, m_dwMessage, CIRCUMREALITYPACKET_MMLIMMEDIATE, (LPARAM)pNode);
      }
      return TRUE;
   }

   // else, it's something that's either added to the main queue, or an adjunct

   // make sure to pass to a render request
   if (gpMainWindow->m_pRT)
      gpMainWindow->m_pRT->RequestRender (pNode, fMainQueue ? RTQUEUE_LOW : RTQUEUE_HIGH, -1, FALSE);   // BUGFIX - Remove ->Clone()

   if (fUseCritSec)
      EnterCriticalSection (&m_CritSecQueue);

   // else, it's something that should be added to the queue
   PCAudioQueue pq;
   if (!m_lPCAudioQueue.Num()) {
      // always at least one queue element, which is the main queue
      pq = NewQueue(FALSE);
      if (!pq) {
         delete pNode;
         goto done;
      }
   }

   if (!m_lPCAudioQueue.Num() || !fMainQueue) {
      // create a new queue and add it... do this if there arent any queues
      // at all, or if this isn't designed for the main queue
      pq = NewQueue(FALSE);
      if (!pq) {
         delete pNode;
         goto done;
      }
   }
   else {
      // put it in the main queue
      pq = *((PCAudioQueue*)m_lPCAudioQueue.Get(0));
   }

   // append to the end
   pq->QueueAdd (pNode, NULL, iTime, FALSE);

done:
   if (fUseCritSec)
      LeaveCriticalSection (&m_CritSecQueue);
   return fRet;
}


static PWSTR gpszID = L"ID";

/*************************************************************************************
CAudioThread::VoiceChat - Called if there's an incoming packet of voice chat

THREAD SAFE - Call from any thread.

inputs
   PCMMLNode2        pNode - This is USED by the call, and afterwards should
                        NOT be used by the caller. It contains specific information
                        about the voice chat.
   PCMem             pMem - Memory containing compressed voice chat inforamtion.
                        m_dwCurPosn must be set to the limit of the compressed
                        data. This is freed up too.
   LARGE_INTEGER     iTime - Time stamp
   BOOL              fUseCritSec - If TRUE then use the critical section,
                           m_CritSecQueue. This should be TRUE unless already
                           calling from within a function that's known to be
                           in the critical section.
returns
   BOOL - TRUE if success, FALSE if failed (but node deleted)
*/
BOOL CAudioThread::VoiceChat (PCMMLNode2 pNode, PCMem pMem, LARGE_INTEGER iTime, BOOL fUseCritSec)
{
   BOOL fRet = TRUE;

   // get the voice from pNode
   GUID gID;
   if (sizeof(gID) != MMLValueGetBinary (pNode, gpszID, (PBYTE)&gID, sizeof(gID)))
      gID = GUID_NULL;

   // make sure the character's voice isn't muted
   if (IsBlacklisted (&gID)) {
      delete pNode;
      delete pMem;
      return FALSE;
   }

   // make sure the data is valid
   if (!VoiceChatDeCompress ((PBYTE)pMem->p, (DWORD)pMem->m_dwCurPosn, NULL, NULL)) {
      delete pNode;
      delete pMem;
      return FALSE;
   }

   if (fUseCritSec)
      EnterCriticalSection (&m_CritSecQueue);

   // try to find a match in the queue
   PCAudioQueue *ppq = (PCAudioQueue*)m_lPCAudioQueue.Get(0);
   PCAudioQueue pq;
   DWORD i;
   for (i = 0; i < m_lPCAudioQueue.Num(); i++) {
      pq = ppq[i];

      if (pq->m_fVoiceChat && IsEqualGUID (pq->m_gVoiceChat, gID))
         break;
   } // i
   if (i >= m_lPCAudioQueue.Num()) {
      // add it
      pq = NewQueue (FALSE);
      if (!pq) {
         fRet = FALSE;
         delete pNode;
         delete pMem;
         goto done;
      }

      pq->m_fVoiceChat = TRUE;
      pq->m_gVoiceChat = gID;
   }

   // add this work item to the queue
   pq->QueueAdd (pNode, pMem, iTime, FALSE);

done:
   if (fUseCritSec)
      LeaveCriticalSection (&m_CritSecQueue);
   return fRet;
}



/*************************************************************************************
CAudioThread::WaveCreateAlways - Creates a wave file and adds it to the list of
queued waves. The reference count it set to 1. It is added to the wave list.

inputs
   PWSTR             pszName - Name. Use blank to indicate unique
   BOOL              fDelWhenNoRef - If TRUE, this wave can be deleted as soon
                     as there's no reference count, FALSE it should be kept around
                     for awhile because it might be used.
   PCMem             pMemVoiceChat - Memory for voice chat. This will be freed
                     by this function. For most waves this is NULL.
   PCVoiceDisguise   pVoiceDisguise - Used with the voice chat. Not freed by this call.
returns
   PCM3DWave         pWave - Wave, or NULL if error
*/
PCM3DWave CAudioThread::WaveCreateAlways (PWSTR pszName, BOOL fDelWhenNoRef, PCMem pMemVoiceChat,
                                          PCVoiceDisguise pVoiceDisguise)
{
   DWORD dwLen = ((DWORD)wcslen(pszName) + 1) * sizeof(WCHAR);

   PCM3DWave pNew = new CM3DWave;
   if (!pNew) {
      if (pMemVoiceChat)
         delete pMemVoiceChat;
      return NULL;
   }

   PATWAVE pat = (PATWAVE) ESCMALLOC (sizeof(ATWAVE));
   if (!pat || (dwLen > sizeof(pat->szName))) {
      delete pNew;
      if (pMemVoiceChat)
         delete pMemVoiceChat;
      return NULL;
   }
   pNew->m_pUserData = pat;

   memset (pat, 0, sizeof(*pat));
   memcpy (pat->szName, pszName, dwLen);
   pat->dwRefCount = 1;
   pat->fDelWhenNoRef = fDelWhenNoRef;
   pat->pThis = this;
   pat->fUseCritical = TRUE;
   pat->iLastUsed = m_iTime;
   pat->fFinishedLoading = FALSE;
   pat->dwSampleValid = 0;
   pat->dwSampleProcessTo = AUDIOTHREAD_LOOKAHEADSAMP;
   pat->dwSampleExpected = -1;
   pat->pMemVoiceChat = pMemVoiceChat;
   pat->pVoiceDisguise = pVoiceDisguise;
   InitializeCriticalSection (&pat->CritSec);

   // add to the wave list
   EnterCriticalSection (&m_CritSecWave);
   WaveFreeOld (FALSE); // free old ones
   m_lPCM3DWave.Add (&pNew);
   LeaveCriticalSection (&m_CritSecWave);

   return pNew;
}


/*************************************************************************************
CAudioThread::MidiCreateAlways - Creates a Midi file and adds it to the list of
queued Midis. The reference count it set to 1. It is added to the Midi list.

inputs
   PWSTR             pszName - Name. Use blank to indicate unique
   BOOL              fDelWhenNoRef - If TRUE, this Midi can be deleted as soon
                     as there's no reference count, FALSE it should be kept around
                     for awhile because it might be used.
returns
   PCMidiFile         pMidi - Midi, or NULL if error
*/
PCMidiFile CAudioThread::MidiCreateAlways (PWSTR pszName, BOOL fDelWhenNoRef)
{
   DWORD dwLen = ((DWORD)wcslen(pszName) + 1) * sizeof(WCHAR);

   PCMidiFile pNew = new CMidiFile;
   if (!pNew)
      return NULL;

   PATWAVE pat = (PATWAVE) ESCMALLOC (sizeof(ATWAVE));
   if (!pat || (dwLen > sizeof(pat->szName))) {
      delete pNew;
      return NULL;
   }
   pNew->m_pUserData = pat;

   memset (pat, 0, sizeof(*pat));
   memcpy (pat->szName, pszName, dwLen);
   pat->dwRefCount = 1;
   pat->fDelWhenNoRef = fDelWhenNoRef;
   pat->pThis = this;
   pat->fUseCritical = TRUE;
   pat->iLastUsed = m_iTime;
   pat->fFinishedLoading = FALSE;
   pat->dwSampleValid = 0;
   pat->dwSampleProcessTo = AUDIOTHREAD_LOOKAHEADSAMP;
   pat->dwSampleExpected = -1;
   InitializeCriticalSection (&pat->CritSec);

   // add to the wave list
   EnterCriticalSection (&m_CritSecWave);
   MidiFreeOld (FALSE); // free old ones
   m_lPCMidiFile.Add (&pNew);
   LeaveCriticalSection (&m_CritSecWave);

   return pNew;
}

/*************************************************************************************
CAudioThread::TTSWaveData - Called for TTS. Standard API
*/
BOOL CAudioThread::TTSSpeedVolume (fp *pafSpeed, fp *pafVolume)
{
   // do nothing
   return TRUE;
}

/*************************************************************************************
CAudioThread::TTSWaveData - Called for TTS. Standard API
*/
BOOL CAudioThread::TTSWaveData (PCM3DWave pWave)
{
   // if want to quit return right away
   if (m_fWantToQuit || !m_pWaveTTS)
      return FALSE;

   // the first time get a call from TTS, increase the priority so doesn skip
   //if (m_fTTSLowPriority) {
   //   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_TTSHIGH));
   //   m_fTTSLowPriority = FALSE;
   //}

   PATWAVE pat = (PATWAVE)m_pWaveTTS->m_pUserData;

   EnterCriticalSection (&m_CritSecQueue);

   EnterCriticalSection (&pat->CritSec);

   // concatenate this onto the wave
   BOOL fWantToQuit = m_fWantToQuit;
   m_pWaveTTS->AppendWave (pWave);  // ignore error
   // update valid samples point
   DWORD dwSampleValid = m_pWaveTTS->m_dwSamples;
   DWORD dwSampleExpect = -1; // since still don't know

   // see if want to quite
   if (!m_fTTSTimeIgnore && (m_iTTSTime.QuadPart > m_iTTSMuteTimeStart.QuadPart) && (m_iTTSTime.QuadPart < m_iTTSMuteTimeEnd.QuadPart)) {
      fWantToQuit = TRUE;
      dwSampleExpect = dwSampleValid;
   }

   // indicate where we've processed to
   // BUGFIX - Only update sample valid if we've gone beyond the minimum processing
   if (dwSampleValid >= pat->dwSampleProcessTo)
      pat->dwSampleValid = dwSampleValid;
   pat->dwSampleExpected = dwSampleExpect;


#if 0 // don't check pat->dwSampleProcessTo here. Just finish synthesizing this phrase
   // BUGFIX - It dwSampleValid == dwSampleExpect, then dont sit around and wait because done processing it all
   while ((dwSampleValid != dwSampleExpect) && (dwSampleValid > pat->dwSampleProcessTo) && !(fWantToQuit || m_fWantToQuit)) {
            // BUGFIX - Include m_fWantToQuit so dont hang on shutdown
      // need to wait
      LeaveCriticalSection (&pat->CritSec);
      LeaveCriticalSection (&m_CritSecQueue);
      Sleep (10);
      EnterCriticalSection (&m_CritSecQueue);
      EnterCriticalSection (&pat->CritSec);
   }
#endif

   LeaveCriticalSection (&pat->CritSec);
   LeaveCriticalSection (&m_CritSecQueue);
   
   // return want to quit
   // BUGFIX - Include m_fWantToQuit so dont hang on shutdown
   return !(fWantToQuit || m_fWantToQuit);
}

/*************************************************************************************
CAudioThread::Update - From CProgressWaveSample. Used to update progress and hold
off loading of wave (of construction of TTS)
*/
BOOL CAudioThread::Update (PCM3DWave pWave, DWORD dwSampleValid, DWORD dwSampleExpect)
{
   // if want to quit return right away
   if (m_fWantToQuit)
      return FALSE;

   // the first time get a call from TTS, increase the priority so doesn skip
   //if (m_fTTSLowPriority) {
   //   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_TTSHIGH));
   //   m_fTTSLowPriority = FALSE;
   //}

   PATWAVE pat = (PATWAVE)pWave->m_pUserData;

   EnterCriticalSection (&pat->CritSec);

   // see if want to quite
   BOOL fWantToQuit = m_fWantToQuit;
   if (!m_fTTSTimeIgnore && (m_iTTSTime.QuadPart > m_iTTSMuteTimeStart.QuadPart) && (m_iTTSTime.QuadPart < m_iTTSMuteTimeEnd.QuadPart)) {
      fWantToQuit = TRUE;
      dwSampleExpect = dwSampleValid;
   }

   // indicate where we've processed to
   pat->dwSampleValid = dwSampleValid;
   pat->dwSampleExpected = dwSampleExpect;

   // BUGFIX - It dwSampleValid == dwSampleExpect, then dont sit around and wait because done processing it all
   while ((dwSampleValid != dwSampleExpect) && (dwSampleValid > pat->dwSampleProcessTo) && !(fWantToQuit || m_fWantToQuit)) {
            // BUGFIX - Include m_fWantToQuit so dont hang on shutdown
      // need to wait
      LeaveCriticalSection (&pat->CritSec);
      Sleep (10);
      EnterCriticalSection (&pat->CritSec);
   }

   LeaveCriticalSection (&pat->CritSec);

   // return want to quit
   // BUGFIX - Include m_fWantToQuit so dont hang on shutdown
   return !(fWantToQuit || m_fWantToQuit);
}



/*************************************************************************************
LoadWaveThreadProc - Thread that handles loading in a wave file.
*/
DWORD WINAPI LoadWaveThreadProc(LPVOID lpParameter)
{
   PCM3DWave pWave = (PCM3DWave) lpParameter;
   PATWAVE pat = (PATWAVE)pWave->m_pUserData;
   MMIOINFO mmio;
   HMMIO hmmio = NULL;

   // see if this is a resource number
   DWORD dwResource = IsWAVResource (pat->szName);
   if (dwResource) {
      HRSRC hRes = FindResourceEx (ghInstance, "WAVE", MAKEINTRESOURCE(dwResource), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
      if (!hRes)
         goto fail;
      HGLOBAL hg = LoadResource (ghInstance, hRes);
      if (!hg)
         goto fail;
      PVOID pData = LockResource (hg);
      DWORD dwSize = SizeofResource (ghInstance, hRes);
      if (!pData || !dwSize)
         goto fail;

      memset (&mmio, 0, sizeof(mmio));
      mmio.pchBuffer = (HPSTR) pData;
      mmio.fccIOProc = FOURCC_MEM;
      mmio.cchBuffer = (DWORD) dwSize;
      hmmio = mmioOpen (NULL, &mmio, MMIO_READ);
   }

   // NOTE: This will load the wave piecemeal and do sampling rate conversion to 22kh
   char szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, pat->szName, -1, szTemp, sizeof(szTemp), 0 ,0);
   if (!pWave->Open (NULL, szTemp, TRUE, hmmio, AUDIOTHREAD_SAMPLESPERSEC, pat->pThis))
      goto fail;

   if (hmmio)
      mmioClose (hmmio, 0);

   // note that all loaded
   EnterCriticalSection (&pat->CritSec);
   pat->dwSampleExpected = pat->dwSampleValid = pWave->m_dwSamples;
   pat->fFinishedLoading = TRUE;
   LeaveCriticalSection (&pat->CritSec);
   pat->fUseCritical = FALSE; // NOTE: Have to clear after fUseCritical
   return 0;

fail:
   if (hmmio)
      mmioClose (hmmio, 0);

   // error, so all done
   EnterCriticalSection (&pat->CritSec);
   pat->dwSampleExpected = pat->dwSampleValid;  // which = 0
   pat->fFinishedLoading = TRUE;
   LeaveCriticalSection (&pat->CritSec);
   pat->fUseCritical = FALSE; // NOTE: Have to clear after fUseCritical
   return 0;
}



/*************************************************************************************
LoadVoiceChatWaveThreadProc - Thread that handles loading in a voice-chat wave file.
*/
DWORD WINAPI LoadVoiceChatWaveThreadProc(LPVOID lpParameter)
{
   PCM3DWave pWave = (PCM3DWave) lpParameter;
   PATWAVE pat = (PATWAVE)pWave->m_pUserData;

   // make sure 22 kHz
   pWave->ConvertSamplesAndChannels (AUDIOTHREAD_SAMPLESPERSEC, 1, NULL);

   // decompress and generate audio
   if (!pat->pMemVoiceChat)
      goto fail;
   if (!VoiceChatDeCompress ((PBYTE)pat->pMemVoiceChat->p, (DWORD)pat->pMemVoiceChat->m_dwCurPosn, pWave,
      pat->pVoiceDisguise, NULL, pat->pThis))
      goto fail;
         // BUBUG - If send NULL instead of pat->pThis, shows off errors with concatenation of voice chat


   // note that all loaded
   EnterCriticalSection (&pat->CritSec);
   pat->dwSampleExpected = pat->dwSampleValid = pWave->m_dwSamples;
   pat->fFinishedLoading = TRUE;
   LeaveCriticalSection (&pat->CritSec);
   pat->fUseCritical = FALSE; // NOTE: Have to clear after fUseCritical
   return 0;

fail:
   // error, so all done
   EnterCriticalSection (&pat->CritSec);
   pat->dwSampleExpected = pat->dwSampleValid;  // which = 0
   pat->fFinishedLoading = TRUE;
   LeaveCriticalSection (&pat->CritSec);
   pat->fUseCritical = FALSE; // NOTE: Have to clear after fUseCritical
   return 0;
}


/*************************************************************************************
LoadMidiThreadProc - Thread that handles loading in a Midi file.
*/
DWORD WINAPI LoadMidiThreadProc(LPVOID lpParameter)
{
   PCMidiFile pMidi = (PCMidiFile) lpParameter;
   PATWAVE pat = (PATWAVE)pMidi->m_pUserData;

   // load the wave in
   if (!pMidi->Open (pat->szName)) {
      // error, so all done
      EnterCriticalSection (&pat->CritSec);
      pat->dwSampleExpected = pat->dwSampleValid;  // which = 0
      pat->fFinishedLoading = TRUE;
      LeaveCriticalSection (&pat->CritSec);
      pat->fUseCritical = FALSE; // NOTE: Have to clear after fUseCritical
      return 0;
   }


   // note that all loaded
   DWORD dwTotal = (DWORD)(pMidi->m_fTimeTotal * (double)AUDIOTHREAD_SAMPLESPERSEC);
   dwTotal = max(dwTotal, 1);
   EnterCriticalSection (&pat->CritSec);
   pat->dwSampleExpected = pat->dwSampleValid = dwTotal;
   pat->fFinishedLoading = TRUE;
   LeaveCriticalSection (&pat->CritSec);
   pat->fUseCritical = FALSE; // NOTE: Have to clear after fUseCritical
   return 0;

}


/*************************************************************************************
CAudioThread::WaveCreate - Given a wave name, first sees if it's on the list.
If it is then that one is used. Otherwise, a new one is created (to be loaded
by a new thread).

If it's already on the list, this increases the reference count, if it's
not on the list the wave is created with a reference count of 1.

inputs
   PWSTR             pszName - Name. This is assumed to be a file name.
                     If this is NULL then a new wave is always created
returns
   PCM3DWave         pWave - Wave, or NULL if error
*/
PCM3DWave CAudioThread::WaveCreate (PWSTR pszName)
{
   DWORD i;
   if (pszName) {
      EnterCriticalSection (&m_CritSecWave);
      PCM3DWave *ppw = (PCM3DWave*)m_lPCM3DWave.Get(0);
      for (i = 0; i < m_lPCM3DWave.Num(); i++) {
         PCM3DWave pWave = ppw[i];
         PATWAVE pat = (PATWAVE)ppw[i]->m_pUserData;

         if (!_wcsicmp(pszName, pat->szName)) {
            // found match

            BOOL fUseCrit = pat->fUseCritical;
            if (fUseCrit)
               EnterCriticalSection (&pat->CritSec);

            // BUGFIX - if 0-length wave then fail. Put in to minimize chance of infinte loop
            if (pat->fFinishedLoading && !pat->dwSampleValid) {
               if (fUseCrit)
                  LeaveCriticalSection (&pat->CritSec);

               LeaveCriticalSection (&m_CritSecWave);
               return NULL;
            }

            pat->dwRefCount++;
            pat->iLastUsed = m_iTime;
            if (fUseCrit)
               LeaveCriticalSection (&pat->CritSec);

            LeaveCriticalSection (&m_CritSecWave);
            return pWave;
         }
      }
      LeaveCriticalSection (&m_CritSecWave);
   } // if pszName

   // if get here need to create
   PCM3DWave pWave = WaveCreateAlways (pszName ? pszName : L"", FALSE);
   if (!pWave)
      return NULL;

   // will need to pass to thread...
   DWORD dwID;
   HANDLE h;
   h = CreateThread (NULL, ESCTHREADCOMMITSIZE, LoadWaveThreadProc, pWave, 0, &dwID);
   if (!h) {
      delete pWave;
      return NULL;
   }
   SetThreadPriority (h, VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));
   CloseHandle (h);  // since wont need again

   return pWave;
}



/*************************************************************************************
CAudioThread::VoiceChatWaveCreate - Creates a thread that loads in voice chat
info.

inputs
   PCMem             pMem - Memory for the voice chat decompress. This will be
                     freed by this funciton.
   PCVoiceDisguise   pVoiceDisguise - To use for the voice
returns
   PCM3DWave         pWave - Wave, or NULL if error
*/
PCM3DWave CAudioThread::VoiceChatWaveCreate (PCMem pMem, PCVoiceDisguise pVoiceDisguise)
{
   // if get here need to create
   PCM3DWave pWave = WaveCreateAlways (L"", TRUE, pMem, pVoiceDisguise);
   if (!pWave)
      return NULL;

   // will need to pass to thread...
   DWORD dwID;
   HANDLE h;
   h = CreateThread (NULL, ESCTHREADCOMMITSIZE, LoadVoiceChatWaveThreadProc, pWave, 0, &dwID);
   if (!h) {
      delete pWave;
      return NULL;
   }
   SetThreadPriority (h, VistaThreadPriorityHack(THREAD_PRIORITY_ABOVE_NORMAL));
      // BUGFIX - Make sure voice chat is above-normal priority so dont get skips
   CloseHandle (h);  // since wont need again

   return pWave;
}

/*************************************************************************************
CAudioThread::MidiCreate - Given a MIDI name, first sees if it's on the list.
If it is then that one is used. Otherwise, a new one is created (to be loaded
by a new thread).

If it's already on the list, this increases the reference count, if it's
not on the list the MIDI is created with a reference count of 1.

inputs
   PWSTR             pszName - Name. This is assumed to be a file name
returns
   PCMidiFile        Midi, or NULL if error
*/
PCMidiFile CAudioThread::MidiCreate (PWSTR pszName)
{
   // Don't use the save-multiple copies of MIDI because
   // if do so the playbakc gets messed up (since playbakc part
   // of the midi instance object). Plus, dont really save
   // that much memory
   // DWORD i;
   //EnterCriticalSection (&m_CritSecWave);
   //PCMidiFile *ppw = (PCMidiFile*)m_lPCMidiFile.Get(0);
   //for (i = 0; i < m_lPCMidiFile.Num(); i++) {
   //   PCMidiFile pMidi = ppw[i];
   //   PATWAVE pat = (PATWAVE)ppw[i]->m_pUserData;
   //
   //   if (!_wcsicmp(pszName, pat->szName)) {
   //      // found match
   //
   //      BOOL fUseCrit = pat->fUseCritical;
   //      if (fUseCrit)
   //         EnterCriticalSection (&pat->CritSec);
   //      pat->dwRefCount++;
   //      pat->iLastUsed = m_iTime;
   //      if (fUseCrit)
   //         LeaveCriticalSection (&pat->CritSec);
   //
   //      LeaveCriticalSection (&m_CritSecWave);
   //      return pMidi;
   //   }
   //}
   //LeaveCriticalSection (&m_CritSecWave);

   // if get here need to create
   PCMidiFile pMidi = MidiCreateAlways (pszName, FALSE);
   if (!pMidi)
      return NULL;

   // will need to pass to thread...
   DWORD dwID;
   HANDLE h;
   h = CreateThread (NULL, ESCTHREADCOMMITSIZE, LoadMidiThreadProc, pMidi, 0, &dwID);
   if (!h) {
      delete pMidi;
      return NULL;
   }
   SetThreadPriority (h, VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));
   CloseHandle (h);  // since wont need again

   return pMidi;
}



/*************************************************************************************
CAudioThread::WaveRelease - Releases the reference count of a wave.

inputs
   PCM3DWave         pWave - Wave to release
*/
void CAudioThread::WaveRelease (PCM3DWave pWave)
{
   PATWAVE pat = (PATWAVE)pWave->m_pUserData;

   BOOL fUseCrit = pat->fUseCritical;
   if (fUseCrit)
      EnterCriticalSection (&pat->CritSec);
   if (pat->dwRefCount)
      pat->dwRefCount--;
   if (fUseCrit)
      LeaveCriticalSection (&pat->CritSec);
}


/*************************************************************************************
CAudioThread::MidiRelease - Releases the reference count of a Midi.

inputs
   PCMidiFile         pMidi - Midi to release
*/
void CAudioThread::MidiRelease (PCMidiFile pMidi)
{
   PATWAVE pat = (PATWAVE)pMidi->m_pUserData;

   BOOL fUseCrit = pat->fUseCritical;
   if (fUseCrit)
      EnterCriticalSection (&pat->CritSec);
   if (pat->dwRefCount)
      pat->dwRefCount--;
   if (fUseCrit)
      LeaveCriticalSection (&pat->CritSec);
}


/*************************************************************************************
CAudioThread::WaveDelete - This deletes a specific wave. It also frees up
the critical section and memory for the wave. NOTE: This is NOT removed from
the wave list.

inputs
   PCM3DWave         pWave - To be deleted
returns
   none
*/
void CAudioThread::WaveDelete (PCM3DWave pWave)
{
   PATWAVE pat = (PATWAVE)pWave->m_pUserData;

   // make sure it's not loading
   while (TRUE) {
      BOOL fUseCrit = pat->fUseCritical;
      BOOL fFinishedLoading;
      if (fUseCrit)
         EnterCriticalSection (&pat->CritSec);
      fFinishedLoading = pat->fFinishedLoading;
      if (fUseCrit)
         LeaveCriticalSection (&pat->CritSec);

      if (fFinishedLoading && !fUseCrit)
         break;

      // else wait and retry
      Sleep (10);
   }

   // delete
   DeleteCriticalSection (&pat->CritSec);
   if (pat->pMemVoiceChat)
      delete pat->pMemVoiceChat;
   ESCFREE (pat);
   delete pWave;
}


/*************************************************************************************
CAudioThread::MidiDelete - This deletes a specific Midi. It also frees up
the critical section and memory for the Midi. NOTE: This is NOT removed from
the Midi list.

inputs
   PCMidiFile         pMidi - To be deleted
returns
   none
*/
void CAudioThread::MidiDelete (PCMidiFile pMidi)
{
   PATWAVE pat = (PATWAVE)pMidi->m_pUserData;

   // make sure it's not loading
   while (TRUE) {
      BOOL fUseCrit = pat->fUseCritical;
      BOOL fFinishedLoading;
      if (fUseCrit)
         EnterCriticalSection (&pat->CritSec);
      fFinishedLoading = pat->fFinishedLoading;
      if (fUseCrit)
         LeaveCriticalSection (&pat->CritSec);

      if (fFinishedLoading && !fUseCrit)
         break;

      // else wait and retry
      Sleep (10);
   }

   // delete
   DeleteCriticalSection (&pat->CritSec);
   ESCFREE (pat);
   delete pMidi;
}


/*************************************************************************************
CAudioThread::WaveFreeOld - This frees up the old, unused waves.

inputs
   BOOL        fUseCritSec - if TRUE then use the critical section wrappers
                     (which is normal, unless already in the critical section)
*/
void CAudioThread::WaveFreeOld (BOOL fUseCritSec)
{
   DWORD i;
   if (fUseCritSec)
      EnterCriticalSection (&m_CritSecWave);

   while (TRUE) {
      BOOL fTooMany = (m_lPCM3DWave.Num() > 25);   // if have too many waves then remove

      DWORD dwBestDel = -1;
      __int64 iBestDelTime = 0;

      PCM3DWave *ppw = (PCM3DWave*)m_lPCM3DWave.Get(0);
      for (i = 0; i < m_lPCM3DWave.Num(); i++) {
         PATWAVE pat = (PATWAVE)ppw[i]->m_pUserData;

         // if have use-critical section flag then just ignore because loading
         if (pat->fUseCritical)
            continue;
         if (pat->dwRefCount)
            continue;

         if (pat->fDelWhenNoRef) {
            // just delete this becasue wants to be
            dwBestDel = i;
            break;
         }

         // if there aren't too many then dont bother deleting
         if (!fTooMany)
            continue;

         if ((dwBestDel == -1) || (pat->iLastUsed < iBestDelTime)) {
            dwBestDel = i;
            iBestDelTime = pat->iLastUsed;
            continue;
         }
      } // i

      // if nothing to delete then done
      if (dwBestDel == -1)
         break;

      // delete this one
      WaveDelete (ppw[dwBestDel]);
      m_lPCM3DWave.Remove (dwBestDel);
   } // while TRUE

   if (fUseCritSec)
      LeaveCriticalSection (&m_CritSecWave);
}


/*************************************************************************************
CAudioThread::MidiFreeOld - This frees up the old, unused Midis.

inputs
   BOOL        fUseCritSec - if TRUE then use the critical section wrappers
                     (which is normal, unless already in the critical section)
*/
void CAudioThread::MidiFreeOld (BOOL fUseCritSec)
{
   DWORD i;
   if (fUseCritSec)
      EnterCriticalSection (&m_CritSecWave);

   while (TRUE) {
      BOOL fTooMany = (m_lPCMidiFile.Num() > 5);   // if have too many Midis then remove
         // BUGFIX - Was 25, convert to 5

      DWORD dwBestDel = -1;
      __int64 iBestDelTime = 0;

      PCMidiFile *ppw = (PCMidiFile*)m_lPCMidiFile.Get(0);
      for (i = 0; i < m_lPCMidiFile.Num(); i++) {
         PATWAVE pat = (PATWAVE)ppw[i]->m_pUserData;

         // if have use-critical section flag then just ignore because loading
         if (pat->fUseCritical)
            continue;
         if (pat->dwRefCount)
            continue;

         if (pat->fDelWhenNoRef) {
            // just delete this becasue wants to be
            dwBestDel = i;
            break;
         }

         // if there aren't too many then dont bother deleting
         if (!fTooMany)
            continue;

         if ((dwBestDel == -1) || (pat->iLastUsed < iBestDelTime)) {
            dwBestDel = i;
            iBestDelTime = pat->iLastUsed;
            continue;
         }
      } // i

      // if nothing to delete then done
      if (dwBestDel == -1)
         break;

      // delete this one
      MidiDelete (ppw[dwBestDel]);
      m_lPCMidiFile.Remove (dwBestDel);
   } // while TRUE

   if (fUseCritSec)
      LeaveCriticalSection (&m_CritSecWave);
}



/*************************************************************************************
CAudioThread::TTSSpeak - Causes TTS to speak.

inputs
   PCMMLNode2         pNode - Node to speak. This is taken over by the audio thread.
   LARGE_INTEGER     iTime - Time that started speaking, used for mute
returns
   PCM3DWave - Wave that will be used. This must be released when finished.
*/
PCM3DWave CAudioThread::TTSSpeak (PCMMLNode2 pNode, LARGE_INTEGER iTime)
{
   PCM3DWave pWave = WaveCreateAlways (L"", TRUE);
   if (!pWave) {
      delete pNode;
      return NULL;
   }

   TTSQUEUE tq;
   memset (&tq, 0, sizeof(tq));
   tq.pNode = pNode;
   tq.pWave = pWave;
   tq.iTime = iTime;

   EnterCriticalSection (&m_CritSecTTS);
   m_lTTSQUEUE.Add (&tq);
   LeaveCriticalSection (&m_CritSecTTS);
   SetEvent (m_hSignalToThreadTTS);

   return pWave;
}


/******************************************************************************
CAudioThread::CanLoadTTS - Called by the internet thread when all the deletion
is completed. This allows the TTS libraryes to be loaded.
*/
void CAudioThread::CanLoadTTS (void)
{
   m_fCanLoadTTS = TRUE;
   SetEvent (m_hSignalToThreadTTS);
}







/*************************************************************************************
CAudioQueue::Constructor and destructor
*/
CAudioQueue::CAudioQueue (PCAudioThread pAT)
{
   m_pAT = pAT;
   m_pWave = NULL;
   m_pMidi = NULL;
   m_lPCMMLNode.Init (sizeof(PCMMLNode2));
   m_lAQTIMEMEM.Init (sizeof(AQTIMEMEM));
   m_lDELAYINFO.Init (sizeof(DELAYINFO));
   m_aiVolume[0] = m_aiVolume[1] = AQ_DEFVOLUME;
   m_fNoLipSync = FALSE;
   memcpy (m_aiVolumeOld, m_aiVolume, sizeof(m_aiVolume));
   m_dwPlayMode = AQPM_NONE;
   m_gID = GUID_NULL;
   m_fVolume3D = FALSE;
   m_pVolume3D.Zero4();
   m_dwFadeInSamples = m_dwFadeOutSamples = 0;
   m_dwExpected = (DWORD)-1;
   m_dwStartFadingOut = (DWORD)-1;

   m_pAmbient = NULL;
   m_dwAmbientLoop = NULL;
   m_fWantToExit = FALSE;
   m_fVoiceChat = FALSE;
   m_gVoiceChat = GUID_NULL;

   m_iTimeStarted.QuadPart = 0;

#ifdef USEDIRECTX
   m_pVolume3DRot.Zero4();
   m_fPlaying = FALSE;
#endif
}

CAudioQueue::~CAudioQueue (void)
{
   // release wave
   if (m_pWave && m_pAT)
      m_pAT->WaveRelease (m_pWave);
   m_pWave = NULL;

   // release MIDI
   if (m_pMidi && m_pAT)
      m_pAT->MidiRelease (m_pMidi);
   m_pMidi = NULL;

   // free up nodes
   DWORD i;
   PCMMLNode2 *ppn = (PCMMLNode2*)m_lPCMMLNode.Get(0);
   for (i = 0; i < m_lPCMMLNode.Num(); i++)
      delete ppn[i];

   PAQTIMEMEM ppm = (PAQTIMEMEM)m_lAQTIMEMEM.Get(0);
   for (i = 0; i < m_lAQTIMEMEM.Num(); i++, ppm++)
      if (ppm->pMem)
         delete ppm->pMem;

   // free up delay info
   PDELAYINFO pdi = (PDELAYINFO)m_lDELAYINFO.Get(0);
   for (i = 0; i < m_lDELAYINFO.Num(); i++, pdi++)
      delete pdi->pNode;
}


/*************************************************************************************
CAudioQueue::VisemeQueue - Posts a viseme to the queue.

inputs
   DWORD          dwSample - Sample number that this occurs at, from the start of the wave
   DWORD          dwEnglishPhone - English phoneme
   BOOL           fUseCritSec - If TRUE then need to use m_pAT->m_CritSecQueue
returns
   BOOL - TRUE if added
*/
BOOL CAudioQueue::VisemeQueue (DWORD dwSample, DWORD dwEnglishPhone, BOOL fUseCritSec)
{
   // if there's no lipsync for this then skip
   if (m_fNoLipSync)
      return FALSE;

   // if no ID for the object then skip
   if (IsEqualGUID(m_gID, GUID_NULL) || !m_pWave || !m_pWave->m_dwSamplesPerSec)
      return FALSE;

   // if we haven't started the timer yet for when the wave started, then do so
   if (!m_iTimeStarted.QuadPart)
      QueryPerformanceCounter (&m_iTimeStarted);

   // figure out how many seconds from when started that the event occurs
   LARGE_INTEGER iTime;
   iTime.QuadPart = (QWORD) dwSample * (m_pAT->m_iPerformanceFrequency.QuadPart / 1000) / (QWORD) m_pWave->m_dwSamplesPerSec * 1000;
   iTime.QuadPart -= m_pAT->m_iPerformanceFrequency.QuadPart / 5; // one-fifth of a second head-start for viseme queue messages so can draw
   iTime.QuadPart += m_iTimeStarted.QuadPart;

   m_pAT->VisemeQueue (iTime, &m_gID, dwEnglishPhone, fUseCritSec);

   return TRUE;
}

/*************************************************************************************
CAudioQueue::QueueAdd - This adds an item to the queue.

inputs
   PCMMLNode2        pNode - Item to queue up. This pNode is kept and freed
                     by the function. (So the caller should forget about it)
   PCMem             pMem - Memory usually associated with item. Usually NULL, but
                     not for voice chat. pMem is kept and used by the queue,
                     so caller should forget about it
   LARGE_INTEGER     iTime - Time stamp, from QueryPerformanceCounter()
   BOOL           fUseCritSec - If TRUE then need to use m_pAT->m_CritSecQueue
*/
BOOL CAudioQueue::QueueAdd (PCMMLNode2 pNode, PCMem pMem, LARGE_INTEGER iTime, BOOL fUseCritSec)
{
   m_lPCMMLNode.Add (&pNode);

   AQTIMEMEM tm;
   memset (&tm, 0, sizeof(tm));
   tm.pMem = pMem;
   tm.iTime = iTime;
   m_lAQTIMEMEM.Add (&tm);

   // consider extracting this right away because may have an empty queue
   ConsiderNextNode (fUseCritSec);

   return TRUE;
}


/*************************************************************************************
CAudioQueue::MidiTimer - If there happens to be a MIDI node available then play it.
This should be called every few milliseconds
*/
void CAudioQueue::MidiTimer (void)
{
   if (m_dwPlayMode != AQPM_MIDI)
      return; // nothing

   // pull from the wave...
   PATWAVE pat = (PATWAVE)m_pMidi->m_pUserData;
   BOOL fUseCrit = pat->fUseCritical;

   if (fUseCrit)
      EnterCriticalSection (&pat->CritSec);

   // determine how many samples can copy, and how many want
   DWORD dwCanCopy;
   if (pat->dwSampleValid)
      dwCanCopy = 1; // just as a flag
   else
      dwCanCopy = 0;

   if (fUseCrit)
      LeaveCriticalSection (&pat->CritSec);

   // just try starting no matter what, since if already started wont
   // do anything
   BOOL fStarted = FALSE;
   if (dwCanCopy) {
      // BUGFIX - Make sure that had not already started
      if (!m_dwSample)
         fStarted = m_pMidi->Start (m_pAT->m_pMidiMix);

      // if the volume has changed then resend
      if ((m_aiVolume[0] != m_aiVolumeOld[0]) || (m_aiVolume[1] != m_aiVolumeOld[1])) {
         memcpy (m_aiVolumeOld, m_aiVolume, sizeof(m_aiVolume));
         m_pMidi->VolumeSet ((WORD)abs(m_aiVolume[0]), (WORD)abs(m_aiVolume[1]));
      }

      // advance
      m_pMidi->PlayAdvance (-1); // negative time so checks its own
   }

   // send audio start message
   if (fStarted) {
      m_dwSample = 1;   // so know that started already
      if (!IsEqualGUID(m_gID, GUID_NULL)) {
         // send an audio start message
         PCMem pMem = new CMem;
         if (pMem && pMem->Required(sizeof(m_gID))) {
            memcpy (pMem->p, &m_gID, sizeof(m_gID));
            PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYAUDIOSTART, 0, (LPARAM)pMem);
         }
         else if (pMem)
            delete pMem;
      }
   }

}


/*************************************************************************************
CAudioQueue::FadeLevel - Given a sample, returns the fade level, from 0 (fully faded)
to 256 (not)

inputs
   DWORD       dwSample - Sample number
returns
   int - Value from 0 to 256.
*/
int CAudioQueue::FadeLevel (DWORD dwSample)
{
   int iRet = 256;
   int i;

   // fade in
   if (dwSample < m_dwFadeInSamples) {
      i = (int)dwSample * 256 / (int)m_dwFadeInSamples;
      iRet = min(i, iRet);
   }

   // fade out
   if (m_dwFadeOutSamples && (m_dwExpected != (DWORD)-1) && (m_dwStartFadingOut != (DWORD)-1) &&
      (dwSample >= m_dwStartFadingOut)) {
      if (dwSample >= m_dwStartFadingOut + m_dwFadeOutSamples)
         i = 0;
      else
         i = 256 - (int) (dwSample - m_dwStartFadingOut) * 256 / (int) m_dwFadeOutSamples;
      iRet = min(i, iRet);
   }

   return iRet;
}




/*************************************************************************************
CAudioQueue::AudioSum - This sums the audio from the queue to the given samples.

inputs
   short             *psSamp - Sample to add to. Not used in USEDIRECTX
   LPDIRECTSOUND8    pDirectSound - Direct sound object
   PCMem             pMemScratch - Scratch memory. Used if USEDIRECTX
   DWORD             dwSamples - NUmber of samples there (at 22 kHz)
   BOOL           fUseCritSec - If TRUE then need to use m_pAT->m_CritSecQueue
returns
   BOOL - TRUE if the queue is still alive and kicking, FALSE if it is finished
         and there's nothing left
*/
#ifdef USEDIRECTX
BOOL CAudioQueue::AudioSum (LPDIRECTSOUND8 pDirectSound, PCMem pMemScratch, DWORD dwSamples, BOOL fUseCritSec)
#else
BOOL CAudioQueue::AudioSum (short *psSamp, DWORD dwSamples, BOOL fUseCritSec)
#endif
{

   // NOTE: This code assumes ALL audio is at 22khz

#ifdef _DEBUG
// #define OUTPUTSAMPLETRACE
#endif // _DEBUG

#ifdef OUTPUTSAMPLETRACE
   char szTemp[256];
   BOOL fNotNone = FALSE;
#endif

   DWORD i, j;
   BOOL fBreak = FALSE;
   DWORD dwOrigSamples = dwSamples;
   DWORD dwSamplesCopied = 0; // assuming 22 khz
   DWORD dwRecommendLeft = 0;
   DWORD dwUrgent = 0;
   DWORD dwDontLoopForever = 0;
   while (!fBreak && dwSamples && (dwDontLoopForever < 1000)) {
      dwDontLoopForever++;  // BUGFIX - so wont get in infinite loops

      // consider pulling out more
      ConsiderNextNode (fUseCritSec);

      // add audio?
      if (m_dwPlayMode == AQPM_NONE) {
         dwSamplesCopied += dwOrigSamples;
         dwSamples = 0; // so will at least increment delay counters
         break;
      }

#ifdef OUTPUTSAMPLETRACE
      sprintf (szTemp, "\r\nAudioSum, dwSamples=%d", (int)dwSamples);
      OutputDebugString (szTemp);
      fNotNone = TRUE;
#endif
      BOOL fClearOutPercent = FALSE;

      switch (m_dwPlayMode) {
      case AQPM_SILENCE:
         {
#ifdef OUTPUTSAMPLETRACE
            OutputDebugString ("\r\n\tAQPM_SILENCE");
#endif
#ifdef USEDIRECTX
            // if we're not playing then start playing
            if (!m_fPlaying) {
               if (!m_bufDX.Init (pDirectSound, AUDIOTHREAD_SAMPLESPERSEC, 1, FALSE)) {
                  // error
                  m_dwPlayMode = AQPM_NONE;
                  m_dwSample = 0;
                  m_iTimeStarted.QuadPart = 0;
                  fClearOutPercent = TRUE;
                  break;
               }
               m_fPlaying = TRUE;
            }

            // how much do we want
            DWORD dwCanCopy = m_bufDX.Recommend (NULL) / (m_bufDX.m_dwChannels * 2); // BUGFIX - removed m_bufDX.m_dwSamplesPerSec * 
            DWORD dwMaxCanCopy = dwCanCopy;
            dwCanCopy = min(dwCanCopy, m_dwDuration - m_dwSample);
            // BUGBUG: This might have problems if the preexisting wave is using non-22kHz wave,
            // the silence duration might be off because seems to assume 22 kHz

            // set this
            DWORD dwBlank = dwCanCopy * m_bufDX.m_dwChannels * 2;
            if (pMemScratch->Required (dwBlank) && dwBlank) {
               // NOTE: Cant just pass in NULL because then wont guarantee exact time
               memset (pMemScratch->p, 0, dwBlank);
               m_bufDX.Write ((PBYTE) pMemScratch->p, dwBlank);
            }
            if (dwCanCopy == dwMaxCanCopy)
               dwSamples = 0; // since filled up as much as possible
            // else, dont change dwSamples
#else
            DWORD dwCanCopy = min(dwSamples, m_dwDuration - m_dwSample);

            // skip over
            psSamp += (dwCanCopy * AUDIOTHREAD_CHANNELS);
            dwSamples -= dwCanCopy;
#endif
            dwSamplesCopied += dwCanCopy;

            // since nothing to copy with silence, just do nothing
            m_dwSample += dwCanCopy;


            if (m_dwSample >= m_dwDuration) {
               // finished with silence period
               m_dwPlayMode = AQPM_NONE;
               m_dwSample = 0;
               m_iTimeStarted.QuadPart = 0;
               fClearOutPercent = TRUE;
            }
         }
         break;

      case AQPM_MIDI:
         {
#ifdef OUTPUTSAMPLETRACE
            OutputDebugString ("\r\n\tAQPM_MIDI");
#endif
#ifdef USEDIRECTX
            // if not finished playing wave then assume no samples
            if (m_fPlaying) {
               if (!m_bufDX.FinishedPlaying())
                  dwSamples = 0; // since have nothing left to fill up
               else {
                  m_bufDX.Release();
                  m_fPlaying = FALSE;
               }
            }
#endif

            // pull from the wave...
            PATWAVE pat = (PATWAVE)m_pMidi->m_pUserData;
            BOOL fUseCrit = pat->fUseCritical;
            BOOL fWantToDelete = FALSE;

            if (fUseCrit)
               EnterCriticalSection (&pat->CritSec);

            // determine how many samples can copy, and how many want
            DWORD dwCanCopy;
            if (m_dwSample)
               dwCanCopy = dwSamples; // just as a flag
            else
               dwCanCopy = 0;

            if (fUseCrit)
               LeaveCriticalSection (&pat->CritSec);

            // just try starting no matter what, since if already started wont
            // do anything
            if (dwCanCopy) {
               // advance
               fWantToDelete = !m_pMidi->PlayAdvance (0);   // note: advance nothing,
                        // mostly so know if have reached the end

               if (m_fWantToExit) {
                  fWantToDelete = TRUE;
                  m_pMidi->Stop();
               }
            }

            m_dwSample += dwCanCopy;   // so keep it up to date

            // if want to release this Midi then do so
            if (fWantToDelete) {
               if (!IsEqualGUID (m_gID, GUID_NULL)) {
                  // send an audio done message
                  PCMem pMem = new CMem;
                  if (pMem && pMem->Required(sizeof(m_gID))) {
                     memcpy (pMem->p, &m_gID, sizeof(m_gID));
                     PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYAUDIOSTOP, 0, (LPARAM)pMem);
                  }
                  else if (pMem)
                     delete pMem;
               }

               m_pAT->MidiRelease (m_pMidi);
               m_pMidi = NULL;
               m_dwSample = 0;
               m_iTimeStarted.QuadPart = 0;
               m_dwPlayMode = AQPM_NONE;
               fClearOutPercent = TRUE;
            }
            

            // clear out samples
            dwSamplesCopied += dwSamples;
            dwSamples = 0;
         }
         break;


      case AQPM_WAVE:
         {
#ifdef OUTPUTSAMPLETRACE
            OutputDebugString ("\r\n\tAQPM_WAVE");
#endif
            // pull from the wave...
            PATWAVE pat = (PATWAVE)m_pWave->m_pUserData;
            BOOL fUseCrit = pat->fUseCritical;
            BOOL fWantToDelete = FALSE;
            if (fUseCrit)
               EnterCriticalSection (&pat->CritSec);

#ifdef OUTPUTSAMPLETRACE
            sprintf (szTemp, "\r\n\tm_dwSample=%d, pat->dwSampleValid=%d", (int)m_dwSample, (int)pat->dwSampleValid);
            OutputDebugString (szTemp);
#endif

            // BUGFIX - If TTS and has been muted, then discard the rest
            if (pat->iTTSTime.QuadPart) {
               EnterCriticalSection (&m_pAT->m_CritSecTTS);
               if ((pat->iTTSTime.QuadPart > m_pAT->m_iTTSMuteTimeStart.QuadPart) && (pat->iTTSTime.QuadPart < m_pAT->m_iTTSMuteTimeEnd.QuadPart))
                  pat->dwSampleValid = pat->dwSampleExpected = m_dwSample; // so wont have any more data
               LeaveCriticalSection (&m_pAT->m_CritSecTTS);
            }

            // determine how many samples can copy, and how many want
            DWORD dwCanCopy;
            if (m_dwSample < pat->dwSampleValid)
               dwCanCopy = pat->dwSampleValid - m_dwSample;
                     // BUGFIX - Had forgotten to subtract m_dwSample
            else
               dwCanCopy = 0;

#ifdef OUTPUTSAMPLETRACE
            sprintf (szTemp, "\r\n\tdwCanCopy=%d", (int)dwCanCopy);
            OutputDebugString (szTemp);
#endif

#ifdef USEDIRECTX
            // if can copy data, make sure we're talking about the same sampling rate and channels
            // here as in the previous wave
            if (dwCanCopy && m_fPlaying) {
               BOOL fCanAppend = TRUE;
               if (fCanAppend && (m_bufDX.m_dwChannels != m_pWave->m_dwChannels))
                  fCanAppend = FALSE;
               if (fCanAppend && (m_bufDX.m_dwSamplesPerSec != m_pWave->m_dwSamplesPerSec))
                  fCanAppend = FALSE;
               if (fCanAppend && ((!m_bufDX.m_f3D) !=(!m_fVolume3D)))
                  fCanAppend = FALSE;

               if (!fCanAppend) {
                  // see if shut down
                  if (m_bufDX.FinishedPlaying()) {
                     m_bufDX.Release();
                     m_fPlaying = FALSE;
                  }
                  else
                     dwCanCopy = 0; // since cant copy any more
               }
            }

            // if we're not playing then initialize
            if (dwCanCopy && !m_fPlaying) {
               if (m_bufDX.Init (pDirectSound, m_pWave->m_dwSamplesPerSec, m_pWave->m_dwChannels, m_fVolume3D))
                  m_fPlaying = TRUE;
               else
                  dwCanCopy = 0; // since cant copy
            }

            // BUGFIX - Ask for recommendation even if no dwCanCopy, since may need to
            // pad in silence
            if (m_fPlaying) {
               dwRecommendLeft = m_bufDX.Recommend (&dwUrgent) / (m_bufDX.m_dwChannels * 2);
               dwUrgent /= (m_bufDX.m_dwChannels * 2);
            }
            else
               dwRecommendLeft = dwUrgent = 0;

            // if we're playing then find out how much data is recommended and do that
            DWORD dwMaxCopy = 0;
            if (dwCanCopy && m_fPlaying)
               dwCanCopy = dwMaxCopy = min(dwCanCopy, dwRecommendLeft);
#ifdef OUTPUTSAMPLETRACE
            sprintf (szTemp, "\r\n\tdwRecommend=%d, dwCanCopy=%d", (int)dwRecommendLeft, (int)dwCanCopy);
            OutputDebugString (szTemp);
#endif

            DWORD dwNeedMem = dwCanCopy * m_bufDX.m_dwChannels * 2;
            short *psSamp = NULL;
            if (pMemScratch->Required (dwNeedMem)) {
               memset (pMemScratch->p, 0, dwNeedMem);
               psSamp = (short*)pMemScratch->p;
            }
            else
               dwCanCopy = NULL;
#else
            dwCanCopy = min(dwCanCopy, dwSamples);
#endif

#ifdef OUTPUTSAMPLETRACE
            sprintf (szTemp, "\r\n\tdwCanCopy=%d", (int)dwCanCopy);
            OutputDebugString (szTemp);
#endif
            pat->dwSampleProcessTo = max(pat->dwSampleProcessTo, m_dwSample + dwCanCopy + AUDIOTHREAD_LOOKAHEADSAMP);

            m_dwExpected = pat->dwSampleExpected;
            if ((m_dwStartFadingOut == (DWORD)-1) && (m_dwExpected != (DWORD)-1) && m_dwFadeOutSamples)
               m_dwStartFadingOut = (m_dwFadeOutSamples < m_dwExpected) ? (m_dwExpected - m_dwFadeOutSamples) : 0;


            // if nothing left then want to delete this
            if ((m_dwExpected != -1) && (pat->dwSampleValid >= m_dwExpected) &&
               (m_dwSample + dwCanCopy >= m_dwExpected) && pat->fFinishedLoading) {
                  fWantToDelete = TRUE;

                  // if make sure don't exceept m_dwExpected.. BUGFIX - fix blip at end of TTS wave
                  if (m_dwSample + dwCanCopy >= m_dwExpected) {
                     if (m_dwSample <= m_dwExpected)
                        dwCanCopy = m_dwExpected - m_dwSample;
                     else
                        dwCanCopy = 0;
                  }
               }

            if (fUseCrit)
               LeaveCriticalSection (&pat->CritSec);

            // send audio start message
            if (dwCanCopy && !m_dwSample && !IsEqualGUID(m_gID, GUID_NULL)) {
               // send an audio start message
               PCMem pMem = new CMem;
               if (pMem && pMem->Required(sizeof(m_gID))) {
                  memcpy (pMem->p, &m_gID, sizeof(m_gID));
                  PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYAUDIOSTART, 0, (LPARAM)pMem);
               }
               else if (pMem)
                  delete pMem;
            }

#ifdef USEDIRECTX
               // set the 2D and 3d volumes
               m_bufDX.SetVolume (m_aiVolume[0], m_aiVolume[1]);
               m_bufDX.SetVolume (&m_pVolume3DRot, m_pVolume3DRot.p[3]);
#endif

            // copy over
            if (dwCanCopy) {
#define MAXDELTAVOL        (AQ_NOVOLCHANGE / 0x800)   // dont allow too fast of a volume change
               short *psOrig = m_pWave->m_psWave + (m_dwSample * m_pWave->m_dwChannels);
               int iSample;

               // send lip sync
               DWORD dwPhone;
               PWVPHONEME pPhone = (PWVPHONEME)m_pWave->m_lWVPHONEME.Get(0);
               for (dwPhone = 0; dwPhone < m_pWave->m_lWVPHONEME.Num(); dwPhone++, pPhone++) {
                  if (pPhone->dwSample < m_dwSample)
                     continue;   // old
                  if (pPhone->dwSample >= m_dwSample + dwCanCopy)
                     break;   // hasn't happened yet

                  // send
                  VisemeQueue (pPhone->dwSample, pPhone->dwEnglishPhone, TRUE);
               } // dwPhone

               // determine fade in and out
               int aiVolumeOld[2], aiVolume[2];
               for (i = 0; i < 2; i++) {
#ifdef USEDIRECTX
                  aiVolumeOld[i] = aiVolume[i] = AQ_NOVOLCHANGE;
#else
                  aiVolumeOld[i] = m_aiVolumeOld[i];
                  aiVolume[i] = m_aiVolume[i];
#endif

                  // apply fade levels
                  aiVolumeOld[i] = aiVolumeOld[i] * FadeLevel(m_dwSample) / 256;
                  aiVolume[i] = aiVolume[i] * FadeLevel(m_dwSample + dwCanCopy) / 256;
                     // BUGFIX - Didn't include dwCanCopy into fade level
               } // i

               for (i = 0; i < dwCanCopy; i++, psOrig += m_pWave->m_dwChannels) {
                  // slow volume change
                  if (aiVolumeOld[0] != aiVolume[0]) {
                     if (aiVolumeOld[0] + MAXDELTAVOL < aiVolume[0])
                        aiVolumeOld[0] += MAXDELTAVOL;
                     else if (aiVolumeOld[0] - MAXDELTAVOL > aiVolume[0])
                        aiVolumeOld[0] -= MAXDELTAVOL;
                     else
                        aiVolumeOld[0] = aiVolume[0];
                  }
                  if (aiVolumeOld[1] != aiVolume[1]) {
                     if (aiVolumeOld[1] + MAXDELTAVOL < aiVolume[1])
                        aiVolumeOld[1] += MAXDELTAVOL;
                     else if (aiVolumeOld[1] - MAXDELTAVOL > aiVolume[1])
                        aiVolumeOld[1] -= MAXDELTAVOL;
                     else
                        aiVolumeOld[1] = aiVolume[1];
                  }

                  // NOTE: Using aiVolumeOld so blend volumes
#ifdef USEDIRECTX
                  for (j = 0; j < m_bufDX.m_dwChannels; j++, psSamp++) {
#else
                  for (j = 0; j < AUDIOTHREAD_CHANNELS; j++, psSamp++) {
#endif
                     iSample = (int)psOrig[j % m_pWave->m_dwChannels] *
                        aiVolumeOld[j % 2] / AQ_NOVOLCHANGE;
                     iSample += (int)psSamp[0];
                     iSample = max(iSample, -32768);
                     iSample = min(iSample, 32767);
                     psSamp[0] = (short)iSample;
                  } // j
               } // i

               // remember new volumes
               memcpy (m_aiVolumeOld, m_aiVolume, sizeof(m_aiVolume));

               m_dwSample += dwCanCopy;

               // if we haven't started the timer yet for when the wave started, then do so
               if (!m_iTimeStarted.QuadPart)
                  QueryPerformanceCounter (&m_iTimeStarted);

#ifdef USEDIRECTX
               if (dwCanCopy)
                  m_bufDX.Write ((PBYTE) pMemScratch->p, dwCanCopy * m_bufDX.m_dwChannels * 2);
               
               // BUGFIX - reduce recommend left by amount written
               if (dwUrgent > dwCanCopy)
                  dwUrgent -= dwCanCopy;
               else
                  dwUrgent = 0;

               if (dwCanCopy == dwMaxCopy)
                  dwSamples = 0; // since just did the maximum
               // else, leave dwSAmples so will come back and try again
#else
               dwSamples -= dwCanCopy;
#endif
               dwSamplesCopied += dwCanCopy;

            }

            // if want to release this wave then do so
            if (fWantToDelete) {
               // restore to 0 phoneme
               if (m_pWave && m_pWave->m_lWVPHONEME.Num())
                  VisemeQueue (m_pWave->m_dwSamples, 0 /* silence */, TRUE);

               if (!IsEqualGUID (m_gID, GUID_NULL)) {
                  // send an audio done message
                  PCMem pMem = new CMem;
                  if (pMem && pMem->Required(sizeof(m_gID))) {
                     memcpy (pMem->p, &m_gID, sizeof(m_gID));
                     PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYAUDIOSTOP, 0, (LPARAM)pMem);
                  }
                  else if (pMem)
                     delete pMem;
               }

               m_pAT->WaveRelease (m_pWave);
               m_pWave = NULL;
               m_dwSample = 0;
               m_iTimeStarted.QuadPart = 0;
               m_dwPlayMode = AQPM_NONE;
               fClearOutPercent = TRUE;
            }
            else if (!dwCanCopy) {
               // couldnt copy anymore
               fBreak = TRUE;
               break;
            }
         }
         break;

      } // switch

#ifdef OUTPUTSAMPLETRACE
      OutputDebugString ("\r\n\tDone switch");
#endif
      PDELAYINFO pdi = (PDELAYINFO) m_lDELAYINFO.Get(0);
      DWORD dwThisWaveDuration = (m_dwPlayMode == AQPM_SILENCE) ? m_dwDuration : -1;
      // now go through and change percent to actual samples
      for (i = 0; i < m_lDELAYINFO.Num(); i++) {
         // if have any marked as unknown, then see if the wave has partially
         // loaded yet so can guestimate how long it will be
         if (pdi[i].dwSample != -1)
            continue;   // already know the length

         if (dwThisWaveDuration == -1) {
#ifdef OUTPUTSAMPLETRACE
            OutputDebugString ("\r\n\tdwThisWaveDuration == -1");
#endif
            if (!m_pWave && !m_pMidi)
               break;   // cant figure out

            PATWAVE pat = (PATWAVE)(m_pWave ? m_pWave->m_pUserData : m_pMidi->m_pUserData);

            BOOL fUseCrit = pat->fUseCritical;
            if (fUseCrit)
               EnterCriticalSection (&pat->CritSec);
            if (pat->dwSampleExpected != -1)
               dwThisWaveDuration = pat->dwSampleExpected;  // not expected sample
            if (fUseCrit)
               LeaveCriticalSection (&pat->CritSec);

            if (dwThisWaveDuration == -1)
               break;   // not loaded enough yet so can't tell
         }
         
         // when get here know that have a wave duration
         // assume it' a percent since that's the only way to have -1 sample
         pdi[i].dwSample = (DWORD)((fp)dwThisWaveDuration * pdi[i].fValue / 100.0);
      } // i

#ifdef OUTPUTSAMPLETRACE
      OutputDebugString ("\r\n\tm_lDELAYINFO endloop");
#endif
      for (i = 0; i < m_lDELAYINFO.Num(); i++) {
         // if have a flag marked to clear out percent that means that have finished
         // playing the current wave or silence. Therefore, if anything is still
         // marked with a percent then set it to 0
         if (fClearOutPercent && pdi[i].fPercent && (pdi[i].dwSample == -1))
            pdi[i].dwSample = 0;
      }

   } // while there are samples left

#ifdef OUTPUTSAMPLETRACE
   if (fNotNone)
      OutputDebugString ("\r\n\tDone while");
#endif

   // go through all of the delayed items and run them if it's time
   if (m_lDELAYINFO.Num()) {
#ifdef OUTPUTSAMPLETRACE
      OutputDebugString ("\r\n\tm_lDELAYINFO.Num()");
#endif
      PDELAYINFO pdi = (PDELAYINFO) m_lDELAYINFO.Get(0);
      BOOL fKillPercent = (m_dwPlayMode == AQPM_NONE);
         // if any with a percent, but there's no more audio coming then act now
      DWORD dwCopied = dwOrigSamples;  // BUGFIX - was dwSamplesCopied; Changed hoping would fix "wink" emote

      for (i = m_lDELAYINFO.Num()-1; i < m_lDELAYINFO.Num(); i--) {
         if (fKillPercent && (pdi[i].fPercent == TRUE) && (pdi[i].dwSample == -1))
            pdi[i].dwSample = 0;

         if (pdi[i].dwSample == -1)
            continue;   // no info on this yet

         if (pdi[i].dwSample > dwCopied) {
            // have worked some of the way there, but not all
            pdi[i].dwSample -= dwCopied;
            continue;
         }

         // else, this one should go off
         PCMMLNode2 pNode = pdi[i].pNode;
         LARGE_INTEGER iTime = pdi[i].iTime;
         m_lDELAYINFO.Remove (i);
         m_pAT->RequestAudio (pNode, iTime, FALSE, FALSE);

         // replaod because delay info may have changed
         pdi = (PDELAYINFO) m_lDELAYINFO.Get(0);
      } // i
   } // m_lDELAYINFO.Num()

#ifdef OUTPUTSAMPLETRACE
   if (fNotNone)
      OutputDebugString ("\r\n\tConsiderNextNode()");
#endif
   // BUGFIX: Call ConsiderNextNode so doens't stop playing after finished with MIDI
   ConsiderNextNode (fUseCritSec);

#ifdef USEDIRECTX
   // BUGFIX - If there are samples left, and we're playing a wave, then add silence
   // onto end so dont loop garbage
   if (m_fPlaying && m_bufDX.HasStartedPlaying() && dwUrgent && (m_dwPlayMode == AQPM_WAVE)) {
#ifdef OUTPUTSAMPLETRACE
      char szTemp[256];
      sprintf (szTemp, "\r\n\tExtend silence=%d", (int)dwUrgent);
      OutputDebugString (szTemp);
#endif
      DWORD dwNeedMem = dwUrgent * m_bufDX.m_dwChannels * 2;
      if (pMemScratch->Required (dwNeedMem)) {
         memset (pMemScratch->p, 0, dwNeedMem);

#if 0 // def _DEBUG // to test urgent
         for (i = 0; i < dwNeedMem/2; i++)
            *((short*)pMemScratch->p) = i * 256;
#endif

         m_bufDX.Write ((PBYTE) pMemScratch->p, dwNeedMem);
      }
   }
#endif

   if ((m_dwPlayMode != AQPM_NONE) || m_lPCMMLNode.Num() || m_lDELAYINFO.Num()) {
#ifdef OUTPUTSAMPLETRACE
      OutputDebugString ("\r\n\tDon't delete");
#endif
      return TRUE;   // dont delete
   }

#ifdef USEDIRECTX
   // else, might be able to delete...

   // cant stop if not playing anything
   if (!m_fPlaying) {
#ifdef OUTPUTSAMPLETRACE
      if (fNotNone)
         OutputDebugString ("\r\n\tCan stop playing 1");
#endif
      return FALSE;
   }

   // else, pump in silence until done
   if (m_bufDX.FinishedPlaying()) {
      m_bufDX.Release();
      m_fPlaying = FALSE;
   }
#ifdef OUTPUTSAMPLETRACE
   sprintf (szTemp, "\r\n\tm_fPlaying=%d", (int)m_fPlaying);
   OutputDebugString (szTemp);
#endif
   return m_fPlaying;  // BUGFIX - Was returning !m_fPlaying, should be the oppsite
#else
   return FALSE;
#endif
}


static PWSTR gpszFile = L"file";
static PWSTR gpszVolL = L"voll";
static PWSTR gpszVolR = L"volr";
static PWSTR gpszVol3D = L"vol3D";
static PWSTR gpszTime = L"time";
static PWSTR gpszPercent = L"percent";
static PWSTR gpszBinary = L"binary";
static PWSTR gpszNoLipSync = L"nolipsync";

static PWSTR gpszFadeIn = L"fadein";
static PWSTR gpszFadeOut = L"fadeout";

/*************************************************************************************
CAudioQueue::Volume3D - Translates the user's camera direction along with the
3D volume information into a full volume. This modified m_aiVolume[] in place
to the new volumes.

NOTE: This does NOT check for thread critical section info.

inputs
   fp          fSpeakerSep - Speaker separation. See m_fEarsSpeakerSep
   fp          fBackAtten - Back attenuation
   fp          fPowDistance - Power to use for distance
   fp          fScale - How much to scale volume for ears
*/
#define SPEAKERANGLE       (PI/6)

void CAudioQueue::Volume3D (fp fSpeakerSep, fp fBackAtten, fp fPowDistance, fp fScale)
{
   if (!m_fVolume3D)
      return;  // nothing

   // store away old volume
   memcpy (m_aiVolumeOld, m_aiVolume, sizeof(m_aiVolume));

   // make a matrix that rotates
   CMatrix m, m2;
   m.RotationZ (m_pAT->m_fLong);
   m2.RotationX (-m_pAT->m_fLat);
   m2.MultiplyLeft (&m);

   CPoint pRot;
   pRot.Copy (&m_pVolume3D);
   pRot.p[3] = 1;
   pRot.MultiplyLeft (&m2);


#if 0 // def _DEBUG
   char szTemp[128];
   sprintf (szTemp, "\r\nPos = %f, %f, %f, %f", (double)pRot.p[0], (double)pRot.p[1], (double)pRot.p[2],
      (double)m_pVolume3D.p[3]);
   OutputDebugString (szTemp);
#endif

   // figure out the distance
   fp fDist = pRot.Length();
   fp fDistOrig = max(fDist, CLOSE);
   fp fAngle = atan2 (pRot.p[0], pRot.p[1]);
         // negative is left, postive is right
   fDist = max(fDist, CLOSE);

   // decibels
   // BUGFIX - Assume the average 3d sound is played from 2m away (speaking distance),
   // not 1m. Therefore, add 6dB to the volume to compensate
   fp fVol = pow (2, (m_pVolume3D.p[3] - 60 + 6) / 6.0);  // 6dB per doubling, approx
   fp f;
   fDist = pow(fDist, fPowDistance);
   fVol = fVol / fDist * (fp)(AQ_DEFVOLUME*2 * fScale);
      // BUGFIX - Added *2 to AQ_DEFVOLUME so that if straight ahead then
      // will be AQ_DEFVOLUME on each channel
   fVol = min(fVol, (fp)(AQ_NOVOLCHANGE*4));  // dont allow to get too loud
      // BUGFIX - Was *16. Changed to *4

#ifdef USEDIRECTX
   // reconstruct the location based on racial effects
   fp fRaceAngle = fAngle / PI;
   fp fRaceVol;
   BOOL fIsLeft = (fAngle < 0);
   fRaceAngle = fabs(fRaceAngle);
   fRaceAngle = pow (fRaceAngle, fSpeakerSep);  // so 0.1 moves everything to back, 5 moves all front
   fRaceVol = fRaceAngle * fBackAtten / 0.75 /* human */ + (1.0 - fRaceAngle);   // so fron is loudness
   fRaceVol *= fScale;  // how loud
   fRaceVol /= sqrt(fDist);   // since fDist already raied to the power
   fRaceVol = max(fRaceVol, CLOSE);
   fRaceAngle *= PI;
   if (fIsLeft)
      fRaceAngle *= -1;

   m_pVolume3DRot.p[0] = sin(fRaceAngle) / fRaceVol;
   m_pVolume3DRot.p[1] = cos(fRaceAngle) / fRaceVol;
   m_pVolume3DRot.p[2] = pRot.p[2] / fDistOrig / fRaceVol;   // assume cant tell above
   m_pVolume3DRot.p[3] = m_pVolume3D.p[3];
#endif

   // assume that fAngle from -PI to PI

   // left channel... if right at -SPEAKERANGLE then = 1.0 * fVol.
   // if at SPEAKERANGLE then = 0. If at -PI (or PI) then 0.5 * fVol
   fp fLeft, fRight;
   fp fSpeakerAngle = SPEAKERANGLE * fSpeakerSep;
   if (fAngle < -fSpeakerAngle) {
      // left rear
      f = (fAngle - -PI) / (-fSpeakerAngle - -PI);
      fLeft = (1.0 - f) * 0.5 + f * 1.0;
      fRight = (1.0 - f) * -0.5 + f * 0;
   }
   else if (fAngle < fSpeakerAngle) {
      // front
      f = (fAngle - -fSpeakerAngle) / (fSpeakerAngle - -fSpeakerAngle);
      fLeft = (1.0 - f) * 1.0 + f * 0;
      fRight = (1.0 - f) * 0 + f * 1.0;
   }
   else {
      // right rear
      f = (fAngle - fSpeakerAngle) / (PI - fSpeakerAngle);
      fLeft = (1.0 - f) * 0 + f * 0.5;
      fRight = (1.0 - f) * 1.0 + f * -0.5;
   }

   // affect volume by back attentuation
   fp fAttenAlpha = fabs(fAngle) / PI;
   fVol *= ((1.0 - fAttenAlpha) + fAttenAlpha * fBackAtten);

   m_aiVolume[0] = (int)(fLeft * fVol);
   m_aiVolume[1] = (int)(fRight * fVol);

   // NOTE: Intentionally NOT multiplying in m_aiVolumeObjScale since
   // it's volumetric
}


/*************************************************************************************
CAudioQueue::AmbientDeleted - Called to indicate that an ambient mode has been deleted.
If the ambient is used then this will fade to 0 and quit.

NOTE: This assumes already in critical section.

inputs
   PCAmbient            pAmbient - Ambient deleted
*/
void CAudioQueue::AmbientDeleted (PCAmbient pAmbient)
{
   if (m_pAmbient != pAmbient)
      return; // nothing to do

   // else need to set
   m_pAmbient = NULL;

   // BUGFIX - Was just setting the new volume to 0, but instead use fadeout
   if (m_dwPlayMode == AQPM_WAVE) {
      if (!m_dwFadeOutSamples)
         m_dwFadeOutSamples = AUDIOTHREAD_SAMPLESPERSEC;

      if (m_dwStartFadingOut == (DWORD)-1)
         m_dwStartFadingOut = m_dwSample;
      else
         m_dwStartFadingOut = min (m_dwSample, m_dwStartFadingOut);
   }
   else {
      // keep this for midi and whatnot
      m_fVolume3D = FALSE; // so fade will work nicely
      m_aiVolume[0] = m_aiVolume[1] = 0;  // fade to 0
   }
   m_fWantToExit = TRUE;   // so will exit when can - for MIDI. Wave doesn't really matter

   // clear out sub queue in case more stuff there
   DWORD i;
   PCMMLNode2 *ppn = (PCMMLNode2*)m_lPCMMLNode.Get(0);
   for (i = 0; i < m_lPCMMLNode.Num(); i++)
      delete ppn[i];
   m_lPCMMLNode.Clear();

   PAQTIMEMEM ppm = (PAQTIMEMEM)m_lAQTIMEMEM.Get(0);
   for (i = 0; i < m_lAQTIMEMEM.Num(); i++, ppm++)
      if (ppm->pMem)
         delete ppm->pMem;
   m_lAQTIMEMEM.Clear();

}


/*************************************************************************************
CAudioQueue::VolumeGet - Fills in the volume from the MML. Fills in m_afVolume.
This also fills in m_gID, m_pVolume3D, m_fVolume3D, and fade in/out times
inputs
   PCMMLNode2     pNode - Node
   BOOL           fDoubleVolume - Used by TTS to make sure tts generates wave that's
                     not clipped
returns
   BOOL - TRUE if OK to speak, FALSE if on the user's blacklist and should be muted
*/
BOOL CAudioQueue::VolumeGet (PCMMLNode2 pNode, BOOL fDoubleVolume)
{
   m_gID = GUID_NULL;
   m_aiVolumeObjScale[0] = m_aiVolumeObjScale[1] = AQ_DEFVOLUMEOBJSCALE;
   m_aiVolume[0] = m_aiVolume[1] = AQ_DEFVOLUME;
   m_pVolume3D.Zero4(); // 3d sound
   m_fNoLipSync = FALSE;

#ifdef USEDIRECTX
   m_pVolume3DRot.Zero4();
#endif

   if (pNode) {
      // fade in and out
      fp fFade;
      fFade = MMLValueGetDouble (pNode, gpszFadeIn, 0);
      m_dwFadeInSamples = (DWORD)(fFade * AUDIOTHREAD_SAMPLESPERSEC);
      fFade = MMLValueGetDouble (pNode, gpszFadeOut, 0);
      m_dwFadeOutSamples = (DWORD)(fFade * AUDIOTHREAD_SAMPLESPERSEC);

      // BUGFIX - Make default volume of voice 1/2 so can have louder versions
      m_aiVolume[0] = (int)(MMLValueGetDouble (pNode, gpszVolL, 1) * AQ_DEFVOLUME);
      m_aiVolume[1] = (int)(MMLValueGetDouble (pNode, gpszVolR, 1) * AQ_DEFVOLUME);

      m_fNoLipSync = (BOOL)MMLValueGetInt (pNode, gpszNoLipSync, 0);

      // see if have 3d sound
      MMLValueGetPoint (pNode, gpszVol3D, &m_pVolume3D);

      // potentially double double volume for tts
      if (fDoubleVolume) {
         m_aiVolume[0] *= 4 / 3; // BUGFIX - Was * 2, but since passing volume=75 into TTS, need to counter
         m_aiVolume[1] *= 4 / 3;
         if (m_pVolume3D.p[3])
            m_pVolume3D.p[3] += 2.0; // BIGFIF - was 6.0, but changed to 2 db since increasing only slightly
      }

      // particularly  because direct-sound doesn't handle volume change at the right moment

      // see if can find the ID on the list of volumes. If so, adjust
      GUID gID;
      if (sizeof(gID) == MMLValueGetBinary (pNode, gpszID, (PBYTE)&gID, sizeof(gID))) {
         DWORD i, dwNum;
         m_gID = gID;

         EnterCriticalSection (&m_pAT->m_CritSecTTS);
         dwNum = m_pAT->m_lTTSVoiceLoc.Num();
         GUID *pgVoice = (GUID*)m_pAT->m_lTTSVoiceLoc.Get(0);
         for (i = 0; i < dwNum; i++, pgVoice++)
            if (IsEqualGUID(*pgVoice, gID))
               break;
         LeaveCriticalSection (&m_pAT->m_CritSecTTS);
#if 0 // BUGFIX - Disable LR volume depending on speaker location because is annoying,
         if ((i < dwNum) && (dwNum > 1)) {
            m_aiVolumeObjScale[1] = i * AQ_DEFVOLUMEOBJSCALE*2 / (dwNum-1);
            m_aiVolumeObjScale[0] = AQ_DEFVOLUMEOBJSCALE*2 - m_aiVolumeObjScale[1];
               // BUGFIX - Was using -m_aiVolume[1]
         }
#endif // 0
      } // if has an ID guid
   }

   // 3d volume check
   m_fVolume3D = (m_pVolume3D.p[3] ? TRUE : FALSE);

   // BUGFIX - If volume3D, then automatically clear out m_aiVolumeObjScale
   if (m_fVolume3D)
      m_aiVolumeObjScale[0] = m_aiVolumeObjScale[1] = AQ_DEFVOLUMEOBJSCALE;

   // scale the volume by the object
   m_aiVolume[0] = m_aiVolume[0] * m_aiVolumeObjScale[0] / AQ_DEFVOLUMEOBJSCALE;
   m_aiVolume[1] = m_aiVolume[1] * m_aiVolumeObjScale[1] / AQ_DEFVOLUMEOBJSCALE;

   // 3d volume
   Volume3D (m_pAT->m_fEarsSpeakerSep, m_pAT->m_fEarsBackAtten, m_pAT->m_fEarsPowDistance, m_pAT->m_fEarsScale);
      // sending parameters down separately as thread change protection

   // will need to set "old volume" to the current one so don't do a blend in
   memcpy (m_aiVolumeOld, m_aiVolume, sizeof(m_aiVolume));

   // see if should be muted
   if (!IsEqualGUID(m_gID, GUID_NULL) && IsBlacklisted(&m_gID))
      return FALSE;

   return TRUE;
}



static PWSTR gpszV = L"v";
static PWSTR gpszVal = L"val";


/*************************************************************************************
CAudioQueue::AmbientExpand - This is called by ConsiderNextNode when the current
queue, if it's ambient, should expand.

inputs
   BOOL           fUseCritSec - If TRUE then need to use m_pAT->m_CritSecQueue
returns
   none
*/
void CAudioQueue::AmbientExpand (BOOL fUseCritSec)
{
   if (!m_pAmbient)
      return;  // nothing ambient

   if (fUseCritSec)
      EnterCriticalSection (&m_pAT->m_CritSecQueue);

   PCAmbient *ppa = (PCAmbient*)m_pAT->m_lPCAmbient.Get(0);
   DWORD dwNum = m_pAT->m_lPCAmbient.Num();
   CListFixed lEvent;
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppa[i] == m_pAmbient)
         break;
   if (i >= dwNum) {
      m_pAmbient = NULL;   // no longer exists
      goto done;
   }

   // found a match, so ask for the next
   lEvent.Init (sizeof(PCMMLNode2));
   m_pAmbient->LoopWhatsNext (m_dwAmbientLoop, &m_pAT->m_tAmbientLoopVar, &lEvent);

   if (!lEvent.Num())
      goto done;  // nothing

   // else, new events
   PCMMLNode2 *ppn = (PCMMLNode2*) lEvent.Get(0);
   if (m_lPCMMLNode.Num() || (m_dwPlayMode != AQPM_NONE)) {
      // creat a new queue
      PCAudioQueue pq = m_pAT->NewQueue (FALSE);
      if (!pq)
         goto done;  // shouldn happen

      pq->m_pAmbient = m_pAmbient;
      pq->m_dwAmbientLoop = m_dwAmbientLoop;
      m_pAmbient = NULL;   // so no longer things this is the ambient one

      // add the queue
      pq->m_lPCMMLNode.Required (pq->m_lPCMMLNode.Num() + lEvent.Num());
      pq->m_lAQTIMEMEM.Required (pq->m_lAQTIMEMEM.Num() + lEvent.Num());
      for (i = 0; i < lEvent.Num(); i++) {
         // PCMem pMem = NULL;
         pq->m_lPCMMLNode.Add (&ppn[i]);

         AQTIMEMEM tm;
         memset (&tm, 0, sizeof(tm));
         QueryPerformanceCounter (&tm.iTime);   // since always expand as now
         pq->m_lAQTIMEMEM.Add (&tm);
      }
   }
   else {
      // nothing in this queue, so add to it
      m_lPCMMLNode.Required (m_lPCMMLNode.Num() + lEvent.Num());
      m_lAQTIMEMEM.Required (m_lAQTIMEMEM.Num() + lEvent.Num());
      for (i = 0; i < lEvent.Num(); i++) {
         // PCMem pMem = NULL;
         m_lPCMMLNode.Add (&ppn[i]);

         AQTIMEMEM tm;
         memset (&tm, 0, sizeof(tm));
         QueryPerformanceCounter (&tm.iTime);   // since always expand as now
         m_lAQTIMEMEM.Add (&tm);
      }
   }

done:
   if (fUseCritSec)
      LeaveCriticalSection (&m_pAT->m_CritSecQueue);
}


/*************************************************************************************
CAudioQueue::ConsiderNextNode - This pulls a node off of m_lPCMMLNode and starts
processing it.

inputs
   BOOL           fUseCritSec - If TRUE then need to use m_pAT->m_CritSecQueue
*/
void CAudioQueue::ConsiderNextNode (BOOL fUseCritSec)
{
   // if we're playing a wave AND we're in a loop AND gotten into the fade-out
   // then spawn a new ambient sound
   if ((m_dwPlayMode == AQPM_WAVE) && m_pAmbient &&
      (m_dwExpected != (DWORD)-1) && m_dwFadeOutSamples &&
      (m_dwSample + m_dwFadeOutSamples >= m_dwExpected) &&
      !m_lPCMMLNode.Num())
      AmbientExpand (fUseCritSec);

   // do nothing if already have a wave
   DWORD dwDontLoopForever = 0;
   while ((m_dwPlayMode == AQPM_NONE) && (dwDontLoopForever < 100)) {
      dwDontLoopForever++;

      // get the node
      if (!m_lPCMMLNode.Num())
         AmbientExpand (fUseCritSec);


      if (!m_lPCMMLNode.Num())
         return;  // no more

      // pull from top of list
      PCMMLNode2 pNode = *((PCMMLNode2*)m_lPCMMLNode.Get(0));
      m_lPCMMLNode.Remove (0);
      PAQTIMEMEM ptm = (PAQTIMEMEM)m_lAQTIMEMEM.Get(0);
      AQTIMEMEM tm = *ptm;
      m_lAQTIMEMEM.Remove (0);

      PWSTR psz = pNode->NameGet();
      if (!psz)
         psz =L"";

      if (!_wcsicmp(psz, CircumrealityQueue())) {
         // pull out the first element and work on that.
         // put the rest of the queue back in line
         PCMMLNode2 pSub = NULL;
         pNode->ContentEnum (0, &psz, &pSub);
         pNode->ContentRemove (0, !pSub);
         if (pNode->ContentNum()) {
            m_lPCMMLNode.Insert (0, &pNode);
            m_lAQTIMEMEM.Insert (0, &tm);
         }
         else {
            delete pNode;  // nothing left
            if (tm.pMem)
               delete tm.pMem;
         }
         if (pSub) {
            AQTIMEMEM tmSub = tm;
            tm.pMem = NULL;
            m_lPCMMLNode.Insert (0, &pSub);
            m_lAQTIMEMEM.Insert (0, &tmSub);
         }
      }
      else if (!_wcsicmp(psz, CircumrealityAmbientSounds())) {
         // act on
         m_pAT->AmbientSounds (pNode, fUseCritSec);
         delete pNode;
         if (tm.pMem)
            delete tm.pMem;
         continue;
      }
      else if (!_wcsicmp(psz, CircumrealityAmbientLoopVar())) {
         // get the value
         double fVal = 0;
         if (!pNode->AttribGetDouble (gpszVal, &fVal))
            fVal = 0;

         PWSTR psz = pNode->AttribGetString (gpszV);

         if (psz && psz[0]) {
            if (fUseCritSec)
               EnterCriticalSection (&m_pAT->m_CritSecQueue);

            m_pAT->m_tAmbientLoopVar.Add (psz, &fVal, sizeof(fVal));

            if (fUseCritSec)
               LeaveCriticalSection (&m_pAT->m_CritSecQueue);
         }

         delete pNode;
         if (tm.pMem)
            delete tm.pMem;
         continue;
      }
      else if (!_wcsicmp(psz, CircumrealityDelay())) {
         fp fTime = MMLValueGetDouble (pNode, gpszTime, 0);
         fp fPercent = MMLValueGetDouble (pNode, gpszPercent, 0);

         DELAYINFO di;
         memset (&di, 0, sizeof(di));
         di.fValue = fPercent ? fPercent : fTime;
         di.fValue = max(di.fValue, 0);
         di.fPercent = (fPercent && di.fValue);
         di.dwSample = (di.fPercent ? -1 : (DWORD)(di.fValue * AUDIOTHREAD_SAMPLESPERSEC));
         di.iTime = tm.iTime;

         // add all the delay elements...
         PCMMLNode2 pSub;
         while (pNode->ContentNum()) {
            pSub = NULL;
            pNode->ContentEnum(0, &psz, &pSub);
            pNode->ContentRemove (0, !pSub);
            if (!pSub)
               continue;

            psz = pSub->NameGet();
            if (psz && (!_wcsicmp(psz, gpszTime) || !_wcsicmp(psz, gpszPercent))) {
               // it's a time or percent indicator, which have already gotten
               delete pSub;
               continue;
            }

            di.pNode = pSub;
            m_lDELAYINFO.Add (&di);
         }
         delete pNode;
         if (tm.pMem)
            delete tm.pMem;
      }
      else if (!_wcsicmp(psz, CircumrealityWave())) {
         WCHAR szTemp[256];
         psz = MMLValueGet (pNode, gpszFile);
         if (psz && ((wcslen(psz)+1)*sizeof(WCHAR) < sizeof(szTemp)))
            wcscpy (szTemp, psz);
         else
            szTemp[0] = 0;
         
         // LR volume
         BOOL fBlacklist = !VolumeGet (pNode);
         delete pNode;
         if (tm.pMem)
            delete tm.pMem;

         if (fBlacklist) {
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_dwPlayMode = AQPM_SILENCE;
            m_dwDuration = AUDIOTHREAD_SAMPLESPERSEC / 10;  // very short silence
         }
         else if (szTemp[0]) {
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_pWave = m_pAT->WaveCreate (szTemp);
            if (m_pWave) {
               m_dwPlayMode = AQPM_WAVE;
               m_dwExpected = (DWORD)-1;  // since just created
               m_dwStartFadingOut = (DWORD)-1;
            }
            else
               dwDontLoopForever++;  // just for easy debug
         }
      }
      else if (!_wcsicmp(psz, CircumrealityVoiceChat())) {
         // LR volume
         BOOL fBlacklist = !VolumeGet (pNode);
         delete pNode;

         if (fBlacklist) {
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_dwPlayMode = AQPM_SILENCE;
            m_dwDuration = AUDIOTHREAD_SAMPLESPERSEC / 10;  // very short silence
            if (tm.pMem)
               delete tm.pMem;
         }
         else {
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_pWave = m_pAT->VoiceChatWaveCreate (tm.pMem, &m_VoiceDisguise);
            if (m_pWave) {
               m_dwPlayMode = AQPM_WAVE;
               m_dwExpected = (DWORD)-1;  // since just created
               m_dwStartFadingOut = (DWORD)-1;
            }
         }
      }
      else if (!_wcsicmp(psz, CircumrealityMusic())) {
         WCHAR szTemp[256];
         psz = MMLValueGet (pNode, gpszFile);
         if (psz && ((wcslen(psz)+1)*sizeof(WCHAR) < sizeof(szTemp)))
            wcscpy (szTemp, psz);
         else
            szTemp[0] = 0;

         CMem memBinary;
         if (!szTemp[0])
            MMLValueGetBinary (pNode, gpszBinary, &memBinary);

#if 0 // HACK to make sure that binary works
         FILE *f;
         f = fopen("c:\\midi\\secretag.mid", "rb");
         if (f) {
            fseek (f, 0, SEEK_END);
            int iSize = ftell (f);
            fseek (f, 0, SEEK_SET);
            memBinary.Required ((DWORD)iSize);
            memBinary.m_dwCurPosn = (DWORD) iSize;
            fread (memBinary.p, 1, iSize, f);
            fclose (f);
            szTemp[0] = 0;
         }
#endif // 0
         
         // LR volume
         BOOL fBlacklist = !VolumeGet (pNode);
         delete pNode;
         if (tm.pMem)
            delete tm.pMem;

         if (fBlacklist) {
            // wants muted, so a bit of silence
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_dwPlayMode = AQPM_SILENCE;
            m_dwDuration = AUDIOTHREAD_SAMPLESPERSEC / 10;  // very short silence
         }
         else if (szTemp[0]) {
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_pMidi = m_pAT->MidiCreate (szTemp);
            if (m_pMidi) {
               m_dwPlayMode = AQPM_MIDI;

               m_pMidi->VolumeSet ((WORD)abs(m_aiVolume[0]), (WORD)abs(m_aiVolume[1]));
            }
         }
         else if (memBinary.m_dwCurPosn) {
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_pMidi = m_pAT->MidiCreateAlways (L"", TRUE);
            if (m_pMidi) {
               // since binary, finished loading right away
               PATWAVE pat = (PATWAVE)m_pMidi->m_pUserData;
               pat->fFinishedLoading = TRUE;
               pat->fUseCritical = FALSE;

               if (!m_pMidi->Open (memBinary.p, (DWORD)memBinary.m_dwCurPosn)) {
                  m_pAT->MidiDelete (m_pMidi);
                  m_pMidi = NULL;
               }
               else {
                  // loaded successfully
                  DWORD dwTotal = (DWORD)(m_pMidi->m_fTimeTotal * (double)AUDIOTHREAD_SAMPLESPERSEC);
                  dwTotal = max(dwTotal, 1);
                  pat->dwSampleExpected = pat->dwSampleValid = dwTotal;
                  m_dwSample = 0;
                  m_iTimeStarted.QuadPart = 0;
                  m_dwPlayMode = AQPM_MIDI;
                  m_pMidi->VolumeSet ((WORD)abs(m_aiVolume[0]), (WORD)abs(m_aiVolume[1]));
               }
            } // if m_pMidi
         } // if have binary version of MIDI
      } // if have Music
      else if (!_wcsicmp(psz, CircumrealitySilence())) {
         fp fTime = MMLValueGetDouble (pNode, gpszTime, 1);
         delete pNode;
         if (tm.pMem)
            delete tm.pMem;

         if (fTime > 0) {
            m_dwSample = 0;
            m_iTimeStarted.QuadPart = 0;
            m_dwPlayMode = AQPM_SILENCE;
            m_dwDuration = (DWORD)(fTime * AUDIOTHREAD_SAMPLESPERSEC);
         }
      }
      else if (!_wcsicmp(psz, CircumrealitySpeak())) {
         // find the volume
         PCMMLNode2 pSub = NULL;
         PWSTR psz;
         pNode->ContentEnum (pNode->ContentFind(L"voice"), &psz, &pSub);
         VolumeGet (pSub, TRUE);
            // NOTE: Ignoring blacklist because its dealt with in TTSSpeak

         // operate on wave
         m_dwSample = 0;
         m_iTimeStarted.QuadPart = 0;
         m_pWave = m_pAT->TTSSpeak (pNode, tm.iTime);
         if (tm.pMem)
            delete tm.pMem;
         if (m_pWave) {
            m_dwPlayMode = AQPM_WAVE;
            m_dwExpected = (DWORD)-1; // since just created
            m_dwStartFadingOut = (DWORD)-1;
         }
      }
      else {
         // unknown, so send it on
         m_pAT->RequestAudio (pNode, tm.iTime, FALSE, FALSE);
         if (tm.pMem)
            delete tm.pMem;
      }
   }  // while !m_pWave
}



/*************************************************************************************
CAudioThread::TTSSurroundSet - Sets the location of different voices in the room
so that they'll come from different directions in the LR speakers.

inputs
   GUID           *pgID - List of dwNum object GUIDs that might speak.
   DWORD          dwNum - Number of pgID
returns
   none
*/
void CAudioThread::TTSSurroundSet (GUID *pgID, WORD dwNum)
{
   EnterCriticalSection (&m_CritSecTTS);
   m_lTTSVoiceLoc.Init (sizeof(GUID), pgID, dwNum);
   LeaveCriticalSection (&m_CritSecTTS);
}



/*************************************************************************************
CAudioThread::PostMidiMessage - Posts a midi message to the main window so
it will be played. This is used to play dings and whatnot from escarpment.

inputs
   DWORD          dwMessage - Message is 0 to create the MIDI instance, 1 to
                              release the MIDI instance, 2 to send a MIDI message (using dwData)
   DWORD          dwData - Data for the MIDI message.
*/
void CAudioThread::PostMidiMessage (DWORD dwMessage, DWORD dwData)
{
   if (m_hWnd)
      PostMessage (m_hWnd, WM_MYMIDIMESSAGE, (WPARAM) dwMessage, (LPARAM) dwData);
}


/*************************************************************************************
CAudioThread::VisemeQueue - Queues up a viseme to be played at a specific time.

NOTE: It's assumed that is is called from within the audio thread's thread.

inputs
   LARGE_INTEGER          iTime - From QueryPerformanceCounter() for when it should be set off
   GUID           *pgID - GUID of the object
   DWORD          dwEnglishPhone - English phoneme
   BOOL              fUseCritSec - If TRUE then need to turn critical section on
*/
void CAudioThread::VisemeQueue (LARGE_INTEGER iTime, GUID *pgID, DWORD dwEnglishPhone, BOOL fUseCritSec)
{
   // must have a GUID
   if (IsEqualGUID(*pgID, GUID_NULL))
      return;

   // add this
   VISEMEMESSAGE vm;
   vm.dwViseme = EnglishPhoneToViseme (dwEnglishPhone);
   vm.gID = *pgID;
   vm.iTime = iTime;

   if (fUseCritSec)
      EnterCriticalSection (&m_CritSecQueue);
   m_lVISEMEMESSAGE.Add (&vm);
   if (fUseCritSec)
      LeaveCriticalSection (&m_CritSecQueue);
}


/*************************************************************************************
CAudioThread::VisemePost - Posts a viseme to the main treahd

inputs
   PVISEMEMESSAGE       pvm - Message
*/
void CAudioThread::VisemePost (PVISEMEMESSAGE pvm)
{
   PCMem pMem = new CMem;
   if (!pMem->Required (sizeof(*pvm)) ) {
      delete pMem;
      return;
   }

   memcpy (pMem->p, pvm, sizeof(*pvm));

   PostMessage (gpMainWindow->m_hWndPrimary, WM_MAINWINDOWNOTIFYVISEMEMESSAGE, (WPARAM) GetTickCount(), (LPARAM)pMem);
}

/*************************************************************************************
CAudioThread::Vis360Changed - Call this when the 360 degree rotation has changed

inputs
   fp          fLong - Longitude in radians, 0 = N, PI/2=e
   fp          fLat - Latitute in radians, 0=ahea, PI/2 = up, -PI/2 = down
*/
void CAudioThread::Vis360Changed (fp fLong, fp fLat)
{
   EnterCriticalSection (&m_CritSecQueue);

   // if they're the same as before do nothing
   if ((fLong == m_fLong) && (fLat == m_fLat)) {
      LeaveCriticalSection (&m_CritSecQueue);
      return;
   }

   // else change
   m_fLong = fLong;
   m_fLat = fLat;

   // update all
   DWORD i;
   PCAudioQueue *ppq = (PCAudioQueue*)m_lPCAudioQueue.Get(0);
   for (i = 0; i < m_lPCAudioQueue.Num(); i++) {
      PCAudioQueue pq = ppq[i];

      pq->Volume3D (m_fEarsSpeakerSep, m_fEarsBackAtten, m_fEarsPowDistance, m_fEarsScale);
         // sending parameters down separately as thread change protection
   } // i

   LeaveCriticalSection (&m_CritSecQueue);
}


/*************************************************************************************
CAudioThread::Mute - Mutes or unmutes the audio

inputs
   BOOL           fMute - Set to TRUE if want to mute, false if not
*/
void CAudioThread::Mute (BOOL fMute)
{
   m_fWantToMute = fMute;
}


/*************************************************************************************
CAudioThread::HandleMute - THis is an internal function that handles muting and
unmuting. Called inthe audio thread.
*/
void CAudioThread::HandleMute (void)
{
   if (m_fWantToMute) {
      // if already muted, do nothng
      if (m_fMuted)
         return;
      m_fMuted = TRUE;

      // pause
#ifdef USEDIRECTX
      m_bufDrone.PauseResume (TRUE);
      // loop through all the buffers and add
      EnterCriticalSection (&m_CritSecQueue);
      DWORD i;
         for (i = 0; i < m_lPCAudioQueue.Num(); i++) {
         PCAudioQueue *ppq = (PCAudioQueue*)m_lPCAudioQueue.Get(i);
         if (!ppq)
            continue;
         PCAudioQueue pq = *ppq;

         pq->m_bufDX.PauseResume (TRUE);
      } // i
      LeaveCriticalSection (&m_CritSecQueue);
#else
      if (m_hWaveOut)
         waveOutPause (m_hWaveOut);
#endif
      if (m_pMidiMix)
         midiOutGetVolume (m_pMidiMix->m_hMidi, &m_dwMidiVol);
      else
         m_dwMidiVol = (DWORD)-1;
   }
   else {
      // if not muted then do nothing
      if (!m_fMuted)
         return;
      m_fMuted = FALSE;

      // resume
#ifdef USEDIRECTX
      m_bufDrone.PauseResume (FALSE);
      // loop through all the buffers and add
      EnterCriticalSection (&m_CritSecQueue);
      DWORD i;
         for (i = 0; i < m_lPCAudioQueue.Num(); i++) {
         PCAudioQueue *ppq = (PCAudioQueue*)m_lPCAudioQueue.Get(i);
         if (!ppq)
            continue;
         PCAudioQueue pq = *ppq;

         pq->m_bufDX.PauseResume (FALSE);
      } // i
      LeaveCriticalSection (&m_CritSecQueue);
#else
      if (m_hWaveOut)
         waveOutRestart (m_hWaveOut);
#endif
      if (m_pMidiMix)
         midiOutSetVolume (m_pMidiMix->m_hMidi, m_dwMidiVol);
   }
}

// DOCUMENT: Should only send <speak> to main queue, not instantaneous audio
// because can only speak one thing at a time, no amtter where

// DOCUMENT: Will need to document tags for tts


