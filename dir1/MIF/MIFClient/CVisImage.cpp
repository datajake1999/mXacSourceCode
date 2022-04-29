/*************************************************************************************
CVisImage.cpp - Code for managing the visible image.

begun 19/5/04 by Mike Rozak.
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


#define BLINKMIN           5     // minmum number of seconds between blinks
#define BLINKMAX           10    // maximum number of seconds between blinks
#define BLINKDURATION      0.25   // duration of a blink


#define SHOWLIKEDELTATIME  3000  // show delta likes for 3 seconds

#define IMAGECACHETIMER_MAX32    (10 * 8)    // maximum number of images that can cache in 32-bit
   // BUGFIX - Was 20 * 8, reduced to 8 * 8
   // BUGFIX - Increased to 10 to make sure can cover 8 directions around room
#define IMAGECACHETIMER_MAX64    (IMAGECACHETIMER_MAX32*2)     // maximum number of images that can cache in 64-bit
   // BUGFIX - Was 4x, but lowered to 2x because too much memory

// IMAGECACHE
typedef struct {
   PCImageStore      pImage;     // image that's cached
   DWORD             dwRefCount; // ref count, when hits 0 can delete
   DWORD             dwLastUsed; // GetTickCount() for the time when the refcount went to 0
   PCMem             pMemFile;   // memory containing the file-name string
   BOOL              fDead;      // set to TRUE if this is a dead image and has been
                                 // replaced by a newer one
   DWORD             dwQuality;  // 0 for low-quality quick render, 1 for high quality
} IMAGECACHE, *PIMAGECACHE;

static CListFixed          glIMAGECACHE;     // image cache, so don't waste memory loading multiple copies



/*************************************************************************************
Is360 - Returns TRUE if this is a 360 degree image

inputs
   PVILAYER       pvi - Layer
*/
BOOL Is360 (PVILAYER pvi)
{
   return pvi && pvi->pImage && (pvi->pImage->m_dwStretch == 4);
}


/*************************************************************************************
ImageCacheIsDead - Looks through the image cache and sees if the image this one
refers to is marked as dead.

inputs
   PCImageStore      pStore - Image referred to
returns
   BOOL - TRUE if marked as dead, FALSE if alive
*/
BOOL ImageCacheIsDead (PCImageStore pStore)
{
   DWORD i;
   PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   for (i = 0; i < glIMAGECACHE.Num(); i++, pic++)
      if (pic->pImage == pStore)
         return pic->fDead;

   return FALSE;
}


/*************************************************************************************
ImageCacheFindLowQuality - Finds the low-quality version of a filename.

inputs
   PWSTR             pszFile - file name
returns
   PCImageStore - Image store (NOT addreffed), or NULL if can't find
*/
PCImageStore ImageCacheFindLowQuality (PWSTR pszFile)
{
   // see if it's cached already, in which case mark it as dead
   DWORD i;
   PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   for (i = 0; i < glIMAGECACHE.Num(); i++, pic++)
         // BUGFIX - If dead, then don't add
      if (!pic->dwQuality && !pic->fDead && !_wcsicmp((PWSTR)pic->pMemFile->p, pszFile))
         return pic->pImage;

   // else, cant find
   return NULL;
}

/*************************************************************************************
ImageCacheAdd - Adds an existing PCImageStore to the cache. If a store
with the same name already exists that one is replaced.

NOTE: If this returns TRUE then the image store is ref-counted by one, and should
be released.

inputs
   PWSTR             pszFile - file name
   PCImageStore      pStore - Store. This is taken over by the function, even if it fails to add
   DWORD             dwQuality - 0 for low-quality quick render, 1 for high quality
returns
   BOOL - TRUE if success (and ref-counted), FALSE if failed to add
*/
BOOL ImageCacheAdd (PWSTR pszFile, PCImageStore pStore, DWORD dwQuality)
{
   // see if it's cached already, in which case mark it as dead
   DWORD i;
   PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   for (i = 0; i < glIMAGECACHE.Num(); i++, pic++)
      if ((pic->dwQuality == dwQuality) && !_wcsicmp((PWSTR)pic->pMemFile->p, pszFile))
         pic->fDead = TRUE;

   // add to cache
   IMAGECACHE ic;
   memset (&ic, 0, sizeof(ic));
   ic.dwRefCount = 1;
   ic.pImage = pStore;
   ic.pMemFile = new CMem;
   ic.dwQuality = dwQuality;
   MemZero (ic.pMemFile);
   MemCat (ic.pMemFile, pszFile);
   if (!glIMAGECACHE.Num())
      glIMAGECACHE.Init (sizeof(IMAGECACHE));
   glIMAGECACHE.Add (&ic);

   return TRUE;
}



/*************************************************************************************
ImageCacheExists - Returns TRUE if the image cache exists.

inputs
   PWSTR             pszFile - File name
   DWORD             dwQuality - 0 for low-quality quick render, 1 for high quality
   BOOL              fTouch - If the image is found AND this is true, then the last-used
                     time-stamp will automatically be updated
returns
   BOOL - TRUE if exsits
*/
BOOL ImageCacheExists (PWSTR pszFile, DWORD dwQuality, BOOL fTouch)
{
   // see if it's cached already
   DWORD i;
   PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   for (i = 0; i < glIMAGECACHE.Num(); i++, pic++)
      if (!pic->fDead && (pic->dwQuality == dwQuality) && !_wcsicmp((PWSTR)pic->pMemFile->p, pszFile)) {
         if (fTouch)
            pic->dwLastUsed = GetTickCount();

         return TRUE;
      }

   // else,doesn't exist
   return FALSE;
}

/*************************************************************************************
ImageCacheOpen - Opens an image from the cache.

inputs
   PWSTR             pszFile - File name
   DWORD             dwQuality - 0 for low-quality quick render, 1 for high quality
   PCMainWindow      pMain - Used to update the main window if load image from disk
returns
   PCImageStore - Image found, or NULL if cant find any. This must be
      freed by calling ImageCacheRelease()
*/
PCImageStore ImageCacheOpen (PWSTR pszFile, DWORD dwQuality, PCMainWindow pMain)
{
   // see if it's cached already
   DWORD i;
   PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   for (i = 0; i < glIMAGECACHE.Num(); i++, pic++)
      if (!pic->fDead && (pic->dwQuality == dwQuality) && !_wcsicmp((PWSTR)pic->pMemFile->p, pszFile)) {
         pic->dwRefCount++;
         return pic->pImage;
      }

   // BUGFIX - If get here, not cached, so load it in
   pMain->ImageLoadThreadAdd (pszFile, dwQuality, 0);

   return NULL;   // so will show loading
   
#if 0
   // if it doesn't exist then just exit here
   if (!pMF->Exists (pszFile))
      return NULL;

   // once in awhile set the flag so will update last access time
   BOOL fOldInfo = pMF->m_fDontUpdateLastAccess;
   if ((GetTickCount() & 0xf00) == 0)
      pMF->m_fDontUpdateLastAccess = FALSE;

   // if get here not in cache, so try to open
   PCImageStore pStore = new CImageStore;
   if (!pStore)
      return NULL;
   if (!pStore->Init (pMF, pszFile)) {
      pMF->m_fDontUpdateLastAccess = fOldInfo;
      delete pStore;
      return NULL;   // not yet done
   }
   pMF->m_fDontUpdateLastAccess = fOldInfo;


   // add to cache
   IMAGECACHE ic;
   memset (&ic, 0, sizeof(ic));
   ic.dwRefCount = 1;
   ic.pImage = pStore;
   ic.pMemFile = new CMem;
   ic.dwQuality = dwQuality;
   MemZero (ic.pMemFile);
   MemCat (ic.pMemFile, pszFile);
   if (!glIMAGECACHE.Num())
      glIMAGECACHE.Init (sizeof(IMAGECACHE));
   glIMAGECACHE.Add (&ic);

   if (pMain)
      pMain->BackgroundUpdate (pStore);

   return pStore;
#endif // 0
}

/*************************************************************************************
ImageCacheRelease - Releases an image from the cache.

inputs
   PCImageStore      pImage - To Release
   BOOL              fForce - If TRUE, forces release right now. Sometimes not released
                     right away
*/
void ImageCacheRelease (PCImageStore pImage, BOOL fForce)
{
   // find it in the image cache and remove it there
   DWORD i;
   PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   for (i = 0; i < glIMAGECACHE.Num(); i++, pic++)
      if (pic->pImage == pImage)
         break;

   if (i < glIMAGECACHE.Num()) {
      if (pic->dwRefCount)
         pic->dwRefCount--;
      if (pic->dwRefCount)
         return;  // do nothing

      if (fForce) {
         delete pic->pImage;
         delete pic->pMemFile;
         glIMAGECACHE.Remove (i);
      }
      else
         // keep around for awhile no matter what
         pic->dwLastUsed = GetTickCount();

#if 0 // old code
      // if this ISN'T a 360, then keep around for awhile
      // in case access soon, such as for lip sync
      if (!fForce && (pic->pImage->m_dwStretch != 4)) {
         pic->dwLastUsed = GetTickCount();
         return;
      }

      // if this is a 360 then delete right away
      delete pic->pImage;
      delete pic->pMemFile;
      glIMAGECACHE.Remove (i);
#endif // 0
   }
}


/*************************************************************************************
ImageCacheTimer - Call this once in awhile to release old images that
are no longer used.

inputs
   BOOL           fForceUnCache - If TRUE then everything will be uncached unless
                        it still has a reference coun
*/
void ImageCacheTimer (BOOL fForceUnCache)
{
   DWORD dwNow = GetTickCount();
   DWORD dwMaxCache = (sizeof(PVOID) > sizeof(DWORD)) ? IMAGECACHETIMER_MAX64 : IMAGECACHETIMER_MAX32;

   while (TRUE) {
      // find the oldest
      DWORD dwOldest = (DWORD)-1;
      DWORD dwOldestTime = 0;
      BOOL fForceDelete = FALSE;
      DWORD i;

      PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
      DWORD dwOldCached = 0;
      for (i = 0; i < glIMAGECACHE.Num(); i++) {
         if (pic[i].dwRefCount)
            continue;   // still open

         // keep track of how many old cached ones exist
         if (pic[i].pImage->m_dwStretch == 4)
            dwOldCached += 8; // since take a LOT of space
         else
            dwOldCached++;

         if (fForceUnCache || pic[i].fDead || (pic[i].dwLastUsed > dwNow)) {
                  // BUGFIX - If found dead one then remove
            // pretend this is the oldest
            dwOldest = i;
            dwOldestTime = pic[i].dwLastUsed;
            fForceDelete = TRUE;
            break;
         }

         if ((dwOldest == (DWORD)-1) || (pic[i].dwLastUsed < dwOldestTime)) {
            dwOldest = i;
            dwOldestTime = pic[i].dwLastUsed;
            // don't break
         }
      } // i

      // if there isn't any oldest then stop
      if (dwOldest == (DWORD)-1)
         break;

      // if cache not full then stop
      if (!fForceDelete && (dwOldCached < dwMaxCache))
         break;   // cache not large enough

      // else, free this
      delete pic[dwOldest].pImage;
      delete pic[dwOldest].pMemFile;
      glIMAGECACHE.Remove (dwOldest);
      dwOldest--;
      // pic = (PIMAGECACHE)glIMAGECACHE.Get(0);

      // repeat around to see if need to free more old ones

   } // while TRUE

#if 0 // old code
   DWORD dwOld = dwNow - 1000 * 60;   // keep around for 60 seconds

   DWORD i;
   PIMAGECACHE pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   for (i = 0; i < glIMAGECACHE.Num(); i++) {
      if (pic[i].dwRefCount)
         continue;   // still open

      if (!fForceUnCache && (pic[i].dwLastUsed >= dwOld) && (pic[i].dwLastUsed <= dwNow))
         continue;   // too new

      // else, delete
      delete pic[i].pImage;
      delete pic[i].pMemFile;
      glIMAGECACHE.Remove (i);
      i--;
      pic = (PIMAGECACHE)glIMAGECACHE.Get(0);
   } // i
#endif // 0
}

/*************************************************************************************
LinkFindBracket - This searches through the link name for a bracketed itme, like
<hello>.

inputs
   PWSTR       psz - String to look in
   BOOL        fIgnoreObject - If finds the "<object>" or "<click>" string, will ignore it here.
   DWORD       *pdwStart - Filled in with the first bracket found (index)
   DWORD       *pdwEnd - Filled in the the last bracked found (index)
returns
   BOOL - TRUE if found bracket, FALSE if found nothing
*/
BOOL LinkFindBracket (PWSTR psz, BOOL fIgnoreObject, DWORD *pdwStart, DWORD *pdwEnd)
{
   PWSTR pszLeft = psz;
   for (pszLeft = wcschr(pszLeft, L'<'); pszLeft; pszLeft = wcschr(pszLeft, L'<')) {
      // find the right bracket
      PWSTR pszRight = wcschr(pszLeft, L'>');
      if (!pszRight)
         return FALSE;  // no right bracket

      // compare in-between
      if (fIgnoreObject) {
         PWSTR pszObject = L"<object>";
         PWSTR pszClick = L"<click>";
         if (!_wcsnicmp(pszLeft, pszObject, wcslen(pszObject)) || !_wcsnicmp(pszLeft, pszClick, wcslen(pszClick))) {
            pszLeft++;
            continue;
         }
      }

      // found
      *pdwStart = (DWORD) ((PBYTE)pszLeft -  (PBYTE)psz) / sizeof(WCHAR);
      *pdwEnd = (DWORD) ((PBYTE)pszRight -  (PBYTE)psz) / sizeof(WCHAR);

      return TRUE;
   };

   // if get here none
   return FALSE;
}


/*************************************************************************************
LinkReplace - Replace the text from dwStart to dwEnd (inclusive) with the new
text.

inputs
   PCMem       pMem - Memory containing the link name
   PWSTR       pszNew - New text
   DWORD       dwStart - Start char
   DWORD       dwEnd - End char
returns
   none
*/
void LinkReplace (PCMem pMem, PWSTR pszNew, DWORD dwStart, DWORD dwEnd)
{
   DWORD dwLenOrig = dwEnd - dwStart + 1;
   DWORD dwLenNew = (DWORD)wcslen(pszNew);
   DWORD dwLenString = (DWORD)wcslen((PWSTR)pMem->p)+1;
   if (!pMem->Required ((dwLenString + dwLenNew - dwLenOrig)*sizeof(WCHAR)))
      return;
   PWSTR psz = (PWSTR)pMem->p;
   memmove (psz + (dwStart + dwLenNew), psz + (dwEnd+1), (dwLenString - dwEnd - 1) * sizeof(WCHAR));
   memcpy (psz + dwStart, pszNew, dwLenNew * sizeof(WCHAR));
}


/*************************************************************************************
LinkControlValue - Identifies the value for the control name, putting it into
pValue.

inputs
   PWSTR       pszControl - Control to look for
   PCEscPage   pPage - Page to look in
   PCMem       pValue - Filled with the value of the control (assuming can get)
   BOOL        *pfShouldHide - Set to TRUE if the edit control should hide the text
               in the transcript since it's a password. This is NOT set otherwise.
returns
   BOOL - TRUE if managed to get the value, FALSE if not. If cant determine value
            then will fill pValue with ""
*/
BOOL LinkControlValue (PWSTR pszControl, PCEscPage pPage, PCMem pValue, BOOL *pfShouldHide)
{
   MemZero (pValue);

   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   // see if it's a scrollbar
   if (pControl->AttribGetInt (L"min") != pControl->AttribGetInt(L"max")) {
      MemCat (pValue, (int)pControl->AttribGetInt (L"pos"));
      return TRUE;
   }

   // date
   int iYear = pControl->AttribGetInt (L"year");
   if (iYear) {
      MemCat (pValue, (int)pControl->AttribGetInt (L"day"));
      MemCat (pValue, L"/");
      MemCat (pValue, (int)pControl->AttribGetInt (L"month"));
      MemCat (pValue, L"/");
      MemCat (pValue, iYear);
      return TRUE;
   }

   WCHAR szTemp[64];
   DWORD dwNeed;
   if (pControl->AttribGet(L"hour", szTemp, sizeof(szTemp), &dwNeed) && dwNeed) {
      MemCat (pValue, (int)pControl->AttribGetInt (L"hour"));
      MemCat (pValue, L":");
      int iMinute = pControl->AttribGetInt (L"minute");
      MemCat (pValue, iMinute / 10);
      MemCat (pValue, iMinute % 10);
      return TRUE;
   }

   // get list box
   int iCurSel = pControl->AttribGetInt (L"cursel");
   if (iCurSel >= 0) {
      ESCMLISTBOXGETITEM gi;
      memset (&gi, 0, sizeof(gi));
      gi.dwIndex = (DWORD)iCurSel;
      if (pControl->Message (ESCM_LISTBOXGETITEM, &gi) && gi.pszName) {
         MemCat (pValue, gi.pszName);
         return TRUE;
      }

      ESCMCOMBOBOXGETITEM ci;
      memset (&ci, 0, sizeof(ci));
      ci.dwIndex = (DWORD)iCurSel;
      if (pControl->Message (ESCM_COMBOBOXGETITEM, &ci) && ci.pszName) {
         MemCat (pValue, ci.pszName);
         return TRUE;
      }
   }

   // assume it's an edit control
   {
      CMem memTemp;

      dwNeed = 0;
      pControl->AttribGet (Text(), NULL, 0, &dwNeed);
      if (!dwNeed)
         return FALSE;
      if (!memTemp.Required(dwNeed))
         return FALSE;
      pControl->AttribGet (Text(), (PWSTR)memTemp.p, (DWORD)memTemp.m_dwAllocated, &dwNeed);

      // BUGFIX - Sanitize anything from an edit control
      MemZero (pValue);
      MemCatSanitize (pValue, (PWSTR)memTemp.p);
   }

   if (pControl->AttribGetBOOL (L"password"))
      *pfShouldHide = TRUE;

   return TRUE;
}



/*************************************************************************************
CVisImage::Constructor and destructor
*/
CVisImage::CVisImage (void)
{
   m_dwID = -1;
   m_gID = GUID_NULL;
   m_pNodeMenu = NULL;
   m_iNodeMenuTime = 0;
   m_pNodeSliders = NULL;
   m_pNodeHypnoEffect = NULL;
   m_lOBJECTSLIDERSOld.Init (sizeof(OBJECTSLIDERS));
   memset (&m_gIDWhenSliders, 0, sizeof(m_gIDWhenSliders));
   memset (&m_rSliders, 0, sizeof(m_rSliders));

   // m_hBmp = NULL;
   m_plPCCircumrealityHotSpot = NULL;
   m_pMain = NULL;
   m_pPage = NULL;
   m_hWnd = NULL;
   m_fIconic = FALSE;
   MemZero (&m_memName);
   MemZero (&m_memOther);
   MemZero (&m_memDescription);
   m_pisLayered = NULL;
   m_hBmpLayered = NULL;
   m_fSpeaking = FALSE;
   m_fVisibleAsSmall = TRUE;
   m_fCanChatTo = FALSE;
   m_fLipSync = FALSE;
   m_fBlinkWait = 0;
   m_dwViseme = 0;

   MasterMMLSet (NULL);

   m_lVILAYER.Init (sizeof(VILAYER));
   m_lOldLayersStretch.Init (sizeof(DWORD));

   memset (&m_rClient, 0 ,sizeof(m_rClient));
}

CVisImage::~CVisImage (void)
{
   if (m_pMain && (m_pMain->m_pMenuContext == this)) {
      m_pMain->m_pMenuContext = NULL;
      m_pMain->MenuReDraw();
   }
   if (m_pMain)
      m_pMain->VisImageTimer (this, 0);

   Clear(FALSE);
}


/*************************************************************************************
CVisImage::Init - Sets the PCMainWindow

inputs
   PCMainWindow      pMain - Main window
   DWORD             dwID - ID to use
   GUID              *pgID - ID for object
   HWND              hWnd - Window where will be drawn
   BOOL              fIconic - TRUE if it's iconic
   BOOL              fCanChatTo - Flag for whether can chat to this NPC
   __int64         iTime - Time event when this occurs
returns
   BOOL- TRUEif success
*/
BOOL CVisImage::Init (PCMainWindow pMain,
                      DWORD dwID, GUID *pgID, HWND hWnd,
                      BOOL fIconic, BOOL fCanChatTo, __int64 iTime)
{
   // keep the most recent menu
   _ASSERTE (!m_pNodeMenu || (iTime >= m_iNodeMenuTime));

   // if already around then clear. That way can call a second time with new data
   if (m_pMain || m_hWnd || m_lVILAYER.Num() || m_pNodeMenu || m_pNodeSliders || m_pNodeHypnoEffect || m_lOBJECTSLIDERSOld.Num())
      Clear (FALSE);

   m_pMain = pMain;
   m_hWnd = hWnd;
   m_fIconic = fIconic;
   m_fCanChatTo = fCanChatTo;

   m_pNodeMenu = NULL;
   m_iNodeMenuTime = 0;
   m_pNodeSliders = NULL;
   m_pNodeHypnoEffect = NULL;
   m_lOBJECTSLIDERSOld.Init (sizeof(OBJECTSLIDERS));
   m_gIDWhenSliders = *pgID;
   memset (&m_rSliders, 0, sizeof(m_rSliders));

   MemZero (&m_memName);
   MemZero (&m_memOther);
   MemZero (&m_memDescription);

   m_dwID = dwID;
   m_gID = *pgID;

   return TRUE;
}



/*************************************************************************************
CVisImage::MasterMMLSet - Sets the master MML in case need to use with lipsync.

inputs
   PWSTR             pszMML - MML. This can be NULL
returns
   none
*/
void CVisImage::MasterMMLSet (PWSTR pszMML)
{
   MemZero (&m_memMaster);
   if (pszMML)
      MemCat (&m_memMaster, pszMML);
   m_fLipSync = (gpMainWindow->m_fLipSync || gpMainWindow->m_fLipSyncLow) && VisemeModifyMML(0, (PWSTR)m_memMaster.p, NULL);
      // NOTE: Only if lip sync is turned on main
   m_fBlinkWait = m_fLipSync ? randf(BLINKMIN, BLINKMAX) : 0;
   m_dwViseme = 0;   // since if just set, reset the viseme
}

/*************************************************************************************
CVisImage::Update - Updates the visview with new information to be displayed

inputs
   PCMMLNode2         pNode - Node to use. This node is KEPT and deleted by the function
   PWSTR              pszMemNodeAsMML - Node as MML text. If this is NULL then one is automatically created.
   PCMMLNode2         pNodeMenu - <ContextMenu> node that describes the context sensative
                        menu. This could be NULL. This node is KEPT and deleted by
                        the function
   PCMMLNode2        pNodeSliders - <Sliders> node that describes any sliders. This
                     could be NULL. This node is KEPT and deleted by the object.
   PCMMLNode2        pNodeHypnoEffect - <HypnoEffect> node that describes any hypno effects. This
                     could be NULL. This node is KEPT and deleted by the object.
   PWSTR             pszName - Name of the object
   PWSTR             pszOther - Other description
   PWSTR             pszDescription - Description of object like, "This is an expensive looking ring."
   BOOL              fForViseme - If this is TRUE, then updating for a new viseme, so don't change anything else except pNode.
                        In that case, pNodeMenu, pNodeSliders, pszName, pszDescription, and pszOther are unused
   BOOL              fCanChatTo - Sets m_fCanChatTo, whether can chat to this NPC
   __int64         iTime - Time when the request original came in. So menu is kept as latest
returns
   BOOL- TRUE if success
*/
BOOL CVisImage::Update (PCMMLNode2 pNode, PWSTR pszMemNodeAsMML, PCMMLNode2 pNodeMenu, PCMMLNode2 pNodeSliders,
                        PCMMLNode2 pNodeHypnoEffect,
                      PWSTR pszName, PWSTR pszOther, PWSTR pszDescription, BOOL fForViseme, BOOL fCanChatTo,
                      __int64 iTime)
{
   m_fCanChatTo = fCanChatTo;

   if (!fForViseme) {
      // copy over the name
      MemZero (&m_memName);
      MemCat (&m_memName, pszName);
      MemZero (&m_memOther);
      MemCat (&m_memOther, pszOther);
      MemZero (&m_memDescription);
      MemCat (&m_memDescription, pszDescription);

      // if the old menu is actually more recent then make sure keep that
      if (m_pNodeMenu && pNodeMenu && (iTime < m_iNodeMenuTime)) {
         // keep old
         if (pNodeMenu)
            delete pNodeMenu;
         pNodeMenu = m_pNodeMenu;
         m_pNodeMenu = NULL;
         iTime = m_iNodeMenuTime;
      }

      // free up the old menu
      if (m_pNodeMenu)
         delete m_pNodeMenu;
      m_pNodeMenu = pNodeMenu;
      m_iNodeMenuTime = iTime;
      if (m_pNodeSliders)
         delete m_pNodeSliders;
      m_pNodeSliders = pNodeSliders;
      if (m_pNodeHypnoEffect)
         delete m_pNodeHypnoEffect;
      m_pNodeHypnoEffect = pNodeHypnoEffect;
   }


   VILAYER vi;
   memset (&vi, 0, sizeof(vi));
   vi.lid = DEFLANGID;
   vi.fWaiting = TRUE;

   vi.pNode = pNode;
   vi.pMem = new CMem;
   if (!vi.pMem) {
      delete vi.pNode;

      if (!fForViseme) {
         if (m_pNodeMenu)
            delete m_pNodeMenu;
         m_pNodeMenu = NULL;
         if (m_pNodeSliders)
            delete m_pNodeSliders;
         m_pNodeSliders = NULL;
         if (m_pNodeHypnoEffect)
            delete m_pNodeHypnoEffect;
         m_pNodeHypnoEffect = NULL;
      }
      return FALSE;
   }
   if (pszMemNodeAsMML) {
      // BUGFIX - If already have MML then just copy
      MemZero (vi.pMem);
      MemCat (vi.pMem, pszMemNodeAsMML);
   }
   else {
      if (!MMLToMem (vi.pNode, vi.pMem)) {
         delete vi.pMem;
         delete vi.pNode;

         if (!fForViseme) {
            if (m_pNodeMenu)
               delete m_pNodeMenu;
            m_pNodeMenu = NULL;
            if (m_pNodeSliders)
               delete m_pNodeSliders;
            m_pNodeSliders = NULL;
            if (m_pNodeHypnoEffect)
               delete m_pNodeHypnoEffect;
            m_pNodeHypnoEffect = NULL;
         }
         return FALSE;
      }
      vi.pMem->CharCat (0);   // so null terminated
   }

#if 0    // take out because doesnt work
   // BUGFIX - Since sometimes 0.3333333332 gets converted to 0.333333, need to
   // parse the output and re-put back in, so can do exact string match
   PCMMLNode2 pTemp = CircumrealityParseMML ((PWSTR)vi.pMem->p);
   if (!pTemp) {
      delete vi.pMem;
      delete vi.pNode;

      if (!fForViseme) {
         if (m_pNodeMenu)
            delete m_pNodeMenu;
         m_pNodeMenu = NULL;
         if (m_pNodeSliders)
            delete m_pNodeSliders;
         m_pNodeSliders = NULL;
         not doing hypnoeffect
      }
      return FALSE;
   }
   vi.pMem->m_dwCurPosn = 0;
   ((PWSTR)vi.pMem->p)[0] = 0;
   if (!MMLToMem (vi.pNode, vi.pMem)) {
      delete vi.pMem;
      delete vi.pNode;

      if (!fForViseme) {
         if (m_pNodeMenu)
            delete m_pNodeMenu;
         m_pNodeMenu = NULL;
         if (m_pNodeSliders)
            delete m_pNodeSliders;
         m_pNodeSliders = NULL;
         not doing hypnoeffect
      }
      return FALSE;
   }
   vi.pMem->CharCat (0);   // so null terminated
   delete pTemp;
#endif // 0



   // if there's an existing layer and it's the same as this layer then
   // dont add
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   if (pvi && !_wcsicmp((PWSTR)vi.pMem->p, (PWSTR)pvi->pMem->p)) {
      // already have this showing, so dont update
      delete vi.pMem;
      delete vi.pNode;
   }
   else {
      // go through with adding it

      // get the transition
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      vi.pNode->ContentEnum (vi.pNode->ContentFind (CircumrealityTransition()), &psz, &pSub);
      vi.pTransition = new CTransition;
      if (vi.pTransition) {
         vi.pTransition->MMLFrom (pSub);
         if (fForViseme) {
            // speed up fades if there are any
            vi.pTransition->m_fFadeInDur = 0; // min( vi.pTransition->m_fFadeInDur, 0.1); // make sure not too slow
            vi.pTransition->m_fFadeFromDur = 0; // min( vi.pTransition->m_fFadeFromDur, 0.1); // make sure not too slow
               // NOTE: If completely eliminate then sometimes shows "drawing" for transition
               // works best with 0 transition time
         }
         vi.pTransition->AnimateInit ();
      }
      m_lVILAYER.Insert (0, &vi);

      // BUGFIX - Keep a history of layers
      PWSTR pszOldLayer = (vi.pMem && vi.pMem->p) ? (PWSTR)vi.pMem->p : NULL;
      if (pszOldLayer && !fForViseme) {   // BUGFIX - Don't keep copy of visemes around because too many
         m_lOldLayers.Insert (0, pszOldLayer, (wcslen(pszOldLayer)+1)*sizeof(WCHAR) );
         DWORD dwStretch = 0;
         m_lOldLayersStretch.Insert (0, &dwStretch);

         // make sure this cache doesn't get too large
         while (m_lOldLayers.Num() > 10) {
            m_lOldLayers.Remove (m_lOldLayers.Num()-1);
            m_lOldLayersStretch.Remove (m_lOldLayersStretch.Num()-1);
         }
      }

      // reset the master MML
      if (!fForViseme) {
         MasterMMLSet ((PWSTR)vi.pMem->p);

         // BUGFIX - clear the old hotspots
         if (m_plPCCircumrealityHotSpot) {
            HotSpotsClear ();
            delete m_plPCCircumrealityHotSpot;
            m_plPCCircumrealityHotSpot = NULL;
         }
      }
   }

   // clear out old just in case
   EliminateDeadLayers ();
   LayeredClear ();

   // may need to update the display
   // NOTE: This will also update if description changed
   if (!fForViseme && m_pMain && (m_pMain->m_pMenuContext == this))
      m_pMain->MenuReDraw();

   // invalidate this just in case, ensure that description is redrawn
   InvalidateRect (m_hWnd, &m_rClient, FALSE);

   if (!fForViseme) {
      // BUGFIX - Update right away so that if doing a <Cutscene> that requests a cached
      // 360 image, it will eventually be loaded
      // make sure is drawn right away
      SeeIfRendered ();

      // NOTE: not making sure that it's updated right away, like a viseme,
      // because may not be necessary for such immediate response
   }

   return TRUE;
}




/*************************************************************************************
CVisImage::RectGet - Gets the rectangle where the image is

inputs
   RECT        *pr - Filled in
*/
void CVisImage::RectGet (RECT *pr)
{
   *pr = m_rClient;
}

/*************************************************************************************
CVisImage::RectSet - Sets the rectangle (in client for m_hWnd) where the
image will be.

inputs
   RECT        *prNew - New rectangle in m_hWnd coords
   BOOL        fMoved - Set to TRUE if rect-set because of a move. If this is the
               case, and have an escarpment window, will need to update its current location
*/
void CVisImage::RectSet (RECT *prNew, BOOL fMoved)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD dwLayer;
   if ((m_rClient.left == prNew->left) && (m_rClient.right == prNew->right) &&
      (m_rClient.top == prNew->top) && (m_rClient.bottom == prNew->bottom)) {

         // look for layered with text window
         for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) {
            // if have an ecarpment window the move that
            if (pvi->pTextWindow) {
               // change font size
               // BUGFIX - Scale MML by player's font-size settings
               WORD wOld = EscFontScaleGet ();
               fp fScale = 256.0 * pow (sqrt(2.0), (fp)m_pMain->m_iTransSize);
               if (m_fIconic)
                  fScale /= 2.0;
               EscFontScaleSet ((WORD)fScale);

               WindowBackground (FALSE);

               pvi->pTextWindow->PosnSet (&m_rClient);

               EscFontScaleSet (wOld);
            }
         } // dwLayer
         
         return;  // no change
      }

   // clear layered image
   LayeredClear();

   m_rClient = *prNew;

   // BUGFIX - Recalc 360 view
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) {
      // if have an ecarpment window the move that
      if (pvi->pTextWindow) {
         // change font size
         // BUGFIX - Scale MML by player's font-size settings
         WORD wOld = EscFontScaleGet ();
         fp fScale = 256.0 * pow (sqrt(2.0), (fp)m_pMain->m_iTransSize);
         if (m_fIconic)
            fScale /= 2.0;
         EscFontScaleSet ((WORD)fScale);

         WindowBackground (FALSE);

         pvi->pTextWindow->PosnSet (&m_rClient);

         EscFontScaleSet (wOld);
      }

      if (Is360(pvi)) {
         // see if width changed
         pvi->dwWidth = (DWORD)max(m_rClient.right - m_rClient.left,1);
         pvi->dwHeight = (DWORD)max(m_rClient.bottom - m_rClient.top,1);
         pvi->dwStretch = 0;  // no stretching for this

         // BUGFIX - Since changed way image worked, just call Surroudn360Set
         pvi->pImage->Surround360Set (m_fIconic ? 1 : 0, m_pMain->m_f360FOV, m_pMain->m_f360Lat, pvi->dwWidth, pvi->dwHeight,
            m_pMain->m_fCurvature);

   #if 0 // DEADCODE
         DWORD dwWidth = (DWORD)(m_rClient.right - m_rClient.left);
         DWORD dwHeight = (DWORD)(m_rClient.bottom - m_rClient.top);
         if ((dwWidth != m_dwWidth) || (dwHeight != m_dwHeight)) {
            // 360 degree image
            m_dwWidth = dwWidth;
            m_dwHeight = dwHeight;
            m_dwStretch = 0;  // no stretching for this

            if (m_hBmp)
               DeleteObject (m_hBmp);
            HDC hDC = GetDC (m_hWnd);

            // get the rectangle for size
            m_pImage->Surround360Set (m_fIconic ? 1 : 0, m_pMain->m_f360FOV, m_pMain->m_f360Lat, m_dwWidth, m_dwHeight);
            m_hBmp = m_pImage->Surround360ToBitmap (m_fIconic ? 1 : 0, hDC, m_pMain->m_f360Long);

            ReleaseDC(m_hWnd, hDC);
         }
   #endif // DEADCODE
      }
   } // dwLayer
   // redraw
   InvalidateRect (m_hWnd, &m_rClient, FALSE);
}

/*************************************************************************************
CVisImage::HotSpotsClear - Clear out the existing hot spots
*/
void CVisImage::HotSpotsClear(void)
{
   if (!m_plPCCircumrealityHotSpot)
      return;

   DWORD i;
   PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*) m_plPCCircumrealityHotSpot->Get(0);
   for (i = 0; i < m_plPCCircumrealityHotSpot->Num(); i++, pph++)
      delete pph[0];
   m_plPCCircumrealityHotSpot->Clear();
}



/*************************************************************************************
CVisImage::Clear - Clears a specific image.

inputs
   BOOL                 fInvalidate - If TRUE then invalidate the rectangle
*/
void CVisImage::Clear (BOOL fInvalidate)
{
   // invalidate?
   if (fInvalidate && m_hWnd)
      InvalidateRect (m_hWnd, &m_rClient, FALSE);

   while (m_lVILAYER.Num())
      LayerClear (m_lVILAYER.Num()-1);

   LayeredClear ();

   MasterMMLSet (NULL);

   if (m_pNodeMenu)
      delete m_pNodeMenu;
   m_pNodeMenu = NULL;
   if (m_pNodeSliders)
      delete m_pNodeSliders;
   m_pNodeSliders = NULL;
   if (m_pNodeHypnoEffect)
      delete m_pNodeHypnoEffect;
   m_pNodeHypnoEffect = NULL;

   // don't do
   //if (m_lOBJECTSLIDERSOld)
   //   m_lOBJECTSLIDERSOld.Init (sizeof(OBJECTSLIDERS));

   //if (m_hBmp)
   //   DeleteObject (m_hBmp);
   //m_hBmp = NULL;

   if (m_plPCCircumrealityHotSpot) {
      HotSpotsClear ();
      delete m_plPCCircumrealityHotSpot;
      m_plPCCircumrealityHotSpot = NULL;
   }
}


/*************************************************************************************
CVisImage::LayerGetImage - Gets the image from the top layer

inputs
   none
returns
   PCImageStore - Image that can use. DON'T delete.
*/
PCImageStore CVisImage::LayerGetImage (void)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD i;
   for (i = 0; i < m_lVILAYER.Num(); i++, pvi++) {
      if (pvi->pImage)
         return pvi->pImage;
   } // i

   return NULL; // not found
}


/*************************************************************************************
CVisImage::LayerFindMatch - See if the layer matches the given string.

inputs
   PWSTR       pszMatch - MML string to match
   BOOL        fLast360 - Only if this is the last 360 image found
returns
   BOOL - TRUE if found
*/
BOOL CVisImage::LayerFindMatch (PWSTR pszMatch, BOOL fLast360)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD i;
   BOOL fFound360 = FALSE;
   for (i = 0; i < m_lVILAYER.Num(); i++, pvi++) {
      if (pvi->pMem && pvi->pMem->p) {
         if (!_wcsicmp ((PWSTR)pvi->pMem->p, pszMatch))
            return TRUE;
      }

      // remember if found 360 already
      if ((pvi->dwStretch == 4) || (pvi->pImage && pvi->pImage->m_dwStretch == 4))
         fFound360 = TRUE;

      // if we found a 360 image and looking for the last 360, then about
      if (fFound360 && fLast360)
         return FALSE;
   } // i

   // look through the cache
   DWORD *padwStretch = (DWORD*)m_lOldLayersStretch.Get(0);
   for (i = 0; i < m_lOldLayers.Num(); i++, padwStretch++) {
      if (padwStretch[0] == 4)
         fFound360 = TRUE;

      if (!_wcsicmp((PWSTR)m_lOldLayers.Get(i), pszMatch))
         return TRUE;

      // if we found a 360 image and looking for the last 360, then about
      if (fFound360 && fLast360)
         return FALSE;
   }

   return FALSE;  // not found

}

/*************************************************************************************
CVisImage::LayerClear - Clears out a layer.

inputs
   DWORD       dwLayer - Layer
returns
   BOOL - TRUE if success
*/
BOOL CVisImage::LayerClear (DWORD dwLayer)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(dwLayer);
   if (!pvi)
      return FALSE;

   if (pvi->pNode)
      delete pvi->pNode;

   if (pvi->pMem)
      delete pvi->pMem;

   if (pvi->pImage)
      ImageCacheRelease (pvi->pImage, FALSE);

   if (pvi->pTextWindow)
      delete pvi->pTextWindow;

   if (pvi->pTransition)
      delete pvi->pTransition;

#ifdef _DEBUG
   memset (pvi, 1, sizeof(*pvi));
#endif

   m_lVILAYER.Remove (dwLayer);

   if (!dwLayer)
      MasterMMLSet (NULL);

   return TRUE;
}



/*************************************************************************************
CVisImage::FuzzyLocation - Fills in the location of the fuzzy overlay area, as
well as the locations of the description and sliders.

inputs
   HDC            hDC - HDC to use. If not, uses the visimage DC.
   RECT           *prFuzzy - If not NULL, filled in with the location of the fuzzy.
                  If size of (0,0) then nothing to draw
   RECT           *prDescription - If not NULL, filled in the the location of the
                  description area, with 0,0 being UL of fuzzy bubble.
                  If filled with size of 0x0 then no decription.
   RECT           *prSliders - Like prDescription, except sliders.
   int            *piSliderLeft - From SlidersSize(). Optional
   int            *piSliderRight - From SlidersSize(). Optional
returns
   none
*/
void CVisImage::FuzzyLocation (HDC hDC, RECT *prFuzzy, RECT *prDescription, RECT *prSliders,
                           int *piSliderLeft, int *piSliderRight)
{
   POINT pSize;
   FuzzySize (hDC, &pSize, prDescription, prSliders, piSliderLeft, piSliderRight);

   // BUGFIX - Fill prFuzzy with 0
   if (prFuzzy)
      memset (prFuzzy, 0, sizeof(*prFuzzy));

   if (!pSize.x)
      return;

   // find a center
   POINT pCenter;
   pCenter.x = (m_rClient.right - m_rClient.left) / 2;
   pCenter.y = m_rClient.bottom - pSize.y / 2 + (int) m_pMain->m_adwFontPixelHeight[1] / 2;
      // BUGFIX - try to keep text as close to bottom as possible so
      // dont use too much screen real-estate
      // was - (m_rClient.bottom - m_rClient.top) / 16;

   // fill in rect (without center)
   RECT rFuzzy;
   rFuzzy.left = rFuzzy.top = 0;
   rFuzzy.right = pSize.x;
   rFuzzy.bottom = pSize.y;

   // offet
   POINT pOffset;
   pOffset.x = pCenter.x - pSize.x / 2;
   pOffset.y = pCenter.y - pSize.y / 2;

   // offset all
   OffsetRect (&rFuzzy, pOffset.x, pOffset.y);
   if (prFuzzy)
      *prFuzzy = rFuzzy;

   if (prDescription)
      OffsetRect (prDescription, pOffset.x, pOffset.y);
   if (prSliders)
      OffsetRect (prSliders, pOffset.x, pOffset.y);
}



/*************************************************************************************
CVisImage::DistantObjectFuzzyLocation - Fills in the location of the fuzzy overlay area, as
well as the locations of the description and sliders.

inputs
   HDC            hDC - HDC to use. If not, uses the visimage DC.
   PMAP360INFO    pInfo - Info about the exit
   RECT           *prFuzzy - If not NULL, filled in with the location of the fuzzy.
                  If size of (0,0) then nothing to draw
   RECT           *prDescription - If not NULL, filled in the the location of the
                  description area, with 0,0 being UL of fuzzy bubble.
                  If filled with size of 0x0 then no decription.
returns
   none
*/
void CVisImage::DistantObjectFuzzyLocation (HDC hDC, PMAP360INFO pInfo, RECT *prFuzzy, RECT *prDescription)
{
   // zero out
   // BUGFIX - Fill prFuzzy with 0
   if (prFuzzy)
      memset (prFuzzy, 0, sizeof(*prFuzzy));
   if (prDescription)
      memset (prDescription, 0, sizeof(*prDescription));


   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   if (!Is360(pvi))
      return;  // nothing, or not 360 degree image

   // number of pixels per radian
   fp fPixelsPerRad = (fp)max(m_rClient.right - m_rClient.left, 1) / (fp)max(m_pMain->m_f360FOV, CLOSE);
   fp fRadOffset = myfmod (pInfo->fAngle - m_pMain->m_f360Long + PI, 2.0 * PI) - PI;
   int iPixelOffset = (int) (fRadOffset * fPixelsPerRad);

   POINT pSize;
   DistantObjectFuzzySize (hDC, pInfo, &pSize, prDescription);

   if (!pSize.x)
      return;

   // find a center
   POINT pCenter;
   pCenter.x = (m_rClient.right - m_rClient.left) / 2 + iPixelOffset;
   pCenter.y = m_rClient.top + pSize.y / 2 - (int) m_pMain->m_adwFontSizedPixelHeight[1];

   // fill in rect (without center)
   RECT rFuzzy;
   rFuzzy.left = rFuzzy.top = 0;
   rFuzzy.right = pSize.x;
   rFuzzy.bottom = pSize.y;

   // offet
   POINT pOffset;
   pOffset.x = pCenter.x - pSize.x / 2;
   pOffset.y = pCenter.y - pSize.y / 2;

   // offset all
   OffsetRect (&rFuzzy, pOffset.x, pOffset.y);

   // make sure intersects with client
   RECT rInter;
   if (!IntersectRect (&rInter, &rFuzzy, &m_rClient)) {
      if (prDescription)
         memset (prDescription, 0, sizeof(*prDescription));
      return;  // nothing
   }

   if (prFuzzy)
      *prFuzzy = rFuzzy;

   if (prDescription)
      OffsetRect (prDescription, pOffset.x, pOffset.y);
}



/*************************************************************************************
CVisImage::FuzzySize - Fills in the size of the fuzzy bubble that is used
to display the text and sliders.

inputs
   HDC            hDC - HDC to use. If not, uses the visimage DC.
   POINT          *pSize - Fills in the width and height. If 0,0 then no sliders
   RECT           *prDescription - If not NULL, filled in the the location of the
                  description area, with 0,0 being UL of fuzzy bubble.
                  If filled with size of 0x0 then no decription.
   RECT           *prSliders - Like prDescription, except sliders.
   int            *piSliderLeft - From SlidersSize(). Optional
   int            *piSliderRight - From SlidersSize(). Optional
returns
   none
*/
void CVisImage::FuzzySize (HDC hDC, POINT *pSize, RECT *prDescription, RECT *prSliders,
                           int *piSliderLeft, int *piSliderRight)
{
   // zero out
   pSize->x = pSize->y;
   if (prDescription)
      memset (prDescription, 0, sizeof(*prDescription));
   if (prSliders)
      memset (prSliders, 0, sizeof(*prSliders));
   if (piSliderLeft)
      *piSliderLeft = 0;
   if (piSliderRight)
      *piSliderRight = 0;
   // BUGFIX - Forgot to zero this out
   if (pSize)
      pSize->x = pSize->y = 0;
   // BUGFIX

   HDC hDCCreated = NULL;
   if (!hDC)
      hDC = hDCCreated = GetDC (m_hWnd);

   m_pMain->FontCreateIfNecessary (hDC);

   // buffer size is large font size
   int iBuffer = (int)m_pMain->m_adwFontPixelHeight[1];

   // get other sizes
   POINT pSizeDescription, pSizeSliders;
   DescriptionSize (hDC, &pSizeDescription);
   SlidersSize (hDC, &pSizeSliders, piSliderLeft, piSliderRight);

   if (hDCCreated)
      ReleaseDC (m_hWnd, hDCCreated);

   // if neither sliders nor description then exit
   if (!pSizeDescription.x && !pSizeSliders.x)
      return;

   // description location
   RECT rDescription;
   rDescription.left = rDescription.top = 0;
   rDescription.right = pSizeDescription.x;
   rDescription.bottom = pSizeDescription.y;

   // slider location
   RECT rSliders;
   rSliders.left = rSliders.top = 0;
   rSliders.right = pSizeSliders.x;
   rSliders.bottom = pSizeSliders.y;

   // offset sliders to right if there's a description
   if (rDescription.right && rSliders.right)
      OffsetRect (&rSliders, iBuffer, 0); // buffer spacing

   OffsetRect (&rSliders, rDescription.right, 0);  // so sliders to right of description

   // find max height, and offset vertically to center
   int iMaxHeight = max(rDescription.bottom - rDescription.top, rSliders.bottom - rSliders.top);
   OffsetRect (&rDescription, 0, (iMaxHeight - (rDescription.bottom - rDescription.top)) / 2);
   OffsetRect (&rSliders, 0, (iMaxHeight - (rSliders.bottom - rSliders.top)) / 2);

   // offset both rects by fuzzy boundary
   OffsetRect (&rDescription, iBuffer, iBuffer);
   OffsetRect (&rSliders, iBuffer, iBuffer);

   // size
   pSize->x = rSliders.right + iBuffer;
   pSize->y = iMaxHeight + 2 * iBuffer;
   if (prDescription)
      *prDescription = rDescription;
   if (prSliders)
      *prSliders = rSliders;
}

/*************************************************************************************
CVisImage::DistantObjectFuzzySize - Fills in the size of the fuzzy bubble that is used
to display the text.

inputs
   HDC            hDC - HDC to use. If not, uses the visimage DC.
   PMAP360INFO    pInfo - Info about the exit
   POINT          *pSize - Fills in the width and height. If 0,0 then no sliders
   RECT           *prDescription - If not NULL, filled in the the location of the
                  description area, with 0,0 being UL of fuzzy bubble.
                  If filled with size of 0x0 then no decription.
returns
   none
*/
void CVisImage::DistantObjectFuzzySize (HDC hDC, PMAP360INFO pInfo,
                                        POINT *pSize, RECT *prDescription)
{
   // zero out
   pSize->x = pSize->y;
   if (prDescription)
      memset (prDescription, 0, sizeof(*prDescription));
   // BUGFIX - Forgot to zero this out
   if (pSize)
      pSize->x = pSize->y = 0;
   // BUGFIX

   HDC hDCCreated = NULL;
   if (!hDC)
      hDC = hDCCreated = GetDC (m_hWnd);

   m_pMain->FontCreateIfNecessary (hDC);

   // buffer size is large font size
   int iBuffer = (int)m_pMain->m_adwFontSizedPixelHeight[1];

   // get other sizes
   POINT pSizeDescription;
   DistantObjectsSize (hDC, pInfo, &pSizeDescription);

   if (hDCCreated)
      ReleaseDC (m_hWnd, hDCCreated);

   // if neither sliders nor description then exit
   if (!pSizeDescription.x)
      return;

   // description location
   RECT rDescription;
   rDescription.left = rDescription.top = 0;
   rDescription.right = pSizeDescription.x;
   rDescription.bottom = pSizeDescription.y;

   // find max height, and offset vertically to center
   int iMaxHeight = rDescription.bottom - rDescription.top;
   OffsetRect (&rDescription, 0, (iMaxHeight - (rDescription.bottom - rDescription.top)) / 2);

   // offset both rects by fuzzy boundary
   OffsetRect (&rDescription, iBuffer, iBuffer);

   // size
   pSize->x = rDescription.right + iBuffer;
   pSize->y = iMaxHeight + 2 * iBuffer;
   if (prDescription)
      *prDescription = rDescription;
}

/*************************************************************************************
CVisImage::DescriptionSize - Size of the object's description

inputs
   HDC            hDC - HDC to use. If not, uses the visimage DC.
   POINT          *pSize - Fills in the width and height. If 0,0 then no sliders
returns
   none
*/
void CVisImage::DescriptionSize (HDC hDC, POINT *pSize)
{
   pSize->x = pSize->y = 0;

   // largest rect
   RECT rLargest;
   rLargest.top = rLargest.left = 0;
   rLargest.right = max(2, m_rClient.right - m_rClient.left) / 2; // can never be wider than half the client
   rLargest.bottom = 10000;

   HDC hDCCreated = NULL;
   if (!hDC)
      hDC = hDCCreated = GetDC (m_hWnd);

   // make sure fonts are calcualted
   m_pMain->FontCreateIfNecessary (hDC);

   // title
   PWSTR psz;
   HFONT hFontOld;
   RECT rSize;
   psz = (PWSTR) m_memName.p;
   if (psz && psz[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[1]);
      rSize = rLargest;
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      SelectObject (hDC, hFontOld);

      pSize->x = max(pSize->x, rSize.right);
      pSize->y += rSize.bottom + (int)m_pMain->m_adwFontPixelHeight[1] / 2;
   }

   // other
   psz = (PWSTR) m_memOther.p;
   if (psz && psz[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[2]);
      rSize = rLargest;
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      SelectObject (hDC, hFontOld);

      pSize->x = max(pSize->x, rSize.right);
      pSize->y += rSize.bottom + (int)m_pMain->m_adwFontPixelHeight[2] / 2;
   }

   // descriptions
   psz = (PWSTR) m_memDescription.p;
   if (psz && psz[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[0]);
      rSize = rLargest;
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      SelectObject (hDC, hFontOld);

      pSize->x = max(pSize->x, rSize.right);
      pSize->y += rSize.bottom;
   }

   if (hDCCreated)
      ReleaseDC (m_hWnd, hDCCreated);
}


/*************************************************************************************
CVisImage::DistantObjectsSize - Size of the distant-objects description

inputs
   HDC            hDC - HDC to use. If not, uses the visimage DC.
   PMAP360INFO    pInfo - Info about what's displayed
   POINT          *pSize - Fills in the width and height. If 0,0 then no sliders
returns
   none
*/

void CVisImage::DistantObjectsSize (HDC hDC, PMAP360INFO pInfo, POINT *pSize)
{
   DWORD dwRoomsDistant = pInfo->dwRoomDist;
   if (dwRoomsDistant)
      dwRoomsDistant--; // ignore room 0
   dwRoomsDistant = min(dwRoomsDistant, NUMMAINFONTSIZED-1);
   PWSTR pszText = pInfo->pszDescription;

   pSize->x = pSize->y = 0;

   // largest rect
   RECT rLargest;
   rLargest.top = rLargest.left = 0;
   rLargest.right = max(2, m_rClient.right - m_rClient.left) / 4; // can never be wider than 1/8 the client
   rLargest.bottom = 10000;

   HDC hDCCreated = NULL;
   if (!hDC)
      hDC = hDCCreated = GetDC (m_hWnd);

   // make sure fonts are calcualted
   m_pMain->FontCreateIfNecessary (hDC);

   // text
   HFONT hFontOld;
   RECT rSize;
   if (pszText && pszText[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFontSized[dwRoomsDistant]);
      rSize = rLargest;
      DrawTextW (hDC, pszText, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      SelectObject (hDC, hFontOld);

      pSize->x = max(pSize->x, rSize.right);
      pSize->y += rSize.bottom; // + (int)m_pMain->m_adwFontSizedPixelHeight[dwRoomsDistant] / 2;
   }

   if (hDCCreated)
      ReleaseDC (m_hWnd, hDCCreated);
}



/*************************************************************************************
CVisImage::SlidersSize - Determines the size of the sliders.

inputs
   HDC            hDC - HDC to use. If not, uses the visimage DC.
   POINT          *pSize - Fills in the width and height. If 0,0 then no sliders
   int            *piLeft - Left size, in pixels (including buffer). Can be NULL
   int            *piRight - Right size, in pixels (including buffer). Can be NULL
returns
   none
*/
void CVisImage::SlidersSize (HDC hDC, POINT *pSize, int *piLeft, int *piRight)
{
   // zero out
   pSize->x = pSize->y = 0;
   if (piLeft)
      *piLeft = 0;
   if (piRight)
      *piRight = 0;

   HDC hDCCreated = NULL;
   if (!hDC)
      hDC = hDCCreated = GetDC (m_hWnd);

   // make sure fonts are calcualted
   m_pMain->FontCreateIfNecessary (hDC);

   CListFixed lOBJECTSLIDERS;
   SlidersGetInterp (&lOBJECTSLIDERS);
   if (!lOBJECTSLIDERS.Num())
      goto done;

   HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[0]);

   // figure out if left/right
   BOOL fLeft = FALSE, fRight = FALSE;
   int iMaxHeight = m_pMain->m_adwFontPixelHeight[0]; // height of slider
   int iLeft = 0, iRight = 0;
   POBJECTSLIDERS pos = (POBJECTSLIDERS)lOBJECTSLIDERS.Get(0);
   RECT rSize;
   DWORD i;
   RECT rLargest;
   memset (&rLargest, 0, sizeof(rLargest));
   rLargest.right = rLargest.bottom = 10000;
   for (i = 0; i < lOBJECTSLIDERS.Num(); i++, pos++) {
      if (pos->szLeft[0]) {
         fLeft = TRUE;

         // figure out size
         rSize = rLargest;
         DrawTextW (hDC, pos->szLeft, -1, &rSize, DT_SINGLELINE | DT_CALCRECT);
         // iMaxHeight = max(iMaxHeight, rSize.bottom - rSize.top);
         iLeft = max(iLeft, rSize.right - rSize.left);
      }
      if (pos->szRight[0]) {
         fRight = TRUE;

         // figure out size
         rSize = rLargest;
         DrawTextW (hDC, pos->szRight, -1, &rSize, DT_SINGLELINE | DT_CALCRECT);
         // iMaxHeight = max(iMaxHeight, rSize.bottom - rSize.top);
         iRight = max(iRight, rSize.right - rSize.left);
      }
   } // i
   SelectObject (hDC, hFontOld);

   int iBuffer = iMaxHeight;
   int iRowHeight = iMaxHeight + iBuffer;

   if (iLeft)
      iLeft += iBuffer * 2;
   if (iRight)
      iRight += iBuffer * 2;
   if (piLeft)
      *piLeft = iLeft;
   if (piRight)
      *piRight = iRight;

   pSize->x = iLeft + iRight + iBuffer * 5;
   pSize->y = iRowHeight * (lOBJECTSLIDERS.Num() - 1) + iMaxHeight;

done:
   if (hDCCreated)
      ReleaseDC (m_hWnd, hDCCreated);
}



/*************************************************************************************
CVisImage::FuzzyBubblePaint - Paints the fuzzy bubble.

inputs
   HDC            hDC - To paint in
   RECT           *prFuzzy - Fuzzy location
   int            iBorder - Border size, in pixels
returns
   none
*/
void CVisImage::FuzzyBubblePaint (HDC hDC, RECT *prFuzzy, int iBorder)
{
   CMem memBlend;
   if (!memBlend.Required ( ((DWORD)iBorder + (DWORD)(iBorder * iBorder)) * 2 * sizeof(DWORD) ))
      return;
   DWORD *padwBlend = (DWORD*)memBlend.p;
   DWORD *padwBlendAdd = padwBlend + iBorder;
   DWORD *padwBlendCorner = padwBlendAdd + iBorder;
   DWORD *padwBlendCornerAdd = padwBlendCorner + iBorder * iBorder;

   RECT rInter, rInterRelative;
   if (!IntersectRect (&rInter, prFuzzy, &m_rClient))
      return;  // dont intersect
   rInterRelative = rInter;
   OffsetRect (&rInterRelative, -prFuzzy->left, -prFuzzy->top);

   // bit-blit into a temporary rect
   int iWidth = prFuzzy->right - prFuzzy->left;
   int iHeight = prFuzzy->bottom - prFuzzy->top;
   HDC hDCBmp = CreateCompatibleDC (hDC);
   HBITMAP hBmp = CreateCompatibleBitmap (hDC, iWidth, iHeight);
   SelectObject (hDCBmp, hBmp);

   RECT rBmp;
   rBmp.left = rBmp.top = 0;
   rBmp.right = iWidth;
   rBmp.bottom = iHeight;
   HBRUSH hbrBlack = CreateSolidBrush (m_pMain->m_fLightBackground ? RGB(0xe0,0xe0,0xe0) : RGB(0,0,0));
   FillRect (hDCBmp, &rBmp, hbrBlack);
   DeleteObject (hbrBlack);

   // copy to temporary bitmap
   BitBlt (hDCBmp,
      rInterRelative.left, rInterRelative.top,
      rInterRelative.right - rInterRelative.left,
      rInterRelative.bottom - rInterRelative.top,
      hDC, rInter.left, rInter.top, SRCCOPY);

   CImage Image;
   Image.Init (hBmp);
   DeleteDC (hDCBmp);
   DeleteObject (hBmp);

   // find out how bright the image is
   DWORD dwPixels = Image.Width() * Image.Height();
   __int64 iSum = 0;
   DWORD i;
   PIMAGEPIXEL pip = Image.Pixel(0,0);
   for (i = 0; i < dwPixels; i++, pip++) {
      iSum += (__int64) pip->wRed;
      iSum += (__int64) pip->wGreen;
      iSum += (__int64) pip->wBlue;
   } // i
   if (dwPixels)  // BUGFIX - Was crashing otherwise
      iSum /= (__int64)(dwPixels * 3);

#define MAXFUZZYBRIGHTNESS       0x1000      // if brighter than this then dim to this
#define MINFUZZYBRIGHTNESS       0x8000      // for light
#define MINFUZZYBRIGHTNESSINV    (0x10000 - MINFUZZYBRIGHTNESS)

   if (!m_pMain->m_fLightBackground && (iSum < MAXFUZZYBRIGHTNESS))
      return;  // already dark enough, so do nothing
   DWORD dwScale;
   if (m_pMain->m_fLightBackground)
      dwScale = MINFUZZYBRIGHTNESS;
   else
      dwScale = MAXFUZZYBRIGHTNESS * (DWORD) 0x10000 / (DWORD)iSum;

   // loop over center and make darker by scale
   RECT rDarker;
   rDarker.left = rDarker.top = iBorder;
   rDarker.right = (int)Image.Width() - iBorder;
   rDarker.bottom = (int)Image.Height() - iBorder;
   int iY, iX;
   for (iY = rDarker.top; iY < rDarker.bottom; iY++) {
      pip = Image.Pixel ((DWORD)rDarker.left, (DWORD)iY);

      if (m_pMain->m_fLightBackground)
         for (iX = rDarker.left; iX < rDarker.right; iX++, pip++) {
            pip->wRed = (WORD)( ((DWORD)pip->wRed * MINFUZZYBRIGHTNESSINV) / 0x10000 + MINFUZZYBRIGHTNESS);
            pip->wGreen = (WORD)( ((DWORD)pip->wGreen * MINFUZZYBRIGHTNESSINV) / 0x10000 + MINFUZZYBRIGHTNESS);
            pip->wBlue = (WORD)( ((DWORD)pip->wBlue * MINFUZZYBRIGHTNESSINV) / 0x10000 + MINFUZZYBRIGHTNESS);
         } // iX
      else
         for (iX = rDarker.left; iX < rDarker.right; iX++, pip++) {
            pip->wRed = (WORD)( ((DWORD)pip->wRed * dwScale) / 0x10000);
            pip->wGreen = (WORD)( ((DWORD)pip->wGreen * dwScale) / 0x10000);
            pip->wBlue = (WORD)( ((DWORD)pip->wBlue * dwScale) / 0x10000);
         } // iX
   } // iY

   // fill the the blend scale
   DWORD *padwCur = padwBlendCorner;
   DWORD *padwCurAdd = padwBlendCornerAdd;
   if (m_pMain->m_fLightBackground)
      dwScale = 0;
   for (iY = 0; iY < iBorder; iY++) {
      fp fYSquare = (fp)(iY * iY) / (fp)(iBorder * iBorder);

      for (iX = 0; iX < iBorder; iX++, padwCur++, padwCurAdd++) {
         fp fSquare = sqrt(fYSquare + (fp)(iX * iX) / (fp)(iBorder*iBorder) );
         fSquare = min(fSquare, 1.0);
         *padwCur = (DWORD)(fSquare * (fp)0x10000 + (1.0 - fSquare) * (fp)dwScale);
         *padwCurAdd = 0;

         if (m_pMain->m_fLightBackground) {
            *padwCur = min(*padwCur, 0x10000);
            *padwCurAdd = ((0x10000 - *padwCur) * MINFUZZYBRIGHTNESS) / 0x10000;
            *padwCur = (*padwCur * MINFUZZYBRIGHTNESSINV) / 0x10000 + MINFUZZYBRIGHTNESS;
         }
      } // iX
   }
   for (i = 0; i < (DWORD)iBorder; i++) {
      padwBlend[i] = padwBlendCorner[(DWORD)iBorder - i - 1];  // one edge
      padwBlendAdd[i] = padwBlendCornerAdd[(DWORD)iBorder - i - 1];  // one edge
   }

   // left side
   rDarker.left = 0;
   rDarker.top = iBorder;
   rDarker.right = iBorder;
   rDarker.bottom = (int)Image.Height() - iBorder;
   DWORD dwAdded;
   for (iY = rDarker.top; iY < rDarker.bottom; iY++) {
      pip = Image.Pixel ((DWORD)rDarker.left, (DWORD)iY);
      padwCur = padwBlend;
      padwCurAdd = padwBlendAdd;
      for (iX = rDarker.left; iX < rDarker.right; iX++, pip++, padwCur++, padwCurAdd++) {
         dwScale = *padwCur;
         dwAdded = *padwCurAdd;

         pip->wRed = (WORD)( ((DWORD)pip->wRed * dwScale) / 0x10000 + dwAdded);
         pip->wGreen = (WORD)( ((DWORD)pip->wGreen * dwScale) / 0x10000 + dwAdded);
         pip->wBlue = (WORD)( ((DWORD)pip->wBlue * dwScale) / 0x10000 + dwAdded);
      } // iX
   } // iY

   // right side
   rDarker.left = (int)Image.Width() - iBorder;
   rDarker.top = iBorder;
   rDarker.right = (int)Image.Width();
   rDarker.bottom = (int)Image.Height() - iBorder;
   for (iY = rDarker.top; iY < rDarker.bottom; iY++) {
      pip = Image.Pixel ((DWORD)rDarker.left, (DWORD)iY);
      padwCur = padwBlend + (iBorder - 1);
      padwCurAdd = padwBlendAdd + (iBorder - 1);
      for (iX = rDarker.left; iX < rDarker.right; iX++, pip++, padwCur--, padwCurAdd--) {
         dwScale = *padwCur;
         dwAdded = *padwCurAdd;

         pip->wRed = (WORD)( ((DWORD)pip->wRed * dwScale) / 0x10000 + dwAdded);
         pip->wGreen = (WORD)( ((DWORD)pip->wGreen * dwScale) / 0x10000 + dwAdded);
         pip->wBlue = (WORD)( ((DWORD)pip->wBlue * dwScale) / 0x10000 + dwAdded);
      } // iX
   } // iY

   // top
   rDarker.left = iBorder;
   rDarker.top = 0;
   rDarker.right = (int)Image.Width() - iBorder;
   rDarker.bottom = iBorder;
   padwCur = padwBlend;
   padwCurAdd = padwBlendAdd;
   for (iY = rDarker.top; iY < rDarker.bottom; iY++, padwCur++, padwCurAdd++) {
      dwScale = *padwCur;
      dwAdded = *padwCurAdd;
      pip = Image.Pixel ((DWORD)rDarker.left, (DWORD)iY);
      for (iX = rDarker.left; iX < rDarker.right; iX++, pip++) {
         pip->wRed = (WORD)( ((DWORD)pip->wRed * dwScale) / 0x10000 + dwAdded);
         pip->wGreen = (WORD)( ((DWORD)pip->wGreen * dwScale) / 0x10000 + dwAdded);
         pip->wBlue = (WORD)( ((DWORD)pip->wBlue * dwScale) / 0x10000 + dwAdded);
      } // iX
   } // iY

   // bottom
   rDarker.left = iBorder;
   rDarker.top = (int)Image.Height() - iBorder;
   rDarker.right = (int)Image.Width() - iBorder;
   rDarker.bottom = (int)Image.Height();
   padwCur = padwBlend + (iBorder - 1);
   padwCurAdd = padwBlendAdd + (iBorder - 1);
   for (iY = rDarker.top; iY < rDarker.bottom; iY++, padwCur--, padwCurAdd--) {
      dwScale = *padwCur;
      dwAdded = *padwCurAdd;
      pip = Image.Pixel ((DWORD)rDarker.left, (DWORD)iY);
      for (iX = rDarker.left; iX < rDarker.right; iX++, pip++) {
         pip->wRed = (WORD)( ((DWORD)pip->wRed * dwScale) / 0x10000 + dwAdded);
         pip->wGreen = (WORD)( ((DWORD)pip->wGreen * dwScale) / 0x10000 + dwAdded);
         pip->wBlue = (WORD)( ((DWORD)pip->wBlue * dwScale) / 0x10000 + dwAdded);
      } // iX
   } // iY

   // do the corners
   padwCur = padwBlendCorner;
   padwCurAdd = padwBlendCornerAdd;
   PIMAGEPIXEL apip[4];
   DWORD dwCorner;
   for (iY = 0; iY < iBorder; iY++) {
      apip[0] = Image.Pixel ((DWORD)iBorder-1, (DWORD)iBorder-iY-1);
      apip[1] = Image.Pixel (Image.Width() - (DWORD)iBorder, (DWORD)iBorder-iY-1);
      apip[2] = Image.Pixel (Image.Width() - (DWORD)iBorder, Image.Height() - (DWORD)iBorder + iY);
      apip[3] = Image.Pixel ((DWORD)iBorder-1, Image.Height() - (DWORD)iBorder + iY);
      for (iX = 0; iX < iBorder; iX++, padwCur++, padwCurAdd++, apip[0]--, apip[1]++, apip[2]++, apip[3]--) {
         dwScale = *padwCur;
         dwAdded = *padwCurAdd;
         for (dwCorner = 0; dwCorner < 4; dwCorner++) {
            pip = apip[dwCorner];

            pip->wRed = (WORD)( ((DWORD)pip->wRed * dwScale) / 0x10000 + dwAdded);
            pip->wGreen = (WORD)( ((DWORD)pip->wGreen * dwScale) / 0x10000 + dwAdded);
            pip->wBlue = (WORD)( ((DWORD)pip->wBlue * dwScale) / 0x10000 + dwAdded);
         } // dwCorner
      } // iX
   } // iY

   // blit the image back
   POINT pDest;
   pDest.x = prFuzzy->left;
   pDest.y = prFuzzy->top;
   Image.Paint (hDC, NULL, pDest);

   // done
}

/*************************************************************************************
CVisImage::FuzzyPaint - Paint the fuzzy description and sliders.

inputs
   HDC            hDC - To draw on
   POINT          pOffset - Offset of points
*/
void CVisImage::FuzzyPaint (HDC hDC, POINT pOffset)
{
   // get the fuzzy locations
   RECT rFuzzy, rDescription, rSliders;
   int iSliderLeft, iSliderRight;
   FuzzyLocation (hDC, &rFuzzy, &rDescription, &rSliders, &iSliderLeft, &iSliderRight);
   m_rSliders = rSliders;  // to remember for fast access

   // if nothing to paint then done
   if ((rFuzzy.right <= rFuzzy.left) || (rFuzzy.bottom <= rFuzzy.top))
      return;

   // offset the fuzzy rect and rest
   OffsetRect (&rFuzzy, pOffset.x, pOffset.y);
   OffsetRect (&rDescription, pOffset.x, pOffset.y);
   OffsetRect (&rSliders, pOffset.x, pOffset.y);
   
   // paint a fuzzy bubble
   FuzzyBubblePaint (hDC, &rFuzzy, (int) m_pMain->m_adwFontPixelHeight[1] * 2 / 3);
      // BUGFIX - Was 4/3, an antiborder, but better with 2/3
   // HBRUSH hbrBack = CreateSolidBrush (RGB(0,0,0));
   // FillRect (hDC, &rFuzzy, hbrBack);
   // DeleteObject (hbrBack);

   // draw the text
   if ((rDescription.right > rDescription.left) && (rDescription.bottom > rDescription.top))
      DescriptionPaint (hDC, &rDescription);

   // draw the sliders
   if ((rSliders.right > rSliders.left) && (rSliders.bottom > rSliders.top))
      SlidersPaint (hDC, &rSliders, iSliderLeft, iSliderRight);

   // done
}


/*************************************************************************************
CVisImage::DistantObjectFuzzyPaint - Paint the fuzzy description and sliders.

inputs
   HDC            hDC - To draw on
   POINT          pOffset - Offset of points
   PMAP360INFO    pInfo - Information
*/
void CVisImage::DistantObjectFuzzyPaint (HDC hDC, POINT pOffset, PMAP360INFO pInfo)
{
   // get the fuzzy locations
   RECT rFuzzy, rDescription;
   DistantObjectFuzzyLocation (hDC, pInfo, &rFuzzy, &rDescription);

   // if nothing to paint then done
   if ((rFuzzy.right <= rFuzzy.left) || (rFuzzy.bottom <= rFuzzy.top))
      return;

   // offset the fuzzy rect and rest
   OffsetRect (&rFuzzy, pOffset.x, pOffset.y);
   OffsetRect (&rDescription, pOffset.x, pOffset.y);
   
   // paint a fuzzy bubble
   FuzzyBubblePaint (hDC, &rFuzzy, (int) m_pMain->m_adwFontSizedPixelHeight[1] * 2 / 3);
      // BUGFIX - Was 4/3, an antiborder, but better with 2/3
   // HBRUSH hbrBack = CreateSolidBrush (RGB(0,0,0));
   // FillRect (hDC, &rFuzzy, hbrBack);
   // DeleteObject (hbrBack);

   // draw the text
   if ((rDescription.right > rDescription.left) && (rDescription.bottom > rDescription.top))
      DistantObjectDescriptionPaint (hDC, &rDescription, pInfo);

   // done
}

/*************************************************************************************
CVisImage::DescriptionPaint - Paint the description over the fuzzy area

inputs
   HDC            hDC - To draw on
   RECT           *prDescription - Where to draw the description
returns
   none
*/
void CVisImage::DescriptionPaint (HDC hDC, RECT *prDescription)
{
   // just in case
   m_pMain->FontCreateIfNecessary (hDC);

   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   SetTextColor (hDC, m_pMain->m_fLightBackground ? RGB(0x10, 0x10, 0x00) : RGB(0xff,0xff,0xff));

   // title
   PWSTR psz;
   HFONT hFontOld;
   RECT rSize;
   int iTop = prDescription->top;
   psz = (PWSTR) m_memName.p;
   if (psz && psz[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[1]);
      rSize = *prDescription;
      rSize.top = iTop;
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK);
      SelectObject (hDC, hFontOld);

      iTop = rSize.bottom + (int)m_pMain->m_adwFontPixelHeight[1] / 2;
   }

   // other
   psz = (PWSTR) m_memOther.p;
   if (psz && psz[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[2]);
      rSize = *prDescription;
      rSize.top = iTop;
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK);
      SelectObject (hDC, hFontOld);

      iTop = rSize.bottom + (int)m_pMain->m_adwFontPixelHeight[2] / 2;
   }

   // descriptions
   psz = (PWSTR) m_memDescription.p;
   if (psz && psz[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[0]);
      rSize = *prDescription;
      rSize.top = iTop;
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      DrawTextW (hDC, psz, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK);
      SelectObject (hDC, hFontOld);

      iTop = rSize.bottom;
   }

   SetBkMode (hDC, iOldMode);
}


/*************************************************************************************
CVisImage::DistantObjectDescriptionPaint - Paint the description over the fuzzy area

inputs
   HDC            hDC - To draw on
   RECT           *prDescription - Where to draw the description
   PMAP360INFO    pInfo - Information
returns
   none
*/
void CVisImage::DistantObjectDescriptionPaint (HDC hDC, RECT *prDescription, PMAP360INFO pInfo)
{
   DWORD dwRoomsDistant = pInfo->dwRoomDist;
   if (dwRoomsDistant)
      dwRoomsDistant--; // ignore room 0
   dwRoomsDistant = min(dwRoomsDistant, NUMMAINFONTSIZED-1);
   PWSTR pszText = pInfo->pszDescription;

   // just in case
   m_pMain->FontCreateIfNecessary (hDC);

   int iOldMode = SetBkMode (hDC, TRANSPARENT);
   SetTextColor (hDC, m_pMain->m_fLightBackground ? RGB(0x10, 0x10, 0x00) : RGB(0xff,0xff,0xff));

   // title
   HFONT hFontOld;
   RECT rSize;
   int iTop = prDescription->top;
   if (pszText && pszText[0]) {
      hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFontSized[dwRoomsDistant]);
      rSize = *prDescription;
      rSize.top = iTop;
      DrawTextW (hDC, pszText, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
      DrawTextW (hDC, pszText, -1, &rSize, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK);
      SelectObject (hDC, hFontOld);

      iTop = rSize.bottom + (int)m_pMain->m_adwFontSizedPixelHeight[dwRoomsDistant] / 2;
   }


   SetBkMode (hDC, iOldMode);
}

/*************************************************************************************
CVisImage::SlidersPaint - Paint the emotion sliders

inputs
   HDC            hDC - To draw on
   RECT           *prSliders - Location of the sliders
   int            iSlidersLeft - Size of left side of sliders in pixels
   int            iSlidersRight - Size of right side of sliders in pixels
*/
void CVisImage::SlidersPaint (HDC hDC, RECT *prSliders, int iSlidersLeft, int iSlidersRight)
{
   CListFixed lOBJECTSLIDERS;

   SlidersGetInterp (&lOBJECTSLIDERS);
   if (!lOBJECTSLIDERS.Num())
      return;

   // keep track of the old sliders too
   if (!IsEqualGUID(m_gID, m_gIDWhenSliders))
      m_lOBJECTSLIDERSOld.Clear();
   POBJECTSLIDERS posOld = (POBJECTSLIDERS) m_lOBJECTSLIDERSOld.Get(0);
   DWORD dwNumOld = m_lOBJECTSLIDERSOld.Num();

   HFONT hFontOld = (HFONT) SelectObject (hDC, m_pMain->m_ahFont[0]);
   int iOldMode = SetBkMode (hDC, TRANSPARENT);

#if 0 // old code
   RECT rClientOffset;
   rClientOffset = m_rClient;
   OffsetRect (&rClientOffset, pOffset.x, pOffset.y);

   // figure out if left/right
   BOOL fLeft = FALSE, fRight = FALSE;
   int iMaxHeight = 10; // height of slider
   int iLeft = 0, iRight = 0;
   POBJECTSLIDERS pos = (POBJECTSLIDERS)lOBJECTSLIDERS.Get(0);
   RECT rSize;
   DWORD i, j;
   for (i = 0; i < lOBJECTSLIDERS.Num(); i++, pos++) {
      if (pos->szLeft[0]) {
         fLeft = TRUE;

         // figure out size
         rSize = rClientOffset;
         DrawTextW (hDC, pos->szLeft, -1, &rSize, DT_SINGLELINE | DT_CALCRECT);
         iMaxHeight = max(iMaxHeight, rSize.bottom - rSize.top);
         iLeft = max(iLeft, rSize.right - rSize.left);
      }
      if (pos->szRight[0]) {
         fRight = TRUE;

         // figure out size
         rSize = rClientOffset;
         DrawTextW (hDC, pos->szRight, -1, &rSize, DT_SINGLELINE | DT_CALCRECT);
         iMaxHeight = max(iMaxHeight, rSize.bottom - rSize.top);
         iRight = max(iRight, rSize.right - rSize.left);
      }
   } // i

   // determine location of sliders
   int iBuffer = iMaxHeight;
   int iRowHeight = iMaxHeight + iBuffer;
   RECT rSlider;
   rSlider.right = rClientOffset.right - iBuffer * 2 - iRight;
   rSlider.left = rSlider.right - iBuffer * 5;
   rSlider.top = rClientOffset.bottom - iBuffer - iRowHeight * lOBJECTSLIDERS.Num();
   rSlider.bottom = rSlider.top + iMaxHeight;
   m_rSliders = rSlider;
   m_rSliders.bottom += iRowHeight * lOBJECTSLIDERS.Num();
#endif // 0
   int iMaxHeight = m_pMain->m_adwFontPixelHeight[0]; // height of slider
   int iBuffer = iMaxHeight;
   int iRowHeight = iMaxHeight + iBuffer;
   int iLeft = iSlidersLeft;
   int iRight = iSlidersRight;
   DWORD i, j;
   RECT rSlider, rSize;
   rSlider = *prSliders;
   rSlider.right -= iSlidersRight;
   rSlider.left += iSlidersLeft;
   rSlider.bottom = rSlider.top + iMaxHeight;

   // draw
   DWORD dwTime = GetTickCount();
   DWORD dwTimeToRefresh = 0;
   POBJECTSLIDERS pos = (POBJECTSLIDERS)lOBJECTSLIDERS.Get(0);
   for (i = 0; i < lOBJECTSLIDERS.Num(); i++, pos++, rSlider.top += iRowHeight, rSlider.bottom += iRowHeight) {
      // try to find a match with old
      pos->fValueAtTime = pos->fValue; // just in case don't find a match among the old
      for (j = 0; j < dwNumOld; j++) {
         // if same values, then don't bother copying over
         if (posOld[j].fValueAtTime == pos->fValue)
            continue;
         
         // if different names then not a match
         if (_wcsicmp(pos->szLeft, posOld[j].szLeft) || _wcsicmp(pos->szRight, posOld[j].szRight))
            continue;

         // if there wasn't an old time, then set one now
         if (!posOld[j].dwTime) 
            posOld[j].dwTime = dwTime;

         // if the old time is newer than the current time then there was a wrap-around
         // in time, so ignore
         if (posOld[j].dwTime > dwTime)
            continue;

         // if the old time is too old, then ignore since have had the update
         // displayed for awhile
         if (posOld[j].dwTime + SHOWLIKEDELTATIME < dwTime)
            continue;
         if (dwTimeToRefresh)
            dwTimeToRefresh = min(dwTimeToRefresh, posOld[j].dwTime + SHOWLIKEDELTATIME);
         else
            dwTimeToRefresh = posOld[j].dwTime + SHOWLIKEDELTATIME;

         // else, remember this
         pos->dwTime = posOld[j].dwTime;
         pos->fValueAtTime = posOld[j].fValueAtTime;
         break;
      } // j

      // text
      SetTextColor (hDC, m_pMain->m_fLightBackground ? pos->cColorLight : pos->cColorDark);
      if (pos->szLeft) {
         rSize = rSlider;
         rSize.right = rSlider.left - iBuffer;
         rSize.left = rSize.right - (iLeft + iBuffer);
         DrawTextW (hDC, pos->szLeft, -1, &rSize, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
      }
      if (pos->szLeft) {
         rSize = rSlider;
         rSize.left = rSlider.right + iBuffer;
         rSize.right = rSize.left + (iRight + iBuffer);
         DrawTextW (hDC, pos->szRight, -1, &rSize, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
      }

      // pen outline
      HPEN hPen = CreatePen (PS_SOLID, 0, m_pMain->m_fLightBackground ? pos->cColorLight : pos->cColorDark);
      HPEN hPenOld = (HPEN)SelectObject (hDC, hPen);
      MoveToEx (hDC, rSlider.left, rSlider.top, NULL);
      LineTo (hDC, rSlider.right, rSlider.top);
      LineTo (hDC, rSlider.right, rSlider.bottom);
      LineTo (hDC, rSlider.left, rSlider.bottom);
      LineTo (hDC, rSlider.left, rSlider.top);
      SelectObject (hDC, hPenOld);  // BUGFIX - Was deleting the old pen, and selecting the new one
      DeleteObject (hPen);

      // size of the line
      RECT rSizeLine = rSlider;
      rSizeLine.left += 2;
      rSizeLine.right -= 1;
      rSizeLine.top += 2;
      rSizeLine.bottom -= 1;
      fp fWidth = (fp)(rSizeLine.right - rSizeLine.left);

      // fill
      fp fValue = pos->dwTime ? min(pos->fValueAtTime, pos->fValue) : pos->fValue;
      rSize = rSizeLine;
      rSize.right = max(1, (int)(fWidth * (fValue + 1.0) / 2.0)) + rSize.left;
         // BUGFIX - so at least 1 pixel
      HBRUSH hBrush = CreateSolidBrush (m_pMain->m_fLightBackground ? pos->cColorLight : pos->cColorDark);
      FillRect (hDC, &rSize, hBrush);
      DeleteObject (hBrush);

      // potentially fill green (good) or red (negative)
      if (pos->dwTime) {
         rSize = rSizeLine;
         rSize.right = max(1, (int)((fp)(rSize.right - rSize.left) * (fValue + 1.0) / 2.0)) + rSize.left;
         if (pos->fValue > pos->fValueAtTime) {
            // green, good
            rSize.left = (int)(fWidth * (pos->fValueAtTime + 1.0) / 2.0) + rSizeLine.left;
            rSize.right = max(1, (int)(fWidth * (pos->fValue + 1.0) / 2.0)) + rSizeLine.left;
            hBrush = CreateSolidBrush (m_pMain->m_fLightBackground ? RGB(0x00, 0x80, 0x00) : RGB(0, 0xff, 0));
         }
         else {
            // red, negative
            rSize.left = (int)(fWidth * (pos->fValue + 1.0) / 2.0) + rSizeLine.left;
            rSize.right = max(1, (int)(fWidth * (pos->fValueAtTime + 1.0) / 2.0)) + rSizeLine.left;
            hBrush = CreateSolidBrush (m_pMain->m_fLightBackground ? RGB(0x80, 0x00, 0x00) : RGB(0xff, 0, 0));
         }

         // paint
         FillRect (hDC, &rSize, hBrush);
         DeleteObject (hBrush);
      }

      // draw ticks
#define NUMSLIDERTICKS     5

      DWORD j;
      hPen = CreatePen (PS_SOLID, 0, gpMainWindow->m_fLightBackground ? RGB(0x40, 0x40, 0x40) : RGB(0x80,0x80,0x80));
      hPenOld = (HPEN)SelectObject (hDC, hPen);
      for (j = 1; j+1 < NUMSLIDERTICKS; j++) {  // intentionally not doing all
         int iHorz = ((rSizeLine.right - rSizeLine.left) * (int)j + NUMSLIDERTICKS/2) / (int)(NUMSLIDERTICKS-1) + rSizeLine.left;
         MoveToEx (hDC, iHorz, rSizeLine.top+2, NULL);
         LineTo (hDC, iHorz, rSizeLine.bottom-2);
      }
      SelectObject (hDC, hPenOld);
      DeleteObject (hPen);
      
      // potentially fill
   } // over lOBJECTSLIDERS


   // set timer to refrsh area
   if (dwTimeToRefresh)
      m_pMain->VisImageTimer (this, dwTimeToRefresh + 500); // add short delay so don't lose anything

   SelectObject (hDC, hFontOld);
   SetBkMode (hDC, iOldMode);

   // store the sliders into the old ones
   m_lOBJECTSLIDERSOld.Init (sizeof(OBJECTSLIDERS), lOBJECTSLIDERS.Get(0), lOBJECTSLIDERS.Num());
   m_gIDWhenSliders = m_gID;
}


/*************************************************************************************
CVisImage::InvalidateSliders - Invvalidates the area  where the sliders are so they're
redrawn.
*/
void CVisImage::InvalidateSliders (void)
{
   // if no old then dont bother
   if (!m_lOBJECTSLIDERSOld.Num())
      return;

   // if it's a text window then dont bother
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   if (pvi && pvi->pTextWindow)
      return;

   if (m_rSliders.bottom <= m_rSliders.top)
      return;   // no slider

   // else, invalidate
   InvalidateRect (m_hWnd, &m_rSliders, FALSE);
}

/*************************************************************************************
CVisImage::Paint - This paints an image. It is being called by
VisImagePaintAll() so 

inputs
   HWND           hWnd - Window drawing too
   HDC            hDC - DC drawing to
   RECT           *prClip - Current clip rectangle
   BOOL           fIncludeSliders - If TRUE then paint the sliders and description too
   BOOL           fDontPaintIfNothing - If would be drawing "drawing..." then don't paint
   BOOL           fTextUnderImages - If TRUE, then drawing text under images
   BOOL           *pfWantedToShowDraingText - This will be set to TRUE paint wanted to show
                  the "drawing..." text. It may or may not have. Can be NULL.
   BOOL           *pfPaintedImage - Set to TRUE if actually painted an image.
returns
   none
*/
void CVisImage::Paint (HWND hWnd, HDC hDC, RECT *prClip, BOOL fIncludeSliders,
                       BOOL fDontPaintIfNothing, BOOL fTextUnderImages,
                       BOOL *pfWantedToShowDraingText, BOOL *pfPaintedImage)
{
   BOOL fShowDrawingText = FALSE;   // set to TRUE if show "drawing..."

   if (pfWantedToShowDraingText)
      *pfWantedToShowDraingText = FALSE;
   if (pfPaintedImage)
      *pfPaintedImage = FALSE;

   // make sure the right window, and painting the right bits
   if (hWnd != m_hWnd)
      return;

   // clear out just in case
   EliminateDeadLayers ();

   // see if intersect
   RECT rInter;
   if (!IntersectRect (&rInter, prClip, &m_rClient))
      return;

   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);

   // if it's a text window then dont bother
   if (pvi->pTextWindow) {
      if (pfPaintedImage)
         *pfPaintedImage = TRUE;
      return;
   }

   // calculate the layers
   BOOL fRet = LayeredCalc (hDC);

#ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
   BOOL fTransparent = !m_pMain->ChildHasTitle (m_hWnd);
#else
   BOOL fTransparent = FALSE;
#endif

   // BUGFIX - Create a bitmap for this image and paint onto that.
   // Need to do because of transparency
   RECT rClientMod;
   rClientMod.left = rClientMod.top = 0;
   rClientMod.right = m_rClient.right - m_rClient.left;
   rClientMod.bottom = m_rClient.bottom - m_rClient.top;
   HDC hDCBmp = CreateCompatibleDC (hDC);
   HBITMAP hBmp = CreateCompatibleBitmap (hDC, m_rClient.right - m_rClient.left, m_rClient.bottom - m_rClient.top);
   SelectObject (hDCBmp, hBmp);

   // draw backgrouns
   POINT pDCOffset;
   pDCOffset.x = m_rClient.left;
   pDCOffset.y = m_rClient.top;
   m_pMain->BackgroundStretch ((fTextUnderImages && !fIncludeSliders) ? BACKGROUND_TEXT : BACKGROUND_IMAGES, fTransparent,
      fIncludeSliders ? WANTDARK_NORMAL : (fTextUnderImages ? WANTDARK_DARK : WANTDARK_NORMAL),
      &m_rClient, hWnd, GetParent(hWnd), 1, hDCBmp, &pDCOffset);

   // if there isn't any bitmap then draw text "drawing..."
   if (!fRet) { // was checking m_hBmp, now dead var
drawnothing:
      // find out which window this is in
      HWND hWndCur;
      for (hWndCur = hWnd; hWndCur; hWndCur = GetParent (hWndCur))
         if ((hWndCur == m_pMain->m_hWndPrimary) || (hWndCur == m_pMain->m_hWndSecond))
            break;
#if 0    // BUGFIX - Dont draw semi-transparent background
      if (hWndCur && m_pMain && m_pMain->m_hbmpJPEGBackDark) {
         // need to stretch this
         RECT rMainClient, rMainScreen;
         GetClientRect (hWndCur, &rMainClient);
         rMainScreen = rMainClient;
         ClientToScreen (hWndCur, ((POINT*)&rMainScreen) + 0);
         ClientToScreen (hWndCur, ((POINT*)&rMainScreen) + 1);

         RECT rThisScreen;
         rThisScreen = m_rClient;
         ClientToScreen (hWnd, ((POINT*)&rThisScreen) + 0);
         ClientToScreen (hWnd, ((POINT*)&rThisScreen) + 1);

         // figure out the pixels copying from in the main window
         RECT rChildInMain;
         rChildInMain.left = rThisScreen.left - rMainScreen.left;
         rChildInMain.top = rThisScreen.top - rMainScreen.top;
         rChildInMain.right = rThisScreen.right - rMainScreen.left;
         rChildInMain.bottom = rThisScreen.bottom - rMainScreen.top;

         // how many pixels in original image
         RECT rLocInOrig;
         rLocInOrig.left = m_pMain->m_pJPEGBack.x * rChildInMain.left / (rMainClient.right - rMainClient.left);
         rLocInOrig.top = m_pMain->m_pJPEGBack.y * rChildInMain.top / (rMainClient.bottom - rMainClient.top);
         rLocInOrig.right = m_pMain->m_pJPEGBack.x * rChildInMain.right / (rMainClient.right - rMainClient.left);
         rLocInOrig.bottom = m_pMain->m_pJPEGBack.y * rChildInMain.bottom / (rMainClient.bottom - rMainClient.top);

         rLocInOrig.left = max(rLocInOrig.left, 0);
         rLocInOrig.top = max(rLocInOrig.top, 0);
         rLocInOrig.right = min(rLocInOrig.right, m_pMain->m_pJPEGBack.x-1);
         rLocInOrig.bottom = min(rLocInOrig.bottom, m_pMain->m_pJPEGBack.y-1);

         // draw stretched bitmap
         HDC hDCBmp = CreateCompatibleDC (hDC);
         SelectObject (hDCBmp, m_pMain->m_hbmpJPEGBackDark);
         int   iOldMode;
         iOldMode = SetStretchBltMode (hDC, COLORONCOLOR);
         StretchBlt(
            hDC, m_rClient.left, m_rClient.top, m_rClient.right - m_rClient.left, m_rClient.bottom - m_rClient.top,
            hDCBmp, rLocInOrig.left, rLocInOrig.top, rLocInOrig.right - rLocInOrig.left, rLocInOrig.bottom - rLocInOrig.top,
            SRCCOPY);
         SetStretchBltMode (hDC, iOldMode);
         DeleteDC (hDCBmp);
      }
      else
#endif // 0


      HFONT hFontOld = (HFONT) SelectObject (hDCBmp, m_pMain->m_hFontBig);
      int iOldMode = SetBkMode (hDCBmp, TRANSPARENT);
      SetTextColor (hDCBmp, m_pMain->m_fLightBackground ? RGB(0x40, 0x40, 0x40) : RGB(0x80,0x80,0x80));
      DrawText (hDCBmp, "Drawing...", -1, &rClientMod, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
      SelectObject (hDCBmp, hFontOld);
      SetBkMode (hDCBmp, iOldMode);
      
      fShowDrawingText = TRUE;

      if (pfWantedToShowDraingText)
         *pfWantedToShowDraingText = TRUE;

      goto drawspeaking;
   }

   // find the first layer with an image
   PVILAYER pviFirst = NULL;
   DWORD i;
   for (i = 0; i < m_lVILAYER.Num(); i++)
      if (pvi[i].pImage) {
         pviFirst = pvi+i;
         break;
      }
   
   // BUGFIX - If nothing then skip
   if (!pviFirst)
      goto drawnothing;

   RECT rTo, rFrom;
   rTo = pviFirst->rStretchTo;
   rFrom = pviFirst->rStretchFrom;
   OffsetRect (&rTo, -m_rClient.left, -m_rClient.top);

#if 0 // not used anymore because draw entire blackness above
   // draw black rectangle around image
   r = m_rClient;
   r.right = rTo.left;
   if ((r.right != r.left) && !fTransparent)
      FillRect (hDC, &r, m_pMain->m_hbrBackground);
   r = m_rClient;
   r.left = rTo.right;
   if ((r.left != r.right) && !fTransparent)
      FillRect (hDC, &r, m_pMain->m_hbrBackground);
   r = m_rClient;
   r.bottom = rTo.top;
   if ((r.bottom != r.top) && !fTransparent)
      FillRect (hDC, &r, m_pMain->m_hbrBackground);
   r = m_rClient;
   r.top = rTo.bottom;
   if ((r.top != r.bottom) && !fTransparent)
      FillRect (hDC, &r, m_pMain->m_hbrBackground);

   // Not used because paint sliders below, no worry about flicker
   // paint the sliders to minimize flicker
   if (fIncludeSliders)
      PaintSliders (hDC);
#endif // 0

   if (m_hBmpLayered) {
      // if have a layered image then blit that onto the surface
      HDC hDCBit = CreateCompatibleDC (hDCBmp);
      SelectObject (hDCBit, m_hBmpLayered);

      // BUGFIX - Transparentblt
      TransparentBlt (
         hDCBmp, 
         rTo.left, rTo.top, m_pisLayered->Width(), m_pisLayered->Height(),
         hDCBit,
         0, 0, m_pisLayered->Width(), m_pisLayered->Height(),
         LAYEREDTRANSPARENTCOLOR);
      //BitBlt (hDCBmp, 
      //   rTo.left, rTo.top, m_pisLayered->Width(), m_pisLayered->Height(),
      //   hDCBit,
      //   0, 0, // BUGFIX - Was using rFrom
      //   SRCCOPY);
      DeleteDC (hDCBit);
   }
   else {
      // get directly from the image
      // have image do the drawing
      COLORREF cFadeColor;
#ifdef MIFTRANSPARENTWINDOWS
      cFadeColor = LAYEREDTRANSPARENTCOLOR;
#else
      cFadeColor = m_pMain->m_cBackground;
#endif
      pviFirst->pImage->CachePaint (TRUE, hDCBmp, m_fIconic ? 1 : 0,
         rTo.left, rTo.top, rTo.right - rTo.left, rTo.bottom - rTo.top,
         &pviFirst->rStretchFrom, m_pMain->m_f360Long, cFadeColor);
   }


#if 0 // DEADCODE
   // create the DC for the bitmap
   HDC hDCBit = CreateCompatibleDC (hDC);
   SelectObject (hDCBit, m_hBmp);

   if ((rFrom.right - rFrom.left != rTo.right - rTo.left) &&
      (rFrom.bottom - rFrom.top != rTo.bottom - rTo.top)) {
         SetStretchBltMode (hDC, COLORONCOLOR);
         StretchBlt (hDC,
            rTo.left, rTo.top, rTo.right - rTo.left, rTo.bottom - rTo.top,
            hDCBit,
            rFrom.left, rFrom.top, rFrom.right - rFrom.left, rFrom.bottom - rFrom.top,
            SRCCOPY);
      }
   else
      BitBlt (hDC, 
         rTo.left, rTo.top, rTo.right - rTo.left, rTo.bottom - rTo.top,
         hDCBit,
         rFrom.left, rFrom.top,
         SRCCOPY);

   pvi->rStretchFrom = rFrom;
   pvi->rStretchTo = rTo;

   DeleteDC (hDCBit);
#endif // 0

drawspeaking:
   // paint the sliders
   if (fIncludeSliders) {
      pDCOffset.x = -m_rClient.left;
      pDCOffset.y = -m_rClient.top;

      // room info
      PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
      if (Is360(pvi) && m_pMain && m_pMain->m_pMapWindow) {
         CListFixed lMAP360INFO;
         CListVariable lMapStrings;
         m_pMain->m_pMapWindow->Generate360Info (&m_gID, &lMAP360INFO, &lMapStrings);

         PMAP360INFO pInfo = (PMAP360INFO)lMAP360INFO.Get(0);
         for (i = 0; i < lMAP360INFO.Num(); i++, pInfo++)
            if (pInfo->pszDescription)
               DistantObjectFuzzyPaint (hDCBmp, pDCOffset, pInfo);
      }

      // paint the object description and text
      FuzzyPaint (hDCBmp, pDCOffset);
   }

   // BUGFIX - May not want to paint if "drawing..."
   if (fShowDrawingText && fDontPaintIfNothing) {
      if (pfPaintedImage)
         *pfPaintedImage = FALSE;
   }
   else {
      BitBlt (hDC, 
         m_rClient.left, m_rClient.top, m_rClient.right - m_rClient.left, m_rClient.bottom - m_rClient.top,
         hDCBmp,
         0, 0, // BUGFIX - Was using rFrom
         SRCCOPY);
      if (pfPaintedImage)
         *pfPaintedImage = !fShowDrawingText;
   }
   DeleteDC (hDCBmp);
   DeleteObject (hBmp);

   // BUGFIX - moved down so actually draws on top of image
   if (!fTextUnderImages && m_fSpeaking && m_fIconic) {
      HICON hIconSpeaker = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_SPEAKERICON));
      DrawIcon (hDC, m_rClient.right - 32, m_rClient.bottom-32, hIconSpeaker);
   }

}

static PWSTR gpszHotSpot = L"HotSpot";
static PWSTR gpszLangID = L"langid";

/*************************************************************************************
CVisImage::HotSpotsSet - This takes a PCMMLNode2 and reads the hot spots from it.
The are elemnets named "hotspot"

inputs
   PCMMLNode2         pNode
   DWORD             dwWidth - If dwWidth and dwHeight are NOT 0 then this assumes
                        that the hot spot coords are normalized to 1000x1000 (as written
                        in the MML). They're then translated to dwWidth and dwHeight
   DWORD             dwHeight
returns
   none
*/
void CVisImage::HotSpotsSet (PCMMLNode2 pNode, DWORD dwWidth, DWORD dwHeight)
{
   HotSpotsClear();  // just in case

   // fill in the hot spots
   DWORD i;
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
      if (!_wcsicmp(psz, gpszHotSpot)) {
         // new hot spot
         PCCircumrealityHotSpot pNew = new CCircumrealityHotSpot;
         pNew->MMLFrom (pSub, DEFLANGID);
               // NOTE - Default language is english, which is ok assumption

         if (dwWidth && dwHeight)
            RenderSceneHotSpotToImage (&pNew->m_rPosn, dwWidth, dwHeight);

         // add it
         m_plPCCircumrealityHotSpot->Add (&pNew);
         continue;
      }
   } // i
}



/*************************************************************************
VisImagePage
*/
BOOL VisImagePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVisImage pvi = (PCVisImage)pPage->m_pUserData;   // node to modify

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      // remember this
      pvi->m_pPage = pPage;
      break;

   case ESCM_DESTRUCTOR:
      // no page
      pvi->m_pPage = NULL;
      break;

   case ESCM_MOUSEMOVE:
      if (pvi->m_fIconic) {
         // if it's iconic then trap cursor
         pPage->SetCursor (pvi->m_pMain->m_hCursorZoom); // BUGFIX - Was IDC_HANDCURSOR);
            // BUGFIX - Was m_hCursorMenu
         // pvi->m_pMain->VerbTooltipUpdate ();
         return TRUE;
      }
      break;

   case ESCM_LBUTTONDOWN:
      // set this as the foreground window
      SetWindowPos (GetParent(pPage->m_pWindow->m_hWnd), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

      if (pvi->m_fIconic) {
         // if it's iconic then trap clicks
         // dont call because causes crash, and works without pPage->MouseCaptureRelease(NULL);

         // if have verb active then handle that
         if (pvi->m_pMain->VerbClickOnObject (&pvi->m_gID))
            return TRUE;

         // send message up indicating that clicked on iconic window
         pvi->m_pMain->ContextMenuDisplay (pPage->m_pWindow->m_hWnd, pvi);

         return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;

         // BUGFIX - If this begins with mailto: or http:// or https:// then
         // use default handler
         PWSTR pszMailTo = L"mailto:", pszHTTP = L"http://", pszHTTPS = L"https://";
         DWORD dwMailToLen = (DWORD)wcslen(pszMailTo), dwHTTPLen = (DWORD)wcslen(pszHTTP),
            dwHTTPSLen = (DWORD)wcslen(pszHTTPS);
         if (!_wcsnicmp(p->psz, pszMailTo, dwMailToLen) ||
            !_wcsnicmp(p->psz, pszHTTP, dwHTTPLen) ||
            !_wcsnicmp(p->psz, pszHTTPS, dwHTTPSLen))
               break;

         pvi->Link (p->psz, pPage, &pvi->m_gID);
      }
      return TRUE;

   }

   return FALSE;
}


/*************************************************************************************
CVisImage::Link - Called when a link is pressed in the page

inputs
   PWSTR       pszLink - Link
   PCEscPage   pPage - Page, so can get info about other controls
   GUID        *pgObject - Object to associate the link with, and translate "<object>" strings
returns
   BOOL - TRUE if success
*/
BOOL CVisImage::Link (PWSTR pszLink, PCEscPage pPage, GUID *pgObject)
{
   if (m_pMain->m_fMessageDiabled) {
      BeepWindowBeep (ESCBEEP_DONTCLICK);
      return FALSE;
   }

   CMem memLink;
   MemZero (&memLink);
   MemCat (&memLink, pszLink);

   // translate all <control> entries in link
   DWORD dwStart, dwEnd;
   CMem memReplace;
   BOOL fShouldHide = FALSE;
   while (LinkFindBracket ((PWSTR)memLink.p, TRUE, &dwStart, &dwEnd)) {
      // fill end with NULL so that can search for it
      PWSTR psz = (PWSTR)memLink.p;
      psz[dwEnd] = 0;

      // find it
      LinkControlValue (psz + (dwStart+1), pPage, &memReplace, &fShouldHide);

      // replace with whatever found
      psz[dwEnd] = L'>';
      LinkReplace (&memLink, (PWSTR)memReplace.p, dwStart, dwEnd);
   } // while find bracket

   // send out command
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   m_pMain->SendTextCommand (pvi ? pvi->lid : DEFLANGID, (PWSTR)memLink.p, NULL, pgObject, NULL,
      !fShouldHide, !fShouldHide, TRUE);
   m_pMain->HotSpotDisable ();
   BeepWindowBeep (ESCBEEP_LINKCLICK);

   return TRUE;
}

/*************************************************************************************
ResHelpPrefix - This makes the background black and changes the font to white.
Used for help.

inputs
   BOOL           fLightBackground - Set to TRUE if it's a light background
   PCMMLNode      pNode - Node being used
returns
   PCMMLNode - New node, or NULL if error. (In which case pNode is freed)
*/
PCMMLNode ResHelpPrefix (BOOL fLightBackground, PCMMLNode pNode)
{
   CEscError err;
   CMem mem;
   MemZero (&mem);
   MemCat (&mem, ResHelpStringPrefix(fLightBackground));
   MemCat (&mem, ResHelpStringSuffix());
   PCMMLNode pRet = ParseMML ((PWSTR)mem.p, ghInstance, NULL, NULL, &err, FALSE);
   if (!pRet) {
      delete pNode;
      return NULL;
   }

   // small and stick inside there
   PCMMLNode pFind = NULL;
   PWSTR psz;
   pRet->ContentEnum(pRet->ContentFind (L"font", NULL, NULL), &psz, &pFind);
   if (!pFind) {
      delete pNode;
      return NULL;
   }

   PCMMLNode pSmall = NULL;
   pFind->ContentEnum(pFind->ContentFind(L"small", NULL, NULL), &psz, &pSmall);
   if (!pSmall) {
      delete pNode;
      return NULL;
   }

   pSmall->ContentAdd (pNode);
   pNode->NameSet (L"null");
   return pRet;
}




/*************************************************************************************
CVisImage::ReRenderAll - Causes the image to (a) pass a re-render request to the
rendering thread, and (b) release the current image for rendering
*/
void CVisImage::ReRenderAll (void)
{
   // loop at the top layer
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD dwLayer;
   BOOL fChanged = FALSE;
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) {
      if (!pvi->pImage || pvi->pTextWindow)
         continue;

      PCMMLNode2 pNode = CircumrealityParseMML ((PWSTR)pvi->pMem->p);
      if (!pNode)
         continue;
      m_pMain->m_pRT->RequestRender (pNode, RTQUEUE_HIGH, -1, FALSE);
      delete pNode;

      // release the image, forcing it to stop
      ImageCacheRelease (pvi->pImage, TRUE);
      pvi->fWaiting = TRUE;
      pvi->pImage = NULL;

      // cause to redraw
      InvalidateRect (m_hWnd, &m_rClient, FALSE);

   } // dwLayer
}


/*************************************************************************************
CVisImage::UpgradeQuality - If the image has a pvi->pImage equal to the low-quality
image then it's marked as invalid so that the next call, for SeeIfRendered(),
will upgrade quality

inputs
   PCImageStore      pisLowQuality - Low quality image
*/
void CVisImage::UpgradeQuality (PCImageStore pisLowQuality)
{
   // loop at the top layer
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD dwLayer;
   BOOL fChanged = FALSE;
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) {
      if (!pvi->pImage || pvi->pTextWindow)
         continue;

      if (pvi->pImage != pisLowQuality)
         continue;   // no match

      ImageCacheRelease (pvi->pImage, TRUE);
      pvi->fWaiting = TRUE;
      pvi->pImage = NULL;

      // cause to redraw
      InvalidateRect (m_hWnd, &m_rClient, FALSE);

   } // dwLayer
}


/*************************************************************************************
CVisImage::WindowBackground - So updates window background for MML

inputs
   BOOL           fForceRefresh - If TRUE, forcefull invalidate
*/
void CVisImage::WindowBackground (BOOL fForceRefresh)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   PCEscWindow pWindow = pvi->pTextWindow;

   // if not main window, do nothing
   if (!pWindow || !m_hWnd || m_fIconic)
      return;

   // if no bitmap then do nothing
   if (!m_pMain->m_ahbmpJPEGBackDark[BACKGROUND_TEXT]) {
      pWindow->m_hbmpBackground = NULL;

      if (fForceRefresh && pWindow->m_pPage)
         pWindow->m_pPage->Invalidate ();

      return;
   }

   // get transparency
#ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
   BOOL fTransparent = !m_pMain->ChildHasTitle (m_hWnd);
#else
   BOOL fTransparent = FALSE;
#endif

   if (fTransparent) {
      RECT rWindow;
      if (pWindow->m_hWnd)
         GetWindowRect (pWindow->m_hWnd, &rWindow);
      else {
         rWindow = m_rClient;
         ClientToScreen (m_hWnd, ((LPPOINT)&rWindow) + 0);
         ClientToScreen (m_hWnd, ((LPPOINT)&rWindow) + 1);
      }

      pWindow->m_hbmpBackground = m_pMain->BackgroundStretchCalc (BACKGROUND_TEXT, &rWindow, WANTDARK_DARKEXTRA, GetParent(m_hWnd), 1,
         &pWindow->m_rBackgroundTo, &pWindow->m_rBackgroundFrom);
         // BUGFIX - Was 1 for dark, but want extra dark
   }
   else
      pWindow->m_hbmpBackground = NULL;

   if (fForceRefresh && pWindow->m_pPage)
      pWindow->m_pPage->Invalidate ();

}

/*************************************************************************************
CVisImage::SeeIfRendered - This looks through all the images that
haven't yet been rendered and sees if they are rendered. If so, their
image is invalidated so it'll be redrawn.

inputs
   none
*/
static PWSTR gpszMML = L"MML";

void CVisImage::SeeIfRendered (void)
{
   // loop at the top layer
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD dwLayer;
   BOOL fChanged = FALSE;
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) {
      // make sure that the image hasn't been killed by an update
      if (pvi->pImage && ImageCacheIsDead (pvi->pImage))
         pvi->fWaiting = TRUE;   // new image has appeared

      if (!pvi->fWaiting || pvi->pTextWindow)
         continue;   // already rendered

      // see if it's a text window
      PWSTR psz = pvi->pNode ? pvi->pNode->NameGet() : NULL;
      if (psz && (!_wcsicmp(psz, CircumrealityText()) || !_wcsicmp(psz, CircumrealityHelp())) ) {
         // find the MML section
         PCMMLNode2 pSub = NULL;
         PWSTR psz;
         pvi->pNode->ContentEnum (pvi->pNode->ContentFind (gpszMML), &psz, &pSub);
         if (!pSub)
            continue;  // error

         PCMMLNode pSub1 = pSub->CloneAsCMMLNode();

         // prepend bits so help is displayed on a dark background
         if (pSub1 && !_wcsicmp(pvi->pNode->NameGet(), CircumrealityHelp()))
            pSub1 = ResHelpPrefix (m_pMain->m_fLightBackground, pSub1);

         if (!pSub1)
            continue;

         pvi->lid = (LANGID)MMLValueGetInt (pvi->pNode, L"LangID", (int)pvi->lid);

         // it's a text window
         pvi->pTextWindow = new CEscWindow;
         if (!pvi->pTextWindow) {
            delete pSub1;
            continue;  // error
         }

         if (!pvi->pTextWindow->Init (ghInstance, m_hWnd,
            EWS_NOTITLE | EWS_FIXEDSIZE | EWS_VSCROLL | EWS_FIXEDHEIGHT | EWS_FIXEDWIDTH | EWS_CHILDWINDOW | EWS_CHILDWINDOWHASNOBORDER,
            &m_rClient)) {
               delete pvi->pTextWindow;
               delete pSub1;
               pvi->pTextWindow = NULL;
               continue;
            }

         EscWindowFontScaleByScreenSize (pvi->pTextWindow);

         WindowBackground (FALSE);

         // start the page
         pSub1->NameSet (L"<Main>"); // BUGFIX - So dont get tag unknown

         // BUGBUG - Getting some memory leaks with memory containing "<Main>"
         // in the string, but they're not from this one. So, I don't know
         // where they're coming from but they dont seem too major. They seem
         // to occur just a few times per running

         // change font size
         WORD wOld = EscFontScaleGet ();
         // BUGFIX - Scale MML by player's font-size settings
         fp fScale = 256.0 * pow (sqrt(2.0), (fp)m_pMain->m_iTransSize);
         if (m_fIconic)
            fScale /= 2.0;
         EscFontScaleSet ((WORD)fScale);

         pvi->pTextWindow->PageDisplay (ghInstance, pSub1, VisImagePage, this);
         EscFontScaleSet (wOld);
         // dont bother because clone pSub1->NameSet (gpszMML); // BUGFIX - Restore back to previous
         delete pSub1;

         pvi->fWaiting = FALSE;
         LayeredClear();

         // BUGFIX - Added this
         fChanged = TRUE;
         continue;
      }

      // see if done yet
      if (pvi->pImage) {
         ImageCacheRelease (pvi->pImage, FALSE);
         pvi->pImage = NULL;
      }

      DWORD dwQuality;
      pvi->pImage = NULL;
      for (dwQuality = NUMIMAGECACHE-1; !pvi->pImage && (dwQuality < NUMIMAGECACHE); dwQuality--) {
         pvi->pImage = ImageCacheOpen ((PWSTR)pvi->pMem->p, dwQuality, m_pMain);

#if 0 // def _DEBUG  // to test
         if (pvi->pImage && dwQuality)
            dwQuality = 1;
#endif
      }
      if (!pvi->pImage) {
         // BUGFIX - Bump up the priority
         // BUGFIX - Only create once every 10 tries, so not always slowing down
         m_pMain->m_pRT->BumpRenderPriority ((PWSTR)pvi->pMem->p, !(rand() % 10));

         continue;   // not yet done
      }

      // else, have pvi->pImage, so see about setting this as a background image
      m_pMain->BackgroundUpdate (BACKGROUND_IMAGES, pvi->pImage, FALSE);

      // find out where it will be drawn
      if (!m_hWnd)
         continue;   // shouldnt happen

      LayeredClear();

      // else done
      //HDC hDC = GetDC (m_hWnd);
      if (Is360(pvi)) {
         // 360 degree image
         pvi->dwWidth = (DWORD)(m_rClient.right - m_rClient.left);
         pvi->dwHeight = (DWORD)(m_rClient.bottom - m_rClient.top);
         pvi->dwStretch = 0;  // no stretching for this

         // get the rectangle for size
         pvi->pImage->Surround360Set (m_fIconic ? 1 : 0, m_pMain->m_f360FOV, m_pMain->m_f360Lat, pvi->dwWidth, pvi->dwHeight,
            m_pMain->m_fCurvature);
         //m_hBmp = pvi->pImage->Surround360ToBitmap (m_fIconic ? 1 : 0, hDC, m_pMain->m_f360Long);
      }
      else {
         // ordinary bitmap
         //m_hBmp = pvi->pImage->ToBitmap (hDC);

         pvi->dwWidth = pvi->pImage->Width();
         pvi->dwHeight = pvi->pImage->Height();
         pvi->dwStretch = pvi->pImage->m_dwStretch;
      }

      // loop through and remember if was stretched
      DWORD i;
      DWORD *padwStretch = (DWORD*)m_lOldLayersStretch.Get(0);
      PWSTR pszMatch = (pvi->pMem && pvi->pMem->p) ? (PWSTR)pvi->pMem->p : NULL;
      if (pszMatch && Is360(pvi) )   // only really care if 360 image
         for (i = 0; i < m_lOldLayers.Num(); i++, padwStretch++)
            if (!_wcsicmp((PWSTR)m_lOldLayers.Get(i), pszMatch))
               padwStretch[0] = 4;

      //ReleaseDC (m_hWnd, hDC);
      //if (!m_hBmp)
      //   return;   // shouldnt happen

      fChanged = TRUE;


      pvi->fWaiting = FALSE;

      // only procede if this is image #0, since that is for the hotspots
      if (dwLayer)
         continue;

      // hot spots
      PCMMLNode2 pHot = pvi->pImage->MMLGet();
      if (pHot) {
         // BUGFIX - Check m_plPCCircumrealityHotSpot to make sure don't clea
         if (!m_plPCCircumrealityHotSpot) {
            m_plPCCircumrealityHotSpot = new CListFixed;
            if (!m_plPCCircumrealityHotSpot)
               return;  // shouldn happen
            m_plPCCircumrealityHotSpot->Init (sizeof(PCCircumrealityHotSpot));
         }
         else
            HotSpotsClear ();

         // if this is a 3d scene, then the hot spots will be normalized, so
         // un-normalize
         PWSTR psz = pHot->NameGet();
         DWORD dwWidth = 0, dwHeight = 0;
         if (psz && (!_wcsicmp(psz, Circumreality3DScene()) || !_wcsicmp(psz, Circumreality3DObjects()) ||
            !_wcsicmp(psz, CircumrealityTitle()) )) {
            dwWidth = pvi->dwWidth;
            dwHeight = pvi->dwHeight;

            // BUGFIX - If 360 image then fix width and height stretch
            if (Is360(pvi)) {
               dwWidth = pvi->pImage->Width();
               dwHeight = pvi->pImage->Height();
            }
         }

         // NOTE: Could move this hotspots set call to an init call?
         // or at least for titles image
         HotSpotsSet (pHot, dwWidth, dwHeight);
      }
   } // dwLayer


   // invalidate rect
   if (fChanged)
      InvalidateRect (m_hWnd, &m_rClient, FALSE);
}


#if 0
/*************************************************************************************
CVisImage::Is360 - Returns TRUE if the main layer is 360-degrees
*/
BOOL CVisImage::Is360 (void)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   return ::Is360(pvi);
}
#endif

/*************************************************************************************
CVisImage::Vis360Changed - Call this whenever a 360 degree settings (such as FOV,
etc.) changes. This will rebuild the display and invalidate the rect
*/
void CVisImage::Vis360Changed (void)
{
   RECT r;
   HWND hWndVis;

   r = m_rClient;
   hWndVis = m_hWnd;

   LayeredClear();

   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD dwLayer;
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) {
      if (!Is360(pvi))
         continue;   // dont care

      pvi->dwWidth = (DWORD)(r.right - r.left);
      pvi->dwHeight = (DWORD)(r.bottom - r.top);

      // get the rectangle for size
      pvi->pImage->Surround360Set (m_fIconic ? 1 : 0, m_pMain->m_f360FOV, m_pMain->m_f360Lat, pvi->dwWidth, pvi->dwHeight,
         m_pMain->m_fCurvature);

      //if (m_hBmp)
      //   DeleteObject (m_hBmp);
      //HDC hDC = GetDC (hWndVis);
      //m_hBmp = m_pImage->Surround360ToBitmap (m_fIconic ? 1 : 0, hDC, m_pMain->m_f360Long);
      //ReleaseDC (hWndVis, hDC);
   } // dwLayer

   InvalidateRect (hWndVis, &r, FALSE);
}


/*************************************************************************************
CVisImage::HotSpotMouseIn - Sees which hot spot the mouse is in.

NOTE: this only cares about the main window

inputs
   int            iX - x, in client coords
   int            iY - y, in client coords
returns
   PCCircumrealityHotSpot - Hot spot, or NULL if not in one
*/
PCCircumrealityHotSpot CVisImage::HotSpotMouseIn (int iX, int iY)
{
   POINT pt;
   pt.x = iX;
   pt.y = iY;
   // BUGFIX - Dont do this anymore because this window is now the main
   //if (!PtInRect (&m_rMain, pt))
   //   return NULL;   // not even in the rect

   if (!m_plPCCircumrealityHotSpot || !m_plPCCircumrealityHotSpot->Num())
      return NULL;   // no hot spots
   PCCircumrealityHotSpot *pph = (PCCircumrealityHotSpot*)m_plPCCircumrealityHotSpot->Get(0);

   // only care about the top layer
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   if (!pvi)
      return NULL;
   if (pvi->pTextWindow || pvi->fWaiting)
      return NULL; // dont do this

   // stretched it?
   // if this is outside the stretch to range then ignore
   if (!PtInRect (&pvi->rStretchTo, pt))
      return NULL;

   pt.x -= pvi->rStretchTo.left;
   pt.y -= pvi->rStretchTo.top;

   pt.x = (int) ((double)pt.x / (double)max(pvi->rStretchTo.right - pvi->rStretchTo.left, 1) *
      (double)(pvi->rStretchFrom.right - pvi->rStretchFrom.left));
   pt.y = (int) ((double)pt.y / (double)max(pvi->rStretchTo.bottom - pvi->rStretchTo.top, 1) *
      (double)(pvi->rStretchFrom.bottom - pvi->rStretchFrom.top));

   pt.x += pvi->rStretchFrom.left;
   pt.y += pvi->rStretchFrom.top;
   //}
   //else {
   //   pt.x -= m_rMain.left;
   //   pt.y -= m_rMain.top;
   //}

   // if any zooming, deal with
   if (pvi->pTransition && pvi->pImage)
      pvi->pTransition->HotSpotRemap (&pt, pvi->pImage->Width(), pvi->pImage->Height());

   // if this is a 360 view then convert from stretched coords to 360 coords
   BOOL fModulo = FALSE;

   if (Is360(pvi)) {
      DWORD dwX, dwY;
      pvi->pImage->Surround360BitPixToOrig (m_fIconic ? 1 : 0, pt.x, pt.y, m_pMain->m_f360Long, &dwX, &dwY);
      pt.x = (int)dwX;
      pt.y = (int)dwY;

      fModulo = TRUE;
   }

   // see if it's in any hotspots
   DWORD i;
   for (i = 0; i < m_plPCCircumrealityHotSpot->Num(); i++, pph++) {
      POINT pt2 = pt;

      // if in 360 degrees then use modulo boundary points
      if (fModulo) {
         if (pt2.x >= pph[0]->m_rPosn.right)
            pt2.x -= (int)pvi->pImage->Width();
         else if (pt2.x < pph[0]->m_rPosn.left)
            pt2.x += (int)pvi->pImage->Width();

         if (pt2.y >= pph[0]->m_rPosn.bottom)
            pt2.y -= (int)pvi->pImage->Height();
         else if (pt2.y < pph[0]->m_rPosn.top)
            pt2.y += (int)pvi->pImage->Height();
      }

      if (PtInRect (&pph[0]->m_rPosn, pt2))
         return pph[0];

   }

   // else, cant find
   return NULL;
}




/*************************************************************************************
CVisImage::CanGetFocus - Returns TRUE if this can get the focus (basically if it's
an escwindow
*/
BOOL CVisImage::CanGetFocus (void)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   if (!pvi)
      return FALSE;

   return pvi->pTextWindow ? TRUE : FALSE;
}


/*************************************************************************************
CVisImage::NodeGet - Returns access to the top pvi->pNode info
*/
PCMMLNode2 CVisImage::NodeGet (void)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   return pvi ? pvi->pNode : NULL;
}

/*************************************************************************************
CVisImage::MenuGet - Returns access to the m_pNodeMenu info. This could be NULL
*/
PCMMLNode2 CVisImage::MenuGet (void)
{
   return m_pNodeMenu;
}

/*************************************************************************************
CVisImage::SlidersGet - Returns access to the m_pNodeSliders info. This could be NULL
*/
PCMMLNode2 CVisImage::SlidersGet (void)
{
   return m_pNodeSliders;
}


/*************************************************************************************
CVisImage::HypnoEffectGet - Returns access to the m_pNodeHypnoEffect info. This could be NULL
*/
PCMMLNode2 CVisImage::HypnoEffectGet (void)
{
   return m_pNodeHypnoEffect;
}



/*************************************************************************************
CVisImage::SlidersGetInterp - Gets the sliders and fills in a list with
OBJECTSLIDERS for easy interpretation.
*/
void CVisImage::SlidersGetInterp (PCListFixed pl)
{
   pl->Init (sizeof(OBJECTSLIDERS));

   if (!m_pNodeSliders)
      return;

   // loop
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   OBJECTSLIDERS os;
   double f;
   for (i = 0; i < m_pNodeSliders->ContentNum(); i++) {
      pSub = NULL;
      m_pNodeSliders->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, L"Slider"))
         continue;   // not a slider

      // get some values
      memset (&os, 0, sizeof(os));
      os.cColorLight = RGB(0x10, 0x10, 0x00);
      os.cColorDark = RGB(0xff, 0xff, 0xff);
      os.cColorTip = RGB(0, 0, 0);
         // NOTE: Not affected by m_fLightBackground
      
      if (pSub->AttribGetDouble (L"v", &f)) {
         f = max(f, -1.0);
         f = min(f, 1.0);
         os.fValue = f;
      }
      if (psz = pSub->AttribGetString (L"color")) {
         AttribToColor (psz, &os.cColorLight);
         os.cColorTip = os.cColorDark = os.cColorLight;
      }
      if (psz = pSub->AttribGetString (L"l")) {
         DWORD dwLen = (DWORD)wcslen(psz);

         // make sure doens't excede size of left
         dwLen = min(dwLen, sizeof(os.szLeft)/sizeof(WCHAR)-1);
         memcpy (os.szLeft, psz, dwLen*sizeof(WCHAR));
         os.szLeft[dwLen] = 0;
      }
      if (psz = pSub->AttribGetString (L"r")) {
         DWORD dwLen = (DWORD)wcslen(psz);

         // make sure doens't excede size of left
         dwLen = min(dwLen, sizeof(os.szRight)/sizeof(WCHAR)-1);
         memcpy (os.szRight, psz, dwLen*sizeof(WCHAR));
         os.szRight[dwLen] = 0;
      }

      pl->Add (&os);
   } // i

}


/*************************************************************************************
CVisImage::IsAnyLayer360 - Returns TRUE if any of the layers are 360
*/
BOOL CVisImage::IsAnyLayer360 (void)
{
   PVILAYER pvi = (PVILAYER) m_lVILAYER.Get(0);
   DWORD dwLayer;
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++)
      if (Is360 (pvi))
         return TRUE;

   // else
   return FALSE;
}

/*************************************************************************************
CVisImage::WindowForTextGet - Gets the window that's the text layer
*/
HWND CVisImage::WindowForTextGet (void)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   if (!pvi)
      return NULL;

   return pvi->pTextWindow ? pvi->pTextWindow->m_hWnd : NULL;
}

/*************************************************************************************
CVisImage::EliminateDeadLayers - If there's a layer beneath a completely covering
layer then elimiante it.
*/
void CVisImage::EliminateDeadLayers (void)
{
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   DWORD dwLayer;
   // if have a text window then all layes except the first are dead
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) { // BUGFIX - Forgot to increment pvi
      // if it's a text window then special case
      if (pvi->pTextWindow) {
         if (!dwLayer)
            dwLayer++;  // delete after this
         break;   // so will deelte
      }

      if (pvi->pTransition && !pvi->pTransition->AnimateNeedBelow()) {
         // dont need anything below this
         dwLayer++;
         break;
      }
   } // dwLayer

   // delete from dwLayer on down
   while (m_lVILAYER.Num() > dwLayer)
      LayerClear (dwLayer);
}


/*************************************************************************************
CVisImage::AnimateAdvance - Advance any animations happening in the transitions.

inputs
   fp          fDelta - Delta time
*/
void CVisImage::AnimateAdvance (fp fDelta)
{
   // potentially blink. must have lip sync
   if (m_fLipSync) {
      if (m_fBlinkWait >= 0) {
         // wait for blink time
         m_fBlinkWait -= fDelta;

         // blink, but only it empty viseme
         if (m_dwViseme == 0 && (m_fBlinkWait < 0)) {
            m_fBlinkWait = -BLINKDURATION;
            Viseme (VISEME_BLINK, (DWORD)-1);
         }
         else
            m_fBlinkWait = max(m_fBlinkWait, 0);
      }
      else {
         // unblink
         m_fBlinkWait += fDelta;

         // unblink, but only if blinked
         if (m_fBlinkWait >= 0) {
            // even if no longer blinking, set a new blink time
            m_fBlinkWait = randf(BLINKMIN, BLINKMAX);
            if (m_dwViseme == VISEME_BLINK)
               Viseme (0, (DWORD)-1);
         }
      }
   } // if lip sync

   DWORD dwLayer;
   PVILAYER pvi = (PVILAYER)m_lVILAYER.Get(0);
   BOOL fAdvanced = FALSE;
   for (dwLayer = 0; dwLayer < m_lVILAYER.Num(); dwLayer++, pvi++) {
      if (!pvi->pTransition || !pvi->pImage) // NOTE: Dont advance unless there's an image
         continue;
      if (!pvi->pTransition->AnimateQuery ())
         continue;   // no advancing

      // else advance
      pvi->pTransition->AnimateAdvance (fDelta);
      fAdvanced = TRUE;
   } // dwLayer

   if (!fAdvanced)
      return;  // nothing needed to advance

   // clear out what's below, since as advanced animation may not need
   // what appears below
   EliminateDeadLayers();

   // clear what's in the layered image
   LayeredClear();

   // redraw
   InvalidateRect (m_hWnd, &m_rClient, FALSE);
}


/*************************************************************************************
CVisImage::LayeredClear - Clears out the image stored in the layered image
*/
void CVisImage::LayeredClear (void)
{
   if (m_pisLayered) {
      delete m_pisLayered;
      m_pisLayered = NULL;
   }
   if (m_hBmpLayered) {
      DeleteObject (m_hBmpLayered);
      m_hBmpLayered = NULL;
   }
}



/*************************************************************************************
CVisImage::LayeredCalc - Calculate the layered image.

inputs
   HDC         hDC - Drawing on
returns
   BOOL - FALSE if failure to create an image at all.
          TRUE if an image can be created.
          NOTE: If this returns TRUE, m_pisLayer
            may NOT be calculated IF it turns out there is only one image and it has
            no animation needed for it.
*/
BOOL CVisImage::LayeredCalc (HDC hDC)
{
   if (m_pisLayered)
      return TRUE;

   // else, create all the info of where to copy to

   // find the list
   PVILAYER pvi = (PVILAYER) m_lVILAYER.Get(0);
   DWORD dwNum = m_lVILAYER.Num();
   if (!dwNum)
      return FALSE;

   // find an actual image that can draw from
   DWORD i;
   PVILAYER pviFirstImage = NULL;
   for (i = 0; i < dwNum; i++)
      if (pvi[i].pImage) {
         pviFirstImage = pvi + i;
         break;
      }
   // if there isn't a first image of any sort then can't draw
   if (!pviFirstImage)
      return FALSE;

   // loop through all the images and figure out the stretch info
   for (i = 0; i < dwNum; i++, pvi++) {
      if (!pvi->pImage)
         continue;   // no info yet

      // we're showing a backroung image. look at the size and stretch information
      // to figure out what need...

      RECT rTo, rFrom;
      rTo = m_rClient;
      rFrom.top = rFrom.left = 0;
      rFrom.right = (int)pvi->dwWidth;
      rFrom.bottom = (int)pvi->dwHeight;
      switch (pvi->dwStretch) {
      default:
      case 1:  // stretch over entire area
         // do nothign special
         break;
      case 0:  // just center, no size
      case 2:  // scale so entirely visible
      case 3:  // scale so nothing left
         {
            // figure out how would need to scale to fit...
            int iToWidth = max(rTo.right - rTo.left, 1);
            int iToHeight = max(rTo.bottom - rTo.top, 1);
            float fScaleX = (float)iToWidth / (float)pvi->dwWidth;
            float fScaleY = (float)iToHeight / (float)pvi->dwHeight;
            float fScale;
            if (pvi->dwStretch == 0)
               fScale = 1;
            else if (pvi->dwStretch == 2)
               fScale = min(fScaleX, fScaleY);
            else
               fScale = max(fScaleX, fScaleY);
            fScale = max (fScale, 0.0001);

            // adjust to rectangle
            // BUGFIX - No longer need to limit iToWidth and and iToHeight
            // because added code to image bitmap draw
            // iToWidth = min(iToWidth, (int)((float)m_dwWidth * fScale));
            // iToHeight = min(iToHeight, (int)((float)m_dwHeight * fScale));
            iToWidth = (int)((float)pvi->dwWidth * fScale);
            iToHeight = (int)((float)pvi->dwHeight * fScale);
            iToWidth = max(iToWidth, 1);
            iToHeight = max(iToHeight, 1);
            rTo.left = ((rTo.left + rTo.right) - iToWidth)/2;
            rTo.right = rTo.left + iToWidth;
            rTo.top = ((rTo.top + rTo.bottom) - iToHeight)/2;
            rTo.bottom = rTo.top + iToHeight;

            // adjust from rectangle
            iToWidth = (int)((float)iToWidth / fScale);
            iToHeight = (int)((float)iToHeight / fScale);
            // BUGFIX - No longer need to limit iToWidth and and iToHeight
            // because added code to image bitmap draw
            //iToWidth = min(iToWidth, (int)m_dwWidth);
            //iToHeight = min(iToHeight, (int)m_dwHeight);
            rFrom.left = ((rFrom.left + rFrom.right) - iToWidth)/2;
            rFrom.right = rFrom.left + iToWidth;
            rFrom.top = ((rFrom.top + rFrom.bottom) - iToHeight)/2;
            rFrom.bottom = rFrom.top + iToHeight;
         }
         break;
      } // switch

      // all the rTo's need to be the same. Therefore, may need to scale rFrom
      if (pvi != pviFirstImage) {
         CMatrix mToToFrom;   // matrix that converts from to to from
         CMatrix m; // temp

         // translate so UL corner of To goes to 0,0
         mToToFrom.Translation (-rTo.left, -rTo.top, 0);

         // scale so coords in To go from 0,0 to 1,1
         m.Scale (1.0 / (fp)(rTo.right - rTo.left), 1.0 / (fp)(rTo.bottom - rTo.top), 1);
         mToToFrom.MultiplyRight (&m);

         // scale to coords go from 0,0 to rFrom widht,height
         m.Scale (rFrom.right - rFrom.left, rFrom.bottom - rFrom.top, 1);
         mToToFrom.MultiplyRight (&m);

         // add to coords go over rFrom left,right,top,bottom
         m.Translation (rFrom.left, rFrom.top, 0);
         mToToFrom.MultiplyRight (&m);

         // now, take UL of pviFirstImage's rect and use that
         CPoint p;
         rTo = pviFirstImage->rStretchTo;
         p.Zero();
         p.p[0] = rTo.left;
         p.p[1] = rTo.top;
         p.MultiplyLeft (&mToToFrom);
         rFrom.left = (int)p.p[0];
         rFrom.top = (int)p.p[1];
         p.Zero();
         p.p[0] = rTo.right;
         p.p[1] = rTo.bottom;
         p.MultiplyLeft (&mToToFrom);
         rFrom.right = (int)p.p[0];
         rFrom.bottom = (int)p.p[1];
      }

      // make sure no 0-sized
      rFrom.right = max(rFrom.right, rFrom.left+1);
      rFrom.bottom = max(rFrom.bottom, rFrom.top+1);
      rTo.right = max(rTo.right, rTo.left+1);
      rTo.bottom = max(rTo.bottom, rTo.top+1);

      // store away
      pvi->rStretchFrom = rFrom;
      pvi->rStretchTo = rTo;
   } // i

   // restore pvi
   pvi = (PVILAYER) m_lVILAYER.Get(0);

   // if no transition then create one here, but leave settings blank so does nothing
   if (!pvi->pTransition) {
      pvi->pTransition = new CTransition;
      pvi->pTransition->AnimateInit ();
   }

   // if there is only one image AND it has no animation applied then dont bother
   // with a layer
   if ((dwNum == 1) && pvi->pTransition->AnimateIsPassthrough())
      return TRUE;   // done, but dont bother with image

   // fill in some arrays with all the information
   DWORD dwNeed = dwNum * (sizeof(PCImageStore) + sizeof(PCTransition) + sizeof(RECT));
   CMem mem;
   if (!mem.Required (dwNeed))
      return FALSE;
   PCImageStore *ppis = (PCImageStore*)mem.p;
   PCTransition *ppt = (PCTransition*)(ppis + dwNum);
   RECT *pr = (RECT*)(ppt + dwNum);
   for (i = 0; i < dwNum; i++, pvi++) {
      ppis[i] = pvi->pImage;
      ppt[i] = pvi->pTransition;
      pr[i] = pvi->rStretchFrom;
   } // i


   // new store
   m_pisLayered = new CImageStore;
   if (!m_pisLayered)
      return FALSE;
   if (!m_pisLayered->Init (
      (DWORD)(pviFirstImage->rStretchTo.right - pviFirstImage->rStretchTo.left),
      (DWORD)(pviFirstImage->rStretchTo.bottom - pviFirstImage->rStretchTo.top) )) {
         delete m_pisLayered;
         m_pisLayered = NULL;
         return FALSE;
      }

   COLORREF cFadeColor;
#ifdef MIFTRANSPARENTWINDOWS // dont do for transparent windows
   BOOL fTransparent = !m_pMain->ChildHasTitle (m_hWnd);
   cFadeColor = LAYEREDTRANSPARENTCOLOR;
#else
   BOOL fTransparent = FALSE;
   cFadeColor = m_pMain->m_cBackground;
#endif

   // animate
   if (!ppt[0]->AnimateFrame (m_fIconic ? 1 : 0, m_pMain->m_f360Long, ppis[0], pr + 0,
      cFadeColor, dwNum-1, ppis + 1, ppt + 1, pr + 1, m_pisLayered)) {
         delete m_pisLayered;
         m_pisLayered = NULL;
         return FALSE;
      }

   // create a new bitmap
   m_hBmpLayered = m_pisLayered->ToBitmap (hDC);
   if (!m_hBmpLayered) {
      delete m_pisLayered;
      m_pisLayered = NULL;
      return FALSE;
   }

   return TRUE;
}



/*************************************************************************************
CVisImage::Viseme - Tell the image that a new viseme has been drawn.

inputs
   DWORD          dwViseme - Viseme number, from EnglishPhoneToViseme
   DWORD          dwTickCount - Tick count timer, from GetTickCount(). Used to make
                  sure viseme rendering isn't that far behind. -1 to ignore dwTickCount
returns
   none
*/
void CVisImage::Viseme (DWORD dwViseme, DWORD dwTickCount)
{
   // if doesn't lip sync then ignore
   if (!m_fLipSync)
      return;
   
   // if viseme already drawing, then ignore
   if (dwViseme == m_dwViseme)
      return;

   // if no lip sync then ignore
   if (!m_pMain->m_fLipSync)
      return;

   // BUGFIX - To make sure that don't get behind in lip sync
   if ((dwTickCount != (DWORD)-1) && (dwViseme != 0)) {
      DWORD dwTime = GetTickCount();
      DWORD dwDiff = max(dwTime, dwTickCount) - min(dwTime, dwTickCount);
      if (dwDiff >= 200)
         return;  // too much of a difference so don't bother to change
   }

   // see if viseme is already rendered. If it isn't then don't bother
   CMem memViseme;
   if (!VisemeModifyMML(dwViseme, (PWSTR) m_memMaster.p, &memViseme))
      return;  // error

   // try to open
   DWORD dwQuality;
   for (dwQuality = NUMIMAGECACHE-1; dwQuality < NUMIMAGECACHE; dwQuality--) {
      if (ImageCacheExists ((PWSTR)memViseme.p, dwQuality, FALSE))
         break;
   } // dwQuality

   // might want to cause this to load if haven't found the best quality
   DWORD i;
   if (dwQuality != NUMIMAGECACHE-1)
      for (i = 0; i < NUMIMAGECACHE; i++)
         if ((i > dwQuality) || (dwQuality >= NUMIMAGECACHE))
            m_pMain->ImageLoadThreadAdd ((PWSTR)memViseme.p, i, 0);

   if (dwQuality >= NUMIMAGECACHE) {
#ifdef _DEBUG
      char szTemp[64];
      sprintf (szTemp, "\r\nViseme not exist = %d", (int) dwViseme);
      OutputDebugString (szTemp);
#endif

      // BUGFIX - Bump up render priority
      // BUGFIX - Only create once every 10 tries, so not always slowing down
      m_pMain->m_pRT->BumpRenderPriority ((PWSTR)memViseme.p, !(rand()%10));


      return;  // dont change since doesnt exist
   }


   // convert to MML and then add
   PCMMLNode2 pNode = CircumrealityParseMML ((PWSTR)memViseme.p);
   if (!pNode)
      return;

   m_dwViseme = dwViseme;
   Update (pNode, (PWSTR)memViseme.p, NULL, NULL, NULL, NULL, NULL, NULL, TRUE, m_fCanChatTo,
      0 /* no time since not changing menu */);

   // make sure is drawn right away
   SeeIfRendered ();

   // make sure it's drawn right away
   if (m_hWnd)
      UpdateWindow (m_hWnd);
}




// BUGBUG - When try to open the image from the megafile it also includes information
// about hotspots and transition that aren't really needed. May want to eliminate
// these from the image name before saving it, and likewise, eliminate it from
// the image name before trying to reload it
