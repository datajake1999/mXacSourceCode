/*************************************************************************************
MegaFile.cpp - Code useful for megafile stuff.

begun 10/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\buildnum.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "resource.h"


/*************************************************************************************
IsValidFile - Given a string, returns TRUE if the string is for a valid file.

inputs
   PWSTR       psz - String
   PCBTree    pTree - Tree that want to check against... if it's already on the
               tree then return FALSE.
   BOOL        fTryToOpen - If TRUE, will also try to open file before verifying
               that it's true.
   DWORD             dwFlags - Series of flags from EFFN_XXX
returns
   BOOL - TRUE if for file
*/
static PWSTR gapszOKSuff3[] = {
   L"m3d", // 3dob file
   L"me3", // 3dob library
   L"mfp", // MIFL project
   L"mfl", // MIFL library
   L"jpg", // jpeg image
   L"bmp", // bitmap image
   L"wav", // wave file
   L"mid", // MIDI file
   L"mlx", // lexion
   L"tts"  // tts file
   };
static PWSTR gapszOKSuff4[] = {
   L"jpeg"  // JPEG image
   };

static BOOL IsValidFile (PWSTR psz, PCBTree pTree, BOOL fTryToOpen, BOOL dwFlags)
{
   BOOL fOK;
   if ( (((psz[0] >= L'a') && (psz[0] <= L'z')) || ((psz[0] >= L'A') || (psz[0] <= L'Z'))) && (psz[1] == L':'))
      fOK = TRUE;
   else if ((psz[0] == L'\\') && (psz[1] == L'\\'))
      fOK = TRUE;
   else
      return FALSE;  // cant be

   DWORD dwLen = (DWORD)wcslen(psz);
   if ((dwLen > 256) || (dwLen < 4))
      return FALSE;  // either too long or too short

   // check suffix
   DWORD dwSuff;
   for (dwSuff = dwLen - 1; dwSuff < dwLen; dwSuff--) {
      if ((dwLen - dwSuff) >= 5)
         return FALSE;  // too long

      if (psz[dwSuff] == L'.') {
         dwSuff++;
         break;
      }
   }
   if (dwSuff >= dwLen)
      return FALSE;  // not found

   // else, compare suffix
   DWORD i;
   if (dwLen - dwSuff == 3) {
      for (i = 0; i < sizeof(gapszOKSuff3)/sizeof(PWSTR); i++)
         if (!_wcsicmp(psz+dwSuff, gapszOKSuff3[i]))
            break;
      if (i >= sizeof(gapszOKSuff3)/sizeof(PWSTR))
         return FALSE;

      // BUGFIX - If a lexicon refers to a tts engine then don't load it
      if ((dwFlags & EFFN_NOTTS) && !_wcsicmp(psz+dwSuff, L"tts"))
         return FALSE;

      // BUGFIX - If refers to resource wave file, dont load it
      if (IsWAVResource(psz))
         return FALSE;
   }
   else if (dwLen - dwSuff == 4) {
      for (i = 0; i < sizeof(gapszOKSuff4)/sizeof(PWSTR); i++)
         if (!_wcsicmp(psz+dwSuff, gapszOKSuff4[i]))
            break;
      if (i >= sizeof(gapszOKSuff4)/sizeof(PWSTR))
         return FALSE;
   }

   // if it's on the tree, then return FALSE... since no point trying
   // to reopen if already know it's a file
   if (pTree->Find (psz))
      return FALSE;

   if (fTryToOpen) { // BUGFIX - Was !fTryToOpen
      // try to actually open
      char szTemp[512];
      WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
      HANDLE hf = CreateFile(szTemp, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hf == INVALID_HANDLE_VALUE)
         return FALSE;  // doesnt exist
      CloseHandle (hf);
   }

   return TRUE;
}


/*************************************************************************************
ExtractFilesFromNode - Given a node, this extracts files from the node. Any new
file references are added to the tree. This recurses to children

inputs
   PCMMLNode2         pNode - Node to extract from
   PCBTree           pTree - If the file doens't already exist in the tree it's added
   BOOL              fTryToOpen - If TRUE, will also try to open file before verifying
                     that it's true.
   DWORD             dwFlags - Series of flags from EFFN_XXX
returns
   none
*/
DLLEXPORT void ExtractFilesFromNode (PCMMLNode2 pNode, PCBTree pTree, BOOL fTryToOpen,
                                     DWORD dwFlags)
{
   // loop through all attributes
   DWORD i;
   PWSTR pszAttrib, psz, psz2;
   DWORD dwType;
   size_t dwSize;
   for (i = 0; i < pNode->AttribNum(); i++) {
      psz = NULL;
      pNode->AttribEnumNoConvert (i, &pszAttrib, (PVOID*) &psz, &dwType, &dwSize);

      if ((dwType == MMNA_STRING) && psz && IsValidFile(psz, pTree, fTryToOpen,dwFlags)) {
         pTree->Add (psz, 0, 0); // found one

#ifdef _DEBUG
         OutputDebugString ("\r\nAdd to megafile: ");
         OutputDebugStringW (psz);
#endif // 0
      }
      // BUGFIX - If this is binary of type MMLCompressed() then need to decompress
      // that and see what's inside
      else if ((dwType == MMNA_BINARY) && (psz2 = pNode->NameGet()) && !_wcsicmp(psz2, MMLCompressed())) {
         PCMMLNode2 pSub = MMLFileOpen ((PBYTE) psz, dwSize, NULL, NULL);
         if (pSub) {
            ExtractFilesFromNode (pSub, pTree, fTryToOpen, dwFlags);
            delete pSub;
         }
      } // if special binary

   } // i

   // loop through all the contents
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      psz = NULL;
      pNode->ContentEnum (i, &psz, &pSub);

      if (pSub) {
         ExtractFilesFromNode (pSub, pTree, fTryToOpen, dwFlags);
      }
      else if (psz)
         if (IsValidFile (psz, pTree, fTryToOpen, dwFlags)) {
            pTree->Add (psz, 0, 0);

#ifdef _DEBUG
         OutputDebugString ("\r\nAdd to megafile: ");
         OutputDebugStringW (psz);
#endif // 0
         }
   } // i
}



/*************************************************************************************
CircumrealityParseMML - This takes a string with MML and parses it. It returns the node.
It assumes format is "<mainnode>stuff</mainnode>"

inputs
   PWSTR       psz - Null-terminated string
returns
   PCMMLNode2 - Node that must be freed
*/
DLLEXPORT PCMMLNode2 CircumrealityParseMML (PWSTR psz)
{
   CEscError err;

   PCMMLNode pRet1;
   PCMMLNode2 pRet;
   pRet1 = ParseMML (psz, ghInstance, NULL, NULL, &err, FALSE);
   if (!pRet1)
      return NULL;
   pRet = pRet1->CloneAsCMMLNode2();
   delete pRet1;
   if (pRet) {
      // parse MML will put a wrapper around this, so take first node
      PCMMLNode2 pSub;
      PWSTR psz;
      pSub = NULL;
      pRet->ContentEnum (0, &psz, &pSub);
      if (pSub)
         pRet->ContentRemove (0, FALSE);
      delete pRet;
      pRet = pSub;
   }

   return pRet;
}

// BUGBUG - when merging .m3d files into large megafile, extract the included
// textures and objects, since should be in the user database anyway
