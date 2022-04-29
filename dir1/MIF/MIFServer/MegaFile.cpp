/*************************************************************************************
MagaFile.cpp - Handes writing out and caching of the megafile.

begun 9/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"


PWSTR gpszProjectFile = L"TheProject.mfp";

/*************************************************************************************
MegaFileIncludeM3DLibrary - Copies the M3D library into the megafile.

inputs
   PWSTR             pszLib - M3D library name
   PCMegaFile        pMega - Mega file to write into
   PCBTree           ptFiles - Filled with all the files in the library megafile
returns
   BOOL - TRUE if success
*/
BOOL MegaFileIncludeM3DLibrary (PWSTR pszLib, PCMegaFile pMega, PCBTree ptFiles)
{
   CMegaFile mf;
   if (!mf.Init (pszLib, &GUID_LibraryHeaderNewer, FALSE))
      return FALSE;

   // enumerate all the contents
   CListVariable lName;
   CListFixed lMFFILEINFO;
   lMFFILEINFO.Init (sizeof(MFFILEINFO));
   mf.Enum (&lName, &lMFFILEINFO);

   // go through and copy them all over
   DWORD i;
   PMFFILEINFO pmfi = (PMFFILEINFO) lMFFILEINFO.Get(0);
   for (i = 0; i < lName.Num(); i++, pmfi++) {
      // add this file to the list
      ptFiles->Add ((PWSTR)lName.Get(i), 0, 0);

      __int64 iSize;
      PBYTE pData = (PBYTE) mf.Load ((PWSTR)lName.Get(i), &iSize);
      if (!pData)
         continue;   // shouldnt happen

      // write it in
      BOOL fRet = pMega->Save ((PWSTR)lName.Get(i), pData, iSize, &pmfi->iTimeCreate,
         &pmfi->iTimeModify, &pmfi->iTimeAccess);
      MegaFileFree (pData);
      if (!fRet)
         return FALSE;
   } // i

   return TRUE;
}


/*************************************************************************************
MegaFileExploreM3DLibrary - Explores the M3D files for sub-file references.
It also looks for a difference in modify times.

NOTE: There is a chance that a file that these points to is changed and the
actual library entry has the same time-stamp, but this is highly unlikely.
Therefore, don't bother to update tFiles.

inputs
   PWSTR             pszLib - M3D library name
   PCMegaFile        pMega - Mega file to compare modify times to.
   PCBTree           ptFiles - Filled with all the files in the library megafile
returns
   BOOL - TRUE if need to reaload the libary, FALSE if not
*/
BOOL MegaFileExploreM3DLibrary (PWSTR pszLib, PCMegaFile pMega, PCBTree ptFiles)
{
   CMegaFile mf;
   if (!mf.Init (pszLib, &GUID_LibraryHeaderNewer, FALSE))
      return FALSE;

   // enumerate all the contents
   CListVariable lName;
   CListFixed lMFFILEINFO;

   // enumerate the contents of the megafile into a tree
   CBTree tMega;
   DWORD i;
   lName.Clear();
   lMFFILEINFO.Init (sizeof(MFFILEINFO));
   pMega->Enum (&lName, &lMFFILEINFO);
   PMFFILEINFO pmfi = (PMFFILEINFO) lMFFILEINFO.Get(0);
   for (i = 0; i < lName.Num(); i++, pmfi++)
      tMega.Add ((PWSTR)lName.Get(i), pmfi, sizeof(*pmfi));

   // enumearte the contents of the library
   lName.Clear();
   lMFFILEINFO.Init (sizeof(MFFILEINFO));
   mf.Enum (&lName, &lMFFILEINFO);

   // fill in all the files from the library file
   for (i = 0; i < lName.Num(); i++)
      ptFiles->Add ((PWSTR)lName.Get(i), 0, 0);

   // go through and check them out
   pmfi = (PMFFILEINFO) lMFFILEINFO.Get(0);
   for (i = 0; i < lName.Num(); i++, pmfi++) {
      // if this doesn't exist in the megafile, or it's date has changed then
      PMFFILEINFO pFind = (PMFFILEINFO) tMega.Find ((PWSTR) lName.Get(i));
      if (!pFind || (pFind->iDataSize != pmfi->iDataSize) ||
         memcmp(&pFind->iTimeCreate, &pmfi->iTimeCreate, sizeof(pmfi->iTimeCreate)) ||
         memcmp(&pFind->iTimeModify, &pmfi->iTimeModify, sizeof(pmfi->iTimeModify)) )
            return TRUE;   // time changed or not there

   } // i

   return FALSE;
}

/*************************************************************************************
MegaFileWriteTitleInfo - Writes the title info into the file.

inputs
   PWSTR             pszFile - File name to save as
   PWSTR             pszFileCRK - Link to Circumreality to save as
   PCMIFLProj        pProj - Project. Used to seed the megafile
returns
   PWSTR - NULL if success, else text string for why failed
*/
PWSTR MegaFileWriteTitleInfo (PWSTR pszFile, PWSTR pszFileCRK, PCMIFLProj pProj)
{
   // title info
   // finally, find the CircumrealityTitleInfo resource and use that
   DWORD i, j;
   PCMIFLResource pRes = NULL;
   for (i = 0; !pRes && (i < pProj->LibraryNum()); i++) {
      PCMIFLLib pLib = pProj->LibraryGet(i);
      for (j = 0; j < pLib->ResourceNum(); j++) {
         PCMIFLResource pr = pLib->ResourceGet(j);
         if (_wcsicmp((PWSTR)pr->m_memType.p, CircumrealityTitleInfo()))
            continue;
         
         // find the one named "rTitleInfo"
         if (_wcsicmp ((PWSTR)pr->m_memName.p, L"rTitleInfo"))
            continue;
         
         // else, found it
         pRes = pr;
         break;
      } // j
   } // i
   if (!pRes)
      return L"You need to provide a rTitleInfo resource.";
   PCMMLNode2 pNode;
   CResTitleInfoSet ris;
   if (!ris.FromResource (pRes))
      return L"Couldn't create ResTitleInfoSet.";
   pNode = ris.MMLTo();
   if (!pNode)
      return L"Couldn't create MML.";

   if (pszFile) {
      CMegaFile Mega;
      if (!Mega.Init (pszFile, &GUID_MegaCircumreality, FALSE)) {
         delete pNode;
         return L"Couldn't create the file.";  // couldnt create the megafile
      }
      if (!MMLFileSave (&Mega, CircumrealityTitleInfo(), &CLSID_MegaTitleInfo, pNode)) {
         delete pNode;
         return L"Couldn't save TitleInfo.";
      }
   }

   if (pszFileCRK) {
      CMegaFile MegaCRK;
      if (!MegaCRK.Init (pszFileCRK, &GUID_MegaCircumrealityLink, TRUE)) {
         delete pNode;
         return L"Couldn't create the file.";  // couldnt create the megafile
      }
      if (!MMLFileSave (&MegaCRK, CircumrealityTitleInfo(), &CLSID_MegaTitleInfo, pNode)) {
         delete pNode;
         return L"Couldn't save TitleInfo.";
      }
   }
   delete pNode;

   return NULL;
}

/*************************************************************************************
MegaFileGenerate - This generates the entire megafile from the seed files
in the project.

inputs
   PWSTR             pszFile - File name to save as
   PWSTR             pszFileCRK - Link to Circumreality to save as
   PCMIFLProj        pProj - Project. Used to seed the megafile
   PCProgressSocket  pProgress - Where to show progress
returns
   PWSTR - NULL if success, else text string for why failed
*/
PWSTR MegaFileGenerate (PWSTR pszFile, PWSTR pszFileCRK, PCMIFLProj pProj, PCProgressSocket pProgress)
{
   CBTree tFiles;
   CListVariable lSeedName;
   CListFixed lSeedInfo;
   PWSTR fRet = NULL;

   // create ANSI version of file
   char szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);

   // get the file size of whatever is there now...
   LARGE_INTEGER li;
   memset (&li, 0, sizeof(li));
   HANDLE hf = CreateFile(szTemp, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
   if (hf != INVALID_HANDLE_VALUE) {
      GetFileSizeEx (hf, &li);
      CloseHandle (hf);
   }
   __int64 iBytesExpected = *((__int64*)&li);
   iBytesExpected = max(iBytesExpected, 20000000);  // at least something

   // delete it just in case
   DeleteFile (szTemp);

   // create the megafile
   CMegaFile Mega;
   if (!Mega.Init (pszFile, &GUID_MegaCircumreality, TRUE))
      return L"Couldn't create the file.";  // couldnt create the megafile

   // set the megafile overrides
   MegaFileSet (&Mega, NULL, NULL);

   // save the project into the megafile
   WCHAR szOldFile[256];   // BUGFIX - change file name
   wcscpy (szOldFile, pProj->m_szFile);
   wcscpy (pProj->m_szFile, gpszProjectFile);
   fRet = pProj->Save (TRUE, FALSE) ? NULL : L"Couldn't save the project.";
   wcscpy (pProj->m_szFile, szOldFile);
   if (fRet)
      goto done;

   // whatever have here is the seed base, so enumearte it
   Mega.Enum (&lSeedName, &lSeedInfo);

   // add all these to the tree
   DWORD i;
   for (i = 0; i < lSeedName.Num(); i++)
      tFiles.Add ((PWSTR) lSeedName.Get(i), 0, 0);

   // add the user and standard libraries
   char szLibTemp[256];
   WCHAR szw[256];
   // installed library
   M3DLibraryDir (szLibTemp, sizeof(szLibTemp));
   strcat (szLibTemp, "LibraryInstalled.me3");
   MultiByteToWideChar (CP_ACP, 0, szLibTemp, -1, szw, sizeof(szw)/sizeof(WCHAR));
   //tFiles.Add (szw, 0, 0);
   MegaFileIncludeM3DLibrary (szw, &Mega, &tFiles);

   // user library
   // BUGFIX - Dont do user library here. Done later on
   //M3DLibraryDir (szLibTemp, sizeof(szLibTemp));
   //strcat (szLibTemp, "LibraryUser.me3");
   //MultiByteToWideChar (CP_ACP, 0, szLibTemp, -1, szw, sizeof(szw)/sizeof(WCHAR));
   // //tFiles.Add (szw, 0, 0);
   //MegaFileIncludeM3DLibrary (szw, &Mega, &tFiles);

   // create a temporary file for user lex
   WCHAR szTempPath[256], szTempFile[256];
   GetTempPathW (sizeof(szTempPath)/sizeof(WCHAR), szTempPath);
   GetTempFileNameW (szTempPath, L"usl", 0, szTempFile);
   PCLibrary pLibrary = LibraryUserClone (DEFAULTRENDERSHARD, szTempFile);

   // loop through pre and post user library
   // Do two passes. First one is most of the files. Second pass includes
   // user-objects that were stored in M3D files from the first pass.
   DWORD dwPostLibrary;
   PWSTR pszM3DSuffix = L".m3d";
   DWORD dwM3DSuffixLen = (DWORD)wcslen(pszM3DSuffix);
   DWORD dwLast = 0;
   for (dwPostLibrary = 0; dwPostLibrary < 2; dwPostLibrary++) {
      DWORD dwInitial = tFiles.Num();

      // loop through the tree, looking up dependencies
      for (i = dwLast; i < tFiles.Num(); i++) {
         // update the progress
         if (pProgress) {
            LARGE_INTEGER li;
            HANDLE hf = CreateFile(szTemp, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf != INVALID_HANDLE_VALUE) {
               GetFileSizeEx (hf, &li);
               CloseHandle (hf);

               double f = (double)(*((__int64*)&li)) / (double)iBytesExpected;
               f = min(f,1);
               pProgress->Update (f);
            }
         }

         PWSTR pszFile = tFiles.Enum(i);

         // copy it over to the megafile
         if (i >= dwInitial) {
            // if this ends with .m3d then do a special save that pulls out object and texture
            // definitions and puts them into the user library
            DWORD dwLen = (DWORD)wcslen(pszFile);
            if (pLibrary && (dwLen > dwM3DSuffixLen) && !_wcsicmp(pszFile + (dwLen - dwM3DSuffixLen), pszM3DSuffix)) {
               MegaFileSet (NULL, NULL, NULL);  // so no megafiles
               BOOL f = M3DFileOpenRemoveUser (DEFAULTRENDERSHARD, pszFile, &Mega, pszFile, pLibrary);
               MegaFileSet (&Mega, NULL, NULL);
               if (!f)
                  continue;
            }
            else {
               // normal save over
               if (!Mega.Save (pszFile))
                  continue;
            }
         }

         // see if can extract information from it about what it references
         PCMMLNode2 pNode = MMLFileOpen (pszFile, NULL, NULL);
         if (!pNode)
            continue;   // nothing to extract from here

         // BUGFIX - If this is a .mlx file, then ignore all TTS engine
         // references from within it
         DWORD dwFlags = 0;
         DWORD dwLen = (DWORD)wcslen(pszFile);
         if ((dwLen >= 4) && (!_wcsicmp(pszFile + (dwLen-4), L".mlx")))
            dwFlags |= EFFN_NOTTS;

#ifdef _DEBUG
         if ((dwLen >= 4) && (!_wcsicmp(pszFile + (dwLen-4), L".tts"))) {
            OutputDebugStringW (L"\r\nTTS = ");
            OutputDebugStringW (pszFile);
         }
#endif

         ExtractFilesFromNode (pNode, &tFiles, TRUE, dwFlags);
         delete pNode;
      } // i
      dwLast = tFiles.Num();

      if (!dwPostLibrary) {
         // save everything into the temp library
         delete pLibrary;
         pLibrary = NULL;

         // transfer this over
         MegaFileIncludeM3DLibrary (szTempFile, &Mega, &tFiles);

         // delete the temp file
         DeleteFileW (szTempFile);
      }
   } // dwPostLibrary

done:
   // close down the library and delete if anything left over (which there shouldnt be)
   if (pLibrary) {
      delete pLibrary;
      DeleteFileW (szTempFile);
   }

   // restore to no megafile
   MegaFileSet (NULL, NULL, NULL);
   if (fRet)
      return fRet;

   return MegaFileWriteTitleInfo (pszFile, pszFileCRK, pProj);

}

/*************************************************************************************
MegaFileGenerateIfNecessary - This regenerates the megafile if anything
it refers to has changed

NOTE: This will create a temporary library and merge all the existing libraries into it,
so that resources are overridden

inputs
   PWSTR             pszFile - File name to save as (for Circumreality)
   PWSTR             pszFileCRK - Link file name to save as
   PCMIFLProj        pProj - Project. Used to seed the megafile
   HWND              hWnd - to create progress off of
returns
   BOOL - TRUE if success. FALSE if error in generating megafile
*/
BOOL MegaFileGenerateIfNecessary (PWSTR pszFile, PWSTR pszFileCRK, PCMIFLProj pProj, HWND hWnd)
{
   BOOL fRetWhenDone = TRUE;
   CListFixed lPCMIFLLibSwap;
   WCHAR szwPath[256], szwTempLib[256];
   szwTempLib[0] = szwPath[0] = 0;

   // need to initialize 3d if not already initialized, so libraries loaded in
   if (!gfM3DInit) {
      CProgress Progress;
      Progress.Start (NULL, "Starting 3D...", TRUE);
      M3DInit (DEFAULTRENDERSHARD, FALSE, &Progress);
      MyCacheM3DFileInit(DEFAULTRENDERSHARD);
      gfM3DInit = TRUE;
   }


   CBTree tFiles;
   CListVariable lEnumName;
   CListFixed lEnumInfo;
   CBTree tEnum;


   {
      PWSTR pszM3DSuffix = L".m3d";
      DWORD dwM3DSuffixLen = (DWORD)wcslen(pszM3DSuffix);

      CProgress Progress;

      // BUGFIX - Need to merge libraries together so that resources can be properly overridden
      if (pProj->LibraryNum()) {
         // create a temporary library file
         GetTempPathW (sizeof(szwPath)/sizeof(WCHAR), szwPath);
         GetTempFileNameW  (szwPath, L"clib", 3, szwTempLib);

         PCMIFLLib pNew = new CMIFLLib(NULL);
         if (!pNew) {
            szwTempLib[0] = 0;   // so know
            goto skipmerge;
         }
         // not needed pNew->ProjectSet (pProj);


         // swap and merge
         lPCMIFLLibSwap.Init (sizeof(PCMIFLLib), &pNew, 1);
         pProj->LibrarySwap (&lPCMIFLLibSwap);
         pNew->Merge ((PCMIFLLib*)lPCMIFLLibSwap.Get(0), lPCMIFLLibSwap.Num(), FALSE);

         // save this to the temporary foler
         wcscpy (pNew->m_szFile, szwTempLib);
         pNew->Save (TRUE);
      }

skipmerge:


      // see if there's an existing megafile
      CMegaFile Mega;
      if (!Mega.Init (pszFile, &GUID_MegaCircumreality, FALSE))
         goto generate;

      // write out the titleinfo, just in case decides this isn't
      // dirty... the titleinfo may have been changed without
      // the dirty flag getting raised
      PWSTR psz;
      psz = MegaFileWriteTitleInfo (pszFile, pszFileCRK, pProj);
      if (psz) {
         EscMessageBox (hWnd, L"Generate Circumreality file", psz, NULL, MB_OK);
         fRetWhenDone = FALSE;
         goto cleanup;
      }

      Progress.Start (hWnd, "Scanning Circumreality file...", TRUE);

      // how many files?
      DWORD dwNumInMega;
      {
         CListVariable lName;
         CListFixed lMegaFile;
         Mega.Enum(&lName, &lMegaFile);
         dwNumInMega = lMegaFile.Num()+1;
      }

      // see if the standard M3D libraries have changed
      char szLibTemp[256];
      WCHAR szw[256];
      // installed library
      M3DLibraryDir (szLibTemp, sizeof(szLibTemp));
      strcat (szLibTemp, "LibraryInstalled.me3");
      MultiByteToWideChar (CP_ACP, 0, szLibTemp, -1, szw, sizeof(szw)/sizeof(WCHAR));
      if (MegaFileExploreM3DLibrary (szw, &Mega, &tFiles))
         goto generate;

      // user library
      // BUGFIX - Dont check to see if user library has changed since it will always
      // have changed given that we're generating it on the fly. This might cause
      // the file not to be rebuilt if the M3D user library has changed, but
      // shouldnt be much of an issue
      //M3DLibraryDir (szLibTemp, sizeof(szLibTemp));
      //strcat (szLibTemp, "LibraryUser.me3");
      //MultiByteToWideChar (CP_ACP, 0, szLibTemp, -1, szw, sizeof(szw)/sizeof(WCHAR));
      //if (MegaFileExploreM3DLibrary (szw, &Mega, &tFiles))
      //   goto generate;



      // set this as the current megafile and save the project. This will allow
      // for scanning
      MegaFileSet (&Mega, NULL, NULL);
      WCHAR szOldFile[256];   // BUGFIX - change file name
      wcscpy (szOldFile, pProj->m_szFile);
      wcscpy (pProj->m_szFile, gpszProjectFile);
      BOOL fRet = pProj->Save (TRUE, FALSE);
      wcscpy (pProj->m_szFile, szOldFile);
      if (!fRet) {
         MegaFileSet (NULL, NULL, NULL);
         fRetWhenDone = TRUE;
         goto cleanup;
      }

      // create a list of what the project and libraries would have been saved as
      tFiles.Add (gpszProjectFile, 0, 0); // BUGFIX - Was pProj->m_szFile
      DWORD i;
      for (i = 0; i < pProj->LibraryNum(); i++) {
         PCMIFLLib pLib = pProj->LibraryGet (i);
         if (pLib->m_fReadOnly)
            continue;   // built in

         tFiles.Add (pLib->m_szFile, 0, 0);
      } // i
      DWORD dwInitial = tFiles.Num();

      // loop through these files (in tFiles), and see what they're dependent upon
      for (i = 0; i < dwInitial; i++) {
         if ((i%10) == 0)
            Progress.Update ((fp)i / (fp)dwNumInMega);

         PWSTR pszFile = tFiles.Enum(i);

         // see if can extract information from it about what it references
         PCMMLNode2 pNode = MMLFileOpen (pszFile, NULL, NULL);
         if (!pNode)
            continue;   // nothing to extract from here
         ExtractFilesFromNode (pNode, &tFiles, TRUE, 0);
         delete pNode;
      } // i
      MegaFileSet (NULL, NULL, NULL);  // so no megafiles

      // enumerate the files in the megafile and convert into tree
      Mega.Enum (&lEnumName, &lEnumInfo);
      for (i = 0; i < lEnumName.Num(); i++)
         tEnum.Add ((PWSTR)lEnumName.Get(i), 0, 0);

      // if any dependencies that we generated from the project/libs is NOT
      // represented in the megafile then need to rebuild
      for (i = 0; i < tFiles.Num(); i++) {   // BUGFIX - Was dwInitial
         PWSTR pszFile = tFiles.Enum(i);
         if (!tEnum.Find (pszFile))
            goto generate;
      } // i

      // if any files in the megafile have a last modified time (or size) that's not the
      // same as the real thing then rebuild
      PMFFILEINFO pmfi = (PMFFILEINFO) lEnumInfo.Get(0);
      char szTemp[512];
      for (i = 0; i < tEnum.Num(); i++, pmfi++) {
         PWSTR pszFile = tEnum.Enum(i);
         if (tFiles.FindNum (pszFile) < dwInitial)
            continue;   // this is one of the libs/projects, so would expect to be changed

         // if it's a known file then ignore
         if (!_wcsicmp(pszFile, CircumrealityTitleInfo()))
            continue;

         WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
         HANDLE hf = CreateFile(szTemp, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
         if (hf == INVALID_HANDLE_VALUE)
            continue;
            // BUGFIX - Used to be generate, but might as well leave old/extra file
            // in. This is a useful hack since the user library files are stored
            // here, but wont show up on the tFiles < dwInitial list
            // goto  generate;

         // get info
         BY_HANDLE_FILE_INFORMATION bhi;
         GetFileInformationByHandle (hf, &bhi);
         CloseHandle (hf);

         // for M3D files, ignore size, since may be reduced. The creation/modify time
         // should be enough
         DWORD dwLen = (DWORD)wcslen(pszFile);
         if ((dwLen > dwM3DSuffixLen) && !_wcsicmp(pszFile + (dwLen-dwM3DSuffixLen), pszM3DSuffix))
            bhi.nFileSizeLow = (DWORD)pmfi->iDataSize;   // so dont re-generate based on size

         // if changed then re-generate
         if (bhi.nFileSizeLow != (DWORD)pmfi->iDataSize)
            goto generate;
         if (memcmp (&bhi.ftLastWriteTime, &pmfi->iTimeModify, sizeof(bhi.ftLastWriteTime)))
            goto generate;
         // BUGFIX - Also take into account creation time
         if (memcmp (&bhi.ftCreationTime, &pmfi->iTimeCreate, sizeof(bhi.ftCreationTime)))
            goto generate;

         // else, it's the same
      } // i
   } // include progress

   // if get here then there aren't any changes to the megafile that need to be
   // made
   fRetWhenDone = TRUE;
   goto cleanup;


generate:
   PWSTR psz;
   {
      CProgress Progress;
      WCHAR szwTemp[512];
      char szaTemp[1024];
      swprintf (szwTemp, L"Generating %s...", pszFile);
      WideCharToMultiByte (CP_ACP, 0, szwTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
      Progress.Start (hWnd, szaTemp, TRUE);
      psz = MegaFileGenerate (pszFile, pszFileCRK, pProj, &Progress);
   }
   if (psz)
      EscMessageBox (hWnd, L"Generate Circumreality file", psz, NULL, MB_OK);

   fRetWhenDone = psz ? FALSE : TRUE;
   goto cleanup;

cleanup:
   // if there's a temporary library then delete
   if (szwTempLib[0])
      DeleteFileW (szwTempLib);

   // swap back the project
   if (lPCMIFLLibSwap.Num()) {
      pProj->LibrarySwap (&lPCMIFLLibSwap);

      // delete
      DWORD i;
      PCMIFLLib *ppl = (PCMIFLLib*)lPCMIFLLibSwap.Get(0);
      for (i = 0; i < lPCMIFLLibSwap.Num(); i++)
         delete ppl[i];
   }
   return fRetWhenDone;
}

/*************************************************************************************
ProjectNameToCircumreality - Converts a project name toa Circumreality file.

inputs
   PWSTR       pszProject - Project name
   BOOL        fCircumreality - If TRUE append Circumreality, else CRK.
   PWSTR       pszCircumreality - Filled with Circumreality name (extension .CRF)
returns
   none
*/
void ProjectNameToCircumreality (PWSTR pszProject, BOOL fCircumreality, PWSTR pszCircumreality)
{
   wcscpy (pszCircumreality, pszProject);

   // find the last period...
   PWSTR pszCur;
   for (pszCur = wcschr(pszCircumreality, L'.'); pszCur; pszCur = wcschr(pszCur+1, L'.'))
      pszCircumreality = pszCur+1;

   wcscpy (pszCircumreality, fCircumreality ? L"crf" : L"crk");
}

// BUGBUG - If megafile generate comes across a file that can't be found in
// the directory.. such as "c:\miketts\xyz.vce", then it should ask the user
// for a new directory to look in (or new file), and remember that

// BUGBUG - When save wave files in megafile remove everything except wave,
// text, lip sync. Get rid of energy and SR features

// BUGBUG - when going through all the sub-objects in the 3d library, is
// it better not to always x-fer over... perhaps only if date/size is different
// than in existing .crf file. ALso, can't get too agressive becuase
// the object might point to another file, and need to verify that one too

// BUGBUG - see if can make scanning faster by not copying individual models,
// and maybe caching list of files need to check for dependencies.

// BUGBUG - If c:\payaso.jpg doesn't exist as a file, even though it should
// then error out