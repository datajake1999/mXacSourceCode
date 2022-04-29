/*************************************************************************************
CMIFLLib.cpp - Code for managing a MIFL library

begun 24/12/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"

/* globals */
static CMem gmemStringFilter;       // store string for filtering
static DWORD gdwStringLang = 0;     // which language to look after

static PWSTR gpszTransPros = L"TransPros";
static PWSTR gpszTransProsQuick = L"rTransProsQuick_";

/*************************************************************************************
CMIFLLib::Constructor and destructor
*/
CMIFLLib::CMIFLLib (PCMIFLAppSocket pSocket)
{
   m_pSocket = pSocket;
   m_pProject = NULL;
   m_lPCMIFLMethDef.Init (sizeof(PCMIFLMeth));
   m_lPCMIFLPropDef.Init (sizeof(PCMIFLProp));
   m_lPCMIFLPropGlobal.Init (sizeof(PCMIFLProp));
   m_lPCMIFLFunc.Init (sizeof(PCMIFLProp));
   m_lPCMIFLObject.Init (sizeof(PCMIFLObject));
   m_lPCMIFLString.Init (sizeof(PCMIFLString));
   m_lPCMIFLResource.Init (sizeof(PCMIFLResource));
   m_lPCMIFLDoc.Init (sizeof(PCMIFLDoc));

   m_pNodeMisc = NULL;

   Clear();
}

CMIFLLib::~CMIFLLib (void)
{
   Clear();

   if (m_pNodeMisc)
      delete m_pNodeMisc;
}


/*************************************************************************************
CMIFLLib::Clear - Clears out the current library
*/
void CMIFLLib::Clear (void)
{
   DWORD i;
   PCMIFLMeth *ppm = (PCMIFLMeth*) m_lPCMIFLMethDef.Get(0);
   for (i = 0; i < m_lPCMIFLMethDef.Num(); i++)
      delete ppm[i];
   m_lPCMIFLMethDef.Clear();

   PCMIFLObject *ppo = (PCMIFLObject*) m_lPCMIFLObject.Get(0);
   for (i = 0; i < m_lPCMIFLObject.Num(); i++)
      delete ppo[i];
   m_lPCMIFLObject.Clear();

   PCMIFLString *pps = (PCMIFLString*) m_lPCMIFLString.Get(0);
   for (i = 0; i < m_lPCMIFLString.Num(); i++)
      delete pps[i];
   m_lPCMIFLString.Clear();

   PCMIFLResource *ppr = (PCMIFLResource*) m_lPCMIFLResource.Get(0);
   for (i = 0; i < m_lPCMIFLResource.Num(); i++)
      delete ppr[i];
   m_lPCMIFLResource.Clear();

   PCMIFLDoc *ppd = (PCMIFLDoc*) m_lPCMIFLDoc.Get(0);
   for (i = 0; i < m_lPCMIFLDoc.Num(); i++)
      delete ppd[i];
   m_lPCMIFLDoc.Clear();

   PCMIFLProp *ppp = (PCMIFLProp*) m_lPCMIFLPropDef.Get(0);
   for (i = 0; i < m_lPCMIFLPropDef.Num(); i++)
      delete ppp[i];
   m_lPCMIFLPropDef.Clear();

   ppp = (PCMIFLProp*) m_lPCMIFLPropGlobal.Get(0);
   for (i = 0; i < m_lPCMIFLPropGlobal.Num(); i++)
      delete ppp[i];
   m_lPCMIFLPropGlobal.Clear();

   PCMIFLFunc *ppf = (PCMIFLFunc*) m_lPCMIFLFunc.Get(0);
   for (i = 0; i < m_lPCMIFLFunc.Num(); i++)
      delete ppf[i];
   m_lPCMIFLFunc.Clear();

   m_szFile[0] = 0;
   m_fDirty = FALSE;
   m_fReadOnly = FALSE;
   MemZero (&m_memDescShort);
   MemZero (&m_memDescLong);
   m_dwTempID = MIFLTempID ();

   if (m_pNodeMisc)
      delete m_pNodeMisc;
   m_pNodeMisc = new CMMLNode2;
   m_pNodeMisc->NameSet (gpszNodeMisc);
}

/*************************************************************************************
CMIFLLib::ProjectSet - Sets the current project. This is important for undo reasons

inputs
   PCMIFLProj           pProject - Project
*/
void CMIFLLib::ProjectSet (PCMIFLProj pProject)
{
   m_pProject = pProject;
}



/*************************************************************************************
CMIFLLib::ProjectGet - Returns the current project
*/
PCMIFLProj CMIFLLib::ProjectGet (void)
{
   return m_pProject;
}


/*************************************************************************************
CMIFLLib::Clone - Standard API
*/
PCMIFLLib CMIFLLib::Clone (void)
{
   PCMIFLLib pNew = new CMIFLLib (m_pSocket);
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



/*************************************************************************************
CMIFLLib::Clone - Standard API
*/
BOOL CMIFLLib::CloneTo (PCMIFLLib pTo)
{
   pTo->Clear();

   pTo->m_fDirty = m_fDirty;
   pTo->m_fReadOnly = m_fReadOnly;
   pTo->m_dwTempID = m_dwTempID;
   wcscpy (pTo->m_szFile, m_szFile);
   MemCat (&pTo->m_memDescShort, (PWSTR)m_memDescShort.p);
   MemCat (&pTo->m_memDescLong, (PWSTR)m_memDescLong.p);

   if (pTo->m_pNodeMisc)
      delete pTo->m_pNodeMisc;
   pTo->m_pNodeMisc = m_pNodeMisc->Clone();

   pTo->m_pSocket = m_pSocket;
   pTo->m_pProject = m_pProject;

   // clone method defs
   pTo->m_lPCMIFLMethDef.Init (sizeof(PCMIFLMeth), m_lPCMIFLMethDef.Get(0), m_lPCMIFLMethDef.Num());
   DWORD i;
   PCMIFLMeth *ppm = (PCMIFLMeth*)pTo->m_lPCMIFLMethDef.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLMethDef.Num(); i++) {
      ppm[i] = ppm[i]->Clone();
      //ppm[i]->ParentSet (pTo);
   }

   // clone  objects
   pTo->m_lPCMIFLObject.Init (sizeof(PCMIFLObject), m_lPCMIFLObject.Get(0), m_lPCMIFLObject.Num());
   PCMIFLObject *ppo = (PCMIFLObject*)pTo->m_lPCMIFLObject.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLObject.Num(); i++) {
      ppo[i] = ppo[i]->Clone();
      //ppm[i]->ParentSet (pTo);
   }

   // clone  stringss
   pTo->m_lPCMIFLString.Init (sizeof(PCMIFLString), m_lPCMIFLString.Get(0), m_lPCMIFLString.Num());
   PCMIFLString *pps = (PCMIFLString*)pTo->m_lPCMIFLString.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLString.Num(); i++) {
      pps[i] = pps[i]->Clone();
      //ppm[i]->ParentSet (pTo);
   }

   // clone  Resourcess
   pTo->m_lPCMIFLResource.Init (sizeof(PCMIFLResource), m_lPCMIFLResource.Get(0), m_lPCMIFLResource.Num());
   PCMIFLResource *ppr = (PCMIFLResource*)pTo->m_lPCMIFLResource.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLResource.Num(); i++) {
      ppr[i] = ppr[i]->Clone();
      //ppm[i]->ParentSet (pTo);
   }

   // clone  Docss
   pTo->m_lPCMIFLDoc.Init (sizeof(PCMIFLDoc), m_lPCMIFLDoc.Get(0), m_lPCMIFLDoc.Num());
   PCMIFLDoc *ppd = (PCMIFLDoc*)pTo->m_lPCMIFLDoc.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLDoc.Num(); i++) {
      ppd[i] = ppd[i]->Clone();
      //ppm[i]->ParentSet (pTo);
   }

   // clone property defs
   pTo->m_lPCMIFLPropDef.Init (sizeof(PCMIFLProp), m_lPCMIFLPropDef.Get(0), m_lPCMIFLPropDef.Num());
   PCMIFLProp *ppp = (PCMIFLProp*)pTo->m_lPCMIFLPropDef.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLPropDef.Num(); i++) {
      ppp[i] = ppp[i]->Clone();
      //ppp[i]->ParentSet (pTo);
   }

   // clone globals
   pTo->m_lPCMIFLPropGlobal.Init (sizeof(PCMIFLProp), m_lPCMIFLPropGlobal.Get(0), m_lPCMIFLPropGlobal.Num());
   ppp = (PCMIFLProp*)pTo->m_lPCMIFLPropGlobal.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLPropGlobal.Num(); i++) {
      ppp[i] = ppp[i]->Clone();
      //ppp[i]->ParentSet (pTo);
   }

   // clone functions
   pTo->m_lPCMIFLFunc.Init (sizeof(PCMIFLFunc), m_lPCMIFLFunc.Get(0), m_lPCMIFLFunc.Num());
   PCMIFLFunc *ppf = (PCMIFLFunc*)pTo->m_lPCMIFLFunc.Get(0);
   for (i = 0; i < pTo->m_lPCMIFLFunc.Num(); i++) {
      ppf[i] = ppf[i]->Clone();
      //ppf[i]->ParentSet (pTo);
   }

   return TRUE;
}

static PWSTR gpszMIFLLib = L"MIFLLib";
static PWSTR gpszDescShort = L"DescShort";
static PWSTR gpszDescLong = L"DescLong";
static PWSTR gpszMIFLMethDef = L"MIFLMethDef";
static PWSTR gpszMIFLPropDef = L"MIFLPropDef";
static PWSTR gpszMIFLPropGlobal = L"MIFLPropGlobal";
static PWSTR gpszMIFLFunc = L"MIFLFunc";
static PWSTR gpszMIFLObject = L"MIFLObject";
static PWSTR gpszMIFLString = L"MIFLString";
static PWSTR gpszMIFLResource = L"MIFLResource";
static PWSTR gpszMIFLDoc = L"MIFLDoc";

/*************************************************************************************
CMIFLLib::MMLTo - Standard API
*/
PCMMLNode2 CMIFLLib::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLLib);

   PWSTR psz = (PWSTR)m_memDescShort.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDescShort, psz);

   psz = (PWSTR)m_memDescLong.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszDescLong, psz);

   PCMMLNode2 pSub;
   pSub = m_pNodeMisc->Clone();
   if (pSub) {
      pSub->NameSet (gpszNodeMisc);
      pNode->ContentAdd (pSub);
   }

   // write out the method defintions
   DWORD i;
   PCMIFLMeth *ppm = (PCMIFLMeth*)m_lPCMIFLMethDef.Get(0);
   for (i = 0; i < m_lPCMIFLMethDef.Num(); i++) {
      PCMMLNode2 pSub = ppm[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLMethDef);
      pNode->ContentAdd (pSub);
   } // i

   // write out the Object defintions
   PCMIFLObject *ppo = (PCMIFLObject*)m_lPCMIFLObject.Get(0);
   for (i = 0; i < m_lPCMIFLObject.Num(); i++) {
      PCMMLNode2 pSub = ppo[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLObject);
      pNode->ContentAdd (pSub);
   } // i

   // write out the String defintions
   PCMIFLString *pps = (PCMIFLString*)m_lPCMIFLString.Get(0);
   for (i = 0; i < m_lPCMIFLString.Num(); i++) {
      PCMMLNode2 pSub = pps[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLString);
      pNode->ContentAdd (pSub);
   } // i

   // write out the Resource defintions
   PCMIFLResource *ppr = (PCMIFLResource*)m_lPCMIFLResource.Get(0);
   for (i = 0; i < m_lPCMIFLResource.Num(); i++) {
      PCMMLNode2 pSub = ppr[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLResource);
      pNode->ContentAdd (pSub);
   } // i

   // write out the Doc defintions
   PCMIFLDoc *ppd = (PCMIFLDoc*)m_lPCMIFLDoc.Get(0);
   for (i = 0; i < m_lPCMIFLDoc.Num(); i++) {
      PCMMLNode2 pSub = ppd[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLDoc);
      pNode->ContentAdd (pSub);
   } // i

   // write out the property definitions
   PCMIFLProp *ppp = (PCMIFLProp*)m_lPCMIFLPropDef.Get(0);
   for (i = 0; i < m_lPCMIFLPropDef.Num(); i++) {
      PCMMLNode2 pSub = ppp[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLPropDef);
      pNode->ContentAdd (pSub);
   } // i

   // write out the globals
   ppp = (PCMIFLProp*)m_lPCMIFLPropGlobal.Get(0);
   for (i = 0; i < m_lPCMIFLPropGlobal.Num(); i++) {
      PCMMLNode2 pSub = ppp[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLPropGlobal);
      pNode->ContentAdd (pSub);
   } // i

   // write out the functions
   PCMIFLFunc *ppf = (PCMIFLFunc*)m_lPCMIFLFunc.Get(0);
   for (i = 0; i < m_lPCMIFLFunc.Num(); i++) {
      PCMMLNode2 pSub = ppf[i]->MMLTo();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMIFLFunc);
      pNode->ContentAdd (pSub);
   } // i


   return pNode;
}


/*************************************************************************************
CMIFLLib::MMLFrom - Standard API
*/
BOOL CMIFLLib::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   PWSTR psz;

   psz = MMLValueGet (pNode, gpszDescShort);
   if (psz)
      MemCat (&m_memDescShort, psz);
   psz = MMLValueGet (pNode, gpszDescLong);
   if (psz)
      MemCat (&m_memDescLong, psz);

   // read the method defintions
   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszMIFLMethDef)) {
         PCMIFLMeth pNew = new CMIFLMeth;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLMethDef.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLObject)) {
         PCMIFLObject pNew = new CMIFLObject;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLObject.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLString)) {
         PCMIFLString pNew = new CMIFLString;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLString.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLResource)) {
         PCMIFLResource pNew = new CMIFLResource;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLResource.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLDoc)) {
         PCMIFLDoc pNew = new CMIFLDoc;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLDoc.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLPropDef)) {
         PCMIFLProp pNew = new CMIFLProp;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLPropDef.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLPropGlobal)) {
         PCMIFLProp pNew = new CMIFLProp;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLPropGlobal.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMIFLFunc)) {
         PCMIFLFunc pNew = new CMIFLFunc;
         if (!pNew)
            return FALSE;
         pNew->MMLFrom (pSub);
         //pNew->ParentSet (this);
         m_lPCMIFLFunc.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszNodeMisc)) {
         if (m_pNodeMisc)
            delete m_pNodeMisc;
         m_pNodeMisc = pSub->Clone();
      }
   } // i

   return TRUE;
}


/************************************************************************************
CMIFLLib::Save - Saves the library to disk. This
sets the dirty flag to false. It uses the library's m_szFile for the file name.

NOTE: This will NOT save read-only files.

inputs
   BOOL        fForce - If TRUE save no matter what, otherwise only if dirty
   BOOL        fClearDirty - If TRUE then automatically clear the dirty flag.
               If FALSE then leave it
*/
BOOL CMIFLLib::Save (BOOL fForce, BOOL fClearDirty)
{
   if (m_fReadOnly)
      return FALSE;

   if (!m_fDirty && !fForce)
      return TRUE;
   
   PCMMLNode2 pNode = MMLTo();
   if (!pNode)
      return FALSE;

   BOOL fRet;
   if (m_pProject && m_pProject->m_pSocket->FileSysSupport())
      fRet = m_pProject->m_pSocket->FileSysSave (m_szFile, pNode);
   else
      fRet = MMLFileSave (m_szFile, &GUID_MIFLLib, pNode);
   delete pNode;

   if (fClearDirty)
      m_fDirty = FALSE;
   return fRet;
}

#ifdef _DEBUG
/************************************************************************************
NodeOutput - Outputs a node to the debug string.

inputs
   PCMMLNode2      pNode - Node
   DWORD          dwInset - Number to inset
   DWORD          dwNum - Node number -for display purposes
returns
   none
*/
static void NodeOutput (PCMMLNode2 pNode, DWORD dwInset, DWORD dwNum = 0)
{
   PWSTR psz = pNode->NameGet();
   DWORD i, dwTab;
#define INSETTAB  for(dwTab = 0; dwTab < dwInset; dwTab++) OutputDebugString ("  ")
#define NEWLINE   OutputDebugString ("\r\n")

   INSETTAB;
   char szTemp[32];
   sprintf (szTemp, "Node %d: ", (int)dwNum);
   OutputDebugString (szTemp);
   if (psz)
      OutputDebugStringW (psz);
   else
      OutputDebugString ("UNKNOWN");
   NEWLINE;

   dwInset++;
   for (i = 0; i < pNode->AttribNum(); i++) {
      PWSTR pszAttrib, pszValue;
      pNode->AttribEnum (i, &pszAttrib, &pszValue);   // OK to use this here

      INSETTAB;
      OutputDebugStringW (pszAttrib ? pszAttrib : L"UNKNOWN");
      OutputDebugString (" = ");
      OutputDebugStringW (pszValue ? pszValue : L"UNKNOWN");
      NEWLINE;
   }

   // and content
   for (i = 0; i < pNode->ContentNum(); i++) {
      PWSTR psz;
      PCMMLNode2 pSub;
      pNode->ContentEnum (i, &psz, &pSub);

      if (pSub) {
         NodeOutput (pSub, dwInset, i);
         continue;
      }

      // else, string
      INSETTAB;
      OutputDebugStringW (psz ? psz : L"UNKNOWN");
      NEWLINE;
   } // i
}

#endif // _DEBUG

/************************************************************************************
CMIFLLib::Open - Opens the file.
*/
BOOL CMIFLLib::Open (PWSTR pszFile)
{
   PCMMLNode2 pNode;

#if 0
         // Get current flag
         int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

         // Turn on leak-checking bit
         tmpFlag |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF;
         //tmpFlag |=  _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF;

         //tmpFlag = LOWORD(tmpFlag) | (_CRTDBG_CHECK_EVERY_16_DF << 4); // BUGFIX - So dont check for memory overwrites that often, make things faster

         // Set flag to the new value
         _CrtSetDbgFlag( tmpFlag );

         // test
         //char *p;
         //p = (char*)MYMALLOC (42);
         // p[43] = 0;
#endif // _DEBUG
   
   if (m_pProject && m_pProject->m_pSocket->FileSysSupport())
      pNode = m_pProject->m_pSocket->FileSysOpen (pszFile);
   else
      pNode = MMLFileOpen (pszFile, &GUID_MIFLLib);
   if (!pNode)
      return FALSE;

   if (!MMLFrom (pNode)) {
      delete pNode;
      return FALSE;
   }

   // rembmeber the file
   wcscpy (m_szFile, pszFile);

#if 0
   NodeOutput (pNode, 0);
#endif

   delete pNode;
   return TRUE;
}


/************************************************************************************
CMIFLProj::Open - Opens the file, but reads in from a resource.

inputs
   PWSTR          pszName - Name of the library to use. From the callback
   HINSTANCE      hInstance - From the callback.
   DWORD          dwRes - Resource iD. From the app callback.
*/
BOOL CMIFLLib::Open (PWSTR pszName, HINSTANCE hInstance, DWORD dwRes)
{
   PCMMLNode2 pNode = MMLFileOpen (hInstance, dwRes, "MIFL", &GUID_MIFLLib);
   if (!pNode)
      return FALSE;

   if (!MMLFrom (pNode)) {
      delete pNode;
      return FALSE;
   }

   // rembmeber the file
   wcscpy (m_szFile, pszName);
   m_fReadOnly = TRUE;

   delete pNode;
   return TRUE;
}

/*************************************************************************************
MIFLLibOpenDialog - Dialog box for opening a CMIFLLib

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL MIFLLibOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
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
   ofn.lpstrFilter = "MIFL Library(*.mfl)\0*.mfl\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save the MIFL library" :
      "Open MIFL library";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "mfl";
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


/*********************************************************************************
LibDescPage - UI
*/

BOOL LibDescPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"descshort");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pLib->m_memDescShort.p);

         pControl = pPage->ControlFind (L"desclong");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pLib->m_memDescLong.p);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"descshort")) {

            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pLib->m_memDescShort.Required (dwNeed))
               return FALSE;

            pLib->AboutToChange ();
            p->pControl->AttribGet (Text(), (PWSTR)pLib->m_memDescShort.p, (DWORD)pLib->m_memDescShort.m_dwAllocated, &dwNeed);
            pLib->Changed ();

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"desclong")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pLib->m_memDescLong.Required (dwNeed))
               return FALSE;

            pLib->AboutToChange ();
            p->pControl->AttribGet (Text(), (PWSTR)pLib->m_memDescLong.p, (DWORD)pLib->m_memDescLong.m_dwAllocated, &dwNeed);
            pLib->Changed ();

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Library description";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PROJFILE")) {
            p->pszSubString = LibraryDisplayName(pLib->m_szFile);
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}




/*********************************************************************************
LibMethDefListPage - UI
*/

BOOL LibMethDefListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of method definitions";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pLib->MethDefNum(); i++) {
               PCMIFLMeth pMeth = pLib->MethDefGet(i);

               MemCat (&gMemTemp, L"<tr><td width=33%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"methdef:");
               MemCat (&gMemTemp, (int)pMeth->m_dwTempID);
               MemCat (&gMemTemp, L"edit>");
               MemCat (&gMemTemp, L"<bold>");
               MemCatSanitize (&gMemTemp, ((PWSTR)(pMeth->m_memName.p))[0] ? (PWSTR)(pMeth->m_memName.p) : L"Not named");
               MemCat (&gMemTemp, L"</bold></a>");

               MemCat (&gMemTemp, L"</td><td width=66%>");

               // see if overridden
               PWSTR pszHigher, pszLower;
               pProj->MethDefOverridden (pLib->m_dwTempID, (PWSTR)pMeth->m_memName.p, &pszHigher, &pszLower);
               if (pszHigher) {
                  MemCat (&gMemTemp, L"<italic>(Overridden in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }
               if (pszLower) {
                  MemCat (&gMemTemp, L"<italic>(Overrides in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }

               pMeth->MemCatParam (&gMemTemp, FALSE);
               MemCat (&gMemTemp, L" - ");

               if (((PWSTR)(pMeth->m_memDescShort.p))[0]) {
                  MemCatSanitize (&gMemTemp, (PWSTR)pMeth->m_memDescShort.p);
               }
               MemCat (&gMemTemp, L"</td></tr>");
            } // i

            if (!i)
               MemCat (&gMemTemp, L"<tr><td><bold>The library doesn't contain any method definitions.</bold></td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/*********************************************************************************
LibObjectListPage - UI
*/

BOOL LibObjectListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of objects";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pLib->ObjectNum(); i++) {
               PCMIFLObject pObj = pLib->ObjectGet(i);

               MemCat (&gMemTemp, L"<tr><td width=33%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"object:");
               MemCat (&gMemTemp, (int)pObj->m_dwTempID);
               MemCat (&gMemTemp, L"edit><bold>");
               MemCatSanitize (&gMemTemp, ((PWSTR)(pObj->m_memName.p))[0] ? (PWSTR)(pObj->m_memName.p) : L"Not named");
               MemCat (&gMemTemp, L"</bold></a>");

               if (pObj->m_fAutoCreate)
                  MemCat (&gMemTemp, L" (Object)");
               else
                  MemCat (&gMemTemp, L" (Class)");

               MemCat (&gMemTemp, L"</td><td width=66%>");
               // see if overridden
               PWSTR pszHigher, pszLower;
               pProj->ObjectOverridden (pLib->m_dwTempID, (PWSTR)pObj->m_memName.p, &pszHigher, &pszLower);
               if (pszHigher) {
                  MemCat (&gMemTemp, L"<italic>(Overridden in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }
               if (pszLower) {
                  MemCat (&gMemTemp, L"<italic>(Overrides in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }

               if (((PWSTR)(pObj->m_memDescShort.p))[0]) {
                  MemCatSanitize (&gMemTemp, (PWSTR)pObj->m_memDescShort.p);
               }
               MemCat (&gMemTemp, L"</td></tr>");
            } // i

            if (!i)
               MemCat (&gMemTemp, L"<tr><td><bold>The library doesn't contain any objects.</bold></td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/*********************************************************************************
LibStringListPage - UI
*/

BOOL LibStringListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"filtertext");
         if (pControl && gmemStringFilter.p)
            pControl->AttribSet (Text(), (PWSTR)gmemStringFilter.p);

         if (pProj->m_lLANGID.Num())
            gdwStringLang = min(gdwStringLang, pProj->m_lLANGID.Num()-1);
         else
            gdwStringLang = 0;
         LANGID *plid = (LANGID*) pProj->m_lLANGID.Get(gdwStringLang);
         MIFLLangComboBoxSet (pPage, L"showlang", plid ? plid[0] : 0, pProj);
#if 0 // old code
         pControl = pPage->ControlFind (L"showlang");
         if (pControl) {
            if (pProj->m_lLANGID.Num())
               gdwStringLang = min(gdwStringLang, pProj->m_lLANGID.Num()-1);
            else
               gdwStringLang = 0;

            // clear the existing combo
            pControl->Message (ESCM_COMBOBOXRESETCONTENT);

            MemZero (&gMemTemp);

            DWORD i;
            LANGID *pl = (LANGID*)pProj->m_lLANGID.Get(0);
            for (i = 0; i < pProj->m_lLANGID.Num(); i++) {
               DWORD dwIndex = MIFLLangFind (pl[i]);
               PWSTR psz = MIFLLangGet (dwIndex, NULL);
               if (!psz)
                  psz = L"Unknown";
               
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><bold>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold>");
               MemCat (&gMemTemp, L"</elem>");
            }

            ESCMCOMBOBOXADD lba;
            memset (&lba, 0,sizeof(lba));
            lba.pszMML = (PWSTR)gMemTemp.p;

            pControl->Message (ESCM_COMBOBOXADD, &lba);

            pControl->AttribSetInt (CurSel(), (int)gdwStringLang);
         }
#endif // 0
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"showlang")) {
            DWORD dwNum = p->dwCurSel;
            if ((dwNum == gdwStringLang) || (dwNum >= pProj->m_lLANGID.Num()))
               return TRUE;// ignore

            gdwStringLang = dwNum;
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"filtertext")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!gmemStringFilter.Required (dwNeed))
               return FALSE;

            p->pControl->AttribGet (Text(), (PWSTR)gmemStringFilter.p, (DWORD)gmemStringFilter.m_dwAllocated, &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"runsearch")) {
            EscChime (ESCCHIME_INFORMATION);
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of strings";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            // what language
            if (pProj->m_lLANGID.Num())
               gdwStringLang = min(gdwStringLang, pProj->m_lLANGID.Num()-1);
            else
               gdwStringLang = 0;
            LANGID *pl = (LANGID*) pProj->m_lLANGID.Get(gdwStringLang);
            LANGID lid = (pl ? pl[0] : 0);

            DWORD i, j;
            BOOL fFound = FALSE;
            for (i = 0; i < pLib->StringNum(); i++) {
               PCMIFLString pString = pLib->StringGet(i);

               // find the language in the string
               pl = (LANGID*)pString->m_lLANGID.Get(0);
               j = MIFLLangMatch (&pString->m_lLANGID, lid, TRUE);
               //for (j = 0; j < pString->m_lLANGID.Num(); j++)
               //   if (pl[j] == lid)
               //      break;
               //if (j >=pString->m_lLANGID.Num())
               //   j = -1;  // so know is empty...

               // look at filter
               PWSTR pszFilter = (PWSTR)gmemStringFilter.p;
               // BUGFIX - search both the string and the namae
               if (pszFilter && pszFilter[0] && !MyStrIStr ((PWSTR)pString->m_memName.p, (PWSTR)gmemStringFilter.p) ) {
                  if (j == -1)
                     continue;   // empty string, so dont show

                  if (!MyStrIStr ((PWSTR)pString->m_lString.Get(j), (PWSTR)gmemStringFilter.p) )
                     continue;
               }

               // find the appropriate language

               MemCat (&gMemTemp, L"<tr><td width=33%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"string:");
               MemCat (&gMemTemp, (int)pString->m_dwTempID);
               MemCat (&gMemTemp, L"edit>");
               MemCatSanitize (&gMemTemp, ((PWSTR)pString->m_memName.p)[0] ?
                  (PWSTR)pString->m_memName.p : L"Unnamed");
               MemCat (&gMemTemp, L"</a></td><td width=66%>");

               PWSTR psz = (PWSTR)pString->m_lString.Get(j);
               if (psz)
                  MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</td></tr>");


               fFound = TRUE;
            } // i

            if (!fFound)
               MemCat (&gMemTemp, L"<tr><td>No entries were found.</td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/*********************************************************************************
LibResourceAddPage - UI
*/

BOOL LibResourceAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // do nothing for now
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszAdd = L"add:";
         DWORD dwAddLen = (DWORD)wcslen(pszAdd);

         if (!wcsncmp(psz, pszAdd, dwAddLen)) {
            DWORD dwObjID = pLib->ResourceNew (psz + dwAddLen);
            if (dwObjID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dresource:%dedit", (int) pLib->m_dwTempID, (int) dwObjID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add a resource";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            if (pLib->m_fReadOnly) {
               p->pszSubString = L"This library is read only, so it cannot have resources added to it.";
               return TRUE;
            };

            MemZero (&gMemTemp);

            DWORD i;
            BOOL fFound = FALSE;
            PCMIFLAppSocket pSocket = pProj->m_pSocket;
            for (i = 0; i < pSocket->ResourceNum(); i++) {
               MASRES res;
               if (!pSocket->ResourceEnum (i, &res))
                  continue;

               MemCat (&gMemTemp, L"<xChoiceButton name=\"add:");
               MemCatSanitize (&gMemTemp, res.pszName);
               MemCat (&gMemTemp, L"\"><bold>");
               MemCatSanitize (&gMemTemp, res.pszName);
               MemCat (&gMemTemp, L"</bold><br/>");
               MemCatSanitize (&gMemTemp, res.pszDescShort);
               MemCat (&gMemTemp, L"</xChoiceButton>");
            } // i

            if (!i)
               MemCat (&gMemTemp, L"The environment doesn't support resources.");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}




/*********************************************************************************
LibResourceListPage - UI
*/

BOOL LibResourceListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"filtertext");
         if (pControl && gmemStringFilter.p)
            pControl->AttribSet (Text(), (PWSTR)gmemStringFilter.p);
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"filtertext")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!gmemStringFilter.Required (dwNeed))
               return FALSE;

            p->pControl->AttribGet (Text(), (PWSTR)gmemStringFilter.p, (DWORD)gmemStringFilter.m_dwAllocated, &dwNeed);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"runsearch")) {
            EscChime (ESCCHIME_INFORMATION);
            pPage->Exit (MIFLRedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of resources";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            BOOL fFound = FALSE;
            for (i = 0; i < pLib->ResourceNum(); i++) {
               PCMIFLResource pResource = pLib->ResourceGet(i);

               // look at filter
               PWSTR pszFilter = (PWSTR)gmemStringFilter.p;
               // BUGFIX - search both the string and the namae
               if (pszFilter && pszFilter[0] && !MyStrIStr ((PWSTR)pResource->m_memName.p, (PWSTR)gmemStringFilter.p) ) {
                  if (MyStrIStr ((PWSTR)pResource->m_memType.p, (PWSTR)gmemStringFilter.p))
                     goto found;

                  if (!MyStrIStr ((PWSTR)pResource->m_memDescShort.p, (PWSTR)gmemStringFilter.p))
                     continue;
              }

found:
               // find the appropriate language

               MemCat (&gMemTemp, L"<tr><td width=25%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"resource:");
               MemCat (&gMemTemp, (int)pResource->m_dwTempID);
               MemCat (&gMemTemp, L"edit>");
               MemCatSanitize (&gMemTemp, ((PWSTR)pResource->m_memName.p)[0] ?
                  (PWSTR)pResource->m_memName.p : L"Unnamed");
               MemCat (&gMemTemp, L"</a></td>");

               MemCat (&gMemTemp, L"<td width=25%><bold>");
               MemCatSanitize (&gMemTemp, (PWSTR)pResource->m_memType.p);
               MemCat (&gMemTemp, L"</bold></td>");

               MemCat (&gMemTemp, L"<td width=50%>");
               MemCatSanitize (&gMemTemp, (PWSTR)pResource->m_memDescShort.p);
               MemCat (&gMemTemp, L"</td></tr>");


               fFound = TRUE;
            } // i

            if (!fFound)
               MemCat (&gMemTemp, L"<tr><td>No entries were found.</td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}




/*********************************************************************************
LibDocListPage - UI
*/

BOOL LibDocListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // do nothing for now
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"docadd")) {
            DWORD dwObjID = pLib->DocNew ();
            if (dwObjID == -1)
               return TRUE;

            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%ddoc:%dedit", (int) pLib->m_dwTempID, (int) dwObjID);
            pPage->Exit (szTemp);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of documents";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            BOOL fFound = FALSE;
            for (i = 0; i < pLib->DocNum(); i++) {
               PCMIFLDoc pDoc = pLib->DocGet(i);

               MemCat (&gMemTemp, L"<tr><td width=33%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"doc:");
               MemCat (&gMemTemp, (int)pDoc->m_dwTempID);
               MemCat (&gMemTemp, L"edit>");
               MemCatSanitize (&gMemTemp, ((PWSTR)pDoc->m_memName.p)[0] ?
                  (PWSTR)pDoc->m_memName.p : L"Unnamed");
               MemCat (&gMemTemp, L"</a></td>");

               MemCat (&gMemTemp, L"<td width=66%>");
               MemCatSanitize (&gMemTemp, (PWSTR)pDoc->m_memDescShort.p);
               MemCat (&gMemTemp, L"</td></tr>");


               fFound = TRUE;
            } // i

            if (!fFound)
               MemCat (&gMemTemp, L"<tr><td>No entries were found.</td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}

/*********************************************************************************
LibPropDefListPage - UI
*/

BOOL LibPropDefListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of property definitions";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pLib->PropDefNum(); i++) {
               PCMIFLProp pProp = pLib->PropDefGet(i);

               MemCat (&gMemTemp, L"<tr><td width=33%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"propdef:");
               MemCat (&gMemTemp, (int)pProp->m_dwTempID);
               MemCat (&gMemTemp, L"edit><bold>");
               MemCatSanitize (&gMemTemp, ((PWSTR)(pProp->m_memName.p))[0] ? (PWSTR)(pProp->m_memName.p) : L"Not named");
               MemCat (&gMemTemp, L"</bold></a>");

               MemCat (&gMemTemp, L"</td><td width=66%>");

               // see if overridden
               PWSTR pszHigher, pszLower;
               pProj->PropDefOverridden (pLib->m_dwTempID, (PWSTR)pProp->m_memName.p, &pszHigher, &pszLower);
               if (pszHigher) {
                  MemCat (&gMemTemp, L"<italic>(Overridden in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }
               if (pszLower) {
                  MemCat (&gMemTemp, L"<italic>(Overrides in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }

               if (((PWSTR)(pProp->m_memDescShort.p))[0]) {
                  MemCatSanitize (&gMemTemp, (PWSTR)pProp->m_memDescShort.p);
               }
               MemCat (&gMemTemp, L"</td></tr>");
            } // i

            if (!i)
               MemCat (&gMemTemp, L"<tr><td><bold>The library doesn't contain any property definitions.</bold></td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/*********************************************************************************
LibGlobalListPage - UI
*/

BOOL LibGlobalListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of global variables";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pLib->GlobalNum(); i++) {
               PCMIFLProp pProp = pLib->GlobalGet(i);

               MemCat (&gMemTemp, L"<tr><td width=33%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"global:");
               MemCat (&gMemTemp, (int)pProp->m_dwTempID);
               MemCat (&gMemTemp, L"edit><bold>");
               MemCatSanitize (&gMemTemp, ((PWSTR)(pProp->m_memName.p))[0] ? (PWSTR)(pProp->m_memName.p) : L"Not named");
               MemCat (&gMemTemp, L"</bold></a>");

               MemCat (&gMemTemp, L"</td><td width=66%>");
               // see if overridden
               PWSTR pszHigher, pszLower;
               pProj->GlobalOverridden (pLib->m_dwTempID, (PWSTR)pProp->m_memName.p, &pszHigher, &pszLower);
               if (pszHigher) {
                  MemCat (&gMemTemp, L"<italic>(Overridden in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }
               if (pszLower) {
                  MemCat (&gMemTemp, L" <italic>(Overrides in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }

               if (((PWSTR)(pProp->m_memDescShort.p))[0]) {
                  MemCatSanitize (&gMemTemp, (PWSTR)pProp->m_memDescShort.p);
               }
               MemCat (&gMemTemp, L"</td></tr>");
            } // i

            if (!i)
               MemCat (&gMemTemp, L"<tr><td><bold>The library doesn't contain any global variables.</bold></td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/*********************************************************************************
LibFuncListPage - UI
*/

BOOL LibFuncListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"List of functions";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LIBLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pLib->FuncNum(); i++) {
               PCMIFLFunc pFunc = pLib->FuncGet(i);

               MemCat (&gMemTemp, L"<tr><td width=33%><a href=lib:");
               MemCat (&gMemTemp, (int)pLib->m_dwTempID);
               MemCat (&gMemTemp, L"func:");
               MemCat (&gMemTemp, (int)pFunc->m_Meth.m_dwTempID);
               MemCat (&gMemTemp, L"edit>");
               MemCat (&gMemTemp, L"<bold>");
               MemCatSanitize (&gMemTemp, ((PWSTR)(pFunc->m_Meth.m_memName.p))[0] ? (PWSTR)(pFunc->m_Meth.m_memName.p) : L"Not named");
               MemCat (&gMemTemp, L"</bold></a>");

               MemCat (&gMemTemp, L"</td><td width=66%>");

               // see if overridden
               PWSTR pszHigher, pszLower;
               pProj->FuncOverridden (pLib->m_dwTempID, (PWSTR)pFunc->m_Meth.m_memName.p, &pszHigher, &pszLower);
               if (pszHigher) {
                  MemCat (&gMemTemp, L"<italic>(Overridden in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }
               if (pszLower) {
                  MemCat (&gMemTemp, L"<italic>(Overrides in ");
                  MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
                  MemCat (&gMemTemp, L")</italic><br/>");
               }

               pFunc->m_Meth.MemCatParam (&gMemTemp, FALSE);
               MemCat (&gMemTemp, L" - ");

               if (((PWSTR)(pFunc->m_Meth.m_memDescShort.p))[0]) {
                  MemCatSanitize (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memDescShort.p);
               }
               MemCat (&gMemTemp, L"</td></tr>");
            } // i

            if (!i)
               MemCat (&gMemTemp, L"<tr><td><bold>The library doesn't contain any functions.</bold></td></tr>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}


/********************************************************************************
CMIFLLib::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll positionreturns
   BOOl - TRUE if success
*/
BOOL CMIFLLib::DialogEditSingle (PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // if it's a method then use that
   PWSTR pszMethDef = L"methdef:", pszPropDef = L"propdef:", pszPropGlobal = L"global:", pszFunc = L"func:", pszObject = L"object:",
      pszString = L"string:", pszResource = L"resource:", pszDoc = L"doc:",
      pszMenuMisc = L"menumisc:";
   DWORD dwMethDefLen = (DWORD)wcslen(pszMethDef), dwPropDefLen = (DWORD)wcslen(pszPropDef), dwPropGlobalLen = (DWORD)wcslen(pszPropGlobal), dwFuncLen = (DWORD)wcslen(pszFunc), dwObjectLen = (DWORD)wcslen(pszObject),
      dwStringLen = (DWORD)wcslen(pszString), dwResourceLen = (DWORD)wcslen(pszResource), dwDocLen = (DWORD)wcslen(pszDoc),
      dwMenuMiscLen = (DWORD)wcslen(pszMenuMisc);
   if (!_wcsnicmp(pszUse, pszMethDef, dwMethDefLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwMethDefLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the method
      DWORD dwIndex = MethDefFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLMeth pMeth = MethDefGet (dwIndex);
      if (!pMeth)
         goto defpage;

      if (!pMeth->DialogEditSingle (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszObject, dwObjectLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwObjectLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the object
      DWORD dwIndex = ObjectFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLObject pObj = ObjectGet (dwIndex);
      if (!pObj)
         goto defpage;

      if (!pObj->DialogEditSingle (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszString, dwStringLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwStringLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the String
      DWORD dwIndex = StringFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLString pObj = StringGet (dwIndex);
      if (!pObj)
         goto defpage;

      if (!pObj->DialogEditSingle (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszResource, dwResourceLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwResourceLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the Resource
      DWORD dwIndex = ResourceFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLResource pObj = ResourceGet (dwIndex);
      if (!pObj)
         goto defpage;

      if (!pObj->DialogEditSingle (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszDoc, dwDocLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwDocLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the Doc
      DWORD dwIndex = DocFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLDoc pObj = DocGet (dwIndex);
      if (!pObj)
         goto defpage;

      if (!pObj->DialogEditSingle (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszPropDef, dwPropDefLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwPropDefLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the property
      DWORD dwIndex = PropDefFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLProp pProp = PropDefGet (dwIndex);
      if (!pProp)
         goto defpage;

      if (!pProp->DialogEditSinglePropDef (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszPropGlobal, dwPropGlobalLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwPropGlobalLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the property
      DWORD dwIndex = GlobalFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLProp pProp = GlobalGet (dwIndex);
      if (!pProp)
         goto defpage;

      if (!pProp->DialogEditSingleGlobal (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszFunc, dwFuncLen)) {
      // will need to extract correct library number
      DWORD dwNum;
      PWSTR pszNewUse = LinkExtractNum (pszUse + dwFuncLen, &dwNum);
      if (!pszNewUse)
         goto defpage;

      // find the property
      DWORD dwIndex = FuncFind (dwNum);
      if (dwIndex == -1)
         goto defpage;
      PCMIFLFunc pFunc = FuncGet (dwIndex);
      if (!pFunc)
         goto defpage;

      if (!pFunc->DialogEditSingle (this, pWindow, pszNewUse, pszNext, piVScroll)) {
         pszNext[0] = 0;   // so dont repeat same page, if there's an error
         return FALSE;
      }
      return TRUE;
   }
   else if (!_wcsnicmp(pszUse, pszMenuMisc, dwMenuMiscLen)) {
      DWORD dwNum = (DWORD)_wtoi(pszUse + dwMenuMiscLen);
      BOOL fRet = m_pSocket ? m_pSocket->MenuCall (m_pProject, this, pWindow, dwNum) : TRUE;
      if (fRet)
         wcscpy (pszNext, Back());   // so go bakc
      else
         wcscpy (pszNext, L"[close]");   // cancel
      return TRUE;
   }
defpage:

   // what resource to use
   DWORD dwPage = IDR_MMLLIBDESC;
   PESCPAGECALLBACK pPage = LibDescPage;

   // if it's a project link...
   if (!_wcsicmp(pszUse, L"desc")) {
      dwPage = IDR_MMLLIBDESC;
      pPage = LibDescPage;
   }
   else if (!_wcsicmp(pszUse, L"methdeflist")) {
      dwPage = IDR_MMLLIBMETHDEFLIST;
      pPage = LibMethDefListPage;
   }
   else if (!_wcsicmp(pszUse, L"objectlist")) {
      dwPage = IDR_MMLLIBOBJECTLIST;
      pPage = LibObjectListPage;
   }
   else if (!_wcsicmp(pszUse, L"stringlist")) {
      dwPage = IDR_MMLLIBSTRINGLIST;
      pPage = LibStringListPage;
   }
   else if (!_wcsicmp(pszUse, L"resourcelist")) {
      dwPage = IDR_MMLLIBRESOURCELIST;
      pPage = LibResourceListPage;
   }
   else if (!_wcsicmp(pszUse, L"doclist")) {
      dwPage = IDR_MMLLIBDOCLIST;
      pPage = LibDocListPage;
   }
   else if (!_wcsicmp(pszUse, L"resourceadd")) {
      dwPage = IDR_MMLLIBRESOURCEADD;
      pPage = LibResourceAddPage;
   }
   else if (!_wcsicmp(pszUse, L"propdeflist")) {
      dwPage = IDR_MMLLIBPROPDEFLIST;
      pPage = LibPropDefListPage;
   }
   else if (!_wcsicmp(pszUse, L"globallist")) {
      dwPage = IDR_MMLLIBGLOBALLIST;
      pPage = LibGlobalListPage;
   }
   else if (!_wcsicmp(pszUse, L"funclist")) {
      dwPage = IDR_MMLLIBFUNCLIST;
      pPage = LibFuncListPage;
   }

   PWSTR pszRet;
   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pProj = m_pProject;
   mp.pLib = this;
   mp.iVScroll = *piVScroll;
redo:
   pszRet = pWindow->PageDialog (ghInstance, dwPage, pPage, &mp);
   *piVScroll = mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, MIFLRedoSamePage()))
      goto redo;

   if (pszRet)
      wcscpy (pszNext, pszRet);
   else
      pszNext[0] = 0;
   return TRUE;
}


/********************************************************************************
CMIFLLib::AboutToChange - Call to inform the undo system that the library
is about to change
*/
void CMIFLLib::AboutToChange (void)
{
   if (m_pProject)
      m_pProject->ObjectAboutToChange (this);
   m_fDirty = TRUE;
}

/********************************************************************************
CMIFLLib::Changed - Call to inform the undo system that the library
is has to changed
*/
void CMIFLLib::Changed (void)
{
   if (m_pProject)
      m_pProject->ObjectChanged (this);
   m_fDirty = TRUE;  // just in case
}




/********************************************************************************
CMIFLLib::MethDefNum - Returns the number of method definitions in the library
*/
DWORD CMIFLLib::MethDefNum (void)
{
   return m_lPCMIFLMethDef.Num();
}


/********************************************************************************
CMIFLLib::MethDefGet - Returns a pointer to a method definition based on
the index. This object can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call MethDefSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..MethDefNum()-1
returns
   PCMIFLMeth - Method, or NULL. do NOT delte this
*/
PCMIFLMeth CMIFLLib::MethDefGet (DWORD dwIndex)
{
   PCMIFLMeth *ppm = (PCMIFLMeth*)m_lPCMIFLMethDef.Get(0);
   if (dwIndex >= m_lPCMIFLMethDef.Num())
      return NULL;
   return ppm[dwIndex];
}


static int _cdecl MIFLMethCompare (const void *elem1, const void *elem2)
{
   PCMIFLMeth pdw1, pdw2;
   pdw1 = *((PCMIFLMeth*) elem1);
   pdw2 = *((PCMIFLMeth*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_memName.p, (PWSTR)pdw2->m_memName.p);
}


/********************************************************************************
CMIFLLib::MethDefSort - Sorts all the method names. If a method name is changed
the list must be resorted.
*/
void CMIFLLib::MethDefSort (void)
{
   qsort (m_lPCMIFLMethDef.Get(0), m_lPCMIFLMethDef.Num(),
      sizeof(PCMIFLMeth), MIFLMethCompare);
}



/********************************************************************************
CMIFLLib::MethDefFind - Given a unique ID, this finds the method's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::MethDefFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLMethDef.Num();
   PCMIFLMeth *ppm  = (PCMIFLMeth*)m_lPCMIFLMethDef.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLLib::MethDefFind - Finds the method with the given name.

NOTE: This relies on the methods to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this method number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::MethDefFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLMethDef.Num();
   PCMIFLMeth *ppm  = (PCMIFLMeth*)m_lPCMIFLMethDef.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_memName.p);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLLib::MethDefNew - Create a new method definition. This returns
the unique ID of the new method. Or -1 if error
*/
DWORD CMIFLLib::MethDefNew (void)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLMeth pNew = new CMIFLMeth;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_dwTempID;
   //pNew->ParentSet (this);

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, L"NewMethod");

   AboutToChange();
   m_lPCMIFLMethDef.Add (&pNew);
   MethDefSort();
   Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::MethDefRemove - Deletes the current method from the list.

inputs
   DWORD          dwID - ID for the method
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::MethDefRemove (DWORD dwID)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = MethDefFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   AboutToChange();
   PCMIFLMeth *ppm  = (PCMIFLMeth*)m_lPCMIFLMethDef.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLMethDef.Remove (dwIndex);
   Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::MethDefAddCopy - This adds a copy of the given method to this
library.

inputs
   PCMIFLMeth        pMeth - Method
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::MethDefAddCopy (PCMIFLMeth pMeth)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLMeth pNew = pMeth->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_dwTempID = dwID = MIFLTempID(); // so have new ID
   //pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLMethDef.Add (&pNew);
   MethDefSort();
   Changed();

   return dwID;
}



/********************************************************************************
CMIFLLib::PropDefNum - Returns the number of Propod definitions in the library
*/
DWORD CMIFLLib::PropDefNum (void)
{
   return m_lPCMIFLPropDef.Num();
}


/********************************************************************************
CMIFLLib::PropDefGet - Returns a pointer to a Propod definition based on
the index. This object can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call PropDefSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..PropDefNum()-1
returns
   PCMIFLProp - Propod, or NULL. do NOT delte this
*/
PCMIFLProp CMIFLLib::PropDefGet (DWORD dwIndex)
{
   PCMIFLProp *ppm = (PCMIFLProp*)m_lPCMIFLPropDef.Get(0);
   if (dwIndex >= m_lPCMIFLPropDef.Num())
      return NULL;
   return ppm[dwIndex];
}


static int _cdecl MIFLPropCompare (const void *elem1, const void *elem2)
{
   PCMIFLProp pdw1, pdw2;
   pdw1 = *((PCMIFLProp*) elem1);
   pdw2 = *((PCMIFLProp*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_memName.p, (PWSTR)pdw2->m_memName.p);
}


/********************************************************************************
CMIFLLib::PropDefSort - Sorts all the Propod names. If a Propod name is changed
the list must be resorted.
*/
void CMIFLLib::PropDefSort (void)
{
   qsort (m_lPCMIFLPropDef.Get(0), m_lPCMIFLPropDef.Num(),
      sizeof(PCMIFLProp), MIFLPropCompare);
}



/********************************************************************************
CMIFLLib::PropDefFind - Given a unique ID, this finds the Propod's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::PropDefFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLPropDef.Num();
   PCMIFLProp *ppm  = (PCMIFLProp*)m_lPCMIFLPropDef.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLPropFind - Given a list of CMIFLProp's, this finds the properties index
given the name

inputs
   PCListFixed    plLook - List to look through. List of PCMIFLProp.
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this Propod number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLPropFind (PCListFixed plLook, PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = plLook->Num();
   PCMIFLProp *ppm  = (PCMIFLProp*)plLook->Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_memName.p);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}


/********************************************************************************
CMIFLLib::PropDefFind - Finds the Propod with the given name.

NOTE: This relies on the Propods to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this Propod number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::PropDefFind (PWSTR pszName, DWORD dwExcludeID)
{
   return CMIFLPropFind (&m_lPCMIFLPropDef, pszName, dwExcludeID);
}



/********************************************************************************
CMIFLLib::PropDefNew - Create a new Propod definition. This returns
the unique ID of the new Propod. Or -1 if error
*/
DWORD CMIFLLib::PropDefNew (void)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLProp pNew = new CMIFLProp;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_dwTempID;
   // pNew->ParentSet (this);

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, L"NewProperty");

   AboutToChange();
   m_lPCMIFLPropDef.Add (&pNew);
   PropDefSort();
   Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::PropDefRemove - Deletes the current Propod from the list.

inputs
   DWORD          dwID - ID for the Propod
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::PropDefRemove (DWORD dwID)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = PropDefFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   AboutToChange();
   PCMIFLProp *ppm  = (PCMIFLProp*)m_lPCMIFLPropDef.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLPropDef.Remove (dwIndex);
   Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::PropDefAddCopy - This adds a copy of the given Propod to this
library.

inputs
   PCMIFLProp        pProp - Propod
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::PropDefAddCopy (PCMIFLProp pProp)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLProp pNew = pProp->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_dwTempID = dwID = MIFLTempID(); // so have new ID
   // pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLPropDef.Add (&pNew);
   PropDefSort();
   Changed();

   return dwID;
}




/********************************************************************************
CMIFLLib::GlobalNum - Returns the number of Globalod definitions in the library
*/
DWORD CMIFLLib::GlobalNum (void)
{
   return m_lPCMIFLPropGlobal.Num();
}


/********************************************************************************
CMIFLLib::GlobalGet - Returns a pointer to a Globalod definition based on
the index. This object can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call GlobalSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..GlobalNum()-1
returns
   PCMIFLProp - Globalod, or NULL. do NOT delte this
*/
PCMIFLProp CMIFLLib::GlobalGet (DWORD dwIndex)
{
   PCMIFLProp *ppm = (PCMIFLProp*)m_lPCMIFLPropGlobal.Get(0);
   if (dwIndex >= m_lPCMIFLPropGlobal.Num())
      return NULL;
   return ppm[dwIndex];
}


/********************************************************************************
CMIFLLib::GlobalSort - Sorts all the Globalod names. If a Globalod name is changed
the list must be resorted.
*/
void CMIFLLib::GlobalSort (void)
{
   qsort (m_lPCMIFLPropGlobal.Get(0), m_lPCMIFLPropGlobal.Num(),
      sizeof(PCMIFLProp), MIFLPropCompare);
}



/********************************************************************************
CMIFLLib::GlobalFind - Given a unique ID, this finds the Globalod's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::GlobalFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLPropGlobal.Num();
   PCMIFLProp *ppm  = (PCMIFLProp*)m_lPCMIFLPropGlobal.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLLib::GlobalFind - Finds the Globalod with the given name.

NOTE: This relies on the Globalods to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this Globalod number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::GlobalFind (PWSTR pszName, DWORD dwExcludeID)
{
   return CMIFLPropFind (&m_lPCMIFLPropGlobal, pszName, dwExcludeID);
}



/********************************************************************************
CMIFLLib::GlobalNew - Create a new Globalod definition. This returns
the unique ID of the new Globalod. Or -1 if error
*/
DWORD CMIFLLib::GlobalNew (void)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLProp pNew = new CMIFLProp;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_dwTempID;
   // pNew->ParentSet (this);

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, L"NewGlobalVariable");

   AboutToChange();
   m_lPCMIFLPropGlobal.Add (&pNew);
   GlobalSort();
   Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::GlobalRemove - Deletes the current Globalod from the list.

inputs
   DWORD          dwID - ID for the Globalod
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::GlobalRemove (DWORD dwID)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = GlobalFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   AboutToChange();
   PCMIFLProp *ppm  = (PCMIFLProp*)m_lPCMIFLPropGlobal.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLPropGlobal.Remove (dwIndex);
   Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::GlobalAddCopy - This adds a copy of the given Globalod to this
library.

inputs
   PCMIFLProp        pGlobal - Globalod
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::GlobalAddCopy (PCMIFLProp pGlobal)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLProp pNew = pGlobal->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_dwTempID = dwID = MIFLTempID(); // so have new ID
   // pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLPropGlobal.Add (&pNew);
   GlobalSort();
   Changed();

   return dwID;
}






/********************************************************************************
CMIFLLib::FuncNum - Returns the number of Funcod definitions in the library
*/
DWORD CMIFLLib::FuncNum (void)
{
   return m_lPCMIFLFunc.Num();
}


/********************************************************************************
CMIFLLib::FuncGet - Returns a pointer to a Funcod definition based on
the index. This object can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call FuncSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..FuncNum()-1
returns
   PCMIFLFunc - Funcod, or NULL. do NOT delte this
*/
PCMIFLFunc CMIFLLib::FuncGet (DWORD dwIndex)
{
   PCMIFLFunc *ppm = (PCMIFLFunc*)m_lPCMIFLFunc.Get(0);
   if (dwIndex >= m_lPCMIFLFunc.Num())
      return NULL;
   return ppm[dwIndex];
}




static int _cdecl MIFLFuncCompare (const void *elem1, const void *elem2)
{
   PCMIFLFunc pdw1, pdw2;
   pdw1 = *((PCMIFLFunc*) elem1);
   pdw2 = *((PCMIFLFunc*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_Meth.m_memName.p, (PWSTR)pdw2->m_Meth.m_memName.p);
}

/********************************************************************************
CMIFLLib::FuncSort - Sorts all the Funcod names. If a Funcod name is changed
the list must be resorted.
*/
void CMIFLLib::FuncSort (void)
{
   qsort (m_lPCMIFLFunc.Get(0), m_lPCMIFLFunc.Num(),
      sizeof(PCMIFLFunc), MIFLFuncCompare);
}



/********************************************************************************
CMIFLLib::FuncFind - Given a unique ID, this finds the Funcod's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::FuncFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLFunc.Num();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLFunc.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_Meth.m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLLib::FuncFind - Finds the Funcod with the given name.

NOTE: This relies on the Funcods to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this Funcod number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::FuncFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLFunc.Num();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLFunc.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_Meth.m_memName.p);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_Meth.m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_Meth.m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_Meth.m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLLib::FuncNew - Create a new Funcod definition. This returns
the unique ID of the new Funcod. Or -1 if error
*/
DWORD CMIFLLib::FuncNew (void)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLFunc pNew = new CMIFLFunc;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_Meth.m_dwTempID;
   // pNew->ParentSet (this);

   MemZero (&pNew->m_Meth.m_memName);
   MemCat (&pNew->m_Meth.m_memName, L"NewFunction");

   AboutToChange();
   m_lPCMIFLFunc.Add (&pNew);
   FuncSort();
   Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::FuncRemove - Deletes the current Funcod from the list.

inputs
   DWORD          dwID - ID for the Funcod
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::FuncRemove (DWORD dwID)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = FuncFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   AboutToChange();
   PCMIFLFunc *ppm  = (PCMIFLFunc*)m_lPCMIFLFunc.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLFunc.Remove (dwIndex);
   Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::FuncAddCopy - This adds a copy of the given Funcod to this
library.

inputs
   PCMIFLFunc        pFunc - Funcod
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::FuncAddCopy (PCMIFLFunc pFunc)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLFunc pNew = pFunc->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_Meth.m_dwTempID = dwID = MIFLTempID(); // so have new ID
   // pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLFunc.Add (&pNew);
   FuncSort();
   Changed();

   return dwID;
}







/********************************************************************************
CMIFLLib::ObjectNum - Returns the number of object definitions in the library
*/
DWORD CMIFLLib::ObjectNum (void)
{
   return m_lPCMIFLObject.Num();
}


/********************************************************************************
CMIFLLib::ObjectGet - Returns a pointer to a od definition based on
the index. This object can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call ObjectSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..ObjectNum()-1
returns
   PCMIFLObject - Object, or NULL. do NOT delte this
*/
PCMIFLObject CMIFLLib::ObjectGet (DWORD dwIndex)
{
   PCMIFLObject *ppm = (PCMIFLObject*)m_lPCMIFLObject.Get(0);
   if (dwIndex >= m_lPCMIFLObject.Num())
      return NULL;
   return ppm[dwIndex];
}


static int _cdecl MIFLObjectCompare (const void *elem1, const void *elem2)
{
   PCMIFLObject pdw1, pdw2;
   pdw1 = *((PCMIFLObject*) elem1);
   pdw2 = *((PCMIFLObject*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_memName.p, (PWSTR)pdw2->m_memName.p);
}


/********************************************************************************
CMIFLLib::ObjectSort - Sorts all the Object names. If a Object name is changed
the list must be resorted.
*/
void CMIFLLib::ObjectSort (void)
{
   qsort (m_lPCMIFLObject.Get(0), m_lPCMIFLObject.Num(),
      sizeof(PCMIFLObject), MIFLObjectCompare);
}



/********************************************************************************
CMIFLLib::ObjectFind - Given a unique ID, this finds the Object's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::ObjectFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLObject.Num();
   PCMIFLObject *ppm  = (PCMIFLObject*)m_lPCMIFLObject.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLLib::ObjectFind - Finds the Object with the given name.

NOTE: This relies on the Objects to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this Object number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::ObjectFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLObject.Num();
   PCMIFLObject *ppm  = (PCMIFLObject*)m_lPCMIFLObject.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_memName.p);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLLib::ObjectNew - Create a new Object definition. This returns
the unique ID of the new Object. Or -1 if error
*/
DWORD CMIFLLib::ObjectNew (void)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLObject pNew = new CMIFLObject;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_dwTempID;
   //pNew->ParentSet (this);

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, L"NewObject");

   AboutToChange();
   m_lPCMIFLObject.Add (&pNew);
   ObjectSort();
   Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::ObjectRemove - Deletes the current Object from the list.

inputs
   DWORD          dwID - ID for the Object
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::ObjectRemove (DWORD dwID)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = ObjectFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   AboutToChange();
   PCMIFLObject *ppm  = (PCMIFLObject*)m_lPCMIFLObject.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLObject.Remove (dwIndex);
   Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::ObjectAddCopy - This adds a copy of the given Object to this
library.

inputs
   PCMIFLObject        pObject - Object
   BOOL                 fNewGUID - If TRUE then create a new GUID
   BOOL                 fUniqueName - If TRUE then unique name
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::ObjectAddCopy (PCMIFLObject pObject, BOOL fNewGUID, BOOL fUniqueName)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLObject pNew = pObject->Clone();
   if (!pNew)
      return -1;

   // if new guid
   if (fNewGUID)
      GUIDGen (&pNew->m_gID);

   // new name
   if (fUniqueName) {
      // find any ending digits and get a value
      PWSTR psz = (PWSTR)pNew->m_memName.p;
      DWORD dwLen = (DWORD)wcslen (psz);
      DWORD i;
      for (i = dwLen-1; i < dwLen; i--)
         if (!iswdigit(psz[i]))
            break;
      DWORD dwDigits = i + 1;   // so know where digits start

      // get current value
      DWORD dwValue;
      if (dwDigits < dwLen) {
         dwValue = (DWORD)_wtoi(psz + dwDigits);
         psz[dwDigits] = 0;   // so terminate
      }
      else
         dwValue = 1;
      dwValue++;  // so start at new value

      // loop through possible
      CMem memTemp;
      WCHAR szTemp[32];
      DWORD j, dwLen2;
      for (i = dwValue; ; i++) {
         MemZero (&memTemp);
         MemCat (&memTemp, psz);
         swprintf (szTemp, L"%d", (int)i);
         dwLen2 = (DWORD)wcslen(szTemp);
         if (dwLen2 < (dwLen - dwDigits))
            for (j = 0; j < dwLen - dwDigits - dwLen2; j++)
               MemCat (&memTemp, L"0");   // so have same number of zeros
         MemCat (&memTemp, szTemp);

         // see if this is unique
         if (!m_pProject)
            break;   // shouldnt happen, but pass the name
         for (j = 0; j < m_pProject->LibraryNum(); j++) {
            PCMIFLLib pLib = m_pProject->LibraryGet (j);
            if (pLib->ObjectFind ((PWSTR)memTemp.p, (DWORD)-1) != (DWORD)-1)
               break;
         } // j
         if (j < m_pProject->LibraryNum())
            continue;   // duplicate, so continue

         // else, if get here found unique
         break;
      } // i

      MemZero (&pNew->m_memName);
      MemCat (&pNew->m_memName, (PWSTR)memTemp.p);
   } // unqiue name

   DWORD dwID;
   pNew->m_dwTempID = dwID = MIFLTempID(); // so have new ID
   //pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLObject.Add (&pNew);
   ObjectSort();
   Changed();

   return dwID;
}













/********************************************************************************
CMIFLLib::StringNum - Returns the number of String definitions in the library
*/
DWORD CMIFLLib::StringNum (void)
{
   return m_lPCMIFLString.Num();
}


/********************************************************************************
CMIFLLib::StringGet - Returns a pointer to a od definition based on
the index. This String can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call StringSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..StringNum()-1
returns
   PCMIFLString - String, or NULL. do NOT delte this
*/
PCMIFLString CMIFLLib::StringGet (DWORD dwIndex)
{
   PCMIFLString *ppm = (PCMIFLString*)m_lPCMIFLString.Get(0);
   if (dwIndex >= m_lPCMIFLString.Num())
      return NULL;
   return ppm[dwIndex];
}


static int _cdecl MIFLStringCompare (const void *elem1, const void *elem2)
{
   PCMIFLString pdw1, pdw2;
   pdw1 = *((PCMIFLString*) elem1);
   pdw2 = *((PCMIFLString*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_memName.p, (PWSTR)pdw2->m_memName.p);
}


/********************************************************************************
CMIFLLib::StringSort - Sorts all the String names. If a String name is changed
the list must be resorted.
*/
void CMIFLLib::StringSort (void)
{
   qsort (m_lPCMIFLString.Get(0), m_lPCMIFLString.Num(),
      sizeof(PCMIFLString), MIFLStringCompare);
}



/********************************************************************************
CMIFLLib::StringFind - Given a unique ID, this finds the String's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::StringFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLString.Num();
   PCMIFLString *ppm  = (PCMIFLString*)m_lPCMIFLString.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLLib::StringFind - Finds the String with the given name.

NOTE: This relies on the Strings to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this String number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::StringFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLString.Num();
   PCMIFLString *ppm  = (PCMIFLString*)m_lPCMIFLString.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_memName.p);
      if (iRet > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLLib::StringNew - Create a new String definition. This returns
the unique ID of the new String. Or -1 if error
*/
DWORD CMIFLLib::StringNew (void)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLString pNew = new CMIFLString;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_dwTempID;
   //pNew->ParentSet (this);

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, L"NewString");

   AboutToChange();
   m_lPCMIFLString.Add (&pNew);
   StringSort();
   Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::StringRemove - Deletes the current String from the list.

inputs
   DWORD          dwID - ID for the String
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::StringRemove (DWORD dwID)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = StringFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   AboutToChange();
   PCMIFLString *ppm  = (PCMIFLString*)m_lPCMIFLString.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLString.Remove (dwIndex);
   Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::StringAddCopy - This adds a copy of the given String to this
library.

inputs
   PCMIFLString        pString - String
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::StringAddCopy (PCMIFLString pString)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLString pNew = pString->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_dwTempID = dwID = MIFLTempID(); // so have new ID
   //pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLString.Add (&pNew);
   StringSort();
   Changed();

   return dwID;
}









/********************************************************************************
CMIFLLib::ResourceNum - Returns the number of Resource definitions in the library
*/
DWORD CMIFLLib::ResourceNum (void)
{
   return m_lPCMIFLResource.Num();
}


/********************************************************************************
CMIFLLib::ResourceGet - Returns a pointer to a od definition based on
the index. This Resource can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call ResourceSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..ResourceNum()-1
returns
   PCMIFLResource - Resource, or NULL. do NOT delte this
*/
PCMIFLResource CMIFLLib::ResourceGet (DWORD dwIndex)
{
   PCMIFLResource *ppm = (PCMIFLResource*)m_lPCMIFLResource.Get(0);
   if (dwIndex >= m_lPCMIFLResource.Num())
      return NULL;
   return ppm[dwIndex];
}


static int _cdecl MIFLResourceCompare (const void *elem1, const void *elem2)
{
   PCMIFLResource pdw1, pdw2;
   pdw1 = *((PCMIFLResource*) elem1);
   pdw2 = *((PCMIFLResource*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_memName.p, (PWSTR)pdw2->m_memName.p);
}


/********************************************************************************
CMIFLLib::ResourceSort - Sorts all the Resource names. If a Resource name is changed
the list must be resorted.
*/
void CMIFLLib::ResourceSort (void)
{
   qsort (m_lPCMIFLResource.Get(0), m_lPCMIFLResource.Num(),
      sizeof(PCMIFLResource), MIFLResourceCompare);
}



/********************************************************************************
CMIFLLib::ResourceFind - Given a unique ID, this finds the Resource's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::ResourceFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLResource.Num();
   PCMIFLResource *ppm  = (PCMIFLResource*)m_lPCMIFLResource.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLLib::ResourceFind - Finds the Resource with the given name.

NOTE: This relies on the Resources to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this Resource number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::ResourceFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLResource.Num();
   PCMIFLResource *ppm  = (PCMIFLResource*)m_lPCMIFLResource.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_memName.p);
      if (iRet > 0) {
         // Resource occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, Resource occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLLib::ResourceNew - Create a new Resource definition. This returns
the unique ID of the new Resource. Or -1 if error

inputs
   PWSTR             pszType - Type of resource
   BOOL           fRememberForUndo - If TRUE (or unspecified), this is automatically
                  remembered for undo. If FALSE, the caller must call AboutToChange()
                  and Changed() before and after this.
*/
DWORD CMIFLLib::ResourceNew (PWSTR pszType, BOOL fRememberForUndo)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLResource pNew = new CMIFLResource;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_dwTempID;
   //pNew->ParentSet (this);

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, L"NewResource");
   MemZero (&pNew->m_memType);
   MemCat (&pNew->m_memType, pszType);

   if (fRememberForUndo)
      AboutToChange();
   m_lPCMIFLResource.Add (&pNew);
   ResourceSort();
   if (fRememberForUndo)
      Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::ResourceRemove - Deletes the current Resource from the list.

inputs
   DWORD          dwID - ID for the Resource
   BOOL           fRememberForUndo - If TRUE (or unspecified), this is automatically
                  remembered for undo. If FALSE, the caller must call AboutToChange()
                  and Changed() before and after this.
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::ResourceRemove (DWORD dwID, BOOL fRememberForUndo)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = ResourceFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   if (fRememberForUndo)
      AboutToChange();
   PCMIFLResource *ppm  = (PCMIFLResource*)m_lPCMIFLResource.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLResource.Remove (dwIndex);
   if (fRememberForUndo)
      Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::ResourceAddCopy - This adds a copy of the given Resource to this
library.

inputs
   PCMIFLResource        pResource - Resource
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::ResourceAddCopy (PCMIFLResource pResource)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLResource pNew = pResource->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_dwTempID = dwID = MIFLTempID(); // so have new ID
   //pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLResource.Add (&pNew);
   ResourceSort();
   Changed();

   return dwID;
}









/********************************************************************************
CMIFLLib::DocNum - Returns the number of Doc definitions in the library
*/
DWORD CMIFLLib::DocNum (void)
{
   return m_lPCMIFLDoc.Num();
}


/********************************************************************************
CMIFLLib::DocGet - Returns a pointer to a od definition based on
the index. This Doc can be changed, but call pLib->AboutToChange() and
pLib->Changed() so undo will work.

IF the NAME changes, then call DocSort() so resort the names right away

inputs
   DWORD          dwIndex - From 0..DocNum()-1
returns
   PCMIFLDoc - Doc, or NULL. do NOT delte this
*/
PCMIFLDoc CMIFLLib::DocGet (DWORD dwIndex)
{
   PCMIFLDoc *ppm = (PCMIFLDoc*)m_lPCMIFLDoc.Get(0);
   if (dwIndex >= m_lPCMIFLDoc.Num())
      return NULL;
   return ppm[dwIndex];
}


static int _cdecl MIFLDocCompare (const void *elem1, const void *elem2)
{
   PCMIFLDoc pdw1, pdw2;
   pdw1 = *((PCMIFLDoc*) elem1);
   pdw2 = *((PCMIFLDoc*) elem2);

   return _wcsicmp((PWSTR)pdw1->m_memName.p, (PWSTR)pdw2->m_memName.p);
}


/********************************************************************************
CMIFLLib::DocSort - Sorts all the Doc names. If a Doc name is changed
the list must be resorted.
*/
void CMIFLLib::DocSort (void)
{
   qsort (m_lPCMIFLDoc.Get(0), m_lPCMIFLDoc.Num(),
      sizeof(PCMIFLDoc), MIFLDocCompare);
}



/********************************************************************************
CMIFLLib::DocFind - Given a unique ID, this finds the Doc's index

inputs
   DWORD          dwID - unique ID
returns
   DWORD - index
*/
DWORD CMIFLLib::DocFind (DWORD dwID)
{
   DWORD dwNum = m_lPCMIFLDoc.Num();
   PCMIFLDoc *ppm  = (PCMIFLDoc*)m_lPCMIFLDoc.Get(0);
   DWORD i;
   for (i = 0; i < dwNum; i++)
      if (ppm[i]->m_dwTempID == dwID)
         return i;

   return -1;
}


/********************************************************************************
CMIFLLib::DocFind - Finds the Doc with the given name.

NOTE: This relies on the Docs to be sorted to work.

inputs
   PWSTR          pszName - Name to look for
   DWORD          dwExcludeID - If this isn't -1 then exclude this Doc number
returns
   DWORD - Index into the list, or -1 if cant find
*/
DWORD CMIFLLib::DocFind (PWSTR pszName, DWORD dwExcludeID)
{
   DWORD dwCur, dwTest;
   DWORD dwNum = m_lPCMIFLDoc.Num();
   PCMIFLDoc *ppm  = (PCMIFLDoc*)m_lPCMIFLDoc.Get(0);
   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      // see how compares
      int iRet = _wcsicmp(pszName, (PWSTR)ppm[dwTry]->m_memName.p);
      if (iRet > 0) {
         // Doc occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iRet == 0) {
         dwCur = dwTry;
         break;   // match
      }
      
      // else, Doc occurs before. so throw out dwTry
      continue;
   } // dwTest

   if (dwCur >= dwNum)
      return -1;  // cant find at all

   // in case have multiple entries with the same name step back
   while (dwCur && !_wcsicmp(pszName, (PWSTR)ppm[dwCur-1]->m_memName.p))
      dwCur--;

   // find match
   while (TRUE) {
      if (_wcsicmp(pszName, (PWSTR)ppm[dwCur]->m_memName.p))
         return -1;  // no more matches

      if (dwExcludeID == -1)
         return dwCur;  // found first one

      // else make sure ID not the same
      if (ppm[dwCur]->m_dwTempID != dwExcludeID)
         return dwCur;

      // else, try next
      dwCur++;
      if (dwCur >= dwNum)
         return -1;
      continue;
   }

   return -1;  // shouldn get here
}



/********************************************************************************
CMIFLLib::DocNew - Create a new Doc definition. This returns
the unique ID of the new Doc. Or -1 if error

*/
DWORD CMIFLLib::DocNew (void)
{
   if (m_fReadOnly)
      return -1;

   PCMIFLDoc pNew = new CMIFLDoc;
   if (!pNew)
      return -1;
   DWORD dwID = pNew->m_dwTempID;
   //pNew->ParentSet (this);

   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, L"NewDocument");

   AboutToChange();
   m_lPCMIFLDoc.Add (&pNew);
   DocSort();
   Changed();

   return dwID;
}


/********************************************************************************
CMIFLLib::DocRemove - Deletes the current Doc from the list.

inputs
   DWORD          dwID - ID for the Doc
returns
   BOOL - TRUE if found and removed
*/
BOOL CMIFLLib::DocRemove (DWORD dwID)
{
   if (m_fReadOnly)
      return FALSE;

   DWORD dwIndex = DocFind (dwID);
   if (dwIndex == -1)
      return FALSE;

   AboutToChange();
   PCMIFLDoc *ppm  = (PCMIFLDoc*)m_lPCMIFLDoc.Get(0);
   delete ppm[dwIndex];
   m_lPCMIFLDoc.Remove (dwIndex);
   Changed();

   return TRUE;
}



/********************************************************************************
CMIFLLib::DocAddCopy - This adds a copy of the given Doc to this
library.

inputs
   PCMIFLDoc        pDoc - Doc
returns
   DWORD - New ID, or -1 if error
*/
DWORD CMIFLLib::DocAddCopy (PCMIFLDoc pDoc)
{
   if (m_fReadOnly)
      return FALSE;

   PCMIFLDoc pNew = pDoc->Clone();
   if (!pNew)
      return -1;
   DWORD dwID;
   pNew->m_dwTempID = dwID = MIFLTempID(); // so have new ID
   //pNew->ParentSet (this);

   AboutToChange();
   m_lPCMIFLDoc.Add (&pNew);
   DocSort();
   Changed();

   return dwID;
}



/********************************************************************************
CMIFLLib::Merge - This merges two or more libraries together into this library.

inputs
   PCMIFLLib      *ppl - Pointer to an array of PCMIFLLib which are the libraries
   DWORD          dwNum - Number of libraries in ppl
   LANGID         lid - Language to filter down to, or -1 if keep all languages
   PCMIFLErrors   pErr - Where to add the errors to. THis can be NULLf
returns
   BOOL - TRUE if success
*/
BOOL CMIFLLib::Merge (PCMIFLLib *ppl, DWORD dwNum, BOOL fKeepDocs, LANGID lid, PCMIFLErrors pErr)
{
   // make a list of PWSTR that can be used for the compares
   CMem memsz;
   if (!memsz.Required (dwNum * sizeof(PWSTR)))
      return FALSE;
   PWSTR *ppsz = (PWSTR*) memsz.p;

   // changed notiication
   AboutToChange ();

   // clear this one out, save the name first
   WCHAR szFile[256];
   DWORD dwID = m_dwTempID;
   wcscpy (szFile, m_szFile);
   Clear ();
   wcscpy (m_szFile, szFile);
   m_dwTempID = dwID;

   // short description
   if (fKeepDocs)
      MemCat (&m_memDescShort, L"Merged library.");

   // allocate memory of a list of data types
   CMem memList;
   if (!memList.Required (dwNum * sizeof(PCListFixed) + 2 * dwNum * sizeof(PVOID) + 3 * dwNum * sizeof(DWORD))) {
      Changed();
      return FALSE;
   }
   PCListFixed *paList = (PCListFixed*) memList.p;
   PVOID *papElem = (PVOID*) (paList + dwNum);
   PVOID *papUse = papElem + dwNum;
   DWORD *padwCount = (DWORD*) (papUse + dwNum);
   DWORD *padwCur = padwCount + dwNum;
   DWORD *padwLibFrom = padwCur + dwNum;

   // loop through all the types of data
   DWORD dwType, dwLib;
   for (dwType = 0; dwType < 8; dwType++) {
      // if type==6 (doc) then dont bother if ignoring docs
      if ((dwType == 6) && !fKeepDocs)
         continue;

      // get pointers to all lists
      for (dwLib = 0; dwLib < dwNum; dwLib++) {
         switch (dwType) {
         case 0:  // methdef
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLMethDef);
            break;
         case 1:  // propdef
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLPropDef);
            break;
         case 2:  // propglobal
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLPropGlobal);
            break;
         case 3:  // func
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLFunc);
            break;
         case 4:  // string
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLString);
            break;
         case 5:  // resource
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLResource);
            break;
         case 6:  // doc
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLDoc);
            break;
         case 7:  // object
            paList[dwLib] = &(ppl[dwLib]->m_lPCMIFLObject);
            break;
         } // switch

         // fill in the count and other points
         papElem[dwLib] = paList[dwLib]->Get(0);
         padwCount[dwLib] = paList[dwLib]->Num();
      } // dwLib

      // repeat as long as can, adding elements...
      memset (padwCur, 0, sizeof(DWORD)*dwNum);
      while (TRUE) {
         // fill in strings...
         for (dwLib = 0; dwLib < dwNum; dwLib++) {
            if (padwCur[dwLib] >= padwCount[dwLib]) {
               ppsz[dwLib] = NULL;  // past end of list
               continue;
            }

            // else, get string
            PWSTR psz;
            switch (dwType) {
            case 0:  // methdef
               psz = (PWSTR) ((PCMIFLMeth*) papElem[dwLib])[padwCur[dwLib]]->m_memName.p;
               break;
            case 1:  // propdef
               psz = (PWSTR) ((PCMIFLProp*) papElem[dwLib])[padwCur[dwLib]]->m_memName.p;
               break;
            case 2:  // propglobal
               psz = (PWSTR) ((PCMIFLProp*) papElem[dwLib])[padwCur[dwLib]]->m_memName.p;
               break;
            case 3:  // func
               psz = (PWSTR) ((PCMIFLFunc*) papElem[dwLib])[padwCur[dwLib]]->m_Meth.m_memName.p;
               break;
            case 4:  // string
               psz = (PWSTR) ((PCMIFLString*) papElem[dwLib])[padwCur[dwLib]]->m_memName.p;
               break;
            case 5:  // resource
               psz = (PWSTR) ((PCMIFLResource*) papElem[dwLib])[padwCur[dwLib]]->m_memName.p;
               break;
            case 6:  // doc
               psz = (PWSTR) ((PCMIFLDoc*) papElem[dwLib])[padwCur[dwLib]]->m_memName.p;
               break;
            case 7:  // object
               psz = (PWSTR) ((PCMIFLObject*) papElem[dwLib])[padwCur[dwLib]]->m_memName.p;
               break;
            } // switch
            ppsz[dwLib] = psz;
         } // dwLib

         // now that have all the strings, find out which is best
         DWORD dwIndex = MIFLStringArrayComp (ppsz, dwNum);
         if (dwIndex == -1)
            break;   // out of elements

         // compact the ones together that need for merge
         DWORD dwFrom = 0, dwTo = 0;
         for (dwFrom = 0; dwFrom < dwNum; dwFrom++) {
            if (!ppsz[dwFrom])
               continue;   // not used

            // else keep
            papUse[dwTo] = ((PCMIFLMeth*)papElem[dwFrom])[padwCur[dwFrom]];
            padwLibFrom[dwTo] = ppl[dwFrom]->m_dwTempID;
            dwTo++;

            // also, increate the counter so will look elsewhere next time
            padwCur[dwFrom]++;
         } // dwFom
         if (!dwTo)
            break;   // shouldnt happen

         // merge the current item into the library
         PCMIFLMeth pMeth;
         PCMIFLProp pProp;
         PCMIFLFunc pFunc;
         PCMIFLString pString;
         PCMIFLDoc pDoc;
         PCMIFLObject pObject;
         PCMIFLResource pRes;
         DWORD i;
         switch (dwType) {
         case 0:  // methdef
            pMeth = (PCMIFLMeth) papUse[0];
            if (!(pMeth = pMeth->Clone(fKeepDocs)))
               return FALSE;
            pMeth->m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLMethDef.Add (&pMeth);
            break;
         case 1:  // propdef
            pProp = (PCMIFLProp) papUse[0];
            if (!(pProp = pProp->Clone(fKeepDocs)))
               return FALSE;
            pProp->m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLPropDef.Add (&pProp);
            break;
         case 2:  // propglobal
            pProp = (PCMIFLProp) papUse[0];
            if (!(pProp = pProp->Clone(fKeepDocs)))
               return FALSE;
            pProp->m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLPropGlobal.Add (&pProp);

            // BUGFIX - If override globals then copy help over
            for (dwFrom = 1; dwFrom < dwTo; dwFrom++) {
               for (i = 0; i < 2; i++)
                  if (!((PWSTR)pProp->m_aMemHelp[i].p)[0])
                     MemCat (&pProp->m_aMemHelp[i], (PWSTR) ((PCMIFLProp)papUse[dwFrom])->m_aMemHelp[i].p);
               if (!((PWSTR)pProp->m_memDescLong.p)[0])
                  MemCat (&pProp->m_memDescLong, (PWSTR) ((PCMIFLProp)papUse[dwFrom])->m_memDescLong.p);
               if (!((PWSTR)pProp->m_memDescShort.p)[0])
                  MemCat (&pProp->m_memDescShort, (PWSTR) ((PCMIFLProp)papUse[dwFrom])->m_memDescShort.p);
            } // dwFrom

            break;
         case 3:  // func
            pFunc = (PCMIFLFunc) papUse[0];
            if (!(pFunc = pFunc->Clone(fKeepDocs)))
               return FALSE;
            pFunc->m_Meth.m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLFunc.Add (&pFunc);

            // BUGFIX - Layer documentation
            for (dwFrom = 1; dwFrom < dwTo; dwFrom++) {
               if (!((PWSTR)pFunc->m_Meth.m_memDescLong.p)[0])
                  MemCat (&pFunc->m_Meth.m_memDescLong, (PWSTR) ((PCMIFLFunc)papUse[dwFrom])->m_Meth.m_memDescLong.p);
               if (!((PWSTR)pFunc->m_Meth.m_memDescShort.p)[0])
                  MemCat (&pFunc->m_Meth.m_memDescShort, (PWSTR) ((PCMIFLFunc)papUse[dwFrom])->m_Meth.m_memDescShort.p);
            } // dwFrom
            break;
         case 4:  // string
            pString = (PCMIFLString) papUse[0];
            if (!(pString = pString->Clone(lid)))
               return FALSE;
            pString->m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLString.Add (&pString);
            break;
         case 5:  // resource
            pRes = (PCMIFLResource) papUse[0];
            if (!(pRes = pRes->Clone(fKeepDocs, lid)))
               return FALSE;
            pRes->m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLResource.Add (&pRes);
            break;
         case 6:  // doc
            pDoc = (PCMIFLDoc) papUse[0];
            if (!(pDoc = pDoc->Clone()))
               return FALSE;
            pDoc->m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLDoc.Add (&pDoc);
            break;
         case 7:  // object
            // BUGFIX - Had a case where if only one object of type then optimized, but
            // this cuased all sorts of problems because never set up dwLibOrig properly,
            // so took out
            //if (dwTo == 1) {
            //   // only one of a kind, so no point merging
            //   pObject = (PCMIFLObject) papUse[0];
            //   if (!(pObject = pObject->Clone(fKeepDocs)))
            //      return FALSE;
            //}
            //else {
               // overlapping objects, so merge
               pObject = new CMIFLObject;
               if (!pObject)
                  return FALSE;

               if (!pObject->Merge ((PCMIFLObject*)papUse, padwLibFrom, dwTo, fKeepDocs, pErr)) {
                  delete pObject;
                  return FALSE;
               }
            //}
            pObject->m_dwLibOrigFrom = padwLibFrom[0];
            m_lPCMIFLObject.Add (&pObject);

            // check for errors...
            if (pObject->m_fAutoCreate && pErr && IsEqualGUID(pObject->m_gID, GUID_NULL)) {
               pErr->Add (L"The object must have an ID.", pObject->m_dwLibOrigFrom, pObject, TRUE);
            }

            break;
         } // switch

      } // while TRUE

      // if got here done with the merging
   } // dwType

   // changed notification
   Changed();
   return TRUE;
}



/********************************************************************************
CMIFLLib::ObjectAllSuper - Enumerates all superclasses of an object, in order
of priority.

NOTE: This only checks for dependencies within the same lib. In general, it's
only useful after all the project's libs have been merged into one.

inputs
   PWSTR                pszName - Pointer to the string for the object. If it somehow
                           references itself (pszName) then FALSE will be returned
                           from the function.
   PCListVariable       plList - This list is added onto, with a list of strings (PWSTR)
                           that are superclasses. This is NOT cleared by the caller, and
                           is in fact referenced so that duplicates won't be added.
   PWSTR                pszRecurse - Usually, this is  NULL, unless
                           it's recursing onto itself, in which case this is the
                           pointer to the string to look for to make sure not
                           recursing on itself
returns
   BOOL - TRUE if list is filled and references ok, FALSE if recurse to self
*/
BOOL CMIFLLib::ObjectAllSuper (PWSTR pszName, PCListVariable plList, PWSTR pszRecurse)
{
   BOOL fRet = TRUE;

   // if this matches the recurse check then will want to return fail
   if (pszRecurse && !_wcsicmp (pszName, pszRecurse))
      return FALSE;

   // if the object is already in the list then exist
   DWORD i, j;
   for (i = 0; i < plList->Num(); i++)
      if (!_wcsicmp(pszName, (PWSTR)plList->Get(i)))
         return fRet;

   if (pszRecurse)
      // not first time around, so add itself to list.
      plList->Add (pszName, (wcslen(pszName)+1)*sizeof(WCHAR));
   else
      pszRecurse = pszName;
   // NOTE: If first time, don't add own object to list

   PCMIFLObject pObj = ObjectGet(ObjectFind(pszName, -1));
   if (!pObj)
      return TRUE;

   // loop through all superclass objects
   for (j = 0; j < pObj->m_lClassSuper.Num(); j++)
      fRet &= ObjectAllSuper ((PWSTR)pObj->m_lClassSuper.Get(j), plList, pszRecurse);

   return fRet;
}



/********************************************************************************
CMIFLLib::KillDead - Loops through all the objects in the library and a) kills dead
parameters, and b) kills resources/strings without any info.

inputs
   BOOL        fLowerCase - If TRUE this lowercases all the method, func, etc. strings
*/
void CMIFLLib::KillDead (BOOL fLowerCase)
{
   if (m_fReadOnly)
      return;

   AboutToChange();

   DWORD i;
   // method defs
   for (i = 0; i < MethDefNum(); i++) {
      PCMIFLMeth pMeth = MethDefGet(i);
      pMeth->KillDead (NULL, fLowerCase);
   } // i

   // property defs
   if (fLowerCase) for (i = 0; i < PropDefNum(); i++) {
      PCMIFLProp pProp = PropDefGet(i);
      MIFLToLower ((PWSTR)pProp->m_memName.p);
   } // i

   // globals
   if (fLowerCase) for (i = 0; i < GlobalNum(); i++) {
      PCMIFLProp pProp = GlobalGet(i);
      MIFLToLower ((PWSTR)pProp->m_memName.p);
   }

   // functions
   for (i = 0; i < FuncNum(); i++) {
      PCMIFLFunc pFunc = FuncGet(i);
      pFunc->KillDead (NULL, fLowerCase);
   } // i

   // Objects
   for (i = 0; i < ObjectNum(); i++) {
      PCMIFLObject pObj = ObjectGet(i);
      pObj->KillDead (NULL, fLowerCase);
   } // i

   // strings...
   PCMIFLString *pps = (PCMIFLString*) m_lPCMIFLString.Get(0);
   for (i = m_lPCMIFLString.Num()-1; i < m_lPCMIFLString.Num(); i--) {
      PCMIFLString ps = pps[i];

      if (!ps->m_lLANGID.Num()) {
         // no languages are supported in this, so might as well kill
         delete ps;
         m_lPCMIFLString.Remove (i);
         pps = (PCMIFLString*) m_lPCMIFLString.Get(0);
         continue;
      }

      // lowercase name
      if (fLowerCase) MIFLToLower ((PWSTR) ps->m_memName.p);
   } // i

   // resources...
   PCMIFLResource *ppr = (PCMIFLResource*) m_lPCMIFLResource.Get(0);
   for (i = m_lPCMIFLResource.Num()-1; i < m_lPCMIFLResource.Num(); i--) {
      PCMIFLResource ps = ppr[i];

      if (!ps->m_lLANGID.Num()) {
         // no languages are supported in this, so might as well kill
         delete ps;
         m_lPCMIFLResource.Remove (i);
         ppr = (PCMIFLResource*) m_lPCMIFLResource.Get(0);
         continue;
      }

      // lowercase name
      if (fLowerCase) MIFLToLower ((PWSTR) ps->m_memName.p);
   } // i

   Changed();
}



/********************************************************************************
CMIFLLib::TransProsQuickFind - Given some text for transplanted prosody,
finds an existing resource.

inputs
   PWSTR          pszText - Text for the quick transplanted prosody
   LANGID         lid - Language
returns
   DWORD - Unique ID for the resource, or -1 if can't find
*/
DWORD CMIFLLib::TransProsQuickFind (PWSTR pszText, LANGID lid)
{
   // if empty text then don't find
   if (!pszText || !pszText[0])
      return (DWORD)-1;

   // loop over all the resources
   DWORD i;
   PWSTR pszPreModText;
   DWORD dwNum = ResourceNum();
   for (i = 0; i < dwNum; i++) {
      PCMIFLResource pRes = ResourceGet (i);
      
      if (!pRes->TransProsQuickText (lid, NULL, &pszPreModText))
         continue;   // not right type
      if (!pszPreModText)
         continue;   // not found

      if (!_wcsicmp(pszText, pszPreModText))
         return pRes->m_dwTempID;
   } // i

   // else, can't find
   return (DWORD)-1;
}



/********************************************************************************
CMIFLLib::TransProsQuickEnum - Enumerate a group of strings looking for transplanted
prosody resources.

inputs
   PCListVariable       plText - List of strings
   PCListFixed          plLANGID - List of langid for each stirng. If NULL,
                           then uses lid
   LANGID               lid - Used if !plLANGID. The language ID to look for
   PCListFixed          plResID - Initialized to sizeof(DWORD) and filled with
                           unique resource ID for each string, or -1 if no
                           transplanted prosody could be found for the string
returns
   none
*/
void CMIFLLib::TransProsQuickEnum (PCListVariable plText, PCListFixed plLANGID,
                                   LANGID lid, PCListFixed plResID)
{
   plResID->Init (sizeof(DWORD));

   LANGID *plid = plLANGID ? (LANGID*)plLANGID->Get(0) : NULL;
   DWORD i, dwID;
   PWSTR psz;
   for (i = 0; i < plText->Num(); i++) {
      psz = (PWSTR)plText->Get(i);

      dwID = TransProsQuickFind (psz, plid ? plid[i] : lid);
      plResID->Add (&dwID);
   } // i
}


/********************************************************************************
CMIFLLib::TransProsQuickDeleteUI - Asks if a user wants to delete
a transplanted prosody, specfically asking UI

inputs
   PCEscPage            pPage - Page to display the UI from.
   PWSTR                pszText - Text
   LANGID               lid - language
returns
   BOOL - TRUE if deleted. FALSE if not
*/
BOOL CMIFLLib::TransProsQuickDeleteUI (PCEscPage pPage, PWSTR pszText, LANGID lid)
{
   // try to find the resource
   DWORD dwID = TransProsQuickFind (pszText, lid);
   if (dwID == (DWORD)-1) {
      pPage->MBWarning (L"The transplanted prosody string doesn't exist.");
      return TRUE;   // effectively deleted
   }

   // verify
   if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the transplanted prosody string?"))
      return FALSE;

   // delete
   ResourceRemove (dwID);
   return TRUE;
}


/********************************************************************************
CMIFLLib::TransProsQuickDelete - Deletes transplanted prosody resources that
were used at the start of editing, but are no longer used now.

inputs
   PCListFixed          plResIDOrig - Original list of resource IDs, from TransProsQuickEnum(),
                           before editing
   PCListFixed          plResIDCur - Current list of resource IDs, from TransProsQuickEnum()
                           as it was just called
returns
   none
*/
void CMIFLLib::TransProsQuickDelete (PCListFixed plResIDOrig, PCListFixed plResIDCur)
{
   DWORD *padwIDOrig = (DWORD*)plResIDOrig->Get(0);
   DWORD *padwIDCur = (DWORD*)plResIDCur->Get(0);
   DWORD i, j;
   for (i = 0; i < plResIDOrig->Num(); i++) {
      DWORD dwToDel = padwIDOrig[i];
      if (dwToDel == (DWORD)-1)
         continue;   // empty

      for (j = 0; j < plResIDCur->Num(); j++)
         if (padwIDCur[j] == dwToDel)
            break;   // found it
      if (j < plResIDCur->Num())
         continue;   // already found, so don't delete

      // else, delete
      ResourceRemove (dwToDel);
   } // i
}


/********************************************************************************
CMIFLLib::TransProsQuickDialog - Either modifies an existing resource, or
adds a new one.

inputs
   PCEscWindow       pWindow - Window to use. If NULL then create off of hWnd
   HWND              hWnd - Usedif !pWindow, create a dialog window off of this.
   LANGID            lid - Language ID to use.
   PWSTR             pszCreatedBy - For the comments, so know which string/property created this.
                        Can be NULL.
   PWSTR             pszText - Text that is to be transplanted
returns
   DWORD - 0 if user pressed cancel or closed window, 1 if pressed back, 2 if accepted
      transplanted prosody result
*/
DWORD CMIFLLib::TransProsQuickDialog (PCEscWindow pWindow, HWND hWnd,
                                      LANGID lid, PWSTR pszCreatedBy, PWSTR pszText)
{
   // error if no text
   if (!pszText || !pszText[0]) {
      EscMessageBox ((pWindow && pWindow->m_hWnd) ? pWindow->m_hWnd : hWnd,
         L"Transplanted prosody", L"You must type in some text to transplant.",
         L"You need to type in some text first.", MB_OK);
      return 1;
   }

   // find an existing one
   DWORD dwID = TransProsQuickFind (pszText, lid);
   PCMIFLResource pRes;
   if (dwID != (DWORD)-1) {
      // already exists, so modify
      pRes = ResourceGet (ResourceFind (dwID));
      if (!pRes)
         return 1;   // shouldnt happen
      return pRes->TransProsQuickDialog (this, pWindow, hWnd, lid, pszCreatedBy, pszText);
   }

   // else, create a new name
   GUID g;
   WCHAR szName[64];
   while (TRUE) {
      GUIDGen (&g);
      wcscpy (szName, gpszTransProsQuick);
      MMLBinaryToString ((PBYTE) &g, sizeof(g), szName + wcslen(szName));

      if ((DWORD)-1 == ResourceFind (szName, (DWORD)-1))
         break;   // unique
   }

   dwID = ResourceNew (gpszTransPros);
   if (dwID == (DWORD)-1)
      return 1;   // shouldn happen
   pRes = ResourceGet (ResourceFind (dwID));
   if (!pRes)
      return 1;   // shoudltn happen
   MemZero (&pRes->m_memName);
   MemCat (&pRes->m_memName, szName);
   ResourceSort();

   DWORD dwRet = pRes->TransProsQuickDialog (this, pWindow, hWnd, lid, pszCreatedBy, pszText);
   if (dwRet == 2)
      return dwRet;  // all done

   // else, pressed cancel, so need to delete temporary resource
   ResourceRemove (dwID);
   return dwRet;
}