/***************************************************************************
Common.cpp - routines common among different SiliconPage exes

begun 11/17/99 by Mike Rozak
Copyright 1999 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include <stdio.h>
#include "z:\setup\buildsetup\Common.h"

#define  MXAC     MAKEFOURCC ('m', 'X', 'a', 'c')

typedef struct {
   DWORD       dwMagic; // magic number, MXAC
   DWORD       dwEncrypt;  // encryption key
} ENCHEADER, *PENCHEADER;


BOOL gDontCompress = FALSE;      // if set to TRUE then dont compress

/**********************************************************************************
MySRand, MyRand - Personal random functions.
*/
static DWORD   gdwRandSeed;

void MySRand (DWORD dwVal)
{
   gdwRandSeed = dwVal;
}

void MySRand (void)
{
   FILETIME ft;
   GetSystemTimeAsFileTime (&ft);

   gdwRandSeed = GetTickCount() + ft.dwHighDateTime + ft.dwLowDateTime;
}

DWORD MyRand (void)
{
   gdwRandSeed = (gdwRandSeed ^ 0x34a892b2) * (gdwRandSeed ^ 0x39b87c8a) +
      (gdwRandSeed ^ 0x1893b78a);

   return gdwRandSeed;
}



/**********************************************************************************
HashString - Hash an E-mail (or other string) to a DWORD number. Use this as the
registration key.

inputs
   char  *psz
returns
   DWORD
*/
DWORD HashString (char *psz)
{
   DWORD dwSum;

   DWORD i;
   dwSum = 324233;
   for (i = 0; psz[i]; i++) {
      MySRand ((DWORD) CharLower((char*) MAKELONG(psz[i], 0)));
      MyRand ();
      dwSum += (DWORD) MyRand();
   }

   return dwSum;
}


/*****************************************************************************
Constructor & destructor */
CEncrypt::CEncrypt (void)
{
   // this space intentionally left blank
}

CEncrypt::~CEncrypt (void)
{
   // this space intentionally left blank
}

/****************************************************************************
Init - Initializes the encrypt object using the given key.
*/
void CEncrypt::Init (DWORD dwKey)
{
   DWORD i;

   MySRand (dwKey);
   for (i = 0; i < ENCRYPTSIZE; i++)
      m_adwEncrypt[i] = MyRand();
}


/****************************************************************************
GetValue - Given a DWORD index into a data stream, this returns the encryption
value that's xored with the data to encrypt it.

inputs
   DWORD    dwPosn - position (in DWORDs)
returns
   DWORD    dwValue - Value to xor
*/
DWORD CEncrypt::GetValue (DWORD dwPosn)
{
   DWORD dwLoc;
   dwLoc = (dwPosn * dwPosn) % ENCRYPTSIZE;
   return m_adwEncrypt[dwLoc];
}


/****************************************************************************
EncryptBuffer - Encrypts an entire buffer. Not that the values are in bytes,
   so encyrption can be on odd DWORD alignments

inputs
   PBYTE    *pData - Data to encrypt (in place)
   DWORD    dwSize - size (in bytes) of the data
   DWORD    dwStartPosn - start of the encryption position, used in GetValue().
               This is automatically converted to DWORDs, and odd alignments are
               dealt with
returns
   none
*/

void  CEncrypt::EncryptBuffer (PBYTE pData, DWORD dwSize, DWORD dwStartPosn)
{
   DWORD dwTemp, dwVal, dwValInv;

   // if the start position is not DWORD align then get it aligned.
   if (dwStartPosn % sizeof(DWORD)) {
      dwTemp = 0;
      dwVal = dwStartPosn % sizeof(DWORD);
      dwValInv = sizeof(DWORD) - dwVal;
      memcpy ((PBYTE) (&dwTemp) + dwVal, pData, min(dwValInv, dwSize));
      
      // encrypt
      dwTemp ^= GetValue ((dwStartPosn - dwVal) / sizeof(DWORD));

      // write back
      memcpy (pData, (PBYTE) (&dwTemp) + dwVal, min(dwValInv, dwSize));

      // adjust the offsets
      pData += min(dwValInv, dwSize);
      dwSize -= min(dwValInv, dwSize);
      dwStartPosn += dwValInv;
   }

   // loop through data
   for (; dwSize >= sizeof(DWORD); dwSize -= sizeof(DWORD), pData += sizeof(DWORD), dwStartPosn += sizeof(DWORD)) {
      DWORD *pdw;
      pdw = (DWORD*) pData;
      *pdw = *pdw ^ GetValue (dwStartPosn / sizeof(DWORD));
   }

   // if there's remaining then special case
   if (dwSize) {
      dwTemp = 0;
      memcpy (&dwTemp, pData, dwSize);
      dwTemp ^= GetValue (dwStartPosn / sizeof(DWORD));
      memcpy (pData, &dwTemp, dwSize);
   }

   // done

}



/*****************************************************************************
Constructor & destructor */
CChunk::CChunk (void)
{
   m_pData = NULL;
   m_paSubChunks = 0;
   ClearOut();
}

CChunk::~CChunk (void)
{
   ClearOut();

}

void CChunk::ClearOut (void)
{
   if (m_pData)
      free (m_pData);
   m_pData = NULL;

   if (m_paSubChunks) {
      DWORD i;
      for (i = 0; i < m_dwSubChunks; i++)
         delete m_paSubChunks[i];
      free (m_paSubChunks);
   }
   m_paSubChunks = NULL;

   m_dwID = 0;
   m_dwFlags = CHUNKFLAG_COMPRESSED;   // default to compressed for all chunks
   m_dwDataSize = NULL;
   m_dwEncoding = 0;
   m_dwSubChunks = 0;
   m_pChunkParent = NULL;
   m_fFileDirty = FALSE;
}


/***************************************************************************************
FileDirty - Sets the file dirty flag of the top-most parent. This is called by any
function that changes the data.
*/
void     CChunk::FileDirty (void)
{
   CChunk   *p;
   for (p = this; p->m_pChunkParent; p = p->m_pChunkParent);

   p->m_fFileDirty = TRUE;
}


/***************************************************************************************
SubChunkGet - Gets a chunk for the subchunk.

inputs
   DWORD    dwIndex - chunk index, from 0 .. ?
returns
   CChunk* - Pointer to chunk. Do not delete this. NULL if beyond end of chunks
*/
CChunk*  CChunk::SubChunkGet (DWORD dwIndex)
{
   if (!m_paSubChunks || (dwIndex >= m_dwSubChunks))
      return NULL;

   return m_paSubChunks[dwIndex];
}

/*************************************************************************************
SubChunkDelete - Deletes a subchunk and all its chunks

inputs
   DWORD    dwIndex - chunk index, from 0 .. ?
returns
   none
*/
VOID     CChunk::SubChunkDelete (DWORD dwIndex)
{
   if (!m_paSubChunks || (dwIndex >= m_dwSubChunks))
      return;

   delete m_paSubChunks[dwIndex];

   memmove (m_paSubChunks + dwIndex, m_paSubChunks + (dwIndex+1), (m_dwSubChunks - dwIndex - 1) * sizeof(CChunk*));
   m_dwSubChunks--;

   FileDirty ();
}


/*************************************************************************************
SubChunkNum - Returns the number ofsubchuns
*/
DWORD CChunk::SubChunkNum (void)
{
   return m_dwSubChunks;
}


/*************************************************************************************
SubChunkInsert - Inserts a new (blank) chunk before the specified index number, and
then returns the chunk object.

inputs
   DWORD    dwIndex - chunk index
returns
   CChunk* - chunk pointer
*/

CChunk*  CChunk::SubChunkInsert (DWORD dwIndex)
{
   if (dwIndex > m_dwSubChunks)
      dwIndex = m_dwSubChunks;

   m_paSubChunks = (CChunk**) realloc (m_paSubChunks, (m_dwSubChunks+1) * sizeof(CChunk*));
   if (!m_paSubChunks)
      return NULL;

   memmove (m_paSubChunks + (dwIndex + 1), m_paSubChunks + dwIndex, m_dwSubChunks - dwIndex);
   m_dwSubChunks ++;

   m_paSubChunks[dwIndex] = new CChunk;
   if (m_paSubChunks[dwIndex])
      m_paSubChunks[dwIndex]->m_pChunkParent = this;

   FileDirty();

   return m_paSubChunks[dwIndex];
}

/*************************************************************************************
SubChunkAdd - Like Insert, but at the end

inputs
   DWORD    dwIndex - chunk index
returns
   CChunk* - chunk pointer
*/
CChunk*  CChunk::SubChunkAdd (void)
{
   return SubChunkInsert (m_dwSubChunks);
}

/*************************************************************************************
SubChunkFind - Searches sequentially through the subchunks to find one with the specified
   m_dwID

inputs
   DWORD    dwID - ID to look for
   DWORD    dwIndex - index to start at.
returns
   DWORD - Index found at, or -1 if cant find
*/
DWORD    CChunk::SubChunkFind (DWORD dwID, DWORD dwIndex)
{
   DWORD i;
   CChunk   *p;
   for (i = dwIndex; ; i++) {
      p = SubChunkGet (i);
      if (!p)
         return (DWORD) -1;
      if (p->m_dwID == dwID)
         return i;
   }

   return (DWORD) -1;
}

/*************************************************************************************
DataGet - Returns a pointer to the chunk's memory. Note that this pointer might change
if DataSet() is called. NOTE: do not change this memory because the system won't
know if it's been changed and won't recompress/encode the data.

inputs
   none
returns
   PVOID - memory. Might be NULL
*/
PVOID CChunk::DataGet (void)
{
   // IMPORTANT - if data was compressed or encoded the decompress & decode
   return m_pData;
}


/*************************************************************************************
DataSize - Returns the size of the data from DataGet.
*/
DWORD    CChunk::DataSize ()
{
   // IMPORTANT - if data was compressed or encoded the decompress & decode
   return m_dwDataSize;
}


/*************************************************************************************
DataSet - Writes new data into the chunks data.

inputs
   PVOID    pData - buffer to copy new data from
   DWORD    dwSize - number of bytes
*/
BOOL     CChunk::DataSet (PVOID pData, DWORD dwSize)
{
   if (m_pData)
      free (m_pData);
   m_pData = malloc (dwSize);
   if (!m_pData)
      return FALSE;

   memcpy (m_pData, pData, dwSize);
   m_dwDataSize = dwSize;

   FileDirty();

   return TRUE;
}


/*************************************************************************************
Write - Writes the chunk (and sub chunks) out to the file object.

inputs
   CEndFile *pFile - file object
   DWORD *pdwCompressedSize - Fills in the compressed size
returns
   BOOL - TRUE if OK, FALSE if error

TECHNICAL NOTE: Chunk Format...
   DWORD    dwSize - in bytes of the encoded/encrypted chunk data
   DWORD    dwID - chunk ID
   DWORD    dwCheckSum - Checksum of the chunk. So it the decrypt is pretty sure
               that the decryption worked
   DWORD    dwFlags - flags. Can be:
               CHUNKFLAG_COMPRESSED - If set then the chunk data is compressed
               CHUNKFLAG_ENCRYPTED - If set then the chunk data is encrypted.
   BYTE     abData[dwSize];
   DWORD    dwNumSubChunks - number of sub chunks
   contents of sub chunks, written out
*/
#define DONTCOMPRESS          30000000     // dont compress files larger than this, 6 MB
   // BUGFIX - Changed from 7MB to 30MB so will try to compress larger TTS voices

BOOL     CChunk::Write (CEncFile *pFile, DWORD *pdwCompressedSized)
{
   // IMPORTANT - right now assume that not encoded or encrypted

   DWORD dw;
   PVOID pCompressed = NULL;
   DWORD dwCompressedSize;

   // compress if necessary
   if (m_dwFlags & CHUNKFLAG_COMPRESSED) {
      if (gDontCompress || (m_dwDataSize >= DONTCOMPRESS) ) {
         pCompressed = NULL;
         dwCompressedSize = m_dwDataSize + 1;
      }
      else
         pCompressed = LZCompress (m_pData, m_dwDataSize, &dwCompressedSize);
      if (pdwCompressedSized)
         *pdwCompressedSized = dwCompressedSize;
      
      // if it's larger than the original then save uncompressed
      if (dwCompressedSize > m_dwDataSize) {
         if (pCompressed)
            free (pCompressed);
         pCompressed = NULL;
         m_dwFlags = m_dwFlags & (~CHUNKFLAG_COMPRESSED);
         if (pdwCompressedSized)
            *pdwCompressedSized = m_dwDataSize;
      }
   }

   // size
   dw = pCompressed ? dwCompressedSize : m_dwDataSize;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }
   
   // ID
   dw = m_dwID;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }
   
   // checksum
   // IMPORTANT - 0 for now
   dw = 0;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }

   // flags
   dw = m_dwFlags;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }

   // data
   if (!pFile->Write(pCompressed ? pCompressed : m_pData, pCompressed ? dwCompressedSize : m_dwDataSize)) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }
   if (pCompressed)
      free (pCompressed);

   // subchunks
   dw = m_dwSubChunks;
   if (!pFile->Write(&dw, sizeof(dw)))
      return FALSE;

   DWORD i;
   for (i = 0; i < m_dwSubChunks; i++) {
      DWORD dwCompressed, dwOrig;
      dwOrig = m_paSubChunks[i]->DataSize();
      if (!m_paSubChunks[i]->Write (pFile, &dwCompressed))
         return FALSE;

      printf ("Compressed %d to %d\n", dwOrig, dwCompressed);
   }

   // all done
   return TRUE;
}

/*************************************************************************************
Read - Reads the chunk (and sub chunks) out to the file object. If any data was already
   in the chunk then it's deleted.

inputs
   CEndFile *pFile - file object
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL     CChunk::Read (CEncFile *pFile)
{
   // IMPORTANT - right now assume that not encoded or encrypted

   DWORD dw;

   // clear out
   ClearOut ();

   // size
   if (!pFile->Read(&m_dwDataSize, sizeof(m_dwDataSize)))
      return FALSE;
   
   // ID
   if (!pFile->Read(&m_dwID, sizeof(m_dwID)))
      return FALSE;
   
   // checksum
   // IMPORTANT - ignoring for now
   if (!pFile->Read(&dw, sizeof(dw)))
      return FALSE;

   // flags
   if (!pFile->Read(&m_dwFlags, sizeof(m_dwFlags)))
      return FALSE;

   // data
   m_pData = malloc(m_dwDataSize);
   if (!m_pData)
      return FALSE;
   if (!pFile->Read(m_pData, m_dwDataSize))
      return FALSE;

   // if it's compressed then do something
   if (m_dwFlags & CHUNKFLAG_COMPRESSED) {
      PVOID pMem;
      DWORD dwSize;
      pMem = LZDecompress (m_pData, m_dwDataSize, &dwSize);
      if (m_pData)
         free (m_pData);
      m_pData = pMem;
      m_dwDataSize = dwSize;
   }

   // subchunks
   if (!pFile->Read(&m_dwSubChunks, sizeof(m_dwSubChunks)))
      return FALSE;
   m_paSubChunks = (CChunk**) malloc (m_dwSubChunks * sizeof(CChunk*));
   if (!m_paSubChunks)
      return FALSE;

   DWORD i;
   for (i = 0; i < m_dwSubChunks; i++) {
      m_paSubChunks[i] = new CChunk;
      if (!m_paSubChunks[i])
         return FALSE;
      if (!m_paSubChunks[i]->Read (pFile))
         return FALSE;
      m_paSubChunks[i]->m_pChunkParent = this;

   }

   // all done
   return TRUE;
}


/**********************************************************************************
CEncFile - functions
*/


/**********************************************************************************
Constructur & destructor */
CEncFile::CEncFile (void)
{
   m_f = NULL;
   m_pMem = NULL;
   m_dwMemSize = 0;
   m_dwMemCur = 0;
}

CEncFile::~CEncFile (void)
{
   Close ();
}

/**********************************************************************************
Open - opens a file for reading.

inputs
   char  *psz - file
returns
   BOOL - true if OK, FALSE if error
*/
BOOL CEncFile::Open (char *psz)
{

   if (m_f)
      return FALSE;  // already open

   m_f = fopen(psz, "rb");
   if (!m_f)
      return FALSE;

   // read in the header
   ENCHEADER   ec;
   if (1 != fread (&ec, sizeof(ec), 1, m_f)) {
      Close ();
      return FALSE;  // file not large enough
   }

   if (ec.dwMagic != MXAC) {
      Close ();
      return FALSE;  // wrong header
   }

   // encryption
   m_Encrypt.Init (ec.dwEncrypt);

   // that's it
   return TRUE;
}


/**********************************************************************************
Open - reads from memory instead of file

inputs
   void  *pMem - memopry
   DWORD dwSize - size
returns
   BOOL - true if OK, FALSE if error
*/
BOOL CEncFile::OpenMem (void *pMem, DWORD dwSize)
{
   if (m_f || m_pMem)
      return FALSE;  // already open

   m_pMem = pMem;
   m_dwMemSize = dwSize;
   m_dwMemCur = 0;

   // read in the header
   ENCHEADER   *pec;
   pec = (ENCHEADER*) pMem;
   m_dwMemCur = sizeof(ENCHEADER);

   if (pec->dwMagic != MXAC) {
      Close ();
      return FALSE;  // wrong header
   }

   // encryption
   m_Encrypt.Init (pec->dwEncrypt);

   // that's it
   return TRUE;
}


/**********************************************************************************
Create - Creates a file

inputs
   char  *psz - file
returns
   BOOL - true if ok, FALSE if error
*/
BOOL CEncFile::Create (char *psz)
{

   if (m_f)
      return FALSE;  // already open

   m_f = fopen(psz, "wb");
   if (!m_f)
      return FALSE;

   // write the header
   ENCHEADER   ec;
   ec.dwMagic = MXAC;
   MySRand();
   ec.dwEncrypt = MyRand();
   m_Encrypt.Init (ec.dwEncrypt);
   if (1 != fwrite (&ec, sizeof(ec), 1, m_f)) {
      Close ();
      return FALSE;  // file not large enough
   }

   // that's it
   return TRUE;
}

/******************************************************************************8
Close - closes the file
*/
BOOL CEncFile::Close (void)
{
   if (m_pMem)
      m_pMem = NULL;

   if (!m_f)
      return FALSE;

   fclose (m_f);
   m_f = NULL;

   return TRUE;
}

/******************************************************************************8
Write - Writes out the specfied data. If encryption is on it encrypts.

inputs
   PVOID pData - dat
   DWORD dwSize - size
returns
   BOOL - true if ok, FALSE if failed
*/
BOOL CEncFile::Write (PVOID pData, DWORD dwBytes)
{
   if (!m_f)
      return FALSE;

   // get the position
   DWORD dwPos;
   dwPos = ftell (m_f);

   // encrypt
   PVOID pTemp;
   pTemp = malloc(dwBytes);
   if (!pTemp)
      return FALSE;
   memcpy (pTemp, pData, dwBytes);
   // BUGFIX - Dont encrypt anymore: m_Encrypt.EncryptBuffer ((LPBYTE) pTemp, dwBytes, dwPos);

   if (dwBytes != fwrite (pTemp, 1, dwBytes, m_f)) {
      free (pTemp);
      return FALSE;
   }

   free (pTemp);

   return TRUE;
}

/******************************************************************************8
Read - Reads in the specfied data. If encryption is on it decrypts.

inputs
   PVOID pData - dat
   DWORD dwSize - size
returns
   BOOL - true if ok, FALSE if failed
*/
BOOL CEncFile::Read (PVOID pData, DWORD dwBytes)
{
   if (!m_f && !m_pMem)
      return FALSE;

   // get the position
   DWORD dwPos;
   dwPos = m_pMem ? m_dwMemCur : ftell (m_f);

   if (m_f) {
      if (dwBytes != fread (pData, 1, dwBytes, m_f))
         return FALSE;
   }
   else {
      if (dwBytes + m_dwMemCur > m_dwMemSize)
         return FALSE;
      memcpy (pData, (PBYTE) m_pMem + m_dwMemCur, dwBytes);
      m_dwMemCur += dwBytes;
   }

   // encrypt
   // BUGFIX: Don't encrypt anumore m_Encrypt.EncryptBuffer ((LPBYTE) pData, dwBytes, dwPos);

   return TRUE;
}

/******************************************************************************8
Seek - Seeks to the position within the file. Right after the header is location 0

inputs
   DWORD    dwLoc - location
returns
   BOOL - TRUE if ok, FALSE if error
*/
BOOL CEncFile::Seek (DWORD dwLoc)
{
   if (m_pMem) {
      m_dwMemCur = dwLoc;
      if (m_dwMemCur > m_dwMemSize)
         m_dwMemCur = m_dwMemSize;
      return TRUE;
   }

   if (!m_f)
      return FALSE;

   return !fseek(m_f, dwLoc + sizeof(ENCHEADER), SEEK_SET);
}


/******************************************************************************8
Tell - Returns the location in the file (excluding the header structure)
*/
DWORD CEncFile::Tell (void)
{
   if (m_pMem)
      return m_dwMemCur - sizeof(ENCHEADER);

   if (!m_f)
      return FALSE;
   
   return (DWORD) ftell (m_f) - sizeof(ENCHEADER);
}



/****************************************************************************8
BitRead - Takes memory and reads N bits from the memory.

inputs
   PVOID    pMem - memory
   DWORD    dwStart - Bit number (0 = start of mem) where to read from
   DWORD    dwBits - Number of bits to read. Max 32.
returns
   DWORD - Bits read. Filling dwBits worth (from low-order bit) up
*/
DWORD BitRead (PVOID pMem, DWORD dwStart, DWORD dwBits)
{
   DWORD    *pdw = (DWORD*) pMem;
   DWORD    dwVal = 0;

   // jump forward by DWORDs
   pdw += (dwStart / 32);
   dwStart = dwStart % 32;

   // if it all fits in the same DWORD, do one thing
   if (dwStart + dwBits <= 32) {
      dwVal = pdw[0] >> dwStart;
   }
   else {
      // else, 2 dwords
      dwVal = (pdw[0] >> dwStart) | (pdw[1] << (32 - dwStart));
   }

   // mask it off
   if (dwBits < 32)
      dwVal = dwVal & ((1 << dwBits) - 1);

   return dwVal;
}


/****************************************************************************8
BitWrite - Appends N bits onto memory.

inputs
   PVOID    *ppMem - Pointer to a PVOID with the memory allocated with malloc.
                        Need this because the memory may need to be realloced,
                        in which case this value will change.
   DWORD    *pdwAlloc - Pointer to a DWORD containing the number of bytes
                        allocated in *ppMem. If data is realloced then this
                        will be increased.
   DWORD    dwStart - Location (in bits) to write to
   DWORD    dwBits - Number of bits to write (max 32)
   DWORD    dwVal - Value to write. The non-valid high bits must be 0.
returns
   none
*/
void BitWrite (PVOID *ppMem, DWORD *pdwAlloc, DWORD dwStart, DWORD dwBits, DWORD dwVal)
{
   // make sure the memory is large enough
   DWORD dwNeed;
   dwNeed = (dwStart + dwBits + 32) / 8;
   if (*pdwAlloc < dwNeed) {
      dwNeed *= 2;   // BUGFIX - So don't do too many reallocs
      dwNeed += 256; // just to do a block at a time
      *ppMem = realloc(*ppMem, dwNeed);
      *pdwAlloc = dwNeed;
      if (!*ppMem)
         return;  // out of memory
   }

   // find the right place to add
   DWORD *pdw;
   pdw = (DWORD*) (*ppMem);
   pdw += (dwStart / 32);
   dwStart = dwStart % 32;

   DWORD dwMask;
   if (dwStart + dwBits <= 32) {
      // it all fits in this DWORD
      if (dwStart + dwBits == 32)
         dwMask = (DWORD) -1;
      else
         dwMask = ((1 << (dwStart + dwBits)) - 1);
      dwMask = dwMask & ~((1 << dwStart) - 1);

      pdw[0] = (pdw[0] & ~dwMask) | (dwVal << dwStart);
   }
   else {
      // it takes 2 DWORDS

      // the first mask is all high
      dwMask = (DWORD)-1 & ~((1 << dwStart) - 1);
      pdw[0] = (pdw[0] & ~dwMask) | (dwVal << dwStart);

      // the second mask is all low
      dwMask = (1 << (dwStart + dwBits - 32)) - 1;
      pdw[1] = (pdw[1] & ~dwMask) | (dwVal >> (32 - dwStart));
   }
}

/****************************************************************************8
LZDeconstructByteType - Desconstructs the byte table, converting tokens into bytes.

inputs
   PDWORD   pdwMap - an array of 8 DWORDS from the beginning of the LZ data.
   PBYTE    pbMap - Filled with a conversion from the token to the byte value.
               value = pbMap[token]
returns
   DWORD - number of different values actually seen.
*/
DWORD LZDeconstructByteTable (PDWORD pdwMap, PBYTE pbMap)
{
   memset (pbMap, 0, sizeof(pbMap));

   DWORD dwCount = 0;
   DWORD i, j, dw;
   for (j = 0; j < 8; j++) {
      dw = pdwMap[j];

      for (i = 0; i < 32; i++, dw >>= 1)
         if (dw & 0x01)
            pbMap[dwCount++] = (BYTE) (i + j * 32);
   }

   return dwCount;
}

/****************************************************************************8
LZConstructByteTable - Constructs a table indicating which bytes get mapped
   to which tokens. (For compression.) It does thsi by scanning through the document
   and learning how many of the 256 bytes are used. This leads to filling in a mapping
   table of byte -> token.

inputs
   PBYTE    pMem - memory to compress
   DWORD    dwSize - number of chars
   PBYTE    pbMap - an array of 256 bytes. Will be filled in with a mapping number
                     from 0..256, for what it gets mapped to. Unusued chars are set to 0.
   PDWORD   pdwMap - an array of 8 DWORDS. Filled with bit-values indicating if a particular
                     byte is used. pdwMap[1], bit 1 being on means char 33 is seen in pMem
returns
   DWORD - Number of different values actually seen.
*/
DWORD LZConstructByteTable (PBYTE pMem, DWORD dwSize, PBYTE pbMap, PDWORD pdwMap)
{
   memset (pbMap, 0, 256);
   memset (pdwMap, 0, 8 * sizeof(DWORD));

   // note which values appear
   DWORD i, j;
   for (i = 0; i < dwSize; i++, pMem++)
      pbMap[*pMem] = 1;

   // create the bit fields
   DWORD dwVal;
   DWORD dwCount;
   dwCount = 0;
   for (j = 0; j < 8; j++) {
      dwVal = 0;

      for (i = 31; i < 32; i--) {
         dwVal <<= 1;
         if (pbMap[i + j * 32])
            dwVal |= 1;
      }

      pdwMap[j] = dwVal;
   }

   // do conversion
   for (i = 0; i < 256; i++)
      if (pbMap[i])
         pbMap[i] = (BYTE) (dwCount++);

   // done
   return dwCount;
}

/****************************************************************************8
LZNumValuesToBits - Given the number of values seen in LZConstructByteTable,
   this returns the number of bits needed to represent them.

inputs
   DWORD dwSeen
returns
   DWORD - bits
*/
DWORD LZNumValuesToBits (DWORD dwSeen)
{
   DWORD dwBits;
   for (dwBits = 0; dwSeen; dwSeen >>= 1, dwBits++);

   return dwBits;
}

//#define  LZNEWBITS      6
#define  LZNEWBITS      6
#define  LZMAXNEW       ((1 << LZNEWBITS) - 1)

//#define  LZREPEATBITS   4     // number of bits to store repeat amount
#define  LZREPEATBITS   4     // number of bits to store repeat amount
#define  LZREPEATMIN    4     // don't bother doing a repeat unless it's this many chars
#define  LZREPEATMAX    ((1 << LZREPEATBITS) + LZREPEATMIN - 1)   // max number that can repeat

//#define  LZAGEBITS      12    // maximum age (# of chars past) that can be stored for repeats
#define  LZAGEBITS      12    // maximum age (# of chars past) that can be stored for repeats
   // BUGFIX - Was 15, but too slow
   //Unopt 15 = time=188340
   //Opt 15 = 169697, size=1468k
   //14 = time=107906, size=1480k
   //13 = time=47471, size=1500k
   //12 = time=24211, size=1533k

#define  LZAGEMAX       ((1 << LZAGEBITS) - 1)



// NEW=6,REPEAT=4,AGE=12 => ffinstall=1004

// NEW=7,REPEAT=4,AGE=12 => 1008k
// NEW=5,REPEAT=4,AGE=12 => ffinstall=1004
// NEW=4,REPEAT=4,AGE=12 => ffinstall=1008

// NEW=6,REPEAT=5,AGE=12 => ffinstall=1004
// NEW=6,REPEAT=6,AGE=12 => ffinstall=1012
// NEW=6,REPEAT=3,AGE=12 => ffinstall=1016

// NEW=6,REPEAT=4,AGE=11 => ffinstall=1028
// NEW=6,REPEAT=4,AGE=13 => ffinstall=988 // 16k savings
// NEW=6,REPEAT=4,AGE=14 => ffinstall=968 // 36k savings
// NEW=6,REPEAT=4,AGE=15 => ffinstall=924 // 80k savings - very slow <--- use this
// NEW=6,REPEAT=5,AGE=14 => ffinstall=968 // same as repeat=4
// NEW=6,REPEAT=4,AGE=15, smaller flyfox.exe => ffinstall=900 // 104k savings - very slow <--- use this

/****************************************************************************8
LZDecompReadNewBytes - Read in compressed data. The data is "new" bytes, meaning
   that it hasnt' appeared in the datastream before.

inputs
   PVOID    pComp - compressed memory
   DWORD    *pdwCompPosn - position within the compressed memory, in bits.
               This will be incremented
   PBYTE    *ppDecomp - decompressed memory. may be changed by call to realloc
   DWORD    *pdwDecompSize - amount allocated for ppDecomp. May be changed
   DWORD    *pdwDecompPosn - Position writing to in the decomp locaton. May be changed
   BYTE     *pabMap - Map from toke to BYTE
   DWORD    dwBitsPerToken - Bits per token

returns
   BOOL - TRUE if OK. FALSE if EOF found
*/
BOOL LZDecompressReadNewBytes (PVOID pComp, DWORD *pdwCompPosn, PBYTE *ppDecomp,
                               DWORD *pdwDecompSize, DWORD *pdwDecompPosn,
                               BYTE *pabMap, DWORD dwBitsPerToken)
{
   // assume that the first bit, containing 1, has already been read.

   // read in the number of tokens following
   DWORD dwNum;
   dwNum = BitRead (pComp, *pdwCompPosn, LZNEWBITS);
   *pdwCompPosn = *pdwCompPosn + LZNEWBITS;

   if (!dwNum)
      return FALSE;  // no more

   // make sure the memory is large enough for new values
   if (*pdwDecompPosn + dwNum >= *pdwDecompSize) {
      *pdwDecompSize = *pdwDecompPosn + dwNum + 256;
      *ppDecomp = (PBYTE) realloc (*ppDecomp, *pdwDecompSize);
      if (!*ppDecomp)
         return FALSE;
   }

   // read the tokens
   DWORD i, dw;
   for (i = 0; i < dwNum; i++) {
      dw = BitRead (pComp, *pdwCompPosn, dwBitsPerToken);
      *pdwCompPosn = *pdwCompPosn + dwBitsPerToken;

      (*ppDecomp)[*pdwDecompPosn] = pabMap[dw];
      *pdwDecompPosn = *pdwDecompPosn + 1;
   }
   
   // done
   return TRUE;
}

/****************************************************************************8
LZDecompReadRepeat - Read in compressed data. The data is repeated data.

inputs
   PVOID    pComp - compressed memory
   DWORD    *pdwCompPosn - position within the compressed memory, in bits.
               This will be incremented
   PBYTE    *ppDecomp - decompressed memory. may be changed by call to realloc
   DWORD    *pdwDecompSize - amount allocated for ppDecomp. May be changed
   DWORD    *pdwDecompPosn - Position writing to in the decomp locaton. May be changed

returns
   BOOL - TRUE if OK. FALSE if EOF found
*/
BOOL LZDecompressReadRepeat (PVOID pComp, DWORD *pdwCompPosn, PBYTE *ppDecomp,
                               DWORD *pdwDecompSize, DWORD *pdwDecompPosn)
{
   // assume that the first bit, containing 0, has already been read.

   // read in the number of tokens following
   DWORD dwNum;
   dwNum = BitRead (pComp, *pdwCompPosn, LZREPEATBITS);
   *pdwCompPosn = *pdwCompPosn + LZREPEATBITS;
   dwNum += LZREPEATMIN;

   // read in the age
   DWORD dwAge;
   dwAge = BitRead (pComp, *pdwCompPosn, LZAGEBITS);
   *pdwCompPosn = *pdwCompPosn + LZAGEBITS;

   // make sure the memory is large enough for new values
   if (*pdwDecompPosn + dwNum >= *pdwDecompSize) {
      *pdwDecompSize = *pdwDecompPosn + dwNum + 256;
      *ppDecomp = (PBYTE) realloc (*ppDecomp, *pdwDecompSize);
      if (!*ppDecomp)
         return FALSE;
   }

   // copy
   memmove (*ppDecomp + *pdwDecompPosn, *ppDecomp + (*pdwDecompPosn - dwAge), dwNum);
   *pdwDecompPosn = *pdwDecompPosn + dwNum;

   // done
   return TRUE;
}

/****************************************************************************8
LZCompLookForRepeat - Look for a repeat up to LZAGEMAX bytes previous. If find a repeat
   then see how long it is. Remember if >= LZREPEATMIN. Then, continue, looking for
   the longest repeat

inputs
   PBYTE    pSrc - source to look through
   DWORD    dwSize - number of bytes of data in source
   DWORD    dwCur - current position. Storing from that byte ownward
   DWORD    *pdwLargestRepeat - Filled with the largest repeat
returns
   DWORD    - repeat age found. or 0 if none long enough found
*/
#if 1 // optimized
__inline DWORD LZCompLookForRepeat (PBYTE pSrc, DWORD dwSize, DWORD dwCur, DWORD *pdwLargestRepeat)
{
   // remember the largest repeat
   DWORD dwLargestCount = LZREPEATMIN-1;
   DWORD dwLargestAge = 0  ;// not found

   // look
   DWORD i;
   int   iStart;
   DWORD dwIStart = LZREPEATMIN;
   iStart = (int) dwCur - (int)dwIStart;

   PBYTE pSrcdwCur = pSrc +dwCur;
   BYTE bSrcdwCur = *pSrcdwCur;
   PBYTE pSrciStart = pSrc + iStart;

   for (i = dwIStart; (i <= LZAGEMAX) && (iStart >= 0); i++, iStart--, pSrciStart--) {
      // position within file
      //iStart = (int) dwCur - i;
      //if (iStart < 0)
      //   break;   // out of bounds

      // trivial reject
      if (bSrcdwCur != *pSrciStart)
         continue;

      // longer test
      DWORD dwRepeat;
      int   iCur = iStart + 1 /*dwRepeat starts at 1*/;
      int iMax = min((int)dwCur, iStart + (int)LZREPEATMAX) ;
      PBYTE pSrcdwCurdwRepeat = pSrcdwCur + 1;  // since dwRepeat starts at 1
      PBYTE pSrciCur = pSrc + iCur;
      for (dwRepeat = 1; iCur < iMax /*(dwRepeat < LZREPEATMAX) && (iCur < (int)dwCur)*/; dwRepeat++, iCur++, pSrcdwCurdwRepeat++, pSrciCur++) {
         // BUGFIX - In optimization
         // don't go past end
         //iCur = iStart + (int) dwRepeat;
         //if (iCur >= (int) dwCur)
         //   break;   // limit of size

         // see if matches
         if (*pSrcdwCurdwRepeat != *pSrciCur) // (pSrc[dwCur+dwRepeat] != pSrc[iCur])
            break;
      }

      // how far did the repeat get
      if (dwRepeat <= dwLargestCount)
         continue;   // not large enough to matter

      // else, new length
      dwLargestCount = dwRepeat;
      dwLargestAge = dwCur - (DWORD) iStart;
   }

   *pdwLargestRepeat = dwLargestCount;
   return dwLargestAge;
}

#endif // optimized


#if 0 // unoptimized version
__inline DWORD LZCompLookForRepeat (PBYTE pSrc, DWORD dwSize, DWORD dwCur, DWORD *pdwLargestRepeat)
{
   // remember the largest repeat
   DWORD dwLargestCount = LZREPEATMIN-1;
   DWORD dwLargestAge = 0  ;// not found

   // look
   DWORD i;
   int   iStart;
   for (i = LZREPEATMIN; i <= LZAGEMAX; i++) {
      // position within file
      iStart = (int) dwCur - i;
      if (iStart < 0)
         break;   // out of bounds

      // trivial reject
      if (pSrc[dwCur] != pSrc[iStart])
         continue;

      // longer test
      DWORD dwRepeat;
      int   iCur;
      for (dwRepeat = 1; dwRepeat < LZREPEATMAX; dwRepeat++) {
         // don't go past end
         iCur = iStart + (int) dwRepeat;
         if (iCur >= (int) dwCur)
            break;   // limit of size

         // see if matches
         if (pSrc[dwCur+dwRepeat] != pSrc[iCur])
            break;
      }

      // how far did the repeat get
      if (dwRepeat <= dwLargestCount)
         continue;   // not large enough to matter

      // else, new length
      dwLargestCount = dwRepeat;
      dwLargestAge = dwCur - (DWORD) iStart;
   }

   *pdwLargestRepeat = dwLargestCount;
   return dwLargestAge;
}
#endif // 0

/****************************************************************************8
LZCompWriteRepeat - Write in compressed data. Note that the data is a marker
   that previous data that has appeared was repeated.

inputs
   DWORD    dwCount - Repeat count
   DWORD    dwAge - Age of the repeat
   PVOID    *ppMem - To write compressed data into. May be changed on relloc
   DWORD    *pdwMemSize - Size of memory in bytes. May be increased
   DWORD    *pdwCurBit - Current bit writing to. Will be increased.
returns
   none
*/
void LZCompWriteRepeat (DWORD dwCount, DWORD dwAge, PVOID *ppMem,
                          DWORD *pdwMemSize, DWORD *pdwCurBit)
{
   // write a 0 bit indicates a repeat
   // of previous data.
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, 1, 0);
   *pdwCurBit = *pdwCurBit + 1;

   // write out the number of bytes data represented
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, LZREPEATBITS, dwCount - LZREPEATMIN);
   *pdwCurBit = *pdwCurBit + LZREPEATBITS;

   // write the number of bytes age
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, LZAGEBITS, dwAge);
   *pdwCurBit = *pdwCurBit + LZAGEBITS;

   // done
}



/****************************************************************************8
LZCompWriteNewBytes - Write in compressed data. Note that the data is "new"
   bytes, meaning they haven't appeared in the datastream before.

inputs
   PBYTE    pabWrte - to write
   DWORD    dwWriteSize - number of bytes. This is limited by LZMAXNEW.
            If this is 0 then EOF is written.
   PVOID    *ppMem - To write compressed data into. May be changed on relloc
   DWORD    *pdwMemSize - Size of memory in bytes. May be increased
   DWORD    *pdwCurBit - Current bit writing to. Will be increased.
   BYTE     *pabMap - Map from BYTE to token
   DWORD    dwBitsPerToken - Bits per token
returns
   none
*/
void LZCompWriteNewBytes (PBYTE pabWrite, DWORD dwWriteSize, PVOID *ppMem,
                          DWORD *pdwMemSize, DWORD *pdwCurBit,
                          BYTE *pabMap, DWORD dwBitsPerToken)
{
   // write a 1 bit indicating that it's new bytes. a 0 bit indicates a repeat
   // of previous data.
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, 1, 1);
   *pdwCurBit = *pdwCurBit + 1;

   // write out the number of bytes data represented
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, LZNEWBITS, dwWriteSize);
   *pdwCurBit = *pdwCurBit + LZNEWBITS;

   // write other data
   DWORD i;
   for (i = 0; i < dwWriteSize; i++) {
      BitWrite (ppMem, pdwMemSize, *pdwCurBit, dwBitsPerToken,
         pabMap[pabWrite[i]]);
      *pdwCurBit = *pdwCurBit + dwBitsPerToken;
   }

   // done
}


/****************************************************************************8
LZDecompress - Reads in compressed data and decompressed

inputs
   PVOID    pComp - compressed memory to decompress
   DWORD    dwCompSize - size in bytes of compressed memory
   DWORD    *pdwDecompSize - filled with the size of the decompressed memry
returns
   PVOID - filled with decompressed memory
*/
PVOID LZDecompress (PVOID pComp, DWORD dwCompSize, DWORD *pdwDecompSize)
{
   PVOID pNew;
   // if it's less than the minimum size can manage then just write out
   if (dwCompSize <= 9 * sizeof(DWORD)) { // BUGFIX - was 8
      *pdwDecompSize = dwCompSize;
      pNew = malloc (dwCompSize);
      if (pNew)
         memcpy (pNew, pComp, dwCompSize);
      return pNew;
   }

   // else data to decompress
   DWORD dwCompCur;
   dwCompCur = 0;

   // read out the final size
   DWORD dwFinalSize = 0;
   dwFinalSize = BitRead (pComp, dwCompCur, 32);
   dwCompCur += 32;

   // load in the byte/token table
   DWORD i, dwBitsPerToken, dwCount;
   DWORD adwMap[8];
   BYTE  abMap[256];
   for (i = 0; i < 8; i++) {
      adwMap[i] = BitRead (pComp, dwCompCur, 32);
      dwCompCur += 32;
   }
   dwCount = LZDeconstructByteTable (adwMap, abMap);
   dwBitsPerToken = LZNumValuesToBits (dwCount);

   // memory
   PVOID pDecomp;
   DWORD dwDecompSize, dwDecompCur;
   dwDecompSize = dwCompSize * 2;
   dwDecompCur = 0;
   pDecomp = malloc (dwDecompSize);
   if (!pDecomp)
      return NULL;

   // go for it
   DWORD dw;
   while (TRUE) {
      // read in a bit
      dw = BitRead (pComp, dwCompCur, 1);
      dwCompCur++;

      if (dw) {
         // its 1, which means new byte pattern
         if (!LZDecompressReadNewBytes (pComp, &dwCompCur, (PBYTE*) &pDecomp,
                                  &dwDecompSize, &dwDecompCur,
                                  abMap, dwBitsPerToken))
                                  break;  // no more data
      }
      else {
         // it's 0, wheich means repeat
         LZDecompressReadRepeat (pComp, &dwCompCur, (PBYTE*) &pDecomp,
                                  &dwDecompSize, &dwDecompCur);
      }

   }

   // all done
   *pdwDecompSize = min(dwFinalSize, dwDecompCur);
      // BUGFIX - Store final size
   return pDecomp;

}

/****************************************************************************8
LZCompress - Compresses data.

inputs
   PVOID    pMem - to compress
   DWORD    dwSize - in bytes
   DWORD    *pdwCompressSize - Filled with the size (in bytes) of compressed data
returns
   PVOID - Memory that compressed to. Must be free()-ed.
*/
PVOID LZCompress (PVOID pMem, DWORD dwSize, DWORD *pdwCompressSize)
{
   PVOID pNew;
   // if it's less than the minimum size can manage then just write out
   if (dwSize <= 9 * sizeof(DWORD)) {  // BUGFIX - was 8
      *pdwCompressSize = dwSize;
      pNew = malloc (dwSize);
      if (pNew)
         memcpy (pNew, pMem, dwSize);
      return pNew;
   }

   // construct the byte table
   DWORD dwCount, dwBitsPerToken;
   BYTE  abMap[256];
   DWORD adwMapBits[8];
   dwCount = LZConstructByteTable ((PBYTE) pMem, dwSize, abMap, adwMapBits);
   dwBitsPerToken = LZNumValuesToBits (dwCount);

   // start out with blank memory
   DWORD dwNewSize;
   DWORD dwCurBit;
   dwNewSize = dwSize / 2; // as a rough guestimate
   pNew = malloc (dwNewSize + sizeof(DWORD));
   dwCurBit = 0;

   // BUGFIX - write out the final size
   BitWrite (&pNew, &dwNewSize, dwCurBit, 32, dwSize);
   dwCurBit += 32;

   // write it out
   DWORD i;
   for (i = 0; i < 8; i++) {
      BitWrite (&pNew, &dwNewSize, dwCurBit, 32, adwMapBits[i]);
      dwCurBit += 32;
   }

   // compress
   DWORD dwCurByte;
   DWORD dwLastDot = 0;
   for (dwCurByte = 0; dwCurByte < dwSize; ) {
      // BUGFIX - Display dots so know progress is happening
      DWORD dwThisDot = dwCurByte / (256 * 256 * 8);  // 500K
      if (dwThisDot != dwLastDot) {
         dwLastDot = dwThisDot;
         printf (".");
      }

      // see how far can go before find a repeat to use
      DWORD i;
      DWORD dwAge, dwRepeat;
      for (i = 0; i < LZMAXNEW; i++) {
         dwAge = 0;
         if (dwCurByte + i >= dwSize)
            break;   // can't go further

         dwAge = LZCompLookForRepeat ((PBYTE) pMem, dwSize, dwCurByte + i, &dwRepeat);
         if (dwAge)
            break;   // found
      }
      
      // write out the non-repeated data
      if (i) {
         LZCompWriteNewBytes ((PBYTE) pMem + dwCurByte, i,
            &pNew, &dwNewSize, &dwCurBit,
            abMap, dwBitsPerToken);

         dwCurByte += i;
      }

      // if we have a repeat then write that out
      if (dwAge) {
         LZCompWriteRepeat (dwRepeat, dwAge,
            &pNew, &dwNewSize, &dwCurBit);

         dwCurByte += dwRepeat;
      }

   }


   // terminating
   LZCompWriteNewBytes ((PBYTE) pMem + dwCurByte, 0,
      &pNew, &dwNewSize, &dwCurBit,
      abMap, dwBitsPerToken);


   // done
   *pdwCompressSize = (dwCurBit + 7) / 8;
   return pNew;
}




#if 0 // NEW COMPRESSION code that doesnt work as well as old

/*****************************************************************************
NEW COMPRESSION CODE TO TRY */


// pattern header
typedef struct {
   DWORD       dwRemapIndex;  // where remaps start (was dwMax in PatternEncode)
   DWORD       dwNumOrig;     // number of original data points
   DWORD       dwNumNew;      // new data points (DWORDs)
   DWORD       dwConvert;     // number of conversions, each one is 2 DWORDs
} PATTERNHDR, *PPATTERNHDR;


/* CMem - Memory object */
class CMem {
   public:
      PVOID    p;       // memory. Allocated to size m_dwAllocated
      DWORD    m_dwAllocated;   // amount of memory allocated
      DWORD    m_dwCurPosn;     // current position

      CMem (void);
      ~CMem (void);

      BOOL  Required (DWORD dwSize);
      DWORD Malloc (DWORD dwSize);
      BOOL  StrCat (PCWSTR psz, DWORD dwCount = (DWORD)-1);   // if count==-1 then uses a strlen
      BOOL  CharCat (WCHAR c);
   };

typedef CMem * PCMem;


/* CListFixed - List of fixed sized elements*/
class CListFixed {
private:
   DWORD    m_dwElemSize;
   CMem     m_apv;      // array of elements. Size = m_dwElemSize
   DWORD    m_dwElem;   // number of elements in list

public:
   CListFixed (void);
   ~CListFixed (void);

   BOOL     Init (DWORD dwElemSize);
   BOOL     Init (DWORD dwElemSize, PVOID paElems, DWORD dwElems);

   DWORD    Add (PVOID pMem);
   BOOL     Insert (DWORD dwElem, PVOID pMem);
   BOOL     Set (DWORD dwElem, PVOID pMem);
   BOOL     Remove (DWORD dwElem);
   DWORD    Num (void);
   PVOID    Get (DWORD dwElem);
   void     Clear (void);
   BOOL     Merge (CListFixed *pMerge);
   BOOL     Truncate (DWORD dwElems);
};
typedef CListFixed * PCListFixed;



#define  MEMEXTRA          128      // memory to allocate above and beyond whats asked for
// BUGFIX - upped memextra because changed realloc code

// #define  MEMEXTRA          32      // memory to allocate above and beyond whats asked for
// BUGFIX - Lowered to 32 to save memory
// BUGFIX - changed from 128 to 64 because when was 128 crashed when loading a large datafile for house app. Seems like memory overrun someplace

typedef struct {
   PVOID    pMem;
   DWORD    dwSize;
} LISTINFO, *PLISTINFO;

/****************************************************************
CMem - memory object */

CMem::CMem (void)
{
   p = NULL;
   m_dwAllocated = m_dwCurPosn = 0;
}

CMem::~CMem (void)
{
   if (p)
      free (p);
}



/***********************************************************8
CListFixed - List of fixed size elements
*/
CListFixed::CListFixed (void)
{
   m_dwElemSize = 1;
   m_dwElem = 0;
}

CListFixed::~CListFixed (void)
{
   // do nothing
}

BOOL     CListFixed::Init (DWORD dwElemSize)
{
   m_dwElemSize = dwElemSize;
   m_dwElem = 0;

   return TRUE;
}

BOOL     CListFixed::Init (DWORD dwElemSize, PVOID paElems, DWORD dwElems)
{
   m_dwElemSize = dwElemSize;
   if (!m_apv.Required (dwElems * dwElemSize))
      return FALSE;

   m_dwElem = dwElems;
   memcpy (m_apv.p, paElems, m_dwElem * m_dwElemSize);
   return TRUE;
}


BOOL     CListFixed::Insert (DWORD dwElem, PVOID pMem)
{
   if (dwElem > m_dwElem)
      dwElem = m_dwElem;

   if (!m_apv.Required (m_dwElemSize * (m_dwElem+1)))
      return FALSE;

   // move memory
   memmove ((PBYTE)m_apv.p + (dwElem+1) * m_dwElemSize,
      (PBYTE)m_apv.p + dwElem * m_dwElemSize, (m_dwElem - dwElem) * m_dwElemSize);

   // put in new stuff
   memcpy ((PBYTE)m_apv.p + dwElem*m_dwElemSize, pMem, m_dwElemSize);

   m_dwElem++;

   return TRUE;
}

/* Truncate - cut off the end elements */
BOOL     CListFixed::Truncate (DWORD dwElems)
{
   m_dwElem = min(m_dwElem, dwElems);
   return TRUE;
}

DWORD    CListFixed::Add (PVOID pMem)
{
   DWORD dw;
   dw = m_dwElem;

   if (!Insert (dw, pMem))
      return (DWORD)-1;
   return dw;
}

BOOL    CListFixed::Set (DWORD dwElem, PVOID pMem)
{
   if (dwElem >= m_dwElem)
      return FALSE;

   memcpy ((PBYTE)m_apv.p + dwElem*m_dwElemSize, pMem, m_dwElemSize);

   return TRUE;
}

BOOL     CListFixed::Remove (DWORD dwElem)
{
   if (dwElem >= m_dwElem)
      return FALSE;

   PBYTE pi;
   pi = (PBYTE) m_apv.p + dwElem * m_dwElemSize;

   memmove (pi, pi + m_dwElemSize, (m_dwElem - dwElem - 1) * m_dwElemSize);
   m_dwElem--;

   return TRUE;
}

DWORD    CListFixed::Num (void)
{
   return m_dwElem;
}

/* note - return is only valid until add/insert/remove an item*/
PVOID    CListFixed::Get (DWORD dwElem)
{
   if (dwElem >= m_dwElem)
      return NULL;

   return (PVOID) ((PBYTE)m_apv.p + m_dwElemSize * dwElem);
}

void     CListFixed::Clear (void)
{
   m_dwElem = 0;
}



/* merge - mergest pMerge onto the end of the current list. pMerge
is then left with nothing. */
BOOL CListFixed::Merge (CListFixed *pMerge)
{
   if (m_dwElemSize != pMerge->m_dwElemSize)
      return FALSE;

   // needed
   if (!m_apv.Required (m_dwElemSize * (m_dwElem + pMerge->m_dwElem)))
      return FALSE;

   // copy
   memcpy ( ((PBYTE) m_apv.p) + m_dwElem * m_dwElemSize, pMerge->m_apv.p, m_dwElemSize*pMerge->m_dwElem);
   m_dwElem += pMerge->m_dwElem;
   pMerge->m_dwElem = 0;

   return TRUE;
}


BOOL CMem::Required (DWORD dwSize)
{
   if (dwSize <= m_dwAllocated)
      return TRUE;

   DWORD dwNeeded;
   // BUGFIX - Changed so that the first time it matches exactly. After
   // that it does incremental increases
   // BUGFIX - So it's not too slow, if it's the second time around always
   // allocate 50% more
   dwNeeded = dwSize + (p ? (dwSize / 2 + MEMEXTRA) : 0);

   // BUGFIX - Try to allocate on 32-byte boundaries. Hope that this will crash
   // a memory overwrite (or maybe bug in realloc) that's causing a crash
   // when realloc is called. Have spent much time stepping through the code and
   // can't find overwritten memory. I don't think this worked but I'll leave it in anyway
   DWORD dwMore;
   dwMore = dwNeeded % 32;
   if (dwMore && p)
      dwNeeded += 32 - dwMore;

   PVOID p2;
   // BUGBUG - Temporary hack to prevent crashes that seem to occur when I use
   // realloc. They don't happen when I use malloc.
   p2 = malloc (dwNeeded);
   if (!p2)
      return FALSE;
   if (p) {  
      memcpy (p2, p, m_dwAllocated);
#ifdef _DEBUG
      memset (p, 0xdfdfdfdf, m_dwAllocated);
#endif
      free (p);
   }
   // old code
//   p2 = MYREALLOC (p, dwNeeded);

   if (!p2)
      return FALSE;

   p = p2;
   m_dwAllocated = dwNeeded;

   return TRUE;
}

/* returns an offset from CMem.p */
DWORD CMem::Malloc (DWORD dwSize)
{
   DWORD dwNeeded, dwStart;
   // BUGFIX - Changed "% ~0x03" to "& 0x03"
   dwStart = (m_dwCurPosn + 3) & ~0x03; // DWORD align
   dwNeeded = dwStart + dwSize;
   if (!Required (dwNeeded))
      return NULL;

   m_dwCurPosn = dwNeeded;

   return dwStart;
}

/* Add a string to the dwCur. Don't do terminating NULL though */
BOOL CMem::StrCat (PCWSTR psz, DWORD dwCount)
{
   if (dwCount == (DWORD)-1) {
      dwCount = wcslen(psz);
   }

   DWORD dwNeeded;
   dwNeeded = m_dwCurPosn + dwCount * sizeof(WCHAR);
   if (!Required(dwNeeded))
      return FALSE;

   // else ad
   memcpy ((PBYTE) p + m_dwCurPosn, psz, dwCount * sizeof(WCHAR));
   m_dwCurPosn += dwCount * sizeof(WCHAR);

   return TRUE;
}

/* Add a character */
BOOL CMem::CharCat (WCHAR c)
{
   return StrCat (&c, 1);
}


/*******************************************************************************
HuffmanEncode - Does a simple huffman encoding by splitting the number into
7-bit slices and using the high bit of a byte to indicate if there's more. 0x00 =
no more data, 0x80 = more data

inputs
   PVOID          pOrigMem - Memory to encode
   DWORD          dwNum - Number of elements
   DWORD          dwElemSize - Element size (in bytes, either 2, or 4, or 8). No point encoding 1
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
returns
   DWORD - 0 if OK, 1 if error
*/
DWORD HuffmanEncode (PBYTE pOrigMem, DWORD dwNum, DWORD dwElemSize, PCMem pMem)
{
   // write out the number of types
   DWORD dwSizeIndex = pMem->m_dwCurPosn;
   if (!pMem->Required(pMem->m_dwCurPosn + sizeof(DWORD)))
      return 1;
   pMem->m_dwCurPosn += sizeof(DWORD);

   // loop
   DWORD dwVal, i;
   PBYTE pCur;
   for (i = 0; i < dwNum; i++, pOrigMem += dwElemSize) {
      // make sure at least have 5 bytes
      if (!pMem->Required(pMem->m_dwCurPosn + 15))
         return 1;
      pCur = (PBYTE) pMem->p + pMem->m_dwCurPosn;

      if (dwElemSize == 8) {
         unsigned __int64 qw;
         qw = *((unsigned __int64 *) pOrigMem);

         // NOTE: being laszy here and not subtracting

         DWORD dwNumBytes, j;
         for (dwNumBytes = 0; !dwNumBytes || qw; qw >>= 7, dwNumBytes++)
            pCur[dwNumBytes] = ((BYTE) qw & 0x7f) | (dwNumBytes ? 0 : 0x80);

         // flip
         BYTE bTemp;
         for (j = 0; j < dwNumBytes/2; j++) {
            bTemp = pCur[j];
            pCur[j] = pCur[dwNumBytes-j-1];
            pCur[dwNumBytes-j-1] = bTemp;
         }
         pCur += dwNumBytes;
         pMem->m_dwCurPosn += dwNumBytes;
      }
      else {
         if (dwElemSize == 4)
            dwVal = *((DWORD*) pOrigMem);
         else
            dwVal = *((WORD*) pOrigMem);

         // write out
         if (dwVal < (1<<7)) {
            *pCur = (BYTE)(dwVal | 0x80);
            pMem->m_dwCurPosn += 1;
         }
         else if (dwVal < (1<<7) + (1<<14)) {
            dwVal -= (1 << 7);
            *(pCur++) = (BYTE) (dwVal >> 7) & 0x7f;
            *pCur = (BYTE) (dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 2;
         }
         else if (dwVal < (1<<7) + (1<<14) + (1 << 21)) {
            dwVal -= ((1 << 7) + (1<<14));
            *(pCur++) = (BYTE)(dwVal >> 14) & 0x7f;
            *(pCur++) = (BYTE)(dwVal >> 7) & 0x7f;
            *pCur = (BYTE) (dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 3;
         }
         else if (dwVal < (1<<7) + (1<<14) + (1 << 21) + (1 << 28)) {
            dwVal -= ((1 << 7) + (1<<14) + (1<<21));
            *(pCur++) = (BYTE)(dwVal >> 21) & 0x7f;
            *(pCur++) = (BYTE)(dwVal >> 14) & 0x7f;
            *(pCur++) = (BYTE)(dwVal >> 7) & 0x7f;
            *pCur = (BYTE)(dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 4;
         }
         else {   // all 5 bytes
            dwVal -= ((1 << 7) + (1<<14) + (1<<21) + (1 << 28));
            *(pCur++) = (BYTE)(dwVal >> 28) & 0x7f;
            *(pCur++) = (BYTE)(dwVal >> 21) & 0x7f;
            *(pCur++) = (BYTE)(dwVal >> 14) & 0x7f;
            *(pCur++) = (BYTE)(dwVal >> 7) & 0x7f;
            *pCur = (BYTE)(dwVal & 0x7f) | 0x80;
            pMem->m_dwCurPosn += 5;
         }
      }  // 2 or 4 byte
   }

   // fill in the size
   DWORD *pdw;
   pdw = (DWORD*) ((BYTE*) pMem->p + dwSizeIndex);
   *pdw = dwNum;  //pMem->m_dwCurPosn - (dwSizeIndex + sizeof(DWORD));

   return 0;
}

/*******************************************************************************
HuffmanDecode - Decodes the encoding from above

inputs
   PBYTE          pHuff - Huffman encoded
   DWORD          dwSize - Number of bytes left in all the memory
   DWORD          dwElemSize - Element size. 2 = word, or 4 = dword, 8=qword
   PCMem          pMem - Filled with the elements. pMem->m_dwCurPosn set to #elems x dwElemSize
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD HuffmanDecode (PBYTE pHuff, DWORD dwSize, DWORD dwElemSize,
                     PCMem pMem, DWORD *pdwUsed)
{
   *pdwUsed = 0;
   DWORD dwOrigSize = dwSize;
   pMem->m_dwCurPosn = 0;

   // get the first DWORD to indicate how many elem
   DWORD dwNum;
   if (dwSize < sizeof(DWORD))
      return 2;
   dwNum = *((DWORD*) pHuff);
   pHuff += sizeof(DWORD);
   dwSize -= sizeof(DWORD);
   if (!pMem->Required (dwNum * dwElemSize))
      return 2;
   pMem->m_dwCurPosn = dwNum * dwElemSize;

   // fill it in
   DWORD i, dwVal, dwOffset;
   unsigned __int64 qwVal;
   for (i = 0; i < dwNum; i++) {
      if (dwElemSize == 8) {
         qwVal = 0;

         while (TRUE) {
            if (!dwSize)
               return 2;   // error
            qwVal = (qwVal << 7) | (pHuff[0] & 0x7f);
            if (pHuff[0] & 0x80) {
               pHuff++;
               dwSize--;
               break;
            }

            pHuff++;
            dwSize--;
         }

         // write the value out
         ((unsigned __int64*) pMem->p)[i] = qwVal;
      }
      else {   // elem size is 4 or 2
         // get bytes until have high bit
         dwVal = dwOffset = 0;
         while (TRUE) {
            if (!dwSize)
               return 2;   // error
            dwVal = (dwVal << 7) | (pHuff[0] & 0x7f);
            if (pHuff[0] & 0x80) {
               pHuff++;
               dwSize--;
               dwVal += dwOffset;
               break;
            }

            pHuff++;
            dwSize--;
            dwOffset = (dwOffset << 7) + (1 << 7);
         }

         // write the value out
         if (dwElemSize == 4)
            ((DWORD*) pMem->p)[i] = dwVal;
         else
            ((WORD*) pMem->p)[i] = (WORD) dwVal;
      }
   }

   // done
   *pdwUsed = dwOrigSize - dwSize;
   return 0;
}


/*******************************************************************************
PatternEncode - Encodes a sequence by finding repeating pairs of elements and
combing them together. Repeat until no more repeats.

inputs
   PVOID          pOrigMem - Memory to encode
   DWORD          dwNum - Number of elements
   DWORD          dwElemSize - Element size (in bytes, either 1, 2, or 4)
   PCMem          pMem - Memory to write token sequence. Just keep on adding
                  on to m_dwCurPosn.
returns
   DWORD - 0 if OK, 1 if error
*/

static int _cdecl PatternSort (const void *elem1, const void *elem2)
{
   DWORD *p1, *p2;
   p1 = *((DWORD**) elem1);
   p2 = *((DWORD**) elem2);


   // higher numbers earlier
   if (p1[0] > p2[0])
      return -1;
   else if (p1[0] < p2[0])
      return 1;

   // else compare next
   if (p1[1] > p2[1])
      return -1;
   else if (p1[1] < p2[1])
      return 1;
   else
      return 0;   // dont carea
}

DWORD PatternEncode (PBYTE pOrigMem, DWORD dwNum, DWORD dwElemSize, PCMem pMem)
{
   DWORD dwOrigNum = dwNum;

   // create memory large enough
   CMem memData;     // data, converted to DWORD blocks
   CMem memPointer;  // pointer to data
   CListFixed lConvert; // list of conversions. Which is a list of 2 DWORDs
   if (!memData.Required ((dwNum+1) * sizeof(DWORD)))
      return 1;
   if (!memPointer.Required (dwNum * sizeof(DWORD*)))
      return 1;
   lConvert.Init (2 * sizeof(DWORD));

   DWORD *padwData;
   DWORD **ppData;
   padwData = (DWORD*) memData.p;
   ppData = (DWORD**) memPointer.p;

   // transfer over
   DWORD i, dwVal, dwMax;
   dwMax = 0;
   for (i = 0; i < dwNum; i++) {
      if (dwElemSize == 4)
         dwVal = *((DWORD*)(pOrigMem + i * sizeof(DWORD)));
      else if (dwElemSize == 2)
         dwVal = *((WORD*)(pOrigMem + i * sizeof(WORD)));
      else
         dwVal = *((BYTE*)(pOrigMem + i * sizeof(BYTE)));


      // keep the max
      dwMax = max(dwMax, dwVal);

      // store away
      padwData[i] = dwVal;
   }
   padwData[dwNum] = -1;   // EOF
   memData.m_dwCurPosn = dwNum * sizeof(DWORD);
   dwMax++;


   DWORD j, k, dwMatch, dwConvert, dwMatches, dwSortedAt;
   while (TRUE) {
      // if there aren't any entries then dont add anything
      if (!dwNum)
         break;

      // create all pointers in sequence so will be able to sort
      for (i = 0; i < dwNum-1; i++)
         ppData[i] = &padwData[i];

      // sort this - Sort 1 less because the element can never be recombined at
      // the beginning of a sequence since it's at the end of the file
      qsort (ppData, dwNum-1, sizeof(DWORD*), PatternSort);

      dwMatches = 0;
      dwSortedAt = lConvert.Num() + dwMax;

      // loop from start to finish
      for (i = 0; i < dwNum-1; i++) {
         // if this node has been changed then skip
         // NOTE: Dont need to do the following because of dwSortedAt check
         //if ( ((ppData[i])[0] == -1) || ((ppData[i])[1] == -1))
         //   continue;

         // if just changed this this go around then don't process

         if ( ((ppData[i])[0] >= dwSortedAt) || ((ppData[i])[1] >= dwSortedAt))
            continue;

         // loop ahead and find out how many matches there are
         dwMatch = 0;
         for (j = i + 1; j < dwNum-1; j++) {
            // if this node has been changed then skip
            if ( ((ppData[j])[0] == -1) || ((ppData[j])[1] == -1))
               continue;

            if ( ((ppData[i])[0] != (ppData[j])[0]) || ((ppData[i])[1] != (ppData[j])[1]) )
               break;   // they're not the same so have come to the end of the sort

            // else, the same
            dwMatch++;
         }  // over j

         // if there weren't enough matches then just skip this section and move on
         // BUGFIX - Chouse this number because reduced the size the most
         if (dwMatch < 3) {   // 4 or more. since dwMatch doens't include first one.
            i = j-1; // skip far enough ahead
            continue;
         }

         // else, compress these
         DWORD adw[2];
         DWORD dwNum;
         BOOL fAdd;
         if (dwMax > 0x7ffffffe - lConvert.Num())
            break;   // too much data
         dwNum = lConvert.Num();
         dwConvert = dwNum + dwMax;
         adw[0] = (ppData[i])[0];
         adw[1] = (ppData[i])[1];

         // BUGFIX - Check to see if it's the same as the one immediately previous
         fAdd = TRUE;
         if (dwNum) {
            DWORD *padwC = (DWORD*) lConvert.Get(0);
            DWORD *padw;
            for (k = dwNum-1; k < dwNum; k--) {
               padw = padwC + (k*2);
               if ((padw[0] == adw[0]) && (padw[1] == adw[1])) {
                  fAdd = FALSE;
                  dwConvert = dwMax + k;
                  break;
               }
            }
         }

         if (fAdd)
            lConvert.Add (adw);  // add the 2 dwords

         // loop through again
         k = j;
         for (j = i; j < k; j++) {
            if ( (adw[0] != (ppData[j])[0]) || (adw[1] != (ppData[j])[1]) )
               continue;   // they're not the same so have come to the end of the sort

            // else, the same
            (ppData[j])[0] = dwConvert;
            (ppData[j])[1] = -1; // so know its blank
         }  // over j


         // note that had a match and conversion here
         dwMatches++;

         // skip ahead
         i = k-1;
      }  // over i

      // go back over the data and get rid of the -1's
      for (i = j = 0; i < dwNum; i++) {
         if (padwData[i] == -1)
            continue;   // should be removed
         if (i != j)
            padwData[j] = padwData[i];
         j++;
      }
      dwNum = j;
      padwData[dwNum] = -1;

      // if there weren't any matches then quick
      if (!dwMatches)
         break;
   }  // repeat resort

   // what's left is a list of data in padwData[], with higher values being remaps
   // and a list of remaps in lConvert

   // how much space is needed?
   DWORD dwNeed;
   dwNeed = sizeof(PATTERNHDR) + dwNum * sizeof(DWORD) + lConvert.Num() * 2 * sizeof(DWORD);
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return 1;

   PPATTERNHDR pph;
   DWORD *padwCompress, *padwConvert;
   pph = (PPATTERNHDR) ((PBYTE)pMem->p + pMem->m_dwCurPosn);
   padwCompress = (DWORD*) (pph+1);
   padwConvert = padwCompress + dwNum;
   pMem->m_dwCurPosn += dwNeed;

   // copy over
   memset (pph, 0, sizeof(PATTERNHDR));
   pph->dwConvert = lConvert.Num();
   pph->dwNumNew = dwNum;
   pph->dwNumOrig = dwOrigNum;
   pph->dwRemapIndex = dwMax;
   memcpy (padwCompress, padwData, dwNum * sizeof(DWORD));
   //memcpy (padwConvert, lConvert.Get(0), lConvert.Num() * 2 * sizeof(DWORD));

   // to subtractive conversion - which will make huffman work better here
   // can subtract because know sorting is from highest to lowest

   // also, resort convert so all first entries, then all second entries so can RLE later
   DWORD *padwOC;
   padwOC = (DWORD*) lConvert.Get(0);
   dwNum = lConvert.Num();
   BOOL fShowOrig;
   for (i = 0; i < dwNum; i++) {
      if (i) {
         padwConvert[i] = padwOC[(i-1)*2] - padwOC[i*2];
         fShowOrig = (padwConvert[i] != 0);
            // if no change then also looping down on 2nd case, so show delta there
      }
      else {
         padwConvert[i] = padwOC[i*2];
         fShowOrig = TRUE;
      }

      if (fShowOrig)
         padwConvert[i + dwNum] = padwOC[i*2+1];
      else
         padwConvert[i + dwNum] = padwOC[(i-1)*2+1] - padwOC[i*2+1];
   }


   return 0;
}

/*******************************************************************************
PatternDecodeConvert - Given a compressed index, this decompresses it.

inputs
   DWORD          dwVal - Potentially compressed value
   DWORD          dwMax - If dwVal >= dwMax it's compressed
   DWORD          *padwConvert - Pointer to list of conversions. Each conversion is 2 DWORDs
   PBYTE          pabDecompTo - Pointer to the START of where decompression happend
   DWORD          dwElemSize - Element size to decompress to
   DWORD          *pdwCurElem - Filled with a pointer to the current element. This
                     is incremeented whena  new element is written
returns
   none
*/
void PatternDecodeConvert (DWORD dwVal, DWORD dwMax, DWORD *padwConvert, PBYTE pabDecompTo,
                           DWORD dwElemSize, DWORD *pdwCurElem)
{
   // if it is a conversion then recurse
   if (dwVal >= dwMax) {
      dwVal -= dwMax;
      PatternDecodeConvert (padwConvert[dwVal*2], dwMax, padwConvert, pabDecompTo, dwElemSize, pdwCurElem);
      PatternDecodeConvert (padwConvert[dwVal*2+1], dwMax, padwConvert, pabDecompTo, dwElemSize, pdwCurElem);
      return;
   }

   // else, it's converted to a single byte/word/etc
   if (dwElemSize == 4)
      ((DWORD*)pabDecompTo)[*pdwCurElem] = dwVal;
   else if (dwElemSize == 2)
      ((WORD*)pabDecompTo)[*pdwCurElem] = (WORD)dwVal;
   else
      ((BYTE*)pabDecompTo)[*pdwCurElem] = (BYTE)dwVal;
   (*pdwCurElem)++;
}

/*******************************************************************************
PatternDecode - Decodes the encoding from PatternEncode

inputs
   PBYTE          pEnc - Pattern encoded
   DWORD          dwSize - Number of bytes left in all the memory
   DWORD          dwElemSize - Element size. 1 = byte, 2 = word, or 4 = dword
   PCMem          pMem - Filled with the decompressed data. m_dwCurPosn = dwNumEelem * dwElemSize
   DWORD          *pdwUsed - Filled with number of bytes used
returns
   DWORD - 0 if OK, 1 if need new version of app, 2 if other error
*/
DWORD PatternDecode (PBYTE pEnc, DWORD dwSize, DWORD dwElemSize,
                     PCMem pMem, DWORD *pdwUsed)
{
   *pdwUsed = 0;
   DWORD dwOrigSize = dwSize;
   pMem->m_dwCurPosn = 0;

   // pull out the header
   PPATTERNHDR pph;
   if (dwSize < sizeof(PATTERNHDR))
      return 2;
   pph = (PPATTERNHDR) pEnc;
   pEnc += sizeof(PATTERNHDR);
   dwSize -= sizeof(PATTERNHDR);

   // other values
   DWORD dwNeed;
   dwNeed = pph->dwNumNew * sizeof(DWORD) + pph->dwConvert * sizeof(DWORD)*2;
   if (dwNeed > dwSize)
      return 2;   // error
   *pdwUsed = dwNeed + sizeof(PATTERNHDR);
   DWORD *padwCompress, *padwConvert;
   padwCompress = (DWORD*) (pph+1);
   padwConvert = padwCompress + pph->dwNumNew;

   // reorder
   CMem memOrder;
   if (!memOrder.Required (pph->dwConvert * sizeof(DWORD)*2))
      return 2;
   DWORD *padwReorder;
   padwReorder = (DWORD*) memOrder.p;
   DWORD i;
   for (i = 0; i < pph->dwConvert; i++) {
      padwReorder[i*2] = padwConvert[i];
      padwReorder[i*2+1] = padwConvert[i+pph->dwConvert];
   }
   padwConvert = padwReorder;

   // rebuild the conversion
   BOOL fShowOrig;
   for (i = 0; i < pph->dwConvert; i++) {
      if (i) {
         fShowOrig = (padwConvert[i*2] != 0);
            // if no change then also looping down on 2nd case, so show delta there
         padwConvert[i*2] = padwConvert[(i-1)*2] - padwConvert[i*2];
      }
      else {
         // no change to padwConvert
         fShowOrig = TRUE;
      }

      if (!fShowOrig)
         padwConvert[i*2+1] = padwConvert[(i-1)*2+1] - padwConvert[i*2+1];
   }

   // make sure enoiugh memory
   if (!pMem->Required (pMem->m_dwCurPosn + pph->dwNumOrig * dwElemSize))
      return 2;
   PBYTE pDecompTo;
   pDecompTo = (PBYTE) pMem->p + pMem->m_dwCurPosn;
   pMem->m_dwCurPosn += pph->dwNumOrig * dwElemSize;

   // loop
   DWORD dwCurElem;
   dwCurElem = 0;
   for (i = 0; i < pph->dwNumNew; i++) {
      PatternDecodeConvert (padwCompress[i], pph->dwRemapIndex, padwConvert,
         pDecompTo, dwElemSize, &dwCurElem);
   }

   return 0;
}




/****************************************************************************8
LZCompress - Compresses data.

inputs
   PVOID    pMem - to compress
   DWORD    dwSize - in bytes
   DWORD    *pdwCompressSize - Filled with the size (in bytes) of compressed data
returns
   PVOID - Memory that compressed to. Must be free()-ed.
*/
PVOID LZCompress (PVOID pMem, DWORD dwSize, DWORD *pdwCompressSize)
{
   CMem memPattern;
   DWORD dwRet;
   dwRet = PatternEncode ((PBYTE)pMem, dwSize, sizeof(BYTE), &memPattern);
   if (dwRet)
      return NULL;

   CMem memHuff;
   dwRet = HuffmanEncode ((PBYTE)memPattern.p, memPattern.m_dwCurPosn / sizeof(DWORD),
      sizeof(DWORD), &memHuff);
   if (dwRet)
      return NULL;

   PVOID pRet;
   pRet = malloc (memHuff.m_dwCurPosn);
   if (!pRet)
      return NULL;
   memcpy (pRet, memHuff.p, memHuff.m_dwCurPosn);

   *pdwCompressSize = memHuff.m_dwCurPosn;
   return pRet;
}


/****************************************************************************8
LZDecompress - Reads in compressed data and decompressed

inputs
   PVOID    pComp - compressed memory to decompress
   DWORD    dwCompSize - size in bytes of compressed memory
   DWORD    *pdwDecompSize - filled with the size of the decompressed memry
returns
   PVOID - filled with decompressed memory
*/
PVOID LZDecompress (PVOID pComp, DWORD dwCompSize, DWORD *pdwDecompSize)
{
   CMem memHuff;
   DWORD dwUsed, dwRet;
   dwRet = HuffmanDecode ((PBYTE) pComp, dwCompSize, sizeof(DWORD), &memHuff, &dwUsed);
   if (dwRet)
      return NULL;

   CMem memPattern;
   dwRet = PatternDecode ((PBYTE) memHuff.p, memHuff.m_dwCurPosn, sizeof(BYTE),
      &memPattern, &dwUsed);
   if (dwRet)
      return NULL;

   PVOID pRet;
   pRet = malloc (memPattern.m_dwCurPosn);
   if (!pRet)
      return NULL;
   memcpy (pRet, memPattern.p, memPattern.m_dwCurPosn);

   *pdwDecompSize = memPattern.m_dwCurPosn;
   return pRet;
}

#endif // 0