/***********************************************************************
MIFL.h - Internal header file

begun 24/12/03 by Mike Rozak
Copyright 2003 Mike Rozak
*/

#ifndef _MIFL_H_
#define _MIFL_H_

DEFINE_GUID(GUID_MIFLProj, 
0x1f3d409c, 0xab4a, 0x1fb8, 0xae, 0x6d, 0x19, 0xab, 0x85, 0x71, 0x99, 0x1c);

DEFINE_GUID(GUID_MIFLLib,
0xa9c4401a, 0xab4a, 0x1fb8, 0xae, 0x6d, 0x19, 0xab, 0x85, 0x71, 0x2f, 0x06);


// #define USEIPV6            // so can use larger addresses

/*************************************************************************************
CMIFLCompiled */

/*********************************************************************************
MIFLMisc */

class CMIFLCompiled;
typedef CMIFLCompiled *PCMIFLCompiled;

// CMIFLVar - Helper object for storing variables
// NOTE: The numeric order of these affects how Compare() works, so dont change
#define MV_UNDEFINED          0
#define MV_NULL               1
#define MV_BOOL               2
#define MV_CHAR               3
#define MV_DOUBLE             4

#define MV_STRING             10       // memory points to a PCMIFLVarString
#define MV_LIST               11       // memory points to PCMIFLVarList
#define MV_STRINGMETH         12       // memory points string, plus dwValue is the ID of the method
#define MV_LISTMETH           13       // memory points to list, plus dwValue is the ID of the method

#define MV_STRINGTABLE        20       // string table. dwValue is the ID
#define MV_RESOURCE           21       // resource in table. dwValue is the ID
#define MV_OBJECT             22       // object GUID stored
#define MV_METH               23       // public/private method, no object. dwValue is the ID
#define MV_OBJECTMETH         24       // public/private method with associated object, dwValue is ID, plus GUID
#define MV_FUNC               25       // function call. dwValue is the ID

#define MLV_NONE              0        // no LValue available
#define MLV_VARIABLE          1        // m_pLValue is a pointer to PCMIFLVar
#define MLV_GLOBAL            2        // m_dwLValueID is the global ID
#define MLV_PROPERTY          3        // m_pLValue is a pointer to PCMILFVMObject, m_dwLValueID is property ID
#define MLV_LISTINDEX         4        // m_pLValue is a pointer to PCMIFLVarList, m_dwLValueID is the index. NOT addrefed
#define MLV_STRINGINDEX       5        // m_pLValue is a pointer to PCMIFLVarString, m_dwLVValueID is the char. NOT addrefed

class CMIFLVar;
typedef CMIFLVar *PCMIFLVar;
class CMIFLVarString;
typedef CMIFLVarString *PCMIFLVarString;
class CMIFLVarList;
typedef CMIFLVarList *PCMIFLVarList;
class CMIFLVM;
typedef CMIFLVM *PCMIFLVM;

class DLLEXPORT CMIFLVar {
public:
   ESCNEWDELETE;

   CMIFLVar (void);
   ~CMIFLVar (void);

   void InitInt (void);
   PCMMLNode2 MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList);
   BOOL MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phList,
      PCHashGUID phObjectRemap);

   // NOTE: Doing a SetXXX() automatically clears any LValue UNLESS call Set().
   void SetUndefined (void);
   void SetNULL (void);
   void SetBOOL (BOOL fValue);
   void SetChar (WCHAR cValue);
   void SetDouble (double fValue);
   BOOL SetString (PWSTR psz, DWORD dwLength = (DWORD)-1);
   void SetString (PCMIFLVarString pString);
   BOOL SetListNew (void);
   void SetList (PCMIFLVarList pList);
   void SetStringTable (DWORD dwID);
   void SetResource (DWORD dwID);
   void SetObject (GUID *pgObject);
   void SetMeth (DWORD dwID);
   void SetObjectMeth (GUID *pgObject, DWORD dwID);
   void SetStringMeth (PCMIFLVarString ps, DWORD dwID);
   void SetListMeth (PCMIFLVarList pl, DWORD dwID);
   void SetFunc (DWORD dwID);
   void Set (PCMIFLVar pFrom);

   void ToBOOL (PCMIFLVM pVM);
   void ToChar (PCMIFLVM pVM);
   void ToDouble (PCMIFLVM pVM);
   void ToString (PCMIFLVM pVM);
   void ToMeth (PCMIFLVM pVM);
   void ToFunction (PCMIFLVM pVM);

   BOOL GetBOOL (PCMIFLVM pVM);
   WCHAR GetChar (PCMIFLVM pVM);
   double GetDouble (PCMIFLVM pVM);
   PCMIFLVarString GetString (PCMIFLVM pVM);
   PCMIFLVarString GetStringNoMod (void);
   PCMIFLVarList GetList (void);
   DWORD GetValue (void);
   GUID GetGUID (void);

   DWORD TypeGet (void);
   PWSTR TypeAsString (void);

   int Compare (PCMIFLVar pWith, BOOL fExact, PCMIFLVM pVM);

   void AddRef (void);
   void Fracture (BOOL fReleaseOld = TRUE);
   void Remap (PCHashGUID phOrigToNew);

   void OperNegation (PCMIFLVM pVM);
   void OperBitwiseNot (PCMIFLVM pVM);
   void OperLogicalNot (PCMIFLVM pVM);
   void OperMultiply (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperDivide (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperModulo (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperAdd (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperSubtract (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperBitwiseLeft (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperBitwiseRight (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperBitwiseAnd (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperBitwiseXOr (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperBitwiseOr (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperLogicalAnd (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);
   void OperLogicalOr (PCMIFLVM pVM, PCMIFLVar pVarL, PCMIFLVar pVarR);

private:
   void ReleaseInt (void);

   // variable info
   DWORD             m_dwType;      // MV_XXX
   DWORD             m_dwValue;     // meaning depends on dwType
   union {
      GUID           m_gValue;      // GUID, used for MV_OBJECT and MB_METHOBJECT
      double         m_fValue;      // for MV_DOUBLE
      PVOID          m_pValue;      // for MV_STRING and MV_LIST
   };

};
typedef CMIFLVar *PCMIFLVar;



// CMIFLVarLValue- Stores CMIFLVar, but with LValue info
class DLLEXPORT CMIFLVarLValue {
public:
   ESCNEWDELETE;

   CMIFLVarLValue (void);
   // ~CMIFLVarLValue (void);
   void Clear (void)
      {
         m_dwLValue = MLV_NONE;
      };


   // lvalue information
   DWORD             m_dwLValue;       // type LValue, MLV_XXX
   PVOID             m_pLValue;        // pointer for LValue, depends on MLV_XXX
   DWORD             m_dwLValueID;     // ID for LValue. depends on MLV_XXX

   CMIFLVar          m_Var;         // actual variable
};
typedef CMIFLVarLValue *PCMIFLVarLValue;


// CMIFLVarProp - Variable information when stored as a property
class CMIFLCode;
typedef CMIFLCode *PCMIFLCode;
class DLLEXPORT CMIFLVarProp {
public:
   ESCNEWDELETE;

   CMIFLVarProp(void) {
      m_dwID = 0;
      m_pCodeGet = m_pCodeSet = 0;
   };

   CMIFLVar          m_Var;         // variable
   DWORD             m_dwID;        // id for the entity
   PCMIFLCode        m_pCodeGet;    // code for getting, or NULL if is none
   PCMIFLCode        m_pCodeSet;    // code for setting, or NULL if is none
};
typedef CMIFLVarProp *PCMIFLVarProp;

// CMIFLVarString - Reference counted string
class DLLEXPORT CMIFLVarString {
public:
   ESCNEWDELETE;

   CMIFLVarString (void);
   // ~CMIFLVarString (void); - Dont call delete() directly. Call Release()
   DWORD Release (void);
   DWORD AddRef (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   BOOL Set (PWSTR psz, DWORD dwLength = (DWORD)-1);
   BOOL Set (DWORD dwIndex, PCMIFLVar pVar, PCMIFLVM pVM);
   PWSTR Get (void);

   PCMIFLVarString Clone (void);
   BOOL Prepend (PWSTR psz, DWORD dwLength);
   BOOL Append (PWSTR psz, DWORD dwLength);
   BOOL Replace (PWSTR psz /* = NULL */, DWORD dwLength /* = -1 */, int iStart /*= 0*/, int iEnd /*= 1000000000*/);
   void Trim (int iTrim = 0);
   void Truncate (DWORD dwChar, BOOL fEllipses);
   PWSTR CharGet (DWORD dwChar);

   DWORD Length (void)
      {
         return (DWORD)m_memString.m_dwCurPosn / sizeof(WCHAR);
      };
   BOOL Append (CMIFLVarString *pvs)
      {
         return Append (pvs->Get(), pvs->Length());
      };

private:
   CMem           m_memString;      // memory containing the string. m_dwCurPosn is set to 2 * wcslen()
   DWORD          m_dwRefCount;     // reference count
};
typedef CMIFLVarString *PCMIFLVarString;


// CMIFLVarList - Reference counted List
class DLLEXPORT CMIFLVarList {
public:
   ESCNEWDELETE;

   CMIFLVarList (void);
   ~CMIFLVarList (void); // Dont call delete() directly. Call Release()
   DWORD Release (void);
   DWORD AddRef (void);
   PCMIFLVarList Clone (void);

   PCMMLNode2 MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList);
   BOOL MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phList,
      PCHashGUID phObjectRemap);

   void Clear (void);
   DWORD Num (void);
   BOOL Add (PCMIFLVar pAdd, BOOL fListAsSublist);
   BOOL Insert (PCMIFLVar pAdd, BOOL fListAsSublist, DWORD dwBefore);
   BOOL Remove (DWORD dwStart, DWORD dwEnd);
   PCMIFLVarList Sublist (DWORD dwStart, DWORD dwEnd);
   void Reverse (void);
   void Randomize (PCMIFLVM pVM);
   int Compare (PCMIFLVarList pWith, BOOL fExact, PCMIFLVM pVM);
   PCMIFLVar Get (DWORD dwIndex);
   BOOL ToString (PCMem pMem, PCMIFLVM pVM, BOOL fPrependComma = FALSE);
   BOOL Set (DWORD dwIndex, PCMIFLVar pVar);

private:
   BOOL VerifyNotRecurse (PCMIFLVar pTest);

   CListFixed     m_lCMIFLVar;      // list of CMIFLVar
   DWORD          m_dwRefCount;     // reference count
};
typedef CMIFLVarList *PCMIFLVarList;

DLLEXPORT PCMMLNode2 CMIFLVarStringMMLTo (PCHashPVOID phString);
DLLEXPORT PCHashDWORD CMIFLVarStringMMLFrom (PCMMLNode2 pNode);
DLLEXPORT PCMMLNode2 CMIFLVarListMMLTo (PCHashPVOID phList, PCMIFLVM pVM, PCHashPVOID phString);
DLLEXPORT PCHashDWORD CMIFLVarListMMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString,
                                           PCHashGUID phObjectRemap);
DLLEXPORT PCMMLNode2 CodeGetSetMMLTo (PCMIFLCode pCode);
DLLEXPORT PVOID CodeGetSetMMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, BOOL fFunc);
DLLEXPORT double MIFLFileTimeToDouble (PFILETIME pft);
DLLEXPORT void MIFLDoubleToFileTime (double f, FILETIME *pft);


/************************************************************************************
MIFLGen */

class CMIFLMeth;
typedef CMIFLMeth *PCMIFLMeth;
class CMIFLObject;
typedef CMIFLObject *PCMIFLObject;
class CMIFLLib;
typedef CMIFLLib *PCMIFLLib;
class CMIFLProj;
typedef CMIFLProj *PCMIFLProj;
class CMIFLProp;
typedef CMIFLProp *PCMIFLProp;
class CMIFLDoc;
typedef CMIFLDoc *PCMIFLDoc;
class CMIFLCode;
typedef CMIFLCode *PCMIFLCode;
class CMIFLFunc;
typedef CMIFLFunc *PCMIFLFunc;
class CMIFLString;
typedef CMIFLString *PCMIFLString;
class CMIFLResource;
typedef CMIFLResource *PCMIFLResource;
class CMIFLErrors;
typedef CMIFLErrors *PCMIFLErrors;
class CMIFLVMObject;
typedef CMIFLVMObject *PCMIFLVMObject;
class CMIFLVMTimer;
typedef CMIFLVMTimer *PCMIFLVMTimer;

// MASLIB - Information that an app returns about the library
typedef struct {
   PWSTR          pszName;       // filled with the name of the library
   PWSTR          pszDescShort;  // filled with a short description of the library
   BOOL           fDefaultOn;    // set to TRUE if library should be used by all new projects
   DWORD          dwResource;    // resource number, of type 'MIFL'
   HINSTANCE      hResourceInst; // instance handle where to get the resource from
} MASLIB, *PMASLIB;

// MASRES - Information that the app returns about a resource type
typedef struct {
   PWSTR          pszName;       // filled in with th ename of the resource
   PWSTR          pszDescShort;  // short description
} MASRES, *PMASRES;


// MIFLPAGE - Information passed to mifl page's
typedef struct {
   PCMIFLProj        pProj;   // current project that modifying
   PCMIFLLib         pLib;    // library used, if modifying library specific
   PCMIFLObject      pObj;    // object used, if on object modifying page
   PCMIFLMeth        pMeth;   // method being modified
   PCMIFLProp        pProp;   // property being modified
   PCMIFLCode        pCode;   // code being modified
   PCMIFLFunc        pFunc;   // function being modified
   PCMIFLString      pString; // string being modified
   PCMIFLResource    pResource; // Resource being modified
   PCMIFLDoc         pDoc;    // document being modified
   PCMIFLVM          pVM;     // VM currently in use
   int               iVScroll;   // amount to scroll
   DWORD             dwFlag;  // meaning changes depending upon dialog
   PCListVariable    plPropPub;     // list of public property names for ESCN_FILTEREDLISTQUERY
   PCListVariable    plMethPub;     // list of public method names for ESCN_FILTEREDLISTQUERY
   PCListVariable    plObject;      // list of object names for ESCN_FILTEREDLISTQUERY
   PCListVariable    plClass;       // list of object names for ESCN_FILTEREDLISTQUERY
   PWSTR             pszErrLink; // used by pages that include compiling, this is the link back to themselves if error'
   DWORD             *pdwTab;   // tab being used, pointer to the DWORD containing it, so can self modify
} MIFLPAGE, *PMIFLPAGE;

// CMIFLAppSocket - for callbacks into the app
class CMIFLAppSocket {
public:
   virtual DWORD LibraryNum (void) = 0;
      // called into the app to return the number of built-in libraries

   virtual BOOL LibraryEnum (DWORD dwNum, PMASLIB pLib) = 0;
      // Called into the app to enumerate the libraries from the resource.
      // pLib is filled in by the app. Returns TRUE if success. FALSE if error

   virtual DWORD ResourceNum (void) = 0;
      // this should return the number of resource types that the app supports

   virtual BOOL ResourceEnum (DWORD dwNum, PMASRES pRes) = 0;
      // Called into the app to enumearate the resources types supported.
      // pRes is filled in by the app. Returnes TRUE if success, FALSE if error.
      // dwNum is from 0 .. ResourceNum()-1

   virtual PCMMLNode2 ResourceEdit (HWND hWnd, PWSTR pszType, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
      PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes) = 0;
      // Called by the MIFL code editing when a resource is being edited or viewed.
      // hWnd is the window to use for the dialog's parent. pszType is the resource type/name,
      // as enumerated in ResourceEnum. pIn is the pre-existing MMLNode structure that the
      // resource starts out with; this should NOT be modified. fReadOnly indicates if the data
      // is read only (in which case don't allow the user to modify it).
      // lid is the language that the resource editing is associated with.
      // pProj can be used to access project information for the resource edit UI
      // Returns a new PCMMLNode2 with all the modifications. This will be freed by the caller.
      // if no modifications were made then this returns NULL. pProj and pLib and pRes contain the resource
   
   virtual BOOL FileSysSupport (void) = 0;
      // The application should return FALSE if it doesn't support a custom file system,
      // and hence all library names are assumed to be actual files on disk. If it does
      // support a custom file system, it returns TRUE. This will cause the library
      // and project code to use FileSysLibOpenDialog(), FileSysSave(), and FileSysOpen()
      // instead of accessing the disk. Apps can use this functionality to remap the
      // location of where dialogs are saved.

   virtual BOOL FileSysLibOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave) = 0;
      // If FileSysSupport() is TRUE, this sill be called to show UI for saving of loading
      // of a library file. hWnd is the window to bring the dialog up from. pszFile is the file
      // name. dwChars is the number of characters capable of being stored in pszFile.
      // fSave is TRUE if pulling up save dialog, FALSE if pulling up load dialog.
      // The app should display its UI. If the user presses cancel return FALSE.
      // If a file is selected, enter the name in pszFile and return TRUE

   virtual PCMMLNode2 FileSysOpen (PWSTR pszFile) = 0;
      // If FileSysSupport() is TRUE, this will be used to open lib files from the apps
      // internal cache. pszFile is the name (from FIleSysLibOpenDialog()). This should
      // return the MMLNode for the data, or NULL if it fails to open. The MMLNode will
      // be freed by the caller

   virtual BOOL FileSysSave (PWSTR pszFile, PCMMLNode2 pNode) = 0;
      // If FileSysSupport() is TRUE, this will be called to save the lib data to the apps
      // internal cache. pszFile is the name (from FileSysLibOpenDialog()). pNode is the
      // data, which should be copied and saved. This returns TRUE if the save is
      // succesful, FALSE if it fails

   virtual BOOL FuncImportIsValid (PWSTR pszName, DWORD *pdwParams) = 0;
      // While compiling, this is called to see if the imported code is supprted by
      // the application. If it isn't , this should return FALSE. If it is
      // then it returns TRUE. pdwParams is filled in with the number of parameters
      // expected, or -1 if any number are acceptable.
      // NOTE: pszName will always be a lower-case version of the name

   virtual BOOL FuncImport (PWSTR pszName, PCMIFLVM pVM, PCMIFLVMObject pObject,
      PCMIFLVarList plParams, DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVar pRet) = 0;
      // This is called when the MIFL script calls an imported function.
      // pszName is the name (all lower case). pVM is the virtual machine its
      // running in. pObject is the object that it's in, or NULL if it's a function
      // and not a method. plParams is a list with all the parameters.
      // dwCharStart and dwCharEnd are the character's location so can report
      // an error using RunTimeError().
      // pRet should be filled in with the return value of the function.
      // Return TRUE if the function exists, FALSE if it's unknown.


   virtual BOOL TestQuery (void) = 0;
      // This is called to see if the program supports "test" functionality
      // to be run in the editor. The app should return TRUE if it supports test
      // functionality, FALSE if it doesn't.

   virtual void TestVMNew (PCMIFLVM pVM) = 0;
      // Called to tell the application that a new VM has been created that will
      // shortly be used for testing. The order of calls is usually
      // TestVMNew(), TestVMPageEnter(), TestVMPageLeave(), repeat page enter/leave, TestVMDelete().
      // NOTE: This may be called before the VM is fully initialized, so that the
      // supporting app has a heads up that it may be called

   virtual void TestVMPageEnter (PCMIFLVM pVM) = 0;
      // Called to tell the application that the test page has been entered, in which
      // case the application may wish to show specific UI.

   virtual void TestVMPageLeave (PCMIFLVM pVM) = 0;
      // Called to tell the application that the test page has been left, in which
      // case the application may wish to hide an UI shown by TestPageVMEnter.

   virtual void TestVMDelete (PCMIFLVM pVM) = 0;
      // Informs the application that the VM has been deleted.

   virtual void VMObjectDelete (PCMIFLVM pVM, GUID *pgID) = 0;
      // informs the application that an object in the VM is about to be
      // deleted. This allows for cleanup of databases. NOTE: This is only
      // called if "delete XXX" is called, or equivalent. It is NOT called
      // if everything is deleted wholesale.
      // If an object and children are deleted, the code for deleting is
      // generall designed so the children will be deleted first.

   virtual void VMTimerToBeCalled (PCMIFLVM pVM) = 0;
      // informs the application that a timer is about to be called.
      // the application can use this to keep CPU metrics.
      // This will be called several times when MaintinenceTimer()
      // is called since several timers might be hit at once

   virtual void Log (PCMIFLVM pVM, PWSTR psz) = 0;
      // Informs the application that CMIFLVM::OutputDebugString() has been called,
      // potentially because of run-time error or other logging, and that the
      // application might wish to log it

   virtual void MenuEnum (PCMIFLProj pProj, PCMIFLLib pLib, PCListVariable plMenu) = 0;
      // fills in plMenu with app-specific "misc" menu options.

   virtual BOOL MenuCall (PCMIFLProj pProj, PCMIFLLib pLib, PCEscWindow pWindow, DWORD dwIndex) = 0;
      // called when the player selects a misc menu (from MenuEnum()), pProj is the
      // curent VM. pLib is the current library being edited. pWindow is the window
      // where the UI can be displayed. dwIndex is the index into MenuEnum() that was selected
      // Returns TRUE if back, FALSE if pressed close
};
typedef CMIFLAppSocket *PCMIFLAppSocket;



/*************************************************************************************
CMIFLString */

class DLLEXPORT CMIFLString {
public:
   ESCNEWDELETE;

   CMIFLString (void);
   ~CMIFLString (void);

   void Clear();
   BOOL CloneTo (CMIFLString *pTo, LANGID lidKeep = (LANGID)-1);
   CMIFLString *Clone (LANGID lidKeep = (LANGID)-1);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   BOOL DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);
   BOOL LangRemove (LANGID lid, PCMIFLLib pLib);
   PWSTR Get (LANGID lid, DWORD *pdwLength = NULL);
   DWORD FindExact (LANGID lid, BOOL fExactSecondary);

   // variables
   CMem           m_memName;        // pointer to CMem containing the name
   CListFixed     m_lLANGID;        // list of language IDs for string, same # elems as in m_lString
   CListVariable  m_lString;        // list of strings, PWSTR
   DWORD          m_dwTempID;       // temporary ID used for going Back in pages
   DWORD          m_dwLibOrigFrom;  // library m_dwTempID that originally from
};
typedef CMIFLString *PCMIFLString;


/*************************************************************************************
CMIFLResource */

class DLLEXPORT CMIFLResource {
public:
   ESCNEWDELETE;

   CMIFLResource (void);
   ~CMIFLResource (void);

   void Clear();
   BOOL CloneTo (CMIFLResource *pTo, BOOL fKeepDocs = TRUE, LANGID lidKeep = (LANGID)-1);
   CMIFLResource *Clone (BOOL fKeepDocs = TRUE, LANGID lidKeep = (LANGID)-1);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   BOOL DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);
   BOOL LangRemove (LANGID lid, PCMIFLLib pLib);
   PCMMLNode2 Get (LANGID lid);
   PWSTR GetAsString (LANGID lid, DWORD *pdwLength);

   DWORD TransProsQuickDialog (PCMIFLLib pLib, PCEscWindow pWindow, HWND hWnd,
                                      LANGID lid, PWSTR pszCreatedBy, PWSTR pszText);
   BOOL TransProsQuickText (LANGID lid, PWSTR *ppszOrigText, PWSTR *ppszPreModText);

   // variables
   CMem           m_memName;        // pointer to CMem containing the name
   CMem           m_memDescShort;   // short description
   CMem           m_memType;        // resource type. NOTE: NO SPACES ALLOWED
   CListFixed     m_lLANGID;        // list of language IDs for Resource, same # elems as in m_lPCMMLNode2
   CListFixed     m_lPCMMLNode2;     // list of pointers to MML nodes, which are the resources
   CListVariable  m_lAsString;      // PCMMLNOde2 as a string
   DWORD          m_dwTempID;       // temporary ID used for going Back in pages
   DWORD          m_dwLibOrigFrom;  // library m_dwTempID that originally from
};
typedef CMIFLResource *PCMIFLResource;

/************************************************************************************
CMIFLProp */
class DLLEXPORT CMIFLProp {
public:
   ESCNEWDELETE;

   CMIFLProp (void);
   ~CMIFLProp (void);

   void Clear();
   BOOL CloneTo (CMIFLProp *pTo, BOOL fKeepDocs = TRUE);
   CMIFLProp *Clone (BOOL fKeepDocs = TRUE);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   void CodeDefault (void);
   void CodeClear (void);

   BOOL DialogEditSinglePropDef (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);
   BOOL DialogEditSingleGlobal (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);

   void PageInitPage (PCEscPage pPage, PMIFLPAGE pmp);
   BOOL PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib, BOOL *pfChangedName);
   BOOL PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib);

   // varaibles
   CMem           m_memName;        // pointer to CMem containing the name
   CMem           m_memDescShort;   // pointer to CMem containing the description. Might be NULL
   CMem           m_memDescLong;    // long description
   CMem           m_aMemHelp[2];    // memory used for the help category, 1 and 2
   CMem           m_memInit;        // pointer to CMem containing the value that initialized to. Might be NULL.
   DWORD          m_dwTempID;             // temporary ID used for going Back in pages
   PCMIFLCode     m_pCodeGet;       // code used for get
   PCMIFLCode     m_pCodeSet;       // code used for set
   DWORD          m_dwLibOrigFrom;  // library m_dwTempID that originally from

   // used for deltas when saving
   PCMIFLObject   m_pObjectFrom;    // class that this came from so can backtrack it
};
typedef CMIFLProp *PCMIFLProp;



/************************************************************************************
CMIFLDoc */
class DLLEXPORT CMIFLDoc {
public:
   ESCNEWDELETE;

   CMIFLDoc (void);
   ~CMIFLDoc (void);

   void Clear();
   BOOL CloneTo (CMIFLDoc *pTo);
   CMIFLDoc *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   BOOL DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);

   void PageInitPage (PCEscPage pPage, PMIFLPAGE pmp);
   BOOL PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib, BOOL *pfChangedName);
   BOOL PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib);

   // varaibles
   CMem           m_memName;        // pointer to CMem containing the name
   CMem           m_memDescShort;   // pointer to CMem containing the description. Might be NULL
   CMem           m_memDescLong;    // long description
   CMem           m_aMemHelp[2];    // memory used for the help category, 1 and 2
   DWORD          m_dwTempID;             // temporary ID used for going Back in pages
   BOOL           m_fUseMML;        // if checked then use MML conversion
   DWORD          m_dwLibOrigFrom;  // library m_dwTempID that originally from
};
typedef CMIFLDoc *PCMIFLDoc;


/************************************************************************************
CMIFLCode */
class DLLEXPORT CMIFLCode {
public:
   ESCNEWDELETE;

   CMIFLCode (void);
   ~CMIFLCode (void);

   void Clear();
   BOOL CloneTo (CMIFLCode *pTo);
   CMIFLCode *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   
   void PageInitPage (PCEscPage pPage, PMIFLPAGE pmp);
   BOOL PageSubstitution (PCEscPage pPage, PMIFLPAGE pmp, PESCMSUBSTITUTION p);
   BOOL PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib);
   BOOL PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib);

   // varaibles
   // CListFixed     m_lPCMIFLPropVar; // list of PCMIFLProp's that are used to store the variables
   CMem           m_memCode;        // pointer to CMem containing the name

   // generated by compiling
   CHashString    m_hVars;          // list of local variables. 0-sized elem.
                                    // Elem 0 = this, elem 1= arguements list. elem2+ = individual arguements, then vars
   CMem           m_memMIFLCOMP;    // memory containing the compressed code
   DWORD          m_dwParamCount;   // number of parameters that function is compiled for.
                                    // need this because meaning of m_hVars depends on this
   PCMIFLObject   m_pObjectLayer;   // so when debugging know which layer of the object came from
                                    // This is also used to determine saving for get/set
   PWSTR          m_pszCodeName;    // used by debugging to display the name
   DWORD          m_dwCodeFrom;     // what the code is from... 0 = function, 1 = global get, 2 = global set,
                                    // 3 = method, 4= method get, 5=method set, 6 for unknown
                                    // if 3,4,5 then m_pObjectLayer is the object it's from
                                    // m_pszCodeName is the name of the entity for 0..5
};
typedef CMIFLCode *PCMIFLCode;



/*************************************************************************************
CMIFLMeth */

class DLLEXPORT CMIFLMeth {
public:
   ESCNEWDELETE;

   CMIFLMeth (void);
   ~CMIFLMeth (void);

   void Clear (void);
   BOOL CloneTo (CMIFLMeth *pTo, BOOL fKeepDocs = TRUE);
   CMIFLMeth *Clone (BOOL fKeepDocs = TRUE);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   void KillDead (PCMIFLLib pLib, BOOL fLowerCase);
   void InitAsGetSet (PWSTR pszPropName, BOOL fGet, BOOL fDocs);
   void InitCodeVars (PCMIFLCode pCode);

   //void ParentSet (PCMIFLLib pLib);
   //PCMIFLLib ParentGetLib (void);
   //PCMIFLLib ParentGetOnlyLib (void);

   BOOL DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);
   void MemCatParam (PCMem pMem, BOOL fSpaceBefore = TRUE);

   void PageInitPage (PCEscPage pPage, PMIFLPAGE pmp);
   BOOL PageSubstitution (PCEscPage pPage, PMIFLPAGE pmp, PESCMSUBSTITUTION p, PCMIFLLib pLib, BOOL fRO);
   BOOL PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib, BOOL *pfChangedName);
   BOOL PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib);
   BOOL PageComboBoxSelChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNCOMBOBOXSELCHANGE p, PCMIFLLib pLib);

   // variables
   CMem                    m_memName;  // memory used for the name
   CMem                    m_memDescShort;   // memory used for the short description
   CMem                    m_memDescLong;    // memory used for the long description
   CMem                    m_aMemHelp[2];    // memory used for the help category, 1 and 2
   CListFixed              m_lPCMIFLProp;   // list of PCMIFLProp structures for the inputs, must be freed when called
   CMIFLProp               m_mpRet;          // describing the return value
   BOOL                    m_fParamAnyNum;   // set to TRUE if method can accept any number of paramters
   BOOL                    m_fCommonAll;     // used only for global, if TRUE then method common to all objects
   DWORD                   m_dwOverride;     // 0 => call only highest pri method,
                                             // Stop on not-undefined: 1 => all methods from high to low, 2=> all methods from low to high,
                                             // Keep going: 3 => all methods from high to low, 4=> all methods from low to high,
      

   DWORD                   m_dwTempID;             // temporary ID used for going Back in pages
   DWORD                   m_dwLibOrigFrom;  // library m_dwTempID that originally from

private:
   //PCMIFLLib               m_pLib;     // if parent is library directly, then points to library

};
typedef CMIFLMeth * PCMIFLMeth;




/*************************************************************************************
CMIFLObject */

class DLLEXPORT CMIFLObject {
public:
   ESCNEWDELETE;

   CMIFLObject (void);
   ~CMIFLObject (void);

   void Clear (void);
   void ClearPropDefault (void);
   BOOL CloneTo (CMIFLObject *pTo, BOOL fKeepDocs = TRUE);
   CMIFLObject *Clone (BOOL fKeepDocs = TRUE);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   //void ParentSet (PCMIFLLib pLib);
   //PCMIFLLib ParentGetLib (void);
   //PCMIFLLib ParentGetOnlyLib (void);
   BOOL Merge (PCMIFLObject *ppo, DWORD *padwLibOrig, DWORD dwNum, BOOL fKeepDocs, PCMIFLErrors pErr);
   void KillDead (PCMIFLLib pLib, BOOL fLowerCase);

   BOOL DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);

   void PageInitPage (PCEscPage pPage, PMIFLPAGE pmp);
   BOOL PageSubstitution (PCEscPage pPage, PMIFLPAGE pmp, PESCMSUBSTITUTION p, PCMIFLLib pLib);
   BOOL PageEditChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNEDITCHANGE p, PCMIFLLib pLib, BOOL *pfChangedName);
   BOOL PageButtonPress (PCEscPage pPage, PMIFLPAGE pmp, PESCNBUTTONPRESS p, PCMIFLLib pLib);
   BOOL PageLink (PCEscPage pPage, PMIFLPAGE pmp, PESCMLINK p, PCMIFLLib pLib);
   BOOL PageFilteredListChange (PCEscPage pPage, PMIFLPAGE pmp, PESCNFILTEREDLISTCHANGE p, PCMIFLLib pLib);

   void MethPrivSort (void);
   DWORD MethPrivNew (PCMIFLLib pLib);
   DWORD MethPrivFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD MethPrivFind (DWORD dwID);
   BOOL MethPrivRemove (DWORD dwID, PCMIFLLib pLib);
   DWORD MethPrivAddCopy (PCMIFLFunc pMethPriv, PCMIFLLib pLib);
   PCMIFLFunc MethPrivGet (DWORD dwIndex);
   DWORD MethPrivNum (void);

   void MethPubSort (void);
   DWORD MethPubNew (PWSTR pszName, PCMIFLLib pLib);
   DWORD MethPubFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD MethPubFind (DWORD dwID);
   BOOL MethPubRemove (DWORD dwID, PCMIFLLib pLib);
   DWORD MethPubAddCopy (PCMIFLFunc pMethPub, PCMIFLLib pLib);
   PCMIFLFunc MethPubGet (DWORD dwIndex);
   DWORD MethPubNum (void);

   BOOL ClassAddWithRecommend (PWSTR pszClass, BOOL fWithRecommend, PCMIFLProj pProj);

   // variables
   CMem                    m_memName;  // memory used for the name
   CMem                    m_memDescShort;   // memory used for the short description
   CMem                    m_memDescLong;    // memory used for the long description
   CMem                    m_aMemHelp[2];    // memory used for the help category, 1 and 2
   CMem                    m_memContained;   // memory used to store what object it's contained in
   BOOL                    m_fAutoCreate;    // set to TRUE if object should be automatically created from class
   BOOL                    m_dwInNewObjectMenu;  // 0 for not in menu, 1 if the object is in the new object menu as object, 2 if as sub-class

   DWORD                   m_dwTempID;             // temporary ID used for going Back in pages
   DWORD                   m_dwLibOrigFrom;  // library m_dwTempID that originally from
   GUID                    m_gID;            // GUID for object ID, or GUID_NULL if not change what overrwrite

   // sub-objects
   CListFixed              m_lPCMIFLPropPriv; // list of private properties in library. NOT sorted
   CListFixed              m_lPCMIFLPropPub; // list of public properties in library. NOT sorted
   CListVariable           m_lClassSuper;    // list of classes that are super-classes to this. Containes strings
   CListVariable           m_lRecMeth;       // list of recommended methods for anything that's a sub-class
   CListVariable           m_lRecProp;       // list of reocmmended properties for anything that's a sub-class

   // derived information, generated by the compile functionality. Not cloned or anything
   // because only derived on compile
   CListVariable           m_lClassSuperAll; // from ObjectAllSuper
   CListFixed              m_lClassSuperPCMIFLObject; // same info as in m_lClassSuper, but pointers to the objects
   CListFixed              m_lClassSuperAllPCMIFLObject; // same infor as m_lClassSuperAll, but pointer to the objects
   PCMIFLObject            m_pContainedIn;   // pointer to object definition that directly contained in
   CHashString             m_hPrivIdentity;  // has of private identities (methods and properties). Stored as MIFLIDENT
   CHashDWORD              m_hUnPrivIdentity;   // go from dwID to MIFLIDENT for methods and properties
   CHashDWORD              m_hPropDefaultJustThis;   // default property values for only properties specifically stated
                                                // in this object, of CMIFLVarProp
   CHashDWORD              m_hPropDefaultAllClass; // property defaults derived from combining all superclasses
                                                   // together. of CMIFLVarProp
   CHashDWORD              m_hMethJustThis;  // hash of method IDs (public and private) supported by the explicitely
                                             // stated methods in the class/object. MIFLIDENT
   CHashDWORD              m_hMethAllClass;  // methods supported by this and all classes. MIFLIDENT
   CListFixed              m_lContainsDefault;  // list of GUIDs to indicate what objects this one contains

private:
   CListFixed              m_lPCMIFLMethPriv; // list of private methods (CMIFLFunc) in object. SORTED
   CListFixed              m_lPCMIFLMethPub;  // list of public methods (CMIFLFunc) in object. SORTED
                                                   // note: only the m_memName of the m_Meth element is used
};
typedef CMIFLObject * PCMIFLObject;

extern void RoomGraphHelpString (PCMIFLLib pLib, PWSTR pszObject, PCMem pMem);


/***************************************************************************************
CMIFLFunc */

class DLLEXPORT CMIFLFunc {
public:
   ESCNEWDELETE;

   CMIFLFunc (void);
   ~CMIFLFunc (void);

   void Clear();
   BOOL CloneTo (CMIFLFunc *pTo, BOOL fKeepDocs = TRUE);
   CMIFLFunc *Clone (BOOL fKeepDocs = TRUE);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   void KillDead (PCMIFLLib pLib, BOOL fLowerCase);

   BOOL DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);
   BOOL DialogEditSingleMethod (BOOL fPriv, PCMIFLLib pLib, PCMIFLObject pObj,
                                        PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);

   // variables
   CMIFLMeth            m_Meth;     // method definition
   CMIFLCode            m_Code;     // code definition

   PCMIFLObject         m_pObjectPrivate; // if this is a private method then will store the
                                    // pointer to the object that created it, so that
                                    // can figure out private method
};
typedef CMIFLFunc *PCMIFLFunc;




/************************************************************************************
CMIFLLib */


class DLLEXPORT CMIFLLib {
public:
   ESCNEWDELETE;

   CMIFLLib (PCMIFLAppSocket pSocket);
   ~CMIFLLib (void);

   void ProjectSet (PCMIFLProj pProject);
   PCMIFLProj ProjectGet (void);

   void Clear (void);
   PCMIFLLib Clone (void);
   BOOL CloneTo (PCMIFLLib pTo);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   BOOL Save (BOOL fForce = FALSE, BOOL fClearDirty = TRUE);
   BOOL Open (PWSTR pszFile);
   BOOL Open (PWSTR pszName, HINSTANCE hInstance, DWORD dwRes);
   BOOL Merge (PCMIFLLib *ppl, DWORD dwNum, BOOL fKeepDocs = TRUE, LANGID lid = (LANGID)-1, PCMIFLErrors pErr = NULL);
   BOOL ObjectAllSuper (PWSTR pszName, PCListVariable plList, PWSTR pszRecurse = NULL);
   void KillDead (BOOL fLowerCase);

   void AboutToChange (void);
   void Changed (void);

   void MethDefSort (void);
   DWORD MethDefNew (void);
   DWORD MethDefFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD MethDefFind (DWORD dwID);
   BOOL MethDefRemove (DWORD dwID);
   DWORD MethDefAddCopy (PCMIFLMeth pMeth);
   PCMIFLMeth MethDefGet (DWORD dwIndex);
   DWORD MethDefNum (void);

   void ObjectSort (void);
   DWORD ObjectNew (void);
   DWORD ObjectFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD ObjectFind (DWORD dwID);
   BOOL ObjectRemove (DWORD dwID);
   DWORD ObjectAddCopy (PCMIFLObject pObject, BOOL fNewGUID, BOOL fUniqueName);
   PCMIFLObject ObjectGet (DWORD dwIndex);
   DWORD ObjectNum (void);

   void PropDefSort (void);
   DWORD PropDefNew (void);
   DWORD PropDefFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD PropDefFind (DWORD dwID);
   BOOL PropDefRemove (DWORD dwID);
   DWORD PropDefAddCopy (PCMIFLProp pProp);
   PCMIFLProp PropDefGet (DWORD dwIndex);
   DWORD PropDefNum (void);

   void DocSort (void);
   DWORD DocNew (void);
   DWORD DocFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD DocFind (DWORD dwID);
   BOOL DocRemove (DWORD dwID);
   DWORD DocAddCopy (PCMIFLDoc pDoc);
   PCMIFLDoc DocGet (DWORD dwIndex);
   DWORD DocNum (void);

   void GlobalSort (void);
   DWORD GlobalNew (void);
   DWORD GlobalFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD GlobalFind (DWORD dwID);
   BOOL GlobalRemove (DWORD dwID);
   DWORD GlobalAddCopy (PCMIFLProp pGlobal);
   PCMIFLProp GlobalGet (DWORD dwIndex);
   DWORD GlobalNum (void);

   void FuncSort (void);
   DWORD FuncNew (void);
   DWORD FuncFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD FuncFind (DWORD dwID);
   BOOL FuncRemove (DWORD dwID);
   DWORD FuncAddCopy (PCMIFLFunc pFunc);
   PCMIFLFunc FuncGet (DWORD dwIndex);
   DWORD FuncNum (void);

   void StringSort (void);
   DWORD StringNew (void);
   DWORD StringFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD StringFind (DWORD dwID);
   BOOL StringRemove (DWORD dwID);
   DWORD StringAddCopy (PCMIFLString pString);
   PCMIFLString StringGet (DWORD dwIndex);
   DWORD StringNum (void);

   void ResourceSort (void);
   DWORD ResourceNew (PWSTR pszType, BOOL fRememberForUndo = TRUE);
   DWORD ResourceFind (PWSTR pszName, DWORD dwExcludeID);
   DWORD ResourceFind (DWORD dwID);
   BOOL ResourceRemove (DWORD dwID, BOOL fRememberForUndo = TRUE);
   DWORD ResourceAddCopy (PCMIFLResource pResource);
   PCMIFLResource ResourceGet (DWORD dwIndex);
   DWORD ResourceNum (void);

   BOOL DialogEditSingle (PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll);

   DWORD TransProsQuickFind (PWSTR pszText, LANGID lid);
   DWORD TransProsQuickDialog (PCEscWindow pWindow, HWND hWnd,
                                      LANGID lid, PWSTR pszCreatedBy, PWSTR pszText);
   void TransProsQuickEnum (PCListVariable plText, PCListFixed plLANGID,
                                   LANGID lid, PCListFixed plResID);
   void TransProsQuickDelete (PCListFixed plResIDOrig, PCListFixed plResIDCur);
   BOOL TransProsQuickDeleteUI (PCEscPage pPage, PWSTR pszText, LANGID lid);

   // variables
   WCHAR             m_szFile[256];          // file name, or name of resource
   BOOL              m_fDirty;               // set to TRUE if dirty
   BOOL              m_fReadOnly;            // set to TRUE if read-only
   DWORD             m_dwTempID;             // temporary ID used for going Back in pages
   CMem              m_memDescShort;         // short description, PWSTR
   CMem              m_memDescLong;          // long description, PWSTR
   PCMMLNode2        m_pNodeMisc;            // can change the sub-nodes to store miscellaneous info here.
                                             // if change, make sure to call AboutToChange() and then Changed()

private:
   PCMIFLAppSocket   m_pSocket;        // callback into the app
   PCMIFLProj        m_pProject;       // project that library is in
   CListFixed        m_lPCMIFLMethDef; // list of global method definitions in the library. Sorted by name
   CListFixed        m_lPCMIFLPropDef;   // list of global propery definitions in library. Sorted by name
   CListFixed        m_lPCMIFLPropGlobal; // list of global variables in library
   CListFixed        m_lPCMIFLFunc;    // list of functions in library
   CListFixed        m_lPCMIFLObject;  // list of objects in the library. Sorted by name
   CListFixed        m_lPCMIFLString;  // list of strings in the library. Sorted by name
   CListFixed        m_lPCMIFLResource;  // list of Resources in the library. Sorted by name
   CListFixed        m_lPCMIFLDoc;     // list of documents in the library. Sorted by name
};

DLLEXPORT BOOL MIFLLibOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave);



/************************************************************************************
CMIFLProj */
class CMIFLUndoPacket;
typedef CMIFLUndoPacket *PCMIFLUndoPacket;

// MIFLOER - Used by ObjectEnumRelationship() to describe object
typedef struct {
   BOOL              fSelfRef;      // filled with TRUE if references itself as a superclass (bad)
   PCListVariable    plSuper;       // pointer to a list of PWSTR for the superclasses. This
                                    // MUST be freed. Element 0 is the name of the object iself.
   DWORD             dwSubCount;    // number of times this is a sub-class of another object
} MIFLOER, *PMIFLOER;



// MIFLHISTORY - History of what help pages have been shown
typedef struct {
   WCHAR    szLink[256];   // link name. controls all
   BOOL     fResolved;     // set to TRUE if resolved, FALSE if cant
   int      iVScroll;      // vertical scroll to use. -1 if use szLink
   int      iSection;      // index into szLink for character just after #. -1 if none
   int      iSlot;         // if >= 0, then slot using this, else MML resource using for this.
   DWORD    dwData;        // Database number using for this. -1 if none
} MIFLHISTORY, *PMIFLHISTORY;

// MDM_XXX - Debug mode
#define MDM_IGNOREALL         0        // ignore all problems, to stop for anything, including "debugbreak"
#define MDM_ONRUNTIME         1        // stop on run-time errors only
#define MDM_STEPOVER          2        // used internally, indicates that the current code is stepped over
#define MDM_EVERYLINE         3        // stop on every line
#define MDM_STEPIN            4        // indicates that stepping in, should use this to ensure that enter debug mode

extern PWSTR gpszNodeMisc;


class DLLEXPORT CMIFLProj {
public:
   ESCNEWDELETE;

   CMIFLProj (PCMIFLAppSocket pSocket);
   ~CMIFLProj (void);

   void Clear (void);
   void ClearVM (void);
   CMIFLProj *Clone (void);
   BOOL CloneTo (CMIFLProj *pTo);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, HWND hWnd, PWSTR pszMaster = NULL);

   BOOL Save (BOOL fForce = FALSE, BOOL fClearDirty = TRUE);
   BOOL Open (PWSTR pszFile, HWND hWnd);
   LANGID LangClosest (LANGID lid);
   void SearchAllCode (PWSTR pszFind);

   BOOL LibraryAddDefault (void);
   BOOL LibraryAdd (PWSTR pszName, BOOL fAtTop, BOOL fDontRememberForUndo); // = FALSE);
   BOOL LibraryRemove (PWSTR pszName, BOOL fDontRememberForUndo = FALSE);
   DWORD LibraryCurGet (void);
   BOOL LibraryCurSet (DWORD dwLib);
   PCMIFLLib LibraryGet (DWORD dwLib);
   DWORD LibraryNum (void);
   BOOL LibraryPriority (DWORD dwLib, BOOL fHigh);
   DWORD LibraryFind (PWSTR pszName);
   DWORD LibraryFind (DWORD dwID);
   void LibrarySwap (PCListFixed plLibs);

   PCMIFLMeth MethDefUsed (PWSTR pszName);

   void ObjectEnum (PCListVariable pList, BOOL fIncludeDesc = FALSE, BOOL fSkipClasses = FALSE);
   void ObjectInLib (PWSTR pszName, PCListFixed plLib);
   BOOL ObjectAllSuper (PWSTR pszName, PCListVariable plList, PWSTR pszRecurse = NULL);
   void ObjectEnumRelationship (PCListFixed plList, BOOL fSortByCount);

   void MethDefOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);
   void ObjectOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);
   void PropDefOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);
   void GlobalOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);
   void FuncOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);
   void StringOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);
   void ResourceOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);
   void DocOverridden (DWORD dwLibID, PWSTR pszName, PWSTR *ppszHigher, PWSTR *ppszLower);

   void UndoClear (BOOL fUndo = TRUE, BOOL fRedo = TRUE);
   void UndoRemember (void);
   BOOL UndoQuery (BOOL *pfRedo);
   BOOL Undo (BOOL fUndoIt);
   void ObjectAboutToChange (PCMIFLLib pObject);
   void ObjectChanged (PCMIFLLib pObject);

   BOOL DialogEditSingle (PCEscWindow pWindow, PWSTR pszLink, int *piVScroll);
   BOOL DialogEdit (PCEscWindow pWindow = NULL, HWND hWndParent = NULL, PWSTR pszLink = NULL);

   void Help (PCEscPage pPage, DWORD dwRes);
   BOOL HelpIndex (PCEscPage pPage, PWSTR pszKeyword, BOOL fParse = FALSE);
   BOOL HelpRebuild (HWND hWnd);
   void HelpMML (PCMem pMem, PWSTR pszTopic, BOOL fIndexPrefix = TRUE);
   BOOL HelpInvokeCode (PWSTR pszName);
   void Compile (PCProgress pProgress,
      PCMIFLMeth pMeth = NULL, PCMIFLCode pCode = NULL, PCMIFLObject pObject = NULL, PWSTR pszErrLink = NULL);
   PCMIFLVM VMCreate (PCEscWindow pDebugWindow = NULL, HWND hWndParent = NULL,
                    DWORD dwDebugMode = MDM_IGNOREALL, PCMMLNode2 pNode = NULL);

   // variables
   BOOL              m_fDirty;         // set to TRUE when data in the project is dirty
   WCHAR             m_szFile[256];    // filename
   CMem              m_memDescShort;   // short description, PWSTR
   CMem              m_memDescLong;    // long description, PWSTR
   PCMIFLAppSocket   m_pSocket;        // callback into the app. External objects can reference but shouldnt change
   CListFixed        m_lLANGID;        // list of languages supported by the project, the 1st one has highest priority
   PCMIFLErrors      m_pErrors;           // errors from compile
   BOOL              m_fErrorsAreSearch;  // set to TRUE if m_pErrors is actually search info
   int               m_iErrCur;        // error that just clicked on

   PCMMLNode2        m_pNodeMisc;      // can change the sub-nodes to store miscellaneous info here.
                                       // if change, make sure to set m_fDirty to TRUE

   BOOL              m_fTestStepDebug; // if TRUE then automatically to step-through debugging when testing
   CMem              m_memDebug;       // code to run when debug

   DWORD             m_dwTabFunc;      // which tab is shown, also used for methdef and methpriv
   DWORD             m_dwTabObjEdit;   // tab for object edit
   PCEscPage         m_pPageCurrent;   // current page that on

   BOOL              m_fTransProsQuick;   // if TRUE, then show quick transplanted prosody UI

   // semi-private for help window
   void SearchIndex (PCEscPage pPage);
   void UpdateSearchList (PCEscPage pPage);
   void HelpCheckPage (void);
   PCEscSearch       m_pSearch;             // search
   CListFixed        m_ListHistory;        // history list
   MIFLHISTORY       m_CurHistory;         // current location
   PCEscWindow       m_pHelpWindow;            // main window object
   DWORD             m_dwHelpTimer;   // timer notification so can use help while dialog is up
   BOOL              m_fSmallFont;
   DWORD             m_dwHelpTOC;         // index into m_lHelp that contains the TOC
   CListVariable     m_lHelp;             // list of entries for help, all with strings of MML
   CListVariable     m_lHelpIndex;        // list of entries for help, but the ones that should be indexed
                                          // these entries will have extraneous text cut out
   CHashString       m_hHELPINDEX;        // hash of HELPINDEX information
   CListFixed        m_lPWSTRIndex;       // list of index strings, sorted

private:
   void HelpEnd (void);
   void HelpInit (void);
   void HelpIndexClear (void);
   void HelpIndexGenerate (PCMIFLLib pLib);
   void ShowPage (PWSTR psz);
   PESCPAGECALLBACK DetermineCallback (DWORD dwID);
   void ParseURL (PWSTR pszURL, PMIFLHISTORY pHistory);

   CListFixed        m_lPCMIFLLib;     // pointer to an array of mifl libraries

   CListFixed        m_listPCUndoPacket;  // list of undo packets. Lowest # index are oldest
   CListFixed        m_listPCRedoPacket;  // list of PCUndoPacket. Lowest # index are oldest
   PCMIFLUndoPacket  m_pUndoCur;          // current undo that writing into
   DWORD             m_dwLibCur;          // current library index

   // store compiled information
   PCMIFLCompiled    m_pCompiled;      // latest compiled code
   PCMIFLVM          m_pVM;            // latest VM used for testing


};
typedef class CMIFLProj * PCMIFLProj;

DLLEXPORT BOOL MIFLProjOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave,
                                   BOOL fAllowMIF = FALSE);


/*************************************************************************************
CMIFLCompiled */


// misc
#define TOKEN_NOP             001      // do nothing
#define TOKEN_ARG1            002      // 1st arguement call. for example "for (x=1;x<3;x++) help;".
                                       // ARG1 = "x=1", ARG2 = "x<3", ARG3 = "x++", ARG4 = "help"
#define TOKEN_ARG2            003
#define TOKEN_ARG3            004
#define TOKEN_ARG4            005

// values
#define TOKEN_VALUESTART      100
#define TOKEN_DOUBLE          100      // double
#define TOKEN_CHAR            101      // character
#define TOKEN_STRING          102      // string
#define TOKEN_NULL            103      // valid type of value
#define TOKEN_UNDEFINED       104      // valid type of value, DOCUMENT: Undefined
#define TOKEN_BOOL            105      // boolean. dwValue is 0 or 1
#define TOKEN_STRINGTABLE     106      // dwValue is reference to string table
#define TOKEN_RESOURCE        107      // dwValue is reference to resource
#define TOKEN_METHPUB         108      // calling a public method. dwValue = method ID
#define TOKEN_PROPPUB         109      // accessing a public property. dwValue = prop ID
#define TOKEN_FUNCTION        110      // accessing a global function. dwValue = function ID
#define TOKEN_GLOBAL          111      // accessing a global variable. dwValue = global ID
#define TOKEN_CLASS           112      // accessing a class. dwValue = class ID. Used only after new token
#define TOKEN_METHPRIV        113      // calling a public method. dwValue = method ID
#define TOKEN_PROPPRIV        114      // accessing a private property. dwValue = prop ID
#define TOKEN_VARIABLE        115      // accessing a private vairalbe. dwValue = varialbe ID
#define TOKEN_LISTDEF         116      // define a list. Within is either one expression,
                                       // or a _COMMA with _ARG1 below, each with multiple expressions
#define TOKEN_LISTINDEX       117      // index into an array. Below is ARG1 with the variable name,
                                       // and ARG2 with the expression to access the array
#define TOKEN_FUNCCALL        118      // make a function call. Below, ARG1 is the function to access,
                                       // ARG2 are the paremters.
#define TOKEN_VALUEEND        120      // so can test if in range of values


// identifier tokens
#define TOKEN_IF              201      // for if statement
#define TOKEN_THEN            202      // for if statement
#define TOKEN_ELSE            203      // for if statement
#define TOKEN_FOR             204      // for statement
#define TOKEN_BREAK           205
#define TOKEN_CONTINUE        206
#define TOKEN_CASE            207
#define TOKEN_DEFAULT         208
#define TOKEN_DO              209
#define TOKEN_RETURN          210
#define TOKEN_SWITCH          211
#define TOKEN_WHILE           212
#define TOKEN_DEBUG           213

#define TOKEN_DELETE          221
#define TOKEN_VAR             222      // DOCUMENT: Define variable

#define TOKEN_TRUE            232
#define TOKEN_FALSE           233

// operators
#define TOKEN_OPER_START      300      // where operators start

#define TOKEN_OPER_DOT        300      // .

#define TOKEN_OPER_PLUSPLUS   310      // ++
#define TOKEN_OPER_MINUSMINUS 311      // --
#define TOKEN_OPER_NEGATION   312      // -
#define TOKEN_OPER_BITWISENOT 313      // ~
#define TOKEN_OPER_LOGICALNOT 314      // !
#define TOKEN_OPER_NEW        315

#define TOKEN_OPER_MULTIPLY   320      // *
#define TOKEN_OPER_DIVIDE     321      // /
#define TOKEN_OPER_MODULO     322      // %
#define TOKEN_OPER_ADD        323      // +
#define TOKEN_OPER_SUBTRACT   324      // -

#define TOKEN_OPER_BITWISELEFT 330     // <<
#define TOKEN_OPER_BITWISERIGHT 331    // >>
#define TOKEN_OPER_LESSTHAN   332      // <
#define TOKEN_OPER_LESSTHANEQUAL 333      // <=
#define TOKEN_OPER_GREATERTHAN 334      // >
#define TOKEN_OPER_GREATERTHANEQUAL 335 // >=
#define TOKEN_OPER_EQUALITY   337      // ==
#define TOKEN_OPER_NOTEQUAL   338      // !=
#define TOKEN_OPER_EQUALITYSTRICT 339      // ===     DOCUMENT: Strict equality
#define TOKEN_OPER_NOTEQUALSTRICT 340      // !==     DOCUMENT: Strict non-equality

#define TOKEN_OPER_BITWISEAND 350      // &
#define TOKEN_OPER_BITWISEXOR 351      // ^
#define TOKEN_OPER_BITWISEOR  352      // |
#define TOKEN_OPER_LOGICALAND 353      // &&
#define TOKEN_OPER_LOGICALOR  354      // ||

#define TOKEN_OPER_CONDITIONAL 360      // ?

#define TOKEN_OPER_ASSIGN     370      // =
#define TOKEN_OPER_ASSIGNADD  371      // +=
#define TOKEN_OPER_ASSIGNSUBTRACT 372  // -=
#define TOKEN_OPER_ASSIGNMULTIPLY 373  // *=
#define TOKEN_OPER_ASSIGNDIVIDE 374    // /=
#define TOKEN_OPER_ASSIGNMODULO 375    // %=
#define TOKEN_OPER_ASSIGNBITWISELEFT 376    // <<=
#define TOKEN_OPER_ASSIGNBITWISERIGHT 378    // >>=
#define TOKEN_OPER_ASSIGNBITWISEAND 379 // &=
#define TOKEN_OPER_ASSIGNBITWISEXOR 380 // ^=
#define TOKEN_OPER_ASSIGNBITWISEOR  381 // |=

#define TOKEN_OPER_END        399      // where operators end


// operators that shouldnt end up with once finished...
#define TOKEN_IDENTIFIER      500      // NOTE: Should always resolve to something... store identifier that is not another token
#define TOKEN_OPER_BRACKETL   501      // [
#define TOKEN_OPER_BRACKETR   502      // ]
#define TOKEN_OPER_BRACEL     503      // {
#define TOKEN_OPER_BRACER     504      // }
#define TOKEN_OPER_PARENL     505      // (
#define TOKEN_OPER_PARENR     506      // )
#define TOKEN_OPER_NUMSIGN    507      // #  - used for #define
#define TOKEN_OPER_COLON      510      // :
#define TOKEN_OPER_SEMICOLON  511      // ;
#define TOKEN_OPER_COMMA      512      // ,


// MIFLERR - Store error/warning away for compile
typedef struct {
   PWSTR             pszDisplay;       // error message to display
   PWSTR             pszLink;          // link used if press
   PWSTR             pszObject;        // object named in the error
   BOOL              fError;           // if TRUE it's an error and prevents progress, if FALSE warning
   DWORD             dwStartChar;      // start char where error occurs, in code
   DWORD             dwEndChar;        // end char of error in code
} MIFLERR, *PMIFLERR;

class DLLEXPORT CMIFLErrors {
public:
   ESCNEWDELETE;

   CMIFLErrors (void);
   ~CMIFLErrors (void);

   void Clear (void);
   DWORD Num (void);
   PMIFLERR Get (DWORD dwIndex);
   BOOL TooManyErrors (void);

   void Add (PWSTR pszDisplay, PWSTR pszLink, PWSTR pszObject, BOOL fError,
      DWORD dwStartChar = 0, DWORD dwEndChar = 0);
   void Add (PWSTR pszDisplay, DWORD dwLibOrigID, PCMIFLObject pObject, BOOL fError);
   void Add (PWSTR pszDisplay, PCMIFLProj pProj, BOOL fError);

   DWORD                m_dwNumError;  // keeps a count of the number of errors, vs. number of warnings

private:
   CListFixed           m_lMIFLERR;    // list of errors
   CListVariable        m_lDisplay;    // list of strings for the pszDisplay of MIFLERR
   CListVariable        m_lLink;       // link used for the pszLink of MIFLERR
   CListVariable        m_lObject;     // list of object names in MIFLERRf
};
typedef CMIFLErrors *PCMIFLErrors;



// MIFLIDENT - Structure that is used in hash to go from name to compiled identifier
#define MIFLI_TOKEN              0     // identifier is a token
#define MIFLI_METHDEF            1     // identifier is a method definition
#define MIFLI_PROPDEF            2     // property definition
#define MIFLI_GLOBAL             3     // global variale (only, not object class)
#define MIFLI_FUNC               4     // function
#define MIFLI_OBJECT             5     // class or object
#define MIFLI_STRING             6     // string
#define MIFLI_RESOURCE           7     // resource
#define MIFLI_METHPRIV           8     // private method
#define MIFLI_PROPPRIV           9     // private property

typedef struct {
   DWORD          dwType;     // type of identifier, MIFLI_XXX
   PVOID          pEntity;    // pointer to the CMIFLXXX object, depends upon dwType
   DWORD          dwIndex;    // index into CMIFLLib's list, based on dwType
   DWORD          dwID;       // DWORD ID used to find the entity at run-time
} MIFLIDENT, *PMIFLIDENT;


// MIFLTOKEN - results from tokenizing the code
typedef struct {
   DWORD             dwType;     // type. TOKEN_XXX
   PWSTR             pszString;  // string associated with token. Must be freed with ESCFREE()
   union {
      double         fValue;     // floating point value, for TOKEN_DOUBLE
      DWORD          dwValue;    // DWORD value, depends on TOKEN_XXX
      WCHAR          cValue;     // char value, for TOKEN_CHAR
   };

   DWORD             dwCharStart;   // character at start of token
   DWORD             dwCharEnd;     // character at end of token

   // where to next
   PVOID             pNext;      // next token, PMIFLTOKEN
   PVOID             pDown;      // used for comma separators, PMIFLTOKEN
   PVOID             pBack;      // work way backwards
} MIFLTOKEN, *PMIFLTOKEN;

// MIFLCOMP - compiled version of tokens which takes less space
typedef struct {
   DWORD             dwType;     // type. TOKEN_XXX
   DWORD             dwValue;    // meaning depends upon dwType. Might even mean an index into memory
   DWORD             dwNext;     // index into memory, or 0
   DWORD             dwDown;     // index into memory, or 0
   WORD              wCharStart; // character at start of token
   WORD              wCharEnd;   // character at end of token
} MIFLCOMP, *PMIFLCOMP;


// CMIFLCompiled - Store the compiled information away
class DLLEXPORT CMIFLCompiled {
public:
   ESCNEWDELETE;

   CMIFLCompiled (void);
   ~CMIFLCompiled (void);

   void Clear(void);

   PCMIFLErrors Compile (PCProgress pProgress, PCMIFLProj pProj,
      PCMIFLMeth pMeth = NULL, PCMIFLCode pCode = NULL, PCMIFLObject pObject = NULL,
      PWSTR pszErrLink = NULL);
   void CompileCode (PCMIFLErrors pErr, PCMIFLMeth pMeth, PCHashString phVars,
                                 BOOL fCanAddVars, PWSTR pszCode, BOOL fAddSemi, PCMem pCodeComp,
                                 PCMIFLObject pObject, PWSTR pszErrLink, PWSTR pszMethName);


   LANGID               m_LangID;      // language ID finally chosen by the compile

   // can read but dont change
   PCMIFLLib            m_pLib;         // library that contained the merged info from the project
   CHashDWORD           m_hUnIdentifiers; // take a DWORD ID, and point back to the identifiers info
   PCMIFLAppSocket      m_pSocket;     // socket for callbacks, from the project
   CHashDWORD           m_hGlobalsDefault;   // default values (CMIFLVarProp) for each of the globals, based on ID
   CHashGUID            m_hObjectIDs;  // list of automatically created objects. GUID -> object. Of MIFLIDENT
   CHashDWORD           m_hGlobals;    // list of globals. Runtime DWORD ID -> global. MIFLIDENT
   CHashString          m_hIdentifiers;   // identifiers table. Name->Class/object. Of MIFLIDENT

private:
   void CompileAddTokens (void);
   void CompileMethDef (void);
   void CompilePropDef (void);
   void CompileGlobal (void);
   void CompileString (void);
   void CompileResource (void);
   void CompileFunc (void);
   void CompileObject (void);
   void CompileObject2 (void);
   void CompileGlobalCode (void);
   void CompileFuncCode (void);
   void CompileObjectCode (void);
   void CompileCode (PCMIFLMeth pMeth, PCMIFLCode pCode, PCMIFLObject pObject,
      PWSTR pszErrLink, DWORD dwCodeFrom, PWSTR pszCodeName);
   void CompileCodeInternal (PCMIFLMeth pMeth, PCHashString phVars,
                                         BOOL fCanAddVars, PWSTR pszCode,PCMem pCodeComp,
                                         PCMIFLObject pObject, PWSTR pszErrLink, PWSTR pszMethName);
   void VarInitQuick (PCMem pCode, DWORD dwIndex, PCMIFLVar pVar);
   void VarInitQuick (void);
   PMIFLTOKEN Tokenize (PWSTR pszText);

   PMIFLTOKEN TokensParseStatement (PMIFLTOKEN pToken, PMIFLTOKEN *ppParse, DWORD dwFlags);
   PMIFLTOKEN TokensParseExpression (PMIFLTOKEN pToken, PMIFLTOKEN *ppParse, BOOL *pfFoundSemi);
   PMIFLTOKEN TokensArrangeParen (PMIFLTOKEN pToken, DWORD dwTokLeft, DWORD dwTokRight);
   PMIFLTOKEN TokensArrange (PMIFLTOKEN pToken);
   PMIFLTOKEN TokensParseStatements (PMIFLTOKEN pToken, DWORD dwFlags);
   PMIFLTOKEN TokensParseOrderOfOper (PMIFLTOKEN pToken);
   PMIFLTOKEN TokensParseOrderOfOperByRange (PMIFLTOKEN pToken,
      DWORD dwTokenMin, DWORD dwTokenMax, BOOL fFromLeft, int iSides, BOOL fLValueOnLeft = FALSE,
      BOOL fParseParen = TRUE);
   // PMIFLTOKEN TokensParseOrderOfOperParen (PMIFLTOKEN pToken);
   PMIFLTOKEN TokensParseParen (PMIFLTOKEN pCur, PMIFLTOKEN *ppToken);
   PMIFLTOKEN TokensParseOrderOfOperConditional (PMIFLTOKEN pToken);
   void TokensIdentResolve (PMIFLTOKEN pToken);
   DWORD TokensCompact (PMIFLTOKEN pToken, PCMem pMem);
   PMIFLTOKEN TokensIsImport (PMIFLTOKEN pToken);

   BOOL VerifyIdenitifier (PWSTR pszName, DWORD dwLibOrigID, DWORD dwTempID, PWSTR pszLink);
   BOOL CompileMethCheckParam (PCMIFLMeth pMeth, DWORD dwType,
      PCMIFLObject pObject, PWSTR pszErrLink = NULL);

   PCMIFLProj           m_pProj;       // just to remember project
   PCMIFLErrors         m_pErr;        // just to remember errors
   PCProgress           m_pProgress;   // progress bar in use. MIght be null

   DWORD                m_dwIDMethPropCur;     // current ID being used. Incremented each time needed
   PWSTR                m_pszMethName;    // to diplay in errors
   PWSTR                m_pszErrLink;     // to link to if click on err
   PCHashString         m_phCodeVars;  // variables supported by the code. directly from pCode->m_hVars. Can be NULL
   BOOL                 m_fCanAddVars; // set to TRUE if when compiling can add vars
   PCMem                m_pCodeComp;   // memory to fill in compiled code with MIFLCOMP
   PCMIFLMeth           m_pMeth;       // current method
   PCMIFLObject         m_pObject;     // object that code is in (which compiling)
};
typedef CMIFLCompiled *PCMIFLCompiled;

/************************************************************************************
MIFLMisc */

#ifdef _DEBUG
void HackRenameAll (PCMem pMem, PWSTR pszOrig, PWSTR pszNew);
//#define HACKRENAMEALL(x)      HackRenameAll(x, L"cMobile", L"cCharacter");
#define HACKRENAMEALL(x)
#else
#define HACKRENAMEALL(x)
#endif

BOOL MIFLDefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
DWORD MIFLTempID (void);
PWSTR LinkExtractNum (PWSTR pszString, DWORD *pdwNum);
PWSTR LibraryDisplayName (PWSTR pszName);
BOOL MIFLComboLibs (PCEscPage pPage, PWSTR pszName, PWSTR pszButton, PCMIFLProj pProj, DWORD dwExclude);
PWSTR MIFLRedoSamePage (void);
DWORD MIFLFilteredListSearch (PWSTR pszName, PCListVariable pList);
void MIFLFilteredListPropPub (PMIFLPAGE pmp);
void MIFLFilteredListMethPub (PMIFLPAGE pmp, PCMIFLObject pObjExclude);
void MIFLFilteredListObject (PMIFLPAGE pmp, BOOL fSkipClasses);
void MIFLFilteredListClass (PMIFLPAGE pmp, BOOL fSkipClasses);
DLLEXPORT DWORD MIFLLangNum (void);
DLLEXPORT PWSTR MIFLLangGet (DWORD dwIndex, LANGID *pLang);
DLLEXPORT DWORD MIFLLangFind (LANGID lid);
DLLEXPORT BOOL MIFLLangComboBoxSet (PCEscPage pPage, PWSTR pszControl, LANGID lid,
                                    PCMIFLProj pProj);
DWORD MIFLStringArrayComp (PWSTR *ppsz, DWORD dwNum);
DLLEXPORT DWORD MIFLLangMatch (PCListFixed plLANGID, LANGID lid, BOOL fExactPrimary);
void MIFLToLower (PWSTR psz);
DLLEXPORT BOOL MIFLIsNameValid (PWSTR pszName);
PWSTR MIFLTabs (DWORD dwTab, DWORD dwNum, PWSTR *ppsz, PWSTR *ppszHelp);


/*************************************************************************************
CMIFLVMObject */

class CMIFLVM;
typedef CMIFLVM *PCMIFLVM;

// MIFLGETSET - Used to store which properties have get/set functions associated
// with the layer
typedef struct {
   DWORD          dwID;          // ID
   PCMIFLCode     m_pCodeGet;    // get code, or NULL
   PCMIFLCode     m_pCodeSet;    // set code, or NULL
} MIFLGETSET, *PMIFLGETSET;

// CMIFLVMTimer - Store information about the timer
class DLLEXPORT CMIFLVMTimer {
public:
   ESCNEWDELETE;

   CMIFLVMTimer (void);
   ~CMIFLVMTimer (void);

   BOOL CloneTo (PCMIFLVMTimer pTo);
   PCMIFLVMTimer Clone (void);
   PCMMLNode2 MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList);
   BOOL MMLFrom (PCMMLNode2 pNode, GUID *pgObject, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phList,
      PCHashGUID phObjectRemap);

   // variables that can set
   BOOL              m_fRepeating;        // if TRUE then timer repeats until stopped, else only once
   double            m_fTimeRepeat;       // repeat every N seconds
   double            m_fTimeLeft;         // time left (in seconds) before it actually goes off
   double            m_fTimeScale;        // amount to speed up (> 1.0) or slow down (< 1.0) the time progression
                                          // for this timer
   GUID              m_gBelongsTo;        // GUID of the object the timer belongs to
   GUID              m_gCall;             // GUID of the object that will be called when the timer goes off
                                          // if this is NULL the calls a function
   DWORD             m_dwCallID;          // if m_gCall==GUID_NULL this is the function ID to call,
                                          // else it's the method ID
   CMIFLVar          m_varName;           // name. This can be a string or other value, like a number
   CMIFLVar          m_varParams;         // list containing the parameters to pass into the call
};
typedef CMIFLVMTimer *PCMIFLVMTimer;

// CMIFLVMLayer - Layer within an object
class DLLEXPORT CMIFLVMLayer {
public:
   ESCNEWDELETE;

   CMIFLVMLayer (void);
   ~CMIFLVMLayer (void);

   PCMMLNode2 MMLTo (PCMIFLVM pVM, PCHashPVOID phString, PCHashPVOID phList);
   BOOL MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phToList);
   CMIFLVMLayer *CMIFLVMLayer::Clone (void);

   CMem                 m_memName;        // name of the layer
   double               m_fRank;          // rank of the layer, higher numbers are called first.
                                          // NOTE: Dont change without resorting the list of layers
   PCMIFLObject         m_pObject;        // original object/class where methods and
                                          // properties are inhereted from by default

   // the following are extra properties and methods not covered by m_pObject
   // any changed to the following requires a call to LayerMerge()
   CHashDWORD           m_hPropGetSet;    // hash (based on property ID) to know which
                                          // properties will have get/set functions associated with them.
                                          // Array of MIFLGETSET.
   CHashDWORD           m_hMeth;          // has (based on property ID) to know about
                                          // additional methods, that are not covered by m_pObject, MIFLIDENT
private:
};
typedef CMIFLVMLayer *PCMIFLVMLayer;

// CMIFLVMObject - Main object
class DLLEXPORT CMIFLVMObject {
public:
   ESCNEWDELETE;

   CMIFLVMObject (void);
   ~CMIFLVMObject (void);
   void CleanDelete (void);
   PCMMLNode2 MMLTo (PCHashPVOID phString, PCHashPVOID phList);
   BOOL MMLFrom (PCMMLNode2 pNode, PCMIFLVM pVM, PCHashDWORD phString, PCHashDWORD phToList,
      PCHashGUID phObjectRemap);

   CMIFLVMObject *Clone (GUID *pgNew, PCHashGUID phOrigToNew);

   BOOL InitAsAutoCreate (PCMIFLVM pVM, PCMIFLObject pObject);
   BOOL InitAsNew (PCMIFLVM pVM, PCMIFLObject pObject);

   BOOL ContainedBySet (GUID *pgContainer);
   BOOL ContainDisconnectAll (void);

   BOOL LayerAdd (PWSTR pszName, double fRank, PCMIFLObject pObject,
      PCHashDWORD phPropGetSet, PCHashDWORD phMeth);
   BOOL LayerRemove (DWORD dwIndex);
   DWORD LayerNum (void);
   PCMIFLVMLayer LayerGet (DWORD dwIndex);
   BOOL LayerSort (void);
   BOOL LayerMerge (void);
   void LayerClear (void);

   double TimerSuspendGet (void);
   void TimerSuspendSet (double fTimerScale, BOOL fChildren);
   BOOL TimerAdd (PCMIFLVar pVarName, BOOL fRepeating, double fTimeRepeat,
      GUID *pgCall, DWORD dwCallID, PCMIFLVar pVarParams);
   BOOL TimerRemove (PCMIFLVar pVarName);
   void TimerEnum (PCMIFLVar pRet);
   void TimerQuery (PCMIFLVar pVarName, PCMIFLVar pRet);
   DWORD TimerFind (PCMIFLVar pVarName);
   void TimerRemovedByVM (void);
   void TimerRemoveAll (void);

   // can look at but dont change
   GUID                 m_gID;            // object ID

   // can look at but dont change
   CHashDWORD           m_hProp;          // hash of property values, accessed by ID. CMIFLVarProp.
                                          // Note that CMIFLVarProp.m_pCodeGet and m_pCodeSet are synced with current layers
   CHashDWORD           m_hMeth;          // hash of methods (by ID). This is the merged result from all the
                                          // CMIFLVMLayer in the object. Used for fast access.
   GUID                 m_gContainedIn;   // which object this is contained in, or GUID_NULL if not in anything
                                          // NOTE: Only change using ContainedBySet() call
   CListFixed           m_lContains;      // list of GUIDs for objects it contains
                                          // NOTE: Only change using ContainedBySet() call

   BOOL                 m_fTimerSuspended;  // set to TRUE if the timers are suspended
   double               m_fTimerTimeScale;   // how much to scale time for timers for this object. Defaults to 1.0.
   DWORD                m_dwTimerNum;     // number of timers that has in VM's list OR the suspended list
   CListFixed           m_lPCMIFLVMTimer; // list of SUSPENDED timers

private:
   void DeltaLayerVarMerge (PCHashDWORD phMerge);

   PCMIFLVM             m_pVM;            // virtual machine used

   CListFixed           m_lPCMIFLVMLayer; // list of layers for the object. Kept sorted so that
                                          // highest m_fRank (from layers) occur first in list
};
typedef CMIFLVMObject *PCMIFLVMObject;

/*************************************************************************************
CMIFLVM */


// MIFLFCI - Information about internal function call that in
typedef struct {
   // set by the calling func
   PWSTR             pszCode;       // Code (in text form). Necessary in case need to debug.
   PCMem             pMemMIFLCOMP;  // Memory from the compiler
   DWORD             dwParamCount;  // Number of parameters expected (stored in CMIFLCode::m_dwParamCount)
   PCMIFLVarList     plParam;       // List of parameters passed in. The reference count returned will be
                                    // the same as the reference count passed in.
   PCHashString      phVarsString;  // String reference names for debug display. 0-sized elems.
                                    // NULL if not occuring in function call
   PCMIFLVMObject    pObject;       // Object that it's happening in. NULL if not in object
   PCMIFLObject      pObjectLayer;  // Specific object layer it's happening in. If this is
                                    // NULL the layer is guessed
   PWSTR             pszWhere1;     // String indicating where the error is to be found. (if runtime error occurs)
   PWSTR             pszWhere2;     // String indicating where the error is to be found. (if runtime error occurs)
   DWORD             dwPropIDGetSet;// usually this is -1. If the code being run is used to get/set a variable
                                    // then this is the global ID (if pObject==NULL) or the prop ID (if pObject!=NULL)

   // internal - filled in by FunctionCallInternal
   PCMIFLVar         paVars;        // array of variables
   DWORD             dwVarsNum;     // number of variables
   PBYTE             pbMIFLCOMP;    // pointer to pMemMIFLCOMP.p
   BOOL              fOnlyVarAccess; // if TRUE then can only access variables and not run functions, etc.
} MIFLFCI, *PMIFLFCI;

// MFC_XXX - Error returns that indicates how flow of control should continue;
#define MFC_NONE              0        // nothing happened, continue on as normal
#define MFC_RETURN            1        // return was called
#define MFC_BREAK             2        // break was called
#define MFC_CONTINUE          3        // continue was called
#define MFC_REPORTERROR       10       // the caller should report an error
#define MFC_ABORTFUNC         11        // continue aborting until function call is finished


#define VM_CUSTOMIDRANGE         1000000  // where custom IDs start

DLLEXPORT BOOL VMFuncImportIsValid (PWSTR pszName, DWORD *pdwParams);
DLLEXPORT BOOL VMLibraryEnum (PMASLIB pLib);

class DLLEXPORT CMIFLVM {
public:
   ESCNEWDELETE;

   CMIFLVM (void);
   ~CMIFLVM (void);
   BOOL Init (PCMIFLCompiled pCompiled, PCEscWindow pDebugWindow, HWND hWndParent,
      DWORD dwDebugMode = MDM_ONRUNTIME, PCMMLNode2 pNode = NULL);
   void Clear (void);
   PCMMLNode2 MMLTo (BOOL fAll, BOOL fGlobalsAndDel, GUID *pagExclude, DWORD dwExcludeNum, BOOL fExcludeChildren);
   BOOL MMLFrom (BOOL fBlankSlate, BOOL fAll, BOOL fRemap, BOOL fRemapErr, PCMMLNode2 pNode,
      PCHashGUID *pphRemap);

   void MaintinenceDeleteAll (void);
   BOOL MaintinenceTimer (double fTimeSinceLast);

   void OutputDebugString (PWSTR psz);
   void OutputDebugString (PCMIFLVar pVar);
   void OutputDebugStringClear (void);

   PCMIFLVMObject ObjectFind (GUID *pgID);
   BOOL ObjectDelete (GUID *pgID);
   BOOL ObjectDeleteFamily (GUID *pgID);

   DWORD GlobalGet (DWORD dwID, BOOL fIgnoreGetSet, PCMIFLVarLValue pVar);
   DWORD GlobalSet (DWORD dwID, BOOL fIgnoreGetSet, PCMIFLVar pVar);
   BOOL GlobalRemove (DWORD dwID);

   DWORD MethodNameToID (PWSTR pszName);
   DWORD FunctionNameToID (PWSTR pszName);
   DWORD GlobalNameToID (PWSTR pszName);
   BOOL AlreadyInFunction (void);

   DWORD PropertyGet (DWORD dwID, PCMIFLVMObject pObject, BOOL fIgnoreGetSet, PCMIFLVarLValue pVar);
   DWORD PropertySet (DWORD dwID, PCMIFLVMObject pObject, BOOL fIgnoreGetSet, PCMIFLVar pVar);
   BOOL PropertyRemove (DWORD dwID, PCMIFLVMObject pObject);

   DWORD FunctionCall (DWORD dwID, PCMIFLVarList plParam, PCMIFLVarLValue pVar);
   DWORD MethodCall (GUID *pgObject, DWORD dwID, PCMIFLVarList plParam,
      DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar);
   DWORD MethodCallVMTOK (GUID *pgObject, DWORD dwVMTOK, PCMIFLVarList plParam,
      DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar);
   DWORD RunTimeCodeCall (PWSTR pszCode, GUID *pgObject, PCMIFLVarLValue pVar);

   BOOL FuncImport (PWSTR pszName, PCMIFLVMObject pObject,
      PCMIFLVarList plParams, DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pRet);

   void DebugModeSet (DWORD dwMode);
   DWORD DebugModeGet (void);
   void DebugWindowSet (PCEscWindow pWindow);
   void DebugWindowSetParent (HWND hWndParent);
   BOOL DebugPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam, BOOL fAllowScope);

   void TimerAutomatic (DWORD dwInterval);

   PCHashGUID ObjectClone (GUID *pag, DWORD dwNum, PWSTR pszProperty, PCMIFLVar pVarProperty);

   // called from CMIFLVMObject
   void TimerAdd (PCMIFLVMTimer pTimer);
   DWORD TimerFind (GUID *pgOwner, PCMIFLVar pVarName);
   PCMIFLVMTimer TimerRemove (DWORD dwIndex);
   void TimerEnum (GUID *pgOwner, BOOL fRemove, PCListFixed plTimers);
   PCMIFLVMTimer TimerGet (DWORD dwIndex);
   void TimerCallback (void);

   int Rand (void);
   void SRand (DWORD dwSeed);

   DWORD ToPropertyID (PCMIFLVar pVar, BOOL fCreateIfNotExist);
   DWORD ToMethodID (PCMIFLVar pVar, BOOL fCreateIfNotExist);
   DWORD ToGlobalID (PCMIFLVar pVar, BOOL fCreateIfNotExist);
   DWORD ToMethodID (PWSTR pszName, PWSTR pszObjectPrivate, BOOL fCreateIfNotExist);
   DWORD ToPropertyID (PWSTR pszName, BOOL fCreateIfNotExist);
   DWORD ToGlobalID (PWSTR pszName, BOOL fCreateIfNotExist);

   // caller can change
   DWORD                m_dwMaxFuncLevelDeep;   // maximum depth that can call before get error
   DWORD                m_dwMaxLoopCount; // maximum loop count allowed before get error
   LANGID               m_LangID;         // language ID to default to (for resources and strings)

   // generally private
   PCMIFLCompiled       m_pCompiled;      // compiled code
   DWORD                m_dwDebugListMajor;  // major item to show for debug list, 0 for none, 1 for objects,
                                          // 2 for globals, 3 functions, 4 strings, 5 resources, 6 for active timers
   DWORD                m_dwDebugListMinor;  // minor item to show. Only used if major == objects.
                                          // 1 for methods, 0 for properties, 2 for timers
   GUID                 m_gDebugListGUID; // object GUID to look at
   CListVariable        m_lDebugVars;     // list of variables that showing. PWSTR
   GUID                 m_gDebugVarsObject;  // object state that in for displaying variables. GUID_NULL if none
   PCMIFLObject         m_pDebugVarsObjectLayer;   // layer to be ysed in m_gDebugVarsObject
   PCHashString         m_phDebugVarsString; // when displaying variables, if non-NULL then within a funtion
   PCMIFLVar            m_paDebugVars;    // when displaying variables, if non-NULL then withing a function
   DWORD                m_dwDebugVarsNum; // number of debug variables
   CMem                 m_memDebug;       // stores the debug string. m_dwCurPosn is next position to append to


   CMem                 m_memTemp;        // temporary memory used so can make VM's multithreaded
   PMIFLFCI             m_pFCICur;        // current function that in

   CListFixed           m_lPCMIFLVMTimer; // list of timers. Sorted by m_fTimeLeft, so ones to go off soonest are at top

   // custom identifers - look but dont touch
   CHashString          m_hIdentifiersCustomGlobal;   // custom identifiers (created within this virtual world). Elements are MIFLIDENT
   CHashDWORD           m_hUnIdentifiersCustomGlobal; // take ID and return index. Note: Element X of m_hUnIdentifiersCustom is the same as element X of m_hIdentifiersCustomXXX
   CHashString          m_hIdentifiersCustomMethod;   // custom identifiers (created within this virtual world). Elements are MIFLIDENT
   CHashDWORD           m_hUnIdentifiersCustomMethod; // take ID and return index. Note: Element X of m_hUnIdentifiersCustom is the same as element X of m_hIdentifiersCustomXXX
   CHashString          m_hIdentifiersCustomProperty;   // custom identifiers (created within this virtual world). Elements are MIFLIDENT
   CHashDWORD           m_hUnIdentifiersCustomProperty; // take ID and return index. Note: Element X of m_hUnIdentifiersCustom is the same as element X of m_hIdentifiersCustomXXX

private:
   void DebugVarToName (PCMIFLVar pVar, BOOL fJustName);
   BOOL VarAccess (PWSTR pszCode, PCMIFLVMObject pObject, PCMIFLObject pObjectLayer, PCHashString phVars,
                         PCMIFLVar paVars, DWORD dwVarsNum, PCMIFLVar pVar);
   BOOL WatchVarShow (PCEscPage pPage, DWORD dwNum, BOOL fVarText);
   DWORD RunTimeErr (PWSTR pszErr, BOOL fError, DWORD dwCharStart, DWORD dwCharEnd);
   DWORD FuncCallInternal (PMIFLFCI pFCI, PCMIFLVarLValue pVar);
   DWORD ExecuteCodeStatement (DWORD dwIndex, PCMIFLVarLValue pVar);
   DWORD ExecuteCode (DWORD dwIndex, PCMIFLVarLValue pVar);
   DWORD DebugUI (DWORD dwCharStart, DWORD dwCharEnd, PWSTR pszErr);
   DWORD ParseArguments (DWORD dwIndex, PCMIFLVarList *ppList);
   DWORD ParseOperatorUnary (PMIFLCOMP pc, DWORD *pdwType, PCMIFLVarLValue pVar);
   DWORD ParseOperatorBinary (BOOL fOnlyVarAccess, PMIFLCOMP pc, PCMIFLVarLValue pVarL, PCMIFLVarLValue pVarR,
      DWORD dwSpecial = 0);
   DWORD LValueSet (PCMIFLVarLValue pLValue, PCMIFLVarLValue pTo, PMIFLCOMP pc);
   DWORD ExecuteIfThenElse (DWORD dwIndex, PCMIFLVarLValue pVar);
   DWORD ExecuteWhile (DWORD dwIndex, PCMIFLVarLValue pVar);
   DWORD ExecuteDoWhile (DWORD dwIndex, PCMIFLVarLValue pVar);
   DWORD ExecuteFor (DWORD dwIndex, PCMIFLVarLValue pVar);
   DWORD ExecuteSwitch (DWORD dwIndex, PCMIFLVarLValue pVar);
   void VMTOKAdd (PWSTR pszName, DWORD dwID);
   DWORD VMTOKToID (DWORD dwVMTOK);
   DWORD IDToVMTOK (DWORD dwID);
   DWORD StringMethodCall (PCMIFLVarString ps, DWORD dwID, PCMIFLVarList plParam,
      DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar);
   DWORD ListMethodCall (PCMIFLVarList pl, DWORD dwID, PCMIFLVarList plParam,
      DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVarLValue pVar);

   DWORD QSortList (PCMIFLVarList pl, int iLow, int iHigh, PCMIFLVar pVarCallback);
   DWORD QSortCompare (PCMIFLVar pLow, PCMIFLVar pHigh, PCMIFLVar pVarCallback,
                             double *pf);
   BOOL QSortCallbackCleanup (PCMIFLVar pVar);

   PCMMLNode2 GlobalMMLTo (PCHashPVOID phString, PCHashPVOID phList);
   BOOL GlobalMMLFrom (BOOL fBlankSlate, PCMMLNode2 pNode, PCHashDWORD phString, PCHashDWORD phToList,
      PCHashGUID phObjectRemap);
   PCMMLNode2 ObjectsMMLTo (BOOL fAll, BOOL fGlobalsAndDel, PCHashGUID phExclude, 
                                 PCHashPVOID phString, PCHashPVOID phList);
   BOOL ObjectsMMLFrom (BOOL fBlankSlate, BOOL fRemap, PCMMLNode2 pNode, PCHashDWORD phString, PCHashDWORD phList,
                             PCHashGUID phObjectRemap, PCHashGUID phObjectAdded);
   BOOL ObjectsMMLFromRemap (BOOL fRemap, BOOL fRemapErr,
                              PCMMLNode2 pNode, PCHashGUID phObjectRemap);
   BOOL ObjectReplace (GUID *pgID, PCMIFLVMObject pNew, PCListFixed plContiainIn);

   PCMIFLAppSocket      m_pSocket;        // callback socket

   CHashGUID            m_hObjects;       // hash of objects (by GUID). Elements are PCMIFLVMObject
   CHashDWORD           m_hGlobals;      // values (CMIFLVarProp) for each of the globals, based on ID
   CHashDWORD           m_hIDToVMTOK;     // converts from ID to a VMTOK_XXX. sizeof (DWORD)
   CHashDWORD           m_hVMTOKToID;     // converts from a VMTOK_XXX to an ID. sizeof(DWORD)

   // custom identifiers
   DWORD                m_dwIDMethPropCur;     // current ID being used. Incremented each time needed

   CListFixed           m_lObjectsToDel;  // list of objects that are on the delete list
   DWORD                m_dwDebugMode;    // what kind of debugging. See MDM_XXX
   PCEscWindow          m_pDebugWindowApp;// debug window, as specified by the app
   PCEscWindow          m_pDebugWindowOwn;// own debug window
   HWND                 m_hWndDebugParent;   // parent for debugginb

   DWORD                m_dwFuncLevelDeep;// number of function levels deep in code

   // DWORD                m_dwRandSeed;     // random seed

   LARGE_INTEGER        m_iPerCountFreq;  // frequency of the performance counter
   LARGE_INTEGER        m_iPerCountLast;  // last performance counter... used for TimerAutomatic()
   DWORD                m_dwTimerAutoID;  // ID used for the automatic timer

   BOOL                 m_fDebugUIIn;     // set to TRUE if already in debug UI

   // timer
   double               m_fTimeCalibrate; // time in days since jan 1, 2001, calibration
   LARGE_INTEGER        m_iTimeCalibrate; // performance counter... calibrated
};
typedef CMIFLVM *PCMIFLVM;


#endif // _MIFL_H_
