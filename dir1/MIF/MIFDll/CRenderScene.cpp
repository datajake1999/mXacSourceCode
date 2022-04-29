/*************************************************************************************
CRenderScene.cpp - Code for UI to modify a scene to be rendered, and for rendering it.

begun 13/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"





#define SCALEPREVIEW          0      // resolution of preview

#define SHADOWSLIMITDEFAULT      30.0     // 50 meters
   // BUGFIX - Changed from 50 to 30.0 to speed up

// RSATTRIB - Structure for modifying attribute
typedef struct {
   GUID              gObject;    // object it's affecting, used for m_fLoadFromFile
   DWORD             dwObject;   // object index, used for !m_fLoadFromFile
   WCHAR             szAttrib[64];  // attribute name
   WCHAR             szName[128];   // if not NULL, use this instead of the GUID. Not a UI option though
   fp                fValue;     // value it's modified to
} RSATTRIB, *PRSATTRIB;

// RSOBJECT - Structure for object
typedef struct {
   GUID              gMajor;     // major ID
   GUID              gMinor;     // minor ID
   CPoint            pLoc;       // location
   CPoint            pRot;       // rotation
   DWORD             dwAttachTo; // other object that is attached to. (Index into list of objects). -1 if not
   WCHAR             szAttachBone[64]; // bone that this is attached to
} RSOBJECT, *PRSOBJECT;

// RSCOLOR - Stores surface information
typedef struct {
   GUID              gObject;    // object it's affecting, used for m_fLoadFromFile
   DWORD             dwObject;   // object index, used for !m_fLoadFromFile
   PCObjectSurface   pSurf;      // surface
} RSCOLOR, *PRSCOLOR;

// CM3DFileCache - For cachine extra info
class CM3DFileCache {
public:
   ESCNEWDELETE;

   CM3DFileCache (void);
   ~CM3DFileCache (void);

   CMem        m_memM3DFileName;          // file name
   COLORREF    m_cM3DFileBackground;      // bacground
   fp          m_fM3DFileExposure;        // exposure
   DWORD       m_dwM3DFileAmbientExtra;   // ambient extra
   DWORD       m_dwM3DFileCamera;         // camera
   CPoint      m_pM3DFileCenter;          // camera center
   CPoint      m_pM3DFileRot;             // camera rot
   CPoint      m_pM3DFileTrans;           // camera trans
   fp          m_fM3DFileScale;           // camera scale

   PCWorld     m_pWorld;                  // world. Always NULL for 0th item of galPCM3DFileCache
   PCSceneSet  m_pSceneSet;               // scene set thats swapped out. Always NULL for 0th item of galPCM3DFileCache
   PCRenderTraditional  m_pRTShadow;      // traditional render where shadows are stored
};
typedef CM3DFileCache *PCM3DFileCache;


static PWSTR gpszEnabledFalse = L" enabled=false ";

static CListFixed     galPCM3DFileCache[MAXRENDERSHARDS];      // list of caches. 0th item is what's currently loaded



/*************************************************************************************
CM3DFileCache::Constructor and destructor
*/
CM3DFileCache::CM3DFileCache (void)
{
   MemZero (&m_memM3DFileName);
   m_cM3DFileBackground = 0;
   m_fM3DFileExposure = 0;
   m_dwM3DFileAmbientExtra = 0;
   m_dwM3DFileCamera = 0;
   m_pM3DFileCenter.Zero();
   m_pM3DFileRot.Zero();
   m_pM3DFileTrans.Zero();
   m_fM3DFileScale = 0;

   m_pWorld = NULL;
   m_pSceneSet = NULL;
   m_pRTShadow = NULL;
}

/*************************************************************************************
CM3DFileCache::Constructor and destructor
*/
CM3DFileCache::~CM3DFileCache (void)
{
   // BUGFIX - MOve deleting sceneset first
   if (m_pSceneSet)
      delete m_pSceneSet;
   if (m_pWorld)
      delete m_pWorld;
   if (m_pRTShadow)
      delete m_pRTShadow;
}

/*************************************************************************************
MyCacheM3DFileLimitSize - Limits the number of cached items... Items at the top of
the list have been used most recently.
*/
void MyCacheM3DFileLimitSize (DWORD dwRenderShard)
{
   while (galPCM3DFileCache[dwRenderShard].Num() > 10 ) {
      PCM3DFileCache *ppFile = (PCM3DFileCache*) galPCM3DFileCache[dwRenderShard].Get(0);
      DWORD dwNum = galPCM3DFileCache[dwRenderShard].Num()-1;
      delete ppFile[dwNum];
      galPCM3DFileCache[dwRenderShard].Remove (dwNum);
   }
}

/*************************************************************************************
MyCacheM3DFileOpen - Cached M3DFileOpen that will keep the same file if it already
   exists

inputs
   PWSTR       pszFile - File name
   BOOL        *pfFailedToLoad - Filled with TRUE if fails to load
   PCProgressSocket pProgress - Progress
   BOOL        fViewLoad - Set to TRUE if want to laod the views. Defaults to TRUE
   PCRenderTraditional *ppRender - Renderer. Can be NULL
   PCRenderTraditional *ppRTShadow - Filled in with the rendere where tos tore shadows.
               Might be filled with NULL. Do NOT delete this
returns
   BOOL - TRUE if success
*/
BOOL MyCacheM3DFileOpen (DWORD dwRenderShard, PWSTR pszFile, BOOL *pfFailedToLoad, PCProgressSocket pProgress,
                            BOOL fViewLoad, PCRenderTraditional *ppRender,
                            PCRenderTraditional *ppRTShadow)
{
#ifdef _DEBUG
   OutputDebugString ("\r\nMyCacheM3DFileOpen: ");
   OutputDebugStringW (pszFile);
   OutputDebugString ("\r\n");
#endif

   if (ppRTShadow)
      *ppRTShadow = NULL;

   // see if can find a match
   DWORD i;
   PCM3DFileCache *ppFile = (PCM3DFileCache*) galPCM3DFileCache[dwRenderShard].Get(0);
   PCM3DFileCache pUse;
   if (pszFile && pszFile[0]) for (i = 0; i < galPCM3DFileCache[dwRenderShard].Num(); i++) {
      if (_wcsicmp(pszFile, (PWSTR)ppFile[i]->m_memM3DFileName.p))
         continue;   // different

      // else, found a match

      // if it's not the first element then swap out
      if (i) {
         PCM3DFileCache pWant = ppFile[i];
         PCM3DFileCache pSwapOut = ppFile[0];
         galPCM3DFileCache[dwRenderShard].Remove (i);  // do after get pWant and pSwapOut
         pSwapOut->m_pWorld = M3DWorldSwapOut (dwRenderShard, &pSwapOut->m_pSceneSet);
         if (!((PWSTR)pSwapOut->m_memM3DFileName.p)[0]) {
            // what currently have isn't to be kept
            galPCM3DFileCache[dwRenderShard].Remove (0);
            delete pSwapOut;
         }

         // put the new one in place
         M3DWorldSwapIn (pWant->m_pWorld, pWant->m_pSceneSet);
         pWant->m_pWorld = NULL; // so dont accidentally delete
         pWant->m_pSceneSet = NULL;
         galPCM3DFileCache[dwRenderShard].Insert (0, &pWant);
      }

      // gauranteed to be the first element
      ppFile = (PCM3DFileCache*) galPCM3DFileCache[dwRenderShard].Get(0);
      pUse = ppFile[0];

      // else, already loaded, so undo and reload
      PCSceneSet pSceneSet = NULL;
      PCWorldSocket pWorld = WorldGet (dwRenderShard, &pSceneSet);
      if (!pWorld)
         return FALSE;

      // set some stuff
      if (pfFailedToLoad)
         *pfFailedToLoad = FALSE;

      // undo everything
      while (pWorld->UndoQuery (NULL))
         pWorld->Undo(TRUE);

      PCRenderTraditional pRender = NULL;
      if (ppRender) {
         pRender = new CRenderTraditional(dwRenderShard);
         if (*ppRender)
            delete *ppRender; // BUGFIX - to clear old
         *ppRender = pRender;

         // priority increase is negative for subsequent render shards
         // BUGFIX - Not dependent on render shard
         // pRender->m_iPriorityIncrease = -(int)dwRenderShard;
      }

      if (pRender) {
//   {
//   fp fExp = pRender->ExposureGet();
//   CHAR szTemp[64];
//   sprintf (szTemp, "nExposure MyCacheM3DFileOpen=%g", (double)fExp);
//   MessageBox (NULL, "Exposure", szTemp, MB_OK);
//   }

         // reset some values
         pRender->BackgroundSet (pUse->m_cM3DFileBackground);
         pRender->ExposureSet (pUse->m_fM3DFileExposure);
         pRender->AmbientExtraSet (pUse->m_dwM3DFileAmbientExtra);

         switch (pUse->m_dwM3DFileCamera) {
         case CAMERAMODEL_FLAT:
            pRender->CameraFlat (&pUse->m_pM3DFileCenter, pUse->m_pM3DFileRot.p[2], pUse->m_pM3DFileRot.p[0], pUse->m_pM3DFileRot.p[1], pUse->m_fM3DFileScale, pUse->m_pM3DFileTrans.p[0], pUse->m_pM3DFileTrans.p[1]);
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            pRender->CameraPerspWalkthrough (&pUse->m_pM3DFileTrans, pUse->m_pM3DFileRot.p[2], pUse->m_pM3DFileRot.p[0], pUse->m_pM3DFileRot.p[1], pUse->m_fM3DFileScale);
            break;
         case CAMERAMODEL_PERSPOBJECT:
            pRender->CameraPerspObject (&pUse->m_pM3DFileTrans, &pUse->m_pM3DFileCenter, pUse->m_pM3DFileRot.p[2], pUse->m_pM3DFileRot.p[0], pUse->m_pM3DFileRot.p[1], pUse->m_fM3DFileScale);
            break;
         }
      } // if want renderer 

      if (ppRTShadow) {
         if (!pUse->m_pRTShadow)
            pUse->m_pRTShadow = new CRenderTraditional(dwRenderShard);   // doesn't really need to be initialized

         *ppRTShadow = pUse->m_pRTShadow;


         // priority increase is negative for subsequent render shards
         // BUGFIX - Not dependent on render shard
         // pUse->m_pRTShadow->m_iPriorityIncrease = -(int)dwRenderShard;
      }

      return TRUE;
   } // i

   // else can't find...

   // if there current best already exists but has no name then delete it
   pUse = ppFile[0];
   if (((PWSTR)pUse->m_memM3DFileName.p)[0]) {
      // swap it out and keep a record of it
      pUse->m_pWorld = M3DWorldSwapOut (dwRenderShard, &pUse->m_pSceneSet);
   }
   else {
      // delete this
      delete pUse;
      galPCM3DFileCache[dwRenderShard].Remove (0);
   }

   // make sure not too many
   MyCacheM3DFileLimitSize (dwRenderShard);

   // create a new one
   pUse = new CM3DFileCache;
   if (!pUse)
      return FALSE;
   galPCM3DFileCache[dwRenderShard].Insert (0, &pUse);

   PCRenderTraditional  pRender = NULL;

   if (!M3DFileOpen (dwRenderShard, pszFile, pfFailedToLoad, pProgress, fViewLoad, &pRender))
      return FALSE;

   // else, loaded
   MemCat (&pUse->m_memM3DFileName, pszFile);

   // store some information away
   if (ppRender) {
      if (*ppRender)
         delete *ppRender; // BUGFIX - to clear old
      *ppRender = pRender;
   }

   pUse->m_cM3DFileBackground = pRender->BackgroundGet ();
   pUse->m_fM3DFileExposure = pRender->ExposureGet ();
   pUse->m_dwM3DFileAmbientExtra = pRender->AmbientExtraGet ();

   pUse->m_dwM3DFileCamera = pRender->CameraModelGet ();
   switch (pUse->m_dwM3DFileCamera) {
   case CAMERAMODEL_FLAT:
      pRender->CameraFlatGet (&pUse->m_pM3DFileCenter, &pUse->m_pM3DFileRot.p[2], &pUse->m_pM3DFileRot.p[0], &pUse->m_pM3DFileRot.p[1], &pUse->m_fM3DFileScale, &pUse->m_pM3DFileTrans.p[0], &pUse->m_pM3DFileTrans.p[1]);
      break;
   case CAMERAMODEL_PERSPWALKTHROUGH:
      pRender->CameraPerspWalkthroughGet (&pUse->m_pM3DFileTrans, &pUse->m_pM3DFileRot.p[2], &pUse->m_pM3DFileRot.p[0], &pUse->m_pM3DFileRot.p[1], &pUse->m_fM3DFileScale);
      break;
   case CAMERAMODEL_PERSPOBJECT:
      pRender->CameraPerspObjectGet (&pUse->m_pM3DFileTrans, &pUse->m_pM3DFileCenter,  &pUse->m_pM3DFileRot.p[2], &pUse->m_pM3DFileRot.p[0], &pUse->m_pM3DFileRot.p[1], &pUse->m_fM3DFileScale);
      break;
   }

   if (!ppRender)
      delete pRender;

   if (ppRTShadow) {
      if (!pUse->m_pRTShadow)
         pUse->m_pRTShadow = new CRenderTraditional(dwRenderShard);   // doesn't really need to be initialized

      *ppRTShadow = pUse->m_pRTShadow;

      // priority increase is negative for subsequent render shards
      // BUGFIX - Not dependent on render shard
      // pUse->m_pRTShadow->m_iPriorityIncrease = -(int)dwRenderShard;
   }

   return TRUE;


}

/*************************************************************************************
MyCacheM3DFileNew - Internal function to use cache

inputs
   PCRenderTraditional *ppRender - Renderer
*/
void MyCacheM3DFileNew (DWORD dwRenderShard, PCRenderTraditional *ppRender, BOOL fCreateGroundSky = TRUE)
{
#ifdef _DEBUG
   OutputDebugString ("\r\nMyCacheM3DFileNew\r\n");
#endif

   // if there current best already exists but has no name then delete it
   PCM3DFileCache *ppFile = (PCM3DFileCache*) galPCM3DFileCache[dwRenderShard].Get(0);
   PCM3DFileCache pUse = ppFile[0];
   if (((PWSTR)pUse->m_memM3DFileName.p)[0]) {
      // swap it out and keep a record of it
      pUse->m_pWorld = M3DWorldSwapOut (dwRenderShard, &pUse->m_pSceneSet);
   }
   else {
      // delete this
      delete pUse;
      galPCM3DFileCache[dwRenderShard].Remove (0);
   }

   // make sure not too many
   MyCacheM3DFileLimitSize (dwRenderShard);

   pUse = new CM3DFileCache;
   galPCM3DFileCache[dwRenderShard].Insert (0, &pUse);

   M3DFileNew (dwRenderShard, fCreateGroundSky);

   if (ppRender) {
      if (*ppRender)
         delete *ppRender; // to stop leak
      *ppRender = new CRenderTraditional(dwRenderShard);
      (*ppRender)->CWorldSet (WorldGet(dwRenderShard, NULL));

      // priority increase is negative for subsequent render shards
      // BUGFIX - Not dependent on render shard
      // (*ppRender)->m_iPriorityIncrease = -(int)dwRenderShard;
   }
}



/*************************************************************************************
MyCacheM3DFileInit - Initializes the cache. Call after M3DInit().
*/
void MyCacheM3DFileInit (DWORD dwRenderShard)
{
   if (galPCM3DFileCache[dwRenderShard].Num())
      return;  // done

   galPCM3DFileCache[dwRenderShard].Init (sizeof(PCM3DFileCache));

   // create a blank one representing the current file
   PCM3DFileCache pNew = new CM3DFileCache;
   if (!pNew)
      return;
   galPCM3DFileCache[dwRenderShard].Add (&pNew);
}


/*************************************************************************************
MyCacheM3DFileEnd - Frees up memory used by the cache. Call before M3DEnd().
*/
void MyCacheM3DFileEnd (DWORD dwRenderShard)
{
   PCM3DFileCache *ppFile = (PCM3DFileCache*)galPCM3DFileCache[dwRenderShard].Get(0);
   DWORD i;
   for (i = 0; i < galPCM3DFileCache[dwRenderShard].Num(); i++)
      delete ppFile[i];
   galPCM3DFileCache[dwRenderShard].ClearCompletely();
}
/*************************************************************************************
OpenImageDialog - Dialog box for opening a .jpg or .bmp

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL OpenImageDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   strcpy (szInitial, gszAppDir);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "All (*.bmp,*.jpg)\0*.bmp;*.jpg;*.jpeg\0JPEG (*.jpg)\0*.jpg;*.jpeg\0Bitmap (*.bmp)\0*.bmp\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save image" :
      "Open an image file";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "jpg";
   // nFileExtension 

   if (fSave) {
      if (!GetSaveFileName(&ofn))
         return FALSE;
   }
   else {
      if (!GetOpenFileName(&ofn))
         return FALSE;
   }

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // copy over
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, pszFile, dwChars);
   return TRUE;
}



/*************************************************************************************
ImageBlendImageStore - Given an image store, this stretches it and blends it
into the given image. It only replaces areas where there are no objects

inputs
   PCImage        pImage - Image
   PCImageStore   pStore - Image store
   BOOL           f360 - If TRUE this is a 360 image, so blending loops around
   BOOL           fNonBlackToObject - If non-black (LAYEREDTRANSPARENTCOLOR) color then create a fake object ID.
returns
   none
*/
void ImageBlendImageStore (PCImage pImage, PCImageStore pStore, BOOL f360, BOOL fNonBlackToObject)
{
   GammaInit ();

   double afImageToStore[2];
   afImageToStore[0] = (double)pStore->Width() / (double)pImage->Width();
   afImageToStore[1] = (double)pStore->Height() / (double)pImage->Height();

   // loop over all pixels
   DWORD x, y, i;
   DWORD dwWidth, dwHeight;
   DWORD adwStoreSize[2];
   PIMAGEPIXEL pip = pImage->Pixel(0,0);
   dwWidth = pImage->Width();
   dwHeight = pImage->Height();
   adwStoreSize[0] = pStore->Width();
   adwStoreSize[1] = pStore->Height();
   double afCur[2], afFloor[2];
   DWORD adwUL[2], adwLR[2];
   BOOL fExact;
   PBYTE pb;

   for (y = 0; y < dwHeight; y++) {
      afCur[1] = (double)y * afImageToStore[1];
      afFloor[1] = floor(afCur[1]);
      afCur[1] -= afFloor[1];
      adwUL[1] = (DWORD)afFloor[1];
      adwLR[1] = adwUL[1] + 1;
      adwUL[1] = min(adwUL[1], adwStoreSize[1]-1);
      adwLR[1] = min(adwLR[1], adwStoreSize[1]-1);

      fExact = !afCur[1];

      for (x = 0; x < dwWidth; x++, pip++) {

         // if something there dont bother
         if (pip->dwID)
            continue;

         afCur[0] = (double)x * afImageToStore[0];
         afFloor[0] = floor(afCur[0]);
         afCur[0] -= afFloor[0];
         adwUL[0] = (DWORD)afFloor[0];
         adwLR[0] = adwUL[0] + 1;
         if (f360) {
            adwUL[0] = adwUL[0] % adwStoreSize[0];
            adwLR[0] = adwLR[0] % adwStoreSize[0];
         }
         else {
            adwUL[0] = min(adwUL[0], adwStoreSize[0]-1);
            adwLR[0] = min(adwLR[0], adwStoreSize[0]-1);
         }

         if (fExact && !afCur[0]) {
            // exact match, so no scaling
            pb = pStore->Pixel (adwUL[0], adwUL[1]);

            pip->wRed = Gamma(pb[0]);
            pip->wGreen = Gamma(pb[1]);
            pip->wBlue = Gamma(pb[2]);

            if (fNonBlackToObject) {
               if (RGB(pb[0], pb[1], pb[2]) == LAYEREDTRANSPARENTCOLOR)
                  pip->dwID = 0;
               else
                  pip->dwID = 1; // so have something
            }
            continue;
         }
         
         // else, blend...
         for (i = 0; i < 3; i++) {
            double fTop = (double)Gamma((pStore->Pixel(adwUL[0], adwUL[1]))[i]) * (1.0 - afCur[0]) +
               (double) Gamma((pStore->Pixel(adwLR[0], adwUL[1]))[i]) * afCur[0];
            double fBottom = (double)Gamma((pStore->Pixel(adwUL[0], adwLR[1]))[i]) * (1.0 - afCur[0]) +
               (double) Gamma((pStore->Pixel(adwLR[0], adwLR[1]))[i]) * afCur[0];

            fTop = fTop * (1.0 - afCur[1]) + fBottom * afCur[1];
            (&pip->wRed)[i] = (WORD)fTop;
         } // i

         if (fNonBlackToObject) {
            if (UnGamma (&pip->wRed) == LAYEREDTRANSPARENTCOLOR)
               pip->dwID = 0;
            else
               pip->dwID = 1; // so have something
         } // nonblack

      } // x
   } // y
}


/*************************************************************************************
ImageBlendedBack - Fills in a blended background for the image.

inputs
   PCImage        pImage - Image
   COLORREF       cBack0 - Background on top/left
   COLROREF       cBack1 - Background on bottom/right
   BOOL           fBackBlendLR - If TRUE then left to right blend, else top to bottom
   BOOL           fNonBlackToObject - If non-black (LAYEREDTRANSPARENTCOLOR) color then create a fake object ID.
returns
   none
*/
void ImageBlendedBack (PCImage pImage, COLORREF cBack0, COLORREF cBack1, BOOL fBackBlendLR, BOOL fNonBlackToObject)
{
   WORD awBack[2][3];
   WORD awLine[3];
   PIMAGEPIXEL pip;
   DWORD i;
   GammaInit();
   Gamma (cBack0, awBack[0]);
   Gamma (cBack1, awBack[1]);

   DWORD dwLines = fBackBlendLR ? pImage->Width() : pImage->Height();
   DWORD dwAcross = fBackBlendLR ? pImage->Height() : pImage->Width();
   DWORD dwLine, dwPix;
   DWORD dwDelta = fBackBlendLR ? pImage->Width() : 1;
   for (dwLine = 0; dwLine < dwLines; dwLine++) {
      pip = pImage->Pixel (fBackBlendLR ? dwLine : 0, fBackBlendLR ? 0 : dwLine);

      fp fDelta = (fp)dwLine / (fp)dwLines;
      for (i = 0; i < 3; i++)
         awLine[i] = (WORD)((fp)awBack[0][i] * (1.0 - fDelta) + (fp)awBack[1][i]*fDelta);

      for (dwPix = 0; dwPix < dwAcross; dwPix++, pip += dwDelta) {
         if (pip->dwID)
            continue;   // has an object

         // copy over background
         memcpy (&pip->wRed, awLine, sizeof(awLine));

         if (fNonBlackToObject) {
            if (UnGamma(&pip->wRed) == LAYEREDTRANSPARENTCOLOR)
               pip->dwID = 0; // black
            else
               pip->dwID = 1; // so have something
         }

      }
   } // dwLine
}


/*************************************************************************************
HotSpotColor - Given an index return a color.

inputs
   DWORD       dwIndex
returns
   COLORREF - color
*/
DLLEXPORT COLORREF HotSpotColor (DWORD dwIndex)
{
   COLORREF ac[] = {
      RGB(0xff,0,0), RGB(0,0xff,0), RGB(0,0,0xff),
      RGB(0xff,0,0xff), RGB(0xff, 0xff,0), RGB(0, 0xff, 0xff),
      RGB(0x80,0,0), RGB(0,0x80,0), RGB(0,0,0x80),
      RGB(0x80,0,0x80), RGB(0x80, 0x80,0), RGB(0, 0x80, 0x80)};

   dwIndex = dwIndex % (sizeof(ac) / sizeof(COLORREF));
   return ac[dwIndex];
}



/*****************************************************************************
RenderSceneTabs - This code displays the tabs used for the RSTABS macro.

inputs
   DWORD          dwTab - Tab ID that's currently selected
   DWORD          dwNum - Number of tabs
   PWSTR          *ppsz - Pointer to an array of dwNum tabs
   PWSTR          *ppszHelp - Pointer to an arrya of dwNum help entries
   DWORD          *padwID - Array of dwNum IDs for each tab
   DWORD          dwSkipNum - Number of IDs stored in padwSkip
   DWORD          *padwSkip - If a tab is this number then it's skipped
returns
   PWSTR - gMemTemp.p with text
*/
PWSTR RenderSceneTabs (DWORD dwTab, DWORD dwNum, PWSTR *ppsz, PWSTR *ppszHelp, DWORD *padwID,
                       DWORD dwSkipNum, DWORD *padwSkip)
{
   MemZero (&gMemTemp); // BUGFIX - Didn't have this
   MemCat (&gMemTemp, L"<tr>");

   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      for (j = 0; j < dwSkipNum; j++)
         if (padwID[i] == padwSkip[j])
            break;   // skip this
      if (j < dwSkipNum)
         continue;

      if (!ppsz[i]) {
         MemCat (&gMemTemp, L"<td/>");
         continue;
      }

      MemCat (&gMemTemp, L"<td align=center");
      if (padwID[i] != dwTab)
         MemCat (&gMemTemp, L" bgcolor=#8080a0");
      MemCat (&gMemTemp,
         L">"
         L"<a href=tabpress:");
      MemCat (&gMemTemp, (int)padwID[i]);
      MemCat (&gMemTemp, L">"
         L"<bold>");
      MemCatSanitize (&gMemTemp, ppsz[i]);
      MemCat (&gMemTemp, L"</bold>"
         L"<xHoverHelp>");
      MemCatSanitize (&gMemTemp, ppszHelp[i]);
      MemCat (&gMemTemp,
         L"</xHoverHelp>"
         L"</a>"
         L"</td>");
   } // i

   MemCat (&gMemTemp, L"</tr>");
   return (PWSTR)gMemTemp.p;
}


/*************************************************************************************
RenderSceneAspectToPixelsInt - Determines with width and height that an image will
be given it's aspect ration setting, m_iAspect.

inputs
   DWORD             iAspect - Aspect setting.0=2:1, 1=16:9, 2=3:2, 3=1:1,
                                4=2:3, 5=9:16, 6=1:2, 10=360 degreee

                             Also, negative numbers = aspectRatio * 1000. Thus, -2000 is 2:1

   fp                fScale - 1.0 = normal scale, 2.0 = 2x res (4x pixels), etc.
                        If this is SCALEPREVIEW (0), then will create a small size
   DWORD             *pdwWidth - Filled with the width, in pixels
   DWORD             *pdwHeight - Filled with the height, in pixels
returns
   none
*/
void RenderSceneAspectToPixelsInt (int iAspect, fp fScale, DWORD *pdwWidth, DWORD *pdwHeight)
{
   if (fScale <= 0)
      fScale = ((iAspect == 10) ? 0.25 : 0.5) * 1024.0 / RSDEFAULTRES;

   fScale *= RSDEFAULTRES;   // a default sized image will be 1024 in the longest dimension
   fp fOrigPixels = fScale * fScale;
   fp fWidth = fScale, fHeight = fScale;

   if (iAspect < 0)
      fWidth *= (fp)(-iAspect) / 1000.0;
   else switch (iAspect) {
      case 0:  // 2:1
         fHeight /= 2.0;
         break;
      case 1:  // 16:9
         fHeight /= (16.0 / 9.0);
         break;
      default:
      case 2:  // 3:2
         fHeight /= (3.0 / 2.0);
         break;
      case 3:  // 1:1
         break;
      case 4:  // 2:3
         fWidth /= (3.0 / 2.0);
         break;
      case 5:  // 9:16
         fWidth /= (16.0 / 9.0);
         break;
      case 6:  // 1:2
         fWidth /= 2.0;
         break;
      case 10: // 360 degree
         // make sure multiples of 8 and 16
         fHeight = floor(fHeight/2.0);
         fHeight = max(fHeight,1)*4;
         fWidth = fHeight * 2;
         break;
   }
   if (iAspect != 10) {
      fp fNewPixels = fWidth * fHeight;
      fScale = sqrt(fOrigPixels / fNewPixels);
      fWidth *= fScale;
      fHeight *= fScale;
   }

   fHeight = max(fHeight, 1);
   fWidth = max(fWidth, 1);

   *pdwWidth = (DWORD)fWidth;
   *pdwHeight = (DWORD)fHeight;
}


/*****************************************************************************
RenderSceneHotSpotToImage - This takes a hot-spot location for a scene, which
is normalized to a 1000 x 1000 grid, and converts it to the pixels in the image.
The conversion is in place.

inputs
   RECT        *pr - Initially contains 1000x1000 coords, but converted to image
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
returns
   none
*/
DLLEXPORT void RenderSceneHotSpotToImage (RECT *pr, DWORD dwWidth, DWORD dwHeight)
{
   pr->left = (pr->left * (int)dwWidth / 1000);
   pr->right = (pr->right * (int)dwWidth / 1000);
   pr->top = (pr->top * (int)dwHeight / 1000);
   pr->bottom = (pr->bottom * (int)dwHeight / 1000);
}


/*****************************************************************************
RenderSceneHotSpotFromImage - This takes an image location and converts
it to a normalize hot-spot location, which
is normalized to a 1000 x 1000 grid.
The conversion is in place.

inputs
   RECT        *pr - Initially contains image coords, converted to hotspot 1000x1000 coords
   DWORD       dwWidth - Width of image
   DWORD       dwHeight - Height of image
returns
   none
*/
DLLEXPORT void RenderSceneHotSpotFromImage (RECT *pr, DWORD dwWidth, DWORD dwHeight)
{
   pr->left = pr->left * 1000 / (int)dwWidth;
   pr->right = pr->right * 1000 / (int)dwWidth;
   pr->top = pr->top * 1000 / (int)dwHeight;
   pr->bottom = pr->bottom * 1000 / (int)dwHeight;
}

/*************************************************************************************
CRenderScene::Constructor and destructor
*/
CRenderScene::CRenderScene (void)
{
   m_szFile[0] = 0;
   m_iAspect = 2;
   m_fShadowsLimit = SHADOWSLIMITDEFAULT;   // default
   m_dwShadowsFlags = 0;
   m_dwQuality = 5;
   m_dwAnti = 1;
   m_lEffect.Init (sizeof(GUID)*2);
   m_pRender = NULL;
   m_dwTab = 0;
   m_lPCCircumrealityHotSpot.Init (sizeof(PCCircumrealityHotSpot));
   m_lRSATTRIB.Init (sizeof(RSATTRIB));
   m_lRSOBJECT.Init (sizeof(RSOBJECT));
   m_lRSCOLOR.Init (sizeof(RSCOLOR));
   m_iVScroll = 0;
   m_gScene = GUID_NULL;
   m_szSceneBookmark[0] = 0;
   m_fSceneTime = 0;

   m_gCamera = GUID_NULL;
   m_fCameraOverride = FALSE;
   m_pCameraXYZ.Zero();
   m_pCameraXYZ.p[1] = -10;   // start out 10m to south, for !m_fLoadFromFile
   m_pCameraRot.Zero();
   m_pCameraAutoHeight.Zero();
   m_pCameraAutoHeight.p[0] = 0;
   m_pCameraAutoHeight.p[1] = 1.8;
   m_pCameraAutoHeight.p[2] = .2;
   m_pCameraAutoHeight.p[3] = 0;
   m_fCameraFOV = PI/2;
   m_fCameraExposure = 1;
   m_fCameraHeightAdjust = 0;
   m_pTime.p[0] = m_pTime.p[1] = m_pTime.p[2] = m_pTime.p[3] = -1;
   m_fSkyDel = FALSE;
   m_pClouds.p[0] = m_pClouds.p[1] = m_pClouds.p[2] = m_pClouds.p[3] = -1;
   m_pLight.p[0] = m_pLight.p[1] = m_pLight.p[2] = 0;
   m_pLight.p[3] = 1;   // 1/2m up and down
   m_fLightsOn = 0;
   m_fMoveDist = 1;  // 1 meter move default

   m_acBackColor[0] = RGB (0,0,0); // RGB(0x60,0x70,0xff);
   m_acBackColor[1] = RGB (0,0,0); // RGB(0xc0,0xd0,0xe0);
   m_fBackBlendLR = FALSE;
   m_szBackFile[0] = 0;
   m_pBackRend = NULL;
   m_dwBackMode = 0; // color blend

   m_lBoneList.Init (sizeof(PCListVariable));

   DWORD i;
   for (i = 0; i < RSLIGHTS; i++) {
      m_apLightDir[i].Zero();
      m_apLightDir[i].p[0] = -PI*3/4;
      m_apLightDir[i].p[1] = PI/3;

      if (i == 0)
         m_apLightDir[i].p[2] =.75; // keylight
      else if (i == RSLIGHTS-1)
         m_apLightDir[i].p[2] = .25;   // diffuse

      if (i == RSLIGHTS-1)
         m_acLightColor[i] = RGB(0xe0,0xe0,0xff);
      else
         m_acLightColor[i] = RGB(0xff,0xff,0xff);
   }

   m_fLoadFromFile = TRUE; // may be modified by caller
      // Usually set based on whether matches Circumreality3DScene or Circumreality3DObjects
}

CRenderScene::~CRenderScene (void)
{
   DWORD i;
   PCListVariable *pplv = (PCListVariable*)m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++)
      delete pplv[i];
   m_lBoneList.Clear();

   if (m_pRender)
      delete m_pRender;

   if (m_pBackRend)
      delete m_pBackRend;
   m_pBackRend = NULL;

   // free up hotspots
   PCCircumrealityHotSpot *pphs = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   for (i = 0; i < m_lPCCircumrealityHotSpot.Num(); i++, pphs++)
      delete *pphs;
   m_lPCCircumrealityHotSpot.Clear();

   // free up the colors
   PRSCOLOR prc = (PRSCOLOR)m_lRSCOLOR.Get(0);
   for (i = 0; i < m_lRSCOLOR.Num(); i++, prc++)
      delete prc->pSurf;
   m_lRSCOLOR.Clear();
}


/*************************************************************************************
CRenderScene::MMLTo - Standard API
*/
static PWSTR gpszFile = L"File";
static PWSTR gpszAspect = L"Aspect";
static PWSTR gpszQuality = L"Quality";
static PWSTR gpszAnti = L"Anti";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszScene = L"Scene";
static PWSTR gpszSceneTime = L"SceneTime";
static PWSTR gpszSceneBookmark = L"SceneBookmark";
static PWSTR gpszCameraOverride = L"CameraOverride";
static PWSTR gpszCamera = L"Camera";
static PWSTR gpszCameraXYZ = L"CameraXYZ";
static PWSTR gpszCameraRot = L"CameraRot";
static PWSTR gpszCameraFOV = L"CameraFOV";
static PWSTR gpszCameraExposure = L"CameraExposure";
static PWSTR gpszCameraHeightAdjust = L"CameraHeightAdjust";
static PWSTR gpszObject = L"Object";
static PWSTR gpszAttrib = L"Attrib";
static PWSTR gpszValue = L"Value";
// static PWSTR gpszObject = L"Object";
static PWSTR gpszMajor = L"Major";
static PWSTR gpszMinor = L"Minor";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszRot = L"Rot";
static PWSTR gpszColor = L"Color";
static PWSTR gpszBackFile = L"BackFile";
static PWSTR gpszBackColor0 = L"BackColor0";
static PWSTR gpszBackColor1 = L"BackColor1";
static PWSTR gpszBackBlendLR = L"BackBlendLR";
static PWSTR gpszEffect = L"Effect";
static PWSTR gpszCode = L"Code";
static PWSTR gpszSub = L"Sub";
static PWSTR gpszHotSpot = L"HotSpot";
static PWSTR gpszLight = L"Light";
static PWSTR gpszLightsOn = L"LightsOn";
static PWSTR gpszSkyDel = L"SkyDel";
static PWSTR gpszTime = L"Time";
static PWSTR gpszClouds = L"Clouds";
static PWSTR gpszName = L"Name";
static PWSTR gpszAttachTo = L"AttachTo";
static PWSTR gpszAttachBone = L"AttachBone";
static PWSTR gpszCameraAutoHeight = L"CameraAutoHeight";
static PWSTR gpszShadowsLimit = L"ShadowsLimit";

PCMMLNode2 CRenderScene::MMLTo (void)
{
   DWORD i;
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (m_fLoadFromFile ? Circumreality3DScene() : Circumreality3DObjects());

   // NOTE: Not saving m_fLoadFromFile

   if (m_fLoadFromFile && !IsEqualGUID (m_gCamera, GUID_NULL))
      MMLValueSet (pNode, gpszCamera, (PBYTE)&m_gCamera, sizeof(m_gCamera));
   if (!m_fLoadFromFile || m_fCameraOverride) {
      MMLValueSet (pNode, gpszCameraOverride, (int)m_fCameraOverride);

      // BUGFIX - round off so not as much text space
      DWORD j;
      for (j = 0; j < 3; j++) {
         m_pCameraXYZ.p[j] = floor(m_pCameraXYZ.p[j]*100.0 + 0.5)/100.0;
         m_pCameraRot.p[j] = floor(m_pCameraRot.p[j]*100.0 + 0.5)/100.0;
            // BUGFIX - made more accurate rotation, and better rounding
      }

      MMLValueSet (pNode, gpszCameraXYZ, &m_pCameraXYZ);
      MMLValueSet (pNode, gpszCameraRot, &m_pCameraRot);
      MMLValueSet (pNode, gpszCameraFOV, (fp)(floor(m_fCameraFOV*100.0 + 0.5)/100.0));   // round so not too large
      MMLValueSet (pNode, gpszCameraExposure, m_fCameraExposure);
//            {
//            CHAR szTemp[64];
//            sprintf (szTemp, "Exposure MMLTo=%g", (double)m_fCameraExposure);
//            MessageBox (NULL, "Exposure", szTemp, MB_OK);
//            }
   }

   if (m_pCameraAutoHeight.p[3])
      MMLValueSet (pNode, gpszCameraAutoHeight, &m_pCameraAutoHeight);

   if (m_fCameraHeightAdjust)
      MMLValueSet (pNode, gpszCameraHeightAdjust, m_fCameraHeightAdjust);

   if ((m_pTime.p[0] >= 0) || (m_pTime.p[1] >= 0) || (m_pTime.p[2] >= 0) || (m_pTime.p[3] >= 0))
      MMLValueSet (pNode, gpszTime, &m_pTime);
   if ((m_pClouds.p[0] >= 0) || (m_pClouds.p[1] >= 0) || (m_pClouds.p[2] >= 0) || (m_pClouds.p[3] >= 0))
      MMLValueSet (pNode, gpszClouds, &m_pClouds);
   if (m_fSkyDel)
      MMLValueSet (pNode, gpszSkyDel, 1);
   if ((m_pLight.p[0] > 0) || (m_pLight.p[1] > 0) || (m_pLight.p[2] > 0))
      MMLValueSet (pNode, gpszLight, &m_pLight);
   if (m_fLightsOn)
      MMLValueSet (pNode, gpszLightsOn, m_fLightsOn);

   if (m_fLoadFromFile && m_szFile[0])
      MMLValueSet (pNode, gpszFile, m_szFile);
   if (m_iAspect != 2)
      MMLValueSet (pNode ,gpszAspect, (int)m_iAspect);
   if (m_fShadowsLimit != SHADOWSLIMITDEFAULT)
      MMLValueSet (pNode, gpszShadowsLimit, m_fShadowsLimit);
   if (m_dwQuality != 5)
      MMLValueSet (pNode, gpszQuality, (int)m_dwQuality);
   if (m_dwAnti != 1)
      MMLValueSet (pNode, gpszAnti, (int)m_dwAnti);

   GUID *pgEffect = (GUID*)m_lEffect.Get(0);
   for (i = 0; i < m_lEffect.Num(); i++, pgEffect += 2) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszEffect);
      MMLValueSet (pSub, gpszCode, (PBYTE)&pgEffect[0], sizeof(pgEffect[0]));
      MMLValueSet (pSub, gpszSub, (PBYTE)&pgEffect[1], sizeof(pgEffect[1]));
   }

   // save lights
   WCHAR szTemp[64];
   if (!m_fLoadFromFile) {
      for (i = 0; i < RSLIGHTS; i++) {
         // BUGFIX - If not bright then dont bother
         if (!m_apLightDir[i].p[2])
            continue;

         // BUGFIX - round off light direction
         DWORD j;
         for (j = 0; j < 3; j++)
            m_apLightDir[i].p[j] = floor(m_apLightDir[i].p[j] * 100.0 + 0.5) / 100.0;

         swprintf (szTemp, L"lightdir%d", (int)i);
         MMLValueSet (pNode, szTemp, &m_apLightDir[i]);

         // BUGFIX - Dont save light color if not default
         swprintf (szTemp, L"lightcolor%d", (int)i);
         if (m_acLightColor[i] != ((i == RSLIGHTS-1) ? RGB(0xe0,0xe0,0xff) : RGB(0xff,0xff,0xff)))
            MMLValueSet (pNode, szTemp, (int)m_acLightColor[i]);
      }// i

      if ((m_dwBackMode == 2) && m_pBackRend) {
         PCMMLNode2 pSub = m_pBackRend->MMLTo ();
         pNode->ContentAdd (pSub);
      }
      else if ((m_dwBackMode == 1) && m_szBackFile[0])
         MMLValueSet (pNode, gpszBackFile, m_szBackFile);
      
      // NOTE: always save background color
      // BUGFIX - Only save background if not black
      if (m_acBackColor[0] != RGB(0,0,0))
         MMLValueSet (pNode, gpszBackColor0, (int)m_acBackColor[0]);
      if (m_acBackColor[1] != RGB(0,0,0))
         MMLValueSet (pNode, gpszBackColor1, (int)m_acBackColor[1]);
      if (m_fBackBlendLR)
         MMLValueSet (pNode, gpszBackBlendLR, (int)m_fBackBlendLR);
   } // not load from file

   if (!IsEqualGUID (m_gScene, GUID_NULL))
      MMLValueSet (pNode, gpszScene, (PBYTE)&m_gScene, sizeof(m_gScene));
   if (m_fSceneTime)
      MMLValueSet (pNode, gpszSceneTime, m_fSceneTime);
   if (m_szSceneBookmark[0])
      MMLValueSet (pNode, gpszSceneBookmark, m_szSceneBookmark);

   // write out the hot spots
   PCCircumrealityHotSpot *pph;
   pph = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   for (i = 0; i < m_lPCCircumrealityHotSpot.Num(); i++, pph++) {
      (*pph)->m_lid = m_lid;
      PCMMLNode2 pSub = (*pph)->MMLTo ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszHotSpot);
      pNode->ContentAdd (pSub);
   } // i

   // write out the attributes
   PRSATTRIB pr = (PRSATTRIB)m_lRSATTRIB.Get(0);
   for (i = 0; i < m_lRSATTRIB.Num(); i++, pr++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszAttrib);

      if (m_fLoadFromFile)
         MMLValueSet (pSub, gpszObject, (LPBYTE)&pr->gObject, sizeof(pr->gObject));
      else
         MMLValueSet (pSub, gpszObject, (int)pr->dwObject);

      MMLValueSet (pSub, gpszValue, pr->fValue);
      if (pr->szAttrib[0])
         MMLValueSet (pSub, gpszAttrib, pr->szAttrib);
      if (pr->szName[0])
         MMLValueSet (pSub, gpszName, pr->szName);
   } // i

   // write out the objects
   PRSOBJECT po = (PRSOBJECT)m_lRSOBJECT.Get(0);
   for (i = 0; i < m_lRSOBJECT.Num(); i++, po++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszObject);

      MMLValueSet (pSub, gpszMajor, (LPBYTE)&po->gMajor, sizeof(po->gMajor));
      MMLValueSet (pSub, gpszMinor, (LPBYTE)&po->gMinor, sizeof(po->gMinor));
      po->pLoc.p[3] = po->pRot.p[3] = 1;  // so doesnt write extra bits

      // BUGFIX - round off so not as much text space
      DWORD j;
      for (j = 0; j < 3; j++) {
         po->pLoc.p[j] = floor(po->pLoc.p[j]*1000.0 + 0.5)/1000.0;   // BUGFIX - Used to be 1 cm accuracy
         po->pRot.p[j] = floor(po->pRot.p[j]*100.0 + 0.5)/100.0;
      }


      // BUGFIX - dont write loc and rot unless have to
      if (po->pLoc.Length())
         MMLValueSet (pSub, gpszLoc, &po->pLoc);
      if (po->pRot.Length())
         MMLValueSet (pSub, gpszRot, &po->pRot);

      // attach to
      if ((po->dwAttachTo != (DWORD)-1) && (po->dwAttachTo < m_lRSOBJECT.Num())) {
         MMLValueSet (pSub, gpszAttachTo, (int)po->dwAttachTo);
         if (po->szAttachBone[0])
            MMLValueSet (pSub, gpszAttachBone, po->szAttachBone);
      }
   } // i

   // write out the objects
   PRSCOLOR prc = (PRSCOLOR)m_lRSCOLOR.Get(0);
   for (i = 0; i < m_lRSCOLOR.Num(); i++, prc++) {
      PCMMLNode2 pSub = prc->pSurf->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszColor);

      if (m_fLoadFromFile)
         MMLValueSet (pSub, gpszObject, (LPBYTE)&prc->gObject, sizeof(prc->gObject));
      else
         MMLValueSet (pSub, gpszObject, (int)prc->dwObject);

      pNode->ContentAdd (pSub);
   } // i

   // write out transition node
   PCMMLNode2 pSub = m_Transition.MMLTo ();
   if (pSub)
      pNode->ContentAdd (pSub);

   return pNode;
}



/*************************************************************************************
CRenderScene:MMLFrom - Standard API
*/
BOOL CRenderScene::MMLFrom (PCMMLNode2 pNode)
{
   // NOTE: m_fLoadFromFile NOT read in. should be set ahead of time

   // free up hotspots
   DWORD i;
   PCCircumrealityHotSpot *pphs = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   for (i = 0; i < m_lPCCircumrealityHotSpot.Num(); i++, pphs++)
      delete (*pphs);
   m_lPCCircumrealityHotSpot.Clear();

   // free up the colors
   PRSCOLOR prc = (PRSCOLOR)m_lRSCOLOR.Get(0);
   for (i = 0; i < m_lRSCOLOR.Num(); i++, prc++)
      delete prc->pSurf;
   m_lRSCOLOR.Clear();

   if (m_pBackRend)
      delete m_pBackRend;
   m_pBackRend = NULL;

   PWSTR psz = MMLValueGet (pNode, gpszFile);
   if (psz && m_fLoadFromFile)
      wcscpy (m_szFile, psz);
   else
      m_szFile[0] = 0;

   m_iAspect = MMLValueGetInt (pNode ,gpszAspect, 2);
   m_fShadowsLimit = MMLValueGetDouble (pNode, gpszShadowsLimit, SHADOWSLIMITDEFAULT);
   m_dwQuality = (DWORD) MMLValueGetInt (pNode, gpszQuality, 5);
   m_dwAnti = (DWORD) MMLValueGetInt (pNode, gpszAnti, 1);

   // load lights
   WCHAR szTemp[64];
   PCMMLNode2 pSub;
   if (!m_fLoadFromFile) {
      for (i = 0; i < RSLIGHTS; i++) {
         swprintf (szTemp, L"lightdir%d", (int)i);
         MMLValueGetPoint (pNode, szTemp, &m_apLightDir[i]);

         swprintf (szTemp, L"lightcolor%d", (int)i);
         m_acLightColor[i] = ((i == RSLIGHTS-1) ? RGB(0xe0,0xe0,0xff) : RGB(0xff,0xff,0xff));
            // BUGFIX - default color
         m_acLightColor[i] = (COLORREF) MMLValueGetInt (pNode, szTemp, (int)m_acLightColor[i]);
      } // i


      psz = MMLValueGet (pNode, gpszBackFile);
      m_dwBackMode = 0;
      if (psz) {
         wcscpy (m_szBackFile, psz);
         m_dwBackMode = 1;
      }
      else
         m_szBackFile[0] = 0;

      m_acBackColor[0] = m_acBackColor[1] = RGB(0,0,0);
      m_acBackColor[0] = MMLValueGetInt (pNode, gpszBackColor0, (int)m_acBackColor[0]);
      m_acBackColor[1] = MMLValueGetInt (pNode, gpszBackColor1, (int)m_acBackColor[1]);
      m_fBackBlendLR = (BOOL) MMLValueGetInt (pNode, gpszBackBlendLR, FALSE);
   }


   m_gCamera = GUID_NULL;
   if (m_fLoadFromFile)
      MMLValueGetBinary (pNode, gpszCamera, (PBYTE)&m_gCamera, sizeof(m_gCamera));
   m_fCameraOverride = (BOOL) MMLValueGetInt (pNode, gpszCameraOverride, !m_fLoadFromFile);
   if (!m_fLoadFromFile || m_fCameraOverride) {
      MMLValueGetPoint (pNode, gpszCameraXYZ, &m_pCameraXYZ);
      MMLValueGetPoint (pNode, gpszCameraRot, &m_pCameraRot);
      m_fCameraFOV = MMLValueGetDouble (pNode, gpszCameraFOV, PI/2);
      m_fCameraExposure = MMLValueGetDouble (pNode, gpszCameraExposure, 1);
//            {
//            CHAR szTemp[64];
//            sprintf (szTemp, "Exposure MMLFrom=%g", (double)m_fCameraExposure);
//            MessageBox (NULL, "Exposure", szTemp, MB_OK);
//            }
   }

   m_pCameraAutoHeight.p[0] = 0;
   m_pCameraAutoHeight.p[1] = 1.8;
   m_pCameraAutoHeight.p[2] = 0.2;
   m_pCameraAutoHeight.p[3] = 0;
   MMLValueGetPoint (pNode, gpszCameraAutoHeight, &m_pCameraAutoHeight);
   m_fCameraHeightAdjust = MMLValueGetDouble (pNode, gpszCameraHeightAdjust, 0);

   m_pTime.p[0] = m_pTime.p[1] = m_pTime.p[2] = m_pTime.p[3] = -1;
   MMLValueGetPoint (pNode, gpszTime, &m_pTime);
   if (m_pTime.p[3] == 1)
      m_pTime.p[3] = -1;   // since will tend to make it 1
   m_fSkyDel = (BOOL) MMLValueGetInt (pNode, gpszSkyDel, FALSE);
   m_pLight.Zero();
   MMLValueGetPoint (pNode, gpszLight, &m_pLight);
   m_fLightsOn = MMLValueGetDouble (pNode, gpszLightsOn, 0);

   m_pClouds.p[0] = m_pClouds.p[1] = m_pClouds.p[2] = m_pClouds.p[3] = -1;
   MMLValueGetPoint (pNode, gpszClouds, &m_pClouds);
   if ((m_pClouds.p[3] == 1) && (m_pClouds.p[0] < 0) && (m_pClouds.p[1] < 0) && (m_pClouds.p[2] < 0))
      m_pClouds.p[3] = -1;   // since will tend to make it 1

   m_gScene = GUID_NULL;
   MMLValueGetBinary (pNode, gpszScene, (PBYTE) &m_gScene, sizeof(m_gScene));
   m_fSceneTime = MMLValueGetDouble (pNode, gpszSceneTime, 0);
   psz = MMLValueGet (pNode, gpszSceneBookmark);
   if (psz && (wcslen(psz)+1 < sizeof(m_szSceneBookmark)/sizeof(WCHAR)))
      wcscpy (m_szSceneBookmark, psz);
   else
      m_szSceneBookmark[0] = 0;

   // fill in the hot spots
   m_lEffect.Clear();
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszHotSpot)) {
         PCCircumrealityHotSpot pNew = new CCircumrealityHotSpot;
         if (!pNew)
            continue;
         pNew->MMLFrom (pSub, m_lid);
         m_lid = pNew->m_lid; // in case different

         // add it
         m_lPCCircumrealityHotSpot.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszAttrib)) {
         // new hot spot
         RSATTRIB rs;
         memset (&rs, 0, sizeof(rs));
         psz = MMLValueGet (pSub, gpszAttrib);
         if (psz)
            wcscpy (rs.szAttrib, psz);

         psz = MMLValueGet (pSub, gpszName);
         if (psz && (wcslen(psz) < sizeof(rs.szName)/sizeof(WCHAR)-1) )
            wcscpy (rs.szName, psz);

         rs.fValue = MMLValueGetDouble (pSub, gpszValue, 0);

         if (m_fLoadFromFile)
            MMLValueGetBinary (pSub, gpszObject, (LPBYTE)&rs.gObject, sizeof(rs.gObject));
         else
            rs.dwObject = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);

         // add it
         m_lRSATTRIB.Add (&rs);
         continue;
      }
      else if (!_wcsicmp(psz, gpszColor)) {
         // new hot spot
         RSCOLOR rc;
         memset (&rc, 0, sizeof(rc));
         rc.pSurf = new CObjectSurface;
         if (!rc.pSurf)
            continue;
         rc.pSurf->MMLFrom (pSub);

         if (m_fLoadFromFile)
            MMLValueGetBinary (pSub, gpszObject, (LPBYTE)&rc.gObject, sizeof(rc.gObject));
         else
            rc.dwObject = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);

         // add it
         m_lRSCOLOR.Add (&rc);
         continue;
      }
      else if (!_wcsicmp(psz, gpszObject)) {
         // new object
         RSOBJECT ro;
         memset (&ro, 0, sizeof(ro));

         MMLValueGetBinary (pSub, gpszMajor, (LPBYTE)&ro.gMajor, sizeof(ro.gMajor));
         MMLValueGetBinary (pSub, gpszMinor, (LPBYTE)&ro.gMinor, sizeof(ro.gMinor));
         MMLValueGetPoint (pSub, gpszLoc, &ro.pLoc);
         MMLValueGetPoint (pSub, gpszRot, &ro.pRot);

         ro.dwAttachTo = (DWORD) MMLValueGetInt (pSub, gpszAttachTo, -1);
         psz = MMLValueGet (pSub, gpszAttachBone);
         if (psz && (wcslen(psz)+1 < sizeof(ro.szAttachBone)/sizeof(WCHAR)))
            wcscpy (ro.szAttachBone, psz);

         // add it
         m_lRSOBJECT.Add (&ro);
         continue;
      }
      else if (!_wcsicmp(psz, Circumreality3DScene())) {
         if (m_pBackRend)
            delete m_pBackRend;
         m_pBackRend = new CRenderScene;
         if (!m_pBackRend)
            continue;
         m_pBackRend->m_fLoadFromFile = TRUE;
         m_pBackRend->MMLFrom (pSub);

         m_dwBackMode = 2;
         continue;
      }
      else if (!_wcsicmp(psz, gpszEffect)) {
         GUID agEffect[2];
         memset (agEffect, 0, sizeof(agEffect));
         MMLValueGetBinary (pSub, gpszCode, (PBYTE) &agEffect[0], sizeof(agEffect[0]));
         MMLValueGetBinary (pSub, gpszSub, (PBYTE) &agEffect[1], sizeof(agEffect[1]));
         m_lEffect.Add (&agEffect[0]);
         continue;
      }
   } // i

   // read in the transition
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind(CircumrealityTransition()), &psz, &pSub);
   m_Transition.MMLFrom (pSub);

   return TRUE;
}


   // ATTRIBADDLIST - For storing list of attributes for a specific object
   typedef struct {
      PRSATTRIB         pr;      // originall attriburte that was from
      PCObjectSocket    pos;     // object
      PCListFixed       plATTRIBVAL;   // list of attributes
   } ATTRIBADDLIST, *PATTRIBADDLIST;


/*************************************************************************************
CRenderScene::Render - Draws the image.

inputs
   fp          fScale - Amount to scale up/down from the standard image size.
                        1.0 = no change, 2.0 = 2x as large (4x the pixels), etc.
   BOOL        fFinalRender - If TRUE draw this as the final render, if FALSE
                        then use using a test render, which means one step lower quality
                        and no final-render flag sent for rendering
   BOOL        fForceReload - If TRUE then reload the scene even if it has already been loaded
   BOOL        fBlankIfFail - If the image fails to load, and this is TRUE, will create
                        a blank image instead. Otherwise, if fails will return null.
   DWORD       dwShadowsFlags - A set of SF_XXX flags from CRenderTraditional
   PCProgressSocket pProgres - Progress bar
   BOOL        fWantCImageStore - If TRUE then this returns a CImageStore, NOT a PCImage.
   PCMegaFile  pMegaFile - This is the megafile where to get the background 3d scenes from.
                        If NULL then not used.
   BOOL        *pf360 - Filled with TRUE if this is a 360 degree image, FALSE if ordinary camera
   PCRenderScene360Callback pCallback - If this is passed in, and a 360 degree image is
               rendered, then this callback will be used to get the latitude and longitude,
               as well as to indicate that refresh should be called.
   PCMem       pMem360Calc - If passed in, then the memory will be used to cache some
                  360-degree render calculations.
returns
   PCImage     pImage - Image. NOTE: If the file can't be loaded the image will be a blank one.
                        It should never return NULL (except out of memory)
*/
PCImage CRenderScene::Render (DWORD dwRenderShard, fp fScale, BOOL fFinalRender, BOOL fForceReload,
                              BOOL fBlankIfFail, DWORD dwShadowsFlags, PCProgressSocket pProgress,
                              BOOL fWantCImageStore,
                              PCMegaFile pMegaFile, BOOL *pf360,
                              PCRenderScene360Callback pCallback,
                              PCMem pMem360Calc)
{
//               {
//               CHAR szTemp[64];
//               sprintf (szTemp, "Exposure Render0=%g", (double)m_fCameraExposure);
//               MessageBox (NULL, "Exposure", szTemp, MB_OK);
//               }
   m_pProgress = pProgress;
   m_dwShadowsFlags = dwShadowsFlags;
   m_dwRenderShardTemp = dwRenderShard;

   PCImage pImage = NULL;
   PCImageStore pIStore = NULL;
   PCRenderRay apRay[MAX360PINGPONG];
   memset (apRay, 0, sizeof(apRay));
   PCRenderTraditional apTrad[MAX360PINGPONG];
   memset (apTrad, 0, sizeof(apTrad));
   PCRender360 p360 = NULL;
   PCImage pImageTemp = NULL;
   PCFImage pFImageTemp = NULL;
   PCImageStore pIS = NULL;
   CListFixed lATTRIBADDLIST; // list of attributes to add, in buld
   CListFixed lObjects;
   DWORD i;
   BOOL fExtraPush = FALSE;
   BOOL fExtraPush2 = FALSE;
   BOOL fDoEffect = fFinalRender && m_lEffect.Num();

   if (pf360)
      *pf360 = (m_iAspect == 10);

   if (m_pProgress && m_pProgress->WantToCancel())
      goto blankimage;

   // if have jpeg image then load that
   if (m_szBackFile[0]) {
      pIStore = new CImageStore;
      if (pIStore && !pIStore->Init (m_szBackFile, TRUE)) {
         delete pIStore;
         pIStore = NULL;
      }
   }
   else if (m_pBackRend) {
      // if there's a megafile see if can get from there...
      CMem mem;
      PCMMLNode2 pNode = NULL;
      BOOL fLoaded = FALSE;
      pIStore = new CImageStore;
      if (pMegaFile) {
         pNode = m_pBackRend->MMLTo();
         if (pNode && MMLToMem (pNode, &mem))
            mem.CharCat (0);
         else
            mem.m_dwCurPosn = 0; // error
      }
      if (pMegaFile && pIStore && pIStore->Init (pMegaFile, (PWSTR)mem.p))
         fLoaded = TRUE;

      // if we haven't loaded image then render one
      if (!fLoaded) {
         fExtraPush = TRUE;

         if (pProgress)
            pProgress->Push (0, 0.5);
         PCImage pRend = m_pBackRend->Render (dwRenderShard, fScale, fFinalRender, fForceReload,
            FALSE, m_dwShadowsFlags, this, FALSE, pMegaFile,
            NULL, NULL, pMem360Calc);
         if (pProgress) {
            pProgress->Pop();
            pProgress->Push (0.5, 1);
         }
         if (pRend) {
            pIStore->Init (pRend, TRUE, !(m_dwShadowsFlags & SF_NOSUPERSAMPLE), m_iAspect == 10);
            delete pRend;
         }
         else {
            delete pIStore;
            pIStore = NULL;
         }
      }


      // save the image to megafile?
      if (!fLoaded && pMegaFile && pIStore && mem.m_dwCurPosn) {
         pIStore->MMLSet (pNode);
         pIStore->ToMegaFile (pMegaFile, (PWSTR)mem.p);
      }

      // delete
      if (pNode)
         delete pNode;
   }

   //determine the real width and height
   DWORD dwWidth, dwHeight;
   RenderSceneAspectToPixelsInt (m_iAspect, fScale * m_Transition.Scale(), &dwWidth, &dwHeight);

   // determine the quality
   DWORD dwQuality = m_dwQuality;
   DWORD dwAnti = max(m_dwAnti, 1);

   // see if should reload
   GammaInit();
   PCSceneSet pSceneSet = NULL;
   PCWorldSocket pWorld = WorldGet (dwRenderShard, &pSceneSet);
   PWSTR psz = pWorld ? pWorld->NameGet () : NULL;
   if (m_fLoadFromFile && (!psz || !psz[0] || _wcsicmp(psz, m_szFile)))
      fForceReload = TRUE;
      // BUGFIX - Added paren before !psz and after m_szFIle)
   if (!m_pRender)
      fForceReload = TRUE;

   // free up old bone list
   PCListVariable *pplv = (PCListVariable*)m_lBoneList.Get(0);
   for (i = 0; i < m_lBoneList.Num(); i++)
      delete pplv[i];
   m_lBoneList.Clear();

   // reload?
   PCRenderTraditional pRTShadow = NULL;
   if (!m_fLoadFromFile) {
      // make sure there's a render object, since sometimes use
      if (!m_pRender)
         m_pRender = new CRenderTraditional(dwRenderShard); // so have something
      if (!m_pRender)
         goto blankimage;


      // priority increase is negative for subsequent render shards
      // BUGFIX - Not dependent on render shard
      // m_pRender->m_iPriorityIncrease = -(int)dwRenderShard;

      MyCacheM3DFileNew (dwRenderShard, &m_pRender, FALSE);
      pWorld = WorldGet (dwRenderShard, &pSceneSet);  // BUGFIX - reload
   }
   else if (fForceReload) {
      // loading from file

      if (m_pRender)
         delete m_pRender;
      m_pRender = NULL;

      if (pProgress)
         pProgress->Push (0, 0.2);

      BOOL fFailedToLoad;
      BOOL fRet;
      fRet = MyCacheM3DFileOpen (dwRenderShard, m_szFile, &fFailedToLoad, this, FALSE, &m_pRender, &pRTShadow);
      pWorld = WorldGet (dwRenderShard, &pSceneSet);  // BUGFIX - reload

      if (pProgress)
         pProgress->Pop();
      if (!fRet)
         goto blankimage;

      if (!m_pRender)
         m_pRender = new CRenderTraditional(dwRenderShard); // so have something
      if (!m_pRender)
         goto blankimage;
      
      // priority increase is negative for subsequent render shards
      // BUGFIX - Not dependent on render shard
      // m_pRender->m_iPriorityIncrease = -(int)dwRenderShard;

      if (pProgress)
         pProgress->Push (0.2, 1);
   }

   // set the scene
   if (m_fLoadFromFile && pSceneSet) {
      DWORD dwIndex = pSceneSet->SceneFind (&m_gScene);
      if (dwIndex == -1)
         dwIndex = 0;   // pick one
      PCScene pScene = pSceneSet->SceneGet (dwIndex);
      if (pScene) {
         // find the bookmark
         if (m_szSceneBookmark[0]) {
            CListFixed lBookmark;
            POSMBOOKMARK pb;
            lBookmark.Init (sizeof(OSMBOOKMARK));
            pScene->BookmarkEnum (&lBookmark, TRUE, TRUE);
            pb = (POSMBOOKMARK) lBookmark.Get(0);

            for (i = 0; i < lBookmark.Num(); i++, pb++)
               if (!_wcsicmp(pb->szName, m_szSceneBookmark)) {
                  m_fSceneTime = pb->fStart;
                  break;
               }
         }// if bookmark

         pSceneSet->StateSet (pScene, m_fSceneTime);
      }
      else {
         m_gScene = GUID_NULL;   // reset since not there anymore
         m_szSceneBookmark[0] = 0;
      }
   }


   // BUGFIX - moved object creation out of !m_fLoadFromFile so that can use
   // to create long-distance ground
   lObjects.Init (sizeof(PCObjectSocket));
   PRSOBJECT po = (PRSOBJECT)m_lRSOBJECT.Get(0);
   for (i = 0; i < m_lRSOBJECT.Num(); i++, po++) {
      PCObjectSocket pos = ObjectCFCreate (dwRenderShard, &po->gMajor, &po->gMinor);
      if (!pos)
         continue;

      // create the matrix
      CMatrix m;
      m.FromXYZLLT (&po->pLoc, po->pRot.p[2], po->pRot.p[0], po->pRot.p[1]);
      pos->ObjectMatrixSet (&m);

      // add it
      pWorld->ObjectAdd (pos, TRUE);

      // enumerate the attachment poitns
      PCListVariable plv = new CListVariable;
      if (!plv)
         continue;
      pos->AttachPointsEnum (plv);
      m_lBoneList.Add (&plv);

      // remember this object
      lObjects.Add (&pos);
   } // i

   // BUGBUG - automatically add large ground to world through pMapMountains,
   // but this causes problems with the large shadow buffer because it ends
   // up being not enough detail for the rest... is there a good work around?

#if 0 // def _DEBUG
   DWORD dwGround = 0;
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      PCObjectSocket pos = pWorld->ObjectGet(i);
      OSMGROUNDINFOGET og;
      if (pos->Message (OSM_GROUNDINFOGET, &og))
         dwGround++;
   } // i
#endif

   // do all the attachments now before set attributes
   po = (PRSOBJECT)m_lRSOBJECT.Get(0);
   PCObjectSocket *ppos = (PCObjectSocket*)lObjects.Get(0);
   for (i = 0; i < m_lRSOBJECT.Num(); i++, po++) {
      if ((po->dwAttachTo >= lObjects.Num()) || (po->dwAttachTo == i))
         continue;   // not attached

      // attach
      CMatrix m;
      GUID g;
      ppos[i]->ObjectMatrixGet (&m);
      ppos[i]->GUIDGet (&g);
      ppos[po->dwAttachTo]->AttachAdd (&g, po->szAttachBone, &m);
   } // i

   // BUGFIX - moved set the lights down
   // set the lights
   if (!m_fLoadFromFile) {
      // eventually add lights
      for (i = 0; i < RSLIGHTS; i++) {
         if (m_apLightDir[i].p[2] <= EPSILON)
            continue;   // not on

         OSINFO info;
         memset (&info, 0, sizeof(info));
         info.gCode = CLSID_LightGeneric;
         info.gSub = CLSID_LightGenericSub;
         info.dwRenderShard = dwRenderShard;
         PCObjectLightGeneric pos = new CObjectLightGeneric (NULL, &info);
         if (!pos)
            continue;

         pos->m_li.afLumens[0] = pos->m_li.afLumens[1] = 0;
         pos->m_li.afLumens[2] = m_apLightDir[i].p[2] * CM_LUMENSSUN;
         Gamma (m_acLightColor[i], pos->m_li.awColor[2]);
         pos->m_li.dwForm = (i == RSLIGHTS-1) ? LIFORM_AMBIENT : LIFORM_INFINITE;
         pos->m_li.fNoShadows = (i ? TRUE : FALSE);
         pos->ShowSet (FALSE);   // so user can't see

         // add to world
         pWorld->ObjectAdd (pos, TRUE);

         // rotate...
         CMatrix mAzimuth, mAlt;
         mAzimuth.RotationZ (-m_apLightDir[i].p[0]);
         mAlt.RotationX (m_apLightDir[i].p[1] - PI/2);
         mAzimuth.MultiplyLeft (&mAlt);
         pos->ObjectMatrixSet (&mAzimuth);
      } // i

      // progress
      if (pProgress)
         pProgress->Push (0, 1);
   }

   // set all the attributes
   PRSATTRIB pr = (PRSATTRIB) m_lRSATTRIB.Get(0);
   ATTRIBVAL av;
   memset (&av,0 ,sizeof(av));
   DWORD j;
   PATTRIBADDLIST paal;
   lATTRIBADDLIST.Init (sizeof(ATTRIBADDLIST));
   for (i = 0; i < m_lRSATTRIB.Num(); i++, pr++) {
      // see if it has already been found
      paal = (PATTRIBADDLIST)lATTRIBADDLIST.Get(0);
      PCObjectSocket pos = NULL;
      if (pr->szName[0])
         for (j = 0; j < lATTRIBADDLIST.Num(); j++, paal++) {
            if (paal->pr->szName[0] && !_wcsicmp(pr->szName, paal->pr->szName)) {
               pos = paal->pos;
               break;
            }
         } // j
      else if (m_fLoadFromFile)
         for (j = 0; j < lATTRIBADDLIST.Num(); j++, paal++) {
            if (!paal->pr->szName[0] && IsEqualGUID(pr->gObject, paal->pr->gObject)) {
               pos = paal->pos;
               break;
            }
         } // j
      else  // !m_fLoadFromFile
         for (j = 0; j < lATTRIBADDLIST.Num(); j++, paal++) {
            if (!paal->pr->szName[0] && (pr->dwObject == paal->pr->dwObject)) {
               pos = paal->pos;
               break;
            }
         } // j

      if (!pos) {
         // if found a name then search
         if (pr->szName[0]) {
            // find it by the name
            DWORD dwNum = pWorld->ObjectNum();
            PWSTR psz;
            for (j = 0; j < dwNum; j++) {
               pos = pWorld->ObjectGet(j);
               if (!pos)
                  continue;
               psz = pos->StringGet(OSSTRING_NAME);
               if (!psz)
                  continue;
               if (!_wcsicmp(psz, pr->szName))
                  break;
            } // j
            if (j >= dwNum)
               pos = NULL;
         }
         else {
            // if !m_fLoadFromFile then do something different
            if (m_fLoadFromFile)
               pos = pWorld->ObjectGet(pWorld->ObjectFind(&pr->gObject));
            else
               pos = pWorld->ObjectGet(pr->dwObject);
         }

         if (!pos)
            continue;

         // else, new entry
         ATTRIBADDLIST al;
         memset (&al, 0, sizeof(al));
         al.plATTRIBVAL = new CListFixed;
         if (!al.plATTRIBVAL)
            continue;
         al.plATTRIBVAL->Init (sizeof(ATTRIBVAL));
         al.pos = pos;
         al.pr = pr;
         lATTRIBADDLIST.Add (&al);
         paal = (PATTRIBADDLIST)lATTRIBADDLIST.Get(lATTRIBADDLIST.Num()-1);
      } // if !pos


      av.fValue = pr->fValue;
      wcscpy (av.szName, pr->szAttrib);
      paal->plATTRIBVAL->Add (&av);
   } // i

   // send and then free all the attributes to add
   paal = (PATTRIBADDLIST)lATTRIBADDLIST.Get(0);
   for (i = 0; i < lATTRIBADDLIST.Num(); i++, paal++) {
      paal->pos->AttribSetGroup (paal->plATTRIBVAL->Num(), (PATTRIBVAL) paal->plATTRIBVAL->Get(0));
      delete paal->plATTRIBVAL;
   }// i

   // set all the colors
   PRSCOLOR prc = (PRSCOLOR)m_lRSCOLOR.Get(0);
   for (i = 0; i < m_lRSCOLOR.Num(); i++, prc++) {
      // if !m_fLoadFromFile then do something different
      PCObjectSocket pos;
      if (m_fLoadFromFile)
         pos = pWorld->ObjectGet(prc->dwObject = pWorld->ObjectFind(&prc->gObject));
      else
         pos = pWorld->ObjectGet(prc->dwObject);

      if (!pos)
         continue;

      // BUGFIX - Since objecteditor objects won't know what surfaces the
      // support until they're rendered, do a PreRender() on them
      pos->PreRender ();

      // BUGFIX - Since objecteditor objects won't know what surfaces they
      // support until they're rendered, do a query bounding box on them
      // BUGFIX - This doesn't really do anything for the textures
      // anymore, but call just in case
      CMatrix m;
      CPoint c[2];
      pWorld->BoundingBoxGet (prc->dwObject, &m, &c[0], &c[1]);

      prc->pSurf->m_szScheme[0] = 0;   // so don't use schemes, because they dont work
      pos->SurfaceSet (prc->pSurf);
   } // i, rscolor

   // figure out where the camera is
   BOOL fUseOverride = !m_fLoadFromFile;
   SetRenderCamera (dwRenderShard, m_pRender);
   if (m_fCameraOverride || !IsEqualGUID(m_gCamera, GUID_NULL))
      fUseOverride = TRUE;

   // determine the average background color
   GammaInit();
   WORD awBack[2][3];
   COLORREF cBack;
   Gamma (m_acBackColor[0], awBack[0]);
   Gamma (m_acBackColor[1], awBack[1]);
   for (i = 0; i < 3; i++)
      awBack[0][i] = awBack[0][i] / 2 + awBack[1][i] / 2;
   cBack = UnGamma (awBack[0]);



   // BUGFIX - Moved skydome here

   // set the skydome time or delete
   if ((m_pTime.p[0] >= 0) || (m_pTime.p[1] >= 0) ||(m_pTime.p[2] >= 0) ||(m_pTime.p[3] >= 0) ||
      (m_pClouds.p[0] >= 0) || (m_pClouds.p[1] >= 0) ||(m_pClouds.p[2] >= 0) ||(m_pClouds.p[3] >= 0) || 
      m_fSkyDel) {
      DWORD dwNum = pWorld->ObjectNum();
      PCObjectSocket pos;
      for (i = 0; i < dwNum; i++) {
         pos = pWorld->ObjectGet (i);
         if (!pos)
            continue;

         if (pos->Message (OSM_SKYDOME, NULL))
            break;
      } // i

      if (i >= dwNum)
         goto noskydome;

      // if want to destroy then do so
      if (m_fSkyDel) {
         pWorld->ObjectRemove (i);
         goto noskydome;
      }

      // get the year, month, and day
      ATTRIBVAL aav[16];
      memset (&aav, 0, sizeof(aav));
      wcscpy (aav[0].szName, L"Time: Year");
      wcscpy (aav[1].szName, L"Time: Month");
      wcscpy (aav[2].szName, L"Time: Hour");
      for (i = 0; i < 3; i++)
         pos->AttribGet (aav[i].szName, &aav[i].fValue);

      DFTIME dfTime;
      DFDATE dfDate;
      TimeInYearDayToDFDATETIME (aav[2].fValue, aav[1].fValue, aav[0].fValue,
                                &dfTime, &dfDate);

      WORD wYear, wMonth, wDay;
      wYear = YEARFROMDFDATE (dfDate);
      wMonth = MONTHFROMDFDATE (dfDate);
      wDay = DAYFROMDFDATE (dfDate);
      if (m_pTime.p[0] >= 0)
         aav[2].fValue = m_pTime.p[0] * 24.0;
      if (m_pTime.p[1] >= 1)
         wDay = (WORD)m_pTime.p[1];
      if (m_pTime.p[2] >= 1)
         wMonth = (WORD)m_pTime.p[2];
      dfDate = TODFDATE (wDay, wMonth, wYear);
      aav[1].fValue = DFDATEToTimeInYear (dfDate);
      if (m_pTime.p[3] > 1)
         aav[0].fValue = floor(m_pTime.p[3]);   // year
      DWORD dw = 3;  // 3 attributes filled in so far

      // figure out version number to choose, for variations in cloud shapes
      // based on the cloud shapes, and the time
      fp fVersion = floor(
         m_pClouds.p[0]*10.0 + m_pClouds.p[1]*100.0 + m_pClouds.p[2]*1000.0 + m_pClouds.p[3]*10000.0 +
         m_pTime.p[0] * 10.0 + m_pTime.p[1]*100.0 + m_pTime.p[2]*1000.0);

      // do clouds
      DWORD j;
      for (j = 0; j < 3; j++) if (m_pClouds.p[j] >= 0) {
         PWSTR psz;
         switch (j) {
         case 0:
            psz = L"cumulus";
            break;
         case 1:
            psz = L"altocumulus";
            break;
         case 2:
            psz = L"cirrus";
            break;
         }

         swprintf (aav[dw].szName, L"Clouds, %s: Draw", psz);
         aav[dw].fValue = m_pClouds.p[j] ? 1 : 0;
         dw++;
         if (!m_pClouds.p[j])
            continue;   // as long as drawing off, ok

         swprintf (aav[dw].szName, L"Clouds, %s: Coverage", psz);
         aav[dw].fValue = m_pClouds.p[j];
         dw++;

         swprintf (aav[dw].szName, L"Clouds, %s: Thickness", psz);
         aav[dw].fValue = m_pClouds.p[j] / 2.0 + 0.25;
         dw++;

         swprintf (aav[dw].szName, L"Clouds, %s: Version", psz);
         aav[dw].fValue = fVersion + j;
         dw++;
      }
      if (m_pClouds.p[3] >= 0) {
         wcscpy (aav[dw].szName, L"Atmosphere: Density zenith");
         aav[dw].fValue = m_pClouds.p[3];
         dw++;
      }

      // set all these attributes
      pos->AttribSetGroup (dw, aav);

   } // if need skydome
noskydome:

   // BUGFIX - If this is loaded from a file AND it's night then force to NOT be low quality.
   // must be 360 image
   // BUGFIX - Was testing for 360 view, but even need for flat view so that can do transitions
   if (m_fLoadFromFile && (m_dwShadowsFlags & SF_TEXTURESONLY) /* && (m_iAspect == 10) */ ) {
      // liik for a bright infinite light
      DWORD dwNum = pWorld->ObjectNum();
      CListFixed lLight;
      PLIGHTINFO pli;
      PCObjectSocket pos;
      fp fLumensMin = CM_LUMENSSUN / 50;
      for (i = 0; i < dwNum; i++) {
         pos = pWorld->ObjectGet (i);
         if (!pos)
            continue;

         lLight.Init (sizeof(LIGHTINFO));
         pos->LightQuery (&lLight, RENDERSHOW_ALL);
         if (!lLight.Num())
            continue;   // not a light

         pli = (PLIGHTINFO)lLight.Get(0);
         for (j = 0; j < lLight.Num(); j++, pli++) {
            if (pli->dwForm != LIFORM_INFINITE)
               continue;   // only care about infinite

            if (pli->afLumens[2] >= fLumensMin)
               break;   // found a strong light
         } // j

         if (j < lLight.Num())
            break;   // found a strong light
      } // i

      // if didn't find any proper lights then can't use fast render
      if (i >= dwNum) {
         m_dwShadowsFlags &= ~(SF_TEXTURESONLY | SF_TWOPASS360);
         m_dwShadowsFlags |= SF_NOSHADOWS | SF_NOSPECULARITY | SF_LOWTRANSPARENCY | SF_NOBUMP; // so fast
            // not doing: SF_LOWDETAIL |
      }
   }

   // BUGFIX - If shadowflags indicates no shadows then limit quality
   if (m_dwShadowsFlags & SF_TEXTURESONLY)
      dwQuality = min(dwQuality, 4);   // dont allow to go above textures mode
   if (!fFinalRender) {
      if (dwQuality)
         dwQuality = min(dwQuality-1, 4); // dont allow quality to go too high
      dwAnti = 1;
   }

   // if "noshadows" or quality!=5, then ignore the cache
   // BUGFIX - If not best quality (doing a quick render first) then set to NULL
   if ((m_dwShadowsFlags & SF_NOSHADOWS) || (dwQuality != 5))
      pRTShadow = NULL;

   // initially create 360
   if (m_iAspect == 10) {
      p360 = new CRender360;
      if (!p360)
         goto blankimage;



      // priority increase is negative for subsequent render shards
      // p360->m_iPriorityIncrease = -(int)dwRenderShard;

      // set the cache memory for faster render
      if (pMem360Calc)
         p360->m_pMem360Calc = pMem360Calc;

   }

   // render
   DWORD dwPing;
   fp fShadowsLimit = m_fLoadFromFile ? m_fShadowsLimit : 0;
   int iDetail = TextureDetailGet(dwRenderShard);
   fShadowsLimit *= pow(2.0, (fp)iDetail/2.0);
   for (dwPing = 0; dwPing < (DWORD)(p360 ? MAX360PINGPONG : 1); dwPing++) {
      if (dwQuality >= 6) {
         apRay[dwPing] = new CRenderRay(dwRenderShard);
         if (!apRay[dwPing])
            goto blankimage;


         // priority increase is negative for subsequent render shards
         // BUGFIX - Not dependent on render shard
         // apRay[dwPing]->m_iPriorityIncrease = -(int)dwRenderShard;
      }
      else {
         apTrad[dwPing] = new CRenderTraditional(dwRenderShard);
         if (!apTrad[dwPing])
            goto blankimage;


         // priority increase is negative for subsequent render shards
         // BUGFIX - Not dependent on render shard
         // apTrad[dwPing]->m_iPriorityIncrease = -(int)dwRenderShard;

         if (m_pRender)
            m_pRender->CloneTo (apTrad[dwPing]);

         // set the shadows limit based on the texture quality
         apTrad[dwPing]->m_fShadowsLimit = fShadowsLimit;
      }
   } // dwPing

   // potentially create 360
   if (p360)  {  // 360
      PCRenderSuper paRS[MAX360PINGPONG];
      PCRenderTraditional paRT[MAX360PINGPONG];
      for (dwPing = 0; dwPing < MAX360PINGPONG; dwPing++) {
         paRS[dwPing] = apTrad[dwPing] ? (PCRenderSuper)apTrad[dwPing] : (PCRenderSuper)apRay[dwPing];
         paRT[dwPing] = apTrad[dwPing] ? apTrad[dwPing] : NULL;
      }

      if (!p360->Init (paRS, paRT, pRTShadow))
         goto blankimage;
      p360->m_fShadowsLimit = fShadowsLimit;
   }

   // abstract things out
   PCRenderSuper pSuper;
   BOOL fOnlyTrad = FALSE;
   if (p360)
      pSuper = p360;
   else if (apTrad[0]) {
      pSuper = apTrad[0];
      fOnlyTrad = TRUE;

      if (pRTShadow)
         pRTShadow->CloneLightsTo (apTrad[0]);
   }
   else
      pSuper = apRay[0];

   // see if can use the IS image
   BOOL fUseISImage = fWantCImageStore;
   if (apTrad[0] && (dwAnti != 1))   // cant antialias
      fUseISImage = FALSE;
   if (!p360)  // must have 360 degree image
      fUseISImage = FALSE;
   if (fDoEffect) // cant use with effect
      fUseISImage = FALSE;
   if (!m_fLoadFromFile) // cant blend with image
      fUseISImage = FALSE;

   if (fUseISImage) {
      pIS = new CImageStore;
      if (!pIS || !pIS->Init (dwWidth, dwHeight)) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }

      p360->CImageStoreSet (pIS);
   }
   else {
      // create the integer image where ultimately goes
      pImage = new CImage;
      if (!pImage) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }
      if (!pImage->Init (dwWidth * (apTrad[0] ? dwAnti : 1), dwHeight * (apTrad[0] ? dwAnti : 1))) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }

      // if we have an optional quality level, then do some checks
      DWORD dwQueryFloat = pSuper->ImageQueryFloat();
      if (dwQueryFloat == 1) {
         if (apTrad[0] && (dwQuality < 5))
            dwQueryFloat = 0; // if not shadows mode then dont bother

         // if no effects then dont bother
         if (!fDoEffect)
            dwQueryFloat = 0;
      }

      if (dwQueryFloat) {
         // ray tracing or something that wants float image
         pFImageTemp = new CFImage;
         if (!pFImageTemp)
            goto blankimage;

         if (!pFImageTemp->Init (dwWidth, dwHeight)) {
            if (pProgress) pProgress->Pop();
            goto blankimage;
         }

         pSuper->CFImageSet (pFImageTemp);
      }
      else
         pSuper->CImageSet (pImage);
   } // if not using CImageStore


   if (m_pProgress && m_pProgress->WantToCancel())
      goto blankimage;


   // settings
   pSuper->CWorldSet (pWorld);
   pSuper->RenderShowSet ( (DWORD)-1 & ~(RENDERSHOW_VIEWCAMERA | RENDERSHOW_ANIMCAMERA));
      // NOTE: apTrad was using RenderShowGet() & ...
   pSuper->BackgroundSet (m_fLoadFromFile ? m_pRender->BackgroundGet() : cBack);
   // move lower pSuper->ShadowsFlagsSet (m_dwShadowsFlags);

   for (dwPing = 0; dwPing < MAX360PINGPONG; dwPing++)
      if (apTrad[dwPing]) {
         // if using traditional render, some extras
         ::QualityApply (dwQuality, apTrad[dwPing]);
         apTrad[dwPing]->m_fFinalRender = fFinalRender;
         apTrad[dwPing]->LightVectorSetFromSun ();   // get vector from existing lights

         // BUGFIX - Dont do outlines, etc
         if (apTrad[dwPing]->m_pEffectFog)
            delete apTrad[dwPing]->m_pEffectFog;
         apTrad[dwPing]->m_pEffectFog = NULL;
         if (apTrad[dwPing]->m_pEffectOutline)
            delete apTrad[dwPing]->m_pEffectOutline;
         apTrad[dwPing]->m_pEffectOutline = NULL;
      }
      else if (apRay[dwPing]) {
         apRay[dwPing]->QuickSetRay (dwQuality - 6, 1, (dwAnti > 2) ? (dwAnti - 1) : 1);
         apRay[dwPing]->m_fAntiEdge = (dwAnti >= 2);
      }

   if (fUseOverride) {
      pSuper->ExposureSet (CM_LUMENSSUN / exp((double)m_fCameraExposure));
            // BUGFIX - Need (double) to get around bug in compiler
      pSuper->CameraPerspWalkthrough (&m_pCameraXYZ, m_pCameraRot.p[2], m_pCameraRot.p[0], m_pCameraRot.p[1], m_fCameraFOV);
//               {
//               fp fExp = pSuper->ExposureGet();
//               CHAR szTemp[64];
//               sprintf (szTemp, "Lumenssun=%g", (double)CM_LUMENSSUN);
//               MessageBox (NULL, "Exposure", szTemp, MB_OK);
//               sprintf (szTemp, "Exposure Orig-log=%g", (double)m_fCameraExposure);
//               MessageBox (NULL, "Exposure", szTemp, MB_OK);
//               sprintf (szTemp, "Exp(exposure)=%g", (double)exp((double)m_fCameraExposure));
//               MessageBox (NULL, "Exposure", szTemp, MB_OK);
//               sprintf (szTemp, "Exposure Orig=%g", (double)(CM_LUMENSSUN / exp((double)m_fCameraExposure)));
//               MessageBox (NULL, "Exposure", szTemp, MB_OK);
//               sprintf (szTemp, "Exposure Render1=%g", (double)fExp);
//               MessageBox (NULL, "Exposure", szTemp, MB_OK);
//               }
   }
   else if (!fOnlyTrad) {   // non-traditional render
      DWORD dwCamera;
      CPoint pCenter, pRot, pTrans;
      fp fScale;
      dwCamera = m_pRender->CameraModelGet ();
      switch (dwCamera) {
      case CAMERAMODEL_FLAT:
         m_pRender->CameraFlatGet (&pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale, &pTrans.p[0], &pTrans.p[1]);
         pSuper->CameraFlat (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1], fScale, pTrans.p[0], pTrans.p[1]);
         break;
      case CAMERAMODEL_PERSPWALKTHROUGH:
         m_pRender->CameraPerspWalkthroughGet (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
         pSuper->CameraPerspWalkthrough (&pTrans, pRot.p[2], pRot.p[0], pRot.p[1], fScale);
         break;
      case CAMERAMODEL_PERSPOBJECT:
         m_pRender->CameraPerspObjectGet (&pTrans, &pCenter,  &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
         pSuper->CameraPerspObject (&pTrans, &pCenter,  pRot.p[2], pRot.p[0], pRot.p[1], fScale);
         break;
      default:
         goto blankimage;  // error
      }

      pSuper->ExposureSet (m_pRender->ExposureGet());
      pSuper->AmbientExtraSet (m_pRender->AmbientExtraGet());
   }

   // adjust the camera height
   if (m_fCameraHeightAdjust || m_pCameraAutoHeight.p[3]) {
      // get the current location
      DWORD dwModel = m_pRender->CameraModelGet ();
      CPoint pCenter, pRot, pTrans;
      fp fScale;
      switch (dwModel) {
         case CAMERAMODEL_FLAT:
            pSuper->CameraFlatGet (&pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale, &pTrans.p[0], &pTrans.p[1]);
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            pSuper->CameraPerspWalkthroughGet (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
            break;
         case CAMERAMODEL_PERSPOBJECT:
            pSuper->CameraPerspObjectGet (&pTrans, &pCenter,  &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
            break;
      }

      // height to adjust
      fp fAdjust = m_fCameraHeightAdjust;

      // NOTE: This only works in walkthrough
      if (m_pCameraAutoHeight.p[3] && (dwModel == CAMERAMODEL_PERSPWALKTHROUGH)) {
         // loop through all the objects in the world
         DWORD dwNum = pWorld->ObjectNum();
         OSMFLOORLEVEL of, ofBest;
         memset (&of, 0, sizeof(of));
         of.pTest.Copy (&pTrans);
         of.iFloor = (int)floor(m_pCameraAutoHeight.p[0] + 0.5);;

         fp fBestArea = 0;
         for (i = 0; i < dwNum; i++) {
            PCObjectSocket pos = pWorld->ObjectGet(i);
            if (!pos || !pos->Message (OSM_FLOORLEVEL, &of))
               continue;   // not a floor
            
            // find the area
            fp fArea = of.fArea;
            if ((fBestArea <= 0) || (fArea < fBestArea)) {
               // found a new match, since this is the smallest object
               ofBest = of;
               fBestArea = fArea;
            }
         } // i

         // if found a best than use that
         if (fBestArea > 0) {
            if (ofBest.fIsWater)
               ofBest.fWater += m_pCameraAutoHeight.p[2];
            ofBest.fLevel += m_pCameraAutoHeight.p[1];
            if (ofBest.fIsWater && (ofBest.fWater > ofBest.fLevel))
               fAdjust = ofBest.fWater - pTrans.p[2];
            else
               fAdjust = ofBest.fLevel - pTrans.p[2];
         }
      }

      switch (dwModel) {
         case CAMERAMODEL_FLAT:
            pCenter.p[2] += fAdjust; // which may not do anything
            pSuper->CameraFlat (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1], fScale, pTrans.p[0], pTrans.p[1]);
            break;
         case CAMERAMODEL_PERSPWALKTHROUGH:
            pTrans.p[2] += fAdjust;
            pSuper->CameraPerspWalkthrough (&pTrans, pRot.p[2], pRot.p[0], pRot.p[1], fScale);
            break;
         case CAMERAMODEL_PERSPOBJECT:
            pTrans.p[2] -= fAdjust;  // since holding object away
            pSuper->CameraPerspObject (&pTrans, &pCenter,  pRot.p[2], pRot.p[0], pRot.p[1], fScale);
            break;
      }
   }


   // get the camera location
   DWORD dwModel = m_pRender->CameraModelGet ();
   CPoint pCenter, pRot, pTrans, pCamera;
   fp fScale;
   switch (dwModel) {
      case CAMERAMODEL_FLAT:
         pSuper->CameraFlatGet (&pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale, &pTrans.p[0], &pTrans.p[1]);
         pCamera.Copy (&pCenter);
         break;
      case CAMERAMODEL_PERSPWALKTHROUGH:
         pSuper->CameraPerspWalkthroughGet (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
         pCamera.Copy (&pTrans);
         break;
      case CAMERAMODEL_PERSPOBJECT:
         pSuper->CameraPerspObjectGet (&pTrans, &pCenter,  &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
         pCamera.Copy (&pTrans);
         pCamera.Scale (-1);
         break;
      default:
         pCamera.Zero();
         break;
   }

   // turn lights on within a certain distance
   if (m_fLightsOn) {
      DWORD dwNum = pWorld->ObjectNum();
      PCObjectSocket pos;
      CMatrix m;
      CPoint p;
      for (i = 0; i < dwNum; i++) {
         pos = pWorld->ObjectGet (i);
         if (!pos)
            continue;

         pos->ObjectMatrixGet (&m);
         p.Zero();
         p.p[3] = 1; // to make sure
         p.MultiplyLeft (&m);

         // distance from camera
         p.Subtract (&pCamera);
         if (p.Length() >= m_fLightsOn)
            continue;

         // try to turn on
         if (pos->TurnOnSet (1))
            continue;   // so have breakpoint to stop at
      } // i

   }

   // create light at camera location
   if ((m_pLight.p[0] > 0) || (m_pLight.p[1] > 0) || (m_pLight.p[2] > 0)) {
      OSINFO info;
      memset (&info, 0, sizeof(info));
      info.gCode = CLSID_LightGeneric;
      info.gSub = CLSID_LightGenericSub;
      info.dwRenderShard = dwRenderShard;
      PCObjectLightGeneric pos = new CObjectLightGeneric (NULL, &info);
      if (!pos)
         goto nolight;


      // find the max, in watts
      fp fMax = max(max(m_pLight.p[0], m_pLight.p[1]), m_pLight.p[2]);

      pos->m_li.dwForm = LIFORM_POINT;
      pos->m_li.fNoShadows = FALSE;
      pos->m_li.pLoc.Zero();
      pos->m_li.pDir.Zero();
      pos->m_li.pDir.p[0] = 1;
      pos->m_li.afLumens[0] = pos->m_li.afLumens[1] = 0;
      pos->m_li.afLumens[2] = fMax * CM_LUMENSPERINCANWATT;
      for (i = 0; i < 3; i++)
         pos->m_li.awColor[2][i] = (WORD) (m_pLight.p[i] / fMax * (fp)0xffff);
      pos->ShowSet (FALSE);   // so user can't see

      // add to world
      pWorld->ObjectAdd (pos, TRUE);

      // apply a random offset
      if (m_pLight.p[3] > 0) {
         srand (GetTickCount());
         for (i = 0; i < 3; i++)
            pCamera.p[i] += randf(-0.5, 0.5) * m_pLight.p[3];
      }
      CMatrix mTrans;
      mTrans.Translation (pCamera.p[0], pCamera.p[1], pCamera.p[2]);
      pos->ObjectMatrixSet (&mTrans);
   } // create light
nolight:

   // if doing a render effect then extra push
   if (pProgress && fDoEffect) {
      pProgress->Push (0, .8);
      fExtraPush2 = TRUE;
   }

   // set up some variables for RenderApplyEffects in case it's called part way through
   m_dwRAEAnti = dwAnti;
   m_fRAEDoEffect = fDoEffect;
   m_pRAEIStore = pIStore;
   m_fRAEFinalRender = fFinalRender;
   m_dwRAERedrawNum = 0;
   m_pRAESuper = pSuper;
   m_pRAEWorld = pWorld;
   m_pRAE360 = p360;
   m_pRAEImage = pFImageTemp ? NULL : pImage;
   m_pRAEFImage = pFImageTemp;
   m_pRAECallback = pCallback;
   if (p360 && pCallback)
      pCallback->RS360LongLatGet (&p360->m_f360Long, &p360->m_f360Lat);

   // set extra-low detail
   PCNPREffectsList pEffect;      // effect to use
   GUID *pgEffect;
   m_fRAEIsPainterly = FALSE;
   if (/*p360 &&*/ m_fRAEDoEffect) {
      //p360->m_fHalfDetail = FALSE;

      pgEffect = (GUID*)m_lEffect.Get(0);
      for (i = 0; (i < m_lEffect.Num()) && !m_fRAEIsPainterly/*p360->m_fHalfDetail*/; i++, pgEffect += 2) {
         pEffect = EffectCreate (dwRenderShard, &pgEffect[0], &pgEffect[1]);
         if (!pEffect)
            continue;
         if (pEffect->IsPainterly())
            m_fRAEIsPainterly = TRUE;
            //p360->m_fHalfDetail = TRUE;
         delete pEffect;
      }

      // remember this
      //m_fRAEIsPainterly = p360->m_fHalfDetail;
   }
   if (m_fRAEIsPainterly && p360)
      p360->m_fHalfDetail = TRUE;

   // set shadow flags here because need painterly info
   // adjust if painterly
   if (m_fRAEIsPainterly)
      m_dwShadowsFlags = (m_dwShadowsFlags | SF_NOBUMP) & ~SF_TWOPASS360;
   pSuper->ShadowsFlagsSet (m_dwShadowsFlags);

//   {
//   fp fExp = pSuper->ExposureGet();
//   CHAR szTemp[64];
//   sprintf (szTemp, "Exposure render2=%g", (double)fExp);
//   MessageBox (NULL, "Exposure", szTemp, MB_OK);
//   }

   // draw
   if (!pSuper->Render (0, NULL, this)) {
      m_pRAE360 = NULL;
      goto blankimage;
   }
   m_pRAE360 = NULL;

   // collect shadows
   if (!p360 && apTrad[0] && pRTShadow)
      apTrad[0]->CloneLightsTo (pRTShadow);

   // if have effect then push
   if (pProgress) {
      pProgress->Pop();
      pProgress->Push (.8, 1);
   }
   PVOID pvRet = RenderApplyEffects (dwRenderShard, pFImageTemp ? NULL : pImage, pFImageTemp, pIS,
      fWantCImageStore, TRUE, this);
   if (!pvRet)
      goto blankimage;


#if 0 // replaced by RenderApplyEffects
   if (pFImageTemp) {
      // convert to image
      DWORD dwMax = dwWidth * dwHeight;
      PIMAGEPIXEL pip = pImage->Pixel(0,0);
      PFIMAGEPIXEL pfp = pFImageTemp->Pixel (0, 0);
      for (i = 0; i < dwMax; i++, pip++, pfp++) {
         pip->wRed = (WORD)max(min(pfp->fRed, (float)0xffff),0);
         pip->wGreen = (WORD)max(min(pfp->fGreen, (float)0xffff),0);
         pip->wBlue = (WORD)max(min(pfp->fBlue, (float)0xffff),0);
         pip->dwID = pfp->dwID;
         pip->dwIDPart = pfp->dwIDPart;
         pip->fZ = pfp->fZ;
      }
   }
   else if (dwAnti >= 2) {
      PCImage pDown = new CImage;
      if (!pDown) {
         if (pProgress) pProgress->Pop();
         goto blankimage;
      }

      pImage->Downsample (pDown, dwAnti);
      delete pImage;
      pImage = pDown;
   }

   // blended color background, if no jpeg or background image
   // or blend in jpeg or image
   if (!m_fLoadFromFile) {
      if (pIStore)
         ImageBlendImageStore (pImage, pIStore, m_iAspect == 10);
      else
         ImageBlendedBack (pImage, m_acBackColor[0], m_acBackColor[1], m_fBackBlendLR);
   }

   // if doing a render effect then extra push
   if (fDoEffect) {
      if (pProgress) {
         pProgress->Pop();
         pProgress->Push (.8, 1);
      }

      pgEffect = (GUID*)m_lEffect.Get(0);
      for (i = 0; i < m_lEffect.Num(); i++, pgEffect += 2) {
         if (pProgress)
            pProgress->Push ((fp)i / (fp)m_lEffect.Num(), (fp)(i+1) / (fp)m_lEffect.Num());
         pEffect = EffectCreate (&pgEffect[0], &pgEffect[1]);
         if (pEffect) {
            pEffect->Render (pImage, pSuper, pWorld, fFinalRender, pProgress);
            delete pEffect;
         }
         if (pProgress)
            pProgress->Pop ();
      }

   } // if fDoEffect
#endif // replaced by RenderApplyEffects

   if (pProgress)
      pProgress->Pop();
   if (pProgress && fExtraPush)
      pProgress->Pop();
   if (pProgress && fExtraPush2)
      pProgress->Pop();

   for (dwPing = 0; dwPing < MAX360PINGPONG; dwPing++) {
      if (apRay[dwPing])
         delete apRay[dwPing];
      if (apTrad[dwPing])
         delete apTrad[dwPing];
   }
   if (p360)
      delete p360;
   if (pImageTemp)
      delete pImageTemp;
   if (pFImageTemp)
      delete pFImageTemp;
   if (pIStore)
      delete pIStore;


#if 0 // replaced byRenderApplyEffects
   if (fWantCImageStore) {
      if (!pIS) {
         pIS = new CImageStore;
         if (!pIS) {
            delete pImage;
            return NULL;
         }
         pIS->Init (pImage);
      }
      if (pImage)
         delete pImage;
      return (PCImage) pIS;
   }
   else
      return pImage;
#endif // 0
   if ((pvRet != (PVOID)pImage) && pImage)
      delete pImage;
   if ((pvRet != (PVOID)pIS) && pIS)
      delete pIS;
   return (PCImage) pvRet;

blankimage:
   for (dwPing = 0; dwPing < MAX360PINGPONG; dwPing++) {
      if (apRay[dwPing])
         delete apRay[dwPing];
      if (apTrad[dwPing])
         delete apTrad[dwPing];
   }
   if (p360)
      delete p360;
   if (pImageTemp)
      delete pImageTemp;
   if (pFImageTemp)
      delete pFImageTemp;

   if (pIStore)
      delete pIStore;

   if (!fBlankIfFail) {
      // in some cases may want to know that error actually occured
      if (pImage)
         delete pImage;
      return NULL;
   }

   if (pProgress && fExtraPush)
      pProgress->Pop();
   if (pProgress && fExtraPush2)
      pProgress->Pop();

   if (!pImage)
      pImage = new CImage;
   if (!pImage)
      return NULL;

   if (fWantCImageStore) {
      if (pImage)
         delete pImage;
      if (!pIS) {
         pIS = new CImageStore;
         if (!pIS)
            return NULL;
      }
      // BUGFIX - To fix crash
      if (!pIS->Init (dwWidth, dwHeight)) {
         delete pIS;
         return NULL;
      }
      memset (pIS->Pixel(0,0), 0, pIS->Width() * pIS->Height() * 3);
      return (PCImage) pIS;
   }
   else
      pImage->Init (dwWidth, dwHeight, RGB(0,0,0));
   return pImage;
}


/*************************************************************************************
CRenderScene::RenderApplyEffects - Apply the effects to the render.

inputs
   PCImage        pImage - If using an integer image. NULL if using a FP imge
   PCFImage       pFImage - If using a floating point image.
   PCImageStore   pIS - In case rendering directly to an iamge store instead of
                  pImage or pFImage.
   BOOL           fWantCImageStore - Set to TRUE if should return a CImageStore, FALSE
                  if should return a CImage. (If FALSE, and pImage != NULL, then pImage
                  will be returned)
   BOOL           fCanModifyImage - If TRUE can modify pImage in place
   PCProgressSocket pProgress - Progress to use. Can be NULL.
returns
   PCImage - Image that needs to be freed by the caller. This might be pImage. If
               fWantCImageStore then this is a CImageStore that needs to be freed
               by the caller. This might return pIS too, if its not NULL.
*/
PCImage CRenderScene::RenderApplyEffects (DWORD dwRenderShard, PCImage pImage, PCFImage pFImage, PCImageStore pIS,
                                          BOOL fWantCImageStore,
                                          BOOL fCanModifyImage, PCProgressSocket pProgress)
{
   PCImage pUse = NULL;
   PCFImage pFUse = NULL;
   BOOL fDoEffect = m_fRAEDoEffect;
   DWORD i;

   // if want to apply effects and have a pFImage, then apply effects before
   // convert to flat image
   if (pFImage && m_fRAEDoEffect && m_fLoadFromFile) {
      if (fCanModifyImage)
         pFUse = pFImage;
      else {
         pFUse = pFImage->Clone();
         if (!pFUse)
            return NULL;
      }
      fDoEffect = FALSE;   // so wont try to do a second time

      // do the effects
      PCNPREffectsList pEffect;      // effect to use
      GUID *pgEffect = (GUID*)m_lEffect.Get(0);
      for (i = 0; i < m_lEffect.Num(); i++, pgEffect += 2) {
         if (pProgress)
            pProgress->Push ((fp)i / (fp)m_lEffect.Num(), (fp)(i+1) / (fp)m_lEffect.Num());
         pEffect = EffectCreate (dwRenderShard, &pgEffect[0], &pgEffect[1]);
         if (pEffect) {
            pEffect->Render (pFUse, m_pRAESuper, m_pRAEWorld, m_fRAEFinalRender, pProgress);
            // BUGBUG - what about FP effects?
            delete pEffect;
         }
         if (pProgress)
            pProgress->Pop ();
      }
   }
   else
      pFUse = pFImage;

   if (pFUse) {
      pUse = new CImage;
      if (!pUse)
         return NULL;
      if (!pUse->Init (pFUse->Width(), pFUse->Height())) {
         delete pUse;
         return NULL;
      }

      // convert to image
      DWORD dwMax = pFUse->Height() * pFUse->Width();
      PIMAGEPIXEL pip = pUse->Pixel(0,0);
      PFIMAGEPIXEL pfp = pFUse->Pixel (0, 0);
      for (i = 0; i < dwMax; i++, pip++, pfp++) {
         pip->wRed = (WORD)max(min(pfp->fRed, (float)0xffff),0);
         pip->wGreen = (WORD)max(min(pfp->fGreen, (float)0xffff),0);
         pip->wBlue = (WORD)max(min(pfp->fBlue, (float)0xffff),0);
         pip->dwID = pfp->dwID;
         pip->dwIDPart = pfp->dwIDPart;
         pip->fZ = pfp->fZ;
      }
   }
   else if (fCanModifyImage)
      pUse = pImage;
   else
      pUse = pImage ? pImage->Clone() : NULL;

   if (pFUse && (pFUse != pFImage))
      delete pFUse;

   if (pImage && (m_dwRAEAnti >= 2)) {
      PCImage pDown = new CImage;
      if (!pDown) {
         if (pProgress) pProgress->Pop();
         return NULL;
      }

      pUse->Downsample (pDown, m_dwRAEAnti);
      if (pImage != pUse)
         delete pUse;
      pUse = pDown;
   }

   // blended color background, if no jpeg or background image
   // or blend in jpeg or image
   if (pUse && !m_fLoadFromFile) {
      if (m_pRAEIStore)
         ImageBlendImageStore (pUse, m_pRAEIStore, m_iAspect == 10, TRUE);
      else
         ImageBlendedBack (pUse, m_acBackColor[0], m_acBackColor[1], m_fBackBlendLR, FALSE);
            // BUGFIX - Passing FALSE in for object ID so will skip with transparency
   }

   // if doing a render effect then extra push
   if (pUse && fDoEffect) {
      PCNPREffectsList pEffect;      // effect to use
      GUID *pgEffect = (GUID*)m_lEffect.Get(0);
      for (i = 0; i < m_lEffect.Num(); i++, pgEffect += 2) {
         if (pProgress)
            pProgress->Push ((fp)i / (fp)m_lEffect.Num(), (fp)(i+1) / (fp)m_lEffect.Num());
         pEffect = EffectCreate (dwRenderShard, &pgEffect[0], &pgEffect[1]);
         if (pEffect) {
            pEffect->Render (pUse, m_pRAESuper, m_pRAEWorld, m_fRAEFinalRender, pProgress);
            // BUGBUG - what about FP effects?
            delete pEffect;
         }
         if (pProgress)
            pProgress->Pop ();
      }

   } // if m_fRAUDoEffect

   if (fWantCImageStore) {
      if (!pIS) {
         pIS = new CImageStore;
         if (!pIS) {
            if (pUse && (pUse != pImage))
               delete pUse;
            return NULL;
         }
         // BUGFIX - Test for out of memory
         if (!pIS->Init (pUse, TRUE, !(m_dwShadowsFlags & SF_NOSUPERSAMPLE), m_iAspect == 10)) {
            delete pIS;
            pIS = NULL;
            // fall through
         }
      }
      if (pUse && (pUse != pImage))
         delete pUse;
      return (PCImage) pIS;
   }
   else
      return pUse;

}



/*************************************************************************************
M3DOpenDialog - Dialog box for opening a .m3d

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL M3DOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   OPENFILENAME   ofn;
   char  szTemp[256];
   szTemp[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   memset (&ofn, 0, sizeof(ofn));
   
   // BUGFIX - Set directory
   char szInitial[256];
   strcpy (szInitial, gszAppDir);
   GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
   ofn.lpstrInitialDir = szInitial;

   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = APPLONGNAME " file (*." M3DFILEEXT ")\0*." M3DFILEEXT "\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save " APPLONGNAME :
      "Open " APPLONGNAME " file";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = M3DFILEEXT;
   // nFileExtension 

   if (fSave) {
      if (!GetSaveFileName(&ofn))
         return FALSE;
   }
   else {
      if (!GetOpenFileName(&ofn))
         return FALSE;
   }

   // BUGFIX - Save diretory
   strcpy (szInitial, ofn.lpstrFile);
   szInitial[ofn.nFileOffset] = 0;
   SetLastDirectory(szInitial);

   // copy over
   MultiByteToWideChar (CP_ACP, 0, ofn.lpstrFile, -1, pszFile, dwChars);
   return TRUE;
}



/****************************************************************************
CRenderScene::SetRenderCamera - Sets the render camera based on the camera
specified in m_AI.gCamera. Actually, this just fills in the parameters usually
associated with override.

inputs
   PCRenderTraditional     pRender - If there's no camera, and NOT overridden, then
                           the camera details will be gotten from this.

NOTE: Will need to call ExposureSet() and CameraPerspWalkthrough() after this.
         m_Render.ExposureSet (CM_LUMENSSUN / exp(oac.m_fCamereaExposure));
         m_Render.CameraPerspWalkthrough (&m_pCameraXYZ, m_pCameraRot.p[2], m_pCameraRot.p[0], m_pCameraRot.p[1], m_fCameraFOV);
*/
void CRenderScene::SetRenderCamera (DWORD dwRenderShard, PCRenderTraditional pRender)
{
   if (m_fCameraOverride)
      return;  // no changes
   if (IsEqualGUID(m_gCamera, GUID_NULL))
      goto extract; // no camera

   PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
   if (!pWorld)
      return;

   // get object
   PCObjectSocket pos;
   pos = pWorld->ObjectGet(pWorld->ObjectFind(&m_gCamera));
   if (!pos) {
      m_gCamera = GUID_NULL;
      goto extract;
   }

   // get FOV from camera
   OSMANIMCAMERA oac;
   memset (&oac, 0, sizeof(oac));
   pos->Message (OSM_ANIMCAMERA, &oac);
   m_fCameraFOV = PI/4;
   if (oac.poac)
      m_fCameraFOV = oac.poac->m_fFOV;
   m_fCameraFOV = max(m_fCameraFOV, .001);
   m_fCameraFOV = min(m_fCameraFOV, PI * .99);

   // set the exposure
   if (oac.poac) {
      m_fCameraExposure = oac.poac->m_fExposure;
//            {
//            CHAR szTemp[64];
//            sprintf (szTemp, "Exposure SetRenderCamera=%g", (double)m_fCameraExposure);
//            MessageBox (NULL, "Exposure", szTemp, MB_OK);
//            }
   }

   // get the matrix
   CMatrix m;
   pos->ObjectMatrixGet (&m);

   // since we are looking at 0,-1,0 but the normal camera looks at 0,1,0,
   // invert y
   CMatrix mFlip;
   mFlip.Scale (-1, -1, 1);   // BUGFIX - Flip more than just y
   m.MultiplyLeft (&mFlip);

   // convert this
   CPoint p, p2;
   p.Zero();
   p.MultiplyLeft (&m);

   m.ToXYZLLT (&m_pCameraXYZ, &m_pCameraRot.p[2], &m_pCameraRot.p[0], &m_pCameraRot.p[1]);
   return;

extract:
   DWORD dwModel = pRender->CameraModelGet ();
   m_fCameraExposure = log(CM_LUMENSSUN / pRender->ExposureGet());
//            {
//            CHAR szTemp[64];
//            sprintf (szTemp, "Exposure SetRenderCamera2=%g", (double)m_fCameraExposure);
//            MessageBox (NULL, "Exposure", szTemp, MB_OK);
//            }
   if (dwModel == CAMERAMODEL_PERSPWALKTHROUGH) {
      pRender->CameraPerspWalkthroughGet (&m_pCameraXYZ,
         &m_pCameraRot.p[2], &m_pCameraRot.p[0], &m_pCameraRot.p[1], &m_fCameraFOV);
      return;
   }
   else if (dwModel != CAMERAMODEL_PERSPOBJECT)
      return;  // cant handle flat

   // else, get this one...
   //CPoint p2;
   fp fX, fY, fZ;
   CPoint pCenter;
   CMatrix mInv, mTrans;
   pRender->CameraPerspObjectGet (&p2, &pCenter, &fZ, &fX, &fY, &m_fCameraFOV);
   mTrans.Translation (pCenter.p[0], pCenter.p[1], pCenter.p[2]);
   // p2.Add (&pCenter);
   m.FromXYZLLT (&p2, fZ, fX, fY);
   m.Invert4 (&mInv);
   mInv.MultiplyRight (&mTrans);
   mInv.ToXYZLLT (&m_pCameraXYZ, &m_pCameraRot.p[2], &m_pCameraRot.p[0], &m_pCameraRot.p[1]);
}


/*************************************************************************
ObjectAttribToCombo - Given an object, this enumerates the attributes and
appends to a mem (often gMemTemp) with the combobox elemns.

inputs
   PCObjectSocket       pos - Object
   PCMem                pMem - To append to. Will append <elem>...</elem>
returns
   none
*/
static void ObjectAttribToCombo (PCObjectSocket pos, PCMem pMem)
{
   CListFixed lav;
   lav.Init (sizeof(ATTRIBVAL));
   pos->AttribGetAll (&lav);

   DWORD i;
   PATTRIBVAL pav = (PATTRIBVAL)lav.Get(0);
   ATTRIBINFO Info;
   for (i = 0; i < lav.Num(); i++, pav++) {
      // get the info
      if (!pos->AttribInfo (pav->szName, &Info))
         Info.pszDescription = NULL;

      MemCat (pMem, L"<elem name=\"");
      MemCatSanitize (pMem, pav->szName);
      MemCat (pMem, L"\"><bold>");
      MemCatSanitize (pMem, pav->szName);
      MemCat (pMem, L"</bold>");
      if (Info.pszDescription) {
         MemCat (pMem, L" - ");
         MemCatSanitize (pMem, (PWSTR) Info.pszDescription);
      }
      MemCat (pMem, L"</elem>");
   } // i
}

/*************************************************************************
ComboBoxSetAttrib - Given an attribute name, sets the attribute. IF the
attribute can't be found, then the attribute will be canged to the first
attribute supported by the object.

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control name for the combobox
   PWSTR          pszAttrib - Attribute
   DWORD          dwID - If the attribute is automatically changed, then get attribute from this object index
   fp             *pfValue - If the attribute is automatically changed then store value in this
returns
   BOOL - TRUE if success
*/
static BOOL ComboBoxSetAttrib (DWORD dwRenderShard, PCEscPage pPage, PWSTR pszControl, PWSTR pszAttrib,
                               DWORD dwID, fp *pfValue)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return FALSE;

   ESCMCOMBOBOXSELECTSTRING fs;
   memset (&fs, 0, sizeof(fs));
   fs.fExact = TRUE;
   fs.iStart = -1;
   fs.psz = pszAttrib;
   if (pControl->Message (ESCM_COMBOBOXSELECTSTRING, &fs) && (fs.dwIndex != -1))
      return TRUE;   // found

   // else, pick first one...
   ESCMCOMBOBOXGETITEM gi;
   memset (&gi, 0, sizeof(gi));
   gi.dwIndex = 0;
   pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
   if (gi.pszName)
      wcscpy (pszAttrib, gi.pszName);
   pControl->AttribSetInt (CurSel(), 0);  // select 0

   // get the value
   PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
   PCObjectSocket pos = pWorld ? pWorld->ObjectGet(dwID) : NULL;
   if (pos)
      pos->AttribGet (pszAttrib, pfValue);
   return TRUE;
}


/*************************************************************************
ComboBoxSetAttrib - Given an attribute name, sets the attribute. IF the
attribute can't be found, then the attribute will be canged to the first
attribute supported by the object.

inputs
   PCEscPage      pPage - Page
   PWSTR          pszControl - Control name for the combobox
   PWSTR          pszAttrib - Attribute
   GUID           *pgObject - If the attribute is automatically changed, then get attribute from this
   fp             *pfValue - If the attribute is automatically changed then store value in this
returns
   BOOL - TRUE if success
*/
static BOOL ComboBoxSetAttrib (DWORD dwRenderShard, PCEscPage pPage, PWSTR pszControl, PWSTR pszAttrib,
                               GUID *pgObject, fp *pfValue)
{
   PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
   if (!pWorld)
      return FALSE;
   DWORD dwID = pWorld->ObjectFind(pgObject);
   return ComboBoxSetAttrib (dwRenderShard, pPage, pszControl, pszAttrib, dwID, pfValue);
}

/*************************************************************************
ValueType - Given an attribute, returns the value type, AIT_XXX

inputs
   GUID              *pgObject - Object ID
   PWSTR             pszAttrib - Attrib name
retursn  
   DWORD - Type, AIT_XXX
*/
static DWORD ValueType (DWORD dwRenderShard, GUID *pgObject, PWSTR pszAttrib)
{
   PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
   if (!pWorld)
      return AIT_NUMBER;
   PCObjectSocket pos = pWorld->ObjectGet(pWorld->ObjectFind (pgObject));
   if (!pos)
      return AIT_NUMBER;

   ATTRIBINFO Info;
   if (!pos->AttribInfo (pszAttrib, &Info))
      return AIT_NUMBER;
   return Info.dwType;
}


/*************************************************************************
ValueType - Given an attribute, returns the value type, AIT_XXX

inputs
   DWORD             dwID - Index into object list
   PWSTR             pszAttrib - Attrib name
retursn  
   DWORD - Type, AIT_XXX
*/
static DWORD ValueType (DWORD dwRenderShard, DWORD dwID, PWSTR pszAttrib)
{
   PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
   if (!pWorld)
      return AIT_NUMBER;
   PCObjectSocket pos = pWorld->ObjectGet(dwID);
   if (!pos)
      return AIT_NUMBER;

   ATTRIBINFO Info;
   if (!pos->AttribInfo (pszAttrib, &Info))
      return AIT_NUMBER;
   return Info.dwType;
}

/*************************************************************************
ValueToControl - Object value to an edit control

inputs
   PCEscPage         pPage - Page
   PWSTR             pszControl - Edit control
   DWORD             dwType - From ValueType()
   fp                fValue - Value to write outf
retursn  
   BOOL - TRUE if success
*/
static BOOL ValueToControl (PCEscPage pPage, PWSTR pszControl, DWORD dwType,
                            fp fValue)
{
   switch (dwType) {
   case AIT_NUMBER:
   case AIT_BOOL:
   case AIT_HZ:
   default:
      DoubleToControl (pPage, pszControl, fValue);
      return TRUE;
   case AIT_DISTANCE:
      MeasureToString (pPage, pszControl, fValue);
      return TRUE;
   case AIT_ANGLE:
      AngleToControl (pPage, pszControl, fValue);
      return TRUE;
   }

   return TRUE;
}


/*************************************************************************
ValueFromControl - Object value to an edit control

inputs
   PCEscPage         pPage - Page
   PWSTR             pszControl - Edit control
   DWORD             dwType - From ValueType()
   fp                *pfValue - Filled with the value from the control
retursn  
   BOOL - TRUE if success
*/
static BOOL ValueFromControl (PCEscPage pPage, PWSTR pszControl, DWORD dwType,
                            fp *pfValue)
{
   switch (dwType) {
   case AIT_NUMBER:
   case AIT_BOOL:
   case AIT_HZ:
   default:
      *pfValue = DoubleFromControl (pPage, pszControl);
      return TRUE;
   case AIT_DISTANCE:
      MeasureParseString (pPage, pszControl, pfValue);
      return TRUE;
   case AIT_ANGLE:
      *pfValue = AngleFromControl (pPage, pszControl);
      return TRUE;
   }

   return TRUE;
}


/*************************************************************************
FillComboAttachBone - Fills the attach bone combo.

inputs
   PCRenderScene  prs - Scene
   PCEscPage      pPage - Page
   DWORD          dwObject - Object index (in m_lRSOBjECT)
returns
   BOOL - TRUE if success
*/
BOOL FillComboAttachBone (PCRenderScene prs, PCEscPage pPage, DWORD dwObject)
{
   // control name
   WCHAR szTemp[64];
   swprintf (szTemp, L"objattachbone%d", (int)dwObject);
   PCEscControl pControl = pPage->ControlFind (szTemp);
   if (!pControl)
      return FALSE;

   // wipe out the contnets
   pControl->Message (ESCM_COMBOBOXRESETCONTENT);

   CListVariable lBlank;
   PCListVariable *pplv = (PCListVariable*)prs->m_lBoneList.Get(0);
   PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);
   PCListVariable plv = (po[dwObject].dwAttachTo >= prs->m_lBoneList.Num()) ? &lBlank : pplv[po[dwObject].dwAttachTo];
   po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwObject);

   // fill in new content
   MemZero (&gMemTemp);
   DWORD i;
   PWSTR psz, pszBone;
   DWORD dwFound = 0;
   for (i = 0; i <= plv->Num(); i++) {
      psz = i ? (PWSTR)plv->Get(i-1) : L"Default";
      pszBone = i ? psz : L"";

      MemCat (&gMemTemp, L"<elem name=");
      MemCat (&gMemTemp, (int)i);
      MemCat (&gMemTemp, L">");
      MemCatSanitize (&gMemTemp, psz);
      MemCat (&gMemTemp, L"</elem>");

      // if found match then remember
      if (!_wcsicmp(pszBone, po->szAttachBone))
         dwFound = i;
   } // i

   ESCMCOMBOBOXADD add;
   memset (&add, 0, sizeof(add));
   add.dwInsertBefore = 0;
   add.pszMML = (PWSTR)gMemTemp.p;
   pControl->Message (ESCM_COMBOBOXADD, &add);

   // set the selection
   pControl->AttribSetInt (CurSel(), (int)dwFound);

   return TRUE;
}


/*************************************************************************
RenderScenePage
*/
BOOL RenderScenePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCRenderScene prs = (PCRenderScene)pPage->m_pUserData;   // node to modify
   DWORD dwRenderShard = prs->m_dwRenderShardTemp;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // scroll to right position
         if (prs->m_iVScroll > 0) {
            pPage->VScroll (prs->m_iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            prs->m_iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         PCEscControl pControl;
         DWORD i, j;
         WCHAR szTemp[64];
         ESCMLISTBOXADD lba;
         PCSceneSet pSceneSet = NULL;
         PCWorldSocket pWorld = WorldGet (dwRenderShard, &pSceneSet);
         PCScene pSceneCur = NULL;

         // diable rotation and FOV controls if 360 degree
         if (prs->m_iAspect == 10) {
            pControl = pPage->ControlFind (L"fov");
            if (pControl)
               pControl->Enable (FALSE);

            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"rot%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->Enable (FALSE);
            }
         }

         // camera auto adjust
         pControl = pPage->ControlFind (L"cameraautoheight3");
         if (pControl) {
            pControl->AttribSetBOOL (Checked(), prs->m_pCameraAutoHeight.p[3] ? TRUE : FALSE);

            DoubleToControl (pPage, L"cameraautoheight0", prs->m_pCameraAutoHeight.p[0]);
            MeasureToString (pPage, L"cameraautoheight1", prs->m_pCameraAutoHeight.p[1]);
            MeasureToString (pPage, L"cameraautoheight2", prs->m_pCameraAutoHeight.p[2]);
         }

         // fill in the camera
         pControl = pPage->ControlFind (L"camera");
         if (pWorld && pControl) {
            // camera
            DWORD dwSel;
            CListFixed lCamera;
            lCamera.Init (sizeof(OSMANIMCAMERA));
            CameraEnum(pWorld, &lCamera);

            // add the option for view camera
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = L"(View camera)";
            pControl->Message (ESCM_LISTBOXADD, &lba);
            dwSel = 0;  // default to the selection being this

            // add option for the custom camera
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = L"(Custom camera)";
            pControl->Message (ESCM_LISTBOXADD, &lba);
            if (prs->m_fCameraOverride)
               dwSel = 1;

            // add the items
            POSMANIMCAMERA pc;
            GUID g;
            pc = (POSMANIMCAMERA) lCamera.Get(0);
            for (i = 0; i < lCamera.Num(); i++, pc++) {
               PWSTR psz = pc->poac->StringGet(OSSTRING_NAME);
               if (!psz)
                  psz = L"Unnnamed";
               pc->poac->GUIDGet (&g);
               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = psz;
               pControl->Message (ESCM_LISTBOXADD, &lba);

               // set selection?
               if (IsEqualGUID (g, prs->m_gCamera))
                  dwSel = i + 2;
            }  // i
            if ((dwSel < 2) && !IsEqualGUID (prs->m_gCamera, GUID_NULL))
               prs->m_gCamera = GUID_NULL;   // since couldnt find

            pControl->AttribSetInt (CurSel(), (int)dwSel);
            
            MeasureToString (pPage, L"movedist", prs->m_fMoveDist);
         } // camera

         pControl = pPage->ControlFind (L"scene");
         if (pWorld && pSceneSet && pControl) {
            DWORD dwSel = -1;

            // scene
            pSceneSet->StateGet (&pSceneCur, NULL);
            for (i = 0; i < pSceneSet->SceneNum(); i++) {
               PCScene pScene = pSceneSet->SceneGet(i);
               if (!pScene)
                  continue;

               PWSTR pszName;
               pszName = pScene->NameGet ();
               if (!pszName || !pszName[0])
                  pszName = L"Unnamed";

               if (pSceneCur == pScene) {
                  // since this is the current scene, change the string, just in case
                  // dont need to set changed bit though
                  pSceneCur->GUIDGet(&prs->m_gScene);
                  dwSel = i;
               }

               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = pszName;
               pControl->Message (ESCM_LISTBOXADD, &lba);
            }

            if (dwSel != -1)
               pControl->AttribSetInt (CurSel(), dwSel);
         }  // if scene display
         DoubleToControl (pPage, L"SceneTime", prs->m_fSceneTime);

         MeasureToString (pPage, L"shadowslimit", prs->m_fShadowsLimit);

         // bookmarks
         pControl = pPage->ControlFind (L"bookmarkrange");
         if (pControl && pSceneCur) {
            // bookmarks
            CListFixed lBookmark;
            POSMBOOKMARK pb;
            lBookmark.Init (sizeof(OSMBOOKMARK));
            pSceneCur->BookmarkEnum (&lBookmark, TRUE, TRUE);
            pb = (POSMBOOKMARK) lBookmark.Get(0);
            DWORD dwSel = -1;

            for (i = 0; i < lBookmark.Num(); i++, pb++) {
               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = pb->szName;
               pControl->Message (ESCM_LISTBOXADD, &lba);

               if (!_wcsicmp(prs->m_szSceneBookmark, pb->szName))
                  dwSel = i;
            }

            // set selection?
            if (dwSel != -1)
               pControl->AttribSetInt (CurSel(), (int)dwSel);
            else {
               prs->m_szSceneBookmark[0] = 0;
               pControl->AttribSetInt (CurSel(), -1);
            }
         } // bookmarks

         pControl = pPage->ControlFind (L"file");
         if (pControl)
            pControl->AttribSet (Text(), prs->m_szFile);

         ComboBoxSet (pPage, L"aspect", (DWORD) max(prs->m_iAspect, 0) );
         ComboBoxSet (pPage, L"quality", prs->m_dwQuality);
         ComboBoxSet (pPage, L"anti", prs->m_dwAnti);

         // image width and height
         DWORD dwWidth, dwHeight;
         RenderSceneAspectToPixelsInt (prs->m_iAspect, SCALEPREVIEW, &dwWidth, &dwHeight);

         // fill in the controls
         HotSpotInitPage (pPage, &prs->m_lPCCircumrealityHotSpot, prs->m_iAspect == 10,
            dwWidth, dwHeight);

         // set the language for the hotspots
         MIFLLangComboBoxSet (pPage, L"langid", prs->m_lid, prs->m_pProj);

         // fill in the attributes to be modified
         if (prs->m_dwTab == 4) {
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            for (i = 0; i < prs->m_lRSATTRIB.Num(); i++, pr++) {
               swprintf (szTemp, L"rsattribnum%d", (int)i);
               if (prs->m_fLoadFromFile)
                  ComboBoxSetAttrib (dwRenderShard, pPage, szTemp, pr->szAttrib, &pr->gObject, &pr->fValue);
               else
                  ComboBoxSetAttrib (dwRenderShard, pPage, szTemp, pr->szAttrib, pr->dwObject, &pr->fValue);
               // NOTE: Not displaying a find-by-name here

               swprintf (szTemp, L"rsattribval%d", (int)i);
               DWORD dwType;
               if (prs->m_fLoadFromFile)
                  dwType = ValueType (dwRenderShard, &pr->gObject, pr->szAttrib);
               else
                  dwType = ValueType (dwRenderShard, pr->dwObject, pr->szAttrib);
               ValueToControl (pPage, szTemp, dwType, pr->fValue);
            } // i
         }

         // tab for modifying objects
         if (prs->m_dwTab == 6) {
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);
            for (i = 0; i < prs->m_lRSOBJECT.Num(); i++, po++) {
               for (j = 0; j < 3; j++) {
                  swprintf (szTemp, L"objloc%d%d", (int)j, (int)i);
                  MeasureToString (pPage, szTemp, po->pLoc.p[j]);
               }
               for (j = 0; j < 3; j++) {
                  swprintf (szTemp, L"objrot%d%d", (int)j, (int)i);
                  AngleToControl (pPage, szTemp, po->pRot.p[j]);
               }

               // set attach to
               swprintf (szTemp, L"objattachto%d", (int)i);
               ComboBoxSet (pPage, szTemp, (int)po->dwAttachTo);

               // will need to fill in attachment
               FillComboAttachBone (prs, pPage, i);
            } // i
         } // tab==6

         // tab for lights
         if (prs->m_dwTab == 8) for (i = 0; i < RSLIGHTS; i++) {
            for (j = 0; j < 3; j++) {
               swprintf (szTemp, L"lightrot%d%d", (int)j, (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (!pControl)
                  continue;

               int iVal;
               if (j < 2)
                  iVal = (int)(prs->m_apLightDir[i].p[j] / (2.0 * PI) * 360);
               else
                  iVal = (int)(sqrt(prs->m_apLightDir[i].p[j]) * 1000.0);

               pControl->AttribSetInt (L"pos", iVal);
            } // j

            swprintf (szTemp, L"lightcolor%d", (int)i);
            FillStatusColor (pPage, szTemp, prs->m_acLightColor[i]);
         } // tab==8, i

         if (prs->m_dwTab == 9) {   // background
            // check the radio button
            swprintf (szTemp, L"backmode%d", (int)prs->m_dwBackMode);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), TRUE);

            // fill in the colors
            for (i = 0; i < 2; i++) {
               swprintf (szTemp, L"backcolor%d", (int) i);
               FillStatusColor (pPage, szTemp, prs->m_acBackColor[i]);
            }

            // check the blended LR
            pControl = pPage->ControlFind (L"backblendlr");
            if (pControl)
               pControl->AttribSetBOOL (Checked(), prs->m_fBackBlendLR);

            // fill in the background file
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);
         } // tab==9

         // update the controls for location
         pPage->Message (ESCM_USER+150);
      }
      break;

   case ESCM_USER+150:  // update controls for location
      {
         MeasureToString (pPage, L"pos0", prs->m_pCameraXYZ.p[0]);
         MeasureToString (pPage, L"pos1", prs->m_pCameraXYZ.p[1]);
         MeasureToString (pPage, L"pos2", prs->m_pCameraXYZ.p[2]);
         AngleToControl (pPage, L"fov", prs->m_fCameraFOV);
         DoubleToControl (pPage, L"exposure", prs->m_fCameraExposure);

         PCEscControl pControl;
         pControl = pPage->ControlFind (L"rot0");
         if (pControl)
            pControl->AttribSetInt (L"pos", (int)
               (prs->m_pCameraRot.p[0] / 2.0 / PI * 360));
         pControl = pPage->ControlFind (L"rot1");
         if (pControl)
            pControl->AttribSetInt (L"pos", (int)
               (prs->m_pCameraRot.p[1] / 2.0 / PI * 360));
         pControl = pPage->ControlFind (L"rot2");
         if (pControl)
            pControl->AttribSetInt (L"pos", -(int)
               (prs->m_pCameraRot.p[2] / 2.0 / PI * 360));
      }
      return TRUE;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;   // no name

         if (!_wcsicmp(p->pControl->m_pszName, L"scene")) {
            PCSceneSet pSceneSet = NULL;
            PCWorldSocket pWorld = WorldGet (dwRenderShard, &pSceneSet);
            PCScene pScene = pSceneSet ? pSceneSet->SceneGet (p->dwCurSel) : NULL;
            if (!pScene)
               return TRUE;

            pScene->GUIDGet (&prs->m_gScene);
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage()); // need to do with aspect change
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"camera")) {
            PCWorldSocket pWorld = WorldGet (dwRenderShard, NULL);
            if (!pWorld)
               return TRUE;

            // get the list
            CListFixed lCamera;
            lCamera.Init (sizeof(OSMANIMCAMERA));
            CameraEnum (pWorld, &lCamera);
            POSMANIMCAMERA pc;
            pc = (p->dwCurSel >= 2) ? (POSMANIMCAMERA) lCamera.Get(p->dwCurSel - 2) : NULL;

            BOOL fReload = FALSE;
            if (pc) {
               pc->poac->GUIDGet (&prs->m_gCamera);
               prs->m_fCameraOverride = FALSE;
            }
            else {
               prs->m_fCameraOverride = (p->dwCurSel == 1);
               fReload = !prs->m_fCameraOverride;
               prs->m_gCamera = GUID_NULL;
            }
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = fReload;  // since may have switched back to original
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            // will want to update the text/sliders/values for the custom
            // camera to reflect any standard camera that using
            pPage->Message (ESCM_USER+150);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"bookmarkrange")) {
            PCSceneSet pSceneSet = NULL;
            PCWorldSocket pWorld = WorldGet (dwRenderShard, &pSceneSet);
            PCScene pSceneCur = NULL;
            if (pSceneSet)
               pSceneSet->StateGet(&pSceneCur, NULL);
            if (!pSceneCur)
               return TRUE;

            // bookmarks
            CListFixed lBookmark;
            POSMBOOKMARK pb;
            lBookmark.Init (sizeof(OSMBOOKMARK));
            pSceneCur->BookmarkEnum (&lBookmark, TRUE, TRUE);
            pb = (POSMBOOKMARK) lBookmark.Get(p->dwCurSel);

            if (!pb)
               prs->m_szSceneBookmark[0] = 0;
            else {
               wcscpy (prs->m_szSceneBookmark, pb->szName);
               DoubleToControl (pPage, L"SceneTime", prs->m_fSceneTime = pb->fStart);
            }
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszRSAttribNum = L"rsattribnum", pszObjAttachTo = L"objattachto",
            pszObjAttachBone = L"objattachbone";
         DWORD dwRSAttribNumLen = (DWORD)wcslen(pszRSAttribNum), dwObjAttachToLen = (DWORD)wcslen(pszObjAttachTo),
            dwObjAttachBoneLen = (DWORD)wcslen(pszObjAttachBone);
         if (!wcsncmp(psz, pszObjAttachTo, dwObjAttachToLen)) {
            DWORD dwIndex = _wtoi(psz + dwObjAttachToLen);
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwIndex >= prs->m_lRSOBJECT.Num())
               return TRUE; // error
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);

            if (po->dwAttachTo == dwVal)
               return TRUE;   // nothing changed

            // if attach to self then error
            if (dwVal == dwIndex) {
               pPage->MBWarning (L"You cannot attach an object to itself.");
               return TRUE;
            }

            // else, changed
            po->dwAttachTo = dwVal;
            po->szAttachBone[0] = 0;   // so not attached
            prs->m_fChanged = TRUE;

            // need to fill in combo for bone
            FillComboAttachBone (prs, pPage, dwIndex);
            return TRUE;
         }
         else if (!wcsncmp(psz, pszObjAttachBone, dwObjAttachBoneLen)) {
            DWORD dwIndex = _wtoi(psz + dwObjAttachBoneLen);
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwIndex >= prs->m_lRSOBJECT.Num())
               return TRUE; // error
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);

            // figure out current match
            CListVariable lEmpty;
            PCListVariable *pplv = (PCListVariable*)prs->m_lBoneList.Get(0);
            PCListVariable plv = (po[dwIndex].dwAttachTo < prs->m_lBoneList.Num()) ? pplv[po[dwIndex].dwAttachTo] : &lEmpty;
            po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);
            DWORD i;
            PWSTR pszBone;
            for (i = 0; i < plv->Num(); i++) {
               pszBone = (PWSTR)plv->Get(i);
               if (!_wcsicmp(pszBone, po->szAttachBone))
                  break;
            } // i
            if (i >= plv->Num())
               i = 0;
            else
               i++;
            if (i == dwVal)
               return TRUE;   // nothing changed

            // else, change
            pszBone = dwVal ? (PWSTR)plv->Get(dwVal-1) : NULL;
            wcscpy (po->szAttachBone, pszBone ? pszBone : L"");
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!wcsncmp(psz, pszRSAttribNum, dwRSAttribNumLen)) {
            DWORD dwIndex = _wtoi(psz + dwRSAttribNumLen);
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(dwIndex);
            if (!pr || !p->pszName)
               break;
            if (!_wcsicmp(pr->szAttrib, p->pszName))
               return TRUE;   // no change

            wcscpy (pr->szAttrib, p->pszName);

            // get the value...
            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
            PCObjectSocket pos;
            if (prs->m_fLoadFromFile)
               pos = pWorld ? pWorld->ObjectGet(pWorld->ObjectFind(&pr->gObject)) : NULL;
            else
               pos = pWorld ? pWorld->ObjectGet(pr->dwObject) : NULL;
            if (pos)
               pos->AttribGet (pr->szAttrib, &pr->fValue);

            // new value
            WCHAR szTemp[64];
            DWORD dwType;
            if (prs->m_fLoadFromFile)
               dwType = ValueType (dwRenderShard, &pr->gObject, pr->szAttrib);
            else
               dwType = ValueType (dwRenderShard, pr->dwObject, pr->szAttrib);
            swprintf (szTemp, L"rsattribval%d", (int)dwIndex);
            ValueToControl (pPage, szTemp, dwType, pr->fValue);

            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (HotSpotComboBoxSelChanged (pPage, p, &prs->m_lPCCircumrealityHotSpot, &prs->m_fChanged))
            return TRUE;
         else if (!_wcsicmp(psz, L"langid")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            LANGID *padw = (LANGID*)prs->m_pProj->m_lLANGID.Get(dwVal);
            dwVal = padw ? padw[0] : prs->m_lid;
            if (dwVal == prs->m_lid)
               return TRUE;

            // else changed
            prs->m_lid = (LANGID)dwVal;
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"aspect")) {
            int iVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (iVal == prs->m_iAspect)
               return TRUE;

            prs->m_iAspect = iVal;
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage()); // need to do with aspect change
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"anti")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == prs->m_dwAnti)
               return TRUE;

            prs->m_dwAnti = dwVal;
            prs->m_fChanged = TRUE;
            // dont refresh since wont show anything
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"quality")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == prs->m_dwQuality)
               return TRUE;

            prs->m_dwQuality = dwVal;
            prs->m_fChanged = TRUE;
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszRSAttribVal = L"rsattribval",
            pszObjLoc = L"objloc", pszObjRot = L"objrot",
            pszCameraAutoHeight = L"cameraautoheight";
         DWORD dwRSAttribValLen = (DWORD)wcslen(pszRSAttribVal),
            dwObjLocLen = (DWORD)wcslen(pszObjLoc), dwObjRotLen = (DWORD)wcslen(pszObjRot),
            dwCameraAutoHeight = (DWORD)wcslen(pszCameraAutoHeight);

         if (!wcsncmp(psz, pszCameraAutoHeight,dwCameraAutoHeight)) {
            DWORD dwDim = psz[dwCameraAutoHeight] - L'0';

            if (dwDim >= 1)
               MeasureParseString (pPage, psz, &prs->m_pCameraAutoHeight.p[dwDim]);
            else
               prs->m_pCameraAutoHeight.p[dwDim] = DoubleFromControl (pPage, psz);
            prs->m_fChanged = TRUE;

            // redraw...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszObjLoc, dwObjLocLen)) {
            DWORD dwDim = psz[dwObjLocLen] - L'0';
            DWORD dwIndex = _wtoi(psz + (dwObjLocLen+1));
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);
            if (!po)
               break;

            MeasureParseString (pPage, psz, &po->pLoc.p[dwDim]);
            prs->m_fChanged = TRUE;

            // redraw...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszObjRot, dwObjRotLen)) {
            DWORD dwDim = psz[dwObjRotLen] - L'0';
            DWORD dwIndex = _wtoi(psz + (dwObjRotLen+1));
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);
            if (!po)
               break;

            po->pRot.p[dwDim] = AngleFromControl (pPage, psz);
            prs->m_fChanged = TRUE;

            // redraw...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (!wcsncmp(psz, pszRSAttribVal, dwRSAttribValLen)) {
            DWORD dwIndex = _wtoi(psz + dwRSAttribValLen);
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(dwIndex);
            if (!pr)
               break;

            DWORD dwType;
            if (prs->m_fLoadFromFile)
               dwType = ValueType (dwRenderShard, &pr->gObject, pr->szAttrib);
            else
               dwType = ValueType (dwRenderShard, pr->dwObject, pr->szAttrib);
            ValueFromControl (pPage, psz, dwType, &pr->fValue);
            prs->m_fChanged = TRUE;

            // redraw...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         else if (HotSpotEditChanged (pPage, p, &prs->m_lPCCircumrealityHotSpot, &prs->m_fChanged))
            return TRUE;
         else if (!_wcsicmp(psz, L"SceneTime")) {
            prs->m_fSceneTime = DoubleFromControl (pPage, psz);
            prs->m_fSceneTime = max(0, prs->m_fSceneTime);
            prs->m_fChanged = TRUE;

            // de-select bookmark list
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"bookmarkrange");
            pControl->AttribSetInt (CurSel(), -1);

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"movedist")) {
            MeasureParseString (pPage, L"movedist", &prs->m_fMoveDist);
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"shadowslimit")) {
            MeasureParseString (pPage, L"shadowslimit", &prs->m_fShadowsLimit);
            prs->m_fChanged = TRUE;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"pos0") || !_wcsicmp(psz, L"pos1") || !_wcsicmp(psz, L"pos2") ||
            !_wcsicmp(psz, L"fov") || !_wcsicmp(psz, L"exposure")) {

            MeasureParseString (pPage, L"pos0", &prs->m_pCameraXYZ.p[0]);
            MeasureParseString (pPage, L"pos1", &prs->m_pCameraXYZ.p[1]);
            MeasureParseString (pPage, L"pos2", &prs->m_pCameraXYZ.p[2]);
            prs->m_fCameraFOV = AngleFromControl (pPage, L"fov");
            prs->m_fCameraExposure = DoubleFromControl (pPage, L"exposure");

//            {
//            CHAR szTemp[64];
//            sprintf (szTemp, "Exposure RendeerScenePage=%g", (double)prs->m_fCameraExposure);
//            MessageBox (NULL, "Exposure", szTemp, MB_OK);
//            }

            prs->m_fChanged = TRUE;

            // make sure that custom is current selection
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"camera");
            if (pControl && (pControl->AttribGetInt(CurSel()) != 1))
               pControl->AttribSetInt (CurSel(), 1);
            prs->m_fCameraOverride = TRUE;
            prs->m_gCamera = GUID_NULL;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_SCROLL:
   case ESCN_SCROLLING:
      {
         PESCNSCROLL p = (PESCNSCROLL)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         PWSTR pszRot = L"rot", pszLightRot = L"lightrot";
         DWORD dwRotLen = (DWORD)wcslen(pszRot), dwLightRotLen = (DWORD)wcslen(pszLightRot);;
         if (!wcsncmp(psz, pszLightRot, dwLightRotLen)) {
            DWORD dwDim = psz[dwLightRotLen] - L'0';
            DWORD dwNum = _wtoi(psz + (dwLightRotLen+1));
            if (dwNum >= RSLIGHTS)
               return TRUE;

            fp fVal;
            if (dwDim < 2)
               fVal = (fp)p->iPos / 360.0 * 2.0 * PI;
            else
               fVal = pow((fp)p->iPos / 1000.0, 2.0);
            if (fabs(fVal - prs->m_apLightDir[dwNum].p[dwDim]) < 0.001)
               return TRUE;   // no change

            // else, changed
            prs->m_apLightDir[dwNum].p[dwDim] = fVal;
            prs->m_fChanged = TRUE;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            return TRUE;
         }
         if (!wcsncmp(psz, pszRot, dwRotLen)) {
            DWORD dwNum = _wtoi(psz + dwRotLen);
            if (dwNum >= 3)
               return TRUE;

            fp fVal = (fp)p->iPos / 360.0 * 2.0 * PI * ((dwNum == 2) ? -1 : 1);
            if (fabs(fVal - prs->m_pCameraRot.p[dwNum]) < 0.001)
               return TRUE;   // no change

            prs->m_pCameraRot.p[dwNum] = fVal;
            prs->m_fChanged = TRUE;

            // make sure that custom is current selection
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"camera");
            if (pControl && (pControl->AttribGetInt(CurSel()) != 1))
               pControl->AttribSetInt (CurSel(), 1);
            prs->m_fCameraOverride = TRUE;
            prs->m_gCamera = GUID_NULL;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszRSAttribDel = L"rsattribdel",
            pszObjDel = L"objdel", pszObjTop = L"objtop", pszRSColorDel = L"rscolordel",
            pszLightColor = L"changelightcolor", pszBackMode = L"backmode",
            pszChangeBackColor = L"changebackcolor";
         DWORD dwRSAttribDel = (DWORD)wcslen(pszRSAttribDel),
            dwObjDelLen = (DWORD)wcslen(pszObjDel), dwObjTopLen = (DWORD)wcslen(pszObjTop),
            dwRSColorDelLen = (DWORD)wcslen(pszRSColorDel), dwLightColorLen = (DWORD)wcslen(pszLightColor),
            dwBackModeLen = (DWORD)wcslen(pszBackMode), dwChangeBackColorLen = (DWORD)wcslen(pszChangeBackColor);

         if (!_wcsicmp(p->pControl->m_pszName, L"changeeffect")) {
            GUID *pgEffect = (GUID*)prs->m_lEffect.Get(0);
            GUID ag[2];
            if (prs->m_lEffect.Num())
               memcpy (ag, pgEffect, sizeof(ag));
            else
               memset (ag, 0, sizeof(ag));

            if (!EffectSelDialog (dwRenderShard, pPage->m_pWindow->m_hWnd, &ag[0], &ag[1]))
               return TRUE;   // no change

            // store away
            if (IsEqualGUID(ag[0], GUID_NULL))
               prs->m_lEffect.Remove (0); // remove it since it's NULL now
            else {
               if (prs->m_lEffect.Num())
                  memcpy (pgEffect, ag, sizeof(ag));
               else
                  prs->m_lEffect.Add (ag);   // since nothing there before
            }

            prs->m_fChanged = TRUE;

            // new bitmap
            if (prs->m_hBitEffect)
               DeleteObject (prs->m_hBitEffect);
            COLORREF cTrans;
            prs->m_hBitEffect = EffectGetThumbnail (dwRenderShard, &ag[0], &ag[1],
               pPage->m_pWindow->m_hWnd, &cTrans, TRUE);

            // set the bitmap
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)prs->m_hBitEffect);
            PCEscControl pControl = pPage->ControlFind (L"effectbit");
            if (pControl)
               pControl->AttribSet (L"hbitmap", szTemp);


            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[0] = TRUE;  // so get a final render
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"backfiledialog")) {
            if (!OpenImageDialog (pPage->m_pWindow->m_hWnd, prs->m_szBackFile,
               sizeof(prs->m_szBackFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            prs->m_fChanged = TRUE;

            // check the button if it isnt' already
            PCEscControl pControl;
            WCHAR szTemp[64];
            DWORD i;
            prs->m_dwBackMode = 1;
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"backmode%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSetBOOL (Checked(), (i == prs->m_dwBackMode));
            }

            // write file
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);

            // wipe out the render scene
            if (prs->m_pBackRend)
               delete prs->m_pBackRend;
            prs->m_pBackRend = NULL;

            // refresh
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"backrenddialog")) {
            // make sure it's created
            if (!prs->m_pBackRend) {
               prs->m_pBackRend = new CRenderScene;
               if (!prs->m_pBackRend)
                  return TRUE;
               prs->m_pBackRend->m_fLoadFromFile = TRUE;
               prs->m_fChanged = TRUE;
            }

            // set radio buttons
            // check the button if it isnt' already
            PCEscControl pControl;
            WCHAR szTemp[64];
            DWORD i;
            prs->m_dwBackMode = 2;
            for (i = 0; i < 3; i++) {
               swprintf (szTemp, L"backmode%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  pControl->AttribSetBOOL (Checked(), (i == prs->m_dwBackMode));
            }

            // wipe out file name
            prs->m_szBackFile[0] = 0;
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);

            // edit
            if (prs->m_pBackRend->Edit (dwRenderShard, pPage->m_pWindow->m_hWnd, prs->m_lid,
               prs->m_fReadOnly, prs->m_pProj, FALSE)) {

               prs->m_fChanged = TRUE;

               // refresh
               BOOL af[2];
               memset (af, 0, sizeof(af));
               pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            }

            return TRUE;
         }
         else if (!_wcsicmp (psz, L"backblendlr")) {
            prs->m_fBackBlendLR = p->pControl->AttribGetBOOL(Checked());
            prs->m_fChanged = TRUE;

            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"cameraautoheight3")) {
            prs->m_pCameraAutoHeight.p[3] = p->pControl->AttribGetBOOL(Checked()) ? 1 : 0;
            prs->m_fChanged = TRUE;

            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!wcsncmp(psz, pszBackMode, dwBackModeLen)) {
            DWORD dwIndex = _wtoi(psz + dwBackModeLen);

            prs->m_dwBackMode = dwIndex;
            prs->m_fChanged = TRUE;

            switch (dwIndex) {
            case 0:  // color blend
               prs->m_szBackFile[0] = 0;  // so none
               if (prs->m_pBackRend)
                  delete prs->m_pBackRend;
               prs->m_pBackRend = 0;
               break;

            case 1:  // back file
               if (prs->m_pBackRend)
                  delete prs->m_pBackRend;
               prs->m_pBackRend = 0;
               break;

            case 2:  // back rend
               prs->m_szBackFile[0] = 0;  // so none
               if (!prs->m_pBackRend) {
                  prs->m_pBackRend = new CRenderScene;
                  if (prs->m_pBackRend)
                     prs->m_pBackRend->m_fLoadFromFile = TRUE;
               }
               break;
            }

            // set edit control
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"backfile");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szBackFile);

            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!wcsncmp(psz, pszChangeBackColor, dwChangeBackColorLen)) {
            DWORD dwIndex = _wtoi(psz + dwChangeBackColorLen);
            if (dwIndex >= 2)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"backcolor%d", (int) dwIndex);

            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, prs->m_acBackColor[dwIndex], pPage, szTemp);
            if (cr != prs->m_acBackColor[dwIndex]) {
               prs->m_acBackColor[dwIndex] = cr;
               prs->m_fChanged = TRUE;

               BOOL af[2];
               memset (af, 0, sizeof(af));
               pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            }
            return TRUE;
         }
         else if (!wcsncmp(psz, pszLightColor, dwLightColorLen)) {
            DWORD dwIndex = _wtoi(psz + dwLightColorLen);
            if (dwIndex >= RSLIGHTS)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lightcolor%d", (int) dwIndex);

            COLORREF cr;
            cr = AskColor (pPage->m_pWindow->m_hWnd, prs->m_acLightColor[dwIndex], pPage, szTemp);
            if (cr != prs->m_acLightColor[dwIndex]) {
               prs->m_acLightColor[dwIndex] = cr;
               prs->m_fChanged = TRUE;
               BOOL af[2];
               memset (af, 0, sizeof(af));
               pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            }
            return TRUE;
         }
         else if (!wcsncmp(psz, pszObjDel, dwObjDelLen)) {
            DWORD dwIndex = _wtoi(psz + dwObjDelLen);
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(dwIndex);
            if (!po)
               break;

            // will need to modify all attribs to note deletion
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            DWORD i;
            for (i = prs->m_lRSATTRIB.Num()-1; i < prs->m_lRSATTRIB.Num(); i--) {
               if (pr[i].dwObject < dwIndex)
                  continue;   // no change
               if (pr[i].dwObject > dwIndex) {
                  pr[i].dwObject--;
                  continue;
               }

               // else, delete
               prs->m_lRSATTRIB.Remove (i);
               pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            }

            // will need to modify all paints to note deletion
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            for (i = prs->m_lRSCOLOR.Num()-1; i < prs->m_lRSCOLOR.Num(); i--) {
               if (prc[i].dwObject < dwIndex)
                  continue;   // no change
               if (prc[i].dwObject > dwIndex) {
                  prc[i].dwObject--;
                  continue;
               }

               // else, delete
               delete prc[i].pSurf;
               prs->m_lRSCOLOR.Remove (i);
               prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            }

            // remove object
            prs->m_lRSOBJECT.Remove (dwIndex);
            prs->m_fChanged = TRUE;

            // loop through all the objects and adjust attachment numbers
            po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);
            for (i = 0; i < prs->m_lRSOBJECT.Num(); i++, po++) {
               if (po->dwAttachTo == (DWORD)-1)
                  continue;

               if (po->dwAttachTo < dwIndex)
                  continue;   // too low
               if (po->dwAttachTo == dwIndex)
                  po->dwAttachTo = (DWORD)-1;   // since no longer there
               else
                  po->dwAttachTo--; // since higher
            } // i


            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszObjTop, dwObjTopLen)) {
            DWORD dwIndex = _wtoi(psz + dwObjTopLen);
            if (dwIndex >= prs->m_lRSOBJECT.Num())
               return TRUE;
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);

            RSOBJECT rTemp;
            rTemp = po[0];
            po[0] = po[dwIndex];
            po[dwIndex] = rTemp;
            prs->m_fChanged = TRUE;

            // will need to modify all attribs to note move
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            DWORD i;
            for (i = prs->m_lRSATTRIB.Num()-1; i < prs->m_lRSATTRIB.Num(); i--) {
               if (pr[i].dwObject == dwIndex)
                  pr[i].dwObject = 0;
               else if (pr[i].dwObject == 0)
                  pr[i].dwObject = dwIndex;
            }

            // will need to modify all paints to note move
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            for (i = prs->m_lRSCOLOR.Num()-1; i < prs->m_lRSCOLOR.Num(); i--) {
               if (prc[i].dwObject == dwIndex)
                  prc[i].dwObject = 0;
               else if (prc[i].dwObject == 0)
                  prc[i].dwObject = dwIndex;
            }

            // will need to adjust all the attachments
            for (i = 0; i < prs->m_lRSOBJECT.Num(); i++) {
               if (po[i].dwAttachTo == 0)
                  po[i].dwAttachTo = dwIndex;
               else if (po[i].dwAttachTo == dwIndex)
                  po[i].dwAttachTo = 0;
            } // i


            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszRSAttribDel, dwRSAttribDel)) {
            DWORD dwIndex = _wtoi(psz + dwRSAttribDel);
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(dwIndex);
            if (!pr)
               break;

            // delete
            prs->m_lRSATTRIB.Remove (dwIndex);
            prs->m_fChanged = TRUE;

            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!wcsncmp(psz, pszRSColorDel, dwRSColorDelLen)) {
            DWORD dwIndex = _wtoi(psz + dwRSColorDelLen);
            PRSCOLOR pr = (PRSCOLOR)prs->m_lRSCOLOR.Get(dwIndex);
            if (!pr)
               break;

            // delete
            delete pr->pSurf;
            prs->m_lRSCOLOR.Remove (dwIndex);
            prs->m_fChanged = TRUE;

            // refresh image...
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = prs->m_fLoadFromFile; // BUGFIX - If load from file and delete, then recalc
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (HotSpotButtonPress (pPage, p, &prs->m_lPCCircumrealityHotSpot, &prs->m_fChanged))
            return TRUE;
         else if (!_wcsicmp(psz, L"open")) {
            if (!M3DOpenDialog (pPage->m_pWindow->m_hWnd, prs->m_szFile, sizeof(prs->m_szFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            prs->m_fChanged = TRUE;
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            PCEscControl pControl;
            pControl = pPage->ControlFind (L"file");
            if (pControl)
               pControl->AttribSet (Text(), prs->m_szFile);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"refresh")) {
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"finalquality")) {
            BOOL af[2];
            memset (af, 0, sizeof(af));
            af[0] = af[1] = TRUE;
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"objnew")) {
            RSOBJECT ro;
            memset (&ro, 0, sizeof(ro));
            ro.dwAttachTo = (DWORD)-1;

            if (!ObjectCFNewDialog (dwRenderShard, pPage->m_pWindow->m_hWnd, &ro.gMajor, &ro.gMinor))
               return TRUE;   // cancelled

            // add it
            prs->m_lRSOBJECT.Add (&ro);
            prs->m_fChanged = TRUE;

            BOOL af[2];
            memset (af, 0, sizeof(af));

            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image
            pPage->Exit (RedoSamePage()); // since need to add entry
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"moveforward") || !_wcsicmp(psz, L"moveback") ||
            !_wcsicmp(psz, L"rotleft") || !_wcsicmp(psz, L"rotright")) {

            if (!_wcsicmp(psz, L"moveforward")) {
               prs->m_pCameraXYZ.p[0] -= sin(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
               prs->m_pCameraXYZ.p[1] += cos(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
            }
            else if (!_wcsicmp(psz, L"moveback")) {
               prs->m_pCameraXYZ.p[0] += sin(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
               prs->m_pCameraXYZ.p[1] -= cos(prs->m_pCameraRot.p[2]) * prs->m_fMoveDist;
            }
            else if (!_wcsicmp(psz, L"rotleft"))
               prs->m_pCameraRot.p[2] += PI/8;
            else if (!_wcsicmp(psz, L"rotright"))
               prs->m_pCameraRot.p[2] -= PI/8;
            prs->m_pCameraRot.p[2] = myfmod(prs->m_pCameraRot.p[2] + PI,2.0*PI) - PI;

            prs->m_fChanged = TRUE;

            // make sure that custom is current selection
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"camera");
            if (pControl && (pControl->AttribGetInt(CurSel()) != 1))
               pControl->AttribSetInt (CurSel(), 1);
            prs->m_fCameraOverride = TRUE;
            prs->m_gCamera = GUID_NULL;

            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            // update the controls for location
            pPage->Message (ESCM_USER+150);
            return TRUE;
         }
      }
      break;

   case ESCM_USER+102:  // called to indicate that should redraw
      {
         CProgress Progress;
         BOOL *paf = (BOOL*)pParam;

         Progress.Start (pPage->m_pWindow->m_hWnd, "Drawing...", paf[0] || paf[1]);

//            {
//            CHAR szTemp[64];
//            sprintf (szTemp, "Exposure RenderScenePage102=%g", (double)prs->m_fCameraExposure);
//            MessageBox (NULL, "Exposure", szTemp, MB_OK);
//            }
         if (prs->m_pImage)
            delete prs->m_pImage;
         prs->m_pImage = prs->Render (dwRenderShard, SCALEPREVIEW, paf[0], paf[1], TRUE, 0, &Progress, FALSE,
            NULL, NULL, NULL, NULL);
         if (!prs->m_pImage)
            return TRUE;   // unlikely

         if (prs->m_hBmp)
            DeleteObject (prs->m_hBmp);
         HDC hDC = GetDC (pPage->m_pWindow->m_hWnd);
         prs->m_hBmp = prs->m_pImage->ToBitmap (hDC);
         ReleaseDC (pPage->m_pWindow->m_hWnd, hDC);

         PCEscControl pControl = pPage->ControlFind (L"image");
         WCHAR szTemp[32];
         swprintf (szTemp, L"%lx", (__int64)prs->m_hBmp);
         if (pControl)
            pControl->AttribSet (L"hbitmap", szTemp);
      }
      return TRUE;

   case ESCN_IMAGEDRAGGED:
      {
         if (prs->m_fReadOnly)
            return TRUE;   // cant change

         PESCNIMAGEDRAGGED p = (PESCNIMAGEDRAGGED)pParam;

         if (prs->m_dwTab == 5) {   // hot spots
            // image width and height
            DWORD dwWidth, dwHeight;
            RenderSceneAspectToPixelsInt (prs->m_iAspect, SCALEPREVIEW, &dwWidth, &dwHeight);

            HotSpotImageDragged (pPage, p, &prs->m_lPCCircumrealityHotSpot,
                                    dwWidth, dwHeight, &prs->m_fChanged);
         }
         else if (prs->m_dwTab == 4) { // attributes
            // see if clickd outside the image
            if (!prs->m_pImage || (p->rPos.left < 0) || (p->rPos.left >= (int)prs->m_pImage->Width()) ||
               (p->rPos.top < 0) || (p->rPos.top >= (int)prs->m_pImage->Height())) {

               pPage->MBWarning (L"Please click on the image.");
               return TRUE;
            }

            // get the pixel
            PIMAGEPIXEL pip = prs->m_pImage->Pixel ((DWORD)p->rPos.left, (DWORD)p->rPos.top);
            DWORD dwID = HIWORD(pip->dwID);
            // if not an object, or if beyond the range of acceptable objects
            if (!dwID || (!prs->m_fLoadFromFile && (dwID-1 >= prs->m_lRSOBJECT.Num())) ) {
               pPage->MBWarning (L"You must click on an object.", L"You clicked on a background.");
               return TRUE;
            }
            dwID--;

            // get the object
            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
            PCObjectSocket pos = pWorld->ObjectGet (dwID);
            if (!pos)
               return TRUE;

            // get the guid
            RSATTRIB rs;
            memset (&rs, 0, sizeof(rs));
            pos->GUIDGet (&rs.gObject);
            rs.dwObject = dwID;
            
            // add it
            prs->m_lRSATTRIB.Add (&rs);
            prs->m_fChanged = TRUE;

            // refresh
            pPage->Exit (RedoSamePage());
         }
         else if (prs->m_dwTab == 7) { // colors
            // see if clickd outside the image
            if (!prs->m_pImage || (p->rPos.left < 0) || (p->rPos.left >= (int)prs->m_pImage->Width()) ||
               (p->rPos.top < 0) || (p->rPos.top >= (int)prs->m_pImage->Height())) {

               pPage->MBWarning (L"Please click on the image.");
               return TRUE;
            }

            // get the pixel
            PIMAGEPIXEL pip = prs->m_pImage->Pixel ((DWORD)p->rPos.left, (DWORD)p->rPos.top);
            DWORD dwID = HIWORD(pip->dwID);
            // if not an object, or if beyond the range of acceptable objects
            if (!dwID || (!prs->m_fLoadFromFile && (dwID-1 >= prs->m_lRSOBJECT.Num())) ) {
               pPage->MBWarning (L"You must click on an object.", L"You clicked on a background.");
               return TRUE;
            }
            dwID--;

            // get the object
            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);
            PCObjectSocket pos = pWorld->ObjectGet (dwID);
            if (!pos)
               return TRUE;
            DWORD dwSurf = LOWORD(pip->dwID);

            // get the guid
            RSCOLOR rc;
            memset (&rc, 0, sizeof(rc));
            pos->GUIDGet (&rc.gObject);
            rc.dwObject = dwID;

            // see if already have this one..
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            DWORD i;
            for (i = 0; i < prs->m_lRSCOLOR.Num(); i++, prc++) {
               if (prc->pSurf->m_dwID != dwSurf)
                  continue;

               if (prs->m_fLoadFromFile && IsEqualGUID(prc->gObject, rc.gObject))
                  break;
               else if (!prs->m_fLoadFromFile && (prc->dwObject == rc.dwObject))
                  break;
            } // i
            if (i >= prs->m_lRSCOLOR.Num()) {
               // need to add
               rc.pSurf = pos->SurfaceGet (dwSurf);
               if (!rc.pSurf) {
                  pPage->MBWarning (L"You can't paint this surface.");
                  return TRUE;
               }
               rc.pSurf->m_dwID = dwSurf;

               prs->m_lRSCOLOR.Add (&rc);
               prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(prs->m_lRSCOLOR.Num()-1);
            }

            if (!TextureSelDialog (dwRenderShard, pPage->m_pWindow->m_hWnd, prc->pSurf, pWorld))
               return TRUE;   // nothing changed
          

            // note changed
            prc->pSurf->m_szScheme[0] = 0;   // so don't use schemes, because they dont work
            prc->pSurf->m_dwID = dwSurf;
            prs->m_fChanged = TRUE;

            // redraw
            BOOL af[2];
            memset (af, 0, sizeof(af));
            pPage->Message (ESCM_USER+102, &af[0]);  // refresh image

            // refresh
            pPage->Exit (RedoSamePage());
         }
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;

         PWSTR pszTab = L"tabpress:";
         DWORD dwLen = (DWORD)wcslen(pszTab);

         if (!wcsncmp(p->psz, pszTab, dwLen)) {
            prs->m_dwTab = (DWORD)_wtoi(p->psz + dwLen);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }

      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         PWSTR pszIfTab = L"IFTAB", pszEndIfTab = L"ENDIFTAB";
         DWORD dwIfTabLen = (DWORD)wcslen(pszIfTab), dwEndIfTabLen = (DWORD)wcslen(pszEndIfTab);

         if (!_wcsicmp(p->pszSubName, L"EFFECTBITMAP")) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)prs->m_hBitEffect);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszIfTab, dwIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!wcsncmp (p->pszSubName, pszEndIfTab, dwEndIfTabLen)) {
            DWORD dwNum = _wtoi(p->pszSubName + dwEndIfTabLen);
            if (dwNum == prs->m_dwTab)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp (p->pszSubName, L"IFEXISTINGCAMERA")) {
            if (prs->m_fLoadFromFile)
               p->pszSubString = L"";
            else
               p->pszSubString = L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp (p->pszSubName, L"ENDIFEXISTINGCAMERA")) {

            if (prs->m_fLoadFromFile)
               p->pszSubString = L"";
            else
               p->pszSubString = L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSTABS")) {
            PWSTR apsz[] = {
               L"File",
               L"Objects",
               L"Lights",
               L"Quality",
               L"Scene",
               L"Camera",
               L"Paint",
               L"Attributes",
               L"Background",
               L"Hot spots",
               L"Transition"
            };
            PWSTR apszHelp[] = {
               L"Loads in a different " APPSHORTNAMEW L" file.",
               L"Add and move objects within the scene.",
               L"Control the lighting of the scene.",
               L"Changes the quality of the image.",
               L"Controls which animation scene and timeslice are drawn.",
               L"Places the camera.",
               L"Lets you change an object's color or texture.",
               L"Changes the objects' attributes.",
               L"Changes the background for the scene.",
               L"Select which areas can be clicked on.",
               L"Controls the transition (fade, pan, and zoom) of the image.",
            };
            DWORD adwID[] = {
               0,  //L"File",
               6, // objects
               8, // lights
               1, //L"Quality",
               2, //L"Scene",
               3, // L"Camera",
               7, // L"Paint",
               4, // L"Attributes",
               9, // background
               5, // L"Hot spots",
               20 // transition
            };

            CListFixed lSkip;
            lSkip.Init (sizeof(DWORD));
            DWORD dw;

            // skip depending upon if have file or not
            if (prs->m_fLoadFromFile) {
               dw = 6;  // objects
               lSkip.Add (&dw);

               dw = 8;  // lights
               lSkip.Add (&dw);

               dw = 9;  // background
               lSkip.Add (&dw);
            }
            else {
               dw = 0;  // file
               lSkip.Add (&dw);

               dw = 2;  //scene
               lSkip.Add (&dw);
            }
            if (!prs->m_fTransition) {
               dw = 20; // transition
               lSkip.Add (&dw);
            }


            // if there's no scene then skip the scene selection
            PCSceneSet pScene;
            PCWorldSocket pWorld = WorldGet (dwRenderShard, &pScene);
            if (!pScene || !pScene->SceneNum()) {
               dw = 2;  // skip scene
               lSkip.Add (&dw);
            }

            p->pszSubString = RenderSceneTabs (prs->m_dwTab, sizeof(apsz)/sizeof(PWSTR), apsz,
               apszHelp, adwID, lSkip.Num(), (DWORD*)lSkip.Get(0));
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"3D scene resource";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IMAGEDRAG")) {
            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<imagedrag name=image clickmode=");
            switch (prs->m_fReadOnly ? 10000 : prs->m_dwTab) {
            case 4:  // attributes
            case 7:  // paint
               // click only
               MemCat (&gMemTemp, L"1");
               break;
            case 5:  // hot spots
               // click and drag
               MemCat (&gMemTemp, L"2");
               break;
            default: // others (including RO)... cant click
               MemCat (&gMemTemp, L"0");
               break;
            }
            MemCat (&gMemTemp, L" border=2 width=");
            int iWidth = 90;  // percent
            DWORD dwWidth, dwHeight;
            RenderSceneAspectToPixelsInt (prs->m_iAspect, 1, &dwWidth, &dwHeight);
            if (dwWidth < dwHeight)
               iWidth = iWidth * (int)dwWidth / (int)dwHeight;

            MemCat (&gMemTemp, iWidth);

            MemCat (&gMemTemp, L"% hbitmap=");
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)prs->m_hBmp);
            MemCat (&gMemTemp, szTemp);

            MemCat (&gMemTemp, L"/>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBENABLE")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"enabled=false";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBREADONLY")) {
            if (prs && prs->m_fReadOnly)
               p->pszSubString = L"readonly=true";
            else
               p->pszSubString = L"";
            return TRUE;
         }
         else if (HotSpotSubstitution (pPage, p, &prs->m_lPCCircumrealityHotSpot, prs->m_fReadOnly))
            return TRUE;
         else if (!_wcsicmp(p->pszSubName, L"COMBOOBJECTS")) {
            MemZero (&gMemTemp);
            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);

            DWORD i;
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);
            for (i = 0; i < prs->m_lRSOBJECT.Num(); i++) {
               PCObjectSocket pos = pWorld->ObjectGet(i);
               PWSTR psz = pos ? pos->StringGet(OSSTRING_NAME) : NULL;
               if (!psz)
                  psz = L"Unknown";

               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</elem>");
            } // i
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OBJECTS")) {
            MemZero (&gMemTemp);
            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);

            DWORD i;
            PRSOBJECT po = (PRSOBJECT)prs->m_lRSOBJECT.Get(0);
            for (i = 0; i < prs->m_lRSOBJECT.Num(); i++) {
               PCObjectSocket pos = pWorld->ObjectGet(i);
               PWSTR psz = pos ? pos->StringGet(OSSTRING_NAME) : NULL;
               if (!psz)
                  psz = L"Unknown";

               MemCat (&gMemTemp, L"<tr><td><bold>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold><p/>");

               if (i) {
                  // button to move to top of list
                  MemCat (&gMemTemp,
                     L"<button style=uparrow name=objtop");
                  MemCat (&gMemTemp, (int)i);
                  if (prs->m_fReadOnly)
                     MemCat (&gMemTemp, gpszEnabledFalse);
                  MemCat (&gMemTemp,
                     L"><bold>Top of list</bold>"
                     L"<xHoverHelp>Moves the object to the top of the list so it's easier to modify.</xHoverHelp>"
                     L"</button>"
                     L"<br/>");
               }

               MemCat (&gMemTemp,
                  L"<button name=objdel");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"><bold>Delete this</bold></button>"

                  L"</td>"
                  L"<td>"
                  L"<align align=right><bold>"
                  L"X: <edit width=80% maxchars=64 name=objloc0");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Y: <edit width=80% maxchars=64 name=objloc1");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Z: <edit width=80% maxchars=64 name=objloc2");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/>"
                  L"<p/>"
                  L"Rot: <edit width=80% maxchars=64 name=objrot2");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Pitch: <edit width=80% maxchars=64 name=objrot0");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"/><br/>"
                  L"Yaw: <edit width=80% maxchars=64 name=objrot1");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);

               MemCat (&gMemTemp,
                  L"/><br/>"
                  L"Attached to: <xComboObjects width=50% name=objattachto");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp,
                  L"/><br/>"
                  L"Connection point: <combobox width=50% cbheight=150 name=objattachbone");
               MemCat (&gMemTemp, (int)i);
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp,
                  L"/>"
                  L"</bold></align>"
                  L"</td>"
                  L"</tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSATTRIB")) {
            MemZero (&gMemTemp);

            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);

            DWORD i;
            PRSATTRIB pr = (PRSATTRIB)prs->m_lRSATTRIB.Get(0);
            for (i = 0; i < prs->m_lRSATTRIB.Num(); i++, pr++) {
               MemCat (&gMemTemp, L"<tr><td><bold>");

               PCObjectSocket pObject;
               if (prs->m_fLoadFromFile)
                  pObject = pWorld->ObjectGet(pWorld->ObjectFind(&pr->gObject));
               else
                  pObject = pWorld->ObjectGet(pr->dwObject);

               PWSTR psz = pObject ? pObject->StringGet(OSSTRING_NAME) : NULL;
               if (!psz)
                  psz = L"Non-existant";

               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold>"
                  L"</td>"

                  L"<td>"
			         L"<combobox width=100% cbheight=300 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rsattribnum");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L">");
               // BUGFIX - Make sure have pObject
               if (pObject)
                  ObjectAttribToCombo (pObject, &gMemTemp);
               MemCat (&gMemTemp, L"</combobox>"
		            L"</td>"

		            L"<td><bold><edit width=100% maxchars=64 ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rsattribval");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold><br/>");
			      MemCat (&gMemTemp, L"<button ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rsattribdel");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Remove this</bold></button>"
                  L"</td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"RSCOLOR")) {
            MemZero (&gMemTemp);

            PCWorldSocket pWorld = WorldGet(dwRenderShard, NULL);

            DWORD i;
            PRSCOLOR prc = (PRSCOLOR)prs->m_lRSCOLOR.Get(0);
            for (i = 0; i < prs->m_lRSCOLOR.Num(); i++, prc++) {
               MemCat (&gMemTemp, L"<tr><td><bold>");

               PCObjectSocket pObject;
               if (prs->m_fLoadFromFile)
                  pObject = pWorld->ObjectGet(pWorld->ObjectFind(&prc->gObject));
               else
                  pObject = pWorld->ObjectGet(prc->dwObject);

               PWSTR psz = pObject ? pObject->StringGet(OSSTRING_NAME) : NULL;
               if (!psz)
                  psz = L"Non-existant";

               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold>"
                  L"</td>"

                  L"<td>");
               MemCat (&gMemTemp, (int)prc->pSurf->m_dwID);
               MemCat (&gMemTemp,
		            L"</td>"

		            L"<td>");

               WCHAR szName[256];
               if (prc->pSurf->m_fUseTextureMap) {
                  szName[0] = 0;
                  TextureNameFromGUIDs (dwRenderShard, &prc->pSurf->m_gTextureCode, &prc->pSurf->m_gTextureSub,
                     NULL, NULL, szName);
                  MemCatSanitize (&gMemTemp, szName);
               }
               else
                  MemCat (&gMemTemp, L"Color");

               MemCat (&gMemTemp, L"<br/>");
			      MemCat (&gMemTemp, L"<button ");
               if (prs->m_fReadOnly)
                  MemCat (&gMemTemp, gpszEnabledFalse);
               MemCat (&gMemTemp, L"name=rscolordel");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Remove this</bold></button>"
                  L"</td></tr>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


/*************************************************************************************
CRenderScene::Edit - This brings up a dialog box for editing the object.

inputs
   HWND           hWnd - Window to bring dialog up from
   LANGID         lid - Language ID to use as default
   BOOL           fReadOnly - If TRUE then data is read only and cant be changed
   PCMIFLProj     pProj - Project it's it
   BOOL           fTransition - If TRUE then allow a transition to be edited
returns
   BOOL - TRUE if changed, FALSE if didnt
*/
BOOL CRenderScene::Edit (DWORD dwRenderShard, HWND hWnd, LANGID lid, BOOL fReadOnly, PCMIFLProj pProj,
                         BOOL fTransition)
{
   m_fChanged = FALSE;
   m_hBmp = NULL;
   m_pImage = NULL;
   m_fReadOnly  = fReadOnly;
   m_lid = lid;
   m_iVScroll = 0;
   m_pProj = pProj;
   m_dwTab = m_fLoadFromFile ? 0 : 6;
   m_hBitEffect = NULL;
   m_fTransition = fTransition;
   CEscWindow Window;

   // if any hotspots then fix the language id
   PCCircumrealityHotSpot *pphs = (PCCircumrealityHotSpot*)m_lPCCircumrealityHotSpot.Get(0);
   if (m_lPCCircumrealityHotSpot.Num())
      m_lid = pphs[0]->m_lid;


   // render an initial pass
   {
      CProgress Progress;
      Progress.Start (hWnd, "Drawing...", TRUE);
      m_pImage = Render (dwRenderShard, SCALEPREVIEW, FALSE, TRUE, TRUE, 0, &Progress, FALSE,
         NULL, NULL, NULL, NULL);
      if (!m_pImage)
         goto done;
   }
   HDC hDC = GetDC (hWnd);
   m_hBmp = m_pImage->ToBitmap (hDC);
   ReleaseDC (hWnd, hDC);
   if (!m_hBmp)
      goto done;

   COLORREF cTrans;
   GUID *pgEffect = (GUID*) m_lEffect.Get(0);
   GUID ag[2];
   if (m_lEffect.Num())
      memcpy (ag, pgEffect, sizeof(ag));
   else
      memset (ag, 0, sizeof(ag));
   m_hBitEffect = EffectGetThumbnail (dwRenderShard, &ag[0], &ag[1],
      hWnd, &cTrans, TRUE);

   // create the window
   RECT r;
   PWSTR psz;
   DialogBoxLocation3 (GetDesktopWindow(), &r, TRUE);
   Window.Init (ghInstance, hWnd, 0, &r);
redo:
   m_dwRenderShardTemp = dwRenderShard;
   psz = Window.PageDialog (ghInstance, IDR_MMLRENDERSCENE, RenderScenePage, this);
   m_iVScroll = Window.m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;
   if (psz && !_wcsicmp(psz, L"TransitionUI")) {
      DWORD dwWidth, dwHeight;
      RenderSceneAspectToPixelsInt (m_iAspect, SCALEPREVIEW, &dwWidth, &dwHeight);
      BOOL fChanged;
      BOOL fRet = m_Transition.Dialog (&Window, m_hBmp, (fp)dwWidth / (fp)dwHeight,
         (m_iAspect == 10), fReadOnly, &fChanged);
      if (fChanged)
         m_fChanged = TRUE;
      if (fRet)
         goto redo;
      else
         goto done;
   }


done:
   if (m_hBmp)
      DeleteObject (m_hBmp);
   if (m_hBitEffect)
      DeleteObject (m_hBitEffect);
   if (m_pImage)
      delete m_pImage;
   return m_fChanged;
}


/***********************************************************************************
CRenderScene::Push - Standard API from CProgressSocket
*/
BOOL CRenderScene::Push (float fMin, float fMax)
{
   if (m_pProgress)
      return m_pProgress->Push(fMin, fMax);
   else
      return FALSE;
}

/***********************************************************************************
CRenderScene::Pop - Standard API from CProgressSocket
*/
BOOL CRenderScene::Pop (void)
{
   if (m_pProgress)
      return m_pProgress->Pop();
   else
      return FALSE;
}


/***********************************************************************************
CRenderScene::Update - Standard API from CProgressSocket
*/
int CRenderScene::Update (float fProgress)
{
   if (m_pProgress)
      return m_pProgress->Update(fProgress);
   else
      return 0;
}


/***********************************************************************************
CRenderScene::WantToCancel - Standard API from CProgressSocket
*/
BOOL CRenderScene::WantToCancel (void)
{
   if (m_pProgress)
      return m_pProgress->WantToCancel();
   else
      return FALSE;
}


/***********************************************************************************
CRenderScene::CanRedraw - Standard API from CProgressSocket
*/
void CRenderScene::CanRedraw (void)
{
   // if doing 360 render and have callback then update
   if (m_pRAECallback && m_pRAE360) {
      // keep track of number of times CanRedraw() called
      m_dwRAERedrawNum++;

      // get the current longitude andlatitude
      m_pRAECallback->RS360LongLatGet (&m_pRAE360->m_f360Long, &m_pRAE360->m_f360Lat);

      // if doing painterly effect for a 360 then only do every other time
      if (m_fRAEIsPainterly && !(m_dwRAERedrawNum % 2))
         return;

      PCImageStore pImage = 
         (m_pRAEImage || m_pRAEFImage) ?     // BUGFIX - Make sure have image of one sort or another
            (PCImageStore) RenderApplyEffects (m_dwRenderShardTemp,
            m_pRAEImage, m_pRAEFImage, NULL, TRUE, FALSE, NULL) :
         NULL;
      if (pImage)
         m_pRAECallback->RS360Update (pImage);
   }
   // do nothing
}



// BUGBUG - the infinite light source adds about a 2cm buffer to the shadow buffer, so
// that hats don't cast shadows on the hair immediately underneath. I think this means
// the infinite light is taking too large an area to calculate the shadow for, since it
// should be much finer shadow resolution