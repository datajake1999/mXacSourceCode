/*************************************************************************************
Main.cpp - Entry code for the M3D wave.

begun 24/12/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\buildnum.h"
#include <crtdbg.h>
#include "..\mifl.h"

// CMIFLSocketTest - For the callback
class CMIFLSocketTest : public CMIFLAppSocket, public CEscObject  {
public:
   virtual DWORD LibraryNum (void);
   virtual BOOL LibraryEnum (DWORD dwNum, PMASLIB pLib);
   virtual DWORD ResourceNum (void);
   virtual BOOL ResourceEnum (DWORD dwNum, PMASRES pRes);
   virtual PCMMLNode2 ResourceEdit (HWND hWnd, PWSTR pszType, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
      PCMIFLProj pProj);
   virtual BOOL FileSysSupport (void);
   virtual BOOL FileSysLibOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);
   virtual PCMMLNode2 FileSysOpen (PWSTR pszFile);
   virtual BOOL FileSysSave (PWSTR pszFile, PCMMLNode2 pNode);
   virtual BOOL FuncImportIsValid (PWSTR pszName, DWORD *pdwParams);
   virtual BOOL FuncImport (PWSTR pszName, PCMIFLVM pVM, PCMIFLVMObject pObject,
      PCMIFLVarList plParams, DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVar pRet);
   virtual BOOL TestQuery (void);
   virtual void TestVMNew (PCMIFLVM pVM);
   virtual void TestVMPageEnter (PCMIFLVM pVM);
   virtual void TestVMPageLeave (PCMIFLVM pVM);
   virtual void TestVMDelete (PCMIFLVM pVM);
   virtual void Log (PCMIFLVM pVM, PWSTR psz);
   virtual void VMObjectDelete (PCMIFLVM pVM, GUID *pgID);
   virtual void VMTimerToBeCalled (PCMIFLVM pVM);
   virtual void MenuEnum (PCMIFLProj pProj, PCMIFLLib pLib, PCListVariable plMenu);
   virtual BOOL MenuCall (PCMIFLProj pProj, PCMIFLLib pLib, PCEscWindow pWindow, DWORD dwIndex);
};

HINSTANCE      ghInstance;
char           gszAppDir[256];
char           gszAppPath[256];     // application path
CMem           gMemTemp; // temporary memoty
CMIFLSocketTest gMIFLSocket;  // callbakc
PCMIFLProj     gpMIFLProj = NULL;    // project


/*****************************************************************************
CMIFLSocketTest - This is a test callback
*/
DWORD CMIFLSocketTest::LibraryNum (void)
{
   return 3;   // NOTE - right now hack
}

BOOL CMIFLSocketTest::LibraryEnum (DWORD dwNum, PMASLIB pLib)
{
   if (dwNum >= LibraryNum())
      return FALSE;

   if (dwNum == 0)
      return VMLibraryEnum (pLib);

   memset (pLib, 0, sizeof(*pLib));
   pLib->hResourceInst = ghInstance;

   switch (dwNum) {
   case 1:  // NOTE - test 1
      pLib->dwResource = IDR_MIFLTESTLIB1;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszDescShort = L"Test library 1";
      pLib->pszName = L"Library 1";
      break;

   case 2:  // NOTE - test 2
      pLib->dwResource = IDR_MIFLTESTLIB2;
      pLib->pszDescShort = L"Test library 2";
      pLib->pszName = L"Library 2";
      break;

   default:
      return FALSE;
   }

   return TRUE;
}


DWORD CMIFLSocketTest::ResourceNum (void)
{
   return 2;
}

BOOL CMIFLSocketTest::ResourceEnum (DWORD dwNum, PMASRES pRes)
{
   memset (pRes, 0, sizeof(*pRes));

   switch (dwNum) {
   case 0:
      pRes->pszName = L"Image";
      pRes->pszDescShort = L"Short description of image resource.";
      return TRUE;

   case 1:
      pRes->pszName = L"3D";
      pRes->pszDescShort = L"Short description of 3D resource.";
      return TRUE;

   default:
      return FALSE;
   }

   return FALSE;
}

PCMMLNode2 CMIFLSocketTest::ResourceEdit (HWND hWnd, PWSTR pszType, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                                         PCMIFLProj pProj)
{
   if (_wcsicmp(pszType, L"Image") && _wcsicmp(pszType, L"3D"))
      return NULL;   // error

   // quick message box
   int iRet = MessageBox (hWnd,
      MMLValueGetInt (pIn, L"Test", 0) ?
      "Has a value" : "This is a hacked editing", "Hack edit", MB_YESNO);

   if (iRet == IDYES) {
      PCMMLNode2 pNew = new CMMLNode2;
      pNew->NameSet (L"Test");
      MMLValueSet (pNew, L"Test", 1);
      return pNew;
   }
   return NULL;
}

BOOL CMIFLSocketTest::FileSysSupport (void)
{
   return FALSE;
}

BOOL CMIFLSocketTest::FileSysLibOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   return FALSE;
}

PCMMLNode2 CMIFLSocketTest::FileSysOpen (PWSTR pszFile)
{
   return NULL;
}

BOOL CMIFLSocketTest::FileSysSave (PWSTR pszFile, PCMMLNode2 pNode)
{
   return TRUE;
}

BOOL CMIFLSocketTest::FuncImportIsValid (PWSTR pszName, DWORD *pdwParams)
{
   // call the VM for standard library
   if (VMFuncImportIsValid (pszName, pdwParams))
      return TRUE;

   if (wcscmp(pszName, L"testthis"))
      return FALSE;

   *pdwParams = 3;
   return TRUE;
}

BOOL CMIFLSocketTest::FuncImport (PWSTR pszName, PCMIFLVM pVM, PCMIFLVMObject pObject,
   PCMIFLVarList plParams, DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVar pRet)
{
   // call the VM for standard library
   CMIFLVarLValue varLValue;
   if (pVM->FuncImport (pszName, pObject, plParams, dwCharStart, dwCharEnd, &varLValue)) {
      pRet->Set (&varLValue.m_Var);
      return TRUE;
   }

   if (wcscmp(pszName, L"testthis"))
      return FALSE;

   // do nothing
   pRet->SetDouble (42);
   return TRUE;
}

BOOL CMIFLSocketTest::TestQuery (void)
{
   return TRUE;
}

void CMIFLSocketTest::VMObjectDelete (PCMIFLVM pVM, GUID *pgID)
{
}


void CMIFLSocketTest::VMTimerToBeCalled (PCMIFLVM pVM)
{
}

void CMIFLSocketTest::MenuEnum (PCMIFLProj pProj, PCMIFLLib pLib, PCListVariable plMenu)
{
   PWSTR psz = L"Test menu item 1";
   PWSTR psz2 = L"Test menu item 2";
   plMenu->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   plMenu->Add (psz2, (wcslen(psz2)+1)*sizeof(WCHAR));
}

BOOL CMIFLSocketTest::MenuCall (PCMIFLProj pProj, PCMIFLLib pLib, PCEscWindow pWindow, DWORD dwIndex)
{
   int iVal = MMLValueGetInt (pLib->m_pNodeMisc, L"TestValue", 0);

   pLib->AboutToChange();
   MMLValueSet (pLib->m_pNodeMisc, L"TestValue", 1);
   pLib->Changed();
   PWSTR psz = pWindow->PageDialog (ghInstance, IDR_MMLTESTMENUPAGE, NULL);
   return TRUE;
}

void CMIFLSocketTest::TestVMNew (PCMIFLVM pVM)
{
   OutputDebugString ("TestVMNew\r\n");
}

void CMIFLSocketTest::TestVMPageEnter (PCMIFLVM pVM)
{
   OutputDebugString ("TestVMPageEnter\r\n");
}

void CMIFLSocketTest::TestVMPageLeave (PCMIFLVM pVM)
{
   OutputDebugString ("TestVMPageLeave\r\n");
}

void CMIFLSocketTest::TestVMDelete (PCMIFLVM pVM)
{
   OutputDebugString ("TestVMDelete\r\n");
}

void CMIFLSocketTest::Log (PCMIFLVM pVM, PWSTR psz)
{
   // do nothing
}




/*****************************************************************************
M3DMainLoop - DOes the main display loop for 3DOB

inputs
   LPSTR       lpCmdLine - command line
   int         nShowCmd - Show
*/
int M3DMainLoop (LPSTR lpCmdLine, int nShowCmd)
{
   // get the name
   GetModuleFileName (ghInstance, gszAppPath, sizeof(gszAppPath));
   strcpy (gszAppDir, gszAppPath);
   char  *pCur;
   for (pCur = gszAppDir + strlen(gszAppDir); pCur >= gszAppDir; pCur--)
      if (*pCur == '\\') {
         pCur[1] = 0;
         break;
      }
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
#endif // _DEBUG


   // initialize
   EscInitialize (L"mikerozak@bigpond.com", 2511603559, 0);

   // FUTURERELEASE - ASPHelpInit ();

   // init beep window
   BeepWindowInit ();

   // project
   gpMIFLProj = new CMIFLProj (&gMIFLSocket);
   gpMIFLProj->m_fDirty = TRUE;  // so will save
   wcscpy (gpMIFLProj->m_szFile, L"c:\\test.mfp"); // NOTE - hack name for now
   gpMIFLProj->Open (L"c:\\test.mfp", NULL);

   gpMIFLProj->DialogEdit ();

#if 0
   // window loop
   MSG msg;
   while( GetMessage( &msg, NULL, 0, 0 ) ) {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
      // FUTURERELEASE - help page ASPHelpCheckPage ();
   }
#endif // 0
 
   // free project
   // NOTE: Dont save because will auto save when exiting
   delete gpMIFLProj;

   // free lexicon and tts
   TTSCacheShutDown();
   MLexiconCacheShutDown ();

   ButtonBitmapReleaseAll ();
   // FUTURERELEASE - end help ASPHelpEnd ();


   // dlete beep window
   BeepWindowEnd ();


   EscUninitialize();

#ifdef _DEBUG
   _CrtCheckMemory ();
#endif // DEBUG
   return 0;
}

/*****************************************************************************
WinMain */
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
   ghInstance = hInstance;

   return M3DMainLoop (lpCmdLine, nShowCmd);
}



