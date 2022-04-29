/*********************************************************************************
main.cpp - Main controller for the application
Begun 31/8/2001
Copyright 2001 by Mike Rozak. All rights reserved
*/
#include <windows.h>
#include <zmouse.h>
#define  COMPILE_MULTIMON_STUBS     // so that works win win95
#include <multimon.h>
#include <commctrl.h>
#include <objbase.h>
#include <initguid.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\buildnum.h"
#include <crtdbg.h>


PCWorld     gapWorld[MAXRENDERSHARDS] = {NULL, NULL, NULL, NULL};              // world
PCSceneSet  gapSceneSet[MAXRENDERSHARDS] = {NULL, NULL, NULL, NULL};           // scene set attached to world
PSTR        gpszButtonLevelShown = "ButtonLevelShown";
PSTR        gpszCompressMax = "CompressMax";
DWORD       gdwButtonLevelShown = 50;   // help level shown // BUGFIX - Was 0, but users dont like
BOOL        gfCanModifyLibrary = TRUE; // set to true if can modify library

static BOOL gafInitA[MAXRENDERSHARDS] = {FALSE, FALSE, FALSE, FALSE};
static BOOL gafInitB[MAXRENDERSHARDS] = {FALSE, FALSE, FALSE, FALSE};
static BOOL gafInitC[MAXRENDERSHARDS] = {FALSE, FALSE, FALSE, FALSE};

CRITICAL_SECTION gcsM3DInitEnd;       // critical section for initialization

#define NUMMRU    9

static PWSTR gpszWorld = L"World";
static PWSTR gpszHouseView = L"HouseView";
char gszFileWantToOpen[256] = "";   // want to open this file
static PWSTR gpszProgramVersion = L"ProgramVersion";
static PSTR gpszCPSize = "CPSize";

/****************************************************************************
MRUListEnum - Fills in the MRU list, loading it in.

inputs
   PCListVariable       pMRU - Filled in with a list of ANSI strings for the
                        MRU list.
*/
void MRUListEnum (PCListVariable pMRU)
{
   pMRU->Clear();
   char szTemp[32], szName[512];

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   DWORD i;
   for (i = 0; i < NUMMRU; i++) {
      DWORD dwSize, dwType;
      dwSize = sizeof(szName);
      szName[0] = 0;

      sprintf (szTemp, "MRU%d", (int) i);

      if (ERROR_SUCCESS != RegQueryValueEx (hKey, szTemp, NULL, &dwType, (LPBYTE) szName, &dwSize))
         continue;
      if (dwType != REG_SZ)
         continue;
      if (!szName[0])
         continue;

      // add it
      pMRU->Add (szName, strlen(szName)+1);
   }

   RegCloseKey (hKey);
}


/****************************************************************************
MRUListAdd - Adds an item to the top of the MRU list.

inputs
   char     *pszFile - File
returns
   none
*/
void MRUListAdd (char *pszFile)
{
   CListVariable gMRU;
   MRUListEnum (&gMRU);

   // if name already exsits then remove it
   DWORD i;
   char *psz;
   for (i = gMRU.Num()-1; i < gMRU.Num(); i--) {
      psz = (char*) gMRU.Get(i);
      if (!_stricmp(psz, pszFile))
         gMRU.Remove (i);
   }

   // add it
   gMRU.Insert (0, pszFile, strlen(pszFile)+1);

   // delete old ones
   while (gMRU.Num() > NUMMRU)
      gMRU.Remove (NUMMRU);

   // write to the registry
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   char szTemp[32];
   for (i = 0; i < NUMMRU; i++) {
      psz = (char*) gMRU.Get(i);
      if (!psz)
         psz = "";

      sprintf (szTemp, "MRU%d", (int) i);
      RegSetValueEx (hKey, szTemp, 0, REG_SZ, (BYTE*) psz, (DWORD)strlen(psz)+1);
   }


   RegCloseKey (hKey);


}
static PWSTR gpszTextures = L"Textures";
static PWSTR gpszUserObjects = L"UserObjects";
static PWSTR gpszSceneSet = L"SceneSet";

/*****************************************************************************
FileSave - Saves the world and view information to a file.

inputs
   char     *pszFile - file name
returns
   BOOL - TRUE if success
*/
BOOL FileSave (DWORD dwRenderShard, char *pszFile)
{
   // sync the info back and forth
   gapSceneSet[dwRenderShard]->SyncFromWorld();
   gapSceneSet[dwRenderShard]->SyncToWorld();

   // make master MML node
   PCMMLNode2 pNode, pSub;
   CProgress Progress;
   Progress.Start (NULL, "Saving " M3DFILEEXT " file...");
   pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;

   Progress.Push (0, .4);

   // save the version
   CPoint pV;
   pV.Zero();
   pV.p[0] = BN_MAJOR;
   pV.p[1] = BN_MINOR;
   pV.p[2] = BN_BN;
   MMLValueSet (pNode, gpszProgramVersion, &pV);

   // convert the world to mml
   pSub = gapWorld[dwRenderShard]->MMLTo (&Progress);
   if (pSub) {
      pSub->NameSet (gpszWorld);
      pNode->ContentAdd (pSub);
   }
   Progress.Pop ();

   // save the scene set
   Progress.Push (.4, .5);
   pSub = gapSceneSet[dwRenderShard]->MMLTo (&Progress);
   if (pSub) {
      pSub->NameSet (gpszSceneSet);
      pNode->ContentAdd (pSub);
   }
   Progress.Pop ();

   // save the textures
   pSub = TextureCacheUserTextures (dwRenderShard, gapWorld[dwRenderShard]);
   if (pSub) {
      pSub->NameSet (gpszTextures);
      pNode->ContentAdd (pSub);
   }

   // save the user objects
   pSub = ObjectCacheUserObjects (gapWorld[dwRenderShard]);
   if (pSub) {
      pSub->NameSet (gpszUserObjects);
      pNode->ContentAdd (pSub);
   }

   // Save the views and their locations
   DWORD i;
   for (i = 0; i < ListPCHouseView()->Num(); i++) {
      PCHouseView pv = *((PCHouseView*) ListPCHouseView()->Get(i));
      pSub = pv->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszHouseView);
         pNode->ContentAdd (pSub);
      }
   }

   Progress.Update (.55);

   Progress.Push (.55, 1);
   WCHAR szTemp[256];
   MultiByteToWideChar (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));
   BOOL fRet = MMLFileSave (szTemp, &CLSID_FileHeaderNewer, pNode, 1, &Progress);
   Progress.Pop ();
   delete pNode;
   if (!fRet)
      return FALSE;

   // old code
   // conver to a string
//   CMem memANSI;
//   Progress.Push (.55, .95);
//   DWORD dwRet;
//#ifdef OLDFILES
//   dwRet = MMLCompressToANSI(pNode, &Progress, &memANSI);
//#else
//   dwRet = MMLCompressToToken(pNode, &Progress, &memANSI, FALSE);
      // NOTE - Eventually pass TRUE to dont encode binary
//#endif
//   Progress.Pop();
//   delete pNode;
   
//   if (dwRet)
//      return FALSE;

   // conver to a string
   //CMem mem;
   // write out
   //if (!MMLToMem (pNode, &mem, TRUE)) {
   //   delete pNode;
   //   return FALSE;
   //}
   // findally, delete node

   //Progress.Update (.90);

   // convert from unicode to ANSI
   //CMem memANSI;
   //if (!memANSI.Required (mem.m_dwCurPosn+2))
   //   return FALSE;
   //int iLen;
   //iLen = WideCharToMultiByte (CP_ACP, 0, (PWSTR) mem.p, -1, (char*) memANSI.p, memANSI.m_dwAllocated, 0, 0);
   // memANSI.m_dwCurPosn = iLen

//   Progress.Update (.95);

//   PMEGAFILE f;
//   f = MegaFileOpen (pszFile, FALSE);
//   if (!f)
//      return FALSE;
//#ifdef OLDFILES
//   MegaFileWrite ((PVOID) &CLSID_FileHeaderOld, sizeof(GUID), 1, f);
//#else
//   MegaFileWrite ((PVOID) &CLSID_FileHeaderNew, sizeof(GUID), 1, f);
//#endif
//   MegaFileWrite (memANSI.p, 1, memANSI.m_dwCurPosn, f);
//   MegaFileClose (f);

   // save to MRU list
   MRUListAdd (pszFile);

   return TRUE;
}

/*****************************************************************************
M3DFileOpenRemoveUser - This function reads in a file, removes the user library
data (such as textures and models), and then writes the file to a new location,
without the user library data.

inputs
   PWSTR       pszFileSource - File name, original
   PCMegaFile  pmfDest - Megafile to write it to. If NULL, then ordinary write.

               NOTE: If a megafile is used, the file create, wwrite, and access
               time as written into the megafile will be the same as the original file.

   PWSTR       pszFileDest - Detination file name
   PCLibrary   pLibraryUserClone - From LibraryUserClone(). Where the library
                  elements are to be put
returns
   BOOL - TRUE if success
*/
BOOL M3DFileOpenRemoveUser (DWORD dwRenderShard, PWSTR pszFileSource, PCMegaFile pmfDest,
                            PWSTR pszFileDest, PCLibrary pLibraryUserClone)
{
   // get file time
   HANDLE hf = CreateFileW(pszFileSource, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
   if (hf == INVALID_HANDLE_VALUE)
      return FALSE;
   BY_HANDLE_FILE_INFORMATION bhi;
   GetFileInformationByHandle (hf, &bhi);
   CloseHandle (hf);


   PCMMLNode2 pNode;
   pNode = MMLFileOpen (pszFileSource, &CLSID_FileHeaderNewer, NULL);
   if (!pNode)
      return FALSE;


   PWSTR psz;
   PCMMLNode2 pSub;

   // load in the textures
   pSub = NULL;
   DWORD dwIndex = pNode->ContentFind (gpszTextures);
   pNode->ContentEnum (dwIndex, &psz, &pSub);
   if (pSub) {
      TextureUnCacheUserTextures (dwRenderShard, pSub, NULL, pLibraryUserClone);
      pNode->ContentRemove (dwIndex);
   }

   // load in the user objects first if can find
   pSub = NULL;
   dwIndex = pNode->ContentFind (gpszUserObjects);
   pNode->ContentEnum (dwIndex, &psz, &pSub);
   if (pSub) {
      ObjectUnCacheUserObjects (dwRenderShard, pSub, NULL, pLibraryUserClone);
      pNode->ContentRemove (dwIndex);
   }

   // save it out
   BOOL fRet;
   if (pmfDest)
      fRet = MMLFileSave (pmfDest, pszFileDest, &CLSID_FileHeaderNewer, pNode, 1, NULL,
         &bhi.ftCreationTime, &bhi.ftLastWriteTime, &bhi.ftLastAccessTime);
   else
      fRet = MMLFileSave (pszFileDest, &CLSID_FileHeaderNewer, pNode);
   delete pNode;

   return fRet;
}

/*****************************************************************************
M3DFileOpen - Opens the file and creates new views as specified in the file.
   NOTE: Caller should probably clear old views first.

inputs
   PWSTR    pszFile - file name
   BOOL     *pfFailedToLoad - Set to TRUE if unknown objects found in the library
   PCProgressSocket     pProgress - Progress bar to use. If NULL then creates own
   BOOL     fViewLoad - If TRUE then load in the views
   PCRenderTraditional *ppRender - If not NULL, then a new CRenderTraditional object
                        is created and filled with the rendering information of the
                        first view. This is then filled into ppRender.
returns
   BOOL - TRUE if success
*/
DLLEXPORT BOOL M3DFileOpen (DWORD dwRenderShard, PWSTR pszFile, BOOL *pfFailedToLoad, PCProgressSocket pProgress,
                            BOOL fViewLoad, PCRenderTraditional *ppRender)
{
#if 0 // def _DEBUG
   // to test
   PCLibrary pLibraryClone = LibraryUserClone (L"c:\\test.me3");
   CMegaFile mf;
   mf.Init (L"c:\\megafile.txt", &CLSID_Ground);
   M3DFileOpenRemoveUser (pszFile, &mf, L"c:\\test.m3d", pLibraryClone);
   delete pLibraryClone;
#endif

   if (ppRender)
      *ppRender = NULL;

   CProgress   Progress;
   if (!pProgress) {
      pProgress = &Progress;
      Progress.Start (NULL, "Opening " M3DFILEEXT " file...", TRUE);
   }

   *pfFailedToLoad = FALSE;

   PCMMLNode2 pNode;
   pProgress->Push (0, .1);
   pNode = MMLFileOpen (pszFile, &CLSID_FileHeaderNewer, pProgress);
   pProgress->Pop ();
#if 0        // dead code, for reading in old format
   if (!pNode) {
      // open and allocate enough memory
      CMem mem;
      PMEGAFILE f;
      f = MegaFileOpen (pszFile);
      if (!f)
         return FALSE;
      MegaFileSeek (f, 0, SEEK_END);
      DWORD dwLen;
      dwLen = MegaFileTell (f);
      MegaFileSeek (f, 0, SEEK_SET);

      if (!mem.Required (dwLen)) {
         MegaFileClose (f);
         return FALSE;
      }

      MegaFileRead (mem.p, 1, dwLen, f);

      MegaFileClose (f);


      // verify right type
      GUID *pg;
      pg = (GUID*) mem.p;
      BOOL fToken;
      if (IsEqualGUID(*pg, CLSID_FileHeaderOld))
         fToken = FALSE;
      else if (IsEqualGUID(*pg, CLSID_FileHeaderNew))
         fToken = TRUE;
      else
         return FALSE;  // not right type

      // get the node
      DWORD dwRet;
      pNode = NULL;
      pProgress->Push (0, .1);
      if (fToken)
         dwRet = MMLDecompressFromToken(pg+1, dwLen - sizeof(GUID), pProgress, &pNode, FALSE);
            // NOTE - Eventually pass TRUE to dont encode binary
      else
         dwRet = MMLDecompressFromANSI(pg+1, dwLen - sizeof(GUID), pProgress, &pNode);
      pProgress->Pop();
      if (dwRet)
         return FALSE;
   }
#endif // _DEBUG
   if (!pNode)
      return FALSE;

   // BUGFIX - get the version. If too new a version refuse to open
   CPoint pV;
   pV.Zero();
   MMLValueGetPoint (pNode, gpszProgramVersion, &pV);
   if (pV.p[0] > BN_MAJOR) {
      delete pNode;
      return FALSE;
   }

   // convert from ANSI to unicode
   //CMem memUni;
   //if (!memUni.Required (mem.m_dwAllocated * 2))
   //   return FALSE;
   //MultiByteToWideChar (CP_ACP, 0, (char*)(pg + 1), -1, (PWSTR) memUni.p, memUni.m_dwAllocated/2);

   // get from world
   //PCMMLNode2 pNode;
   //pNode = MMLFromMem((PWSTR) memUni.p);
   //if (!pNode)
   //   return FALSE;

   // update progress meter
   pProgress->Update (.1);
   
   // push sub
   pProgress->Push (.1, .8);

   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;

   // load in the textures
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszTextures), &psz, &pSub);
   if (pSub) {
      TextureUnCacheUserTextures (dwRenderShard, pSub, NULL, NULL);
      // BUGBUG - because passing NULL for hWnd, get screwy windows when report the error.
      // need to pass in a real HWND
   }

   // load in the user objects first if can find
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszUserObjects), &psz, &pSub);
   if (pSub) {
      ObjectUnCacheUserObjects (dwRenderShard, pSub, NULL, NULL);
      // BUGBUG - because passing NULL for hWnd, get screwy windows when report the error.
      // need to pass in a real HWND
   }

   // see what else can find
   for (i = 0; pNode->ContentNum(); i++) {
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         break;
      if (!pSub)
         continue;
      
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszWorld)) {
         // read in the world
         if (!gapWorld[dwRenderShard]->MMLFrom (pSub, pfFailedToLoad, pProgress)) {
            delete pNode;
            return FALSE;
         }
      }
   }
   // views
   pProgress->Pop();

   // load in the scenes
   pProgress->Push (.8, .85);
   gapSceneSet[dwRenderShard]->Clear();
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszSceneSet), &psz, &pSub);
   if (pSub) {
      if (!gapSceneSet[dwRenderShard]->MMLFrom (pSub, pProgress)) {
         delete pNode;
         return FALSE;
      }
   }

   // views
   pProgress->Pop();
   pProgress->Push (.85, 1);

   // go through again for the views
   for (i = 0; pNode->ContentNum(); i++) {
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         break;
      if (!pSub)
         continue;
      
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszHouseView)) {

         if (fViewLoad) {
            PCHouseView phv;
            phv = new CHouseView (gapWorld[dwRenderShard], gapSceneSet[dwRenderShard], VIEWWHAT_WORLD);
            phv->MMLFrom (pSub); // dont need to call init since MMLForm is sub
         }

         // if we want some render info, but dont have one loaded yet, then
         // get it fromr a house view
         if (ppRender && !(*ppRender)) {
            PCMMLNode2 pRender = NULL;
            //DWORD i;
            //for (i = 0; i < pSub->ContentNum(); i++) {
            //   pSub->ContentEnum (i, &psz, &pRender);
            //   if (pRender->NameGet())
            //      OutputDebugStringW (pRender->NameGet());
            //}
            pSub->ContentEnum (pSub->ContentFind (L"Render"), &psz, &pRender);
            if (pRender) {
               *ppRender = new CRenderTraditional(dwRenderShard);
               if (*ppRender)
                  (*ppRender)->MMLFrom (pRender);
            }
         }
         continue;
      }

      // show progress for each of these
      pProgress->Update ((fp) i / (fp)pNode->ContentNum());
   }
   pProgress->Pop(); // BUGFIX - missing one pop in open
   delete pNode;

   // rebuild color list
   TextureRebuildTextureColorList (dwRenderShard);

   // set some info
   gapWorld[dwRenderShard]->NameSet (pszFile);
   gapWorld[dwRenderShard]->UndoClear(TRUE, TRUE);
   gapSceneSet[dwRenderShard]->UndoClear (TRUE, TRUE);
   gapWorld[dwRenderShard]->m_fDirty = FALSE;


   return TRUE;
}

/*****************************************************************************
M3DFileNew - Intialize the world to the starting information.

inputs
   BOOL        fCreateGroundSky - If TRUE then automatically fill the world
                  with ground and sky.
*/
void M3DFileNew (DWORD dwRenderShard, BOOL fCreateGroundSky)
{
   // BUGFIX - Was passing in TRUE before so kept settings from previously opened file
   gapWorld[dwRenderShard]->Clear(FALSE);
   gapSceneSet[dwRenderShard]->Clear();

   if (fCreateGroundSky) {
      // create the ground
      PCObjectGround pg;
      OSINFO OI;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_Ground;
      //OI.gSub = GUID_NULL;
      OI.dwRenderShard = dwRenderShard;
      pg = new CObjectGround(NULL, &OI);
      if (pg) {
         // BUGFIX - Set a name
         pg->StringSet (OSSTRING_NAME, L"Ground");

         gapWorld[dwRenderShard]->ObjectAdd (pg);
         pg->WorldSetFinished();
      }

      // create the skydome
      PCObjectSkydome ps;
      memset (&OI, 0, sizeof(OI));
      OI.gCode = CLSID_Skydome;
      OI.gSub = CLSID_SkydomeSub;
      OI.dwRenderShard = dwRenderShard;
      ps = new CObjectSkydome (NULL, &OI);
      if (ps) {
         ps->StringSet (OSSTRING_NAME, L"Skydome");
         gapWorld[dwRenderShard]->ObjectAdd (ps);
         ps->WorldSetFinished ();
      }
   }

   gapWorld[dwRenderShard]->NameSet (L"");
   gapWorld[dwRenderShard]->UndoClear(TRUE, TRUE);
   gapSceneSet[dwRenderShard]->UndoClear (TRUE, TRUE);
   gapWorld[dwRenderShard]->m_fDirty = FALSE;

   // rebuidl the texture and color list
   TextureRebuildTextureColorList(dwRenderShard);
}


/*****************************************************************************
FileOpenWithUI - Opens a file. If it fails to open the user is alerted and
a new world is created. If no views are opened then a default view is opened.
Also, sets the view's strings to the file.

NOTE: This has similarities to M3DFileOpen (used for external file open), so
if changing this code, may want to change other.


inputs
   char        *pszFile - file name. If this is NULL just creates a new file
returns
   BOOL - TRUE if opened the file
*/
BOOL FileOpenWithUI (DWORD dwRenderShard, char *pszFile)
{
   BOOL fOpen;
   BOOL fFailedToLoad = FALSE;
   WCHAR szw[256];
   MultiByteToWideChar (CP_ACP, 0, pszFile ? pszFile : "", -1, szw, sizeof(szw)/2);
   if (pszFile)
      fOpen = M3DFileOpen (dwRenderShard, szw, &fFailedToLoad, NULL, TRUE, NULL);
   else
      fOpen = FALSE;

   if (fOpen)
      // add this to the MRU list
      MRUListAdd (pszFile);


   if (!fOpen) {
      if (pszFile) {
         WCHAR szTemp[256];
         wcscpy (szTemp, APPSHORTNAMEW L" could not open ");
         MultiByteToWideChar (CP_ACP, 0, pszFile, -1, szTemp + wcslen(szTemp), 128);
         wcscat (szTemp, L".");

         EscMessageBox (NULL, ASPString(),
            szTemp,
            L"The file may not exist, or it may require a more recent version of "
            APPSHORTNAMEW L" to load. (You might look at http://www.mXac.com.au to "
            L"see if a newer version is out.) A new session will be started instead.",
            MB_ICONEXCLAMATION | MB_OK);
      }
      pszFile = NULL;

      // create a new world
      M3DFileNew (dwRenderShard, TRUE);
   }

   if (fFailedToLoad) {
      EscMessageBox (NULL, ASPString(),
         L"Unknown objects were found in the file.",
         L"Some of the objects in the file were unknown to this version of "
         APPSHORTNAMEW
         L". They haven't been loaded. "
         L"You may wish to install the latest version of "
         APPLONGNAMEW L" from "
         L"www.mXac.com.au and try reloading the file.",
         MB_ICONEXCLAMATION | MB_OK);
   }

   // BUGFIX - dont need this because moved into M3DFileNew and FileOpen()
   //gWorld.NameSet (szw);
   SetViewTitles (dwRenderShard);

   // if there aren't any views then add one
   if (!ListPCHouseView()->Num()) {
      PCHouseView phv;
      phv = new CHouseView (gapWorld[dwRenderShard], gapSceneSet[dwRenderShard], VIEWWHAT_WORLD);
      phv->Init();
   }

   // BUGFIX - dont need this because moved into M3DFileNew and FileOpen()
   //gWorld.UndoClear(TRUE, TRUE);
   //gapSceneSet[dwRenderShard]->UndoClear (TRUE, TRUE);
   //gWorld.m_fDirty = FALSE;

   // done
   return fOpen;
}


/*****************************************************************************
CloseAllViewsDown - Shuts down all the views. NOTE: The last view to do this
will sent a PostQuitMessage().
*/
void CloseAllViewsDown (void)
{
   while (ListPCHouseView()->Num()) {
      PCHouseView pv = *((PCHouseView*) ListPCHouseView()->Get(0));
      delete pv;
   }
}



/*****************************************************************************
MakeWindowText - Looks at the current file name and makes up a title.

inputs
   char     *psz - Filled with the title. Should be 256 chars
*/
void MakeWindowText (DWORD dwRenderShard, char *psz)
{
   char  szTemp[256], szFile[256];
   WideCharToMultiByte (CP_ACP, 0, gapWorld[dwRenderShard]->NameGet(), -1, szTemp, sizeof(szTemp), 0,0);
   if (szTemp[0]) {
      char *psz;
      for (psz = szTemp; strchr(psz, '\\'); psz = strchr(psz, '\\') + 1);
      strcpy (szFile, psz);
   }
   else
      strcpy (szFile, "New file");

   sprintf (psz, "%s - " APPSHORTNAME, szFile);
}


/*****************************************************************************
SetViewTitles - Sets the titles of all the views.
*/
void SetViewTitles (DWORD dwRenderShard)
{
   char szTemp[256];
   MakeWindowText (dwRenderShard, szTemp);

   DWORD i;
   for (i = 0; i < ListPCHouseView()->Num(); i++) {
      PCHouseView pv = *((PCHouseView*) ListPCHouseView()->Get(i));
      SetWindowText (pv->m_hWnd, szTemp);
      pv->InvalidateDisplay();   // so redraws the file name in main window
   }
}


/*****************************************************************************
M3DWorldSwapIn - Swaps in a world that was swapped out.

NOTE: THe swapped in world WILL be deleted if load is called, or M3D is shut down.

inputs
   PCWorld        pWorld - WOrld to swap in
   PCSceneSet     pScene - Scene to swap in, associated with world
returns
   none
*/
void M3DWorldSwapIn (PCWorld pWorld, PCSceneSet pScene)
{
   DWORD dwRenderShard = pWorld->RenderShardGet();

   // if there's a world already then delete it
   if (gapWorld[dwRenderShard])
      delete gapWorld[dwRenderShard];
   if (gapSceneSet[dwRenderShard])
      delete gapSceneSet[dwRenderShard];

   gapWorld[dwRenderShard] = pWorld;
   gapSceneSet[dwRenderShard] = pScene;
   WorldSet (dwRenderShard, gapWorld[dwRenderShard], gapSceneSet[dwRenderShard]);
}


/*****************************************************************************
M3DWorldSwapOut - Swaps out a world that was swapped in. As soon as a world
is swapped out a blank one is created in its place.

NOTE: The caller will have to delete the swapped out world and scene.

inputs
   PCSceneSet     *ppScene - Filled with the scene
returns
   PCWorld - Current world
*/
PCWorld M3DWorldSwapOut (DWORD dwRenderShard, PCSceneSet* ppScene)
{
   PCWorld pRet = gapWorld[dwRenderShard];
   *ppScene = gapSceneSet[dwRenderShard];

   gapWorld[dwRenderShard] = new CWorld;
   gapWorld[dwRenderShard]->RenderShardSet (dwRenderShard);
   gapSceneSet[dwRenderShard] = new CSceneSet;
   WorldSet (dwRenderShard, gapWorld[dwRenderShard], gapSceneSet[dwRenderShard]);

   return pRet;
}


/*****************************************************************************
M3DInitEndCount - Count the number of times that M3DInit/End have been called
for each of the rendershards.

inputs
   BOOL       *pafInitialized - Array of MAXRENDERSHARDS bools
returns
   DWORD - Count
*/
DWORD M3DInitEndCount (BOOL *pafInitializaed)
{
   DWORD dwSum = 0;
   DWORD i;
   for (i = 0; i < MAXRENDERSHARDS; i++)
      if (pafInitializaed[i])
         dwSum++;

   return dwSum;
}

/*****************************************************************************
M3DInit - Initializes M3D.

NOTE: This assumes that EscIntialize() has been called.

inputs
   BOOL                 fLibraryAppDir - If TRUE then the libraries are loaded
                        from the app's dir. If FALSE then read registry and load
                        from whever m3d.exe is.
   PCProgressSocket     pProgress - Progress. If not specified one will be created.
returns
   BOOL - TRUE if success
*/
DLLEXPORT BOOL M3DInit (DWORD dwRenderShard, BOOL fLibraryAppDir, PCProgressSocket pProgress)
{
   // BUGFIX - globals
   if (!gapWorld[dwRenderShard]) {
      gapWorld[dwRenderShard] = new CWorld;
      gapWorld[dwRenderShard]->RenderShardSet (dwRenderShard);
   }
   if (!gapSceneSet[dwRenderShard])
      gapSceneSet[dwRenderShard] = new CSceneSet;

   // start up progress if there isn't one
   CProgress Progress;
   if (!pProgress) {
      pProgress = &Progress;
      Progress.Start (NULL, "Loading " APPLONGNAME "...", TRUE);
   }

   EnterCriticalSection (&gcsM3DInitEnd);
   DWORD dwCount = M3DInitEndCount (gafInitA);
   if (!dwCount) {
      //gdwButtonLevelShown = KeyGet (gpszButtonLevelShown, 0);
      RendCPSizeSet (KeyGet (gpszCPSize, 1));
      MMLCompressMaxSet (KeyGet(gpszCompressMax, (DWORD) 0));

      ASPHelpInit ();
   }
   gafInitA[dwRenderShard] = TRUE;
   LeaveCriticalSection (&gcsM3DInitEnd);

   // do worldset
   WorldSet (dwRenderShard, gapWorld[dwRenderShard], gapSceneSet[dwRenderShard]);


   // initialize the textures
   pProgress->Push (0, .8);
   LibraryOpenFiles(dwRenderShard, fLibraryAppDir, pProgress);
   pProgress->Pop ();

   pProgress->Push (.8, .9); // BUGFIX - smaller progress

   EnterCriticalSection (&gcsM3DInitEnd);
   dwCount = M3DInitEndCount (gafInitB);
   if (!dwCount) {
      ThumbnailOpenFile(pProgress);
   }
   gafInitB[dwRenderShard] = TRUE;
   LeaveCriticalSection (&gcsM3DInitEnd);

   pProgress->Update (1);
   pProgress->Pop ();

   pProgress->Push (.9, 1);   // BUGFIX - Smaller progress
   TextureCacheInit (dwRenderShard, pProgress);
   pProgress->Update (1);
   pProgress->Pop ();

   EnterCriticalSection (&gcsM3DInitEnd);
   dwCount = M3DInitEndCount (gafInitC);
   if (!dwCount) {
      ObjectCFInit();
   }
   gafInitC[dwRenderShard] = TRUE;
   LeaveCriticalSection (&gcsM3DInitEnd);

   // connect the sceneset to the world
   gapSceneSet[dwRenderShard]->WorldSet (gapWorld[dwRenderShard]);

   return TRUE;
}



/*****************************************************************************
M3DEnd - Shuts down M3D stuff.

inputs
   PCProgressSocket     pProgress - Progress. If not specified one will be created.
returns
   BOOL - TRUE if success
*/
DLLEXPORT BOOL M3DEnd (DWORD dwRenderShard, PCProgressSocket pProgress)
{
   // start up progress if there isn't one
   CProgress Progress;
   if (!pProgress) {
      pProgress = &Progress;
      Progress.Start (NULL, "Closing " APPLONGNAME "...");
   }

   EnterCriticalSection (&gcsM3DInitEnd);
   gafInitA[dwRenderShard] = FALSE;
   DWORD dwCount = M3DInitEndCount (gafInitA);
   if (!dwCount) {
      ButtonBitmapReleaseAll ();
      ASPHelpEnd ();
   }
   LeaveCriticalSection (&gcsM3DInitEnd);


//done:



   // disconnect the scene from the world
   gapSceneSet[dwRenderShard]->WorldSet (NULL);
   gapSceneSet[dwRenderShard]->m_fKeepUndo = FALSE;
   gapSceneSet[dwRenderShard]->Clear ();
   gapSceneSet[dwRenderShard]->UndoClear (TRUE, TRUE);

   // clear gworld of object here so dont detect as memory leak
   gapWorld[dwRenderShard]->m_fKeepUndo = FALSE;   // BUGFIX - Dont keep undo - will speed things up on shutdown
   gapWorld[dwRenderShard]->Clear(FALSE);
   gapWorld[dwRenderShard]->UndoClear (TRUE, TRUE);

   // free up the textures
   pProgress->Push (0, .8);
   if (!gfCanModifyLibrary) {
      // BUGIFX - since not allowed to modify libraries because this is the
      // second copy of M3D running, make sure doesnt save
      gaLibraryUser[dwRenderShard].DirtySet (FALSE);
      gaLibraryInstalled[dwRenderShard].DirtySet (FALSE);
   }
   LibrarySaveFiles (dwRenderShard, FALSE, pProgress);   // note: gett library file directory from registry
   pProgress->Pop ();


   EnterCriticalSection (&gcsM3DInitEnd);
   gafInitB[dwRenderShard] = FALSE;
   dwCount = M3DInitEndCount (gafInitB);
   if (!dwCount) {
      pProgress->Push (.8, .9);  // BUGFIX - smaler progress
      ThumbnailSaveFile(pProgress);
      pProgress->Pop ();
   }
   LeaveCriticalSection (&gcsM3DInitEnd);

   pProgress->Push (.9, 1);   // BUGFIX - smaller progress
   TextureCacheEnd (dwRenderShard, pProgress);
   pProgress->Pop ();

   EnterCriticalSection (&gcsM3DInitEnd);
   gafInitC[dwRenderShard] = FALSE;
   dwCount = M3DInitEndCount (gafInitC);
   if (!dwCount) {
      ObjectCFEnd();

      //KeySet (gpszButtonLevelShown, gdwButtonLevelShown);
      KeySet (gpszCPSize, RendCPSizeGet());
      KeySet (gpszCompressMax, (DWORD)MMLCompressMaxGet()); // BUGFIX - Was KeyGet() by accident bugbug
   }
   LeaveCriticalSection (&gcsM3DInitEnd);



   // BUGFIX - globals
   if (gapWorld[dwRenderShard]) {
      delete gapWorld[dwRenderShard];
      gapWorld[dwRenderShard] = NULL;
   }
   if (gapSceneSet[dwRenderShard]) {
      delete gapSceneSet[dwRenderShard];
      gapSceneSet[dwRenderShard] = NULL;
   }

   return TRUE;
}

/*****************************************************************************
M3DMainLoop - DOes the main display loop for 3DOB

inputs
   LPSTR       lpCmdLine - command line
   int         nShowCmd - Show
*/
int M3DMainLoop (LPSTR lpCmdLine, int nShowCmd)
{
#ifdef _DEBUG
   // Get current flag
   int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

   // Turn on leak-checking bit
   tmpFlag |= _CRTDBG_LEAK_CHECK_DF; // | _CRTDBG_CHECK_ALWAYS_DF;
   //tmpFlag |= _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_DELAY_FREE_MEM_DF;

   // BUGFIX - Turn off the high values so dont check for leak every 10K
   // Do this to make things faster
   // tmpFlag &= ~(_CRTDBG_CHECK_EVERY_1024_DF);

   // Set flag to the new value
   _CrtSetDbgFlag( tmpFlag );

   // test
   //char *p;
   //p = (char*)MYMALLOC (42);
   // p[43] = 0;

   DWORD dwStartTime = GetTickCount();
#endif // _DEBUG


   // call this so scrollbars look better
   InitCommonControls ();

   // gdwMouseWheel = 0;
   // BUGFIX - Disable gdwMouseWheel = RegisterWindowMessage (MSH_MOUSEWHEEL); since
   // only needed for win95, which wont work anyway


   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);

   // expiration date
   BOOL fExpired;
   fExpired = Today() > TODFDATE(1,1,2010);
#ifndef _DEBUG
   EscMessageBox (NULL, ASPString(),
      L"This copy of " APPLONGNAMEW L" will expire on 1 Jan 2010.",
      L"You are running a pre-release of 3DOB. By Jan 1 there should be "
      L"another update.",
      MB_ICONINFORMATION | MB_OK);
#endif
   if (fExpired)
      exit (0);


   // BUGFIX - Only one instance of ASP modifying objects at once
   HANDLE hMutex;
   gfCanModifyLibrary = TRUE;
   hMutex = CreateMutex (NULL, TRUE, APPSHORTNAME " check for run once");
   if (GetLastError() == ERROR_ALREADY_EXISTS) {
      hMutex = NULL;
      gfCanModifyLibrary = FALSE;   // cant modify library
   }

   // init beep window
   BeepWindowInit ();

   if (!M3DInit (DEFAULTRENDERSHARD, FALSE, NULL))
      goto done;

   if (lpCmdLine && !_stricmp(lpCmdLine, "-tutorial")) {
      gszFileWantToOpen[0] = 0;
      ASPHelp (IDR_HTUTORIALSTART);
   }
   else if (lpCmdLine && lpCmdLine[0])
      strcpy (gszFileWantToOpen, lpCmdLine);


open:
   // start up with blank world, or open a file
   if (gszFileWantToOpen[0] == '*')
      gszFileWantToOpen[0] = 0;  // special signal sent for file new
   FileOpenWithUI (DEFAULTRENDERSHARD, gszFileWantToOpen[0] ? gszFileWantToOpen : NULL);
   gszFileWantToOpen[0] = 0;

#ifdef _DEBUG
   char szTemp[64];
   sprintf (szTemp, "\r\n3DLoad: %d ms\r\n", (int)(GetTickCount()-dwStartTime));
   OutputDebugString (szTemp);
#endif

   // window loop
   MSG msg;
   while( GetMessage( &msg, NULL, 0, 0 ) ) {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
      ASPHelpCheckPage ();
      ListViewCheckPage ();
   }
 
   if (gszFileWantToOpen[0])
      goto open;

done:
   M3DEnd (DEFAULTRENDERSHARD, NULL);

   // dlete beep window
   BeepWindowEnd ();

   // free lexicon and tts
   TTSCacheShutDown();
   MLexiconCacheShutDown (TRUE);

   RLEND();
   EscUninitialize();
   TraceCacheClear ();

#ifdef _DEBUG
   _CrtCheckMemory ();
#endif // DEBUG

   if (hMutex)
      ReleaseMutex (hMutex);

   return 0;
}



/*****************************************************************************
CanModifyLibrary - Returns TRUE if can modify library (because dont have two
instances open)
*/
BOOL CanModifyLibrary (void)
{
   return gfCanModifyLibrary;
}



/*****************************************************************************
MainFileWantToOpen - Returns a pointer to the file that want to open.
*/
char *MainFileWantToOpen (void)
{
   return gszFileWantToOpen;
}

