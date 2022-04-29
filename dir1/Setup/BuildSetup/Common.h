/* common.h - for common.cpp */

#ifndef _COMMON_H_
#define _COMMON_H_

extern BOOL gDontCompress;      // if set to TRUE then dont compress

// langid
typedef struct {
   LANGID      lid;
   char        *psz;
} LANGIDTOSTRING, *PLANGIDTOSTRING;


DWORD LangIndexFromID (LANGID lid);
LANGIDTOSTRING *LangIndex (DWORD i);
char *LangFromID (LANGID lid);
LANGID LangToID (char *psz);
DWORD LangNum (void);

// subject
typedef struct {
   WORD        wMajor;  // major subject
   WORD        wMinor;  // minor subject. 0 if this is a lable for major subhect
   char        *psz;
} SUBIDTOSTRING, *PSUBIDTOSTRING;

DWORD SubNum (void);
char *SubFromID (WORD wPrimary, WORD wSecondary);
WORD SubToPrimaryID (char *psz);
WORD SubToSecondaryID (WORD wPrimary, char *psz);
SUBIDTOSTRING * SubIndex (DWORD i);

// hash string for registration
DWORD HashString (char *psz);

// for siliconpage book files
#define  FOURCC_SILICONPAGE MAKEFOURCC ('s', 'p', 'g', 'e') // main chunk
#define  FOURCC_VERSION    MAKEFOURCC ('v', 'r', 's', 'n')
#define  FOURCC_INFO       MAKEFOURCC ('i', 'n', 'f', 'o')
#define  FOURCC_AD         MAKEFOURCC ('a', 'd', ' ', ' ')
#define  FOURCC_ADURL      MAKEFOURCC ('U', 'R', 'L', ' ')  // in ad chunk
#define  FOURCC_CHAPTER    MAKEFOURCC ('c', 'h', 'a', 'p')
#define  FOURCC_IMAGE      MAKEFOURCC ('j', 'p', 'g', ' ')
#define  FOURCC_CHAPTERNAME MAKEFOURCC ('n', 'a', 'm', 'e') // in the chapter chunk
#define  FOURCC_IMAGENAME  MAKEFOURCC ('n', 'a', 'm', 'e') // in the chapter chunk
#define  FOURCC_TOC        MAKEFOURCC ('t', 'o', 'c', ' ')  // in the chapter chunk
#define  FOURCC_PASSWORD   MAKEFOURCC ('p', 'w', 'd', ' ')  // in the chapter chunk. contains a DWORD

// project file
#define  FOURCC_SPPROJECT  MAKEFOURCC ('s', 'p', 'p', 'r') // main chunk. Also contains info
#define  FOURCC_SPP_CHAPTERS MAKEFOURCC ('c', 'h', 'a', 'p') // chapters
#define  FOURCC_SPP_IMAGES  MAKEFOURCC ('j', 'p', 'g', ' ') // jpg files

typedef struct {
   DWORD    dwVersion;     // major version number. Need this or above to read file
} SPAGEVERSION, *PSPAGEVERSION;

#define  MAXSTR   128

typedef struct {
   LANGID   iLanguage;
   BOOL     fExpires;         // true if expires
   WORD     wExpireMonth;     // 1 = jan
   WORD     wExpireYear;      // such as 2000
   BOOL     fCopyPassword;    // have a password to prevent copying
   DWORD    dwCopyChapter;    // which chapter number this starts on
   char     szCopyWeb[MAXSTR];        // web page to acquire password
   char     szTitle[MAXSTR];  // book title
   char     szAuthor[MAXSTR];
   char     szPublisher[MAXSTR];
   char     szEmail[MAXSTR];
   char     szWebAuthorPublisher[MAXSTR];
   char     szWebBook[MAXSTR];
   char     szURLSPB[MAXSTR];    // web page for url to spb
   WORD     wVersionMajor;
   WORD     wVersionMinor;
   WORD     wVersionVeryMinor;
   BOOL     fRegistered;      // TRUE if author/pub registered, so book doesn't have frowning
} SPAGEINFO, *PSPAGEINFO;


/****************************************************************************8
Encrypt object */
#define  ENCRYPTSIZE    4096

class CEncrypt {
public:
   CEncrypt (void);
   ~CEncrypt (void);

   void  Init (DWORD dwKey);
   DWORD GetValue (DWORD dwPosn);
   void  EncryptBuffer (PBYTE pData, DWORD dwSize, DWORD dwStartPosn);

private:
   DWORD    m_adwEncrypt[ENCRYPTSIZE];
};

typedef CEncrypt * PCEncrypt;

/****************************************************************************8
Encoded file
*/
class CEncFile {
public:
   CEncFile (void);
   ~CEncFile (void);

   BOOL Open (char *psz);
   BOOL OpenMem (void *pMem, DWORD dwSize);
   BOOL Create (char *psz);
   BOOL Close (void);

   BOOL Write (PVOID pData, DWORD dwBytes);
   BOOL Read (PVOID pData, DWORD dwBytes);
   BOOL Seek (DWORD dwLoc);
   DWORD Tell (void);

private:
   FILE  *m_f;    // file reading/writing to/from

   void  *m_pMem; // if using memory based
   DWORD m_dwMemSize;   // if using memory based
   DWORD m_dwMemCur; // current position if using memory based

   CEncrypt    m_Encrypt;  // encryption object
};
typedef CEncFile * PCEncFile;

/****************************************************************************8
Chunk
*/
#define  CHUNKFLAG_COMPRESSED    0x0001   // - If set then the chunk data is compressed
#define  CHUNKFLAG_ENCRYPTED     0x0002   // - If set then the chunk data is encrypted.

class CChunk {
public:
   CChunk(void);
   ~CChunk(void);

   DWORD    m_dwID;       // chunk ID/type
   DWORD    m_dwFlags;     // CHUNKFLAG_XXX
   CChunk*  m_pChunkParent;   // parent chunk
   BOOL     m_fFileDirty;     // only valid for topmost parent. If TRUE file is dirty

   CChunk*  SubChunkGet (DWORD dwIndex);
   VOID     SubChunkDelete (DWORD dwIndex);
   CChunk*  SubChunkInsert (DWORD dwIndex);
   CChunk*  SubChunkAdd (void);
   DWORD    SubChunkFind (DWORD dwID, DWORD dwIndex = 0);
   PVOID    DataGet ();
   DWORD    DataSize ();
   BOOL     DataSet (PVOID pData, DWORD dwSize);
   void     FileDirty (void);
   DWORD    SubChunkNum (void);

   BOOL     Write (CEncFile *pFile, DWORD *pdwCompressedSized = NULL);
   BOOL     Read (CEncFile *pFile);

private:
   PVOID    m_pData;      // uncompressed, unencoded data
   DWORD    m_dwDataSize; // size in bytes
   DWORD    m_dwEncoding; // IMPORTANT - do this later

   CChunk**  m_paSubChunks; // pointer to an array of subchunk pointers
   DWORD    m_dwSubChunks; // number of sub chunks
   void     ClearOut (void);  // clears out the contents
};

typedef CChunk * PCChunk;


// LZ routines
PVOID LZCompress (PVOID pMem, DWORD dwSize, DWORD *pdwCompressSize);
PVOID LZDecompress (PVOID pComp, DWORD dwCompSize, DWORD *pdwDecompSize);

// random routines
void MySRand (DWORD dwVal);
void MySRand (void);
DWORD MyRand (void);

#endif // _COMMON_H_
