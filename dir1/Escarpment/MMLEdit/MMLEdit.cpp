/*************************************************************************
MMLEdit.cpp - Application that lets you edit MML files.

begun 4/21/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdio.h>
#include <escarpment.h>
#include "resource.h"

/* globals */
HINSTANCE      ghInstance;
char           gszAppPath[256], gszAppDir[256];
PSTR           gpszCmdLine = NULL;
DWORD          gdwErrorNum;
CMem           gMemErrorString;
CMem           gMemErrorSurround;
DWORD          gdwErrorSurroundChar = (DWORD)-1;
WCHAR          gszEdit[] = L"mml";     // name of the edit control
WCHAR          gszFile[256] = L"";     // file currently working on
BOOL           gfFileUnicode = FALSE;  // so when save, will save as unicode if was unicode
char           gszRegKey[] = "Software\\mXac\\Escarpment\\MMLEdit";

// saved to registry
DWORD          gdwInitFlags = EWS_TITLE | EWS_SIZABLE | EWS_VSCROLL | EWS_FIXEDHEIGHT | EWS_NOSPECIALCLOSE;
CListVariable  glistSubKey;   // list of substitution keys
CListVariable  glistSubString;   // corresponding list of what they're converted to
CListVariable  glistMapJPEG;  // map resources
CListVariable  glistMapBMP;   // map resources
CListVariable  glistMapMML;   // map resources


/***********************************************************************
RegSave - Save everything to the registry
*/
void RegSave (void)
{
   HKEY  hInfo = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegKey, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (!hInfo)
      return;

   // write the init flags
   RegSetValueEx (hInfo, "Flags", 0, REG_DWORD, (BYTE*) &gdwInitFlags, sizeof(gdwInitFlags));

   // write the 5 variable lists
   DWORD dwList;
   for (dwList = 0; dwList < 5; dwList++) {
      PCListVariable plist;
      char  *pszName;
      switch (dwList) {
      case 0:
         plist = &glistSubKey;
         pszName = "SubKey";
         break;
      case 1:
         plist = &glistSubString;
         pszName = "SubString";
         break;
      case 2:
         plist = &glistMapJPEG;
         pszName = "JPEG";
         break;
      case 3:
         plist = &glistMapBMP;
         pszName = "BMP";
         break;
      case 4:
         plist = &glistMapMML;
         pszName = "MML";
         break;
      }

      // write out the num elemenhts
      DWORD dwNum;
      dwNum = plist->Num();
      RegSetValueEx (hInfo, pszName, 0, REG_DWORD, (BYTE*) &dwNum, sizeof(dwNum));

      // write out all the elemens
      DWORD i;
      char  szTemp[64];
      for (i = 0; i < dwNum; i++) {
         sprintf (szTemp, "%s%d", pszName, i);
         RegSetValueEx (hInfo, szTemp, 0, REG_BINARY, (BYTE*) plist->Get(i), (DWORD)plist->Size(i));
      }
   }

   RegCloseKey (hInfo);

}

/***********************************************************************
RegLoad - Load everything to the registry
*/
void RegLoad (void)
{
   HKEY  hInfo = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, gszRegKey, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (!hInfo)
      return;

   // write the init flags
   DWORD dwSize, dwType;
   dwSize = sizeof(gdwInitFlags);
   RegQueryValueEx (hInfo, "Flags", NULL, &dwType, (LPBYTE) &gdwInitFlags, &dwSize);

   // write the 5 variable lists
   DWORD dwList;
   for (dwList = 0; dwList < 5; dwList++) {
      PCListVariable plist;
      char  *pszName;
      switch (dwList) {
      case 0:
         plist = &glistSubKey;
         pszName = "SubKey";
         break;
      case 1:
         plist = &glistSubString;
         pszName = "SubString";
         break;
      case 2:
         plist = &glistMapJPEG;
         pszName = "JPEG";
         break;
      case 3:
         plist = &glistMapBMP;
         pszName = "BMP";
         break;
      case 4:
         plist = &glistMapMML;
         pszName = "MML";
         break;
      }

      // read in the num elemenhts
      DWORD dwNum;
      dwSize = sizeof(dwNum);
      dwNum = 0;
      RegQueryValueEx (hInfo, pszName, NULL, &dwType, (LPBYTE) &dwNum, &dwSize);

      // read out all the elemens
      DWORD i;
      char  szTemp[64];
      for (i = 0; i < dwNum; i++) {
         BYTE  abHuge[10000];
         sprintf (szTemp, "%s%d", pszName, i);

         dwSize = sizeof(abHuge);
         RegQueryValueEx (hInfo, szTemp, NULL, &dwType, (LPBYTE) abHuge, &dwSize);

         // add this
         plist->Add (abHuge, dwSize);

         // if we're loading a resource remap then tell system
         DWORD dw;
         dw = *((DWORD*) abHuge);
         PWSTR psz = (PWSTR) (abHuge + sizeof(DWORD));

         // find the resource number so can tell the system
         switch (dwList) {
         case 2:  // JPEG
            EscRemapJPEG (dw, psz);
            break;
         case 3:  // Bitmap
            EscRemapBMP (dw, psz);
            break;
         case 4:  // MML
            EscRemapMML (dw, psz);
            break;
         }
      }
   }

   RegCloseKey (hInfo);

}

/***********************************************************************
TestPage callback - All it really does is store the error info away
*/
BOOL TestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      // add am accelerator for escape just in case there's no title bar
      // as set by preferences
      ESCACCELERATOR a;
      memset (&a, 0, sizeof(a));
      a.c = VK_ESCAPE;
      a.dwMessage = ESCM_CLOSE;
      pPage->m_listESCACCELERATOR.Add (&a);
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         
         // find match
         DWORD i;
         for (i = 0; i < glistSubKey.Num(); i++) {
            PWSTR psz = (PWSTR) glistSubKey.Get(i);

            if (_wcsicmp(psz, p->pszSubName))
               continue;

            // else found substittuion
            p->pszSubString = (PWSTR) glistSubString.Get(i);
            p->fMustFree = FALSE;
            return TRUE;
         }

         // if got here unknown substitution. put in text
         p->fMustFree = FALSE;
         p->pszSubString = L"SUBSTITUTION";
      }
      return TRUE;

   case ESCM_INTERPRETERROR:
      {
         PESCMINTERPRETERROR p = (PESCMINTERPRETERROR) pParam;

         gdwErrorNum = p->pError->m_dwNum;
         gMemErrorString.Required ((wcslen(p->pError->m_pszDesc)+1)*2);
         wcscpy ((PWSTR)gMemErrorString.p, p->pError->m_pszDesc);

         if (p->pError->m_pszSurround) {
            gMemErrorSurround.Required ((wcslen(p->pError->m_pszSurround)+1)*2);
            wcscpy ((PWSTR) gMemErrorSurround.p, p->pError->m_pszSurround);
            gdwErrorSurroundChar = p->pError->m_dwSurroundChar;
         }
         else {
            gdwErrorSurroundChar = (DWORD)-1;
         }
      }
      return TRUE;
   }
   return FALSE;
}

/***********************************************************************
SetMMLTitle - sets the title to the file currently working on
*/
void SetMMLTitle (PCEscPage pPage)
{
   WCHAR szTemp[256];
   swprintf (szTemp, L"%s - MML Edit", gszFile[0] ? gszFile : L"New Document");
   pPage->m_pWindow->TitleSet (szTemp);
}

/****************************************************************************
SaveFile - Saves the file. If there's no name in gszFile then asks for the name
*/
BOOL SaveFile (PCEscPage pPage, BOOL fSaveAs)
{
   if (fSaveAs || !gszFile[0]) {
      OPENFILENAME   ofn;
      char  szTemp[256];
      WideCharToMultiByte (CP_ACP, 0, gszFile, -1, szTemp, sizeof(szTemp), 0,0);

      memset (&ofn, 0, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
      ofn.hInstance = ghInstance;
      ofn.lpstrFilter =
         "MML (*.mml)\0*.mml\0"
         "Text (*.txt)\0*.txt\0"
         "\0\0";
      ofn.lpstrFile = szTemp;
      ofn.nMaxFile = sizeof(szTemp);
      ofn.lpstrTitle = "Save MML Document";
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
      ofn.lpstrDefExt = "mml";
      // nFileExtension 

      if (!GetSaveFileName(&ofn))
         return FALSE;

      // else have a name
      // convert to unicode
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, gszFile, sizeof(gszFile)/2);
      SetMMLTitle (pPage);
   }

   // save it
   char szFile[256];
   WideCharToMultiByte (CP_ACP, 0, gszFile, -1, szFile, sizeof(szFile), 0,0);

   DWORD dwNeeded = 0;
   PCEscControl   pc;
   pc = pPage->ControlFind (gszEdit);
   pc->AttribGet (L"text", NULL, 0, &dwNeeded);
   if (!dwNeeded) {
      pPage->MBError (L"The text is empty.", L"You don't have any text to save.");
      return TRUE;
   }
   CMem  mem;
   mem.Required (dwNeeded);
   pc->AttribGet (L"text", (PWSTR) mem.p, dwNeeded, &dwNeeded);

   FILE  *f;
   f = fopen(szFile, "wb");
   if (!f) {
      pPage->MBError (
         L"Unable to save the file.",
         L"You may have entered a bad file name or the file may be in use.");
      return FALSE;
   }

   // write ANSI or unicode
   if (gfFileUnicode) {
      WORD  wHeader;
      wHeader = 0xfeff;
      fwrite (&wHeader, sizeof(wHeader), 1, f);
      fwrite (mem.p, wcslen((WCHAR*) mem.p), 2, f);
   }
   else {
      // convert to ANSI
      CMem  ansi;
      DWORD dwLen = (DWORD)wcslen((WCHAR*) mem.p) * 2;
      ansi.Required (dwLen);
      int   iRet;
      iRet = WideCharToMultiByte (CP_ACP, 0, (WCHAR*) mem.p, dwLen/2, (char*)ansi.p, dwLen, 0, 0);

      fwrite (ansi.p, iRet, 1, f);
   }

   fclose (f);

   // set the dirty bit to false
   pc->AttribSet (L"dirty", L"false");

   // done
   return TRUE;
}


/***********************************************************************
AskIfWantToSave - if the dirty flag is set ask the user if they want
to save the file

inputs
   PCEscPage   pPage
returns
   BOOL - TRUE if OK to procede with blanking, FALSE if should cancel
*/
BOOL AskIfWantToSave (PCEscPage pPage)
{
   // find the edit control
   PCEscControl   pc;
   pc = pPage->ControlFind (gszEdit);

   // see if it's dirty
   WCHAR szTemp[16];
   DWORD dwNeeded;
   szTemp[0] = 0;
   pc->AttribGet (L"dirty", szTemp, sizeof(szTemp), &dwNeeded);
   BOOL  fVal;
   if (!AttribToYesNo(szTemp, &fVal))
      fVal = FALSE;

   // if it's dirty then ask user if they want to save
   if (!fVal)
      return TRUE;   // not dirty
   DWORD dwRet;
   dwRet = pPage->MBYesNo (
      L"Do you want to save your changes?",
      L"The current document has been changed. Do you want to save your changes first so you don't lose them?", TRUE);
   if (dwRet != IDYES) {
      // the user doesn't want to save
      return (dwRet == IDNO); // if press cancel will return FALSE;
   }

   // save
   return SaveFile (pPage, FALSE);
}

/********************************************************************
DataToUnicode - Takes a pointer to unknown data, either from a file
or from a resource, and determines if it's a unciode text. If it's
unicode, the data is copied and null-terminated. If it's ANSI it's
converted and NULL terminated. Use the magic unicode tag to determine

inputs
   PVOID       pData - data
   DWORD       dwSize - data size
   BOOL        *pfWasUnicode - if not NULL, this is filled in with TRUE
               if the original data was unciode, FLASE if its ANSI
returns
   PWSTR - Unicode string. Must bee freeed()
*/
PWSTR DataToUnicode (PVOID pData, DWORD dwSize, BOOL *pfWasUnicode)
{
   WCHAR *pc;

   // might be unicode
   if (dwSize >= 2) {
      pc = (WCHAR*) pData;
      if (pc[0] == 0xfeff) {
         // it's unicode
         PWSTR psz;
         psz = (PWSTR) malloc(dwSize);
         if (!psz)
            return NULL;
         memcpy (psz, pc + 1, dwSize - 2);
         psz[dwSize/2 - 1] = 0;   // null terminate

         if (pfWasUnicode)
            *pfWasUnicode = TRUE;
         return psz;
      }
   }

   // else it's ANSI
   int   iLen;
   pc = (PWSTR) malloc (dwSize*2+2);
   if (!pc)
      return FALSE;
   iLen = MultiByteToWideChar (CP_ACP, 0, (char*) pData, dwSize, pc, dwSize+1);
   pc[iLen] = 0;  // NULL terminate

   if (pfWasUnicode)
      *pfWasUnicode = FALSE;
   return pc;
}

/********************************************************************
FileToUnicode - Reads a file and returns a unicode string. The
file is converted from ANSI if necessary.

inputs
   PWSTR       pszFile - file
   BOOL        *pfWasUnicode - if not NULL, this is filled in with TRUE
               if the original data was unciode, FLASE if its ANSI
returns
   PWSTR - Unicode string. Must be freed(). NULL if error
*/
PWSTR FileToUnicode (PWSTR pszFile, BOOL *pfWasUnicode)
{
   // try opening the file
   FILE  *f = NULL;
   PBYTE pMem = NULL;
   PWSTR pszUnicode = NULL;

   // convert to ANSI name
   char  szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
   f = fopen (szTemp, "rb");
   if (!f)
      return NULL;

   // how big is it?
   fseek (f, 0, SEEK_END);
   DWORD dwSize;
   dwSize = ftell (f);
   fseek (f, 0, 0);

   // read it in
   pMem = (PBYTE) malloc (dwSize);
   if (!pMem) {
      fclose (f);
      return NULL;
   }

   fread (pMem, 1, dwSize, f);
   fclose (f);


   // convert
   pszUnicode = DataToUnicode (pMem, dwSize, pfWasUnicode);

   if (pMem)
      free (pMem);

   return pszUnicode;
}


/****************************************************************************
OpenFile - Opens a file, destroying current contents.

inputs
   char     *psz - file to load. if NULL then ask
   PCEscPage   pPage - to use for the window & stuff
returns
   BOOL - TRUE if ok
*/
BOOL OpenFile (char *psz, PCEscPage pPage)
{
   OPENFILENAME   ofn;
   HWND  hWnd = pPage->m_pWindow->m_hWnd;
   char  szTemp[256];
   szTemp[0] = 0;

   if (psz) {
      strcpy (szTemp, psz);
   }
   else {
      memset (&ofn, 0, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = hWnd;
      ofn.hInstance = ghInstance;
      ofn.lpstrFilter =
         "MML (*.mml)\0*.mml\0"
         "Text (*.txt)\0*.txt\0"
         "\0\0";
      ofn.lpstrFile = szTemp;
      ofn.nMaxFile = sizeof(szTemp);
      ofn.lpstrTitle = "Open MML File";
      ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
      ofn.lpstrDefExt = "mml";
      // nFileExtension 

      if (!GetOpenFileName(&ofn))
         return FALSE;
   }

   // convert to unicode
   WCHAR szwTemp[256];
   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szwTemp, sizeof(szwTemp)/2);

   PWSTR pszData;
   pszData = FileToUnicode (szwTemp, &gfFileUnicode);

   if (!pszData) {
      pPage->MBError (
         L"The file didn't load.",
         L"You might have specified an incorrect file or it might already be used by another application.");
      return FALSE;
   }
   wcscpy (gszFile, szwTemp);
   SetMMLTitle (pPage);

   // load it in
   PCEscControl   pc;
   pc = pPage->ControlFind (gszEdit);
   pc->AttribSet (L"selstart", L"0");
   pc->AttribSet (L"selend", L"0");
   pc->AttribSet (L"text", pszData);
   pc->AttribSet (L"dirty", L"false");
   pc->AttribSet (L"topline", L"0");   // scroll to top
   pc->AttribSet (L"scrollx", L"0");   // all the way to the left

   free (pszData);

   return TRUE;
}


/***********************************************************************
AddResToList - adds an element to the list so it looks pretty.

inputs
   PCEscPage      pPage - page
   DWORD          dwID - resource iD
   PWSTR          pszFile - file name
returns
   none
*/
void AddResToList (PCEscPage pPage, DWORD dwID, PWSTR pszFile)
{
   PCEscControl pc = pPage->ControlFind (L"list");
   if (!pc)
      return;

   // really huge temporary buffer & fill in
   WCHAR szTemp[1024];
   WCHAR szMMLFile[1024];
   
   // convert strings to MML
   size_t dwNeeded;
   StringToMMLString (pszFile, szMMLFile, sizeof(szMMLFile), &dwNeeded);

   // write out
   swprintf (szTemp,
      L"<elem>Simulate resource <font color=#800000><bold>%d</bold></font> from \"<font color=#800000><bold>%s</bold></font>\"</elem>",
      dwID, szMMLFile);

   // add it
   ESCMLISTBOXADD a;
   memset (&a, 0, sizeof(a));
   a.dwInsertBefore = -1;
   a.pszMML = szTemp;

   pc->Message (ESCM_LISTBOXADD, &a);
}


/***********************************************************************
AddSubToList - adds an element to the list so it looks pretty.

inputs
   PCEscPage      pPage - page
   PWSTR          pszKey - key string
   PWSTR          pszString - string to replace
returns
   none
*/
void AddSubToList (PCEscPage pPage, PWSTR pszKey, PWSTR pszString)
{
   PCEscControl pc = pPage->ControlFind (L"list");
   if (!pc)
      return;

   // really huge temporary buffer & fill in
   WCHAR szTemp[2045];
   WCHAR szMMLKey[1024], szMMLString[1024];
   
   // convert strings to MML
   size_t dwNeeded;
   StringToMMLString (pszKey, szMMLKey, sizeof(szMMLKey), &dwNeeded);
   StringToMMLString (pszString, szMMLString, sizeof(szMMLString), &dwNeeded);

   // write out
   swprintf (szTemp,
      L"<elem>Convert \"<font color=#800000><bold>%s</bold></font>\" to \"<font color=#800000><bold>%s</bold></font>\"</elem>",
      szMMLKey, szMMLString);

   // add it
   ESCMLISTBOXADD a;
   memset (&a, 0, sizeof(a));
   a.dwInsertBefore = -1;
   a.pszMML = szTemp;

   pc->Message (ESCM_LISTBOXADD, &a);
}

/***********************************************************************
PageRemap - Page callback for resource mapping
*/
BOOL PageRemap (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // figure out which data to use
         PCListVariable plist;
         switch ((DWORD) (size_t) pPage->m_pUserData) {
         case 0:  // JPEG
            plist = &glistMapJPEG;
            break;
         case 1:  // Bitmap
            plist = &glistMapBMP;
            break;
         case 2:  // MML
            plist = &glistMapMML;
            break;
         }

         // fill in the list
         DWORD i;
         for (i = 0; i < plist->Num(); i++) {
            DWORD *pdw = (DWORD*) plist->Get(i);
            if (!pdw) continue;

            PWSTR psz;
            psz = (PWSTR) (pdw+1);

            AddResToList (pPage, *pdw, psz);
         }
      }
      return TRUE;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;
         
         if (!_wcsicmp(p->pszSubName, L"DATATYPE")) {
            p->fMustFree = FALSE;

            switch ((DWORD) (size_t) pPage->m_pUserData) {
            case 0:  // JPEG
               p->pszSubString = L"JPEG";
               break;
            case 1:  // Bitmap
               p->pszSubString = L"Bitmap";
               break;
            case 2:  // MML
               p->pszSubString = L"MML";
               break;
            }
         }
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // figure out which data to use
         PCListVariable plist;
         switch ((DWORD) (size_t)pPage->m_pUserData) {
         case 0:  // JPEG
            plist = &glistMapJPEG;
            break;
         case 1:  // Bitmap
            plist = &glistMapBMP;
            break;
         case 2:  // MML
            plist = &glistMapMML;
            break;
         }

         if (!_wcsicmp(p->psz, L"add")) {
            // add a new entry

            // just use really huge buffers on stack since lazy
            WCHAR szFile[256];
            DWORD dwNeeded;
            DWORD dwRes;
            szFile[0] = 0;
            pPage->ControlFind (L"file")->AttribGet (L"text", szFile, sizeof(szFile), &dwNeeded);
            dwRes = (DWORD) pPage->ControlFind (L"resnum")->AttribGetInt (L"text");


            // must have a file
            if (!szFile[0]) {
               pPage->MBError (L"You must type something into the file name.");
               return TRUE;
            }

            // is the res # a duplicate
            DWORD i;
            for (i = 0; i < plist->Num(); i++) {
               DWORD *pdw = (DWORD*) plist->Get(i);
               if (*pdw == dwRes) {
                  pPage->MBSpeakWarning (L"The resource ID is already used. Please delete it first.");
                  return TRUE;
               }
            }

            // add it
            BYTE     abTemp[512];
            memcpy (abTemp, &dwRes, sizeof(dwRes));
            wcscpy ((WCHAR*) (abTemp + sizeof(DWORD)), szFile);
            plist->Add (abTemp, sizeof(DWORD) + (wcslen(szFile)+1)*2);

            // add it to the list
            AddResToList (pPage, dwRes, szFile);

            // make sure the remapping occurs in the escarpment dll
            switch ((DWORD) (size_t) pPage->m_pUserData) {
            case 0:  // JPEG
               EscRemapJPEG (dwRes, szFile);
               break;
            case 1:  // Bitmap
               EscRemapBMP (dwRes, szFile);
               break;
            case 2:  // MML
               EscRemapMML (dwRes, szFile);
               break;
            }

            return TRUE;
         }
         else if (!_wcsicmp (p->psz, L"delete")) {
            // delete

            // find out what selection is current
            PCEscControl pc = pPage->ControlFind (L"list");
            int   iCur = pc->AttribGetInt (L"cursel");

            // make sure it's valid
            DWORD *pdw;
            pdw = (DWORD*) plist->Get((DWORD) iCur);
            if (!pdw) {
               pPage->MBSpeakWarning (L"You must select an item to delete.");
               return TRUE;
            }

            // find the resource number so can tell the system
            switch ((DWORD) (size_t) pPage->m_pUserData) {
            case 0:  // JPEG
               EscRemapJPEG (*pdw, NULL);
               break;
            case 1:  // Bitmap
               EscRemapBMP (*pdw, NULL);
               break;
            case 2:  // MML
               EscRemapMML (*pdw, NULL);
               break;
            }
            

            // delete it
            plist->Remove((DWORD) iCur);

            ESCMLISTBOXDELETE d;
            d.dwIndex = (DWORD) iCur;
            pc->Message (ESCM_LISTBOXDELETE, &d);
            return TRUE;
         }
      }
      return FALSE;  // default
   }
   return FALSE;
}


/***********************************************************************
PageSubstitution - Page callback
*/
BOOL PageSubstitution (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list
         DWORD i;
         for (i = 0; i < glistSubKey.Num(); i++) {
            PWSTR pszKey = (PWSTR) glistSubKey.Get(i);
            PWSTR pszString = (PWSTR) glistSubString.Get(i);
            if (!pszKey || !pszString)
               continue;

            AddSubToList (pPage, pszKey, pszString);
         }
      }
      return TRUE;


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!_wcsicmp(p->psz, L"add")) {
            // add a new entry

            // just use really huge buffers on stack since lazy
            WCHAR szKey[1024], szString[10000];
            DWORD dwNeeded;
            szKey[0] = szString[0] = 0;
            pPage->ControlFind (L"subkey")->AttribGet (L"text", szKey, sizeof(szKey), &dwNeeded);
            pPage->ControlFind (L"substring")->AttribGet (L"text", szString, sizeof(szString), &dwNeeded);

            // must have a key
            if (!szKey[0]) {
               pPage->MBError (L"You must type something into the key.");
               return TRUE;
            }

            // is the key a duplicate
            DWORD i;
            for (i = 0; i < glistSubKey.Num(); i++) {
               PWSTR psz = (PWSTR) glistSubKey.Get(i);

               if (!_wcsicmp(psz, szKey)) {
               pPage->MBSpeakWarning (L"A substitution using that key already exists. Please delete it first.");
               return TRUE;
               }
            }

            // add it
            glistSubKey.Add (szKey, (wcslen(szKey)+1)*2);
            glistSubString.Add (szString, (wcslen(szString)+1)*2);

            // add it to the list
            AddSubToList (pPage, szKey, szString);
            return TRUE;
         }
         else if (!_wcsicmp (p->psz, L"delete")) {
            // delete

            // find out what selection is current
            PCEscControl pc = pPage->ControlFind (L"list");
            int   iCur = pc->AttribGetInt (L"cursel");

            // make sure it's valid
            if (!glistSubKey.Get((DWORD) iCur)) {
               pPage->MBSpeakWarning (L"You must select an item to delete.");
               return TRUE;
            }

            // delete it
            glistSubKey.Remove((DWORD) iCur);
            glistSubString.Remove ((DWORD)iCur);

            ESCMLISTBOXDELETE d;
            d.dwIndex = (DWORD) iCur;
            pc->Message (ESCM_LISTBOXDELETE, &d);
            return TRUE;
         }
      }
      return FALSE;  // default
   }
   return FALSE;
}

/***********************************************************************
WindowAppearance - Page callback
*/
BOOL WindowAppearance (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // based on the gdwInitFlags check the buttons
         pPage->ControlFind(L"title")->AttribSetBOOL (L"checked",
            (gdwInitFlags & EWS_NOTITLE) ? FALSE : TRUE);
         pPage->ControlFind(L"sizable")->AttribSetBOOL (L"checked",
            (gdwInitFlags & EWS_FIXEDSIZE) ? FALSE : TRUE);
         pPage->ControlFind(L"vscroll")->AttribSetBOOL (L"checked",
            (gdwInitFlags & EWS_NOVSCROLL) ? FALSE : TRUE);
         pPage->ControlFind(L"autoheight")->AttribSetBOOL (L"checked",
            (gdwInitFlags & EWS_AUTOHEIGHT) ? TRUE : FALSE);
         pPage->ControlFind(L"autowidth")->AttribSetBOOL (L"checked",
            (gdwInitFlags & EWS_AUTOWIDTH) ? TRUE : FALSE);

         if (gdwInitFlags & EWS_CLOSEMOUSEMOVE)
            pPage->ControlFind(L"closemousemove")->AttribSetBOOL (L"checked", TRUE);
         else if (gdwInitFlags & EWS_CLOSENOMOUSE)
            pPage->ControlFind(L"closenomouse")->AttribSetBOOL (L"checked", TRUE);
         else
            pPage->ControlFind(L"normalclose")->AttribSetBOOL (L"checked", TRUE);

      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         // if user pressed OK then remember settings
         if (!_wcsicmp(p->psz, L"ok")) {
            DWORD dw = 0;
            if (!pPage->ControlFind(L"title")->AttribGetBOOL(L"checked"))
               dw |= EWS_NOTITLE;
            if (!pPage->ControlFind(L"sizable")->AttribGetBOOL(L"checked"))
               dw |= EWS_FIXEDSIZE;
            if (!pPage->ControlFind(L"vscroll")->AttribGetBOOL(L"checked"))
               dw |= EWS_NOVSCROLL;
            if (pPage->ControlFind(L"autoheight")->AttribGetBOOL(L"checked"))
               dw |= EWS_AUTOHEIGHT;
            if (pPage->ControlFind(L"autowidth")->AttribGetBOOL(L"checked"))
               dw |= EWS_AUTOWIDTH;

            if (pPage->ControlFind(L"closemousemove")->AttribGetBOOL(L"checked"))
               dw |= EWS_CLOSEMOUSEMOVE;
            else if (pPage->ControlFind(L"closenomouse")->AttribGetBOOL(L"checked"))
               dw |= EWS_CLOSENOMOUSE;

            // store away
            gdwInitFlags = dw;
         }
      }
      return FALSE;  // pass onto default
   }

   return FALSE;
}


/***********************************************************************
Page callback
*/
BOOL MyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // when init set focus to next control
         PCEscControl   pc;
         pc = pPage->ControlFind (gszEdit);
         pPage->FocusSet (pc);

         // if command line then open the file
         if (gpszCmdLine && gpszCmdLine[0]) {
            OpenFile (gpszCmdLine, pPage);
         }
      }
      return TRUE;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!_wcsicmp(p->psz, L"test")) {
            // find the edit control
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);

            DWORD dwNeeded = 0;
            pc->AttribGet (L"text", NULL, 0, &dwNeeded);
            if (!dwNeeded) {
               pPage->MBError (L"The compile failed.", L"You don't have any text.");
               return TRUE;
            }
            CMem  mem;
            mem.Required (dwNeeded);
            pc->AttribGet (L"text", (PWSTR) mem.p, dwNeeded, &dwNeeded);

            // try to compile this
            CEscWindow  cWindow;
   
            WCHAR *psz;
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd, gdwInitFlags);
            psz = cWindow.PageDialog (ghInstance, (PWSTR) mem.p, TestPage);
            
            // hide the window so can show message
            cWindow.ShowWindow (SW_HIDE);

            // if return string then report that
            PCEscControl   pc2;
            if (psz) {
               WCHAR szTemp[512];
               swprintf (szTemp, L"The page returned, %s", psz);
               pPage->MBSpeakInformation (szTemp);

               pc2 = pPage->ControlFind (L"status");
               ESCMSTATUSTEXT st;
               memset (&st, 0, sizeof(st));
               st.pszText = psz;
               pc2->Message (ESCM_STATUSTEXT, &st);
            }
            else {
               // error
               pPage->MBError (L"The compile failed.",
                  gMemErrorString.p ? (PWSTR) gMemErrorString.p : L"No reason given.");

               pc2 = pPage->ControlFind (L"status");
               ESCMSTATUSTEXT st;
               memset (&st, 0, sizeof(st));
               st.pszText = gMemErrorString.p ? (PWSTR) gMemErrorString.p : L"No reason given.";
               pc2->Message (ESCM_STATUSTEXT, &st);

               // see if can find the location of the error and set the caret there
               PWSTR pszErr;
               pszErr = (PWSTR) gMemErrorSurround.p;
               if ((gdwErrorSurroundChar != (DWORD)-1) && pszErr) {
                  PWSTR pszFind, pszFind2;
                  PWSTR pszSrc = (WCHAR*) mem.p;
                  pszFind = wcsstr (pszSrc, pszErr);

                  // keep on looking for the last occurance
                  pszFind2 = pszFind;
                  while (pszFind2) {
                     pszFind2 = wcsstr (pszFind+1, pszErr);
                     if (pszFind2)
                        pszFind = pszFind2;
                  }

                  if (pszFind) {
                     // found it, set the attribute
                     WCHAR szTemp[16];
                     swprintf (szTemp, L"%d", ((PBYTE) pszFind - (PBYTE) pszSrc)/2 + gdwErrorSurroundChar);
                     pc->AttribSet (L"selstart", szTemp);
                     pc->AttribSet (L"selend", szTemp);
                     pc->Message (ESCM_EDITSCROLLCARET);
                  }
               }
            }

            // make sure has focus
            pPage->FocusSet (pc);

            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"search")) {
            // get the search string
            WCHAR szSearch[1024];
            PCEscControl   pc, pc2;
            pc = pPage->ControlFind (L"searchstring");
            pc2 = pPage->ControlFind (gszEdit);
            DWORD dwNeeded = 0;
            if (!pc->AttribGet (L"text", szSearch, sizeof(szSearch), &dwNeeded))
               return TRUE;

            // find it
            ESCMEDITFINDTEXT p;
            memset (&p, 0, sizeof(p));
            p.pszFind = szSearch;
            p.dwStart = pc2->AttribGetInt(L"selend");
            p.dwEnd = (DWORD)-1; // to full end
            pc2->Message (ESCM_EDITFINDTEXT, &p);

            if (p.dwFoundStart == (DWORD)-1) {
               // try fromt he beginning
               p.dwEnd = p.dwStart;
               p.dwStart = 0;
               pc2->Message (ESCM_EDITFINDTEXT, &p);
            }
            if (p.dwFoundStart == (DWORD)-1) {
               pPage->MBSpeakWarning (L"Can't find the search string.", szSearch);
               return TRUE;
            }

            // else found it
            pc2->AttribSetInt (L"selstart", p.dwFoundStart);
            pc2->AttribSetInt (L"selend", p.dwFoundEnd);
            pc2->Message (ESCM_EDITSCROLLCARET);
            pPage->FocusSet (pc2);
         }
         else if (!_wcsicmp(p->psz, L"new")) {
            if (!AskIfWantToSave(pPage))
               return TRUE;

            // wipe out
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pc->AttribSet (L"selstart", L"0");
            pc->AttribSet (L"selend", L"0");
            pc->AttribSet (L"text", L"");
            pc->AttribSet (L"dirty", L"false");
            pc->AttribSet (L"topline", L"0");   // scroll to top
            pc->AttribSet (L"scrollx", L"0");   // all the way to the left

            // set the filename to null
            gszFile[0] = 0;
            gfFileUnicode = FALSE;
            SetMMLTitle (pPage);

            // make sure edit has focus
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"open")) {
            if (!AskIfWantToSave(pPage))
               return TRUE;

            OpenFile (NULL, pPage);


            // make sure edit has focus
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"save")) {
            SaveFile (pPage, FALSE);

            // make sure edit has focus
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"saveas")) {
            SaveFile (pPage, TRUE);

            // make sure edit has focus
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"undo")) {
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pc->Message (ESCM_EDITUNDO);

            // make sure edit has focus
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"redo")) {
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pc->Message (ESCM_EDITREDO);

            // make sure edit has focus
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"cut")) {
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pc->Message (ESCM_EDITCUT);

            // make sure edit has focus
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"copy")) {
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pc->Message (ESCM_EDITCOPY);

            // make sure edit has focus
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"paste")) {
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pc->Message (ESCM_EDITPASTE);

            // make sure edit has focus
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"selectall")) {
            PCEscControl   pc;
            pc = pPage->ControlFind (gszEdit);
            pc->AttribSet (L"selstart", L"0");
            pc->AttribSet (L"selend", L"1000000");
            pc->Message (ESCM_EDITSCROLLCARET);

            // make sure edit has focus
            pPage->FocusSet (pc);
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"about")) {
            pPage->MBInformation (
               L"MML Edit",
               L"Copyright (c) 2000 mXac. All rights reserved. "
               L"JPEG decompression from the Independent JPEG Group."
               );
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"appearance")) {
            CEscWindow  cWindow;
            GetWindowRect (pPage->m_pWindow->m_hWnd, &cWindow.m_rMouseMove);
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, IDR_MMLAPPEARANCE, WindowAppearance);

            RegSave (); // just in case crash later on
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"sub")) {
            CEscWindow  cWindow;
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, IDR_MMLSUBSTITUTE, PageSubstitution);

            RegSave (); // just in case crash later on
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"jpegres")) {
            CEscWindow  cWindow;
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, IDR_MMLREMAP, PageRemap, (PVOID) 0);

            RegSave (); // just in case crash later on
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"bitmapres")) {
            CEscWindow  cWindow;
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, IDR_MMLREMAP, PageRemap, (PVOID) 1);

            RegSave (); // just in case crash later on
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"mmlres")) {
            CEscWindow  cWindow;
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, IDR_MMLREMAP, PageRemap, (PVOID) 2);

            RegSave (); // just in case crash later on
            return TRUE;
         }
         else if (!_wcsicmp(p->psz, L"help")) {
            CEscWindow  cWindow;
            cWindow.Init (ghInstance, pPage->m_pWindow->m_hWnd);
            cWindow.PageDialog (ghInstance, IDR_MMLHELP, NULL);
            return TRUE;
         }
         else {
            // user has closed. veify dont want to save
            if (!AskIfWantToSave(pPage))
               return TRUE;
            return FALSE;  // default close behavior
         }

      }
      return TRUE;  // default behavior

   case ESCM_CLOSE:
      // user has closed. veify dont want to save
      if (!AskIfWantToSave(pPage))
         return TRUE;
      return FALSE;  // default behaviour
   }

   return FALSE;
}



/*************************************************************************
AllowToDoubleClick - Writes the registry entries so a user can double click
*/
void AllowToDoubleClick (void)
{
   char  szFlyFox[] = "MMLDocoument.1";
   char  szFileName[] = "MML Document";
   HKEY  hInfo = NULL;
   DWORD dwDisp;
   char  szTemp[256];

   // verify that it's not already written
   sprintf (szTemp, "\"%s\" %%1", gszAppPath);
   RegOpenKeyEx (HKEY_CLASSES_ROOT, "MMLDocoument.1\\shell\\open\\command", 0,
      KEY_READ, &hInfo);
   if (hInfo) {
      char  szTemp2[256];
      szTemp2[0] = 0;
      DWORD dwType;
      dwDisp = sizeof(szTemp2);
      RegQueryValueEx (hInfo, NULL, 0, &dwType, (BYTE*) szTemp2, &dwDisp);
      RegCloseKey (hInfo);

      if (!_stricmp(szTemp, szTemp2))
         return;
   }
   // else, not there, so write

   // so can double click .mml
   RegCreateKeyEx (HKEY_CLASSES_ROOT, ".mml", 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szFlyFox, sizeof(szFlyFox));
      RegCloseKey (hInfo);
   }

   RegCreateKeyEx (HKEY_CLASSES_ROOT, szFlyFox, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szFileName, sizeof(szFileName));
      RegCloseKey (hInfo);
   }

   sprintf (szTemp, "%s,0", gszAppPath);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, "MMLDocoument.1\\DefaultIcon", 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szTemp, (DWORD)strlen(szTemp)+1);
      RegCloseKey (hInfo);
   }

   RegCreateKeyEx (HKEY_CLASSES_ROOT, "MMLDocoument.1\\protocol\\StdFileEditing\\Server", 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) gszAppPath, (DWORD)strlen(gszAppPath)+1);
      RegCloseKey (hInfo);
   }

   sprintf (szTemp, "\"%s\" %%1", gszAppPath);
   RegCreateKeyEx (HKEY_CLASSES_ROOT, "MMLDocoument.1\\shell\\open\\command", 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hInfo, &dwDisp);
   if (hInfo) {
      RegSetValueEx (hInfo, NULL, 0, REG_SZ, (BYTE*) szTemp, (DWORD)strlen(szTemp)+1);
      RegCloseKey (hInfo);
   }
}



int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;

   // Note: you would replace "sample" and "42" with your E-mail and the
   // registration key sent to you
   EscInitialize(L"sample", 42, 0);

   gpszCmdLine = lpCmdLine;

   // store the directory away
   GetModuleFileName (hInstance, gszAppPath, sizeof(gszAppPath));
   strcpy (gszAppDir, gszAppPath);
   char  *pCur;
   for (pCur = gszAppDir + strlen(gszAppDir); pCur >= gszAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }
   // so can double click mml
   AllowToDoubleClick();

   // load in options
   RegLoad ();

   CEscWindow  cWindow;
   
#ifdef _DEBUG
   // to test bitmaps
   HBITMAP hBmp = JPegOrBitmapLoad ("c:\\test.jpg", TRUE);
   cWindow.m_hbmpBackground = hBmp;
   cWindow.m_rBackgroundFrom.left = cWindow.m_rBackgroundFrom.top = 0;
   cWindow.m_rBackgroundFrom.right = 1024;
   cWindow.m_rBackgroundFrom.bottom = 768;
   cWindow.m_rBackgroundTo.left = cWindow.m_rBackgroundTo.right = 0;
   cWindow.m_rBackgroundTo.right = 1024*2;
   cWindow.m_rBackgroundTo.bottom = 768*2;
#endif

   WCHAR *psz;
   cWindow.Init (hInstance, NULL,
      EWS_TITLE | EWS_SIZABLE | EWS_NOVSCROLL | EWS_FIXEDHEIGHT | EWS_FIXEDWIDTH);
   cWindow.IconSet (LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_APPICON)));
   psz = cWindow.PageDialog (ghInstance, IDR_MMLMAIN, MyPage);

#ifdef _DEBUG
   DeleteObject (hBmp);
#endif

   // write out preferences to registry
   RegSave ();

   EscUninitialize();

   // done
   return 0;
}


