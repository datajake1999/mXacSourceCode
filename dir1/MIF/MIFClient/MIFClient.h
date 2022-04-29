/*********************************************************************************
MIFClient.h - header specific to the client
*/

#ifndef _MIFCLIENT_H_
#define _MIFCLIENT_H_

#define UNIFIEDTRANSCRIPT  // if set the unified transcript, command, and menu

#define USEDIRECTX         // set to use directx for playback, otherwise wave
// #define USEPASSWORDSTORE   // store passwords in logon file
   // BUGFIX - Not using password store for two reasons:
   // 1. What happens if password is changed elsewhere. user can't log on.
   // 2. Not safe, especially in far-east where the password would be saved on the computer

#define LIMITSIZE             1000000000     // limit to cache size, 1 gig

/***********************************************************************************
CPieChart */
class DLLEXPORT CPieChart {
   friend LRESULT CALLBACK CPieChartWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
   ESCNEWDELETE;

   CPieChart(void);
   ~CPieChart(void);

   BOOL Init (HINSTANCE hInstance, PWSTR pszTip, RECT *pr, HWND hWndParent, DWORD dwID);
   BOOL Move (RECT *pr);
   void Enable (BOOL fEnable);
   void ColorSet (COLORREF cBackground, COLORREF cPie);
   void PieSet (fp fValue, fp fValueDelta);

   HWND           m_hWnd;        // window handle
   CMem           m_memTip;      // tip string
   HBITMAP        m_hBmp;        // bitmap image
   DWORD          m_dwID;        // notification command ID if clicked
   PCToolTip      m_pTip;        // tooltip
   BOOL           m_fTimer;      // set to true if the timer is on
   DWORD          m_dwTimerOn;   // mumber of milliseconds timer is on
   COLORREF       m_cBackground; // background color
   COLORREF       m_cPie;        // pie color
   fp             m_fValue;      // value displayed by the pie
   fp             m_fValueDelta; // change in the value every second
   fp             m_fValueDrawn; // value that's drawn
   DWORD          m_dwLastTick;  // last tick count when got WM_TIMER

private:
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

typedef CPieChart *PCPieChart;


/*************************************************************************************
CInternetThread */

BOOL RunCircumrealityClient (void);

class CInternetThread {
public:
   ESCNEWDELETE;

   CInternetThread (void);
   ~CInternetThread (void);

   // called from the main thread
   BOOL Connect (PWSTR pszFile, BOOL fRemote, DWORD dwIP, DWORD dwPort, HWND hWndNotify, DWORD dwMessage,
      int *piWinSockErr);
   BOOL Disconnect (void);
   BOOL FileRequest (PWSTR pszFile, BOOL fIgnoreDir);
   BOOL FileCheckRequest (PWSTR pszFile);


   // called from the within the internet thread
   void ThreadProc (void);
   void ThreadProcLow (void);

   // public
   PCCircumrealityPacket       m_pPacket;      // object to send/receive packets
   WCHAR             m_szError[512];      // error to display if Connect fails().

private:
   void BinaryDataRefresh (PWSTR pszFile);

   HANDLE            m_hThread;        // thread handle
   HANDLE            m_hSignalFromThread; // set when the thread has information
   HANDLE            m_hSignalToThread;   // set when the thread should shut down
   BOOL              m_fConnected;     // set to TRUE if connected, FALSE if not
   HWND              m_hWndNotify;     // window to notify if get incoming message
   DWORD             m_dwMessage;      // message to send to window if incoming message
   BOOL              m_fShuttingDown;  // set to TRUE when shutting down. Use with m_CritSec
   HANDLE            m_hThreadLow;     // thread handle for the low-priority thread
   HANDLE            m_hSignalToThreadLow;   // set when data has been added to low-priority queue
   CListFixed        m_lTHREADLOWMSG;  // messages to low-priority thread

   // about the connection
   WCHAR             m_szConnectFile[256]; // file connected to
   BOOL              m_fConnectRemote; // TRUE if connection is remote, FALSE if is local
   DWORD             m_dwConnectIP;    // IP address of connection, if remote
   DWORD             m_dwConnectPort;  // Port address of connection, if remote
   int               m_iWinSockErr;    // winsock err
   SOCKET            m_iSocket;        // current socket

   // under critical section
   CRITICAL_SECTION  m_CritSec;        // to protect some variables
   CListVariable     m_lFileRequest;   // list of files on the request list
   CBTree            m_tFileBad;       // tree of files requests that have gone bad
};
typedef CInternetThread *PCInternetThread;


/*************************************************************************************
CServerLoadThread */
class CServerLoadThread {
public:
   ESCNEWDELETE;

   CServerLoadThread (void);
   ~CServerLoadThread (void);

   // called from the main thread
   BOOL Connect (PWSTR pszDomain, DWORD dwPort, PCIRCUMREALITYSERVERLOAD pServerLoad,
      HANDLE hSignalDone, int *piWinSockErr);
   BOOL Disconnect (void);


   // called from the within the internet thread
   void ThreadProc (void);

   // public
   PCCircumrealityPacket       m_pPacket;      // object to send/receive packets
   WCHAR                      m_szError[512];   // error if server load thread fails

private:
   void BinaryDataRefresh (PWSTR pszFile);

   HANDLE            m_hThread;        // thread handle
   HANDLE            m_hSignalFromThread; // set when the thread has information
   HANDLE            m_hSignalToThread;   // set when the thread should shut down
   BOOL              m_fConnected;     // set to TRUE if connected, FALSE if not

   // about the connection
   WCHAR             m_szDomain[256];  // domain name connected to, or IP address as string
   DWORD             m_dwConnectIP;    // IP address of connection, if remote
   DWORD             m_dwConnectPort;  // Port address of connection, if remote
   int               m_iWinSockErr;    // winsock err
   SOCKET            m_iSocket;        // current socket
   PCIRCUMREALITYSERVERLOAD m_pServerLoad;   // where to write the server load info
   HANDLE            m_hSignalFinal;   // final signal from the thread indicating that pServerLoad is filled in

   // under critical section
   CRITICAL_SECTION  m_CritSec;        // to protect some variables
};
typedef CServerLoadThread *PCServerLoadThread;


// SERVERLOADQUERY - Information for ServerLoadQuery
typedef struct {
   WCHAR             szDomain[256];    // either a domain name, like http://www.mxac.com.au, or an IP address
   DWORD             dwPort;           // port to connect to
   CIRCUMREALITYSERVERLOAD ServerLoad; // filled with the server load by ServerLoadQuery()
   PVOID             pUserData;        // user data
} SERVERLOADQUERY, *PSERVERLOADQUERY;

void ServerLoadQuery (PSERVERLOADQUERY paQuery, DWORD dwNum, HWND hWndProgress);


/*************************************************************************************
CRenderThread */

#define MAXQUEUETOSEND_RENDER       2000000  // queue up render cache if 2 meg or less

#define RENDERTHREAD_RENDERED       1     // sent to indicate that an image was rendered
#define RENDERTHREAD_RENDEREDHIGH   2     // high quality render has been drawn
#define RENDERTHREAD_PROGRESS       3     // send to indicte that progress has been made in rendering.
                                          // lParam / 1000000 = progress
#define RENDERTHREAD_IMBORED        4     // renderthread has nothing to do
#define RENDERTHREAD_PRELOAD        5     // sent to indicate that may want to load an image


#define RTQUEUENUM                  8     // 8 queues, with high-quality alternating odd values

#define RTQUEUE_HIGHQUALITYBIT      0x01  // or/add this in with RTQUEUE_HIGHEST, etc.

#define RTQUEUE_HIGH                0
#define RTQUEUE_MEDIUM              2
#define RTQUEUE_LOW                 4
#define RTQUEUE_PRERENDER           6     // prerender

extern PWSTR gpszResolutionFileInfo;
fp ResolutionToRenderScale (int iResolution);

// RTQUEUE - Stores information about a render task in the queue
typedef struct {
   PCMMLNode2     pNode;      // copy of the node passed in
   PCMem          pMem;       // memory containin a string version of the node passed in
   BOOL           fIs360;     // set to TRUE if this a 360-degree image
   int            iDirection; // used for prerender to indicate direction its in, so can prerender
                              // in general direction that player is looking first
   DWORD          dwTime;     // GetTickCount() when this came in
   DWORD          dwTimesFailed; // number of times that failed, so dont get in infinite loop
   BOOL           fDownloadRequested;  // set to TRUE if tried to download from server already
} RTQUEUE, *PRTQUEUE;

class CRenderThread;
typedef CRenderThread *PCRenderThread;

// CRenderThreadProgress - Handles progress for each of the thread
class CRenderThreadProgress : public CProgressSocket, public CRenderScene360Callback  {
public:
   ESCNEWDELETE;

   DWORD             m_dwRenderShard;    // render shard which this is associated
   PCRenderThread    m_pRT;      // render thread

   // from CProgressSocket, from within internet thread
   virtual BOOL Push (float fMin, float fMax);
   virtual BOOL Pop (void);
   virtual int Update (float fProgress);
   virtual BOOL WantToCancel (void);
   virtual void CanRedraw (void);

   // CRenderSceneCallback
   virtual void RS360LongLatGet (fp *pfLong, fp *pfLat);
   virtual void RS360Update (PCImageStore pImage);
};
typedef CRenderThreadProgress *PCRenderThreadProgress;

class CRenderThread  {
public:
   // BUGBUG - had temporarily disabled this to find memory leak
   // BUGBUG - re-enabled ESCNEWDELTE, as it should be
   ESCNEWDELETE;

   CRenderThread (void);
   ~CRenderThread (void);

   // called from the main thread
   BOOL Connect (HWND hWndNotify, DWORD dwMessage);
   BOOL Disconnect (void);

   // thread safe
   BOOL RequestRender (PCMMLNode2 pNode, DWORD dwPriority, int iDirection, BOOL fNotInList);
   BOOL BumpRenderPriority (PCWSTR pszImageString, BOOL fCreateIfNotExist);
   void CanLoad3D (void);
   void ResolutionSet (int iResolution, DWORD dwShadowsFlags, int iTextureDetail, BOOL fLipSync,
                                   int iResolutionLow, DWORD dwShadowsFlagsLow, BOOL fLipSyncLow, BOOL fClear);
   void ResolutionCheck (void);
   void Vis360Changed (fp fLong, fp fLat);
   BOOL FindCanddiateToDownload (PCMem pMemString, BOOL *pfExpectHighQuality);

   // called from the within the internet thread
   void ThreadProc (DWORD dwRenderShard);
   void ThreadProcSave (void);
   
   // from CProgressSocket, from within internet thread
   BOOL MyPush (DWORD dwRenderShard, float fMin, float fMax);
   BOOL MyPop (DWORD dwRenderShard);
   int MyUpdate (DWORD dwRenderShard, float fProgress);
   BOOL MyWantToCancel (DWORD dwRenderShard);
   void MyCanRedraw (DWORD dwRenderShard);

   // CRenderSceneCallback
   void MyRS360LongLatGet (DWORD dwRenderShard, fp *pfLong, fp *pfLat);
   void MyRS360Update (DWORD dwRenderShard, PCImageStore pImage);

   // look but don't touch
   DWORD             m_dwRenderShards;    // number of render shards to use

private:
   CRenderThreadProgress m_aRTP[MAXRENDERSHARDS];

   HANDLE            m_ahThread[MAXRENDERSHARDS];        // thread handle
   HANDLE            m_ahSignalFromThread[MAXRENDERSHARDS]; // set when the thread has information
   HANDLE            m_ahSignalToThread[MAXRENDERSHARDS];   // set when the thread should shut down

   HANDLE            m_hThreadSave;    // thread handle for saving
   HANDLE            m_hSignalFromThreadSave;   // thread handle for saving
   HANDLE            m_hSignalToThreadSave;  // shut down signal for saving

   BOOL              m_afConnected[MAXRENDERSHARDS];     // set to TRUE if connected, FALSE if not
   HWND              m_hWndNotify;     // window to notify if get incoming message
   DWORD             m_dwMessage;      // message to send to window if incoming message
   BOOL              m_afWantToQuit[MAXRENDERSHARDS];    // set to TRUE on a disconnect, so will stop rendering when close
   BOOL              m_fWantToQuitSave;   // set to TRUE when want save to quit
   int               m_iResolution;    // how much detail to use, 0 for normal, - for low, + for high
   int               m_iResolutionLow; // resolution for low-quality quick render
   int               m_iTextureDetail; // texture detail to use. 0 for normal, -1 for low, +1 (or more) for high
   BOOL              m_fLipSync;       // set to TRUE if lip sync should happen
   BOOL              m_fLipSyncLow;    // for low-quality quick render
   DWORD             m_dwShadowsFlags; // flags to pass into shadows more, SF_XXX from CRenderTraditional
   DWORD             m_dwShadowsFlagsLow; // shadows for low-quality quick render
   fp                m_f360Long;       // longitude looking, radiuans, 0=n, PI/2=e, etc.
   fp                m_f360Lat;        // latitude looking, radians, 0=level, PI/2=up, etc.

   BOOL DoRender (DWORD dwRenderShard);
   PCImageStore DoRenderImage (DWORD dwRenderShard, PCMMLNode2 pNode);
   PCImageStore DoRenderScene (DWORD dwRenderShard, PCMMLNode2 pNode, BOOL fScene, BOOL fPreRender, DWORD dwQuality);
   PCImageStore DoRenderTitle (DWORD dwRenderShard, PCMMLNode2 pNode, DWORD dwQuality);
   void PostImageRendered (PCImageStore pis, BOOL fSave, BOOL fPostToMain, DWORD dwQuality,
      BOOL fDontPostIfFileExists);
   void PostImageRenderedAsThread (PCImageStore pis, BOOL fSave, BOOL fPostToMain, DWORD dwQuality,
      DWORD dwFinalQualityImage);
   BOOL IsNode360 (PCMMLNode2 pNode);
   void Calc360InQueue (void);

   __int64           m_iPhysicalTotal; // total physical memory on the computer
   __int64           m_iPhysicalTotalHalf;   // half way point when start freeing up resources

   CMem              m_amem360Calc[MAXRENDERSHARDS];     // cache of 360 calc for optimization
   // progress
   DWORD             m_adwLastUpdateTime[MAXRENDERSHARDS];     // last time update was called, GetTickCount()
   fp                m_afProgress[MAXRENDERSHARDS];            // progress, from 0..1
   CListFixed        m_alStack[MAXRENDERSHARDS];               // stack containing the current min and max - to scale Update() call

   // under critical section
   CRITICAL_SECTION  m_CritSecSave;    // critical section for save data
   CListFixed        m_lPOSTIMAGERENDERED;   // to save

   CRITICAL_SECTION  m_CritSec;        // to protect some variables
   CListFixed        m_alRTQUEUE[RTQUEUENUM];   // queue for stuff to be rendered,
                                       // RTQUEUE_HIGHEST, etc.
   DWORD             m_dw360InQueue;   // number of 360 images in the low-quality queue (excluding pre-render)
   DWORD             m_dw360InQueueQuality;  // number of 360 in high quality queue (excluding pre-render)
   BOOL              m_afRendering360[MAXRENDERSHARDS];  // set to TRUE if currently rendering a 360 degree image
   BOOL              m_afDontAbort360[MAXRENDERSHARDS];  // set to TRUE if <DontAbort360 v=1/> set to TRUE
   BOOL              m_afRenderingPreRender[MAXRENDERSHARDS];  // set to TRUE if currently rendering a pre-render image
   BOOL              m_adwRenderingHighQuality[MAXRENDERSHARDS];   // set to TRUE if rendering a high-quality image
   BOOL              m_afM3DInitialized[MAXRENDERSHARDS];   // set to TRUE if 3D routines have been initialized
   BOOL              m_afM3DCanStart[MAXRENDERSHARDS];   // set to TRUE if can start 3d routnes

   RTQUEUE           m_artCurrent[MAXRENDERSHARDS];      // what currently rendering

   // for freeing
   DWORD             m_adwCacheFree[MAXRENDERSHARDS];  // if non-zero then want to free memory. GetTickCount() time
   DWORD             m_adwCacheFreeLast[MAXRENDERSHARDS];   // last time freed up, 0 if didn't
   BOOL              m_afCacheFreeBig[MAXRENDERSHARDS];  // if TRUE want to to a big clear

   // used for renderscene callback
   PCMMLNode2        m_apRS360Node[MAXRENDERSHARDS];     // current node
};
typedef CRenderThread *PCRenderThread;


#define NUMVISEMES               9        // number of visemes
#define VISEME_BLINK             (NUMVISEMES-1)    // blink

DWORD EnglishPhoneToViseme (DWORD dwPhone);
BOOL VisemeModifyMML (DWORD dwViseme, PWSTR pszMML, PCMem pMem);

/*************************************************************************************
CMidiMix */
#define MIDICHANNELS       16
#define DRUMCHANNEL        9     // channel 10, but convert to 0-base

class CMidiInstance;
typedef CMidiInstance *PCMidiInstance;

// CMidiMix - Mixes a number of independent MIDI playback instances together
class CMidiMix {
   friend class CMidiInstance;

public:
   ESCNEWDELETE;

   CMidiMix (void);
   ~CMidiMix (void);

   PCMidiInstance InstanceNew (void);

   // can look, but dont touch
   HMIDIOUT          m_hMidi;       // midi output device
private:
   DWORD ChannelClaim (DWORD dwOrigChannel);
   void ChannelRelease (DWORD dwChannel);
   BOOL MidiMessage (BYTE bStatus = 0, BYTE bData1 = 0, BYTE bData2 = 0);

   DWORD             m_dwTimeCount; // time counter
   BOOL              m_afClaimed[MIDICHANNELS]; // if claimed or not
   DWORD             m_adwLastClaimed[MIDICHANNELS];   // when last claimed, so fnid the oldes unclaimed
};
typedef CMidiMix *PCMidiMix;


// MINSTCHANNEL - Information on a chanell for the Midi instance
typedef struct {
   DWORD             dwChannelMap;  // the channel is mapped to this one, or -1 if not mapped
   DWORD             adwNoteOn[128/32];   // one bit for each note that's on
   WORD              wMidiVolume;   // MIDI volume being used
   WORD              wMidiPan;      // MIDI pan being used
} MINSTCHANNEL, *PMINSTCHANNEL;

// CMidiInstance - An independent playback instance
class CMidiInstance {
public:
   ESCNEWDELETE;

   CMidiInstance (PCMidiMix pMidiMix);
   ~CMidiInstance (void);

   BOOL MidiMessage (BYTE bStatus = 0, BYTE bData1 = 0, BYTE bData2 = 0);
   void VolumeSet (WORD wLeft, WORD wRight);

private:
   BOOL ChannelClaimIfNotAlready (DWORD dwChannel);
   BOOL ChannelRelease (DWORD dwChannel);
   BOOL AllNotesOff (DWORD dwChannel);
   void VolumeSetInternal (DWORD dwChannel, DWORD dwFlags);

   MINSTCHANNEL      m_aChannel[MIDICHANNELS];  // info on each channel
   PCMidiMix         m_pMidiMix;       // mix
   WORD              m_awVolume[2];    // volume to play at
};


/*************************************************************************************
CMidiFile */
// MFTRACKINFO - Information about the track
typedef struct {
   DWORD          dwOffsetStart;    // offset for the track start
   DWORD          dwSize;           // size fo the track (from the dwOffsetStart)
   DWORD          dwOffsetCur;      // current offset
   double         fTimeTotal;       // total duration of the track, in seconds
   double         fDelayTillNext;   // delay until should pull out the next bit of MIDI
   BOOL           fReachedEOT;      // set to TRUE if have reached end of track
   BYTE           bLastCommand;     // for running MIDI
} MFTRACKINFO, *PMFTRACKINFO;

class CMidiFile {
public:
   ESCNEWDELETE;

   CMidiFile (void);
   ~CMidiFile (void);

   BOOL Open (PVOID pData, DWORD dwSize);
   BOOL Open (PWSTR pszFile);
   BOOL Start (PCMidiMix pMidiMix);
   BOOL Stop (void);
   BOOL PlayAdvance (double fDelta);
   void VolumeSet (WORD wLeft, WORD wRight);

   // user
   PVOID          m_pUserData;      // user data

   // read but dont touch
   double         m_fTimeTotal;     // total duration of the music in seconds

private:
   BOOL OpenInternal ();
   BOOL GetHeader (DWORD *pdwOffset, DWORD *pdwID, DWORD *pdwSize);
   BOOL GetWORD (DWORD *pdwOffset, WORD *pw, BOOL fBigEndIn = FALSE);
   BOOL GetDWORD (DWORD *pdwOffset, DWORD *pdw, BOOL fBigEndIn = FALSE);
   BOOL GetVarLength (DWORD *pdwOffset, DWORD *pdwValue);
   double GetDeltaTime (PMFTRACKINFO pti);
   BOOL ActOnMidi (PMFTRACKINFO pti, PCMidiInstance pInstance);
   void TrackPullOutDelay (PMFTRACKINFO pti);
   double PlayAdvanceTrack (PMFTRACKINFO pti, double fDelta);

   CMem           m_memData;        // data from the file, m_dwCurPosn is filled with the size
   PCMidiInstance m_pMidiInstance;  // MIDI isntance playing to

   DWORD          m_dwTicksPerBeat; // ticks per beat
   double         m_fBeatDur;       // duration of a beat
   double         m_fTickDurHeader; // tick duration from the header
   DWORD          m_dwTrackFormat;  // 0 for single, 1 for synchronous, 2 for asychronous
   CListFixed     m_lMFTRACKINFO;   // track information
   DWORD          m_dwCurTrack;     // current track that playing on, if synchronous
   __int64        m_iPerfFreq;      // from QueryPerformanceFrequency()
   __int64        m_iLastTime;      // from QueryPerformanceCounter(), last time that collected info

   WORD           m_awVolume[2];    // volume to play at
};
typedef CMidiFile *PCMidiFile;


/*************************************************************************************
CAudioThread */


// CAudioQueue - Queue of audio events
#define AQ_NOVOLCHANGE           0x1000      // volume level where there's no volume change
#define AQ_DEFVOLUME             (AQ_NOVOLCHANGE*2/3)      // default volume level
   // BUGFIX - Was AQ_NOVOLCHANGE/2, but changed to 2/3 since cant control volume that well on notebook
#define AQ_DEFVOLUMEOBJSCALE     0x100       // 256

#define AQPM_NONE                0           // not playing anything
#define AQPM_WAVE                1           // playing wave
#define AQPM_SILENCE             2           // playing silence
#define AQPM_MIDI                3           // playing MIDI


// VISEMEMESSAGE
typedef struct {
   LARGE_INTEGER     iTime;         // time when is supposed to be set off
   GUID              gID;           // ID of the object speaking
   DWORD             dwViseme;      // viseme number, from EnglishPhoneToViseme()
} VISEMEMESSAGE, *PVISEMEMESSAGE;

class CAudioThread;
typedef CAudioThread *PCAudioThread;


#ifdef USEDIRECTX

// #define LPDIRECTSOUND8     LPDIRECTSOUND
// #define LPDIRECTSOUNDBUFFER8 LPDIRECTSOUNDBUFFER
// #define LPDIRECTSOUND3DBUFFER8 LPDIRECTSOUND3DBUFFER

// CDXBuffer - for managing directx buffer
class CDXBuffer {
public:
   ESCNEWDELETE;

   CDXBuffer (void);
   ~CDXBuffer (void);
   BOOL Init (LPDIRECTSOUND8 pDirectSound, DWORD dwSamplesPerSec, DWORD dwChannels, BOOL f3D);
   BOOL Release (void);

   BOOL SetFormatGlobal (void);

   BOOL GetCurrentPosition (__int64 *piPlay = NULL, __int64 *piWrite = NULL, DWORD *pdwPlay = NULL, DWORD *pdwWrite = NULL);
   BOOL FinishedPlaying (void);
   DWORD Recommend (DWORD *pdwUrgent);
   BOOL Write (PBYTE pbData, DWORD dwSize);
   void PauseResume (BOOL fPause);
   BOOL SetVolume (int iLeft, int iRight);
   BOOL SetVolume (PCPoint pVolume, fp fDB);

   BOOL HasStartedPlaying (void)
      {
         return m_fStartedPlaying && !m_fFinishedPlaying;
      };

   // look, but dont touch
   BOOL              m_f3D;   // set to TRUE if 3D
   DWORD             m_dwSamplesPerSec;   // sampling rate
   DWORD             m_dwChannels;        // chanels
   // assume 16 bit audio

private:
   LPDIRECTSOUNDBUFFER8    m_pDSB;     // buffer
   LPDIRECTSOUND3DBUFFER8  m_p3D;      // 3d buffer
   BOOL              m_fStartedPlaying;   // set to TRUE if started playing
   BOOL              m_fFinishedPlaying;  // set to TRUE when finished
   BOOL              m_fPaused; // set to TRUE if paused
   DWORD             m_dwBufSize;         // buffer size, in bytes
   int               m_iVolumeLeft;       // left volume, AQ_NOVOLCHANGE = no change
   int               m_iVolumeRight;      // right volume, AQ_NOVOLCHANGE = no change
   CPoint            m_pVolume3D;         // 3D volume, with p[3] being decibles (60 = normal speak)

   DWORD             m_dwLastPlayPosn;    // last play position, modulo
   DWORD             m_dwLastWritePosn;   // last write position, module
   __int64           m_iTotalPlay;        // total played, in bytes
   __int64           m_iTotalWrite;       // total written, in bytes
   __int64           m_iLastWriteNotSilence; // last value written that wasnt silence
};
#endif USEDIRECTX


// AQTIMEMEM - Structure for sturing time and memory associated with a note
typedef struct {
   PCMem          pMem;       // memory
   LARGE_INTEGER  iTime;      // time stamp, from QueryPerformanceCounter()
} AQTIMEMEM, *PAQTIMEMEM;

class CAudioQueue {
public:
   ESCNEWDELETE;

   CAudioQueue (PCAudioThread pAT);
   ~CAudioQueue (void);

#ifdef USEDIRECTX
   BOOL AudioSum (LPDIRECTSOUND8 pDirectSound, PCMem pMemScratch, DWORD dwSamples, BOOL fUseCritSec);
#else
   BOOL AudioSum (short *psSamp, DWORD dwSamples, BOOL fUseCritSec);
#endif
   BOOL QueueAdd (PCMMLNode2 pNode, PCMem pMem, LARGE_INTEGER iTime, BOOL fUseCritSec);
   void MidiTimer (void);
   void Volume3D (fp fSpeakerSep, fp fBackAtten, fp fPowDistance, fp fScale);
   void AmbientDeleted (PCAmbient pAmbient);

   // for ambient info
   PCAmbient            m_pAmbient;    // if assocaited with ambient loop, not NULL
   DWORD                m_dwAmbientLoop; // ambient loop number

   // set if used for voice chat
   BOOL                 m_fVoiceChat;  // set to TRUE if the queue is specific to voice chat
   GUID                 m_gVoiceChat;  // object that's speaking, so can match up
   CVoiceDisguise       m_VoiceDisguise;  // used for voice chat


#ifdef USEDIRECTX
   CDXBuffer            m_bufDX;       // direct X buffer to use
   BOOL                 m_fPlaying;   // set to TRUE if has started playing
#endif

private:
   void ConsiderNextNode (BOOL fUseCritSec);
   BOOL VolumeGet (PCMMLNode2 pNode, BOOL fDoubleVolume = FALSE);
   int FadeLevel (DWORD dwSample);
   void AmbientExpand (BOOL fUseCritSec);
   BOOL VisemeQueue (DWORD dwSample, DWORD dwEnglishPhone, BOOL fUseCritSec);

   PCAudioThread        m_pAT;         // audio thread that uses
   DWORD                m_dwPlayMode;  // play mode. AQPM_XXX
   PCMidiFile           m_pMidi;       // current MIDI that playing
   PCM3DWave            m_pWave;       // current wave that playing. Used if m_dwPlayMode == AQPM_WAVE
   DWORD                m_dwSample;    // sample in m_pWave that up to
   DWORD                m_dwDuration;  // duration in samples, used if m_dwPlayMode == AQPM_SILENCE
   int                  m_aiVolumeOld[2]; // old volume. If there's a change need to blend volumes
   int                  m_aiVolume[2]; // volume for L&R channels, AQ_DEFVOLUME = normal volume
                                       // this can be negative, causing surround sound to work
   int                  m_aiVolumeObjScale[2]; // This is the amount to scale the volume for a specific object.
                                       // 256 = no change
   CPoint               m_pVolume3D;   // 3d sound. [0]..[2] are EW,NS,UD location, [3] = decibels
   BOOL                 m_fNoLipSync;  // if TRUE then don't send any lipsync information
#ifdef USEDIRECTX
   CPoint               m_pVolume3DRot;   // like m_pVolume3D, but includes rotation of head
#endif
   BOOL                 m_fVolume3D;   // set to TRUE if use 3D volume
   GUID                 m_gID;         // ID of the object creating the sound. Might be GUID_NULL
   CListFixed           m_lPCMMLNode;  // list of PCMMLNode2 for queued up events
   CListFixed           m_lAQTIMEMEM;  // list of memories to go along with PCMMLNode2
   CListFixed           m_lDELAYINFO;  // list of DELAYINFO for queued up events
   BOOL                 m_fWantToExit; // if volume gets to 0 and this is set then will exit.
                                       // used to end queue created by CAmbient
   DWORD                m_dwFadeInSamples; // number of samples to fade in over
   DWORD                m_dwFadeOutSamples;  // number of samples to fade out over
   DWORD                m_dwExpected;  // samples expected
   DWORD                m_dwStartFadingOut;  // time to start fading, -1 if dont know
   LARGE_INTEGER        m_iTimeStarted;   // time when started, so can keep track of lip sync
};
typedef CAudioQueue *PCAudioQueue;


class CAudioThread : public CProgressWaveSample, public CProgressWaveTTS  {
   friend class CAudioQueue;

public:
   ESCNEWDELETE;

   CAudioThread (void);
   ~CAudioThread (void);

   // called from the main thread
   BOOL Connect (HWND hWndNotify, DWORD dwMessage);
   BOOL Disconnect (void);
   BOOL StartDisconnect (void);

   // thread safe
   BOOL RequestAudio (PCMMLNode2 pNode, LARGE_INTEGER iTime, BOOL fMainQueue, BOOL fUseCritSec /* = TRUE*/);
   BOOL VoiceChat (PCMMLNode2 pNode, PCMem pMem, LARGE_INTEGER iTime, BOOL fUseCritSec /* = TRUE*/);
   void Mute (BOOL fMute);

   PCM3DWave WaveCreateAlways (PWSTR pszName, BOOL fDelWhenNoRef, PCMem pMemVoiceChat = NULL,
      PCVoiceDisguise pVoiceDisguise = NULL);
   void WaveDelete (PCM3DWave pWave);
   void WaveFreeOld (BOOL fUseCritSec);
   PCM3DWave WaveCreate (PWSTR pszName);
   PCM3DWave VoiceChatWaveCreate (PCMem pMem, PCVoiceDisguise pVoiceDisguise);
   void WaveRelease (PCM3DWave pWave);

   PCMidiFile MidiCreateAlways (PWSTR pszName, BOOL fDelWhenNoRef);
   PCMidiFile MidiCreate (PWSTR pszName);
   void MidiDelete (PCMidiFile pMidi);
   void MidiRelease (PCMidiFile pMidi);
   void MidiFreeOld (BOOL fUseCritSec);

   void PostMidiMessage (DWORD dwMessage, DWORD dwData);
   void VisemePost (PVISEMEMESSAGE pvm);
   void VisemeQueue (LARGE_INTEGER iTime, GUID *pgID, DWORD dwEnglishPhone, BOOL fUseCritSec = TRUE);

   PCM3DWave TTSSpeak (PCMMLNode2 pNode, LARGE_INTEGER iTime);
   void CanLoadTTS (void);
   void TTSSurroundSet (GUID *pgID, WORD dwNum);
   void TTSMuteTime (LARGE_INTEGER iTimeStart, LARGE_INTEGER iTimeEnd);

   void Vis360Changed (fp fLong, fp fLat);

   // called from the within the internet thread
   void ThreadProc (void);
   void ThreadProcTTS (void);
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   // can read but dont change
   BOOL              m_fWantToQuit;    // set to TRUE on a disconnect, so will stop rendering when close

   // from CProgressWaveSample
   virtual BOOL Update (PCM3DWave pWave, DWORD dwSampleValid, DWORD dwSampleExpect);

   // from CProgressWaveTTS
   virtual BOOL TTSWaveData (PCM3DWave pWave);
   virtual BOOL TTSSpeedVolume (fp *pafSpeed, fp *pafVolume);

   // public, and can change. DOn't bother to thread protect since
   // changing should be fairly autonomous, and changing wont cause a crash
   fp                m_fEarsSpeakerSep;    // how much speakers are separated beyond normal, 1.0 = normal
   fp                m_fEarsBackAtten;     // how much sound from the back is attenuated. 1.0 = none, 0.5 = half, etc. Human defaults to around 0.75
   fp                m_fEarsPowDistance;   // how much sound volume decreases over distance, default = 2.0 (square distance), but might be less
   fp                m_fEarsScale;        // how much to scale volume of ears for distance related, default 1.0

private:
#ifdef USEDIRECTX
   BOOL PlayAddBuffer (void);
#else
   BOOL PlayAddBuffer (PWAVEHDR pwh);
#endif

   PCAudioQueue NewQueue (BOOL fUseCritSec);
   void AmbientSoundsElapsed (double fDelta, BOOL fUseCritSec);
   void AmbientSounds (PCMMLNode2 pNode, BOOL fUseCritSec);
   void HandleMute (void);


   HANDLE            m_hThread;        // thread handle
   HANDLE            m_hSignalFromThread; // set when the thread has information
   HANDLE            m_hSignalToThread;   // set when the thread should shut down
   BOOL              m_fConnected;     // set to TRUE if connected, FALSE if not
   HWND              m_hWndNotify;     // window to notify if get incoming message
   DWORD             m_dwMessage;      // message to send to window if incoming message
   BOOL              m_fCanLoadTTS;    // set to TRUE one deleted list comes back and can load TTS

   HANDLE            m_hThreadTTS;     // TTS thread
   HANDLE            m_hSignalToThreadTTS;   // TTS signal to shut down (or whatever)
   LARGE_INTEGER     m_iTTSMuteTimeStart; // when TTS automute start, from QueryPerformanceCounter()
   LARGE_INTEGER     m_iTTSMuteTimeEnd;   // when TTS automute start, from QueryPerformanceCounter()
   LARGE_INTEGER     m_iTTSTime;          // time of currently-speaking TTS
   BOOL              m_fTTSTimeIgnore;    // if set then ignore TTS time since just speaking silence

   HWND              m_hWnd;           // window to play to
   BOOL              m_fTTSLowPriority; // raise and lower TTS priority
   PCM3DWave         m_pWaveTTS;       // so TTSWaveData() knows what wave using

   CListFixed        m_lVISEMEMESSAGE; // list of viseme messages to send out
   LARGE_INTEGER     m_iPerformanceFrequency;   // how fast performance counter is

#ifdef USEDIRECTX
   LPDIRECTSOUND8    m_pDirectSound;   // direct sound
   CDXBuffer         m_bufDrone;       // drone
#else
   HWAVEOUT          m_hWaveOut;       // waveout playback
   WAVEHDR           m_aPlayWaveHdr[WVPLAYBUF]; // headers for playback
   CMem              m_memPlay;        // memory for playback
#endif
   __int64           m_iTime;          // time in samples

   // under critical section
   CRITICAL_SECTION  m_CritSecWave;        // to protect some variables
   CListFixed        m_lPCM3DWave;     // list of waves. All have m_pUserData with PATWAVE
   CListFixed        m_lPCMidiFile;    // list of midi files. All have m_pUserData with PATWAVE

   // critical section
   CRITICAL_SECTION  m_CritSecQueue;   // to proect audio queue
   CListFixed        m_lPCAudioQueue;  // list of audio queues
   fp                m_fLong;          // longitude in radians that listening to
   fp                m_fLat;           // latitude in radians that listening to
   CBTree            m_tAmbientLoopVar;// tree of variable names with double values

   // critical section
   CRITICAL_SECTION  m_CritSecTTS;     // to access tts queue
   CListFixed        m_lTTSQUEUE;      // list of TTSQUEUE structures
   CListFixed        m_lTTSVoiceLoc;   // list of GUIDs for TTS voices, so can know positional location from L to R

   // MIDI mix
   PCMidiMix         m_pMidiMix;       // midi mixer
   PCMidiInstance    m_pMidiInstanceEscarpment; // MIDI instance used by escarpment

   CListFixed        m_lPCAmbient;     // list of ambient sounds

   BOOL              m_fWantToMute;    // set to TRUE if want to mute. doesnt need to be protected
   BOOL              m_fMuted;         // set to TRUE if muted
   DWORD             m_dwMidiVol;      // midi volume before muted
};
typedef CAudioThread *PCAudioThread;

extern PWSTR gpszRecentTTSFile;


/*************************************************************************************
CVisImage */



// MAP360INFO - Information about a map room that should appear on 360 view
typedef struct {
   fp             fDistance;        // distance from the viewer, in meters
   fp             fAngle;           // angle, in radians. 0 = N, PI/2 = E, etc.
   DWORD          dwRoomDist;       // room distance, 0 = player's room, 1 = 1 room away, etc.
   GUID           gRoom;            // room's GUID
   PWSTR          pszDescription;   // objects to be displayed
} MAP360INFO, *PMAP360INFO;

class CImageStore;
typedef CImageStore *PCImageStore;
class CMainWindow;
typedef CMainWindow *PCMainWindow;

// VILAYER - Layer drawn on the CVisImage
typedef struct {
   PCMMLNode2           pNode;      // node that describes it
   PCMem                pMem;       // memory containing the string version of the node
   DWORD                dwWidth;    // width
   DWORD                dwHeight;   // height
   BOOL                 fWaiting;   // set to TRUE if waiting for image to be drawn
   PCTransition         pTransition;   // transition effect

   // if drawing just text
   PCEscWindow          pTextWindow; // window to display text in, or NULL if not use
   LANGID               lid;         // language from the text

   // if drawn
   PCImageStore         pImage;     // image
   DWORD                dwStretch;  // stretch mode

   // stretch information
   RECT                 rStretchFrom;  // where it was stretched from
   RECT                 rStretchTo; // where it was stretched to
} VILAYER, *PVILAYER;

// OBJECTSLIDERS - Information about sliders
typedef struct {
   fp                   fValue;     // value, from -1.0 to 1.0 (already range limited)
   COLORREF             cColorLight;     // color to use with a light background
   COLORREF             cColorDark;     // color to use with a dark background
   COLORREF             cColorTip;  // color to use on the tooltip
   WCHAR                szLeft[32]; // left (-1.0) text
   WCHAR                szRight[32];   // right (1.0) text

   // history, so can show color deltas
   fp                   fValueAtTime;  // value at the time
   DWORD                dwTime;     // GetTickCount(), or 0 if never checked
} OBJECTSLIDERS, *POBJECTSLIDERS;

class CVisImage {
public:
   ESCNEWDELETE;

   CVisImage (void);
   ~CVisImage (void);

   BOOL Init (PCMainWindow pMain,
                      DWORD dwID, GUID *pgID, HWND hWnd,
                      BOOL fIconic, BOOL fCanChatTo, __int64 iTime);
   BOOL Update (PCMMLNode2 pNode, PWSTR pszMemNodeAsMML, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                        PCMMLNode2 pNodeHypnoEffect,
                      PWSTR pszName, PWSTR pszOther, PWSTR pszDescription, BOOL fForViseme, BOOL fCanChatTo,
                      __int64 iTime);
   void Viseme (DWORD dwViseme, DWORD dwTickCount);

   PCCircumrealityHotSpot HotSpotMouseIn (int iX, int iY);
   BOOL Link (PWSTR pszLink, PCEscPage pPage, GUID *pgObject);
   void AnimateAdvance (fp fDelta);

   void Paint (HWND hWnd, HDC hDC, RECT *prClip, BOOL fIncludeSliders,
      BOOL fDontPaintIfNothing, BOOL fTextUnderImages, 
      BOOL *pfWantedToShowDraingText, BOOL *pfPaintedImage);
   void InvalidateSliders (void);
   void SeeIfRendered (void);
   void WindowBackground (BOOL fForceRefresh);
   void UpgradeQuality (PCImageStore pisLowQuality);
   void ReRenderAll (void);
   void Vis360Changed (void);
   BOOL CanGetFocus (void);
   HWND WindowGet (void) {return m_hWnd;};
   HWND WindowForTextGet (void);
   void RectSet (RECT *prNew, BOOL fMoved);
   void RectGet (RECT *pr);
   PCMMLNode2 NodeGet (void);
   PCMMLNode2 MenuGet (void);
   PCMMLNode2 SlidersGet (void);
   PCMMLNode2 HypnoEffectGet (void);
   void SlidersGetInterp (PCListFixed pl);
   BOOL IsAnyLayer360 (void);
   PCImageStore LayerGetImage (void);
   BOOL LayerFindMatch (PWSTR pszMatch, BOOL fLast360);

   // public member variables
   DWORD                m_dwID;       // identifier. 0 = main window, others are for sub-windows
   GUID                 m_gID;         // object ID that this displays.
   BOOL                 m_fIconic;     // if TRUE it's iconic and cant click in specific spots
                                       // used by m_pTextWindow to trap button clicks
   CMem                 m_memName;     // name, string
   CMem                 m_memOther;    // other description, stirng
   CMem                 m_memDescription; // description, like "This looks like an expensive ring."
   BOOL                 m_fSpeaking;   // set to TRUE if image is currently producing sound,
                                       // so will draw speaker over it
   BOOL                 m_fCanChatTo;  // set to TRUE if can talk to this NPC
   BOOL                 m_fVisibleAsSmall;   // set to TRUE if this is currently visible as a small icon

   // look at but dont touch
   PCMainWindow         m_pMain;       // main window, will call back into for some info
   HWND                 m_hWnd;        // window where drawn
   PCEscPage            m_pPage;       // current page displayed, or NULL if none
   PCListFixed          m_plPCCircumrealityHotSpot; // list of hot spots
   CListFixed           m_lVILAYER;    // list of layer inforamtion
   __int64              m_iNodeMenuTime;  // so keep highest-time menu, in case come in out of order
   CListVariable        m_lOldLayers;  // list of names of old MML layers, so can update background image properly
   CListFixed           m_lOldLayersStretch;  // one element per m_lOldLayers. Store stretch information for the layer

private:
   void FuzzyPaint (HDC hDC, POINT pOffset);
   void DescriptionPaint (HDC hDC, RECT *prDescription);
   void SlidersPaint (HDC hDC, RECT *prSliders, int iSlidersLeft, int iSlidersRight);
   void FuzzyBubblePaint (HDC hDC, RECT *prFuzzy, int iBorder);
   void HotSpotsSet (PCMMLNode2 pNode, DWORD dwWidth, DWORD dwHeight);
   void HotSpotsClear(void);
   void Clear (BOOL fInvalidate);
   void EliminateDeadLayers (void);
   BOOL LayerClear (DWORD dwLayer);
   void LayeredClear (void);
   BOOL LayeredCalc (HDC hDC);
   void MasterMMLSet (PWSTR pszMML);

   void DistantObjectDescriptionPaint (HDC hDC, RECT *prDescription, PMAP360INFO pInfo);
   void DistantObjectFuzzyPaint (HDC hDC, POINT pOffset, PMAP360INFO pInfo);
   void DistantObjectsSize (HDC hDC, PMAP360INFO pInfo, POINT *pSize);
   void DistantObjectFuzzySize (HDC hDC, PMAP360INFO pInfo,
                                        POINT *pSize, RECT *prDescription);
   void DistantObjectFuzzyLocation (HDC hDC, PMAP360INFO pInfo, RECT *prFuzzy, RECT *prDescription);

   void SlidersSize (HDC hDC, POINT *pSize, int *piLeft, int *piRight);
   void DescriptionSize (HDC hDC, POINT *pSize);
   void FuzzySize (HDC hDC, POINT *pSize, RECT *prDescription, RECT *prSliders,
      int *piSliderLeft, int *piSliderRight);
   void FuzzyLocation (HDC hDC, RECT *prFuzzy, RECT *prDescription, RECT *prSliders,
                           int *piSliderLeft, int *piSliderRight);


   RECT                 m_rClient;     // location of where drawn in client rect of m_hWnd

   // viseme information
   CMem                 m_memMaster;   // master MML... in case use for lipsync
   BOOL                 m_fLipSync;    // set to TRUE if lip syncs
   DWORD                m_dwViseme;    // current viseme being drawn
   fp                   m_fBlinkWait;  // how many seconds to wait before blinking. If less than 0 then is the number of seconds that hold the blink

   // if still waiting for it to be drawn
   PCMMLNode2           m_pNodeMenu;   // node that described the menu, type <ContextMenu>
   PCMMLNode2           m_pNodeSliders;   // node that describes all the sliders, type <Sliders>
   PCMMLNode2           m_pNodeHypnoEffect;  // node that describes the hypnoeffect, type <HypnoEffect>
   CListFixed           m_lOBJECTSLIDERSOld; /// old sliders so can keep track of what has changed
   RECT                 m_rSliders;    // old location of the sliders
   GUID                 m_gIDWhenSliders; // GUID when sliders

   //HBITMAP              m_hBmp;       // bitmap

   // layered image
   PCImageStore         m_pisLayered;  // layered image
   HBITMAP              m_hBmpLayered; // layered bitmap
};
typedef CVisImage *PCVisImage;

void LinkReplace (PCMem pMem, PWSTR pszNew, DWORD dwStart, DWORD dwEnd);
BOOL ImageCacheAdd (PWSTR pszFile, PCImageStore pStore, DWORD dwQuality);
PCImageStore ImageCacheFindLowQuality (PWSTR pszFile);
void ImageCacheRelease (PCImageStore pImage, BOOL fForce);
void ImageCacheTimer (BOOL fForceUnCache);
BOOL ImageCacheExists (PWSTR pszFile, DWORD dwQuality, BOOL fTouch);

/*************************************************************************************
CSlidingWindow */

#define NUMTABS                     4        // number of tabs
#define NUMTABMONITORS              (NUMTABS*2)

#define TAB_EXPLORE                 0        // tab used for exploration
#define TAB_CHAT                    1        // tab used for chatting
#define TAB_COMBAT                  2        // tab used for combat
#define TAB_MISC                    3        // tab for miscellaneous


class CDisplayWindow;
typedef CDisplayWindow *PCDisplayWindow;
class CIconWindow;
typedef CIconWindow *PCIconWindow;

// TASKBARWINDOW - Information about windows displayed in taskbar
typedef struct {
   HWND           hWnd;       // of the window, where to get name from if pszName is NULL
   PWSTR          pszName;    // name string
   RECT           rLocText;   // location if the text
   // RECT           rLocMiniIcon;  // location of the mini icon
   BOOL           fIsVisible; // set to TRUE if the window is visible
   BOOL           fCantShowHide; // set to TRUE if CAN'T show/hide this
   PCDisplayWindow   pdw;     // if display window
   PCIconWindow      piw;     // if icon window
   DWORD          dwType;     // 0 for display window, 1 for icon window,
                              // 10 for verb window
                              // 11 for map window
                              // 12 for tasncript window
} TASKBARWINDOW, *PTASKBARWINDOW;


class CSlidingWindow {
public:
   ESCNEWDELETE;

   CSlidingWindow (void);
   ~CSlidingWindow (void);
   BOOL Init (HWND hWndParent, BOOL fOnTop, BOOL fLocked, PCMainWindow pMain);
   void Resize (void);
   void DefaultCoords (DWORD dwLocked, PRECT pr);
   void Locked (BOOL fLocked);
   BOOL PieChartAddSet (DWORD dwID, PWSTR psz, fp fValue, fp fDelta, COLORREF cr);
   void SlideDownTimed (DWORD dwTime);

   void Invalidate (void);
   void InvalidateProgress (void);
   void InvalidatePTT (void);
   void TaskBarSyncTimer (void);

   void CommandSetFocus (void);
   BOOL CommandIsVisible (void);
   void CommandLineShow (BOOL fShow);
   void CommandTextSet (PWSTR psz);

   void ToolbarShow (BOOL fShow);
   void TutorialCursor (DWORD dwItem, POINT *pp, RECT *pr);
   BOOL ToolbarIsVisible (void);
   void ToolbarArrange (void);

   void ChildWindowContextMenu (PTASKBARWINDOW ptbw, int iAction);

   // look, but dont touch
   HWND                 m_hWnd;        // parent window where drawn
   DWORD             m_dwTab;             // current tab that's shown

   // called for window procedure
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   CListFixed        m_lPCPieChart;       // list of pie chart controls

private:
   void SlideTimer (BOOL fForTimer);

   //void TabLocation (RECT *pr);
   void TabDraw (HDC hDC, PWSTR pszText, RECT *prTab, BOOL fOnTop, BOOL fHighlight);
   void TabLoc (DWORD dwIndex, RECT *prTab);
   void TabsDraw (HDC hDC, RECT *prHDC, int iHighlight);
   void TabPTTLoc (RECT *prMenu);
   void TabProgressLoc (RECT *prMenu);
   int TabOverItem (POINT p);
   void ProgressDraw (HDC hDC, RECT *pr);
   void VoiceChatScope (HDC hDC, RECT *pr);
   void PieChartLoc (DWORD dwNum, RECT *pr);

   void TaskBarLocation (RECT *pr);
   void TaskBarDraw (HDC hDC, RECT *prHDC, RECT *prInWindow, int iHighlight);
   void CircumRealityButtonDraw (HDC hDC, RECT *prHDC, RECT *prInWindow, int iHighlight);
   void TaskBarMenuLoc (RECT *prMenu);
   int TaskBarOverItem (POINT p, BOOL fToShowHide);
   void TaskBarSync (void);

   void CommandLoc (RECT *pr);
   void CommandLabelLoc (RECT *pr);

   void CSlidingWindow::CloseButtonLoc (RECT *pr);

   void ToolbarLoc (RECT *pr);
   DWORD ToolbarHeight (void);

   BOOL                 m_fOnTop;      // set to TRUE if on the top half of screen, FALSE if on bottom
   BOOL                 m_fLocked;     // set to TRUE if locked in open position
   HWND                 m_hWndParent;  // prent window
   PCMainWindow         m_pMain;       // main window, will call back into for some info

   // tab
   int               m_iTabHighlight;     // to highlight in TabDraw()

   // taskbar
   CListFixed        m_lTASKBARWINDOW;    // list of taskbar windows
   BOOL              m_fTaskBarTimer;     // set to TRUE if the taskbar sync timer is turned on
   int               m_iTaskBarHighlight; // to highlight in TaskBarDraw()
   HFONT             m_hFontSymbol; // symbol font
   HFONT             m_hFontBig;    // big font to use

   // command
   HWND              m_hWndCommand;    // command edit window

   // toolbar
   BOOL                 m_fToolbarVisible;   // set to TRUE if the toolbar is visible

   // timer
   BOOL                 m_fSlideTimer; // set to TRUE if the slide-into-place timer is on
   int                  m_iStayOpenTime;  // stay open for this many ms

   // close button
   CIconButton          m_CloseButton;     // for close
};
typedef CSlidingWindow *PCSlidingWindow;


/*************************************************************************************
CTickerTape */


class CTickerTape {
public:
   ESCNEWDELETE;

   CTickerTape (void);
   ~CTickerTape (void);
   BOOL Init (HWND hWndParent, BOOL fOnTop, int iFontSize, PCMainWindow pMain);
   void Resize (void);
   void DefaultCoords (DWORD dwLocked, PRECT pr);
   void SlideDownTimed (DWORD dwTime);
   BOOL StringAdd (PWSTR psz, COLORREF cColor);
   void Phrase (GUID *pgID, PWSTR pszSpeaker, PWSTR pszSpoken, BOOL fFinished);
   BOOL FontSet (DWORD dwSize);

   void Invalidate (void);

   // look, but dont touch
   HWND                 m_hWnd;        // parent window where drawn

   // called for window procedure
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
   void SlideTimer (BOOL fForTimer);
   void TickerTimer (void);
   BOOL CreateFontIfNecessary (BOOL fForce);


   void ToolbarLoc (RECT *pr);
   DWORD ToolbarHeight (void);

   BOOL                 m_fOnTop;      // set to TRUE if on the top half of screen, FALSE if on bottom
   HWND                 m_hWndParent;  // prent window
   PCMainWindow         m_pMain;       // main window, will call back into for some info

   HFONT                m_hFontSymbol; // symbol font
   HFONT                m_hFontBig;    // big font to use
   int                  m_iFontSize;   // font size to use
   int                  m_iFontPixels; // how high the font is in pixels, calculated by CreateFontIfNecessary()

   // ticker
   CListFixed           m_lTICKERINFO; // ticker info, TICKERINFO struct
   CListVariable        m_lTickerStrings; // string associated with each m_lTICKERINFO
   DWORD                m_dwLastTick;  // last tick, in GetTickCount()

   // timer
   BOOL                 m_fSlideTimer; // set to TRUE if the slide-into-place timer is on
   int                  m_iStayOpenTime;  // stay open for this many ms
};
typedef CTickerTape *PCTickerTape;


/*************************************************************************************
CSubtitle */
#define SUBTITLEFONTS         4     // number of subtitle fonts

// SUBTITLEPHRASE - Information for each phrase
typedef struct {
   GUID              gID;                 // ID of the speaker
   BOOL              fFinished;           // set to TRUE if finished
   int               iTimeLeft;           // number of milliseconds left before can hide itself
} SUBTITLEPHRASE, *PSUBTITLEPHRASE;

class CSubtitle {
public:
   ESCNEWDELETE;

   CSubtitle (void);
   ~CSubtitle (void);

   BOOL Init (HWND hWndParent);
   void End (void);
   BOOL FontSet (DWORD dwFontSize);
   DWORD FontGet (void);

   void Phrase (GUID *pgID, PWSTR pszSpeaker, PWSTR pszSpoken, BOOL fFinished);

   // called for window procedure
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   // look but dont touch
   HWND              m_hWnd;              // window of subtitle, if it's visible

private:
   void DeleteOldPhraseIfNecessary (void);
   void CalcSizeAndLoc (RECT *pr);
   void Refresh (RECT *pr);

   HWND              m_hWndParent;        // window to create off of
   DWORD             m_dwFontSize;        // font size. 0 (none) to SUBTITLEFONTS-1

   HFONT             m_ahFontBold[SUBTITLEFONTS];   // subtitle fonts. [0] is always NULL
   HFONT             m_ahFontItalic[SUBTITLEFONTS];   // subtitle fonts. [0] is always NULL
   CListFixed        m_lSUBTITLEPHRASE;   // phrase information
   CListVariable     m_lSpeaker;          // string of speaker, corresponding to each phrase
   CListVariable     m_lSpoken;           // spooken string, corresponding to each phrase
   DWORD             m_dwCorner;          // 0 = UL, 1 = UR, 2 = LR, 3 = LL
   DWORD             m_dwLastAutoMove;    // last time automoved

   HBRUSH            m_hbrBackground;     // background
};
typedef CSubtitle * PCSubtitle;


/*************************************************************************************
CMainWindow */

#define MINIICONSIZE                16       // size of small icon for menu
#define MENUICONSIZE                24       // size of large icon for menu

DEFINE_GUID(GUID_MegaFileCache, 
0x173558c7, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GUID_MegaImageCache, 
0x273558c7, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);
DEFINE_GUID(GUID_MegaUserCache, 
0x143558c7, 0x89ab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);

extern int giCPUSpeed;     // CPU speed
extern BOOL gfRenderCache; // using render cache
extern PWSTR gpszCircumrealityClient;
extern PWSTR gpszSettingsTempEnabled;
extern PWSTR gpszSettingsDisabled;
extern PWSTR gpszNarrator;

// VISMAIN.dwID values
#define VIS_ICONWINDOW        1     // drawing on one of the icon windows
#define VIS_DISPLAYWINDOW     2     // drawing on a single display window


class CIconWindow;
typedef CIconWindow *PCIconWindow;
class CIWGroup;
typedef CIWGroup *PCIWGroup;
class CDisplayWindow;
typedef CDisplayWindow *PCDisplayWindow;

class CMapWindow;
typedef CMapWindow *PCMapWindow;

DEFINE_GUID(CLSID_PlayerAction, 
0x3e29ddf9, 0xcd44, 0x4617, 0xae, 0x85, 0x2f, 0xb2, 0xfa, 0x91, 0xfb, 0xb8);

#define MAXTTSSIZE      45000000       // non-paying TTS can't be any larger than 30 meg
      // BUGFIX - Upped to 45 meg because my voice is larger
      // BUGFIX - Since restored size of small voices to 7 meg, shrink this
      // BUGFIX - Upped to 35 meg since making small voices larger

#define MINITTSSIZE     10000000       // if all voices smaller than this then miniTTS voices being used


#define WM_MAINWINDOWNOTIFYTRANSCRIPT     (WM_USER+140)     // lParam is a PCMMLNode2 that must be freed
#define WM_MAINWINDOWNOTIFYAUDIOSTART     (WM_USER+141)     // lParam is a PCMem that must be freed, containing guid
#define WM_MAINWINDOWNOTIFYAUDIOSTOP      (WM_USER+142)     // lParam is a PCMem that must be freed, containing guid
#define WM_MAINWINDOWNOTIFYTRANSUPDATE    (WM_USER+143)
#define WM_MAINWINDOWNOTIFYTRANSCRIPTEND  (WM_USER+144)     // lParam is a PCMMLNode2 that must be freed
#define WM_MAINWINDOWNOTIFYVISEMEMESSAGE  (WM_USER+145)     // lParam is PCMem that must be freed, containing VISEMEMESSAGE
#define WM_MAINWINDOWNOTIFYTTSLOADPROGRESS (WM_USER+146)    // lParam is a value from 0 to 100 to indicated TTS load progress. 0 => done

//#define MINGRAPHICSQUALITY                (-3)              // minimum graphics quality setting (for CPU speed)

// #define RESQUAL_RESOLUTIONMAX             8                 // cant have higher resolution than this (exclusive)
// #define RESQUAL_QUALITYMAX                5                 // cant have higher quality than this (exclusive)
               // BUGFIX - Was 4, but added new quliayt
// #define RESQUAL_DYNAMICMAX                6                 // values 0..5 ok
// #define RESQUAL_RESOLUTIONMAXIFNOTPAID    3                 // if not paid, can never have higher than this resolution (exclusive)
// #define RESQUAL_QUALITYMAXIFNOTPAID       2                 // if not paid, can never have higher than this quality (exclusive)
// #define RESQUAL_DYNAMICMAXIFNOTPAID       2                 // if not paid, can never have higher than this dynamic (exclusive)
#define RESQUAL_QUALITYMONOMAX                6                 // cant have higher quality than this (exclusive)
#define RESQUAL_QUALITYMONOMAXIFNOTPAID       2                 // if not paid, can never have higher than this quality (exclusive)
#define RESQUAL_RESOLUTIONBEYONDRECOMMEND    2                 // provide check-boxes for resolutions beyond the recommended
   // BUGFIX - Was only 1, but upped to 2 so players could try a higher variety of qualities

// TABWINDOW - Information about a specific tab
typedef struct {
   DWORD             dwType;           // window type. of TW_XXX
   DWORD             dwMonitor;        // monitor number that's stored on
   BOOL              fVisible;         // set to TRUE if the window is visible by default
   BOOL              fTitle;           // set to TRUE if the window has a title bar by default
   CPoint            pLoc;             // default location, [0]=left, [1]=right, [2]=top, [3]=bottom, all 0..1
} TABWINDOW, *PTABWINDOW;

#define TW_DISPLAYWINDOW         0x000          // used for a display window
#define TW_ICONWINDOW            0x001          // used for an icon window
   // NOTE: The bit set in the 2nd byte indicates that doesnt use the string
// #define TW_EDIT                  0x100         // for the edit window
#define TW_MAP                   0x101       // the map window
#define TW_TRANSCRIPT            0x102       // transcript window
// #define TW_VERB                  0x103        // verb window
#define TW_MENU                  0x105       // menu display


#define WS_EX_IFTITLE            0
#define WS_EX_IFNOTITLE          0  
#define WS_EX_ALWAYS             (WS_EX_TOOLWINDOW | WS_EX_STATICEDGE)  // BUGFIX - Static edge always
#define WS_IFTITLE               (WS_CAPTION | WS_SIZEBOX /* ??? | WS_EX_STATICEDGE*/) // BUGFIX - Edge only if have title
#define WS_IFTITLECLOSE          (WS_IFTITLE | WS_SYSMENU)
#define WS_ALWAYS                (WS_CHILD)


#define  NUMMAINFONT       3        // 3 main fonts
#define  NUMMAINFONTSIZED     3        // fonts of different sizes

#define  RECORDBUF         8        // number of record buffers

// transscript info
class CTransInfo {
public:
   ESCNEWDELETE;

   CTransInfo (void);
   ~CTransInfo (void);

   // properties
   GUID           m_gID;            // ID of the object that did the action
   DWORD          m_dwType;         // type. 0 for a normal string, 1 for MML, 2 for audio
   CMem           m_memObjectName;  // name of the object
   CMem           m_memString;      // string to display
   CMem           m_memLang;        // language
   CMem           m_memVCAudio;     // compressed voice chat audio, several combined
   PCMMLNode2     m_pVCNode;        // VC audio MMLNode

   DWORD          m_dwTimeStart;    // GetTickCount() when first data written
   DWORD          m_dwTimeLast;     // GetTickCount() when last data written
};
typedef CTransInfo *PCTransInfo;

// VCTHREADINFO
typedef struct {
   HANDLE            hThread;    // thread handle
   PCMainWindow      pMain;      // main window
   DWORD             dwThread;   // thread number
   HANDLE            hSignal;    // signal to the thread
} VCTHREADINFO, *PVCTHREADINFO;

#define NUMIMAGECACHE      2     // 2 image caches, 0 = low quality, 1 = high quality
#define IMAGELOADPRIORITY  2     // 2 image load priorities, 0 = high priority, 1 = low priority

void LinkExtractFromPage (PCEscPage pPage, PCListVariable plLinks, PCListFixed plLinksLANGID);
int CPUSpeedToQualityMono (int iCPUSpeed);
void ResolutionQualityToRenderSettingsInt (int iQualityMono, BOOL fHasPaid,
                                                      int *piDefaultResolution, DWORD *pdwShadowFlags,
                                    DWORD *pdwServerSpeed, int *piDefaultTextureDetail, BOOL *pfLipSync,
                                    DWORD *pdwMovementTransition);
int FontScaleByScreenSize (int iFontSize);
void EscWindowFontScaleByScreenSize (PCEscWindow pWindow);

#define BACKGROUND_TEXT          0  // background behind text
#define BACKGROUND_IMAGES        1  // background behind images
#define BACKGROUND_NUM           (BACKGROUND_IMAGES+1)  // number of images

#define WANTDARK_NORMAL          0  // normal darkness
#define WANTDARK_DARK            1  // darker version of background
#define WANTDARK_DARKEXTRA       2  // extra dark version of background

class CHypnoManager;
typedef CHypnoManager *PCHypnoManager;

extern BOOL gfChildLocIgnoreSave;       // if TRUE then ignore changes to the windows size
                                 // as far as the master settings
class CMainWindow {
   friend class CVisImage;
   friend class CIconWindow;
   friend class CDisplayWindow;
   friend class CMapWindow;
   friend class CSlidingWindow;
   friend class CTickerTape;

public:
   ESCNEWDELETE;

   CMainWindow (void);
   ~CMainWindow (void);
   BOOL Init (PWSTR pszFile, PWSTR pszUserName, PWSTR pszPassword,
                        BOOL fNewAccount, BOOL fRemote, DWORD dwIP, DWORD dwPort, BOOL fQuickLogon,
                        PCMem pMemJPEGBack);

   BOOL MessageParse (PCMMLNode2 pNode, __int64 iTime);
   void MainClear (void);

   HBITMAP BackgroundStretchCalc (DWORD dwTextOrImages, RECT *prScreen, DWORD dwWantDark, HWND hWndMain, DWORD dwScale,
                                            RECT *prStretch, RECT *prOrig);
   void BackgroundStretch (DWORD dwTextOrImages, BOOL fTransparent, DWORD dwWantDark, RECT *prClient, HWND hWndClient,
                                     HWND hWndMain, DWORD dwScale, HDC hDC, POINT *pDCOffset);
   void BackgroundStretchViaBitmap (DWORD dwTextOrImages, BOOL fTransparent, DWORD dwWantDark, RECT *prClient, HWND hWndClient,
                                     HWND hWndMain, HDC hDC);
   void BackgroundUpdate (DWORD dwTextOrImages, PCImageStore pis, BOOL fRequireForceBackground);
   void BackgroundUpdate (DWORD dwTextOrImages);
   BOOL BackgroundUpdate (DWORD dwTextOrImages, PCWSTR psz);

   BOOL VisImageDelete (DWORD dwIndex);
   DWORD VisImageFindPtr (PCVisImage pvi);
   BOOL SetMainWindow (PCVisImage pImage);
   BOOL SetMainWindow (PCMMLNode2 pNode, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                                 PCMMLNode2 pNodeHypnoEffect, GUID *pgID,
                                 PWSTR pszName, PWSTR pszOther, PWSTR pszDescription, BOOL fCanChatTo,
                                 __int64 iTime);

   PCMMLNode2 UserLoad (BOOL fAccount, PWSTR pszName);
   BOOL UserSave (BOOL fAccount, PWSTR pszName, PCMMLNode2 pNode);
   BOOL MenuLink (PWSTR pszLink);
   BOOL MenuLinkDefault (void);
   BOOL VerbTooltipUpdate (DWORD dwMonitor = 0, BOOL fShow = FALSE, int iX = 0, int iY = 0);
   void HotSpotTooltipUpdate (PCVisImage pvi = NULL, DWORD dwMonitor = 0, BOOL fShow = FALSE, int iX = 0, int iY = 0);
   BOOL VerbClickOnObject (GUID *pgID);
   void VerbSelect (PWSTR pszShow, PWSTR pszDo, LANGID lid,
                              DWORD dwVerbSelected = (DWORD)-1, BOOL fVerbSelectedIcon = FALSE);
   void VerbDeselect (void);
   void PacketSendError (int iError, PCWSTR psz);
   void DirectSoundError (void);

   void CommandSetFocus (BOOL fForce);
   BOOL SpeakSetFocus (BOOL fForce);
   PCIconWindow FindChatWindow (void);
   void HypnoEffect (PWSTR psz, fp fDuration, fp fPriority);
   void HypnoEffect (PCMMLNode2 pNodeHypnoEffect);
   void HypnoEffectTimer (fp fTime);

#if 0 // not used
   void InvalidateRectSpecial (HWND hWnd);
#endif // 0

   //void ChildLocSave (HWND hChild, PCMMLNode2 pNode, BOOL fHidden);
   //BOOL ChildLocGet (PCMMLNode2 pNode, RECT *prLoc, BOOL *pfHidden);
   BOOL ChildLocSave (PTABWINDOW ptw, PWSTR psz);
   BOOL ChildLocSave (HWND hChild, DWORD dwType, PWSTR psz, BOOL *pfHidden);
   BOOL ChildLocGet (DWORD dwType, PWSTR psz, DWORD *pdwMonitor, RECT *prLoc, BOOL *pfHidden, BOOL *pfTitle);
   void ChildShowMove (HWND hChild, DWORD dwType, PWSTR psz);
   BOOL ChildHasTitle (HWND hChild);
   void ChildShowMoveAll (void);
   void ChildTitleShowHideAll (void);
   void ChildTitleShowHide (BOOL fShow, HWND hChild, DWORD dwType, PWSTR psz);
   BOOL ChildWouldOverlap (HWND hChild, HWND hChildLoc, RECT *prLoc, DWORD dwMonitorLoc);
   void ChildShowTitleIfOverlap (HWND hChild, RECT *prLoc, DWORD dwMonitorLoc, BOOL fHidden, BOOL *pfTitle);
   void ChildShowWindow (HWND hChild, DWORD dwType, PWSTR psz, int nShow,
      BOOL *pfHidden = NULL, BOOL fNoChangeTitle = FALSE);
   BOOL ChildShowToggle (DWORD dwType, PWSTR psz, int iToggle);
   BOOL ChildMoveMonitor (DWORD dwType, PWSTR psz);
   void RecordCallback (HWAVEIN hwi,UINT uMsg, LONG_PTR dwParam1, LONG_PTR dwParam2);
   BOOL VoiceChatStop (void);
   BOOL VoiceChatStart (void);
   int VoiceChatThread (DWORD dwThread);
   int ImageLoadThread (DWORD dwThread);
   void ImageLoadThreadAdd (PCWSTR psz, DWORD dwQuality, DWORD dwPriority);
   BOOL ContextMenuDisplay (HWND hWnd, PCVisImage pView);
   BOOL IsWindowObscured (HWND hWnd);
   void FontCreateIfNecessary (HDC hDC);
   void FlashChildWindow (RECT *prScreen);
   void ResolutionQualityToRenderSettings (BOOL fPowerSaver, int iQualityMono, BOOL fTestIfPaid,
      int *piDefaultResolution, DWORD *pdwShadowFlags,
                                    DWORD *pdwServerSpeed, int *piDefaultTextureDetail, BOOL *pfLipSync,
                                    int *piDefaultResolutionLow, DWORD *pdwShadowFlagsLow, BOOL *pfLipSyncLow,
                                    DWORD *pdwMovementTransition);
   int QualityMonoGet (int *piResolution = NULL);

   void VisImageReRenderAll (void);
   void VisImageTimer (PCVisImage pvi, DWORD dwTime);
   PCVisImage FindMainVisImage (void);

   PWSTR TranscriptMenuAndDesc (void);
   void TranscriptString (DWORD dwType, PWSTR pszActor, GUID *pgActor, PWSTR pszLanguage, PWSTR pszSpeak,
      PCMMLNode2 pVCNode = NULL, PCMem pVCAudio = NULL);
   void TranscriptUpdate (void);
   void HotSpotDisable (void);
   void HotSpotEnable (void);
   BOOL SendTextCommand (LANGID lid, PWSTR pszDo, PWSTR pszShow,
                                   GUID *pgObject, GUID *pgClick,
                                   BOOL fShowCommand, BOOL fShowTranscript /*= TRUE*/, BOOL fAutomute);
   void CommandClean (PWSTR psz, GUID *pgObject, GUID *pgClick,
                                BOOL fDo, PCMem pMem);
   void TextVsSpeechSet (int iTextVsSpeech, BOOL fActNow);
   void LightBackgroundSet (BOOL fLight, BOOL fUpdateImage);

   LRESULT TrapWM_MOVING (HWND hWnd, LPARAM lParam, BOOL fSizing);

   BOOL InfoForServer (PWSTR pszName, PWSTR pszValue, fp fValue);
   PCDisplayWindow FindMainDisplayWindow (void);
   void SendDownloadRequests (void);
   void ReceivedDownloadRequest (PWSTR pszString, PCMem pMem);
   void ReceivedTTSCache (PVOID pMem, size_t dwSize);
   BOOL OfferWaveToServer (PCMMLNode2 pNode, PCM3DWave pWave, DWORD dwQuality);
   BOOL SendWaveToServer (PWSTR pszString, DWORD dwQuality);
   PCM3DWave GetTTSCacheWave (PWSTR pszString);

   // second monitor
   void SecondCreateDestroy (BOOL fCreate, BOOL fMoveAlso);
   DWORD ChildOnMonitor (HWND hWnd);

   // megafiles...
   CMegaFile         m_mfFiles;     // megafile from the server
   CMegaFile         m_amfImages[NUMIMAGECACHE];    // image file cache'
   CMegaFile         m_mfUser;      // user data stored here. only storing TTS now

   // semiprivate
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT WndProcSecond (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT WndProcFlash (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#ifndef UNIFIEDTRANSCRIPT
   LRESULT WndProcMenu (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

   LRESULT WndProcVerb (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT WndProcTranscript (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   PCInternetThread  m_pIT;         // talk to the internet
   PCRenderThread    m_pRT;         // render thread
   PCAudioThread     m_pAT;         // audio thread
   PCHypnoManager    m_pHM;         // hypnomanager thread
#ifdef UNIFIEDTRANSCRIPT
   void MenuMML (PCMem pMem, DWORD dwColumns);
#endif

   // member viaraibles
   HWND              m_hWndPrimary;        // window handle for the main window
   HWND              m_hWndSecond;        // second window

   // can look at but dont change
   BOOL              m_fMessageDiabled;   // disable messages for about 2 seconds after action - slow server
   BOOL              m_fMenuExclusive; // set to TRUE if it's an exclusive menu
   fp                m_fMenuTimeOut;   // number of seconds before time out, or 0 if none
   fp                m_fMenuTimeOutOrig;  // original timeout
   CListFixed        m_lPCIconWindow;  // list of icon windows
   BOOL              m_fLightBackground;  // if TRUE, will use black text on light backgroud, FALSE white text on dark background
   PCEscPage         m_pPageTranscript;   // transcript page

   // verb
   DWORD             m_dwVerbSelected;    // which verb has the click mode (-1 if none)
   BOOL              m_fVerbSelectedIcon; // if TRUE the verb is selected in the icon
   CMem              m_memVerbShow;       // show string for verb. If empty string then nothing to show
   CMem              m_memVerbDo;         // do string for verb
   LANGID            m_lidVerb;           // language ID for verb
   LANGID            m_lidCommandLast;    // last language used

   GUID              m_gMainLast;         // last object displayed in main

   __int64           m_iTimeLastCommand;  // last time a command was run

   // transcript
   CRITICAL_SECTION  m_crTransInfo;       // critial section surrounding m_lPCTransInfo
   CListFixed        m_lPCTransInfo;      // transcript information
   int               m_iSpeakSpeed;       // current speaking speed. 0 = norm, 1+ = faster, -1 lower = slower
   int               m_aiSpeakSpeed[NUMTABS]; // speaking speed for each of the tabs
   int               m_iTextVsSpeech;     // 0 for text only, 1 for text + speech, 2 for mostly speech
   BOOL              m_fMuteAll;          // mute all for the current tab
   BOOL              m_afMuteAll[NUMTABS];   // mute for a specific tab
   DWORD             m_dwTransShow;       // current transcript showing
   DWORD             m_adwTransShow[NUMTABS];  // transcript show bits. 1 for graphics options, 2 for transcript,
                                          // 4 for description, 8 for commands
   int               m_iTransSize;        // font size. 0 = norm, 1+ is larger, -1 lower is smaller
   CListFixed        m_lSpeakBlacklist;   // list of GUIDs that ae blacklisted. Only access when m_csSpeakBlacklist is entered
   CRITICAL_SECTION  m_crSpeakBlacklist;  // critical section to get to blacklist
   WCHAR             m_szRecordDir[128];       // record directory, for easy-to-record tool
   HWND              m_hWndTranscript; // transcript window
   CMem              m_memTutorial;       // appended to the transcript for the tutorial

   // flash window
   HWND              m_hWndFlashWindow;      // to flash other windows for tutorial
   DWORD             m_dwFlashWindowTicks;   // number of ticks to flash

   BOOL              m_fServerNotFoundErrorShowing;   // set to TRUE if server-not-found error is showing

#ifdef UNIFIEDTRANSCRIPT
   BOOL              m_fCombinedSpeak;       // set to TRUE if the combined edit field means speak, FALSE for action
   POINT             m_pTransSelCombined;    // selection start/end
   WCHAR             m_szTransCombined[256]; // combined string
   BOOL              m_fTransCommandVisible; // set to TRUE if the transcript command is visible
   // BOOL              m_fTransSpeakVisible;   // set to TRUE if the transcript speak is visible
   //WCHAR             m_szTransCommand[256];  // command text
   //WCHAR             m_szTransSpeak[256];    // speak text
   //POINT             m_pTransSelCommand;     // selection start/end
   //POINT             m_pTransSelSpeak;       // selection start/end
   //BOOL              m_fTransFocusCommand;   // set to TRUE if the focus should be on the command, FALSE if chat
#endif

   // fonts
   int               m_iFontSizeLast;     // last font size that actually initialied to
   HFONT             m_ahFont[NUMMAINFONT];         // fonts for display. [0] = normal, [1] = bold+large, [2] = small+italic
   DWORD             m_adwFontPixelHeight[NUMMAINFONT];   // fills in the height of the font, in pixels
   HFONT             m_ahFontSized[NUMMAINFONTSIZED]; // sized fonts, from largest to smallest
   DWORD             m_adwFontSizedPixelHeight[NUMMAINFONTSIZED]; // sized font, from alrgest to smallest

   BOOL              m_fAllSmall;         // set to TRUE if all the TTS voices are small

   // chat verb
   PCResVerb         m_pResVerbChat;      // verbs used for the chat window
   PCResVerb         m_pResVerbChatSent;  // verbs sent by the apps

   // background image
   CImage            m_aImageBackground[BACKGROUND_NUM];   // background image
   CMem              m_amemImageBackgroundName[BACKGROUND_NUM];  // background image name, so doesn't reload

   // user and password
   WCHAR             m_szUserName[64];    // user name
   WCHAR             m_szPassword[64];    // user password
   BOOL              m_fNewAccount;       // TRUE if want to create new user account


   HCURSOR           m_hCursorZoom; // zoom cursor
   HCURSOR           m_hCursorMenu; // menu cursor

   // 360 degree info
   fp                m_f360Long;    // longitude looking, radiuans, 0=n, PI/2=e, etc.
   fp                m_f360Lat;     // latitude looking, radians, 0=level, PI/2=up, etc.
   fp                m_f360FOV;     // field of view, radians
   fp                m_f360RotSpeed;   // rotation speed, in radians
   fp                m_fCurvature;  // visual curvature to use
   TEXTUREPOINT      m_tpFOVMinMax; // field of view minimum and maximum

   COLORREF          m_cBackground; // background color
   COLORREF          m_cBackgroundNoChange;  // background color that doesn't change
   COLORREF          m_cBackgroundNoTabNoChange;  // background color in tab area
   COLORREF          m_cBackgroundMenuNoChange;   // main menu background
   COLORREF          m_cText;       // color of text
   COLORREF          m_cTextNoChange;  // color of text in areas that don't change
   COLORREF          m_cTextDim;    // dimly colored text
   COLORREF          m_cTextDimNoChange;  // dim text in areas that don't change
   COLORREF          m_cTextHighlight; // color to highlight text
   COLORREF          m_cTextHighlightNoChange;  // highlight colors in areas that dont change
   COLORREF          m_cVerbDim;    // dim verb color
   COLORREF          m_cVerbDimNoChange;  // verb color in area that doesn't change

   // files not to donwload
   CListVariable     m_lDontDownloadOrig;  // list of files that shouldnt be downloaded, original path
   CListVariable     m_lDontDownloadFile;  // list of files that shouldnt be downloaded, just the files
   CListFixed        m_lDontDownloadDate;   // list of date information

   // user preferences
   int               m_iResolution;    // 0 = default, - numbers less, + more
   int               m_iResolutionLow; // resolution for low-quality quick render
   int               m_iTextureDetail; // 0 = default, -1 = low, 1 = high
   BOOL              m_fLipSync;       // set to TRUE if lip sync should happen
   BOOL              m_fLipSyncLow;    // for low-quality quick render
   DWORD             m_dwShadowsFlags; // flags to pass into shadows more, SF_XXX from CRenderTraditional
   DWORD             m_dwShadowsFlagsLow; // shadows for low-quality quick render
   DWORD             m_dwServerSpeed;  // 0..4. Value sent to server so server knows how fast PC is
   DWORD             m_dwMovementTransition; // 0 for none, 1 for moderate, 2 for best
   DWORD             m_dwArtStyle;     // artisitc style, 0..4
   int               m_iPreferredQualityMono;      // pass into CPUSpeedToQualityMono
   BOOL              m_fPowerSaver;    // if TRUE, power-saving mode
   LANGID            m_langID;         // preferred language ID
   BOOL              m_fSubtitleSpeech;   // if subtitles shown for for speech
   BOOL              m_fSubtitleText;  // if subtitles shown for text
   BOOL              m_fTTSAutomute;   // if TRUE, then automatically stop speaking when do action
   int               m_iTTSQuality;    // TTS quality. Passed into TTS functions
   BOOL              m_fDisablePCM;    // for TTS. Passed into TTS functions
   DWORD             m_dwSubtitleSize; // subtitle size, 1 to 3

   // upload image info
   DWORD             m_dwUploadImageLimitsNum;   // number of images that can load in
   DWORD             m_dwUploadImageLimitsMaxWidth; // maximum width
   DWORD             m_dwUploadImageLimitsMaxHeight;   // maximum height

   // voice chat
   BOOL              m_fTempDisablePTT;      // temporarily disable PTT
   BOOL              m_fVCHavePreviouslySent;   // set to TRUE if vC has previously sent during this recording
   CResVoiceChatInfo m_VoiceChatInfo;        // info sent on voice chat
   CVoiceDisguise    m_VoiceDisguise;        // voice disguise to use for player, and info about completed wizard
   CRITICAL_SECTION  m_csVCStopped;     // critical section arond stopped.. to protect the following
   CM3DWave          m_VCWave;               // wave for voice chat
   CListFixed        m_lVCEnergy;         // list of VC energy, initialized to fp
   HWAVEIN           m_hVCWaveIn;         // recording from
   WAVEHDR           m_aVCWaveHdr[RECORDBUF]; // wave headers
   DWORD             m_dwVCWaveOut;     // number of wave headers out and recording
   DWORD             m_dwVCTossOut;    // number of bytes to toss out because wave device gave non-word aligned data
   DWORD             m_dwVCTimeRecording; // number of millseconds that has been recording so far
   BOOL              m_fVCStopped;      // set to TRUE when stopped and exiting
   BOOL              m_fVCRecording;    // set to TRUE if recording
   CMem              m_memVCWave;      // to buffer

   // image load thread
   VCTHREADINFO      m_aImageLoadThread[NUMIMAGECACHE];  // threads for loading in images in background
   BOOL              m_fImageLoadThreadToShutDown;  // set to TRUE when the thread is to shut down
   CRITICAL_SECTION  m_csImageLoadThread;     // critical section for the VCthread
   CListVariable     m_alImageLoadThreadFile[NUMIMAGECACHE][IMAGELOADPRIORITY];   // file to load
                                                // [0..NUMIMAGECACHE-1][0 = high pri, 1 = low pri]
   CListVariable     m_lILTDOWNLOADQUEUE;    // list if file-requests sent to the server
   CListVariable     m_lCIRCUMREALITYPACKETCLIENTCACHE;  // list of memory to decompress as waves
   CListVariable     m_lCIRCUMREALITYPACKETCLIENTCACHETTS;  // list of TTS that was sent back
   CListFixed        m_lILTWAVEOUTCACHE;     // list, with waves rememberd
   BOOL              m_fCanAskForDownloads;     // so don't ask for downloads when shutdown

   // voice chat thread
   CRITICAL_SECTION  m_csVCThread;     // critical section for the VCthread
   VCTHREADINFO      m_aVCTHREADINFO[MAXRAYTHREAD];
   BOOL              m_fVCThreadToShutDown;  // set to TRUE when the thread is to shut down
   DWORD             m_dwVCTicketAvail; // if thread pulls a ticket, gets this one
   DWORD             m_dwVCTicketPlay; // ticket that's allowed to play
   CListFixed        m_lVCPCM3DWave;   // list of waves to process
   CListFixed        m_lVCPCMMLNode2;  // list of nodes to send to server along with the wave

   // sliding windows... look but dont touch
   PCSlidingWindow   m_pSlideTop;   // top sliding window
   PCSlidingWindow   m_pSlideBottom;   // bottom sliding window
   BOOL              m_fSlideLocked;   // set to TRUE if the sliding windows are locked into position
   BOOL              m_afSlideLocked[NUMTABS];  // based on the current tab
   PCTickerTape      m_pTickerTape; // ticker tape

   BOOL              m_fMinimizeIfSwitch; // if switch away from window then minimize

   // verb info
   PCResVerb         m_pResVerb;     // verb currently being used
   PCResVerb         m_pResVerbSent; // last verb-list sent (for reset purposes)

   CListFixed        m_alTABWINDOW[NUMTABMONITORS];   // list of TABWINDOW structures
   CListVariable     m_alTabWindowString[NUMTABMONITORS]; // list of strings associated with each TABWINDOW
                                          // in m_alTABWINDOW
   // CSubtitle         m_Subtitle;          // subtitle object

   // background image
   HBITMAP           m_ahbmpJPEGBack[BACKGROUND_NUM];      // background image
   HBITMAP           m_ahbmpJPEGBackDark[BACKGROUND_NUM];  // dark verison used for "drawing..."
   HBITMAP           m_ahbmpJPEGBackDarkExtra[BACKGROUND_NUM];   // extra dark verion for transcript window
   POINT             m_apJPEGBack[BACKGROUND_NUM];         // background image size
   COLORREF          m_crJPEGBackAll;        // average color of the top of the background image
   COLORREF          m_crJPEGBackTop;        // average color of the top of the background image top, used for tabs
   COLORREF          m_crJPEGBackDarkAll;    // dark version of backgroun
   HBRUSH            m_hbrJPEGBackAll;       // JPEG backrgound
   HBRUSH            m_hbrJPEGBackTop;    // JPEG backrgound, for top
   HBRUSH            m_hbrJPEGBackDarkAll;       // JPEG backrgound

   // random actions
   BOOL              m_fSettingsControl;  // set to TRUE if control key held down when go into settings
   BOOL              m_fRandomActions;    // set to TRUE if random actions are turned on
   fp                m_fRandomTime;       // time, in seconds, for random actions

   // connection
   BOOL              m_fConnectRemote; // TRUE if connection is remote, FALSE if is local

   // mini-timers for CVisImage
   CListFixed        m_lVISIMAGETIMER;    // list of timers for visimage display

private:
   void FillUpFiles (void);
   BOOL EnumPreinstalledTTS (void);

   BOOL RoomCenter (GUID *pgRoom);

   BOOL RandomActionTab (void);
   BOOL RandomActionClickOnIcon (void);
   BOOL RandomActionResVerb (PCMem pMemAction, GUID *pgObject, LANGID *pLangID);
   BOOL RandomActionTranscript (PCMem pMemAction, GUID *pgObject, LANGID *pLangID);
   BOOL RandomActionVisImage (PCMem pMemAction, GUID *pgObject, LANGID *pLangID);

   PTABWINDOW ChildLocTABWINDOW (DWORD dwType, PWSTR psz);
   HWND ChildToHWND (DWORD dwType, PWSTR psz);
   BOOL ChildLocSaveAll (void);
   BOOL ChildLocLoadAll (void);
   void ClientRectGet (DWORD dwMonitor, RECT *pr);

   BOOL TabSwitch (DWORD dwTab, BOOL fUserClick = TRUE);

   BOOL DialogRenderQuality (PCEscWindow pWindow);
   BOOL DialogUploadImage (PCEscWindow pWindow);
   BOOL DialogLayout (PCEscWindow pWindow);
   BOOL DialogSpeech (PCEscWindow pWindow);
   BOOL DialogSettings (DWORD dwPage);

   PWSTR ContextMenuDisplay (HWND hWnd, PCListVariable plMenuPre, PCListVariable plMenuShow,
                                       PCListVariable plMenuExtraText, PCListVariable plMenuDo);
   void DestroyAllChildWindows (DWORD dwType = 0xffff);
   void CloseNicely (void);
   
   void VisImageClearAll (BOOL fInvalidate);
   void VisImageSeeIfRendered (PCImageStore pis = NULL, DWORD dwQuality = 0);
   DWORD VisImageFind (DWORD dwID);
   PCVisImage VisImageNew (PCMMLNode2 pNode, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                                    PCMMLNode2 pNodeHypnoEffect, DWORD dwID, HWND hWnd,
                                     GUID *pg, BOOL fIconic, PWSTR pszName, PWSTR pszOther, PWSTR pszDescription,
                                     RECT *prClient, BOOL fCanChatTo, __int64 iTime);
   PCVisImage VisImageUpdate (PCVisImage pvi, PCMMLNode2 pNode, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                                        PCMMLNode2 pNodeHypnoEffect,
                                     GUID *pg, PWSTR pszName, PWSTR pszOther, PWSTR pszDescription, BOOL fCanChatTo,
                                     __int64 iTime);
   void VisImagePaintAll (HWND hWnd, HDC hDC, RECT *prClip);
   void Vis360Changed (void);
   PCVisImage VisImage (DWORD dwID);

   BOOL ObjectDisplay (PCMMLNode2 pNode, PCIWGroup pGroup, PCDisplayWindow pDispWin,
      BOOL fCanSetMainView, BOOL fCanDelete /*= FALSE*/, PWSTR pszID /*= NULL*/, BOOL fCanChatTo,
      __int64 iTime);
   void MenuReDraw (void);
   void VerbButtonsArrange (void);
   void VerbWindowShow (void);

   void ImageLoadThreadFree (void);

   void VoiceChatThreadFree (void);
   void VoiceChatWaveSnippet (PCM3DWave pWave);
   DWORD VoiceChatHowManyProcessors (void);

   void SimulateMOUSEMOVE (void);

   BOOL TranscriptDescription (PCMem pMem);
   BOOL TranscriptMenu (PCMem pMem, DWORD dwColumns);
   void TranscriptBackground (BOOL fForceRefresh);

   // map
   PCMapWindow       m_pMapWindow;  // map window

   //HWND              m_hWndVerb;    // verb window
   // BOOL              m_fVerbHiddenByUser; // set to TRUE if the verb is hidden by the user
   BOOL              m_fVerbHiddenByServer;  // set to TRUE if the verb is hidden by the server
   PCToolTip         m_pVerbToolTip;   // verb tooltip
   PCToolTip         m_pHotSpotToolTip;   // tooltip for hotspot

   HFONT             m_hFont;       // font to use
   HFONT             m_hFontBig;    // big font to use
   CIRCUMREALITYPACKETINFO     m_MPI;         // last information from the packet, used for progress
   __int64           m_iBytesShow;  // number of bytes that were received when started to show UI
   double            m_fRenderProgress;   // progress bar for rendering
   fp                m_fTTSLoadProgress;  // progress bar for TTS loading
   BOOL              m_fShowingDownload;  // set to TRUE if showing downloading...
   DWORD             m_dwShowProgressTicks;  // number of times that show has been true

   HBRUSH            m_hbrBackground;  // brush created with background color
   PCImageStore      m_pisBackgroundCur;  // so dont repeatedly reload

   HCURSOR           m_hCursorEye;  // eye cursor
   HCURSOR           m_hCursorMouth;   // mouth cursor
   HCURSOR           m_hCursorWalk;
   HCURSOR           m_hCursorWalkDont;
   HCURSOR           m_hCursorKey;
   HCURSOR           m_hCursorDoor;
   HCURSOR           m_hCursorHand; // hand cursor
   HCURSOR           m_hCursorTalk;
   HCURSOR           m_hCursorRotRight, m_hCursorRotLeft, m_hCursorRotUp, m_hCursorRotDown;
   HCURSOR           m_hCursor360Scroll;  // cursor for 360 scroll
   HCURSOR           m_hCursor360ScrollOn;  // cursor for 360 scroll
   HCURSOR           m_hCursorNo, m_hCursorNoMenu;
   CListFixed        m_lPCVisImage;   // list of visible images
   CListFixed        m_lPCDisplayWindow;  // list of display windows

   // hypno effect
   CMem              m_memHypnoEffectInfinite;  // hypno effect to use if no temporary ones exist
   CListFixed        m_lHYPNOEFFECTINFO;  // list of hypno effects stored away
   CListVariable     m_lHypnoEffectName;  // list of hypno effect names, one per m_lHYPNOEFFECTINFO

   // menu
#ifndef UNIFIEDTRANSCRIPT
   HWND              m_hWndMenu;    // menu window
   DWORD             m_fMenuPosSet;   // TRUE if the menu position has been set
   PCEscWindow       m_pMenuWindow; // esc window in the menu
#endif
   CListVariable     m_lMenuShow;   // list of strings that shown
   CListVariable     m_lMenuExtraText; // extra text to be displayed
   CListVariable     m_lMenuDo;     // list of strings to do
   LANGID            m_lidMenu;     // language ID for the menu (general one)
   LANGID            m_lidMenuContext; // language ID for the menu (context menu)
   DWORD             m_dwMenuDefault;  // default choice for the menu, 0-indexed
   PCVisImage        m_pMenuContext;   // vis-image being used for the context menu

   // transcript
   PCEscWindow       m_pTranscriptWindow; // escarpment

   // animation timer
   LARGE_INTEGER     m_liPerCountFreq; // counts per second
   LARGE_INTEGER     m_liLastCount;    // last timer tick

   // misc info
   fp                m_fMainAspect;       // aspect ratio of main window, width/height
   fp                m_fMainAspectSentToServer;  // what aspect ratio the server is using
   fp                m_f360FOVSentToServer;  // field of view sent to server

   // about the connection
   WCHAR             m_szConnectFile[256]; // file connected to
   DWORD             m_dwConnectIP;    // IP address of connection, if remote
   DWORD             m_dwConnectPort;  // Port address of connection, if remote
   BOOL              m_fQuickLogon;    // if TRUE then logon quickly without user info

   // tutorial cursor
   BOOL              m_fTutorialCursor;   // if TRUE then moving the tutorial cursor
   POINT             m_pTutorialCursor;   // where cursor should move to, in screen coords
   DWORD             m_dwTutorialCursorTime; // how long this has been running for. Dont let run forever
   
   // object diplay
   CBTree            m_tObjectDisplayID;  // object display IDs

   BOOL              m_fInPacketSendError;   // set to TRUE if already handling a packet send error

};
typedef CMainWindow *PCMainWindow;

LRESULT CALLBACK EditSubclassWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern WNDPROC gpEditWndProc;
//void PlayBeep (DWORD dwResource);
void BeepWindowBeepShow (DWORD dwShow);
extern DWORD gdwBeepWindowBeepShowDisable;  // temporarily increase to disable beeps

BOOL AppDataDirGet (PWSTR psz);
BOOL CacheFilenameGet (DWORD dwCache, DWORD dwMFNum, PWSTR pszFile);
void WriteRegRenderCache (DWORD dwKey);
DWORD GetRegRenderCache (void);



/*************************************************************************************
CIconWindow */

// CIWGroup - Store a group of icon info
class CIWGroup {
public:
   ESCNEWDELETE;

   CIWGroup (void);
   ~CIWGroup (void);

   BOOL Init (PCIconWindow pIW, PWSTR pszName, BOOL fCanChatTo);
   void Add (PCVisImage pAdd);

   // members
   PCIconWindow         m_pIW;         // icon window
   CMem                 m_memName;     // name of group, displayed to user
   BOOL                 m_fCanChatTo;  // set to TRUE if can chat to
   CListFixed           m_lPCVisImage; // list of icons in the group
   CListFixed           m_lVIRect;     // list of RECT where the images should go, and around image, onr per m_lPCVisImage

   // scratch space
   RECT                 m_rTitle;      // rectangle for where tile text should go
   RECT                 m_rGroup;      // group box line
};
typedef CIWGroup *PCIWGroup;


class CIconWindow {
   friend class CIWGroup;

public:
   ESCNEWDELETE;

   CIconWindow (void);
   ~CIconWindow (void);

   BOOL Init (PCMainWindow pMain, PCMMLNode2 pNode, __int64 iTime);
   BOOL Update (PCMMLNode2 pNode, __int64 iTime);
   void ObjectDelete (GUID *pgID);
   PCVisImage MouseOver (int x, int y, BOOL *pfOverMenu);
   void AudioStart (GUID *pgID);
   void AudioStop (GUID *pgID);
   void VerbChatNew (void);
   void VerbDeselect (DWORD dwSelected);
   BOOL ChatWhoTalkingTo (GUID *pgID, PWSTR *ppszLang, PWSTR *ppszStyle);
   BOOL ContainsPCVisImage (PCVisImage pView);
   void ChatToSet (int iSpeakMode, GUID *pgID);
   BOOL CanChatTo (void);

   BOOL Speak (PWSTR pszText);
   PCVisImage FindLastIcon (void);
   BOOL PositionIcons (BOOL fChangeScroll, BOOL fMove);

   // basically private, can look at but dont touch
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   CMem                 m_memID;       // string ID for the window
   CMem                 m_memName;     // name to display to user
   HWND                 m_hWnd;        // window for this
   BOOL                 m_fChatWindow; // set to TRUE if the icon window is used as a chat window
   BOOL                 m_fCanChatTo;  // set to TRUE if allows "type in text to speak" window
   BOOL                 m_fTextUnderImages;  // if TRUE, then draw text under images.
   CListVariable        m_lLanguage;   // list of languages the user speaks
   DWORD                m_dwLanguage;  // language index (into m_lLanguage) to use, or -1 for "most appropriate"
   CListFixed           m_lPCIWGroup;  // list of icon group info

private:
   BOOL TooltipUpdate (POINT pt);
   void Paint (HWND hWnd, HDC hDC, RECT *prClip);
   BOOL UpdateGroup (PCIWGroup pg, PCMMLNode2 pNode, BOOL fAppend, __int64 iTime);
   void VerbButtonsArrange (void);
   void ClientLoc (RECT *pr, BOOL fUseExisting);
   void ChatWindowToolbarLoc (int *piLeft, int *piBottom);
   void CursorToScroll (POINT pCursor, fp *pfRight, fp *pfDown);

   PCMainWindow         m_pMain;       // main window

   // scroll
   BOOL                 m_fScrollTimer;   // set to TRUE if scrolling timer
   DWORD                m_dwScrollDelay;  // clicks until scroll
   SCROLLINFO           m_siHorz;      // default horizontal scroll info, if no scrollbars
   SCROLLINFO           m_siVert;      // default vertical scroll info, if no scrollbars
   BOOL                 m_fScrollAuto; // set to TRUE if will automatically scroll (only if no title)
   HWND                 m_hWndHScroll; // horizontal scrollbar
   HWND                 m_hWndVScroll; // vertical scrollbar

   CListFixed           m_lRECTBack;   // list of background rectangles to clear
   int                  m_iTextHeight; // height of text line

#ifndef UNIFIEDTRANSCRIPT
   HWND                 m_hWndEdit;    // edit window for chat
#endif
   CListFixed           m_lPCIconButton;  // buttons used for chat window. Correspond to gpMainWindow->m_pResVerbChat list
   CListFixed           m_lAudioGUID;  // list of objects making noise
   GUID                 m_gSpeakWith;  // object that specifically speaking to, NULL if none (or none if can't find match)
   int                  m_iSpeakMode;  // 0 if talking to (if m_gSpeakWidth then specific person, else all),
                                       // 1 if whispering to specific person, -1 if yelling

   PCToolTip            m_pToolTip;    // tooltip for name
   CMem                 m_memToolTipCur;  // current tooltip string
   RECT                 m_rToolTipCur; // current tooltip location
};


/*************************************************************************************
CDisplayWindow */

class CDisplayWindow {
public:
   ESCNEWDELETE;

   CDisplayWindow (void);
   ~CDisplayWindow (void);

   BOOL Init (PCMainWindow pMain, PCMMLNode2 pNode, __int64 iTime);
   BOOL Init (PCMainWindow pMain, PWSTR pszName, PWSTR pszID, PCMMLNode2 pNode);
   BOOL Update (PCMMLNode2 pNode, __int64 iTime);
   void DeletePCVisView (void);

   // basically private, can look at but dont touch
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   CMem                 m_memID;       // string ID for the window
   CMem                 m_memName;     // name to display to user
   HWND                 m_hWnd;        // window for this
   PCVisImage           m_pvi;         // image in the window
   int                  m_iScrollSize; // size of the scrollbars
   BOOL                 m_fScrollCapture; // set to TRUE if the scrollbar is captured
   fp                   m_fScrollLastLR; // last horizontal scroll position
   fp                   m_fScrollLastUD; // last horizontal scroll position

private:
   void Paint (HWND hWnd, HDC hDC, RECT *prClip);
   void RoomAutoScrollAmount (POINT *pRoom, fp *pfLR, fp *pfUD, fp *pfLRScroll, fp *pfUDScroll, BOOL *pfInScrollRegion);

   void DelayClearTimerKill (void);
   void DelayClearTimerStart (void);

   PCMainWindow         m_pMain;       // main window
   DWORD                m_dwTimeFirstWantToScroll; // GetTickCount() when first want tos croll, or 0 if nothing so far
   BOOL                 m_fDelayClearTimerOn;   // set to TRUE if the delay clear timer is in
   BOOL                 m_fDelayClearTimerWentOff; // set to TRUE if timer has gone off
};


/************************************************************************************
Main */
extern PCMainWindow   gpMainWindow;
extern BOOL           gfQuitBecauseUserClosed;   // set to TRUE if the close was the user's doing
extern char           gszAppDir[256];
extern BOOL           gfMonitorUseSecond;   // if TRUE the use the second monitor
extern RECT           grMonitorSecondRect;        // rectangle for the secondary monitor
extern DWORD          gdwMonitorNum;         // number of monitors attached to the system
extern PCResTitleInfoSet gpLinkWorld;      // where to link to
extern __int64        giTotalPhysicalMemory;

void MonitorInfoFill (BOOL fSecondTime);

/*************************************************************************************
CMapWindow */

class CMapZone;
typedef CMapZone *PCMapZone;

class CMapRegion;
typedef CMapRegion *PCMapRegion;

class CMapMap;
typedef CMapMap *PCMapMap;

#define ROOMEXITS          12       // number of exits. 0=N, 1=NE, clickwise, 8=u, 9=d, 10=in, 11=out

// CMapRoom - Describe a room
class CMapRoom {
public:
   ESCNEWDELETE;

   CMapRoom (void);
   ~CMapRoom (void);
   void DeleteAll (PCHashGUID phRooms);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PCMapZone pZone, PCMapRegion pRegion, PCMapMap pMap,
      PCHashGUID phRooms);

   // public variables
   GUID              m_gID;         // GUID. Dont change once set
   CMem              m_memName;     // Name string
   CMem              m_memDescription; // description for the room, usually the room objects
   CPoint            m_pLoc;        // location, [0]=x center (E=pos), [1]=y center (N=pos), [2]=x width, [3]y width, in meters
   DWORD             m_dwShape;     // shape. 0 = rectangular, 1 = elliptical
   COLORREF          m_acColor[2];      // color, [0] = dark, [1] = light
   fp                m_fRotation;   // amount to rotate. 0 = none, right-handed around Z
   GUID              m_agExits[ROOMEXITS];   // exits to what room. GUID_NULL if no exit

   // links to other objects
   PCMapZone         m_pZone;       // zone it's in
   PCMapRegion       m_pRegion;     // region it's in
   PCMapMap          m_pMap;        // map it's in

private:
};
typedef CMapRoom *PCMapRoom;


// CMapMap - Describe an individual map
class CMapMap {
public:
   ESCNEWDELETE;

   CMapMap (void);
   ~CMapMap (void);
   void DeleteAll (PCHashGUID phRooms);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PCMapZone pZone, PCMapRegion pRegion, PCHashGUID phRooms);

   BOOL RoomAdd (PCMapRoom pRoom);
   BOOL RoomRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms);
   DWORD RoomNum (void);
   PCMapRoom RoomGet (DWORD dwIndex);
   DWORD RoomFindIndex (GUID *pgID);
   PCMapRoom RoomFind (GUID *pgID);

   BOOL Size (PCPoint pSize);

   // public
   CMem              m_memName;        // name string
   CListFixed        m_lPCMapRoom;     // list of rooms

   // links
   PCMapZone         m_pZone;          // zone
   PCMapRegion       m_pRegion;        // region

private:
};


// CMapRegion - Describe a region
class CMapRegion {
public:
   ESCNEWDELETE;

   CMapRegion (void);
   ~CMapRegion (void);
   void DeleteAll (PCHashGUID phRooms);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PCMapZone pZone, PCHashGUID phRooms);

   BOOL MapAdd (PCMapMap pMap);
   BOOL MapRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms);
   DWORD MapNum (void);
   PCMapMap MapGet (DWORD dwIndex);
   DWORD MapFindIndex (PWSTR pszName, BOOL fCreateIfNotExist = FALSE);
   PCMapMap MapFind (PWSTR pszName, BOOL fCreateIfNotExist = FALSE);

   HMENU PopupMenu (PCListFixed plPCMapRoom, PCMapMap pCurMap, PCMapMap *ppMap);

   // public
   CMem              m_memName;        // name string
   CListFixed        m_lPCMapMap;      // list of Maps

   // links
   PCMapZone         m_pZone;          // zone

private:
};



// CMapZone - Describe a Zone
class CMapZone {
public:
   ESCNEWDELETE;

   CMapZone (void);
   ~CMapZone (void);
   void DeleteAll (PCHashGUID phRooms);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PCHashGUID phRooms);

   BOOL RegionAdd (PCMapRegion pRegion);
   BOOL RegionRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms);
   DWORD RegionNum (void);
   PCMapRegion RegionGet (DWORD dwIndex);
   DWORD RegionFindIndex (PWSTR pszName, BOOL fCreateIfNotExist = FALSE);
   PCMapRegion RegionFind (PWSTR pszName, BOOL fCreateIfNotExist = FALSE);

   HMENU PopupMenu (PCListFixed plPCMapRoom, PCMapMap pCurMap, PCMapMap *ppMap);

   // public
   CMem              m_memName;        // name string
   CListFixed        m_lPCMapRegion;      // list of Regions

private:
};


// CMapWindow - Handle the window
class CMapWindow {
public:
   ESCNEWDELETE;

   CMapWindow (void);
   ~CMapWindow (void);

   BOOL Init (PCMainWindow pMain);
   BOOL AutoMapMessage (PCMMLNode2 pNode, BOOL fShow);

   void DeleteAll (PCHashGUID phRooms);
   void Show (BOOL fNoChangeTitle);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   BOOL ZoneAdd (PCMapZone pZone);
   BOOL ZoneRemove (DWORD dwIndex, BOOL fDelete, PCHashGUID phRooms);
   DWORD ZoneNum (void);
   PCMapZone ZoneGet (DWORD dwIndex);
   DWORD ZoneFindIndex (PWSTR pszName, BOOL fCreateIfNotExist = FALSE);
   PCMapZone ZoneFind (PWSTR pszName, BOOL fCreateIfNotExist = FALSE);

   PCMapRoom RoomFind (GUID *pgID);
   void MapPointTo (PWSTR pszText, PCPoint pLoc, COLORREF cColor);
   void CursorToScroll (POINT pCursor, fp *pfRight, fp *pfDown);
   void RoomCenter (PCMapRoom pCurRoom);

   void Generate360Info (GUID *pgRoom, PCListFixed plMAP360INFO, PCListVariable plMapStrings);
   BOOL RandomAction (PCMem pMemAction, GUID *pgObject, LANGID *pLangID);

   // public
   CListFixed        m_lPCMapZone;      // list of Zones
   GUID              m_gRoomCur;       // current room, or GUID_NULL of none
   BOOL              m_fHiddenByUser; // set to TRUE if the verb is hidden by the user
   BOOL              m_fHiddenByServer;  // set to TRUE if the verb is hidden by the server

   // private, look but dont touch
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   HWND              m_hWnd;           // window

private:
   void ClearDeadMaps (void);
   void AdjustSizeForAspect (PCPoint pSize);
   void RecalcView (BOOL fIgnoreScroll = FALSE);
   void RecalcView360 (void);
   void MetersToClient (fp fX, fp fY, LONG *piX, LONG *piY);
   void ClientToMeters (int iX, int iY, fp *pfX, fp *pfY);
   void PaintRoom (HDC hDC, PCMapRoom pRoom, DWORD dwPass, int iScale, HFONT hFontBig, HFONT hFontSmall);
   HMENU PopupMenu (PCListFixed plPCMapRoom, PCMapMap pCurMap, PCMapMap *ppMap);
   void ContextMenu (void);
   void RoomBoundingBox (PCMapRoom pCurRoom, DWORD dwDist, PCPoint pMinMax);
   PCMapRoom IsOverRoom (POINT *p);
   DWORD WalkToDirection (PCMapRoom pRoom);
   DWORD WalkToDirection (POINT *p);
   void PaintMapPointTo (HDC hDC, int iScale, HFONT hFont);
   HBITMAP PaintEnlarged (HDC hDCPaint, int iScale, BOOL fTransparent);
   void PaintEnlargedNoBitmap (HDC hDCPaint, int iScale, BOOL fClearBackground);
   HBITMAP PaintAnti (HDC hDCPaint, BOOL fTransparent);
   void Generate360InfoRecurse (GUID *pgRoom, DWORD dwDistance, DWORD dwThisDistance, PCMapRoom pRoomCenter,
      PCListFixed plMAP360INFO, PCListVariable plMapStrings);
   BOOL WalkToAction (PCMapRoom pRoom, PCMem pMemDo, PCMem pMemShow, LANGID *pLangID);

   CHashGUID         m_hRooms;         // hash of GUID to PCMapRoom
   PCMainWindow      m_pMain;          // main window
   RECT              m_rWindowLoc;     // window location

   // for displaying window
   RECT              m_rClient;        // client coords
   CMem              m_memCurZone;     // current zone
   CMem              m_memCurRegion;   // curent region
   CMem              m_memCurMap;      // current map
   CPoint            m_pMapSize;       // size of the map, [0]=l, [1]=r, [2]=y min, [3]=ymax
   CPoint            m_pView;          // what vieweing, [0]=centerx in m, [1]=centery in m, [2]=width in m

   // map point to
   BOOL              m_fMapPointTo;    // if TRUE then pointing
   CMem              m_memMapPointTo;  // text displayed
   CPoint            m_pMapPointTo;    // location that pointing to
   COLORREF          m_acMapPointTo[2];    // color. [0] = dark, [1]= light

   // scroll
   BOOL              m_fScrollTimer;   // set to TRUE if scrolling timer
   DWORD             m_dwScrollDelay;  // clicks until scroll

   DWORD             m_dwLastUpdateTime;  // time of last update, GetTickCount()
   CListFixed        m_lLastUpdate;    // list of rooms in the last update time
};

/************************************************************************************
LogIn */
BOOL LogIn (PWSTR pszFile, PCResTitleInfoSet *ppLinkWorld, PWSTR pszUserName, PWSTR pszPassword, BOOL *pfNewAccount,
            BOOL *pfRemote, DWORD *pdwIP, DWORD *pdwPort, BOOL *pfQuickLogon,
            PCMem pMemJPEGBack);
void LargeTTSRequirements (BOOL *pfRegistered, BOOL *pfWin64, BOOL *pfCirc64,
                           BOOL *pfDualCore, BOOL *pfRAM);
char *CircumrealityRegBase (void);
DWORD BeginsWithHTTP (PWSTR psz);
BOOL NameToIP (PWSTR psz, DWORD *pdwIP, int *piErr, PWSTR pszErr);
BOOL AddressToIP (PWSTR psz, DWORD *pdwIP);





/***********************************************************************************
MIFOpen */

// CCircumrealityFileInfo - Info about specific wave file in directory
class CCircumrealityFileInfo {
public:
   ESCNEWDELETE;

   CCircumrealityFileInfo (void);
   ~CCircumrealityFileInfo (void);
   BOOL SetText (PWSTR pszFile);
   BOOL FillFromFile (PWSTR pszDir, PWSTR pszFile);
   BOOL FillFromFindFile (PWIN32_FIND_DATA pFF);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CCircumrealityFileInfo *Clone (void);

   // can be read, but dont change
   PWSTR          m_pszFile;        // file name (without directory)

   PCResTitleInfoSet m_pTitleInfoSet;      // title info resource

   // can be changed
   FILETIME       m_FileTime;       // file time
   __int64        m_iFileSize;      // file size
   BOOL           m_fCircumreality;           // set to TRUE if CRF, else CRL
   CIRCUMREALITYSERVERLOAD m_ServerLoad;  // how many known users

private:
   CMem           m_memInfo;        // memory to store info
};
typedef CCircumrealityFileInfo *PCCircumrealityFileInfo;



// CCircumrealityDirInfo - Information about all the wave files in a directory
class CCircumrealityDirInfo {
public:
   ESCNEWDELETE;

   CCircumrealityDirInfo(void);
   ~CCircumrealityDirInfo (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CCircumrealityDirInfo *Clone (void);

   BOOL SyncWithDirectory (PCProgressSocket pProgress);
   BOOL SyncFiles (PCProgressSocket pProgress, PCWSTR pszDir);
   DWORD FillListBox (PCEscPage pPage, PCWSTR pszControl, PCWSTR pszDir, LANGID lid);
   BOOL RemoveFile (PCWSTR pszFile);
   void ClearFiles (void);

   // should set directory into this
   WCHAR             m_szDir[256];     // directory. Form "c:\hello\bye"... not final "\"

   // can read but don't change
   CListFixed        m_lPCCircumrealityFileInfo;   // list of Pwavefileinfo, sorted alphabetically
};
typedef CCircumrealityDirInfo *PCCircumrealityDirInfo;

DEFINE_GUID(GUID_CircumrealityCache, 
0x3e164df3, 0x9444, 0x4917, 0xae, 0x85, 0x6f, 0xb2, 0xfc, 0x91, 0xfb, 0x48);

BOOL CircumrealityFileOpen (HWND hWnd, BOOL fSave, LANGID lid, PWSTR pszFile, BOOL fSkipIfOnlyOneFile);
BOOL CircumrealityFileOpen (PCEscWindow pWindow, BOOL fSave, LANGID lid, PWSTR pszFile, BOOL fSkipIfOnlyOneFile);
BOOL CircumrealityFileIsDefault (HWND hWnd, PWSTR pszFile);



/***********************************************************************************
Register */
DWORD GetAndIncreaseUsage (DWORD dwAmount = 0);
int RegisterMode (void);
BOOL RegisterPageDisplay (PCEscWindow pWindow);



/*************************************************************************************
CPasswordFile */

#define DEFAULTTIMESSINCEREMIND           5        // remind to shut off tutorial ever 5 times that log on

DEFINE_GUID(GUID_PasswordFile, 
0x1735af77, 0x72cb, 0x479c, 0x1f, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0xa4, 0xb6);

class CPasswordFileAccount;
typedef CPasswordFileAccount *PCPasswordFileAccount;

class CPasswordFile {
public:
   ESCNEWDELETE;

   CPasswordFile (void);
   ~CPasswordFile (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PWSTR pszPassword, PCMMLNode2 pNode);
   BOOL CloneTo (CPasswordFile *pTo);
   CPasswordFile *Clone (void);
   void Clear (void);
   BOOL Open (PWSTR pszFile, PWSTR pszPassword, BOOL fCreateIfNotExist);
   BOOL Save (void);
   void Dirty (void);

   DWORD Checksum (PWSTR pszPassword);
   BOOL PasswordChange (PWSTR pszPassword);
   DWORD AccountNew (PCResTitleInfoSet pInfoSet, DWORD dwShard, LANGID LID,
      PWSTR pszPasswordFile, PWSTR pszFile);
   DWORD AccountFind (DWORD dwID);
   PCPasswordFileAccount AccountGet (DWORD dwIndex);
   BOOL AccountRemove (DWORD dwIndex);
   DWORD AccountNum (void);
   void AccountSort (void);

   // public
   BOOL              m_fDirty;            // set to TRUE if has changed and needs to be saved
   WCHAR             m_szFile[256];       // filename to save/load
   WCHAR             m_szPassword[64];    // password
   PCMMLNode2        m_pMMLNodeMisc;      // miscellaneous information

   // preferences
   WCHAR             m_szEmail[64];       // Email to use, can be empty
   WCHAR             m_szPrefName[64];    // preferred character name, can be empty
   WCHAR             m_szDescLink[128];   // link to web site in player description
   CMem              m_memPrefDesc;       // preferred player description
   DWORD             m_dwAge;             // age to use, or 0 if none
   DWORD             m_dwHoursPlayed;     // hours played per week for matchup, 0 if not used
   int               m_iPrefGender;       // preferred gender. 1= male, -1 = female, 0 = ask
   BOOL              m_fExposeTimeZone;   // TRUE if expose time zone
   BOOL              m_fDisableTutorial;  // if TRUE then disable the tutorial

   DWORD             m_dwTimesSinceRemind;   // number of times since has reminded that should turn off tutorial

   // misc bits
   DWORD             m_dwPasswordChecksum;   // checksum on the password for quick test of valid
   DWORD             m_dwUniqueIDCur;     // next unique ID to use. Increase after add PasswordFileAccount

   // accounts
   CListFixed        m_lPCPasswordFileAccount;  // list of accounts that are set up

   // just used for the player info ui, nowhere else
   BOOL              m_fSimplifiedUI;     // set for the player info ui

private:
};

typedef CPasswordFile *PCPasswordFile;

void PasswordFileGet (PWSTR pszFile, PWSTR pszPassword);
DWORD PasswordFileSet (PWSTR pszFile, PWSTR pszPassword, BOOL fCreateIfNotExist = FALSE);
BOOL PasswordFileRelease (void);
PCPasswordFile PasswordFileCache (BOOL fCreateIfNotExist = FALSE);
void RemoveIllegalFilenameChars (PWSTR psz);
PCPasswordFileAccount PasswordFileAccountCache (void);
void PasswordFileUniqueIDSet (DWORD dwID);




// CPasswordFileAccount - Where an account is stored
class CPasswordFileAccount {
public:
   ESCNEWDELETE;

   CPasswordFileAccount (void);
   ~CPasswordFileAccount (void);
   BOOL Init (PCPasswordFile pPasswordFile);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CPasswordFileAccount *pTo);
   CPasswordFileAccount *Clone (void);
   void Clear (void);
   void Dirty (void);
   void GenerateUserPassword (PWSTR pszPasswordFile);
   void GenerateUniqueID (void);

   // settings
   WCHAR             m_szUser[64];        // user name, unencoded
   WCHAR             m_szPassword[64];    // password, unencoded
   DFDATE            m_dwLastUsed;        // last date that this was used
   BOOL              m_fNewUser;          // if TRUE, then the next time account is used, log on as new user
   PCResTitleInfoSet m_pCResTitleInfoSet; // information about the title saved in the file
   DWORD             m_dwShard;           // shard used in the m_pCResTitleInfoSet
   DWORD             m_dwUniqueID;        // unique ID for each account. Gotteon from m_dwUniqueIDCur
   CMem              m_memFileName;       // filename of the .crf file. Used for single-player
   CMem              m_memName;           // user-specified name for the account. If empty-string then use from pCResTitleInfoSet

   PCMMLNode2        m_pMMLNodeMisc;      // miscellaneous information

   PCPasswordFile    m_pCPasswordFile;    // password file

private:
   void EncodeDecode (PWSTR pszPassword, PBYTE pabData, DWORD dwSize);

};



/*************************************************************************************
CHypnoManager */


// CHypnoEffectSocket - virtual class for hypno effects
class CHypnoEffectSocket {
public:
   virtual BOOL EffectUnderstand (PWSTR pszEffect) = 0; // see if the socket can understand the effect.
                                                      // it doesn't start using it though. Returns TRUE if it understands
   virtual BOOL EffectUse (PWSTR pszEffect) = 0;     // causes the socket to use the effect. Called from thread #0
   virtual BOOL TimeTick (DWORD dwThread, fp fTimeElapsed, BOOL fLightBackground, PCImageStore pis) = 0;
         // Called once every 30-200 milliseconds to have the effect do something using the current
         // effect. dwThread is the thread number, 0 or 1. fTimeElapsed is the number of seconds since
         // the last call. fLightBackground indicates the the displayed text has a white background.
         // pis is only passed in for thread #0, and contains the last image returned from Tick(). It
         // should be filled in with a new image. Returns TRUE to use the image, FALSE to skip
   virtual void DeleteSelf (void) = 0;       // use this to delete
};
typedef CHypnoEffectSocket *PCHypnoEffectSocket;

// CHypnoEffectNoise
#define HEN_DEPTH       4        // how much detail

class CHypnoEffectNoise : public CHypnoEffectSocket {
public:
   ESCNEWDELETE;

   CHypnoEffectNoise (void);
   ~CHypnoEffectNoise (void);

   // from CHypnoEffectSocket
   BOOL EffectUnderstand (PWSTR pszEffect);
   BOOL EffectUse (PWSTR pszEffect);
   BOOL TimeTick (DWORD dwThread, fp fTimeElapsed, BOOL fLightBackground, PCImageStore pis);
   void DeleteSelf (void);

private:
   void NoiseFill (DWORD dwPingPong);
   void ColorMapAdjust (DWORD dwIndex, DWORD dwEffect, DWORD dwEffectSub, BOOL fLightBackground, fp *pafRGB);

   // all under the critical section
   CRITICAL_SECTION        m_CritSec;     // critical section for accessing the following
   POINT                   m_pRes;        // resolution to use
   CMem                    m_memHeightField; // height field. m_dwCurPosn = width x height x sizeof(byte)
   fp                      m_afScale[HEN_DEPTH];   // scaling that aspire to
   fp                      m_afScaleNoise[HEN_DEPTH]; // scaling that aspire to
   fp                      m_fInterpScaleWithOldTime; // so interpolate with old scale

   // settings
   DWORD                   m_dwEffect;       // cellular effect number to use
   DWORD                   m_dwEffectSub;    // sub-effect
   fp                      m_fNoiseAlphaPerSec; // how much the noise's alpha changes per second
   fp                      m_fColorNoiseAlphaPerSec; // how much the color map changes per second
   fp                      m_fColorOffsetPerSec;  // how much to change offset per sec
   CPoint                  m_pNoiseOffsetPerSec;   // only p[0] and p[1] are used

   // thread #0
   fp                      m_fInterpRGBWithOldTime;   // so blend RGB in with whatever is shown over
                                                      // short time
   fp                      m_fInterpColorMapWithOldTime; // so blend in the new color map with the old one
   COLORREF                m_acMapOld[256];     // old color map
   CNoise2D                m_aColorNoise[2]; // ping-pong for color noise
   fp                      m_fColorNoiseAlpha;  // current noise alpha
   BOOL                    m_fColorNoiseAlphaDirection;  // if TRUE then alpha is going in positive direction, FALSE then negative
   fp                      m_afColorOffset[HEN_DEPTH]; // how much to offset
   fp                      m_afColorScale[HEN_DEPTH];   // scaling that aspire to
   fp                      m_afColorScaleNoise[HEN_DEPTH]; // scaling that aspire to

   // thread #1
   CMem                    m_memHeightWork;  // working height field
   fp                      m_fNoiseAlpha;    // current noise alpha
   BOOL                    m_fNoiseAlphaDirection; // if TRUE then alpha is going in the positive direction
   CPoint                  m_apNoiseOffset[HEN_DEPTH];
   CMem                    m_amemNoise[2];     // memory contianing the noise
   fp                      m_afScaleCur[HEN_DEPTH];   // current scaling to use
   fp                      m_afScaleNoiseCur[HEN_DEPTH]; // current scaling to use
};
typedef CHypnoEffectNoise *PCHypnoEffectNoise;



class CHypnoEffectCellular : public CHypnoEffectSocket {
public:
   ESCNEWDELETE;

   CHypnoEffectCellular (void);
   ~CHypnoEffectCellular (void);

   // from CHypnoEffectSocket
   BOOL EffectUnderstand (PWSTR pszEffect);
   BOOL EffectUse (PWSTR pszEffect);
   BOOL TimeTick (DWORD dwThread, fp fTimeElapsed, BOOL fLightBackground, PCImageStore pis);
   void DeleteSelf (void);

private:
   void CellularFill (DWORD dwPingPong, POINT pRes, DWORD dwEffect, BOOL fLightBackground);
   void StimulusFill (PCImageStore pImage, DWORD dwEffect, BOOL fLightBackground, fp fTimeElapsed);

   // all under the critical section
   CRITICAL_SECTION        m_CritSec;     // critical section for accessing the following
   POINT                   m_pRes;        // resolution to use
   CMem                    m_memHYPNOCELL; // cellular rules to use. m_dwCurPosn = width x height x sizeof(HYPNOCELL)

   // settings
   DWORD                   m_dwEffect;       // cellular effect number to use
   fp                      m_fFrameAlphaPerSec; // how much the cell's frame changes per second

   // thread #0
   CImageStore             m_isCopy;         // copy of data passed in

   // thread #1
   CMem                    m_memHYPNOCELLWork;  // working HYPNOCELL field
   fp                      m_fFrameAlpha;    // current noise alpha
   BOOL                    m_fFrameAlphaDirection; // if TRUE then alpha is going in the positive direction
   CMem                    m_amemHYPNOCELL[2];     // memory contianing the cell frames to interpolate between
};
typedef CHypnoEffectCellular *PCHypnoEffectCellular;


// HMTHREADINFO - Thread info
typedef struct {
   PCHypnoManager    pThis;   // this
   DWORD             dwThread;   // thread number to use
} HMTHREADINFO, *PHMTHREADINFO;

// CHypnoManager
#define HYPNOTHREADS       2
#define HYPNOMONITORS      2

class CHypnoManager {
public:
   ESCNEWDELETE;

   CHypnoManager (void);
   ~CHypnoManager (void);

   void MonitorsSet (DWORD dwMonitors);
   void MonitorResSet (DWORD dwMonitor, int iX, int iY);
   void LightBackgroundSet (BOOL fLightBackground);
   void LowPowerSet (BOOL fLowPower);
   void EffectSet (PWSTR pszEffect);
   BOOL BitBltImage (HDC hDC, RECT *prClient, DWORD dwMonitor);

   BOOL Init (void);
   BOOL ShutDown (void);

   // thread
   void HypnoManagerThread (DWORD dwThread);

private:
   // under critical section
   CRITICAL_SECTION  m_CritSec;     // critical section
   BOOL              m_fRunning;    // set to TRUE when running. Makes sure don't send message when shutting down
   DWORD             m_dwMonitors;  // number of monitors
   POINT             m_apMonitorRes[HYPNOMONITORS];   // resolution of each monitor
   BOOL              m_fLightBackground;  // if TRUE then white background with black text
   BOOL              m_fLowPower;   // if TRUE then low-power mode
   CMem              m_memEffect;   // memory with string containing effect name to use
   BOOL              m_afThreadWantActive[HYPNOTHREADS];  // set to TRUE if want thread active
   BOOL              m_afThreadActive[HYPNOTHREADS];   // set to TRUE when thread is active

   // second critical sction
   CRITICAL_SECTION  m_CritSecBmp;     // critical section
   HBITMAP           m_ahbmpBack[HYPNOMONITORS];   // background image for each monitor
   POINT             m_apBackRes[HYPNOMONITORS];  // size of each image

   // threads
   HANDLE            m_ahThread[HYPNOTHREADS];      // threads
   HANDLE            m_ahSignalToThread[HYPNOTHREADS];   // signal to threads
   HANDLE            m_ahSignalFromThread[HYPNOTHREADS]; // signal from threads

   PCHypnoEffectSocket m_apHypnoEffect[HYPNOMONITORS];             // current hypno effect

   HMTHREADINFO      m_aHMTHREADINFO[HYPNOTHREADS];   // thread info
   // queues
   //CListFixed        m_alHYPNOMSG[HYPNOTHREADS];   // messages for each thread
   //CListVariable     m_alHypnoMsgExtra[HYPNOTHREADS]; // extra information for each thread
};


/*************************************************************************************
ColorBalance.cpp */
void ImageSquashIntensity (PCImage pImage, DWORD dwIntensityMin, DWORD dwIntensityMax);
void AverageColorOfImage (PCImage pImage, WORD *pawColorAll, WORD *pawColorTop);
HBITMAP ImageSquashIntensityToBitmap (PCImage pImage, BOOL fLightBackground, DWORD dwExtra, HWND hWnd,
                                      COLORREF *pcrAverageAll, COLORREF *pcrAverageTop);

#endif // _MIFCLIENT_H


