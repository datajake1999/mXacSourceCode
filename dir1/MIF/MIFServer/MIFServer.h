/*********************************************************************************
MIFServer.h - header specific to the server
*/

#ifndef _MIFSERVER_H_
#define _MIFSERVER_H_


/*************************************************************************************
CEmailThread */

class DLLEXPORT CEmailThread {
public:
   ESCNEWDELETE;

   CEmailThread (void);
   ~CEmailThread (void);

   BOOL Mail (PWSTR pszDomain, PWSTR pszSMTPServer, PWSTR pszEmailTo, PWSTR pszEmailFrom,
      PWSTR pszNameFrom, PWSTR pszSubject, PWSTR pszMessage,
      PWSTR pszAuthUser, PWSTR pszAuthPassword);

   void ThreadProc (void);

private:
   BOOL ReceiveLine (int iSocket, PCMem pMem);
   BOOL EncodeMessageText (PWSTR pszOrig, PCMem pMem);

   CMem                 m_memRecieve;              // memory to store receive in

   // thread protected
   HANDLE               m_hThread;                 // thread handle
   HANDLE               m_hSignalFromThread;       // set when the thread has information for main thread
   HANDLE               m_hSignalToThread;         // set when the thread should check its queue and shutdown
   BOOL                 m_fWantToQuit;             // set to TRUE when want to quit
   CRITICAL_SECTION     m_CritSec;                 // critical section to access the following
   CListVariable        m_lSMTPServer;             // list of servers
   CListVariable        m_lEmailTo;                // list of email to
   CListVariable        m_lEmailFrom;              // list of email from
   CListVariable        m_lNameFrom;               // list of names that Email from
   CListVariable        m_lMessage;                // list of messages
   CListVariable        m_lSubject;                // subject
   CListVariable        m_lDomain;                 // domain sending from
   CListVariable        m_lAuthUser;               // user for authorization
   CListVariable        m_lAuthPassword;           // password for authorization
};
typedef CEmailThread *PCEmailThread;


/*************************************************************************************
CMegaFileInThread */

// MFITQ - Store information about the queue
typedef struct {
   DWORD          dwAction;      // action. One of MFITQ_XXX
   PCMem          pMemName;      // file name
   PCMem          pMemData;      // data, used for binary saves
   PCMMLNode2     pNodeData;     // data, used for save PCMMLNode2
   BOOL           fCompressed;   // used for pNodeData to inidicate if should save compressed or not
   DWORD          dwFTValid;     // flags. 0x01 if created valid, 0x02 if modified valid, 0x04 if accessed valid
   FILETIME       ftCreated;     // created file time
   FILETIME       ftModified;    // modified file time
   FILETIME       ftAccessed;    // accessed file time
} MFITQ, *PMFITQ;

#define MFITQ_NONE            0     // no action
#define MFITQ_SAVE            1     // saving file
#define MFITQ_DELETE          2     // deleting file
#define MFITQ_CLEAR           3     // clearing entire megafile

class DLLEXPORT CMegaFileInThread {
public:
   ESCNEWDELETE;

   CMegaFileInThread (void);
   ~CMegaFileInThread (void);

   // initialization
   BOOL Init (PWSTR pszMegaFile, const GUID *pgIDSub, BOOL fCreateIfNotExist = TRUE, DWORD dwQueue = 1000);

   // access to files... these are all thread safe
   BOOL Exists (PWSTR pszFile, PMFFILEINFO pInfo = NULL);
   PVOID LoadBinary (PWSTR pszFile, __int64 *piSize);
   PCMMLNode2 LoadMML (PWSTR pszFile, BOOL fCompressed);
   BOOL SaveBinary (PWSTR pszFile, PCMem pMem,
      FILETIME *pftCreated = NULL, FILETIME *pftModified = NULL, FILETIME *pftAccessed = NULL);
   BOOL SaveMML (PWSTR pszFile, PCMMLNode2 pNode, BOOL fCompressed,
      FILETIME *pftCreated = NULL, FILETIME *pftModified = NULL, FILETIME *pftAccessed = NULL);
   BOOL Enum (PCListVariable plName, PCListFixed plMFFILEINFO, PWSTR pszPrefix = NULL);
   BOOL Delete (PWSTR pszFile);
   void LimitSize (__int64 iWant, DWORD dwTooYoung);
   BOOL Clear (void);
   DWORD Num (void);
   BOOL GetNum (DWORD dwIndex, PCMem pMem);

   void ThreadProc (void);

private:
   CMegaFile            m_MegaFile;                // megafile
   DWORD                m_dwQueue;                 // max number of entries in the queue
   HANDLE               m_hThread;                 // thread handle
   HANDLE               m_hSignalFromThread;       // set when the thread has information for main thread
   HANDLE               m_hSignalToThread;         // set when the thread should check its queue and shutdown

   CRITICAL_SECTION     m_CritSec;                 // critical section to access the following
   CListFixed           m_lMFITQ;                  // queue of items
   MFITQ                m_CIRCUMREALITYTQInProgress;         // queue being worked on
   BOOL                 m_fWantToQuit;             // set to TRUE when want to shut down
   BOOL                 m_fNotifyWhenDoneWithProgress;   // indicates main thread wants notification
                                                   // when the secondary thread is done with progress
};
typedef CMegaFileInThread *PCMegaFileInThread;


/*************************************************************************************
CDatabase */

// DBITEM - Information about a database itme
typedef struct {
   GUID           gID;        // object ID
   FILETIME       ftCreate;   // creation time
   FILETIME       ftModify;   // last modify time
   FILETIME       ftAccess;   // last access time
   PCListFixed    plCMIFLVar; // list of CMIFLVar for cached quick-access properties
   DWORD          dwCheckedOut;  // non-zero if checked out, DWORD is ID of app that checks out
   BOOL           fDirty;     // set to TRUE if element in pNode not saved to database
   PCMMLNode2      pNode;      // node (if cached)
} DBITEM, *PDBITEM;

// DCOQ - Informaton for ObjectQuery
typedef struct {
   FILETIME       ftCreate;   // creation time
   FILETIME       ftModify;   // last modify time
   FILETIME       ftAccess;   // last access time
   DWORD          dwCheckedOut;  // non-zero if checked out, DWORD is ID of app that checks out
} DCOQ, *PDCOQ;

DEFINE_GUID(GUID_DatabaseCat,
0x1fc440a4, 0xab4a, 0x1fb8, 0xae, 0x6d, 0x45, 0xab, 0x85, 0x71, 0x1c, 0x26);

DEFINE_GUID(GUID_DatabaseCompressed,
0xa2c440a4, 0xab4a, 0x1fb8, 0xae, 0x6d, 0x45, 0xab, 0x85, 0x71, 0x1c, 0x26);

class CDatabaseCat {
public:
   ESCNEWDELETE;

   CDatabaseCat (void);
   ~CDatabaseCat (void);

   BOOL Init (PWSTR pszName, PWSTR pszFile, PCMIFLVM pVM);

   BOOL SaveAll (void);
   BOOL SavePartial (DWORD dwNumber);

   BOOL ObjectAdd (PCMIFLVMObject pObj);
   BOOL ObjectSave (PCMIFLVMObject pObj);
   BOOL ObjectCheckIn (PCMIFLVMObject pObj, BOOL fNoSave, BOOL fDelete, PCMIFLVM pVM);
   BOOL ObjectCheckOut (GUID *pgID);
   BOOL ObjectDelete (GUID *pgID);
   BOOL ObjectDelete (PCMIFLVar pVar);
   int ObjectQueryCheckOut (GUID *pgID, BOOL fTestForOtherVM);
   BOOL ObjectQuery (GUID *pgID, PDCOQ pdcoq);
   BOOL ObjectAttribGet (GUID *pgID, DWORD dwNum, PWSTR *ppszAttrib,
                                    PCMIFLVar paVar, BOOL *pfCheckedOut);
   BOOL ObjectAttribGet (PCMIFLVar pvObject, PCMIFLVar pvProp, PCMIFLVar pvResult,
      BOOL *pfCheckedOut);
   BOOL ObjectAttribSet (GUID *pgID, DWORD dwNum, PWSTR *ppszAttrib,
                                    PCMIFLVar paVar);
   BOOL ObjectAttribSet (PCMIFLVar pvObject, PCMIFLVar pvProp, PCMIFLVar pvResult);
   BOOL ObjectEnum (PCMIFLVar pvConst, WCHAR cPropDisambig, PCMIFLVar pvResult);
   BOOL ObjectEnumCheckOut (PCMIFLVar pvResult);
   DWORD ObjectNum (BOOL fCheckOutOnly);
   BOOL ObjectGet (DWORD dwIndex, BOOL fCheckOutOnly, GUID *pgID);

   BOOL CacheAdd (PWSTR pszProp);
   BOOL CacheRemove (PWSTR pszProp);
   BOOL CacheEnum (PCMIFLVar pVar);

   // public members
   WCHAR             m_szName[256];    // name

private:
   PCMMLNode2 MMLLoad (BOOL fCompressed, PWSTR pszName);
   BOOL MMLSave (BOOL fCompressed, PWSTR pszName, PCMMLNode2 pNode, FILETIME *pftCreate = NULL, FILETIME *pftModify = NULL,
      FILETIME *pftAccess = NULL);
   BOOL ElemCache (DWORD dwIndex);
   BOOL ElemUnCache (DWORD dwIndex);
   BOOL ElemDelete (DWORD dwIndex);
   BOOL ElemSyncFromObject (PCMIFLVMObject pObj, DWORD dwIndex);
   void ElemFlush (void);

   BOOL SaveElem (DWORD dwIndex);
   BOOL SaveQuick (void);
   PCMMLNode2 GetObjectMML (DWORD dwIndex, PCHashDWORD *pphString, PCHashDWORD *pphList);
   void FreeStringList (PCHashDWORD phString, PCHashDWORD phList);
   DWORD GetPropMML (PCMMLNode2 pObject, PWSTR pszProp);
   void EnumFindProps (PCMIFLVar pvConst, WCHAR cPropDisambig, PCHashString phProp);
   void EnumEval (PCMIFLVar pvConst, WCHAR cPropDisambig, PCHashString phProp, PCMIFLVar paVar,
                             PCMIFLVar pvResult);

   CMegaFileInThread  m_MegaFileInThread;       // file that using
   CHashGUID         m_hDBITEM;        // hash of DBITEM
   BOOL              m_fDirtyQuick;    // set to TRUE if the quick access is dirty
   CHashString       m_hQuick;         // hash of names for quick-access variables
   PCMIFLVM          m_pVM;            // virtual machine to use
   DWORD             m_dwCached;       // number of nodes cached
   DWORD             m_dwCheckedOut;   // number of nodes checked out
};
typedef CDatabaseCat *PCDatabaseCat;


class CDatabase {
public:
   ESCNEWDELETE;

   CDatabase (void);
   ~CDatabase (void);
   BOOL Init (PWSTR pszDir, PCMIFLVM pVM);

   PCDatabaseCat Get (PWSTR pszCategory);
   BOOL SaveAll (void);
   BOOL SavePartial (DWORD dwNumber);
   BOOL SaveBackup (PWSTR pszDir);
   BOOL ObjectDelete (GUID *pgID, BOOL fOnlyIfCheckedOut);
   PWSTR ObjectQueryCheckOut (GUID *pgID, BOOL fTestForOtherVM);

   WCHAR          m_szDir[256];     // directory that will store info in

private:
   PCMIFLVM       m_pVM;            // VM to use
   CHashString    m_hPCDatabaseCat; // list of databases, hashed by name
};
typedef CDatabase *PCDatabase;



/*************************************************************************************
CInstance */

DEFINE_GUID(GUID_InstanceFile,
0x2bc440a4, 0xab4a, 0x1fb8, 0xfc, 0x6d, 0x45, 0xab, 0x85, 0x71, 0x1c, 0x26);

// CInstanceFile.cpp - Store instance inforamtion for a file
class CInstanceFile {
public:
   ESCNEWDELETE;

   CInstanceFile (void);
   ~CInstanceFile (void);

   BOOL Init (PWSTR pszName, PWSTR pszFile);
   BOOL Save (PCMIFLVM pVM, PWSTR pszSubFile, BOOL fSaveAll, BOOL fChildren,
                          GUID *pagList, DWORD dwNum);
   BOOL Load (PCMIFLVM pVM, PWSTR pszSubFile, BOOL fRemap, PCHashGUID *pphRemap);
   DWORD Num (void);
   BOOL GetNum (DWORD dwIndex, PCMem pMem);
   BOOL Enum (PCListVariable plSubFiles, PCListFixed plMFFILEINFO = NULL);
   BOOL Delete (PWSTR pszSubFile);
   BOOL Exists (PWSTR pszFile, PMFFILEINFO pInfo = NULL);

   WCHAR                m_szName[256];    // name

private:
   CMegaFileInThread    m_MegaFileInThread;       // file that using
};
typedef CInstanceFile *PCInstanceFile;


// CInstance - Maintains a list of instances saved away
class CInstance {
public:
   ESCNEWDELETE;

   CInstance (void);
   ~CInstance (void);

   BOOL Init (PWSTR pszPath);
   DWORD Num (void);
   BOOL GetNum (DWORD dwIndex, PCMem pMem);
   BOOL Enum (PCListVariable plFiles);
   BOOL Delete (PWSTR pszFile);
   PCInstanceFile InstanceFileGet (PWSTR pszFile, BOOL fCreateIfNotExist = TRUE);
   PCInstanceFile InstanceFileGet (PCMIFLVM pVM, PCMIFLVar pvFile, BOOL fCreateIfNotExist = TRUE);

private:
   void LimitCacheSize (void);

   WCHAR                m_szPath[256];    // path where all the instance are written, excluding final '\'
   CHashString          m_hFilesInDir;    // tree of all files known to be in the directory. No data associated
   CHashString          m_hCached;        // tree of all files cached. Data is a PCInstanceFile.
};
typedef CInstance *PCInstance;


/*************************************************************************************
CInternetThread */

typedef struct {
   BYTE           ab[32];     // large memory
} IP6ADDRHACK, *PIP6ADDRHACK;

// IPBLACKLIST - Keep track of IP addresses that aren't allowed to connect at all
typedef struct {
#ifdef USEIPV6
   IP6ADDRHACK       iAddr;   // internet IP address, large enough
#else
   unsigned long iAddr;    // internet IP address
#endif
   fp          fScore;     // score. At 1.0 or higher doesn't allow on
   __int64     iFileTime;  // file time, after which point blacklist automatically expired
} IPBLACKLIST, *PIPBLACKLIST;

class CInternetThread {
public:
   ESCNEWDELETE;

   CInternetThread (void);
   ~CInternetThread (void);

   // called from the main thread
   BOOL Connect (PWSTR pszFile, BOOL fRemote, DWORD dwPort, HWND hWndNotify, DWORD dwMessage,
      int *piWinSockErr);
   BOOL Disconnect (void);

   // thread safe
   DWORD ConnectionCreatedGet (void);
   DWORD ConnectionReceivedGet (void);
   DWORD ConnectionErrGet (int *piErr, PWSTR pszErr);
   BOOL ConnectionRemove (DWORD dwID);
   PCCircumrealityPacket ConnectionGet (DWORD dwID);
   void ConnectionGetRelease (void);
   void ConnectionEnum (PCListFixed plEnum);
   void PerformanceNetwork (__int64 *piSent, __int64 *piReceived);
   BOOL IPBlacklist (PWSTR pszIP, fp fpAmount, fp fDays);
   BOOL ConnectionsLimit (DWORD dwNum);


   // called from the within the internet thread
   void ThreadProc (void);
   void CompressorThreadProc (DWORD dwThread);

   // can read
#ifdef USEIPV6
   IP6ADDRHACK             m_qwConnectIP;    // IP address connected to, filled if connected to internet
#else
   DWORD             m_qwConnectIP;    // IP address connected to, filled if connected to internet
#endif

   // semiprivate... accessed from within internet thread
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   HWND              m_hWnd;           // window handle for the main window

private:
   void WaitUntilSafeToDeletePacket (PCCircumrealityPacket pPacket);

   CRITICAL_SECTION  m_CritSec;        // criitcal section for accessing list of notification windows
   CHashDWORD        m_hCIRCUMREALITYCONNECT;   // hash of connected sockets
   CListFixed        m_lConnectNew;    // list of newly created connection IDs
   CListFixed        m_lConnectMessage;   // list of connections that contain message
   CListFixed        m_lConnectErr;    // list of connections that have returned an error => disconnected
   DWORD             m_dwCurSocketID;  // current socket ID to use
   CListFixed        m_lConnectRemove; // CONREMOVE struct, list of connection IDs to remove in internet thread
   BOOL              m_dwConnectCanDelete; // set to 0 if actually can delete a connection, 1+ => cant delete

   HANDLE            m_hThread;        // thread handle
   HANDLE            m_hSignalFromThread; // set when the thread has information
   HANDLE            m_hSignalToThread;   // set when the thread should shut down
   BOOL              m_fConnected;     // set to TRUE if connected, FALSE if not
   HWND              m_hWndNotify;     // window to notify if get incoming message
   DWORD             m_dwMessage;      // message to send to window if incoming message

   // connection
   DWORD             m_dwMaxConnections;  // maximum number of connections
   WCHAR             m_szConnectFile[256];  // Circumreality file name
   BOOL              m_fConnectRemote; // set to TRUE if remote connection
   DWORD             m_dwConnectPort;  // connection port (if remote)
   int               m_iWinSockErr;    // winsock err
   SOCKET            m_iSocket;        // current socket

   __int64           m_iNetworkSent;   // total number of bytes sent over the network
   __int64           m_iNetworkReceived;  // total number of bytes received from the network

   // compressor threads
   DWORD             m_dwCompNum;      // number of compressor threads, up to MAXRAYTHREAD
   CRITICAL_SECTION  m_CritSecComp;    // critical section for the compressors
   CListFixed        m_lPCCircumrealityPacketComp;   // list of packets that need servicing
   PCCircumrealityPacket       m_aPCCircumrealityPacketComp[MAXRAYTHREAD]; // list of packets that are currently being compressed
   HANDLE            m_ahSignalToThreadComp[MAXRAYTHREAD]; // signal to compressor threads that want them to wake up
   HANDLE            m_ahSignalFromThreadComp[MAXRAYTHREAD]; // signal from compressor threads that have information
   BOOL              m_afNotifyWhenDoneComp[MAXRAYTHREAD]; // set to TRUE when want compressor thread to nofigy
   BOOL              m_fWantToQuitComp;   // set to TRUE when want the thread to quit compressing
   HANDLE            m_ahThreadComp[MAXRAYTHREAD];      // compressor thread

   // blacklist IP's
   CRITICAL_SECTION  m_CritSecIPBLACKLIST;   // critical seciton for the IP blacklist
   CListFixed        m_lIPBLACKLIST;      // list of blacklisted IPs and their scored
};
typedef CInternetThread *PCInternetThread;




/*************************************************************************************
CTextLogs */

// LOGLINE - Store information about a line of log entry
typedef struct {
   __int64        iOffset;    // byte offset into the file
   DWORD          dwOffsetData;   // where the actual data starts, based off iOffset
   DWORD          dwDate;     // year in high word, then month, and day in low byte
   DWORD          dwTime;     // hour in high byte, then minute, then second, then 100th of a second
   GUID           gUser;      // user GUID, or NULL if none
   GUID           gActor;     // actor/character guid or null
   GUID           gRoom;      // room guid or null
   GUID           gObject;    // object maniuplated guid, or null
} LOGLINE, *PLOGLINE;

// LOGQUEUE - queued up information
typedef struct {
   DWORD          dwAction;   // action to queue. 0 = write operation, 1 = read, 2 = search, 3 = num lines

   // used mainly by write operation
   DWORD          dwDate;
   DWORD          dwTime;
   GUID           gActor;
   GUID           gObject;
   GUID           gRoom;
   GUID           gUser;
   PCMem          pMem;       // memory that stores important info

   // used for reading a line
   DWORD          dwLine;     // line to read
   PLOGLINE       pLOGLINE;   // filled with the log-line information. pLOGLINE.iOffset = -1 if error
   // PCMem       pMem        // filled with the string
   HANDLE         hSignalFromThread;   // call this signal to indicate that finished

   // used for numlines
   DWORD          *pdwNumLines;  // DWORD pointed to is filled in with the number of lines
   // HANDLE      hSignalFromThread;   // signal when done

   // used for search
   // DWORD       dwDate, dwTime // used to indicate start date time
   DWORD          dwDateEnd;     // end date
   DWORD          dwTimeEnd;     // end time
   // GUID        gactor, gObject, gRoom, gUser - Used to indicate room
   // PCMem       pMem;          // used to indicate memory string to match
   PCListFixed    plLOGFILESEARCH;  // search entries that match are appeneded to this
   HWND           hWndNotify;    // window to post to
   UINT           uMsg;          // message to post
   PVOID          pUserData;     // user data
   // DWORD       dwLine - Line to start at. Should always be 0 to begin with

} LOGQUEUE, *PLOGQUEUE;

// LOGFILESEARCH - Search results
typedef struct {
   DWORD          dwIDDate;      // date and time that uniquely identifies file
   DWORD          dwIDTime;      // see above
   DWORD          dwLine;        // line number where found something
} LOGFILESEARCH, *PLOGFILESEARCH;



class CTextLogFile {
public:
   ESCNEWDELETE;

   CTextLogFile (void);
   ~CTextLogFile (void);
   BOOL Init (PWSTR pszFile, DWORD dwIDDate, DWORD dwIDTime);

   BOOL Write (DWORD dwDate, DWORD dwTime, PWSTR psz, GUID *pgActor,
                          GUID *pgObject, GUID *pgRoom, GUID *pgUser);
   BOOL Read (DWORD dwLine, DWORD *pdwDate, DWORD *pdwTime, PCMem pMemText, GUID *pgActor,
                          GUID *pgObject, GUID *pgRoom, GUID *pgUser);
   DWORD NumLines (void);
   BOOL Search (DWORD dwDateStart, DWORD dwTimeStart, DWORD dwDateEnd, DWORD dwTimeEnd,
                           GUID *pgActor, GUID *pgObject, GUID *pgRoom, GUID *pgUser,
                           PWSTR pszString, PCListFixed plLOGFILESEARCH,
                           HWND hWndNotify, UINT uMsg, PVOID pUserData);
   BOOL IsBusy (void);

   // don't call, used internally
   void ThreadProc (void);

   // some info that might update from time to time
   FILETIME          m_ftLastUsed;  // last used time, for clearing out unused logs, NOT updated internally

   // can look but dont touch
   DWORD             m_dwIDDate;    // unique identifier for date
   DWORD             m_dwIDTime;    // unique identifier for time

private:
   void DeleteQueue (PLOGQUEUE plq);
   size_t InternalFRead (__int64 iLoc, size_t dwSize, PVOID pMem);
   size_t InternalFWrite (size_t dwSize, PVOID pMem);
   size_t InternalFWrite (PWSTR psz);
   PVOID MemoryFromFile (__int64 iLoc, size_t dwSize, size_t *pdwAvail = NULL);
   __int64 ReadLineHeader (__int64 iLoc);
   __int64 FindNextLineStart (__int64 iLoc);

   HANDLE            m_hThread;     // thread
   CRITICAL_SECTION  m_CritSec;     // critical section for accessing this data
   HANDLE            m_hSignalToThread;  // signal to wake thread up
   HANDLE            m_hSignalFromThread;  // signal from thread
   BOOL              m_fWantToQuit; // set to TRUE when want to quit
   BOOL              m_fWantSignal; // the write-data proc will set this if it wants a signal when written

   BOOL              m_fReadOnly;   // set to TRUE if file is read only
   FILE*             m_File;        // file
   CListFixed        m_lLOGLINE;    // info on each line
   __int64           m_iSize;       // size of the file in bytes
   __int64           m_iLoc;        // location where pointer is at
   BOOL              m_fWorking;    // set to TRUE if working on an item

   CListFixed        m_alLOGQUEUE[2];  // log queues. [0] is primary and done first. [1] is done with [0] is empty
   CMem              m_memScratch;  // scratch memory

   CMem              m_memCache;    // cache of the file
   size_t             m_dwCacheValid; // number of bytes in cache that are valid
   __int64           m_iCacheLoc;   // where cache was read from
   BOOL              m_fNeedToFlush;   // set to TRUE when should do fflush()
   BOOL              m_fJustRead;   // set to TRUE when just read
   BOOL              m_fJustWrote;  // set to TRUE when just wrote

};
typedef CTextLogFile *PCTextLogFile;


// TEXTLOGFILE
typedef struct {
   DWORD             dwIDDate;         // ID date
   DWORD             dwIDTime;         // ID time... just the hour noted
   PCTextLogFile     pTLF;             // log file, could be NULL if not loaded
} TEXTLOGFILE, *PTEXTLOGFILE;

// TEXTLOGSEARCH - internal storage for search info
typedef struct {
   DWORD          dwDateLast;       // last date that searched through
   DWORD          dwTimeLast;       // last time that searched through
   DWORD          dwDateStart;
   DWORD          dwTimeStart;
   DWORD          dwDateEnd;
   DWORD          dwTimeEnd;
   GUID           gActor;
   GUID           gObject;
   GUID           gRoom;
   GUID           gUser;
   PCMem          pMemString;       // string to search for
   PCListFixed    plLOGFILESEARCH;  // search results
   HWND           hWndNotify;       // notification window to use
   UINT           uMsg;             // message to send there
   PCMIFLVM       pVM;              // virual machine to call with final results
   PCMIFLVar      pCallback;        // callback var
   PCMIFLVar      pParam;           // parameter to pass
} TEXTLOGSEARCH, *PTEXTLOGSEARCH;

class CTextLog {
public:
   ESCNEWDELETE;

   CTextLog (void);
   ~CTextLog (void);

   BOOL Init (PWSTR pszDir);           // directory that log files are in
   BOOL Delete (DWORD dwIDDate, DWORD dwIDTime);
   BOOL Enum (PCListFixed plDATETIME);
   BOOL Log (PWSTR psz, GUID *pgActor = NULL, GUID *pgObject = NULL, GUID *pgRoom = NULL, GUID *pgUser = NULL);
   BOOL LogAuto (PWSTR psz, GUID *pgObject = NULL);
   BOOL AutoSet (DWORD dwType, GUID *pg);
   BOOL AutoGet (DWORD dwType, GUID *pg);
   void EnableSet (BOOL fEnable);
   BOOL EnableGet (void);

   BOOL Read (DWORD dwDate, DWORD dwTime, DWORD dwLine, DWORD *pdwDate, DWORD *pdwTime, PCMem pMemText, GUID *pgActor,
                          GUID *pgObject, GUID *pgRoom, GUID *pgUser);
   DWORD NumLines (DWORD dwDate, DWORD dwTime);
   BOOL Search (DWORD dwDateStart, DWORD dwTimeStart, DWORD dwDateEnd, DWORD dwTimeEnd,
                           GUID *pgActor, GUID *pgObject, GUID *pgRoom, GUID *pgUser,
                           PWSTR pszString,
                           HWND hWndNotify, UINT uMsg, PCMIFLVM pVM, PCMIFLVar pCallback,
                           PCMIFLVar pParam);

   void FILETIMEToDateTime (FILETIME *pft, DWORD *pdwDate, DWORD *pdwTime);
   void DateTimeToFILETIME (DWORD dwDate, DWORD dwTime, FILETIME *pft);
   BOOL IDStringToDateTime (PWSTR pszID, DWORD dwLen, DWORD *pdwDate, DWORD *pdwTime);
   void DateTimeToIDString (DWORD dwDate, DWORD dwTime, PWSTR pszID);

   void OnMessage (PTEXTLOGSEARCH pTLS, BOOL fInCritSec);

private:
   BOOL GenerateFileName (DWORD dwIDDate, DWORD dwIDTime, PCMem pMem);
   void UncacheOld (PFILETIME pft);

   CRITICAL_SECTION  m_CritSec;     // critical section for accessing this data
   CMem              m_memDir;         // directory string, unicode
   CListFixed        m_lTEXTLOGFILE;   // log file info
   CListVariable     m_lTEXTLOGSEARCH; // list of search info
   GUID              m_gAutoActor;     // actor to use for automatic
   GUID              m_gAutoRoom;      // room to use for automatic
   GUID              m_gAutoUser;      // user to use for automatic
   BOOL              m_fEnabled;       // must be TRUE for logging to be enabled
};
typedef CTextLog *PCTextLog;

/*************************************************************************************
CMIFNLP */

// CFGNODE - Node in a CFG
typedef struct {
   DWORD          dwToken;       // token needed to match node. Can be CFGNODETOK_XXX
   DWORD          dwNext;        // next token (index) in the sequence, -1 if none
   DWORD          dwAlt;         // alternative token (index) to this, -1 if none
} CFGNODE, *PCFGNODE;

#define CFGNODETOK_NOP           0xffffffff     // this node is the a N-OP token
#define CFGNODETOK_ONEWORD       0xfffffff0     // this node can be any one word, add index for wildcard bin number
#define CFGNODETOK_MANYWORDS     0xffffffe0     // this node can be one or more words, add index for wildcard bin number


// CFGPARSETEMP - Temporary structure used for CFGPARSE
typedef struct {
   WORD           awBinStart[10];  // the starting token index of the bin. If -1 then not filled
   WORD           awBinEnd[10];    // ending token index of bin.
} CFGPARSETEMP, *PCFGPARSETEMP;


// CCircumrealityCFG - Handles context free grammar parsing
class CCircumrealityCFG {
public:
   ESCNEWDELETE;

   CCircumrealityCFG (void);
   ~CCircumrealityCFG (void);
   BOOL Init (PCHashString phTokens);
   BOOL ExtractCFG (PWSTR psz, DWORD dwParseMode);
   void Parse (DWORD *padwTokens, DWORD dwNum, BOOL fMustUseAll, CCircumrealityCFG **ppCFGTemplate, DWORD dwNumTemplate,
                             PCListVariable plParsed);
   BOOL IsStartingToken (DWORD dwToken);
   BOOL HasStartingToken (DWORD *padwTokens, DWORD dwNum);

   BOOL CloneTo (CCircumrealityCFG *pTo);
   CCircumrealityCFG *Clone (void);

private:
   DWORD ExtractCFGFromSubString (PWSTR psz, DWORD dwCount, DWORD dwEndIndex, DWORD dwParseMode);
   void CalcStartTokens (DWORD dwIndex);
   void ParseInternal (DWORD *padwTokens, DWORD dwNum, BOOL fMustUseAll, CCircumrealityCFG **ppCFGTemplate, DWORD dwNumTemplate,
                             PCListVariable plParsed, PCMem pMem, PCFGPARSETEMP pBin, DWORD dwIndex,
                             DWORD dwCurToken);
   void UseAsTemplate (PCMem pMem, PCFGPARSETEMP pBin, DWORD *padwTokens, DWORD dwNum);

   CListFixed        m_lCFGNODE;    // node in a CFG
   CListFixed        m_lStartToken; // list of possible starting token IDs, from m_phTokens
   DWORD             m_dwNodeStart; // starting node
   PCHashString      m_phTokens;    // hash of strings for the token IDs
};
typedef CCircumrealityCFG *PCCircumrealityCFG;



// CCircumrealityNLPHyp - Hypothesis for NLP
class CCircumrealityNLPHyp {
public:
   ESCNEWDELETE;

   CCircumrealityNLPHyp (void);
   ~CCircumrealityNLPHyp (void);

   BOOL CloneTo (CCircumrealityNLPHyp *pTo);
   CCircumrealityNLPHyp *Clone (void);
   void NewID (void);

   CMem        m_memTokens;         // array of DWORD for token IDs from m_phTokens, or -1 if sentence split. m_dwCurPosn = size
   fp          m_fProb;             // probability score
   DWORD       m_dwRuleGroup;       // next rule group processed on this
   DWORD       m_dwRuleElem;        // next element in the rule group to be processed
   BOOL        m_fStable;           // set to TRUE if this one is stable and doesn't need to be reprocessed
   CListFixed  m_lParents;          // hypothesis ID of all parents used to create this
   DWORD       m_dwID;              // hypothesis ID of this one
   DWORD       *m_pdwIDCount;       // ID counter that gets incremented each time a new hypothesis
                                    // ID is set

private:
};
typedef CCircumrealityNLPHyp *PCCircumrealityNLPHyp;


// CCircumrealityNLPRule - Rule for NLP
class CCircumrealityNLPRule {
public:
   ESCNEWDELETE;

   CCircumrealityNLPRule (void);
   ~CCircumrealityNLPRule (void);
   BOOL Init (PCHashString phTokens);

   BOOL MMLTo (PCMMLNode2 pNode);
   BOOL MMLFrom (PCMMLNode2 pNode);
   BOOL CloneTo (CCircumrealityNLPRule *pTo);
   CCircumrealityNLPRule *Clone (void);

   BOOL CalcCFG (void);       // call after changing m_lCFGString or m_dwType
   BOOL Parse (PCCircumrealityNLPHyp pHypBase, DWORD *padwTokens, DWORD dwNum, BOOL fMultipleSent,
                         DWORD dwRuleGroup, DWORD dwRuleElem,
                         PCListFixed plPCCircumrealityNLPHyp, PCMem pMem);

   // public
   CListVariable     m_lCFGString;  // strings for the CFG, elem 0 = from, other elements are to
   DWORD             m_dwType;      // 0 = synonym, 1=reword (1-to-1 substitution), 2=reword and split.
                                    // 0 and 1 only have 2 m_lCFGString, while type 2 will have many
   fp                m_fProb;       // probability score, >= 0.0001, <= 0.99, if 0.5 then default

private:
   BOOL ParseSentence (PCCircumrealityNLPHyp pHypBase, DWORD *padwTokens, DWORD dwNum,
                                 DWORD *padwTokPre, DWORD dwNumPre, DWORD *padwTokPost, DWORD dwNumPost,
                                 DWORD dwRuleGroup, DWORD dwRuleElem,
                                 PCListFixed plPCCircumrealityNLPHyp, PCMem pMem);

   PCHashString      m_phTokens;    // hash of strings for the token IDs
   CListFixed        m_lPCCircumrealityCFG;   // list of CFGs. elem 0 = from, other elements are to
};
typedef CCircumrealityNLPRule *PCCircumrealityNLPRule;



// CIRCUMREALITYRSINFO - Rule set info
typedef struct {
   LANGID            lid;           // language that this is for
   PCListFixed       plPCCircumrealityNLPRule;// list of rules
} CIRCUMREALITYRSINFO, *PCIRCUMREALITYRSINFO;

// CCircumrealityNLPRuleSet - Set of rules
class CCircumrealityNLPRuleSet {
public:
   ESCNEWDELETE;

   CCircumrealityNLPRuleSet (void);
   ~CCircumrealityNLPRuleSet (void);
   BOOL Init (PCHashString phTokens);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, LANGID lid);
   BOOL CloneTo (CCircumrealityNLPRuleSet *pTo);
   CCircumrealityNLPRuleSet *Clone (void);

   BOOL LanguageSet (LANGID lid, DWORD dwExact);
   LANGID LanguageGet (void);

   DWORD RuleNum (void);
   PCCircumrealityNLPRule RuleGet (DWORD dwIndex);
   BOOL RuleRemove (DWORD dwIndex);
   DWORD RuleAdd (void);
   void RuleSort (DWORD dwSort);

   // cant be modified
   CMem              m_memName;     // WCHAR name string
   BOOL              m_fEnabled;    // set to TRUE if enabled

private:
   BOOL CIRCUMREALITYRSINFOClear (DWORD dwIndex);

   CListFixed        m_lCIRCUMREALITYRSINFO;  // list of riles
   PCIRCUMREALITYRSINFO        m_pCur;        // current language, etc.
   DWORD             m_dwCurIndex;  // index for the current language, -1 if none
   PCHashString      m_phTokens;    // hash of strings for the token IDs
};
typedef CCircumrealityNLPRuleSet *PCCircumrealityNLPRuleSet;


// CCircumrealityNLPParser - Collection of CCircumrealityNLPRuleSet
class CCircumrealityNLPParser {
public:
   ESCNEWDELETE;

   CCircumrealityNLPParser (void);
   ~CCircumrealityNLPParser (void);
   BOOL Init (PCHashString phTokens);

   BOOL CloneTo (CCircumrealityNLPParser *pTo);
   CCircumrealityNLPParser *Clone (void);

   BOOL LanguageSet (LANGID lid, DWORD dwExact);
   LANGID LanguageGet (void);

   DWORD RuleSetNum (void);
   PCCircumrealityNLPRuleSet RuleSetGet (DWORD dwIndex);
   BOOL RuleSetRemove (DWORD dwIndex);
   DWORD RuleSetFind (PWSTR pszName);
   DWORD RuleSetAdd (PWSTR pszName);

   void HypClear (void);
   DWORD HypNum (void);
   PCCircumrealityNLPHyp HypGet (DWORD dwIndex);
   PWSTR HypTokenGet (DWORD dwToken);
   void HypSort (DWORD dwSort);

   BOOL Parse (PWSTR pszText);

   CMem              m_memName;     // WCHAR name of the parser

private:
   BOOL WordBreak (PWSTR pszText);
   BOOL WordBreakAddToken (PWSTR pszToken, PCCircumrealityNLPHyp pHyp,
      BOOL fForceTempToken = FALSE);
   BOOL WordBreakNoQuotes (PWSTR pszText, PCCircumrealityNLPHyp pHyp);
   BOOL WordBreakNoNumbers (PWSTR pszText, PCCircumrealityNLPHyp pHyp);
   BOOL WordBreakNoPunct (PWSTR pszText, PCCircumrealityNLPHyp pHyp);
   BOOL WordBreakNoGUIDs (PWSTR pszText, PCCircumrealityNLPHyp pHyp);

   BOOL HypExpand (PCCircumrealityNLPHyp pHyp);
   BOOL HypExpandAll (void);

   CListFixed        m_lPCCircumrealityNLPRuleSet;  // list of rule sets
   CListFixed        m_lPCCircumrealityNLPHyp;      // hypothesis list
   PCHashString      m_phTokens;          // tokens to use for dictionary
   CHashString       m_hTokensTemp;       // temporary tokens used for a parse
   LANGID            m_lid;               // current language
   DWORD             m_dwExact;           // exact setting from LanguageSet()
   DWORD             m_dwIDHyp;           // hypothesis ID counter
};
typedef CCircumrealityNLPParser *PCCircumrealityNLPParser;



// CCircumrealityNLP - Object that stores all the parsers
class CCircumrealityNLP {
public:
   ESCNEWDELETE;

   CCircumrealityNLP (void);
   ~CCircumrealityNLP (void);

   DWORD ParserNum (void);
   PCCircumrealityNLPParser ParserGet (DWORD dwIndex);
   BOOL ParserRemove (DWORD dwIndex);
   DWORD ParserFind (PWSTR pszName);
   DWORD ParserAdd (PWSTR pszName);
   DWORD ParserClone (DWORD dwIndex);

private:
   CHashString       m_hTokens;           // tokens used for words
   CListFixed        m_lPCCircumrealityNLPParser;   // list of parsers
   DWORD             m_dwLastParser;      // last parser accessed, to optimize
};
typedef CCircumrealityNLP *PCCircumrealityNLP;



/*************************************************************************************
COnlineHelp */

// OHCATEGORY - For storing information about a category
typedef struct {
   PCListFixed       plPCResHelp;      // help topics at this level
   PCBTree           ptOHCATEGORY;     // tree of sub-entries
} OHCATEGORY, *POHCATEGORY;

class CResHelp;
typedef CResHelp *PCResHelp;

class COnlineHelp {
public:
   ESCNEWDELETE;

   COnlineHelp (void);
   ~COnlineHelp (void);

   BOOL Init (PCMIFLVM pVM, LANGID lid, PWSTR pszDir, HWND hWnd);

   // can read but dont touch
   CBTree            m_tTitles;        // tree of the help titles
   OHCATEGORY        m_OHCATEGORY;     // category for root
   CEscSearch        m_Search;         // search to use

private:
   void OHCATEGORYFree (POHCATEGORY pCat);
   BOOL OHCATEGORYAdd (PWSTR pszPath, PCResHelp pHelp, POHCATEGORY pCat);

   PCMIFLVM          m_pVM;            // virtual machine
   LANGID            m_lid;            // langauge for the help
};
typedef COnlineHelp *PCOnlineHelp;

/*************************************************************************************
CMainWindow */

#define WM_SEARCHCALLBACK        (WM_USER+186)

extern PSTR gpszCircumRealityServerMainWindow;

DEFINE_GUID(GUID_SavedGame,
0xa1c440a4, 0xab4a, 0x1fb8, 0xae, 0x1c, 0x45, 0xab, 0x85, 0x71, 0x1c, 0x09);

DEFINE_GUID(GUID_BinaryData,
0xa25440a6, 0xa14a, 0x1fb8, 0xae, 0x1c, 0x45, 0xab, 0xc5, 0x71, 0x1c, 0x69);

// MFCACHE - Used to cache all the information from the megafile so
// that it's fast to be downloaded
typedef struct {
   PVOID          pbData;     // data, must be freed on deletiong
   __int64        iSize;      // # of bytes of data in pbData
   FILETIME       ftModify;   // last modify filetime
} MFCACHE, *PMFCACHE;

// CPUMONITOR - Information for monitoring CPU usage
typedef struct {
   GUID           gObject;    // Object
   __int64        iTimeStart; // time, from QueryPerformanceCounter(), when the event started
   __int64        iTimeElapsed;   // time elapsed between calls, in milliseconds
   __int64        iTimeThread;// time spent in the thread
} CPUMONITOR, *PCPUMONITOR;

void MainWindowTitle (PCWSTR pszFile, PSTR pszTitle, DWORD dwTitleSize);

class CImageDatabaseThread;
typedef CImageDatabaseThread *PCImageDatabaseThread;

class CMainWindow {
public:
   ESCNEWDELETE;

   CMainWindow (void);
   ~CMainWindow (void);
   BOOL Init (PWSTR pszMegaFile);

   BOOL VMSet (PCMIFLVM pVM);
   PCMIFLVM VMGet (void);

   PCMegaFile BinaryDataMega (void);

   BOOL HelpConstruct (void);
   PWSTR HelpGetArticle (PWSTR pszName, PCListVariable plBooks, PCResHelp *ppHelp,
      PCMIFLVar pActor, PCMIFLVM pVM);
   BOOL HelpContents (PWSTR pszPath, PCListVariable plBooks,
                                PCListFixed plPCResHelp, PCListVariable plSubDir,
                                PCMIFLVar pActor, PCMIFLVM pVM);
   BOOL HelpSearch (PWSTR pszKeywords, PCListVariable plBooks,
                              PCListFixed plPCResHelp, PCListFixed plScore,
                              PCMIFLVar pActor, PCMIFLVM pVM);

   void CPUMonitorTrim (__int64 iTime);
   void CPUMonitor (GUID *pgObject);
   double CPUMonitorUsed (GUID *pgObject);

   // semiprivate
   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   // member viaraibles
   HWND              m_hWnd;        // window handle for the main window
   BOOL              m_fPostQuitMessage;  // if TRUE then post quit message when user closes the window
   PCInternetThread  m_pIT;          // talk to the internet
   PCImageDatabaseThread m_pIDT;    // image database thread
   PCTextLog         m_pTextLog;       // where to log date
   PCEmailThread     m_pEmailThread;

   // can read but dont change
   CBTree            m_tMFCACHE;    // tree of cached file info
   PCDatabase        m_pDatabase;      // database for storing items
   PCInstance        m_pInstance;   // instance for storing items
   CCircumrealityNLP           m_NLP;         // NLP database
   CMem              m_memShardParam;  // parameter string passed into shard, or "offline" if offline
   BOOL              m_fShutDownImmediately; // set to TRUE if request an immediate shutodnw
   WCHAR             m_szDatabaseDir[256];  // directory where put database, including last '\'

   CListVariable     m_lWaveBaseValid; // list of valid wave bases

private:
   PCOnlineHelp HelpGetPCOnlineHelp (void);
   BOOL HelpVerifyCatHasVisibleBooks (POHCATEGORY pCat, PCListVariable plBooks);
   BOOL HelpCanUserSee (PCMIFLVar pActor, PCMIFLVM pVM, PCResHelp pHelp);

   void Paint (void);
   void PacketTextCommand (PCCircumrealityPacket pp, PCMem pMem);
   void PacketUploadImage (PCCircumrealityPacket pp, PCMem pMem);
   void PacketVoiceChat (PCCircumrealityPacket pp, PCMMLNode2 pNode, PCMem pMem);
   void PacketLogOff (PCCircumrealityPacket pp, PCMem pMem);
   void PacketFileRequest (PCCircumrealityPacket pp, PCMem pMem, BOOL fIgnoreDir);
   void PacketFileDateQuery (PCCircumrealityPacket pp, PCMem pMem);
   void PacketInfoForServer (PCCircumrealityPacket pp, PCMMLNode2 pNode);
   void PacketServerQueryWant (DWORD dwConnectionID, BOOL fRender, PCCircumrealityPacket pp, PCMem pMem);
   void PacketServerCache (DWORD dwConnectionID, BOOL fRender, PCCircumrealityPacket pp, PCMem pMem);
   void PacketServerRequest (DWORD dwConnectionID, BOOL fRender, PCCircumrealityPacket pp, PCMem pMem);

   CListFixed        m_lPCOnlineHelp;  // list of online helps generated so far

   PCMegaFile        m_pmfBinaryData;  // megefile for binary data
   HWND              m_hWndList;    // list of connections
   PCMIFLVM          m_pVM;         // VM being used. NOT freed when this is shut down
   BOOL              m_fTimerInFunc;   // set to TRUE if have created timer because had message when in function

   DWORD             m_dwConnectionNew;   // function ID for connection new, -1 if none
   DWORD             m_dwConnectionMessage;  // method ID for connection message, -1 if none
   DWORD             m_dwConnectionUploadImage; // method ID for connection upload image, -1 if none
   DWORD             m_dwConnectionVoiceChat; // method ID for connection voice chat, -1 if none
   DWORD             m_dwConnectionError; // method ID for connection error, -1 if there is none
   DWORD             m_dwConnectionLogOff;   // method ID for a logoff message coming in, or -1 if none
   DWORD             m_dwConnectionInfoForServer;  // method ID for info for server message, -1 if none

   WCHAR             m_szConnectFile[256];   // megafile that using
   BOOL              m_fConnectRemote;    // true if the conenction is remote
   DWORD             m_dwConnectPort;     // port connected to

   DWORD             m_dwDatabaseCount;   // so can trick-save the database

   // CPU usage by "object"
   __int64           m_iPerCountFreq;     // from QueryPerformanceFrequency()
   __int64           m_iPerCountLast;     // when actual object set
   __int64           m_iPerCount60Sec;    // 60 seconds of count
   __int64           m_iCPUThreadTimeStart;  // thread time at start
   GUID              m_gCPUObject;        // object that looking at
   BOOL              m_fCPUObjectNULL;    // set to TRUE if the CPU object is NULL
   CListFixed        m_lCPUMONITOR;       // list of CPU minitoring information
};
typedef CMainWindow *PCMainWindow;


extern PCMainWindow gpMainWindow;

BOOL StringPlusOneToBinary (PWSTR psz, PCMem pMem);
BOOL BinaryToStringPlusOne (PBYTE pbData, DWORD dwSize, PCMem pMem);

/**********************************************************************************
CMIFLSocketServer */

// SLIMP_XXX - Convert string to this ID for fast info about fuctions, etc.
#define SLIMP_CONNECTIONNEW            1  // callback function in library that causes new func to be created
#define SLIMP_CONNECTIONMESSAGE        2  // callback method that is called when message receveid
#define SLIMP_CONNECTIONERROR          3  // callback method that is called when an error occurs in the connection (=> disconnect)

#define SLIMP_CONNECTIONQUEUEDTOSEND   8  // returns number of bytes queued to send out
#define SLIMP_CONNECTIONBAN            9  // bans an IP address from connecting, closing it right away
#define SLIMP_CONNECTIONDISCONNECT     10 // function that will disconnect and delete connection
#define SLIMP_CONNECTIONSEND           11 // function to send a message out to a given connection
#define SLIMP_CONNECTIONINFOSET        12 // sets piece of information about the connection (such as user name)
#define SLIMP_CONNECTIONINFOGET        13 // gets piece of information about the connection
#define SLIMP_CONNECTIONENUM           14 // enumerates all the connections, as numbers or objects
#define SLIMP_TOSTRINGMML              15 // sanitizes the string so can combine it in resource tags
#define SLIMP_SHARDPARAM               16 // returns string specific to the shard
#define SLIMP_SHUTDOWNIMMEDIATELY      17 // set flag for immediate shutdown
#define SLIMP_CONNECTIONSENDVOICECHAT  18 // function to send a voice-chat message out to a given connection
#define SLIMP_VOICECHATALLOWWAVES      19 // allow waves in voice chat

#define SLIMP_INFOIP                   20 // IP address
#define SLIMP_INFOSTATUS               21 // status string
#define SLIMP_INFOUSER                 22 // user string
#define SLIMP_INFOCHARACTER            23 // character string
#define SLIMP_INFOOBJECT               24 // object
#define SLIMP_INFOSENDBYTES            25 // number bytes sent
#define SLIMP_INFOSENDBYTESEXPECT      26 // number bytes sent expected
#define SLIMP_INFORECEIVEBYTES         27 // number bytes receivec
#define SLIMP_INFORECEIVEBYTESEXPECT   28 // number bytes received expected
#define SLIMP_INFOCONNECTTIME          29 // time that connected, number of seconds before present
#define SLIMP_INFOSENDLAST             30 // last time that sent, number of seconds before present
#define SLIMP_INFORECEIVELAST          31 // last time that received, number of seconds before present
#define SLIMP_INFOBYTESPERSEC          32 // average bytes per second received
#define SLIMP_INFOUNIQUEID             33 // unique ID
#define SLIMP_INFOSENDBYTESCOMP        34 // compressed bytes sent
#define SLIMP_INFORECEIVEBYTESCOMP     35 // compressed bytes received

#define SLIMP_DATABASEOBJECTADD        40 // add object to database
#define SLIMP_DATABASEOBJECTSAVE       41 // saves a checked out object
#define SLIMP_DATABASEOBJECTCHECKOUT   42 // checks out an object
#define SLIMP_DATABASEOBJECTCHECKIN    43 // checks in an object
#define SLIMP_DATABASEOBJECTDELETE     44 // deletes an object that's not checked out
#define SLIMP_DATABASEPROPERTYGET      45 // gets 1 or more properties from one or more objects in the database
#define SLIMP_DATABASEPROPERTYSET      46 // sets 1 or more properties from one or more objects in the database
#define SLIMP_DATABASEQUERY            47 // runs a query on the database
#define SLIMP_DATABASEPROPERTYADD      48 // adds a property to be indexed
#define SLIMP_DATABASEPROPERTYREMOVE   49 // removes a property that is to be indexed
#define SLIMP_DATABASESAVE             50 // saves the database to disk
#define SLIMP_DATABASEBACKUP           51 // saves the database, and backs up to another directory
#define SLIMP_DATABASEPROPERTYENUM     52 // enumerates all the cached properties
#define SLIMP_DATABASEOBJECTQUERYCHECKOUT 53 // returns whether or not the object is checked out
#define SLIMP_DATABASEOBJECTENUMCHECKOUT 54 // returns a list of checked out objects
#define SLIMP_DATABASEOBJECTNUM        55 // returns the number of objects (checked out)
#define SLIMP_DATABASEOBJECTGET        56 // gets the Nth (checked out) object
#define SLIMP_DATABASEOBJECTQUERYCHECKOUT2 57 // returns the database that the object is checked out to

#define SLIMP_NLPPARSERENUM            60 // enumerate parsers
#define SLIMP_NLPPARSERREMOVE          61 // Delete parsers
#define SLIMP_NLPPARSE                 62 // parse using parser
#define SLIMP_NLPRULESETENUM           63 // enumerate rule sets
#define SLIMP_NLPRULESETADD            64 // add rule set
#define SLIMP_NLPRULESETENABLEGET      65 // enable rule-set get
#define SLIMP_NLPRULESETENABLESET      66 // enable rule set
#define SLIMP_NLPRULESETREMOVE         67 // remove a rule sset
#define SLIMP_NLPPARSERCLONE           68 // clone a parser
#define SLIMP_NLPVERBFORM              69 // conjugate the verbs
#define SLIMP_NLPNOUNCASE              70 // apply noun case rules to parser
#define SLIMP_NLPRULESETENABLEALL      71 // enable/disable all rule-sets
#define SLIMP_NLPRULESETEXISTS         72 // sees if the rule set exists

#define SLIMP_SAVEDGAMEENUM            80 // enumerate items in saved game
#define SLIMP_SAVEDGAMEREMOVE          81 // remote file in saved game
#define SLIMP_SAVEDGAMESAVE            82 // save a game
#define SLIMP_SAVEDGAMELOAD            83 // load a saved game
#define SLIMP_SAVEDGAMENUM             84 // returns number of saved games
#define SLIMP_SAVEDGAMENAME            85 // given index number, reutrns the name
#define SLIMP_SAVEDGAMEINFO            86 // returns info about a specific save
#define SLIMP_SAVEDGAMEFILESENUM       87 // enumerates main files
#define SLIMP_SAVEDGAMEFILESDELETE     88 // deletes main files
#define SLIMP_SAVEDGAMEFILESNUM        89 // returns number of saved games
#define SLIMP_SAVEDGAMEFILESNAME       90 // returns the name

#define SLIMP_BINARYDATASAVE           100   // save file to binary data
#define SLIMP_BINARYDATAREMOVE         101   // deletge file
#define SLIMP_BINARYDATARENAME         102   // rename
#define SLIMP_BINARYDATAENUM           103   // enumerate files
#define SLIMP_BINARYDATANUM            104   // return number of files
#define SLIMP_BINARYDATAGETNUM         105   // gets the name based on a number
#define SLIMP_BINARYDATALOAD           106   // load info from binary
#define SLIMP_BINARYDATAQUERY          107   // query file info

#define SLIMP_EMAILSEND                110   // sends an email message

#define SLIMP_PERFORMANCEMEMORY        120   // information about memory
#define SLIMP_PERFORMANCECPU           121   // inforamtion about CPU usage
#define SLIMP_PERFORMANCENETWORK       122   // amount of data sent/received
#define SLIMP_PERFORMANCEDISK          123   // amount of disk space free
#define SLIMP_PERFORMANCEGUI           124   // GDI resources for the process
#define SLIMP_PERFORMANCETHREADS       125   // number of threads (total) in the server
#define SLIMP_PERFORMANCEHANDLES       126   // number of handles (total) in the server
#define SLIMP_PERFORMANCECPUOBJECT     127   // keep track of CPU used by a specific object (PC)
#define SLIMP_PERFORMANCECPUOBJECTQUERY 128  // returns percent of CPU used by the object

#define SLIMP_TEXTLOG                  130   // log text using auto
#define SLIMP_TEXTLOGNOAUTO            131   // log text with parameters
#define SLIMP_TEXTLOGNUMLINES          132   // return number of lines
#define SLIMP_TEXTLOGENUM              133   // enumerates logs
#define SLIMP_TEXTLOGDELETE            134   // delete file
#define SLIMP_TEXTLOGREAD              135   // reads a line
#define SLIMP_TEXTLOGAUTOSET           136   // set auto value
#define SLIMP_TEXTLOGAUTOGET           137   // get an auto value
#define SLIMP_TEXTLOGENABLESET         138   // set enable
#define SLIMP_TEXTLOGENABLEGET         139   // get enable
#define SLIMP_TEXTLOGSEARCH            140   // search through text log

#define SLIMP_HELPARTICLE              150 // returns a help article
#define SLIMP_HELPCONTENTS             151 // returns the contents at a specific level
#define SLIMP_HELPSEARCH               152 // searches through the help

#define SLIMP_CONNECTIONSLIMIT         160   // limit the number of connections

#define SLIMP_RENDERCACHELIMITS        170   // limit the amount of memory/disk that the render cache can use
#define SLIMP_RENDERCACHESEND          171   // send a cached item to the client (usually used for TTS)


// CMIFLSocketServer - For the callback
class CMIFLSocketServer : public CMIFLAppSocket  {
public:
   ESCNEWDELETE;

   CMIFLSocketServer ();
   ~CMIFLSocketServer ();

   virtual DWORD LibraryNum (void);
   virtual BOOL LibraryEnum (DWORD dwNum, PMASLIB pLib);
   virtual DWORD ResourceNum (void);
   virtual BOOL ResourceEnum (DWORD dwNum, PMASRES pRes);
   virtual PCMMLNode2 ResourceEdit (HWND hWnd, PWSTR pszType, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
      PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes);
   virtual BOOL FileSysSupport (void);
   virtual BOOL FileSysLibOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);
   virtual PCMMLNode2 FileSysOpen (PWSTR pszFile);
   virtual BOOL FileSysSave (PWSTR pszFile, PCMMLNode2 pNode);
   virtual BOOL FuncImportIsValid (PWSTR pszName, DWORD *pdwParams);
   virtual BOOL FuncImport (PWSTR pszName, PCMIFLVM pVM, PCMIFLVMObject pObject,
      PCMIFLVarList plParams, DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVar pRet);
   virtual BOOL TestQuery (void);
   virtual void TestVMNew (PCMIFLVM pVM);
   virtual void TestVMPageEnter (PCMIFLVM pVM);
   virtual void TestVMPageLeave (PCMIFLVM pVM);
   virtual void TestVMDelete (PCMIFLVM pVM);
   virtual void Log (PCMIFLVM pVM, PWSTR psz);
   virtual void VMObjectDelete (PCMIFLVM pVM, GUID *pgID);
   virtual void VMTimerToBeCalled (PCMIFLVM pVM);
   virtual void MenuEnum (PCMIFLProj pProj, PCMIFLLib pLib, PCListVariable plMenu);
   virtual BOOL MenuCall (PCMIFLProj pProj, PCMIFLLib pLib, PCEscWindow pWindow, DWORD dwIndex);

   // can read, but don't change
   CHashString       m_hImport;           // converts import functions to DWORD (SLIMP_XXX)

private:
   void InternalTestVMNew (PCMIFLVM pVM, BOOL fHack);
   BOOL NLPParserEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPParserRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPParserClone (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPParse (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPRuleSetEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPRuleSetRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPRuleSetAdd (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPRuleSetEnableGet (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPRuleSetExists (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPRuleSetEnableAll (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPRuleSetEnableSet (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPVerbForm (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL NLPNounCase (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameSave (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameLoad (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameName (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameInfo (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameFilesEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameFilesDelete (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameFilesNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL SavedGameFilesName (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL HelpArticle (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL HelpContents (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL HelpSearch (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL HelpGetBooks (PCMIFLVM pVM, PCMIFLVar pBooks, PCListVariable plBooks);
   BOOL BinaryDataSave (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL BinaryDataRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL BinaryDataRename (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL BinaryDataEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL BinaryDataNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL BinaryDataGetNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL BinaryDataLoad (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);
   BOOL BinaryDataQuery (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet);

   BOOL m_fJustCalledHackITVMNew;           // set to TRUE if just called with a hack InternalTestVMNew
};

/**********************************************************************************
ResImageEdit */
PCMMLNode2 ResImageEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);

/**********************************************************************************
ResWaveEdit */
PCMMLNode2 ResWaveEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);

/**********************************************************************************
ResCutScene */
#define SPEAKSTYLEDEFAULT        5     // 5th entry. normal
extern PWSTR gapszSpeakStyle[10];

PCMMLNode2 ResCutSceneEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                            PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes);
BOOL IsObjectDerivedFromClass (PCMIFLProj pProj, PWSTR pszObject, PWSTR pszClass, DWORD dwRecurse = 0);
void PrependOptionToList (PWSTR pszOption, PCListVariable pl, PCHashString ph);
void FillListWithResources (PWSTR *papszResourceType, DWORD dwNumResourceType,
                            PWSTR *papszClass, DWORD dwNumClass, PCMIFLProj pProj, PCListVariable pl, PCHashString ph);

/**********************************************************************************
ResSpeakScript */
void TransProsQuickFillResourceFromNode (PCMIFLLib pLib, PCMMLNode2 pNode, LANGID lid,
                                         PCListFixed plResID);
PCMMLNode2 ResSpeakScriptEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                            PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes);

/**********************************************************************************
ResConvScript */
PCMMLNode2 ResConvScriptEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                            PCMIFLProj pProj, PCMIFLLib pLi, PCMIFLResource pResb);

/**********************************************************************************
ResMusicEdit */
PCMMLNode2 ResMusicEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);


/**********************************************************************************
ResVoice */
PCMMLNode2 ResVoiceEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);


/**********************************************************************************
ResText */
PCMMLNode2 ResTextEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);


/**********************************************************************************
ResLotsOfText */
PCMMLNode2 ResLotsOfTextEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);

/**********************************************************************************
ResMenu */
PCMMLNode2 ResMenuEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);


/**********************************************************************************
MegaFile */
BOOL MegaFileGenerateIfNecessary (PWSTR pszFile, PWSTR pszFileCRL, PCMIFLProj pProj, HWND hWnd);
void ProjectNameToCircumreality (PWSTR pszProject, BOOL fCircumreality, PWSTR pszCircumreality);
extern PWSTR gpszProjectFile;


/**********************************************************************************
Main */
extern PCMIFLProj     gpMIFLProj;    // project
extern BOOL           gfM3DInit;   // set if 3D initialized
extern DWORD          gdwCmdLineParam; // what is sent into command line paramter
extern BOOL           gfRestart;    // if TRUE then will restart the application on a shutdown
extern char *        gpszServerKey;


/**********************************************************************************
CMIFNLP */
PCMMLNode2 NLPRuleSetEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly, PCMIFLProj pProj);



// NLPNC_XXX flags for the noun case
#define NLPNC_ART_MASK           0x0003         // article mask
#define NLPNC_ART_NONE           0x0001         // no article used
#define NLPNC_ART_INDEFINITE     0x0002         // indefinite article
#define NLPNC_ART_DEFINITE       0x0003         // indefinite article

#define NLPNC_COUNT_MASK         0x000c         // number mask
#define NLPNC_COUNT_SINGLE       0x0004         // single item
#define NLPNC_COUNT_FEW          0x0008         // a few - depends upon the language
#define NLPNC_COUNT_MANY         0x000c         // many

#define NLPNC_ANIM_MASK          0x0030         // animate mask
#define NLPNC_ANIM_ANIMATE       0x0010         // animate object
#define NLPNC_ANIM_INANIMATE     0x0020         // inanimate object

#define NLPNC_FAM_MASK           0x00c0         // familiar mask
#define NLPNC_FAM_FAMILIAR       0x0040         // familiar object
#define NLPNC_FAM_FORMAL         0x0080         // formal object

#define NLPNC_VERBOSE_MASK       0x0300         // short vs. long name mask
#define NLPNC_VERBOSE_SHORT      0x0100         // short form of the name
#define NLPNC_VERBOSE_LONG       0x0200         // long form of the name

#define NLPNC_GENDER_MASK        0x0c00         // gender mask
#define NLPNC_GENDER_MALE        0x0400
#define NLPNC_GENDER_FEMALE      0x0800
#define NLPNC_GENDER_NEUTER      0x0c00

#define NLPNC_CAPS_MASK          0x3000         // capitalization mask
#define NLPNC_CAPS_LOWER         0x1000         // lower case
#define NLPNC_CAPS_UPPER         0x2000         // upper case

#define NLPNC_PERSON_MASK        0xc000         // 1st, 2nd, 3rd person mask
#define NLPNC_PERSON_FIRST       0x4000
#define NLPNC_PERSON_SECOND      0x8000
#define NLPNC_PERSON_THIRD       0xc000

#define NLPNC_CASE_MASK          0x00ff0000     // case mask
#define NLPNC_CASE_ABESSIVE      0x00010000
#define NLPNC_CASE_ABLATIVE      0x00020000
#define NLPNC_CASE_ABSOLUTIVE    0x00030000
#define NLPNC_CASE_ACCUSATIVE    0x00040000     // english objective
#define NLPNC_CASE_ADESSIVE      0x00050000
#define NLPNC_CASE_ALLATIVE      0x00060000
#define NLPNC_CASE_COMITATIVE    0x00070000
#define NLPNC_CASE_DATIVE        0x00080000     // english objective
#define NLPNC_CASE_DEDATIVE      0x00090000
#define NLPNC_CASE_ELATIVE       0x000a0000
#define NLPNC_CASE_ERGATIVE      0x000b0000
#define NLPNC_CASE_ESSIVE        0x000c0000
#define NLPNC_CASE_GENITIVE      0x000d0000     // english posessive
#define NLPNC_CASE_ILLATIVE      0x000e0000
#define NLPNC_CASE_INESSIVE      0x000f0000
#define NLPNC_CASE_INSTRUMENTAL  0x00100000
#define NLPNC_CASE_LOCATIVE      0x00110000
#define NLPNC_CASE_NOMINATIVE    0x00120000     // english subjective
#define NLPNC_CASE_OBLIQUE       0x00130000
#define NLPNC_CASE_PARTITIVE     0x00140000
#define NLPNC_CASE_POSSESSIVE    0x00150000     // english posessive
#define NLPNC_CASE_POSTPOSITIONAL 0x00160000
#define NLPNC_CASE_PREPOSITIONAL 0x00170000
#define NLPNC_CASE_PROLATIVE     0x00180000
#define NLPNC_CASE_TERMINATIVE   0x00190000
#define NLPNC_CASE_TRANSLATIVE   0x001a0000
#define NLPNC_CASE_VOCATIVE      0x001b0000

#define NLPNC_QUANTITY_MASK      0x03000000     // quantity mask
#define NLPNC_QUANTITY_NOQUANTITY 0x01000000     // don't show quantity
#define NLPNC_QUANTITY_QUANTITY  0x02000000     // show quantity



PWSTR NLPNounCase (PWSTR pszParse, PCMem pmParsed, DWORD dwNounCase, BOOL fAppendNounCase);
BOOL NLPVerbForm (PWSTR psz, PCMIFLVM pVM, PCMem pMem);


/*************************************************************************************
CResHelp */

class DLLEXPORT CResHelp {
public:
   ESCNEWDELETE;

   CResHelp (void);
   ~CResHelp (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, LANGID lid);

   DWORD CheckSum (void);
   PWSTR ResourceGet (void);

   // variables
   CMem           m_memName;        // pointer to CMem containing the name
   CMem           m_memDescShort;   // pointer to CMem containing the description. Might be NULL
   CMem           m_memDescLong;    // long description, MML text
   CMem           m_aMemHelp[2];    // memory used for the help category, 1 and 2
   CMem           m_memFunction;    // function to call, or empty string if none
   CMem           m_memFunctionParam;// paramter for the function
   CMem           m_memBook;        // book that this help topic is in
   LANGID         m_lid;            // language ID to use

private:
   CMem           m_memAsResource;  // what this would be as a resource string
};
typedef CResHelp *PCResHelp;

PCMMLNode2 ResHelpEdit (HWND hWnd, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly);




/**********************************************************************************
Monitor */


#define NUMWORLDSTOMONITOR       3     // number of worlds to monitor
// WORLDMONITOR
typedef struct {
   WCHAR          aszWorldFile[NUMWORLDSTOMONITOR][256]; // files to monitor, or empty string if not
   DWORD          adwShardNum[NUMWORLDSTOMONITOR]; // shard number to use
   WCHAR          szDomain[128];       // domain name
   WCHAR          szSMTPServer[128];   // SMTP server
   WCHAR          szEMailTo[128];      // Email to
   WCHAR          szEMailFrom[128];    // Email from
   WCHAR          szAuthUser[128];     // Authentication user
   WCHAR          szAuthPassword[128]; // Authentication password
   BOOL           fEmailOnSuccess;     // Email even on success
} WORLDMONITOR, *PWORLDMONITOR;

void MonitorInfoFromReg (PWORLDMONITOR pWM);
void MonitorInfoToReg (PWORLDMONITOR pWM);
void MonitorInfoFromReg (PWORLDMONITOR pWM);
BOOL MonitorHeartbeatCheck (PCWSTR pszFile);
void MonitorHeartbeatTimerStart (PCWSTR pszFile);
void MonitorHeartbeatTimerStop (void);
void MonitorAll (void);


/**********************************************************************************
CImageDatabase.cpp */

#define IMAGEDATABASE_MAXQUALITIES  8           // maximum number of quality levels stored in the database

#define IMAGEDATABASETYPE_IMAGE     0     // this is an image
#define IMAGEDATABASETYPE_WAVE      1     // this is a wave file


// IMAGEDATABASEQUALITYINFO - Information about a quality level
typedef struct {
   DWORD                dwIndexFile;   // index file number. -1 = empty
   __int64              iFileSize;     // file size, in bytes
} IMAGEDATABASEQUALITYINFO, *PIMAGEDATABASEQUALITYINFO;

// IMAGEDATABASEENTRY - Entry for actual info
typedef struct {
   // same as IMAGEDATABASEINDEXENTRY
   __int64              iTimesRequested;  // number of times this file was requested
   __int64              iTimesDelivered;  // number of times this file was delivered
   DWORD                dwType;           // IMAGEDATABASETYPE_IMAGE or IMAGEDATABASETYPE_WAVE
   IMAGEDATABASEQUALITYINFO aQuality[IMAGEDATABASE_MAXQUALITIES]; // array of differnet qualities

   PCWSTR               pszStringFromHash;        // identity string. Actually owned by m_hEntries
} IMAGEDATABASEENTRY, *PIMAGEDATABASEENTRY;

/* CImageDatabase */
class CImageDatabase {
public:
   ESCNEWDELETE;

   CImageDatabase (void);
   ~CImageDatabase (void);

   BOOL Init (PCWSTR pszPath, __int64 iFileTime);
   PIMAGEDATABASEENTRY Add (PCWSTR pszString);
   BOOL Remove (PCWSTR pszString, BOOL fAlreadyLower, CRITICAL_SECTION *pCritSec);
   PIMAGEDATABASEENTRY Find (PCWSTR pszString, BOOL fAlreadyLower);
   DWORD Num (void);
   PIMAGEDATABASEENTRY Get (DWORD dwIndex);
   BOOL DeleteFileNum (DWORD dwFileNum, __int64 iFileSize, CRITICAL_SECTION  *pCritSec);
   DWORD SaveFileNum (PVOID pMem, size_t iSize, CRITICAL_SECTION *pCritSec);
   BOOL IndexSave (void);
   void FileName (DWORD dwFileNumber, PWSTR pszFileName);

   // read but don't modify
   __int64           m_iTotalFileSize;    // total size used by all files
   WCHAR             m_szPath[256];       // path passed into Init()

private:
   BOOL EnumSubDirectories (PCMem pMemSizes);
   BOOL IndexLoad (PCMem pMemSizes);
   PCWSTR ToLowerForHash (PCWSTR psz, PCMem pMemTemp);

   CMem              m_memFilesUsed;      // bitfield for which files are used. m_dwCurPosn = amount of valid data
                                          // DWORD aligned
   __int64           m_iFileTime;         // file time passed into Init()
   CHashString        m_hEntries;          // QWORD hash of entries
};
typedef CImageDatabase *PCImageDatabase;



/* CImageDatabaseThread - Thread that manages the image database.
*/
class CImageDatabaseThread {
public:
   ESCNEWDELETE;

   CImageDatabaseThread (void);
   ~CImageDatabaseThread (void);

   BOOL ThreadStart (PCWSTR pszPath, __int64 iFileTime, PCInternetThread pIT);
   BOOL ThreadStop (void);

   BOOL EntryAdd (BOOL fAlreadyInCritSec, DWORD dwAction, PCWSTR pszString,
                                     DWORD dwQuality, DWORD dwQualityMax, DWORD dwType, DWORD dwConnectionID,
                                     PVOID pData, size_t iDataSize);
   BOOL EntryAddAccept (BOOL fAlreadyInCritSec, PCWSTR pszString,
                                     DWORD dwQuality, DWORD dwType, DWORD dwConnectionID,
                                     PVOID pData, size_t iDataSize);
   void LimitsSet (__int64 iDataMaximum, __int64 iDataMinimumHardDrive,
                                      __int64 iDataGreedy, DWORD dwDataMaxEntries);
   int EntryExists (BOOL fAlreadyInCritSec, PCWSTR pszString,
                                       DWORD dwQuality, DWORD dwQualityMax, DWORD dwType);

   // called from the within the internet thread
   void ThreadProc (void);

private:
   void CleanUp (BOOL fAlreadyInCritSec, __int64 iTime);
   
   
   // protected under critical section
   CRITICAL_SECTION  m_CritSec;        // criitcal section for accessing variables
   BOOL              m_fRunning;       // set to TRUE if the thread is running, FALSE if hasn't started
                                       // or is shutting down
   CImageDatabase    m_ImageDatabase;  // database
   HANDLE            m_hThread;        // thread handle
   HANDLE            m_hSignalToThread;   // set when the thread should shut down
   __int64           m_iDataMaximum;   // maximum data allowd in m_ImageDatabase
   __int64           m_iDataMinimumHardDrive; // mininum hard drive bytes that must be available
   __int64           m_iDataGreedy;    // if have less than this amount of data then be greedy,
                                       // and accept it from anyone
   DWORD             m_dwDataMaxEntries;  // maximum entries allowed
   CListFixed        m_lIDTENTRYQUEUE; // list of actions to do
   CListFixed        m_lIDTCONNECTIONDATA;   // list of last times data was sent
   PCInternetThread  m_pIT;            // internet thread to send to

};
typedef CImageDatabaseThread *PCImageDatabaseThread;



#endif // _MIFSERVER_H



