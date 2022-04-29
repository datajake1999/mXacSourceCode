// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// CircumSAPI.cpp : Implementation of DLL Exports.
//
// Note: Proxy/Stub Information
//      To build a separate proxy/stub DLL, 
//      run nmake -f msttsdrvps.mk in the project directory.

#include "stdafx.h"
#include "resource.h"
#include <initguid.h>
#include <escarpment.h>
#include "..\m3d.h"
#include "CircumSAPI_h.h"
#include "CircumSAPI_i.c"
#include "TtsEngObj.h"


HINSTANCE ghInstance;      // hInstance

static PWSTR gpszSAPIGUID = L"{76D2054C-8841-4fe0-93BA-4EBA0C2C4D00}";
static PWSTR gpszSAPIVoicedRoot = L"SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens";

// {A3BA21EC-15A4-4d42-8939-39E25F647854}
// DEFINE_GUID(TypeLib, 
// 0xa3ba21ec, 0x15a4, 0x4d42, 0x89, 0x39, 0x39, 0xe2, 0x5f, 0x64, 0x78, 0x54);

// {76D2054C-8841-4fe0-93BA-4EBA0C2C4D00}
//DEFINE_GUID(Class, 
//0x76d2054c, 0x8841, 0x4fe0, 0x93, 0xba, 0x4e, 0xba, 0xc, 0x2c, 0x4d, 0x0);

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY( CLSID_CircumSAPI   , CTTSEngObj    )
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

#ifdef _WIN32_WCE
extern "C" BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID)
#else
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
#endif
{
   ghInstance = hInstance;

    if (dwReason == DLL_PROCESS_ATTACH)
    {

         _Module.Init(ObjectMap, (HINSTANCE)hInstance, &LIBID_SAMPLETTSENGLib);
    }
    else if (dwReason == DLL_PROCESS_DETACH) {

         _Module.Term();
    }
    return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
   HRESULT hRes = (_Module.GetLockCount()==0) ? S_OK : S_FALSE;

   return hRes;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}


/*************************************************************************
FindAllSAPIVoices - Find all the registered SAPI voices.

inputs
   PCListVariable       plToken - Filled with the token strings
   PCListVariable       plFile - Filled with the filename
returns
   none
*/
void FindAllSAPIVoiced (PCListVariable plToken, PCListVariable plFile)
{
   HKEY hKey;

   // get the root
   hKey = NULL;
   DWORD dwDisp = 0;
   if (ERROR_SUCCESS != RegCreateKeyExW (HKEY_LOCAL_MACHINE, gpszSAPIVoicedRoot, 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp))
      return;

   DWORD dwIndex;
   WCHAR szVoiceToken[512], szTemp[512];
   DWORD dwSize;
   FILETIME ft;

   HKEY hKeySub;
   CMem memKeySub;
   DWORD dwType;

   for (dwIndex = 0; ; dwIndex++) {
      dwSize = sizeof(szVoiceToken) / sizeof(WCHAR);
      if (ERROR_SUCCESS != RegEnumKeyExW (hKey, dwIndex, szVoiceToken, &dwSize, NULL, NULL, NULL, &ft))
         break;

      // verify that it's for cirucmreality
      hKeySub = NULL;
      MemZero (&memKeySub);
      MemCat (&memKeySub, gpszSAPIVoicedRoot);
      MemCat (&memKeySub, L"\\");
      MemCat (&memKeySub, szVoiceToken);
      if (ERROR_SUCCESS != RegOpenKeyExW (HKEY_LOCAL_MACHINE, (PWSTR)memKeySub.p, 0, KEY_READ, &hKeySub))
         continue;   // error

      // make sure the guid is right
      szTemp[0] = 0;
      dwType = REG_SZ;
      dwSize = sizeof(szTemp);
      if (ERROR_SUCCESS != RegQueryValueExW (hKeySub, L"CLSID", 0, &dwType, (LPBYTE) &szTemp[0], &dwSize)) {
         RegCloseKey (hKeySub);
         continue;
      }
      if ((dwType != REG_SZ) || _wcsicmp(szTemp, gpszSAPIGUID)) {
         RegCloseKey (hKeySub);
         continue;
      }

      // get the filename
      szTemp[0] = 0;
      dwType = REG_SZ;
      dwSize = sizeof(szTemp);
      if (ERROR_SUCCESS != RegQueryValueExW (hKeySub, L"VoiceData", 0, &dwType, (LPBYTE) &szTemp[0], &dwSize)) {
         RegCloseKey (hKeySub);
         continue;
      }
      if (dwType != REG_SZ) {
         RegCloseKey (hKeySub);
         continue;
      }

      // close this key
      RegCloseKey (hKeySub);

      // remember this
      plToken->Add (szVoiceToken, (wcslen(szVoiceToken)+1)*sizeof(WCHAR));
      plFile->Add (szTemp, (wcslen(szTemp)+1)*sizeof(WCHAR));
   }
   RegCloseKey (hKey);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{

   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);

   // get the module filename, remove the filename
   WCHAR szAppDir[256];
   GetModuleFileNameW (ghInstance, szAppDir, sizeof(szAppDir) / sizeof(WCHAR));
   WCHAR *pCur;
   for (pCur = szAppDir + wcslen(szAppDir); pCur >= szAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }

   // get a list of all the TTS files
   CMem memFind, memFound;
   MemZero (&memFind);
   MemCat (&memFind, szAppDir);
   MemCat (&memFind, L"*.tts");
   WIN32_FIND_DATAW  fd;
   HANDLE hFind = FindFirstFileW ((PWSTR)memFind.p, &fd);
   CListVariable lFound;
   if (hFind != INVALID_HANDLE_VALUE) {
      while (TRUE) {
         // analyze this file
         MemZero (&memFound);
         MemCat (&memFound, szAppDir);
         MemCat (&memFound, fd.cFileName);

         lFound.Add (memFound.p, (wcslen((PWSTR)memFound.p)+1) * sizeof(WCHAR));

         if (!FindNextFileW (hFind, &fd))
            break;
      }

      FindClose(hFind);
   }


   // find any TTS voices already registered and remove them from
   // the list, just to make regitration fast
   CListVariable lToken, lFile;
   DWORD i; //, j;
   PWSTR psz;
   FindAllSAPIVoiced (&lToken, &lFile);
   for (i = 0; i < lFound.Num(); i++) {
      psz = (PWSTR)lFound.Get(i);

#if 0 // BUGFIX - Don't bother doing this since will be slow anyway, and shouldnt be any voices,
            // and might cause voices to not be registered properly
      for (j = 0; j < lFile.Num(); j++)
         if (!_wcsicmp((PWSTR)lFile.Get(j), psz)) {
            // already registered, so don't bother
            lFound.Remove (i);
            i--;
            break;
         }
#endif // 0
   } // i


   // loop through any of the remaining TTS voices, load them, and have
   // them register themselves
   for (i = 0; i < lFound.Num(); i++) {
      psz = (PWSTR)lFound.Get(i);
      PCMTTS pTTS = new CMTTS;
      if (!pTTS)
         continue;

      if (!pTTS->Open (psz, TRUE)) {
         delete pTTS;
         continue;   // error
      }

      // register
      pTTS->SAPIRegister ();

      delete pTTS;
   } // i


   // BUGFIX - Move this into register and unregister so setup doenst hang
   MLexiconCacheShutDown (FALSE);

   EscUninitialize ();


    // registers object, typelib and all interfaces in typelib
   HRESULT ret = _Module.RegisterServer(TRUE);


   return ret;

}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{

   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);


   // find any TTS voices already registered and remove them from
   // the list, just to make regitration fast
   CListVariable lToken, lFile;
   DWORD i;
   PWSTR psz;
   FindAllSAPIVoiced (&lToken, &lFile);


   // remove them
   HKEY hKey;

   // NOTE: NOT unregistering for the opposite bit-ness (unregistering a 64-bit cirumsapi.dll
   // doesn't unregister 32-bit voices). Doing this intentionally

   // get the root
   hKey = NULL;
   DWORD dwDisp = 0;
   if (lToken.Num())
      if (ERROR_SUCCESS == RegCreateKeyExW (HKEY_LOCAL_MACHINE, gpszSAPIVoicedRoot, 0, 0, REG_OPTION_NON_VOLATILE,
         KEY_ALL_ACCESS /*KEY_READ | KEY_WRITE*/, NULL, &hKey, &dwDisp)) {

         // delete sub keys
         for (i = 0; i < lToken.Num(); i++) {
            psz = (PWSTR)lToken.Get(i);

            if (ERROR_SUCCESS != SHDeleteKey (hKey, psz)) { // RegDeleteKeyEx (hKey, psz, KEY_ALL_ACCESS, 0))
            }

            // since that doesn't seem to be doing everything
            if (ERROR_SUCCESS != RegDeleteKeyEx (hKey, psz, 
               (sizeof(PVOID)>sizeof(DWORD)) ? KEY_WOW64_64KEY : KEY_WOW64_32KEY, 0)) {
            }

         } // i
         
         RegCloseKey (hKey);
      } // if get root


   // BUGFIX - Move this into register and unregister so setup doenst hang
   MLexiconCacheShutDown (FALSE);

   EscUninitialize ();


   HRESULT ret = _Module.UnregisterServer(TRUE);


   return ret;
}




