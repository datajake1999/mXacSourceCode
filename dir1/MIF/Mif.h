/******************************************************************************
Mif.h - Header for MIF components

Begun 28/2/04 by Mike Rozak
*/


#ifndef _MIF_H_
#define _MIF_H_


#define CLIENTVERSIONNEED           4           // version of client that server needs
   // BUGFIX - Upped to 2 since changed TTS file format
#define DEFLANGID                   0x409       // default to english

#define MIFTRANSPARENTWINDOWS       // so that will try to draw transparent windows

#ifdef MIFTRANSPARENTWINDOWS
#define LAYEREDTRANSPARENTCOLOR     RGB(0, 0, 0)     // this color is transparent
#define LAYEREDTRANSPARENTCOLORREMAP RGB(0,0,1)    // if transparent color, remap to this
#endif

/************************************************************************************
CCircumrealityWinSockData */
#define WINSOCK_SENDRECEIVETIMEOUT        30000       // 30 seconds

class DLLEXPORT CCircumrealityWinSockData {
public:
   ESCNEWDELETE;

   CCircumrealityWinSockData (void);
   ~CCircumrealityWinSockData (void);

   BOOL Init (SOCKET iSocket, char *pszNameThis, char *pszNameServer = NULL);

   int Send (PVOID pData, DWORD dwSize);
   int Receive (PVOID pData, DWORD dwSize);
   int GetLastError (PCWSTR *ppszErr);
   int GetFirstError (PCWSTR *ppszErr);

   // for accessing interal window info
   HWND WindowGet (void);
   void WindowSentToSet (HWND hWnd);

   DWORD             m_dwLag;       // simulated network lag, in milliseconds
   DWORD             m_dwBaud;      // simulated bits-per-second of transfer
   DWORD             m_dwSendSize;  // simulated send size
   DWORD             m_dwSendPeriod;   // number of milliseconds between send. Must be set before call init if want to change
   DWORD             m_dwMaxChanBufSize;  // recommened maximum channel buffer size

   // look at but dont touch
   SOCKET            m_iSocket;     // if using winsock, this is the socket, else INVALID_SOCKET

   // private
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
   HWND              m_hWnd;        // window to receive WM_COPYDATA
   HWND              m_hWndSendTo;  // window to send messages to
   CListVariable     m_lSend;       // list of data to send
   CListFixed        m_lSendAt;     // list of DWORDs for time to send
   CMem              m_memReceive;  // memory to receive
   int               m_iErrLast;        // current error
   PCWSTR            m_pszErrLast;      // associated error description. MIght be NULL.
   int               m_iErrFirst;   // first occurance of an error
   PCWSTR            m_pszErrFirst; // first occurance of an error
   DWORD             m_dwLastDataSent; // time the last data was sent from here
};
typedef CCircumrealityWinSockData *PCCircumrealityWinSockData;


/*************************************************************************************
CMIFPacket */

#define CIRCUMREALITYPACKET_PINGSEND          1     // test send a ping, the recipient automatically
                                          // replies with CIRCUMREALITYPACKET_PINGRECIEVED
#define CIRCUMREALITYPACKET_PINGRECEIVED      2     // automatically sent in response to CIRCUMREALITYPACKET_PINGSEND

// sent to client
#define CIRCUMREALITYPACKET_MMLIMMEDIATE      100   // this is a MML display/audio and should be processed immedaitely
#define CIRCUMREALITYPACKET_MMLQUEUE          101   // this is a MML display/audio and should be put on the queue
#define CIRCUMREALITYPACKET_FILEDATA          102   // file data that is to be sent from server to client
                                          // Data is a FILETIME (for modify time) followed by a NULL-terminated file name followed by the full file data.
                                          // 0-sized file data indicates the file couldnt be found
#define CIRCUMREALITYPACKET_FILEDELETE        103   // list of files that the client should delete. NULL-terminates strings.
#define CIRCUMREALITYPACKET_CLIENTVERSIONNEED 104   // the data is the version of client needed, CLIENTVERSIONNEED as a DWORD.
#define CIRCUMREALITYPACKET_VOICECHAT         105   // MML with voice chat info, followed by binary of voice chat
#define CIRCUMREALITYPACKET_SERVERLOAD        106   // how much the server is being used. CIRCUMREALITYSERVERLOAD structure.

#define CIRCUMREALITYPACKET_CLIENTREPLYWANTTTS  107   // sent to client saying if server wants or doesnt want TTS data
                                                      // uses CIRCUMREALITYPACKETCLIENTREPLYWANT
#define CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER  108   // sent to client saying if server wants or doesnt want render data
                                                      // uses CIRCUMREALITYPACKETCLIENTREPLYWANT

#define CIRCUMREALITYPACKET_CLIENTCACHETTS   109      // server is sending cached TTS data to the client
                                                      // uses CIRCUMREALITYPACKETCLIENTCACHE
#define CIRCUMREALITYPACKET_CLIENTCACHERENDER 110      // server is sending cached render data to the client
                                                      // uses CIRCUMREALITYPACKETCLIENTCACHE
#define CIRCUMREALITYPACKET_CLIENTCACHEFAILTTS 111      // server can't find the requested cached data
                                                      // uses CIRCUMREALITYPACKETCLIENTCACHE
#define CIRCUMREALITYPACKET_CLIENTCACHEFAILRENDER 112      // server can't find the requested cached data
                                                      // uses CIRCUMREALITYPACKETCLIENTCACHE

// sent to server
#define CIRCUMREALITYPACKET_TEXTCOMMAND       200    // send a text command from the client to the server. Data is LANGID, followed by raw text with NULL terminateion
#define CIRCUMREALITYPACKET_FILEREQUEST       201   // sent from client to server to request that a file be download.
                                          // Data is the null-terminated name of the file
                                          // Ther server is expected to send vakc CIRCUMREALITYPACKET_FILEREQUEST. A 0-sized
                                          // file indicates an error (no such file)
#define CIRCUMREALITYPACKET_FILEREQUESTIGNOREDIR 202   // likse CIRCUMREALITYPACKET_FILEREQUEST, except that doesn't care about directory
                                          // name (used for M3D libraries)
#define CIRCUMREALITYPACKET_FILEDATEQUERY     203   // this queries the server to see what files are out of data.
                                          // the data is a list of FILETIME (for the file's date), followed
                                          // by null-terminated file name. Repeat as many files as necessary.
                                          // Client then expects a CIRCUMREALITYPACKET_FILEDELETE call with
                                          // any files that are out of date
#define CIRCUMREALITYPACKET_CLIENTLOGOFF      204   // sent by the client if logs off from the server. No data
#define CIRCUMREALITYPACKET_UPLOADIMAGE       205   // sent to upload an image to the server
#define CIRCUMREALITYPACKET_UNIQUEID          206   // transfer string with a unique ID, WCHAR string
//#define CIRCUMREALITYPACKET_VOICECHAT       105   // voicechat can also be recevied by the server
#define CIRCUMREALITYPACKET_INFOFORSERVER     207   // single MML node with name and a value, v=xxx. Info such as time zone, language, graph speed

#define CIRCUMREALITYPACKET_SERVERQUERYWANTTTS  208   // asks the server if it wants TTS
                                                      // uses CIRCUMREALITYPACKETSEVERQUERYWANT
#define CIRCUMREALITYPACKET_SERVERQUERYWANTRENDER 209 // asks the server if it wants rendered image
                                                      // uses CIRCUMREALITYPACKETSEVERQUERYWANT
#define CIRCUMREALITYPACKET_SERVERCACHETTS      210   // sent to server from client saying here's your TTS data
                                                      // uses CIRCUMREALITYPACKETSEVERCACHE
#define CIRCUMREALITYPACKET_SERVERCACHERENDER   211   // sent to server from client saying here's your render data
                                                      // uses CIRCUMREALITYPACKETSEVERCACHE
#define CIRCUMREALITYPACKET_SERVERREQUESTTTS    212   // sent to server requesting a specific TTS string
                                                      // uses CIRCUMREALITYPACKETSEVERREQUEST
#define CIRCUMREALITYPACKET_SERVERREQUESTRENDER 213   // sent to server requesting a specific TTS string
                                                      // uses CIRCUMREALITYPACKETSEVERREQUEST


// CIRCUMREALITYPACKETSEVERREQUEST - Used by CIRCUMREALITYPACKET_SERVERREQUESTTTS and CIRCUMREALITYPACKET_SERVERREQUESTRENDER
typedef struct {
   DWORD             dwQualityMin;     // quality level, minimum
   DWORD             dwQualityMax;     // quality level, maximum (exclusive)
   DWORD             dwStringSize;  // size of the string, in bytes. Follows immediately after structure
   // BYTE[dwStringSize] - String follows at the end of the structure
} CIRCUMREALITYPACKETSEVERREQUEST, *PCIRCUMREALITYPACKETSEVERREQUEST;

// CIRCUMREALITYPACKETCLIENTREPLYWANT - Used by CIRCUMREALITYPACKET_CLIENTREPLYWANTTTS and CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER
typedef struct {
   DWORD             dwQuality;     // quality level
   BOOL              fWant;         // set to TRUE if the server wants the data, FALSE if not
   DWORD             dwStringSize;  // size of the string, in bytes. Follows immediately after structure
   // BYTE[dwStringSize] - String follows at the end of the structure
} CIRCUMREALITYPACKETCLIENTREPLYWANT, *PCIRCUMREALITYPACKETCLIENTREPLYWANT;


// CIRCUMREALITYPACKETSEVERQUERYWANT - Used by CIRCUMREALITYPACKET_SERVERQUERYWANTTTS and CIRCUMREALITYPACKET_SERVERQUERYWANTRENDER
typedef struct {
   DWORD             dwQuality;     // quality level
   DWORD             dwStringSize;  // size of the string, in bytes. Follows immediately after structure
   // BYTE[dwStringSize] - String follows at the end of the structure
} CIRCUMREALITYPACKETSEVERQUERYWANT, *PCIRCUMREALITYPACKETSEVERQUERYWANT;

// CIRCUMREALITYPACKETSEVERCACHE - Used by CIRCUMREALITYPACKET_SERVERCACHETTS and CIRCUMREALITYPACKET_SERVERCACHERENDER
typedef struct {
   DWORD             dwQuality;     // quality level
   DWORD             dwStringSize;  // size of the string, in bytes. Follows immediately after structure
   DWORD             dwDataSize;    // size of the data
   // BYTE[dwStringSize] - String follows at the end of the structure
   // BYTE[dwDataSize] - Data follows after the string
} CIRCUMREALITYPACKETSEVERCACHE, *PCIRCUMREALITYPACKETSEVERCACHE;


// CIRCUMREALITYPACKETCLIENTCACHE - Used by CIRCUMREALITYPACKET_CLIENTCACHETTS, etc.
typedef struct {
   DWORD             dwQuality;     // quality level
   DWORD             dwStringSize;  // size of the string, in bytes. Follows immediately after structure
   DWORD             dwDataSize;    // size of the data. If can't find the data, this is 0.
   // BYTE[dwStringSize] - String follows at the end of the structure
   // BYTE[dwDataSize] - Data follows after the string
} CIRCUMREALITYPACKETCLIENTCACHE, *PCIRCUMREALITYPACKETCLIENTCACHE;


// CIRCUMREALITYSERVERLOAD - Sent in CIRCUMREALITYPACKET_SERVERLOAD message
typedef struct {
   DWORD             dwConnections;    // number of users currently on
   DWORD             dwMaxConnections; // maximum number of connections allowed
} CIRCUMREALITYSERVERLOAD, *PCIRCUMREALITYSERVERLOAD;

// CIRCUMREALITYPACKETINFO - Information about the packet
typedef struct {
   // NOTE: There are assumiption in CCircumrealityLSocketServer that all string buffers are the same length
   WCHAR             szIP[64];      // IP address, or other location info, initially set by packet
   WCHAR             szStatus[64];  // status, such as "connecting"
   WCHAR             szUser[64];    // user name
   WCHAR             szCharacter[64]; // character name
   WCHAR             szUniqueID[64]; // unique ID
   GUID              gObject;       // MIFL object that assocaited with

   // the following can be read but should generally NOT be changed
   __int64           iSendBytes;    // total number of bytes sent
   __int64           iSendBytesExpect;  // number of bytes that expect to send
   __int64           iReceiveBytes; // total number of bytes received
   __int64           iReceiveBytesExpect;  // number of bytes that expect to receieve
   __int64           iConnectTime;  // time (in milliseconds since PC started) that connected
   __int64           iSendLast;     // last time (in milliseconds since PC started) that sent data
   __int64           iReceiveLast;  // last time (in milliseconds since PC started) that received data
   DWORD             dwBytesPerSec; // average bytes per second received
   __int64           iSendBytesComp; // compressed bytes sent
   __int64           iReceiveBytesComp;   // compressed bytes received
} CIRCUMREALITYPACKETINFO, *PCIRCUMREALITYPACKETINFO;

// CIRCUMREALITYCHANHEADER - Header to send/receive a channel
typedef struct {
   WORD              wChannel;      // channel number
   WORD              wSize;         // size of this block of data (excluding the header)
   DWORD             dwLeftInQueue; // amount of data left in the queue (for all channels)
} CIRCUMREALITYCHANHEADER, *PCIRCUMREALITYCHANHEADER;

#define NUMPACKCHAN           4     // number of packet channels

#define PACKCHAN_HIGH         0     // high prioroty messages
#define PACKCHAN_LOW          1     // low priority messages
#define PACKCHAN_TTS          2     // TTS audio
#define PACKCHAN_RENDER       3     // Rendered images

class DLLEXPORT CCircumrealityPacket {
public:
   ESCNEWDELETE;

   // functions that are NOT thread safe, and called from creator thread
   CCircumrealityPacket (void);
   ~CCircumrealityPacket (void);
   BOOL Init (SOCKET iSocket, char *pszNameThis, char *pszNameServer = NULL);
   int MaintinenceSendIfCan (__int64 iTimeMilli);
   int MaintinenceReceiveIfCan (__int64 iTimeMilli, BOOL *pfIncoming = NULL);
   BOOL MaintinenceCheckForSilence (__int64 iTimeMilli);

   // for accessing interal window info. Not thread safe
   HWND WindowGet (void);
   void WindowSentToSet (HWND hWnd);


   // thread safe functions
   BOOL PacketSend (DWORD dwType, PVOID pData, DWORD dwSize, PVOID pExtra = NULL, DWORD dwExtraSize = 0);
   BOOL PacketSend (DWORD dwType, PCMMLNode2 pNode, PVOID pData = NULL, DWORD dwSize = 0);
   DWORD PacketGetType (DWORD dwIndex = 0);
   DWORD PacketGetFind (DWORD dwType);
   PCMem PacketGetMem (DWORD dwIndex = 0);
   PCMMLNode2 PacketGetMML (DWORD dwIndex = 0, PCMem pMem = NULL);
   int GetLastError (PCWSTR *ppszErr);
   int GetFirstError (PCWSTR *ppszErr);
   void GetSendReceive (__int64 *piSent = NULL, __int64 *piSentExpect = NULL,__int64 *piSendComp = NULL,
                                 __int64 *piReceived = NULL, __int64 *piReceivedExpect = NULL,__int64 *piReceivedComp = NULL,
                                 DWORD *pdwBytesPerSec = NULL);
   void InfoGet (PCIRCUMREALITYPACKETINFO pInfo);
   void InfoSet (PCIRCUMREALITYPACKETINFO pInfo);
   SOCKET SocketGet (void);
   __int64 QueuedToSend (DWORD dwChannel);

   // if seperate thread for compress
   int CompressedDataToWinsock (BOOL fInCritSec, DWORD *pdwRemaining);
   int CompCompressAndSend (BOOL fInCritSec);
   void CompressInSeparateThread (CRITICAL_SECTION *pCritSec,
                                                PCListFixed plPCCircumrealityPacket, PHANDLE paSignal);
   void AddToSeparateThreadQueue (void);

   __int64              m_iMaxQueuedReceive; // maximum amount of data that can be queued up to receive
                                             // used to prevent overloading of server

private:
   int CompSend (PVOID pData, DWORD dwSize, BOOL fTryCompress);
   int CompSendEndOfData (void);
   DWORD CompFindLongestMatch (PWSTR pszData, DWORD dwCount, DWORD *pdwMatched);
   DWORD CompBuffer (PWSTR pszComp, DWORD dwMaxCount, BOOL fTryCompress);
   int CompReceiveAndDecompress (void);
   DWORD CompDecompBuffer (PBYTE pbData, DWORD dwSize);
   int CompReceive (PVOID pData, DWORD dwSize);

   CCircumrealityWinSockData      m_WinSock;     // connector to winsock
   CRITICAL_SECTION     m_CritSec;     // critical section so safe to call from other threads
   CMem                 m_amemRecieve[NUMPACKCHAN];  // receive buffer
   int                  m_iErrLast;        // last error returned by winsock
   PCWSTR               m_pszErrLast;      // associated error description. MIght be NULL.
   int                  m_iErrFirst;        // last error returned by winsock
   PCWSTR               m_pszErrFirst;      // associated error description. MIght be NULL.

   // access only under critical section
   CListVariable        m_lReceivePacket; // list of data for received backets, data received
   CListFixed           m_lReceive;    // type associated with each receive packet. CIRCUMREALITYPACKETRECEIVE
   CListVariable        m_alSendPacket[NUMPACKCHAN]; // list of packets queued to send, data to send
   CListFixed           m_alSend[NUMPACKCHAN];   // type of packet for the send, CIRCUMREALITYPACKETRSEND
   CIRCUMREALITYPACKETINFO        m_PI;       // status info

   // channel info
   DWORD                m_dwReceiveChan;  // which channel is being received, -1 if receiving channel header
   DWORD                m_dwSendChan;     // which channel is being send, -1 if sending channel header
   WORD                 m_wReceiveChanLeft;// amount of data left in the receive channel
   WORD                 m_wSendChanLeft;  // amount of data left in the send channel
   CIRCUMREALITYCHANHEADER        m_ReceiveChanHeader; // channel hearder for the receive
   CIRCUMREALITYCHANHEADER        m_SendChanHeader; // channel hearder for sending

   // no longer needed because in m_PI
   //__int64              m_iReceiveBytes;  // number of bytes received
   //__int64              m_iReceiveBytesExpect;  // number of bytes that expect to receive
   //__int64              m_iSendBytes;  // number of bytes send
   //__int64              m_iSendBytesExpect;  // number of bytes that expect to send
   //DWORD                m_dwBytesPerSec;  // average number of bytes per second that coming in

   // keep track of incoming data rate
   __int64              m_iStartTime;  // time that stared receiving this packet
   __int64              m_iStartCount; // number of bytes when started receiving this packet

   // queue up data so don't sent lots of small packets
   __int64              m_iSendQueueTimeStart;  // time when queue was started. 0 if has been running
   __int64              m_iSendQueueTimeWait;   // time to wait and cache up data before start sending

   // compression
   CListFixed           m_lCompressOrNot; // This is relevant to m_amemDecomp[0]. It's a list of integers.
                                          // if positive then that many BYTEs will be compressed. If negative
                                          // then that many BYTEs uncompressed.
   CMem                 m_amemDecomp[2];  // non-compressed data, m_dwCurPosn = amt of data. [0]=out, [1]=in
   CMem                 m_amemComp[2];    // compressed data, m_dwCurPosn = amt of data, [0]=out, [1]=in
   CMem                 m_amemDict[2];    // compression dictionary
   DWORD                m_adwDictLoc[2];  // add-to dictioanry here, in number of WCHAR
   BOOL                 m_fEndOfOut;      // if TRUE not expecting any more output data
   WORD                 m_wCompBitHistory;  // bit indicating whether compressed last one or not
   WORD                 m_wCompTick;     // compression time tick

   // for compression in separate thread
   BOOL                 m_fCompInSeparateThread;   // set to TRUE if compress in separate thread
   CRITICAL_SECTION     *m_pCritSecSeparateThread; // critical section for the separate thread
   PCListFixed          m_plPCCircumrealityPacketSeparateThread;   // where to write packets for separate thread
   PHANDLE              m_paSignalSeparateThread;  // signals for separate thread
};
typedef CCircumrealityPacket *PCCircumrealityPacket;


/*********************************************************************************
ResImage.cpp */

/*************************************************************************************
CCircumrealityHotSpot */
#define  ESCN_IMAGEDRAGGING        (ESCM_NOTIFICATION+20)
// Send by the image to say that dragging is in the process
typedef struct {
   CEscControl    *pControl;
   RECT           rPos;          // current position. If click then left==right, top==bottom
} ESCNIMAGEDRAGGING, *PESCNIMAGEDRAGGING;

#define  ESCN_IMAGEDRAGGED         (ESCM_NOTIFICATION+21)
// Send by the image to say that dragging (or clicking) is finished
typedef ESCNIMAGEDRAGGING *PESCNIMAGEDRAGGED;
typedef ESCNIMAGEDRAGGING ESCNIMAGEDRAGGED;


class DLLEXPORT CCircumrealityHotSpot {
public:
   ESCNEWDELETE;

   CCircumrealityHotSpot (void);
   ~CCircumrealityHotSpot (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, LANGID lid);

   // member variables
   RECT           m_rPosn;             // position of the hot spot
   DWORD          m_dwCursor;          // cursor to use. 0 for arrow, 1 for eye, 2 for hand, 3 for mouth
                                       // 10 for menu
   PCMIFLVarString m_ps;               // string for the hot spot action. Must be freed
   LANGID         m_lid;               // language ID that the hot spot action command is sent
   CListVariable  m_lMenuShow;         // text shown for the menu
   CListVariable  m_lMenuExtraText;    // extra text for menu item
   CListVariable  m_lMenuDo;           // text command to act on for the menu

private:
};
typedef CCircumrealityHotSpot *PCCircumrealityHotSpot;

DLLEXPORT BOOL HotSpotSubstitution (PCEscPage pPage, PESCMSUBSTITUTION p,
                                    PCListFixed plCPCircumrealityHotSpot, BOOL fReadOnly);
DLLEXPORT BOOL HotSpotInitPage (PCEscPage pPage, PCListFixed plCPCircumrealityHotSpot, BOOL f360,
                                DWORD dwWidth, DWORD dwHeight);
DLLEXPORT BOOL HotSpotComboBoxSelChanged (PCEscPage pPage, PESCNCOMBOBOXSELCHANGE p,
                                          PCListFixed plPCCircumrealityHotSpot, BOOL *pfChanged);
DLLEXPORT BOOL HotSpotEditChanged (PCEscPage pPage, PESCNEDITCHANGE p,
                                          PCListFixed plPCCircumrealityHotSpot, BOOL *pfChanged);
DLLEXPORT BOOL HotSpotButtonPress (PCEscPage pPage, PESCNBUTTONPRESS p,
                                          PCListFixed plPCCircumrealityHotSpot, BOOL *pfChanged);
DLLEXPORT BOOL HotSpotImageDragged (PCEscPage pPage, PESCNIMAGEDRAGGED p,
                                    PCListFixed plPCCircumrealityHotSpot,
                                    DWORD dwWidth, DWORD dwHeight, BOOL *pfChanged);
DLLEXPORT void MMLFromContextMenu (PCMMLNode2 pNode, PCListVariable plShow, PCListVariable plExtraText, PCListVariable plDo,
                         LANGID *plid, DWORD *pdwDefault);
DLLEXPORT void MMLToContextMenu (PCMMLNode2 pNode, PCListVariable plShow, PCListVariable plExtraText, PCListVariable plDo,
                       LANGID lid, DWORD dwDefault);

/*********************************************************************************
Megafile */
#define EFFN_NOTTS            0x0001      // dont extract any TTS from within this file

DLLEXPORT void ExtractFilesFromNode (PCMMLNode2 pNode, PCBTree pTree, BOOL fTryToOpen,
                                     DWORD dwFlags);
DLLEXPORT PCMMLNode2 CircumrealityParseMML (PWSTR psz);

/*************************************************************************************
CImageStore */
#define NUMISCACHE        3     // number of caches for 360 degree info
                                 // 0 is big image
                                 // 1 is small
                                 // 2 is for background

class CImageStore;
typedef CImageStore *PCImageStore;

// ISCACHE - Cached image information
typedef struct {
   PCImageStore         pis;     // cached image
   HBITMAP              hBmp;    // bitmap of cached image (if exists)
   int                  iWidth;  // width of the cached image
   int                  iHeight; // height of the cached image
   RECT                 rFrom;   // pixels where the cached image is from (in original image)
   fp                   fLong;   // used for 360 degree cache
   COLORREF             cBack;   // background color used
} ISCACHE, *PISCACHE;

class DLLEXPORT CImageStore : public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CImageStore (void);
   ~CImageStore (void);

   BOOL Init (DWORD dwWidth, DWORD dwHeight);
   BOOL Init (HBITMAP hBit, BOOL fDontAllowBlack);
   BOOL Init (PCImage pImage, BOOL fTransparentToBlack, BOOL fUpsample, BOOL f360);
   BOOL Init (PCFImage pImage, BOOL fTransparentToBlack, BOOL fUpsample, BOOL f360);
   BOOL Init (PCMegaFile pmf, PWSTR pszName);
   BOOL InitFromBinary (BOOL fJPEG, PBYTE pMem, __int64 iSize);
   BOOL Init (PWSTR pszFile, BOOL fDontAllowBlack);
   void Release(void);
   void ClearToColor (COLORREF cr);
   BOOL CloneTo (CImageStore *pTo);

   HBITMAP ToBitmap (HDC hDC);
   BOOL ToMegaFile (PCMegaFile pmf, PWSTR pszName);
   BOOL ToBinary (BOOL fJPEG, PCMem pMem);

   __inline PBYTE Pixel (DWORD dwX, DWORD dwY)
      {
         return ((PBYTE)m_memRGB.p) + ((dwX + dwY * m_dwWidth)*3);
      };

   BOOL MMLSet (PCMMLNode2 pNode);
   PCMMLNode2 MMLGet (void);
   DWORD Width (void);
   DWORD Height (void);

   BOOL Surround360Set (DWORD dwc, fp fFOV, fp fLat, DWORD dwWidth, DWORD dwHeight,
      fp fCurvature);
   void Surround360Get (DWORD dwc, fp *pfFOV, fp *pfLat, DWORD *pdwWidth, DWORD *pdwHeight,
      fp *pfCurvature);
   // HBITMAP Surround360ToBitmap (DWORD dwc, HDC hDC, fp fLong);
   void Surround360BitPixToOrig (DWORD dwc, int iX, int iY, fp fLong, DWORD *pdwX, DWORD *pdwY);

   BOOL CacheSurround360 (DWORD dwc, fp fLong);
   void CacheClear (DWORD dwItem = -1);
   BOOL CacheStretch (DWORD dwc, int iWidth, int iHeight,
                                RECT *prFrom, COLORREF cBack);
   BOOL CachePaint (BOOL fTransparent, HDC hDC, DWORD dwc, int iOffsetX, int iOffsetY,
                              int iWidth, int iHeight, RECT *prFrom,
                              fp fLong, COLORREF cBack);
   PCImageStore CacheGet (DWORD dwc, int iWidth, int iHeight, RECT *prFrom,
                              fp fLong, COLORREF cBack);

   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize,
      DWORD dwThread);

   // public varaibles
   DWORD             m_dwStretch;      // 0 for none, 1 for stretch to fit, 2 for scale to fit,
                                       // 3 for scale to cover, 4 for 360 degree
   DWORD             m_dwRefCount;     // reference count, if reaches 0 on a Release() call then deletes itelse
   BOOL              m_fTransient;     // flag used by calling app to mark if image is transient (aka: not yet completed)
                                       // defaults to FALSE

private:
   DWORD             m_dwWidth;        // width
   DWORD             m_dwHeight;       // height
   PCMMLNode2        m_pNode;          // node with misc information
   CMem              m_memRGB;         // mem with RGB bytes

   ISCACHE           m_aISCACHE[NUMISCACHE]; // cached images

   // for 360 render
   CMem              m_amem360[NUMISCACHE];         // array of m_dw360Half x dw360Height x 2-ints for pixel x delta, and pixel y
   fp                m_af360FOV[NUMISCACHE];        // field of view for 360
   fp                m_af360Lat[NUMISCACHE];        // latitude (in radians) that looking up
   fp                m_afCurvature[NUMISCACHE];     // curvature of camera. 0 = plane, 1 = circular
   DWORD             m_adw360Width[NUMISCACHE];     // width of 360 image
   DWORD             m_adw360Height[NUMISCACHE];    // height of 360 image
   DWORD             m_adw360Half[NUMISCACHE];      // half window (half od m_dw360Width)
};
typedef CImageStore *PCImageStore;


/*************************************************************************************
CTransition
*/
class DLLEXPORT CTransition {
public:
   ESCNEWDELETE;

   CTransition (void);
   ~CTransition (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CTransition *Clone (void);
   BOOL CloneTo (CTransition *pTo);
   void Clear (void);

   BOOL AnimateQuery (void);
   void AnimateInit (void);
   void AnimateAdvance (fp fTime);
   BOOL AnimateNeedBelow (void);
   BOOL AnimateFrame (DWORD dwc, fp fLong,
                        PCImageStore pisTop, RECT *prFromTop, COLORREF bkColor, DWORD dwNum,
                        PCImageStore *ppisImage, CTransition **ppTrans,
                        RECT *prFrom, PCImageStore pisDest);
   BOOL AnimateIsPassthrough (void);

   void HotSpotRemap (POINT *pPixel, DWORD dwWidth, DWORD dwHeight);

   fp Scale (void);

   BOOL Dialog (PCEscWindow pWindow, HBITMAP hBmp, fp fAspect,
      BOOL f360, BOOL fReadOnly, BOOL *pfChanged);

   // fade from color
   fp                m_fFadeFromDur;      // how many seconds it takes to fade from the color (<=0 => no fade to)
   COLORREF          m_cFadeFromColor;    // color fading from

   // fade to color
   fp                m_fFadeToStart;       // time at which starts fading to (if <= 0 then if no fade to)
   fp                m_fFadeToStop;      // time at which Stoped fading to
   COLORREF          m_cFadeToColor;      // color that fades to

   // fade in over existing image
   fp                m_fFadeInDur;        // numer of seconds it takes to fade in from existing image (<= 0 => no fade in)

   // transparent
   BOOL              m_fUseTransparent;   // set to TRUE if use transparency
   COLORREF          m_cTransparent;      // transparent color... so will leave background.
   DWORD             m_dwTransparentDist; // transparent distance. 0=> exact. Sum of 3 bytes is max

   // pan/zoom
   CPoint            m_pPanStart;         // starting point (at time 0). [0]=x from 0..1,[1]=y from 0..1
                                          // [2] = width comapred to max width, from 0..1
   CPoint            m_pPanStop;           // finishing pan. Same info as m_pPanStart
   fp                m_fPanStartTime;     // when pannining starts, in seconds. If <= 0 then no pan
   fp                m_fPanStopTime;       // when panning ends, in seconds

   // extra resolution
   int               m_iResExtra;         // if 0 no res extra, but 1 => sqrt(2) in each time, 2 => 2x in each dim, etc.

   // used for the dialog
   HBITMAP           m_hBmp;              // bitmap displayed
   int               m_iVScroll;          // vertial scroll information
   BOOL              *m_pfChanged;        // to set if changed this
   BOOL              m_fReadOnly;         // if TRUE then readonly
   fp                m_fAspect;           // expected aspect, width/height.
   DWORD             m_dwTab;             // which tab is on
   BOOL              m_f360;              // if 360 then no scrolling

private:
   BOOL AnimateQueryInternal (void);

   fp                m_fAnimateTime;      // current animation time
   BOOL              m_fAnimateQuery;     // response returned by animate query
};
typedef CTransition *PCTransition;




/*************************************************************************************
CRenderScene */

DLLEXPORT void MyCacheM3DFileEnd (DWORD dwRenderShard);
DLLEXPORT void MyCacheM3DFileInit (DWORD dwRenderShard);
void DLLEXPORT RenderSceneAspectToPixelsInt (int iAspect, fp fScale, DWORD *pdwWidth, DWORD *pdwHeight);

#define RSLIGHTS           4     // number of lights in renderscene
#define RSDEFAULTRES       1024   // default witdth in pixels of image
      // BUGFIX - Try reducing even further so faster for players
      // BUGFIX - Despite being fast, 512 way too low. Back to 1024, which
      // is more reasonable now since sped up rendering
//#define RSDEFAULTRES       726   // default witdth in pixels of image
// #define RSDEFAULTRES       1024   // default witdth in pixels of image
// BUGFIX - Was 1024, but too slow, so return to 1024/sqrt(2). Hopefully OK since smaller display
// BUGFIX - Nearly oubled the size so that at lowest resolution would still look ok
// #define RSDEFAULTRES       640   // default witdth in pixels of image

class CRenderScene360Callback {
public:
   virtual void RS360LongLatGet (fp *pfLong, fp *pfLat) = 0;
   virtual void RS360Update (PCImageStore pImage) = 0;
};
typedef CRenderScene360Callback *PCRenderScene360Callback;

class CRender360;
typedef CRender360 *PCRender360;

class DLLEXPORT CRenderScene : public CProgressSocket {
public:
   ESCNEWDELETE;

   CRenderScene (void);
   ~CRenderScene (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   PCImage Render (DWORD dwRenderShard, fp fScale, BOOL fFinalRender, BOOL fForceReload,
      BOOL fBlankIfFail, DWORD dwShadowsFlags, PCProgressSocket pProgress, BOOL fWantCImageStore,
      PCMegaFile pMegaFile, BOOL *pf360,
      PCRenderScene360Callback pCallback,
      PCMem pMem360Calc);
      //PCMegaFile pMegaFile = NULL, BOOL *pf360 = NULL,
      //PCRenderScene360Callback pCallback = NULL,
      //PCMem pMem360Calc = NULL);
   BOOL Edit (DWORD dwRenderShard, HWND hWnd, LANGID lid, BOOL fReadOnly, PCMIFLProj pProj, BOOL fTransition /*= TRUE*/);

   // from CProgressSocket
   virtual BOOL Push (float fMin, float fMax);
   virtual BOOL Pop (void);
   virtual int Update (float fProgress);
   virtual BOOL WantToCancel (void);
   virtual void CanRedraw (void);


   // properties
   BOOL                 m_fLoadFromFile;  // if TRUE then loads image from file. If
                                          // FALSE then creates several objects on the fly

   // properties specific to !m_fLoadFromFile
   CListFixed           m_lRSOBJECT;      // list of objects
   CPoint               m_apLightDir[RSLIGHTS]; // p[0] = azimuth, p[1] = altitude, p[2] = brightness (1.0 = sun)
   COLORREF             m_acLightColor[RSLIGHTS];  // color of the light
   COLORREF             m_acBackColor[2];   // background blend colors
   BOOL                 m_fBackBlendLR;       // set to TRUE if blend is left to right, FALSE if top to bottom
   WCHAR                m_szBackFile[256];  // file for background image
   CRenderScene         *m_pBackRend;        // if not NULL then use to render
   DWORD                m_dwBackMode;     // 0 for color blend, 1 for JPEG, 2 for image

   // properties to specific from m_fLoadFromFile
   WCHAR                m_szFile[256];    // filename
   GUID                 m_gScene;         // GUID for the current scene, or GUID_NULL if unknown
   fp                   m_fSceneTime;     // time (in seconds) to offset into the scene. Used unless can find bookmark
   WCHAR                m_szSceneBookmark[64];  // bookmark name
   GUID                 m_gCamera;        // if non-NULL then use this as the camera
   BOOL                 m_fCameraOverride;   // if TRUE then override the default camera with specific settings

   // properties
   int                  m_iAspect;       // aspect ratio. 0=2:1, 1=16:9, 2=3:2, 3=1:1,
                                          // 4=2:3, 5=9:16, 6=1:2, 10=360 degreee
                                          // Negative = aspect * 1000
   DWORD                m_dwShadowsFlags; // temporary used for when render
   fp                   m_fShadowsLimit;  // longest distance for light. If 0, slower shadows for longer.
                                          // defaults to 50. Internally modified based on texture quality
   DWORD                m_dwQuality;      // render quality, same as used in m3d exposures
   DWORD                m_dwAnti;         // antialiasing, 1+
   CListFixed           m_lEffect;        // list of effects, identified by 2 guids, first is code, second is sub
   CListFixed           m_lPCCircumrealityHotSpot;      // list of hotspots
   CListFixed           m_lRSATTRIB;      // attributes modified - for RS that opens file
   CListFixed           m_lRSCOLOR;       // colors modified

   CPoint               m_pCameraAutoHeight; // if p[3] is 1 then autoheight is used. Otherwise ignored.
                                          // p[0] is the floor. 0 = ground, -1 = basement, 1 = 1st floor
                                          // p[1] is height above floor to place camera
                                          // p[2] is height above water to place camera, only if water camera height higher than ground camera height
   fp                   m_fCameraHeightAdjust;  // increases the camera height by this much
   CPoint               m_pCameraXYZ;     // override XYZ location
   CPoint               m_pCameraRot;     // override rotation location
   fp                   m_fCameraFOV;     // override camera FOV, in radians
   fp                   m_fCameraExposure;   // override camera exposure
   CTransition          m_Transition;     // animation transition

   CPoint               m_pTime;          // time to set to the skydome. Values set to -1 if
                                          // don't bother to set. [0] = hours from 0.0 (midnight) to 0.5 (noon) to 1.0,
                                          // [1] = day of month (1 based), [2] = month of year (1 based)
                                          // [3] = year
   BOOL                 m_fSkyDel;        // if TRUE then delete the skydome if there is one
   CPoint               m_pClouds;        // clouds to use, [0] for cum 0..1, [1] = altocom, [2]= cirrius,
                                          // [3] = atmos thick
   CPoint               m_pLight;         // light source at the camera. [0] = red watts of incandescent
                                          // [1] = green, [2] = blue. (Thus, 60-watt white light = 60,60,60).
                                          // [3] = random XYZ variation in meters so light not
                                          // exactly at eye
   fp                   m_fLightsOn;      // number of meters around the camera to turn the lights on

   // internal. used for edit page
   BOOL                 m_fChanged;       // set to TRUE if changed while in Edit dialog
   HBITMAP              m_hBmp;           // current bitmap in edit dialog
   HBITMAP              m_hBitEffect;     // bitmap to show effect
   PCImage              m_pImage;         // current image in edit dialog
   BOOL                 m_fReadOnly;      // set from Edit call()
   LANGID               m_lid;            // language ID, from Edit call()
   PCMIFLProj           m_pProj;          // project it's in
   DWORD                m_dwTab;          // which tab is currently pressed down
   int                  m_iVScroll;       // for refresh
   fp                   m_fMoveDist;      // how much to move the camera every arrow click
   BOOL                 m_fTransition;    // allow editing of transition
   CListFixed           m_lBoneList;      // list of bones from the last render. Must free all these.
   DWORD                m_dwRenderShardTemp; // temporary render shard

private:
   void SetRenderCamera (DWORD dwRenderShard, PCRenderTraditional pRender);
   PCImage RenderApplyEffects (DWORD dwRenderShard, PCImage pImage, PCFImage pFImage, PCImageStore pIS,
                              BOOL fWantCImageStore,
                              BOOL fCanModifyImage, PCProgressSocket pProgress);

   PCRenderTraditional  m_pRender;        // render from MMLFileOpen()

   // used by RenderApplyEffects
   DWORD                m_dwRAEAnti;      // how much to antialias image
   BOOL                 m_fRAEDoEffect;   // if have effects to apply
   PCImageStore         m_pRAEIStore;     // image for the background
   BOOL                 m_fRAEFinalRender;   // set if this is the final render
   DWORD                m_dwRAERedrawNum; // keep track of number of times CanRedraw() is called
   BOOL                 m_fRAEIsPainterly;   // set to TRUE if there's a painterly effect taking place that can use lower res
   PCRenderSuper        m_pRAESuper;      // rendering engine
   PCWorldSocket        m_pRAEWorld;      // world
   PCRender360          m_pRAE360;        // 360 degree renderer, if using one
   PCImage              m_pRAEImage;      // image being used
   PCFImage             m_pRAEFImage;     // image being used
   PCRenderScene360Callback m_pRAECallback;  // to notify of 360 degree updates
   PCProgressSocket     m_pProgress;      // progress to call
};
typedef CRenderScene *PCRenderScene;

DLLEXPORT void RenderSceneHotSpotFromImage (RECT *pr, DWORD dwWidth, DWORD dwHeight);
DLLEXPORT void RenderSceneHotSpotToImage (RECT *pr, DWORD dwWidth, DWORD dwHeight);
DLLEXPORT COLORREF HotSpotColor (DWORD dwIndex);
DLLEXPORT BOOL OpenImageDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);

void ImageBlendImageStore (PCImage pImage, PCImageStore pStore, BOOL f360, BOOL fNonBlackToObject);
void ImageBlendedBack (PCImage pImage, COLORREF cBack0, COLORREF cBack1, BOOL fBackBlendLR, BOOL fNonBlackToObject);
DLLEXPORT PWSTR RenderSceneTabs (DWORD dwTab, DWORD dwNum, PWSTR *ppsz, PWSTR *ppszHelp, DWORD *padwID,
                       DWORD dwSkipNum, DWORD *padwSkip);



/*************************************************************************************
CRenderTitle */

class DLLEXPORT CRenderTitle {
public:
   ESCNEWDELETE;

   CRenderTitle (void);
   ~CRenderTitle (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   PCImage Render (DWORD dwRenderShard, fp fScale, BOOL fFinalRender, BOOL fForceReload,
      BOOL fBlankIfFail, DWORD dwShadowsFlags, PCProgressSocket pProgress,
      PCMegaFile pMegaFile, BOOL *pf360, PCMem pMem360Calc);
      //PCMegaFile pMegaFile = NULL, BOOL *pf360 = NULL, PCMem pMem360Calc = NULL);
   BOOL Edit (DWORD dwRenderShard, HWND hWnd, LANGID lid, BOOL fReadOnly, PCMIFLProj pProj);


   COLORREF             m_acBackColor[2];   // background blend colors
   BOOL                 m_fBackBlendLR;       // set to TRUE if blend is left to right, FALSE if top to bottom
   WCHAR                m_szBackFile[256];  // file for background image
   CRenderScene         *m_pBackRend;        // if not NULL then use to render
   DWORD                m_dwBackMode;     // 0 for color blend, 1 for JPEG, 2 for image

   // properties
   int                  m_iAspect;       // aspect ratio. 0=2:1, 1=16:9, 2=3:2, 3=1:1,
                                          // 4=2:3, 5=9:16, 6=1:2, 10=360 degreee
                                          // Negative = aspect * 1000
   CListFixed           m_lPCCircumrealityHotSpot;      // list of hotspots
   CListFixed           m_lTITLEITEM;     // list of titles

   CTransition          m_Transition;     // transition

   // internal. used for edit page
   BOOL                 m_fChanged;       // set to TRUE if changed while in Edit dialog
   HBITMAP              m_hBmp;           // current bitmap in edit dialog
   PCImage              m_pImage;         // current image in edit dialog
   BOOL                 m_fReadOnly;      // set from Edit call()
   LANGID               m_lid;            // language ID, from Edit call()
   PCMIFLProj           m_pProj;          // project it's in
   DWORD                m_dwTab;          // which tab is currently pressed down
   int                  m_iVScroll;       // for refresh
   DWORD                m_dwRenderShardTemp; // temp render shard
};
typedef CRenderTitle *PCRenderTitle;



/**********************************************************************************
Misc */
DLLEXPORT DWORD IsWAVResource (PWSTR psz);
DLLEXPORT PWSTR CircumrealityTitle (void);
DLLEXPORT PWSTR CircumrealityText (void);
DLLEXPORT PWSTR CircumrealityLotsOfText (void);
DLLEXPORT PWSTR Circumreality3DScene (void);
DLLEXPORT PWSTR Circumreality3DObjects (void);
DLLEXPORT PWSTR CircumrealityImage (void);
DLLEXPORT PWSTR CircumrealityPosition360 (void);
DLLEXPORT PWSTR CircumrealityFOVRange360 (void);
DLLEXPORT PWSTR CircumrealityThreeDSound (void);
DLLEXPORT PWSTR CircumrealityWave (void);
DLLEXPORT PWSTR CircumrealityCutScene (void);
DLLEXPORT PWSTR CircumrealitySpeakScript (void);
DLLEXPORT PWSTR CircumrealityConvScript (void);
DLLEXPORT PWSTR CircumrealitySliders (void);
DLLEXPORT PWSTR CircumrealityHypnoEffect (void);
DLLEXPORT PWSTR CircumrealityPreRender (void);
DLLEXPORT PWSTR CircumrealityMusic (void);
DLLEXPORT PWSTR CircumrealityTitleInfo (void);
DLLEXPORT PWSTR CircumrealityVoice (void);
DLLEXPORT PWSTR CircumrealitySpeak (void);
DLLEXPORT PWSTR CircumrealitySilence (void);
DLLEXPORT PWSTR CircumrealityQueue (void);
DLLEXPORT PWSTR CircumrealityDelay (void);
DLLEXPORT PWSTR CircumrealityTransPros (void);
DLLEXPORT PWSTR CircumrealityIconWindow (void);
DLLEXPORT PWSTR CircumrealityObjectDisplay (void);
DLLEXPORT PWSTR CircumrealityIconWindowDeleteAll (void);
DLLEXPORT PWSTR CircumrealityDisplayWindow (void);
DLLEXPORT PWSTR CircumrealityNotInMain (void);
DLLEXPORT PWSTR CircumrealityDisplayWindowDeleteAll (void);
DLLEXPORT PWSTR CircumrealityCommandLine (void);
DLLEXPORT PWSTR CircumrealityGeneralMenu (void);
DLLEXPORT PWSTR CircumrealityVerbWindow (void);
DLLEXPORT PWSTR CircumrealityAutoMap (void);
DLLEXPORT PWSTR CircumrealityAutoMapShow (void);
DLLEXPORT PWSTR CircumrealityMapPointTo (void);
DLLEXPORT PWSTR CircumrealityNLPRuleSet (void);
DLLEXPORT PWSTR CircumrealityVerbChat (void);
DLLEXPORT PWSTR CircumrealityAmbient (void);
DLLEXPORT PWSTR CircumrealityAmbientLoopVar (void);
DLLEXPORT PWSTR CircumrealityAmbientSounds (void);
DLLEXPORT PWSTR CircumrealityTransition (void);
DLLEXPORT PWSTR CircumrealityAutoCommand (void);
DLLEXPORT PWSTR CircumrealityChangePassword (void);
DLLEXPORT PWSTR CircumrealityLogOff (void);
DLLEXPORT PWSTR CircumrealityPointOutWindow (void);
DLLEXPORT PWSTR CircumrealityTutorial (void);
DLLEXPORT PWSTR CircumrealitySwitchToTab (void);
DLLEXPORT PWSTR CircumrealityHelp (void);
DLLEXPORT PWSTR CircumrealityTranscriptMML (void);
DLLEXPORT PWSTR CircumrealityTextBackground (void);
DLLEXPORT PWSTR CircumrealityUploadImageLimits (void);
DLLEXPORT PWSTR CircumrealityBinaryDataRefresh (void);
DLLEXPORT PWSTR CircumrealityVoiceChat (void);
DLLEXPORT PWSTR CircumrealityVoiceChatInfo (void);
DLLEXPORT PWSTR CircumrealityPieChart (void);

DLLEXPORT PWSTR ResHelpStringPrefix (BOOL fLightBackground);
DLLEXPORT PWSTR ResHelpStringSuffix (void);

/**********************************************************************************
ControlImageDrag */


// CONTROLIMAGEDRAGRECT - Store information about rectangles to draw
typedef struct {
   RECT           rPos;          // position in rectangle, in coords of bitmap
   COLORREF       cColor;        // color to draw in
   BOOL           fModulo;       // if TRUE then if the rectangle goes off one
                                 // edge will be drawn onto the other
} CONTROLIMAGEDRAGRECT, *PCONTROLIMAGEDRAGRECT;

#define  ESCM_IMAGERECTSET        (ESCM_CONTROLBASE+40)
// Sets the hot-spot rectangles for the control
typedef struct {
   PCONTROLIMAGEDRAGRECT pRect;  // pointer to an array of rectangles to use
   DWORD          dwNum;         // number of rectangles to use
} ESCMIMAGERECTSET, *PESCMIMAGERECTSET;

#define  ESCM_IMAGERECTGET        (ESCM_CONTROLBASE+41)
// Gets the hot-spot rectangles for the control. pRect and dwNum are filled in
typedef ESCMIMAGERECTSET ESCMIMAGERECTGET;
typedef ESCMIMAGERECTSET *PESCMIMAGERECTGET;



DLLEXPORT BOOL ControlImageDrag (PCEscControl pControl, DWORD dwMessage, PVOID pParam);

/***********************************************************************************
CRender360
*/

#define MAX360PINGPONG              2     // max ping-pong renderers for 360 render

class DLLEXPORT CRender360 : public CRenderSuper, public CProgressSocket {
public:
   ESCNEWDELETE;

   CRender360 (void);
   ~CRender360 (void);

   BOOL Init (PCRenderSuper *papRender, PCRenderTraditional *papRenderTrad,
      PCRenderTraditional pRTShadow);

   // from  CRenderSuper
   virtual DWORD ImageQueryFloat (void);
   virtual BOOL CImageSet (PCImage pImage);
   virtual BOOL CFImageSet (PCFImage pImage);
   virtual void CWorldSet (PCWorldSocket pWorld);
   virtual void CameraFlat (PCPoint pCenter, fp fLongitude, fp fTiltX, fp fTiltY, fp fScale,
                                     fp fTransX, fp fTransY);
   virtual void CameraFlatGet (PCPoint pCenter, fp *pfLongitude, fp *pfTiltX, fp *pfTiltY, fp *pfScale, fp *pfTransX, fp *pfTransY = 0);
   virtual void CameraPerspWalkthrough (PCPoint pLookFrom, fp fLongitude = 0, fp fLatitude = 0, fp fTilt = 0, fp fFOV = PI / 4);
   virtual void CameraPerspWalkthroughGet (PCPoint pLookFrom, fp *pfLongitude, fp *pfLatitude, fp *pfTilt, fp *pfFOV);
   virtual void CameraPerspObject (PCPoint pTranslate, PCPoint pCenter, fp fRotateZ, fp fRotateX, fp fRotateY, fp fFOV); 
   virtual void CameraPerspObjectGet (PCPoint pTranslate, PCPoint pCenter, fp *pfRotateZ, fp *pfRotateX, fp *pfRotateY, fp *pfFOV);
   virtual DWORD CameraModelGet (void);
   virtual BOOL Render (DWORD dwWorldChanged, HANDLE hEventSafe, PCProgressSocket pProgress = NULL);
   virtual fp ExposureGet (void);
   virtual void ExposureSet (fp fExposure);
   virtual COLORREF BackgroundGet (void);
   virtual void BackgroundSet (COLORREF cBack);
   virtual DWORD AmbientExtraGet (void);
   virtual void AmbientExtraSet (DWORD dwAmbient);
   virtual DWORD RenderShowGet (void);
   virtual void RenderShowSet (DWORD dwShow);
   virtual fp PixelToZ (DWORD dwX, DWORD dwY);
   virtual void PixelToViewerSpace (fp dwX, fp dwY, fp fZ, PCPoint p);
   virtual void PixelToWorldSpace (fp dwX, fp dwY, fp fZ, PCPoint p);
   virtual BOOL WorldSpaceToPixel (const PCPoint pWorld, fp *pfX, fp *pfY, fp *pfZ = NULL);
   virtual void ShadowsFlagsSet (DWORD dwFlags);
   virtual DWORD ShadowsFlagsGet (void);


   // from CProgressSocket
   virtual BOOL Push (float fMin, float fMax);
   virtual BOOL Pop (void);
   virtual int Update (float fProgress);
   virtual BOOL WantToCancel (void);
   virtual void CanRedraw (void);

   // custom
   BOOL CImageStoreSet (PCImageStore pImage);
   DWORD PingPongThreadProc (DWORD dwPing, DWORD dwWorldChanged);

   // can change on the fly
   fp                   m_f360Long;    // longitude that camera pointing in radians, 0=N, PI/2=E, etc.
                                       // used to draw the part looked on first
   fp                   m_f360Lat;     // latitude that camera pointing in radians, 0=ahead, PI/2=up, etc
                                       // used to draw the part looked on first
   fp                   m_fShadowsLimit;     // if 0, shadows as far as need. Else, sun shadows limited to this (in meters).
                                       // non-infinite light sources are half or quarter, depending on the strength
   DWORD                m_dwShadowsFlags; // set of SF_XXX from CRenderTraditional::m_dwShadowsFlags
   BOOL                 m_fHalfDetail; // set if IsPainterly() so produces extra-low resolution

   // can set
   PCMem                m_pMem360Calc; // if you set this to your own CMem object before rendering, the
                                       // precalc info will be cached away and be faster.

private:
   PCRenderSuper        m_apRender[MAX360PINGPONG];     // real renderer
   PCRenderTraditional  m_apRenderTrad[MAX360PINGPONG]; // so can call CloneLightsTo()
   PCRenderTraditional  m_pRTShadow;   // from Init()
   PCProgressSocket     m_pProgress;   // progress passed into render
   CPoint               m_pCamera;     // where the camera is
   PCImage              m_pImage;      // integer image
   PCFImage             m_pFImage;     // floating point image
   PCImageStore         m_pIS;         // image store - alternative renering
   CMem                 m_mem360Calc;  // m_pMem360Calc defaults to this

   // for multhreaded
   BOOL                 m_fWantToCancel;  // threads will check this to see if should cancel while rendering
   BOOL                 m_f360ThreadsShutDown;  // set to TRUE when want threads to shut down
   BOOL                 m_af360PingPongRet[MAX360PINGPONG]; // pingpong ret
   HANDLE               m_ahEventToThread[MAX360PINGPONG];   // hevent signal to thead that should check message bank
   HANDLE               m_ahEventPingPongSafe[MAX360PINGPONG]; // events to indicate that safe to go to next render
   HANDLE               m_ahEventFromThread[MAX360PINGPONG];   // event from thread indicating that done rendering
   DWORD                m_adwPingPongProgress[MAX360PINGPONG]; // 0 = thread is waiting
                                                               // 1 = thread is rendering AND not safe to procede,
                                                               // 2 = thread is rendering BUT safeto procede
                                                               // 3 = renderd but NOT safe to proceded
                                                               // 4 = got safe to procede and rendered
};
typedef CRender360 *PCRender360;

/*************************************************************************************
CResVerb */

// CResVerbIcon - Information about a specific icon
class DLLEXPORT CResVerbIcon {
public:
   ESCNEWDELETE;

   CResVerbIcon (void);
   ~CResVerbIcon (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CResVerbIcon *Clone (void);

   DWORD IconNum (void);
   DWORD IconResourceID (void);
   HINSTANCE IconResourceInstance (void);

   // public variables
   DWORD             m_dwIcon;      // icon to use, 0+
   LANGID            m_lid;         // language ID to use
   CMem              m_memDo;       // string to send. Might have "<click>" in it to
                                    // require that the user clicks on an object
   CMem              m_memShow;     // string to show for tooltip. If blank then
                                    // will show m_memDo
   BOOL              m_fHasClick;  // when load from MML, if m_memDo has "<click>" in
                                    // it then this flag will be set to TRUE
   PCIconButton      m_pButton;     // not created by this, but will be freed by it

};
typedef CResVerbIcon *PCResVerbIcon;

class CResVerb;
typedef CResVerb *PCResVerb;
class DLLEXPORT CResVerb {
public:
   ESCNEWDELETE;

   CResVerb (void);
   ~CResVerb (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Edit (HWND hWnd, LANGID lid, BOOL fReadOnly, PCMIFLProj pMIFLProj,
      BOOL fClientUI = FALSE, PCResVerb pRevert = NULL);

   // public
   CListFixed        m_lPCResVerbIcon; // list of PCResVerbIcon for icons
   CMem              m_memVersion;     // version string.
   CPoint            m_pWindowLoc;     // window location, [0]=left,[1]=right,[2]=top,[3]=bottom, 0..1
   BOOL              m_fHidden;        // if TRUE then hidden from user by default
   BOOL              m_fDelete;        // if TRUE then should completely hide the verb window

   // used for the dialogs
   LANGID            m_lidDefault;     // default Language ID
   PCMIFLProj        m_pMIFLProj;      // project that using
   BOOL              m_fReadOnly;      // TRUE if readonly
   BOOL              m_fChanged;       // set to TRUE if changed
   DWORD             m_dwCurIcon;      // curent icon to use
   BOOL              m_fClientUI;      // set to TRUE if displaying the client UI
   PCResVerb         m_pRevert;        // what to revert to, or NULL
private:
};



/*************************************************************************************
CResVoiceChatInfo */

class CResVoiceChatInfo;
typedef CResVoiceChatInfo *PCResVoiceChatInfo;
class DLLEXPORT CResVoiceChatInfo {
public:
   ESCNEWDELETE;

   CResVoiceChatInfo (void);
   ~CResVoiceChatInfo (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Edit (HWND hWnd, BOOL fReadOnly);

   // public
   CListFixed        m_lWAVEBASECHOICE;   // list of choices
   DWORD             m_dwQuality;      // quality, VCH_ID_XXX
   BOOL              m_fAllowVoiceChat; // if TRUE then player can use voice chat

   // used for the dialogs
   BOOL              m_fReadOnly;      // TRUE if readonly
   BOOL              m_fChanged;       // set to TRUE if changed
private:
};

/*************************************************************************************
CAmbient */
DLLEXPORT BOOL OpenMusicDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);

class DLLEXPORT CAmbient {
public:
   ESCNEWDELETE;

   CAmbient (void);
   ~CAmbient (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Edit (HWND hWnd, BOOL fReadOnly);

   void TimeInit (void);
   void TimeElapsed (double fDelta, PCListFixed plEvents);
   DWORD LoopNum (void);
   BOOL LoopWhatsNext (DWORD dwNum, PCBTree ptAmbientLoopVar, PCListFixed plEvents);

   // public
   CMem           m_memName;        // name, WCHAR
   GUID           m_gID;            // Object ID this is associated with, or GUID_NULL if none
   CListFixed     m_lPCAmbientRandom; // list of random sound effects that will be played
   CListFixed     m_lPCAmbientLoop; // list of looped sound effects that will be played
   CPoint         m_pOffset;        // offset all 3D sounds by this distance

   // used for dialog
   BOOL           m_fChanged;       // so know if anything was changed in edit dialog
   BOOL           m_fReadOnly;      // so know if was read only
private:
};
typedef CAmbient *PCAmbient;


// CAmbientRandom - For random sound playback
class DLLEXPORT CAmbientRandom {
public:
   ESCNEWDELETE;

   CAmbientRandom (void);
   ~CAmbientRandom (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Edit (PCEscWindow pWindow, BOOL fReadOnly, BOOL *pfChanged);

   void TimeUpdate (void); // call this to initialize
   void TimeElapsed (PCAmbient pAmbient, double fDelta, GUID *pgID, PCListFixed plEvents);

   // public
   BOOL           m_f3D;            // if TRUE using 3D sound, else stereo
   CPoint         m_pMin;           // minimu values. [0]..[2] = loc, [3] = minimum dB
   CPoint         m_pMax;           // maximum values. [0]..[2] = loc, [3] = maximum dB, if p[3] == 0 then dont use 3d sound
   fp             m_fMinDist;       // minimum distance for 3d sounds
   CPoint         m_pVol;           // volumes. Used if not using 3D.
                                    // [0] = left min, [1] = left max, [2] = right min, [3] = right max
                                    // 1.0 = normal value volume
   CPoint         m_pTime;          // time randomness. [0] and [1] and range of tiem between
                                    // events. [2] is jitter
   CListVariable  m_lWave;          // list of wave file names, WCHAR
   CListVariable  m_lMusic;         // list of MIDI file names. WCAHR

   // used for edit dialog
   BOOL           m_fReadOnly;      // set to TRUE if read only
   BOOL           *m_pfChanged;     // modified if changed
private:
   // used while playing
   double         m_fTimeToNextSound; // timer (seconds) to the next sound
   double         m_fTimeToNextTimer; // timer (seconds) to the next timer

};
typedef CAmbientRandom *PCAmbientRandom;


// ALSBRANCH - Information for branching in state
typedef struct {
   DWORD                dwState;       // state number that branches to
   WCHAR                szVar[64];     // variable to check. If == "" then no tests
   int                  iCompare;      // 0 => must be equal, -1 => <=, -2 => <,
                                       // 1 => >=, 2 => >
   double               fValue;        // value to compare to
} ALSBRANCH, *PALSBRANCH;

// CAmbientLoopState - Information about a specific state in a loop
class DLLEXPORT CAmbientLoopState {
public:
   ESCNEWDELETE;

   CAmbientLoopState (void);
   ~CAmbientLoopState (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   void StateRemove (DWORD dwState);
   void StateSwap (DWORD dwStateA, DWORD dwStateB);

   CListVariable        m_lWaveMusic;  // list of wave or music to play (in sequenence), PWSTR
   CListFixed           m_lALSBRANCH;  // list of branches

private:
};
typedef CAmbientLoopState *PCAmbientLoopState;

// CAmbientLoop - For looped playback
class DLLEXPORT CAmbientLoop {
public:
   ESCNEWDELETE;

   CAmbientLoop (void);
   ~CAmbientLoop (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Edit (PCEscWindow pWindow, BOOL fReadOnly, BOOL *pfChanged);
   BOOL LoopWhatsNext (PCAmbient pAmbient, GUID *pgID, PCBTree ptAmbientLoopVar, PCListFixed plEvents);

   // public
   BOOL              m_f3D;                  // if TRUE use 3D volume
   fp                m_fVolL;                // left volume
   fp                m_fVolR;                // right volume
   fp                m_fOverlap;             // overlap in seconds
   CPoint            m_pVol3D;               // 3D volume
   CListFixed        m_lPCAmbientLoopState;  // list of states

   // playback
   DWORD             m_dwLastState;          // last state played, -1 = just started, -2 = came to end

   // used for edit dialog
   BOOL           m_fReadOnly;      // set to TRUE if read only
   BOOL           *m_pfChanged;     // modified if changed
   int            m_iVScroll;       // to scroll into right loc
private:
};
typedef CAmbientLoop *PCAmbientLoop;



/*************************************************************************************
CResTitleInfo
*/
class DLLEXPORT CResTitleInfoShard {
public:
   ESCNEWDELETE;

   CResTitleInfoShard (void);
   ~CResTitleInfoShard (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CResTitleInfoShard *Clone (void);

   // properties that are saved
   DWORD             m_dwPort;         // port to use
   CMem              m_memName;        // name
   CMem              m_memDescShort;   // short description
   CMem              m_memAddr;        // address, either domain name, IP address, or web page
   CMem              m_memParam;       // parameter to send to the server

private:
};
typedef CResTitleInfoShard *PCResTitleInfoShard;

class DLLEXPORT CResTitleInfo {
public:
   ESCNEWDELETE;

   CResTitleInfo (void);
   ~CResTitleInfo (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL Edit (HWND hWnd, LANGID lid, BOOL fReadOnly);
   CResTitleInfo *Clone (void);
   BOOL CloneTo (CResTitleInfo *pTo);

   // properties that are saved
   LANGID            m_lid;            // language to save as
   BOOL              m_fPlayOffline;   // TRUE if can be played offline
   CMem              m_memName;        // name
   CMem              m_memFileName;        // name
   CMem              m_memDescShort;   // short description
   CMem              m_memDescLong;    // long description
   // CMem              m_memEULA;        // EULA
   CMem              m_memWebSite;     // web site where info can be found
   CListFixed        m_lPCResTitleInfoShard; // list of shards
   DWORD             m_dwDelUnusedUsers;  // how many days after unusued users are deleted
   DFDATE            m_dwDateModified;    // last modified date
   CMem              m_memJPEGBack;    // memory for jpeg background

   // for the edit dialog
   int               m_iVScroll;          // vertial scroll information
   BOOL              *m_pfChanged;        // to set if changed this
   BOOL              m_fReadOnly;         // if TRUE then readonly
   DWORD             m_dwTab;             // which tab is on

private:
};
typedef CResTitleInfo *PCResTitleInfo;


// CResTitleInfoSet - Set of title info, allowing for different languages
class DLLEXPORT CResTitleInfoSet {
public:
   ESCNEWDELETE;

   CResTitleInfoSet (void);
   ~CResTitleInfoSet (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL FromResource (PCMIFLResource pRes);
   PCResTitleInfo Find (LANGID lid);
   CResTitleInfoSet *Clone (void);

   // properties
   CListFixed           m_lPCResTitleInfo;      // list of titles

private:
};
typedef CResTitleInfoSet *PCResTitleInfoSet;

DEFINE_GUID(GUID_MegaCircumreality, 
0xd73558c7, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0xec);

DEFINE_GUID(GUID_MegaCircumrealityLink, 
0x1735acc7, 0x7bab, 0x479c, 0xa4, 0xa, 0x2c, 0x9c, 0xcd, 0x2e, 0x2c, 0x18);

DEFINE_GUID(CLSID_MegaTitleInfo, 
0x986514f3, 0xcd44, 0xaf2c, 0xaf, 0x85, 0x12, 0xb2, 0xc6, 0xa8, 0xfb, 0xb8);

DLLEXPORT PCResTitleInfoSet PCResTitleInfoSetFromFile (PWSTR pszFile, BOOL *pfCircumreality = NULL);
DLLEXPORT DFDATE DFDATEToday (BOOL fLocalTime);



#endif // _MIF_H_