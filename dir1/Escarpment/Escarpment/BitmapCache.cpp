/*************************************************************
BitmapCache.cpp - Caches bitmaps.

begun 5/10/00 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include "mymalloc.h"
#include <windows.h>
#include "escarpment.h"
#include <stdio.h>
#include <stdlib.h>
#include "jpeg.h"
#include "resleak.h"

typedef struct {
   DWORD       dwID;       // ID for purposes of the cache
   WCHAR       szFile[256];   // file name. NULL-string if not file
   size_t       dwResource; // resource ID. 0 if used fil
   BOOL        fJPEG;      // if TRUE dwResource is JPEG, else it's a bitmap
   HINSTANCE   hInstance;  // where resource loaded from
   COLORREF    cMatch;     // color ref to match
   DWORD       dwColorDist;   // color distance to use
   HBITMAP     hBitmap;    // main bitmap
   HBITMAP     hMask;      // mask. NULL if no mask
   DWORD       dwCount;    // 0 => can free, 1+ => in use
   DWORD       dwLastFreed;   // tick count for last time freed and dwCount-- called
} BMCACHE, *PBMCACHE;


static DWORD   gdwCurID = 1;  // ID counter
static CBitmapCache gBitmapCache;   // global bitmap cache

/*******************************************************************************
constructor & destructor */
CBitmapCache::CBitmapCache (void)
{
   m_listBMCACHE.Init (sizeof(BMCACHE));
}

CBitmapCache::~CBitmapCache (void)
{
   ClearCompletely();

   // all freed
}

/********************************************************************************
CBitmapCache::ClearCompletely - Completely free
*/
void CBitmapCache::ClearCompletely (void)
{
   DWORD i;
   BMCACHE  *p;
   for (i = 0; i < m_listBMCACHE.Num(); i++) {
      p = (BMCACHE*) m_listBMCACHE.Get(i);
      if (p->hMask)
         DeleteObject (p->hMask);
      if (p->hBitmap)
         DeleteObject (p->hBitmap);
   }
   m_listBMCACHE.ClearCompletely();
}

/*******************************************************************************
CacheCleanUp - If a bitmap hasn't been used for 10 minutes then get rid of it.

inputs
   BOOL     fForce - If TRUE force a cleanup no matter how long it's been around
*/
void CBitmapCache::CacheCleanUp (BOOL fForce)
{
   DWORD i, dwNum;
   BMCACHE  *p;
   dwNum = m_listBMCACHE.Num();

   for (i = dwNum-1; i < dwNum; i--) {
      p = (BMCACHE*) m_listBMCACHE.Get(i);
      if (!p) continue;

      // if it still has a count then ignore
      if (p->dwCount)
         continue;

      // if it's not old then ignore
      if (!fForce && (p->dwLastFreed + 10 * 60 * 1000 > GetTickCount()))
         continue;

      // else free
      if (p->hMask)
         DeleteObject (p->hMask);
      if (p->hBitmap)
         DeleteObject (p->hBitmap);

      m_listBMCACHE.Remove(i);
   }
}


/*******************************************************************************
Cache - Caches a file into the bitmap cache.

inputs
   PWSTR       psz - File string. NULL if not file
   DWORD       dwResource - resource iD. 0 if no resource
   BOOL        fJPEG - TRUE if JPEG resource.
   HINSTANCE   hInstance - instance to load resource from
   HBITMAP     *phbmpBitmap - Filled with the bitmap
   HBITMAP     *phbmpMask - Filled with the mask bitmap. Leave NULL if you dont want mask
   COLORREF    cMatch - Color to match
   DWORD       dwColorDist - color distance
returns
   DWORD - Cache ID. Use this to release
*/
DWORD CBitmapCache::Cache (PWSTR psz, size_t dwResource, BOOL fJPEG, HINSTANCE hInstance,
                               HBITMAP *phbmpBitmap, HBITMAP *phbmpMask,
                               COLORREF cMatch, DWORD dwColorDist)
{
   // first, see if there's a match
   DWORD i;
   BMCACHE *p;
   for (i = 0; i < m_listBMCACHE.Num(); i++) {
      p = (BMCACHE*) m_listBMCACHE.Get(i);
      if (!p) continue;

      // see if matches
      if (psz) {
         if (_wcsicmp(p->szFile, psz))
            continue;
      }
      if (dwResource) {
         if ((p->dwResource != dwResource) || (p->fJPEG != fJPEG))
            continue;
      }

      if (phbmpMask) {
         if (!p->hMask)
            continue;
         if (p->cMatch != cMatch)
            continue;
         if (p->dwColorDist != dwColorDist)
            continue;
      }
      else {   // no mask
         if (p->hMask)
            continue;   // this has a mask and dont want
      }

      // if get to here found a match
      p->dwCount++;
      *phbmpBitmap = p->hBitmap;
      if (phbmpMask)
         *phbmpMask = p->hMask;
      return p->dwID;
   }

   // get rid of old bitmaps
   CacheCleanUp();

   // else, create new
   BMCACHE  b;
   memset (&b, 0, sizeof(b));

   if (psz)
      wcscpy (b.szFile, psz);
   b.cMatch = cMatch;
   b.dwColorDist = dwColorDist;
   b.dwCount = 1;
   b.dwID = gdwCurID++;
   b.fJPEG = fJPEG;
   b.dwResource = dwResource;
   b.hInstance = hInstance;

   if (psz) {
      // load in bitmap
      char  szTemp[256];
      WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
      b.hBitmap = JPegOrBitmapLoad (szTemp, FALSE);
   }
   else {
      if (fJPEG)
         b.hBitmap = JPegToBitmap (dwResource, hInstance);
      else {
         b.hBitmap = (HBITMAP) LoadImage (hInstance, MAKEINTRESOURCE(dwResource),
            IMAGE_BITMAP, 0, 0, 0);
      }
   }
   if (!b.hBitmap) {
      *phbmpBitmap = 0;
      if (phbmpMask)
         *phbmpMask = 0;
      return 0;
   }

   // mask?
   if (phbmpMask) {
      b.hMask = TransparentBitmap (b.hBitmap, cMatch, dwColorDist);
   }

   // write globals
   *phbmpBitmap = b.hBitmap;
   if (phbmpMask)
      *phbmpMask = b.hMask;

   m_listBMCACHE.Add(&b);

   // done
   return b.dwID;
}


/*******************************************************************************
CBitmapCache::Invalidate - If a file is cached for a given file name, this
invalidates the cache so it won't be used again if the filename is reloaded.

inputs
   PWSTR       psz - File string.
returns
   none
*/
void CBitmapCache::Invalidate (PWSTR psz)
{
   // first, see if there's a match
   DWORD i;
   BMCACHE *p;
   for (i = 0; i < m_listBMCACHE.Num(); i++) {
      p = (BMCACHE*) m_listBMCACHE.Get(i);
      if (!p) continue;

      // see if matches
      if (!_wcsicmp(p->szFile, psz))
         p->szFile[0] = 0; // to invalidate it
   }
}


/*****************************************************************************
CacheRelease - Releases one of the bitmaps

inputs
   DWORD    dwID - cache ID to look for
returns
   BOOL - TRUE if success. FALSE if error
*/

BOOL CBitmapCache::CacheRelease (DWORD dwID)
{
   DWORD i;
   BMCACHE *p;
   for (i = 0; i < m_listBMCACHE.Num(); i++) {
      p = (BMCACHE*) m_listBMCACHE.Get(i);
      if (!p) continue;

      if (p->dwID != dwID)
         continue;

      // else, make sure there's data there
      if (!p->dwCount)
         return FALSE;

      // decrement
      p->dwCount--;
      p->dwLastFreed = GetTickCount();
      return TRUE;
   }

   // didn't find
   return FALSE;
}


/*******************************************************************************
CacheFile, CacheResourceBMP, CacheResourceJPG - All call cache
*/
DWORD CBitmapCache::CacheFile (PWSTR psz, HBITMAP *phbmpBitmap, HBITMAP *phbmpMask,
   COLORREF cMatch, DWORD dwColorDist)
{
   return Cache (psz, 0, 0, 0, phbmpBitmap, phbmpMask, cMatch, dwColorDist);
}

DWORD CBitmapCache::CacheResourceBMP (size_t dwID, HINSTANCE hInstance, HBITMAP *phbmpBitmap, HBITMAP *phbmpMask,
   COLORREF cMatch, DWORD dwColorDist)
{
   return Cache (NULL, dwID, FALSE, hInstance, phbmpBitmap, phbmpMask, cMatch, dwColorDist);
}

DWORD CBitmapCache::CacheResourceJPG (size_t dwID, HINSTANCE hInstance, HBITMAP *phbmpBitmap, HBITMAP *phbmpMask,
   COLORREF cMatch, DWORD dwColorDist)
{
   return Cache (NULL, dwID, TRUE, hInstance, phbmpBitmap, phbmpMask, cMatch, dwColorDist);
}


/*******************************************************************************
EscBitmapCache - Returns the bitmap cache.

inputs
   none
returns
   PCBitmapCache - cache
*/
PCBitmapCache EscBitmapCache (void)
{
   return &gBitmapCache;
}
