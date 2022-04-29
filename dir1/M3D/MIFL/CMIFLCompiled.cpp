/*************************************************************************************
CMIFLCompiled.cpp - Code for compiling and handling the compiled version

begun 21/1/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


#define TOTALSTEPS         30.0        // total number of steps in compiling


// flags for TokenParseStatement
#define TPS_INLOOP            0x0001      // in a while, or for loop, where can do continue and break
#define TPS_INSWITCH          0x0002      // in a switch statement where can do break
#define TPS_INBLOCK           0x0004      // in block of code, so cant create vars

static PMIFLTOKEN TokensParseStatements (PMIFLTOKEN pToken, DWORD dwFlags,
                                 PCMIFLErrors pErr, PWSTR pszMethName, PWSTR pszErrLink);
static DWORD ErrObjectToID (DWORD dwLibOrigID, PCMIFLProj pProj, PCMIFLObject pObj);

/*************************************************************************************
CMIFLErrors::Constructor and destructor
*/
CMIFLErrors::CMIFLErrors (void)
{
   m_lMIFLERR.Init (sizeof(MIFLERR));
   Clear();
}

CMIFLErrors::~CMIFLErrors (void)
{
   // do nothing for now
}

/*************************************************************************************
CMIFLErrors::Clear - wipes out the current contents
*/
void CMIFLErrors::Clear (void)
{
   m_lMIFLERR.Clear();
   m_lDisplay.Clear();
   m_lLink.Clear();
   m_lObject.Clear();

   m_dwNumError = 0;
}

/*************************************************************************************
CMIFLErrors::Num - Returns the number of errors/warnings in the list
*/
DWORD CMIFLErrors::Num (void)
{
   return m_lMIFLERR.Num();
}

/*************************************************************************************
CMIFLErrors::Get - Returns a poitner to the MIFLERR so that it can be displayed
*/
PMIFLERR CMIFLErrors::Get (DWORD dwIndex)
{
   return (PMIFLERR) m_lMIFLERR.Get(dwIndex);
}


/*************************************************************************************
CMIFLErrors::TooManyErrors - Returns TRUE if there are too many errors to continue
*/
BOOL CMIFLErrors::TooManyErrors (void)
{
   if (m_dwNumError < 50)
      return FALSE;
   else
      return TRUE;
}

/*************************************************************************************
ErrObjectToID - Given a pObject, this figures out the ID of the object

inputs
   DWORD       dwLibOrigID - Original library where object occurs
   PCMIFLProj  pProj - Original project
   PCMIFLObject pObj - Object (that results from the merge)
returns
   DWORD - m_dwTempID for object as it appears in dwLibOrigID
*/
static DWORD ErrObjectToID (DWORD dwLibOrigID, PCMIFLProj pProj, PCMIFLObject pObj)
{
   PCMIFLLib pLib = pProj->LibraryGet (pProj->LibraryFind (dwLibOrigID));
   if (!pLib)
      return 0;

   PCMIFLObject pFind = pLib->ObjectGet(pLib->ObjectFind ((PWSTR)pObj->m_memName.p, -1));
   if (!pFind)
      return 0;

   return pFind->m_dwTempID;
}

/*************************************************************************************
ErrTypeToString - Takes a dwType from MIFLIDENT and returns a string for the type

inputs
   DWORD          dwType - MIFLI_XXX
returns
   PWSTR psz - String.
*/
PWSTR ErrTypeToString (DWORD dwType)
{
   switch (dwType) {
   case MIFLI_TOKEN:
      return L"token";
   case MIFLI_METHDEF:
      return L"method definition";
   case MIFLI_PROPDEF:
      return L"property definition";
   case MIFLI_GLOBAL:
      return L"gobal variable";
   case MIFLI_FUNC:
      return L"function";
   case MIFLI_OBJECT:
      return L"object";
   case MIFLI_STRING:
      return L"string";
   case MIFLI_RESOURCE:
      return L"resource";
   case MIFLI_METHPRIV:
      return L"private method";
   case MIFLI_PROPPRIV:
      return L"private property";
   default:
      return L"unknown";
   }
}


/*************************************************************************************
CMIFLErrors::Add - Adds an error onto the end of the list

inputs
   PWSTR          pszDisplay - Error string to display
   PWSTR          pszLink - Link to use if clicked on
   PWSTR          pszObject - Object in the error
   BOOL           fError - TRUE if error, FALSE if warning
   DWORD          dwStartChar - Starting character, for where error occurs in code
   DWORD          dwEndChar - Ending character
*/
void CMIFLErrors::Add (PWSTR pszDisplay, PWSTR pszLink, PWSTR pszObject, BOOL fError,
                       DWORD dwStartChar, DWORD dwEndChar)
{
   // add items
   m_lDisplay.Add ((PWSTR)pszDisplay, (wcslen(pszDisplay)+1)*2);
   m_lLink.Add ((PWSTR)pszLink, (wcslen(pszLink)+1)*2);
   m_lObject.Add ((PWSTR)pszObject, (wcslen(pszObject)+1)*2);

   MIFLERR err;
   memset (&err, 0, sizeof(err));
   err.fError = fError;
   err.pszDisplay = (PWSTR)m_lDisplay.Get(m_lDisplay.Num()-1);
   err.pszLink = (PWSTR)m_lLink.Get(m_lLink.Num()-1);
   err.pszObject = (PWSTR)m_lObject.Get(m_lObject.Num()-1);
   err.dwStartChar = dwStartChar;
   err.dwEndChar = dwEndChar;

   m_lMIFLERR.Add (&err);

   if (fError)
      m_dwNumError++;
}


/*************************************************************************************
CMIFLErrors::Add - Adds an error that occurs in a specific object

inputs
   PWSTR          pszDisplay - Error string to display
   DWORD          dwLibOrigID - Original library ID
   PCMIFLObject   pObject - Object that this occurs in. Must be contained in dwLibOrigID
   BOOL           fError - TRUE if error, FALSE if warning
*/
void CMIFLErrors::Add (PWSTR pszDisplay, DWORD dwLibOrigID, PCMIFLObject pObject, BOOL fError)
{
   // make up the link
   WCHAR szTemp[64];
   swprintf (szTemp, L"lib:%dobject:%dedit", (int)dwLibOrigID, (int)pObject->m_dwTempID);

   Add (pszDisplay, szTemp, (PWSTR)pObject->m_memName.p, fError);
}





/*************************************************************************************
CMIFLErrors::Add - Adds an error that occurs in a specific object

inputs
   PWSTR          pszDisplay - Error string to display
   PCMIFLProj     pProj - Project
   BOOL           fError - TRUE if error, FALSE if warning
*/
void CMIFLErrors::Add (PWSTR pszDisplay, PCMIFLProj pProj, BOOL fError)
{
   Add (pszDisplay, L"proj:liblist", L"Project", fError);
}







/*****************************************************************************************
CMIFLCompiled::Contructor and destructor
*/
CMIFLCompiled::CMIFLCompiled (void)
{
   m_pLib = NULL;
   m_pProj = NULL;
   m_pErr = NULL;
   m_pProgress = NULL;
   m_hIdentifiers.Init (sizeof(MIFLIDENT));
   m_hGlobals.Init (sizeof(MIFLIDENT));
   m_hObjectIDs.Init (sizeof(MIFLIDENT));
   m_hUnIdentifiers.Init (sizeof(MIFLIDENT));
   m_hGlobalsDefault.Init (sizeof(CMIFLVarProp));

   Clear();
}

CMIFLCompiled::~CMIFLCompiled (void)
{
   Clear();
}


/*****************************************************************************************
CMIFLCompiled::Clear - Clears out everything stored in the compile object
*/
void CMIFLCompiled::Clear(void)
{
   if (m_pLib)
      delete m_pLib;
   m_pLib = NULL;

   m_pSocket = NULL;
   m_dwIDMethPropCur = 0;

   m_hIdentifiers.Clear();
   m_hObjectIDs.Clear();
   m_hGlobals.Clear();
   m_hUnIdentifiers.Clear();

   // loop through all the globals and free the data
   DWORD i;
   for (i = 0; i < m_hGlobalsDefault.Num(); i++) {
      PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobalsDefault.Get(i);
      pv->m_Var.SetUndefined();
   }
   m_hGlobalsDefault.Clear();
}




/*****************************************************************************************
CMIFLCompiled::Compile - Compiles the project.

inputs
   PCProgress           pProgress - Progress bar to use. Can be null
   PCMIFLProj           pProj - Project to compile from
   PCMIFLMeth           pMeth - If only want to compile a specific function, then this points to the
                        method definition. If compile all then this is NULL
   PCMIFLCode           pCode - If only want to compile a specific function, then this points to the
                        code definition. If compile all then this is NULL
   PCMIFLObject         pObject - If only want to compiled  aspecific function/method, then this is
                        the object it's in. If it's a global function/variable then pObject = NULL
   PWSTR                pszErrLink - If compiling a specific function, then this string is used for
                        links for the error. Not used if pMeth or pCode are NULL.
returns
   PCMIFLErrors - Returns an errors object. If any pErrors->m_dwNumError then the compile failed.
         Otherwise it succeeded. The errors object must be freed by the caller
*/
PCMIFLErrors CMIFLCompiled::Compile (PCProgress pProgress, PCMIFLProj pProj,
                                     PCMIFLMeth pMeth, PCMIFLCode pCode, PCMIFLObject pObject, PWSTR pszErrLink)
{
   Clear();

   PCMIFLErrors pErr = new CMIFLErrors;
   if (!pErr)
      return NULL;

   // pick a language ID
   m_pProj = pProj;
   m_pErr = pErr;

   // remember socket
   m_pSocket = pProj->m_pSocket;
   m_pProgress = pProgress;

   // merge
   // get all the libraries
   CListFixed lpLib;
   lpLib.Init (sizeof(PCMIFLLib));
   DWORD i;
   lpLib.Required (pProj->LibraryNum());
   for (i = 0; i < pProj->LibraryNum(); i++) {
      PCMIFLLib pLib = pProj->LibraryGet(i);
      lpLib.Add (&pLib);
   }

   // merge
   m_pLib = new CMIFLLib (m_pSocket);
   if (!m_pLib) {
      pErr->Add (L"Out of memory.", pProj, TRUE);
      return pErr;
   }
   PCMIFLLib *ppl = (PCMIFLLib*)lpLib.Get(0);
   if (!m_pLib->Merge (ppl, lpLib.Num(), FALSE, -1, pErr)) {   // BUGFIX - keep all languages
      pErr->Add (L"The libraries couldn't be merged together.", pProj, TRUE);
      Clear();
      return pErr;
   }
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (1.0 / TOTALSTEPS);

   // lower-case and kill dead
   m_pLib->KillDead (TRUE);


   // create tokens
   CompileAddTokens ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (2.0 / TOTALSTEPS);

   // method definitions
   CompileMethDef ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (3.0 / TOTALSTEPS);

   // property definitions
   CompilePropDef ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (4.0 / TOTALSTEPS);

   // globals
   CompileGlobal ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (5.0 / TOTALSTEPS);

   // strings
   CompileString ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (6.0 / TOTALSTEPS);

   // Resources
   CompileResource ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (7.0 / TOTALSTEPS);

   // func
   CompileFunc ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (8.0 / TOTALSTEPS);

   // objects, pass 1
   CompileObject ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (9.0 / TOTALSTEPS);

   // objects, pass 2
   CompileObject2 ();
   if (pErr->TooManyErrors())
      return pErr;

   if (m_pProgress)
      m_pProgress->Update (10.0 / TOTALSTEPS);

   if (pMeth && pCode && pszErrLink) {
      // BUGFIX - just double check that method is valid
      CompileMethCheckParam (pMeth, 3, NULL, pszErrLink);

      // because the objects used a different copies, need to find
      // the right copy
      if (pObject)
         pObject = m_pLib->ObjectGet(m_pLib->ObjectFind((PWSTR)pObject->m_memName.p, -1));

      CompileCode (pMeth, pCode, pObject, pszErrLink, 3, (PWSTR) pMeth->m_memName.p);
         // NOTE: passing in NULL for the code name because don't necessaily know if string is permenant
   }
   else {
      m_pProgress->Push (10.0 / TOTALSTEPS, TOTALSTEPS/TOTALSTEPS);

      // globals
      m_pProgress->Push (0, .1);
      CompileGlobalCode ();
      m_pProgress->Pop();
      if (pErr->TooManyErrors()) {
         m_pProgress->Pop();
         return pErr;
      }

      // func
      m_pProgress->Push (0.1, .2);
      CompileFuncCode ();
      m_pProgress->Pop();
      if (pErr->TooManyErrors()) {
         m_pProgress->Pop();
         return pErr;
      }

      // objects, pass 2
      m_pProgress->Push (0.2, 0.9);
      CompileObjectCode ();
      m_pProgress->Pop();
      if (pErr->TooManyErrors()) {
         m_pProgress->Pop();
         return pErr;
      }

      m_pProgress->Push (0.9, 1);
      VarInitQuick ();
      m_pProgress->Pop();
      if (pErr->TooManyErrors()) {
         m_pProgress->Pop();
         return pErr;
      }
      m_pProgress->Pop();
   }

   m_pProgress->Update (TOTALSTEPS / TOTALSTEPS);
   return pErr;
}



/*****************************************************************************************
CMIFLCompiled::CompileAddTokens - Adds all the tokens to the default m_hIdentifiers.
*/
void CMIFLCompiled::CompileAddTokens (void)
{
   // DOCUMENT: DOcument all these tokens

   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_TOKEN;

   mi.dwIndex = TOKEN_IF;
   m_hIdentifiers.Add (L"if", &mi, FALSE);

   mi.dwIndex = TOKEN_THEN;
   m_hIdentifiers.Add (L"then", &mi, FALSE);

   mi.dwIndex = TOKEN_ELSE;
   m_hIdentifiers.Add (L"else", &mi, FALSE);

   mi.dwIndex = TOKEN_FOR;
   m_hIdentifiers.Add (L"for", &mi, FALSE);

   mi.dwIndex = TOKEN_BREAK;
   m_hIdentifiers.Add (L"break", &mi, FALSE);

   mi.dwIndex = TOKEN_CONTINUE;
   m_hIdentifiers.Add (L"continue", &mi, FALSE);

   mi.dwIndex = TOKEN_CASE;
   m_hIdentifiers.Add (L"case", &mi, FALSE);

   mi.dwIndex = TOKEN_DEFAULT;
   m_hIdentifiers.Add (L"default", &mi, FALSE);

   mi.dwIndex = TOKEN_DO;
   m_hIdentifiers.Add (L"do", &mi, FALSE);

   mi.dwIndex = TOKEN_RETURN;
   m_hIdentifiers.Add (L"return", &mi, FALSE);

   mi.dwIndex = TOKEN_DEBUG;
   m_hIdentifiers.Add (L"debug", &mi, FALSE);
   // DOCUMENT: "debug;" token that cuases debugger to turn on (if in debug mode)

   mi.dwIndex = TOKEN_SWITCH;
   m_hIdentifiers.Add (L"switch", &mi, FALSE);

   mi.dwIndex = TOKEN_WHILE;
   m_hIdentifiers.Add (L"while", &mi, FALSE);

   mi.dwIndex = TOKEN_OPER_NEW;
   m_hIdentifiers.Add (L"new", &mi, FALSE);

   mi.dwIndex = TOKEN_DELETE;
   m_hIdentifiers.Add (L"delete", &mi, FALSE);

   mi.dwIndex = TOKEN_VAR;
   m_hIdentifiers.Add (L"var", &mi, FALSE);

   mi.dwIndex = TOKEN_NULL;
   m_hIdentifiers.Add (L"null", &mi, FALSE);

   mi.dwIndex = TOKEN_UNDEFINED;
   m_hIdentifiers.Add (L"undefined", &mi, FALSE);

   mi.dwIndex = TOKEN_TRUE;
   m_hIdentifiers.Add (L"true", &mi, FALSE);

   mi.dwIndex = TOKEN_FALSE;
   m_hIdentifiers.Add (L"false", &mi, FALSE);
}


/*****************************************************************************************
CMIFLCompiled::VerifyIdentifier - Verifies that an identifier is a valid string
and that it doesn't already exist. If either case happens then an error is added
and returns FALSE

inputs
   PWSTR          pszName - Name of identifier
   DWORD          dwLibOrigID - Original ID for the library
   DWORD          dwTempID - ID of the entity with the identifier
   PWSTR          pszLink - Portion of link to use to indicate it's the identifier, like "methdef:"
returns
   BOOL - TRUE if name OK, FALSE if bad name or exists
*/
BOOL CMIFLCompiled::VerifyIdenitifier (PWSTR pszName, DWORD dwLibOrigID, DWORD dwTempID, PWSTR pszLink)
{
   // check the name valid
   WCHAR szTemp[64];
   if (!MIFLIsNameValid (pszName)) {
      swprintf (szTemp, L"lib:%d%s%dedit", (int)dwLibOrigID, pszLink, (int)dwTempID);
      m_pErr->Add (
         L"The identifier's name can only contain letters, numbers, and an underscore. "
         L"The name cannot begin with a number.",
         szTemp, pszName, TRUE);
      return FALSE;
   }

   // look for dups
   PMIFLIDENT pmi = (PMIFLIDENT) m_hIdentifiers.Find (pszName, FALSE);
   if (pmi) {
      swprintf (szTemp, L"lib:%d%s%dedit", (int)dwLibOrigID, pszLink, (int)dwTempID);

      MemZero (&gMemTemp);
      MemCat (&gMemTemp, L"The identifier's name is already used by a ");
      MemCat (&gMemTemp, ErrTypeToString(pmi->dwType));
      MemCat (&gMemTemp, L".");
      m_pErr->Add ((PWSTR)gMemTemp.p, szTemp, pszName, TRUE);
      return FALSE;

   }

   // else good
   return TRUE;
}


/*****************************************************************************************
CMIFLCompiled::CompileMethCheckParam - Checks the parameters of a method to make sure
they're valid names.

inputs
   PCMIFLMeth        pMeth - Method
   DWORD             dwType - 0 for method definition, 1 for global function, 2 for private method, 3 for use pszErrLink
   PCMIFLObject      pObject - If this is somehow part of an object (private method) then
                        this will be valid
   PWSTR             pszErrLink - Used if detype == 3
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CMIFLCompiled::CompileMethCheckParam (PCMIFLMeth pMeth, DWORD dwType,
                                           PCMIFLObject pObject, PWSTR pszErrLink)
{
   DWORD i;
   PCMIFLProp *ppp = (PCMIFLProp*)pMeth->m_lPCMIFLProp.Get(0);
   BOOL fRet = TRUE;
   for (i = 0; i < pMeth->m_lPCMIFLProp.Num(); i++) {
      PCMIFLProp pp = ppp[i];

      if (!MIFLIsNameValid ((PWSTR)pp->m_memName.p)) {
         WCHAR szTemp[64];
         switch (dwType) {
         default:
         case 0:  // methdef
            swprintf (szTemp, L"lib:%dmethdef:%dedit", (int)pMeth->m_dwLibOrigFrom, (int)pMeth->m_dwTempID);
            break;
         case 1:  // global
            swprintf (szTemp, L"lib:%dfunc:%dedit", (int)pMeth->m_dwLibOrigFrom, (int)pMeth->m_dwTempID);
            break;
         case 2:  // private method
            swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit",
               (int)pMeth->m_dwLibOrigFrom,
               (int)ErrObjectToID (pMeth->m_dwLibOrigFrom, m_pProj, pObject),
               (int)pMeth->m_dwTempID);
            break;
         case 3:
            wcscpy (szTemp, pszErrLink);
            break;
         }

         MemZero (&gMemTemp);
         MemCat (&gMemTemp, L"The parameter, ");
         MemCat (&gMemTemp, (PWSTR)pp->m_memName.p);
         MemCat (&gMemTemp, L" is not a valid name. "
            L"The identifier's name can only contain letters, numbers, and an underscore. "
            L"The name cannot begin with a number.");

         m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pMeth->m_memName.p, TRUE);
         fRet = FALSE;
      }
   } // j

   return fRet;
}


/*****************************************************************************************
CMIFLCompiled::CompileMethDef - Compile method definitions
*/
void CMIFLCompiled::CompileMethDef (void)
{
   DWORD i;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_METHDEF;

   for (i = 0; i < m_pLib->MethDefNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLMeth pMeth = m_pLib->MethDefGet(i);

      // verify it's name
      PWSTR psz = (PWSTR)pMeth->m_memName.p;
      VerifyIdenitifier (psz, pMeth->m_dwLibOrigFrom, pMeth->m_dwTempID, L"methdef:");

      // add it
      mi.pEntity = pMeth;
      mi.dwIndex = i;
      mi.dwID = m_dwIDMethPropCur++;
      m_hIdentifiers.Add (psz, &mi, FALSE);
      m_hUnIdentifiers.Add (mi.dwID, &mi);

      // check its input parameters
      CompileMethCheckParam (pMeth, 0, NULL);
   } // i
}




/*****************************************************************************************
CMIFLCompiled::CompilePropDef - Compile property definitions
*/
void CMIFLCompiled::CompilePropDef (void)
{
   DWORD i;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_PROPDEF;

   for (i = 0; i < m_pLib->PropDefNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLProp pProp = m_pLib->PropDefGet(i);

      // verify it's name
      PWSTR psz = (PWSTR)pProp->m_memName.p;
      VerifyIdenitifier (psz, pProp->m_dwLibOrigFrom, pProp->m_dwTempID, L"propdef:");

      // BUGFIX - Remember that not from any property in specific
      pProp->m_pObjectFrom = NULL;

      // add it
      mi.pEntity = pProp;
      mi.dwIndex = i;
      mi.dwID = m_dwIDMethPropCur++;
      m_hIdentifiers.Add (psz, &mi, FALSE);
      m_hUnIdentifiers.Add (mi.dwID, &mi);
   } // i
}



/*****************************************************************************************
CMIFLCompiled::CompileGlobal - Compile globals
*/
void CMIFLCompiled::CompileGlobal (void)
{
   DWORD i;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_GLOBAL;

   for (i = 0; i < m_pLib->GlobalNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLProp pProp = m_pLib->GlobalGet(i);

      // verify it's name
      PWSTR psz = (PWSTR)pProp->m_memName.p;
      VerifyIdenitifier (psz, pProp->m_dwLibOrigFrom, pProp->m_dwTempID, L"global:");

      // BUGFIX - Remember that not from any property in specific
      pProp->m_pObjectFrom = NULL;

      // add it
      mi.pEntity = pProp;
      mi.dwIndex = i;
      mi.dwID = m_hGlobals.Num();   // link directly into global number
      m_hIdentifiers.Add (psz, &mi, FALSE);
      m_hGlobals.Add (mi.dwID, &mi);
      // NOTE: Specifically not using m_hUnIdentifiers
   } // i
}




/*****************************************************************************************
CMIFLCompiled::CompileGlobalCode - Compile globals
*/
void CMIFLCompiled::CompileGlobalCode (void)
{
   DWORD i;

   for (i = 0; i < m_pLib->GlobalNum(); i++) {
      if (!(i%10))
         m_pProgress->Update ((fp)i / (fp)m_pLib->GlobalNum());

      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLProp pProp = m_pLib->GlobalGet(i);

      // compile initialized to/from code
      if (pProp->m_pCodeGet) {
         WCHAR szTemp[64];
         swprintf (szTemp, L"lib:%dglobal:%dget", pProp->m_dwLibOrigFrom, pProp->m_dwTempID);

         CMIFLMeth Meth;
         Meth.InitAsGetSet ((PWSTR)pProp->m_memName.p, TRUE, FALSE);

         CompileCode (&Meth, pProp->m_pCodeGet, NULL, szTemp, 1, (PWSTR)pProp->m_memName.p);
      }
      if (pProp->m_pCodeSet) {
         WCHAR szTemp[64];
         swprintf (szTemp, L"lib:%dglobal:%dset", pProp->m_dwLibOrigFrom, pProp->m_dwTempID);

         CMIFLMeth Meth;
         Meth.InitAsGetSet ((PWSTR)pProp->m_memName.p, FALSE, FALSE);

         CompileCode (&Meth, pProp->m_pCodeSet, NULL, szTemp, 2, (PWSTR)pProp->m_memName.p);
      }
   } // i
}




/*****************************************************************************************
CMIFLCompiled::CompileString - Compile Stringss
*/
void CMIFLCompiled::CompileString (void)
{
   DWORD i;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_STRING;

   for (i = 0; i < m_pLib->StringNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLString pString = m_pLib->StringGet(i);

      // verify it's name
      PWSTR psz = (PWSTR)pString->m_memName.p;
      VerifyIdenitifier (psz, pString->m_dwLibOrigFrom, pString->m_dwTempID, L"string:");

      // add it
      mi.pEntity = pString;
      mi.dwIndex = i;
      mi.dwID = i;
      m_hIdentifiers.Add (psz, &mi, FALSE);
      // NOTE: Specifically not filling m_hUnIdentifiers
   } // i
}




/*****************************************************************************************
CMIFLCompiled::CompileResource - Compile Resourcess
*/
void CMIFLCompiled::CompileResource (void)
{
   DWORD i;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_RESOURCE;

   for (i = 0; i < m_pLib->ResourceNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLResource pResource = m_pLib->ResourceGet(i);

      // verify it's name
      PWSTR psz = (PWSTR)pResource->m_memName.p;
      VerifyIdenitifier (psz, pResource->m_dwLibOrigFrom, pResource->m_dwTempID, L"resource:");

      // while at it, set not to m_memType, set as name. This ensures that when
      // do "ToString()", will be OK
      DWORD j;
      PCMMLNode2 *ppn = (PCMMLNode2*)pResource->m_lPCMMLNode2.Get(0);
      for (j = 0; j < pResource->m_lPCMMLNode2.Num(); j++)
         ppn[j]->NameSet ((PWSTR)pResource->m_memType.p);

      // add it
      mi.pEntity = pResource;
      mi.dwIndex = i;
      mi.dwID = i;
      m_hIdentifiers.Add (psz, &mi, FALSE);
      // NOTE: Specifically not filling m_hUnIdentifiers
   } // i
}





/*****************************************************************************************
CMIFLCompiled::CompileFunc - Compile Funcs
*/
void CMIFLCompiled::CompileFunc (void)
{
   DWORD i;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_FUNC;

   for (i = 0; i < m_pLib->FuncNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLFunc pFunc = m_pLib->FuncGet(i);

      // verify it's name
      PWSTR psz = (PWSTR)pFunc->m_Meth.m_memName.p;
      VerifyIdenitifier (psz, pFunc->m_Meth.m_dwLibOrigFrom, pFunc->m_Meth.m_dwTempID, L"func:");

      // add it
      mi.pEntity = pFunc;
      mi.dwIndex = i;
      mi.dwID = i;
      m_hIdentifiers.Add (psz, &mi, FALSE);
      // NOTE: Specifically not filling m_hUnIdentifiers

      // check its input parameters
      CompileMethCheckParam (&pFunc->m_Meth, 1, NULL);
   } // i
}




/*****************************************************************************************
CMIFLCompiled::CompileFuncCode - Compile Funcs
*/
void CMIFLCompiled::CompileFuncCode (void)
{
   DWORD i;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_FUNC;

   for (i = 0; i < m_pLib->FuncNum(); i++) {
      if (!(i%10))
         m_pProgress->Update ((fp)i / (fp)m_pLib->FuncNum());

      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLFunc pFunc = m_pLib->FuncGet(i);

      WCHAR szTemp[64];
      swprintf (szTemp, L"lib:%dfunc:%dedit", pFunc->m_Meth.m_dwLibOrigFrom, pFunc->m_Meth.m_dwTempID);

      CompileCode (&pFunc->m_Meth, &pFunc->m_Code, NULL, szTemp, 0, (PWSTR)pFunc->m_Meth.m_memName.p);
   } // i
}




/*****************************************************************************************
CMIFLCompiled::CompileObject - Compile Objects
*/
void CMIFLCompiled::CompileObject (void)
{
   DWORD i, j;
   MIFLIDENT mi;
   memset (&mi, 0, sizeof(mi));
   mi.dwType = MIFLI_OBJECT;

   for (i = 0; i < m_pLib->ObjectNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLObject pObject = m_pLib->ObjectGet(i);

      // verify it's name
      PWSTR psz = (PWSTR)pObject->m_memName.p;
      VerifyIdenitifier (psz, pObject->m_dwLibOrigFrom, pObject->m_dwTempID, L"object:");

      // add it
      mi.pEntity = pObject;
      mi.dwIndex = i;
      mi.dwID = (pObject->m_fAutoCreate ? m_hGlobals.Num() : -1); // only global for objects that instantiated
      m_hIdentifiers.Add (psz, &mi, FALSE);
      // NOTE: Specifically not filling m_hUnIdentifiers

      // make sure object ID is unique and non-zero
      CMIFLVarProp vp;
      if (pObject->m_fAutoCreate) {
         // add to list of globals
         m_hGlobals.Add (mi.dwID, &mi);

         vp.m_dwID = mi.dwID;
         vp.m_pCodeGet = vp.m_pCodeSet = NULL;

         // set a global for this
         vp.m_Var.SetObject (&pObject->m_gID);
         m_hGlobalsDefault.Add (mi.dwID, &vp);

         if (IsEqualGUID (pObject->m_gID, GUID_NULL)) {
            m_pErr->Add (L"The object must have a non-zero ID.", pObject->m_dwLibOrigFrom, pObject, TRUE);
         }
         else {
            // see if already exists
            PMIFLIDENT pmi;
            if (pmi = (PMIFLIDENT) m_hObjectIDs.Find (&pObject->m_gID)) {
               MemZero (&gMemTemp);
               MemCat (&gMemTemp, L"The object, ");
               MemCat (&gMemTemp, (PWSTR)((PCMIFLObject)pmi->pEntity)->m_memName.p);
               MemCat (&gMemTemp, L", has the same ID as this one.");
   
               m_pErr->Add ((PWSTR)gMemTemp.p, pObject->m_dwLibOrigFrom, pObject, TRUE);
            }
            else
               m_hObjectIDs.Add (&pObject->m_gID, &mi);
         }
      } // if autocreate

      // make sure all public methods have valid names
      for (j = 0; j < pObject->MethPubNum(); j++) {
         PCMIFLFunc pFunc = pObject->MethPubGet(j);

         // find it
         PMIFLIDENT pmi = (PMIFLIDENT)m_hIdentifiers.Find ((PWSTR)pFunc->m_Meth.m_memName.p, FALSE);
         if (pmi && (pmi->dwType == MIFLI_METHDEF))
            continue;

         // else, error...
         WCHAR szTemp[64];
         swprintf (szTemp, L"lib:%dobject:%dedit",
            (int)pFunc->m_Meth.m_dwLibOrigFrom,
            (int)ErrObjectToID (pFunc->m_Meth.m_dwLibOrigFrom, m_pProj, pObject));

         MemZero (&gMemTemp);
         MemCat (&gMemTemp, L"The public method, ");
         MemCat (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);
         MemCat (&gMemTemp, L", is not defined in any of the libraries.");

         m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, TRUE);
      } // j

      // take a pass through all private methods
      for (j = 0; j < pObject->MethPrivNum(); j++) {
         PCMIFLFunc pFunc = pObject->MethPrivGet(j);

         // verify the name
         PWSTR psz = (PWSTR)pFunc->m_Meth.m_memName.p;

         if (!MIFLIsNameValid (psz)) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit",
               (int)pFunc->m_Meth.m_dwLibOrigFrom,
               (int)ErrObjectToID (pFunc->m_Meth.m_dwLibOrigFrom, m_pProj, pObject),
               (int)pFunc->m_Meth.m_dwTempID);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The private method, ");
            MemCat (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);
            MemCat (&gMemTemp, L", has illegal characters in its name. It can only contain "
               L"letters, numbers, and underscores.");

            m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, TRUE);
         }

         // find it
         PMIFLIDENT pmi = (PMIFLIDENT)m_hIdentifiers.Find ((PWSTR)pFunc->m_Meth.m_memName.p, FALSE);
         if (pmi) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit",
               (int)pFunc->m_Meth.m_dwLibOrigFrom,
               (int)ErrObjectToID (pFunc->m_Meth.m_dwLibOrigFrom, m_pProj, pObject),
               (int)pFunc->m_Meth.m_dwTempID);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The private method, ");
            MemCat (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);
            MemCat (&gMemTemp, L", has the same name as a ");
            MemCat (&gMemTemp, ErrTypeToString (pmi->dwType));
            MemCat (&gMemTemp, L".");

            m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, FALSE /*warning*/);
         }

         // check its input parameters
         CompileMethCheckParam (&pFunc->m_Meth, 2, pObject);
      } // j, private methods

      PCMIFLProp *ppp = (PCMIFLProp*)pObject->m_lPCMIFLPropPub.Get(0);
      for (j = 0; j < pObject->m_lPCMIFLPropPub.Num(); j++) {
         PCMIFLProp pp = ppp[j];

         // find it
         PMIFLIDENT pmi = (PMIFLIDENT)m_hIdentifiers.Find ((PWSTR)pp->m_memName.p, FALSE);
         if (pmi && (pmi->dwType == MIFLI_PROPDEF))
            continue;

         // else, error...
         WCHAR szTemp[64];
         swprintf (szTemp, L"lib:%dobject:%dedit",
            (int)pp->m_dwLibOrigFrom,
            (int)ErrObjectToID (pp->m_dwLibOrigFrom, m_pProj, pObject));

         MemZero (&gMemTemp);
         MemCat (&gMemTemp, L"The public property, ");
         MemCat (&gMemTemp, (PWSTR)pp->m_memName.p);
         MemCat (&gMemTemp, L", is not defined in any of the libraries.");

         m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, TRUE);
      } // j

      // while at it, test all the private properties
      ppp = (PCMIFLProp*)pObject->m_lPCMIFLPropPriv.Get(0);
      for (j = 0; j < pObject->m_lPCMIFLPropPriv.Num(); j++) {
         PCMIFLProp pp = ppp[j];

         // verify the name
         PWSTR psz = (PWSTR)pp->m_memName.p;

         if (!MIFLIsNameValid (psz)) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit",
               (int)pp->m_dwLibOrigFrom,
               (int)ErrObjectToID (pp->m_dwLibOrigFrom, m_pProj, pObject));

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The private property, ");
            MemCat (&gMemTemp, (PWSTR)pp->m_memName.p);
            MemCat (&gMemTemp, L", has illegal characters in its name. It can only contain "
               L"letters, numbers, and underscores.");

            m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, TRUE);
         }

         // find it
         PMIFLIDENT pmi = (PMIFLIDENT)m_hIdentifiers.Find ((PWSTR)pp->m_memName.p, FALSE);
         if (pmi) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit",
               (int)pp->m_dwLibOrigFrom,
               (int)ErrObjectToID (pp->m_dwLibOrigFrom, m_pProj, pObject));

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The private property, ");
            MemCat (&gMemTemp, (PWSTR)pp->m_memName.p);
            MemCat (&gMemTemp, L", has the same name as a ");
            MemCat (&gMemTemp, ErrTypeToString (pmi->dwType));
            MemCat (&gMemTemp, L".");

            m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, FALSE /*warning*/);
         }
      } // j
   } // i
}




/*****************************************************************************************
CMIFLCompiled::CompileObjectCode - Compile Objects code
*/
void CMIFLCompiled::CompileObjectCode (void)
{
   DWORD i, j;

   for (i = 0; i < m_pLib->ObjectNum(); i++) {
      m_pProgress->Update ((fp)i / (fp)m_pLib->ObjectNum());

      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLObject pObject = m_pLib->ObjectGet(i);

      // get its name
      PWSTR psz = (PWSTR)pObject->m_memName.p;

      // loop through all public and private methods
      DWORD dwPub;
      for (dwPub = 0; dwPub < 2; dwPub++) {
         DWORD dwNum = (dwPub ? pObject->MethPubNum() : pObject->MethPrivNum());

         for (j = 0; j < dwNum; j++) {
            // abort if too many errors
            if (m_pErr->TooManyErrors())
               return;

            PCMIFLFunc pFunc = dwPub ? pObject->MethPubGet(j) : pObject->MethPrivGet(j);
            PCMIFLMeth pMeth;
            // BUGFIX - If it's a public method get definition from main library
            if (dwPub)
               pMeth = m_pLib->MethDefGet(m_pLib->MethDefFind((PWSTR)pFunc->m_Meth.m_memName.p, -1));
            else
               pMeth = &pFunc->m_Meth;

            // error name
            WCHAR szTemp[64];
            swprintf (szTemp,
               dwPub ? L"lib:%dobject:%dmethpub:%dedit" : L"lib:%dobject:%dmethpriv:%dedit",
               (int)pFunc->m_Meth.m_dwLibOrigFrom,
               (int)ErrObjectToID (pFunc->m_Meth.m_dwLibOrigFrom, m_pProj, pObject),
               (int)pFunc->m_Meth.m_dwTempID);

            CompileCode (pMeth, &pFunc->m_Code, pObject, szTemp,
               3, (PWSTR)pFunc->m_Meth.m_memName.p);
         } // j
      } // dwPub

      // loop through all the public and private variables
      for (dwPub = 0; dwPub < 2; dwPub++) {
         PCListFixed pl = dwPub ? &pObject->m_lPCMIFLPropPub : &pObject->m_lPCMIFLPropPriv;
         DWORD dwNum = pl->Num();
         WCHAR szTemp[64];

         PCMIFLProp *ppp = (PCMIFLProp*)pl->Get(0);

         for (j = 0; j < pl->Num(); j++) {
            // abort if too many errors
            if (m_pErr->TooManyErrors())
               return;

            PCMIFLProp pProp = ppp[j];

            // compile initialized to/from code
            if (pProp->m_pCodeGet) {
               swprintf (szTemp,
                  dwPub ? L"lib:%dobject:%dproppub:%dget" : L"lib:%dobject:%dproppriv:%dget",
                  (int)pProp->m_dwLibOrigFrom,
                  (int)ErrObjectToID (pProp->m_dwLibOrigFrom, m_pProj, pObject),
                  (int)pProp->m_dwTempID);

               CMIFLMeth Meth;
               Meth.InitAsGetSet ((PWSTR)pProp->m_memName.p, TRUE, FALSE);

               CompileCode (&Meth, pProp->m_pCodeGet, pObject, szTemp,
                  4, (PWSTR)pProp->m_memName.p);
            }
            if (pProp->m_pCodeSet) {
               swprintf (szTemp,
                  dwPub ? L"lib:%dobject:%dproppub:%dset" : L"lib:%dobject:%dproppriv:%dset",
                  (int)pProp->m_dwLibOrigFrom,
                  (int)ErrObjectToID (pProp->m_dwLibOrigFrom, m_pProj, pObject),
                  (int)pProp->m_dwTempID);

               CMIFLMeth Meth;
               Meth.InitAsGetSet ((PWSTR)pProp->m_memName.p, FALSE, FALSE);

               CompileCode (&Meth, pProp->m_pCodeSet, pObject, szTemp,
                  5, (PWSTR)pProp->m_memName.p);
            }
         } // j
      } // dwPub
   } // i
}

/*****************************************************************************************
CMIFLCompiled::CompileObject2 - Compile Objects, pass 2
*/
void CMIFLCompiled::CompileObject2 (void)
{
   DWORD i, j, dw;

   for (i = 0; i < m_pLib->ObjectNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLObject pObject = m_pLib->ObjectGet(i);

      // clear out
      DWORD dwSize = (pObject->m_lPCMIFLPropPriv.Num() + pObject->MethPrivNum())*3;
      pObject->m_hPrivIdentity.Init (sizeof(MIFLIDENT), dwSize);
      pObject->m_hUnPrivIdentity.Init (sizeof(MIFLIDENT), dwSize);
      pObject->m_lContainsDefault.Init (sizeof(GUID));
      pObject->m_hMethJustThis.Init (sizeof(MIFLIDENT), dwSize);
      pObject->m_hMethAllClass.Init (sizeof(MIFLIDENT), dwSize);

      // expand the list of its dependencies...
      pObject->m_lClassSuperAll.Clear();
      BOOL fExpand = m_pLib->ObjectAllSuper ((PWSTR)pObject->m_memName.p, &pObject->m_lClassSuperAll);
      if (!fExpand) {
         m_pErr->Add (L"The super-classes of the object have a circular reference to the object.",
            pObject->m_dwLibOrigFrom, pObject, TRUE);
      }

      // create a list of of pointers to super-classes, immediate and all
      for (dw = 0; dw < 2; dw++) {
         PCListVariable plFrom = dw ? &pObject->m_lClassSuperAll : &pObject->m_lClassSuper;
         PCListFixed plTo = dw ? &pObject->m_lClassSuperAllPCMIFLObject : &pObject->m_lClassSuperPCMIFLObject;

         plTo->Init (sizeof(PCMIFLObject));

         for (j = 0; j < plFrom->Num(); j++) {
            PMIFLIDENT pmi = (PMIFLIDENT) m_hIdentifiers.Find ((PWSTR)plFrom->Get(j), FALSE);
            if (!pmi || pmi->dwType != MIFLI_OBJECT) {
               MemZero (&gMemTemp);
               MemCat (&gMemTemp, dw ? L"The indirect super-class, " : L"The super-class, ");
               MemCat (&gMemTemp, (PWSTR)plFrom->Get(j));
               MemCat (&gMemTemp, L", does not exist.");
               m_pErr->Add ((PWSTR)gMemTemp.p,
                  pObject->m_dwLibOrigFrom, pObject, TRUE);
               continue;
            }

            PCMIFLObject pSuper = (PCMIFLObject)pmi->pEntity;
            plTo->Add (&pSuper);
         } // j
      } // dw

      // verify that contained in exists and is valid
      pObject->m_pContainedIn = NULL;
      PWSTR psz;
      psz = (PWSTR)pObject->m_memContained.p;
      if (pObject->m_fAutoCreate && psz[0]) {
         PMIFLIDENT pmi = (PMIFLIDENT) m_hIdentifiers.Find (psz, FALSE);
         PCMIFLObject pIn = pmi ? (PCMIFLObject)pmi->pEntity : NULL;
         if (!pmi || (pmi->dwType != MIFLI_OBJECT))
            m_pErr->Add (L"The object that this is created in does not exist.",
               pObject->m_dwLibOrigFrom, pObject, TRUE);
         else if (!pIn->m_fAutoCreate)
            m_pErr->Add (L"The object that this is created in is not automatically created itself.",
               pObject->m_dwLibOrigFrom, pObject, TRUE);
         else
            pObject->m_pContainedIn = pIn;
      } // autocreate



      // take a pass through all private methods
      for (j = 0; j < pObject->MethPrivNum(); j++) {
         PCMIFLFunc pFunc = pObject->MethPrivGet(j);

         // see if it already exists
         PWSTR psz = (PWSTR)pFunc->m_Meth.m_memName.p;

         PMIFLIDENT pmi = (PMIFLIDENT) pObject->m_hPrivIdentity.Find (psz, FALSE);
         if (pmi) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dmethpriv:%dedit",
               (int)pFunc->m_Meth.m_dwLibOrigFrom,
               (int)ErrObjectToID (pFunc->m_Meth.m_dwLibOrigFrom, m_pProj, pObject),
               (int)pFunc->m_Meth.m_dwTempID);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The private method, ");
            MemCat (&gMemTemp, (PWSTR)pFunc->m_Meth.m_memName.p);
            MemCat (&gMemTemp, L", has the same name as another private method or property.");

            m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, TRUE);
            continue;
         }

         // BUGFIX - Since private methods cannot be overridden, ensure that
         // flag is set properly
         pFunc->m_Meth.m_dwOverride = 0;

         // BUGFIX - Keep track of the object that adde this so that can go
         // back and save a private method variable to MML
         pFunc->m_pObjectPrivate = pObject;

         // add it
         MIFLIDENT mi;
         memset (&mi, 0, sizeof(mi));
         mi.dwIndex = j;
         mi.dwType = MIFLI_METHPRIV;
         mi.pEntity = pFunc;
         mi.dwID = m_dwIDMethPropCur++;
         pObject->m_hPrivIdentity.Add (psz, &mi, FALSE);
         pObject->m_hUnPrivIdentity.Add (mi.dwID, &mi);
         m_hUnIdentifiers.Add (mi.dwID, &mi);   // so can go to private names

         pObject->m_hMethJustThis.Add (mi.dwID, &mi);  // so know what private methods supported
         pObject->m_hMethAllClass.Add (mi.dwID, &mi);
      } // j, private methods

      // go through all public methods and remember that support...
      for (j = 0; j < pObject->MethPubNum(); j++) {
         PCMIFLFunc pFunc = pObject->MethPubGet (j);

         // see if it already exists
         PWSTR psz = (PWSTR)pFunc->m_Meth.m_memName.p;

         PMIFLIDENT pmi = (PMIFLIDENT) m_hIdentifiers.Find (psz, FALSE);
         if (!pmi)
            continue;   // shouldnt happen

         // BUGFIX - Since public methods can be overridden, ensure that
         // flag is set to the standard method deginition
         pFunc->m_Meth.m_dwOverride = ((PCMIFLMeth)pmi->pEntity)->m_dwOverride;


         // BUGFIX - need to rework the information so is pointing directly
         // to the function in the object
         MIFLIDENT mi;
         mi = *pmi;
         mi.pEntity = pFunc;

         pObject->m_hMethJustThis.Add (mi.dwID, &mi);  // so know what public methods supported
         pObject->m_hMethAllClass.Add (mi.dwID, &mi);
      }

      // while at it, test all the private properties
      PCMIFLProp *ppp = (PCMIFLProp*)pObject->m_lPCMIFLPropPriv.Get(0);
      for (j = 0; j < pObject->m_lPCMIFLPropPriv.Num(); j++) {
         PCMIFLProp pp = ppp[j];

         // see if it already exists
         PWSTR psz = (PWSTR)pp->m_memName.p;
         PMIFLIDENT pmi = (PMIFLIDENT) pObject->m_hPrivIdentity.Find (psz, FALSE);
         if (pmi) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dobject:%dedit",
               (int)pp->m_dwLibOrigFrom,
               (int)ErrObjectToID (pp->m_dwLibOrigFrom, m_pProj, pObject));

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"The private property, ");
            MemCat (&gMemTemp, (PWSTR)pp->m_memName.p);
            MemCat (&gMemTemp, L", has the same name as another private method or property.");

            m_pErr->Add ( (PWSTR)gMemTemp.p, szTemp, (PWSTR)pObject->m_memName.p, TRUE);
            continue;
         }

         // BUGFIX - Remember that is from this object
         pp->m_pObjectFrom = pObject;

         // add it
         MIFLIDENT mi;
         memset (&mi, 0, sizeof(mi));
         mi.dwIndex = j;
         mi.dwType = MIFLI_PROPPRIV;
         mi.pEntity = pp;
         mi.dwID = m_dwIDMethPropCur++;
         pObject->m_hPrivIdentity.Add (psz, &mi, FALSE);
         pObject->m_hUnPrivIdentity.Add (mi.dwID, &mi);
         m_hUnIdentifiers.Add (mi.dwID, &mi);   // so can go to private names
      } // j

   } // i


   // go through all the objects again, this time looking at superclasses and
   // deriving values from there...
   DWORD k;
   for (i = 0; i < m_pLib->ObjectNum(); i++) {
      PCMIFLObject pObject = m_pLib->ObjectGet(i);

      PCMIFLObject *pps = (PCMIFLObject*)pObject->m_lClassSuperAllPCMIFLObject.Get(0);
      for (j = 0; j < pObject->m_lClassSuperAllPCMIFLObject.Num(); j++) {
         PCMIFLObject pSuper = pps[j];

         for (k = 0; k < pSuper->m_hMethJustThis.Num(); k++) {
            PMIFLIDENT pmiSuper = (PMIFLIDENT)pSuper->m_hMethJustThis.Get(k);

            if (-1 != pObject->m_hMethAllClass.FindIndex (pmiSuper->dwID))
               continue;   // already have in based on higher class
               // BUGFIX - Was -1 == pObject... which was wrong

            // else, add it
            pObject->m_hMethAllClass.Add (pmiSuper->dwID, pmiSuper);
         } // k
      } // j
   } // i


}



#ifdef _DEBUG
/*****************************************************************************************
TokenDebug - This function displays all the tokens on the output.

inputs
   PMIFLTOKEN        pToken - Token to display
   DWORD             dwIndent - Amount to indent
   BOOL              fIgnoreDown - Ignore checking down slot
returns
   none
*/
static void TokenDebug (PMIFLTOKEN pToken, DWORD dwIndent = 0, BOOL fIgnoreDown = FALSE)
{
   // print the right number of spaces...
   DWORD i;
   for (i = 0; i < dwIndent; i++)
      OutputDebugString (" ");

   // output token
   char szTemp[64];
   switch (pToken->dwType) {
   case TOKEN_DOUBLE:
      sprintf (szTemp, "%f", pToken->fValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_CHAR:
      sprintf (szTemp, "'%c'", (char)pToken->cValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_STRING:
      OutputDebugString ("\"");
      OutputDebugStringW (pToken->pszString);
      OutputDebugString ("\"");
      break;
   case TOKEN_IDENTIFIER:
      OutputDebugString (":");
      OutputDebugStringW (pToken->pszString);
      break;

   // identifier tokens
   case TOKEN_IF:
      OutputDebugString ("if");
      break;
   case TOKEN_THEN:
      OutputDebugString ("then");
      break;
   case TOKEN_ELSE:
      OutputDebugString ("else");
      break;
   case TOKEN_FOR:
      OutputDebugString ("for");
      break;
   case TOKEN_BREAK:
      OutputDebugString ("break");
      break;
   case TOKEN_CONTINUE:
      OutputDebugString ("continue");
      break;
   case TOKEN_NOP:
      OutputDebugString ("NOP");
      break;
   case TOKEN_CASE:
      OutputDebugString ("case");
      break;
   case TOKEN_DEFAULT:
      OutputDebugString ("default");
      break;
   case TOKEN_DO:
      OutputDebugString ("do");
      break;
   case TOKEN_RETURN:
      OutputDebugString ("return");
      break;
   case TOKEN_DEBUG:
      OutputDebugString ("debug");
      break;
   case TOKEN_SWITCH:
      OutputDebugString ("switch");
      break;
   case TOKEN_WHILE:
      OutputDebugString ("while");
      break;

   case TOKEN_OPER_NEW:
      OutputDebugString ("new");
      break;
   case TOKEN_DELETE:
      OutputDebugString ("delete");
      break;
   case TOKEN_VAR:
      OutputDebugString ("var");
      break;

   case TOKEN_NULL:
      OutputDebugString ("null");
      break;
   case TOKEN_UNDEFINED:
      OutputDebugString ("undefined");
      break;
   case TOKEN_BOOL:      // boolean. dwValue is 0 or 1
      sprintf (szTemp, "bool:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_STRINGTABLE:      // dwValue is reference to string table
      sprintf (szTemp, "stringtable:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_RESOURCE:      // dwValue is reference to resource
      sprintf (szTemp, "resource:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_METHPUB:      // calling a private or public method. dwValue = method ID
      sprintf (szTemp, "methpub:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_METHPRIV:      // calling a private or public method. dwValue = method ID
      sprintf (szTemp, "methpriv:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_PROPPUB:      // accessing a private or public property. dwValue = prop ID
      sprintf (szTemp, "proppub:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_PROPPRIV:      // accessing a private or public property. dwValue = prop ID
      sprintf (szTemp, "proppriv:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_FUNCTION:      // accessing a global function. dwValue = function ID
      sprintf (szTemp, "function:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_GLOBAL:      // accessing a global variable. dwValue = global ID
      sprintf (szTemp, "global:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_VARIABLE:      // accessing a global variable. dwValue = global ID
      sprintf (szTemp, "variable:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_CLASS:      // accessing a class. dwValue = class ID. Used only after new token
      sprintf (szTemp, "class:%d", (int)pToken->dwValue);
      OutputDebugString (szTemp);
      break;
   case TOKEN_TRUE:
      OutputDebugString ("true");
      break;
   case TOKEN_FALSE:
      OutputDebugString ("false");
      break;

   // operators
   case TOKEN_OPER_DOT:
      OutputDebugString (".");
      break;
   case TOKEN_OPER_BRACKETL:
      OutputDebugString ("[");
      break;
   case TOKEN_OPER_BRACKETR:
      OutputDebugString ("]");
      break;
   case TOKEN_OPER_BRACEL:
      OutputDebugString ("{");
      break;
   case TOKEN_OPER_BRACER:
      OutputDebugString ("}");
      break;
   case TOKEN_OPER_PARENL:
      OutputDebugString ("(");
      break;
   case TOKEN_OPER_PARENR:
      OutputDebugString (")");
      break;
   case TOKEN_OPER_NUMSIGN:
      OutputDebugString ("#");
      break;

   case TOKEN_OPER_PLUSPLUS:
      OutputDebugString ("++");
      break;
   case TOKEN_OPER_MINUSMINUS:
      OutputDebugString ("--");
      break;
   case TOKEN_OPER_NEGATION:
      OutputDebugString ("-");
      break;
   case TOKEN_OPER_BITWISENOT:
      OutputDebugString ("~");
      break;
   case TOKEN_OPER_LOGICALNOT:
      OutputDebugString ("!");
      break;

   case TOKEN_OPER_MULTIPLY:
      OutputDebugString ("*");
      break;
   case TOKEN_OPER_DIVIDE:
      OutputDebugString ("/");
      break;
   case TOKEN_OPER_MODULO:
      OutputDebugString ("%");
      break;
   case TOKEN_OPER_ADD:
      OutputDebugString ("+");
      break;
   case TOKEN_OPER_SUBTRACT:
      OutputDebugString ("-");
      break;

   case TOKEN_OPER_BITWISELEFT:
      OutputDebugString ("<<");
      break;
   case TOKEN_OPER_BITWISERIGHT:
      OutputDebugString (">>");
      break;
   case TOKEN_OPER_LESSTHAN:
      OutputDebugString ("<");
      break;
   case TOKEN_OPER_LESSTHANEQUAL:
      OutputDebugString ("<=");
      break;
   case TOKEN_OPER_GREATERTHAN:
      OutputDebugString (">");
      break;
   case TOKEN_OPER_GREATERTHANEQUAL:
      OutputDebugString (">=");
      break;
   case TOKEN_OPER_EQUALITY:
      OutputDebugString ("==");
      break;
   case TOKEN_OPER_NOTEQUAL:
      OutputDebugString ("!=");
      break;
   case TOKEN_OPER_EQUALITYSTRICT:
      OutputDebugString ("===");
      break;
   case TOKEN_OPER_NOTEQUALSTRICT:
      OutputDebugString ("!==");
      break;

   case TOKEN_OPER_BITWISEAND:
      OutputDebugString ("&");
      break;
   case TOKEN_OPER_BITWISEXOR:
      OutputDebugString ("^");
      break;
   case TOKEN_OPER_BITWISEOR:
      OutputDebugString ("|");
      break;
   case TOKEN_OPER_LOGICALAND:
      OutputDebugString ("&&");
      break;
   case TOKEN_OPER_LOGICALOR:
      OutputDebugString ("||");
      break;

   case TOKEN_OPER_CONDITIONAL:
      OutputDebugString ("?");
      break;

   case TOKEN_OPER_ASSIGN:
      OutputDebugString ("=");
      break;
   case TOKEN_OPER_ASSIGNADD:
      OutputDebugString ("+=");
      break;
   case TOKEN_OPER_ASSIGNSUBTRACT:
      OutputDebugString ("-=");
      break;
   case TOKEN_OPER_ASSIGNMULTIPLY:
      OutputDebugString ("*=");
      break;
   case TOKEN_OPER_ASSIGNDIVIDE:
      OutputDebugString ("/=");
      break;
   case TOKEN_OPER_ASSIGNMODULO:
      OutputDebugString ("%=");
      break;
   case TOKEN_OPER_ASSIGNBITWISELEFT:
      OutputDebugString ("<<=");
      break;
   case TOKEN_OPER_ASSIGNBITWISERIGHT:
      OutputDebugString (">>=");
      break;
   case TOKEN_OPER_ASSIGNBITWISEAND:
      OutputDebugString ("&=");
      break;
   case TOKEN_OPER_ASSIGNBITWISEXOR:
      OutputDebugString ("^=");
      break;
   case TOKEN_OPER_ASSIGNBITWISEOR:
      OutputDebugString ("|=");
      break;

   case TOKEN_OPER_COLON:
      OutputDebugString (":");
      break;
   case TOKEN_OPER_SEMICOLON:
      OutputDebugString (";");
      break;
   case TOKEN_OPER_COMMA:
      OutputDebugString (",");
      break;

   case TOKEN_ARG1:
      OutputDebugString ("Arg1:");
      break;
   case TOKEN_ARG2:
      OutputDebugString ("Arg2:");
      break;
   case TOKEN_ARG3:
      OutputDebugString ("Arg3:");
      break;
   case TOKEN_ARG4:
      OutputDebugString ("Arg4:");
      break;
   case TOKEN_LISTDEF:
      OutputDebugString ("list definition");
      break;
   case TOKEN_LISTINDEX:
      OutputDebugString ("list index");
      break;
   case TOKEN_FUNCCALL:
      OutputDebugString ("function call");
      break;

   default:
      OutputDebugString ("UnkToken");
      break;
   }

   // print out down
   PMIFLTOKEN pDown;
   if (!fIgnoreDown) for (pDown = (PMIFLTOKEN) pToken->pDown; pDown; pDown = (PMIFLTOKEN) pDown->pDown) {
      OutputDebugString ("\r\n");
      TokenDebug (pDown, dwIndent + 3, TRUE);
   }

   // print out one to the right
   OutputDebugString ("\r\n");
   if (pToken->pNext)
      TokenDebug ((PMIFLTOKEN) pToken->pNext, dwIndent);
}
#endif // _DEBUG


/*****************************************************************************************
TokenFindRange - This fills in the character range for the token and its contents.

inputs
   PMIFLTOKEN        pToken - Token to free
   DWORD             *pdwCharStart - Filled with the start character
   DWORD             *pdwCharEnd - End character
returns
   none
*/
static void TokenFindRange (PMIFLTOKEN pToken, DWORD *pdwCharStart, DWORD *pdwCharEnd)
{
   PMIFLTOKEN pCur, pDown;
   for (pCur = pToken; pCur; pCur = (PMIFLTOKEN)pCur->pNext) {
      if (pCur == pToken) {
         // By doing this first, can have pdwCharStart and pdwCharEnd pointed into pToken
         *pdwCharStart = pCur->dwCharStart;
         *pdwCharEnd = pCur->dwCharEnd;
      }
      else {
         *pdwCharStart = min(*pdwCharStart, pCur->dwCharStart);
         *pdwCharEnd = max(*pdwCharEnd, pCur->dwCharEnd);
      }

      // look down
      // BUGFIX - Was a loop, but would end up looping overitself a very large
      // amount of times because of recusion. Thefore, just let recursion do the
      // trick
      // for (pDown = (PMIFLTOKEN)pCur->pDown; pDown; pDown = (PMIFLTOKEN)pDown->pDown) {
      if (pDown = (PMIFLTOKEN)pCur->pDown) {
         DWORD dwStart, dwEnd;
         TokenFindRange (pDown, &dwStart, &dwEnd);
         *pdwCharStart = min(*pdwCharStart, dwStart);
         *pdwCharEnd = max(*pdwCharEnd, dwEnd);
      }
   }

}

/*****************************************************************************************
TokenFree - This function walks through the token linked list (returned from Tokenize())
and frees it all.

inputs
   PMIFLTOKEN        pToken - Token to free
returns
   none
*/
static void TokenFree (PMIFLTOKEN pToken)
{
   if (!pToken)
      return;  // nothing to free

   if (pToken->pszString)
      ESCFREE (pToken->pszString);
   if (pToken->pDown)
      TokenFree ((PMIFLTOKEN) pToken->pDown);
   if (pToken->pNext)
      TokenFree ((PMIFLTOKEN) pToken->pNext);

   ESCFREE (pToken);
}


/*****************************************************************************************
ParseNatural - Parses a natural number.

inputs
   PWSTR          psz - inputs.
   __int64        *piValue - Filled with the value of the number
returns
   DWORD - NUmber of characters used. 0 if doesnt start with number
*/
static DWORD ParseNatural (PWSTR psz, __int64 *piValue)
{
   DWORD dwCount = 0;

   *piValue = 0;

   while (psz[0]) {
      if ((psz[0] < L'0') || (psz[0] > L'9'))
         break;

      piValue[0] = piValue[0] * 10 + (__int64)(psz[0] - L'0');
      dwCount++;
      psz++;
   }

   return dwCount;
}


/*****************************************************************************************
ParseInteger - Parses an integer (which can be negative)

inputs
   PWSTR          psz - inputs.
   __int64        *piValue - Filled with the value of the number
returns
   DWORD - NUmber of characters used. 0 if doesnt start with number
*/
static DWORD ParseInteger (PWSTR psz, __int64 *piValue)
{
   DWORD dwCount;
   if (psz[0] == L'-') {
      dwCount = ParseNatural (psz+1, piValue);
      if (!dwCount)
         return 0;   // error, only a minus sign
      piValue[0] *= -1; // flip sign
      return dwCount+1;
   }

   return ParseNatural (psz, piValue);
}


/*****************************************************************************************
ParseDoubleNoE - Parses out a double value.

NOTE: This doesn't allow for "343e34"

inputs
   PWSTR          psz - inputs.
   BOOL           fNegative - If TRUE then allows a negative number at the start
   double         *pfValue - Filled with the value of the number
returns
   DWORD - NUmber of characters used. 0 if doesnt start with number
*/
static DWORD ParseDoubleNoE (PWSTR psz, BOOL fNegative, double *pfValue)
{
   DWORD dwCount;
   __int64 iVal;
   if (fNegative)
      dwCount = ParseInteger (psz, &iVal);
   else
      dwCount = ParseNatural (psz, &iVal);

   // see if there's a period
   psz += dwCount;
   BOOL fPeriod = (psz[0] == L'.');

   if (fPeriod && ((psz[1] < L'0') || (psz[1] > L'9')))
      fPeriod = FALSE;  // since not followed by digit dont treat as period

   if (!fPeriod) {
      if (!dwCount)
         return 0;   // not a number

      pfValue[0] = (double)iVal;
      return dwCount;
   }

   // else had period. If no count then zero out value
   if (!dwCount)
      // else, is a fp starting with .5465
      iVal = 0;


   // else has a period
   // skip period
   psz++;
   dwCount++;

   // look for fractional amount, which is guaranteed to be there because
   // have checked that number after priod
   DWORD dwFrac;
   __int64 iFrac;
   dwFrac = ParseNatural (psz, &iFrac);

   pfValue[0] = (double)iVal + (double)iFrac / pow ((double)10.0, (double)dwFrac);
   return dwCount + dwFrac;
}




/*****************************************************************************************
ParseDouble - Parses out a double value.

NOTE: This allows for "343e34"

inputs
   PWSTR          psz - inputs.
   BOOL           fNegative - If TRUE then allows a negative number at the start
   double         *pfValue - Filled with the value of the number
returns
   DWORD - NUmber of characters used. 0 if doesnt start with number
*/
static DWORD ParseDouble (PWSTR psz, BOOL fNegative, double *pfValue)
{
   DWORD dwCount = ParseDoubleNoE (psz, fNegative, pfValue);
   if (!dwCount)
      return 0;

   // see if have e followed by number
   psz += dwCount;
   if ((psz[0] == L'e') || (psz[0] == L'E')) {
      __int64 iExp;
      DWORD dwPow = ParseInteger (psz+1, &iExp);
      if (dwPow) {
         dwCount += dwPow+1;
         pfValue[0] *= pow (10, (double)iExp);
      }
   }

   return dwCount;
}



/*****************************************************************************************
ParseHexNoPrefix - Parses a hexadecimal number, assuming no prefixes.

inputs
   PWSTR          psz - inputs.
   __int64        *piValue - Filled with the value of the number
returns
   DWORD - NUmber of characters used. 0 if doesnt start with number
*/
static DWORD ParseHexNoPrefix (PWSTR psz, __int64 *piValue)
{
   DWORD dwCount = 0;

   piValue[0] = 0;

   while (psz[0]) {
      if ((psz[0] >= L'0') && (psz[0] <= L'9')) {
         piValue[0] = piValue[0] * 16 + (__int64)(psz[0] - L'0');
         dwCount++;
         psz++;
         continue;
      }
      if ((psz[0] >= L'a') && (psz[0] <= L'f')) {
         piValue[0] = piValue[0] * 16 + (__int64)(psz[0] - L'a' + 10);
         dwCount++;
         psz++;
         continue;
      }
      if ((psz[0] >= L'A') && (psz[0] <= L'F')) {
         piValue[0] = piValue[0] * 16 + (__int64)(psz[0] - L'A' + 10);
         dwCount++;
         psz++;
         continue;
      }


      break;   // end
   }

   return dwCount;
}



/*****************************************************************************************
ParseHexPrefix - Parses a hexadecimal number, assuming a prefix of "0x"

inputs
   PWSTR          psz - inputs.
   __int64        *piValue - Filled with the value of the number
returns
   DWORD - NUmber of characters used. 0 if doesnt start with number
*/
static DWORD ParseHexPrefix (PWSTR psz, __int64 *piValue)
{
   if ((psz[0] != L'0') || ((psz[1] != L'x') && (psz[1] != L'X')))
      return 0;   // no prefix

   DWORD dwCount = ParseHexNoPrefix (psz+2, piValue);
   if (!dwCount)
      return 0;

   return dwCount+2; // account for prefix
}

/*****************************************************************************************
ParseOperator - Pulls an operator out of the token stream.

inputs
   PWSTR       psz - String to look at
   DWORD       *pdwOper - Filled with the operator
returns
   DWORD - Characters used, or 0 if none
*/
static DWORD ParseOperator (PWSTR psz, DWORD *pdwOper)
{
   switch (psz[0]) {
   case L'.':
      *pdwOper = TOKEN_OPER_DOT;
      return 1;

   case L'[':
      *pdwOper = TOKEN_OPER_BRACKETL;
      return 1;
   case L']':
      *pdwOper = TOKEN_OPER_BRACKETR;
      return 1;
   case L'{':
      *pdwOper = TOKEN_OPER_BRACEL;
      return 1;
   case L'}':
      *pdwOper = TOKEN_OPER_BRACER;
      return 1;
   case L'(':
      *pdwOper = TOKEN_OPER_PARENL;
      return 1;
   case L')':
      *pdwOper = TOKEN_OPER_PARENR;
      return 1;
   case L'#':
      *pdwOper = TOKEN_OPER_NUMSIGN;
      return 1;

   case L'+':
      switch (psz[1]) {
      case L'+':
         *pdwOper = TOKEN_OPER_PLUSPLUS;
         return 2;
      case L'=':
         *pdwOper = TOKEN_OPER_ASSIGNADD;
         return 2;
      }
      *pdwOper = TOKEN_OPER_ADD;
      return 1;

   case L'-':
      switch (psz[1]) {
      case L'-':
         *pdwOper = TOKEN_OPER_MINUSMINUS;
         return 2;
      case L'=':
         *pdwOper = TOKEN_OPER_ASSIGNSUBTRACT;
         return 2;
      }
      *pdwOper = TOKEN_OPER_NEGATION;
      return 1;

   case L'~':
      *pdwOper = TOKEN_OPER_BITWISENOT;
      return 1;

   case L'!':
      if (psz[1] == L'=') {
         if (psz[2] == L'=') {
            *pdwOper = TOKEN_OPER_NOTEQUALSTRICT;
            return 3;
         }
         *pdwOper = TOKEN_OPER_NOTEQUAL;
         return 2;
      }
      *pdwOper = TOKEN_OPER_LOGICALNOT;
      return 1;

   case L'*':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_ASSIGNMULTIPLY;
         return 2;
      }
      *pdwOper = TOKEN_OPER_MULTIPLY;
      return 1;

   case L'/':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_ASSIGNDIVIDE;
         return 2;
      }
      *pdwOper = TOKEN_OPER_DIVIDE;
      return 1;

   case L'%':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_ASSIGNMODULO;
         return 2;
      }
      *pdwOper = TOKEN_OPER_MODULO;
      return 1;

   case L'<':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_LESSTHANEQUAL;
         return 2;
      }
      else if (psz[1] == L'<') {
         if (psz[2] == L'=') {
            *pdwOper = TOKEN_OPER_ASSIGNBITWISELEFT;
            return 3;
         }
         *pdwOper = TOKEN_OPER_BITWISELEFT;
         return 2;
      }
      *pdwOper = TOKEN_OPER_LESSTHAN;
      return 1;

   case L'>':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_GREATERTHANEQUAL;
         return 2;
      }
      else if (psz[1] == L'>') {
         if (psz[2] == L'=') {
            *pdwOper = TOKEN_OPER_ASSIGNBITWISERIGHT;
            return 3;
         }
         *pdwOper = TOKEN_OPER_BITWISERIGHT;
         return 2;
      }
      *pdwOper = TOKEN_OPER_GREATERTHAN;
      return 1;

   case L'=':
      if (psz[1] == L'=') {
         if (psz[2] == L'=') {
            *pdwOper = TOKEN_OPER_EQUALITYSTRICT;
            return 3;
         }
         *pdwOper = TOKEN_OPER_EQUALITY;
         return 2;
      }
      *pdwOper = TOKEN_OPER_ASSIGN;
      return 1;

   case L'&':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_ASSIGNBITWISEAND;
         return 2;
      }
      else if (psz[1] == L'&') {
         *pdwOper = TOKEN_OPER_LOGICALAND;
         return 2;
      }
      *pdwOper = TOKEN_OPER_BITWISEAND;
      return 1;

   case L'^':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_ASSIGNBITWISEXOR;
         return 2;
      }
      *pdwOper = TOKEN_OPER_BITWISEXOR;
      return 1;

   case L'|':
      if (psz[1] == L'=') {
         *pdwOper = TOKEN_OPER_ASSIGNBITWISEOR;
         return 2;
      }
      else if (psz[1] == L'|') {
         *pdwOper = TOKEN_OPER_LOGICALOR;
         return 2;
      }
      *pdwOper = TOKEN_OPER_BITWISEOR;
      return 1;

   case L'?':
      *pdwOper = TOKEN_OPER_CONDITIONAL;
      return 1;

   case L':':
      *pdwOper = TOKEN_OPER_COLON;
      return 1;

   case L';':
      *pdwOper = TOKEN_OPER_SEMICOLON;
      return 1;

   case L',':
      *pdwOper = TOKEN_OPER_COMMA;
      return 1;

   }

   return 0; // none
}


/*****************************************************************************************
TokenizeString - Takes in a string (assuming that the first " has been parsed, and parses
the rest.

inputs
   PWSTR       psz - Current location
   PWSTR       pszStart - Start of entire file, so can add decent error offsets
   PCMem       pMem - Cleared out and filled with string
   PCMIFLErr   pErr - Error to report to
   PWSTR       pszMethName - Method name to display in errors
   PWSTR       pszErrLink - Link to use if an error occurs in the compilation
   BOOL        fIsChar - If TRUE then assume it's a character, and has ' around it instead of "
returns
   DWORD - Number of characters used up. Will be pointing after last "
*/
static _inline void AppendString (PCMem pMem, WCHAR c)
{
   size_t dwNeed = pMem->m_dwCurPosn + sizeof(WCHAR);
   if (!pMem->Required (dwNeed))
      return;
   *((PWSTR)((PBYTE)(pMem->p) + pMem->m_dwCurPosn)) = c;
   pMem->m_dwCurPosn = dwNeed;
}

static DWORD TokenizeString (PWSTR psz, PWSTR pszStart, PCMem pMem, PCMIFLErrors pErr,
                             PWSTR pszMethName, PWSTR pszErrLink, BOOL fIsChar)
{
   DWORD dwUsed = 0;
   PWSTR pszOrig = psz;

   // zero string
   pMem->m_dwCurPosn = 0;

   // repeat
   while (TRUE) {
      // if end-of string then error
      if (psz[0] == 0) {
         DWORD dwStart = (DWORD)((PBYTE)pszOrig - (PBYTE)pszStart) / sizeof(WCHAR);
         pErr->Add (fIsChar ? L"Character unexpectedly ended." : L"String unexpectedly ended.", pszErrLink, pszMethName, TRUE,
            dwStart, dwStart + (DWORD)wcslen(pszOrig));
         break;
      }

      // end of string
      if (psz[0] == (fIsChar ? L'\'' : L'\"')) {
         // end of string
         dwUsed++;
         break;
      }

      // if newline in string then error
      if ((psz[0] == L'\r') || (psz[0] == L'\n')) {
         DWORD dwStart = (DWORD)((PBYTE)psz - (PBYTE)pszStart) / sizeof(WCHAR);
         pErr->Add (L"Newline in string.", pszErrLink, pszMethName, TRUE,
            dwStart, dwStart + 1);
         // purposely go on
      }

      // if not escape character then just add
      if (psz[0] != L'\\') {
         AppendString (pMem, psz[0]);
         psz++;
         dwUsed++;
         continue;
      }

      // if escape character
      psz++;
      dwUsed++;

      // see if it's octal
      // DOCUMENT: Octal string escape
      if ((psz[0] >= L'0') && (psz[0] <= L'7')) {
         if ((psz[1] < L'0') || (psz[1] > L'7') || (psz[2] < L'0') || (psz[2] > L'7')) {
            // error in octal string
            DWORD dwStart = (DWORD)((PBYTE)psz - (PBYTE)pszStart) / sizeof(WCHAR);
            pErr->Add (L"Octal escape sequence should only contain numbers between 0 and 7.",
               pszErrLink, pszMethName, TRUE,
               dwStart, dwStart + 3);
            continue;
         }

         // add this
         AppendString (pMem,
            (psz[0] - L'0') * 8 * 8 +
            (psz[1] - L'0') * 8 +
            (psz[2] - L'0'));

         // skip
         psz += 3;
         dwUsed += 3;
         continue;
      }


      // see if it's hex
      // DOCUMENT: Hex string escape
      if ((psz[0] == L'x') || (psz[0] == L'X')) {
         psz++;
         dwUsed++;

         DWORD i;
         WCHAR wVal = 0;
         for (i = 0; i < 3; i++) {
            if ((psz[i] >= L'0') && (psz[i] <= L'9')) {
               wVal = wVal * 16 + (psz[i] - L'0');
               continue;
            }
            if ((psz[i] >= L'A') && (psz[i] <= L'F')) {
               wVal = wVal * 16 + (psz[i] - L'A' + 10);
               continue;
            }
            if ((psz[i] >= L'a') && (psz[i] <= L'f')) {
               wVal = wVal * 16 + (psz[i] - L'a' + 10);
               continue;
            }
            break;   // error
         }
         if (i < 3) {
            // error in hex string
            DWORD dwStart = (DWORD)((PBYTE)psz - (PBYTE)pszStart) / sizeof(WCHAR);
            pErr->Add (L"Hex escape sequence should only contain numbers between 0 and f.",
               pszErrLink, pszMethName, TRUE,
               dwStart, dwStart + 3);
            continue;
         }

         // add this
         AppendString (pMem, wVal);

         // skip
         psz += 3;
         dwUsed += 3;
         continue;
      }


      // other escapes
      // DOCUMENT: Other escapes
      WORD wVal = 0;
      switch (psz[0]) {
         case L'n':
            wVal = L'\n';
            break;
         case L't':
            wVal = L'\t';
            break;
         case L'v':
            wVal = L'\v';
            break;
         case L'b':
            wVal = L'\b';
            break;
         case L'r':
            wVal = L'\r';
            break;
         case L'f':
            wVal = L'\f';
            break;
         case L'a':
            wVal = L'\a';
            break;
         case L'\\':
            wVal = L'\\';
            break;
         case L'?':
            wVal = L'\?';
            break;
         case L'\'':
            wVal = L'\'';
            break;
         case L'\"':
            wVal = L'\"';
            break;
         case L'0':
            wVal = L'\0';
            break;
         case 0:
            continue;   // just ignore this
         default:
            DWORD dwStart = (DWORD)((PBYTE)psz - (PBYTE)pszStart) / sizeof(WCHAR);
            pErr->Add (L"Unknown escape sequence.",
               pszErrLink, pszMethName, TRUE,
               dwStart, dwStart + 1);
            wVal = psz[0];
            break;
      }
      AppendString (pMem, wVal);
      psz++;
      dwUsed++;
   }

   // finall null
   AppendString (pMem, 0);
   return dwUsed;
}
   
/*****************************************************************************************
CMIFLCompiled::Tokenize - Takes a string of code and converts it into tokens.
It returns a pointer to the first token in the linked list, or NULL if no tokens
generated.

inputs
   PWSTR       pszText  - Text to analyze
returns
   PMIFLTOKEN - Token. NOTE: This list must be freed using ESCFREE()
*/
PMIFLTOKEN CMIFLCompiled::Tokenize (PWSTR pszText)
{
   PMIFLTOKEN pStart = NULL;
   PMIFLTOKEN pTail = NULL;
   PMIFLTOKEN *ppTail = &pStart;
   PWSTR pszOrig = pszText;
   DWORD dwUsed, dwOper;
   double fValue;
   __int64 iValue;

   // repeat
   while (pszText[0]) {
      // skip whitespace
      while (iswspace (pszText[0]))
         pszText++;
      if (!pszText[0])
         break;

      // have somthing, so might as well remember the start
      DWORD dwStart = (DWORD)((PBYTE) pszText - (PBYTE)pszOrig) / sizeof(WCHAR);

      // if it's a comment then remove
      // DOCUMENT:: Comments using /* */
      if ((pszText[0] == L'/') && (pszText[1] == L'*')) {
         PWSTR pszStart = pszText;
         pszText += 2;

         PWSTR pszNext = wcsstr (pszText, L"*/");
         if (!pszNext) {
            // error. didn't find the end of the comment
            m_pErr->Add (L"No end of comment found.", m_pszErrLink, m_pszMethName, TRUE,
               dwStart, dwStart + (DWORD)wcslen(pszStart));
            return pStart;
         }

         // found end
         pszText = pszNext + 2;
         continue;
      }

      // DOCUMENT: Comment with //
      if ((pszText[0] == L'/') && (pszText[1] == L'/')) {
         pszText += 2;

         while (pszText[0] && (pszText[0] != L'\r') && (pszText[0] != L'\n'))
            pszText++;
         continue;
      }

      // try for a hexidecimal number
      // DOCUMENT: Hex numbers, 0xabc
      dwUsed = ParseHexPrefix (pszText, &iValue);
      if (dwUsed) {
         PMIFLTOKEN pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(MIFLTOKEN));
         if (!pNew)
            continue;   // out of memory, dont worry about...
         memset (pNew, 0, sizeof(MIFLTOKEN));
         pNew->dwType = TOKEN_DOUBLE;
         pNew->dwCharStart = dwStart;
         pNew->dwCharEnd = dwStart + dwUsed;
         pNew->fValue = (double) iValue;
         pNew->pBack = pTail;

         // add to link
         *ppTail = pTail = pNew;
         ppTail = (PMIFLTOKEN*) &pNew->pNext;

         // next
         pszText += dwUsed;
         continue;
      }

      // try for a fp or natural number
      // DOCUMENT: Numbers and floating point numbers
      dwUsed = ParseDouble (pszText, FALSE, &fValue);
      if (dwUsed) {
         PMIFLTOKEN pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(MIFLTOKEN));
         if (!pNew)
            continue;   // out of memory, dont worry about...
         memset (pNew, 0, sizeof(MIFLTOKEN));
         pNew->dwType = TOKEN_DOUBLE;
         pNew->dwCharStart = dwStart;
         pNew->dwCharEnd = dwStart + dwUsed;
         pNew->fValue = fValue;
         pNew->pBack = pTail;

         // add to link
         *ppTail = pTail = pNew;
         ppTail = (PMIFLTOKEN*) &pNew->pNext;

         // next
         pszText += dwUsed;
         continue;
      }

      // identifier
      // DOCUMENT: Valid identifier starts with letter/underscrore, and has those + number
      if (iswalpha(pszText[0]) || (pszText[0] == L'_')) {
         PWSTR pszStart = pszText;
         pszText++;

         while (iswalpha(pszText[0]) || (pszText[0] == L'_') || ((pszText[0] >= L'0') && (pszText[0] <= L'9')))
            pszText++;
         DWORD dwChars = (DWORD) ((PBYTE)pszText - (PBYTE)pszStart) / sizeof(WCHAR);

         // add to link
         PMIFLTOKEN pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(MIFLTOKEN));
         if (!pNew)
            continue;   // out of memory, dont worry about...
         memset (pNew, 0, sizeof(MIFLTOKEN));
         pNew->dwType = TOKEN_IDENTIFIER;
         pNew->dwCharStart = dwStart;
         pNew->dwCharEnd = dwStart + dwChars;
         pNew->pszString = (PWSTR)ESCMALLOC((dwChars+1)*sizeof(WCHAR));
         if (!pNew->pszString) {
            // out of memory, shouldnt happen often enough to worry about
            ESCFREE (pNew);
            continue;
         }
         memcpy (pNew->pszString, pszStart, dwChars * sizeof(WCHAR));
         pNew->pszString[dwChars] = 0; // NULL terminate
         MIFLToLower (pNew->pszString);   // lower case so faster checks

         // see if it matches a token...
         PMIFLIDENT pmi;
         pmi = (PMIFLIDENT) m_hIdentifiers.Find (pNew->pszString, FALSE);
         if (pmi && (pmi->dwType == MIFLI_TOKEN)) {
            // found a token, so just convert it now
            ESCFREE (pNew->pszString);
            pNew->pszString = NULL;
            pNew->dwType = pmi->dwIndex;

            // special case. Some tokens convert right to numbers...
            switch (pNew->dwType) {
            case TOKEN_TRUE:
               pNew->dwType = TOKEN_BOOL;
               pNew->dwValue = 1;
               break;
            case TOKEN_FALSE:
               pNew->dwType = TOKEN_BOOL;
               pNew->dwValue = 0;
               break;
            }
         }
         pNew->pBack = pTail;

         // add to link
         *ppTail = pTail = pNew;
         ppTail = (PMIFLTOKEN*) &pNew->pNext;

         continue;
      }

      // symbols
      dwUsed = ParseOperator(pszText, &dwOper);
      if (dwUsed) {
         PMIFLTOKEN pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(MIFLTOKEN));  // BUGBUG - memory leak here with TOKEN_OPER_BRACEL
         if (!pNew)
            continue;   // out of memory, dont worry about...
         memset (pNew, 0, sizeof(MIFLTOKEN));
         pNew->dwType = dwOper;
         pNew->dwCharStart = dwStart;
         pNew->dwCharEnd = dwStart + dwUsed;
         pNew->pBack = pTail;

         // add to link
         *ppTail = pTail = pNew;
         ppTail = (PMIFLTOKEN*) &pNew->pNext;

         // next
         pszText += dwUsed;
         continue;
      }

      // strings
      if (pszText[0] == L'\"') {
         pszText++;

         dwUsed = TokenizeString (pszText, pszOrig, &gMemTemp,
            m_pErr, m_pszMethName, m_pszErrLink, FALSE);
         pszText += dwUsed;

         // add the token
         PMIFLTOKEN pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(MIFLTOKEN));
         if (!pNew)
            continue;   // out of memory, dont worry about...
         memset (pNew, 0, sizeof(MIFLTOKEN));
         pNew->dwType = TOKEN_STRING;
         pNew->dwCharStart = dwStart;
         pNew->dwCharEnd = dwStart + dwUsed + 1;
         DWORD dwChars = (DWORD)wcslen((PWSTR)gMemTemp.p);
         pNew->pszString = (PWSTR)ESCMALLOC((dwChars+1)*sizeof(WCHAR));
         if (!pNew->pszString) {
            // out of memory, shouldnt happen often enough to worry about
            ESCFREE (pNew);
            continue;
         }
         memcpy (pNew->pszString, gMemTemp.p, (dwChars+1) * sizeof(WCHAR));
         pNew->pBack = pTail;

         // add to link
         *ppTail = pTail = pNew;
         ppTail = (PMIFLTOKEN*) &pNew->pNext;

         // next
         continue;
      }

      // characters
      // DOCUMENT: Characters
      if (pszText[0] == L'\'') {
         pszText++;

         dwUsed = TokenizeString (pszText, pszOrig, &gMemTemp,
            m_pErr, m_pszMethName, m_pszErrLink, TRUE);
         pszText += dwUsed;

         PWSTR psz = (PWSTR)gMemTemp.p;
         if (gMemTemp.m_dwCurPosn != 2*sizeof(WCHAR)) {
            m_pErr->Add (L"Invalid character.", m_pszErrLink, m_pszMethName, TRUE,
               dwStart, dwStart + dwUsed);
         }

         // add the token
         PMIFLTOKEN pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(MIFLTOKEN));
         if (!pNew)
            continue;   // out of memory, dont worry about...
         memset (pNew, 0, sizeof(MIFLTOKEN));
         pNew->dwType = TOKEN_CHAR;
         pNew->dwCharStart = dwStart;
         pNew->dwCharEnd = dwStart + dwUsed + 1;
         pNew->cValue = psz[0];
         pNew->pBack = pTail;

         // add to link
         *ppTail = pTail = pNew;
         ppTail = (PMIFLTOKEN*) &pNew->pNext;

         continue;
      }

      // else, unknown character
      m_pErr->Add (L"Unknown character.", m_pszErrLink, m_pszMethName, TRUE,
         dwStart, dwStart + 1);
      pszText++;
   }

   return pStart;
}



/*****************************************************************************************
CMIFLCompiled::TokensArrangeParen - Pulls out braces, parent, and brackets, finding matching ones

inputs
   PMIFLTOKEN  pToken - Token to start arranging
   DWORD       dwTokLeft - Token for the left brace, paren, bracket... this will be kept
   DWORD       dwTokRight - Token for the right brace, parent, bracket
   PCMIFLErr   pErr - Error to report to
   PWSTR       m_pszMethName - Method name to display in errors
   PWSTR       m_pszErrLink - Link to use if an error occurs in the compilation
returns
   PMIFLTOKEN - First token, just in case rearranged the first element.
*/
PMIFLTOKEN CMIFLCompiled::TokensArrangeParen (PMIFLTOKEN pToken, DWORD dwTokLeft, DWORD dwTokRight)
{
   // keep list of left brackets
   CListFixed lLeft;
   lLeft.Init (sizeof(PMIFLTOKEN));

   // loop
   PMIFLTOKEN pCur;
   for (pCur = pToken; pCur; pCur = (PMIFLTOKEN) pCur->pNext) {
      // go down and handle those first
      PMIFLTOKEN pDown;
      for (pDown = (PMIFLTOKEN) pCur->pDown; pDown; pDown = (PMIFLTOKEN)pDown->pDown)
         TokensArrangeParen (pDown, dwTokLeft, dwTokRight);
         // NOTE: Can ignire the return value since tokens arrange parent doesn't change first one, ever

      // BUGFIX - Was getting into trouble with "((actor[0] != 1));", because
      // would convert [, and this would a down to the list, and
      // would re-parser same code twice, causing probles, at the [0],
      // so make sure it doesn't redo twice
      // so added && !pCur->pDown
      if ((pCur->dwType == dwTokLeft) && !pCur->pDown) {
         lLeft.Add (&pCur);
         continue;
      }

      if (pCur->dwType != dwTokRight)
         continue;   // ignore this

      // found a right paren...


      // make sure there's a left
      if (!lLeft.Num()) {
         m_pErr->Add (L"No matching left paren/bracket/brace found.", m_pszErrLink, m_pszMethName,
            TRUE, pCur->dwCharStart, pCur->dwCharEnd);

         // ignore this and continue on
         continue;
      }

      // else, have a left and right group, so bundle it up and move it into down
      PMIFLTOKEN pLeft;
      pLeft = *((PMIFLTOKEN*) lLeft.Get(lLeft.Num()-1));
      lLeft.Remove (lLeft.Num()-1);

      // pCur == pLeft->pNext then empty brackets...
      if (pCur != (PMIFLTOKEN)pLeft->pNext) {
         pLeft->pDown = pLeft->pNext;
         ((PMIFLTOKEN)pLeft->pDown)->pBack = NULL;
         ((PMIFLTOKEN)pCur->pBack)->pNext = NULL;
      }

      // and connect the two parens
      pLeft->pNext = pCur->pNext;
      if (pCur->pNext)
         ((PMIFLTOKEN)pCur->pNext)->pBack = pLeft;
      pCur->pNext = pCur->pBack = NULL;

      // make range be whole contents
      pLeft->dwCharEnd = pCur->dwCharEnd;

      TokenFree (pCur); // delete right parent

      pCur = pLeft;  // to reset and continue
   } // pCur

   // see if used all the parents
   if (lLeft.Num()) {
      // haven't conneted all
      DWORD i;
      for (i = 0; i < lLeft.Num(); i++) {
         PMIFLTOKEN pLeft;
         pLeft = *((PMIFLTOKEN*) lLeft.Get(i));

         m_pErr->Add (L"No matching right paren/bracket/brace found.", m_pszErrLink, m_pszMethName,
            TRUE, pLeft->dwCharStart, pLeft->dwCharEnd);
      }
   }

   return pToken;
}



/*****************************************************************************************
CMIFLCompiled::TokensIdentResolve - Resolve identifiers in an expression.

NOTE: This does NOT go into sub-expressions because inherently expressions will recurse
into themselves.

inputs
   PMIFLTOKEN  pToken - Token to start with
*/
void CMIFLCompiled::TokensIdentResolve (PMIFLTOKEN pToken)
{
   for (; pToken; pToken = (PMIFLTOKEN)pToken->pNext) {
      // only care about these
      if (pToken->dwType != TOKEN_IDENTIFIER)
         continue;

      // see if it's a variable...
      DWORD dwVarIndex = m_phCodeVars ? m_phCodeVars->FindIndex (pToken->pszString, FALSE) : -1;
      if (dwVarIndex != -1) {
         // found variable
         ESCFREE (pToken->pszString);
         pToken->pszString = NULL;
         pToken->dwType = TOKEN_VARIABLE;
         pToken->dwValue = dwVarIndex;
         continue;
      }

      // else, see if it's a local method/property
      PMIFLIDENT pmi;
      if (m_pObject && (pmi = (PMIFLIDENT)m_pObject->m_hPrivIdentity.Find (pToken->pszString, FALSE))) {
         ESCFREE (pToken->pszString);
         pToken->pszString = NULL;
         pToken->dwType = (pmi->dwType == MIFLI_METHPRIV) ? TOKEN_METHPRIV : TOKEN_PROPPRIV;
            // know that only two types of entities in this list
         pToken->dwValue = pmi->dwID;
         continue;
      }

      // else, see if it's a global token
      pmi = (PMIFLIDENT)m_hIdentifiers.Find (pToken->pszString, FALSE);
      if (pmi) {
         ESCFREE (pToken->pszString);
         pToken->pszString = NULL;
         pToken->dwValue = pmi->dwID;

         switch (pmi->dwType) {
         case MIFLI_METHDEF:     // identifier is a method definition
            pToken->dwType = TOKEN_METHPUB;
            break;
         case MIFLI_PROPDEF:     // property definition
            pToken->dwType = TOKEN_PROPPUB;
            break;
         case MIFLI_GLOBAL:     // global variale (only, not object class)
            pToken->dwType = TOKEN_GLOBAL;
            break;
         case MIFLI_FUNC:     // function
            pToken->dwType = TOKEN_FUNCTION;
            break;
         case MIFLI_OBJECT:     // class or object
            {
               // look to see what's left of this...
               PMIFLTOKEN pLeft = (PMIFLTOKEN)pToken->pBack;
               if (pLeft && (pLeft->dwType == TOKEN_OPER_NEW)) {
                  pToken->dwType = TOKEN_CLASS;
                  pToken->dwValue = pmi->dwIndex;  // for classes use the object index
               }
               else {
                  // since not after a new, must be a global variable reference
                  // but, if the ID ends up being -1 then it isn't a global variable
                  // either
                  pToken->dwType = TOKEN_GLOBAL;
                  if (pToken->dwValue == -1)
                     goto unknown;
               }
            }
            break;
         case MIFLI_STRING:     // string
            pToken->dwType = TOKEN_STRINGTABLE;
            break;
         case MIFLI_RESOURCE:     // resource
            pToken->dwType = TOKEN_RESOURCE;
            break;
         default:
            goto unknown;
         }
         continue;
      } // if in main token list

unknown:
      // unknown token
      m_pErr->Add (L"Unknown identifier.", m_pszErrLink, m_pszMethName, TRUE,
         pToken->dwCharStart, pToken->dwCharEnd);
   } // over tokens

}


/*****************************************************************************************
CanConnectToOper - Returns TRUE if the type of token and connect to an operator (such as
a variable), of FALSE if it can't (such as another operator)

inputs
   PMIFLTOKEN  pToken - Token to start with
   BOOL        fLValue - Set to TRUE if doing a ++ or --, or if must be a lValue
returns
   BOOL - TRUE if can connect
*/
static BOOL CanConnectToOper (PMIFLTOKEN pToken, BOOL fLValue = FALSE)
{
   if (!pToken)
      return FALSE;

   // if the adjacent token is a value, can operate on
   if ((pToken->dwType >= TOKEN_VALUESTART)&& (pToken->dwType <= TOKEN_VALUEEND)) {
      // if ++ or -- then only apply to certain types
      // NOTE: Check for l-value isn't exhaustive
      if (fLValue) switch (pToken->dwType) {
      case TOKEN_DOUBLE:      // double
      case TOKEN_CHAR:      // character
      case TOKEN_NULL:      // valid type of value
      case TOKEN_UNDEFINED:      // valid type of value, DOCUMENT: Undefined
      case TOKEN_BOOL:      // boolean. dwValue is 0 or 1
      case TOKEN_STRINGTABLE:      // dwValue is reference to string table
      case TOKEN_RESOURCE:      // dwValue is reference to resource
      case TOKEN_METHPUB:      // calling a public method. dwValue = method ID
      case TOKEN_METHPRIV:      // accessing a public property. dwValue = prop ID
      case TOKEN_FUNCTION:      // accessing a global function. dwValue = function ID
      case TOKEN_CLASS:      // accessing a class. dwValue = class ID. Used only after new token
         return FALSE;
      }

      return TRUE;
   }

   // if the adjacent token is an operator, can operate on so long as the operator
   // contains a down
   if ((pToken->dwType >= TOKEN_OPER_START) && (pToken->dwType <= TOKEN_OPER_END)) {
      // NOTE: fLValue doesn't catch everything, because some operators will return
      // entities that are not lValues

      if (fLValue) switch (pToken->dwType) {
      case TOKEN_OPER_DOT:
         // could be LValue
         break;

      default:
         return FALSE;
      }
      return pToken->pDown ? TRUE : FALSE;
   }

   // remaining tests
   switch (pToken->dwType) {
   // BUGFIX - Then removed... case TOKEN_OPER_PARENL: // BUGFIX - Added so (list[0]).property = 2; works
   case TOKEN_OPER_BRACKETL:      // [
   case TOKEN_OPER_BRACEL:      // {
      // only if have contents
      return pToken->pDown ? TRUE : FALSE;
   }


   return FALSE;
}

/*****************************************************************************************
CMIFLCompiled::TokensParseOrderOfOperByRange - Parses the order of operations looking
for operators within the given range. If they're found then their left and right elements
are grouped together.

inputs
   PMIFLTOKEN  pToken - Token to start with
   DWORD       dwTokenMin - Minimum token, TOKEN_XXX
   DWORD       dwTokenMax - Maximum token (inclusive), TOKEN_XXX
   BOOL        fFromLeft - If TRUE then parse from left to right, else right to left
   int         iSides - If 0 then both left and right, if 1 then associates only with right
               if -1 then only with left, if -2 then either left or right, but not both.
   BOOL        fLValueOnLeft - If TRUE then left must be an LValue
   BOOL        fParseParen - If set to TRUE (which it should be only for _DOT) then
               this parses the parenthesis too. Also, must be left-to-right parse too
returns
   PMIFLTOKEN - New starting token. May be different than pToken
*/
PMIFLTOKEN CMIFLCompiled::TokensParseOrderOfOperByRange (PMIFLTOKEN pToken,
   DWORD dwTokenMin, DWORD dwTokenMax, BOOL fFromLeft, int iSides, BOOL fLValueOnLeft,
   BOOL fParseParen)
{
   // starting point
   PMIFLTOKEN pStart = pToken;
   if (!fFromLeft)
      for (; pStart->pNext; pStart = (PMIFLTOKEN)pStart->pNext);

   // loop..
   PMIFLTOKEN pCur, pCur2;
   for (pCur = pStart; pCur; pCur = (PMIFLTOKEN) (fFromLeft ? pCur->pNext : pCur->pBack)) {
      // BUGFIX - Needed to put paren and bracket parsing in here so that they're
      // at the same level as a . operator. Therefore, if the parse paren flag is
      // set, check to see if this a a paren. If so, modify bits.
      if (fParseParen)
         if (pCur2 = TokensParseParen (pCur, &pToken)) {
            pCur = pCur2;
            continue;
         }

      if ((pCur->pDown) || (pCur->dwType < dwTokenMin) || (pCur->dwType > dwTokenMax))
         continue;      // not what looking for

      // else, combine...
      PMIFLTOKEN pLeft, pRight;
      BOOL fLeftOK, fRightOK;
      pLeft = (PMIFLTOKEN)pCur->pBack;
      pRight = (PMIFLTOKEN)pCur->pNext;
      fLeftOK = CanConnectToOper (pLeft, (iSides == -2) || fLValueOnLeft);
      fRightOK = CanConnectToOper (pRight, (iSides == -2));

      // if side == 1 (associate with right), then ignore left
      if ((iSides == 1) && (pCur->dwType != TOKEN_OPER_NEGATION))
         fLeftOK = FALSE;
      else if (iSides == -1)
         fRightOK = FALSE;

      // if this is negation and both the left and right are OK then convert to a minus
      if ((pCur->dwType == TOKEN_OPER_NEGATION) && fLeftOK && fRightOK) {
         pCur->dwType = TOKEN_OPER_SUBTRACT;
         continue;
      }

      // verify parameters OK
      if (!fLeftOK && ((iSides == 0) || (iSides == -1))) {
         m_pErr->Add (L"Operator cannot use the left input as a parameter.", m_pszErrLink,
            m_pszMethName, TRUE,
            pLeft ? pLeft->dwCharStart : pCur->dwCharStart,
            pLeft ? pLeft->dwCharEnd : pCur->dwCharEnd);
         continue;
      }
      if (!fRightOK && ((iSides == 0) || (iSides == 1))) {
         m_pErr->Add (L"Operator cannot use the right input as a parameter.", m_pszErrLink,
            m_pszMethName, TRUE,
            pRight ? pRight->dwCharStart : pCur->dwCharStart,
            pRight ? pRight->dwCharEnd : pCur->dwCharEnd);
         continue;
      }
      if (iSides == -2) {
         if (!fLeftOK && !fRightOK) {
            m_pErr->Add (L"The operator needs to be left or right of an L-value.", m_pszErrLink,
               m_pszMethName, TRUE, pCur->dwCharStart, pCur->dwCharEnd);
            continue;
         }
         if (fLeftOK && fRightOK) {
            m_pErr->Add (L"Can't determine if the operator should be assigned to the left or right L-value.", m_pszErrLink,
               m_pszMethName, TRUE, pCur->dwCharStart, pCur->dwCharEnd);
            continue;
         }
      }

      // which side attach operation to?
      PMIFLTOKEN pNew, pDown;
      if (fLeftOK && fRightOK) {
         if (pToken == pLeft)
            pToken = pCur; // since will be sucking left in

         pCur->pBack = (PMIFLTOKEN) pLeft->pBack;
         pCur->pNext = (PMIFLTOKEN) pRight->pNext;
         if (pCur->pBack)
            ((PMIFLTOKEN)pCur->pBack)->pNext = pCur;
         if (pCur->pNext)
            ((PMIFLTOKEN)pCur->pNext)->pBack = pCur;
         pLeft->pBack = pLeft->pNext = NULL;
         pRight->pBack = pRight->pNext = NULL;

         // arg1
         pDown = pCur;
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG1;
            pNew->pNext = pLeft;
            TokenFindRange (pLeft, &pNew->dwCharStart, &pNew->dwCharEnd);
            pLeft->pBack = pNew;
            pDown->pDown = pNew; // so add argyement

            // BUGFIX - Operator range is entire thing
            pCur->dwCharStart = min(pCur->dwCharStart, pLeft->dwCharStart);
         }  // if pNew

         // arg2
         pDown = (PMIFLTOKEN) pCur->pDown;
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG2;
            pNew->pNext = pRight;
            TokenFindRange (pRight, &pNew->dwCharStart, &pNew->dwCharEnd);
            pRight->pBack = pNew;
            pDown->pDown = pNew; // so add argyement

            // BUGFIX - Operator range is entire thing
            pCur->dwCharEnd = max(pCur->dwCharEnd, pRight->dwCharEnd);
         }  // if pNew

      }
      else if (fLeftOK) {
         if (pToken == pLeft)
            pToken = pCur; // since will be sucking left in

         pCur->pBack = (PMIFLTOKEN) pLeft->pBack;
         if (pCur->pBack)
            ((PMIFLTOKEN)pCur->pBack)->pNext = pCur;
         pLeft->pBack = pLeft->pNext = NULL;

         // arg1
         pDown = pCur;
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG1;
            pNew->pNext = pLeft;
            TokenFindRange (pLeft, &pNew->dwCharStart, &pNew->dwCharEnd);
            pLeft->pBack = pNew;
            pDown->pDown = pNew; // so add argyement

            // BUGFIX - Operator range is entire thing
            pCur->dwCharStart = min(pCur->dwCharStart, pLeft->dwCharStart);
         }  // if pNew
      }
      else if (fRightOK) {
         pCur->pNext = (PMIFLTOKEN) pRight->pNext;
         if (pCur->pNext)
            ((PMIFLTOKEN)pCur->pNext)->pBack = pCur;
         pRight->pBack = pRight->pNext = NULL;

         // if this is negation and pRight is a double, then just negate pRight
         // and forget the negation
         if ((pCur->dwType == TOKEN_OPER_NEGATION) && (pRight->dwType == TOKEN_DOUBLE)) {
            pCur->dwType = pRight->dwType;
            pCur->fValue = -pRight->fValue;
            pCur->dwCharEnd = pRight->dwCharEnd;

            TokenFree (pRight);
            continue;
         }

         // arg1
         pDown = pCur;
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            // NOTE: If it's a right-connecting expression then always use arg2
            // this allows to identify is ++ on left or right
            pNew->dwType = TOKEN_ARG2;
            pNew->pNext = pRight;
            TokenFindRange (pRight, &pNew->dwCharStart, &pNew->dwCharEnd);
            pRight->pBack = pNew;
            pDown->pDown = pNew; // so add argyement

            // BUGFIX - Operator range is entire thing
            pCur->dwCharEnd = max(pCur->dwCharEnd, pRight->dwCharEnd);
         }  // if pNew
      }

   } // pCur

   return pToken;
}

/*****************************************************************************************
CMIFLCompiled::TokensParseOrderOfOperConditional - Parses the order of operations looking
for contiionals

inputs
   PMIFLTOKEN  pToken - Token to start with
   DWORD       dwTokenMin - Minimum token, TOKEN_XXX
   DWORD       dwTokenMax - Maximum token (inclusive), TOKEN_XXX
   BOOL        fFromLeft - If TRUE then parse from left to right, else right to left
   int         iSides - If 0 then both left and right, if 1 then associates only with right
               if -1 then only with left, if -2 then either left or right, but not both.
returns
   PMIFLTOKEN - New starting token. May be different than pToken
*/
PMIFLTOKEN CMIFLCompiled::TokensParseOrderOfOperConditional (PMIFLTOKEN pToken)
{
   // starting point
   BOOL fFromLeft = FALSE;
   PMIFLTOKEN pStart = pToken;
   if (!fFromLeft)
      for (; pStart->pNext; pStart = (PMIFLTOKEN)pStart->pNext);

   // loop..
   PMIFLTOKEN pCur;
   for (pCur = pStart; pCur; pCur = (PMIFLTOKEN) (fFromLeft ? pCur->pNext : pCur->pBack)) {
      if ((pCur->pDown) || (pCur->dwType != TOKEN_OPER_CONDITIONAL))
         continue;      // not what looking for

      // next few operators should be known
      PMIFLTOKEN pIf = (PMIFLTOKEN)pCur->pBack;
      PMIFLTOKEN pCase1 = (PMIFLTOKEN)pCur->pNext;
      PMIFLTOKEN pColon = pCase1 ? (PMIFLTOKEN)pCase1->pNext : NULL;
      PMIFLTOKEN pCase2 = pColon ? (PMIFLTOKEN)pColon->pNext : NULL;

      if (!pColon || (pColon->dwType != TOKEN_OPER_COLON)) {
         m_pErr->Add (L"':' expected.", m_pszErrLink,
            m_pszMethName, TRUE,
            pColon ? pColon->dwCharStart : pCur->dwCharStart,
            pColon ? pColon->dwCharEnd : pCur->dwCharEnd);
         continue;
      }

      BOOL fIfOK, fCase1OK, fCase2OK;
      fIfOK = CanConnectToOper (pIf, FALSE);
      fCase1OK = CanConnectToOper (pCase1, FALSE);
      fCase2OK = CanConnectToOper (pCase2, FALSE);

      if (!fIfOK) {
         m_pErr->Add (L"Missing argument before '?'.", m_pszErrLink,
            m_pszMethName, TRUE,
            pIf ? pIf->dwCharStart : pCur->dwCharStart,
            pIf ? pIf->dwCharEnd : pCur->dwCharEnd);
         continue;
      }
      if (!fCase1OK) {
         m_pErr->Add (L"Missing argument after '?'.", m_pszErrLink,
            m_pszMethName, TRUE,
            pCase1 ? pCase1->dwCharStart : pCur->dwCharStart,
            pCase1 ? pCase1->dwCharEnd : pCur->dwCharEnd);
         continue;
      }
      if (!fCase2OK) {
         m_pErr->Add (L"Missing argument after ':'.", m_pszErrLink,
            m_pszMethName, TRUE,
            pCase2 ? pCase2->dwCharStart : pCur->dwCharStart,
            pCase2 ? pCase2->dwCharEnd : pCur->dwCharEnd);
         continue;
      }

      // disconnect...
      if (pToken == pIf)
         pToken = pCur;
      pCur->pBack = pIf->pBack;
      if (pCur->pBack)
         ((PMIFLTOKEN)pCur->pBack)->pNext = pCur;
      pCur->pNext = pCase2->pNext;
      if (pCur->pNext)
         ((PMIFLTOKEN)pCur->pNext)->pBack = pCur;
      pIf->pBack = pIf->pNext = NULL;
      pCase1->pBack = pCase1->pNext = NULL;
      pCase2->pBack = pCase2->pNext = NULL;
      pColon->pBack = pColon->pNext = NULL;
      TokenFree (pColon);

      // arg1
      PMIFLTOKEN pDown, pNew;
      pDown = pCur;
      pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
      if (pNew) {
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG1;
         pNew->pNext = pIf;
         TokenFindRange (pIf, &pNew->dwCharStart, &pNew->dwCharEnd);
         pIf->pBack = pNew;
         pDown->pDown = pNew; // so add argyement
      }  // if pNew

      // arg2
      pDown = (PMIFLTOKEN) pDown->pDown;
      pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
      if (pNew) {
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG2;
         pNew->pNext = pCase1;
         TokenFindRange (pCase1, &pNew->dwCharStart, &pNew->dwCharEnd);
         pCase1->pBack = pNew;
         pDown->pDown = pNew; // so add argyement
      }  // if pNew

      // arg3
      pDown = (PMIFLTOKEN) pDown->pDown;
      pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
      if (pNew) {
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG3;
         pNew->pNext = pCase2;
         TokenFindRange (pCase2, &pNew->dwCharStart, &pNew->dwCharEnd);
         pCase2->pBack = pNew;
         pDown->pDown = pNew; // so add argyement
      }  // if pNew

   } // pCur

   return pToken;
}


#if 0

// BUGFIX - No longer have this since must be the same order of operations
// as the . operator, so must be includes with that code

/*****************************************************************************************
CMIFLCompiled::TokensParseOrderOfOperParen - This looks for parens and brackets, and
determined if they're meant to be used as parenthesis vs. function call, or
new list vs. array.


inputs
   PMIFLTOKEN  pToken - Token to start with
returns
   PMIFLTOKEN - New starting token. May be different than pToken
*/
PMIFLTOKEN CMIFLCompiled::TokensParseOrderOfOperParen (PMIFLTOKEN pToken)
{
   PMIFLTOKEN pCur;
   BOOL fParen, fLeftOK;
   DWORD dwArgs, dwNonNullArgs;

   for (pCur = pToken; pCur; pCur = (PMIFLTOKEN)pCur->pNext) {
      if (pCur->dwType == TOKEN_OPER_PARENL)
         fParen = TRUE;
      else if (pCur->dwType == TOKEN_OPER_BRACKETL)
         fParen = FALSE;
      else
         continue;

      // see how many args
      dwArgs = dwNonNullArgs = 0;
      PMIFLTOKEN pDown = (PMIFLTOKEN)pCur->pDown;
      if (pDown && (pDown->dwType == TOKEN_OPER_COMMA)) {
         pDown = (PMIFLTOKEN) pDown->pDown;
         for (; pDown; pDown = (PMIFLTOKEN)pDown->pDown) {
            dwArgs++;
            if (pDown->pNext && (((PMIFLTOKEN)pDown->pNext)->dwType != TOKEN_NOP))
               dwNonNullArgs++;
         }
      }
      else if (pDown) {
         dwArgs++;
         if (pDown->dwType != TOKEN_NOP)
            dwNonNullArgs++;
      }

      // can operate on left?
      fLeftOK = FALSE;
      PMIFLTOKEN pMethName = NULL;  // so can guess the method name and hence # params
      PMIFLTOKEN pLeft = (PMIFLTOKEN)pCur->pBack;
      if (pLeft) switch (pLeft->dwType) {
      case TOKEN_STRING:
      case TOKEN_STRINGTABLE:
      case TOKEN_LISTDEF:
         // ok to use this for list, indexing array
         // DOCUMENT: Access string array using []
         fLeftOK = !fParen;
         break;

      case TOKEN_OPER_DOT:
         // either accessing private method/var, so assume true
         fLeftOK = TRUE;
         if (fParen) {
            // keep looking to see if can find a specific name
            PMIFLTOKEN pArg1 = (PMIFLTOKEN)pLeft->pDown;
            PMIFLTOKEN pArg2 = pArg1 ? (PMIFLTOKEN)pArg1->pDown : NULL;
            PMIFLTOKEN pRight = pArg2 ? (PMIFLTOKEN)pArg2->pNext : NULL;
            if (pRight && ((pRight->dwType == TOKEN_METHPUB) || (pRight->dwType == TOKEN_METHPRIV)))
               pMethName = pRight;
         }
         break;

      case TOKEN_PROPPUB:
      case TOKEN_GLOBAL:
      case TOKEN_PROPPRIV:
      case TOKEN_VARIABLE:
      case TOKEN_LISTINDEX:
      case TOKEN_FUNCCALL:
         // either accessing private method/var, so assume true
         fLeftOK = TRUE;
         break;

      case TOKEN_METHPUB:
      case TOKEN_FUNCTION:
      case TOKEN_METHPRIV:
         // could be a function in here
         fLeftOK = fParen;
         pMethName = pLeft;
         break;

        
      // NOTE: Not accepting case TOKEN_OPER_PARENL:
      // NOTE: Not accepting case TOKEN_OPER_BRACKETL:
      } // switch

      // how many args
      DWORD dwArgMin = 0, dwArgMax = 0;
      if (fParen) {
         if (fLeftOK) {
            // left is a function call
            dwArgMax = 1000;  // very large number

            // see if can rustle up the actual method
            PCMIFLMeth pMeth = NULL;
            if (pMethName) switch (pMethName->dwType) {
            case TOKEN_METHPUB:
               {
                  PMIFLIDENT pFind = (PMIFLIDENT) m_hUnIdentifiers.Find (pMethName->dwValue);
                  if (pFind)
                     pMeth = (PCMIFLMeth) pFind->pEntity;
               }
               break;
            case TOKEN_FUNCTION:
               {
                  PCMIFLFunc pFunc = m_pLib->FuncGet (pMethName->dwValue);
                  if (pFunc)
                     pMeth = &pFunc->m_Meth;
               }
               break;
            case TOKEN_METHPRIV:
               {
                  PMIFLIDENT pFind = (PMIFLIDENT) m_pObject->m_hUnPrivIdentity.Find (pMethName->dwValue);
                  if (pFind)
                     pMeth = &((PCMIFLFunc) pFind->pEntity)->m_Meth;
               }
               break;
            }
            
            // if know the method then may have min and max
            if (pMeth && !pMeth->m_fParamAnyNum)
               dwArgMin = dwArgMax = pMeth->m_lPCMIFLProp.Num();
         }
         else { // left is operation
            dwArgMin = dwArgMax = 1;
         }
      }
      else { // bracket
         if (fLeftOK)
            // access an array
            dwArgMin = dwArgMax = 1;
         else
            // List
            dwArgMax = 1000000;
      }


      // if have any null args then error
      if (dwArgs != dwNonNullArgs)
         m_pErr->Add (L"You must include values between all the commas.",
            m_pszErrLink, m_pszMethName, TRUE,
            pCur->dwCharStart, pCur->dwCharEnd);

      // if wrong number of arguements then error
      if ((dwArgs < dwArgMin) || (dwArgs > dwArgMax)) {
         if (dwArgMax == 0)
            m_pErr->Add (L"No parameters are allowed.",
               m_pszErrLink, m_pszMethName, TRUE,
               pCur->dwCharStart, pCur->dwCharEnd);
         else if (dwArgMax == 1)
            m_pErr->Add (L"Only one parameter is allowed.",
               m_pszErrLink, m_pszMethName, TRUE,
               pCur->dwCharStart, pCur->dwCharEnd);
         else {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"Only ");
            MemCat (&gMemTemp, (int) dwArgMax);
            MemCat (&gMemTemp, L" parameters are allowed.");
            m_pErr->Add ((PWSTR)gMemTemp.p,
               m_pszErrLink, m_pszMethName, TRUE,
               pCur->dwCharStart, pCur->dwCharEnd);
         }
      }

      // merge these operations....
      if (fLeftOK) {
         // either make it a function call or access into array
         // detact the left
         if (pToken == pLeft)
            pToken = pCur;
         pCur->pBack = pLeft->pBack;
         if (pCur->pBack)
            ((PMIFLTOKEN)pCur->pBack)->pNext = pCur;
         pLeft->pBack = pLeft->pNext = NULL;

         // convert pCur to being a function call
         pCur->dwType = fParen ? TOKEN_FUNCCALL : TOKEN_LISTINDEX;

         // put in some new args...
         PMIFLTOKEN pOldDown = (PMIFLTOKEN) pCur->pDown;
         pCur->pDown = NULL;

         PMIFLTOKEN pNew, pDown;
         pDown = pCur;
         pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(*pNew));
         if (pNew) {
            // arg1 contains the function name
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG1;
            pNew->dwCharStart = pLeft->dwCharStart;
            pNew->dwCharEnd = pLeft->dwCharEnd;
            pNew->pNext = pLeft;
            pLeft->pBack = pNew;
            pDown->pDown = pNew;
            pDown = pNew;
         }

         pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(*pNew));
         if (pNew) {
            // arg2 contains the function parameters, which may first be preceded by comma
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG2;
            pNew->dwCharStart = (pOldDown ? pOldDown : pCur)->dwCharStart;
            pNew->dwCharEnd = (pOldDown ? pOldDown : pCur)->dwCharEnd;
            pNew->pNext = pOldDown;
            if (pOldDown)
               pOldDown->pBack = pNew;
            pDown->pDown = pNew;
         }

         pCur->dwCharStart = pOldDown ? min(pOldDown->dwCharStart, pLeft->dwCharStart) : pLeft->dwCharStart;
         pCur->dwCharEnd = pOldDown ? max(pOldDown->dwCharEnd, pLeft->dwCharEnd) : pLeft->dwCharEnd;
      }
      else {
         // either take out parens or turn into list generator
         if (fParen) {
            // end up removing the list and putting in pDown instead

            PMIFLTOKEN pRight = (PMIFLTOKEN)pCur->pNext;
            PMIFLTOKEN pDown = (PMIFLTOKEN)pCur->pDown;
            
            if (pDown) {
               if (pToken == pCur)
                  pToken = pDown;

               if (pLeft)
                  pLeft->pNext = pDown;
               if (pRight)
                  pRight->pBack = pDown;
               pDown->pBack = pLeft;
               pDown->pNext = pRight;

               // delete pCur
               pCur->pBack = pCur->pNext = pCur->pDown = NULL;
               TokenFree (pCur);

               // since just replaced cur with down, the current one is down
               pCur = pDown;
            }
            else {
               // no down, so just convert this to a NOP to be easy
               pCur->dwType = TOKEN_NOP;
            }
         }
         else { // brackets
            // just convert the type
            pCur->dwType = TOKEN_LISTDEF;
         }
      }
   } // pCur

   return pToken;
}
#endif // 0



/*****************************************************************************************
CMIFLCompiled::TokensParseParen - If pCur is a parenthsis or bracket this will
parse it and return TRUE. Else it returns FALSE to indicate it's not a parenthsis
or token.


inputs
   PMIFLTOKEN  pCur - Current token looking at
   PMIFLTOKEN  *ppToken - Original token that start with. If returns TRUE then
            this may have been modified in pace
returns
   PMIFLTOKEN - New pCur if was a parenthisis/bracket and changed, NULL if no change
*/
PMIFLTOKEN CMIFLCompiled::TokensParseParen (PMIFLTOKEN pCur, PMIFLTOKEN *ppToken)
{
   BOOL fParen, fLeftOK;
   DWORD dwArgs, dwNonNullArgs;

   if (pCur->dwType == TOKEN_OPER_PARENL)
      fParen = TRUE;
   else if (pCur->dwType == TOKEN_OPER_BRACKETL)
      fParen = FALSE;
   else
      return NULL;

   // see how many args
   dwArgs = dwNonNullArgs = 0;
   PMIFLTOKEN pDown = (PMIFLTOKEN)pCur->pDown;
   if (pDown && (pDown->dwType == TOKEN_OPER_COMMA)) {
      pDown = (PMIFLTOKEN) pDown->pDown;
      for (; pDown; pDown = (PMIFLTOKEN)pDown->pDown) {
         dwArgs++;
         if (pDown->pNext && (((PMIFLTOKEN)pDown->pNext)->dwType != TOKEN_NOP))
            dwNonNullArgs++;
      }
   }
   else if (pDown) {
      dwArgs++;
      if (pDown->dwType != TOKEN_NOP)
         dwNonNullArgs++;
   }

   // can operate on left?
   fLeftOK = FALSE;
   PMIFLTOKEN pMethName = NULL;  // so can guess the method name and hence # params
   PMIFLTOKEN pLeft = (PMIFLTOKEN)pCur->pBack;
   if (pLeft) switch (pLeft->dwType) {
   case TOKEN_STRING:
   case TOKEN_STRINGTABLE:
   case TOKEN_LISTDEF:
      // ok to use this for list, indexing array
      // DOCUMENT: Access string array using []
      fLeftOK = !fParen;
      break;

   case TOKEN_OPER_DOT:
      // either accessing private method/var, so assume true
      fLeftOK = TRUE;
      if (fParen) {
         // keep looking to see if can find a specific name
         PMIFLTOKEN pArg1 = (PMIFLTOKEN)pLeft->pDown;
         PMIFLTOKEN pArg2 = pArg1 ? (PMIFLTOKEN)pArg1->pDown : NULL;
         PMIFLTOKEN pRight = pArg2 ? (PMIFLTOKEN)pArg2->pNext : NULL;
         if (pRight && ((pRight->dwType == TOKEN_METHPUB) || (pRight->dwType == TOKEN_METHPRIV)))
            pMethName = pRight;
      }
      break;

   case TOKEN_PROPPUB:
   case TOKEN_GLOBAL:
   case TOKEN_PROPPRIV:
   case TOKEN_VARIABLE:
   case TOKEN_LISTINDEX:
   case TOKEN_FUNCCALL:
      // either accessing private method/var, so assume true
      fLeftOK = TRUE;
      break;

   case TOKEN_METHPUB:
   case TOKEN_FUNCTION:
   case TOKEN_METHPRIV:
      // could be a function in here
      fLeftOK = fParen;
      pMethName = pLeft;
      break;

      
   // NOTE: Not accepting case TOKEN_OPER_PARENL:
   // NOTE: Not accepting case TOKEN_OPER_BRACKETL:
   } // switch

   // how many args
   DWORD dwArgMin = 0, dwArgMax = 0;
   if (fParen) {
      if (fLeftOK) {
         // left is a function call
         dwArgMax = 1000;  // very large number

         // see if can rustle up the actual method
         PCMIFLMeth pMeth = NULL;
         if (pMethName) switch (pMethName->dwType) {
         case TOKEN_METHPUB:
            {
               PMIFLIDENT pFind = (PMIFLIDENT) m_hUnIdentifiers.Find (pMethName->dwValue);
               if (pFind)
                  pMeth = (PCMIFLMeth) pFind->pEntity;
            }
            break;
         case TOKEN_FUNCTION:
            {
               PCMIFLFunc pFunc = m_pLib->FuncGet (pMethName->dwValue);
               if (pFunc)
                  pMeth = &pFunc->m_Meth;
            }
            break;
         case TOKEN_METHPRIV:
            {
               PMIFLIDENT pFind = (PMIFLIDENT) m_pObject->m_hUnPrivIdentity.Find (pMethName->dwValue);
               if (pFind)
                  pMeth = &((PCMIFLFunc) pFind->pEntity)->m_Meth;
            }
            break;
         }
         
         // if know the method then may have min and max
         if (pMeth && !pMeth->m_fParamAnyNum)
            dwArgMin = dwArgMax = pMeth->m_lPCMIFLProp.Num();
      }
      else { // left is operation
         dwArgMin = dwArgMax = 1;
      }
   }
   else { // bracket
      if (fLeftOK)
         // access an array
         dwArgMin = dwArgMax = 1;
      else
         // List
         dwArgMax = 1000000;
   }


   // if have any null args then error
   if (dwArgs != dwNonNullArgs)
      m_pErr->Add (L"You must include values between all the commas.",
         m_pszErrLink, m_pszMethName, TRUE,
         pCur->dwCharStart, pCur->dwCharEnd);

   // if wrong number of arguements then error
   if ((dwArgs < dwArgMin) || (dwArgs > dwArgMax)) {
      if (dwArgMax == 0)
         m_pErr->Add (L"No parameters are allowed.",
            m_pszErrLink, m_pszMethName, TRUE,
            pCur->dwCharStart, pCur->dwCharEnd);
      else if (dwArgMax == 1)
         m_pErr->Add (L"Only one parameter is allowed.",
            m_pszErrLink, m_pszMethName, TRUE,
            pCur->dwCharStart, pCur->dwCharEnd);
      else {
         MemZero (&gMemTemp);
         MemCat (&gMemTemp, L"Only ");
         MemCat (&gMemTemp, (int) dwArgMax);
         MemCat (&gMemTemp, L" parameters are allowed.");
         m_pErr->Add ((PWSTR)gMemTemp.p,
            m_pszErrLink, m_pszMethName, TRUE,
            pCur->dwCharStart, pCur->dwCharEnd);
      }
   }

   // merge these operations....
   if (fLeftOK) {
      // either make it a function call or access into array
      // detact the left
      if (*ppToken == pLeft)
         *ppToken = pCur;
      pCur->pBack = pLeft->pBack;
      if (pCur->pBack)
         ((PMIFLTOKEN)pCur->pBack)->pNext = pCur;
      pLeft->pBack = pLeft->pNext = NULL;

      // convert pCur to being a function call
      pCur->dwType = fParen ? TOKEN_FUNCCALL : TOKEN_LISTINDEX;

      // put in some new args...
      PMIFLTOKEN pOldDown = (PMIFLTOKEN) pCur->pDown;
      pCur->pDown = NULL;

      PMIFLTOKEN pNew, pDown;
      pDown = pCur;
      pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(*pNew));
      if (pNew) {
         // arg1 contains the function name
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG1;
         pNew->dwCharStart = pLeft->dwCharStart;
         pNew->dwCharEnd = pLeft->dwCharEnd;
         pNew->pNext = pLeft;
         pLeft->pBack = pNew;
         pDown->pDown = pNew;
         pDown = pNew;
      }

      pNew = (PMIFLTOKEN) ESCMALLOC (sizeof(*pNew));
      if (pNew) {
         // arg2 contains the function parameters, which may first be preceded by comma
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG2;
         pNew->dwCharStart = (pOldDown ? pOldDown : pCur)->dwCharStart;
         pNew->dwCharEnd = (pOldDown ? pOldDown : pCur)->dwCharEnd;
         pNew->pNext = pOldDown;
         if (pOldDown)
            pOldDown->pBack = pNew;
         pDown->pDown = pNew;
      }

      pCur->dwCharStart = pOldDown ? min(pOldDown->dwCharStart, pLeft->dwCharStart) : pLeft->dwCharStart;
      pCur->dwCharEnd = pOldDown ? max(pOldDown->dwCharEnd, pLeft->dwCharEnd) : pLeft->dwCharEnd;
   }
   else {
      // either take out parens or turn into list generator
      if (fParen) {
         // end up removing the list and putting in pDown instead

         PMIFLTOKEN pRight = (PMIFLTOKEN)pCur->pNext;
         PMIFLTOKEN pDown = (PMIFLTOKEN)pCur->pDown;
         
         if (pDown) {
            if (*ppToken == pCur)
               *ppToken = pDown;

            if (pLeft)
               pLeft->pNext = pDown;
            if (pRight)
               pRight->pBack = pDown;
            pDown->pBack = pLeft;
            pDown->pNext = pRight;

            // delete pCur
            pCur->pBack = pCur->pNext = pCur->pDown = NULL;
            TokenFree (pCur);

            // since just replaced cur with down, the current one is down
            pCur = pDown;
         }
         else {
            // no down, so just convert this to a NOP to be easy
            pCur->dwType = TOKEN_NOP;
         }
      }
      else { // brackets
         // just convert the type
         pCur->dwType = TOKEN_LISTDEF;
      }
   }

   return pCur;
}

/*****************************************************************************************
CMIFLCompiled::TokensParseOrderOfOper - This is called by TokensParseExpression() and
remaps the data so it handles the proper order of operations. It assumes that there are no
commans or semi-colons in the expression.

inputs
   PMIFLTOKEN  pToken - Token to start with
returns
   PMIFLTOKEN - New starting token. May be different than pToken
*/
PMIFLTOKEN CMIFLCompiled::TokensParseOrderOfOper (PMIFLTOKEN pToken)
{
   if (!pToken)
      return NULL;   // no token

   // loop through looking for parens and [], parsing within them
   PMIFLTOKEN pCur;
   for (pCur = pToken; pCur; pCur = (PMIFLTOKEN)pCur->pNext) {
      if (!pCur->pDown || ((pCur->dwType != TOKEN_OPER_PARENL) && (pCur->dwType != TOKEN_OPER_BRACKETL)))
         continue;

      PMIFLTOKEN pNext, pExp;
      BOOL fFoundSemi;
      pNext = TokensParseExpression ((PMIFLTOKEN) pCur->pDown, &pExp, &fFoundSemi);
      pCur->pDown = pExp;

      if (fFoundSemi)
         m_pErr->Add (L"Semicolon unexpected.", m_pszErrLink, m_pszMethName, TRUE,
            pExp ? pExp->dwCharStart : pCur->dwCharStart,
            pExp ? pExp->dwCharEnd : pCur->dwCharEnd);

      if (pNext) {
         m_pErr->Add (L"Unexpected tokens in expression.", m_pszErrLink, m_pszMethName, TRUE,
            pNext->dwCharStart, pNext->dwCharEnd);
         TokenFree (pNext);
      }

   } // pCur


   // DOCUMENT: Order of operations

   // parse the . operator
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_DOT, TOKEN_OPER_DOT, TRUE, 0, FALSE, TRUE);
      // NOTE: this will end up parsing parenthesis and brackets too

   // go back and look at parents
   // BUGFIX - Incorporate into Tokens ParseOrderOfOperByRange... pToken = TokensParseOrderOfOperParen (pToken);

   // do ++ and --
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_PLUSPLUS, TOKEN_OPER_MINUSMINUS, TRUE, -2);

   // do -, ~, new, and !
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_NEGATION, TOKEN_OPER_NEW, FALSE, 1);

   // do *, /, and %
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_MULTIPLY, TOKEN_OPER_MODULO, TRUE, 0);

   // do + and -
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_ADD, TOKEN_OPER_SUBTRACT, TRUE, 0);

   // do << and >>
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_BITWISELEFT, TOKEN_OPER_BITWISERIGHT, TRUE, 0);

   // do <, <=, <, >=
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_LESSTHAN, TOKEN_OPER_GREATERTHANEQUAL, TRUE, 0);

   // do ==, !=, ===, !===
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_EQUALITY, TOKEN_OPER_NOTEQUALSTRICT, TRUE, 0);

   // do &
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_BITWISEAND, TOKEN_OPER_BITWISEAND, TRUE, 0);

   // do ^
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_BITWISEXOR, TOKEN_OPER_BITWISEXOR, TRUE, 0);

   // do |
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_BITWISEOR, TOKEN_OPER_BITWISEOR, TRUE, 0);

   // do &&
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_LOGICALAND, TOKEN_OPER_LOGICALAND, TRUE, 0);

   // do ||
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_LOGICALOR, TOKEN_OPER_LOGICALOR, TRUE, 0);

   // do ? :
   pToken = TokensParseOrderOfOperConditional (pToken);

   // do assignments, =, +=, -=, *=, /=, %=, <<=, >>=, &=, ^=, |=
   pToken = TokensParseOrderOfOperByRange (pToken, TOKEN_OPER_ASSIGN, TOKEN_OPER_ASSIGNBITWISEOR,
      FALSE, 0, TRUE);

   // if have a pToken->pNext then there's an error somewhere since this should never happen
   if (pToken->pNext) {
      PMIFLTOKEN pDel = (PMIFLTOKEN)pToken->pNext;
      pToken->pNext = NULL;
      pDel->pBack = NULL;

      DWORD dwStart, dwEnd;
      TokenFindRange (pDel, &dwStart, &dwEnd);

      m_pErr->Add (L"Unexpected elements in the expression.", m_pszErrLink, m_pszMethName,
         TRUE, dwStart, dwEnd);
      TokenFree (pDel);
   }

   // if the token isn't one that can be connected to then error
   if (!CanConnectToOper (pToken, FALSE))
      m_pErr->Add (L"Bad expression.", m_pszErrLink, m_pszMethName,
         TRUE, pToken->dwCharStart, pToken->dwCharEnd);


   return pToken;
}

/*****************************************************************************************
CMIFLCompiled::TokensParseExpression - This starts at the given token and tried to find the next expression.
An expression is something that goes to the next semi-colon (inclusive), or the end of
the linked list.

It can also be separated by commas, such as is common in for loops.
*ppParse could also be filled with a NOP if it's just a semicolon expression.

inputs
   PMIFLTOKEN  pToken - Token to start with
   PMIFLTOKEN  *ppParse - Token that represents the parsed expression. This will be one
               token with down elements. If commas are used in the expression
               then this will have a TOKEN_OPER_COMMA, with down's being all TOKEN_OPER_ARG1.
               Following each ARG1 is the parse from within the commas.
   BOOL        *pfFoundSemi - Filled with TRUE if found a semi-colon ending the expression
returns
   PMIFLTOKEN - Returns the token that is the next statement/expression to be parsed, or NULL if no more
*/
PMIFLTOKEN CMIFLCompiled::TokensParseExpression (PMIFLTOKEN pToken, PMIFLTOKEN *ppParse, BOOL *pfFoundSemi)
{
   PMIFLTOKEN pNext = pToken;
   PMIFLTOKEN pNew;
   *pfFoundSemi = FALSE;
   *ppParse = NULL;

   // look for a semicolon
   PMIFLTOKEN pCur;
   BOOL fFoundComma = FALSE;
   for (pCur = pToken; pCur; pCur = (PMIFLTOKEN) pCur->pNext) {
      if (pCur->dwType == TOKEN_OPER_COMMA)
         fFoundComma = TRUE;

      if (pCur->dwType == TOKEN_OPER_SEMICOLON)
         break;
   }

   if (!pCur)
      // whole thing is an expression
      pNext = NULL;
   else {
      // found a semi-colon
      *pfFoundSemi = TRUE;

      // only part is an expression
      pNext = (PMIFLTOKEN) pCur->pNext;
      if (pNext)
         pNext->pBack = NULL;

      // sever ties in pCur and delete
      if (pCur->pBack)
         ((PMIFLTOKEN)pCur->pBack)->pNext = NULL;
      pCur->pNext = NULL;
      pCur->pBack = NULL;
      TokenFree (pCur);
   }

   // if pCur is the current token then create a NOP
   if (pCur == pToken) {
      // make a NOP statement
      pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
      if (!pNew)
         return pNext; // err
      memset (pNew, 0, sizeof(*pNew));
      pNew->dwType = TOKEN_NOP;
      pNew->dwCharStart = pToken->dwCharStart;
      pNew->dwCharEnd = pToken->dwCharEnd;
      *ppParse = pNew;

      pToken = pNew;
   }
   else
      *ppParse = pToken;

   // if found any commas then do special case
   if (fFoundComma) {
      // start out with an initial comma
      pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
      if (!pNew)
         return pNext; // err
      memset (pNew, 0, sizeof(*pNew));
      pNew->dwType = TOKEN_OPER_COMMA;
      TokenFindRange (pToken, &pNew->dwCharStart, &pNew->dwCharEnd);
      *ppParse = pNew;
      PMIFLTOKEN pDown = pNew;

      // repeat to find the first comma...
      for (pCur = pToken; pCur; pCur = (PMIFLTOKEN)pCur->pNext)
         if (pCur->dwType == TOKEN_OPER_COMMA)
            break;

      // separate pCur from pToken, and add to pnew
      if (pCur->pBack)
         ((PMIFLTOKEN)pCur->pBack)->pNext = NULL;
      pCur->pBack = NULL;
      if (pCur == pToken)
         pToken = NULL;
      if (pToken)
         pToken->pBack = NULL;

      // put under arg1
      pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
      if (pNew) {
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG1;
         if (pToken)
            TokenFindRange (pToken, &pNew->dwCharStart, &pNew->dwCharEnd);
         else {
            pNew->dwCharStart = pNew->dwCharEnd = pCur->dwCharStart;
         }

         // resolve tokens
         if (pToken)
            TokensIdentResolve (pToken);

         pNew->pNext = TokensParseOrderOfOper(pToken);
         if (pNew->pNext)
            ((PMIFLTOKEN)pNew->pNext)->pBack = pNew;

         // move down pointer
         pDown->pDown = pNew;
         pDown = pNew;
      }

      // now, loop through starting with commas, until find next comma
      while (pCur) {
         pToken = pCur;
         for (pCur = (PMIFLTOKEN)pToken->pNext; pCur; pCur = (PMIFLTOKEN)pCur->pNext)
            if (pCur->dwType == TOKEN_OPER_COMMA)
               break;

         // separate pCur from pToken, and add to the tail..
         if (pCur) {
            if (pCur->pBack)
               ((PMIFLTOKEN)pCur->pBack)->pNext = NULL;
            pCur->pBack = NULL;
         }

         // change the comm to an arg1
         pToken->dwType = TOKEN_ARG1;
         TokenFindRange (pToken, &pToken->dwCharStart, &pToken->dwCharEnd);

         // move down pointer
         pDown->pDown = pToken;
         pDown = pToken;

         // temporarily cutout
         pNew = (PMIFLTOKEN) pToken->pNext;
         pToken->pNext = NULL;
         if (pNew)
            pNew->pBack = NULL;

         // clean up
         if (pNew)
            TokensIdentResolve (pNew); // BUGFIX - Was pToken

         pToken->pNext = TokensParseOrderOfOper(pNew);
         if (pToken->pNext)
            ((PMIFLTOKEN)pToken->pNext)->pBack = pToken;
      }
   }
   else {
      // resolve tokens
      if (*ppParse)
         TokensIdentResolve (*ppParse);

      *ppParse = TokensParseOrderOfOper (*ppParse);
   }


   return pNext;
}


/*****************************************************************************************
TokensParseStatement - This pulls an individual statement starting from the beginning
token.

inputs
   PMIFLTOKEN  pToken - Token to start with
   PMIFLTOKEN  *ppParse - Token that represents the parsed statement. This MUST
               be a single token with down elements, but no right elements.
   DWORD       dwFlags - Form of TPS_XXX
returns
   PMIFLTOKEN - Returns the token that is the next statement to be parsed, or NULL if no more
*/
PMIFLTOKEN CMIFLCompiled::TokensParseStatement (PMIFLTOKEN pToken, PMIFLTOKEN *ppParse, DWORD dwFlags)
{
   *ppParse = NULL;  // just in case
   PMIFLTOKEN pNew;
   PMIFLTOKEN pNext = pToken;

   switch (pToken->dwType) {
   case TOKEN_OPER_SEMICOLON: // empty statement
      // next statement
      pNext = (PMIFLTOKEN)pToken->pNext;

      // make a NOP statement
      pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
      if (!pNew)
         return pNext; // err
      memset (pNew, 0, sizeof(*pNew));
      pNew->dwType = TOKEN_NOP;
      pNew->dwCharStart = pToken->dwCharStart;
      pNew->dwCharEnd = pToken->dwCharEnd;
      *ppParse = pNew;

      // deletete this
      pToken->pNext = NULL;
      TokenFree (pToken);
      break;

   
   case TOKEN_OPER_BRACEL: // code block
      // NOTE: Know that if have a brace will only have on level of down

      // next statement
      pNext = (PMIFLTOKEN)pToken->pNext;
      pToken->pNext = NULL;

      // recurse and parse code-block on down
      if (pToken->pDown) {
         pToken->pDown = TokensParseStatements ((PMIFLTOKEN)pToken->pDown, dwFlags | TPS_INBLOCK);
         *ppParse = pToken;
      }
      // BUGFIX - Put NOP back in because was memory leak otherwise
      // else, empty brace, but just leave it since wont do anything
      else {
         // make a NOP statement since had empty brackets
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (!pNew)
            return pNext; // err
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_NOP;
         pNew->dwCharStart = pToken->dwCharStart;
         pNew->dwCharEnd = pToken->dwCharEnd;
         *ppParse = pNew;

         // delete this
         pToken->pNext = NULL;
         pToken->pDown = NULL;
         TokenFree (pToken);
      }


      break;

   case TOKEN_RETURN:
      if (pToken->pNext) {
         pNext = (PMIFLTOKEN)pToken->pNext;
         pToken->pNext = NULL;
         *ppParse = pToken;
         if (pNext)
            pNext->pBack = NULL;

         // BUGFIX: if next is a semicolon then just ext
         if (pNext->dwType == TOKEN_OPER_SEMICOLON) {
            pToken = pNext;
            pNext = (PMIFLTOKEN)pNext->pNext;
            pToken->pNext = NULL;
            if (pNext)
               pNext->pBack = NULL;
            TokenFree (pToken);
            break;
         }

         // add an arguement to return
         // make a NOP statement since had empty brackets
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (!pNew)
            return pNext; // err
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG1;

         PMIFLTOKEN pExp;
         BOOL fFoundSemi;
         pNext = TokensParseExpression (pNext, &pExp, &fFoundSemi);

         pNew->pNext = pExp;
         if (pExp)
            pExp->pBack = pNew;
         pToken->pDown = pNew;
         
         // find range
         TokenFindRange (pExp, &pNew->dwCharStart, &pNew->dwCharEnd);
         pToken->dwCharEnd = max(pToken->dwCharEnd, pNew->dwCharEnd);

         // if didn't find semicolon then error
         if (!fFoundSemi)
            m_pErr->Add (L"Semi-colon expected after return.", m_pszErrLink, m_pszMethName, TRUE,
               pToken->dwCharStart, pToken->dwCharEnd);
      }
      else {
         // error
         m_pErr->Add (L"Semi-colon expected after return.", m_pszErrLink, m_pszMethName, TRUE,
            pToken->dwCharStart, pToken->dwCharEnd);
         
         *ppParse = pToken;
         pNext = (PMIFLTOKEN) pToken->pNext; // which will be NULL
         pToken->pNext = NULL;
      }
      break;

   case TOKEN_BREAK:
      if (!(dwFlags & (TPS_INLOOP | TPS_INSWITCH)))
         m_pErr->Add (L"Break is not valid outside a for-loop, while-loop, or switch statement.", m_pszErrLink, m_pszMethName, TRUE,
            pToken->dwCharStart, pToken->dwCharEnd);
      if (!pToken->pNext) {
         m_pErr->Add (L"A semicolon is expected after break.", m_pszErrLink, m_pszMethName, TRUE,
            pToken->dwCharStart, pToken->dwCharEnd);

         *ppParse = pToken;
         pNext = (PMIFLTOKEN) pToken->pNext; // which will be NULL
         pToken->pNext = NULL;
      }
      else {
         PMIFLTOKEN pSemi = (PMIFLTOKEN)pToken->pNext;
         if (pSemi->dwType == TOKEN_OPER_SEMICOLON) {
            // found a semicolon as expected
            *ppParse = pToken;
            pToken->pNext = NULL;

            pSemi->pBack = NULL;
            pNext = (PMIFLTOKEN) pSemi->pNext;
            pSemi->pNext = NULL;
            TokenFree (pSemi);
         }
         else {
            // not a semicolon, so error
            m_pErr->Add (L"A semicolon is expected after break.", m_pszErrLink, m_pszMethName, TRUE,
               pToken->dwCharStart, pToken->dwCharEnd);

            *ppParse = pToken;
            pToken->pNext = NULL;
            pSemi->pBack = NULL;
            pNext = pSemi;
         }
      }
      break;

   case TOKEN_CONTINUE:
      if (!(dwFlags & TPS_INLOOP))
         m_pErr->Add (L"Continue is not valid outside a for-loop or while-loop.", m_pszErrLink, m_pszMethName, TRUE,
            pToken->dwCharStart, pToken->dwCharEnd);
      if (!pToken->pNext) {
         m_pErr->Add (L"A semicolon is expected after continue.", m_pszErrLink, m_pszMethName, TRUE,
            pToken->dwCharStart, pToken->dwCharEnd);

         *ppParse = pToken;
         pNext = (PMIFLTOKEN) pToken->pNext; // which will be NULL
         pToken->pNext = NULL;
      }
      else {
         PMIFLTOKEN pSemi = (PMIFLTOKEN)pToken->pNext;
         if (pSemi->dwType == TOKEN_OPER_SEMICOLON) {
            // found a semicolon as expected
            *ppParse = pToken;
            pToken->pNext = NULL;

            pSemi->pBack = NULL;
            pNext = (PMIFLTOKEN) pSemi->pNext;
            pSemi->pNext = NULL;
            TokenFree (pSemi);
         }
         else {
            // not a semicolon, so error
            m_pErr->Add (L"A semicolon is expected after continue.", m_pszErrLink, m_pszMethName, TRUE,
               pToken->dwCharStart, pToken->dwCharEnd);

            *ppParse = pToken;
            pToken->pNext = NULL;
            pSemi->pBack = NULL;
            pNext = pSemi;
         }
      }
      break;


   case TOKEN_DEBUG:
      if (!pToken->pNext) {
         m_pErr->Add (L"A semicolon is expected after \"debug\".", m_pszErrLink, m_pszMethName, TRUE,
            pToken->dwCharStart, pToken->dwCharEnd);

         *ppParse = pToken;
         pNext = (PMIFLTOKEN) pToken->pNext; // which will be NULL
         pToken->pNext = NULL;
      }
      else {
         PMIFLTOKEN pSemi = (PMIFLTOKEN)pToken->pNext;
         if (pSemi->dwType == TOKEN_OPER_SEMICOLON) {
            // found a semicolon as expected
            *ppParse = pToken;
            pToken->pNext = NULL;

            pSemi->pBack = NULL;
            pNext = (PMIFLTOKEN) pSemi->pNext;
            pSemi->pNext = NULL;
            TokenFree (pSemi);
         }
         else {
            // not a semicolon, so error
            m_pErr->Add (L"A semicolon is expected after \"debug\".", m_pszErrLink, m_pszMethName, TRUE,
               pToken->dwCharStart, pToken->dwCharEnd);

            *ppParse = pToken;
            pToken->pNext = NULL;
            pSemi->pBack = NULL;
            pNext = pSemi;
         }
      }
      break;


   case TOKEN_DELETE:
      if (pToken->pNext) {
         pNext = (PMIFLTOKEN)pToken->pNext;
         pToken->pNext = NULL;
         pNext->pBack = NULL; // BUGFIX - make sure back is 0
         *ppParse = pToken;

         // add an arguement to delete
         // make a NOP statement since had empty brackets
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (!pNew)
            return pNext; // err
         memset (pNew, 0, sizeof(*pNew));
         pNew->dwType = TOKEN_ARG1;

         PMIFLTOKEN pExp;
         BOOL fFoundSemi;
         pNext = TokensParseExpression (pNext, &pExp, &fFoundSemi);

         pNew->pNext = pExp;
         if (pExp)
            pExp->pBack = pNew;
         pToken->pDown = pNew;
         
         // find range
         TokenFindRange (pExp, &pNew->dwCharStart, &pNew->dwCharEnd);
         pToken->dwCharEnd = max(pToken->dwCharEnd, pNew->dwCharEnd);

         // if didn't find semicolon then error
         if (!fFoundSemi)
            m_pErr->Add (L"A semicolon expected after delete.", m_pszErrLink, m_pszMethName, TRUE,
               pToken->dwCharStart, pToken->dwCharEnd);

         if ((pExp == NULL) || (pExp->dwType == TOKEN_NOP))
            m_pErr->Add (L"Delete requires an object to be deleted.", m_pszErrLink, m_pszMethName, TRUE,
               pToken->dwCharStart, pToken->dwCharEnd);
      }
      else {
         // error
         m_pErr->Add (L"Delete requires an object to be deleted.", m_pszErrLink, m_pszMethName, TRUE,
            pToken->dwCharStart, pToken->dwCharEnd);
         
         *ppParse = pToken;
         pNext = (PMIFLTOKEN) pToken->pNext; // which will be NULL
         pToken->pNext = NULL;
      }
      break;

   case TOKEN_FOR:
      {
         // isolate the for and use it
         PMIFLTOKEN pCur, pFor;
         *ppParse = pFor = pToken;
         pCur = (PMIFLTOKEN)pToken->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;


         // require parens...
         if (!pCur || (pCur->dwType != TOKEN_OPER_PARENL) || !pCur->pDown) {
            m_pErr->Add (L"A for-loop needs () after it.", m_pszErrLink, m_pszMethName, TRUE,
               pFor->dwCharStart, pFor->dwCharEnd);
            break;
         }
         PMIFLTOKEN pParen = (PMIFLTOKEN) pCur->pDown;
         pCur->pDown = NULL;
         pToken = pCur;
         pCur = (PMIFLTOKEN)pCur->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;
         TokenFree (pToken);  // free the parens...

         // look for 3 semicolons
         DWORD i;
         PMIFLTOKEN pDown;
         for (i = 0; i < 3; i++) {
            if ((i < 2) && !pParen) {
               m_pErr->Add (L"Not enough parameters for the for (;;) loop", m_pszErrLink, m_pszMethName, TRUE,
                  pFor->dwCharStart, pFor->dwCharEnd);
               break;
            }

            PMIFLTOKEN pArg;
            BOOL fFoundSemi;
            if (pParen)
               pParen = TokensParseExpression (pParen, &pArg, &fFoundSemi);
            else {
               pArg = NULL;   // nothing
               fFoundSemi = FALSE;
            }
            if (!fFoundSemi && (i < 2))
               m_pErr->Add (L"Not enough parameters for the for (;;) loop.", m_pszErrLink, m_pszMethName, TRUE,
                  pFor->dwCharStart, pFor->dwCharEnd);
            else if (fFoundSemi && (i == 2))
               m_pErr->Add (L"Too many parameters for the for (;;) loop.", m_pszErrLink, m_pszMethName, TRUE,
                  pFor->dwCharStart, pFor->dwCharEnd);

            // add the pArg to an arguement list
            for (pDown = pFor; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
            pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
            if (pNew) {
               memset (pNew, 0, sizeof(*pNew));
               pNew->dwType = TOKEN_ARG1 + i;   // NOTE: Assumes _ARG2 and _ARG3 are seqyential
               pNew->pNext = pArg;
               if (pArg)
                  pArg->pBack = pNew;
               if (pArg)
                  TokenFindRange (pArg, &pNew->dwCharStart, &pNew->dwCharEnd);
               else {
                  pNew->dwCharStart = pFor->dwCharStart;
                  pNew->dwCharEnd = pFor->dwCharEnd;
               }
               pDown->pDown = pNew; // so add argyement
            }  // if pNew
         } // i
         if (pParen) {
            DWORD dwStart, dwEnd;
            TokenFindRange (pParen, &dwStart, &dwEnd);
            m_pErr->Add (L"Too many parameters for the for (;;) loop", m_pszErrLink, m_pszMethName, TRUE,
               dwStart, dwEnd);
            TokenFree (pParen);
         }

         // get the statement of the for loop
         if (!pCur) {
            m_pErr->Add (L"For-loop is missing a body.", m_pszErrLink, m_pszMethName, TRUE,
               pFor->dwCharStart, pFor->dwCharEnd);
            break;
         }

         PMIFLTOKEN pBody;
         pNext = TokensParseStatement (pNext, &pBody, dwFlags | TPS_INLOOP | TPS_INBLOCK);
         for (pDown = pFor; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG4;
            pNew->pNext = pBody;
            if (pBody)
               pBody->pBack = pNew;
            if (pBody)
               TokenFindRange (pBody, &pNew->dwCharStart, &pNew->dwCharEnd);
            else {
               pNew->dwCharStart = pFor->dwCharStart;
               pNew->dwCharEnd = pFor->dwCharEnd;
            }
            pDown->pDown = pNew; // so add argyement
         }  // if pNew

         TokenFindRange (pFor, &pFor->dwCharStart, &pFor->dwCharEnd);
      }
      break;

   case TOKEN_WHILE:
      {
         // isolate the while and use it
         PMIFLTOKEN pCur, pWhile;
         *ppParse = pWhile = pToken;
         pCur = (PMIFLTOKEN)pToken->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;

         // require parens...
         if (!pCur || (pCur->dwType != TOKEN_OPER_PARENL) || !pCur->pDown) {
            m_pErr->Add (L"A while-loop needs () after it.", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
            break;
         }
         PMIFLTOKEN pParen = (PMIFLTOKEN) pCur->pDown;
         pCur->pDown = NULL;
         pToken = pCur;
         pCur = (PMIFLTOKEN)pCur->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;
         TokenFree (pToken);  // free the parens...

         // look for one expression
         PMIFLTOKEN pDown;
         if (!pParen) {
            m_pErr->Add (L"Not enough parameters for the while() loop", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
         }
         else {
            PMIFLTOKEN pArg;
            BOOL fFoundSemi;
            pParen = TokensParseExpression (pParen, &pArg, &fFoundSemi);

            if (fFoundSemi || pParen) {
               m_pErr->Add (L"Too many parameters for the while() loop.", m_pszErrLink, m_pszMethName, TRUE,
                  pWhile->dwCharStart, pWhile->dwCharEnd);
               if (pParen)
                  TokenFree (pParen);
            }

            // add the pArg to an arguement list
            for (pDown = pWhile; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
            pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
            if (pNew) {
               memset (pNew, 0, sizeof(*pNew));
               pNew->dwType = TOKEN_ARG1;
               pNew->pNext = pArg;
               if (pArg)
                  pArg->pBack = pNew;
               if (pArg)
                  TokenFindRange (pArg, &pNew->dwCharStart, &pNew->dwCharEnd);
               else {
                  pNew->dwCharStart = pWhile->dwCharStart;
                  pNew->dwCharEnd = pWhile->dwCharEnd;
               }
               pDown->pDown = pNew; // so add argyement
            }  // if pNew
         } // i

         // get the statement of the While loop
         if (!pCur) {
            m_pErr->Add (L"While-loop is missing a body.", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
            break;
         }

         PMIFLTOKEN pBody;
         pNext = TokensParseStatement (pNext, &pBody, dwFlags | TPS_INLOOP | TPS_INBLOCK);
         for (pDown = pWhile; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG2;
            pNew->pNext = pBody;
            if (pBody)
               pBody->pBack = pNew;
            if (pBody)
               TokenFindRange (pBody, &pNew->dwCharStart, &pNew->dwCharEnd);
            else {
               pNew->dwCharStart = pWhile->dwCharStart;
               pNew->dwCharEnd = pWhile->dwCharEnd;
            }
            pDown->pDown = pNew; // so add argyement
         }  // if pNew

         TokenFindRange (pWhile, &pWhile->dwCharStart, &pWhile->dwCharEnd);
      }
      break;


   case TOKEN_DO:
      {
         // isolate the while and use it
         PMIFLTOKEN pCur, pWhile;
         *ppParse = pWhile = pToken;
         pCur = (PMIFLTOKEN)pToken->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;

         // get the statement of the While loop
         if (!pCur) {
            m_pErr->Add (L"Do-while-loop is missing a body.", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
            break;
         }

         PMIFLTOKEN pBody;
         pNext = TokensParseStatement (pNext, &pBody, dwFlags | TPS_INLOOP | TPS_INBLOCK);
         PMIFLTOKEN pDown;
         for (pDown = pWhile; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG1;
            pNew->pNext = pBody;
            if (pBody)
               pBody->pBack = pNew;
            if (pBody)
               TokenFindRange (pBody, &pNew->dwCharStart, &pNew->dwCharEnd);
            else {
               pNew->dwCharStart = pWhile->dwCharStart;
               pNew->dwCharEnd = pWhile->dwCharEnd;
            }
            pDown->pDown = pNew; // so add argyement
         }  // if pNew


         // make sure have a while
         pCur = pNext;
         if (!pCur || (pCur->dwType != TOKEN_WHILE)) {
            m_pErr->Add (L"A while is expected to complete the \"do {} while ()\" loop.", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
            break;
         }

         // delete the while
         pToken = pCur;
         pNext = (PMIFLTOKEN)pCur->pNext;
         pToken->pNext = NULL;
         if (pNext)
            pNext->pBack = NULL;
         TokenFree (pToken);

         // require parens...
         pCur = pNext;
         if (!pCur || (pCur->dwType != TOKEN_OPER_PARENL) || !pCur->pDown) {
            m_pErr->Add (L"A do-while-loop needs () after it.", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
            break;
         }
         PMIFLTOKEN pParen = (PMIFLTOKEN) pCur->pDown;
         pCur->pDown = NULL;
         pToken = pCur;
         pCur = (PMIFLTOKEN)pCur->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;
         TokenFree (pToken);  // free the parens...

         // look for one expression
         if (!pParen) {
            m_pErr->Add (L"Not enough parameters for the while() loop", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
         }
         else {
            PMIFLTOKEN pArg;
            BOOL fFoundSemi;
            pParen = TokensParseExpression (pParen, &pArg, &fFoundSemi);

            if (fFoundSemi || pParen) {
               m_pErr->Add (L"Too many parameters for the while() loop.", m_pszErrLink, m_pszMethName, TRUE,
                  pWhile->dwCharStart, pWhile->dwCharEnd);
               if (pParen)
                  TokenFree (pParen);
            }

            // add the pArg to an arguement list
            for (pDown = pWhile; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
            pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
            if (pNew) {
               memset (pNew, 0, sizeof(*pNew));
               pNew->dwType = TOKEN_ARG2;
               pNew->pNext = pArg;
               if (pArg)
                  pArg->pBack = pNew;
               if (pArg)
                  TokenFindRange (pArg, &pNew->dwCharStart, &pNew->dwCharEnd);
               else {
                  pNew->dwCharStart = pWhile->dwCharStart;
                  pNew->dwCharEnd = pWhile->dwCharEnd;
               }
               pDown->pDown = pNew; // so add argyement
            }  // if pNew
         } // i

         TokenFindRange (pWhile, &pWhile->dwCharStart, &pWhile->dwCharEnd);

         // terminating semi colon
         if (!pNext || (pNext->dwType != TOKEN_OPER_SEMICOLON)) {
            m_pErr->Add (L"Semicolon expected at the end of the do-while() loop.", m_pszErrLink, m_pszMethName, TRUE,
               pWhile->dwCharStart, pWhile->dwCharEnd);
         }
         else {
            // remove semi
            pToken = pNext;
            pNext = (PMIFLTOKEN)pCur->pNext;
            pToken->pNext = NULL;
            if (pNext)
               pNext->pBack = NULL;
            TokenFree (pToken);
         }

      }
      break;

   case TOKEN_IF:
      {
         // isolate the if-loop and use it
         PMIFLTOKEN pCur, pIf;
         *ppParse = pIf = pToken;
         pCur = (PMIFLTOKEN)pToken->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;

         // require parens...
         if (!pCur || (pCur->dwType != TOKEN_OPER_PARENL) || !pCur->pDown) {
            m_pErr->Add (L"A if statement needs a () after it.", m_pszErrLink, m_pszMethName, TRUE,
               pIf->dwCharStart, pIf->dwCharEnd);
            break;
         }
         PMIFLTOKEN pParen = (PMIFLTOKEN) pCur->pDown;
         pCur->pDown = NULL;
         pToken = pCur;
         pCur = (PMIFLTOKEN)pCur->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;
         TokenFree (pToken);  // free the parens...

         // look for one expression
         PMIFLTOKEN pDown;
         if (!pParen) {
            m_pErr->Add (L"Not enough parameters for the if() statement.", m_pszErrLink, m_pszMethName, TRUE,
               pIf->dwCharStart, pIf->dwCharEnd);
         }
         else {
            PMIFLTOKEN pArg;
            BOOL fFoundSemi;
            pParen = TokensParseExpression (pParen, &pArg, &fFoundSemi);

            if (fFoundSemi || pParen) {
               m_pErr->Add (L"Too many parameters for the if() statement.", m_pszErrLink, m_pszMethName, TRUE,
                  pIf->dwCharStart, pIf->dwCharEnd);
               if (pParen)
                  TokenFree (pParen);
            }

            // add the pArg to an arguement list
            for (pDown = pIf; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
            pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
            if (pNew) {
               memset (pNew, 0, sizeof(*pNew));
               pNew->dwType = TOKEN_ARG1;
               pNew->pNext = pArg;
               if (pArg)
                  pArg->pBack = pNew;
               if (pArg)
                  TokenFindRange (pArg, &pNew->dwCharStart, &pNew->dwCharEnd);
               else {
                  pNew->dwCharStart = pIf->dwCharStart;
                  pNew->dwCharEnd = pIf->dwCharEnd;
               }
               pDown->pDown = pNew; // so add argyement
            }  // if pNew
         } // i

         // get the statement of the If loop
         if (!pCur) {
            m_pErr->Add (L"The if() statement is missing a body.", m_pszErrLink, m_pszMethName, TRUE,
               pIf->dwCharStart, pIf->dwCharEnd);
            break;
         }

         PMIFLTOKEN pBody;
         pNext = TokensParseStatement (pNext, &pBody, dwFlags | TPS_INBLOCK);
         for (pDown = pIf; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
         pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
         if (pNew) {
            memset (pNew, 0, sizeof(*pNew));
            pNew->dwType = TOKEN_ARG2;
            pNew->pNext = pBody;
            if (pBody)
               pBody->pBack = pNew;
            if (pBody)
               TokenFindRange (pBody, &pNew->dwCharStart, &pNew->dwCharEnd);
            else {
               pNew->dwCharStart = pIf->dwCharStart;
               pNew->dwCharEnd = pIf->dwCharEnd;
            }
            pDown->pDown = pNew; // so add argyement
         }  // if pNew

         // see if there's an else
         if (pNext && (pNext->dwType == TOKEN_ELSE)) {
            // remove the else
            DWORD dwStart = pNext->dwCharStart;
            DWORD dwEnd = pNext->dwCharEnd;
            pToken = pNext;
            pNext = (PMIFLTOKEN)pNext->pNext;
            pToken->pNext = NULL;
            if (pNext)
               pNext->pBack = NULL;
            TokenFree (pToken);

            if (pNext) {
               pNext = TokensParseStatement (pNext, &pBody, dwFlags | TPS_INBLOCK);

               for (pDown = pIf; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
               pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
               if (pNew) {
                  memset (pNew, 0, sizeof(*pNew));
                  pNew->dwType = TOKEN_ARG3;
                  pNew->pNext = pBody;
                  if (pBody)
                     pBody->pBack = pNew;
                  if (pBody)
                     TokenFindRange (pBody, &pNew->dwCharStart, &pNew->dwCharEnd);
                  else {
                     pNew->dwCharStart = pIf->dwCharStart;
                     pNew->dwCharEnd = pIf->dwCharEnd;
                  }
                  pDown->pDown = pNew; // so add argyement
               }
            } // if pNext
            else {
               // nothing after else
               m_pErr->Add (L"Statement expected after the \"else\".", m_pszErrLink, m_pszMethName, TRUE,
                  dwStart, dwEnd);
            }
         }

         TokenFindRange (pIf, &pIf->dwCharStart, &pIf->dwCharEnd);
      }
      break;



   case TOKEN_SWITCH:
      {
         // isolate the Switch and use it
         PMIFLTOKEN pCur, pSwitch;
         *ppParse = pSwitch = pToken;
         pCur = (PMIFLTOKEN)pToken->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;

         // require parens...
         if (!pCur || (pCur->dwType != TOKEN_OPER_PARENL) || !pCur->pDown) {
            m_pErr->Add (L"A switch statement needs () after it.", m_pszErrLink, m_pszMethName, TRUE,
               pSwitch->dwCharStart, pSwitch->dwCharEnd);
            break;
         }
         PMIFLTOKEN pParen = (PMIFLTOKEN) pCur->pDown;
         pCur->pDown = NULL;
         pToken = pCur;
         pCur = (PMIFLTOKEN)pCur->pNext;
         pToken->pNext = NULL;
         if (pCur)
            pCur->pBack = NULL;
         pNext = pCur;
         TokenFree (pToken);  // free the parens...

         // look for one expression
         PMIFLTOKEN pDown;
         if (!pParen) {
            m_pErr->Add (L"Not enough parameters for the switch() statement.", m_pszErrLink, m_pszMethName, TRUE,
               pSwitch->dwCharStart, pSwitch->dwCharEnd);
         }
         else {
            PMIFLTOKEN pArg;
            BOOL fFoundSemi;
            pParen = TokensParseExpression (pParen, &pArg, &fFoundSemi);

            if (fFoundSemi || pParen) {
               m_pErr->Add (L"Too many parameters for the switch() statement.", m_pszErrLink, m_pszMethName, TRUE,
                  pSwitch->dwCharStart, pSwitch->dwCharEnd);
               if (pParen)
                  TokenFree (pParen);
            }

            // add the pArg to an arguement list
            for (pDown = pSwitch; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
            pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
            if (pNew) {
               memset (pNew, 0, sizeof(*pNew));
               pNew->dwType = TOKEN_ARG1;
               pNew->pNext = pArg;
               if (pArg)
                  pArg->pBack = pNew;
               if (pArg)
                  TokenFindRange (pArg, &pNew->dwCharStart, &pNew->dwCharEnd);
               else {
                  pNew->dwCharStart = pSwitch->dwCharStart;
                  pNew->dwCharEnd = pSwitch->dwCharEnd;
               }
               pDown->pDown = pNew; // so add argyement
            }  // if pNew
         } // i

         if (!pNext || (pNext->dwType != TOKEN_OPER_BRACEL) || !pNext->pDown) {
            m_pErr->Add (L"Switch statement is missing a body.", m_pszErrLink, m_pszMethName, TRUE,
               pSwitch->dwCharStart, pSwitch->dwCharEnd);
            break;
         }

         // remember the cases and then free the rest
         PMIFLTOKEN pCases;
         pCases = (PMIFLTOKEN)pNext->pDown;
         pNext->pDown = NULL;
         pToken = pNext;
         pNext = (PMIFLTOKEN)pNext->pNext;
         pToken->pNext = NULL;
         if (pNext)
            pNext->pBack = NULL;
         TokenFree (pToken);

         // look through all the cases...
         BOOL fFoundDefault = FALSE;
         BOOL fExpectCase = TRUE;
         while (pCases) {
            BOOL fFoundCase = ((pCases->dwType == TOKEN_CASE) || (pCases->dwType == TOKEN_DEFAULT));
            if (fExpectCase && !fFoundCase) {
               // should have a case or a default
               m_pErr->Add (L"\"case\" or \"default\" expected.", m_pszErrLink, m_pszMethName, TRUE,
                  pCases->dwCharStart, pCases->dwCharEnd);
               TokenFree (pCases);
               break;
            }

            if (fFoundCase) {
               fExpectCase = FALSE; // dont really need a case anymore

               DWORD dwStartCase = pCases->dwCharStart;
               DWORD dwEndCase = pCases->dwCharEnd;

               // remember type and free
               BOOL fDefault = (pCases->dwType == TOKEN_DEFAULT);
               if (fDefault && fFoundDefault) {
                  // should have a case or a default
                  m_pErr->Add (L"\"default\" is already used in the switch statement.", m_pszErrLink, m_pszMethName, TRUE,
                     pCases->dwCharStart, pCases->dwCharEnd);
                  TokenFree (pCases);
                  break;
               }
               fFoundDefault |= fDefault;
               pToken = pCases;
               pCases = (PMIFLTOKEN)pCases->pNext;
               pToken->pNext = NULL;
               if (pCases)
                  pCases->pBack = NULL;
               TokenFree (pToken);

               // find colon...
               PMIFLTOKEN pColon;
               for (pColon = pCases; pColon; pColon = (PMIFLTOKEN)pColon->pNext)
                  if (pColon->dwType == TOKEN_OPER_COLON)
                     break;
               if (!pColon) {
                  // should have a case or a default
                  m_pErr->Add (L"':' expected after case or default.", m_pszErrLink, m_pszMethName, TRUE,
                     pCases ? pCases->dwCharStart : dwStartCase, pCases ? pCases->dwCharEnd : dwEndCase);
               }
               if (fDefault && (pColon != pCases)) {
                  // should have a case or a default
                  m_pErr->Add (L"':' expected immediately after the \"default\".", m_pszErrLink, m_pszMethName, TRUE,
                     pCases->dwCharStart, pCases->dwCharEnd);
                  // continue on and compile
               }
               else if (!fDefault && (pColon == pCases)) {
                  // should have a case or a default
                  m_pErr->Add (L"Expression expected before ':'.", m_pszErrLink, m_pszMethName, TRUE,
                     pColon->dwCharStart, pColon->dwCharEnd);
                  // continue on and compile
               }

               // isolate between the start of the statement and colon, to create an expression
               PMIFLTOKEN pPreExp;
               pPreExp = (pColon == pCases) ? NULL : pCases;
               pCases = pColon ? (PMIFLTOKEN)pColon->pNext : NULL;
               if (pColon) {
                  if (pColon->pBack)
                     ((PMIFLTOKEN)pColon->pBack)->pNext = NULL;
                  pColon->pBack = NULL;
                  pColon->pNext = NULL;
               }
               if (pCases)
                  pCases->pBack = NULL;
               if (pColon)
                  TokenFree (pColon);

               // parse this as expression
               PMIFLTOKEN pExp;
               BOOL fFoundSemi;
               DWORD dwStart, dwEnd;
               if (pPreExp)
                  pPreExp = TokensParseExpression (pPreExp, &pExp, &fFoundSemi);
               else {
                  fFoundSemi = FALSE;
                  pExp = NULL;
               }
               if (fFoundSemi) {
                  TokenFindRange (pExp, &dwStart, &dwEnd);
                  m_pErr->Add (L"Semicolon unexpected in expression.", m_pszErrLink, m_pszMethName, TRUE,
                     dwStart, dwEnd);
               }
               if (pPreExp) {
                  TokenFindRange (pPreExp, &dwStart, &dwEnd);
                  m_pErr->Add (L"Only one expression per case statement.", m_pszErrLink, m_pszMethName, TRUE,
                     dwStart, dwEnd);
                  TokenFree (pPreExp);
               }

               // add an arg2 or arg3 for the expression
               for (pDown = pSwitch; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
               pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
               if (pNew) {
                  memset (pNew, 0, sizeof(*pNew));
                  pNew->dwType = fDefault ? TOKEN_ARG2 : TOKEN_ARG3;
                  pNew->pNext = pExp;
                  if (pExp)
                     pExp->pBack = pNew;
                  if (pExp)
                     TokenFindRange (pExp, &pNew->dwCharStart, &pNew->dwCharEnd);
                  else {
                     pNew->dwCharStart = dwStartCase;
                     pNew->dwCharEnd = dwEndCase;
                  }
                  pDown->pDown = pNew; // so add argyement
               }  // if pNew
            }  // if fFoundCase;
            else {
               // else, block of code...

               // find the next case/default
               PMIFLTOKEN pEnd;
               for (pEnd = pCases; pEnd; pEnd = (PMIFLTOKEN)pEnd->pNext)
                  if ((pEnd->dwType == TOKEN_CASE) || (pEnd->dwType == TOKEN_DEFAULT))
                     break;

               // isolate
               PMIFLTOKEN pBody = pCases;
               pCases = (PMIFLTOKEN) pEnd;
               if (pEnd && pEnd->pBack) {
                  ((PMIFLTOKEN)pEnd->pBack)->pNext = NULL;
                  pEnd->pBack = NULL;
               }

               // parse these statements
               pBody = TokensParseStatements (pBody, dwFlags | TPS_INSWITCH | TPS_INLOOP);

               // add this
               for (pDown = pSwitch; pDown->pDown; pDown = (PMIFLTOKEN)pDown->pDown);
               pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
               if (pNew) {
                  memset (pNew, 0, sizeof(*pNew));
                  pNew->dwType = TOKEN_ARG4; // so know it's a body of code
                  pNew->pNext = pBody;
                  if (pBody)
                     pBody->pBack = pNew;
                  if (pBody)
                     TokenFindRange (pBody, &pNew->dwCharStart, &pNew->dwCharEnd);
                  else {
                     pNew->dwCharStart = pSwitch->dwCharStart;
                     pNew->dwCharEnd = pSwitch->dwCharEnd;
                  }
                  pDown->pDown = pNew; // so add argyement
               }  // if pNew
            }

         } // while pCases

         TokenFindRange (pSwitch, &pSwitch->dwCharStart, &pSwitch->dwCharEnd);

#if 0 // dont think need for switch
         // terminating semi colon
         if (!pNext || (pNext->dwType != TOKEN_OPER_SEMICOLON)) {
            m_pErr->Add (L"Semicolon expected at the end of the switch() {} statment.", m_pszErrLink, m_pszMethName, TRUE,
               pSwitch->dwCharStart, pSwitch->dwCharEnd);
         }
         else {
            // remove semi
            pToken = pNext;
            pNext = (PMIFLTOKEN)pCur->pNext;
            pToken->pNext = NULL;
            if (pNext)
               pNext->pBack = NULL;
            TokenFree (pToken);
         }
#endif // 0

      }
      break;

   case TOKEN_VAR:
      {
         // isolate var...
         PMIFLTOKEN pEnd;
         for (pEnd = pToken; pEnd; pEnd = (PMIFLTOKEN)pEnd->pNext)
            if (pEnd->dwType == TOKEN_OPER_SEMICOLON)
               break;

         // if there's no end then complain about semi
         if (!pEnd)
            m_pErr->Add (L"Semicolon expected at end of \"var\" declaration.",
               m_pszErrLink, m_pszMethName, TRUE, pToken->dwCharStart, pToken->dwCharEnd);

         if (pEnd) {
            pNext = (PMIFLTOKEN)pEnd->pNext;
            if (pEnd->pBack)
               ((PMIFLTOKEN)pEnd->pBack)->pNext = NULL;
            pEnd->pNext = NULL;
            pEnd->pBack = NULL;
            if (pNext)
               pNext->pBack = NULL;
            TokenFree (pEnd);
         }
         else
            pNext = NULL;

         // pull out the "var" part, but remember loc
         DWORD dwStart = pToken->dwCharStart;
         DWORD dwEnd = pToken->dwCharEnd;
         PMIFLTOKEN pBody = (PMIFLTOKEN) pToken->pNext;
         if (pBody)
            pBody->pBack = NULL;
         pToken->pNext = NULL;
         TokenFree (pToken);

         // repeat while there are variables
         PMIFLTOKEN pPrepend = NULL;
         while (pBody) {
            // find a comma
            PMIFLTOKEN pComma, pDecl;
            for (pComma = pBody; pComma; pComma = (PMIFLTOKEN)pComma->pNext)
               if (pComma->dwType == TOKEN_OPER_COMMA)
                  break;
            if (pComma == pBody) {
               // have a comma without any variables
               m_pErr->Add (L"Variable declaration expected.",
                  m_pszErrLink, m_pszMethName, TRUE, pComma->dwCharStart, pComma->dwCharEnd);
               pBody = (PMIFLTOKEN)pComma->pNext;
               pComma->pNext = NULL;
               if (pBody)
                  pBody->pBack = NULL;
               TokenFree (pComma);
               continue;
            }

            // isolate the declaration... know that there will be at least something
            pDecl = pBody;
            if (pComma) {
               pBody = (PMIFLTOKEN)pComma->pNext;
               ((PMIFLTOKEN)pComma->pBack)->pNext = NULL;
               pComma->pBack = NULL;
               pComma->pNext = NULL;
               if (pBody)
                  pBody->pBack = NULL;
               TokenFree (pComma);
            }
            else
               pBody = NULL;  // next one

            // if it's not an identifier then error
            if (pDecl->dwType != TOKEN_IDENTIFIER) {
               m_pErr->Add (L"Variable name expected.", m_pszErrLink, m_pszMethName, TRUE,
                  pDecl->dwCharStart, pDecl->dwCharEnd);
               TokenFree (pDecl);
               continue;
            }

            if (!m_fCanAddVars) {
               m_pErr->Add (L"Variables cannot be created here.", m_pszErrLink, m_pszMethName, TRUE,
                  pDecl->dwCharStart, pDecl->dwCharEnd);
               TokenFree (pDecl);
               continue;
            }

            if (dwFlags & (TPS_INBLOCK | TPS_INSWITCH | TPS_INLOOP))
               m_pErr->Add (L"Variable declarations are always global, and not local to blocks.", m_pszErrLink, m_pszMethName, FALSE,
                  pDecl->dwCharStart, pDecl->dwCharEnd);


            // test that declaration unique
            DWORD dwFindIndex = m_phCodeVars ? m_phCodeVars->FindIndex (pDecl->pszString, FALSE) : -1;
            if (dwFindIndex == -1) {
               // see if it's one of the global IDs and warn
               PMIFLIDENT pmi;
               if (pmi = (PMIFLIDENT) m_hIdentifiers.Find (pDecl->pszString, FALSE)) {
                  MemZero (&gMemTemp);
                  MemCat (&gMemTemp, L"The variable's name is already used by a ");
                  MemCat (&gMemTemp, ErrTypeToString(pmi->dwType));
                  MemCat (&gMemTemp, L".");
                  m_pErr->Add ((PWSTR)gMemTemp.p, m_pszErrLink, m_pszMethName, FALSE,
                     pDecl->dwCharStart, pDecl->dwCharEnd);
               }

               // add it
               if (m_phCodeVars && m_fCanAddVars)
                  m_phCodeVars->Add (pDecl->pszString, NULL, FALSE);
            }
            else
               m_pErr->Add (L"Variable previously defined.", m_pszErrLink, m_pszMethName, TRUE,
                  pDecl->dwCharStart, pDecl->dwCharEnd);

            // look for equal
            PMIFLTOKEN pEqual = (PMIFLTOKEN) pDecl->pNext;
            if (pEqual && (pEqual->dwType != TOKEN_OPER_ASSIGN)) {
               m_pErr->Add (L"'=' or ',' expected.", m_pszErrLink, m_pszMethName, TRUE,
                  pEqual->dwCharStart, pEqual->dwCharEnd);
               TokenFree (pDecl);
               continue;
            }
            else if (pEqual) {
               // basically have case wher "var x=52", so create an assignment of "x=52"

               // add a semicolon
               PMIFLTOKEN pCur;
               for (pCur = pDecl; pCur->pNext; pCur = (PMIFLTOKEN)pCur->pNext);
               pNew = (PMIFLTOKEN) ESCMALLOC(sizeof(MIFLTOKEN));
               if (pNew) {
                  memset (pNew, 0, sizeof(*pNew));
                  pNew->dwType = TOKEN_OPER_SEMICOLON;
                  pNew->pBack = pCur;
                  pNew->dwCharStart = pCur->dwCharStart;
                  pNew->dwCharEnd = pCur->dwCharEnd;
                  pCur->pNext = pNew;
               }  // if pNew

               // append onto prepend
               if (pPrepend) {
                  for (pCur = pPrepend; pCur->pNext; pCur = (PMIFLTOKEN)pCur->pNext);
                  pCur->pNext = pDecl;
                  pDecl->pBack = pCur;
               }
               else
                  pPrepend = pDecl;
            }
            else {
               // just free delcaration since finished with it
               TokenFree (pDecl);
            }

         } // while pBody

         // if have anything to prepend then do that
         if (pPrepend) {
            PMIFLTOKEN pCur;
            for (pCur = pPrepend; pCur->pNext; pCur = (PMIFLTOKEN)pCur->pNext);

            pCur->pNext = pNext;
            if (pNext)
               pNext->pBack = pCur;
            pNext = pPrepend;
         }
      }
      break;


   default: // unknown, which means look for some sort of expression
      {
         BOOL fFoundSemi;
         PMIFLTOKEN pExp;

         pNext = TokensParseExpression (pToken, &pExp, &fFoundSemi);

         // BUGFIX - If don't find semi-colon then error
         if (!fFoundSemi && pExp) {
            DWORD dwStart, dwEnd;
            TokenFindRange (pExp, &dwStart, &dwEnd);
            m_pErr->Add (L"Semicolon expected at the end of the statement.", m_pszErrLink, m_pszMethName,
               TRUE, dwStart, dwEnd);
         }

         // since expression won't have any elements to the right, ok to return this
         *ppParse = pExp;
      }
      break;
   }

   if (pNext)
      pNext->pBack = NULL;

#ifdef _DEBUG
   if (*ppParse && (*ppParse)->pNext)
      OutputDebugString ("\r\nTokenParseStatement returning bad data!\r\n");
#endif

   return pNext;
}


/*****************************************************************************************
CMIFLCompiled::TokensParseStatements - This starts at the beginning and creates a new list of statements.
   It parses multiple statements.

inputs
   PMIFLTOKEN  pToken - Token to start with
   DWORD       dwFlags - Form of TPS_XXX
   PCMIFLErr   m_pErr - Error to report to
   PWSTR       m_pszMethName - Method name to display in errors
   PWSTR       m_pszErrLink - Link to use if an error occurs in the compilation
returns
   PMIFLTOKEN - Returns the token and has all the parsed statements in a list. This
      isnt always the same as pToken
*/
PMIFLTOKEN CMIFLCompiled::TokensParseStatements (PMIFLTOKEN pToken, DWORD dwFlags)
{
   PMIFLTOKEN pStart = NULL, pTail = NULL;
   PMIFLTOKEN *ppTail = &pStart;

   while (pToken) {
      // stop if too many errors
      if (m_pErr->TooManyErrors()) {
         TokenFree (pToken);
         return NULL; // BUGFIX - Was pToken;
      }

      PMIFLTOKEN pNext, pNew;
      pNext = TokensParseStatement (pToken, &pNew, dwFlags);

      if (pNew) {
         // if there's a new one then hook it in
         pNew->pBack = pTail;
         *ppTail = pTail = pNew;
         ppTail = (PMIFLTOKEN*) &pNew->pNext;
      }

      pToken = pNext;
   }

   return pStart;
}

   
/*****************************************************************************************
CMIFLCompiled::TokensArrange - Orders the tokens according to precedence, etc.

inputs
   PMIFLTOKEN  pToken - Token to start arranging
   PCMIFLErr   m_pErr - Error to report to
   PWSTR       m_pszMethName - Method name to display in errors
   PWSTR       m_pszErrLink - Link to use if an error occurs in the compilation
returns
   PMIFLTOKEN - First token, just in case rearranged the first element.
*/
PMIFLTOKEN CMIFLCompiled::TokensArrange (PMIFLTOKEN pToken)
{
   // pull out paren, brackets, etc.
   pToken = TokensArrangeParen (pToken, TOKEN_OPER_PARENL, TOKEN_OPER_PARENR);
   if (m_pErr->TooManyErrors()) {
      TokenFree (pToken);
      return pToken;
   }
   pToken = TokensArrangeParen (pToken, TOKEN_OPER_BRACKETL, TOKEN_OPER_BRACKETR);
   if (m_pErr->TooManyErrors()) {
      TokenFree (pToken);
      return pToken;
   }
   pToken = TokensArrangeParen (pToken, TOKEN_OPER_BRACEL, TOKEN_OPER_BRACER);
   if (m_pErr->TooManyErrors()) {
      TokenFree (pToken);
      return pToken;
   }

   // parse statemetns
   pToken = TokensParseStatements (pToken, 0);
   if (m_pErr->TooManyErrors()) {
      TokenFree (pToken);
      return NULL;
   }


   return pToken;
}


/*****************************************************************************************
CMIFLCompiled::TokensCompact - Compact all the tokens into a smaller form, MIFLCOMP,
that won't take as much memory for the code.

inputs
   PMIFLTOKEN           pToken - Original token
   PCMem                pMem - Memory to append to
returns
   DWORD - Offset into pMem where this token is placed. Note: If pmem->m_dwCurPosn == 0
   then the token will always be placed at 0.
*/
DWORD CMIFLCompiled::TokensCompact (PMIFLTOKEN pToken, PCMem pMem)
{
   // allocate space for the token
   size_t dwIndex = pMem->Malloc (sizeof(MIFLCOMP));
   PMIFLCOMP pmc = (PMIFLCOMP) ((PBYTE)pMem->p + dwIndex);

   pmc->dwType = pToken->dwType;
   pmc->wCharStart = (WORD)min(pToken->dwCharStart, 0xffff);
   pmc->wCharEnd = (WORD)min(pToken->dwCharEnd, 0xffff);

   // fill in the value
   size_t dwInfo;
   switch (pmc->dwType) {
   case TOKEN_DOUBLE:
      dwInfo = pMem->Malloc (sizeof(double));
      pmc = (PMIFLCOMP) ((PBYTE)pMem->p + dwIndex);
      double *pd;
      pd = (double*) ((PBYTE)pMem->p + dwInfo);
      pmc->dwValue = (DWORD) dwInfo;
      *pd = pToken->fValue;
      break;

   case TOKEN_CHAR:
      pmc->dwValue = (DWORD) pToken->cValue;
      break;

   case TOKEN_STRING:
   case TOKEN_IDENTIFIER:
      DWORD dwLen;
      dwLen = ((DWORD)wcslen(pToken->pszString)+1)*sizeof(WCHAR);
      dwInfo = pMem->Malloc (dwLen);
      pmc = (PMIFLCOMP) ((PBYTE)pMem->p + dwIndex);
      PWSTR psz;
      psz = (PWSTR) ((PBYTE)pMem->p + dwInfo);
      pmc->dwValue = (DWORD)dwInfo;
      memcpy (psz, pToken->pszString, dwLen);
      break;

   case TOKEN_BOOL:
   case TOKEN_STRINGTABLE:
   case TOKEN_RESOURCE:
   case TOKEN_METHPUB:
   case TOKEN_PROPPUB:
   case TOKEN_FUNCTION:
   case TOKEN_GLOBAL:
   case TOKEN_CLASS:
   case TOKEN_METHPRIV:
   case TOKEN_PROPPRIV:
   case TOKEN_VARIABLE:
      pmc->dwValue = pToken->dwValue;
      break;

   default:
      pmc->dwValue = 0;
      break;
   }

   // fill in the next slot
   if (pToken->pNext) {
      DWORD dwNext = TokensCompact ((PMIFLTOKEN)pToken->pNext, pMem);
      pmc = (PMIFLCOMP) ((PBYTE)pMem->p + dwIndex);
      pmc->dwNext = dwNext;
   }
   else
      pmc->dwNext = NULL;

   if (pToken->pDown) {
      DWORD dwDown = TokensCompact ((PMIFLTOKEN)pToken->pDown, pMem);
      pmc = (PMIFLCOMP) ((PBYTE)pMem->p + dwIndex);
      pmc->dwDown = dwDown;
   }
   else
      pmc->dwDown = NULL;

   return (DWORD) dwIndex;
}



/*****************************************************************************************
CMIFLCompiled::TokensIsImport - Checkes to see if the code handles import function

DOCUMENT: Document how to import function from main code.

inputs
   PMIFLTOKEN           pToken - Original token
returns
   pToken - Non-NULL if it was a tag for importing a function, NULL if not and normal
   code. This will report errors.
*/
PMIFLTOKEN CMIFLCompiled::TokensIsImport (PMIFLTOKEN pToken)
{
   if (!pToken || (pToken->dwType != TOKEN_OPER_NUMSIGN) || !m_pMeth)
      return NULL;
      // BUGFIX - If no m_pMeth than cant include import

   // should expect "importfunc" next
   PMIFLTOKEN pCur = (PMIFLTOKEN)pToken->pNext;
   if (!pCur || (pCur->dwType != TOKEN_IDENTIFIER) || wcscmp(pCur->pszString, L"importfunc")) {
      m_pErr->Add (L"Expected \"importfunc\".", m_pszErrLink, m_pszMethName, TRUE,
         (pCur ? pCur : pToken)->dwCharStart, (pCur ? pCur : pToken)->dwCharEnd);
      return pToken;
   }

   // delete pCur
   PMIFLTOKEN pName;
   pToken->pNext = NULL;
   pName = (PMIFLTOKEN)pCur->pNext;
   pCur->pBack = pCur->pNext = NULL;
   if (pName)
      pName->pBack = NULL;
   TokenFree (pCur);

   // should expect a function name next
   if (!pName || (pName->dwType != TOKEN_IDENTIFIER)) {
      m_pErr->Add (L"Expected function/method name to import.", m_pszErrLink, m_pszMethName, TRUE,
         (pName ? pName : pToken)->dwCharStart, (pName ? pName : pToken)->dwCharEnd);

      if (pName)
         TokenFree (pName);
      return pToken;
   }

   // make sure the imported function call is valid
   DWORD dwParams;
   BOOL fValid = m_pSocket->FuncImportIsValid (pName->pszString, &dwParams);
   if (!fValid) {
      m_pErr->Add (L"The imported function name is not supported by this application.", m_pszErrLink, m_pszMethName, TRUE,
         pName->dwCharStart, pName->dwCharEnd);

      TokenFree (pName);
      return pToken;
   }

   // see if right number of params...
   DWORD dwInThis = m_pMeth->m_lPCMIFLProp.Num();
   if (m_pMeth->m_fParamAnyNum)
      dwInThis = -1;
   if (dwInThis != dwParams) {
      MemZero (&gMemTemp);
      if (dwParams == -1)
         MemCat (&gMemTemp, L"The imported function accepts any number of parameters. This method/function should also.");
      else {
         MemCat (&gMemTemp, L"The imported function accepts only ");
         MemCat (&gMemTemp, (int)dwParams);
         MemCat (&gMemTemp, L" parameters.");
      }

      m_pErr->Add ((PWSTR)gMemTemp.p, m_pszErrLink, m_pszMethName, TRUE,
         pName->dwCharStart, pName->dwCharEnd);

      TokenFree (pName);
      return pToken;
   }

   if (pName->pNext) {
      PMIFLTOKEN pDel = (PMIFLTOKEN)pName->pNext;
      pName->pNext = NULL;
      pDel->pBack = NULL;

      TokenFindRange (pDel, &pDel->dwCharStart, &pDel->dwCharEnd);

      m_pErr->Add (L"The following code is ignored.", m_pszErrLink, m_pszMethName, FALSE,
         pDel->dwCharStart, pDel->dwCharEnd);
      TokenFree (pDel);
   }

   // move name under the #
   pToken->pDown = pName;
   return pToken;
}



/*****************************************************************************************
CMIFLCompiled::CompileCodeInternal - This compiles a segment of code.

inputs
   PCMIFLMeth        pMeth - Method to use for input. This can be null.
   PCHashString      phVars - Pointer to hash of local variables. 0-sized elements.
                        NULL if no local variables are possible
   BOOL              fCanAddVars - Set to TRUE if can add vars
   PWSTR             pszCode - Code
   PCMem             pCodeComp - Where the compiled code ends up. Filled with MIFLCOMP
   PCMIFLObject      pObject - Object that this is in. NULL if not in object.
   PWSTR             pszErrLink - Link to use if an error occurs in the compilation
   PWSTR             pszMethName - Method name to use when reporting errors
returns
   None - If errors were added to m_pErr then it didn't compile properly
*/
void CMIFLCompiled::CompileCodeInternal (PCMIFLMeth pMeth, PCHashString phVars,
                                         BOOL fCanAddVars, PWSTR pszCode, PCMem pCodeComp,
                                         PCMIFLObject pObject, PWSTR pszErrLink, PWSTR pszMethName)
{
   // set up method and and link so can use later
   m_pszMethName = pszMethName;
   m_pszErrLink = pszErrLink;
   m_phCodeVars = phVars;
   m_fCanAddVars = fCanAddVars;
   m_pCodeComp = pCodeComp;
   m_pMeth = pMeth;
   m_pObject = pObject;
   pCodeComp->m_dwCurPosn = 0;

   PMIFLTOKEN pToken = Tokenize (pszCode);
   if (!pToken)
      return;
   if (m_pErr->TooManyErrors()) {
      TokenFree (pToken);
      return;
   }

   // arrange tokens... see if it starts with an importfunc first
   PMIFLTOKEN pImport;
   pImport = TokensIsImport (pToken);
   if (pImport)
      pToken = pImport;
   else
      pToken = TokensArrange (pToken);
   if (!pToken)
      return;  // BUGFIX - Since had an error pop up
   if (m_pErr->TooManyErrors()) {

      if (pToken)
         TokenFree (pToken);
      return;
   }

   // see what have...
   // TokenDebug (pToken);

   // exit if any errors
   gMemTemp.m_dwCurPosn = 0;
   if (m_pErr->m_dwNumError) {
      TokenFree (pToken);
      return;
   }

   // compact
   TokensCompact (pToken, &gMemTemp);
   TokenFree (pToken);
   if (!pCodeComp->Required (gMemTemp.m_dwCurPosn)) {
      pCodeComp->m_dwCurPosn = 0;
      m_pErr->Add (L"Out of memory.", m_pszErrLink, m_pszMethName, TRUE, 0, 0);
      // out of memory
   }
   else {
      memcpy (pCodeComp->p, gMemTemp.p, gMemTemp.m_dwCurPosn);
      pCodeComp->m_dwCurPosn = gMemTemp.m_dwCurPosn;
   }

   // done
}



/*****************************************************************************
HackAddSemi - If an initializer is missing a semi-colon at the end then add one

inputs
   PCMem          pMemCode - Memory contianing the code
   PWSTR          psz - Initializer
*/
static void HackAddSemi (PCMem pMemCode, PWSTR psz)
{
   MemZero (pMemCode);
   MemCat (pMemCode, psz);

   DWORD dwLen = (DWORD)wcslen(psz);
   for (; dwLen; dwLen--) {
      if (iswspace (psz[dwLen-1]))
         continue;
      if (psz[dwLen-1] == L';')
         return;  // found a semi, so no need to add
      break;
   }

   // else, need to add semi
   MemCat (pMemCode, L";");
}


/*****************************************************************************************
CMIFLCompiled::CompileCode - This is a public function that compiles some code.

inputs
   PCMIFLErrors      pErr - Will be filled with the errors. If any errors occur then
                     the compilation failed.
   PCMIFLMeth        pMeth - Method to use for input. This can be null. You only need
                     this if want to be able to import a function.
   PCHashString      phVars - Pointer to hash of local variables. 0-sized elements.
                        NULL if no local variables are possible
   BOOL              fCanAddVars - Set to TRUE if can add vars
   PWSTR             pszCode - Code
   BOOL              fAddSemi - If TRUE then add a semi-colon to the code.
   PCMem             pCodeComp - Where the compiled code ends up. Filled with MIFLCOMP
   PCMIFLObject      pObject - Object that this is in. NULL if not in object.
   PWSTR             pszErrLink - Link to use if an error occurs in the compilation
   PWSTR             pszMethName - Method name to use when reporting errors
returns
   None - If errors were added to m_pErr then it didn't compile properly
*/
void CMIFLCompiled::CompileCode (PCMIFLErrors pErr, PCMIFLMeth pMeth, PCHashString phVars,
                                 BOOL fCanAddVars, PWSTR pszCode, BOOL fAddSemi, PCMem pCodeComp,
                                 PCMIFLObject pObject, PWSTR pszErrLink, PWSTR pszMethName)
{
   m_pErr = pErr;

   CMem memSemi;
   if (fAddSemi) {
      HackAddSemi (&memSemi, pszCode);
      pszCode = (PWSTR)memSemi.p;
   }
   
   CompileCodeInternal (pMeth, phVars, fCanAddVars, pszCode, pCodeComp, pObject,
      pszErrLink, pszMethName);
}


/*****************************************************************************************
CMIFLCompiled::CompileCode - This compiles a segment of code.

NOTE: This assumes m_pErr has been initialized

inputs
   PCMIFLMeth        pMeth - Method to use for input
   PCMIFLCode        pCode - Code to use, and where to write compiled results
   PCMIFLObject      pObject - Object that this is in. NULL if not in object
                        Must be an object if dwCodeFrom ==3,4,5
   PWSTR             m_pszErrLink - Link to use if an error occurs in the compilation
   DWORD             dwCodeFrom - Where the code came from. This is used to remember where
                        the code came from, for purposes of archiving.
                        0 = function, 1=global get, 2=global set,
                        3 = method, 4=property get, 5=property set, 6 = kniwn
   PWSTR             pszCodeName - Name of the code, displayed when debugging.
                        This MUST be a permenant string, such as the method's name,
                        or if it's a get/set variable, then the property's name.
                        This must correspond to the entity from dwCodeFrom
returns
   None - If errors were added to m_pErr then it didn't compile properly
*/
void CMIFLCompiled::CompileCode (PCMIFLMeth pMeth, PCMIFLCode pCode,
                                 PCMIFLObject pObject, PWSTR pszErrLink,
                                 DWORD dwCodeFrom, PWSTR pszCodeName)
{
   pMeth->InitCodeVars (pCode);
   pCode->m_pObjectLayer = pObject; // BUGFIX - So that know what layer came from
   pCode->m_pszCodeName = pszCodeName;
   pCode->m_dwCodeFrom = dwCodeFrom;

   CompileCodeInternal (pMeth, &pCode->m_hVars, TRUE, (PWSTR)pCode->m_memCode.p,
      &pCode->m_memMIFLCOMP, pObject, pszErrLink, (PWSTR)pMeth->m_memName.p);
}



/*****************************************************************************
CMIFLCompiled::VarInitQuick - This internal function looks at a compiled section
of code (belonging to a variable initialization) and derives a value from it.

This will add onto m_pErr. It also assumes m_pszMethName, and m_pszErrLink are set.

FUTURERELEASE - May eventually want better initialization because can only set
a global/property to an initial value; can't call a function in the process.

inputs
   PCMem          pCode - Memory containing the MIFLCOMP code
   DWORD          dwIndex - Index to use
   PCMIFLVar      pVar - Should be initialized to something, but will be modified by
                  this function and filled in
*/
void CMIFLCompiled::VarInitQuick (PCMem pCode, DWORD dwIndex, PCMIFLVar pVar)
{
   PMIFLCOMP pc = (PMIFLCOMP)((PBYTE)pCode->p + dwIndex);

   if (pc->dwNext) {
      PMIFLCOMP pc2 = (PMIFLCOMP)((PBYTE)pCode->p + pc->dwNext);
      m_pErr->Add (L"Unexpected token in variable initializer.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc2->wCharStart, (DWORD)pc2->wCharEnd);
   }

   if ((pc->dwType >= TOKEN_OPER_START) && (pc->dwType <= TOKEN_OPER_END)) {
      pVar->SetUndefined();
      m_pErr->Add (L"Operators are not allowed in variable initializers.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
      return;
   }

   switch (pc->dwType) {
   case TOKEN_NOP:
      pVar->SetUndefined();
      return;

   case TOKEN_DOUBLE:
      {
         double *pf = (double*)((PBYTE)pCode->p + pc->dwValue);
         pVar->SetDouble (*pf);
      }
      return;

   case TOKEN_CHAR:
      pVar->SetChar ((WCHAR)pc->dwValue);
      return;

   case TOKEN_STRING:
      {
         PWSTR psz = (PWSTR)((PBYTE)pCode->p + pc->dwValue);
         pVar->SetString (psz, (DWORD)-1);
      }
      return;

   case TOKEN_NULL:
      pVar->SetNULL ();
      return;

   case TOKEN_UNDEFINED:
      pVar->SetUndefined ();
      return;

   case TOKEN_BOOL:
      pVar->SetBOOL (pc->dwValue);
      return;

   case TOKEN_STRINGTABLE:
      pVar->SetStringTable (pc->dwValue);
      return;

   case TOKEN_RESOURCE:
      pVar->SetResource (pc->dwValue);
      return;

   case TOKEN_METHPUB:
      pVar->SetMeth (pc->dwValue);
      return;

   case TOKEN_PROPPUB:
   case TOKEN_PROPPRIV:
      m_pErr->Add (L"A variable/property initializer cannot access an object's properties.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
      break;

   // BUGFIX - ALlow to put function name, put not function call.
   // Originally prevented functions from being placed
   case TOKEN_FUNCTION:
      pVar->SetFunc (pc->dwValue);
      return;

   // case TOKEN_FUNCTION:
   case TOKEN_FUNCCALL:
      m_pErr->Add (L"A variable/property initializer cannot access a function.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
      break;

   case TOKEN_GLOBAL:
      {
         PCMIFLVarProp pv = (PCMIFLVarProp)m_hGlobalsDefault.Find (pc->dwValue);
         if (!pv) {
            m_pErr->Add (L"The referenced global variable has not been initialized yet.",
               m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
            break;
         }

         // else, copy it over
         pVar->Set (&pv->m_Var);
      }
      return;

   case TOKEN_CLASS:
      m_pErr->Add (L"A variable/property initializer cannot access a class.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
      break;

   case TOKEN_METHPRIV:
      pVar->SetMeth (pc->dwValue);
      return;

   case TOKEN_VARIABLE:
      m_pErr->Add (L"A variable/property initializer cannot access an object's properties.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
      break;

   case TOKEN_LISTDEF:
      {
         // create a new list
         PCMIFLVarList pList = new CMIFLVarList;
         if (!pList)
            break;   // error

         if (pc->dwDown) {
            PMIFLCOMP pDown;
            DWORD dwLoc = pc->dwDown;
            pDown = (PMIFLCOMP)((PBYTE)pCode->p + dwLoc);

            if ((pDown->dwType == TOKEN_OPER_COMMA) && (pDown->dwDown)) {
               pDown = (PMIFLCOMP)((PBYTE)pCode->p + pDown->dwDown);
               while (TRUE) {
                  CMIFLVar var;
                  if (pDown->dwType == TOKEN_ARG1) {
                     if (pDown->dwNext)
                        VarInitQuick (pCode, pDown->dwNext, &var);
                     else
                        var.SetUndefined();
                  }
                  
                  // add this to the list
                  pList->Add (&var, TRUE);

                  // move next
                  if (!pDown->dwDown)
                     break;
                  pDown = (PMIFLCOMP)((PBYTE)pCode->p + pDown->dwDown);

               } // while TRUE
            } // comma
            else if (pDown->dwType != TOKEN_OPER_COMMA) {
               // one element by itself
               CMIFLVar var;
               VarInitQuick (pCode, dwLoc, &var);
               // add this to the list
               pList->Add (&var, TRUE);
            }
         }

         // set the list
         pVar->SetList (pList);
         pList->Release();
      }
      return;

   case TOKEN_LISTINDEX:
      m_pErr->Add (L"Indexing an array is not allowed in a variable initializer.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
      break;

   default:
      m_pErr->Add (L"Unexpected token.",
         m_pszErrLink, m_pszMethName, TRUE, (DWORD)pc->wCharStart, (DWORD)pc->wCharEnd);
      break;

   }

   // default
   pVar->SetUndefined();
}



/*****************************************************************************
CMIFLCompiled::VarInitQuick - Loops through all the global definitions and the
objects (with their properties) and initializes variables based upon the
given values.
*/
void CMIFLCompiled::VarInitQuick (void)
{
   // loop through all the globals...
   DWORD i;
   CMIFLVarProp vpUndefined;
   vpUndefined.m_Var.SetUndefined();

   // leave both code and meth blank
   CMIFLCode Code;
   CMem memName;
   WCHAR szTemp[64];

   CMIFLVarProp vp;

   for (i = 0; i < m_hGlobals.Num(); i++) {
      if (m_pErr->TooManyErrors())
         return;

      PMIFLIDENT pmi = (PMIFLIDENT)m_hGlobals.Get(i);

      if (pmi->dwType != MIFLI_GLOBAL)
         continue;

      // if already exists in list then ignore
      if (-1 != m_hGlobalsDefault.FindIndex (pmi->dwID))
         continue;   // shouldnt happen

      // should be a property
      PCMIFLProp pProp = (PCMIFLProp)pmi->pEntity;

      // if empty string then easy
      PWSTR psz = (PWSTR)pProp->m_memInit.p;
      if (!psz[0]) {
         // not set to anything
         vpUndefined.m_dwID = pmi->dwID;
         m_hGlobalsDefault.Add (pmi->dwID, &vpUndefined);
         continue;
      }

      // else, try to compile
      HackAddSemi (&Code.m_memCode, psz);
      swprintf (szTemp, L"lib:%dglobal:%dedit", (int)pProp->m_dwLibOrigFrom, (int)pProp->m_dwTempID);
      MemZero (&memName);
      MemCat (&memName, (PWSTR)pProp->m_memName.p);
      MemCat (&memName, L" (Global variable)");
      CompileCodeInternal (NULL, NULL, FALSE, (PWSTR)Code.m_memCode.p, &Code.m_memMIFLCOMP,
         NULL, szTemp, (PWSTR)memName.p);

      // if nothing left after compile then default
      if (!Code.m_memMIFLCOMP.m_dwCurPosn) {
         // not set to anything
         vpUndefined.m_dwID = pmi->dwID;
         m_hGlobalsDefault.Add (pmi->dwID, &vpUndefined);
         continue;
      }

      // else, see how it works
      VarInitQuick (&Code.m_memMIFLCOMP, 0, &vp.m_Var);
      vp.m_Var.AddRef();
      vp.m_dwID = pmi->dwID;
      vp.m_pCodeGet = pProp->m_pCodeGet;
      vp.m_pCodeSet = pProp->m_pCodeSet;
      m_hGlobalsDefault.Add (pmi->dwID, &vp);
   } // i

   // go through every object
   DWORD j, dwPriv;
   for (i = 0; i < m_pLib->ObjectNum(); i++) {
      // abort if too many errors
      if (m_pErr->TooManyErrors())
         return;

      PCMIFLObject pObject = m_pLib->ObjectGet(i);
      pObject->ClearPropDefault();

      // while here, update contained in
      if (pObject->m_pContainedIn)
         pObject->m_pContainedIn->m_lContainsDefault.Add (&pObject->m_gID);

      // pass through all private and public properties...
      for (dwPriv = 0; dwPriv < 2; dwPriv++) {
         PCListFixed pl = dwPriv ? &pObject->m_lPCMIFLPropPriv : &pObject->m_lPCMIFLPropPub;

         PCMIFLProp *ppp = (PCMIFLProp*)pl->Get(0);
         for (j = 0; j < pl->Num(); j++) {
            PCMIFLProp pProp = ppp[j];

            // find it
            PMIFLIDENT pmi;
            if (dwPriv)
               pmi = (PMIFLIDENT) pObject->m_hPrivIdentity.Find ((PWSTR)pProp->m_memName.p, FALSE);
            else
               pmi = (PMIFLIDENT) m_hIdentifiers.Find ((PWSTR)pProp->m_memName.p, FALSE);

            if (!pmi || !((pmi->dwType == MIFLI_PROPDEF) || (pmi->dwType == MIFLI_PROPPRIV)))
               continue;   // shouldnt happen

            // verify that doesn't already exist
            if (-1 != pObject->m_hPropDefaultJustThis.FindIndex (pmi->dwID))
               continue;   // shouldnt happen

            // if empty string then easy
            PWSTR psz = (PWSTR)pProp->m_memInit.p;
            if (!psz[0]) {
               // BUGFIX - If have codeget or codeset then default to undefined
               // need to do this so can have property overrides for layers
               if (pProp->m_pCodeGet || pProp->m_pCodeSet) {
                  vp.m_Var.SetUndefined();
                  goto bypasscompile;
               }

               // not set to anything
               // BUGFIX - Since this is empty, leave it to use the default value
               // up the chain, so if a recommended method is automatically added
               // but left blank by the user, it won't affect
               // vpUndefined.m_dwID = pmi->dwID;
               // pObject->m_hPropDefaultJustThis.Add (pmi->dwID, &vpUndefined);
               // pObject->m_hPropDefaultAllClass.Add (pmi->dwID, &vpUndefined);
               continue;
            }

            // else, try to compile
            HackAddSemi (&Code.m_memCode, psz);
            swprintf (szTemp, L"lib:%dobject:%dedit",
               (int)pProp->m_dwLibOrigFrom,
               (int)ErrObjectToID (pProp->m_dwLibOrigFrom, m_pProj, pObject));
            MemZero (&memName);
            MemCat (&memName, (PWSTR)pProp->m_memName.p);
            MemCat (&memName, L" (Property)");
            CompileCodeInternal (NULL, NULL, FALSE, (PWSTR)Code.m_memCode.p, &Code.m_memMIFLCOMP,
               NULL, szTemp, (PWSTR)memName.p);

            // if nothing left after compile then default
            if (!Code.m_memMIFLCOMP.m_dwCurPosn) {
               // not set to anything

               // BUGFIX - If have codeget or codeset then default to undefined
               // need to do this so can have property overrides for layers
               if (pProp->m_pCodeGet || pProp->m_pCodeSet) {
                  vp.m_Var.SetUndefined();
                  goto bypasscompile;
               }

               // BUGFIX - Since this is empty, leave it to use the default value
               // up the chain, so if a recommended method is automatically added
               // but left blank by the user, it won't affect
               //vpUndefined.m_dwID = pmi->dwID;
               //pObject->m_hPropDefaultJustThis.Add (pmi->dwID, &vpUndefined);
               //pObject->m_hPropDefaultAllClass.Add (pmi->dwID, &vpUndefined);
               continue;
            }

            // else, see how it works
            VarInitQuick (&Code.m_memMIFLCOMP, 0, &vp.m_Var);
bypasscompile:
            vp.m_Var.AddRef();
            vp.m_Var.AddRef();   // addref twice because in two lists
            vp.m_dwID = pmi->dwID;
            vp.m_pCodeGet = pProp->m_pCodeGet;
            vp.m_pCodeSet = pProp->m_pCodeSet;
            pObject->m_hPropDefaultJustThis.Add (pmi->dwID, &vp);
            pObject->m_hPropDefaultAllClass.Add (pmi->dwID, &vp);
         } // j
      } // dwPriv

   } // i


   // go through all the objects again, this time looking at superclasses and
   // deriving properties from there...
   DWORD k;
   for (i = 0; i < m_pLib->ObjectNum(); i++) {
      PCMIFLObject pObject = m_pLib->ObjectGet(i);

      PCMIFLObject *pps = (PCMIFLObject*)pObject->m_lClassSuperAllPCMIFLObject.Get(0);
      for (j = 0; j < pObject->m_lClassSuperAllPCMIFLObject.Num(); j++) {
         PCMIFLObject pSuper = pps[j];

         for (k = 0; k < pSuper->m_hPropDefaultJustThis.Num(); k++) {
            PCMIFLVarProp pvSuper = (PCMIFLVarProp) pSuper->m_hPropDefaultJustThis.Get(k);

            // see if already exists
            // BUGFIX - Was -1 ==, changed to -1 !=
            if (-1 != pObject->m_hPropDefaultAllClass.FindIndex (pvSuper->m_dwID))
               continue;   // already have in based on higher class

            // else, add it
            pvSuper->m_Var.AddRef();
            pObject->m_hPropDefaultAllClass.Add (pvSuper->m_dwID, pvSuper);
         } // k
      } // j
   } // i


}



// DOCUMENT: "arguements" and "this" variable in every function

// DOCUMENT: Private methods/properties will return "undefined" if accessed from another object

// NOTE - there's a better way of deleting in a hash that will make deleting
// very quick, except that must also loop though entire hash table decementing
// numbers too. In the end, it's not an awful lot quicker and ends up being more
// risky to code. May do later if really need speed.

// FUTURERELEASE - Make a faster delete for the hash functions. Only need to do
// this when have 1000+ objects


// BUGBUG - Had "for (; vRepeat; vRepeat--)" and got a bad expression error.
// it worked when changed to "for (0; vRepeat; vRepeat--)"



