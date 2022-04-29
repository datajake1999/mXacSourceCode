/**********************************************************************************
CThumbnail.cpp - Cache and store thumbnails.

begun 4/7/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


/**********************************************************************************
CThumbnail */

class CThumbnail {
public:
   ESCNEWDELETE;

   CThumbnail (void);
   ~CThumbnail (void);

   BOOL Init (PCMegaFile pmf, GUID *pgMajor, GUID *pgMinor);
   BOOL Init (GUID *pgMajor, GUID *pgMinor, PCImage pImage, COLORREF cTransparent = (DWORD)-1);
   BOOL Write (PCMegaFile pmf);
   HBITMAP ToBitmap (HWND hWnd, COLORREF *pcTransparent);
   void Size (DWORD *pdwWidth, DWORD *pdwHeight);

   // filled in when call Init
   GUID     m_gMajor;      // major iD
   GUID     m_gMinor;      // minor ID
   DFDATE   m_dwLastViewed;   // last time this was viewed
   BOOL     m_fDirty;      // set to TRUE if dirty

private:
   WORD     m_wWidth;      // width, in pixels
   WORD     m_wHeight;     // height, in pixels
   COLORREF m_cTransparent;   // transparent color
   CMem     m_memBits;     // RLE bits
};

/* globlas */
PCThumbnailCache      pgThumbnail = NULL;    // main thumbnail cache

/**********************************************************************************
ThumbnailGet - Returns a pointer go gThumbnail
*/
PCThumbnailCache ThumbnailGet (void)
{
   return pgThumbnail;
}

/**********************************************************************************
CThumbnail::Constructor and destructor
*/
CThumbnail::CThumbnail (void)
{
   memset (&m_gMajor, 0 ,sizeof(m_gMajor));
   memset (&m_gMinor, 0, sizeof(m_gMinor));
   m_wWidth = m_wHeight = 0;
   m_cTransparent = -1;
   m_dwLastViewed = 0;
   m_fDirty = FALSE;
}

CThumbnail::~CThumbnail (void)
{
   // do nothing
}

/**********************************************************************************
CThumbnail::Init - Reading from a file written out by write. Since the first DWORD
is the size of the thumbnail, this only reads in the data for this thumbnail.

inputs
   PCMegaFile     pmf - mega file to read from
   GUID           gMajor - Major guid
   GUID           gMinor - Minor guid
returns
   BOOL - TRUE for success
*/
typedef struct {
   GUID     gMajor;
   GUID     gMinor;
   WORD     wWidth;
   WORD     wHeight;
   COLORREF cTransparent;
   DFDATE   dwLastViewed;
} THUMBFILE, *PTHUMBFILE;
BOOL CThumbnail::Init (PCMegaFile pmf, GUID *pgMajor, GUID *pgMinor)
{
   // make a string
   WCHAR szTemp[96];
   GUID ag[2];
   ag[0] = *pgMinor;
   ag[1] = *pgMajor;
   MMLBinaryToString ((PBYTE) &ag[0], sizeof(ag), szTemp);

   // BUGFIX - Once in awhile set the "dont update access time" to FALSE
   // so that keep track of which thumbnails are most recently used
   BOOL fOldInfo = pmf->m_fDontUpdateLastAccess;
   if ((GetTickCount()%0xf00) == 0)
      pmf->m_fDontUpdateLastAccess = FALSE;

   __int64 iSize;
   PBYTE pData, pDataOrig;
   pData = pDataOrig = (PBYTE) pmf->Load (szTemp, &iSize, FALSE);
   pmf->m_fDontUpdateLastAccess = fOldInfo;
   if (!pData)
      return FALSE;
   DWORD dwSize = (DWORD)iSize;


   DWORD    dwSizeRead;
   if (dwSize < sizeof(DWORD)) {
      MegaFileFree (pDataOrig);
      return FALSE;
   }
   dwSizeRead = *((DWORD*)pData);
   pData += sizeof(DWORD);
   dwSize -= sizeof(DWORD);

   // make sure enough for width, height, guids and transparent
   if (dwSizeRead <= sizeof(THUMBFILE)) {
      MegaFileFree (pDataOrig);
      return FALSE;
   }
   dwSizeRead -= sizeof(THUMBFILE);

   // read in
   THUMBFILE tf;
   if (dwSize < sizeof(tf)) {
      MegaFileFree (pDataOrig);
      return FALSE;
   }
   memcpy (&tf, pData, sizeof(tf));
   pData += sizeof(tf);
   dwSize -= sizeof(tf);

   // read in rest of data
   if (!m_memBits.Required(dwSizeRead)) {
      MegaFileFree (pDataOrig);
      return FALSE;
   }
   if (dwSize < dwSizeRead) {
      MegaFileFree (pDataOrig);
      return FALSE;
   }
   memcpy (m_memBits.p, pData, dwSizeRead);
   pData += dwSizeRead;
   dwSize -= dwSizeRead;
   m_memBits.m_dwCurPosn = dwSizeRead;

   // fill in params
   m_gMajor = tf.gMajor;
   m_gMinor = tf.gMinor;
   m_wWidth = tf.wWidth;
   m_wHeight = tf.wHeight;
   m_cTransparent = tf.cTransparent;
   m_dwLastViewed = tf.dwLastViewed;

   MegaFileFree (pDataOrig);
   m_fDirty = FALSE;
   return TRUE;
}

/**********************************************************************************
CThumbnail::Write - Writes the thumbnail out to disk, writing the size of the data
first, so when read is called it knows how large it is.

inputs
   PCMegaFile     pmf - mega file to write to
returns
   BOOL - TRUE if success
*/
BOOL CThumbnail::Write (PCMegaFile pmf)
{
   THUMBFILE tf;
   CMem mem;
   DWORD dwNeed = sizeof(DWORD) + sizeof(tf) + (DWORD) m_memBits.m_dwCurPosn;
   if (!mem.Required (dwNeed))
      return FALSE;
   PBYTE pData = (PBYTE) mem.p;
   *((DWORD*)pData) = dwNeed - sizeof(DWORD);
   pData += sizeof(DWORD);

   memset (&tf, 0, sizeof(tf));
   tf.cTransparent = m_cTransparent;
   tf.gMajor = m_gMajor;
   tf.gMinor = m_gMinor;
   tf.wHeight = m_wHeight;
   tf.wWidth = m_wWidth;
   tf.dwLastViewed = m_dwLastViewed;

   memcpy (pData, &tf, sizeof(tf));
   pData += sizeof(tf);

   memcpy (pData, m_memBits.p, m_memBits.m_dwCurPosn);

   // make a string
   WCHAR szTemp[96];
   GUID ag[2];
   ag[0] = m_gMinor;
   ag[1] = m_gMajor;
   MMLBinaryToString ((PBYTE) &ag[0], sizeof(ag), szTemp);

   // write to the megafile
   if (!pmf->Save (szTemp, mem.p, dwNeed))
      return FALSE;

   m_fDirty = FALSE;
   return TRUE;
}


/**********************************************************************************
CThumbnail::Init - intializes a thumnail from a PCImage, which is used to get the size
information.

inputs
   GUID     *pgMajor, *pgMinor - ID Guids
   PCImage  pImage - Image where the info is gotten from
   COLORREF cTransparent - What the transparent color is, or -1 if no transparent
returns
   BOOL - TRUE if success
*/
BOOL CThumbnail::Init (GUID *pgMajor, GUID *pgMinor, PCImage pImage, COLORREF cTransparent)
{
   m_gMajor = *pgMajor;
   m_gMinor = *pgMinor;
   m_cTransparent = cTransparent;
   m_wWidth = (WORD)pImage->Width();
   m_wHeight = (WORD) pImage->Height();
   m_dwLastViewed = gdwToday;

   // loop over the image
   DWORD dwNum, i, j, dwNeed, k;
   PIMAGEPIXEL pip;
   BYTE *pb;
   pip = pImage->Pixel(0,0);
   m_memBits.m_dwCurPosn = 0;
   dwNum = pImage->Width() * pImage->Height();
   for (i = 0; i < dwNum; ) {
      // get this color
      COLORREF cStart, cPrev;
      cStart = pImage->UnGamma (&pip->wRed);

      // loop until no longer the same
      COLORREF cTest;
      for (j = 1; (i+j < dwNum) && (j < 128); j++) {
         cTest = pImage->UnGamma (&pip[j].wRed);
         if (cTest != cStart)
            break;
      }

      // wherever got to was a change in data...
      // if j > 1 then have that many in a row
      if (j > 1) {
         // allocate enough space
         dwNeed = (DWORD) m_memBits.m_dwCurPosn + 1 + 3;
         if (dwNeed >= m_memBits.m_dwAllocated) {
            if (!m_memBits.Required (dwNeed + 1024))
               return FALSE;
         }

         pb = (PBYTE) m_memBits.p;
         pb[m_memBits.m_dwCurPosn++] = (BYTE)(j-1);
         pb[m_memBits.m_dwCurPosn++] = (BYTE)GetRValue (cStart);
         pb[m_memBits.m_dwCurPosn++] = (BYTE)GetGValue (cStart);
         pb[m_memBits.m_dwCurPosn++] = (BYTE)GetBValue (cStart);
         
         i += j;
         pip += j;
         continue;
      }  // found several ina row that match

      // else, see how far can go before a match in colors
      cPrev = cStart;
      for (j = 1; (i+j < dwNum) && (j < 128); j++) {
         cTest = pImage->UnGamma (&pip[j].wRed);
         if (cTest == cPrev) {
            // NOTE: Being a bit stupid because 2 in a row the same will cause a
            // break, when probably should be 3 or 4.
            j--;
            break;
         }
         cPrev = cTest;
      }

      // write out this many colors
      dwNeed = (DWORD) m_memBits.m_dwCurPosn + 1 + j * 3;
      if (dwNeed >= m_memBits.m_dwAllocated) {
         if (!m_memBits.Required (dwNeed + 1024))
            return FALSE;
      }
      pb = (PBYTE) m_memBits.p;
      pb[m_memBits.m_dwCurPosn++] = (BYTE)(j-1) + 128;

      for (k = 0; k < j; k++) {
         cStart = pImage->UnGamma (&pip[k].wRed);
         pb = (PBYTE) m_memBits.p;
         pb[m_memBits.m_dwCurPosn++] = (BYTE)GetRValue (cStart);
         pb[m_memBits.m_dwCurPosn++] = (BYTE)GetGValue (cStart);
         pb[m_memBits.m_dwCurPosn++] = (BYTE)GetBValue (cStart);
      }

      // continue
      i += j;
      pip += j;
      continue;
   }  // over all pixels

   // done
   m_fDirty = TRUE;
   return TRUE;
}

/**********************************************************************************
CThumbnail::ToBitmap - Convers the thumbnail to a bitmap.

inputs
   HWND     hWnd - Used for the device context
   COLORREF *pcTransparent - Filled with the color that's transparent, or -1 if no transparent
returns
   HBITMAP - Bitmap that must be freed by caller, or NULL if error
*/
HBITMAP CThumbnail::ToBitmap (HWND hWnd, COLORREF *pcTransparent)
{
   CImage Image;

   if (!Image.Init (m_wWidth, m_wHeight))
      return FALSE;
   m_dwLastViewed = gdwToday;

   // fill it in
   DWORD i, j, dwCur, dwNum, dwCount;
   PIMAGEPIXEL pip = Image.Pixel(0,0);
   COLORREF cColor;
   BYTE bVal, bRed, bGreen, bBlue;
   dwNum = (DWORD)m_wWidth * (DWORD)m_wHeight;
   dwCur = 0;
   PBYTE pb;
   pb = (PBYTE) m_memBits.p;
   for (i = 0; i < dwNum; ) {
      // get a byte
      if (dwCur >= m_memBits.m_dwCurPosn)
         return FALSE;
      bVal = pb[dwCur++];
      dwCount = ((DWORD)bVal & 0x7f) + 1;

      if (bVal & 0x80) {  // non-repeating
         if ((dwCur + 3 * dwCount > m_memBits.m_dwCurPosn) || (i + dwCount > dwNum))
            return FALSE;
         for (j = 0; j < dwCount; j++) {
            bRed = pb[dwCur++];
            bGreen = pb[dwCur++];
            bBlue = pb[dwCur++];
            cColor = RGB(bRed, bGreen, bBlue);
            Image.Gamma (cColor, &pip->wRed);
            pip++;
            i++;
         }
      }
      else {   // repeating
         if ((dwCur + 3 > m_memBits.m_dwCurPosn) || (i + dwCount > dwNum))
            return FALSE;
         bRed = pb[dwCur++];
         bGreen = pb[dwCur++];
         bBlue = pb[dwCur++];
         cColor = RGB(bRed, bGreen, bBlue);
         for (j = 0; j < dwCount; j++) {
            Image.Gamma (cColor, &pip->wRed);
            pip++;
            i++;
         }
      }
   }  // over all num

   // fill in transparent color
   *pcTransparent = m_cTransparent;

   // write to bitmap
   HDC   hDC;
   HBITMAP hBmp;
   hDC = GetDC (hWnd);
   hBmp = Image.ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);
   return hBmp;
}

/**********************************************************************************
CThumbnail::Size - Fills in the size of the bitmap

inputs
   DWORD    *pdwWidth, *pdwHeight - Filled in with width and height
*/
void CThumbnail::Size (DWORD *pdwWidth, DWORD *pdwHeight)
{
   *pdwWidth = m_wWidth;
   *pdwHeight = m_wHeight;
}



/**********************************************************************************
CThumbnailCache::Constructor and destructor
*/
CThumbnailCache::CThumbnailCache (void)
{
   memset (m_szFile, 0, sizeof(m_szFile));
   m_lPCThumbnail.Init (sizeof(PCThumbnail));
}

CThumbnailCache::~CThumbnailCache (void)
{
   ThumbnailClearAll (TRUE);
}

/**********************************************************************************
CThumbnailCache::Init - Reads the thumbnail cache from disk.

inputs
   WCHAR     *pszFile - file
returns
   BOOL - TRUE if success, FALSE if failure
*/
BOOL CThumbnailCache::Init (WCHAR *pszFile, PCProgressSocket pProgress)
{
   ThumbnailClearAll(TRUE);
   wcscpy (m_szFile, pszFile);

   if (!m_mfFile.Init (m_szFile, &GUID_ThumbnailHeader))
      return FALSE;
   m_mfFile.LimitSize (100 * 1000000, 0); // don't allow this to get too large

   return TRUE;
}

/**********************************************************************************
CThumbnailCache::Commit - Writes out the thumbnail cache IF it is dirty

returns
   BOOL - TRUE if success
*/
BOOL CThumbnailCache::Commit (PCProgressSocket pProgress)
{
   // write any dirty entries out
   DWORD i;
   for (i = 0; i < m_lPCThumbnail.Num(); i++) {
      PCThumbnail pt = *((PCThumbnail*) m_lPCThumbnail.Get(i));
      if (!pt->m_fDirty)
         continue;   // not dirty

      if (pProgress && ((i%50) == 0)) {
         // show progress
         pProgress->Update ((fp) i / (fp) m_lPCThumbnail.Num());
      }

      if (!pt->Write (&m_mfFile))
         return FALSE;
   }

   return TRUE;
}


/**********************************************************************************
CThumbnailCache::ThumbnailToBitmap - Converts a thumbail to a bitmap.

inputs
   GUID     *pgMajor, *pgMinor - Identify the thumbail
   HWND     hWnd - Used for HDC
   COLORREF *pfTransparent - Filled with transparent color, or -1 if no transparent
returns
   HBITMAP - Bitmap that must be freed by caller, or NULL if no cache
*/
HBITMAP CThumbnailCache::ThumbnailToBitmap (GUID *pgMajor, GUID *pgMinor, HWND hWnd, COLORREF *pcTransparent)
{
   DWORD i;
   PCThumbnail pt;
   for (i = 0; i < m_lPCThumbnail.Num(); i++) {
      pt = *((PCThumbnail*) m_lPCThumbnail.Get(i));
      if (IsEqualGUID(pt->m_gMinor, *pgMinor) && IsEqualGUID(pt->m_gMajor, *pgMajor))
         break;
   }
   if (i >= m_lPCThumbnail.Num()) {
      // else, try to load from disk
      pt = new CThumbnail;
      if (!pt)
         return NULL;
      if (!pt->Init (&m_mfFile, pgMajor, pgMinor)) {
         // not found in the cache
         delete pt;
         return NULL;
      }

      HouseCleaning();
      
      // else, found in file, so add to list and continue
      m_lPCThumbnail.Add (&pt);
   }

   // else found
   return pt->ToBitmap (hWnd, pcTransparent);
}

/**********************************************************************************
CThumbnailCache::ThumbnailSize - returns the size of the thumbal

inputs
   GUID     *pgMajor, *pgMinor - Identify the thumbail
return
   BOOL - TRUE if found, FALSE if cant find
*/
BOOL CThumbnailCache::ThumbnailSize (GUID *pgMajor, GUID *pgMinor, DWORD *pdwWidth, DWORD *pdwHeight)
{
   DWORD i;
   PCThumbnail pt;
   for (i = 0; i < m_lPCThumbnail.Num(); i++) {
      pt = *((PCThumbnail*) m_lPCThumbnail.Get(i));
      if (IsEqualGUID(pt->m_gMinor, *pgMinor) && IsEqualGUID(pt->m_gMajor, *pgMajor))
         break;
   }
   if (i >= m_lPCThumbnail.Num()) {
      // NOTE likely to get called
      // else, try to load from disk
      pt = new CThumbnail;
      if (!pt)
         return NULL;
      if (!pt->Init (&m_mfFile, pgMajor, pgMinor)) {
         // not found in the cache
         delete pt;
         return NULL;
      }

      HouseCleaning();
      
      // else, found in file, so add to list and continue
      m_lPCThumbnail.Add (&pt);
   }

   pt->Size (pdwWidth, pdwHeight);
   return TRUE;
}

/**********************************************************************************
CThumbnailCache::ThumbnailRemove - Deletes an exsiting thumbnail

inputs
   GUID     *pgMajor, *pgMinor - Identify the thumbail
return
   BOOL - TRUE if found, FALSE if cant find
*/
BOOL CThumbnailCache::ThumbnailRemove (GUID *pgMajor, GUID *pgMinor)
{
   // delete it from the mefagile
   // make a string
   WCHAR szTemp[96];
   GUID ag[2];
   ag[0] = *pgMinor;
   ag[1] = *pgMajor;
   MMLBinaryToString ((PBYTE) &ag[0], sizeof(ag), szTemp);
   BOOL fRet = m_mfFile.Delete (szTemp);

   DWORD i;
   PCThumbnail pt;
   for (i = 0; i < m_lPCThumbnail.Num(); i++) {
      pt = *((PCThumbnail*) m_lPCThumbnail.Get(i));
      if (IsEqualGUID(pt->m_gMinor, *pgMinor) && IsEqualGUID(pt->m_gMajor, *pgMajor))
         break;
   }
   if (i >= m_lPCThumbnail.Num())
      return fRet;

   // else, remove
   delete pt;
   m_lPCThumbnail.Remove(i);
   return TRUE;
}

/**********************************************************************************
CThumbnailCache::ThumbnailAdd - Add a thumbnail from an image.

inputs
   GUID     *pgMajor, *pgMinor - GUIDs for the thumbail.
   PCImage  pImage - Not modified, but get the image from this
   COLORREF cTransparent - What the transparent color is, or -1 if all opaque
*/
BOOL CThumbnailCache::ThumbnailAdd (GUID *pgMajor, GUID *pgMinor, PCImage pImage, COLORREF cTransparent)
{
   // delete existing one
   ThumbnailRemove (pgMajor, pgMinor);

   // if too many then clear out
   HouseCleaning();

   // create new one
   PCThumbnail pt;
   pt = new CThumbnail;
   if (!pt)
      return FALSE;

   if (!pt->Init (pgMajor, pgMinor, pImage, cTransparent)) {
      delete pt;
      return FALSE;
   }

   // add it
   m_lPCThumbnail.Add (&pt);
   pt->m_fDirty = TRUE;
   return TRUE;
}

/**********************************************************************************
CThumbnailCache::ThumbnailClearAll - Clear all the thumbnails in the list in MEMORY.

inputs
   BOOL     fMemoryOnly - If TRUE, only the contents of memory area cleared. If FALSE,
               the disk contents are cleared too.
*/
BOOL CThumbnailCache::ThumbnailClearAll (BOOL fMemoryOnly)
{
   DWORD i;
   for (i = 0; i < m_lPCThumbnail.Num(); i++) {
      PCThumbnail pt = *((PCThumbnail*) m_lPCThumbnail.Get(i));
      delete pt;
   }
   m_lPCThumbnail.Clear();

   if (!fMemoryOnly)
      m_mfFile.Clear();

   return TRUE;
}


/**********************************************************************************
CThumbnailCache::HouseCleaning - If there are too many thumnails then this
eliminates the oldest ones.
*/

#define MAXTNCACHE      500  // BUGFIX - Now used as max cache within memory
BOOL CThumbnailCache::HouseCleaning (void)
{
   // if not reached max then dont care
   if (m_lPCThumbnail.Num() < MAXTNCACHE)
      return TRUE;

   // because not really deleting files, just toss out at random
   while (m_lPCThumbnail.Num() >= MAXTNCACHE) {
      DWORD dwNum = (DWORD) rand() % m_lPCThumbnail.Num();
      PCThumbnail pt = *((PCThumbnail*) m_lPCThumbnail.Get(dwNum));
      
      // save
      if (pt->m_fDirty)
         pt->Write (&m_mfFile);
      delete pt;
      m_lPCThumbnail.Remove (dwNum);
   }
   return TRUE;
}


/************************************************************************************
ThumbnailOpenFile - Opens thumbnail file
The globals gThumbnails is filled in
*/
void ThumbnailOpenFile (PCProgressSocket pProgress)
{
   if (pgThumbnail)
      return;
   pgThumbnail = new CThumbnailCache;
   if (!pgThumbnail)
      return;

   WCHAR szTemp[512];

   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));
   wcscat (szTemp, L"ThumbnailCache." CACHEFILEEXT);
   pgThumbnail->Init (szTemp, pProgress);
}

/************************************************************************************
ThumbnailSaveFile - If the files are dirty, the library files are saved.
*/
void ThumbnailSaveFile (PCProgressSocket pProgress)
{
   pgThumbnail->Commit(pProgress);

   delete pgThumbnail;
   pgThumbnail = NULL;
}

